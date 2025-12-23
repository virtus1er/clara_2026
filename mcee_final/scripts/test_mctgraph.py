#!/usr/bin/env python3
"""
Script de test pour MCTGraph - Envoie des tokens et émotions via RabbitMQ
Usage: python3 test_mctgraph.py
"""

import pika
import json
import time
import uuid
from datetime import datetime

# Configuration RabbitMQ
RABBITMQ_HOST = "localhost"
RABBITMQ_PORT = 5672
RABBITMQ_USER = "virtus"
RABBITMQ_PASS = "virtus@83"

# Exchanges
TOKENS_EXCHANGE = "neo4j.tokens.output"
TOKENS_ROUTING_KEY = "tokens.extracted"

EMOTIONS_EXCHANGE = "mcee.emotional.input"
EMOTIONS_ROUTING_KEY = "emotions.predictions"

# Les 24 émotions du système MCEE
EMOTION_NAMES = [
    "Admiration", "Adoration", "Appreciation_esthetique", "Amusement",
    "Anxiete", "Emerveillement", "Gene", "Ennui",
    "Calme", "Confusion", "Degout", "Douleur_empathique",
    "Fascination", "Excitation", "Peur", "Horreur",
    "Interet", "Joie", "Nostalgie", "Soulagement",
    "Tristesse", "Satisfaction", "Sympathie", "Triomphe"
]


def get_connection():
    """Établit une connexion RabbitMQ"""
    credentials = pika.PlainCredentials(RABBITMQ_USER, RABBITMQ_PASS)
    parameters = pika.ConnectionParameters(
        host=RABBITMQ_HOST,
        port=RABBITMQ_PORT,
        credentials=credentials
    )
    return pika.BlockingConnection(parameters)


def send_tokens(channel, sentence: str, sentence_id: str = None):
    """Envoie des tokens simulés (comme si venant de spaCy/Neo4j)"""
    if sentence_id is None:
        sentence_id = f"sent_{uuid.uuid4().hex[:8]}"

    # Simulation simple de tokenisation
    words = sentence.split()
    tokens = []

    # POS tags simulés basiques
    pos_map = {
        "je": "PRON", "tu": "PRON", "il": "PRON", "elle": "PRON",
        "nous": "PRON", "vous": "PRON", "ils": "PRON", "elles": "PRON",
        "suis": "AUX", "es": "AUX", "est": "AUX", "sommes": "AUX",
        "êtes": "AUX", "sont": "AUX", "ai": "AUX", "as": "AUX",
        "le": "DET", "la": "DET", "les": "DET", "un": "DET", "une": "DET",
        "très": "ADV", "vraiment": "ADV", "tellement": "ADV",
        "ne": "ADV", "pas": "ADV", "jamais": "ADV",
    }

    for word in words:
        word_lower = word.lower().strip(".,!?;:")
        pos = pos_map.get(word_lower, "NOUN")  # Default to NOUN

        # Détection basique adjectifs
        if word_lower.endswith(("eux", "euse", "ant", "ent", "ible", "able")):
            pos = "ADJ"
        # Détection basique verbes
        elif word_lower.endswith(("er", "ir", "re", "ais", "ait", "ons", "ez")):
            pos = "VERB"

        tokens.append({
            "text": word,
            "lemma": word_lower,
            "pos": pos,
            "sentiment": 0.0  # Neutral par défaut
        })

    # Relations syntaxiques simulées (sujet-verbe basique)
    relations = []
    for i, token in enumerate(tokens):
        if token["pos"] == "VERB" and i > 0:
            # Le mot précédent est probablement le sujet
            relations.append({
                "source": i - 1,
                "target": i,
                "type": "nsubj"
            })

    message = {
        "sentence_id": sentence_id,
        "tokens": tokens,
        "relations": relations,
        "timestamp": datetime.now().isoformat()
    }

    channel.basic_publish(
        exchange=TOKENS_EXCHANGE,
        routing_key=TOKENS_ROUTING_KEY,
        body=json.dumps(message)
    )

    print(f"[Tokens] Envoyé: {len(tokens)} tokens pour '{sentence[:50]}...'")
    return tokens


def send_emotions(channel, dominant_emotion: str, intensity: float = 0.7):
    """Envoie un état émotionnel"""
    emotions = {}

    for name in EMOTION_NAMES:
        if name == dominant_emotion:
            emotions[name] = intensity
        else:
            # Bruit de fond faible
            emotions[name] = 0.05 + (0.1 * (hash(name) % 10) / 10)

    channel.basic_publish(
        exchange=EMOTIONS_EXCHANGE,
        routing_key=EMOTIONS_ROUTING_KEY,
        body=json.dumps(emotions)
    )

    print(f"[Emotions] Envoyé: {dominant_emotion} = {intensity:.2f}")


def scenario_joie():
    """Scénario: Expression de joie"""
    print("\n" + "="*60)
    print("SCÉNARIO: Expression de joie")
    print("="*60)

    connection = get_connection()
    channel = connection.channel()

    # Déclarer les exchanges
    channel.exchange_declare(exchange=TOKENS_EXCHANGE, exchange_type="topic", durable=True)
    channel.exchange_declare(exchange=EMOTIONS_EXCHANGE, exchange_type="topic", durable=True)

    # 1. Envoyer les mots
    send_tokens(channel, "Je suis vraiment heureux aujourd'hui")
    time.sleep(0.3)

    # 2. Envoyer l'émotion (devrait créer un lien causal)
    send_emotions(channel, "Joie", 0.85)
    time.sleep(0.5)

    # 3. Suite de la conversation
    send_tokens(channel, "C'est une magnifique journée")
    time.sleep(0.3)
    send_emotions(channel, "Satisfaction", 0.7)

    connection.close()
    print("[OK] Scénario joie terminé\n")


