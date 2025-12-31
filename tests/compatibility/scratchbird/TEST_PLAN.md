# ScratchBird Comprehensive Test Plan

## Overview

This document outlines the complete test coverage plan for ScratchBird, designed to match the depth and breadth of the 11,905 compatibility tests from Firebird, MySQL, and PostgreSQL.

**Target**: 5,000+ native ScratchBird tests covering all features comprehensively

## Test Categories

### 1. Data Types (Target: 1,500 tests)

#### 1.1 Numeric Types (300 tests)
- **Integer Types** (100 tests)
  - INT8, INT16, INT32, INT64, INT128
  - UINT8, UINT16, UINT32, UINT64
  - Aliases: SMALLINT, INTEGER, BIGINT
  - Tests: boundary values, overflow, underflow, arithmetic, bitwise, casting

- **Floating Point Types** (100 tests)
  - FLOAT32 (REAL, FLOAT)
  - FLOAT64 (DOUBLE, DOUBLE PRECISION)
  - Tests: precision, special values (NaN, Infinity), scientific notation, math functions

- **Decimal/Money Types** (100 tests)
  - DECIMAL(p, s) - various precision/scale combinations
  - MONEY
  - Tests: exact arithmetic, rounding, financial calculations, precision preservation

#### 1.2 String and Binary Types (300 tests)
- **String Types** (150 tests)
  - CHAR(n), VARCHAR(n), TEXT
  - Tests: UTF-8, encoding, collation, length, concatenation, pattern matching, LIKE/ILIKE

- **Binary Types** (150 tests)
  - BINARY, VARBINARY, BLOB, BYTEA
  - Tests: binary operations, encoding, hex representation, large objects

#### 1.3 Temporal Types (200 tests)
- DATE, TIME, TIMESTAMP, INTERVAL
- TIMESTAMP WITH TIME ZONE
- Tests: formatting, parsing, arithmetic, timezone conversions, date math, intervals

#### 1.4 Boolean Type (50 tests)
- BOOLEAN (TRUE, FALSE, NULL)
- Tests: logic operations, comparisons, three-valued logic

#### 1.5 Spatial/Geometric Types (200 tests)
- POINT, LINESTRING, POLYGON
- MULTIPOINT, MULTILINESTRING, MULTIPOLYGON, GEOMETRYCOLLECTION
- Tests: spatial operations, distance calculations, intersections, containment

#### 1.6 Special Types (250 tests)
- **UUID** (50 tests): generation, UUIDv7, ordering, indexing
- **JSON/JSONB** (100 tests): parsing, querying, operators, indexing
- **XML** (50 tests): parsing, XPath, validation
- **VECTOR** (50 tests): embeddings, similarity search, distance metrics

#### 1.7 Array and Composite Types (100 tests)
- ARRAY: operations, indexing, unnesting, array functions
- COMPOSITE: record types, field access, nesting

#### 1.8 Range Types (150 tests)
- INT4RANGE, INT8RANGE, NUMRANGE
- TSRANGE, TSTZRANGE, DATERANGE
- Tests: containment, overlap, operations, indexing

#### 1.9 Network Types (100 tests)
- INET, CIDR, MACADDR, MACADDR8
- Tests: address validation, subnet operations, sorting

#### 1.10 Text Search Types (100 tests)
- TSVECTOR, TSQUERY
- Tests: tokenization, stemming, ranking, GIN indexes

#### 1.11 Polymorphic Types (50 tests)
- VARIANT (tagged union)
- Tests: type switching, storage, retrieval

### 2. DDL Operations (Target: 800 tests)

#### 2.1 CREATE Operations (200 tests)
- CREATE DATABASE, SCHEMA, TABLE, VIEW, MATERIALIZED VIEW
- CREATE INDEX (all 11 types)
- CREATE DOMAIN, TYPE, SEQUENCE, GENERATOR

#### 2.2 ALTER Operations (200 tests)
- ALTER TABLE: add/drop/modify columns, constraints, rename
- ALTER INDEX: rebuild, rename
- ALTER DOMAIN, VIEW, SEQUENCE

#### 2.3 DROP Operations (100 tests)
- DROP with CASCADE
- DROP IF EXISTS
- DROP restrictions

