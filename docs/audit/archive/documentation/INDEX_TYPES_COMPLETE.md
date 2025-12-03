# Index Types - Complete Specification

**Last Updated:** November 23, 2025
**Status:** Alpha 1 - 11/11 Index Types Production-Ready (100%)
**Purpose:** Complete specification of all index types

---

## Overview

ScratchBird implements 11 production-ready index types, all MGA-compliant with stable TID support. All indexes support expression indexes and partial indexes.

**Implementation Status:** ✅ 100% Complete (11/11)

**File Location:** `/home/user/ScratchBird/include/scratchbird/core/catalog_manager.h:399-414`

---

## Index Type Enum

```cpp
enum class IndexType : uint8_t {
    BTREE = 0,        // B-tree index
    HASH = 1,         // Hash index
    HNSW = 2,         // Vector similarity (alias: VECTOR)
    FULLTEXT = 3,     // Full-text search (GIN-based)
    GIN = 4,          // Generalized Inverted Index
    GIST = 5,         // Generalized Search Tree
    BRIN = 6,         // Block Range Index
    RTREE = 7,        // R-tree spatial index
    SPGIST = 8,       // Space-Partitioned GiST
    BITMAP = 9,       // Bitmap index
    COLUMNSTORE = 10, // Columnstore index
    LSM = 11          // LSM-Tree
};
```

---

## 1. B-TREE Index (Default)

**Type:** `BTREE` (0)
**Status:** ✅ Production-Ready
**Implementation:** `/home/user/ScratchBird/src/core/btree.cpp` (117KB)

**Purpose:** General-purpose balanced tree index for ordered data.

**Best For:**
- Equality searches (`WHERE id = 42`)
- Range queries (`WHERE date BETWEEN ... AND ...`)
- Sorted scans (`ORDER BY column`)
- Prefix matching (`WHERE name LIKE 'prefix%'`)

**Syntax:**

```sql
CREATE INDEX idx_name ON table_name (column_name);
CREATE INDEX idx_name ON table_name USING BTREE (column_name);
```

**Examples:**

```sql
-- Single column
CREATE INDEX idx_users_email ON users(email);

-- Multi-column
CREATE INDEX idx_orders_customer_date ON orders(customer_id, order_date);

-- Unique B-tree
CREATE UNIQUE INDEX idx_products_sku ON products(sku);

-- Expression index
CREATE INDEX idx_users_lower_username ON users(LOWER(username));

-- Partial index
CREATE INDEX idx_active_users ON users(email) WHERE active = TRUE;
```

**Features:**
- ✅ Unique indexes
- ✅ Multi-column indexes (up to 16 columns)
- ✅ Expression indexes
- ✅ Partial indexes (WHERE clause)
- ✅ Stable TIDs (MGA-compliant)
- ✅ Concurrent access

**Complexity:**
- Search: O(log n)
- Insert: O(log n)
- Delete: O(log n)

---

## 2. HASH Index

**Type:** `HASH` (1)
**Status:** ✅ Production-Ready
**Implementation:** `/home/user/ScratchBird/src/core/hash_index.cpp`

**Purpose:** Hash-based index optimized for equality searches.

**Best For:**
- Exact match queries (`WHERE id = 42`)
- High-cardinality columns
- Fast point lookups

**Not Suitable For:**
- Range queries
- Sorting
- Pattern matching

**Syntax:**

```sql
CREATE INDEX idx_name ON table_name USING HASH (column_name);
```

**Examples:**

```sql
-- Single column hash index
CREATE INDEX idx_users_uuid ON users USING HASH (user_id);

-- Hash index for lookups
CREATE INDEX idx_sessions_token ON sessions USING HASH (session_token);
```

**Features:**
- ✅ Fast equality searches
- ✅ Constant-time lookups (O(1) average)
- ✅ Stable TIDs (MGA-compliant)
- ❌ No range queries
- ❌ No ordering support

**Complexity:**
- Search (equality): O(1) average, O(n) worst case
- Insert: O(1) average
- Delete: O(1) average

