#include "ConscienceEngine.hpp"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <thread>
#include <chrono>

using namespace MCEE;

// ═══════════════════════════════════════════════════════════════════════════════
// TESTS UNITAIRES
// ═══════════════════════════════════════════════════════════════════════════════

void test_basic_conscience() {
    std::cout << "=== Test: Conscience de base ===" << std::endl;
    
    ConscienceConfig config;
    ConscienceEngine engine(config);
    
    // Émotions neutres
    std::array<double, 24> emotions{};
    emotions.fill(0.5);
    engine.updateEmotions(emotions, "SERENITE");
    
    auto state = engine.computeConscience();
    
    std::cout << "  Ct = " << state.Ct << std::endl;
    std::cout << "  emotion_component = " << state.emotion_component << std::endl;
    std::cout << "  Pattern: " << state.active_pattern << std::endl;
    
    assert(state.Ct != 0.0);
    assert(state.active_pattern == "SERENITE");
    
    std::cout << "  ✓ Test réussi" << std::endl << std::endl;
}

void test_trauma_dominance() {
    std::cout << "=== Test: Dominance trauma ===" << std::endl;
    
    ConscienceConfig config;
    config.trauma_dominance_threshold = 0.7;
    ConscienceEngine engine(config);
    
    // Émotions calmes
    std::array<double, 24> emotions{};
    emotions.fill(0.2);
    engine.updateEmotions(emotions, "SERENITE");
    
    // Sans trauma
    auto state_no_trauma = engine.computeConscience();
    std::cout << "  Sans trauma: Ct = " << state_no_trauma.Ct << std::endl;
    
    // Avec trauma dominant
    TraumaState trauma;
    trauma.type = TraumaType::PHYSICAL;
    trauma.intensity = 0.9;
    trauma.trigger_context = "collision";
    engine.updateTrauma(trauma);
    
    auto state_with_trauma = engine.computeConscience();
    std::cout << "  Avec trauma (0.9): Ct = " << state_with_trauma.Ct << std::endl;
    std::cout << "  trauma_component = " << state_with_trauma.trauma_component << std::endl;
    
    // Le trauma doit avoir un impact significatif
    assert(std::abs(state_with_trauma.Ct) > std::abs(state_no_trauma.Ct));
    assert(state_with_trauma.hasTrauma());
    
    std::cout << "  ✓ Test réussi" << std::endl << std::endl;
}

void test_memory_activation() {
    std::cout << "=== Test: Activation mémoire ===" << std::endl;
    
    ConscienceEngine engine;
    
    // Configurer les émotions
    std::array<double, 24> emotions{};
    emotions.fill(0.3);
    engine.updateEmotions(emotions, "EXPLORATION");
    
    // Activation mémoire forte
    MemoryActivation activation;
    activation.MCT_score = 0.8;   // MCT actif
    activation.MLT_score = 0.6;   // MLT modéré
    activation.ME_score = 0.9;    // Épisodique fort
    engine.updateMemoryActivation(activation);
    
    auto state = engine.computeConscience();
    
    std::cout << "  memory_component = " << state.memory_component << std::endl;
    std::cout << "  Ct = " << state.Ct << std::endl;
    
    // La composante mémoire doit être significative
    assert(state.memory_component > 0.3);
    
    std::cout << "  ✓ Test réussi" << std::endl << std::endl;
}

void test_sentiment_accumulation() {
    std::cout << "=== Test: Accumulation sentiments ===" << std::endl;
    
    ConscienceConfig config;
    config.sentiment_history_size = 10;
    config.gamma_decay = 0.9;
    ConscienceEngine engine(config);
    
    // Simuler plusieurs cycles de conscience positive
    std::array<double, 24> emotions{};
    for (int i = 0; i < 10; ++i) {
        emotions.fill(0.6 + i * 0.03);  // Émotions croissantes
        engine.updateEmotions(emotions, "JOIE");
        
        auto full_state = engine.compute();
        std::cout << "  Cycle " << i << ": Ct=" << std::fixed << std::setprecision(3) 
                  << full_state.conscience.Ct 
                  << ", Ft=" << full_state.sentiment.Ft 
                  << ", BG=" << full_state.affective_background << std::endl;
    }
    
    auto final_sentiment = engine.getLastSentiment();
    auto bg = engine.getAffectiveBackground();
    
    std::cout << "  Final Ft = " << final_sentiment.Ft << std::endl;
    std::cout << "  Affective Background = " << bg << std::endl;
    std::cout << "  History depth = " << final_sentiment.history_depth << std::endl;
    
    // Le sentiment doit être positif après des consciences positives
    assert(final_sentiment.Ft > 0);
    assert(final_sentiment.history_depth == 10);
    
    std::cout << "  ✓ Test réussi" << std::endl << std::endl;
}

void test_wisdom_growth() {
    std::cout << "=== Test: Croissance sagesse ===" << std::endl;
    
    ConscienceConfig config;
    config.wisdom_initial = 1.0;
    config.wisdom_growth_rate = 0.1;
    config.wisdom_max = 2.0;
    ConscienceEngine engine(config);
    
    double initial_wisdom = engine.getWisdom();
    std::cout << "  Sagesse initiale: " << initial_wisdom << std::endl;
    
    // Ajouter des expériences
    for (int i = 0; i < 10; ++i) {
        engine.addExperience(1.0);
    }
    
    double final_wisdom = engine.getWisdom();
    std::cout << "  Sagesse après 10 expériences: " << final_wisdom << std::endl;
    
    assert(final_wisdom > initial_wisdom);
    assert(final_wisdom <= config.wisdom_max);
    
    // Reset
    engine.resetWisdom();
    assert(engine.getWisdom() == config.wisdom_initial);
    
    std::cout << "  ✓ Test réussi" << std::endl << std::endl;
}

