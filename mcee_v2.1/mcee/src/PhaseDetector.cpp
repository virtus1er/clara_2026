/**
 * @file PhaseDetector.cpp
 * @brief Implémentation du détecteur de phases émotionnelles
 * @version 2.0
 * @date 2025-12-19
 */

#include "PhaseDetector.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace mcee {

using json = nlohmann::json;

PhaseDetector::PhaseDetector(double hysteresis_margin, double min_phase_duration)
    : hysteresis_margin_(hysteresis_margin)
    , min_phase_duration_s_(min_phase_duration)
    , phase_start_time_(std::chrono::steady_clock::now())
    , phase_configs_(DEFAULT_PHASE_CONFIGS)
{
    std::cout << "[PhaseDetector] Initialisé avec hystérésis=" << hysteresis_margin_
              << ", durée min=" << min_phase_duration_s_ << "s\n";
}

Phase PhaseDetector::detectPhase(const EmotionalState& state) {
    // 1. Vérifier urgence (court-circuit toutes les règles)
    if (checkEmergencyTransition(state)) {
        if (current_phase_ != Phase::PEUR) {
            transitionTo(Phase::PEUR, "URGENCE");
        }
        return current_phase_;
    }

    // 2. Calculer les scores de chaque phase
    auto scores = computePhaseScores(state);

    // 3. Trouver la meilleure phase
    Phase best_phase = Phase::SERENITE;
    double best_score = -1.0;
    
    for (const auto& [phase, score] : scores) {
        // Pondérer par la priorité
        double weighted_score = score * (1.0 + phase_configs_.at(phase).priority * 0.1);
        if (weighted_score > best_score) {
            best_score = weighted_score;
            best_phase = phase;
        }
    }

    // 4. Vérifier si on peut changer de phase
    if (best_phase != current_phase_) {
        if (canTransition() && applyHysteresis(scores, best_phase)) {
            transitionTo(best_phase, "SCORE");
        }
    }

    return current_phase_;
}

std::unordered_map<Phase, double> PhaseDetector::computePhaseScores(
    const EmotionalState& state) const 
{
    std::unordered_map<Phase, double> scores;

    // Extraire les émotions clés
    double peur = state.getEmotion("Peur");
    double horreur = state.getEmotion("Horreur");
    double anxiete = state.getEmotion("Anxiété");
    double joie = state.getEmotion("Joie");
    double calme = state.getEmotion("Calme");
    double tristesse = state.getEmotion("Tristesse");
    double degout = state.getEmotion("Dégoût");
    double confusion = state.getEmotion("Confusion");
    double interet = state.getEmotion("Intérêt");
    double fascination = state.getEmotion("Fascination");
    double excitation = state.getEmotion("Excitation");
    double satisfaction = state.getEmotion("Satisfaction");

    // Score SÉRÉNITÉ: calme élevé, émotions équilibrées
    scores[Phase::SERENITE] = calme * 0.4 + satisfaction * 0.3 
                            + (1.0 - anxiete) * 0.2 + (1.0 - peur) * 0.1;

    // Score JOIE: joie et excitation élevées
    scores[Phase::JOIE] = joie * 0.5 + excitation * 0.25 
                        + satisfaction * 0.15 + (1.0 - tristesse) * 0.1;

    // Score EXPLORATION: intérêt et fascination élevés
    scores[Phase::EXPLORATION] = interet * 0.35 + fascination * 0.35 
                               + excitation * 0.2 + (1.0 - peur) * 0.1;

    // Score ANXIÉTÉ: anxiété élevée sans peur extrême
    scores[Phase::ANXIETE] = anxiete * 0.5 + confusion * 0.2 
                           + (1.0 - calme) * 0.2 + peur * 0.1;

    // Score PEUR: peur et horreur élevées
    scores[Phase::PEUR] = peur * 0.4 + horreur * 0.4 
                        + anxiete * 0.15 + (1.0 - calme) * 0.05;

    // Score TRISTESSE: tristesse élevée
    double nostalgie = state.getEmotion("Nostalgie");
    scores[Phase::TRISTESSE] = tristesse * 0.5 + nostalgie * 0.25 
                             + (1.0 - joie) * 0.15 + (1.0 - excitation) * 0.1;

    // Score DÉGOÛT: dégoût élevé
    scores[Phase::DEGOUT] = degout * 0.6 + (1.0 - satisfaction) * 0.2 
                          + horreur * 0.2;

    // Score CONFUSION: confusion élevée
    scores[Phase::CONFUSION] = confusion * 0.5 + anxiete * 0.2 
                             + (1.0 - calme) * 0.2 + (1.0 - satisfaction) * 0.1;

    // Normaliser les scores
    double maxScore = 0.0;
    for (const auto& [_, score] : scores) {
        maxScore = std::max(maxScore, score);
    }
    
    if (maxScore > 0.0) {
        for (auto& [_, score] : scores) {
            score /= maxScore;
        }
    }

    return scores;
}

