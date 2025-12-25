#!/usr/bin/env python3
"""
Test de vérification du stockage MCT (Mémoire Court Terme)
==========================================================

Ce test vérifie spécifiquement:
1. Que les états émotionnels sont stockés dans la MCT
2. Que la MCT calcule correctement stabilité/volatilité/trend
3. Que le MCTGraph reçoit les émotions (intensité suffisante)
4. Que les patterns sont correctement identifiés

Usage:
    python test_mct_storage.py
    python test_mct_storage.py -v  # verbose
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

MCEE_INPUT_EXCHANGE = "mcee.emotional.input"
MCEE_INPUT_ROUTING_KEY = "emotions.predictions"
MCEE_OUTPUT_EXCHANGE = "mcee.emotional.output"
MCEE_OUTPUT_ROUTING_KEY = "mcee.state"

# 24 émotions
EMOTIONS_24 = [
    "Admiration", "Adoration", "Appréciation esthétique", "Amusement",
    "Anxiété", "Émerveillement", "Gêne", "Ennui", "Calme", "Confusion",
    "Dégoût", "Douleur empathique", "Fascination", "Excitation",
    "Peur", "Horreur", "Intérêt", "Joie", "Nostalgie", "Soulagement",
    "Tristesse", "Satisfaction", "Sympathie", "Triomphe"
]


@dataclass
class MCTStorageResult:
    """Résultat d'un test de stockage MCT."""
    test_name: str
    mct_size: int
    mct_stability: float
    mct_volatility: float
    mct_trend: float
    pattern_name: str
    pattern_similarity: float
    mctgraph_words: int
    mctgraph_emotions: int
    mctgraph_edges: int
    e_global: float
    intensity: float
    dominant: str
    dominant_value: float
    raw_response: Dict = field(default_factory=dict)


