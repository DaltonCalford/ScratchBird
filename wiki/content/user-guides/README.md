# User Guides

**Status:** Alpha documentation
**Last Updated:** 2026-01-19

---

## Overview

This section contains feature-focused guides for ScratchBird users. Each guide covers a specific topic in depth, providing practical examples and best practices for day-to-day database work.

**Audience:** Database developers, application developers, and anyone working with ScratchBird databases.

---

## Guide Categories

### Data Operations

| Guide | Description |
|-------|-------------|
| [Transactions](Transactions.md) | ACID transactions, isolation levels, savepoints |
| [Sequences](Sequences.md) | Auto-incrementing values, generators |
| [Triggers](Triggers.md) | Event-driven automation, BEFORE/AFTER triggers |
| [Trigger Cheat Sheet](Trigger-Cheat-Sheet.md) | Quick reference for trigger syntax |
| [Procedures](Procedures.md) | Stored procedures and functions in SBLR |

### Performance & Optimization

| Guide | Description |
|-------|-------------|
| [Indexes](Indexes.md) | Index types, creation, and maintenance |
| [Performance Tuning](Performance-Tuning.md) | Query optimization, configuration, monitoring |
| [Vector Search](Vector-Search.md) | Vector embeddings, HNSW indexes, similarity search |

### Administration

| Guide | Description |
|-------|-------------|
| [Security](Security.md) | Authentication, authorization, TLS |
| [Backup and Restore](Backup-Restore.md) | Data protection, recovery procedures |

---

## Quick Start by Task

### "I need to..."

| Task | Guide | Key Section |
|------|-------|-------------|
| Start a transaction | [Transactions](Transactions.md) | Basic Transactions |
| Create an auto-increment column | [Sequences](Sequences.md) | SERIAL Columns |
| Add an index to speed up queries | [Indexes](Indexes.md) | Creating Indexes |
| Automate data validation | [Triggers](Triggers.md) | BEFORE INSERT Triggers |
| Create a stored procedure | [Procedures](Procedures.md) | Basic Procedures |
| Back up my database | [Backup and Restore](Backup-Restore.md) | Quick Start |
| Improve slow query performance | [Performance Tuning](Performance-Tuning.md) | Query Optimization |
| Set up user permissions | [Security](Security.md) | GRANT/REVOKE |
| Build a semantic search feature | [Vector Search](Vector-Search.md) | Similarity Search |
| Implement RAG for AI apps | [Vector Search](Vector-Search.md) | Use Cases |

---

## Data Operations Guides

### Transactions

**Guide:** [Transactions](Transactions.md)

Manage data consistency with ACID transactions.

```sql
-- Basic transaction
BEGIN;
UPDATE accounts SET balance = balance - 100 WHERE id = 1;
UPDATE accounts SET balance = balance + 100 WHERE id = 2;
COMMIT;

-- With savepoint
BEGIN;
INSERT INTO orders (customer_id, total) VALUES (1, 99.99);
SAVEPOINT before_items;
INSERT INTO order_items (order_id, product_id) VALUES (1, 100);
-- If something goes wrong:
ROLLBACK TO before_items;
COMMIT;
```

**Topics covered:** BEGIN/COMMIT/ROLLBACK, isolation levels, savepoints, deadlock handling.

---

### Sequences

**Guide:** [Sequences](Sequences.md)

Generate unique identifiers automatically.

```sql
-- Using SERIAL (most common)
CREATE TABLE customers (
    id SERIAL PRIMARY KEY,
    name VARCHAR(100)
);

-- Using explicit sequence
CREATE SEQUENCE order_seq START 1000;
INSERT INTO orders (id, total) VALUES (NEXTVAL('order_seq'), 99.99);

-- Get current value
SELECT CURRVAL('order_seq');
```

**Topics covered:** SERIAL columns, CREATE SEQUENCE, NEXTVAL/CURRVAL, sequence options.

---

### Triggers

