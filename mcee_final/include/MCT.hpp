/**
 * @file MCT.hpp
 * @brief Mémoire Court Terme (MCT) - Buffer temporel des états émotionnels
 * 
 * La MCT agit comme un buffer glissant qui maintient les états émotionnels
 * récents. Elle permet de calculer un état intégré et de comparer avec
 * les patterns en MLT.
 * 
 * @version 3.0
 * @date 2024
 */

#pragma once

#include "Types.hpp"
#include <nlohmann/json.hpp>
#include <deque>
#include <vector>
#include <chrono>
#include <mutex>
#include <functional>
#include <optional>

namespace mcee {

/**
 * @brief État émotionnel horodaté pour la MCT
 */
struct TimestampedState {
    EmotionalState state;
    std::chrono::steady_clock::time_point timestamp;
    double speech_sentiment{0.0};      // Sentiment associé (si parole)
    double speech_arousal{0.0};        // Arousal associé
    std::string context;               // Contexte textuel optionnel
    
    TimestampedState() : timestamp(std::chrono::steady_clock::now()) {}
    TimestampedState(const EmotionalState& s) 
        : state(s), timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Résultat de l'intégration MCT
 */
struct MCTIntegration {
    EmotionalState integrated_state;   // État intégré (moyenne pondérée)
    double stability;                  // Stabilité temporelle [0, 1]
    double volatility;                 // Volatilité (changements rapides) [0, 1]
    double trend;                      // Tendance (-1 négatif, +1 positif)
    std::array<double, 24> emotion_velocity;  // Vitesse de changement par émotion
    size_t sample_count;               // Nombre d'échantillons utilisés
    double time_span_seconds;          // Durée couverte
};

/**
 * @brief Signature émotionnelle pour comparaison avec patterns
 */
struct EmotionalSignature {
    std::array<double, 24> mean_emotions;      // Moyenne des émotions
    std::array<double, 24> std_dev;            // Écart-type par émotion
    std::array<double, 24> trend;              // Tendance par émotion
    double global_intensity;                    // Intensité globale moyenne
    double global_valence;                      // Valence globale
    double global_arousal;                      // Arousal global
    double stability;                           // Stabilité de la signature
};

/**
 * @brief Configuration de la MCT
 */
struct MCTConfig {
    size_t max_size{60};                       // Taille max du buffer (états)
    double time_window_seconds{30.0};          // Fenêtre temporelle (secondes)
    double decay_factor{0.95};                 // Facteur de décroissance temporelle
    double stability_threshold{0.1};           // Seuil pour considérer stable
    double volatility_threshold{0.3};          // Seuil pour considérer volatile
    bool use_exponential_weighting{true};      // Pondération exponentielle
    double min_samples_for_signature{5};       // Min échantillons pour signature
};

/**
 * @class MCT
 * @brief Mémoire Court Terme - Buffer temporel des états émotionnels
 * 
 * La MCT maintient une fenêtre glissante des états émotionnels récents.
 * Elle permet de :
 * - Stocker les états horodatés
 * - Calculer un état intégré (moyenne pondérée temporellement)
 * - Extraire une signature émotionnelle pour comparaison MLT
 * - Analyser la stabilité et les tendances
 */
class MCT {
public:
    MCT();
    explicit MCT(const MCTConfig& config);
    
    // ═══════════════════════════════════════════════════════════════
    // GESTION DU BUFFER
    // ═══════════════════════════════════════════════════════════════
    
    /**
     * @brief Ajoute un nouvel état au buffer
     * @param state État émotionnel à ajouter
     */
    void push(const EmotionalState& state);
    
    /**
     * @brief Ajoute un état avec contexte de parole
     */
    void pushWithSpeech(const EmotionalState& state, 
                        double sentiment, 
                        double arousal,
                        const std::string& context = "");
    
    /**
     * @brief Vide le buffer
     */
    void clear();
    
    /**
     * @brief Nettoie les états trop anciens
     */
    void pruneOld();
    
    // ═══════════════════════════════════════════════════════════════
    // INTÉGRATION ET ANALYSE
    // ═══════════════════════════════════════════════════════════════
    
    /**
     * @brief Calcule l'état intégré (moyenne pondérée temporellement)
     * @return État intégré avec métadonnées
     */
    MCTIntegration integrate() const;
    
    /**
     * @brief Extrait la signature émotionnelle pour comparaison MLT
     * @return Signature si assez d'échantillons, nullopt sinon
     */
    std::optional<EmotionalSignature> extractSignature() const;
    
    /**
     * @brief Calcule la similarité avec une autre signature
     * @param other Signature à comparer
     * @return Similarité cosinus [0, 1]
     */
    double similarityWith(const EmotionalSignature& other) const;
    
    /**
     * @brief Obtient l'état le plus récent
     */
    std::optional<TimestampedState> getLatest() const;
    
    /**
     * @brief Obtient les N derniers états
     */
    std::vector<TimestampedState> getRecent(size_t n) const;
    
    // ═══════════════════════════════════════════════════════════════
    // MÉTRIQUES
    // ═══════════════════════════════════════════════════════════════
    
    /**
     * @brief Calcule la stabilité actuelle [0, 1]
     */
    double getStability() const;
    
    /**
     * @brief Calcule la volatilité actuelle [0, 1]
     */
    double getVolatility() const;
    
    /**
     * @brief Calcule la tendance globale [-1, +1]
     */
    double getTrend() const;
    
    /**
     * @brief Vérifie si le buffer contient assez de données
     */
    bool hasEnoughData() const;
    
    // ═══════════════════════════════════════════════════════════════
    // ACCESSEURS
    // ═══════════════════════════════════════════════════════════════
    
    size_t size() const;
    bool empty() const;
    const MCTConfig& getConfig() const { return config_; }
    void setConfig(const MCTConfig& config) { config_ = config; }
    
    // ═══════════════════════════════════════════════════════════════
    // CALLBACKS
    // ═══════════════════════════════════════════════════════════════
    
    using StabilityCallback = std::function<void(double stability, double volatility)>;
    using TrendCallback = std::function<void(double trend, const std::array<double, 24>& velocity)>;
    
    void setStabilityCallback(StabilityCallback cb) { stability_callback_ = std::move(cb); }
    void setTrendCallback(TrendCallback cb) { trend_callback_ = std::move(cb); }
    
    // ═══════════════════════════════════════════════════════════════
    // SÉRIALISATION
    // ═══════════════════════════════════════════════════════════════
    
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
    
private:
    MCTConfig config_;
    std::deque<TimestampedState> buffer_;
    mutable std::mutex mutex_;
    
    // Callbacks
    StabilityCallback stability_callback_;
    TrendCallback trend_callback_;
    
    // Cache pour optimisation
    mutable std::optional<MCTIntegration> cached_integration_;
    mutable bool cache_valid_{false};
    
    // Méthodes internes
    void invalidateCache();
    double computeWeight(const std::chrono::steady_clock::time_point& timestamp) const;
    std::array<double, 24> computeVelocity() const;
};

} // namespace mcee
