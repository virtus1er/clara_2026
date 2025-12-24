/**
 * @file ADDOEngine.hpp
 * @brief Moteur ADDO - Algorithme de Détermination Dynamique des Objectifs
 * @version 1.0
 * @date 2025-12-23
 *
 * Implémente l'équation:
 * G(t) = f( Σ w_i·P_i·L_i + Σ c_ij^+·P_i·P_j - Σ c_ik^-·P_i·P_k
 *          + Rs(t)·Σ P_ℓ + S(t) + M_graph(t) )
 */

#pragma once

#include "ADDOConfig.hpp"
#include "ConscienceConfig.hpp"
#include "MCTGraph.hpp"
#include "Types.hpp"
#include <memory>
#include <deque>
#include <mutex>

namespace mcee {

/**
 * @class ADDOEngine
 * @brief Moteur de détermination dynamique des objectifs
 */
class ADDOEngine {
public:
    /**
     * @brief Constructeur avec configuration
     * @param config Configuration ADDO
     */
    explicit ADDOEngine(const ADDOConfig& config = ADDOConfig{});

    /**
     * @brief Destructeur
     */
    ~ADDOEngine() = default;

    // ═══════════════════════════════════════════════════════════════════════
    // MISE À JOUR PRINCIPALE
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Met à jour l'objectif G(t) avec les entrées courantes
     * @param emotional_state État émotionnel (pour Sentiments)
     * @param sentiment Ft du ConscienceEngine
     * @param wisdom Wt du ConscienceEngine
     * @return État de l'objectif calculé
     */
    GoalState update(const EmotionalState& emotional_state,
                     double sentiment,
                     double wisdom);

    /**
     * @brief Met à jour avec influence mémoire explicite
     * @param emotional_state État émotionnel
     * @param sentiment Ft
     * @param wisdom Wt
     * @param memory_influence Influence de la mémoire graphe
     * @return État de l'objectif
     */
    GoalState updateWithMemory(const EmotionalState& emotional_state,
                               double sentiment,
                               double wisdom,
                               const MemoryGraphInfluence& memory_influence);

    // ═══════════════════════════════════════════════════════════════════════
    // MODIFICATION DES VARIABLES
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Définit une variable P_i(t)
     * @param var Variable à modifier
     * @param value Nouvelle valeur [0, 1]
     */
    void setVariable(GoalVariable var, double value);

    /**
     * @brief Définit une contrainte L_i(t)
     * @param var Variable concernée
     * @param constraint Contrainte [0, 2] (0=bloqué, 1=normal, 2=amplifié)
     */
    void setConstraint(GoalVariable var, double constraint);

    /**
     * @brief Met à jour plusieurs variables depuis un contexte externe
     * @param environment Score environnement [0, 1]
     * @param circumstances Score circonstances [0, 1]
     */
    void updateExternalContext(double environment, double circumstances);

    /**
     * @brief Met à jour les variables liées aux besoins
     * @param physiological Besoins physiologiques satisfaits [0, 1]
     * @param safety Sécurité [0, 1]
     * @param belonging Appartenance [0, 1]
     * @param esteem Estime [0, 1]
     * @param self_actualization Auto-actualisation [0, 1]
     */
    void updateNeeds(double physiological, double safety, double belonging,
                     double esteem, double self_actualization);

    // ═══════════════════════════════════════════════════════════════════════
    // RÉSILIENCE ET TRAUMA
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Enregistre un succès (augmente la résilience)
     * @param intensity Intensité du succès [0, 1]
     */
    void recordSuccess(double intensity);

    /**
     * @brief Enregistre un échec (peut diminuer la résilience)
     * @param intensity Intensité de l'échec [0, 1]
     */
    void recordFailure(double intensity);

    /**
     * @brief Signale un trauma actif
     * @param trauma État du trauma
     */
    void signalTrauma(const TraumaState& trauma);

    /**
     * @brief Définit l'influence de la mémoire graphe
     * @param S_positive Somme souvenirs positifs
     * @param S_negative Somme souvenirs négatifs
     * @param T_trauma Intensité trauma
     */
    void setMemoryInfluence(double S_positive, double S_negative, double T_trauma);

    // ═══════════════════════════════════════════════════════════════════════
    // URGENCE (AMYGHALEON)
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Active le mode urgence (court-circuite G(t))
     * @param emergency_goal Objectif d'urgence (SURVIE, FUITE, PROTECTION)
     */
    void triggerEmergencyOverride(const std::string& emergency_goal);

    /**
     * @brief Désactive le mode urgence
     */
    void clearEmergencyOverride();

    /**
     * @brief Vérifie si en mode urgence
     */
    [[nodiscard]] bool isInEmergencyMode() const;

    // ═══════════════════════════════════════════════════════════════════════
    // ACCESSEURS
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Retourne l'objectif courant G(t)
     */
    [[nodiscard]] double getCurrentGoal() const;

    /**
     * @brief Retourne l'état complet
     */
    [[nodiscard]] const GoalState& getCurrentState() const;

    /**
     * @brief Retourne la résilience Rs(t)
     */
    [[nodiscard]] double getResilience() const;

