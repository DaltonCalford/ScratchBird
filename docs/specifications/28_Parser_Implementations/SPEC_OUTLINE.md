# Spec Outline - 28_Parser_Implementations

## Purpose
Define complete parser implementation requirements so native and emulated dialect surfaces can be implemented deterministically and mapped to shared SBLR without embedding SQL logic in the engine.

## Required Deliverables in This Section
- `PARSER_IMPLEMENTATION_CANONICAL_SPEC.md`
- `DIALECT_PROFILE_MATRIX.md`
- `CAPABILITY_PROFILE_BUILD_ALGORITHM.md`
- `CAPABILITY_PROFILE_DECISION_TABLE.csv`
- `CAPABILITY_PROFILE_ROW_SERIALIZATION_EXAMPLES.md`
- `EMULATED_ENGINE_PACKAGE_MODEL.md`
- `SBLR_TRANSLATION_PIPELINE.md`
- `NORMATIVE_WIRE_PROTOCOL_FIREBIRD_CHECKLIST.md`
- `NORMATIVE_WIRE_PROTOCOL_POSTGRESQL_CHECKLIST.md`
- `NORMATIVE_WIRE_PROTOCOL_MYSQL_CHECKLIST.md`
- `NORMATIVE_WIRE_PROTOCOL_CASSANDRA_CHECKLIST.md`
- `NORMATIVE_WIRE_PROTOCOL_MONGODB_CHECKLIST.md`
- `NORMATIVE_WIRE_PROTOCOL_NEO4J_BOLT_CHECKLIST.md`
- `NORMATIVE_WIRE_PROTOCOL_REDIS_RESP_CHECKLIST.md`
- `NORMATIVE_WIRE_PROTOCOL_MILVUS_GRPC_CHECKLIST.md`
- `NORMATIVE_EMULATED_PARSER_LAYER_BASELINE.md`
- `NORMATIVE_EMULATED_PARSER_LAYER_FIREBIRD.md`
- `NORMATIVE_EMULATED_PARSER_LAYER_POSTGRESQL.md`
- `NORMATIVE_EMULATED_PARSER_LAYER_MYSQL.md`
- `NORMATIVE_EMULATED_PARSER_LAYER_CASSANDRA.md`
- `NORMATIVE_EMULATED_PARSER_LAYER_MONGODB.md`
- `NORMATIVE_EMULATED_PARSER_LAYER_NEO4J.md`
- `NORMATIVE_EMULATED_PARSER_LAYER_REDIS.md`
- `NORMATIVE_EMULATED_PARSER_LAYER_MILVUS.md`
- `NORMATIVE_PARSER_QUERY_TO_SBLR_CHECKLIST.md`
- `NORMATIVE_PARSER_CLUSTER_FABRIC_CONTROL_CHECKLIST.md`
- `NORMATIVE_P0_PARSER_NORMALIZATION_GATE_CHECKLIST.md`
- `NORMATIVE_P1_PARSER_DISTRIBUTED_POLICY_AND_TELEMETRY_CHECKLIST.md`
- `NORMATIVE_P2_PARSER_PLAN_STABILITY_AND_HINTS_CHECKLIST.md`
- `K_SUITE_FIXTURE_AND_EVIDENCE_MANIFEST.md`
- `ERROR_MAPPING_AND_DIAGNOSTICS.md`
- `TEST_CONTRACT.md`
- `LEGACY_V3_GAP_MIGRATION_MAP.md`

## Parser Targets
- Native ScratchBird parser.
- Emulated parsers:
  - Firebird
  - PostgreSQL
  - MySQL
  - Cassandra
  - MongoDB
  - Neo4j
  - Redis
  - Milvus

## Current Beta 1 package boundary
- package `03` closes the native ScratchBird parser plus parser-isolation and
  SBLR-to-V3 reconstruction rules from this section
- emulated parser-family implementations, wire adapters, and listener-runtime
  obligations remain later work-plan deliverables
- remote connector, cluster-fabric, and blob-filter items are consumed here as
  parser-front-door or checklist surfaces only; runtime parity stays bounded to
  their owning sections and later parser-emulation work

## Architecture Outline
1. Trust boundary:
   - listener and parser are untrusted
   - engine is authoritative for auth, authorization, SBLR validation, and execution
2. Dedicated parser model:
   - one dedicated parser implementation per emulated engine
   - no fallback from emulated parser to native parser
