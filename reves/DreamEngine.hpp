#pragma once

#include "DreamState.hpp"
#include "DreamConfig.hpp"

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <chrono>
#include <mutex>
#include <random>
#include <optional>
#include <array>

namespace MCEE {

// Forward declarations (à adapter selon ton architecture)
class MCT;          // Mémoire Court Terme
class MLT;          // Mémoire Long Terme
class PatternMatcher;
class Amyghaleon;

/**
 * Structure représentant un souvenir en MCT
 */
struct Memory {
    std::string id;
    std::string type;                    // "episodic", "semantic", "procedural", "autobiographic"
    bool isSocial = false;
    std::string interlocuteur;
    std::string contexte;
    
    // Vecteur émotionnel (24 dimensions selon MCEE)
    std::array<double, 24> emotionalVector{};
    
    // Métadonnées pour le scoring
    double feedback = 0.0;               // Signal externe/interne
    int usageCount = 0;                  // Nombre de réactivations
    double decisionalInfluence = 0.0;    // Lien avec décisions prises
    bool isTrauma = false;               // Marquage trauma
    
    std::chrono::steady_clock::time_point timestamp;
    
    // Score calculé lors du scan
    double consolidationScore = 0.0;
};

/**
 * Structure pour les arêtes du graphe de mémoire
 */
struct MemoryEdge {
    std::string sourceId;
    std::string targetId;
    double weight = 1.0;
    std::string relationType;            // "temporal", "emotional", "semantic", "causal"
    std::chrono::steady_clock::time_point lastActivation;
};

/**
 * Lien causal mot→émotion (issu du MCTGraph)
 */
struct CausalLink {
    std::string wordId;
    std::string wordLemma;
    std::string wordPos;                  // NOUN, VERB, ADJ...
    std::string emotionId;
    std::string dominantEmotion;          // Émotion dominante déclenchée
    double causalStrength;                // Force du lien [0, 1]
    double temporalDistanceMs;            // Délai mot→émotion
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * Nœud mot enrichi (issu du MCTGraph snapshot)
 */
struct WordNodeSnapshot {
    std::string id;
    std::string lemma;
    std::string pos;
    std::string sentenceId;
    double sentimentScore = 0.0;
    bool isNegation = false;
    bool isIntensifier = false;
};

/**
 * Statistiques causales globales
 */
struct CausalStats {
    std::vector<std::string> topTriggerWords;  // Mots causant le plus d'émotions
    std::string mostFrequentEmotion;
    double averageEmotionIntensity = 0.0;
    size_t totalCausalEdges = 0;
    double graphDensity = 0.0;
};

/**
 * Callback pour notifier les changements d'état
 */
using DreamStateCallback = std::function<void(DreamState oldState, DreamState newState)>;

/**
 * Callback pour les opérations Neo4j (consolidation MLT)
 */
using Neo4jConsolidateCallback = std::function<void(const Memory& memory)>;
using Neo4jReinforceCallback = std::function<void(const MemoryEdge& edge, double newWeight)>;
using Neo4jDeleteCallback = std::function<void(const std::string& memoryId)>;
using Neo4jCreateEdgeCallback = std::function<void(const MemoryEdge& edge)>;

/**
 * ═══════════════════════════════════════════════════════════════════════════
 * DreamEngine - Module de consolidation nocturne/off-line
 * 
 * Gère le cycle circadien simulé (T=12h) et orchestre :
 * - La transition éveil → rêve
 * - Les 4 phases du rêve (scan, consolidation, exploration, nettoyage)
 * - L'interruption par Amyghaleon si urgence
 * - Le scoring Csocial(t) pour la consolidation sélective
 * ═══════════════════════════════════════════════════════════════════════════
 */
class DreamEngine {
public:
    explicit DreamEngine(const DreamConfig& config = DreamConfig{});
    ~DreamEngine() = default;
    
