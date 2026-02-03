# Indexes

**Last Updated:** 2026-02-03


Create an index for faster queries.


---

## Syntax

```sql
CREATE [ UNIQUE ] INDEX [ CONCURRENTLY ] [ IF NOT EXISTS ] index_name
    ON table_name [ USING method ]
    ( column_name [ ASC | DESC ] [ NULLS { FIRST | LAST } ], ... )
    [ INCLUDE ( column_name, ... ) ]
    [ WHERE predicate ];
```

---

## Index Types

ScratchBird supports 11 index types:

| Type | Use Case |
|------|----------|
| `BTREE` | General purpose (default) |
| `HASH` | Equality comparisons |
| `GIN` | Full-text search, arrays, JSON |
| `GIST` | Spatial data, ranges |
| `BRIN` | Large tables with natural ordering |
| `SP-GIST` | Non-balanced structures |
| `BLOOM` | Multi-column equality |
| `RTREE` | Legacy spatial |
| `BITMAP` | Low-cardinality columns |
| `PARTIAL` | Filtered indexes |
| `COVERING` | Index-only scans |

---

## Examples

### Basic Index

```sql
CREATE INDEX idx_users_email ON users(email);
```

### Unique Index

```sql
CREATE UNIQUE INDEX idx_users_email_unique ON users(email);
```

### Composite Index

```sql
CREATE INDEX idx_orders_user_date ON orders(user_id, created_at);
```

### Specify Type

```sql
-- Hash for equality only
CREATE INDEX idx_users_email_hash ON users USING HASH (email);

-- GIN for full-text
CREATE INDEX idx_posts_content ON posts USING GIN (to_tsvector('english', content));

-- GIN for JSON
CREATE INDEX idx_data_json ON documents USING GIN (data jsonb_path_ops);

-- BRIN for time-series
CREATE INDEX idx_logs_created ON logs USING BRIN (created_at);
```

### Partial Index

Index only matching rows:

```sql
-- Only active users
CREATE INDEX idx_users_email_active ON users(email)
WHERE active = TRUE;

-- Only pending orders
CREATE INDEX idx_orders_pending ON orders(created_at)
WHERE status = 'pending';
```

### Covering Index

Include extra columns for index-only scans:

```sql
CREATE INDEX idx_orders_customer ON orders(customer_id)
INCLUDE (total, status);
```

### Expression Index

Index computed values:

```sql
-- Lowercase email
CREATE INDEX idx_users_email_lower ON users(LOWER(email));

-- Date part
CREATE INDEX idx_orders_month ON orders(DATE_TRUNC('month', created_at));

-- JSON field
CREATE INDEX idx_docs_title ON documents((data->>'title'));
```

### Descending Order

```sql
CREATE INDEX idx_orders_recent ON orders(created_at DESC);

CREATE INDEX idx_scores_rank ON scores(score DESC NULLS LAST);
```

### Concurrent Creation

Build without blocking writes:

```sql
CREATE INDEX CONCURRENTLY idx_large_table ON large_table(column);
```

---

## When to Use Each Type

### BTREE (Default)

Best for:
- Equality (`=`)
- Range queries (`<`, `>`, `BETWEEN`)
- Sorting (`ORDER BY`)
- Pattern matching (`LIKE 'prefix%'`)

```sql
CREATE INDEX idx_price ON products(price);
-- Supports: price = 100, price > 50, price BETWEEN 10 AND 50
```

### HASH

Best for:
- Equality only (`=`)
- Slightly faster than BTREE for pure equality

```sql
CREATE INDEX idx_code USING HASH ON products(product_code);
-- Supports: product_code = 'ABC123'
-- Does NOT support: product_code LIKE 'ABC%'
```

### GIN

Best for:
- Full-text search
- Array containment (`@>`, `<@`)
- JSON containment

```sql
-- Full-text
CREATE INDEX idx_search ON articles USING GIN (to_tsvector('english', body));

-- Array
CREATE INDEX idx_tags ON posts USING GIN (tags);
-- Supports: tags @> ARRAY['tech']

-- JSONB
CREATE INDEX idx_data ON events USING GIN (data);
-- Supports: data @> '{"type": "click"}'
```

### GIST

Best for:
- Spatial queries
- Range types
- Complex overlapping

```sql
-- Geometry (with GEOS)
CREATE INDEX idx_location ON places USING GIST (location);

-- Range types
CREATE INDEX idx_period ON events USING GIST (validity_period);
```

### BRIN

Best for:
- Very large tables
- Naturally ordered data (timestamps, auto-increment)
- Small index size

```sql
CREATE INDEX idx_logs_time ON logs USING BRIN (created_at);
-- Table should be physically ordered by created_at
```

---

## Index Management

### List Indexes

```sql
-- sb_isql / psql
\di

-- SQL
SELECT indexname, indexdef
FROM pg_indexes
WHERE tablename = 'users';
```

### Drop Index

```sql
DROP INDEX idx_users_email;

DROP INDEX IF EXISTS idx_users_email;

-- Concurrent drop
DROP INDEX CONCURRENTLY idx_large_index;
```

### Reindex

```sql
-- Single index
REINDEX INDEX idx_users_email;

-- All indexes on table
REINDEX TABLE users;

-- All indexes in database
REINDEX DATABASE mydb;
```

---

## Index Statistics

```sql
-- Index usage
SELECT
    indexrelname,
    idx_scan,
    idx_tup_read,
    idx_tup_fetch
FROM pg_stat_user_indexes
WHERE relname = 'users';

-- Unused indexes
SELECT indexrelname, idx_scan
FROM pg_stat_user_indexes
WHERE idx_scan = 0;
```

---

## Index Design Tips

1. **Index columns used in WHERE**
2. **Index foreign key columns**
3. **Order composite indexes by selectivity**
4. **Use partial indexes for filtered queries**
5. **Don't over-index** - indexes slow writes
6. **Monitor usage** - drop unused indexes

---

## Notes

- Indexes are automatically updated on INSERT/UPDATE/DELETE
- Too many indexes slow down write operations
- UNIQUE indexes also enforce uniqueness constraint
- NULL values are indexed (except in HASH indexes)

---

## See Also

- [Performance Tuning](../../admin/performance-tuning.md)
- [EXPLAIN](../dml/select.md#explain)