---

## 3. HNSW / VECTOR Index

**Type:** `HNSW` (2)
**Alias:** `VECTOR`
**Status:** ✅ Production-Ready
**Implementation:** `/home/user/ScratchBird/src/core/hnsw_index.cpp`

**Purpose:** Hierarchical Navigable Small World graph for vector similarity search.

**Best For:**
- Embedding similarity search
- Nearest neighbor queries
- Semantic search
- Image/audio similarity

**Syntax:**

```sql
CREATE INDEX idx_name ON table_name USING HNSW (vector_column);
CREATE INDEX idx_name ON table_name USING VECTOR (vector_column);
```

**Examples:**

```sql
-- Vector embeddings index
CREATE INDEX idx_embeddings_vector ON documents USING HNSW (embedding);

-- Configure HNSW parameters
CREATE INDEX idx_images_features ON images USING HNSW (feature_vector)
WITH (m = 16, ef_construction = 200);

-- Vector similarity query
SELECT * FROM documents
ORDER BY embedding <-> '[0.1, 0.2, 0.3, ...]'::VECTOR
LIMIT 10;
```

**Features:**
- ✅ k-NN (k-Nearest Neighbors) search
- ✅ ANN (Approximate Nearest Neighbors)
- ✅ Multiple distance metrics (L2, cosine, dot product)
- ✅ Configurable accuracy/speed tradeoff

**Parameters:**
- `m` - Max connections per layer (default: 16)
- `ef_construction` - Build-time search depth (default: 200)
- `ef_search` - Query-time search depth (default: 100)

**Complexity:**
- Search (k-NN): O(log n) average
- Insert: O(log n)

---

## 4. FULLTEXT Index (GIN-based)

**Type:** `FULLTEXT` (3)
**Status:** ✅ Production-Ready
**Implementation:** `/home/user/ScratchBird/src/core/fulltext_index.cpp`

**Purpose:** Full-text search with linguistic features (stemming, stopwords).

**Best For:**
- Text search with relevance ranking
- Natural language queries
- Document search

**Syntax:**

```sql
CREATE INDEX idx_name ON table_name USING FULLTEXT (text_column);
```

**Examples:**

```sql
-- Full-text index on text column
CREATE INDEX idx_articles_body ON articles USING FULLTEXT (body_text);

-- Full-text search
SELECT * FROM articles
WHERE to_tsvector('english', body_text) @@ to_tsquery('english', 'database & performance');

-- With ranking
SELECT title, ts_rank(to_tsvector(body_text), to_tsquery('search terms')) as rank
FROM articles
WHERE to_tsvector(body_text) @@ to_tsquery('search terms')
ORDER BY rank DESC;
```

**Features:**
- ✅ Stemming and lemmatization
- ✅ Stopword filtering
- ✅ Relevance ranking (ts_rank)
- ✅ Phrase search
- ✅ Boolean operators (AND, OR, NOT)

---

## 5. GIN Index (Generalized Inverted Index)

**Type:** `GIN` (4)
**Status:** ✅ Production-Ready
**Implementation:** `/home/user/ScratchBird/src/core/gin_index.cpp` (176KB)

**Purpose:** Inverted index for composite values (arrays, JSONB, full-text).

**Best For:**
- Array containment queries
- JSONB key/value searches
- Full-text search (tsvector)
- Multi-value columns

**Syntax:**

```sql
CREATE INDEX idx_name ON table_name USING GIN (column_name);
```

**Examples:**

```sql
-- Array containment
CREATE INDEX idx_tags ON articles USING GIN (tags);
SELECT * FROM articles WHERE tags @> ARRAY['database', 'sql'];

-- JSONB indexing
CREATE INDEX idx_metadata ON products USING GIN (metadata_jsonb);
SELECT * FROM products WHERE metadata_jsonb @> '{"color": "red"}';

-- Full-text search
CREATE INDEX idx_document_vector ON documents USING GIN (to_tsvector('english', content));
SELECT * FROM documents WHERE to_tsvector('english', content) @@ to_tsquery('search');
```

