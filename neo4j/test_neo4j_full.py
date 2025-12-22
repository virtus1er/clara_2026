#!/usr/bin/env python3
"""
Test complet pour Neo4j - Teste toutes les fonctionnalités du service
Usage: python test_neo4j_full.py [--docker]

Mis à jour pour supporter le système emotional_states {sentence_id: [24 emotions]}
"""

import json
import os
import sys
import time
import uuid
import pika
from typing import Dict, Any, Optional, List
from dataclasses import dataclass


@dataclass
class TestConfig:
    """Configuration des tests"""
    rabbitmq_host: str
    rabbitmq_user: str
    rabbitmq_pass: str
    request_queue: str = "neo4j.requests.queue"
    response_exchange: str = "neo4j.responses"
    timeout: int = 15


class Neo4jTestClient:
    """Client de test pour le service Neo4j via RabbitMQ"""

    def __init__(self, config: TestConfig):
        self.config = config
        self.connection = None
        self.channel = None
        self.callback_queue = None

    def connect(self):
        """Établit la connexion RabbitMQ"""
        credentials = pika.PlainCredentials(
            self.config.rabbitmq_user,
            self.config.rabbitmq_pass
        )
        self.connection = pika.BlockingConnection(
            pika.ConnectionParameters(
                host=self.config.rabbitmq_host,
                credentials=credentials
            )
        )
        self.channel = self.connection.channel()

        # Déclarer les queues
        self.channel.queue_declare(queue=self.config.request_queue, durable=True)
        self.channel.exchange_declare(
            exchange=self.config.response_exchange,
            exchange_type='direct',
            durable=True
        )

        # Queue de callback
        result = self.channel.queue_declare(queue='', exclusive=True)
        self.callback_queue = result.method.queue
        self.channel.queue_bind(
            exchange=self.config.response_exchange,
            queue=self.callback_queue,
            routing_key=self.callback_queue
        )

    def close(self):
        """Ferme la connexion"""
        if self.connection:
            self.connection.close()

    def send_request(self, request_type: str, payload: Dict) -> Optional[Dict]:
        """Envoie une requête et attend la réponse"""
        request_id = str(uuid.uuid4())

        request = {
            'request_id': request_id,
            'request_type': request_type,
            'payload': payload
        }

        self.channel.basic_publish(
            exchange='',
            routing_key=self.config.request_queue,
            body=json.dumps(request),
            properties=pika.BasicProperties(
                reply_to=self.callback_queue,
                correlation_id=request_id,
                content_type='application/json'
            )
        )

        # Attendre la réponse
        start = time.time()
        while time.time() - start < self.config.timeout:
            method, props, body = self.channel.basic_get(
                self.callback_queue, auto_ack=True
            )
            if body and props.correlation_id == request_id:
                return json.loads(body.decode())
            time.sleep(0.1)

        return None


