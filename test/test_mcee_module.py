#!/usr/bin/env python3
"""
Test du module MCEE (Modele Complet d'Evaluation des Etats) v3.0

Ce module teste specifiquement le moteur MCEE qui traite les emotions:
- Reception des emotions sur mcee.emotional.input
- Traitement par MCT/MLT/PatternMatcher
- Emission de l'etat sur mcee.emotional.output

Le flux est:
  Emotion Module -> dimensions_queue -> emotion predictor
                                             |
                                             v
  mcee.emotional.input <- emotions.predictions (24 emotions)
                                             |
                                             v
                                    MCEE Engine (MCT/MLT)
                                             |
                                             v
  mcee.emotional.output -> mcee.state (etat complet)

Usage:
    python test_mcee_module.py           # Test complet
    python test_mcee_module.py -v        # Mode verbose
    python test_mcee_module.py --check   # Verifier connexion seulement
"""

import pika
import json
import threading
import time
import sys
import argparse
from dataclasses import dataclass
from typing import Dict, List, Optional, Any
from datetime import datetime

# =============================================================================
# CONFIGURATION
# =============================================================================

RABBITMQ_HOST = "localhost"
RABBITMQ_PORT = 5672
RABBITMQ_USER = "virtus"
RABBITMQ_PASS = "virtus@83"

# Exchanges pour le module MCEE
MCEE_INPUT_EXCHANGE = "mcee.emotional.input"
MCEE_INPUT_ROUTING_KEY = "emotions.predictions"
MCEE_OUTPUT_EXCHANGE = "mcee.emotional.output"
MCEE_OUTPUT_ROUTING_KEY = "mcee.state"

# Les 24 emotions avec les accents corrects (comme attendu par MCEE)
EMOTIONS_24 = [
    "Admiration", "Adoration", "Appréciation esthétique", "Amusement",
    "Anxiété", "Émerveillement", "Gêne", "Ennui", "Calme", "Confusion",
    "Dégoût", "Douleur empathique", "Fascination", "Excitation",
    "Peur", "Horreur", "Intérêt", "Joie", "Nostalgie", "Soulagement",
    "Tristesse", "Satisfaction", "Sympathie", "Triomphe"
]

# Les 8 phases emotionnelles
PHASES = ["SERENITE", "JOIE", "EXPLORATION", "ANXIETE", "PEUR", "TRISTESSE", "DEGOUT", "CONFUSION"]


# =============================================================================
# DATA CLASSES
# =============================================================================

@dataclass
class MCEETestResult:
    """Resultat d'un test MCEE."""
    name: str
    passed: bool
    input_emotions: Dict[str, float]
    output_state: Optional[Dict[str, Any]]
    expected_phase: str
    actual_phase: str
    duration_ms: float
    message: str = ""


@dataclass
class EmotionScenario:
    """Scenario de test avec emotions predefinies."""
    name: str
    emotions: Dict[str, float]
    expected_phase: str
    expected_dominant: str
    description: str


# =============================================================================
# SCENARIOS DE TEST
# =============================================================================

