/**
 * @file MCEEEngine.cpp
 * @brief Implémentation du moteur principal MCEE
 * @version 2.0
 * @date 2025-12-19
 */

#include "MCEEEngine.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <fstream>

namespace mcee {

MCEEEngine::MCEEEngine(const RabbitMQConfig& rabbitmq_config)
    : rabbitmq_config_(rabbitmq_config)
    , phase_detector_(DEFAULT_HYSTERESIS_MARGIN, DEFAULT_MIN_PHASE_DURATION)
    , last_update_time_(std::chrono::steady_clock::now())
{
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           MCEE - Modèle Complet d'Évaluation des États       ║\n";
    std::cout << "║                        Version 2.0                            ║\n";
    std::cout << "║         Avec intégration Module Émotions + Parole            ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    // Configurer les callbacks
    phase_detector_.setTransitionCallback(
        [this](Phase from, Phase to, double duration) {
            stats_.previous_phase = from;
            stats_.current_phase = to;
            stats_.phase_duration = 0.0;
            stats_.phase_transitions++;
            
            // Mettre à jour les coefficients
            emotion_updater_.setCoefficientsFromPhase(phase_detector_.getCurrentConfig());
        }
    );

    amyghaleon_.setEmergencyCallback(
        [this](const EmergencyResponse& response) {
            executeEmergencyAction(response);
            stats_.emergency_triggers++;
        }
    );

    // Configurer le callback d'urgence du module parole
    speech_input_.setUrgencyCallback(
        [this](const std::string& text, double urgency) {
            std::cout << "[MCEEEngine] ⚠ Urgence détectée dans le texte (score=" 
                      << std::fixed << std::setprecision(2) << urgency << ")\n";
            // Augmenter le feedback interne en cas d'urgence textuelle
            current_feedback_.internal = std::max(current_feedback_.internal, urgency * 0.5);
        }
    );

    std::cout << "[MCEEEngine] Moteur initialisé avec système de phases + parole\n";
}

MCEEEngine::~MCEEEngine() {
    stop();
}

bool MCEEEngine::start() {
    if (running_.load()) {
        std::cout << "[MCEEEngine] Déjà en cours d'exécution\n";
        return true;
    }

    // Initialiser RabbitMQ
    if (!initRabbitMQ()) {
        std::cerr << "[MCEEEngine] Échec initialisation RabbitMQ\n";
        return false;
    }

    running_.store(true);
    
    // Démarrer les threads de consommation
    emotions_consumer_thread_ = std::thread(&MCEEEngine::emotionsConsumeLoop, this);
    speech_consumer_thread_ = std::thread(&MCEEEngine::speechConsumeLoop, this);

    std::cout << "[MCEEEngine] ✓ Démarré et en attente de messages RabbitMQ\n";
    std::cout << "[MCEEEngine] Émotions: " << rabbitmq_config_.emotions_exchange 
              << " / " << rabbitmq_config_.emotions_routing_key << "\n";
    std::cout << "[MCEEEngine] Parole: " << rabbitmq_config_.speech_exchange 
              << " / " << rabbitmq_config_.speech_routing_key << "\n\n";

    return true;
}

void MCEEEngine::stop() {
    if (!running_.load()) {
        return;
    }

    running_.store(false);

    if (emotions_consumer_thread_.joinable()) {
        emotions_consumer_thread_.join();
    }
    
    if (speech_consumer_thread_.joinable()) {
        speech_consumer_thread_.join();
    }

    std::cout << "[MCEEEngine] Arrêté\n";
    std::cout << "[MCEEEngine] Statistiques finales:\n";
    std::cout << "  - Transitions de phase: " << stats_.phase_transitions << "\n";
    std::cout << "  - Urgences déclenchées: " << stats_.emergency_triggers << "\n";
    std::cout << "  - Souvenirs créés: " << memory_manager_.getMemoryCount() << "\n";
    std::cout << "  - Traumas: " << memory_manager_.getTraumaCount() << "\n";
    std::cout << "  - Textes traités: " << speech_input_.getProcessedCount() << "\n";
    std::cout << "  - Sentiment moyen: " << std::fixed << std::setprecision(2) 
              << speech_input_.getAverageSentiment() << "\n";
}

bool MCEEEngine::initRabbitMQ() {
    try {
        AmqpClient::Channel::OpenOpts opts;
        opts.host = rabbitmq_config_.host;
        opts.port = rabbitmq_config_.port;
        opts.auth = AmqpClient::Channel::OpenOpts::BasicAuth{
            rabbitmq_config_.user, 
            rabbitmq_config_.password
        };

        channel_ = AmqpClient::Channel::Open(opts);

        // === Exchanges ===
        
        // Exchange pour les émotions (entrée)
        channel_->DeclareExchange(
            rabbitmq_config_.emotions_exchange,
            AmqpClient::Channel::EXCHANGE_TYPE_TOPIC,
            false, true, false
        );

        // Exchange pour la parole (entrée)
        channel_->DeclareExchange(
            rabbitmq_config_.speech_exchange,
            AmqpClient::Channel::EXCHANGE_TYPE_TOPIC,
            false, true, false
        );

        // Exchange pour la sortie
        channel_->DeclareExchange(
            rabbitmq_config_.output_exchange,
            AmqpClient::Channel::EXCHANGE_TYPE_TOPIC,
            false, true, false
        );

        // === Queues ===
        
        // Queue pour les émotions
        std::string emotions_queue = channel_->DeclareQueue(
            "mcee_emotions_queue", false, true, false, false
        );
        channel_->BindQueue(
            emotions_queue,
            rabbitmq_config_.emotions_exchange,
            rabbitmq_config_.emotions_routing_key
        );
        emotions_consumer_tag_ = channel_->BasicConsume(
            emotions_queue, "", true, false, false, 1
        );

        // Queue pour la parole
        std::string speech_queue = channel_->DeclareQueue(
            "mcee_speech_queue", false, true, false, false
        );
        channel_->BindQueue(
            speech_queue,
            rabbitmq_config_.speech_exchange,
            rabbitmq_config_.speech_routing_key
        );
        speech_consumer_tag_ = channel_->BasicConsume(
            speech_queue, "", true, false, false, 1
        );

        std::cout << "[MCEEEngine] Connexion RabbitMQ établie\n";
        std::cout << "[MCEEEngine] Queues créées: emotions + speech\n";
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[MCEEEngine] Erreur RabbitMQ: " << e.what() << "\n";
        return false;
    }
}

void MCEEEngine::emotionsConsumeLoop() {
    std::cout << "[MCEEEngine] Boucle de consommation des émotions démarrée\n";

    while (running_.load()) {
        try {
            AmqpClient::Envelope::ptr_t envelope;
            bool received = channel_->BasicConsumeMessage(emotions_consumer_tag_, envelope, 500);

            if (received && envelope) {
                std::string body(envelope->Message()->Body().begin(),
                                envelope->Message()->Body().end());
                
                handleEmotionMessage(body);
                channel_->BasicAck(envelope);
            }

        } catch (const std::exception& e) {
            std::cerr << "[MCEEEngine] Erreur consommation émotions: " << e.what() << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void MCEEEngine::speechConsumeLoop() {
    std::cout << "[MCEEEngine] Boucle de consommation de la parole démarrée\n";

    while (running_.load()) {
        try {
            AmqpClient::Envelope::ptr_t envelope;
            bool received = channel_->BasicConsumeMessage(speech_consumer_tag_, envelope, 500);

            if (received && envelope) {
                std::string body(envelope->Message()->Body().begin(),
                                envelope->Message()->Body().end());
                
                handleSpeechMessage(body);
                channel_->BasicAck(envelope);
            }

        } catch (const std::exception& e) {
            std::cerr << "[MCEEEngine] Erreur consommation parole: " << e.what() << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void MCEEEngine::handleEmotionMessage(const std::string& body) {
    try {
        json input = json::parse(body);
        
        std::unordered_map<std::string, double> raw_emotions;
        for (const auto& name : EMOTION_NAMES) {
            if (input.contains(name)) {
                raw_emotions[name] = input[name].get<double>();
            } else {
                raw_emotions[name] = 0.0;
            }
        }

        processEmotions(raw_emotions);

    } catch (const std::exception& e) {
        std::cerr << "[MCEEEngine] Erreur parsing JSON émotions: " << e.what() << "\n";
    }
}

void MCEEEngine::handleSpeechMessage(const std::string& body) {
    try {
        json input = json::parse(body);
        
        std::string text = input.value("text", "");
        std::string source = input.value("source", "user");
        double confidence = input.value("confidence", 1.0);

        if (!text.empty()) {
            TextInput text_input;
            text_input.text = text;
            text_input.source = source;
            text_input.confidence = confidence;
            
            // Traiter le texte
            last_speech_analysis_ = speech_input_.processText(text_input);
            
            // Mettre à jour le feedback externe basé sur le texte
            double fb_ext = speech_input_.computeFeedbackExternal(last_speech_analysis_);
            
            {
                std::lock_guard<std::mutex> lock(state_mutex_);
                // Combiner avec le feedback existant (moyenne pondérée)
                current_feedback_.external = current_feedback_.external * 0.3 + fb_ext * 0.7;
            }

            std::cout << "[MCEEEngine] Texte traité, feedback externe ajusté: " 
                      << std::fixed << std::setprecision(2) << current_feedback_.external << "\n";
        }

    } catch (const std::exception& e) {
        std::cerr << "[MCEEEngine] Erreur parsing JSON parole: " << e.what() << "\n";
    }
}

void MCEEEngine::processEmotions(const std::unordered_map<std::string, double>& raw_emotions) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    // Sauvegarder l'état précédent
    previous_state_ = current_state_;

    // Convertir en EmotionalState
    current_state_ = rawToState(raw_emotions);

    // Exécuter le pipeline complet
    processPipeline(raw_emotions);
}

void MCEEEngine::processSpeechText(const std::string& text, const std::string& source) {
    // Traiter le texte via SpeechInput
    last_speech_analysis_ = speech_input_.processText(text, source);
    
    // Mettre à jour le feedback externe
    double fb_ext = speech_input_.computeFeedbackExternal(last_speech_analysis_);
    
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        current_feedback_.external = current_feedback_.external * 0.3 + fb_ext * 0.7;
    }
    
    // Créer un contexte pour le souvenir si le texte est significatif
    if (std::abs(last_speech_analysis_.sentiment_score) > 0.3 || 
        last_speech_analysis_.urgency_score > 0.5) {
        std::string context = speech_input_.generateMemoryContext(last_speech_analysis_);
        memory_manager_.recordMemory(current_state_, phase_detector_.getCurrentPhase(), context);
    }

    std::cout << "[MCEEEngine] Texte traité: sentiment=" 
              << std::fixed << std::setprecision(2) << last_speech_analysis_.sentiment_score
              << ", fb_ext=" << current_feedback_.external << "\n";
}

void MCEEEngine::processPipeline(const std::unordered_map<std::string, double>& raw_emotions) {
    // 1. DÉTECTION DE PHASE
    Phase previous_phase = phase_detector_.getCurrentPhase();
    Phase current_phase = phase_detector_.detectPhase(current_state_);

    if (current_phase != previous_phase) {
        std::cout << "\n[MCEEEngine] ═══ Transition de phase: " 
                  << phaseToString(previous_phase) << " → " 
                  << phaseToString(current_phase) << " ═══\n\n";
    }

    // 2. RÉCUPÉRER LA CONFIGURATION DE PHASE
    const auto& phase_config = phase_detector_.getCurrentConfig();

    // 3. METTRE À JOUR LES COEFFICIENTS
    emotion_updater_.setCoefficientsFromPhase(phase_config);

    // 4. RÉCUPÉRER LES SOUVENIRS PERTINENTS
    auto memories = memory_manager_.queryRelevantMemories(current_phase, current_state_, 10);

    // Mettre à jour les activations des souvenirs
    for (auto& mem : memories) {
        memory_manager_.updateActivation(mem, current_state_);
    }

    // 5. VÉRIFIER AMYGHALEON (court-circuit)
    if (amyghaleon_.checkEmergency(current_state_, memories, phase_config.amyghaleon_threshold)) {
        auto response = amyghaleon_.triggerEmergencyResponse(current_state_, current_phase);
        
        // En phase PEUR, créer un trauma potentiel
        if (current_phase == Phase::PEUR) {
            memory_manager_.createPotentialTrauma(current_state_);
        }
        
        // Publier l'état d'urgence
        publishState();
        return;  // Court-circuit
    }

    // 6. CALCULER LE DELTA TEMPS
    auto now = std::chrono::steady_clock::now();
    double delta_t = std::chrono::duration<double>(now - last_update_time_).count();
    last_update_time_ = now;

    // 7. CALCULER L'INFLUENCE DES SOUVENIRS
    auto memory_influences = memory_manager_.computeMemoryInfluences(
        memories, phase_config.delta
    );

    // 8. METTRE À JOUR LA SAGESSE
    updateWisdom();

    // 9. METTRE À JOUR LES ÉMOTIONS
    emotion_updater_.updateAllEmotions(
        current_state_,
        current_feedback_,
        delta_t,
        memory_influences,
        wisdom_
    );

    // 10. CALCULER LA VARIANCE GLOBALE
    current_state_.variance_global = emotion_updater_.computeGlobalVariance(
        current_state_, memories
    );

    // 11. CALCULER E_GLOBAL
    current_state_.E_global = emotion_updater_.computeEGlobal(
        current_state_,
        previous_state_.E_global,
        current_state_.variance_global
    );

    // 12. GÉRER LES BOUCLES EN PHASE PEUR
    if (current_phase == Phase::PEUR) {
        handleFearLoop();
    }

    // 13. ENREGISTRER UN SOUVENIR SI SIGNIFICATIF
    if (current_state_.getMeanIntensity() > 0.5) {
        auto [dominant, value] = current_state_.getDominant();
        memory_manager_.recordMemory(current_state_, current_phase, 
            "Auto_" + dominant + "_" + std::to_string(now.time_since_epoch().count()));
    }

    // 14. PUBLIER L'ÉTAT
    publishState();

    // 15. CALLBACK
    if (on_state_change_) {
        on_state_change_(current_state_, current_phase);
    }

    // Afficher l'état
    printState();
}

void MCEEEngine::printState() const {
    auto [dominant, value] = current_state_.getDominant();
    
    std::cout << "\n[MCEEEngine] État émotionnel mis à jour:\n";
    std::cout << "  Phase     : " << phaseToString(phase_detector_.getCurrentPhase()) << "\n";
    std::cout << "  Dominant  : " << dominant << " = " 
              << std::fixed << std::setprecision(3) << value << "\n";
    std::cout << "  E_global  : " << std::fixed << std::setprecision(3) 
              << current_state_.E_global << "\n";
    std::cout << "  Variance  : " << std::fixed << std::setprecision(3) 
              << current_state_.variance_global << "\n";
    std::cout << "  Valence   : " << std::fixed << std::setprecision(3) 
              << current_state_.getValence() << "\n";
    std::cout << "  Intensité : " << std::fixed << std::setprecision(3) 
              << current_state_.getMeanIntensity() << "\n\n";
}

void MCEEEngine::publishState() {
    if (!channel_) return;

    try {
        json output;
        
        // Émotions
        for (size_t i = 0; i < NUM_EMOTIONS; ++i) {
            output["emotions"][EMOTION_NAMES[i]] = current_state_.emotions[i];
        }

        // Méta-données
        output["E_global"] = current_state_.E_global;
        output["variance_global"] = current_state_.variance_global;
        output["valence"] = current_state_.getValence();
        output["intensity"] = current_state_.getMeanIntensity();
        
        auto [dominant, value] = current_state_.getDominant();
        output["dominant"] = dominant;
        output["dominant_value"] = value;

        // Phase
        output["phase"] = phaseToString(phase_detector_.getCurrentPhase());
        output["phase_duration"] = phase_detector_.getPhaseDuration();

        // Coefficients actifs
        const auto& config = phase_detector_.getCurrentConfig();
        output["coefficients"]["alpha"] = config.alpha;
        output["coefficients"]["beta"] = config.beta;
        output["coefficients"]["gamma"] = config.gamma;
        output["coefficients"]["delta"] = config.delta;
        output["coefficients"]["theta"] = config.theta;

        // Statistiques
        output["stats"]["phase_transitions"] = stats_.phase_transitions;
        output["stats"]["emergency_triggers"] = stats_.emergency_triggers;
        output["stats"]["wisdom"] = wisdom_;

        std::string body = output.dump();
        channel_->BasicPublish(
            rabbitmq_config_.output_exchange,
            rabbitmq_config_.output_routing_key,
            AmqpClient::BasicMessage::Create(body),
            false, false
        );

    } catch (const std::exception& e) {
        std::cerr << "[MCEEEngine] Erreur publication: " << e.what() << "\n";
    }
}

void MCEEEngine::setFeedback(double external, double internal) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    current_feedback_.external = std::clamp(external, -1.0, 1.0);
    current_feedback_.internal = std::clamp(internal, -1.0, 1.0);
}

void MCEEEngine::setStateCallback(StateCallback callback) {
    on_state_change_ = std::move(callback);
}

void MCEEEngine::updateWisdom() {
    // La sagesse augmente avec l'expérience (nombre de transitions)
    // et diminue en phase PEUR
    Phase phase = phase_detector_.getCurrentPhase();
    const auto& config = phase_detector_.getCurrentConfig();

    if (phase == Phase::PEUR) {
        wisdom_ *= 0.95;  // Décroissance en phase PEUR
    } else {
        wisdom_ += config.learning_rate * 0.001;  // Croissance lente
    }

    wisdom_ = std::clamp(wisdom_, 0.0, 1.0);
    stats_.wisdom = wisdom_;
}

void MCEEEngine::handleFearLoop() {
    double phase_duration = phase_detector_.getPhaseDuration();

    // Si en phase PEUR depuis > 60 secondes, forcer décroissance
    if (phase_duration > 60.0) {
        current_state_.setEmotion("Peur", 
            current_state_.getEmotion("Peur") * 0.95);
        current_state_.setEmotion("Horreur", 
            current_state_.getEmotion("Horreur") * 0.95);

        std::cout << "[MCEEEngine] Décroissance forcée des émotions de peur "
                  << "(durée: " << std::fixed << std::setprecision(1) 
                  << phase_duration << "s)\n";
    }

    // Si > 5 minutes, forcer transition vers ANXIÉTÉ
    if (phase_duration > 300.0) {
        double peur = current_state_.getEmotion("Peur");
        double horreur = current_state_.getEmotion("Horreur");
        
        if (std::max(peur, horreur) < 0.6) {
            phase_detector_.forceTransition(Phase::ANXIETE, "FEAR_TIMEOUT");
        }
    }
}

void MCEEEngine::executeEmergencyAction(const EmergencyResponse& response) {
    std::cout << "\n[MCEEEngine] ⚡ Exécution action d'urgence: " << response.action << "\n";

    // Actions spécifiques selon le type
    if (response.action == "FUITE") {
        // Préparer le système pour une réponse de fuite
        current_feedback_.internal = 0.8;  // Stress élevé
    } else if (response.action == "BLOCAGE") {
        // Bloquer temporairement les mises à jour
        current_feedback_.internal = 1.0;
    } else if (response.action == "ALERTE") {
        // Mode alerte
        current_feedback_.internal = 0.5;
    }
}

void MCEEEngine::forcePhaseTransition(Phase phase, const std::string& reason) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    phase_detector_.forceTransition(phase, reason);
    emotion_updater_.setCoefficientsFromPhase(phase_detector_.getCurrentConfig());
}

bool MCEEEngine::loadConfig(const std::string& config_path) {
    bool success = phase_detector_.loadConfig(config_path);
    if (success) {
        emotion_updater_.setCoefficientsFromPhase(phase_detector_.getCurrentConfig());
    }
    return success;
}

EmotionalState MCEEEngine::rawToState(
    const std::unordered_map<std::string, double>& raw) const 
{
    EmotionalState state;
    
    for (size_t i = 0; i < NUM_EMOTIONS; ++i) {
        auto it = raw.find(EMOTION_NAMES[i]);
        if (it != raw.end()) {
            state.emotions[i] = std::clamp(it->second, 0.0, 1.0);
        }
    }

    return state;
}

} // namespace mcee
