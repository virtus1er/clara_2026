"""
test_hybrid_search.py
Tests unitaires pour le module de recherche hybride.

Utilise des mocks pour éviter les dépendances externes (Neo4j, CamemBERT).

Usage:
    pytest test_hybrid_search.py -v
    pytest test_hybrid_search.py -v -k "test_analyze"
"""

import pytest
import numpy as np
from unittest.mock import Mock, MagicMock, patch
from dataclasses import dataclass

# Import du module à tester
from hybrid_search import (
    HybridSearchEngine,
    SearchMode,
    SearchResult,
    SearchResponse,
    format_for_llm,
    generate_llm_prompt
)


# ═══════════════════════════════════════════════════════════════════════════════
# FIXTURES
# ═══════════════════════════════════════════════════════════════════════════════

@pytest.fixture
def mock_spacy():
    """Mock du modèle spaCy"""
    with patch('hybrid_search.spacy.load') as mock_load:
        mock_nlp = MagicMock()

        # Simuler le traitement d'un document
        def process_text(text):
            mock_doc = MagicMock()

            # Simuler des tokens
            words = text.lower().split()
            mock_tokens = []

            for i, word in enumerate(words):
                token = MagicMock()
                token.text = word
                token.lemma_ = word.rstrip('s')  # Lemmatisation simple
                token.pos_ = "NOUN" if i % 2 == 0 else "VERB"
                token.is_stop = word in ['le', 'la', 'les', 'de', 'du', 'un', 'une', 'et', 'ou', 'je', 'tu', 'il']
                token.is_punct = word in ['.', ',', '!', '?']
                token.is_space = False
                mock_tokens.append(token)

            mock_doc.__iter__ = lambda self: iter(mock_tokens)
            return mock_doc

        mock_nlp.side_effect = process_text
        mock_nlp.return_value = process_text("test")
        mock_load.return_value = mock_nlp

        yield mock_load


@pytest.fixture
def mock_camembert():
    """Mock du modèle CamemBERT"""
    with patch('hybrid_search.CamembertTokenizer') as mock_tokenizer_cls, \
         patch('hybrid_search.CamembertModel') as mock_model_cls:

        # Mock tokenizer
        mock_tokenizer = MagicMock()
        mock_tokenizer.return_value = {
            'input_ids': MagicMock(),
            'attention_mask': MagicMock()
        }
        mock_tokenizer_cls.from_pretrained.return_value = mock_tokenizer

        # Mock model
        mock_model = MagicMock()
        mock_output = MagicMock()
        mock_output.last_hidden_state = MagicMock()
        mock_model.return_value = mock_output
        mock_model_cls.from_pretrained.return_value = mock_model

        yield mock_tokenizer_cls, mock_model_cls


@pytest.fixture
def mock_neo4j():
    """Mock du driver Neo4j"""
    with patch('hybrid_search.GraphDatabase.driver') as mock_driver_cls:
        mock_driver = MagicMock()
        mock_session = MagicMock()

        mock_driver.session.return_value.__enter__ = lambda self: mock_session
        mock_driver.session.return_value.__exit__ = MagicMock(return_value=False)

        mock_driver_cls.return_value = mock_driver

        yield mock_driver_cls, mock_driver, mock_session


@pytest.fixture
def engine_with_mocks(mock_spacy, mock_neo4j):
    """Engine avec tous les mocks configurés"""
    mock_driver_cls, mock_driver, mock_session = mock_neo4j

    # Configurer le mock session pour retourner des résultats vides par défaut
    mock_session.run.return_value = []

    engine = HybridSearchEngine(
        neo4j_uri="bolt://mock:7687",
        lazy_load=True
    )

    # Injecter les mocks
    engine._driver = mock_driver

    return engine, mock_session


# ═══════════════════════════════════════════════════════════════════════════════
# TESTS ANALYSE LINGUISTIQUE
# ═══════════════════════════════════════════════════════════════════════════════

