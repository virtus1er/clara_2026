/**
 * @file HybridSearchEngine.cpp
 * @brief Implémentation du moteur de recherche hybride
 * @version 1.0
 * @date 2024-12
 */

#include "HybridSearchEngine.hpp"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <iostream>
#include <functional>
#include <unordered_set>

namespace mcee {

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTEUR
// ═══════════════════════════════════════════════════════════════════════════

HybridSearchEngine::HybridSearchEngine(
    std::shared_ptr<Neo4jClient> neo4j_client,
    std::shared_ptr<ConscienceEngine> conscience_engine,
    const HybridSearchConfig& config)
    : neo4j_(std::move(neo4j_client))
    , conscience_(std::move(conscience_engine))
    , config_(config) {
}

// ═══════════════════════════════════════════════════════════════════════════
// RECHERCHE PRINCIPALE
// ═══════════════════════════════════════════════════════════════════════════

SearchResponse HybridSearchEngine::search(
    const std::string& question,
    const std::vector<ParsedToken>& tokens,
    const std::vector<double>& embedding) {

    auto start_time = std::chrono::steady_clock::now();
    SearchResponse response;

    // Obtenir l'état de conscience actuel
    ConscienceSentimentState conscience_state;
    if (conscience_) {
        conscience_state = conscience_->getCurrentState();
    }
    response.Ft = conscience_state.sentiment;
    response.Ct = conscience_state.consciousness_level;

    // Extraire les lemmes significatifs
    auto lemmas = extractSignificantLemmas(tokens);

    log("Recherche avec Ft=" + std::to_string(response.Ft)
        + ", Ct=" + std::to_string(response.Ct));
    log("Lemmes: " + [&]() {
        std::ostringstream oss;
        for (const auto& l : lemmas) oss << l << " ";
        return oss.str();
    }());

    // Vérifier le cache
    std::string cache_key = generateCacheKey(lemmas);
    if (config_.enable_cache && isCacheValid(cache_key)) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto& cached = cache_[cache_key];
        response = cached.first;
        log("Résultat depuis cache");

        auto end_time = std::chrono::steady_clock::now();
        response.search_time_ms = std::chrono::duration<double, std::milli>(
            end_time - start_time).count();
        return response;
    }

    // Déterminer le mode de recherche
    response.mode_used = determineSearchMode(lemmas, embedding, response.Ft, response.Ct);

    // Recherche lexicale
    std::vector<SearchResult> lexical_results;
    if (response.mode_used != SearchMode::SEMANTIC_ONLY && !lemmas.empty()) {
        lexical_results = searchLexical(lemmas);
        response.lexical_confidence = lexical_results.empty() ? 0.0 :
            std::accumulate(lexical_results.begin(), lexical_results.end(), 0.0,
                [](double sum, const SearchResult& r) { return sum + r.lexical_score; })
            / lexical_results.size();

        log("Lexical: " + std::to_string(lexical_results.size())
            + " résultats, confiance=" + std::to_string(response.lexical_confidence));
    }

    // Recherche sémantique
    std::vector<SearchResult> semantic_results;
    if (response.mode_used != SearchMode::LEXICAL_ONLY && !embedding.empty()) {
        semantic_results = searchSemantic(embedding);
        response.semantic_confidence = semantic_results.empty() ? 0.0 :
            std::accumulate(semantic_results.begin(), semantic_results.end(), 0.0,
                [](double sum, const SearchResult& r) { return sum + r.semantic_score; })
            / semantic_results.size();

        log("Sémantique: " + std::to_string(semantic_results.size())
            + " résultats, confiance=" + std::to_string(response.semantic_confidence));
    }

    // Calculer les poids adaptatifs
    auto [lex_weight, sem_weight] = computeAdaptiveWeights(
        response.Ft, response.Ct,
        response.lexical_confidence,
        response.semantic_confidence
    );

