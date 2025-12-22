#include "../DreamEngine.hpp"

#include <iostream>
#include <cassert>
#include <thread>

using namespace MCEE;

// Helper to create a test memory
Memory createTestMemory(const std::string& id, bool isSocial = false, bool isTrauma = false) {
    Memory m;
    m.id = id;
    m.type = "episodic";
    m.isSocial = isSocial;
    m.isTrauma = isTrauma;
    m.feedback = 0.5;
    m.usageCount = 2;
    m.decisionalInfluence = 0.3;
    m.timestamp = std::chrono::steady_clock::now();
    m.emotionalVector.fill(0.1);
    return m;
}

void testConstruction() {
    std::cout << "Test: Construction... ";

    DreamEngine engine;
    assert(engine.getCurrentState() == DreamState::AWAKE);
    assert(engine.getMCTMemories().empty());

    std::cout << "OK\n";
}

void testConfigCustomization() {
    std::cout << "Test: Config customization... ";

    DreamConfig config;
    config.cyclePeriod_s = 60.0;  // 1 minute for testing
    config.minTimeSinceLastDream_s = 10.0;

    DreamEngine engine(config);
    assert(engine.getConfig().cyclePeriod_s == 60.0);

    DreamConfig newConfig;
    newConfig.cyclePeriod_s = 120.0;
    engine.setConfig(newConfig);
    assert(engine.getConfig().cyclePeriod_s == 120.0);

    std::cout << "OK\n";
}

void testMemoryManagement() {
    std::cout << "Test: Memory management... ";

    DreamEngine engine;

    assert(engine.getMCTMemories().empty());

    engine.addMemoryToMCT(createTestMemory("mem1"));
    assert(engine.getMCTMemories().size() == 1);

    engine.addMemoryToMCT(createTestMemory("mem2", true));
    assert(engine.getMCTMemories().size() == 2);

    engine.clearMCT();
    assert(engine.getMCTMemories().empty());

    std::cout << "OK\n";
}

void testForceDreamStart() {
    std::cout << "Test: Force dream start... ";

    DreamEngine engine;
    assert(engine.getCurrentState() == DreamState::AWAKE);

    engine.forceDreamStart();
    assert(engine.getCurrentState() == DreamState::DREAM_SCAN);
    assert(isDreaming(engine.getCurrentState()));

    std::cout << "OK\n";
}

void testInterruptDream() {
    std::cout << "Test: Interrupt dream... ";

    DreamEngine engine;
    engine.forceDreamStart();
    assert(isDreaming(engine.getCurrentState()));

    engine.interruptDream();
    assert(engine.getCurrentState() == DreamState::INTERRUPTED);

    std::cout << "OK\n";
}

void testStateChangeCallback() {
    std::cout << "Test: State change callback... ";

    DreamEngine engine;
    bool callbackCalled = false;
    DreamState oldStateReceived = DreamState::AWAKE;
    DreamState newStateReceived = DreamState::AWAKE;

    engine.setStateChangeCallback([&](DreamState oldState, DreamState newState) {
        callbackCalled = true;
        oldStateReceived = oldState;
        newStateReceived = newState;
    });

    engine.forceDreamStart();

    assert(callbackCalled);
    assert(oldStateReceived == DreamState::AWAKE);
    assert(newStateReceived == DreamState::DREAM_SCAN);

    std::cout << "OK\n";
}

void testDreamStateToString() {
    std::cout << "Test: DreamState to string... ";

    assert(dreamStateToString(DreamState::AWAKE) == "AWAKE");
    assert(dreamStateToString(DreamState::DREAM_SCAN) == "DREAM_SCAN");
    assert(dreamStateToString(DreamState::DREAM_CONSOLIDATE) == "DREAM_CONSOLIDATE");
    assert(dreamStateToString(DreamState::DREAM_EXPLORE) == "DREAM_EXPLORE");
    assert(dreamStateToString(DreamState::DREAM_CLEANUP) == "DREAM_CLEANUP");
    assert(dreamStateToString(DreamState::INTERRUPTED) == "INTERRUPTED");

    std::cout << "OK\n";
}

void testIsDreaming() {
    std::cout << "Test: isDreaming helper... ";

    assert(!isDreaming(DreamState::AWAKE));
    assert(isDreaming(DreamState::DREAM_SCAN));
    assert(isDreaming(DreamState::DREAM_CONSOLIDATE));
    assert(isDreaming(DreamState::DREAM_EXPLORE));
    assert(isDreaming(DreamState::DREAM_CLEANUP));
    assert(!isDreaming(DreamState::INTERRUPTED));

    std::cout << "OK\n";
}

void testStats() {
    std::cout << "Test: Statistics... ";

    DreamEngine engine;
    auto stats = engine.getStats();

    assert(stats.totalCyclesCompleted == 0);
    assert(stats.totalMemoriesConsolidated == 0);
    assert(stats.totalMemoriesForgotten == 0);
    assert(stats.totalEdgesCreated == 0);
    assert(stats.totalInterruptions == 0);

    engine.forceDreamStart();
    engine.interruptDream();

    stats = engine.getStats();
    assert(stats.totalInterruptions == 1);

    engine.resetStats();
    stats = engine.getStats();
    assert(stats.totalInterruptions == 0);

    std::cout << "OK\n";
}

void testCycleProgress() {
    std::cout << "Test: Cycle progress... ";

    DreamEngine engine;
    double progress = engine.getCycleProgress();

    // Progress should be between 0 and 1
    assert(progress >= 0.0 && progress <= 1.0);

    std::cout << "OK\n";
}

void testDreamConfigHelpers() {
    std::cout << "Test: DreamConfig helpers... ";

    DreamConfig config;
    config.cyclePeriod_s = 100.0;
    config.dreamRatio = 0.2;
    config.scanPhaseRatio = 0.1;
    config.consolidateRatio = 0.6;
    config.exploreRatio = 0.2;
    config.cleanupRatio = 0.1;

    assert(config.dreamDuration_s() == 20.0);
    assert(config.scanDuration_s() == 2.0);
    assert(config.consolidateDuration_s() == 12.0);
    assert(config.exploreDuration_s() == 4.0);
    assert(config.cleanupDuration_s() == 2.0);

    std::cout << "OK\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "DreamEngine Unit Tests\n";
    std::cout << "========================================\n\n";

    testConstruction();
    testConfigCustomization();
    testMemoryManagement();
    testForceDreamStart();
    testInterruptDream();
    testStateChangeCallback();
    testDreamStateToString();
    testIsDreaming();
    testStats();
    testCycleProgress();
    testDreamConfigHelpers();

    std::cout << "\n========================================\n";
    std::cout << "All tests passed!\n";
    std::cout << "========================================\n";

    return 0;
}