class TestAnalyzeQuery:
    """Tests pour l'analyse de requête spaCy"""

    def test_analyze_empty_query(self, engine_with_mocks):
        """Une requête vide retourne des listes vides"""
        engine, _ = engine_with_mocks

        result = engine.analyze_query("")

        assert result["original"] == ""
        assert result["search_lemmas"] == []
        assert result["all_lemmas"] == []

    def test_analyze_none_query(self, engine_with_mocks):
        """Une requête None est gérée"""
        engine, _ = engine_with_mocks

        result = engine.analyze_query(None)

        assert result["search_lemmas"] == []

    def test_analyze_returns_structure(self, engine_with_mocks):
        """L'analyse retourne la structure attendue"""
        engine, _ = engine_with_mocks

        # Mock du nlp pour ce test
        mock_doc = MagicMock()
        mock_token = MagicMock()
        mock_token.text = "projet"
        mock_token.lemma_ = "projet"
        mock_token.pos_ = "NOUN"
        mock_token.is_stop = False
        mock_token.is_punct = False
        mock_token.is_space = False
        mock_doc.__iter__ = lambda self: iter([mock_token])

        engine._nlp = MagicMock(return_value=mock_doc)

        result = engine.analyze_query("projet")

        assert "original" in result
        assert "tokens" in result
        assert "search_lemmas" in result
        assert "all_lemmas" in result


# ═══════════════════════════════════════════════════════════════════════════════
# TESTS PONDÉRATION
# ═══════════════════════════════════════════════════════════════════════════════

class TestDetermineWeights:
    """Tests pour la pondération dynamique"""

    def test_high_confidence_favors_lexical(self, engine_with_mocks):
        """Haute confiance lexicale → mode LEXICAL_PRIORITY"""
        engine, _ = engine_with_mocks

        w_lex, w_sem, mode = engine.determine_weights(0.9)

        assert mode == SearchMode.LEXICAL_PRIORITY
        assert w_lex == 0.8
        assert w_sem == 0.2

    def test_low_confidence_favors_semantic(self, engine_with_mocks):
        """Basse confiance lexicale → mode SEMANTIC_PRIORITY"""
        engine, _ = engine_with_mocks

        w_lex, w_sem, mode = engine.determine_weights(0.2)

        assert mode == SearchMode.SEMANTIC_PRIORITY
        assert w_lex == 0.2
        assert w_sem == 0.8

    def test_medium_confidence_balanced(self, engine_with_mocks):
        """Confiance moyenne → mode BALANCED"""
        engine, _ = engine_with_mocks

        w_lex, w_sem, mode = engine.determine_weights(0.5)

        assert mode == SearchMode.BALANCED
        assert w_lex == 0.5
        assert w_sem == 0.5

    def test_threshold_boundaries(self, engine_with_mocks):
        """Test des valeurs limites des seuils"""
        engine, _ = engine_with_mocks

        # Exactement au seuil haut
        _, _, mode = engine.determine_weights(0.8)
        assert mode == SearchMode.LEXICAL_PRIORITY

        # Juste en dessous du seuil haut
        _, _, mode = engine.determine_weights(0.79)
        assert mode == SearchMode.BALANCED

        # Exactement au seuil bas
        _, _, mode = engine.determine_weights(0.3)
        assert mode == SearchMode.SEMANTIC_PRIORITY

        # Juste au-dessus du seuil bas
        _, _, mode = engine.determine_weights(0.31)
        assert mode == SearchMode.BALANCED


# ═══════════════════════════════════════════════════════════════════════════════
# TESTS FUSION
# ═══════════════════════════════════════════════════════════════════════════════

