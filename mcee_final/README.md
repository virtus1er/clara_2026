# MCEE - Modèle Complet d'Évaluation des États

## Version 3.0 - Architecture MCT/MLT avec Patterns Dynamiques

Le MCEE est un système avancé de traitement émotionnel en temps réel. La version 3.0 introduit une architecture révolutionnaire basée sur des **patterns émotionnels dynamiques** qui remplacent les phases fixes.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          MCEE v3.0 Architecture                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   ┌──────────────┐     ┌──────────────┐     ┌──────────────┐               │
│   │   Émotions   │────▶│     MCT      │────▶│   Pattern    │               │
│   │   (24 dim)   │     │  (Buffer)    │     │   Matcher    │               │
│   └──────────────┘     └──────────────┘     └──────┬───────┘               │
│                              │                      │                       │
│   ┌──────────────┐           │                      │                       │
│   │   Parole     │───────────┘                      ▼                       │
│   │  (Texte)     │                          ┌──────────────┐               │
│   └──────────────┘                          │     MLT      │               │
│                                             │  (Patterns)  │               │
│                                             └──────┬───────┘               │
│                                                    │                       │
│                          ┌─────────────────────────┴──────────────┐        │
│                          │                                        │        │
│                          ▼                                        ▼        │
│                   ┌──────────────┐                        ┌──────────────┐ │
│                   │  Coefficients │                        │   Pattern    │ │
│                   │  Dynamiques   │                        │   Lifecycle  │ │
│                   │  (α,β,γ,δ,θ)  │                        │  (Create/    │ │
│                   └──────┬───────┘                        │   Merge/     │ │
│                          │                                │   Decay)     │ │
│                          ▼                                └──────────────┘ │
│                   ┌──────────────┐                                         │
│                   │   Emotion    │                                         │
│                   │   Updater    │──────▶ État Émotionnel Traité          │
│                   └──────────────┘                                         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Nouveautés v3.0

### 🧠 Patterns Dynamiques (remplace les 8 phases fixes)
- **Création automatique** : Nouveaux patterns émergent de l'expérience
- **Apprentissage** : Les coefficients évoluent avec le feedback
- **Fusion** : Patterns similaires fusionnent automatiquement
- **Déclin** : Patterns non utilisés s'effacent progressivement

### 💾 Système de Mémoire MCT/MLT
- **MCT (Mémoire Court Terme)** : Buffer glissant des 30 dernières secondes
- **MLT (Mémoire Long Terme)** : Stockage persistant des patterns appris
- **PatternMatcher** : Identification et création intelligente de patterns

### 🎯 Coefficients Adaptatifs
Chaque pattern a ses propres coefficients qui s'adaptent :
- `α (alpha)` : Poids des émotions dominantes
- `β (beta)` : Poids de la mémoire
- `γ (gamma)` : Poids du feedback externe
- `δ (delta)` : Poids de l'environnement
- `θ (theta)` : Poids de l'état précédent (inertie)

## Architecture des Composants

### MCT - Mémoire Court Terme
```cpp
MCTConfig config;
config.max_size = 60;              // 60 états max
config.time_window_seconds = 30.0; // Fenêtre de 30s
config.decay_factor = 0.95;        // Décroissance temporelle

// Méthodes principales
mct->push(state);                  // Ajouter un état
mct->extractSignature();           // Obtenir la signature 24D
mct->getStability();               // Stabilité [0, 1]
mct->getTrend();                   // Tendance [-1, +1]
```

### MLT - Mémoire Long Terme
```cpp
// 8 patterns de base initialisés automatiquement :
// SERENITE, JOIE, EXPLORATION, ANXIETE, PEUR, TRISTESSE, DEGOUT, CONFUSION

// Nouveaux patterns créés dynamiquement :
std::string id = mlt->createPattern(signature, "CUSTOM_PATTERN");

// Mise à jour par apprentissage
mlt->updatePattern(id, signature, feedback);

// Fusion automatique de patterns similaires
mlt->autoMerge();

// Nettoyage des patterns obsolètes
mlt->prune();
```

### PatternMatcher
```cpp
// Matching automatique MCT → MLT
MatchResult match = pattern_matcher->match();

// Le résultat contient :
match.pattern_id          // ID unique
match.pattern_name        // Nom lisible (ex: "JOIE")
match.similarity          // Similarité cosinus [0, 1]
match.confidence          // Confiance du pattern [0, 1]
match.alpha, beta, ...    // Coefficients à utiliser
match.is_new_pattern      // true si pattern nouvellement créé
match.is_transition       // true si changement de pattern
```

## Pipeline de Traitement v3.0

```
1. Réception émotions brutes (24 dimensions)
       │
2. Push vers MCT (buffer temporel)
       │
3. Extraction signature MCT (moyenne pondérée + métriques)
       │
4. PatternMatcher : Comparaison avec MLT
       │
       ├── Similarité > 0.85 → Utiliser pattern existant
       ├── Similarité 0.6-0.85 → Modifier pattern
       └── Similarité < 0.6 → Créer nouveau pattern
       │
5. Application coefficients du pattern
       │
6. EmotionUpdater avec coefficients dynamiques
       │
7. Consolidation MLT (si significatif)
       │
8. Publication état via RabbitMQ
```

## Configuration

### RabbitMQ
```json
{
  "host": "localhost",
  "port": 5672,
  "user": "virtus",
  "password": "virtus@83",
  "emotions_exchange": "mcee.emotional.input",
  "speech_exchange": "mcee.speech.input",
  "output_exchange": "mcee.emotional.output"
}
```

