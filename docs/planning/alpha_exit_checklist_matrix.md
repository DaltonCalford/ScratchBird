# Alpha Exit Checklist Matrix

## Status
- Version: 1.0
- Owner: TBD
- Date: YYYY-MM-DD
- Scope: Alpha exit for ScratchBird (standalone/local + full ScratchBird/MySQL/PostgreSQL/Firebird parity).

## How to Use
- Every item in this document must be marked PASS before Alpha exit.
- Do not mark PASS unless the required tests, artifacts, and evidence are attached.
- If an item is intentionally deferred, record DEFERRED with a signed exception and a Beta target.

## Global Alpha Exit Gates (all must PASS)
- [ ] Full build + full test suite passes from `build/`:
  - `cmake --build .`
  - `ctest -j 8 --output-on-failure`
- [ ] No TODO/stub markers remain in engine or tests:
  - `rg -n "TODO|stub|not implemented" src include tests`
- [ ] Protocol trace diffs pass:
  - `tests/protocol_traces/firebird/*`
  - `tests/protocol_traces/mysql/*`
  - `tests/protocol_traces/postgresql/*`
- [ ] All docs in `docs/findings/` are closed or explicitly deferred with reasons.
- [ ] All plans in `docs/planning/` are fully implemented and audited.

## Plan Compliance Matrix (Summary)
| Plan ID | File | Core Deliverables | Required Tests | Status | Evidence |
|--------|------|------------------|----------------|--------|----------|
| 01 | `docs/planning/plan_01_core_storage_gc.md` | Heap page ownership, columnstore persistence, index GC, shadow rebuild | Storage + restart + per-index GC | [ ] | |
| 01C | `docs/planning/plan_01_core_storage_gc_clarifications.md` | Clarifications applied (table_id, dual meta page, shadow rebuild) | Same as Plan 01 | [ ] | |
| 01D | `docs/planning/plan_01_task_d_index_gc_checklist.md` | Index GC coverage per type | Per-index GC tests | [ ] | |
| 02 | `docs/planning/plan_02_uuid_resolution_and_rename_move.md` | UUID resolver view, rename/move ops | Resolver + rename/move tests | [ ] | |
| 03A | `docs/planning/plan_03_sblr_version2_extended_opcodes.md` | SBLR v2 + 16-bit opcodes + txn payloads | Bytecode encode/decode tests | [ ] | |
| 03B | `docs/planning/plan_03_security_context_auth_audit_quorum.md` | AuthKey, session binding, audit, quorum | Security + audit tests | [ ] | |
| 04 | `docs/planning/plan_04_parser_and_compatibility.md` | Parser coverage + guardrails + txn syntax | Parser + bytecode tests | [ ] | |
| 05 | `docs/planning/plan_05_protocol_odbc_pool.md` | Protocol adapters + ODBC + pool + attachment routing | Protocol + ODBC + pool tests | [ ] | |
| 06 | `docs/planning/plan_06_metadata_show_and_catalog.md` | Catalog tables + SHOW + runtime monitoring | Catalog + SHOW tests | [ ] | |
| 07 | `docs/planning/plan_07_emulated_protocol_compatibility.md` | Emulated wire parity + tx semantics | Native client tests | [ ] | |
| 08 | `docs/planning/plan_08_protocol_conformance_testing.md` | Golden traces + fuzz + integration | Trace + fuzz tests | [ ] | |
| 09 | `docs/planning/plan_09_audit_methodology.md` | Audit report + evidence | Audit checks | [ ] | |
| 10 | `docs/planning/plan_10_cluster_domains_and_conflict_resolution.md` | Cluster-wide domains + conflict algorithm | Domain conflict tests | [ ] | |
| 11 | `docs/planning/plan_11_alpha_cluster_compatibility_guardrails.md` | Alpha guardrails for cluster | Guardrail tests | [ ] | |
| 12 | `docs/planning/plan_12_domain_runtime_and_type_system.md` | Domain runtime + casting + arrays | Domain runtime tests | [ ] | |
| 13 | `docs/planning/plan_13_mysql_emulation_parity.md` | MySQL parity (parser+protocol+catalog) | MySQL client tests | [ ] | |
| 14 | `docs/planning/plan_14_postgresql_emulation_parity.md` | PostgreSQL parity (parser+protocol+catalog) | PostgreSQL client tests | [ ] | |
| 15 | `docs/planning/plan_15_firebird_emulation_parity.md` | Firebird parity (parser+protocol+catalog) | Firebird client tests | [ ] | |
| 16 | `docs/planning/plan_16_attachment_transaction_model.md` | Always-in-transaction + attachments | Attachment/txn tests | [ ] | |