class TestMergeResults:
    """Tests pour la fusion des résultats"""

    def test_merge_disjoint_results(self, engine_with_mocks):
        """Fusion de résultats sans chevauchement"""
        engine, _ = engine_with_mocks

        lexical = [
            SearchResult(concept_name="projet", lemma="projet", lexical_score=1.0, source="lexical")
        ]
        semantic = [
            SearchResult(concept_name="travail", lemma="travail", semantic_score=0.8, source="semantic")
        ]

        merged = engine.merge_results(lexical, semantic, 0.5, 0.5)

        assert len(merged) == 2
        # Projet a lexical_score=1.0 * 0.5 = 0.5
        # Travail a semantic_score=0.8 * 0.5 = 0.4
        assert merged[0].concept_name == "projet"
        assert merged[1].concept_name == "travail"

    def test_merge_overlapping_results(self, engine_with_mocks):
        """Fusion de résultats avec chevauchement"""
        engine, _ = engine_with_mocks

        lexical = [
            SearchResult(concept_name="projet", lemma="projet", lexical_score=1.0, source="lexical")
        ]
        semantic = [
            SearchResult(concept_name="projet", lemma="projet", semantic_score=0.9, source="semantic")
        ]

        merged = engine.merge_results(lexical, semantic, 0.5, 0.5)

        assert len(merged) == 1
        assert merged[0].concept_name == "projet"
        assert merged[0].source == "both"
        assert merged[0].lexical_score == 1.0
        assert merged[0].semantic_score == 0.9
        # Score final: 1.0 * 0.5 + 0.9 * 0.5 = 0.95
        assert merged[0].final_score == pytest.approx(0.95)

    def test_merge_empty_lexical(self, engine_with_mocks):
        """Fusion avec résultats lexicaux vides"""
        engine, _ = engine_with_mocks

        semantic = [
            SearchResult(concept_name="test", lemma="test", semantic_score=0.7, source="semantic")
        ]

        merged = engine.merge_results([], semantic, 0.5, 0.5)

        assert len(merged) == 1
        assert merged[0].source == "semantic"

    def test_merge_preserves_emotional_data(self, engine_with_mocks):
        """La fusion préserve les données émotionnelles"""
        engine, _ = engine_with_mocks

        lexical = [
            SearchResult(
                concept_name="joie",
                lemma="joie",
                lexical_score=1.0,
                emotional_states={"1": [0.9] + [0.0] * 23},
                dominant_emotion="Joie",
                avg_valence=0.8,
                source="lexical"
            )
        ]

        merged = engine.merge_results(lexical, [], 1.0, 0.0)

        assert merged[0].dominant_emotion == "Joie"
        assert merged[0].avg_valence == 0.8
        assert "1" in merged[0].emotional_states


# ═══════════════════════════════════════════════════════════════════════════════
# TESTS RECHERCHE LEXICALE
# ═══════════════════════════════════════════════════════════════════════════════

class TestSearchLexical:
    """Tests pour la recherche lexicale"""

    def test_empty_lemmas_returns_empty(self, engine_with_mocks):
        """Liste de lemmes vide retourne résultats vides"""
        engine, _ = engine_with_mocks

        results, confidence = engine.search_lexical([])

        assert results == []
        assert confidence == 0.0

    def test_search_lexical_returns_results(self, engine_with_mocks):
        """Recherche lexicale retourne des résultats"""
        engine, mock_session = engine_with_mocks

        # Mock des résultats Neo4j
        mock_record = MagicMock()
        mock_record.__getitem__ = lambda self, key: {
            "name": "projet",
            "emotional_states": '{"1": [0.5, 0.0]}',
            "concept_memory_ids": ["MEM_1"],
            "linked_memory_ids": [],
            "trauma": False
        }.get(key)

        mock_session.run.return_value = [mock_record]

        results, confidence = engine.search_lexical(["projet"])

        assert len(results) == 1
        assert results[0].concept_name == "projet"
        assert confidence > 0


# ═══════════════════════════════════════════════════════════════════════════════
# TESTS EMBEDDINGS
# ═══════════════════════════════════════════════════════════════════════════════

