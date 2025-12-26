#!/usr/bin/env python3
"""
Test complet des modules MCEE: Conscience, ADDO, Decision
Envoie des scénarios émotionnels et vérifie les réponses du système
"""

import pika
import json
import time
import sys
import threading
from datetime import datetime

# Configuration RabbitMQ
RABBITMQ_HOST = 'localhost'
RABBITMQ_USER = 'virtus'
RABBITMQ_PASS = 'virtus@83'

# Queues et Exchanges
DIMENSIONS_QUEUE = 'dimensions_queue'
EMOTIONS_EXCHANGE = 'mcee.emotional.input'
EMOTIONS_ROUTING_KEY = 'emotions.predictions'
SPEECH_EXCHANGE = 'mcee.speech.input'
SPEECH_ROUTING_KEY = 'speech.text'
OUTPUT_EXCHANGE = 'mcee.emotional.output'
SNAPSHOT_EXCHANGE = 'mcee.mct.snapshot'

# Neo4j
NEO4J_REQUEST_QUEUE = 'neo4j.requests.queue'
NEO4J_RESPONSE_EXCHANGE = 'neo4j.responses'


class MCEEModuleTester:
    """Testeur complet des modules MCEE"""

    def __init__(self):
        self.credentials = pika.PlainCredentials(RABBITMQ_USER, RABBITMQ_PASS)
        self.params = pika.ConnectionParameters(
            host=RABBITMQ_HOST,
            credentials=self.credentials
        )
        self.received_outputs = []
        self.received_snapshots = []
        self.consumer_running = False

    def connect(self):
        """Établit la connexion RabbitMQ"""
        self.connection = pika.BlockingConnection(self.params)
        self.channel = self.connection.channel()

        # Déclarer les exchanges d'entrée pour pouvoir y publier
        self.channel.exchange_declare(exchange=EMOTIONS_EXCHANGE, exchange_type='topic', durable=True)
        self.channel.exchange_declare(exchange=SPEECH_EXCHANGE, exchange_type='topic', durable=True)

        return True

    def close(self):
        """Ferme la connexion"""
        self.consumer_running = False
        if hasattr(self, 'connection') and self.connection.is_open:
            self.connection.close()

    def start_output_consumer(self):
        """Démarre un consommateur pour capturer les sorties MCEE"""
        def consumer_thread():
            try:
                conn = pika.BlockingConnection(self.params)
                ch = conn.channel()

                # Déclarer les exchanges (topic pour les deux)
                ch.exchange_declare(exchange=OUTPUT_EXCHANGE, exchange_type='topic', durable=True)
                ch.exchange_declare(exchange=SNAPSHOT_EXCHANGE, exchange_type='topic', durable=True)

                # Queue temporaire pour les sorties
                result = ch.queue_declare(queue='', exclusive=True)
                output_queue = result.method.queue
                ch.queue_bind(exchange=OUTPUT_EXCHANGE, queue=output_queue, routing_key='#')

                # Queue temporaire pour les snapshots
                result2 = ch.queue_declare(queue='', exclusive=True)
                snapshot_queue = result2.method.queue
                ch.queue_bind(exchange=SNAPSHOT_EXCHANGE, queue=snapshot_queue, routing_key='mct.#')

                def on_output(ch, method, props, body):
                    try:
                        data = json.loads(body.decode())
                        self.received_outputs.append({
                            'timestamp': datetime.now().isoformat(),
                            'routing_key': method.routing_key,
                            'data': data
                        })
                        print(f"    📥 Output reçu: {method.routing_key}")
                    except Exception as e:
                        print(f"    ⚠️ Erreur parsing output: {e}")

                def on_snapshot(ch, method, props, body):
                    try:
                        data = json.loads(body.decode())
                        self.received_snapshots.append({
                            'timestamp': datetime.now().isoformat(),
                            'data': data
                        })
                        print(f"    📥 Snapshot reçu")
                    except Exception as e:
                        print(f"    ⚠️ Erreur parsing snapshot: {e}")

                ch.basic_consume(queue=output_queue, on_message_callback=on_output, auto_ack=True)
                ch.basic_consume(queue=snapshot_queue, on_message_callback=on_snapshot, auto_ack=True)

                self.consumer_running = True
                while self.consumer_running:
                    conn.process_data_events(time_limit=0.5)

                conn.close()
            except Exception as e:
                print(f"Erreur consumer: {e}")

        thread = threading.Thread(target=consumer_thread, daemon=True)
        thread.start()
        time.sleep(1.0)  # Attendre que le consumer démarre

    def send_emotions(self, emotions: list, name: str = "test"):
        """Envoie un vecteur de 24 émotions au MCEE"""
        message = {
            "emotions": emotions,
            "timestamp": time.time(),
            "source": "test_modules"
        }
        self.channel.basic_publish(
            exchange=EMOTIONS_EXCHANGE,
            routing_key=EMOTIONS_ROUTING_KEY,
            body=json.dumps(message)
        )
        print(f"    📤 Émotions envoyées: {name}")

    def send_speech(self, text: str):
        """Envoie un texte au module Speech"""
        message = {
            "text": text,
            "timestamp": time.time(),
            "source": "test_modules"
        }
        self.channel.basic_publish(
            exchange=SPEECH_EXCHANGE,
            routing_key=SPEECH_ROUTING_KEY,
            body=json.dumps(message)
        )
        print(f"    💬 Texte envoyé: \"{text[:50]}...\"" if len(text) > 50 else f"    💬 Texte envoyé: \"{text}\"")

    def send_dimensions(self, dimensions: dict):
        """Envoie des dimensions au module émotionnel"""
        self.channel.queue_declare(queue=DIMENSIONS_QUEUE, durable=True)
        self.channel.basic_publish(
            exchange='',
            routing_key=DIMENSIONS_QUEUE,
            body=json.dumps(dimensions)
        )
        print(f"    📊 Dimensions envoyées")

    def query_neo4j(self, request_type: str, payload: dict) -> dict:
        """Envoie une requête au service Neo4j et attend la réponse"""
        import uuid

        # Créer une queue de réponse temporaire
        result = self.channel.queue_declare(queue='', exclusive=True)
        callback_queue = result.method.queue

        self.channel.exchange_declare(exchange=NEO4J_RESPONSE_EXCHANGE, exchange_type='direct', durable=True)
        self.channel.queue_bind(exchange=NEO4J_RESPONSE_EXCHANGE, queue=callback_queue, routing_key=callback_queue)

        response = {'received': False, 'data': None}

        def on_response(ch, method, props, body):
            response['received'] = True
            response['data'] = json.loads(body.decode())

        self.channel.basic_consume(queue=callback_queue, on_message_callback=on_response, auto_ack=True)

        request_id = f"TEST_{uuid.uuid4().hex[:8]}"
        request = {
            "request_id": request_id,
            "request_type": request_type,
            "payload": payload,
            "timestamp": time.time()
        }

        self.channel.basic_publish(
            exchange='',
            routing_key=NEO4J_REQUEST_QUEUE,
            body=json.dumps(request),
            properties=pika.BasicProperties(
                reply_to=callback_queue,
                correlation_id=request_id
            )
        )

        timeout = time.time() + 5
        while not response['received'] and time.time() < timeout:
            self.connection.process_data_events(time_limit=0.1)

        return response['data'] if response['received'] else None


