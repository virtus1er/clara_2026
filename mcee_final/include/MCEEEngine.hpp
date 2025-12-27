/**
 * @file MCEEEngine.hpp
 * @brief Moteur principal du MCEE (Modèle Complet d'Évaluation des États)
 * @version 3.0
 * @date 2024
 * 
 * Architecture MCT/MLT avec patterns dynamiques :
 * - MCT (Mémoire Court Terme) : Buffer temporel des états
 * - MLT (Mémoire Long Terme) : Patterns émotionnels dynamiques
 * - PatternMatcher : Identification et création de patterns
 * - Amyghaleon : Système d'urgence
 */

#ifndef MCEE_ENGINE_HPP
#define MCEE_ENGINE_HPP

#include "Types.hpp"
#include "MCT.hpp"
#include "MCTGraph.hpp"
#include "MLT.hpp"
#include "PatternMatcher.hpp"
#include "PhaseDetector.hpp"
#include "EmotionUpdater.hpp"
#include "Amyghaleon.hpp"
#include "MemoryManager.hpp"
#include "SpeechInput.hpp"
#include "ConscienceEngine.hpp"
#include "ADDOEngine.hpp"
#include "DecisionEngine.hpp"
#include "LLMClient.hpp"
#include "HybridSearchEngine.hpp"
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>

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
    
    // Entrée émotions (depuis module emotion C++)
    std::string emotions_exchange = "mcee.emotional.input";
    std::string emotions_routing_key = "emotions.predictions";
    
    // Entrée parole (depuis module speech)
    std::string speech_exchange = "mcee.speech.input";
    std::string speech_routing_key = "speech.text";

    // Entrée tokens Neo4j/spaCy (pour MCTGraph)
    std::string tokens_exchange = "neo4j.tokens.output";
    std::string tokens_routing_key = "tokens.extracted";

    // Sortie état MCEE
    std::string output_exchange = "mcee.emotional.output";
    std::string output_routing_key = "mcee.state";

    // Sortie snapshots MCTGraph (vers module rêves)
    std::string snapshot_exchange = "mcee.mct.snapshot";
    std::string snapshot_routing_key = "mct.graph";
};

/**
 * @class MCEEEngine
 * @brief Moteur principal du système MCEE v3.0 avec MCT/MLT
 * 
 * Flux de traitement :
 * 1. Réception émotions/parole → MCT (buffer)
 * 2. Extraction signature MCT → PatternMatcher
 * 3. Comparaison avec MLT → Pattern identifié
 * 4. Coefficients dynamiques → EmotionUpdater
 * 5. Nouvelle émotion → MCT + consolidation MLT
 */
class MCEEEngine {
public:
    using StateCallback = std::function<void(const EmotionalState&, const std::string& pattern_name)>;

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
     * @brief Traite un texte reçu du module de parole
     * @param text Texte à traiter
     * @param source Source du texte (user, environment, system)
     */
    void processSpeechText(const std::string& text, const std::string& source = "user");

    /**
     * @brief Définit les feedbacks externes/internes
     */
    void setFeedback(double external, double internal);

    /**
     * @brief Retourne le gestionnaire de parole
     */
    SpeechInput& getSpeechInput() { return speech_input_; }

    /**
     * @brief Définit le callback pour les changements d'état
     */
    void setStateCallback(StateCallback callback);

    /**
     * @brief Retourne l'état émotionnel actuel
     */
    [[nodiscard]] const EmotionalState& getCurrentState() const { return current_state_; }

    /**
     * @brief Retourne le nom du pattern actuel (remplace Phase)
     */
    [[nodiscard]] std::string getCurrentPatternName() const;

    /**
     * @brief Retourne l'ID du pattern actuel
     */
    [[nodiscard]] std::string getCurrentPatternId() const;

    /**
     * @brief Retourne les statistiques
     */
    [[nodiscard]] const MCEEStats& getStats() const { return stats_; }

    /**
     * @brief Retourne les coefficients actuels du pattern
     */
    [[nodiscard]] MatchResult getCurrentMatchResult() const { return current_match_; }

    /**
     * @brief Charge la configuration depuis un fichier JSON
     * @param config_path Chemin vers le fichier de configuration
     * @param skip_neo4j Si true, ignore la configuration Neo4j (mode démo)
     * @return true si chargement réussi
     */
    bool loadConfig(const std::string& config_path, bool skip_neo4j = false);

    /**
     * @brief Charge les patterns depuis un fichier
     */
    bool loadPatterns(const std::string& path);

    /**
     * @brief Sauvegarde les patterns dans un fichier
     */
    bool savePatterns(const std::string& path) const;

    /**
     * @brief Force un pattern spécifique
     */
    void forcePattern(const std::string& pattern_name, const std::string& reason = "MANUAL");

    /**
     * @brief Crée un nouveau pattern à partir de l'état actuel
     */
    std::string createPatternFromCurrent(const std::string& name, const std::string& description = "");

