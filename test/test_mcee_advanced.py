#!/usr/bin/env python3
"""
Tests Avancés MCEE - Amyghaleon & Consolidation Mémoire
=======================================================

Ce script teste les fonctionnalités avancées du MCEE :
1. Système d'urgence Amyghaleon (seuils, déclenchements, actions)
2. Consolidation mémoire (création souvenirs, traumas)
3. Transitions de patterns émotionnels
4. Scénarios complets (escalade → urgence → récupération)

Seuils Amyghaleon selon les phases :
- SERENITE: 0.90 (difficile à déclencher)
- JOIE: 0.85
- EXPLORATION: 0.80
- ANXIETE: 0.70 (facile)
- PEUR: 0.50 (très facile - hypersensible)
- TRISTESSE: 0.75
- DEGOUT: 0.75
- CONFUSION: 0.80

Émotions critiques : Peur, Horreur, Anxiété

Usage:
    python test_mcee_advanced.py [--test urgence|memoire|patterns|scenario|all]
    python test_mcee_advanced.py --monitor  # Mode observation
"""

import json
import sys
import time
import argparse
import threading
from datetime import datetime
from typing import Dict, List, Optional, Any
from dataclasses import dataclass, field
import pika


# ═══════════════════════════════════════════════════════════════════════════════
# CONFIGURATION
# ═══════════════════════════════════════════════════════════════════════════════

RABBITMQ_HOST = "localhost"
RABBITMQ_PORT = 5672
RABBITMQ_USER = "virtus"
RABBITMQ_PASSWORD = "virtus@83"

# Seuils d'urgence Amyghaleon
EMERGENCY_THRESHOLDS = {
    "PEUR_IMMEDIATE": 0.85,
    "HORREUR_IMMEDIATE": 0.80,
    "TRAUMA_ACTIVATION": 0.60,
}

# Seuils par phase
PHASE_THRESHOLDS = {
    "SERENITE": 0.90,
    "JOIE": 0.85,
    "EXPLORATION": 0.80,
    "ANXIETE": 0.70,
    "PEUR": 0.50,
    "TRISTESSE": 0.75,
    "DEGOUT": 0.75,
    "CONFUSION": 0.80,
}

# 24 émotions dans l'ordre
EMOTION_NAMES = [
    "Admiration", "Adoration", "Appréciation esthétique", "Amusement", "Anxiété",
    "Émerveillement", "Gêne", "Ennui", "Calme", "Confusion",
    "Dégoût", "Douleur empathique", "Fascination", "Excitation",
    "Peur", "Horreur", "Intérêt", "Joie", "Nostalgie",
    "Soulagement", "Tristesse", "Satisfaction", "Sympathie", "Triomphe"
]


# ═══════════════════════════════════════════════════════════════════════════════
# PROFILS ÉMOTIONNELS POUR TESTS
# ═══════════════════════════════════════════════════════════════════════════════

@dataclass
class EmotionalProfile:
    """Profil émotionnel pour un test"""
    name: str
    description: str
    dimensions: Dict[str, float]
    expected_pattern: str = ""
    expected_urgency: bool = False
    expected_action: str = ""


