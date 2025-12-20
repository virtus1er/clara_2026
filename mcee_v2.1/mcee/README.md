# MCEE - Modèle Complet d'Évaluation des États

**Version 2.0** | 19 décembre 2025

## Vue d'ensemble

Le MCEE est un système émotionnel complet qui intègre :
- **24 émotions instantanées** (reçues du module C++ emotion)
- **Entrées textuelles** (reçues du module de parole)
- **Système de phases émotionnelles** (8 phases qui modulent le comportement)
- **Gestion de la mémoire** (souvenirs, concepts, traumas - préparé pour Neo4j)
- **Mécanismes de fusion et modulation** (adaptatifs selon la phase)
- **Système d'urgence « Amyghaleon »** (déclenché selon la phase)

## Architecture

```
┌─────────────────────┐     ┌─────────────────────┐
│  Module C++ Emotion │     │  Module Parole      │
│  (24 émotions)      │     │  (texte transcrit)  │
└─────────┬───────────┘     └─────────┬───────────┘
          │                           │
          │ RabbitMQ                  │ RabbitMQ
          │ mcee.emotional.input      │ mcee.speech.input
          │                           │
          ▼                           ▼
┌─────────────────────────────────────────────────┐
│                  MCEE Engine                     │
├─────────────────────────────────────────────────┤
│  ┌─────────────┐    ┌─────────────────────┐     │
│  │   Phase     │    │   Speech Input      │     │
│  │   Detector  │    │   (analyse texte)   │     │
│  └──────┬──────┘    └──────────┬──────────┘     │
│         │                      │                 │
│         ▼                      ▼                 │
│  ┌─────────────────────────────────────────┐    │
│  │         Emotion Updater                  │    │
│  │   E_i(t+1) = E_i(t) + α·Fb_ext + ...    │    │
│  └──────────────────┬──────────────────────┘    │
│                     │                            │
│         ┌───────────┴───────────┐               │
│         ▼                       ▼               │
│  ┌─────────────┐         ┌─────────────┐        │
│  │  Memory     │         │  Amyghaleon │        │
│  │  Manager    │         │  (urgences) │        │
│  └─────────────┘         └─────────────┘        │
└─────────────────────────────────────────────────┘
          │
          │ RabbitMQ
          │ mcee.emotional.output
          ▼
    [État émotionnel complet]
```

## Flux de données

1. **Module Emotion C++** → Envoie 24 émotions via RabbitMQ
2. **Module Parole** → Envoie le texte transcrit via RabbitMQ
3. **Phase Detector** → Détecte la phase émotionnelle actuelle
4. **Speech Input** → Analyse le texte (sentiment, arousal, urgence)
5. **Emotion Updater** → Met à jour les émotions avec tous les feedbacks
6. **Memory Manager** → Gère les souvenirs et leur influence
7. **Amyghaleon** → Vérifie les conditions d'urgence
8. **Sortie** → Publie l'état émotionnel complet

## Les 8 Phases Émotionnelles

| Phase | α | δ | γ | θ | Seuil A. | Comportement |
|-------|------|------|------|------|----------|--------------|
| SÉRÉNITÉ | 0.25 | 0.30 | 0.12 | 0.10 | 0.85 | Équilibre, apprentissage optimal |
| JOIE | 0.40 | 0.35 | 0.08 | 0.05 | 0.95 | Euphorie, renforcement positif |
| EXPLORATION | 0.35 | 0.25 | 0.10 | 0.15 | 0.80 | Apprentissage maximal |
| ANXIÉTÉ | 0.40 | 0.45 | 0.06 | 0.08 | 0.70 | Hypervigilance, biais négatif |
| **PEUR** △! | 0.60 | 0.70 | 0.02 | 0.02 | **0.50** | URGENCE - Traumas dominants |
| TRISTESSE | 0.20 | 0.55 | 0.05 | 0.12 | 0.90 | Rumination, introspection |
| DÉGOÛT | 0.50 | 0.40 | 0.08 | 0.08 | 0.75 | Évitement, associations négatives |
| CONFUSION | 0.35 | 0.50 | 0.15 | 0.15 | 0.80 | Recherche d'info, incertitude |

## Formules Clés

### Mise à jour des émotions
```
E_i(t+1) = E_i(t) + α·Fb_ext + β·Fb_int - γ·Δt + δ·IS + θ·Wt
```

### Fusion globale
```
E_global(t+1) = tanh(E_global(t) + Σ[E_i(t+1) × (1 - Var_global)])
```

### Activation des souvenirs
```
A(Si) = forget × (1 + R) × Σ[C × Me × U]
```

## Prérequis

