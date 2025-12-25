#!/usr/bin/env python3
"""
Test direct de la communication Neo4j via RabbitMQ.
"""

import pika
import json
import time
import sys
from datetime import datetime

RABBITMQ_HOST = "localhost"
RABBITMQ_PORT = 5672
RABBITMQ_USER = "virtus"
RABBITMQ_PASS = "virtus@83"

REQUEST_QUEUE = "neo4j.requests.queue"
RESPONSE_EXCHANGE = "neo4j.responses"


def test_create_session():
    """Test création de session."""
    print("\n" + "=" * 60)
    print("TEST: Création de session Neo4j")
    print("=" * 60)

    credentials = pika.PlainCredentials(RABBITMQ_USER, RABBITMQ_PASS)
    params = pika.ConnectionParameters(
        host=RABBITMQ_HOST,
        port=RABBITMQ_PORT,
        credentials=credentials,
        heartbeat=600
    )

    try:
        # Connexion
        conn = pika.BlockingConnection(params)
        ch = conn.channel()

        # Déclarer la queue de requêtes
        ch.queue_declare(queue=REQUEST_QUEUE, durable=True)

        # Déclarer l'exchange de réponses
        ch.exchange_declare(exchange=RESPONSE_EXCHANGE, exchange_type='direct', durable=True)

        # Créer une queue temporaire pour les réponses
        result = ch.queue_declare(queue='', exclusive=True)
        response_queue = result.method.queue
        ch.queue_bind(exchange=RESPONSE_EXCHANGE, queue=response_queue, routing_key=response_queue)

        print(f"  Queue réponse: {response_queue}")

        # Préparer la requête
        request_id = f"TEST_{int(time.time() * 1000)}"
        request = {
            "request_id": request_id,
            "request_type": "create_session",
            "payload": {"pattern": "SERENITE"},
            "timestamp": datetime.now().isoformat()
        }

        print(f"  Request ID: {request_id}")
        print(f"  Requête: {json.dumps(request, indent=2)}")

        # Envoyer la requête
        ch.basic_publish(
            exchange='',
            routing_key=REQUEST_QUEUE,
            body=json.dumps(request),
            properties=pika.BasicProperties(
                content_type='application/json',
                reply_to=response_queue,
                correlation_id=request_id
            )
        )
        print("  ✓ Requête envoyée")

        # Attendre la réponse
        response_data = [None]

        def on_response(ch, method, props, body):
            print(f"\n  Réponse brute: {body.decode()}")
            try:
                response_data[0] = json.loads(body)
            except Exception as e:
                print(f"  ✗ Erreur parsing JSON: {e}")
            ch.basic_ack(delivery_tag=method.delivery_tag)
            ch.stop_consuming()

        ch.basic_consume(queue=response_queue, on_message_callback=on_response)

        # Timeout de 10 secondes
        conn.call_later(10, lambda: ch.stop_consuming())

        print("  Attente de la réponse...")
        ch.start_consuming()

        if response_data[0]:
            resp = response_data[0]
            print("\n  Réponse parsée:")
            print(f"    request_id: {resp.get('request_id')}")
            print(f"    success: {resp.get('success')}")
            print(f"    data: {resp.get('data')}")
            print(f"    error: {resp.get('error')}")
            print(f"    execution_time_ms: {resp.get('execution_time_ms')}")

            if resp.get('success') and resp.get('data', {}).get('id'):
                print(f"\n  ✓ Session créée: {resp['data']['id']}")
            else:
                print(f"\n  ✗ Échec: {resp.get('error')}")
        else:
            print("  ✗ Pas de réponse (timeout)")

        conn.close()

    except Exception as e:
        print(f"  ✗ Erreur: {e}")
        import traceback
        traceback.print_exc()


def test_create_memory():
    """Test création de mémoire."""
    print("\n" + "=" * 60)
    print("TEST: Création de mémoire Neo4j")
    print("=" * 60)

    credentials = pika.PlainCredentials(RABBITMQ_USER, RABBITMQ_PASS)
    params = pika.ConnectionParameters(
        host=RABBITMQ_HOST,
        port=RABBITMQ_PORT,
        credentials=credentials,
        heartbeat=600
    )

    try:
        conn = pika.BlockingConnection(params)
        ch = conn.channel()

        ch.queue_declare(queue=REQUEST_QUEUE, durable=True)
        ch.exchange_declare(exchange=RESPONSE_EXCHANGE, exchange_type='direct', durable=True)

        result = ch.queue_declare(queue='', exclusive=True)
        response_queue = result.method.queue
        ch.queue_bind(exchange=RESPONSE_EXCHANGE, queue=response_queue, routing_key=response_queue)

        # Requête de création mémoire
        request_id = f"TEST_MEM_{int(time.time() * 1000)}"
        emotions = [0.1] * 24
        emotions[17] = 0.8  # Joie
        emotions[9] = 0.5   # Intérêt

        request = {
            "request_id": request_id,
            "request_type": "create_memory",
            "payload": {
                "id": f"MEM_TEST_{int(time.time())}",
                "emotions": emotions,
                "dominant": "Joie",
                "intensity": 0.8,
                "valence": 0.7,
                "weight": 0.5,
                "context": "Test de création de mémoire depuis Python",
                "keywords": ["test", "mémoire", "python"]
            },
            "timestamp": datetime.now().isoformat()
        }

        print(f"  Request ID: {request_id}")

        ch.basic_publish(
            exchange='',
            routing_key=REQUEST_QUEUE,
            body=json.dumps(request),
            properties=pika.BasicProperties(
                content_type='application/json',
                reply_to=response_queue,
                correlation_id=request_id
            )
        )
        print("  ✓ Requête envoyée")

        response_data = [None]

        def on_response(ch, method, props, body):
            print(f"\n  Réponse brute: {body.decode()[:500]}")
            try:
                response_data[0] = json.loads(body)
            except Exception as e:
                print(f"  ✗ Erreur parsing JSON: {e}")
            ch.basic_ack(delivery_tag=method.delivery_tag)
            ch.stop_consuming()

        ch.basic_consume(queue=response_queue, on_message_callback=on_response)
        conn.call_later(10, lambda: ch.stop_consuming())

        print("  Attente de la réponse...")
        ch.start_consuming()

        if response_data[0]:
            resp = response_data[0]
            print("\n  Réponse:")
            print(f"    success: {resp.get('success')}")
            print(f"    data: {resp.get('data')}")
            if resp.get('success'):
                print(f"\n  ✓ Mémoire créée")
            else:
                print(f"\n  ✗ Échec: {resp.get('error')}")
        else:
            print("  ✗ Pas de réponse (timeout)")

        conn.close()

    except Exception as e:
        print(f"  ✗ Erreur: {e}")
        import traceback
        traceback.print_exc()


if __name__ == "__main__":
    test_create_session()
    test_create_memory()
    print("\n" + "=" * 60)
    print("Tests terminés")
    print("=" * 60)
