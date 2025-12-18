//===== MCEEConfig.cpp - Version propre =====
#include "MCEEConfig.h"
#include <fstream>
#include <iostream>
#include <sstream>

namespace MCEE {
    ConfigManager& ConfigManager::getInstance() {
        static ConfigManager instance;
        return instance;
    }

    bool ConfigManager::loadFromFile(const std::string& configPath) {
        std::ifstream file(configPath);
        if (!file.is_open()) {
            std::cerr << "Could not open config file: " << configPath << std::endl;
            setDefaults();
            return false;
        }

        setDefaults(); // Start with defaults

        std::string line;
        while (std::getline(file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;

            size_t pos = line.find('=');
            if (pos == std::string::npos) continue;

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            parseConfigLine(key, value);
        }

        return true;
    }

    void ConfigManager::parseConfigLine(const std::string& key, const std::string& value) {
        try {
            // String parameters - traiter en premier pour éviter l'erreur stod
            if (key == "rabbitmq_host") {
                params_.rabbitmq_host = value;
                return;
            } else if (key == "rabbitmq_username") {
                params_.rabbitmq_username = value;
                return;
            } else if (key == "rabbitmq_password") {
                params_.rabbitmq_password = value;
                return;
            } else if (key == "queue_emotional_input") {
                params_.queue_emotional_input = value;
                return;
            } else if (key == "queue_context_input") {
                params_.queue_context_input = value;
                return;
            } else if (key == "queue_consciousness_output") {
                params_.queue_consciousness_output = value;
                return;
            } else if (key == "queue_amygdaleon_output") {
                params_.queue_amygdaleon_output = value;
                return;
            } else if (key == "queue_mlt_output") {
                params_.queue_mlt_output = value;
                return;
            }
            // Integer parameters
            else if (key == "rabbitmq_port") {
                params_.rabbitmq_port = std::stoi(value);
                return;
            } else if (key == "frequence_maj_hz") {
                params_.frequence_maj_hz = std::stoi(value);
                return;
            } else if (key == "latence_max_ms") {
                params_.latence_max_ms = std::stoi(value);
                return;
            }
            // Double parameters - traiter en dernier
            else {
                double dvalue = std::stod(value);
                setParameter(key, dvalue);
                return;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error parsing config parameter " << key << " = " << value
                      << ": " << e.what() << std::endl;
        }
    }

    void ConfigManager::setParameter(const std::string& key, double value) {
        if (key == "alpha_feedbacks") params_.alpha_feedbacks = value;
        else if (key == "beta_etats_techniques") params_.beta_etats_techniques = value;
        else if (key == "gamma_capteurs_physiques") params_.gamma_capteurs_physiques = value;
        else if (key == "delta_souvenirs_mct") params_.delta_souvenirs_mct = value;
        else if (key == "epsilon_transition_contexte") params_.epsilon_transition_contexte = value;
        else if (key == "eta_gradient_danger") params_.eta_gradient_danger = value;
        else if (key == "omega1_gyroscope") params_.omega1_gyroscope = value;
        else if (key == "omega2_volume") params_.omega2_volume = value;
        else if (key == "omega3_temperature") params_.omega3_temperature = value;
        else if (key == "omega4_orientation") params_.omega4_orientation = value;
        else if (key == "sigma1_charge_cpu") params_.sigma1_charge_cpu = value;
        else if (key == "sigma2_utilisation_ram") params_.sigma2_utilisation_ram = value;
        else if (key == "sigma3_temperature_critique") params_.sigma3_temperature_critique = value;
        else if (key == "sigma4_stabilite_systeme") params_.sigma4_stabilite_systeme = value;
        else if (key == "poids_environnemental") params_.poids_environnemental = value;
        else if (key == "poids_stress_systeme") params_.poids_stress_systeme = value;
        else if (key == "poids_trauma_historique") params_.poids_trauma_historique = value;
        else if (key == "poids_instabilite_emotionnelle") params_.poids_instabilite_emotionnelle = value;
        else if (key == "seuil_amyghaleon") params_.seuil_amyghaleon = value;
        else if (key == "seuil_mlt_base") params_.seuil_mlt_base = value;
        else if (key == "seuil_variation_critique") params_.seuil_variation_critique = value;
        else if (key == "seuil_normal_max") params_.seuil_normal_max = value;
        else if (key == "seuil_surveillance_max") params_.seuil_surveillance_max = value;
        else if (key == "seuil_alerte_max") params_.seuil_alerte_max = value;
        else if (key == "seuil_critique_max") params_.seuil_critique_max = value;
        else if (key == "charge_cpu_max") params_.charge_cpu_max = value;
        else {
            std::cerr << "Unknown double parameter: " << key << std::endl;
        }
    }

    void ConfigManager::setParameter(const std::string& key, const std::string& value) {
        if (key == "rabbitmq_host") params_.rabbitmq_host = value;
        else if (key == "rabbitmq_username") params_.rabbitmq_username = value;
        else if (key == "rabbitmq_password") params_.rabbitmq_password = value;
        else if (key == "queue_emotional_input") params_.queue_emotional_input = value;
        else if (key == "queue_context_input") params_.queue_context_input = value;
        else if (key == "queue_consciousness_output") params_.queue_consciousness_output = value;
        else if (key == "queue_amygdaleon_output") params_.queue_amygdaleon_output = value;
        else if (key == "queue_mlt_output") params_.queue_mlt_output = value;
        else {
            std::cerr << "Unknown string parameter: " << key << std::endl;
        }
    }

    void ConfigManager::setParameter(const std::string& key, int value) {
        if (key == "rabbitmq_port") params_.rabbitmq_port = value;
        else if (key == "frequence_maj_hz") params_.frequence_maj_hz = value;
        else if (key == "latence_max_ms") params_.latence_max_ms = value;
        else {
            std::cerr << "Unknown integer parameter: " << key << std::endl;
        }
    }

    void ConfigManager::setDefaults() {
        // All defaults are already set in the struct initialization
    }
}