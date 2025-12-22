# MCEE v2.0 - Guide de Migration vers le Système de Phases

## 📋 Résumé des Changements

Le MCEE v2.0 introduit un **système de phases émotionnelles** qui module dynamiquement tous les paramètres du système selon l'état mental actuel.

---

## 🔄 Changements Architecturaux

### Architecture v1.0 (Originale)
```
Capteurs → Module C++ → MCEE Engine → Neo4j → Amyghaleon
                         (coefficients fixes)
```

### Architecture v2.0 (Avec Phases)
```
Capteurs → Module C++ → 🎭 Phase Detector → MCEE Engine → Neo4j → Amyghaleon
                         (analyse 24 émotions)  (coefficients dynamiques)
                         
Le Phase Detector fournit:
  - Phase active (SERENITE, JOIE, PEUR, etc.)
  - Coefficients α,β,γ,δ,θ adaptés
  - Seuil Amyghaleon ajusté
```

---

## 🎯 Modifications Principales

### 1. Coefficients MCEE (α, β, γ, δ, θ)

**Avant (v1.0)** - Coefficients fixes :
```python
alpha = 0.3   # Feedback externe
beta = 0.2    # Feedback interne
gamma = 0.1   # Décroissance
delta = 0.4   # Influence souvenirs
theta = 0.1   # Sagesse
```

**Maintenant (v2.0)** - Coefficients dynamiques selon phase :
```python
# Phase SÉRÉNITÉ (équilibre)
alpha = 0.25, beta = 0.15, gamma = 0.12, delta = 0.30, theta = 0.10

# Phase PEUR (urgence)
alpha = 0.60  # ⚠️ MAXIMAL
beta = 0.45   # Très élevé
gamma = 0.02  # Très lent (état persistant)
delta = 0.70  # TRAUMAS DOMINANTS
theta = 0.02  # Sagesse quasi absente

# Phase JOIE (euphorie)
alpha = 0.40, beta = 0.25, gamma = 0.08, delta = 0.35, theta = 0.05

# Phase EXPLORATION (apprentissage)
alpha = 0.35, beta = 0.10, gamma = 0.10, delta = 0.25, theta = 0.15
```

### 2. Formule de Mise à Jour

**Avant :**
```python
E_i(t+1) = E_i(t) + 0.3·Fb_ext + 0.2·Fb_int - 0.1·Δt + 0.4·Influence + 0.1·W_t
                    ^^^^ coefficients fixes
```

**Maintenant :**
```python
E_i(t+1) = E_i(t) + α_phase·Fb_ext + β_phase·Fb_int - γ_phase·Δt + δ_phase·Influence + θ_phase·W_t
                    ^^^^^^^^^^^^^ coefficients de la phase active
```

### 3. Seuil Amyghaleon

**Avant :**
```python
AMYGHALEON_THRESHOLD = 0.85  # Fixe
```

**Maintenant :**
```python
# Seuil selon la phase active
SERENITE:    0.85  # Difficile à déclencher
JOIE:        0.95  # Très difficile (euphorie)
ANXIETE:     0.70  # Facile (déjà vigilant)
PEUR:        0.50  # ⚠️ TRÈS FACILE (hypersensible)
TRISTESSE:   0.90  # Difficile
```

**Impact :** Avec le même niveau de Peur = 0.65
- Phase SÉRÉNITÉ : 0.65 < 0.85 → Pas d'urgence ✅
- Phase PEUR : 0.65 > 0.50 → 🚨 URGENCE DÉCLENCHÉE

### 4. Requêtes Neo4j

**Avant :**
```cypher
// Requête générique
MATCH (s:Souvenir)
WHERE s.last_activated > date() - duration({days: 30})
RETURN s
ORDER BY s.weight DESC
LIMIT 10
```

