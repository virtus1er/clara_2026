/**
 * @file Neo4jClient.cpp
 * @brief Implémentation du client Neo4j via RabbitMQ
 * @version 1.0
 * @date 2025-12-21
 */

#include "Neo4jClient.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>

namespace mcee {

Neo4jClient::Neo4jClient(const Neo4jClientConfig& config)
    : config_(config) {
    std::cout << "[Neo4jClient] Client initialisé (host=" << config_.rabbitmq_host
              << ", queue=" << config_.request_queue << ")\n";
}

Neo4jClient::~Neo4jClient() {
    disconnect();
}

bool Neo4jClient::connect() {
    if (connected_.load()) {
        return true;
    }

    try {
        // Créer le canal RabbitMQ avec Channel::Open (méthode recommandée)
        AmqpClient::Channel::OpenOpts opts;
        opts.host = config_.rabbitmq_host;
        opts.port = config_.rabbitmq_port;
        opts.auth = AmqpClient::Channel::OpenOpts::BasicAuth{
            config_.rabbitmq_user,
            config_.rabbitmq_password
        };

        // Timeout de connexion court pour ne pas bloquer en mode démo
        opts.frame_max = 131072;

        channel_ = AmqpClient::Channel::Open(opts);

        if (!channel_) {
            std::cerr << "[Neo4jClient] Échec création du canal RabbitMQ\n";
            return false;
        }

        // Déclarer la queue de requêtes
        channel_->DeclareQueue(config_.request_queue, false, true, false, false);

        // Déclarer l'exchange de réponses
        channel_->DeclareExchange(config_.response_exchange, "direct", false, true);

        // Créer une queue temporaire pour les réponses
        response_queue_ = channel_->DeclareQueue("");
        channel_->BindQueue(response_queue_, config_.response_exchange, response_queue_);

        // Démarrer le consommateur de réponses
        consumer_tag_ = channel_->BasicConsume(response_queue_, "", true, false);

        connected_.store(true);
        running_.store(true);

        // Lancer le thread de consommation des réponses
        response_thread_ = std::thread(&Neo4jClient::responseConsumerLoop, this);

        std::cout << "[Neo4jClient] Connecté à RabbitMQ (response_queue=" << response_queue_ << ")\n";
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[Neo4jClient] Erreur connexion: " << e.what() << "\n";
        return false;
    }
}

void Neo4jClient::disconnect() {
    if (!connected_.load()) {
        return;
    }

    running_.store(false);
    connected_.store(false);

    // Réveiller les threads en attente
    sync_cv_.notify_all();

    // Attendre le thread de réponse
    if (response_thread_.joinable()) {
        response_thread_.join();
    }

    try {
        if (channel_) {
            channel_->BasicCancel(consumer_tag_);
        }
    } catch (...) {}

    channel_.reset();
    std::cout << "[Neo4jClient] Déconnecté\n";
}

std::string Neo4jClient::generateRequestId() {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ).count();
    uint64_t count = request_counter_++;

    std::ostringstream oss;
    oss << "MCEE_" << ms << "_" << count;
    return oss.str();
}

std::string Neo4jClient::sendRequest(
    const std::string& request_type,
    const json& payload,
    Neo4jCallback callback)
{
    if (!connected_.load()) {
        std::cerr << "[Neo4jClient] Non connecté, requête ignorée\n";
        return "";
    }

    std::string request_id = generateRequestId();

    // Construire le message
    json request = {
        {"request_id", request_id},
        {"request_type", request_type},
        {"payload", payload},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
    };

    std::string body = request.dump();

    try {
        // Enregistrer le callback si fourni
        if (callback) {
            std::lock_guard<std::mutex> lock(callbacks_mutex_);
            pending_callbacks_[request_id] = callback;
        }

        // Publier la requête
        AmqpClient::BasicMessage::ptr_t message = AmqpClient::BasicMessage::Create(body);
        message->ContentType("application/json");
        message->ReplyTo(response_queue_);
        message->CorrelationId(request_id);

        channel_->BasicPublish("", config_.request_queue, message);

        return request_id;

    } catch (const std::exception& e) {
        std::cerr << "[Neo4jClient] Erreur envoi requête: " << e.what() << "\n";

        // Retirer le callback en cas d'erreur
        if (callback) {
            std::lock_guard<std::mutex> lock(callbacks_mutex_);
            pending_callbacks_.erase(request_id);
        }
        return "";
    }
}

Neo4jResponse Neo4jClient::waitForResponse(const std::string& request_id) {
    std::unique_lock<std::mutex> lock(sync_mutex_);

    // Attendre la réponse avec timeout
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(config_.request_timeout_ms);

    while (running_.load()) {
        auto it = sync_responses_.find(request_id);
        if (it != sync_responses_.end()) {
            Neo4jResponse response = std::move(it->second);
            sync_responses_.erase(it);
            return response;
        }

        if (sync_cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            return Neo4jResponse{request_id, false, {}, "Timeout", 0};
        }
    }

    return Neo4jResponse{request_id, false, {}, "Disconnected", 0};
}

