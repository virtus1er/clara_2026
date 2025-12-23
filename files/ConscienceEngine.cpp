#include "ConscienceEngine.hpp"

#include <cmath>
#include <numeric>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace MCEE {

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTEUR
// ═══════════════════════════════════════════════════════════════════════════════

ConscienceEngine::ConscienceEngine(const ConscienceConfig& config)
    : config_(config)
    , wisdom_(config.wisdom_initial)
    , last_update_(std::chrono::steady_clock::now())
    , start_time_(std::chrono::steady_clock::now())
{
    current_emotions_.fill(0.0);
    alpha_modulation_.fill(1.0);  // Modulation neutre par défaut
    
    // Initialiser l'historique vide
    conscience_history_.clear();
}

// ═══════════════════════════════════════════════════════════════════════════════
// MISE À JOUR DES ENTRÉES
// ═══════════════════════════════════════════════════════════════════════════════

void ConscienceEngine::updateEmotions(const std::array<double, 24>& emotions,
                                       const std::string& active_pattern) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_emotions_ = emotions;
    active_pattern_ = active_pattern;
}

void ConscienceEngine::updateMemoryActivation(const MemoryActivation& activation) {
    std::lock_guard<std::mutex> lock(mutex_);
    memory_activation_ = activation;
}

void ConscienceEngine::updateTrauma(const TraumaState& trauma) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    bool was_active = current_trauma_.isActive();
    current_trauma_ = trauma;
    
    // Callback si trauma devient actif
    if (!was_active && trauma.isActive() && trauma_callback_) {
        trauma_callback_(trauma);
    }
}

void ConscienceEngine::updateFeedback(const FeedbackState& feedback) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_feedback_ = feedback;
}

void ConscienceEngine::updateEnvironment(const EnvironmentState& environment) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_environment_ = environment;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CALCUL DES COMPOSANTES
// ═══════════════════════════════════════════════════════════════════════════════

double ConscienceEngine::computeEmotionComponent() const {
    // Σ αi · Ei(t) avec modulation dynamique
    double sum = 0.0;
    for (size_t i = 0; i < 24; ++i) {
        double alpha = config_.alpha_emotions[i] * alpha_modulation_[i];
        sum += alpha * current_emotions_[i];
    }
    return sum;
}

double ConscienceEngine::computeMemoryComponent() const {
    // Mtotal(t) = Σ ωj · Mj(t)
    return memory_activation_.computeMtotal(config_);
}

double ConscienceEngine::computeTraumaComponent() const {
    // Tdominant(t) - retourne l'intensité du trauma dominant si actif
    if (current_trauma_.isDominant(config_.trauma_dominance_threshold)) {
        // Le trauma domine : valeur amplifiée
        return current_trauma_.intensity * config_.omega_trauma;
    } else if (current_trauma_.isActive()) {
        // Trauma actif mais non dominant : contribution proportionnelle
        return current_trauma_.intensity * config_.omega_trauma * 0.5;
    }
    return 0.0;
}

double ConscienceEngine::computeFeedbackComponent() const {
    // β · Fbt
    return config_.beta_feedback * current_feedback_.computeScore();
}

double ConscienceEngine::computeEnvironmentComponent() const {
    // δ · Ent
    return config_.delta_environment * current_environment_.computeScore();
}

// ═══════════════════════════════════════════════════════════════════════════════
// CALCUL CONSCIENCE
// ═══════════════════════════════════════════════════════════════════════════════

ConscienceState ConscienceEngine::computeConscience() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ConscienceState state;
    state.timestamp = std::chrono::steady_clock::now();
    state.active_pattern = active_pattern_;
    state.dominant_trauma = current_trauma_;
    
    // Calculer chaque composante
    state.emotion_component = computeEmotionComponent();
    state.memory_component = computeMemoryComponent();
    state.trauma_component = computeTraumaComponent();
    state.feedback_component = computeFeedbackComponent();
    state.environment_component = computeEnvironmentComponent();
    state.wisdom_factor = wisdom_;
    
    // Équation principale : Ct = (Σcomposantes) × Wt
    double raw_sum = state.emotion_component +
                     state.memory_component +
                     state.trauma_component +
                     state.feedback_component +
                     state.environment_component;
    
    state.Ct = raw_sum * wisdom_;
    
    // Normalisation optionnelle [-1, 1]
    if (config_.use_tanh_normalization) {
        state.Ct = std::tanh(state.Ct * 0.1);  // Scaling pour tanh
    }
    
    return state;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CALCUL SENTIMENTS
// ═══════════════════════════════════════════════════════════════════════════════

double ConscienceEngine::computeWeightedHistory() const {
    // Σ γk · Ck avec décroissance géométrique
    if (conscience_history_.empty()) {
        return 0.0;
    }
    
    double sum = 0.0;
    double gamma_power = 1.0;
    
    // Parcourir du plus récent au plus ancien
    for (auto it = conscience_history_.rbegin(); it != conscience_history_.rend(); ++it) {
        sum += gamma_power * (*it);
        gamma_power *= config_.gamma_decay;
    }
    
    return sum;
}

