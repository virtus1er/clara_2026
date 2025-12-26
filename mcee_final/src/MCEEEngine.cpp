/**
 * @file MCEEEngine.cpp
 * @brief Implémentation du moteur principal MCEE
 * @version 3.0
 * @date 2024
 * 
 * Version 3.0 : Architecture MCT/MLT avec patterns dynamiques
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
    , pattern_start_time_(std::chrono::steady_clock::now())
{
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           MCEE - Modèle Complet d'Évaluation des États       ║\n";
    std::cout << "║                        Version 3.0                            ║\n";
    std::cout << "║              Architecture MCT/MLT - Patterns Dynamiques       ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    // Initialiser le système MCT/MLT
    initMemorySystem();

    // Configurer les callbacks legacy (PhaseDetector)
    phase_detector_.setTransitionCallback(
        [this](Phase from, Phase to, double duration) {
            stats_.previous_phase = from;
            stats_.current_phase = to;
            stats_.phase_duration = 0.0;
            stats_.phase_transitions++;
            
            emotion_updater_.setCoefficientsFromPhase(phase_detector_.getCurrentConfig());
        }
    );

    // Configurer Amyghaleon
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
            current_feedback_.internal = std::max(current_feedback_.internal, urgency * 0.5);
        }
    );

    // Configurer les callbacks MCT/MLT
    setupCallbacks();

    std::cout << "[MCEEEngine] Moteur v3.0 initialisé avec MCT/MLT + Parole\n";
}

void MCEEEngine::initMemorySystem() {
    // Créer la MCT
    MCTConfig mct_config;
    mct_config.max_size = 60;
    mct_config.time_window_seconds = 30.0;
    mct_config.decay_factor = 0.95;
    mct_config.min_samples_for_signature = 5;
    mct_ = std::make_shared<MCT>(mct_config);

    // Créer le MCTGraph (graphe relationnel mots-émotions)
    MCTGraphConfig graph_config;
    graph_config.time_window_seconds = 120.0;  // 2 minutes
    graph_config.emotion_persistence_threshold_seconds = 2.0;
    graph_config.causality_threshold_ms = 500.0;
    graph_config.slow_emotion_causality_threshold_ms = 800.0;
    graph_config.snapshot_interval_seconds = 10.0;
    mct_graph_ = std::make_shared<MCTGraph>(graph_config);

    // Configurer le callback de snapshot
    mct_graph_->setSnapshotCallback([this](const MCTGraphSnapshot& snapshot) {
        publishSnapshot(snapshot);
    });

    // Configurer le callback de détection causale
    mct_graph_->setCausalDetectionCallback(
        [](const std::string& word_id, const std::string& emotion_id, double strength) {
            std::cout << "[MCTGraph] Causalité détectée: " << word_id
                      << " → " << emotion_id << " (force="
                      << std::fixed << std::setprecision(2) << strength << ")\n";
        }
    );

    // Créer la MLT avec les patterns de base
    MLTConfig mlt_config;
    mlt_config.min_similarity_threshold = 0.6;
    mlt_config.high_similarity_threshold = 0.85;
    mlt_config.learning_rate = 0.1;
    mlt_config.max_patterns = 100;
    mlt_ = std::make_shared<MLT>(mlt_config);

    // Créer le PatternMatcher
    PatternMatcherConfig pm_config;
    pm_config.high_match_threshold = 0.85;
    pm_config.medium_match_threshold = 0.6;
    pm_config.low_match_threshold = 0.4;
    pm_config.hysteresis_margin = 0.1;
    pm_config.min_frames_before_switch = 3;
    pm_config.verbose_logging = true;
    pattern_matcher_ = std::make_shared<PatternMatcher>(mct_, mlt_, pm_config);

    // Créer le ConscienceEngine (module Conscience & Sentiments)
    ConscienceConfig conscience_config;
    conscience_config.beta_memory = 0.15;
    conscience_config.delta_environment = 0.1;
    conscience_config.omega_trauma = 5.0;
    conscience_config.lambda_feedback = 0.2;
    conscience_config.sentiment_smoothing = 0.1;
    conscience_engine_ = std::make_shared<ConscienceEngine>(conscience_config);

    // Configurer les callbacks du ConscienceEngine
    conscience_engine_->setUpdateCallback([this](const ConscienceSentimentState& state) {
        std::cout << "[Conscience] Ct=" << std::fixed << std::setprecision(2)
                  << state.consciousness_level << " Ft=" << state.sentiment
                  << " (" << state.dominant_state << ")\n";
    });

    conscience_engine_->setTraumaAlertCallback([this](const TraumaState& trauma) {
        std::cout << "[Conscience] ⚠ Trauma actif: " << trauma.source
                  << " (intensité=" << trauma.intensity << ")\n";
        // Relier au système Amyghaleon si nécessaire
        if (trauma.intensity >= 0.8) {
            Phase current_phase = phase_detector_.getCurrentPhase();
            auto memories = memory_manager_.queryRelevantMemories(current_phase, current_state_, 5);
            if (amyghaleon_.checkEmergency(current_state_, memories, 0.5)) {
                (void)amyghaleon_.triggerEmergencyResponse(current_state_, current_phase);
            }
        }
    });

    // Créer le module ADDO (Détermination des Objectifs)
    ADDOConfig addo_config;
    addo_config.use_wisdom_modulation = true;
    addo_config.use_sentiment_for_S = true;
    addo_config.emergency_override = true;
    addo_engine_ = std::make_shared<ADDOEngine>(addo_config);

    // Configurer les callbacks ADDO
    addo_engine_->setUpdateCallback([](const GoalState& state) {
        std::cout << "[ADDO] G(t)=" << std::fixed << std::setprecision(2)
                  << state.G << " (dominant: " << state.dominant_variable << ")\n";
    });

    addo_engine_->setEmergencyCallback([this](const std::string& emergency_goal) {
        std::cout << "[ADDO] ⚡ Objectif d'urgence: " << emergency_goal << "\n";
    });

    // Créer le module de Prise de Décision Réfléchie
    DecisionConfig decision_config;
    decision_config.tau_max_ms = 5000.0;
    decision_config.theta_veto = 0.80;
    decision_config.enable_meta_actions = true;
    decision_engine_ = std::make_shared<DecisionEngine>(decision_config);

    // Configurer les callbacks Decision
    decision_engine_->setDecisionCallback([](const DecisionResult& result) {
        std::cout << "[Decision] D(t)=" << result.action_name
                  << " κ=" << std::fixed << std::setprecision(2) << result.confidence
                  << (result.is_meta_action ? " [META]" : "") << "\n";
    });

    decision_engine_->setVetoCallback([](const ActionOption& option, const std::string& reason) {
        std::cout << "[Decision] ⛔ Veto: " << option.name << " (" << reason << ")\n";
    });

    // Créer le HybridSearchEngine (recherche mémoire hybride)
    // Note: On utilise un shared_ptr avec deleter vide car MemoryManager gère la durée de vie
    HybridSearchConfig hs_config;
    hs_config.enable_sentiment_modulation = true;
    hs_config.default_mode = SearchMode::ADAPTIVE;
    hs_config.verbose = false;

    std::shared_ptr<Neo4jClient> neo4j_shared(
        memory_manager_.getNeo4jClient(),
        [](Neo4jClient*) {}  // Deleter vide: MemoryManager gère la durée de vie
    );

    hybrid_search_ = std::make_shared<HybridSearchEngine>(
        neo4j_shared,
        conscience_engine_,
        hs_config
    );

    // Créer le LLMClient (reformulation émotionnelle)
    LLMClientConfig llm_config;
    llm_config.mode = LLMMode::DIRECT_HTTP;  // Mode HTTP direct par défaut
    llm_config.model = "gpt-4o-mini";
    llm_config.temperature = 0.7;
    llm_config.max_tokens = 500;
    llm_config.verbose = false;
    llm_config.loadFromEnvironment();  // Charge OPENAI_API_KEY

    llm_client_ = std::make_shared<LLMClient>(llm_config);

    // Initialiser le LLMClient si la clé API est disponible
    if (!llm_config.api_key.empty()) {
        if (llm_client_->initialize()) {
            std::cout << "[MCEEEngine] LLMClient initialisé (modèle=" << llm_config.model << ")\n";
        } else {
            std::cout << "[MCEEEngine] ⚠ LLMClient: échec initialisation\n";
        }
    } else {
        std::cout << "[MCEEEngine] LLMClient: OPENAI_API_KEY non défini (mode désactivé)\n";
    }

    std::cout << "[MCEEEngine] Système MCT/MLT initialisé\n";
    std::cout << "[MCEEEngine] MCTGraph: fenêtre=" << graph_config.time_window_seconds << "s\n";
    std::cout << "[MCEEEngine] MLT: " << mlt_->patternCount() << " patterns de base\n";
    std::cout << "[MCEEEngine] ConscienceEngine initialisé (Wt=" << conscience_engine_->getWisdom() << ")\n";
    std::cout << "[MCEEEngine] ADDOEngine initialisé (Rs=" << addo_engine_->getResilience() << ")\n";
    std::cout << "[MCEEEngine] DecisionEngine initialisé (τ_max=" << decision_config.tau_max_ms << "ms)\n";
    std::cout << "[MCEEEngine] HybridSearchEngine initialisé\n";
}

void MCEEEngine::setupCallbacks() {
    if (!pattern_matcher_) return;
    
    // Callback sur changement de pattern
    pattern_matcher_->setMatchCallback([this](const MatchResult& match) {
        if (match.is_transition) {
            std::cout << "\n[MCEEEngine] ═══ Transition de pattern: " 
                      << current_match_.pattern_name << " → " 
                      << match.pattern_name << " ═══\n";
            std::cout << "[MCEEEngine] Similarité: " << std::fixed << std::setprecision(3) 
                      << match.similarity << ", Confiance: " << match.confidence << "\n\n";
        }
    });
    
    // Callback sur création de pattern
    pattern_matcher_->setNewPatternCallback([this](const std::string& id, const std::string& name) {
        std::cout << "[MCEEEngine] ★ Nouveau pattern créé: " << name << " (id: " << id << ")\n";
    });
    
    // Callback sur transition
    pattern_matcher_->setTransitionCallback([this](const std::string& from, const std::string& to, double prob) {
        stats_.phase_transitions++;

        // Enregistrer la durée du pattern précédent
        auto now = std::chrono::steady_clock::now();
        double duration = std::chrono::duration<double>(now - pattern_start_time_).count();
        mlt_->recordActivation(from, duration);

        // Enregistrer la transition dans Neo4j
        memory_manager_.recordPatternTransition(from, to, duration, "auto");

        pattern_start_time_ = now;
    });
    
    // Callback MLT sur événements pattern
    if (mlt_) {
        mlt_->setEventCallback([](const PatternEvent& event) {
            std::string type_str;
            switch (event.type) {
                case PatternEvent::Type::CREATED: type_str = "CREATED"; break;
                case PatternEvent::Type::MODIFIED: type_str = "MODIFIED"; break;
                case PatternEvent::Type::MERGED: type_str = "MERGED"; break;
                case PatternEvent::Type::DELETED: type_str = "DELETED"; break;
                case PatternEvent::Type::ACTIVATED: type_str = "ACTIVATED"; break;
                case PatternEvent::Type::DEACTIVATED: type_str = "DEACTIVATED"; break;
            }
            std::cout << "[MLT Event] " << type_str << ": " << event.pattern_name;
            if (!event.details.empty()) {
                std::cout << " - " << event.details;
            }
            std::cout << "\n";
        });
    }
}

MCEEEngine::~MCEEEngine() {
    stop();
}

bool MCEEEngine::start() {
    if (running_.load()) {
        std::cout << "[MCEEEngine] Déjà en cours d'exécution" << std::endl;
        return true;
    }

    std::cout << "[MCEEEngine] Démarrage en cours..." << std::endl;

    // Initialiser RabbitMQ
    if (!initRabbitMQ()) {
        std::cerr << "[MCEEEngine] Échec initialisation RabbitMQ" << std::endl;
        return false;
    }

    running_.store(true);

    // Démarrer les threads de consommation
    emotions_consumer_thread_ = std::thread(&MCEEEngine::emotionsConsumeLoop, this);
    speech_consumer_thread_ = std::thread(&MCEEEngine::speechConsumeLoop, this);
    tokens_consumer_thread_ = std::thread(&MCEEEngine::tokensConsumeLoop, this);
    snapshot_timer_thread_ = std::thread(&MCEEEngine::snapshotTimerLoop, this);

    std::cout << "[MCEEEngine] ✓ Démarré et en attente de messages RabbitMQ" << std::endl;
    std::cout << "[MCEEEngine] Émotions: " << rabbitmq_config_.emotions_exchange
              << " / " << rabbitmq_config_.emotions_routing_key << std::endl;
    std::cout << "[MCEEEngine] Parole: " << rabbitmq_config_.speech_exchange
              << " / " << rabbitmq_config_.speech_routing_key << std::endl;
    std::cout << "[MCEEEngine] Tokens: " << rabbitmq_config_.tokens_exchange
              << " / " << rabbitmq_config_.tokens_routing_key << std::endl;
    std::cout << "[MCEEEngine] MCTGraph snapshot interval: "
              << mct_graph_->getConfig().snapshot_interval_seconds << "s" << std::endl;

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

    if (tokens_consumer_thread_.joinable()) {
        tokens_consumer_thread_.join();
    }

    if (snapshot_timer_thread_.joinable()) {
        snapshot_timer_thread_.join();
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

    // Statistiques MCTGraph
    if (mct_graph_) {
        std::cout << "  - MCTGraph mots: " << mct_graph_->getWordCount() << "\n";
        std::cout << "  - MCTGraph émotions: " << mct_graph_->getEmotionCount() << "\n";
        std::cout << "  - MCTGraph arêtes causales: " << mct_graph_->getCausalEdgeCount() << "\n";
    }
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

        // === Créer 4 channels séparés (thread-safety) ===
        // AmqpClient::Channel n'est PAS thread-safe, chaque thread doit avoir son propre channel
        emotions_channel_ = AmqpClient::Channel::Open(opts);
        speech_channel_ = AmqpClient::Channel::Open(opts);
        tokens_channel_ = AmqpClient::Channel::Open(opts);
        publish_channel_ = AmqpClient::Channel::Open(opts);

        // === Déclarer les exchanges sur le channel de publication ===

        // Exchange pour les émotions (entrée)
        publish_channel_->DeclareExchange(
            rabbitmq_config_.emotions_exchange,
            AmqpClient::Channel::EXCHANGE_TYPE_TOPIC,
            false, true, false
        );

        // Exchange pour la parole (entrée)
        publish_channel_->DeclareExchange(
            rabbitmq_config_.speech_exchange,
            AmqpClient::Channel::EXCHANGE_TYPE_TOPIC,
            false, true, false
        );

        // Exchange pour la sortie
        publish_channel_->DeclareExchange(
            rabbitmq_config_.output_exchange,
            AmqpClient::Channel::EXCHANGE_TYPE_TOPIC,
            false, true, false
        );

        // Exchange pour les snapshots MCTGraph (sortie vers module rêves)
        publish_channel_->DeclareExchange(
            rabbitmq_config_.snapshot_exchange,
            AmqpClient::Channel::EXCHANGE_TYPE_TOPIC,
            false, true, false
        );

        // Exchange pour les tokens Neo4j/spaCy
        publish_channel_->DeclareExchange(
            rabbitmq_config_.tokens_exchange,
            AmqpClient::Channel::EXCHANGE_TYPE_TOPIC,
            false, true, false
        );

        // === Configurer le channel émotions ===
        std::string emotions_queue = emotions_channel_->DeclareQueue(
            "mcee_emotions_queue", false, true, false, false
        );
        emotions_channel_->BindQueue(
            emotions_queue,
            rabbitmq_config_.emotions_exchange,
            rabbitmq_config_.emotions_routing_key
        );
        emotions_consumer_tag_ = emotions_channel_->BasicConsume(
            emotions_queue, "", true, false, false, 1
        );

        // === Configurer le channel parole ===
        std::string speech_queue = speech_channel_->DeclareQueue(
            "mcee_speech_queue", false, true, false, false
        );
        speech_channel_->BindQueue(
            speech_queue,
            rabbitmq_config_.speech_exchange,
            rabbitmq_config_.speech_routing_key
        );
        speech_consumer_tag_ = speech_channel_->BasicConsume(
            speech_queue, "", true, false, false, 1
        );

        // === Configurer le channel tokens ===
        std::string tokens_queue = tokens_channel_->DeclareQueue(
            "mcee_tokens_queue", false, true, false, false
        );
        tokens_channel_->BindQueue(
            tokens_queue,
            rabbitmq_config_.tokens_exchange,
            rabbitmq_config_.tokens_routing_key
        );
        tokens_consumer_tag_ = tokens_channel_->BasicConsume(
            tokens_queue, "", true, false, false, 1
        );

        std::cout << "[MCEEEngine] Connexion RabbitMQ établie (4 channels)" << std::endl;
        std::cout << "[MCEEEngine] Queues créées: emotions + speech + tokens" << std::endl;
        std::cout << "[MCEEEngine] Exchange snapshot: " << rabbitmq_config_.snapshot_exchange << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[MCEEEngine] Erreur RabbitMQ: " << e.what() << "\n";
        return false;
    }
}

void MCEEEngine::emotionsConsumeLoop() {
    std::cout << "[MCEEEngine] Boucle de consommation des émotions démarrée" << std::endl;

    while (running_.load()) {
        try {
            AmqpClient::Envelope::ptr_t envelope;
            bool received = emotions_channel_->BasicConsumeMessage(emotions_consumer_tag_, envelope, 500);

            if (received && envelope) {
                std::string body(envelope->Message()->Body().begin(),
                                envelope->Message()->Body().end());

                handleEmotionMessage(body);
                emotions_channel_->BasicAck(envelope);
            }

        } catch (const std::exception& e) {
            std::cerr << "[MCEEEngine] Erreur consommation émotions: " << e.what() << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void MCEEEngine::speechConsumeLoop() {
    std::cout << "[MCEEEngine] Boucle de consommation de la parole démarrée" << std::endl;

    while (running_.load()) {
        try {
            AmqpClient::Envelope::ptr_t envelope;
            bool received = speech_channel_->BasicConsumeMessage(speech_consumer_tag_, envelope, 500);

            if (received && envelope) {
                std::string body(envelope->Message()->Body().begin(),
                                envelope->Message()->Body().end());

                handleSpeechMessage(body);
                speech_channel_->BasicAck(envelope);
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

        std::cout << "[MCEEEngine] Message émotion reçu (" << body.size() << " bytes)\n";

        std::unordered_map<std::string, double> raw_emotions;
        size_t found_count = 0;
        for (const auto& name : EMOTION_NAMES) {
            if (input.contains(name)) {
                raw_emotions[name] = input[name].get<double>();
                found_count++;
            } else {
                raw_emotions[name] = 0.0;
            }
        }

        std::cout << "[MCEEEngine] Émotions trouvées: " << found_count << "/24\n";

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
    // ═══════════════════════════════════════════════════════════════════════
    // PIPELINE V3.0 : MCT → PatternMatcher → MLT → MCTGraph
    // ═══════════════════════════════════════════════════════════════════════

    // 1. AJOUTER L'ÉTAT À LA MCT
    pushToMCT(current_state_);

    // 1b. AJOUTER L'ÉTAT AU MCTGRAPH (graphe relationnel)
    if (mct_graph_) {
        // Calculer la persistance estimée (basée sur l'intensité)
        double persistence = current_state_.getMeanIntensity() * 5.0;  // 0-5 secondes
        if (persistence >= mct_graph_->getConfig().emotion_persistence_threshold_seconds) {
            last_emotion_node_id_ = mct_graph_->addEmotionWithContext(
                current_state_,
                persistence,
                current_state_.getValence(),
                current_state_.getMeanIntensity()
            );

            // Détecter automatiquement les liens causaux avec les mots récents
            if (!last_emotion_node_id_.empty()) {
                mct_graph_->detectCausality(last_emotion_node_id_);
            }
        }
    }

    // 2. IDENTIFIER LE PATTERN VIA PatternMatcher
    MatchResult match = identifyPattern();
    
    // Stocker le match courant
    MatchResult previous_match = current_match_;
    current_match_ = match;
    
    // Log si transition de pattern
    if (match.is_transition || match.pattern_id != previous_match.pattern_id) {
        std::cout << "[MCEEEngine] Pattern actif: " << match.pattern_name
                  << " (sim=" << std::fixed << std::setprecision(3) << match.similarity
                  << ", conf=" << match.confidence << ")\n";
    }

    // 3. APPLIQUER LES COEFFICIENTS DU PATTERN
    EmotionalState processed_state = applyPatternCoefficients(current_state_, match);

    // 4. RÉCUPÉRER LES SOUVENIRS PERTINENTS (legacy)
    Phase current_phase = phase_detector_.getCurrentPhase();
    auto memories = memory_manager_.queryRelevantMemories(current_phase, current_state_, 10);

    for (auto& mem : memories) {
        memory_manager_.updateActivation(mem, current_state_);
    }

    // 5. VÉRIFIER AMYGHALEON (court-circuit d'urgence)
    handleEmergency(match);
    
    // 6. CALCULER LE DELTA TEMPS
    auto now = std::chrono::steady_clock::now();
    double delta_t = std::chrono::duration<double>(now - last_update_time_).count();
    last_update_time_ = now;
    
    // 7. METTRE À JOUR LA SAGESSE
    updateWisdom();
    
    // 8. CONFIGURER EmotionUpdater AVEC LES COEFFICIENTS DU PATTERN
    PhaseConfig pattern_config;
    pattern_config.alpha = match.alpha;
    pattern_config.beta = match.beta;
    pattern_config.gamma = match.gamma;
    pattern_config.delta = match.delta;
    pattern_config.theta = match.theta;
    pattern_config.amyghaleon_threshold = match.emergency_threshold;
    pattern_config.memory_consolidation = match.memory_trigger_threshold;
    
    emotion_updater_.setCoefficientsFromPhase(pattern_config);
    
    // 9. CALCULER L'INFLUENCE DES SOUVENIRS
    auto memory_influences = memory_manager_.computeMemoryInfluences(
        memories, match.delta
    );
    
    // 10. METTRE À JOUR LES ÉMOTIONS
    emotion_updater_.updateAllEmotions(
        current_state_,
        current_feedback_,
        delta_t,
        memory_influences,
        wisdom_
    );
    
    // 11. CALCULER LA VARIANCE GLOBALE
    current_state_.variance_global = emotion_updater_.computeGlobalVariance(
        current_state_, memories
    );
    
    // 12. CALCULER E_GLOBAL
    current_state_.E_global = emotion_updater_.computeEGlobal(
        current_state_,
        previous_state_.E_global,
        current_state_.variance_global
    );
    
    // 13. REPOUSSER L'ÉTAT TRAITÉ DANS LA MCT (feedback loop)
    if (mct_) {
        if (!last_speech_analysis_.raw_text.empty()) {
            mct_->pushWithSpeech(current_state_, 
                                 last_speech_analysis_.sentiment_score,
                                 last_speech_analysis_.arousal_score,
                                 last_speech_analysis_.raw_text);
        } else {
            mct_->push(current_state_);
        }
    }
    
    // 14. CONSOLIDER EN MLT SI SIGNIFICATIF
    consolidateToMLT();

    // 15. ENREGISTRER UN SOUVENIR SI SIGNIFICATIF
    if (current_state_.getMeanIntensity() > match.memory_trigger_threshold) {
        auto [dominant, value] = current_state_.getDominant();
        std::string context = "Pattern:" + match.pattern_name + "_" + dominant;
        memory_manager_.recordMemory(current_state_, current_phase, context);
    }

    // 16. PUBLIER L'ÉTAT
    publishState();
    
    // 17. CALLBACK
    if (on_state_change_) {
        on_state_change_(current_state_, match.pattern_name);
    }
    
    // Afficher l'état
    printState();
}

void MCEEEngine::printState() const {
    auto [dominant, value] = current_state_.getDominant();
    
    std::cout << "\n[MCEEEngine] État émotionnel mis à jour:\n";
    std::cout << "  Pattern   : " << current_match_.pattern_name 
              << " (sim=" << std::fixed << std::setprecision(2) << current_match_.similarity 
              << ", conf=" << current_match_.confidence << ")\n";
    std::cout << "  Dominant  : " << dominant << " = " 
              << std::fixed << std::setprecision(3) << value << "\n";
    std::cout << "  E_global  : " << std::fixed << std::setprecision(3) 
              << current_state_.E_global << "\n";
    std::cout << "  Variance  : " << std::fixed << std::setprecision(3) 
              << current_state_.variance_global << "\n";
    std::cout << "  Valence   : " << std::fixed << std::setprecision(3) 
              << current_state_.getValence() << "\n";
    std::cout << "  Intensité : " << std::fixed << std::setprecision(3) 
              << current_state_.getMeanIntensity() << "\n";
    
    // Afficher métriques MCT si disponible
    if (mct_ && !mct_->empty()) {
        std::cout << "  MCT       : size=" << mct_->size()
                  << ", stability=" << std::fixed << std::setprecision(2) << mct_->getStability()
                  << ", trend=" << mct_->getTrend() << "\n";
    }

    // Afficher métriques MCTGraph si disponible
    if (mct_graph_ && (mct_graph_->getWordCount() > 0 || mct_graph_->getEmotionCount() > 0)) {
        std::cout << "  MCTGraph  : " << mct_graph_->getWordCount() << " mots, "
                  << mct_graph_->getEmotionCount() << " émotions, "
                  << mct_graph_->getCausalEdgeCount() << " liens causaux\n";
    }
    std::cout << "\n";
}

void MCEEEngine::publishState() {
    if (!publish_channel_) return;

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

        // Pattern actif (v3.0)
        output["pattern"]["id"] = current_match_.pattern_id;
        output["pattern"]["name"] = current_match_.pattern_name;
        output["pattern"]["similarity"] = current_match_.similarity;
        output["pattern"]["confidence"] = current_match_.confidence;
        output["pattern"]["is_new"] = current_match_.is_new_pattern;
        output["pattern"]["is_transition"] = current_match_.is_transition;

        // Coefficients actifs (du pattern)
        output["coefficients"]["alpha"] = current_match_.alpha;
        output["coefficients"]["beta"] = current_match_.beta;
        output["coefficients"]["gamma"] = current_match_.gamma;
        output["coefficients"]["delta"] = current_match_.delta;
        output["coefficients"]["theta"] = current_match_.theta;
        output["coefficients"]["emergency_threshold"] = current_match_.emergency_threshold;

        // Phase legacy (pour compatibilité)
        output["phase"] = phaseToString(phase_detector_.getCurrentPhase());
        output["phase_duration"] = phase_detector_.getPhaseDuration();

        // Métriques MCT
        if (mct_) {
            output["mct"]["size"] = mct_->size();
            output["mct"]["stability"] = mct_->getStability();
            output["mct"]["volatility"] = mct_->getVolatility();
            output["mct"]["trend"] = mct_->getTrend();
        }

        // Métriques MCTGraph (graphe relationnel)
        if (mct_graph_) {
            output["mct_graph"]["word_count"] = mct_graph_->getWordCount();
            output["mct_graph"]["emotion_count"] = mct_graph_->getEmotionCount();
            output["mct_graph"]["edge_count"] = mct_graph_->getEdgeCount();
            output["mct_graph"]["causal_edge_count"] = mct_graph_->getCausalEdgeCount();
            output["mct_graph"]["density"] = mct_graph_->getGraphDensity();
        }

        // Statistiques
        output["stats"]["pattern_transitions"] = stats_.phase_transitions;
        output["stats"]["emergency_triggers"] = stats_.emergency_triggers;
        output["stats"]["wisdom"] = wisdom_;
        
        if (mlt_) {
            output["stats"]["total_patterns"] = mlt_->patternCount();
        }
        if (pattern_matcher_) {
            output["stats"]["total_matches"] = pattern_matcher_->getTotalMatches();
            output["stats"]["patterns_created"] = pattern_matcher_->getPatternsCreated();
        }

        std::string body = output.dump();
        publish_channel_->BasicPublish(
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

bool MCEEEngine::loadConfig(const std::string& config_path, bool skip_neo4j) {
    bool success = phase_detector_.loadConfig(config_path);
    if (success) {
        emotion_updater_.setCoefficientsFromPhase(phase_detector_.getCurrentConfig());
    }

    // Charger la configuration Neo4j si présente et non ignorée
    if (!skip_neo4j) {
        try {
            std::ifstream file(config_path);
            if (file.is_open()) {
                json config = json::parse(file);

                if (config.contains("neo4j") && config["neo4j"].value("enabled", false)) {
                    Neo4jClientConfig neo4j_config;
                    auto& neo4j_json = config["neo4j"];

                    neo4j_config.rabbitmq_host = neo4j_json.value("rabbitmq_host", "localhost");
                    neo4j_config.rabbitmq_port = neo4j_json.value("rabbitmq_port", 5672);
                    neo4j_config.rabbitmq_user = neo4j_json.value("rabbitmq_user", "virtus");
                    neo4j_config.rabbitmq_password = neo4j_json.value("rabbitmq_password", "virtus@83");
                    neo4j_config.request_queue = neo4j_json.value("request_queue", "neo4j.requests.queue");
                    neo4j_config.response_exchange = neo4j_json.value("response_exchange", "neo4j.responses");
                    neo4j_config.request_timeout_ms = neo4j_json.value("request_timeout_ms", 5000);
                    neo4j_config.max_retries = neo4j_json.value("max_retries", 3);
                    neo4j_config.async_mode = neo4j_json.value("async_mode", true);

                    std::cout << "[MCEEEngine] Configuration Neo4j..." << std::flush;
                    if (memory_manager_.setNeo4jConfig(neo4j_config)) {
                        std::cout << " activé et connecté" << std::endl;
                    } else {
                        std::cerr << " configuré mais non connecté" << std::endl;
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[MCEEEngine] Erreur chargement config Neo4j: " << e.what() << "\n";
        }
    }

    return success;
}

EmotionalState MCEEEngine::rawToState(
    const std::unordered_map<std::string, double>& raw) const
{
    EmotionalState state;
    double sum = 0.0;

    for (size_t i = 0; i < NUM_EMOTIONS; ++i) {
        auto it = raw.find(EMOTION_NAMES[i]);
        if (it != raw.end()) {
            state.emotions[i] = std::clamp(it->second, 0.0, 1.0);
            sum += state.emotions[i];
        }
    }

    // Calculer E_global (moyenne des 24 émotions)
    state.E_global = sum / static_cast<double>(NUM_EMOTIONS);

    // Calculer la variance globale
    double var_sum = 0.0;
    for (size_t i = 0; i < NUM_EMOTIONS; ++i) {
        double diff = state.emotions[i] - state.E_global;
        var_sum += diff * diff;
    }
    state.variance_global = var_sum / static_cast<double>(NUM_EMOTIONS);

    return state;
}

// ═══════════════════════════════════════════════════════════════════════════
// MÉTHODES MCT/MLT V3.0
// ═══════════════════════════════════════════════════════════════════════════

void MCEEEngine::pushToMCT(const EmotionalState& state) {
    if (!mct_) return;
    
    if (!last_speech_analysis_.raw_text.empty()) {
        mct_->pushWithSpeech(state, 
                             last_speech_analysis_.sentiment_score,
                             last_speech_analysis_.arousal_score,
                             last_speech_analysis_.raw_text);
    } else {
        mct_->push(state);
    }
}

MatchResult MCEEEngine::identifyPattern() {
    if (!pattern_matcher_) {
        // Fallback si pas de PatternMatcher
        MatchResult fallback;
        fallback.pattern_name = "DEFAULT";
        fallback.similarity = 0.0;
        fallback.confidence = 0.0;
        fallback.alpha = 0.3;
        fallback.beta = 0.2;
        fallback.gamma = 0.15;
        fallback.delta = 0.1;
        fallback.theta = 0.25;
        fallback.emergency_threshold = 0.8;
        fallback.memory_trigger_threshold = 0.5;
        return fallback;
    }
    
    return pattern_matcher_->match();
}

EmotionalState MCEEEngine::applyPatternCoefficients(const EmotionalState& raw_state, 
                                                     const MatchResult& match) {
    EmotionalState result = raw_state;
    
    // Les coefficients sont appliqués via EmotionUpdater dans processPipeline
    // Cette méthode peut être utilisée pour un pré-traitement si nécessaire
    
    // Pondérer l'état par la confiance du match
    double confidence_factor = 0.5 + 0.5 * match.confidence;
    
    for (size_t i = 0; i < NUM_EMOTIONS; ++i) {
        result.emotions[i] *= confidence_factor;
    }
    
    return result;
}

void MCEEEngine::consolidateToMLT() {
    if (!mct_ || !mlt_ || !pattern_matcher_) return;
    
    // Vérifier si l'état actuel est significatif
    double intensity = current_state_.getMeanIntensity();
    double stability = mct_->getStability();
    
    bool is_significant = (intensity > current_match_.memory_trigger_threshold) ||
                          (std::abs(last_speech_analysis_.sentiment_score) > 0.5) ||
                          (last_speech_analysis_.urgency_score > 0.7);
    
    if (!is_significant) return;
    
    // Si stable et significatif, renforcer le pattern actuel
    if (stability > 0.6 && !current_match_.pattern_id.empty()) {
        auto sig_opt = mct_->extractSignature();
        if (sig_opt) {
            // Mettre à jour le pattern avec la nouvelle signature
            double feedback = (last_speech_analysis_.sentiment_score + 1.0) / 2.0; // [0,1]
            mlt_->updatePattern(current_match_.pattern_id, *sig_opt, feedback);
        }
    }
    
    // Déclencher une passe d'apprentissage périodiquement
    if (stats_.phase_transitions % 10 == 0 && stats_.phase_transitions > 0) {
        mlt_->runLearningPass();
    }
}

void MCEEEngine::handleEmergency(const MatchResult& match) {
    // Vérifier le seuil d'urgence du pattern
    double max_emotion = 0.0;
    for (const auto& e : current_state_.emotions) {
        max_emotion = std::max(max_emotion, e);
    }
    
    // Récupérer les souvenirs pertinents
    Phase current_phase = phase_detector_.getCurrentPhase();
    auto memories = memory_manager_.queryRelevantMemories(current_phase, current_state_, 5);
    
    if (amyghaleon_.checkEmergency(current_state_, memories, match.emergency_threshold)) {
        auto response = amyghaleon_.triggerEmergencyResponse(current_state_, current_phase);
        
        // Créer un trauma potentiel si pattern est PEUR ou similaire
        if (match.pattern_name.find("PEUR") != std::string::npos ||
            match.pattern_name.find("FEAR") != std::string::npos) {
            memory_manager_.createPotentialTrauma(current_state_);
        }
        
        // Court-circuiter et publier l'état d'urgence
        publishState();
    }
}

std::string MCEEEngine::getCurrentPatternName() const {
    if (!current_match_.pattern_name.empty()) {
        return current_match_.pattern_name;
    }
    return phaseToString(phase_detector_.getCurrentPhase());
}

std::string MCEEEngine::getCurrentPatternId() const {
    return current_match_.pattern_id;
}

bool MCEEEngine::loadPatterns(const std::string& path) {
    if (!mlt_) return false;
    return mlt_->loadFromFile(path);
}

bool MCEEEngine::savePatterns(const std::string& path) const {
    if (!mlt_) return false;
    return mlt_->saveToFile(path);
}

void MCEEEngine::forcePattern(const std::string& pattern_name, const std::string& reason) {
    if (!mlt_ || !pattern_matcher_) return;
    
    auto pattern = mlt_->getPatternByName(pattern_name);
    if (pattern) {
        // Mettre à jour le match courant
        current_match_.pattern_id = pattern->id;
        current_match_.pattern_name = pattern->name;
        current_match_.similarity = 1.0;
        current_match_.confidence = pattern->confidence;
        current_match_.alpha = pattern->alpha;
        current_match_.beta = pattern->beta;
        current_match_.gamma = pattern->gamma;
        current_match_.delta = pattern->delta;
        current_match_.theta = pattern->theta;
        current_match_.emergency_threshold = pattern->emergency_threshold;
        current_match_.memory_trigger_threshold = pattern->memory_trigger_threshold;
        
        // Notifier le PatternMatcher
        pattern_matcher_->notifyPatternChange(pattern->id);
        
        std::cout << "[MCEEEngine] Pattern forcé: " << pattern_name 
                  << " (raison: " << reason << ")\n";
    }
}

std::string MCEEEngine::createPatternFromCurrent(const std::string& name, 
                                                  const std::string& description) {
    if (!pattern_matcher_) return "";
    return pattern_matcher_->forceCreatePattern(name, description);
}

void MCEEEngine::provideFeedback(double feedback) {
    if (!pattern_matcher_) return;
    pattern_matcher_->provideFeedback(feedback);
}

void MCEEEngine::runLearning() {
    if (!mlt_) return;
    mlt_->runLearningPass();
    std::cout << "[MCEEEngine] Passe d'apprentissage MLT terminée\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// MCTGRAPH INTEGRATION
// ═══════════════════════════════════════════════════════════════════════════

void MCEEEngine::tokensConsumeLoop() {
    std::cout << "[MCEEEngine] Boucle de consommation des tokens démarrée" << std::endl;

    while (running_.load()) {
        try {
            AmqpClient::Envelope::ptr_t envelope;
            bool received = tokens_channel_->BasicConsumeMessage(tokens_consumer_tag_, envelope, 500);

            if (received && envelope) {
                std::string body(envelope->Message()->Body().begin(),
                                envelope->Message()->Body().end());

                handleTokensMessage(body);
                tokens_channel_->BasicAck(envelope);
            }

        } catch (const std::exception& e) {
            std::cerr << "[MCEEEngine] Erreur consommation tokens: " << e.what() << "\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void MCEEEngine::snapshotTimerLoop() {
    std::cout << "[MCEEEngine] Timer snapshot MCTGraph démarré" << std::endl;

    // Utiliser l'intervalle de snapshot configuré au lieu de polling toutes les 500ms
    const auto snapshot_interval = std::chrono::milliseconds(
        static_cast<int>(mct_graph_->getConfig().snapshot_interval_seconds * 1000)
    );

    while (running_.load()) {
        // Attendre l'intervalle de snapshot complet
        std::this_thread::sleep_for(snapshot_interval);

        if (!running_.load()) break;

        if (mct_graph_) {
            // Créer et publier le snapshot directement
            auto snapshot = mct_graph_->createSnapshot();
            publishSnapshot(snapshot);

            // Maintenance périodique du graphe
            mct_graph_->pruneExpiredNodes();
            mct_graph_->applyEdgeDecay();
        }
    }
}

void MCEEEngine::handleTokensMessage(const std::string& body) {
    if (!mct_graph_) return;

    try {
        json input = json::parse(body);

        // Format attendu :
        // {
        //   "sentence_id": "...",
        //   "tokens": [
        //     {"text": "...", "lemma": "...", "pos": "NOUN", "sentiment": 0.5},
        //     ...
        //   ],
        //   "relations": [
        //     {"source": 0, "target": 1, "type": "nsubj"},
        //     ...
        //   ]
        // }

        std::string sentence_id = input.value("sentence_id", "");
        std::vector<std::string> word_ids;

        // Ajouter les tokens au graphe
        if (input.contains("tokens") && input["tokens"].is_array()) {
            for (const auto& token : input["tokens"]) {
                std::string lemma = token.value("lemma", token.value("text", ""));
                std::string pos = token.value("pos", "UNKNOWN");
                std::string original = token.value("text", lemma);

                std::string word_id = mct_graph_->addWord(lemma, pos, sentence_id, original);
                word_ids.push_back(word_id);

                // Détecter les co-occurrences temporelles
                mct_graph_->detectTemporalCooccurrences(word_id);

                // Détecter la causalité avec la dernière émotion
                if (!last_emotion_node_id_.empty()) {
                    mct_graph_->detectCausality(last_emotion_node_id_);
                }
            }
        }

        // Ajouter les relations sémantiques
        if (input.contains("relations") && input["relations"].is_array()) {
            for (const auto& rel : input["relations"]) {
                size_t source_idx = rel.value("source", size_t(0));
                size_t target_idx = rel.value("target", size_t(0));
                std::string rel_type = rel.value("type", "related");

                if (source_idx < word_ids.size() && target_idx < word_ids.size()) {
                    mct_graph_->addSemanticEdge(
                        word_ids[source_idx],
                        word_ids[target_idx],
                        rel_type
                    );
                }
            }
        }

        std::cout << "[MCEEEngine] Tokens traités: " << word_ids.size()
                  << " mots ajoutés au MCTGraph\n";

    } catch (const std::exception& e) {
        std::cerr << "[MCEEEngine] Erreur parsing JSON tokens: " << e.what() << "\n";
    }
}

void MCEEEngine::publishSnapshot(const MCTGraphSnapshot& snapshot) {
    if (!publish_channel_) return;

    try {
        json output = snapshot.toJson();

        // Ajouter des métadonnées
        output["source"] = "mcee";
        output["pattern"] = current_match_.pattern_name;
        output["pattern_confidence"] = current_match_.confidence;

        std::string body = output.dump();
        publish_channel_->BasicPublish(
            rabbitmq_config_.snapshot_exchange,
            rabbitmq_config_.snapshot_routing_key,
            AmqpClient::BasicMessage::Create(body),
            false, false
        );

        std::cout << "[MCEEEngine] Snapshot MCTGraph publié: "
                  << snapshot.stats.total_words << " mots, "
                  << snapshot.stats.total_emotions << " émotions, "
                  << snapshot.stats.causal_edges << " liens causaux\n";

    } catch (const std::exception& e) {
        std::cerr << "[MCEEEngine] Erreur publication snapshot: " << e.what() << "\n";
    }
}

ConscienceSentimentState MCEEEngine::getConscienceState() const {
    if (conscience_engine_) {
        return conscience_engine_->getCurrentState();
    }
    return ConscienceSentimentState{};
}

GoalState MCEEEngine::getGoalState() const {
    if (addo_engine_) {
        return addo_engine_->getCurrentState();
    }
    return GoalState{};
}

DecisionResult MCEEEngine::makeDecision(
    const std::string& context_type,
    const std::vector<ActionOption>& available_actions)
{
    if (!decision_engine_) {
        return DecisionResult{};
    }

    // Récupérer les états actuels
    ConscienceSentimentState conscience_state = getConscienceState();
    GoalState goal_state = getGoalState();

    // Déléguer au DecisionEngine
    return decision_engine_->decide(
        current_state_,
        conscience_state,
        goal_state,
        context_type,
        available_actions
    );
}

// ═══════════════════════════════════════════════════════════════════════════════
// GÉNÉRATION DE RÉPONSE ÉMOTIONNELLE (LLM)
// ═══════════════════════════════════════════════════════════════════════════════

std::string MCEEEngine::generateEmotionalResponse(
    const std::string& question,
    const std::vector<std::string>& lemmas,
    const std::vector<double>& embedding)
{
    // Vérifier que le LLMClient est prêt
    if (!llm_client_ || !llm_client_->isReady()) {
        std::cerr << "[MCEEEngine] LLMClient non disponible. "
                  << "Définissez OPENAI_API_KEY pour activer.\n";
        return "";
    }

    // 1. TOUJOURS construire le contexte depuis les émotions en temps réel
    LLMContext context;

    // Récupérer l'état de conscience actuel (Ft, Ct)
    if (conscience_engine_) {
        auto state = conscience_engine_->getCurrentState();
        context.Ft = state.sentiment;
        context.Ct = state.consciousness_level;
        context.sentiment_label = state.sentiment > 0.2 ? "positif" :
                                  (state.sentiment < -0.2 ? "négatif" : "neutre");
    }

    // Construire le contexte depuis l'état émotionnel actuel (émotions temps réel)
    {
        std::lock_guard<std::mutex> lock(state_mutex_);

        // Extraire les émotions significatives (entre 0.4 et 1.0)
        for (size_t i = 0; i < NUM_EMOTIONS; ++i) {
            if (current_state_.emotions[i] >= 0.4 && current_state_.emotions[i] <= 1.0) {
                EmotionScore es;
                es.name = EMOTION_NAMES[i];
                es.score = current_state_.emotions[i];
                es.trigger = "temps_réel";
                context.emotions.push_back(es);
            }
        }

        // Trier par score décroissant
        std::sort(context.emotions.begin(), context.emotions.end(),
            [](const EmotionScore& a, const EmotionScore& b) {
                return a.score > b.score;
            });

        // Dominante
        if (!context.emotions.empty()) {
            context.dominant_emotion = context.emotions[0].name;
            context.dominant_score = context.emotions[0].score;
        }
    }

    std::cout << "[MCEEEngine] État émotionnel temps réel: Ft=" << std::fixed << std::setprecision(2)
              << context.Ft << " Ct=" << context.Ct;
    if (!context.dominant_emotion.empty()) {
        std::cout << " dominant=" << context.dominant_emotion
                  << "(" << static_cast<int>(context.dominant_score * 100) << "%)";
    }
    std::cout << "\n";

    // 2. Enrichir avec la recherche mémoire (si disponible et si lemmas fournis)
    bool memory_found = false;
    if (hybrid_search_ && !lemmas.empty()) {
        std::vector<ParsedToken> tokens;
        for (const auto& lemma : lemmas) {
            ParsedToken t;
            t.lemma = lemma;
            t.text = lemma;
            t.pos = "NOUN";
            t.is_significant = true;
            tokens.push_back(t);
        }

        auto search_result = hybrid_search_->search(question, tokens, embedding);

        if (!search_result.results.empty()) {
            memory_found = true;

            // Fusionner les mots-clés de la mémoire
            for (const auto& result : search_result.results) {
                for (const auto& kw : result.keywords) {
                    if (std::find(context.context_words.begin(),
                                  context.context_words.end(), kw) == context.context_words.end()) {
                        context.context_words.push_back(kw);
                    }
                }
                // Ajouter les souvenirs activés
                if (!result.memory_name.empty() && context.activated_memories.size() < 5) {
                    context.activated_memories.push_back(result.memory_name);
                }
            }

            context.search_confidence = search_result.overall_confidence;

            std::cout << "[MCEEEngine] Mémoire enrichie: "
                      << search_result.results.size() << " souvenirs, "
                      << "confiance=" << search_result.overall_confidence << "\n";
        }
    }

    if (!memory_found) {
        std::cout << "[MCEEEngine] Pas de souvenir trouvé - utilisation émotions temps réel uniquement\n";
    }

    // Ajouter les lemmas comme mots-clés contextuels
    for (const auto& lemma : lemmas) {
        if (std::find(context.context_words.begin(),
                      context.context_words.end(), lemma) == context.context_words.end()) {
            context.context_words.push_back(lemma);
        }
    }

    // 3. Générer la réponse via LLM
    LLMRequest request;
    request.user_question = question;
    request.emotional_context = context;

    auto response = llm_client_->generate(request);

    if (response.success) {
        std::cout << "[MCEEEngine] Réponse LLM générée: "
                  << response.tokens_total << " tokens, "
                  << response.generation_time_ms << "ms\n";
        return response.content;
    } else {
        std::cerr << "[MCEEEngine] Erreur LLM: " << response.error_message << "\n";
        return "";
    }
}

void MCEEEngine::generateEmotionalResponseAsync(
    const std::string& question,
    const std::vector<std::string>& lemmas,
    const std::vector<double>& embedding,
    std::function<void(const PipelineResult&)> callback)
{
    // Vérifier que le LLMClient est prêt
    if (!llm_client_ || !llm_client_->isReady()) {
        PipelineResult result;
        result.success = false;
        result.error = "LLMClient non disponible";
        if (callback) callback(result);
        return;
    }

    // Lancer dans un thread séparé
    std::thread([this, question, lemmas, embedding, callback]() {
        PipelineResult result;
        auto start_time = std::chrono::steady_clock::now();

        // 1. TOUJOURS construire depuis les émotions temps réel
        LLMContext context;

        if (conscience_engine_) {
            auto state = conscience_engine_->getCurrentState();
            context.Ft = state.sentiment;
            context.Ct = state.consciousness_level;
            context.sentiment_label = state.sentiment > 0.2 ? "positif" :
                                      (state.sentiment < -0.2 ? "négatif" : "neutre");
        }

        // Émotions temps réel (entre 0.4 et 1.0)
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            for (size_t i = 0; i < NUM_EMOTIONS; ++i) {
                if (current_state_.emotions[i] >= 0.4 && current_state_.emotions[i] <= 1.0) {
                    EmotionScore es;
                    es.name = EMOTION_NAMES[i];
                    es.score = current_state_.emotions[i];
                    es.trigger = "temps_réel";
                    context.emotions.push_back(es);
                }
            }
            std::sort(context.emotions.begin(), context.emotions.end(),
                [](const EmotionScore& a, const EmotionScore& b) {
                    return a.score > b.score;
                });
            if (!context.emotions.empty()) {
                context.dominant_emotion = context.emotions[0].name;
                context.dominant_score = context.emotions[0].score;
            }
        }

        // 2. Enrichir avec recherche mémoire si disponible
        if (hybrid_search_ && !lemmas.empty()) {
            std::vector<ParsedToken> tokens;
            for (const auto& lemma : lemmas) {
                ParsedToken t;
                t.lemma = lemma;
                t.text = lemma;
                t.is_significant = true;
                tokens.push_back(t);
            }

            auto search_start = std::chrono::steady_clock::now();
            auto search_result = hybrid_search_->search(question, tokens, embedding);
            auto search_end = std::chrono::steady_clock::now();

            result.search_time_ms = std::chrono::duration<double, std::milli>(
                search_end - search_start).count();

            // Fusionner si résultats trouvés
            if (!search_result.results.empty()) {
                for (const auto& r : search_result.results) {
                    for (const auto& kw : r.keywords) {
                        context.context_words.push_back(kw);
                    }
                    if (!r.memory_name.empty()) {
                        context.activated_memories.push_back(r.memory_name);
                    }
                }
                context.search_confidence = search_result.overall_confidence;
            }
        }

        // Ajouter lemmas
        for (const auto& lemma : lemmas) {
            context.context_words.push_back(lemma);
        }

        result.context = context;

        // Génération LLM
        auto llm_start = std::chrono::steady_clock::now();

        LLMRequest request;
        request.user_question = question;
        request.emotional_context = context;

        auto response = llm_client_->generate(request);

        auto llm_end = std::chrono::steady_clock::now();
        result.llm_time_ms = std::chrono::duration<double, std::milli>(llm_end - llm_start).count();

        result.success = response.success;
        result.response = response.content;
        if (!response.success) {
            result.error = response.error_message;
        }

        auto end_time = std::chrono::steady_clock::now();
        result.total_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

        if (callback) {
            callback(result);
        }
    }).detach();
}

} // namespace mcee