**Maintenant :**
```cypher
// Phase PEUR: Priorité aux traumas
MATCH (s:Souvenir)
WHERE s.dominant IN ['Peur', 'Horreur', 'Anxiété']
   OR EXISTS((s)<-[:CONCERNE]-(t:Trauma))
RETURN s
ORDER BY s.intensity DESC
LIMIT 20

// Phase JOIE: Priorité aux positifs
MATCH (s:Souvenir)
WHERE s.valence > 0.5
  AND s.dominant IN ['Joie', 'Satisfaction']
RETURN s
ORDER BY s.valence DESC
LIMIT 10
```

### 5. Structure des Souvenirs Neo4j

**Avant :**
```cypher
CREATE (s:Souvenir {
    name: 'Événement X',
    emotions: [...],
    dominant: 'Joie',
    valence: 0.7
})
```

**Maintenant :**
```cypher
CREATE (s:Souvenir {
    name: 'Événement X',
    emotions: [...],
    dominant: 'Joie',
    valence: 0.7,
    phase_at_creation: 'JOIE',  // 🆕 Phase lors création
    weight: 0.6  // 🆕 Poids selon phase
})
```

---

## 🔧 Guide de Migration du Code

### Étape 1 : Ajouter le Phase Detector

**Option A - Python**
```python
from phase_detector import PhaseDetector

# Dans MCEEEngine.__init__
self.phase_detector = PhaseDetector(
    hysteresis_margin=0.15,
    min_phase_duration=30.0
)
self.current_phase = 'SERENITE'
```

**Option B - C++ (via RabbitMQ)**
```python
# Le Phase Detector C++ envoie la phase actuelle
# Recevoir depuis RabbitMQ exchange 'mcee.phase.output'
```

### Étape 2 : Modifier la Boucle Principale

**Avant :**
```python
def on_emotions_received(self, ch, method, properties, body):
    emotions_raw = json.loads(body.decode('utf-8'))
    
    # Traitement direct avec coefficients fixes
    emotions_updated = self.emotion_updater.update_all(emotions_raw)
```

**Maintenant :**
```python
def on_emotions_received(self, ch, method, properties, body):
    emotions_raw = json.loads(body.decode('utf-8'))
    
    # 🆕 1. Détection de phase
    self.current_phase = self.phase_detector.detect_phase(emotions_raw)
    
    # 🆕 2. Récupération configuration
    phase_config = self.phase_detector.get_phase_config()
    
    # 🆕 3. Mise à jour des coefficients
    self.emotion_updater.set_coefficients_from_phase(phase_config)
    
    # 4. Traitement avec coefficients de phase
    emotions_updated = self.emotion_updater.update_all(emotions_raw)
```

### Étape 3 : Adapter EmotionUpdater

**Avant :**
```python
class EmotionUpdater:
    def __init__(self):
        self.alpha = 0.3
        self.beta = 0.2
        # ... coefficients fixes
```

**Maintenant :**
```python
class EmotionUpdater:
    def __init__(self):
        self.alpha = 0.3  # Valeurs par défaut
        self.beta = 0.2
        # ... seront écrasées par la phase
    
    def set_coefficients_from_phase(self, phase_config):
        """🆕 Mise à jour dynamique"""
        self.alpha = phase_config['alpha']
        self.beta = phase_config['beta']
        self.gamma = phase_config['gamma']
        self.delta = phase_config['delta']
        self.theta = phase_config['theta']
```

### Étape 4 : Adapter Amyghaleon

**Avant :**
```python
def check_emergency(self, emotions, souvenirs):
    THRESHOLD = 0.85  # Fixe
    max_critical = max([emotions.get(e, 0) for e in CRITICAL_EMOTIONS])
    return max_critical > THRESHOLD
```

**Maintenant :**
```python
def check_emergency(self, emotions, souvenirs, phase_threshold):
    """🆕 Seuil variable selon phase"""
    max_critical = max([emotions.get(e, 0) for e in CRITICAL_EMOTIONS])
    return max_critical > phase_threshold

# Appel
amyghaleon_threshold = phase_config['amyghaleon_threshold']
if self.amyghaleon.check_emergency(emotions, souvenirs, amyghaleon_threshold):
    # Urgence détectée
```