SentimentState ConscienceEngine::computeSentiment() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    SentimentState state;
    state.timestamp = std::chrono::steady_clock::now();
    state.history_depth = conscience_history_.size();
    
    // Calculer la somme pondérée de l'historique
    state.accumulated_conscience = computeWeightedHistory();
    
    // Influence du feedback
    state.feedback_influence = config_.lambda_feedback * current_feedback_.computeScore();
    
    // Somme avant normalisation
    state.Ft_raw = state.accumulated_conscience + state.feedback_influence;
    
    // Application de la fonction de normalisation g(x) = tanh(x)
    if (config_.use_tanh_normalization) {
        state.Ft = std::tanh(state.Ft_raw * config_.tanh_scale);
    } else {
        // Clamp simple [-1, 1]
        state.Ft = std::clamp(state.Ft_raw, -1.0, 1.0);
    }
    
    return state;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CALCUL COMPLET
// ═══════════════════════════════════════════════════════════════════════════════

ConscienceSentimentState ConscienceEngine::compute() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ConscienceSentimentState result;
    result.timestamp = std::chrono::steady_clock::now();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // CALCUL CONSCIENCE (sans lock interne)
    // ═══════════════════════════════════════════════════════════════════════════
    
    result.conscience.timestamp = result.timestamp;
    result.conscience.active_pattern = active_pattern_;
    result.conscience.dominant_trauma = current_trauma_;
    
    // Composantes
    result.conscience.emotion_component = computeEmotionComponent();
    result.conscience.memory_component = computeMemoryComponent();
    result.conscience.trauma_component = computeTraumaComponent();
    result.conscience.feedback_component = computeFeedbackComponent();
    result.conscience.environment_component = computeEnvironmentComponent();
    result.conscience.wisdom_factor = wisdom_;
    
    // Équation Ct
    double raw_sum = result.conscience.emotion_component +
                     result.conscience.memory_component +
                     result.conscience.trauma_component +
                     result.conscience.feedback_component +
                     result.conscience.environment_component;
    
    result.conscience.Ct = raw_sum * wisdom_;
    if (config_.use_tanh_normalization) {
        result.conscience.Ct = std::tanh(result.conscience.Ct * 0.1);
    }
    
    // Ajouter à l'historique
    addToConscienceHistory(result.conscience.Ct);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // CALCUL SENTIMENT (sans lock interne)
    // ═══════════════════════════════════════════════════════════════════════════
    
    result.sentiment.timestamp = result.timestamp;
    result.sentiment.history_depth = conscience_history_.size();
    result.sentiment.accumulated_conscience = computeWeightedHistory();
    result.sentiment.feedback_influence = config_.lambda_feedback * current_feedback_.computeScore();
    result.sentiment.Ft_raw = result.sentiment.accumulated_conscience + result.sentiment.feedback_influence;
    
    if (config_.use_tanh_normalization) {
        result.sentiment.Ft = std::tanh(result.sentiment.Ft_raw * config_.tanh_scale);
    } else {
        result.sentiment.Ft = std::clamp(result.sentiment.Ft_raw, -1.0, 1.0);
    }
    
    // Mettre à jour le fond affectif
    updateAffectiveBackground(result.sentiment.Ft);
    result.affective_background = affective_background_;
    
    // Stocker pour accès ultérieur
    last_conscience_ = result.conscience;
    last_sentiment_ = result.sentiment;
    
    return result;
}

void ConscienceEngine::tick() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double, std::milli>(now - last_update_).count();
    
    if (elapsed < config_.update_interval_ms) {
        return;  // Pas encore temps de mettre à jour
    }
    
    // Calculer l'état complet
    auto state = compute();
    
    // Stocker les résultats
    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_conscience_ = state.conscience;
        last_sentiment_ = state.sentiment;
        last_update_ = now;
    }
    
    // Déclencher les callbacks
    if (conscience_callback_) {
        conscience_callback_(state.conscience);
    }
    if (sentiment_callback_) {
        sentiment_callback_(state.sentiment);
    }
    if (affective_callback_) {
        affective_callback_(state.affective_background);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// GESTION HISTORIQUE
// ═══════════════════════════════════════════════════════════════════════════════

void ConscienceEngine::addToConscienceHistory(double Ct) {
    conscience_history_.push_back(Ct);
    
    // Maintenir la taille de la fenêtre glissante
    while (conscience_history_.size() > config_.sentiment_history_size) {
        conscience_history_.pop_front();
    }
}

void ConscienceEngine::updateAffectiveBackground(double new_sentiment) {
    // Moyenne mobile exponentielle pour le fond affectif
    const double smoothing = 0.1;  // Lissage fort pour stabilité
    affective_background_ = smoothing * new_sentiment + (1.0 - smoothing) * affective_background_;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ACCÈS À L'ÉTAT
// ═══════════════════════════════════════════════════════════════════════════════

const ConscienceState& ConscienceEngine::getLastConscience() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_conscience_;
}

const SentimentState& ConscienceEngine::getLastSentiment() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_sentiment_;
}