    /**
     * @brief Retourne le gestionnaire de mémoire
     */
    MemoryManager& getMemoryManager() { return memory_manager_; }

    /**
     * @brief Retourne la MCT
     */
    std::shared_ptr<MCT> getMCT() { return mct_; }

    /**
     * @brief Retourne la MLT
     */
    std::shared_ptr<MLT> getMLT() { return mlt_; }

    /**
     * @brief Retourne le PatternMatcher
     */
    std::shared_ptr<PatternMatcher> getPatternMatcher() { return pattern_matcher_; }

    /**
     * @brief Retourne le MCTGraph (graphe relationnel mots-émotions)
     */
    std::shared_ptr<MCTGraph> getMCTGraph() { return mct_graph_; }

    /**
     * @brief Retourne le ConscienceEngine
     */
    std::shared_ptr<ConscienceEngine> getConscienceEngine() { return conscience_engine_; }

    /**
     * @brief Retourne l'ADDOEngine (Détermination des Objectifs)
     */
    std::shared_ptr<ADDOEngine> getADDOEngine() { return addo_engine_; }

    /**
     * @brief Retourne le DecisionEngine (Prise de Décision Réfléchie)
     */
    std::shared_ptr<DecisionEngine> getDecisionEngine() { return decision_engine_; }

    /**
     * @brief Retourne le LLMClient (Reformulation Émotionnelle)
     */
    std::shared_ptr<LLMClient> getLLMClient() { return llm_client_; }

    /**
     * @brief Retourne le HybridSearchEngine (Recherche Mémoire)
     */
    std::shared_ptr<HybridSearchEngine> getHybridSearchEngine() { return hybrid_search_; }

    /**
     * @brief Génère une réponse émotionnellement adaptée via LLM
     * @param question Question utilisateur
     * @param lemmas Lemmes extraits (du module Python)
     * @param embedding Embedding de la question (optionnel)
     * @return Réponse reformulée avec contexte émotionnel
     */
    std::string generateEmotionalResponse(
        const std::string& question,
        const std::vector<std::string>& lemmas = {},
        const std::vector<double>& embedding = {}
    );

    /**
     * @brief Génère une réponse émotionnelle de façon asynchrone
     * @param question Question utilisateur
     * @param lemmas Lemmes extraits
     * @param embedding Embedding (optionnel)
     * @param callback Callback appelé avec le résultat
     */
    void generateEmotionalResponseAsync(
        const std::string& question,
        const std::vector<std::string>& lemmas,
        const std::vector<double>& embedding,
        std::function<void(const PipelineResult&)> callback
    );

    /**
     * @brief Vérifie si le LLMClient est prêt
     */
    [[nodiscard]] bool isLLMReady() const { return llm_client_ && llm_client_->isReady(); }

    /**
     * @brief Active/désactive le mode silencieux (moins de logs)
     */
    void setQuietMode(bool quiet) {
        quiet_mode_ = quiet;
        emotion_updater_.setQuietMode(quiet);
        speech_input_.setQuietMode(quiet);
    }
    [[nodiscard]] bool isQuietMode() const { return quiet_mode_; }

    /**
     * @brief Récupère l'état de conscience et sentiment actuel
     */
    ConscienceSentimentState getConscienceState() const;

    /**
     * @brief Récupère l'objectif courant G(t)
     */
    GoalState getGoalState() const;

    /**
     * @brief Effectue une prise de décision réfléchie
     * @param context_type Type de contexte
     * @param available_actions Actions disponibles (optionnel)
     * @return Résultat de décision
     */
    DecisionResult makeDecision(
        const std::string& context_type,
        const std::vector<ActionOption>& available_actions = {}
    );

    /**
     * @brief Envoie un feedback sur le pattern actuel
     * @param feedback Score [-1, 1]
     */
    void provideFeedback(double feedback);

    /**
     * @brief Déclenche une passe d'apprentissage MLT
     */
    void runLearning();

    // Compatibilité legacy (Phase)
    [[nodiscard]] Phase getCurrentPhase() const { return phase_detector_.getCurrentPhase(); }
    [[nodiscard]] const PhaseConfig& getPhaseConfig() const { return phase_detector_.getCurrentConfig(); }
    void forcePhaseTransition(Phase phase, const std::string& reason = "MANUAL");

private:
    // Configuration
    RabbitMQConfig rabbitmq_config_;
    bool quiet_mode_ = false;  // Mode silencieux (moins de logs)

