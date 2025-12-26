// Migration 001: Ajout de l'index vectoriel pour les embeddings
// Nécessite Neo4j 5.11+ pour l'index vectoriel natif
//
// Pour exécuter cette migration:
//   cypher-shell -f migrations/001_add_vector_index.cypher
//
// Ou via Python:
//   with driver.session() as session:
//       session.run(open('migrations/001_add_vector_index.cypher').read())

// ═══════════════════════════════════════════════════════════════════════════════
// 1. Ajouter la propriété embedding aux Concepts (si non existante)
// ═══════════════════════════════════════════════════════════════════════════════

// Initialiser embedding à null pour les concepts qui n'en ont pas
MATCH (c:Concept)
WHERE c.embedding IS NULL
SET c.embedding = null,
    c.embedding_updated_at = null;

// ═══════════════════════════════════════════════════════════════════════════════
// 2. Créer l'index vectoriel (Neo4j 5.11+)
// ═══════════════════════════════════════════════════════════════════════════════

// Index vectoriel pour recherche sémantique rapide
// Dimension 768 = CamemBERT base
// Similarité cosinus pour correspondance sémantique
CREATE VECTOR INDEX concept_embeddings IF NOT EXISTS
FOR (c:Concept) ON c.embedding
OPTIONS {
    indexConfig: {
        `vector.dimensions`: 768,
        `vector.similarity_function`: 'cosine'
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// 3. Index standard pour la recherche lexicale (nom en minuscules)
// ═══════════════════════════════════════════════════════════════════════════════

// Index texte pour recherche par nom
CREATE TEXT INDEX concept_name_text IF NOT EXISTS
FOR (c:Concept) ON (c.name);

// Index standard pour recherche exacte
CREATE INDEX concept_name IF NOT EXISTS
FOR (c:Concept) ON (c.name);

// ═══════════════════════════════════════════════════════════════════════════════
// 4. Index pour les Memory IDs (optimisation des jointures)
// ═══════════════════════════════════════════════════════════════════════════════

CREATE INDEX memory_id IF NOT EXISTS
FOR (m:Memory) ON (m.id);

// ═══════════════════════════════════════════════════════════════════════════════
// 5. Vérification
// ═══════════════════════════════════════════════════════════════════════════════

// Afficher les index créés
SHOW INDEXES
YIELD name, type, labelsOrTypes, properties, state
WHERE labelsOrTypes = ['Concept'] OR labelsOrTypes = ['Memory']
RETURN name, type, labelsOrTypes, properties, state;