double ConscienceEngine::getAffectiveBackground() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return affective_background_;
}

double ConscienceEngine::getWisdom() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return wisdom_;
}

const ConscienceConfig& ConscienceEngine::getConfig() const {
    return config_;
}

// ═══════════════════════════════════════════════════════════════════════════════
// GESTION SAGESSE
// ═══════════════════════════════════════════════════════════════════════════════

void ConscienceEngine::addExperience(double experience_value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Croissance logarithmique pour éviter explosion
    double growth = config_.wisdom_growth_rate * std::log1p(std::abs(experience_value));
    wisdom_ = std::min(wisdom_ + growth, config_.wisdom_max);
}

void ConscienceEngine::resetWisdom() {
    std::lock_guard<std::mutex> lock(mutex_);
    wisdom_ = config_.wisdom_initial;
}

// ═══════════════════════════════════════════════════════════════════════════════
// MODULATION COEFFICIENTS
// ═══════════════════════════════════════════════════════════════════════════════

void ConscienceEngine::modulateEmotionCoefficients(const std::array<double, 24>& modulation) {
    std::lock_guard<std::mutex> lock(mutex_);
    alpha_modulation_ = modulation;
}

void ConscienceEngine::modulateMemoryCoefficients(double omega_MCT, double omega_MLT,
                                                   double omega_MP, double omega_ME,
                                                   double omega_MS, double omega_MA) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.omega_MCT = omega_MCT;
    config_.omega_MLT = omega_MLT;
    config_.omega_MP = omega_MP;
    config_.omega_ME = omega_ME;
    config_.omega_MS = omega_MS;
    config_.omega_MA = omega_MA;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════════

void ConscienceEngine::setConscienceUpdateCallback(ConscienceUpdateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    conscience_callback_ = std::move(callback);
}

void ConscienceEngine::setSentimentUpdateCallback(SentimentUpdateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    sentiment_callback_ = std::move(callback);
}

void ConscienceEngine::setTraumaActivationCallback(TraumaActivationCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    trauma_callback_ = std::move(callback);
}

void ConscienceEngine::setAffectiveBackgroundCallback(AffectiveBackgroundCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    affective_callback_ = std::move(callback);
}

// ═══════════════════════════════════════════════════════════════════════════════
// CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════════

void ConscienceEngine::setConfig(const ConscienceConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

// ═══════════════════════════════════════════════════════════════════════════════
// SÉRIALISATION JSON
// ═══════════════════════════════════════════════════════════════════════════════

std::string ConscienceEngine::toJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);
    
    oss << "{\n";
    oss << "  \"conscience\": {\n";
    oss << "    \"Ct\": " << last_conscience_.Ct << ",\n";
    oss << "    \"emotion_component\": " << last_conscience_.emotion_component << ",\n";
    oss << "    \"memory_component\": " << last_conscience_.memory_component << ",\n";
    oss << "    \"trauma_component\": " << last_conscience_.trauma_component << ",\n";
    oss << "    \"feedback_component\": " << last_conscience_.feedback_component << ",\n";
    oss << "    \"environment_component\": " << last_conscience_.environment_component << ",\n";
    oss << "    \"wisdom_factor\": " << last_conscience_.wisdom_factor << ",\n";
    oss << "    \"active_pattern\": \"" << last_conscience_.active_pattern << "\",\n";
    oss << "    \"has_trauma\": " << (last_conscience_.hasTrauma() ? "true" : "false") << "\n";
    oss << "  },\n";
    
    oss << "  \"sentiment\": {\n";
    oss << "    \"Ft\": " << last_sentiment_.Ft << ",\n";
    oss << "    \"Ft_raw\": " << last_sentiment_.Ft_raw << ",\n";
    oss << "    \"accumulated_conscience\": " << last_sentiment_.accumulated_conscience << ",\n";
    oss << "    \"feedback_influence\": " << last_sentiment_.feedback_influence << ",\n";
    oss << "    \"history_depth\": " << last_sentiment_.history_depth << "\n";
    oss << "  },\n";
    
    oss << "  \"affective_background\": " << affective_background_ << ",\n";
    oss << "  \"wisdom\": " << wisdom_ << ",\n";
    oss << "  \"history_size\": " << conscience_history_.size() << "\n";
    oss << "}\n";
    
    return oss.str();
}

bool ConscienceEngine::fromJson(const std::string& /*json*/) {
    // TODO: Implémenter le parsing JSON si nécessaire
    return false;
}

} // namespace MCEE
