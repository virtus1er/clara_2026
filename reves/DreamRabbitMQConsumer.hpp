#pragma once

/**
 * @file DreamRabbitMQConsumer.hpp
 * @brief Consumer RabbitMQ pour alimenter le DreamEngine en souvenirs
 * 
 * Écoute les queues:
 * - mcee.mct.events : événements de création/activation de souvenirs
 * - mcee.dream.commands : commandes (force_dream, interrupt, etc.)
 */

#include "DreamEngine.hpp"
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
#include <functional>
#include <iostream>

namespace MCEE {

using json = nlohmann::json;

/**
 * Configuration du consumer
 */
struct DreamConsumerConfig {
    std::string host = "localhost";
    int port = 5672;
    std::string user = "virtus";
    std::string password = "virtus@83";
    std::string vhost = "/";
    
    // Queues à écouter
    std::string mct_events_queue = "mcee.mct.events";
    std::string dream_commands_queue = "mcee.dream.commands";
    
    // Exchange pour publier le statut
    std::string dream_status_exchange = "mcee.dream.status";
};

/**
 * @class DreamRabbitMQConsumer
 * @brief Consomme les messages RabbitMQ et alimente le DreamEngine
 */
class DreamRabbitMQConsumer {
public:
    using MemoryCreatedCallback = std::function<void(const Memory&)>;
    using CommandCallback = std::function<void(const std::string&, const json&)>;
    
    explicit DreamRabbitMQConsumer(const DreamConsumerConfig& config = DreamConsumerConfig{})
        : config_(config)
        , running_(false)
    {}
    
    ~DreamRabbitMQConsumer() {
        stop();
    }
    
    /**
     * @brief Connecte le consumer à un DreamEngine
     */
    void connectToDreamEngine(DreamEngine& engine) {
        dreamEngine_ = &engine;
        
        // Callback par défaut pour les souvenirs
        onMemoryCreated_ = [this](const Memory& memory) {
            if (dreamEngine_) {
                dreamEngine_->addMemoryToMCT(memory);
            }
        };
        
        // Callback par défaut pour les commandes
        onCommand_ = [this](const std::string& command, const json& data) {
            handleCommand(command, data);
        };
    }
    
    /**
     * @brief Définit un callback custom pour les souvenirs
     */
    void setMemoryCreatedCallback(MemoryCreatedCallback callback) {
        onMemoryCreated_ = std::move(callback);
    }
    
    /**
     * @brief Définit un callback custom pour les commandes
     */
    void setCommandCallback(CommandCallback callback) {
        onCommand_ = std::move(callback);
    }
    
    /**
     * @brief Démarre le consumer
     */
    void start() {
        if (running_) return;
        
        running_ = true;
        
        consumerThread_ = std::thread([this]() {
            runConsumerLoop();
        });
        
        std::cout << "[DreamConsumer] Démarré\n";
    }
    
    /**
     * @brief Arrête le consumer
     */
    void stop() {
        running_ = false;
        
        if (consumerThread_.joinable()) {
            consumerThread_.join();
        }
        
        std::cout << "[DreamConsumer] Arrêté\n";
    }
    
    /**
     * @brief Publie le statut du DreamEngine
     */
    void publishStatus() {
        if (!channel_ || !dreamEngine_) return;
        
        try {
            json status;
            status["state"] = dreamStateToString(dreamEngine_->getCurrentState());
            status["cycle_progress"] = dreamEngine_->getCycleProgress();
            status["dream_phase_progress"] = dreamEngine_->getDreamPhaseProgress();
            status["time_since_last_dream_s"] = dreamEngine_->getTimeSinceLastDream_s();
            status["can_start_dream"] = dreamEngine_->canStartDream();
            
            auto stats = dreamEngine_->getStats();
            status["stats"] = {
                {"cycles_completed", stats.totalCyclesCompleted},
                {"memories_consolidated", stats.totalMemoriesConsolidated},
                {"memories_forgotten", stats.totalMemoriesForgotten},
                {"edges_created", stats.totalEdgesCreated},
                {"interruptions", stats.totalInterruptions},
                {"avg_consolidation_score", stats.averageConsolidationScore}
            };
            
            status["mct_size"] = dreamEngine_->getMCTMemories().size();
            
            auto message = AmqpClient::BasicMessage::Create(status.dump());
            message->ContentType("application/json");
            
            channel_->BasicPublish(
                config_.dream_status_exchange,
                "status",
                message
            );
            
        } catch (const std::exception& e) {
            std::cerr << "[DreamConsumer] Erreur publication statut: " << e.what() << "\n";
        }
    }
    
    bool isRunning() const { return running_; }

private:
    DreamConsumerConfig config_;
    DreamEngine* dreamEngine_ = nullptr;
    
    AmqpClient::Channel::ptr_t channel_;
    std::atomic<bool> running_;
    std::thread consumerThread_;
    
    MemoryCreatedCallback onMemoryCreated_;
    CommandCallback onCommand_;
    
