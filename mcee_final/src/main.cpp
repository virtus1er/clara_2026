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
              << "  --llm-test            Mode test LLM interactif\n"
              << "\n";
}

void runLLMTest(MCEEEngine& engine) {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║              MODE TEST LLM - Reformulation Émotionnelle       ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n\n";

    // Vérifier que le LLM est prêt
    if (!engine.isLLMReady()) {
        std::cerr << "[LLM Test] ERREUR: LLMClient non disponible.\n";
        std::cerr << "           Vérifiez que OPENAI_API_KEY est défini.\n";
        return;
    }

    std::cout << "[LLM Test] LLMClient prêt. Tapez vos questions (ou 'quit' pour sortir).\n";
    std::cout << "[LLM Test] Les émotions RabbitMQ arrivent en temps réel.\n";
    std::cout << "[LLM Test] Tapez /quiet pour désactiver les logs en arrière-plan.\n";
    std::cout << "[LLM Test] Commandes: /state, /joy, /sad, /calm, /quiet, /help\n\n";

    std::string input;
    while (g_running.load()) {
        std::cout << "\n> ";
        std::getline(std::cin, input);

        if (input.empty()) continue;
        if (input == "quit" || input == "exit" || input == "q") {
            std::cout << "[LLM Test] Au revoir!\n";
            break;
        }

        // Commandes spéciales
        if (input == "/state") {
            auto conscience_state = engine.getConscienceState();
            std::cout << "[État] Ft=" << std::fixed << std::setprecision(2)
                      << conscience_state.sentiment
                      << " Ct=" << conscience_state.consciousness_level
                      << " (" << conscience_state.dominant_state << ")\n";
            continue;
        }

        if (input == "/joy") {
            std::unordered_map<std::string, double> joy_state;
            for (const auto& name : EMOTION_NAMES) joy_state[name] = 0.1;
            joy_state["Joie"] = 0.8;
            joy_state["Excitation"] = 0.6;
            engine.processEmotions(joy_state);
            std::cout << "[État] Passage en mode JOIE\n";
            continue;
        }

        if (input == "/sad") {
            std::unordered_map<std::string, double> sad_state;
            for (const auto& name : EMOTION_NAMES) sad_state[name] = 0.1;
            sad_state["Tristesse"] = 0.7;
            sad_state["Nostalgie"] = 0.5;
            engine.processEmotions(sad_state);
            std::cout << "[État] Passage en mode TRISTESSE\n";
            continue;
        }

        if (input == "/calm") {
            std::unordered_map<std::string, double> calm_state;
            for (const auto& name : EMOTION_NAMES) calm_state[name] = 0.1;
            calm_state["Calme"] = 0.7;
            calm_state["Soulagement"] = 0.4;
            engine.processEmotions(calm_state);
            std::cout << "[État] Passage en mode CALME\n";
            continue;
        }

        if (input == "/quiet") {
            bool current = engine.isQuietMode();
            engine.setQuietMode(!current);
            std::cout << "[Mode] Logs " << (engine.isQuietMode() ? "DÉSACTIVÉS" : "ACTIVÉS") << "\n";
            continue;
        }

        if (input == "/help") {
            std::cout << "Commandes disponibles:\n";
            std::cout << "  /state  - Affiche l'état émotionnel actuel\n";
            std::cout << "  /joy    - Passe en état de joie\n";
            std::cout << "  /sad    - Passe en état de tristesse\n";
            std::cout << "  /calm   - Passe en état calme\n";
            std::cout << "  /quiet  - Active/désactive les logs en arrière-plan\n";
            std::cout << "  /help   - Affiche cette aide\n";
            std::cout << "  quit    - Quitte le mode test\n";
            continue;
        }

        // Traiter le texte pour enrichir le contexte émotionnel
        engine.processSpeechText(input, "user");

        // Générer la réponse LLM
        std::cout << "\n[Génération en cours...]\n";

        auto start_time = std::chrono::steady_clock::now();
        std::string response = engine.generateEmotionalResponse(input);
        auto end_time = std::chrono::steady_clock::now();

        double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

        if (!response.empty()) {
            std::cout << "\n┌─────────────────────────────────────────────────────────────┐\n";
            std::cout << "│ Réponse (" << std::fixed << std::setprecision(0) << elapsed_ms << "ms):\n";
            std::cout << "├─────────────────────────────────────────────────────────────┘\n";
            std::cout << response << "\n";
            std::cout << "└─────────────────────────────────────────────────────────────\n";
        } else {
            std::cout << "[LLM Test] Erreur: pas de réponse générée.\n";
        }
    }
}

