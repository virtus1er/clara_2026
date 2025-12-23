#pragma once

#include "ConscienceConfig.hpp"

#include <array>
#include <deque>
#include <mutex>
#include <functional>
#include <chrono>
#include <optional>
#include <string>

namespace MCEE {

/**
 * Résultat du calcul de Conscience à un instant t
 */
struct ConscienceState {
    double Ct = 0.0;                     // Conscience globale
    double emotion_component = 0.0;      // Σ αi·Ei(t)
    double memory_component = 0.0;       // Mtotal(t)
    double trauma_component = 0.0;       // Tdominant(t)
    double feedback_component = 0.0;     // β·Fbt
    double environment_component = 0.0;  // δ·Ent
    double wisdom_factor = 1.0;          // Wt
    
    std::string active_pattern;          // Pattern émotionnel actif
    TraumaState dominant_trauma;         // Trauma dominant si présent
    
    std::chrono::steady_clock::time_point timestamp;
    
    [[nodiscard]] bool hasTrauma() const { return dominant_trauma.isActive(); }
};

/**
 * Résultat du calcul de Sentiment
 */
struct SentimentState {
    double Ft = 0.0;                     // Sentiment global normalisé [-1, 1]
    double Ft_raw = 0.0;                 // Sentiment avant normalisation
    double accumulated_conscience = 0.0; // Σ γk·Ck
    double feedback_influence = 0.0;     // λ·Fbt
    
    size_t history_depth = 0;            // Profondeur historique utilisée
    
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * État complet Conscience + Sentiments
 */
struct ConscienceSentimentState {
    ConscienceState conscience;
    SentimentState sentiment;
    
    // "Fond affectif" stable
    double affective_background = 0.0;
    
    // Timestamp du calcul
    std::chrono::steady_clock::time_point timestamp;
    
