#!/usr/bin/env python3
"""
Test de latence bout-en-bout pour le système MCEE
Mesure le temps entre l'émission d'une émotion et la réception de la réponse
"""

import pika
import json
import time
import sys
import threading
import statistics
from datetime import datetime

# Configuration RabbitMQ
RABBITMQ_HOST = 'localhost'
RABBITMQ_USER = 'virtus'
RABBITMQ_PASS = 'virtus@83'

# Queues et Exchanges
EMOTIONS_EXCHANGE = 'mcee.emotional.input'
EMOTIONS_ROUTING_KEY = 'emotions.predictions'
OUTPUT_EXCHANGE = 'mcee.emotional.output'


class LatencyTester:
    """Testeur de latence MCEE"""

    def __init__(self):
        self.credentials = pika.PlainCredentials(RABBITMQ_USER, RABBITMQ_PASS)
        self.params = pika.ConnectionParameters(
            host=RABBITMQ_HOST,
            credentials=self.credentials
        )
        self.response_times = {}
        self.latencies = []
        self.consumer_running = False

    def connect(self):
        """Établit la connexion RabbitMQ"""
        self.connection = pika.BlockingConnection(self.params)
        self.channel = self.connection.channel()

        # Déclarer les exchanges
        self.channel.exchange_declare(exchange=EMOTIONS_EXCHANGE, exchange_type='topic', durable=True)

        # Déclarer la queue MCEE
        self.channel.queue_declare(queue='mcee_emotions_queue', durable=True)
        self.channel.queue_bind(queue='mcee_emotions_queue', exchange=EMOTIONS_EXCHANGE, routing_key=EMOTIONS_ROUTING_KEY)

        return True

    def close(self):
        """Ferme la connexion"""
        self.consumer_running = False
        time.sleep(0.2)
        if hasattr(self, 'connection') and self.connection.is_open:
            self.connection.close()

    def start_response_consumer(self):
        """Démarre un consommateur pour capturer les réponses"""
        def consumer_thread():
            try:
                conn = pika.BlockingConnection(self.params)
                ch = conn.channel()

                ch.exchange_declare(exchange=OUTPUT_EXCHANGE, exchange_type='topic', durable=True)

                result = ch.queue_declare(queue='', exclusive=True)
                queue = result.method.queue
                ch.queue_bind(exchange=OUTPUT_EXCHANGE, queue=queue, routing_key='#')

                def on_response(ch, method, props, body):
                    receive_time = time.perf_counter()
                    try:
                        data = json.loads(body.decode())
                        # Chercher le test_id dans les données ou utiliser le dernier envoi
                        if self.response_times:
                            # Prendre le plus ancien envoi non traité
                            for test_id, send_time in list(self.response_times.items()):
                                latency = (receive_time - send_time) * 1000  # en ms
                                self.latencies.append(latency)
                                del self.response_times[test_id]
                                break
                    except Exception as e:
                        pass

                ch.basic_consume(queue=queue, on_message_callback=on_response, auto_ack=True)

                self.consumer_running = True
                while self.consumer_running:
                    conn.process_data_events(time_limit=0.1)

                conn.close()
            except Exception as e:
                print(f"Erreur consumer: {e}")

        thread = threading.Thread(target=consumer_thread, daemon=True)
        thread.start()
        time.sleep(0.5)

    def send_emotion_timed(self, test_id: str, emotions: list):
        """Envoie une émotion et enregistre le temps d'envoi"""
        message = {
            "emotions": emotions,
            "timestamp": time.time(),
            "source": "latency_test",
            "test_id": test_id
        }

        send_time = time.perf_counter()
        self.response_times[test_id] = send_time

        self.channel.basic_publish(
            exchange=EMOTIONS_EXCHANGE,
            routing_key=EMOTIONS_ROUTING_KEY,
            body=json.dumps(message)
        )

    def run_latency_test(self, num_samples: int = 50, interval_ms: int = 100):
        """Exécute le test de latence"""
        print(f"\n  Envoi de {num_samples} échantillons (intervalle: {interval_ms}ms)...")

        for i in range(num_samples):
            test_id = f"LAT_{i:04d}"
            # Créer des émotions variées pour chaque test
            emotions = [0.1 + (i % 10) * 0.05] * 24
            emotions[i % 24] = 0.8  # Une émotion dominante différente

            self.send_emotion_timed(test_id, emotions)
            time.sleep(interval_ms / 1000.0)

        # Attendre les dernières réponses
        print("  Attente des dernières réponses...")
        time.sleep(1.0)

        return self.latencies


def main():
    print("╔" + "═" * 68 + "╗")
    print("║" + "  TEST DE LATENCE MCEE  ".center(68) + "║")
    print("╚" + "═" * 68 + "╝")
    print(f"\n  Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")

    tester = LatencyTester()

    try:
        print("\n  Connexion à RabbitMQ...")
        tester.connect()
        print("  ✓ Connecté")

        print("  Démarrage du consumer de réponses...")
        tester.start_response_consumer()
        print("  ✓ Consumer démarré")

        # Test de latence
        print("\n" + "=" * 70)
        print("  TEST DE LATENCE BOUT-EN-BOUT")
        print("=" * 70)

        # Warmup
        print("\n  Phase de warmup (10 messages)...")
        tester.run_latency_test(num_samples=10, interval_ms=50)
        tester.latencies.clear()

        # Test réel
        print("\n  Phase de test (50 messages)...")
        latencies = tester.run_latency_test(num_samples=50, interval_ms=100)

        # Résultats
        print("\n" + "=" * 70)
        print("  RÉSULTATS")
        print("=" * 70)

        if latencies:
            print(f"\n  Échantillons collectés: {len(latencies)}")
            print(f"\n  Latence minimale:  {min(latencies):.2f} ms")
            print(f"  Latence maximale:  {max(latencies):.2f} ms")
            print(f"  Latence moyenne:   {statistics.mean(latencies):.2f} ms")
            print(f"  Latence médiane:   {statistics.median(latencies):.2f} ms")

            if len(latencies) > 1:
                print(f"  Écart-type:        {statistics.stdev(latencies):.2f} ms")

            # Distribution
            print("\n  Distribution:")
            ranges = [(0, 10), (10, 25), (25, 50), (50, 100), (100, 250), (250, float('inf'))]
            for low, high in ranges:
                count = sum(1 for l in latencies if low <= l < high)
                pct = count / len(latencies) * 100
                high_str = f"{high}ms" if high != float('inf') else "+"
                bar = "█" * int(pct / 5)
                print(f"    {low:3d}-{high_str:>5}: {count:3d} ({pct:5.1f}%) {bar}")

            # Verdict
            print("\n  Verdict:")
            avg = statistics.mean(latencies)
            if avg < 20:
                print("    ✓ Excellente latence (<20ms)")
            elif avg < 50:
                print("    ✓ Bonne latence (<50ms)")
            elif avg < 100:
                print("    ⚠ Latence acceptable (<100ms)")
            else:
                print("    ✗ Latence élevée (>100ms) - Optimisation recommandée")
        else:
            print("\n  ✗ Aucune réponse reçue - MCEE ne répond pas")

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
        tester.close()

    return 0


if __name__ == "__main__":
    sys.exit(main())
