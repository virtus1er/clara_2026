# MCEE - Résumé Technique pour le Développement (avec Système de Phases)

## 🎯 Vue d'ensemble

Le MCEE (Modèle Complet d'Évaluation des États) est un système émotionnel complet intégrant :
- **24 émotions instantanées** (prédites par le module C++)
- **🎭 Système de phases émotionnelles** (8 phases qui modulent le comportement)
- **Graphe de mémoire Neo4j** (souvenirs, concepts, traumas)
- **Mécanismes de fusion et modulation** (adaptatifs selon la phase)
- **Système d'urgence "Amyghaleon"** (déclenché selon la phase)

---

## 1. Architecture des Composants (avec Phases)

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
┌─────────────────────────────────────────────────────┐
│  🎭 Phase Detector                                  │
│  Détecte la phase émotionnelle actuelle             │
│                                                      │
│  Input:  24 émotions                                │
│  Output: Phase + PhaseConfig (α,β,γ,δ,θ,seuils)    │
│                                                      │
│  Phases: SERENITE | JOIE | EXPLORATION | ANXIETE   │
│          PEUR | TRISTESSE | DEGOUT | CONFUSION      │
└────────┬────────────────────────────────────────────┘
         ↓
┌─────────────────┐
│  MCEE Engine    │ → Mise à jour E_i(t+1) avec coefficients de phase
│  (Python)       │    Fusion → E_global(t+1)
│                 │    Applique les paramètres de la phase active
└────────┬────────┘
         ↓
┌─────────────────┐
│  Neo4j Graphe   │ → Souvenirs, concepts, traumas
│  Mémoire        │    Activation, oubli, renforcement
│                 │    Consolidation modulée par phase
└────────┬────────┘
         ↓
┌─────────────────┐
│  Amyghaleon     │ → Réactions d'urgence (seuil selon phase)
│  (court-circuit)│    Phase PEUR: seuil = 0.50 ⚠️
│                 │    Phase SERENITE: seuil = 0.85 ✅
└─────────────────┘
```

### 1.1 Flux de données détaillé

```
1. Module C++ émet 24 émotions → RabbitMQ (mcee.emotional.input)
2. Phase Detector reçoit et analyse → Détecte phase actuelle
3. Phase Detector fournit PhaseConfig → Coefficients adaptés
4. MCEE Engine reçoit:
   - Les 24 émotions brutes
   - La phase actuelle
   - Les coefficients (α,β,γ,δ,θ) de cette phase
   - Les seuils (Amyghaleon, consolidation, etc.)
5. MCEE Engine applique les formules avec les coefficients de phase
6. Neo4j est mis à jour selon les paramètres de phase
7. Amyghaleon vérifie le seuil de la phase actuelle
```

---

## 2. Système de Phases Émotionnelles 🎭

### 2.1 Les 8 Phases

Le système MCEE fonctionne en **8 phases émotionnelles** qui modulent tous les paramètres du système.

| Phase | Priorité | Trigger | α | δ | γ | θ | Seuil Amyg. | Caractéristiques |
|-------|----------|---------|---|---|---|---|-------------|------------------|
| **SÉRÉNITÉ** | 1 | Calme>0.5 | 0.25 | 0.30 | 0.12 | 0.10 | 0.85 | Équilibre, apprentissage optimal |
| **JOIE** | 2 | Joie>0.6 | 0.40 | 0.35 | 0.08 | 0.05 | 0.95 | Euphorie, renforcement positif |
| **EXPLORATION** | 2 | Intérêt>0.6 | 0.35 | 0.25 | 0.10 | 0.15 | 0.80 | Apprentissage maximal |
| **ANXIÉTÉ** | 3 | Anxiété 0.5-0.8 | 0.40 | 0.45 | 0.06 | 0.08 | 0.70 | Hypervigilance, biais négatif |
| **PEUR** ⚠️ | 5 | Peur>0.8 | 0.60 | 0.70 | 0.02 | 0.02 | 0.50 | 🚨 URGENCE - Traumas dominants |
| **TRISTESSE** | 3 | Tristesse>0.6 | 0.20 | 0.55 | 0.05 | 0.12 | 0.90 | Rumination, introspection |
| **DÉGOÛT** | 4 | Dégoût>0.6 | 0.50 | 0.40 | 0.08 | 0.08 | 0.75 | Évitement, associations négatives |
| **CONFUSION** | 2 | Confusion>0.6 | 0.35 | 0.50 | 0.15 | 0.15 | 0.80 | Recherche d'info, incertitude |

### 2.2 Transitions entre phases

**Règles de transition :**
1. **Priorité** : Phase PEUR (priorité 5) court-circuite toutes les autres
2. **Hysteresis** : Marge de 0.15 pour éviter oscillations
3. **Durée minimale** : 30 secondes avant changement (configurable)
4. **Urgence** : Peur > 0.85 OU Horreur > 0.8 → transition IMMÉDIATE

**Exemple de séquence :**
```
SÉRÉNITÉ (120s) → EXPLORATION (45s) → JOIE (60s) → ANXIÉTÉ (35s) → PEUR (15s)
                                                                        ↓
                                                              AMYGHALEON ACTIVÉ
```

### 2.3 Impact des phases sur le MCEE

Chaque phase modifie :
- ✅ **Coefficients MCEE** (α,β,γ,δ,θ) → Comportement des émotions
- ✅ **Seuil Amyghaleon** → Sensibilité aux urgences
- ✅ **Consolidation mémoire** → Force de mémorisation
- ✅ **Focus attentionnel** → Filtrage des stimuli
- ✅ **Taux d'apprentissage** → Vitesse d'adaptation

---

## 3. Formules Clés à Implémenter (avec Phases)

### 3.1 Mise à jour des émotions individuelles (coefficients de phase)

```python
E_i(t+1) = E_i(t) + α_phase·Fb_ext + β_phase·Fb_int(t) - γ_phase·Δt + δ_phase·Influence_Souvenirs + θ_phase·W_t
```

**⚠️ CHANGEMENT CLÉS :**
- Les coefficients `α, β, γ, δ, θ` sont maintenant **fournis par la phase active**
- Ils ne sont plus fixes mais **changent dynamiquement** selon l'état émotionnel

**Variables :**
- `E_i(t)` : Émotion i actuelle (du module C++)
- `α_phase, β_phase, γ_phase, δ_phase, θ_phase` : **Coefficients de la phase active**
- `Fb_ext` : Feedback externe (utilisateur, environnement)
- `Fb_int` : Feedback interne (énergie, tension, auto-évaluation)
- `Δt` : Delta temps (décroissance naturelle)
- `W_t` : Coefficient de sagesse (expérience accumulée)
- `Influence_Souvenirs` : Calculé depuis Neo4j

**Implémentation avec phases :**
```python
class EmotionUpdater:
    def __init__(self):
        # Les coefficients sont maintenant dynamiques
        self.alpha = 0.3  # Valeur par défaut (sera écrasée)
        self.beta = 0.2
        self.gamma = 0.1
        self.delta = 0.4
        self.theta = 0.1
    
    def set_coefficients_from_phase(self, phase_config):
        """
        Met à jour les coefficients selon la phase active.
        
        Args:
            phase_config: PhaseConfig contenant les coefficients
        """
        self.alpha = phase_config['alpha']
        self.beta = phase_config['beta']
        self.gamma = phase_config['gamma']
        self.delta = phase_config['delta']
        self.theta = phase_config['theta']
        
        print(f"🎭 Coefficients mis à jour pour phase:")
        print(f"   α={self.alpha:.2f}, β={self.beta:.2f}, γ={self.gamma:.2f}")
        print(f"   δ={self.delta:.2f}, θ={self.theta:.2f}")
    
    def update_emotion(self, E_current, fb_ext, fb_int, delta_t, 
                       influence_memories, wisdom):
        """
        Met à jour une émotion avec les coefficients de la phase active.
        """
        E_next = (E_current + 
                  self.alpha * fb_ext +
                  self.beta * fb_int -
                  self.gamma * delta_t +
                  self.delta * influence_memories +
                  self.theta * wisdom)
        return np.clip(E_next, 0.0, 1.0)
```

### 3.2 Exemples concrets par phase

#### Phase SÉRÉNITÉ
```python
# Coefficients équilibrés
α = 0.25  # Feedback externe modéré
β = 0.15  # Feedback interne bas
γ = 0.12  # Décroissance normale
δ = 0.30  # Souvenirs modérés
θ = 0.10  # Sagesse active

# Résultat: Apprentissage stable, décisions posées
```

#### Phase PEUR ⚠️
```python
# Coefficients d'urgence
α = 0.60  # Feedback externe MAXIMAL (danger!)
β = 0.45  # Feedback interne ÉLEVÉ (stress)
γ = 0.02  # Décroissance TRÈS LENTE (état persistant)
δ = 0.70  # Souvenirs DOMINANTS (traumas activés)
θ = 0.02  # Sagesse QUASI ABSENTE (réflexes)

# Résultat: Réaction d'urgence, traumas activés, pas d'apprentissage rationnel
```

#### Phase EXPLORATION
```python
# Coefficients d'apprentissage
α = 0.35  # Feedback externe élevé (perception)
β = 0.10  # Feedback interne bas (focus externe)
γ = 0.10  # Décroissance normale
δ = 0.25  # Souvenirs moins influents (nouveauté)
θ = 0.15  # Sagesse ÉLEVÉE (apprentissage)

# Résultat: Apprentissage maximal, attention focalisée
```

### 3.3 Variance (détection d'anomalies)

```python
Var_i(t) = (1/m) * Σ[E_i(t) - S_i,j]²
```

**Inchangé**, mais son interprétation dépend de la phase :
- **Phase ANXIÉTÉ** : Variance élevée = menace potentielle
- **Phase EXPLORATION** : Variance élevée = nouveauté intéressante
- **Phase PEUR** : Variance ignorée (réaction immédiate)

### 3.4 Fusion des émotions (avec stabilisation tanh)

```python
E_global(t+1) = tanh(E_global(t) + Σ[E_i(t+1) · (1 - Var_global(t))])
```

**Inchangé dans la formule**, mais les `E_i(t+1)` sont calculés avec les coefficients de phase.

---

## 4. Gestion de la Mémoire (Neo4j) - Modulée par Phase

### 4.1 Structure des nœuds (inchangée)

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
    state: 'SouvenirConsolider',
    phase_at_creation: 'JOIE'  // 🆕 Phase lors de la création
})
```

### 4.2 Calcul de l'Influence des Souvenirs (modulé par phase)

**Formule d'activation (identique) :**
```python
A(S_i) = forget(S_i, t) × (1 + R(S_i)) × Σ[C(S_i, S_k) × Me(S_i, E_current) × U(S_i)]
```

**Mais :**
- **Phase PEUR** : Le coefficient `δ = 0.70` amplifie massivement l'influence
- **Phase ANXIÉTÉ** : `δ = 0.45`, souvenirs anxiogènes activés en priorité
- **Phase SÉRÉNITÉ** : `δ = 0.30`, influence équilibrée

**Requête Neo4j adaptée selon la phase :**
```python
def query_relevant_memories(phase, emotions):
    """
    Récupère les souvenirs pertinents selon la phase active.
    """
    if phase == 'PEUR':
        # Priorité aux traumas et souvenirs de peur
        query = """
        MATCH (s:Souvenir)
        WHERE s.dominant IN ['Peur', 'Horreur', 'Anxiété']
           OR EXISTS((s)<-[:CONCERNE]-(t:Trauma))
        RETURN s
        ORDER BY s.intensity DESC, s.weight DESC
        LIMIT 20
        """
    
    elif phase == 'JOIE':
        # Priorité aux souvenirs positifs
        query = """
        MATCH (s:Souvenir)
        WHERE s.valence > 0.5
          AND s.dominant IN ['Joie', 'Satisfaction', 'Excitation']
        RETURN s
        ORDER BY s.valence DESC
        LIMIT 10
        """
    
    elif phase == 'ANXIETE':
        # Souvenirs négatifs récents
        query = """
        MATCH (s:Souvenir)
        WHERE s.dominant IN ['Anxiété', 'Peur', 'Confusion']
          AND s.last_activated > datetime() - duration({days: 30})
        RETURN s
        ORDER BY s.activation_count DESC
        LIMIT 15
        """
    
    else:  # SERENITE, EXPLORATION, etc.
        # Requête équilibrée
        query = """
        MATCH (s:Souvenir)
        WHERE s.last_activated > datetime() - duration({days: 30})
        RETURN s
        ORDER BY s.weight DESC
        LIMIT 10
        """
    
    return neo4j.run(query)
```

### 4.3 Consolidation modulée par phase

**Processus de consolidation adapté :**
```python
def should_consolidate(souvenir, phase_at_creation):
    """
    Décide si un souvenir doit être consolidé selon la phase
    où il a été créé.
    """
    intensity = souvenir['intensity']
    valence = souvenir['valence']
    
    # Phase PEUR: consolidation automatique (trauma potentiel)
    if phase_at_creation == 'PEUR':
        if intensity > 0.85 and valence < 0.2:
            return 'TRAUMA'  # Consolider immédiatement en trauma
        return 'CONSOLIDATE_STRONG'
    
    # Phase JOIE: consolidation forte des souvenirs positifs
    elif phase_at_creation == 'JOIE':
        if valence > 0.7 and intensity > 0.6:
            return 'CONSOLIDATE_STRONG'
        return 'CONSOLIDATE_NORMAL'
    
    # Phase ANXIETE: consolidation sélective (renforce biais négatif)
    elif phase_at_creation == 'ANXIETE':
        if valence < 0.3:
            return 'CONSOLIDATE_STRONG'  # Renforce souvenirs négatifs
        return 'FORGET'  # Oublie les positifs
    
    # Autres phases: consolidation normale
    else:
        if intensity > 0.5:
            return 'CONSOLIDATE_NORMAL'
        return 'FORGET'
```

---

## 5. Système d'Urgence "Amyghaleon" (Seuils de Phase)

### 5.1 Principe

Le seuil d'activation d'Amyghaleon **dépend de la phase active** :

```python
AMYGHALEON_THRESHOLDS = {
    'SERENITE':    0.85,  # Difficile à déclencher
    'JOIE':        0.95,  # Très difficile (euphorie)
    'EXPLORATION': 0.80,  # Modéré
    'ANXIETE':     0.70,  # Facile (déjà vigilant)
    'PEUR':        0.50,  # ⚠️ TRÈS FACILE (hypersensible)
    'TRISTESSE':   0.90,  # Difficile (état dépressif)
    'DEGOUT':      0.75,  # Modéré
    'CONFUSION':   0.80   # Modéré
}
```

### 5.2 Implémentation

```python
class Amyghaleon:
    CRITICAL_EMOTIONS = ['Peur', 'Horreur', 'Anxiété']
    
    def check_emergency(self, emotions, souvenirs_actives, phase_threshold):
        """
        Vérifie si une urgence est détectée avec le seuil de la phase.
        
        Args:
            emotions: dict, 24 émotions actuelles
            souvenirs_actives: list, souvenirs activés
            phase_threshold: float, seuil de la phase active (0.50 à 0.95)
        """
        # 1. Vérifier les émotions critiques
        max_critical = max([emotions.get(e, 0) for e in self.CRITICAL_EMOTIONS])
        
        if max_critical > phase_threshold:
            print(f"🚨 Amyghaleon: Émotion critique {max_critical:.3f} > seuil {phase_threshold:.3f}")
            return True
        
        # 2. Vérifier les traumas activés
        for s in souvenirs_actives:
            if s.get('trauma', False) and s['activation'] > (phase_threshold - 0.2):
                print(f"🚨 Amyghaleon: Trauma activé {s['name']}")
                return True
        
        # 3. Combinaison critique + trauma
        if max_critical > (phase_threshold + 0.2):
            for s in souvenirs_actives:
                if s.get('trauma') and s['activation'] > 0.6:
                    print(f"🚨 Amyghaleon: Combinaison critique+trauma")
                    return True
        
        return False
    
    def trigger_emergency_response(self, emotions, phase):
        """
        Déclenche une réponse d'urgence adaptée à la phase.
        """
        max_emo = max(emotions.items(), key=lambda x: x[1])
        
        responses = {
            'Peur': {'action': 'FUITE', 'priority': 'CRITIQUE'},
            'Horreur': {'action': 'BLOCAGE', 'priority': 'CRITIQUE'},
            'Anxiété': {'action': 'ALERTE', 'priority': 'ÉLEVÉE'}
        }
        
        response = responses.get(max_emo[0], {'action': 'SURVEILLANCE', 'priority': 'MOYENNE'})
        response['phase_at_trigger'] = phase
        response['emotion_value'] = max_emo[1]
        
        return response
```

### 5.3 Exemple : Phase PEUR vs Phase SÉRÉNITÉ

**Situation identique : Peur = 0.65**

```python
# Phase SÉRÉNITÉ (seuil = 0.85)
if 0.65 > 0.85:  # False
    # Pas d'urgence, traitement normal

# Phase PEUR (seuil = 0.50)
if 0.65 > 0.50:  # True
    # 🚨 URGENCE DÉTECTÉE
    # Court-circuit MCEE
    # Action immédiate : FUITE/BLOCAGE
```

---

## 6. Implémentation Complète du MCEE Engine (avec Phases)

### 6.1 Classe principale mise à jour

```python
import pika
import numpy as np
from datetime import datetime, timedelta
from py2neo import Graph
from sklearn.metrics.pairwise import cosine_similarity

class MCEEEngine:
    def __init__(self, neo4j_uri, neo4j_user, neo4j_pass, rabbitmq_config):
        # Connexion Neo4j
        self.graph = Graph(neo4j_uri, auth=(neo4j_user, neo4j_pass))
        
        # 🆕 Détecteur de phase (peut être externe ou intégré)
        self.phase_detector = PhaseDetector(hysteresis_margin=0.15, 
                                           min_phase_duration=30.0)
        
        # Modules MCEE
        self.emotion_updater = EmotionUpdater()
        self.memory_manager = MemoryManager(self.graph)
        self.amyghaleon = Amyghaleon()
        self.dream = DreamModule(self.graph)
        
        # État
        self.E_global = 0.0
        self.wisdom = 0.0
        self.emotions_history = []
        self.current_phase = 'SERENITE'  # 🆕
        
        # RabbitMQ
        credentials = pika.PlainCredentials(
            rabbitmq_config['user'],
            rabbitmq_config['password']
        )
        parameters = pika.ConnectionParameters(
            host=rabbitmq_config['host'],
            credentials=credentials
        )
        self.connection = pika.BlockingConnection(parameters)
        self.channel = self.connection.channel()
        
        # Déclarer l'exchange
        self.channel.exchange_declare(
            exchange='mcee.emotional.input',
            exchange_type='topic',
            durable=True
        )
        
        # Queue
        result = self.channel.queue_declare(queue='', exclusive=True)
        self.queue_name = result.method.queue
        
        self.channel.queue_bind(
            exchange='mcee.emotional.input',
            queue=self.queue_name,
            routing_key='emotions.predictions'
        )
        
        print("✅ MCEE Engine initialisé avec système de phases")
    
    def on_emotions_received(self, ch, method, properties, body):
        """
        Callback appelé quand des émotions sont reçues du module C++.
        """
        import json
        emotions_raw = json.loads(body.decode('utf-8'))
        
        print(f"\n{'='*60}")
        print(f"📥 Émotions reçues: {len(emotions_raw)} émotions")
        print(f"{'='*60}")
        
        # 🆕 1. DÉTECTION DE PHASE
        previous_phase = self.current_phase
        self.current_phase = self.phase_detector.detect_phase(emotions_raw)
        
        if self.current_phase != previous_phase:
            print(f"🔄 Transition de phase: {previous_phase} → {self.current_phase}")
        
        # 🆕 2. RÉCUPÉRER LA CONFIGURATION DE LA PHASE
        phase_config = self.phase_detector.get_phase_config()
        
        print(f"🎭 Phase active: {self.current_phase}")
        print(f"   Coefficients: α={phase_config['alpha']:.2f}, "
              f"β={phase_config['beta']:.2f}, γ={phase_config['gamma']:.2f}, "
              f"δ={phase_config['delta']:.2f}, θ={phase_config['theta']:.2f}")
        print(f"   Seuil Amyghaleon: {phase_config['amyghaleon_threshold']:.2f}")
        
        # 🆕 3. METTRE À JOUR LES COEFFICIENTS MCEE
        self.emotion_updater.set_coefficients_from_phase(phase_config)
        
        # 4. RÉCUPÉRER LES SOUVENIRS (adapté selon phase)
        souvenirs = self.memory_manager.query_relevant_memories(
            phase=self.current_phase,
            emotions=emotions_raw
        )
        print(f"💭 Souvenirs activés: {len(souvenirs)}")
        
        # 🆕 5. VÉRIFIER AMYGHALEON (seuil de phase)
        amyghaleon_threshold = phase_config['amyghaleon_threshold']
        
        if self.amyghaleon.check_emergency(emotions_raw, souvenirs, amyghaleon_threshold):
            print(f"🚨 AMYGHALEON DÉCLENCHÉ (seuil={amyghaleon_threshold:.2f})")
            response = self.amyghaleon.trigger_emergency_response(
                emotions_raw, 
                self.current_phase
            )
            self.execute_emergency_action(response)
            return  # Court-circuit
        
        # 6. CALCUL INFLUENCE MÉMOIRE
        influence_memories = self.memory_manager.compute_memory_influence(
            souvenirs,
            emotions_raw
        )
        
        # 7. MISE À JOUR DES ÉMOTIONS (avec coefficients de phase)
        emotions_updated = {}
        for emo_name, emo_value in emotions_raw.items():
            fb_ext = self.get_external_feedback()
            fb_int = self.get_internal_feedback()
            delta_t = 1.0
            
            emotions_updated[emo_name] = self.emotion_updater.update_emotion(
                E_current=emo_value,
                fb_ext=fb_ext,
                fb_int=fb_int,
                delta_t=delta_t,
                influence_memories=influence_memories,
                wisdom=self.wisdom
            )
        
        print(f"✅ Émotions mises à jour avec coefficients de phase {self.current_phase}")
        
        # 8. VARIANCE
        variances = self.memory_manager.compute_variances(emotions_updated, souvenirs)
        var_global = np.mean(list(variances.values()))
        print(f"📊 Variance globale: {var_global:.3f}")
        
        # 9. FUSION
        self.E_global = self.emotion_updater.fuse_emotions(
            self.E_global,
            emotions_updated,
            var_global
        )
        print(f"🎯 Émotion globale: {self.E_global:.3f}")
        
        # 🆕 10. ENREGISTREMENT EN MÉMOIRE (avec phase)
        self.memory_manager.record_new_memory(
            emotions_updated,
            self.E_global,
            phase=self.current_phase,  # 🆕 Enregistrer la phase
            context=self.get_current_context()
        )
        
        # 11. MISE À JOUR GRAPHE
        self.memory_manager.update_memory_graph(souvenirs)
        
        # 12. SAGESSE
        self.wisdom += 0.001
        
        # 13. HISTORIQUE
        self.emotions_history.append({
            'timestamp': datetime.now(),
            'emotions': emotions_updated,
            'E_global': self.E_global,
            'phase': self.current_phase  # 🆕
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
        print(f"⚠️ Action d'urgence: {response['action']} (priorité: {response['priority']})")
        print(f"   Phase au déclenchement: {response['phase_at_trigger']}")
        print(f"   Valeur émotionnelle: {response['emotion_value']:.3f}")
        
        # 🆕 Si phase PEUR, créer un trauma potentiel
        if response['phase_at_trigger'] == 'PEUR':
            self.memory_manager.create_potential_trauma(
                self.emotions_history[-1] if self.emotions_history else None
            )
    
    def run_dream_cycle(self):
        """Lance un cycle de rêve (consolidation)"""
        print("\n💤 Cycle de rêve - Consolidation des souvenirs")
        yesterday = (datetime.now() - timedelta(days=1)).strftime('%Y-%m-%d')
        self.dream.consolidate_daily_memories(yesterday)
        print("✅ Consolidation terminée\n")
    
    def start(self):
        """Démarre la boucle principale"""
        print("🚀 MCEE Engine démarré avec système de phases")
        
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
```

### 6.2 Détecteur de phases (peut être externe)

```python
class PhaseDetector:
    """
    Détecteur de phases émotionnelles.
    Peut être implémenté en Python ou utiliser le module C++.
    """
    
    PHASE_CONFIGS = {
        'SERENITE': {
            'alpha': 0.25, 'beta': 0.15, 'gamma': 0.12, 'delta': 0.30, 'theta': 0.10,
            'amyghaleon_threshold': 0.85,
            'memory_consolidation': 0.4,
            'learning_rate': 1.0,
            'priority': 1
        },
        'PEUR': {
            'alpha': 0.60, 'beta': 0.45, 'gamma': 0.02, 'delta': 0.70, 'theta': 0.02,
            'amyghaleon_threshold': 0.50,
            'memory_consolidation': 0.8,
            'learning_rate': 0.3,
            'priority': 5
        },
        # ... autres phases
    }
    
    def __init__(self, hysteresis_margin=0.15, min_phase_duration=30.0):
        self.current_phase = 'SERENITE'
        self.phase_start_time = datetime.now()
        self.hysteresis_margin = hysteresis_margin
        self.min_phase_duration = timedelta(seconds=min_phase_duration)
    
    def detect_phase(self, emotions):
        """
        Détecte la phase actuelle selon les émotions.
        
        Args:
            emotions: dict[str, float], 24 émotions
        
        Returns:
            str, nom de la phase détectée
        """
        # Calculer les scores de chaque phase
        scores = self._compute_phase_scores(emotions)
        
        # Trouver la meilleure
        best_phase = max(scores.items(), key=lambda x: x[1])[0]
        
        # Vérifier urgence
        if best_phase == 'PEUR' and (emotions.get('Peur', 0) > 0.85 or 
                                     emotions.get('Horreur', 0) > 0.8):
            self._transition_to(best_phase, reason="URGENCE")
            return best_phase
        
        # Appliquer hysteresis
        current_score = scores.get(self.current_phase, 0.0)
        best_score = scores[best_phase]
        
        if current_score > best_score - self.hysteresis_margin:
            return self.current_phase
        
        # Vérifier durée minimale
        time_in_phase = datetime.now() - self.phase_start_time
        if time_in_phase < self.min_phase_duration:
            return self.current_phase
        
        # Transition
        self._transition_to(best_phase, reason="SCORE_SUPERIEUR")
        return best_phase
    
    def _compute_phase_scores(self, emotions):
        """Calcule le score de chaque phase"""
        scores = {}
        
        # SERENITE
        if emotions.get('Calme', 0) > 0.5 and emotions.get('Satisfaction', 0) > 0.4:
            scores['SERENITE'] = self.PHASE_CONFIGS['SERENITE']['priority'] + 0.5
        
        # PEUR
        if emotions.get('Peur', 0) > 0.8 or emotions.get('Horreur', 0) > 0.7:
            scores['PEUR'] = self.PHASE_CONFIGS['PEUR']['priority'] + 1.0
        
        # ... autres phases
        
        return scores
    
    def _transition_to(self, new_phase, reason):
        """Effectue une transition de phase"""
        if new_phase != self.current_phase:
            print(f"🔄 Transition: {self.current_phase} → {new_phase} ({reason})")
            self.current_phase = new_phase
            self.phase_start_time = datetime.now()
    
    def get_phase_config(self):
        """Retourne la configuration de la phase actuelle"""
        return self.PHASE_CONFIGS[self.current_phase]
```

---

## 7. Mise à Jour de Neo4j avec Phases

### 7.1 Enregistrement des souvenirs avec phase

```python
class MemoryManager:
    def record_new_memory(self, emotions, E_global, phase, context):
        """
        Enregistre un nouveau souvenir avec la phase active.
        """
        dominant = max(emotions.items(), key=lambda x: x[1])[0]
        valence = self._compute_valence(emotions)
        intensity = np.mean(list(emotions.values()))
        
        query = """
        CREATE (s:Souvenir {
            name: $name,
            date: date($date),
            emotions: $emotions,
            dominant: $dominant,
            valence: $valence,
            intensity: $intensity,
            last_activated: datetime(),
            activation_count: 1,
            weight: $weight,
            state: 'SouvenirConsolider',
            phase_at_creation: $phase,
            E_global: $E_global
        })
        RETURN s
        """
        
        # Poids initial selon phase
        initial_weight = self._get_initial_weight(phase, intensity, valence)
        
        self.graph.run(query,
            name=f"Événement_{context['timestamp'].strftime('%Y%m%d_%H%M%S')}",
            date=context['timestamp'].date().isoformat(),
            emotions=list(emotions.values()),
            dominant=dominant,
            valence=valence,
            intensity=intensity,
            weight=initial_weight,
            phase=phase,
            E_global=E_global
        )
    
    def _get_initial_weight(self, phase, intensity, valence):
        """
        Calcule le poids initial selon la phase.
        """
        # Phase PEUR: poids élevé (consolidation forte)
        if phase == 'PEUR':
            return min(1.0, 0.7 + intensity * 0.3)
        
        # Phase JOIE: poids selon valence
        elif phase == 'JOIE':
            if valence > 0.6:
                return min(1.0, 0.6 + valence * 0.4)
            return 0.3
        
        # Phase ANXIETE: poids négatif renforcé
        elif phase == 'ANXIETE':
            if valence < 0.3:
                return min(1.0, 0.5 + (1 - valence) * 0.5)
            return 0.2
        
        # Autres: poids normal
        else:
            return 0.5
    
    def create_potential_trauma(self, emotion_state):
        """
        Crée un trauma potentiel suite à une phase PEUR.
        """
        if not emotion_state:
            return
        
        query = """
        CREATE (t:Trauma {
            name: $name,
            date: date($date),
            emotion_signature: $emotions,
            intensity: $intensity,
            trauma: true,
            forget_rate: 0.005,
            state: 'TraumaConsolider',
            immediate_transfer: true,
            phase_trigger: 'PEUR'
        })
        RETURN t
        """
        
        intensity = max(emotion_state['emotions'].values())
        
        self.graph.run(query,
            name=f"Trauma_potentiel_{datetime.now().strftime('%Y%m%d_%H%M%S')}",
            date=datetime.now().date().isoformat(),
            emotions=list(emotion_state['emotions'].values()),
            intensity=intensity
        )
        
        print(f"⚠️ Trauma potentiel créé (intensité={intensity:.3f})")
```

### 7.2 Requêtes selon phase

```python
def query_relevant_memories(self, phase, emotions):
    """
    Récupère les souvenirs selon la phase active.
    """
    if phase == 'PEUR':
        query = """
        MATCH (s:Souvenir)
        WHERE s.dominant IN ['Peur', 'Horreur', 'Anxiété']
           OR EXISTS((s)<-[:CONCERNE]-(t:Trauma))
        RETURN s.name, s.emotions, s.weight, s.valence, s.activation_count
        ORDER BY s.intensity DESC, s.weight DESC
        LIMIT 20
        """
    
    elif phase == 'JOIE':
        query = """
        MATCH (s:Souvenir)
        WHERE s.valence > 0.5
          AND s.dominant IN ['Joie', 'Satisfaction', 'Excitation']
        RETURN s.name, s.emotions, s.weight, s.valence, s.activation_count
        ORDER BY s.valence DESC
        LIMIT 10
        """
    
    # ... autres phases
    
    results = self.graph.run(query).data()
    return results
```

---

## 8. Statistiques et Monitoring avec Phases

### 8.1 Enregistrement des phases dans Neo4j

```python
def log_phase_transition(from_phase, to_phase, emotions, duration):
    """
    Enregistre une transition de phase dans Neo4j.
    """
    query = """
    CREATE (t:PhaseTransition {
        from_phase: $from,
        to_phase: $to,
        timestamp: datetime(),
        duration_previous: $duration,
        trigger_emotions: $emotions,
        emotion_max: $emotion_max
    })
    RETURN t
    """
    
    emotion_max = max(emotions.items(), key=lambda x: x[1])
    
    graph.run(query,
        from_phase=from_phase,
        to_phase=to_phase,
        duration=duration,
        emotions=list(emotions.values()),
        emotion_max=f"{emotion_max[0]}={emotion_max[1]:.3f}"
    )
```

### 8.2 Analyse des patterns de phases

```cypher
// Transitions les plus fréquentes
MATCH (t:PhaseTransition)
RETURN t.from_phase, t.to_phase, count(*) as count
ORDER BY count DESC
LIMIT 10

// Durée moyenne par phase
MATCH (t:PhaseTransition)
RETURN t.from_phase, avg(t.duration_previous) as avg_duration
ORDER BY avg_duration DESC

// Phases chroniques (>1h sans changement)
MATCH (t:PhaseTransition)
WHERE t.duration_previous > 3600
RETURN t.from_phase, t.timestamp, t.duration_previous
ORDER BY t.timestamp DESC

// Corrélation phases / souvenirs créés
MATCH (s:Souvenir)
RETURN s.phase_at_creation, count(*) as count, avg(s.intensity) as avg_intensity
ORDER BY count DESC
```

---

## 9. Roadmap de Développement (Mise à Jour)

### Phase 1 : Base avec Phases (2-3 semaines)
- [x] **Détecteur de phases** (Python ou C++)
- [ ] Implémenter `EmotionUpdater` avec coefficients dynamiques
- [ ] Créer `MemoryManager` avec requêtes adaptées par phase
- [ ] Intégration RabbitMQ + Phase Detector
- [ ] Tests unitaires

### Phase 2 : Mémoire Modulée (2-3 semaines)
- [ ] Calcul d'activation avec coefficients de phase
- [ ] Gestion MCT/MLT avec poids selon phase
- [ ] Enregistrement avec `phase_at_creation`
- [ ] Consolidation adaptée par phase
- [ ] Tests d'intégration Neo4j

### Phase 3 : Amyghaleon Adaptatif (2-3 semaines)
- [ ] Seuils dynamiques selon phase
- [ ] Création traumas en phase PEUR
- [ ] Court-circuit avec contexte de phase
- [ ] Module Rêve avec analyse des phases
- [ ] Interface de monitoring

### Phase 4 : Optimisation (1-2 semaines)
- [ ] Calibration coefficients par phase
- [ ] Performance Neo4j (index sur phases)
- [ ] Tests de charge avec changements de phase
- [ ] Documentation complète avec phases

---

## 10. Points Critiques avec Phases

### 10.1 Éviter les boucles infinies

**Problème** : Phase PEUR → active traumas → renforce PEUR → boucle

**Solution** : Durée minimale forcée + décrémentation graduelle
```python
if phase == 'PEUR' and time_in_phase > 60:
    # Forcer décroissance des émotions critiques
    emotions['Peur'] *= 0.95
    emotions['Horreur'] *= 0.95
```

### 10.2 Transitions trop rapides

**Problème** : Oscillations ANXIETE ↔ PEUR

**Solution** : Hysteresis + durée minimale
```python
HYSTERESIS_MARGIN = 0.15  # Marge pour rester dans phase actuelle
MIN_PHASE_DURATION = 30.0  # Secondes minimum
```

### 10.3 Phase PEUR chronique

**Problème** : Reste bloqué en phase PEUR

**Solution** : Seuil de sortie différent + timeout
```python
if phase == 'PEUR' and time_in_phase > 300:  # 5 minutes
    # Forcer transition vers ANXIETE si émotions < 0.6
    if max(emotions['Peur'], emotions['Horreur']) < 0.6:
        force_transition('ANXIETE')
```

---

## 11. Configuration Recommandée

### 11.1 Fichier de configuration JSON

```json
{
  "phases": {
    "serenite": {
      "alpha": 0.25,
      "beta": 0.15,
      "gamma": 0.12,
      "delta": 0.30,
      "theta": 0.10,
      "amyghaleon_threshold": 0.85,
      "memory_consolidation": 0.4,
      "learning_rate": 1.0
    },
    "peur": {
      "alpha": 0.60,
      "beta": 0.45,
      "gamma": 0.02,
      "delta": 0.70,
      "theta": 0.02,
      "amyghaleon_threshold": 0.50,
      "memory_consolidation": 0.8,
      "learning_rate": 0.3
    }
  },
  "phase_detector": {
    "hysteresis_margin": 0.15,
    "min_phase_duration": 30.0,
    "emergency_threshold_peur": 0.85,
    "emergency_threshold_horreur": 0.80
  }
}
```

### 11.2 Chargement dynamique

```python
import json

def load_phase_config(config_file='phase_config.json'):
    with open(config_file, 'r') as f:
        config = json.load(f)
    return config

# Utilisation
config = load_phase_config()
phase_detector = PhaseDetector(
    hysteresis_margin=config['phase_detector']['hysteresis_margin'],
    min_phase_duration=config['phase_detector']['min_phase_duration']
)
```

---

## 12. Résumé des Changements par rapport à la Version Originale

### ✅ Ce qui reste identique
- Structure Neo4j des nœuds et relations
- Formule d'activation des souvenirs : `A(S_i) = forget × (1+R) × Σ[C×Me×U]`
- Formule de fusion : `E_global(t+1) = tanh(...)`
- Module Rêve pour consolidation
- Amyghaleon (principe général)

### 🆕 Ce qui change
| Aspect | Avant | Maintenant |
|--------|-------|------------|
| **Coefficients α,β,γ,δ,θ** | Fixes (0.3, 0.2, 0.1, 0.4, 0.1) | Dynamiques selon phase |
| **Seuil Amyghaleon** | Fixe (0.85) | Variable (0.50 à 0.95) |
| **Requêtes Neo4j** | Génériques | Adaptées par phase |
| **Consolidation** | Uniforme | Selon phase de création |
| **Pipeline** | C++ → MCEE → Neo4j | C++ → **Phase** → MCEE → Neo4j |
| **Souvenirs** | État simple | + `phase_at_creation` |
| **Architecture** | 4 composants | **5 composants** (+ Phase Detector) |

---

## 13. Dépendances Mises à Jour

```txt
# requirements_mcee.txt
pika>=1.3.0
py2neo>=2021.2.3
numpy>=1.21.0
scikit-learn>=1.0.0
python-dateutil>=2.8.0
# 🆕 Pour le détecteur de phases (si version Python)
typing-extensions>=4.0.0
```

---

## 14. Formules Résumées (Version avec Phases)

```
🎭 PHASE ACTIVE → (α_phase, β_phase, γ_phase, δ_phase, θ_phase, seuil_phase)
                                      ↓
E_i(t+1) = E_i(t) + α_phase·Fb_ext + β_phase·Fb_int - γ_phase·Δt + δ_phase·Influence + θ_phase·W_t
                                      ↓
Var_i(t) = (1/m) Σ[E_i(t) - S_i,j]²   (interprétation selon phase)
                                      ↓
E_global(t+1) = tanh(E_global(t) + Σ[E_i(t+1)·(1 - Var_global)])
                                      ↓
A(S_i) = forget × (1 + R) × Σ[C × Me × U]   (souvenirs filtrés par phase)
                                      ↓
Amyghaleon vérifie: max(Peur, Horreur, Anxiété) > seuil_phase
```

---

**Fin du résumé technique (version avec phases) - MCEE v2.0**

---

## Annexe : Tableau Complet des Phases

| Phase | α | β | γ | δ | θ | Seuil Amyg. | Consolidation | Learning | Focus | Comportement |
|-------|---|---|---|---|---|-------------|---------------|----------|-------|--------------|
| **SÉRÉNITÉ** | 0.25 | 0.15 | 0.12 | 0.30 | 0.10 | 0.85 | 0.4 | 1.0 | 0.5 | Équilibre, apprentissage optimal |
| **JOIE** | 0.40 | 0.25 | 0.08 | 0.35 | 0.05 | 0.95 | 0.5 | 1.3 | 0.3 | Renforcement positif, risque sous-estimé |
| **EXPLORATION** | 0.35 | 0.10 | 0.10 | 0.25 | 0.15 | 0.80 | 0.6 | 1.5 | 0.8 | Apprentissage MAXIMAL, attention focalisée |
| **ANXIÉTÉ** | 0.40 | 0.30 | 0.06 | 0.45 | 0.08 | 0.70 | 0.4 | 0.8 | 0.6 | Hypervigilance, biais négatif |
| **PEUR** | 0.60 | 0.45 | 0.02 | 0.70 | 0.02 | 0.50 | 0.8 | 0.3 | 0.95 | 🚨 URGENCE - Traumas dominants |
| **TRISTESSE** | 0.20 | 0.40 | 0.05 | 0.55 | 0.12 | 0.90 | 0.5 | 0.6 | 0.4 | Rumination, introspection |
| **DÉGOÛT** | 0.50 | 0.25 | 0.08 | 0.40 | 0.08 | 0.75 | 0.6 | 0.9 | 0.7 | Évitement, associations négatives |
| **CONFUSION** | 0.35 | 0.30 | 0.15 | 0.50 | 0.15 | 0.80 | 0.3 | 0.7 | 0.5 | Recherche info, incertitude |
