/**
 * @file MDDOEngine.hpp
 * @brief Module de Décision Délibérée et Orientée (MDDO)
 * @version 1.0
 * @date 2025-12-26
 *
 * Le MDDO est le chaînon manquant entre la perception émotionnelle (MCEE)
 * et l'action. Il implémente un processus de décision en 4 phases :
 *
 * 1. PERCEPTION    : Réception du SituationFrame (état subjectif)
 * 2. ACTIVATION    : Rappel des souvenirs pertinents (MCT/MLT)
 * 3. SIMULATION    : Évaluation des options (prédiction des conséquences)
 * 4. ARBITRAGE     : Sélection de l'intention d'action
 *
 * Architecture inspirée des neurosciences :
 * - Cortex préfrontal (planification)
 * - Hippocampe (mémoire épisodique)
 * - Striatum (évaluation des récompenses)
 * - ACC (gestion des conflits)
 */

#pragma once

#include "EmotionalState.hpp"
#include "MemoryManager.hpp"
#include <string>
#include <vector>
#include <array>
#include <functional>
#include <chrono>
#include <optional>
#include <unordered_map>

namespace mcee {

// ═══════════════════════════════════════════════════════════════════════════
// STRUCTURES DE DONNÉES
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Cadre situationnel subjectif et biaisé
 * Représente la perception du monde par le système, pas la réalité objective
 */
struct SituationFrame {
    // État émotionnel actuel
    EmotionalState emotional_state;

    // Contexte perçu
    std::string context;              // Description textuelle
    std::vector<std::string> entities; // Entités détectées (personnes, objets)

    // Évaluation subjective
    double threat_level = 0.0;        // Niveau de menace perçu [0, 1]
    double opportunity_level = 0.0;   // Niveau d'opportunité perçu [0, 1]
    double urgency = 0.0;             // Urgence perçue [0, 1]
    double uncertainty = 0.5;         // Incertitude sur la situation [0, 1]

    // Biais cognitifs actifs
    std::vector<std::string> active_biases;

    // Timestamp
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Option d'action possible
 */
struct ActionOption {
    std::string id;                   // Identifiant unique
    std::string name;                 // Nom de l'action
    std::string category;             // Catégorie (APPROACH, AVOID, FREEZE, EXPLORE, COMMUNICATE)

    // Évaluation prédictive
    double expected_reward = 0.0;     // Récompense attendue [-1, 1]
    double expected_cost = 0.0;       // Coût attendu [0, 1]
    double success_probability = 0.5; // Probabilité de succès [0, 1]
    double emotional_impact = 0.0;    // Impact émotionnel prédit [-1, 1]

    // Mémoires associées
    std::vector<std::string> supporting_memories;  // IDs des souvenirs qui soutiennent
    std::vector<std::string> opposing_memories;    // IDs des souvenirs qui s'opposent

    // Score final (calculé lors de l'arbitrage)
    double utility_score = 0.0;
};

/**
 * @brief Intention d'action (sortie du MDDO)
 */
struct ActionIntention {
    ActionOption selected_action;

    // Méta-données de la décision
    double confidence = 0.0;          // Confiance dans le choix [0, 1]
    double deliberation_time_ms = 0;  // Temps de délibération
    bool was_interrupted = false;     // Interrompu par Amyghaleon ?

    // Alternatives considérées
    std::vector<ActionOption> alternatives;

    // Justification (pour debugging/introspection)
    std::string rationale;

    // Timestamp
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Configuration du MDDO
 */
struct MDDOConfig {
    // Temps de délibération
    double tau_min_ms = 100.0;        // Temps minimum avant décision
    double tau_max_ms = 5000.0;       // Temps maximum de délibération
    double tau_urgency_factor = 0.8;  // Réduction par urgence (1 - urgency * factor)

    // Seuils de décision
    double theta_confidence = 0.6;    // Confiance minimale pour agir
    double theta_conflict = 0.3;      // Seuil de conflit entre options
    double theta_veto = 0.85;         // Seuil pour veto Amyghaleon

    // Pondération des facteurs
    double w_reward = 0.35;           // Poids de la récompense attendue
    double w_cost = 0.20;             // Poids du coût
    double w_probability = 0.25;      // Poids de la probabilité de succès
    double w_emotional = 0.20;        // Poids de l'impact émotionnel

    // Biais temporels
    double temporal_discount = 0.95;  // Dévaluation temporelle des récompenses futures

    // Nombre maximum de simulations
    size_t max_simulations = 100;
};

/**
 * @brief État interne du MDDO
 */
struct MDDOState {
    // Phase actuelle
    enum class Phase { IDLE, PERCEIVING, ACTIVATING, SIMULATING, ARBITRATING };
    Phase current_phase = Phase::IDLE;

    // SituationFrame en cours de traitement
    std::optional<SituationFrame> current_situation;

    // Options générées
    std::vector<ActionOption> generated_options;

