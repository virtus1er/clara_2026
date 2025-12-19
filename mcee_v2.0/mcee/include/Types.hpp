/**
 * @file Types.hpp
 * @brief Types et structures de données pour le MCEE v2.0
 * @version 2.0
 * @date 2025-12-19
 */

#ifndef MCEE_TYPES_HPP
#define MCEE_TYPES_HPP

#include <array>
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <cmath>

namespace mcee {

// Constantes du système
constexpr size_t NUM_EMOTIONS = 24;
constexpr double DEFAULT_HYSTERESIS_MARGIN = 0.15;
constexpr double DEFAULT_MIN_PHASE_DURATION = 30.0; // secondes

/**
 * @brief Liste ordonnée des 24 émotions
 */
inline const std::array<std::string, NUM_EMOTIONS> EMOTION_NAMES = {
    "Admiration", "Adoration", "Appréciation esthétique", "Amusement",
    "Anxiété", "Émerveillement", "Gêne", "Ennui",
    "Calme", "Confusion", "Dégoût", "Douleur empathique",
    "Fascination", "Excitation", "Peur", "Horreur",
    "Intérêt", "Joie", "Nostalgie", "Soulagement",
    "Tristesse", "Satisfaction", "Sympathie", "Triomphe"
};

/**
 * @brief Les 8 phases émotionnelles du système
 */
enum class Phase {
    SERENITE,
    JOIE,
    EXPLORATION,
    ANXIETE,
    PEUR,
    TRISTESSE,
    DEGOUT,
    CONFUSION
};

/**
 * @brief Convertit une Phase en chaîne de caractères
 */
inline std::string phaseToString(Phase phase) {
    switch (phase) {
        case Phase::SERENITE:    return "SERENITE";
        case Phase::JOIE:        return "JOIE";
        case Phase::EXPLORATION: return "EXPLORATION";
        case Phase::ANXIETE:     return "ANXIETE";
        case Phase::PEUR:        return "PEUR";
        case Phase::TRISTESSE:   return "TRISTESSE";
        case Phase::DEGOUT:      return "DEGOUT";
        case Phase::CONFUSION:   return "CONFUSION";
        default:                 return "UNKNOWN";
    }
}

/**
 * @brief Convertit une chaîne en Phase
 */
inline Phase stringToPhase(const std::string& str) {
    static const std::unordered_map<std::string, Phase> phaseMap = {
        {"SERENITE", Phase::SERENITE},
        {"JOIE", Phase::JOIE},
        {"EXPLORATION", Phase::EXPLORATION},
        {"ANXIETE", Phase::ANXIETE},
        {"PEUR", Phase::PEUR},
        {"TRISTESSE", Phase::TRISTESSE},
        {"DEGOUT", Phase::DEGOUT},
        {"CONFUSION", Phase::CONFUSION}
    };
    auto it = phaseMap.find(str);
    return (it != phaseMap.end()) ? it->second : Phase::SERENITE;
}

/**
 * @brief Configuration d'une phase émotionnelle
 */
struct PhaseConfig {
    double alpha;                  // Coefficient feedback externe
    double beta;                   // Coefficient feedback interne
    double gamma;                  // Coefficient décroissance
    double delta;                  // Coefficient influence souvenirs
    double theta;                  // Coefficient sagesse
    double amyghaleon_threshold;   // Seuil d'activation Amyghaleon
    double memory_consolidation;   // Force de consolidation mémoire
    double learning_rate;          // Taux d'apprentissage
    double focus;                  // Focus attentionnel
    int priority;                  // Priorité de la phase (1-5)
};

/**
 * @brief État émotionnel complet (24 émotions)
 */
struct EmotionalState {
    std::array<double, NUM_EMOTIONS> emotions{};
    double E_global = 0.0;
    double variance_global = 0.0;
    std::chrono::steady_clock::time_point timestamp;
    
    EmotionalState() : timestamp(std::chrono::steady_clock::now()) {
        emotions.fill(0.0);
    }
    
