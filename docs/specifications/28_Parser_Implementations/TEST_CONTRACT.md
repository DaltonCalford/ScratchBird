# Test Contract - 28_Parser_Implementations

## Purpose
Define mandatory verification for parser correctness, compatibility, safety boundaries, and deterministic behavior.

## Current Beta 1 package boundary
- package `03` executes only the native-V3 parser, parser-isolation, and V3
  reverse-render substrate from this section
- `B-001`, the native-facing rows in suites `C` through `L`, and the retained-
  symbol or render checks required by sections `22` and `23` are current
  package obligations
- `B-002` through `B-010`, suites `M` through `T`, and any wire or
  listener-parity gates for emulated families remain later-package obligations
- remote connector, cluster-fabric, and blob-filter surfaces in this package
  are parser-front-door and fail-closed boundary checks only; they do not imply
  live runtime parity

## Test Suites

### Suite A: Architecture and Trust Boundary
- `A-001`: parser cannot execute statements locally.
- `A-002`: engine rejects execute request without required metadata.
- `A-003`: parser cannot bypass engine auth or privilege checks.
- `A-004`: parser crash does not crash engine process.

### Suite B: Dedicated Parser Surface Coverage
- `B-001`: native parser endpoint accepts native syntax only.
- `B-002`: firebird parser accepts firebird protocol and dialect.
- `B-003`: postgresql parser accepts PostgreSQL protocol and dialect.
- `B-004`: mysql parser accepts MySQL protocol and dialect.
- `B-005`: cassandra parser accepts CQL protocol and command surface.
- `B-006`: mongodb parser accepts MongoDB command protocol.
- `B-007`: neo4j parser accepts Bolt and Cypher surface.
- `B-008`: redis parser accepts RESP command surface.
- `B-009`: milvus parser accepts configured Milvus API surface.
- `B-010`: no emulated parser falls back to native parser.
- `B-011`: sqlserver parser accepts TDS protocol and T-SQL surface.
- `B-012`: db2 parser accepts DRDA/DDM protocol and Db2 SQL surface.

### Suite C: Capability Gate Behavior
- `C-001`: disabled dialect returns explicit dialect-disabled error.
- `C-002`: disabled feature returns explicit feature-disabled error.
- `C-003`: unsupported clause returns deterministic unsupported error.
- `C-004`: remap path emits expected transform id and output canonical AST.
- `C-005`: missing capability profile entry is rejected deterministically.
- `C-006`: capability profile build produces the required cross-product cardinality for the active target set (`target_count * feature_count`); package `03` uses the native target set, while universal nine-family cardinality remains later-package.
- `C-007`: required-engines token normalization rejects unknown tokens deterministically.
- `C-008`: normalized required-target expansion matches canonical build rules in `CAPABILITY_PROFILE_BUILD_ALGORITHM.md`.
- `C-009`: generated rows follow canonical field order and checksum formula in `CAPABILITY_PROFILE_ROW_SERIALIZATION_EXAMPLES.md`.
- `C-010`: representative native sample rows, and deferred-family sample rows when their package is active, match expected decision, transform, reject-code, and precedence values.
- `C-011`: `CAPABILITY_PROFILE_DECISION_TABLE.csv` row count equals `feature_count + 1` (header plus one row per feature key).
- `C-012`: every section-21 feature key exists exactly once in `CAPABILITY_PROFILE_DECISION_TABLE.csv`.
- `C-013`: no extra feature key exists in `CAPABILITY_PROFILE_DECISION_TABLE.csv` that is absent from section-21 matrix.

### Suite D: Translation Determinism
- `D-001`: same request + same profile version -> byte-identical SBLR.
- `D-002`: UUID binding output contains no raw object names.
- `D-003`: canonicalization preserves equivalent semantics across dialect spellings.
- `D-004`: source map contains stable mappings for token spans.
- `D-005`: engine-visible SQL or command rendering is stable.