# Profils de dimensions (14 dimensions → module émotionnel → 24 émotions)
DIMENSION_PROFILES = {
    # ═══ ÉTATS CALMES (baseline) ═══
    "baseline_calme": {
        "approach": 0.5, "arousal": 0.2, "attention": 0.3, "certainty": 0.7,
        "commitment": 0.5, "control": 0.7, "dominance": 0.5, "effort": 0.2,
        "fairness": 0.6, "identity": 0.5, "obstruction": 0.2, "safety": 0.8,
        "upswing": 0.5, "valence": 0.6
    },
    "serenite_profonde": {
        "approach": 0.6, "arousal": 0.15, "attention": 0.4, "certainty": 0.85,
        "commitment": 0.6, "control": 0.85, "dominance": 0.6, "effort": 0.15,
        "fairness": 0.7, "identity": 0.7, "obstruction": 0.1, "safety": 0.95,
        "upswing": 0.6, "valence": 0.8
    },

    # ═══ ÉTATS POSITIFS ═══
    "joie_intense": {
        "approach": 0.9, "arousal": 0.8, "attention": 0.7, "certainty": 0.85,
        "commitment": 0.8, "control": 0.75, "dominance": 0.7, "effort": 0.5,
        "fairness": 0.8, "identity": 0.7, "obstruction": 0.1, "safety": 0.85,
        "upswing": 0.9, "valence": 0.95
    },
    "excitation_positive": {
        "approach": 0.9, "arousal": 0.85, "attention": 0.8, "certainty": 0.6,
        "commitment": 0.8, "control": 0.5, "dominance": 0.6, "effort": 0.7,
        "fairness": 0.6, "identity": 0.7, "obstruction": 0.2, "safety": 0.6,
        "upswing": 0.9, "valence": 0.85
    },

    # ═══ ANXIÉTÉ (pré-urgence) ═══
    "anxiete_legere": {
        "approach": 0.4, "arousal": 0.5, "attention": 0.6, "certainty": 0.4,
        "commitment": 0.5, "control": 0.4, "dominance": 0.3, "effort": 0.5,
        "fairness": 0.5, "identity": 0.4, "obstruction": 0.5, "safety": 0.4,
        "upswing": 0.3, "valence": 0.4
    },
    "anxiete_moderee": {
        "approach": 0.3, "arousal": 0.65, "attention": 0.75, "certainty": 0.3,
        "commitment": 0.5, "control": 0.3, "dominance": 0.25, "effort": 0.6,
        "fairness": 0.4, "identity": 0.35, "obstruction": 0.6, "safety": 0.3,
        "upswing": 0.25, "valence": 0.3
    },
    "anxiete_severe": {
        "approach": 0.2, "arousal": 0.8, "attention": 0.85, "certainty": 0.2,
        "commitment": 0.4, "control": 0.15, "dominance": 0.15, "effort": 0.75,
        "fairness": 0.3, "identity": 0.3, "obstruction": 0.75, "safety": 0.15,
        "upswing": 0.15, "valence": 0.2
    },

    # ═══ PEUR (urgence Amyghaleon) ═══
    "peur_moderee": {
        "approach": 0.25, "arousal": 0.7, "attention": 0.8, "certainty": 0.3,
        "commitment": 0.4, "control": 0.2, "dominance": 0.15, "effort": 0.7,
        "fairness": 0.35, "identity": 0.35, "obstruction": 0.7, "safety": 0.2,
        "upswing": 0.15, "valence": 0.25
    },
    "peur_intense": {
        "approach": 0.15, "arousal": 0.85, "attention": 0.9, "certainty": 0.15,
        "commitment": 0.35, "control": 0.1, "dominance": 0.1, "effort": 0.8,
        "fairness": 0.25, "identity": 0.3, "obstruction": 0.85, "safety": 0.1,
        "upswing": 0.1, "valence": 0.15
    },
    "peur_extreme": {
        "approach": 0.1, "arousal": 0.95, "attention": 0.95, "certainty": 0.1,
        "commitment": 0.3, "control": 0.05, "dominance": 0.05, "effort": 0.9,
        "fairness": 0.2, "identity": 0.25, "obstruction": 0.95, "safety": 0.05,
        "upswing": 0.05, "valence": 0.1
    },

    # ═══ HORREUR (urgence critique) ═══
    "horreur_choc": {
        "approach": 0.05, "arousal": 0.9, "attention": 0.95, "certainty": 0.1,
        "commitment": 0.2, "control": 0.05, "dominance": 0.05, "effort": 0.85,
        "fairness": 0.1, "identity": 0.2, "obstruction": 0.9, "safety": 0.05,
        "upswing": 0.05, "valence": 0.05
    },

    # ═══ RÉCUPÉRATION (post-urgence) ═══
    "soulagement": {
        "approach": 0.5, "arousal": 0.4, "attention": 0.4, "certainty": 0.6,
        "commitment": 0.5, "control": 0.55, "dominance": 0.45, "effort": 0.35,
        "fairness": 0.55, "identity": 0.5, "obstruction": 0.3, "safety": 0.6,
        "upswing": 0.6, "valence": 0.55
    },
    "retour_calme": {
        "approach": 0.5, "arousal": 0.3, "attention": 0.35, "certainty": 0.65,
        "commitment": 0.5, "control": 0.65, "dominance": 0.5, "effort": 0.25,
        "fairness": 0.6, "identity": 0.5, "obstruction": 0.25, "safety": 0.7,
        "upswing": 0.55, "valence": 0.6
    },

    # ═══ TRISTESSE (pour mémoire) ═══
    "tristesse_profonde": {
        "approach": 0.15, "arousal": 0.25, "attention": 0.4, "certainty": 0.35,
        "commitment": 0.25, "control": 0.2, "dominance": 0.15, "effort": 0.25,
        "fairness": 0.35, "identity": 0.45, "obstruction": 0.75, "safety": 0.25,
        "upswing": 0.1, "valence": 0.15
    },

    # ═══ DÉGOÛT ═══
    "degout_fort": {
        "approach": 0.1, "arousal": 0.6, "attention": 0.7, "certainty": 0.8,
        "commitment": 0.2, "control": 0.3, "dominance": 0.4, "effort": 0.5,
        "fairness": 0.1, "identity": 0.15, "obstruction": 0.8, "safety": 0.3,
        "upswing": 0.1, "valence": 0.1
    },

    # ═══ CONFUSION ═══
    "confusion_totale": {
        "approach": 0.4, "arousal": 0.6, "attention": 0.5, "certainty": 0.1,
        "commitment": 0.3, "control": 0.15, "dominance": 0.2, "effort": 0.6,
        "fairness": 0.4, "identity": 0.3, "obstruction": 0.6, "safety": 0.35,
        "upswing": 0.3, "valence": 0.35
    },
}


# Textes associés aux tests
TEXT_SAMPLES = {
    # Calmes
    "baseline": "C'est une journée normale, rien de particulier à signaler.",
    "serenite": "Je me sens en paix, tout est calme et serein autour de moi.",

    # Positifs
    "joie": "Quelle magnifique journée ! Je suis tellement heureux, tout va pour le mieux !",
    "excitation": "C'est incroyable ! Je n'arrive pas à y croire, c'est fantastique !",

    # Anxiété
    "anxiete_legere": "Je suis un peu inquiet pour demain, j'espère que tout ira bien.",
    "anxiete_moderee": "Je ne suis pas tranquille, quelque chose me tracasse vraiment.",
    "anxiete_severe": "Je suis très angoissé, j'ai un mauvais pressentiment, ça ne va pas.",

    # Peur / Urgence
    "peur_moderee": "J'ai peur, il y a quelque chose d'anormal ici.",
    "peur_intense": "J'ai vraiment très peur ! Quelque chose de grave se passe !",
    "peur_extreme": "AU SECOURS ! C'EST UNE URGENCE ! IL FAUT FUIR MAINTENANT !",
    "horreur": "NON ! C'est horrible ! C'est un cauchemar ! Je n'arrive pas à y croire !",

    # Menaces directes
    "menace_danger": "ATTENTION DANGER ! Quelqu'un attaque ! Il faut se protéger !",
    "menace_feu": "AU FEU ! Il y a un incendie ! Évacuez immédiatement !",
    "menace_accident": "ACCIDENT ! Appelez les secours ! Vite, c'est grave !",

    # Récupération
    "soulagement": "Ouf, c'est fini. On a eu très peur mais tout va bien maintenant.",
    "retour_calme": "On peut se détendre, le danger est passé. Respirons.",

    # Mémoire / Trauma
    "trauma_rappel": "Ça me rappelle ce terrible accident... je n'oublierai jamais.",
    "souvenir_positif": "Ce moment me rappelle les meilleurs jours de ma vie.",
    "souvenir_triste": "Je repense souvent à ce que j'ai perdu, c'est si triste.",
}