**Guide:** [Triggers](Triggers.md)

Automate database actions in response to data changes.

```sql
-- Audit trigger example
CREATE OR REPLACE FUNCTION audit_changes()
RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO audit_log (table_name, operation, changed_at)
    VALUES (TG_TABLE_NAME, TG_OP, CURRENT_TIMESTAMP);
    RETURN NEW;
END;
$$ LANGUAGE SBLR;

CREATE TRIGGER customers_audit
AFTER INSERT OR UPDATE OR DELETE ON customers
FOR EACH ROW EXECUTE FUNCTION audit_changes();
```

**Topics covered:** BEFORE/AFTER triggers, row-level vs statement-level, NEW/OLD references, trigger functions.

Also see: [Trigger Cheat Sheet](Trigger-Cheat-Sheet.md) for quick syntax reference.

---

### Procedures

**Guide:** [Procedures](Procedures.md)

Encapsulate business logic in reusable database routines.

```sql
-- Stored procedure example
CREATE OR REPLACE PROCEDURE transfer_funds(
    sender_id INT,
    receiver_id INT,
    amount DECIMAL
)
LANGUAGE SBLR AS $$
BEGIN
    UPDATE accounts SET balance = balance - amount WHERE id = sender_id;
    UPDATE accounts SET balance = balance + amount WHERE id = receiver_id;
END;
$$;

-- Call the procedure
CALL transfer_funds(1, 2, 100.00);
```

**Topics covered:** CREATE PROCEDURE, CREATE FUNCTION, parameters, return values, exception handling.

---

## Performance Guides

### Indexes

**Guide:** [Indexes](Indexes.md)

Speed up queries with appropriate indexes.

```sql
-- B-tree index (default, most common)
CREATE INDEX idx_customers_email ON customers(email);

-- Composite index for multi-column queries
CREATE INDEX idx_orders_customer_date ON orders(customer_id, order_date);

-- Partial index for filtered queries
CREATE INDEX idx_active_users ON users(email)
WHERE status = 'active';

-- Covering index to avoid table lookups
CREATE INDEX idx_orders_covering ON orders(customer_id)
INCLUDE (total, status);
```

**Topics covered:** B-tree, Hash, GiST, GIN indexes, composite indexes, partial indexes, covering indexes, maintenance.

---

### Performance Tuning

**Guide:** [Performance Tuning](Performance-Tuning.md)

Optimize queries and server configuration for best performance.

```sql
-- Analyze query performance
EXPLAIN ANALYZE
SELECT * FROM orders
WHERE customer_id = 123
AND order_date > '2026-01-01';

-- Check for missing indexes
SELECT * FROM pg_stat_user_tables
WHERE seq_scan > idx_scan
AND n_live_tup > 10000;
```

**Topics covered:**
- Query analysis with EXPLAIN
- Index optimization strategies
- Configuration tuning (shared_buffers, work_mem)
- Connection pooling
- Monitoring and diagnostics

---

### Vector Search

**Guide:** [Vector Search](Vector-Search.md)

Build AI-powered applications with vector similarity search.

```sql
-- Create table with vector column
CREATE TABLE documents (
    id SERIAL PRIMARY KEY,
    content TEXT,
    embedding vector(1536)  -- OpenAI embedding size
);

-- Create HNSW index for fast search
CREATE INDEX idx_docs_embedding ON documents
USING hnsw (embedding vector_cosine_ops);

-- Find similar documents
SELECT id, content,
    1 - (embedding <=> $1) AS similarity
FROM documents
ORDER BY embedding <=> $1
LIMIT 5;
```

**Topics covered:**
- Vector data type
- Distance functions (L2, cosine, inner product)
- HNSW indexing
- Semantic search, RAG, recommendations

---

## Administration Guides

### Security

**Guide:** [Security](Security.md)

Protect your database with proper authentication and authorization.

