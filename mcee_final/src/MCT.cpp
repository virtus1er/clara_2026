/**
 * @file MCT.cpp
 * @brief Implémentation de la Mémoire Court Terme
 */

#include "MCT.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace mcee {

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTEURS
// ═══════════════════════════════════════════════════════════════════════════

MCT::MCT() : config_() {}

MCT::MCT(const MCTConfig& config) : config_(config) {}

// ═══════════════════════════════════════════════════════════════════════════
// GESTION DU BUFFER
// ═══════════════════════════════════════════════════════════════════════════

bool MCT::push(const EmotionalState& state) {
    // Validation avant insertion (sans lock pour éviter deadlock)
    if (config_.enable_input_validation) {
        ValidationResult validation = validate(state);

        if (!validation.valid) {
            // Notifier le callback si défini
            if (validation_callback_) {
                validation_callback_(validation);
            }

            if (config_.log_validation_errors) {
                std::cerr << "[MCT] Validation error: " << validation.error_code
                          << " - " << validation.error_message << "\n";
            }

            if (config_.reject_on_validation_failure) {
                return false;
            }
            // Sinon, on sanitize et on continue
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Sanitize l'état si la validation est active (même si valide, clamp les valeurs)
    EmotionalState safe_state = config_.enable_input_validation ? sanitize(state) : state;

    TimestampedState ts(safe_state);
    buffer_.push_back(ts);

    // Limite de taille
    while (buffer_.size() > config_.max_size) {
        buffer_.pop_front();
    }

    invalidateCache();

    // Callbacks
    if (stability_callback_ && buffer_.size() >= 2) {
        auto stability = getStability();
        auto volatility = getVolatility();
        stability_callback_(stability, volatility);
    }

    return true;
}

void MCT::pushWithSpeech(const EmotionalState& state,
                          double sentiment,
                          double arousal,
                          const std::string& context) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Sanitize l'état si la validation est active (comme dans push())
    EmotionalState safe_state = config_.enable_input_validation ? sanitize(state) : state;

    TimestampedState ts(safe_state);
    ts.speech_sentiment = sentiment;
    ts.speech_arousal = arousal;
    ts.context = context;

    buffer_.push_back(ts);

    while (buffer_.size() > config_.max_size) {
        buffer_.pop_front();
    }

    invalidateCache();

    // Callbacks (comme dans push())
    if (stability_callback_ && buffer_.size() >= 2) {
        auto stability = getStability();
        auto volatility = getVolatility();
        stability_callback_(stability, volatility);
    }
}

void MCT::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    buffer_.clear();
    invalidateCache();
}

void MCT::pruneOld() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - std::chrono::duration<double>(config_.time_window_seconds);
    
    while (!buffer_.empty() && buffer_.front().timestamp < cutoff) {
        buffer_.pop_front();
    }
    
    invalidateCache();
}

// ═══════════════════════════════════════════════════════════════════════════
// INTÉGRATION ET ANALYSE
// ═══════════════════════════════════════════════════════════════════════════