    // Non-copiable, mais déplaçable
    DreamEngine(const DreamEngine&) = delete;
    DreamEngine& operator=(const DreamEngine&) = delete;
    DreamEngine(DreamEngine&&) = default;
    DreamEngine& operator=(DreamEngine&&) = default;
    
    // ═══════════════════════════════════════════════════════════
    // CYCLE PRINCIPAL
    // ═══════════════════════════════════════════════════════════
    
    /**
     * Met à jour le cycle - à appeler régulièrement (tick)
     * @param currentEmotionalState Vecteur émotionnel actuel (24 dim)
     * @param activePattern Pattern émotionnel actif (ex: "SERENITE", "PEUR")
     * @param amyghaleonAlert True si Amyghaleon signale une urgence
     */
    void update(const std::array<double, 24>& currentEmotionalState,
                const std::string& activePattern,
                bool amyghaleonAlert = false);
    
    /**
     * Force le démarrage du mode rêve (pour tests ou déclenchement manuel)
     */
    void forceDreamStart();
    
    /**
     * Force l'interruption du rêve
     */
    void interruptDream();
    
    // ═══════════════════════════════════════════════════════════
    // ACCÈS À L'ÉTAT
    // ═══════════════════════════════════════════════════════════
    
    [[nodiscard]] DreamState getCurrentState() const;
    [[nodiscard]] double getCycleProgress() const;          // 0.0 à 1.0
    [[nodiscard]] double getDreamPhaseProgress() const;     // 0.0 à 1.0 dans la phase actuelle
    [[nodiscard]] double getTimeSinceLastDream_s() const;
    [[nodiscard]] bool canStartDream() const;
    
    // ═══════════════════════════════════════════════════════════
    // GESTION DE LA MCT (entrée)
    // ═══════════════════════════════════════════════════════════
    
    /**
     * Ajoute un souvenir à la MCT (appelé pendant l'éveil)
     */
    void addMemoryToMCT(const Memory& memory);
    
    /**
     * Récupère les souvenirs en attente dans la MCT
     */
    [[nodiscard]] const std::vector<Memory>& getMCTMemories() const;
    
    /**
     * Vide la MCT (après consolidation complète)
     */
    void clearMCT();

    // ═══════════════════════════════════════════════════════════
    // GESTION DES LIENS CAUSAUX (MCTGraph)
    // ═══════════════════════════════════════════════════════════

    /**
     * Traite un snapshot MCTGraph enrichi
     * Extrait les liens causaux mot→émotion
     */
    void processMCTGraphSnapshot(const std::vector<WordNodeSnapshot>& words,
                                  const std::vector<CausalLink>& causalLinks,
                                  const CausalStats& stats);

    /**
     * Récupère les liens causaux actifs
     */
    [[nodiscard]] const std::vector<CausalLink>& getCausalLinks() const;

    /**
     * Récupère les mots déclencheurs (top trigger words)
     */
    [[nodiscard]] const std::vector<std::string>& getTopTriggerWords() const;

    /**
     * Vide les liens causaux
     */
    void clearCausalLinks();
    
    // ═══════════════════════════════════════════════════════════
    // CALLBACKS
    // ═══════════════════════════════════════════════════════════
    
    void setStateChangeCallback(DreamStateCallback callback);
    void setNeo4jConsolidateCallback(Neo4jConsolidateCallback callback);
    void setNeo4jReinforceCallback(Neo4jReinforceCallback callback);
    void setNeo4jDeleteCallback(Neo4jDeleteCallback callback);
    void setNeo4jCreateEdgeCallback(Neo4jCreateEdgeCallback callback);
    
    // ═══════════════════════════════════════════════════════════
    // CONFIGURATION
    // ═══════════════════════════════════════════════════════════
    
    void setConfig(const DreamConfig& config);
    [[nodiscard]] const DreamConfig& getConfig() const;
    
    // ═══════════════════════════════════════════════════════════
    // STATISTIQUES
    // ═══════════════════════════════════════════════════════════
    
