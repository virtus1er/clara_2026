#pragma once

/**
 * @file DreamNeo4jBridge.hpp
 * @brief Bridge entre DreamEngine et Neo4j via RabbitMQ
 * 
 * Envoie les commandes de consolidation/suppression au service Neo4j Python
 * via RabbitMQ au lieu d'appels directs.
 */

#include "DreamEngine.hpp"
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <nlohmann/json.hpp>
#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <iostream>

namespace MCEE {

using json = nlohmann::json;

/**
 * Configuration RabbitMQ pour le bridge
 */
struct DreamRabbitMQConfig {
    std::string host = "localhost";
    int port = 5672;
    std::string user = "virtus";
    std::string password = "virtus@83";
    std::string vhost = "/";
    
    // Queues
    std::string neo4j_request_queue = "mcee.neo4j.request";
    std::string neo4j_response_queue = "mcee.neo4j.response";
    std::string dream_events_exchange = "mcee.dream.events";
};

/**
 * @class DreamNeo4jBridge
 * @brief Envoie les opérations DreamEngine vers Neo4j via RabbitMQ
 */
class DreamNeo4jBridge {
public:
    explicit DreamNeo4jBridge(const DreamRabbitMQConfig& config = DreamRabbitMQConfig{})
        : config_(config)
    {
        connect();
    }
    
    ~DreamNeo4jBridge() {
        disconnect();
    }
    
    /**
     * @brief Connecte les callbacks du DreamEngine au bridge Neo4j
     */
    void connectToDreamEngine(DreamEngine& engine) {
        // Callback de consolidation
        engine.setNeo4jConsolidateCallback([this](const Memory& memory) {
            consolidateMemory(memory);
        });
        
        // Callback de renforcement d'arête
        engine.setNeo4jReinforceCallback([this](const MemoryEdge& edge, double newWeight) {
            reinforceEdge(edge, newWeight);
        });
        
        // Callback de suppression
        engine.setNeo4jDeleteCallback([this](const std::string& memoryId) {
            deleteMemory(memoryId);
        });
        
        // Callback de création d'arête
        engine.setNeo4jCreateEdgeCallback([this](const MemoryEdge& edge) {
            createEdge(edge);
        });
        
        // Callback de changement d'état (pour logging/monitoring)
        engine.setStateChangeCallback([this](DreamState oldState, DreamState newState) {
            publishDreamStateChange(oldState, newState);
        });
        
        std::cout << "[DreamNeo4jBridge] Callbacks connectés au DreamEngine\n";
    }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // OPÉRATIONS NEO4J
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Consolide un souvenir vers MLT (Neo4j)
     */
    void consolidateMemory(const Memory& memory) {
        json payload;
        payload["action"] = "consolidate_memory";
        payload["data"] = {
            {"id", memory.id},
            {"type", memory.type},
            {"is_social", memory.isSocial},
            {"interlocuteur", memory.interlocuteur},
            {"contexte", memory.contexte},
            {"feedback", memory.feedback},
            {"usage_count", memory.usageCount},
            {"decisional_influence", memory.decisionalInfluence},
            {"is_trauma", memory.isTrauma},
            {"consolidation_score", memory.consolidationScore}
        };
        
        // Convertir le vecteur émotionnel
        json emotions = json::array();
        for (double e : memory.emotionalVector) {
            emotions.push_back(e);
        }
        payload["data"]["emotional_vector"] = emotions;
        
        sendToNeo4j(payload);
        
        std::cout << "[DreamNeo4jBridge] → Consolidation: " << memory.id 
                  << " (score=" << memory.consolidationScore << ")\n";
    }
    
    /**
     * @brief Renforce une arête existante dans le graphe
     */
    void reinforceEdge(const MemoryEdge& edge, double newWeight) {
        json payload;
        payload["action"] = "reinforce_edge";
        payload["data"] = {
            {"source_id", edge.sourceId},
            {"target_id", edge.targetId},
            {"new_weight", newWeight},
            {"relation_type", edge.relationType}
        };
        
        sendToNeo4j(payload);
        
        std::cout << "[DreamNeo4jBridge] → Renforcement: " 
                  << edge.sourceId << " → " << edge.targetId 
                  << " (poids=" << newWeight << ")\n";
    }
    