MCTIntegration MCT::integrate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Retourne le cache si valide
    if (cache_valid_ && cached_integration_) {
        return *cached_integration_;
    }
    
    MCTIntegration result;
    result.sample_count = buffer_.size();
    
    if (buffer_.empty()) {
        result.stability = 0.0;
        result.volatility = 0.0;
        result.trend = 0.0;
        result.time_span_seconds = 0.0;
        return result;
    }
    
    // Calcul de la fenêtre temporelle
    auto first_time = buffer_.front().timestamp;
    auto last_time = buffer_.back().timestamp;
    result.time_span_seconds = std::chrono::duration<double>(last_time - first_time).count();
    
    // Moyenne pondérée des émotions
    std::array<double, 24> weighted_sum{};
    double total_weight = 0.0;
    
    for (const auto& ts : buffer_) {
        double weight = computeWeight(ts.timestamp);
        total_weight += weight;
        
        for (size_t i = 0; i < 24; ++i) {
            weighted_sum[i] += ts.state.emotions[i] * weight;
        }
    }
    
    if (total_weight > 0.0) {
        for (size_t i = 0; i < 24; ++i) {
            result.integrated_state.emotions[i] = weighted_sum[i] / total_weight;
        }
    }
    
    // Calcul de E_global
    double sum_e = std::accumulate(result.integrated_state.emotions.begin(),
                                    result.integrated_state.emotions.end(), 0.0);
    result.integrated_state.E_global = sum_e / 24.0;
    
    // Calcul de la stabilité (inverse de l'écart-type moyen)
    if (buffer_.size() >= 2) {
        double sum_variance = 0.0;
        for (size_t i = 0; i < 24; ++i) {
            double mean = result.integrated_state.emotions[i];
            double var = 0.0;
            for (const auto& ts : buffer_) {
                double diff = ts.state.emotions[i] - mean;
                var += diff * diff;
            }
            var /= buffer_.size();
            sum_variance += var;
        }
        double avg_std = std::sqrt(sum_variance / 24.0);
        result.stability = std::max(0.0, 1.0 - avg_std * 2.0);  // Normalisation
    } else {
        result.stability = 1.0;
    }
    
    // Calcul de la volatilité (changements frame à frame)
    result.volatility = 1.0 - result.stability;
    
    // Calcul de la tendance (régression linéaire simplifiée sur E_global)
    if (buffer_.size() >= 3) {
        double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_xx = 0.0;
        double n = buffer_.size();
        
        for (size_t i = 0; i < buffer_.size(); ++i) {
            double x = static_cast<double>(i);
            double y = buffer_[i].state.E_global;
            sum_x += x;
            sum_y += y;
            sum_xy += x * y;
            sum_xx += x * x;
        }
        
        double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x + 1e-10);
        result.trend = std::clamp(slope * 10.0, -1.0, 1.0);  // Normalisation
    } else {
        result.trend = 0.0;
    }
    
    // Vélocité par émotion
    result.emotion_velocity = computeVelocity();
    
    // Met en cache
    cached_integration_ = result;
    cache_valid_ = true;
    
    return result;
}

std::optional<EmotionalSignature> MCT::extractSignature() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (buffer_.size() < config_.min_samples_for_signature) {
        return std::nullopt;
    }
    
    EmotionalSignature sig;
    
    // Moyenne des émotions
    for (size_t i = 0; i < 24; ++i) {
        double sum = 0.0;
        for (const auto& ts : buffer_) {
            sum += ts.state.emotions[i];
        }
        sig.mean_emotions[i] = sum / buffer_.size();
    }
    
    // Écart-type par émotion
    for (size_t i = 0; i < 24; ++i) {
        double sum_sq = 0.0;
        for (const auto& ts : buffer_) {
            double diff = ts.state.emotions[i] - sig.mean_emotions[i];
            sum_sq += diff * diff;
        }
        sig.std_dev[i] = std::sqrt(sum_sq / buffer_.size());
    }
    
    // Tendance, accélération, position du pic, oscillations
    sig.trend.fill(0.0);
    sig.acceleration.fill(0.0);
    sig.peak_position.fill(0.5);
    sig.oscillation_count.fill(0);

    if (buffer_.size() >= 5) {
        size_t quarter = buffer_.size() / 4;
        size_t third = buffer_.size() / 3;

        for (size_t i = 0; i < 24; ++i) {
            // Tendance (1ère dérivée) : différence début/fin
            double early_avg = 0.0, late_avg = 0.0;
            for (size_t j = 0; j < third; ++j) {
                early_avg += buffer_[j].state.emotions[i];
            }
            for (size_t j = buffer_.size() - third; j < buffer_.size(); ++j) {
                late_avg += buffer_[j].state.emotions[i];
            }
            early_avg /= third;
            late_avg /= third;
            sig.trend[i] = late_avg - early_avg;

            // Accélération (2ème dérivée) : différence des tendances
            double mid_avg = 0.0;
            size_t mid_start = buffer_.size() / 3;
            size_t mid_end = 2 * buffer_.size() / 3;
            for (size_t j = mid_start; j < mid_end; ++j) {
                mid_avg += buffer_[j].state.emotions[i];
            }
            mid_avg /= (mid_end - mid_start);
            double early_trend = mid_avg - early_avg;
            double late_trend = late_avg - mid_avg;
            sig.acceleration[i] = late_trend - early_trend;

            // Position du pic [0, 1]
            double max_val = 0.0;
            size_t max_pos = 0;
            for (size_t j = 0; j < buffer_.size(); ++j) {
                if (buffer_[j].state.emotions[i] > max_val) {
                    max_val = buffer_[j].state.emotions[i];
                    max_pos = j;
                }
            }
            sig.peak_position[i] = static_cast<double>(max_pos) / buffer_.size();

            // Oscillations (changements de direction)
            int oscillations = 0;
            double prev_diff = 0.0;
            for (size_t j = 1; j < buffer_.size(); ++j) {
                double diff = buffer_[j].state.emotions[i] - buffer_[j-1].state.emotions[i];
                if (j > 1 && prev_diff * diff < 0 && std::abs(diff) > 0.01) {
                    oscillations++;
                }
                prev_diff = diff;
            }
            sig.oscillation_count[i] = oscillations;
        }
    }
    
    // Métriques globales
    sig.global_intensity = std::accumulate(sig.mean_emotions.begin(),
                                            sig.mean_emotions.end(), 0.0) / 24.0;
    
    // Valence (joie - tristesse + amour - peur + ...)
    // Indices: joy=0, sadness=1, fear=2, love=8, hope=14, despair=15
    sig.global_valence = (sig.mean_emotions[0] + sig.mean_emotions[8] + sig.mean_emotions[14]) -
                         (sig.mean_emotions[1] + sig.mean_emotions[2] + sig.mean_emotions[15]);
    sig.global_valence = std::clamp(sig.global_valence / 3.0, -1.0, 1.0);
    
    // Arousal (excitation + surprise + peur - sérénité - ennui)
    // Indices: excitement=22, surprise=4, fear=2, serenity=23, boredom=16
    sig.global_arousal = (sig.mean_emotions[22] + sig.mean_emotions[4] + sig.mean_emotions[2]) -
                         (sig.mean_emotions[23] + sig.mean_emotions[16]);
    sig.global_arousal = std::clamp((sig.global_arousal + 2.0) / 4.0, 0.0, 1.0);
    
    // Stabilité
    double avg_std = std::accumulate(sig.std_dev.begin(), sig.std_dev.end(), 0.0) / 24.0;
    sig.stability = std::max(0.0, 1.0 - avg_std * 2.0);

    // Fréquence dominante (estimation simple basée sur oscillations E_global)
    if (buffer_.size() >= 10) {
        int global_oscillations = 0;
        double prev_e = buffer_[0].state.E_global;
        double prev_diff = 0.0;
        for (size_t j = 1; j < buffer_.size(); ++j) {
            double diff = buffer_[j].state.E_global - prev_e;
            if (j > 1 && prev_diff * diff < 0 && std::abs(diff) > 0.02) {
                global_oscillations++;
            }
            prev_diff = diff;
            prev_e = buffer_[j].state.E_global;
        }
        // Fréquence = oscillations / durée en secondes
        sig.dominant_frequency = global_oscillations / config_.time_window_seconds;
    } else {
        sig.dominant_frequency = 0.0;
    }

    return sig;
}

