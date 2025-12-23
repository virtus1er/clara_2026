#include "ConscienceEngine.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace mcee {

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTEUR
// ═══════════════════════════════════════════════════════════════════════════

ConscienceEngine::ConscienceEngine(const ConscienceConfig& config)
    : config_(config)
    , wisdom_(config.wisdom_base)
{
    current_state_.timestamp = std::chrono::steady_clock::now();
    current_state_.wisdom = wisdom_;
}

// ═══════════════════════════════════════════════════════════════════════════
// MISE À JOUR PRINCIPALE
// ═══════════════════════════════════════════════════════════════════════════

ConscienceSentimentState ConscienceEngine::update(
    const EmotionalState& emotions,
    const std::vector<MemoryActivation>& memories,
    const FeedbackState& feedback,
    const EnvironmentState& environment)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Calculer les contributions
    double emotional_contrib = computeEmotionalContribution(emotions);
    double memory_contrib = computeMemoryContribution(memories);
    double trauma_contrib = computeTraumaContribution();
    double feedback_contrib = feedback.combined();
    double environment_contrib = environment.score();

    // Calculer le niveau de conscience Ct
    double consciousness = computeConsciousness(
        emotional_contrib,
        memory_contrib,
        trauma_contrib,
        feedback_contrib,
        environment_contrib
    );

    // Calculer le sentiment Ft
    double sentiment = computeSentiment(
        emotional_contrib,
        memory_contrib,
        feedback_contrib
    );

    // Mettre à jour la moyenne mobile du sentiment
    updateSentimentEMA(sentiment);

    // Construire l'état de sortie
    current_state_.consciousness_level = consciousness;
    current_state_.sentiment = sentiment;
    current_state_.wisdom = wisdom_;

    current_state_.emotional_contribution = emotional_contrib;
    current_state_.memory_contribution = memory_contrib;
    current_state_.trauma_contribution = trauma_contrib;
    current_state_.feedback_contribution = feedback_contrib;
    current_state_.environment_contribution = environment_contrib;

    current_state_.sentiment_moving_average = sentiment_ema_;
    current_state_.dominant_state = determineDominantState(consciousness, sentiment);
    current_state_.timestamp = std::chrono::steady_clock::now();

    // Appeler le callback si défini
    if (update_callback_) {
        update_callback_(current_state_);
    }

    return current_state_;
}

ConscienceSentimentState ConscienceEngine::updateSimple(const EmotionalState& emotions) {
    return update(
        emotions,
        {},                     // Pas de mémoires
        FeedbackState{},        // Feedback neutre
        EnvironmentState{}      // Environnement neutre
    );
}

// ═══════════════════════════════════════════════════════════════════════════
// GESTION DES TRAUMAS
// ═══════════════════════════════════════════════════════════════════════════

void ConscienceEngine::activateTrauma(const TraumaState& trauma) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Vérifier si le trauma existe déjà
    auto it = std::find_if(active_traumas_.begin(), active_traumas_.end(),
        [&trauma](const TraumaState& t) { return t.id == trauma.id; });

    if (it != active_traumas_.end()) {
        // Mettre à jour le trauma existant
        *it = trauma;
    } else {
        // Ajouter le nouveau trauma
        active_traumas_.push_back(trauma);
    }

    // Alerter si le trauma dépasse le seuil
    if (trauma.intensity >= config_.trauma_alert_threshold && trauma_callback_) {
        trauma_callback_(trauma);
    }
}

void ConscienceEngine::deactivateTrauma(const std::string& trauma_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    active_traumas_.erase(
        std::remove_if(active_traumas_.begin(), active_traumas_.end(),
            [&trauma_id](const TraumaState& t) { return t.id == trauma_id; }),
        active_traumas_.end()
    );
}

std::optional<TraumaState> ConscienceEngine::getDominantTrauma() const {
    std::lock_guard<std::mutex> lock(mutex_);

    if (active_traumas_.empty()) {
        return std::nullopt;
    }

    // Trouver le trauma le plus intense
    auto max_it = std::max_element(active_traumas_.begin(), active_traumas_.end(),
        [](const TraumaState& a, const TraumaState& b) {
            return a.intensity < b.intensity;
        });

    return *max_it;
}

// ═══════════════════════════════════════════════════════════════════════════
// MODULATION MLT
// ═══════════════════════════════════════════════════════════════════════════

void ConscienceEngine::modulateEmotionCoefficients(const std::array<double, 24>& new_alphas) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Normaliser les coefficients pour que leur somme = 1
    double sum = std::accumulate(new_alphas.begin(), new_alphas.end(), 0.0);
    if (sum > 0.0) {
        for (size_t i = 0; i < 24; ++i) {
            config_.alpha_emotions[i] = new_alphas[i] / sum;
        }
    }

    // Notifier MLT de la modulation
    if (mlt_callback_) {
        mlt_callback_(config_.alpha_emotions);
    }
}

const std::array<double, 24>& ConscienceEngine::getEmotionCoefficients() const {
    return config_.alpha_emotions;
}

// ═══════════════════════════════════════════════════════════════════════════
// SAGESSE (Wt)
// ═══════════════════════════════════════════════════════════════════════════