    /**
     * @brief Retourne les variables courantes
     */
    [[nodiscard]] const GoalVariables& getVariables() const;

    /**
     * @brief Retourne la matrice d'interactions
     */
    [[nodiscard]] const InteractionMatrix& getInteractionMatrix() const;

    /**
     * @brief Retourne la configuration
     */
    [[nodiscard]] const ADDOConfig& getConfig() const { return config_; }

    /**
     * @brief Retourne le mapping émotions → variables
     */
    [[nodiscard]] const EmotionVariableMapping& getEmotionMapping() const { return emotion_mapping_; }

    // ═══════════════════════════════════════════════════════════════════════
    // INTÉGRATION MCTGraph
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Connecte le MCTGraph pour enrichir M_graph(t)
     * @param mct_graph Pointeur vers le graphe mot-émotion
     */
    void setMCTGraph(std::shared_ptr<MCTGraph> mct_graph);

    /**
     * @brief Retourne le MCTGraph connecté
     */
    [[nodiscard]] std::shared_ptr<MCTGraph> getMCTGraph() const { return mct_graph_; }

    // ═══════════════════════════════════════════════════════════════════════
    // CALLBACKS
    // ═══════════════════════════════════════════════════════════════════════

    void setUpdateCallback(GoalUpdateCallback callback);
    void setGoalChangeCallback(GoalChangeCallback callback);
    void setEmergencyCallback(EmergencyGoalCallback callback);

    // ═══════════════════════════════════════════════════════════════════════
    // HISTORIQUE
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Retourne l'historique des objectifs
     */
    [[nodiscard]] const std::deque<double>& getGoalHistory() const;

    /**
     * @brief Retourne la tendance de l'objectif (croissant/décroissant)
     */
    [[nodiscard]] double getGoalTrend() const;

    /**
     * @brief Retourne la stabilité de l'objectif
     */
    [[nodiscard]] double getGoalStability() const;

private:
    ADDOConfig config_;
    GoalState current_state_;
    GoalVariables variables_;
    InteractionMatrix interactions_;
    EmotionVariableMapping emotion_mapping_;

    // Connexion au MCTGraph pour M_graph(t)
    std::shared_ptr<MCTGraph> mct_graph_;

    double resilience_;
    MemoryGraphInfluence memory_influence_;

    bool emergency_mode_ = false;
    std::string emergency_goal_;

    // Historique
    std::deque<double> goal_history_;
    static constexpr size_t MAX_HISTORY_SIZE = 100;

    // Callbacks
    GoalUpdateCallback on_update_;
    GoalChangeCallback on_goal_change_;
    EmergencyGoalCallback on_emergency_;

    // Générateur aléatoire pour stochasticité
    std::mt19937 rng_;
    std::normal_distribution<double> noise_dist_;

    mutable std::mutex mutex_;

    // ─────────────────────────────────────────────────────────────────────────
    // Méthodes privées
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Calcule Σ w_i·P_i·L_i
     */
    [[nodiscard]] double computeWeightedSum() const;

    /**
     * @brief Calcule les interactions positives Σ c_ij^+·P_i·P_j
     */
    [[nodiscard]] double computePositiveInteractions() const;

    /**
     * @brief Calcule les interactions négatives Σ c_ik^-·P_i·P_k
     */
    [[nodiscard]] double computeNegativeInteractions() const;

    /**
     * @brief Calcule le terme de résilience Rs(t)·Σ P_ℓ
     */
    [[nodiscard]] double computeResilienceTerm() const;

    /**
     * @brief Génère le terme stochastique S(t)
     */
    [[nodiscard]] double generateStochasticity();

    /**
     * @brief Calcule M_graph(t)
     */
    [[nodiscard]] double computeMemoryInfluence() const;

    /**
     * @brief Applique la fonction de sortie f()
     * @param raw_value Valeur brute
     * @return Valeur normalisée
     */
    [[nodiscard]] double applyOutputFunction(double raw_value) const;

    /**
     * @brief Met à jour les poids adaptatifs
     * @param wisdom Sagesse Wt
     */
    void adaptWeights(double wisdom);

    /**
     * @brief Trouve la variable dominante
     */
    void findDominantVariable();

    /**
     * @brief Met à jour l'historique
     */
    void updateHistory(double goal);

    /**
     * @brief Applique le mapping émotions → variables
     * @param emotional_state État émotionnel (24 émotions)
     *
     * Pour chaque émotion active, modifie les variables selon la matrice
     * EmotionVariableMapping. L'influence est pondérée par l'intensité.
     */
    void applyEmotionMapping(const EmotionalState& emotional_state);

    /**
     * @brief Met à jour M_graph(t) depuis le MCTGraph connecté
     *
     * Extrait les associations causales récentes du graphe pour calculer:
     * - S⁺(t) : somme pondérée des émotions positives associées aux mots
     * - S⁻(t) : somme pondérée des émotions négatives
     * - T_trauma : intensité des associations traumatiques
     */
    void updateFromMCTGraph();
};

} // namespace mcee
