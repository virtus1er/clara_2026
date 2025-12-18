//===== MCEEContextualizer.cpp - Version sans warnings =====
#include "MCEEContextualizer.h"
#include "MCEEConfig.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace MCEE {
    EmotionContextualizer::EmotionContextualizer()
        : gradient_calculator_(std::make_unique<GradientCalculator>()) {
        last_context_change_ = std::chrono::steady_clock::now();
    }

    ContextualizedEmotions EmotionContextualizer::contextualizeEmotions(
        const EmotionalInput& emotional_input,
        const ContextData& context_data) {

        ContextualizedEmotions result;
        result.text_id = emotional_input.text_id;
        result.timestamp = std::chrono::steady_clock::now();

        // Calculate gradients
        double env_gradient = gradient_calculator_->calculateEnvironmentalGradient(
            context_data.capteurs_physiques);
        double sys_gradient = gradient_calculator_->calculateSystemStressGradient(
            context_data.etats_internes);

        result.gradient_danger_global = gradient_calculator_->calculateGlobalDangerGradient(
            env_gradient, sys_gradient);

        result.niveau_danger = gradient_calculator_->classifyDangerLevel(
            result.gradient_danger_global);

        // Detect context
        result.contexte_detecte = detectContext(emotional_input.emotions_brutes, context_data);
        result.confiance_contexte = 0.85; // Simplified for now

        // Contextualize each emotion
        for (size_t i = 0; i < 24; ++i) {
            result.emotions_contextualisees[i] = contextualizeEmotion(
                emotional_input.emotions_brutes[i],
                result.gradient_danger_global,
                result.contexte_detecte,
                context_data
            );
        }

        // Calculate global emotion
        double coherence = calculateContextualCoherence(result.emotions_contextualisees,
                                                       result.contexte_detecte);
        result.emotion_globale = calculateGlobalContextualizedEmotion(
            result.emotions_contextualisees, result.contexte_detecte, coherence);

        // Determine if Amygdaleon signal is needed
        double adaptive_threshold = gradient_calculator_->getAdaptiveAmygdaleonThreshold(
            result.gradient_danger_global);

        result.signal_amyghaleon =
            (result.gradient_danger_global > adaptive_threshold) ||
            (*std::max_element(result.emotions_contextualisees.begin(),
                             result.emotions_contextualisees.end()) > adaptive_threshold);

        // Determine if memory consolidation is needed
        double significance_score = calculateSignificanceScore(result);
        double adaptive_mlt_threshold = gradient_calculator_->getAdaptiveMLTThreshold(
            result.gradient_danger_global);

        result.souvenir_a_consolider = significance_score >= adaptive_mlt_threshold;

        if (result.gradient_danger_global > 0.8) {
            result.priorite_mlt = Priority::CRITIQUE;
        } else if (result.gradient_danger_global > 0.6) {
            result.priorite_mlt = Priority::HIGH;
        } else {
            result.priorite_mlt = Priority::NORMAL;
        }

        return result;
    }

    std::string EmotionContextualizer::detectContext(const EmotionArray& emotions,
                                                    const ContextData& context) {
        // Simplified context detection based on dominant patterns

        // Check for technical stress
        if (context.etats_internes.charge_cpu > 0.7 ||
            context.etats_internes.utilisation_ram > 0.8 ||
            std::max(context.etats_internes.temperature_cpu,
                    context.etats_internes.temperature_gpu) > 75.0) {
            return "stress_technique";
        }

        // Check for physical urgency
        if (context.capteurs_physiques.gyroscope_stabilite > 0.8 ||
            context.capteurs_physiques.volume_sonore > 0.8) {
            return "urgence_physique";
        }

        // Check for social joy (high positive emotions + social interaction)
        double positive_emotions = emotions[1] + emotions[13] + emotions[16] + emotions[17] + emotions[21]; // Joy, satisfaction, etc.
        if (positive_emotions > 2.0 && context.feedbacks_externes.interaction_sociale) {
            return "joie_sociale";
        }

        // Check for stable routine (low gradients, low emotional intensity)
        double total_intensity = std::accumulate(emotions.begin(), emotions.end(), 0.0);
        if (total_intensity < 3.0 &&
            context.capteurs_physiques.gyroscope_stabilite < 0.3 &&
            context.etats_internes.charge_cpu < 0.5) {
            return "routine_stable";
        }

        return "contexte_general";
    }

    double EmotionContextualizer::calculateSignificanceScore(const ContextualizedEmotions& emotions) {
        // Intensity factor
        double intensity = std::accumulate(emotions.emotions_contextualisees.begin(),
                                         emotions.emotions_contextualisees.end(), 0.0) / 24.0;

        // Novelty factor (simplified)
        double novelty = (emotions.contexte_detecte != last_context_) ? 0.8 : 0.2;

        // Coherence factor
        double coherence = calculateContextualCoherence(emotions.emotions_contextualisees,
                                                       emotions.contexte_detecte);

        // Duration factor (simplified)
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_context_change_).count();
        double duration_factor = std::min(1.0, duration / 60.0); // Max factor at 1 minute

        double score = intensity * 0.35 +
                      novelty * 0.20 +
                      coherence * 0.15 +
                      emotions.gradient_danger_global * 0.20 +
                      duration_factor * 0.10;

        return score;
    }

    double EmotionContextualizer::contextualizeEmotion(double raw_emotion,
                                                      double danger_gradient,
                                                      const std::string& context,
                                                      const ContextData& context_data) {
        const auto& params = ConfigManager::getInstance().getParameters();

        // Base modulation
        double contextualized = raw_emotion * 1.0; // Base modulator could be context-dependent

        // Apply influences
        contextualized += params.alpha_feedbacks *
            calculateInfluenceExternalFeedbacks(context_data.feedbacks_externes);

        contextualized += params.beta_etats_techniques *
            calculateInfluenceTechnicalStates(context_data.etats_internes);

        contextualized += params.gamma_capteurs_physiques *
            calculateInfluencePhysicalSensors(context_data.capteurs_physiques);

        contextualized += params.delta_souvenirs_mct *
            calculateInfluenceMemoryMCT(context);

        contextualized += params.epsilon_transition_contexte *
            calculateTransitionAdjustment(context);

        contextualized += params.eta_gradient_danger *
            calculateDangerModulation(danger_gradient);

        // Normalize to [0,1]
        return std::max(0.0, std::min(1.0, contextualized));
    }

    double EmotionContextualizer::calculateInfluenceExternalFeedbacks(
        const ExternalFeedbacks& feedbacks) {
        double positive_sum = (feedbacks.validation_positive ? 1.0 : 0.0) +
                             (feedbacks.encouragement_recu ? 1.0 : 0.0) +
                             (feedbacks.interaction_sociale ? 1.0 : 0.0);

        double negative_sum = (feedbacks.alerte_externe ? 1.0 : 0.0);

        return positive_sum * 0.3 - negative_sum * 0.5;
    }

    double EmotionContextualizer::calculateInfluenceTechnicalStates(
        const TechnicalStates& states) {
        double cpu_factor = states.charge_cpu > 0.7 ? (states.charge_cpu - 0.7) : 0.0;
        double ram_factor = states.utilisation_ram > 0.8 ? (states.utilisation_ram - 0.8) : 0.0;
        double temp_factor = gradient_calculator_->calculateEnvironmentalGradient(
            PhysicalSensors{}); // Simplified
        double stability_factor = 1.0 - states.stabilite_systeme;

        return -(cpu_factor * 0.3 + ram_factor * 0.25 + temp_factor * 0.35 + stability_factor * 0.1);
    }

    double EmotionContextualizer::calculateInfluencePhysicalSensors(
        const PhysicalSensors& sensors) {
        return sensors.temperature_ambiante * 0.2 +
               sensors.volume_sonore * 0.3 +
               sensors.luminosite * 0.2 +
               sensors.gyroscope_stabilite * 0.3;
    }

    double EmotionContextualizer::calculateInfluenceMemoryMCT(const std::string& context) {
        // Simplified - would normally query MCT database
        if (context == "stress_technique" || context == "urgence_physique") {
            return 0.3; // Historical trauma influence
        }
        return 0.1;
    }

    double EmotionContextualizer::calculateTransitionAdjustment(const std::string& new_context) {
        if (new_context != last_context_) {
            last_context_ = new_context;
            last_context_change_ = std::chrono::steady_clock::now();
            return 0.2; // Transition boost
        }
        return 0.0;
    }

    double EmotionContextualizer::calculateDangerModulation(double danger_gradient) {
        // Suppress positive emotions and amplify protective emotions under danger
        return -danger_gradient * 0.3; // General suppression factor
    }

    double EmotionContextualizer::calculateGlobalContextualizedEmotion(
        const EmotionArray& emotions,
        const std::string& context,
        double coherence) {

        double weighted_sum = 0.0;
        double total_weight = 0.0;

        for (size_t i = 0; i < emotions.size(); ++i) {
            double weight = getEmotionWeight(i, context);
            weighted_sum += emotions[i] * weight;
            total_weight += weight;
        }

        double base_emotion = total_weight > 0 ? weighted_sum / total_weight : 0.0;
        return base_emotion * coherence;
    }

    double EmotionContextualizer::calculateContextualCoherence(
        const EmotionArray& emotions,
        const std::string& context) {
        // Simplified coherence calculation
        // Count conflicting emotions vs compatible emotions
        int compatible = 0;
        int total_active = 0;

        for (size_t i = 0; i < emotions.size(); ++i) {
            if (emotions[i] > 0.1) { // Consider as active
                total_active++;
                double weight = getEmotionWeight(i, context);
                if (weight > 1.0) compatible++; // Compatible emotion
            }
        }

        return total_active > 0 ? static_cast<double>(compatible) / total_active : 1.0;
    }

    double EmotionContextualizer::getEmotionWeight(int emotion_index,
                                                  const std::string& context) {
        // Simplified emotion weighting based on context
        if (context == "joie_sociale") {
            // Positive emotions get higher weight
            if (emotion_index == 1 || emotion_index == 13 ||
                emotion_index == 16 || emotion_index == 17 || emotion_index == 21) {
                return 1.5; // Joy, satisfaction, excitement, etc.
            }
            if (emotion_index == 4 || emotion_index == 6 || emotion_index == 14) {
                return 0.5; // Fear, stress, tension
            }
        } else if (context == "stress_technique" || context == "urgence_physique") {
            // Stress/fear emotions get higher weight
            if (emotion_index == 4 || emotion_index == 6 || emotion_index == 9 || emotion_index == 14) {
                return 1.5; // Fear, stress, anxiety, tension
            }
            if (emotion_index == 1 || emotion_index == 13 || emotion_index == 17) {
                return 0.5; // Joy, satisfaction, excitement
            }
        }

        return 1.0; // Default weight
    }
}