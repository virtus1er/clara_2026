/**
 * @file main.cpp
 * @brief Module Rêves et Mémoire - Service autonome
 * 
 * Entrées (RabbitMQ):
 *   - mcee.memory.episodic
 *   - mcee.memory.semantic
 *   - mcee.memory.procedural
 *   - mcee.memory.autobiographic
 *   - mcee.mct.snapshot
 *   - mcee.pattern.active
 *   - mcee.amyghaleon.alerts
 * 
 * Sorties (RabbitMQ):
 *   - mcee.mlt.consolidate
 *   - mcee.mlt.create_edge
 *   - mcee.mlt.forget
 *   - mcee.dream.status
 */

#include "DreamEngine.hpp"
#include <SimpleAmqpClient/SimpleAmqpClient.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <cstring>

using namespace MCEE;
using json = nlohmann::json;

// ═══════════════════════════════════════════════════════════════════════════
// CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════

struct Config {
    // RabbitMQ
    std::string rabbitmq_host = "localhost";
    int rabbitmq_port = 5672;
    std::string rabbitmq_user = "virtus";
    std::string rabbitmq_password = "virtus@83";
    
    // Queues entrée
    std::string q_episodic = "mcee.memory.episodic";
    std::string q_semantic = "mcee.memory.semantic";
    std::string q_procedural = "mcee.memory.procedural";
    std::string q_autobio = "mcee.memory.autobiographic";
    std::string q_mct = "mcee.mct.snapshot";
    std::string q_pattern = "mcee.pattern.active";
    std::string q_amyghaleon = "mcee.amyghaleon.alerts";
    
    // Queues sortie
    std::string q_consolidate = "mcee.mlt.consolidate";
    std::string q_create_edge = "mcee.mlt.create_edge";
    std::string q_forget = "mcee.mlt.forget";
    std::string q_status = "mcee.dream.status";
    
    // Cycle
    double cycle_hours = 12.0;
    double awake_hours = 9.0;
};

// ═══════════════════════════════════════════════════════════════════════════
// GLOBALS
// ═══════════════════════════════════════════════════════════════════════════

std::atomic<bool> g_running{true};
Config g_config;
AmqpClient::Channel::ptr_t g_channel;

// État partagé
std::string g_activePattern = "SERENITE";
std::array<double, 24> g_currentEmotions{};
std::atomic<bool> g_amyghaleonAlert{false};
std::mutex g_stateMutex;

void signalHandler(int) {
    std::cout << "\n[Main] Arrêt demandé...\n";
    g_running = false;
}

// ═══════════════════════════════════════════════════════════════════════════
// HELPERS
// ═══════════════════════════════════════════════════════════════════════════

Memory parseMemory(const json& j) {
    Memory m;
    m.id = j.value("id", "mem_" + std::to_string(rand()));
    m.type = j.value("type", "episodic");
    m.isSocial = j.value("is_social", false);
    m.interlocuteur = j.value("interlocuteur", "");
    m.contexte = j.value("contexte", "");
    m.feedback = j.value("feedback", 0.0);
    m.usageCount = j.value("usage_count", 1);
    m.decisionalInfluence = j.value("decisional_influence", 0.0);
    m.isTrauma = j.value("is_trauma", false);
    m.timestamp = std::chrono::steady_clock::now();

    m.emotionalVector.fill(0.0);
    if (j.contains("emotional_vector") && j["emotional_vector"].is_array()) {
        for (size_t i = 0; i < std::min(j["emotional_vector"].size(), (size_t)24); ++i) {
            m.emotionalVector[i] = j["emotional_vector"][i].get<double>();
        }
    }
    return m;
}

/**
 * Parse un snapshot MCTGraph enrichi
 */
