#!/usr/bin/env python3
"""
Test Réaliste de la Mémoire Court Terme (MCT) du MCEE
======================================================

Ce test simule le flux complet:
1. Réception des émotions du module émotionnel en temps réel
2. Réception des contextes sous forme de phases
3. Consultation des mémoires
4. Vérification du stockage dans la MCT

Paramètres MCT:
- max_size: 60 états maximum
- time_window_seconds: 30 secondes
- decay_factor: 0.95
- min_samples_for_signature: 5

Usage:
    python test_mct_realiste.py           # Test complet
    python test_mct_realiste.py -v        # Mode verbose
    python test_mct_realiste.py --quick   # Test rapide (sans attendre MCEE)
"""

import pika
import json
import time
import sys
import argparse
import threading
from datetime import datetime
from typing import Dict, List, Optional, Any
from dataclasses import dataclass, field
from enum import Enum


# =============================================================================
# CONFIGURATION
# =============================================================================

RABBITMQ_HOST = "localhost"
RABBITMQ_PORT = 5672
RABBITMQ_USER = "virtus"
RABBITMQ_PASS = "virtus@83"

# Exchanges MCEE
MCEE_INPUT_EXCHANGE = "mcee.emotional.input"
MCEE_INPUT_ROUTING_KEY = "emotions.predictions"
MCEE_OUTPUT_EXCHANGE = "mcee.emotional.output"
MCEE_OUTPUT_ROUTING_KEY = "mcee.state"
MCEE_SPEECH_EXCHANGE = "mcee.speech.input"
MCEE_SPEECH_ROUTING_KEY = "speech.text"

# Les 24 émotions
EMOTIONS_24 = [
    "Admiration", "Adoration", "Appréciation esthétique", "Amusement",
    "Anxiété", "Émerveillement", "Gêne", "Ennui", "Calme", "Confusion",
    "Dégoût", "Douleur empathique", "Fascination", "Excitation",
    "Peur", "Horreur", "Intérêt", "Joie", "Nostalgie", "Soulagement",
    "Tristesse", "Satisfaction", "Sympathie", "Triomphe"
]


# =============================================================================
# STRUCTURES DE DONNÉES
# =============================================================================

class Phase(Enum):
    SERENITE = "SERENITE"
    JOIE = "JOIE"
    EXPLORATION = "EXPLORATION"
    ANXIETE = "ANXIETE"
    PEUR = "PEUR"
    TRISTESSE = "TRISTESSE"
    DEGOUT = "DEGOUT"
    CONFUSION = "CONFUSION"


@dataclass
class MCTState:
    """État MCT reçu du MCEE."""
    size: int = 0
    stability: float = 0.0
    volatility: float = 0.0
    trend: float = 0.0


@dataclass
class PatternInfo:
    """Information sur le pattern actif."""
    id: str = ""
    name: str = ""
    similarity: float = 0.0
    confidence: float = 0.0
    is_new: bool = False
    is_transition: bool = False


@dataclass
class MCEEResponse:
    """Réponse complète du MCEE."""
    emotions: Dict[str, float] = field(default_factory=dict)
    E_global: float = 0.0
    variance_global: float = 0.0
    valence: float = 0.0
    intensity: float = 0.0
    dominant: str = ""
    dominant_value: float = 0.0
    phase: str = ""
    pattern: PatternInfo = field(default_factory=PatternInfo)
    mct: MCTState = field(default_factory=MCTState)
    coefficients: Dict[str, float] = field(default_factory=dict)
    timestamp: datetime = field(default_factory=datetime.now)


@dataclass
class TestResult:
    """Résultat d'un test individuel."""
    name: str
    passed: bool
    message: str
    mct_size_before: int = 0
    mct_size_after: int = 0
    stability: float = 0.0
    pattern_name: str = ""
    duration_ms: float = 0.0


# =============================================================================
# SCÉNARIOS DE TEST
# =============================================================================

class EmotionalScenario:
    """Scénario émotionnel pour le test."""

    def __init__(self, name: str, emotions: Dict[str, float],
                 expected_phase: Phase, description: str,
                 speech_text: str = "", expected_mct_behavior: str = ""):
        self.name = name
        self.emotions = emotions
        self.expected_phase = expected_phase
        self.description = description
        self.speech_text = speech_text
        self.expected_mct_behavior = expected_mct_behavior


