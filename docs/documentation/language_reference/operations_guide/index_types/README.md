<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# Index Types Guide

[Language Reference README](../../README.md) | [Operations Guide README](../README.md)

## Coverage and Evidence Status

Status: Complete

## Synopsis

ScratchBird implements 11 core index types, each optimized for specific query patterns and data characteristics. This guide describes each type, its use cases, and selection criteria.

## Index Type Overview

| # | Type | Use Case | Best For |
|---|------|----------|----------|
| 1 | [BTREE](#btree) | B-tree balanced tree | Equality, range, sorting |
| 2 | [HASH](#hash) | Hash table | Exact match equality |
| 3 | [RTREE](#rtree) | R-tree spatial | 2D/3D geometric data |
| 4 | [FULLTEXT](#fulltext) | Inverted text index | Text search, relevance |
| 5 | [BITMAP](#bitmap) | Bitmap compression | Low cardinality, OLAP |
| 6 | [GIN](#gin) | Generalized Inverted | Arrays, JSONB, composites |
| 7 | [GIST](#gist) | Generalized Search Tree | Ranges, spatial, custom |
| 8 | [VECTOR](#vector) | Approximate NN | Vector similarity search |
| 9 | [LSM](#lsm) | Log-Structured Merge | Write-heavy, time-series |
| 10 | [COLUMNAR](#columnar) | Column-oriented | Analytics, aggregation |
| 11 | [COMPOUND](#compound) | Multi-column/Intersection | Complex filters |

## Selection Flowchart

```
What is your query pattern?
│
├─> Exact equality (single value lookup)
│   ├─> Small table or cache-hot → BTREE
│   └─> Large table, read-heavy → HASH
│
├─> Range queries (<, >, BETWEEN)
│   └─> BTREE
│
├─> Text search (contains words)
│   └─> FULLTEXT
│
├─> Array/JSONB contains element
│   └─> GIN
│
├─> Spatial (distance, overlap, within)
│   ├─> Simple 2D bounding box → RTREE
│   └─> Complex geometries → GIST
│
├─> Vector similarity (embeddings)
│   └─> VECTOR (HNSW/IVF)
│
├─> Time-series append-mostly
│   └─> LSM
│
├─> Analytical aggregation (SUM, AVG)
│   └─> COLUMNAR
│
├─> Low cardinality filter (status = 'active')
│   └─> BITMAP
│
└─> Multiple combined conditions
    └─> COMPOUND or multiple indexes
```

---

## 1. BTREE

B-tree (balanced tree) index - the default and most versatile index type.

### Use Cases
- Primary keys
- Foreign keys
- Range queries (`<`, `<=`, `>`, `>=`, `BETWEEN`)
- Prefix matching (`LIKE 'prefix%'`)
- Ordering (`ORDER BY`)

### Characteristics
| Property | Value |
|----------|-------|
| Time complexity | O(log n) search, insert, delete |
| Space overhead | ~20-30% of table size |
| Supports multi-column | Yes (up to 32 columns) |
| Supports expressions | Yes |
| Supports partial | Yes |
| Lock granularity | Page-level |

### Example
```sql
-- Single column
CREATE INDEX idx_users_email ON users(email);

-- Multi-column for combined queries
CREATE INDEX idx_orders_user_date ON orders(user_id, created_at);

-- Functional for case-insensitive search
CREATE INDEX idx_users_lower_email ON users(LOWER(email));

-- Partial for active users only
CREATE INDEX idx_users_active ON users(email) WHERE status = 'active';
```

### Best Practices
- Order columns by selectivity (most selective first)
- Include columns used in WHERE, JOIN, ORDER BY
- Consider covering indexes (INCLUDE) for index-only scans

---

## 2. HASH

Hash table index optimized for exact equality lookups.

### Use Cases
- Exact match queries (`=`)
- High-cardinality columns (unique values)
- Large tables where BTREE would be deep

### Characteristics
| Property | Value |
|----------|-------|
| Time complexity | O(1) average case |
| Space overhead | ~15-25% of table size |
| Supports multi-column | No (single column only) |
| Supports range queries | No |
| Collision handling | Chaining |

### Example
```sql
-- Email lookup (exact match)
CREATE INDEX idx_users_email_hash ON users USING HASH (email);

-- API key lookup
CREATE INDEX idx_api_keys_hash ON api_keys USING HASH (key_hash);
```

### Limitations
- Cannot use for `>`, `<`, `BETWEEN`
- Cannot use for `ORDER BY`
- Cannot use for `LIKE` (even `LIKE 'prefix%'`)

---

## 3. RTREE

R-tree (rectangle tree) for spatial indexing.

### Use Cases
- Geographic data (points, polygons)
- Nearest neighbor search
- Overlap/containment queries
- 2D and 3D bounding box operations

### Characteristics
| Property | Value |
|----------|-------|
| Time complexity | O(log n) average for search |
| Supports geometries | Point, Box, Polygon, Circle |
| Supports operators | `&&` (overlap), `@>` (contains), `<@` (within) |
| Page size impact | Larger pages (32K+) recommended |

### Example
```sql
-- Location-based indexing
CREATE INDEX idx_locations_geom ON locations USING RTREE (geom);

-- Query: Find points within bounding box
SELECT * FROM locations 
WHERE geom && BOX(POINT(-74, 40), POINT(-73, 41));

-- Query: Find nearest locations
SELECT * FROM locations 
WHERE geom <-> POINT(-74.006, 40.713) < 1000;
```

### Best Practices
- Use GiST for complex geometries (curves, 3D)
- RTREE is optimized for bounding box queries

---

## 4. FULLTEXT

Inverted index for text search with relevance ranking.

### Use Cases
- Article/content search
- Document search
- Product catalog search
- Log analysis

### Characteristics
| Property | Value |
|----------|-------|
| Tokenization | Word-based with stemming |
| Relevance ranking | TF-IDF, BM25 |
| Supports highlighting | Yes |
| Supports boolean | AND, OR, NOT |
| Languages | 20+ supported |

### Example
```sql
-- Text search index
CREATE INDEX idx_articles_search ON articles 
    USING FULLTEXT (title, content);

-- Query: Search articles
SELECT * FROM articles 
WHERE title @@ 'database & performance';

-- Query: With relevance ranking
SELECT title, ts_rank_cd(search_vector, query) AS rank
FROM articles, plainto_tsquery('english', 'query optimization') query
WHERE search_vector @@ query
ORDER BY rank DESC;
```

### Configuration
```sql
-- Specify language
CREATE INDEX idx_docs ON docs USING FULLTEXT (content) 
    WITH (language = 'english');

-- Custom stop words
CREATE INDEX idx_docs ON docs USING FULLTEXT (content)
    WITH (stop_words = '{a,an,the,is,are}');
```

---

## 5. BITMAP

Bitmap index optimized for low-cardinality columns.

### Use Cases
- Status fields (active/inactive/pending)
- Category codes
- Boolean flags
- OLAP queries with OR conditions

### Characteristics
| Property | Value |
|----------|-------|
| Best cardinality | < 1000 distinct values |
| Compression | Run-length encoding |
| OR performance | Excellent (bitmap OR) |
| AND performance | Good |
| Update cost | High (not for write-heavy) |

### Example
```sql
-- Low cardinality status
CREATE INDEX idx_orders_status ON orders USING BITMAP (status);

-- Boolean flag
CREATE INDEX idx_users_verified ON users USING BITMAP (is_verified);

-- Category (if few categories)
CREATE INDEX idx_products_category ON products USING BITMAP (category_id);
```

### When NOT to Use
- High cardinality (> 10K distinct values)
- Write-heavy tables
- Primary keys
- Unique constraints

---

## 6. GIN

Generalized Inverted Index for composite values.

### Use Cases
- Array containment (`array_column @> ARRAY['value']`)
- JSONB containment (`jsonb_column @> '{"key": "value"}'`)
- Full-text search (alternative to FULLTEXT)
- Composite type containment

### Characteristics
| Property | Value |
|----------|-------|
| Structure | Inverted list (value → rows) |
| Fastupdate | Deferred index updates |
| Pending list | In-memory buffer for updates |
| Query patterns | `@>`, `&&`, `@@` operators |

### Example
```sql
-- Array containment
CREATE INDEX idx_products_tags ON products USING GIN (tags);
SELECT * FROM products WHERE tags @> ARRAY['electronics'];

-- JSONB queries
CREATE INDEX idx_events_data ON events USING GIN (data);
SELECT * FROM events WHERE data @> '{"type": "error"}';

-- Multi-column GIN
CREATE INDEX idx_docs ON docs USING GIN (title, tags, authors);
```

### Fastupdate Optimization
```sql
-- Enable fastupdate for write-heavy tables
CREATE INDEX idx_logs ON logs USING GIN (data) 
    WITH (fastupdate = on);
```

---

## 7. GIST

Generalized Search Tree for extensible indexing.

### Use Cases
- Range types (int4range, tsrange, etc.)
- Complex spatial types (PostGIS-style)
- Custom operator classes
- Nearest neighbor (K-NN) for custom types

### Characteristics
| Property | Value |
|----------|-------|
| Extensibility | Custom operator classes |
| Picksplit algorithm | Balanced tree building |
| Penalty function | Insertion optimization |
| Consistent function | Search predicate matching |

### Example
```sql
-- Range type index
CREATE INDEX idx_reservations_period ON reservations 
    USING GIST (period);

-- Query: Overlapping reservations
SELECT * FROM reservations 
WHERE period && '[2024-01-01, 2024-01-31]';

-- Query: Contains point
SELECT * FROM reservations 
WHERE period @> '2024-01-15'::date;
```

### Operator Classes
```sql
-- 2D point K-NN
CREATE INDEX idx_points ON points USING GIST (location);
SELECT * FROM points 
ORDER BY location <-> POINT(0, 0) 
LIMIT 10;
```

---

## 8. VECTOR

Approximate Nearest Neighbor (ANN) index for vector embeddings.

### Use Cases
- Semantic search (text embeddings)
- Image similarity
- Recommendation systems
- AI/ML feature similarity

### Characteristics
| Property | Value |
|----------|-------|
| Algorithms | HNSW, IVF_FLAT, IVF_PQ |
| Distance metrics | L2, Cosine, Inner Product |
| Approximate | May miss true nearest neighbors |
| Build time | Varies by algorithm and size |

### Algorithms

| Algorithm | Build Time | Query Speed | Memory | Best For |
|-----------|------------|-------------|--------|----------|
| HNSW | Slow | Very Fast | High | General purpose |
| IVF_FLAT | Medium | Fast | Medium | Large datasets |
| IVF_PQ | Fast | Medium | Low | Memory constrained |

### Example (HNSW)
```sql
-- High-dimensional embeddings
CREATE INDEX idx_items_embedding ON items 
    USING VECTOR (embedding vector_cosine_ops)
    WITH (
        algorithm = 'hnsw',
        dimensions = 768,
        m = 16,
        ef_construction = 128,
        ef_search = 64
    );

-- Similarity search
SELECT id, name, 
       embedding <=> query_embedding AS distance
FROM items
ORDER BY embedding <=> query_embedding
LIMIT 10;
```

### Example (IVF)
```sql
-- Large-scale dataset
CREATE INDEX idx_docs_embedding ON documents 
    USING VECTOR (embedding vector_l2_ops)
    WITH (
        algorithm = 'ivf_flat',
        dimensions = 1536,
        nlist = 1000
    );
```

### Tuning Parameters

| Parameter | Description | Default |
|-----------|-------------|---------|
| `m` | Connections per layer (HNSW) | 16 |
| `ef_construction` | Search depth at build | 64 |
| `ef_search` | Search depth at query | 64 |
| `nlist` | Number of clusters (IVF) | 100 |

---

## 9. LSM

Log-Structured Merge Tree for write-optimized workloads.

### Use Cases
- Time-series data
- Event logs
- IoT sensor data
- Write-heavy OLTP

### Characteristics
| Property | Value |
|----------|-------|
| Write pattern | Sequential append |
| Compaction | Background merge |
| Levels | L0 (memtable) → L1 → L2 → ... |
| Bloom filters | Reduce disk reads |
| Read amplification | Higher than BTREE |
| Write amplification | Lower than BTREE |

### Example
```sql
-- Time-series events
CREATE INDEX idx_events_timestamp ON events 
    USING LSM (timestamp)
    WITH (write_buffer_size = '128MB');

-- Sensor readings
CREATE INDEX idx_sensors_reading ON sensor_data 
    USING LSM (sensor_id, recorded_at);
```

### When to Use
- Append-mostly workloads
- Time-series with recent queries
- High write throughput requirements

### When NOT to Use
- Random read patterns
- Range scans across large time windows
- Small datasets

---

## 10. COLUMNAR

Column-oriented index for analytical workloads.

### Use Cases
- OLAP aggregations (SUM, AVG, COUNT)
- Columnar compression
- Star schema fact tables
- Read-heavy analytics

### Characteristics
| Property | Value |
|----------|-------|
| Storage | Column-major layout |
| Compression | Per-column (zstd, lz4) |
| Vectorization | SIMD-optimized scans |
| Predicate pushdown | Min/max skipping |
| Update cost | Very high (rewrite stripes) |

### Example
```sql
-- Analytics index on fact table
CREATE INDEX idx_sales_columnar ON sales 
    USING COLUMNAR (product_id, region, amount, sale_date)
    WITH (
        compression = 'zstd',
        stripe_row_count = 50000,
        block_size = '256KB'
    );

-- Fast aggregation
SELECT region, SUM(amount) 
FROM sales 
WHERE sale_date >= '2024-01-01'
GROUP BY region;
```

### When NOT to Use
- OLTP workloads
- Frequent updates
- Point lookups (by primary key)

---

## 11. COMPOUND

Multi-column or index intersection optimization.

### Use Cases
- Complex AND/OR filter combinations
- Index intersection (bitmap AND)
- Covering multiple query patterns

### Characteristics
| Property | Value |
|----------|-------|
| Structure | Virtual (intersection) or physical (multi-column) |
| Bitmap ops | AND, OR, NOT between indexes |
| Cost-based | Optimizer decides intersection |

### Example
```sql
-- Compound index for multiple filters
CREATE INDEX idx_orders_compound ON orders 
    USING COMPOUND (status, created_at, user_id);

-- Optimizer may use index intersection:
-- idx_orders_status (bitmap) AND idx_orders_created_at (bitmap)
```

---

## Comparison Summary

| Type | Equality | Range | Insert | Size | Best For |
|------|----------|-------|--------|------|----------|
| BTREE | ★★★ | ★★★ | ★★ | Medium | General purpose |
| HASH | ★★★ | ✗ | ★★★ | Small | Exact match |
| RTREE | ✗ | ✗ | ★★ | Medium | 2D spatial |
| FULLTEXT | ★ | ✗ | ★ | Large | Text search |
| BITMAP | ★★ | ✗ | ★ | Small | Low cardinality |
| GIN | ★★ | ✗ | ★ | Large | Arrays, JSONB |
| GIST | ★★ | ★★ | ★★ | Medium | Ranges, custom |
| VECTOR | ★★ | ✗ | ★ | Large | Embeddings |
| LSM | ★★ | ★ | ★★★ | Medium | Time-series |
| COLUMNAR | ★ | ★ | ✗ | Small | Analytics |
| COMPOUND | ★★ | ★★ | ★ | Variable | Complex filters |

Legend: ★★★ Excellent, ★★ Good, ★ Fair, ✗ Not supported

## See Also

- [CREATE INDEX](../../syntax_guide/ddl/table_and_constraints/04_create_index.md)
- [Index Observability](../../../metrics_guide/index_observability/README.md)
- [Performance Tuning](../../../performance_and_capacity_guide/index_and_data_layout_tuning/README.md)
