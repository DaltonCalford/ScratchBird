# Executor Lock/GC/Constraint Enforcement Matrix (V3)

Date: 2026-02-08  
Status: Authoritative (V3)

Purpose: define deterministic lock ordering, constraint enforcement order,
GC/visibility behavior, and error codes per opcode family. This document
eliminates ambiguity for a low‑context implementation.

**Firebird Source Alignment (Normative):**
- Locking: `firebird/src/jrd/lck.cpp`, `firebird/src/jrd/lck.h`
- Visibility/GC: `firebird/src/jrd/vio.cpp`
- Transaction state: `firebird/src/jrd/tra.cpp`

---

## 1) Global Lock Ordering (Authoritative)

When multiple objects must be locked, acquire in **strict order**:
1. Database
2. Tablespace
3. Schema
4. Table
5. Index
6. Row (ordered by primary key or stable UUID)
7. TOAST/LOB

Within a level, acquire locks in **lexicographic UUID order**.

Lock modes:
- `S` (shared/read)
- `X` (exclusive/write)
- `IS` / `IX` (intent shared / intent exclusive)

Deadlock handling:
- Use wait‑for graph.
- If deadlock detected, abort **youngest** transaction and return `SBX-LOCK-DEADLOCK`
  with SQLSTATE `40P01`.

---

## 2) Constraint Enforcement Order (Authoritative)

For all DML (INSERT/UPDATE/UPSERT/MERGE):
1. Domain validation (including domain CHECK).
2. NOT NULL constraints.
3. Column CHECK constraints.
4. Table CHECK constraints.
5. FOREIGN KEY constraints (referential).
6. UNIQUE constraints (including PRIMARY KEY).
7. DEFERRABLE constraints are queued and evaluated at COMMIT.

If any step fails, no physical write may occur.

---

## 3) GC / Visibility Rules (Authoritative)

All reads and writes follow MGA:
- Reads use snapshot visibility rules from `vio.cpp`.
- Updates create a new version; old version becomes a back‑version.
- DELETE marks record version as deleted (tombstone).
- GC may remove old versions only when no active transaction can see them.

---

## 4) Error Code Map (Executor)

All runtime semantic violations MUST use these codes:

| Code | SQLSTATE | Condition |
| --- | --- | --- |
| `SBX-CONSTRAINT-NOTNULL` | 23502 | NOT NULL constraint violated |
| `SBX-CONSTRAINT-UNIQUE` | 23505 | UNIQUE or PRIMARY KEY violated |
| `SBX-CONSTRAINT-FK` | 23503 | FOREIGN KEY violated |
| `SBX-CONSTRAINT-CHECK` | 23514 | CHECK constraint violated |
| `SBX-CONSTRAINT-EXCLUDE` | 23P01 | Exclusion constraint violated |
| `SBX-DOMAIN-VIOLATION` | 23514 | Domain constraint violated |
| `SBX-LOCK-DEADLOCK` | 40P01 | Deadlock detected |
| `SBX-LOCK-NOTAVAILABLE` | 55P03 | NOWAIT lock could not be acquired |
| `SBX-TXN-SERIALIZATION` | 40001 | Serialization failure |
| `SBX-TXN-READONLY` | 25006 | Write attempted in READ ONLY txn |
| `SBX-TXN-NOACTIVE` | 25P01 | Transaction required but not active |
| `SBX-OBJ-NOTFOUND` | 42P01 | Target object not found |
| `SBX-TYPE-MISMATCH` | 42804 | Type mismatch / cast failure |

---

## 5) Per‑Opcode Matrix

### 5.1 SELECT (SBLR3_SELECT)

Locks:
- `IS` on table(s) in FROM.
- `S` on referenced indexes if index‑only scan.
- If `FOR UPDATE/SHARE`, acquire row locks in PK order:
  - `FOR UPDATE` → row `X`
  - `FOR SHARE` → row `S`

Constraints:
- None.

GC/Visibility:
- Read‑only; snapshot visibility applied to all rows.

Errors:
- `SBX-LOCK-NOTAVAILABLE` if NOWAIT lock fails.