double MCT::similarityWith(const EmotionalSignature& other) const {
    auto sig_opt = extractSignature();
    if (!sig_opt) {
        return 0.0;
    }
    
    const auto& sig = *sig_opt;
    
    // Similarité cosinus sur les émotions moyennes
    double dot = 0.0, norm1 = 0.0, norm2 = 0.0;
    for (size_t i = 0; i < 24; ++i) {
        dot += sig.mean_emotions[i] * other.mean_emotions[i];
        norm1 += sig.mean_emotions[i] * sig.mean_emotions[i];
        norm2 += other.mean_emotions[i] * other.mean_emotions[i];
    }
    
    if (norm1 < 1e-10 || norm2 < 1e-10) {
        return 0.0;
    }
    
    return dot / (std::sqrt(norm1) * std::sqrt(norm2));
}

std::optional<TimestampedState> MCT::getLatest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (buffer_.empty()) {
        return std::nullopt;
    }
    return buffer_.back();
}

std::vector<TimestampedState> MCT::getRecent(size_t n) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<TimestampedState> result;
    size_t start = (buffer_.size() > n) ? buffer_.size() - n : 0;
    
    for (size_t i = start; i < buffer_.size(); ++i) {
        result.push_back(buffer_[i]);
    }
    
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// MÉTRIQUES
// ═══════════════════════════════════════════════════════════════════════════

double MCT::getStability() const {
    auto integration = integrate();
    return integration.stability;
}

double MCT::getVolatility() const {
    auto integration = integrate();
    return integration.volatility;
}

double MCT::getTrend() const {
    auto integration = integrate();
    return integration.trend;
}

bool MCT::hasEnoughData() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.size() >= config_.min_samples_for_signature;
}

size_t MCT::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.size();
}

bool MCT::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buffer_.empty();
}

// ═══════════════════════════════════════════════════════════════════════════
// SÉRIALISATION
// ═══════════════════════════════════════════════════════════════════════════

