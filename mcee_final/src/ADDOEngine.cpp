/**
 * @file ADDOEngine.cpp
 * @brief Implémentation du moteur ADDO
 * @version 1.0
 * @date 2025-12-23
 */

#include "ADDOEngine.hpp"
#include <numeric>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <iomanip>

namespace mcee {

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTEUR
// ═══════════════════════════════════════════════════════════════════════════

ADDOEngine::ADDOEngine(const ADDOConfig& config)
    : config_(config)
    , resilience_(config.resilience_base)
    , rng_(std::random_device{}())
    , noise_dist_(config.stochasticity_bias, config.stochasticity_amplitude)
{
    // Initialiser les poids depuis la configuration
    variables_.w = config_.initial_weights;

    // Initialiser les variables à des valeurs neutres
    variables_.P.fill(0.5);

    // Contraintes par défaut (pas de limitation)
    variables_.L.fill(1.0);

    std::cout << "[ADDO] Moteur initialisé avec " << NUM_GOAL_VARIABLES
              << " variables\n";
    std::cout << "[ADDO] Résilience initiale: " << resilience_ << "\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// MISE À JOUR PRINCIPALE
// ═══════════════════════════════════════════════════════════════════════════

GoalState ADDOEngine::update(const EmotionalState& emotional_state,
                              double sentiment,
                              double wisdom)
{
    return updateWithMemory(emotional_state, sentiment, wisdom, memory_influence_);
}

GoalState ADDOEngine::updateWithMemory(const EmotionalState& emotional_state,
                                        double sentiment,
                                        double wisdom,
                                        const MemoryGraphInfluence& memory_influence)
{
    std::lock_guard<std::mutex> lock(mutex_);

    double previous_goal = current_state_.G;

    // Mode urgence : court-circuiter le calcul normal
    if (emergency_mode_ && config_.emergency_override) {
        current_state_.emergency_override = true;
        current_state_.emergency_goal = emergency_goal_;
        current_state_.G = 1.0;  // Priorité maximale
        current_state_.timestamp = std::chrono::steady_clock::now();

        if (on_emergency_) {
            on_emergency_(emergency_goal_);
        }

        return current_state_;
    }

    current_state_.emergency_override = false;

    // ─────────────────────────────────────────────────────────────────────────
    // 1. Appliquer le mapping émotions (24D) → variables (16)
    // ─────────────────────────────────────────────────────────────────────────

    applyEmotionMapping(emotional_state);

    // ─────────────────────────────────────────────────────────────────────────
    // 2. Mettre à jour les variables depuis les entrées externes
    // ─────────────────────────────────────────────────────────────────────────

    // Sentiments ← Ft du ConscienceEngine (surcharge le mapping si activé)
    if (config_.use_sentiment_for_S) {
        // Convertir sentiment [-1, +1] vers [0, 1]
        variables_.P[static_cast<size_t>(GoalVariable::SENTIMENTS)] =
            (sentiment + 1.0) / 2.0;
    }

    // Souvenirs émotionnels ← valence de l'état émotionnel
    variables_.P[static_cast<size_t>(GoalVariable::SOUVENIRS_EMOTIONNELS)] =
        emotional_state.getValence();

    // ─────────────────────────────────────────────────────────────────────────
    // 3. Mettre à jour M_graph(t) depuis MCTGraph si connecté
    // ─────────────────────────────────────────────────────────────────────────

    if (mct_graph_) {
        updateFromMCTGraph();
    } else {
        // Utiliser l'influence mémoire fournie en paramètre
        memory_influence_ = memory_influence;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // 4. Adapter les poids si sagesse active
    // ─────────────────────────────────────────────────────────────────────────

    if (config_.use_wisdom_modulation) {
        adaptWeights(wisdom);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // 5. Calculer les composantes de G(t)
    // ─────────────────────────────────────────────────────────────────────────

    // Σ w_i(t)·P_i(t)·L_i(t)
    double weighted_sum = computeWeightedSum();

    // Interactions positives
    double pos_interactions = computePositiveInteractions();
    current_state_.interaction_positive_sum = pos_interactions;

    // Interactions négatives
    double neg_interactions = computeNegativeInteractions();
    current_state_.interaction_negative_sum = neg_interactions;

    // Terme de résilience
    double resilience_term = computeResilienceTerm();

    // Stochasticité
    double stochastic_term = generateStochasticity();
    current_state_.stochasticity = stochastic_term;

    // Influence mémoire graphe
    double memory_term = computeMemoryInfluence();

    // ─────────────────────────────────────────────────────────────────────────
    // 6. Calculer G(t) brut
    // ─────────────────────────────────────────────────────────────────────────

    double G_raw = weighted_sum
                 + pos_interactions
                 - neg_interactions
                 + resilience_term
                 + stochastic_term
                 + memory_term;

    current_state_.G_raw = G_raw;

    // ─────────────────────────────────────────────────────────────────────────
    // 7. Appliquer la fonction de sortie
    // ─────────────────────────────────────────────────────────────────────────

    double G = applyOutputFunction(G_raw);
    current_state_.G = G;

    // ─────────────────────────────────────────────────────────────────────────
    // 8. Mettre à jour l'état
    // ─────────────────────────────────────────────────────────────────────────

    current_state_.variables = variables_;
    current_state_.resilience = resilience_;
    current_state_.memory_influence = memory_influence_;
    current_state_.timestamp = std::chrono::steady_clock::now();

    // Trouver la variable dominante
    findDominantVariable();

    // Mettre à jour l'historique
    updateHistory(G);

    // ─────────────────────────────────────────────────────────────────────────
    // 9. Callbacks
    // ─────────────────────────────────────────────────────────────────────────

    if (on_update_) {
        on_update_(current_state_);
    }

    // Détecter changement significatif
    if (std::abs(G - previous_goal) > 0.1 && on_goal_change_) {
        std::string reason = "Variable dominante: " + current_state_.dominant_variable;
        on_goal_change_(previous_goal, G, reason);
    }

    return current_state_;
}

// ═══════════════════════════════════════════════════════════════════════════
// MODIFICATION DES VARIABLES
// ═══════════════════════════════════════════════════════════════════════════

void ADDOEngine::setVariable(GoalVariable var, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    variables_.setVariable(var, value);
}

void ADDOEngine::setConstraint(GoalVariable var, double constraint) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t idx = static_cast<size_t>(var);
    variables_.L[idx] = std::clamp(constraint, 0.0, 2.0);
}

void ADDOEngine::updateExternalContext(double environment, double circumstances) {
    std::lock_guard<std::mutex> lock(mutex_);
    variables_.setVariable(GoalVariable::ENVIRONNEMENT, environment);
    variables_.setVariable(GoalVariable::CIRCONSTANCES, circumstances);
}

void ADDOEngine::updateNeeds(double physiological, double safety, double belonging,
                              double esteem, double self_actualization) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Combiner selon la pyramide de Maslow (pondération hiérarchique)
    double needs_score = 0.0;
    double total_weight = 0.0;

    // Les besoins inférieurs ont plus de poids s'ils ne sont pas satisfaits
    double weights[] = {0.35, 0.25, 0.20, 0.12, 0.08};
    double values[] = {physiological, safety, belonging, esteem, self_actualization};

    for (int i = 0; i < 5; ++i) {
        // Si un besoin inférieur n'est pas satisfait, il prend plus d'importance
        double effective_weight = weights[i];
        if (i > 0 && values[i-1] < 0.5) {
            effective_weight *= (1.0 + (0.5 - values[i-1]));  // Amplifier
        }
        needs_score += effective_weight * values[i];
        total_weight += effective_weight;
    }

    variables_.setVariable(GoalVariable::BESOINS, needs_score / total_weight);
}

// ═══════════════════════════════════════════════════════════════════════════
// RÉSILIENCE ET TRAUMA
// ═══════════════════════════════════════════════════════════════════════════

void ADDOEngine::recordSuccess(double intensity) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Augmenter la résilience
    resilience_ += config_.resilience_growth_rate * intensity;
    resilience_ = std::min(resilience_, config_.resilience_max);

    // Augmenter la confiance en soi
    double current = variables_.getVariable(GoalVariable::CONNAISSANCE_SOI);
    variables_.setVariable(GoalVariable::CONNAISSANCE_SOI,
                           current + 0.05 * intensity);

    // Augmenter la motivation
    current = variables_.getVariable(GoalVariable::MOTIVATIONS);
    variables_.setVariable(GoalVariable::MOTIVATIONS,
                           current + 0.03 * intensity);
}

void ADDOEngine::recordFailure(double intensity) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Diminuer la résilience (mais pas trop)
    resilience_ -= config_.resilience_decay_on_trauma * intensity * 0.5;
    resilience_ = std::max(resilience_, 0.1);  // Plancher

