#pragma once

#include "Types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <atomic>
#include <functional>
#include <optional>
#include <nlohmann/json.hpp>

namespace mcee {

// ============================================================================
// Configuration
// ============================================================================

struct MCTGraphConfig {
    // Fenêtre temporelle de rétention des nœuds
    double time_window_seconds = 120.0;  // 2 minutes

    // Seuil minimum de persistance pour stocker une émotion
    double emotion_persistence_threshold_seconds = 2.0;

    // Délai maximum mot→émotion pour relation causale
    double causality_threshold_ms = 500.0;

    // Intervalle d'export snapshot vers module rêves
    double snapshot_interval_seconds = 10.0;

    // Poids initial des arêtes
    double initial_causal_weight = 1.0;
    double initial_temporal_weight = 0.5;
    double initial_semantic_weight = 0.8;

    // Décroissance temporelle des arêtes
    double edge_decay_factor = 0.95;

    // Nombre maximum de nœuds (protection mémoire)
    size_t max_nodes = 500;

    // Seuil de causalité étendu pour émotions lentes (tristesse, nostalgie)
    double slow_emotion_causality_threshold_ms = 800.0;
};

// ============================================================================
// Types de nœuds
// ============================================================================

enum class NodeType {
    WORD,
    EMOTION
};

enum class EdgeType {
    CAUSAL,     // Le mot a déclenché l'émotion
    TEMPORAL,   // Co-occurrence temporelle
    SEMANTIC    // Relation sémantique entre mots
};

// ============================================================================
// Nœud WORD - Mot extrait par Neo4j/spaCy
// ============================================================================

struct WordNode {
    std::string id;                    // Identifiant unique
    std::string lemma;                 // Forme lemmatisée
    std::string pos;                   // Catégorie grammaticale (NOUN, VERB, ADJ...)
    std::string sentence_id;           // ID de la phrase source
    std::string original_form;         // Forme originale du mot

    std::chrono::steady_clock::time_point timestamp;

    // Métadonnées optionnelles
    double sentiment_score = 0.0;      // Score de sentiment si disponible
    bool is_negation = false;          // Mot de négation
    bool is_intensifier = false;       // Mot intensificateur

    nlohmann::json toJson() const;
    static WordNode fromJson(const nlohmann::json& j);
};

// ============================================================================
// Nœud EMOTION - État émotionnel persistant
// ============================================================================

struct EmotionNode {
    std::string id;                    // Identifiant unique
    std::array<double, 24> emotions;   // Vecteur des 24 émotions
    double intensity;                  // Intensité globale
    std::string dominant_emotion;      // Émotion dominante
    double persistence_duration;       // Durée de persistance en secondes

    std::chrono::steady_clock::time_point timestamp;
    std::chrono::steady_clock::time_point end_timestamp;  // Fin de persistance

    // Contexte additionnel
    double valence = 0.0;              // Valence émotionnelle [-1, 1]
    double arousal = 0.0;              // Niveau d'activation [0, 1]

    nlohmann::json toJson() const;
    static EmotionNode fromJson(const nlohmann::json& j);

    // Vérifie si c'est une émotion "lente" (tristesse, nostalgie, calme)
    bool isSlowEmotion() const;
};

// ============================================================================
// Arête du graphe
// ============================================================================

struct GraphEdge {
    std::string id;
    std::string source_id;             // ID du nœud source
    std::string target_id;             // ID du nœud cible
    EdgeType type;
    double weight;                     // Poids de l'arête [0, 1]

    std::chrono::steady_clock::time_point created_at;

    // Métadonnées selon le type
    double temporal_distance_ms = 0.0; // Distance temporelle (CAUSAL, TEMPORAL)
    std::string semantic_relation;     // Type de relation sémantique (SEMANTIC)

    nlohmann::json toJson() const;
    static GraphEdge fromJson(const nlohmann::json& j);
};

// ============================================================================
// Snapshot pour export vers module rêves
// ============================================================================

struct MCTGraphSnapshot {
    std::string snapshot_id;
    std::chrono::system_clock::time_point timestamp;