**Features:**
- ✅ Array operators (@>, <@, &&)
- ✅ JSONB operators (@>, ?, ?&, ?|)
- ✅ Text search operators (@@)
- ✅ Pending list for fast inserts
- ✅ Posting tree compression

**Complexity:**
- Search: O(log n) + O(k) where k is result size
- Insert: O(log n) for each element

---

## 6. GiST Index (Generalized Search Tree)

**Type:** `GIST` (5)
**Status:** ✅ Production-Ready
**Implementation:** `/home/user/ScratchBird/src/core/gist_index.cpp`

**Purpose:** Balanced tree for custom types with overlapping keys.

**Best For:**
- Geometric data (overlaps, contains)
- Range types
- Network address types (INET, CIDR)
- Custom types with complex predicates

**Syntax:**

```sql
CREATE INDEX idx_name ON table_name USING GIST (column_name);
```

**Examples:**

```sql
-- Geometric searches
CREATE INDEX idx_locations ON venues USING GIST (location);
SELECT * FROM venues WHERE location && ST_MakeEnvelope(...);

-- Range types
CREATE INDEX idx_reservations ON bookings USING GIST (time_range);
SELECT * FROM bookings WHERE time_range && '[2025-01-01, 2025-01-07]'::tsrange;

-- IP address searches
CREATE INDEX idx_ip_ranges ON network_blocks USING GIST (ip_range);
SELECT * FROM network_blocks WHERE ip_range >> inet '192.168.1.100';
```

**Features:**
- ✅ Spatial predicates (overlaps, contains, within)
- ✅ Range operators (overlaps, contains, adjacent)
- ✅ Lossy compression
- ✅ Extensible for custom types

---

## 7. BRIN Index (Block Range Index)

**Type:** `BRIN` (6)
**Status:** ✅ Production-Ready
**Implementation:** `/home/user/ScratchBird/src/core/brin_index.cpp`

**Purpose:** Compact index storing min/max summaries per block range.

**Best For:**
- Large tables with naturally ordered data
- Time-series data
- Log tables
- Append-only workloads

**Syntax:**

```sql
CREATE INDEX idx_name ON table_name USING BRIN (column_name);
```

**Examples:**

```sql
-- Time-series index
CREATE INDEX idx_logs_timestamp ON logs USING BRIN (created_at);
SELECT * FROM logs WHERE created_at BETWEEN '2025-01-01' AND '2025-01-07';

-- Sequential data
CREATE INDEX idx_orders_id ON orders USING BRIN (order_id);

-- Configure pages per range
CREATE INDEX idx_events_time ON events USING BRIN (event_time)
WITH (pages_per_range = 128);
```

**Features:**
- ✅ Tiny index size (100-1000x smaller than B-tree)
- ✅ Fast index creation
- ✅ Efficient for sequential scans
- ✅ Min/max/inclusion/bloom summaries

**Best Practices:**
- Use on large tables (millions of rows)
- Best with naturally ordered data (timestamps, auto-increment IDs)
- Tune `pages_per_range` (default: 128)

**Complexity:**
- Search: O(k) where k is number of ranges
- Insert: O(1)
- Space: O(n/pages_per_range)

---

## 8. R-TREE Index

**Type:** `RTREE` (7)
**Status:** ✅ Production-Ready
**Implementation:** `/home/user/ScratchBird/src/core/rtree_index.cpp`

**Purpose:** Spatial index for 2D geometric data.

**Best For:**
- Point-in-polygon queries
- Bounding box intersections
- Nearest neighbor searches (spatial)
- GIS applications

**Syntax:**

```sql
CREATE INDEX idx_name ON table_name USING RTREE (geometry_column);
```

**Examples:**

```sql
-- Spatial index
CREATE INDEX idx_buildings_geom ON buildings USING RTREE (geometry);

-- Find buildings in area
SELECT * FROM buildings
WHERE ST_Intersects(geometry, ST_MakeEnvelope(xmin, ymin, xmax, ymax, 4326));

-- Nearest neighbors
SELECT * FROM restaurants
ORDER BY geometry <-> ST_Point(lon, lat)
LIMIT 10;
```

