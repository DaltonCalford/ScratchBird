# Official Roadmap

## Alpha (current)

### Core Engine (in progress)
- **Completed:** storage engine (heap), MGA transactions, base catalog, scheduler/jobs,
  constraint enforcement, RLS/definer security wiring, cache/buffer plan.
- **Remaining (tracked in `docs/planning/ALPHA_COMPLETION_MASTER_PLAN.md`):**
  - Tablespace routing defaults + root page allocation
  - Index migration safety for SPGIST/BITMAP/COLUMNSTORE/LSM
  - Monitoring parity (remaining MON$ placeholders)
  - Backup/restore parity across tablespaces/catalogs
  - Timezone/charset/collation resource loading + catalog persistence

### Parser + PSQL (in progress)
- **Completed:** V2 parser base + dialect parsers, semantic analyzer, baseline bytecode generation.
- **Remaining:** full V2 DDL/DML/utility coverage; MERGE/COPY/EXECUTE completeness;
  PSQL bytecode emission + executor parity; statement‑level CASE.

### Network & Service
- **Completed:** listener/pool/parser/server process operational, wire adapters (FB/MySQL/PG/native).
- **Remaining:** dialect parity test suites and any remaining auth/config wiring.

## Beta (next)
- Cluster + replication (leaderless quorum, UUIDv8‑HLC, Merkle anti‑entropy)
- Sharding + cross‑server migration
- JIT/AOT native execution tiers + vectorized execution
- Drivers/ORMs/tools/app integrations (ODBC/JDBC, BI tools, frameworks)
- Big‑data/streaming + cloud/container packaging
- Optional engine enhancements (storage encoding, file shrink/compaction)

## References
- Status: `docs/IMPLEMENTATION_STATUS_DASHBOARD.md`
- Alpha/Beta scope: `docs/findings/ALPHA_BETA_SCOPE_STATUS.md`
- Alpha plan: `docs/planning/ALPHA_COMPLETION_MASTER_PLAN.md`
- Planning index: `docs/planning/`