nlohmann::json MCT::toJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json j;
    j["config"] = {
        {"max_size", config_.max_size},
        {"time_window_seconds", config_.time_window_seconds},
        {"decay_factor", config_.decay_factor},
        {"stability_threshold", config_.stability_threshold},
        {"volatility_threshold", config_.volatility_threshold},
        {"use_exponential_weighting", config_.use_exponential_weighting},
        {"min_samples_for_signature", config_.min_samples_for_signature}
    };
    
    j["buffer_size"] = buffer_.size();
    
    // Intégration actuelle
    auto integration = integrate();
    j["integration"] = {
        {"stability", integration.stability},
        {"volatility", integration.volatility},
        {"trend", integration.trend},
        {"sample_count", integration.sample_count},
        {"time_span_seconds", integration.time_span_seconds},
        {"E_global", integration.integrated_state.E_global}
    };
    
    // Signature si disponible
    auto sig_opt = extractSignature();
    if (sig_opt) {
        const auto& sig = *sig_opt;
        j["signature"] = {
            {"global_intensity", sig.global_intensity},
            {"global_valence", sig.global_valence},
            {"global_arousal", sig.global_arousal},
            {"stability", sig.stability}
        };
        
        nlohmann::json emotions_json;
        for (size_t i = 0; i < 24; ++i) {
            emotions_json.push_back(sig.mean_emotions[i]);
        }
        j["signature"]["mean_emotions"] = emotions_json;
    }
    
    return j;
}

void MCT::fromJson(const nlohmann::json& j) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (j.contains("config")) {
        const auto& c = j["config"];
        if (c.contains("max_size")) config_.max_size = c["max_size"];
        if (c.contains("time_window_seconds")) config_.time_window_seconds = c["time_window_seconds"];
        if (c.contains("decay_factor")) config_.decay_factor = c["decay_factor"];
        if (c.contains("stability_threshold")) config_.stability_threshold = c["stability_threshold"];
        if (c.contains("volatility_threshold")) config_.volatility_threshold = c["volatility_threshold"];
        if (c.contains("use_exponential_weighting")) config_.use_exponential_weighting = c["use_exponential_weighting"];
        if (c.contains("min_samples_for_signature")) config_.min_samples_for_signature = c["min_samples_for_signature"];
    }
    
    invalidateCache();
}

// ═══════════════════════════════════════════════════════════════════════════
// MÉTHODES PRIVÉES
// ═══════════════════════════════════════════════════════════════════════════

void MCT::invalidateCache() {
    cache_valid_ = false;
    cached_integration_.reset();
}

double MCT::computeWeight(const std::chrono::steady_clock::time_point& timestamp) const {
    if (!config_.use_exponential_weighting) {
        return 1.0;
    }
    
    auto now = std::chrono::steady_clock::now();
    double age = std::chrono::duration<double>(now - timestamp).count();
    
    // Poids exponentiel décroissant
    double half_life = config_.time_window_seconds / 3.0;
    return std::exp(-age / half_life);
}

std::array<double, 24> MCT::computeVelocity() const {
    std::array<double, 24> velocity{};

    if (buffer_.size() < 2) {
        return velocity;
    }

    // Différence entre état actuel et état précédent
    const auto& current = buffer_.back();
    const auto& previous = buffer_[buffer_.size() - 2];

    double dt = std::chrono::duration<double>(current.timestamp - previous.timestamp).count();
    if (dt < 1e-6) {
        return velocity;
    }

    for (size_t i = 0; i < 24; ++i) {
        velocity[i] = (current.state.emotions[i] - previous.state.emotions[i]) / dt;
    }

    return velocity;
}

// ═══════════════════════════════════════════════════════════════════════════
// VALIDATION D'ENTRÉE
// ═══════════════════════════════════════════════════════════════════════════

