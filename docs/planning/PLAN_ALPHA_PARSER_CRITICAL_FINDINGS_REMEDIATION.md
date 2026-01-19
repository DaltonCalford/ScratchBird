# Plan: Alpha Parser Critical Findings Remediation

Source: `docs/audit/parsers/CRITICAL_FINDINGS.md` + `docs/audit/languages/*`

Goal: Close all parser critical findings so Alpha can proceed to network listener work.

Scope: V2 parser completeness, temporary table semantics, index type coverage, and any remaining
PostgreSQL/MySQL bytecode alignment gaps. Firebird parser stays dialect-pure; only fill missing
parser->bytecode or executor coverage if gaps are confirmed.

Non-scope (Alpha): Cluster routing/sharding (tracked separately).

## Decisions Needed (before implementation)
1) **V2 PostgreSQL-aligned syntax (DECIDED):** keep the current PG-style syntax in V2 (no replacement).
2) **Temporary table model (DECIDED):** implement GTTT, GTTS, UTTT, UTTS using ScratchBird MGA
   semantics (commit behaves like rollback for temp table changes).
3) **CTE scope (DECIDED):** include RECURSIVE in Alpha.
4) **PSQL scope (DECIDED):** full AST coverage in `ast_v2.h`.
5) **Parsed features policy (DECIDED):** any parsed feature must be implemented end-to-end;
   no parse-only features.

## Phase 0 - Verification / Current-State Audit (must be done first)
- Reconcile `CRITICAL_FINDINGS.md` against current code:
  - Confirm which PostgreSQL/MySQL bytecode mismatches remain.
  - Confirm V2 CTE parsing status (WITH clause in INSERT/UPDATE/DELETE).
  - Confirm PSQL parser coverage in V2 (create/execute block/flow control).
- Produce full list of PostgreSQL-specific syntax currently accepted by V2 (for documentation).
- Update the audit notes under `docs/audit/parsers/CRITICAL_FINDINGS.md` if any items are already
  resolved by recent work.
- Add a short delta summary to `docs/audit/languages/README.md` explaining what changed since audit.

Deliverable: "Verified gaps list" that is the source of truth for remaining work.

Progress:
- ✅ Phase 0 verification completed (2026-01-14).
- ✅ Verified gaps list captured in `docs/audit/parsers/CRITICAL_FINDINGS.md` (Phase 0 section).

## Phase 1 - Temporary Tables (CRITICAL)
### 1.1 AST / Parser
- Extend AST to capture four temp types + ON COMMIT semantics:
  - **GTTT** (global temporary, transaction-locked) = visible only to the creating transaction.
  - **GTTS** (global temporary, session-locked) = visible only to the owning session.
  - **UTTT** (user temporary, transaction-locked) = like GTTT but created under user temp schema.
  - **UTTS** (user temporary, session-locked) = like GTTS but created under user temp schema.
  - Firebird: GLOBAL TEMPORARY + ON COMMIT DELETE ROWS → GTTT; PRESERVE ROWS → GTTS.
  - PostgreSQL/MySQL/V2 TEMP/TEMPORARY → UTTS by default.
  - ON COMMIT DELETE ROWS → UTTT; ON COMMIT PRESERVE ROWS → UTTS.
- Ensure parser fills these fields (no discard).

Progress:
- ✅ V2 parser captures TEMP/TEMPORARY + ON COMMIT and maps to temp_type/on_commit.
- ✅ Firebird parser captures GLOBAL TEMPORARY + ON COMMIT and maps to temp_type/on_commit.
- ✅ PostgreSQL/MySQL parsers now emit temp flags; PG ON COMMIT handled only for TEMP tables.

### 1.2 Bytecode + Executor
- Extend CREATE TABLE bytecode payload to include temp type + ON COMMIT action.
- Update executor to:
  - Enforce visibility isolation by transaction/session (MGA-based filtering).
  - Treat commits as **rollback** for temp table data (per type semantics).
  - Clean up temp rows on transaction end (GTTT/UTTT) or session end (GTTS/UTTS).

Progress:
- ✅ CREATE TABLE bytecode now includes temp/on-commit flags; executor maps to catalog metadata.
- ✅ Session-scoped tuple tagging + visibility filtering for temp data paths.
- ✅ ON COMMIT DELETE/DROP applied at commit time for temp tables.
- ✅ Session-end cleanup + schema placement complete.

### 1.3 Catalog + Session Tracking
- Catalog: record temp table metadata (type, owner session/tx, on-commit action).
- ConnectionContext: track temp tables owned by the session (for cleanup).
- Schema placement:
  - UTT*: `user.<name>.temp` if personal schema exists, else `public.temp`.
  - GTT*: global schema (dialect-specific as per emulation).

Progress:
- ✅ Catalog records temp metadata + session ownership.
- ✅ Temp schema placement + session-end cleanup wiring.

### 1.4 Tests
- Add parser tests per dialect: TEMP parsing + ON COMMIT semantics.
- Add executor tests: visibility isolation, ON COMMIT DELETE/PRESERVE, cleanup on disconnect.

Progress:
- ✅ Parser tests updated for TEMP/GTT coverage.
- ✅ Executor lifecycle tests for temp schema placement, ON COMMIT, and session cleanup.