**Features:**
- ✅ 2D bounding boxes
- ✅ Overlap queries
- ✅ Contains/within predicates
- ✅ k-NN searches
- ✅ Configurable max entries per node

**Parameters:**
- `rtree_max_entries` - Max entries per node (default: 50)

---

## 9. SP-GiST Index (Space-Partitioned GiST)

**Type:** `SPGIST` (8)
**Status:** ✅ Production-Ready
**Implementation:** `/home/user/ScratchBird/src/core/spgist_index.cpp`

**Purpose:** Space-partitioning index for non-balanced trees.

**Best For:**
- Quadtrees (2D points)
- k-d trees (multi-dimensional)
- Radix trees (text prefix search)
- IP address hierarchies

**Syntax:**

```sql
CREATE INDEX idx_name ON table_name USING SPGIST (column_name);
```

**Examples:**

```sql
-- Point index (quadtree)
CREATE INDEX idx_locations_point ON locations USING SPGIST (coordinates);

-- IP address hierarchy
CREATE INDEX idx_ip_addresses ON devices USING SPGIST (ip_address);

-- Text prefix search (radix tree)
CREATE INDEX idx_words_prefix ON dictionary USING SPGIST (word);
SELECT * FROM dictionary WHERE word LIKE 'data%';
```

**Features:**
- ✅ Quadtree partitioning for points
- ✅ Radix tree for strings
- ✅ k-d tree for multi-dimensional data
- ✅ Unbalanced tree (matches data distribution)

---

## 10. BITMAP Index

**Type:** `BITMAP` (9)
**Status:** ✅ Production-Ready
**Implementation:** `/home/user/ScratchBird/src/core/bitmap_index.cpp` (89KB)

**Purpose:** Compressed bitmap index using Roaring bitmaps for low-cardinality columns.

**Best For:**
- Boolean flags
- Status/category columns (few distinct values)
- Gender, country, department columns
- Data warehouse queries with multiple filters

**Syntax:**

```sql
CREATE INDEX idx_name ON table_name USING BITMAP (column_name);
```

**Examples:**

```sql
-- Boolean column
CREATE INDEX idx_users_active ON users USING BITMAP (active);

-- Status column (low cardinality)
CREATE INDEX idx_orders_status ON orders USING BITMAP (status);

-- Efficient multi-filter queries
SELECT * FROM orders
WHERE status = 'PENDING'
  AND priority = 'HIGH'
  AND region = 'US';
-- Combines 3 bitmap indexes efficiently
```

**Features:**
- ✅ Roaring bitmap compression
- ✅ Extremely fast AND/OR/NOT operations
- ✅ Tiny space for low-cardinality columns
- ✅ Bitmap dictionary encoding

**Best Practices:**
- Use for columns with < 1000 distinct values
- Ideal for boolean/enum columns
- Excellent for data warehouse star schema

**Complexity:**
- Search: O(log n) + O(k) for k results
- Bitmap operations: Near O(1) with compression
- Space: Much smaller than B-tree for low cardinality

---

## 11. COLUMNSTORE Index

**Type:** `COLUMNSTORE` (10)
**Status:** ✅ Production-Ready
**Implementation:** `/home/user/ScratchBird/src/core/columnstore_index.cpp`

**Purpose:** Columnar storage for analytical queries (OLAP).

**Best For:**
- Analytical queries (aggregations, scans)
- Data warehouse fact tables
- Read-mostly workloads
- Compression-friendly data

**Syntax:**

```sql
CREATE INDEX idx_name ON table_name USING COLUMNSTORE (column1, column2, ...);
```

**Examples:**