#### 2.4 Constraints (300 tests)
- PRIMARY KEY, FOREIGN KEY, UNIQUE
- CHECK constraints
- NOT NULL, DEFAULT
- Deferred/immediate constraint checking

### 3. DML Operations (Target: 500 tests)

#### 3.1 INSERT (100 tests)
- Single row, multi-row, INSERT...SELECT
- INSERT...ON CONFLICT (UPSERT)
- RETURNING clause

#### 3.2 UPDATE (150 tests)
- Single row, multi-row
- UPDATE...FROM
- Subqueries in UPDATE
- RETURNING clause

#### 3.3 DELETE (100 tests)
- DELETE with WHERE
- DELETE...USING
- TRUNCATE vs DELETE

#### 3.4 SELECT (150 tests)
- Projections, WHERE, GROUP BY, HAVING, ORDER BY
- LIMIT, OFFSET
- DISTINCT, DISTINCT ON
- Window functions, CTEs, subqueries

### 4. Index Types (Target: 550 tests - 50 per index type)

#### 4.1 B-Tree Index (50 tests)
- Single column, composite
- Unique, non-unique
- Partial indexes, expression indexes
- ASC/DESC, NULLS FIRST/LAST

#### 4.2 Hash Index (50 tests)
- Equality searches
- Performance characteristics

#### 4.3 GiST Index (50 tests)
- Spatial data, ranges, text search
- Operator classes
- Custom operator classes

#### 4.4 SP-GiST Index (50 tests)
- Space-partitioned data
- Quad-trees, k-d trees

#### 4.5 GIN Index (50 tests)
- Array operations
- Full-text search
- JSONB querying

#### 4.6 BRIN Index (50 tests)
- Block range indexes
- Sequential data
- Time-series data

#### 4.7 R-Tree Index (50 tests)
- Spatial indexing
- Bounding boxes
- Nearest neighbor

#### 4.8 HNSW Index (50 tests)
- Vector similarity
- Approximate nearest neighbor
- Distance metrics (L2, cosine, dot product)

#### 4.9 LSM Tree Index (50 tests)
- Write-optimized
- Compaction
- Tiered storage

#### 4.10 Columnstore Index (50 tests)
- Column-oriented storage
- Compression
- Analytics queries

#### 4.11 Full-Text Index (50 tests)
- Text tokenization
- Stemming, ranking
- Search queries

### 5. Triggers (Target: 600 tests)

#### 5.1 Database Triggers (100 tests)
- **CONNECT** (20 tests): on database connection
- **ON TRANSACTION START** (20 tests): when transaction begins
- **ON COMMIT** (20 tests): before/after commit
- **ON ROLLBACK** (20 tests): before/after rollback
- **ON DISCONNECT** (20 tests): on database disconnection

#### 5.2 Table Triggers (400 tests)
- **BEFORE INSERT** (50 tests): validation, transformation
- **AFTER INSERT** (50 tests): auditing, cascading
- **BEFORE UPDATE** (50 tests): validation, OLD/NEW values
- **AFTER UPDATE** (50 tests): auditing, change tracking
- **BEFORE DELETE** (50 tests): validation, soft delete
- **AFTER DELETE** (50 tests): cleanup, cascading
- **INSTEAD OF** (50 tests): view updates
- **FOR EACH ROW** vs **FOR EACH STATEMENT** (100 tests)

#### 5.3 View Triggers (100 tests)
- **BEFORE SELECT** (25 tests): access control
- **AFTER SELECT** (25 tests): auditing
- **INSTEAD OF INSERT/UPDATE/DELETE** (50 tests): updatable views

### 6. Transactions and Concurrency (Target: 400 tests)

#### 6.1 Transaction Control (100 tests)
- BEGIN, COMMIT, ROLLBACK
- SAVEPOINT, RELEASE SAVEPOINT, ROLLBACK TO SAVEPOINT
- COMMIT RETAIN, ROLLBACK RETAIN (Firebird-style)
- Nested transactions

#### 6.2 Isolation Levels (100 tests)
- READ UNCOMMITTED
- READ COMMITTED
- REPEATABLE READ
- SERIALIZABLE

#### 6.3 MVCC and Visibility (100 tests)
- Snapshot isolation
- Own-changes visibility
- Phantom reads prevention
- Tuple visibility

