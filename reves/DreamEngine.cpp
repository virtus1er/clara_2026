#include "DreamEngine.hpp"

#include <algorithm>
#include <numeric>
#include <cmath>
#include <unordered_map>

namespace MCEE {

// ═══════════════════════════════════════════════════════════════════════════
// CONSTRUCTEUR
// ═══════════════════════════════════════════════════════════════════════════

DreamEngine::DreamEngine(const DreamConfig& config)
    : config_(config)
    , cycleStartTime_(std::chrono::steady_clock::now())
    , lastDreamEndTime_(std::chrono::steady_clock::now())
    , currentPhaseStartTime_(std::chrono::steady_clock::now())
{
    currentEmotionalState_.fill(0.0);
}

// ═══════════════════════════════════════════════════════════════════════════
// CYCLE PRINCIPAL
// ═══════════════════════════════════════════════════════════════════════════

void DreamEngine::update(const std::array<double, 24>& currentEmotionalState,
                         const std::string& activePattern,
                         bool amyghaleonAlert) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    currentEmotionalState_ = currentEmotionalState;
    activePattern_ = activePattern;
    
    auto now = std::chrono::steady_clock::now();
    
    // Gestion interruption Amyghaleon
    if (amyghaleonAlert && isDreaming(currentState_)) {
        transitionTo(DreamState::INTERRUPTED);
        stats_.totalInterruptions++;
        return;
    }
    
    // Reprise après interruption
    if (currentState_ == DreamState::INTERRUPTED && !amyghaleonAlert) {
        transitionTo(DreamState::AWAKE);
        lastDreamEndTime_ = now;  // Reset pour éviter redémarrage immédiat
        return;
    }
    
    // État AWAKE - vérifier si on doit démarrer le rêve
    if (currentState_ == DreamState::AWAKE) {
        if (shouldStartDream(activePattern, amyghaleonAlert)) {
            transitionTo(DreamState::DREAM_SCAN);
            currentPhaseStartTime_ = now;
        }
        return;
    }
    
    // États de RÊVE - gérer la progression des phases
    if (isDreaming(currentState_)) {
        auto phaseElapsed = std::chrono::duration<double>(now - currentPhaseStartTime_).count();
        
        double phaseDuration = 0.0;
        switch (currentState_) {
            case DreamState::DREAM_SCAN:
                phaseDuration = config_.scanDuration_s();
                if (phaseElapsed >= phaseDuration) {
                    executeScanPhase();
                    transitionTo(DreamState::DREAM_CONSOLIDATE);
                    currentPhaseStartTime_ = now;
                }
                break;
                
            case DreamState::DREAM_CONSOLIDATE:
                phaseDuration = config_.consolidateDuration_s();
                if (phaseElapsed >= phaseDuration) {
                    executeConsolidatePhase();
                    transitionTo(DreamState::DREAM_EXPLORE);
                    currentPhaseStartTime_ = now;
                }
                break;
                
            case DreamState::DREAM_EXPLORE:
                phaseDuration = config_.exploreDuration_s();
                if (phaseElapsed >= phaseDuration) {
                    executeExplorePhase();
                    transitionTo(DreamState::DREAM_CLEANUP);
                    currentPhaseStartTime_ = now;
                }
                break;
                
            case DreamState::DREAM_CLEANUP:
                phaseDuration = config_.cleanupDuration_s();
                if (phaseElapsed >= phaseDuration) {
                    executeCleanupPhase();
                    transitionTo(DreamState::AWAKE);
                    lastDreamEndTime_ = now;
                    cycleStartTime_ = now;  // Nouveau cycle
                    stats_.totalCyclesCompleted++;
                }
                break;
                
            default:
                break;
        }
    }
}

void DreamEngine::forceDreamStart() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (currentState_ == DreamState::AWAKE) {
        transitionTo(DreamState::DREAM_SCAN);
        currentPhaseStartTime_ = std::chrono::steady_clock::now();
    }
}

