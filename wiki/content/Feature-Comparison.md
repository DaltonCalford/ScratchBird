# ScratchBird Feature Comparison

This page is a high-level comparison of ScratchBird against other engines. It focuses on capability and intent, not marketing. ScratchBird status is based on current source code and project plans.

ScratchBird status legend:
- Implemented (Alpha) - available in the Alpha core
- In progress (Alpha) - planned for Alpha, not fully complete
- Planned (Beta) - planned for Beta scope

Notes:
- Other engines vary by version, edition, and extensions.
- The SQL Standard column indicates whether a feature is in the standard or left to implementations.

---

## Architecture and transactions

| Feature | ScratchBird status | Oracle | SQL Server | MySQL | PostgreSQL | Firebird | SQL Standard |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Snapshot MVCC / MGA transactions | Implemented (Alpha) - MGA | Yes (MVCC) | Partial (snapshot isolation) | Yes (InnoDB MVCC) | Yes (MVCC) | Yes (MGA) | Optional |
| WAL required for commit | Implemented (Alpha) - No, write-after log optional | Yes | Yes | Yes | Yes | No | Not specified |
| Readers block writers | Implemented (Alpha) - No | No | Partial (depends on isolation) | No (InnoDB) | No | No | Not specified |
| Version cleanup / garbage collection | Implemented (Alpha) | Yes | Yes (when versioning enabled) | Yes | Yes | Yes | Not specified |

---

## Language and execution

| Feature | ScratchBird status | Oracle | SQL Server | MySQL | PostgreSQL | Firebird | SQL Standard |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Engine uses bytecode IR (SBLR / BLR) | Implemented (Alpha) - SBLR | Yes | Yes | Partial | Yes | Yes (BLR) | No |
| Stored procedures | Implemented (Alpha) | Yes | Yes | Yes | Yes | Yes | Optional (SQL/PSM) |
| Triggers | Implemented (Alpha) | Yes | Yes | Yes | Yes | Yes | Optional |
| Views | Implemented (Alpha) | Yes | Yes | Yes | Yes | Yes | Yes |
| SQL dialect emulation (multi-parser) | Implemented (Alpha) | No | No | No | No | No | Not applicable |
| Wire protocol emulation | Implemented (Alpha) - PostgreSQL, Firebird, MySQL, Native adapters | No | No | No | No | No | Not applicable |
| Native compilation of procedures | Planned (Beta) | Yes | Yes | Partial | Partial (JIT/LLVM) | Partial | No |

---

## Schema and catalog model

| Feature | ScratchBird status | Oracle | SQL Server | MySQL | PostgreSQL | Firebird | SQL Standard |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Recursive multi-level schemas | Implemented (Alpha) | No | No | No | No | No | No |
| Multi-dialect catalog mapping | Implemented (Alpha) | No | No | No | No | No | Not applicable |
| Emulated system catalogs per dialect | Implemented (Alpha) | No | No | No | No | No | Not applicable |

---

## Operations and scale

| Feature | ScratchBird status | Oracle | SQL Server | MySQL | PostgreSQL | Firebird | SQL Standard |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Replication | Planned (Beta) | Yes | Yes | Yes | Yes | External | No |
| Point-in-time recovery | In progress (Alpha) | Yes | Yes | Yes | Yes | External | No |
| Clustering / HA | Planned (Beta) | Yes | Yes | Yes | External | External | No |
| Backup and restore tooling | Implemented (Alpha) | Yes | Yes | Yes | Yes | Yes | No |

---

## Index types

