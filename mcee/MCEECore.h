//===== MCEECore.h - SEULEMENT LES DECLARATIONS =====
#pragma once
#include "MCEETypes.h"
#include "MCEERabbitMQ.h"
#include "MCEEContextualizer.h"
#include <memory>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>

namespace MCEE {
    class MCEECore {
    public:
        MCEECore();
        ~MCEECore();

        bool initialize(const std::string& config_path);
        void start();
        void stop();
        bool isRunning() const { return running_; }

        // Métriques de performance
        double getAverageProcessingTime() const;
        double getCurrentCPUUsage() const;
        size_t getProcessedMessagesCount() const;

    private:
        std::unique_ptr<RabbitMQManager> rabbitmq_manager_;
        std::unique_ptr<EmotionContextualizer> contextualizer_;

        std::atomic<bool> running_{false};
        std::atomic<bool> should_stop_{false};

        // Thread synchronization
        std::mutex data_mutex_;
        std::condition_variable data_cv_;

        // Data buffers
        EmotionalInput latest_emotional_input_;
        ContextData latest_context_data_;
        bool has_emotional_data_ = false;
        bool has_context_data_ = false;

        // Performance metrics
        mutable std::mutex metrics_mutex_;
        std::vector<double> processing_times_;
        std::atomic<size_t> processed_messages_{0};

        // Main processing thread
        std::thread processing_thread_;

        void processingLoop();
        void processData();
        void onEmotionalInput(const EmotionalInput& input);
        void onContextData(const ContextData& context);

        void handleAmygdaleonTrigger(const ContextualizedEmotions& emotions);
        void handleMLTConsolidation(const EmotionalInput& input,
                                  const ContextualizedEmotions& emotions);

        void updateMetrics(double processing_time);
        std::string generateTextId() const;
    };
}