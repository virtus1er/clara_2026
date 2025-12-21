/**
 * @file MLT.hpp
 * @brief Mémoire Long Terme (MLT) - Patterns émotionnels dynamiques
 * 
 * La MLT stocke les patterns émotionnels appris qui remplacent les phases fixes.
 * Les patterns peuvent être créés, modifiés, fusionnés dynamiquement.
 * 
 * @version 3.0
 * @date 2024
 */

#pragma once

#include "Types.hpp"
#include "MCT.hpp"
#include <nlohmann/json.hpp>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>
#include <optional>
#include <functional>
#include <chrono>

namespace mcee {

/**
 * @brief Pattern émotionnel dynamique (remplace les phases fixes)
 */
struct EmotionalPattern {
    // Identification
    std::string id;                            // UUID unique
    std::string name;                          // Nom lisible (ex: "PEUR", "JOIE_INTENSE")
    std::string description;                   // Description du pattern
    
    // Signature émotionnelle
    EmotionalSignature signature;              // Signature pour matching
    
    // Coefficients dynamiques (appris)
    double alpha{0.3};                         // Poids émotions dominantes
    double beta{0.2};                          // Poids mémoire
    double gamma{0.15};                        // Poids feedback externe
    double delta{0.1};                         // Poids environnement
    double theta{0.25};                        // Poids état précédent
    
    // Seuils d'urgence dynamiques
    double emergency_threshold{0.8};           // Seuil Amyghaleon
    double memory_trigger_threshold{0.5};      // Seuil création souvenir
    
    // Métadonnées d'apprentissage
    int activation_count{0};                   // Nombre d'activations
    double confidence{0.5};                    // Confiance dans ce pattern [0, 1]
    double average_duration_seconds{0.0};      // Durée moyenne d'activation
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_activated;
    std::chrono::system_clock::time_point last_modified;
    
    // Relations avec autres patterns
    std::vector<std::string> parent_ids;       // Patterns dont il dérive
    std::vector<std::string> child_ids;        // Patterns dérivés
    std::unordered_map<std::string, double> transition_probabilities;  // Probabilités de transition
    
    // Contextes associés
    std::vector<std::string> associated_contexts;  // Mots-clés/contextes associés
    std::vector<std::string> trigger_words;        // Mots déclencheurs
    
    // État
    bool is_base_pattern{false};               // Pattern de base (non supprimable)
    bool is_active{true};                      // Pattern actif
    bool is_locked{false};                     // Pattern verrouillé (non modifiable)
    
    EmotionalPattern() {
        created_at = std::chrono::system_clock::now();
        last_modified = created_at;
        last_activated = created_at;
    }
};

/**
 * @brief Résultat de matching de pattern
 */
struct PatternMatch {
    std::string pattern_id;
    std::string pattern_name;
    double similarity;                         // Similarité [0, 1]
    double confidence;                         // Confiance pondérée
    const EmotionalPattern* pattern{nullptr};  // Pointeur vers le pattern
    
    bool operator<(const PatternMatch& other) const {
        return (similarity * confidence) > (other.similarity * other.confidence);
    }
};

/**
 * @brief Configuration de la MLT
 */
struct MLTConfig {
    // Matching
    double min_similarity_threshold{0.6};      // Similarité min pour match
    double high_similarity_threshold{0.85};    // Similarité haute (pas de création)
    size_t max_matches_returned{5};            // Nombre max de matches retournés
    
    // Création de patterns
    double min_confidence_for_creation{0.3};   // Confiance min pour créer
    size_t min_activations_for_learning{3};    // Activations min avant apprentissage
    double learning_rate{0.1};                 // Taux d'apprentissage des coefficients
    
    // Fusion de patterns
    double fusion_similarity_threshold{0.9};   // Similarité pour fusion automatique
    size_t min_activations_for_fusion{10};     // Activations min avant fusion
    
    // Nettoyage
    size_t max_patterns{100};                  // Nombre max de patterns
    double min_confidence_to_keep{0.1};        // Confiance min pour garder
    int days_before_pruning{30};               // Jours d'inactivité avant suppression
    