### Suite E: Error Mapping and Diagnostics
- `E-001`: parser syntax error maps to dialect-native error envelope.
- `E-002`: engine privilege error maps to dialect-native access error.
- `E-003`: engine object-not-found maps correctly per dialect.
- `E-004`: engine type-cast error maps correctly per dialect.
- `E-005`: diagnostics include original input, engine-visible form, and correlation id.
- `E-006`: unmapped engine error falls back to deterministic generic mapping.
- `E-007`: parser mapping selects rows by `error_ref_uuid`, not by engine text.
- `E-008`: every enabled donor parser has a complete donor map pack for the
  admitted registry snapshot.
- `E-009`: native and donor render packs preserve typed detail-slot rendering
  deterministically.
- `E-010`: uncataloged internal failures never expose raw legacy engine text to
  the client.

### Suite F: Session and Naming Surface
- `F-001`: session variables exposed per dialect profile only.
- `F-002`: hidden variables are not visible through emulated parser metadata.
- `F-003`: reserved word set used by parser matches profile version.
- `F-004`: identifier case and quoting rules follow dialect profile.
- `F-005`: name rendering uses session language with fallback to default name.

### Suite G: Wire and IPC Contract
- `G-001`: parser-engine IPC handshake validates feature negotiation fields.
- `G-002`: execute request includes original input and SBLR payload.
- `G-003`: prepared statement lifecycle messages map correctly to engine requests.
- `G-004`: copy and streaming flows enforce backpressure signaling.
- `G-005`: cancel request interrupts active execution context.
- `G-006`: notification subscribe and unsubscribe path maps correctly.

### Suite H: Conformance and Regression
- `H-001`: per-dialect conformance corpus passes for enabled profile version.
- `H-002`: parser output remains stable across patch releases for same profile version.
- `H-003`: legacy behavior deltas are explicitly declared and tested.
- `H-004`: mixed parser pool load does not cross-contaminate session state.

### Suite I: Performance and Reliability
- `I-001`: parser translation p95 latency remains under configured gate.
- `I-002`: parser pool warm and recycle policies match configured thresholds.
- `I-003`: sustained load with cancellation and streaming keeps error rate under configured gate.
- `I-004`: memory usage per parser worker stays within configured hard limit.

### Suite J: Infrastructure SQL Surface
- `J-001`: native `CONFIG HISTORY` and `RELOAD CONFIG` map to deterministic feature keys and result shapes.
- `J-002`: native `SHOW <object_kind> SYSTEM` enforces privilege and deterministic visibility filtering.
- `J-003`: domain variant forms (`AS RECORD|ENUM|SET OF|RANGE OF`) parse to deterministic AST variants.
- `J-004`: full index management surface maps to canonical index feature keys with deterministic option parsing.
- `J-005`: text-search dictionary/configuration SQL maps to deterministic feature keys and rejects invalid mappings.
- `J-006`: cluster routing/admission SQL maps to cluster control family with deterministic parse and binding.
- `J-007`: alerting/healing SQL maps to deterministic control-plane feature keys.
- `J-008`: job type and job control SQL enforce parameter validation and deterministic error mapping.
- `J-009`: sharding and cube SQL surfaces map deterministically and reject unsupported dialect exposure.
- `J-010`: encryption and certificate SQL enforces private-key non-readability and deterministic errors.
- `J-011`: listener lifecycle/config/pool SQL (`START|STOP|RELOAD|SHOW LISTENER`, `ALTER LISTENER`, `ALTER LISTENER POOL`, `DRAIN|UNDRAIN`) maps to deterministic native-only feature keys with strict selector and key validation.

### Suite K: Normative Parser Checklist Gates
- `K-001`: P00 context freeze enforces active transaction and immutable per-request context.
- `K-002`: P01 ingress decode rejects malformed wire frames deterministically.
- `K-003`: P02 parse/decode stage emits stable source-span mappings.
- `K-004`: P03 capability gate enforces precedence and rejects missing profile rows.
- `K-005`: P04 canonicalization removes dialect-specific semantics while preserving source-map traceability.
- `K-006`: P05 discoverability-safe binding maps `NOT_DISCOVERABLE` as `NOT_FOUND` outside admin diagnostic mode.
- `K-007`: P06 parameter extraction produces deterministic parameter metadata and `parameter_signature`.
- `K-008`: P07 SBLR emission includes required normalization evidence fields and deterministic checksum.
- `K-009`: P08 parser preflight rejects structurally invalid SBLR before IPC dispatch.
- `K-010`: P09 engine request envelope includes required context, source map, capability log, and parameter metadata.
- `K-011`: P10 response mapping forbids unmapped engine statuses and preserves correlation id.
- `K-012`: P11 stale plan-handle rejection path reruns deterministic parse-to-SBLR flow.
- `K-013`: P12 egress render enforces dialect-specific case, quoting, and language fallback rules.

