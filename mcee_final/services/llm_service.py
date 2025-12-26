#!/usr/bin/env python3
"""
LLM Service - Service de reformulation émotionnelle via OpenAI/Anthropic

Ce service écoute les requêtes LLM sur RabbitMQ et renvoie les réponses.
Il sert d'intermédiaire entre le code C++ MCEE et les APIs LLM.

Usage:
    export OPENAI_API_KEY="sk-..."
    python llm_service.py

Configuration via variables d'environnement:
    OPENAI_API_KEY      - Clé API OpenAI (requis)
    OPENAI_MODEL        - Modèle à utiliser (défaut: gpt-4o-mini)
    ANTHROPIC_API_KEY   - Clé API Anthropic (optionnel)
    RABBITMQ_HOST       - Hôte RabbitMQ (défaut: localhost)
    RABBITMQ_PORT       - Port RabbitMQ (défaut: 5672)
    RABBITMQ_USER       - Utilisateur RabbitMQ (défaut: guest)
    RABBITMQ_PASS       - Mot de passe RabbitMQ (défaut: guest)
    LLM_PROVIDER        - Provider LLM: openai, anthropic (défaut: openai)
    LOG_LEVEL           - Niveau de log: DEBUG, INFO, WARNING, ERROR (défaut: INFO)
"""

import os
import sys
import json
import time
import logging
import signal
from datetime import datetime
from typing import Dict, Any, Optional, List
from dataclasses import dataclass, field
from enum import Enum

import pika
from pika.exceptions import AMQPConnectionError, AMQPChannelError

# Configuration du logging
LOG_LEVEL = os.environ.get("LOG_LEVEL", "INFO").upper()
logging.basicConfig(
    level=getattr(logging, LOG_LEVEL),
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)
logger = logging.getLogger("LLMService")


class LLMProvider(Enum):
    OPENAI = "openai"
    ANTHROPIC = "anthropic"


@dataclass
class LLMConfig:
    """Configuration du service LLM."""
    # Provider
    provider: LLMProvider = LLMProvider.OPENAI

    # OpenAI
    openai_api_key: str = ""
    openai_model: str = "gpt-4o-mini"
    openai_base_url: str = "https://api.openai.com/v1"

    # Anthropic
    anthropic_api_key: str = ""
    anthropic_model: str = "claude-3-haiku-20240307"

    # Paramètres de génération
    default_temperature: float = 0.7
    default_max_tokens: int = 500
    timeout: int = 30

    # RabbitMQ
    rabbitmq_host: str = "localhost"
    rabbitmq_port: int = 5672
    rabbitmq_user: str = "guest"
    rabbitmq_pass: str = "guest"
    request_exchange: str = "mcee.llm.request"
    response_exchange: str = "mcee.llm.response"

    # Retry
    max_retries: int = 3
    retry_delay: float = 1.0

    @classmethod
    def from_environment(cls) -> "LLMConfig":
        """Charge la configuration depuis les variables d'environnement."""
        config = cls()

        # Provider
        provider_str = os.environ.get("LLM_PROVIDER", "openai").lower()
        config.provider = LLMProvider(provider_str) if provider_str in ["openai", "anthropic"] else LLMProvider.OPENAI

        # OpenAI
        config.openai_api_key = os.environ.get("OPENAI_API_KEY", "")
        config.openai_model = os.environ.get("OPENAI_MODEL", "gpt-4o-mini")
        config.openai_base_url = os.environ.get("OPENAI_BASE_URL", "https://api.openai.com/v1")

        # Anthropic
        config.anthropic_api_key = os.environ.get("ANTHROPIC_API_KEY", "")
        config.anthropic_model = os.environ.get("ANTHROPIC_MODEL", "claude-3-haiku-20240307")

        # Paramètres
        config.default_temperature = float(os.environ.get("OPENAI_TEMPERATURE", "0.7"))
        config.default_max_tokens = int(os.environ.get("OPENAI_MAX_TOKENS", "500"))
        config.timeout = int(os.environ.get("LLM_TIMEOUT", "30"))

        # RabbitMQ
        config.rabbitmq_host = os.environ.get("RABBITMQ_HOST", "localhost")
        config.rabbitmq_port = int(os.environ.get("RABBITMQ_PORT", "5672"))
        config.rabbitmq_user = os.environ.get("RABBITMQ_USER", "guest")
        config.rabbitmq_pass = os.environ.get("RABBITMQ_PASS", "guest")
        config.request_exchange = os.environ.get("LLM_REQUEST_EXCHANGE", "mcee.llm.request")
        config.response_exchange = os.environ.get("LLM_RESPONSE_EXCHANGE", "mcee.llm.response")

        return config

    def validate(self) -> bool:
        """Valide la configuration."""
        if self.provider == LLMProvider.OPENAI and not self.openai_api_key:
            logger.error("OPENAI_API_KEY non défini")
            return False
        if self.provider == LLMProvider.ANTHROPIC and not self.anthropic_api_key:
            logger.error("ANTHROPIC_API_KEY non défini")
            return False
        return True