TEST_SCENARIOS = {
    "Calme": EmotionScenario(
        name="Calme",
        emotions={
            "Admiration": 0.05, "Adoration": 0.08, "Appréciation esthétique": 0.15,
            "Amusement": 0.12, "Anxiété": 0.02, "Émerveillement": 0.10,
            "Gêne": 0.01, "Ennui": 0.05, "Calme": 0.65, "Confusion": 0.03,
            "Dégoût": 0.01, "Douleur empathique": 0.02, "Fascination": 0.08,
            "Excitation": 0.10, "Peur": 0.01, "Horreur": 0.00, "Intérêt": 0.15,
            "Joie": 0.25, "Nostalgie": 0.05, "Soulagement": 0.20,
            "Tristesse": 0.02, "Satisfaction": 0.35, "Sympathie": 0.08, "Triomphe": 0.05
        },
        expected_phase="SERENITE",
        expected_dominant="Calme",
        description="Etat de serenite avec calme dominant"
    ),

    "Joie": EmotionScenario(
        name="Joie",
        emotions={
            "Admiration": 0.15, "Adoration": 0.20, "Appréciation esthétique": 0.12,
            "Amusement": 0.55, "Anxiété": 0.01, "Émerveillement": 0.25,
            "Gêne": 0.00, "Ennui": 0.00, "Calme": 0.15, "Confusion": 0.02,
            "Dégoût": 0.00, "Douleur empathique": 0.01, "Fascination": 0.18,
            "Excitation": 0.45, "Peur": 0.00, "Horreur": 0.00, "Intérêt": 0.30,
            "Joie": 0.75, "Nostalgie": 0.05, "Soulagement": 0.10,
            "Tristesse": 0.00, "Satisfaction": 0.40, "Sympathie": 0.12, "Triomphe": 0.25
        },
        expected_phase="JOIE",
        expected_dominant="Joie",
        description="Etat de joie intense"
    ),

    "Exploration": EmotionScenario(
        name="Exploration",
        emotions={
            "Admiration": 0.20, "Adoration": 0.10, "Appréciation esthétique": 0.25,
            "Amusement": 0.15, "Anxiété": 0.05, "Émerveillement": 0.35,
            "Gêne": 0.02, "Ennui": 0.00, "Calme": 0.10, "Confusion": 0.08,
            "Dégoût": 0.01, "Douleur empathique": 0.02, "Fascination": 0.50,
            "Excitation": 0.30, "Peur": 0.03, "Horreur": 0.00, "Intérêt": 0.65,
            "Joie": 0.25, "Nostalgie": 0.05, "Soulagement": 0.05,
            "Tristesse": 0.02, "Satisfaction": 0.15, "Sympathie": 0.10, "Triomphe": 0.08
        },
        expected_phase="EXPLORATION",
        expected_dominant="Intérêt",
        description="Etat de curiosite et exploration"
    ),

    "Anxiete": EmotionScenario(
        name="Anxiété",
        emotions={
            "Admiration": 0.02, "Adoration": 0.01, "Appréciation esthétique": 0.01,
            "Amusement": 0.00, "Anxiété": 0.65, "Émerveillement": 0.02,
            "Gêne": 0.15, "Ennui": 0.05, "Calme": 0.05, "Confusion": 0.30,
            "Dégoût": 0.10, "Douleur empathique": 0.12, "Fascination": 0.02,
            "Excitation": 0.08, "Peur": 0.35, "Horreur": 0.10, "Intérêt": 0.05,
            "Joie": 0.02, "Nostalgie": 0.08, "Soulagement": 0.02,
            "Tristesse": 0.20, "Satisfaction": 0.02, "Sympathie": 0.05, "Triomphe": 0.00
        },
        expected_phase="ANXIETE",
        expected_dominant="Anxiété",
        description="Etat d'anxiete et inquietude"
    ),

    "Peur": EmotionScenario(
        name="Peur",
        emotions={
            "Admiration": 0.00, "Adoration": 0.00, "Appréciation esthétique": 0.00,
            "Amusement": 0.00, "Anxiété": 0.55, "Émerveillement": 0.00,
            "Gêne": 0.08, "Ennui": 0.00, "Calme": 0.00, "Confusion": 0.20,
            "Dégoût": 0.25, "Douleur empathique": 0.15, "Fascination": 0.00,
            "Excitation": 0.05, "Peur": 0.85, "Horreur": 0.60, "Intérêt": 0.02,
            "Joie": 0.00, "Nostalgie": 0.00, "Soulagement": 0.00,
            "Tristesse": 0.35, "Satisfaction": 0.00, "Sympathie": 0.05, "Triomphe": 0.00
        },
        expected_phase="PEUR",
        expected_dominant="Peur",
        description="Etat de peur intense"
    ),

    "Tristesse": EmotionScenario(
        name="Tristesse",
        emotions={
            "Admiration": 0.02, "Adoration": 0.01, "Appréciation esthétique": 0.03,
            "Amusement": 0.00, "Anxiété": 0.15, "Émerveillement": 0.01,
            "Gêne": 0.05, "Ennui": 0.12, "Calme": 0.08, "Confusion": 0.10,
            "Dégoût": 0.08, "Douleur empathique": 0.35, "Fascination": 0.01,
            "Excitation": 0.00, "Peur": 0.10, "Horreur": 0.05, "Intérêt": 0.03,
            "Joie": 0.00, "Nostalgie": 0.40, "Soulagement": 0.02,
            "Tristesse": 0.70, "Satisfaction": 0.01, "Sympathie": 0.25, "Triomphe": 0.00
        },
        expected_phase="TRISTESSE",
        expected_dominant="Tristesse",
        description="Etat de tristesse profonde"
    ),

    "Degout": EmotionScenario(
        name="Dégoût",
        emotions={
            "Admiration": 0.00, "Adoration": 0.00, "Appréciation esthétique": 0.00,
            "Amusement": 0.00, "Anxiété": 0.20, "Émerveillement": 0.00,
            "Gêne": 0.15, "Ennui": 0.08, "Calme": 0.02, "Confusion": 0.12,
            "Dégoût": 0.75, "Douleur empathique": 0.10, "Fascination": 0.00,
            "Excitation": 0.00, "Peur": 0.25, "Horreur": 0.40, "Intérêt": 0.01,
            "Joie": 0.00, "Nostalgie": 0.02, "Soulagement": 0.00,
            "Tristesse": 0.15, "Satisfaction": 0.00, "Sympathie": 0.05, "Triomphe": 0.00
        },
        expected_phase="DEGOUT",
        expected_dominant="Dégoût",
        description="Etat de degout et aversion"
    ),

    "Confusion": EmotionScenario(
        name="Confusion",
        emotions={
            "Admiration": 0.05, "Adoration": 0.02, "Appréciation esthétique": 0.05,
            "Amusement": 0.08, "Anxiété": 0.25, "Émerveillement": 0.05,
            "Gêne": 0.20, "Ennui": 0.15, "Calme": 0.05, "Confusion": 0.60,
            "Dégoût": 0.08, "Douleur empathique": 0.05, "Fascination": 0.10,
            "Excitation": 0.08, "Peur": 0.15, "Horreur": 0.05, "Intérêt": 0.15,
            "Joie": 0.05, "Nostalgie": 0.08, "Soulagement": 0.02,
            "Tristesse": 0.12, "Satisfaction": 0.03, "Sympathie": 0.05, "Triomphe": 0.02
        },
        expected_phase="CONFUSION",
        expected_dominant="Confusion",
        description="Etat de confusion et desorientation"
    ),
}


