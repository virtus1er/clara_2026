#!/usr/bin/env python3
"""
Test réaliste complet du stockage MCT (Mémoire Court Terme)
============================================================

Ce test simule une interaction réaliste:
1. Émotions reçues en temps réel (comme du module émotionnel)
2. Contextes sous forme de parole (phrases de l'utilisateur)
3. Observation de ce que le MCEE choisit de stocker

Scénario: Une journée émotionnelle typique
- Réveil calme
- Bonne nouvelle (joie)
- Problème au travail (stress/anxiété)
- Situation de danger (peur intense - urgence)
- Résolution et soulagement
- Retour au calme

Usage:
    python test_mct_realiste_complet.py
    python test_mct_realiste_complet.py -v  # verbose
"""

import pika
import json
import time
import sys
import argparse
from datetime import datetime
from typing import Dict, Optional, Any, List
from dataclasses import dataclass, field

# =============================================================================
# CONFIGURATION
# =============================================================================

RABBITMQ_HOST = "localhost"
RABBITMQ_PORT = 5672
RABBITMQ_USER = "virtus"
RABBITMQ_PASS = "virtus@83"

# Exchanges MCEE
MCEE_EMOTIONS_EXCHANGE = "mcee.emotional.input"
MCEE_EMOTIONS_ROUTING_KEY = "emotions.predictions"
MCEE_SPEECH_EXCHANGE = "mcee.speech.input"
MCEE_SPEECH_ROUTING_KEY = "speech.text"
MCEE_OUTPUT_EXCHANGE = "mcee.emotional.output"
MCEE_OUTPUT_ROUTING_KEY = "mcee.state"

# 24 émotions du modèle
EMOTIONS_24 = [
    "Admiration", "Adoration", "Appréciation esthétique", "Amusement",
    "Anxiété", "Émerveillement", "Gêne", "Ennui", "Calme", "Confusion",
    "Dégoût", "Douleur empathique", "Fascination", "Excitation",
    "Peur", "Horreur", "Intérêt", "Joie", "Nostalgie", "Soulagement",
    "Tristesse", "Satisfaction", "Sympathie", "Triomphe"
]


@dataclass
class ScenarioStep:
    """Étape d'un scénario émotionnel."""
    name: str
    description: str
    emotions: Dict[str, float]
    speech: Optional[str] = None
    expected_pattern: Optional[str] = None
    delay_after: float = 1.0  # secondes


@dataclass
class MCTState:
    """État de la MCT à un instant donné."""
    step_name: str
    mct_size: int
    stability: float
    volatility: float
    trend: float
    pattern: str
    pattern_similarity: float
    dominant: str
    dominant_value: float
    intensity: float
    e_global: float
    mctgraph_emotions: int
    mctgraph_words: int
    emergency_triggered: bool = False
    raw: Dict = field(default_factory=dict)


