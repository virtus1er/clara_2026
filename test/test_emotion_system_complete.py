#!/usr/bin/env python3
"""
Test complet du systeme emotionnel MCEE v3.0

Ce module teste tous les composants du systeme emotionnel:
- Les 14 dimensions d'entree
- Les 24 emotions predites
- Les 8 phases emotionnelles
- La communication RabbitMQ
- Les cas limites et validations
- Les tests de performance

Usage:
    python test_emotion_system_complete.py           # Tous les tests
    python test_emotion_system_complete.py -v        # Mode verbose
    python test_emotion_system_complete.py --unit    # Tests unitaires seulement
    python test_emotion_system_complete.py --integration  # Tests d'integration seulement
"""

import pika
import json
import threading
import time
import sys
import argparse
import unittest
from dataclasses import dataclass
from typing import Dict, List, Optional, Tuple, Callable
from enum import Enum
from datetime import datetime
import random

# =============================================================================
# CONFIGURATION
# =============================================================================

RABBITMQ_HOST = "localhost"
RABBITMQ_PORT = 5672
RABBITMQ_USER = "virtus"
RABBITMQ_PASS = "virtus@83"

INPUT_QUEUE = "dimensions_queue"
OUTPUT_EXCHANGE = "mcee.emotional.input"
ROUTING_KEY = "emotions.predictions"

# Les 14 dimensions attendues par le modele
DIMENSIONS = [
    "approach", "arousal", "attention", "certainty", "commitment",
    "control", "dominance", "effort", "fairness", "identity",
    "obstruction", "safety", "upswing", "valence"
]

# Les 24 emotions predites par le modele
EMOTIONS = [
    "Admiration", "Adoration", "Appreciation esthetique", "Amusement",
    "Anxiete", "Emerveillement", "Gene", "Ennui", "Calme", "Confusion",
    "Degout", "Douleur empathique", "Fascination", "Excitation",
    "Peur", "Horreur", "Interet", "Joie", "Nostalgie", "Soulagement",
    "Tristesse", "Satisfaction", "Sympathie", "Triomphe"
]

# Les 8 phases emotionnelles du systeme MCEE
PHASES = ["SERENITE", "JOIE", "EXPLORATION", "ANXIETE", "PEUR", "TRISTESSE", "DEGOUT", "CONFUSION"]


# =============================================================================
# DATA CLASSES
# =============================================================================

@dataclass
class TestResult:
    """Resultat d'un test individuel."""
    name: str
    passed: bool
    expected_emotion: str
    top_emotion: str
    top_value: float
    expected_value: float
    all_predictions: Dict[str, float]
    duration_ms: float
    message: str = ""


@dataclass
class DimensionProfile:
    """Profil de dimensions pour un etat emotionnel."""
    name: str
    dimensions: Dict[str, float]
    expected_emotions: List[str]  # Emotions attendues (top 3)
    expected_phase: str
    description: str


# =============================================================================
# PROFILS EMOTIONNELS DE TEST
# =============================================================================

