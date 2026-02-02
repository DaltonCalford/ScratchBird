# Live Migration with Emulated Listener

Status: Draft (Beta). This document defines the end-to-end behavior for live
migration when legacy applications connect to ScratchBird via an emulated
wire protocol listener (Firebird/MySQL/PostgreSQL), while ScratchBird uses a
UDR connector to the legacy database.


## Implementation Checklist (Quick Tracking)

| Area | Requirement | Status |
| --- | --- | --- |
| Snapshot | Consistent snapshot tied to CDC cursor | TODO |
| CDC | Log-based CDC preferred, trigger fallback | TODO |
| Ordering | Commit-order apply + atomic txns | TODO |
| Schema Drift | Detection + pause + re-introspect | TODO |
| Constraints | Enforce PK/UNIQUE/FK on apply | TODO |
| LOBs | Full LOB snapshot + CDC updates | TODO |
| Types | Charset/collation/time zone preservation | TODO |
| Idempotency | Duplicate-safe apply via PK/unique | TODO |
| Telemetry | sys.migration_status + audit log | TODO |
| Cutover | Lag=0 + audit pass + sequence sync | TODO |
| Rollback | Recorded window + divergence audit | TODO |
| Security | TLS + secret storage + host allowlist | TODO |

## 1. Overview

**Goal:** Allow legacy applications to point at ScratchBird immediately, while
data is migrated in the background and the emulated schema becomes the new
primary database with controlled cutover and audit phases.

This specification is the canonical description for:
- Emulated listener routing during migration.
- Dual-write and dual-read audit behavior.
- Cutover and optional mirror-to-legacy rollback window.


This specification defines a **snapshot + change data capture (CDC)** approach
per engine, with an emphasis on **log-based CDC** (WAL/binlog/replication log)
as the preferred method. Trigger-based CDC is an allowed fallback when log-based
CDC is unavailable.

## 2. Actors and Components

- **Legacy App**: Unmodified client (e.g., Firebird client tools).
- **ScratchBird Listener**: Emulated protocol listener for legacy dialect.
- **UDR Connector**: Remote Database UDR connection to legacy DB.
- **Emulated Schema**: `emulated_<server>` schema generated from legacy metadata.
- **Legacy Schema**: `legacy_<server>` foreign schemas and tables.

## 3. High-Level Flow

```
Legacy App
   |
   | (emulated protocol: FB/PG/MySQL)
   v
ScratchBird Listener
   |
   | Migration Router
   +------------------------------+
   |                              |
   v                              v
Legacy DB (UDR)            Emulated Schema (SB)
```


## 3.2 Schema Mounting and Emulated Tree Reconstruction

### Legacy Mount (Remote Schema Tree)
- On migration_begin, create a foreign server and mount the legacy schema under
  `legacy_<server>` using IMPORT FOREIGN SCHEMA (or equivalent metadata import).
- This mount is read-only by default unless allow_dml/allow_ddl is enabled.

### Emulated Tree Construction
- Generate an emulated schema tree under `emulated_<server>` that mirrors the
  legacy schema layout (schemas/tables/views/routines).
- All object name mappings and identifier normalization are recorded in a
  migration mapping table to support round-trips and error translation.

### Passthrough Merge (Legacy + Emulated)
- The emulated listener acts as a passthrough proxy in early modes
  (PROXY_ONLY/EMULATED_BUILD) and merges legacy and emulated trees logically:
  - Legacy reads/writes are routed to `legacy_<server>`
  - Emulated objects are created in `emulated_<server>`
- During DUAL_WRITE and DUAL_READ_AUDIT, both trees are active:
  - Writes apply to legacy first, then emulated
  - Reads are executed on legacy and emulated (audit)

### Verification and Control
- The router must tag each query with a source label (legacy/emulated/both).
- Audit results are stored with query hash, parameters, row counts, and
  checksum/sample diff.

## 4. Migration Modes (State Machine)

```
PROXY_ONLY
   |
   v
EMULATED_BUILD
   |
   v
DUAL_WRITE
   |
   v
DUAL_READ_AUDIT
   |
   v
PRIMARY_EMULATED
   |
   v
MIRROR_LEGACY (optional)
   |
   v
RETIRED
```