void parseMCTGraphSnapshot(const json& j, DreamEngine& engine) {
    // Vérifier que c'est bien un snapshot MCTGraph
    if (!j.contains("word_nodes") || !j.contains("edges")) {
        return;  // Format ancien, ignorer
    }

    std::vector<WordNodeSnapshot> words;
    std::vector<CausalLink> causalLinks;
    CausalStats stats;

    // Parser les nœuds mot
    if (j["word_nodes"].is_array()) {
        for (const auto& wn : j["word_nodes"]) {
            WordNodeSnapshot w;
            w.id = wn.value("id", "");
            w.lemma = wn.value("lemma", "");
            w.pos = wn.value("pos", "");
            w.sentenceId = wn.value("sentence_id", "");
            w.sentimentScore = wn.value("sentiment_score", 0.0);
            w.isNegation = wn.value("is_negation", false);
            w.isIntensifier = wn.value("is_intensifier", false);
            words.push_back(w);
        }
    }

    // Parser les arêtes et extraire les liens causaux
    if (j["edges"].is_array()) {
        for (const auto& edge : j["edges"]) {
            std::string edgeType = edge.value("type", "");
            if (edgeType == "CAUSAL") {
                CausalLink link;
                link.wordId = edge.value("source_id", "");
                link.emotionId = edge.value("target_id", "");
                link.causalStrength = edge.value("weight", 0.5);
                link.temporalDistanceMs = edge.value("temporal_distance_ms", 0.0);
                link.timestamp = std::chrono::steady_clock::now();

                // Trouver le lemma du mot source
                for (const auto& w : words) {
                    if (w.id == link.wordId) {
                        link.wordLemma = w.lemma;
                        link.wordPos = w.pos;
                        break;
                    }
                }

                causalLinks.push_back(link);
                stats.totalCausalEdges++;
            }
        }
    }

    // Parser les nœuds émotion pour enrichir les liens causaux
    if (j["emotion_nodes"].is_array()) {
        for (auto& link : causalLinks) {
            for (const auto& en : j["emotion_nodes"]) {
                if (en.value("id", "") == link.emotionId) {
                    link.dominantEmotion = en.value("dominant_emotion", "");
                    break;
                }
            }
        }
    }

    // Parser les statistiques globales
    if (j.contains("stats")) {
        const auto& s = j["stats"];
        stats.mostFrequentEmotion = s.value("most_frequent_emotion", "");
        stats.averageEmotionIntensity = s.value("average_emotion_intensity", 0.0);
        stats.graphDensity = s.value("graph_density", 0.0);

        if (s.contains("top_trigger_words") && s["top_trigger_words"].is_array()) {
            for (const auto& tw : s["top_trigger_words"]) {
                stats.topTriggerWords.push_back(tw.get<std::string>());
            }
        }
    }

    // Envoyer au DreamEngine
    engine.processMCTGraphSnapshot(words, causalLinks, stats);

    std::cout << "[Consumer] MCTGraph snapshot: " << words.size() << " mots, "
              << causalLinks.size() << " liens causaux\n";
}

