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

        // ═══════════════════════════════════════════════════════════════════
        // SYNERGIES (c_ij^+) - Variables qui se renforcent mutuellement
        // ═══════════════════════════════════════════════════════════════════

        // Cluster "Croissance personnelle"
        setPositive(GoalVariable::MOTIVATIONS, GoalVariable::DEPASSEMENT, 0.35);
        setPositive(GoalVariable::COMPETENCES, GoalVariable::DEPASSEMENT, 0.30);
        setPositive(GoalVariable::CONNAISSANCE_SOI, GoalVariable::DEPASSEMENT, 0.25);
        setPositive(GoalVariable::EXPERIENCES, GoalVariable::COMPETENCES, 0.30);
        setPositive(GoalVariable::MODELES, GoalVariable::DEPASSEMENT, 0.20);

        // Cluster "Vision claire"
        setPositive(GoalVariable::CONNAISSANCE_SOI, GoalVariable::CLARTE, 0.35);
        setPositive(GoalVariable::VALEURS, GoalVariable::CLARTE, 0.25);
        setPositive(GoalVariable::EXPERIENCES, GoalVariable::CLARTE, 0.20);
        setPositive(GoalVariable::CROYANCES, GoalVariable::CLARTE, 0.15);

        // Cluster "Fondement identitaire"
        setPositive(GoalVariable::VALEURS, GoalVariable::CROYANCES, 0.30);
        setPositive(GoalVariable::VALEURS, GoalVariable::CONNAISSANCE_SOI, 0.25);
        setPositive(GoalVariable::EXPERIENCES, GoalVariable::CONNAISSANCE_SOI, 0.20);
        setPositive(GoalVariable::MODELES, GoalVariable::VALEURS, 0.15);

        // Cluster "Motivation et inspiration"
        setPositive(GoalVariable::MODELES, GoalVariable::MOTIVATIONS, 0.25);
        setPositive(GoalVariable::SOUVENIRS_EMOTIONNELS, GoalVariable::MOTIVATIONS, 0.20);
        setPositive(GoalVariable::ENVIRONNEMENT, GoalVariable::MOTIVATIONS, 0.20);
        setPositive(GoalVariable::BESOINS, GoalVariable::MOTIVATIONS, 0.15);

        // Cluster "Ressources et capacités"
        setPositive(GoalVariable::COMPETENCES, GoalVariable::CONNAISSANCE_SOI, 0.20);
        setPositive(GoalVariable::ENVIRONNEMENT, GoalVariable::COMPETENCES, 0.15);
        setPositive(GoalVariable::CIRCONSTANCES, GoalVariable::COMPETENCES, 0.10);

        // Cluster "Émotionnel positif"
        setPositive(GoalVariable::SENTIMENTS, GoalVariable::MOTIVATIONS, 0.25);
        setPositive(GoalVariable::SENTIMENTS, GoalVariable::DEPASSEMENT, 0.20);
        setPositive(GoalVariable::SOUVENIRS_EMOTIONNELS, GoalVariable::SENTIMENTS, 0.25);

        // ═══════════════════════════════════════════════════════════════════
        // OPPOSITIONS (c_ik^-) - Variables qui s'inhibent mutuellement
        // ═══════════════════════════════════════════════════════════════════

        // Trauma inhibe le développement
        setNegative(GoalVariable::TRAUMATISMES, GoalVariable::DEPASSEMENT, 0.45);
        setNegative(GoalVariable::TRAUMATISMES, GoalVariable::MOTIVATIONS, 0.40);
        setNegative(GoalVariable::TRAUMATISMES, GoalVariable::CLARTE, 0.35);
        setNegative(GoalVariable::TRAUMATISMES, GoalVariable::CONNAISSANCE_SOI, 0.30);
        setNegative(GoalVariable::TRAUMATISMES, GoalVariable::SENTIMENTS, 0.35);

        // Regrets inhibent l'action
        setNegative(GoalVariable::REGRETS, GoalVariable::MOTIVATIONS, 0.30);
        setNegative(GoalVariable::REGRETS, GoalVariable::DEPASSEMENT, 0.25);
        setNegative(GoalVariable::REGRETS, GoalVariable::CLARTE, 0.20);
        setNegative(GoalVariable::REGRETS, GoalVariable::SENTIMENTS, 0.25);

        // Circonstances défavorables
        setNegative(GoalVariable::CIRCONSTANCES, GoalVariable::ENVIRONNEMENT, 0.35);
        setNegative(GoalVariable::CIRCONSTANCES, GoalVariable::MOTIVATIONS, 0.20);
        setNegative(GoalVariable::CIRCONSTANCES, GoalVariable::DEPASSEMENT, 0.25);

        // Besoins non satisfaits perturbent la clarté
        setNegative(GoalVariable::BESOINS, GoalVariable::CLARTE, 0.25);
        setNegative(GoalVariable::BESOINS, GoalVariable::DEPASSEMENT, 0.20);

        // Croyances limitantes vs Dépassement
        setNegative(GoalVariable::CROYANCES, GoalVariable::DEPASSEMENT, 0.15);  // Peut être +/-

        // Souvenirs émotionnels négatifs
        setNegative(GoalVariable::SOUVENIRS_EMOTIONNELS, GoalVariable::CLARTE, 0.15);

        // Environnement hostile
        setNegative(GoalVariable::ENVIRONNEMENT, GoalVariable::BESOINS, 0.20);
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
// MAPPING ÉMOTIONS → VARIABLES
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Matrice de mapping des 24 émotions vers les 16 variables MDDO
 *
 * Chaque émotion influence une ou plusieurs variables avec des poids différents.
 * Les valeurs sont normalisées pour refléter l'impact relatif.
 *
 * Format: emotion_variable_mapping[emotion_index][variable_index] = weight
 */
