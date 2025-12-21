/**
 * @file MLT.cpp
 * @brief Implémentation de la Mémoire Long Terme
 */

#include "MLT.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <fstream>
#include <sstream>
#include <random>
#include <iomanip>

namespace mcee {

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTEURS
// ═══════════════════════════════════════════════════════════════════════════

MLT::MLT() : config_() {
    initializeBasePatterns();
}

MLT::MLT(const MLTConfig& config) : config_(config) {
    initializeBasePatterns();
}

// ═══════════════════════════════════════════════════════════════════════════
// INITIALISATION
// ═══════════════════════════════════════════════════════════════════════════

void MLT::initializeBasePatterns() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Définition des 8 patterns de base
    // Indices: joy=0, sadness=1, fear=2, anger=3, surprise=4, disgust=5, trust=6, anticipation=7
    // love=8, guilt=9, shame=10, pride=11, envy=12, gratitude=13, hope=14, despair=15
    // boredom=16, curiosity=17, confusion=18, awe=19, contempt=20, embarrassment=21
    // excitement=22, serenity=23
    
    // SÉRÉNITÉ : calme, confiance, peu d'activation
    {
        std::array<double, 24> sig{};
        sig[23] = 0.8;  // serenity
        sig[6] = 0.6;   // trust
        sig[14] = 0.5;  // hope
        sig[0] = 0.4;   // joy
        auto pattern = createBasePattern("SERENITE", sig, 0.25, 0.25, 0.1, 0.15, 0.25, 0.9);
        pattern.description = "État de calme et de paix intérieure";
        pattern.trigger_words = {"calme", "serein", "paisible", "tranquille", "zen"};
        patterns_[pattern.id] = pattern;
    }
    
    // JOIE : bonheur, excitation positive
    {
        std::array<double, 24> sig{};
        sig[0] = 0.85;  // joy
        sig[22] = 0.7;  // excitement
        sig[14] = 0.6;  // hope
        sig[11] = 0.5;  // pride
        sig[13] = 0.5;  // gratitude
        auto pattern = createBasePattern("JOIE", sig, 0.35, 0.2, 0.15, 0.1, 0.2, 0.85);
        pattern.description = "État de bonheur et d'enthousiasme";
        pattern.trigger_words = {"content", "heureux", "joyeux", "ravi", "super"};
        patterns_[pattern.id] = pattern;
    }
    
    // EXPLORATION : curiosité, anticipation, ouverture
    {
        std::array<double, 24> sig{};
        sig[17] = 0.8;  // curiosity
        sig[7] = 0.7;   // anticipation
        sig[19] = 0.6;  // awe
        sig[4] = 0.5;   // surprise
        sig[22] = 0.4;  // excitement
        auto pattern = createBasePattern("EXPLORATION", sig, 0.3, 0.2, 0.2, 0.15, 0.15, 0.8);
        pattern.description = "État de curiosité et découverte";
        pattern.trigger_words = {"curieux", "intéressant", "découvrir", "explorer", "nouveau"};
        patterns_[pattern.id] = pattern;
    }
    
    // ANXIÉTÉ : inquiétude modérée, anticipation négative
    {
        std::array<double, 24> sig{};
        sig[7] = 0.6;   // anticipation (négative)
        sig[2] = 0.5;   // fear
        sig[9] = 0.4;   // guilt
        sig[18] = 0.4;  // confusion
        sig[1] = 0.3;   // sadness
        auto pattern = createBasePattern("ANXIETE", sig, 0.3, 0.25, 0.15, 0.15, 0.15, 0.7);
        pattern.description = "État d'inquiétude et de tension";
        pattern.trigger_words = {"inquiet", "anxieux", "stressé", "préoccupé", "nerveux"};
        patterns_[pattern.id] = pattern;
    }
    
    // PEUR : peur intense, mode urgence
    {
        std::array<double, 24> sig{};
        sig[2] = 0.9;   // fear
        sig[4] = 0.6;   // surprise
        sig[15] = 0.5;  // despair
        sig[3] = 0.4;   // anger (fight/flight)
        auto pattern = createBasePattern("PEUR", sig, 0.4, 0.15, 0.15, 0.1, 0.2, 0.5);
        pattern.description = "État de peur intense - mode urgence";
        pattern.trigger_words = {"peur", "terreur", "danger", "menace", "urgence"};
        patterns_[pattern.id] = pattern;
    }
    