# Profils pour les emotions de base
EMOTION_PROFILES = {
    # Emotions positives a haute arousal
    "Joie": DimensionProfile(
        name="Joie",
        dimensions={
            "approach": 0.8, "arousal": 0.7, "attention": 0.6, "certainty": 0.8,
            "commitment": 0.7, "control": 0.7, "dominance": 0.6, "effort": 0.4,
            "fairness": 0.7, "identity": 0.6, "obstruction": 0.2, "safety": 0.8,
            "upswing": 0.8, "valence": 0.9
        },
        expected_emotions=["Joie", "Amusement", "Satisfaction"],
        expected_phase="JOIE",
        description="Etat de joie intense avec haute valence positive"
    ),

    "Excitation": DimensionProfile(
        name="Excitation",
        dimensions={
            "approach": 0.9, "arousal": 0.9, "attention": 0.8, "certainty": 0.6,
            "commitment": 0.8, "control": 0.5, "dominance": 0.6, "effort": 0.7,
            "fairness": 0.6, "identity": 0.7, "obstruction": 0.2, "safety": 0.6,
            "upswing": 0.9, "valence": 0.8
        },
        expected_emotions=["Excitation", "Joie", "Amusement"],
        expected_phase="JOIE",
        description="Etat d'excitation avec haute arousal"
    ),

    "Triomphe": DimensionProfile(
        name="Triomphe",
        dimensions={
            "approach": 0.9, "arousal": 0.8, "attention": 0.7, "certainty": 0.9,
            "commitment": 0.9, "control": 0.9, "dominance": 0.9, "effort": 0.3,
            "fairness": 0.8, "identity": 0.9, "obstruction": 0.1, "safety": 0.9,
            "upswing": 0.9, "valence": 0.95
        },
        expected_emotions=["Triomphe", "Joie", "Satisfaction"],
        expected_phase="JOIE",
        description="Sentiment de victoire et accomplissement"
    ),

    "Amusement": DimensionProfile(
        name="Amusement",
        dimensions={
            "approach": 0.7, "arousal": 0.6, "attention": 0.5, "certainty": 0.7,
            "commitment": 0.5, "control": 0.6, "dominance": 0.5, "effort": 0.3,
            "fairness": 0.6, "identity": 0.4, "obstruction": 0.1, "safety": 0.8,
            "upswing": 0.7, "valence": 0.85
        },
        expected_emotions=["Amusement", "Joie", "Interet"],
        expected_phase="JOIE",
        description="Etat amuse et leger"
    ),

    # Emotions positives a basse arousal
    "Calme": DimensionProfile(
        name="Calme",
        dimensions={
            "approach": 0.5, "arousal": 0.2, "attention": 0.3, "certainty": 0.7,
            "commitment": 0.5, "control": 0.7, "dominance": 0.5, "effort": 0.2,
            "fairness": 0.6, "identity": 0.5, "obstruction": 0.2, "safety": 0.8,
            "upswing": 0.5, "valence": 0.6
        },
        expected_emotions=["Calme", "Satisfaction", "Soulagement"],
        expected_phase="SERENITE",
        description="Etat de calme et serenite"
    ),

    "Satisfaction": DimensionProfile(
        name="Satisfaction",
        dimensions={
            "approach": 0.6, "arousal": 0.4, "attention": 0.4, "certainty": 0.8,
            "commitment": 0.6, "control": 0.7, "dominance": 0.6, "effort": 0.3,
            "fairness": 0.7, "identity": 0.6, "obstruction": 0.2, "safety": 0.8,
            "upswing": 0.6, "valence": 0.75
        },
        expected_emotions=["Satisfaction", "Calme", "Joie"],
        expected_phase="SERENITE",
        description="Contentement apres accomplissement"
    ),

    "Soulagement": DimensionProfile(
        name="Soulagement",
        dimensions={
            "approach": 0.4, "arousal": 0.3, "attention": 0.3, "certainty": 0.7,
            "commitment": 0.4, "control": 0.6, "dominance": 0.5, "effort": 0.2,
            "fairness": 0.6, "identity": 0.5, "obstruction": 0.1, "safety": 0.9,
            "upswing": 0.7, "valence": 0.7
        },
        expected_emotions=["Soulagement", "Calme", "Satisfaction"],
        expected_phase="SERENITE",
        description="Liberation apres une tension"
    ),

    # Emotions sociales positives
    "Admiration": DimensionProfile(
        name="Admiration",
        dimensions={
            "approach": 0.7, "arousal": 0.5, "attention": 0.8, "certainty": 0.7,
            "commitment": 0.6, "control": 0.5, "dominance": 0.3, "effort": 0.3,
            "fairness": 0.8, "identity": 0.4, "obstruction": 0.1, "safety": 0.7,
            "upswing": 0.6, "valence": 0.8
        },
        expected_emotions=["Admiration", "Emerveillement", "Interet"],
        expected_phase="EXPLORATION",
        description="Respect et estime envers quelqu'un"
    ),

    "Adoration": DimensionProfile(
        name="Adoration",
        dimensions={
            "approach": 0.9, "arousal": 0.6, "attention": 0.9, "certainty": 0.8,
            "commitment": 0.9, "control": 0.4, "dominance": 0.2, "effort": 0.4,
            "fairness": 0.7, "identity": 0.3, "obstruction": 0.1, "safety": 0.8,
            "upswing": 0.7, "valence": 0.9
        },
        expected_emotions=["Adoration", "Admiration", "Joie"],
        expected_phase="JOIE",
        description="Amour profond et devotion"
    ),

    "Sympathie": DimensionProfile(
        name="Sympathie",
        dimensions={
            "approach": 0.6, "arousal": 0.4, "attention": 0.7, "certainty": 0.5,
            "commitment": 0.6, "control": 0.4, "dominance": 0.3, "effort": 0.4,
            "fairness": 0.8, "identity": 0.3, "obstruction": 0.3, "safety": 0.6,
            "upswing": 0.4, "valence": 0.5
        },
        expected_emotions=["Sympathie", "Douleur empathique", "Tristesse"],
        expected_phase="TRISTESSE",
        description="Compassion pour la souffrance d'autrui"
    ),

    # Emotions d'interet et curiosite
    "Interet": DimensionProfile(
        name="Interet",
        dimensions={
            "approach": 0.7, "arousal": 0.5, "attention": 0.9, "certainty": 0.5,
            "commitment": 0.6, "control": 0.6, "dominance": 0.5, "effort": 0.5,
            "fairness": 0.6, "identity": 0.5, "obstruction": 0.2, "safety": 0.7,
            "upswing": 0.6, "valence": 0.65
        },
        expected_emotions=["Interet", "Fascination", "Curiosite"],
        expected_phase="EXPLORATION",
        description="Engagement cognitif actif"
    ),

    "Fascination": DimensionProfile(
        name="Fascination",
        dimensions={
            "approach": 0.8, "arousal": 0.6, "attention": 0.95, "certainty": 0.4,
            "commitment": 0.7, "control": 0.4, "dominance": 0.3, "effort": 0.6,
            "fairness": 0.5, "identity": 0.4, "obstruction": 0.2, "safety": 0.6,
            "upswing": 0.6, "valence": 0.7
        },
        expected_emotions=["Fascination", "Interet", "Emerveillement"],
        expected_phase="EXPLORATION",
        description="Captivation intense"
    ),

    "Emerveillement": DimensionProfile(
        name="Emerveillement",
        dimensions={
            "approach": 0.8, "arousal": 0.7, "attention": 0.9, "certainty": 0.3,
            "commitment": 0.6, "control": 0.3, "dominance": 0.2, "effort": 0.4,
            "fairness": 0.7, "identity": 0.3, "obstruction": 0.1, "safety": 0.7,
            "upswing": 0.7, "valence": 0.85
        },
        expected_emotions=["Emerveillement", "Admiration", "Joie"],
        expected_phase="EXPLORATION",
        description="Stupefaction positive devant la grandeur"
    ),

    "Appreciation_esthetique": DimensionProfile(
        name="Appreciation esthetique",
        dimensions={
            "approach": 0.6, "arousal": 0.4, "attention": 0.8, "certainty": 0.6,
            "commitment": 0.5, "control": 0.5, "dominance": 0.4, "effort": 0.3,
            "fairness": 0.7, "identity": 0.4, "obstruction": 0.1, "safety": 0.7,
            "upswing": 0.5, "valence": 0.75
        },
        expected_emotions=["Appreciation esthetique", "Calme", "Admiration"],
        expected_phase="SERENITE",
        description="Plaisir devant la beaute"
    ),

    # Emotions negatives a haute arousal
    "Peur": DimensionProfile(
        name="Peur",
        dimensions={
            "approach": 0.2, "arousal": 0.9, "attention": 0.9, "certainty": 0.2,
            "commitment": 0.4, "control": 0.1, "dominance": 0.1, "effort": 0.8,
            "fairness": 0.3, "identity": 0.4, "obstruction": 0.8, "safety": 0.1,
            "upswing": 0.1, "valence": 0.2
        },
        expected_emotions=["Peur", "Horreur", "Anxiete"],
        expected_phase="PEUR",
        description="Reponse a une menace percue"
    ),

    "Horreur": DimensionProfile(
        name="Horreur",
        dimensions={
            "approach": 0.1, "arousal": 0.95, "attention": 0.95, "certainty": 0.3,
            "commitment": 0.3, "control": 0.05, "dominance": 0.05, "effort": 0.9,
            "fairness": 0.2, "identity": 0.3, "obstruction": 0.9, "safety": 0.05,
            "upswing": 0.05, "valence": 0.1
        },
        expected_emotions=["Horreur", "Peur", "Degout"],
        expected_phase="PEUR",
        description="Terreur extreme avec repulsion"
    ),

    "Anxiete": DimensionProfile(
        name="Anxiete",
        dimensions={
            "approach": 0.3, "arousal": 0.7, "attention": 0.8, "certainty": 0.2,
            "commitment": 0.5, "control": 0.2, "dominance": 0.2, "effort": 0.7,
            "fairness": 0.4, "identity": 0.5, "obstruction": 0.7, "safety": 0.3,
            "upswing": 0.2, "valence": 0.25
        },
        expected_emotions=["Anxiete", "Peur", "Confusion"],
        expected_phase="ANXIETE",
        description="Inquietude diffuse face a l'incertitude"
    ),

    "Colere": DimensionProfile(
        name="Colere",
        dimensions={
            "approach": 0.2, "arousal": 0.9, "attention": 0.8, "certainty": 0.7,
            "commitment": 0.8, "control": 0.1, "dominance": 0.2, "effort": 0.9,
            "fairness": 0.1, "identity": 0.1, "obstruction": 0.9, "safety": 0.4,
            "upswing": 0.2, "valence": 0.2
        },
        expected_emotions=["Horreur", "Peur", "Degout"],
        expected_phase="PEUR",
        description="Frustration intense face a l'injustice"
    ),

    # Emotions negatives a basse arousal
    "Tristesse": DimensionProfile(
        name="Tristesse",
        dimensions={
            "approach": 0.2, "arousal": 0.3, "attention": 0.4, "certainty": 0.4,
            "commitment": 0.3, "control": 0.2, "dominance": 0.2, "effort": 0.3,
            "fairness": 0.4, "identity": 0.5, "obstruction": 0.7, "safety": 0.3,
            "upswing": 0.1, "valence": 0.2
        },
        expected_emotions=["Tristesse", "Douleur empathique", "Anxiete"],
        expected_phase="TRISTESSE",
        description="Chagrin et abattement"
    ),

    "Ennui": DimensionProfile(
        name="Ennui",
        dimensions={
            "approach": 0.3, "arousal": 0.2, "attention": 0.2, "certainty": 0.6,
            "commitment": 0.2, "control": 0.5, "dominance": 0.4, "effort": 0.1,
            "fairness": 0.5, "identity": 0.4, "obstruction": 0.3, "safety": 0.6,
            "upswing": 0.3, "valence": 0.4
        },
        expected_emotions=["Ennui", "Calme", "Confusion"],
        expected_phase="CONFUSION",
        description="Manque de stimulation"
    ),

    "Nostalgie": DimensionProfile(
        name="Nostalgie",
        dimensions={
            "approach": 0.4, "arousal": 0.4, "attention": 0.6, "certainty": 0.5,
            "commitment": 0.5, "control": 0.3, "dominance": 0.3, "effort": 0.3,
            "fairness": 0.6, "identity": 0.8, "obstruction": 0.4, "safety": 0.5,
            "upswing": 0.3, "valence": 0.5
        },
        expected_emotions=["Nostalgie", "Tristesse", "Calme"],
        expected_phase="TRISTESSE",
        description="Melancolie douce du passe"
    ),

    # Emotions de repulsion
    "Degout": DimensionProfile(
        name="Degout",
        dimensions={
            "approach": 0.1, "arousal": 0.6, "attention": 0.7, "certainty": 0.8,
            "commitment": 0.3, "control": 0.5, "dominance": 0.4, "effort": 0.5,
            "fairness": 0.2, "identity": 0.6, "obstruction": 0.7, "safety": 0.4,
            "upswing": 0.2, "valence": 0.15
        },
        expected_emotions=["Degout", "Horreur", "Anxiete"],
        expected_phase="DEGOUT",
        description="Aversion et repulsion"
    ),

    # Emotions de confusion
    "Confusion": DimensionProfile(
        name="Confusion",
        dimensions={
            "approach": 0.4, "arousal": 0.5, "attention": 0.6, "certainty": 0.1,
            "commitment": 0.4, "control": 0.2, "dominance": 0.2, "effort": 0.6,
            "fairness": 0.4, "identity": 0.4, "obstruction": 0.6, "safety": 0.4,
            "upswing": 0.3, "valence": 0.4
        },
        expected_emotions=["Confusion", "Anxiete", "Ennui"],
        expected_phase="CONFUSION",
        description="Desorientation cognitive"
    ),

    "Gene": DimensionProfile(
        name="Gene",
        dimensions={
            "approach": 0.3, "arousal": 0.6, "attention": 0.7, "certainty": 0.4,
            "commitment": 0.4, "control": 0.2, "dominance": 0.1, "effort": 0.5,
            "fairness": 0.5, "identity": 0.7, "obstruction": 0.5, "safety": 0.4,
            "upswing": 0.2, "valence": 0.3
        },
        expected_emotions=["Gene", "Anxiete", "Confusion"],
        expected_phase="ANXIETE",
        description="Malaise social"
    ),

    "Douleur_empathique": DimensionProfile(
        name="Douleur empathique",
        dimensions={
            "approach": 0.5, "arousal": 0.5, "attention": 0.8, "certainty": 0.4,
            "commitment": 0.6, "control": 0.2, "dominance": 0.2, "effort": 0.5,
            "fairness": 0.7, "identity": 0.3, "obstruction": 0.5, "safety": 0.4,
            "upswing": 0.2, "valence": 0.3
        },
        expected_emotions=["Douleur empathique", "Sympathie", "Tristesse"],
        expected_phase="TRISTESSE",
        description="Souffrance ressentie pour autrui"
    ),
}

