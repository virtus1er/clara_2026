# MCEE - Modèle Complet d'Évaluation des États

**Version 3.0** | Architecture Dynamique

## Vue d'ensemble

Le MCEE v3 est un système émotionnel avec **patterns adaptatifs**. Contrairement aux systèmes à phases fixes, les patterns émotionnels sont :

- **Créés dynamiquement** quand un nouvel état non reconnu apparaît
- **Renforcés** par répétition
- **Fusionnés** si trop similaires  
- **Oubliés** si non utilisés

## Architecture

```
┌──────────────────┐     ┌──────────────────┐
│ Module Emotion   │     │ Module Parole    │
│ (24 émotions)    │     │ (texte)          │
└────────┬─────────┘     └────────┬─────────┘
         │ RabbitMQ               │ RabbitMQ
         │                        │
         ▼                        ▼
┌─────────────────────────────────────────────────────────────┐
│                      MCEEEngine v3                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│   ┌─────────────────────────────────────────────────────┐   │
│   │                    MCT                               │   │
│   │            (Mémoire à Court Terme)                  │   │
│   │         Buffer temporel des états récents           │   │
│   └──────────────────────┬──────────────────────────────┘   │
│                          │                                   │
│                          ▼                                   │
│   ┌─────────────────────────────────────────────────────┐   │
│   │               PatternManager                         │   │
│   │         Patterns dynamiques + Coefficients          │   │
│   │    ┌──────────┐ ┌──────────┐ ┌──────────┐          │   │
│   │    │ Pattern  │ │ Pattern  │ │ Pattern  │   ...    │   │
│   │    │  Joie    │ │  Peur    │ │ Calme+   │          │   │
│   │    │ α=0.40   │ │ α=0.60   │ │ α=0.25   │          │   │
│   │    └──────────┘ └──────────┘ └──────────┘          │   │
│   └──────────────────────┬──────────────────────────────┘   │
│                          │                                   │
│                          ▼                                   │
│   ┌─────────────────────────────────────────────────────┐   │
│   │              EmotionUpdater                          │   │
│   │   E_i(t+1) = E_i(t) + α·Fb + β·Fb_int - γ·Δt + ...  │   │
│   │        (coefficients du pattern actif)               │   │
│   └──────────────────────┬──────────────────────────────┘   │
│                          │                                   │
│             ┌────────────┴────────────┐                     │
│             ▼                         ▼                     │
│   ┌──────────────────┐      ┌──────────────────┐           │
│   │   Amyghaleon     │      │  MemoryManager   │           │
│   │   (urgences)     │      │  (MLT/Neo4j)     │           │
│   └──────────────────┘      └──────────────────┘           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
         │
         │ RabbitMQ
         ▼
   [État émotionnel + Pattern actif]
```

## Concepts clés

### MCT (Mémoire à Court Terme)

Buffer circulaire des états émotionnels récents (~60 secondes).

```cpp
MCT mct;
mct.push(emotional_state);                    // Ajouter un état
auto integrated = mct.getIntegratedState();   // État moyenné
auto signature = mct.getCurrentSignature();   // Vecteur 24D normalisé
double volatility = mct.computeVolatility();  // Variabilité récente
```

### PatternManager (remplace les phases fixes)

Les patterns sont des "attracteurs" émotionnels appris.

```cpp
PatternManager pm;

// Trouver/créer le pattern correspondant
auto match = pm.findBestMatch(signature, auto_create=true);

if (match.is_new) {
    // Nouveau pattern créé automatiquement
    std::cout << "Nouveau pattern: " << match.pattern->label << std::endl;
}

// Les coefficients sont dynamiques
PatternCoefficients coeff = match.pattern->coefficients;
// α, β, γ, δ, θ adaptés à ce pattern
```

### Création/Fusion de patterns

| Similarité | Action |
|------------|--------|
| < 0.5 | **Créer** nouveau pattern |
| 0.5 - 0.9 | **Activer** pattern existant (le renforce) |
| > 0.9 | **Fusionner** patterns similaires |

### Patterns d'urgence (protégés)

Deux patterns sont créés au démarrage et protégés contre la suppression :
- `🚨 PEUR (urgence)` - seuil bas (0.50), haute réactivité
- `⛔ HORREUR (critique)` - seuil très bas (0.40), réponse maximale

## Communication RabbitMQ

### Entrée Émotions
- **Exchange**: `mcee.emotional.input`
- **Routing Key**: `emotions.predictions`
```json
{
  "Joie": 0.8,
  "Calme": 0.6,
  "Peur": 0.1,
  ...
}
```

