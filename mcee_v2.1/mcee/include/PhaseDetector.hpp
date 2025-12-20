/**
 * @file PhaseDetector.hpp
 * @brief Détecteur de phases émotionnelles pour le MCEE
 * @version 2.0
 * @date 2025-12-19
 */

#ifndef MCEE_PHASE_DETECTOR_HPP
#define MCEE_PHASE_DETECTOR_HPP

#include "Types.hpp"
#include "PhaseConfig.hpp"
#include <chrono>
#include <unordered_map>
#include <functional>

namespace mcee {

/**
 * @class PhaseDetector
 * @brief Détecte et gère les transitions entre phases émotionnelles
 * 
 * Le détecteur analyse les 24 émotions et détermine la phase active.
 * Il gère:
 * - L'hystérésis pour éviter les oscillations
 * - La durée minimale dans une phase
 * - Les transitions d'urgence vers PEUR
 */
class PhaseDetector {
public:
    using TransitionCallback = std::function<void(Phase from, Phase to, double duration)>;

    /**
     * @brief Constructeur
     * @param hysteresis_margin Marge d'hystérésis (défaut: 0.15)
     * @param min_phase_duration Durée minimale en phase (secondes, défaut: 30)
     */
    explicit PhaseDetector(
        double hysteresis_margin = DEFAULT_HYSTERESIS_MARGIN,
        double min_phase_duration = DEFAULT_MIN_PHASE_DURATION
    );

    /**
     * @brief Détecte la phase actuelle selon les émotions
     * @param state État émotionnel avec 24 émotions
     * @return Phase détectée
     */
    Phase detectPhase(const EmotionalState& state);

    /**
     * @brief Retourne la configuration de la phase actuelle
     * @return PhaseConfig avec les coefficients
     */
    [[nodiscard]] const PhaseConfig& getCurrentConfig() const;

    /**
     * @brief Retourne la phase actuelle
     */
    [[nodiscard]] Phase getCurrentPhase() const { return current_phase_; }

    /**
     * @brief Retourne la phase précédente
     */
    [[nodiscard]] Phase getPreviousPhase() const { return previous_phase_; }

    /**
     * @brief Retourne la durée dans la phase actuelle (secondes)
     */
    [[nodiscard]] double getPhaseDuration() const;

    /**
     * @brief Force une transition vers une phase spécifique
     * @param phase Phase cible
     * @param reason Raison de la transition forcée
     */
    void forceTransition(Phase phase, const std::string& reason = "MANUAL");

    /**
     * @brief Définit un callback pour les transitions
     * @param callback Fonction appelée à chaque transition
     */
    void setTransitionCallback(TransitionCallback callback);

    /**
     * @brief Charge les configurations depuis un fichier JSON
     * @param config_path Chemin vers phase_config.json
     * @return true si chargement réussi
     */
    bool loadConfig(const std::string& config_path);

    /**
     * @brief Met à jour les paramètres du détecteur
     */
    void setHysteresisMargin(double margin) { hysteresis_margin_ = margin; }
    void setMinPhaseDuration(double duration) { min_phase_duration_s_ = duration; }

    /**
     * @brief Retourne les statistiques de détection
     */
    [[nodiscard]] size_t getTransitionCount() const { return transition_count_; }

private:
    // État
    Phase current_phase_ = Phase::SERENITE;
    Phase previous_phase_ = Phase::SERENITE;
    std::chrono::steady_clock::time_point phase_start_time_;
    
    // Configuration
    double hysteresis_margin_;
    double min_phase_duration_s_;
    std::unordered_map<Phase, PhaseConfig> phase_configs_;
    
    // Statistiques
    size_t transition_count_ = 0;
    
    // Callback
    TransitionCallback on_transition_;

    /**
     * @brief Calcule les scores pour chaque phase
     * @param state État émotionnel
     * @return Map phase -> score
     */
    std::unordered_map<Phase, double> computePhaseScores(const EmotionalState& state) const;

    /**
     * @brief Vérifie si une transition d'urgence est nécessaire
     * @param state État émotionnel
     * @return true si urgence détectée
     */
    bool checkEmergencyTransition(const EmotionalState& state) const;

    /**
     * @brief Effectue la transition vers une nouvelle phase
     * @param new_phase Phase cible
     * @param reason Raison de la transition
     */
    void transitionTo(Phase new_phase, const std::string& reason);

    /**
     * @brief Vérifie si la durée minimale est respectée
     */
    [[nodiscard]] bool canTransition() const;

    /**
     * @brief Applique l'hystérésis pour éviter les oscillations
     * @param scores Scores des phases
     * @param best_phase Meilleure phase candidate
     * @return true si transition autorisée
     */
    bool applyHysteresis(
        const std::unordered_map<Phase, double>& scores,
        Phase best_phase
    ) const;
};

} // namespace mcee

#endif // MCEE_PHASE_DETECTOR_HPP