    log("Mode: " + std::to_string(static_cast<int>(response.mode_used))
        + " (lex=" + std::to_string(lex_weight)
        + ", sem=" + std::to_string(sem_weight) + ")");

    // Fusionner les résultats
    response.results = mergeResults(lexical_results, semantic_results, lex_weight, sem_weight);

    // Appliquer la modulation par sentiment
    if (config_.enable_sentiment_modulation) {
        applySentimentModulation(response.results, response.Ft);
    }

    // Trier par score combiné décroissant
    std::sort(response.results.begin(), response.results.end(),
        [](const SearchResult& a, const SearchResult& b) {
            return a.combined_score > b.combined_score;
        });

    // Limiter le nombre de résultats
    if (response.results.size() > config_.max_results) {
        response.results.resize(config_.max_results);
    }

    // Calculer la confiance globale
    if (!response.results.empty()) {
        response.overall_confidence =
            lex_weight * response.lexical_confidence +
            sem_weight * response.semantic_confidence;
    }

    // Construire le contexte LLM
    response.context = buildContext(response.results, lemmas);

    // Mettre en cache
    if (config_.enable_cache) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cache_[cache_key] = {response, std::chrono::steady_clock::now()};
    }

    auto end_time = std::chrono::steady_clock::now();
    response.search_time_ms = std::chrono::duration<double, std::milli>(
        end_time - start_time).count();

    log("Terminé en " + std::to_string(response.search_time_ms) + "ms, "
        + std::to_string(response.results.size()) + " résultats");

    return response;
}

SearchResponse HybridSearchEngine::searchByLemmas(
    const std::vector<std::string>& lemmas,
    const std::vector<double>& embedding) {

    // Convertir en tokens simples
    std::vector<ParsedToken> tokens;
    for (const auto& lemma : lemmas) {
        ParsedToken t;
        t.text = lemma;
        t.lemma = lemma;
        t.pos = "NOUN";  // Supposer NOUN par défaut
        t.is_significant = true;
        tokens.push_back(t);
    }

    return search("", tokens, embedding);
}

// ═══════════════════════════════════════════════════════════════════════════
// PARSING TOKENS
// ═══════════════════════════════════════════════════════════════════════════

std::vector<ParsedToken> HybridSearchEngine::parseTokens(const json& tokens_json) const {
    std::vector<ParsedToken> tokens;

    if (!tokens_json.is_array()) {
        return tokens;
    }

    for (const auto& t : tokens_json) {
        tokens.push_back(ParsedToken::fromJson(t));
    }

    return tokens;
}

std::vector<std::string> HybridSearchEngine::extractSignificantLemmas(
    const std::vector<ParsedToken>& tokens) const {

    std::vector<std::string> lemmas;

    // POS significatifs pour la recherche
    static const std::unordered_set<std::string> significant_pos = {
        "NOUN", "VERB", "ADJ", "PROPN", "ADV"
    };

    for (const auto& token : tokens) {
        if (token.is_stop) continue;
        if (!token.is_significant) continue;
        if (token.lemma.empty()) continue;
        if (token.lemma.length() < 2) continue;

        // Filtrer par POS si disponible
        if (!token.pos.empty() && significant_pos.find(token.pos) == significant_pos.end()) {
            continue;
        }

        // Éviter les doublons
        if (std::find(lemmas.begin(), lemmas.end(), token.lemma) == lemmas.end()) {
            lemmas.push_back(token.lemma);
        }
    }

    return lemmas;
}

// ═══════════════════════════════════════════════════════════════════════════
// RECHERCHE LEXICALE
// ═══════════════════════════════════════════════════════════════════════════

