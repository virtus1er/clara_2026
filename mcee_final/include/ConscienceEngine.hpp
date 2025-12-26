#pragma once

#include "ConscienceConfig.hpp"
#include "Types.hpp"
#include <memory>
#include <mutex>
#include <deque>
#include <chrono>

namespace mcee {

/**
 * ═══════════════════════════════════════════════════════════════════════════
 * ConscienceEngine - Module de Conscience et Sentiments
 *
 * Calcule le niveau de conscience Ct et le sentiment global Ft selon :
 *
 * Conscience : Ct = (Σ αi·Ei(t) + Mtotal(t) + Tdominant(t) + β·Fbt + δ·Ent) × Wt
 * Sentiment  : Ft = tanh(Σ γk·Ck + λ·Fbt)
 *
 * Où :
 *   - Ei(t)    : 24 émotions normalisées
 *   - Mtotal   : Activation mémoire totale
 *   - Tdominant: Trauma dominant (pondéré par ω = 5.0)
 *   - Fbt      : Feedback combiné
 *   - Ent      : Score environnemental
 *   - Wt       : Sagesse (croissance logarithmique)
 *   - γk       : Coefficients de sentiment par composante
 * ═══════════════════════════════════════════════════════════════════════════
 */
class ConscienceEngine {
public:
    explicit ConscienceEngine(const ConscienceConfig& config = ConscienceConfig{});
    ~ConscienceEngine() = default;

    // Non-copiable, déplaçable
    ConscienceEngine(const ConscienceEngine&) = delete;
    ConscienceEngine& operator=(const ConscienceEngine&) = delete;
    ConscienceEngine(ConscienceEngine&&) = default;
    ConscienceEngine& operator=(ConscienceEngine&&) = default;

    // ═══════════════════════════════════════════════════════════
    // MISE À JOUR PRINCIPALE
    // ═══════════════════════════════════════════════════════════

    /**
     * Met à jour l'état de conscience avec les entrées actuelles
     * @return État de conscience et sentiment calculés
     */
    ConscienceSentimentState update(
        const EmotionalState& emotions,
        const std::vector<MemoryActivation>& memories,
        const FeedbackState& feedback,
        const EnvironmentState& environment
    );

    /**
     * Met à jour uniquement avec les émotions (mode simplifié)
     */
    ConscienceSentimentState updateSimple(const EmotionalState& emotions);

    // ═══════════════════════════════════════════════════════════
    // GESTION DES TRAUMAS
    // ═══════════════════════════════════════════════════════════

    /**
     * Signale un trauma actif
     */
    void activateTrauma(const TraumaState& trauma);

    /**
     * Désactive un trauma
     */
    void deactivateTrauma(const std::string& trauma_id);

    /**
     * Récupère le trauma dominant actuel
     */
    [[nodiscard]] std::optional<TraumaState> getDominantTrauma() const;

    // ═══════════════════════════════════════════════════════════
    // MODULATION MLT
    // ═══════════════════════════════════════════════════════════

    /**
     * Permet à MLT de moduler les coefficients émotionnels αi
     */
    void modulateEmotionCoefficients(const std::array<double, 24>& new_alphas);

    /**
     * Récupère les coefficients émotionnels actuels
     */
    [[nodiscard]] const std::array<double, 24>& getEmotionCoefficients() const;

    // ═══════════════════════════════════════════════════════════
    // SAGESSE (Wt)
    // ═══════════════════════════════════════════════════════════

    /**
     * Ajoute de l'expérience pour augmenter la sagesse
     * Wt = wisdom_base + wisdom_growth_rate * log(1 + experience)
     */
    void addExperience(double amount);

    /**
     * Applique la décroissance de la sagesse pour éviter la rigidité
     * Doit être appelé périodiquement (ex: chaque cycle)
     */
    void applyWisdomDecay();

    /**
     * Récupère le niveau de sagesse actuel
     */
    [[nodiscard]] double getWisdom() const;

    /**
     * Récupère l'expérience accumulée
     */
    [[nodiscard]] double getExperience() const;

    // ═══════════════════════════════════════════════════════════
    // ACCÈS À L'ÉTAT
    // ═══════════════════════════════════════════════════════════

    /**
     * Récupère le dernier état calculé
     */
    [[nodiscard]] ConscienceSentimentState getCurrentState() const;

    /**
     * Récupère l'historique du sentiment (fond affectif)
     */
    [[nodiscard]] double getSentimentMovingAverage() const;

    /**
     * Récupère la configuration
     */
    [[nodiscard]] const ConscienceConfig& getConfig() const { return config_; }

    // ═══════════════════════════════════════════════════════════
    // CALLBACKS
    // ═══════════════════════════════════════════════════════════

    void setUpdateCallback(ConscienceUpdateCallback callback);
    void setTraumaAlertCallback(TraumaAlertCallback callback);
    void setMLTModulationCallback(MLTModulationCallback callback);

private:
    ConscienceConfig config_;

    // État courant
    ConscienceSentimentState current_state_;

    // Traumas actifs
    std::vector<TraumaState> active_traumas_;

    // Historique du sentiment pour moyenne mobile
    std::deque<double> sentiment_history_;
    double sentiment_ema_ = 0.0;  // Exponential moving average

    // Sagesse et expérience
    double experience_ = 0.0;
    double wisdom_ = 1.0;

    // Callbacks
    ConscienceUpdateCallback update_callback_;
    TraumaAlertCallback trauma_callback_;
    MLTModulationCallback mlt_callback_;

    // Thread safety
    mutable std::mutex mutex_;

    // ═══════════════════════════════════════════════════════════
    // CALCULS INTERNES
    // ═══════════════════════════════════════════════════════════

    /**
     * Calcule la contribution émotionnelle Σ αi·Ei(t)
     */
    double computeEmotionalContribution(const EmotionalState& emotions) const;

    /**
     * Calcule la contribution mémoire Mtotal(t)
     */
    double computeMemoryContribution(const std::vector<MemoryActivation>& memories) const;

    /**
     * Calcule la contribution trauma Tdominant(t)
     */
    double computeTraumaContribution() const;

    /**
     * Calcule le niveau de conscience Ct
     */
    double computeConsciousness(
        double emotional_contrib,
        double memory_contrib,
        double trauma_contrib,
        double feedback_contrib,
        double environment_contrib
    ) const;

    /**
     * Calcule le sentiment Ft = tanh(Σ γk·Ck + λ·Fbt)
     */
    double computeSentiment(
        double emotional_contrib,
        double memory_contrib,
        double feedback_value
    ) const;

    /**
     * Met à jour la moyenne mobile exponentielle du sentiment
     */
    void updateSentimentEMA(double new_sentiment);

    /**
     * Détermine l'état dominant (positive/negative/neutral)
     */
    std::string determineDominantState(double consciousness, double sentiment) const;
};

} // namespace mcee