def test_conscience_module(tester: MCEEModuleTester):
    """
    Test du module Conscience (Wt - Niveau de conscience)

    Le module Conscience calcule le niveau de conscience global basé sur:
    - L'intensité émotionnelle
    - La complexité du traitement
    - L'intégration des informations
    """
    print("\n" + "=" * 70)
    print("  TEST MODULE CONSCIENCE (Wt)")
    print("=" * 70)
    print("  Le module Conscience évalue le niveau de conscience global")
    print("  Wt = f(intensité, complexité, intégration)")
    print("-" * 70)

    # Scénario 1: État de faible conscience (émotions basses)
    print("\n  📋 Scénario 1: Faible conscience (émotions faibles)")
    low_emotions = [0.1] * 24
    tester.send_emotions(low_emotions, "conscience_faible")
    time.sleep(0.5)

    # Scénario 2: État de haute conscience (émotions intenses)
    print("\n  📋 Scénario 2: Haute conscience (émotions intenses)")
    high_emotions = [0.1] * 24
    high_emotions[17] = 0.9  # Joie intense
    high_emotions[16] = 0.8  # Intérêt fort
    high_emotions[5] = 0.7   # Émerveillement
    tester.send_emotions(high_emotions, "conscience_haute")
    time.sleep(0.5)

    # Scénario 3: Conscience avec texte (intégration multimodale)
    print("\n  📋 Scénario 3: Conscience multimodale (émotions + texte)")
    tester.send_emotions(high_emotions, "conscience_multimodale")
    tester.send_speech("Je suis pleinement conscient de ce moment présent, je ressens une profonde joie.")
    time.sleep(1)

    print("\n  ✓ Tests Conscience envoyés")
    print("    Vérifiez dans les logs MCEE: [ConscienceEngine] Wt=...")