    // Augmenter les regrets
    double current = variables_.getVariable(GoalVariable::REGRETS);
    variables_.setVariable(GoalVariable::REGRETS,
                           current + 0.1 * intensity);
}

void ADDOEngine::signalTrauma(const TraumaState& trauma) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Mettre à jour la variable Traumatismes
    variables_.setVariable(GoalVariable::TRAUMATISMES, trauma.intensity);

    // Diminuer la résilience significativement
    resilience_ -= config_.resilience_decay_on_trauma * trauma.intensity;
    resilience_ = std::max(resilience_, 0.1);

    // Mettre à jour l'influence mémoire
    memory_influence_.T_trauma = trauma.intensity;
}

void ADDOEngine::setMemoryInfluence(double S_positive, double S_negative, double T_trauma) {
    std::lock_guard<std::mutex> lock(mutex_);
    memory_influence_.S_positive = S_positive;
    memory_influence_.S_negative = S_negative;
    memory_influence_.T_trauma = T_trauma;
    memory_influence_.timestamp = std::chrono::steady_clock::now();
}

// ═══════════════════════════════════════════════════════════════════════════
// URGENCE (AMYGHALEON)
// ═══════════════════════════════════════════════════════════════════════════

void ADDOEngine::triggerEmergencyOverride(const std::string& emergency_goal) {
    std::lock_guard<std::mutex> lock(mutex_);
    emergency_mode_ = true;
    emergency_goal_ = emergency_goal;

    std::cout << "[ADDO] ⚡ Mode urgence activé: " << emergency_goal << "\n";
}