### Suite L: P0 Parser Normalization Gate Checklist
- `L-001`: N00 freezes context/profile versions and rejects missing required fields.
- `L-002`: N01 rejects malformed ingress framing and maps deterministic dialect-native errors.
- `L-003`: N02 produces deterministic parse tree and complete source-span mapping.
- `L-004`: N03 capability gate precedence rejects any missing capability profile row.
- `L-005`: N04 canonical operator normalization rejects unmapped operators and emits deterministic operator vector.
- `L-006`: N05 coercion normalization rejects unresolved cast/coercion paths and emits deterministic coercion vector.
- `L-007`: N06 clause normalization emits valid clause bitmap/order and rejects clause-order violations.
- `L-008`: N07 UUID binding maps `NOT_DISCOVERABLE` according to parser discoverability policy and never leaks object existence by default.
- `L-009`: N08 canonical AST serialization is byte-identical for identical inputs.
- `L-010`: N09 normalization evidence hash is byte-identical for identical normalized evidence payloads.
- `L-011`: N10 emitted SBLR includes required normalization evidence fields and checksums.
- `L-012`: N11 preflight rejects unresolved symbols and malformed metadata before IPC dispatch.
- `L-013`: N12 engine envelope includes all fields required by section 23 plan-key construction.
- `L-014`: N13 stale plan-handle rejection reruns full N02..N12 flow deterministically.
- `L-015`: N14 and N15 preserve deterministic error mapping and dialect-correct egress formatting.

### Suite M: P1 Parser Distributed Policy and Telemetry
- `M-001`: PD00 gates distributed policy features by dialect profile with deterministic rejects.
- `M-002`: PD01 maps distributed policy controls to canonical fields without ambiguity.
- `M-003`: PD02 emits deterministic policy envelope ordering with correlation id.
- `M-004`: PD03 maps verification, repair, and degraded events to dialect diagnostics correctly.
- `M-005`: PD04 enforces telemetry exposure profile for request and response mapping.
- `M-006`: PD05 redacts restricted telemetry/event fields by role and policy.
- `M-007`: PD06 stale policy version rejection triggers deterministic remap and bounded retry.

### Suite N: P2 Parser Plan Stability and Hint Translation
- `N-001`: PH00 capability gate rejects unsupported hints deterministically.
- `N-002`: PH01 canonical hint translation emits stable serialized ordering.
- `N-003`: PH02 stable query-shape key remains identical across text-equivalent forms.
- `N-004`: PH03 tie-break policy propagation honors native/emulated capability boundaries.
- `N-005`: PH04 plan pin/unpin controls enforce privilege and deterministic mapping.
- `N-006`: PH05 diagnostics expose engine tie-break and hint application metadata correctly.
- `N-007`: PH06 stale hint policy recovery is deterministic and retry-bounded.

### Suite O: Passthrough and Live Migration Routing
- `O-001`: parser resolves `migration_name` to one valid `migration_uuid` before route derivation.
- `O-002`: parser route intent matches the canonical mode routing matrix for each statement class.
- `O-003`: stale `mode_version` on dispatch triggers deterministic reload and one bounded retry.
- `O-004`: `DUAL_WRITE` envelope preserves ordered targets (`legacy` then `emulated`).
- `O-005`: `MIRROR_LEGACY` envelope preserves ordered targets (`emulated` then `legacy`).
- `O-006`: `DUAL_READ_AUDIT` emits required compare metadata (`statement_fingerprint`, `compare_policy`, `return_source`).
- `O-007`: migration routing failures map to deterministic dialect-native errors without leaking internal enum ids.
- `O-008`: emulated parsers preserve 1:1 wire behavior while enforcing parser-specific feature gating.