std::vector<SearchResult> HybridSearchEngine::searchLexical(
    const std::vector<std::string>& lemmas) {

    std::vector<SearchResult> results;

    if (!neo4j_ || !neo4j_->isConnected() || lemmas.empty()) {
        return results;
    }

    // Construire la requête Cypher pour recherche par mots-clés
    // Cherche les nœuds Memory liés à des nœuds Word correspondant aux lemmas
    std::ostringstream cypher;
    cypher << R"(
        MATCH (m:Memory)-[:ASSOCIATED_WITH]->(w:Word)
        WHERE w.lemma IN $lemmas
        WITH m, COUNT(DISTINCT w) as match_count, COLLECT(w.lemma) as matched_words
        RETURN m.id as memory_id,
               m.name as memory_name,
               m.emotions as emotions,
               m.dominant as dominant,
               m.dominant_score as dominant_score,
               m.is_trauma as is_trauma,
               m.activation_count as activation_count,
               m.last_activated as last_activated,
               matched_words,
               toFloat(match_count) / toFloat($total_lemmas) as lexical_score
        ORDER BY lexical_score DESC
        LIMIT $limit
    )";

    json params;
    params["lemmas"] = lemmas;
    params["total_lemmas"] = lemmas.size();
    params["limit"] = static_cast<int>(config_.max_results * 2);  // Marge pour fusion

    try {
        json result = neo4j_->executeCypher(cypher.str(), params);

        if (result.contains("records")) {
            for (const auto& record : result["records"]) {
                SearchResult sr;
                sr.memory_id = record.value("memory_id", "");
                sr.memory_name = record.value("memory_name", "");
                sr.lexical_score = record.value("lexical_score", 0.0);
                sr.dominant_emotion = record.value("dominant", "");
                sr.dominant_score = record.value("dominant_score", 0.0);
                sr.is_trauma = record.value("is_trauma", false);
                sr.activation_count = record.value("activation_count", 0);
                sr.last_activated = record.value("last_activated", "");

                // Mots-clés matchés
                if (record.contains("matched_words") && record["matched_words"].is_array()) {
                    for (const auto& w : record["matched_words"]) {
                        if (sr.keywords.size() < config_.max_keywords_per_result) {
                            sr.keywords.push_back(w.get<std::string>());
                        }
                    }
                }

                // Émotions
                if (record.contains("emotions") && record["emotions"].is_array()) {
                    for (size_t i = 0; i < std::min(record["emotions"].size(), NUM_EMOTIONS); ++i) {
                        sr.emotions[i] = record["emotions"][i].get<double>();
                    }
                }

                // Filtrer par score minimum
                if (sr.lexical_score >= config_.min_lexical_score) {
                    results.push_back(sr);
                }
            }
        }
    } catch (const std::exception& e) {
        log("Erreur recherche lexicale: " + std::string(e.what()));
    }

    return results;
}

// ═══════════════════════════════════════════════════════════════════════════
// RECHERCHE SÉMANTIQUE
// ═══════════════════════════════════════════════════════════════════════════

