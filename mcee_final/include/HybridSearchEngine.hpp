/**
 * @file HybridSearchEngine.hpp
 * @brief Moteur de recherche hybride (lexical + sémantique) avec modulation émotionnelle
 * @version 1.0
 * @date 2024-12
 *
 * Combine recherche lexicale (mots-clés) et sémantique (embeddings) dans Neo4j,
 * avec modulation par le sentiment global Ft du ConscienceEngine.
 *
 * Pipeline :
 * 1. Parse les tokens (lemmes, POS, entités)
 * 2. Recherche lexicale dans Neo4j (mots → souvenirs)
 * 3. Recherche sémantique (embedding → souvenirs similaires)
 * 4. Fusion pondérée selon Ft et Ct
 * 5. Construction du contexte émotionnel pour LLMClient
 */

#ifndef MCEE_HYBRID_SEARCH_ENGINE_HPP
#define MCEE_HYBRID_SEARCH_ENGINE_HPP

#include "Types.hpp"
#include "Neo4jClient.hpp"
#include "ConscienceEngine.hpp"
#include "LLMClient.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>

namespace mcee {

using json = nlohmann::json;

// ═══════════════════════════════════════════════════════════════════════════
// STRUCTURES DE DONNÉES
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Token parsé depuis le module Python (spaCy)
 */
struct ParsedToken {
    std::string text;           // Texte original
    std::string lemma;          // Forme lemmatisée
    std::string pos;            // Part-of-speech (NOUN, VERB, ADJ, etc.)
    std::string dep;            // Dépendance syntaxique
    bool is_stop = false;       // Mot vide
    bool is_significant = true; // Significatif pour la recherche

    // Entité nommée (optionnel)
    std::string entity_type;    // PERSON, ORG, LOC, etc.
    std::string entity_value;

    json toJson() const {
        return {
            {"text", text},
            {"lemma", lemma},
            {"pos", pos},
            {"dep", dep},
            {"is_stop", is_stop},
            {"is_significant", is_significant},
            {"entity_type", entity_type},
            {"entity_value", entity_value}
        };
    }

    static ParsedToken fromJson(const json& j) {
        ParsedToken t;
        t.text = j.value("text", "");
        t.lemma = j.value("lemma", "");
        t.pos = j.value("pos", "");
        t.dep = j.value("dep", "");
        t.is_stop = j.value("is_stop", false);
        t.is_significant = j.value("is_significant", true);
        t.entity_type = j.value("entity_type", "");
        t.entity_value = j.value("entity_value", "");
        return t;
    }
};

/**
 * @brief Résultat de recherche (souvenir trouvé)
 */
struct SearchResult {
    std::string memory_id;              // ID du souvenir Neo4j
    std::string memory_name;            // Nom/description
    double lexical_score = 0.0;         // Score recherche lexicale
    double semantic_score = 0.0;        // Score recherche sémantique
    double combined_score = 0.0;        // Score combiné pondéré
    double emotional_relevance = 0.0;   // Pertinence émotionnelle

    // Émotions du souvenir
    std::array<double, NUM_EMOTIONS> emotions{};
    std::string dominant_emotion;
    double dominant_score = 0.0;

    // Mots-clés associés
    std::vector<std::string> keywords;

    // Métadonnées
    bool is_trauma = false;
    int activation_count = 0;
    std::string last_activated;

    json toJson() const {
        json j;
        j["memory_id"] = memory_id;
        j["memory_name"] = memory_name;
        j["lexical_score"] = lexical_score;
        j["semantic_score"] = semantic_score;
        j["combined_score"] = combined_score;
        j["emotional_relevance"] = emotional_relevance;
        j["dominant_emotion"] = dominant_emotion;
        j["dominant_score"] = dominant_score;
        j["keywords"] = keywords;
        j["is_trauma"] = is_trauma;
        j["activation_count"] = activation_count;
        return j;
    }
};

/**
 * @brief Mode de recherche hybride
 */
enum class SearchMode {
    LEXICAL_ONLY,       // Uniquement mots-clés
    SEMANTIC_ONLY,      // Uniquement embeddings
    BALANCED,           // 50/50 lexical/sémantique
    ADAPTIVE            // Adapté selon Ft et disponibilité
};

/**
 * @brief Réponse complète de recherche
 */
struct SearchResponse {
    // Résultats
    std::vector<SearchResult> results;

