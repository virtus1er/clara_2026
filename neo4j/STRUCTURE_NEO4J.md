# Structure Neo4j - MCEE (Modèle Complet d'Évaluation des États)

## Vue d'ensemble

Le système utilise Neo4j comme base de données graphe pour stocker les mémoires émotionnelles, les concepts sémantiques, les patterns comportementaux et leurs relations.

```
┌─────────────────────────────────────────────────────────────────────┐
│                        ARCHITECTURE MÉMOIRE                         │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│   ┌─────────┐    EVOQUE     ┌─────────┐    SEMANTIQUE   ┌────────┐ │
│   │ Memory  │──────────────▶│ Concept │───────────────▶│ Concept│ │
│   └────┬────┘               └────┬────┘                 └────────┘ │
│        │                         │                                  │
│        │ ACTIVATED_BY            │ ASSOCIE_A                       │
│        ▼                         ▼                                  │
│   ┌─────────┐              ┌─────────┐                             │
│   │ Pattern │◀────────────▶│ Session │                             │
│   └────┬────┘ TRANSITION   └─────────┘                             │
│        │                                                            │
│        ▼                                                            │
│   ┌─────────┐                                                       │
│   │ Pattern │                                                       │
│   └─────────┘                                                       │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Nœuds (Nodes)

### 1. Memory

Représente un souvenir/mémoire avec son contexte émotionnel.

```cypher
(:Memory {
    id: String,                    -- Identifiant unique (ex: "MEM_1734567890.123")
    
    -- Émotions (format principal)
    emotional_states: String,      -- JSON: {"sentence_id": [24 floats], ...}
    dominant: String,              -- Émotion dominante (ex: "Joie", "Peur")
    intensity: Float,              -- Intensité globale [0.0 - 1.0]
    valence: Float,                -- Valence [-1.0 (négatif) à 1.0 (positif)]
    
    -- Métadonnées
    weight: Float,                 -- Poids/importance [0.0 - 1.0]
    context: String,               -- Texte du contexte
    category: String,              -- Catégorie sémantique
    
    -- Classification mémoire
    type: String,                  -- "MCT" | "MLT" | "Working" | "Episodic" | "Semantic"
    consolidated: Boolean,         -- True si consolidé en MLT
    active: Boolean,               -- True si actif en mémoire de travail
    
    -- Statistiques
    activation_count: Integer,     -- Nombre de réactivations
    
    -- Timestamps
    created_at: DateTime,
    updated_at: DateTime,
    last_accessed: DateTime
})
```

**Labels additionnels possibles :**
- `:Trauma` - Pour les mémoires traumatiques
- `:MCT` - Mémoire à Court Terme
- `:MLT` - Mémoire à Long Terme
- `:Archived` - Mémoire archivée

#### Format emotional_states

```json
{
    "1": [0.8, 0.24, 0.16, 0.0, ...],   // sentence_id: [24 émotions]
    "4": [0.1, 0.7, 0.21, 0.0, ...],
    "7": [0.0, 0.0, 0.0, 0.9, ...]
}
```

Les 24 émotions (indices 0-23) :
| Index | Émotion | Index | Émotion | Index | Émotion |
|-------|---------|-------|---------|-------|---------|
| 0 | Joie | 8 | Sérénité | 16 | Extase |
| 1 | Confiance | 9 | Acceptation | 17 | Admiration |
| 2 | Peur | 10 | Appréhension | 18 | Terreur |
| 3 | Surprise | 11 | Distraction | 19 | Étonnement |
| 4 | Tristesse | 12 | Pensivité | 20 | Chagrin |
| 5 | Dégoût | 13 | Ennui | 21 | Aversion |
| 6 | Colère | 14 | Contrariété | 22 | Rage |
| 7 | Anticipation | 15 | Intérêt | 23 | Vigilance |

---

### 2. Trauma

Extension de Memory pour les souvenirs traumatiques.

```cypher
(:Memory:Trauma {
    -- Hérite de toutes les propriétés Memory
    id: String,
    emotional_states: String,
    ...
    
    -- Propriétés spécifiques Trauma
    trauma: Boolean,               -- Toujours true
    triggers: [String],            -- Mots/concepts déclencheurs
    protection_level: Float,       -- Niveau de protection [0.0 - 1.0]
    
    -- Le weight a un plancher (floor) de 0.3 pour les traumas
})
```

---

### 3. Concept

Représente un concept sémantique extrait des mémoires.

```cypher
(:Concept {
    name: String,                  -- Nom du concept (minuscules, ex: "parc")
    
    -- Émotions accumulées
    emotional_states: String,      -- JSON: {"sentence_id": [24 floats], ...}
    
    -- Références
    memory_ids: [String],          -- Liste des IDs de mémoires liées
    
    -- Timestamps
    created_at: DateTime,
    updated_at: DateTime
})
```

**Note :** Les `emotional_states` d'un Concept accumulent les états de toutes les mémoires qui l'évoquent, permettant une analyse de l'évolution émotionnelle du concept.

---

### 4. Pattern

Représente un pattern émotionnel/comportemental.

```cypher
(:Pattern {
    name: String,                  -- Nom du pattern (ex: "SERENITE", "ANXIETE")
    description: String,           -- Description du pattern
    characteristics: String,       -- JSON des caractéristiques
    
    -- Timestamps
    created_at: DateTime,
    updated_at: DateTime
})
```

**Patterns standards :**
- `SERENITE` - État calme et paisible
- `JOIE` - État joyeux
- `EXPLORATION` - Curiosité, découverte
- `ANXIETE` - État anxieux
- `PEUR` - État de peur
- `TRISTESSE` - État triste
- `DEGOUT` - État de dégoût
- `CONFUSION` - État confus

---

### 5. Session

Représente une session d'interaction.

```cypher
(:Session:MCT {
    id: String,                    -- Identifiant unique
    current_pattern: String,       -- Pattern actif
    
    -- Métriques
    stability: Float,              -- Stabilité émotionnelle [0.0 - 1.0]
    volatility: Float,             -- Volatilité [0.0 - 1.0]
    trend: String,                 -- "stable" | "ascending" | "descending"
    state_count: Integer,          -- Nombre d'états enregistrés
    
    -- Timestamps
    created_at: DateTime,
    updated_at: DateTime
})
```

---

### 6. EmotionalState

État émotionnel ponctuel (lié à une Session).

```cypher
(:EmotionalState {
    emotions: [Float],             -- 24 valeurs d'émotions
    dominant: String,              -- Émotion dominante
    intensity: Float,              -- Intensité
    valence: Float,                -- Valence
    timestamp: DateTime            -- Moment de l'enregistrement
})
```

---

## Relations

### 1. EVOQUE

Lie une mémoire aux concepts qu'elle évoque.

```cypher
(:Memory)-[:EVOQUE]->(:Concept)
```

**Propriétés :** Aucune (relation simple)

---

### 2. SEMANTIQUE

Relation sémantique entre deux concepts.

```cypher
(:Concept)-[:SEMANTIQUE {
    type: String,                  -- Type de relation (voir ci-dessous)
    count: Integer,                -- Nombre d'occurrences
    memory_ids: [String],          -- Mémoires source
    emotional_states: String       -- JSON des états émotionnels
}]->(:Concept)
```

**Types de relations sémantiques :**
| Type | Description | Exemple |
|------|-------------|---------|
| `EST` | Attribution/Identité | "chat" -[EST]-> "noir" |
| `UTILISE` | Action/Utilisation | "chat" -[UTILISE]-> "dort" |
| `LOCALISE` | Localisation | "chat" -[LOCALISE]-> "canapé" |
| `MODIFIE` | Modification/Adverbe | "paisiblement" -[MODIFIE]-> "dort" |
| `POSSEDE` | Possession | "marie" -[POSSEDE]-> "chat" |
| `CAUSE` | Causalité | "pluie" -[CAUSE]-> "tristesse" |

---

### 3. TRANSITION_TO

Transition entre patterns émotionnels.

```cypher
(:Pattern)-[:TRANSITION_TO {
    count: Integer,                -- Nombre de transitions observées
    probability: Float,            -- Probabilité de transition
    last_transition: DateTime,     -- Dernière occurrence
    contexts: [String]             -- Contextes des transitions
}]->(:Pattern)
```

---

### 4. ACTIVATED_BY

Lie un pattern aux mémoires qui l'activent.

```cypher
(:Pattern)-[:ACTIVATED_BY]->(:Memory)
```

---

### 5. ASSOCIE_A

Association générique entre Memory et Concept.

```cypher
(:Memory)-[:ASSOCIE_A {
    strength: Float,               -- Force de l'association [0.0 - 1.0]
    created_at: DateTime
}]->(:Concept)
```

---

### 6. CONTAINS

Lie une session à ses états émotionnels.

```cypher
(:Session)-[:CONTAINS]->(:EmotionalState)
```

---

### 7. TRIGGERS

Lie un trauma à ses concepts déclencheurs.

```cypher
(:Trauma)-[:TRIGGERS]->(:Concept)
```

---

## Index recommandés

```cypher
-- Index primaires
CREATE INDEX memory_id IF NOT EXISTS FOR (m:Memory) ON (m.id);
CREATE INDEX concept_name IF NOT EXISTS FOR (c:Concept) ON (c.name);
CREATE INDEX pattern_name IF NOT EXISTS FOR (p:Pattern) ON (p.name);
CREATE INDEX session_id IF NOT EXISTS FOR (s:Session) ON (s.id);

