#include "MCTGraph.hpp"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace mcee {

// ============================================================================
// Fonctions utilitaires
// ============================================================================

std::string edgeTypeToString(EdgeType type) {
    switch (type) {
        case EdgeType::CAUSAL: return "CAUSAL";
        case EdgeType::TEMPORAL: return "TEMPORAL";
        case EdgeType::SEMANTIC: return "SEMANTIC";
        default: return "UNKNOWN";
    }
}

EdgeType stringToEdgeType(const std::string& str) {
    if (str == "CAUSAL") return EdgeType::CAUSAL;
    if (str == "TEMPORAL") return EdgeType::TEMPORAL;
    if (str == "SEMANTIC") return EdgeType::SEMANTIC;
    return EdgeType::TEMPORAL; // défaut
}

std::string nodeTypeToString(NodeType type) {
    switch (type) {
        case NodeType::WORD: return "WORD";
        case NodeType::EMOTION: return "EMOTION";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// WordNode
// ============================================================================

nlohmann::json WordNode::toJson() const {
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();

    return {
        {"id", id},
        {"type", "WORD"},
        {"lemma", lemma},
        {"pos", pos},
        {"sentence_id", sentence_id},
        {"original_form", original_form},
        {"timestamp_ms", ts},
        {"sentiment_score", sentiment_score},
        {"is_negation", is_negation},
        {"is_intensifier", is_intensifier}
    };
}

WordNode WordNode::fromJson(const nlohmann::json& j) {
    WordNode node;
    node.id = j.value("id", "");
    node.lemma = j.value("lemma", "");
    node.pos = j.value("pos", "");
    node.sentence_id = j.value("sentence_id", "");
    node.original_form = j.value("original_form", "");
    node.sentiment_score = j.value("sentiment_score", 0.0);
    node.is_negation = j.value("is_negation", false);
    node.is_intensifier = j.value("is_intensifier", false);

    if (j.contains("timestamp_ms")) {
        auto ms = j["timestamp_ms"].get<int64_t>();
        node.timestamp = std::chrono::steady_clock::time_point(
            std::chrono::milliseconds(ms));
    } else {
        node.timestamp = std::chrono::steady_clock::now();
    }

    return node;
}

// ============================================================================
// EmotionNode
// ============================================================================

nlohmann::json EmotionNode::toJson() const {
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();
    auto end_ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_timestamp.time_since_epoch()).count();

    nlohmann::json emotions_array = nlohmann::json::array();
    for (const auto& e : emotions) {
        emotions_array.push_back(e);
    }

    return {
        {"id", id},
        {"type", "EMOTION"},
        {"emotions", emotions_array},
        {"intensity", intensity},
        {"dominant_emotion", dominant_emotion},
        {"persistence_duration", persistence_duration},
        {"timestamp_ms", ts},
        {"end_timestamp_ms", end_ts},
        {"valence", valence},
        {"arousal", arousal}
    };
}

EmotionNode EmotionNode::fromJson(const nlohmann::json& j) {
    EmotionNode node;
    node.id = j.value("id", "");
    node.intensity = j.value("intensity", 0.0);
    node.dominant_emotion = j.value("dominant_emotion", "");
    node.persistence_duration = j.value("persistence_duration", 0.0);
    node.valence = j.value("valence", 0.0);
    node.arousal = j.value("arousal", 0.0);

    if (j.contains("emotions") && j["emotions"].is_array()) {
        const auto& arr = j["emotions"];
        for (size_t i = 0; i < std::min(arr.size(), size_t(24)); ++i) {
            node.emotions[i] = arr[i].get<double>();
        }
    }

    if (j.contains("timestamp_ms")) {
        auto ms = j["timestamp_ms"].get<int64_t>();
        node.timestamp = std::chrono::steady_clock::time_point(
            std::chrono::milliseconds(ms));
    } else {
        node.timestamp = std::chrono::steady_clock::now();
    }

    if (j.contains("end_timestamp_ms")) {
        auto ms = j["end_timestamp_ms"].get<int64_t>();
        node.end_timestamp = std::chrono::steady_clock::time_point(
            std::chrono::milliseconds(ms));
    } else {
        node.end_timestamp = node.timestamp;
    }

    return node;
}

bool EmotionNode::isSlowEmotion() const {
    // Émotions "lentes" : tristesse (20), nostalgie (18), calme (8), ennui (7)
    static const std::vector<size_t> slow_indices = {20, 18, 8, 7};

    double slow_sum = 0.0;
    double total_sum = 0.0;

    for (size_t i = 0; i < 24; ++i) {
        total_sum += emotions[i];
        if (std::find(slow_indices.begin(), slow_indices.end(), i) != slow_indices.end()) {
            slow_sum += emotions[i];
        }
    }

    // Si les émotions lentes représentent > 40% du total
    return total_sum > 0.0 && (slow_sum / total_sum) > 0.4;
}

// ============================================================================
// GraphEdge
// ============================================================================

nlohmann::json GraphEdge::toJson() const {
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        created_at.time_since_epoch()).count();

    return {
        {"id", id},
        {"source_id", source_id},
        {"target_id", target_id},
        {"type", edgeTypeToString(type)},
        {"weight", weight},
        {"created_at_ms", ts},
        {"temporal_distance_ms", temporal_distance_ms},
        {"semantic_relation", semantic_relation}
    };
}

