# Native Parser Emulation Gap Analysis (Targeted Sweep)

Date: 2026-02-11  
Scope: Step 1 parser analysis before SBLR opcode mapping.  
Goal: Identify what the native parser specifications still need so native can act as a superset of all emulated engines, with emulated parsers as gated subsets.

## Method
- Reviewed canonical parser/dialect specs only (excluding `legacy_imports`) in:
  - `docs/specifications/21_V3_Dialect_Surface/`
  - `docs/specifications/28_Parser_Implementations/`
- Reviewed legacy parser v3 reference tree for historical coverage signals:
  - `docs/specifications/28_Parser_Implementations/legacy_imports/specifications_old/parser/v3/`
- Collected parser/protocol feature evidence from local engine source clones:
  - `/home/dcalford/CliWork/firebird`
  - `/home/dcalford/CliWork/postgresql`
  - `/home/dcalford/CliWork/mysql-server`
  - `/home/dcalford/CliWork/cassandra`
  - `/home/dcalford/CliWork/mongo`
  - `/home/dcalford/CliWork/neo4j`
  - `/home/dcalford/CliWork/redis`
  - `/home/dcalford/CliWork/milvus`

## High-Level Findings
- Canonical section 28 has good architecture and invariants (separate parsers, capability gating, deterministic SQL/command->SBLR pipeline).
- Canonical section 21 is still underspecified for native SQL/command surface breadth versus all emulated engines.
- Current native SQL spec is mostly a compact baseline and does not yet enumerate many required feature families that emulated engines expose.
- Result: not yet implementation-complete for a low-capability non-reasoning AI to implement full parser parity without guessing.

## Evidence Highlights
- Firebird grammar includes package/procedure/function/exception/trigger/execute-block/savepoint/returning constructs:
  - `/home/dcalford/CliWork/firebird/src/dsql/parse.y`
- PostgreSQL grammar includes COPY, LISTEN/NOTIFY, PREPARE/DEALLOCATE, FDW, policy, publication/subscription, materialized view:
  - `/home/dcalford/CliWork/postgresql/src/backend/parser/gram.y`
- MySQL grammar command routing includes create user/view/trigger/procedure/function/event, XA, clone, grant/revoke:
  - `/home/dcalford/CliWork/mysql-server/sql/sql_yacc.yy`
- Cassandra CQL grammar includes keyspace/table/type/index/materialized view/batch/ttl:
  - `/home/dcalford/CliWork/cassandra/src/antlr/Parser.g`
  - `/home/dcalford/CliWork/cassandra/src/java/org/apache/cassandra/cql3/statements/` (75 files)
  - `/home/dcalford/CliWork/cassandra/src/java/org/apache/cassandra/cql3/statements/schema/` (37 files)
- Mongo command surface is command-document based and broad:
  - `/home/dcalford/CliWork/mongo/src/mongo/db/commands/` (173 `*.cpp`/`*.idl` files)
- Neo4j Cypher grammar includes match/merge/unwind/call/load csv plus admin grants/revokes:
  - `/home/dcalford/CliWork/neo4j/community/cypher/front-end/parser/v5/parser/src/main/antlr4/org/neo4j/cypher/internal/parser/v5/Cypher5Parser.g4`
- Redis command registry is large and family-based:
  - `/home/dcalford/CliWork/redis/src/commands/` (420 command JSON files)
- Milvus API surface includes collection/index/search/query/rbac/database operations:
  - `/home/dcalford/CliWork/milvus/pkg/proto/root_coord.proto`
  - `/home/dcalford/CliWork/milvus/pkg/proto/index_coord.proto`
  - `/home/dcalford/CliWork/milvus/pkg/proto/data_coord.proto`

## Engine-by-Engine Native Gap Matrix

