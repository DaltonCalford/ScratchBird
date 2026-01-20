# Temporary Tables Specification

**Document ID:** SB-SPEC-TEMP-01
**Version:** 0.1
**Status:** Draft
**Date:** 2026-01-06

**WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional post-gold (replication/PITR).
Any WAL references in this document describe an optional post-gold stream for
replication/PITR only.

## 1. Purpose

This document defines the ScratchBird design for temporary tables, including
syntax, catalog representation, lifecycle, visibility rules, and compatibility
with Firebird, PostgreSQL, and MySQL behavior.

## 2. Scope

This specification covers:

- Temporary table DDL and DML semantics
- Session-scoped and transaction-scoped table behavior
- ON COMMIT actions
- Catalog metadata and visibility rules
- Storage, logging, and replication treatment
- Dialect mapping rules for Firebird, PostgreSQL, MySQL, and ScratchBird native

## 3. Goals

- Provide 1:1 wire and behavioral parity with supported dialects where possible.
- Ensure temporary tables are private to their creating session or transaction.
- Prevent temporary tables from leaking into global visibility or replication.
- Provide deterministic cleanup on commit, rollback, disconnect, and reset.

## 4. Terminology

- **Temporary table:** A table whose data is scoped to a session or transaction.
- **Metadata scope:** Whether the table definition is global (catalog persistent)
  or session-local (ephemeral metadata).
- **Data scope:** Whether table contents are per session or per transaction.
- **ON COMMIT action:** Behavior triggered when a transaction commits.

## 5. Supported Syntax

ScratchBird supports dialect-specific CREATE TABLE syntax and a normalized
internal form.

```
CREATE [ { GLOBAL | LOCAL } TEMPORARY | TEMP ] TABLE [ IF NOT EXISTS ] <name> (
    <column_def> [ , ... ]
    [ , <table_constraint> [ , ... ] ]
)
[ TABLESPACE <tablespace_name> ]
[ ON COMMIT { PRESERVE ROWS | DELETE ROWS | DROP } ];
```

Notes:
- ON COMMIT is accepted only where the emulated dialect supports it.
- For dialects that do not support ON COMMIT (e.g., MySQL), its presence MUST
  raise a dialect-appropriate syntax error.

## 6. Behavioral Model

### 6.1 Metadata Scope

Two metadata scopes are supported:

- **Global metadata:** Table definition persists in the catalog (Firebird GTT).
- **Session metadata:** Table definition exists only within the session (PG TEMP,
  MySQL TEMPORARY).

Rules:
- Global metadata MUST be visible in system catalogs to all sessions.
- Session metadata MUST be visible only to the creating session.
- Session metadata MUST be dropped on disconnect or explicit session reset.

### 6.2 Data Scope

Two data scopes are supported:

- **Session scope:** Data persists for the session, with ON COMMIT behavior
  controlling per-transaction cleanup.
- **Transaction scope:** Data exists only for the current transaction and is
  cleared on commit or rollback.

Rules:
- Data scope MUST match dialect expectations (see Section 9).
- Temporary data MUST NOT be visible to other sessions.

### 6.3 ON COMMIT Actions

Supported actions:

- **PRESERVE ROWS:** Data persists across commits (session scope only).
- **DELETE ROWS:** Data is truncated on commit.
- **DROP:** Temporary table is dropped on commit (session metadata only).

Rules:
- ON COMMIT DROP is valid only for dialects that support it (PostgreSQL).
- ON COMMIT DELETE ROWS is the default for Firebird GTTs.
- ON COMMIT PRESERVE ROWS is the default for PostgreSQL TEMP tables.

### 6.4 Visibility and Name Resolution

- A session-local temporary table MUST be resolved before a permanent table
  with the same name for PostgreSQL and ScratchBird native.
- For Firebird mode, global temporary table names MUST NOT conflict with
  permanent tables (Firebird behavior).
- Temporary tables MUST be visible only in the session that created them.

## 7. Catalog and Metadata Requirements

Temporary tables MUST include the following metadata fields:

- `is_temporary` (bool)
- `temp_metadata_scope` (GLOBAL, SESSION)
- `temp_data_scope` (SESSION, TRANSACTION)
- `on_commit_action` (NONE, DELETE_ROWS, PRESERVE_ROWS, DROP)
- `creating_session_id` (nullable)
- `creating_transaction_id` (nullable)
- `temp_schema_id` (nullable, session-local namespace)

Rules:
- Global metadata MUST NOT store `creating_session_id` except for auditing.
- Session metadata MUST store `creating_session_id` and be filtered by session.
- Catalog APIs MUST enforce visibility restrictions for temporary tables.

## 8. Storage, Logging, and Replication

- Temporary table data MUST be stored outside the main durable data path.
- Temporary table data MUST NOT be written to the optional write-after log stream.
- Temporary table data MUST NOT be included in replication streams by default.
- Temporary table data MUST NOT be included in full backups unless explicitly
  configured for diagnostic use.
- Engine MAY store temporary table data in memory or a dedicated temp tablespace.

## 9. Dialect Mapping

### 9.1 Firebird

- Syntax: `CREATE GLOBAL TEMPORARY TABLE` with ON COMMIT.
- Metadata scope: Global (catalog persistent).
- Data scope:
  - ON COMMIT DELETE ROWS -> Transaction scope.
  - ON COMMIT PRESERVE ROWS -> Session scope.