#### 6.4 Locking and Concurrency (100 tests)
- Row-level locking
- Table-level locking
- Deadlock detection
- Lock timeouts

### 7. Stored Procedures and Functions (Target: 300 tests)

#### 7.1 Stored Procedures (150 tests)
- Parameter passing (IN, OUT, INOUT)
- Exception handling
- Cursors
- Dynamic SQL

#### 7.2 User-Defined Functions (150 tests)
- Scalar functions
- Table-valued functions
- Aggregate functions
- Window functions

### 8. Views (Target: 200 tests)

#### 8.1 Simple Views (100 tests)
- CREATE VIEW
- View updates (updatable views)
- CHECK OPTION

#### 8.2 Materialized Views (100 tests)
- CREATE MATERIALIZED VIEW
- REFRESH MATERIALIZED VIEW
- Incremental refresh
- Indexes on materialized views

### 9. Security and Permissions (Target: 300 tests)

#### 9.1 User Management (100 tests)
- CREATE USER, ALTER USER, DROP USER
- Password management
- User attributes

#### 9.2 Role Management (100 tests)
- CREATE ROLE, GRANT ROLE, REVOKE ROLE
- Role hierarchies

#### 9.3 Permissions (100 tests)
- GRANT/REVOKE on tables, views, sequences
- Column-level permissions
- Row-level security (RLS)
- WITH GRANT OPTION

### 10. Advanced Features (Target: 450 tests)

#### 10.1 Sequences and Generators (50 tests)
- CREATE SEQUENCE
- NEXTVAL, CURRVAL, SETVAL
- AUTO_INCREMENT behavior

#### 10.2 Domains (100 tests)
- CREATE DOMAIN with constraints
- ALTER DOMAIN
- Nested domains

#### 10.3 Common Table Expressions (50 tests)
- Non-recursive CTEs
- Recursive CTEs
- MATERIALIZED hints

#### 10.4 Window Functions (100 tests)
- ROW_NUMBER, RANK, DENSE_RANK
- LAG, LEAD, FIRST_VALUE, LAST_VALUE
- NTILE, PERCENT_RANK
- Custom window frames

#### 10.5 Foreign Data Wrappers (50 tests)
- FDW basics
- Remote queries
- JOIN pushdown

#### 10.6 Partitioning (100 tests)
- RANGE partitioning
- LIST partitioning
- HASH partitioning
- Sub-partitioning

## Test File Organization

