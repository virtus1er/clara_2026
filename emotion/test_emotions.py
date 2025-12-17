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

# 14 dimensions expected by the model
DIMENSIONS = [
    "Valence", "Activation", "Dominance", "Certitude",
    "Nouveauté", "Pertinence", "Congruence", "Contrôle",
    "Agentivité", "Normativité", "Attention", "Effort",
    "Plaisir intrinsèque", "Signification"
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

    # Test 1: Joie (high valence, high activation)
    test_emotion("Joie", {
        "Valence": 0.9,
        "Activation": 0.8,
        "Dominance": 0.7,
        "Certitude": 0.8,
        "Nouveauté": 0.5,
        "Pertinence": 0.7,
        "Congruence": 0.8,
        "Contrôle": 0.7,
        "Agentivité": 0.6,
        "Normativité": 0.7,
        "Attention": 0.6,
        "Effort": 0.4,
        "Plaisir intrinsèque": 0.9,
        "Signification": 0.7
    })

    # Test 2: Tristesse (low valence, low activation)
    test_emotion("Tristesse", {
        "Valence": 0.2,
        "Activation": 0.3,
        "Dominance": 0.3,
        "Certitude": 0.4,
        "Nouveauté": 0.2,
        "Pertinence": 0.6,
        "Congruence": 0.3,
        "Contrôle": 0.2,
        "Agentivité": 0.3,
        "Normativité": 0.5,
        "Attention": 0.4,
        "Effort": 0.3,
        "Plaisir intrinsèque": 0.1,
        "Signification": 0.6
    })

    # Test 3: Peur (low valence, high activation)
    test_emotion("Peur", {
        "Valence": 0.2,
        "Activation": 0.9,
        "Dominance": 0.2,
        "Certitude": 0.3,
        "Nouveauté": 0.8,
        "Pertinence": 0.9,
        "Congruence": 0.2,
        "Contrôle": 0.1,
        "Agentivité": 0.2,
        "Normativité": 0.3,
        "Attention": 0.9,
        "Effort": 0.8,
        "Plaisir intrinsèque": 0.1,
        "Signification": 0.8
    })

    # Test 4: Calme (medium valence, low activation)
    test_emotion("Calme", {
        "Valence": 0.6,
        "Activation": 0.2,
        "Dominance": 0.5,
        "Certitude": 0.7,
        "Nouveauté": 0.2,
        "Pertinence": 0.4,
        "Congruence": 0.7,
        "Contrôle": 0.7,
        "Agentivité": 0.5,
        "Normativité": 0.6,
        "Attention": 0.3,
        "Effort": 0.2,
        "Plaisir intrinsèque": 0.5,
        "Signification": 0.4
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
