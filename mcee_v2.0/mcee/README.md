# MCEE - Modèle Complet d'Évaluation des États

**Version 2.0** | 19 décembre 2025

## Vue d'ensemble

Le MCEE est un système émotionnel complet qui intègre :
- **24 émotions instantanées** (reçues du module C++ emotion)
- **Système de phases émotionnelles** (8 phases qui modulent le comportement)
- **Gestion de la mémoire** (souvenirs, concepts, traumas - préparé pour Neo4j)
- **Mécanismes de fusion et modulation** (adaptatifs selon la phase)
- **Système d'urgence « Amyghaleon »** (déclenché selon la phase)

## Architecture

```
┌─────────────────────┐
│  Module C++ Emotion │ ─► Prédit 24 émotions
└─────────┬───────────┘
          │ RabbitMQ (mcee.emotional.input)
          ▼
┌─────────────────────┐
│   Phase Detector    │ ─► Détecte phase + PhaseConfig
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│    MCEE Engine      │ ─► Mise à jour E_i(t+1) + fusion E_global
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│   Memory Manager    │ ─► Souvenirs, concepts, traumas
└─────────┬───────────┘
          ▼
┌─────────────────────┐
│     Amyghaleon      │ ─► Réactions d'urgence
└─────────────────────┘
```

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
│   ├── Types.hpp
│   ├── PhaseConfig.hpp
│   ├── PhaseDetector.hpp
│   ├── EmotionUpdater.hpp
│   ├── Amyghaleon.hpp
│   ├── MemoryManager.hpp
│   └── MCEEEngine.hpp
└── src/
    ├── main.cpp
    ├── PhaseDetector.cpp
    ├── EmotionUpdater.cpp
    ├── Amyghaleon.cpp
    ├── MemoryManager.cpp
    └── MCEEEngine.cpp
```

## Communication RabbitMQ

### Entrée (depuis module emotion)
- Exchange: `mcee.emotional.input`
- Routing Key: `emotions.predictions`
- Format: JSON avec 24 émotions (0.0 - 1.0)

### Sortie
- Exchange: `mcee.emotional.output`
- Routing Key: `mcee.state`
- Format: JSON avec état complet (émotions, phase, coefficients, stats)

## Exemple de sortie JSON

```json
{
  "emotions": {
    "Admiration": 0.123,
    "Joie": 0.856,
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
  "stats": {
    "phase_transitions": 3,
    "emergency_triggers": 0,
    "wisdom": 0.045
  }
}
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
