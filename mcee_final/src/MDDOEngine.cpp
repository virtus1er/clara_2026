/**
 * @file MDDOEngine.cpp
 * @brief Implémentation du Module de Décision Délibérée et Orientée
 * @version 1.0
 * @date 2025-12-26
 */

#include "MDDOEngine.hpp"
#include <iostream>
#include <algorithm>
#include <random>
#include <cmath>

namespace mcee {

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTEUR
// ═══════════════════════════════════════════════════════════════════════════

MDDOEngine::MDDOEngine(const MDDOConfig& config)
    : config_(config) {

    // Initialiser les templates d'actions de base
    action_templates_ = {
        {"APPROACH", "Approcher", "APPROACH", 0.0, 0.0, 0.5, 0.0, {}, {}, 0.0},
        {"AVOID", "Éviter", "AVOID", 0.0, 0.0, 0.5, 0.0, {}, {}, 0.0},
        {"FREEZE", "Se figer", "FREEZE", 0.0, 0.0, 0.5, 0.0, {}, {}, 0.0},
        {"EXPLORE", "Explorer", "EXPLORE", 0.0, 0.0, 0.5, 0.0, {}, {}, 0.0},
        {"COMMUNICATE", "Communiquer", "COMMUNICATE", 0.0, 0.0, 0.5, 0.0, {}, {}, 0.0},
        {"WAIT", "Attendre", "WAIT", 0.0, 0.0, 0.8, 0.0, {}, {}, 0.0},
        {"FLEE", "Fuir", "FLEE", 0.0, 0.3, 0.7, -0.2, {}, {}, 0.0},
        {"ENGAGE", "S'engager", "ENGAGE", 0.0, 0.2, 0.6, 0.3, {}, {}, 0.0}
    };

    std::cout << "[MDDO] Moteur initialisé\n";
    std::cout << "[MDDO] τ_max=" << config_.tau_max_ms << "ms, θ_conf="
              << config_.theta_confidence << ", θ_veto=" << config_.theta_veto << "\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// INTERFACE PRINCIPALE
// ═══════════════════════════════════════════════════════════════════════════

ActionIntention MDDOEngine::deliberate(const SituationFrame& situation) {
    auto start_time = std::chrono::steady_clock::now();

    // Phase 1: Perception
    state_.current_phase = MDDOState::Phase::PERCEIVING;
    SituationFrame enriched_situation = perceive(situation);
    state_.current_situation = enriched_situation;

    // Phase 2: Activation mémorielle
    state_.current_phase = MDDOState::Phase::ACTIVATING;
    std::vector<Memory> relevant_memories;
    if (memory_manager_) {
        relevant_memories = activateMemories(enriched_situation);
    }

    // Phase 3: Simulation
    state_.current_phase = MDDOState::Phase::SIMULATING;
    std::vector<ActionOption> options = simulate(enriched_situation, relevant_memories);
    state_.generated_options = options;

    // Phase 4: Arbitrage
    state_.current_phase = MDDOState::Phase::ARBITRATING;
    ActionIntention intention = arbitrate(enriched_situation, options);

    // Calculer le temps de délibération
    auto end_time = std::chrono::steady_clock::now();
    intention.deliberation_time_ms = std::chrono::duration<double, std::milli>(
        end_time - start_time).count();
    intention.timestamp = std::chrono::system_clock::now();

    // Mettre à jour les statistiques
    state_.decisions_made++;
    state_.avg_deliberation_time_ms =
        (state_.avg_deliberation_time_ms * (state_.decisions_made - 1) +
         intention.deliberation_time_ms) / state_.decisions_made;
    state_.avg_confidence =
        (state_.avg_confidence * (state_.decisions_made - 1) +
         intention.confidence) / state_.decisions_made;

    // Ajouter à l'historique
    decision_history_.push_back(intention);
    if (decision_history_.size() > MAX_HISTORY_SIZE) {
        decision_history_.erase(decision_history_.begin());
    }

    state_.current_phase = MDDOState::Phase::IDLE;
    state_.current_situation.reset();

    std::cout << "[MDDO] Décision: " << intention.selected_action.name
              << " (conf=" << std::fixed << std::setprecision(2) << intention.confidence
              << ", τ=" << intention.deliberation_time_ms << "ms)\n";

    return intention;
}

void MDDOEngine::deliberateAsync(const SituationFrame& situation, IntentionCallback callback) {
    intention_callback_ = std::move(callback);

    // Dans une implémentation réelle, ceci serait exécuté dans un thread séparé
    ActionIntention intention = deliberate(situation);

    if (intention_callback_) {
        intention_callback_(intention);
    }
}

void MDDOEngine::interrupt(const ActionOption& emergency_action) {
    std::cout << "[MDDO] ⚡ INTERRUPTION - Action d'urgence: " << emergency_action.name << "\n";

    // Créer une intention d'urgence
    ActionIntention emergency_intention;
    emergency_intention.selected_action = emergency_action;
    emergency_intention.confidence = 1.0;
    emergency_intention.was_interrupted = true;
    emergency_intention.rationale = "Interruption d'urgence par Amyghaleon";
    emergency_intention.timestamp = std::chrono::system_clock::now();

    state_.vetoes_received++;
    state_.current_phase = MDDOState::Phase::IDLE;

    if (intention_callback_) {
        intention_callback_(emergency_intention);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// PHASES DE DÉLIBÉRATION
// ═══════════════════════════════════════════════════════════════════════════

SituationFrame MDDOEngine::perceive(const SituationFrame& raw_situation) {
    SituationFrame enriched = raw_situation;

    // Calculer le niveau de menace à partir de l'état émotionnel
    const auto& emotions = raw_situation.emotional_state.emotions;

    // Peur (index 14) + Horreur (15) + Anxiété (4)
    enriched.threat_level = std::clamp(
        emotions[14] * 0.5 + emotions[15] * 0.3 + emotions[4] * 0.2,
        0.0, 1.0);

    // Opportunité : Joie (17) + Intérêt (16) + Excitation (13)
    enriched.opportunity_level = std::clamp(
        emotions[17] * 0.4 + emotions[16] * 0.35 + emotions[13] * 0.25,
        0.0, 1.0);

    // Détecter les biais cognitifs actifs
    enriched.active_biases.clear();

    // Biais de négativité (focus sur les menaces)
    if (enriched.threat_level > 0.6) {
        enriched.active_biases.push_back("NEGATIVITY_BIAS");
    }

    // Biais d'optimisme
    if (enriched.opportunity_level > 0.7 && enriched.threat_level < 0.2) {
        enriched.active_biases.push_back("OPTIMISM_BIAS");
    }

    // Biais de confirmation (si pattern dominant très stable)
    if (raw_situation.emotional_state.variance_global < 0.1) {
        enriched.active_biases.push_back("CONFIRMATION_BIAS");
    }

    return enriched;
}

std::vector<Memory> MDDOEngine::activateMemories(const SituationFrame& situation) {
    std::vector<Memory> activated;

    if (!memory_manager_) {
        return activated;
    }

    // Rechercher les souvenirs similaires à l'état émotionnel actuel
    // (Dans une implémentation complète, utiliserait Neo4j/findSimilarMemories)

    // Prioriser les souvenirs récents et intenses
    // Filtrer par contexte si disponible

    return activated;
}

std::vector<ActionOption> MDDOEngine::simulate(
    const SituationFrame& situation,
    const std::vector<Memory>& memories) {

    std::vector<ActionOption> options = generateOptions(situation);

    // Générateur aléatoire pour Monte-Carlo
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> noise(0.0, 0.1);

    for (auto& option : options) {
        // Simulation Monte-Carlo simplifiée
        double total_reward = 0.0;
        double total_cost = 0.0;
        int success_count = 0;

        for (size_t sim = 0; sim < config_.max_simulations; ++sim) {
            // Simuler le résultat avec du bruit
            double sim_reward = option.expected_reward + noise(gen);
            double sim_cost = option.expected_cost + std::abs(noise(gen));

            // Probabilité de succès affectée par l'urgence et la menace
            double adjusted_prob = option.success_probability;
            adjusted_prob -= situation.threat_level * 0.2;
            adjusted_prob += situation.opportunity_level * 0.1;
            adjusted_prob = std::clamp(adjusted_prob, 0.1, 0.95);

            std::uniform_real_distribution<> uniform(0.0, 1.0);
            if (uniform(gen) < adjusted_prob) {
                success_count++;
                total_reward += sim_reward;
            } else {
                total_cost += sim_cost;
            }
        }

        // Moyenner les résultats
        option.expected_reward = total_reward / config_.max_simulations;
        option.expected_cost = total_cost / config_.max_simulations;
        option.success_probability = static_cast<double>(success_count) / config_.max_simulations;

        // Calculer l'impact émotionnel prédit
        if (option.category == "APPROACH" || option.category == "ENGAGE") {
            option.emotional_impact = situation.opportunity_level * 0.5;
        } else if (option.category == "AVOID" || option.category == "FLEE") {
            option.emotional_impact = -situation.threat_level * 0.3 + 0.2; // Soulagement
        } else if (option.category == "EXPLORE") {
            option.emotional_impact = 0.3 - situation.uncertainty * 0.2;
        }

        // Calculer le score d'utilité
        option.utility_score = computeUtility(option);
    }

    // Trier par utilité décroissante
    std::sort(options.begin(), options.end(),
        [](const ActionOption& a, const ActionOption& b) {
            return a.utility_score > b.utility_score;
        });

    return options;
}

ActionIntention MDDOEngine::arbitrate(
    const SituationFrame& situation,
    std::vector<ActionOption>& options) {

    ActionIntention intention;
    intention.alternatives = options;

    if (options.empty()) {
        // Aucune option : attendre par défaut
        ActionOption wait;
        wait.id = "DEFAULT_WAIT";
        wait.name = "Attendre (défaut)";
        wait.category = "WAIT";
        intention.selected_action = wait;
        intention.confidence = 0.3;
        intention.rationale = "Aucune option disponible";
        return intention;
    }

    // Détecter les conflits entre les meilleures options
    double conflict = detectConflict(options);

    // Calculer le temps de délibération requis
    double deliberation_budget = computeDeliberationTime(situation.urgency);

    // Si conflit élevé, augmenter le temps de délibération
    if (conflict > config_.theta_conflict) {
        deliberation_budget *= (1.0 + conflict);
    }

    // Sélectionner la meilleure option
    ActionOption& best = options[0];

    // Vérifier le veto de l'Amyghaleon
    if (checkVeto(best)) {
        // Chercher une alternative sûre
        for (auto& opt : options) {
            if (opt.category == "AVOID" || opt.category == "FLEE" || opt.category == "FREEZE") {
                best = opt;
                intention.was_interrupted = true;
                intention.rationale = "Veto Amyghaleon - action de protection sélectionnée";
                break;
            }
        }
    }

    intention.selected_action = best;

    // Calculer la confiance
    double confidence = best.utility_score;

    // Réduire si conflit élevé
    if (conflict > config_.theta_conflict) {
        confidence *= (1.0 - conflict * 0.5);
    }

    // Réduire si incertitude élevée
    confidence *= (1.0 - situation.uncertainty * 0.3);

    intention.confidence = std::clamp(confidence, 0.0, 1.0);

    // Générer la justification
    std::ostringstream rationale;
    rationale << "Action: " << best.name
              << " (utilité=" << std::fixed << std::setprecision(2) << best.utility_score
              << ", P(succès)=" << best.success_probability
              << ", conflit=" << conflict << ")";
    intention.rationale = rationale.str();

    return intention;
}

// ═══════════════════════════════════════════════════════════════════════════
// UTILITAIRES
// ═══════════════════════════════════════════════════════════════════════════

std::vector<ActionOption> MDDOEngine::generateOptions(const SituationFrame& situation) {
    std::vector<ActionOption> options;

    for (const auto& template_opt : action_templates_) {
        ActionOption option = template_opt;

        // Personnaliser selon la situation
        if (option.category == "APPROACH") {
            option.expected_reward = situation.opportunity_level * 0.8;
            option.expected_cost = situation.threat_level * 0.3;
            option.success_probability = 0.6 + situation.opportunity_level * 0.2;
        }
        else if (option.category == "AVOID") {
            option.expected_reward = 0.3 + situation.threat_level * 0.4; // Éviter le danger = récompense
            option.expected_cost = situation.opportunity_level * 0.5;   // Coût d'opportunité
            option.success_probability = 0.8;
        }
        else if (option.category == "FLEE") {
            option.expected_reward = situation.threat_level * 0.7;
            option.expected_cost = 0.2 + situation.opportunity_level * 0.3;
            option.success_probability = 0.85;
        }
        else if (option.category == "EXPLORE") {
            option.expected_reward = 0.4 + situation.uncertainty * 0.4;
            option.expected_cost = situation.threat_level * 0.4;
            option.success_probability = 0.6;
        }
        else if (option.category == "COMMUNICATE") {
            option.expected_reward = 0.5;
            option.expected_cost = 0.1;
            option.success_probability = 0.7;
        }
        else if (option.category == "WAIT") {
            option.expected_reward = 0.1;
            option.expected_cost = situation.urgency * 0.5;
            option.success_probability = 0.95;
        }

        options.push_back(option);
    }

    return options;
}

double MDDOEngine::computeUtility(const ActionOption& option) const {
    double utility = 0.0;

    // Formule d'utilité pondérée
    utility += config_.w_reward * option.expected_reward;
    utility -= config_.w_cost * option.expected_cost;
    utility += config_.w_probability * (option.success_probability - 0.5);
    utility += config_.w_emotional * option.emotional_impact;

    return std::clamp(utility, 0.0, 1.0);
}

bool MDDOEngine::checkVeto(const ActionOption& option) {
    // Si callback de veto défini, l'utiliser
    if (veto_callback_) {
        return veto_callback_(option);
    }

    // Sinon, veto automatique si action risquée avec menace élevée
    if (state_.current_situation) {
        double threat = state_.current_situation->threat_level;
        if (threat > config_.theta_veto &&
            (option.category == "APPROACH" || option.category == "ENGAGE")) {
            return true;
        }
    }

    return false;
}

double MDDOEngine::computeDeliberationTime(double urgency) const {
    // Plus l'urgence est haute, moins on délibère
    double factor = 1.0 - urgency * config_.tau_urgency_factor;
    factor = std::clamp(factor, 0.1, 1.0);

    return config_.tau_min_ms + (config_.tau_max_ms - config_.tau_min_ms) * factor;
}

double MDDOEngine::detectConflict(const std::vector<ActionOption>& options) const {
    if (options.size() < 2) {
        return 0.0;
    }

    // Conflit = similarité des utilités des meilleures options
    double best = options[0].utility_score;
    double second = options[1].utility_score;

    if (best < 0.01) {
        return 0.0;
    }

    // Plus les scores sont proches, plus le conflit est élevé
    double diff = best - second;
    double conflict = 1.0 - (diff / best);

    return std::clamp(conflict, 0.0, 1.0);
}

} // namespace mcee
