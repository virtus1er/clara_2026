/**
 * @file MemoryManager.hpp
 * @brief Gestionnaire de mémoire pour le MCEE (interface avec Neo4j)
 * @version 2.1
 * @date 2025-12-21
 *
 * Gère les souvenirs, concepts et traumas.
 * Intégration Neo4j via RabbitMQ pour la persistance long terme.
 *
 * Formule d'activation des souvenirs:
 * A(Si) = forget(Si,t) × (1 + R(Si)) × Σ[C(Si,Sk) × Me(Si,E_current) × U(Si)]
 */

#ifndef MCEE_MEMORY_MANAGER_HPP
#define MCEE_MEMORY_MANAGER_HPP

#include "Types.hpp"
#include "PhaseConfig.hpp"
#include "Neo4jClient.hpp"
#include <vector>
#include <string>
#include <optional>
#include <functional>
#include <memory>

namespace mcee {

/**
 * @class MemoryManager
 * @brief Gère les souvenirs et leur influence sur les émotions
 */
class MemoryManager {
public:
    /**
     * @brief Constructeur
     */
    MemoryManager();

    /**
     * @brief Récupère les souvenirs pertinents selon la phase
     * @param phase Phase émotionnelle actuelle
     * @param state État émotionnel actuel
     * @param max_count Nombre maximum de souvenirs à retourner
     * @return Liste des souvenirs pertinents
     */
    [[nodiscard]] std::vector<Memory> queryRelevantMemories(
        Phase phase,
        const EmotionalState& state,
        size_t max_count = 10
    );

    /**
     * @brief Calcule l'influence des souvenirs sur chaque émotion
     * @param memories Souvenirs actifs
     * @param delta_coeff Coefficient δ de la phase
     * @return Influences par émotion [0-1]
     */
    [[nodiscard]] std::array<double, NUM_EMOTIONS> computeMemoryInfluences(
        const std::vector<Memory>& memories,
        double delta_coeff
    ) const;

    /**
     * @brief Enregistre un nouveau souvenir
     * @param state État émotionnel lors de la création
     * @param phase Phase lors de la création
     * @param context Contexte du souvenir
     * @return Le souvenir créé
     */
    Memory recordMemory(
        const EmotionalState& state,
        Phase phase,
        const std::string& context
    );

    /**
     * @brief Crée un trauma potentiel (appelé en phase PEUR)
     * @param state État émotionnel lors de la création
     * @return Le trauma créé, ou nullopt si non qualifié
     */
    std::optional<Memory> createPotentialTrauma(const EmotionalState& state);

    /**
     * @brief Met à jour l'activation d'un souvenir
     * @param memory Souvenir à mettre à jour
     * @param current_state État émotionnel actuel
     * @return Nouvelle valeur d'activation
     */
    double updateActivation(Memory& memory, const EmotionalState& current_state);

    /**
     * @brief Décide si un souvenir doit être consolidé
     * @param memory Souvenir à évaluer
     * @param phase_at_creation Phase lors de la création
     * @return Action de consolidation recommandée
     */
    [[nodiscard]] std::string shouldConsolidate(
        const Memory& memory,
        Phase phase_at_creation
    ) const;

    /**
     * @brief Retourne tous les souvenirs en mémoire
     */
    [[nodiscard]] const std::vector<Memory>& getAllMemories() const { return memories_; }

    /**
     * @brief Retourne le nombre de souvenirs
     */
    [[nodiscard]] size_t getMemoryCount() const { return memories_.size(); }

    /**
     * @brief Retourne le nombre de traumas
     */
    [[nodiscard]] size_t getTraumaCount() const;

    /**
     * @brief Applique l'oubli aux souvenirs
     * @param decay_factor Facteur de décroissance
     */
    void applyForget(double decay_factor = 0.01);

    /**
     * @brief Définit la configuration Neo4j et établit la connexion
     * @param config Configuration du client Neo4j
     * @return true si connexion réussie
     */
    bool setNeo4jConfig(const Neo4jClientConfig& config);

    /**
     * @brief Vérifie si Neo4j est connecté
     */
    [[nodiscard]] bool isNeo4jConnected() const;

    /**
     * @brief Synchronise les souvenirs locaux vers Neo4j
     * @return Nombre de souvenirs synchronisés
     */
    size_t syncToNeo4j();

    /**
     * @brief Charge les souvenirs depuis Neo4j
     * @param pattern_filter Filtre optionnel par pattern
     * @return Nombre de souvenirs chargés
     */
    size_t loadFromNeo4j(const std::string& pattern_filter = "");

    /**
     * @brief Recherche les souvenirs similaires dans Neo4j
     * @param state État émotionnel de recherche
     * @param threshold Seuil de similarité
     * @param limit Nombre max de résultats
     * @return Souvenirs similaires
     */
    std::vector<Memory> findSimilarInNeo4j(
        const EmotionalState& state,
        double threshold = 0.85,
        size_t limit = 5
    );

    /**
     * @brief Enregistre une transition de pattern dans Neo4j
     * @param from_pattern Pattern source
     * @param to_pattern Pattern destination
     * @param duration_s Durée en secondes
     * @param trigger Déclencheur
     */
    void recordPatternTransition(
        const std::string& from_pattern,
        const std::string& to_pattern,
        double duration_s = 0,
        const std::string& trigger = ""
    );

    /**
     * @brief Retourne le client Neo4j (pour accès avancé)
     */
    Neo4jClient* getNeo4jClient() { return neo4j_client_.get(); }

private:
    // Stockage local des souvenirs
    std::vector<Memory> memories_;

    // Client Neo4j
    std::unique_ptr<Neo4jClient> neo4j_client_;
    bool neo4j_enabled_ = false;

    // ID de session Neo4j
    std::string neo4j_session_id_;

    /**
     * @brief Calcule la correspondance émotionnelle entre état et souvenir
     * @param state État émotionnel actuel
     * @param memory Souvenir à comparer
     * @return Score de correspondance [0-1]
     */
    [[nodiscard]] double computeEmotionalMatch(
        const EmotionalState& state,
        const Memory& memory
    ) const;

    /**
     * @brief Calcule le facteur d'oubli d'un souvenir
     * @param memory Souvenir
     * @return Facteur d'oubli [0-1]
     */
    [[nodiscard]] double computeForgetFactor(const Memory& memory) const;

    /**
     * @brief Calcule le poids initial selon la phase
     * @param phase Phase de création
     * @param intensity Intensité émotionnelle
     * @param valence Valence émotionnelle
     * @return Poids initial
     */
    [[nodiscard]] double computeInitialWeight(
        Phase phase,
        double intensity,
        double valence
    ) const;
};

} // namespace mcee

#endif // MCEE_MEMORY_MANAGER_HPP