| Feature | ScratchBird status | Oracle | SQL Server | MySQL | PostgreSQL | Firebird | SQL Standard |
| --- | --- | --- | --- | --- | --- | --- | --- |
| B-tree | Implemented (Alpha) | Yes | Yes | Yes | Yes | Yes | Not specified |
| Hash | Implemented (Alpha) | Yes | Partial | Yes | Yes | No | Not specified |
| GiST | Implemented (Alpha) | No | No | No | Yes | No | No |
| GIN | Implemented (Alpha) - with compression | No | No | No | Yes | No | No |
| SP-GiST | Implemented (Alpha) | No | No | No | Yes | No | No |
| BRIN | Implemented (Alpha) | No | No | No | Yes | No | No |
| R-tree | Implemented (Alpha) | No | No | Yes (limited) | No (via GiST) | No | No |
| Bitmap | Implemented (Alpha) | Yes | No | No | No | No | No |
| LSM-Tree | Implemented (Alpha) - with compaction and block cache | No | No | No | No | No | No |
| HNSW (vector) | Implemented (Alpha) | Varies | Varies | Varies | Yes (pgvector) | No | No |
| Columnstore index | Implemented (Alpha) | Yes | Yes | No | No | No | No |
| Full-text index | Implemented (Alpha) | Yes | Yes | Yes | Yes (GIN+tsvector) | No | No |
| Inverted index | Implemented (Alpha) | Yes | Partial | Partial | Yes (GIN) | No | No |
| Bloom filter (attachable) | Implemented (Alpha) - on B-tree, Hash, GIN | No (built-in) | No | No | Yes (extension) | No | No |
| Expression indexes | Implemented (Alpha) | Yes | Yes (computed) | Yes (generated) | Yes | Yes | No |

---

## Security and authentication

| Feature | ScratchBird status | Oracle | SQL Server | MySQL | PostgreSQL | Firebird | SQL Standard |
| --- | --- | --- | --- | --- | --- | --- | --- |
| SCRAM-SHA-256 | Implemented (Alpha) | No | No | Yes | Yes | No | No |
| Kerberos/GSSAPI | Implemented (Alpha) | Yes | Yes | Yes (plugin) | Yes | No | No |
| LDAP authentication | Implemented (Alpha) | Yes | Yes (AD) | Yes (plugin) | Yes | No | No |
| OAuth 2.0 | Implemented (Alpha) | Partial | No | No | No | No | No |
| SAML | Implemented (Alpha) | Yes | No | No | No | No | No |
| Multi-factor auth (MFA) | Implemented (Alpha) | Yes | Yes | No | No | No | No |
| TLS client certificates | Implemented (Alpha) | Yes | Yes | Yes | Yes | Partial | No |
| Password policy | Implemented (Alpha) | Yes | Yes | Yes | No | No | No |
| Login attempt tracking | Implemented (Alpha) | Yes | Yes | Yes | No | No | No |
| Security quorum | Implemented (Alpha) | No | No | No | No | No | No |
| Row-level security | Implemented (Alpha) | Yes | Yes | No | Yes | No | Optional |
| Data encryption at rest | Implemented (Alpha) | Yes | Yes | Yes | No (extension) | No | No |
| Data masking | Implemented (Alpha) | Yes | Yes | No | No | No | No |
| Audit logging | Implemented (Alpha) | Yes | Yes | Yes (plugin) | Yes (extension) | No | No |

---

## Data types and search

| Feature | ScratchBird status | Oracle | SQL Server | MySQL | PostgreSQL | Firebird | SQL Standard |
| --- | --- | --- | --- | --- | --- | --- | --- |
| JSON / JSONB | Implemented (Alpha) | Yes | Partial | Yes | Yes | Partial | Optional |
| XML type | Implemented (Alpha) | Yes | Yes | Partial | Yes | No | Optional |
| Vector type | Implemented (Alpha) | Varies | Varies | Varies | Yes (pgvector ext) | No | No |
| Spatial types (geometry) | Implemented (Alpha) - 7 geometry types with SRID | Yes | Yes | Yes | Yes (PostGIS) | External | Optional |
| Range types | Implemented (Alpha) - 6 range types | No | No | No | Yes | No | No |
| Network types (INET/CIDR) | Implemented (Alpha) | No | No | No | Yes | No | No |
| UUID type | Implemented (Alpha) - UUIDv7 | Yes | Yes | Partial | Yes | Yes | No |
| TSVECTOR/TSQUERY | Implemented (Alpha) | No | No | No | Yes | No | No |
| Array type | Implemented (Alpha) | Yes (varrays) | No | No | Yes | No | Optional |
| Composite/Record type | Implemented (Alpha) | Yes | No | No | Yes | No | Optional |
| Domain types | Implemented (Alpha) | No | No | No | Yes | Yes | Yes |
| MONEY type | Implemented (Alpha) | No | Yes | No | Yes | No | No |
| INT128 | Implemented (Alpha) | No | No | No | No | Yes (FB4) | No |
| INTERVAL type | Implemented (Alpha) | Yes | No | No | Yes | No | Yes |
