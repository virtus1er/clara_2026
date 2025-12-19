/**
 * @file main.cpp
 * @brief Point d'entrée du module MCEE v2.0
 * @version 2.0
 * @date 2025-12-19
 * 
 * Le MCEE (Modèle Complet d'Évaluation des États) reçoit les 24 émotions
 * du module C++ emotion via RabbitMQ, les traite avec le système de phases,
 * et publie l'état émotionnel complet.
 */

#include "MCEEEngine.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <fstream>

using namespace mcee;

// Signal handler pour arrêt propre
std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    std::cout << "\n[Main] Signal " << signal << " reçu, arrêt en cours...\n";
    g_running.store(false);
}

void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n"
              << "Options:\n"
              << "  -h, --help            Affiche cette aide\n"
              << "  -c, --config <file>   Fichier de configuration JSON\n"
              << "  --host <host>         Hôte RabbitMQ (défaut: localhost)\n"
              << "  --port <port>         Port RabbitMQ (défaut: 5672)\n"
              << "  --user <user>         Utilisateur RabbitMQ (défaut: virtus)\n"
              << "  --pass <password>     Mot de passe RabbitMQ\n"
              << "  --demo                Mode démonstration (sans RabbitMQ)\n"
              << "\n";
}

void runDemo(MCEEEngine& engine) {
    std::cout << "\n[Demo] Mode démonstration - simulation d'émotions\n\n";

    // Scénario 1: État calme (SÉRÉNITÉ)
    std::cout << "═══ Scénario 1: État calme ═══\n";
    std::unordered_map<std::string, double> calm_state;
    for (const auto& name : EMOTION_NAMES) {
        calm_state[name] = 0.1;
    }
    calm_state["Calme"] = 0.8;
    calm_state["Satisfaction"] = 0.6;
    calm_state["Soulagement"] = 0.4;
    engine.processEmotions(calm_state);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Scénario 2: Montée de joie (transition JOIE)
    std::cout << "\n═══ Scénario 2: Montée de joie ═══\n";
    std::unordered_map<std::string, double> joy_state;
    for (const auto& name : EMOTION_NAMES) {
        joy_state[name] = 0.1;
    }
    joy_state["Joie"] = 0.85;
    joy_state["Excitation"] = 0.7;
    joy_state["Satisfaction"] = 0.6;
    joy_state["Triomphe"] = 0.5;
    engine.processEmotions(joy_state);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Scénario 3: Exploration (curiosité)
    std::cout << "\n═══ Scénario 3: Mode exploration ═══\n";
    std::unordered_map<std::string, double> explore_state;
    for (const auto& name : EMOTION_NAMES) {
        explore_state[name] = 0.1;
    }
    explore_state["Intérêt"] = 0.8;
    explore_state["Fascination"] = 0.75;
    explore_state["Excitation"] = 0.5;
    explore_state["Émerveillement"] = 0.6;
    engine.processEmotions(explore_state);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Scénario 4: Anxiété croissante
    std::cout << "\n═══ Scénario 4: Anxiété croissante ═══\n";
    std::unordered_map<std::string, double> anxiety_state;
    for (const auto& name : EMOTION_NAMES) {
        anxiety_state[name] = 0.1;
    }
    anxiety_state["Anxiété"] = 0.7;
    anxiety_state["Confusion"] = 0.5;
    anxiety_state["Peur"] = 0.3;
    engine.processEmotions(anxiety_state);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Scénario 5: URGENCE - Peur intense
    std::cout << "\n═══ Scénario 5: ⚠ URGENCE - Peur intense ═══\n";
    std::unordered_map<std::string, double> fear_state;
    for (const auto& name : EMOTION_NAMES) {
        fear_state[name] = 0.05;
    }
    fear_state["Peur"] = 0.9;
    fear_state["Horreur"] = 0.7;
    fear_state["Anxiété"] = 0.8;
    engine.processEmotions(fear_state);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Scénario 6: Retour au calme
    std::cout << "\n═══ Scénario 6: Retour progressif au calme ═══\n";
    std::unordered_map<std::string, double> recovery_state;
    for (const auto& name : EMOTION_NAMES) {
        recovery_state[name] = 0.1;
    }
    recovery_state["Soulagement"] = 0.7;
    recovery_state["Calme"] = 0.5;
    recovery_state["Peur"] = 0.2;
    engine.processEmotions(recovery_state);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Statistiques finales
    std::cout << "\n╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                    STATISTIQUES DEMO                          ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    const auto& stats = engine.getStats();
    std::cout << "  Transitions de phase : " << stats.phase_transitions << "\n";
    std::cout << "  Urgences déclenchées : " << stats.emergency_triggers << "\n";
    std::cout << "  Souvenirs créés      : " << engine.getMemoryManager().getMemoryCount() << "\n";
    std::cout << "  Traumas              : " << engine.getMemoryManager().getTraumaCount() << "\n";
    std::cout << "  Sagesse accumulée    : " << std::fixed << std::setprecision(3) 
              << stats.wisdom << "\n\n";
}

int main(int argc, char* argv[]) {
    // Configuration par défaut
    RabbitMQConfig config;
    std::string config_file = "phase_config.json";
    bool demo_mode = false;

    // Parser les arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                config_file = argv[++i];
            }
        } else if (arg == "--host") {
            if (i + 1 < argc) {
                config.host = argv[++i];
            }
        } else if (arg == "--port") {
            if (i + 1 < argc) {
                config.port = std::stoi(argv[++i]);
            }
        } else if (arg == "--user") {
            if (i + 1 < argc) {
                config.user = argv[++i];
            }
        } else if (arg == "--pass") {
            if (i + 1 < argc) {
                config.password = argv[++i];
            }
        } else if (arg == "--demo") {
            demo_mode = true;
        }
    }

    // Installer le signal handler
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        // Créer le moteur MCEE
        MCEEEngine engine(config);

        // Charger la configuration
        if (std::ifstream(config_file).good()) {
            engine.loadConfig(config_file);
        } else {
            std::cout << "[Main] Fichier config non trouvé, utilisation des valeurs par défaut\n";
        }

        // Définir un callback pour afficher les changements d'état
        engine.setStateCallback([](const EmotionalState& state, Phase phase) {
            // Le callback est appelé à chaque mise à jour
            // On peut ajouter du logging ou des actions ici
        });

        if (demo_mode) {
            // Mode démonstration
            runDemo(engine);
        } else {
            // Mode normal avec RabbitMQ
            if (!engine.start()) {
                std::cerr << "[Main] Échec du démarrage du moteur MCEE\n";
                return 1;
            }

            std::cout << "[Main] MCEE en cours d'exécution. Appuyez sur Ctrl+C pour arrêter.\n";

            // Boucle principale
            while (g_running.load() && engine.isRunning()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            engine.stop();
        }

        std::cout << "[Main] MCEE terminé proprement.\n";
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "[Main] Erreur fatale: " << e.what() << "\n";
        return 1;
    }
}