### Mode Summary

| Mode | Reads | Writes | Audit | Source of Truth |
|------|-------|--------|-------|-----------------|
| PROXY_ONLY | Legacy | Legacy | None | Legacy |
| EMULATED_BUILD | Legacy | Legacy | None | Legacy |
| DUAL_WRITE | Legacy | Legacy + Emulated | None | Legacy |
| DUAL_READ_AUDIT | Legacy (returns) + Emulated (audit) | Legacy + Emulated | Diff logging | Legacy |
| PRIMARY_EMULATED | Emulated | Emulated (+ optional mirror) | Optional | Emulated |
| MIRROR_LEGACY | Emulated | Emulated + Legacy | Optional | Emulated |
| RETIRED | Emulated | Emulated | None | Emulated |

## 5. Routing Rules

### 5.1 Reads

- **PROXY_ONLY / EMULATED_BUILD / DUAL_WRITE**: Reads execute on legacy only.
- **DUAL_READ_AUDIT**: Reads execute on both legacy and emulated.
  - **Return source**: Legacy (default) or emulated (configurable).
- **PRIMARY_EMULATED / MIRROR_LEGACY / RETIRED**: Reads execute on emulated only.

### 5.2 Writes

- **PROXY_ONLY / EMULATED_BUILD**: Writes execute on legacy only.
- **DUAL_WRITE / DUAL_READ_AUDIT**: Writes execute on legacy first, then emulated.
  - If legacy fails, the write fails and emulated is not modified.
  - If emulated fails after legacy succeeds, attempt compensation:
    - rollback emulated (if possible), log divergence if legacy cannot roll back.
- **PRIMARY_EMULATED / MIRROR_LEGACY**: Writes execute on emulated first.
  - If mirror is enabled, apply to legacy second.
  - Mirror failure is policy-controlled (strict or lenient).

### 5.3 DDL

- DDL is always applied to emulated schema only.
- Legacy schema changes must be reflected via re-introspection or manual admin
  updates to the emulated schema template.

## 6. Audit and Comparison Rules

During **DUAL_READ_AUDIT**, each query is executed against both legacy and
emulated targets, then compared using a selected policy:

- **Row count** (required)
- **Checksum** (recommended, ordered or unordered hash)
- **Sample compare** (optional)

Mismatch handling:
- Log mismatch with query hash, counts, timing, and sample diff.
- If mismatch rate exceeds a threshold, auto-block cutover.

Audit output should be persisted for review, e.g. `sys.migration_audit_log`
(planned), or a user-defined table if system views are not yet available.

## 7. Administrative Controls

Migration control uses the Remote Database UDR admin procedures (canonical):

```sql
CALL sys.migration_begin('legacy_fb', target_schema => 'emulated_fb', mode => 'proxy_only');
CALL sys.migration_begin('legacy_fb', mode => 'emulated_build');
CALL sys.migration_begin('legacy_fb', mode => 'dual_write');
CALL sys.migration_verify('legacy_fb');   -- enters DUAL_READ_AUDIT
CALL sys.migration_cutover('legacy_fb');  -- enters PRIMARY_EMULATED
CALL sys.migration_retire('legacy_fb');   -- enters RETIRED
```

### Policy Options (server-level or migration-level)

- `migration_mode`: current mode (see state machine).
- `audit_mode`: none | sample | full
- `audit_sample_rate`: 0.0 - 1.0 (default 0.01)
- `audit_return_source`: legacy | emulated (default legacy)
- `dual_write_policy`: strict | lenient (default strict)
- `mirror_policy`: strict | lenient (default strict)
- `mirror_legacy`: true | false (default false)

## 8. Cutover Criteria

Cutover should only proceed when:
- Migration backlog is 0 (replication lag is 0).
- Row counts match for all migrated tables.
- Audit mismatch rate is below threshold.
- Admin review is complete and approved.

## 9. Rollback Strategy

- **During DUAL_WRITE / DUAL_READ_AUDIT**: Continue to use legacy as source of
  truth; reset emulated data if needed and restart migration.
