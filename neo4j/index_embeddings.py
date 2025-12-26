"""
index_embeddings.py
Script d'indexation des embeddings CamemBERT pour tous les concepts en mémoire.

Ce script parcourt tous les nœuds Concept dans Neo4j et génère leurs embeddings
CamemBERT pour permettre la recherche sémantique.

Usage:
    python index_embeddings.py [--batch-size 32] [--force]
"""

import os
import sys
import argparse
import logging
from typing import List, Dict, Optional
from datetime import datetime

import numpy as np
from neo4j import GraphDatabase
from neo4j.exceptions import ServiceUnavailable

# Configuration du logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S'
)
logger = logging.getLogger(__name__)


class EmbeddingIndexer:
    """Indexeur d'embeddings pour les concepts Neo4j"""

    def __init__(
        self,
        neo4j_uri: str = "bolt://localhost:7687",
        neo4j_user: str = "neo4j",
        neo4j_password: str = "",
        device: str = "cpu",
        camembert_model: str = "camembert-base"
    ):
        """
        Initialise l'indexeur.

        Args:
            neo4j_uri: URI Neo4j
            neo4j_user: Utilisateur Neo4j
            neo4j_password: Mot de passe (vide = pas d'auth)
            device: Device pour CamemBERT ("cpu" ou "cuda")
            camembert_model: Modèle à utiliser
        """
        self.device = device
        self.camembert_model = camembert_model

        # Connexion Neo4j
        logger.info(f"Connexion à Neo4j: {neo4j_uri}")
        if neo4j_password:
            self.driver = GraphDatabase.driver(
                neo4j_uri,
                auth=(neo4j_user, neo4j_password)
            )
        else:
            self.driver = GraphDatabase.driver(neo4j_uri)

        # Vérifier la connexion
        with self.driver.session() as session:
            session.run("RETURN 1")
        logger.info("Connexion Neo4j établie")

        # Charger CamemBERT
        self._tokenizer = None
        self._model = None

    @property
    def tokenizer(self):
        if self._tokenizer is None:
            logger.info(f"Chargement tokenizer {self.camembert_model}...")
            from transformers import CamembertTokenizer
            self._tokenizer = CamembertTokenizer.from_pretrained(self.camembert_model)
        return self._tokenizer

    @property
    def model(self):
        if self._model is None:
            logger.info(f"Chargement modèle {self.camembert_model}...")
            import torch
            from transformers import CamembertModel
            self._model = CamembertModel.from_pretrained(self.camembert_model)
            self._model.to(self.device)
            self._model.eval()
            logger.info(f"Modèle chargé sur {self.device}")
        return self._model

    def close(self):
        """Ferme les connexions"""
        self.driver.close()

    def get_concepts_without_embedding(self, limit: Optional[int] = None) -> List[Dict]:
        """
        Récupère les concepts sans embedding.

        Args:
            limit: Nombre max de concepts (None = tous)

        Returns:
            Liste de {name: str, ...}
        """
        query = """
        MATCH (c:Concept)
        WHERE c.embedding IS NULL
        RETURN c.name AS name
        """
        if limit:
            query += f" LIMIT {limit}"

        with self.driver.session() as session:
            records = session.run(query)
            return [{"name": r["name"]} for r in records]

    def get_all_concepts(self) -> List[Dict]:
        """Récupère tous les concepts"""
        query = """
        MATCH (c:Concept)
        RETURN c.name AS name, c.embedding IS NOT NULL AS has_embedding
        """
        with self.driver.session() as session:
            records = session.run(query)
            return [{"name": r["name"], "has_embedding": r["has_embedding"]} for r in records]

    def generate_embedding(self, text: str) -> np.ndarray:
        """
        Génère l'embedding pour un texte.

        Args:
            text: Texte à encoder

        Returns:
            Vecteur numpy de dimension 768
        """
        import torch

        if not text or not text.strip():
            return np.zeros(768)

        inputs = self.tokenizer(
            text,
            return_tensors="pt",
            padding=True,
            truncation=True,
            max_length=512
        )
        inputs = {k: v.to(self.device) for k, v in inputs.items()}

        with torch.no_grad():
            outputs = self.model(**inputs)

        attention_mask = inputs["attention_mask"]
        token_embeddings = outputs.last_hidden_state

        input_mask_expanded = attention_mask.unsqueeze(-1).expand(token_embeddings.size()).float()
        sum_embeddings = torch.sum(token_embeddings * input_mask_expanded, 1)
        sum_mask = torch.clamp(input_mask_expanded.sum(1), min=1e-9)
        embedding = (sum_embeddings / sum_mask).squeeze().cpu().numpy()

        return embedding

    def generate_embeddings_batch(self, texts: List[str]) -> List[np.ndarray]:
        """
        Génère les embeddings pour un batch de textes.

        Args:
            texts: Liste de textes

        Returns:
            Liste d'embeddings
        """
        import torch

        if not texts:
            return []

        # Filtrer les textes vides
        valid_texts = [(i, t) for i, t in enumerate(texts) if t and t.strip()]
        if not valid_texts:
            return [np.zeros(768) for _ in texts]

        indices, batch_texts = zip(*valid_texts)

        inputs = self.tokenizer(
            list(batch_texts),
            return_tensors="pt",
            padding=True,
            truncation=True,
            max_length=512
        )
        inputs = {k: v.to(self.device) for k, v in inputs.items()}

        with torch.no_grad():
            outputs = self.model(**inputs)

        attention_mask = inputs["attention_mask"]
        token_embeddings = outputs.last_hidden_state

        input_mask_expanded = attention_mask.unsqueeze(-1).expand(token_embeddings.size()).float()
        sum_embeddings = torch.sum(token_embeddings * input_mask_expanded, 1)
        sum_mask = torch.clamp(input_mask_expanded.sum(1), min=1e-9)
        embeddings = (sum_embeddings / sum_mask).cpu().numpy()

        # Reconstituer la liste avec les textes vides
        results = [np.zeros(768) for _ in texts]
        for i, emb in zip(indices, embeddings):
            results[i] = emb

        return results

    def update_embedding(self, concept_name: str, embedding: np.ndarray) -> bool:
        """
        Met à jour l'embedding d'un concept.

        Args:
            concept_name: Nom du concept
            embedding: Vecteur embedding

        Returns:
            True si succès
        """
        query = """
        MATCH (c:Concept {name: $name})
        SET c.embedding = $embedding,
            c.embedding_updated_at = datetime()
        RETURN c.name AS name
        """
        with self.driver.session() as session:
            result = session.run(
                query,
                name=concept_name,
                embedding=embedding.tolist()
            )
            return result.single() is not None

    def index_all(
        self,
        batch_size: int = 32,
        force: bool = False,
        progress_callback=None
    ) -> Dict:
        """
        Indexe tous les concepts.

        Args:
            batch_size: Taille des batchs
            force: Ré-indexer même si embedding existe
            progress_callback: Callback(current, total, concept_name)

        Returns:
            Statistiques d'indexation
        """
        start_time = datetime.now()

        # Récupérer les concepts à indexer
        if force:
            concepts = self.get_all_concepts()
            concepts_to_index = [c for c in concepts]
        else:
            concepts_to_index = self.get_concepts_without_embedding()

        total = len(concepts_to_index)
        logger.info(f"Concepts à indexer: {total}")

        if total == 0:
            logger.info("Aucun concept à indexer")
            return {
                "indexed": 0,
                "total": 0,
                "duration_seconds": 0,
                "status": "nothing_to_do"
            }

        indexed = 0
        errors = 0

        # Traiter par batch
        for batch_start in range(0, total, batch_size):
            batch_end = min(batch_start + batch_size, total)
            batch = concepts_to_index[batch_start:batch_end]

            # Extraire les noms
            names = [c["name"] for c in batch]

            # Générer les embeddings
            try:
                embeddings = self.generate_embeddings_batch(names)

                # Mettre à jour Neo4j
                for i, (concept, embedding) in enumerate(zip(batch, embeddings)):
                    try:
                        self.update_embedding(concept["name"], embedding)
                        indexed += 1

                        if progress_callback:
                            progress_callback(indexed, total, concept["name"])

                    except Exception as e:
                        logger.error(f"Erreur mise à jour {concept['name']}: {e}")
                        errors += 1

            except Exception as e:
                logger.error(f"Erreur batch {batch_start}-{batch_end}: {e}")
                errors += len(batch)

            # Log progression
            if (batch_end) % (batch_size * 5) == 0 or batch_end == total:
                logger.info(f"Progression: {batch_end}/{total} ({100*batch_end/total:.1f}%)")

        duration = (datetime.now() - start_time).total_seconds()

        stats = {
            "indexed": indexed,
            "errors": errors,
            "total": total,
            "duration_seconds": duration,
            "concepts_per_second": indexed / duration if duration > 0 else 0,
            "status": "completed"
        }

        logger.info(f"Indexation terminée: {indexed}/{total} en {duration:.1f}s")
        if errors:
            logger.warning(f"Erreurs: {errors}")

        return stats