    // TRISTESSE : mélancolie, désespoir
    {
        std::array<double, 24> sig{};
        sig[1] = 0.85;  // sadness
        sig[15] = 0.6;  // despair
        sig[9] = 0.4;   // guilt
        sig[10] = 0.3;  // shame
        sig[16] = 0.4;  // boredom
        auto pattern = createBasePattern("TRISTESSE", sig, 0.25, 0.3, 0.15, 0.1, 0.2, 0.75);
        pattern.description = "État de tristesse et mélancolie";
        pattern.trigger_words = {"triste", "malheureux", "déprimé", "abattu", "chagrin"};
        patterns_[pattern.id] = pattern;
    }
    
    // DÉGOÛT : rejet, aversion
    {
        std::array<double, 24> sig{};
        sig[5] = 0.85;  // disgust
        sig[20] = 0.6;  // contempt
        sig[3] = 0.4;   // anger
        auto pattern = createBasePattern("DEGOUT", sig, 0.35, 0.2, 0.2, 0.1, 0.15, 0.7);
        pattern.description = "État de dégoût et rejet";
        pattern.trigger_words = {"dégoûté", "répugnant", "horrible", "déteste", "écœurant"};
        patterns_[pattern.id] = pattern;
    }
    
    // CONFUSION : incertitude, désorientation
    {
        std::array<double, 24> sig{};
        sig[18] = 0.8;  // confusion
        sig[4] = 0.5;   // surprise
        sig[17] = 0.4;  // curiosity
        sig[2] = 0.3;   // fear (légère)
        auto pattern = createBasePattern("CONFUSION", sig, 0.25, 0.25, 0.2, 0.15, 0.15, 0.75);
        pattern.description = "État de confusion et désorientation";
        pattern.trigger_words = {"confus", "perdu", "désorienté", "incompris", "bizarre"};
        patterns_[pattern.id] = pattern;
    }
    
    emitEvent(PatternEvent::Type::CREATED, "", "BASE_PATTERNS", 
              "8 patterns de base initialisés");
}

EmotionalPattern MLT::createBasePattern(const std::string& name,
                                         const std::array<double, 24>& emotions,
                                         double alpha, double beta, double gamma,
                                         double delta, double theta,
                                         double emergency_threshold) {
    EmotionalPattern pattern;
    pattern.id = "BASE_" + name;
    pattern.name = name;
    pattern.is_base_pattern = true;
    pattern.is_active = true;
    pattern.confidence = 1.0;  // Confiance max pour patterns de base
    
    // Coefficients
    pattern.alpha = alpha;
    pattern.beta = beta;
    pattern.gamma = gamma;
    pattern.delta = delta;
    pattern.theta = theta;
    pattern.emergency_threshold = emergency_threshold;
    pattern.memory_trigger_threshold = 0.5;
    
    // Signature
    pattern.signature.mean_emotions = emotions;
    pattern.signature.std_dev.fill(0.1);  // Tolérance modérée
    pattern.signature.trend.fill(0.0);
    
    // Calcul des métriques globales
    pattern.signature.global_intensity = std::accumulate(
        emotions.begin(), emotions.end(), 0.0) / 24.0;
    
    // Valence simplifiée
    pattern.signature.global_valence = (emotions[0] + emotions[8] + emotions[14]) -
                                        (emotions[1] + emotions[2] + emotions[15]);
    pattern.signature.global_valence = std::clamp(pattern.signature.global_valence / 3.0, -1.0, 1.0);
    
    // Arousal
    pattern.signature.global_arousal = (emotions[22] + emotions[4] + emotions[2]) -
                                        (emotions[23] + emotions[16]);
    pattern.signature.global_arousal = std::clamp((pattern.signature.global_arousal + 2.0) / 4.0, 0.0, 1.0);
    
    pattern.signature.stability = 1.0;
    
    return pattern;
}