- **After PRIMARY_EMULATED**: If mirror-to-legacy is enabled, rollback can
  re-route reads/writes back to legacy using recorded delta windows.

## 10. Compatibility Notes

- Firebird emulation must respect on-commit event semantics (no immediate
  events).
- Legacy procedures/functions are passed through or recompiled into emulated
  PSQL as part of schema translation.


## 11. Engine-Specific CDC Methods (Preferred and Fallback)

### 11.1 PostgreSQL (Preferred: Logical Replication)

**Preferred CDC:** PostgreSQL logical replication (pgoutput) with replication
slots.

Key points:
- Use a replication connection (replication=database) and create a logical
  replication slot with `EXPORT_SNAPSHOT` to obtain a snapshot identifier.
- Use `COPY` (or a consistent `SELECT`) tied to the exported snapshot to
  backfill data.
- Start streaming from the slot at the exported snapshot LSN using the
  replication protocol.
- Apply changes in commit order to the emulated schema.

ScratchBird requirements:
- UDR connector must support replication protocol messages and decoding of
  `pgoutput` (or configured plugin).
- CDC stream must include table mapping and row changes (INSERT/UPDATE/DELETE).

References:
- https://www.postgresql.org/docs/current/logical-replication.html
- https://www.postgresql.org/docs/current/protocol-replication.html

**Fallback CDC:** Trigger-based change capture (INSERT/UPDATE/DELETE triggers
writing to a shadow change table) when replication is disabled.

### 11.2 MySQL / MariaDB (Preferred: Binlog Row-Based Replication)

**Preferred CDC:** Binlog row-based replication with GTID when available.

Key points:
- Ensure `binlog_format = ROW` (or MIXED with row events for all migrated tables).
- Establish a consistent snapshot using REPEATABLE READ.
- Record binlog file+position or GTID at snapshot start.
- Backfill using a consistent snapshot (SELECT/COPY-equivalent).
- Stream binlog events from the recorded position and apply in order.

ScratchBird requirements:
- UDR connector must decode binlog row events and apply them to emulated tables.
- Maintain binlog position/GTID watermark for recovery.

Reference (MariaDB replication overview, binlog model):
- https://mariadb.com/kb/en/replication-overview/

**Fallback CDC:** Trigger-based change capture (row-level triggers into a change
table) when binlog access is not allowed.

### 11.3 Firebird (Preferred: Firebird 4+ Logical Replication)

**Preferred CDC:** Firebird 4+ logical replication (when available on the
legacy server build).

Key points:
- Use the Firebird replication log to capture committed changes.
- Establish a consistent snapshot baseline using a stable backup/restore
  or a consistent snapshot transaction.
- Stream replication log entries to apply to emulated schema in order.

ScratchBird requirements:
- UDR connector must decode Firebird replication log stream (if exposed by
  protocol or plugin) and map to row changes.

Reference:
- https://firebirdsql.org/en/firebird-4-0/

**Fallback CDC:** Trigger-based capture on legacy tables (insert/update/delete
into a shadow change log) with periodic polling.

## 12. Consistency Guarantees

- Snapshot + CDC must be anchored to a shared consistency point.
- Apply order must preserve commit order from the source.
- For idempotency, changes should be applied using primary keys and
  conflict detection (ignore duplicate inserts, update-if-newer).

## 13. Failure Handling

- **CDC Stream Disconnect:** reconnect and resume from last persisted LSN/GTID.
- **Apply Errors:** isolate the offending transaction, log, and halt further
  apply until resolved (strict policy) or skip with audit (lenient policy).
- **Schema Drift:** detected via periodic re-introspection and audit diff.

## 14. Migration Metadata and Telemetry

Persist per-server migration metadata:
- current mode
- snapshot identifier
- CDC cursor (LSN/binlog file+pos/GTID/replication seq)
- lag metrics (seconds, transactions)
- audit mismatch counters

Expose via sys.migration_status and sys.migration_audit_log (or equivalent).

---