    struct Stats {
        int totalCyclesCompleted = 0;
        int totalMemoriesConsolidated = 0;
        int totalMemoriesForgotten = 0;
        int totalEdgesCreated = 0;
        int totalInterruptions = 0;
        double averageConsolidationScore = 0.0;
    };
    
    [[nodiscard]] Stats getStats() const;
    void resetStats();

private:
    // ═══════════════════════════════════════════════════════════
    // PHASES DU RÊVE
    // ═══════════════════════════════════════════════════════════
    
    void executeScanPhase();
    void executeConsolidatePhase();
    void executeExplorePhase();
    void executeCleanupPhase();

    /**
     * Génère des associations basées sur les liens causaux
     * Relie les souvenirs partageant des mots déclencheurs communs
     */
    void exploreCausalAssociations();
    
    // ═══════════════════════════════════════════════════════════
    // CALCULS
    // ═══════════════════════════════════════════════════════════
    
    /**
     * Calcule le score de consolidation sociale
     * Csocial(t) = ρ·|Ecurrent - Esouvenir| + λ·Feedback + η·Usage + θ·Influence
     */
    double calculateConsolidationScore(const Memory& memory,
                                       const std::array<double, 24>& currentEmotionalState) const;
    
    /**
     * Calcule la distance entre deux vecteurs émotionnels
     */
    double emotionalDistance(const std::array<double, 24>& e1,
                             const std::array<double, 24>& e2) const;
    
    /**
     * Calcule l'intensité émotionnelle globale
     */
    double calculateEmotionalIntensity(const std::array<double, 24>& emotions) const;
    
    /**
     * Génère du bruit stochastique pour la phase d'exploration
     */
    double generateStochasticNoise(const std::string& activePattern) const;
    
    /**
     * Détermine si deux souvenirs peuvent être associés
     */
    bool canCreateAssociation(const Memory& m1, const Memory& m2) const;
    
    // ═══════════════════════════════════════════════════════════
    // TRANSITIONS D'ÉTAT
    // ═══════════════════════════════════════════════════════════
    
    void transitionTo(DreamState newState);
    bool shouldStartDream(const std::string& activePattern, bool amyghaleonAlert) const;
    DreamState getNextDreamPhase() const;
    
    // ═══════════════════════════════════════════════════════════
    // MEMBRES
    // ═══════════════════════════════════════════════════════════
    
    DreamConfig config_;
    DreamState currentState_ = DreamState::AWAKE;
    
    // Timing
    std::chrono::steady_clock::time_point cycleStartTime_;
    std::chrono::steady_clock::time_point lastDreamEndTime_;
    std::chrono::steady_clock::time_point currentPhaseStartTime_;
    
    // État émotionnel courant (pour calculs)
    std::array<double, 24> currentEmotionalState_{};
    std::string activePattern_;
    
    // MCT buffer
    std::vector<Memory> mctBuffer_;

    // Souvenirs scorés pendant le scan
    std::vector<Memory> scoredMemories_;

    // Données MCTGraph (liens causaux mot→émotion)
    std::vector<CausalLink> causalLinks_;
    std::vector<WordNodeSnapshot> wordNodes_;
    CausalStats causalStats_;
    
    // Arêtes à traiter
    std::vector<MemoryEdge> edgesToReinforce_;
    std::vector<MemoryEdge> edgesToCreate_;
    std::vector<std::string> memoriesToDelete_;
    
    // Callbacks
    DreamStateCallback stateChangeCallback_;
    Neo4jConsolidateCallback consolidateCallback_;
    Neo4jReinforceCallback reinforceCallback_;
    Neo4jDeleteCallback deleteCallback_;
    Neo4jCreateEdgeCallback createEdgeCallback_;
    
    // Stats
    Stats stats_;
    
    // Thread safety
    mutable std::mutex mutex_;
    
    // RNG pour stochastique
    mutable std::mt19937 rng_{std::random_device{}()};
};

} // namespace MCEE