bool MLT::loadFromFile(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            return false;
        }
        
        nlohmann::json j;
        file >> j;
        fromJson(j);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool MLT::saveToFile(const std::string& path) const {
    try {
        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }
        
        file << std::setw(2) << toJson() << std::endl;
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// MATCHING
// ═══════════════════════════════════════════════════════════════════════════

std::optional<PatternMatch> MLT::findBestMatch(const EmotionalSignature& signature) const {
    auto matches = findMatches(signature, 1);
    if (matches.empty()) {
        return std::nullopt;
    }
    return matches[0];
}

std::vector<PatternMatch> MLT::findMatches(const EmotionalSignature& signature, size_t n) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<PatternMatch> matches;
    
    for (const auto& [id, pattern] : patterns_) {
        if (!pattern.is_active) continue;
        
        double similarity = computeSimilarity(signature, pattern);
        
        if (similarity >= config_.min_similarity_threshold) {
            PatternMatch match;
            match.pattern_id = id;
            match.pattern_name = pattern.name;
            match.similarity = similarity;
            match.confidence = pattern.confidence;
            match.pattern = &pattern;
            matches.push_back(match);
        }
    }
    
    // Tri par score combiné (similarité * confiance)
    std::sort(matches.begin(), matches.end());
    
    // Limite au nombre demandé
    if (matches.size() > n) {
        matches.resize(n);
    }
    
    return matches;
}

double MLT::computeSimilarity(const EmotionalSignature& signature, 
                              const EmotionalPattern& pattern) const {
    // Similarité cosinus sur les émotions moyennes
    double dot = 0.0, norm1 = 0.0, norm2 = 0.0;
    
    for (size_t i = 0; i < 24; ++i) {
        dot += signature.mean_emotions[i] * pattern.signature.mean_emotions[i];
        norm1 += signature.mean_emotions[i] * signature.mean_emotions[i];
        norm2 += pattern.signature.mean_emotions[i] * pattern.signature.mean_emotions[i];
    }
    
    if (norm1 < 1e-10 || norm2 < 1e-10) {
        return 0.0;
    }
    
    double cosine_sim = dot / (std::sqrt(norm1) * std::sqrt(norm2));
    
    // Bonus pour correspondance de valence et arousal
    double valence_diff = std::abs(signature.global_valence - pattern.signature.global_valence);
    double arousal_diff = std::abs(signature.global_arousal - pattern.signature.global_arousal);
    
    double valence_bonus = (1.0 - valence_diff) * 0.1;
    double arousal_bonus = (1.0 - arousal_diff) * 0.1;
    
    return std::clamp(cosine_sim + valence_bonus + arousal_bonus, 0.0, 1.0);
}

// ═══════════════════════════════════════════════════════════════════════════
// GESTION DES PATTERNS
// ═══════════════════════════════════════════════════════════════════════════

std::string MLT::createPattern(const EmotionalSignature& signature,
                               const std::string& name,
                               const std::string& description) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    EmotionalPattern pattern;
    pattern.id = generatePatternId();
    pattern.name = name.empty() ? generatePatternName(signature) : name;
    pattern.description = description;
    pattern.signature = signature;
    pattern.is_base_pattern = false;
    pattern.is_active = true;
    pattern.confidence = config_.min_confidence_for_creation;
    
    // Coefficients par défaut (moyenne des patterns de base similaires)
    pattern.alpha = 0.3;
    pattern.beta = 0.2;
    pattern.gamma = 0.15;
    pattern.delta = 0.1;
    pattern.theta = 0.25;
    pattern.emergency_threshold = 0.75;
    pattern.memory_trigger_threshold = 0.5;
    
    patterns_[pattern.id] = pattern;
    
    emitEvent(PatternEvent::Type::CREATED, pattern.id, pattern.name, 
              "Nouveau pattern créé");
    
    return pattern.id;
}

