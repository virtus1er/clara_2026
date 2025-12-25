#!/usr/bin/env python3
"""
Test de vérification Neo4j - Vérifie si les données arrivent correctement
"""

import pika
import json
import uuid
import time
import sys

RABBITMQ_HOST = 'localhost'
RABBITMQ_USER = 'virtus'
RABBITMQ_PASS = 'virtus@83'
REQUEST_QUEUE = 'neo4j.requests.queue'
RESPONSE_EXCHANGE = 'neo4j.responses'

def test_neo4j_service():
    """Teste si le service Neo4j répond aux requêtes"""
    print("=" * 60)
    print("  TEST DE VÉRIFICATION NEO4J")
    print("=" * 60)

    try:
        # Connexion RabbitMQ
        params = pika.ConnectionParameters(
            host=RABBITMQ_HOST,
            credentials=pika.PlainCredentials(RABBITMQ_USER, RABBITMQ_PASS),
            connection_attempts=3,
            retry_delay=1
        )
        conn = pika.BlockingConnection(params)
        channel = conn.channel()
        print("✓ Connecté à RabbitMQ")

        # Créer une queue temporaire pour les réponses
        result = channel.queue_declare(queue='', exclusive=True)
        callback_queue = result.method.queue

        # Bind à l'exchange de réponses
        channel.exchange_declare(exchange=RESPONSE_EXCHANGE, exchange_type='direct', durable=True)
        channel.queue_bind(exchange=RESPONSE_EXCHANGE, queue=callback_queue, routing_key=callback_queue)

        responses = {}

        def on_response(ch, method, props, body):
            if props.correlation_id in responses:
                responses[props.correlation_id] = json.loads(body.decode())

        channel.basic_consume(queue=callback_queue, on_message_callback=on_response, auto_ack=True)

        # Test 1: Créer une session
        print("\n1. Test création de session...")
        request_id = f"TEST_{uuid.uuid4().hex[:8]}"
        responses[request_id] = None

        request = {
            "request_id": request_id,
            "request_type": "create_session",
            "payload": {"pattern": "TEST_SERENITE"},
            "timestamp": time.time()
        }

        channel.basic_publish(
            exchange='',
            routing_key=REQUEST_QUEUE,
            body=json.dumps(request),
            properties=pika.BasicProperties(
                reply_to=callback_queue,
                correlation_id=request_id,
                content_type='application/json'
            )
        )

        # Attendre la réponse
        timeout = time.time() + 5
        while responses[request_id] is None and time.time() < timeout:
            conn.process_data_events(time_limit=0.1)

        if responses[request_id]:
            resp = responses[request_id]
            if resp.get('success'):
                session_id = resp.get('data', {}).get('id', 'N/A')
                print(f"   ✓ Session créée: {session_id}")
                print(f"   Temps d'exécution: {resp.get('execution_time_ms', 0):.2f}ms")
            else:
                print(f"   ✗ Erreur: {resp.get('error', 'Unknown')}")
        else:
            print("   ✗ Timeout - pas de réponse du service Neo4j")
            print("   Le service neodocjtext est-il démarré ?")
            conn.close()
            return False

        # Test 2: Créer une mémoire
        print("\n2. Test création de mémoire...")
        request_id = f"TEST_{uuid.uuid4().hex[:8]}"
        responses[request_id] = None

        emotions = [0.1] * 24
        emotions[17] = 0.8  # Joie
        emotions[8] = 0.6   # Calme

        request = {
            "request_id": request_id,
            "request_type": "create_memory",
            "payload": {
                "id": f"MEM_TEST_{int(time.time())}",
                "emotions": emotions,
                "dominant": "Joie",
                "intensity": 0.8,
                "valence": 0.7,
                "weight": 0.6,
                "context": "Test de création de mémoire via RabbitMQ",
                "type": "MCT"
            },
            "timestamp": time.time()
        }

        channel.basic_publish(
            exchange='',
            routing_key=REQUEST_QUEUE,
            body=json.dumps(request),
            properties=pika.BasicProperties(
                reply_to=callback_queue,
                correlation_id=request_id,
                content_type='application/json'
            )
        )

        timeout = time.time() + 5
        while responses[request_id] is None and time.time() < timeout:
            conn.process_data_events(time_limit=0.1)

        if responses[request_id]:
            resp = responses[request_id]
            if resp.get('success'):
                mem_id = resp.get('data', {}).get('id', 'N/A')
                keywords = resp.get('data', {}).get('keywords_extracted', [])
                relations = resp.get('data', {}).get('relations_created', 0)
                print(f"   ✓ Mémoire créée: {mem_id}")
                print(f"   Mots-clés: {keywords}")
                print(f"   Relations: {relations}")
            else:
                print(f"   ✗ Erreur: {resp.get('error', 'Unknown')}")
        else:
            print("   ✗ Timeout")

        # Test 3: Statistiques MCT
        print("\n3. Test statistiques MCT...")
        request_id = f"TEST_{uuid.uuid4().hex[:8]}"
        responses[request_id] = None

        request = {
            "request_id": request_id,
            "request_type": "get_mct_stats",
            "payload": {},
            "timestamp": time.time()
        }

        channel.basic_publish(
            exchange='',
            routing_key=REQUEST_QUEUE,
            body=json.dumps(request),
            properties=pika.BasicProperties(
                reply_to=callback_queue,
                correlation_id=request_id,
                content_type='application/json'
            )
        )

        timeout = time.time() + 5
        while responses[request_id] is None and time.time() < timeout:
            conn.process_data_events(time_limit=0.1)

        if responses[request_id]:
            resp = responses[request_id]
            if resp.get('success'):
                data = resp.get('data', {})
                print(f"   ✓ Stats MCT récupérées:")
                print(f"     - Total mémoires: {data.get('total_count', 0)}")
                print(f"     - Poids moyen: {data.get('avg_weight', 0):.3f}")
                print(f"     - Intensité moyenne: {data.get('avg_intensity', 0):.3f}")
                print(f"     - Traumas: {data.get('trauma_count', 0)}")
            else:
                print(f"   ✗ Erreur: {resp.get('error', 'Unknown')}")
        else:
            print("   ✗ Timeout")

        conn.close()
        print("\n" + "=" * 60)
        print("  ✓ TESTS NEO4J TERMINÉS")
        print("=" * 60)
        return True

    except pika.exceptions.AMQPConnectionError as e:
        print(f"✗ Impossible de se connecter à RabbitMQ: {e}")
        print("  Vérifiez que RabbitMQ est démarré")
        return False
    except Exception as e:
        print(f"✗ Erreur: {e}")
        import traceback
        traceback.print_exc()
        return False

if __name__ == "__main__":
    success = test_neo4j_service()
    sys.exit(0 if success else 1)
