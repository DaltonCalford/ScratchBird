# Audit + Temporal History Archive (Optional Beta)

**Document ID**: SBSEC-BETA-ARCHIVE-01  
**Status**: Draft (Optional Beta)  
**Scope**: Engine (temporal tables + audit logging + GC integration)  
**Related**:  
- `docs/specifications/ddl/DDL_TEMPORAL_TABLES.md`  
- `docs/specifications/Security Design Specification/08_AUDIT_COMPLIANCE.md`  
- `docs/specifications/triggers/TRIGGER_CONTEXT_VARIABLES.md`  
- `docs/specifications/transaction/TRANSACTION_MAIN.md`  

---

## 1. Purpose

Provide an **optional Beta implementation** that combines temporal tables and audit logging to support **long-term, high-assurance user activity tracking** (DML over time) with **GC-assisted archival** of historical versions and audit records.

This feature is designed for extreme security/compliance scenarios such as:
- “Show me exactly what user X did from time A to time B.”
- “Track all changes for tables Y with transaction and session correlation.”
- “Move old history versions out of the live database into a sealed, tamper-evident archive.”

---

## 2. Goals

- **Temporal lineage with user context**: link row history to user/session/transaction.
- **GC-driven archival**: optionally offload historical row versions and audit events.
- **Tamper-evident evidence chain**: preserve audit integrity across archive boundaries.
- **Queryable history**: provide standard SQL access to history and audit data.
- **Low disruption**: feature is opt-in and backward compatible.

---

## 3. Non-Goals

- Replace system-versioned temporal tables (this builds on them).
- Provide full PITR/WAL-based recovery (not in Beta scope).
- Enable cross-database time travel.

---

## 4. Concepts

### 4.1 Temporal History Tables
ScratchBird’s **system-versioned temporal tables** already preserve row history in hidden history tables.

This spec adds **optional enrichment** of those history rows with:
- `transaction_id`
- `statement_id`
- `session_id`
- `user_id` / `username`
- `client_address` / `application_name`

These are sourced from trigger context variables or engine session context.

### 4.2 Audit Log
ScratchBird’s audit log (`sys.audit_log`) provides a chain-hashed event record.

This spec adds **explicit correlation fields** so that audit events can be joined with temporal history rows by:
- `transaction_id`
- `statement_id`
- `session_id`

### 4.3 History Archive
GC can optionally relocate **older history rows** and/or **audit events** into an archive store.

Archive store options:
- **Internal archive tables** (recommended for Beta)
- **External sinks** (S3, WORM, syslog/Kafka; optional)

---

## 5. Data Model Extensions

### 5.1 Temporal History Columns (optional)
When enabled, history rows include audit metadata:

```sql
ALTER TABLE <history_table>
ADD COLUMN history_txn_id        BIGINT,
ADD COLUMN history_stmt_id       BIGINT,
ADD COLUMN history_session_id    UUID,
ADD COLUMN history_user_id       UUID,
ADD COLUMN history_username      VARCHAR(128),
ADD COLUMN history_client_addr   INET,
ADD COLUMN history_app_name      VARCHAR(128);
```

### 5.2 Audit Event Correlation Fields
Extend audit log storage with explicit transaction context:

```sql
ALTER TABLE sys.audit_log
ADD COLUMN transaction_id BIGINT,
ADD COLUMN statement_id   BIGINT;
```

### 5.3 Archive Tables
Optional archive tables mirror the active audit/history tables.

```sql
CREATE TABLE sys.audit_log_archive (
    LIKE sys.audit_log
);

CREATE TABLE sys.temporal_history_archive (
    table_uuid      UUID NOT NULL,
    row_data        JSONB NOT NULL,
    valid_from      TIMESTAMP NOT NULL,
    valid_to        TIMESTAMP NOT NULL,
    transaction_id  BIGINT,
    statement_id    BIGINT,
    session_id      UUID,
    user_id         UUID,
    username        VARCHAR(128),
    client_address  INET,
    application     VARCHAR(128),
    archived_at     TIMESTAMP NOT NULL,
    archive_batch   UUID NOT NULL
);
```

---

## 6. GC Archival Flow

### 6.1 Policy Configuration
Add **optional GC archival policies**:

```ini
# Enable archival
history_archive.enabled = true
history_archive.cutoff = '90 days'       # only move versions older than cutoff
history_archive.batch_size = 10000
history_archive.verify_chain = true
history_archive.target = 'catalog'       # catalog|file|external

# Optional audit archival
audit_archive.enabled = true
audit_archive.cutoff = '365 days'
```

### 6.2 GC Workflow

1. GC identifies history rows older than `cutoff`.
2. GC copies rows to archive table or external sink.
3. If `verify_chain = true`, GC verifies audit chain continuity for the batch.
4. GC deletes archived history rows from the live history table.

### 6.3 Chain Integrity
Audit chain should span both live and archived events:
- The **archive batch** stores the last chain hash from the live table.
- Archive records preserve `previous_hash` and `event_hash`.

---

## 7. Querying History + Audit

### 7.1 User Activity Report (Joined)

```sql
SELECT h.*, a.event_name, a.event_timestamp
FROM sys.temporal_history_archive h
JOIN sys.audit_log_archive a
  ON h.transaction_id = a.transaction_id
 AND h.statement_id = a.statement_id
WHERE h.username = 'alice'
  AND h.archived_at BETWEEN '2025-01-01' AND '2025-12-31'
  AND h.table_uuid IN (...);
```

### 7.2 Live + Archive Union View

```sql
CREATE VIEW sys.audit_log_all AS
SELECT * FROM sys.audit_log
UNION ALL
SELECT * FROM sys.audit_log_archive;
```

---

## 8. Trigger Integration (Optional)

If enabled, system-versioned tables automatically write audit metadata into history rows at commit time.

Alternatively, user-defined triggers can populate audit metadata using:
- `GET TRIGGER_TRANSACTION_ID`
- `GET TRIGGER_STATEMENT_ID`
- `GET TRIGGER_SESSION_ID`
- `GET TRIGGER_USER`

---

## 9. Security & Compliance

- Archive stores must maintain **tamper-evident chain hashing**.
- Archive access restricted to `role_auditor` or equivalent.
- Optional external WORM storage recommended for Level 6.

---

## 10. Open Implementation Notes

- `AuditEvent` struct currently lacks `transaction_id` / `statement_id`.
- History tables do not yet carry user/session metadata.
- GC/archive code path does not exist; this spec defines required behavior.

---

## 11. Acceptance Criteria (Optional Beta)

- [ ] Audit events contain transaction + statement IDs.
- [ ] Temporal history rows include user/session metadata when enabled.
- [ ] GC can archive history rows older than cutoff.
- [ ] Archive tables preserve audit chain integrity.
- [ ] Queryable reports show user actions across live + archive.