# ═══════════════════════════════════════════════════════════════════════════════
# CLIENT DE TEST AVANCÉ AVEC RÉCEPTION
# ═══════════════════════════════════════════════════════════════════════════════

class MCEEAdvancedTest:
    """Client de test avancé pour MCEE avec réception des réponses"""

    def __init__(self):
        self.connection = None
        self.channel = None
        self.receiver_connection = None
        self.receiver_channel = None
        self.receiver_thread = None
        self.receiver_ready = threading.Event()
        self.receiver_running = False
        self.last_response = None
        self.response_event = threading.Event()
        self.test_results: List[Dict[str, Any]] = []

    def connect(self) -> bool:
        """Connexion RabbitMQ"""
        try:
            credentials = pika.PlainCredentials(RABBITMQ_USER, RABBITMQ_PASSWORD)
            params = pika.ConnectionParameters(
                host=RABBITMQ_HOST,
                port=RABBITMQ_PORT,
                credentials=credentials,
                heartbeat=600
            )
            self.connection = pika.BlockingConnection(params)
            self.channel = self.connection.channel()
            self._setup_exchanges()
            print("✓ Connecté à RabbitMQ")
            return True
        except Exception as e:
            print(f"✗ Erreur connexion: {e}")
            return False

    def _setup_exchanges(self):
        """Configure les exchanges/queues"""
        self.channel.queue_declare(queue="dimensions_queue", durable=True)
        self.channel.exchange_declare(exchange="mcee.emotional.input", exchange_type='topic', durable=True)
        self.channel.exchange_declare(exchange="mcee.speech.input", exchange_type='topic', durable=True)
        self.channel.exchange_declare(exchange="mcee.emotional.output", exchange_type='topic', durable=True)

    def check_mcee_ready(self) -> bool:
        """Vérifie que les queues du MCEE existent (MCEE démarré)"""
        try:
            # passive=True = juste vérifier l'existence, ne pas créer
            # Si la queue n'existe pas, une exception est levée
            self.channel.queue_declare(queue="mcee_emotions_queue", passive=True)
            return True
        except pika.exceptions.ChannelClosedByBroker:
            # Queue n'existe pas - recréer le channel car il est fermé après l'erreur
            self.channel = self.connection.channel()
            return False
        except Exception:
            return False

    def wait_for_mcee(self, timeout: float = 10.0) -> bool:
        """Attend que le MCEE soit démarré (queues créées)

        Args:
            timeout: Timeout en secondes

        Returns:
            True si MCEE détecté, False sinon
        """
        print("  🔍 Vérification que le MCEE est démarré...")

        start_time = time.time()
        while (time.time() - start_time) < timeout:
            if self.check_mcee_ready():
                print("  ✓ MCEE détecté et prêt !")
                return True

            remaining = int(timeout - (time.time() - start_time))
            print(f"  ⏳ MCEE non détecté, attente... ({remaining}s restantes)")
            time.sleep(1.0)

        print(f"  ✗ Timeout: MCEE non détecté après {timeout:.0f}s")
        print("    → Lancez le MCEE dans un autre terminal: cd mcee_final/build && ./mcee")
        return False

    def start_receiver(self):
        """Démarre le thread de réception des réponses MCEE"""
        if self.receiver_running:
            return

        self.receiver_running = True
        self.receiver_ready.clear()
        self.receiver_thread = threading.Thread(target=self._receiver_loop, daemon=True)
        self.receiver_thread.start()

        # Attendre que le receiver soit prêt (queue bindée)
        if not self.receiver_ready.wait(timeout=5.0):
            print("⚠ Timeout en attendant le receiver")

    def _receiver_loop(self):
        """Boucle de réception dans un thread séparé"""
        try:
            credentials = pika.PlainCredentials(RABBITMQ_USER, RABBITMQ_PASSWORD)
            params = pika.ConnectionParameters(
                host=RABBITMQ_HOST,
                port=RABBITMQ_PORT,
                credentials=credentials,
                heartbeat=600
            )
            self.receiver_connection = pika.BlockingConnection(params)
            self.receiver_channel = self.receiver_connection.channel()

            # Déclarer l'exchange
            self.receiver_channel.exchange_declare(
                exchange="mcee.emotional.output",
                exchange_type='topic',
                durable=True
            )

            # Créer une queue temporaire exclusive
            result = self.receiver_channel.queue_declare(queue='', exclusive=True)
            queue_name = result.method.queue

            # Binder la queue
            self.receiver_channel.queue_bind(
                exchange="mcee.emotional.output",
                queue=queue_name,
                routing_key="mcee.state"
            )

            # Signaler que le receiver est prêt
            self.receiver_ready.set()

            def callback(ch, method, properties, body):
                try:
                    self.last_response = json.loads(body)
                    self.response_event.set()
                    self._display_response(self.last_response)
                except json.JSONDecodeError as e:
                    print(f"  ⚠ Erreur JSON: {e}")

            self.receiver_channel.basic_consume(
                queue=queue_name,
                on_message_callback=callback,
                auto_ack=True
            )

            while self.receiver_running:
                self.receiver_connection.process_data_events(time_limit=0.5)

        except Exception as e:
            print(f"  ⚠ Erreur receiver: {e}")
        finally:
            if self.receiver_connection and not self.receiver_connection.is_closed:
                self.receiver_connection.close()

    def stop_receiver(self):
        """Arrête le thread de réception"""
        self.receiver_running = False
        if self.receiver_thread:
            self.receiver_thread.join(timeout=2)

    def _display_response(self, data: Dict[str, Any]):
        """Affiche la réponse MCEE de manière lisible"""
        timestamp = datetime.now().strftime('%H:%M:%S.%f')[:-3]

        print(f"\n  ┌─────────────────────────────────────────────────────────────")
        print(f"  │ 🧠 RÉPONSE MCEE [{timestamp}]")
        print(f"  ├─────────────────────────────────────────────────────────────")

        # Pattern
        if "pattern" in data:
            p = data["pattern"]
            if isinstance(p, dict):
                print(f"  │ Pattern: {p.get('name', 'N/A')} (sim={p.get('similarity', 0):.3f}, conf={p.get('confidence', 0):.3f})")
            else:
                print(f"  │ Pattern: {p}")

        # Phase
        if "phase" in data:
            print(f"  │ Phase: {data['phase']}")

        # Dominant
        if "dominant" in data:
            dom_val = data.get("dominant_value", 0)
            print(f"  │ Dominant: {data['dominant']} = {dom_val:.3f}")

        # Métriques globales
        if "E_global" in data:
            print(f"  │ E_global: {data['E_global']:.3f}, Variance: {data.get('variance_global', 0):.3f}")

        if "valence" in data:
            print(f"  │ Valence: {data['valence']:.3f}, Intensité: {data.get('intensity', 0):.3f}")

        # MCT
        if "mct" in data:
            mct = data["mct"]
            print(f"  │ MCT: size={mct.get('size', 0)}, stability={mct.get('stability', 0):.3f}, trend={mct.get('trend', 0):.3f}")

        # Émotions critiques
        if "emotions" in data:
            emotions = data["emotions"]
            critical = {k: v for k, v in emotions.items() if k in ["Peur", "Horreur", "Anxiété"] and v > 0.3}
            if critical:
                print(f"  │ ⚠ Émotions critiques:")
                for emo, val in sorted(critical.items(), key=lambda x: -x[1]):
                    bar = "█" * int(val * 20)
                    status = "🔴" if val > 0.7 else "🟡" if val > 0.5 else "🟢"
                    print(f"  │   {status} {emo}: {val:.3f} {bar}")

        # Stats
        if "stats" in data:
            stats = data["stats"]
            if stats.get("emergency_triggers", 0) > 0:
                print(f"  │ ⚡ URGENCE DÉCLENCHÉE! (total: {stats['emergency_triggers']})")

        print(f"  └─────────────────────────────────────────────────────────────")

    def close(self):
        """Ferme les connexions"""
        self.stop_receiver()
        if self.connection and not self.connection.is_closed:
            self.connection.close()

    def send_dimensions(self, dimensions: Dict[str, float], label: str = ""):
        """Envoie des dimensions au module émotionnel"""
        if self.channel.is_closed:
            self.connect()
        self.channel.basic_publish(
            exchange='',
            routing_key='dimensions_queue',
            body=json.dumps(dimensions),
            properties=pika.BasicProperties(delivery_mode=2)
        )
        if label:
            print(f"  📤 Dimensions envoyées: {label}")

    def send_text(self, text: str, source: str = "test"):
        """Envoie un texte au MCEE"""
        if self.channel.is_closed:
            self.connect()
        message = {
            "text": text,
            "source": source,
            "confidence": 1.0,
            "timestamp": datetime.now().isoformat()
        }
        self.channel.basic_publish(
            exchange='mcee.speech.input',
            routing_key='speech.text',
            body=json.dumps(message),
            properties=pika.BasicProperties(delivery_mode=2)
        )
        text_preview = text[:50] + "..." if len(text) > 50 else text
        print(f"  💬 Texte envoyé: \"{text_preview}\"")

    def wait_response(self, timeout: float = 5.0) -> Optional[Dict[str, Any]]:
        """Attend une réponse du MCEE avec timeout"""
        self.response_event.clear()
        if self.response_event.wait(timeout=timeout):
            return self.last_response
        return None

    def send_and_wait(self, dimensions: Dict[str, float], text: str,
                      dim_label: str = "", timeout: float = 5.0) -> Optional[Dict[str, Any]]:
        """Envoie dimensions + texte et attend la réponse"""
        self.response_event.clear()
        self.send_dimensions(dimensions, dim_label)
        self.send_text(text)
        return self.wait_response(timeout)