### Étape 5 : Adapter MemoryManager

**Avant :**
```python
def query_relevant_memories(self, emotions):
    # Requête générique
    query = "MATCH (s:Souvenir) WHERE ... RETURN s LIMIT 10"
```

**Maintenant :**
```python
def query_relevant_memories(self, phase, emotions):
    """🆕 Requête selon phase"""
    if phase == 'PEUR':
        query = """
        MATCH (s:Souvenir)
        WHERE s.dominant IN ['Peur', 'Horreur']
           OR EXISTS((s)<-[:CONCERNE]-(t:Trauma))
        RETURN s ORDER BY s.intensity DESC LIMIT 20
        """
    elif phase == 'JOIE':
        query = """
        MATCH (s:Souvenir)
        WHERE s.valence > 0.5
        RETURN s ORDER BY s.valence DESC LIMIT 10
        """
    # ... autres phases
```

### Étape 6 : Enregistrer la Phase dans Neo4j

**Nouveau :**
```python
def record_new_memory(self, emotions, E_global, phase, context):
    """🆕 Enregistre avec phase"""
    query = """
    CREATE (s:Souvenir {
        ...
        phase_at_creation: $phase,
        weight: $initial_weight
    })
    """
    
    # Poids initial selon phase
    initial_weight = self._get_initial_weight(phase, intensity, valence)
    
    self.graph.run(query, phase=phase, initial_weight=initial_weight, ...)

def _get_initial_weight(self, phase, intensity, valence):
    """🆕 Poids selon phase"""
    if phase == 'PEUR':
        return min(1.0, 0.7 + intensity * 0.3)
    elif phase == 'JOIE' and valence > 0.6:
        return min(1.0, 0.6 + valence * 0.4)
    # ... autres phases
    return 0.5
```

---

## 📊 Impact sur le Comportement

### Scénario 1 : Situation Normale

**Émotions :** Calme=0.8, Satisfaction=0.7

**v1.0 :**
- Coefficients fixes
- Traitement standard
- Amyghaleon seuil = 0.85

**v2.0 :**
- Phase détectée : **SÉRÉNITÉ**
- α=0.25 (modéré), δ=0.30 (équilibré)
- Amyghaleon seuil = 0.85
- Résultat : Apprentissage optimal, décisions posées ✅

### Scénario 2 : Menace Détectée

**Émotions :** Peur=0.927, Horreur=0.838, Anxiété=0.659

**v1.0 :**
- Coefficients fixes (α=0.3, δ=0.4)
- Amyghaleon seuil = 0.85
- Peur=0.927 > 0.85 → Urgence déclenchée
- Souvenirs influence modérée

**v2.0 :**
- Phase détectée : **PEUR** (transition IMMÉDIATE)
- α=0.60 (MAXIMAL), δ=0.70 (TRAUMAS)
- Amyghaleon seuil = 0.50 ⚠️
- Peur=0.927 > 0.50 → Urgence déclenchée
- Résultat : 
  - Traumas activés MASSIVEMENT
  - Feedback externe amplifié (danger)
  - Décroissance très lente (état persiste)
  - Création trauma potentiel
  - Sagesse désactivée (réflexes)

### Scénario 3 : Découverte Intéressante

**Émotions :** Intérêt=0.92, Fascination=0.85, Admiration=0.68

**v1.0 :**
- Coefficients fixes
- Apprentissage standard

**v2.0 :**
- Phase détectée : **EXPLORATION**
- α=0.35 (perception), θ=0.15 (sagesse élevée)
- Learning rate = 1.5x
- Focus attentionnel = 0.8
- Résultat : Apprentissage MAXIMAL ✅

---

## ⚠️ Points d'Attention

### 1. Boucles Infinites en Phase PEUR

**Problème :**
```
Phase PEUR → active traumas → renforce Peur → reste en Phase PEUR
```

