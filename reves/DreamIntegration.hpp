#pragma once

/**
 * @file DreamIntegration.hpp
 * @brief Intégration du DreamEngine avec les composants MCEE existants
 * 
 * Connecte le DreamEngine à:
 * - PatternMatcher (pour obtenir le pattern actif)
 * - Amyghaleon (pour les interruptions d'urgence)
 * - MCT existante (pour alimenter les souvenirs)
 * - RabbitMQ (pour la communication inter-composants)
 */

#include "DreamEngine.hpp"
#include "DreamNeo4jBridge.hpp"

// Inclure les headers MCEE existants (adapter les chemins selon ton projet)
// #include "PatternMatcher.hpp"
// #include "Amyghaleon.hpp"
// #include "MCT.hpp"
// #include "Types.hpp"

#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>

namespace MCEE {

using json = nlohmann::json;

/**
 * Configuration de l'intégration
 */
struct DreamIntegrationConfig {
    // RabbitMQ
    std::string rabbitmq_host = "localhost";
    int rabbitmq_port = 5672;
    std::string rabbitmq_user = "virtus";
    std::string rabbitmq_password = "virtus@83";
    
    // Queues d'entrée
    std::string emotions_queue = "mcee.emotional.input";
    std::string speech_queue = "mcee.speech.input";
    std::string mct_events_queue = "mcee.mct.events";
    
    // Queues de sortie
    std::string dream_status_queue = "mcee.dream.status";
    
    // Cycle de rêve
    double cycle_period_s = 12.0 * 60.0 * 60.0;  // 12h
    double min_time_since_last_dream_s = 9.0 * 60.0 * 60.0;  // 9h
    
    // Tick rate
    int tick_interval_ms = 1000;  // 1 seconde
};

/**
 * @class DreamIntegration
 * @brief Orchestre le DreamEngine avec le reste de MCEE
 * 
 * Cette classe fait le pont entre:
 * - Le DreamEngine (consolidation nocturne)
 * - Le PatternMatcher (pattern émotionnel actif)
 * - Amyghaleon (interruptions d'urgence)
 * - La MCT existante (source des souvenirs)
 * - Neo4j via RabbitMQ (stockage MLT)
 */
class DreamIntegration {
public:
    // Callbacks pour recevoir l'état depuis les autres composants
    using PatternCallback = std::function<std::string()>;  // Retourne le pattern actif
    using EmotionCallback = std::function<std::array<double, 24>()>;  // Retourne état émotionnel
    using AmyghaleonCallback = std::function<bool()>;  // Retourne true si alerte active
    
    explicit DreamIntegration(const DreamIntegrationConfig& config = DreamIntegrationConfig{})
        : config_(config)
        , running_(false)
    {
        // Configurer le DreamEngine
        DreamConfig dreamConfig;
        dreamConfig.cyclePeriod_s = config.cycle_period_s;
        dreamConfig.minTimeSinceLastDream_s = config.min_time_since_last_dream_s;
        
        dreamEngine_ = std::make_unique<DreamEngine>(dreamConfig);
        
        // Configurer le bridge Neo4j
        DreamRabbitMQConfig bridgeConfig;
        bridgeConfig.host = config.rabbitmq_host;
        bridgeConfig.port = config.rabbitmq_port;
        bridgeConfig.user = config.rabbitmq_user;
        bridgeConfig.password = config.rabbitmq_password;
        
        neo4jBridge_ = std::make_unique<DreamNeo4jBridge>(bridgeConfig);
        neo4jBridge_->connectToDreamEngine(*dreamEngine_);
    }
    
    ~DreamIntegration() {
        stop();
    }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // CONNEXION AUX COMPOSANTS MCEE
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Définit le callback pour obtenir le pattern actif du PatternMatcher
     * @param callback Fonction retournant le nom du pattern ("SERENITE", "PEUR", etc.)
     */
    void setPatternCallback(PatternCallback callback) {
        patternCallback_ = std::move(callback);
    }
    
    /**
     * @brief Définit le callback pour obtenir l'état émotionnel actuel
     * @param callback Fonction retournant le vecteur émotionnel 24D
     */
    void setEmotionCallback(EmotionCallback callback) {
        emotionCallback_ = std::move(callback);
    }
    
    /**
     * @brief Définit le callback pour vérifier les alertes Amyghaleon
     * @param callback Fonction retournant true si une urgence est active
     */
    void setAmyghaleonCallback(AmyghaleonCallback callback) {
        amyghaleonCallback_ = std::move(callback);
    }
    
    /**
     * @brief Intégration directe avec le PatternMatcher MCEE
     * 
     * Exemple d'utilisation:
     * @code
     * auto patternMatcher = std::make_shared<mcee::PatternMatcher>(mct, mlt);
     * dreamIntegration.connectPatternMatcher(patternMatcher);
     * @endcode
     */
    template<typename PatternMatcherType>
    void connectPatternMatcher(std::shared_ptr<PatternMatcherType> matcher) {
        patternCallback_ = [matcher]() -> std::string {
            auto result = matcher->match();
            return result.pattern_name;
        };
        std::cout << "[DreamIntegration] PatternMatcher connecté\n";
    }
    