std::string MLT::createDerivedPattern(const std::string& parent_id,
                                       const EmotionalSignature& signature,
                                       const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto parent_it = patterns_.find(parent_id);
    if (parent_it == patterns_.end()) {
        return "";
    }
    
    const auto& parent = parent_it->second;
    
    EmotionalPattern pattern;
    pattern.id = generatePatternId();
    pattern.name = name.empty() ? parent.name + "_derived" : name;
    pattern.description = "Dérivé de " + parent.name;
    pattern.signature = signature;
    pattern.is_base_pattern = false;
    pattern.is_active = true;
    pattern.confidence = config_.min_confidence_for_creation;
    
    // Hérite des coefficients du parent
    pattern.alpha = parent.alpha;
    pattern.beta = parent.beta;
    pattern.gamma = parent.gamma;
    pattern.delta = parent.delta;
    pattern.theta = parent.theta;
    pattern.emergency_threshold = parent.emergency_threshold;
    pattern.memory_trigger_threshold = parent.memory_trigger_threshold;
    
    // Relations
    pattern.parent_ids.push_back(parent_id);
    
    patterns_[pattern.id] = pattern;
    
    // Met à jour le parent
    patterns_[parent_id].child_ids.push_back(pattern.id);
    
    emitEvent(PatternEvent::Type::CREATED, pattern.id, pattern.name,
              "Pattern dérivé de " + parent.name);
    
    return pattern.id;
}

void MLT::updatePattern(const std::string& pattern_id,
                        const EmotionalSignature& signature,
                        std::optional<double> feedback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = patterns_.find(pattern_id);
    if (it == patterns_.end() || it->second.is_locked) {
        return;
    }
    
    auto& pattern = it->second;
    
    // Mise à jour de la signature (moyenne mobile)
    double learning_rate = config_.learning_rate;
    
    for (size_t i = 0; i < 24; ++i) {
        pattern.signature.mean_emotions[i] = 
            (1.0 - learning_rate) * pattern.signature.mean_emotions[i] +
            learning_rate * signature.mean_emotions[i];
        
        pattern.signature.std_dev[i] =
            (1.0 - learning_rate) * pattern.signature.std_dev[i] +
            learning_rate * signature.std_dev[i];
    }
    
    // Mise à jour des métriques globales
    pattern.signature.global_intensity =
        (1.0 - learning_rate) * pattern.signature.global_intensity +
        learning_rate * signature.global_intensity;
    
    pattern.signature.global_valence =
        (1.0 - learning_rate) * pattern.signature.global_valence +
        learning_rate * signature.global_valence;
    
    pattern.signature.global_arousal =
        (1.0 - learning_rate) * pattern.signature.global_arousal +
        learning_rate * signature.global_arousal;
    
    // Ajustement de confiance si feedback fourni
    if (feedback) {
        double fb = *feedback;
        pattern.confidence = std::clamp(
            pattern.confidence + fb * 0.1, 0.0, 1.0);
    }
    
    pattern.last_modified = std::chrono::system_clock::now();
    
    emitEvent(PatternEvent::Type::MODIFIED, pattern_id, pattern.name,
              "Pattern mis à jour");
}

std::string MLT::mergePatterns(const std::string& id1, const std::string& id2) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it1 = patterns_.find(id1);
    auto it2 = patterns_.find(id2);
    
    if (it1 == patterns_.end() || it2 == patterns_.end()) {
        return "";
    }
    
    if (it1->second.is_base_pattern || it2->second.is_base_pattern) {
        // Ne pas fusionner les patterns de base
        return "";
    }
    
    const auto& p1 = it1->second;
    const auto& p2 = it2->second;
    
    // Poids basé sur le nombre d'activations
    double weight1 = p1.activation_count / 
                     (p1.activation_count + p2.activation_count + 1.0);
    
    EmotionalPattern merged;
    merged.id = generatePatternId();
    merged.name = p1.name + "+" + p2.name;
    merged.description = "Fusion de " + p1.name + " et " + p2.name;
    merged.signature = averageSignatures(p1.signature, p2.signature, weight1);
    merged.is_base_pattern = false;
    merged.is_active = true;
    merged.confidence = (p1.confidence + p2.confidence) / 2.0;
    merged.activation_count = p1.activation_count + p2.activation_count;
    
    // Coefficients moyennés
    merged.alpha = weight1 * p1.alpha + (1.0 - weight1) * p2.alpha;
    merged.beta = weight1 * p1.beta + (1.0 - weight1) * p2.beta;
    merged.gamma = weight1 * p1.gamma + (1.0 - weight1) * p2.gamma;
    merged.delta = weight1 * p1.delta + (1.0 - weight1) * p2.delta;
    merged.theta = weight1 * p1.theta + (1.0 - weight1) * p2.theta;
    merged.emergency_threshold = std::min(p1.emergency_threshold, p2.emergency_threshold);
    
    // Relations
    merged.parent_ids.push_back(id1);
    merged.parent_ids.push_back(id2);
    
    // Contextes fusionnés
    merged.associated_contexts = p1.associated_contexts;
    merged.associated_contexts.insert(merged.associated_contexts.end(),
                                       p2.associated_contexts.begin(),
                                       p2.associated_contexts.end());
    
    patterns_[merged.id] = merged;
    
    // Désactive les patterns source
    patterns_[id1].is_active = false;
    patterns_[id2].is_active = false;
    
    emitEvent(PatternEvent::Type::MERGED, merged.id, merged.name,
              "Fusion de " + id1 + " et " + id2);
    
    return merged.id;
}