    /**
     * @brief Accès à une émotion par son nom
     */
    double getEmotion(const std::string& name) const {
        for (size_t i = 0; i < NUM_EMOTIONS; ++i) {
            if (EMOTION_NAMES[i] == name) {
                return emotions[i];
            }
        }
        return 0.0;
    }
    
    /**
     * @brief Définir une émotion par son nom
     */
    void setEmotion(const std::string& name, double value) {
        for (size_t i = 0; i < NUM_EMOTIONS; ++i) {
            if (EMOTION_NAMES[i] == name) {
                emotions[i] = std::clamp(value, 0.0, 1.0);
                return;
            }
        }
    }
    
    /**
     * @brief Trouve l'émotion dominante
     */
    std::pair<std::string, double> getDominant() const {
        size_t maxIdx = 0;
        double maxVal = emotions[0];
        for (size_t i = 1; i < NUM_EMOTIONS; ++i) {
            if (emotions[i] > maxVal) {
                maxVal = emotions[i];
                maxIdx = i;
            }
        }
        return {EMOTION_NAMES[maxIdx], maxVal};
    }
    
    /**
     * @brief Calcule l'intensité moyenne
     */
    double getMeanIntensity() const {
        double sum = 0.0;
        for (const auto& e : emotions) {
            sum += e;
        }
        return sum / NUM_EMOTIONS;
    }
    
    /**
     * @brief Calcule la valence (positif vs négatif)
     */
    double getValence() const {
        // Émotions positives: indices 0-3, 5, 8, 12, 13, 16, 17, 19, 21, 22, 23
        // Émotions négatives: indices 4, 6, 7, 9, 10, 11, 14, 15, 18, 20
        static const std::array<size_t, 14> positiveIndices = {0, 1, 2, 3, 5, 8, 12, 13, 16, 17, 19, 21, 22, 23};
        static const std::array<size_t, 10> negativeIndices = {4, 6, 7, 9, 10, 11, 14, 15, 18, 20};
        
        double posSum = 0.0, negSum = 0.0;
        for (auto i : positiveIndices) posSum += emotions[i];
        for (auto i : negativeIndices) negSum += emotions[i];
        
        double total = posSum + negSum;
        if (total < 1e-6) return 0.5;
        return posSum / total;
    }
};

/**
 * @brief Feedback externe et interne
 */
struct Feedback {
    double external = 0.0;  // Fb_ext: feedback environnement/utilisateur
    double internal = 0.0;  // Fb_int: feedback interne (énergie, tension)
};

/**
 * @brief Réponse d'urgence Amyghaleon
 */
struct EmergencyResponse {
    bool triggered = false;
    std::string action;           // FUITE, BLOCAGE, ALERTE, SURVEILLANCE
    std::string priority;         // CRITIQUE, ELEVEE, MOYENNE
    Phase phase_at_trigger;
    std::string trigger_emotion;
    double emotion_value = 0.0;
};

/**
 * @brief Souvenir pour l'influence mémoire
 */
struct Memory {
    std::string name;
    std::array<double, NUM_EMOTIONS> emotions{};
    std::string dominant;
    double valence = 0.0;
    double intensity = 0.0;
    double weight = 0.5;
    double activation = 0.0;
    bool is_trauma = false;
    Phase phase_at_creation = Phase::SERENITE;
    std::chrono::system_clock::time_point last_activated;
    int activation_count = 0;
};

/**
 * @brief Statistiques du moteur MCEE
 */
struct MCEEStats {
    Phase current_phase = Phase::SERENITE;
    Phase previous_phase = Phase::SERENITE;
    double phase_duration = 0.0;
    size_t phase_transitions = 0;
    size_t emergency_triggers = 0;
    double wisdom = 0.0;
    std::chrono::steady_clock::time_point start_time;
    
    MCEEStats() : start_time(std::chrono::steady_clock::now()) {}
};

} // namespace mcee

#endif // MCEE_TYPES_HPP
