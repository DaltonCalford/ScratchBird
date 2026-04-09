# Dependencies - 28_Parser_Implementations

## Upstream Dependencies
- 00_Governance_and_Invarients
- 01_Configuration_Subsystem
- 08_Transaction_Core
- 09_Lock_Manager_Core
- 10_GC_and_Sweep
- 16_Context_Variables
- 19_Security_Model
- 21_V3_Dialect_Surface
- 22_SBLR_Canonical_Model_and_Opcodes
- 23_SBLR_VM_Compiler_and_Executor
- 24_Catalog_Model_and_Virtual_Overlays
- 25_Runtime_Modes
- 26_Native_Wire_Protocol
- 27_Native_Handshake

## Downstream Dependents
- 29_Listener_and_Server_Orchestration
- 30_Client_Tooling
- 31_Conformance_Performance_and_Reliability_Gates

## External References
- `docs/specifications_old/parser/v3/ARCHITECTURE_CLARIFICATIONS.md`
- `docs/specifications_old/parser/v3/network/ENGINE_PARSER_IPC_CONTRACT.md`
- `docs/specifications_old/parser/v3/network/NETWORK_LISTENER_AND_PARSER_POOL_SPEC.md`
- `docs/specifications_old/parser/v3/V3_SINGLE_PATH_IMPLEMENTATION_GUIDE.md`

## Audit normalization note (2026-03-28)
- Current code-backed parser authority is bounded to the native V3 stack (`parser_v3`, `lexer_v3`, `ast_v3`, `v3_emitter`) plus dedicated shipped emulated SQL-family parser code for Firebird, PostgreSQL, and MySQL.
- Dedicated parser-agent and listener proof currently exists only for `sb_parser_fb`, `sb_parser_pg`, `sb_parser_mysql`, and the matching listener front doors; universal nine-family dedicated parser parity is not current implementation proof.
- Builtin emulation package scaffold proof is currently limited to `firebirdsql`, `postgresql`, and `mysql`.
- Cassandra, MongoDB, Neo4j, Redis, and Milvus are currently represented in this section by native-V3 feature vocabulary, catalog/runtime vocabulary, or checklist material rather than shipped dedicated parser implementations.
- Broad section-wide parity, corpus cardinality, and universal profile-generation claims are therefore bounded and must not be treated as present-day implementation proof without family-local source evidence.
