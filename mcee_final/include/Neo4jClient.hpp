/**
 * @file Neo4jClient.hpp
 * @brief Client Neo4j via RabbitMQ pour le MCEE
 * @version 1.0
 * @date 2025-12-21
 *
 * Communique avec le service Neo4j Python via RabbitMQ.
 * Gère la synchronisation des souvenirs entre MCEE et Neo4j.
 */

#ifndef MCEE_NEO4J_CLIENT_HPP
#define MCEE_NEO4J_CLIENT_HPP

#include "Types.hpp"
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <nlohmann/json.hpp>
#include <string>
#include <optional>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <queue>
#include <functional>
#include <condition_variable>

namespace mcee {

using json = nlohmann::json;

/**
 * @brief Configuration du client Neo4j
 */
struct Neo4jClientConfig {
    // RabbitMQ
    std::string rabbitmq_host = "localhost";
    int rabbitmq_port = 5672;
    std::string rabbitmq_user = "virtus";
    std::string rabbitmq_password = "virtus@83";

    // Queues Neo4j
    std::string request_queue = "neo4j.requests.queue";
    std::string response_exchange = "neo4j.responses";

    // Timeouts
    int request_timeout_ms = 5000;
    int max_retries = 3;

    // Mode
    bool async_mode = true;  // true = async, false = sync
};

/**
 * @brief Réponse Neo4j
 */
struct Neo4jResponse {
    std::string request_id;
    bool success = false;
    json data;
    std::string error;
    double execution_time_ms = 0;
};

/**
 * @brief Callback pour réponses asynchrones
 */
using Neo4jCallback = std::function<void(const Neo4jResponse&)>;

/**
 * @class Neo4jClient
 * @brief Client pour communiquer avec le service Neo4j via RabbitMQ
 */
class Neo4jClient {
public:
    /**
     * @brief Constructeur
     * @param config Configuration du client
     */
    explicit Neo4jClient(const Neo4jClientConfig& config = Neo4jClientConfig{});

    /**
     * @brief Destructeur
     */
    ~Neo4jClient();

    // Non-copyable
    Neo4jClient(const Neo4jClient&) = delete;
    Neo4jClient& operator=(const Neo4jClient&) = delete;

    /**
     * @brief Connecte au service RabbitMQ
     * @return true si connexion réussie
     */
    bool connect();

    /**
     * @brief Déconnecte du service
     */
    void disconnect();

    /**
     * @brief Vérifie si connecté
     */
    [[nodiscard]] bool isConnected() const { return connected_.load(); }

    // ═══════════════════════════════════════════════════════════════════════════
    // OPÉRATIONS MÉMOIRE
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Crée un souvenir dans Neo4j
     * @param memory Souvenir à créer
     * @param context Contexte textuel
     * @param callback Callback optionnel (mode async)
     * @return ID du souvenir créé (mode sync) ou request_id (mode async)
     */
    std::string createMemory(
        const Memory& memory,
        const std::string& context = "",
        Neo4jCallback callback = nullptr
    );

    /**
     * @brief Crée un trauma dans Neo4j
     * @param memory Souvenir trauma
     * @param trigger_keywords Mots-clés déclencheurs
     * @param callback Callback optionnel
     * @return ID du trauma créé
     */
    std::string createTrauma(
        const Memory& memory,
        const std::vector<std::string>& trigger_keywords = {},
        Neo4jCallback callback = nullptr
    );

    /**
     * @brief Fusionne avec un souvenir existant
     * @param target_id ID du souvenir cible
     * @param emotions Émotions à fusionner
     * @param transfer_weight Poids de transfert
     * @param callback Callback optionnel
     */
    void mergeMemory(
        const std::string& target_id,
        const std::array<double, NUM_EMOTIONS>& emotions,
        double transfer_weight = 0.3,
        Neo4jCallback callback = nullptr
    );

    /**
     * @brief Récupère un souvenir par ID
     * @param memory_id ID du souvenir
     * @return Souvenir optionnel
     */
    std::optional<Memory> getMemory(const std::string& memory_id);

    /**
     * @brief Trouve les souvenirs similaires
     * @param emotions Vecteur d'émotions de recherche
     * @param threshold Seuil de similarité (0-1)
     * @param limit Nombre maximum de résultats
     * @return Liste des IDs et scores de similarité
     */
    std::vector<std::pair<std::string, double>> findSimilarMemories(
        const std::array<double, NUM_EMOTIONS>& emotions,
        double threshold = 0.85,
        size_t limit = 5
    );