# Profils pour les phases emotionnelles
PHASE_PROFILES = {
    "SERENITE": DimensionProfile(
        name="Phase Serenite",
        dimensions={
            "approach": 0.5, "arousal": 0.25, "attention": 0.4, "certainty": 0.75,
            "commitment": 0.5, "control": 0.75, "dominance": 0.5, "effort": 0.2,
            "fairness": 0.7, "identity": 0.5, "obstruction": 0.15, "safety": 0.85,
            "upswing": 0.55, "valence": 0.65
        },
        expected_emotions=["Calme", "Satisfaction", "Appreciation esthetique"],
        expected_phase="SERENITE",
        description="Etat d'equilibre optimal"
    ),

    "JOIE": DimensionProfile(
        name="Phase Joie",
        dimensions={
            "approach": 0.85, "arousal": 0.75, "attention": 0.65, "certainty": 0.8,
            "commitment": 0.75, "control": 0.7, "dominance": 0.65, "effort": 0.35,
            "fairness": 0.75, "identity": 0.65, "obstruction": 0.15, "safety": 0.85,
            "upswing": 0.85, "valence": 0.9
        },
        expected_emotions=["Joie", "Amusement", "Excitation"],
        expected_phase="JOIE",
        description="Euphorie et renforcement positif"
    ),

    "EXPLORATION": DimensionProfile(
        name="Phase Exploration",
        dimensions={
            "approach": 0.75, "arousal": 0.6, "attention": 0.9, "certainty": 0.4,
            "commitment": 0.65, "control": 0.5, "dominance": 0.4, "effort": 0.6,
            "fairness": 0.6, "identity": 0.45, "obstruction": 0.25, "safety": 0.65,
            "upswing": 0.6, "valence": 0.7
        },
        expected_emotions=["Interet", "Fascination", "Emerveillement"],
        expected_phase="EXPLORATION",
        description="Curiosite et apprentissage maximum"
    ),

    "ANXIETE": DimensionProfile(
        name="Phase Anxiete",
        dimensions={
            "approach": 0.35, "arousal": 0.7, "attention": 0.75, "certainty": 0.25,
            "commitment": 0.5, "control": 0.25, "dominance": 0.25, "effort": 0.7,
            "fairness": 0.4, "identity": 0.5, "obstruction": 0.65, "safety": 0.35,
            "upswing": 0.25, "valence": 0.3
        },
        expected_emotions=["Anxiete", "Peur", "Confusion"],
        expected_phase="ANXIETE",
        description="Hypervigilance et biais negatif"
    ),

    "PEUR": DimensionProfile(
        name="Phase Peur",
        dimensions={
            "approach": 0.15, "arousal": 0.9, "attention": 0.95, "certainty": 0.2,
            "commitment": 0.35, "control": 0.1, "dominance": 0.1, "effort": 0.85,
            "fairness": 0.25, "identity": 0.35, "obstruction": 0.85, "safety": 0.1,
            "upswing": 0.1, "valence": 0.15
        },
        expected_emotions=["Peur", "Horreur", "Anxiete"],
        expected_phase="PEUR",
        description="Etat d'urgence - trauma dominant"
    ),

    "TRISTESSE": DimensionProfile(
        name="Phase Tristesse",
        dimensions={
            "approach": 0.25, "arousal": 0.3, "attention": 0.45, "certainty": 0.4,
            "commitment": 0.35, "control": 0.25, "dominance": 0.25, "effort": 0.35,
            "fairness": 0.45, "identity": 0.55, "obstruction": 0.65, "safety": 0.35,
            "upswing": 0.15, "valence": 0.25
        },
        expected_emotions=["Tristesse", "Douleur empathique", "Nostalgie"],
        expected_phase="TRISTESSE",
        description="Rumination et introspection"
    ),

    "DEGOUT": DimensionProfile(
        name="Phase Degout",
        dimensions={
            "approach": 0.15, "arousal": 0.6, "attention": 0.7, "certainty": 0.75,
            "commitment": 0.35, "control": 0.5, "dominance": 0.45, "effort": 0.5,
            "fairness": 0.2, "identity": 0.6, "obstruction": 0.7, "safety": 0.4,
            "upswing": 0.2, "valence": 0.2
        },
        expected_emotions=["Degout", "Horreur", "Anxiete"],
        expected_phase="DEGOUT",
        description="Evitement et associations negatives"
    ),

    "CONFUSION": DimensionProfile(
        name="Phase Confusion",
        dimensions={
            "approach": 0.4, "arousal": 0.5, "attention": 0.6, "certainty": 0.15,
            "commitment": 0.4, "control": 0.25, "dominance": 0.25, "effort": 0.6,
            "fairness": 0.45, "identity": 0.4, "obstruction": 0.55, "safety": 0.45,
            "upswing": 0.35, "valence": 0.4
        },
        expected_emotions=["Confusion", "Anxiete", "Ennui"],
        expected_phase="CONFUSION",
        description="Recherche d'information et incertitude"
    ),
}