bool MLT::deletePattern(const std::string& pattern_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = patterns_.find(pattern_id);
    if (it == patterns_.end()) {
        return false;
    }
    
    if (it->second.is_base_pattern || it->second.is_locked) {
        return false;
    }
    
    std::string name = it->second.name;
    patterns_.erase(it);
    
    emitEvent(PatternEvent::Type::DELETED, pattern_id, name,
              "Pattern supprimé");
    
    return true;
}

void MLT::setPatternActive(const std::string& pattern_id, bool active) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = patterns_.find(pattern_id);
    if (it != patterns_.end()) {
        it->second.is_active = active;
        emitEvent(active ? PatternEvent::Type::ACTIVATED : PatternEvent::Type::DEACTIVATED,
                  pattern_id, it->second.name, "");
    }
}

void MLT::setPatternLocked(const std::string& pattern_id, bool locked) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = patterns_.find(pattern_id);
    if (it != patterns_.end()) {
        it->second.is_locked = locked;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// APPRENTISSAGE
// ═══════════════════════════════════════════════════════════════════════════

void MLT::recordActivation(const std::string& pattern_id, double duration_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = patterns_.find(pattern_id);
    if (it == patterns_.end()) return;
    
    auto& pattern = it->second;
    pattern.activation_count++;
    pattern.last_activated = std::chrono::system_clock::now();
    
    // Mise à jour de la durée moyenne
    if (duration_seconds > 0.0) {
        double n = pattern.activation_count;
        pattern.average_duration_seconds = 
            ((n - 1) * pattern.average_duration_seconds + duration_seconds) / n;
    }
    
    // Augmente légèrement la confiance à chaque activation
    pattern.confidence = std::min(1.0, pattern.confidence + 0.01);
}

void MLT::recordTransition(const std::string& from_id, const std::string& to_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = patterns_.find(from_id);
    if (it == patterns_.end()) return;
    
    auto& pattern = it->second;
    pattern.transition_probabilities[to_id] += 1.0;
    
    // Normalise les probabilités
    double sum = 0.0;
    for (const auto& [id, count] : pattern.transition_probabilities) {
        sum += count;
    }
    if (sum > 0.0) {
        for (auto& [id, count] : pattern.transition_probabilities) {
            count /= sum;
        }
    }
}

void MLT::adjustCoefficients(const std::string& pattern_id,
                              double feedback,
                              const std::array<double, 24>* emotion_feedback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = patterns_.find(pattern_id);
    if (it == patterns_.end() || it->second.is_locked) return;
    
    auto& pattern = it->second;
    double lr = config_.learning_rate * feedback;
    
    // Ajustement global des coefficients
    // Si feedback positif : renforcer les coefficients actuels
    // Si feedback négatif : rapprocher de la moyenne
    const double avg_coef = 0.2;
    
    pattern.alpha = std::clamp(pattern.alpha + lr * (pattern.alpha - avg_coef), 0.05, 0.5);
    pattern.beta = std::clamp(pattern.beta + lr * (pattern.beta - avg_coef), 0.05, 0.5);
    pattern.gamma = std::clamp(pattern.gamma + lr * (pattern.gamma - avg_coef), 0.05, 0.5);
    pattern.delta = std::clamp(pattern.delta + lr * (pattern.delta - avg_coef), 0.05, 0.5);
    pattern.theta = std::clamp(pattern.theta + lr * (pattern.theta - avg_coef), 0.05, 0.5);
    
    // Normalisation pour que la somme reste à 1
    double sum = pattern.alpha + pattern.beta + pattern.gamma + pattern.delta + pattern.theta;
    pattern.alpha /= sum;
    pattern.beta /= sum;
    pattern.gamma /= sum;
    pattern.delta /= sum;
    pattern.theta /= sum;
    
    pattern.last_modified = std::chrono::system_clock::now();
}

void MLT::runLearningPass() {
    autoMerge();
    prune();
    recalculateStatistics();
}

// ═══════════════════════════════════════════════════════════════════════════
// MAINTENANCE
// ═══════════════════════════════════════════════════════════════════════════

void MLT::autoMerge() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::pair<std::string, std::string>> to_merge;
    
    // Trouve les paires de patterns similaires
    for (auto it1 = patterns_.begin(); it1 != patterns_.end(); ++it1) {
        if (it1->second.is_base_pattern || !it1->second.is_active) continue;
        if (it1->second.activation_count < config_.min_activations_for_fusion) continue;
        
        for (auto it2 = std::next(it1); it2 != patterns_.end(); ++it2) {
            if (it2->second.is_base_pattern || !it2->second.is_active) continue;
            if (it2->second.activation_count < config_.min_activations_for_fusion) continue;
            
            double sim = computeSimilarity(it1->second.signature, it2->second);
            if (sim >= config_.fusion_similarity_threshold) {
                to_merge.emplace_back(it1->first, it2->first);
            }
        }
    }
    
    // Fusionne (sans le lock)
    mutex_.unlock();
    for (const auto& [id1, id2] : to_merge) {
        mergePatterns(id1, id2);
    }
    mutex_.lock();
}

