# Plan: Audit 16–23 Repair (Parsers, Protocols, Test Binaries)

## Purpose
Address audit findings 16–23 and restart paused plans 06–08 with verified, executor-compatible behavior. Emulated parsers must emit the current executor bytecode layout; extensions to the bytecode are only allowed when the existing format cannot express required functionality.

## Decisions (Locked)
- Emulated parsers (PostgreSQL/MySQL/Firebird) emit the **current executor bytecode layout**.
- Bytecode layout extensions are allowed **only when the existing layout cannot express needed functionality** and must be documented.
- Page 1 is reserved for FSM; **no legacy catalog** lives there. Remove legacy references.
- Extensions catalog must align with **UDR system specifications** (CREATE LIBRARY / UDR engines/modules).
- Schema tree output must be a **human-readable indented tree**.
- Use `/docs/specifications/wire_protocols/*` for listeners and ISQL clients.

## Scope
- Audits: `docs/audit/16_*` through `docs/audit/23_*`.
- Plans: `docs/planning/PLAN_06_DEDICATED_ISQL_CLIENTS.md`, `docs/planning/PLAN_06_TEST_AUTOMATION_DESIGN.md`, `docs/planning/plan_07_emulated_protocol_compatibility.md`, `docs/planning/plan_08_protocol_conformance_testing.md`.

## Workstreams

### 1) Parser → Executor Bytecode Alignment (P0)
**Goal:** PostgreSQL/MySQL/Firebird parsers emit the executor’s current payload layout.

Tasks:
- PostgreSQL: implement the bytecode fixes in `docs/audit/19_postgresql_parser_correction_plan_checklist.md`.
- MySQL: implement the bytecode fixes in `docs/audit/20_mysql_parser_correction_plan_checklist.md`.
- Firebird: implement the bytecode fixes in `docs/audit/22_firebird_parser_correction_plan_checklist.md`.
- Document any required bytecode extensions and add versioning flags only when absolutely required.

### 2) Missing Test Binaries (P0)
**Goal:** `ctest --test-dir build` builds and runs all declared tests (no “Not Run” due to missing executables).

Tasks:
- Ensure all `add_executable(test_*)` targets in `tests/CMakeLists.txt` are built by default.
- Add/extend a top-level `scratchbird_test_binaries` dependency from the main build or make it part of `all`.
- Verify binaries exist for: `test_brin_gc`, `test_rtree_gc`, `test_gin_gc`, `test_shadow_index_rebuild`, columnstore tests, and remaining integration/perf/tsan targets as appropriate.

### 3) Dedicated ISQL Clients (P0)
**Goal:** Provide native-wire ISQL clients for Firebird/MySQL/PostgreSQL (plus ScratchBird native).

Tasks:
- Implement `sb_pg_isql` and `sb_my_isql` using the wire protocol specs in `docs/specifications/wire_protocols/`.
- Keep `sb_fb_isql` aligned to Firebird wire protocol spec; validate batch mode and output formatting.
- Create a shared CLI helper library if needed (`cli/isql_common`) to avoid duplication.

### 4) Server Listener/Pool Completion (P0)
**Goal:** A complete listener + parser pool per protocol for native and emulated engines.

Tasks:
- Complete ServiceController listener wiring (native + PG/MySQL/Firebird) using `docs/specifications/wire_protocols/*`.
- Fix sb_server CLI vs client autostart mismatch (`--database/--daemon`).
- Ensure listener passes connection to parser pool and enforces per-protocol sessions.

### 5) Auth + TLS Wiring (P0)
**Goal:** Security integrations active in server path.

Tasks:
- Wire HBA, SCRAM, cert, MFA, LDAP, Kerberos, OAuth, SAML into server authentication flow.
- Wire TLS server negotiation into listeners (not just client-side).
- Ensure AuthManager is used by protocol adapters and server session code.

### 6) Schema Tree Output (P1)
**Goal:** Provide a human-readable schema tree of current resolver state.

Tasks:
- Add `sb_isql --schema-tree` (or dedicated CLI) that prints:
  - schema path hierarchy
  - sqlobject type labels: `(table)`, `(view)`, `(index)`, `(function)` etc.
- Source data from resolver + catalog caches.

### 7) Catalog + UDR Extensions (P1)
**Goal:** Implement the extensions catalog aligned to UDR specs.

Tasks:
- Implement catalog records + CRUD for UDR engines/modules/libraries.
- Link `CREATE LIBRARY` and UDR registrations to catalog tables.

### 8) Index DML Maintenance (P1)
**Goal:** Remove `NOT_IMPLEMENTED` for index DML on GIN/GiST/BRIN/SPGiST/HNSW.

Tasks:
- Wire insert/remove paths in `StorageEngine` for each index type.
- Ensure key extractors and index API surfaces are present.
- Add/enable integration tests for each index DML path.

### 9) RLS SELECT + View Security (P1)
**Goal:** Enforce SELECT RLS and view privilege checks.

Tasks:
- Add SELECT enforcement in executor path.
- Implement view privilege checks in `ViewSecurityManager` and integrate with permission system.

### 10) Protocol Conformance Testing (P1)
**Goal:** Wire protocol trace capture/diff tools and native client conformance tests.

Tasks:
- Implement `tools/proto_trace_capture` and `tools/proto_trace_diff` per plan 08.
- Add trace assets under `tests/protocol_traces/`.
- Run protocol regression tests with native clients.

## Acceptance Criteria
- Emulated parsers execute core DDL/DML against executor with no layout mismatches.
- Full CTest run has **no missing executables**; remaining skips are explicit and documented.
- ISQL clients exist for Firebird/MySQL/PostgreSQL and run compatibility suites.
- Server listeners handle native and emulated protocols with TLS + Auth integrations wired.
- Schema tree command produces indented, human-readable output.

## Notes
- Page 1 is reserved for FSM; no legacy catalog support is required.
- Bytecode extensions must be documented in this plan and added only when strictly necessary.
