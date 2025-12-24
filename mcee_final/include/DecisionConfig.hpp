/**
 * @file DecisionConfig.hpp
 * @brief Configuration du Module Prise de Décision Réfléchie
 * @version 1.0
 * @date 2025-12-23
 *
 * Architecture 4 phases :
 * 1. Perception → SituationFrame Σ(t)
 * 2. Activation mémorielle → MemoryContext M(t)
 * 3. Génération & Simulation → Options A(t) + Projections
 * 4. Arbitrage & Sélection → Décision D(t) + Confiance κ(t)
 */

#pragma once

#include "Types.hpp"
#include "ConscienceConfig.hpp"
#include "ADDOConfig.hpp"
#include <array>
#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <optional>

namespace mcee {

// ═══════════════════════════════════════════════════════════════════════════
// CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════

struct DecisionConfig {
    // ─────────────────────────────────────────────────────────────────────────
    // Temps de délibération
    // ─────────────────────────────────────────────────────────────────────────
    double tau_max_ms = 5000.0;          // Temps max de délibération (5s)
    double tau_min_ms = 100.0;           // Temps min (mode réflexe)

    // ─────────────────────────────────────────────────────────────────────────
    // Seuils Phase 4 (Arbitrage)
    // ─────────────────────────────────────────────────────────────────────────
    double theta_veto = 0.80;            // Seuil veto Amyghaleon
    double theta_conflict = 0.30;        // Seuil détection conflit d'objectifs
    double theta_info = 0.60;            // Seuil incertitude pour InfoRequest
    double theta_confidence = 0.20;      // Seuil confiance minimal
    double theta_meta = 0.50;            // Seuil métacognition "know_unknown"
    double theta_automate = 10;          // Succès requis pour promotion réflexe

    // ─────────────────────────────────────────────────────────────────────────
    // Poids fonction de score (modifiables par Ft)
    // ─────────────────────────────────────────────────────────────────────────
    double w1_goal_align = 0.35;         // Alignement objectifs
    double w2_emo_forecast = 0.25;       // Prédiction émotionnelle
    double w3_confidence = 0.15;         // Confiance dans l'option
    double w4_uncertainty = 0.10;        // Pénalité incertitude
    double w5_risk = 0.15;               // Pénalité risque

    // ─────────────────────────────────────────────────────────────────────────
    // Similarité épisodique
    // ─────────────────────────────────────────────────────────────────────────
    double alpha_ctx = 0.40;             // Poids similarité contextuelle
    double beta_emo = 0.40;              // Poids similarité émotionnelle
    double gamma_temp = 0.20;            // Poids proximité temporelle

    // ─────────────────────────────────────────────────────────────────────────
    // Génération d'options
    // ─────────────────────────────────────────────────────────────────────────
    size_t max_macro_options = 8;        // Familles d'actions max
    size_t top_k_refinement = 4;         // Options raffinées
    size_t max_simulation_depth = 3;     // Profondeur simulation max

    // ─────────────────────────────────────────────────────────────────────────
    // Modulation affective
    // ─────────────────────────────────────────────────────────────────────────
    double Ft_positive_exploration_boost = 0.15;   // Bonus exploration si Ft > 0
    double Ft_negative_prudence_boost = 0.20;      // Bonus prudence si Ft < 0

    // ─────────────────────────────────────────────────────────────────────────
    // Méta-actions
    // ─────────────────────────────────────────────────────────────────────────
    bool enable_meta_actions = true;     // Autoriser InfoRequest, Defer, etc.
    double info_request_cost = 0.1;      // Coût court terme de demander info
};

// ═══════════════════════════════════════════════════════════════════════════
// PHASE 1 : PERCEPTION - SituationFrame
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Alerte Amyghaleon active
 */
struct AmyghaleonAlert {
    std::string type;          // "reputation", "escalation", "financial", etc.
    double severity = 0.0;     // [0, 1]
    std::string source;        // Origine de l'alerte
};

/**
 * @brief Cadre situationnel subjectif Σ(t)
 */
struct SituationFrame {
    // État émotionnel
    EmotionalState emotional_state;
    double Ct = 0.0;                     // Niveau de conscience
    double Ft = 0.0;                     // Fond affectif

    // Contexte
    std::string context_type;            // "reunion", "projet", "personnel", etc.
    std::string context_detail;          // Détails additionnels

    // Urgence et alertes
    double urgency = 0.0;                // U(t) ∈ [0, 1]
    std::vector<AmyghaleonAlert> alerts;

    // Temps de délibération calculé
    double tau_delib_ms = 0.0;

    std::chrono::steady_clock::time_point timestamp;

    SituationFrame() : timestamp(std::chrono::steady_clock::now()) {}

    /**
     * @brief Calcule τ_delib = τ_max × (1 - U(t))
     */
    void computeDeliberationTime(double tau_max) {
        tau_delib_ms = tau_max * (1.0 - urgency);
    }

