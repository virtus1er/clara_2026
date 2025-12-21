/**
 * @file PatternMatcher.cpp
 * @brief Implémentation du PatternMatcher
 */

#include "PatternMatcher.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>

namespace mcee {

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTEURS
// ═══════════════════════════════════════════════════════════════════════════

PatternMatcher::PatternMatcher() : config_() {}

PatternMatcher::PatternMatcher(std::shared_ptr<MCT> mct,
                               std::shared_ptr<MLT> mlt,
                               const PatternMatcherConfig& config)
    : mct_(mct), mlt_(mlt), config_(config) {}

// ═══════════════════════════════════════════════════════════════════════════
// MATCHING PRINCIPAL
// ═══════════════════════════════════════════════════════════════════════════

MatchResult PatternMatcher::match() {
    if (!mct_ || !mlt_) {
        MatchResult empty;
        empty.pattern_name = "UNKNOWN";
        empty.similarity = 0.0;
        empty.confidence = 0.0;
        return empty;
    }
    
    // Extrait la signature de la MCT
    auto signature_opt = mct_->extractSignature();
    
    if (!signature_opt) {
        // Pas assez de données - retourne le pattern actuel ou SERENITE par défaut
        if (!current_pattern_id_.empty()) {
            auto pattern_opt = mlt_->getPattern(current_pattern_id_);
            if (pattern_opt) {
                MatchResult result;
                result.pattern_id = current_pattern_id_;
                result.pattern_name = pattern_opt->name;
                result.similarity = current_match_similarity_;
                result.confidence = pattern_opt->confidence;
                result.alpha = pattern_opt->alpha;
                result.beta = pattern_opt->beta;
                result.gamma = pattern_opt->gamma;
                result.delta = pattern_opt->delta;
                result.theta = pattern_opt->theta;
                result.emergency_threshold = pattern_opt->emergency_threshold;
                result.memory_trigger_threshold = pattern_opt->memory_trigger_threshold;
                return result;
            }
        }
        
        // Défaut : SÉRÉNITÉ
        auto serenity = mlt_->getPatternByName("SERENITE");
        if (serenity) {
            MatchResult result;
            result.pattern_id = serenity->id;
            result.pattern_name = serenity->name;
            result.similarity = 0.5;
            result.confidence = serenity->confidence;
            result.alpha = serenity->alpha;
            result.beta = serenity->beta;
            result.gamma = serenity->gamma;
            result.delta = serenity->delta;
            result.theta = serenity->theta;
            result.emergency_threshold = serenity->emergency_threshold;
            result.memory_trigger_threshold = serenity->memory_trigger_threshold;
            return result;
        }
        
        // Fallback ultime
        MatchResult fallback;
        fallback.pattern_name = "DEFAULT";
        fallback.alpha = 0.3;
        fallback.beta = 0.2;
        fallback.gamma = 0.15;
        fallback.delta = 0.1;
        fallback.theta = 0.25;
        fallback.emergency_threshold = 0.75;
        fallback.memory_trigger_threshold = 0.5;
        return fallback;
    }
    
    return matchSignature(*signature_opt);
}

MatchResult PatternMatcher::matchSignature(const EmotionalSignature& signature) {
    total_matches_++;
    
    // Trouve les meilleurs matches en MLT
    auto matches = mlt_->findMatches(signature, config_.max_matches_returned);
    
    if (matches.empty()) {
        // Aucun match - créer un nouveau pattern ?
        return createResultForNewPattern(signature);
    }
    
    const auto& best_match = matches[0];
    sum_similarities_ += best_match.similarity;
    
    // Décision basée sur la similarité
    MatchDecision decision = getDecision(signature);
    
    switch (decision) {
        case MatchDecision::USE_EXISTING: {
            // Vérifie si on doit changer de pattern (avec hysteresis)
            if (shouldSwitchPattern(best_match)) {
                // Enregistre la transition
                if (!current_pattern_id_.empty() && current_pattern_id_ != best_match.pattern_id) {
                    mlt_->recordTransition(current_pattern_id_, best_match.pattern_id);
                    transitions_recorded_++;
                    
                    if (transition_callback_) {
                        double prob = mlt_->getPattern(current_pattern_id_)
                            .value_or(EmotionalPattern{}).transition_probabilities[best_match.pattern_id];
                        transition_callback_(current_pattern_id_, best_match.pattern_id, prob);
                    }
                }
                
                updateHistory(best_match.pattern_id);
                current_pattern_id_ = best_match.pattern_id;
                current_match_similarity_ = best_match.similarity;
                frames_in_current_pattern_ = 1;
            } else {
                frames_in_current_pattern_++;
            }
            
            // Enregistre l'activation
            mlt_->recordActivation(current_pattern_id_);
            
            return createResultFromPattern(best_match, 
                                           std::vector<PatternMatch>(matches.begin() + 1, matches.end()));
        }
        
        case MatchDecision::MODIFY_EXISTING: {
            // Met à jour le pattern avec cette nouvelle observation
            mlt_->updatePattern(best_match.pattern_id, signature);
            
            auto result = createResultFromPattern(best_match,
                                                   std::vector<PatternMatch>(matches.begin() + 1, matches.end()));
            
            updateHistory(best_match.pattern_id);
            current_pattern_id_ = best_match.pattern_id;
            current_match_similarity_ = best_match.similarity;
            frames_in_current_pattern_ = 1;
            
            return result;
        }
        
        case MatchDecision::CREATE_NEW: {
            return createResultForNewPattern(signature);
        }
        
        case MatchDecision::MERGE_PATTERNS: {
            // Si deux patterns sont très proches, suggérer une fusion
            if (matches.size() >= 2 && 
                matches[0].similarity > config_.high_match_threshold &&
                matches[1].similarity > config_.high_match_threshold) {
                // La fusion sera gérée par le système de maintenance
                unmatched_signatures_.push_back(signature);
            }
            return createResultFromPattern(best_match,
                                           std::vector<PatternMatch>(matches.begin() + 1, matches.end()));
        }
        
        case MatchDecision::UNCERTAIN:
        default: {
            // Garde le pattern actuel
            if (!current_pattern_id_.empty()) {
                auto pattern_opt = mlt_->getPattern(current_pattern_id_);
                if (pattern_opt) {
                    PatternMatch current_match;
                    current_match.pattern_id = current_pattern_id_;
                    current_match.pattern_name = pattern_opt->name;
                    current_match.similarity = current_match_similarity_;
                    current_match.confidence = pattern_opt->confidence;
                    current_match.pattern = nullptr;
                    return createResultFromPattern(current_match, matches);
                }
            }
            return createResultFromPattern(best_match, 
                                           std::vector<PatternMatch>(matches.begin() + 1, matches.end()));
        }
    }
}

MatchDecision PatternMatcher::getDecision(const EmotionalSignature& signature) const {
    auto matches = mlt_->findMatches(signature, 3);
    
    if (matches.empty()) {
        // Vérifie si on a assez de stabilité pour créer
        if (mct_ && mct_->getStability() >= config_.min_stability_for_creation) {
            return MatchDecision::CREATE_NEW;
        }
        return MatchDecision::UNCERTAIN;
    }
    
    double best_sim = matches[0].similarity;
    
    if (best_sim >= config_.high_match_threshold) {
        return MatchDecision::USE_EXISTING;
    } else if (best_sim >= config_.medium_match_threshold) {
        return MatchDecision::MODIFY_EXISTING;
    } else if (best_sim >= config_.low_match_threshold) {
        // Zone intermédiaire - dépend de la stabilité
        if (mct_ && mct_->getStability() >= config_.min_stability_for_creation) {
            return MatchDecision::CREATE_NEW;
        }
        return MatchDecision::MODIFY_EXISTING;
    } else {
        return MatchDecision::CREATE_NEW;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// GESTION DES TRANSITIONS
// ═══════════════════════════════════════════════════════════════════════════

void PatternMatcher::notifyPatternChange(const std::string& new_pattern_id) {
    if (!current_pattern_id_.empty() && current_pattern_id_ != new_pattern_id) {
        mlt_->recordTransition(current_pattern_id_, new_pattern_id);
    }
    
    updateHistory(new_pattern_id);
    current_pattern_id_ = new_pattern_id;
    frames_in_current_pattern_ = 0;
}

std::optional<std::string> PatternMatcher::getCurrentPatternId() const {
    if (current_pattern_id_.empty()) return std::nullopt;
    return current_pattern_id_;
}

std::vector<std::pair<std::string, double>> PatternMatcher::getPatternHistory(size_t n) const {
    std::vector<std::pair<std::string, double>> result;
    
    size_t count = std::min(n, pattern_history_.size());
    for (size_t i = 0; i < count; ++i) {
        auto duration = std::chrono::duration<double>(
            (i + 1 < pattern_history_.size() ? 
             pattern_history_[i + 1].second : std::chrono::steady_clock::now()) -
            pattern_history_[i].second
        ).count();
        result.emplace_back(pattern_history_[i].first, duration);
    }
    
    return result;
}

double PatternMatcher::getTransitionProbability(const std::string& to_pattern_id) const {
    if (current_pattern_id_.empty()) return 0.0;
    
    auto pattern_opt = mlt_->getPattern(current_pattern_id_);
    if (!pattern_opt) return 0.0;
    
    auto it = pattern_opt->transition_probabilities.find(to_pattern_id);
    if (it == pattern_opt->transition_probabilities.end()) return 0.0;
    
    return it->second;
}

// ═══════════════════════════════════════════════════════════════════════════
// FEEDBACK ET APPRENTISSAGE
// ═══════════════════════════════════════════════════════════════════════════

void PatternMatcher::provideFeedback(double feedback) {
    if (current_pattern_id_.empty()) return;
    
    mlt_->adjustCoefficients(current_pattern_id_, feedback);
}

void PatternMatcher::confirmCurrentPattern() {
    provideFeedback(1.0);
}

void PatternMatcher::rejectCurrentPattern(const std::string& correct_pattern_id) {
    provideFeedback(-1.0);
    
    if (!correct_pattern_id.empty()) {
        mlt_->adjustCoefficients(correct_pattern_id, 0.5);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// CRÉATION DE PATTERNS
// ═══════════════════════════════════════════════════════════════════════════

std::string PatternMatcher::forceCreatePattern(const std::string& name,
                                                const std::string& description) {
    if (!mct_) return "";
    
    auto signature_opt = mct_->extractSignature();
    if (!signature_opt) return "";
    
    std::string id = mlt_->createPattern(*signature_opt, name, description);
    patterns_created_++;
    
    if (new_pattern_callback_) {
        auto pattern = mlt_->getPattern(id);
        new_pattern_callback_(id, pattern ? pattern->name : name);
    }
    
    return id;
}

std::vector<EmotionalSignature> PatternMatcher::suggestNewPatterns() const {
    // Analyse les signatures non matchées pour trouver des clusters
    return unmatched_signatures_;  // Pour l'instant, retourne toutes les signatures
}

// ═══════════════════════════════════════════════════════════════════════════
// STATISTIQUES
// ═══════════════════════════════════════════════════════════════════════════

double PatternMatcher::getAverageMatchSimilarity() const {
    if (total_matches_ == 0) return 0.0;
    return sum_similarities_ / total_matches_;
}

// ═══════════════════════════════════════════════════════════════════════════
// SÉRIALISATION
// ═══════════════════════════════════════════════════════════════════════════

nlohmann::json PatternMatcher::toJson() const {
    nlohmann::json j;
    
    j["config"] = {
        {"high_match_threshold", config_.high_match_threshold},
        {"medium_match_threshold", config_.medium_match_threshold},
        {"low_match_threshold", config_.low_match_threshold},
        {"hysteresis_margin", config_.hysteresis_margin},
        {"min_frames_before_switch", config_.min_frames_before_switch},
        {"min_stability_for_creation", config_.min_stability_for_creation}
    };
    
    j["state"] = {
        {"current_pattern_id", current_pattern_id_},
        {"frames_in_current", frames_in_current_pattern_},
        {"current_similarity", current_match_similarity_}
    };
    
    j["statistics"] = {
        {"total_matches", total_matches_},
        {"patterns_created", patterns_created_},
        {"transitions_recorded", transitions_recorded_},
        {"average_similarity", getAverageMatchSimilarity()}
    };
    
    return j;
}

void PatternMatcher::fromJson(const nlohmann::json& j) {
    if (j.contains("config")) {
        const auto& c = j["config"];
        if (c.contains("high_match_threshold")) 
            config_.high_match_threshold = c["high_match_threshold"];
        if (c.contains("medium_match_threshold")) 
            config_.medium_match_threshold = c["medium_match_threshold"];
        if (c.contains("low_match_threshold")) 
            config_.low_match_threshold = c["low_match_threshold"];
        if (c.contains("hysteresis_margin")) 
            config_.hysteresis_margin = c["hysteresis_margin"];
        if (c.contains("min_frames_before_switch")) 
            config_.min_frames_before_switch = c["min_frames_before_switch"];
    }
    
    if (j.contains("state")) {
        const auto& s = j["state"];
        if (s.contains("current_pattern_id"))
            current_pattern_id_ = s["current_pattern_id"];
        if (s.contains("frames_in_current"))
            frames_in_current_pattern_ = s["frames_in_current"];
        if (s.contains("current_similarity"))
            current_match_similarity_ = s["current_similarity"];
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// MÉTHODES PRIVÉES
// ═══════════════════════════════════════════════════════════════════════════

MatchResult PatternMatcher::createResultFromPattern(const PatternMatch& match,
                                                     const std::vector<PatternMatch>& alternatives) {
    MatchResult result;
    result.pattern_id = match.pattern_id;
    result.pattern_name = match.pattern_name;
    result.similarity = match.similarity;
    result.confidence = match.confidence;
    result.alternatives = alternatives;
    result.is_new_pattern = false;
    
    // Récupère les coefficients du pattern
    auto pattern_opt = mlt_->getPattern(match.pattern_id);
    if (pattern_opt) {
        result.alpha = pattern_opt->alpha;
        result.beta = pattern_opt->beta;
        result.gamma = pattern_opt->gamma;
        result.delta = pattern_opt->delta;
        result.theta = pattern_opt->theta;
        result.emergency_threshold = pattern_opt->emergency_threshold;
        result.memory_trigger_threshold = pattern_opt->memory_trigger_threshold;
    } else {
        // Valeurs par défaut
        result.alpha = 0.3;
        result.beta = 0.2;
        result.gamma = 0.15;
        result.delta = 0.1;
        result.theta = 0.25;
        result.emergency_threshold = 0.75;
        result.memory_trigger_threshold = 0.5;
    }
    
    // Transition ?
    if (!current_pattern_id_.empty() && current_pattern_id_ != match.pattern_id) {
        result.is_transition = true;
        result.previous_pattern_id = current_pattern_id_;
        result.transition_probability = getTransitionProbability(match.pattern_id);
    }
    
    // Callback
    if (match_callback_) {
        match_callback_(result);
    }
    
    return result;
}

MatchResult PatternMatcher::createResultForNewPattern(const EmotionalSignature& signature) {
    // Crée un nouveau pattern
    std::string new_id = mlt_->createPattern(signature);
    patterns_created_++;
    
    auto pattern_opt = mlt_->getPattern(new_id);
    
    MatchResult result;
    result.pattern_id = new_id;
    result.pattern_name = pattern_opt ? pattern_opt->name : "NEW_PATTERN";
    result.similarity = 1.0;  // Match parfait avec lui-même
    result.confidence = config_.min_confidence_for_creation;
    result.is_new_pattern = true;
    
    if (pattern_opt) {
        result.alpha = pattern_opt->alpha;
        result.beta = pattern_opt->beta;
        result.gamma = pattern_opt->gamma;
        result.delta = pattern_opt->delta;
        result.theta = pattern_opt->theta;
        result.emergency_threshold = pattern_opt->emergency_threshold;
        result.memory_trigger_threshold = pattern_opt->memory_trigger_threshold;
    } else {
        result.alpha = 0.3;
        result.beta = 0.2;
        result.gamma = 0.15;
        result.delta = 0.1;
        result.theta = 0.25;
        result.emergency_threshold = 0.75;
        result.memory_trigger_threshold = 0.5;
    }
    
    // Callbacks
    if (new_pattern_callback_) {
        new_pattern_callback_(new_id, result.pattern_name);
    }
    
    if (match_callback_) {
        match_callback_(result);
    }
    
    // Met à jour l'état
    updateHistory(new_id);
    current_pattern_id_ = new_id;
    current_match_similarity_ = 1.0;
    frames_in_current_pattern_ = 1;
    
    return result;
}

bool PatternMatcher::shouldSwitchPattern(const PatternMatch& new_match) const {
    // Si pas de pattern actuel, toujours switcher
    if (current_pattern_id_.empty()) {
        return true;
    }
    
    // Si même pattern, pas de switch
    if (new_match.pattern_id == current_pattern_id_) {
        return false;
    }
    
    // Hysteresis : le nouveau pattern doit être significativement meilleur
    double threshold = current_match_similarity_ + config_.hysteresis_margin;
    
    // ET on doit avoir été dans le pattern actuel assez longtemps
    bool enough_frames = frames_in_current_pattern_ >= config_.min_frames_before_switch;
    
    return (new_match.similarity > threshold) && enough_frames;
}

void PatternMatcher::updateHistory(const std::string& pattern_id) {
    pattern_history_.emplace_back(pattern_id, std::chrono::steady_clock::now());
    
    while (pattern_history_.size() > max_history_size_) {
        pattern_history_.pop_front();
    }
}

void PatternMatcher::analyzeUnmatchedSignatures() {
    // Nettoie les signatures anciennes
    if (unmatched_signatures_.size() > 100) {
        unmatched_signatures_.erase(unmatched_signatures_.begin(),
                                     unmatched_signatures_.begin() + 50);
    }
}

} // namespace mcee