## Detailed Plan Checklists

### Plan 01 - Core Storage and GC
- Implementation
  - [ ] PageHeader includes `table_id` and is set on heap page allocation.
  - [ ] Heap page enumeration filters by `table_id` and rejects invalid table_id as corruption.
  - [ ] Columnstore segment catalog persists via dual meta page with generation counter + CRC32C.
  - [ ] Columnstore `loadSegmentCatalog()` and `saveSegmentCatalog()` implemented.
  - [ ] GC for all index types (`removeDeadEntries()` implemented per index).
  - [ ] Shadow index rebuild implemented: BUILDING -> ACTIVE -> RETIRED with XID range visibility.
  - [ ] `sys.catalog.index_versions` catalog table exists and is used.
- Tests
  - [ ] Restart tests for heap ownership and columnstore persistence.
  - [ ] Per-index GC tests for all index types listed in Plan 01D.
  - [ ] Table migration uses shadow rebuild; old index GC after no readers.
- Evidence
  - [ ] Code references for PageHeader, columnstore, and GC in `src/core/*`.
  - [ ] Tests under `tests/` with pass logs.

### Plan 02 - UUID Resolution + Rename/Move
- Implementation
  - [ ] Resolver view returns UUID, path, full path, object name, type.
  - [ ] Rename/move SBLR opcodes emitted and executed.
  - [ ] Hash indexes for UUIDs; B-tree for path lookups.
- Tests
  - [ ] Resolver lookup by UUID/path/name.
  - [ ] Rename/move across schemas and name changes.
- Evidence
  - [ ] Resolver view DDL and index DDL in catalog code.

### Plan 03A - SBLR v2 + Extended Opcodes
- Implementation
  - [ ] SBLR v2 enforced; 16-bit extended opcodes everywhere.
  - [ ] New transaction payload v2 and 2PC/autocommit opcodes.
- Tests
  - [ ] Bytecode encode/decode tests for extended opcodes.
  - [ ] START/SET/COMMIT/ROLLBACK payload tests.
- Evidence
  - [ ] `include/scratchbird/sblr/opcodes.h` and `src/sblr/executor.cpp` changes.

### Plan 03B - Security Context + Audit + Quorum
- Implementation
  - [ ] AuthKey catalog tables + CRUD.
  - [ ] Session binding persists and is immutable per transaction.
  - [ ] Audit log persistence with hash chain.
  - [ ] Quorum gate in permission cache.
- Tests
  - [ ] AuthKey lifecycle tests.
  - [ ] Audit persistence/integrity tests.
  - [ ] Quorum failure modes.
- Evidence
  - [ ] Catalog tables `sys.cluster.security.authkeys`, `sys.security.sessions`, `sys.security.audit_log` present.

### Plan 04 - Parser Coverage + Guardrails
- Implementation
  - [ ] V2 parser DDL coverage complete.
  - [ ] ScratchBird transaction syntax: SQL + Firebird legacy + conflict action + autocommit.
  - [ ] Emulated parser guardrails enforced; no ScratchBird-only features bleed.
- Tests
  - [ ] Parser unit tests for DDL/DML and transaction syntax.
  - [ ] Guardrail negative tests per dialect.
- Evidence
  - [ ] Parser implementations in `src/parser/*`.