-- Index de recherche
CREATE INDEX memory_dominant IF NOT EXISTS FOR (m:Memory) ON (m.dominant);
CREATE INDEX memory_type IF NOT EXISTS FOR (m:Memory) ON (m.type);
CREATE INDEX memory_weight IF NOT EXISTS FOR (m:Memory) ON (m.weight);

-- Index composites
CREATE INDEX memory_type_weight IF NOT EXISTS FOR (m:Memory) ON (m.type, m.weight);
```

---

## Requêtes Cypher courantes

### Créer une mémoire avec concepts

```cypher
// 1. Créer la mémoire
CREATE (m:Memory:MCT {
    id: $id,
    emotional_states: $emotional_states,  // JSON string
    dominant: $dominant,
    intensity: $intensity,
    valence: $valence,
    weight: $weight,
    type: 'MCT',
    created_at: datetime()
})

// 2. Créer/lier les concepts
MERGE (c:Concept {name: $concept_name})
ON CREATE SET c.created_at = datetime(), c.memory_ids = [$id]
ON MATCH SET c.memory_ids = c.memory_ids + $id
MERGE (m)-[:EVOQUE]->(c)
```

### Rechercher par sentence_id dans emotional_states

```cypher
// Concepts contenant un sentence_id spécifique
MATCH (c:Concept)
WHERE c.emotional_states IS NOT NULL 
  AND c.emotional_states CONTAINS '"<sentence_id>":'
