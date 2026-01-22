# ODBC Driver Specification

**Status:** Draft (Beta)
**Last Updated:** 2026-01-20

---

## 1) Purpose

Define the ScratchBird ODBC 3.8 driver requirements for broad BI and enterprise
compatibility on Windows and Linux.

---

## 2) Scope

**In scope:**
- ODBC 3.8 core compliance
- Unicode support (UTF-16/UTF-8)
- Windows and Linux (unixODBC)
- ScratchBird native wire protocol (SBWP)

**Out of scope:**
- MSSQL/TDS emulation (post-gold)
- Embedded engine driver

---

## 3) Architecture

```
App -> ODBC Driver -> ScratchBird Network Listener (port 3092)
```

- Driver uses SBWP over TCP/TLS.
- No direct file access; all operations are server-side.

---

## 4) API Compliance

**Required API sets:**
- Core: SQLConnect, SQLDriverConnect, SQLPrepare, SQLExecute, SQLFetch
- Metadata: SQLTables, SQLColumns, SQLPrimaryKeys, SQLForeignKeys
- Diagnostics: SQLGetDiagRec, SQLGetDiagField
- Transactions: SQLSetConnectAttr(autocommit), SQLEndTran

**Optional (P1):**
- SQLBulkOperations
- SQLSetPos
- Asynchronous execution

---

## 5) Data Type Mapping (Baseline)

| ODBC Type | ScratchBird Type |
| --- | --- |
| SQL_INTEGER | INT |
| SQL_BIGINT | BIGINT |
| SQL_VARCHAR | VARCHAR |
| SQL_WVARCHAR | VARCHAR (UTF-8) |
| SQL_DECIMAL | NUMERIC |
| SQL_DOUBLE | DOUBLE |
| SQL_TIMESTAMP | TIMESTAMP |
| SQL_BINARY | VARBINARY |
| SQL_GUID | UUID |

---

## 6) Connection String

Minimum keys:
- `Server` (host)
- `Port` (default 3092)
- `Database`
- `User`
- `Password`
- `SSLMode` (disable/prefer/require)

Example:
```
Driver={ScratchBird};Server=localhost;Port=3092;Database=mydb;User=app;Password=secret;SSLMode=prefer;
```

---

## 7) Transactions

- Default: autocommit on
- Support explicit BEGIN/COMMIT/ROLLBACK
- Isolation levels: read_committed, repeatable_read, serializable

---

## 8) Error Handling

- Use SQLSTATE codes (HYC00 for optional features not implemented)
- Map ScratchBird error classes to ODBC diagnostics

---

## 9) Security

- TLS 1.3 required in production configurations
- Password auth gated by server policy
- No local file access from driver

---

## 10) Testing

- ODBC 3.8 compliance suite
- BI tool smoke tests (Power BI, Tableau, Excel)
- Unicode round-trip tests
- Large result set paging tests

---

## 11) Related Documents

- `docs/specifications/beta_requirements/connectivity/odbc/README.md`
- `docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md`
- `docs/specifications/types/DATA_TYPE_PERSISTENCE_AND_CASTS.md`