void ADDOEngine::clearEmergencyOverride() {
    std::lock_guard<std::mutex> lock(mutex_);
    emergency_mode_ = false;
    emergency_goal_.clear();

    std::cout << "[ADDO] Mode urgence désactivé\n";
}

bool ADDOEngine::isInEmergencyMode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return emergency_mode_;
}

// ═══════════════════════════════════════════════════════════════════════════
// ACCESSEURS
// ═══════════════════════════════════════════════════════════════════════════

double ADDOEngine::getCurrentGoal() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_state_.G;
}

const GoalState& ADDOEngine::getCurrentState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_state_;
}

double ADDOEngine::getResilience() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return resilience_;
}

const GoalVariables& ADDOEngine::getVariables() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return variables_;
}

const InteractionMatrix& ADDOEngine::getInteractionMatrix() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return interactions_;
}

// ═══════════════════════════════════════════════════════════════════════════
// CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════

void ADDOEngine::setUpdateCallback(GoalUpdateCallback callback) {
    on_update_ = std::move(callback);
}

void ADDOEngine::setGoalChangeCallback(GoalChangeCallback callback) {
    on_goal_change_ = std::move(callback);
}

void ADDOEngine::setEmergencyCallback(EmergencyGoalCallback callback) {
    on_emergency_ = std::move(callback);
}

// ═══════════════════════════════════════════════════════════════════════════
// HISTORIQUE
// ═══════════════════════════════════════════════════════════════════════════

const std::deque<double>& ADDOEngine::getGoalHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return goal_history_;
}

double ADDOEngine::getGoalTrend() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (goal_history_.size() < 10) return 0.0;

    // Comparer la moyenne récente à la moyenne ancienne
    double recent_sum = 0.0, old_sum = 0.0;
    size_t n = goal_history_.size();
    size_t half = n / 2;

    for (size_t i = 0; i < half; ++i) {
        old_sum += goal_history_[i];
    }
    for (size_t i = half; i < n; ++i) {
        recent_sum += goal_history_[i];
    }

    double old_avg = old_sum / half;
    double recent_avg = recent_sum / (n - half);

    return recent_avg - old_avg;  // Positif = croissant
}