def test_addo_module(tester: MCEEModuleTester):
    """
    Test du module ADDO (Anticipation, Décision, Délibération, Observation)

    Le module ADDO gère:
    - Rs (Résilience): capacité à se remettre des perturbations
    - Les 16 variables d'état
    - L'adaptation aux changements émotionnels
    """
    print("\n" + "=" * 70)
    print("  TEST MODULE ADDO (Rs - Résilience)")
    print("=" * 70)
    print("  Le module ADDO gère la résilience et l'adaptation")
    print("  Rs = f(perturbations, récupération, stabilité)")
    print("-" * 70)

    # Scénario 1: État stable (haute résilience attendue)
    print("\n  📋 Scénario 1: État stable (résilience haute)")
    stable_emotions = [0.3] * 24
    stable_emotions[8] = 0.7   # Calme
    stable_emotions[21] = 0.6  # Satisfaction
    for i in range(5):
        tester.send_emotions(stable_emotions, f"stable_{i+1}")
        time.sleep(0.3)

    # Scénario 2: Perturbation soudaine (test de résilience)
    print("\n  📋 Scénario 2: Perturbation soudaine (choc émotionnel)")
    shock_emotions = [0.1] * 24
    shock_emotions[4] = 0.95   # Anxiété maximale
    shock_emotions[14] = 0.9   # Peur intense
    shock_emotions[15] = 0.85  # Horreur
    tester.send_emotions(shock_emotions, "perturbation_choc")
    time.sleep(0.5)

    # Scénario 3: Récupération progressive
    print("\n  📋 Scénario 3: Récupération progressive")
    recovery_steps = [
        ([0.1] * 24, "récup_1"),  # Encore perturbé
        ([0.2] * 24, "récup_2"),  # Début de récupération
        ([0.3] * 24, "récup_3"),  # En amélioration
        ([0.4] * 24, "récup_4"),  # Presque stable
    ]
    for emotions, name in recovery_steps:
        emotions[8] = 0.3 + 0.1 * recovery_steps.index((emotions, name))  # Calme croissant
        tester.send_emotions(emotions, name)
        time.sleep(0.4)

    print("\n  ✓ Tests ADDO envoyés")
    print("    Vérifiez dans les logs MCEE: [ADDO] Résilience: Rs=...")


def test_decision_module(tester: MCEEModuleTester):
    """
    Test du module Decision (Prise de décision émotionnelle)

    Le module Decision gère:
    - τ_max (temps de délibération maximal)
    - θ_veto (seuil de veto émotionnel)
    - θ_meta (seuil de métacognition)
    """
    print("\n" + "=" * 70)
    print("  TEST MODULE DECISION")
    print("=" * 70)
    print("  Le module Decision gère la prise de décision émotionnelle")
    print("  Paramètres: τ_max=5000ms, θ_veto=0.80, θ_meta=0.50")
    print("-" * 70)

    # Scénario 1: Décision simple (pas de conflit)
    print("\n  📋 Scénario 1: Décision simple (émotions cohérentes)")
    simple_emotions = [0.2] * 24
    simple_emotions[17] = 0.7  # Joie dominante
    simple_emotions[16] = 0.6  # Intérêt
    tester.send_emotions(simple_emotions, "decision_simple")
    tester.send_speech("Je veux continuer cette activité agréable.")
    time.sleep(0.5)

    # Scénario 2: Conflit émotionnel (approche-évitement)
    print("\n  📋 Scénario 2: Conflit émotionnel (approche-évitement)")
    conflict_emotions = [0.3] * 24
    conflict_emotions[17] = 0.7  # Joie (approche)
    conflict_emotions[14] = 0.6  # Peur (évitement)
    conflict_emotions[4] = 0.5   # Anxiété
    tester.send_emotions(conflict_emotions, "decision_conflit")
    tester.send_speech("Je veux y aller mais j'ai peur des conséquences.")
    time.sleep(0.5)

    # Scénario 3: Veto émotionnel (peur > seuil)
    print("\n  📋 Scénario 3: Veto émotionnel (peur > θ_veto=0.80)")
    veto_emotions = [0.2] * 24
    veto_emotions[14] = 0.85   # Peur > seuil veto
    veto_emotions[15] = 0.75   # Horreur
    tester.send_emotions(veto_emotions, "decision_veto")
    tester.send_speech("Non! C'est trop dangereux!")
    time.sleep(0.5)

    # Scénario 4: Métacognition (réflexion sur soi)
    print("\n  📋 Scénario 4: Métacognition (réflexion)")
    meta_emotions = [0.4] * 24
    meta_emotions[12] = 0.7   # Fascination
    meta_emotions[9] = 0.6    # Confusion (réflexion)
    meta_emotions[16] = 0.65  # Intérêt
    tester.send_emotions(meta_emotions, "decision_meta")
    tester.send_speech("Je me demande pourquoi je ressens cela, c'est étrange.")
    time.sleep(0.5)

    print("\n  ✓ Tests Decision envoyés")
    print("    Vérifiez dans les logs MCEE: [Decision] τ=..., veto=...")