struct EmotionVariableMapping {
    // Émotions principales (6 de base + extensions)
    // Indices: 0=Joie, 1=Tristesse, 2=Peur, 3=Colère, 4=Surprise, 5=Dégoût
    // Extensions: 6=Anxiété, 7=Frustration, 8=Espoir, 9=Fierté, 10=Honte,
    // 11=Culpabilité, 12=Nostalgie, 13=Sérénité, 14=Excitation, 15=Ennui,
    // 16=Confiance, 17=Méfiance, 18=Gratitude, 19=Envie, 20=Empathie,
    // 21=Mépris, 22=Admiration, 23=Amour

    static constexpr size_t NUM_EMOTIONS = 24;

    // Mapping: [émotion][variable] → poids d'influence [-1, 1]
    // Positif = renforce la variable, Négatif = diminue la variable
    std::array<std::array<double, NUM_GOAL_VARIABLES>, NUM_EMOTIONS> weights{};

    EmotionVariableMapping() {
        // Initialiser à zéro
        for (auto& row : weights) row.fill(0.0);

        // ═══════════════════════════════════════════════════════════════════
        // Émotions de base (Ekman)
        // ═══════════════════════════════════════════════════════════════════

        // JOIE (0)
        set(0, GoalVariable::SENTIMENTS, 0.9);
        set(0, GoalVariable::MOTIVATIONS, 0.5);
        set(0, GoalVariable::DEPASSEMENT, 0.4);
        set(0, GoalVariable::SOUVENIRS_EMOTIONNELS, 0.6);
        set(0, GoalVariable::CONNAISSANCE_SOI, 0.2);
        set(0, GoalVariable::REGRETS, -0.3);

        // TRISTESSE (1)
        set(1, GoalVariable::SENTIMENTS, -0.7);
        set(1, GoalVariable::MOTIVATIONS, -0.4);
        set(1, GoalVariable::SOUVENIRS_EMOTIONNELS, 0.5);
        set(1, GoalVariable::REGRETS, 0.4);
        set(1, GoalVariable::CONNAISSANCE_SOI, 0.2);  // Introspection
        set(1, GoalVariable::DEPASSEMENT, -0.3);

        // PEUR (2)
        set(2, GoalVariable::SENTIMENTS, -0.6);
        set(2, GoalVariable::TRAUMATISMES, 0.5);
        set(2, GoalVariable::DEPASSEMENT, -0.5);
        set(2, GoalVariable::CLARTE, -0.4);
        set(2, GoalVariable::BESOINS, 0.3);  // Besoin de sécurité
        set(2, GoalVariable::ENVIRONNEMENT, -0.3);

        // COLÈRE (3)
        set(3, GoalVariable::SENTIMENTS, -0.5);
        set(3, GoalVariable::MOTIVATIONS, 0.3);  // Peut motiver l'action
        set(3, GoalVariable::VALEURS, 0.2);  // Réaction à injustice
        set(3, GoalVariable::CROYANCES, 0.2);
        set(3, GoalVariable::CLARTE, -0.3);
        set(3, GoalVariable::CONNAISSANCE_SOI, -0.2);

        // SURPRISE (4)
        set(4, GoalVariable::CLARTE, -0.2);
        set(4, GoalVariable::EXPERIENCES, 0.3);
        set(4, GoalVariable::CONNAISSANCE_SOI, 0.2);
        set(4, GoalVariable::ENVIRONNEMENT, 0.2);

        // DÉGOÛT (5)
        set(5, GoalVariable::SENTIMENTS, -0.4);
        set(5, GoalVariable::VALEURS, 0.3);  // Réaction morale
        set(5, GoalVariable::CROYANCES, 0.2);
        set(5, GoalVariable::ENVIRONNEMENT, -0.3);

        // ═══════════════════════════════════════════════════════════════════
        // Émotions secondaires
        // ═══════════════════════════════════════════════════════════════════

        // ANXIÉTÉ (6)
        set(6, GoalVariable::SENTIMENTS, -0.5);
        set(6, GoalVariable::CLARTE, -0.5);
        set(6, GoalVariable::DEPASSEMENT, -0.4);
        set(6, GoalVariable::BESOINS, 0.4);
        set(6, GoalVariable::TRAUMATISMES, 0.3);

        // FRUSTRATION (7)
        set(7, GoalVariable::SENTIMENTS, -0.4);
        set(7, GoalVariable::MOTIVATIONS, -0.3);
        set(7, GoalVariable::DEPASSEMENT, 0.2);  // Peut pousser à agir
        set(7, GoalVariable::CIRCONSTANCES, -0.3);

        // ESPOIR (8)
        set(8, GoalVariable::SENTIMENTS, 0.6);
        set(8, GoalVariable::MOTIVATIONS, 0.6);
        set(8, GoalVariable::DEPASSEMENT, 0.5);
        set(8, GoalVariable::CLARTE, 0.3);
        set(8, GoalVariable::CROYANCES, 0.3);

        // FIERTÉ (9)
        set(9, GoalVariable::SENTIMENTS, 0.7);
        set(9, GoalVariable::CONNAISSANCE_SOI, 0.5);
        set(9, GoalVariable::COMPETENCES, 0.4);
        set(9, GoalVariable::DEPASSEMENT, 0.4);
        set(9, GoalVariable::VALEURS, 0.2);

        // HONTE (10)
        set(10, GoalVariable::SENTIMENTS, -0.6);
        set(10, GoalVariable::CONNAISSANCE_SOI, 0.3);  // Conscience de soi
        set(10, GoalVariable::VALEURS, 0.2);
        set(10, GoalVariable::DEPASSEMENT, -0.4);
        set(10, GoalVariable::TRAUMATISMES, 0.3);

        // CULPABILITÉ (11)
        set(11, GoalVariable::SENTIMENTS, -0.5);
        set(11, GoalVariable::VALEURS, 0.4);  // Sens moral
        set(11, GoalVariable::REGRETS, 0.5);
        set(11, GoalVariable::CONNAISSANCE_SOI, 0.3);
        set(11, GoalVariable::CROYANCES, 0.2);

        // NOSTALGIE (12)
        set(12, GoalVariable::SOUVENIRS_EMOTIONNELS, 0.8);
        set(12, GoalVariable::SENTIMENTS, 0.2);
        set(12, GoalVariable::REGRETS, 0.3);
        set(12, GoalVariable::EXPERIENCES, 0.3);
        set(12, GoalVariable::MODELES, 0.2);

        // SÉRÉNITÉ (13)
        set(13, GoalVariable::SENTIMENTS, 0.7);
        set(13, GoalVariable::CLARTE, 0.5);
        set(13, GoalVariable::CONNAISSANCE_SOI, 0.4);
        set(13, GoalVariable::BESOINS, -0.3);  // Besoins satisfaits
        set(13, GoalVariable::TRAUMATISMES, -0.2);

        // EXCITATION (14)
        set(14, GoalVariable::SENTIMENTS, 0.5);
        set(14, GoalVariable::MOTIVATIONS, 0.6);
        set(14, GoalVariable::DEPASSEMENT, 0.5);
        set(14, GoalVariable::CLARTE, -0.2);  // Impulsivité

        // ENNUI (15)
        set(15, GoalVariable::SENTIMENTS, -0.2);
        set(15, GoalVariable::MOTIVATIONS, -0.5);
        set(15, GoalVariable::DEPASSEMENT, 0.3);  // Besoin de stimulation
        set(15, GoalVariable::ENVIRONNEMENT, -0.3);
        set(15, GoalVariable::CLARTE, -0.2);

        // CONFIANCE (16)
        set(16, GoalVariable::SENTIMENTS, 0.4);
        set(16, GoalVariable::CONNAISSANCE_SOI, 0.5);
        set(16, GoalVariable::COMPETENCES, 0.4);
        set(16, GoalVariable::DEPASSEMENT, 0.4);
        set(16, GoalVariable::ENVIRONNEMENT, 0.3);

        // MÉFIANCE (17)
        set(17, GoalVariable::SENTIMENTS, -0.3);
        set(17, GoalVariable::ENVIRONNEMENT, -0.5);
        set(17, GoalVariable::CLARTE, 0.2);  // Vigilance
        set(17, GoalVariable::TRAUMATISMES, 0.2);

        // GRATITUDE (18)
        set(18, GoalVariable::SENTIMENTS, 0.6);
        set(18, GoalVariable::VALEURS, 0.4);
        set(18, GoalVariable::SOUVENIRS_EMOTIONNELS, 0.4);
        set(18, GoalVariable::CONNAISSANCE_SOI, 0.2);

        // ENVIE (19)
        set(19, GoalVariable::SENTIMENTS, -0.3);
        set(19, GoalVariable::MOTIVATIONS, 0.3);
        set(19, GoalVariable::MODELES, 0.4);
        set(19, GoalVariable::DEPASSEMENT, 0.3);
        set(19, GoalVariable::VALEURS, -0.2);

        // EMPATHIE (20)
        set(20, GoalVariable::SENTIMENTS, 0.3);
        set(20, GoalVariable::VALEURS, 0.5);
        set(20, GoalVariable::CONNAISSANCE_SOI, 0.3);
        set(20, GoalVariable::CROYANCES, 0.2);
        set(20, GoalVariable::MODELES, 0.2);

        // MÉPRIS (21)
        set(21, GoalVariable::SENTIMENTS, -0.3);
        set(21, GoalVariable::VALEURS, 0.2);
        set(21, GoalVariable::CROYANCES, 0.3);
        set(21, GoalVariable::CONNAISSANCE_SOI, -0.2);

        // ADMIRATION (22)
        set(22, GoalVariable::SENTIMENTS, 0.5);
        set(22, GoalVariable::MODELES, 0.7);
        set(22, GoalVariable::MOTIVATIONS, 0.4);
        set(22, GoalVariable::DEPASSEMENT, 0.4);
        set(22, GoalVariable::VALEURS, 0.2);

        // AMOUR (23)
        set(23, GoalVariable::SENTIMENTS, 0.8);
        set(23, GoalVariable::VALEURS, 0.4);
        set(23, GoalVariable::BESOINS, -0.4);  // Besoins d'appartenance satisfaits
        set(23, GoalVariable::SOUVENIRS_EMOTIONNELS, 0.5);
        set(23, GoalVariable::CONNAISSANCE_SOI, 0.2);
    }

    void set(size_t emotion_idx, GoalVariable var, double weight) {
        weights[emotion_idx][static_cast<size_t>(var)] = weight;
    }

    [[nodiscard]] double get(size_t emotion_idx, GoalVariable var) const {
        return weights[emotion_idx][static_cast<size_t>(var)];
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════

using GoalUpdateCallback = std::function<void(const GoalState&)>;
using GoalChangeCallback = std::function<void(double old_goal, double new_goal, const std::string& reason)>;
using EmergencyGoalCallback = std::function<void(const std::string& emergency_goal)>;

} // namespace mcee