**Solution :**
```python
if phase == 'PEUR' and time_in_phase > 60:
    # Forcer décroissance
    emotions['Peur'] *= 0.95
    emotions['Horreur'] *= 0.95
```

### 2. Oscillations Rapides

**Problème :**
```
ANXIETE (1s) → PEUR (2s) → ANXIETE (1s) → PEUR (2s) ...
```

**Solution :**
```python
# Hysteresis + durée minimale
HYSTERESIS_MARGIN = 0.15
MIN_PHASE_DURATION = 30.0  # secondes
```

### 3. Calibration Nécessaire

Les coefficients de phase ont été calibrés empiriquement mais peuvent nécessiter des ajustements selon votre cas d'usage :

```python
# Fichier phase_config.json
{
  "phases": {
    "peur": {
      "alpha": 0.60,  // Ajuster si trop/pas assez réactif
      "delta": 0.70,  // Ajuster influence traumas
      "amyghaleon_threshold": 0.50  // Ajuster sensibilité
    }
  }
}
```

---

## 📈 Bénéfices du Système de Phases

### 1. Adaptation Contextuelle
- ✅ Comportement adapté à l'état mental
- ✅ Réponse appropriée selon urgence
- ✅ Modulation automatique des paramètres

### 2. Réalisme Psychologique
- ✅ Mimique états mentaux humains
- ✅ Hypervigilance en anxiété
- ✅ Inhibition en tristesse
- ✅ Activation traumas en peur

### 3. Gestion Traumas
- ✅ Détection automatique situations critiques
- ✅ Protection contre re-traumatisation
- ✅ Consolidation forte des événements majeurs

### 4. Optimisation Apprentissage
- ✅ Phase EXPLORATION : learning x1.5
- ✅ Phase JOIE : renforcement positif
- ✅ Phase PEUR : survie immédiate (pas d'apprentissage)

---

## 🚀 Ordre de Déploiement Recommandé

### Étape 1 : Tests Isolés
```bash
# Tester le Phase Detector seul
python test_phase_detector.py

# Vérifier détection correcte
```

### Étape 2 : Intégration Partielle
```python
# Garder coefficients fixes mais logger la phase
phase = phase_detector.detect_phase(emotions)
print(f"Phase détectée: {phase} (coefficients non appliqués)")
```

### Étape 3 : Activation Progressive
```python
# Activer seulement α et δ au début
if ENABLE_PHASE_ALPHA_DELTA:
    self.emotion_updater.alpha = phase_config['alpha']
    self.emotion_updater.delta = phase_config['delta']
```

### Étape 4 : Activation Complète
```python
# Tous les coefficients + seuils
self.emotion_updater.set_coefficients_from_phase(phase_config)
amyghaleon_threshold = phase_config['amyghaleon_threshold']
```

### Étape 5 : Monitoring
```cypher
// Analyser les transitions
MATCH (t:PhaseTransition)
RETURN t.from_phase, t.to_phase, count(*) as count
ORDER BY count DESC
```

---

## 📚 Ressources

- **Documentation Phases** : `mcee_phases_system.md`
- **Code Python** : `phase_detector.py`, `mcee_phase_monitor.py`
- **Code C++** : `phase_detector.hpp/.cpp`
- **Tests** : `test_phase_detector.cpp`
- **README C++** : `README_CPP20.md`

---

## 🔗 Compatibilité

### Rétrocompatibilité

Le système v2.0 est **compatible** avec v1.0 :
- Si Phase Detector désactivé → coefficients par défaut (v1.0)
- Neo4j : ancien schéma fonctionne (phase_at_creation optionnel)
- RabbitMQ : même exchange

### Migration Graduelle

Possible de migrer progressivement :
1. Déployer Phase Detector en monitoring seul
2. Activer modulation coefficients
3. Activer seuils Amyghaleon
4. Activer requêtes Neo4j adaptées

---

**Document de Migration MCEE v1.0 → v2.0 (avec Phases)**