# Scénarios réalistes simulant une séquence de vie
REALISTIC_SCENARIOS = [
    # 1. État initial calme
    EmotionalScenario(
        name="1_Réveil_Calme",
        emotions={
            "Calme": 0.70, "Soulagement": 0.40, "Satisfaction": 0.35,
            "Joie": 0.20, "Intérêt": 0.15, "Ennui": 0.10,
            **{e: 0.02 for e in EMOTIONS_24 if e not in ["Calme", "Soulagement", "Satisfaction", "Joie", "Intérêt", "Ennui"]}
        },
        expected_phase=Phase.SERENITE,
        description="Réveil tranquille du matin",
        speech_text="Bonjour, je me sens bien ce matin.",
        expected_mct_behavior="État initial stable, MCT vide → size=1"
    ),

    # 2. Légère curiosité
    EmotionalScenario(
        name="2_Curiosité_Matinale",
        emotions={
            "Intérêt": 0.55, "Fascination": 0.40, "Calme": 0.45,
            "Émerveillement": 0.30, "Joie": 0.25, "Excitation": 0.20,
            **{e: 0.05 for e in EMOTIONS_24 if e not in ["Intérêt", "Fascination", "Calme", "Émerveillement", "Joie", "Excitation"]}
        },
        expected_phase=Phase.EXPLORATION,
        description="Découverte de quelque chose d'intéressant",
        speech_text="Tiens, c'est intéressant ça.",
        expected_mct_behavior="Transition douce, MCT size=2, stabilité ~0.7"
    ),

    # 3. Montée de joie
    EmotionalScenario(
        name="3_Bonne_Nouvelle",
        emotions={
            "Joie": 0.75, "Excitation": 0.60, "Triomphe": 0.45,
            "Satisfaction": 0.50, "Émerveillement": 0.35, "Admiration": 0.25,
            **{e: 0.03 for e in EMOTIONS_24 if e not in ["Joie", "Excitation", "Triomphe", "Satisfaction", "Émerveillement", "Admiration"]}
        },
        expected_phase=Phase.JOIE,
        description="Réception d'une bonne nouvelle",
        speech_text="Super! C'est une excellente nouvelle!",
        expected_mct_behavior="Transition vers JOIE, MCT size=3, volatilité augmente"
    ),

    # 4. Maintien joie (stabilisation)
    EmotionalScenario(
        name="4_Joie_Stable",
        emotions={
            "Joie": 0.70, "Satisfaction": 0.55, "Soulagement": 0.40,
            "Calme": 0.35, "Excitation": 0.30, "Amusement": 0.45,
            **{e: 0.02 for e in EMOTIONS_24 if e not in ["Joie", "Satisfaction", "Soulagement", "Calme", "Excitation", "Amusement"]}
        },
        expected_phase=Phase.JOIE,
        description="Maintien de l'état joyeux",
        speech_text="Je suis vraiment content.",
        expected_mct_behavior="Stabilisation dans JOIE, MCT size=4, stabilité augmente"
    ),

    # 5. Apparition d'inquiétude
    EmotionalScenario(
        name="5_Inquiétude_Soudaine",
        emotions={
            "Anxiété": 0.55, "Peur": 0.35, "Confusion": 0.40,
            "Joie": 0.20, "Intérêt": 0.15, "Gêne": 0.25,
            **{e: 0.05 for e in EMOTIONS_24 if e not in ["Anxiété", "Peur", "Confusion", "Joie", "Intérêt", "Gêne"]}
        },
        expected_phase=Phase.ANXIETE,
        description="Apparition soudaine d'inquiétude",
        speech_text="Attends... qu'est-ce qui se passe?",
        expected_mct_behavior="Transition rapide, volatilité élevée, trend négatif"
    ),

    # 6. Peur intense (test Amyghaleon)
    EmotionalScenario(
        name="6_Peur_Intense",
        emotions={
            "Peur": 0.85, "Horreur": 0.60, "Anxiété": 0.70,
            "Dégoût": 0.30, "Confusion": 0.25, "Tristesse": 0.20,
            **{e: 0.02 for e in EMOTIONS_24 if e not in ["Peur", "Horreur", "Anxiété", "Dégoût", "Confusion", "Tristesse"]}
        },
        expected_phase=Phase.PEUR,
        description="Peur intense - déclenchement Amyghaleon",
        speech_text="Non! Danger!",
        expected_mct_behavior="ALERTE! Amyghaleon doit se déclencher, MCT volatile"
    ),

    # 7. Retour au calme progressif
    EmotionalScenario(
        name="7_Apaisement",
        emotions={
            "Soulagement": 0.60, "Calme": 0.50, "Anxiété": 0.25,
            "Peur": 0.15, "Satisfaction": 0.30, "Joie": 0.15,
            **{e: 0.05 for e in EMOTIONS_24 if e not in ["Soulagement", "Calme", "Anxiété", "Peur", "Satisfaction", "Joie"]}
        },
        expected_phase=Phase.SERENITE,
        description="Retour progressif au calme",
        speech_text="Ouf, ce n'était rien finalement.",
        expected_mct_behavior="Transition vers SERENITE, trend positif, stabilité remonte"
    ),

    # 8. Tristesse légère
    EmotionalScenario(
        name="8_Tristesse_Légère",
        emotions={
            "Tristesse": 0.55, "Nostalgie": 0.45, "Calme": 0.30,
            "Sympathie": 0.35, "Douleur empathique": 0.25, "Ennui": 0.15,
            **{e: 0.03 for e in EMOTIONS_24 if e not in ["Tristesse", "Nostalgie", "Calme", "Sympathie", "Douleur empathique", "Ennui"]}
        },
        expected_phase=Phase.TRISTESSE,
        description="Moment de tristesse légère",
        speech_text="Ça me rend un peu triste...",
        expected_mct_behavior="Transition douce vers TRISTESSE, stabilité modérée"
    ),
]