### Entrée Parole
- **Exchange**: `mcee.speech.input`
- **Routing Key**: `speech.text`
```json
{
  "text": "Je suis content !",
  "source": "user",
  "confidence": 0.95
}
```

### Sortie État
- **Exchange**: `mcee.emotional.output`
- **Routing Key**: `mcee.state`
```json
{
  "emotions": {...},
  "E_global": 0.542,
  "variance": 0.087,
  "volatility": 0.12,
  "pattern": {
    "id": "pat_a1b2c3d4",
    "label": "Joie+Excitation",
    "similarity": 0.89
  },
  "coefficients": {
    "alpha": 0.40,
    "beta": 0.25,
    "gamma": 0.08,
    "delta": 0.35,
    "theta": 0.05,
    "amyghaleon_threshold": 0.85
  },
  "mct": {
    "size": 45,
    "in_transition": false
  },
  "trends": {
    "Joie": 0.05,
    "Peur": -0.02
  }
}
```

## Structure du projet

```
mcee/
├── CMakeLists.txt
├── README.md
├── config/
│   └── phase_config.json
├── include/
│   ├── Types.hpp            # Types de base
│   ├── MCT.hpp              # Mémoire à Court Terme
│   ├── PatternManager.hpp   # Patterns dynamiques
│   ├── EmotionUpdater.hpp   # Mise à jour émotions
│   ├── Amyghaleon.hpp       # Système d'urgence
│   ├── MemoryManager.hpp    # MLT (Neo4j prévu)
│   ├── SpeechInput.hpp      # Analyse parole
│   └── MCEEEngine.hpp       # Moteur principal
└── src/
    ├── main.cpp
    ├── MCT.cpp
    ├── PatternManager.cpp
    ├── EmotionUpdater.cpp
    ├── Amyghaleon.cpp
    ├── MemoryManager.cpp
    ├── SpeechInput.cpp
    └── MCEEEngine.cpp
```

## Compilation

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Dépendances
- CMake 3.14+
- C++20
- Boost (system, thread)
- RabbitMQ + SimpleAmqpClient
- nlohmann/json (auto-téléchargé)

## Utilisation

```bash
# Mode démonstration (sans RabbitMQ)
./mcee --demo

# Mode normal
./mcee --host localhost --port 5672

# Avec configuration
./mcee -c config/phase_config.json
```

## Exemple de sortie (démo)

```
═══════════════════════════════════════════════════════════════
 Phase 1: État initial calme - Découverte des patterns
═══════════════════════════════════════════════════════════════

[PatternManager] ✨ Nouveau pattern créé: Calme+Satisfaction (id=pat_a1b2c3d4)
[MCT] État ajouté, buffer: 5 états

═══════════════════════════════════════════════════════════════
 Phase 5: ⚠ URGENCE - Peur intense (pattern d'urgence)
═══════════════════════════════════════════════════════════════

╔══════════════════════════════════════════════════════════════╗
║           🚨 URGENCE AMYGHALEON DÉCLENCHÉE 🚨                ║
╠══════════════════════════════════════════════════════════════╣
║ Action    : FUITE                                            ║
║ Priorité  : CRITIQUE                                         ║
║ Pattern   : 🚨 PEUR (urgence)                               ║
╚══════════════════════════════════════════════════════════════╝
```

## Différences v2 → v3

| Aspect | v2 (phases fixes) | v3 (patterns dynamiques) |
|--------|-------------------|--------------------------|
| **États** | 8 phases prédéfinies | Patterns illimités, appris |
| **Coefficients** | Statiques par phase | Dynamiques, évoluent |
| **Création** | Aucune | Automatique si nouveau |
| **Adaptation** | Règles if/else | Similarité cosinus |
| **Mémoire** | État courant | MCT (fenêtre temporelle) |
| **Oubli** | Non | Décroissance si non utilisé |

## Formules

### Mise à jour émotion
```
E_i(t+1) = E_i(t) + α·Fb_ext + β·Fb_int - γ·Δt + δ·IS + θ·Wt
```
Où α, β, γ, δ, θ proviennent du **pattern actif**.

### Similarité pattern
```
sim(A, B) = (A · B) / (||A|| × ||B||)
```
Similarité cosinus entre signatures 24D.

### Volatilité MCT
```
volatility = mean(distance(state[i], state[i-1]))
```
Moyenne des changements récents.

## Roadmap

- [x] Architecture MCT + PatternManager
- [x] Patterns dynamiques (création/fusion/oubli)
- [x] Intégration parole (SpeechInput)
- [x] Coefficients adaptatifs
- [ ] Intégration Neo4j pour MLT
- [ ] Apprentissage des coefficients par feedback
- [ ] Module Rêve (consolidation nocturne)
- [ ] Interface de visualisation temps réel