    /**
     * @brief Vérifie si mode réflexe (urgence maximale)
     */
    [[nodiscard]] bool isReflexMode(double threshold = 0.9) const {
        return urgency >= threshold;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// PHASE 2 : ACTIVATION MÉMORIELLE - MemoryContext
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Épisode mémoriel récupéré
 */
struct MemoryEpisode {
    std::string id;
    std::string description;
    double similarity = 0.0;             // Similarité avec situation actuelle
    double outcome_valence = 0.0;        // Résultat : positif/négatif
    std::string action_taken;            // Action prise à l'époque
    std::string lesson;                  // Leçon apprise
};

/**
 * @brief Procédure mémorisée (MP)
 */
struct MemoryProcedure {
    std::string id;
    std::string name;
    std::string trigger_context;         // Contexte déclencheur
    double success_rate = 0.0;           // Taux de succès
    size_t activation_count = 0;         // Nombre d'activations
    bool is_reflex = false;              // Promu en réflexe ?
};

/**
 * @brief Concept sémantique (MS)
 */
struct SemanticConcept {
    std::string name;             // Nom du concept (renommé de 'concept' - mot-clé C++20)
    double relevance = 0.0;
    std::vector<std::string> associations;
};

/**
 * @brief Contexte mémoriel M(t)
 */
struct MemoryContext {
    std::vector<MemoryEpisode> episodes;      // ME : top-k épisodes similaires
    std::vector<SemanticConcept> concepts;    // MS : concepts associés
    std::vector<MemoryProcedure> procedures;  // MP : procédures applicables
    std::vector<std::string> patterns;        // MLT : patterns émotionnels

    std::chrono::steady_clock::time_point timestamp;

    MemoryContext() : timestamp(std::chrono::steady_clock::now()) {}

    /**
     * @brief Trouve le meilleur réflexe disponible
     */
    [[nodiscard]] std::optional<MemoryProcedure> getBestReflex() const {
        for (const auto& proc : procedures) {
            if (proc.is_reflex) return proc;
        }
        return std::nullopt;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// PHASE 3 : GÉNÉRATION & SIMULATION
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Type de méta-action
 */
enum class MetaActionType {
    NONE,           // Action normale
    OBSERVE,        // Collecter plus d'info passive
    QUESTION,       // Demander activement des clarifications
    DEFER           // Reporter la décision
};

/**
 * @brief Projection des conséquences d'une action
 */
struct ActionProjection {
    double outcome_expected = 0.0;       // E[outcome | action, episodes]
    double emotional_forecast = 0.0;     // Prédiction émotionnelle (valence)
    double goal_alignment = 0.0;         // Σ w_k × align(action, G_k)
    double uncertainty = 0.0;            // H[outcome | action]
    double risk = 0.0;                   // Risque évalué

    size_t simulation_depth = 1;         // Profondeur de simulation utilisée
};

/**
 * @brief Option d'action avec projection
 */
struct ActionOption {
    std::string id;
    std::string name;
    std::string description;
    std::string category;                // Macro-option : "agir", "attendre", "fuir", etc.

    ActionProjection projection;

    // Méta-action ?
    MetaActionType meta_type = MetaActionType::NONE;
    std::string meta_target;             // Cible de la demande d'info

    // Veto
    bool vetoed = false;
    std::string veto_reason;

    // Score calculé en Phase 4
    double score = 0.0;

    [[nodiscard]] bool isMetaAction() const {
        return meta_type != MetaActionType::NONE;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// PHASE 4 : ARBITRAGE & SÉLECTION
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief État métacognitif
 */
struct MetaState {
    double confidence = 0.0;             // κ(t) : marge de victoire
    double uncertainty_global = 0.0;     // Incertitude moyenne
    double conflict_level = 0.0;         // Niveau de conflit entre objectifs
    bool know_unknown = false;           // "Je sais que je ne sais pas"

    std::chrono::steady_clock::time_point timestamp;

    MetaState() : timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Conflit entre objectifs
 */
struct GoalConflict {
    std::string goal1_name;
    std::string goal2_name;
    double conflict_intensity = 0.0;
    std::string recommended_resolution;   // "priority", "consultation_MA", "extend_delib"
};

/**
 * @brief Résultat de décision
 */
struct DecisionResult {
    // Décision principale
    std::string action_id;
    std::string action_name;
    double score = 0.0;
    double confidence = 0.0;             // κ(t)

    // Méta-action ?
    bool is_meta_action = false;
    MetaActionType meta_type = MetaActionType::NONE;

    // Mode dégradé ?
    bool reflex_mode = false;

    // Conflits détectés
    std::vector<GoalConflict> conflicts;

    // État métacognitif
    MetaState meta_state;

    // Temps de délibération effectif
    double deliberation_time_ms = 0.0;

    // Toutes les options évaluées
    std::vector<ActionOption> all_options;

    std::chrono::steady_clock::time_point timestamp;

    DecisionResult() : timestamp(std::chrono::steady_clock::now()) {}
};

// ═══════════════════════════════════════════════════════════════════════════
// APPRENTISSAGE POST-DÉCISION
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Feedback post-décision
 */
struct DecisionOutcome {
    std::string decision_id;
    double expected_outcome = 0.0;
    double actual_outcome = 0.0;         // R(t) observé
    double prediction_error = 0.0;       // actual - expected

    bool success = false;
    std::string identity_impact;         // Impact sur l'identité (MA)

    std::chrono::steady_clock::time_point timestamp;

    DecisionOutcome() : timestamp(std::chrono::steady_clock::now()) {}
};

// ═══════════════════════════════════════════════════════════════════════════
// CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════

using DecisionCallback = std::function<void(const DecisionResult&)>;
using VetoCallback = std::function<void(const ActionOption&, const std::string& reason)>;
using MetaActionCallback = std::function<void(MetaActionType, const std::string& target)>;
using ConflictCallback = std::function<void(const GoalConflict&)>;

} // namespace mcee
