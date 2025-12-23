/**
 * @file ADDOConfig.hpp
 * @brief Configuration du module ADDO (Algorithme de Détermination Dynamique des Objectifs)
 * @version 1.0
 * @date 2025-12-23
 *
 * Implémente le modèle MDDO avec 16 variables principales, interactions,
 * résilience, stochasticité et influence de la mémoire graphe.
 *
 * Équation générale:
 * G(t) = f( Σ w_i(t)·P_i(t)·L_i(t) + interactions + Rs(t)·Σ P_ℓ + S(t) + M_graph(t) )
 */

#pragma once

#include <array>
#include <string>
#include <chrono>
#include <functional>
#include <cmath>
#include <random>

namespace mcee {

// ═══════════════════════════════════════════════════════════════════════════
// CONSTANTES
// ═══════════════════════════════════════════════════════════════════════════

constexpr size_t NUM_GOAL_VARIABLES = 16;

/**
 * @brief Noms des 16 variables principales du modèle MDDO
 */
inline const std::array<std::string, NUM_GOAL_VARIABLES> GOAL_VARIABLE_NAMES = {
    "Valeurs",              // V(t)  - Valeurs personnelles
    "Motivations",          // M(t)  - Motivations intrinsèques/extrinsèques
    "Experiences",          // E(t)  - Expériences personnelles passées
    "Sentiments",           // S(t)  - Sentiments et émotions (lié à Ft)
    "Clarte",               // C(t)  - Clarté de la vision
    "Environnement",        // En(t) - Opportunités et contraintes externes
    "Competences",          // T(t)  - Compétences et talents
    "Besoins",              // B(t)  - Besoins fondamentaux (Maslow)
    "Modeles",              // Mo(t) - Modèles et inspirations
    "ConnaissanceSoi",      // K(t)  - Connaissance de soi
    "Croyances",            // Cr(t) - Croyances personnelles
    "Depassement",          // D(t)  - Volonté de dépassement
    "Circonstances",        // Ci(t) - Circonstances de vie
    "SouvenirsEmotionnels", // Em(t) - Souvenirs émotionnels
    "Regrets",              // R(t)  - Regrets et nostalgie
    "Traumatismes"          // Tr(t) - Traumatismes
};

/**
 * @brief Indices des variables pour accès direct
 */
enum class GoalVariable : size_t {
    VALEURS = 0,
    MOTIVATIONS = 1,
    EXPERIENCES = 2,
    SENTIMENTS = 3,
    CLARTE = 4,
    ENVIRONNEMENT = 5,
    COMPETENCES = 6,
    BESOINS = 7,
    MODELES = 8,
    CONNAISSANCE_SOI = 9,
    CROYANCES = 10,
    DEPASSEMENT = 11,
    CIRCONSTANCES = 12,
    SOUVENIRS_EMOTIONNELS = 13,
    REGRETS = 14,
    TRAUMATISMES = 15
};

// ═══════════════════════════════════════════════════════════════════════════
// CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Configuration du module ADDO
 */
struct ADDOConfig {
    // ─────────────────────────────────────────────────────────────────────────
    // Poids initiaux des 16 variables w_i(t=0)
    // ─────────────────────────────────────────────────────────────────────────
    std::array<double, NUM_GOAL_VARIABLES> initial_weights = {
        0.12,  // Valeurs - impact fort
        0.10,  // Motivations
        0.08,  // Expériences
        0.08,  // Sentiments (lié à Ft du ConscienceEngine)
        0.06,  // Clarté de vision
        0.07,  // Environnement
        0.06,  // Compétences
        0.08,  // Besoins fondamentaux
        0.04,  // Modèles
        0.05,  // Connaissance de soi
        0.06,  // Croyances
        0.05,  // Dépassement
        0.04,  // Circonstances
        0.04,  // Souvenirs émotionnels
        0.03,  // Regrets
        0.04   // Traumatismes
    };
    // Note: Σ w_i = 1.0

