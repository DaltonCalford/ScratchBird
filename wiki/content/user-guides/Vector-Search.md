# Vector Search

**Status:** Alpha documentation
**Last Updated:** 2026-01-19

---

## Overview

ScratchBird provides vector search capabilities for building AI-powered applications, semantic search, recommendation systems, and more. Store high-dimensional vectors alongside your relational data and perform efficient similarity searches.

**Topics covered:**
- Vector data types
- Creating vector columns
- Similarity search operations
- HNSW indexing
- Use cases and examples

---

## Part 1: Vector Basics

### What Are Vectors?

Vectors are arrays of numbers that represent data in high-dimensional space. They're commonly used to represent:

- **Text embeddings** - Semantic meaning of sentences, documents
- **Image embeddings** - Visual features of images
- **User preferences** - Behavioral patterns for recommendations
- **Product features** - Characteristics for similarity matching

```sql
-- A 3-dimensional vector
SELECT '[1.0, 2.5, 3.7]'::vector(3);

-- A 1536-dimensional embedding (OpenAI ada-002)
-- '[0.0123, -0.0456, 0.0789, ...]'::vector(1536)
```

### Vector Data Type

**Syntax:**
```sql
-- Vector type with dimension
vector(dimensions)

-- Examples
vector(3)      -- 3-dimensional vector
vector(384)    -- Sentence transformer embedding
vector(768)    -- BERT embedding
vector(1536)   -- OpenAI text-embedding-ada-002
vector(3072)   -- OpenAI text-embedding-3-large
```

**Creating tables with vectors:**
```sql
CREATE TABLE documents (
    id SERIAL PRIMARY KEY,
    title VARCHAR(255),
    content TEXT,
    embedding vector(1536)  -- Store OpenAI embeddings
);

CREATE TABLE products (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255),
    description TEXT,
    image_embedding vector(512),   -- Image features
    text_embedding vector(384)     -- Text features
);
```

---

## Part 2: Inserting Vector Data

### Basic Insert

```sql
-- Insert a vector literal
INSERT INTO documents (title, content, embedding)
VALUES (
    'Introduction to AI',
    'Artificial intelligence is...',
    '[0.1, 0.2, 0.3, ...]'::vector(1536)
);

-- Insert from array
INSERT INTO documents (title, content, embedding)
VALUES (
    'Machine Learning Basics',
    'Machine learning uses...',
    ARRAY[0.1, 0.2, 0.3, ...]::vector(1536)
);
```

### Inserting from Application Code

**Python with psycopg2:**
```python
import psycopg2
import openai

# Generate embedding
response = openai.Embedding.create(
    input="Introduction to artificial intelligence",
    model="text-embedding-ada-002"
)
embedding = response['data'][0]['embedding']

# Insert into database
conn = psycopg2.connect(...)
cursor = conn.cursor()
cursor.execute(
    """
    INSERT INTO documents (title, content, embedding)
    VALUES (%s, %s, %s)
    """,
    ("Introduction to AI", "Artificial intelligence is...", embedding)
)
conn.commit()
```

**Node.js:**
```javascript
const { Pool } = require('pg');
const OpenAI = require('openai');

const pool = new Pool({ ... });
const openai = new OpenAI();

async function insertDocument(title, content) {
    const embedding = await openai.embeddings.create({
        input: content,
        model: 'text-embedding-ada-002'
    });

    await pool.query(
        'INSERT INTO documents (title, content, embedding) VALUES ($1, $2, $3)',
        [title, content, JSON.stringify(embedding.data[0].embedding)]
    );
}
```

### Batch Insert

```python
# Batch insert for efficiency
documents = [
    ("Doc 1", "Content 1", embedding1),
    ("Doc 2", "Content 2", embedding2),
    ("Doc 3", "Content 3", embedding3),
]

cursor.executemany(
    """
    INSERT INTO documents (title, content, embedding)
    VALUES (%s, %s, %s)
    """,
    documents
)
conn.commit()
```

---

## Part 3: Similarity Search

### Distance Functions

ScratchBird supports several distance metrics for vector comparison:

| Function | Operator | Description | Best For |
|----------|----------|-------------|----------|
| L2 distance | `<->` | Euclidean distance | General purpose |
| Inner product | `<#>` | Negative inner product | Normalized vectors |
| Cosine distance | `<=>` | 1 - cosine similarity | Text embeddings |

### Basic Similarity Search

