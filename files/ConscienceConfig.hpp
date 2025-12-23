#pragma once

#include <array>
#include <string>
#include <cstdint>

namespace MCEE {

/**
 * Configuration des coefficients de pondération pour la Conscience & Sentiments
 * Basé sur le document "Conscience et Sentiments Version Ajustée"
 */
struct ConscienceConfig {
    // ═══════════════════════════════════════════════════════════════════════════
    // COEFFICIENTS ÉMOTIONS INSTANTANÉES (αi)
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Pondération des 24 émotions dans le calcul de conscience
    std::array<double, 24> alpha_emotions = {
        1.0, 1.0, 1.0, 1.0, 1.0, 1.0,   // Émotions 0-5
        1.0, 1.0, 1.0, 1.0, 1.0, 1.0,   // Émotions 6-11
        1.0, 1.0, 1.0, 1.0, 1.0, 1.0,   // Émotions 12-17
        1.0, 1.0, 1.0, 1.0, 1.0, 1.0    // Émotions 18-23
    };
    
    // ═══════════════════════════════════════════════════════════════════════════
    // COEFFICIENTS MÉMOIRES (ωj pour Mtotal)
    // ═══════════════════════════════════════════════════════════════════════════
    
    double omega_MCT = 0.3;        // ωC : Mémoire Court Terme (récence)
    double omega_MLT = 0.25;       // ωL : Mémoire Long Terme (expérience)
    double omega_MP = 0.15;        // ωP : Mémoire Procédurale (habitudes)
    double omega_ME = 0.15;        // ωE : Mémoire Épisodique (vécu)
    double omega_MS = 0.1;         // ωS : Mémoire Sémantique (connaissances)
    double omega_MA = 0.05;        // ωA : Mémoire Autobiographique (identité)
    
    // ═══════════════════════════════════════════════════════════════════════════
    // COEFFICIENTS TRAUMA
    // ═══════════════════════════════════════════════════════════════════════════
    
    double omega_trauma = 5.0;           // Pondération trauma (quasi-inoubliable)
    double trauma_dominance_threshold = 0.7;  // Seuil activation Tdominant
    double trauma_decay_rate = 0.001;    // Décroissance très lente des traumas
    
    // ═══════════════════════════════════════════════════════════════════════════
    // COEFFICIENTS FEEDBACK & ENVIRONNEMENT
    // ═══════════════════════════════════════════════════════════════════════════
    
    double beta_feedback = 0.2;    // β : Pondération feedback externe
    double delta_environment = 0.15;  // δ : Influence environnement
    
    // ═══════════════════════════════════════════════════════════════════════════
    // COEFFICIENT DE SAGESSE (Wt)
    // ═══════════════════════════════════════════════════════════════════════════
    
    double wisdom_initial = 1.0;         // Sagesse initiale
    double wisdom_growth_rate = 0.001;   // Croissance par expérience
    double wisdom_max = 2.0;             // Plafond de sagesse
    
    // ═══════════════════════════════════════════════════════════════════════════
    // PARAMÈTRES SENTIMENTS (Ft)
    // ═══════════════════════════════════════════════════════════════════════════
    
    double gamma_decay = 0.95;           // γ : Décroissance temporelle conscience passée
    double lambda_feedback = 0.1;        // λ : Impact feedback sur sentiments
    size_t sentiment_history_size = 100; // Taille fenêtre glissante Σγk·Ck
    
    // ═══════════════════════════════════════════════════════════════════════════
    // PARAMÈTRES MISE À JOUR
    // ═══════════════════════════════════════════════════════════════════════════
    
    double update_interval_ms = 50.0;    // Intervalle mise à jour (20 Hz)
    bool use_tanh_normalization = true;  // Utiliser tanh pour Ft
    double tanh_scale = 1.0;             // Facteur d'échelle pour tanh
};

/**
 * Types de trauma reconnus par le système
 */
enum class TraumaType : uint8_t {
    NONE = 0,
    PHYSICAL = 1,      // Tp : Trauma physique
    EMOTIONAL = 2,     // Te : Trauma émotionnel  
    SOCIAL = 3         // Ts : Trauma social
};

/**
 * Structure représentant un trauma actif
 */
struct TraumaState {
    TraumaType type = TraumaType::NONE;
    double intensity = 0.0;              // 0.0 à 1.0
    double activation_time = 0.0;        // Timestamp activation
    std::string trigger_context;         // Contexte déclencheur
    uint64_t source_memory_id = 0;       // ID du souvenir source dans le graphe
    
    [[nodiscard]] bool isActive() const { return type != TraumaType::NONE && intensity > 0.1; }
    [[nodiscard]] bool isDominant(double threshold) const { return intensity >= threshold; }
};

/**
 * État des mémoires agrégées depuis le graphe
 */
struct MemoryActivation {
    double MCT_score = 0.0;    // Score activation MCT
    double MLT_score = 0.0;    // Score activation MLT
    double MP_score = 0.0;     // Score activation procédurale
    double ME_score = 0.0;     // Score activation épisodique
    double MS_score = 0.0;     // Score activation sémantique
    double MA_score = 0.0;     // Score activation autobiographique
    
    // Calcul Mtotal avec coefficients
    [[nodiscard]] double computeMtotal(const ConscienceConfig& cfg) const {
        return cfg.omega_MCT * MCT_score +
               cfg.omega_MLT * MLT_score +
               cfg.omega_MP * MP_score +
               cfg.omega_ME * ME_score +
               cfg.omega_MS * MS_score +
               cfg.omega_MA * MA_score;
    }
};

/**
 * État de l'environnement
 */
struct EnvironmentState {
    double hostility = 0.0;      // 0 = calme, 1 = hostile
    double noise_level = 0.0;    // Niveau sonore normalisé
    double familiarity = 0.5;    // 0 = inconnu, 1 = familier
    double social_density = 0.0; // Densité sociale
    
    // Score environnemental global
    [[nodiscard]] double computeScore() const {
        return (1.0 - hostility) * 0.4 + 
               (1.0 - noise_level) * 0.2 + 
               familiarity * 0.3 + 
               (1.0 - social_density * 0.5) * 0.1;
    }
};

/**
 * Feedback externe reçu
 */
struct FeedbackState {
    double valence = 0.0;        // -1 (négatif) à +1 (positif)
    double intensity = 0.0;      // Force du feedback
    double credibility = 1.0;    // Crédibilité de la source
    
    [[nodiscard]] double computeScore() const {
        return valence * intensity * credibility;
    }
};

} // namespace MCEE