ValidationResult MCT::validate(const EmotionalState& state) const {
    ValidationResult result;
    result.valid = true;

    // 1. Vérifier les valeurs NaN ou infinies
    for (size_t i = 0; i < 24; ++i) {
        if (std::isnan(state.emotions[i]) || std::isinf(state.emotions[i])) {
            result.valid = false;
            result.error_code = "NAN_OR_INF";
            result.error_message = "Emotion " + std::to_string(i) + " contient NaN ou Inf";
            result.invalid_emotion_index = i;
            result.invalid_value = state.emotions[i];
            return result;
        }
    }

    // Vérifier E_global aussi
    if (std::isnan(state.E_global) || std::isinf(state.E_global)) {
        result.valid = false;
        result.error_code = "NAN_OR_INF";
        result.error_message = "E_global contient NaN ou Inf";
        result.invalid_value = state.E_global;
        return result;
    }

    // 2. Vérifier les valeurs hors limites
    for (size_t i = 0; i < 24; ++i) {
        if (state.emotions[i] < config_.emotion_min || state.emotions[i] > config_.emotion_max) {
            result.valid = false;
            result.error_code = "OUT_OF_RANGE";
            result.error_message = "Emotion " + std::to_string(i) + " hors limites [" +
                                   std::to_string(config_.emotion_min) + ", " +
                                   std::to_string(config_.emotion_max) + "]";
            result.invalid_emotion_index = i;
            result.invalid_value = state.emotions[i];
            return result;
        }
    }

    // 3. Vérifier qu'il y a au moins quelques émotions non-nulles (détection panne capteur)
    size_t nonzero_count = 0;
    for (size_t i = 0; i < 24; ++i) {
        if (state.emotions[i] > 0.001) {
            nonzero_count++;
        }
    }
    if (nonzero_count < static_cast<size_t>(config_.min_nonzero_emotions)) {
        result.valid = false;
        result.error_code = "ALL_ZERO";
        result.error_message = "Moins de " + std::to_string(config_.min_nonzero_emotions) +
                               " émotions non-nulles (état potentiellement invalide)";
        result.invalid_value = static_cast<double>(nonzero_count);
        return result;
    }

    // 4. Vérifier les sauts extrêmes par rapport à l'état précédent
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!buffer_.empty()) {
            const auto& prev_state = buffer_.back().state;
            for (size_t i = 0; i < 24; ++i) {
                double jump = std::abs(state.emotions[i] - prev_state.emotions[i]);
                if (jump > config_.max_jump_threshold) {
                    result.valid = false;
                    result.error_code = "EXTREME_JUMP";
                    result.error_message = "Emotion " + std::to_string(i) +
                                           " a sauté de " + std::to_string(jump) +
                                           " (max autorisé: " + std::to_string(config_.max_jump_threshold) + ")";
                    result.invalid_emotion_index = i;
                    result.invalid_value = jump;
                    return result;
                }
            }
        }
    }

    return result;
}

EmotionalState MCT::sanitize(const EmotionalState& state) const {
    EmotionalState safe = state;

    // Clamp toutes les émotions dans les limites
    for (size_t i = 0; i < 24; ++i) {
        // Remplacer NaN/Inf par 0
        if (std::isnan(safe.emotions[i]) || std::isinf(safe.emotions[i])) {
            safe.emotions[i] = 0.0;
        }
        // Clamp dans les limites
        safe.emotions[i] = std::clamp(safe.emotions[i], config_.emotion_min, config_.emotion_max);
    }

    // Limiter les sauts extrêmes si on a un état précédent
    // NOTE: Cette méthode est appelée depuis push() qui détient déjà mutex_
    // donc on accède directement à buffer_ sans lock supplémentaire
    if (!buffer_.empty()) {
        const auto& prev_state = buffer_.back().state;
        for (size_t i = 0; i < 24; ++i) {
            double diff = safe.emotions[i] - prev_state.emotions[i];
            if (std::abs(diff) > config_.max_jump_threshold) {
                // Limiter le saut au max autorisé
                double sign = (diff > 0) ? 1.0 : -1.0;
                safe.emotions[i] = prev_state.emotions[i] + sign * config_.max_jump_threshold;
            }
        }
    }

    // Garantir qu'au moins une émotion est non-nulle (éviter ALL_ZERO)
    size_t nonzero_count = 0;
    size_t max_idx = 0;
    double max_val = safe.emotions[0];
    for (size_t i = 0; i < 24; ++i) {
        if (safe.emotions[i] > 0.001) {
            nonzero_count++;
        }
        if (safe.emotions[i] > max_val) {
            max_val = safe.emotions[i];
            max_idx = i;
        }
    }
    if (nonzero_count < static_cast<size_t>(config_.min_nonzero_emotions)) {
        // Préserver l'émotion dominante à un niveau minimal
        safe.emotions[max_idx] = std::max(safe.emotions[max_idx], 0.01);
    }

    // Recalculer E_global
    double sum = 0.0;
    for (size_t i = 0; i < 24; ++i) {
        sum += safe.emotions[i];
    }
    safe.E_global = sum / 24.0;

    // Sanitizer variance_global
    if (std::isnan(safe.variance_global) || std::isinf(safe.variance_global)) {
        safe.variance_global = 0.0;
    }
    safe.variance_global = std::max(0.0, safe.variance_global);

    return safe;
}

} // namespace mcee