- CMake 3.14+
- Compilateur C++20 (GCC 10+, Clang 12+)
- Boost (system, thread)
- RabbitMQ + SimpleAmqpClient
- nlohmann/json (téléchargé automatiquement via FetchContent)

## Compilation

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Utilisation

### Mode normal (avec RabbitMQ)
```bash
./mcee
```

### Mode démonstration
```bash
./mcee --demo
```

### Options
```
./mcee [options]
  -h, --help            Affiche l'aide
  -c, --config <file>   Fichier de configuration JSON
  --host <host>         Hôte RabbitMQ (défaut: localhost)
  --port <port>         Port RabbitMQ (défaut: 5672)
  --user <user>         Utilisateur RabbitMQ (défaut: virtus)
  --pass <password>     Mot de passe RabbitMQ
  --demo                Mode démonstration
```

## Configuration

Le fichier `config/phase_config.json` permet de personnaliser :
- Les coefficients de chaque phase (α, β, γ, δ, θ)
- Les seuils Amyghaleon
- Les paramètres du détecteur de phase
- La configuration RabbitMQ
- Les paramètres de mémoire

## Structure du projet

```
mcee/
├── CMakeLists.txt
├── README.md
├── config/
│   └── phase_config.json
├── include/
│   ├── Types.hpp           # Types et structures de données
│   ├── PhaseConfig.hpp     # Configurations des 8 phases
│   ├── PhaseDetector.hpp   # Détecteur de phases émotionnelles
│   ├── EmotionUpdater.hpp  # Mise à jour des émotions (formule MCEE)
│   ├── Amyghaleon.hpp      # Système d'urgence
│   ├── MemoryManager.hpp   # Gestionnaire de mémoire
│   ├── SpeechInput.hpp     # Analyse des textes de parole
│   └── MCEEEngine.hpp      # Moteur principal
└── src/
    ├── main.cpp
    ├── PhaseDetector.cpp
    ├── EmotionUpdater.cpp
    ├── Amyghaleon.cpp
    ├── MemoryManager.cpp
    ├── SpeechInput.cpp
    └── MCEEEngine.cpp
```

## Communication RabbitMQ

### Entrée Émotions (depuis module emotion C++)
- Exchange: `mcee.emotional.input`
- Routing Key: `emotions.predictions`
- Format JSON:
```json
{
  "Admiration": 0.123,
  "Joie": 0.856,
  "Peur": 0.045,
  ...
}
```

### Entrée Parole (depuis module speech)
- Exchange: `mcee.speech.input`
- Routing Key: `speech.text`
- Format JSON:
```json
{
  "text": "Bonjour, je suis content de te voir",
  "source": "user",
  "confidence": 0.95
}
```

### Sortie État MCEE
- Exchange: `mcee.emotional.output`
- Routing Key: `mcee.state`
- Format: JSON avec état complet (émotions, phase, coefficients, stats)

## Exemple de sortie JSON

```json
{
  "emotions": {
    "Admiration": 0.123,
    "Joie": 0.856,
    "Peur": 0.045,
    ...
  },
  "E_global": 0.542,
  "variance_global": 0.087,
  "valence": 0.723,
  "intensity": 0.456,
  "dominant": "Joie",
  "dominant_value": 0.856,
  "phase": "JOIE",
  "phase_duration": 45.2,
  "coefficients": {
    "alpha": 0.40,
    "beta": 0.25,
    "gamma": 0.08,
    "delta": 0.35,
    "theta": 0.05
  },
  "speech": {
    "last_sentiment": 0.65,
    "last_arousal": 0.45,
    "texts_processed": 12
  },
  "stats": {
    "phase_transitions": 3,
    "emergency_triggers": 0,
    "wisdom": 0.045
  }
}
```

## Module SpeechInput

Le module `SpeechInput` analyse les textes reçus pour :

1. **Analyse de sentiment** : Score de -1 (négatif) à +1 (positif)
2. **Analyse d'arousal** : Niveau d'activation de 0 (calme) à 1 (excité)
3. **Détection de menaces** : Mots-clés de danger (peur, mort, urgence...)
4. **Extraction de mots-clés** : Mots significatifs pour le contexte
5. **Score d'urgence** : Combinaison menace + arousal + sentiment négatif

Le feedback externe (`Fb_ext`) est calculé à partir de cette analyse :
```
Fb_ext = sentiment × (1 + arousal × 0.5) + bonus_menace + bonus_positif
```

## Roadmap

- [x] Phase 1: Base avec Phases
- [x] Phase 2: Mémoire modulée (local)
- [ ] Phase 3: Intégration Neo4j
- [ ] Phase 4: Amyghaleon adaptatif avancé
- [ ] Phase 5: Module Rêve

## Licence

Projet propriétaire - Virtus AI

---

*MCEE v2.0 - Système de phases émotionnelles*
