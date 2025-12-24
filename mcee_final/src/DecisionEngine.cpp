/**
 * @file DecisionEngine.cpp
 * @brief Implémentation du Moteur de Prise de Décision Réfléchie
 * @version 1.0
 * @date 2025-12-23
 */

#include "DecisionEngine.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>
#include <iomanip>

namespace mcee {

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTEUR
// ═══════════════════════════════════════════════════════════════════════════

DecisionEngine::DecisionEngine(const DecisionConfig& config)
    : config_(config)
    , rng_(std::random_device{}())
{
    std::cout << "[Decision] Moteur initialisé\n";
    std::cout << "[Decision] τ_max=" << config_.tau_max_ms << "ms, "
              << "θ_veto=" << config_.theta_veto << ", "
              << "θ_meta=" << config_.theta_meta << "\n";
}

// ═══════════════════════════════════════════════════════════════════════════
// DÉCISION PRINCIPALE
// ═══════════════════════════════════════════════════════════════════════════

DecisionResult DecisionEngine::decide(
    const EmotionalState& emotional_state,
    const ConscienceSentimentState& conscience_state,
    const GoalState& goal_state,
    const std::string& context_type,
    const std::vector<ActionOption>& available_actions)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto start_time = std::chrono::steady_clock::now();

    total_decisions_++;

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 1 : PERCEPTION → Σ(t)
    // ═══════════════════════════════════════════════════════════════════════

    std::vector<AmyghaleonAlert> alerts;
    // Générer alertes à partir de l'état émotionnel
    if (emotional_state.getEmotion("Peur") > 0.6) {
        alerts.push_back({"danger", emotional_state.getEmotion("Peur"), "emotion"});
    }
    if (emotional_state.getEmotion("Colère") > 0.7) {
        alerts.push_back({"escalation", emotional_state.getEmotion("Colère"), "emotion"});
    }

    SituationFrame frame = buildSituationFrame(
        emotional_state, conscience_state, context_type, alerts
    );

    std::cout << "[Decision] Phase 1: Σ(t) construit, U(t)="
              << std::fixed << std::setprecision(2) << frame.urgency
              << ", τ_delib=" << frame.tau_delib_ms << "ms\n";

    // Mode réflexe si urgence maximale
    if (frame.isReflexMode()) {
        std::cout << "[Decision] ⚡ Mode réflexe activé\n";
        reflex_decisions_++;
        return decideReflex(frame);
    }

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 2 : ACTIVATION MÉMORIELLE → M(t)
    // ═══════════════════════════════════════════════════════════════════════

    MemoryContext memory = buildMemoryContext(frame);

    std::cout << "[Decision] Phase 2: M(t) construit, "
              << memory.episodes.size() << " épisodes, "
              << memory.procedures.size() << " procédures\n";

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 3 : GÉNÉRATION & SIMULATION → A(t)
    // ═══════════════════════════════════════════════════════════════════════

    std::vector<ActionOption> options;
    if (available_actions.empty()) {
        options = generateOptions(frame, memory, goal_state);
    } else {
        options = available_actions;
    }

    // Projeter chaque option
    for (auto& option : options) {
        option.projection = projectAction(option, frame, memory, goal_state);
    }

    std::cout << "[Decision] Phase 3: " << options.size() << " options générées\n";

    // ═══════════════════════════════════════════════════════════════════════
    // PHASE 4 : ARBITRAGE & SÉLECTION → D(t), κ(t)
    // ═══════════════════════════════════════════════════════════════════════

    // 4.1 Veto Amyghaleon
    size_t vetoed = applyVeto(options, frame.alerts);
    if (vetoed > 0) {
        std::cout << "[Decision] " << vetoed << " options retirées par veto\n";
    }

    // 4.2 Scorer les options restantes
    for (auto& option : options) {
        if (!option.vetoed) {
            option.score = computeScore(option, frame.Ft);
        }
    }