    /**
     * @brief Supprime un souvenir du graphe
     */
    void deleteMemory(const std::string& memoryId) {
        json payload;
        payload["action"] = "delete_memory";
        payload["data"] = {
            {"id", memoryId}
        };
        
        sendToNeo4j(payload);
        
        std::cout << "[DreamNeo4jBridge] → Suppression: " << memoryId << "\n";
    }
    
    /**
     * @brief Crée une nouvelle arête (association stochastique)
     */
    void createEdge(const MemoryEdge& edge) {
        json payload;
        payload["action"] = "create_edge";
        payload["data"] = {
            {"source_id", edge.sourceId},
            {"target_id", edge.targetId},
            {"weight", edge.weight},
            {"relation_type", edge.relationType}
        };
        
        sendToNeo4j(payload);
        
        std::cout << "[DreamNeo4jBridge] → Nouvelle arête: " 
                  << edge.sourceId << " --[" << edge.relationType << "]--> " 
                  << edge.targetId << "\n";
    }
    
    /**
     * @brief Déclenche un cycle de rêve complet via Neo4j
     */
    void triggerFullDreamCycle(double importanceThreshold = 0.6,
                                int maxMctAgeHours = 24,
                                double minWeightToKeep = 0.1) {
        json payload;
        payload["action"] = "dream_cycle";
        payload["data"] = {
            {"importance_threshold", importanceThreshold},
            {"max_mct_age_hours", maxMctAgeHours},
            {"min_weight_to_keep", minWeightToKeep}
        };
        
        sendToNeo4j(payload);
        
        std::cout << "[DreamNeo4jBridge] → Cycle de rêve déclenché\n";
    }
    
    /**
     * @brief Publie un changement d'état du rêve
     */
    void publishDreamStateChange(DreamState oldState, DreamState newState) {
        if (!channel_) return;
        
        try {
            json event;
            event["event"] = "dream_state_change";
            event["old_state"] = dreamStateToString(oldState);
            event["new_state"] = dreamStateToString(newState);
            event["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();
            
            auto message = AmqpClient::BasicMessage::Create(event.dump());
            message->ContentType("application/json");
            
            channel_->BasicPublish(
                config_.dream_events_exchange,
                "state.change",
                message
            );
        } catch (const std::exception& e) {
            std::cerr << "[DreamNeo4jBridge] Erreur publication: " << e.what() << "\n";
        }
    }
    
    bool isConnected() const { return channel_ != nullptr; }

private:
    DreamRabbitMQConfig config_;
    AmqpClient::Channel::ptr_t channel_;
    
    void connect() {
        try {
            channel_ = AmqpClient::Channel::Create(
                config_.host,
                config_.port,
                config_.user,
                config_.password,
                config_.vhost
            );
            
            // Déclarer la queue de requêtes Neo4j
            channel_->DeclareQueue(config_.neo4j_request_queue, false, true, false, false);
            
            // Déclarer l'exchange pour les événements de rêve
            channel_->DeclareExchange(config_.dream_events_exchange, "topic", false, true);
            
            std::cout << "[DreamNeo4jBridge] Connecté à RabbitMQ (" 
                      << config_.host << ":" << config_.port << ")\n";
                      
        } catch (const std::exception& e) {
            std::cerr << "[DreamNeo4jBridge] Erreur connexion: " << e.what() << "\n";
            channel_ = nullptr;
        }
    }
    
    void disconnect() {
        channel_.reset();
    }
    
    void sendToNeo4j(const json& payload) {
        if (!channel_) {
            std::cerr << "[DreamNeo4jBridge] Non connecté, message ignoré\n";
            return;
        }
        
        try {
            auto message = AmqpClient::BasicMessage::Create(payload.dump());
            message->ContentType("application/json");
            
            channel_->BasicPublish(
                "",  // Default exchange
                config_.neo4j_request_queue,
                message
            );
        } catch (const std::exception& e) {
            std::cerr << "[DreamNeo4jBridge] Erreur envoi: " << e.what() << "\n";
        }
    }
};

} // namespace MCEE
