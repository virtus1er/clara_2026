/**
 * @file DecisionEngine.hpp
 * @brief Moteur de Prise de Décision Réfléchie
 * @version 1.0
 * @date 2025-12-23
 *
 * Architecture 4 phases :
 * Phase 1: Perception → Σ(t)
 * Phase 2: Activation mémorielle → M(t)
 * Phase 3: Génération & Simulation → A(t)
 * Phase 4: Arbitrage & Sélection → D(t), κ(t)
 */

#pragma once

#include "DecisionConfig.hpp"
#include "ConscienceConfig.hpp"
#include "ADDOConfig.hpp"
#include "Types.hpp"
#include <memory>
#include <vector>
#include <deque>
#include <mutex>
#include <random>

namespace mcee {

/**
 * @class DecisionEngine
 * @brief Moteur de prise de décision réfléchie à 4 phases
 */
class DecisionEngine {
public:
    /**
     * @brief Constructeur avec configuration
     */
    explicit DecisionEngine(const DecisionConfig& config = DecisionConfig{});

    /**
     * @brief Destructeur
     */
    ~DecisionEngine() = default;

    // ═══════════════════════════════════════════════════════════════════════
    // DÉCISION PRINCIPALE
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Processus de décision complet (4 phases)
     * @param emotional_state État émotionnel actuel
     * @param conscience_state État de conscience (Ct, Ft)
     * @param goal_state État des objectifs ADDO
     * @param context_type Type de contexte ("reunion", "projet", etc.)
     * @param available_actions Actions possibles (si vide, génération auto)
     * @return Résultat de décision avec confiance
     */
    DecisionResult decide(
        const EmotionalState& emotional_state,
        const ConscienceSentimentState& conscience_state,
        const GoalState& goal_state,
        const std::string& context_type,
        const std::vector<ActionOption>& available_actions = {}
    );

    /**
     * @brief Décision rapide en mode réflexe (bypass phases 2-3)
     * @param frame Cadre situationnel
     * @return Meilleur réflexe disponible
     */
    DecisionResult decideReflex(const SituationFrame& frame);

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 1 : PERCEPTION
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Construit le SituationFrame Σ(t)
     */
    SituationFrame buildSituationFrame(
        const EmotionalState& emotional_state,
        const ConscienceSentimentState& conscience_state,
        const std::string& context_type,
        const std::vector<AmyghaleonAlert>& alerts = {}
    );

    /**
     * @brief Calcule l'urgence U(t) à partir des alertes et émotions
     */
    double computeUrgency(
        const EmotionalState& emotional_state,
        const std::vector<AmyghaleonAlert>& alerts
    );

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 2 : ACTIVATION MÉMORIELLE
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Construit le MemoryContext M(t)
     */
    MemoryContext buildMemoryContext(const SituationFrame& frame);

    /**
     * @brief Ajoute un épisode à la mémoire (ME)
     */
    void addEpisode(const MemoryEpisode& episode);

    /**
     * @brief Ajoute une procédure à la mémoire (MP)
     */
    void addProcedure(const MemoryProcedure& procedure);

    /**
     * @brief Ajoute un concept sémantique (MS)
     */
    void addConcept(const SemanticConcept& semantic_concept);

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 3 : GÉNÉRATION & SIMULATION
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Génère les options d'action
     */
    std::vector<ActionOption> generateOptions(
        const SituationFrame& frame,
        const MemoryContext& memory,
        const GoalState& goals
    );

    /**
     * @brief Projette les conséquences d'une action
     */
    ActionProjection projectAction(
        const ActionOption& action,
        const SituationFrame& frame,
        const MemoryContext& memory,
        const GoalState& goals
    );

    /**
     * @brief Crée une méta-action InfoRequest
     */
    ActionOption createInfoRequest(
        const std::string& target,
        const std::string& question_type
    );

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 4 : ARBITRAGE & SÉLECTION
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Applique le veto Amyghaleon aux options
     * @return Nombre d'options retirées
     */
    size_t applyVeto(
        std::vector<ActionOption>& options,
        const std::vector<AmyghaleonAlert>& alerts
    );

    /**
     * @brief Calcule le score d'une option
     */
    double computeScore(
        const ActionOption& option,
        double Ft
    );

