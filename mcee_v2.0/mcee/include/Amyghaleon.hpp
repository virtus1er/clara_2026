/**
 * @file Amyghaleon.hpp
 * @brief Système d'urgence « Amyghaleon » avec seuils de phase
 * @version 2.0
 * @date 2025-12-19
 * 
 * Le système Amyghaleon détecte les situations d'urgence et déclenche
 * des réponses immédiates (court-circuit du traitement MCEE normal).
 * 
 * Le seuil d'activation dépend de la phase active:
 * - SÉRÉNITÉ: 0.85 (difficile à déclencher)
 * - JOIE: 0.95 (très difficile)
 * - PEUR: 0.50 (très facile - hypersensible)
 */

#ifndef MCEE_AMYGHALEON_HPP
#define MCEE_AMYGHALEON_HPP

#include "Types.hpp"
#include "PhaseConfig.hpp"
#include <functional>
#include <vector>

namespace mcee {

/**
 * @class Amyghaleon
 * @brief Système de détection et réponse aux urgences émotionnelles
 */
class Amyghaleon {
public:
    using EmergencyCallback = std::function<void(const EmergencyResponse&)>;

    /**
     * @brief Constructeur
     */
    Amyghaleon();

    /**
     * @brief Vérifie si une urgence est détectée
     * @param state État émotionnel actuel
     * @param active_memories Souvenirs actuellement activés
     * @param phase_threshold Seuil de la phase active
     * @return true si urgence détectée
     */
    [[nodiscard]] bool checkEmergency(
        const EmotionalState& state,
        const std::vector<Memory>& active_memories,
        double phase_threshold
    ) const;

    /**
     * @brief Déclenche une réponse d'urgence
     * @param state État émotionnel
     * @param phase Phase active lors du déclenchement
     * @return Réponse d'urgence structurée
     */
    [[nodiscard]] EmergencyResponse triggerEmergencyResponse(
        const EmotionalState& state,
        Phase phase
    );

    /**
     * @brief Définit le callback pour les urgences
     * @param callback Fonction appelée lors d'une urgence
     */
    void setEmergencyCallback(EmergencyCallback callback);

    /**
     * @brief Retourne le nombre d'urgences déclenchées
     */
    [[nodiscard]] size_t getEmergencyCount() const { return emergency_count_; }

    /**
     * @brief Réinitialise le compteur d'urgences
     */
    void resetEmergencyCount() { emergency_count_ = 0; }

    /**
     * @brief Vérifie si un trauma spécifique est activé au-dessus du seuil
     * @param memory Souvenir à vérifier
     * @param threshold Seuil d'activation
     * @return true si trauma activé
     */
    [[nodiscard]] bool isTraumaActivated(
        const Memory& memory,
        double threshold
    ) const;

private:
    size_t emergency_count_ = 0;
    EmergencyCallback on_emergency_;

    /**
     * @brief Trouve l'émotion critique la plus élevée
     * @param state État émotionnel
     * @return Paire (nom_emotion, valeur)
     */
    [[nodiscard]] std::pair<std::string, double> findMaxCriticalEmotion(
        const EmotionalState& state
    ) const;

    /**
     * @brief Détermine l'action appropriée selon l'émotion
     * @param emotion_name Nom de l'émotion dominante
     * @return Action recommandée
     */
    [[nodiscard]] std::string determineAction(const std::string& emotion_name) const;

    /**
     * @brief Détermine la priorité de la réponse
     * @param emotion_value Valeur de l'émotion
     * @return Niveau de priorité
     */
    [[nodiscard]] std::string determinePriority(double emotion_value) const;
};

} // namespace mcee

#endif // MCEE_AMYGHALEON_HPP
