/**
 * @file EmotionUpdater.hpp
 * @brief Mise à jour des émotions selon la formule MCEE avec coefficients de phase
 * @version 2.0
 * @date 2025-12-19
 * 
 * Formule principale:
 * E_i(t+1) = E_i(t) + α·Fb_ext + β·Fb_int - γ·Δt + δ·IS + θ·Wt
 * 
 * Où:
 * - α, β, γ, δ, θ sont les coefficients de la phase active
 * - Fb_ext: Feedback externe
 * - Fb_int: Feedback interne  
 * - Δt: Delta temps (décroissance naturelle)
 * - IS: Influence des souvenirs
 * - Wt: Coefficient de sagesse
 */

#ifndef MCEE_EMOTION_UPDATER_HPP
#define MCEE_EMOTION_UPDATER_HPP

#include "Types.hpp"
#include "PhaseConfig.hpp"
#include <array>
#include <vector>

namespace mcee {

/**
 * @class EmotionUpdater
 * @brief Met à jour les 24 émotions selon la formule MCEE avec les coefficients de phase
 */
class EmotionUpdater {
public:
    /**
     * @brief Constructeur avec coefficients par défaut (SÉRÉNITÉ)
     */
    EmotionUpdater();

    /**
     * @brief Met à jour les coefficients selon la phase active
     * @param config Configuration de la phase
     */
    void setCoefficientsFromPhase(const PhaseConfig& config);

    /**
     * @brief Met à jour une seule émotion
     * @param E_current Valeur actuelle de l'émotion
     * @param fb_ext Feedback externe
     * @param fb_int Feedback interne
     * @param delta_t Delta temps (décroissance)
     * @param influence_memories Influence des souvenirs
     * @param wisdom Coefficient de sagesse
     * @return Nouvelle valeur de l'émotion [0, 1]
     */
    [[nodiscard]] double updateEmotion(
        double E_current,
        double fb_ext,
        double fb_int,
        double delta_t,
        double influence_memories,
        double wisdom
    ) const;

    /**
     * @brief Met à jour toutes les émotions d'un état
     * @param state État émotionnel à mettre à jour
     * @param feedback Feedbacks externe et interne
     * @param delta_t Delta temps depuis dernière mise à jour
     * @param memory_influences Influences des souvenirs par émotion
     * @param wisdom Coefficient de sagesse global
     */
    void updateAllEmotions(
        EmotionalState& state,
        const Feedback& feedback,
        double delta_t,
        const std::array<double, NUM_EMOTIONS>& memory_influences,
        double wisdom
    );

    /**
     * @brief Calcule la variance d'une émotion par rapport aux souvenirs
     * @param E_current Valeur actuelle de l'émotion
     * @param memory_values Valeurs de l'émotion dans les souvenirs
     * @return Variance calculée
     * 
     * Formule: Var_i(t) = (1/m) × Σ[E_i(t) - S_i,j]²
     */
    [[nodiscard]] double computeVariance(
        double E_current,
        const std::vector<double>& memory_values
    ) const;

    /**
     * @brief Calcule la variance globale de l'état émotionnel
     * @param state État émotionnel actuel
     * @param memories Vecteur de souvenirs
     * @return Variance globale
     */
    [[nodiscard]] double computeGlobalVariance(
        const EmotionalState& state,
        const std::vector<Memory>& memories
    ) const;

    /**
     * @brief Fusionne les émotions pour calculer E_global
     * @param state État émotionnel
     * @param E_global_prev E_global précédent
     * @param variance_global Variance globale
     * @return Nouveau E_global
     * 
     * Formule: E_global(t+1) = tanh(E_global(t) + Σ[E_i(t+1) × (1 - Var_global)])
     */
    [[nodiscard]] double computeEGlobal(
        const EmotionalState& state,
        double E_global_prev,
        double variance_global
    ) const;

    // Getters pour les coefficients actuels
    [[nodiscard]] double getAlpha() const { return alpha_; }
    [[nodiscard]] double getBeta() const { return beta_; }
    [[nodiscard]] double getGamma() const { return gamma_; }
    [[nodiscard]] double getDelta() const { return delta_; }
    [[nodiscard]] double getTheta() const { return theta_; }

    // Mode silencieux
    void setQuietMode(bool quiet) { quiet_mode_ = quiet; }
    [[nodiscard]] bool isQuietMode() const { return quiet_mode_; }

private:
    // Coefficients dynamiques selon la phase
    double alpha_;  // Coefficient feedback externe
    double beta_;   // Coefficient feedback interne
    double gamma_;  // Coefficient décroissance
    double delta_;  // Coefficient influence souvenirs
    double theta_;  // Coefficient sagesse
    bool quiet_mode_ = false;  // Mode silencieux
};

} // namespace mcee

#endif // MCEE_EMOTION_UPDATER_HPP
