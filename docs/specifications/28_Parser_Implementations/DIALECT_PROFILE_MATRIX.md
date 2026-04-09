# Dialect Profile Matrix

## Purpose
Define parser target profiles, compatibility boundaries, and deterministic gating behavior for each native and emulated surface.

## Profile Storage Contract
- Profile definitions are stored as database configuration data, not hardcoded constants.
- Required profile tables are defined in section 24 catalog specifications.
- Parsers must load profile version at session start and include version in every engine request.

## Parser Target Matrix

| Parser Target | Ingress Protocol | Request Surface | Alpha Goal | Fallback Policy | Service Channel Support |
| --- | --- | --- | --- | --- | --- |
| native | SBWP and IPC-native request protocol | ScratchBird SQL and native admin surfaces | Full native surface | No fallback needed | Required |
| firebird | Firebird wire protocol | Firebird SQL, PSQL, service manager operations | 1:1 Firebird 5.x behavior in enabled profile | No fallback to native | Required |
| postgresql | PostgreSQL wire protocol | PostgreSQL SQL and protocol command set | 1:1 PostgreSQL behavior in enabled profile | No fallback to native | Required |
| mysql | MySQL wire protocol | MySQL SQL command surface | 1:1 MySQL 8.x behavior in enabled profile | No fallback to native | Required |
| cassandra | Cassandra native protocol | CQL command and query surface | 1:1 Cassandra behavior in enabled profile | No fallback to native | Required |
| mongodb | MongoDB command protocol | MongoDB command document model | 1:1 MongoDB behavior in enabled profile | No fallback to native | Required |
| neo4j | Bolt protocol | Cypher query and command surface | 1:1 Neo4j behavior in enabled profile | No fallback to native | Required |
| redis | RESP2 and RESP3 | Redis command surface | 1:1 Redis behavior in enabled profile | No fallback to native | Required |
| milvus | Milvus API protocol | Vector and collection API surface | 1:1 Milvus behavior in enabled profile | No fallback to native | Required |

## Capability Decision Matrix

| Capability State | Condition | Required Parser Action | Required Error or Artifact |
| --- | --- | --- | --- |
| `IMPLEMENT` | Profile has direct support | emit canonical AST without remap | capability decision artifact |
| `REMAP` | Profile defines transform id | apply transform and emit canonical AST | capability decision artifact with transform id |
| `REJECT` | Disabled, unsupported, or missing entry | stop processing before SBLR emission | dialect-native error mapped from decision code |

## Decision Codes
- `DIALECT_DISABLED`
- `FEATURE_DISABLED`
- `UNSUPPORTED_IN_DIALECT`
- `PROFILE_ENTRY_MISSING`
- `INVALID_PROFILE_VERSION`

Every parser must map decision codes to dialect-native errors.

## Session Variable Exposure Policy

### Allowed Exposure Modes
- `EXPOSED`
- `ALIASED`
- `HIDDEN`

### Required Behavior
1. Native parser must expose the full native variable set and any compatibility aliases declared in profile data.
2. Emulated parser must expose only variables marked `EXPOSED` or `ALIASED` in the profile.
3. Hidden variables must not appear in metadata introspection for the emulated parser.

## Identifier and Reserved Word Policy
- Identifier case, quoting, and reserved word handling are profile-defined.
- Native parser rules:
  - unquoted identifiers are case-insensitive for matching
  - quoted identifiers preserve exact spelling and block ambiguous variants
- Emulated parser rules:
  - follow the emulated profile rules exactly
  - any unsupported naming construct must fail with dialect-native error

## Service Channel Requirements
- Parsers that emulate engines with service channels must support service channel message families through parser-engine IPC mappings.
- At minimum, profile controls service families for:
  - administrative commands
  - backup and restore
  - event or notification feeds
  - long-running operation progress streams

## Profile Versioning Rules
1. Parser must reject requests when profile version is missing or unsupported.
2. Profile version upgrades require conformance test rerun for affected parser target.
3. Profile version must be logged with every parser request trace.

## Implementation Checklist
- Per-target profile exists.
- Capability map exists and has no missing entries for enabled features.
- Reserved word list exists for parser target.
- Session variable exposure map exists for parser target.
- Error mapping table exists for parser target.
- Conformance corpus exists for parser target.

## Audit normalization note (2026-03-28)
- Current code-backed parser authority is bounded to the native V3 stack (`parser_v3`, `lexer_v3`, `ast_v3`, `v3_emitter`) plus dedicated shipped emulated SQL-family parser code for Firebird, PostgreSQL, and MySQL.
- Dedicated parser-agent and listener proof currently exists only for `sb_parser_fb`, `sb_parser_pg`, `sb_parser_mysql`, and the matching listener front doors; universal nine-family dedicated parser parity is not current implementation proof.
- Builtin emulation package scaffold proof is currently limited to `firebirdsql`, `postgresql`, and `mysql`.
- Cassandra, MongoDB, Neo4j, Redis, and Milvus are currently represented in this section by native-V3 feature vocabulary, catalog/runtime vocabulary, or checklist material rather than shipped dedicated parser implementations.
- Broad section-wide parity, corpus cardinality, and universal profile-generation claims are therefore bounded and must not be treated as present-day implementation proof without family-local source evidence.