class OpenAIClient:
    """Client OpenAI avec retry et gestion d'erreurs."""

    def __init__(self, config: LLMConfig):
        self.config = config
        self._client = None

    def _get_client(self):
        """Lazy initialization du client OpenAI."""
        if self._client is None:
            try:
                from openai import OpenAI
                self._client = OpenAI(
                    api_key=self.config.openai_api_key,
                    base_url=self.config.openai_base_url,
                    timeout=self.config.timeout
                )
            except ImportError:
                logger.error("Package 'openai' non installé. Installer avec: pip install openai")
                raise
        return self._client

    def generate(
        self,
        messages: List[Dict[str, str]],
        model: Optional[str] = None,
        temperature: Optional[float] = None,
        max_tokens: Optional[int] = None
    ) -> Dict[str, Any]:
        """Génère une réponse via l'API OpenAI."""
        client = self._get_client()

        model = model or self.config.openai_model
        temperature = temperature if temperature is not None else self.config.default_temperature
        max_tokens = max_tokens or self.config.default_max_tokens

        start_time = time.time()

        try:
            response = client.chat.completions.create(
                model=model,
                messages=messages,
                temperature=temperature,
                max_tokens=max_tokens
            )

            elapsed_ms = (time.time() - start_time) * 1000

            return {
                "success": True,
                "content": response.choices[0].message.content,
                "model": response.model,
                "tokens_prompt": response.usage.prompt_tokens,
                "tokens_completion": response.usage.completion_tokens,
                "tokens_used": response.usage.total_tokens,
                "generation_time_ms": elapsed_ms
            }

        except Exception as e:
            logger.error(f"Erreur OpenAI: {e}")
            return {
                "success": False,
                "error": str(e),
                "error_type": type(e).__name__
            }


class AnthropicClient:
    """Client Anthropic avec retry et gestion d'erreurs."""

    def __init__(self, config: LLMConfig):
        self.config = config
        self._client = None

    def _get_client(self):
        """Lazy initialization du client Anthropic."""
        if self._client is None:
            try:
                import anthropic
                self._client = anthropic.Anthropic(
                    api_key=self.config.anthropic_api_key,
                    timeout=self.config.timeout
                )
            except ImportError:
                logger.error("Package 'anthropic' non installé. Installer avec: pip install anthropic")
                raise
        return self._client

    def generate(
        self,
        messages: List[Dict[str, str]],
        model: Optional[str] = None,
        temperature: Optional[float] = None,
        max_tokens: Optional[int] = None
    ) -> Dict[str, Any]:
        """Génère une réponse via l'API Anthropic."""
        client = self._get_client()

        model = model or self.config.anthropic_model
        temperature = temperature if temperature is not None else self.config.default_temperature
        max_tokens = max_tokens or self.config.default_max_tokens

        # Extraire le system prompt (Anthropic le traite séparément)
        system_prompt = ""
        chat_messages = []

        for msg in messages:
            if msg["role"] == "system":
                system_prompt = msg["content"]
            else:
                chat_messages.append(msg)

        start_time = time.time()

        try:
            response = client.messages.create(
                model=model,
                max_tokens=max_tokens,
                temperature=temperature,
                system=system_prompt,
                messages=chat_messages
            )

            elapsed_ms = (time.time() - start_time) * 1000

            # Extraire le contenu texte
            content = ""
            for block in response.content:
                if block.type == "text":
                    content += block.text

            return {
                "success": True,
                "content": content,
                "model": response.model,
                "tokens_prompt": response.usage.input_tokens,
                "tokens_completion": response.usage.output_tokens,
                "tokens_used": response.usage.input_tokens + response.usage.output_tokens,
                "generation_time_ms": elapsed_ms
            }

        except Exception as e:
            logger.error(f"Erreur Anthropic: {e}")
            return {
                "success": False,
                "error": str(e),
                "error_type": type(e).__name__
            }