    // Contexte émotionnel construit
    LLMContext context;

    // Métriques
    double lexical_confidence = 0.0;
    double semantic_confidence = 0.0;
    double overall_confidence = 0.0;
    SearchMode mode_used = SearchMode::BALANCED;
    double search_time_ms = 0.0;

    // État de conscience utilisé
    double Ft = 0.0;
    double Ct = 0.0;

    json toJson() const {
        json j;
        j["results"] = json::array();
        for (const auto& r : results) {
            j["results"].push_back(r.toJson());
        }
        j["context"] = context.toJson();
        j["lexical_confidence"] = lexical_confidence;
        j["semantic_confidence"] = semantic_confidence;
        j["overall_confidence"] = overall_confidence;
        j["mode_used"] = static_cast<int>(mode_used);
        j["search_time_ms"] = search_time_ms;
        j["Ft"] = Ft;
        j["Ct"] = Ct;
        return j;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Configuration du moteur de recherche hybride
 */
struct HybridSearchConfig {
    // Mode par défaut
    SearchMode default_mode = SearchMode::ADAPTIVE;

    // Pondération lexical/sémantique (BALANCED)
    double lexical_weight = 0.5;
    double semantic_weight = 0.5;

    // Seuils
    double min_lexical_score = 0.3;     // Score minimum pour inclusion
    double min_semantic_score = 0.7;    // Similarité cosinus minimum
    double trauma_boost = 1.5;          // Boost pour traumas pertinents

    // Limites
    size_t max_results = 10;
    size_t max_keywords_per_result = 5;

    // Modulation par sentiment
    bool enable_sentiment_modulation = true;
    double negative_sentiment_threshold = -0.3;  // En dessous, boost souvenirs positifs
    double positive_sentiment_threshold = 0.3;   // Au dessus, boost souvenirs similaires

    // Cache
    bool enable_cache = true;
    int cache_ttl_seconds = 300;

    // Debug
    bool verbose = false;
};

// ═══════════════════════════════════════════════════════════════════════════
// CLASSE HYBRIDSEARCHENGINE
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @class HybridSearchEngine
 * @brief Moteur de recherche combinant lexical et sémantique avec conscience émotionnelle
 */
class HybridSearchEngine {
public:
    /**
     * @brief Constructeur
     * @param neo4j_client Client Neo4j pour les requêtes
     * @param conscience_engine Moteur de conscience pour Ft/Ct
     * @param config Configuration
     */
    HybridSearchEngine(
        std::shared_ptr<Neo4jClient> neo4j_client,
        std::shared_ptr<ConscienceEngine> conscience_engine,
        const HybridSearchConfig& config = HybridSearchConfig{}
    );

    /**
     * @brief Destructeur
     */
    ~HybridSearchEngine() = default;

    // Non-copyable
    HybridSearchEngine(const HybridSearchEngine&) = delete;
    HybridSearchEngine& operator=(const HybridSearchEngine&) = delete;

    // ═══════════════════════════════════════════════════════════════════════
    // RECHERCHE PRINCIPALE
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Recherche hybride complète
     * @param question Question utilisateur (texte brut)
     * @param tokens Tokens parsés (depuis module Python)
     * @param embedding Embedding de la question (optionnel)
     * @return Réponse de recherche avec contexte
     */
    SearchResponse search(
        const std::string& question,
        const std::vector<ParsedToken>& tokens,
        const std::vector<double>& embedding = {}
    );

    /**
     * @brief Recherche avec lemmes uniquement (raccourci)
     * @param lemmas Liste de lemmes significatifs
     * @param embedding Embedding (optionnel)
     * @return Réponse de recherche
     */
    SearchResponse searchByLemmas(
        const std::vector<std::string>& lemmas,
        const std::vector<double>& embedding = {}
    );

    // ═══════════════════════════════════════════════════════════════════════
    // PARSING TOKENS
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Parse les tokens depuis JSON (format module Python)
     * @param tokens_json JSON des tokens
     * @return Tokens parsés
     */
    std::vector<ParsedToken> parseTokens(const json& tokens_json) const;

    /**
     * @brief Extrait les lemmes significatifs
     * @param tokens Tokens parsés
     * @return Lemmes filtrés (NOUN, VERB, ADJ, PROPN)
     */
    std::vector<std::string> extractSignificantLemmas(
        const std::vector<ParsedToken>& tokens) const;

    // ═══════════════════════════════════════════════════════════════════════
    // CONSTRUCTION DE CONTEXTE
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Construit le contexte LLM à partir des résultats
     * @param results Résultats de recherche
     * @param lemmas Lemmes de la question
     * @return Contexte émotionnel pour LLMClient
     */
    LLMContext buildContext(
        const std::vector<SearchResult>& results,
        const std::vector<std::string>& lemmas
    ) const;

    // ═══════════════════════════════════════════════════════════════════════
    // ACCESSEURS
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Retourne la configuration
     */
    [[nodiscard]] const HybridSearchConfig& getConfig() const { return config_; }

    /**
     * @brief Met à jour la configuration
     */
    void setConfig(const HybridSearchConfig& config) { config_ = config; }

    /**
     * @brief Retourne le client Neo4j
     */
    [[nodiscard]] std::shared_ptr<Neo4jClient> getNeo4jClient() const { return neo4j_; }

    /**
     * @brief Retourne le ConscienceEngine
     */
    [[nodiscard]] std::shared_ptr<ConscienceEngine> getConscienceEngine() const { return conscience_; }

private:
    std::shared_ptr<Neo4jClient> neo4j_;
    std::shared_ptr<ConscienceEngine> conscience_;
    HybridSearchConfig config_;

    // Cache simple (clé: hash des lemmas)
    mutable std::unordered_map<std::string, std::pair<SearchResponse, std::chrono::steady_clock::time_point>> cache_;
    mutable std::mutex cache_mutex_;

    // ═══════════════════════════════════════════════════════════════════════
    // MÉTHODES PRIVÉES
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Recherche lexicale dans Neo4j
     */
    std::vector<SearchResult> searchLexical(const std::vector<std::string>& lemmas);

    /**
     * @brief Recherche sémantique par embedding
     */
    std::vector<SearchResult> searchSemantic(const std::vector<double>& embedding);

    /**
     * @brief Fusionne les résultats lexicaux et sémantiques
     */
    std::vector<SearchResult> mergeResults(
        const std::vector<SearchResult>& lexical_results,
        const std::vector<SearchResult>& semantic_results,
        double lexical_weight,
        double semantic_weight
    );

    /**
     * @brief Applique la modulation par sentiment (Ft)
     */
    void applySentimentModulation(
        std::vector<SearchResult>& results,
        double Ft
    );

    /**
     * @brief Détermine le mode de recherche adaptatif
     */
    SearchMode determineSearchMode(
        const std::vector<std::string>& lemmas,
        const std::vector<double>& embedding,
        double Ft,
        double Ct
    ) const;

    /**
     * @brief Calcule les poids adaptatifs lexical/sémantique
     */
    std::pair<double, double> computeAdaptiveWeights(
        double Ft,
        double Ct,
        double lexical_confidence,
        double semantic_confidence
    ) const;

    /**
     * @brief Génère une clé de cache
     */
    std::string generateCacheKey(const std::vector<std::string>& lemmas) const;

    /**
     * @brief Vérifie si une entrée de cache est valide
     */
    bool isCacheValid(const std::string& key) const;

    /**
     * @brief Log verbose
     */
    void log(const std::string& message) const;
};

} // namespace mcee

#endif // MCEE_HYBRID_SEARCH_ENGINE_HPP