    // 4.3 Détecter conflits
    auto conflicts = detectConflicts(options, goal_state);
    for (const auto& conflict : conflicts) {
        std::cout << "[Decision] Conflit: " << conflict.goal1_name
                  << " vs " << conflict.goal2_name << "\n";
        if (on_conflict_) {
            on_conflict_(conflict);
        }
    }

    // 4.4 Trier par score
    std::vector<ActionOption*> sorted_options;
    for (auto& opt : options) {
        if (!opt.vetoed) {
            sorted_options.push_back(&opt);
        }
    }
    std::sort(sorted_options.begin(), sorted_options.end(),
              [](const ActionOption* a, const ActionOption* b) {
                  return a->score > b->score;
              });

    // 4.5 Calculer MetaState
    double winning_score = sorted_options.empty() ? 0.0 : sorted_options[0]->score;
    double second_score = sorted_options.size() > 1 ? sorted_options[1]->score : 0.0;
    MetaState meta_state = buildMetaState(options, winning_score, second_score);

    // 4.6 Sélection finale
    DecisionResult result = selectBestAction(options, meta_state, frame);
    result.conflicts = conflicts;
    result.all_options = options;

    // Temps de délibération effectif
    auto end_time = std::chrono::steady_clock::now();
    result.deliberation_time_ms = std::chrono::duration<double, std::milli>(
        end_time - start_time
    ).count();

    std::cout << "[Decision] Phase 4: D(t)=" << result.action_name
              << ", κ=" << std::fixed << std::setprecision(2) << result.confidence
              << ", temps=" << result.deliberation_time_ms << "ms\n";

    // Callback
    if (on_decision_) {
        on_decision_(result);
    }

    // Historique
    updateHistory(result);

    return result;
}

DecisionResult DecisionEngine::decideReflex(const SituationFrame& frame) {
    DecisionResult result;
    result.reflex_mode = true;
    result.confidence = 0.9;  // Haute confiance pour les réflexes

    // Chercher le meilleur réflexe
    MemoryContext memory = buildMemoryContext(frame);
    auto best_reflex = memory.getBestReflex();

    if (best_reflex) {
        result.action_id = best_reflex->id;
        result.action_name = best_reflex->name;
        std::cout << "[Decision] Réflexe: " << result.action_name << "\n";
    } else {
        // Réflexe par défaut : protection
        result.action_id = "reflex_protect";
        result.action_name = "Protection/Retrait";
        std::cout << "[Decision] Réflexe par défaut: Protection\n";
    }

    if (on_decision_) {
        on_decision_(result);
    }

    updateHistory(result);
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// PHASE 1 : PERCEPTION
// ═══════════════════════════════════════════════════════════════════════════

SituationFrame DecisionEngine::buildSituationFrame(
    const EmotionalState& emotional_state,
    const ConscienceSentimentState& conscience_state,
    const std::string& context_type,
    const std::vector<AmyghaleonAlert>& alerts)
{
    SituationFrame frame;
    frame.emotional_state = emotional_state;
    frame.Ct = conscience_state.consciousness_level;
    frame.Ft = conscience_state.sentiment;
    frame.context_type = context_type;
    frame.alerts = alerts;
    frame.urgency = computeUrgency(emotional_state, alerts);
    frame.computeDeliberationTime(config_.tau_max_ms);
    frame.timestamp = std::chrono::steady_clock::now();

    return frame;
}

double DecisionEngine::computeUrgency(
    const EmotionalState& emotional_state,
    const std::vector<AmyghaleonAlert>& alerts)
{
    double urgency = 0.0;

    // Contribution des émotions critiques
    urgency += emotional_state.getEmotion("Peur") * 0.4;
    urgency += emotional_state.getEmotion("Colère") * 0.2;
    urgency += emotional_state.getEmotion("Anxiété") * 0.2;

    // Contribution des alertes
    for (const auto& alert : alerts) {
        urgency += alert.severity * 0.3;
    }

    return std::clamp(urgency, 0.0, 1.0);
}

// ═══════════════════════════════════════════════════════════════════════════
// PHASE 2 : ACTIVATION MÉMORIELLE
// ═══════════════════════════════════════════════════════════════════════════

MemoryContext DecisionEngine::buildMemoryContext(const SituationFrame& frame) {
    MemoryContext context;

    // Récupérer les épisodes similaires (top-5)
    std::vector<std::pair<double, const MemoryEpisode*>> scored_episodes;
    for (const auto& episode : episodes_) {
        double sim = computeEpisodeSimilarity(episode, frame);
        scored_episodes.push_back({sim, &episode});
    }
    std::sort(scored_episodes.begin(), scored_episodes.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });

    size_t count = std::min(size_t(5), scored_episodes.size());
    for (size_t i = 0; i < count; ++i) {
        MemoryEpisode ep = *scored_episodes[i].second;
        ep.similarity = scored_episodes[i].first;
        context.episodes.push_back(ep);
    }

    // Récupérer les procédures applicables
    for (const auto& proc : procedures_) {
        if (proc.trigger_context == frame.context_type || proc.trigger_context == "*") {
            context.procedures.push_back(proc);
        }
    }

    // Récupérer les concepts pertinents
    for (const auto& sem_concept : concepts_) {
        if (sem_concept.relevance > 0.3) {
            context.concepts.push_back(sem_concept);
        }
    }

    context.timestamp = std::chrono::steady_clock::now();
    return context;
}