GraphEdge GraphEdge::fromJson(const nlohmann::json& j) {
    GraphEdge edge;
    edge.id = j.value("id", "");
    edge.source_id = j.value("source_id", "");
    edge.target_id = j.value("target_id", "");
    edge.type = stringToEdgeType(j.value("type", "TEMPORAL"));
    edge.weight = j.value("weight", 0.5);
    edge.temporal_distance_ms = j.value("temporal_distance_ms", 0.0);
    edge.semantic_relation = j.value("semantic_relation", "");

    if (j.contains("created_at_ms")) {
        auto ms = j["created_at_ms"].get<int64_t>();
        edge.created_at = std::chrono::steady_clock::time_point(
            std::chrono::milliseconds(ms));
    } else {
        edge.created_at = std::chrono::steady_clock::now();
    }

    return edge;
}

// ============================================================================
// MCTGraphSnapshot
// ============================================================================

nlohmann::json MCTGraphSnapshot::toJson() const {
    auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();

    nlohmann::json words_json = nlohmann::json::array();
    for (const auto& w : word_nodes) {
        words_json.push_back(w.toJson());
    }

    nlohmann::json emotions_json = nlohmann::json::array();
    for (const auto& e : emotion_nodes) {
        emotions_json.push_back(e.toJson());
    }

    nlohmann::json edges_json = nlohmann::json::array();
    for (const auto& e : edges) {
        edges_json.push_back(e.toJson());
    }

    return {
        {"snapshot_id", snapshot_id},
        {"timestamp_ms", ts},
        {"word_nodes", words_json},
        {"emotion_nodes", emotions_json},
        {"edges", edges_json},
        {"statistics", {
            {"total_words", stats.total_words},
            {"total_emotions", stats.total_emotions},
            {"causal_edges", stats.causal_edges},
            {"temporal_edges", stats.temporal_edges},
            {"semantic_edges", stats.semantic_edges},
            {"average_emotion_intensity", stats.average_emotion_intensity},
            {"most_frequent_emotion", stats.most_frequent_emotion},
            {"top_trigger_words", stats.top_trigger_words},
            {"graph_density", stats.graph_density},
            {"time_span_seconds", stats.time_span_seconds}
        }}
    };
}

MCTGraphSnapshot MCTGraphSnapshot::fromJson(const nlohmann::json& j) {
    MCTGraphSnapshot snapshot;
    snapshot.snapshot_id = j.value("snapshot_id", "");

    if (j.contains("timestamp_ms")) {
        auto ms = j["timestamp_ms"].get<int64_t>();
        snapshot.timestamp = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(ms));
    }

    if (j.contains("word_nodes")) {
        for (const auto& w : j["word_nodes"]) {
            snapshot.word_nodes.push_back(WordNode::fromJson(w));
        }
    }

    if (j.contains("emotion_nodes")) {
        for (const auto& e : j["emotion_nodes"]) {
            snapshot.emotion_nodes.push_back(EmotionNode::fromJson(e));
        }
    }

    if (j.contains("edges")) {
        for (const auto& e : j["edges"]) {
            snapshot.edges.push_back(GraphEdge::fromJson(e));
        }
    }

    if (j.contains("statistics")) {
        const auto& s = j["statistics"];
        snapshot.stats.total_words = s.value("total_words", size_t(0));
        snapshot.stats.total_emotions = s.value("total_emotions", size_t(0));
        snapshot.stats.causal_edges = s.value("causal_edges", size_t(0));
        snapshot.stats.temporal_edges = s.value("temporal_edges", size_t(0));
        snapshot.stats.semantic_edges = s.value("semantic_edges", size_t(0));
        snapshot.stats.average_emotion_intensity = s.value("average_emotion_intensity", 0.0);
        snapshot.stats.most_frequent_emotion = s.value("most_frequent_emotion", "");
        snapshot.stats.graph_density = s.value("graph_density", 0.0);
        snapshot.stats.time_span_seconds = s.value("time_span_seconds", 0.0);

        if (s.contains("top_trigger_words")) {
            for (const auto& w : s["top_trigger_words"]) {
                snapshot.stats.top_trigger_words.push_back(w.get<std::string>());
            }
        }
    }

    return snapshot;
}

