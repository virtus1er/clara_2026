/**
 * @file MemoryManager.cpp
 * @brief Implémentation du gestionnaire de mémoire MCEE
 * @version 2.0
 * @date 2025-12-19
 */

#include "MemoryManager.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>
#include <iomanip>

namespace mcee {

MemoryManager::MemoryManager() {
    std::cout << "[MemoryManager] Gestionnaire de mémoire initialisé (mode local)\n";
}

std::vector<Memory> MemoryManager::queryRelevantMemories(
    Phase phase,
    const EmotionalState& state,
    size_t max_count) 
{
    std::vector<Memory> result;

    // Filtrer et trier selon la phase
    std::vector<std::pair<double, size_t>> scored_indices;

    for (size_t i = 0; i < memories_.size(); ++i) {
        double score = 0.0;
        const auto& mem = memories_[i];

        switch (phase) {
            case Phase::PEUR:
                // Priorité aux traumas et souvenirs de peur
                if (mem.is_trauma) {
                    score = 1.0 + mem.intensity;
                } else if (mem.dominant == "Peur" || mem.dominant == "Horreur" || 
                          mem.dominant == "Anxiété") {
                    score = 0.8 * mem.intensity;
                }
                break;

            case Phase::JOIE:
                // Priorité aux souvenirs positifs
                if (mem.valence > 0.5) {
                    if (mem.dominant == "Joie" || mem.dominant == "Satisfaction" || 
                        mem.dominant == "Excitation") {
                        score = mem.valence * mem.intensity;
                    }
                }
                break;

            case Phase::ANXIETE:
                // Souvenirs négatifs récents
                if (mem.dominant == "Anxiété" || mem.dominant == "Peur" || 
                    mem.dominant == "Confusion") {
                    score = mem.activation_count * 0.1 + mem.intensity;
                }
                break;

            default:
                // Requête équilibrée basée sur le poids
                score = mem.weight * computeEmotionalMatch(state, mem);
                break;
        }

        if (score > 0.0) {
            scored_indices.emplace_back(score, i);
        }
    }

    // Trier par score décroissant
    std::sort(scored_indices.begin(), scored_indices.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    // Collecter les résultats
    size_t count = std::min(max_count, scored_indices.size());
    result.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        result.push_back(memories_[scored_indices[i].second]);
    }

    return result;
}

std::array<double, NUM_EMOTIONS> MemoryManager::computeMemoryInfluences(
    const std::vector<Memory>& memories,
    double delta_coeff) const 
{
    std::array<double, NUM_EMOTIONS> influences{};
    influences.fill(0.0);

    if (memories.empty()) {
        return influences;
    }

    // Accumuler l'influence de chaque souvenir
    for (const auto& mem : memories) {
        double weight = mem.weight * mem.activation * delta_coeff;
        
        for (size_t i = 0; i < NUM_EMOTIONS; ++i) {
            influences[i] += mem.emotions[i] * weight;
        }
    }

    // Normaliser par le nombre de souvenirs
    double norm = static_cast<double>(memories.size());
    for (auto& inf : influences) {
        inf = std::clamp(inf / norm, 0.0, 1.0);
    }

    return influences;
}

Memory MemoryManager::recordMemory(
    const EmotionalState& state,
    Phase phase,
    const std::string& context) 
{
    Memory mem;
    mem.name = context;
    mem.emotions = state.emotions;
    
    auto [dominant_name, dominant_value] = state.getDominant();
    mem.dominant = dominant_name;
    mem.valence = state.getValence();
    mem.intensity = state.getMeanIntensity();
    mem.weight = computeInitialWeight(phase, mem.intensity, mem.valence);
    mem.activation = mem.intensity;
    mem.is_trauma = false;
    mem.phase_at_creation = phase;
    mem.last_activated = std::chrono::system_clock::now();
    mem.activation_count = 1;

    memories_.push_back(mem);

    std::cout << "[MemoryManager] Souvenir enregistré: \"" << context 
              << "\" (phase=" << phaseToString(phase) 
              << ", dominant=" << dominant_name 
              << ", poids=" << std::fixed << std::setprecision(2) << mem.weight << ")\n";

    return mem;
}

std::optional<Memory> MemoryManager::createPotentialTrauma(const EmotionalState& state) {
    // Vérifier les critères de trauma
    double peur = state.getEmotion("Peur");
    double horreur = state.getEmotion("Horreur");
    double intensity = state.getMeanIntensity();
    double valence = state.getValence();

    // Critères: intensité > 0.85 ET valence < 0.2
    if (intensity > 0.85 && valence < 0.2) {
        Memory trauma;
        trauma.name = "Trauma_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        trauma.emotions = state.emotions;
        
        auto [dominant_name, dominant_value] = state.getDominant();
        trauma.dominant = dominant_name;
        trauma.valence = valence;
        trauma.intensity = intensity;
        trauma.weight = std::min(1.0, 0.7 + intensity * 0.3);  // Poids élevé
        trauma.activation = intensity;
        trauma.is_trauma = true;
        trauma.phase_at_creation = Phase::PEUR;
        trauma.last_activated = std::chrono::system_clock::now();
        trauma.activation_count = 1;

        memories_.push_back(trauma);

        std::cout << "[MemoryManager] ⚠ TRAUMA créé (intensité=" 
                  << std::fixed << std::setprecision(3) << intensity << ")\n";

        return trauma;
    }

    return std::nullopt;
}

double MemoryManager::updateActivation(Memory& memory, const EmotionalState& current_state) {
    // Formule d'activation:
    // A(Si) = forget(Si,t) × (1 + R(Si)) × Match(Si, E_current)
    
    double forget_factor = computeForgetFactor(memory);
    double reinforcement = memory.is_trauma ? 1.5 : 1.0;  // Traumas renforcés
    double match = computeEmotionalMatch(current_state, memory);

    memory.activation = forget_factor * reinforcement * match;
    memory.activation = std::clamp(memory.activation, 0.0, 1.0);
    
    if (memory.activation > 0.3) {
        memory.activation_count++;
        memory.last_activated = std::chrono::system_clock::now();
    }

    return memory.activation;
}

std::string MemoryManager::shouldConsolidate(
    const Memory& memory,
    Phase phase_at_creation) const 
{
    double intensity = memory.intensity;
    double valence = memory.valence;

    switch (phase_at_creation) {
        case Phase::PEUR:
            if (intensity > 0.85 && valence < 0.2) {
                return "TRAUMA";
            }
            return "CONSOLIDATE_STRONG";

        case Phase::JOIE:
            if (valence > 0.7 && intensity > 0.6) {
                return "CONSOLIDATE_STRONG";
            }
            return "CONSOLIDATE_NORMAL";

        case Phase::ANXIETE:
            if (valence < 0.3) {
                return "CONSOLIDATE_STRONG";
            }
            return "FORGET";

        default:
            if (intensity > 0.5) {
                return "CONSOLIDATE_NORMAL";
            }
            return "FORGET";
    }
}

size_t MemoryManager::getTraumaCount() const {
    return std::count_if(memories_.begin(), memories_.end(),
                         [](const Memory& m) { return m.is_trauma; });
}

void MemoryManager::applyForget(double decay_factor) {
    for (auto& mem : memories_) {
        // Les traumas ont un taux d'oubli réduit
        double effective_decay = mem.is_trauma ? decay_factor * 0.1 : decay_factor;
        mem.weight *= (1.0 - effective_decay);
    }

    // Supprimer les souvenirs avec poids trop faible (sauf traumas)
    memories_.erase(
        std::remove_if(memories_.begin(), memories_.end(),
                       [](const Memory& m) { return !m.is_trauma && m.weight < 0.01; }),
        memories_.end()
    );
}

void MemoryManager::setNeo4jConfig(
    const std::string& uri,
    const std::string& user,
    const std::string& password) 
{
    neo4j_uri_ = uri;
    neo4j_user_ = user;
    neo4j_password_ = password;
    
    // TODO: Établir connexion Neo4j
    std::cout << "[MemoryManager] Configuration Neo4j définie (connexion non implémentée)\n";
}

double MemoryManager::computeEmotionalMatch(
    const EmotionalState& state,
    const Memory& memory) const 
{
    // Similarité cosinus entre vecteurs d'émotions
    double dot_product = 0.0;
    double norm_state = 0.0;
    double norm_mem = 0.0;

    for (size_t i = 0; i < NUM_EMOTIONS; ++i) {
        dot_product += state.emotions[i] * memory.emotions[i];
        norm_state += state.emotions[i] * state.emotions[i];
        norm_mem += memory.emotions[i] * memory.emotions[i];
    }

    norm_state = std::sqrt(norm_state);
    norm_mem = std::sqrt(norm_mem);

    if (norm_state < 1e-6 || norm_mem < 1e-6) {
        return 0.0;
    }

    return dot_product / (norm_state * norm_mem);
}

double MemoryManager::computeForgetFactor(const Memory& memory) const {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::hours>(
        now - memory.last_activated
    ).count();

    // Décroissance exponentielle avec demi-vie de 720 heures (30 jours)
    double half_life = 720.0;
    double forget = std::exp(-0.693 * duration / half_life);

    // Les traumas résistent à l'oubli
    if (memory.is_trauma) {
        forget = std::max(forget, 0.5);
    }

    return forget;
}

double MemoryManager::computeInitialWeight(
    Phase phase,
    double intensity,
    double valence) const 
{
    switch (phase) {
        case Phase::PEUR:
            return std::min(1.0, 0.7 + intensity * 0.3);

        case Phase::JOIE:
            if (valence > 0.6) {
                return std::min(1.0, 0.6 + valence * 0.4);
            }
            return 0.3;

        case Phase::ANXIETE:
            if (valence < 0.3) {
                return std::min(1.0, 0.5 + (1.0 - valence) * 0.5);
            }
            return 0.2;

        default:
            return 0.5;
    }
}

} // namespace mcee