```sql
-- Columnstore for analytics
CREATE INDEX idx_sales_analytics ON sales USING COLUMNSTORE (
    product_id, sale_date, quantity, amount, region
);

-- Analytical query (scans only needed columns)
SELECT region, SUM(amount) as total_sales
FROM sales
WHERE sale_date BETWEEN '2025-01-01' AND '2025-12-31'
GROUP BY region;
-- Only scans region, amount, sale_date columns
```

**Features:**
- ✅ Column-oriented storage
- ✅ High compression ratios
- ✅ Vectorized execution
- ✅ Zone maps (min/max per segment)
- ✅ Dictionary encoding
- ✅ Run-length encoding (RLE)

**Best Practices:**
- Use for large fact tables (millions of rows)
- Best for read-heavy analytics
- Not ideal for transactional workloads (row updates)
- Combine with BRIN for time-series data

---

## 12. LSM-TREE Index

**Type:** `LSM` (11)
**Status:** ✅ Production-Ready
**Implementation:** `/home/user/ScratchBird/src/core/lsm_tree_index.cpp`

**Purpose:** Log-Structured Merge-Tree optimized for write-heavy workloads.

**Best For:**
- High write throughput
- Time-series data
- Log ingestion
- Append-heavy workloads

**Syntax:**

```sql
CREATE INDEX idx_name ON table_name USING LSM (column_name);
```

**Examples:**

```sql
-- High-write time-series
CREATE INDEX idx_metrics_timestamp ON metrics USING LSM (timestamp);

-- Log ingestion
CREATE INDEX idx_logs_event_id ON application_logs USING LSM (event_id);
```

**Features:**
- ✅ Write-optimized (sequential writes to memtable)
- ✅ Background compaction
- ✅ Bloom filters for fast negative lookups
- ✅ Multiple levels (L0, L1, L2, ...)
- ✅ Configurable compaction strategy

**Architecture:**
- Memtable (in-memory sorted tree)
- Immutable SSTables on disk
- Background compaction merges levels

**Best Practices:**
- Use for write-heavy workloads (logs, metrics, events)
- Tune compaction thresholds
- Monitor LSM depth (too deep = slow reads)

**Complexity:**
- Write: O(log n) in memtable, O(1) amortized
- Read: O(k × log n) where k is number of levels

---

## Common Index Features (All Types)

### Expression Indexes

Create indexes on computed expressions.

```sql
-- Index on lowercase username
CREATE INDEX idx_users_lower_username ON users(LOWER(username));

-- Index on year extracted from timestamp
CREATE INDEX idx_orders_year ON orders(EXTRACT(YEAR FROM order_date));

-- Index on concatenated columns
CREATE INDEX idx_full_name ON employees((first_name || ' ' || last_name));
```

**Status:** ✅ Supported on BTREE, HASH, GIN, GIST, SPGIST

---

### Partial Indexes

Index only rows matching a WHERE clause.

```sql
-- Index only active users
CREATE INDEX idx_active_users ON users(email) WHERE active = TRUE;

-- Index only recent orders
CREATE INDEX idx_recent_orders ON orders(order_date)
WHERE order_date > CURRENT_DATE - INTERVAL '30 days';

-- Index only non-null values
CREATE INDEX idx_optional_field ON data(optional_column)
WHERE optional_column IS NOT NULL;
```

**Benefits:**
- Smaller index size
- Faster index updates
- Targeted for specific queries

**Status:** ✅ Supported on BTREE, HASH, GIN, GIST, BITMAP

---

### Unique Indexes

Enforce uniqueness constraint.

```sql
-- Unique index
CREATE UNIQUE INDEX idx_users_email ON users(email);

-- Composite unique index
CREATE UNIQUE INDEX idx_enrollment ON enrollments(student_id, course_id);

-- Unique partial index (NULL values allowed)
CREATE UNIQUE INDEX idx_username ON users(username) WHERE username IS NOT NULL;
```

**Status:** ✅ Supported on BTREE, HASH

---

### Multi-Column Indexes

Index multiple columns (up to 16).

```sql
-- Two columns
CREATE INDEX idx_orders_customer_date ON orders(customer_id, order_date);

-- Three columns
CREATE INDEX idx_employees ON employees(department_id, job_title, hire_date);
```

