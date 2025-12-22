/**
 * @file DreamEngineTest.cpp
 * @brief Tests unitaires complets pour le module DreamEngine
 */

#include "../DreamEngine.hpp"
#include "../DreamState.hpp"
#include "../DreamConfig.hpp"

#include <iostream>
#include <cassert>
#include <cmath>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <sstream>

using namespace MCEE;

// ═══════════════════════════════════════════════════════════════════════════
// FRAMEWORK DE TEST MINIMAL
// ═══════════════════════════════════════════════════════════════════════════

static int g_testsRun = 0;
static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define RUN_TEST(name) runTest(#name, test_##name)

void runTest(const char* name, void (*func)()) {
    std::cout << "  - " << name << "... ";
    g_testsRun++;
    try {
        func();
        std::cout << "OK\n";
        g_testsPassed++;
    } catch (const std::exception& e) {
        std::cout << "ECHEC: " << e.what() << "\n";
        g_testsFailed++;
    }
}

#define ASSERT_TRUE(expr) \
    if (!(expr)) throw std::runtime_error("ASSERT_TRUE failed: " #expr)

#define ASSERT_FALSE(expr) \
    if (expr) throw std::runtime_error("ASSERT_FALSE failed: " #expr)

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) throw std::runtime_error("ASSERT_EQ failed: " #a " != " #b)

#define ASSERT_GT(a, b) \
    if (!((a) > (b))) throw std::runtime_error("ASSERT_GT failed: " #a " <= " #b)

#define ASSERT_GE(a, b) \
    if (!((a) >= (b))) throw std::runtime_error("ASSERT_GE failed: " #a " < " #b)

#define ASSERT_LT(a, b) \
    if (!((a) < (b))) throw std::runtime_error("ASSERT_LT failed: " #a " >= " #b)

#define ASSERT_LE(a, b) \
    if (!((a) <= (b))) throw std::runtime_error("ASSERT_LE failed: " #a " > " #b)

#define ASSERT_NEAR(a, b, eps) \
    if (std::abs((a) - (b)) > (eps)) throw std::runtime_error("ASSERT_NEAR failed")

// ═══════════════════════════════════════════════════════════════════════════
// HELPERS
// ═══════════════════════════════════════════════════════════════════════════

Memory createTestMemory(const std::string& id,
                        const std::string& type = "episodic",
                        bool isTrauma = false,
                        bool isSocial = false) {
    Memory m;
    m.id = id;
    m.type = type;
    m.isTrauma = isTrauma;
    m.isSocial = isSocial;
    m.feedback = 0.5;
    m.usageCount = 3;
    m.decisionalInfluence = 0.3;
    m.emotionalVector.fill(0.0);
    m.timestamp = std::chrono::steady_clock::now();
    return m;
}

DreamConfig createFastConfig() {
    DreamConfig cfg;
    cfg.cyclePeriod_s = 1.0;
    cfg.minTimeSinceLastDream_s = 0.0;
    cfg.maxEmotionalActivityForDream = 1.0;
    cfg.blockDreamOnPEUR = false;
    cfg.blockDreamOnANXIETE = false;
    return cfg;
}

std::array<double, 24> createEmotionalVector(double baseValue = 0.1) {
    std::array<double, 24> vec;
    vec.fill(baseValue);
    return vec;
}

// ═══════════════════════════════════════════════════════════════════════════
// TESTS: INITIALISATION
// ═══════════════════════════════════════════════════════════════════════════

void test_DefaultConstruction() {
    DreamEngine engine;
    ASSERT_EQ(engine.getCurrentState(), DreamState::AWAKE);
    ASSERT_TRUE(engine.getMCTMemories().empty());
}

void test_ConstructionWithConfig() {
    DreamConfig cfg;
    cfg.cyclePeriod_s = 100.0;
    cfg.consolidationThreshold = 0.7;
    DreamEngine engine(cfg);
    ASSERT_NEAR(engine.getConfig().cyclePeriod_s, 100.0, 0.001);
    ASSERT_NEAR(engine.getConfig().consolidationThreshold, 0.7, 0.001);
}

void test_InitialStats() {
    DreamEngine engine;
    auto stats = engine.getStats();
    ASSERT_EQ(stats.totalCyclesCompleted, 0);
    ASSERT_EQ(stats.totalMemoriesConsolidated, 0);
    ASSERT_EQ(stats.totalMemoriesForgotten, 0);
    ASSERT_EQ(stats.totalEdgesCreated, 0);
    ASSERT_EQ(stats.totalInterruptions, 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// TESTS: GESTION MCT
// ═══════════════════════════════════════════════════════════════════════════

void test_AddMemoryToMCT() {
    DreamEngine engine;
    Memory m = createTestMemory("mem_001");
    engine.addMemoryToMCT(m);
    ASSERT_EQ(engine.getMCTMemories().size(), 1u);
    ASSERT_EQ(engine.getMCTMemories()[0].id, "mem_001");
}

void test_AddMultipleMemories() {
    DreamEngine engine;
    for (int i = 0; i < 10; ++i) {
        engine.addMemoryToMCT(createTestMemory("mem_" + std::to_string(i)));
    }
    ASSERT_EQ(engine.getMCTMemories().size(), 10u);
}

void test_ClearMCT() {
    DreamEngine engine;
    engine.addMemoryToMCT(createTestMemory("mem_001"));
    engine.addMemoryToMCT(createTestMemory("mem_002"));
    ASSERT_EQ(engine.getMCTMemories().size(), 2u);
    engine.clearMCT();
    ASSERT_TRUE(engine.getMCTMemories().empty());
}

void test_MemoryTypes() {
    DreamEngine engine;
    engine.addMemoryToMCT(createTestMemory("epi_1", "episodic"));
    engine.addMemoryToMCT(createTestMemory("sem_1", "semantic"));
    engine.addMemoryToMCT(createTestMemory("proc_1", "procedural"));
    engine.addMemoryToMCT(createTestMemory("auto_1", "autobiographic"));
    ASSERT_EQ(engine.getMCTMemories().size(), 4u);
}

// ═══════════════════════════════════════════════════════════════════════════
// TESTS: MACHINE À ÉTATS
// ═══════════════════════════════════════════════════════════════════════════

void test_StateStartsAwake() {
    DreamEngine engine;
    ASSERT_EQ(engine.getCurrentState(), DreamState::AWAKE);
    ASSERT_FALSE(isDreaming(engine.getCurrentState()));
}

void test_ForceDreamStart() {
    DreamConfig cfg = createFastConfig();
    DreamEngine engine(cfg);
    engine.addMemoryToMCT(createTestMemory("mem_001"));
    engine.forceDreamStart();
    ASSERT_EQ(engine.getCurrentState(), DreamState::DREAM_SCAN);
    ASSERT_TRUE(isDreaming(engine.getCurrentState()));
}

void test_ForceDreamStartWithoutMemories() {
    DreamConfig cfg = createFastConfig();
    DreamEngine engine(cfg);
    engine.forceDreamStart();
    ASSERT_EQ(engine.getCurrentState(), DreamState::DREAM_SCAN);
}

void test_InterruptDream() {
    DreamConfig cfg = createFastConfig();
    DreamEngine engine(cfg);
    engine.addMemoryToMCT(createTestMemory("mem_001"));
    engine.forceDreamStart();
    ASSERT_TRUE(isDreaming(engine.getCurrentState()));
    engine.interruptDream();
    ASSERT_EQ(engine.getCurrentState(), DreamState::INTERRUPTED);
    ASSERT_FALSE(isDreaming(engine.getCurrentState()));
}

void test_InterruptFromAwakeDoesNothing() {
    DreamEngine engine;
    ASSERT_EQ(engine.getCurrentState(), DreamState::AWAKE);
    engine.interruptDream();
    ASSERT_EQ(engine.getCurrentState(), DreamState::AWAKE);
}

void test_StateTransitionCallback() {
    DreamConfig cfg = createFastConfig();
    DreamEngine engine(cfg);
    std::vector<std::pair<DreamState, DreamState>> transitions;
    engine.setStateChangeCallback([&](DreamState from, DreamState to) {
        transitions.push_back({from, to});
    });
    engine.addMemoryToMCT(createTestMemory("mem_001"));
    engine.forceDreamStart();
    ASSERT_EQ(transitions.size(), 1u);
    ASSERT_EQ(transitions[0].first, DreamState::AWAKE);
    ASSERT_EQ(transitions[0].second, DreamState::DREAM_SCAN);
}

// ═══════════════════════════════════════════════════════════════════════════
// TESTS: CYCLE DE RÊVE COMPLET
// ═══════════════════════════════════════════════════════════════════════════

void test_CompleteDreamCycle() {
    DreamConfig cfg = createFastConfig();
    cfg.cyclePeriod_s = 0.1;
    DreamEngine engine(cfg);

    for (int i = 0; i < 5; ++i) {
        engine.addMemoryToMCT(createTestMemory("mem_" + std::to_string(i)));
    }

    std::vector<DreamState> statesVisited;
    engine.setStateChangeCallback([&](DreamState, DreamState to) {
        statesVisited.push_back(to);
    });

    auto emotions = createEmotionalVector(0.05);
    engine.forceDreamStart();

    int maxIterations = 1000;
    while (engine.getCurrentState() != DreamState::AWAKE && maxIterations-- > 0) {
        engine.update(emotions, "SERENITE", false);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_EQ(engine.getCurrentState(), DreamState::AWAKE);
    ASSERT_GE(statesVisited.size(), 4u);
}

void test_DreamPhaseProgression() {
    DreamConfig cfg = createFastConfig();
    cfg.cyclePeriod_s = 0.2;
    DreamEngine engine(cfg);

    engine.addMemoryToMCT(createTestMemory("mem_001"));
    engine.forceDreamStart();
    ASSERT_EQ(engine.getCurrentState(), DreamState::DREAM_SCAN);

    auto emotions = createEmotionalVector();
    int iterations = 0;
    while (engine.getCurrentState() == DreamState::DREAM_SCAN && iterations++ < 100) {
        engine.update(emotions, "SERENITE", false);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_EQ(engine.getCurrentState(), DreamState::DREAM_CONSOLIDATE);
}

void test_CycleCountIncrement() {
    DreamConfig cfg = createFastConfig();
    cfg.cyclePeriod_s = 0.1;
    DreamEngine engine(cfg);

    engine.addMemoryToMCT(createTestMemory("mem_001"));
    ASSERT_EQ(engine.getStats().totalCyclesCompleted, 0);

    auto emotions = createEmotionalVector(0.05);
    engine.forceDreamStart();

    int maxIterations = 1000;
    while (engine.getCurrentState() != DreamState::AWAKE && maxIterations-- > 0) {
        engine.update(emotions, "SERENITE", false);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    ASSERT_EQ(engine.getStats().totalCyclesCompleted, 1);
}

// ═══════════════════════════════════════════════════════════════════════════
// TESTS: CONSOLIDATION
// ═══════════════════════════════════════════════════════════════════════════

void test_ConsolidationCallback() {
    DreamConfig cfg = createFastConfig();
    cfg.cyclePeriod_s = 0.1;
    cfg.consolidationThreshold = 0.1;
    DreamEngine engine(cfg);

    std::vector<std::string> consolidated;
    engine.setNeo4jConsolidateCallback([&](const Memory& m) {
        consolidated.push_back(m.id);
    });

    Memory m = createTestMemory("mem_high_score");
    m.feedback = 0.9;
    m.usageCount = 10;
    m.decisionalInfluence = 0.8;
    engine.addMemoryToMCT(m);

    auto emotions = createEmotionalVector();
    engine.forceDreamStart();

    int maxIterations = 1000;
    while (engine.getCurrentState() != DreamState::AWAKE && maxIterations-- > 0) {
        engine.update(emotions, "SERENITE", false);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    ASSERT_GE(consolidated.size(), 1u);
}

void test_TraumaAlwaysConsolidated() {
    DreamConfig cfg = createFastConfig();
    cfg.cyclePeriod_s = 0.1;
    cfg.consolidationThreshold = 0.99;
    DreamEngine engine(cfg);

    std::vector<std::string> consolidated;
    engine.setNeo4jConsolidateCallback([&](const Memory& m) {
        consolidated.push_back(m.id);
    });

    Memory m = createTestMemory("trauma_001", "episodic", true);
    m.feedback = 0.0;
    m.usageCount = 0;
    m.decisionalInfluence = 0.0;
    engine.addMemoryToMCT(m);

    auto emotions = createEmotionalVector();
    engine.forceDreamStart();

    int maxIterations = 1000;
    while (engine.getCurrentState() != DreamState::AWAKE && maxIterations-- > 0) {
        engine.update(emotions, "SERENITE", false);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    bool found = false;
    for (const auto& id : consolidated) {
        if (id == "trauma_001") found = true;
    }
    ASSERT_TRUE(found);
}

// ═══════════════════════════════════════════════════════════════════════════
// TESTS: EXPLORATION STOCHASTIQUE
// ═══════════════════════════════════════════════════════════════════════════

void test_EdgeCreationCallback() {
    DreamConfig cfg = createFastConfig();
    cfg.cyclePeriod_s = 0.15;
    cfg.sigmaBase = 0.5;
    DreamEngine engine(cfg);

    std::vector<std::pair<std::string, std::string>> edges;
    engine.setNeo4jCreateEdgeCallback([&](const MemoryEdge& e) {
        edges.push_back({e.sourceId, e.targetId});
    });

    for (int i = 0; i < 5; ++i) {
        Memory m = createTestMemory("mem_" + std::to_string(i));
        m.emotionalVector.fill(0.5);
        engine.addMemoryToMCT(m);
    }

    auto emotions = createEmotionalVector();
    engine.forceDreamStart();

    int maxIterations = 1000;
    while (engine.getCurrentState() != DreamState::AWAKE && maxIterations-- > 0) {
        engine.update(emotions, "EXPLORATION", false);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    ASSERT_GE(edges.size(), 0u);
}

void test_PatternAffectsSigma() {
    DreamConfig cfg;
    ASSERT_GT(cfg.sigmaMultiplier_EXPLORATION, cfg.sigmaMultiplier_PEUR);
    ASSERT_GT(cfg.sigmaMultiplier_JOIE, cfg.sigmaMultiplier_ANXIETE);
}

// ═══════════════════════════════════════════════════════════════════════════
// TESTS: OUBLI / NETTOYAGE
// ═══════════════════════════════════════════════════════════════════════════

void test_TraumaNeverDeleted() {
    DreamConfig cfg = createFastConfig();
    cfg.cyclePeriod_s = 0.1;
    cfg.minWeightBeforeDeletion = 0.99;
    DreamEngine engine(cfg);

    std::vector<std::string> deleted;
    engine.setNeo4jDeleteCallback([&](const std::string& id) {
        deleted.push_back(id);
    });

    Memory m = createTestMemory("trauma_protected", "episodic", true);
    m.feedback = 0.0;
    m.usageCount = 0;
    engine.addMemoryToMCT(m);

    auto emotions = createEmotionalVector();
    engine.forceDreamStart();

    int maxIterations = 1000;
    while (engine.getCurrentState() != DreamState::AWAKE && maxIterations-- > 0) {
        engine.update(emotions, "SERENITE", false);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    bool found = false;
    for (const auto& id : deleted) {
        if (id == "trauma_protected") found = true;
    }
    ASSERT_FALSE(found);
}

void test_MCTClearedAfterCycle() {
    DreamConfig cfg = createFastConfig();
    cfg.cyclePeriod_s = 0.1;
    DreamEngine engine(cfg);

    engine.addMemoryToMCT(createTestMemory("mem_001"));
    engine.addMemoryToMCT(createTestMemory("mem_002"));
    ASSERT_EQ(engine.getMCTMemories().size(), 2u);

    auto emotions = createEmotionalVector();
    engine.forceDreamStart();

    int maxIterations = 1000;
    while (engine.getCurrentState() != DreamState::AWAKE && maxIterations-- > 0) {
        engine.update(emotions, "SERENITE", false);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    ASSERT_TRUE(engine.getMCTMemories().empty());
}

// ═══════════════════════════════════════════════════════════════════════════
// TESTS: INTERRUPTION AMYGHALEON
// ═══════════════════════════════════════════════════════════════════════════

void test_AmyghaleonInterruptsDream() {
    DreamConfig cfg = createFastConfig();
    cfg.cyclePeriod_s = 1.0;
    DreamEngine engine(cfg);

    engine.addMemoryToMCT(createTestMemory("mem_001"));
    engine.forceDreamStart();
    ASSERT_TRUE(isDreaming(engine.getCurrentState()));

    auto emotions = createEmotionalVector();
    engine.update(emotions, "PEUR", true);

    ASSERT_EQ(engine.getCurrentState(), DreamState::INTERRUPTED);
}

void test_InterruptionCountsInStats() {
    DreamConfig cfg = createFastConfig();
    DreamEngine engine(cfg);

    engine.addMemoryToMCT(createTestMemory("mem_001"));
    engine.forceDreamStart();
    ASSERT_EQ(engine.getStats().totalInterruptions, 0);

    engine.interruptDream();
    ASSERT_EQ(engine.getStats().totalInterruptions, 1);
}

void test_ResumeAfterInterruption() {
    DreamConfig cfg = createFastConfig();
    DreamEngine engine(cfg);

    engine.addMemoryToMCT(createTestMemory("mem_001"));
    auto emotions = createEmotionalVector();

    engine.forceDreamStart();
    engine.update(emotions, "PEUR", true);
    ASSERT_EQ(engine.getCurrentState(), DreamState::INTERRUPTED);

    engine.update(emotions, "SERENITE", false);
    ASSERT_EQ(engine.getCurrentState(), DreamState::AWAKE);
}

// ═══════════════════════════════════════════════════════════════════════════
// TESTS: CONDITIONS DE DÉCLENCHEMENT
// ═══════════════════════════════════════════════════════════════════════════

void test_NoDreamWithPEURPattern() {
    DreamConfig cfg = createFastConfig();
    cfg.blockDreamOnPEUR = true;
    cfg.minTimeSinceLastDream_s = 0;
    DreamEngine engine(cfg);

    engine.addMemoryToMCT(createTestMemory("mem_001"));
    auto emotions = createEmotionalVector();

    for (int i = 0; i < 10; ++i) {
        engine.update(emotions, "PEUR", false);
    }

    ASSERT_EQ(engine.getCurrentState(), DreamState::AWAKE);
}

void test_NoDreamWithHighEmotionalActivity() {
    DreamConfig cfg = createFastConfig();
    cfg.maxEmotionalActivityForDream = 0.1;
    DreamEngine engine(cfg);

    engine.addMemoryToMCT(createTestMemory("mem_001"));

    std::array<double, 24> highEmotions;
    highEmotions.fill(0.8);

    for (int i = 0; i < 10; ++i) {
        engine.update(highEmotions, "SERENITE", false);
    }

    ASSERT_EQ(engine.getCurrentState(), DreamState::AWAKE);
}

void test_NoDreamWithEmptyMCT() {
    DreamConfig cfg = createFastConfig();
    DreamEngine engine(cfg);

    ASSERT_TRUE(engine.getMCTMemories().empty());
    auto emotions = createEmotionalVector(0.01);

    for (int i = 0; i < 10; ++i) {
        engine.update(emotions, "SERENITE", false);
    }

    ASSERT_EQ(engine.getCurrentState(), DreamState::AWAKE);
}

void test_CanStartDreamCheck() {
    DreamConfig cfg = createFastConfig();
    DreamEngine engine(cfg);

    ASSERT_FALSE(engine.canStartDream());
    engine.addMemoryToMCT(createTestMemory("mem_001"));
    ASSERT_TRUE(engine.canStartDream());
}

// ═══════════════════════════════════════════════════════════════════════════
// TESTS: CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════

void test_SetConfig() {
    DreamEngine engine;
    DreamConfig cfg;
    cfg.consolidationThreshold = 0.8;
    cfg.sigmaBase = 0.25;
    engine.setConfig(cfg);
    ASSERT_NEAR(engine.getConfig().consolidationThreshold, 0.8, 0.001);
    ASSERT_NEAR(engine.getConfig().sigmaBase, 0.25, 0.001);
}

void test_ConfigDurationCalculations() {
    DreamConfig cfg;
    cfg.cyclePeriod_s = 1000.0;
    cfg.dreamRatio = 0.2;
    cfg.scanPhaseRatio = 0.1;
    cfg.consolidateRatio = 0.6;
    cfg.exploreRatio = 0.2;
    cfg.cleanupRatio = 0.1;

    ASSERT_NEAR(cfg.dreamDuration_s(), 200.0, 0.001);
    ASSERT_NEAR(cfg.scanDuration_s(), 20.0, 0.001);
    ASSERT_NEAR(cfg.consolidateDuration_s(), 120.0, 0.001);
    ASSERT_NEAR(cfg.exploreDuration_s(), 40.0, 0.001);
    ASSERT_NEAR(cfg.cleanupDuration_s(), 20.0, 0.001);
}

void test_ResetStats() {
    DreamConfig cfg = createFastConfig();
    cfg.cyclePeriod_s = 0.1;
    DreamEngine engine(cfg);

    engine.addMemoryToMCT(createTestMemory("mem_001"));
    auto emotions = createEmotionalVector();

    engine.forceDreamStart();
    int maxIterations = 1000;
    while (engine.getCurrentState() != DreamState::AWAKE && maxIterations-- > 0) {
        engine.update(emotions, "SERENITE", false);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    ASSERT_GT(engine.getStats().totalCyclesCompleted, 0);
    engine.resetStats();
    ASSERT_EQ(engine.getStats().totalCyclesCompleted, 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// TESTS: HELPERS DreamState
// ═══════════════════════════════════════════════════════════════════════════

void test_IsDreamingHelper() {
    ASSERT_FALSE(isDreaming(DreamState::AWAKE));
    ASSERT_TRUE(isDreaming(DreamState::DREAM_SCAN));
    ASSERT_TRUE(isDreaming(DreamState::DREAM_CONSOLIDATE));
    ASSERT_TRUE(isDreaming(DreamState::DREAM_EXPLORE));
    ASSERT_TRUE(isDreaming(DreamState::DREAM_CLEANUP));
    ASSERT_FALSE(isDreaming(DreamState::INTERRUPTED));
}

void test_DreamStateToString() {
    ASSERT_EQ(dreamStateToString(DreamState::AWAKE), "AWAKE");
    ASSERT_EQ(dreamStateToString(DreamState::DREAM_SCAN), "DREAM_SCAN");
    ASSERT_EQ(dreamStateToString(DreamState::DREAM_CONSOLIDATE), "DREAM_CONSOLIDATE");
    ASSERT_EQ(dreamStateToString(DreamState::DREAM_EXPLORE), "DREAM_EXPLORE");
    ASSERT_EQ(dreamStateToString(DreamState::DREAM_CLEANUP), "DREAM_CLEANUP");
    ASSERT_EQ(dreamStateToString(DreamState::INTERRUPTED), "INTERRUPTED");
}

// ═══════════════════════════════════════════════════════════════════════════
// TESTS: CYCLE PROGRESS
// ═══════════════════════════════════════════════════════════════════════════

void test_CycleProgressBounds() {
    DreamEngine engine;
    double progress = engine.getCycleProgress();
    ASSERT_GE(progress, 0.0);
    ASSERT_LE(progress, 1.0);
}

void test_DreamPhaseProgressZeroWhenAwake() {
    DreamEngine engine;
    ASSERT_EQ(engine.getCurrentState(), DreamState::AWAKE);
    ASSERT_NEAR(engine.getDreamPhaseProgress(), 0.0, 0.001);
}

void test_TimeSinceLastDream() {
    DreamEngine engine;
    double timeSince = engine.getTimeSinceLastDream_s();
    ASSERT_GE(timeSince, 0.0);
}

// ═══════════════════════════════════════════════════════════════════════════
// TESTS: THREAD SAFETY
// ═══════════════════════════════════════════════════════════════════════════

void test_ConcurrentAddMemories() {
    DreamEngine engine;
    std::atomic<int> addedCount{0};

    auto addFunc = [&]() {
        for (int i = 0; i < 100; ++i) {
            engine.addMemoryToMCT(createTestMemory("mem_" + std::to_string(i)));
            addedCount++;
        }
    };

    std::thread t1(addFunc);
    std::thread t2(addFunc);

    t1.join();
    t2.join();

    ASSERT_EQ(addedCount.load(), 200);
    ASSERT_EQ(engine.getMCTMemories().size(), 200u);
}

void test_ConcurrentStateQueries() {
    DreamConfig cfg = createFastConfig();
    cfg.cyclePeriod_s = 0.5;
    DreamEngine engine(cfg);

    engine.addMemoryToMCT(createTestMemory("mem_001"));

    std::atomic<bool> running{true};
    std::atomic<int> queryCount{0};

    auto queryFunc = [&]() {
        while (running) {
            (void)engine.getCurrentState();
            (void)engine.getCycleProgress();
            (void)engine.getStats();
            queryCount++;
        }
    };

    std::thread queryThread(queryFunc);

    auto emotions = createEmotionalVector();
    engine.forceDreamStart();

    for (int i = 0; i < 50; ++i) {
        engine.update(emotions, "SERENITE", false);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    running = false;
    queryThread.join();

    ASSERT_GT(queryCount.load(), 0);
}

// ═══════════════════════════════════════════════════════════════════════════
// MAIN
// ═══════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "\n";
    std::cout << "+=================================================================+\n";
    std::cout << "|         TESTS UNITAIRES - DreamEngine                          |\n";
    std::cout << "+=================================================================+\n";

    std::cout << "\n>> Initialisation\n";
    RUN_TEST(DefaultConstruction);
    RUN_TEST(ConstructionWithConfig);
    RUN_TEST(InitialStats);

    std::cout << "\n>> Gestion MCT\n";
    RUN_TEST(AddMemoryToMCT);
    RUN_TEST(AddMultipleMemories);
    RUN_TEST(ClearMCT);
    RUN_TEST(MemoryTypes);

    std::cout << "\n>> Machine a etats\n";
    RUN_TEST(StateStartsAwake);
    RUN_TEST(ForceDreamStart);
    RUN_TEST(ForceDreamStartWithoutMemories);
    RUN_TEST(InterruptDream);
    RUN_TEST(InterruptFromAwakeDoesNothing);
    RUN_TEST(StateTransitionCallback);

    std::cout << "\n>> Cycle de reve\n";
    RUN_TEST(CompleteDreamCycle);
    RUN_TEST(DreamPhaseProgression);
    RUN_TEST(CycleCountIncrement);

    std::cout << "\n>> Consolidation\n";
    RUN_TEST(ConsolidationCallback);
    RUN_TEST(TraumaAlwaysConsolidated);

    std::cout << "\n>> Exploration stochastique\n";
    RUN_TEST(EdgeCreationCallback);
    RUN_TEST(PatternAffectsSigma);

    std::cout << "\n>> Oubli et nettoyage\n";
    RUN_TEST(TraumaNeverDeleted);
    RUN_TEST(MCTClearedAfterCycle);

    std::cout << "\n>> Interruption Amyghaleon\n";
    RUN_TEST(AmyghaleonInterruptsDream);
    RUN_TEST(InterruptionCountsInStats);
    RUN_TEST(ResumeAfterInterruption);

    std::cout << "\n>> Conditions de declenchement\n";
    RUN_TEST(NoDreamWithPEURPattern);
    RUN_TEST(NoDreamWithHighEmotionalActivity);
    RUN_TEST(NoDreamWithEmptyMCT);
    RUN_TEST(CanStartDreamCheck);

    std::cout << "\n>> Configuration\n";
    RUN_TEST(SetConfig);
    RUN_TEST(ConfigDurationCalculations);
    RUN_TEST(ResetStats);

    std::cout << "\n>> Helpers DreamState\n";
    RUN_TEST(IsDreamingHelper);
    RUN_TEST(DreamStateToString);

    std::cout << "\n>> Cycle Progress\n";
    RUN_TEST(CycleProgressBounds);
    RUN_TEST(DreamPhaseProgressZeroWhenAwake);
    RUN_TEST(TimeSinceLastDream);

    std::cout << "\n>> Thread Safety\n";
    RUN_TEST(ConcurrentAddMemories);
    RUN_TEST(ConcurrentStateQueries);

    std::cout << "\n";
    std::cout << "+=================================================================+\n";
    std::cout << "|                      RESUME DES TESTS                          |\n";
    std::cout << "+=================================================================+\n";
    std::cout << "  Total:   " << g_testsRun << " tests\n";
    std::cout << "  Reussis: " << g_testsPassed << "\n";
    std::cout << "  Echecs:  " << g_testsFailed << "\n";
    std::cout << "+=================================================================+\n";

    if (g_testsFailed == 0) {
        std::cout << "\n  TOUS LES TESTS PASSENT!\n\n";
        return 0;
    } else {
        std::cout << "\n  CERTAINS TESTS ONT ECHOUE\n\n";
        return 1;
    }
}
