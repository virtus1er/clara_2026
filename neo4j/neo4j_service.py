# neo4j/neo4j_service.py

import json
import pika
import threading
import logging
import numpy as np
from typing import Dict, List, Tuple, Optional, Any
from dataclasses import dataclass, asdict
from datetime import datetime
from enum import Enum
from neo4j import GraphDatabase
from app import RelationExtractor, EmotionalAnalyzer

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class RequestType(Enum):
    """Types de requêtes supportées"""
    # Mémoire de base
    CREATE_MEMORY = "create_memory"
    MERGE_MEMORY = "merge_memory"
    CREATE_TRAUMA = "create_trauma"
    GET_MEMORY = "get_memory"
    FIND_SIMILAR = "find_similar"
    APPLY_DECAY = "apply_decay"
    REACTIVATE = "reactivate"
    DELETE_MEMORY = "delete_memory"

    # Architecture mémoire avancée
    CONSOLIDATE_TO_MLT = "consolidate_to_mlt"  # MCT → MLT
    ACTIVATE_WORKING = "activate_working"      # Activer en MT
    CREATE_PROCEDURAL = "create_procedural"    # Mémoire procédurale
    GET_AUTOBIOGRAPHIC = "get_autobiographic"  # Mémoire autobiographique
    LINK_ASSOCIATIVE = "link_associative"      # Lien associatif (odeurs, etc.)

    # Patterns
    GET_PATTERN = "get_pattern"
    UPDATE_PATTERN = "update_pattern"
    GET_TRANSITIONS = "get_transitions"
    RECORD_TRANSITION = "record_transition"

    # Relations / Concepts
    EXTRACT_RELATIONS = "extract_relations"
    CREATE_CONCEPT = "create_concept"
    LINK_MEMORY_CONCEPT = "link_memory_concept"
    GET_CONCEPT = "get_concept"
    GET_CONCEPTS_BY_MEMORY = "get_concepts_by_memory"
    GET_RELATIONS_WITH_IDS = "get_relations_with_ids"
    CREATE_SEMANTIC_RELATION = "create_semantic_relation"  # is-a, part-of
    GET_CONCEPTS_BY_SENTENCE = "get_concepts_by_sentence"
    GET_RELATIONS_BY_SENTENCE = "get_relations_by_sentence"
    
    # Analyse émotionnelle
    ANALYZE_CONCEPT_EMOTIONS = "analyze_concept_emotions"
    GET_EMOTIONAL_SIGNATURE = "get_emotional_signature"
    GET_TRAUMA_CONCEPTS = "get_trauma_concepts"
    GET_CONCEPT_EMOTIONAL_HISTORY = "get_concept_emotional_history"

    # Session
    CREATE_SESSION = "create_session"
    UPDATE_SESSION = "update_session"
    GET_SESSION = "get_session"

    # Module Dreams (consolidation nocturne)
    GET_MCT_STATS = "get_mct_stats"
    GET_MLT_STATS = "get_mlt_stats"
    CONSOLIDATE_ALL_MCT = "consolidate_all_mct"
    CLEANUP_MCT = "cleanup_mct"
    DREAM_CYCLE = "dream_cycle"

    # Requêtes génériques
    CYPHER_QUERY = "cypher_query"
    BATCH_QUERY = "batch_query"


class MemoryType(Enum):
    """Types de mémoire"""
    MCT = "MCT"              # Mémoire à Court Terme (éphémère)
    MLT = "MLT"              # Mémoire à Long Terme (consolidée)
    WORKING = "Working"       # Mémoire de Travail (activée)
    EPISODIC = "Episodic"     # Épisodique (chapitres de vie)
    SEMANTIC = "Semantic"     # Sémantique (encyclopédie)
    PROCEDURAL = "Procedural" # Procédurale (routines)
    ASSOCIATIVE = "Associative" # Associative (odeurs, sensations)
    AUTOBIOGRAPHIC = "Autobiographic" # Autobiographique (identité IA)


@dataclass
class Neo4jRequest:
    """Structure d'une requête Neo4j"""
    request_id: str
    request_type: str
    payload: Dict
    timestamp: str = None
    correlation_id: str = None

    def __post_init__(self):
        if self.timestamp is None:
            self.timestamp = datetime.now().isoformat()


@dataclass
class Neo4jResponse:
    """Structure d'une réponse Neo4j"""
    request_id: str
    success: bool
    data: Any = None
    error: str = None
    execution_time_ms: float = 0
    timestamp: str = None

    def __post_init__(self):
        if self.timestamp is None:
            self.timestamp = datetime.now().isoformat()