    [[nodiscard]] double getOverallState() const {
        return (conscience.Ct + sentiment.Ft) / 2.0;
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════════

using ConscienceUpdateCallback = std::function<void(const ConscienceState&)>;
using SentimentUpdateCallback = std::function<void(const SentimentState&)>;
using TraumaActivationCallback = std::function<void(const TraumaState&)>;
using AffectiveBackgroundCallback = std::function<void(double)>;

/**
 * ConscienceEngine - Moteur de calcul Conscience & Sentiments
 * 
 * Implémente les équations du document "Conscience et Sentiments Version Ajustée" :
 * 
 * Conscience : Ct = (Σ αi·Ei(t) + Mtotal(t) + Tdominant(t) + β·Fbt + δ·Ent) × Wt
 * Sentiments : Ft = tanh(Σ γk·Ck + λ·Fbt)
 * 
 * Intégration avec Mémoire Graphe pour Mtotal et gestion prioritaire des traumas.
 */
class ConscienceEngine {
public:
    // ═══════════════════════════════════════════════════════════════════════════
    // CONSTRUCTEUR / DESTRUCTEUR
    // ═══════════════════════════════════════════════════════════════════════════
    
    explicit ConscienceEngine(const ConscienceConfig& config = ConscienceConfig{});
    ~ConscienceEngine() = default;
    
    // Non-copiable
    ConscienceEngine(const ConscienceEngine&) = delete;
    ConscienceEngine& operator=(const ConscienceEngine&) = delete;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // MISE À JOUR DES ENTRÉES
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Met à jour les émotions instantanées (depuis Module Émotionnel)
     * @param emotions 24 valeurs émotionnelles normalisées [0, 1]
     * @param active_pattern Pattern émotionnel actif (SERENITE, JOIE, etc.)
     */
    void updateEmotions(const std::array<double, 24>& emotions,
                        const std::string& active_pattern);
    
    /**
     * Met à jour l'activation des mémoires (depuis Mémoire Graphe)
     * @param activation Scores d'activation par type de mémoire
     */
    void updateMemoryActivation(const MemoryActivation& activation);
    
    /**
     * Met à jour l'état de trauma (depuis Amyghaleon ou Graphe)
     * @param trauma État du trauma actif
     */
    void updateTrauma(const TraumaState& trauma);
    
    /**
     * Met à jour le feedback externe
     * @param feedback État du feedback reçu
     */
    void updateFeedback(const FeedbackState& feedback);
    
    /**
     * Met à jour l'état de l'environnement
     * @param environment État environnemental
     */
    void updateEnvironment(const EnvironmentState& environment);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // CALCUL CONSCIENCE & SENTIMENTS
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Calcule la conscience à l'instant courant
     * @return État de conscience calculé
     */
    [[nodiscard]] ConscienceState computeConscience();
    
    /**
     * Calcule le sentiment basé sur l'historique de conscience
     * @return État de sentiment calculé
     */
    [[nodiscard]] SentimentState computeSentiment();
    
    /**
     * Calcule conscience + sentiment en une seule opération
     * @return État complet
     */
    [[nodiscard]] ConscienceSentimentState compute();
    
    /**
     * Tick de mise à jour complète (à appeler périodiquement)
     * Calcule conscience, sentiment, met à jour l'historique
     */
    void tick();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // ACCÈS À L'ÉTAT
    // ═══════════════════════════════════════════════════════════════════════════
    
    [[nodiscard]] const ConscienceState& getLastConscience() const;
    [[nodiscard]] const SentimentState& getLastSentiment() const;
    [[nodiscard]] double getAffectiveBackground() const;
    [[nodiscard]] double getWisdom() const;
    [[nodiscard]] const ConscienceConfig& getConfig() const;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // GESTION SAGESSE (Wt)
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Incrémente la sagesse suite à une expérience
     * @param experience_value Valeur de l'expérience (positive ou négative)
     */
    void addExperience(double experience_value);
    
    /**
     * Réinitialise la sagesse à sa valeur initiale
     */
    void resetWisdom();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // COEFFICIENTS DYNAMIQUES (modulation par MLT)
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Modifie dynamiquement les coefficients alpha des émotions
     * Appelé par MLT pour pondérer selon souvenirs positifs/négatifs
     */
    void modulateEmotionCoefficients(const std::array<double, 24>& modulation);
    
    /**
     * Modifie les coefficients omega des mémoires
     */
    void modulateMemoryCoefficients(double omega_MCT, double omega_MLT, 
                                     double omega_MP, double omega_ME,
                                     double omega_MS, double omega_MA);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // CALLBACKS
    // ═══════════════════════════════════════════════════════════════════════════
    
    void setConscienceUpdateCallback(ConscienceUpdateCallback callback);
    void setSentimentUpdateCallback(SentimentUpdateCallback callback);
    void setTraumaActivationCallback(TraumaActivationCallback callback);
    void setAffectiveBackgroundCallback(AffectiveBackgroundCallback callback);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // CONFIGURATION
    // ═══════════════════════════════════════════════════════════════════════════
    
    void setConfig(const ConscienceConfig& config);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // SÉRIALISATION
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * Exporte l'état complet en JSON
     */
    [[nodiscard]] std::string toJson() const;
    
    /**
     * Charge un état depuis JSON
     */
    bool fromJson(const std::string& json);

private:
    // ═══════════════════════════════════════════════════════════════════════════
    // MEMBRES
    // ═══════════════════════════════════════════════════════════════════════════
    
    ConscienceConfig config_;
    mutable std::mutex mutex_;
    
    // Entrées courantes
    std::array<double, 24> current_emotions_{};
    std::string active_pattern_;
    MemoryActivation memory_activation_;
    TraumaState current_trauma_;
    FeedbackState current_feedback_;
    EnvironmentState current_environment_;
    
    // Coefficients dynamiques (modifiables par MLT)
    std::array<double, 24> alpha_modulation_{};
    
    // État calculé
    ConscienceState last_conscience_;
    SentimentState last_sentiment_;
    double affective_background_ = 0.0;
    double wisdom_ = 1.0;
    
    // Historique pour calcul sentiments (fenêtre glissante)
    std::deque<double> conscience_history_;
    
    // Callbacks
    ConscienceUpdateCallback conscience_callback_;
    SentimentUpdateCallback sentiment_callback_;
    TraumaActivationCallback trauma_callback_;
    AffectiveBackgroundCallback affective_callback_;
    
    // Timestamps
    std::chrono::steady_clock::time_point last_update_;
    std::chrono::steady_clock::time_point start_time_;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // MÉTHODES PRIVÉES
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Calcul des composantes de Ct
    [[nodiscard]] double computeEmotionComponent() const;
    [[nodiscard]] double computeMemoryComponent() const;
    [[nodiscard]] double computeTraumaComponent() const;
    [[nodiscard]] double computeFeedbackComponent() const;
    [[nodiscard]] double computeEnvironmentComponent() const;
    
    // Mise à jour fond affectif
    void updateAffectiveBackground(double new_sentiment);
    
    // Mise à jour historique conscience
    void addToConscienceHistory(double Ct);
    
    // Calcul somme pondérée historique
    [[nodiscard]] double computeWeightedHistory() const;
};

} // namespace MCEE