void MLT::prune() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::system_clock::now();
    auto cutoff = now - std::chrono::hours(24 * config_.days_before_pruning);
    
    std::vector<std::string> to_remove;
    
    for (const auto& [id, pattern] : patterns_) {
        if (pattern.is_base_pattern || pattern.is_locked) continue;
        
        // Supprime si confiance trop basse ou inactif depuis longtemps
        if (pattern.confidence < config_.min_confidence_to_keep) {
            to_remove.push_back(id);
        } else if (pattern.last_activated < cutoff && pattern.activation_count < 5) {
            to_remove.push_back(id);
        }
    }
    
    // Vérifie qu'on ne dépasse pas le max
    if (patterns_.size() - to_remove.size() > config_.max_patterns) {
        // Trie par score (confiance * activations)
        std::vector<std::pair<std::string, double>> scores;
        for (const auto& [id, pattern] : patterns_) {
            if (pattern.is_base_pattern || pattern.is_locked) continue;
            if (std::find(to_remove.begin(), to_remove.end(), id) != to_remove.end()) continue;
            
            double score = pattern.confidence * std::log1p(pattern.activation_count);
            scores.emplace_back(id, score);
        }
        
        std::sort(scores.begin(), scores.end(),
                  [](const auto& a, const auto& b) { return a.second < b.second; });
        
        size_t to_remove_count = patterns_.size() - config_.max_patterns;
        for (size_t i = 0; i < to_remove_count && i < scores.size(); ++i) {
            to_remove.push_back(scores[i].first);
        }
    }
    
    for (const auto& id : to_remove) {
        patterns_.erase(id);
    }
}

void MLT::recalculateStatistics() {
    // Pour l'instant, rien de spécial
}

// ═══════════════════════════════════════════════════════════════════════════
// ACCESSEURS
// ═══════════════════════════════════════════════════════════════════════════

std::optional<EmotionalPattern> MLT::getPattern(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = patterns_.find(id);
    if (it == patterns_.end()) return std::nullopt;
    return it->second;
}