void DecisionEngine::addEpisode(const MemoryEpisode& episode) {
    std::lock_guard<std::mutex> lock(mutex_);
    episodes_.push_back(episode);
}

void DecisionEngine::addProcedure(const MemoryProcedure& procedure) {
    std::lock_guard<std::mutex> lock(mutex_);
    procedures_.push_back(procedure);
}

void DecisionEngine::addConcept(const SemanticConcept& semantic_concept) {
    std::lock_guard<std::mutex> lock(mutex_);
    concepts_.push_back(semantic_concept);
}

// ═══════════════════════════════════════════════════════════════════════════
// PHASE 3 : GÉNÉRATION & SIMULATION
// ═══════════════════════════════════════════════════════════════════════════

std::vector<ActionOption> DecisionEngine::generateOptions(
    const SituationFrame& frame,
    const MemoryContext& memory,
    const GoalState& goals)
{
    std::vector<ActionOption> options;

    // Générer options par défaut selon le contexte
    auto default_opts = generateDefaultOptions(frame.context_type);
    options.insert(options.end(), default_opts.begin(), default_opts.end());

    // Ajouter méta-action InfoRequest si activée
    if (config_.enable_meta_actions) {
        ActionOption info_request;
        info_request.id = "meta_info_request";
        info_request.name = "Demander plus d'informations";
        info_request.category = "meta";
        info_request.meta_type = MetaActionType::QUESTION;
        info_request.meta_target = "clarification";
        options.push_back(info_request);

        ActionOption defer;
        defer.id = "meta_defer";
        defer.name = "Différer la décision";
        defer.category = "meta";
        defer.meta_type = MetaActionType::DEFER;
        options.push_back(defer);
    }

    return options;
}

ActionProjection DecisionEngine::projectAction(
    const ActionOption& action,
    const SituationFrame& frame,
    const MemoryContext& memory,
    const GoalState& goals)
{
    ActionProjection proj;

    // Calculer l'incertitude de base
    proj.uncertainty = 0.5;  // Base

    // Ajuster selon les épisodes similaires
    for (const auto& episode : memory.episodes) {
        if (episode.action_taken == action.name) {
            proj.uncertainty *= (1.0 - episode.similarity * 0.3);
            proj.outcome_expected += episode.outcome_valence * episode.similarity;
        }
    }

    // Profondeur de simulation
    proj.simulation_depth = computeSimulationDepth(proj.uncertainty);

    // Prédiction émotionnelle
    // Positive si action constructive, négative si risquée
    if (action.category == "constructif" || action.category == "calme") {
        proj.emotional_forecast = 0.3;
    } else if (action.category == "agressif" || action.category == "risque") {
        proj.emotional_forecast = -0.2;
    } else {
        proj.emotional_forecast = 0.0;
    }

    // Alignement avec objectifs
    proj.goal_alignment = goals.G * 0.5 + 0.25;  // Simplifié

    // Risque
    if (action.category == "agressif" || action.category == "risque") {
        proj.risk = 0.6;
    } else if (action.category == "meta") {
        proj.risk = 0.1;
    } else {
        proj.risk = 0.3;
    }

    return proj;
}

