# Parser Implementation Canonical Specification

## Purpose
Define implementation-complete parser requirements for native and emulated surfaces, including lifecycle, translation behavior, catalog synchronization, security lowering, and response mapping.

## Scope
- Native parser and all emulated parser families.
- Parser process behavior from accepted client request to engine response delivery.
- Parser capability gating and mapping to `SBLR`.
- Parser diagnostics and error conversion.
- Parser-side catalog synchronization helpers used to lower names to UUID-bound `SBLR`.
- Parser-side lowering of security, DCL, RLS, and domain-security statements.

## Out of scope
- Engine opcode definitions (section `22` and section `23`).
- Listener orchestration internals (section `29`).
- Full `DDL` and `DML` grammar definitions (section `21`).

## Current Beta 1 package boundary
- package `03` closes the native V3 parser, direct V3 lowering, parser
  independence, and V3-only reverse-rendering substrate from this section
- emulated parser-family implementations and wire or listener closures remain
  later work-plan scope even where section `28` preserves their long-range
  canonical contracts
- remote connector, cluster-fabric, and blob-filter paths in package `03` are
  parser-envelope and UDR-readiness surfaces only; runtime parity remains in
  sections `17`, `24`, `29`, and later parser-emulation work

## Non-negotiable invariants
1. Engine never parses SQL and executes `SBLR` only.
2. Parser and listener are untrusted and cannot bypass engine enforcement.
3. Parser cannot execute data operations directly; execution only occurs in engine.
4. Persistent object references in execution payloads are UUID-based after binding.
5. Emulated parser behavior is profile-driven and does not silently fall back to native behavior.
6. Every parser family lowers its own client syntax to canonical AST and `SBLR`.
7. No cross-parser lowering dependency is allowed.
8. Parser packages are optional and independently loadable.
9. ScratchBird is always in a transaction.
10. Parser catalog synchronization is commit-bound and `MGA`-bound; there is no `WAL`-anchored parser metadata model.

## Parser targets and isolation
- `native`
- `firebird`
- `postgresql`
- `mysql`
- `mariadb`
- `sqlite`
- `cassandra`
- `mongodb`
- `neo4j`
- `redis`
- `milvus`
- `opensearch`
- `clickhouse`
- `influxdb`
- `foundationdb`
- `ignite`
- `cockroachdb`
- `citus`
- `dolt`
- `duckdb`
- `immudb`
- `tidb`
- `vitess`
- `xtdb`
- `yugabytedb`

Rules:
- a parser target may be absent without preventing any other parser target from loading
- a parser target may not import another parser target’s lowering rules as an execution dependency
- dialect parity is achieved by converging on canonical AST and `SBLR`, not by parser-to-parser delegation

## Required components per parser target
1. Ingress wire adapter.
2. Request decoder.
3. Dialect parser or command decoder.
4. Catalog sync coordinator.
5. Capability gate evaluator.
6. Canonical AST builder.
7. UUID binder.
8. `SBLR` emitter.
9. Engine request encoder.
10. Response mapper and encoder.
11. Error mapper.
12. Metrics and trace emitter.

## Parser-assist helper contract

Canonical helper names:
- `sb_catalog_resolve_name_to_uuid`
- `sb_catalog_resolve_uuid_to_path_name`
- `sb_catalog_snapshot_begin`
- `sb_catalog_delta_since_anchor`

Required rules:
1. `sb_catalog_snapshot_begin` and `sb_catalog_delta_since_anchor` return committed-baseline cache material only.
2. `sb_catalog_resolve_name_to_uuid` and `sb_catalog_resolve_uuid_to_path_name` are authoritative for current transaction-local overlay truth.
3. Bulk cache helpers reduce traffic; they do not replace point helpers when same-transaction `DDL` has occurred.
4. A parser must not treat a failed statement as cache-anchor advancement because no commit happened.
5. After a successful autocommit statement, the parser may accept a piggybacked committed delta instead of issuing an immediate follow-up delta call.

## Security and DCL lowering families

The canonical parser security family includes at least:
- `CREATE USER`
- `ALTER USER`
- `CREATE ROLE`
- `CREATE GROUP`
- `GRANT`
- `REVOKE`
- `SHOW GRANTS`
- `SET ROLE`
- `SET SESSION AUTHORIZATION`
- `CREATE POLICY`
- `ALTER POLICY`
- `DROP POLICY`
- `ALTER TABLE ... {ENABLE|DISABLE|FORCE|NO FORCE} ROW LEVEL SECURITY`
- domain `WITH SECURITY (...)` clauses

Rules:
1. security statements are lowered by the active dialect parser only
2. parser lowering may never fabricate authorization results
3. parser lowering may normalize syntax, but not weaken privilege meaning
4. security-barrier, definer, RLS, masking, and role semantics must survive lowering into canonical payloads
5. current parser support gaps are implementation drift, not license to delete canonical lowering rules

## Current parse/emit proof state

Current code-backed proof already shows:
- AST nodes for users, roles, groups, grants, revokes, policies, set-role, and session authorization
- domain-security option parsing for `MASKING`, `MASK_PATTERN`, `ENCRYPTION`, `AUDIT_ACCESS`, and `REQUIRE PRIVILEGE`
- V3 emitter lowering for policy create, alter, drop, RLS alter-table actions, grant, revoke, and show-grants
- Firebird parser-local grant and revoke parsing with its own syntax handling and its own unsupported-surface refusals

Current drift also exists:
- parser-v3 integration coverage for some DCL and RLS statement families lags the code-backed parse and emit surfaces
- some dialect-local unsupported statements remain explicit parse-time refusals

Canon rule:
- code-backed lowering capability is authoritative current behavior
- lagging tests and missing parser coverage are tracked as conformance drift

## Error mapping contract
- Parser errors and engine errors must produce dialect-native error envelopes.
- Error mapping uses explicit profile tables, never heuristic text matching.
- Every error response must include:
  - stable correlation id
  - dialect error code
  - mapped SQLSTATE or equivalent state field
  - sanitized user-visible message
  - engine-visible diagnostic text reference

## Security requirements
1. Parser never stores plaintext credentials beyond active handshake buffer lifetime.
2. Parser must zero auth payload buffers after auth exchange completion.
3. Parser must not permit local file or OS command execution through dialect command surfaces unless explicitly allowed by engine-approved procedure calls.
4. Parser must enforce maximum frame sizes and decode limits before parsing.
5. Parser teardown on disconnect must zero session and auth buffers before process termination.