**Primary references:**
- `ScratchBird/docs/specifications/Alpha Phase 2/11-Remote-Database-UDR-Specification.md`
- `ScratchBird/docs/specifications/udr_connectors/UDR_CONNECTOR_BASELINE.md`


## Appendix A: Snapshot + CDC Bootstrap Sequences

Permissions required (summary):
- PostgreSQL: replication privilege, CREATE PUBLICATION, REPLICATION SLOT usage.
- MySQL/MariaDB: REPLICATION SLAVE/REPLICATION CLIENT (or SUPER), SHOW MASTER STATUS.
- Firebird: SYSDBA or replication-capable role for backup/replication access.


### A1. PostgreSQL (Logical Replication)

Prereqs:
- wal_level = logical
- max_replication_slots >= 1
- max_wal_senders >= 1

Example (run on legacy PostgreSQL):
```
-- Create a publication for all tables (or a subset)
CREATE PUBLICATION sb_migration_pub FOR ALL TABLES;
```

UDR-driven bootstrap (conceptual):
```
-- 1) Create replication slot and export snapshot
SELECT * FROM pg_create_logical_replication_slot('sb_slot', 'pgoutput');
-- Or use a logical replication connection to request EXPORT_SNAPSHOT

-- 2) Snapshot backfill using exported snapshot
SET TRANSACTION ISOLATION LEVEL REPEATABLE READ;
SET TRANSACTION SNAPSHOT '<exported_snapshot>';
COPY public.my_table TO STDOUT WITH (FORMAT csv);

-- 3) Start streaming from slot LSN and apply changes
START_REPLICATION SLOT sb_slot LOGICAL <lsn> ("proto_version" '1', "publication_names" 'sb_migration_pub');
```

ScratchBird actions:
- Use UDR to create the slot/publication (if allowed).
- Backfill with COPY or SELECT at exported snapshot.
- Stream logical replication and apply to emulated schema.

### A2. MySQL / MariaDB (Binlog Row-Based)

Prereqs:
- binlog_format = ROW
- log_bin enabled
- GTID enabled (optional but recommended)

Example (run on legacy MySQL/MariaDB):
```
-- Ensure row-based binlog
SET GLOBAL binlog_format = 'ROW';
```

UDR-driven bootstrap (conceptual):
```
-- 1) Capture consistent snapshot
SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ;
START TRANSACTION WITH CONSISTENT SNAPSHOT;

-- 2) Record binlog file + pos (or GTID)
SHOW MASTER STATUS;

-- 3) Backfill snapshot
SELECT * FROM my_table;  -- streamed back to ScratchBird

-- 4) Start binlog stream from recorded position
-- (UDR reads binlog events from file+pos or GTID and applies to emulated schema)
```

ScratchBird actions:
- Use UDR to capture SHOW MASTER STATUS and read binlog events.
- Apply row events in commit order.

### A3. Firebird (Replication Log + Snapshot)

Prereqs:
- Firebird 4+ with replication enabled (if available)

Example (run on legacy Firebird):
```
-- 1) Take a consistent backup snapshot
gbak -b -g legacy.fdb legacy_snapshot.fbk

-- 2) Restore snapshot for backfill
gbak -c legacy_snapshot.fbk legacy_snapshot.fdb
```

UDR-driven bootstrap (conceptual):
```
-- 3) Backfill from snapshot database
SELECT * FROM MY_TABLE;  -- streamed via UDR

-- 4) Stream replication log for live changes
-- (Requires replication log access or plugin; apply changes in order)
```

ScratchBird actions:
- Use UDR to read snapshot database for backfill.
- Use replication log or trigger-based fallback for CDC.


## Appendix B: Operational Requirements and Edge Cases

This appendix is normative for live migration behavior and closes remaining
undefined areas.

### B1. Permissions and Roles
- PostgreSQL: replication privilege; CREATE PUBLICATION; REPLICATION SLOT usage.
- MySQL/MariaDB: REPLICATION SLAVE/REPLICATION CLIENT or SUPER; SHOW MASTER STATUS.
- Firebird: SYSDBA or equivalent role for backup/replication access.
- UDR credentials must be stored in user mappings or external secrets.

