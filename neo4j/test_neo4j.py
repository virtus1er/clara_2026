#!/usr/bin/env python3
"""
Test script for Neo4j and RabbitMQ integration
Usage: python test_neo4j.py [--docker]
"""

import json
import os
import sys
import time
import uuid
import pika
from neo4j import GraphDatabase


def test_neo4j_connection(uri: str, user: str, password: str) -> bool:
    """Test direct Neo4j connection"""
    print(f"\n{'='*60}")
    print("TEST 1: Connexion directe à Neo4j")
    print(f"{'='*60}")

    try:
        if password:
            driver = GraphDatabase.driver(uri, auth=(user, password))
        else:
            driver = GraphDatabase.driver(uri)

        with driver.session() as session:
            result = session.run("RETURN 1 AS test")
            value = result.single()['test']
            assert value == 1, "Unexpected result"

        driver.close()
        print(f"✓ Connexion Neo4j OK ({uri})")
        return True

    except Exception as e:
        print(f"✗ Erreur Neo4j: {e}")
        return False


def test_rabbitmq_connection(host: str, user: str, password: str) -> bool:
    """Test RabbitMQ connection"""
    print(f"\n{'='*60}")
    print("TEST 2: Connexion à RabbitMQ")
    print(f"{'='*60}")

    try:
        credentials = pika.PlainCredentials(user, password)
        connection = pika.BlockingConnection(
            pika.ConnectionParameters(host=host, credentials=credentials)
        )
        channel = connection.channel()

        # Declare test queue
        channel.queue_declare(queue='test.queue', durable=False, auto_delete=True)

        connection.close()
        print(f"✓ Connexion RabbitMQ OK ({host})")
        return True

    except Exception as e:
        print(f"✗ Erreur RabbitMQ: {e}")
        return False


def test_neo4j_crud(uri: str, user: str, password: str) -> bool:
    """Test Neo4j CRUD operations"""
    print(f"\n{'='*60}")
    print("TEST 3: Opérations CRUD Neo4j")
    print(f"{'='*60}")

    try:
        if password:
            driver = GraphDatabase.driver(uri, auth=(user, password))
        else:
            driver = GraphDatabase.driver(uri)

        test_id = f"TEST_{uuid.uuid4().hex[:8]}"

        with driver.session() as session:
            # CREATE
            print("  → CREATE Memory node...")
            session.run("""
                CREATE (m:Memory:Test {
                    id: $id,
                    emotions: [0.5, 0.3, 0.2, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                               0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
                    dominant: 'Joie',
                    intensity: 0.7,
                    valence: 0.8,
                    weight: 0.5,
                    context: 'Test memory for integration testing',
                    created_at: datetime()
                })
            """, id=test_id)
            print(f"    ✓ Créé: {test_id}")

            # READ
            print("  → READ Memory node...")
            result = session.run("""
                MATCH (m:Memory {id: $id})
                RETURN m.dominant AS dominant, m.intensity AS intensity
            """, id=test_id)
            record = result.single()
            assert record['dominant'] == 'Joie', "Wrong dominant emotion"
            assert record['intensity'] == 0.7, "Wrong intensity"
            print(f"    ✓ Lu: dominant={record['dominant']}, intensity={record['intensity']}")

            # UPDATE
            print("  → UPDATE Memory node...")
            session.run("""
                MATCH (m:Memory {id: $id})
                SET m.weight = 0.9, m.activation_count = 1
            """, id=test_id)
            result = session.run("""
                MATCH (m:Memory {id: $id})
                RETURN m.weight AS weight
            """, id=test_id)
            weight = result.single()['weight']
            assert weight == 0.9, "Update failed"
            print(f"    ✓ Mis à jour: weight={weight}")

            # DELETE
            print("  → DELETE Memory node...")
            session.run("MATCH (m:Memory {id: $id}) DETACH DELETE m", id=test_id)
            result = session.run("MATCH (m:Memory {id: $id}) RETURN count(m) AS count", id=test_id)
            count = result.single()['count']
            assert count == 0, "Delete failed"
            print(f"    ✓ Supprimé")

        driver.close()
        print("✓ Opérations CRUD OK")
        return True

    except Exception as e:
        print(f"✗ Erreur CRUD: {e}")
        return False