**Column Order Matters:**
- Index can support queries on leftmost columns
- `(a, b, c)` supports queries on `a`, `(a,b)`, `(a,b,c)`
- Does NOT support queries on `b` or `c` alone

**Status:** ✅ Supported on BTREE, GIN, GIST, BITMAP, COLUMNSTORE

---

## MGA Compliance (All Indexes)

All indexes follow Firebird MGA rules:

1. **Stable TIDs**
   - Index entries never change unless indexed column modified
   - TID remains same across updates
   - No index bloat from non-indexed column updates

2. **Version Visibility**
   - Use `isVersionVisible(xmin, current_xid)` not snapshots
   - Check TIP (Transaction Inventory Pages) for transaction state

3. **In-Place Updates**
   - Primary record modified at original location
   - Back-versions chain from primary
   - Indexes always point to stable primary record

**Implementation Files:**
- MGA Rules: `/home/user/ScratchBird/MGA_RULES.md`
- Index Architecture: `/home/user/ScratchBird/docs/specifications/INDEX_ARCHITECTURE.md`

---

## Index Selection Guidelines

| Use Case | Recommended Index Type |
|----------|----------------------|
| General purpose (equality, range, order) | BTREE |
| Equality lookups only | HASH |
| Vector similarity search | HNSW/VECTOR |
| Full-text search | FULLTEXT or GIN |
| Array/JSONB containment | GIN |
| Geometric queries | RTREE or GIST |
| Large time-series tables | BRIN + COLUMNSTORE |
| Low-cardinality columns | BITMAP |
| High write throughput | LSM |
| Point queries (2D) | SPGIST (quadtree) |
| Prefix search | SPGIST (radix tree) |
| Analytical queries (OLAP) | COLUMNSTORE |

---

## DDL Operations

### CREATE INDEX

```sql
CREATE [ UNIQUE ] INDEX [ IF NOT EXISTS ] index_name
ON table_name
USING index_type
( column_name | ( expression ) [ , ... ] )
[ WHERE predicate ]
[ WITH ( parameter = value [ , ... ] ) ];
```

**Status:** ✅ 100% Complete (Phase 2.2.3)

---

### DROP INDEX

```sql
DROP INDEX [ IF EXISTS ] index_name [ CASCADE | RESTRICT ];
```

**Status:** ✅ 100% Complete (Alpha 1 - DDL Modifications)

---

### ALTER INDEX

```sql
ALTER INDEX index_name RENAME TO new_name;
ALTER INDEX index_name SET TABLESPACE tablespace_name;
```

**Status:** ✅ 100% Complete

---

## Summary

| Index Type | Status | Best For | Complexity |
|-----------|--------|----------|-----------|
| BTREE | ✅ Production | General purpose, range queries | O(log n) |
| HASH | ✅ Production | Equality lookups | O(1) avg |
| HNSW | ✅ Production | Vector similarity | O(log n) |
| FULLTEXT | ✅ Production | Text search | O(log n) |
| GIN | ✅ Production | Arrays, JSONB, text search | O(log n) |
| GIST | ✅ Production | Geometry, ranges, custom types | O(log n) |
| BRIN | ✅ Production | Large ordered tables | O(k) ranges |
| RTREE | ✅ Production | 2D spatial queries | O(log n) |
| SPGIST | ✅ Production | Quadtrees, radix trees | O(log n) |
| BITMAP | ✅ Production | Low-cardinality columns | Near O(1) |
| COLUMNSTORE | ✅ Production | Analytics (OLAP) | Column scan |
| LSM | ✅ Production | Write-heavy workloads | O(1) write |

**Overall:** ✅ 11/11 Index Types Complete (100%)

**Common Features:**
- ✅ Expression indexes
- ✅ Partial indexes
- ✅ Multi-column indexes (up to 16)
- ✅ Unique indexes (BTREE, HASH)
- ✅ MGA compliance (stable TIDs)
- ✅ Concurrent access
