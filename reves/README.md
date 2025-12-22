# Module Rêves et Mémoire

Service autonome de consolidation nocturne des mémoires.

## Build

```bash
mkdir build && cd build
cmake ..
make
```

## Run

```bash
./reves --host localhost --port 5672 --user virtus --pass virtus@83 --cycle 12.0
```

## Docker

```bash
docker build -t mcee-reves .
docker run --network mcee-net mcee-reves --host rabbitmq
```

## Queues RabbitMQ

**Entrées:**
- `mcee.memory.episodic` - Mémoire épisodique (ME)
- `mcee.memory.semantic` - Mémoire sémantique (MS)
- `mcee.memory.procedural` - Mémoire procédurale (MP)
- `mcee.memory.autobiographic` - Mémoire autobiographique (MA)
- `mcee.mct.snapshot` - Buffer MCT
- `mcee.pattern.active` - Pattern émotionnel actif
- `mcee.amyghaleon.alerts` - Alertes urgence

**Sorties:**
- `mcee.mlt.consolidate` - Consolidation vers MLT
- `mcee.mlt.create_edge` - Création d'arêtes
- `mcee.mlt.forget` - Oubli
- `mcee.dream.status` - Statut du module

## Format messages

### Mémoire (entrée)
```json
{
  "id": "mem_123",
  "type": "episodic",
  "is_social": true,
  "interlocuteur": "Alice",
  "feedback": 0.7,
  "usage_count": 3,
  "is_trauma": false,
  "emotional_vector": [0.1, 0.2, ...]
}
```

### Pattern (entrée)
```json
{
  "pattern": "SERENITE",
  "emotions": [0.1, 0.2, ...]
}
```

### Consolidation (sortie)
```json
{
  "action": "consolidate",
  "memory": {
    "id": "mem_123",
    "score": 0.72,
    "emotional_vector": [...]
  }
}
```
