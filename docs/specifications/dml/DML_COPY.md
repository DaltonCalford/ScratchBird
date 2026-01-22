# DML COPY Statement Specification

**Status:** Draft (Alpha)
**Last Updated:** 2026-01-20

---

## 1) Purpose

Define the `COPY` statement for high-performance bulk data loading and unloading.
`COPY` supports streaming data between files, STDIN/STDOUT, and server-managed
storage, with predictable error handling and transaction semantics.

---

## 2) Scope

**In scope:**
- `COPY ... FROM` and `COPY ... TO` for tables
- CSV, TEXT, and BINARY formats
- Streaming via STDIN/STDOUT
- Server-side file access (with privilege checks)

**Out of scope:**
- Copying to/from remote URLs
- Cross-database copy
- In-place transformation pipelines (use SELECT into temp tables)

---

## 3) Syntax

```sql
COPY <table_name> [ '(' <column_list> ')' ]
  FROM { <filename> | STDIN }
  [ WITH '(' <copy_option_list> ')' ];

COPY <table_name> [ '(' <column_list> ')' ]
  TO { <filename> | STDOUT }
  [ WITH '(' <copy_option_list> ')' ];

COPY ( <select_statement> )
  TO { <filename> | STDOUT }
  [ WITH '(' <copy_option_list> ')' ];
```

---

## 4) Options

```sql
<copy_option_list> ::= <copy_option> { ',' <copy_option> }

<copy_option> ::=
    FORMAT { CSV | TEXT | BINARY }
  | DELIMITER <string_literal>
  | NULL <string_literal>
  | HEADER [ <boolean> ]
  | QUOTE <string_literal>
  | ESCAPE <string_literal>
  | ENCODING <string_literal>
  | BATCH_SIZE <integer_literal>
  | MAX_ERRORS <integer_literal>
  | ON_ERROR { ABORT | SKIP }
```

**Defaults:**
- `FORMAT TEXT`
- `DELIMITER '\t'` (TEXT) / `','` (CSV)
- `NULL '\\N'` (TEXT) / empty (CSV)
- `HEADER false`
- `QUOTE '"'` (CSV)
- `ESCAPE '"'` (CSV)
- `ENCODING 'UTF8'`
- `BATCH_SIZE 10000`
- `MAX_ERRORS 0`
- `ON_ERROR ABORT`

**Notes:**
- `BATCH_SIZE` controls flush/checkpoint frequency for large loads.
- `MAX_ERRORS` and `ON_ERROR` are optional safety valves; if `MAX_ERRORS=0`, any error aborts.

---

## 5) Semantics

### 5.1 Transaction Behavior

- `COPY FROM` runs in a single transaction by default.
- `COPY TO` reads from a consistent snapshot (per transaction isolation level).
- On error with `ON_ERROR ABORT`, the transaction is rolled back.
- With `ON_ERROR SKIP`, the load continues until `MAX_ERRORS` is exceeded.

### 5.2 Column Mapping

- If column list is provided, only those columns are read/written.
- Missing columns use default values when defined; otherwise error.

### 5.3 File Access

- `COPY ... FROM/TO <filename>` is server-side and requires file access privileges.
- `COPY ... FROM STDIN` and `COPY ... TO STDOUT` stream data through the client.

### 5.4 Data Type Handling

- Text format uses the canonical text representation of each type.
- CSV uses RFC4180-style escaping with configured `QUOTE` and `ESCAPE`.
- BINARY is engine-specific and versioned (documented in wire protocol appendix).

---

## 6) Security

- Server-side file access requires a dedicated role or privilege.
- The server must restrict paths to configured allowlists (e.g., `copy_allowed_paths`).
- `STDIN/STDOUT` is permitted for authenticated sessions without file access.

---

## 7) Emulation Mapping

- PostgreSQL: map directly to `COPY` semantics.
- MySQL: map `LOAD DATA INFILE` to `COPY FROM` where possible.
- Firebird: emulate bulk load via `EXECUTE BLOCK` or batched inserts (degraded mode).

---

## 8) Examples

### 8.1 CSV Import

```sql
COPY orders (id, customer_id, total)
FROM '/var/lib/scratchbird/import/orders.csv'
WITH (FORMAT CSV, HEADER true);
```

### 8.2 CSV Export

```sql
COPY orders TO '/var/lib/scratchbird/export/orders.csv'
WITH (FORMAT CSV, HEADER true);
```

### 8.3 Stream to Client

```sql
COPY (SELECT * FROM orders WHERE total > 100)
TO STDOUT WITH (FORMAT CSV, HEADER true);
```

---

## 9) Monitoring

Expose counters in `sys.performance` and Prometheus metrics:
- `copy_rows_total{direction=from|to}`
- `copy_bytes_total{direction=from|to}`
- `copy_errors_total`

---

## 10) Related Specifications

- `docs/specifications/dml/04_DML_STATEMENTS_OVERVIEW.md`
- `docs/specifications/tools/SB_ISQL_CLI_SPECIFICATION.md`
- `docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md`
