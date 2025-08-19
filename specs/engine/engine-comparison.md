## Cross-Engine Feature and Implementation Comparison

Scope: Oracle Database, PostgreSQL, Microsoft SQL Server (MSSQL), Firebird (v6 baseline), and ScratchBird (planned: parser complete; engine design in `enginelib`). Focus areas: storage/ODS, concurrency, recovery, indexing, partitioning, replication/HA, extensibility, security/RLS, tablespaces, backup/restore, parallelism, and optimizer.

### Storage / On-disk structure
- **Oracle**: Datafiles grouped into tablespaces; blocks/extents/segments; ASM option; redo/undo. Strong checksums and corruption detection.
- **PostgreSQL**: Heap with 8KB (default) pages; per-relation files (relfilenodes); FSM/VM maps; TOAST for large rows; WAL; checksums optional.
- **MSSQL**: 8KB pages, extents (8 pages); allocation maps (PFS/GAM/SGAM/IAM); write-ahead log; `tempdb` for many transient ops.
- **Firebird**: Single logical database file (can be multi-file); PIP/TIP page families; MVCC back versions on data pages; no universal WAL; nbackup/incremental; page cache (CCH).
- **Implications**: Oracle/PG/MSSQL lean on WAL for durability and recovery speed; Firebird’s page-centric approach is simpler but complicates recovery/GC.

### Concurrency & MVCC
- **Oracle**: Consistent read via undo segments; SCN; readers never block writers. MVCC through UNDO.
- **PostgreSQL**: Tuple versioning with `xmin/xmax` and visibility map; VACUUM/HOT pruning; snapshots; readers and writers largely non-blocking.
- **MSSQL**: Lock-based by default; snapshot isolation optional, using version store in `tempdb`.
- **Firebird**: MVCC with record back-version chains on data pages; cooperative sweep/GC; TIP tracks txn states.
- **Implications**: PG/Oracle offer robust MVCC defaults; MSSQL provides choice; Firebird’s MVCC works but relies on periodic cleanup.

### Recovery & Durability
- **Oracle**: Redo logs + archive; fast instance/media recovery; fine-grained recovery windows.
- **PostgreSQL**: WAL + checkpoints; crash recovery; PITR with base backups + WAL archive.
- **MSSQL**: Transaction log; checkpoints; tail-log backups; multiple recovery models.
- **Firebird**: Forced writes; nbackup (incremental snapshots); page-shadowing legacy; no universal WAL.
- **Implications**: Without WAL, Firebird recovery scenarios are more limited; nbackup helps but is different from write-ahead logging.

### Indexing
- **Oracle**: B-tree, bitmap, reverse key, function-based; domain/spatial indexes.
- **PostgreSQL**: B-tree, GIN, GiST, BRIN, SP-GiST, hash; partial indexes; expression indexes.
- **MSSQL**: B-tree clustered/nonclustered; columnstore (row/clustered); filtered indexes; full-text.
- **Firebird**: Primarily B-tree; expression-like via computed fields; no GIN/GiST/BRIN/columnstore.
- **Implications**: Advanced index families (GIN/GiST/BRIN/bitmap/columnstore) offer big wins; Firebird lags here.

### Partitioning
- **Oracle**: Range/hash/list; composite; local/global indexes; mature management.
- **PostgreSQL**: Declarative partitioning (range/list/hash); pruning; attach/detach; improving steadily.
- **MSSQL**: Partition functions/schemes; sliding windows.
- **Firebird**: No native table partitioning.

### Replication & HA
- **Oracle**: Data Guard, GoldenGate; RAC for shared-disk clustering.
- **PostgreSQL**: Streaming replication; logical replication; robust ecosystem for HA/failover.
- **MSSQL**: AlwaysOn Availability Groups; log shipping; transactional/merge replication.
- **Firebird**: Built-in replication is limited/newer; historically third-party.

### Extensibility / FDW / Procedural
- **Oracle**: PL/SQL; Java stored procedures; extensibility via options/features.
- **PostgreSQL**: Rich extension system; FDW (foreign data); multiple procedural languages.
- **MSSQL**: T-SQL; CLR integration; external scripts (ML Services).
- **Firebird**: PSQL; UDRs; no native FDW.

### Security & RLS
- **Oracle**: VPD/FGAC; RLS; TDE; fine-grained auditing.
- **PostgreSQL**: RLS; extension-based encryption/TDE; strong auth/RBAC.
- **MSSQL**: Row-Level Security; TDE; Always Encrypted; auditing.
- **Firebird**: Users/roles; plugin-based encryption; no native RLS.

### Tablespaces & Storage Mgmt
- **Oracle**: Full-featured tablespaces; ASM; temp/undo tablespaces.
- **PostgreSQL**: Tablespaces (placement-oriented); simpler than Oracle.
- **MSSQL**: Filegroups; flexible placement/management.
- **Firebird**: Secondary files; limited tablespace semantics.

### Backup/Restore
- **Oracle**: RMAN (hot, incremental, PITR); archivelog.
- **PostgreSQL**: Base backups + WAL; PITR; tooling (pg_basebackup/barman/wal-g).
- **MSSQL**: Full/diff/log; PITR; enterprise tooling.
- **Firebird**: gbak logical; nbackup physical (incremental snapshot); online backup supported via nbackup.

### Parallelism & Optimizer
- **Oracle**: Mature parallel query; advanced CBO with hints.
- **PostgreSQL**: Parallel query; evolving CBO; extensions for statistics.
- **MSSQL**: Parallel query; advanced CBO; query store.
- **Firebird**: Limited parallel query; cost-based optimizer but fewer features.

---

## Differences vs Firebird (Gaps) and Strengths/Weaknesses
- **Missing or limited in Firebird**:
  - WAL-based durability and PITR
  - Native table partitioning
  - Advanced index methods (GIN/GiST/BRIN/bitmap/columnstore)
  - Robust built-in logical replication and HA management
  - Foreign data wrappers and pushdown
  - Row-level security and policy management
  - Parallel query and richer optimizer strategies
  - Rich tablespace/filegroup management
- **Strengths of Firebird**:
  - Simple deployment (embedded-friendly)
  - MVCC with non-blocking readers
  - Lightweight, page-centric engine
- **Weaknesses**:
  - GC/sweep can accumulate debt
  - Recovery options less flexible without WAL
  - Fewer enterprise features compared to Oracle/PG/MSSQL

## ScratchBird: Planned Additions (aligned with parser/specs)
- **Durability/Recovery**: Optional WAL/journal alongside nbackup-style incremental
- **Partitioning**: Declarative (range/list/hash), attach/detach, pruning
- **Indexes**: Hooks for GIN/RTREE/BITMAP/BRIN; partial and expression indexes
- **Replication/HA**: Logical replication (pub/sub) with pause/resume; HA orchestration specs
- **FDW**: Provider SPI (ODBC/MSSQL); foreign server/table/user mapping; import foreign schema
- **Security**: Row-level security policies; audit/trace surfaces
- **Tablespaces**: True page spaces with move/set on tables/indexes; detach/attach; transport
- **Backup/Restore**: SQL surfaces with full/incremental and history; online-friendly
- **Parallelism**: Future phase; planner hooks and worker model

## Implementation Notes (EngineLib)
- See `specs/engine/enginelib.yaml` for layered design and ODS outline.
- Create DB bootstrap: header page, space catalog, system relations, generator pages, TIP/PIP seeds.
- MVCC: snapshot transaction IDs; back-version pruning; background vacuum optional.
- Planner: cost model aware of index methods, partition pruning, FDW pushdown.