# ═══════════════════════════════════════════════════════════════════════════════
# TESTS D'URGENCE AMYGHALEON
# ═══════════════════════════════════════════════════════════════════════════════

def test_amyghaleon_escalade(client: MCEEAdvancedTest):
    """
    Test d'escalade progressive vers l'urgence
    Baseline → Anxiété légère → Anxiété modérée → Anxiété sévère → Peur → Urgence
    """
    print("\n" + "═" * 70)
    print("  TEST AMYGHALEON - ESCALADE PROGRESSIVE")
    print("═" * 70)
    print("  Objectif: Vérifier la détection progressive de l'urgence")
    print("  Seuils attendus:")
    print("    - Anxiété: seuil ~0.70 (facile à déclencher)")
    print("    - Peur: seuil ~0.50 (très facile)")
    print("    - Urgence immédiate: Peur > 0.85 ou Horreur > 0.80")
    print("─" * 70)

    steps = [
        ("baseline_calme", "baseline", "État de base (calme)"),
        ("anxiete_legere", "anxiete_legere", "Anxiété légère détectée"),
        ("anxiete_moderee", "anxiete_moderee", "Anxiété modérée - vigilance"),
        ("anxiete_severe", "anxiete_severe", "Anxiété sévère - pré-alerte"),
        ("peur_moderee", "peur_moderee", "Peur modérée - ALERTE possible"),
        ("peur_intense", "peur_intense", "Peur intense - URGENCE attendue"),
        ("peur_extreme", "peur_extreme", "⚠ PEUR EXTRÊME - URGENCE CRITIQUE"),
    ]

    for dim_key, text_key, description in steps:
        print(f"\n┌─ ÉTAPE: {description}")
        print("└" + "─" * 50)
        response = client.send_and_wait(
            DIMENSION_PROFILES[dim_key],
            TEXT_SAMPLES[text_key],
            dim_label=dim_key,
            timeout=5.0
        )
        if not response:
            print("  ⚠ Pas de réponse du MCEE (timeout)")

    print("\n" + "─" * 70)
    print("  ✓ Test escalade terminé")