def scenario_anxiete():
    """Scénario: Montée d'anxiété"""
    print("\n" + "="*60)
    print("SCÉNARIO: Montée d'anxiété")
    print("="*60)

    connection = get_connection()
    channel = connection.channel()

    channel.exchange_declare(exchange=TOKENS_EXCHANGE, exchange_type="topic", durable=True)
    channel.exchange_declare(exchange=EMOTIONS_EXCHANGE, exchange_type="topic", durable=True)

    # Progression anxiété
    phrases = [
        ("Je ne sais pas si ça va marcher", "Anxiete", 0.4),
        ("J'ai peur de me tromper", "Peur", 0.6),
        ("C'est vraiment stressant", "Anxiete", 0.75),
    ]

    for phrase, emotion, intensity in phrases:
        send_tokens(channel, phrase)
        time.sleep(0.2)
        send_emotions(channel, emotion, intensity)
        time.sleep(0.5)

    connection.close()
    print("[OK] Scénario anxiété terminé\n")


def scenario_fascination():
    """Scénario: Découverte fascinante"""
    print("\n" + "="*60)
    print("SCÉNARIO: Découverte fascinante")
    print("="*60)

    connection = get_connection()
    channel = connection.channel()

    channel.exchange_declare(exchange=TOKENS_EXCHANGE, exchange_type="topic", durable=True)
    channel.exchange_declare(exchange=EMOTIONS_EXCHANGE, exchange_type="topic", durable=True)

    send_tokens(channel, "Regarde cette incroyable découverte scientifique")
    time.sleep(0.2)
    send_emotions(channel, "Fascination", 0.8)
    time.sleep(0.3)

    send_tokens(channel, "C'est absolument extraordinaire")
    time.sleep(0.2)
    send_emotions(channel, "Emerveillement", 0.9)
    time.sleep(0.3)

    send_tokens(channel, "Je veux en apprendre davantage")
    time.sleep(0.2)
    send_emotions(channel, "Interet", 0.85)

    connection.close()
    print("[OK] Scénario fascination terminé\n")


def interactive_mode():
    """Mode interactif - envoie des phrases personnalisées"""
    print("\n" + "="*60)
    print("MODE INTERACTIF")
    print("Entrez une phrase et une émotion (ou 'quit' pour quitter)")
    print("="*60)

    connection = get_connection()
    channel = connection.channel()

    channel.exchange_declare(exchange=TOKENS_EXCHANGE, exchange_type="topic", durable=True)
    channel.exchange_declare(exchange=EMOTIONS_EXCHANGE, exchange_type="topic", durable=True)

    print("\nÉmotions disponibles:")
    for i, name in enumerate(EMOTION_NAMES):
        print(f"  {i:2d}. {name}")

    while True:
        print("\n" + "-"*40)
        phrase = input("Phrase (ou 'quit'): ").strip()
        if phrase.lower() == 'quit':
            break

        if not phrase:
            continue

        emotion_input = input(f"Émotion (0-23 ou nom): ").strip()

        # Trouver l'émotion
        emotion = None
        try:
            idx = int(emotion_input)
            if 0 <= idx < 24:
                emotion = EMOTION_NAMES[idx]
        except ValueError:
            # Chercher par nom
            for name in EMOTION_NAMES:
                if emotion_input.lower() in name.lower():
                    emotion = name
                    break

        if not emotion:
            print("Émotion non reconnue, utilisation de 'Calme'")
            emotion = "Calme"

        intensity = input("Intensité (0.0-1.0, défaut 0.7): ").strip()
        try:
            intensity = float(intensity)
        except:
            intensity = 0.7

        # Envoyer
        send_tokens(channel, phrase)
        time.sleep(0.2)
        send_emotions(channel, emotion, intensity)

        print(f"[OK] Envoyé: '{phrase}' + {emotion}={intensity:.2f}")

    connection.close()
    print("\n[FIN] Mode interactif terminé")


def main():
    print("""
╔══════════════════════════════════════════════════════════════╗
║          Test MCTGraph - Envoi de tokens et émotions         ║
╚══════════════════════════════════════════════════════════════╝
    """)

    print("Options:")
    print("  1. Scénario Joie")
    print("  2. Scénario Anxiété")
    print("  3. Scénario Fascination")
    print("  4. Tous les scénarios")
    print("  5. Mode interactif")
    print("  q. Quitter")

    choice = input("\nChoix: ").strip().lower()

    if choice == '1':
        scenario_joie()
    elif choice == '2':
        scenario_anxiete()
    elif choice == '3':
        scenario_fascination()
    elif choice == '4':
        scenario_joie()
        time.sleep(1)
        scenario_anxiete()
        time.sleep(1)
        scenario_fascination()
    elif choice == '5':
        interactive_mode()
    elif choice == 'q':
        print("Au revoir!")
    else:
        print("Choix invalide")


if __name__ == "__main__":
    main()