    // ─────────────────────────────────────────────────────────────────────────
    // Paramètres d'interaction
    // ─────────────────────────────────────────────────────────────────────────
    double interaction_positive_scale = 0.1;   // Échelle des synergies c_ij^+
    double interaction_negative_scale = 0.15;  // Échelle des oppositions c_ik^-

    // ─────────────────────────────────────────────────────────────────────────
    // Résilience Rs(t)
    // ─────────────────────────────────────────────────────────────────────────
    double resilience_base = 0.5;              // Résilience initiale
    double resilience_growth_rate = 0.001;     // Croissance avec l'expérience
    double resilience_max = 1.0;               // Plafond
    double resilience_decay_on_trauma = 0.1;   // Décroissance sur trauma

    // ─────────────────────────────────────────────────────────────────────────
    // Stochasticité S(t)
    // ─────────────────────────────────────────────────────────────────────────
    double stochasticity_amplitude = 0.05;     // Amplitude du bruit
    double stochasticity_bias = 0.0;           // Biais (opportunités vs crises)

    // ─────────────────────────────────────────────────────────────────────────
    // Influence Mémoire Graphe M_graph(t)
    // ─────────────────────────────────────────────────────────────────────────
    double alpha_memory_positive = 0.3;        // α : poids souvenirs positifs
    double alpha_memory_negative = 0.3;        // α : poids souvenirs négatifs
    double gamma_trauma = 0.5;                 // γ : poids trauma dans M_graph

    // ─────────────────────────────────────────────────────────────────────────
    // Fonction de sortie
    // ─────────────────────────────────────────────────────────────────────────
    bool use_sigmoid_output = true;            // Appliquer sigmoïde à G(t)
    double sigmoid_steepness = 2.0;            // Pente de la sigmoïde

    // ─────────────────────────────────────────────────────────────────────────
    // Intégration avec autres modules
    // ─────────────────────────────────────────────────────────────────────────
    bool use_wisdom_modulation = true;         // Moduler w_i par Wt (sagesse)
    bool use_sentiment_for_S = true;           // Utiliser Ft pour Sentiments
    bool emergency_override = true;            // Amyghaleon court-circuite G(t)

    // ─────────────────────────────────────────────────────────────────────────
    // Mise à jour
    // ─────────────────────────────────────────────────────────────────────────
    double update_interval_ms = 100.0;         // Intervalle (10 Hz)
    double weight_adaptation_rate = 0.01;      // Taux d'adaptation des poids
};

// ═══════════════════════════════════════════════════════════════════════════
// STRUCTURES DE DONNÉES
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief État des 16 variables à un instant t
 */
struct GoalVariables {
    std::array<double, NUM_GOAL_VARIABLES> P{};      // Valeurs des variables P_i(t)
    std::array<double, NUM_GOAL_VARIABLES> w{};      // Poids w_i(t)
    std::array<double, NUM_GOAL_VARIABLES> L{};      // Contraintes L_i(t)

    GoalVariables() {
        P.fill(0.5);  // Valeurs neutres
        w.fill(1.0 / NUM_GOAL_VARIABLES);  // Poids uniformes
        L.fill(1.0);  // Pas de contrainte
    }

    [[nodiscard]] double getVariable(GoalVariable var) const {
        return P[static_cast<size_t>(var)];
    }

    void setVariable(GoalVariable var, double value) {
        P[static_cast<size_t>(var)] = std::clamp(value, 0.0, 1.0);
    }

    [[nodiscard]] double getWeight(GoalVariable var) const {
        return w[static_cast<size_t>(var)];
    }

    void setWeight(GoalVariable var, double value) {
        w[static_cast<size_t>(var)] = std::clamp(value, 0.0, 1.0);
    }
};

/**
 * @brief Matrice d'interactions entre variables
 */
struct InteractionMatrix {
    // Interactions positives (synergies) : c_ij^+
    std::array<std::array<double, NUM_GOAL_VARIABLES>, NUM_GOAL_VARIABLES> positive{};

    // Interactions négatives (oppositions) : c_ik^-
    std::array<std::array<double, NUM_GOAL_VARIABLES>, NUM_GOAL_VARIABLES> negative{};