---

### 5.2 INSERT (SBLR3_INSERT)

Locks:
- `IX` on target table.
- `X` on affected indexes.
- Row locks on inserted rows are implicit (new versions).
- `S` on referenced parent tables for FK checks.

Constraints:
- Enforce order in §2.
- For FK checks, validate referenced keys exist and lock parent rows (`S`).
- For UNIQUE/PK, check index and lock duplicate keys (`S`) before insert.

GC/Visibility:
- New version created; old versions unaffected.

Errors:
- `SBX-CONSTRAINT-*` on violations.

---

### 5.3 UPDATE (SBLR3_UPDATE)

Locks:
- `IX` on target table.
- `X` on affected indexes.
- Row locks in PK order.
- `S` on referenced parent tables for FK checks.

Constraints:
- Enforce order in §2 on **new values**.
- If PK/UNIQUE key changes, treat as delete+insert for uniqueness checks.

GC/Visibility:
- New record version created; old version becomes back‑version.

Errors:
- `SBX-CONSTRAINT-*` on violations.

---

### 5.4 DELETE (SBLR3_DELETE)

Locks:
- `IX` on target table.
- `X` on affected indexes.
- Row locks in PK order.
- `S` on referencing child tables for FK checks.

Constraints:
- FK ON DELETE actions executed before deletion:
  - RESTRICT/NO ACTION → error if child exists.
  - CASCADE/SET NULL/SET DEFAULT executed in deterministic order.

GC/Visibility:
- Row marked deleted; versions GC’d when safe.

Errors:
- `SBX-CONSTRAINT-FK` on RESTRICT/NO ACTION failure.

---

### 5.5 MERGE (SBLR3_MERGE)

Locks:
- `IX` on target table.
- `S` or `X` on source depending on type (table/subquery).
- Row locks on matched target rows in PK order.

Constraints:
- INSERT/UPDATE/DELETE branches apply §2 order per branch.

GC/Visibility:
- Same as INSERT/UPDATE/DELETE paths.

Errors:
- Same as per branch.

---

### 5.6 COPY (SBLR3_COPY)

Locks:
- COPY FROM → `IX` on target table, `X` on indexes.
- COPY TO → `IS` on source tables.

Constraints:
- COPY FROM enforces §2 order per row batch.

GC/Visibility:
- COPY FROM creates new versions.
- COPY TO is read‑only.

---

### 5.7 DDL (CREATE/ALTER/DROP/TRUNCATE)

Locks:
- `X` on schema + target objects.
- `X` on dependent indexes/tables.

Constraints:
- DDL that validates constraints MUST scan existing rows and apply §2.

GC/Visibility:
- DDL creates catalog versions and invalidates dependent cache entries.

Errors:
- `SBX-OBJ-NOTFOUND` if target missing (unless IF EXISTS).

---

### 5.8 PSQL (Procedures/Functions/Triggers)

Locks:
- Use underlying DML/DDL lock rules for statements executed in block.

Constraints:
- Enforced by invoked DML operations.

GC/Visibility:
- Determined by the current transaction isolation and MGA snapshot.

---

## 6) Deterministic Ordering for Cascades

When cascading (FK ON DELETE/UPDATE CASCADE):
- Child tables are processed in lexicographic UUID order.
- Within each child, rows are processed in PK order.

---

## 7) Lock Escalation Rules

- Row lock count exceeding `lock_escalation_threshold` triggers escalation:
  - Row locks released.
  - Table lock upgraded to `X`.
- Escalation is deterministic and must be logged.

---

## 8) Deferred Constraint Queue

- DEFERRABLE constraints are recorded in a per‑transaction queue.
- Queue is evaluated at COMMIT:
  - Failures rollback the transaction (unless conflict action specifies otherwise).

---

## 9) Cross‑References

- Transaction isolation/visibility: `transaction/TRANSACTION_MGA_CORE.md`
- Lock manager: `transaction/TRANSACTION_LOCK_MANAGER.md`
- Executor core: `EXECUTOR_V3_SBLR.md`