void runDemo(MCEEEngine& engine) {
    std::cout << "\n[Demo] Mode démonstration - simulation d'émotions et parole\n\n";

    // Scénario 1: État calme (SÉRÉNITÉ) + texte positif
    std::cout << "═══ Scénario 1: État calme + parole positive ═══\n";
    std::unordered_map<std::string, double> calm_state;
    for (const auto& name : EMOTION_NAMES) {
        calm_state[name] = 0.1;
    }
    calm_state["Calme"] = 0.8;
    calm_state["Satisfaction"] = 0.6;
    calm_state["Soulagement"] = 0.4;
    engine.processEmotions(calm_state);
    engine.processSpeechText("Bonjour, je suis content de te voir. Tout va bien aujourd'hui.", "user");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Scénario 2: Montée de joie (transition JOIE) + texte enthousiaste
    std::cout << "\n═══ Scénario 2: Montée de joie + parole enthousiaste ═══\n";
    std::unordered_map<std::string, double> joy_state;
    for (const auto& name : EMOTION_NAMES) {
        joy_state[name] = 0.1;
    }
    joy_state["Joie"] = 0.85;
    joy_state["Excitation"] = 0.7;
    joy_state["Satisfaction"] = 0.6;
    joy_state["Triomphe"] = 0.5;
    engine.processEmotions(joy_state);
    engine.processSpeechText("C'est fantastique ! J'ai réussi mon examen, je suis tellement heureux !", "user");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Scénario 3: Exploration (curiosité) + texte interrogatif
    std::cout << "\n═══ Scénario 3: Mode exploration + questions ═══\n";
    std::unordered_map<std::string, double> explore_state;
    for (const auto& name : EMOTION_NAMES) {
        explore_state[name] = 0.1;
    }
    explore_state["Intérêt"] = 0.8;
    explore_state["Fascination"] = 0.75;
    explore_state["Excitation"] = 0.5;
    explore_state["Émerveillement"] = 0.6;
    engine.processEmotions(explore_state);
    engine.processSpeechText("Comment ça fonctionne ? C'est vraiment incroyable ce système !", "user");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Scénario 4: Anxiété croissante + texte stressant
    std::cout << "\n═══ Scénario 4: Anxiété + parole inquiète ═══\n";
    std::unordered_map<std::string, double> anxiety_state;
    for (const auto& name : EMOTION_NAMES) {
        anxiety_state[name] = 0.1;
    }
    anxiety_state["Anxiété"] = 0.7;
    anxiety_state["Confusion"] = 0.5;
    anxiety_state["Peur"] = 0.3;
    engine.processEmotions(anxiety_state);
    engine.processSpeechText("Je suis inquiet, j'ai un mauvais pressentiment. Quelque chose ne va pas.", "user");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Scénario 5: URGENCE - Peur intense + texte de danger
    std::cout << "\n═══ Scénario 5: ⚠ URGENCE - Peur + alerte danger ═══\n";
    std::unordered_map<std::string, double> fear_state;
    for (const auto& name : EMOTION_NAMES) {
        fear_state[name] = 0.05;
    }
    fear_state["Peur"] = 0.9;
    fear_state["Horreur"] = 0.7;
    fear_state["Anxiété"] = 0.8;
    engine.processEmotions(fear_state);
    engine.processSpeechText("Attention danger ! Il faut fuir immédiatement, c'est une urgence !", "user");
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Scénario 6: Retour au calme + texte rassurant
    std::cout << "\n═══ Scénario 6: Retour au calme + parole apaisante ═══\n";
    std::unordered_map<std::string, double> recovery_state;
    for (const auto& name : EMOTION_NAMES) {
        recovery_state[name] = 0.1;
    }
    recovery_state["Soulagement"] = 0.7;
    recovery_state["Calme"] = 0.5;
    recovery_state["Peur"] = 0.2;
    engine.processEmotions(recovery_state);
    engine.processSpeechText("C'est fini, tout va bien maintenant. On peut se détendre.", "user");
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
    std::cout << "  Textes traités       : " << engine.getSpeechInput().getProcessedCount() << "\n";
    std::cout << "  Sentiment moyen      : " << std::fixed << std::setprecision(2) 
              << engine.getSpeechInput().getAverageSentiment() << "\n";
    std::cout << "  Sagesse accumulée    : " << std::fixed << std::setprecision(3) 
              << stats.wisdom << "\n\n";
}