void test_feedback_environment() {
    std::cout << "=== Test: Feedback et environnement ===" << std::endl;
    
    ConscienceEngine engine;
    
    std::array<double, 24> emotions{};
    emotions.fill(0.5);
    engine.updateEmotions(emotions, "SERENITE");
    
    // État de base
    auto base_state = engine.computeConscience();
    std::cout << "  Base Ct = " << base_state.Ct << std::endl;
    
    // Feedback positif
    FeedbackState feedback;
    feedback.valence = 0.8;
    feedback.intensity = 0.7;
    feedback.credibility = 1.0;
    engine.updateFeedback(feedback);
    
    auto with_feedback = engine.computeConscience();
    std::cout << "  Avec feedback positif: Ct = " << with_feedback.Ct << std::endl;
    std::cout << "  feedback_component = " << with_feedback.feedback_component << std::endl;
    
    // Environnement hostile
    EnvironmentState env;
    env.hostility = 0.8;
    env.noise_level = 0.9;
    env.familiarity = 0.1;
    engine.updateEnvironment(env);
    
    auto with_env = engine.computeConscience();
    std::cout << "  Avec env hostile: Ct = " << with_env.Ct << std::endl;
    std::cout << "  environment_component = " << with_env.environment_component << std::endl;
    
    std::cout << "  ✓ Test réussi" << std::endl << std::endl;
}

void test_coefficient_modulation() {
    std::cout << "=== Test: Modulation coefficients par MLT ===" << std::endl;
    
    ConscienceEngine engine;
    
    std::array<double, 24> emotions{};
    emotions.fill(0.5);
    engine.updateEmotions(emotions, "SERENITE");
    
    auto base_state = engine.computeConscience();
    std::cout << "  Base emotion_component = " << base_state.emotion_component << std::endl;
    
    // Moduler les coefficients alpha (doubler certaines émotions)
    std::array<double, 24> modulation{};
    modulation.fill(1.0);
    modulation[0] = 2.0;  // Doubler la première émotion
    modulation[1] = 2.0;
    modulation[2] = 2.0;
    engine.modulateEmotionCoefficients(modulation);
    
    auto modulated_state = engine.computeConscience();
    std::cout << "  Modulé emotion_component = " << modulated_state.emotion_component << std::endl;
    
    assert(modulated_state.emotion_component > base_state.emotion_component);
    
    std::cout << "  ✓ Test réussi" << std::endl << std::endl;
}

void test_json_serialization() {
    std::cout << "=== Test: Sérialisation JSON ===" << std::endl;
    
    ConscienceEngine engine;
    
    std::array<double, 24> emotions{};
    emotions.fill(0.6);
    engine.updateEmotions(emotions, "JOIE");
    
    // Générer quelques états
    for (int i = 0; i < 5; ++i) {
        engine.compute();
    }
    
    std::string json = engine.toJson();
    std::cout << json << std::endl;
    
    assert(!json.empty());
    assert(json.find("conscience") != std::string::npos);
    assert(json.find("sentiment") != std::string::npos);
    
    std::cout << "  ✓ Test réussi" << std::endl << std::endl;
}

void test_callbacks() {
    std::cout << "=== Test: Callbacks ===" << std::endl;
    
    ConscienceConfig config;
    config.update_interval_ms = 10;  // Mise à jour rapide pour le test
    ConscienceEngine engine(config);
    
    int conscience_count = 0;
    int sentiment_count = 0;
    
    engine.setConscienceUpdateCallback([&](const ConscienceState&) {
        conscience_count++;
    });
    
    engine.setSentimentUpdateCallback([&](const SentimentState&) {
        sentiment_count++;
    });
    
    std::array<double, 24> emotions{};
    emotions.fill(0.5);
    engine.updateEmotions(emotions, "SERENITE");
    
    // Plusieurs ticks
    for (int i = 0; i < 5; ++i) {
        engine.tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }
    
    std::cout << "  Conscience callbacks: " << conscience_count << std::endl;
    std::cout << "  Sentiment callbacks: " << sentiment_count << std::endl;
    
    assert(conscience_count > 0);
    assert(sentiment_count > 0);
    
    std::cout << "  ✓ Test réussi" << std::endl << std::endl;
}

// ═══════════════════════════════════════════════════════════════════════════════
// MAIN
// ═══════════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "\n╔═══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║     TESTS ConscienceEngine - Module Conscience & Sentiments      ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n" << std::endl;
    
    try {
        test_basic_conscience();
        test_trauma_dominance();
        test_memory_activation();
        test_sentiment_accumulation();
        test_wisdom_growth();
        test_feedback_environment();
        test_coefficient_modulation();
        test_json_serialization();
        test_callbacks();
        
        std::cout << "╔═══════════════════════════════════════════════════════════╗" << std::endl;
        std::cout << "║              TOUS LES TESTS RÉUSSIS ! ✓                   ║" << std::endl;
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n" << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERREUR: " << e.what() << std::endl;
        return 1;
    }
}