    // Nœuds
    std::vector<WordNode> word_nodes;
    std::vector<EmotionNode> emotion_nodes;

    // Arêtes
    std::vector<GraphEdge> edges;

    // Statistiques globales
    struct Statistics {
        size_t total_words = 0;
        size_t total_emotions = 0;
        size_t causal_edges = 0;
        size_t temporal_edges = 0;
        size_t semantic_edges = 0;

        double average_emotion_intensity = 0.0;
        std::string most_frequent_emotion;
        std::vector<std::string> top_trigger_words;  // Mots causant le plus d'émotions

        double graph_density = 0.0;    // Densité du graphe
        double time_span_seconds = 0.0; // Étendue temporelle couverte
    } stats;

    nlohmann::json toJson() const;
    static MCTGraphSnapshot fromJson(const nlohmann::json& j);
};

// ============================================================================
// Résultat d'analyse causale
// ============================================================================

struct CausalAnalysis {
    std::string word_id;
    std::string word_lemma;
    std::vector<std::string> triggered_emotion_ids;
    double causal_strength;            // Force causale agrégée
    int trigger_count;                 // Nombre de fois que ce mot a déclenché une émotion
};

// ============================================================================
// Classe principale MCTGraph
// ============================================================================

class MCTGraph {
public:
    using SnapshotCallback = std::function<void(const MCTGraphSnapshot&)>;
    using CausalDetectionCallback = std::function<void(const std::string& word_id,
                                                        const std::string& emotion_id,
                                                        double strength)>;

    explicit MCTGraph(const MCTGraphConfig& config = MCTGraphConfig{});
    ~MCTGraph() = default;

    // Non-copiable mais déplaçable
    MCTGraph(const MCTGraph&) = delete;
    MCTGraph& operator=(const MCTGraph&) = delete;
    MCTGraph(MCTGraph&&) = default;
    MCTGraph& operator=(MCTGraph&&) = default;

    // ========================================================================
    // Ajout de nœuds
    // ========================================================================

    /// Ajoute un mot extrait par Neo4j/spaCy
    std::string addWord(const std::string& lemma,
                        const std::string& pos,
                        const std::string& sentence_id,
                        const std::string& original_form = "");

    /// Ajoute un mot depuis un message JSON (format Neo4j)
    std::string addWordFromJson(const nlohmann::json& word_data);

    /// Ajoute un état émotionnel persistant
    std::string addEmotion(const EmotionalState& state,
                           double persistence_duration);

    /// Ajoute une émotion depuis le format existant avec contexte
    std::string addEmotionWithContext(const EmotionalState& state,
                                       double persistence_duration,
                                       double valence,
                                       double arousal);

    // ========================================================================
    // Gestion des arêtes
    // ========================================================================

    /// Ajoute une arête causale (mot → émotion)
    std::string addCausalEdge(const std::string& word_id,
                               const std::string& emotion_id,
                               double weight = -1.0);  // -1 = calculer automatiquement

    /// Ajoute une arête temporelle
    std::string addTemporalEdge(const std::string& node1_id,
                                 const std::string& node2_id);

    /// Ajoute une arête sémantique entre deux mots
    std::string addSemanticEdge(const std::string& word1_id,
                                 const std::string& word2_id,
                                 const std::string& relation_type,
                                 double weight = -1.0);

    // ========================================================================
    // Détection automatique de causalité
    // ========================================================================

    /// Détecte et crée les arêtes causales pour une nouvelle émotion
    void detectCausality(const std::string& emotion_id);

    /// Détecte les co-occurrences temporelles
    void detectTemporalCooccurrences(const std::string& node_id);

    // ========================================================================
    // Requêtes sur le graphe
    // ========================================================================

    /// Récupère tous les mots ayant déclenché une émotion
    std::vector<WordNode> getTriggerWords(const std::string& emotion_id) const;

    /// Récupère toutes les émotions déclenchées par un mot
    std::vector<EmotionNode> getTriggeredEmotions(const std::string& word_id) const;

