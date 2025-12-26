"""
hybrid_search.py
Module de recherche hybride lexicale/sémantique pour MCEE 3.0

Combine :
- Recherche lexicale (spaCy) : correspondance exacte sur les lemmes
- Recherche sémantique (CamemBERT) : similarité vectorielle

La pondération est dynamique selon la qualité des résultats lexicaux.
"""

import json
import logging
import spacy
import torch
import numpy as np
from functools import lru_cache
from typing import List, Dict, Optional, Tuple, Any
from dataclasses import dataclass, field
from enum import Enum
from cachetools import LRUCache
from sklearn.metrics.pairwise import cosine_similarity
from neo4j import GraphDatabase
from neo4j.exceptions import ServiceUnavailable, SessionExpired

from app import EmotionalAnalyzer

# Configuration du logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


class SearchMode(Enum):
    """Mode de recherche utilisé"""
    LEXICAL_PRIORITY = "lexical_priority"      # 80% lexical / 20% sémantique
    SEMANTIC_PRIORITY = "semantic_priority"    # 20% lexical / 80% sémantique
    BALANCED = "balanced"                       # 50% / 50%


@dataclass
class SearchResult:
    """Résultat de recherche avec score et métadonnées émotionnelles"""
    concept_name: str
    lemma: str

    # Données émotionnelles (format MCEE)
    emotional_states: Dict[str, List[float]] = field(default_factory=dict)
    dominant_emotion: str = "Neutre"
    avg_valence: float = 0.0

    # Scores par méthode (normalisés 0-1)
    lexical_score: float = 0.0
    semantic_score: float = 0.0

    # Score final pondéré
    final_score: float = 0.0

    # Métadonnées
    source: str = ""  # "lexical", "semantic", "both"
    memory_ids: List[str] = field(default_factory=list)
    sentence_ids: List[str] = field(default_factory=list)

    def to_dict(self) -> Dict:
        return {
            "concept_name": self.concept_name,
            "lemma": self.lemma,
            "emotional_states": self.emotional_states,
            "dominant_emotion": self.dominant_emotion,
            "avg_valence": self.avg_valence,
            "lexical_score": self.lexical_score,
            "semantic_score": self.semantic_score,
            "final_score": self.final_score,
            "source": self.source,
            "memory_ids": self.memory_ids,
            "sentence_ids": self.sentence_ids
        }


@dataclass
class SearchResponse:
    """Réponse complète de la recherche hybride"""
    results: List[SearchResult]
    mode_used: SearchMode
    lexical_confidence: float
    semantic_coverage: float
    query_tokens: List[str]
    query_lemmas: List[str]
    query_embedding: Optional[np.ndarray] = None

    def to_dict(self) -> Dict:
        return {
            "results": [r.to_dict() for r in self.results],
            "mode_used": self.mode_used.value,
            "lexical_confidence": self.lexical_confidence,
            "semantic_coverage": self.semantic_coverage,
            "query_tokens": self.query_tokens,
            "query_lemmas": self.query_lemmas
        }