- Default ON COMMIT: DELETE ROWS.
- Name conflicts with permanent tables are not allowed.

### 9.2 PostgreSQL

- Syntax: `CREATE TEMP[TEMPORARY] TABLE` with optional ON COMMIT.
- Metadata scope: Session.
- Data scope: Session.
- Default ON COMMIT: PRESERVE ROWS.
- ON COMMIT DROP is supported.
- TEMP table name SHOULD shadow permanent table names for that session.

### 9.3 MySQL

- Syntax: `CREATE TEMPORARY TABLE`.
- Metadata scope: Session.
- Data scope: Session.
- ON COMMIT is NOT supported and MUST be rejected in MySQL mode.

### 9.4 ScratchBird Native

- Supports both GLOBAL TEMPORARY (Firebird semantics) and TEMPORARY (PostgreSQL
  semantics), depending on explicit keywords.
- Default behavior MUST be documented in the native SQL specification.

## 10. Execution Model

### 10.1 Parser

- Parser MUST record temporary flags and ON COMMIT behavior in the AST.
- Parser MUST enforce dialect-specific syntax limits.

### 10.2 SBLR Encoding

`CREATE_TABLE` MUST include temporary flags and ON COMMIT actions. A compact
bitfield is required for compatibility with existing plans:

```
flags byte layout
- bit 0: if_not_exists
- bit 1: or_replace
- bit 2-3: temp_type (0=none, 1=session, 2=transaction, 3=global)
- bit 4-5: on_commit (0=none, 1=delete_rows, 2=preserve_rows, 3=drop)
- bit 6-7: reserved
```

### 10.3 Executor

- Executor MUST create temporary metadata and register tables for cleanup.
- Executor MUST enforce visibility restrictions for temporary tables.
- Executor MUST apply ON COMMIT actions during transaction end.

### 10.4 Session and Transaction Cleanup

- On COMMIT:
  - DELETE ROWS: truncate table data.
  - PRESERVE ROWS: no change.
  - DROP: drop the table and release metadata.
- On ROLLBACK:
  - Transaction-scoped temp tables MUST be dropped.
  - Session-scoped temp tables MUST preserve definition; data MAY be cleared
    depending on dialect rules (Firebird: clear on transaction end).
- On DISCONNECT or SESSION RESET:
  - All session metadata and data MUST be dropped.

## 11. Restrictions and Safety Rules

Minimum enforced rules (dialect-specific extensions MAY apply):

- Temporary tables MUST NOT be visible to other sessions.
- Temporary tables MUST NOT be used for cross-session data exchange.
- Temporary tables MUST NOT be replicated by default.
- Global temporary tables MUST NOT allow external table/file clauses.
- Dialect-specific restrictions from Firebird/PostgreSQL/MySQL references MUST
  be enforced by the parser and executor.

## 12. Error Handling

Implementations MUST raise dialect-appropriate errors for:

- Attempted access to a temporary table from another session.
- ON COMMIT clauses not supported by the active dialect.
- Name collisions that violate dialect rules.
- Illegal references between tables when dialect rules disallow them.

## 13. Test Cases (Minimum)

```
-- Session-scoped temporary table
CREATE TEMPORARY TABLE session_temp (id INT, value TEXT);
INSERT INTO session_temp VALUES (1, 'test');
SELECT * FROM session_temp;  -- 1 row
-- Disconnect and reconnect
SELECT * FROM session_temp;  -- should fail

-- Transaction-scoped (Firebird)
CREATE GLOBAL TEMPORARY TABLE tx_temp (id INT) ON COMMIT DELETE ROWS;
BEGIN;
INSERT INTO tx_temp VALUES (1);
SELECT * FROM tx_temp;       -- 1 row
COMMIT;
SELECT * FROM tx_temp;       -- 0 rows

-- Session-scoped with preserve rows (Firebird)
CREATE GLOBAL TEMPORARY TABLE s_temp (id INT) ON COMMIT PRESERVE ROWS;
BEGIN;
INSERT INTO s_temp VALUES (1);
COMMIT;
SELECT * FROM s_temp;        -- 1 row

-- PostgreSQL ON COMMIT DROP
CREATE TEMP TABLE drop_on_commit (id INT) ON COMMIT DROP;
BEGIN;
INSERT INTO drop_on_commit VALUES (1);
COMMIT;
SELECT * FROM drop_on_commit;  -- should fail

-- Visibility isolation
-- Session 1:
CREATE TEMP TABLE my_temp (id INT);
-- Session 2:
SELECT * FROM my_temp;        -- should fail
```

## 14. Implementation Checklist (Summary)

- Parser: capture temp flags and ON COMMIT behavior.
- AST: store metadata scope, data scope, and on-commit action.
- SBLR: encode temp flags in CREATE_TABLE payload.
- Catalog: persist metadata, enforce visibility, store temp scopes.
- Executor: enforce lifecycle and cleanup on commit/rollback/disconnect.
- Storage: use temp tablespace or in-memory storage, no optional post-gold write-after log (WAL).
- Replication: exclude temp tables by default.
- Tests: implement and run cases in Section 13.