# =============================================================================
# CLASSES DE TEST
# =============================================================================

class RabbitMQConnection:
    """Gestion de la connexion RabbitMQ."""

    def __init__(self):
        self.connection = None
        self.channel = None

    def connect(self) -> bool:
        """Etablit la connexion RabbitMQ."""
        try:
            credentials = pika.PlainCredentials(RABBITMQ_USER, RABBITMQ_PASS)
            parameters = pika.ConnectionParameters(
                host=RABBITMQ_HOST,
                port=RABBITMQ_PORT,
                credentials=credentials,
                connection_attempts=3,
                retry_delay=1
            )
            self.connection = pika.BlockingConnection(parameters)
            self.channel = self.connection.channel()
            return True
        except Exception as e:
            print(f"Erreur de connexion RabbitMQ: {e}")
            return False

    def close(self):
        """Ferme la connexion."""
        if self.connection and self.connection.is_open:
            self.connection.close()

    def is_connected(self) -> bool:
        """Verifie si la connexion est active."""
        return self.connection is not None and self.connection.is_open


class EmotionSystemTester:
    """Testeur principal du systeme emotionnel."""

    def __init__(self, verbose: bool = False, timeout: int = 10):
        self.verbose = verbose
        self.timeout = timeout
        self.results: List[TestResult] = []
        self.start_time = None

    def log(self, message: str, level: str = "INFO"):
        """Affiche un message si mode verbose."""
        if self.verbose or level == "ERROR":
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            print(f"[{timestamp}] [{level}] {message}")

    def send_dimensions(self, dimensions: Dict[str, float]) -> bool:
        """Envoie les dimensions au systeme."""
        try:
            conn = RabbitMQConnection()
            if not conn.connect():
                return False

            conn.channel.queue_declare(queue=INPUT_QUEUE, durable=True)

            message = json.dumps(dimensions)
            conn.channel.basic_publish(
                exchange="",
                routing_key=INPUT_QUEUE,
                body=message,
                properties=pika.BasicProperties(delivery_mode=2)
            )

            conn.close()
            self.log(f"Dimensions envoyees: {len(dimensions)} dimensions")
            return True
        except Exception as e:
            self.log(f"Erreur envoi dimensions: {e}", "ERROR")
            return False

    def receive_predictions(self, timeout: int = None) -> Optional[Dict[str, float]]:
        """Recoit les predictions du systeme."""
        if timeout is None:
            timeout = self.timeout

        predictions_received = []

        def callback(ch, method, properties, body):
            predictions = json.loads(body)
            predictions_received.append(predictions)
            ch.stop_consuming()

        try:
            conn = RabbitMQConnection()
            if not conn.connect():
                return None

            conn.channel.exchange_declare(
                exchange=OUTPUT_EXCHANGE,
                exchange_type="topic",
                durable=True
            )

            result = conn.channel.queue_declare(queue="", exclusive=True)
            queue_name = result.method.queue

            conn.channel.queue_bind(
                exchange=OUTPUT_EXCHANGE,
                queue=queue_name,
                routing_key=ROUTING_KEY
            )

            conn.channel.basic_consume(
                queue=queue_name,
                on_message_callback=callback,
                auto_ack=True
            )

            conn.connection.call_later(timeout, lambda: conn.channel.stop_consuming())

            conn.channel.start_consuming()
            conn.close()

            return predictions_received[0] if predictions_received else None

        except Exception as e:
            self.log(f"Erreur reception predictions: {e}", "ERROR")
            return None

    def test_emotion(self, profile: DimensionProfile) -> TestResult:
        """Execute un test d'emotion complet."""
        start_time = time.time()

        self.log(f"\n{'='*60}")
        self.log(f"TEST: {profile.name}")
        self.log(f"{'='*60}")
        self.log(f"Description: {profile.description}")

        # Demarrer le recepteur en arriere-plan
        predictions = [None]
        receiver_done = threading.Event()

        def receiver_thread():
            predictions[0] = self.receive_predictions()
            receiver_done.set()

        thread = threading.Thread(target=receiver_thread)
        thread.start()

        time.sleep(0.5)  # Laisser le recepteur se connecter

        # Envoyer les dimensions
        if not self.send_dimensions(profile.dimensions):
            return TestResult(
                name=profile.name,
                passed=False,
                expected_emotion=profile.expected_emotions[0],
                top_emotion="",
                top_value=0.0,
                expected_value=0.0,
                all_predictions={},
                duration_ms=(time.time() - start_time) * 1000,
                message="Erreur d'envoi des dimensions"
            )

        # Attendre la reponse
        receiver_done.wait(timeout=self.timeout + 1)
        thread.join(timeout=1)

        duration_ms = (time.time() - start_time) * 1000

        if predictions[0] is None:
            return TestResult(
                name=profile.name,
                passed=False,
                expected_emotion=profile.expected_emotions[0],
                top_emotion="",
                top_value=0.0,
                expected_value=0.0,
                all_predictions={},
                duration_ms=duration_ms,
                message="Timeout - pas de reponse recue"
            )

        # Analyser les resultats
        pred = predictions[0]
        sorted_emotions = sorted(pred.items(), key=lambda x: x[1], reverse=True)
        top_emotion, top_value = sorted_emotions[0]
        top_3_emotions = [e[0] for e in sorted_emotions[:3]]

        # Verifier si l'emotion attendue est dans le top 3
        expected_in_top3 = any(exp in top_3_emotions for exp in profile.expected_emotions)
        expected_value = pred.get(profile.expected_emotions[0], 0.0)

        # Afficher les resultats
        if self.verbose:
            print(f"\n{'Emotion':<30} | {'Valeur':>8}")
            print("-" * 42)
            for emotion, value in sorted_emotions:
                bar = "#" * int(value * 20)
                marker = " <--" if emotion in profile.expected_emotions else ""
                print(f"{emotion:<30} | {value:>7.3f} {bar}{marker}")

        result = TestResult(
            name=profile.name,
            passed=expected_in_top3,
            expected_emotion=profile.expected_emotions[0],
            top_emotion=top_emotion,
            top_value=top_value,
            expected_value=expected_value,
            all_predictions=pred,
            duration_ms=duration_ms,
            message=f"Top 3: {', '.join(top_3_emotions)}"
        )

        self.results.append(result)
        return result

    def test_dimension_validation(self) -> List[TestResult]:
        """Teste la validation des dimensions."""
        results = []

        # Test: valeurs limites (0.0 et 1.0)
        boundary_dims = {dim: 0.0 for dim in DIMENSIONS}
        result = self._test_dimensions("Valeurs limites (0.0)", boundary_dims)
        results.append(result)

        boundary_dims = {dim: 1.0 for dim in DIMENSIONS}
        result = self._test_dimensions("Valeurs limites (1.0)", boundary_dims)
        results.append(result)

        # Test: valeurs moyennes
        mid_dims = {dim: 0.5 for dim in DIMENSIONS}
        result = self._test_dimensions("Valeurs moyennes (0.5)", mid_dims)
        results.append(result)

        # Test: dimensions manquantes (devrait echouer ou utiliser valeurs par defaut)
        partial_dims = {"valence": 0.8, "arousal": 0.6}
        result = self._test_dimensions("Dimensions partielles", partial_dims, expect_failure=True)
        results.append(result)

        return results

    def _test_dimensions(self, name: str, dimensions: Dict[str, float],
                         expect_failure: bool = False) -> TestResult:
        """Teste un ensemble de dimensions specifique."""
        start_time = time.time()

        predictions = [None]
        receiver_done = threading.Event()

        def receiver_thread():
            predictions[0] = self.receive_predictions(timeout=3)
            receiver_done.set()

        thread = threading.Thread(target=receiver_thread)
        thread.start()
        time.sleep(0.3)

        self.send_dimensions(dimensions)
        receiver_done.wait(timeout=4)
        thread.join(timeout=1)

        duration_ms = (time.time() - start_time) * 1000
        received = predictions[0] is not None

        passed = (received and not expect_failure) or (not received and expect_failure)

        return TestResult(
            name=f"Validation: {name}",
            passed=passed,
            expected_emotion="N/A",
            top_emotion="N/A" if not received else max(predictions[0].items(), key=lambda x: x[1])[0],
            top_value=0.0 if not received else max(predictions[0].values()),
            expected_value=0.0,
            all_predictions=predictions[0] if received else {},
            duration_ms=duration_ms,
            message="Reponse recue" if received else "Pas de reponse"
        )

    def test_performance(self, num_requests: int = 10) -> Dict:
        """Teste les performances du systeme."""
        latencies = []
        successes = 0

        test_dims = EMOTION_PROFILES["Joie"].dimensions

        for i in range(num_requests):
            start = time.time()

            predictions = [None]
            done = threading.Event()

            def receiver():
                predictions[0] = self.receive_predictions(timeout=5)
                done.set()

            thread = threading.Thread(target=receiver)
            thread.start()
            time.sleep(0.2)

            self.send_dimensions(test_dims)
            done.wait(timeout=6)
            thread.join(timeout=1)

            latency = (time.time() - start) * 1000
            latencies.append(latency)

            if predictions[0] is not None:
                successes += 1

            self.log(f"Requete {i+1}/{num_requests}: {latency:.1f}ms")

        return {
            "total_requests": num_requests,
            "successes": successes,
            "success_rate": successes / num_requests * 100,
            "avg_latency_ms": sum(latencies) / len(latencies),
            "min_latency_ms": min(latencies),
            "max_latency_ms": max(latencies),
            "latencies": latencies
        }

    def test_stress(self, concurrent_requests: int = 5) -> Dict:
        """Test de stress avec requetes concurrentes."""
        results = []
        threads = []

        def stress_worker(worker_id: int):
            dims = EMOTION_PROFILES["Calme"].dimensions.copy()
            dims["arousal"] = random.uniform(0.2, 0.8)

            start = time.time()

            predictions = [None]
            done = threading.Event()

            def receiver():
                predictions[0] = self.receive_predictions(timeout=10)
                done.set()

            t = threading.Thread(target=receiver)
            t.start()
            time.sleep(0.2)

            self.send_dimensions(dims)
            done.wait(timeout=11)
            t.join(timeout=1)

            latency = (time.time() - start) * 1000
            success = predictions[0] is not None

            results.append({
                "worker_id": worker_id,
                "success": success,
                "latency_ms": latency
            })

        # Lancer les workers
        for i in range(concurrent_requests):
            t = threading.Thread(target=stress_worker, args=(i,))
            threads.append(t)
            t.start()
            time.sleep(0.1)  # Leger decalage

        # Attendre la fin
        for t in threads:
            t.join(timeout=15)

        successes = sum(1 for r in results if r["success"])
        latencies = [r["latency_ms"] for r in results]

        return {
            "concurrent_requests": concurrent_requests,
            "successes": successes,
            "success_rate": successes / concurrent_requests * 100,
            "avg_latency_ms": sum(latencies) / len(latencies) if latencies else 0,
            "results": results
        }

    def run_all_tests(self) -> Dict:
        """Execute tous les tests."""
        self.start_time = time.time()
        self.results = []

        print("\n" + "=" * 70)
        print("  TEST COMPLET DU SYSTEME EMOTIONNEL MCEE v3.0")
        print("=" * 70)

        # 1. Test de connectivite
        print("\n[1/6] Test de connectivite RabbitMQ...")
        conn = RabbitMQConnection()
        if not conn.connect():
            print("ECHEC: Impossible de se connecter a RabbitMQ")
            return {"error": "Connection failed"}
        conn.close()
        print("OK: Connexion RabbitMQ etablie")

        # 2. Tests des emotions
        print("\n[2/6] Tests des 24 emotions...")
        emotion_results = []
        for name, profile in EMOTION_PROFILES.items():
            result = self.test_emotion(profile)
            emotion_results.append(result)
            status = "PASS" if result.passed else "FAIL"
            print(f"  {status}: {name} -> Top: {result.top_emotion} ({result.top_value:.3f})")

        # 3. Tests des phases
        print("\n[3/6] Tests des 8 phases emotionnelles...")
        phase_results = []
        for name, profile in PHASE_PROFILES.items():
            result = self.test_emotion(profile)
            phase_results.append(result)
            status = "PASS" if result.passed else "FAIL"
            print(f"  {status}: {name} -> Top: {result.top_emotion} ({result.top_value:.3f})")

        # 4. Tests de validation
        print("\n[4/6] Tests de validation des dimensions...")
        validation_results = self.test_dimension_validation()
        for result in validation_results:
            status = "PASS" if result.passed else "FAIL"
            print(f"  {status}: {result.name}")

        # 5. Tests de performance
        print("\n[5/6] Tests de performance (10 requetes)...")
        perf_results = self.test_performance(10)
        print(f"  Taux de succes: {perf_results['success_rate']:.1f}%")
        print(f"  Latence moyenne: {perf_results['avg_latency_ms']:.1f}ms")
        print(f"  Latence min/max: {perf_results['min_latency_ms']:.1f}ms / {perf_results['max_latency_ms']:.1f}ms")

        # 6. Test de stress
        print("\n[6/6] Test de stress (5 requetes concurrentes)...")
        stress_results = self.test_stress(5)
        print(f"  Taux de succes: {stress_results['success_rate']:.1f}%")
        print(f"  Latence moyenne: {stress_results['avg_latency_ms']:.1f}ms")

        # Resume
        total_duration = time.time() - self.start_time
        all_results = emotion_results + phase_results + validation_results
        passed = sum(1 for r in all_results if r.passed)
        total = len(all_results)

        print("\n" + "=" * 70)
        print("  RESUME DES TESTS")
        print("=" * 70)
        print(f"  Tests reussis: {passed}/{total} ({passed/total*100:.1f}%)")
        print(f"  Duree totale: {total_duration:.1f}s")
        print(f"  Emotions: {sum(1 for r in emotion_results if r.passed)}/{len(emotion_results)}")
        print(f"  Phases: {sum(1 for r in phase_results if r.passed)}/{len(phase_results)}")
        print(f"  Validation: {sum(1 for r in validation_results if r.passed)}/{len(validation_results)}")
        print(f"  Performance: {perf_results['success_rate']:.1f}% succes")
        print(f"  Stress: {stress_results['success_rate']:.1f}% succes")
        print("=" * 70)

        return {
            "total_tests": total,
            "passed": passed,
            "failed": total - passed,
            "success_rate": passed / total * 100,
            "duration_seconds": total_duration,
            "emotion_results": emotion_results,
            "phase_results": phase_results,
            "validation_results": validation_results,
            "performance": perf_results,
            "stress": stress_results
        }

    def generate_report(self, results: Dict, filename: str = None) -> str:
        """Genere un rapport detaille des tests."""
        report = []
        report.append("=" * 70)
        report.append("RAPPORT DE TEST - SYSTEME EMOTIONNEL MCEE v3.0")
        report.append(f"Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        report.append("=" * 70)

        report.append(f"\nRESUME GLOBAL:")
        report.append(f"  Tests executes: {results['total_tests']}")
        report.append(f"  Tests reussis: {results['passed']}")
        report.append(f"  Tests echoues: {results['failed']}")
        report.append(f"  Taux de succes: {results['success_rate']:.1f}%")
        report.append(f"  Duree: {results['duration_seconds']:.1f}s")

        report.append("\n" + "-" * 70)
        report.append("DETAILS DES TESTS D'EMOTIONS:")
        report.append("-" * 70)
        for r in results.get('emotion_results', []):
            status = "PASS" if r.passed else "FAIL"
            report.append(f"[{status}] {r.name}")
            report.append(f"    Attendu: {r.expected_emotion}")
            report.append(f"    Obtenu: {r.top_emotion} ({r.top_value:.3f})")
            report.append(f"    Duree: {r.duration_ms:.1f}ms")

        report.append("\n" + "-" * 70)
        report.append("DETAILS DES TESTS DE PHASES:")
        report.append("-" * 70)
        for r in results.get('phase_results', []):
            status = "PASS" if r.passed else "FAIL"
            report.append(f"[{status}] {r.name}")
            report.append(f"    Attendu: {r.expected_emotion}")
            report.append(f"    Obtenu: {r.top_emotion} ({r.top_value:.3f})")

        report.append("\n" + "-" * 70)
        report.append("PERFORMANCES:")
        report.append("-" * 70)
        perf = results.get('performance', {})
        report.append(f"  Requetes: {perf.get('total_requests', 0)}")
        report.append(f"  Succes: {perf.get('successes', 0)}")
        report.append(f"  Taux: {perf.get('success_rate', 0):.1f}%")
        report.append(f"  Latence moyenne: {perf.get('avg_latency_ms', 0):.1f}ms")
        report.append(f"  Latence min: {perf.get('min_latency_ms', 0):.1f}ms")
        report.append(f"  Latence max: {perf.get('max_latency_ms', 0):.1f}ms")

        report_text = "\n".join(report)

        if filename:
            with open(filename, 'w') as f:
                f.write(report_text)

        return report_text


# =============================================================================
# TESTS UNITAIRES
# =============================================================================

class TestDimensionProfiles(unittest.TestCase):
    """Tests unitaires des profils de dimensions."""

    def test_all_dimensions_present(self):
        """Verifie que tous les profils ont les 14 dimensions."""
        for name, profile in EMOTION_PROFILES.items():
            with self.subTest(profile=name):
                self.assertEqual(set(profile.dimensions.keys()), set(DIMENSIONS),
                                f"Dimensions manquantes pour {name}")

    def test_dimension_values_in_range(self):
        """Verifie que toutes les valeurs sont entre 0 et 1."""
        for name, profile in EMOTION_PROFILES.items():
            for dim, value in profile.dimensions.items():
                with self.subTest(profile=name, dimension=dim):
                    self.assertGreaterEqual(value, 0.0)
                    self.assertLessEqual(value, 1.0)

    def test_expected_emotions_not_empty(self):
        """Verifie que chaque profil a des emotions attendues."""
        for name, profile in EMOTION_PROFILES.items():
            with self.subTest(profile=name):
                self.assertGreater(len(profile.expected_emotions), 0)

    def test_phase_profiles_complete(self):
        """Verifie que toutes les phases sont definies."""
        self.assertEqual(set(PHASE_PROFILES.keys()), set(PHASES))


class TestRabbitMQConnection(unittest.TestCase):
    """Tests de la connexion RabbitMQ."""

    def test_connection(self):
        """Teste la connexion RabbitMQ."""
        conn = RabbitMQConnection()
        result = conn.connect()
        if result:
            self.assertTrue(conn.is_connected())
            conn.close()
            self.assertFalse(conn.is_connected())


# =============================================================================
# MAIN
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description="Test complet du systeme emotionnel MCEE")
    parser.add_argument("-v", "--verbose", action="store_true", help="Mode verbose")
    parser.add_argument("--unit", action="store_true", help="Tests unitaires seulement")
    parser.add_argument("--integration", action="store_true", help="Tests d'integration seulement")
    parser.add_argument("--quick", action="store_true", help="Tests rapides (5 emotions)")
    parser.add_argument("--report", type=str, help="Fichier de sortie pour le rapport")
    parser.add_argument("--timeout", type=int, default=10, help="Timeout en secondes")
    args = parser.parse_args()

    if args.unit:
        # Tests unitaires seulement
        loader = unittest.TestLoader()
        suite = unittest.TestSuite()
        suite.addTests(loader.loadTestsFromTestCase(TestDimensionProfiles))
        suite.addTests(loader.loadTestsFromTestCase(TestRabbitMQConnection))
        runner = unittest.TextTestRunner(verbosity=2)
        runner.run(suite)
        return

    # Tests d'integration
    tester = EmotionSystemTester(verbose=args.verbose, timeout=args.timeout)

    if args.quick:
        # Mode rapide: seulement 5 emotions
        print("\n=== MODE RAPIDE: 5 emotions ===\n")
        quick_profiles = ["Joie", "Tristesse", "Peur", "Calme", "Colere"]
        for name in quick_profiles:
            if name in EMOTION_PROFILES:
                result = tester.test_emotion(EMOTION_PROFILES[name])
                status = "PASS" if result.passed else "FAIL"
                print(f"[{status}] {name}: {result.top_emotion} ({result.top_value:.3f})")
    else:
        # Tests complets
        results = tester.run_all_tests()

        if args.report:
            report = tester.generate_report(results, args.report)
            print(f"\nRapport sauvegarde dans: {args.report}")


if __name__ == "__main__":
    main()
