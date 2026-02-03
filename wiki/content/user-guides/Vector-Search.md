# Vector Search

**Last Updated:** 2026-02-03

---

ScratchBird includes a vector type and vector indexes (HNSW/IVF) for similarity
search.

## Create a Vector Column

```sql
CREATE TABLE embeddings (
    id INTEGER PRIMARY KEY,
    vec VECTOR(1536)
);
```

## Indexing

```sql
CREATE INDEX idx_embeddings_vec ON embeddings USING HNSW (vec);
```

## Query Example

```sql
SELECT id
FROM embeddings
ORDER BY vec <-> :query_vec
LIMIT 10;
```

---

*Last updated: 2026-02-03 | Wiki version synced with codebase*