std::vector<SearchResult> HybridSearchEngine::searchSemantic(
    const std::vector<double>& embedding) {

    std::vector<SearchResult> results;

    if (!neo4j_ || !neo4j_->isConnected() || embedding.empty()) {
        return results;
    }

    // Recherche par similarité cosinus d'embedding
    // Utilise l'index vectoriel Neo4j si disponible
    std::ostringstream cypher;
    cypher << R"(
        CALL db.index.vector.queryNodes('memory_embedding_index', $limit, $embedding)
        YIELD node, score
        WHERE score >= $threshold
        RETURN node.id as memory_id,
               node.name as memory_name,
               node.emotions as emotions,
               node.dominant as dominant,
               node.dominant_score as dominant_score,
               node.is_trauma as is_trauma,
               node.activation_count as activation_count,
               node.last_activated as last_activated,
               score as semantic_score
        ORDER BY semantic_score DESC
    )";

    json params;
    params["embedding"] = embedding;
    params["limit"] = static_cast<int>(config_.max_results * 2);
    params["threshold"] = config_.min_semantic_score;

    try {
        json result = neo4j_->executeCypher(cypher.str(), params);

        if (result.contains("records")) {
            for (const auto& record : result["records"]) {
                SearchResult sr;
                sr.memory_id = record.value("memory_id", "");
                sr.memory_name = record.value("memory_name", "");
                sr.semantic_score = record.value("semantic_score", 0.0);
                sr.dominant_emotion = record.value("dominant", "");
                sr.dominant_score = record.value("dominant_score", 0.0);
                sr.is_trauma = record.value("is_trauma", false);
                sr.activation_count = record.value("activation_count", 0);
                sr.last_activated = record.value("last_activated", "");

                // Émotions
                if (record.contains("emotions") && record["emotions"].is_array()) {
                    for (size_t i = 0; i < std::min(record["emotions"].size(), NUM_EMOTIONS); ++i) {
                        sr.emotions[i] = record["emotions"][i].get<double>();
                    }
                }

                results.push_back(sr);
            }
        }
    } catch (const std::exception& e) {
        // Si l'index vectoriel n'existe pas, fallback sur recherche manuelle
        log("Index vectoriel non disponible, fallback désactivé: " + std::string(e.what()));
    }

    return results;
}

// ═══════════════════════════════════════════════════════════════════════════
// FUSION DES RÉSULTATS
// ═══════════════════════════════════════════════════════════════════════════

std::vector<SearchResult> HybridSearchEngine::mergeResults(
    const std::vector<SearchResult>& lexical_results,
    const std::vector<SearchResult>& semantic_results,
    double lexical_weight,
    double semantic_weight) {

    // Map par memory_id pour fusionner
    std::unordered_map<std::string, SearchResult> merged;

    // Ajouter les résultats lexicaux
    for (const auto& r : lexical_results) {
        merged[r.memory_id] = r;
        merged[r.memory_id].combined_score = lexical_weight * r.lexical_score;
    }

    // Fusionner avec les résultats sémantiques
    for (const auto& r : semantic_results) {
        if (merged.find(r.memory_id) != merged.end()) {
            // Fusionner les scores
            merged[r.memory_id].semantic_score = r.semantic_score;
            merged[r.memory_id].combined_score += semantic_weight * r.semantic_score;
        } else {
            // Nouveau résultat
            merged[r.memory_id] = r;
            merged[r.memory_id].combined_score = semantic_weight * r.semantic_score;
        }
    }

    // Convertir en vecteur
    std::vector<SearchResult> results;
    results.reserve(merged.size());
    for (auto& [id, result] : merged) {
        // Normaliser le score combiné si les deux sources contribuent
        if (result.lexical_score > 0 && result.semantic_score > 0) {
            // Bonus pour les résultats trouvés par les deux méthodes
            result.combined_score *= 1.2;
        }

        // Boost pour les traumas pertinents
        if (result.is_trauma) {
            result.combined_score *= config_.trauma_boost;
        }

        results.push_back(std::move(result));
    }

    return results;
}

// ═══════════════════════════════════════════════════════════════════════════
// MODULATION PAR SENTIMENT
// ═══════════════════════════════════════════════════════════════════════════