    /// Analyse causale globale
    std::vector<CausalAnalysis> analyzeCausality() const;

    /// Récupère les voisins d'un nœud
    std::vector<std::string> getNeighbors(const std::string& node_id,
                                           std::optional<EdgeType> edge_type = std::nullopt) const;

    /// Vérifie si un nœud existe
    bool hasNode(const std::string& node_id) const;

    /// Récupère un nœud mot par ID
    std::optional<WordNode> getWordNode(const std::string& id) const;

    /// Récupère un nœud émotion par ID
    std::optional<EmotionNode> getEmotionNode(const std::string& id) const;

    // ========================================================================
    // Maintenance du graphe
    // ========================================================================

    /// Supprime les nœuds expirés (hors fenêtre temporelle)
    size_t pruneExpiredNodes();

    /// Applique la décroissance temporelle aux poids des arêtes
    void applyEdgeDecay();

    /// Nettoie le graphe complet
    void clear();

    // ========================================================================
    // Export et snapshots
    // ========================================================================

    /// Génère un snapshot de l'état actuel du graphe
    MCTGraphSnapshot createSnapshot() const;

    /// Exporte le graphe en JSON
    nlohmann::json toJson() const;

    /// Charge un graphe depuis JSON
    void loadFromJson(const nlohmann::json& j);

    // ========================================================================
    // Callbacks
    // ========================================================================

    /// Définit le callback pour l'export périodique des snapshots
    void setSnapshotCallback(SnapshotCallback callback);

    /// Définit le callback pour la détection de causalité
    void setCausalDetectionCallback(CausalDetectionCallback callback);

    /// Déclenche manuellement un snapshot (appelé par le timer externe)
    void triggerSnapshot();

    // ========================================================================
    // Statistiques
    // ========================================================================

    size_t getWordCount() const;
    size_t getEmotionCount() const;
    size_t getEdgeCount() const;
    size_t getCausalEdgeCount() const;

    /// Retourne la densité du graphe
    double getGraphDensity() const;

    /// Retourne la configuration
    const MCTGraphConfig& getConfig() const { return config_; }

private:
    MCTGraphConfig config_;

    // Stockage des nœuds
    std::unordered_map<std::string, WordNode> word_nodes_;
    std::unordered_map<std::string, EmotionNode> emotion_nodes_;

    // Stockage des arêtes (par ID)
    std::unordered_map<std::string, GraphEdge> edges_;

    // Index d'adjacence (node_id → liste d'edge_ids)
    std::unordered_map<std::string, std::vector<std::string>> adjacency_;

    // Compteur pour génération d'IDs
    mutable std::atomic<uint64_t> id_counter_{0};

    // Thread safety
    mutable std::mutex mutex_;

    // Callbacks
    SnapshotCallback snapshot_callback_;
    CausalDetectionCallback causal_callback_;

    // Dernier snapshot
    std::chrono::steady_clock::time_point last_snapshot_time_;

    // ========================================================================
    // Méthodes privées
    // ========================================================================

    std::string generateId(const std::string& prefix) const;

    double calculateCausalWeight(const WordNode& word,
                                  const EmotionNode& emotion) const;

    double calculateTemporalDistance(
        const std::chrono::steady_clock::time_point& t1,
        const std::chrono::steady_clock::time_point& t2) const;

    bool isWithinCausalityWindow(const WordNode& word,
                                  const EmotionNode& emotion) const;

    void removeEdgesForNode(const std::string& node_id);

    void updateAdjacency(const std::string& edge_id,
                          const std::string& source_id,
                          const std::string& target_id);

    void computeSnapshotStatistics(MCTGraphSnapshot& snapshot) const;

    std::string findDominantEmotion(const std::array<double, 24>& emotions) const;
};

// ============================================================================
// Fonctions utilitaires
// ============================================================================

/// Convertit EdgeType en string
std::string edgeTypeToString(EdgeType type);

/// Convertit string en EdgeType
EdgeType stringToEdgeType(const std::string& str);

/// Convertit NodeType en string
std::string nodeTypeToString(NodeType type);

} // namespace mcee
