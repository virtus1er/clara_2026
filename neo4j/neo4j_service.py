# neo4j/neo4j_service.py

import json
import pika
import threading
import logging
from typing import Dict, List, Tuple, Optional, Any
from dataclasses import dataclass, asdict
from datetime import datetime
from enum import Enum
from neo4j import GraphDatabase
from app import RelationExtractor

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class RequestType(Enum):
    """Types de requêtes supportées"""
    # Mémoire
    CREATE_MEMORY = "create_memory"
    MERGE_MEMORY = "merge_memory"
    CREATE_TRAUMA = "create_trauma"
    GET_MEMORY = "get_memory"
    FIND_SIMILAR = "find_similar"
    APPLY_DECAY = "apply_decay"
    REACTIVATE = "reactivate"
    DELETE_MEMORY = "delete_memory"

    # Patterns
    GET_PATTERN = "get_pattern"
    UPDATE_PATTERN = "update_pattern"
    GET_TRANSITIONS = "get_transitions"
    RECORD_TRANSITION = "record_transition"

    # Relations
    EXTRACT_RELATIONS = "extract_relations"
    CREATE_CONCEPT = "create_concept"
    LINK_MEMORY_CONCEPT = "link_memory_concept"

    # Session
    CREATE_SESSION = "create_session"
    UPDATE_SESSION = "update_session"
    GET_SESSION = "get_session"

    # Requêtes génériques
    CYPHER_QUERY = "cypher_query"
    BATCH_QUERY = "batch_query"


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
            RequestType.CREATE_MEMORY.value: self._handle_create_memory,
            RequestType.MERGE_MEMORY.value: self._handle_merge_memory,
            RequestType.CREATE_TRAUMA.value: self._handle_create_trauma,
            RequestType.GET_MEMORY.value: self._handle_get_memory,
            RequestType.FIND_SIMILAR.value: self._handle_find_similar,
            RequestType.APPLY_DECAY.value: self._handle_apply_decay,
            RequestType.REACTIVATE.value: self._handle_reactivate,
            RequestType.DELETE_MEMORY.value: self._handle_delete_memory,
            RequestType.GET_PATTERN.value: self._handle_get_pattern,
            RequestType.UPDATE_PATTERN.value: self._handle_update_pattern,
            RequestType.GET_TRANSITIONS.value: self._handle_get_transitions,
            RequestType.RECORD_TRANSITION.value: self._handle_record_transition,
            RequestType.EXTRACT_RELATIONS.value: self._handle_extract_relations,
            RequestType.CREATE_CONCEPT.value: self._handle_create_concept,
            RequestType.LINK_MEMORY_CONCEPT.value: self._handle_link_memory_concept,
            RequestType.CREATE_SESSION.value: self._handle_create_session,
            RequestType.UPDATE_SESSION.value: self._handle_update_session,
            RequestType.GET_SESSION.value: self._handle_get_session,
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
    # HANDLERS MÉMOIRE
    # ═══════════════════════════════════════════════════════════════════════════

    def _handle_create_memory(self, payload: Dict) -> Dict:
        """Crée un nouveau souvenir"""
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

        # Extraire les relations du contexte
        relations = []
        if context:
            words, rels = self.relation_extractor.extract(context)
            relations = rels
            if not keywords:
                keywords = words

        with self.driver.session() as session:
            # Créer le souvenir (label dynamique via propriété type)
            result = session.run("""
                CREATE (m:Memory {
                    id: $id,
                    type: $type,
                    emotions: $emotions,
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
                                 emotions=emotions,
                                 dominant=dominant,
                                 intensity=intensity,
                                 valence=valence,
                                 weight=weight,
                                 pattern=pattern,
                                 context=context,
                                 keywords=keywords
                                 )

            created_id = result.single()['id']

            # Créer les concepts et relations
            for word in keywords:
                session.run("""
                    MERGE (c:Concept {name: $name})
                    ON CREATE SET c.created_at = datetime()
                    WITH c
                    MATCH (m:Memory {id: $mem_id})
                    MERGE (m)-[:EVOQUE]->(c)
                """, name=word.lower(), mem_id=created_id)

            # Créer les relations sémantiques
            for w1, rel_type, w2 in relations:
                session.run("""
                    MERGE (c1:Concept {name: $w1})
                    MERGE (c2:Concept {name: $w2})
                    MERGE (c1)-[r:SEMANTIQUE {type: $rel_type}]->(c2)
                    ON CREATE SET r.count = 1
                    ON MATCH SET r.count = r.count + 1
                """, w1=w1.lower(), w2=w2.lower(), rel_type=rel_type)

            # Lier au pattern
            session.run("""
                MATCH (m:Memory {id: $mem_id})
                MATCH (p:Pattern {name: $pattern})
                MERGE (p)-[:ACTIVATED_BY]->(m)
            """, mem_id=created_id, pattern=pattern)

        return {
            'id': created_id,
            'keywords_extracted': keywords,
            'relations_created': len(relations)
        }

    def _handle_merge_memory(self, payload: Dict) -> Dict:
        """Fusionne avec un souvenir existant"""
        target_id = payload['target_id']
        emotions = payload['emotions']
        transfer_weight = payload.get('transfer_weight', 0.3)

        with self.driver.session() as session:
            result = session.run("""
                MATCH (m:Memory {id: $target_id})
                SET m.emotions = [i IN range(0, 23) | 
                    (m.emotions[i] * m.weight + $emotions[i] * $transfer) / (m.weight + $transfer)
                ],
                m.weight = CASE WHEN m.weight + $transfer > 1.0 THEN 1.0 
                           ELSE m.weight + $transfer END,
                m.activation_count = m.activation_count + 1,
                m.last_activated = datetime(),
                m.merge_count = COALESCE(m.merge_count, 0) + 1
                RETURN m.id AS id, m.weight AS new_weight, m.merge_count AS merges
            """, target_id=target_id, emotions=emotions, transfer=transfer_weight)

            record = result.single()
            return {
                'id': record['id'],
                'new_weight': record['new_weight'],
                'merge_count': record['merges']
            }

    def _handle_create_trauma(self, payload: Dict) -> Dict:
        """Crée un trauma"""
        trauma_id = payload.get('id', f"TRAUMA_{datetime.now().timestamp()}")
        emotions = payload.get('emotions', [0.0] * 24)
        dominant = payload.get('dominant', 'Peur')
        intensity = payload.get('intensity', 0.9)
        valence = payload.get('valence', 0.1)
        context = payload.get('context', '')
        trigger_keywords = payload.get('trigger_keywords', [])

        # Extraire les relations du contexte
        if context and not trigger_keywords:
            words, _ = self.relation_extractor.extract(context)
            trigger_keywords = words

        with self.driver.session() as session:
            result = session.run("""
                CREATE (t:Memory:Trauma {
                    id: $id,
                    emotions: $emotions,
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
                                 emotions=emotions,
                                 dominant=dominant,
                                 intensity=intensity,
                                 valence=valence,
                                 context=context,
                                 keywords=trigger_keywords
                                 )

            created_id = result.single()['id']

            # Créer les concepts déclencheurs
            for keyword in trigger_keywords:
                session.run("""
                    MERGE (c:Concept {name: $name})
                    ON CREATE SET c.created_at = datetime()
                    WITH c
                    MATCH (t:Trauma {id: $trauma_id})
                    MERGE (t)-[:TRIGGERED_BY {strength: 0.9}]->(c)
                    SET c.trauma_associated = true,
                        c.emotional_valence_personal = -0.5
                """, name=keyword.lower(), trauma_id=created_id)

        return {
            'id': created_id,
            'trigger_keywords': trigger_keywords
        }

    def _handle_get_memory(self, payload: Dict) -> Optional[Dict]:
        """Récupère un souvenir par ID"""
        memory_id = payload['id']

        with self.driver.session() as session:
            result = session.run("""
                MATCH (m:Memory {id: $id})
                OPTIONAL MATCH (m)-[:EVOQUE]->(c:Concept)
                RETURN m, collect(c.name) AS concepts
            """, id=memory_id)

            record = result.single()
            if record:
                m = record['m']
                return {
                    'id': m['id'],
                    'emotions': list(m['emotions']),
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
                WITH m, $emotions AS query_emotions,
                     reduce(dot = 0.0, i IN range(0, 23) | 
                            dot + m.emotions[i] * query_emotions[i]) AS dot_product,
                     sqrt(reduce(a = 0.0, i IN range(0, 23) | a + m.emotions[i]^2)) AS norm_m,
                     sqrt(reduce(b = 0.0, i IN range(0, 23) | b + query_emotions[i]^2)) AS norm_q
                WITH m, 
                     CASE WHEN norm_m * norm_q > 0 
                          THEN dot_product / (norm_m * norm_q) 
                          ELSE 0 END AS similarity
                WHERE similarity >= $threshold
                RETURN m.id AS id, m.dominant AS dominant, m.weight AS weight, 
                       similarity, m.trauma AS trauma
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
                RETURN m.id AS id, m.weight AS new_weight, m.activation_count AS activations
            """, id=memory_id, strength=strength, boost=boost)

            record = result.single()
            if record:
                return dict(record)

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
        """Extrait les relations d'un texte"""
        text = payload['text']
        store = payload.get('store', False)

        words, relations = self.relation_extractor.extract(text)

        if store:
            with self.driver.session() as session:
                for w1, rel_type, w2 in relations:
                    session.run("""
                        MERGE (c1:Concept {name: $w1})
                        MERGE (c2:Concept {name: $w2})
                        MERGE (c1)-[r:SEMANTIQUE {type: $rel_type}]->(c2)
                        ON CREATE SET r.count = 1
                        ON MATCH SET r.count = r.count + 1
                    """, w1=w1.lower(), w2=w2.lower(), rel_type=rel_type)

        return {
            'keywords': words,
            'relations': [(w1, rel, w2) for w1, rel, w2 in relations],
            'stored': store
        }

    def _handle_create_concept(self, payload: Dict) -> Dict:
        """Crée ou met à jour un concept"""
        name = payload['name'].lower()
        attributes = payload.get('attributes', {})

        with self.driver.session() as session:
            result = session.run("""
                MERGE (c:Concept {name: $name})
                ON CREATE SET c.created_at = datetime()
                SET c += $attrs
                RETURN c.name AS name
            """, name=name, attrs=attributes)

            return {'name': result.single()['name']}

    def _handle_link_memory_concept(self, payload: Dict) -> Dict:
        """Lie un souvenir à un concept"""
        memory_id = payload['memory_id']
        concept_name = payload['concept_name'].lower()
        relation_type = payload.get('relation', 'EVOQUE')
        properties = payload.get('properties', {})

        with self.driver.session() as session:
            result = session.run(f"""
                MATCH (m:Memory {{id: $mem_id}})
                MERGE (c:Concept {{name: $concept}})
                MERGE (m)-[r:{relation_type}]->(c)
                SET r += $props
                RETURN m.id AS memory, c.name AS concept
            """, mem_id=memory_id, concept=concept_name, props=properties)

            record = result.single()
            return {
                'memory': record['memory'],
                'concept': record['concept'],
                'relation': relation_type
            }

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