int main(int argc, char* argv[]) {
    // Configuration par défaut
    RabbitMQConfig config;
    std::string config_file = "phase_config.json";
    bool demo_mode = false;
    bool llm_test_mode = false;

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
        } else if (arg == "--llm-test") {
            llm_test_mode = true;
        }
    }

    // Installer le signal handler
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        // Créer le moteur MCEE
        MCEEEngine engine(config);

        // Charger la configuration de base (SANS Neo4j pour ne pas bloquer)
        std::cout << "[Main] Chargement configuration (phase)..." << std::endl << std::flush;
        if (std::ifstream(config_file).good()) {
            engine.loadConfig(config_file, true);  // skip_neo4j = true pour l'instant
            std::cout << "[Main] Configuration de base chargée" << std::endl << std::flush;
        } else {
            std::cout << "[Main] Fichier config non trouvé, utilisation des valeurs par défaut" << std::endl;
        }

        // Définir un callback pour afficher les changements d'état
        engine.setStateCallback([](const EmotionalState& state, const std::string& pattern_name) {
            // Le callback est appelé à chaque mise à jour
        });

        if (demo_mode) {
            // Mode démonstration
            runDemo(engine);
        } else if (llm_test_mode) {
            // Mode test LLM interactif AVEC consommation RabbitMQ
            std::cout << "[Main] Démarrage du moteur (RabbitMQ + LLM Test)..." << std::endl << std::flush;

            if (!engine.start()) {
                std::cerr << "[Main] Échec du démarrage du moteur MCEE" << std::endl;
                return 1;
            }

            std::cout << "[Main] MCEE actif - les émotions RabbitMQ arrivent en temps réel" << std::endl;

            // Initialiser Neo4j en arrière-plan
            std::cout << "[Main] Initialisation Neo4j..." << std::endl << std::flush;
            engine.loadConfig(config_file, false);

            // Lancer le test LLM interactif
            runLLMTest(engine);

            engine.stop();
        } else {
            // Mode normal avec RabbitMQ
            // IMPORTANT: Démarrer le consumer RabbitMQ EN PREMIER
            std::cout << "[Main] Démarrage du moteur (RabbitMQ)..." << std::endl << std::flush;

            if (!engine.start()) {
                std::cerr << "[Main] Échec du démarrage du moteur MCEE" << std::endl;
                return 1;
            }

            std::cout << "[Main] MCEE actif et en écoute RabbitMQ" << std::endl;

            // ENSUITE initialiser Neo4j (peut bloquer, mais MCEE consomme déjà)
            std::cout << "[Main] Initialisation Neo4j en arrière-plan..." << std::endl << std::flush;
            engine.loadConfig(config_file, false);  // Maintenant charger Neo4j
            std::cout << "[Main] Neo4j initialisé" << std::endl << std::flush;

            std::cout << "[Main] MCEE prêt. Appuyez sur Ctrl+C pour arrêter." << std::endl;

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