RETURN c
```

### Trouver les mémoires similaires

```cypher
MATCH (m:Memory)
WHERE m.intensity IS NOT NULL AND m.valence IS NOT NULL
WITH m,
     1 - abs(m.intensity - $query_intensity) AS intensity_sim,
     1 - abs(m.valence - $query_valence) AS valence_sim
WITH m, (intensity_sim + valence_sim) / 2 AS similarity
WHERE similarity >= $threshold
RETURN m ORDER BY similarity DESC LIMIT $limit
```

### Consolider MCT → MLT

```cypher
MATCH (m:Memory)
WHERE (m.type = 'MCT' OR m.type IS NULL) 
  AND (m.consolidated IS NULL OR m.consolidated = false)
  AND m.weight >= $threshold
SET m.type = 'MLT', m.consolidated = true, m:MLT
REMOVE m:MCT
RETURN count(m) AS consolidated
```

### Obtenir l'historique émotionnel d'un concept

```cypher
MATCH (c:Concept {name: $name})
RETURN c.name, c.emotional_states, c.memory_ids
```

---

## Conventions importantes

### 1. Sérialisation JSON

Les `emotional_states` sont **toujours** stockés comme JSON string :
```python
# Python → Neo4j
json.dumps({str(k): v for k, v in emotional_states.items()})

# Neo4j → Python  
json.loads(emotional_states_string)
```

### 2. Clés de sentence_id

Les clés dans `emotional_states` sont **toujours des strings** :
```json
{"1": [...], "42": [...]}  // ✅ Correct
{1: [...], 42: [...]}      // ❌ Incorrect
```

### 3. Recherche dans JSON

Utiliser `CONTAINS` pour rechercher dans les JSON strings :
```cypher
WHERE c.emotional_states CONTAINS '"42":'  // Cherche sentence_id 42
```

### 4. Syntaxe Neo4j 5+

- `NOT EXISTS(m.prop)` → `m.prop IS NULL`
- `EXISTS(m.prop)` → `m.prop IS NOT NULL`

---

## Schéma visuel complet

```
                                    ┌──────────────┐
                                    │   Pattern    │
                                    │  (SERENITE)  │
                                    └──────┬───────┘
                                           │
                              ACTIVATED_BY │
                                           ▼
┌─────────────────────────────────────────────────────────────────┐
│                          Memory (MCT/MLT)                       │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │ id: "MEM_123"                                              │ │
│  │ emotional_states: '{"1": [0.8, 0.2, ...], "4": [...]}'    │ │
│  │ dominant: "Joie"                                           │ │
│  │ intensity: 0.85                                            │ │
│  │ valence: 0.9                                               │ │
│  │ weight: 0.7                                                │ │
│  │ type: "MLT"                                                │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────────────────┬───────────────────────────────────────────┘
                      │
                      │ EVOQUE
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│                           Concept                               │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │ name: "parc"                                               │ │
│  │ emotional_states: '{"1": [...], "4": [...], "7": [...]}'  │ │
│  │ memory_ids: ["MEM_123", "MEM_456", "MEM_789"]              │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────────────────┬───────────────────────────────────────────┘
                      │
                      │ SEMANTIQUE {type: "LOCALISE"}
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│                           Concept                               │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │ name: "canards"                                            │ │
│  │ emotional_states: '{"1": [...]}'                          │ │
│  │ memory_ids: ["MEM_123"]                                    │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

---

## Analyse émotionnelle

Lors de la récupération d'un concept avec `get_concept`, une analyse est effectuée :

```python
{
    "name": "parc",
    "memory_ids": ["MEM_1", "MEM_2"],
    "emotional_states": {"1": [...], "4": [...]},
    "sentence_ids": ["1", "4"],
    "emotional_analysis": {
        "dominant_emotion": "Joie",           # Émotion la plus fréquente
        "average_valence": 0.73,              # Valence moyenne
        "stability": 0.96,                    # Stabilité [0-1]
        "trajectory": "stable",               # "stable" | "ascending" | "descending"
        "trauma_score": 0.12                  # Score de trauma [0-1]
    }
}
```

---

*Documentation générée le 22/12/2024 - Version 3.0 avec support emotional_states*