### Format de Sortie JSON
```json
{
  "emotions": {
    "Joie": 0.65,
    "Peur": 0.12
  },
  "pattern": {
    "id": "PAT_abc123",
    "name": "JOIE",
    "similarity": 0.92,
    "confidence": 0.85,
    "is_new": false,
    "is_transition": true
  },
  "coefficients": {
    "alpha": 0.35,
    "beta": 0.20,
    "gamma": 0.15,
    "delta": 0.10,
    "theta": 0.20,
    "emergency_threshold": 0.85
  },
  "mct": {
    "size": 45,
    "stability": 0.78,
    "volatility": 0.22,
    "trend": 0.15
  },
  "E_global": 0.42,
  "valence": 0.53,
  "intensity": 0.38
}
```

## Compilation

```bash
mkdir build && cd build
cmake ..
make -j4
```

### Dépendances
- C++20
- nlohmann/json (FetchContent)
- SimpleAmqpClient
- RabbitMQ (librabbitmq)
- Boost (system, thread)

## Utilisation

### Démarrage
```cpp
RabbitMQConfig config;
config.host = "localhost";

MCEEEngine engine(config);
engine.start();
```

### API v3.0
```cpp
// Obtenir le pattern actuel
std::string pattern = engine.getCurrentPatternName();

// Forcer un pattern spécifique
engine.forcePattern("SERENITE", "Manual override");

// Créer un pattern à partir de l'état actuel
std::string new_id = engine.createPatternFromCurrent("MON_PATTERN", "Description");

// Envoyer un feedback sur le matching
engine.provideFeedback(0.8);  // Bon match

// Déclencher une passe d'apprentissage
engine.runLearning();

// Sauvegarder/Charger les patterns
engine.savePatterns("patterns.json");
engine.loadPatterns("patterns.json");
```

## Émotions Supportées (24)

| Index | Émotion      | Index | Émotion       |
|-------|--------------|-------|---------------|
| 0     | Joie         | 12    | Envie         |
| 1     | Tristesse    | 13    | Gratitude     |
| 2     | Peur         | 14    | Espoir        |
| 3     | Colère       | 15    | Désespoir     |
| 4     | Surprise     | 16    | Ennui         |
| 5     | Dégoût       | 17    | Curiosité     |
| 6     | Confiance    | 18    | Confusion     |
| 7     | Anticipation | 19    | Émerveillement|
| 8     | Amour        | 20    | Mépris        |
| 9     | Culpabilité  | 21    | Embarras      |
| 10    | Honte        | 22    | Excitation    |
| 11    | Fierté       | 23    | Sérénité      |

## Patterns de Base

| Pattern     | Émotions Dominantes           | Seuil Urgence |
|-------------|-------------------------------|---------------|
| SERENITE    | Sérénité, Confiance, Espoir   | 0.90          |
| JOIE        | Joie, Excitation, Fierté      | 0.85          |
| EXPLORATION | Curiosité, Anticipation, Awe  | 0.80          |
| ANXIETE     | Peur (modérée), Anticipation  | 0.70          |
| PEUR        | Peur (intense), Surprise      | 0.50          |
| TRISTESSE   | Tristesse, Désespoir          | 0.75          |
| DEGOUT      | Dégoût, Mépris, Colère        | 0.70          |
| CONFUSION   | Confusion, Surprise           | 0.75          |

## Cycle de Vie des Patterns

```
                    ┌─────────────────┐
                    │   CRÉATION      │
                    │ (nouvelle exp.) │
                    └────────┬────────┘
                             │
                             ▼
┌─────────────────┐   ┌──────────────┐   ┌─────────────────┐
│    FUSION       │◀──│   RENFORCEMENT│──▶│    DÉCLIN       │
│ (sim > 0.92)    │   │  (activations)│   │ (non utilisé)   │
└────────┬────────┘   └──────────────┘   └────────┬────────┘
         │                                         │
         └────────────────┬────────────────────────┘
                          │
                          ▼
                    ┌─────────────────┐
                    │     OUBLI       │
                    │ (force < 0.1)   │
                    └─────────────────┘
```

## Migration v2.0 → v3.0

Les phases fixes sont remplacées par des patterns. Le mapping :
- `Phase::SERENITE` → Pattern "SERENITE"
- `Phase::JOIE` → Pattern "JOIE"
- etc.

Les anciennes APIs restent disponibles pour compatibilité :
```cpp
// Legacy (fonctionne toujours)
Phase phase = engine.getCurrentPhase();

// Nouveau (recommandé)
std::string pattern = engine.getCurrentPatternName();
```

## Fichiers du Projet

```
mcee/
├── CMakeLists.txt
├── README.md
├── config/
│   └── phase_config.json      # Config patterns de base
├── include/
│   ├── Types.hpp              # Types de base (EmotionalState, etc.)
│   ├── MCT.hpp                # Mémoire Court Terme
│   ├── MLT.hpp                # Mémoire Long Terme
│   ├── PatternMatcher.hpp     # Matching MCT/MLT
│   ├── MCEEEngine.hpp         # Moteur principal
│   ├── PhaseDetector.hpp      # Legacy (compatibilité)
│   ├── EmotionUpdater.hpp     # Mise à jour émotions
│   ├── Amyghaleon.hpp         # Système urgence
│   ├── MemoryManager.hpp      # Gestion souvenirs
│   └── SpeechInput.hpp        # Analyse parole
└── src/
    ├── main.cpp
    ├── MCEEEngine.cpp
    ├── MCT.cpp
    ├── MLT.cpp
    ├── PatternMatcher.cpp
    ├── PhaseDetector.cpp
    ├── EmotionUpdater.cpp
    ├── Amyghaleon.cpp
    ├── MemoryManager.cpp
    └── SpeechInput.cpp
```

---

**Version 3.0** - Architecture MCT/MLT avec Patterns Dynamiques  
Auteur: virtus1er  
Date: Décembre 2024
