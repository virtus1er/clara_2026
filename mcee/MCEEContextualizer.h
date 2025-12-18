//===== MCEEContextualizer.h - SEULEMENT LES DECLARATIONS =====
#pragma once
#include "MCEETypes.h"
#include "MCEEGradients.h"
#include <memory>

namespace MCEE {
    class EmotionContextualizer {
    public:
        explicit EmotionContextualizer();

        ContextualizedEmotions contextualizeEmotions(
            const EmotionalInput& emotional_input,
            const ContextData& context_data
        );

        std::string detectContext(const EmotionArray& emotions, const ContextData& context);
        double calculateSignificanceScore(const ContextualizedEmotions& emotions);

    private:
        std::unique_ptr<GradientCalculator> gradient_calculator_;

        double contextualizeEmotion(double raw_emotion, double danger_gradient,
                                  const std::string& context, const ContextData& context_data);

        double calculateInfluenceExternalFeedbacks(const ExternalFeedbacks& feedbacks);
        double calculateInfluenceTechnicalStates(const TechnicalStates& states);
        double calculateInfluencePhysicalSensors(const PhysicalSensors& sensors);
        double calculateInfluenceMemoryMCT(const std::string& context);
        double calculateTransitionAdjustment(const std::string& new_context);
        double calculateDangerModulation(double danger_gradient);

        double calculateGlobalContextualizedEmotion(const EmotionArray& emotions,
                                                   const std::string& context,
                                                   double coherence);
        double calculateContextualCoherence(const EmotionArray& emotions, const std::string& context);
        double getEmotionWeight(int emotion_index, const std::string& context);

        std::string last_context_;
        Timestamp last_context_change_;
    };
}