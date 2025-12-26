#!/usr/bin/env python3
"""
Test de validation comportementale pour le système MCEE
Vérifie que le comportement émergent est cohérent et réaliste

Critères de validation :
1. Cohérence temporelle : Les transitions de patterns sont logiques
2. Réactivité aux menaces : Temps de réponse < 100ms
3. Stabilité MCT : Augmente avec des stimuli cohérents
4. Patterns dominants : Correspondent aux émotions envoyées
"""

import pika
import json
import time
import sys
import threading
from datetime import datetime
from dataclasses import dataclass
from typing import List, Dict, Optional
import statistics

# Configuration RabbitMQ
RABBITMQ_HOST = 'localhost'
RABBITMQ_USER = 'virtus'
RABBITMQ_PASS = 'virtus@83'

# Queues et Exchanges
EMOTIONS_EXCHANGE = 'mcee.emotional.input'
EMOTIONS_ROUTING_KEY = 'emotions.predictions'
OUTPUT_EXCHANGE = 'mcee.emotional.output'


@dataclass
class ValidationResult:
    """Résultat d'un test de validation"""
    test_name: str
    passed: bool
    score: float  # 0.0 à 1.0
    details: str
    metrics: Dict


class BehavioralValidator:
    """Validateur comportemental MCEE"""

    def __init__(self):
        self.credentials = pika.PlainCredentials(RABBITMQ_USER, RABBITMQ_PASS)
        self.params = pika.ConnectionParameters(
            host=RABBITMQ_HOST,
            credentials=self.credentials
        )
        self.received_states = []
        self.consumer_running = False
        self.results: List[ValidationResult] = []

    def connect(self):
        """Établit la connexion RabbitMQ"""
        self.connection = pika.BlockingConnection(self.params)
        self.channel = self.connection.channel()

        self.channel.exchange_declare(exchange=EMOTIONS_EXCHANGE, exchange_type='topic', durable=True)
        self.channel.queue_declare(queue='mcee_emotions_queue', durable=True)
        self.channel.queue_bind(queue='mcee_emotions_queue', exchange=EMOTIONS_EXCHANGE, routing_key=EMOTIONS_ROUTING_KEY)

        return True

    def close(self):
        """Ferme la connexion"""
        self.consumer_running = False
        time.sleep(0.2)
        if hasattr(self, 'connection') and self.connection.is_open:
            self.connection.close()

    def start_state_consumer(self):
        """Démarre un consommateur pour capturer les états"""
        def consumer_thread():
            try:
                conn = pika.BlockingConnection(self.params)
                ch = conn.channel()

                ch.exchange_declare(exchange=OUTPUT_EXCHANGE, exchange_type='topic', durable=True)

                result = ch.queue_declare(queue='', exclusive=True)
                queue = result.method.queue
                ch.queue_bind(exchange=OUTPUT_EXCHANGE, queue=queue, routing_key='#')

                def on_state(ch, method, props, body):
                    receive_time = time.perf_counter()
                    try:
                        data = json.loads(body.decode())
                        data['_receive_time'] = receive_time
                        self.received_states.append(data)
                    except:
                        pass

                ch.basic_consume(queue=queue, on_message_callback=on_state, auto_ack=True)

                self.consumer_running = True
                while self.consumer_running:
                    conn.process_data_events(time_limit=0.1)

                conn.close()
            except Exception as e:
                print(f"Erreur consumer: {e}")

        thread = threading.Thread(target=consumer_thread, daemon=True)
        thread.start()
        time.sleep(0.5)

    def send_emotions(self, emotions: list):
        """Envoie un vecteur de 24 émotions"""
        message = {
            "emotions": emotions,
            "timestamp": time.time(),
            "source": "behavioral_test",
            "_send_time": time.perf_counter()
        }

        self.channel.basic_publish(
            exchange=EMOTIONS_EXCHANGE,
            routing_key=EMOTIONS_ROUTING_KEY,
            body=json.dumps(message)
        )

    def clear_states(self):
        """Efface les états collectés"""
        self.received_states.clear()

    # ═══════════════════════════════════════════════════════════════════════
    # TESTS DE VALIDATION
    # ═══════════════════════════════════════════════════════════════════════

    def test_temporal_coherence(self) -> ValidationResult:
        """
        Test 1 : Cohérence temporelle
        Vérifie que les transitions de patterns suivent une logique
        """
        print("\n  🔍 Test de cohérence temporelle...")
        self.clear_states()

        # Transitions logiques attendues
        logical_transitions = {
            'SERENITE': ['JOIE', 'EXPLORATION', 'SERENITE'],
            'JOIE': ['SERENITE', 'EXPLORATION', 'JOIE', 'TRISTESSE'],
            'EXPLORATION': ['JOIE', 'SERENITE', 'PEUR', 'EXPLORATION'],
            'PEUR': ['SERENITE', 'COLERE', 'PEUR'],
            'COLERE': ['SERENITE', 'PEUR', 'COLERE'],
            'TRISTESSE': ['SERENITE', 'TRISTESSE', 'JOIE'],
        }

        # Séquence de test : SERENITE → JOIE → EXPLORATION → PEUR → SERENITE
        sequences = [
            ([0.1] * 24, 'calme'),       # Calme
            ([0.1] * 24, 'joie'),        # Transition vers joie
            ([0.1] * 24, 'explore'),     # Exploration
            ([0.1] * 24, 'peur'),        # Peur soudaine
            ([0.1] * 24, 'retour'),      # Retour au calme
        ]

        # Modifier pour joie
        sequences[1][0][17] = 0.8  # Joie
        sequences[1][0][13] = 0.6  # Excitation

        # Modifier pour exploration
        sequences[2][0][16] = 0.8  # Intérêt
        sequences[2][0][12] = 0.7  # Fascination

        # Modifier pour peur
        sequences[3][0][14] = 0.85  # Peur
        sequences[3][0][4] = 0.7    # Anxiété

        # Modifier pour calme
        sequences[4][0][8] = 0.7   # Calme
        sequences[4][0][19] = 0.5  # Soulagement

        for emotions, label in sequences:
            self.send_emotions(emotions)
            time.sleep(0.5)

        time.sleep(1.0)

        # Analyser les transitions
        patterns = []
        for state in self.received_states:
            if 'pattern' in state:
                patterns.append(state['pattern'])

        logical_count = 0
        total_transitions = 0

        for i in range(1, len(patterns)):
            prev = patterns[i - 1]
            curr = patterns[i]
            total_transitions += 1

            # Vérifier si la transition est logique
            if prev in logical_transitions:
                if curr in logical_transitions[prev] or curr == prev:
                    logical_count += 1
            else:
                # Pattern inconnu, considérer comme acceptable
                logical_count += 1

        score = logical_count / max(total_transitions, 1)
        passed = score >= 0.7

        return ValidationResult(
            test_name="Cohérence temporelle",
            passed=passed,
            score=score,
            details=f"{logical_count}/{total_transitions} transitions logiques",
            metrics={'patterns': patterns, 'logical_transitions': logical_count}
        )

    def test_threat_reactivity(self) -> ValidationResult:
        """
        Test 2 : Réactivité aux menaces
        Vérifie que le système réagit rapidement aux menaces
        """
        print("\n  🔍 Test de réactivité aux menaces...")
        self.clear_states()

        # Envoyer un état calme
        calm = [0.3] * 24
        calm[8] = 0.7  # Calme
        self.send_emotions(calm)
        time.sleep(0.5)
        self.clear_states()

        # Envoyer une menace soudaine
        threat = [0.1] * 24
        threat[14] = 0.95  # Peur maximale
        threat[15] = 0.85  # Horreur
        threat[4] = 0.80   # Anxiété

        send_time = time.perf_counter()
        self.send_emotions(threat)

        # Attendre la réponse
        timeout = time.time() + 2
        while len(self.received_states) == 0 and time.time() < timeout:
            time.sleep(0.01)

        if self.received_states:
            receive_time = self.received_states[0].get('_receive_time', time.perf_counter())
            reaction_time = (receive_time - send_time) * 1000  # en ms

            passed = reaction_time < 100
            score = max(0, 1 - (reaction_time / 200))

            return ValidationResult(
                test_name="Réactivité menaces",
                passed=passed,
                score=score,
                details=f"Temps de réaction: {reaction_time:.1f}ms",
                metrics={'reaction_time_ms': reaction_time}
            )
        else:
            return ValidationResult(
                test_name="Réactivité menaces",
                passed=False,
                score=0.0,
                details="Aucune réponse reçue",
                metrics={'reaction_time_ms': None}
            )

    def test_mct_stability(self) -> ValidationResult:
        """
        Test 3 : Stabilité MCT
        Vérifie que la stabilité augmente avec des stimuli cohérents
        """
        print("\n  🔍 Test de stabilité MCT...")
        self.clear_states()

        stabilities = []

        # Envoyer 20 états cohérents (calme)
        for i in range(20):
            calm = [0.3] * 24
            calm[8] = 0.6 + (i * 0.01)  # Calme croissant légèrement
            calm[21] = 0.5

            self.send_emotions(calm)
            time.sleep(0.2)

            # Capturer la stabilité
            if self.received_states:
                latest = self.received_states[-1]
                if 'stability' in latest:
                    stabilities.append(latest['stability'])

        time.sleep(0.5)

        if len(stabilities) >= 5:
            # La stabilité devrait augmenter
            first_half = stabilities[:len(stabilities)//2]
            second_half = stabilities[len(stabilities)//2:]

            avg_first = statistics.mean(first_half) if first_half else 0
            avg_second = statistics.mean(second_half) if second_half else 0

            improvement = avg_second - avg_first
            passed = improvement >= 0 or avg_second > 0.8
            score = min(1.0, avg_second)

            return ValidationResult(
                test_name="Stabilité MCT",
                passed=passed,
                score=score,
                details=f"Stabilité finale: {avg_second:.2f} (amélioration: {improvement:+.2f})",
                metrics={'stabilities': stabilities, 'avg_first': avg_first, 'avg_second': avg_second}
            )
        else:
            return ValidationResult(
                test_name="Stabilité MCT",
                passed=False,
                score=0.0,
                details="Données insuffisantes",
                metrics={'stabilities': stabilities}
            )

    def test_dominant_pattern_match(self) -> ValidationResult:
        """
        Test 4 : Correspondance patterns dominants
        Vérifie que les patterns détectés correspondent aux émotions envoyées
        """
        print("\n  🔍 Test de correspondance patterns...")
        self.clear_states()

        # Paires (émotions, pattern attendu)
        test_cases = [
            # JOIE
            (self._create_emotion_vector({17: 0.9, 13: 0.7, 21: 0.6}), ['JOIE', 'EXPLORATION', 'CUSTOM_']),
            # PEUR
            (self._create_emotion_vector({14: 0.9, 15: 0.7, 4: 0.8}), ['PEUR', 'CUSTOM_']),
            # COLERE
            (self._create_emotion_vector({6: 0.9, 10: 0.6}), ['COLERE', 'CUSTOM_']),
            # SERENITE
            (self._create_emotion_vector({8: 0.8, 21: 0.6, 19: 0.5}), ['SERENITE', 'CUSTOM_']),
        ]

        matches = 0
        total = len(test_cases)

        for emotions, expected_patterns in test_cases:
            self.clear_states()
            self.send_emotions(emotions)
            time.sleep(0.8)

            if self.received_states:
                detected = self.received_states[-1].get('pattern', '')
                # Vérifier si le pattern détecté est dans les attendus
                if any(exp in detected for exp in expected_patterns):
                    matches += 1

        score = matches / max(total, 1)
        passed = score >= 0.5

        return ValidationResult(
            test_name="Correspondance patterns",
            passed=passed,
            score=score,
            details=f"{matches}/{total} patterns correspondants",
            metrics={'matches': matches, 'total': total}
        )

    def test_emotional_valence_coherence(self) -> ValidationResult:
        """
        Test 5 : Cohérence de la valence émotionnelle
        Vérifie que la valence rapportée correspond aux émotions
        """
        print("\n  🔍 Test de cohérence valence...")
        self.clear_states()

        # Test avec émotions positives
        positive = [0.1] * 24
        positive[17] = 0.9  # Joie
        positive[21] = 0.8  # Satisfaction
        positive[0] = 0.7   # Admiration

        self.send_emotions(positive)
        time.sleep(0.5)

        positive_valence = None
        if self.received_states:
            positive_valence = self.received_states[-1].get('valence', 0.5)

        self.clear_states()

        # Test avec émotions négatives
        negative = [0.1] * 24
        negative[20] = 0.9  # Tristesse
        negative[14] = 0.7  # Peur
        negative[10] = 0.6  # Dégoût

        self.send_emotions(negative)
        time.sleep(0.5)

        negative_valence = None
        if self.received_states:
            negative_valence = self.received_states[-1].get('valence', 0.5)

        # Vérifier la cohérence
        if positive_valence is not None and negative_valence is not None:
            # Positive devrait être > 0.5, négative < 0.5
            correct_positive = positive_valence > 0.5
            correct_negative = negative_valence < 0.6  # Tolérance
            correct_order = positive_valence > negative_valence

            score = (int(correct_positive) + int(correct_negative) + int(correct_order)) / 3
            passed = score >= 0.66

            return ValidationResult(
                test_name="Cohérence valence",
                passed=passed,
                score=score,
                details=f"Positive: {positive_valence:.2f}, Négative: {negative_valence:.2f}",
                metrics={'positive_valence': positive_valence, 'negative_valence': negative_valence}
            )
        else:
            return ValidationResult(
                test_name="Cohérence valence",
                passed=False,
                score=0.0,
                details="Données manquantes",
                metrics={}
            )

    def _create_emotion_vector(self, overrides: Dict[int, float]) -> List[float]:
        """Crée un vecteur d'émotions avec des valeurs spécifiques"""
        emotions = [0.1] * 24
        for idx, val in overrides.items():
            if 0 <= idx < 24:
                emotions[idx] = val
        return emotions


def main():
    print("╔" + "═" * 68 + "╗")
    print("║" + "  VALIDATION COMPORTEMENTALE MCEE  ".center(68) + "║")
    print("╚" + "═" * 68 + "╝")
    print(f"\n  Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")

    validator = BehavioralValidator()

    try:
        print("\n  Connexion à RabbitMQ...")
        validator.connect()
        print("  ✓ Connecté")

        print("  Démarrage du consumer...")
        validator.start_state_consumer()
        print("  ✓ Consumer démarré")

        # Exécuter les tests
        print("\n" + "=" * 70)
        print("  EXÉCUTION DES TESTS DE VALIDATION")
        print("=" * 70)

        results = []

        results.append(validator.test_temporal_coherence())
        results.append(validator.test_threat_reactivity())
        results.append(validator.test_mct_stability())
        results.append(validator.test_dominant_pattern_match())
        results.append(validator.test_emotional_valence_coherence())

        # Résumé
        print("\n" + "=" * 70)
        print("  RÉSULTATS DE VALIDATION")
        print("=" * 70)

        passed_count = 0
        total_score = 0.0

        for result in results:
            status = "✓ PASS" if result.passed else "✗ FAIL"
            print(f"\n  {status} {result.test_name}")
            print(f"       Score: {result.score:.2f}")
            print(f"       {result.details}")

            if result.passed:
                passed_count += 1
            total_score += result.score

        avg_score = total_score / len(results)

        print("\n" + "=" * 70)
        print("  VERDICT FINAL")
        print("=" * 70)
        print(f"\n  Tests réussis: {passed_count}/{len(results)}")
        print(f"  Score moyen: {avg_score:.2f}")

        if passed_count == len(results):
            print("\n  ✓ VALIDATION RÉUSSIE - Comportement cohérent")
        elif passed_count >= len(results) * 0.6:
            print("\n  ⚠ VALIDATION PARTIELLE - Ajustements recommandés")
        else:
            print("\n  ✗ VALIDATION ÉCHOUÉE - Révision nécessaire")

        print("\n" + "=" * 70)

    except pika.exceptions.AMQPConnectionError:
        print("  ✗ Impossible de se connecter à RabbitMQ")
        return 1
    except KeyboardInterrupt:
        print("\n  Interruption utilisateur")
    except Exception as e:
        print(f"  ✗ Erreur: {e}")
        import traceback
        traceback.print_exc()
        return 1
    finally:
        validator.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