**Find similar documents (L2 distance):**
```sql
-- Find 5 most similar documents
SELECT id, title, embedding <-> '[0.1, 0.2, ...]'::vector AS distance
FROM documents
ORDER BY embedding <-> '[0.1, 0.2, ...]'::vector
LIMIT 5;
```

**Cosine similarity (recommended for text):**
```sql
-- Find semantically similar documents
SELECT
    id,
    title,
    1 - (embedding <=> '[0.1, 0.2, ...]'::vector) AS similarity
FROM documents
ORDER BY embedding <=> '[0.1, 0.2, ...]'::vector
LIMIT 5;
```

**Inner product (for normalized vectors):**
```sql
-- If vectors are normalized to unit length
SELECT
    id,
    title,
    (embedding <#> '[0.1, 0.2, ...]'::vector) * -1 AS similarity
FROM documents
ORDER BY embedding <#> '[0.1, 0.2, ...]'::vector
LIMIT 5;
```

### Search with Filters

**Combine vector search with SQL filters:**
```sql
-- Search within a category
SELECT id, title,
    1 - (embedding <=> $1) AS similarity
FROM documents
WHERE category = 'technology'
ORDER BY embedding <=> $1
LIMIT 10;

-- Search with date filter
SELECT id, title,
    1 - (embedding <=> $1) AS similarity
FROM documents
WHERE created_at > '2026-01-01'
ORDER BY embedding <=> $1
LIMIT 10;

-- Search with multiple conditions
SELECT id, title, author,
    1 - (embedding <=> $1) AS similarity
FROM documents
WHERE status = 'published'
AND language = 'en'
ORDER BY embedding <=> $1
LIMIT 10;
```

### Similarity Threshold

```sql
-- Only return results above similarity threshold
SELECT id, title,
    1 - (embedding <=> $1) AS similarity
FROM documents
WHERE 1 - (embedding <=> $1) > 0.8  -- 80% similarity threshold
ORDER BY embedding <=> $1
LIMIT 10;
```

---

## Part 4: HNSW Indexing

### Why Use HNSW Index?

Without an index, vector searches perform a sequential scan comparing every vector. HNSW (Hierarchical Navigable Small World) indexes enable approximate nearest neighbor (ANN) search that's orders of magnitude faster.

| Rows | Sequential Scan | HNSW Index |
|------|-----------------|------------|
| 10,000 | ~50ms | ~1ms |
| 100,000 | ~500ms | ~5ms |
| 1,000,000 | ~5s | ~10ms |

### Creating HNSW Index

```sql
-- Basic HNSW index for L2 distance
CREATE INDEX idx_documents_embedding ON documents
USING hnsw (embedding vector_l2_ops);

-- For cosine distance (most common for text)
CREATE INDEX idx_documents_embedding ON documents
USING hnsw (embedding vector_cosine_ops);

-- For inner product
CREATE INDEX idx_documents_embedding ON documents
USING hnsw (embedding vector_ip_ops);
```

### Index Parameters

```sql
-- Tune HNSW parameters
CREATE INDEX idx_documents_embedding ON documents
USING hnsw (embedding vector_cosine_ops)
WITH (
    m = 16,              -- Max connections per layer (default: 16)
    ef_construction = 64 -- Build-time search width (default: 64)
);
```

**Parameter guidelines:**

| Parameter | Low | Medium | High |
|-----------|-----|--------|------|
| m | 8 | 16 | 32 |
| ef_construction | 32 | 64 | 128 |
| **Index size** | Smaller | Medium | Larger |
| **Build time** | Faster | Medium | Slower |
| **Recall** | Lower | Medium | Higher |

### Search Parameters

```sql
-- Set search-time parameter
SET hnsw.ef_search = 100;  -- Higher = better recall, slower

-- Run search
SELECT id, title
FROM documents
ORDER BY embedding <=> $1
LIMIT 10;

-- Reset to default
RESET hnsw.ef_search;
```

### Partial Indexes

```sql
-- Index only active documents
CREATE INDEX idx_active_docs_embedding ON documents
USING hnsw (embedding vector_cosine_ops)
WHERE status = 'active';

-- Queries must include the filter to use index
SELECT * FROM documents
WHERE status = 'active'
ORDER BY embedding <=> $1
LIMIT 10;
```

---

## Part 5: Use Cases

### Semantic Search

**Search by meaning, not keywords:**