```sql
-- Create user with password
CREATE USER app_user WITH PASSWORD 'secure_password';

-- Create role with specific permissions
CREATE ROLE readonly;
GRANT SELECT ON ALL TABLES IN SCHEMA public TO readonly;

-- Assign role to user
GRANT readonly TO app_user;

-- Revoke access
REVOKE INSERT, UPDATE, DELETE ON customers FROM app_user;
```

**Topics covered:** User management, roles, GRANT/REVOKE, TLS configuration, audit logging.

---

### Backup and Restore

**Guide:** [Backup and Restore](Backup-Restore.md)

Protect your data with regular backups and tested recovery procedures.

```bash
# Full database backup
sb_backup -U admin -d mydb -o backup.sbk --compress

# Restore database
sb_restore -U admin -d mydb --clean backup.sbk

# Export specific tables to CSV
sb_backup -U admin -d mydb -t customers -t orders \
    -f csv -o exports/

# Point-in-time recovery
sb_restore -U admin -d mydb \
    --base-backup base_20260119.sbk \
    --wal-dir /var/lib/scratchbird/wal \
    --target-time "2026-01-19 14:30:00"
```

**Topics covered:**
- Full and incremental backups
- Point-in-time recovery (PITR)
- Export formats (native, SQL, CSV)
- Automated backup scheduling
- Disaster recovery planning

---

## Common Patterns

### Optimistic Locking

```sql
-- Add version column
ALTER TABLE products ADD COLUMN version INT DEFAULT 1;

-- Update with version check
UPDATE products
SET name = 'New Name', version = version + 1
WHERE id = 123 AND version = 5;

-- Check if update succeeded (row count = 1)
```

### Soft Deletes

```sql
-- Add deleted_at column
ALTER TABLE customers ADD COLUMN deleted_at TIMESTAMP;

-- "Delete" by setting timestamp
UPDATE customers SET deleted_at = CURRENT_TIMESTAMP WHERE id = 123;

-- Query only active records
SELECT * FROM customers WHERE deleted_at IS NULL;

-- Create partial index for active records only
CREATE INDEX idx_active_customers ON customers(email)
WHERE deleted_at IS NULL;
```

### Audit Trail

```sql
-- Audit table
CREATE TABLE audit_log (
    id SERIAL PRIMARY KEY,
    table_name VARCHAR(100),
    record_id INT,
    operation VARCHAR(10),
    old_values JSONB,
    new_values JSONB,
    changed_by VARCHAR(100),
    changed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- Generic audit trigger (see Triggers guide for full example)
```

---

## Guide Conventions

All user guides follow these conventions:

| Element | Meaning |
|---------|---------|
| `code blocks` | SQL or shell commands you can run |
| **Bold** | Important terms or emphasis |
| *Italic* | Variable values you should replace |
| `-- comments` | Explanatory notes in SQL |
| Tables | Reference information, options, comparisons |

### SQL Dialect Notes

Most examples use PostgreSQL-compatible syntax since it's the most common. Where syntax differs between dialects:

```sql
-- PostgreSQL/ScratchBird native
CREATE TABLE example (id SERIAL PRIMARY KEY);

-- MySQL emulation
CREATE TABLE example (id INT AUTO_INCREMENT PRIMARY KEY);

-- Firebird emulation
CREATE TABLE example (id INT GENERATED BY DEFAULT AS IDENTITY PRIMARY KEY);
```

---

## Related Documentation

| Section | Description |
|---------|-------------|
| [Getting Started](../getting-started/) | First-time setup and basic usage |
| [Tutorials](../tutorials/) | Step-by-step project walkthroughs |
| [Admin Guide](../admin/) | Server administration and operations |
| [Reference](../reference/) | Complete syntax and option reference |
| [Language Guides](../language-guides/) | SQL dialect documentation |
| [Troubleshooting](../troubleshooting/) | Problem diagnosis and solutions |

---

## Contributing

Found an error or want to improve a guide? See the [Contributing Guide](../Contributing.md) for how to submit corrections and enhancements.

