/**
 * @file simple_example.cpp
 * @brief Exemple simplifié d'intégration DreamEngine avec PatternMatcher/Amyghaleon
 * 
 * Cet exemple montre comment le DreamEngine s'intègre avec:
 * - PatternMatcher (simulé) : fournit le pattern émotionnel actif
 * - Amyghaleon (simulé) : peut interrompre le rêve en cas d'urgence
 * 
 * Pas de dépendance RabbitMQ - tout est simulé localement.
 */

#include "DreamEngine.hpp"

#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <random>
#include <functional>

using namespace MCEE;

// ═══════════════════════════════════════════════════════════════════════════
// SIMULATEURS DES COMPOSANTS MCEE
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Simule le PatternMatcher de mcee_final
 * 
 * Dans le vrai système, tu appellerais:
 *   auto result = patternMatcher->match();
 *   return result.pattern_name;
 */
class SimulatedPatternMatcher {
public:
    std::string getCurrentPattern() {
        // Simuler des transitions de patterns réalistes
        tick_++;
        
        // Cycle: SERENITE → JOIE → EXPLORATION → retour SERENITE
        // Avec possibilité d'ANXIETE/PEUR si intensité élevée
        
        if (intensityLevel_ > 0.7) {
            return "ANXIETE";
        }
        
        if (intensityLevel_ > 0.85) {
            return "PEUR";
        }
        
        int phase = (tick_ / 20) % 4;
        switch (phase) {
            case 0: return "SERENITE";
            case 1: return "JOIE";
            case 2: return "EXPLORATION";
            case 3: return "SERENITE";
            default: return "SERENITE";
        }
    }
    
    void setIntensityLevel(double level) {
        intensityLevel_ = level;
    }
    
private:
    int tick_ = 0;
    double intensityLevel_ = 0.3;
};

/**
 * @brief Simule Amyghaleon de mcee_final
 * 
 * Dans le vrai système, tu appellerais:
 *   return amyghaleon->checkEmergency(state, activeMemories, threshold);
 */
class SimulatedAmyghaleon {
public:
    bool checkEmergency() {
        // Simuler une urgence rare (2% de chance)
        std::uniform_real_distribution<> dist(0.0, 1.0);
        return dist(rng_) < 0.02;
    }
    
    void forceEmergency(bool state) {
        forcedEmergency_ = state;
    }
    
private:
    std::mt19937 rng_{std::random_device{}()};
    bool forcedEmergency_ = false;
};

/**
 * @brief Génère un état émotionnel simulé
 */
class SimulatedEmotionalState {
public:
    std::array<double, 24> getEmotions() {
        std::array<double, 24> emotions{};
        std::uniform_real_distribution<> dist(0.05, 0.3);
        
        // Remplir avec des valeurs de base
        for (size_t i = 0; i < 24; ++i) {
            emotions[i] = dist(rng_);
        }
        
        // Une émotion dominante
        std::uniform_int_distribution<> indexDist(0, 23);
        emotions[indexDist(rng_)] = 0.6 + dist(rng_);
        
        return emotions;
    }
    
private:
    std::mt19937 rng_{std::random_device{}()};
};

/**
 * @brief Génère des souvenirs de test réalistes
 */
Memory createRealisticMemory(int index, std::mt19937& rng) {
    std::uniform_real_distribution<> dist(0.0, 1.0);
    
    static const std::vector<std::string> types = {"episodic", "semantic", "procedural", "autobiographic"};
    static const std::vector<std::string> contexts = {"travail", "maison", "social", "loisir", "apprentissage"};
    static const std::vector<std::string> interlocuteurs = {"Alice", "Bob", "Charlie", "System", ""};
    
    Memory memory;
    memory.id = "souvenir_" + std::to_string(index);
    memory.type = types[rng() % types.size()];
    memory.isSocial = dist(rng) > 0.6;
    memory.interlocuteur = memory.isSocial ? interlocuteurs[rng() % (interlocuteurs.size() - 1)] : "";
    memory.contexte = contexts[rng() % contexts.size()];
    memory.feedback = dist(rng) * 0.8 + 0.1;  // 0.1 à 0.9
    memory.usageCount = 1 + (rng() % 10);
    memory.decisionalInfluence = dist(rng) * 0.7;
    memory.isTrauma = dist(rng) < 0.05;  // 5% de traumas
    memory.timestamp = std::chrono::steady_clock::now();
    
    // Vecteur émotionnel
    for (size_t i = 0; i < 24; ++i) {
        memory.emotionalVector[i] = dist(rng) * 0.4;
    }
    // Émotion dominante
    memory.emotionalVector[rng() % 24] = 0.5 + dist(rng) * 0.4;
    
    return memory;
}