| Engine | Upstream Surface Signal | Native Canonical Status (`21_*`) | Gap |
| --- | --- | --- | --- |
| Firebird | Package/function/procedure/exception/execute-block/trigger DDL and PSQL in `parse.y` | Native docs cover only compact generic DDL/DML examples; no full Firebird-equivalent native surface clauses | Missing explicit native grammar/spec clauses for Firebird-equivalent procedural and DDL families |
| PostgreSQL | COPY, LISTEN/NOTIFY, PREPARE/DEALLOCATE, FDW, policy, materialized view, publication/subscription in `gram.y` | Only diagnostics mentions and compact core SQL examples; no full clause-level native parity matrix | Missing full native specification for these families and exact remap/accept/reject behavior |
| MySQL | SQLCOM coverage for event/procedure/function/trigger/user/auth/XA/clone/admin in `sql_yacc.yy` | No explicit native spec for MySQL-equivalent admin and procedural command families | Missing native statement families and deterministic option handling rules |
| Cassandra | CQL keyspace/table/type/index/materialized view/batch/ttl semantics in `Parser.g` | Section 28 acknowledges parser target; section 21 lacks native clause-level equivalents for CQL behavior | Missing native-surface definitions for CQL-equivalent DDL/DML/consistency/TTL grammar contracts |
| MongoDB | Command-document protocol with broad command set in `commands/*.idl/*.cpp` | Native section 21 is SQL-focused and does not define command-document native syntax surface | Missing native canonical command abstraction and mapping contract for document operations |
| Neo4j | Cypher match/merge/unwind/call/load csv and admin commands in `Cypher5Parser.g4` | No native parser specification for graph-pattern clauses and procedural call surface | Missing native grammar contract for graph-equivalent operations |
| Redis | 420 command JSON definitions (ACL/cluster/pubsub/streams/scripting/functions) | No native command-family specification for key/value command semantics | Missing native command abstraction, transactional/pipeline semantics, and error mapping rules |
| Milvus | Collection/index/search/query/load/rbac/database APIs in proto services | No native API-style command surface spec for vector lifecycle and query/search pipeline | Missing native vector API grammar contracts and request/response mapping rules |

## Cross-Engine Structural Gaps
- Missing authoritative native superset matrix in section 21:
  - For each feature family: `native_syntax`, `supported_engines`, `engine_profile_gates`, `expected_result_shape`, `error_map_key`.
- Missing parser conformance corpus definition by feature family in section 28:
  - Current tests are high-level suite names; no canonical per-feature test inventory.
- Missing explicit service-channel language contracts in section 21 for:
  - backup/restore/admin/event/long-running job progress flows.
- Missing deterministic feature normalization tables:
  - Example classes: upsert, returning clauses, conflict handling, copy/bulk ingest, notification channels, graph traversal.
- Missing explicit low-capability implementation directives:
  - tokenization rules
  - AST node schema per family
  - disallowed ambiguity fallbacks
  - exact reject code mapping when capability is absent

## Legacy v3 Review Notes (What Is Useful vs Missing)
- Useful signals in legacy tree:
  - Separation of parser from engine is explicit.
  - Emulated parser separation is explicit.
  - Broad family coverage exists in historical docs.
- Missing for current goals:
  - Legacy tree does not fully reflect current 9-parser target and current canonical section layout.
  - Legacy docs are inconsistent in authority and often reference deprecated phase/status models.
  - Legacy docs are not sufficient as direct implementation source for low-capability deterministic execution.

## Required Additions Before SBLR Mapping Work Starts
1. Section 21 must gain an authoritative `native superset compatibility matrix` document.
2. Section 21 must define native clause-level syntax and behavior for each emulated engine feature family.
3. Section 21 must define deterministic admin/service-channel command syntax and response contracts.
4. Section 28 must gain feature-family conformance corpus inventory per parser target.
5. Section 28 must define per-feature capability entries and deterministic reject/remap rules.
6. Section 28 must define per-feature diagnostic and error-map examples for all parser targets.

## Proposed Output Artifacts for Next Pass
- `docs/specifications/21_V3_Dialect_Surface/NATIVE_SUPERSET_COMPATIBILITY_MATRIX.md`
- `docs/specifications/21_V3_Dialect_Surface/NATIVE_PARSER_FEATURE_FAMILIES.md`
- `docs/specifications/28_Parser_Implementations/PARSER_CONFORMANCE_CORPUS_INDEX.md`
- `docs/specifications/28_Parser_Implementations/CAPABILITY_PROFILE_ENTRIES_CANONICAL.md`

## Conclusion
Step 1 analysis confirms architecture is in place, but native dialect specification breadth is not yet complete for deterministic low-capability implementation across all nine parser targets. The immediate blocker is missing clause-level native superset definitions and per-feature capability/test inventories.