### B2. Snapshot Consistency and Isolation
- Snapshot must be taken under a consistent isolation level.
- The CDC cursor must be captured at snapshot start (LSN/GTID/pos/seq).
- Snapshot and CDC start must be recorded atomically in migration metadata.

### B3. DDL and Schema Drift
- DDL is applied to emulated schema only; legacy DDL is not executed by default.
- Schema drift detection must run at a configurable interval.
- If legacy schema changes during migration, migration is paused until:
  - emulated schema is updated, and
  - backfill + CDC mapping is refreshed.

### B4. Identity, Sequences, and Auto-Increment
- Snapshot must copy sequence/identity values from legacy to emulated.
- CDC apply must update sequence state if rows are inserted with explicit values.
- Cutover requires sequence/identity alignment within a configured tolerance.

### B5. Constraints, Triggers, and Generated Columns
- During backfill, constraint checks can be deferred if supported.
- CDC apply must respect NOT NULL/PK/UNIQUE/FK constraints.
- Legacy triggers are not executed on emulated schema unless explicitly
  recompiled into PSQL.
- Generated columns must be recomputed on emulated writes.

### B6. Time Zones, Encodings, and Collations
- Snapshot data must be converted using the configured charset/collation
  mapping per engine.
- Timestamp values must preserve time zone semantics.
- Any lossy conversions must be logged in migration audit output.

### B7. LOBs, Large Objects, and Binary Data
- Snapshot must include blob/bytea/blob columns without truncation.
- CDC apply must support LOB updates (replace or chunked apply).
- For Firebird blob segments, order and segment boundaries must be preserved.

### B8. CDC Apply Ordering and Transaction Boundaries
- Apply changes in commit order.
- Multi-row transactions must be applied atomically to emulated schema.
- For engines that emit row events without explicit commit boundaries,
  the connector must reconstruct transaction boundaries from the log stream.

### B9. Conflict Resolution and Idempotency
- CDC apply must be idempotent; duplicate events must not corrupt state.
- Use primary keys or a deterministic unique key for UPSERT resolution.
- For UPDATE without PK, use a full row match with strict policy (or fail).

### B10. Backpressure and Throttling
- Apply rate is bounded by configurable limits to avoid saturating the engine.
- CDC lag metrics must drive throttling (CPU, IO, queue depth).
- Backfill can be chunked by primary key ranges or time windows.

### B11. Error Handling and Retry Policy
- All CDC apply errors are classified as retryable or fatal.
- Retryable errors use exponential backoff; fatal errors pause migration.
- Errors are logged with the source transaction ID and cursor position.

### B12. Monitoring and Telemetry
- Expose migration status in sys.migration_status:
  - mode, snapshot id, CDC cursor, lag, apply rate, error counts.
- Expose audit mismatches in sys.migration_audit_log.
- Emit alerts when lag exceeds threshold or audit mismatch rate spikes.

### B13. Security and Secrets
- Credentials are stored in encrypted secrets and never logged.
- TLS is required for remote connections in production.
- Host allowlists are enforced for UDR connectors.

### B14. Cutover Checklist
- CDC lag == 0
- Audit mismatch rate below threshold
- Sequences/identity synchronized
- No pending schema drift
- Admin approval recorded

### B15. Rollback Checklist
- Stop emulated writes or enable mirror-to-legacy
- Restore legacy as source of truth
- Record rollback window and affected transactions
- Reconcile divergence via audit report

### B16. Cleanup and Decommission
- Drop replication slots or binlog subscriptions
- Remove migration metadata and audit tables if configured
- Revoke UDR credentials no longer required

### B17. Test Coverage Requirements
- Unit tests for CDC cursor persistence and recovery
- Integration tests for snapshot + CDC on each engine
- Failure injection for disconnects, schema drift, and apply errors


## Appendix C: End-to-End Admin Guide (Commands + DDL/DML/PSQL)

This guide shows the complete migration lifecycle from initial mount through
cutover. Commands are written as canonical admin procedures and SQL.

