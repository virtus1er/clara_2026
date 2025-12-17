#!/usr/bin/env python3
"""
Test script for the emotion prediction system.
Sends test dimensions via RabbitMQ and receives emotion predictions.
"""

import pika
import json
import threading
import time

# RabbitMQ configuration
RABBITMQ_HOST = "localhost"
RABBITMQ_PORT = 5672
RABBITMQ_USER = "virtus"
RABBITMQ_PASS = "virtus@83"

INPUT_QUEUE = "dimensions_queue"
OUTPUT_EXCHANGE = "mcee.emotional.input"
ROUTING_KEY = "emotions.predictions"

# 14 dimensions expected by the model (from model.json)
DIMENSIONS = [
    "approach", "arousal", "attention", "certainty", "commitment",
    "control", "dominance", "effort", "fairness", "identity",
    "obstruction", "safety", "upswing", "valence"
]

def get_connection():
    """Create RabbitMQ connection."""
    credentials = pika.PlainCredentials(RABBITMQ_USER, RABBITMQ_PASS)
    parameters = pika.ConnectionParameters(
        host=RABBITMQ_HOST,
        port=RABBITMQ_PORT,
        credentials=credentials
    )
    return pika.BlockingConnection(parameters)

def send_dimensions(dimensions: dict):
    """Send dimension values to the emotion predictor."""
    connection = get_connection()
    channel = connection.channel()

    channel.queue_declare(queue=INPUT_QUEUE, durable=True)

    message = json.dumps(dimensions)
    channel.basic_publish(
        exchange="",
        routing_key=INPUT_QUEUE,
        body=message,
        properties=pika.BasicProperties(delivery_mode=2)
    )

    print(f"Dimensions envoyées: {message}")
    connection.close()

def receive_predictions(timeout=10):
    """Listen for emotion predictions."""
    connection = get_connection()
    channel = connection.channel()

    channel.exchange_declare(
        exchange=OUTPUT_EXCHANGE,
        exchange_type="topic",
        durable=True
    )

    result = channel.queue_declare(queue="", exclusive=True)
    queue_name = result.method.queue

    channel.queue_bind(
        exchange=OUTPUT_EXCHANGE,
        queue=queue_name,
        routing_key=ROUTING_KEY
    )

    print(f"En attente des prédictions sur {OUTPUT_EXCHANGE}...")

    predictions_received = []

    def callback(ch, method, properties, body):
        predictions = json.loads(body)
        predictions_received.append(predictions)
        print("\n=== Prédictions reçues ===")
        print(f"{'Émotion':<30} | {'Valeur':>8}")
        print("-" * 42)
        for emotion, value in sorted(predictions.items()):
            bar = "█" * int(value * 20)
            print(f"{emotion:<30} | {value:>7.3f} {bar}")
        ch.stop_consuming()

    channel.basic_consume(
        queue=queue_name,
        on_message_callback=callback,
        auto_ack=True
    )

    # Set timeout
    connection.call_later(timeout, lambda: channel.stop_consuming())

    try:
        channel.start_consuming()
    except Exception:
        pass

    connection.close()
    return predictions_received

def test_emotion(name: str, dimensions: dict):
    """Run a complete test: send dimensions and receive predictions."""
    print(f"\n{'='*50}")
    print(f"TEST: {name}")
    print(f"{'='*50}")

    # Start receiver in background
    receiver_thread = threading.Thread(target=receive_predictions, args=(5,))
    receiver_thread.start()

    time.sleep(0.5)  # Let receiver connect

    # Send dimensions
    send_dimensions(dimensions)

    # Wait for response
    receiver_thread.join()

def main():
    print("=== Test du système de prédiction d'émotions ===\n")

    # Test 1: Joie (high valence, high arousal, positive approach)
    test_emotion("Joie", {
        "approach": 0.8,
        "arousal": 0.7,
        "attention": 0.6,
        "certainty": 0.8,
        "commitment": 0.7,
        "control": 0.7,
        "dominance": 0.6,
        "effort": 0.4,
        "fairness": 0.7,
        "identity": 0.6,
        "obstruction": 0.2,
        "safety": 0.8,
        "upswing": 0.8,
        "valence": 0.9
    })

    # Test 2: Tristesse (low valence, low arousal)
    test_emotion("Tristesse", {
        "approach": 0.2,
        "arousal": 0.3,
        "attention": 0.4,
        "certainty": 0.4,
        "commitment": 0.3,
        "control": 0.2,
        "dominance": 0.2,
        "effort": 0.3,
        "fairness": 0.4,
        "identity": 0.5,
        "obstruction": 0.7,
        "safety": 0.3,
        "upswing": 0.1,
        "valence": 0.2
    })

    # Test 3: Peur (low valence, high arousal, low safety)
    test_emotion("Peur", {
        "approach": 0.2,
        "arousal": 0.9,
        "attention": 0.9,
        "certainty": 0.2,
        "commitment": 0.4,
        "control": 0.1,
        "dominance": 0.1,
        "effort": 0.8,
        "fairness": 0.3,
        "identity": 0.4,
        "obstruction": 0.8,
        "safety": 0.1,
        "upswing": 0.1,
        "valence": 0.2
    })

    # Test 4: Calme (medium valence, low arousal, high safety)
    test_emotion("Calme", {
        "approach": 0.5,
        "arousal": 0.2,
        "attention": 0.3,
        "certainty": 0.7,
        "commitment": 0.5,
        "control": 0.7,
        "dominance": 0.5,
        "effort": 0.2,
        "fairness": 0.6,
        "identity": 0.5,
        "obstruction": 0.2,
        "safety": 0.8,
        "upswing": 0.5,
        "valence": 0.6
    })

    # Test 5: Colère (low valence, high arousal, high dominance)
    test_emotion("Colère", {
        "approach": 0.7,
        "arousal": 0.9,
        "attention": 0.8,
        "certainty": 0.7,
        "commitment": 0.8,
        "control": 0.3,
        "dominance": 0.8,
        "effort": 0.9,
        "fairness": 0.1,
        "identity": 0.7,
        "obstruction": 0.9,
        "safety": 0.4,
        "upswing": 0.2,
        "valence": 0.2
    })

    print("\n=== Tests terminés ===")

def interactive_mode():
    """Interactive mode to manually set dimensions."""
    print("=== Mode interactif ===")
    print("Entrez les valeurs des dimensions (0.0 à 1.0)")
    print("Tapez 'quit' pour quitter\n")

    while True:
        dimensions = {}
        print("\nNouvelle entrée:")

        for dim in DIMENSIONS:
            while True:
                val = input(f"  {dim}: ")
                if val.lower() == 'quit':
                    return
                try:
                    val = float(val)
                    if 0.0 <= val <= 1.0:
                        dimensions[dim] = val
                        break
                    else:
                        print("    Valeur doit être entre 0.0 et 1.0")
                except ValueError:
                    print("    Valeur invalide")

        # Start receiver
        receiver_thread = threading.Thread(target=receive_predictions, args=(5,))
        receiver_thread.start()

        time.sleep(0.5)
        send_dimensions(dimensions)
        receiver_thread.join()

if __name__ == "__main__":
    import sys

    if len(sys.argv) > 1 and sys.argv[1] == "-i":
        interactive_mode()
    else:
        main()
