//===== MCEECore.cpp - Corrections =====
#include "MCEECore.h"
#include "MCEEConfig.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>  // AJOUT MANQUANT pour std::accumulate

namespace MCEE {
    MCEECore::MCEECore()
        : rabbitmq_manager_(std::make_unique<RabbitMQManager>()),
          contextualizer_(std::make_unique<EmotionContextualizer>()) {
    }

    MCEECore::~MCEECore() {
        stop();
    }

    bool MCEECore::initialize(const std::string& config_path) {
        // Load configuration
        if (!ConfigManager::getInstance().loadFromFile(config_path)) {
            std::cerr << "Warning: Could not load config file, using defaults" << std::endl;
        }

        const auto& params = ConfigManager::getInstance().getParameters();

        // Initialize RabbitMQ
        if (!rabbitmq_manager_->initialize(params.rabbitmq_host, params.rabbitmq_port,
                                          params.rabbitmq_username, params.rabbitmq_password)) {
            std::cerr << "Failed to initialize RabbitMQ connection" << std::endl;
            return false;
        }

        // Set up subscriptions
        rabbitmq_manager_->subscribeToEmotionalInput(
            params.queue_emotional_input,
            [this](const EmotionalInput& input) { onEmotionalInput(input); }
        );

        rabbitmq_manager_->subscribeToContextData(
            params.queue_context_input,
            [this](const ContextData& context) { onContextData(context); }
        );

        std::cout << "MCEE Core initialized successfully" << std::endl;
        return true;
    }

    void MCEECore::start() {
        if (running_) return;

        running_ = true;
        should_stop_ = false;

        processing_thread_ = std::thread(&MCEECore::processingLoop, this);

        std::cout << "MCEE Core started" << std::endl;
    }

    void MCEECore::stop() {
        if (!running_) return;

        should_stop_ = true;
        running_ = false;

        data_cv_.notify_all();

        if (processing_thread_.joinable()) {
            processing_thread_.join();
        }

        if (rabbitmq_manager_) {
            rabbitmq_manager_->shutdown();
        }

        std::cout << "MCEE Core stopped" << std::endl;
    }