double ADDOEngine::getGoalStability() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (goal_history_.size() < 5) return 1.0;

    // Calculer l'écart-type
    double sum = 0.0, sum_sq = 0.0;
    for (double g : goal_history_) {
        sum += g;
        sum_sq += g * g;
    }

    double n = static_cast<double>(goal_history_.size());
    double mean = sum / n;
    double variance = (sum_sq / n) - (mean * mean);
    double stddev = std::sqrt(std::max(0.0, variance));

    // Stabilité = 1 - stddev normalisé
    return std::max(0.0, 1.0 - stddev * 2.0);
}

// ═══════════════════════════════════════════════════════════════════════════
// MÉTHODES PRIVÉES
// ═══════════════════════════════════════════════════════════════════════════

double ADDOEngine::computeWeightedSum() const {
    double sum = 0.0;
    for (size_t i = 0; i < NUM_GOAL_VARIABLES; ++i) {
        sum += variables_.w[i] * variables_.P[i] * variables_.L[i];
    }
    return sum;
}

double ADDOEngine::computePositiveInteractions() const {
    double sum = 0.0;
    for (size_t i = 0; i < NUM_GOAL_VARIABLES; ++i) {
        for (size_t j = i + 1; j < NUM_GOAL_VARIABLES; ++j) {
            double c_ij = interactions_.positive[i][j];
            if (c_ij > 0.0) {
                sum += c_ij * variables_.P[i] * variables_.P[j];
            }
        }
    }
    return sum * config_.interaction_positive_scale;
}

double ADDOEngine::computeNegativeInteractions() const {
    double sum = 0.0;
    for (size_t i = 0; i < NUM_GOAL_VARIABLES; ++i) {
        for (size_t k = i + 1; k < NUM_GOAL_VARIABLES; ++k) {
            double c_ik = interactions_.negative[i][k];
            if (c_ik > 0.0) {
                sum += c_ik * variables_.P[i] * variables_.P[k];
            }
        }
    }
    return sum * config_.interaction_negative_scale;
}

double ADDOEngine::computeResilienceTerm() const {
    // Rs(t) × Σ P_ℓ (pour les variables négatives : regrets, traumas)
    double negative_vars_sum = 0.0;
    negative_vars_sum += variables_.P[static_cast<size_t>(GoalVariable::REGRETS)];
    negative_vars_sum += variables_.P[static_cast<size_t>(GoalVariable::TRAUMATISMES)];

    // La résilience atténue l'impact négatif
    return resilience_ * (1.0 - negative_vars_sum) * 0.1;
}

double ADDOEngine::generateStochasticity() {
    return noise_dist_(rng_);
}

double ADDOEngine::computeMemoryInfluence() const {
    return memory_influence_.compute(
        config_.alpha_memory_positive,
        config_.gamma_trauma
    );
}

double ADDOEngine::applyOutputFunction(double raw_value) const {
    if (config_.use_sigmoid_output) {
        // Sigmoïde : 1 / (1 + e^(-k*x))
        // Centrée sur 0.5 pour un raw_value autour de 0.5
        double centered = (raw_value - 0.5) * config_.sigmoid_steepness;
        return 1.0 / (1.0 + std::exp(-centered));
    }

    // Linéaire avec clamp
    return std::clamp(raw_value, 0.0, 1.0);
}

void ADDOEngine::adaptWeights(double wisdom) {
    if (wisdom <= 0.0) return;

    // La sagesse modère les extrêmes
    for (size_t i = 0; i < NUM_GOAL_VARIABLES; ++i) {
        double base_weight = config_.initial_weights[i];
        double current_weight = variables_.w[i];

        // Tirer les poids vers les valeurs de base avec la sagesse
        double target = base_weight * wisdom;
        double adaptation = config_.weight_adaptation_rate * (target - current_weight);

        variables_.w[i] = std::clamp(current_weight + adaptation, 0.01, 0.5);
    }

    // Renormaliser pour que Σ w_i = 1
    double sum = std::accumulate(variables_.w.begin(), variables_.w.end(), 0.0);
    if (sum > 0.0) {
        for (auto& w : variables_.w) {
            w /= sum;
        }
    }
}

void ADDOEngine::findDominantVariable() {
    size_t max_idx = 0;
    double max_val = 0.0;

    for (size_t i = 0; i < NUM_GOAL_VARIABLES; ++i) {
        double weighted_val = variables_.w[i] * variables_.P[i] * variables_.L[i];
        if (weighted_val > max_val) {
            max_val = weighted_val;
            max_idx = i;
        }
    }

    current_state_.dominant_variable = GOAL_VARIABLE_NAMES[max_idx];
    current_state_.dominant_value = max_val;
}