// ============================================================================
// MCTGraph - Constructeur
// ============================================================================

MCTGraph::MCTGraph(const MCTGraphConfig& config)
    : config_(config)
    , last_snapshot_time_(std::chrono::steady_clock::now()) {
}

// ============================================================================
// Génération d'ID
// ============================================================================

std::string MCTGraph::generateId(const std::string& prefix) const {
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    uint64_t count = id_counter_.fetch_add(1);

    std::ostringstream oss;
    oss << prefix << "_" << ms << "_" << count;
    return oss.str();
}

// ============================================================================
// Ajout de nœuds WORD
// ============================================================================

std::string MCTGraph::addWord(const std::string& lemma,
                               const std::string& pos,
                               const std::string& sentence_id,
                               const std::string& original_form) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Vérification limite de nœuds
    if (word_nodes_.size() + emotion_nodes_.size() >= config_.max_nodes) {
        pruneExpiredNodes();
    }

    WordNode node;
    node.id = generateId("WORD");
    node.lemma = lemma;
    node.pos = pos;
    node.sentence_id = sentence_id;
    node.original_form = original_form.empty() ? lemma : original_form;
    node.timestamp = std::chrono::steady_clock::now();

    // Détection automatique de négation/intensificateur
    static const std::vector<std::string> negations = {
        "ne", "pas", "jamais", "rien", "aucun", "personne", "non"
    };
    static const std::vector<std::string> intensifiers = {
        "très", "vraiment", "extrêmement", "tellement", "super", "trop"
    };

    std::string lower_lemma = lemma;
    std::transform(lower_lemma.begin(), lower_lemma.end(), lower_lemma.begin(), ::tolower);

    node.is_negation = std::find(negations.begin(), negations.end(), lower_lemma) != negations.end();
    node.is_intensifier = std::find(intensifiers.begin(), intensifiers.end(), lower_lemma) != intensifiers.end();

    word_nodes_[node.id] = node;
    adjacency_[node.id] = {};

    return node.id;
}

std::string MCTGraph::addWordFromJson(const nlohmann::json& word_data) {
    std::string lemma = word_data.value("lemma", word_data.value("text", ""));
    std::string pos = word_data.value("pos", "UNKNOWN");
    std::string sentence_id = word_data.value("sentence_id", "");
    std::string original = word_data.value("original", word_data.value("text", ""));

    std::string word_id = addWord(lemma, pos, sentence_id, original);

    // Mise à jour avec métadonnées supplémentaires si présentes
    if (word_data.contains("sentiment")) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (word_nodes_.count(word_id)) {
            word_nodes_[word_id].sentiment_score = word_data["sentiment"].get<double>();
        }
    }

    return word_id;
}

// ============================================================================
// Ajout de nœuds EMOTION
// ============================================================================

std::string MCTGraph::addEmotion(const EmotionalState& state,
                                  double persistence_duration) {
    // Vérification du seuil de persistance
    if (persistence_duration < config_.emotion_persistence_threshold_seconds) {
        return "";  // Émotion trop courte, non stockée
    }

    std::lock_guard<std::mutex> lock(mutex_);

    if (word_nodes_.size() + emotion_nodes_.size() >= config_.max_nodes) {
        pruneExpiredNodes();
    }

    EmotionNode node;
    node.id = generateId("EMO");
    node.emotions = state.emotions;
    node.intensity = state.E_global;
    node.dominant_emotion = findDominantEmotion(state.emotions);
    node.persistence_duration = persistence_duration;
    node.timestamp = state.timestamp;
    node.end_timestamp = state.timestamp + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(persistence_duration));

    emotion_nodes_[node.id] = node;
    adjacency_[node.id] = {};

    return node.id;
}

std::string MCTGraph::addEmotionWithContext(const EmotionalState& state,
                                             double persistence_duration,
                                             double valence,
                                             double arousal) {
    std::string emotion_id = addEmotion(state, persistence_duration);

    if (!emotion_id.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (emotion_nodes_.count(emotion_id)) {
            emotion_nodes_[emotion_id].valence = valence;
            emotion_nodes_[emotion_id].arousal = arousal;
        }
    }

    return emotion_id;
}

// ============================================================================
// Gestion des arêtes
// ============================================================================

