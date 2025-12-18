# Guide de Résolution des Problèmes de Compilation MCEE

## Problème Principal Identifié

Le fichier `MCEETypes.h` contenait **à la fois les déclarations ET les implémentations complètes** de toutes les classes, causant des redéfinitions multiples lors de la compilation.

## Solution Appliquée

### 1. Restructuration des Headers

- **MCEETypes.h** : Contient uniquement les types, structures et enums
- **MCEEConfig.h** : Déclarations de classe seulement
- **MCEERabbitMQ.h** : Déclarations de classe seulement
- **MCEEGradients.h** : Déclarations de classe seulement
- **MCEEContextualizer.h** : Déclarations de classe seulement
- **MCEECore.h** : Déclarations de classe seulement

### 2. Fichiers d'Implémentation

- **MCEEConfig.cpp** : Implémentations complètes
- **MCEERabbitMQ.cpp** : Version avec SimpleAmqpClient
- **MCEERabbitMQ_stub.cpp** : Version simulation sans dépendances
- **MCEEGradients.cpp** : Implémentations des calculs
- **MCEEContextualizer.cpp** : Implémentations de contextualisation
- **MCEECore.cpp** : Implémentations du moteur principal

## Méthodes de Compilation

### Méthode 1 : Compilation Simple (Recommandée)

```bash
# Rendre le script exécutable
chmod +x build_simple.sh

# Compiler
./build_simple.sh
```

**Avantages :**
- Détecte automatiquement les dépendances manquantes
- Utilise un mode simulation si SimpleAmqpClient n'est pas disponible
- Fonctionne même sans nlohmann/json

### Méthode 2 : CMake

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Méthode 3 : Compilation Manuelle

```bash
# Avec toutes les dépendances
g++ -std=c++20 -Wall -Wextra -Wpedantic -O2 -I. \
    MCEEConfig.cpp MCEEGradients.cpp MCEEContextualizer.cpp \
    MCEECore.cpp MCEERabbitMQ.cpp main.cpp \
    -lSimpleAmqpClient -pthread -o mcee

# Mode simulation (sans SimpleAmqpClient)
g++ -std=c++20 -Wall -Wextra -Wpedantic -O2 -I. \
    -DUSE_RABBITMQ_STUB \
    MCEEConfig.cpp MCEEGradients.cpp MCEEContextualizer.cpp \
    MCEECore.cpp MCEERabbitMQ_stub.cpp main.cpp \
    -pthread -o mcee
```

## Résolution des Dépendances

### Installation des Dépendances Ubuntu/Debian

```bash
# Dépendances de base
sudo apt update
sudo apt install build-essential cmake pkg-config git

# JSON library
sudo apt install nlohmann-json3-dev

# RabbitMQ C library (optionnel)
sudo apt install librabbitmq-dev

# SimpleAmqpClient (optionnel, pour mode production)
git clone https://github.com/alanxz/SimpleAmqpClient.git
cd SimpleAmqpClient
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install
sudo ldconfig
```

## Modes de Fonctionnement

### Mode Simulation (STUB)
- **Quand** : SimpleAmqpClient non disponible
- **Avantage** : Compilation simple, pas de dépendances externes
- **Fonctionnement** :
    - Simule les données d'entrée automatiquement
    - Sauvegarde les sorties dans `output_*.json`
    - Aucun serveur RabbitMQ requis

### Mode Production
- **Quand** : SimpleAmqpClient installé
- **Avantage** : Communication RabbitMQ complète
- **Prérequis** : Serveur RabbitMQ actif

## Test du Build

```bash
# Lancer le programme
./mcee

# Avec fichier de config personnalisé
./mcee /path/to/custom_config.txt
```

### Vérification Mode Simulation

Si vous voyez ces messages au démarrage :
```
Mode: SIMULATION (RabbitMQ stub)
=== MODE SIMULATION ===
• Données d'entrée simulées automatiquement
• Sorties sauvées dans output_*.json
• Aucun serveur RabbitMQ requis
```

Le programme fonctionne en mode simulation et génère des fichiers de sortie.

### Vérification Mode Production

```
Mode: PRODUCTION (RabbitMQ complet)
Connected to RabbitMQ at localhost:5672
```

## Dépannage

### Erreur "redefinition of class"
- **Cause** : Ancien MCEETypes.h avec implémentations
- **Solution** : Utiliser les nouveaux headers séparés

### SimpleAmqpClient non trouvé
- **Solution rapide** : Utiliser `build_simple.sh` (mode simulation)
- **Solution complète** : Installer SimpleAmqpClient

### nlohmann/json non trouvé
- **Solution** : `sudo apt install nlohmann-json3-dev`
- **Alternative** : Le script utilise un JSON simplifié

### Erreurs de linking boost
- **Cause** : Conflit entre boost::shared_ptr et std::shared_ptr
- **Solution** : Les nouveaux fichiers gèrent cette incompatibilité

## Fichiers de Sortie (Mode Simulation)

- `output_mcee.consciousness.output.json` : Émotions contextualisées
- `output_mcee.amygdaleon.output.json` : Signaux d'urgence
- `output_mcee.mlt.output.json` : Demandes de consolidation mémoire

## Performance

Le mode simulation affiche des statistiques toutes les 30 secondes :
```
=== Performance Stats ===
Processed messages: 42
Average processing time: 2.34 ms
=========================
```

## Support

Les nouvelles versions corrigent :
- ✅ Redéfinitions multiples de classes
- ✅ Incompatibilités boost/std smart pointers
- ✅ Dépendances manquantes
- ✅ Compilation sans SimpleAmqpClient
- ✅ Mode de test intégré