void DreamEngine::interruptDream() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isDreaming(currentState_)) {
        transitionTo(DreamState::INTERRUPTED);
        stats_.totalInterruptions++;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// PHASES DU RÊVE
// ═══════════════════════════════════════════════════════════════════════════

void DreamEngine::executeScanPhase() {
    // Phase 1: Scanner la MCT et calculer les scores Csocial(t)
    scoredMemories_.clear();
    
    for (auto& memory : mctBuffer_) {
        memory.consolidationScore = calculateConsolidationScore(memory, currentEmotionalState_);
        scoredMemories_.push_back(memory);
    }
    
    // Trier par score décroissant
    std::sort(scoredMemories_.begin(), scoredMemories_.end(),
              [](const Memory& a, const Memory& b) {
                  return a.consolidationScore > b.consolidationScore;
              });
}

void DreamEngine::executeConsolidatePhase() {
    // Phase 2: Transférer les souvenirs significatifs vers MLT
    double totalScore = 0.0;
    int consolidated = 0;
    
    for (const auto& memory : scoredMemories_) {
        // Toujours consolider les traumas
        bool shouldConsolidate = memory.isTrauma || 
                                 memory.consolidationScore >= config_.consolidationThreshold;
        
        if (shouldConsolidate) {
            // Appeler le callback Neo4j pour persistance
            if (consolidateCallback_) {
                consolidateCallback_(memory);
            }
            totalScore += memory.consolidationScore;
            consolidated++;
        }
    }
    
    // Mettre à jour les stats
    if (consolidated > 0) {
        stats_.totalMemoriesConsolidated += consolidated;
        stats_.averageConsolidationScore = 
            (stats_.averageConsolidationScore * (stats_.totalMemoriesConsolidated - consolidated) + totalScore) 
            / stats_.totalMemoriesConsolidated;
    }
    
    // Préparer les arêtes à renforcer
    edgesToReinforce_.clear();
    for (size_t i = 0; i < scoredMemories_.size() && i < 10; ++i) {
        for (size_t j = i + 1; j < scoredMemories_.size() && j < 10; ++j) {
            if (canCreateAssociation(scoredMemories_[i], scoredMemories_[j])) {
                MemoryEdge edge;
                edge.sourceId = scoredMemories_[i].id;
                edge.targetId = scoredMemories_[j].id;
                edge.weight = (scoredMemories_[i].consolidationScore + 
                              scoredMemories_[j].consolidationScore) / 2.0;
                edge.relationType = "emotional";
                edge.lastActivation = std::chrono::steady_clock::now();
                edgesToReinforce_.push_back(edge);
            }
        }
    }
    
    // Appliquer le renforcement
    for (const auto& edge : edgesToReinforce_) {
        double newWeight = edge.weight * config_.reinforcementFactor;
        if (reinforceCallback_) {
            reinforceCallback_(edge, newWeight);
        }
    }
}

void DreamEngine::executeExplorePhase() {
    // Phase 3: Associations stochastiques - créer des liens inédits
    edgesToCreate_.clear();

    // 3a: Explorer les associations causales (basées sur MCTGraph)
    exploreCausalAssociations();

    // 3b: Associations stochastiques classiques
    double sigma = generateStochasticNoise(activePattern_);
    std::normal_distribution<double> noise(0.0, sigma);

    // Explorer des associations entre souvenirs non évidemment liés
    for (size_t i = 0; i < scoredMemories_.size(); ++i) {
        for (size_t j = i + 2; j < scoredMemories_.size(); ++j) {  // Skip voisins directs
            double randomFactor = std::abs(noise(rng_));

            // Association probabiliste basée sur le bruit
            if (randomFactor > sigma * 0.5) {
                // Calculer similarité émotionnelle
                double similarity = 1.0 - emotionalDistance(
                    scoredMemories_[i].emotionalVector,
                    scoredMemories_[j].emotionalVector
                ) / std::sqrt(24.0);  // Normaliser

                // Créer l'arête si similarité + bruit suffisants
                if (similarity + randomFactor > 0.6) {
                    MemoryEdge newEdge;
                    newEdge.sourceId = scoredMemories_[i].id;
                    newEdge.targetId = scoredMemories_[j].id;
                    newEdge.weight = similarity * randomFactor;
                    newEdge.relationType = "stochastic";
                    newEdge.lastActivation = std::chrono::steady_clock::now();

                    edgesToCreate_.push_back(newEdge);

                    if (createEdgeCallback_) {
                        createEdgeCallback_(newEdge);
                    }
                    stats_.totalEdgesCreated++;
                }
            }
        }
    }
}

void DreamEngine::exploreCausalAssociations() {
    // Créer des associations basées sur les liens causaux mot→émotion
    // Si deux souvenirs partagent un mot déclencheur commun, ils sont liés

    if (causalLinks_.empty()) return;

    // Construire un index: mot→liste de souvenirs ayant cette émotion dominante
    std::unordered_map<std::string, std::vector<size_t>> wordToMemories;

    for (const auto& link : causalLinks_) {
        // Trouver les souvenirs ayant la même émotion dominante
        for (size_t i = 0; i < scoredMemories_.size(); ++i) {
            // Trouver l'émotion dominante du souvenir
            auto& emotions = scoredMemories_[i].emotionalVector;
            size_t maxIdx = 0;
            double maxVal = emotions[0];
            for (size_t j = 1; j < 24; ++j) {
                if (emotions[j] > maxVal) {
                    maxVal = emotions[j];
                    maxIdx = j;
                }
            }

            // Correspondance approximative avec l'émotion du lien causal
            // (on utilise le nom de l'émotion dominante du lien)
            if (maxVal > 0.1) {
                wordToMemories[link.wordLemma].push_back(i);
            }
        }
    }

    // Créer des arêtes entre souvenirs partageant un mot déclencheur
    for (const auto& [word, memoryIndices] : wordToMemories) {
        if (memoryIndices.size() < 2) continue;

        // Trouver le lien causal correspondant pour obtenir la force
        double causalStrength = 0.5;
        for (const auto& link : causalLinks_) {
            if (link.wordLemma == word) {
                causalStrength = link.causalStrength;
                break;
            }
        }

        // Créer des arêtes entre toutes les paires (limité à 5 pour éviter explosion)
        size_t maxPairs = std::min(memoryIndices.size(), (size_t)5);
        for (size_t i = 0; i < maxPairs; ++i) {
            for (size_t j = i + 1; j < maxPairs; ++j) {
                const auto& m1 = scoredMemories_[memoryIndices[i]];
                const auto& m2 = scoredMemories_[memoryIndices[j]];

                MemoryEdge newEdge;
                newEdge.sourceId = m1.id;
                newEdge.targetId = m2.id;
                newEdge.weight = causalStrength * 0.8;  // Poids basé sur force causale
                newEdge.relationType = "causal_association";
                newEdge.lastActivation = std::chrono::steady_clock::now();

                edgesToCreate_.push_back(newEdge);

                if (createEdgeCallback_) {
                    createEdgeCallback_(newEdge);
                }
                stats_.totalEdgesCreated++;
            }
        }
    }

    // Bonus: Créer des arêtes directement depuis les top trigger words
    for (const auto& triggerWord : causalStats_.topTriggerWords) {
        // Chercher les liens causaux avec ce mot
        std::vector<std::string> emotionIds;
        for (const auto& link : causalLinks_) {
            if (link.wordLemma == triggerWord) {
                emotionIds.push_back(link.dominantEmotion);
            }
        }

        // Signaler les mots déclencheurs importants (pour debug/log)
        if (!emotionIds.empty()) {
            // Le mot déclencheur crée des associations plus fortes
            // (logique métier: ces mots sont significatifs pour Clara)
        }
    }
}

void DreamEngine::executeCleanupPhase() {
    // Phase 4: Supprimer les souvenirs sous le seuil (sauf traumas)
    memoriesToDelete_.clear();
    
    for (const auto& memory : scoredMemories_) {
        // Ne jamais supprimer les traumas
        if (memory.isTrauma) {
            continue;
        }
        
        // Appliquer l'oubli exponentiel
        double decayedScore = memory.consolidationScore * 
                              std::exp(-config_.forgetDecayRate);
        
        if (decayedScore < config_.minWeightBeforeDeletion) {
            memoriesToDelete_.push_back(memory.id);
            
            if (deleteCallback_) {
                deleteCallback_(memory.id);
            }
            stats_.totalMemoriesForgotten++;
        }
    }
    
    // Vider la MCT des éléments traités
    mctBuffer_.clear();
    scoredMemories_.clear();
}

// ═══════════════════════════════════════════════════════════════════════════
// CALCULS
// ═══════════════════════════════════════════════════════════════════════════

double DreamEngine::calculateConsolidationScore(
    const Memory& memory,
    const std::array<double, 24>& currentEmotionalState) const {
    
    // Csocial(t) = ρ·|Ecurrent - Esouvenir| + λ·Feedback + η·Usage + θ·Influence
    
    double emotionalDist = emotionalDistance(currentEmotionalState, memory.emotionalVector);
    
    // Normaliser la distance (max théorique = sqrt(24) ≈ 4.9)
    double normalizedDist = emotionalDist / std::sqrt(24.0);
    
    // Inverser: on veut favoriser les souvenirs proches émotionnellement
    // mais aussi ceux très distants (contrastes significatifs)
    double emotionalComponent = normalizedDist;  // On garde la distance brute
    
    // Normaliser usage (log pour éviter domination)
    double usageNorm = std::log1p(memory.usageCount) / 5.0;  // log(1+x), plafonné
    usageNorm = std::min(usageNorm, 1.0);
    
    // Calculer le score
    double score = config_.rho * emotionalComponent +
                   config_.lambda * memory.feedback +
                   config_.eta * usageNorm +
                   config_.theta * memory.decisionalInfluence;
    
    // Bonus pour souvenirs sociaux
    if (memory.isSocial) {
        score *= 1.2;
    }
    
    // Protection trauma: score très élevé
    if (memory.isTrauma) {
        score = std::max(score, config_.consolidationThreshold * 2.0);
    }
    
    return std::clamp(score, 0.0, 1.0);
}

double DreamEngine::emotionalDistance(
    const std::array<double, 24>& e1,
    const std::array<double, 24>& e2) const {
    
    double sumSquares = 0.0;
    for (size_t i = 0; i < 24; ++i) {
        double diff = e1[i] - e2[i];
        sumSquares += diff * diff;
    }
    return std::sqrt(sumSquares);
}

double DreamEngine::calculateEmotionalIntensity(
    const std::array<double, 24>& emotions) const {
    
    double sum = std::accumulate(emotions.begin(), emotions.end(), 0.0,
                                 [](double acc, double e) { return acc + std::abs(e); });
    return sum / 24.0;
}

double DreamEngine::generateStochasticNoise(const std::string& activePattern) const {
    double multiplier = 1.0;
    
    if (activePattern == "EXPLORATION") {
        multiplier = config_.sigmaMultiplier_EXPLORATION;
    } else if (activePattern == "SERENITE") {
        multiplier = config_.sigmaMultiplier_SERENITE;
    } else if (activePattern == "JOIE") {
        multiplier = config_.sigmaMultiplier_JOIE;
    } else if (activePattern == "ANXIETE") {
        multiplier = config_.sigmaMultiplier_ANXIETE;
    } else if (activePattern == "PEUR") {
        multiplier = config_.sigmaMultiplier_PEUR;
    } else if (activePattern == "TRISTESSE") {
        multiplier = config_.sigmaMultiplier_TRISTESSE;
    } else if (activePattern == "DEGOUT") {
        multiplier = config_.sigmaMultiplier_DEGOUT;
    } else if (activePattern == "CONFUSION") {
        multiplier = config_.sigmaMultiplier_CONFUSION;
    }
    
    return config_.sigmaBase * multiplier;
}

bool DreamEngine::canCreateAssociation(const Memory& m1, const Memory& m2) const {
    // Même type de mémoire
    if (m1.type == m2.type) return true;
    
    // Même contexte
    if (!m1.contexte.empty() && m1.contexte == m2.contexte) return true;
    
    // Même interlocuteur (pour souvenirs sociaux)
    if (m1.isSocial && m2.isSocial && 
        !m1.interlocuteur.empty() && m1.interlocuteur == m2.interlocuteur) {
        return true;
    }
    
    // Proximité émotionnelle
    double dist = emotionalDistance(m1.emotionalVector, m2.emotionalVector);
    if (dist < 1.0) return true;  // Très proches
    
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// TRANSITIONS D'ÉTAT
// ═══════════════════════════════════════════════════════════════════════════

void DreamEngine::transitionTo(DreamState newState) {
    DreamState oldState = currentState_;
    currentState_ = newState;
    
    if (stateChangeCallback_ && oldState != newState) {
        stateChangeCallback_(oldState, newState);
    }
}

bool DreamEngine::shouldStartDream(const std::string& activePattern, 
                                   bool amyghaleonAlert) const {
    // Condition 1: Pas d'alerte Amyghaleon
    if (amyghaleonAlert) return false;
    
    // Condition 2: Patterns bloquants
    if (config_.blockDreamOnPEUR && activePattern == "PEUR") return false;
    if (config_.blockDreamOnANXIETE && activePattern == "ANXIETE") return false;
    
    // Condition 3: Temps minimum depuis dernier rêve
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - lastDreamEndTime_).count();
    if (elapsed < config_.minTimeSinceLastDream_s) return false;
    
    // Condition 4: Activité émotionnelle basse
    double intensity = calculateEmotionalIntensity(currentEmotionalState_);
    if (intensity > config_.maxEmotionalActivityForDream) return false;
    
    // Condition 5: Il y a des souvenirs à traiter
    if (mctBuffer_.empty()) return false;
    
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════
// ACCÈS À L'ÉTAT
// ═══════════════════════════════════════════════════════════════════════════

DreamState DreamEngine::getCurrentState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentState_;
}

double DreamEngine::getCycleProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - cycleStartTime_).count();
    return std::fmod(elapsed, config_.cyclePeriod_s) / config_.cyclePeriod_s;
}