std::optional<EmotionalPattern> MLT::getPatternByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [id, pattern] : patterns_) {
        if (pattern.name == name) return pattern;
    }
    return std::nullopt;
}

std::vector<EmotionalPattern> MLT::getAllPatterns() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<EmotionalPattern> result;
    for (const auto& [id, pattern] : patterns_) {
        result.push_back(pattern);
    }
    return result;
}

std::vector<EmotionalPattern> MLT::getActivePatterns() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<EmotionalPattern> result;
    for (const auto& [id, pattern] : patterns_) {
        if (pattern.is_active) {
            result.push_back(pattern);
        }
    }
    return result;
}

std::vector<EmotionalPattern> MLT::getBasePatterns() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<EmotionalPattern> result;
    for (const auto& [id, pattern] : patterns_) {
        if (pattern.is_base_pattern) {
            result.push_back(pattern);
        }
    }
    return result;
}

size_t MLT::patternCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return patterns_.size();
}

// ═══════════════════════════════════════════════════════════════════════════
// SÉRIALISATION
// ═══════════════════════════════════════════════════════════════════════════

nlohmann::json MLT::toJson() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    nlohmann::json j;
    j["config"] = {
        {"min_similarity_threshold", config_.min_similarity_threshold},
        {"high_similarity_threshold", config_.high_similarity_threshold},
        {"max_matches_returned", config_.max_matches_returned},
        {"min_confidence_for_creation", config_.min_confidence_for_creation},
        {"min_activations_for_learning", config_.min_activations_for_learning},
        {"learning_rate", config_.learning_rate},
        {"fusion_similarity_threshold", config_.fusion_similarity_threshold},
        {"max_patterns", config_.max_patterns}
    };
    
    nlohmann::json patterns_json;
    for (const auto& [id, pattern] : patterns_) {
        nlohmann::json p;
        p["id"] = pattern.id;
        p["name"] = pattern.name;
        p["description"] = pattern.description;
        p["is_base_pattern"] = pattern.is_base_pattern;
        p["is_active"] = pattern.is_active;
        p["is_locked"] = pattern.is_locked;
        p["confidence"] = pattern.confidence;
        p["activation_count"] = pattern.activation_count;
        
        p["coefficients"] = {
            {"alpha", pattern.alpha},
            {"beta", pattern.beta},
            {"gamma", pattern.gamma},
            {"delta", pattern.delta},
            {"theta", pattern.theta}
        };
        
        p["thresholds"] = {
            {"emergency", pattern.emergency_threshold},
            {"memory_trigger", pattern.memory_trigger_threshold}
        };
        
        // Signature
        p["signature"]["mean_emotions"] = pattern.signature.mean_emotions;
        p["signature"]["global_intensity"] = pattern.signature.global_intensity;
        p["signature"]["global_valence"] = pattern.signature.global_valence;
        p["signature"]["global_arousal"] = pattern.signature.global_arousal;
        
        p["trigger_words"] = pattern.trigger_words;
        p["associated_contexts"] = pattern.associated_contexts;
        
        patterns_json[id] = p;
    }
    
    j["patterns"] = patterns_json;
    j["pattern_count"] = patterns_.size();
    
    return j;
}