    /**
     * @brief Intégration directe avec Amyghaleon
     * 
     * Exemple d'utilisation:
     * @code
     * auto amyghaleon = std::make_shared<mcee::Amyghaleon>();
     * dreamIntegration.connectAmyghaleon(amyghaleon, emotionalState, activeMemories, threshold);
     * @endcode
     */
    template<typename AmyghaleonType, typename StateType, typename MemoriesType>
    void connectAmyghaleon(std::shared_ptr<AmyghaleonType> amyghaleon,
                          std::function<StateType()> getState,
                          std::function<MemoriesType()> getMemories,
                          std::function<double()> getThreshold) {
        amyghaleonCallback_ = [amyghaleon, getState, getMemories, getThreshold]() -> bool {
            return amyghaleon->checkEmergency(getState(), getMemories(), getThreshold());
        };
        std::cout << "[DreamIntegration] Amyghaleon connecté\n";
    }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // RÉCEPTION DES SOUVENIRS
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Ajoute un souvenir directement (sans RabbitMQ)
     */
    void addMemory(const Memory& memory) {
        dreamEngine_->addMemoryToMCT(memory);
    }
    
    /**
     * @brief Convertit un JSON en Memory et l'ajoute
     */
    void addMemoryFromJson(const json& j) {
        Memory memory;
        memory.id = j.value("id", "");
        memory.type = j.value("type", "episodic");
        memory.isSocial = j.value("is_social", false);
        memory.interlocuteur = j.value("interlocuteur", "");
        memory.contexte = j.value("contexte", "");
        memory.feedback = j.value("feedback", 0.0);
        memory.usageCount = j.value("usage_count", 0);
        memory.decisionalInfluence = j.value("decisional_influence", 0.0);
        memory.isTrauma = j.value("is_trauma", false);
        memory.timestamp = std::chrono::steady_clock::now();
        
        if (j.contains("emotional_vector") && j["emotional_vector"].is_array()) {
            const auto& ev = j["emotional_vector"];
            for (size_t i = 0; i < std::min(ev.size(), (size_t)24); ++i) {
                memory.emotionalVector[i] = ev[i].get<double>();
            }
        }
        
        addMemory(memory);
    }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // DÉMARRAGE / ARRÊT
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Démarre la boucle principale d'intégration
     */
    void start() {
        if (running_) return;
        
        running_ = true;
        
        // Démarrer le thread de tick
        tickThread_ = std::thread([this]() {
            while (running_) {
                tick();
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(config_.tick_interval_ms)
                );
            }
        });
        
        // Démarrer le consumer RabbitMQ (optionnel)
        startRabbitMQConsumer();
        
        std::cout << "[DreamIntegration] Démarré (tick=" 
                  << config_.tick_interval_ms << "ms)\n";
    }
    
    /**
     * @brief Arrête la boucle principale
     */
    void stop() {
        running_ = false;
        
        if (tickThread_.joinable()) {
            tickThread_.join();
        }
        
        if (consumerThread_.joinable()) {
            consumerThread_.join();
        }
        
        std::cout << "[DreamIntegration] Arrêté\n";
    }
    
    /**
     * @brief Force le démarrage du rêve (pour tests)
     */
    void forceDreamStart() {
        dreamEngine_->forceDreamStart();
    }
    
    /**
     * @brief Interrompt le rêve en cours
     */
    void interruptDream() {
        dreamEngine_->interruptDream();
    }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // ACCESSEURS
    // ═══════════════════════════════════════════════════════════════════════════
    
    DreamEngine& getDreamEngine() { return *dreamEngine_; }
    const DreamEngine& getDreamEngine() const { return *dreamEngine_; }
    
    DreamState getCurrentState() const {
        return dreamEngine_->getCurrentState();
    }
    
    DreamEngine::Stats getStats() const {
        return dreamEngine_->getStats();
    }
    
    bool isRunning() const { return running_; }

private:
    DreamIntegrationConfig config_;
    std::unique_ptr<DreamEngine> dreamEngine_;
    std::unique_ptr<DreamNeo4jBridge> neo4jBridge_;
    
    // Callbacks vers les composants MCEE
    PatternCallback patternCallback_;
    EmotionCallback emotionCallback_;
    AmyghaleonCallback amyghaleonCallback_;
    
    // Threads
    std::atomic<bool> running_;
    std::thread tickThread_;
    std::thread consumerThread_;
    
    AmqpClient::Channel::ptr_t consumerChannel_;
    
