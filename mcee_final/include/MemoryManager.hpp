/**
 * @file MemoryManager.hpp
 * @brief Gestionnaire de mémoire pour le MCEE (interface avec Neo4j)
 * @version 2.0
 * @date 2025-12-19
 * 
 * Gère les souvenirs, concepts et traumas.
 * L'intégration Neo4j est prévue pour une phase ultérieure.
 * 
 * Formule d'activation des souvenirs:
 * A(Si) = forget(Si,t) × (1 + R(Si)) × Σ[C(Si,Sk) × Me(Si,E_current) × U(Si)]
 */

#ifndef MCEE_MEMORY_MANAGER_HPP
#define MCEE_MEMORY_MANAGER_HPP

#include "Types.hpp"
#include "PhaseConfig.hpp"
#include <vector>
#include <string>
#include <optional>
#include <functional>

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
     * @brief Définit la configuration Neo4j (pour future intégration)
     */
    void setNeo4jConfig(
        const std::string& uri,
        const std::string& user,
        const std::string& password
    );

private:
    // Stockage local des souvenirs (avant intégration Neo4j)
    std::vector<Memory> memories_;
    
    // Configuration Neo4j (future)
    std::string neo4j_uri_;
    std::string neo4j_user_;
    std::string neo4j_password_;
    bool neo4j_connected_ = false;

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