Acceptance: TEMP tables do not leak across sessions, and ON COMMIT matches dialect rules.

## Phase 2 - V2 PSQL + CTE Parsing
### 2.1 PSQL (V2 parser)
- Implement full AST coverage:
  - CREATE FUNCTION / PROCEDURE / TRIGGER
  - EXECUTE BLOCK
  - IF / WHILE / FOR SELECT / exception handling
  - Any remaining AST nodes in `ast_v2.h` that are currently unparsed

### 2.2 CTE (WITH clause)
- Implement WITH clause parsing for SELECT/INSERT/UPDATE/DELETE.
- Include RECURSIVE support.
- Wire AST fields to semantic analyzer and bytecode generator.

### 2.3 Tests
- Parser tests for CREATE FUNCTION/PROCEDURE/TRIGGER and PSQL blocks.
- CTE parse tests for DML (WITH + INSERT/UPDATE/DELETE).

Progress:
- ✅ V2 parser covers CREATE FUNCTION/PROCEDURE/TRIGGER and core PSQL control flow/DDL statements.
- ✅ EXECUTE BLOCK/PROCEDURE/STATEMENT parsing wired in V2.
- ✅ WITH clause parsing for SELECT/INSERT/UPDATE/DELETE including RECURSIVE.
- ✅ AST → semantic analyzer → bytecode path for CTEs (EXT_WITH_CLAUSE / EXT_CTE_DEF).
- ✅ Added parser tests for PSQL constructs and DML CTEs.

Acceptance: V2 can parse core PSQL and CTEs with stable AST output.

## Phase 3 - V2 Index Type Completeness (11 types)
### 3.1 AST + Parser
- Extend V2 AST enum to include: SPGIST, RTREE, HNSW, BITMAP, COLUMNSTORE, LSM.
- Parse `USING <type>` for all 11 types.

### 3.2 Semantic + Bytecode
- Map all index types to correct storage engine identifiers.
- Remove dead code in semantic analyzer.
- Emit correct SBLR opcodes for all types.

### 3.3 Tests
- Parser tests for each index type.
- Integration tests: CREATE INDEX + basic insert/select for each type.

Acceptance: V2 supports all index types already present in storage engine/opcodes.

Progress:
- ✅ V2 AST + parser accept SPGIST/RTREE/HNSW/BITMAP/COLUMNSTORE/LSM.
- ✅ Semantic analyzer + bytecode generator map all 11 index types.
- ✅ Added parser and bytecode-generation tests for additional index types.
- ✅ Integration tests cover CREATE/INSERT/SELECT per index type (btree/hash/gin/gist/spgist/brin/rtree/hnsw/bitmap/columnstore/lsm).

## Phase 4 - PostgreSQL/MySQL Bytecode Alignment (if any gaps remain)
- Re-audit CREATE TABLE / CREATE INDEX / CREATE VIEW payload formats.
- Align parser output to executor format (or version bytecode if needed).
- Validate SELECT/INSERT/UPDATE/DELETE payloads and MERGE if used.

Acceptance: dialect parser bytecode executes without format errors for core DDL/DML.

Progress:
- ✅ PostgreSQL CREATE TABLE column count now uses uvarint (BEGIN_LIST alignment).
- ✅ MySQL TABLE_REF payload includes ref_kind + alias; FROM list count uses uvarint.

## Phase 5 - Parsed-but-Not-Implemented Features (cleanup)
- Implement all parsed features end-to-end (no silent or partial support).
- If a feature is removed/replaced in V2 syntax, remove parsing for the old syntax.
- Update dialect docs to state final supported syntax.

Progress:
- ✅ V2 CREATE SEQUENCE wired through semantic + bytecode + executor.
- ✅ TEMP VIEW/SEQUENCE now session-scoped temp metadata (non-persistent in Alpha).
- ✅ UNLOGGED table warnings emitted at execution time.
- ✅ ALTER TABLE ADD/DROP CONSTRAINT (PK/UNIQUE/FK/CHECK) wired through bytecode + executor.
- ✅ V2 CREATE TABLE emits column PK/UNIQUE + table-level PK/UNIQUE/CHECK with enforcement.
- ✅ COPY FORMAT/ENCODING accepted end-to-end (UTF8/UTF-8 only; BINARY unsupported in Alpha).

## Phase 6 - Documentation + Final Verification
- Update `docs/audit/parsers/CRITICAL_FINDINGS.md` to mark all items resolved.
- Update `docs/IMPLEMENTATION_STATUS_DASHBOARD.md` with new status + test counts.
- Run full build + full test suite (excluding TCP/Unix socket gated tests).

Acceptance: Full ctest run passes (except gated network tests); audit doc states all critical parser
findings resolved.

Progress:
- ✅ Updated `docs/audit/parsers/CRITICAL_FINDINGS.md`.
- ✅ Updated `docs/IMPLEMENTATION_STATUS_DASHBOARD.md`.
- ✅ Full build + full test suite pass (2026-01-18; gated network tests skipped).

## Suggested Execution Order
1) Phase 0 (verification)  
2) Phase 1 (temporary tables)  
3) Phase 2 (PSQL + CTE)  
4) Phase 3 (index types)  
5) Phase 4 (PG/MySQL alignment)  
6) Phase 5 (implement remaining parsed features)  
7) Phase 6 (docs + full test pass)
