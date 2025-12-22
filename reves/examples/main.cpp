#include "../DreamEngine.hpp"

#include <iostream>
#include <iomanip>
#include <thread>

using namespace MCEE;

int main() {
    std::cout << "========================================\n";
    std::cout << "DreamEngine - Example Usage\n";
    std::cout << "========================================\n\n";

    // Create a configuration with shorter cycle for demo
    DreamConfig config;
    config.cyclePeriod_s = 10.0;           // 10 seconds cycle
    config.minTimeSinceLastDream_s = 0.0;  // No minimum wait
    config.maxEmotionalActivityForDream = 1.0;  // Allow dream at any intensity

    // Create the engine
    DreamEngine engine(config);

    // Set up state change callback
    engine.setStateChangeCallback([](DreamState oldState, DreamState newState) {
        std::cout << "[STATE] " << dreamStateToString(oldState)
                  << " -> " << dreamStateToString(newState) << "\n";
    });

    // Set up Neo4j callbacks (simulated)
    engine.setNeo4jConsolidateCallback([](const Memory& memory) {
        std::cout << "[NEO4J] Consolidating memory: " << memory.id
                  << " (score: " << std::fixed << std::setprecision(2)
                  << memory.consolidationScore << ")\n";
    });

    engine.setNeo4jCreateEdgeCallback([](const MemoryEdge& edge) {
        std::cout << "[NEO4J] Creating edge: " << edge.sourceId
                  << " -> " << edge.targetId
                  << " (type: " << edge.relationType << ")\n";
    });

    engine.setNeo4jDeleteCallback([](const std::string& id) {
        std::cout << "[NEO4J] Deleting memory: " << id << "\n";
    });

    // Add some test memories
    std::cout << "Adding memories to MCT...\n";

    for (int i = 1; i <= 5; ++i) {
        Memory m;
        m.id = "memory_" + std::to_string(i);
        m.type = (i % 2 == 0) ? "episodic" : "semantic";
        m.isSocial = (i % 3 == 0);
        m.contexte = "test_context";
        m.feedback = 0.3 + (i * 0.1);
        m.usageCount = i;
        m.decisionalInfluence = 0.2 * i;
        m.timestamp = std::chrono::steady_clock::now();

        // Varied emotional vectors
        m.emotionalVector.fill(0.0);
        m.emotionalVector[i % 24] = 0.8;
        m.emotionalVector[(i + 5) % 24] = 0.5;

        engine.addMemoryToMCT(m);
        std::cout << "  Added: " << m.id << " (" << m.type << ")\n";
    }

    std::cout << "\nMCT now contains " << engine.getMCTMemories().size() << " memories\n";

    // Force start the dream cycle
    std::cout << "\n--- Starting Dream Cycle ---\n\n";
    engine.forceDreamStart();

    // Simulate the dream cycle with updates
    std::array<double, 24> emotionalState{};
    emotionalState.fill(0.1);

    std::cout << "Running dream cycle simulation...\n\n";

    for (int tick = 0; tick < 50; ++tick) {
        engine.update(emotionalState, "SERENITE", false);

        // Print progress every 10 ticks
        if (tick % 10 == 0) {
            std::cout << "[TICK " << std::setw(2) << tick << "] "
                      << "State: " << std::setw(18) << dreamStateToString(engine.getCurrentState())
                      << " | Phase progress: " << std::fixed << std::setprecision(0)
                      << (engine.getDreamPhaseProgress() * 100) << "%\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Print final stats
    std::cout << "\n--- Final Statistics ---\n";
    auto stats = engine.getStats();
    std::cout << "Cycles completed:      " << stats.totalCyclesCompleted << "\n";
    std::cout << "Memories consolidated: " << stats.totalMemoriesConsolidated << "\n";
    std::cout << "Memories forgotten:    " << stats.totalMemoriesForgotten << "\n";
    std::cout << "Edges created:         " << stats.totalEdgesCreated << "\n";
    std::cout << "Interruptions:         " << stats.totalInterruptions << "\n";
    std::cout << "Avg consolidation:     " << std::fixed << std::setprecision(3)
              << stats.averageConsolidationScore << "\n";

    std::cout << "\n========================================\n";
    std::cout << "Demo completed.\n";
    std::cout << "========================================\n";

    return 0;
}