    void MCEECore::processingLoop() {
        const auto& params = ConfigManager::getInstance().getParameters();
        auto sleep_duration = std::chrono::milliseconds(1000 / params.frequence_maj_hz);

        while (!should_stop_) {
            std::unique_lock<std::mutex> lock(data_mutex_);

            // Wait for data or timeout
            data_cv_.wait_for(lock, sleep_duration, [this] {
                return has_emotional_data_ && has_context_data_;
            });

            if (has_emotional_data_ && has_context_data_) {
                auto start_time = std::chrono::high_resolution_clock::now();

                processData();

                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                    end_time - start_time).count() / 1000.0; // Convert to milliseconds

                updateMetrics(duration);

                // Reset flags after processing
                has_emotional_data_ = false;
                has_context_data_ = false;
            }

            lock.unlock();
            std::this_thread::sleep_for(sleep_duration);
        }
    }

    void MCEECore::processData() {
        // Contextualize emotions
        ContextualizedEmotions result = contextualizer_->contextualizeEmotions(
            latest_emotional_input_, latest_context_data_);

        const auto& params = ConfigManager::getInstance().getParameters();

        // Publish to consciousness module
        rabbitmq_manager_->publishContextualizedEmotions(
            params.queue_consciousness_output, result);

        // Handle Amygdaleon trigger if needed
        if (result.signal_amyghaleon) {
            handleAmygdaleonTrigger(result);
        }

        // Handle MLT consolidation if needed
        if (result.souvenir_a_consolider) {
            handleMLTConsolidation(latest_emotional_input_, result);
        }

        processed_messages_++;

        // Log important events
        if (result.niveau_danger != DangerLevel::NORMAL) {
            std::cout << "Danger level: " << static_cast<int>(result.niveau_danger)
                      << ", Context: " << result.contexte_detecte
                      << ", Gradient: " << result.gradient_danger_global << std::endl;
        }
    }

    void MCEECore::onEmotionalInput(const EmotionalInput& input) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        latest_emotional_input_ = input;
        has_emotional_data_ = true;
        data_cv_.notify_one();
    }

    void MCEECore::onContextData(const ContextData& context) {
        std::lock_guard<std::mutex> lock(data_mutex_);
        latest_context_data_ = context;
        has_context_data_ = true;
        data_cv_.notify_one();
    }

    void MCEECore::handleAmygdaleonTrigger(const ContextualizedEmotions& emotions) {
        AmygdaleonSignal signal;
        signal.urgence = true;
        signal.niveau_danger = emotions.niveau_danger;
        signal.gradient_danger_global = emotions.gradient_danger_global;
        signal.contexte_detecte = emotions.contexte_detecte;
        signal.text_id = emotions.text_id;
        signal.timestamp = std::chrono::steady_clock::now();

        // Identify critical emotions
        for (size_t i = 0; i < emotions.emotions_contextualisees.size(); ++i) {
            if (emotions.emotions_contextualisees[i] > 0.7) {
                signal.emotions_critiques.push_back("emotion_" + std::to_string(i));
            }
        }

        // Set gradients that triggered the signal
        signal.gradients_declencheurs["global"] = emotions.gradient_danger_global;

        // Determine intervention recommendation
        if (emotions.gradient_danger_global > 0.9) {
            signal.recommandation_intervention = "PROTECTION_IMMEDIATE_TECHNIQUE";
        } else if (emotions.contexte_detecte == "urgence_physique") {
            signal.recommandation_intervention = "PROTECTION_PHYSIQUE";
        } else {
            signal.recommandation_intervention = "SURVEILLANCE_RENFORCEE";
        }

        const auto& params = ConfigManager::getInstance().getParameters();
        rabbitmq_manager_->publishAmygdaleonSignal(params.queue_amygdaleon_output, signal);

        std::cout << "Amygdaleon signal sent: " << signal.recommandation_intervention << std::endl;
    }

    void MCEECore::handleMLTConsolidation(const EmotionalInput& input,
                                        const ContextualizedEmotions& emotions) {
        MemoryToConsolidate memory;
        memory.id_mcee = generateTextId();
        memory.priorite = emotions.priorite_mlt;
        memory.emotions_brutes = input.emotions_brutes;
        memory.emotions_contextualisees = emotions.emotions_contextualisees;
        memory.contexte_detecte = emotions.contexte_detecte;
        memory.score_significativite = contextualizer_->calculateSignificanceScore(emotions);
        memory.timestamp = std::chrono::steady_clock::now();

        // Set processing recommendation
        if (emotions.priorite_mlt == Priority::CRITIQUE) {
            memory.recommandation_traitement = "CONSOLIDATION_PRIORITAIRE_PATTERNS_DANGER";
        } else if (emotions.gradient_danger_global > 0.6) {
            memory.recommandation_traitement = "CONSOLIDATION_PATTERNS_STRESS";
        } else {
            memory.recommandation_traitement = "CONSOLIDATION_STANDARD";
        }

        const auto& params = ConfigManager::getInstance().getParameters();
        rabbitmq_manager_->publishMemoryToConsolidate(params.queue_mlt_output, memory);

        std::cout << "Memory consolidation request sent: " << memory.recommandation_traitement << std::endl;
    }

    void MCEECore::updateMetrics(double processing_time) {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        processing_times_.push_back(processing_time);

        // Keep only last 1000 measurements
        if (processing_times_.size() > 1000) {
            processing_times_.erase(processing_times_.begin());
        }
    }

    std::string MCEECore::generateTextId() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << "mcee_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S")
           << "_" << std::setfill('0') << std::setw(3) << ms.count();

        return ss.str();
    }

    double MCEECore::getAverageProcessingTime() const {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        if (processing_times_.empty()) return 0.0;

        double sum = std::accumulate(processing_times_.begin(), processing_times_.end(), 0.0);
        return sum / processing_times_.size();
    }

    double MCEECore::getCurrentCPUUsage() const {
        // Simplified CPU usage calculation
        // In a real implementation, this would use system calls
        return 0.0;
    }

    size_t MCEECore::getProcessedMessagesCount() const {
        return processed_messages_.load();
    }
}