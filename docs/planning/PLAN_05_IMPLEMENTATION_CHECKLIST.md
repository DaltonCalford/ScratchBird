# Plan 05 - ODBC Driver - IMPLEMENTATION CHECKLIST (ALPHA)

**Version:** 1.0
**Created:** 2026-01-05
**Status:** IN PROGRESS (Phase 1 started)
**Scope:** ScratchBird native ODBC driver (core conformance, network listener + parser bridge)

---

## PHASE 1: LIBSCRATCHBIRD NETWORK CLIENT + ODBC BRIDGE

- [x] Create shared `libscratchbird` wrapper target (shared library)
- [x] Define network client config struct (host/port/TLS/timeout/user/db)
- [x] Implement network client session (send/receive Message header/payload)
- [x] Implement CONNECT/AUTH handshake via native protocol
- [x] Implement QUERY execution + RowDescription/RowData parsing
- [x] Implement COMMIT/ROLLBACK/BEGIN message flow
- [x] Implement DISCONNECT handling
- [x] Add ODBC client bridge wrapper (C++ class) around libscratchbird
- [x] Wire ODBC connection lifecycle to client bridge
- [x] Add error propagation path (Status -> SQLSTATE + native code)
- [x] Implement TLS 1.3 handshake integration

## PHASE 2: TYPE CONVERSION (HYBRID STRATEGY)

- [x] Create type mapping table (WireType -> ODBC SQL type)
- [x] Create conversion table (WireType -> string/bytes fallback)
- [x] Implement `convertAndStore()` for numeric, text, binary, temporal
- [x] Implement UUID conversion
- [x] Implement JSON/XML/ARRAY/RECORD/VARIANT as text
- [x] Implement GEOMETRY/VECTOR as binary (WKB/bytes)
- [x] Add unit tests for type conversions (core set)

## PHASE 3: RESULT SET BINDING + FETCH

- [x] Implement `bindResultData()` (row -> bound buffers)
- [x] Implement `SQLGetData` for unbound columns
- [x] Support forward-only fetch (SQLFetch)
- [x] Support SQLFetchScroll (NEXT only for Alpha)
- [x] Populate descriptor metadata from result columns
- [x] Add tests for fetch + bind

## PHASE 4: CATALOG FUNCTIONS (FULL 10)

- [x] SQLTables
- [x] SQLColumns
- [x] SQLPrimaryKeys
- [x] SQLForeignKeys
- [x] SQLStatistics
- [x] SQLSpecialColumns
- [x] SQLTablePrivileges
- [x] SQLColumnPrivileges
- [x] SQLProcedures
- [x] SQLProcedureColumns
- [x] Ensure catalog results scoped to current database only
- [x] Add catalog tests (basic correctness)

## PHASE 5: TRANSACTION + AUTOCOMMIT SEMANTICS

- [x] Enforce autocommit ON: COMMIT after every statement
- [x] Autocommit OFF: COMMIT/ROLLBACK only via SQLEndTran
- [x] Ensure new transaction begins after COMMIT/ROLLBACK
- [x] Implement isolation level mapping (default for Alpha)
- [x] Add tests for autocommit behavior

## PHASE 6: CORE CONFORMANCE CLEANUP

- [ ] Remove remaining stubs in core ODBC API (Core/Basic)
- [ ] Ensure SQLBrowseConnect reports unsupported (Alpha)
- [ ] Validate SQLGetInfo responses for core conformance
- [ ] Add diagnostics for unsupported functions

## PHASE 7: TESTING + VALIDATION

- [ ] Add smoke tests for connect/exec/fetch
- [ ] unixODBC isql sanity tests
- [ ] Basic Excel/PowerBI/Tableau connection tests (manual)
- [ ] Document known limitations for Alpha

---

**Alpha Constraints (Locked):**
- ODBC connects via **network listener → parser → engine** only
- Core/Basic conformance only
- Hybrid type mapping
- Full 10 catalog functions
- Federation visibility: current database only
