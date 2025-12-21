/**
 * @file PatternMatcher.hpp
 * @brief Moteur de correspondance entre MCT et MLT
 * 
 * Le PatternMatcher remplace le PhaseDetector fixe. Il compare l'état
 * actuel de la MCT avec les patterns stockés en MLT pour identifier
 * le pattern actif et décider de créer/modifier des patterns.
 * 
 * @version 3.0
 * @date 2024
 */

#pragma once

#include "MCT.hpp"
#include "MLT.hpp"
#include "Types.hpp"
#include <nlohmann/json.hpp>
#include <memory>
#include <functional>
#include <optional>
#include <vector>

namespace mcee {

/**
 * @brief Résultat du matching pattern
 */
struct MatchResult {
    // Pattern identifié
    std::string pattern_id;
    std::string pattern_name;
    double similarity;
    double confidence;
    
    // Coefficients à utiliser
    double alpha;
    double beta;
    double gamma;
    double delta;
    double theta;
    double emergency_threshold;
    double memory_trigger_threshold;
    
    // Métadonnées
    bool is_new_pattern{false};           // Pattern créé pour cette situation
    bool is_transition{false};            // Transition depuis un autre pattern
    std::string previous_pattern_id;       // Pattern précédent (si transition)
    double transition_probability{0.0};   // Probabilité de cette transition
    
    // Alternatives
    std::vector<PatternMatch> alternatives;  // Autres patterns possibles
};

/**
 * @brief Décision du matcher
 */
enum class MatchDecision {
    USE_EXISTING,      // Utiliser un pattern existant
    CREATE_NEW,        // Créer un nouveau pattern
    MODIFY_EXISTING,   // Modifier un pattern existant
    MERGE_PATTERNS,    // Fusionner des patterns
    UNCERTAIN          // Pas assez de données
};

/**
 * @brief Configuration du PatternMatcher
 */
struct PatternMatcherConfig {
    // Seuils de décision
    double high_match_threshold{0.85};     // Au-dessus = utiliser tel quel
    double medium_match_threshold{0.6};    // Entre medium et high = modifier
    double low_match_threshold{0.4};       // En dessous = créer nouveau
    
    // Hysteresis (pour éviter oscillations)
    double hysteresis_margin{0.1};         // Marge avant changement de pattern
    int min_frames_before_switch{3};       // Frames min avant switch
    
    // Création de patterns
    double min_stability_for_creation{0.5}; // Stabilité MCT min pour créer
    double min_confidence_for_creation{0.3}; // Confiance min du nouveau pattern
    
    // Matching
    size_t max_matches_returned{5};         // Nombre max de matches retournés
    
    // Logging
    bool verbose_logging{false};
};

/**
 * @class PatternMatcher
 * @brief Compare MCT avec MLT pour identifier/créer patterns
 * 
 * Le PatternMatcher est le cerveau de l'identification des états émotionnels.
 * Il analyse la MCT, compare avec la MLT, et décide :
 * - Quel pattern utiliser
 * - Si créer un nouveau pattern
 * - Si modifier/fusionner des patterns existants
 */
class PatternMatcher {
public:
    PatternMatcher();
    PatternMatcher(std::shared_ptr<MCT> mct, 
                   std::shared_ptr<MLT> mlt,
                   const PatternMatcherConfig& config = {});
    
    // ═══════════════════════════════════════════════════════════════
    // MATCHING PRINCIPAL
    // ═══════════════════════════════════════════════════════════════
    
    /**
     * @brief Effectue le matching et retourne le pattern à utiliser
     * 
     * Workflow:
     * 1. Extrait la signature de la MCT
     * 2. Compare avec les patterns MLT
     * 3. Décide de la stratégie (utiliser/créer/modifier)
     * 4. Retourne le résultat avec les coefficients
     * 
     * @return Résultat du matching avec pattern et coefficients
     */
    MatchResult match();
    
    /**
     * @brief Matching avec une signature spécifique (sans utiliser MCT)
     */
    MatchResult matchSignature(const EmotionalSignature& signature);
    
    /**
     * @brief Obtient la décision sans l'appliquer
     */
    MatchDecision getDecision(const EmotionalSignature& signature) const;
    
    // ═══════════════════════════════════════════════════════════════
    // GESTION DES TRANSITIONS
    // ═══════════════════════════════════════════════════════════════
    
    /**
     * @brief Notifie le matcher d'un changement de pattern
     * @param new_pattern_id ID du nouveau pattern actif
     */
    void notifyPatternChange(const std::string& new_pattern_id);
    
    /**
     * @brief Obtient le pattern actuellement actif
     */
    std::optional<std::string> getCurrentPatternId() const;
    