    /**
     * @brief Détecte les conflits entre objectifs
     */
    std::vector<GoalConflict> detectConflicts(
        const std::vector<ActionOption>& options,
        const GoalState& goals
    );

    /**
     * @brief Construit l'état métacognitif
     */
    MetaState buildMetaState(
        const std::vector<ActionOption>& options,
        double winning_score,
        double second_score
    );

    /**
     * @brief Sélectionne la meilleure action
     */
    DecisionResult selectBestAction(
        std::vector<ActionOption>& options,
        const MetaState& meta_state,
        const SituationFrame& frame
    );

    // ═══════════════════════════════════════════════════════════════════════
    // APPRENTISSAGE POST-DÉCISION
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Enregistre le résultat d'une décision
     */
    void recordOutcome(const DecisionOutcome& outcome);

    /**
     * @brief Promeut une procédure en réflexe si suffisamment de succès
     */
    void promoteToReflex(const std::string& procedure_id);

    // ═══════════════════════════════════════════════════════════════════════
    // CONFIGURATION & ACCESSEURS
    // ═══════════════════════════════════════════════════════════════════════

    /**
     * @brief Modifie le seuil de veto
     */
    void setVetoThreshold(double theta);

    /**
     * @brief Active/désactive les méta-actions
     */
    void enableMetaActions(bool enable);

    /**
     * @brief Retourne la configuration
     */
    [[nodiscard]] const DecisionConfig& getConfig() const { return config_; }

    /**
     * @brief Retourne l'historique des décisions
     */
    [[nodiscard]] const std::deque<DecisionResult>& getDecisionHistory() const;

    /**
     * @brief Retourne les statistiques
     */
    [[nodiscard]] size_t getTotalDecisions() const { return total_decisions_; }
    [[nodiscard]] size_t getReflexDecisions() const { return reflex_decisions_; }
    [[nodiscard]] size_t getMetaActionDecisions() const { return meta_action_decisions_; }
    [[nodiscard]] size_t getVetoCount() const { return veto_count_; }

    // ═══════════════════════════════════════════════════════════════════════
    // CALLBACKS
    // ═══════════════════════════════════════════════════════════════════════

    void setDecisionCallback(DecisionCallback callback);
    void setVetoCallback(VetoCallback callback);
    void setMetaActionCallback(MetaActionCallback callback);
    void setConflictCallback(ConflictCallback callback);

private:
    DecisionConfig config_;

    // Mémoires internes (simplifiées)
    std::vector<MemoryEpisode> episodes_;
    std::vector<MemoryProcedure> procedures_;
    std::vector<SemanticConcept> concepts_;

    // Historique
    std::deque<DecisionResult> decision_history_;
    static constexpr size_t MAX_HISTORY_SIZE = 100;

    // Statistiques
    size_t total_decisions_ = 0;
    size_t reflex_decisions_ = 0;
    size_t meta_action_decisions_ = 0;
    size_t veto_count_ = 0;

    // Callbacks
    DecisionCallback on_decision_;
    VetoCallback on_veto_;
    MetaActionCallback on_meta_action_;
    ConflictCallback on_conflict_;

    // Générateur aléatoire
    std::mt19937 rng_;

    mutable std::mutex mutex_;

    // ─────────────────────────────────────────────────────────────────────────
    // Méthodes privées
    // ─────────────────────────────────────────────────────────────────────────

    /**
     * @brief Calcule la similarité entre un épisode et la situation
     */
    [[nodiscard]] double computeEpisodeSimilarity(
        const MemoryEpisode& episode,
        const SituationFrame& frame
    ) const;

    /**
     * @brief Calcule la profondeur de simulation adaptative
     */
    [[nodiscard]] size_t computeSimulationDepth(double uncertainty) const;

    /**
     * @brief Modifie les poids de score selon Ft
     */
    void modulateWeights(double Ft,
                         double& w1, double& w2, double& w3,
                         double& w4, double& w5) const;

    /**
     * @brief Génère des options par défaut si aucune fournie
     */
    std::vector<ActionOption> generateDefaultOptions(
        const std::string& context_type
    );

    /**
     * @brief Met à jour l'historique
     */
    void updateHistory(const DecisionResult& result);
};

} // namespace mcee
