#pragma once

#include <string>
#include <cstdint>

namespace MCEE {

/**
 * États du cycle circadien simulé (T = 12h)
 */
enum class DreamState : uint8_t {
    AWAKE,              // Mode éveil - accumulation MCT (80% du cycle)
    
    // Phases du rêve (20% du cycle)
    DREAM_SCAN,         // Phase 1: Scan MCT, calcul Csocial(t) - 10%
    DREAM_CONSOLIDATE,  // Phase 2: Transfert MLT, renforcement arêtes - 60%
    DREAM_EXPLORE,      // Phase 3: Associations stochastiques - 20%
    DREAM_CLEANUP,      // Phase 4: Suppression nœuds sous seuil - 10%
    
    INTERRUPTED         // Interruption par Amyghaleon
};

/**
 * Convertit un DreamState en string lisible
 */
inline std::string dreamStateToString(DreamState state) {
    switch (state) {
        case DreamState::AWAKE:             return "AWAKE";
        case DreamState::DREAM_SCAN:        return "DREAM_SCAN";
        case DreamState::DREAM_CONSOLIDATE: return "DREAM_CONSOLIDATE";
        case DreamState::DREAM_EXPLORE:     return "DREAM_EXPLORE";
        case DreamState::DREAM_CLEANUP:     return "DREAM_CLEANUP";
        case DreamState::INTERRUPTED:       return "INTERRUPTED";
        default:                            return "UNKNOWN";
    }
}

/**
 * Vérifie si le système est en mode rêve (toute phase)
 */
inline bool isDreaming(DreamState state) {
    return state == DreamState::DREAM_SCAN ||
           state == DreamState::DREAM_CONSOLIDATE ||
           state == DreamState::DREAM_EXPLORE ||
           state == DreamState::DREAM_CLEANUP;
}

} // namespace MCEE