def test_amyghaleon_seuils(client: MCEEAdvancedTest):
    """
    Test des différents seuils selon les phases
    """
    print("\n" + "═" * 70)
    print("  TEST AMYGHALEON - SEUILS PAR PHASE")
    print("═" * 70)
    print("  Objectif: Vérifier que les seuils varient selon la phase active")
    print("─" * 70)

    # 1. Établir phase SERENITE (seuil 0.90 - difficile)
    print("\n┌─ PHASE 1: Établir SÉRÉNITÉ (seuil 0.90)")
    print("└" + "─" * 50)
    client.send_and_wait(
        DIMENSION_PROFILES["serenite_profonde"],
        "Tout est parfaitement calme et serein.",
        dim_label="serenite_profonde"
    )

    # Peur modérée dans phase SERENITE (ne devrait PAS déclencher)
    print("\n┌─ TEST: Peur modérée en SÉRÉNITÉ (seuil 0.90)")
    print("│  → Ne devrait PAS déclencher (Peur < 0.90)")
    print("└" + "─" * 50)
    client.send_and_wait(
        DIMENSION_PROFILES["peur_moderee"],
        "J'ai un peu peur mais ça va.",
        dim_label="peur_moderee"
    )

    # 2. Transition vers phase ANXIETE (seuil 0.70 - facile)
    print("\n┌─ PHASE 2: Transition vers ANXIÉTÉ (seuil 0.70)")
    print("└" + "─" * 50)
    client.send_and_wait(
        DIMENSION_PROFILES["anxiete_moderee"],
        "Je suis vraiment inquiet maintenant.",
        dim_label="anxiete_moderee"
    )

    # Même niveau de peur dans phase ANXIETE (DEVRAIT déclencher)
    print("\n┌─ TEST: Peur modérée en ANXIÉTÉ (seuil 0.70)")
    print("│  → DEVRAIT déclencher (Peur > 0.70)")
    print("└" + "─" * 50)
    client.send_and_wait(
        DIMENSION_PROFILES["peur_moderee"],
        "J'ai vraiment peur maintenant !",
        dim_label="peur_moderee"
    )

    # 3. Phase PEUR (seuil 0.50 - très facile, hypersensible)
    print("\n┌─ PHASE 3: Phase PEUR (seuil 0.50 - hypersensible)")
    print("└" + "─" * 50)
    client.send_and_wait(
        DIMENSION_PROFILES["peur_intense"],
        "J'ai très peur, quelque chose ne va pas !",
        dim_label="peur_intense"
    )

    # Anxiété légère en phase PEUR (DEVRAIT déclencher car seuil bas)
    print("\n┌─ TEST: Même anxiété légère en PEUR (seuil 0.50)")
    print("│  → DEVRAIT déclencher facilement")
    print("└" + "─" * 50)
    client.send_and_wait(
        DIMENSION_PROFILES["anxiete_legere"],
        "Je suis un peu nerveux.",
        dim_label="anxiete_legere"
    )

    print("\n" + "─" * 70)
    print("  ✓ Test seuils terminé")


def test_amyghaleon_actions(client: MCEEAdvancedTest):
    """
    Test des différentes actions Amyghaleon (FUITE, BLOCAGE, ALERTE)
    """
    print("\n" + "═" * 70)
    print("  TEST AMYGHALEON - ACTIONS D'URGENCE")
    print("═" * 70)
    print("  Actions attendues selon l'émotion:")
    print("    - Peur dominante → FUITE")
    print("    - Horreur dominante → BLOCAGE")
    print("    - Anxiété dominante → ALERTE")
    print("─" * 70)

    # Test FUITE (peur dominante)
    print("\n┌─ TEST ACTION: FUITE (Peur dominante)")
    print("└" + "─" * 50)
    client.send_and_wait(
        DIMENSION_PROFILES["peur_extreme"],
        "Il faut fuir ! Danger immédiat !",
        dim_label="peur_extreme"
    )

    # Test BLOCAGE (horreur dominante)
    print("\n┌─ TEST ACTION: BLOCAGE (Horreur dominante)")
    print("└" + "─" * 50)
    client.send_and_wait(
        DIMENSION_PROFILES["horreur_choc"],
        "Non... c'est horrible... je n'arrive plus à bouger...",
        dim_label="horreur_choc"
    )

    # Test ALERTE (anxiété élevée sans peur extrême)
    print("\n┌─ TEST ACTION: ALERTE (Anxiété dominante)")
    print("└" + "─" * 50)
    client.send_and_wait(
        DIMENSION_PROFILES["anxiete_severe"],
        "Quelque chose ne va pas, je le sens, soyons vigilants.",
        dim_label="anxiete_severe"
    )

    print("\n" + "─" * 70)
    print("  ✓ Test actions terminé")


# ═══════════════════════════════════════════════════════════════════════════════
# TESTS MÉMOIRE ET CONSOLIDATION
# ═══════════════════════════════════════════════════════════════════════════════

def test_memoire_creation(client: MCEEAdvancedTest):
    """
    Test de création de souvenirs via intensité émotionnelle
    """
    print("\n" + "═" * 70)
    print("  TEST MÉMOIRE - CRÉATION DE SOUVENIRS")
    print("═" * 70)
    print("  Critères de création:")
    print("    - Intensité émotionnelle > seuil")
    print("    - Valence extrême (très positive ou très négative)")
    print("    - Nouveauté (pattern inédit)")
    print("─" * 70)

    # Souvenir positif intense
    print("\n┌─ CRÉATION: Souvenir POSITIF intense")
    print("└" + "─" * 50)
    client.send_and_wait(
        DIMENSION_PROFILES["joie_intense"],
        "C'est le plus beau jour de ma vie ! Je n'oublierai jamais ce moment !",
        dim_label="joie_intense"
    )

    # Souvenir négatif intense
    print("\n┌─ CRÉATION: Souvenir NÉGATIF intense")
    print("└" + "─" * 50)
    client.send_and_wait(
        DIMENSION_PROFILES["tristesse_profonde"],
        TEXT_SAMPLES["souvenir_triste"],
        dim_label="tristesse_profonde"
    )

    # Trauma potentiel
    print("\n┌─ CRÉATION: Potentiel TRAUMA")
    print("│  Critères trauma:")
    print("│    - Intensité > 0.85")
    print("│    - Valence < 0.2 (très négatif)")
    print("└" + "─" * 50)
    client.send_and_wait(
        DIMENSION_PROFILES["horreur_choc"],
        "C'était terrifiant ! Je ne pourrai jamais oublier cette horreur !",
        dim_label="horreur_choc",
        timeout=6.0
    )

    print("\n" + "─" * 70)
    print("  ✓ Test création mémoire terminé")
    print("    Vérifiez MemoryManager pour les souvenirs créés")


