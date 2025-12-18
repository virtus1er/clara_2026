//===== MCEEGradients.cpp =====
#include "MCEEGradients.h"
#include "MCEEConfig.h"
#include <algorithm>
#include <cmath>

namespace MCEE {
    GradientCalculator::GradientCalculator() {
        last_calculation_time_ = std::chrono::steady_clock::now();
    }

    double GradientCalculator::calculateEnvironmentalGradient(const PhysicalSensors& sensors) {
        const auto& params = ConfigManager::getInstance().getParameters();

        double gradient = 0.0;

        // Gyroscope instability
        gradient += params.omega1_gyroscope * sensors.gyroscope_stabilite;

        // Volume sonore (danger si > 0.8)
        gradient += params.omega2_volume * std::max(0.0, sensors.volume_sonore - 0.8);

        // Température ambiante (danger si trop éloigné de 0.5)
        gradient += params.omega3_temperature * std::abs(sensors.temperature_ambiante - 0.5);

        // Orientation danger (placeholder - peut être étendu)
        gradient += params.omega4_orientation * 0.0;

        return std::min(1.0, gradient);
    }

    double GradientCalculator::calculateSystemStressGradient(const TechnicalStates& states) {
        const auto& params = ConfigManager::getInstance().getParameters();

        double gradient = 0.0;

        // Charge CPU critique
        gradient += params.sigma1_charge_cpu * std::max(0.0, states.charge_cpu - 0.7);

        // Utilisation RAM critique
        gradient += params.sigma2_utilisation_ram * std::max(0.0, states.utilisation_ram - 0.8);

        // Facteur température critique
        double temp_factor = calculateTemperatureCriticalFactor(states.temperature_cpu,
                                                               states.temperature_gpu);
        gradient += params.sigma3_temperature_critique * temp_factor;

        // Instabilité système
        gradient += params.sigma4_stabilite_systeme * (1.0 - states.stabilite_systeme);

        return std::min(1.0, gradient);
    }

    double GradientCalculator::calculateGlobalDangerGradient(double environmental,
                                                           double system_stress,
                                                           double trauma_historical,
                                                           double emotional_instability) {
        const auto& params = ConfigManager::getInstance().getParameters();

        double global_gradient =
            params.poids_environnemental * environmental +
            params.poids_stress_systeme * system_stress +
            params.poids_trauma_historique * trauma_historical +
            params.poids_instabilite_emotionnelle * emotional_instability;

        last_global_gradient_ = std::min(1.0, global_gradient);
        last_calculation_time_ = std::chrono::steady_clock::now();

        return last_global_gradient_;
    }

    DangerLevel GradientCalculator::classifyDangerLevel(double global_gradient) {
        const auto& params = ConfigManager::getInstance().getParameters();

        if (global_gradient < params.seuil_normal_max) return DangerLevel::NORMAL;
        if (global_gradient < params.seuil_surveillance_max) return DangerLevel::SURVEILLANCE;
        if (global_gradient < params.seuil_alerte_max) return DangerLevel::ALERTE;
        if (global_gradient < params.seuil_critique_max) return DangerLevel::CRITIQUE;
        return DangerLevel::URGENCE;
    }

    double GradientCalculator::getAdaptiveMLTThreshold(double danger_gradient) {
        const auto& params = ConfigManager::getInstance().getParameters();
        return std::max(0.45, params.seuil_mlt_base - (danger_gradient * 0.20));
    }

    double GradientCalculator::getAdaptiveAmygdaleonThreshold(double danger_gradient) {
        const auto& params = ConfigManager::getInstance().getParameters();
        return std::max(0.75, params.seuil_amyghaleon - (danger_gradient * 0.15));
    }

    double GradientCalculator::getUrgencyFactor(double danger_gradient) {
        return 1.0 + (danger_gradient * 0.5);
    }

    bool GradientCalculator::isCriticalPatternDetected(double global_gradient,
                                                      double gradient_derivative,
                                                      double persistence_time_s) {
        return (global_gradient > 0.8) &&
               (gradient_derivative > 0.3) &&
               (persistence_time_s > 30.0);
    }

    double GradientCalculator::calculateTemperatureCriticalFactor(double cpu_temp, double gpu_temp) {
        double max_temp = std::max(cpu_temp, gpu_temp);

        if (max_temp < 60.0) return 0.0;
        if (max_temp < 75.0) return 0.3;
        if (max_temp < 85.0) return 0.7;
        return 1.0;
    }
}