    /**
     * @brief Réactive un souvenir
     * @param memory_id ID du souvenir
     * @param strength Force de réactivation
     * @param callback Callback optionnel
     */
    void reactivateMemory(
        const std::string& memory_id,
        double strength = 1.0,
        Neo4jCallback callback = nullptr
    );

    /**
     * @brief Applique le decay aux souvenirs
     * @param elapsed_hours Heures écoulées
     * @param callback Callback optionnel
     */
    void applyDecay(
        double elapsed_hours = 1.0,
        Neo4jCallback callback = nullptr
    );

    /**
     * @brief Supprime un souvenir
     * @param memory_id ID du souvenir
     * @param archive Archiver avant suppression
     * @param callback Callback optionnel
     */
    void deleteMemory(
        const std::string& memory_id,
        bool archive = true,
        Neo4jCallback callback = nullptr
    );

    // ═══════════════════════════════════════════════════════════════════════════
    // OPÉRATIONS PATTERNS
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Enregistre une transition de pattern
     * @param from_pattern Pattern source
     * @param to_pattern Pattern destination
     * @param duration_s Durée en secondes
     * @param trigger Déclencheur
     * @param callback Callback optionnel
     */
    void recordPatternTransition(
        const std::string& from_pattern,
        const std::string& to_pattern,
        double duration_s = 0,
        const std::string& trigger = "",
        Neo4jCallback callback = nullptr
    );

    /**
     * @brief Récupère les transitions possibles depuis un pattern
     * @param from_pattern Pattern source
     * @return Liste des transitions (pattern, probabilité)
     */
    std::vector<std::pair<std::string, double>> getPatternTransitions(
        const std::string& from_pattern
    );

    // ═══════════════════════════════════════════════════════════════════════════
    // OPÉRATIONS SESSION
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Crée une nouvelle session MCT (synchrone)
     * @param pattern Pattern initial
     * @return ID de la session
     */
    std::string createSession(const std::string& pattern = "SERENITE");

    /**
     * @brief Crée une nouvelle session MCT (asynchrone, non-bloquant)
     * @param pattern Pattern initial
     * @param callback Callback appelé quand la session est créée
     */
    void createSessionAsync(const std::string& pattern, Neo4jCallback callback);

    /**
     * @brief Met à jour une session avec un état émotionnel
     * @param session_id ID de la session
     * @param state État émotionnel
     * @param callback Callback optionnel
     */
    void updateSession(
        const std::string& session_id,
        const EmotionalState& state,
        Neo4jCallback callback = nullptr
    );

    // ═══════════════════════════════════════════════════════════════════════════
    // REQUÊTES GÉNÉRIQUES
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Exécute une requête Cypher personnalisée
     * @param query Requête Cypher
     * @param params Paramètres
     * @return Résultat JSON
     */
    json executeCypher(const std::string& query, const json& params = {});

private:
    Neo4jClientConfig config_;
    AmqpClient::Channel::ptr_t channel_;
    std::string response_queue_;
    std::string consumer_tag_;

    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> request_counter_{0};

    // Thread pour consommer les réponses
    std::thread response_thread_;

    // Callbacks en attente
    std::mutex callbacks_mutex_;
    std::unordered_map<std::string, Neo4jCallback> pending_callbacks_;

    // Réponses synchrones en attente
    std::mutex sync_mutex_;
    std::condition_variable sync_cv_;
    std::unordered_map<std::string, Neo4jResponse> sync_responses_;

    /**
     * @brief Génère un ID de requête unique
     */
    std::string generateRequestId();

    /**
     * @brief Envoie une requête au service Neo4j
     * @param request_type Type de requête
     * @param payload Données
     * @param callback Callback optionnel
     * @return ID de la requête
     */
    std::string sendRequest(
        const std::string& request_type,
        const json& payload,
        Neo4jCallback callback = nullptr
    );

    /**
     * @brief Attend une réponse synchrone
     * @param request_id ID de la requête
     * @return Réponse
     */
    Neo4jResponse waitForResponse(const std::string& request_id);

    /**
     * @brief Boucle de consommation des réponses
     */
    void responseConsumerLoop();

    /**
     * @brief Convertit un Memory C++ en JSON
     */
    json memoryToJson(const Memory& memory) const;

    /**
     * @brief Convertit un JSON en Memory C++
     */
    Memory jsonToMemory(const json& j) const;

    /**
     * @brief Convertit un EmotionalState en JSON
     */
    json stateToJson(const EmotionalState& state) const;
};

} // namespace mcee

#endif // MCEE_NEO4J_CLIENT_HPP