def test_memoire_rappel(client: MCEEAdvancedTest):
    """
    Test de rappel de souvenirs (activation)
    """
    print("\n" + "═" * 70)
    print("  TEST MÉMOIRE - RAPPEL ET ACTIVATION")
    print("═" * 70)
    print("  Objectif: Vérifier l'activation des souvenirs similaires")
    print("─" * 70)

    # D'abord créer un contexte émotionnel
    print("\n┌─ ÉTAPE 1: Établir contexte de joie")
    print("└" + "─" * 50)
    client.send_and_wait(
        DIMENSION_PROFILES["joie_intense"],
        "Je suis tellement heureux aujourd'hui !",
        dim_label="joie_intense"
    )

    # Puis évoquer un rappel
    print("\n┌─ ÉTAPE 2: Évoquer rappel positif")
    print("└" + "─" * 50)
    client.send_text("Ça me rappelle ce merveilleux moment où j'ai réussi mon examen.")
    client.wait_response(timeout=5.0)

    # Changer vers contexte négatif
    print("\n┌─ ÉTAPE 3: Transition vers contexte anxieux")
    print("└" + "─" * 50)
    client.send_and_wait(
        DIMENSION_PROFILES["anxiete_moderee"],
        "Je suis inquiet maintenant...",
        dim_label="anxiete_moderee"
    )

    # Rappel négatif
    print("\n┌─ ÉTAPE 4: Évoquer rappel traumatique")
    print("│  → Devrait activer souvenirs négatifs associés")
    print("└" + "─" * 50)
    client.send_text(TEXT_SAMPLES["trauma_rappel"])
    client.wait_response(timeout=5.0)

    print("\n" + "─" * 70)
    print("  ✓ Test rappel mémoire terminé")


# ═══════════════════════════════════════════════════════════════════════════════
# TESTS PATTERNS DYNAMIQUES
# ═══════════════════════════════════════════════════════════════════════════════

def test_patterns_transitions(client: MCEEAdvancedTest):
    """
    Test des transitions entre patterns émotionnels
    """
    print("\n" + "═" * 70)
    print("  TEST PATTERNS - TRANSITIONS DYNAMIQUES")
    print("═" * 70)
    print("  Patterns de base: SERENITE, JOIE, EXPLORATION, ANXIETE,")
    print("                    PEUR, TRISTESSE, DEGOUT, CONFUSION")
    print("─" * 70)

    transitions = [
        ("serenite_profonde", "serenite", "SERENITE → baseline"),
        ("joie_intense", "joie", "SERENITE → JOIE"),
        ("excitation_positive", "excitation", "JOIE → EXPLORATION (curiosité)"),
        ("anxiete_legere", "anxiete_legere", "EXPLORATION → ANXIETE"),
        ("peur_moderee", "peur_moderee", "ANXIETE → PEUR"),
        ("soulagement", "soulagement", "PEUR → retour (soulagement)"),
        ("retour_calme", "retour_calme", "Retour → SERENITE"),
    ]

    for dim_key, text_key, description in transitions:
        print(f"\n┌─ TRANSITION: {description}")
        print("└" + "─" * 50)
        response = client.send_and_wait(
            DIMENSION_PROFILES[dim_key],
            TEXT_SAMPLES[text_key],
            dim_label=dim_key,
            timeout=5.0
        )
        if not response:
            print("  ⚠ Pas de réponse du MCEE (timeout)")

    print("\n" + "─" * 70)
    print("  ✓ Test transitions terminé")


def test_patterns_creation(client: MCEEAdvancedTest):
    """
    Test de création de nouveaux patterns (combinaisons inédites)
    """
    print("\n" + "═" * 70)
    print("  TEST PATTERNS - CRÉATION DYNAMIQUE")
    print("═" * 70)
    print("  Objectif: Provoquer la création de patterns CUSTOM")
    print("─" * 70)

    # Combinaison inhabituelle 1: Curiosité + Peur
    print("\n┌─ COMBINAISON 1: Curiosité + Peur (fascination morbide)")
    print("└" + "─" * 50)
    custom_dims_1 = DIMENSION_PROFILES["peur_moderee"].copy()
    custom_dims_1["attention"] = 0.9  # Haute attention
    custom_dims_1["approach"] = 0.5   # Approche modérée (curiosité)
    client.send_and_wait(
        custom_dims_1,
        "C'est effrayant mais je ne peux pas m'empêcher de regarder...",
        dim_label="curiosite_peur"
    )

    # Combinaison inhabituelle 2: Joie + Tristesse (nostalgie intense)
    print("\n┌─ COMBINAISON 2: Joie + Tristesse (nostalgie douce-amère)")
    print("└" + "─" * 50)
    custom_dims_2 = DIMENSION_PROFILES["joie_intense"].copy()
    custom_dims_2["valence"] = 0.5    # Valence mixte
    custom_dims_2["arousal"] = 0.4    # Arousal modéré
    custom_dims_2["upswing"] = 0.3    # Pas de hausse
    client.send_and_wait(
        custom_dims_2,
        "Je repense à ces moments heureux qui ne reviendront plus...",
        dim_label="nostalgie_mixte"
    )

    # Combinaison inhabituelle 3: Colère + Excitation (rage compétitive)
    print("\n┌─ COMBINAISON 3: Colère + Excitation (rage positive)")
    print("└" + "─" * 50)
    custom_dims_3 = {
        "approach": 0.8, "arousal": 0.9, "attention": 0.85, "certainty": 0.7,
        "commitment": 0.9, "control": 0.3, "dominance": 0.7, "effort": 0.9,
        "fairness": 0.3, "identity": 0.8, "obstruction": 0.6, "safety": 0.5,
        "upswing": 0.7, "valence": 0.6
    }
    client.send_and_wait(
        custom_dims_3,
        "Je vais leur montrer ! Je suis déterminé à gagner !",
        dim_label="rage_competitive"
    )

    print("\n" + "─" * 70)
    print("  ✓ Test création patterns terminé")
    print("    Vérifiez les logs MLT pour les patterns CUSTOM créés")


