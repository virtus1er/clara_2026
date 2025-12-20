/**
 * @file EmotionUpdater.cpp
 * @brief Implémentation de la mise à jour des émotions MCEE
 * @version 2.0
 * @date 2025-12-19
 */

#include "EmotionUpdater.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>

namespace mcee {

EmotionUpdater::EmotionUpdater() {
    // Coefficients par défaut (phase SÉRÉNITÉ)
    const auto& default_config = DEFAULT_PHASE_CONFIGS.at(Phase::SERENITE);
    setCoefficientsFromPhase(default_config);
}

void EmotionUpdater::setCoefficientsFromPhase(const PhaseConfig& config) {
    alpha_ = config.alpha;
    beta_ = config.beta;
    gamma_ = config.gamma;
    delta_ = config.delta;
    theta_ = config.theta;

    std::cout << "[EmotionUpdater] Coefficients mis à jour:\n"
              << "  α=" << std::fixed << std::setprecision(2) << alpha_
              << ", β=" << beta_
              << ", γ=" << gamma_
              << ", δ=" << delta_
              << ", θ=" << theta_ << "\n";
}

double EmotionUpdater::updateEmotion(
    double E_current,
    double fb_ext,
    double fb_int,
    double delta_t,
    double influence_memories,
    double wisdom) const 
{
    // Formule MCEE:
    // E_i(t+1) = E_i(t) + α·Fb_ext + β·Fb_int - γ·Δt + δ·IS + θ·Wt
    
    double E_next = E_current
                  + alpha_ * fb_ext
                  + beta_ * fb_int
                  - gamma_ * delta_t
                  + delta_ * influence_memories
                  + theta_ * wisdom;

    // Clamp entre 0 et 1
    return std::clamp(E_next, 0.0, 1.0);
}

void EmotionUpdater::updateAllEmotions(
    EmotionalState& state,
    const Feedback& feedback,
    double delta_t,
    const std::array<double, NUM_EMOTIONS>& memory_influences,
    double wisdom) 
{
    for (size_t i = 0; i < NUM_EMOTIONS; ++i) {
        state.emotions[i] = updateEmotion(
            state.emotions[i],
            feedback.external,
            feedback.internal,
            delta_t,
            memory_influences[i],
            wisdom
        );
    }

    // Mettre à jour le timestamp
    state.timestamp = std::chrono::steady_clock::now();
}

double EmotionUpdater::computeVariance(
    double E_current,
    const std::vector<double>& memory_values) const 
{
    if (memory_values.empty()) {
        return 0.0;
    }

    // Formule: Var_i(t) = (1/m) × Σ[E_i(t) - S_i,j]²
    double sum_sq_diff = 0.0;
    for (const auto& mem_val : memory_values) {
        double diff = E_current - mem_val;
        sum_sq_diff += diff * diff;
    }

    return sum_sq_diff / static_cast<double>(memory_values.size());
}

double EmotionUpdater::computeGlobalVariance(
    const EmotionalState& state,
    const std::vector<Memory>& memories) const 
{
    if (memories.empty()) {
        return 0.0;
    }

    double total_variance = 0.0;

    for (size_t i = 0; i < NUM_EMOTIONS; ++i) {
        // Collecter les valeurs de cette émotion dans tous les souvenirs
        std::vector<double> memory_values;
        memory_values.reserve(memories.size());
        
        for (const auto& mem : memories) {
            memory_values.push_back(mem.emotions[i]);
        }

        total_variance += computeVariance(state.emotions[i], memory_values);
    }

    // Variance moyenne sur toutes les émotions
    return total_variance / static_cast<double>(NUM_EMOTIONS);
}

double EmotionUpdater::computeEGlobal(
    const EmotionalState& state,
    double E_global_prev,
    double variance_global) const 
{
    // Formule: E_global(t+1) = tanh(E_global(t) + Σ[E_i(t+1) × (1 - Var_global)])
    
    double sum_weighted = 0.0;
    double weight = 1.0 - std::clamp(variance_global, 0.0, 1.0);

    for (size_t i = 0; i < NUM_EMOTIONS; ++i) {
        sum_weighted += state.emotions[i] * weight;
    }

    // Normaliser par le nombre d'émotions pour éviter explosion
    sum_weighted /= static_cast<double>(NUM_EMOTIONS);

    return std::tanh(E_global_prev + sum_weighted);
}

} // namespace mcee