void MLT::fromJson(const nlohmann::json& j) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (j.contains("config")) {
        const auto& c = j["config"];
        if (c.contains("min_similarity_threshold")) 
            config_.min_similarity_threshold = c["min_similarity_threshold"];
        if (c.contains("high_similarity_threshold")) 
            config_.high_similarity_threshold = c["high_similarity_threshold"];
        if (c.contains("learning_rate")) 
            config_.learning_rate = c["learning_rate"];
    }
    
    if (j.contains("patterns")) {
        for (const auto& [id, p] : j["patterns"].items()) {
            EmotionalPattern pattern;
            pattern.id = p.value("id", id);
            pattern.name = p.value("name", "");
            pattern.description = p.value("description", "");
            pattern.is_base_pattern = p.value("is_base_pattern", false);
            pattern.is_active = p.value("is_active", true);
            pattern.is_locked = p.value("is_locked", false);
            pattern.confidence = p.value("confidence", 0.5);
            pattern.activation_count = p.value("activation_count", 0);
            
            if (p.contains("coefficients")) {
                pattern.alpha = p["coefficients"].value("alpha", 0.3);
                pattern.beta = p["coefficients"].value("beta", 0.2);
                pattern.gamma = p["coefficients"].value("gamma", 0.15);
                pattern.delta = p["coefficients"].value("delta", 0.1);
                pattern.theta = p["coefficients"].value("theta", 0.25);
            }
            
            if (p.contains("thresholds")) {
                pattern.emergency_threshold = p["thresholds"].value("emergency", 0.75);
                pattern.memory_trigger_threshold = p["thresholds"].value("memory_trigger", 0.5);
            }
            
            if (p.contains("signature") && p["signature"].contains("mean_emotions")) {
                pattern.signature.mean_emotions = 
                    p["signature"]["mean_emotions"].get<std::array<double, 24>>();
                pattern.signature.global_intensity = 
                    p["signature"].value("global_intensity", 0.0);
                pattern.signature.global_valence = 
                    p["signature"].value("global_valence", 0.0);
                pattern.signature.global_arousal = 
                    p["signature"].value("global_arousal", 0.5);
            }
            
            if (p.contains("trigger_words")) {
                pattern.trigger_words = p["trigger_words"].get<std::vector<std::string>>();
            }
            
            patterns_[pattern.id] = pattern;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// MÉTHODES PRIVÉES
// ═══════════════════════════════════════════════════════════════════════════

std::string MLT::generatePatternId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "PAT_";
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

std::string MLT::generatePatternName(const EmotionalSignature& signature) const {
    // Génère un nom basé sur les émotions dominantes
    std::vector<std::pair<size_t, double>> sorted_emotions;
    for (size_t i = 0; i < 24; ++i) {
        sorted_emotions.emplace_back(i, signature.mean_emotions[i]);
    }
    std::sort(sorted_emotions.begin(), sorted_emotions.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    static const char* emotion_names[] = {
        "joy", "sadness", "fear", "anger", "surprise", "disgust", "trust", "anticipation",
        "love", "guilt", "shame", "pride", "envy", "gratitude", "hope", "despair",
        "boredom", "curiosity", "confusion", "awe", "contempt", "embarrassment",
        "excitement", "serenity"
    };
    
    std::string name = "CUSTOM_";
    if (sorted_emotions[0].second > 0.3) {
        name += emotion_names[sorted_emotions[0].first];
    }
    if (sorted_emotions[1].second > 0.2) {
        name += "_" + std::string(emotion_names[sorted_emotions[1].first]);
    }
    
    return name;
}

EmotionalSignature MLT::averageSignatures(const EmotionalSignature& s1,
                                          const EmotionalSignature& s2,
                                          double weight1) const {
    double weight2 = 1.0 - weight1;
    
    EmotionalSignature result;
    for (size_t i = 0; i < 24; ++i) {
        result.mean_emotions[i] = weight1 * s1.mean_emotions[i] + weight2 * s2.mean_emotions[i];
        result.std_dev[i] = weight1 * s1.std_dev[i] + weight2 * s2.std_dev[i];
        result.trend[i] = weight1 * s1.trend[i] + weight2 * s2.trend[i];
    }
    
    result.global_intensity = weight1 * s1.global_intensity + weight2 * s2.global_intensity;
    result.global_valence = weight1 * s1.global_valence + weight2 * s2.global_valence;
    result.global_arousal = weight1 * s1.global_arousal + weight2 * s2.global_arousal;
    result.stability = weight1 * s1.stability + weight2 * s2.stability;
    
    return result;
}

void MLT::emitEvent(PatternEvent::Type type,
                    const std::string& id,
                    const std::string& name,
                    const std::string& details) {
    if (event_callback_) {
        PatternEvent event;
        event.type = type;
        event.pattern_id = id;
        event.pattern_name = name;
        event.details = details;
        event.timestamp = std::chrono::system_clock::now();
        event_callback_(event);
    }
}

} // namespace mcee