class TestEmbeddings:
    """Tests pour la génération d'embeddings"""

    def test_empty_text_returns_zero_embedding(self, engine_with_mocks):
        """Texte vide retourne un embedding nul"""
        engine, _ = engine_with_mocks

        emb = engine.get_embedding("")

        assert emb.shape == (768,)
        assert np.allclose(emb, 0)

    def test_none_text_returns_zero_embedding(self, engine_with_mocks):
        """Texte None retourne un embedding nul"""
        engine, _ = engine_with_mocks

        emb = engine.get_embedding(None)

        assert np.allclose(emb, 0)

    def test_embedding_cache(self, engine_with_mocks):
        """Le cache d'embeddings fonctionne"""
        engine, _ = engine_with_mocks

        # Pré-remplir le cache
        test_emb = np.random.rand(768)
        engine._embedding_cache["test"] = test_emb

        result = engine.get_embedding("test", use_cache=True)

        assert np.array_equal(result, test_emb)


# ═══════════════════════════════════════════════════════════════════════════════
# TESTS FORMATAGE LLM
# ═══════════════════════════════════════════════════════════════════════════════

class TestFormatForLLM:
    """Tests pour le formatage LLM"""

    def test_format_empty_results(self):
        """Formatage avec résultats vides"""
        response = SearchResponse(
            results=[],
            mode_used=SearchMode.BALANCED,
            lexical_confidence=0.0,
            semantic_coverage=0.0,
            query_tokens=[],
            query_lemmas=[]
        )

        llm_context = format_for_llm(response)

        assert llm_context["emotions"] == []
        assert llm_context["context_words"] == []
        assert llm_context["search_mode"] == "balanced"

    def test_format_with_emotions(self):
        """Formatage avec émotions détectées"""
        results = [
            SearchResult(
                concept_name="bonheur",
                lemma="bonheur",
                dominant_emotion="Joie",
                avg_valence=0.9,
                final_score=0.85
            ),
            SearchResult(
                concept_name="projet",
                lemma="projet",
                dominant_emotion="Neutre",
                final_score=0.7
            )
        ]

        response = SearchResponse(
            results=results,
            mode_used=SearchMode.LEXICAL_PRIORITY,
            lexical_confidence=0.9,
            semantic_coverage=0.5,
            query_tokens=["bonheur"],
            query_lemmas=["bonheur"]
        )

        llm_context = format_for_llm(response)

        # Seule "Joie" doit être incluse (pas "Neutre")
        assert len(llm_context["emotions"]) == 1
        assert llm_context["emotions"][0]["emotion"] == "Joie"
        assert "bonheur" in llm_context["context_words"]

    def test_format_max_three_emotions(self):
        """Maximum 3 émotions dans le formatage"""
        results = [
            SearchResult(concept_name=f"concept_{i}", lemma=f"c{i}",
                        dominant_emotion=f"Emotion{i}", final_score=0.9-i*0.1)
            for i in range(5)
        ]

        response = SearchResponse(
            results=results,
            mode_used=SearchMode.BALANCED,
            lexical_confidence=0.5,
            semantic_coverage=0.5,
            query_tokens=[],
            query_lemmas=[]
        )

        llm_context = format_for_llm(response)

        assert len(llm_context["emotions"]) <= 3


class TestGenerateLLMPrompt:
    """Tests pour la génération de prompt LLM"""

    def test_generate_prompt_with_emotions(self):
        """Génération de prompt avec émotions"""
        llm_context = {
            "emotions": [
                {"emotion": "Joie", "confidence": 0.9, "valence": 0.8}
            ],
            "context_words": ["projet", "réussite"]
        }

        prompt = generate_llm_prompt(llm_context, "Comment ça va ?")

        assert "Joie" in prompt
        assert "projet" in prompt
        assert "Comment ça va ?" in prompt

    def test_generate_prompt_no_emotions(self):
        """Génération de prompt sans émotions"""
        llm_context = {
            "emotions": [],
            "context_words": []
        }

        prompt = generate_llm_prompt(llm_context, "Test")

        assert "Aucune émotion" in prompt


# ═══════════════════════════════════════════════════════════════════════════════
# TESTS SEARCHRESULT
# ═══════════════════════════════════════════════════════════════════════════════