```python
def semantic_search(query_text, limit=10):
    # Generate embedding for query
    response = openai.Embedding.create(
        input=query_text,
        model="text-embedding-ada-002"
    )
    query_embedding = response['data'][0]['embedding']

    # Search database
    cursor.execute("""
        SELECT id, title, content,
            1 - (embedding <=> %s) AS similarity
        FROM documents
        WHERE 1 - (embedding <=> %s) > 0.7
        ORDER BY embedding <=> %s
        LIMIT %s
    """, (query_embedding, query_embedding, query_embedding, limit))

    return cursor.fetchall()

# Find documents about machine learning
# even if they don't contain those exact words
results = semantic_search("how do computers learn from data")
```

### RAG (Retrieval-Augmented Generation)

**Enhance LLM responses with relevant context:**

```python
def rag_query(user_question):
    # 1. Get relevant documents
    relevant_docs = semantic_search(user_question, limit=5)

    # 2. Build context
    context = "\n\n".join([doc['content'] for doc in relevant_docs])

    # 3. Generate response with context
    response = openai.ChatCompletion.create(
        model="gpt-4",
        messages=[
            {"role": "system", "content": f"Answer based on this context:\n{context}"},
            {"role": "user", "content": user_question}
        ]
    )

    return response.choices[0].message.content
```

### Product Recommendations

**Find similar products:**

```sql
-- Product schema
CREATE TABLE products (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255),
    description TEXT,
    category VARCHAR(100),
    price DECIMAL(10,2),
    embedding vector(384)
);

-- Create index
CREATE INDEX idx_products_embedding ON products
USING hnsw (embedding vector_cosine_ops);

-- Find similar products (excluding the source product)
SELECT
    p.id,
    p.name,
    p.price,
    1 - (p.embedding <=> source.embedding) AS similarity
FROM products p
CROSS JOIN (SELECT embedding FROM products WHERE id = 123) source
WHERE p.id != 123
ORDER BY p.embedding <=> source.embedding
LIMIT 5;
```

**Recommendation with filters:**

```sql
-- Similar products in same price range
SELECT
    p.id,
    p.name,
    p.price,
    1 - (p.embedding <=> $1) AS similarity
FROM products p
WHERE p.category = $2
AND p.price BETWEEN $3 AND $4
AND p.id != $5
ORDER BY p.embedding <=> $1
LIMIT 5;
```

### Image Similarity

**Find similar images:**

```sql
CREATE TABLE images (
    id SERIAL PRIMARY KEY,
    filename VARCHAR(255),
    tags TEXT[],
    embedding vector(512)  -- Image embedding from CLIP, ResNet, etc.
);

CREATE INDEX idx_images_embedding ON images
USING hnsw (embedding vector_cosine_ops);

-- Find similar images
SELECT id, filename, 1 - (embedding <=> $1) AS similarity
FROM images
ORDER BY embedding <=> $1
LIMIT 20;
```

### Hybrid Search

**Combine keyword and vector search:**

```sql
-- Create full-text search index
CREATE INDEX idx_documents_fts ON documents
USING gin(to_tsvector('english', content));

-- Hybrid search function
CREATE FUNCTION hybrid_search(
    query_text TEXT,
    query_embedding vector(1536),
    keyword_weight FLOAT DEFAULT 0.3,
    vector_weight FLOAT DEFAULT 0.7,
    result_limit INT DEFAULT 10
)
RETURNS TABLE (
    id INT,
    title VARCHAR,
    combined_score FLOAT
)
LANGUAGE SQL AS $$
    WITH keyword_scores AS (
        SELECT id,
            ts_rank(to_tsvector('english', content), plainto_tsquery('english', query_text)) AS score
        FROM documents
        WHERE to_tsvector('english', content) @@ plainto_tsquery('english', query_text)
    ),
    vector_scores AS (
        SELECT id,
            1 - (embedding <=> query_embedding) AS score
        FROM documents
    )
    SELECT
        d.id,
        d.title,
        COALESCE(k.score, 0) * keyword_weight +
        COALESCE(v.score, 0) * vector_weight AS combined_score
    FROM documents d
    LEFT JOIN keyword_scores k ON d.id = k.id
    LEFT JOIN vector_scores v ON d.id = v.id
    ORDER BY combined_score DESC
    LIMIT result_limit;
$$;
```

---

## Part 6: Best Practices

### Choosing Embedding Dimensions

| Model | Dimensions | Best For |
|-------|------------|----------|
| text-embedding-3-small | 1536 | General text (balance) |
| text-embedding-3-large | 3072 | High accuracy needs |
| text-embedding-ada-002 | 1536 | Legacy, still good |
| Sentence Transformers | 384-768 | Open source, fast |
| CLIP | 512 | Images + text |