class RealisticMCTTester:
    """Testeur réaliste du MCT avec scénarios."""

    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self.states: List[MCTState] = []
        self.conn = None
        self.channel = None

    def log(self, msg: str, level: str = "INFO"):
        if self.verbose or level in ["ERROR", "SUCCESS", "WARNING", "SCENARIO"]:
            ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            icons = {
                "INFO": "ℹ", "ERROR": "✗", "SUCCESS": "✓",
                "WARNING": "⚠", "SCENARIO": "🎬"
            }
            print(f"  [{ts}] {icons.get(level, '•')} {msg}")

    def _get_params(self):
        return pika.ConnectionParameters(
            host=RABBITMQ_HOST,
            port=RABBITMQ_PORT,
            credentials=pika.PlainCredentials(RABBITMQ_USER, RABBITMQ_PASS),
            heartbeat=600
        )

    def _create_base_emotions(self, base_level: float = 0.05) -> Dict[str, float]:
        """Crée un état émotionnel de base."""
        return {e: base_level for e in EMOTIONS_24}

    def send_emotions(self, emotions: Dict[str, float]) -> bool:
        """Envoie des émotions au MCEE."""
        try:
            conn = pika.BlockingConnection(self._get_params())
            ch = conn.channel()
            ch.exchange_declare(
                exchange=MCEE_EMOTIONS_EXCHANGE,
                exchange_type="topic",
                durable=True
            )
            ch.basic_publish(
                exchange=MCEE_EMOTIONS_EXCHANGE,
                routing_key=MCEE_EMOTIONS_ROUTING_KEY,
                body=json.dumps(emotions)
            )
            conn.close()
            return True
        except Exception as e:
            self.log(f"Erreur envoi émotions: {e}", "ERROR")
            return False

    def send_speech(self, text: str, source: str = "user") -> bool:
        """Envoie du texte (parole) au MCEE."""
        try:
            conn = pika.BlockingConnection(self._get_params())
            ch = conn.channel()
            ch.exchange_declare(
                exchange=MCEE_SPEECH_EXCHANGE,
                exchange_type="topic",
                durable=True
            )
            message = {
                "text": text,
                "source": source,
                "timestamp": datetime.now().isoformat()
            }
            ch.basic_publish(
                exchange=MCEE_SPEECH_EXCHANGE,
                routing_key=MCEE_SPEECH_ROUTING_KEY,
                body=json.dumps(message)
            )
            conn.close()
            self.log(f"Parole envoyée: \"{text[:50]}...\"" if len(text) > 50 else f"Parole envoyée: \"{text}\"", "INFO")
            return True
        except Exception as e:
            self.log(f"Erreur envoi parole: {e}", "ERROR")
            return False

    def receive_response(self, timeout: int = 5) -> Optional[Dict]:
        """Reçoit la réponse du MCEE."""
        response = [None]

        try:
            conn = pika.BlockingConnection(self._get_params())
            ch = conn.channel()
            ch.exchange_declare(
                exchange=MCEE_OUTPUT_EXCHANGE,
                exchange_type="topic",
                durable=True
            )
            result = ch.queue_declare(queue="", exclusive=True)
            queue_name = result.method.queue
            ch.queue_bind(
                exchange=MCEE_OUTPUT_EXCHANGE,
                queue=queue_name,
                routing_key=MCEE_OUTPUT_ROUTING_KEY
            )

            def callback(ch, method, props, body):
                response[0] = json.loads(body)
                ch.stop_consuming()

            ch.basic_consume(queue=queue_name, on_message_callback=callback, auto_ack=True)
            conn.call_later(timeout, lambda: ch.stop_consuming())
            ch.start_consuming()
            conn.close()

        except Exception as e:
            self.log(f"Erreur réception: {e}", "ERROR")

        return response[0]

    def execute_step(self, step: ScenarioStep) -> Optional[MCTState]:
        """Exécute une étape du scénario."""
        self.log(f"Étape: {step.name}", "SCENARIO")
        self.log(f"  {step.description}", "INFO")

        response = [None]

        try:
            # 1. D'ABORD s'abonner pour recevoir la réponse
            recv_conn = pika.BlockingConnection(self._get_params())
            recv_ch = recv_conn.channel()
            recv_ch.exchange_declare(
                exchange=MCEE_OUTPUT_EXCHANGE,
                exchange_type="topic",
                durable=True
            )
            result = recv_ch.queue_declare(queue="", exclusive=True)
            queue_name = result.method.queue
            recv_ch.queue_bind(
                exchange=MCEE_OUTPUT_EXCHANGE,
                queue=queue_name,
                routing_key=MCEE_OUTPUT_ROUTING_KEY
            )

            def callback(ch, method, props, body):
                response[0] = json.loads(body)
                ch.stop_consuming()

            recv_ch.basic_consume(queue=queue_name, on_message_callback=callback, auto_ack=True)
            recv_conn.call_later(8, lambda: recv_ch.stop_consuming())

            # 2. Envoyer la parole (contexte)
            if step.speech:
                self.send_speech(step.speech)

            # 3. Envoyer les émotions
            if not self.send_emotions(step.emotions):
                recv_conn.close()
                return None

            # 4. Attendre la réponse
            self.log("Attente réponse...", "INFO")
            recv_ch.start_consuming()
            recv_conn.close()

        except Exception as e:
            self.log(f"Erreur: {e}", "ERROR")
            import traceback
            traceback.print_exc()
            return None

        if not response[0]:
            self.log("Pas de réponse du MCEE", "WARNING")
            return None

        # Parser la réponse
        mct = response[0].get("mct", {})
        pattern = response[0].get("pattern", {})
        graph = response[0].get("mct_graph", {})

        state = MCTState(
            step_name=step.name,
            mct_size=mct.get("size", 0),
            stability=mct.get("stability", 0.0),
            volatility=mct.get("volatility", 0.0),
            trend=mct.get("trend", 0.0),
            pattern=pattern.get("name", "?"),
            pattern_similarity=pattern.get("similarity", 0.0),
            dominant=response[0].get("dominant", "?"),
            dominant_value=response[0].get("dominant_value", 0.0),
            intensity=response[0].get("intensity", 0.0),
            e_global=response[0].get("E_global", 0.0),
            mctgraph_emotions=graph.get("emotion_count", 0),
            mctgraph_words=graph.get("word_count", 0),
            emergency_triggered="emergency" in response[0],
            raw=response[0]
        )

        self.states.append(state)

        # Afficher le résultat
        self._print_step_result(state, step)

        # Attendre avant la prochaine étape
        time.sleep(step.delay_after)

        return state

    def _print_step_result(self, state: MCTState, step: ScenarioStep):
        """Affiche le résultat d'une étape."""
        print(f"\n  📊 Résultat:")
        print(f"     Pattern: {state.pattern} (sim={state.pattern_similarity:.2f})")
        print(f"     Dominant: {state.dominant} = {state.dominant_value:.3f}")
        print(f"     MCT: size={state.mct_size}, stability={state.stability:.2f}, trend={state.trend:+.2f}")

        if state.mctgraph_emotions > 0 or state.mctgraph_words > 0:
            print(f"     MCTGraph: {state.mctgraph_emotions} émotions, {state.mctgraph_words} mots")

        if state.emergency_triggered:
            print(f"     ⚡ URGENCE DÉCLENCHÉE")

        # Vérifier le pattern attendu
        if step.expected_pattern:
            if state.pattern == step.expected_pattern:
                self.log(f"Pattern correct: {state.pattern}", "SUCCESS")
            else:
                self.log(f"Pattern inattendu: {state.pattern} (attendu: {step.expected_pattern})", "WARNING")

    def create_daily_scenario(self) -> List[ScenarioStep]:
        """Crée un scénario de journée émotionnelle."""

        scenario = []

        # 1. Réveil calme
        emotions = self._create_base_emotions(0.05)
        emotions["Calme"] = 0.65
        emotions["Satisfaction"] = 0.40
        emotions["Soulagement"] = 0.30
        scenario.append(ScenarioStep(
            name="Réveil",
            description="Début de journée, état calme et serein",
            emotions=emotions,
            speech="Bonjour, je me réveille tranquillement. La journée commence bien.",
            expected_pattern="SERENITE",
            delay_after=1.5
        ))

        # 2. Bonne nouvelle - Joie montante
        emotions = self._create_base_emotions(0.10)
        emotions["Joie"] = 0.75
        emotions["Excitation"] = 0.60
        emotions["Satisfaction"] = 0.55
        emotions["Triomphe"] = 0.40
        scenario.append(ScenarioStep(
            name="Bonne nouvelle",
            description="Réception d'une excellente nouvelle",
            emotions=emotions,
            speech="J'ai reçu une super nouvelle ! Mon projet a été accepté, c'est fantastique !",
            expected_pattern="JOIE",
            delay_after=1.5
        ))

        # 3. Curiosité / Exploration
        emotions = self._create_base_emotions(0.10)
        emotions["Intérêt"] = 0.70
        emotions["Fascination"] = 0.65
        emotions["Excitation"] = 0.50
        emotions["Émerveillement"] = 0.45
        scenario.append(ScenarioStep(
            name="Découverte",
            description="Exploration d'un nouveau sujet passionnant",
            emotions=emotions,
            speech="C'est vraiment intéressant ! Comment ça fonctionne exactement ?",
            expected_pattern="EXPLORATION",
            delay_after=1.5
        ))

        # 4. Problème au travail - Stress montant
        emotions = self._create_base_emotions(0.15)
        emotions["Anxiété"] = 0.55
        emotions["Confusion"] = 0.45
        emotions["Peur"] = 0.35
        emotions["Tristesse"] = 0.30
        scenario.append(ScenarioStep(
            name="Problème",
            description="Un problème surgit, le stress monte",
            emotions=emotions,
            speech="Il y a un problème avec le système. Je ne comprends pas ce qui se passe.",
            expected_pattern="ANXIETE",
            delay_after=1.5
        ))

        # 5. Situation de danger - Peur intense (doit déclencher urgence)
        emotions = self._create_base_emotions(0.10)
        emotions["Peur"] = 0.92
        emotions["Horreur"] = 0.75
        emotions["Anxiété"] = 0.80
        emotions["Dégoût"] = 0.40
        scenario.append(ScenarioStep(
            name="DANGER",
            description="⚠ Situation de danger immédiat",
            emotions=emotions,
            speech="Attention ! Il y a un danger immédiat ! Il faut agir maintenant !",
            expected_pattern="PEUR",
            delay_after=2.0
        ))

        # 6. Crise en cours
        emotions = self._create_base_emotions(0.15)
        emotions["Peur"] = 0.70
        emotions["Anxiété"] = 0.65
        emotions["Confusion"] = 0.50
        emotions["Tristesse"] = 0.35
        scenario.append(ScenarioStep(
            name="Crise",
            description="Gestion de la situation de crise",
            emotions=emotions,
            speech="On doit gérer cette situation. Restez calmes, on va s'en sortir.",
            expected_pattern="PEUR",
            delay_after=1.5
        ))

        # 7. Résolution - Soulagement
        emotions = self._create_base_emotions(0.10)
        emotions["Soulagement"] = 0.80
        emotions["Calme"] = 0.50
        emotions["Joie"] = 0.45
        emotions["Satisfaction"] = 0.40
        scenario.append(ScenarioStep(
            name="Résolution",
            description="Le problème est résolu, soulagement",
            emotions=emotions,
            speech="C'est bon, le problème est résolu. Tout va bien maintenant.",
            expected_pattern="SERENITE",
            delay_after=1.5
        ))

        # 8. Retour à la normale
        emotions = self._create_base_emotions(0.05)
        emotions["Calme"] = 0.70
        emotions["Satisfaction"] = 0.55
        emotions["Soulagement"] = 0.45
        emotions["Joie"] = 0.30
        scenario.append(ScenarioStep(
            name="Normalité",
            description="Retour à un état calme et serein",
            emotions=emotions,
            speech="La journée se termine bien. Je suis content de comment ça s'est passé.",
            expected_pattern="SERENITE",
            delay_after=1.0
        ))

        return scenario

    def run_scenario(self) -> Dict[str, Any]:
        """Exécute le scénario complet."""
        print("\n" + "=" * 70)
        print("  TEST RÉALISTE MCT - SCÉNARIO: Une Journée Émotionnelle")
        print("=" * 70)
        print("\n  Ce test simule une journée avec:")
        print("  • Émotions en temps réel (module émotionnel)")
        print("  • Contexte via parole (ce que dit l'utilisateur)")
        print("  • Observation du stockage MCT")
        print()

        # Vérifier MCEE
        try:
            conn = pika.BlockingConnection(self._get_params())
            ch = conn.channel()
            ch.queue_declare(queue="mcee_emotions_queue", passive=True)
            conn.close()
            print("  ✓ MCEE détecté et prêt\n")
        except:
            print("  ✗ MCEE non détecté. Lancez: ./mcee")
            return {"error": "MCEE non disponible"}

        # Créer et exécuter le scénario
        scenario = self.create_daily_scenario()

        for i, step in enumerate(scenario, 1):
            print(f"\n{'─' * 70}")
            print(f"  ÉTAPE {i}/{len(scenario)}: {step.name}")
            print(f"{'─' * 70}")

            state = self.execute_step(step)
            if not state:
                self.log(f"Échec de l'étape {step.name}", "ERROR")

        # Afficher le résumé
        self._print_summary()

        return {
            "states": self.states,
            "final_mct_size": self.states[-1].mct_size if self.states else 0,
            "emergencies": sum(1 for s in self.states if s.emergency_triggered)
        }

    def _print_summary(self):
        """Affiche le résumé du test."""
        print("\n" + "=" * 70)
        print("  RÉSUMÉ: STOCKAGE MCT")
        print("=" * 70)

        if not self.states:
            print("  Aucune donnée collectée.")
            return

        # Tableau d'évolution
        print("\n  Évolution temporelle:")
        print("  " + "─" * 66)
        print(f"  {'Étape':<15} {'MCT':<6} {'Stab':<7} {'Pattern':<12} {'Dominant':<15} {'Intensité':<10}")
        print("  " + "─" * 66)

        for s in self.states:
            emergency = "⚡" if s.emergency_triggered else " "
            print(f"  {s.step_name:<15} {s.mct_size:<6} {s.stability:<7.2f} "
                  f"{s.pattern:<12} {s.dominant:<15} {s.intensity:<10.3f} {emergency}")

        print("  " + "─" * 66)

        # Statistiques
        final = self.states[-1]
        max_intensity = max(s.intensity for s in self.states)
        min_stability = min(s.stability for s in self.states)
        emergencies = sum(1 for s in self.states if s.emergency_triggered)
        patterns_seen = set(s.pattern for s in self.states)

        print("\n  Statistiques:")
        print(f"  • MCT finale: {final.mct_size} états stockés")
        print(f"  • Stabilité min: {min_stability:.2f} (lors des crises)")
        print(f"  • Intensité max: {max_intensity:.3f}")
        print(f"  • Patterns traversés: {', '.join(patterns_seen)}")
        print(f"  • Urgences déclenchées: {emergencies}")

        # Vérifications
        print("\n  Vérifications:")

        # MCT stocke bien
        if final.mct_size >= len(self.states):
            print(f"  ✓ MCT stocke les états ({final.mct_size} >= {len(self.states)} étapes)")
        else:
            print(f"  ⚠ MCT plus petite que les étapes ({final.mct_size} < {len(self.states)})")

        # Stabilité réagit aux crises
        if min_stability < 0.7:
            print(f"  ✓ Stabilité réagit aux crises (min={min_stability:.2f})")
        else:
            print(f"  ⚠ Stabilité stable même en crise ({min_stability:.2f})")

        # Urgence déclenchée lors du danger
        danger_states = [s for s in self.states if s.step_name == "DANGER"]
        if danger_states and danger_states[0].emergency_triggered:
            print(f"  ✓ Urgence déclenchée lors du danger")
        elif emergencies > 0:
            print(f"  ✓ {emergencies} urgence(s) déclenchée(s)")
        else:
            print(f"  ⚠ Aucune urgence (Peur < seuil?)")

        # Patterns variés
        if len(patterns_seen) >= 3:
            print(f"  ✓ Transitions de patterns ({len(patterns_seen)} patterns)")
        else:
            print(f"  ⚠ Peu de transitions ({len(patterns_seen)} patterns)")

        print("\n" + "=" * 70)


def main():
    parser = argparse.ArgumentParser(description="Test réaliste MCT")
    parser.add_argument("-v", "--verbose", action="store_true", help="Affiche plus de détails")
    args = parser.parse_args()

    tester = RealisticMCTTester(verbose=args.verbose)
    results = tester.run_scenario()

    if "error" in results:
        sys.exit(1)


if __name__ == "__main__":
    main()