std::string MCTGraph::addCausalEdge(const std::string& word_id,
                                     const std::string& emotion_id,
                                     double weight) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!word_nodes_.count(word_id) || !emotion_nodes_.count(emotion_id)) {
        return "";
    }

    const auto& word = word_nodes_.at(word_id);
    const auto& emotion = emotion_nodes_.at(emotion_id);

    GraphEdge edge;
    edge.id = generateId("EDGE");
    edge.source_id = word_id;
    edge.target_id = emotion_id;
    edge.type = EdgeType::CAUSAL;
    edge.created_at = std::chrono::steady_clock::now();

    // Calcul de la distance temporelle
    edge.temporal_distance_ms = calculateTemporalDistance(word.timestamp, emotion.timestamp);

    // Calcul du poids si non spécifié
    if (weight < 0) {
        edge.weight = calculateCausalWeight(word, emotion);
    } else {
        edge.weight = weight;
    }

    edges_[edge.id] = edge;
    updateAdjacency(edge.id, word_id, emotion_id);

    // Callback de détection causale
    if (causal_callback_) {
        causal_callback_(word_id, emotion_id, edge.weight);
    }

    return edge.id;
}

std::string MCTGraph::addTemporalEdge(const std::string& node1_id,
                                       const std::string& node2_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Vérifier que les nœuds existent
    bool node1_exists = word_nodes_.count(node1_id) || emotion_nodes_.count(node1_id);
    bool node2_exists = word_nodes_.count(node2_id) || emotion_nodes_.count(node2_id);

    if (!node1_exists || !node2_exists) {
        return "";
    }

    GraphEdge edge;
    edge.id = generateId("EDGE");
    edge.source_id = node1_id;
    edge.target_id = node2_id;
    edge.type = EdgeType::TEMPORAL;
    edge.weight = config_.initial_temporal_weight;
    edge.created_at = std::chrono::steady_clock::now();

    // Calcul de la distance temporelle
    std::chrono::steady_clock::time_point t1, t2;

    if (word_nodes_.count(node1_id)) {
        t1 = word_nodes_.at(node1_id).timestamp;
    } else {
        t1 = emotion_nodes_.at(node1_id).timestamp;
    }

    if (word_nodes_.count(node2_id)) {
        t2 = word_nodes_.at(node2_id).timestamp;
    } else {
        t2 = emotion_nodes_.at(node2_id).timestamp;
    }

    edge.temporal_distance_ms = calculateTemporalDistance(t1, t2);

    edges_[edge.id] = edge;
    updateAdjacency(edge.id, node1_id, node2_id);

    return edge.id;
}

std::string MCTGraph::addSemanticEdge(const std::string& word1_id,
                                       const std::string& word2_id,
                                       const std::string& relation_type,
                                       double weight) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!word_nodes_.count(word1_id) || !word_nodes_.count(word2_id)) {
        return "";
    }

    GraphEdge edge;
    edge.id = generateId("EDGE");
    edge.source_id = word1_id;
    edge.target_id = word2_id;
    edge.type = EdgeType::SEMANTIC;
    edge.weight = weight < 0 ? config_.initial_semantic_weight : weight;
    edge.semantic_relation = relation_type;
    edge.created_at = std::chrono::steady_clock::now();

    edges_[edge.id] = edge;
    updateAdjacency(edge.id, word1_id, word2_id);

    return edge.id;
}

// ============================================================================
// Détection automatique de causalité
// ============================================================================

void MCTGraph::detectCausality(const std::string& emotion_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!emotion_nodes_.count(emotion_id)) {
        return;
    }

    const auto& emotion = emotion_nodes_.at(emotion_id);

    // Parcourir tous les mots et vérifier la fenêtre de causalité
    for (const auto& [word_id, word] : word_nodes_) {
        if (isWithinCausalityWindow(word, emotion)) {
            // Créer l'arête causale (sans lock car déjà verrouillé)
            GraphEdge edge;
            edge.id = generateId("EDGE");
            edge.source_id = word_id;
            edge.target_id = emotion_id;
            edge.type = EdgeType::CAUSAL;
            edge.created_at = std::chrono::steady_clock::now();
            edge.temporal_distance_ms = calculateTemporalDistance(word.timestamp, emotion.timestamp);
            edge.weight = calculateCausalWeight(word, emotion);

            edges_[edge.id] = edge;
            updateAdjacency(edge.id, word_id, emotion_id);

            if (causal_callback_) {
                causal_callback_(word_id, emotion_id, edge.weight);
            }
        }
    }
}

