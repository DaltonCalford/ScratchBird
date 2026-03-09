<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# CREATE INDEX

[Prev](./03_drop_table.md) | [Next](./05_alter_index.md) | [Topic README](./README.md) | [DDL README](../README.md) | [Syntax Guide README](../../README.md) | [Language Reference README](../../../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1

## Synopsis

Creates an index on one or more columns of a table to speed up data retrieval.

## Syntax

```sql
CREATE [ UNIQUE ] INDEX [ CONCURRENTLY ] [ [ IF NOT EXISTS ] index_name ]
    ON [ ONLY ] table_name [ USING method ]
    ( { column_name | ( expression ) } [ opclass ] [ ASC | DESC ] [ NULLS { FIRST | LAST } ] [, ...] )
    [ INCLUDE ( column_name [, ...] ) ]
    [ NULLS [ NOT ] DISTINCT ]
    [ WITH ( storage_parameter [= value] [, ...] ) ]
    [ TABLESPACE tablespace_name ]
    [ WHERE predicate ]

-- For certain index methods only:
CREATE INDEX index_name ON table_name USING GIST ( column opclass )
    [ WITH ( storage_parameter [= value] [, ...] ) ]

CREATE INDEX index_name ON table_name USING GIN ( column opclass )
    [ WITH ( storage_parameter [= value] [, ...] ) ]
```

## Index Methods (11 Core Types)

| Method | Best For | Multi-Column | Expression | Partial |
|--------|----------|--------------|------------|---------|
| BTREE | Equality, range, sorting | Yes | Yes | Yes |
| HASH | Equality only | No | Yes | Yes |
| RTREE | Spatial data (2D/3D) | No | Yes | Yes |
| FULLTEXT | Text search | No | Yes | Yes |
| BITMAP | Low cardinality, OLAP | Yes | Yes | Yes |
| GIN | Array, JSONB, composite | Yes | Yes | Yes |
| GIST | Range, spatial, custom | No | Yes | Yes |
| VECTOR | Approximate nearest neighbor | No | Yes | Yes |
| LSM | Write-heavy workloads | Yes | Yes | No |
| COLUMNAR | Analytics, compression | Yes | Yes | No |
| COMPOUND | Multi-column, index intersection | Yes | Yes | Yes |

## Parameters

| Parameter | Description |
|-----------|-------------|
| `UNIQUE` | Enforce uniqueness of indexed values |
| `CONCURRENTLY` | Build without locking table (slower, non-blocking) |
| `IF NOT EXISTS` | Skip if index exists (no error) |
| `index_name` | Index name. Supports qualified paths. |
| `ONLY` | Don't recurse to partitioned table partitions |
| `table_name` | Table to index. Supports qualified paths. |
| `USING method` | Index method (BTREE, HASH, GIN, etc.) |
| `column_name` | Table column to index |
| `expression` | Expression to index (functional index) |
| `opclass` | Operator class (data type specific) |
| `ASC` / `DESC` | Sort order (default: ASC) |
| `NULLS FIRST` / `NULLS LAST` | NULL sort position |
| `INCLUDE` | Additional columns in index (covering index) |
| `NULLS NOT DISTINCT` | Treat NULLs as equal (UNIQUE indexes) |
| `WITH` | Method-specific storage parameters |
| `TABLESPACE` | Physical storage location |
| `WHERE` | Partial index predicate |

## Storage Parameters by Method

### BTREE / HASH
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `fillfactor` | integer | 90 | Page fill percentage |
| `vacuum_cleanup_index_scale_factor` | float | 0.1 | Cleanup threshold |

### GIN
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `fastupdate` | boolean | on | Deferred index update |
| `gin_pending_list_limit` | integer | 4MB | Pending list size |

### GIST
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `fillfactor` | integer | 90 | Page fill percentage |
| `buffering` | enum | auto | Build buffering mode |

### VECTOR (Approximate Nearest Neighbor)
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `algorithm` | string | 'hnsw' | 'hnsw', 'ivf_flat', 'ivf_pq' |
| `dimensions` | integer | required | Vector dimensions |
| `m` | integer | 16 | HNSW: connections per layer |
| `ef_construction` | integer | 64 | HNSW: search depth during build |
| `ef_search` | integer | 64 | HNSW: search depth during query |
| `nlist` | integer | 100 | IVF: number of clusters |

### LSM
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `level0_file_num_compaction_trigger` | integer | 4 | L0 compaction trigger |
| `write_buffer_size` | integer | 64MB | Memtable size |

### COLUMNAR
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `compression` | string | 'zstd' | 'none', 'lz4', 'zstd' |
| `stripe_row_count` | integer | 10000 | Rows per stripe |
| `block_size` | integer | 256KB | Compression block size |

## Examples

### Basic Indexes

```sql
-- Single column BTREE (default)
CREATE INDEX idx_users_email ON users(email);

-- Multi-column
CREATE INDEX idx_orders_user_date ON orders(user_id, created_at DESC);

-- Unique index
CREATE UNIQUE INDEX idx_users_email_unique ON users(email);

-- Named with schema path
CREATE INDEX !:prod.mydb.public.idx_metrics_name ON metrics(name);
```

### Index Methods

```sql
-- HASH for equality lookups
CREATE INDEX idx_users_hash ON users USING HASH (email);

-- GIN for JSONB
CREATE INDEX idx_events_data ON events USING GIN (data);

-- GIN for array
CREATE INDEX idx_products_tags ON products USING GIN (tags);

-- RTREE for spatial
CREATE INDEX idx_locations_geom ON locations USING RTREE (geom);

-- FULLTEXT
CREATE INDEX idx_articles_search ON articles USING FULLTEXT (title, content);

-- BITMAP for low cardinality
CREATE INDEX idx_orders_status ON orders USING BITMAP (status);
```

### Advanced Features

```sql
-- Functional index (expression)
CREATE INDEX idx_users_lower_email ON users(LOWER(email));

-- Partial index
CREATE INDEX idx_orders_active ON orders(created_at) 
    WHERE status = 'active';

-- Covering index (INCLUDE)
CREATE INDEX idx_orders_covering ON orders(user_id) 
    INCLUDE (amount, status);

-- Unique with NULL handling
CREATE UNIQUE INDEX idx_products_sku 
    ON products(sku) 
    NULLS NOT DISTINCT;

-- Concurrent build (non-blocking)
CREATE INDEX CONCURRENTLY idx_large_table ON large_table(column);
```

### Vector Index (AI/ML)

```sql
-- HNSW index for vector search
CREATE INDEX idx_items_embedding ON items 
    USING VECTOR (embedding vector_cosine_ops)
    WITH (algorithm = 'hnsw', dimensions = 768, m = 16, ef_construction = 128);

-- IVF index for large datasets
CREATE INDEX idx_docs_embedding ON documents 
    USING VECTOR (embedding vector_l2_ops)
    WITH (algorithm = 'ivf_flat', dimensions = 1536, nlist = 1000);
```

### Columnar (Analytics)

```sql
-- Columnar index for analytical queries
CREATE INDEX idx_sales_columnar ON sales 
    USING COLUMNAR (product_id, region, amount, sale_date)
    WITH (compression = 'zstd', stripe_row_count = 50000);
```

### LSM (Write-Heavy)

```sql
-- LSM index for high write throughput
CREATE INDEX idx_logs_timestamp ON logs 
    USING LSM (timestamp)
    WITH (write_buffer_size = '128MB');
```

## Method Selection Guide

| Use Case | Recommended Method | Example |
|----------|-------------------|---------|
| Primary key, foreign key | BTREE | `PRIMARY KEY (id)` |
| Email lookup | BTREE or HASH | `UNIQUE INDEX (email)` |
| Date ranges | BTREE | `INDEX (created_at)` |
| Spatial queries | RTREE or GIST | `INDEX (location)` |
| Text search | FULLTEXT or GIN | `INDEX (content)` |
| Array contains | GIN | `USING GIN (tags)` |
| JSONB queries | GIN | `USING GIN (data)` |
| Low cardinality filter | BITMAP | `USING BITMAP (status)` |
| Vector similarity | VECTOR | `USING VECTOR (embedding)` |
| Time-series append | LSM | `USING LSM (timestamp)` |
| Analytical aggregation | COLUMNAR | `USING COLUMNAR (...)` |

## MGA Index Behavior

Under ScratchBird's Multi-Generational Architecture:

- **Indexes point to ALL versions** of a row
- No index bloat from updates (versions coexist)
- Index entries removed when all versions are garbage collected
- Unique constraints check against ALL visible versions

## Parser Acceptance Cases

```sql
CREATE INDEX idx1 ON t1(col1);
CREATE UNIQUE INDEX idx1 ON t1(col1);
CREATE INDEX CONCURRENTLY idx1 ON t1(col1);
CREATE INDEX idx1 ON t1 USING HASH (col1);
CREATE INDEX idx1 ON t1(col1, col2 DESC);
CREATE INDEX idx1 ON t1(LOWER(col1));
CREATE INDEX idx1 ON t1(col1) WHERE col1 > 0;
CREATE INDEX idx1 ON t1(col1) INCLUDE (col2);
```

## Parser Rejection Cases

```sql
-- Duplicate column
CREATE INDEX idx1 ON t1(col1, col1);  -- Error: duplicate column

-- Non-existent column
CREATE INDEX idx1 ON t1(nonexistent);  -- Error: column does not exist

-- Non-existent table
CREATE INDEX idx1 ON nonexistent(col1);  -- Error: table does not exist

-- Invalid method
CREATE INDEX idx1 ON t1 USING INVALID (col1);  -- Error: unknown method
```

## Error Conditions

| Error | Cause |
|-------|-------|
| `duplicate_index` | Index exists (no IF NOT EXISTS) |
| `undefined_table` | Table doesn't exist |
| `undefined_column` | Column doesn't exist |
| `invalid_index_method` | Unknown USING method |
| `unique_violation` | Duplicate values for UNIQUE |

## Completion Checklist

- [x] Canonical forms documented
- [x] Clause matrix completed
- [x] Positive and negative parser cases listed
- [x] Examples validated against v3 parser behavior
- [x] Error conditions documented
- [x] Cross-references added

## See Also

- [ALTER INDEX](05_alter_index.md)
- [DROP INDEX](06_drop_index.md)
- [Index Observability](../../metrics_guide/index_observability/README.md)
- [Path Resolution and Scoping](../../03_path_resolution_and_scoping.md)
