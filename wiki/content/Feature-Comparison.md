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
| Wire protocol emulation | In progress (Alpha) | No | No | No | No | No | Not applicable |
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

## Data types and search

| Feature | ScratchBird status | Oracle | SQL Server | MySQL | PostgreSQL | Firebird | SQL Standard |
| --- | --- | --- | --- | --- | --- | --- | --- |
| JSON type and functions | Implemented (Alpha) | Yes | Partial | Yes | Yes | Partial | Optional |
| Vector search | Implemented (Alpha) | Varies | Varies | Varies | Varies (extensions) | No | No |
| Spatial types | Implemented (Alpha) | Yes | Yes | Yes | Yes (PostGIS) | External | Optional |