ActionOption DecisionEngine::createInfoRequest(
    const std::string& target,
    const std::string& question_type)
{
    ActionOption option;
    option.id = "info_request_" + target;
    option.name = "Demander info: " + target;
    option.category = "meta";
    option.meta_type = MetaActionType::QUESTION;
    option.meta_target = target;

    option.projection.uncertainty = 0.8;  // Haute incertitude avant info
    option.projection.emotional_forecast = 0.05;
    option.projection.goal_alignment = 0.5;
    option.projection.risk = 0.1;

    return option;
}

// ═══════════════════════════════════════════════════════════════════════════
// PHASE 4 : ARBITRAGE & SÉLECTION
// ═══════════════════════════════════════════════════════════════════════════

size_t DecisionEngine::applyVeto(
    std::vector<ActionOption>& options,
    const std::vector<AmyghaleonAlert>& alerts)
{
    size_t vetoed_count = 0;

    for (auto& option : options) {
        double risk = option.projection.risk;

        // Augmenter le risque selon les alertes actives
        for (const auto& alert : alerts) {
            if (alert.type == "escalation" && option.category == "agressif") {
                risk += alert.severity * 0.3;
            }
            if (alert.type == "reputation" && option.category == "impulsif") {
                risk += alert.severity * 0.2;
            }
        }

        if (risk > config_.theta_veto) {
            option.vetoed = true;
            option.veto_reason = "Risque " + std::to_string(risk) + " > θ_veto";
            vetoed_count++;
            veto_count_++;

            if (on_veto_) {
                on_veto_(option, option.veto_reason);
            }
        }
    }

    return vetoed_count;
}

double DecisionEngine::computeScore(const ActionOption& option, double Ft) {
    double w1 = config_.w1_goal_align;
    double w2 = config_.w2_emo_forecast;
    double w3 = config_.w3_confidence;
    double w4 = config_.w4_uncertainty;
    double w5 = config_.w5_risk;

    // Moduler selon Ft
    modulateWeights(Ft, w1, w2, w3, w4, w5);

    const auto& proj = option.projection;

    // Score = w1·goal_align + w2·emo_forecast + w3·confidence - w4·uncertainty - w5·risk
    double confidence = 1.0 - proj.uncertainty;  // Approximation
    double score = w1 * proj.goal_alignment
                 + w2 * proj.emotional_forecast
                 + w3 * confidence
                 - w4 * proj.uncertainty
                 - w5 * proj.risk;

    return score;
}

std::vector<GoalConflict> DecisionEngine::detectConflicts(
    const std::vector<ActionOption>& options,
    const GoalState& goals)
{
    std::vector<GoalConflict> conflicts;

    // Simplification : détecter si la variable dominante est en conflit
    // avec une autre variable importante
    if (goals.dominant_variable == "Depassement" &&
        goals.variables.getVariable(GoalVariable::TRAUMATISMES) > 0.5) {
        GoalConflict conflict;
        conflict.goal1_name = "Dépassement";
        conflict.goal2_name = "Protection (trauma)";
        conflict.conflict_intensity = goals.variables.getVariable(GoalVariable::TRAUMATISMES);
        conflict.recommended_resolution = "extend_delib";
        conflicts.push_back(conflict);
    }

    return conflicts;
}

MetaState DecisionEngine::buildMetaState(
    const std::vector<ActionOption>& options,
    double winning_score,
    double second_score)
{
    MetaState state;

    // Confiance = marge de victoire
    state.confidence = winning_score - second_score;

    // Incertitude globale
    double total_uncertainty = 0.0;
    size_t count = 0;
    for (const auto& opt : options) {
        if (!opt.vetoed) {
            total_uncertainty += opt.projection.uncertainty;
            count++;
        }
    }
    state.uncertainty_global = count > 0 ? total_uncertainty / count : 0.0;

    // "Je sais que je ne sais pas"
    state.know_unknown = state.uncertainty_global > config_.theta_meta;

    state.timestamp = std::chrono::steady_clock::now();
    return state;
}

