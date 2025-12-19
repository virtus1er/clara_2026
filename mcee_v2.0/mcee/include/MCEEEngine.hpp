/**
 * @file MCEEEngine.hpp
 * @brief Moteur principal du MCEE (Modèle Complet d'Évaluation des États)
 * @version 2.0
 * @date 2025-12-19
 * 
 * Le MCEEEngine orchestre:
 * - La réception des émotions via RabbitMQ
 * - La détection de phase
 * - La mise à jour des émotions
 * - La gestion de la mémoire
 * - Le système d'urgence Amyghaleon
 */

#ifndef MCEE_ENGINE_HPP
#define MCEE_ENGINE_HPP

#include "Types.hpp"
#include "PhaseDetector.hpp"
#include "EmotionUpdater.hpp"
#include "Amyghaleon.hpp"
#include "MemoryManager.hpp"
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace mcee {

using json = nlohmann::json;

/**
 * @brief Configuration RabbitMQ
 */
struct RabbitMQConfig {
    std::string host = "localhost";
    int port = 5672;
    std::string user = "virtus";
    std::string password = "virtus@83";
    std::string input_exchange = "mcee.emotional.input";
    std::string input_routing_key = "emotions.predictions";
    std::string output_exchange = "mcee.emotional.output";
    std::string output_routing_key = "mcee.state";
};

/**
 * @class MCEEEngine
 * @brief Moteur principal du système MCEE v2.0
 */
class MCEEEngine {
public:
    using StateCallback = std::function<void(const EmotionalState&, Phase)>;

    /**
     * @brief Constructeur
     * @param rabbitmq_config Configuration RabbitMQ
     */
    explicit MCEEEngine(const RabbitMQConfig& rabbitmq_config = RabbitMQConfig{});

    /**
     * @brief Destructeur
     */
    ~MCEEEngine();

    // Non-copyable
    MCEEEngine(const MCEEEngine&) = delete;
    MCEEEngine& operator=(const MCEEEngine&) = delete;

    /**
     * @brief Démarre le moteur MCEE
     * @return true si démarrage réussi
     */
    bool start();

    /**
     * @brief Arrête le moteur MCEE
     */
    void stop();

    /**
     * @brief Vérifie si le moteur est en cours d'exécution
     */
    [[nodiscard]] bool isRunning() const { return running_.load(); }

    /**
     * @brief Traite un état émotionnel (entrée directe sans RabbitMQ)
     * @param raw_emotions Émotions brutes (24 valeurs)
     */
    void processEmotions(const std::unordered_map<std::string, double>& raw_emotions);

    /**
     * @brief Définit les feedbacks externes/internes
     */
    void setFeedback(double external, double internal);

    /**
     * @brief Définit le callback pour les changements d'état
     */
    void setStateCallback(StateCallback callback);

    /**
     * @brief Retourne l'état émotionnel actuel
     */
    [[nodiscard]] const EmotionalState& getCurrentState() const { return current_state_; }

    /**
     * @brief Retourne la phase actuelle
     */
    [[nodiscard]] Phase getCurrentPhase() const { return phase_detector_.getCurrentPhase(); }

    /**
     * @brief Retourne les statistiques
     */
    [[nodiscard]] const MCEEStats& getStats() const { return stats_; }

    /**
     * @brief Retourne la configuration de la phase actuelle
     */
    [[nodiscard]] const PhaseConfig& getPhaseConfig() const { 
        return phase_detector_.getCurrentConfig(); 
    }

    /**
     * @brief Charge la configuration depuis un fichier JSON
     * @param config_path Chemin vers le fichier de configuration
     * @return true si chargement réussi
     */
    bool loadConfig(const std::string& config_path);

    /**
     * @brief Force une transition de phase
     */
    void forcePhaseTransition(Phase phase, const std::string& reason = "MANUAL");

    /**
     * @brief Retourne le gestionnaire de mémoire
     */
    MemoryManager& getMemoryManager() { return memory_manager_; }

private:
    // Configuration
    RabbitMQConfig rabbitmq_config_;

    // Composants MCEE
    PhaseDetector phase_detector_;
    EmotionUpdater emotion_updater_;
    Amyghaleon amyghaleon_;
    MemoryManager memory_manager_;

    // État
    EmotionalState current_state_;
    EmotionalState previous_state_;
    Feedback current_feedback_;
    double wisdom_ = 0.0;
    MCEEStats stats_;

    // RabbitMQ
    AmqpClient::Channel::ptr_t channel_;
    std::string consumer_tag_;

    // Threading
    std::atomic<bool> running_{false};
    std::thread consumer_thread_;
    std::mutex state_mutex_;
    StateCallback on_state_change_;

    // Timestamps
    std::chrono::steady_clock::time_point last_update_time_;

    /**
     * @brief Initialise la connexion RabbitMQ
     */
    bool initRabbitMQ();

    /**
     * @brief Boucle de consommation des messages RabbitMQ
     */
    void consumeLoop();

    /**
     * @brief Traite un message RabbitMQ
     * @param body Corps du message (JSON)
     */
    void handleMessage(const std::string& body);

    /**
     * @brief Pipeline de traitement MCEE complet
     * @param raw_emotions Émotions brutes du module C++
     */
    void processPipeline(const std::unordered_map<std::string, double>& raw_emotions);

    /**
     * @brief Publie l'état actuel via RabbitMQ
     */
    void publishState();

    /**
     * @brief Met à jour la sagesse accumulée
     */
    void updateWisdom();

    /**
     * @brief Gère les boucles infinies en phase PEUR
     */
    void handleFearLoop();

    /**
     * @brief Exécute une action d'urgence
     */
    void executeEmergencyAction(const EmergencyResponse& response);

    /**
     * @brief Convertit les émotions brutes en EmotionalState
     */
    EmotionalState rawToState(const std::unordered_map<std::string, double>& raw) const;

    /**
     * @brief Affiche l'état émotionnel actuel
     */
    void printState() const;
};

} // namespace mcee

#endif // MCEE_ENGINE_HPP