void ADDOEngine::updateHistory(double goal) {
    goal_history_.push_back(goal);
    while (goal_history_.size() > MAX_HISTORY_SIZE) {
        goal_history_.pop_front();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// INTÉGRATION MCTGraph
// ═══════════════════════════════════════════════════════════════════════════

void ADDOEngine::setMCTGraph(std::shared_ptr<MCTGraph> mct_graph) {
    std::lock_guard<std::mutex> lock(mutex_);
    mct_graph_ = std::move(mct_graph);
    std::cout << "[ADDO] MCTGraph connecté pour enrichissement M_graph(t)\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// MAPPING ÉMOTIONS → VARIABLES
// ═══════════════════════════════════════════════════════════════════════════

void ADDOEngine::applyEmotionMapping(const EmotionalState& emotional_state) {
    // Accéder directement au vecteur des 24 émotions
    const auto& emo_values = emotional_state.emotions;

    // Pour chaque émotion active, appliquer son influence sur les variables
    for (size_t emo_idx = 0; emo_idx < EmotionVariableMapping::NUM_EMOTIONS; ++emo_idx) {
        double emotion_intensity = emo_values[emo_idx];

        // Ignorer les émotions inactives (seuil minimal)
        if (emotion_intensity < 0.05) continue;

        // Appliquer l'influence sur chaque variable
        for (size_t var_idx = 0; var_idx < NUM_GOAL_VARIABLES; ++var_idx) {
            double weight = emotion_mapping_.weights[emo_idx][var_idx];

            if (std::abs(weight) < 0.01) continue;  // Ignorer les poids négligeables

            // Calculer l'influence: intensité_émotion × poids_mapping
            double influence = emotion_intensity * weight;

            // Modifier la variable avec atténuation (éviter les changements brusques)
            double current_val = variables_.P[var_idx];
            double delta = influence * 0.3;  // Facteur d'atténuation

            // Appliquer le changement (positif ou négatif selon le poids)
            double new_val = std::clamp(current_val + delta, 0.0, 1.0);
            variables_.P[var_idx] = new_val;
        }
    }
}

void ADDOEngine::updateFromMCTGraph() {
    if (!mct_graph_) return;

    // Analyser les associations causales du graphe
    auto causal_analysis = mct_graph_->analyzeCausality();

    double S_positive = 0.0;
    double S_negative = 0.0;
    double T_trauma = 0.0;
    double total_weight = 0.0;

    // Parcourir les associations mot → émotion
    for (const auto& analysis : causal_analysis) {
        for (const auto& emotion_id : analysis.triggered_emotion_ids) {
            auto emotion_node = mct_graph_->getEmotionNode(emotion_id);
            if (!emotion_node) continue;

            double weight = analysis.causal_strength;
            total_weight += weight;

            // Classifier selon la valence de l'émotion
            if (emotion_node->valence > 0.2) {
                // Émotion positive
                S_positive += weight * emotion_node->intensity;
            } else if (emotion_node->valence < -0.2) {
                // Émotion négative
                S_negative += weight * emotion_node->intensity;

                // Détecter les émotions traumatiques (peur, anxiété intenses)
                // Indices: Peur=2, Anxiété=6, Honte=10
                const auto& emotions = emotion_node->emotions;
                double fear = emotions[2];
                double anxiety = emotions[6];
                double shame = emotions[10];

                if (fear > 0.7 || anxiety > 0.7 || shame > 0.6) {
                    T_trauma += weight * std::max({fear, anxiety, shame});
                }
            }
        }
    }

    // Normaliser si possible
    if (total_weight > 0.0) {
        S_positive /= total_weight;
        S_negative /= total_weight;
        T_trauma /= total_weight;
    }

    // Mettre à jour l'influence mémoire
    memory_influence_.S_positive = S_positive;
    memory_influence_.S_negative = S_negative;
    memory_influence_.T_trauma = std::clamp(T_trauma, 0.0, 1.0);
    memory_influence_.timestamp = std::chrono::steady_clock::now();
}

} // namespace mcee