    // Statistiques
    size_t decisions_made = 0;
    size_t vetoes_received = 0;
    double avg_deliberation_time_ms = 0.0;
    double avg_confidence = 0.0;
};

// ═══════════════════════════════════════════════════════════════════════════
// CLASSE PRINCIPALE
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @class MDDOEngine
 * @brief Moteur de décision délibérée et orientée
 *
 * Flux de traitement :
 *
 *   SituationFrame  ─────►  PERCEPTION
 *         │                     │
 *         │                     ▼
 *         │              ACTIVATION MÉMORIELLE
 *         │                     │
 *         │                     ▼
 *         │              SIMULATION (Monte-Carlo)
 *         │                     │
 *         │                     ▼
 *         │              ARBITRAGE
 *         │                     │
 *         ▼                     ▼
 *   [Veto Amyghaleon?] ──► ActionIntention
 */
class MDDOEngine {
public:
    using IntentionCallback = std::function<void(const ActionIntention&)>;
    using VetoCallback = std::function<bool(const ActionOption&)>;

    /**
     * @brief Constructeur avec configuration
     */
    explicit MDDOEngine(const MDDOConfig& config = MDDOConfig());

    /**
     * @brief Destructeur
     */
    ~MDDOEngine() = default;

    // ═══════════════════════════════════════════════════════════════════════
    // INTERFACE PRINCIPALE
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Traite une situation et génère une intention d'action
     * @param situation Le cadre situationnel perçu
     * @return L'intention d'action résultante
     */
    ActionIntention deliberate(const SituationFrame& situation);

    /**
     * @brief Traite une situation de manière asynchrone
     * @param situation Le cadre situationnel perçu
     * @param callback Fonction appelée avec le résultat
     */
    void deliberateAsync(const SituationFrame& situation, IntentionCallback callback);

    /**
     * @brief Interrompt la délibération en cours (appel d'urgence)
     * @param emergency_action Action d'urgence à exécuter
     */
    void interrupt(const ActionOption& emergency_action);

    // ═══════════════════════════════════════════════════════════════════════
    // CONFIGURATION
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Définit le callback de veto (Amyghaleon)
     */
    void setVetoCallback(VetoCallback callback) { veto_callback_ = std::move(callback); }

    /**
     * @brief Définit le gestionnaire de mémoire
     */
    void setMemoryManager(MemoryManager* memory) { memory_manager_ = memory; }

    /**
     * @brief Met à jour la configuration
     */
    void setConfig(const MDDOConfig& config) { config_ = config; }

    /**
     * @brief Obtient la configuration actuelle
     */
    const MDDOConfig& getConfig() const { return config_; }

    /**
     * @brief Obtient l'état actuel
     */
    const MDDOState& getState() const { return state_; }

private:
    // ═══════════════════════════════════════════════════════════════════════
    // PHASES DE DÉLIBÉRATION
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Phase 1 : Perception et construction du frame
     */
    SituationFrame perceive(const SituationFrame& raw_situation);

    /**
     * @brief Phase 2 : Activation mémorielle
     * Rappelle les souvenirs pertinents du MCT/MLT
     */
    std::vector<Memory> activateMemories(const SituationFrame& situation);

    /**
     * @brief Phase 3 : Génération et simulation des options
     * Utilise Monte-Carlo pour évaluer les conséquences
     */
    std::vector<ActionOption> simulate(
        const SituationFrame& situation,
        const std::vector<Memory>& memories);

    /**
     * @brief Phase 4 : Arbitrage final
     * Sélectionne l'action optimale selon l'utilité
     */
    ActionIntention arbitrate(
        const SituationFrame& situation,
        std::vector<ActionOption>& options);

    // ═══════════════════════════════════════════════════════════════════════
    // UTILITAIRES
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Génère les options d'action possibles
     */
    std::vector<ActionOption> generateOptions(const SituationFrame& situation);

    /**
     * @brief Calcule le score d'utilité d'une option
     */
    double computeUtility(const ActionOption& option) const;

    /**
     * @brief Vérifie si l'Amyghaleon pose un veto
     */
    bool checkVeto(const ActionOption& option);

    /**
     * @brief Calcule le temps de délibération basé sur l'urgence
     */
    double computeDeliberationTime(double urgency) const;

    /**
     * @brief Détecte les conflits entre options
     */
    double detectConflict(const std::vector<ActionOption>& options) const;

    // ═══════════════════════════════════════════════════════════════════════
    // DONNÉES MEMBRES
    // ═══════════════════════════════════════════════════════════════════════

    MDDOConfig config_;
    MDDOState state_;

    MemoryManager* memory_manager_ = nullptr;
    VetoCallback veto_callback_;
    IntentionCallback intention_callback_;

    // Templates d'actions de base
    std::vector<ActionOption> action_templates_;

    // Cache des dernières décisions
    std::vector<ActionIntention> decision_history_;
    static constexpr size_t MAX_HISTORY_SIZE = 100;
};

} // namespace mcee
