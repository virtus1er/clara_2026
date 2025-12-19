/**
 * @file PhaseConfig.hpp
 * @brief Configurations des 8 phases émotionnelles
 * @version 2.0
 * @date 2025-12-19
 */

#ifndef MCEE_PHASE_CONFIG_HPP
#define MCEE_PHASE_CONFIG_HPP

#include "Types.hpp"
#include <unordered_map>

namespace mcee {

/**
 * @brief Configurations par défaut des 8 phases émotionnelles
 * 
 * Chaque phase modifie:
 * - α (alpha): Coefficient feedback externe
 * - β (beta): Coefficient feedback interne  
 * - γ (gamma): Coefficient décroissance naturelle
 * - δ (delta): Coefficient influence souvenirs
 * - θ (theta): Coefficient sagesse
 * - Seuil Amyghaleon: Sensibilité aux urgences
 * - Consolidation: Force de mémorisation
 * - Learning rate: Vitesse d'adaptation
 * - Focus: Filtrage attentionnel
 */
inline const std::unordered_map<Phase, PhaseConfig> DEFAULT_PHASE_CONFIGS = {
    // SÉRÉNITÉ: Équilibre, apprentissage optimal
    {Phase::SERENITE, {
        .alpha = 0.25,                  // Feedback externe modéré
        .beta = 0.15,                   // Feedback interne bas
        .gamma = 0.12,                  // Décroissance normale
        .delta = 0.30,                  // Souvenirs modérés
        .theta = 0.10,                  // Sagesse active
        .amyghaleon_threshold = 0.85,   // Difficile à déclencher
        .memory_consolidation = 0.4,
        .learning_rate = 1.0,
        .focus = 0.5,
        .priority = 1
    }},
    
    // JOIE: Euphorie, renforcement positif
    {Phase::JOIE, {
        .alpha = 0.40,                  // Feedback externe élevé
        .beta = 0.25,                   // Feedback interne modéré
        .gamma = 0.08,                  // Décroissance lente
        .delta = 0.35,                  // Souvenirs positifs actifs
        .theta = 0.05,                  // Sagesse réduite (euphorie)
        .amyghaleon_threshold = 0.95,   // Très difficile à déclencher
        .memory_consolidation = 0.5,
        .learning_rate = 1.3,
        .focus = 0.3,
        .priority = 2
    }},
    
    // EXPLORATION: Apprentissage maximal, attention focalisée
    {Phase::EXPLORATION, {
        .alpha = 0.35,                  // Feedback externe élevé (perception)
        .beta = 0.10,                   // Feedback interne bas (focus externe)
        .gamma = 0.10,                  // Décroissance normale
        .delta = 0.25,                  // Souvenirs moins influents (nouveauté)
        .theta = 0.15,                  // Sagesse ÉLEVÉE (apprentissage)
        .amyghaleon_threshold = 0.80,   // Modéré
        .memory_consolidation = 0.6,
        .learning_rate = 1.5,           // Apprentissage MAXIMAL
        .focus = 0.8,
        .priority = 2
    }},
    
    // ANXIÉTÉ: Hypervigilance, biais négatif
    {Phase::ANXIETE, {
        .alpha = 0.40,                  // Feedback externe élevé
        .beta = 0.30,                   // Feedback interne élevé (stress)
        .gamma = 0.06,                  // Décroissance lente (persistant)
        .delta = 0.45,                  // Souvenirs anxiogènes activés
        .theta = 0.08,                  // Sagesse réduite
        .amyghaleon_threshold = 0.70,   // Facile à déclencher
        .memory_consolidation = 0.4,
        .learning_rate = 0.8,
        .focus = 0.6,
        .priority = 3
    }},
    
    // PEUR △!: URGENCE - Traumas dominants, réflexes
    {Phase::PEUR, {
        .alpha = 0.60,                  // Feedback externe MAXIMAL (danger!)
        .beta = 0.45,                   // Feedback interne ÉLEVÉ (stress)
        .gamma = 0.02,                  // Décroissance TRÈS LENTE (persistant)
        .delta = 0.70,                  // Souvenirs DOMINANTS (traumas)
        .theta = 0.02,                  // Sagesse QUASI ABSENTE (réflexes)
        .amyghaleon_threshold = 0.50,   // TRÈS FACILE à déclencher
        .memory_consolidation = 0.8,    // Consolidation forte (trauma)
        .learning_rate = 0.3,           // Pas d'apprentissage rationnel
        .focus = 0.95,                  // Focus MAXIMAL sur menace
        .priority = 5                   // PRIORITÉ MAXIMALE
    }},
    
    // TRISTESSE: Rumination, introspection
    {Phase::TRISTESSE, {
        .alpha = 0.20,                  // Feedback externe bas (repli)
        .beta = 0.40,                   // Feedback interne élevé (rumination)
        .gamma = 0.05,                  // Décroissance très lente
        .delta = 0.55,                  // Souvenirs nostalgiques actifs
        .theta = 0.12,                  // Sagesse modérée
        .amyghaleon_threshold = 0.90,   // Difficile (état dépressif)
        .memory_consolidation = 0.5,
        .learning_rate = 0.6,
        .focus = 0.4,
        .priority = 3
    }},
    
    // DÉGOÛT: Évitement, associations négatives
    {Phase::DEGOUT, {
        .alpha = 0.50,                  // Feedback externe élevé (réactif)
        .beta = 0.25,                   // Feedback interne modéré
        .gamma = 0.08,                  // Décroissance normale
        .delta = 0.40,                  // Souvenirs associés
        .theta = 0.08,                  // Sagesse réduite
        .amyghaleon_threshold = 0.75,   // Modéré
        .memory_consolidation = 0.6,
        .learning_rate = 0.9,
        .focus = 0.7,
        .priority = 4
    }},
    
    // CONFUSION: Recherche d'info, incertitude
    {Phase::CONFUSION, {
        .alpha = 0.35,                  // Feedback externe modéré
        .beta = 0.30,                   // Feedback interne (incertitude)
        .gamma = 0.15,                  // Décroissance rapide (instable)
        .delta = 0.50,                  // Recherche dans les souvenirs
        .theta = 0.15,                  // Sagesse élevée (analyse)
        .amyghaleon_threshold = 0.80,   // Modéré
        .memory_consolidation = 0.3,
        .learning_rate = 0.7,
        .focus = 0.5,
        .priority = 2
    }}
};

/**
 * @brief Émotions critiques pour Amyghaleon
 */
inline const std::array<std::string, 3> CRITICAL_EMOTIONS = {
    "Peur", "Horreur", "Anxiété"
};

/**
 * @brief Indices des émotions critiques
 */
inline const std::array<size_t, 3> CRITICAL_EMOTION_INDICES = {
    14,  // Peur
    15,  // Horreur
    4    // Anxiété
};

/**
 * @brief Seuils d'urgence pour transition immédiate vers PEUR
 */
struct EmergencyThresholds {
    static constexpr double PEUR_IMMEDIATE = 0.85;
    static constexpr double HORREUR_IMMEDIATE = 0.80;
    static constexpr double TRAUMA_ACTIVATION = 0.60;
};

} // namespace mcee

#endif // MCEE_PHASE_CONFIG_HPP