    /**
     * @brief Tick principal - met à jour le DreamEngine
     */
    void tick() {
        // Obtenir le pattern actif
        std::string activePattern = "SERENITE";
        if (patternCallback_) {
            activePattern = patternCallback_();
        }
        
        // Obtenir l'état émotionnel
        std::array<double, 24> emotionalState{};
        if (emotionCallback_) {
            emotionalState = emotionCallback_();
        }
        
        // Vérifier les alertes Amyghaleon
        bool amyghaleonAlert = false;
        if (amyghaleonCallback_) {
            amyghaleonAlert = amyghaleonCallback_();
        }
        
        // Mettre à jour le DreamEngine
        dreamEngine_->update(emotionalState, activePattern, amyghaleonAlert);
    }
    
    /**
     * @brief Démarre le consumer RabbitMQ pour les événements MCT
     */
    void startRabbitMQConsumer() {
        try {
            consumerChannel_ = AmqpClient::Channel::Create(
                config_.rabbitmq_host,
                config_.rabbitmq_port,
                config_.rabbitmq_user,
                config_.rabbitmq_password
            );
            
            // Déclarer et bind la queue des événements MCT
            consumerChannel_->DeclareQueue(config_.mct_events_queue, false, true, false, false);
            
            consumerThread_ = std::thread([this]() {
                std::string consumer_tag = consumerChannel_->BasicConsume(
                    config_.mct_events_queue,
                    "",     // consumer tag auto
                    true,   // no_local
                    true,   // no_ack
                    false   // exclusive
                );
                
                while (running_) {
                    try {
                        AmqpClient::Envelope::ptr_t envelope;
                        if (consumerChannel_->BasicConsumeMessage(consumer_tag, envelope, 100)) {
                            handleMCTEvent(envelope->Message()->Body());
                        }
                    } catch (const std::exception& e) {
                        if (running_) {
                            std::cerr << "[DreamIntegration] Erreur consumer: " << e.what() << "\n";
                        }
                    }
                }
            });
            
            std::cout << "[DreamIntegration] Consumer RabbitMQ démarré\n";
            
        } catch (const std::exception& e) {
            std::cerr << "[DreamIntegration] Erreur démarrage consumer: " << e.what() << "\n";
        }
    }
    
    /**
     * @brief Traite un événement MCT reçu via RabbitMQ
     */
    void handleMCTEvent(const std::string& body) {
        try {
            json event = json::parse(body);
            
            std::string eventType = event.value("event", "");
            
            if (eventType == "memory_created") {
                // Nouveau souvenir créé dans la MCT
                addMemoryFromJson(event["data"]);
                
            } else if (eventType == "memory_activated") {
                // Souvenir réactivé - incrémenter usage_count
                std::string memoryId = event["data"].value("id", "");
                // TODO: Mettre à jour le compteur dans le buffer MCT du DreamEngine
                
            } else if (eventType == "trauma_detected") {
                // Trauma détecté - marquer le souvenir
                // Le flag isTrauma sera utilisé lors de la consolidation
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[DreamIntegration] Erreur parsing événement: " << e.what() << "\n";
        }
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// HELPER: Connexion rapide avec les composants MCEE existants
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Crée une intégration complète avec tous les composants
 * 
 * Exemple d'utilisation dans le main MCEE:
 * @code
 * auto mct = std::make_shared<mcee::MCT>();
 * auto mlt = std::make_shared<mcee::MLT>();
 * auto patternMatcher = std::make_shared<mcee::PatternMatcher>(mct, mlt);
 * auto amyghaleon = std::make_shared<mcee::Amyghaleon>();
 * 
 * auto dreamIntegration = createDreamIntegration(
 *     patternMatcher,
 *     amyghaleon,
 *     [&engine]() { return engine.getCurrentEmotionalState(); },
 *     [&engine]() { return engine.getActiveMemories(); },
 *     [&engine]() { return engine.getEmergencyThreshold(); }
 * );
 * 
 * dreamIntegration->start();
 * @endcode
 */
template<typename PatternMatcherPtr, typename AmyghaleonPtr, 
         typename StateGetter, typename MemoriesGetter, typename ThresholdGetter>
std::unique_ptr<DreamIntegration> createDreamIntegration(
    PatternMatcherPtr patternMatcher,
    AmyghaleonPtr amyghaleon,
    StateGetter getState,
    MemoriesGetter getMemories,
    ThresholdGetter getThreshold,
    const DreamIntegrationConfig& config = DreamIntegrationConfig{})
{
    auto integration = std::make_unique<DreamIntegration>(config);
    
    // Connecter PatternMatcher
    integration->setPatternCallback([patternMatcher]() -> std::string {
        auto result = patternMatcher->match();
        return result.pattern_name;
    });
    
    // Connecter Amyghaleon
    integration->setAmyghaleonCallback([amyghaleon, getState, getMemories, getThreshold]() -> bool {
        return amyghaleon->checkEmergency(getState(), getMemories(), getThreshold());
    });
    
    // Connecter état émotionnel
    integration->setEmotionCallback([getState]() -> std::array<double, 24> {
        auto state = getState();
        std::array<double, 24> result{};
        // Adapter selon le type de EmotionalState dans ton projet
        // result = state.emotions;
        return result;
    });
    
    return integration;
}

} // namespace MCEE