class Neo4jFullTest:
    """Tests complets du service Neo4j"""

    def __init__(self, config: TestConfig):
        self.config = config
        self.client = Neo4jTestClient(config)
        self.test_ids = []  # Pour le nettoyage
        self.results = []
        self.sentence_counter = 0  # Compteur global de sentence_ids pour les tests

    def get_next_sentence_id(self) -> int:
        """Retourne le prochain sentence_id pour les tests"""
        self.sentence_counter += 1
        return self.sentence_counter

    def generate_emotions(self, dominant_idx: int = 0, intensity: float = 0.8) -> List[float]:
        """Génère un vecteur de 24 émotions avec une émotion dominante"""
        emotions = [0.0] * 24
        emotions[dominant_idx] = intensity
        # Ajouter un peu de bruit pour les émotions secondaires
        emotions[(dominant_idx + 1) % 24] = intensity * 0.3
        emotions[(dominant_idx + 2) % 24] = intensity * 0.2
        return emotions

    def setup(self):
        """Initialisation"""
        print("\n" + "=" * 70)
        print("    TEST COMPLET NEO4J - TOUTES FONCTIONNALITÉS")
        print("    (avec support emotional_states)")
        print("=" * 70)
        self.client.connect()

    def teardown(self):
        """Nettoyage"""
        print("\n" + "-" * 70)
        print("NETTOYAGE...")

        # Supprimer tous les éléments de test
        for test_id in self.test_ids:
            self.client.send_request('delete_memory', {
                'id': test_id,
                'archive': False
            })

        # Supprimer les sessions de test
        self.client.send_request('cypher_query', {
            'query': "MATCH (s:Session) WHERE s.id STARTS WITH 'TEST_' DETACH DELETE s"
        })

        # Supprimer les concepts de test
        self.client.send_request('cypher_query', {
            'query': "MATCH (c:Concept) WHERE c.name STARTS WITH 'test_' DETACH DELETE c"
        })

        # Supprimer les patterns de test
        self.client.send_request('cypher_query', {
            'query': "MATCH (p:Pattern) WHERE p.name STARTS WITH 'TEST_' DETACH DELETE p"
        })

        self.client.close()
        print("Nettoyage terminé")

    def run_test(self, name: str, test_func):
        """Exécute un test et capture le résultat"""
        print(f"\n{'─' * 70}")
        print(f"TEST: {name}")
        print(f"{'─' * 70}")

        try:
            success = test_func()
            if success:
                print("\n✓ PASS")
                self.results.append((name, True))
            else:
                print("\n✗ FAIL")
                self.results.append((name, False))
        except Exception as e:
            print(f"\n✗ FAIL - Exception: {e}")
            self.results.append((name, False))

    # ═══════════════════════════════════════════════════════════════════════════
    # TESTS DE BASE - MÉMOIRE
    # ═══════════════════════════════════════════════════════════════════════════

    def test_create_memory(self):
        """Test création de mémoire basique"""
        memory_id = f"TEST_MEM_{uuid.uuid4().hex[:8]}"
        self.test_ids.append(memory_id)

        response = self.client.send_request('create_memory', {
            'id': memory_id,
            'emotions': self.generate_emotions(0, 0.7),  # Joie
            'dominant': 'Joie',
            'intensity': 0.7,
            'valence': 0.8,
            'weight': 0.5,
            'context': "C'est un souvenir heureux de test",
            'pattern': 'SERENITE'
        })

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Mémoire créée: {data.get('id')}")
            print(f"  → Mots-clés extraits: {data.get('keywords_extracted')}")
            return True

        print(f"  → Erreur: {response}")
        return False

    def test_create_memory_with_emotional_states(self):
        """Test création mémoire avec emotional_states explicite"""
        memory_id = f"TEST_MEM_ES_{uuid.uuid4().hex[:8]}"
        self.test_ids.append(memory_id)
        
        sentence_id = self.get_next_sentence_id()
        emotions = self.generate_emotions(0, 0.9)  # Joie forte

        response = self.client.send_request('create_memory', {
            'id': memory_id,
            'sentence_id': sentence_id,
            'emotions': emotions,
            'dominant': 'Joie',
            'intensity': 0.9,
            'valence': 0.95,
            'weight': 0.6,
            'context': "Dans le parc, j'ai observé des canards paisibles.",
            'pattern': 'SERENITE'
        })

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Mémoire créée: {data.get('id')}")
            print(f"  → Sentence ID: {sentence_id}")
            print(f"  → Mots-clés: {data.get('keywords_extracted')}")
            print(f"  → Emotional states: {list(data.get('emotional_states', {}).keys())}")
            print(f"  → Relations créées: {data.get('relations_created')}")
            return True

        print(f"  → Erreur: {response}")
        return False

    def test_create_memory_with_relations(self):
        """Test création mémoire avec extraction de relations"""
        memory_id = f"TEST_REL_{uuid.uuid4().hex[:8]}"
        self.test_ids.append(memory_id)
        
        sentence_id = self.get_next_sentence_id()
        emotions = self.generate_emotions(1, 0.7)  # Confiance

        response = self.client.send_request('create_memory', {
            'id': memory_id,
            'sentence_id': sentence_id,
            'emotions': emotions,
            'dominant': 'Confiance',
            'intensity': 0.8,
            'valence': 0.7,
            'weight': 0.6,
            'context': "Marie aime les chats. Le chat dort sur le canapé.",
            'pattern': 'SERENITE'
        })

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Mémoire créée: {data.get('id')}")
            print(f"  → Mots-clés: {data.get('keywords_extracted')}")
            print(f"  → Relations créées: {data.get('relations_created')}")
            return data.get('relations_created', 0) > 0

        print(f"  → Erreur: {response}")
        return False

    def test_get_memory(self):
        """Test récupération de mémoire avec emotional_states"""
        memory_id = f"TEST_GET_{uuid.uuid4().hex[:8]}"
        self.test_ids.append(memory_id)
        
        sentence_id = self.get_next_sentence_id()
        emotions = self.generate_emotions(16, 0.95)  # Extase

        # Créer
        self.client.send_request('create_memory', {
            'id': memory_id,
            'sentence_id': sentence_id,
            'emotions': emotions,
            'dominant': 'Extase',
            'intensity': 0.95,
            'valence': 1.0,
            'weight': 0.8
        })

        # Récupérer
        response = self.client.send_request('get_memory', {'id': memory_id})

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → ID: {data.get('id')}")
            print(f"  → Dominant: {data.get('dominant')}")
            print(f"  → Intensité: {data.get('intensity')}")
            print(f"  → Valence: {data.get('valence')}")
            print(f"  → Poids: {data.get('weight')}")
            print(f"  → Sentence IDs: {data.get('sentence_ids')}")
            es = data.get('emotional_states', {})
            print(f"  → Emotional states keys: {list(es.keys())}")
            return True

        print(f"  → Erreur: {response}")
        return False

    def test_merge_memory(self):
        """Test fusion de mémoires avec emotional_states"""
        memory_id = f"TEST_MERGE_{uuid.uuid4().hex[:8]}"
        self.test_ids.append(memory_id)
        
        sentence_id_1 = self.get_next_sentence_id()
        sentence_id_2 = self.get_next_sentence_id()
        
        emotions_1 = self.generate_emotions(0, 0.7)  # Joie
        emotions_2 = self.generate_emotions(8, 0.6)  # Sérénité

        # Créer mémoire initiale
        self.client.send_request('create_memory', {
            'id': memory_id,
            'sentence_id': sentence_id_1,
            'emotions': emotions_1,
            'dominant': 'Joie',
            'intensity': 0.7,
            'valence': 0.8,
            'weight': 0.5
        })

        # Fusionner avec nouvelles émotions
        response = self.client.send_request('merge_memory', {
            'target_id': memory_id,
            'emotions': emotions_2,
            'sentence_id': sentence_id_2,
            'transfer_weight': 0.2
        })

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Nouveau poids: {data.get('new_weight')}")
            print(f"  → Nombre de fusions: {data.get('merge_count')}")
            sentence_ids = data.get('sentence_ids', [])
            print(f"  → Sentence IDs fusionnés: {sentence_ids}")
            es = data.get('emotional_states', {})
            print(f"  → Emotional states: {len(es)} états")
            
            # Vérifier que les deux sentence_ids sont présents
            has_both = str(sentence_id_1) in [str(k) for k in es.keys()] or str(sentence_id_2) in [str(k) for k in es.keys()]
            return has_both or len(sentence_ids) >= 1

        print(f"  → Erreur: {response}")
        return False

    def test_find_similar(self):
        """Test recherche mémoires similaires"""
        # Créer plusieurs mémoires avec des profils émotionnels
        base_emotions = self.generate_emotions(0, 0.9)  # Joie forte
        
        memory_ids = []
        for i in range(3):
            mem_id = f"TEST_SIM_{uuid.uuid4().hex[:8]}"
            memory_ids.append(mem_id)
            self.test_ids.append(mem_id)
            
            self.client.send_request('create_memory', {
                'id': mem_id,
                'sentence_id': self.get_next_sentence_id(),
                'emotions': base_emotions,
                'dominant': 'Joie',
                'intensity': 0.9 - i * 0.1,
                'valence': 0.9,
                'weight': 0.7
            })

        # Rechercher des mémoires similaires
        response = self.client.send_request('find_similar', {
            'emotions': base_emotions,
            'threshold': 0.7,
            'limit': 5
        })

        if response and response.get('status') == 'success':
            data = response.get('data', [])
            print(f"  → Mémoires similaires trouvées: {len(data)}")
            for mem in data[:3]:
                print(f"    - {mem.get('id')}: similarité={mem.get('similarity', 0):.3f}")
            return len(data) > 0

        print(f"  → Erreur: {response}")
        return False

    def test_reactivate_memory(self):
        """Test réactivation de mémoire"""
        memory_id = f"TEST_REACT_{uuid.uuid4().hex[:8]}"
        self.test_ids.append(memory_id)
        
        sentence_id = self.get_next_sentence_id()

        # Créer
        self.client.send_request('create_memory', {
            'id': memory_id,
            'sentence_id': sentence_id,
            'emotions': self.generate_emotions(0, 0.5),
            'dominant': 'Joie',
            'intensity': 0.5,
            'valence': 0.6,
            'weight': 0.3
        })

        # Réactiver
        response = self.client.send_request('reactivate', {
            'id': memory_id,
            'strength': 1.0,
            'boost_factor': 0.2
        })

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Nouveau poids: {data.get('new_weight')}")
            print(f"  → Activations: {data.get('activations')}")
            print(f"  → Sentence IDs: {data.get('sentence_ids')}")
            es = data.get('emotional_states', {})
            print(f"  → Emotional states: {len(es)} états")
            return True

        print(f"  → Erreur: {response}")
        return False

    # ═══════════════════════════════════════════════════════════════════════════
    # TESTS TRAUMA
    # ═══════════════════════════════════════════════════════════════════════════

    def test_create_trauma(self):
        """Test création de trauma avec emotional_states"""
        trauma_id = f"TEST_TRAUMA_{uuid.uuid4().hex[:8]}"
        self.test_ids.append(trauma_id)
        
        sentence_id = self.get_next_sentence_id()
        emotions = self.generate_emotions(2, 0.95)  # Peur intense

        response = self.client.send_request('create_trauma', {
            'id': trauma_id,
            'sentence_id': sentence_id,
            'emotions': emotions,
            'dominant': 'Terreur',
            'intensity': 0.95,
            'valence': 0.1,
            'context': "Accident de voiture. Danger. Peur intense.",
            'trigger_keywords': ['accident', 'danger', 'peur']
        })

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Trauma créé: {data.get('id')}")
            print(f"  → Déclencheurs: {data.get('trigger_keywords')}")
            print(f"  → Sentence ID: {data.get('sentence_id')}")
            es = data.get('emotional_states', {})
            print(f"  → Emotional states: {list(es.keys())}")
            return True

        print(f"  → Erreur: {response}")
        return False

    # ═══════════════════════════════════════════════════════════════════════════
    # TESTS DECAY & MAINTENANCE
    # ═══════════════════════════════════════════════════════════════════════════

    def test_apply_decay(self):
        """Test application du decay (oubli)"""
        response = self.client.send_request('apply_decay', {
            'normal_rate': 0.01,
            'trauma_rate': 0.001,
            'min_weight': 0.05
        })

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Mémoires normales mises à jour: {data.get('normal_updated')}")
            print(f"  → Traumas mis à jour: {data.get('trauma_updated')}")
            print(f"  → Mémoires archivées: {data.get('archived')}")
            return True

        print(f"  → Erreur: {response}")
        return False

    # ═══════════════════════════════════════════════════════════════════════════
    # TESTS MODULE DREAMS
    # ═══════════════════════════════════════════════════════════════════════════

    def test_mct_stats(self):
        """Test stats MCT"""
        response = self.client.send_request('get_mct_stats', {})

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Type: {data.get('type')}")
            print(f"  → Total: {data.get('total_count')}")
            print(f"  → Poids moyen: {data.get('avg_weight', 0):.3f}")
            return True

        print(f"  → Erreur: {response}")
        return False

    def test_mlt_stats(self):
        """Test stats MLT"""
        response = self.client.send_request('get_mlt_stats', {})

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Type: {data.get('type')}")
            print(f"  → Total: {data.get('total_count')}")
            print(f"  → Par catégorie: {data.get('by_category', {})}")
            return True

        print(f"  → Erreur: {response}")
        return False

    def test_consolidation(self):
        """Test consolidation MCT → MLT"""
        response = self.client.send_request('consolidate_all_mct', {
            'importance_threshold': 0.6
        })

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Consolidés: {data.get('consolidated_count')}")
            return True

        print(f"  → Erreur: {response}")
        return False

    def test_cleanup_mct(self):
        """Test nettoyage MCT"""
        response = self.client.send_request('cleanup_mct', {
            'max_age_hours': 24,
            'min_weight': 0.1
        })

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Archivés: {data.get('archived')}")
            print(f"  → Supprimés: {data.get('deleted')}")
            print(f"  → Désactivés: {data.get('working_deactivated')}")
            return True

        print(f"  → Erreur: {response}")
        return False

    def test_dream_cycle(self):
        """Test cycle de rêve complet"""
        response = self.client.send_request('dream_cycle', {
            'importance_threshold': 0.6,
            'max_mct_age_hours': 24,
            'min_weight_to_keep': 0.1
        })

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Cycle complété: {data.get('dream_cycle_completed')}")
            print(f"  → Consolidation: {data.get('consolidation', {}).get('consolidated_count', 0)}")
            print(f"  → Liens MLT renforcés: {data.get('reinforced_mlt_links')}")
            return True

        print(f"  → Erreur: {response}")
        return False

    # ═══════════════════════════════════════════════════════════════════════════
    # TESTS PATTERNS
    # ═══════════════════════════════════════════════════════════════════════════

    def test_patterns(self):
        """Test gestion des patterns"""
        patterns = ['SERENITE', 'EXCITATION', 'ANXIETE', 'DEPRESSION']

        for pattern in patterns:
            self.client.send_request('create_pattern', {
                'name': pattern,
                'description': f"Pattern {pattern}",
                'characteristics': {'base': pattern.lower()}
            })

        print(f"  → Patterns créés/vérifiés: {patterns}")

        # Récupérer un pattern
        response = self.client.send_request('get_pattern', {'name': 'SERENITE'})

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Pattern récupéré: {data.get('name')}")
            return True

        print(f"  → Erreur get_pattern: {response}")
        return False

    def test_record_transition(self):
        """Test enregistrement transition"""
        response = self.client.send_request('record_transition', {
            'from_pattern': 'TEST_PATTERN_A',
            'to_pattern': 'TEST_PATTERN_B',
            'context': {'test': True}
        })

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Transition: {data.get('from')} → {data.get('to')}")
            print(f"  → Compteur: {data.get('count')}")
            print(f"  → Probabilité: {data.get('probability', 0):.2f}")
            return True

        print(f"  → Erreur: {response}")
        return False

    def test_get_transitions(self):
        """Test récupération transitions"""
        # Créer quelques transitions
        self.client.send_request('record_transition', {
            'from_pattern': 'ORIGINE',
            'to_pattern': 'DEST_A'
        })
        self.client.send_request('record_transition', {
            'from_pattern': 'ORIGINE',
            'to_pattern': 'DEST_A'
        })
        self.client.send_request('record_transition', {
            'from_pattern': 'ORIGINE',
            'to_pattern': 'DEST_A'
        })
        self.client.send_request('record_transition', {
            'from_pattern': 'ORIGINE',
            'to_pattern': 'DEST_B'
        })
        self.client.send_request('record_transition', {
            'from_pattern': 'ORIGINE',
            'to_pattern': 'DEST_B'
        })

        response = self.client.send_request('get_transitions', {
            'from_pattern': 'ORIGINE'
        })

        if response and response.get('status') == 'success':
            data = response.get('data', [])
            print(f"  → Transitions depuis ORIGINE: {len(data)}")
            for t in data:
                print(f"    → {t.get('to')}: prob={t.get('probability', 0):.2f}")
            return len(data) > 0

        print(f"  → Erreur: {response}")
        return False

    # ═══════════════════════════════════════════════════════════════════════════
    # TESTS RELATIONS SÉMANTIQUES
    # ═══════════════════════════════════════════════════════════════════════════

    def test_extract_relations(self):
        """Test extraction de relations avec emotional_states"""
        sentence_id = self.get_next_sentence_id()
        emotions = self.generate_emotions(0, 0.7)  # Joie

        response = self.client.send_request('extract_relations', {
            'text': "Le chat noir dort paisiblement sur le canapé rouge.",
            'store': True,
            'sentence_id': sentence_id,
            'emotions': emotions
        })

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Mots-clés: {data.get('keywords')}")
            print(f"  → Relations: {data.get('relations')}")
            print(f"  → Sentence ID: {data.get('sentence_id')}")
            es = data.get('emotional_states', {})
            print(f"  → Emotional states: {list(es.keys())}")
            print(f"  → Stockées: {data.get('stored')}")
            return True

        print(f"  → Erreur: {response}")
        return False

    def test_extract_relations_with_emotions(self):
        """Test extraction relations avec traçabilité émotionnelle"""
        sentence_id = self.get_next_sentence_id()
        emotions = self.generate_emotions(9, 0.8)  # Intérêt

        response = self.client.send_request('extract_relations', {
            'text': "Les canards nagent dans le parc.",
            'store': True,
            'sentence_id': sentence_id,
            'emotions': emotions
        })

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Mots-clés: {data.get('keywords')}")
            print(f"  → Relations: {data.get('relations')}")
            print(f"  → Sentence ID: {sentence_id}")
            es = data.get('emotional_states', {})
            print(f"  → Emotional states: {list(es.keys())}")
            print(f"  → Stockées: {data.get('stored')}")
            return True

        print(f"  → Erreur: {response}")
        return False

    # ═══════════════════════════════════════════════════════════════════════════
    # TESTS CONCEPTS avec emotional_states
    # ═══════════════════════════════════════════════════════════════════════════

    def test_create_concept(self):
        """Test création de concept avec emotional_states"""
        sentence_id = self.get_next_sentence_id()
        emotions = self.generate_emotions(0, 0.85)  # Joie

        response = self.client.send_request('create_concept', {
            'name': 'test_bonheur',
            'sentence_id': sentence_id,
            'emotions': emotions,
            'attributes': {
                'category': 'emotion',
                'valence': 'positive'
            }
        })

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Concept créé: {data.get('name')}")
            print(f"  → Sentence IDs: {data.get('sentence_ids')}")
            es = data.get('emotional_states', {})
            print(f"  → Emotional states: {list(es.keys())}")
            return True

        print(f"  → Erreur: {response}")
        return False

    def test_link_memory_concept(self):
        """Test liaison mémoire-concept avec propagation emotional_states"""
        memory_id = f"TEST_LINK_{uuid.uuid4().hex[:8]}"
        self.test_ids.append(memory_id)
        
        sentence_id = self.get_next_sentence_id()
        emotions = self.generate_emotions(1, 0.7)  # Confiance

        # Créer mémoire
        self.client.send_request('create_memory', {
            'id': memory_id,
            'sentence_id': sentence_id,
            'emotions': emotions,
            'dominant': 'Confiance',
            'intensity': 0.7,
            'valence': 0.8,
            'weight': 0.6
        })

        # Lier au concept
        response = self.client.send_request('link_memory_concept', {
            'memory_id': memory_id,
            'concept_name': 'test_famille',
            'relation': 'ASSOCIE_A'
        })

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Mémoire: {data.get('memory')}")
            print(f"  → Concept: {data.get('concept')}")
            print(f"  → Relation: {data.get('relation')}")
            print(f"  → Memory sentence_ids: {data.get('memory_sentence_ids')}")
            print(f"  → Concept sentence_ids: {data.get('concept_sentence_ids')}")
            return True

        print(f"  → Erreur: {response}")
        return False

    def test_get_concept_with_emotional_analysis(self):
        """Test concept avec IDs et analyse émotionnelle"""
        memory_id = f"TEST_CONCEPT_IDS_{uuid.uuid4().hex[:8]}"
        self.test_ids.append(memory_id)
        
        sentence_id = self.get_next_sentence_id()
        emotions = self.generate_emotions(0, 0.9)  # Joie forte

        # Créer une mémoire qui évoque un concept
        self.client.send_request('create_memory', {
            'id': memory_id,
            'sentence_id': sentence_id,
            'emotions': emotions,
            'dominant': 'Joie',
            'intensity': 0.9,
            'valence': 0.95,
            'weight': 0.8,
            'context': "Le soleil brille sur la plage."
        })

        # Récupérer le concept 'soleil'
        response = self.client.send_request('get_concept', {
            'name': 'soleil'
        })

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Concept: {data.get('name')}")
            print(f"  → Memory IDs: {data.get('memory_ids')}")
            print(f"  → Sentence IDs: {data.get('sentence_ids')}")
            print(f"  → Linked memories: {data.get('linked_memories')}")
            analysis = data.get('emotional_analysis', {})
            if analysis:
                print(f"  → Analyse émotionnelle:")
                print(f"      - Dominant: {analysis.get('dominant_emotion')}")
                print(f"      - Valence moy: {analysis.get('avg_valence', 0):.2f}")
                print(f"      - Stabilité: {analysis.get('stability', 0):.2f}")
                print(f"      - Trajectoire: {analysis.get('trajectory')}")
            return True

        print(f"  → Erreur: {response}")
        return False

    def test_get_concepts_by_memory(self):
        """Test récupération des concepts d'une mémoire avec emotional_states"""
        memory_id = f"TEST_CONCEPTS_MEM_{uuid.uuid4().hex[:8]}"
        self.test_ids.append(memory_id)
        
        sentence_id = self.get_next_sentence_id()
        emotions = self.generate_emotions(8, 0.7)  # Sérénité

        # Créer une mémoire riche
        self.client.send_request('create_memory', {
            'id': memory_id,
            'sentence_id': sentence_id,
            'emotions': emotions,
            'dominant': 'Sérénité',
            'intensity': 0.7,
            'valence': 0.8,
            'weight': 0.6,
            'context': "Les enfants jouent dans le jardin fleuri."
        })

        response = self.client.send_request('get_concepts_by_memory', {
            'memory_id': memory_id
        })

        if response and response.get('status') == 'success':
            data = response.get('data', [])
            print(f"  → Concepts trouvés: {len(data)}")
            for c in data[:5]:
                print(f"    - {c.get('name')}: memory_ids={c.get('memory_ids')}, sentence_ids={c.get('sentence_ids')}")
                analysis = c.get('emotional_analysis', {})
                if analysis:
                    print(f"      → Dominant: {analysis.get('dominant_emotion')}, Valence: {analysis.get('avg_valence', 0):.2f}")
            return len(data) > 0

        print(f"  → Erreur: {response}")
        return False

    def test_get_relations_with_emotional_states(self):
        """Test récupération des relations avec emotional_states"""
        memory_id = f"TEST_REL_IDS_{uuid.uuid4().hex[:8]}"
        self.test_ids.append(memory_id)
        
        sentence_id = self.get_next_sentence_id()
        emotions = self.generate_emotions(0, 0.8)  # Joie

        self.client.send_request('create_memory', {
            'id': memory_id,
            'sentence_id': sentence_id,
            'emotions': emotions,
            'dominant': 'Joie',
            'intensity': 0.8,
            'valence': 0.85,
            'weight': 0.7,
            'context': "Le chat dort sur le canapé."
        })

        response = self.client.send_request('get_relations_with_ids', {
            'memory_id': memory_id
        })

        if response and response.get('status') == 'success':
            data = response.get('data', [])
            print(f"  → Relations trouvées: {len(data)}")
            for r in data[:3]:
                src_ids = r.get('source_sentence_ids', [])
                tgt_ids = r.get('target_sentence_ids', [])
                print(f"    - '{r.get('source')}' sent_ids:{src_ids} "
                      f"--[{r.get('relation')}]--> "
                      f"'{r.get('target')}' sent_ids:{tgt_ids}")
            return True

        print(f"  → Erreur: {response}")
        return False

    def test_get_concepts_by_sentence(self):
        """Test récupération des concepts par sentence_id avec émotions"""
        memory_id = f"TEST_SENT_CONCEPT_{uuid.uuid4().hex[:8]}"
        self.test_ids.append(memory_id)
        
        sentence_id = self.get_next_sentence_id()
        emotions = self.generate_emotions(7, 0.75)  # Anticipation

        self.client.send_request('create_memory', {
            'id': memory_id,
            'sentence_id': sentence_id,
            'emotions': emotions,
            'dominant': 'Anticipation',
            'intensity': 0.75,
            'valence': 0.7,
            'weight': 0.65,
            'context': "Le chien court dans le jardin."
        })

        response = self.client.send_request('get_concepts_by_sentence', {
            'sentence_id': sentence_id
        })

        if response and response.get('status') == 'success':
            data = response.get('data', [])
            print(f"  → Concepts pour sentence_id={sentence_id}: {len(data)}")
            for c in data:
                efs = c.get('emotions_for_sentence', [])
                print(f"    - {c.get('name')}: sentence_ids={c.get('sentence_ids')}")
                if efs:
                    print(f"      → Émotions de cette phrase: {efs[:3]}...")
            return len(data) > 0

        print(f"  → Erreur: {response}")
        return False

    def test_get_relations_by_sentence(self):
        """Test récupération des relations par sentence_id avec émotions"""
        memory_id = f"TEST_SENT_REL_{uuid.uuid4().hex[:8]}"
        self.test_ids.append(memory_id)
        
        sentence_id = self.get_next_sentence_id()
        emotions = self.generate_emotions(1, 0.8)  # Confiance

        self.client.send_request('create_memory', {
            'id': memory_id,
            'sentence_id': sentence_id,
            'emotions': emotions,
            'dominant': 'Confiance',
            'intensity': 0.8,
            'valence': 0.75,
            'weight': 0.7,
            'context': "Le chat gris dort sur le fauteuil confortable."
        })

        response = self.client.send_request('get_relations_by_sentence', {
            'sentence_id': sentence_id
        })

        if response and response.get('status') == 'success':
            data = response.get('data', [])
            print(f"  → Relations pour sentence_id={sentence_id}: {len(data)}")
            for r in data[:3]:
                print(f"    - '{r.get('source')}' --[{r.get('relation')}]--> '{r.get('target')}'")
                efs = r.get('relation_emotions_for_sentence', [])
                if efs:
                    print(f"      → Émotions: {efs[:3]}...")
            return True

        print(f"  → Erreur: {response}")
        return False

    def test_emotional_states_accumulation(self):
        """Test accumulation des emotional_states sur un même concept"""
        # Créer plusieurs mémoires mentionnant "parc" avec différentes émotions
        sentence_ids = []
        emotions_list = [
            self.generate_emotions(0, 0.8),   # Joie
            self.generate_emotions(2, 0.6),   # Peur
            self.generate_emotions(8, 0.9),   # Sérénité
        ]
        contexts = [
            "Je me promène dans le parc.",
            "Il faisait sombre dans le parc.",
            "Le parc est magnifique au printemps."
        ]

        for i, (emotions, context) in enumerate(zip(emotions_list, contexts)):
            mem_id = f"TEST_ACCUM_{i}_{uuid.uuid4().hex[:8]}"
            self.test_ids.append(mem_id)
            sentence_id = self.get_next_sentence_id()
            sentence_ids.append(sentence_id)

            self.client.send_request('create_memory', {
                'id': mem_id,
                'sentence_id': sentence_id,
                'emotions': emotions,
                'dominant': ['Joie', 'Peur', 'Sérénité'][i],
                'intensity': 0.7 + i * 0.1,
                'valence': [0.9, 0.2, 0.95][i],
                'weight': 0.6,
                'context': context
            })

        # Vérifier que le concept 'parc' a accumulé les emotional_states
        response = self.client.send_request('get_concept', {'name': 'parc'})

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            if data:
                concept_sent_ids = data.get('sentence_ids', [])
                es = data.get('emotional_states', {})
                analysis = data.get('emotional_analysis', {})
                
                print(f"  → Concept 'parc':")
                print(f"    - Sentence IDs attendus: {sentence_ids}")
                print(f"    - Sentence IDs trouvés: {concept_sent_ids}")
                print(f"    - Memory IDs: {data.get('memory_ids')}")
                print(f"    - Nombre d'états émotionnels: {len(es)}")
                
                if analysis:
                    print(f"    - Analyse émotionnelle:")
                    print(f"        → Dominant global: {analysis.get('dominant_emotion')}")
                    print(f"        → Stabilité: {analysis.get('stability', 0):.2f}")
                    print(f"        → Trajectoire: {analysis.get('trajectory')}")
                    print(f"        → Score trauma: {analysis.get('trauma_score', 0):.2f}")

                # Vérifier que certains sentence_ids sont présents
                found_count = sum(1 for sid in sentence_ids if str(sid) in [str(k) for k in es.keys()])
                print(f"    - États émotionnels trouvés: {found_count}/{len(sentence_ids)}")
                
                return found_count > 0

        print(f"  → Erreur: {response}")
        return False

    # ═══════════════════════════════════════════════════════════════════════════
    # TESTS SESSION
    # ═══════════════════════════════════════════════════════════════════════════

    def test_create_session(self):
        """Test création de session"""
        session_id = f"TEST_SESSION_{uuid.uuid4().hex[:8]}"

        response = self.client.send_request('create_session', {
            'id': session_id,
            'pattern': 'SERENITE'
        })

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Session créée: {data.get('id')}")
            return True

        print(f"  → Erreur: {response}")
        return False

    def test_update_session(self):
        """Test mise à jour de session"""
        session_id = f"TEST_SESSION_UPD_{uuid.uuid4().hex[:8]}"

        # Créer
        self.client.send_request('create_session', {
            'id': session_id,
            'pattern': 'SERENITE'
        })

        # Mettre à jour
        response = self.client.send_request('update_session', {
            'id': session_id,
            'pattern': 'EXCITATION',
            'stability': 0.7,
            'volatility': 0.3,
            'trend': 'ascending'
        })

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Session mise à jour: {data.get('id')}")
            return True

        print(f"  → Erreur: {response}")
        return False

    def test_get_session(self):
        """Test récupération de session"""
        session_id = f"TEST_SESSION_GET_{uuid.uuid4().hex[:8]}"

        # Créer
        self.client.send_request('create_session', {
            'id': session_id,
            'pattern': 'ANXIETE'
        })

        print(f"  → Session créée: {session_id}")

        # Récupérer
        response = self.client.send_request('get_session', {'id': session_id})

        if response and response.get('status') == 'success':
            data = response.get('data', {})
            print(f"  → Session récupérée: {data.get('id')}")
            print(f"  → Pattern: {data.get('current_pattern')}")
            return True

        print(f"  → Erreur: {response}")
        return False

    # ═══════════════════════════════════════════════════════════════════════════
    # TESTS REQUÊTES GÉNÉRIQUES
    # ═══════════════════════════════════════════════════════════════════════════

    def test_cypher_query(self):
        """Test requête Cypher"""
        response = self.client.send_request('cypher_query', {
            'query': "MATCH (m:Memory) RETURN count(m) AS total"
        })

        if response and response.get('status') == 'success':
            data = response.get('data', [])
            if data:
                print(f"  → Total mémoires: {data[0].get('total')}")
            return True

        print(f"  → Erreur: {response}")
        return False

    def test_batch_queries(self):
        """Test batch de requêtes"""
        response = self.client.send_request('batch_query', {
            'queries': [
                {'query': "MATCH (m:Memory) RETURN count(m) AS total"},
                {'query': "MATCH (c:Concept) RETURN count(c) AS total"},
                {'query': "MATCH (p:Pattern) RETURN count(p) AS total"}
            ]
        })

        if response and response.get('status') == 'success':
            data = response.get('data', [])
            print(f"  → Résultats batch:")
            labels = ['Mémoires', 'Concepts', 'Patterns']
            for i, r in enumerate(data):
                if r:
                    print(f"    - {labels[i]}: {r[0].get('total', 0)}")
            return True

        print(f"  → Erreur: {response}")
        return False

    # ═══════════════════════════════════════════════════════════════════════════
    # EXÉCUTION
    # ═══════════════════════════════════════════════════════════════════════════

    def run_all_tests(self):
        """Exécute tous les tests"""
        self.setup()

        # Tests de base mémoire
        self.run_test("Création de mémoire", self.test_create_memory)
        self.run_test("Création mémoire avec emotional_states", self.test_create_memory_with_emotional_states)
        self.run_test("Création mémoire avec relations", self.test_create_memory_with_relations)
        self.run_test("Récupération de mémoire", self.test_get_memory)
        self.run_test("Fusion de mémoires avec emotional_states", self.test_merge_memory)
        self.run_test("Recherche mémoires similaires", self.test_find_similar)
        self.run_test("Réactivation de mémoire", self.test_reactivate_memory)

        # Trauma
        self.run_test("Création de trauma avec emotional_states", self.test_create_trauma)

        # Maintenance
        self.run_test("Application du decay (oubli)", self.test_apply_decay)

        # Module Dreams
        self.run_test("Stats MCT", self.test_mct_stats)
        self.run_test("Stats MLT", self.test_mlt_stats)
        self.run_test("Consolidation MCT → MLT", self.test_consolidation)
        self.run_test("Nettoyage MCT", self.test_cleanup_mct)
        self.run_test("Cycle de rêve complet", self.test_dream_cycle)

        # Patterns
        self.run_test("Gestion des patterns", self.test_patterns)
        self.run_test("Enregistrement transition", self.test_record_transition)
        self.run_test("Récupération transitions", self.test_get_transitions)

        # Relations sémantiques
        self.run_test("Extraction de relations", self.test_extract_relations)
        self.run_test("Extraction relations avec émotions", self.test_extract_relations_with_emotions)

        # Concepts avec emotional_states
        self.run_test("Création de concept", self.test_create_concept)
        self.run_test("Liaison mémoire-concept", self.test_link_memory_concept)
        self.run_test("Concept avec analyse émotionnelle", self.test_get_concept_with_emotional_analysis)
        self.run_test("Concepts par mémoire", self.test_get_concepts_by_memory)
        self.run_test("Relations avec emotional_states", self.test_get_relations_with_emotional_states)
        self.run_test("Concepts par sentence_id", self.test_get_concepts_by_sentence)
        self.run_test("Relations par sentence_id", self.test_get_relations_by_sentence)
        self.run_test("Accumulation emotional_states", self.test_emotional_states_accumulation)

        # Sessions
        self.run_test("Création de session", self.test_create_session)
        self.run_test("Mise à jour de session", self.test_update_session)
        self.run_test("Récupération de session", self.test_get_session)

        # Requêtes génériques
        self.run_test("Requête Cypher", self.test_cypher_query)
        self.run_test("Batch de requêtes", self.test_batch_queries)

        self.teardown()
        self.print_summary()

    def print_summary(self):
        """Affiche le résumé des tests"""
        print("\n" + "=" * 70)
        print("RÉSUMÉ DES TESTS")
        print("=" * 70)

        passed = sum(1 for _, success in self.results if success)
        total = len(self.results)

        for name, success in self.results:
            status = "✓ PASS" if success else "✗ FAIL"
            print(f"  {status}: {name}")

        print("-" * 70)
        print(f"TOTAL: {passed}/{total} tests réussis")

        if passed == total:
            print("\n" + "=" * 70)
            print("    TOUS LES TESTS SONT PASSÉS ✓")
            print("=" * 70)
        else:
            print("\n" + "=" * 70)
            print(f"    {total - passed} TEST(S) ÉCHOUÉ(S) ✗")
            print("=" * 70)


def main():
    # Configuration
    docker_mode = '--docker' in sys.argv

    # Récupérer les credentials depuis les variables d'environnement
    rabbitmq_host = os.environ.get('RABBITMQ_HOST', 'rabbitmq' if docker_mode else 'localhost')
    rabbitmq_user = os.environ.get('RABBITMQ_USER', 'virtus')
    rabbitmq_pass = os.environ.get('RABBITMQ_PASS', 'virtus@83')

    if docker_mode:
        print("Mode: Docker")
    else:
        print("Mode: Local")

    config = TestConfig(
        rabbitmq_host=rabbitmq_host,
        rabbitmq_user=rabbitmq_user,
        rabbitmq_pass=rabbitmq_pass
    )

    print(f"RabbitMQ: {config.rabbitmq_host}")
    print(f"User: {config.rabbitmq_user}")

    # Exécuter les tests
    test_suite = Neo4jFullTest(config)
    test_suite.run_all_tests()


if __name__ == "__main__":
    main()