DecisionResult DecisionEngine::selectBestAction(
    std::vector<ActionOption>& options,
    const MetaState& meta_state,
    const SituationFrame& frame)
{
    DecisionResult result;
    result.meta_state = meta_state;

    // Filtrer les options non-vetoed
    std::vector<ActionOption*> valid_options;
    for (auto& opt : options) {
        if (!opt.vetoed) {
            valid_options.push_back(&opt);
        }
    }

    if (valid_options.empty()) {
        result.action_id = "no_action";
        result.action_name = "Aucune action valide";
        result.confidence = 0.0;
        return result;
    }

    // Trier par score
    std::sort(valid_options.begin(), valid_options.end(),
              [](const ActionOption* a, const ActionOption* b) {
                  return a->score > b->score;
              });

    ActionOption* best = valid_options[0];

    // Vérifier si méta-action nécessaire
    if (config_.enable_meta_actions &&
        meta_state.uncertainty_global > config_.theta_info &&
        meta_state.confidence < config_.theta_confidence) {

        // Chercher une méta-action dans les options
        for (auto* opt : valid_options) {
            if (opt->isMetaAction()) {
                best = opt;
                meta_action_decisions_++;
                std::cout << "[Decision] Méta-action déclenchée: " << opt->name << "\n";

                if (on_meta_action_) {
                    on_meta_action_(opt->meta_type, opt->meta_target);
                }
                break;
            }
        }
    }

    result.action_id = best->id;
    result.action_name = best->name;
    result.score = best->score;
    result.confidence = meta_state.confidence;
    result.is_meta_action = best->isMetaAction();
    result.meta_type = best->meta_type;

    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// APPRENTISSAGE POST-DÉCISION
// ═══════════════════════════════════════════════════════════════════════════

void DecisionEngine::recordOutcome(const DecisionOutcome& outcome) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Mettre à jour les procédures
    for (auto& proc : procedures_) {
        if (proc.name == outcome.decision_id || proc.id == outcome.decision_id) {
            proc.activation_count++;
            if (outcome.success) {
                proc.success_rate = (proc.success_rate * (proc.activation_count - 1) + 1.0)
                                   / proc.activation_count;

                // Promouvoir en réflexe si suffisamment de succès
                if (proc.activation_count >= static_cast<size_t>(config_.theta_automate)) {
                    promoteToReflex(proc.id);
                }
            } else {
                proc.success_rate = (proc.success_rate * (proc.activation_count - 1))
                                   / proc.activation_count;
            }
        }
    }

    // Ajouter comme nouvel épisode
    MemoryEpisode episode;
    episode.id = "ep_" + std::to_string(episodes_.size());
    episode.description = "Décision: " + outcome.decision_id;
    episode.outcome_valence = outcome.actual_outcome;
    episode.action_taken = outcome.decision_id;
    episode.lesson = outcome.success ? "Succès" : "Échec";
    episodes_.push_back(episode);

    std::cout << "[Decision] Outcome enregistré: " << outcome.decision_id
              << " (success=" << outcome.success << ")\n";
}