### Plan 05 - Protocols, ODBC, Pool
- Implementation
  - [ ] Native adapter auth/cancel/describe implemented.
  - [ ] MySQL/PG/Firebird adapter gaps closed.
  - [ ] ODBC connect/execute/prepare/cancel implemented.
  - [ ] Connection pool reset enforces always-in-transaction behavior.
  - [ ] Native protocol uses header-based `attachment_id` + `txn_id` routing (non-zero required after AUTH_OK).
- Tests
  - [ ] Protocol integration tests for auth/cancel/COPY.
  - [ ] ODBC end-to-end tests.
  - [ ] Pool reset + reuse tests.
- Evidence
  - [ ] Adapter code under `src/protocol/adapters/*`.

### Plan 06 - Metadata, SHOW, Catalogs
- Implementation
  - [ ] All catalog tables for core objects exist.
  - [ ] SHOW commands use real catalog queries.
  - [ ] Runtime monitoring views implemented (attachments/transactions/locks).
- Tests
  - [ ] SHOW tests in `tests/unit/test_show_set_commands.cpp`.
  - [ ] Catalog tests in `tests/unit/test_catalog_manager.cpp`.
- Evidence
  - [ ] `src/catalog/virtual_catalog.cpp` runtime view definitions.

### Plan 07 - Emulated Protocol Compatibility
- Implementation
  - [ ] Full wire parity per engine.
  - [ ] Transaction semantics mapped for MySQL/PG/Firebird.
  - [ ] Replication commands rejected with explicit errors.
- Tests
  - [ ] Native client integration tests for each engine.
  - [ ] Autocommit/explicit txn block tests.
- Evidence
  - [ ] Protocol traces + logs.

### Plan 08 - Protocol Conformance Testing
- Implementation
  - [ ] Golden trace capture + diff tooling.
  - [ ] Fuzz harness for adapters.
- Tests
  - [ ] Trace diff passes for handshake/auth/query + txn flows.
  - [ ] Fuzz runs without crash.
- Evidence
  - [ ] `tests/protocol_traces/*` and diff outputs.

### Plan 09 - Audit Methodology
- Implementation
  - [ ] Audit report under `docs/findings/audit_results/` for all plans.
- Tests
  - [ ] Audit commands executed with outputs archived.
- Evidence
  - [ ] Audit report references code + tests.

### Plan 10 - Cluster Domains + Conflict Resolution
- Implementation
  - [ ] Cluster-wide domains with UUID identity.
  - [ ] Conflict resolution algorithm implemented.
- Tests
  - [ ] Domain conflict simulation tests.
- Evidence
  - [ ] Domain catalog DDL and conflict tables.

### Plan 11 - Alpha Cluster Compatibility Guardrails
- Implementation
  - [ ] Alpha features do not block future cluster support.
  - [ ] Composite XID reserved fields for cluster.
- Tests
  - [ ] Guardrail tests (schema, txn ID, metadata).
- Evidence
  - [ ] Code changes in transaction ID formats and metadata.

### Plan 12 - Domain Runtime + Type System
- Implementation
  - [ ] Domain DDL, constraints, validation, casts, arrays, sub-domains.
  - [ ] ALTER DOMAIN validation and pending cluster confirmation.
- Tests
  - [ ] Domain runtime tests including array constraints and depth limits.
- Evidence
  - [ ] Domain tables and cast rules in code.

### Plan 13 - MySQL Emulation Parity
- Implementation
  - [ ] Parser parity and guardrails.
  - [ ] Protocol parity and auth plugins.
  - [ ] Full catalog views per appendix.
  - [ ] Autocommit behavior matches MySQL.
- Tests
  - [ ] mysql CLI/JDBC integration tests.
  - [ ] SHOW/INFORMATION_SCHEMA queries match MySQL.
- Evidence
  - [ ] View SQL definitions + mapping code.

### Plan 14 - PostgreSQL Emulation Parity
- Implementation
  - [ ] Parser parity + guardrails.
  - [ ] Protocol parity (MD5/SCRAM, Cancel, COPY).
  - [ ] pg_catalog + information_schema coverage.
  - [ ] ReadyForQuery state and SQLSTATE parity.