void publish(const std::string& queue, const json& payload) {
    if (!g_channel) return;
    try {
        auto msg = AmqpClient::BasicMessage::Create(payload.dump());
        msg->ContentType("application/json");
        g_channel->BasicPublish("", queue, msg);
    } catch (const std::exception& e) {
        std::cerr << "[RMQ] Erreur publish: " << e.what() << "\n";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// CONSUMER THREAD
// ═══════════════════════════════════════════════════════════════════════════

void consumerThread(DreamEngine& engine) {
    try {
        auto channel = AmqpClient::Channel::Create(
            g_config.rabbitmq_host, g_config.rabbitmq_port,
            g_config.rabbitmq_user, g_config.rabbitmq_password
        );
        
        // Déclarer les queues
        channel->DeclareQueue(g_config.q_episodic, false, true, false, false);
        channel->DeclareQueue(g_config.q_semantic, false, true, false, false);
        channel->DeclareQueue(g_config.q_procedural, false, true, false, false);
        channel->DeclareQueue(g_config.q_autobio, false, true, false, false);
        channel->DeclareQueue(g_config.q_mct, false, true, false, false);
        channel->DeclareQueue(g_config.q_pattern, false, true, false, false);
        channel->DeclareQueue(g_config.q_amyghaleon, false, true, false, false);
        
        // Consumers
        auto tagEpi = channel->BasicConsume(g_config.q_episodic, "", true, true, false);
        auto tagSem = channel->BasicConsume(g_config.q_semantic, "", true, true, false);
        auto tagProc = channel->BasicConsume(g_config.q_procedural, "", true, true, false);
        auto tagAuto = channel->BasicConsume(g_config.q_autobio, "", true, true, false);
        auto tagMct = channel->BasicConsume(g_config.q_mct, "", true, true, false);
        auto tagPat = channel->BasicConsume(g_config.q_pattern, "", true, true, false);
        auto tagAmy = channel->BasicConsume(g_config.q_amyghaleon, "", true, true, false);
        
        std::cout << "[Consumer] Écoute des queues...\n";
        
        while (g_running) {
            AmqpClient::Envelope::ptr_t env;
            
            // Mémoires
            auto processMemory = [&](const std::string& tag, const std::string& type) {
                if (channel->BasicConsumeMessage(tag, env, 10)) {
                    try {
                        json j = json::parse(env->Message()->Body());
                        Memory m = parseMemory(j);
                        m.type = type;
                        engine.addMemoryToMCT(m);
                        std::cout << "[Consumer] + " << type << ": " << m.id << "\n";
                    } catch (...) {}
                }
            };
            
            processMemory(tagEpi, "episodic");
            processMemory(tagSem, "semantic");
            processMemory(tagProc, "procedural");
            processMemory(tagAuto, "autobiographic");

            // Traitement spécial pour MCTGraph snapshots
            if (channel->BasicConsumeMessage(tagMct, env, 10)) {
                try {
                    json j = json::parse(env->Message()->Body());
                    // Vérifier si c'est un snapshot MCTGraph enrichi
                    if (j.contains("word_nodes") && j.contains("edges")) {
                        parseMCTGraphSnapshot(j, engine);
                    } else {
                        // Format ancien: traiter comme mémoire simple
                        Memory m = parseMemory(j);
                        m.type = "mct";
                        engine.addMemoryToMCT(m);
                        std::cout << "[Consumer] + mct (legacy): " << m.id << "\n";
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[Consumer] Erreur parsing MCT: " << e.what() << "\n";
                }
            }
            
            // Pattern
            if (channel->BasicConsumeMessage(tagPat, env, 10)) {
                try {
                    json j = json::parse(env->Message()->Body());
                    std::lock_guard<std::mutex> lock(g_stateMutex);
                    g_activePattern = j.value("pattern", "SERENITE");
                    if (j.contains("emotions") && j["emotions"].is_array()) {
                        for (size_t i = 0; i < std::min(j["emotions"].size(), (size_t)24); ++i) {
                            g_currentEmotions[i] = j["emotions"][i].get<double>();
                        }
                    }
                } catch (...) {}
            }
            
            // Amyghaleon
            if (channel->BasicConsumeMessage(tagAmy, env, 10)) {
                try {
                    json j = json::parse(env->Message()->Body());
                    g_amyghaleonAlert = j.value("alert", false);
                    if (g_amyghaleonAlert) {
                        std::cout << "[Consumer] ⚠️ Alerte Amyghaleon!\n";
                    }
                } catch (...) {}
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[Consumer] Erreur: " << e.what() << "\n";
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// MAIN
// ═══════════════════════════════════════════════════════════════════════════

void printBanner() {
    std::cout << R"(
╔═══════════════════════════════════════════════════════════╗
║         MODULE RÊVES ET MÉMOIRE - MCEE                    ║
╚═══════════════════════════════════════════════════════════╝
)" << std::endl;
}

int main(int argc, char* argv[]) {
    printBanner();
    
    // Parser args
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--host") == 0 && i+1 < argc)
            g_config.rabbitmq_host = argv[++i];
        else if (strcmp(argv[i], "--port") == 0 && i+1 < argc)
            g_config.rabbitmq_port = std::stoi(argv[++i]);
        else if (strcmp(argv[i], "--user") == 0 && i+1 < argc)
            g_config.rabbitmq_user = argv[++i];
        else if (strcmp(argv[i], "--pass") == 0 && i+1 < argc)
            g_config.rabbitmq_password = argv[++i];
        else if (strcmp(argv[i], "--cycle") == 0 && i+1 < argc)
            g_config.cycle_hours = std::stod(argv[++i]);
    }
    
    std::cout << "[Config] RabbitMQ: " << g_config.rabbitmq_host 
              << ":" << g_config.rabbitmq_port << "\n";
    std::cout << "[Config] Cycle: " << g_config.cycle_hours << "h\n\n";
    
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        // Connexion RabbitMQ pour publication
        g_channel = AmqpClient::Channel::Create(
            g_config.rabbitmq_host, g_config.rabbitmq_port,
            g_config.rabbitmq_user, g_config.rabbitmq_password
        );
        
        g_channel->DeclareQueue(g_config.q_consolidate, false, true, false, false);
        g_channel->DeclareQueue(g_config.q_create_edge, false, true, false, false);
        g_channel->DeclareQueue(g_config.q_forget, false, true, false, false);
        g_channel->DeclareQueue(g_config.q_status, false, true, false, false);
        
        // DreamEngine
        DreamConfig cfg;
        cfg.cyclePeriod_s = g_config.cycle_hours * 3600.0;
        cfg.minTimeSinceLastDream_s = g_config.awake_hours * 3600.0;
        
        DreamEngine engine(cfg);
        
        // Callbacks vers MLT
        engine.setNeo4jConsolidateCallback([](const Memory& m) {
            json payload;
            payload["action"] = "consolidate";
            payload["memory"] = {
                {"id", m.id}, {"type", m.type}, {"score", m.consolidationScore},
                {"is_trauma", m.isTrauma}, {"is_social", m.isSocial}
            };
            json ev = json::array();
            for (double e : m.emotionalVector) ev.push_back(e);
            payload["memory"]["emotional_vector"] = ev;
            publish(g_config.q_consolidate, payload);
            std::cout << "[MLT] → Consolidate: " << m.id << "\n";
        });
        
        engine.setNeo4jCreateEdgeCallback([](const MemoryEdge& e) {
            json payload;
            payload["action"] = "create_edge";
            payload["edge"] = {
                {"source", e.sourceId}, {"target", e.targetId},
                {"weight", e.weight}, {"type", e.relationType}
            };
            publish(g_config.q_create_edge, payload);
            std::cout << "[MLT] → Edge: " << e.sourceId << " → " << e.targetId << "\n";
        });
        
        engine.setNeo4jDeleteCallback([](const std::string& id) {
            json payload;
            payload["action"] = "forget";
            payload["memory_id"] = id;
            publish(g_config.q_forget, payload);
            std::cout << "[MLT] → Forget: " << id << "\n";
        });
        
        engine.setStateChangeCallback([](DreamState from, DreamState to) {
            std::cout << "[Dream] " << dreamStateToString(from) 
                      << " → " << dreamStateToString(to) << "\n";
        });
        
        // Lancer consumer
        std::thread consumer(consumerThread, std::ref(engine));
        
        std::cout << "[Main] Module démarré. Ctrl+C pour arrêter.\n\n";
        
        // Boucle principale
        int tick = 0;
        while (g_running) {
            // Récupérer état
            std::string pattern;
            std::array<double, 24> emotions;
            {
                std::lock_guard<std::mutex> lock(g_stateMutex);
                pattern = g_activePattern;
                emotions = g_currentEmotions;
            }
            
            // Update
            engine.update(emotions, pattern, g_amyghaleonAlert.load());
            g_amyghaleonAlert = false;
            
            // Status toutes les 10s
            if (++tick % 10 == 0) {
                auto stats = engine.getStats();
                auto causalLinks = engine.getCausalLinks();
                auto topWords = engine.getTopTriggerWords();

                json status;
                status["state"] = dreamStateToString(engine.getCurrentState());
                status["pattern"] = pattern;
                status["cycle_progress"] = engine.getCycleProgress();
                status["mct_size"] = engine.getMCTMemories().size();
                status["causal_links"] = causalLinks.size();
                status["stats"] = {
                    {"cycles", stats.totalCyclesCompleted},
                    {"consolidated", stats.totalMemoriesConsolidated},
                    {"edges", stats.totalEdgesCreated}
                };

                // Ajouter les mots déclencheurs
                if (!topWords.empty()) {
                    status["top_trigger_words"] = json::array();
                    for (const auto& w : topWords) {
                        status["top_trigger_words"].push_back(w);
                    }
                }

                publish(g_config.q_status, status);

                std::cout << "[Status] " << dreamStateToString(engine.getCurrentState())
                          << " | MCT: " << engine.getMCTMemories().size()
                          << " | Causal: " << causalLinks.size()
                          << " | Cycles: " << stats.totalCyclesCompleted << "\n";
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        consumer.join();
        
        // Stats finales
        auto stats = engine.getStats();
        std::cout << "\n[Stats] Cycles: " << stats.totalCyclesCompleted
                  << " | Consolidés: " << stats.totalMemoriesConsolidated
                  << " | Arêtes: " << stats.totalEdgesCreated << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "[Erreur] " << e.what() << "\n";
        return 1;
    }
    
    std::cout << "[Main] Arrêté.\n";
    return 0;
}