# =============================================================================
# CLASSE DE TEST MCEE
# =============================================================================

class MCEEModuleTester:
    """Testeur du module MCEE."""

    def __init__(self, verbose: bool = False, timeout: int = 10):
        self.verbose = verbose
        self.timeout = timeout
        self.results: List[MCEETestResult] = []

    def log(self, message: str, level: str = "INFO"):
        """Affiche un message si mode verbose."""
        if self.verbose or level == "ERROR":
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            print(f"[{timestamp}] [{level}] {message}")

    def get_connection(self):
        """Cree une connexion RabbitMQ."""
        credentials = pika.PlainCredentials(RABBITMQ_USER, RABBITMQ_PASS)
        parameters = pika.ConnectionParameters(
            host=RABBITMQ_HOST,
            port=RABBITMQ_PORT,
            credentials=credentials,
            connection_attempts=3,
            retry_delay=1
        )
        return pika.BlockingConnection(parameters)

    def check_connection(self) -> bool:
        """Verifie la connexion RabbitMQ."""
        try:
            conn = self.get_connection()
            channel = conn.channel()

            # Verifier les exchanges
            channel.exchange_declare(
                exchange=MCEE_INPUT_EXCHANGE,
                exchange_type="topic",
                durable=True,
                passive=True  # Ne pas creer, juste verifier
            )
            channel.exchange_declare(
                exchange=MCEE_OUTPUT_EXCHANGE,
                exchange_type="topic",
                durable=True,
                passive=True
            )

            conn.close()
            return True
        except pika.exceptions.ChannelClosedByBroker:
            # Exchange n'existe pas
            return False
        except Exception as e:
            self.log(f"Erreur de connexion: {e}", "ERROR")
            return False

    def check_mcee_running(self) -> bool:
        """Verifie si le module MCEE est en cours d'execution."""
        try:
            conn = self.get_connection()
            channel = conn.channel()

            # Essayer de declarer les exchanges (passif)
            try:
                channel.exchange_declare(
                    exchange=MCEE_INPUT_EXCHANGE,
                    exchange_type="topic",
                    durable=True,
                    passive=True
                )
                input_exists = True
            except pika.exceptions.ChannelClosedByBroker:
                input_exists = False
                conn = self.get_connection()
                channel = conn.channel()

            try:
                channel.exchange_declare(
                    exchange=MCEE_OUTPUT_EXCHANGE,
                    exchange_type="topic",
                    durable=True,
                    passive=True
                )
                output_exists = True
            except pika.exceptions.ChannelClosedByBroker:
                output_exists = False

            conn.close()
            return input_exists and output_exists

        except Exception as e:
            self.log(f"Erreur verification MCEE: {e}", "ERROR")
            return False

    def send_emotions_to_mcee(self, emotions: Dict[str, float]) -> bool:
        """Envoie les emotions directement au module MCEE."""
        try:
            conn = self.get_connection()
            channel = conn.channel()

            # Declarer l'exchange (au cas ou il n'existe pas)
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

            self.log(f"Emotions envoyees au MCEE: {len(emotions)} emotions")
            conn.close()
            return True

        except Exception as e:
            self.log(f"Erreur envoi emotions: {e}", "ERROR")
            return False

    def receive_mcee_state(self, timeout: int = None, ready_event: threading.Event = None) -> Optional[Dict[str, Any]]:
        """Recoit l'etat traite par le MCEE.

        Args:
            timeout: Timeout en secondes
            ready_event: Event a signaler quand le receiver est pret (queue bindee)
        """
        if timeout is None:
            timeout = self.timeout

        state_received = []

        def callback(ch, method, properties, body):
            try:
                state = json.loads(body)
                state_received.append(state)
                self.log(f"Etat MCEE recu: {len(body)} bytes")
            except json.JSONDecodeError as e:
                self.log(f"Erreur parsing JSON: {e}", "ERROR")
            ch.stop_consuming()

        try:
            conn = self.get_connection()
            channel = conn.channel()

            # Declarer l'exchange
            channel.exchange_declare(
                exchange=MCEE_OUTPUT_EXCHANGE,
                exchange_type="topic",
                durable=True
            )

            # Creer une queue temporaire
            result = channel.queue_declare(queue="", exclusive=True)
            queue_name = result.method.queue

            # Bind a l'exchange
            channel.queue_bind(
                exchange=MCEE_OUTPUT_EXCHANGE,
                queue=queue_name,
                routing_key=MCEE_OUTPUT_ROUTING_KEY
            )

            self.log(f"En attente sur {MCEE_OUTPUT_EXCHANGE}...")

            channel.basic_consume(
                queue=queue_name,
                on_message_callback=callback,
                auto_ack=True
            )

            # Signaler que le receiver est pret (queue bindee, pret a recevoir)
            if ready_event:
                ready_event.set()

            # Timeout
            conn.call_later(timeout, lambda: channel.stop_consuming())

            channel.start_consuming()
            conn.close()

            return state_received[0] if state_received else None

        except Exception as e:
            self.log(f"Erreur reception etat: {e}", "ERROR")
            # Signaler meme en cas d'erreur pour debloquer le thread principal
            if ready_event:
                ready_event.set()
            return None

    def test_scenario(self, scenario: EmotionScenario) -> MCEETestResult:
        """Teste un scenario complet."""
        start_time = time.time()

        self.log(f"\n{'='*60}")
        self.log(f"TEST MCEE: {scenario.name}")
        self.log(f"{'='*60}")
        self.log(f"Description: {scenario.description}")

        # Demarrer le recepteur en arriere-plan
        state_result = [None]
        receiver_done = threading.Event()
        receiver_ready = threading.Event()  # Signale quand le receiver est pret

        def receiver_thread():
            state_result[0] = self.receive_mcee_state(ready_event=receiver_ready)
            receiver_done.set()

        thread = threading.Thread(target=receiver_thread)
        thread.start()

        # Attendre que le receiver soit vraiment pret (queue bindee)
        if not receiver_ready.wait(timeout=5.0):
            self.log("Timeout en attendant que le receiver soit pret", "ERROR")

        # Envoyer les emotions
        if not self.send_emotions_to_mcee(scenario.emotions):
            return MCEETestResult(
                name=scenario.name,
                passed=False,
                input_emotions=scenario.emotions,
                output_state=None,
                expected_phase=scenario.expected_phase,
                actual_phase="",
                duration_ms=(time.time() - start_time) * 1000,
                message="Erreur d'envoi des emotions"
            )

        # Attendre la reponse
        receiver_done.wait(timeout=self.timeout + 1)
        thread.join(timeout=1)

        duration_ms = (time.time() - start_time) * 1000

        if state_result[0] is None:
            return MCEETestResult(
                name=scenario.name,
                passed=False,
                input_emotions=scenario.emotions,
                output_state=None,
                expected_phase=scenario.expected_phase,
                actual_phase="",
                duration_ms=duration_ms,
                message="Timeout - MCEE n'a pas repondu. Le module est-il en cours d'execution?"
            )

        state = state_result[0]

        # Analyser l'etat recu
        actual_phase = state.get("phase", "UNKNOWN")
        dominant = state.get("dominant", "")
        e_global = state.get("E_global", 0.0)

        # Verifier le succes
        phase_match = actual_phase == scenario.expected_phase
        dominant_match = dominant == scenario.expected_dominant

        passed = phase_match  # Le phase matching est le critere principal

        # Afficher les resultats
        if self.verbose:
            print(f"\n--- Etat MCEE recu ---")
            print(f"Phase: {actual_phase} (attendu: {scenario.expected_phase})")
            print(f"Dominant: {dominant} (attendu: {scenario.expected_dominant})")
            print(f"E_global: {e_global:.3f}")
            if "pattern" in state:
                p = state["pattern"]
                print(f"Pattern: {p.get('name', 'N/A')} (similarite: {p.get('similarity', 0):.3f})")
            if "mct" in state:
                m = state["mct"]
                print(f"MCT: size={m.get('size', 0)}, stability={m.get('stability', 0):.3f}")

        result = MCEETestResult(
            name=scenario.name,
            passed=passed,
            input_emotions=scenario.emotions,
            output_state=state,
            expected_phase=scenario.expected_phase,
            actual_phase=actual_phase,
            duration_ms=duration_ms,
            message=f"Phase: {actual_phase}, Dominant: {dominant}"
        )

        self.results.append(result)
        return result

    def test_mcee_output_format(self) -> bool:
        """Teste que le format de sortie MCEE est correct."""
        # Envoyer un scenario simple
        scenario = TEST_SCENARIOS["Calme"]

        state_result = [None]
        done = threading.Event()
        ready = threading.Event()

        def receiver():
            state_result[0] = self.receive_mcee_state(timeout=5, ready_event=ready)
            done.set()

        thread = threading.Thread(target=receiver)
        thread.start()

        # Attendre que le receiver soit vraiment pret
        if not ready.wait(timeout=5.0):
            print("ECHEC: Timeout en attendant que le receiver soit pret")
            return False

        self.send_emotions_to_mcee(scenario.emotions)
        done.wait(timeout=6)
        thread.join(timeout=1)

        if state_result[0] is None:
            print("ECHEC: Pas de reponse du MCEE")
            return False

        state = state_result[0]

        # Verifier les champs obligatoires
        required_fields = [
            "emotions", "E_global", "variance_global", "valence",
            "intensity", "dominant", "dominant_value", "phase"
        ]

        missing = [f for f in required_fields if f not in state]
        if missing:
            print(f"ECHEC: Champs manquants: {missing}")
            return False

        # Verifier les 24 emotions
        if "emotions" in state:
            emotions = state["emotions"]
            missing_emotions = [e for e in EMOTIONS_24 if e not in emotions]
            if missing_emotions:
                print(f"ATTENTION: Emotions manquantes: {missing_emotions[:5]}...")

        print("OK: Format de sortie MCEE valide")
        return True

    def run_all_tests(self) -> Dict:
        """Execute tous les tests MCEE."""
        start_time = time.time()
        self.results = []

        print("\n" + "=" * 70)
        print("  TEST DU MODULE MCEE (Moteur de Traitement Emotionnel)")
        print("=" * 70)

        # 1. Verifier la connexion RabbitMQ
        print("\n[1/4] Verification de la connexion RabbitMQ...")
        try:
            conn = self.get_connection()
            conn.close()
            print("OK: Connexion RabbitMQ etablie")
        except Exception as e:
            print(f"ECHEC: {e}")
            return {"error": "Connection failed"}

        # 2. Verifier si MCEE est en cours d'execution
        print("\n[2/4] Verification du module MCEE...")
        mcee_running = self.check_mcee_running()
        if mcee_running:
            print("OK: Les exchanges MCEE existent")
        else:
            print("ATTENTION: Les exchanges MCEE n'existent pas.")
            print("          Le module MCEE doit etre demarre pour les tests complets.")
            print("          Commande: cd mcee_final/build && ./mcee")

        # 3. Tester le format de sortie
        print("\n[3/4] Test du format de sortie MCEE...")
        if mcee_running:
            format_ok = self.test_mcee_output_format()
        else:
            print("SKIP: Module MCEE non disponible")
            format_ok = False

        # 4. Tester tous les scenarios
        print("\n[4/4] Tests des 8 scenarios emotionnels...")
        scenario_results = []

        if mcee_running:
            for name, scenario in TEST_SCENARIOS.items():
                result = self.test_scenario(scenario)
                scenario_results.append(result)
                status = "PASS" if result.passed else "FAIL"
                print(f"  {status}: {name} -> Phase: {result.actual_phase}")
        else:
            print("SKIP: Module MCEE non disponible")

        # Resume
        total_duration = time.time() - start_time

        print("\n" + "=" * 70)
        print("  RESUME DES TESTS MCEE")
        print("=" * 70)

        if scenario_results:
            passed = sum(1 for r in scenario_results if r.passed)
            total = len(scenario_results)
            print(f"  Tests reussis: {passed}/{total} ({passed/total*100:.1f}%)")
        else:
            print("  Tests: Non executes (MCEE non disponible)")

        print(f"  Duree totale: {total_duration:.1f}s")
        print(f"  MCEE actif: {'Oui' if mcee_running else 'Non'}")
        print("=" * 70)

        return {
            "mcee_running": mcee_running,
            "format_valid": format_ok,
            "scenario_results": scenario_results,
            "duration_seconds": total_duration
        }