def test_amyghaleon_module(tester: MCEEModuleTester):
    """
    Test du module Amyghaleon (Système d'urgence)

    Le module Amyghaleon gère les réponses d'urgence:
    - Détection de menace (Peur > 0.80)
    - Réponse fight/flight/freeze
    - Bypass de la délibération normale
    """
    print("\n" + "=" * 70)
    print("  TEST MODULE AMYGHALEON (Urgence)")
    print("=" * 70)
    print("  Le module Amyghaleon détecte les menaces et déclenche")
    print("  des réponses d'urgence (fight/flight/freeze)")
    print("-" * 70)

    # Scénario 1: Menace détectée (Peur > 0.80)
    print("\n  📋 Scénario 1: Menace détectée (réponse d'urgence)")
    threat_emotions = [0.1] * 24
    threat_emotions[14] = 0.90  # Peur intense
    threat_emotions[15] = 0.85  # Horreur
    threat_emotions[4] = 0.80   # Anxiété
    tester.send_emotions(threat_emotions, "menace_detectee")
    tester.send_speech("Attention! Danger imminent! Il faut fuir!")
    time.sleep(0.5)

    # Scénario 2: Colère intense (fight response)
    print("\n  📋 Scénario 2: Colère intense (réponse fight)")
    anger_emotions = [0.1] * 24
    anger_emotions[6] = 0.90   # Colère
    anger_emotions[22] = 0.85  # Rage (si disponible)
    tester.send_emotions(anger_emotions, "colere_intense")
    tester.send_speech("C'est inacceptable! Je ne laisserai pas faire!")
    time.sleep(0.5)

    # Scénario 3: Retour au calme après urgence
    print("\n  📋 Scénario 3: Retour au calme (désactivation urgence)")
    calm_emotions = [0.3] * 24
    calm_emotions[8] = 0.7   # Calme
    calm_emotions[19] = 0.6  # Soulagement
    tester.send_emotions(calm_emotions, "retour_calme")
    tester.send_speech("Ouf, le danger est passé, je peux me détendre.")
    time.sleep(0.5)

    print("\n  ✓ Tests Amyghaleon envoyés")
    print("    Vérifiez dans les logs MCEE: [Amyghaleon] URGENCE DÉCLENCHÉE...")


def test_mlt_patterns(tester: MCEEModuleTester):
    """
    Test des patterns MLT (Mémoire Long Terme)

    Teste la détection et l'évolution des patterns émotionnels:
    - SERENITE, JOIE, TRISTESSE, COLERE, PEUR, SURPRISE, DEGOUT, ANTICIPATION
    """
    print("\n" + "=" * 70)
    print("  TEST PATTERNS MLT")
    print("=" * 70)
    print("  Teste la détection des 8 patterns émotionnels de base")
    print("-" * 70)

    patterns = {
        'SERENITE': {'emotions': [8], 'values': [0.8], 'text': "Tout est paisible et serein."},
        'JOIE': {'emotions': [17, 3], 'values': [0.85, 0.7], 'text': "Je suis tellement heureux!"},
        'TRISTESSE': {'emotions': [20, 11], 'values': [0.8, 0.6], 'text': "Je me sens triste et mélancolique."},
        'COLERE': {'emotions': [6, 22], 'values': [0.85, 0.7], 'text': "C'est révoltant et injuste!"},
        'PEUR': {'emotions': [14, 4], 'values': [0.8, 0.7], 'text': "J'ai peur, quelque chose ne va pas."},
        'SURPRISE': {'emotions': [3, 19], 'values': [0.85, 0.6], 'text': "Oh! Je ne m'attendais pas à ça!"},
        'DEGOUT': {'emotions': [10, 5], 'values': [0.8, 0.65], 'text': "C'est dégoûtant et répugnant."},
        'ANTICIPATION': {'emotions': [7, 16], 'values': [0.8, 0.7], 'text': "J'attends avec impatience ce qui va arriver."},
    }

    for pattern_name, config in patterns.items():
        print(f"\n  📋 Pattern: {pattern_name}")
        emotions = [0.2] * 24
        for idx, val in zip(config['emotions'], config['values']):
            if idx < 24:
                emotions[idx] = val
        tester.send_emotions(emotions, f"pattern_{pattern_name}")
        tester.send_speech(config['text'])
        time.sleep(0.6)

    print("\n  ✓ Tests MLT Patterns envoyés")
    print("    Vérifiez dans les logs MCEE: [MLT Event] Pattern détecté...")


