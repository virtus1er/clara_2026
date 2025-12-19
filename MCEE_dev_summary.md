# MCEE - Résumé Technique pour le Développement

## 🎯 Vue d'ensemble

Le MCEE (Modèle Complet d'Évaluation des États) est un système émotionnel complet intégrant :
- **24 émotions instantanées** (déjà prédites par votre module C++)
- **Graphe de mémoire Neo4j** (structure partiellement en place)
- **Mécanismes de fusion et modulation** (à implémenter)
- **Système d'urgence "Amyghaleon"** (à développer)

---

## 1. Architecture des Composants

```
┌─────────────────┐
│   Capteurs      │ → Données environnementales
│   Feedbacks     │ → Fb_ext (externe), Fb_int (interne)
└────────┬────────┘
         ↓
┌─────────────────┐
│  Module C++     │ → Prédit 24 émotions E_i(t)
│  (emotion)      │    depuis 14 dimensions
└────────┬────────┘
         ↓
┌─────────────────┐
│  MCEE Engine    │ → Mise à jour E_i(t+1)
│  (à développer) │    Fusion → E_global(t+1)
└────────┬────────┘
         ↓
┌─────────────────┐
│  Neo4j Graphe   │ → Souvenirs, concepts, traumas
│  Mémoire        │    Activation, oubli, renforcement
└────────┬────────┘
         ↓
┌─────────────────┐
│  Amyghaleon     │ → Réactions d'urgence
│  (court-circuit)│    si trauma ou pic émotionnel
└─────────────────┘
```

---

## 2. Formules Clés à Implémenter

### 2.1 Mise à jour des émotions individuelles

```python
E_i(t+1) = E_i(t) + α·Fb_ext + β·Fb_int(t) - γ·Δt + δ·Influence_Souvenirs + θ·W_t
```

**Variables :**
- `E_i(t)` : Émotion i actuelle (de votre module C++)
- `α, β, γ, δ, θ` : Coefficients de pondération (à calibrer)
- `Fb_ext` : Feedback externe (utilisateur, environnement)
- `Fb_int` : Feedback interne (énergie, tension, auto-évaluation)
- `Δt` : Delta temps (décroissance naturelle)
- `W_t` : Coefficient de sagesse (expérience accumulée)
- `Influence_Souvenirs` : Calculé depuis Neo4j (voir 3.2)

**À implémenter :**
```python
class EmotionUpdater:
    def __init__(self, alpha=0.3, beta=0.2, gamma=0.1, delta=0.4, theta=0.1):
        self.alpha = alpha
        self.beta = beta
        self.gamma = gamma
        self.delta = delta
        self.theta = theta
    
    def update_emotion(self, E_current, fb_ext, fb_int, delta_t, 
                       influence_memories, wisdom):
        E_next = (E_current + 
                  self.alpha * fb_ext +
                  self.beta * fb_int -
                  self.gamma * delta_t +
                  self.delta * influence_memories +
                  self.theta * wisdom)
        return np.clip(E_next, 0.0, 1.0)
```

### 2.2 Variance (détection d'anomalies)

```python
Var_i(t) = (1/m) * Σ[E_i(t) - S_i,j]²
```

**Variables :**
- `S_i,j` : Valeurs passées de l'émotion i (depuis Neo4j)
- `m` : Nombre de souvenirs pertinents

**Usage :** Détecter si l'état émotionnel actuel est cohérent avec le passé.

```python
def compute_variance(emotion_current, past_values):
    """
    Args:
        emotion_current: float, valeur actuelle de l'émotion
        past_values: list[float], valeurs passées depuis Neo4j
    """
    if not past_values:
        return 0.0
    variance = np.mean([(emotion_current - s)**2 for s in past_values])
    return variance
```

### 2.3 Fusion des émotions (avec stabilisation tanh)

```python
E_global(t+1) = tanh(E_global(t) + Σ[E_i(t+1) · (1 - Var_global(t))])
```

**Pourquoi tanh ?** Limite les valeurs dans [-1, 1] pour éviter les explosions.

```python
def fuse_emotions(E_global_t, emotions_updated, var_global):
    """
    Args:
        E_global_t: float, émotion globale précédente
        emotions_updated: dict[str, float], 24 émotions mises à jour
        var_global: float, variance globale moyenne
    """
    sum_weighted = sum(e * (1 - var_global) for e in emotions_updated.values())
    E_global_next = np.tanh(E_global_t + sum_weighted)
    return E_global_next
```

---

## 3. Gestion de la Mémoire (Neo4j)

### 3.1 Structure des nœuds

**Types de nœuds :**
```cypher
// Souvenir épisodique
CREATE (s:Souvenir {
    name: 'Événement X',
    date: date('2025-12-19'),
    emotions: [0.7, 0.2, ...],  // 24 valeurs
    dominant: 'Joie',
    valence: 0.7,
    intensity: 0.8,
    last_activated: datetime(),
    activation_count: 1,
    weight: 0.5,
    type: 'positif',
    state: 'SouvenirConsolider'  // État temporaire
})

// Trauma (souvenir traumatique)
CREATE (t:Trauma {
    name: 'Incident Y',
    date: date('2024-10-15'),
    emotion_signature: [0.9, 0.1, ...],
    intensity: 0.95,
    trauma: true,
    forget_rate: 0.01,  // Très bas = difficile à oublier
    reinforced: true
})

// Concept (abstraction)
CREATE (c:Concept {
    name: 'Chien',
    keywords: ['canin', 'aboiements', 'compagnon'],
    created_at: date('2025-12-19')
})
```

**Types de relations :**
```cypher
// Liens entre souvenirs et concepts
(souvenir)-[:EVOQUE]->(concept)        // Évoque un concept
(souvenir)-[:CONTEXTE]->(concept)      // Contexte environnemental
(trauma)-[:CONCERNE]->(souvenir)       // Trauma lié à un souvenir
(concept1)-[:ASSOCIE_A {              // Association émotionnelle
    emotions: [...],
    etiquette: 'peur',
    emotions_valeur: 0.6,
    trauma: true,
    weight: 0.9,
    context: 'morsure ancienne'
}]->(concept2)
```

### 3.2 Calcul de l'Influence des Souvenirs

**Formule d'activation :**
```python
A(S_i) = forget(S_i, t) × (1 + R(S_i)) × Σ[C(S_i, S_k) × Me(S_i, E_current) × U(S_i)]
```

**Composantes :**
1. **forget(S_i, t)** : Décroissance exponentielle
   ```python
   forget = exp(-λ * Δt)  # λ = taux d'oubli (ex: 0.1)
   ```

2. **R(S_i)** : Renforcement (fréquence d'usage)
   ```python
   R = activation_count / max_activations
   ```

3. **C(S_i, S_k)** : Force de connexion (poids de l'arête)
   ```cypher
   MATCH (s1)-[r]-(s2) RETURN r.weight
   ```

4. **Me(S_i, E_current)** : Similarité émotionnelle (distance cosinus)
   ```python
   def emotion_similarity(emotion_memory, emotion_current):
       return cosine_similarity(emotion_memory, emotion_current)
   ```

5. **U(S_i)** : Utilisation récente
   ```python
   U = 1.0 / (1 + days_since_last_activation)
   ```

**Requête Neo4j pour récupérer les souvenirs pertinents :**
```cypher
MATCH (s:Souvenir)
WHERE s.dominant IN ['Joie', 'Peur']  // Filtre par émotion dominante
  AND s.last_activated > date() - duration({days: 30})
RETURN s.name, s.emotions, s.weight, s.activation_count, s.last_activated
ORDER BY s.weight DESC
LIMIT 10
```

**Calcul Python :**
```python
def compute_memory_influence(souvenirs, emotion_current, delta_t):
    """
    Args:
        souvenirs: list[dict], souvenirs depuis Neo4j
        emotion_current: np.array, vecteur 24 émotions actuelles
        delta_t: float, temps écoulé depuis dernière activation
    Returns:
        float, influence globale des souvenirs
    """
    total_influence = 0.0
    
    for s in souvenirs:
        # 1. Oubli
        forget_factor = np.exp(-0.1 * delta_t)
        
        # 2. Renforcement
        R = s['activation_count'] / 100.0
        
        # 3. Similarité émotionnelle
        Me = cosine_similarity([emotion_current], [s['emotions']])[0][0]
        
        # 4. Utilisation
        days_ago = (datetime.now() - s['last_activated']).days
        U = 1.0 / (1 + days_ago)
        
        # 5. Poids de connexion (depuis les relations)
        C = s.get('weight', 0.5)
        
        # Activation totale
        A = forget_factor * (1 + R) * C * Me * U
        
        # Influence (valence * activation)
        total_influence += s.get('valence', 0.0) * A
    
    return total_influence
```

### 3.3 Gestion quotidienne (Consolidation)

**Processus :**
1. **Marquage en cours de journée** : État `SouvenirConsolider`
2. **Analyse nocturne** (module Rêve) : Évalue chaque souvenir
3. **Décisions** :
   - **Consolider** : Passe en MLT, enlève le tag temporaire
   - **Oublier** : Diminue le poids ou supprime
   - **Fusionner** : Combine plusieurs souvenirs similaires

**Requête Neo4j pour consolidation :**
```cypher
// Récupérer les souvenirs à consolider
MATCH (s:Souvenir {state: 'SouvenirConsolider'})
WHERE s.date = date('2025-12-19')
RETURN s

// Consolider (enlever le tag, renforcer)
MATCH (s:Souvenir {state: 'SouvenirConsolider', name: 'Événement X'})
SET s.state = 'Consolidé',
    s.weight = s.weight * 1.2,
    s.memory_type = 'MLT'

// Oublier (diminuer poids)
MATCH (s:Souvenir {state: 'SouvenirConsolider', name: 'Événement Y'})
WHERE s.intensity < 0.3
SET s.weight = s.weight * 0.5

// Fusionner (créer un nouveau nœud synthétique)
MATCH (s1:Souvenir {dominant: 'Joie'}), (s2:Souvenir {dominant: 'Joie'})
WHERE s1.date = s2.date AND id(s1) < id(s2)
CREATE (merged:Souvenir {
    name: 'Fusion: ' + s1.name + ' + ' + s2.name,
    emotions: [(x + y) / 2 for x, y in zip(s1.emotions, s2.emotions)],
    weight: (s1.weight + s2.weight) / 2
})
DELETE s1, s2
```

### 3.4 Traumas (gestion spéciale)

**Critères de trauma :**
- Intensité émotionnelle > 0.85
- Valence très négative < 0.2
- OU détection explicite (événement critique)

**Actions :**
1. Transfert immédiat en MLT (pas d'attente de consolidation)
2. Label `trauma: true`
3. `forget_rate` très bas (0.01 au lieu de 0.1)
4. `reinforced: true` pour renforcement automatique

```python
def check_if_trauma(emotions, intensity, valence):
    """
    Détermine si un événement est traumatique
    """
    # Seuils configurables
    TRAUMA_INTENSITY = 0.85
    TRAUMA_VALENCE = 0.2
    
    # Émotions négatives dominantes
    negative_emotions = ['Peur', 'Horreur', 'Anxiété', 'Dégoût']
    max_negative = max([emotions.get(e, 0) for e in negative_emotions])
    
    is_trauma = (
        intensity > TRAUMA_INTENSITY and 
        valence < TRAUMA_VALENCE and
        max_negative > 0.7
    )
    
    return is_trauma
```

**Stockage Neo4j :**
```cypher
CREATE (t:Trauma:Souvenir {
    name: 'Trauma: Incident critique',
    date: datetime(),
    emotion_signature: [0.9, 0.05, ...],
    intensity: 0.95,
    valence: 0.1,
    trauma: true,
    forget_rate: 0.01,
    reinforced: true,
    memory_type: 'MLT',
    immediate_transfer: true
})
```

---

## 4. Mécanisme Amyghaleon (Urgence)

### 4.1 Principe

**Court-circuite le MCEE normal** quand :
- Émotion critique > 0.9 (Peur, Horreur, Dégoût)
- Trauma similaire activé > 0.8
- Combinaison des deux

**Actions :**
- Réaction immédiate (fuite, blocage, alerte)
- Bypass de la fusion globale
- Retour au MCEE normal après stabilisation

### 4.2 Implémentation

```python
class Amyghaleon:
    """Système de réaction d'urgence"""
    
    CRITICAL_EMOTIONS = ['Peur', 'Horreur', 'Anxiété']
    THRESHOLD_CRITICAL = 0.9
    THRESHOLD_TRAUMA = 0.8
    
    def __init__(self, neo4j_driver):
        self.driver = neo4j_driver
        self.in_emergency = False
    
    def check_emergency(self, emotions, souvenirs_actives):
        """
        Vérifie si une réaction d'urgence est nécessaire
        
        Args:
            emotions: dict[str, float], 24 émotions actuelles
            souvenirs_actives: list[dict], souvenirs avec activation
        Returns:
            bool, True si urgence détectée
        """
        # 1. Vérifier les émotions critiques
        max_critical = max([emotions.get(e, 0) for e in self.CRITICAL_EMOTIONS])
        if max_critical > self.THRESHOLD_CRITICAL:
            return True
        
        # 2. Vérifier les traumas activés
        for s in souvenirs_actives:
            if s.get('trauma', False) and s['activation'] > self.THRESHOLD_TRAUMA:
                return True
        
        # 3. Combinaison (seuils plus bas)
        if max_critical > 0.7 and any(s.get('trauma') and s['activation'] > 0.6 
                                       for s in souvenirs_actives):
            return True
        
        return False
    
    def trigger_emergency_response(self, emotions, trauma_context):
        """
        Déclenche une réaction d'urgence
        
        Returns:
            dict, action à prendre
        """
        self.in_emergency = True
        
        # Identifier le trauma ou l'émotion dominante
        dominant_emotion = max(emotions.items(), key=lambda x: x[1])[0]
        
        # Actions possibles
        if dominant_emotion == 'Peur':
            return {'action': 'FUITE', 'priority': 'CRITIQUE'}
        elif dominant_emotion == 'Horreur':
            return {'action': 'BLOCAGE', 'priority': 'CRITIQUE'}
        elif dominant_emotion == 'Anxiété':
            return {'action': 'ALERTE', 'priority': 'ÉLEVÉE'}
        elif trauma_context:
            return {
                'action': 'ÉVITER_CONTEXTE',
                'context': trauma_context,
                'priority': 'CRITIQUE'
            }
        else:
            return {'action': 'PRUDENCE', 'priority': 'MODÉRÉE'}
    
    def stabilize(self, emotions):
        """
        Vérifie si la situation s'est stabilisée
        """
        max_critical = max([emotions.get(e, 0) for e in self.CRITICAL_EMOTIONS])
        if max_critical < 0.5:
            self.in_emergency = False
            return True
        return False
```

**Intégration dans la boucle principale :**
```python
def main_loop(mcee, neo4j, amyghaleon):
    while True:
        # 1. Recevoir les émotions du module C++
        emotions = receive_emotions_from_cpp()
        
        # 2. Récupérer les souvenirs pertinents
        souvenirs = neo4j.query_relevant_memories(emotions)
        
        # 3. CHECK AMYGHALEON (prioritaire)
        if amyghaleon.check_emergency(emotions, souvenirs):
            response = amyghaleon.trigger_emergency_response(emotions, souvenirs)
            execute_emergency_action(response)
            continue  # Skip MCEE normal
        
        # 4. MCEE normal si pas d'urgence
        emotions_updated = mcee.update_all_emotions(
            emotions, 
            fb_ext, 
            fb_int, 
            souvenirs
        )
        
        # 5. Fusion
        E_global = mcee.fuse_emotions(emotions_updated)
        
        # 6. Mise à jour Neo4j (renforcement, oubli)
        neo4j.update_memory_graph(emotions_updated, souvenirs)
```

---

## 5. Module de Rêve (Consolidation nocturne)

### 5.1 Processus

**Déclenché périodiquement** (ex: toutes les 24h, ou lors d'inactivité)

**Étapes :**
1. Récupérer tous les `SouvenirConsolider` du jour
2. Pour chaque souvenir :
   - Calculer intensité émotionnelle moyenne
   - Compter le nombre d'activations
   - Vérifier la similarité avec d'autres souvenirs
3. Décider : Consolider / Oublier / Fusionner
4. Mettre à jour le graphe Neo4j

### 5.2 Implémentation

```python
class DreamModule:
    """Module de consolidation des souvenirs pendant le rêve"""
    
    INTENSITY_THRESHOLD = 0.6  # Seuil de consolidation
    ACTIVATION_THRESHOLD = 3   # Nombre min d'activations
    SIMILARITY_THRESHOLD = 0.8 # Seuil de fusion
    
    def __init__(self, neo4j_driver):
        self.driver = neo4j_driver
    
    def consolidate_daily_memories(self, date):
        """
        Consolide les souvenirs d'une journée
        
        Args:
            date: str, date au format 'YYYY-MM-DD'
        """
        # 1. Récupérer les souvenirs à consolider
        query = """
        MATCH (s:Souvenir {state: 'SouvenirConsolider'})
        WHERE s.date = date($date)
        RETURN s
        """
        souvenirs = self.driver.execute_query(query, date=date)
        
        # 2. Analyser chaque souvenir
        for s in souvenirs:
            decision = self._evaluate_memory(s)
            
            if decision == 'CONSOLIDATE':
                self._consolidate_memory(s)
            elif decision == 'FORGET':
                self._forget_memory(s)
            elif decision == 'MERGE':
                self._merge_similar_memories(s)
    
    def _evaluate_memory(self, souvenir):
        """
        Évalue un souvenir et décide de son sort
        """
        intensity = souvenir['intensity']
        activations = souvenir['activation_count']
        
        # Critères de consolidation
        if intensity > self.INTENSITY_THRESHOLD and activations >= self.ACTIVATION_THRESHOLD:
            return 'CONSOLIDATE'
        
        # Critères d'oubli
        if intensity < 0.3 and activations < 2:
            return 'FORGET'
        
        # Vérifier si fusion possible
        similar = self._find_similar_memories(souvenir)
        if similar and self._compute_similarity(souvenir, similar) > self.SIMILARITY_THRESHOLD:
            return 'MERGE'
        
        # Par défaut, garder tel quel
        return 'CONSOLIDATE'
    
    def _consolidate_memory(self, souvenir):
        """Transfère un souvenir en MLT"""
        query = """
        MATCH (s:Souvenir {name: $name})
        SET s.state = 'Consolidé',
            s.memory_type = 'MLT',
            s.weight = s.weight * 1.3,
            s.consolidation_date = datetime()
        """
        self.driver.execute_query(query, name=souvenir['name'])
    
    def _forget_memory(self, souvenir):
        """Diminue le poids ou supprime un souvenir"""
        query = """
        MATCH (s:Souvenir {name: $name})
        SET s.weight = s.weight * 0.3,
            s.state = 'Affaibli'
        """
        self.driver.execute_query(query, name=souvenir['name'])
    
    def _merge_similar_memories(self, souvenir):
        """Fusionne des souvenirs similaires"""
        similar = self._find_similar_memories(souvenir)
        
        # Créer un nouveau souvenir fusionné
        merged_emotions = [
            (souvenir['emotions'][i] + similar['emotions'][i]) / 2
            for i in range(24)
        ]
        
        query = """
        MATCH (s1:Souvenir {name: $name1}), (s2:Souvenir {name: $name2})
        CREATE (merged:Souvenir {
            name: 'Fusion: ' + s1.name + ' & ' + s2.name,
            emotions: $emotions,
            intensity: (s1.intensity + s2.intensity) / 2,
            weight: (s1.weight + s2.weight),
            memory_type: 'MLT',
            merged: true
        })
        DELETE s1, s2
        """
        self.driver.execute_query(
            query,
            name1=souvenir['name'],
            name2=similar['name'],
            emotions=merged_emotions
        )
```

---

## 6. Pipeline d'Intégration Complète

### 6.1 Architecture logicielle proposée

```
┌──────────────────────────────────────────────────────────────┐
│                     MCEE_ENGINE.py                          │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐            │
│  │  Emotion   │  │   Memory   │  │ Amyghaleon │            │
│  │  Updater   │  │  Manager   │  │  Module    │            │
│  └────────────┘  └────────────┘  └────────────┘            │
│         ↕              ↕                ↕                    │
│  ┌────────────────────────────────────────────┐             │
│  │         Neo4j Driver (py2neo)             │             │
│  └────────────────────────────────────────────┘             │
└──────────────────────────────────────────────────────────────┘
         ↑                                    ↓
         │                                    │
   ┌─────────────┐                   ┌──────────────┐
   │ RabbitMQ    │                   │  Actions /   │
   │ (C++ → Py)  │                   │  Décisions   │
   └─────────────┘                   └──────────────┘
```

### 6.2 Fichier principal

```python
# mcee_engine.py

import numpy as np
import pika
import json
from py2neo import Graph
from datetime import datetime, timedelta

from emotion_updater import EmotionUpdater
from memory_manager import MemoryManager
from amyghaleon import Amyghaleon
from dream_module import DreamModule

class MCEEEngine:
    """
    Moteur principal du MCEE
    Orchestre les émotions, la mémoire et les réactions
    """
    
    def __init__(self, neo4j_uri, neo4j_user, neo4j_pass, rabbitmq_config):
        # Connexion Neo4j
        self.graph = Graph(neo4j_uri, auth=(neo4j_user, neo4j_pass))
        
        # Modules
        self.emotion_updater = EmotionUpdater()
        self.memory_manager = MemoryManager(self.graph)
        self.amyghaleon = Amyghaleon(self.graph)
        self.dream = DreamModule(self.graph)
        
        # État global
        self.E_global = 0.0
        self.wisdom = 0.0  # S'accumule avec le temps
        self.emotions_history = []
        
        # RabbitMQ
        self.rabbitmq_config = rabbitmq_config
        self.setup_rabbitmq()
    
    def setup_rabbitmq(self):
        """Configure les connexions RabbitMQ"""
        credentials = pika.PlainCredentials(
            self.rabbitmq_config['user'],
            self.rabbitmq_config['password']
        )
        self.connection = pika.BlockingConnection(
            pika.ConnectionParameters(
                host=self.rabbitmq_config['host'],
                credentials=credentials
            )
        )
        self.channel = self.connection.channel()
        
        # Déclarer l'exchange pour recevoir les émotions du C++
        self.channel.exchange_declare(
            exchange='mcee.emotional.input',
            exchange_type='topic',
            durable=True
        )
        
        # Queue pour recevoir
        result = self.channel.queue_declare(queue='', exclusive=True)
        self.queue_name = result.method.queue
        self.channel.queue_bind(
            exchange='mcee.emotional.input',
            queue=self.queue_name,
            routing_key='emotions.predictions'
        )
    
    def on_emotions_received(self, ch, method, properties, body):
        """
        Callback appelé à la réception des émotions du C++
        """
        try:
            emotions = json.loads(body.decode('utf-8'))
            self.process_emotions(emotions)
        except Exception as e:
            print(f"Erreur traitement: {e}")
    
    def process_emotions(self, emotions_raw):
        """
        Traitement principal : MCEE + Mémoire + Amyghaleon
        
        Args:
            emotions_raw: dict[str, float], 24 émotions du C++
        """
        print(f"\n{'='*60}")
        print(f"🧠 Traitement MCEE - {datetime.now()}")
        print(f"{'='*60}")
        
        # 1. Récupérer les souvenirs pertinents
        souvenirs = self.memory_manager.query_relevant_memories(emotions_raw)
        print(f"📚 {len(souvenirs)} souvenirs activés")
        
        # 2. CHECK AMYGHALEON (prioritaire)
        if self.amyghaleon.check_emergency(emotions_raw, souvenirs):
            print("🚨 URGENCE DÉTECTÉE - AMYGHALEON ACTIVÉ")
            response = self.amyghaleon.trigger_emergency_response(
                emotions_raw, 
                souvenirs
            )
            print(f"   Action: {response['action']} (priorité: {response['priority']})")
            self.execute_emergency_action(response)
            return  # Court-circuit
        
        # 3. Mise à jour MCEE normale
        influence_memories = self.memory_manager.compute_influence(
            souvenirs, 
            emotions_raw
        )
        
        emotions_updated = {}
        for emo_name, emo_value in emotions_raw.items():
            # Feedbacks (à implémenter selon vos capteurs)
            fb_ext = self.get_external_feedback()
            fb_int = self.get_internal_feedback()
            delta_t = 1.0  # 1 pas de temps
            
            # Mise à jour individuelle
            emotions_updated[emo_name] = self.emotion_updater.update_emotion(
                E_current=emo_value,
                fb_ext=fb_ext,
                fb_int=fb_int,
                delta_t=delta_t,
                influence_memories=influence_memories,
                wisdom=self.wisdom
            )
        
        print(f"✅ Émotions mises à jour: {len(emotions_updated)}")
        
        # 4. Calcul de la variance
        variances = self.memory_manager.compute_variances(
            emotions_updated, 
            souvenirs
        )
        var_global = np.mean(list(variances.values()))
        print(f"📊 Variance globale: {var_global:.3f}")
        
        # 5. Fusion des émotions
        self.E_global = self.emotion_updater.fuse_emotions(
            self.E_global,
            emotions_updated,
            var_global
        )
        print(f"🎯 Émotion globale: {self.E_global:.3f}")
        
        # 6. Enregistrement en mémoire
        self.memory_manager.record_new_memory(
            emotions_updated,
            self.E_global,
            context=self.get_current_context()
        )
        
        # 7. Mise à jour du graphe (renforcement, oubli)
        self.memory_manager.update_memory_graph(souvenirs)
        
        # 8. Incrémenter la sagesse
        self.wisdom += 0.001
        
        # 9. Historique
        self.emotions_history.append({
            'timestamp': datetime.now(),
            'emotions': emotions_updated,
            'E_global': self.E_global
        })
        
        print(f"{'='*60}\n")
    
    def get_external_feedback(self):
        """À implémenter selon vos capteurs"""
        return 0.0
    
    def get_internal_feedback(self):
        """À implémenter selon votre système interne"""
        return 0.0
    
    def get_current_context(self):
        """Récupère le contexte actuel"""
        return {
            'timestamp': datetime.now(),
            'location': 'unknown',
            'activity': 'unknown'
        }
    
    def execute_emergency_action(self, response):
        """Exécute une action d'urgence"""
        # À implémenter selon votre système
        print(f"Exécution: {response}")
    
    def run_dream_cycle(self):
        """Lance un cycle de rêve (consolidation)"""
        print("\n💤 Cycle de rêve - Consolidation des souvenirs")
        yesterday = (datetime.now() - timedelta(days=1)).strftime('%Y-%m-%d')
        self.dream.consolidate_daily_memories(yesterday)
        print("✅ Consolidation terminée\n")
    
    def start(self):
        """Démarre la boucle principale"""
        print("🚀 MCEE Engine démarré")
        
        # Consumer RabbitMQ
        self.channel.basic_consume(
            queue=self.queue_name,
            on_message_callback=self.on_emotions_received,
            auto_ack=True
        )
        
        print("⏳ En attente d'émotions...\n")
        try:
            self.channel.start_consuming()
        except KeyboardInterrupt:
            print("\n⛔ Arrêt demandé")
            self.connection.close()


if __name__ == "__main__":
    config = {
        'neo4j': {
            'uri': 'bolt://localhost:7687',
            'user': 'neo4j',
            'pass': 'password'
        },
        'rabbitmq': {
            'host': 'localhost',
            'user': 'virtus',
            'password': 'virtus@83'
        }
    }
    
    engine = MCEEEngine(
        neo4j_uri=config['neo4j']['uri'],
        neo4j_user=config['neo4j']['user'],
        neo4j_pass=config['neo4j']['pass'],
        rabbitmq_config=config['rabbitmq']
    )
    
    # Lancer un cycle de rêve toutes les 24h (dans un thread séparé)
    # engine.run_dream_cycle()
    
    engine.start()
```

---

## 7. Points d'Attention pour le Développement

### 7.1 Calibration des coefficients

Les coefficients α, β, γ, δ, θ doivent être calibrés empiriquement :

```python
# Exemple de configuration
COEFFICIENTS = {
    'alpha': 0.3,    # Feedback externe
    'beta': 0.2,     # Feedback interne
    'gamma': 0.1,    # Décroissance temporelle
    'delta': 0.4,    # Influence souvenirs (IMPORTANT)
    'theta': 0.05    # Sagesse
}
```

**Recommandation :** Créer un système de configuration JSON pour ajuster facilement.

### 7.2 Performance Neo4j

Pour éviter les requêtes lentes :

1. **Index sur les propriétés clés** :
```cypher
CREATE INDEX souvenir_date FOR (s:Souvenir) ON (s.date)
CREATE INDEX souvenir_dominant FOR (s:Souvenir) ON (s.dominant)
CREATE INDEX souvenir_state FOR (s:Souvenir) ON (s.state)
```

2. **Limiter les résultats** : Toujours utiliser `LIMIT` dans les requêtes

3. **Batch updates** : Grouper les mises à jour plutôt que des requêtes individuelles

### 7.3 Gestion des erreurs

```python
# Wrapper pour les requêtes Neo4j
def safe_neo4j_query(func):
    def wrapper(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except Exception as e:
            print(f"Erreur Neo4j: {e}")
            return None
    return wrapper
```

### 7.4 Tests

Créer des tests unitaires pour chaque module :

```python
# test_emotion_updater.py
def test_emotion_update():
    updater = EmotionUpdater()
    E_next = updater.update_emotion(
        E_current=0.5,
        fb_ext=0.2,
        fb_int=0.1,
        delta_t=1.0,
        influence_memories=0.3,
        wisdom=0.05
    )
    assert 0.0 <= E_next <= 1.0

# test_memory_manager.py
def test_compute_influence():
    # Mock des souvenirs
    souvenirs = [
        {'emotions': [0.7]*24, 'valence': 0.6, 'activation': 0.8}
    ]
    influence = compute_memory_influence(souvenirs, [0.5]*24, 1.0)
    assert isinstance(influence, float)
```

---

## 8. Roadmap de Développement

### Phase 1 : Base (2-3 semaines)
- [ ] Implémenter `EmotionUpdater` avec formules de base
- [ ] Créer `MemoryManager` pour requêtes Neo4j
- [ ] Intégration RabbitMQ (recevoir depuis C++)
- [ ] Tests unitaires de base

### Phase 2 : Mémoire (2-3 semaines)
- [ ] Calcul d'activation des souvenirs
- [ ] Gestion MCT/MLT
- [ ] Enregistrement quotidien avec `SouvenirConsolider`
- [ ] Tests d'intégration avec Neo4j

### Phase 3 : Avancé (2-3 semaines)
- [ ] Module Amyghaleon (détection urgence)
- [ ] Module Rêve (consolidation)
- [ ] Gestion des traumas
- [ ] Interface de monitoring

### Phase 4 : Optimisation (1-2 semaines)
- [ ] Calibration des coefficients
- [ ] Performance Neo4j (index, optimisation requêtes)
- [ ] Tests de charge
- [ ] Documentation complète

---

## 9. Dépendances Python

```txt
# requirements_mcee.txt
pika>=1.3.0
py2neo>=2021.2.3
numpy>=1.21.0
scikit-learn>=1.0.0
python-dateutil>=2.8.0
```

---

## 10. Ressources Supplémentaires

### Formules résumées
```
E_i(t+1) = E_i(t) + α·Fb_ext + β·Fb_int - γ·Δt + δ·Influence + θ·W_t
Var_i(t) = (1/m) Σ[E_i(t) - S_i,j]²
E_global(t+1) = tanh(E_global(t) + Σ[E_i(t+1)·(1 - Var_global)])
A(S_i) = forget × (1 + R) × Σ[C × Me × U]
```

### Structure Neo4j minimale
```cypher
// Nœuds
(:Souvenir {emotions, dominant, valence, intensity, state})
(:Trauma {emotion_signature, intensity, forget_rate})
(:Concept {name, keywords})

// Relations
(s)-[:EVOQUE]->(c)
(s)-[:CONTEXTE]->(c)
(t)-[:CONCERNE]->(s)
(c1)-[:ASSOCIE_A {emotions, weight}]->(c2)
```

### Variables clés à persister
- `E_global` : État émotionnel global actuel
- `wisdom` : Sagesse accumulée
- `emotions_history` : Historique pour analyse
- Souvenirs avec état `SouvenirConsolider`

---

**Fin du résumé technique**
