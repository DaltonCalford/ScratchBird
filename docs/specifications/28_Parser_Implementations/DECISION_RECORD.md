# Decision Record - 28_Parser_Implementations

## Scope
- Parser architecture and runtime behavior for native and emulated interfaces.
- Parser-to-engine boundary, including IPC request requirements.
- Dialect compatibility, feature gating, and error mapping.
- Deterministic SQL or command surface to SBLR translation.

## Non-Negotiable Invariants
1. The engine is not a SQL parser and executes SBLR and internal procedures only.
2. Parser and listener processes are untrusted; engine is the sole authority for auth, authorization, validation, and execution.
3. Internal object identity is UUID-based only; names are parser-layer concerns.
4. Emulated engines use dedicated parser implementations and do not share grammar with the native parser.
5. Emulated parsers never fall back to native parsing. Disabled dialects or disabled features are rejected explicitly.
6. Alpha transaction/recovery semantics remain MGA-based; parser specs must not introduce WAL-based core behavior.

## Decisions

### D1 - Dedicated Parser Set
- Required parser surfaces:
  - `native` (ScratchBird SQL)
  - `firebird`
  - `postgresql`
  - `mysql`
  - `cassandra`
  - `mongodb`
  - `neo4j`
  - `redis`
  - `milvus`
- Rationale: 1:1 emulation requires per-dialect grammar, wire protocol, and error behavior without contradictory cross-dialect compromises.

### D2 - Fixed Translation Pipeline
- Every parser request follows one canonical pipeline:
  - ingress decode
  - dialect parse or command decode
  - capability gate (implement/remap/reject)
  - UUID binding
  - SBLR emission
  - engine execution request
  - dialect response mapping
- Rationale: deterministic behavior for low-capability implementation agents.

### D3 - Data-Driven Capability and Reserved Word Profiles
- Dialect feature support, reserved words, and compatibility version pins are stored as database configuration data.
- Parser binaries load and cache profiles; they do not hardcode support matrices.
- Rationale: allows profile updates without parser code rewrites.

### D4 - Mandatory Traceability Artifacts
- For each executable request, parser must transmit:
  - original client text or command payload
  - normalized engine-visible SQL text (for SQL dialects)
  - SBLR payload
  - source map from client input to canonical form
  - capability gate decision record
- Rationale: required for diagnostics, audit, and deterministic error reporting.

### D5 - Explicit Error Mapping Tables
- Each dialect parser maintains explicit mappings from ScratchBird engine error domains to dialect-visible error envelopes.
- Mapping tables are versioned and tested.
- Rationale: removes ambiguity and avoids ad-hoc error translation.

### D6 - Session Surface Isolation
- Session variables, reserved words, and identifier rules are surfaced per dialect profile.
- Native parser exposes native behavior plus compatibility aliases declared in profile data.
- Emulated parsers expose only profile-supported behavior.
- Rationale: preserve compatibility while keeping engine semantics unified.

### D7 - Parserless Cluster Fabric Data Plane
- Parser implementations handle only native cluster-fabric control SQL.
- ScratchBird-to-ScratchBird fabric data-plane operations are UDR protocol operations and are not parser execution paths.
- Rationale: preserve architecture boundary and enable server-to-server operations without parser dependency.

### D8 - Fresh Parser Worker Per Client Connection
- Parser workers assigned to client connections are single-connection lifecycle workers.
- On disconnect, assigned worker is terminated and replaced by warm-pool replenishment.
- Rationale: guarantees fresh parser memory state per new client connection.

### D9 - Mandatory Pre-Auth Database Availability Gate
- Parser must run server pre-auth availability check before dialect auth success.
- If server has no open ScratchBird database, parser must fail authentication automatically.
- Rationale: prevents IP parser access before database bootstrap and avoids undefined backend state.

### D10 - Emulated Create Database Policy
- Emulated parser `CREATE DATABASE` operations are logical emulation bootstrap operations only.
- Emulated parser path must not create physical ScratchBird database files.
- Physical database create is allowed by server-native create flows, embedded-local create flows, and bridge-authorized cluster flows.
- Rationale: preserve architecture boundary and bootstrap safety.

### D11 - Split Emulated Package Model
- Every emulated engine family is composed of:
  - one client-facing parser package
  - one engine-facing support UDR package
- Core engine carries no family-specific compiled emulation logic.
- Rationale: reduce engine bloat while preserving deterministic 1:1 emulation.

### D12 - Engine-Facing Dynamic Translation Is UDR-Owned
- When engine-owned emulation routines need SQL or command text translated to
  SBLR, the matching support UDR package performs the translation.
- Parser and support UDR must use the same family profile and produce the same
  canonical translation artifacts for identical inputs.
- Rationale: preserve the engine no-parser invariant even for internal
  emulation helpers.

## Alternatives Considered and Rejected
- Single shared parser for all dialects:
  - Rejected due to contradictory dialect semantics and poor compatibility outcomes.
- Parser-side authorization or execution:
  - Rejected due to trust boundary violations.
- Implicit fallback from emulated parser to native parser:
  - Rejected; it breaks 1:1 emulation guarantees and hides compatibility gaps.

## Consequences
- Additional implementation volume across parser adapters and mapping tables.
- Lower ambiguity for implementation and testing.
- Stronger safety boundary between client-facing dialect logic and engine internals.

## Open Questions
- None.

## References
- `docs/specifications/28_Parser_Implementations/PARSER_IMPLEMENTATION_CANONICAL_SPEC.md`
- `docs/specifications/28_Parser_Implementations/DIALECT_PROFILE_MATRIX.md`
- `docs/specifications/28_Parser_Implementations/SBLR_TRANSLATION_PIPELINE.md`
- `docs/specifications/28_Parser_Implementations/ERROR_MAPPING_AND_DIAGNOSTICS.md`

## Audit normalization note (2026-03-28)
- Current code-backed parser authority is bounded to the native V3 stack (`parser_v3`, `lexer_v3`, `ast_v3`, `v3_emitter`) plus dedicated shipped emulated SQL-family parser code for Firebird, PostgreSQL, and MySQL.
- Dedicated parser-agent and listener proof currently exists only for `sb_parser_fb`, `sb_parser_pg`, `sb_parser_mysql`, and the matching listener front doors; universal nine-family dedicated parser parity is not current implementation proof.
- Builtin emulation package scaffold proof is currently limited to `firebirdsql`, `postgresql`, and `mysql`.
- Cassandra, MongoDB, Neo4j, Redis, and Milvus are currently represented in this section by native-V3 feature vocabulary, catalog/runtime vocabulary, or checklist material rather than shipped dedicated parser implementations.
- Broad section-wide parity, corpus cardinality, and universal profile-generation claims are therefore bounded and must not be treated as present-day implementation proof without family-local source evidence.