void MCTGraph::detectTemporalCooccurrences(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::chrono::steady_clock::time_point node_time;
    bool is_word = word_nodes_.count(node_id) > 0;

    if (is_word) {
        node_time = word_nodes_.at(node_id).timestamp;
    } else if (emotion_nodes_.count(node_id)) {
        node_time = emotion_nodes_.at(node_id).timestamp;
    } else {
        return;
    }

    // Fenêtre de co-occurrence : 2 secondes
    const double cooccurrence_window_ms = 2000.0;

    // Vérifier les mots
    for (const auto& [other_id, word] : word_nodes_) {
        if (other_id == node_id) continue;

        double distance = calculateTemporalDistance(node_time, word.timestamp);
        if (distance <= cooccurrence_window_ms) {
            // Vérifier si l'arête existe déjà
            bool exists = false;
            for (const auto& edge_id : adjacency_[node_id]) {
                const auto& edge = edges_.at(edge_id);
                if ((edge.source_id == other_id || edge.target_id == other_id) &&
                    edge.type == EdgeType::TEMPORAL) {
                    exists = true;
                    break;
                }
            }

            if (!exists) {
                GraphEdge edge;
                edge.id = generateId("EDGE");
                edge.source_id = node_id;
                edge.target_id = other_id;
                edge.type = EdgeType::TEMPORAL;
                edge.weight = config_.initial_temporal_weight;
                edge.temporal_distance_ms = distance;
                edge.created_at = std::chrono::steady_clock::now();

                edges_[edge.id] = edge;
                updateAdjacency(edge.id, node_id, other_id);
            }
        }
    }

    // Vérifier les émotions
    for (const auto& [other_id, emotion] : emotion_nodes_) {
        if (other_id == node_id) continue;

        double distance = calculateTemporalDistance(node_time, emotion.timestamp);
        if (distance <= cooccurrence_window_ms) {
            bool exists = false;
            for (const auto& edge_id : adjacency_[node_id]) {
                const auto& edge = edges_.at(edge_id);
                if ((edge.source_id == other_id || edge.target_id == other_id) &&
                    edge.type == EdgeType::TEMPORAL) {
                    exists = true;
                    break;
                }
            }

            if (!exists) {
                GraphEdge edge;
                edge.id = generateId("EDGE");
                edge.source_id = node_id;
                edge.target_id = other_id;
                edge.type = EdgeType::TEMPORAL;
                edge.weight = config_.initial_temporal_weight;
                edge.temporal_distance_ms = distance;
                edge.created_at = std::chrono::steady_clock::now();

                edges_[edge.id] = edge;
                updateAdjacency(edge.id, node_id, other_id);
            }
        }
    }
}

// ============================================================================
// Requêtes sur le graphe
// ============================================================================

std::vector<WordNode> MCTGraph::getTriggerWords(const std::string& emotion_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<WordNode> result;

    if (!adjacency_.count(emotion_id)) {
        return result;
    }

    for (const auto& edge_id : adjacency_.at(emotion_id)) {
        const auto& edge = edges_.at(edge_id);
        if (edge.type == EdgeType::CAUSAL && edge.target_id == emotion_id) {
            if (word_nodes_.count(edge.source_id)) {
                result.push_back(word_nodes_.at(edge.source_id));
            }
        }
    }

    return result;
}

std::vector<EmotionNode> MCTGraph::getTriggeredEmotions(const std::string& word_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<EmotionNode> result;

    if (!adjacency_.count(word_id)) {
        return result;
    }

    for (const auto& edge_id : adjacency_.at(word_id)) {
        const auto& edge = edges_.at(edge_id);
        if (edge.type == EdgeType::CAUSAL && edge.source_id == word_id) {
            if (emotion_nodes_.count(edge.target_id)) {
                result.push_back(emotion_nodes_.at(edge.target_id));
            }
        }
    }

    return result;
}

std::vector<CausalAnalysis> MCTGraph::analyzeCausality() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_map<std::string, CausalAnalysis> analysis_map;

    for (const auto& [edge_id, edge] : edges_) {
        if (edge.type != EdgeType::CAUSAL) continue;

        if (!analysis_map.count(edge.source_id)) {
            CausalAnalysis ca;
            ca.word_id = edge.source_id;
            if (word_nodes_.count(edge.source_id)) {
                ca.word_lemma = word_nodes_.at(edge.source_id).lemma;
            }
            ca.causal_strength = 0.0;
            ca.trigger_count = 0;
            analysis_map[edge.source_id] = ca;
        }

        analysis_map[edge.source_id].triggered_emotion_ids.push_back(edge.target_id);
        analysis_map[edge.source_id].causal_strength += edge.weight;
        analysis_map[edge.source_id].trigger_count++;
    }

    std::vector<CausalAnalysis> result;
    for (auto& [id, ca] : analysis_map) {
        // Normaliser la force causale
        if (ca.trigger_count > 0) {
            ca.causal_strength /= ca.trigger_count;
        }
        result.push_back(ca);
    }

    // Trier par force causale décroissante
    std::sort(result.begin(), result.end(),
              [](const CausalAnalysis& a, const CausalAnalysis& b) {
                  return a.causal_strength > b.causal_strength;
              });

    return result;
}