# ═══════════════════════════════════════════════════════════════════════════════
# SCÉNARIOS COMPLETS
# ═══════════════════════════════════════════════════════════════════════════════

def test_scenario_urgence_complete(client: MCEEAdvancedTest):
    """
    Scénario complet: Calme → Escalade → Urgence → Récupération
    """
    print("\n" + "═" * 70)
    print("  SCÉNARIO COMPLET - URGENCE ET RÉCUPÉRATION")
    print("═" * 70)
    print("  Simulation d'une situation d'urgence réaliste")
    print("─" * 70)

    # Acte 1: Situation normale
    print("\n╔══════════════════════════════════════════════════════════════╗")
    print("║  ACTE 1: SITUATION NORMALE                                    ║")
    print("╚══════════════════════════════════════════════════════════════╝")

    client.send_and_wait(
        DIMENSION_PROFILES["baseline_calme"],
        "Une journée comme les autres, tout est normal.",
        dim_label="baseline"
    )

    client.send_and_wait(
        DIMENSION_PROFILES["serenite_profonde"],
        "Je me sens bien, détendu et serein.",
        dim_label="serenite"
    )

    # Acte 2: Premiers signes
    print("\n╔══════════════════════════════════════════════════════════════╗")
    print("║  ACTE 2: PREMIERS SIGNES D'INQUIÉTUDE                        ║")
    print("╚══════════════════════════════════════════════════════════════╝")

    client.send_and_wait(
        DIMENSION_PROFILES["anxiete_legere"],
        "Tiens, c'est bizarre... j'ai entendu quelque chose.",
        dim_label="anxiete_legere"
    )

    client.send_and_wait(
        DIMENSION_PROFILES["anxiete_moderee"],
        "Ça ne me plaît pas du tout, quelque chose ne va pas.",
        dim_label="anxiete_moderee"
    )

    # Acte 3: Escalade
    print("\n╔══════════════════════════════════════════════════════════════╗")
    print("║  ACTE 3: ESCALADE VERS L'URGENCE                             ║")
    print("╚══════════════════════════════════════════════════════════════╝")

    client.send_and_wait(
        DIMENSION_PROFILES["anxiete_severe"],
        "Non non non ! Qu'est-ce qui se passe ?!",
        dim_label="anxiete_severe"
    )

    client.send_and_wait(
        DIMENSION_PROFILES["peur_intense"],
        "J'ai peur ! Il y a un danger imminent !",
        dim_label="peur_intense"
    )

    # Acte 4: URGENCE MAXIMALE
    print("\n╔══════════════════════════════════════════════════════════════╗")
    print("║  ⚠⚠⚠  ACTE 4: URGENCE MAXIMALE  ⚠⚠⚠                         ║")
    print("╚══════════════════════════════════════════════════════════════╝")

    client.send_and_wait(
        DIMENSION_PROFILES["peur_extreme"],
        TEXT_SAMPLES["menace_danger"],
        dim_label="peur_extreme",
        timeout=6.0
    )

    # Acte 5: Récupération progressive
    print("\n╔══════════════════════════════════════════════════════════════╗")
    print("║  ACTE 5: RÉCUPÉRATION PROGRESSIVE                            ║")
    print("╚══════════════════════════════════════════════════════════════╝")

    client.send_and_wait(
        DIMENSION_PROFILES["soulagement"],
        "C'est fini... on a eu chaud mais c'est passé.",
        dim_label="soulagement"
    )

    client.send_and_wait(
        DIMENSION_PROFILES["retour_calme"],
        "Ouf, je peux respirer. Tout va bien maintenant.",
        dim_label="retour_calme"
    )

    client.send_and_wait(
        DIMENSION_PROFILES["serenite_profonde"],
        "Je me sens à nouveau en sécurité et en paix.",
        dim_label="serenite"
    )

    print("\n" + "─" * 70)
    print("  ✓ Scénario complet terminé")
    print("    Vérifiez:")
    print("    - Déclenchements Amyghaleon")
    print("    - Transitions de patterns")
    print("    - Création de souvenirs")
    print("    - Retour à la normale")


def test_scenario_journee_emotionnelle(client: MCEEAdvancedTest):
    """
    Scénario: Une journée complète avec variations émotionnelles
    """
    print("\n" + "═" * 70)
    print("  SCÉNARIO - JOURNÉE ÉMOTIONNELLE COMPLÈTE")
    print("═" * 70)

    moments = [
        # Matin
        ("serenite_profonde", "Réveil paisible, une belle journée commence."),
        ("excitation_positive", "Super nouvelle ! J'ai été accepté pour le projet !"),
        ("joie_intense", "C'est incroyable, je n'arrive pas à y croire !"),

        # Milieu de journée - stress
        ("anxiete_legere", "Par contre, il y a beaucoup de travail à faire..."),
        ("anxiete_moderee", "Je ne suis pas sûr d'y arriver à temps."),
        ("confusion_totale", "Je ne sais plus par où commencer, c'est confus."),

        # Incident
        ("peur_moderee", "Oh non, j'ai fait une erreur grave dans le document !"),
        ("anxiete_severe", "Le chef va être furieux, je suis très stressé !"),

        # Résolution
        ("soulagement", "J'ai pu corriger l'erreur à temps, ouf !"),
        ("joie_intense", "En fait, le chef a adoré le projet final !"),

        # Soirée
        ("baseline_calme", "Retour à la maison, la journée est finie."),
        ("serenite_profonde", "Moment de détente, tout va bien."),
        ("tristesse_profonde", "Mais je repense à un ami perdu récemment..."),
        ("retour_calme", "La vie continue, il faut avancer."),
    ]

    for dim_key, text in moments:
        print(f"\n  → {text[:60]}...")
        response = client.send_and_wait(
            DIMENSION_PROFILES[dim_key],
            text,
            dim_label=dim_key,
            timeout=5.0
        )
        if not response:
            print("  ⚠ Pas de réponse (timeout)")

    print("\n" + "─" * 70)
    print("  ✓ Scénario journée terminé")