double DreamEngine::getDreamPhaseProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isDreaming(currentState_)) return 0.0;
    
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - currentPhaseStartTime_).count();
    
    double phaseDuration = 0.0;
    switch (currentState_) {
        case DreamState::DREAM_SCAN:       phaseDuration = config_.scanDuration_s(); break;
        case DreamState::DREAM_CONSOLIDATE: phaseDuration = config_.consolidateDuration_s(); break;
        case DreamState::DREAM_EXPLORE:    phaseDuration = config_.exploreDuration_s(); break;
        case DreamState::DREAM_CLEANUP:    phaseDuration = config_.cleanupDuration_s(); break;
        default: return 0.0;
    }
    
    return std::min(elapsed / phaseDuration, 1.0);
}

double DreamEngine::getTimeSinceLastDream_s() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - lastDreamEndTime_).count();
}

bool DreamEngine::canStartDream() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return shouldStartDream(activePattern_, false);
}

// ═══════════════════════════════════════════════════════════════════════════
// GESTION MCT
// ═══════════════════════════════════════════════════════════════════════════

void DreamEngine::addMemoryToMCT(const Memory& memory) {
    std::lock_guard<std::mutex> lock(mutex_);
    mctBuffer_.push_back(memory);
}

const std::vector<Memory>& DreamEngine::getMCTMemories() const {
    // Note: pas de lock ici car on retourne une référence
    // L'appelant doit s'assurer de la synchronisation
    return mctBuffer_;
}

