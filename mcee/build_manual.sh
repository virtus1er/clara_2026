#!/bin/bash

# Compilation manuelle MCEE avec G++
echo "=== Compilation manuelle MCEE avec G++ ==="

# Vérifier les dépendances installées
echo "Vérification des dépendances..."
pkg-config --exists librabbitmq || { echo "librabbitmq non trouvé!"; exit 1; }

# Variables de compilation
CXX="g++"
CXXFLAGS="-std=c++20 -Wall -Wextra -Wpedantic -O2"
INCLUDES="-I. -I/usr/local/include $(pkg-config --cflags librabbitmq)"
LIBS="$(pkg-config --libs librabbitmq) -lSimpleAmqpClient -pthread"

# Sources à compiler
SOURCES="MCEEConfig.cpp MCEERabbitMQ.cpp MCEEGradients.cpp MCEEContextualizer.cpp MCEECore.cpp main.cpp"

# Commande de compilation complète
echo "Compilation en cours..."
$CXX $CXXFLAGS $INCLUDES $SOURCES $LIBS -o mcee

# Vérifier le succès
if [ $? -eq 0 ]; then
    echo "✅ Compilation réussie!"
    echo "Exécutable créé: ./mcee"

    # Copier le fichier de config si nécessaire
    if [ ! -f "mcee_config.txt" ] && [ -f "../mcee_config.txt" ]; then
        cp ../mcee_config.txt .
        echo "Fichier de configuration copié"
    fi

    echo ""
    echo "Pour lancer:"
    echo "./mcee"
else
    echo "❌ Erreur de compilation!"
    exit 1
fi