#pragma once

#include <chrono>
#include <cmath>

namespace MCEE {

/**
 * Configuration du cycle circadien et des paramètres de consolidation
 */
struct DreamConfig {
    // ═══════════════════════════════════════════════════════════
    // CYCLE CIRCADIEN (T = 12h)
    // ═══════════════════════════════════════════════════════════
    
    /// Période du cycle en secondes (12h = 43200s)
    double cyclePeriod_s = 12.0 * 60.0 * 60.0;  // 43200s
    
    /// Ratio éveil/rêve (80% éveil, 20% rêve)
    double awakeRatio = 0.80;
    double dreamRatio = 0.20;
    
    /// Distribution des phases de rêve (sur les 20%)
    double scanPhaseRatio      = 0.10;  // 10% du temps de rêve
    double consolidateRatio    = 0.60;  // 60% du temps de rêve
    double exploreRatio        = 0.20;  // 20% du temps de rêve
    double cleanupRatio        = 0.10;  // 10% du temps de rêve
    
    // ═══════════════════════════════════════════════════════════
    // CONDITIONS DE DÉCLENCHEMENT DU RÊVE
    // ═══════════════════════════════════════════════════════════
    
    /// Seuil minimum depuis dernier cycle (en secondes)
    double minTimeSinceLastDream_s = 9.0 * 60.0 * 60.0;  // 9h
    
    /// Seuil d'activité émotionnelle basse pour autoriser le rêve
    double maxEmotionalActivityForDream = 0.3;
    
    /// Patterns bloquant le rêve
    bool blockDreamOnPEUR = true;
    bool blockDreamOnANXIETE = true;
    
    // ═══════════════════════════════════════════════════════════
    // SCORE DE CONSOLIDATION SOCIALE: Csocial(t)
    // Csocial(t) = ρ·|Ecurrent - Esouvenir| + λ·Feedback + η·Usage + θ·Influence
    // ═══════════════════════════════════════════════════════════
    
    double rho   = 0.35;   // Poids de la distance émotionnelle
    double lambda = 0.25;  // Poids du feedback
    double eta   = 0.20;   // Poids de l'usage/réactivation
    double theta = 0.20;   // Poids de l'influence décisionnelle
    
    /// Seuil τ de consolidation
    double consolidationThreshold = 0.5;
    
    // ═══════════════════════════════════════════════════════════
    // PARAMÈTRES STOCHASTIQUES (Phase exploration)
    // σi = coefficient de bruit pour associations nouvelles
    // ═══════════════════════════════════════════════════════════
    
    /// Bruit de base
    double sigmaBase = 0.15;
    
    /// Modulation selon le pattern actif (multiplicateurs)
    double sigmaMultiplier_EXPLORATION = 1.5;
    double sigmaMultiplier_SERENITE    = 1.2;
    double sigmaMultiplier_JOIE        = 1.3;
    double sigmaMultiplier_ANXIETE     = 0.6;
    double sigmaMultiplier_PEUR        = 0.4;
    double sigmaMultiplier_TRISTESSE   = 0.8;
    double sigmaMultiplier_DEGOUT      = 0.7;
    double sigmaMultiplier_CONFUSION   = 0.9;
    
    // ═══════════════════════════════════════════════════════════
    // OUBLI ET RENFORCEMENT
    // ═══════════════════════════════════════════════════════════
    
    /// Taux de décroissance exponentielle pour oubli
    double forgetDecayRate = 0.05;
    
    /// Facteur de renforcement des arêtes (lors de consolidation)
    double reinforcementFactor = 1.2;
    
    /// Seuil de poids minimum avant suppression
    double minWeightBeforeDeletion = 0.1;
    
    /// Protection trauma (multiplicateur de rétention)
    double traumaRetentionMultiplier = 10.0;
    
    // ═══════════════════════════════════════════════════════════
    // HELPERS
    // ═══════════════════════════════════════════════════════════
    
    /// Calcule la position dans le cycle sinusoïdal S(t) = sin(2πt/T)
    [[nodiscard]] double cyclePosition(double elapsed_s) const {
        return std::sin(2.0 * M_PI * elapsed_s / cyclePeriod_s);
    }
    
    /// Durée totale du rêve en secondes
    [[nodiscard]] double dreamDuration_s() const {
        return cyclePeriod_s * dreamRatio;
    }
    
    /// Durées des phases individuelles
    [[nodiscard]] double scanDuration_s() const {
        return dreamDuration_s() * scanPhaseRatio;
    }
    [[nodiscard]] double consolidateDuration_s() const {
        return dreamDuration_s() * consolidateRatio;
    }
    [[nodiscard]] double exploreDuration_s() const {
        return dreamDuration_s() * exploreRatio;
    }
    [[nodiscard]] double cleanupDuration_s() const {
        return dreamDuration_s() * cleanupRatio;
    }
};

} // namespace MCEE