bool PhaseDetector::checkEmergencyTransition(const EmotionalState& state) const {
    double peur = state.getEmotion("Peur");
    double horreur = state.getEmotion("Horreur");

    // Transition immédiate si seuils d'urgence dépassés
    return (peur > EmergencyThresholds::PEUR_IMMEDIATE || 
            horreur > EmergencyThresholds::HORREUR_IMMEDIATE);
}

bool PhaseDetector::canTransition() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(now - phase_start_time_).count();
    return duration >= min_phase_duration_s_;
}

bool PhaseDetector::applyHysteresis(
    const std::unordered_map<Phase, double>& scores,
    Phase best_phase) const 
{
    double current_score = scores.at(current_phase_);
    double best_score = scores.at(best_phase);

    // La nouvelle phase doit dépasser l'actuelle + marge d'hystérésis
    return (best_score > current_score + hysteresis_margin_);
}

void PhaseDetector::transitionTo(Phase new_phase, const std::string& reason) {
    auto now = std::chrono::steady_clock::now();
    double duration = std::chrono::duration<double>(now - phase_start_time_).count();

    previous_phase_ = current_phase_;
    current_phase_ = new_phase;
    phase_start_time_ = now;
    transition_count_++;

    std::cout << "[PhaseDetector] Transition: " << phaseToString(previous_phase_)
              << " -> " << phaseToString(current_phase_)
              << " (raison: " << reason 
              << ", durée précédente: " << std::fixed << std::setprecision(1) 
              << duration << "s)\n";

    if (on_transition_) {
        on_transition_(previous_phase_, current_phase_, duration);
    }
}

void PhaseDetector::forceTransition(Phase phase, const std::string& reason) {
    transitionTo(phase, "FORCE: " + reason);
}

const PhaseConfig& PhaseDetector::getCurrentConfig() const {
    return phase_configs_.at(current_phase_);
}

double PhaseDetector::getPhaseDuration() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - phase_start_time_).count();
}

void PhaseDetector::setTransitionCallback(TransitionCallback callback) {
    on_transition_ = std::move(callback);
}

bool PhaseDetector::loadConfig(const std::string& config_path) {
    try {
        std::ifstream file(config_path);
        if (!file) {
            std::cerr << "[PhaseDetector] Fichier config introuvable: " << config_path << "\n";
            return false;
        }

        json config;
        file >> config;

        // Charger les paramètres du détecteur
        if (config.contains("phase_detector")) {
            auto& pd = config["phase_detector"];
            if (pd.contains("hysteresis_margin")) {
                hysteresis_margin_ = pd["hysteresis_margin"].get<double>();
            }
            if (pd.contains("min_phase_duration")) {
                min_phase_duration_s_ = pd["min_phase_duration"].get<double>();
            }
        }

        // Charger les configurations de phases
        if (config.contains("phases")) {
            for (auto& [name, cfg] : config["phases"].items()) {
                Phase phase = stringToPhase(name);
                
                PhaseConfig pc;
                pc.alpha = cfg.value("alpha", 0.3);
                pc.beta = cfg.value("beta", 0.2);
                pc.gamma = cfg.value("gamma", 0.1);
                pc.delta = cfg.value("delta", 0.4);
                pc.theta = cfg.value("theta", 0.1);
                pc.amyghaleon_threshold = cfg.value("amyghaleon_threshold", 0.85);
                pc.memory_consolidation = cfg.value("memory_consolidation", 0.5);
                pc.learning_rate = cfg.value("learning_rate", 1.0);
                pc.focus = cfg.value("focus", 0.5);
                pc.priority = cfg.value("priority", 1);

                phase_configs_[phase] = pc;
            }
        }

        std::cout << "[PhaseDetector] Configuration chargée depuis " << config_path << "\n";
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[PhaseDetector] Erreur chargement config: " << e.what() << "\n";
        return false;
    }
}

} // namespace mcee