void HybridSearchEngine::applySentimentModulation(
    std::vector<SearchResult>& results,
    double Ft) {

    for (auto& result : results) {
        // Calculer la valence du souvenir
        double memory_valence = 0.0;

        // Indices des émotions positives et négatives (cf Types.hpp)
        static const std::array<size_t, 14> positive_indices = {
            0, 1, 2, 3, 5, 8, 12, 13, 16, 17, 19, 21, 22, 23
        };
        static const std::array<size_t, 10> negative_indices = {
            4, 6, 7, 9, 10, 11, 14, 15, 18, 20
        };

        double pos_sum = 0.0, neg_sum = 0.0;
        for (auto i : positive_indices) {
            pos_sum += result.emotions[i];
        }
        for (auto i : negative_indices) {
            neg_sum += result.emotions[i];
        }

        double total = pos_sum + neg_sum;
        if (total > 0.01) {
            memory_valence = (pos_sum - neg_sum) / total;  // [-1, 1]
        }

        result.emotional_relevance = memory_valence;

        // Modulation selon Ft
        if (Ft < config_.negative_sentiment_threshold) {
            // Sentiment négatif : favoriser les souvenirs positifs (effet compensatoire)
            if (memory_valence > 0) {
                result.combined_score *= 1.0 + (0.3 * memory_valence * std::abs(Ft));
            }
        } else if (Ft > config_.positive_sentiment_threshold) {
            // Sentiment positif : favoriser les souvenirs similaires (renforcement)
            if (memory_valence > 0) {
                result.combined_score *= 1.0 + (0.2 * memory_valence * Ft);
            }
        }
        // Sentiment neutre : pas de modulation
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// MODE ET POIDS ADAPTATIFS
// ═══════════════════════════════════════════════════════════════════════════

SearchMode HybridSearchEngine::determineSearchMode(
    const std::vector<std::string>& lemmas,
    const std::vector<double>& embedding,
    double Ft,
    double Ct) const {

    if (config_.default_mode != SearchMode::ADAPTIVE) {
        return config_.default_mode;
    }

    bool has_lemmas = !lemmas.empty();
    bool has_embedding = !embedding.empty();

    // Si une seule source disponible
    if (has_lemmas && !has_embedding) return SearchMode::LEXICAL_ONLY;
    if (!has_lemmas && has_embedding) return SearchMode::SEMANTIC_ONLY;
    if (!has_lemmas && !has_embedding) return SearchMode::BALANCED;  // Fallback

    // Adapter selon l'état de conscience
    // - Ct élevé (alerte) : favoriser lexical (plus précis)
    // - Ct bas (calme) : favoriser sémantique (plus associatif)
    if (Ct > 0.7) {
        return SearchMode::BALANCED;  // Mais avec poids adaptatifs
    }

    return SearchMode::BALANCED;
}

std::pair<double, double> HybridSearchEngine::computeAdaptiveWeights(
    double Ft,
    double Ct,
    double lexical_confidence,
    double semantic_confidence) const {

    double lex_weight = config_.lexical_weight;
    double sem_weight = config_.semantic_weight;

    // Adapter selon les confiances
    double total_confidence = lexical_confidence + semantic_confidence;
    if (total_confidence > 0.01) {
        // Pondérer proportionnellement aux confiances
        double conf_lex = lexical_confidence / total_confidence;
        double conf_sem = semantic_confidence / total_confidence;

        // Mélange entre config et confiance (50/50)
        lex_weight = 0.5 * config_.lexical_weight + 0.5 * conf_lex;
        sem_weight = 0.5 * config_.semantic_weight + 0.5 * conf_sem;
    }

    // Modulation par Ct
    // Ct élevé : plus de précision (lexical)
    // Ct bas : plus d'association (sémantique)
    if (Ct > 0.6) {
        lex_weight *= 1.0 + 0.2 * (Ct - 0.5);
        sem_weight *= 1.0 - 0.1 * (Ct - 0.5);
    } else if (Ct < 0.4) {
        sem_weight *= 1.0 + 0.2 * (0.5 - Ct);
        lex_weight *= 1.0 - 0.1 * (0.5 - Ct);
    }

    // Normaliser
    double total = lex_weight + sem_weight;
    if (total > 0.01) {
        lex_weight /= total;
        sem_weight /= total;
    } else {
        lex_weight = 0.5;
        sem_weight = 0.5;
    }

    return {lex_weight, sem_weight};
}

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTION DE CONTEXTE
// ═══════════════════════════════════════════════════════════════════════════

LLMContext HybridSearchEngine::buildContext(
    const std::vector<SearchResult>& results,
    const std::vector<std::string>& lemmas) const {

    LLMContext context;

    // État de conscience
    if (conscience_) {
        auto state = conscience_->getCurrentState();
        context.Ft = state.sentiment;
        context.Ct = state.consciousness_level;
        context.sentiment_label = state.sentiment > 0.2 ? "positif" :
                                  (state.sentiment < -0.2 ? "négatif" : "neutre");
    }

    // Mots-clés contextuels (lemmas + keywords des résultats)
    context.context_words = lemmas;
    for (const auto& result : results) {
        for (const auto& kw : result.keywords) {
            if (std::find(context.context_words.begin(),
                          context.context_words.end(), kw) == context.context_words.end()) {
                context.context_words.push_back(kw);
            }
            if (context.context_words.size() >= 10) break;
        }
        if (context.context_words.size() >= 10) break;
    }

    // Émotions agrégées des résultats
    std::array<double, NUM_EMOTIONS> aggregated_emotions{};
    double total_weight = 0.0;

    for (const auto& result : results) {
        double weight = result.combined_score;
        total_weight += weight;

        for (size_t i = 0; i < NUM_EMOTIONS; ++i) {
            aggregated_emotions[i] += result.emotions[i] * weight;
        }
    }

    // Normaliser et extraire les émotions significatives
    if (total_weight > 0.01) {
        for (size_t i = 0; i < NUM_EMOTIONS; ++i) {
            aggregated_emotions[i] /= total_weight;

            if (aggregated_emotions[i] > 0.2) {
                EmotionScore es;
                es.name = EMOTION_NAMES[i];
                es.score = aggregated_emotions[i];

                // Chercher le trigger dans les keywords
                for (const auto& result : results) {
                    if (result.emotions[i] > 0.3 && !result.keywords.empty()) {
                        es.trigger = result.keywords[0];
                        break;
                    }
                }

                context.emotions.push_back(es);
            }
        }
    }

    // Trier par score décroissant
    std::sort(context.emotions.begin(), context.emotions.end(),
        [](const EmotionScore& a, const EmotionScore& b) {
            return a.score > b.score;
        });

    // Dominante
    if (!context.emotions.empty()) {
        context.dominant_emotion = context.emotions[0].name;
        context.dominant_score = context.emotions[0].score;
    }

    // Souvenirs activés
    for (const auto& result : results) {
        if (!result.memory_name.empty()) {
            context.activated_memories.push_back(result.memory_name);
            if (context.activated_memories.size() >= 5) break;
        }
    }

    // Confiance globale
    if (!results.empty()) {
        context.search_confidence = results[0].combined_score;
    }

    return context;
}

// ═══════════════════════════════════════════════════════════════════════════
// CACHE
// ═══════════════════════════════════════════════════════════════════════════

std::string HybridSearchEngine::generateCacheKey(
    const std::vector<std::string>& lemmas) const {

    // Hash simple des lemmas triés
    std::vector<std::string> sorted_lemmas = lemmas;
    std::sort(sorted_lemmas.begin(), sorted_lemmas.end());

    std::ostringstream oss;
    for (const auto& l : sorted_lemmas) {
        oss << l << "|";
    }

    // Hash basique
    std::hash<std::string> hasher;
    return std::to_string(hasher(oss.str()));
}

bool HybridSearchEngine::isCacheValid(const std::string& key) const {
    std::lock_guard<std::mutex> lock(cache_mutex_);

    auto it = cache_.find(key);
    if (it == cache_.end()) {
        return false;
    }

    auto age = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - it->second.second).count();

    return age < config_.cache_ttl_seconds;
}

// ═══════════════════════════════════════════════════════════════════════════
// LOGGING
// ═══════════════════════════════════════════════════════════════════════════

void HybridSearchEngine::log(const std::string& message) const {
    if (config_.verbose) {
        std::cout << "[HybridSearch] " << message << std::endl;
    }
}

} // namespace mcee