### Suite P: Replication Control and Routing
- `P-001`: parser resolves `channel_name` to one valid `replication_channel_uuid` before control-op dispatch.
- `P-002`: all mutable replication commands require `EXPECT VERSION` and reject missing version guards.
- `P-003`: one-way channels reject peer-only controls; bi-directional channels require origin-sequence metadata.
- `P-004`: replication DDL and conflict policy tokens map to canonical enum labels deterministically.
- `P-005`: status commands (`SHOW REPLICATION STATUS|LAG|CURSORS|CONFLICTS`) return fixed response shape ids and column order.
- `P-006`: conflict-resolution commands require valid conflict UUID and action compatibility checks.
- `P-007`: split-brain fence and illegal state failures map to deterministic dialect-native errors.
- `P-008`: emulated parser publication/subscription commands map to canonical control ops or deterministic rejects per profile.

### Suite Q: Emulated Wire Protocol Contracts
- `Q-001`: Firebird adapter passes handshake negotiation and opcode family mapping gates in `NORMATIVE_WIRE_PROTOCOL_FIREBIRD_CHECKLIST.md`.
- `Q-002`: PostgreSQL adapter passes startup/simple/extended/copy flow gates in `NORMATIVE_WIRE_PROTOCOL_POSTGRESQL_CHECKLIST.md`.
- `Q-003`: MySQL adapter passes packet framing, handshake, capability, and prepared-statement lifecycle gates in `NORMATIVE_WIRE_PROTOCOL_MYSQL_CHECKLIST.md`.
- `Q-004`: Cassandra adapter passes v4/v5 framing, stream-id, and opcode mapping gates in `NORMATIVE_WIRE_PROTOCOL_CASSANDRA_CHECKLIST.md`.
- `Q-005`: MongoDB adapter passes header/opcode, OP_MSG/OP_COMPRESSED, and command mapping gates in `NORMATIVE_WIRE_PROTOCOL_MONGODB_CHECKLIST.md`.
- `Q-006`: Neo4j Bolt adapter passes negotiation/chunk framing/signature mapping/reset recovery gates in `NORMATIVE_WIRE_PROTOCOL_NEO4J_BOLT_CHECKLIST.md`.
- `Q-007`: Redis adapter passes RESP2/RESP3 framing, HELLO mode negotiation, command mapping, and push/error mapping gates in `NORMATIVE_WIRE_PROTOCOL_REDIS_RESP_CHECKLIST.md`.
- `Q-008`: Milvus adapter passes gRPC method routing, protobuf validation, unary/stream mapping, and auth/error gates in `NORMATIVE_WIRE_PROTOCOL_MILVUS_GRPC_CHECKLIST.md`.
- `Q-009`: each parser target writes its protocol evidence artifacts under `docs/specifications/work/conformance/wire/<engine>/` with required filenames from its normative wire checklist.
- `Q-010`: SQL Server / Azure SQL adapter passes prelogin/login, batch/RPC, transaction-manager, and tabular-result gates in `NORMATIVE_WIRE_PROTOCOL_SQLSERVER_TDS_CHECKLIST.md`.
- `Q-011`: Db2 adapter passes DDM framing, `EXCSAT`/`ACCSEC`/`SECCHK`/`ACCRDB`, statement/package, and query lifecycle gates in `NORMATIVE_WIRE_PROTOCOL_DB2_DRDA_CHECKLIST.md`.