def test_neo4j_queries(tester: MCEEModuleTester):
    """
    Test des requêtes Neo4j pour vérifier le stockage
    """
    print("\n" + "=" * 70)
    print("  TEST REQUÊTES NEO4J")
    print("=" * 70)

    # Stats MCT
    print("\n  📊 Statistiques MCT:")
    response = tester.query_neo4j("get_mct_stats", {})
    if response and response.get('success'):
        data = response.get('data', {})
        print(f"    - Mémoires totales: {data.get('total_count', 0)}")
        print(f"    - Poids moyen: {data.get('avg_weight', 0):.3f}")
        print(f"    - Intensité moyenne: {data.get('avg_intensity', 0):.3f}")
        print(f"    - Traumas: {data.get('trauma_count', 0)}")
    else:
        print("    ✗ Erreur ou timeout")

    # Stats MLT
    print("\n  📊 Statistiques MLT:")
    response = tester.query_neo4j("get_mlt_stats", {})
    if response and response.get('success'):
        data = response.get('data', {})
        print(f"    - Mémoires consolidées: {data.get('total_count', 0)}")
        print(f"    - Score consolidation moyen: {data.get('avg_consolidation_score', 0):.3f}" if data.get('avg_consolidation_score') else "    - Score: N/A")
        by_cat = data.get('by_category', {})
        if by_cat:
            print(f"    - Par catégorie: {by_cat}")
    else:
        print("    ✗ Erreur ou timeout")

    print("\n  ✓ Requêtes Neo4j terminées")


def main():
    print("╔" + "═" * 68 + "╗")
    print("║" + "  TEST COMPLET DES MODULES MCEE  ".center(68) + "║")
    print("║" + "  Conscience, ADDO, Decision, Amyghaleon, MLT  ".center(68) + "║")
    print("╚" + "═" * 68 + "╝")
    print(f"\n  Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")

    tester = MCEEModuleTester()

    try:
        # Connexion
        print("\n  Connexion à RabbitMQ...")
        tester.connect()
        print("  ✓ Connecté")

        # Démarrer le consumer de sorties
        print("  Démarrage du consumer de sorties...")
        tester.start_output_consumer()
        print("  ✓ Consumer démarré")

        # Exécuter les tests
        test_conscience_module(tester)
        time.sleep(1)

        test_addo_module(tester)
        time.sleep(1)

        test_decision_module(tester)
        time.sleep(1)

        test_amyghaleon_module(tester)
        time.sleep(1)

        test_mlt_patterns(tester)
        time.sleep(2)

        test_neo4j_queries(tester)

        # Résumé
        print("\n" + "=" * 70)
        print("  RÉSUMÉ DES TESTS")
        print("=" * 70)
        print(f"  Sorties MCEE reçues: {len(tester.received_outputs)}")
        print(f"  Snapshots MCTGraph reçus: {len(tester.received_snapshots)}")

        if tester.received_outputs:
            print("\n  Dernières sorties:")
            for output in tester.received_outputs[-3:]:
                data = output.get('data', {})
                print(f"    - {output['routing_key']}: {data.get('dominant', 'N/A')}")

        print("\n" + "=" * 70)
        print("  ✓ TESTS TERMINÉS")
        print("=" * 70)
        print("\n  Consultez les logs MCEE pour voir les détails de chaque module.")

    except pika.exceptions.AMQPConnectionError:
        print("  ✗ Impossible de se connecter à RabbitMQ")
        print("    Vérifiez que RabbitMQ est démarré")
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