def test_service_roundtrip(rabbitmq_host: str, rabbitmq_user: str, rabbitmq_pass: str) -> bool:
    """Test sending a request through RabbitMQ to Neo4j service"""
    print(f"\n{'='*60}")
    print("TEST 4: Communication via RabbitMQ (roundtrip)")
    print(f"{'='*60}")

    try:
        credentials = pika.PlainCredentials(rabbitmq_user, rabbitmq_pass)
        connection = pika.BlockingConnection(
            pika.ConnectionParameters(host=rabbitmq_host, credentials=credentials)
        )
        channel = connection.channel()

        # Setup
        request_queue = 'neo4j.requests.queue'
        response_exchange = 'neo4j.responses'

        channel.queue_declare(queue=request_queue, durable=True)
        channel.exchange_declare(exchange=response_exchange, exchange_type='direct', durable=True)

        # Create response queue
        result = channel.queue_declare(queue='', exclusive=True)
        callback_queue = result.method.queue
        channel.queue_bind(exchange=response_exchange, queue=callback_queue, routing_key=callback_queue)

        # Send request
        request_id = str(uuid.uuid4())
        test_id = f"ROUNDTRIP_TEST_{uuid.uuid4().hex[:8]}"

        request = {
            'request_id': request_id,
            'request_type': 'create_memory',
            'payload': {
                'id': test_id,
                'emotions': [0.8] + [0.0] * 23,
                'dominant': 'Joie',
                'intensity': 0.8,
                'valence': 0.9,
                'weight': 0.6,
                'context': 'Roundtrip test memory'
            }
        }

        print(f"  → Envoi requête: {request['request_type']} (id: {test_id[:20]}...)")

        channel.basic_publish(
            exchange='',
            routing_key=request_queue,
            body=json.dumps(request),
            properties=pika.BasicProperties(
                reply_to=callback_queue,
                correlation_id=request_id,
                content_type='application/json'
            )
        )

        # Wait for response
        response = None
        timeout = 10
        start = time.time()

        while time.time() - start < timeout:
            method, props, body = channel.basic_get(callback_queue, auto_ack=True)
            if body and props.correlation_id == request_id:
                response = json.loads(body.decode())
                break
            time.sleep(0.1)

        if response:
            if response.get('success'):
                print(f"    ✓ Réponse reçue: {response.get('data')}")

                # Cleanup: delete test memory
                cleanup_request = {
                    'request_id': str(uuid.uuid4()),
                    'request_type': 'delete_memory',
                    'payload': {'id': test_id, 'archive': False}
                }
                channel.basic_publish(
                    exchange='',
                    routing_key=request_queue,
                    body=json.dumps(cleanup_request),
                    properties=pika.BasicProperties(content_type='application/json')
                )
                print("    ✓ Nettoyage effectué")
            else:
                print(f"    ✗ Erreur service: {response.get('error')}")
                connection.close()
                return False
        else:
            print(f"    ✗ Timeout ({timeout}s) - le service Neo4j est-il démarré?")
            connection.close()
            return False

        connection.close()
        print("✓ Communication RabbitMQ OK")
        return True

    except Exception as e:
        print(f"✗ Erreur roundtrip: {e}")
        return False


def main():
    # Configuration
    docker_mode = '--docker' in sys.argv

    if docker_mode:
        neo4j_uri = os.getenv('NEO4J_URI', 'bolt://neo4j:7687')
        neo4j_user = os.getenv('NEO4J_USER', 'neo4j')
        neo4j_password = os.getenv('NEO4J_PASSWORD', '')
        rabbitmq_host = os.getenv('RABBITMQ_HOST', 'rabbitmq')
        rabbitmq_user = os.getenv('RABBITMQ_USER', 'virtus')
        rabbitmq_pass = os.getenv('RABBITMQ_PASS', 'virtus@83')
    else:
        neo4j_uri = os.getenv('NEO4J_URI', 'bolt://localhost:7687')
        neo4j_user = os.getenv('NEO4J_USER', 'neo4j')
        neo4j_password = os.getenv('NEO4J_PASSWORD', '')
        rabbitmq_host = os.getenv('RABBITMQ_HOST', 'localhost')
        rabbitmq_user = os.getenv('RABBITMQ_USER', 'virtus')
        rabbitmq_pass = os.getenv('RABBITMQ_PASS', 'virtus@83')

    print("\n" + "="*60)
    print("    TEST INTÉGRATION NEO4J / RABBITMQ")
    print("="*60)
    print(f"Mode: {'Docker' if docker_mode else 'Local'}")
    print(f"Neo4j: {neo4j_uri}")
    print(f"RabbitMQ: {rabbitmq_host}")

    results = []

    # Run tests
    results.append(("Neo4j Connection", test_neo4j_connection(neo4j_uri, neo4j_user, neo4j_password)))
    results.append(("RabbitMQ Connection", test_rabbitmq_connection(rabbitmq_host, rabbitmq_user, rabbitmq_pass)))

    if results[0][1]:  # Only run CRUD if connection works
        results.append(("Neo4j CRUD", test_neo4j_crud(neo4j_uri, neo4j_user, neo4j_password)))

    if results[0][1] and results[1][1]:  # Only run roundtrip if both connections work
        results.append(("Service Roundtrip", test_service_roundtrip(rabbitmq_host, rabbitmq_user, rabbitmq_pass)))

    # Summary
    print(f"\n{'='*60}")
    print("RÉSUMÉ")
    print(f"{'='*60}")

    all_passed = True
    for name, passed in results:
        status = "✓ PASS" if passed else "✗ FAIL"
        print(f"  {status}: {name}")
        if not passed:
            all_passed = False

    print(f"\n{'='*60}")
    if all_passed:
        print("TOUS LES TESTS RÉUSSIS ✓")
    else:
        print("CERTAINS TESTS ONT ÉCHOUÉ ✗")
    print(f"{'='*60}\n")

    return 0 if all_passed else 1


if __name__ == "__main__":
    sys.exit(main())