### Suite R: Native Remote Connector Control Surface
- `R-001`: parser classifies all remote connector SQL into `FG_FDW` with deterministic feature keys from section 21.
- `R-002`: parser rejects any remote connector statement with missing capability profile row as `PROFILE_ENTRY_MISSING`.
- `R-003`: parser resolves wrapper/server/user-mapping/foreign-table identifiers to UUID without discoverability leakage.
- `R-004`: parser option normalization rejects unknown option keys and preserves deterministic option ordering.
- `R-005`: parser metadata operations (`ANALYZE|REFRESH|SHOW REMOTE *`) emit complete and deterministic control envelopes.
- `R-006`: parser passthrough execute envelopes include transaction mode, capability expectations, limits, and deterministic statement fingerprints.
- `R-007`: parser prepared remote lifecycle (`PREPARE|EXECUTE PREPARED|DEALLOCATE`) enforces deterministic session+connector handle rules.
- `R-008`: parser remote transaction control rejects invalid transaction-mode usage and missing open bindings.
- `R-009`: parser response mapping enforces fixed result-shape and fixed column-order contracts for all `SHOW REMOTE *` statements.
- `R-010`: parser error mapping preserves deterministic dialect-native errors and never leaks secret payload fields.
- `R-011`: emulated parsers reject native-only remote connector SQL unless explicit profile remap exists.
- `R-012`: every remote request carries request correlation fields required by section-24 audit writes.

### Suite S: Native Cluster Fabric Control Surface
- `S-001`: parser maps cluster-fabric SQL to deterministic cluster-control feature keys.
- `S-002`: parser rejects missing capability entries with `PROFILE_ENTRY_MISSING`.
- `S-003`: parser enforces `EXPECT VERSION` for mutable cluster-fabric link state/policy commands.
- `S-004`: parser enforces strict UUID binding for link/session/task identity fields.
- `S-005`: parser rejects passthrough fabric task submissions missing SBLR artifact payload reference.
- `S-006`: parser maps `SHOW CLUSTER FABRIC STATUS|SESSIONS|TASKS` to fixed result-shape contracts.
- `S-007`: emulated parsers reject native cluster-fabric control SQL unless explicit profile remap exists.
- `S-008`: parser never maps cluster-fabric data-plane execution payloads into SQL execution paths.

### Suite T: Emulated Parser Layer Bootstrap and Lifecycle
- `T-001`: every emulated parser family performs server pre-auth database availability check before auth success.
- `T-002`: `DENY_NO_OPEN_DATABASE` precheck result maps to deterministic dialect-auth-failure envelope and closes connection.
- `T-003`: emulated parser `CREATE DATABASE` requests are rejected for physical database file creation paths.
- `T-004`: emulated parser `CREATE DATABASE` requests map to logical emulation bootstrap only when base ScratchBird database is open.
- `T-005`: listener enforces one parser-family policy per bound port for all emulated families.
- `T-006`: assigned parser worker handles one client connection and is terminated on disconnect.
- `T-007`: parser teardown zeroizes auth/session buffers before process termination.
- `T-008`: first-install without open database causes deterministic auth failure across all emulated parser families.
- `T-009`: warm pool replenishment restores ready parser capacity after disconnect-driven worker teardown.
- `T-010`: no emulated parser family falls back to native parser path under bootstrap/auth/database-unavailable failures.
- `T-011`: emulated listener service remains disabled when matching support UDR package is missing, disabled, or not `READY`.
- `T-012`: parser-package and support-UDR translation of shared fixtures produce byte-identical SBLR and matching capability logs.
- `T-013`: emulated family operations owned by support UDR packages return deterministic package-missing or package-disabled errors when the package is incomplete.
- `T-014`: parser-only and support-only staged installs never fall back to compiled core-engine emulation behavior.



### Suite U: Beta 2 Emulation Family Bundle And Reference Readiness
- `U-001`: every Beta 2 family publishes parser, compiler UDR, and emulation UDR package manifests with one matching `bundle_contract_id`.
- `U-002`: every Beta 2 family has both a local 1:1 packet and an official donor web supplement before listener exposure is enabled.
- `U-003`: every Beta 2 family ships empty-database overlay goldens proving donor-visible system rows for a new logical database.
- `U-004`: parser and compiler UDR lowering of shared donor fixtures are byte-identical at the emitted SBLR layer.
- `U-005`: every Beta 2 family ships donor plan render goldens produced from canonical `RuntimePlan` inputs.
- `U-006`: every Beta 2 family ships a donor error-map completeness artifact keyed by `error_ref_uuid`.
- `U-007`: every Beta 2 family bridge client passes attestation, auth, and transaction-lifecycle tests without external donor client libraries.
- `U-008`: any missing bundle part or missing reference artifact keeps the family in a deterministic not-ready state.
- `U-009`: Beta 2 commercial families admitted from `commercial_protocol_readiness_2026-04-03` remain not-ready until the family-local 1:1 packet and official supplement exist under the emulation packet root.