# =============================================================================
# TESTEUR MCT RÉALISTE
# =============================================================================

class MCTRealisteTester:
    """Testeur réaliste de la MCT."""

    def __init__(self, verbose: bool = False, timeout: int = 10):
        self.verbose = verbose
        self.timeout = timeout
        self.results: List[TestResult] = []
        self.responses: List[MCEEResponse] = []

    def log(self, message: str, level: str = "INFO"):
        """Log un message."""
        if self.verbose or level in ["ERROR", "SUCCESS", "WARNING"]:
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            icon = {"INFO": "ℹ", "ERROR": "✗", "SUCCESS": "✓", "WARNING": "⚠"}.get(level, "•")
            print(f"[{timestamp}] {icon} [{level}] {message}")

    def _get_connection_params(self):
        """Retourne les paramètres de connexion."""
        credentials = pika.PlainCredentials(RABBITMQ_USER, RABBITMQ_PASS)
        return pika.ConnectionParameters(
            host=RABBITMQ_HOST,
            port=RABBITMQ_PORT,
            credentials=credentials,
            connection_attempts=3,
            retry_delay=1,
            heartbeat=600,
            blocked_connection_timeout=300
        )

    def _new_connection(self):
        """Crée une nouvelle connexion."""
        return pika.BlockingConnection(self._get_connection_params())

    def check_connection(self) -> bool:
        """Vérifie la connexion RabbitMQ."""
        try:
            conn = self._new_connection()
            conn.close()
            self.log("Connexion RabbitMQ établie", "SUCCESS")
            return True
        except Exception as e:
            self.log(f"Erreur connexion: {e}", "ERROR")
            return False

    def check_mcee_ready(self) -> bool:
        """Vérifie que le MCEE est prêt."""
        try:
            conn = self._new_connection()
            channel = conn.channel()
            channel.queue_declare(queue="mcee_emotions_queue", passive=True)
            channel.queue_declare(queue="mcee_speech_queue", passive=True)
            conn.close()
            return True
        except pika.exceptions.ChannelClosedByBroker:
            return False
        except Exception as e:
            self.log(f"Erreur vérification MCEE: {e}", "ERROR")
            return False

    def wait_for_mcee(self, timeout: float = 30.0) -> bool:
        """Attend que le MCEE soit prêt."""
        start = time.time()
        self.log(f"Attente du MCEE (timeout: {timeout}s)...")

        while (time.time() - start) < timeout:
            if self.check_mcee_ready():
                self.log("MCEE détecté et prêt!", "SUCCESS")
                return True
            time.sleep(1.0)
            remaining = int(timeout - (time.time() - start))
            print(f"  ⏳ En attente... ({remaining}s)")

        self.log("Timeout: MCEE non détecté", "ERROR")
        return False

    def send_emotions(self, emotions: Dict[str, float]) -> bool:
        """Envoie les émotions au MCEE (nouvelle connexion à chaque fois)."""
        try:
            conn = self._new_connection()
            channel = conn.channel()

            # Déclarer l'exchange
            channel.exchange_declare(
                exchange=MCEE_INPUT_EXCHANGE,
                exchange_type="topic",
                durable=True
            )

            message = json.dumps(emotions)
            channel.basic_publish(
                exchange=MCEE_INPUT_EXCHANGE,
                routing_key=MCEE_INPUT_ROUTING_KEY,
                body=message,
                properties=pika.BasicProperties(
                    delivery_mode=2,
                    content_type="application/json"
                )
            )
            self.log(f"Émotions envoyées ({len(emotions)} valeurs)")
            conn.close()
            return True
        except Exception as e:
            self.log(f"Erreur envoi émotions: {e}", "ERROR")
            return False

    def send_speech(self, text: str, source: str = "user") -> bool:
        """Envoie un texte au module parole (nouvelle connexion)."""
        try:
            conn = self._new_connection()
            channel = conn.channel()

            channel.exchange_declare(
                exchange=MCEE_SPEECH_EXCHANGE,
                exchange_type="topic",
                durable=True
            )

            message = json.dumps({
                "text": text,
                "source": source,
                "confidence": 0.95
            })

            channel.basic_publish(
                exchange=MCEE_SPEECH_EXCHANGE,
                routing_key=MCEE_SPEECH_ROUTING_KEY,
                body=message,
                properties=pika.BasicProperties(
                    delivery_mode=2,
                    content_type="application/json"
                )
            )
            self.log(f"Parole envoyée: '{text[:50]}...'")
            conn.close()
            return True
        except Exception as e:
            self.log(f"Erreur envoi parole: {e}", "ERROR")
            return False

    def receive_response(self, timeout: int = None) -> Optional[MCEEResponse]:
        """Reçoit la réponse du MCEE (nouvelle connexion dédiée)."""
        if timeout is None:
            timeout = self.timeout

        response = [None]
        conn = None

        try:
            conn = self._new_connection()
            channel = conn.channel()

            channel.exchange_declare(
                exchange=MCEE_OUTPUT_EXCHANGE,
                exchange_type="topic",
                durable=True
            )

            result = channel.queue_declare(queue="", exclusive=True)
            queue_name = result.method.queue

            channel.queue_bind(
                exchange=MCEE_OUTPUT_EXCHANGE,
                queue=queue_name,
                routing_key=MCEE_OUTPUT_ROUTING_KEY
            )

            def callback(ch, method, properties, body):
                try:
                    data = json.loads(body)

                    resp = MCEEResponse()
                    resp.emotions = data.get("emotions", {})
                    resp.E_global = data.get("E_global", 0.0)
                    resp.variance_global = data.get("variance_global", 0.0)
                    resp.valence = data.get("valence", 0.0)
                    resp.intensity = data.get("intensity", 0.0)
                    resp.dominant = data.get("dominant", "")
                    resp.dominant_value = data.get("dominant_value", 0.0)
                    resp.phase = data.get("phase", "")

                    if "pattern" in data:
                        p = data["pattern"]
                        resp.pattern = PatternInfo(
                            id=p.get("id", ""),
                            name=p.get("name", ""),
                            similarity=p.get("similarity", 0.0),
                            confidence=p.get("confidence", 0.0),
                            is_new=p.get("is_new", False),
                            is_transition=p.get("is_transition", False)
                        )

                    if "mct" in data:
                        m = data["mct"]
                        resp.mct = MCTState(
                            size=m.get("size", 0),
                            stability=m.get("stability", 0.0),
                            volatility=m.get("volatility", 0.0),
                            trend=m.get("trend", 0.0)
                        )

                    resp.coefficients = data.get("coefficients", {})
                    resp.timestamp = datetime.now()

                    response[0] = resp
                    self.log(f"Réponse reçue: MCT size={resp.mct.size}, pattern={resp.pattern.name}")

                except Exception as e:
                    self.log(f"Erreur parsing réponse: {e}", "ERROR")

                ch.stop_consuming()

            channel.basic_consume(
                queue=queue_name,
                on_message_callback=callback,
                auto_ack=True
            )

            # Timeout via timer
            conn.call_later(timeout, lambda: channel.stop_consuming())
            channel.start_consuming()

        except Exception as e:
            self.log(f"Erreur réception: {e}", "ERROR")
        finally:
            if conn and conn.is_open:
                try:
                    conn.close()
                except:
                    pass

        return response[0]

    def run_scenario(self, scenario: EmotionalScenario,
                     previous_mct_size: int = 0) -> TestResult:
        """Exécute un scénario et vérifie les résultats."""
        start_time = time.time()

        print(f"\n{'='*70}")
        print(f"  TEST: {scenario.name}")
        print(f"  Description: {scenario.description}")
        print(f"  Phase attendue: {scenario.expected_phase.value}")
        print(f"{'='*70}")

        # Envoyer le texte si présent (avant les émotions)
        if scenario.speech_text:
            self.send_speech(scenario.speech_text)
            time.sleep(0.3)

        # Démarrer le récepteur en arrière-plan
        response = [None]
        receiver_done = threading.Event()

        def receive():
            response[0] = self.receive_response()
            receiver_done.set()

        thread = threading.Thread(target=receive)
        thread.start()
        time.sleep(0.5)  # Laisser le récepteur se connecter

        # Envoyer les émotions
        if not self.send_emotions(scenario.emotions):
            receiver_done.set()
            thread.join(timeout=1)
            return TestResult(
                name=scenario.name,
                passed=False,
                message="Erreur d'envoi des émotions",
                mct_size_before=previous_mct_size
            )

        # Attendre la réponse
        receiver_done.wait(timeout=self.timeout + 2)
        thread.join(timeout=2)

        duration_ms = (time.time() - start_time) * 1000

        if response[0] is None:
            return TestResult(
                name=scenario.name,
                passed=False,
                message="Timeout - pas de réponse du MCEE",
                mct_size_before=previous_mct_size,
                duration_ms=duration_ms
            )

        resp = response[0]
        self.responses.append(resp)

        # ===================================================================
        # VÉRIFICATIONS MCT
        # ===================================================================

        checks = []

        # 1. Vérifier que la MCT a bien stocké l'état
        if resp.mct.size > previous_mct_size:
            checks.append(f"✓ MCT stockage OK (size: {previous_mct_size} → {resp.mct.size})")
            mct_check = True
        elif resp.mct.size == previous_mct_size and previous_mct_size >= 60:
            checks.append(f"✓ MCT pleine (size={resp.mct.size}, max=60)")
            mct_check = True
        else:
            checks.append(f"✗ MCT stockage ÉCHEC (size: {previous_mct_size} → {resp.mct.size})")
            mct_check = False

        # 2. Vérifier la stabilité
        if 0.0 <= resp.mct.stability <= 1.0:
            stability_str = "haute" if resp.mct.stability > 0.7 else "moyenne" if resp.mct.stability > 0.4 else "basse"
            checks.append(f"✓ Stabilité MCT: {resp.mct.stability:.2f} ({stability_str})")
        else:
            checks.append(f"✗ Stabilité MCT invalide: {resp.mct.stability}")

        # 3. Vérifier la volatilité (inverse de stabilité)
        expected_volatility = 1.0 - resp.mct.stability
        if abs(resp.mct.volatility - expected_volatility) < 0.05:
            checks.append(f"✓ Volatilité MCT: {resp.mct.volatility:.2f}")
        else:
            checks.append(f"⚠ Volatilité MCT: {resp.mct.volatility:.2f} (attendu ~{expected_volatility:.2f})")

        # 4. Vérifier le trend
        if -1.0 <= resp.mct.trend <= 1.0:
            trend_str = "positif" if resp.mct.trend > 0.1 else "négatif" if resp.mct.trend < -0.1 else "stable"
            checks.append(f"✓ Trend MCT: {resp.mct.trend:+.2f} ({trend_str})")
        else:
            checks.append(f"✗ Trend MCT invalide: {resp.mct.trend}")

        # 5. Vérifier le pattern
        if resp.pattern.name:
            checks.append(f"✓ Pattern actif: {resp.pattern.name} (sim={resp.pattern.similarity:.2f})")
            if resp.pattern.is_transition:
                checks.append(f"  → Transition de pattern détectée!")
        else:
            checks.append("⚠ Aucun pattern actif")

        # 6. Vérifier la phase
        phase_match = resp.phase.upper() == scenario.expected_phase.value
        if phase_match:
            checks.append(f"✓ Phase: {resp.phase} (attendue: {scenario.expected_phase.value})")
        else:
            checks.append(f"⚠ Phase: {resp.phase} (attendue: {scenario.expected_phase.value})")

        # 7. Afficher les coefficients
        if resp.coefficients:
            coef_str = ", ".join([f"{k}={v:.2f}" for k, v in list(resp.coefficients.items())[:5]])
            checks.append(f"  Coefficients: {coef_str}")

        # Afficher les vérifications
        print("\nVérifications:")
        for check in checks:
            print(f"  {check}")

        # Déterminer si le test a réussi
        passed = mct_check and (resp.mct.size > 0)

        print(f"\nComportement attendu: {scenario.expected_mct_behavior}")

        result = TestResult(
            name=scenario.name,
            passed=passed,
            message=f"Phase={resp.phase}, Pattern={resp.pattern.name}",
            mct_size_before=previous_mct_size,
            mct_size_after=resp.mct.size,
            stability=resp.mct.stability,
            pattern_name=resp.pattern.name,
            duration_ms=duration_ms
        )

        self.results.append(result)
        return result

    def run_all_scenarios(self) -> Dict[str, Any]:
        """Exécute tous les scénarios."""
        print("\n" + "=" * 70)
        print("  TEST RÉALISTE DE LA MÉMOIRE COURT TERME (MCT)")
        print("=" * 70)
        print(f"  Scénarios: {len(REALISTIC_SCENARIOS)}")
        print(f"  Timeout: {self.timeout}s")
        print(f"  Verbose: {self.verbose}")
        print("=" * 70)

        # Vérifier connexion
        if not self.check_connection():
            return {"error": "Connexion RabbitMQ impossible"}

        # Attendre MCEE
        if not self.wait_for_mcee(timeout=15.0):
            print("\n⚠ MCEE non détecté. Lancez-le avec:")
            print("  cd mcee_final/build && ./mcee")
            return {"error": "MCEE non disponible"}

        # Exécuter les scénarios
        start_time = time.time()
        mct_size = 0

        for scenario in REALISTIC_SCENARIOS:
            result = self.run_scenario(scenario, mct_size)
            mct_size = result.mct_size_after

            # Pause entre les scénarios pour simuler le temps réel
            time.sleep(0.5)

        # Résumé
        total_duration = time.time() - start_time
        passed_count = sum(1 for r in self.results if r.passed)

        print("\n" + "=" * 70)
        print("  RÉSUMÉ DU TEST MCT RÉALISTE")
        print("=" * 70)
        print(f"  Tests réussis: {passed_count}/{len(self.results)}")
        print(f"  Durée totale: {total_duration:.1f}s")
        print()

        # Tableau des résultats
        print("  Résultats par scénario:")
        print("  " + "-" * 66)
        print(f"  {'Scénario':<25} {'MCT':<10} {'Stab.':<8} {'Pattern':<15} {'Status':<6}")
        print("  " + "-" * 66)

        for r in self.results:
            status = "PASS" if r.passed else "FAIL"
            print(f"  {r.name:<25} {r.mct_size_after:<10} {r.stability:<8.2f} {r.pattern_name:<15} {status:<6}")

        print("  " + "-" * 66)

        # Analyse de l'évolution MCT
        if self.responses:
            print("\n  Évolution de la MCT:")
            sizes = [r.mct.size for r in self.responses]
            stabilities = [r.mct.stability for r in self.responses]

            print(f"  - Taille finale: {sizes[-1]} états")
            print(f"  - Stabilité moyenne: {sum(stabilities)/len(stabilities):.2f}")
            print(f"  - Patterns détectés: {len(set(r.pattern.name for r in self.responses))}")

            # Compter les transitions
            transitions = sum(1 for r in self.responses if r.pattern.is_transition)
            print(f"  - Transitions de pattern: {transitions}")

        print("=" * 70)

        return {
            "passed": passed_count,
            "total": len(self.results),
            "duration_seconds": total_duration,
            "results": self.results,
            "responses": self.responses
        }


# =============================================================================
# MAIN
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description="Test réaliste de la MCT")
    parser.add_argument("-v", "--verbose", action="store_true", help="Mode verbose")
    parser.add_argument("--timeout", type=int, default=10, help="Timeout (secondes)")
    parser.add_argument("--quick", action="store_true", help="Test rapide (pas d'attente MCEE)")
    args = parser.parse_args()

    tester = MCTRealisteTester(verbose=args.verbose, timeout=args.timeout)

    if args.quick:
        print("Mode rapide: vérification connexion seulement")
        if tester.check_connection():
            if tester.check_mcee_ready():
                print("✓ MCEE prêt")
            else:
                print("✗ MCEE non prêt")
        return

    results = tester.run_all_scenarios()

    if "error" in results:
        print(f"\n✗ Erreur: {results['error']}")
        sys.exit(1)

    success_rate = results["passed"] / results["total"] if results["total"] > 0 else 0
    sys.exit(0 if success_rate >= 0.8 else 1)


if __name__ == "__main__":
    main()