void Neo4jClient::responseConsumerLoop() {
    while (running_.load()) {
        try {
            AmqpClient::Envelope::ptr_t envelope;

            // Consommer avec timeout
            if (channel_->BasicConsumeMessage(consumer_tag_, envelope, 100)) {
                std::string body = envelope->Message()->Body();
                json response_json = json::parse(body);

                Neo4jResponse response;
                response.request_id = response_json.value("request_id", "");
                response.success = response_json.value("success", false);
                response.data = response_json.value("data", json{});
                response.error = response_json.value("error", "");
                response.execution_time_ms = response_json.value("execution_time_ms", 0.0);

                // Vérifier si un callback attend cette réponse
                {
                    std::lock_guard<std::mutex> lock(callbacks_mutex_);
                    auto it = pending_callbacks_.find(response.request_id);
                    if (it != pending_callbacks_.end()) {
                        // Appeler le callback
                        it->second(response);
                        pending_callbacks_.erase(it);
                        continue;
                    }
                }

                // Sinon, stocker pour une attente synchrone
                {
                    std::lock_guard<std::mutex> lock(sync_mutex_);
                    sync_responses_[response.request_id] = response;
                }
                sync_cv_.notify_all();

                channel_->BasicAck(envelope);
            }

        } catch (const std::exception& e) {
            if (running_.load()) {
                std::cerr << "[Neo4jClient] Erreur consommation: " << e.what() << "\n";
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// CONVERSIONS JSON
// ═══════════════════════════════════════════════════════════════════════════

json Neo4jClient::memoryToJson(const Memory& memory) const {
    std::vector<double> emotions_vec(memory.emotions.begin(), memory.emotions.end());

    return {
        {"id", memory.name},
        {"emotions", emotions_vec},
        {"dominant", memory.dominant},
        {"intensity", memory.intensity},
        {"valence", memory.valence},
        {"weight", memory.weight},
        {"pattern", phaseToString(memory.phase_at_creation)},
        {"trauma", memory.is_trauma}
    };
}

Memory Neo4jClient::jsonToMemory(const json& j) const {
    Memory memory;
    memory.name = j.value("id", "");
    memory.dominant = j.value("dominant", "");
    memory.intensity = j.value("intensity", 0.0);
    memory.valence = j.value("valence", 0.5);
    memory.weight = j.value("weight", 0.5);
    memory.is_trauma = j.value("trauma", false);
    memory.activation_count = j.value("activation_count", 0);

    if (j.contains("emotions") && j["emotions"].is_array()) {
        auto& emotions_arr = j["emotions"];
        for (size_t i = 0; i < std::min(emotions_arr.size(), (size_t)NUM_EMOTIONS); ++i) {
            memory.emotions[i] = emotions_arr[i].get<double>();
        }
    }

    std::string pattern_str = j.value("pattern", "SERENITE");
    memory.phase_at_creation = stringToPhase(pattern_str);

    return memory;
}

json Neo4jClient::stateToJson(const EmotionalState& state) const {
    std::vector<double> emotions_vec(state.emotions.begin(), state.emotions.end());
    auto [dominant_name, dominant_value] = state.getDominant();

    return {
        {"emotions", emotions_vec},
        {"dominant", dominant_name},
        {"valence", state.getValence()},
        {"intensity", state.getMeanIntensity()}
    };
}

// ═══════════════════════════════════════════════════════════════════════════
// OPÉRATIONS MÉMOIRE
// ═══════════════════════════════════════════════════════════════════════════

std::string Neo4jClient::createMemory(
    const Memory& memory,
    const std::string& context,
    Neo4jCallback callback)
{
    json payload = memoryToJson(memory);
    payload["context"] = context;

    if (config_.async_mode && callback) {
        return sendRequest("create_memory", payload, callback);
    } else {
        std::string request_id = sendRequest("create_memory", payload);
        if (request_id.empty()) return "";

        Neo4jResponse response = waitForResponse(request_id);
        if (response.success && response.data.contains("id")) {
            std::cout << "[Neo4jClient] Souvenir créé: " << response.data["id"] << "\n";
            return response.data["id"].get<std::string>();
        }

        std::cerr << "[Neo4jClient] Erreur création souvenir: " << response.error << "\n";
        return "";
    }
}

std::string Neo4jClient::createTrauma(
    const Memory& memory,
    const std::vector<std::string>& trigger_keywords,
    Neo4jCallback callback)
{
    json payload = memoryToJson(memory);
    payload["trigger_keywords"] = trigger_keywords;

    if (config_.async_mode && callback) {
        return sendRequest("create_trauma", payload, callback);
    } else {
        std::string request_id = sendRequest("create_trauma", payload);
        if (request_id.empty()) return "";

        Neo4jResponse response = waitForResponse(request_id);
        if (response.success && response.data.contains("id")) {
            std::cout << "[Neo4jClient] Trauma créé: " << response.data["id"] << "\n";
            return response.data["id"].get<std::string>();
        }

        std::cerr << "[Neo4jClient] Erreur création trauma: " << response.error << "\n";
        return "";
    }
}

void Neo4jClient::mergeMemory(
    const std::string& target_id,
    const std::array<double, NUM_EMOTIONS>& emotions,
    double transfer_weight,
    Neo4jCallback callback)
{
    std::vector<double> emotions_vec(emotions.begin(), emotions.end());
    json payload = {
        {"target_id", target_id},
        {"emotions", emotions_vec},
        {"transfer_weight", transfer_weight}
    };

    sendRequest("merge_memory", payload, callback);
}

std::optional<Memory> Neo4jClient::getMemory(const std::string& memory_id) {
    json payload = {{"id", memory_id}};

    std::string request_id = sendRequest("get_memory", payload);
    if (request_id.empty()) return std::nullopt;

    Neo4jResponse response = waitForResponse(request_id);
    if (response.success && !response.data.is_null()) {
        return jsonToMemory(response.data);
    }

    return std::nullopt;
}

std::vector<std::pair<std::string, double>> Neo4jClient::findSimilarMemories(
    const std::array<double, NUM_EMOTIONS>& emotions,
    double threshold,
    size_t limit)
{
    std::vector<double> emotions_vec(emotions.begin(), emotions.end());
    json payload = {
        {"emotions", emotions_vec},
        {"threshold", threshold},
        {"limit", limit}
    };

    std::string request_id = sendRequest("find_similar", payload);
    if (request_id.empty()) return {};

    Neo4jResponse response = waitForResponse(request_id);
    std::vector<std::pair<std::string, double>> results;

    if (response.success && response.data.is_array()) {
        for (const auto& item : response.data) {
            std::string id = item.value("id", "");
            double similarity = item.value("similarity", 0.0);
            results.emplace_back(id, similarity);
        }
    }

    return results;
}

void Neo4jClient::reactivateMemory(
    const std::string& memory_id,
    double strength,
    Neo4jCallback callback)
{
    json payload = {
        {"id", memory_id},
        {"strength", strength}
    };

    sendRequest("reactivate", payload, callback);
}

void Neo4jClient::applyDecay(double elapsed_hours, Neo4jCallback callback) {
    json payload = {{"elapsed_hours", elapsed_hours}};
    sendRequest("apply_decay", payload, callback);
}

void Neo4jClient::deleteMemory(
    const std::string& memory_id,
    bool archive,
    Neo4jCallback callback)
{
    json payload = {
        {"id", memory_id},
        {"archive", archive}
    };

    sendRequest("delete_memory", payload, callback);
}

// ═══════════════════════════════════════════════════════════════════════════
// OPÉRATIONS PATTERNS
// ═══════════════════════════════════════════════════════════════════════════

void Neo4jClient::recordPatternTransition(
    const std::string& from_pattern,
    const std::string& to_pattern,
    double duration_s,
    const std::string& trigger,
    Neo4jCallback callback)
{
    json payload = {
        {"from", from_pattern},
        {"to", to_pattern},
        {"duration_s", duration_s},
        {"trigger", trigger}
    };

    sendRequest("record_transition", payload, callback);
}

std::vector<std::pair<std::string, double>> Neo4jClient::getPatternTransitions(
    const std::string& from_pattern)
{
    json payload = {{"from", from_pattern}};

    std::string request_id = sendRequest("get_transitions", payload);
    if (request_id.empty()) return {};

    Neo4jResponse response = waitForResponse(request_id);
    std::vector<std::pair<std::string, double>> results;

    if (response.success && response.data.is_array()) {
        for (const auto& item : response.data) {
            std::string to_pattern = item.value("to_pattern", "");
            double probability = item.value("probability", 0.0);
            results.emplace_back(to_pattern, probability);
        }
    }

    return results;
}

// ═══════════════════════════════════════════════════════════════════════════
// OPÉRATIONS SESSION
// ═══════════════════════════════════════════════════════════════════════════

std::string Neo4jClient::createSession(const std::string& pattern) {
    json payload = {{"pattern", pattern}};

    std::string request_id = sendRequest("create_session", payload);
    if (request_id.empty()) return "";

    Neo4jResponse response = waitForResponse(request_id);
    if (response.success && response.data.contains("id")) {
        std::cout << "[Neo4jClient] Session créée: " << response.data["id"] << "\n";
        return response.data["id"].get<std::string>();
    }

    return "";
}

void Neo4jClient::updateSession(
    const std::string& session_id,
    const EmotionalState& state,
    Neo4jCallback callback)
{
    json payload = {
        {"id", session_id},
        {"emotional_state", stateToJson(state)}
    };

    sendRequest("update_session", payload, callback);
}

// ═══════════════════════════════════════════════════════════════════════════
// REQUÊTES GÉNÉRIQUES
// ═══════════════════════════════════════════════════════════════════════════

json Neo4jClient::executeCypher(const std::string& query, const json& params) {
    json payload = {
        {"query", query},
        {"params", params}
    };

    std::string request_id = sendRequest("cypher_query", payload);
    if (request_id.empty()) return {};

    Neo4jResponse response = waitForResponse(request_id);
    if (response.success) {
        return response.data;
    }

    std::cerr << "[Neo4jClient] Erreur Cypher: " << response.error << "\n";
    return {};
}

} // namespace mcee