// ═══════════════════════════════════════════════════════════════════════════
// MAIN
// ═══════════════════════════════════════════════════════════════════════════

int main() {
    std::cout << "═══════════════════════════════════════════════════════════\n";
    std::cout << "  DreamEngine - Simulation PatternMatcher + Amyghaleon\n";
    std::cout << "═══════════════════════════════════════════════════════════\n\n";
    
    std::mt19937 rng{std::random_device{}()};
    
    // ─────────────────────────────────────────────────────────────────────
    // 1. CRÉER LES COMPOSANTS
    // ─────────────────────────────────────────────────────────────────────
    
    std::cout << "[1] Création des composants...\n";
    
    // Configuration avec cycle court pour la démo
    DreamConfig config;
    config.cyclePeriod_s = 20.0;               // 20 secondes
    config.minTimeSinceLastDream_s = 15.0;     // Rêve possible après 15s
    config.maxEmotionalActivityForDream = 0.5; // Seuil permissif
    
    DreamEngine engine(config);
    
    SimulatedPatternMatcher patternMatcher;
    SimulatedAmyghaleon amyghaleon;
    SimulatedEmotionalState emotionalState;
    
    std::cout << "  ✓ DreamEngine créé (cycle=" << config.cyclePeriod_s << "s)\n";
    std::cout << "  ✓ PatternMatcher simulé\n";
    std::cout << "  ✓ Amyghaleon simulé\n";
    
    // ─────────────────────────────────────────────────────────────────────
    // 2. CONFIGURER LES CALLBACKS NEO4J (simulés)
    // ─────────────────────────────────────────────────────────────────────
    
    std::cout << "\n[2] Configuration des callbacks Neo4j (simulés)...\n";
    
    engine.setStateChangeCallback([](DreamState oldState, DreamState newState) {
        std::cout << "\n  ▶ [STATE] " << dreamStateToString(oldState) 
                  << " → " << dreamStateToString(newState) << "\n";
    });
    
    engine.setNeo4jConsolidateCallback([](const Memory& memory) {
        std::cout << "    [NEO4J] MERGE (m:Memory {id: '" << memory.id 
                  << "', score: " << std::fixed << std::setprecision(2) 
                  << memory.consolidationScore << ", type: 'MLT'})\n";
    });
    
    engine.setNeo4jCreateEdgeCallback([](const MemoryEdge& edge) {
        std::cout << "    [NEO4J] MERGE (" << edge.sourceId << ")-[:" 
                  << edge.relationType << " {w:" << std::fixed << std::setprecision(2) 
                  << edge.weight << "}]->(" << edge.targetId << ")\n";
    });
    
    engine.setNeo4jDeleteCallback([](const std::string& id) {
        std::cout << "    [NEO4J] DELETE (m:Memory {id: '" << id << "'})\n";
    });
    
    // ─────────────────────────────────────────────────────────────────────
    // 3. AJOUTER DES SOUVENIRS
    // ─────────────────────────────────────────────────────────────────────
    
    std::cout << "\n[3] Ajout de souvenirs à la MCT...\n";
    
    for (int i = 1; i <= 8; ++i) {
        Memory memory = createRealisticMemory(i, rng);
        engine.addMemoryToMCT(memory);
        
        std::cout << "    + " << memory.id << " (" << memory.type << ")"
                  << (memory.isSocial ? " [social:" + memory.interlocuteur + "]" : "")
                  << (memory.isTrauma ? " [TRAUMA]" : "")
                  << " ctx:" << memory.contexte
                  << "\n";
    }
    
    std::cout << "\n  MCT contient " << engine.getMCTMemories().size() << " souvenirs\n";
    
    // ─────────────────────────────────────────────────────────────────────
    // 4. BOUCLE DE SIMULATION
    // ─────────────────────────────────────────────────────────────────────
    
    std::cout << "\n[4] Démarrage de la simulation...\n";
    std::cout << "    (Le rêve démarre automatiquement après ~15s d'éveil calme)\n\n";
    
    int tick = 0;
    bool dreamCompleted = false;
    
    while (!dreamCompleted && tick < 200) {
        // Obtenir l'état actuel des composants MCEE
        std::string activePattern = patternMatcher.getCurrentPattern();
        std::array<double, 24> currentEmotions = emotionalState.getEmotions();
        bool amyghaleonAlert = amyghaleon.checkEmergency();
        
        // Mettre à jour le DreamEngine
        engine.update(currentEmotions, activePattern, amyghaleonAlert);
        
        // Afficher le statut toutes les 10 itérations
        if (tick % 10 == 0) {
            auto state = engine.getCurrentState();
            double cycleProgress = engine.getCycleProgress() * 100;
            double timeSinceDream = engine.getTimeSinceLastDream_s();
            
            std::cout << "  [T=" << std::setw(3) << tick << "] "
                      << "Pattern: " << std::setw(12) << activePattern
                      << " | État: " << std::setw(18) << dreamStateToString(state)
                      << " | Cycle: " << std::setw(3) << static_cast<int>(cycleProgress) << "%"
                      << " | Depuis rêve: " << std::setw(4) << static_cast<int>(timeSinceDream) << "s"
                      << (amyghaleonAlert ? " ⚠️ ALERTE" : "")
                      << "\n";
        }
        
        // Vérifier si un cycle de rêve a été complété
        auto stats = engine.getStats();
        if (stats.totalCyclesCompleted > 0) {
            dreamCompleted = true;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        tick++;
    }
    
    // ─────────────────────────────────────────────────────────────────────
    // 5. RÉSULTATS
    // ─────────────────────────────────────────────────────────────────────
    
    auto stats = engine.getStats();
    
    std::cout << "\n═══════════════════════════════════════════════════════════\n";
    std::cout << "  RÉSULTATS DE LA SIMULATION\n";
    std::cout << "═══════════════════════════════════════════════════════════\n";
    std::cout << "  Cycles de rêve complétés:  " << stats.totalCyclesCompleted << "\n";
    std::cout << "  Souvenirs consolidés:      " << stats.totalMemoriesConsolidated << "\n";
    std::cout << "  Souvenirs oubliés:         " << stats.totalMemoriesForgotten << "\n";
    std::cout << "  Arêtes créées:             " << stats.totalEdgesCreated << "\n";
    std::cout << "  Interruptions Amyghaleon:  " << stats.totalInterruptions << "\n";
    std::cout << "  Score moyen consolidation: " << std::fixed << std::setprecision(3) 
              << stats.averageConsolidationScore << "\n";
    std::cout << "  MCT restante:              " << engine.getMCTMemories().size() << " souvenirs\n";
    std::cout << "═══════════════════════════════════════════════════════════\n";
    
    // ─────────────────────────────────────────────────────────────────────
    // 6. TEST INTERRUPTION AMYGHALEON
    // ─────────────────────────────────────────────────────────────────────
    
    std::cout << "\n[5] Test d'interruption Amyghaleon...\n";
    
    // Ajouter plus de souvenirs pour un nouveau cycle
    for (int i = 9; i <= 15; ++i) {
        engine.addMemoryToMCT(createRealisticMemory(i, rng));
    }
    std::cout << "    + 7 nouveaux souvenirs ajoutés\n";
    
    // Forcer le démarrage du rêve
    engine.forceDreamStart();
    std::cout << "    Rêve forcé: " << dreamStateToString(engine.getCurrentState()) << "\n";
    
    // Simuler quelques ticks
    for (int i = 0; i < 5; ++i) {
        engine.update(emotionalState.getEmotions(), "SERENITE", false);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Déclencher une alerte Amyghaleon
    std::cout << "    ⚠️ Alerte Amyghaleon déclenchée!\n";
    engine.update(emotionalState.getEmotions(), "PEUR", true);
    
    std::cout << "    État après alerte: " << dreamStateToString(engine.getCurrentState()) << "\n";
    
    auto finalStats = engine.getStats();
    std::cout << "    Interruptions totales: " << finalStats.totalInterruptions << "\n";
    
    std::cout << "\n═══════════════════════════════════════════════════════════\n";
    std::cout << "  Simulation terminée avec succès!\n";
    std::cout << "═══════════════════════════════════════════════════════════\n";
    
    return 0;
}