    // Persistance
    std::string storage_path{"patterns.json"}; // Chemin de sauvegarde
    bool auto_save{true};                      // Sauvegarde automatique
    int auto_save_interval_minutes{5};         // Intervalle de sauvegarde
};

/**
 * @brief Événement de pattern (pour callbacks)
 */
struct PatternEvent {
    enum class Type {
        CREATED,
        MODIFIED,
        MERGED,
        DELETED,
        ACTIVATED,
        DEACTIVATED
    };
    
    Type type;
    std::string pattern_id;
    std::string pattern_name;
    std::string details;
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @class MLT
 * @brief Mémoire Long Terme - Stockage et gestion des patterns émotionnels
 * 
 * La MLT est le cœur du système de patterns dynamiques. Elle permet de :
 * - Stocker des patterns émotionnels avec leurs coefficients
 * - Matcher une signature MCT avec les patterns existants
 * - Créer de nouveaux patterns quand nécessaire
 * - Fusionner des patterns similaires
 * - Faire évoluer les coefficients par apprentissage
 */
class MLT {
public:
    MLT();
    explicit MLT(const MLTConfig& config);
    
    // ═══════════════════════════════════════════════════════════════
    // INITIALISATION
    // ═══════════════════════════════════════════════════════════════
    
    /**
     * @brief Initialise avec les patterns de base (équivalent des 8 phases)
     */
    void initializeBasePatterns();
    
    /**
     * @brief Charge les patterns depuis un fichier
     */
    bool loadFromFile(const std::string& path);
    
    /**
     * @brief Sauvegarde les patterns dans un fichier
     */
    bool saveToFile(const std::string& path) const;
    
    // ═══════════════════════════════════════════════════════════════
    // MATCHING
    // ═══════════════════════════════════════════════════════════════
    
    /**
     * @brief Trouve le pattern le plus similaire à une signature
     * @param signature Signature émotionnelle de la MCT
     * @return Meilleur match ou nullopt si aucun
     */
    std::optional<PatternMatch> findBestMatch(const EmotionalSignature& signature) const;
    
    /**
     * @brief Trouve les N meilleurs patterns
     * @param signature Signature à matcher
     * @param n Nombre de résultats
     * @return Liste triée par pertinence
     */
    std::vector<PatternMatch> findMatches(const EmotionalSignature& signature, size_t n = 5) const;
    
    /**
     * @brief Calcule la similarité entre une signature et un pattern
     */
    double computeSimilarity(const EmotionalSignature& signature, 
                            const EmotionalPattern& pattern) const;
    
    // ═══════════════════════════════════════════════════════════════
    // GESTION DES PATTERNS
    // ═══════════════════════════════════════════════════════════════
    
    /**
     * @brief Crée un nouveau pattern à partir d'une signature
     * @param signature Signature source
     * @param name Nom du pattern
     * @return ID du pattern créé
     */
    std::string createPattern(const EmotionalSignature& signature,
                              const std::string& name = "",
                              const std::string& description = "");
    
    /**
     * @brief Crée un pattern dérivé d'un parent
     */
    std::string createDerivedPattern(const std::string& parent_id,
                                     const EmotionalSignature& signature,
                                     const std::string& name = "");
    
    /**
     * @brief Met à jour un pattern existant avec une nouvelle observation
     * @param pattern_id ID du pattern
     * @param signature Nouvelle observation
     * @param feedback Feedback optionnel [-1, 1] pour ajuster les coefficients
     */
    void updatePattern(const std::string& pattern_id,
                       const EmotionalSignature& signature,
                       std::optional<double> feedback = std::nullopt);
    
    /**
     * @brief Fusionne deux patterns similaires
     * @return ID du nouveau pattern fusionné
     */
    std::string mergePatterns(const std::string& id1, const std::string& id2);
    
    /**
     * @brief Supprime un pattern (si non verrouillé)
     */
    bool deletePattern(const std::string& pattern_id);
    
    /**
     * @brief Active/désactive un pattern
     */
    void setPatternActive(const std::string& pattern_id, bool active);
    
    /**
     * @brief Verrouille/déverrouille un pattern
     */
    void setPatternLocked(const std::string& pattern_id, bool locked);
    
    // ═══════════════════════════════════════════════════════════════
    // APPRENTISSAGE
    // ═══════════════════════════════════════════════════════════════
    
    /**
     * @brief Enregistre une activation de pattern
     */
    void recordActivation(const std::string& pattern_id, double duration_seconds = 0.0);
    
