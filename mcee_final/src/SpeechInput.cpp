/**
 * @file SpeechInput.cpp
 * @brief Implémentation du gestionnaire d'entrées textuelles
 * @version 2.0
 * @date 2025-12-19
 */

#include "SpeechInput.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cctype>
#include <regex>

namespace mcee {

using json = nlohmann::json;

SpeechInput::SpeechInput() {
    initDefaultDictionaries();
    std::cout << "[SpeechInput] Gestionnaire d'entrées textuelles initialisé\n";
}

void SpeechInput::initDefaultDictionaries() {
    // Mots de menace / danger (français)
    threat_words_ = {
        "danger", "menace", "attaque", "mort", "tuer", "mourir", "peur",
        "terrifiant", "horrible", "catastrophe", "urgence", "aide", "secours",
        "feu", "accident", "violence", "agression", "blessure", "douleur",
        "arrête", "stop", "non", "jamais", "déteste", "horreur", "terreur",
        "panique", "crise", "alerte", "warning", "attention", "fuite",
        "ennemi", "hostile", "agressif", "dangereux", "mortel", "grave"
    };

    // Mots positifs (français)
    positive_words_ = {
        "bien", "bon", "super", "génial", "excellent", "parfait", "magnifique",
        "merveilleux", "fantastique", "incroyable", "heureux", "content", "joie",
        "amour", "aimer", "adorer", "plaisir", "bonheur", "succès", "victoire",
        "bravo", "félicitations", "merci", "gratitude", "espoir", "confiance",
        "calme", "serein", "paisible", "tranquille", "satisfait", "ravi",
        "sourire", "rire", "fête", "célébrer", "gagner", "réussir", "accomplir",
        "beau", "belle", "joli", "agréable", "plaisant", "délicieux", "savoureux"
    };

    // Mots négatifs (français)
    negative_words_ = {
        "mal", "mauvais", "nul", "horrible", "terrible", "affreux", "détestable",
        "triste", "malheureux", "déprimé", "anxieux", "stressé", "inquiet",
        "colère", "furieux", "énervé", "agacé", "frustré", "déçu", "déception",
        "échec", "perdre", "rater", "échouer", "problème", "souci", "ennui",
        "difficile", "dur", "pénible", "fatigué", "épuisé", "lassé", "ennuyé",
        "seul", "isolé", "abandonné", "rejeté", "incompris", "perdu", "confus",
        "pire", "dommage", "hélas", "malheureusement", "regret", "désolé"
    };

    // Mots à haute activation (arousal élevé)
    high_arousal_words_ = {
        "excité", "enthousiaste", "exalté", "électrisé", "survolté", "explosif",
        "intense", "passionné", "fougueux", "ardent", "vif", "dynamique",
        "urgent", "immédiat", "maintenant", "vite", "rapide", "cours", "fonce",
        "incroyable", "extraordinaire", "spectaculaire", "sensationnel",
        "choquant", "stupéfiant", "sidérant", "bouleversant", "renversant",
        "crier", "hurler", "exploser", "bondir", "sauter", "courir"
    };

    // Mots à basse activation (arousal faible)
    low_arousal_words_ = {
        "calme", "tranquille", "paisible", "serein", "détendu", "relaxé",
        "lent", "doux", "tendre", "léger", "subtil", "discret", "silencieux",
        "fatigué", "épuisé", "las", "somnolent", "endormi", "assoupi",
        "triste", "mélancolique", "nostalgique", "pensif", "rêveur",
        "ennuyé", "monotone", "routinier", "ordinaire", "banal"
    };

    // Scores émotionnels par mot (valence)
    emotion_word_scores_ = {
        // Très positif (+0.8 à +1.0)
        {"adore", 0.9}, {"aime", 0.8}, {"bonheur", 0.95}, {"joie", 0.9},
        {"merveilleux", 0.85}, {"fantastique", 0.85}, {"excellent", 0.8},
        
        // Positif (+0.4 à +0.7)
        {"bien", 0.5}, {"bon", 0.5}, {"content", 0.6}, {"heureux", 0.7},
        {"satisfait", 0.6}, {"agréable", 0.5}, {"plaisant", 0.5},
        
        // Légèrement positif (+0.1 à +0.3)
        {"ok", 0.2}, {"correct", 0.2}, {"acceptable", 0.1},
        
        // Neutre (0)
        {"peut-être", 0.0}, {"normal", 0.0}, {"ordinaire", 0.0},
        
        // Légèrement négatif (-0.1 à -0.3)
        {"bof", -0.2}, {"moyen", -0.1}, {"passable", -0.2},
        
        // Négatif (-0.4 à -0.7)
        {"mal", -0.5}, {"mauvais", -0.5}, {"triste", -0.6}, {"déçu", -0.5},
        {"ennuyé", -0.4}, {"fatigué", -0.4}, {"stressé", -0.5},
        
        // Très négatif (-0.8 à -1.0)
        {"déteste", -0.9}, {"horrible", -0.85}, {"terrible", -0.8},
        {"catastrophe", -0.9}, {"mort", -0.95}, {"peur", -0.7},
        {"terreur", -0.9}, {"horreur", -0.9}, {"panique", -0.8}
    };

    std::cout << "[SpeechInput] Dictionnaires initialisés:\n"
              << "  - Mots de menace: " << threat_words_.size() << "\n"
              << "  - Mots positifs: " << positive_words_.size() << "\n"
              << "  - Mots négatifs: " << negative_words_.size() << "\n"
              << "  - Scores émotionnels: " << emotion_word_scores_.size() << "\n";
}

SpeechAnalysis SpeechInput::processText(const TextInput& input) {
    SpeechAnalysis analysis;
    analysis.raw_text = input.text;
    analysis.timestamp = input.timestamp;

    // Normaliser le texte
    analysis.normalized_text = normalizeText(input.text);

    // Tokenizer
    auto tokens = tokenize(analysis.normalized_text);

    if (tokens.empty()) {
        updateHistory(analysis);
        return analysis;
    }

    // Analyser le sentiment
    analysis.sentiment_score = analyzeSentiment(tokens);

    // Analyser l'arousal
    analysis.arousal_score = analyzeArousal(tokens);

    // Détecter les menaces
    analysis.contains_threat = detectThreats(tokens);

    // Détecter le positif
    analysis.contains_positive = std::any_of(tokens.begin(), tokens.end(),
        [this](const std::string& t) { return positive_words_.count(t) > 0; });

    // Détecter les questions
    analysis.contains_question = (input.text.find('?') != std::string::npos) ||
        (analysis.normalized_text.find("pourquoi") != std::string::npos) ||
        (analysis.normalized_text.find("comment") != std::string::npos) ||
        (analysis.normalized_text.find("quoi") != std::string::npos) ||
        (analysis.normalized_text.find("qui") != std::string::npos);

    // Extraire les mots-clés
    analysis.keywords = extractKeywords(tokens);

    // Extraire les mots émotionnels
    analysis.emotion_words = extractEmotionWords(tokens);

    // Calculer le score d'urgence
    analysis.urgency_score = computeUrgencyScore(analysis);

    // Mettre à jour les statistiques
    processed_count_++;
    sentiment_sum_ += analysis.sentiment_score;
    average_sentiment_ = sentiment_sum_ / static_cast<double>(processed_count_);

    // Mettre à jour l'historique
    updateHistory(analysis);

    // Callbacks
    if (on_text_) {
        on_text_(analysis);
    }

    if (analysis.urgency_score > 0.7 && on_urgency_) {
        on_urgency_(analysis.raw_text, analysis.urgency_score);
    }

    // Log (sauf en mode silencieux)
    if (!quiet_mode_) {
        std::cout << "[SpeechInput] Texte traité:\n"
                  << "  Texte: \"" << (input.text.length() > 50 ? input.text.substr(0, 50) + "..." : input.text) << "\"\n"
                  << "  Sentiment: " << std::fixed << std::setprecision(2) << analysis.sentiment_score << "\n"
                  << "  Arousal: " << analysis.arousal_score << "\n"
                  << "  Urgence: " << analysis.urgency_score << "\n"
                  << "  Menace: " << (analysis.contains_threat ? "OUI" : "non") << "\n";
    }

    return analysis;
}

SpeechAnalysis SpeechInput::processText(const std::string& text, const std::string& source) {
    TextInput input;
    input.text = text;
    input.source = source;
    input.confidence = 1.0;
    return processText(input);
}

double SpeechInput::computeFeedbackExternal(const SpeechAnalysis& analysis) const {
    // Le feedback externe est basé sur:
    // - Le sentiment du texte (poids principal)
    // - L'arousal (modulation)
    // - La présence de menaces (boost négatif)

    double feedback = analysis.sentiment_score;

    // Moduler par l'arousal (amplifie le sentiment)
    feedback *= (1.0 + analysis.arousal_score * 0.5);

    // Boost négatif si menace détectée
    if (analysis.contains_threat) {
        feedback = std::min(feedback, -0.3);
        feedback -= 0.2;
    }

    // Boost positif si contenu positif
    if (analysis.contains_positive && !analysis.contains_threat) {
        feedback = std::max(feedback, 0.1);
        feedback += 0.1;
    }

    return std::clamp(feedback, -1.0, 1.0);
}

std::string SpeechInput::generateMemoryContext(const SpeechAnalysis& analysis) const {
    std::ostringstream context;

    // Ajouter un résumé du sentiment
    if (analysis.sentiment_score > 0.5) {
        context << "Échange positif";
    } else if (analysis.sentiment_score < -0.5) {
        context << "Échange négatif";
    } else {
        context << "Échange neutre";
    }

    // Ajouter les mots-clés
    if (!analysis.keywords.empty()) {
        context << " concernant: ";
        for (size_t i = 0; i < std::min(analysis.keywords.size(), size_t(3)); ++i) {
            if (i > 0) context << ", ";
            context << analysis.keywords[i];
        }
    }

    // Ajouter l'état émotionnel
    if (!analysis.emotion_words.empty()) {
        context << " (émotions: ";
        for (size_t i = 0; i < std::min(analysis.emotion_words.size(), size_t(2)); ++i) {
            if (i > 0) context << ", ";
            context << analysis.emotion_words[i];
        }
        context << ")";
    }

    return context.str();
}

std::vector<SpeechAnalysis> SpeechInput::getRecentAnalyses(size_t count) const {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    size_t start = analysis_history_.size() > count ? 
                   analysis_history_.size() - count : 0;
    
    return std::vector<SpeechAnalysis>(
        analysis_history_.begin() + start,
        analysis_history_.end()
    );
}

void SpeechInput::setTextCallback(TextCallback callback) {
    on_text_ = std::move(callback);
}

void SpeechInput::setUrgencyCallback(UrgencyCallback callback) {
    on_urgency_ = std::move(callback);
}

void SpeechInput::addCustomKeywords(const std::string& category, 
                                     const std::vector<std::string>& words) {
    if (category == "threat") {
        for (const auto& w : words) threat_words_.insert(w);
    } else if (category == "positive") {
        for (const auto& w : words) positive_words_.insert(w);
    } else if (category == "negative") {
        for (const auto& w : words) negative_words_.insert(w);
    } else if (category == "high_arousal") {
        for (const auto& w : words) high_arousal_words_.insert(w);
    } else if (category == "low_arousal") {
        for (const auto& w : words) low_arousal_words_.insert(w);
    }
}

bool SpeechInput::loadEmotionalDictionary(const std::string& path) {
    try {
        std::ifstream file(path);
        if (!file) {
            std::cerr << "[SpeechInput] Fichier non trouvé: " << path << "\n";
            return false;
        }

        json dict;
        file >> dict;

        if (dict.contains("threat_words")) {
            for (const auto& w : dict["threat_words"]) {
                threat_words_.insert(w.get<std::string>());
            }
        }

        if (dict.contains("positive_words")) {
            for (const auto& w : dict["positive_words"]) {
                positive_words_.insert(w.get<std::string>());
            }
        }

        if (dict.contains("negative_words")) {
            for (const auto& w : dict["negative_words"]) {
                negative_words_.insert(w.get<std::string>());
            }
        }

        if (dict.contains("emotion_scores")) {
            for (auto& [word, score] : dict["emotion_scores"].items()) {
                emotion_word_scores_[word] = score.get<double>();
            }
        }

        std::cout << "[SpeechInput] Dictionnaire chargé depuis " << path << "\n";
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[SpeechInput] Erreur chargement dictionnaire: " << e.what() << "\n";
        return false;
    }
}

std::string SpeechInput::normalizeText(const std::string& text) const {
    std::string normalized;
    normalized.reserve(text.size());

    for (unsigned char c : text) {
        if (std::isalpha(c) || c == ' ' || c == '\'') {
            normalized += std::tolower(c);
        } else if (std::isspace(c)) {
            normalized += ' ';
        }
        // Gérer les caractères accentués (UTF-8 simplifié)
        else if (c >= 0xC0) {
            normalized += c;  // Garder les caractères UTF-8
        }
    }

    // Supprimer les espaces multiples
    auto new_end = std::unique(normalized.begin(), normalized.end(),
        [](char a, char b) { return a == ' ' && b == ' '; });
    normalized.erase(new_end, normalized.end());

    // Trim
    size_t start = normalized.find_first_not_of(' ');
    size_t end = normalized.find_last_not_of(' ');
    if (start != std::string::npos && end != std::string::npos) {
        return normalized.substr(start, end - start + 1);
    }

    return normalized;
}

std::vector<std::string> SpeechInput::tokenize(const std::string& text) const {
    std::vector<std::string> tokens;
    std::istringstream stream(text);
    std::string word;

    while (stream >> word) {
        // Supprimer les apostrophes en début/fin
        while (!word.empty() && (word.front() == '\'' || word.front() == ' ')) {
            word.erase(0, 1);
        }
        while (!word.empty() && (word.back() == '\'' || word.back() == ' ')) {
            word.pop_back();
        }

        if (!word.empty() && word.length() > 1) {
            tokens.push_back(word);
        }
    }

    return tokens;
}

double SpeechInput::analyzeSentiment(const std::vector<std::string>& tokens) const {
    if (tokens.empty()) return 0.0;

    double total_score = 0.0;
    int scored_words = 0;

    for (const auto& token : tokens) {
        // Vérifier le dictionnaire de scores
        auto it = emotion_word_scores_.find(token);
        if (it != emotion_word_scores_.end()) {
            total_score += it->second;
            scored_words++;
            continue;
        }

        // Vérifier les listes positives/négatives
        if (positive_words_.count(token) > 0) {
            total_score += 0.5;
            scored_words++;
        } else if (negative_words_.count(token) > 0) {
            total_score -= 0.5;
            scored_words++;
        }
    }

    if (scored_words == 0) return 0.0;

    return std::clamp(total_score / scored_words, -1.0, 1.0);
}

double SpeechInput::analyzeArousal(const std::vector<std::string>& tokens) const {
    if (tokens.empty()) return 0.5;

    int high_count = 0;
    int low_count = 0;

    for (const auto& token : tokens) {
        if (high_arousal_words_.count(token) > 0) {
            high_count++;
        } else if (low_arousal_words_.count(token) > 0) {
            low_count++;
        }
    }

    if (high_count == 0 && low_count == 0) return 0.5;

    double ratio = static_cast<double>(high_count) / (high_count + low_count);
    return ratio;
}

bool SpeechInput::detectThreats(const std::vector<std::string>& tokens) const {
    for (const auto& token : tokens) {
        if (threat_words_.count(token) > 0) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> SpeechInput::extractKeywords(
    const std::vector<std::string>& tokens) const 
{
    std::vector<std::string> keywords;
    
    // Mots à ignorer (stop words français)
    static const std::unordered_set<std::string> stop_words = {
        "le", "la", "les", "un", "une", "des", "du", "de", "et", "ou",
        "je", "tu", "il", "elle", "nous", "vous", "ils", "elles",
        "ce", "cette", "ces", "mon", "ma", "mes", "ton", "ta", "tes",
        "son", "sa", "ses", "notre", "votre", "leur", "leurs",
        "qui", "que", "quoi", "dont", "où", "est", "sont", "suis",
        "ai", "as", "avons", "avez", "ont", "être", "avoir",
        "pour", "avec", "sans", "dans", "sur", "sous", "par",
        "ne", "pas", "plus", "moins", "très", "trop", "aussi"
    };

    for (const auto& token : tokens) {
        if (token.length() > 2 && stop_words.count(token) == 0) {
            // Vérifier si pas déjà présent
            if (std::find(keywords.begin(), keywords.end(), token) == keywords.end()) {
                keywords.push_back(token);
            }
        }
    }

    // Limiter à 10 mots-clés
    if (keywords.size() > 10) {
        keywords.resize(10);
    }

    return keywords;
}

std::vector<std::string> SpeechInput::extractEmotionWords(
    const std::vector<std::string>& tokens) const 
{
    std::vector<std::string> emotion_words;

    for (const auto& token : tokens) {
        if (emotion_word_scores_.count(token) > 0 ||
            positive_words_.count(token) > 0 ||
            negative_words_.count(token) > 0 ||
            threat_words_.count(token) > 0) {
            emotion_words.push_back(token);
        }
    }

    return emotion_words;
}

double SpeechInput::computeUrgencyScore(const SpeechAnalysis& analysis) const {
    double urgency = 0.0;

    // Menace détectée = urgence élevée
    if (analysis.contains_threat) {
        urgency += 0.5;
    }

    // Arousal élevé = urgence modérée
    urgency += analysis.arousal_score * 0.3;

    // Sentiment très négatif = urgence
    if (analysis.sentiment_score < -0.5) {
        urgency += std::abs(analysis.sentiment_score) * 0.2;
    }

    // Mots d'urgence spécifiques
    static const std::unordered_set<std::string> urgency_words = {
        "urgence", "urgent", "vite", "maintenant", "immédiatement",
        "aide", "secours", "sos", "danger", "alerte", "attention"
    };

    for (const auto& kw : analysis.keywords) {
        if (urgency_words.count(kw) > 0) {
            urgency += 0.2;
        }
    }

    return std::clamp(urgency, 0.0, 1.0);
}

void SpeechInput::updateHistory(const SpeechAnalysis& analysis) {
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    last_analysis_ = analysis;
    analysis_history_.push_back(analysis);

    // Limiter la taille de l'historique
    if (analysis_history_.size() > 1000) {
        analysis_history_.erase(analysis_history_.begin(),
                                analysis_history_.begin() + 500);
    }
}

} // namespace mcee