## Required Fixtures
- Dialect-specific conformance corpus per parser target.
- Shared object-name and UUID binding fixtures.
- Error mapping fixtures that cover parser and engine error classes.
- Wire protocol frame fixtures for success and malformed traffic.
- K-suite fixture and evidence mapping is normatively defined in:
  - `K_SUITE_FIXTURE_AND_EVIDENCE_MANIFEST.md`

## Negative and Fuzz Requirements
- Grammar fuzzing for SQL and command decoders.
- Malformed wire frame fuzzing per protocol.
- Capability profile corruption tests.
- IPC payload corruption tests.

## Gate Criteria
1. All suites required for the currently enabled parser targets pass; package `03` enables the native V3 target only, and later packages expand the enabled target set.
2. No unresolved `REJECT` path lacks explicit error mapping.
3. Determinism checks (`D-001`, `D-005`) must pass before release.
4. Any parser profile version change requires full suite rerun.
5. Migration routing suite `O` must pass before enabling passthrough or live migration control.
6. Replication control suite `P` must pass before enabling one-way or bi-directional replication controls.
7. Native remote connector suite `R` must pass before enabling remote metadata or remote passthrough execution surfaces.
8. Native cluster fabric suite `S` must pass before enabling parserless SB<->SB fabric control surfaces.
9. Emulated parser layer suite `T` must pass before enabling legacy-emulation listener ports.

## Beta 2 required proof additions

- every donor family in `BETA2_EMULATED_DONOR_MAPPING_AND_SHARED_LOWERING_MODEL.md`
  has a source-backed mapping to shared AST and SBLR structures
- native v3 and donor parser packages emit equivalent canonical payloads for
  shared Beta 2 surfaces
- no donor parser introduces donor-private AST nodes or donor-private SBLR
  payloads for the initial Beta 2 surface set
- every donor family listed in `BETA2_FUNCTION_SURFACE_DONOR_MAPPING_MODEL.md`
  emits shared SQL/XML, SQL/JSON, aggregate-option, insert-source, parametric,
  lambda, `XMLTABLE`, and richer `ROWS FROM` carriers where that donor owns the
  surface
- donor packages for MySQL-family, PostgreSQL-family, ClickHouse, and DuckDB
  prove canonical lowering for the function-gap closure surfaces without
  donor-private AST or SBLR payloads
- no donor parser introduces donor-private opcodes for admitted Beta 2
  function-gap closure features
- parser conformance proves the native-v3 hard refusals for `APPLY`,
  `CREATE MATERIALIZED VIEW`, and MySQL-family insert aliases are removed
- parser coverage proves exposure of the canonical multi-model entry points
  for CQL, Mongo, Cypher, Redis, Milvus, OpenSearch, and FoundationDB

## Audit normalization note (2026-03-28)
- Current code-backed parser authority is bounded to the native V3 stack (`parser_v3`, `lexer_v3`, `ast_v3`, `v3_emitter`) plus dedicated shipped emulated SQL-family parser code for Firebird, PostgreSQL, and MySQL.
- Dedicated parser-agent and listener proof currently exists only for `sb_parser_fb`, `sb_parser_pg`, `sb_parser_mysql`, and the matching listener front doors; universal nine-family dedicated parser parity is not current implementation proof.
- Builtin emulation package scaffold proof is currently limited to `firebirdsql`, `postgresql`, and `mysql`.
- Cassandra, MongoDB, Neo4j, Redis, and Milvus are currently represented in this section by native-V3 feature vocabulary, catalog/runtime vocabulary, or checklist material rather than shipped dedicated parser implementations.
- Broad section-wide parity, corpus cardinality, and universal profile-generation claims are therefore bounded and must not be treated as present-day implementation proof without family-local source evidence.
- package `03` applies this bounded authority to the native-V3 lane only; later
  packages own any promotion of emulated-family suites into release gates

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
