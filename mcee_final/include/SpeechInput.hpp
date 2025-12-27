/**
 * @file SpeechInput.hpp
 * @brief Gestionnaire des entrées textuelles du module de parole
 * @version 2.0
 * @date 2025-12-19
 * 
 * Reçoit les textes du module de parole et les analyse pour :
 * - Extraire le contexte émotionnel
 * - Générer le feedback externe (Fb_ext)
 * - Détecter des mots-clés d'urgence
 * - Fournir du contexte pour les souvenirs
 */

#ifndef MCEE_SPEECH_INPUT_HPP
#define MCEE_SPEECH_INPUT_HPP

#include "Types.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <chrono>
#include <queue>
#include <mutex>

namespace mcee {

/**
 * @brief Résultat de l'analyse d'un texte
 */
struct SpeechAnalysis {
    std::string raw_text;                          // Texte brut reçu
    std::string normalized_text;                   // Texte normalisé
    double sentiment_score = 0.0;                  // Score de sentiment [-1, 1]
    double arousal_score = 0.0;                    // Niveau d'activation [0, 1]
    double urgency_score = 0.0;                    // Score d'urgence [0, 1]
    std::vector<std::string> keywords;             // Mots-clés détectés
    std::vector<std::string> emotion_words;        // Mots émotionnels détectés
    bool contains_threat = false;                  // Contient une menace
    bool contains_positive = false;                // Contient du positif
    bool contains_question = false;                // Est une question
    std::chrono::steady_clock::time_point timestamp;
    
    SpeechAnalysis() : timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @brief Entrée textuelle avec métadonnées
 */
struct TextInput {
    std::string text;
    std::string source;                            // "user", "environment", "system"
    double confidence = 1.0;                       // Confiance de la transcription
    std::chrono::steady_clock::time_point timestamp;
    
    TextInput() : timestamp(std::chrono::steady_clock::now()) {}
};

/**
 * @class SpeechInput
 * @brief Gère les entrées textuelles et leur analyse émotionnelle
 */
class SpeechInput {
public:
    using TextCallback = std::function<void(const SpeechAnalysis&)>;
    using UrgencyCallback = std::function<void(const std::string&, double)>;

    /**
     * @brief Constructeur
     */
    SpeechInput();

    /**
     * @brief Traite un nouveau texte reçu
     * @param input Entrée textuelle
     * @return Analyse du texte
     */
    SpeechAnalysis processText(const TextInput& input);

    /**
     * @brief Traite un texte brut (version simplifiée)
     * @param text Texte brut
     * @param source Source du texte
     * @return Analyse du texte
     */
    SpeechAnalysis processText(const std::string& text, const std::string& source = "user");

    /**
     * @brief Calcule le feedback externe basé sur l'analyse
     * @param analysis Analyse du texte
     * @return Valeur de feedback externe [-1, 1]
     */
    [[nodiscard]] double computeFeedbackExternal(const SpeechAnalysis& analysis) const;

    /**
     * @brief Génère un contexte pour la création de souvenir
     * @param analysis Analyse du texte
     * @return Contexte textuel pour le souvenir
     */
    [[nodiscard]] std::string generateMemoryContext(const SpeechAnalysis& analysis) const;

    /**
     * @brief Retourne l'historique récent des analyses
     * @param count Nombre d'analyses à retourner
     * @return Vecteur des analyses récentes
     */
    [[nodiscard]] std::vector<SpeechAnalysis> getRecentAnalyses(size_t count = 10) const;

    /**
     * @brief Retourne la dernière analyse
     */
    [[nodiscard]] const SpeechAnalysis& getLastAnalysis() const { return last_analysis_; }

    /**
     * @brief Définit le callback pour les nouveaux textes
     */
    void setTextCallback(TextCallback callback);

    /**
     * @brief Définit le callback pour les urgences détectées
     */
    void setUrgencyCallback(UrgencyCallback callback);

    /**
     * @brief Ajoute des mots-clés personnalisés pour une catégorie
     * @param category Catégorie (threat, positive, emotion, etc.)
     * @param words Liste de mots
     */
    void addCustomKeywords(const std::string& category, const std::vector<std::string>& words);

    /**
     * @brief Charge un dictionnaire émotionnel depuis un fichier JSON
     * @param path Chemin vers le fichier
     * @return true si chargement réussi
     */
    bool loadEmotionalDictionary(const std::string& path);

    /**
     * @brief Retourne les statistiques
     */
    [[nodiscard]] size_t getProcessedCount() const { return processed_count_; }
    [[nodiscard]] double getAverageSentiment() const { return average_sentiment_; }

    /**
     * @brief Mode silencieux (moins de logs)
     */
    void setQuietMode(bool quiet) { quiet_mode_ = quiet; }
    [[nodiscard]] bool isQuietMode() const { return quiet_mode_; }

private:
    // Dictionnaires de mots
    std::unordered_set<std::string> threat_words_;
    std::unordered_set<std::string> positive_words_;
    std::unordered_set<std::string> negative_words_;
    std::unordered_set<std::string> high_arousal_words_;
    std::unordered_set<std::string> low_arousal_words_;
    std::unordered_map<std::string, double> emotion_word_scores_;

    // Historique
    std::vector<SpeechAnalysis> analysis_history_;
    SpeechAnalysis last_analysis_;
    mutable std::mutex history_mutex_;

    // Statistiques
    size_t processed_count_ = 0;
    double average_sentiment_ = 0.0;
    double sentiment_sum_ = 0.0;

    // Callbacks
    TextCallback on_text_;
    UrgencyCallback on_urgency_;

    // Mode silencieux
    bool quiet_mode_ = false;

    /**
     * @brief Initialise les dictionnaires par défaut
     */
    void initDefaultDictionaries();

    /**
     * @brief Normalise un texte (minuscules, suppression ponctuation)
     */
    [[nodiscard]] std::string normalizeText(const std::string& text) const;

    /**
     * @brief Tokenize un texte en mots
     */
    [[nodiscard]] std::vector<std::string> tokenize(const std::string& text) const;

    /**
     * @brief Analyse le sentiment du texte
     */
    [[nodiscard]] double analyzeSentiment(const std::vector<std::string>& tokens) const;

    /**
     * @brief Analyse le niveau d'activation (arousal)
     */
    [[nodiscard]] double analyzeArousal(const std::vector<std::string>& tokens) const;

    /**
     * @brief Détecte les menaces dans le texte
     */
    [[nodiscard]] bool detectThreats(const std::vector<std::string>& tokens) const;

    /**
     * @brief Extrait les mots-clés significatifs
     */
    [[nodiscard]] std::vector<std::string> extractKeywords(const std::vector<std::string>& tokens) const;

    /**
     * @brief Extrait les mots émotionnels
     */
    [[nodiscard]] std::vector<std::string> extractEmotionWords(const std::vector<std::string>& tokens) const;

    /**
     * @brief Calcule le score d'urgence
     */
    [[nodiscard]] double computeUrgencyScore(const SpeechAnalysis& analysis) const;

    /**
     * @brief Met à jour l'historique
     */
    void updateHistory(const SpeechAnalysis& analysis);
};

} // namespace mcee

#endif // MCEE_SPEECH_INPUT_HPP
