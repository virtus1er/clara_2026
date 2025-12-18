//===== MCEERabbitMQ_stub.cpp - Version stub sans dépendances =====
#include "MCEERabbitMQ.h"
#include "MCEEConfig.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace MCEE {
    RabbitMQManager::RabbitMQManager() {}

    RabbitMQManager::~RabbitMQManager() {
        shutdown();
    }

    bool RabbitMQManager::initialize(const std::string& host, int port,
                                   const std::string& username, const std::string& password) {
        connection_host_ = host;
        connection_port_ = port;
        connection_username_ = username;
        connection_password_ = password;

        std::cout << "STUB: RabbitMQ connection to " << host << ":" << port << " (simulation mode)" << std::endl;
        connected_ = true;
        return true;
    }

    void RabbitMQManager::shutdown() {
        should_stop_ = true;
        connected_ = false;

        if (emotional_consumer_thread_.joinable()) {
            emotional_consumer_thread_.join();
        }
        if (context_consumer_thread_.joinable()) {
            context_consumer_thread_.join();
        }

        std::cout << "STUB: RabbitMQ disconnected" << std::endl;
    }

    void RabbitMQManager::subscribeToEmotionalInput(const std::string& queue,
                                                   EmotionalInputCallback callback) {
        emotional_callback_ = callback;
        emotional_queue_ = queue;

        // Start simulation thread
        emotional_consumer_thread_ = std::thread(&RabbitMQManager::emotionalConsumerLoop, this);

        std::cout << "STUB: Subscribed to emotional queue: " << queue << std::endl;
    }

    void RabbitMQManager::subscribeToContextData(const std::string& queue,
                                                ContextDataCallback callback) {
        context_callback_ = callback;
        context_queue_ = queue;

        // Start simulation thread
        context_consumer_thread_ = std::thread(&RabbitMQManager::contextConsumerLoop, this);

        std::cout << "STUB: Subscribed to context queue: " << queue << std::endl;
    }

    bool RabbitMQManager::publishContextualizedEmotions(const std::string& queue,
                                                       const ContextualizedEmotions& emotions) {
        std::string message = serializeToJson(emotions);

        // Write to file for debugging
        std::ofstream file("output_" + queue + ".json", std::ios::app);
        file << message << std::endl;

        std::cout << "STUB: Published to " << queue << " (size: " << message.size() << " bytes)" << std::endl;
        return true;
    }

    bool RabbitMQManager::publishAmygdaleonSignal(const std::string& queue,
                                                 const AmygdaleonSignal& signal) {
        std::string message = serializeToJson(signal);

        std::ofstream file("output_" + queue + ".json", std::ios::app);
        file << message << std::endl;

        std::cout << "STUB: Amygdaleon signal published to " << queue << std::endl;
        return true;
    }

    bool RabbitMQManager::publishMemoryToConsolidate(const std::string& queue,
                                                    const MemoryToConsolidate& memory) {
        std::string message = serializeToJson(memory);

        std::ofstream file("output_" + queue + ".json", std::ios::app);
        file << message << std::endl;

        std::cout << "STUB: Memory consolidation published to " << queue << std::endl;
        return true;
    }

    void RabbitMQManager::emotionalConsumerLoop() {
        std::cout << "STUB: Starting emotional consumer simulation" << std::endl;

        int counter = 0;
        while (!should_stop_ && connected_) {
            std::this_thread::sleep_for(std::chrono::seconds(5));

            if (emotional_callback_) {
                // Simulate emotional input
                EmotionalInput input;
                input.text_id = "stub_emotion_" + std::to_string(counter++);
                input.timestamp = std::chrono::steady_clock::now();
                input.intensite_globale = 0.5;

                // Fill with some test emotion values
                for (size_t i = 0; i < 24; ++i) {
                    input.emotions_brutes[i] = (i % 4) * 0.25; // Simple pattern
                }

                input.emotions_dominantes = {"Joie", "Curiosité"};

                emotional_callback_(input);
                std::cout << "STUB: Simulated emotional input sent" << std::endl;
            }
        }

        std::cout << "STUB: Emotional consumer simulation ended" << std::endl;
    }

    void RabbitMQManager::contextConsumerLoop() {
        std::cout << "STUB: Starting context consumer simulation" << std::endl;

        int counter = 0;
        while (!should_stop_ && connected_) {
            std::this_thread::sleep_for(std::chrono::seconds(3));

            if (context_callback_) {
                // Simulate context data
                ContextData context;
                context.timestamp = std::chrono::steady_clock::now();

                // Simulate sensor values
                context.capteurs_physiques.temperature_ambiante = 0.5;
                context.capteurs_physiques.volume_sonore = 0.2;
                context.capteurs_physiques.luminosite = 0.7;
                context.capteurs_physiques.gyroscope_stabilite = 0.1;

                // Simulate technical states
                context.etats_internes.temperature_cpu = 55.0 + (counter % 10);
                context.etats_internes.temperature_gpu = 60.0 + (counter % 8);
                context.etats_internes.charge_cpu = 0.3 + (counter % 5) * 0.1;
                context.etats_internes.utilisation_ram = 0.5 + (counter % 3) * 0.15;
                context.etats_internes.stabilite_systeme = 0.95;

                // Simulate external feedbacks
                context.feedbacks_externes.validation_positive = (counter % 4) == 0;
                context.feedbacks_externes.encouragement_recu = (counter % 6) == 0;
                context.feedbacks_externes.alerte_externe = (counter % 20) == 0;
                context.feedbacks_externes.interaction_sociale = (counter % 8) == 0;

                context_callback_(context);
                std::cout << "STUB: Simulated context data sent" << std::endl;
                counter++;
            }
        }

        std::cout << "STUB: Context consumer simulation ended" << std::endl;
    }

    void RabbitMQManager::processEmotionalMessage(const std::string& message) {
        // Not used in stub mode
    }

    void RabbitMQManager::processContextMessage(const std::string& message) {
        // Not used in stub mode
    }

    void RabbitMQManager::declareQueue(const std::string& queue_name) {
        std::cout << "STUB: Queue declared: " << queue_name << std::endl;
    }

    bool RabbitMQManager::reconnectIfNeeded() {
        return connected_;
    }

    // Serialization methods (simplified JSON without nlohmann)
    std::string RabbitMQManager::serializeToJson(const ContextualizedEmotions& emotions) {
        std::stringstream ss;
        ss << "{"
           << "\"emotion_globale\":" << emotions.emotion_globale << ","
           << "\"contexte_detecte\":\"" << emotions.contexte_detecte << "\","
           << "\"gradient_danger_global\":" << emotions.gradient_danger_global << ","
           << "\"niveau_danger\":" << static_cast<int>(emotions.niveau_danger) << ","
           << "\"signal_amyghaleon\":" << (emotions.signal_amyghaleon ? "true" : "false") << ","
           << "\"text_id\":\"" << emotions.text_id << "\","
           << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
                  emotions.timestamp.time_since_epoch()).count()
           << "}";
        return ss.str();
    }

    std::string RabbitMQManager::serializeToJson(const AmygdaleonSignal& signal) {
        std::stringstream ss;
        ss << "{"
           << "\"urgence\":" << (signal.urgence ? "true" : "false") << ","
           << "\"niveau_danger\":" << static_cast<int>(signal.niveau_danger) << ","
           << "\"gradient_danger_global\":" << signal.gradient_danger_global << ","
           << "\"contexte_detecte\":\"" << signal.contexte_detecte << "\","
           << "\"recommandation_intervention\":\"" << signal.recommandation_intervention << "\","
           << "\"text_id\":\"" << signal.text_id << "\","
           << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
                  signal.timestamp.time_since_epoch()).count()
           << "}";
        return ss.str();
    }

    std::string RabbitMQManager::serializeToJson(const MemoryToConsolidate& memory) {
        std::stringstream ss;
        ss << "{"
           << "\"id_mcee\":\"" << memory.id_mcee << "\","
           << "\"statut\":\"" << memory.statut << "\","
           << "\"priorite\":" << static_cast<int>(memory.priorite) << ","
           << "\"contexte_detecte\":\"" << memory.contexte_detecte << "\","
           << "\"score_significativite\":" << memory.score_significativite << ","
           << "\"recommandation_traitement\":\"" << memory.recommandation_traitement << "\","
           << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
                  memory.timestamp.time_since_epoch()).count()
           << "}";
        return ss.str();
    }

    EmotionalInput RabbitMQManager::deserializeEmotionalInput(const std::string& json_str) {
        // Stub implementation - not used in simulation mode
        EmotionalInput input;
        return input;
    }

    ContextData RabbitMQManager::deserializeContextData(const std::string& json_str) {
        // Stub implementation - not used in simulation mode
        ContextData context;
        return context;
    }
}