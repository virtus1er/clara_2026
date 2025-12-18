//===== MCEETypes.h - SEULEMENT LES TYPES ET DECLARATIONS =====
#pragma once
#include <array>
#include <string>
#include <chrono>
#include <map>
#include <vector>

namespace MCEE {
    using EmotionArray = std::array<double, 24>;
    using Timestamp = std::chrono::time_point<std::chrono::steady_clock>;

    enum class DangerLevel {
        NORMAL,      // [0.0, 0.3[
        SURVEILLANCE,// [0.3, 0.6[
        ALERTE,      // [0.6, 0.8[
        CRITIQUE,    // [0.8, 0.95[
        URGENCE      // [0.95, 1.0]
    };

    enum class Priority {
        LOW,
        NORMAL,
        HIGH,
        CRITIQUE
    };

    struct PhysicalSensors {
        double temperature_ambiante = 0.0;  // 0.0=très froid → 1.0=très chaud
        double volume_sonore = 0.0;         // 0.0=silence → 1.0=très bruyant
        double luminosite = 0.0;            // 0.0=obscurité → 1.0=très lumineux
        double gyroscope_stabilite = 0.0;   // 0.0=stable → 1.0=très instable
    };

    struct TechnicalStates {
        double temperature_cpu = 0.0;       // En °C
        double temperature_gpu = 0.0;       // En °C
        double charge_cpu = 0.0;            // 0.0-1.0
        double utilisation_ram = 0.0;       // 0.0-1.0
        double stabilite_systeme = 1.0;     // 0.0-1.0
    };

    struct ExternalFeedbacks {
        bool validation_positive = false;
        bool encouragement_recu = false;
        bool alerte_externe = false;
        bool interaction_sociale = false;
    };

    struct ContextData {
        PhysicalSensors capteurs_physiques;
        TechnicalStates etats_internes;
        ExternalFeedbacks feedbacks_externes;
        Timestamp timestamp;
    };

    struct EmotionalInput {
        EmotionArray emotions_brutes;
        double intensite_globale = 0.0;
        std::vector<std::string> emotions_dominantes;
        Timestamp timestamp;
        std::string text_id;
    };

    struct ContextualizedEmotions {
        EmotionArray emotions_contextualisees;
        double emotion_globale = 0.0;
        std::string contexte_detecte;
        double confiance_contexte = 0.0;
        double gradient_danger_global = 0.0;
        DangerLevel niveau_danger = DangerLevel::NORMAL;
        bool signal_amyghaleon = false;
        bool souvenir_a_consolider = false;
        Priority priorite_mlt = Priority::NORMAL;
        Timestamp timestamp;
        std::string text_id;
    };

    struct AmygdaleonSignal {
        bool urgence = false;
        DangerLevel niveau_danger = DangerLevel::NORMAL;
        double gradient_danger_global = 0.0;
        std::string contexte_detecte;
        std::vector<std::string> emotions_critiques;
        std::map<std::string, double> gradients_declencheurs;
        std::string recommandation_intervention;
        Timestamp timestamp;
        std::string text_id;
    };

    struct MemoryToConsolidate {
        std::string id_mcee;
        std::string statut = "EN_ATTENTE_CONSOLIDATION";
        Priority priorite = Priority::NORMAL;
        EmotionArray emotions_brutes;
        EmotionArray emotions_contextualisees;
        std::string contexte_detecte;
        ContextData gradients_complets;
        double score_significativite = 0.0;
        std::string recommandation_traitement;
        Timestamp timestamp;
    };

    struct MCEEParameters {
        // Coefficients de modulation émotionnelle
        double alpha_feedbacks = 0.25;
        double beta_etats_techniques = 0.15;
        double gamma_capteurs_physiques = 0.20;
        double delta_souvenirs_mct = 0.35;
        double epsilon_transition_contexte = 0.20;
        double eta_gradient_danger = 0.30;

        // Coefficients gradients environnementaux
        double omega1_gyroscope = 0.4;
        double omega2_volume = 0.3;
        double omega3_temperature = 0.2;
        double omega4_orientation = 0.1;

        // Coefficients stress système
        double sigma1_charge_cpu = 0.3;
        double sigma2_utilisation_ram = 0.25;
        double sigma3_temperature_critique = 0.35;
        double sigma4_stabilite_systeme = 0.1;

        // Coefficients gradient danger global
        double poids_environnemental = 0.4;
        double poids_stress_systeme = 0.3;
        double poids_trauma_historique = 0.2;
        double poids_instabilite_emotionnelle = 0.1;

        // Seuils adaptatifs
        double seuil_amyghaleon = 0.85;
        double seuil_mlt_base = 0.65;
        double seuil_variation_critique = 0.7;

        // Seuils de danger
        double seuil_normal_max = 0.3;
        double seuil_surveillance_max = 0.6;
        double seuil_alerte_max = 0.8;
        double seuil_critique_max = 0.95;

        // Performance
        int frequence_maj_hz = 100;
        int latence_max_ms = 10;
        double charge_cpu_max = 0.15;

        // RabbitMQ Configuration
        std::string rabbitmq_host = "localhost";
        int rabbitmq_port = 5672;
        std::string rabbitmq_username = "virtus";
        std::string rabbitmq_password = "virtus@83";
        std::string queue_emotional_input = "mcee.emotional.input";
        std::string queue_context_input = "mcee.context.input";
        std::string queue_consciousness_output = "mcee.consciousness.output";
        std::string queue_amygdaleon_output = "mcee.amygdaleon.output";
        std::string queue_mlt_output = "mcee.mlt.output";
    };
}