class MCTStorageTester:
    """Testeur du stockage MCT."""

    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self.results: List[MCTStorageResult] = []

    def log(self, msg: str, level: str = "INFO"):
        if self.verbose or level in ["ERROR", "SUCCESS"]:
            ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            icon = {"INFO": "ℹ", "ERROR": "✗", "SUCCESS": "✓", "WARNING": "⚠"}.get(level, "•")
            print(f"[{ts}] {icon} {msg}")

    def _get_params(self):
        return pika.ConnectionParameters(
            host=RABBITMQ_HOST,
            port=RABBITMQ_PORT,
            credentials=pika.PlainCredentials(RABBITMQ_USER, RABBITMQ_PASS),
            heartbeat=600
        )

    def check_mcee(self) -> bool:
        """Vérifie que le MCEE est prêt."""
        try:
            conn = pika.BlockingConnection(self._get_params())
            ch = conn.channel()
            ch.queue_declare(queue="mcee_emotions_queue", passive=True)
            conn.close()
            return True
        except:
            return False

    def send_and_receive(self, emotions: Dict[str, float], timeout: int = 10) -> Optional[Dict]:
        """Envoie des émotions et reçoit la réponse."""
        response = [None]

        try:
            # Connexion pour recevoir
            recv_conn = pika.BlockingConnection(self._get_params())
            recv_ch = recv_conn.channel()
            recv_ch.exchange_declare(exchange=MCEE_OUTPUT_EXCHANGE, exchange_type="topic", durable=True)
            result = recv_ch.queue_declare(queue="", exclusive=True)
            queue_name = result.method.queue
            recv_ch.queue_bind(exchange=MCEE_OUTPUT_EXCHANGE, queue=queue_name, routing_key=MCEE_OUTPUT_ROUTING_KEY)

            def callback(ch, method, props, body):
                response[0] = json.loads(body)
                ch.stop_consuming()

            recv_ch.basic_consume(queue=queue_name, on_message_callback=callback, auto_ack=True)
            recv_conn.call_later(timeout, lambda: recv_ch.stop_consuming())

            # Envoyer les émotions
            send_conn = pika.BlockingConnection(self._get_params())
            send_ch = send_conn.channel()
            send_ch.exchange_declare(exchange=MCEE_INPUT_EXCHANGE, exchange_type="topic", durable=True)
            send_ch.basic_publish(
                exchange=MCEE_INPUT_EXCHANGE,
                routing_key=MCEE_INPUT_ROUTING_KEY,
                body=json.dumps(emotions)
            )
            send_conn.close()

            # Recevoir
            recv_ch.start_consuming()
            recv_conn.close()

        except Exception as e:
            self.log(f"Erreur: {e}", "ERROR")

        return response[0]

    def run_storage_test(self) -> Dict[str, Any]:
        """Test complet du stockage MCT."""
        print("\n" + "=" * 70)
        print("  TEST DE STOCKAGE MCT (Mémoire Court Terme)")
        print("=" * 70)

        if not self.check_mcee():
            print("\n✗ MCEE non détecté. Lancez: cd mcee_final/build && ./mcee")
            return {"error": "MCEE non disponible"}

        print("✓ MCEE détecté\n")

        # ===================================================================
        # TEST 1: État initial - intensité faible
        # ===================================================================
        print("-" * 70)
        print("TEST 1: État calme (intensité faible ~0.1)")
        print("-" * 70)

        emotions_calm = {e: 0.05 for e in EMOTIONS_24}
        emotions_calm["Calme"] = 0.30
        emotions_calm["Satisfaction"] = 0.20
        emotions_calm["Soulagement"] = 0.15

        resp1 = self.send_and_receive(emotions_calm)
        if resp1:
            r1 = self._parse_response("Test1_Calme", resp1)
            self._print_result(r1)
            self.results.append(r1)

        time.sleep(1)

        # ===================================================================
        # TEST 2: Intensité moyenne (devrait remplir MCT)
        # ===================================================================
        print("\n" + "-" * 70)
        print("TEST 2: Joie modérée (intensité moyenne ~0.3)")
        print("-" * 70)

        emotions_joy = {e: 0.10 for e in EMOTIONS_24}
        emotions_joy["Joie"] = 0.65
        emotions_joy["Excitation"] = 0.50
        emotions_joy["Satisfaction"] = 0.45
        emotions_joy["Amusement"] = 0.40

        resp2 = self.send_and_receive(emotions_joy)
        if resp2:
            r2 = self._parse_response("Test2_Joie", resp2)
            self._print_result(r2)
            self.results.append(r2)

        time.sleep(1)

        # ===================================================================
        # TEST 3: Intensité élevée (DOIT remplir MCTGraph!)
        # ===================================================================
        print("\n" + "-" * 70)
        print("TEST 3: Peur intense (intensité élevée ~0.5 - DOIT alimenter MCTGraph)")
        print("-" * 70)

        emotions_fear = {e: 0.15 for e in EMOTIONS_24}
        emotions_fear["Peur"] = 0.90
        emotions_fear["Horreur"] = 0.75
        emotions_fear["Anxiété"] = 0.70
        emotions_fear["Dégoût"] = 0.50

        resp3 = self.send_and_receive(emotions_fear)
        if resp3:
            r3 = self._parse_response("Test3_Peur", resp3)
            self._print_result(r3)
            self.results.append(r3)

        time.sleep(1)

        # ===================================================================
        # TEST 4: Séquence rapide pour tester volatilité
        # ===================================================================
        print("\n" + "-" * 70)
        print("TEST 4: Séquence rapide (5 états en 2.5s) - test volatilité")
        print("-" * 70)

        intensities = [0.3, 0.7, 0.4, 0.8, 0.5]
        for i, intensity in enumerate(intensities):
            emotions_seq = {e: intensity * 0.5 for e in EMOTIONS_24}
            emotions_seq["Joie"] = intensity
            emotions_seq["Excitation"] = intensity * 0.8
            self.send_and_receive(emotions_seq, timeout=3)
            time.sleep(0.5)

        # Récupérer l'état final
        emotions_final = {e: 0.25 for e in EMOTIONS_24}
        emotions_final["Calme"] = 0.50
        resp4 = self.send_and_receive(emotions_final)
        if resp4:
            r4 = self._parse_response("Test4_Sequence", resp4)
            self._print_result(r4)
            self.results.append(r4)

        # ===================================================================
        # RÉSUMÉ
        # ===================================================================
        print("\n" + "=" * 70)
        print("  RÉSUMÉ DU TEST DE STOCKAGE MCT")
        print("=" * 70)

        if self.results:
            print("\n  Évolution de la MCT:")
            print("  " + "-" * 66)
            print(f"  {'Test':<20} {'Size':<8} {'Stab':<8} {'Volat':<8} {'Trend':<8} {'Pattern':<15}")
            print("  " + "-" * 66)

            for r in self.results:
                print(f"  {r.test_name:<20} {r.mct_size:<8} {r.mct_stability:<8.2f} "
                      f"{r.mct_volatility:<8.2f} {r.mct_trend:<+8.2f} {r.pattern_name:<15}")

            print("  " + "-" * 66)

            # Vérifications
            print("\n  Vérifications:")
            final = self.results[-1]

            # MCT size augmente?
            if final.mct_size > 5:
                print(f"  ✓ MCT stocke les états (size={final.mct_size})")
            else:
                print(f"  ✗ MCT ne stocke pas assez (size={final.mct_size})")

            # Stabilité calculée?
            if 0 < final.mct_stability < 1:
                print(f"  ✓ Stabilité calculée ({final.mct_stability:.2f})")
            else:
                print(f"  ⚠ Stabilité anormale ({final.mct_stability})")

            # MCTGraph alimenté?
            if final.mctgraph_emotions > 0:
                print(f"  ✓ MCTGraph alimenté ({final.mctgraph_emotions} émotions)")
            else:
                print(f"  ⚠ MCTGraph vide (intensité trop basse? seuil=0.40)")

            # Intensité maximale atteinte?
            max_intensity = max(r.intensity for r in self.results)
            print(f"  ℹ Intensité max: {max_intensity:.3f} (seuil MCTGraph: 0.40)")

        print("=" * 70)

        return {
            "results": self.results,
            "mct_final_size": self.results[-1].mct_size if self.results else 0
        }

    def _parse_response(self, name: str, resp: Dict) -> MCTStorageResult:
        """Parse une réponse MCEE."""
        mct = resp.get("mct", {})
        pattern = resp.get("pattern", {})
        graph = resp.get("mct_graph", {})

        return MCTStorageResult(
            test_name=name,
            mct_size=mct.get("size", 0),
            mct_stability=mct.get("stability", 0.0),
            mct_volatility=mct.get("volatility", 0.0),
            mct_trend=mct.get("trend", 0.0),
            pattern_name=pattern.get("name", "?"),
            pattern_similarity=pattern.get("similarity", 0.0),
            mctgraph_words=graph.get("word_count", 0),
            mctgraph_emotions=graph.get("emotion_count", 0),
            mctgraph_edges=graph.get("causal_edge_count", 0),
            e_global=resp.get("E_global", 0.0),
            intensity=resp.get("intensity", 0.0),
            dominant=resp.get("dominant", "?"),
            dominant_value=resp.get("dominant_value", 0.0),
            raw_response=resp
        )

    def _print_result(self, r: MCTStorageResult):
        """Affiche un résultat."""
        print(f"\n  📊 Réponse MCEE:")
        print(f"     MCT:")
        print(f"       - Size: {r.mct_size} états")
        print(f"       - Stabilité: {r.mct_stability:.3f}")
        print(f"       - Volatilité: {r.mct_volatility:.3f}")
        print(f"       - Trend: {r.mct_trend:+.3f}")
        print(f"     Pattern: {r.pattern_name} (sim={r.pattern_similarity:.2f})")
        print(f"     Dominant: {r.dominant} = {r.dominant_value:.3f}")
        print(f"     Intensité: {r.intensity:.3f}")
        print(f"     E_global: {r.e_global:.3f}")
        print(f"     MCTGraph: {r.mctgraph_emotions} émotions, {r.mctgraph_words} mots, {r.mctgraph_edges} liens")


def main():
    parser = argparse.ArgumentParser(description="Test stockage MCT")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    tester = MCTStorageTester(verbose=args.verbose)
    results = tester.run_storage_test()

    if "error" in results:
        sys.exit(1)


if __name__ == "__main__":
    main()