    void runConsumerLoop() {
        try {
            // Créer la connexion
            channel_ = AmqpClient::Channel::Create(
                config_.host,
                config_.port,
                config_.user,
                config_.password,
                config_.vhost
            );
            
            // Déclarer les queues
            channel_->DeclareQueue(config_.mct_events_queue, false, true, false, false);
            channel_->DeclareQueue(config_.dream_commands_queue, false, true, false, false);
            
            // Déclarer l'exchange de statut
            channel_->DeclareExchange(config_.dream_status_exchange, "topic", false, true);
            
            // Commencer à consommer
            std::string eventsTag = channel_->BasicConsume(
                config_.mct_events_queue, "", true, true, false
            );
            
            std::string commandsTag = channel_->BasicConsume(
                config_.dream_commands_queue, "", true, true, false
            );
            
            std::cout << "[DreamConsumer] Connecté à RabbitMQ, écoute sur:\n"
                      << "  - " << config_.mct_events_queue << "\n"
                      << "  - " << config_.dream_commands_queue << "\n";
            
            while (running_) {
                try {
                    AmqpClient::Envelope::ptr_t envelope;
                    
                    // Polling avec timeout
                    if (channel_->BasicConsumeMessage(eventsTag, envelope, 50)) {
                        handleMCTEvent(envelope->Message()->Body());
                    }
                    
                    if (channel_->BasicConsumeMessage(commandsTag, envelope, 50)) {
                        handleDreamCommand(envelope->Message()->Body());
                    }
                    
                } catch (const AmqpClient::ConsumerCancelledException&) {
                    if (running_) {
                        std::cerr << "[DreamConsumer] Consumer annulé, reconnexion...\n";
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                }
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[DreamConsumer] Erreur: " << e.what() << "\n";
        }
        
        channel_.reset();
    }
    
    void handleMCTEvent(const std::string& body) {
        try {
            json event = json::parse(body);
            std::string eventType = event.value("event", "");
            
            if (eventType == "memory_created" || eventType == "memory_added") {
                Memory memory = parseMemory(event["data"]);
                
                if (onMemoryCreated_) {
                    onMemoryCreated_(memory);
                }
                
                std::cout << "[DreamConsumer] Souvenir reçu: " << memory.id << "\n";
                
            } else if (eventType == "memory_activated") {
                // Un souvenir existant a été réactivé
                // TODO: Incrémenter usage_count dans le buffer
                std::string memoryId = event["data"].value("id", "");
                std::cout << "[DreamConsumer] Souvenir activé: " << memoryId << "\n";
                
            } else if (eventType == "batch_memories") {
                // Batch de souvenirs
                if (event["data"].is_array()) {
                    for (const auto& memJson : event["data"]) {
                        Memory memory = parseMemory(memJson);
                        if (onMemoryCreated_) {
                            onMemoryCreated_(memory);
                        }
                    }
                    std::cout << "[DreamConsumer] Batch de " 
                              << event["data"].size() << " souvenirs reçu\n";
                }
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[DreamConsumer] Erreur parsing événement: " << e.what() << "\n";
        }
    }
    
    void handleDreamCommand(const std::string& body) {
        try {
            json cmd = json::parse(body);
            std::string command = cmd.value("command", "");
            json data = cmd.value("data", json::object());
            
            if (onCommand_) {
                onCommand_(command, data);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "[DreamConsumer] Erreur parsing commande: " << e.what() << "\n";
        }
    }
    
    void handleCommand(const std::string& command, const json& data) {
        if (!dreamEngine_) return;
        
        if (command == "force_dream_start") {
            dreamEngine_->forceDreamStart();
            std::cout << "[DreamConsumer] Commande: force_dream_start\n";
            
        } else if (command == "interrupt_dream") {
            dreamEngine_->interruptDream();
            std::cout << "[DreamConsumer] Commande: interrupt_dream\n";
            
        } else if (command == "get_status") {
            publishStatus();
            
        } else if (command == "clear_mct") {
            dreamEngine_->clearMCT();
            std::cout << "[DreamConsumer] Commande: clear_mct\n";
            
        } else if (command == "reset_stats") {
            dreamEngine_->resetStats();
            std::cout << "[DreamConsumer] Commande: reset_stats\n";
            
        } else if (command == "set_config") {
            DreamConfig newConfig = dreamEngine_->getConfig();
            
            if (data.contains("cycle_period_s")) {
                newConfig.cyclePeriod_s = data["cycle_period_s"];
            }
            if (data.contains("min_time_since_last_dream_s")) {
                newConfig.minTimeSinceLastDream_s = data["min_time_since_last_dream_s"];
            }
            if (data.contains("consolidation_threshold")) {
                newConfig.consolidationThreshold = data["consolidation_threshold"];
            }
            
            dreamEngine_->setConfig(newConfig);
            std::cout << "[DreamConsumer] Commande: set_config\n";
            
        } else {
            std::cout << "[DreamConsumer] Commande inconnue: " << command << "\n";
        }
    }
    
    Memory parseMemory(const json& j) {
        Memory memory;
        memory.id = j.value("id", generateUUID());
        memory.type = j.value("type", "episodic");
        memory.isSocial = j.value("is_social", false);
        memory.interlocuteur = j.value("interlocuteur", "");
        memory.contexte = j.value("contexte", "");
        memory.feedback = j.value("feedback", 0.0);
        memory.usageCount = j.value("usage_count", 1);
        memory.decisionalInfluence = j.value("decisional_influence", 0.0);
        memory.isTrauma = j.value("is_trauma", false);
        memory.timestamp = std::chrono::steady_clock::now();
        
        // Vecteur émotionnel
        memory.emotionalVector.fill(0.0);
        if (j.contains("emotional_vector") && j["emotional_vector"].is_array()) {
            const auto& ev = j["emotional_vector"];
            for (size_t i = 0; i < std::min(ev.size(), (size_t)24); ++i) {
                memory.emotionalVector[i] = ev[i].get<double>();
            }
        } else if (j.contains("emotions") && j["emotions"].is_array()) {
            const auto& ev = j["emotions"];
            for (size_t i = 0; i < std::min(ev.size(), (size_t)24); ++i) {
                memory.emotionalVector[i] = ev[i].get<double>();
            }
        }
        
        return memory;
    }
    
    std::string generateUUID() {
        static int counter = 0;
        return "mem_" + std::to_string(++counter) + "_" + 
               std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    }
};

} // namespace MCEE
