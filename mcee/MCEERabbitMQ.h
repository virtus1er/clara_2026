//===== MCEERabbitMQ.h - SEULEMENT LES DECLARATIONS =====
#pragma once
#include "MCEETypes.h"
#include <functional>
#include <memory>
#include <thread>
#include <atomic>

// Forward declarations pour SimpleAmqpClient
namespace AmqpClient {
    class Channel;
    class Envelope;
}

namespace MCEE {
    class RabbitMQManager {
    public:
        using EmotionalInputCallback = std::function<void(const EmotionalInput&)>;
        using ContextDataCallback = std::function<void(const ContextData&)>;

        RabbitMQManager();
        ~RabbitMQManager();

        bool initialize(const std::string& host, int port,
                       const std::string& username, const std::string& password);
        void shutdown();

        // Subscriptions
        void subscribeToEmotionalInput(const std::string& queue, EmotionalInputCallback callback);
        void subscribeToContextData(const std::string& queue, ContextDataCallback callback);

        // Publications
        bool publishContextualizedEmotions(const std::string& queue, const ContextualizedEmotions& emotions);
        bool publishAmygdaleonSignal(const std::string& queue, const AmygdaleonSignal& signal);
        bool publishMemoryToConsolidate(const std::string& queue, const MemoryToConsolidate& memory);

        bool isConnected() const { return connected_; }

    private:
        std::shared_ptr<AmqpClient::Channel> channel_;
        std::atomic<bool> connected_{false};
        std::atomic<bool> should_stop_{false};

        // Consumer threads
        std::thread emotional_consumer_thread_;
        std::thread context_consumer_thread_;

        // Callbacks
        EmotionalInputCallback emotional_callback_;
        ContextDataCallback context_callback_;

        // Queue names
        std::string emotional_queue_;
        std::string context_queue_;

        // Connection parameters
        std::string connection_host_;
        int connection_port_;
        std::string connection_username_;
        std::string connection_password_;

        // Consumer methods
        void emotionalConsumerLoop();
        void contextConsumerLoop();
        void processEmotionalMessage(const std::string& message);
        void processContextMessage(const std::string& message);

        // Serialization methods
        std::string serializeToJson(const ContextualizedEmotions& emotions);
        std::string serializeToJson(const AmygdaleonSignal& signal);
        std::string serializeToJson(const MemoryToConsolidate& memory);
        EmotionalInput deserializeEmotionalInput(const std::string& json);
        ContextData deserializeContextData(const std::string& json);

        // Queue management
        void declareQueue(const std::string& queue_name);
        bool reconnectIfNeeded();
    };
}