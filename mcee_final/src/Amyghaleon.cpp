/**
 * @file Amyghaleon.cpp
 * @brief Implémentation du système d'urgence Amyghaleon
 * @version 2.0
 * @date 2025-12-19
 */

#include "Amyghaleon.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>

namespace mcee {

Amyghaleon::Amyghaleon() {
    std::cout << "[Amyghaleon] Système d'urgence initialisé\n";
}

bool Amyghaleon::checkEmergency(
    const EmotionalState& state,
    const std::vector<Memory>& active_memories,
    double phase_threshold) const
{
    // 1. Vérifier les émotions critiques
    auto [max_emotion, max_value] = findMaxCriticalEmotion(state);

    if (max_value > phase_threshold) {
        if (!quiet_mode_) {
            std::cout << "[Amyghaleon] ⚠ Émotion critique détectée: "
                      << max_emotion << " = " << std::fixed << std::setprecision(3)
                      << max_value << " > seuil " << phase_threshold << "\n";
        }
        return true;
    }

    // 2. Vérifier les traumas activés
    double trauma_threshold = phase_threshold - 0.2;
    for (const auto& mem : active_memories) {
        if (isTraumaActivated(mem, trauma_threshold)) {
            if (!quiet_mode_) {
                std::cout << "[Amyghaleon] ⚠ Trauma activé: " << mem.name
                          << " (activation=" << std::fixed << std::setprecision(3)
                          << mem.activation << ")\n";
            }
            return true;
        }
    }

    // 3. Vérifier combinaison émotion critique + trauma
    if (max_value > (phase_threshold + 0.2)) {
        for (const auto& mem : active_memories) {
            if (mem.is_trauma && mem.activation > 0.6) {
                if (!quiet_mode_) {
                    std::cout << "[Amyghaleon] ⚠ Combinaison critique + trauma détectée\n";
                }
                return true;
            }
        }
    }

    return false;
}

EmergencyResponse Amyghaleon::triggerEmergencyResponse(
    const EmotionalState& state,
    Phase phase)
{
    emergency_count_++;

    auto [max_emotion, max_value] = findMaxCriticalEmotion(state);

    EmergencyResponse response;
    response.triggered = true;
    response.action = determineAction(max_emotion);
    response.priority = determinePriority(max_value);
    response.phase_at_trigger = phase;
    response.trigger_emotion = max_emotion;
    response.emotion_value = max_value;

    // Afficher le banner seulement si pas en mode silencieux
    if (!quiet_mode_) {
        std::cout << "\n[Amyghaleon] ═══════════════════════════════════════\n"
                  << "[Amyghaleon] ⚡ RÉPONSE D'URGENCE DÉCLENCHÉE #" << emergency_count_ << "\n"
                  << "[Amyghaleon] ═══════════════════════════════════════\n"
                  << "[Amyghaleon] Action    : " << response.action << "\n"
                  << "[Amyghaleon] Priorité  : " << response.priority << "\n"
                  << "[Amyghaleon] Phase     : " << phaseToString(phase) << "\n"
                  << "[Amyghaleon] Émotion   : " << max_emotion << " = "
                  << std::fixed << std::setprecision(3) << max_value << "\n"
                  << "[Amyghaleon] ═══════════════════════════════════════\n\n";
    }

    // Appeler le callback si défini
    if (on_emergency_) {
        on_emergency_(response);
    }

    return response;
}

void Amyghaleon::setEmergencyCallback(EmergencyCallback callback) {
    on_emergency_ = std::move(callback);
}

bool Amyghaleon::isTraumaActivated(const Memory& memory, double threshold) const {
    return memory.is_trauma && memory.activation > threshold;
}

std::pair<std::string, double> Amyghaleon::findMaxCriticalEmotion(
    const EmotionalState& state) const 
{
    std::string max_name;
    double max_value = -1.0;

    for (const auto& emo_name : CRITICAL_EMOTIONS) {
        double value = state.getEmotion(emo_name);
        if (value > max_value) {
            max_value = value;
            max_name = emo_name;
        }
    }

    return {max_name, max_value};
}

std::string Amyghaleon::determineAction(const std::string& emotion_name) const {
    if (emotion_name == "Peur") {
        return "FUITE";
    } else if (emotion_name == "Horreur") {
        return "BLOCAGE";
    } else if (emotion_name == "Anxiété") {
        return "ALERTE";
    }
    return "SURVEILLANCE";
}

std::string Amyghaleon::determinePriority(double emotion_value) const {
    if (emotion_value > 0.85) {
        return "CRITIQUE";
    } else if (emotion_value > 0.70) {
        return "ELEVEE";
    } else if (emotion_value > 0.50) {
        return "MOYENNE";
    }
    return "BASSE";
}

} // namespace mcee