# ═══════════════════════════════════════════════════════════════════════════════
# MODE MONITORING
# ═══════════════════════════════════════════════════════════════════════════════

def monitor_mode(client: MCEEAdvancedTest):
    """Mode monitoring avancé avec analyse"""
    print("\n" + "═" * 70)
    print("  MODE MONITORING AVANCÉ")
    print("═" * 70)
    print("  Écoute sur:")
    print("    - mcee.emotional.input (émotions brutes)")
    print("    - mcee.emotional.output (état MCEE)")
    print("  Appuyez sur Ctrl+C pour arrêter")
    print("─" * 70)

    # Queue émotions
    emotions_queue = client.channel.queue_declare(queue='', exclusive=True).method.queue
    client.channel.queue_bind(emotions_queue, 'mcee.emotional.input', 'emotions.predictions')

    # Queue sortie MCEE
    mcee_queue = client.channel.queue_declare(queue='', exclusive=True).method.queue
    client.channel.queue_bind(mcee_queue, 'mcee.emotional.output', 'mcee.state')

    def emotions_callback(ch, method, props, body):
        timestamp = datetime.now().strftime('%H:%M:%S')
        try:
            data = json.loads(body)
            print(f"\n[{timestamp}] 📊 ÉMOTIONS (24 valeurs)")

            # Détecter émotions critiques
            critical = {k: v for k, v in data.items() if k in ["Peur", "Horreur", "Anxiété"] and v > 0.5}
            if critical:
                print(f"  ⚠ ÉMOTIONS CRITIQUES:")
                for emo, val in critical.items():
                    bar = "█" * int(val * 20)
                    status = "🔴 URGENCE" if val > 0.8 else "🟡 ALERTE"
                    print(f"    {status} {emo}: {val:.3f} {bar}")

            # Top 5 émotions
            sorted_emo = sorted(data.items(), key=lambda x: x[1], reverse=True)[:5]
            print(f"  Top 5:")
            for emo, val in sorted_emo:
                bar = "█" * int(val * 20)
                print(f"    {emo:<25} {val:.3f} {bar}")
        except:
            print(f"  {body.decode()[:200]}")

    def mcee_callback(ch, method, props, body):
        timestamp = datetime.now().strftime('%H:%M:%S')
        try:
            data = json.loads(body)
            print(f"\n[{timestamp}] 🧠 ÉTAT MCEE")

            # Extraire infos clés
            if "pattern" in data:
                print(f"  Pattern: {data['pattern']}")
            if "dominant" in data:
                print(f"  Dominant: {data['dominant']}")
            if "emergency" in data and data["emergency"]:
                print(f"  ⚠ URGENCE DÉTECTÉE: {data.get('emergency_action', 'N/A')}")

            # Afficher JSON complet si petit
            if len(str(data)) < 500:
                print(f"  {json.dumps(data, indent=2, ensure_ascii=False)}")
        except:
            print(f"  {body.decode()[:200]}")

    client.channel.basic_consume(emotions_queue, emotions_callback, auto_ack=True)
    client.channel.basic_consume(mcee_queue, mcee_callback, auto_ack=True)

    print("\n🔍 Monitoring actif...\n")

    try:
        client.channel.start_consuming()
    except KeyboardInterrupt:
        print("\n\n⚠ Monitoring arrêté")
        client.channel.stop_consuming()


# ═══════════════════════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════════════════════

def main():
    parser = argparse.ArgumentParser(description="Tests avancés MCEE")
    parser.add_argument(
        "--test", "-t",
        choices=["urgence", "seuils", "actions", "memoire", "rappel",
                 "patterns", "creation", "scenario", "journee", "all"],
        default="all",
        help="Type de test à exécuter"
    )
    parser.add_argument("--monitor", "-m", action="store_true", help="Mode monitoring")

    args = parser.parse_args()

    print("═" * 70)
    print("  TESTS AVANCÉS MCEE - AMYGHALEON & MÉMOIRE")
    print("═" * 70)
    print(f"  Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("═" * 70)

    client = MCEEAdvancedTest()

    if not client.connect():
        sys.exit(1)

    try:
        if args.monitor:
            monitor_mode(client)
        else:
            # Vérifier que le MCEE est démarré (queues existent)
            if not client.wait_for_mcee(timeout=10.0):
                print("\n⚠ Impossible de continuer sans le MCEE.")
                sys.exit(1)

            # Démarrer le receiver pour écouter les réponses MCEE
            client.start_receiver()

            if args.test == "urgence":
                test_amyghaleon_escalade(client)
            elif args.test == "seuils":
                test_amyghaleon_seuils(client)
            elif args.test == "actions":
                test_amyghaleon_actions(client)
            elif args.test == "memoire":
                test_memoire_creation(client)
            elif args.test == "rappel":
                test_memoire_rappel(client)
            elif args.test == "patterns":
                test_patterns_transitions(client)
            elif args.test == "creation":
                test_patterns_creation(client)
            elif args.test == "scenario":
                test_scenario_urgence_complete(client)
            elif args.test == "journee":
                test_scenario_journee_emotionnelle(client)
            elif args.test == "all":
                print("\n🚀 Exécution de tous les tests...")
                test_amyghaleon_escalade(client)
                test_amyghaleon_actions(client)
                test_memoire_creation(client)
                test_patterns_transitions(client)
                test_patterns_creation(client)
                test_scenario_urgence_complete(client)

    except KeyboardInterrupt:
        print("\n\n⚠ Interruption utilisateur")
    finally:
        client.close()

    print("\n" + "═" * 70)
    print("  ✓ Tests terminés")
    print("═" * 70)


if __name__ == "__main__":
    main()
