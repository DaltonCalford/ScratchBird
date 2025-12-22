# Plan 15 - Firebird Emulation Parity (Parser + Protocol + Catalog)

## Scope
Deliver 1:1 Firebird 5.0 client compatibility over the Firebird wire protocol, including SQL parser coverage (DDL/DML/PSQL/DCL), protocol flows, RDB$/MON$/SEC$ catalogs, and strict dialect guardrails. Prevent ScratchBird-only features from leaking into Firebird dialect.

## Priority
P0 (Alpha requirement).

## References
- `docs/specifications/firebird_spec.md`
- `docs/specifications/FirebirdReferenceDocument.md`
- `docs/specifications/wire_protocols/firebird_wire_protocol.md`
- `docs/findings/firebird_emulation_parity_audit.md`
- `docs/findings/firebird_wire_protocol_gaps.md`
- `docs/planning/appendix_firebird_catalog_columns.md`
- `docs/specifications/SECURITY_SYSTEM_SPECIFICATION.md`

## Decisions / Constraints (Resolved)
- Emulated databases live under schema path `remote.emulated.firebird.<server>.<db>` (full path `/remote/emulated/firebird/<server>/<db>`).
- RDB$, MON$, SEC$ catalogs must be cluster-safe and scoped to the emulated DB.
- Firebird SHOW commands remain **client-side** (isql); server rejects SHOW statements.
- Auth methods required in Alpha: SRP and legacy.
- Firebird user identity is not host-specific, but host/IP allow/deny must be enforced per security configuration.
- Deterministic hash ID mapping for Firebird-specific IDs (RDB$RELATION_ID, etc) with collision table.
- No emulated replication in Alpha.
- Firebird transaction semantics must be preserved:
  - Always-in-transaction (attach starts a transaction immediately).
  - COMMIT RETAINING / ROLLBACK RETAINING supported.
  - TPB options mapped to ScratchBird transaction settings.

## Order of Implementation
1) Schema mapping + emulated catalog bootstrap (server/db schema + view generation + filter scoping).
2) Firebird parser completion (DDL/DCL/PSQL) + dialect guardrails.
3) Firebird protocol adapter parity (auth, drop database, info packets, transactions).
4) Catalog coverage (RDB$/MON$/SEC$) using appendix column lists.
5) Tests (parser, protocol, catalog, native client compatibility).

## Concrete Code Touchpoints (Exact Files + Functions)
- Parser:
  - `src/parser/firebird/firebird_parser.cpp`
    - ALTER/DROP/RECREATE object gaps
    - CREATE PROCEDURE/FUNCTION/TRIGGER/DOMAIN/EXCEPTION/ROLE/PACKAGE stubs
    - MERGE, EXECUTE PROCEDURE/STATEMENT stubs
    - SET/SHOW/GRANT/REVOKE/COMMENT stubs
    - PSQL `FOR EXECUTE STATEMENT`, `LOOP`
    - Window spec TODO, predicate variant TODO
    - `parseSchemaPath` (restrict to Firebird rules)
- Adapter:
  - `src/protocol/adapters/firebird_adapter.cpp`
    - `ensureFirebirdSystemTables` (placeholder views)
    - DROP DATABASE stub
    - Auth validation TODO
- Catalog:
  - `src/catalog/firebird_catalog.cpp` (partial)
  - `include/scratchbird/catalog/emulation_view_generator.h` (expand Firebird views)
- Mapping:
  - `src/core/tid_resolver.*` (Firebird ID mapping)
- Tests:
  - `tests/unit/test_firebird_parser.cpp`
  - `tests/unit/test_firebird_drop_semantics.cpp`
  - `tests/integration/test_firebird_adapter_ipc.cpp`

## Required Data/Schema Changes
- Add Firebird ID mapping to `sys.emulation.emulated_id_map` (see Plan 13 DDL).
- Create per-DB catalog view schemas:
  - `remote.emulated.firebird.<server>.<db>.rdb$`
  - `remote.emulated.firebird.<server>.<db>.mon$`
  - `remote.emulated.firebird.<server>.<db>.sec$`
  - These are views/synonyms that point to `sys.catalog.*`, `sys.cluster.configuration.*`, `sys.security.*`, and `sys.runtime.*` (no physical tables under the emulated schema).

## Implementation Tasks (Detailed)