    // Nouveau système MCT/MLT (v3)
    std::shared_ptr<MCT> mct_;
    std::shared_ptr<MCTGraph> mct_graph_;  // Graphe relationnel mots-émotions
    std::shared_ptr<MLT> mlt_;
    std::shared_ptr<PatternMatcher> pattern_matcher_;
    std::shared_ptr<ConscienceEngine> conscience_engine_;  // Module Conscience & Sentiments
    std::shared_ptr<ADDOEngine> addo_engine_;              // Module Détermination des Objectifs
    std::shared_ptr<DecisionEngine> decision_engine_;      // Module Prise de Décision Réfléchie
    std::shared_ptr<LLMClient> llm_client_;                // Module Reformulation Émotionnelle
    std::shared_ptr<HybridSearchEngine> hybrid_search_;    // Module Recherche Hybride Mémoire
    MatchResult current_match_;

    // ID de la dernière émotion ajoutée (pour liaison avec mots)
    std::string last_emotion_node_id_;

    // Composants legacy (pour compatibilité)
    PhaseDetector phase_detector_;
    EmotionUpdater emotion_updater_;
    Amyghaleon amyghaleon_;
    MemoryManager memory_manager_;
    SpeechInput speech_input_;

    // État
    EmotionalState current_state_;
    EmotionalState previous_state_;
    Feedback current_feedback_;
    SpeechAnalysis last_speech_analysis_;
    double wisdom_ = 0.0;
    MCEEStats stats_;

    // RabbitMQ - Channels séparés pour éviter le blocage entre consumers
    // Note: AmqpClient::Channel n'est PAS thread-safe, chaque thread doit avoir son propre channel
    AmqpClient::Channel::ptr_t emotions_channel_;   // Channel dédié consommation émotions
    AmqpClient::Channel::ptr_t speech_channel_;     // Channel dédié consommation parole
    AmqpClient::Channel::ptr_t tokens_channel_;     // Channel dédié consommation tokens
    AmqpClient::Channel::ptr_t publish_channel_;    // Channel dédié publications (état + snapshots)
    std::string emotions_consumer_tag_;
    std::string speech_consumer_tag_;
    std::string tokens_consumer_tag_;

    // Threading
    std::atomic<bool> running_{false};
    std::thread emotions_consumer_thread_;
    std::thread speech_consumer_thread_;
    std::thread tokens_consumer_thread_;
    std::thread snapshot_timer_thread_;
    std::mutex state_mutex_;
    StateCallback on_state_change_;

    // Timestamps
    std::chrono::steady_clock::time_point last_update_time_;
    std::chrono::steady_clock::time_point pattern_start_time_;

    /**
     * @brief Initialise la connexion RabbitMQ
     */
    bool initRabbitMQ();

    /**
     * @brief Initialise le système MCT/MLT
     */
    void initMemorySystem();

    /**
     * @brief Boucle de consommation des émotions RabbitMQ
     */
    void emotionsConsumeLoop();

    /**
     * @brief Boucle de consommation des textes RabbitMQ
     */
    void speechConsumeLoop();

    /**
     * @brief Boucle de consommation des tokens Neo4j/spaCy
     */
    void tokensConsumeLoop();

    /**
     * @brief Boucle du timer pour export snapshots MCTGraph
     */
    void snapshotTimerLoop();

    /**
     * @brief Traite un message d'émotion RabbitMQ
     * @param body Corps du message (JSON)
     */
    void handleEmotionMessage(const std::string& body);

    /**
     * @brief Traite un message de parole RabbitMQ
     * @param body Corps du message (JSON)
     */
    void handleSpeechMessage(const std::string& body);

    /**
     * @brief Traite un message de tokens Neo4j/spaCy
     * @param body Corps du message (JSON)
     */
    void handleTokensMessage(const std::string& body);

    /**
     * @brief Publie un snapshot MCTGraph vers le module rêves
     */
    void publishSnapshot(const MCTGraphSnapshot& snapshot);

    /**
     * @brief Pipeline de traitement MCEE v3 complet
     * @param raw_emotions Émotions brutes du module C++
     */
    void processPipeline(const std::unordered_map<std::string, double>& raw_emotions);

    /**
     * @brief Étape 1: Ajoute l'état brut à la MCT
     */
    void pushToMCT(const EmotionalState& state);

    /**
     * @brief Étape 2: Identifie le pattern via MLT
     */
    MatchResult identifyPattern();

    /**
     * @brief Étape 3: Applique les coefficients du pattern
     */
    EmotionalState applyPatternCoefficients(const EmotionalState& raw_state, const MatchResult& match);

    /**
     * @brief Étape 4: Consolide en MLT si significatif
     */
    void consolidateToMLT();

    /**
     * @brief Publie l'état actuel via RabbitMQ
     */
    void publishState();

    /**
     * @brief Met à jour la sagesse accumulée
     */
    void updateWisdom();

    /**
     * @brief Gère les urgences (patterns à seuil bas)
     */
    void handleEmergency(const MatchResult& match);

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

    /**
     * @brief Configure les callbacks MCT/MLT/Matcher
     */
    void setupCallbacks();
};

} // namespace mcee

#endif // MCEE_ENGINE_HPP