### C1. Install and Verify UDR Connector
```
-- Admin (one-time)
CALL sys.udr_install('postgresql_udr', '/opt/scratchbird/udr/libscratchbird_postgresql_udr.so');
CALL sys.udr_verify('postgresql_udr');
```

### C2. Register FDW + Server + User Mapping
```
CREATE FOREIGN DATA WRAPPER postgresql_fdw
    HANDLER postgresql_udr_handler
    VALIDATOR postgresql_udr_validator;

CREATE SERVER legacy_pg
    FOREIGN DATA WRAPPER postgresql_fdw
    OPTIONS (host 'legacy-db', port '5432', dbname 'prod',
             allow_ddl 'true', allow_dml 'true', allow_psql 'true',
             allow_passthrough 'true');

CREATE USER MAPPING FOR migrator
    SERVER legacy_pg
    OPTIONS (user 'legacy_user', password '***');
```

### C3. Mount Legacy Schema Tree
```
IMPORT FOREIGN SCHEMA public
  FROM SERVER legacy_pg
  INTO legacy_pg;
```

### C4. Build Emulated Schema Tree
```
-- Generate emulated schema from legacy metadata
CALL sys.migration_begin('legacy_pg', target_schema => 'emulated_pg', mode => 'emulated_build');

-- Example: create an emulated table based on remote definition
CREATE TABLE emulated_pg.public.users (
  id bigint PRIMARY KEY,
  name text,
  created_at timestamp
);
```

### C5. Dual-Write Phase
```
CALL sys.migration_begin('legacy_pg', mode => 'dual_write');

-- Example write (applied to legacy then emulated)
INSERT INTO emulated_pg.public.users (id, name, created_at)
VALUES (1001, 'Alice', now());
```

### C6. Dual-Read Audit Phase
```
CALL sys.migration_verify('legacy_pg');

-- Example read (executes on both legacy and emulated)
SELECT count(*) FROM emulated_pg.public.users;

-- Example PSQL passthrough to legacy
CALL sys.remote_exec('legacy_pg', 'VACUUM');
```

### C7. Cutover
```
CALL sys.migration_cutover('legacy_pg');
```

### C8. Mirror Legacy (Optional)
```
CALL sys.migration_begin('legacy_pg', mode => 'mirror_legacy', mirror_policy => 'lenient');
```

### C9. Retire Legacy


### C10. MySQL/MariaDB Variant

```
-- Install + verify connector
CALL sys.udr_install('mysql_udr', '/opt/scratchbird/udr/libscratchbird_mysql_udr.so');
CALL sys.udr_verify('mysql_udr');

CREATE FOREIGN DATA WRAPPER mysql_fdw
    HANDLER mysql_udr_handler
    VALIDATOR mysql_udr_validator;

CREATE SERVER legacy_mysql
    FOREIGN DATA WRAPPER mysql_fdw
    OPTIONS (host 'legacy-db', port '3306', dbname 'prod',
             allow_dml 'true', allow_ddl 'true', allow_psql 'true',
             allow_passthrough 'true');

CREATE USER MAPPING FOR migrator
    SERVER legacy_mysql
    OPTIONS (user 'legacy_user', password '***');

-- Mount legacy schema
IMPORT FOREIGN SCHEMA prod
  FROM SERVER legacy_mysql
  INTO legacy_mysql;

-- Build emulated tree
CALL sys.migration_begin('legacy_mysql', target_schema => 'emulated_mysql', mode => 'emulated_build');

-- Dual-write
CALL sys.migration_begin('legacy_mysql', mode => 'dual_write');
INSERT INTO emulated_mysql.prod.users (id, name) VALUES (1001, 'Alice');

-- Dual-read audit
CALL sys.migration_verify('legacy_mysql');
SELECT count(*) FROM emulated_mysql.prod.users;

-- Cutover
CALL sys.migration_cutover('legacy_mysql');
```

### C11. Firebird Variant


### C12. ScratchBird Variant (SB-to-SB)

Use this flow to migrate between ScratchBird versions or hosts.