# =============================================================================
# DIAGNOSTIC
# =============================================================================

def diagnose_mcee():
    """Diagnostic du module MCEE."""
    print("\n" + "=" * 70)
    print("  DIAGNOSTIC DU MODULE MCEE")
    print("=" * 70)

    # 1. Connexion RabbitMQ
    print("\n[1] Connexion RabbitMQ...")
    try:
        credentials = pika.PlainCredentials(RABBITMQ_USER, RABBITMQ_PASS)
        params = pika.ConnectionParameters(
            host=RABBITMQ_HOST,
            port=RABBITMQ_PORT,
            credentials=credentials
        )
        conn = pika.BlockingConnection(params)
        channel = conn.channel()
        print(f"    OK: Connecte a {RABBITMQ_HOST}:{RABBITMQ_PORT}")
    except Exception as e:
        print(f"    ECHEC: {e}")
        return

    # 2. Exchange d'entree MCEE
    print(f"\n[2] Exchange d'entree ({MCEE_INPUT_EXCHANGE})...")
    try:
        channel.exchange_declare(
            exchange=MCEE_INPUT_EXCHANGE,
            exchange_type="topic",
            durable=True,
            passive=True
        )
        print(f"    OK: Exchange existe")
    except pika.exceptions.ChannelClosedByBroker:
        print(f"    ATTENTION: Exchange n'existe pas")
        print(f"    -> Le module MCEE doit etre demarre pour creer l'exchange")
        conn = pika.BlockingConnection(params)
        channel = conn.channel()

    # 3. Exchange de sortie MCEE
    print(f"\n[3] Exchange de sortie ({MCEE_OUTPUT_EXCHANGE})...")
    try:
        channel.exchange_declare(
            exchange=MCEE_OUTPUT_EXCHANGE,
            exchange_type="topic",
            durable=True,
            passive=True
        )
        print(f"    OK: Exchange existe")
    except pika.exceptions.ChannelClosedByBroker:
        print(f"    ATTENTION: Exchange n'existe pas")
        conn = pika.BlockingConnection(params)
        channel = conn.channel()

    # 4. Lister les queues
    print("\n[4] Queues RabbitMQ...")
    try:
        # Note: Ceci necessite le plugin management
        print("    (Utiliser 'rabbitmqctl list_queues' pour voir les queues)")
    except:
        pass

    conn.close()

    print("\n" + "-" * 70)
    print("RESUME:")
    print("  - Si les exchanges n'existent pas, demarrer MCEE:")
    print("    cd mcee_final/build && ./mcee")
    print("  - Verifier que RabbitMQ est demarre:")
    print("    sudo systemctl status rabbitmq-server")
    print("-" * 70)


# =============================================================================
# MAIN
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description="Test du module MCEE")
    parser.add_argument("-v", "--verbose", action="store_true", help="Mode verbose")
    parser.add_argument("--check", action="store_true", help="Verifier connexion seulement")
    parser.add_argument("--diagnose", action="store_true", help="Diagnostic complet")
    parser.add_argument("--timeout", type=int, default=10, help="Timeout en secondes")
    args = parser.parse_args()

    if args.diagnose:
        diagnose_mcee()
        return

    if args.check:
        tester = MCEEModuleTester(verbose=True)
        mcee_ok = tester.check_mcee_running()
        if mcee_ok:
            print("OK: Module MCEE detecte")
        else:
            print("ATTENTION: Module MCEE non detecte")
        return

    tester = MCEEModuleTester(verbose=args.verbose, timeout=args.timeout)
    results = tester.run_all_tests()


if __name__ == "__main__":
    main()
