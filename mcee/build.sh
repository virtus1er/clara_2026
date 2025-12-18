#!/bin/bash

# Script de compilation MCEE simplifié
echo "=== Compilation MCEE (Mode simplifié) ==="
echo "=========================================="

# Variables
CXX="g++"
CXXFLAGS="-std=c++20 -Wall -Wextra -Wpedantic -O2"
INCLUDES="-I."
LIBS="-pthread"
TARGET="mcee"

# Check for nlohmann/json
echo "Vérification de nlohmann/json..."
if ! echo '#include <nlohmann/json.hpp>' | $CXX -x c++ - -c -o /dev/null 2>/dev/null; then
    echo "❌ nlohmann/json non trouvé!"
    echo "Installation avec: sudo apt install nlohmann-json3-dev"
    echo "Ou compilation sans JSON avancé..."
    USE_SIMPLE_JSON=1
else
    echo "✅ nlohmann/json trouvé"
    USE_SIMPLE_JSON=0
fi

# Check for SimpleAmqpClient
echo "Vérification de SimpleAmqpClient..."
if ! echo '#include <SimpleAmqpClient/SimpleAmqpClient.h>' | $CXX -x c++ - -c -o /dev/null 2>/dev/null; then
    echo "⚠️  SimpleAmqpClient non trouvé - utilisation du mode stub"
    USE_STUB=1
    CXXFLAGS="$CXXFLAGS -DUSE_RABBITMQ_STUB"
else
    echo "✅ SimpleAmqpClient trouvé"
    USE_STUB=0
    LIBS="$LIBS -lSimpleAmqpClient"
fi

# Sources de base
SOURCES="MCEEConfig.cpp MCEEGradients.cpp MCEEContextualizer.cpp MCEECore.cpp main.cpp"

# Ajouter la version RabbitMQ appropriée
if [ $USE_STUB -eq 1 ]; then
    SOURCES="$SOURCES MCEERabbitMQ_stub.cpp"
    echo "📝 Utilisation du stub RabbitMQ (mode simulation)"
else
    SOURCES="$SOURCES MCEERabbitMQ.cpp"
    echo "📡 Utilisation de RabbitMQ réel"
fi

# JSON simple si nécessaire
if [ $USE_SIMPLE_JSON -eq 1 ]; then
    CXXFLAGS="$CXXFLAGS -DUSE_SIMPLE_JSON"
    echo "📄 Mode JSON simplifié"
fi

# Compilation
echo ""
echo "Compilation en cours..."
echo "Commande: $CXX $CXXFLAGS $INCLUDES $SOURCES $LIBS -o $TARGET"
echo ""

$CXX $CXXFLAGS $INCLUDES $SOURCES $LIBS -o $TARGET

# Vérifier le résultat
if [ $? -eq 0 ]; then
    echo ""
    echo "✅ Compilation réussie!"
    echo "Exécutable créé: ./$TARGET"

    # Copier le fichier de configuration
    if [ ! -f "mcee_config.txt" ]; then
        if [ -f "../mcee_config.txt" ]; then
            cp ../mcee_config.txt .
            echo "📋 Configuration copiée"
        else
            echo "⚠️  Fichier mcee_config.txt manquant"
        fi
    fi

    echo ""
    echo "Mode de fonctionnement:"
    if [ $USE_STUB -eq 1 ]; then
        echo "  🔧 SIMULATION - Pas de RabbitMQ réel"
        echo "  📁 Les sorties seront dans output_*.json"
        echo "  🔄 Données d'entrée simulées automatiquement"
    else
        echo "  📡 PRODUCTION - RabbitMQ complet"
        echo "  🐰 Nécessite un serveur RabbitMQ actif"
    fi

    echo ""
    echo "Pour lancer:"
    echo "  ./$TARGET"
    echo ""

else
    echo ""
    echo "❌ Erreur de compilation!"
    echo ""
    echo "Solutions possibles:"
    echo "1. Installer les dépendances:"
    echo "   sudo apt update"
    echo "   sudo apt install build-essential nlohmann-json3-dev"
    echo ""
    echo "2. Ou utiliser CMake:"
    echo "   mkdir build && cd build"
    echo "   cmake .."
    echo "   make"
    echo ""
    exit 1
fi