std::vector<std::string> MCTGraph::getNeighbors(const std::string& node_id,
                                                 std::optional<EdgeType> edge_type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> result;

    if (!adjacency_.count(node_id)) {
        return result;
    }

    for (const auto& edge_id : adjacency_.at(node_id)) {
        const auto& edge = edges_.at(edge_id);

        if (edge_type && edge.type != *edge_type) {
            continue;
        }

        std::string neighbor_id = (edge.source_id == node_id) ? edge.target_id : edge.source_id;
        result.push_back(neighbor_id);
    }

    return result;
}

bool MCTGraph::hasNode(const std::string& node_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return word_nodes_.count(node_id) || emotion_nodes_.count(node_id);
}

std::optional<WordNode> MCTGraph::getWordNode(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (word_nodes_.count(id)) {
        return word_nodes_.at(id);
    }
    return std::nullopt;
}

std::optional<EmotionNode> MCTGraph::getEmotionNode(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (emotion_nodes_.count(id)) {
        return emotion_nodes_.at(id);
    }
    return std::nullopt;
}

// ============================================================================
// Maintenance du graphe
// ============================================================================

size_t MCTGraph::pruneExpiredNodes() {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::steady_clock::now();
    auto window = std::chrono::duration<double>(config_.time_window_seconds);
    size_t removed = 0;

    // Collecter les IDs à supprimer
    std::vector<std::string> words_to_remove;
    std::vector<std::string> emotions_to_remove;

    for (const auto& [id, node] : word_nodes_) {
        if (now - node.timestamp > window) {
            words_to_remove.push_back(id);
        }
    }

    for (const auto& [id, node] : emotion_nodes_) {
        if (now - node.timestamp > window) {
            emotions_to_remove.push_back(id);
        }
    }

    // Supprimer les nœuds et leurs arêtes
    for (const auto& id : words_to_remove) {
        removeEdgesForNode(id);
        word_nodes_.erase(id);
        adjacency_.erase(id);
        removed++;
    }

    for (const auto& id : emotions_to_remove) {
        removeEdgesForNode(id);
        emotion_nodes_.erase(id);
        adjacency_.erase(id);
        removed++;
    }

    return removed;
}

void MCTGraph::applyEdgeDecay() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& [id, edge] : edges_) {
        edge.weight *= config_.edge_decay_factor;

        // Supprimer les arêtes avec un poids trop faible
        if (edge.weight < 0.01) {
            adjacency_[edge.source_id].erase(
                std::remove(adjacency_[edge.source_id].begin(),
                           adjacency_[edge.source_id].end(), id),
                adjacency_[edge.source_id].end());

            adjacency_[edge.target_id].erase(
                std::remove(adjacency_[edge.target_id].begin(),
                           adjacency_[edge.target_id].end(), id),
                adjacency_[edge.target_id].end());
        }
    }

    // Supprimer les arêtes avec poids < 0.01
    std::erase_if(edges_, [](const auto& pair) {
        return pair.second.weight < 0.01;
    });
}

void MCTGraph::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    word_nodes_.clear();
    emotion_nodes_.clear();
    edges_.clear();
    adjacency_.clear();
}

// ============================================================================
// Export et snapshots
// ============================================================================

MCTGraphSnapshot MCTGraph::createSnapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);

    MCTGraphSnapshot snapshot;
    snapshot.snapshot_id = generateId("SNAP");
    snapshot.timestamp = std::chrono::system_clock::now();

    // Copier les nœuds
    for (const auto& [id, node] : word_nodes_) {
        snapshot.word_nodes.push_back(node);
    }

    for (const auto& [id, node] : emotion_nodes_) {
        snapshot.emotion_nodes.push_back(node);
    }

    // Copier les arêtes
    for (const auto& [id, edge] : edges_) {
        snapshot.edges.push_back(edge);
    }

    // Calculer les statistiques
    computeSnapshotStatistics(snapshot);

    return snapshot;
}

nlohmann::json MCTGraph::toJson() const {
    std::lock_guard<std::mutex> lock(mutex_);

    nlohmann::json words_json = nlohmann::json::array();
    for (const auto& [id, node] : word_nodes_) {
        words_json.push_back(node.toJson());
    }

    nlohmann::json emotions_json = nlohmann::json::array();
    for (const auto& [id, node] : emotion_nodes_) {
        emotions_json.push_back(node.toJson());
    }

    nlohmann::json edges_json = nlohmann::json::array();
    for (const auto& [id, edge] : edges_) {
        edges_json.push_back(edge.toJson());
    }

    return {
        {"word_nodes", words_json},
        {"emotion_nodes", emotions_json},
        {"edges", edges_json},
        {"config", {
            {"time_window_seconds", config_.time_window_seconds},
            {"emotion_persistence_threshold_seconds", config_.emotion_persistence_threshold_seconds},
            {"causality_threshold_ms", config_.causality_threshold_ms},
            {"snapshot_interval_seconds", config_.snapshot_interval_seconds}
        }}
    };
}