class HybridSearchEngine:
    """
    Moteur de recherche hybride combinant approches lexicale et sémantique.

    Utilise spaCy pour la lemmatisation et CamemBERT pour les embeddings
    contextuels, avec pondération dynamique selon la qualité des résultats.
    """

    # Seuils de confiance pour la pondération
    LEXICAL_HIGH_CONFIDENCE = 0.8
    LEXICAL_LOW_CONFIDENCE = 0.3

    # Seuil de similarité sémantique minimum
    SEMANTIC_MIN_SIMILARITY = 0.5

    # Taille max du cache d'embeddings
    EMBEDDING_CACHE_SIZE = 10000

    def __init__(
        self,
        neo4j_uri: str = "bolt://localhost:7687",
        neo4j_user: str = "neo4j",
        neo4j_password: str = "",
        device: str = "cpu",
        spacy_model: str = "fr_core_news_lg",
        camembert_model: str = "camembert-base",
        lazy_load: bool = True
    ):
        """
        Initialise le moteur de recherche.

        Args:
            neo4j_uri: URI de connexion Neo4j
            neo4j_user: Utilisateur Neo4j
            neo4j_password: Mot de passe Neo4j (vide = pas d'auth)
            device: Device pour CamemBERT ("cpu" ou "cuda")
            spacy_model: Modèle spaCy à utiliser
            camembert_model: Modèle CamemBERT à utiliser
            lazy_load: Charger les modèles à la demande
        """
        self.neo4j_uri = neo4j_uri
        self.neo4j_user = neo4j_user
        self.neo4j_password = neo4j_password
        self.device = device
        self.spacy_model = spacy_model
        self.camembert_model = camembert_model

        # Modèles (chargés à la demande si lazy_load=True)
        self._nlp = None
        self._tokenizer = None
        self._camembert = None
        self._driver = None

        # Cache des embeddings avec limite
        self._embedding_cache: LRUCache = LRUCache(maxsize=self.EMBEDDING_CACHE_SIZE)

        # Analyseur émotionnel
        self.analyzer = EmotionalAnalyzer()

        if not lazy_load:
            self._load_all_models()

        logger.info(f"[HybridSearch] Initialisé (lazy_load={lazy_load})")

    def _load_all_models(self):
        """Charge tous les modèles"""
        _ = self.nlp
        _ = self.tokenizer
        _ = self.camembert
        _ = self.driver

    @property
    def nlp(self):
        """Charge spaCy à la demande"""
        if self._nlp is None:
            logger.info(f"[HybridSearch] Chargement spaCy {self.spacy_model}...")
            try:
                self._nlp = spacy.load(self.spacy_model)
            except OSError as e:
                logger.error(f"Modèle spaCy non trouvé: {e}")
                raise RuntimeError(
                    f"Modèle spaCy '{self.spacy_model}' non installé. "
                    f"Exécutez: python -m spacy download {self.spacy_model}"
                )
        return self._nlp

    @property
    def tokenizer(self):
        """Charge le tokenizer CamemBERT à la demande"""
        if self._tokenizer is None:
            logger.info(f"[HybridSearch] Chargement tokenizer {self.camembert_model}...")
            try:
                from transformers import CamembertTokenizer
                self._tokenizer = CamembertTokenizer.from_pretrained(self.camembert_model)
            except Exception as e:
                logger.error(f"Erreur chargement tokenizer: {e}")
                raise
        return self._tokenizer

    @property
    def camembert(self):
        """Charge CamemBERT à la demande"""
        if self._camembert is None:
            logger.info(f"[HybridSearch] Chargement CamemBERT {self.camembert_model}...")
            try:
                from transformers import CamembertModel
                self._camembert = CamembertModel.from_pretrained(self.camembert_model)
                self._camembert.to(self.device)
                self._camembert.eval()
            except Exception as e:
                logger.error(f"Erreur chargement CamemBERT: {e}")
                raise
        return self._camembert

    @property
    def driver(self):
        """Connexion Neo4j à la demande avec retry"""
        if self._driver is None:
            logger.info(f"[HybridSearch] Connexion Neo4j {self.neo4j_uri}...")
            try:
                if self.neo4j_password:
                    self._driver = GraphDatabase.driver(
                        self.neo4j_uri,
                        auth=(self.neo4j_user, self.neo4j_password)
                    )
                else:
                    self._driver = GraphDatabase.driver(self.neo4j_uri)
                # Vérifier la connexion
                with self._driver.session() as session:
                    session.run("RETURN 1")
                logger.info("[HybridSearch] Connexion Neo4j établie")
            except ServiceUnavailable as e:
                logger.error(f"Neo4j indisponible: {e}")
                raise
        return self._driver

    def close(self):
        """Ferme les connexions proprement"""
        if self._driver:
            self._driver.close()
            self._driver = None
        logger.info("[HybridSearch] Connexions fermées")

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    # ─────────────────────────────────────────────────────────────────
    # ANALYSE LINGUISTIQUE (spaCy)
    # ─────────────────────────────────────────────────────────────────

    def analyze_query(self, query: str) -> Dict[str, Any]:
        """
        Analyse linguistique de la requête avec spaCy.

        Args:
            query: Texte de la question

        Returns:
            Dict avec tokens, lemmes, POS tags
        """
        if not query or not query.strip():
            return {
                "original": query,
                "tokens": [],
                "search_lemmas": [],
                "all_lemmas": []
            }

        doc = self.nlp(query)

        # Filtrer les mots significatifs (pas de stopwords, ponctuation)
        significant_tokens = [
            {
                "text": token.text,
                "lemma": token.lemma_.lower(),
                "pos": token.pos_,
                "is_stop": token.is_stop
            }
            for token in doc
            if not token.is_punct and not token.is_space
        ]

        # Extraire uniquement les lemmes significatifs pour la recherche
        search_lemmas = [
            t["lemma"] for t in significant_tokens
            if not t["is_stop"] and t["pos"] in ("NOUN", "VERB", "ADJ", "PROPN", "ADV")
        ]

        return {
            "original": query,
            "tokens": significant_tokens,
            "search_lemmas": search_lemmas,
            "all_lemmas": [t["lemma"] for t in significant_tokens]
        }

    # ─────────────────────────────────────────────────────────────────
    # EMBEDDINGS (CamemBERT)
    # ─────────────────────────────────────────────────────────────────

    def get_embedding(self, text: str, use_cache: bool = True) -> np.ndarray:
        """
        Génère l'embedding CamemBERT pour un texte.

        Args:
            text: Texte à encoder
            use_cache: Utiliser le cache si disponible

        Returns:
            Vecteur numpy de dimension 768
        """
        if not text or not text.strip():
            return np.zeros(768)

        # Vérifier le cache
        cache_key = text.strip().lower()
        if use_cache and cache_key in self._embedding_cache:
            return self._embedding_cache[cache_key]

        try:
            # Tokenisation
            inputs = self.tokenizer(
                text,
                return_tensors="pt",
                padding=True,
                truncation=True,
                max_length=512
            )
            inputs = {k: v.to(self.device) for k, v in inputs.items()}

            # Inférence
            with torch.no_grad():
                outputs = self.camembert(**inputs)

            # Mean pooling sur les tokens (exclure padding)
            attention_mask = inputs["attention_mask"]
            token_embeddings = outputs.last_hidden_state

            input_mask_expanded = attention_mask.unsqueeze(-1).expand(token_embeddings.size()).float()
            sum_embeddings = torch.sum(token_embeddings * input_mask_expanded, 1)
            sum_mask = torch.clamp(input_mask_expanded.sum(1), min=1e-9)
            embedding = (sum_embeddings / sum_mask).squeeze().cpu().numpy()

            # Mettre en cache
            if use_cache:
                self._embedding_cache[cache_key] = embedding

            return embedding

        except Exception as e:
            logger.error(f"Erreur génération embedding: {e}")
            return np.zeros(768)

    def get_embeddings_batch(self, texts: List[str], use_cache: bool = True) -> List[np.ndarray]:
        """
        Génère les embeddings pour plusieurs textes (optimisé).

        Args:
            texts: Liste de textes à encoder
            use_cache: Utiliser le cache

        Returns:
            Liste de vecteurs numpy
        """
        results = []
        texts_to_encode = []
        indices_to_encode = []

        # Vérifier le cache d'abord
        for i, text in enumerate(texts):
            cache_key = text.strip().lower() if text else ""
            if use_cache and cache_key in self._embedding_cache:
                results.append(self._embedding_cache[cache_key])
            else:
                results.append(None)
                if text and text.strip():
                    texts_to_encode.append(text)
                    indices_to_encode.append(i)

        # Encoder les textes non-cachés en batch
        if texts_to_encode:
            try:
                inputs = self.tokenizer(
                    texts_to_encode,
                    return_tensors="pt",
                    padding=True,
                    truncation=True,
                    max_length=512
                )
                inputs = {k: v.to(self.device) for k, v in inputs.items()}

                with torch.no_grad():
                    outputs = self.camembert(**inputs)

                attention_mask = inputs["attention_mask"]
                token_embeddings = outputs.last_hidden_state

                input_mask_expanded = attention_mask.unsqueeze(-1).expand(token_embeddings.size()).float()
                sum_embeddings = torch.sum(token_embeddings * input_mask_expanded, 1)
                sum_mask = torch.clamp(input_mask_expanded.sum(1), min=1e-9)
                embeddings = (sum_embeddings / sum_mask).cpu().numpy()

                for j, idx in enumerate(indices_to_encode):
                    emb = embeddings[j]
                    results[idx] = emb
                    if use_cache:
                        cache_key = texts_to_encode[j].strip().lower()
                        self._embedding_cache[cache_key] = emb

            except Exception as e:
                logger.error(f"Erreur batch embedding: {e}")
                for idx in indices_to_encode:
                    results[idx] = np.zeros(768)

        # Remplacer les None restants
        return [r if r is not None else np.zeros(768) for r in results]

    # ─────────────────────────────────────────────────────────────────
    # RECHERCHE LEXICALE (Neo4j)
    # ─────────────────────────────────────────────────────────────────

    def search_lexical(
        self,
        lemmas: List[str],
        limit: int = 20
    ) -> Tuple[List[SearchResult], float]:
        """
        Recherche lexicale par correspondance exacte des lemmes.

        Args:
            lemmas: Liste des lemmes à rechercher
            limit: Nombre max de résultats

        Returns:
            Tuple (résultats, score de confiance 0-1)
        """
        if not lemmas:
            return [], 0.0

        # Normaliser les lemmes
        lemmas_lower = [l.lower() for l in lemmas]

        query = """
        MATCH (c:Concept)
        WHERE toLower(c.name) IN $lemmas
        OPTIONAL MATCH (c)<-[:EVOQUE]-(m:Memory)
        WITH c, collect(DISTINCT m.id) AS memory_ids
        RETURN c.name AS name,
               c.emotional_states AS emotional_states,
               c.memory_ids AS concept_memory_ids,
               memory_ids AS linked_memory_ids,
               c.trauma_associated AS trauma
        LIMIT $limit
        """

        results = []
        found_lemmas = set()

        try:
            with self.driver.session() as session:
                records = session.run(query, lemmas=lemmas_lower, limit=limit)

                for record in records:
                    name = record["name"]
                    found_lemmas.add(name.lower())

                    # Désérialiser emotional_states
                    es_json = record["emotional_states"] or "{}"
                    try:
                        emotional_states = json.loads(es_json) if isinstance(es_json, str) else es_json
                    except (json.JSONDecodeError, TypeError):
                        emotional_states = {}

                    # Analyser les émotions
                    if emotional_states:
                        # Convertir les clés string en int pour l'analyse
                        es_int_keys = {int(k): v for k, v in emotional_states.items() if v}
                        analysis = self.analyzer.analyze_history(es_int_keys)
                        dominant = analysis.get("dominant_emotion", "Neutre")
                        valence = analysis.get("avg_valence", 0.0)
                    else:
                        dominant = "Neutre"
                        valence = 0.0

                    # Fusionner memory_ids
                    concept_mids = record["concept_memory_ids"] or []
                    linked_mids = record["linked_memory_ids"] or []
                    all_memory_ids = list(set(concept_mids + [m for m in linked_mids if m]))

                    # Score lexical basé sur le match exact
                    lexical_score = 1.0

                    results.append(SearchResult(
                        concept_name=name,
                        lemma=name.lower(),
                        emotional_states=emotional_states,
                        dominant_emotion=dominant,
                        avg_valence=valence,
                        lexical_score=lexical_score,
                        source="lexical",
                        memory_ids=all_memory_ids,
                        sentence_ids=list(emotional_states.keys())
                    ))

            # Calcul de la confiance : ratio de lemmes trouvés
            if lemmas:
                coverage = len(found_lemmas) / len(lemmas)
                # Ajuster par le nombre de résultats
                result_factor = min(1.0, len(results) / max(1, len(lemmas)))
                confidence = coverage * 0.7 + result_factor * 0.3
            else:
                confidence = 0.0

            return results, confidence

        except (ServiceUnavailable, SessionExpired) as e:
            logger.error(f"Erreur Neo4j recherche lexicale: {e}")
            self._driver = None  # Forcer reconnexion
            return [], 0.0

    # ─────────────────────────────────────────────────────────────────
    # RECHERCHE SÉMANTIQUE (CamemBERT + Neo4j)
    # ─────────────────────────────────────────────────────────────────

    def search_semantic(
        self,
        query_embedding: np.ndarray,
        limit: int = 20,
        exclude_concepts: Optional[List[str]] = None
    ) -> Tuple[List[SearchResult], float]:
        """
        Recherche sémantique par similarité d'embeddings.

        Note: Cette version charge tous les concepts avec embeddings.
        Pour de grandes bases, utiliser un index vectoriel Neo4j.

        Args:
            query_embedding: Embedding de la requête
            limit: Nombre max de résultats
            exclude_concepts: Concepts à exclure (déjà trouvés en lexical)

        Returns:
            Tuple (résultats, couverture sémantique moyenne)
        """
        if query_embedding is None or np.allclose(query_embedding, 0):
            return [], 0.0

        exclude_set = set(c.lower() for c in (exclude_concepts or []))

        # Récupérer les concepts avec embeddings
        fetch_query = """
        MATCH (c:Concept)
        WHERE c.embedding IS NOT NULL
        OPTIONAL MATCH (c)<-[:EVOQUE]-(m:Memory)
        WITH c, collect(DISTINCT m.id) AS memory_ids
        RETURN c.name AS name,
               c.embedding AS embedding,
               c.emotional_states AS emotional_states,
               c.memory_ids AS concept_memory_ids,
               memory_ids AS linked_memory_ids
        """

        candidates = []

        try:
            with self.driver.session() as session:
                records = list(session.run(fetch_query))

                for record in records:
                    name = record["name"]

                    # Exclure les concepts déjà trouvés en lexical
                    if name.lower() in exclude_set:
                        continue

                    # Récupérer l'embedding
                    emb = record["embedding"]
                    if emb is None:
                        continue

                    concept_embedding = np.array(emb)

                    # Calculer la similarité cosinus
                    similarity = cosine_similarity(
                        query_embedding.reshape(1, -1),
                        concept_embedding.reshape(1, -1)
                    )[0][0]

                    # Normaliser la similarité de [-1, 1] à [0, 1]
                    normalized_similarity = (similarity + 1) / 2

                    if normalized_similarity >= self.SEMANTIC_MIN_SIMILARITY:
                        candidates.append({
                            "name": name,
                            "similarity": normalized_similarity,
                            "emotional_states": record["emotional_states"],
                            "concept_memory_ids": record["concept_memory_ids"] or [],
                            "linked_memory_ids": record["linked_memory_ids"] or []
                        })

            # Trier par similarité et limiter
            candidates.sort(key=lambda x: x["similarity"], reverse=True)
            candidates = candidates[:limit]

            results = []
            for c in candidates:
                # Désérialiser emotional_states
                es_json = c["emotional_states"] or "{}"
                try:
                    emotional_states = json.loads(es_json) if isinstance(es_json, str) else es_json
                except (json.JSONDecodeError, TypeError):
                    emotional_states = {}

                # Analyser les émotions
                if emotional_states:
                    es_int_keys = {int(k): v for k, v in emotional_states.items() if v}
                    analysis = self.analyzer.analyze_history(es_int_keys)
                    dominant = analysis.get("dominant_emotion", "Neutre")
                    valence = analysis.get("avg_valence", 0.0)
                else:
                    dominant = "Neutre"
                    valence = 0.0

                all_memory_ids = list(set(
                    c["concept_memory_ids"] +
                    [m for m in c["linked_memory_ids"] if m]
                ))

                results.append(SearchResult(
                    concept_name=c["name"],
                    lemma=c["name"].lower(),
                    emotional_states=emotional_states,
                    dominant_emotion=dominant,
                    avg_valence=valence,
                    semantic_score=c["similarity"],
                    source="semantic",
                    memory_ids=all_memory_ids,
                    sentence_ids=list(emotional_states.keys())
                ))

            # Couverture : moyenne des similarités
            coverage = np.mean([r.semantic_score for r in results]) if results else 0.0

            return results, float(coverage)

        except (ServiceUnavailable, SessionExpired) as e:
            logger.error(f"Erreur Neo4j recherche sémantique: {e}")
            self._driver = None
            return [], 0.0

    # ─────────────────────────────────────────────────────────────────
    # FUSION ET PONDÉRATION DYNAMIQUE
    # ─────────────────────────────────────────────────────────────────

    def determine_weights(self, lexical_confidence: float) -> Tuple[float, float, SearchMode]:
        """
        Détermine les poids lexical/sémantique selon la confiance.

        Args:
            lexical_confidence: Score de confiance de la recherche lexicale (0-1)

        Returns:
            Tuple (poids_lexical, poids_sémantique, mode)
        """
        if lexical_confidence >= self.LEXICAL_HIGH_CONFIDENCE:
            # Match lexical fort → privilégier lexical
            return 0.8, 0.2, SearchMode.LEXICAL_PRIORITY

        elif lexical_confidence <= self.LEXICAL_LOW_CONFIDENCE:
            # Match lexical faible → privilégier sémantique
            return 0.2, 0.8, SearchMode.SEMANTIC_PRIORITY

        else:
            # Zone intermédiaire → équilibré
            return 0.5, 0.5, SearchMode.BALANCED

    def merge_results(
        self,
        lexical_results: List[SearchResult],
        semantic_results: List[SearchResult],
        weight_lexical: float,
        weight_semantic: float
    ) -> List[SearchResult]:
        """
        Fusionne les résultats des deux méthodes.

        Args:
            lexical_results: Résultats de la recherche lexicale
            semantic_results: Résultats de la recherche sémantique
            weight_lexical: Poids de la recherche lexicale
            weight_semantic: Poids de la recherche sémantique

        Returns:
            Liste fusionnée et triée par score final
        """
        # Index par nom de concept pour fusion
        merged: Dict[str, SearchResult] = {}

        # Ajouter résultats lexicaux
        for r in lexical_results:
            key = r.concept_name.lower()
            merged[key] = SearchResult(
                concept_name=r.concept_name,
                lemma=r.lemma,
                emotional_states=r.emotional_states,
                dominant_emotion=r.dominant_emotion,
                avg_valence=r.avg_valence,
                lexical_score=r.lexical_score,
                semantic_score=0.0,
                source="lexical",
                memory_ids=r.memory_ids,
                sentence_ids=r.sentence_ids
            )

        # Fusionner résultats sémantiques
        for r in semantic_results:
            key = r.concept_name.lower()
            if key in merged:
                # Existe déjà → fusionner
                merged[key].semantic_score = r.semantic_score
                merged[key].source = "both"
                # Fusionner memory_ids
                existing_mids = set(merged[key].memory_ids)
                merged[key].memory_ids = list(existing_mids | set(r.memory_ids))
            else:
                # Nouveau → ajouter
                merged[key] = SearchResult(
                    concept_name=r.concept_name,
                    lemma=r.lemma,
                    emotional_states=r.emotional_states,
                    dominant_emotion=r.dominant_emotion,
                    avg_valence=r.avg_valence,
                    lexical_score=0.0,
                    semantic_score=r.semantic_score,
                    source="semantic",
                    memory_ids=r.memory_ids,
                    sentence_ids=r.sentence_ids
                )

        # Calculer scores finaux
        for r in merged.values():
            r.final_score = (
                weight_lexical * r.lexical_score +
                weight_semantic * r.semantic_score
            )

        # Trier par score final
        results = list(merged.values())
        results.sort(key=lambda x: x.final_score, reverse=True)

        return results

    # ─────────────────────────────────────────────────────────────────
    # MÉTHODE PRINCIPALE
    # ─────────────────────────────────────────────────────────────────

    def search(self, query: str, limit: int = 10) -> SearchResponse:
        """
        Recherche hybride complète.

        Args:
            query: Question utilisateur
            limit: Nombre max de résultats

        Returns:
            SearchResponse avec résultats et métadonnées
        """
        # 1. Analyse linguistique
        analysis = self.analyze_query(query)
        search_lemmas = analysis["search_lemmas"]
        all_tokens = [t["text"] for t in analysis["tokens"]]

        # 2. Embedding de la requête
        query_embedding = self.get_embedding(query)

        # 3. Recherche lexicale
        lexical_results, lexical_confidence = self.search_lexical(
            search_lemmas,
            limit=limit * 2
        )

        # 4. Recherche sémantique (exclure les concepts trouvés en lexical)
        exclude = [r.concept_name for r in lexical_results]
        semantic_results, semantic_coverage = self.search_semantic(
            query_embedding,
            limit=limit * 2,
            exclude_concepts=exclude
        )

        # 5. Déterminer pondération
        weight_lex, weight_sem, mode = self.determine_weights(lexical_confidence)

        # 6. Fusionner et trier
        merged = self.merge_results(
            lexical_results,
            semantic_results,
            weight_lex,
            weight_sem
        )[:limit]

        return SearchResponse(
            results=merged,
            mode_used=mode,
            lexical_confidence=lexical_confidence,
            semantic_coverage=semantic_coverage,
            query_tokens=all_tokens,
            query_lemmas=search_lemmas,
            query_embedding=query_embedding
        )