class LLMService:
    """Service principal de traitement des requêtes LLM via RabbitMQ."""

    def __init__(self, config: LLMConfig):
        self.config = config
        self.connection: Optional[pika.BlockingConnection] = None
        self.channel: Optional[pika.channel.Channel] = None
        self.running = False

        # Client LLM selon le provider
        if config.provider == LLMProvider.OPENAI:
            self.llm_client = OpenAIClient(config)
        else:
            self.llm_client = AnthropicClient(config)

        # Stats
        self.stats = {
            "requests_total": 0,
            "requests_success": 0,
            "requests_failed": 0,
            "tokens_total": 0,
            "start_time": None
        }

    def connect(self) -> bool:
        """Établit la connexion RabbitMQ."""
        try:
            credentials = pika.PlainCredentials(
                self.config.rabbitmq_user,
                self.config.rabbitmq_pass
            )
            parameters = pika.ConnectionParameters(
                host=self.config.rabbitmq_host,
                port=self.config.rabbitmq_port,
                credentials=credentials,
                heartbeat=60,
                blocked_connection_timeout=300
            )

            self.connection = pika.BlockingConnection(parameters)
            self.channel = self.connection.channel()

            # Déclarer les exchanges
            self.channel.exchange_declare(
                exchange=self.config.request_exchange,
                exchange_type="topic",
                durable=True
            )
            self.channel.exchange_declare(
                exchange=self.config.response_exchange,
                exchange_type="topic",
                durable=True
            )

            # Créer une queue pour les requêtes
            result = self.channel.queue_declare(queue="llm.requests", durable=True)
            self.request_queue = result.method.queue

            # Bind la queue à l'exchange de requêtes
            self.channel.queue_bind(
                exchange=self.config.request_exchange,
                queue=self.request_queue,
                routing_key="llm.#"
            )

            # QoS : préfetch 1 message à la fois
            self.channel.basic_qos(prefetch_count=1)

            logger.info(f"Connexion RabbitMQ établie ({self.config.rabbitmq_host})")
            return True

        except AMQPConnectionError as e:
            logger.error(f"Erreur connexion RabbitMQ: {e}")
            return False

    def handle_request(
        self,
        ch: pika.channel.Channel,
        method: pika.spec.Basic.Deliver,
        properties: pika.spec.BasicProperties,
        body: bytes
    ):
        """Traite une requête LLM."""
        self.stats["requests_total"] += 1

        try:
            request = json.loads(body.decode("utf-8"))
            logger.debug(f"Requête reçue: {json.dumps(request, ensure_ascii=False)[:200]}...")

            # Extraire les paramètres
            messages = request.get("messages", [])
            model = request.get("model")
            temperature = request.get("temperature")
            max_tokens = request.get("max_tokens")

            if not messages:
                raise ValueError("Champ 'messages' manquant ou vide")

            # Générer la réponse
            result = self.llm_client.generate(
                messages=messages,
                model=model,
                temperature=temperature,
                max_tokens=max_tokens
            )

            if result["success"]:
                self.stats["requests_success"] += 1
                self.stats["tokens_total"] += result.get("tokens_used", 0)
                logger.info(
                    f"Réponse générée: {result.get('tokens_used', 0)} tokens, "
                    f"{result.get('generation_time_ms', 0):.0f}ms"
                )
            else:
                self.stats["requests_failed"] += 1
                logger.warning(f"Génération échouée: {result.get('error', 'Unknown error')}")

            # Publier la réponse
            response_body = json.dumps(result, ensure_ascii=False)

            response_properties = pika.BasicProperties(
                content_type="application/json",
                correlation_id=properties.correlation_id,
                delivery_mode=2  # Persistant
            )

            self.channel.basic_publish(
                exchange=self.config.response_exchange,
                routing_key="llm.response",
                body=response_body.encode("utf-8"),
                properties=response_properties
            )

            # ACK le message
            ch.basic_ack(delivery_tag=method.delivery_tag)

        except json.JSONDecodeError as e:
            logger.error(f"Erreur parsing JSON: {e}")
            ch.basic_nack(delivery_tag=method.delivery_tag, requeue=False)

        except Exception as e:
            logger.error(f"Erreur traitement requête: {e}", exc_info=True)

            # Envoyer une réponse d'erreur
            error_response = {
                "success": False,
                "error": str(e),
                "error_type": type(e).__name__
            }

            self.channel.basic_publish(
                exchange=self.config.response_exchange,
                routing_key="llm.response",
                body=json.dumps(error_response).encode("utf-8"),
                properties=pika.BasicProperties(
                    correlation_id=properties.correlation_id
                )
            )

            ch.basic_ack(delivery_tag=method.delivery_tag)

    def run(self):
        """Démarre le service."""
        if not self.connect():
            logger.error("Impossible de démarrer le service")
            return False

        self.running = True
        self.stats["start_time"] = datetime.now().isoformat()

        # Afficher la bannière
        print("=" * 60)
        print("LLM Service démarré")
        print(f"  Provider: {self.config.provider.value}")
        print(f"  Modèle: {self.config.openai_model if self.config.provider == LLMProvider.OPENAI else self.config.anthropic_model}")
        print(f"  Exchange requêtes: {self.config.request_exchange}")
        print(f"  Exchange réponses: {self.config.response_exchange}")
        print("=" * 60)

        # Démarrer la consommation
        self.channel.basic_consume(
            queue=self.request_queue,
            on_message_callback=self.handle_request
        )

        try:
            logger.info("En attente de requêtes...")
            self.channel.start_consuming()
        except KeyboardInterrupt:
            logger.info("Arrêt demandé par l'utilisateur")
        finally:
            self.stop()

        return True

    def stop(self):
        """Arrête le service."""
        self.running = False

        if self.channel and self.channel.is_open:
            try:
                self.channel.stop_consuming()
            except Exception:
                pass

        if self.connection and self.connection.is_open:
            try:
                self.connection.close()
            except Exception:
                pass

        # Afficher les stats
        print("\n" + "=" * 60)
        print("Statistiques de session:")
        print(f"  Requêtes totales: {self.stats['requests_total']}")
        print(f"  Succès: {self.stats['requests_success']}")
        print(f"  Échecs: {self.stats['requests_failed']}")
        print(f"  Tokens utilisés: {self.stats['tokens_total']}")
        print("=" * 60)

        logger.info("Service arrêté")


def main():
    """Point d'entrée principal."""
    # Charger la configuration
    config = LLMConfig.from_environment()

    # Valider
    if not config.validate():
        logger.error("Configuration invalide")
        sys.exit(1)

    # Créer et démarrer le service
    service = LLMService(config)

    # Gérer les signaux
    def signal_handler(signum, frame):
        logger.info(f"Signal {signum} reçu, arrêt...")
        service.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    # Démarrer
    success = service.run()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