class Neo4jService:
    """Service Neo4j avec communication RabbitMQ"""

    def __init__(self,
                 neo4j_uri: str = "bolt://localhost:7687",
                 neo4j_user: str = "neo4j",
                 neo4j_password: str = "",
                 rabbitmq_host: str = "localhost",
                 rabbitmq_user: str = "guest",
                 rabbitmq_pass: str = "guest",
                 request_queue: str = "neo4j.requests.queue",
                 response_exchange: str = "neo4j.responses"):

        # Neo4j - connect without auth if password is empty (NEO4J_AUTH=none)
        if neo4j_password:
            self.driver = GraphDatabase.driver(neo4j_uri, auth=(neo4j_user, neo4j_password))
        else:
            self.driver = GraphDatabase.driver(neo4j_uri)
        self._verify_connection()

        # RabbitMQ
        self.rabbitmq_host = rabbitmq_host
        self.rabbitmq_user = rabbitmq_user
        self.rabbitmq_pass = rabbitmq_pass
        self.request_queue = request_queue
        self.response_exchange = response_exchange

        # Extracteur de relations
        self.relation_extractor = RelationExtractor()

        # État
        self.running = False
        self.consumer_thread = None

        # Handlers
        self.handlers = {
            # Mémoire de base
            RequestType.CREATE_MEMORY.value: self._handle_create_memory,
            RequestType.MERGE_MEMORY.value: self._handle_merge_memory,
            RequestType.CREATE_TRAUMA.value: self._handle_create_trauma,
            RequestType.GET_MEMORY.value: self._handle_get_memory,
            RequestType.FIND_SIMILAR.value: self._handle_find_similar,
            RequestType.APPLY_DECAY.value: self._handle_apply_decay,
            RequestType.REACTIVATE.value: self._handle_reactivate,
            RequestType.DELETE_MEMORY.value: self._handle_delete_memory,
            # Architecture mémoire avancée
            RequestType.CONSOLIDATE_TO_MLT.value: self._handle_consolidate_to_mlt,
            RequestType.ACTIVATE_WORKING.value: self._handle_activate_working,
            RequestType.CREATE_PROCEDURAL.value: self._handle_create_procedural,
            RequestType.GET_AUTOBIOGRAPHIC.value: self._handle_get_autobiographic,
            RequestType.LINK_ASSOCIATIVE.value: self._handle_link_associative,
            # Patterns
            RequestType.GET_PATTERN.value: self._handle_get_pattern,
            RequestType.UPDATE_PATTERN.value: self._handle_update_pattern,
            RequestType.GET_TRANSITIONS.value: self._handle_get_transitions,
            RequestType.RECORD_TRANSITION.value: self._handle_record_transition,
            # Relations / Concepts
            RequestType.EXTRACT_RELATIONS.value: self._handle_extract_relations,
            RequestType.CREATE_CONCEPT.value: self._handle_create_concept,
            RequestType.LINK_MEMORY_CONCEPT.value: self._handle_link_memory_concept,
            RequestType.GET_CONCEPT.value: self._handle_get_concept,
            RequestType.GET_CONCEPTS_BY_MEMORY.value: self._handle_get_concepts_by_memory,
            RequestType.GET_RELATIONS_WITH_IDS.value: self._handle_get_relations_with_ids,
            RequestType.CREATE_SEMANTIC_RELATION.value: self._handle_create_semantic_relation,
            RequestType.GET_CONCEPTS_BY_SENTENCE.value: self._handle_get_concepts_by_sentence,
            RequestType.GET_RELATIONS_BY_SENTENCE.value: self._handle_get_relations_by_sentence,
            # Sessions
            RequestType.CREATE_SESSION.value: self._handle_create_session,
            RequestType.UPDATE_SESSION.value: self._handle_update_session,
            RequestType.GET_SESSION.value: self._handle_get_session,
            # Module Dreams
            RequestType.GET_MCT_STATS.value: self._handle_get_mct_stats,
            RequestType.GET_MLT_STATS.value: self._handle_get_mlt_stats,
            RequestType.CONSOLIDATE_ALL_MCT.value: self._handle_consolidate_all_mct,
            RequestType.CLEANUP_MCT.value: self._handle_cleanup_mct,
            RequestType.DREAM_CYCLE.value: self._handle_dream_cycle,
            # Requêtes génériques
            RequestType.CYPHER_QUERY.value: self._handle_cypher_query,
            RequestType.BATCH_QUERY.value: self._handle_batch_query,
        }

        logger.info(f"Neo4jService initialisé - Neo4j: {neo4j_uri}, RabbitMQ: {rabbitmq_host}")

    def _verify_connection(self):
        """Vérifie la connexion Neo4j"""
        with self.driver.session() as session:
            result = session.run("RETURN 1 AS test")
            result.single()
        logger.info("Connexion Neo4j vérifiée")

    def start(self):
        """Démarre le service"""
        self.running = True
        self.consumer_thread = threading.Thread(target=self._consume_loop, daemon=True)
        self.consumer_thread.start()
        logger.info("Neo4jService démarré")

    def stop(self):
        """Arrête le service"""
        self.running = False
        if self.consumer_thread:
            self.consumer_thread.join(timeout=5)
        self.driver.close()
        logger.info("Neo4jService arrêté")

    def _consume_loop(self):
        """Boucle de consommation RabbitMQ"""
        credentials = pika.PlainCredentials(self.rabbitmq_user, self.rabbitmq_pass)
        connection = pika.BlockingConnection(
            pika.ConnectionParameters(host=self.rabbitmq_host, credentials=credentials)
        )
        channel = connection.channel()

        # Déclarer la queue de requêtes
        channel.queue_declare(queue=self.request_queue, durable=True)

        # Déclarer l'exchange de réponses
        channel.exchange_declare(
            exchange=self.response_exchange,
            exchange_type='direct',
            durable=True
        )

        def callback(ch, method, properties, body):
            try:
                request_data = json.loads(body.decode())
                request = Neo4jRequest(**request_data)

                start_time = datetime.now()
                response = self._process_request(request)
                response.execution_time_ms = (datetime.now() - start_time).total_seconds() * 1000

                # Envoyer la réponse
                routing_key = properties.reply_to or f"response.{request.request_id}"
                ch.basic_publish(
                    exchange=self.response_exchange,
                    routing_key=routing_key,
                    body=json.dumps(asdict(response)),
                    properties=pika.BasicProperties(
                        correlation_id=properties.correlation_id,
                        content_type='application/json'
                    )
                )

                ch.basic_ack(delivery_tag=method.delivery_tag)

            except Exception as e:
                logger.error(f"Erreur traitement requête: {e}")
                ch.basic_nack(delivery_tag=method.delivery_tag, requeue=False)

        channel.basic_qos(prefetch_count=10)
        channel.basic_consume(queue=self.request_queue, on_message_callback=callback)

        logger.info(f"Écoute sur {self.request_queue}...")

        while self.running:
            connection.process_data_events(time_limit=1)

        connection.close()

    def _process_request(self, request: Neo4jRequest) -> Neo4jResponse:
        """Traite une requête"""
        handler = self.handlers.get(request.request_type)

        if not handler:
            return Neo4jResponse(
                request_id=request.request_id,
                success=False,
                error=f"Type de requête inconnu: {request.request_type}"
            )

        try:
            result = handler(request.payload)
            return Neo4jResponse(
                request_id=request.request_id,
                success=True,
                data=result
            )
        except Exception as e:
            logger.error(f"Erreur handler {request.request_type}: {e}")
            return Neo4jResponse(
                request_id=request.request_id,
                success=False,
                error=str(e)
            )

    # ═══════════════════════════════════════════════════════════════════════════
    # HELPERS POUR SENTENCE_IDS
    # ═══════════════════════════════════════════════════════════════════════════

    def _merge_sentence_ids(self, existing_ids: List[int], new_ids: List[int]) -> List[int]:
        """Fusionne deux listes d'IDs de phrases sans doublons"""
        return sorted(list(set(existing_ids or []) | set(new_ids or [])))

    # ═══════════════════════════════════════════════════════════════════════════
    # HANDLERS MÉMOIRE
    # ═══════════════════════════════════════════════════════════════════════════

    def _handle_create_memory(self, payload: Dict) -> Dict:
        """Crée un nouveau souvenir avec support emotional_states {sentence_id: [24 emotions]}"""
        memory_id = payload.get('id', f"MEM_{datetime.now().timestamp()}")
        emotions = payload.get('emotions', [0.0] * 24)
        dominant = payload.get('dominant', 'Neutre')
        intensity = payload.get('intensity', 0.0)
        valence = payload.get('valence', 0.5)
        weight = payload.get('weight', 0.5)
        pattern = payload.get('pattern', 'INCONNU')
        context = payload.get('context', '')
        keywords = payload.get('keywords', [])
        memory_type = payload.get('type', 'Episodic')
        sentence_id = payload.get('sentence_id')
        
        # Support du nouveau format emotional_states
        emotional_states = payload.get('emotional_states', {})
        if sentence_id and not emotional_states:
            # Rétrocompatibilité: créer emotional_states à partir de sentence_id + emotions
            emotional_states = {str(sentence_id): emotions}
        
        # Convertir les clés en string pour Neo4j (les maps Neo4j ont des clés string)
        emotional_states_str = {str(k): v for k, v in emotional_states.items()}

        # Extraire les relations du contexte avec émotions
        relations = []
        words_with_emotions = []
        if context:
            mots, rels = self.relation_extractor.extract(context, sentence_id=sentence_id, emotions=emotions)
            words_with_emotions = mots
            relations = rels
            if not keywords:
                keywords = [m['word'] for m in mots]

        with self.driver.session() as session:
            # Créer le souvenir avec emotional_states
            result = session.run("""
                CREATE (m:Memory {
                    id: $id,
                    type: $type,
                    emotional_states: $emotional_states,
                    dominant: $dominant,
                    intensity: $intensity,
                    valence: $valence,
                    weight: $weight,
                    pattern_at_creation: $pattern,
                    context: $context,
                    keywords: $keywords,
                    created_at: datetime(),
                    last_activated: datetime(),
                    activation_count: 1
                })
                RETURN m.id AS id
            """,
                                 type=memory_type,
                                 id=memory_id,
                                 emotional_states=emotional_states_str,
                                 dominant=dominant,
                                 intensity=intensity,
                                 valence=valence,
                                 weight=weight,
                                 pattern=pattern,
                                 context=context,
                                 keywords=keywords
                                 )

            created_id = result.single()['id']

            # Créer les concepts avec emotional_states
            for word_info in words_with_emotions:
                word = word_info['word'] if isinstance(word_info, dict) else word_info
                word_emotional_states = word_info.get('emotional_states', {str(sentence_id): emotions} if sentence_id else {}) if isinstance(word_info, dict) else ({str(sentence_id): emotions} if sentence_id else {})
                word_emotional_states_str = {str(k): v for k, v in word_emotional_states.items()}
                
                session.run("""
                    MERGE (c:Concept {name: $name})
                    ON CREATE SET 
                        c.created_at = datetime(), 
                        c.memory_ids = [$mem_id],
                        c.emotional_states = $emotional_states
                    ON MATCH SET 
                        c.memory_ids = CASE
                            WHEN $mem_id IN c.memory_ids THEN c.memory_ids
                            ELSE c.memory_ids + $mem_id
                        END,
                        c.emotional_states = c.emotional_states + $emotional_states
                    WITH c
                    MATCH (m:Memory {id: $mem_id})
                    MERGE (m)-[:EVOQUE]->(c)
                """, name=word.lower(), mem_id=created_id, emotional_states=word_emotional_states_str)

            # Créer les relations sémantiques avec emotional_states
            for rel_info in relations:
                w1 = rel_info['source'] if isinstance(rel_info, dict) else rel_info[0]
                rel_type = rel_info['relation'] if isinstance(rel_info, dict) else rel_info[1]
                w2 = rel_info['target'] if isinstance(rel_info, dict) else rel_info[2]
                rel_emotional_states = rel_info.get('emotional_states', {str(sentence_id): emotions} if sentence_id else {}) if isinstance(rel_info, dict) else ({str(sentence_id): emotions} if sentence_id else {})
                rel_emotional_states_str = {str(k): v for k, v in rel_emotional_states.items()}
                
                session.run("""
                    MERGE (c1:Concept {name: $w1})
                    ON CREATE SET 
                        c1.created_at = datetime(), 
                        c1.memory_ids = [$mem_id],
                        c1.emotional_states = $emotional_states
                    ON MATCH SET 
                        c1.memory_ids = CASE
                            WHEN $mem_id IN c1.memory_ids THEN c1.memory_ids
                            ELSE c1.memory_ids + $mem_id
                        END,
                        c1.emotional_states = c1.emotional_states + $emotional_states
                    MERGE (c2:Concept {name: $w2})
                    ON CREATE SET 
                        c2.created_at = datetime(), 
                        c2.memory_ids = [$mem_id],
                        c2.emotional_states = $emotional_states
                    ON MATCH SET 
                        c2.memory_ids = CASE
                            WHEN $mem_id IN c2.memory_ids THEN c2.memory_ids
                            ELSE c2.memory_ids + $mem_id
                        END,
                        c2.emotional_states = c2.emotional_states + $emotional_states
                    MERGE (c1)-[r:SEMANTIQUE {type: $rel_type}]->(c2)
                    ON CREATE SET 
                        r.count = 1, 
                        r.memory_ids = [$mem_id],
                        r.emotional_states = $emotional_states
                    ON MATCH SET 
                        r.count = r.count + 1,
                        r.memory_ids = CASE
                            WHEN $mem_id IN r.memory_ids THEN r.memory_ids
                            ELSE r.memory_ids + $mem_id
                        END,
                        r.emotional_states = r.emotional_states + $emotional_states
                """, w1=w1.lower(), w2=w2.lower(), rel_type=rel_type, mem_id=created_id, emotional_states=rel_emotional_states_str)

            # Lier au pattern
            session.run("""
                MATCH (m:Memory {id: $mem_id})
                MATCH (p:Pattern {name: $pattern})
                MERGE (p)-[:ACTIVATED_BY]->(m)
            """, mem_id=created_id, pattern=pattern)

        return {
            'id': created_id,
            'keywords_extracted': keywords,
            'relations_created': len(relations),
            'sentence_id': sentence_id,
            'emotional_states': emotional_states_str
        }

    def _handle_merge_memory(self, payload: Dict) -> Dict:
        """Fusionne avec un souvenir existant en ajoutant les emotional_states"""
        target_id = payload['target_id']
        emotions = payload['emotions']
        transfer_weight = payload.get('transfer_weight', 0.3)
        sentence_id = payload.get('sentence_id')
        new_emotional_states = payload.get('emotional_states', {})
        
        # Rétrocompatibilité
        if sentence_id and not new_emotional_states:
            new_emotional_states = {str(sentence_id): emotions}
        
        new_emotional_states_str = {str(k): v for k, v in new_emotional_states.items()}

        with self.driver.session() as session:
            result = session.run("""
                MATCH (m:Memory {id: $target_id})
                SET m.weight = CASE WHEN m.weight + $transfer > 1.0 THEN 1.0 
                           ELSE m.weight + $transfer END,
                m.activation_count = m.activation_count + 1,
                m.last_activated = datetime(),
                m.merge_count = COALESCE(m.merge_count, 0) + 1,
                m.emotional_states = COALESCE(m.emotional_states, {}) + $new_states
                RETURN m.id AS id, m.weight AS new_weight, m.merge_count AS merges, 
                       m.emotional_states AS emotional_states
            """, target_id=target_id, transfer=transfer_weight, new_states=new_emotional_states_str)

            record = result.single()
            emotional_states = record['emotional_states'] if record else {}
            return {
                'id': record['id'],
                'new_weight': record['new_weight'],
                'merge_count': record['merges'],
                'emotional_states': dict(emotional_states) if emotional_states else {},
                'sentence_ids': list(emotional_states.keys()) if emotional_states else []
            }

    def _handle_create_trauma(self, payload: Dict) -> Dict:
        """Crée un trauma avec emotional_states"""
        trauma_id = payload.get('id', f"TRAUMA_{datetime.now().timestamp()}")
        emotions = payload.get('emotions', [0.0] * 24)
        dominant = payload.get('dominant', 'Peur')
        intensity = payload.get('intensity', 0.9)
        valence = payload.get('valence', 0.1)
        context = payload.get('context', '')
        trigger_keywords = payload.get('trigger_keywords', [])
        sentence_id = payload.get('sentence_id')
        
        # Support emotional_states
        emotional_states = payload.get('emotional_states', {})
        if sentence_id and not emotional_states:
            emotional_states = {str(sentence_id): emotions}
        emotional_states_str = {str(k): v for k, v in emotional_states.items()}

        # Extraire les relations du contexte
        if context and not trigger_keywords:
            mots, _ = self.relation_extractor.extract(context, sentence_id=sentence_id, emotions=emotions)
            trigger_keywords = [m['word'] if isinstance(m, dict) else m for m in mots]

        with self.driver.session() as session:
            result = session.run("""
                CREATE (t:Memory:Trauma {
                    id: $id,
                    emotional_states: $emotional_states,
                    dominant: $dominant,
                    intensity: $intensity,
                    valence: $valence,
                    weight: 0.95,
                    trauma: true,
                    reinforced: true,
                    forget_rate: 0.001,
                    context: $context,
                    trigger_keywords: $keywords,
                    avoidance_behaviors: [],
                    coping_strategies: [],
                    therapy_progress: 0.0,
                    created_at: datetime(),
                    last_activated: datetime(),
                    activation_count: 1
                })
                RETURN t.id AS id
            """,
                                 id=trauma_id,
                                 emotional_states=emotional_states_str,
                                 dominant=dominant,
                                 intensity=intensity,
                                 valence=valence,
                                 context=context,
                                 keywords=trigger_keywords
                                 )

            created_id = result.single()['id']

            # Créer les concepts déclencheurs avec emotional_states
            for keyword in trigger_keywords:
                session.run("""
                    MERGE (c:Concept {name: $name})
                    ON CREATE SET 
                        c.created_at = datetime(), 
                        c.memory_ids = [$trauma_id],
                        c.emotional_states = $emotional_states
                    ON MATCH SET 
                        c.memory_ids = CASE
                            WHEN $trauma_id IN c.memory_ids THEN c.memory_ids
                            ELSE c.memory_ids + $trauma_id
                        END,
                        c.emotional_states = c.emotional_states + $emotional_states
                    WITH c
                    MATCH (t:Trauma {id: $trauma_id})
                    MERGE (t)-[:TRIGGERED_BY {strength: 0.9}]->(c)
                    SET c.trauma_associated = true,
                        c.emotional_valence_personal = -0.5
                """, name=keyword.lower(), trauma_id=created_id, emotional_states=emotional_states_str)

        return {
            'id': created_id,
            'trigger_keywords': trigger_keywords,
            'sentence_id': sentence_id,
            'emotional_states': emotional_states_str
        }

    def _handle_get_memory(self, payload: Dict) -> Optional[Dict]:
        """Récupère un souvenir par ID avec emotional_states"""
        memory_id = payload['id']

        with self.driver.session() as session:
            result = session.run("""
                MATCH (m:Memory {id: $id})
                OPTIONAL MATCH (m)-[:EVOQUE]->(c:Concept)
                RETURN m, collect({name: c.name, emotional_states: c.emotional_states}) AS concepts
            """, id=memory_id)

            record = result.single()
            if record:
                m = record['m']
                emotional_states = m.get('emotional_states', {})
                
                # Calculer les stats à partir de emotional_states
                sentence_ids = list(emotional_states.keys()) if emotional_states else []
                
                return {
                    'id': m['id'],
                    'emotional_states': dict(emotional_states) if emotional_states else {},
                    'sentence_ids': sentence_ids,
                    'dominant': m['dominant'],
                    'intensity': m['intensity'],
                    'valence': m['valence'],
                    'weight': m['weight'],
                    'pattern': m.get('pattern_at_creation'),
                    'context': m.get('context'),
                    'keywords': m.get('keywords', []),
                    'concepts': record['concepts'],
                    'activation_count': m.get('activation_count', 0),
                    'trauma': m.get('trauma', False)
                }

        return None

    def _handle_find_similar(self, payload: Dict) -> List[Dict]:
        """Trouve les souvenirs similaires par signature émotionnelle"""
        emotions = payload['emotions']
        threshold = payload.get('threshold', 0.85)
        limit = payload.get('limit', 5)

        with self.driver.session() as session:
            # Calcul de similarité cosinus en Cypher
            result = session.run("""
                MATCH (m:Memory)
                WHERE m.emotions IS NOT NULL
                WITH m, $emotions AS qe
                WITH m, qe,
                     reduce(dot = 0.0, i IN range(0, 23) | 
                            dot + m.emotions[i] * qe[i]) AS dot_product,
                     sqrt(reduce(a = 0.0, i IN range(0, 23) | a + m.emotions[i]^2)) AS norm_m,
                     sqrt(reduce(b = 0.0, i IN range(0, 23) | b + qe[i]^2)) AS norm_q
                WITH m, 
                     CASE WHEN norm_m * norm_q > 0 
                          THEN dot_product / (norm_m * norm_q) 
                          ELSE 0 END AS similarity
                WHERE similarity >= $threshold
                RETURN m.id AS id, m.dominant AS dominant, m.weight AS weight, 
                       similarity, m.trauma AS trauma, m.sentence_ids AS sentence_ids
                ORDER BY similarity DESC
                LIMIT $limit
            """, emotions=emotions, threshold=threshold, limit=limit)

            return [dict(r) for r in result]

    def _handle_apply_decay(self, payload: Dict) -> Dict:
        """Applique le decay à tous les souvenirs"""
        elapsed_hours = payload.get('elapsed_hours', 1.0)
        elapsed_days = elapsed_hours / 24.0
        base_decay = payload.get('base_decay_rate', 0.01)
        trauma_decay = payload.get('trauma_decay_rate', 0.001)

        with self.driver.session() as session:
            # Decay normal
            result1 = session.run("""
                MATCH (m:Memory)
                WHERE m.trauma IS NULL OR m.trauma = false
                WITH m, $elapsed_days AS days,
                     COALESCE(m.activation_count, 1) AS activations
                SET m.weight = m.weight * exp(-$decay * days / (1 + 0.1 * activations))
                RETURN count(m) AS updated
            """, elapsed_days=elapsed_days, decay=base_decay)
            normal_updated = result1.single()['updated']

            # Decay trauma (avec plancher)
            result2 = session.run("""
                MATCH (t:Memory:Trauma)
                WITH t, $elapsed_days AS days, 0.3 AS floor_weight
                SET t.weight = floor_weight + (t.weight - floor_weight) * exp(-$decay * days)
                RETURN count(t) AS updated
            """, elapsed_days=elapsed_days, decay=trauma_decay)
            trauma_updated = result2.single()['updated']

            # Archiver les souvenirs très faibles
            result3 = session.run("""
                MATCH (m:Memory)
                WHERE m.weight < 0.05 
                  AND (m.trauma IS NULL OR m.trauma = false)
                  AND m.created_at < datetime() - duration('P30D')
                WITH m LIMIT 100
                CREATE (a:ArchivedMemory)
                SET a = properties(m), a.archived_at = datetime()
                DETACH DELETE m
                RETURN count(a) AS archived
            """)
            archived = result3.single()['archived']

        return {
            'normal_updated': normal_updated,
            'trauma_updated': trauma_updated,
            'archived': archived,
            'elapsed_days': elapsed_days
        }

    def _handle_reactivate(self, payload: Dict) -> Dict:
        """Réactive un souvenir"""
        memory_id = payload['id']
        strength = payload.get('strength', 1.0)
        boost = payload.get('boost_factor', 0.1)

        with self.driver.session() as session:
            result = session.run("""
                MATCH (m:Memory {id: $id})
                WITH m, $strength AS strength, $boost AS boost
                SET m.weight = CASE 
                    WHEN m.weight + boost * strength * (1 - m.weight) > 1.0 THEN 1.0
                    ELSE m.weight + boost * strength * (1 - m.weight)
                END,
                m.activation_count = COALESCE(m.activation_count, 0) + 1,
                m.last_activated = datetime()
                RETURN m.id AS id, m.weight AS new_weight, m.activation_count AS activations, 
                       m.emotional_states AS emotional_states
            """, id=memory_id, strength=strength, boost=boost)

            record = result.single()
            if record:
                es = record['emotional_states'] or {}
                return {
                    'id': record['id'],
                    'new_weight': record['new_weight'],
                    'activations': record['activations'],
                    'sentence_ids': list(es.keys()),
                    'emotional_states': dict(es)
                }

        return {'error': 'Memory not found'}

    def _handle_delete_memory(self, payload: Dict) -> Dict:
        """Supprime un souvenir"""
        memory_id = payload['id']
        archive = payload.get('archive', True)

        with self.driver.session() as session:
            if archive:
                session.run("""
                    MATCH (m:Memory {id: $id})
                    CREATE (a:ArchivedMemory)
                    SET a = properties(m), a.archived_at = datetime()
                    DETACH DELETE m
                """, id=memory_id)
            else:
                session.run("MATCH (m:Memory {id: $id}) DETACH DELETE m", id=memory_id)

        return {'deleted': memory_id, 'archived': archive}

    # ═══════════════════════════════════════════════════════════════════════════
    # HANDLERS ARCHITECTURE MÉMOIRE AVANCÉE
    # ═══════════════════════════════════════════════════════════════════════════

    def _handle_consolidate_to_mlt(self, payload: Dict) -> Dict:
        """Consolide une mémoire MCT vers MLT (mémoire à long terme)"""
        memory_id = payload['id']
        importance = payload.get('importance', 0.7)  # Seuil d'importance

        with self.driver.session() as session:
            # Vérifier si la mémoire est éligible à la consolidation
            result = session.run("""
                MATCH (m:Memory {id: $id})
                WHERE m.type = 'MCT' OR NOT EXISTS(m.consolidated)
                WITH m,
                     CASE
                         WHEN m.trauma = true THEN 1.0
                         WHEN m.intensity >= $importance THEN m.intensity
                         WHEN m.activation_count >= 3 THEN 0.8
                         ELSE m.weight * m.intensity
                     END AS consolidation_score
                WHERE consolidation_score >= $importance
                SET m.type = 'MLT',
                    m.consolidated = true,
                    m.consolidated_at = datetime(),
                    m.consolidation_score = consolidation_score
                RETURN m.id AS id, m.type AS type, consolidation_score, m.sentence_ids AS sentence_ids
            """, id=memory_id, importance=importance)

            record = result.single()
            if record:
                return {
                    'consolidated': True,
                    'id': record['id'],
                    'type': record['type'],
                    'score': record['consolidation_score'],
                    'sentence_ids': record['sentence_ids']
                }

        return {'consolidated': False, 'reason': 'Memory not eligible or not found'}

    def _handle_activate_working(self, payload: Dict) -> Dict:
        """Active une mémoire dans la mémoire de travail (MT)"""
        memory_id = payload['id']
        task_context = payload.get('task_context', '')

        with self.driver.session() as session:
            result = session.run("""
                MATCH (m:Memory {id: $id})
                SET m.working_active = true,
                    m.working_activated_at = datetime(),
                    m.working_task_context = $task_context,
                    m.working_access_count = COALESCE(m.working_access_count, 0) + 1,
                    m.activation_count = COALESCE(m.activation_count, 0) + 1,
                    m.last_activated = datetime()
                RETURN m.id AS id, m.working_access_count AS access_count, m.sentence_ids AS sentence_ids
            """, id=memory_id, task_context=task_context)

            record = result.single()
            if record:
                return {
                    'activated': True,
                    'id': record['id'],
                    'access_count': record['access_count'],
                    'sentence_ids': record['sentence_ids']
                }

        return {'activated': False, 'error': 'Memory not found'}

    def _handle_create_procedural(self, payload: Dict) -> Dict:
        """Crée une mémoire procédurale (routine)"""
        proc_id = payload.get('id', f"PROC_{datetime.now().timestamp()}")
        name = payload['name']
        steps = payload.get('steps', [])
        trigger = payload.get('trigger', '')
        frequency = payload.get('frequency', 0)  # Nombre de fois exécutée

        with self.driver.session() as session:
            result = session.run("""
                CREATE (p:Memory:Procedural {
                    id: $id,
                    type: 'Procedural',
                    name: $name,
                    steps: $steps,
                    trigger: $trigger,
                    frequency: $frequency,
                    automaticity: CASE WHEN $frequency > 10 THEN 0.9
                                       WHEN $frequency > 5 THEN 0.7
                                       ELSE 0.3 END,
                    created_at: datetime(),
                    last_executed: datetime()
                })
                RETURN p.id AS id, p.name AS name, p.automaticity AS automaticity
            """, id=proc_id, name=name, steps=steps, trigger=trigger, frequency=frequency)

            record = result.single()
            return {
                'id': record['id'],
                'name': record['name'],
                'automaticity': record['automaticity']
            }

    def _handle_get_autobiographic(self, payload: Dict) -> Dict:
        """Récupère ou crée le nœud de mémoire autobiographique (identité IA)"""
        ia_id = payload.get('ia_id', 'CLARA')

        with self.driver.session() as session:
            # Créer ou récupérer le nœud autobiographique
            result = session.run("""
                MERGE (a:Memory:Autobiographic {ia_id: $ia_id})
                ON CREATE SET
                    a.id = 'AUTOBIO_' + $ia_id,
                    a.type = 'Autobiographic',
                    a.created_at = datetime(),
                    a.personality_traits = [],
                    a.core_values = [],
                    a.significant_events = [],
                    a.relationships = []
                WITH a
                OPTIONAL MATCH (a)-[:REMEMBERS]->(e:Memory:Episodic)
                OPTIONAL MATCH (a)-[:KNOWS]->(s:Memory:Semantic)
                RETURN a,
                       count(DISTINCT e) AS episodic_count,
                       count(DISTINCT s) AS semantic_count
            """, ia_id=ia_id)

            record = result.single()
            if record:
                auto = record['a']
                return {
                    'id': auto['id'],
                    'ia_id': auto['ia_id'],
                    'personality_traits': auto.get('personality_traits', []),
                    'core_values': auto.get('core_values', []),
                    'episodic_memories': record['episodic_count'],
                    'semantic_knowledge': record['semantic_count']
                }

        return {'error': 'Could not create autobiographic memory'}

    def _handle_link_associative(self, payload: Dict) -> Dict:
        """Crée un lien associatif entre mémoires (odeurs, sensations, etc.)"""
        source_id = payload['source_id']
        target_id = payload['target_id']
        association_type = payload.get('type', 'SENSORY')  # SENSORY, EMOTIONAL, CONTEXTUAL
        trigger = payload.get('trigger', '')  # Ex: "odeur de café"
        strength = payload.get('strength', 0.5)

        with self.driver.session() as session:
            result = session.run("""
                MATCH (s:Memory {id: $source_id})
                MATCH (t:Memory {id: $target_id})
                MERGE (s)-[r:ASSOCIE {type: $assoc_type}]->(t)
                ON CREATE SET
                    r.created_at = datetime(),
                    r.trigger = $trigger,
                    r.strength = $strength,
                    r.activation_count = 1
                ON MATCH SET
                    r.strength = CASE WHEN r.strength + 0.1 > 1.0 THEN 1.0
                                      ELSE r.strength + 0.1 END,
                    r.activation_count = r.activation_count + 1
                RETURN s.id AS source, t.id AS target,
                       r.type AS type, r.strength AS strength
            """, source_id=source_id, target_id=target_id,
                 assoc_type=association_type, trigger=trigger, strength=strength)

            record = result.single()
            if record:
                return dict(record)

        return {'error': 'Could not create associative link'}

    def _handle_create_semantic_relation(self, payload: Dict) -> Dict:
        """Crée une relation sémantique (is-a, part-of, has-property, etc.)"""
        subject = payload['subject'].lower()
        relation = payload['relation']  # IS_A, PART_OF, HAS_PROPERTY, LOCATED_IN, etc.
        object_name = payload['object'].lower()
        properties = payload.get('properties', {})
        sentence_ids = payload.get('sentence_ids', [])

        with self.driver.session() as session:
            result = session.run(f"""
                MERGE (s:Concept {{name: $subject}})
                ON CREATE SET s.created_at = datetime(), s.memory_ids = [], s.sentence_ids = $sentence_ids
                ON MATCH SET s.sentence_ids = [x IN s.sentence_ids WHERE NOT x IN $sentence_ids] + $sentence_ids
                MERGE (o:Concept {{name: $object}})
                ON CREATE SET o.created_at = datetime(), o.memory_ids = [], o.sentence_ids = $sentence_ids
                ON MATCH SET o.sentence_ids = [x IN o.sentence_ids WHERE NOT x IN $sentence_ids] + $sentence_ids
                MERGE (s)-[r:{relation}]->(o)
                ON CREATE SET r.created_at = datetime(), r.count = 1, r.sentence_ids = $sentence_ids
                ON MATCH SET r.count = r.count + 1, r.sentence_ids = [x IN r.sentence_ids WHERE NOT x IN $sentence_ids] + $sentence_ids
                SET r += $props
                RETURN s.name AS subject, type(r) AS relation, o.name AS object, r.count AS count, r.sentence_ids AS sentence_ids
            """, subject=subject, object=object_name, props=properties, sentence_ids=sentence_ids)

            record = result.single()
            if record:
                return dict(record)

        return {'error': 'Could not create semantic relation'}

    # ═══════════════════════════════════════════════════════════════════════════
    # HANDLERS PATTERNS
    # ═══════════════════════════════════════════════════════════════════════════

    def _handle_get_pattern(self, payload: Dict) -> Optional[Dict]:
        """Récupère un pattern par nom"""
        pattern_name = payload['name']

        with self.driver.session() as session:
            result = session.run("""
                MATCH (p:Pattern {name: $name})
                RETURN p
            """, name=pattern_name)

            record = result.single()
            if record:
                p = record['p']
                return dict(p)

        return None

    def _handle_update_pattern(self, payload: Dict) -> Dict:
        """Met à jour un pattern"""
        pattern_name = payload['name']
        updates = payload.get('updates', {})

        # Construire la requête dynamiquement
        set_clauses = []
        params = {'name': pattern_name}

        for key, value in updates.items():
            set_clauses.append(f"p.{key} = ${key}")
            params[key] = value

        if not set_clauses:
            return {'error': 'No updates provided'}

        query = f"""
            MATCH (p:Pattern {{name: $name}})
            SET {', '.join(set_clauses)}
            RETURN p.name AS name
        """

        with self.driver.session() as session:
            result = session.run(query, **params)
            record = result.single()
            if record:
                return {'updated': record['name']}

        return {'error': 'Pattern not found'}

    def _handle_get_transitions(self, payload: Dict) -> List[Dict]:
        """Récupère les transitions depuis un pattern"""
        from_pattern = payload['from']
        limit = payload.get('limit', 10)

        with self.driver.session() as session:
            result = session.run("""
                MATCH (p1:Pattern {name: $from})-[t:TRANSITION_TO]->(p2:Pattern)
                RETURN p2.name AS to_pattern, t.probability AS probability,
                       t.avg_duration_s AS avg_duration, t.count AS count
                ORDER BY t.probability DESC
                LIMIT $limit
            """, **{'from': from_pattern}, limit=limit)

            return [dict(r) for r in result]

    def _handle_record_transition(self, payload: Dict) -> Dict:
        """Enregistre une transition entre patterns"""
        from_pattern = payload['from']
        to_pattern = payload['to']
        duration_s = payload.get('duration_s', 0)
        trigger = payload.get('trigger', '')

        with self.driver.session() as session:
            result = session.run("""
                MATCH (p1:Pattern {name: $from})
                MATCH (p2:Pattern {name: $to})
                MERGE (p1)-[t:TRANSITION_TO]->(p2)
                ON CREATE SET 
                    t.count = 1,
                    t.probability = 0.1,
                    t.avg_duration_s = $duration,
                    t.triggers = [$trigger]
                ON MATCH SET 
                    t.count = t.count + 1,
                    t.avg_duration_s = (t.avg_duration_s * (t.count - 1) + $duration) / t.count,
                    t.triggers = CASE 
                        WHEN $trigger IN t.triggers THEN t.triggers
                        ELSE t.triggers + $trigger
                    END
                WITH p1, t
                // Recalculer les probabilités
                MATCH (p1)-[all_t:TRANSITION_TO]->()
                WITH t, sum(all_t.count) AS total
                SET t.probability = toFloat(t.count) / total
                RETURN t.count AS count, t.probability AS probability
            """, **{'from': from_pattern, 'to': to_pattern}, duration=duration_s, trigger=trigger)

            record = result.single()
            return {
                'from': from_pattern,
                'to': to_pattern,
                'count': record['count'],
                'probability': record['probability']
            }

    # ═══════════════════════════════════════════════════════════════════════════
    # HANDLERS RELATIONS
    # ═══════════════════════════════════════════════════════════════════════════

    def _handle_extract_relations(self, payload: Dict) -> Dict:
        """Extrait les relations d'un texte avec emotional_states"""
        text = payload['text']
        store = payload.get('store', False)
        sentence_id = payload.get('sentence_id')
        emotions = payload.get('emotions', [0.0] * 24)
        
        # Support emotional_states
        emotional_states = payload.get('emotional_states', {})
        if sentence_id and not emotional_states:
            emotional_states = {str(sentence_id): emotions}
        emotional_states_str = {str(k): v for k, v in emotional_states.items()}

        mots, relations = self.relation_extractor.extract(text, sentence_id=sentence_id, emotions=emotions)
        
        # Convertir pour le retour
        words = [m['word'] if isinstance(m, dict) else m for m in mots]

        if store:
            with self.driver.session() as session:
                for rel_info in relations:
                    w1 = rel_info['source'] if isinstance(rel_info, dict) else rel_info[0]
                    rel_type = rel_info['relation'] if isinstance(rel_info, dict) else rel_info[1]
                    w2 = rel_info['target'] if isinstance(rel_info, dict) else rel_info[2]
                    rel_emotional_states = rel_info.get('emotional_states', emotional_states_str) if isinstance(rel_info, dict) else emotional_states_str
                    
                    session.run("""
                        MERGE (c1:Concept {name: $w1})
                        ON CREATE SET c1.emotional_states = $emotional_states
                        ON MATCH SET c1.emotional_states = c1.emotional_states + $emotional_states
                        MERGE (c2:Concept {name: $w2})
                        ON CREATE SET c2.emotional_states = $emotional_states
                        ON MATCH SET c2.emotional_states = c2.emotional_states + $emotional_states
                        MERGE (c1)-[r:SEMANTIQUE {type: $rel_type}]->(c2)
                        ON CREATE SET r.count = 1, r.emotional_states = $emotional_states
                        ON MATCH SET r.count = r.count + 1, r.emotional_states = r.emotional_states + $emotional_states
                    """, w1=w1.lower(), w2=w2.lower(), rel_type=rel_type, emotional_states=rel_emotional_states)

        return {
            'keywords': words,
            'relations': relations,
            'stored': store,
            'sentence_id': sentence_id,
            'emotional_states': emotional_states_str
        }

    def _handle_create_concept(self, payload: Dict) -> Dict:
        """Crée ou met à jour un concept avec emotional_states"""
        name = payload['name'].lower()
        attributes = payload.get('attributes', {})
        sentence_id = payload.get('sentence_id')
        emotions = payload.get('emotions', [0.0] * 24)
        
        # Support emotional_states
        emotional_states = payload.get('emotional_states', {})
        if sentence_id and not emotional_states:
            emotional_states = {str(sentence_id): emotions}
        emotional_states_str = {str(k): v for k, v in emotional_states.items()}

        with self.driver.session() as session:
            result = session.run("""
                MERGE (c:Concept {name: $name})
                ON CREATE SET c.created_at = datetime(), c.emotional_states = $emotional_states
                ON MATCH SET c.emotional_states = c.emotional_states + $emotional_states
                SET c += $attrs
                RETURN c.name AS name, c.emotional_states AS emotional_states
            """, name=name, attrs=attributes, emotional_states=emotional_states_str)

            record = result.single()
            es = record['emotional_states'] or {}
            return {
                'name': record['name'], 
                'emotional_states': dict(es),
                'sentence_ids': list(es.keys())
            }

    def _handle_link_memory_concept(self, payload: Dict) -> Dict:
        """Lie un souvenir à un concept avec propagation des emotional_states"""
        memory_id = payload['memory_id']
        concept_name = payload['concept_name'].lower()
        relation_type = payload.get('relation', 'EVOQUE')
        properties = payload.get('properties', {})

        with self.driver.session() as session:
            result = session.run(f"""
                MATCH (m:Memory {{id: $mem_id}})
                MERGE (c:Concept {{name: $concept}})
                ON CREATE SET c.emotional_states = COALESCE(m.emotional_states, {{}})
                ON MATCH SET c.emotional_states = c.emotional_states + COALESCE(m.emotional_states, {{}})
                MERGE (m)-[r:{relation_type}]->(c)
                SET r += $props
                RETURN m.id AS memory, c.name AS concept, 
                       m.emotional_states AS memory_emotional_states, 
                       c.emotional_states AS concept_emotional_states
            """, mem_id=memory_id, concept=concept_name, props=properties)

            record = result.single()
            mem_es = record['memory_emotional_states'] or {}
            con_es = record['concept_emotional_states'] or {}
            return {
                'memory': record['memory'],
                'concept': record['concept'],
                'relation': relation_type,
                'memory_sentence_ids': list(mem_es.keys()),
                'concept_sentence_ids': list(con_es.keys()),
                'memory_emotional_states': dict(mem_es),
                'concept_emotional_states': dict(con_es)
            }

    def _handle_get_concept(self, payload: Dict) -> Optional[Dict]:
        """Récupère un concept avec ses emotional_states"""
        concept_name = payload['name'].lower()

        with self.driver.session() as session:
            result = session.run("""
                MATCH (c:Concept {name: $name})
                OPTIONAL MATCH (c)<-[:EVOQUE]-(m:Memory)
                RETURN c, collect(m.id) AS linked_memories
            """, name=concept_name)

            record = result.single()
            if record and record['c']:
                c = record['c']
                emotional_states = c.get('emotional_states', {})
                
                # Analyser l'historique émotionnel
                analysis = self._analyze_emotional_history(emotional_states)
                
                return {
                    'name': c['name'],
                    'memory_ids': c.get('memory_ids', []),
                    'emotional_states': dict(emotional_states) if emotional_states else {},
                    'sentence_ids': list(emotional_states.keys()) if emotional_states else [],
                    'linked_memories': [m for m in record['linked_memories'] if m],
                    'trauma_associated': c.get('trauma_associated', False),
                    'created_at': str(c.get('created_at', '')),
                    # Nouvelles métriques d'analyse émotionnelle
                    'emotional_analysis': analysis
                }
        return None
    
    def _analyze_emotional_history(self, emotional_states: Dict) -> Dict:
        """Analyse l'historique émotionnel d'un concept"""
        if not emotional_states:
            return {
                'dominant_emotion': 'Neutre',
                'avg_valence': 0.0,
                'stability': 1.0,
                'trajectory': 'stable',
                'trauma_score': 0.0,
                'emotion_count': 0
            }
        
        EMOTION_NAMES = [
            'joie', 'confiance', 'peur', 'surprise', 'tristesse', 
            'dégoût', 'colère', 'anticipation', 'sérénité', 'intérêt',
            'acceptation', 'appréhension', 'distraction', 'ennui', 'contrariété',
            'pensivité', 'extase', 'admiration', 'terreur', 'étonnement',
            'chagrin', 'aversion', 'rage', 'vigilance'
        ]
        
        positive_indices = [0, 1, 8, 9, 10, 16, 17]
        negative_indices = [2, 4, 5, 6, 11, 13, 20, 21, 22]
        
        emotions_list = list(emotional_states.values())
        emotions_array = np.array(emotions_list)
        avg_emotions = np.mean(emotions_array, axis=0).tolist()
        variance = float(np.mean(np.var(emotions_array, axis=0)))
        
        # Dominant
        if all(e == 0 for e in avg_emotions):
            dominant = 'Neutre'
        else:
            max_idx = avg_emotions.index(max(avg_emotions))
            dominant = EMOTION_NAMES[max_idx].capitalize() if max_idx < len(EMOTION_NAMES) else 'Inconnu'
        
        # Valence
        positive = sum(avg_emotions[i] for i in positive_indices if i < len(avg_emotions))
        negative = sum(avg_emotions[i] for i in negative_indices if i < len(avg_emotions))
        total = positive + negative
        valence = (positive - negative) / total if total > 0 else 0.0
        
        # Trajectoire
        if len(emotions_list) >= 3:
            valences = []
            for e in emotions_list:
                pos = sum(e[i] for i in positive_indices if i < len(e))
                neg = sum(e[i] for i in negative_indices if i < len(e))
                t = pos + neg
                valences.append((pos - neg) / t if t > 0 else 0.0)
            trend = np.polyfit(range(len(valences)), valences, 1)[0]
            if trend > 0.1:
                trajectory = 'ascending'
            elif trend < -0.1:
                trajectory = 'descending'
            elif variance > 0.3:
                trajectory = 'volatile'
            else:
                trajectory = 'stable'
        else:
            trajectory = 'stable'
        
        # Trauma score
        trauma_emotions = [[e[i] for i in negative_indices if i < len(e)] for e in emotions_list]
        trauma_score = float(np.mean([max(te) if te else 0 for te in trauma_emotions]))
        
        return {
            'dominant_emotion': dominant,
            'avg_valence': valence,
            'stability': max(0.0, 1.0 - variance * 2),
            'trajectory': trajectory,
            'trauma_score': trauma_score,
            'emotion_count': len(emotional_states)
        }

    def _handle_get_concepts_by_memory(self, payload: Dict) -> List[Dict]:
        """Récupère tous les concepts associés à une mémoire avec emotional_states"""
        memory_id = payload['memory_id']

        with self.driver.session() as session:
            result = session.run("""
                MATCH (m:Memory {id: $mem_id})-[:EVOQUE]->(c:Concept)
                RETURN c.name AS name, c.memory_ids AS memory_ids, 
                       c.emotional_states AS emotional_states,
                       c.trauma_associated AS trauma_associated
            """, mem_id=memory_id)

            concepts = []
            for record in result:
                emotional_states = record['emotional_states'] or {}
                analysis = self._analyze_emotional_history(emotional_states)
                concepts.append({
                    'name': record['name'],
                    'memory_ids': record['memory_ids'] or [],
                    'emotional_states': dict(emotional_states),
                    'sentence_ids': list(emotional_states.keys()),
                    'trauma_associated': record['trauma_associated'],
                    'emotional_analysis': analysis
                })
            return concepts

    def _handle_get_relations_with_ids(self, payload: Dict) -> List[Dict]:
        """Récupère les relations sémantiques avec leurs emotional_states"""
        memory_id = payload.get('memory_id')
        concept_name = payload.get('concept_name')
        sentence_id = payload.get('sentence_id')
        limit = payload.get('limit', 50)

        with self.driver.session() as session:
            if sentence_id:
                # Relations pour un sentence_id spécifique
                result = session.run("""
                    MATCH (c1:Concept)-[r:SEMANTIQUE]->(c2:Concept)
                    WHERE $sent_id IN keys(r.emotional_states)
                    RETURN c1.name AS source, c1.memory_ids AS source_memory_ids, 
                           c1.emotional_states AS source_emotional_states,
                           r.type AS relation, r.memory_ids AS relation_memory_ids, 
                           r.emotional_states AS relation_emotional_states,
                           c2.name AS target, c2.memory_ids AS target_memory_ids, 
                           c2.emotional_states AS target_emotional_states
                    LIMIT $limit
                """, sent_id=str(sentence_id), limit=limit)
            elif memory_id:
                # Relations pour une mémoire spécifique
                result = session.run("""
                    MATCH (c1:Concept)-[r:SEMANTIQUE]->(c2:Concept)
                    WHERE $mem_id IN r.memory_ids
                    RETURN c1.name AS source, c1.memory_ids AS source_memory_ids, 
                           c1.emotional_states AS source_emotional_states,
                           r.type AS relation, r.memory_ids AS relation_memory_ids, 
                           r.emotional_states AS relation_emotional_states,
                           c2.name AS target, c2.memory_ids AS target_memory_ids, 
                           c2.emotional_states AS target_emotional_states
                    LIMIT $limit
                """, mem_id=memory_id, limit=limit)
            elif concept_name:
                # Relations pour un concept
                result = session.run("""
                    MATCH (c1:Concept {name: $name})-[r:SEMANTIQUE]->(c2:Concept)
                    RETURN c1.name AS source, c1.memory_ids AS source_memory_ids, 
                           c1.emotional_states AS source_emotional_states,
                           r.type AS relation, r.memory_ids AS relation_memory_ids, 
                           r.emotional_states AS relation_emotional_states,
                           c2.name AS target, c2.memory_ids AS target_memory_ids, 
                           c2.emotional_states AS target_emotional_states
                    UNION
                    MATCH (c1:Concept)-[r:SEMANTIQUE]->(c2:Concept {name: $name})
                    RETURN c1.name AS source, c1.memory_ids AS source_memory_ids, 
                           c1.emotional_states AS source_emotional_states,
                           r.type AS relation, r.memory_ids AS relation_memory_ids, 
                           r.emotional_states AS relation_emotional_states,
                           c2.name AS target, c2.memory_ids AS target_memory_ids, 
                           c2.emotional_states AS target_emotional_states
                    LIMIT $limit
                """, name=concept_name.lower(), limit=limit)
            else:
                # Toutes les relations
                result = session.run("""
                    MATCH (c1:Concept)-[r:SEMANTIQUE]->(c2:Concept)
                    RETURN c1.name AS source, c1.memory_ids AS source_memory_ids, 
                           c1.emotional_states AS source_emotional_states,
                           r.type AS relation, r.memory_ids AS relation_memory_ids, 
                           r.emotional_states AS relation_emotional_states,
                           c2.name AS target, c2.memory_ids AS target_memory_ids, 
                           c2.emotional_states AS target_emotional_states
                    LIMIT $limit
                """, limit=limit)

            relations = []
            for r in result:
                src_es = r['source_emotional_states'] or {}
                rel_es = r['relation_emotional_states'] or {}
                tgt_es = r['target_emotional_states'] or {}
                relations.append({
                    'source': r['source'],
                    'source_memory_ids': r['source_memory_ids'] or [],
                    'source_sentence_ids': list(src_es.keys()),
                    'source_emotional_states': dict(src_es),
                    'relation': r['relation'],
                    'relation_memory_ids': r['relation_memory_ids'] or [],
                    'relation_sentence_ids': list(rel_es.keys()),
                    'relation_emotional_states': dict(rel_es),
                    'target': r['target'],
                    'target_memory_ids': r['target_memory_ids'] or [],
                    'target_sentence_ids': list(tgt_es.keys()),
                    'target_emotional_states': dict(tgt_es)
                })
            return relations

    def _handle_get_concepts_by_sentence(self, payload: Dict) -> List[Dict]:
        """Récupère tous les concepts d'une phrase spécifique avec leurs émotions"""
        sentence_id = payload['sentence_id']

        with self.driver.session() as session:
            result = session.run("""
                MATCH (c:Concept)
                WHERE $sent_id IN keys(c.emotional_states)
                RETURN c.name AS name, c.memory_ids AS memory_ids, 
                       c.emotional_states AS emotional_states,
                       c.trauma_associated AS trauma_associated
                ORDER BY c.name
            """, sent_id=str(sentence_id))

            concepts = []
            for r in result:
                emotional_states = r['emotional_states'] or {}
                # Extraire les émotions de ce sentence_id spécifique
                this_sentence_emotions = emotional_states.get(str(sentence_id), [])
                concepts.append({
                    'name': r['name'],
                    'memory_ids': r['memory_ids'] or [],
                    'sentence_ids': list(emotional_states.keys()),
                    'emotional_states': dict(emotional_states),
                    'emotions_for_sentence': this_sentence_emotions,
                    'trauma_associated': r['trauma_associated']
                })
            return concepts

    def _handle_get_relations_by_sentence(self, payload: Dict) -> List[Dict]:
        """Récupère toutes les relations d'une phrase spécifique avec leurs émotions"""
        sentence_id = payload['sentence_id']

        with self.driver.session() as session:
            result = session.run("""
                MATCH (c1:Concept)-[r:SEMANTIQUE]->(c2:Concept)
                WHERE $sent_id IN keys(r.emotional_states)
                RETURN c1.name AS source, c1.emotional_states AS source_emotional_states,
                       r.type AS relation, r.emotional_states AS relation_emotional_states,
                       c2.name AS target, c2.emotional_states AS target_emotional_states
            """, sent_id=str(sentence_id))

            relations = []
            for r in result:
                src_es = r['source_emotional_states'] or {}
                rel_es = r['relation_emotional_states'] or {}
                tgt_es = r['target_emotional_states'] or {}
                
                relations.append({
                    'source': r['source'],
                    'source_sentence_ids': list(src_es.keys()),
                    'source_emotions_for_sentence': src_es.get(str(sentence_id), []),
                    'relation': r['relation'],
                    'relation_sentence_ids': list(rel_es.keys()),
                    'relation_emotions_for_sentence': rel_es.get(str(sentence_id), []),
                    'target': r['target'],
                    'target_sentence_ids': list(tgt_es.keys()),
                    'target_emotions_for_sentence': tgt_es.get(str(sentence_id), [])
                })
            return relations

    # ═══════════════════════════════════════════════════════════════════════════
    # HANDLERS SESSION
    # ═══════════════════════════════════════════════════════════════════════════

    def _handle_create_session(self, payload: Dict) -> Dict:
        """Crée une nouvelle session MCT"""
        session_id = payload.get('id', f"SESSION_{datetime.now().timestamp()}")
        pattern = payload.get('pattern', 'INCONNU')

        with self.driver.session() as neo_session:
            result = neo_session.run("""
                CREATE (s:Session:MCT {
                    id: $id,
                    current_pattern: $pattern,
                    stability: 0.0,
                    volatility: 0.0,
                    trend: 'stable',
                    created_at: datetime(),
                    updated_at: datetime(),
                    state_count: 0
                })
                RETURN s.id AS id
            """, id=session_id, pattern=pattern)

            return {'id': result.single()['id']}

    def _handle_update_session(self, payload: Dict) -> Dict:
        """Met à jour une session"""
        session_id = payload['id']
        updates = payload.get('updates', {})

        # Ajouter un état émotionnel si fourni
        if 'emotional_state' in payload:
            state = payload['emotional_state']
            with self.driver.session() as neo_session:
                neo_session.run("""
                    MATCH (s:Session {id: $session_id})
                    CREATE (e:EmotionalState {
                        timestamp: datetime(),
                        emotions: $emotions,
                        dominant: $dominant,
                        valence: $valence,
                        intensity: $intensity
                    })
                    CREATE (s)-[:CONTAINS]->(e)
                    SET s.state_count = s.state_count + 1,
                        s.updated_at = datetime()
                """, session_id=session_id, **state)

        if updates:
            set_clauses = ', '.join([f"s.{k} = ${k}" for k in updates.keys()])
            with self.driver.session() as neo_session:
                neo_session.run(f"""
                    MATCH (s:Session {{id: $session_id}})
                    SET {set_clauses}, s.updated_at = datetime()
                """, session_id=session_id, **updates)

        return {'updated': session_id}

    def _handle_get_session(self, payload: Dict) -> Optional[Dict]:
        """Récupère une session avec ses états récents"""
        session_id = payload['id']
        state_limit = payload.get('state_limit', 10)

        with self.driver.session() as neo_session:
            result = neo_session.run("""
                MATCH (s:Session {id: $id})
                OPTIONAL MATCH (s)-[:CONTAINS]->(e:EmotionalState)
                WITH s, e ORDER BY e.timestamp DESC LIMIT $limit
                RETURN s, collect(e) AS states
            """, id=session_id, limit=state_limit)

            record = result.single()
            if record and record['s'] is not None:
                s = record['s']
                # Filtrer les états None (du OPTIONAL MATCH)
                states = [dict(e) for e in record['states'] if e is not None]
                return {
                    **dict(s),
                    'recent_states': states
                }

        return None

    # ═══════════════════════════════════════════════════════════════════════════
    # HANDLERS MODULE DREAMS (Consolidation nocturne)
    # ═══════════════════════════════════════════════════════════════════════════

    def _handle_get_mct_stats(self, payload: Dict) -> Dict:
        """Statistiques de la mémoire à court terme"""
        with self.driver.session() as session:
            result = session.run("""
                MATCH (m:Memory)
                WHERE m.type = 'MCT' OR (m.type IS NULL AND NOT EXISTS(m.consolidated))
                WITH m
                RETURN 
                    'MCT' AS type,
                    count(m) AS total_count,
                    avg(m.weight) AS avg_weight,
                    avg(m.intensity) AS avg_intensity,
                    sum(CASE WHEN m.trauma = true THEN 1 ELSE 0 END) AS trauma_count,
                    sum(CASE WHEN m.working_active = true THEN 1 ELSE 0 END) AS working_active_count
            """)
            
            record = result.single()
            if record:
                return {
                    'type': record['type'],
                    'total_count': record['total_count'],
                    'avg_weight': record['avg_weight'] or 0,
                    'avg_intensity': record['avg_intensity'] or 0,
                    'trauma_count': record['trauma_count'],
                    'working_active_count': record['working_active_count']
                }
        
        return {'type': 'MCT', 'total_count': 0}

    def _handle_get_mlt_stats(self, payload: Dict) -> Dict:
        """Statistiques de la mémoire à long terme"""
        with self.driver.session() as session:
            result = session.run("""
                MATCH (m:Memory)
                WHERE m.type = 'MLT' OR m.consolidated = true
                WITH m
                RETURN 
                    'MLT' AS type,
                    count(m) AS total_count,
                    avg(m.consolidation_score) AS avg_consolidation_score,
                    avg(m.weight) AS avg_weight
            """)
            
            record = result.single()
            
            # Récupérer les stats par catégorie
            cat_result = session.run("""
                MATCH (m:Memory)
                WHERE m.type = 'MLT' OR m.consolidated = true
                RETURN m.dominant AS category, count(m) AS count
                ORDER BY count DESC
            """)
            
            by_category = {r['category']: r['count'] for r in cat_result if r['category']}
            
            if record:
                return {
                    'type': record['type'],
                    'total_count': record['total_count'],
                    'avg_consolidation_score': record['avg_consolidation_score'],
                    'avg_weight': record['avg_weight'] or 0,
                    'by_category': by_category
                }
        
        return {'type': 'MLT', 'total_count': 0, 'by_category': {}}

    def _handle_consolidate_all_mct(self, payload: Dict) -> Dict:
        """Consolide toutes les MCT éligibles vers MLT"""
        importance_threshold = payload.get('importance_threshold', 0.6)
        
        with self.driver.session() as session:
            result = session.run("""
                MATCH (m:Memory)
                WHERE (m.type = 'MCT' OR NOT EXISTS(m.consolidated))
                  AND m.consolidated IS NULL OR m.consolidated = false
                WITH m,
                     CASE
                         WHEN m.trauma = true THEN 1.0
                         WHEN m.intensity >= $threshold THEN m.intensity
                         WHEN COALESCE(m.activation_count, 0) >= 3 THEN 0.8
                         ELSE COALESCE(m.weight, 0.5) * COALESCE(m.intensity, 0.5)
                     END AS score
                WHERE score >= $threshold
                SET m.type = 'MLT',
                    m.consolidated = true,
                    m.consolidated_at = datetime(),
                    m.consolidation_score = score
                RETURN m.id AS id, score
            """, threshold=importance_threshold)
            
            consolidated = [{'id': r['id'], 'score': r['score']} for r in result]
            
            return {
                'consolidated_count': len(consolidated),
                'consolidated_memories': consolidated
            }

    def _handle_cleanup_mct(self, payload: Dict) -> Dict:
        """Nettoie les MCT expirées ou faibles"""
        max_age_hours = payload.get('max_age_hours', 24)
        min_weight = payload.get('min_weight', 0.1)
        archive_threshold = payload.get('archive_threshold', 0.05)
        
        with self.driver.session() as session:
            # Archiver les mémoires très faibles
            archive_result = session.run("""
                MATCH (m:Memory)
                WHERE (m.type = 'MCT' OR NOT EXISTS(m.type))
                  AND m.weight < $archive_threshold
                  AND (m.trauma IS NULL OR m.trauma = false)
                WITH m LIMIT 50
                CREATE (a:ArchivedMemory)
                SET a = properties(m), a.archived_at = datetime()
                DETACH DELETE m
                RETURN count(a) AS archived
            """, archive_threshold=archive_threshold)
            
            archived = archive_result.single()['archived']
            
            # Supprimer les mémoires anciennes et faibles
            delete_result = session.run("""
                MATCH (m:Memory)
                WHERE (m.type = 'MCT' OR NOT EXISTS(m.type))
                  AND m.weight < $min_weight
                  AND m.created_at < datetime() - duration({hours: $max_age})
                  AND (m.trauma IS NULL OR m.trauma = false)
                WITH m LIMIT 50
                DETACH DELETE m
                RETURN count(m) AS deleted
            """, min_weight=min_weight, max_age=max_age_hours)
            
            deleted = delete_result.single()['deleted']
            
            # Désactiver les mémoires de travail anciennes
            deactivate_result = session.run("""
                MATCH (m:Memory)
                WHERE m.working_active = true
                  AND m.working_activated_at < datetime() - duration({hours: 2})
                SET m.working_active = false
                RETURN count(m) AS deactivated
            """)
            
            deactivated = deactivate_result.single()['deactivated']
            
            return {
                'archived': archived,
                'deleted': deleted,
                'working_deactivated': deactivated
            }

    def _handle_dream_cycle(self, payload: Dict) -> Dict:
        """Exécute un cycle de rêve complet (consolidation + nettoyage)"""
        importance_threshold = payload.get('importance_threshold', 0.6)
        max_mct_age_hours = payload.get('max_mct_age_hours', 24)
        min_weight_to_keep = payload.get('min_weight_to_keep', 0.1)
        
        # 1. Consolidation
        consolidation_result = self._handle_consolidate_all_mct({
            'importance_threshold': importance_threshold
        })
        
        # 2. Nettoyage
        cleanup_result = self._handle_cleanup_mct({
            'max_age_hours': max_mct_age_hours,
            'min_weight': min_weight_to_keep,
            'archive_threshold': 0.05
        })
        
        # 3. Renforcer les liens MLT
        with self.driver.session() as session:
            reinforce_result = session.run("""
                MATCH (m1:Memory)-[r:ASSOCIE]->(m2:Memory)
                WHERE m1.type = 'MLT' AND m2.type = 'MLT'
                SET r.strength = CASE 
                    WHEN r.strength + 0.05 > 1.0 THEN 1.0 
                    ELSE r.strength + 0.05 
                END
                RETURN count(r) AS reinforced
            """)
            
            reinforced = reinforce_result.single()['reinforced']
        
        return {
            'dream_cycle_completed': True,
            'consolidation': consolidation_result,
            'cleanup': cleanup_result,
            'reinforced_mlt_links': reinforced
        }

    # ═══════════════════════════════════════════════════════════════════════════
    # HANDLERS GÉNÉRIQUES
    # ═══════════════════════════════════════════════════════════════════════════

    def _handle_cypher_query(self, payload: Dict) -> List[Dict]:
        """Exécute une requête Cypher arbitraire"""
        query = payload['query']
        params = payload.get('params', {})

        with self.driver.session() as session:
            result = session.run(query, **params)
            return [dict(r) for r in result]

    def _handle_batch_query(self, payload: Dict) -> List[Dict]:
        """Exécute un batch de requêtes"""
        queries = payload['queries']  # Liste de {query, params}
        results = []

        with self.driver.session() as session:
            for q in queries:
                result = session.run(q['query'], **q.get('params', {}))
                results.append([dict(r) for r in result])

        return results


# ═══════════════════════════════════════════════════════════════════════════
# POINT D'ENTRÉE
# ═══════════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    import os

    service = Neo4jService(
        neo4j_uri=os.getenv('NEO4J_URI', 'bolt://neo4j:7687'),
        neo4j_user=os.getenv('NEO4J_USER', 'neo4j'),
        neo4j_password=os.getenv('NEO4J_PASSWORD', ''),  # Empty = no auth (NEO4J_AUTH=none)
        rabbitmq_host=os.getenv('RABBITMQ_HOST', 'rabbitmq'),
        rabbitmq_user=os.getenv('RABBITMQ_USER', 'guest'),
        rabbitmq_pass=os.getenv('RABBITMQ_PASS', 'guest')
    )

    try:
        service.start()

        # Maintenir le service actif
        import signal


        def shutdown(sig, frame):
            logger.info("Arrêt demandé...")
            service.stop()


        signal.signal(signal.SIGINT, shutdown)
        signal.signal(signal.SIGTERM, shutdown)

        # Boucle principale
        while service.running:
            import time

            time.sleep(1)

    except Exception as e:
        logger.error(f"Erreur fatale: {e}")
        service.stop()