    /**
     * @brief Enregistre une transition entre patterns
     */
    void recordTransition(const std::string& from_id, const std::string& to_id);
    
    /**
     * @brief Ajuste les coefficients d'un pattern basé sur feedback
     * @param pattern_id ID du pattern
     * @param feedback Feedback global [-1, 1]
     * @param emotion_feedback Feedback par émotion optionnel
     */
    void adjustCoefficients(const std::string& pattern_id,
                            double feedback,
                            const std::array<double, 24>* emotion_feedback = nullptr);
    
    /**
     * @brief Déclenche une passe d'apprentissage globale
     */
    void runLearningPass();
    
    // ═══════════════════════════════════════════════════════════════
    // MAINTENANCE
    // ═══════════════════════════════════════════════════════════════
    
    /**
     * @brief Fusionne automatiquement les patterns trop similaires
     */
    void autoMerge();
    
    /**
     * @brief Supprime les patterns obsolètes
     */
    void prune();
    
    /**
     * @brief Recalcule les statistiques de tous les patterns
     */
    void recalculateStatistics();
    
    // ═══════════════════════════════════════════════════════════════
    // ACCESSEURS
    // ═══════════════════════════════════════════════════════════════
    
    std::optional<EmotionalPattern> getPattern(const std::string& id) const;
    std::optional<EmotionalPattern> getPatternByName(const std::string& name) const;
    std::vector<EmotionalPattern> getAllPatterns() const;
    std::vector<EmotionalPattern> getActivePatterns() const;
    std::vector<EmotionalPattern> getBasePatterns() const;
    size_t patternCount() const;
    
    const MLTConfig& getConfig() const { return config_; }
    void setConfig(const MLTConfig& config) { config_ = config; }
    
    // ═══════════════════════════════════════════════════════════════
    // CALLBACKS
    // ═══════════════════════════════════════════════════════════════
    
    using PatternEventCallback = std::function<void(const PatternEvent&)>;
    void setEventCallback(PatternEventCallback cb) { event_callback_ = std::move(cb); }
    
    // ═══════════════════════════════════════════════════════════════
    // SÉRIALISATION
    // ═══════════════════════════════════════════════════════════════
    
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
    
private:
    MLTConfig config_;
    std::unordered_map<std::string, EmotionalPattern> patterns_;
    mutable std::mutex mutex_;
    
    PatternEventCallback event_callback_;
    
    // Générateur d'ID unique
    std::string generatePatternId() const;
    
    // Génération de nom automatique
    std::string generatePatternName(const EmotionalSignature& signature) const;
    
    // Calcul de signature moyenne entre deux patterns
    EmotionalSignature averageSignatures(const EmotionalSignature& s1,
                                         const EmotionalSignature& s2,
                                         double weight1 = 0.5) const;
    
    // Émet un événement
    void emitEvent(PatternEvent::Type type, 
                   const std::string& id,
                   const std::string& name,
                   const std::string& details = "");
    
    // Initialisation des 8 patterns de base
    EmotionalPattern createBasePattern(const std::string& name,
                                       const std::array<double, 24>& emotions,
                                       double alpha, double beta, double gamma,
                                       double delta, double theta,
                                       double emergency_threshold);
};

// ═══════════════════════════════════════════════════════════════════════════
// SIGNATURES DES 8 PATTERNS DE BASE
// ═══════════════════════════════════════════════════════════════════════════

namespace BasePatterns {

// Indices des émotions dominantes pour chaque pattern de base
// (utilisé pour l'initialisation)

constexpr std::array<const char*, 8> NAMES = {
    "SERENITE", "JOIE", "EXPLORATION", "ANXIETE",
    "PEUR", "TRISTESSE", "DEGOUT", "CONFUSION"
};

// Émotions dominantes par pattern (indices dans le tableau de 24 émotions)
// joy=0, sadness=1, fear=2, anger=3, surprise=4, disgust=5, trust=6, anticipation=7
// love=8, guilt=9, shame=10, pride=11, envy=12, gratitude=13, hope=14, despair=15
// boredom=16, curiosity=17, confusion=18, awe=19, contempt=20, embarrassment=21
// excitement=22, serenity=23

struct BasePatternDef {
    const char* name;
    std::array<double, 24> signature;
    double alpha, beta, gamma, delta, theta;
    double emergency_threshold;
};

} // namespace BasePatterns

} // namespace mcee