void ConscienceEngine::addExperience(double amount) {
    std::lock_guard<std::mutex> lock(mutex_);

    experience_ += amount;

    // Wt = wisdom_base + wisdom_growth_rate * log(1 + experience)
    wisdom_ = config_.wisdom_base +
              config_.wisdom_growth_rate * std::log1p(experience_);
}

double ConscienceEngine::getWisdom() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return wisdom_;
}

// ═══════════════════════════════════════════════════════════════════════════
// ACCÈS À L'ÉTAT
// ═══════════════════════════════════════════════════════════════════════════

ConscienceSentimentState ConscienceEngine::getCurrentState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_state_;
}

double ConscienceEngine::getSentimentMovingAverage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sentiment_ema_;
}

// ═══════════════════════════════════════════════════════════════════════════
// CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════

void ConscienceEngine::setUpdateCallback(ConscienceUpdateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    update_callback_ = std::move(callback);
}

void ConscienceEngine::setTraumaAlertCallback(TraumaAlertCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    trauma_callback_ = std::move(callback);
}

void ConscienceEngine::setMLTModulationCallback(MLTModulationCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    mlt_callback_ = std::move(callback);
}

// ═══════════════════════════════════════════════════════════════════════════
// CALCULS INTERNES
// ═══════════════════════════════════════════════════════════════════════════

double ConscienceEngine::computeEmotionalContribution(const EmotionalState& emotions) const {
    // Σ αi·Ei(t)
    double sum = 0.0;
    for (size_t i = 0; i < 24; ++i) {
        sum += config_.alpha_emotions[i] * emotions.emotions[i];
    }
    return sum;
}

double ConscienceEngine::computeMemoryContribution(
    const std::vector<MemoryActivation>& memories) const {

    if (memories.empty()) {
        return 0.0;
    }

    // Mtotal = Σ (activation_strength × emotional_resonance)
    double total = 0.0;
    for (const auto& mem : memories) {
        total += mem.activation_strength * mem.emotional_resonance;
    }

    // Normaliser par le nombre de mémoires (avec plafond)
    return std::min(total / std::max(memories.size(), (size_t)1), 1.0);
}

double ConscienceEngine::computeTraumaContribution() const {
    if (active_traumas_.empty()) {
        return 0.0;
    }

    // Trouver le trauma dominant
    double max_intensity = 0.0;
    for (const auto& trauma : active_traumas_) {
        if (trauma.is_active && trauma.intensity > max_intensity) {
            max_intensity = trauma.intensity;
        }
    }

    // Pondérer par omega_trauma (priorité absolue)
    return config_.omega_trauma * max_intensity;
}

double ConscienceEngine::computeConsciousness(
    double emotional_contrib,
    double memory_contrib,
    double trauma_contrib,
    double feedback_contrib,
    double environment_contrib) const
{
    // Ct = (Σ αi·Ei(t) + Mtotal(t) + Tdominant(t) + β·Fbt + δ·Ent) × Wt

    double raw_consciousness =
        emotional_contrib +
        memory_contrib +
        trauma_contrib +
        config_.beta_memory * feedback_contrib +
        config_.delta_environment * environment_contrib;

    // Appliquer la sagesse comme multiplicateur
    double consciousness = raw_consciousness * wisdom_;

    // Normaliser entre 0 et 1 (avec saturation douce via tanh)
    return std::tanh(consciousness);
}

double ConscienceEngine::computeSentiment(
    double emotional_contrib,
    double memory_contrib,
    double feedback_value) const
{
    // Ft = tanh(Σ γk·Ck + λ·Fbt)
    // Où γk sont des coefficients implicites (ici simplifiés à 1.0)

    double raw_sentiment =
        emotional_contrib * 0.5 +      // γ_emotion
        memory_contrib * 0.3 +          // γ_memory
        config_.lambda_feedback * feedback_value;

    return std::tanh(raw_sentiment);
}

void ConscienceEngine::updateSentimentEMA(double new_sentiment) {
    // Moyenne mobile exponentielle
    // EMA_t = α × value_t + (1 - α) × EMA_{t-1}

    sentiment_ema_ = config_.sentiment_smoothing * new_sentiment +
                     (1.0 - config_.sentiment_smoothing) * sentiment_ema_;

    // Garder aussi un historique pour analyse
    sentiment_history_.push_back(new_sentiment);

    // Limiter la taille de l'historique (5 minutes à 1Hz = 300 samples)
    const size_t max_history = static_cast<size_t>(config_.sentiment_window_seconds);
    while (sentiment_history_.size() > max_history) {
        sentiment_history_.pop_front();
    }
}

std::string ConscienceEngine::determineDominantState(
    double consciousness,
    double sentiment) const
{
    // Combinaison de conscience et sentiment pour déterminer l'état

    if (consciousness < config_.min_consciousness_threshold) {
        return "dormant";  // Conscience trop basse
    }

    if (sentiment > 0.3) {
        return "positive";
    } else if (sentiment < -0.3) {
        return "negative";
    } else {
        return "neutral";
    }
}

} // namespace mcee