# ═════════════════════════════════════════════════════════════════════════════
# UTILITAIRES
# ═════════════════════════════════════════════════════════════════════════════

def format_for_llm(response: SearchResponse) -> Dict:
    """
    Formate les résultats pour envoi à un LLM (ChatGPT, Claude, etc.).

    Args:
        response: Réponse de la recherche hybride

    Returns:
        Dict formaté pour le prompt LLM
    """
    # Extraire émotions dominantes uniques
    emotions = []
    seen = set()
    for r in response.results:
        if r.dominant_emotion and r.dominant_emotion not in seen and r.dominant_emotion != "Neutre":
            emotions.append({
                "emotion": r.dominant_emotion,
                "context_word": r.concept_name,
                "confidence": r.final_score,
                "valence": r.avg_valence
            })
            seen.add(r.dominant_emotion)

    # Top 3 émotions
    top_emotions = sorted(emotions, key=lambda x: x["confidence"], reverse=True)[:3]

    # Mots-clés contextuels
    context_words = [r.concept_name for r in response.results[:5]]

    # Mémoires associées
    all_memory_ids = set()
    for r in response.results:
        all_memory_ids.update(r.memory_ids)

    return {
        "emotions": top_emotions,
        "context_words": context_words,
        "search_mode": response.mode_used.value,
        "confidence": {
            "lexical": response.lexical_confidence,
            "semantic": response.semantic_coverage
        },
        "memory_ids": list(all_memory_ids)[:10],
        "query_lemmas": response.query_lemmas
    }


