# Alpha vs Beta Scope Status (Working Summary)

This document summarizes **what remains for Alpha** and **what is scoped for
Beta**, based on current planning/specs. It is intended as input to the
implementation plan and official roadmap updates.

Last updated: 2026-02-XX (doc-only)

---

## Alpha Remaining (synced with audits)

Status legend: Open | In Progress | Done

| Area | Item | Status | Source |
| --- | --- | --- | --- |
| Engine core | Tablespace routing defaults (table root pages + default tablespace resolution) | Done | `ScratchBird/docs/planning/ENGINE_CORE_ALPHA_COMPLETION_PLAN.md` (WS‑2) |
| Engine core | Index migration safety for SPGIST/BITMAP/COLUMNSTORE/LSM (TID/GPID update paths) | Done | `ScratchBird/docs/planning/ENGINE_CORE_ALPHA_COMPLETION_PLAN.md` (WS‑3) |
| Engine core | Expression/partial index root allocation still uses primary tablespace | Open | Code: `ScratchBird/src/core/catalog_manager.cpp:7513` |
| Engine core | Monitoring parity: remaining MON$ placeholders wired to sys.* | In Progress | `ScratchBird/docs/planning/ENGINE_CORE_ALPHA_COMPLETION_PLAN.md` (WS‑7) |
| Engine core | Backup/restore parity across tablespaces + catalogs | In Progress | `ScratchBird/docs/planning/ENGINE_CORE_ALPHA_COMPLETION_PLAN.md` (WS‑8) |
| Engine core | Restore uses only first tablespace file path (multi‑file tablespace not restored) | Open | Code: `ScratchBird/src/core/backup_manager.cpp:590` |
| Parser/PSQL | V2 parser completeness (DDL/DML/utility/PSQL) | In Progress | `ScratchBird/docs/planning/PLAN_V2_PARSER_COMPLETION.md` |
| PSQL runtime | PSQL bytecode emission + executor parity (FOR/CASE/SUSPEND/etc) | Open | `ScratchBird/docs/findings/V2_PARSER_DDL_DML_PSQL_AUDIT.md` |
| Resources | Timezones/Charsets/Collations data + loaders + catalog persistence | Open | `ScratchBird/docs/planning/RESOURCES_I18N_TIMEZONE_REMEDIATION_PLAN.md` |
| Indexes | Inverted GC purge, IVF, Zone Maps, GPID/TID format checks | Open | `ScratchBird/docs/planning/TRACKER_INDEX_SPEC_GAPS.md` |

### Resolved Since Last Sync

- Tablespace routing defaults now tablespace‑aware (table root pages + schema default selection).
- Index migration safety now covers SPGIST/BITMAP/COLUMNSTORE/LSM update paths.

---

## Beta Scope (planned features beyond Alpha)

### 1) Cluster + Replication
Sources:
- `ScratchBird/docs/specifications/beta_requirements/replication/uuidv7-optimized/*`
- `ScratchBird/docs/specifications/Cluster Specification Work/`

- UUIDv8-HLC, leaderless quorum replication, schema colocation
- Time‑partitioned Merkle anti‑entropy
- MGA integration for distributed commits (2PC/consensus)
- Cluster manager, orchestration, and policy enforcement

### 2) Sharding + Cross‑Server Migration
Sources:
- `ScratchBird/docs/specifications/storage/ONLINE_TABLESPACE_MIGRATION.md`
- `ScratchBird/docs/specifications/storage/SHARD_MIGRATION_SPEC.md`

- Online migration to other servers (shard movement)
- Shard‑aware metadata and monitoring (`sys.shard_migrations`)

### 3) Native Execution Tiers
Sources:
- `ScratchBird/docs/specifications/sblr/SBLR_EXECUTION_PERFORMANCE_BETA.md`

- Optional JIT/AOT for hot PSQL paths
- Vectorized execution where safe

### 4) Drivers, ORMs, Tools, Applications
Sources:
- `ScratchBird/docs/specifications/beta_requirements/README.md`

- P0 language drivers, ODBC/JDBC, major ORMs
- Tooling (DBeaver, pgAdmin, BI tools), app integrations

### 5) Big‑Data / Streaming / Cloud
Sources:
- `ScratchBird/docs/specifications/beta_requirements/big-data-streaming/`
- `ScratchBird/docs/specifications/beta_requirements/cloud-container/`

- Spark/Flink/Kafka/Hadoop/ETL integrations
- Docker/Kubernetes/Helm/Terraform packaging

### 6) Optional Beta Engine Enhancements
Sources:
- `ScratchBird/docs/specifications/beta_requirements/optional/`
- `ScratchBird/docs/specifications/storage/FILE_SHRINK_COMPACTION_SPEC.md`

- Storage encoding optimizations
- File shrink/compaction tooling

---

## Notes
- Alpha definition: core engine + embedded/IPC/INET readiness; cluster/sharding
  explicitly deferred to Beta.
- This list is a consolidation; track current status in the per‑plan trackers
  for detailed checklists and code references.