- Tests
  - [ ] psql/libpq integration tests.
  - [ ] Catalog query parity tests.
- Evidence
  - [ ] View SQL definitions + mapping code.

### Plan 15 - Firebird Emulation Parity
- Implementation
  - [ ] Parser parity + guardrails.
  - [ ] Protocol parity (SRP/legacy, TPB, retaining).
  - [ ] RDB$/MON$/SEC$ coverage.
- Tests
  - [ ] isql + FlameRobin integration tests.
  - [ ] MON$ transaction/attachment views.
- Evidence
  - [ ] View SQL definitions + mapping code.

### Plan 16 - Attachment + Transaction Model
- Implementation
  - [ ] Attachment registry + multi-attachment routing.
  - [ ] Always-in-transaction enforcement.
  - [ ] Header-based attachment_id/txn_id required after AUTH_OK.
- Tests
  - [ ] Two attachments on one connection; isolated transactions.
  - [ ] Conflict action behavior for START/SET TRANSACTION.
- Evidence
  - [ ] Native protocol v1.1 header fields in spec and code.

## Findings Closure Matrix
| Findings Doc | Closed By Plan(s) | Evidence | Status |
|--------------|-------------------|----------|--------|
| `docs/findings/engine_gap_report.md` | 01,02,03A,04,06 | | [ ] |
| `docs/findings/domain_support_gaps.md` | 10,12 | | [ ] |
| `docs/findings/firebird_emulation_parity_audit.md` | 15,07 | | [ ] |
| `docs/findings/mysql_emulation_parity_audit.md` | 13,07 | | [ ] |
| `docs/findings/postgresql_emulation_parity_audit.md` | 14,07 | | [ ] |
| `docs/findings/firebird_wire_protocol_gaps.md` | 05,07,08 | | [ ] |
| `docs/findings/mysql_wire_protocol_gaps.md` | 05,07,08 | | [ ] |
| `docs/findings/postgresql_wire_protocol_gaps.md` | 05,07,08 | | [ ] |
| `docs/findings/alpha_cluster_compatibility_audit.md` | 11,16 | | [ ] |
| `docs/findings/database_lifecycle_upgrade_plan.md` | 11 (guardrails), 16 (attachment model) | | [ ] |

## Spec Coverage Matrix (Alpha-critical)
| Spec Doc | Covered By Plan(s) | Evidence | Status |
|----------|--------------------|----------|--------|
| `docs/specifications/FIREBIRD_TRANSACTION_MODEL_SPEC.md` | 03A,04,05,07,16 | | [ ] |
| `docs/specifications/Appendix_A_SBLR_BYTECODE.md` | 03A | | [ ] |
| `docs/specifications/wire_protocols/scratchbird_native_wire_protocol.md` | 05,16 | | [ ] |
| `docs/specifications/wire_protocols/mysql_wire_protocol.md` | 05,07,13 | | [ ] |
| `docs/specifications/wire_protocols/postgresql_wire_protocol.md` | 05,07,14 | | [ ] |
| `docs/specifications/wire_protocols/firebird_wire_protocol.md` | 05,07,15 | | [ ] |
| `docs/specifications/SECURITY_SYSTEM_SPECIFICATION.md` | 03B | | [ ] |
| `docs/specifications/draft_security_architecture_specification.md` | 03B | | [ ] |
| `docs/specifications/SYSTEM_CATALOG_STRUCTURE.md` | 06,13,14,15 | | [ ] |
| `docs/specifications/MYSQL_PARSER_SPECIFICATION.md` | 04,13 | | [ ] |
| `docs/specifications/POSTGRESQL_PARSER_SPECIFICATION.md` | 04,14 | | [ ] |
| `docs/specifications/firebird_spec.md` | 04,15 | | [ ] |

## Alpha Exit Sign-Off
- [ ] All matrix items PASS.
- [ ] Audit report attached under `docs/findings/audit_results/`.
- [ ] Release note for Alpha exit prepared (internal).
