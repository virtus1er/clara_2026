/**
 * @file LLMClient.cpp
 * @brief Implémentation du client LLM pour reformulation émotionnelle
 * @version 1.0
 * @date 2024-12
 */

#include "LLMClient.hpp"
#include <curl/curl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <cmath>

namespace mcee {

// ═══════════════════════════════════════════════════════════════════════════
// HELPERS CURL
// ═══════════════════════════════════════════════════════════════════════════

namespace {

/**
 * @brief Callback pour recevoir les données CURL
 */
size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t realsize = size * nmemb;
    userp->append(static_cast<char*>(contents), realsize);
    return realsize;
}

/**
 * @brief Initialise CURL globalement (thread-safe)
 */
class CurlGlobalInit {
public:
    CurlGlobalInit() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    ~CurlGlobalInit() {
        curl_global_cleanup();
    }
};

static CurlGlobalInit curlInit;

} // namespace anonyme

// ═══════════════════════════════════════════════════════════════════════════
// CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════

bool LLMClientConfig::loadFromJson(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }

        json j;
        file >> j;

        if (j.contains("llm_client")) {
            const auto& llm = j["llm_client"];

            if (llm.contains("mode")) {
                std::string mode_str = llm["mode"];
                mode = (mode_str == "direct_http") ? LLMMode::DIRECT_HTTP : LLMMode::RABBITMQ;
            }

            if (llm.contains("provider")) {
                std::string prov = llm["provider"];
                if (prov == "anthropic") provider = LLMProvider::ANTHROPIC;
                else if (prov == "local") provider = LLMProvider::LOCAL;
                else provider = LLMProvider::OPENAI;
            }

            if (llm.contains("openai")) {
                const auto& oa = llm["openai"];
                if (oa.contains("model")) model = oa["model"];
                if (oa.contains("temperature")) temperature = oa["temperature"];
                if (oa.contains("max_tokens")) max_tokens = oa["max_tokens"];
                if (oa.contains("timeout_seconds")) timeout_seconds = oa["timeout_seconds"];
            }

            if (llm.contains("rabbitmq")) {
                const auto& rmq = llm["rabbitmq"];
                if (rmq.contains("host")) rabbitmq_host = rmq["host"];
                if (rmq.contains("port")) rabbitmq_port = rmq["port"];
                if (rmq.contains("request_exchange")) request_exchange = rmq["request_exchange"];
                if (rmq.contains("response_exchange")) response_exchange = rmq["response_exchange"];
            }

            if (llm.contains("system_prompt")) {
                system_prompt = llm["system_prompt"];
            }

            if (llm.contains("verbose")) {
                verbose = llm["verbose"];
            }

            if (llm.contains("resilience")) {
                const auto& res = llm["resilience"];
                if (res.contains("max_retries")) max_retries = res["max_retries"];
                if (res.contains("retry_delay_ms")) retry_delay_ms = res["retry_delay_ms"];
                if (res.contains("enable_circuit_breaker")) enable_circuit_breaker = res["enable_circuit_breaker"];
                if (res.contains("circuit_breaker_threshold")) circuit_breaker_threshold = res["circuit_breaker_threshold"];
            }
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[LLMClient] Erreur chargement config: " << e.what() << std::endl;
        return false;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTEUR / DESTRUCTEUR
// ═══════════════════════════════════════════════════════════════════════════

LLMClient::LLMClient(const LLMClientConfig& config)
    : config_(config) {
    config_.loadFromEnvironment();
}

LLMClient::~LLMClient() {
    shutdown();
}

// ═══════════════════════════════════════════════════════════════════════════
// INITIALISATION
// ═══════════════════════════════════════════════════════════════════════════

bool LLMClient::initialize() {
    log("Initialisation LLMClient...");

    // Validation de la clé API (mode HTTP direct)
    if (config_.mode == LLMMode::DIRECT_HTTP) {
        if (config_.api_key.empty()) {
            std::cerr << "[LLMClient] ERREUR: Clé API manquante. "
                      << "Définissez OPENAI_API_KEY ou config.api_key" << std::endl;
            return false;
        }
        log("Mode DIRECT_HTTP configuré avec modèle: " + config_.model);
    }

    // Connexion RabbitMQ (mode RabbitMQ)
    if (config_.mode == LLMMode::RABBITMQ) {
        if (!initRabbitMQ()) {
            std::cerr << "[LLMClient] ERREUR: Connexion RabbitMQ échouée" << std::endl;
            return false;
        }
        log("Mode RABBITMQ configuré");
    }

    ready_.store(true);
    log("LLMClient initialisé avec succès");
    return true;
}

bool LLMClient::initRabbitMQ() {
    try {
        // Construire la chaîne de connexion AMQP
        std::string amqp_uri = "amqp://" + config_.rabbitmq_user + ":" +
            config_.rabbitmq_password + "@" + config_.rabbitmq_host + ":" +
            std::to_string(config_.rabbitmq_port) + "/";

        channel_ = AmqpClient::Channel::Open(amqp_uri);

        // Déclarer l'exchange de requêtes
        channel_->DeclareExchange(
            config_.request_exchange,
            AmqpClient::Channel::EXCHANGE_TYPE_TOPIC,
            false,  // passive
            true,   // durable
            false   // auto_delete
        );

        // Déclarer l'exchange de réponses
        channel_->DeclareExchange(
            config_.response_exchange,
            AmqpClient::Channel::EXCHANGE_TYPE_TOPIC,
            false,
            true,
            false
        );

        // Créer une queue exclusive pour les réponses
        response_queue_ = channel_->DeclareQueue(
            "",      // nom auto-généré
            false,   // passive
            false,   // durable
            true,    // exclusive
            true     // auto_delete
        );

        // Bind la queue à l'exchange de réponses
        channel_->BindQueue(response_queue_, config_.response_exchange, "llm.response.#");

        // Démarrer la consommation
        consumer_tag_ = channel_->BasicConsume(
            response_queue_,
            "",      // consumer_tag auto
            true,    // no_local
            true,    // no_ack
            true     // exclusive
        );

        rabbitmq_connected_.store(true);
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[LLMClient] Erreur RabbitMQ: " << e.what() << std::endl;
        return false;
    }
}

void LLMClient::shutdown() {
    ready_.store(false);

    if (rabbitmq_connected_.load() && channel_) {
        try {
            channel_->BasicCancel(consumer_tag_);
        } catch (...) {}
        rabbitmq_connected_.store(false);
    }

    log("LLMClient arrêté");
}

// ═══════════════════════════════════════════════════════════════════════════
// GÉNÉRATION
// ═══════════════════════════════════════════════════════════════════════════

LLMResponse LLMClient::generate(const LLMRequest& request) {
    auto start_time = std::chrono::steady_clock::now();
    total_requests_++;

    // Vérifier le circuit breaker
    if (!shouldAttemptRequest()) {
        LLMResponse response;
        response.success = false;
        response.error_message = "Circuit breaker ouvert - trop d'échecs consécutifs";
        return response;
    }

    // Construire les messages
    auto messages = buildMessages(request.user_question, request.emotional_context);

    // Paramètres de génération
    double temperature = request.temperature.value_or(config_.temperature);
    int max_tokens = request.max_tokens.value_or(config_.max_tokens);

    // Appel avec retry
    LLMResponse response;
    int attempts = 0;
    int delay_ms = config_.retry_delay_ms;

    while (attempts < config_.max_retries) {
        attempts++;

        try {
            if (config_.mode == LLMMode::DIRECT_HTTP) {
                response = callDirectHTTP(messages, temperature, max_tokens);
            } else {
                response = callViaRabbitMQ(messages, temperature, max_tokens);
            }

            if (response.success) {
                recordSuccess();
                break;
            }
        } catch (const std::exception& e) {
            response.success = false;
            response.error_message = e.what();
        }

        // Retry avec exponential backoff
        if (attempts < config_.max_retries) {
            log("Tentative " + std::to_string(attempts) + " échouée, retry dans "
                + std::to_string(delay_ms) + "ms...");
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            delay_ms *= 2;  // Exponential backoff
        }
    }

    if (!response.success) {
        recordFailure();
    } else {
        successful_requests_++;
        total_tokens_ += response.tokens_total;

        // Ajouter à l'historique
        ChatMessage user_msg{"user", request.user_question};
        ChatMessage assistant_msg{"assistant", response.content};

        std::lock_guard<std::mutex> lock(history_mutex_);
        history_.push_back(user_msg);
        history_.push_back(assistant_msg);

        // Limiter l'historique
        while (history_.size() > history_limit_ * 2) {
            history_.erase(history_.begin());
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    response.generation_time_ms = std::chrono::duration<double, std::milli>(
        end_time - start_time).count();

    return response;
}

void LLMClient::generateAsync(const LLMRequest& request, LLMResponseCallback callback) {
    std::thread([this, request, callback]() {
        auto response = generate(request);
        if (callback) {
            callback(response);
        }
    }).detach();
}

std::string LLMClient::reformulate(const std::string& question, const LLMContext& context) {
    LLMRequest request;
    request.user_question = question;
    request.emotional_context = context;

    auto response = generate(request);

    if (response.success) {
        return response.content;
    } else {
        log("Erreur reformulation: " + response.error_message);
        return "";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTION DE PROMPTS
// ═══════════════════════════════════════════════════════════════════════════

std::string LLMClient::buildUserPrompt(const std::string& question, const LLMContext& context) const {
    std::ostringstream oss;

    oss << "Contexte émotionnel détecté:\n";

    // Sentiment global (Ft)
    oss << "- Fond affectif (Ft): " << std::fixed << std::setprecision(2)
        << context.Ft << " (" << context.sentiment_label << ")\n";

    // Niveau de conscience (Ct)
    oss << "- Niveau de conscience (Ct): " << std::fixed << std::setprecision(2)
        << context.Ct << "\n";

    // Émotion dominante
    if (!context.dominant_emotion.empty()) {
        oss << "- Émotion dominante: " << context.dominant_emotion
            << " (" << static_cast<int>(context.dominant_score * 100) << "%)\n";
    }

    // Autres émotions significatives
    if (context.emotions.size() > 1) {
        oss << "- Autres émotions: ";
        for (size_t i = 1; i < std::min(context.emotions.size(), size_t(3)); ++i) {
            if (i > 1) oss << ", ";
            oss << context.emotions[i].name
                << " (" << static_cast<int>(context.emotions[i].score * 100) << "%)";
        }
        oss << "\n";
    }

    // Mots-clés contextuels
    if (!context.context_words.empty()) {
        oss << "- Mots-clés du contexte: ";
        for (size_t i = 0; i < std::min(context.context_words.size(), size_t(5)); ++i) {
            if (i > 0) oss << ", ";
            oss << context.context_words[i];
        }
        oss << "\n";
    }

    // Souvenirs activés
    if (!context.activated_memories.empty()) {
        oss << "- Souvenirs activés: ";
        for (size_t i = 0; i < std::min(context.activated_memories.size(), size_t(3)); ++i) {
            if (i > 0) oss << ", ";
            oss << context.activated_memories[i];
        }
        oss << "\n";
    }

    oss << "\nQuestion de l'utilisateur: " << question;

    return oss.str();
}

std::vector<ChatMessage> LLMClient::buildMessages(const std::string& question, const LLMContext& context) const {
    std::vector<ChatMessage> messages;

    // Message système
    messages.push_back({"system", config_.system_prompt});

    // Historique (si présent)
    {
        std::lock_guard<std::mutex> lock(history_mutex_);
        for (const auto& msg : history_) {
            messages.push_back(msg);
        }
    }

    // Message utilisateur avec contexte
    std::string user_prompt = buildUserPrompt(question, context);
    messages.push_back({"user", user_prompt});

    return messages;
}

// ═══════════════════════════════════════════════════════════════════════════
// APPELS API
// ═══════════════════════════════════════════════════════════════════════════

LLMResponse LLMClient::callDirectHTTP(const std::vector<ChatMessage>& messages,
                                       double temperature, int max_tokens) {
    LLMResponse response;
    response.model_used = config_.model;

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.success = false;
        response.error_message = "Échec initialisation CURL";
        return response;
    }

    // Construire le body JSON
    json request_body;
    request_body["model"] = config_.model;
    request_body["temperature"] = temperature;
    request_body["max_tokens"] = max_tokens;

    request_body["messages"] = json::array();
    for (const auto& msg : messages) {
        request_body["messages"].push_back(msg.toJson());
    }

    std::string body_str = request_body.dump();
    std::string response_str;

    // Headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string auth_header = "Authorization: Bearer " + config_.api_key;
    headers = curl_slist_append(headers, auth_header.c_str());

    // Configuration CURL
    curl_easy_setopt(curl, CURLOPT_URL, config_.api_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body_str.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_str);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, config_.timeout_seconds);

    // Log requête (mode verbose)
    if (config_.verbose) {
        log("Appel API " + config_.model + "...");
    }

    // Exécuter la requête
    CURLcode res = curl_easy_perform(curl);

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        response.success = false;
        response.error_message = std::string("CURL error: ") + curl_easy_strerror(res);
        return response;
    }

    if (http_code != 200) {
        response.success = false;
        response.error_message = "HTTP " + std::to_string(http_code) + ": " + response_str;
        return response;
    }

    // Parser la réponse selon le provider
    if (config_.provider == LLMProvider::ANTHROPIC) {
        return parseAnthropicResponse(response_str);
    } else {
        return parseOpenAIResponse(response_str);
    }
}

LLMResponse LLMClient::callViaRabbitMQ(const std::vector<ChatMessage>& messages,
                                        double temperature, int max_tokens) {
    LLMResponse response;

    if (!rabbitmq_connected_.load()) {
        response.success = false;
        response.error_message = "RabbitMQ non connecté";
        return response;
    }

    try {
        // Construire la requête
        json request;
        request["model"] = config_.model;
        request["temperature"] = temperature;
        request["max_tokens"] = max_tokens;
        request["messages"] = json::array();

        for (const auto& msg : messages) {
            request["messages"].push_back(msg.toJson());
        }

        // Générer un correlation_id
        std::string correlation_id = "llm-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());

        // Publier la requête
        AmqpClient::BasicMessage::ptr_t message =
            AmqpClient::BasicMessage::Create(request.dump());
        message->CorrelationId(correlation_id);
        message->ReplyTo(response_queue_);
        message->ContentType("application/json");

        channel_->BasicPublish(
            config_.request_exchange,
            "llm.generate",
            message
        );

        log("Requête publiée, correlation_id: " + correlation_id);

        // Attendre la réponse (avec timeout)
        auto deadline = std::chrono::steady_clock::now()
            + std::chrono::seconds(config_.timeout_seconds);

        while (std::chrono::steady_clock::now() < deadline) {
            AmqpClient::Envelope::ptr_t envelope;

            if (channel_->BasicConsumeMessage(consumer_tag_, envelope, 100)) {
                // Vérifier le correlation_id
                if (envelope->Message()->CorrelationId() == correlation_id) {
                    std::string body = envelope->Message()->Body();
                    json resp_json = json::parse(body);

                    response.success = resp_json.value("success", false);
                    response.content = resp_json.value("content", "");
                    response.model_used = resp_json.value("model", config_.model);
                    response.tokens_total = resp_json.value("tokens_used", 0);

                    if (!response.success) {
                        response.error_message = resp_json.value("error", "Erreur inconnue");
                    }

                    return response;
                }
            }
        }

        response.success = false;
        response.error_message = "Timeout en attente de réponse RabbitMQ";
        return response;

    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = std::string("Erreur RabbitMQ: ") + e.what();
        return response;
    }
}

LLMResponse LLMClient::parseOpenAIResponse(const std::string& json_response) {
    LLMResponse response;

    try {
        json j = json::parse(json_response);

        if (j.contains("error")) {
            response.success = false;
            response.error_message = j["error"]["message"].get<std::string>();
            return response;
        }

        response.success = true;

        if (j.contains("choices") && !j["choices"].empty()) {
            response.content = j["choices"][0]["message"]["content"].get<std::string>();
        }

        response.model_used = j.value("model", config_.model);

        if (j.contains("usage")) {
            response.tokens_prompt = j["usage"].value("prompt_tokens", 0);
            response.tokens_completion = j["usage"].value("completion_tokens", 0);
            response.tokens_total = j["usage"].value("total_tokens", 0);
        }

    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = std::string("Erreur parsing JSON: ") + e.what();
    }

    return response;
}

LLMResponse LLMClient::parseAnthropicResponse(const std::string& json_response) {
    LLMResponse response;

    try {
        json j = json::parse(json_response);

        if (j.contains("error")) {
            response.success = false;
            response.error_message = j["error"]["message"].get<std::string>();
            return response;
        }

        response.success = true;

        if (j.contains("content") && !j["content"].empty()) {
            // Anthropic renvoie un array de content blocks
            for (const auto& block : j["content"]) {
                if (block["type"] == "text") {
                    response.content += block["text"].get<std::string>();
                }
            }
        }

        response.model_used = j.value("model", config_.model);

        if (j.contains("usage")) {
            response.tokens_prompt = j["usage"].value("input_tokens", 0);
            response.tokens_completion = j["usage"].value("output_tokens", 0);
            response.tokens_total = response.tokens_prompt + response.tokens_completion;
        }

    } catch (const std::exception& e) {
        response.success = false;
        response.error_message = std::string("Erreur parsing JSON: ") + e.what();
    }

    return response;
}

// ═══════════════════════════════════════════════════════════════════════════
// HISTORIQUE
// ═══════════════════════════════════════════════════════════════════════════

void LLMClient::addToHistory(const ChatMessage& message) {
    std::lock_guard<std::mutex> lock(history_mutex_);
    history_.push_back(message);

    while (history_.size() > history_limit_ * 2) {
        history_.erase(history_.begin());
    }
}

void LLMClient::clearHistory() {
    std::lock_guard<std::mutex> lock(history_mutex_);
    history_.clear();
}

// ═══════════════════════════════════════════════════════════════════════════
// CIRCUIT BREAKER
// ═══════════════════════════════════════════════════════════════════════════

void LLMClient::recordSuccess() {
    consecutive_failures_.store(0);

    if (circuit_open_.load()) {
        circuit_open_.store(false);
        log("Circuit breaker fermé - service rétabli");
    }
}

void LLMClient::recordFailure() {
    int failures = ++consecutive_failures_;

    if (config_.enable_circuit_breaker &&
        failures >= config_.circuit_breaker_threshold &&
        !circuit_open_.load()) {

        circuit_open_.store(true);
        circuit_opened_at_ = std::chrono::steady_clock::now();
        log("Circuit breaker OUVERT après " + std::to_string(failures) + " échecs");
    }
}

bool LLMClient::shouldAttemptRequest() {
    if (!config_.enable_circuit_breaker) {
        return true;
    }

    if (!circuit_open_.load()) {
        return true;
    }

    // Vérifier si le timeout du circuit breaker est écoulé
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - circuit_opened_at_).count();

    if (elapsed >= config_.circuit_breaker_timeout_s) {
        log("Circuit breaker - tentative de fermeture après timeout");
        return true;  // Laisser passer une requête pour tester
    }

    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// LOGGING
// ═══════════════════════════════════════════════════════════════════════════

void LLMClient::log(const std::string& message) const {
    if (config_.verbose) {
        std::cout << "[LLMClient] " << message << std::endl;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// EMOTIONAL RESPONSE PIPELINE
// ═══════════════════════════════════════════════════════════════════════════

EmotionalResponsePipeline::EmotionalResponsePipeline(
    std::shared_ptr<HybridSearchEngine> search_engine,
    std::shared_ptr<LLMClient> llm_client)
    : search_engine_(std::move(search_engine))
    , llm_client_(std::move(llm_client)) {
}

PipelineResult EmotionalResponsePipeline::process(
    const std::string& question,
    const std::vector<std::string>& lemmas,
    const std::vector<double>& embedding) {

    PipelineResult result;
    auto start_time = std::chrono::steady_clock::now();

    // Étape 1: Recherche hybride (si search_engine disponible)
    auto search_start = std::chrono::steady_clock::now();

    if (search_engine_) {
        // Note: HybridSearchEngine::search() retourne un SearchResponse avec contexte
        // Pour l'instant, on utilise un contexte par défaut si pas de search_engine
        // L'implémentation complète viendra avec HybridSearchEngine
    }

    auto search_end = std::chrono::steady_clock::now();
    result.search_time_ms = std::chrono::duration<double, std::milli>(
        search_end - search_start).count();

    // Étape 2: Génération LLM
    auto llm_start = std::chrono::steady_clock::now();

    LLMRequest request;
    request.user_question = question;
    request.emotional_context = result.context;

    auto llm_response = llm_client_->generate(request);

    auto llm_end = std::chrono::steady_clock::now();
    result.llm_time_ms = std::chrono::duration<double, std::milli>(
        llm_end - llm_start).count();

    result.success = llm_response.success;
    result.response = llm_response.content;

    if (!llm_response.success) {
        result.error = llm_response.error_message;
    }

    auto end_time = std::chrono::steady_clock::now();
    result.total_time_ms = std::chrono::duration<double, std::milli>(
        end_time - start_time).count();

    return result;
}

void EmotionalResponsePipeline::processAsync(
    const std::string& question,
    const std::vector<std::string>& lemmas,
    const std::vector<double>& embedding,
    std::function<void(const PipelineResult&)> callback) {

    std::thread([this, question, lemmas, embedding, callback]() {
        auto result = process(question, lemmas, embedding);
        if (callback) {
            callback(result);
        }
    }).detach();
}

} // namespace mcee