void MCTGraph::loadFromJson(const nlohmann::json& j) {
    std::lock_guard<std::mutex> lock(mutex_);

    clear();

    if (j.contains("word_nodes")) {
        for (const auto& w : j["word_nodes"]) {
            WordNode node = WordNode::fromJson(w);
            word_nodes_[node.id] = node;
            adjacency_[node.id] = {};
        }
    }

    if (j.contains("emotion_nodes")) {
        for (const auto& e : j["emotion_nodes"]) {
            EmotionNode node = EmotionNode::fromJson(e);
            emotion_nodes_[node.id] = node;
            adjacency_[node.id] = {};
        }
    }

    if (j.contains("edges")) {
        for (const auto& e : j["edges"]) {
            GraphEdge edge = GraphEdge::fromJson(e);
            edges_[edge.id] = edge;
            updateAdjacency(edge.id, edge.source_id, edge.target_id);
        }
    }
}

// ============================================================================
// Callbacks
// ============================================================================

void MCTGraph::setSnapshotCallback(SnapshotCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_callback_ = std::move(callback);
}

void MCTGraph::setCausalDetectionCallback(CausalDetectionCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    causal_callback_ = std::move(callback);
}

void MCTGraph::triggerSnapshot() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - last_snapshot_time_).count();

    if (elapsed >= config_.snapshot_interval_seconds) {
        MCTGraphSnapshot snapshot = createSnapshot();

        if (snapshot_callback_) {
            snapshot_callback_(snapshot);
        }

        last_snapshot_time_ = now;
    }
}

// ============================================================================
// Statistiques
// ============================================================================

size_t MCTGraph::getWordCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return word_nodes_.size();
}

size_t MCTGraph::getEmotionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return emotion_nodes_.size();
}

size_t MCTGraph::getEdgeCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return edges_.size();
}

size_t MCTGraph::getCausalEdgeCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t count = 0;
    for (const auto& [id, edge] : edges_) {
        if (edge.type == EdgeType::CAUSAL) {
            count++;
        }
    }
    return count;
}

double MCTGraph::getGraphDensity() const {
    std::lock_guard<std::mutex> lock(mutex_);

    size_t n = word_nodes_.size() + emotion_nodes_.size();
    if (n < 2) return 0.0;

    size_t max_edges = n * (n - 1) / 2;
    return static_cast<double>(edges_.size()) / max_edges;
}

// ============================================================================
// Méthodes privées
// ============================================================================

double MCTGraph::calculateCausalWeight(const WordNode& word,
                                        const EmotionNode& emotion) const {
    double distance_ms = calculateTemporalDistance(word.timestamp, emotion.timestamp);
    double threshold = emotion.isSlowEmotion()
        ? config_.slow_emotion_causality_threshold_ms
        : config_.causality_threshold_ms;

    // Poids inversement proportionnel à la distance temporelle
    double temporal_factor = 1.0 - (distance_ms / threshold);
    temporal_factor = std::max(0.0, temporal_factor);

    // Bonus pour les intensificateurs
    double modifier = 1.0;
    if (word.is_intensifier) {
        modifier = 1.3;
    } else if (word.is_negation) {
        modifier = 0.7;  // Les négations réduisent la causalité directe
    }

    // Intégrer le sentiment du mot si disponible
    double sentiment_factor = 1.0;
    if (std::abs(word.sentiment_score) > 0.1) {
        sentiment_factor = 1.0 + std::abs(word.sentiment_score) * 0.2;
    }

    return config_.initial_causal_weight * temporal_factor * modifier * sentiment_factor;
}

double MCTGraph::calculateTemporalDistance(
    const std::chrono::steady_clock::time_point& t1,
    const std::chrono::steady_clock::time_point& t2) const {
    auto diff = t1 > t2 ? t1 - t2 : t2 - t1;
    return std::chrono::duration<double, std::milli>(diff).count();
}

bool MCTGraph::isWithinCausalityWindow(const WordNode& word,
                                        const EmotionNode& emotion) const {
    // Le mot doit précéder l'émotion
    if (word.timestamp > emotion.timestamp) {
        return false;
    }

    double distance_ms = calculateTemporalDistance(word.timestamp, emotion.timestamp);
    double threshold = emotion.isSlowEmotion()
        ? config_.slow_emotion_causality_threshold_ms
        : config_.causality_threshold_ms;

    return distance_ms <= threshold;
}