3. Runtime model:
   - parser worker handles one active client connection at a time
   - assigned parser worker is terminated on disconnect and not reused for next client
   - listener uses warm pool replenishment to provide fresh parser workers
   - enabled emulated families also require matching support UDR packages in
     `READY` state
4. Translation model:
   - parse or decode client input
   - canonicalize and capability-gate
   - resolve identifiers to UUID
   - emit SBLR with UUID references only
   - map engine response to dialect wire protocol and error envelope
   - engine-origin dynamic translation for emulated families is handled by
     support UDR packages, not by core engine code

## Data and Configuration Outline
- Parser session context:
  - connection identity
  - session identity
  - transaction identity
  - role or group context
  - dialect profile id and version
  - language and naming policy
- Dialect profile data:
  - reserved words
  - feature flags
  - remap rules
  - unsupported statements and options
  - error mapping tables
- Capability state:
  - `IMPLEMENT`
  - `REMAP`
  - `REJECT`
- Parser pre-auth database availability gate:
  - server-side precheck required before dialect auth success
  - if no open ScratchBird database, parser auth fails deterministically
- Emulated package activation state:
  - absent
  - parser_only
  - support_only
  - installed_disabled
  - enabled
  - degraded

## Required Algorithms
- Deterministic request pipeline from ingress to egress.
- Capability gate decision procedure with fixed precedence.
- SQL or command to SBLR conversion using canonical AST structures.
- SBLR to engine-visible SQL rendering for diagnostics.
- Engine error to dialect error mapping with explicit table lookup order.

## Security and Correctness Outline
- Parser cannot bypass engine auth or privilege enforcement.
- Parser must fail auth when server reports no open database for parser service.
- Parser never executes SQL locally.
- Parser must include original client request and canonical trace metadata in execute requests.
- Session variables and reserved words are surfaced per dialect profile.

## Test and Gate Outline
- Architecture boundary tests.
- Dialect compatibility tests per target parser.
- Capability gate tests (`IMPLEMENT`, `REMAP`, `REJECT`).
- SQL or command traceability tests (client text <-> engine-visible text <-> SBLR).
- Parser-package and support-UDR translation parity tests.
- Error mapping tests by dialect.
- Performance and reliability gates for parser pools and translation latency.

## Legacy Coverage Policy
- Legacy parser-v3 material is mined for behavior requirements only.
- Canonical documents in this section replace legacy structure and wording.
- Every high-value legacy requirement must map to one canonical subsection.

## Open Questions
- None.

## Audit normalization note (2026-03-28)
- Current code-backed parser authority is bounded to the native V3 stack (`parser_v3`, `lexer_v3`, `ast_v3`, `v3_emitter`) plus dedicated shipped emulated SQL-family parser code for Firebird, PostgreSQL, and MySQL.
- Dedicated parser-agent and listener proof currently exists only for `sb_parser_fb`, `sb_parser_pg`, `sb_parser_mysql`, and the matching listener front doors; universal nine-family dedicated parser parity is not current implementation proof.
- Builtin emulation package scaffold proof is currently limited to `firebirdsql`, `postgresql`, and `mysql`.
- Cassandra, MongoDB, Neo4j, Redis, and Milvus are currently represented in this section by native-V3 feature vocabulary, catalog/runtime vocabulary, or checklist material rather than shipped dedicated parser implementations.
- Broad section-wide parity, corpus cardinality, and universal profile-generation claims are therefore bounded and must not be treated as present-day implementation proof without family-local source evidence.
- package `03` uses this normalization to execute only the native-V3 subset;
  later packages are responsible for converting bounded emulated-family material
  into implementation-complete work

## Hardening promotion note (2026-03-28)
- section `28` now carries explicit capability-state vocabulary for parser implementation proof lanes:
  - `supported_native_v3`
  - `supported_emulated_sql_family`
  - `supported_scaffold_or_udr_boundary`
  - `bounded_shipped_front_door`
  - `checklist_only`
  - `target_state_only`
  - `fail_closed`
- dedicated parser-family proof must be anchored to live parser code plus shipped parser-agent or listener/runtime seams, not to checklist presence alone
- native-V3 internal feature vocabulary must not be promoted into dedicated external parser-family parity without family-local source proof
- universal capability-profile generation, universal corpus cardinality, and universal wire parity claims remain non-authoritative unless backed by generated or runtime evidence
