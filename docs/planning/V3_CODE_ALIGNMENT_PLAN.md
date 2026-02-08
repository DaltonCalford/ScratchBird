# V3 Code Alignment Plan (Specs → Implementation)

Status: Draft for Execution
Last Updated: 2026-02-08
Owner: ScratchBird Engineering

## Objective

Bring the ScratchBird codebase into full alignment with the authoritative V3
specifications under `docs/specifications/parser/v3/` and eliminate all
V2/legacy pipelines, ambiguities, and implementation shortcuts.

## Non‑Negotiable Principles

- The engine never parses SQL. Parsers emit SBLR only.
- V3 SBLR opcodes, payloads, validation rules, and container format are the
  sole bytecode contract.
- Catalog uses SBDB$ domains and UUID v7 identifiers.
- Firebird MGA rules govern visibility, GC, and lock ordering.

## Current State Observations (Code Evidence)

The codebase still relies heavily on V2 components:
- V2 compiler pipeline is invoked in server and executor:
  - `src/server/server_session.cpp` uses `QueryCompilerV2`
  - `src/sblr/executor.cpp` includes `parser_v2.h`, `semantic_analyzer_v2.h`,
    `bytecode_generator_v2.h` and compiles views with V2
- V2 AST and optimizer dependencies:
  - `src/optimizer/query_planner.cpp`, `src/optimizer/mv_rewriter.cpp` use
    `parser::v2::*` types
- IPC/agent path still references V2:
  - `src/ipc/external_agents/engine_ipc_session_handler.cpp`
- SBLR versioning is still v2‑oriented in executor and protocol paths

These are incompatible with the V3 specs and must be removed or rewritten.

## Workstreams and Order of Execution

### 1) V3 Parser + AST Front‑Ends (ScratchBird + Emulated Dialects)

Goal: Replace V2 parsing and AST with V3 parser/AST and direct SBLR emission.

Tasks:
- Implement V3 AST structures per `AST_TYPE_AND_LITERAL_SPEC.md`.
- Implement ScratchBird parser using V3 grammar:
  - `parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md`
  - `parser/ScratchBird Master Grammar Specification v2.0.md`
- Implement emulated parsers (PostgreSQL/MySQL/Firebird) as separate front‑ends
  that emit V3 SBLR directly.
- Remove all V2 parser usage in `server_session.cpp` and IPC handlers.

Exit criteria:
- No `parser_v2` includes remain under `src/`.
- All SQL paths emit V3 bytecode containers.

### 2) V3 SBLR Container + Constant Pools + Validation

Goal: Implement V3 bytecode container, constant pool, symbol table, canonical
encoder/decoder, and verifier exactly as specified.

Tasks:
- Implement container format from `SBLR_V3_BYTECODE_CONTAINER.md`.
- Implement constant pools and symbol tables per
  `SBLR_V3_CONSTANT_POOL_AND_SYMBOLS.md`.
- Implement canonicalization rules per
  `SBLR_V3_BYTECODE_CANONICALIZATION.md`.
- Implement verifier per `SBLR_V3_VALIDATION_RULES.md`.

Exit criteria:
- Bytecode validator accepts all V3 test vectors and examples.
- V2 SBLR headers and opcodes are fully removed.

### 3) V3 SBLR Opcode Payloads + Executor Semantics

Goal: Implement every opcode’s payload schema and execution semantics.

Tasks:
- Implement opcode definitions per `SBLR_V3_OPCODE_SPEC.md`.
- Implement payload serialization per `SBLR_V3_OPCODE_PAYLOADS.md`.
- Implement runtime semantics per `SBLR_V3_OPCODE_SEMANTICS.md`.
- Ensure lock ordering and constraint enforcement per
  `EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`.

Exit criteria:
- Executor runs V3 opcode test vectors and passes validation.
- No execution path calls V2 semantic analyzer or bytecode generator.

### 4) Storage + Catalog Alignment (SBDB$ Domains, UUID v7)

Goal: Align storage and catalog to V3 specs and replace any raw types or legacy
OID usage with SBDB$ domains.