def create_vector_index(driver, dimension: int = 768):
    """
    Crée l'index vectoriel Neo4j (si supporté).

    Args:
        driver: Driver Neo4j
        dimension: Dimension des vecteurs

    Note: Nécessite Neo4j 5.11+
    """
    query = """
    CREATE VECTOR INDEX concept_embeddings IF NOT EXISTS
    FOR (c:Concept) ON c.embedding
    OPTIONS {
        indexConfig: {
            `vector.dimensions`: $dimension,
            `vector.similarity_function`: 'cosine'
        }
    }
    """
    try:
        with driver.session() as session:
            session.run(query, dimension=dimension)
            logger.info(f"Index vectoriel créé (dimension={dimension})")
            return True
    except Exception as e:
        logger.warning(f"Index vectoriel non créé (Neo4j < 5.11?): {e}")
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Indexe les embeddings CamemBERT pour les concepts Neo4j"
    )
    parser.add_argument(
        "--batch-size", "-b",
        type=int,
        default=32,
        help="Taille des batchs (défaut: 32)"
    )
    parser.add_argument(
        "--force", "-f",
        action="store_true",
        help="Ré-indexer tous les concepts (même ceux avec embedding)"
    )
    parser.add_argument(
        "--create-index",
        action="store_true",
        help="Créer l'index vectoriel Neo4j"
    )
    parser.add_argument(
        "--neo4j-uri",
        default=os.getenv("NEO4J_URI", "bolt://localhost:7687"),
        help="URI Neo4j"
    )
    parser.add_argument(
        "--neo4j-password",
        default=os.getenv("NEO4J_PASSWORD", ""),
        help="Mot de passe Neo4j"
    )
    parser.add_argument(
        "--device",
        default="cuda" if os.getenv("CUDA_VISIBLE_DEVICES") else "cpu",
        choices=["cpu", "cuda"],
        help="Device pour CamemBERT"
    )

    args = parser.parse_args()

    print("=" * 60)
    print("INDEXATION DES EMBEDDINGS CAMEMBERT")
    print("=" * 60)
    print(f"Neo4j URI: {args.neo4j_uri}")
    print(f"Device: {args.device}")
    print(f"Batch size: {args.batch_size}")
    print(f"Force: {args.force}")
    print("=" * 60)

    try:
        indexer = EmbeddingIndexer(
            neo4j_uri=args.neo4j_uri,
            neo4j_password=args.neo4j_password,
            device=args.device
        )

        # Créer l'index vectoriel si demandé
        if args.create_index:
            print("\nCréation de l'index vectoriel...")
            create_vector_index(indexer.driver)

        # Indexer
        print("\nDémarrage de l'indexation...")

        def progress(current, total, name):
            if current % 100 == 0 or current == total:
                print(f"  [{current}/{total}] {name}")

        stats = indexer.index_all(
            batch_size=args.batch_size,
            force=args.force,
            progress_callback=progress
        )

        print("\n" + "=" * 60)
        print("RÉSULTATS")
        print("=" * 60)
        print(f"Concepts indexés: {stats['indexed']}/{stats['total']}")
        if stats.get('errors'):
            print(f"Erreurs: {stats['errors']}")
        print(f"Durée: {stats['duration_seconds']:.1f}s")
        if stats.get('concepts_per_second'):
            print(f"Vitesse: {stats['concepts_per_second']:.1f} concepts/s")
        print("=" * 60)

        indexer.close()
        return 0

    except ServiceUnavailable:
        print("\n[ERREUR] Neo4j n'est pas accessible.")
        print("Vérifiez que Neo4j est démarré et accessible.")
        return 1
    except Exception as e:
        print(f"\n[ERREUR] {type(e).__name__}: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