    InteractionMatrix() {
        // Initialiser à zéro
        for (auto& row : positive) row.fill(0.0);
        for (auto& row : negative) row.fill(0.0);

        // Définir les synergies connues
        setPositive(GoalVariable::MOTIVATIONS, GoalVariable::DEPASSEMENT, 0.3);
        setPositive(GoalVariable::COMPETENCES, GoalVariable::DEPASSEMENT, 0.2);
        setPositive(GoalVariable::CONNAISSANCE_SOI, GoalVariable::CLARTE, 0.25);
        setPositive(GoalVariable::VALEURS, GoalVariable::CROYANCES, 0.2);
        setPositive(GoalVariable::MODELES, GoalVariable::MOTIVATIONS, 0.15);
        setPositive(GoalVariable::EXPERIENCES, GoalVariable::COMPETENCES, 0.2);

        // Définir les oppositions connues
        setNegative(GoalVariable::TRAUMATISMES, GoalVariable::DEPASSEMENT, 0.4);
        setNegative(GoalVariable::REGRETS, GoalVariable::MOTIVATIONS, 0.25);
        setNegative(GoalVariable::TRAUMATISMES, GoalVariable::CLARTE, 0.3);
        setNegative(GoalVariable::CIRCONSTANCES, GoalVariable::ENVIRONNEMENT, 0.2);
    }

    void setPositive(GoalVariable i, GoalVariable j, double value) {
        positive[static_cast<size_t>(i)][static_cast<size_t>(j)] = value;
        positive[static_cast<size_t>(j)][static_cast<size_t>(i)] = value; // Symétrique
    }

    void setNegative(GoalVariable i, GoalVariable k, double value) {
        negative[static_cast<size_t>(i)][static_cast<size_t>(k)] = value;
        negative[static_cast<size_t>(k)][static_cast<size_t>(i)] = value; // Symétrique
    }
};

/**
 * @brief Influence de la mémoire graphe
 */
struct MemoryGraphInfluence {
    double S_positive = 0.0;   // S⁺(t) : somme souvenirs positifs
    double S_negative = 0.0;   // S⁻(t) : somme souvenirs négatifs
    double T_trauma = 0.0;     // T_trauma(t) : intensité trauma actif

    std::chrono::steady_clock::time_point timestamp;

    MemoryGraphInfluence() : timestamp(std::chrono::steady_clock::now()) {}

    /**
     * @brief Calcule M_graph(t) = α[S⁺ - S⁻] - γ·T_trauma
     */
    [[nodiscard]] double compute(double alpha, double gamma) const {
        return alpha * (S_positive - S_negative) - gamma * T_trauma;
    }
};

/**
 * @brief État complet de l'objectif
 */
struct GoalState {
    double G = 0.0;                              // G(t) : valeur de l'objectif
    double G_raw = 0.0;                          // Avant normalisation

    GoalVariables variables;                     // Variables P, w, L
    double resilience = 0.5;                     // Rs(t)
    double stochasticity = 0.0;                  // S(t) terme aléatoire
    MemoryGraphInfluence memory_influence;       // M_graph(t)

    double interaction_positive_sum = 0.0;       // Σ c_ij^+ P_i P_j
    double interaction_negative_sum = 0.0;       // Σ c_ik^- P_i P_k

    std::string dominant_variable;               // Variable dominante
    double dominant_value = 0.0;                 // Sa valeur

    bool emergency_override = false;             // Court-circuit Amyghaleon
    std::string emergency_goal;                  // Objectif d'urgence

    std::chrono::steady_clock::time_point timestamp;

    GoalState() : timestamp(std::chrono::steady_clock::now()) {}
};

// ═══════════════════════════════════════════════════════════════════════════
// CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════

using GoalUpdateCallback = std::function<void(const GoalState&)>;
using GoalChangeCallback = std::function<void(double old_goal, double new_goal, const std::string& reason)>;
using EmergencyGoalCallback = std::function<void(const std::string& emergency_goal)>;

} // namespace mcee