    /**
     * @brief Obtient l'historique récent des patterns
     */
    std::vector<std::pair<std::string, double>> getPatternHistory(size_t n = 10) const;
    
    /**
     * @brief Calcule la probabilité de transition vers un pattern
     */
    double getTransitionProbability(const std::string& to_pattern_id) const;
    
    // ═══════════════════════════════════════════════════════════════
    // FEEDBACK ET APPRENTISSAGE
    // ═══════════════════════════════════════════════════════════════
    
    /**
     * @brief Envoie un feedback sur le matching actuel
     * @param feedback Score [-1, 1] (négatif = mauvais match, positif = bon match)
     */
    void provideFeedback(double feedback);
    
    /**
     * @brief Confirme que le pattern actuel est correct
     */
    void confirmCurrentPattern();
    
    /**
     * @brief Indique que le pattern actuel est incorrect
     * @param correct_pattern_id ID du pattern qui aurait dû être sélectionné (optionnel)
     */
    void rejectCurrentPattern(const std::string& correct_pattern_id = "");
    
    // ═══════════════════════════════════════════════════════════════
    // CRÉATION DE PATTERNS
    // ═══════════════════════════════════════════════════════════════
    
    /**
     * @brief Force la création d'un nouveau pattern à partir de la MCT
     * @param name Nom du pattern
     * @param description Description
     * @return ID du pattern créé
     */
    std::string forceCreatePattern(const std::string& name = "",
                                   const std::string& description = "");
    
    /**
     * @brief Suggère des patterns à créer basé sur l'historique
     */
    std::vector<EmotionalSignature> suggestNewPatterns() const;
    
    // ═══════════════════════════════════════════════════════════════
    // ACCESSEURS
    // ═══════════════════════════════════════════════════════════════
    
    void setMCT(std::shared_ptr<MCT> mct) { mct_ = mct; }
    void setMLT(std::shared_ptr<MLT> mlt) { mlt_ = mlt; }
    
    const PatternMatcherConfig& getConfig() const { return config_; }
    void setConfig(const PatternMatcherConfig& config) { config_ = config; }
    
    // Statistiques
    size_t getTotalMatches() const { return total_matches_; }
    size_t getPatternsCreated() const { return patterns_created_; }
    size_t getTransitionsRecorded() const { return transitions_recorded_; }
    double getAverageMatchSimilarity() const;
    
    // ═══════════════════════════════════════════════════════════════
    // CALLBACKS
    // ═══════════════════════════════════════════════════════════════
    
    using MatchCallback = std::function<void(const MatchResult&)>;
    using TransitionCallback = std::function<void(const std::string& from, 
                                                   const std::string& to,
                                                   double probability)>;
    using NewPatternCallback = std::function<void(const std::string& id,
                                                   const std::string& name)>;
    
    void setMatchCallback(MatchCallback cb) { match_callback_ = std::move(cb); }
    void setTransitionCallback(TransitionCallback cb) { transition_callback_ = std::move(cb); }
    void setNewPatternCallback(NewPatternCallback cb) { new_pattern_callback_ = std::move(cb); }
    
    // ═══════════════════════════════════════════════════════════════
    // SÉRIALISATION
    // ═══════════════════════════════════════════════════════════════
    
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
    
private:
    std::shared_ptr<MCT> mct_;
    std::shared_ptr<MLT> mlt_;
    PatternMatcherConfig config_;
    
    // État actuel
    std::string current_pattern_id_;
    int frames_in_current_pattern_{0};
    double current_match_similarity_{0.0};
    
    // Historique
    std::deque<std::pair<std::string, std::chrono::steady_clock::time_point>> pattern_history_;
    size_t max_history_size_{100};
    
    // Statistiques
    size_t total_matches_{0};
    size_t patterns_created_{0};
    size_t transitions_recorded_{0};
    double sum_similarities_{0.0};
    
    // Signatures non matchées (candidates pour nouveau pattern)
    std::vector<EmotionalSignature> unmatched_signatures_;
    
    // Callbacks
    MatchCallback match_callback_;
    TransitionCallback transition_callback_;
    NewPatternCallback new_pattern_callback_;
    
    // Méthodes internes
    MatchResult createResultFromPattern(const PatternMatch& match,
                                        const std::vector<PatternMatch>& alternatives);
    MatchResult createResultForNewPattern(const EmotionalSignature& signature);
    bool shouldSwitchPattern(const PatternMatch& new_match) const;
    void updateHistory(const std::string& pattern_id);
    void analyzeUnmatchedSignatures();
};

} // namespace mcee