### 1) Schema Mapping and Catalog Bootstrap
- Add `server_name` to `ProtocolAdapterConfig` and set it from listener config.
- Update `FirebirdAdapter::ensureFirebirdSystemTables`:
  - Use schema path `remote.emulated.firebird.<server>.<db>`.
  - Create child schemas `rdb$`, `mon$`, `sec$`.
  - Replace literal UNION-based view construction with catalog-backed views.
- Implement placeholder substitution in `EmulationViewGenerator` for `{schema_id}`.

### 2) Parser Coverage and Guardrails
- Implement all Firebird DDL/DCL/PSQL stubs listed in the audit:
  - CREATE/ALTER/DROP/RECREATE for procedures, functions, triggers, domains, exceptions, roles, packages, sequences.
  - MERGE, EXECUTE PROCEDURE, EXECUTE STATEMENT.
  - SET, GRANT, REVOKE, COMMENT.
  - PSQL `FOR EXECUTE STATEMENT`, `LOOP`.
- Window specification parsing in `parseFunctionCall()`.
- Predicate variants in `parseLikeExpression()` (LIKE/CONTAINING/STARTING/SIMILAR).
- Guardrails:
  - Reject ScratchBird schema path tokens.
  - Firebird has no schemas; allow only single identifier, except `package.procedure` where Firebird syntax allows.
  - Reject CREATE SCHEMA and other non-Firebird constructs.

### 3) Protocol Adapter Parity
- Implement SRP authentication (server-side verification) and legacy auth.
- Enforce IP allow/deny rules per security configuration.
- Implement DROP DATABASE (currently stubbed).
- Complete info/response packet fields per Firebird protocol versions 10-13.
- Transaction behavior:
  - On attach: start a default transaction immediately.
  - Implement COMMIT RETAINING and ROLLBACK RETAINING opcodes.
  - Map TPB flags (read-only, isolation, lock-wait) to ScratchBird transaction settings.

### 4) Catalog Coverage (RDB$/MON$/SEC$)
- Implement **all** tables in `appendix_firebird_catalog_columns.md`:
  - RDB$ system tables
  - MON$ monitoring tables
  - SEC$ security tables
- Ensure all views filter to the emulated DB schema id.
- Map Firebird numeric type codes to ScratchBird types.
- Use deterministic mapping for `RDB$RELATION_ID`, `RDB$INDEX_ID`, etc.
- Populate RDB$ view metadata for views, generators, constraints, triggers, packages.

### 5) SHOW Behavior
- Firebird `SHOW` is client-side only.
- Parser should reject SHOW statements with a clear error (do not map to ScratchBird SHOW opcodes).

## Completion Checklist (Developer)
- [ ] All Firebird parser stubs removed and implemented.
- [ ] Firebird dialect guardrails enforced (no schemas, no ScratchBird-only features).
- [ ] Firebird auth supports SRP and legacy.
- [ ] Firebird transaction semantics implemented (always-in-transaction, retaining).
- [ ] RDB$/MON$/SEC$ tables match appendix column lists.
- [ ] DROP DATABASE and protocol gaps closed.

## Completion Checklist (Auditor)
- [ ] Parser accepts only Firebird syntax and rejects non-Firebird features.
- [ ] Wire protocol matches Firebird client expectations.
- [ ] Firebird transaction lifecycles match native behavior.
- [ ] System catalog queries return correct columns.
- [ ] All items in `docs/findings/firebird_wire_protocol_gaps.md` closed or deferred with explicit note.

## Testing Requirements
- Unit tests:
  - Extend `tests/unit/test_firebird_parser.cpp` for all DDL/PSQL features.
  - Add negative tests for non-Firebird constructs.
- Integration tests:
  - Firebird `isql` client connects and queries RDB$ catalogs.
  - DDL/DML on Firebird emulated DB works with correct metadata.
- Protocol tests:
  - SRP and legacy auth flows.
  - DROP DATABASE operation.
  - COMMIT/ROLLBACK RETAINING behavior.

## Acceptance Criteria
- Native Firebird clients connect and operate without protocol or catalog errors.
- Firebird SQL support is 1:1 with Firebird 5.0 (no ScratchBird-only features).
- RDB$/MON$/SEC$ catalog queries return correct columns and data types.

## Implementation Notes (Concrete)
- Use `appendix_firebird_catalog_columns.md` as the authoritative column list.
- For columns without ScratchBird equivalents, return NULL but keep column ordering.
- Add code comments for deterministic ID mapping and collision handling.
- Replication features are rejected explicitly in Alpha.
