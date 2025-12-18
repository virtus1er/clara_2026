//===== MCEEGradients.h - SEULEMENT LES DECLARATIONS =====
#pragma once
#include "MCEETypes.h"

namespace MCEE {
    class GradientCalculator {
    public:
        explicit GradientCalculator();

        double calculateEnvironmentalGradient(const PhysicalSensors& sensors);
        double calculateSystemStressGradient(const TechnicalStates& states);
        double calculateGlobalDangerGradient(double environmental, double system_stress,
                                           double trauma_historical = 0.0,
                                           double emotional_instability = 0.0);

        DangerLevel classifyDangerLevel(double global_gradient);
        double getAdaptiveMLTThreshold(double danger_gradient);
        double getAdaptiveAmygdaleonThreshold(double danger_gradient);
        double getUrgencyFactor(double danger_gradient);

        bool isCriticalPatternDetected(double global_gradient, double gradient_derivative,
                                     double persistence_time_s);

    private:
        double calculateTemperatureCriticalFactor(double cpu_temp, double gpu_temp);
        Timestamp last_calculation_time_;
        double last_global_gradient_ = 0.0;
    };
}