### Normalizing Vectors

```sql
-- Normalize vectors for consistent similarity scores
UPDATE documents
SET embedding = embedding / sqrt(embedding <-> ARRAY_FILL(0, ARRAY[1536])::vector);

-- Or normalize on insert
INSERT INTO documents (title, embedding)
VALUES (
    'New Document',
    (embedding_array::vector / |/embedding_array::vector)
);
```

### Batch Processing

```python
# Generate embeddings in batches
def batch_embed(texts, batch_size=100):
    embeddings = []
    for i in range(0, len(texts), batch_size):
        batch = texts[i:i+batch_size]
        response = openai.Embedding.create(
            input=batch,
            model="text-embedding-ada-002"
        )
        embeddings.extend([e['embedding'] for e in response['data']])
    return embeddings
```

### Index Maintenance

```sql
-- Monitor index usage
SELECT
    indexrelname,
    idx_scan,
    idx_tup_read,
    idx_tup_fetch
FROM pg_stat_user_indexes
WHERE indexrelname LIKE '%embedding%';

-- Rebuild index if needed
REINDEX INDEX idx_documents_embedding;
```

### Memory Considerations

```ini
# sb_server.conf
# Increase work_mem for vector operations
work_mem = 64MB

# Ensure enough buffer_pool_size for index
buffer_pool_size = 2GB
```

---

## Part 7: Troubleshooting

### Index Not Being Used

**Check explain plan:**
```sql
EXPLAIN ANALYZE
SELECT * FROM documents
ORDER BY embedding <=> $1
LIMIT 10;
```

**Common causes:**
1. Wrong operator class (use matching ops for your distance function)
2. Query returns too many rows (try adding LIMIT)
3. Index not created yet

**Solution:**
```sql
-- Verify index exists and operator class
SELECT indexname, indexdef
FROM pg_indexes
WHERE tablename = 'documents';

-- Force index usage (for testing)
SET enable_seqscan = off;
```

### Slow Searches

**Tune HNSW parameters:**
```sql
-- Increase ef_search for better accuracy
SET hnsw.ef_search = 200;

-- Or rebuild index with higher parameters
DROP INDEX idx_documents_embedding;
CREATE INDEX idx_documents_embedding ON documents
USING hnsw (embedding vector_cosine_ops)
WITH (m = 32, ef_construction = 128);
```

### Memory Errors

**Reduce batch sizes:**
```python
# Process smaller batches
for batch in chunks(documents, 100):
    process_batch(batch)
    conn.commit()
```

### Dimension Mismatch

```sql
-- Error: different vector dimensions
-- Solution: Ensure all vectors have same dimensions

-- Check existing dimensions
SELECT id, array_length(embedding::float[], 1) as dims
FROM documents
LIMIT 10;

-- Add constraint to prevent mismatches
ALTER TABLE documents
ADD CONSTRAINT check_embedding_dims
CHECK (array_length(embedding::float[], 1) = 1536);
```

---

## Quick Reference

### Essential Commands

| Task | Command |
|------|---------|
| Create vector column | `embedding vector(1536)` |
| L2 distance | `embedding <-> query_vec` |
| Cosine distance | `embedding <=> query_vec` |
| Inner product | `embedding <#> query_vec` |
| Create HNSW index | `CREATE INDEX ... USING hnsw (col vector_cosine_ops)` |
| Set search quality | `SET hnsw.ef_search = 100` |

### Distance Function Operators

| Operator | Function | Index Ops Class |
|----------|----------|-----------------|
| `<->` | L2 distance | `vector_l2_ops` |
| `<=>` | Cosine distance | `vector_cosine_ops` |
| `<#>` | Inner product | `vector_ip_ops` |

### Common Embedding Sizes

| Model/Source | Dimensions |
|--------------|------------|
| OpenAI ada-002 | 1536 |
| OpenAI text-embedding-3-small | 1536 |
| OpenAI text-embedding-3-large | 3072 |
| Cohere embed | 1024 / 4096 |
| sentence-transformers/all-MiniLM-L6-v2 | 384 |
| CLIP ViT-B/32 | 512 |

---

## See Also

- [Performance Tuning](Performance-Tuning.md) - Optimize vector search performance
- [Indexes Guide](Indexes.md) - General index documentation
- [HNSW Index Specification](../../docs/specifications/indexes/HNSW_INDEX_SPEC.md) - Technical details
- [Tutorials](../tutorials/) - Building applications with vectors