Tasks:
- Implement page layouts per `storage/PAGE_TYPES_AND_LAYOUTS.md`.
- Implement VALUE_SPEC encodings per `types/VALUE_SPEC_STORAGE_ENCODINGS.md`.
- Implement catalog tables using SBDB$ domains per
  `catalog/SYSTEM_CATALOG_DOMAIN_MAP.md`.
- Enforce UUID v7 lifecycle rules per `catalog/UUID_LIFECYCLE_RULES.md`.

Exit criteria:
- All catalog columns use SBDB$ domains.
- UUID v7 enforced for all identifiers.

### 5) Transaction, MGA, GC, Locking

Goal: Align transaction subsystem with V3 MGA and lock rules.

Tasks:
- Implement MGA core rules per `transaction/TRANSACTION_MGA_CORE.md`.
- Implement lock manager per `transaction/TRANSACTION_LOCK_MANAGER.md`.
- Implement transaction lifecycle per `transaction/TRANSACTION_MAIN.md`.
- Ensure executor uses lock ordering and GC rules from
  `EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`.

Exit criteria:
- All DML/DLL paths honor MGA visibility and lock ordering.

### 6) Index Layer Alignment

Goal: Implement index contracts and ensure index ops integrate with MGA and
page layout authority.

Tasks:
- Implement shared index rules per
  `indexes/INDEX_IMPLEMENTATION_SPEC.md` and `INDEX_GC_PROTOCOL.md`.
- For each core index, implement algorithm from its spec (BTREE, HASH, GIN,
  GIST, SPGIST, BRIN, BITMAP, RTREE, HNSW, COLUMNSTORE, LSM, IVF, FULLTEXT,
  ZONEMAP, ZORDER, GEOHASH, etc).
- Use `indexes/INDEX_IMPLEMENTATION_REFERENCE.md` as normative link map.

Exit criteria:
- Index operations use V3 page types, MGA visibility, and GC protocol.

### 7) Network/IPC and Protocol Compliance

Goal: Align listeners, parsers, and engine IPC with V3 network specs.

Tasks:
- Implement IPC contract per `network/ENGINE_PARSER_IPC_CONTRACT.md`.
- Implement listener/pool lifecycle per
  `network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md`.
- Implement protocol baselines per `network/WIRE_PROTOCOL_SPECIFICATIONS.md`.

Exit criteria:
- Network endpoints authenticate via engine, parsers remain untrusted.

### 8) Tooling, Build, Testing

Goal: Ensure deterministic build/test CLI and conformance suite match V3.

Tasks:
- Implement `tools/SB_BUILD_AND_TEST_CLI_SPEC.md`.
- Add conformance tests per `testing/DIALECT_CONFORMANCE_ASSERTIONS.md`.
- Add SBLR bytecode tests per `sblr/` test vectors.

Exit criteria:
- Full CI run uses SB build/test CLI and V3 test vectors.

## Proposed Implementation Order (Top‑Down)

1. V3 SBLR container + opcode registry + verifier
2. V3 parser/AST for ScratchBird + minimal DML/DDL subset
3. Executor core support for DML/DDL + transaction/MGA
4. Catalog + storage alignment (UUID v7 + SBDB$ domains)
5. Full SBLR opcode payload coverage
6. Emulated parsers (PG/MySQL/Firebird) + wire protocols
7. Index implementation per core specs
8. Full tooling + conformance suites

## Acceptance Criteria

- No V2 parser or SBLR references in `src/`.
- All V3 bytecode examples and test vectors validate and execute.
- All DDL/DML/PSQL statements have deterministic SBLR emission.
- Catalog schema uses SBDB$ domains and UUID v7 identifiers everywhere.
- Lock ordering and MGA visibility match V3 specs.

## Immediate Next Actions

- Remove the V2 compiler pipeline from `src/server/server_session.cpp` and
  replace with V3 SBLR execution path.
- Stand up V3 SBLR validator and container serializer as the first executable
  deliverable.
- Create a tracker checklist per subsystem aligned to the workstreams above.