```
-- Install + verify connector
CALL sys.udr_install('scratchbird_udr', '/opt/scratchbird/udr/libscratchbird_scratchbird_udr.so');
CALL sys.udr_verify('scratchbird_udr');

CREATE FOREIGN DATA WRAPPER scratchbird_fdw
    HANDLER scratchbird_udr_handler
    VALIDATOR scratchbird_udr_validator;

CREATE SERVER legacy_sb
    FOREIGN DATA WRAPPER scratchbird_fdw
    OPTIONS (host 'legacy-host', port '5439', dbname 'prod',
             allow_dml 'true', allow_ddl 'true', allow_psql 'true',
             allow_passthrough 'true',
             tls_mode 'require');

CREATE USER MAPPING FOR migrator
    SERVER legacy_sb
    OPTIONS (user 'SYSARCH', password '***');

-- Mount legacy schema
IMPORT FOREIGN SCHEMA public
  FROM SERVER legacy_sb
  INTO legacy_sb;

-- Build emulated tree
CALL sys.migration_begin('legacy_sb', target_schema => 'emulated_sb', mode => 'emulated_build');

-- Dual-write
CALL sys.migration_begin('legacy_sb', mode => 'dual_write');
INSERT INTO emulated_sb.public.users (id, name) VALUES (1001, 'Alice');

-- Dual-read audit
CALL sys.migration_verify('legacy_sb');
SELECT count(*) FROM emulated_sb.public.users;

-- Cutover
CALL sys.migration_cutover('legacy_sb');
```

```
-- Install + verify connector
CALL sys.udr_install('firebird_udr', '/opt/scratchbird/udr/libscratchbird_firebird_udr.so');
CALL sys.udr_verify('firebird_udr');

CREATE FOREIGN DATA WRAPPER firebird_fdw
    HANDLER firebird_udr_handler
    VALIDATOR firebird_udr_validator;

CREATE SERVER legacy_fb
    FOREIGN DATA WRAPPER firebird_fdw
    OPTIONS (host 'legacy-db', port '3050', dbname '/data/employee.fdb',
             allow_dml 'true', allow_ddl 'true', allow_psql 'true',
             allow_passthrough 'true');

CREATE USER MAPPING FOR migrator
    SERVER legacy_fb
    OPTIONS (user 'SYSDBA', password '***');

-- Mount legacy schema
IMPORT FOREIGN SCHEMA public
  FROM SERVER legacy_fb
  INTO legacy_fb;

-- Build emulated tree
CALL sys.migration_begin('legacy_fb', target_schema => 'emulated_fb', mode => 'emulated_build');

-- Dual-write
CALL sys.migration_begin('legacy_fb', mode => 'dual_write');
INSERT INTO emulated_fb.public.employee (id, name) VALUES (1001, 'Alice');

-- Dual-read audit
CALL sys.migration_verify('legacy_fb');
SELECT count(*) FROM emulated_fb.public.employee;

-- Cutover
CALL sys.migration_cutover('legacy_fb');
```
```
CALL sys.migration_retire('legacy_pg');
```

## Appendix D: Design Guide (Routing + Control)

### D1. Router Responsibilities
- Route reads/writes based on migration_mode.
- Tag query source (legacy/emulated/both) for audit logging.
- Enforce allow_ddl/allow_dml/allow_psql/allow_passthrough.

### D2. Control Plane Contracts
- sys.migration_begin(mode, target_schema, policy options)
- sys.migration_verify()
- sys.migration_cutover()
- sys.migration_retire()
- sys.migration_status() (read-only view)

### D3. Audit Contracts
- Persist query hash, parameters, row counts, checksum/samples.
- Mismatch threshold blocks cutover until resolved.

### D4. Emulated vs Legacy DDL
- Emulated DDL is authoritative; legacy DDL is never executed by default.
- Legacy schema changes require re-introspection + emulated update.

### D5. Failure Domains
- Legacy failure during DUAL_WRITE => rollback emulated; keep legacy as source.
- Emulated failure during PRIMARY_EMULATED => use mirror_legacy if enabled.
- CDC stream disconnect => resume from cursor.

### D6. Verification Rules
- Minimum criteria: row counts, checksums, and sampled query diffs.
- All verification artifacts must be persisted for review.