void DreamEngine::clearMCT() {
    std::lock_guard<std::mutex> lock(mutex_);
    mctBuffer_.clear();
}

// ═══════════════════════════════════════════════════════════════════════════
// GESTION DES LIENS CAUSAUX (MCTGraph)
// ═══════════════════════════════════════════════════════════════════════════

void DreamEngine::processMCTGraphSnapshot(
    const std::vector<WordNodeSnapshot>& words,
    const std::vector<CausalLink>& causalLinks,
    const CausalStats& stats) {

    std::lock_guard<std::mutex> lock(mutex_);

    // Stocker les nouvelles données
    wordNodes_ = words;
    causalLinks_ = causalLinks;
    causalStats_ = stats;
}

const std::vector<CausalLink>& DreamEngine::getCausalLinks() const {
    return causalLinks_;
}

const std::vector<std::string>& DreamEngine::getTopTriggerWords() const {
    return causalStats_.topTriggerWords;
}

void DreamEngine::clearCausalLinks() {
    std::lock_guard<std::mutex> lock(mutex_);
    causalLinks_.clear();
    wordNodes_.clear();
    causalStats_ = CausalStats{};
}

// ═══════════════════════════════════════════════════════════════════════════
// CALLBACKS
// ═══════════════════════════════════════════════════════════════════════════

void DreamEngine::setStateChangeCallback(DreamStateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    stateChangeCallback_ = std::move(callback);
}

void DreamEngine::setNeo4jConsolidateCallback(Neo4jConsolidateCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    consolidateCallback_ = std::move(callback);
}

void DreamEngine::setNeo4jReinforceCallback(Neo4jReinforceCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    reinforceCallback_ = std::move(callback);
}

void DreamEngine::setNeo4jDeleteCallback(Neo4jDeleteCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    deleteCallback_ = std::move(callback);
}

void DreamEngine::setNeo4jCreateEdgeCallback(Neo4jCreateEdgeCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    createEdgeCallback_ = std::move(callback);
}

// ═══════════════════════════════════════════════════════════════════════════
// CONFIGURATION & STATS
// ═══════════════════════════════════════════════════════════════════════════

void DreamEngine::setConfig(const DreamConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

const DreamConfig& DreamEngine::getConfig() const {
    return config_;
}

DreamEngine::Stats DreamEngine::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void DreamEngine::resetStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    stats_ = Stats{};
}

} // namespace MCEE