def generate_llm_prompt(llm_context: Dict, question: str) -> str:
    """
    Génère un prompt enrichi pour le LLM.

    Args:
        llm_context: Contexte formaté par format_for_llm
        question: Question originale de l'utilisateur

    Returns:
        Prompt complet pour le LLM
    """
    emotions = llm_context.get("emotions", [])
    context_words = llm_context.get("context_words", [])

    emotion_str = ""
    if emotions:
        emotion_str = "\n".join([
            f"- {e['emotion']} (confiance: {e['confidence']:.0%}, valence: {e['valence']:+.2f})"
            for e in emotions
        ])
    else:
        emotion_str = "- Aucune émotion particulière détectée"

    return f"""Contexte émotionnel détecté dans la mémoire :
{emotion_str}

Mots-clés contextuels associés : {', '.join(context_words) if context_words else 'Aucun'}

Question de l'utilisateur : {question}

Réponds en tenant compte de l'état émotionnel détecté dans la mémoire.
Adapte ton ton à l'émotion dominante si elle est significative."""


# ═════════════════════════════════════════════════════════════════════════════
# POINT D'ENTRÉE (TEST)
# ═════════════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    import os

    print("=" * 80)
    print("TEST MODULE HYBRID SEARCH")
    print("=" * 80)

    # Configuration
    neo4j_uri = os.getenv("NEO4J_URI", "bolt://localhost:7687")
    neo4j_password = os.getenv("NEO4J_PASSWORD", "")

    try:
        with HybridSearchEngine(
            neo4j_uri=neo4j_uri,
            neo4j_password=neo4j_password,
            lazy_load=True
        ) as engine:

            # Test analyse de requête
            print("\n[TEST 1] Analyse de requête")
            analysis = engine.analyze_query("Comment te sens-tu par rapport au projet ?")
            print(f"  Lemmes trouvés: {analysis['search_lemmas']}")

            # Test embedding
            print("\n[TEST 2] Génération d'embedding")
            emb = engine.get_embedding("test de l'embedding")
            print(f"  Dimension: {emb.shape}")
            print(f"  Norme: {np.linalg.norm(emb):.4f}")

            # Test recherche complète
            print("\n[TEST 3] Recherche hybride")
            response = engine.search("Comment vas-tu aujourd'hui ?", limit=5)
            print(f"  Mode utilisé: {response.mode_used.value}")
            print(f"  Confiance lexicale: {response.lexical_confidence:.2%}")
            print(f"  Couverture sémantique: {response.semantic_coverage:.2%}")
            print(f"  Résultats: {len(response.results)}")

            for i, r in enumerate(response.results[:3]):
                print(f"    [{i+1}] {r.concept_name} - score: {r.final_score:.3f} ({r.source})")
                print(f"        Émotion: {r.dominant_emotion}, Valence: {r.avg_valence:+.2f}")

            # Test formatage LLM
            print("\n[TEST 4] Formatage pour LLM")
            llm_context = format_for_llm(response)
            print(f"  Émotions détectées: {len(llm_context['emotions'])}")
            print(f"  Mots contextuels: {llm_context['context_words']}")

            print("\n" + "=" * 80)
            print("TESTS TERMINÉS AVEC SUCCÈS")
            print("=" * 80)

    except Exception as e:
        print(f"\n[ERREUR] {type(e).__name__}: {e}")
        print("Assurez-vous que Neo4j est démarré et les modèles installés.")
