//===== main.cpp - Version corrigée =====
#include "MCEECore.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

int main(int argc, char* argv[]) {
    std::cout << "=== MCEE (Modulation Contextuelle des Émotions Expérientielles) ===" << std::endl;
    std::cout << "Version: 1.0.0" << std::endl;
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << std::endl;

    #ifdef USE_RABBITMQ_STUB
    std::cout << "Mode: SIMULATION (RabbitMQ stub)" << std::endl;
    #else
    std::cout << "Mode: PRODUCTION (RabbitMQ complet)" << std::endl;
    #endif

    std::cout << "=================================================================" << std::endl;

    // Setup signal handling
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Determine config file path
    std::string config_path = "mcee_config.txt";
    if (argc > 1) {
        config_path = argv[1];
    }

    std::cout << "Using config file: " << config_path << std::endl;

    // Create and initialize MCEE Core
    MCEE::MCEECore core;

    if (!core.initialize(config_path)) {
        std::cerr << "Failed to initialize MCEE Core" << std::endl;
        return 1;
    }

    // Start the core
    core.start();

    std::cout << "MCEE Core is running. Press Ctrl+C to stop." << std::endl;

    #ifdef USE_RABBITMQ_STUB
    std::cout << "" << std::endl;
    std::cout << "=== MODE SIMULATION ===" << std::endl;
    std::cout << "• Données d'entrée simulées automatiquement" << std::endl;
    std::cout << "• Sorties sauvées dans output_*.json" << std::endl;
    std::cout << "• Aucun serveur RabbitMQ requis" << std::endl;
    std::cout << "=======================" << std::endl;
    #endif

    // Main loop - monitor performance and wait for shutdown
    auto last_stats_time = std::chrono::steady_clock::now();
    const auto stats_interval = std::chrono::seconds(30);

    while (g_running && core.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        auto now = std::chrono::steady_clock::now();
        if (now - last_stats_time >= stats_interval) {
            // Print performance statistics
            std::cout << "=== Performance Stats ===" << std::endl;
            std::cout << "Processed messages: " << core.getProcessedMessagesCount() << std::endl;
            std::cout << "Average processing time: " << core.getAverageProcessingTime()
                      << " ms" << std::endl;
            std::cout << "=========================" << std::endl;

            last_stats_time = now;
        }
    }

    // Graceful shutdown
    std::cout << "Stopping MCEE Core..." << std::endl;
    core.stop();

    std::cout << "MCEE Core stopped successfully" << std::endl;
    return 0;
}