class TestSearchResult:
    """Tests pour la dataclass SearchResult"""

    def test_default_values(self):
        """Valeurs par défaut correctes"""
        result = SearchResult(concept_name="test", lemma="test")

        assert result.lexical_score == 0.0
        assert result.semantic_score == 0.0
        assert result.final_score == 0.0
        assert result.dominant_emotion == "Neutre"
        assert result.emotional_states == {}
        assert result.memory_ids == []

    def test_to_dict(self):
        """Conversion en dictionnaire"""
        result = SearchResult(
            concept_name="test",
            lemma="test",
            lexical_score=0.5,
            semantic_score=0.7,
            final_score=0.6,
            source="both"
        )

        d = result.to_dict()

        assert d["concept_name"] == "test"
        assert d["lexical_score"] == 0.5
        assert d["source"] == "both"


# ═══════════════════════════════════════════════════════════════════════════════
# TESTS D'INTÉGRATION (avec mocks)
# ═══════════════════════════════════════════════════════════════════════════════

class TestSearchIntegration:
    """Tests d'intégration de bout en bout avec mocks"""

    def test_full_search_pipeline(self, engine_with_mocks):
        """Test du pipeline complet de recherche"""
        engine, mock_session = engine_with_mocks

        # Mock de l'analyse spaCy
        mock_doc = MagicMock()
        mock_token = MagicMock()
        mock_token.text = "projet"
        mock_token.lemma_ = "projet"
        mock_token.pos_ = "NOUN"
        mock_token.is_stop = False
        mock_token.is_punct = False
        mock_token.is_space = False
        mock_doc.__iter__ = lambda self: iter([mock_token])
        engine._nlp = MagicMock(return_value=mock_doc)

        # Mock de l'embedding
        engine._embedding_cache["comment va le projet"] = np.random.rand(768)

        # Mock des résultats Neo4j
        mock_session.run.return_value = []

        # Exécuter la recherche
        response = engine.search("Comment va le projet ?", limit=5)

        # Vérifications
        assert isinstance(response, SearchResponse)
        assert response.mode_used in SearchMode
        assert isinstance(response.results, list)


# ═══════════════════════════════════════════════════════════════════════════════
# TESTS DE ROBUSTESSE
# ═══════════════════════════════════════════════════════════════════════════════

class TestRobustness:
    """Tests de robustesse et cas limites"""

    def test_neo4j_connection_error_lexical(self, engine_with_mocks):
        """Gestion des erreurs de connexion Neo4j en recherche lexicale"""
        engine, mock_session = engine_with_mocks

        from neo4j.exceptions import ServiceUnavailable
        mock_session.run.side_effect = ServiceUnavailable("Connection failed")

        results, confidence = engine.search_lexical(["test"])

        assert results == []
        assert confidence == 0.0

    def test_neo4j_connection_error_semantic(self, engine_with_mocks):
        """Gestion des erreurs de connexion Neo4j en recherche sémantique"""
        engine, mock_session = engine_with_mocks

        from neo4j.exceptions import ServiceUnavailable
        mock_session.run.side_effect = ServiceUnavailable("Connection failed")

        query_emb = np.random.rand(768)
        results, coverage = engine.search_semantic(query_emb)

        assert results == []
        assert coverage == 0.0

    def test_malformed_emotional_states_json(self, engine_with_mocks):
        """Gestion des emotional_states JSON malformés"""
        engine, mock_session = engine_with_mocks

        mock_record = MagicMock()
        mock_record.__getitem__ = lambda self, key: {
            "name": "test",
            "emotional_states": "invalid json {{{",  # JSON invalide
            "concept_memory_ids": [],
            "linked_memory_ids": [],
            "trauma": False
        }.get(key)

        mock_session.run.return_value = [mock_record]

        results, _ = engine.search_lexical(["test"])

        # Ne doit pas planter, retourne états vides
        assert len(results) == 1
        assert results[0].emotional_states == {}


# ═══════════════════════════════════════════════════════════════════════════════
# RUN TESTS
# ═══════════════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    pytest.main([__file__, "-v", "--tb=short"])