void MCTGraph::removeEdgesForNode(const std::string& node_id) {
    // Collecter les IDs d'arêtes à supprimer
    std::vector<std::string> edges_to_remove;

    for (const auto& [edge_id, edge] : edges_) {
        if (edge.source_id == node_id || edge.target_id == node_id) {
            edges_to_remove.push_back(edge_id);
        }
    }

    // Supprimer les arêtes et mettre à jour l'adjacence
    for (const auto& edge_id : edges_to_remove) {
        const auto& edge = edges_.at(edge_id);

        // Retirer de l'adjacence de l'autre nœud
        std::string other_id = (edge.source_id == node_id) ? edge.target_id : edge.source_id;
        if (adjacency_.count(other_id)) {
            adjacency_[other_id].erase(
                std::remove(adjacency_[other_id].begin(),
                           adjacency_[other_id].end(), edge_id),
                adjacency_[other_id].end());
        }

        edges_.erase(edge_id);
    }
}

void MCTGraph::updateAdjacency(const std::string& edge_id,
                                const std::string& source_id,
                                const std::string& target_id) {
    if (adjacency_.count(source_id)) {
        adjacency_[source_id].push_back(edge_id);
    }
    if (adjacency_.count(target_id)) {
        adjacency_[target_id].push_back(edge_id);
    }
}

void MCTGraph::computeSnapshotStatistics(MCTGraphSnapshot& snapshot) const {
    auto& stats = snapshot.stats;

    stats.total_words = snapshot.word_nodes.size();
    stats.total_emotions = snapshot.emotion_nodes.size();

    // Comptage des types d'arêtes
    for (const auto& edge : snapshot.edges) {
        switch (edge.type) {
            case EdgeType::CAUSAL: stats.causal_edges++; break;
            case EdgeType::TEMPORAL: stats.temporal_edges++; break;
            case EdgeType::SEMANTIC: stats.semantic_edges++; break;
        }
    }

    // Intensité moyenne des émotions
    if (!snapshot.emotion_nodes.empty()) {
        double total_intensity = 0.0;
        std::unordered_map<std::string, int> emotion_counts;

        for (const auto& emo : snapshot.emotion_nodes) {
            total_intensity += emo.intensity;
            emotion_counts[emo.dominant_emotion]++;
        }

        stats.average_emotion_intensity = total_intensity / snapshot.emotion_nodes.size();

        // Émotion la plus fréquente
        int max_count = 0;
        for (const auto& [name, count] : emotion_counts) {
            if (count > max_count) {
                max_count = count;
                stats.most_frequent_emotion = name;
            }
        }
    }

    // Top mots déclencheurs
    std::unordered_map<std::string, int> word_trigger_counts;
    for (const auto& edge : snapshot.edges) {
        if (edge.type == EdgeType::CAUSAL) {
            for (const auto& word : snapshot.word_nodes) {
                if (word.id == edge.source_id) {
                    word_trigger_counts[word.lemma]++;
                    break;
                }
            }
        }
    }

    std::vector<std::pair<std::string, int>> sorted_triggers(
        word_trigger_counts.begin(), word_trigger_counts.end());
    std::sort(sorted_triggers.begin(), sorted_triggers.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    for (size_t i = 0; i < std::min(size_t(5), sorted_triggers.size()); ++i) {
        stats.top_trigger_words.push_back(sorted_triggers[i].first);
    }

    // Densité du graphe
    size_t n = stats.total_words + stats.total_emotions;
    if (n >= 2) {
        size_t max_edges = n * (n - 1) / 2;
        stats.graph_density = static_cast<double>(snapshot.edges.size()) / max_edges;
    }

    // Étendue temporelle
    if (!snapshot.word_nodes.empty() || !snapshot.emotion_nodes.empty()) {
        auto min_time = std::chrono::steady_clock::time_point::max();
        auto max_time = std::chrono::steady_clock::time_point::min();

        for (const auto& word : snapshot.word_nodes) {
            min_time = std::min(min_time, word.timestamp);
            max_time = std::max(max_time, word.timestamp);
        }

        for (const auto& emo : snapshot.emotion_nodes) {
            min_time = std::min(min_time, emo.timestamp);
            max_time = std::max(max_time, emo.timestamp);
        }

        if (min_time < max_time) {
            stats.time_span_seconds = std::chrono::duration<double>(max_time - min_time).count();
        }
    }
}

std::string MCTGraph::findDominantEmotion(const std::array<double, 24>& emotions) const {
    size_t max_idx = 0;
    double max_val = emotions[0];

    for (size_t i = 1; i < 24; ++i) {
        if (emotions[i] > max_val) {
            max_val = emotions[i];
            max_idx = i;
        }
    }

    // Utiliser EMOTION_NAMES de Types.hpp
    if (max_idx < EMOTION_NAMES.size()) {
        return std::string(EMOTION_NAMES[max_idx]);
    }

    return "unknown";
}

} // namespace mcee