void DecisionEngine::promoteToReflex(const std::string& procedure_id) {
    for (auto& proc : procedures_) {
        if (proc.id == procedure_id) {
            proc.is_reflex = true;
            std::cout << "[Decision] Procédure promue en réflexe: " << proc.name << "\n";
            break;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// CONFIGURATION & ACCESSEURS
// ═══════════════════════════════════════════════════════════════════════════

void DecisionEngine::setVetoThreshold(double theta) {
    config_.theta_veto = std::clamp(theta, 0.0, 1.0);
}

void DecisionEngine::enableMetaActions(bool enable) {
    config_.enable_meta_actions = enable;
}

const std::deque<DecisionResult>& DecisionEngine::getDecisionHistory() const {
    return decision_history_;
}

void DecisionEngine::setDecisionCallback(DecisionCallback callback) {
    on_decision_ = std::move(callback);
}

void DecisionEngine::setVetoCallback(VetoCallback callback) {
    on_veto_ = std::move(callback);
}

void DecisionEngine::setMetaActionCallback(MetaActionCallback callback) {
    on_meta_action_ = std::move(callback);
}

void DecisionEngine::setConflictCallback(ConflictCallback callback) {
    on_conflict_ = std::move(callback);
}

// ═══════════════════════════════════════════════════════════════════════════
// MÉTHODES PRIVÉES
// ═══════════════════════════════════════════════════════════════════════════

double DecisionEngine::computeEpisodeSimilarity(
    const MemoryEpisode& episode,
    const SituationFrame& frame) const
{
    // Simplification : utiliser la description
    double sim_ctx = 0.3;  // Base
    double sim_emo = 0.3;  // Base
    double sim_temp = 0.2; // Base

    return config_.alpha_ctx * sim_ctx +
           config_.beta_emo * sim_emo +
           config_.gamma_temp * sim_temp;
}

size_t DecisionEngine::computeSimulationDepth(double uncertainty) const {
    if (uncertainty < 0.2) return 1;
    if (uncertainty < 0.5) return 2;
    return std::min(size_t(3), config_.max_simulation_depth);
}

void DecisionEngine::modulateWeights(
    double Ft,
    double& w1, double& w2, double& w3, double& w4, double& w5) const
{
    if (Ft > 0) {
        // Fond affectif positif : favorise exploration
        w4 -= config_.Ft_positive_exploration_boost * Ft;
        w3 += config_.Ft_positive_exploration_boost * Ft;
    } else {
        // Fond affectif négatif : favorise prudence
        w5 += config_.Ft_negative_prudence_boost * std::abs(Ft);
    }

    // Renormaliser
    double sum = w1 + w2 + w3 + w4 + w5;
    if (sum > 0) {
        w1 /= sum; w2 /= sum; w3 /= sum; w4 /= sum; w5 /= sum;
    }
}

std::vector<ActionOption> DecisionEngine::generateDefaultOptions(
    const std::string& context_type)
{
    std::vector<ActionOption> options;

    if (context_type == "reunion" || context_type == "professionnel") {
        options.push_back({"act_respond_calm", "Recadrer calmement", "", "calme", {}, MetaActionType::NONE, "", false, "", 0.0});
        options.push_back({"act_humor", "Désamorcer par l'humour", "", "constructif", {}, MetaActionType::NONE, "", false, "", 0.0});
        options.push_back({"act_defer", "Reporter la discussion", "", "neutre", {}, MetaActionType::NONE, "", false, "", 0.0});
        options.push_back({"act_respond_sharp", "Répondre sèchement", "", "agressif", {}, MetaActionType::NONE, "", false, "", 0.0});
    } else if (context_type == "projet" || context_type == "strategique") {
        options.push_back({"act_accept", "Accepter la proposition", "", "constructif", {}, MetaActionType::NONE, "", false, "", 0.0});
        options.push_back({"act_refuse", "Refuser la proposition", "", "neutre", {}, MetaActionType::NONE, "", false, "", 0.0});
        options.push_back({"act_pilot", "Lancer un pilote limité", "", "prudent", {}, MetaActionType::NONE, "", false, "", 0.0});
    } else {
        // Options génériques
        options.push_back({"act_engage", "S'engager", "", "constructif", {}, MetaActionType::NONE, "", false, "", 0.0});
        options.push_back({"act_wait", "Attendre", "", "neutre", {}, MetaActionType::NONE, "", false, "", 0.0});
        options.push_back({"act_retreat", "Se retirer", "", "prudent", {}, MetaActionType::NONE, "", false, "", 0.0});
    }

    return options;
}

void DecisionEngine::updateHistory(const DecisionResult& result) {
    decision_history_.push_back(result);
    while (decision_history_.size() > MAX_HISTORY_SIZE) {
        decision_history_.pop_front();
    }
}

} // namespace mcee