```
tests/
├── datatypes/
│   ├── 001_numeric_integer_types.sql
│   ├── 002_numeric_floating_decimal.sql
│   ├── 003_string_char_varchar.sql
│   ├── 004_string_text.sql
│   ├── 005_binary_blob_bytea.sql
│   ├── 006_temporal_date_time.sql
│   ├── 007_temporal_timestamp.sql
│   ├── 008_temporal_interval.sql
│   ├── 009_boolean.sql
│   ├── 010-016_spatial_*.sql
│   ├── 017_uuid.sql
│   ├── 018_json_jsonb.sql
│   ├── 019_xml.sql
│   ├── 020_vector.sql
│   ├── 021_array.sql
│   ├── 022_composite.sql
│   ├── 023-028_range_*.sql
│   ├── 029-032_network_*.sql
│   ├── 033_tsvector_tsquery.sql
│   └── 034_variant.sql
│
├── ddl/
│   ├── 001-050_create_*.sql
│   ├── 051-100_alter_*.sql
│   ├── 101-150_drop_*.sql
│   └── 151-200_constraints_*.sql
│
├── dml/
│   ├── 001-020_insert_*.sql
│   ├── 021-040_update_*.sql
│   ├── 041-060_delete_*.sql
│   └── 061-100_select_*.sql
│
├── indexes/
│   ├── 001-005_btree_*.sql
│   ├── 006-010_hash_*.sql
│   ├── 011-015_gist_*.sql
│   ├── 016-020_spgist_*.sql
│   ├── 021-025_gin_*.sql
│   ├── 026-030_brin_*.sql
│   ├── 031-035_rtree_*.sql
│   ├── 036-040_hnsw_*.sql
│   ├── 041-045_lsm_*.sql
│   ├── 046-050_columnstore_*.sql
│   └── 051-055_fulltext_*.sql
│
├── triggers/
│   ├── database/
│   │   ├── 001-020_connect_*.sql
│   │   ├── 021-040_transaction_start_*.sql
│   │   ├── 041-060_commit_*.sql
│   │   ├── 061-080_rollback_*.sql
│   │   └── 081-100_disconnect_*.sql
│   ├── table/
│   │   ├── 001-050_before_insert_*.sql
│   │   ├── 051-100_after_insert_*.sql
│   │   ├── 101-150_before_update_*.sql
│   │   ├── 151-200_after_update_*.sql
│   │   ├── 201-250_before_delete_*.sql
│   │   ├── 251-300_after_delete_*.sql
│   │   ├── 301-350_instead_of_*.sql
│   │   └── 351-400_row_vs_statement_*.sql
│   └── view/
│       ├── 001-025_before_select_*.sql
│       ├── 026-050_after_select_*.sql
│       └── 051-100_instead_of_*.sql
│
├── transactions/
│   ├── 001-025_basic_transactions_*.sql
│   ├── 026-050_savepoints_*.sql
│   ├── 051-100_isolation_*.sql
│   ├── 101-150_mvcc_*.sql
│   └── 151-200_locking_*.sql
│
├── procedures/
│   ├── 001-075_stored_procedures_*.sql
│   └── 076-150_functions_*.sql
│
├── views/
│   ├── 001-050_simple_views_*.sql
│   └── 051-100_materialized_views_*.sql
│
├── security/
│   ├── 001-033_users_*.sql
│   ├── 034-066_roles_*.sql
│   └── 067-100_permissions_*.sql
│
└── advanced/
    ├── 001-010_sequences_*.sql
    ├── 011-030_domains_*.sql
    ├── 031-040_ctes_*.sql
    ├── 041-060_window_functions_*.sql
    ├── 061-070_fdw_*.sql
    └── 071-100_partitioning_*.sql
```

## Test Execution Strategy

### Phase 1: Core Data Types and Basic Operations (Weeks 1-2)
- All numeric types
- String and binary types
- Basic DML operations

### Phase 2: Advanced Data Types (Weeks 3-4)
- Temporal types
- Spatial types
- Special types (JSON, XML, UUID, VECTOR)

### Phase 3: DDL and Schema Management (Weeks 5-6)
- CREATE/ALTER/DROP operations
- Constraints
- Domains

### Phase 4: Indexes (Weeks 7-8)
- All 11 index types
- Index operations and maintenance

### Phase 5: Triggers (Weeks 9-10)
- Database triggers
- Table triggers
- View triggers

### Phase 6: Transactions and Concurrency (Weeks 11-12)
- Transaction control
- Isolation levels
- MVCC testing

### Phase 7: Advanced Features (Weeks 13-14)
- Stored procedures/functions
- Views
- Window functions
- Partitioning

### Phase 8: Security (Week 15)
- User management
- Role management
- Permissions and RLS

## Test Generation Automation

To accelerate test creation, we will use:

1. **Template-based generation**: Templates for common test patterns
2. **Parameterized tests**: Generate variants with different parameters
3. **Property-based testing**: Automated test case generation
4. **Reference test adaptation**: Adapt Firebird/MySQL/PostgreSQL tests

## Success Criteria

- **Coverage**: All 54+ data types tested comprehensively
- **Index Coverage**: All 11 index types with 50+ tests each
- **Trigger Coverage**: All trigger types and timings tested
- **Feature Coverage**: All major features tested
- **Pass Rate**: 95%+ pass rate on implemented features
- **Documentation**: Every test has clear purpose and expected results

## Timeline

- **Month 1**: Data types and basic DML (1,500 tests)
- **Month 2**: DDL and indexes (1,350 tests)
- **Month 3**: Triggers and transactions (1,000 tests)
- **Month 4**: Advanced features and security (750 tests)
- **Month 5**: Refinement and additional coverage (400 tests)

**Total**: 5,000+ comprehensive native ScratchBird tests

---

**Last Updated**: 2025-12-31
**Status**: In Progress
**Current Test Count**: 10 (initial set created)
**Target Test Count**: 5,000+
