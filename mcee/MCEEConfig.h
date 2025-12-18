//===== MCEEConfig.h - SEULEMENT LES DECLARATIONS =====
#pragma once
#include "MCEETypes.h"
#include <string>

namespace MCEE {
    class ConfigManager {
    public:
        static ConfigManager& getInstance();
        bool loadFromFile(const std::string& configPath);
        const MCEEParameters& getParameters() const { return params_; }
        void setParameter(const std::string& key, double value);
        void setParameter(const std::string& key, const std::string& value);
        void setParameter(const std::string& key, int value);

    private:
        ConfigManager() = default;
        MCEEParameters params_;
        void setDefaults();
        void parseConfigLine(const std::string& key, const std::string& value);
    };
}