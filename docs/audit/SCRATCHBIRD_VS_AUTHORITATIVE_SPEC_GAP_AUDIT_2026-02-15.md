# ScratchBird vs Authoritative Specification Gap Audit

Date: 2026-02-15  
Scope audited: `~/CliWork/ScratchBird` code/build/tests only (no progress-note trust)  
Authority baseline: `~/CliWork/local_work/docs/specifications` (`v1.0.0`, effective `2026-02-11`)

## 1) Audit Verdict

ScratchBird is **not implementation-complete** against the authoritative spec set.  
There are multiple **hard not-implemented gaps** (missing parser families, missing tool binaries, placeholder/stub core paths), plus a broad **conformance evidence gap** (required gate artifact structure absent).

Because section 31 requires gate evidence bundles for release, and those artifact roots are missing, the project cannot be promoted as conformant even where partial code exists.

## 2) Hard Not-Implemented Gaps (Code/Binary Surface)

### 2.1 Parser/listener family inventory is incomplete (P0)

Spec requires exactly 9 parser families (`native`, `firebird`, `postgresql`, `mysql`, `cassandra`, `mongodb`, `neo4j`, `redis`, `milvus`):
- `local_work/docs/specifications/25_Runtime_Modes/TEST_CONTRACT.md:14`
- `local_work/docs/specifications/25_Runtime_Modes/TEST_CONTRACT.md:112`
- `local_work/docs/specifications/28_Parser_Implementations/TEST_CONTRACT.md:19`
- `local_work/docs/specifications/28_Parser_Implementations/TEST_CONTRACT.md:23`

Current build defines only 4 listener/parser families:
- `ScratchBird/src/CMakeLists.txt:647`
- `ScratchBird/src/CMakeLists.txt:659`
- `ScratchBird/src/CMakeLists.txt:671`
- `ScratchBird/src/CMakeLists.txt:683`
- `ScratchBird/src/CMakeLists.txt:701`
- `ScratchBird/src/CMakeLists.txt:713`
- `ScratchBird/src/CMakeLists.txt:725`
- `ScratchBird/src/CMakeLists.txt:737`

Current parser/protocol source surface also only covers native+3 emulations:
- `ScratchBird/src/parser` (only `firebird`, `mysql`, `postgresql` subtrees)
- `ScratchBird/src/protocol/adapters/firebird_adapter.cpp`
- `ScratchBird/src/protocol/adapters/mysql_adapter.cpp`
- `ScratchBird/src/protocol/adapters/postgresql_adapter.cpp`
- `ScratchBird/src/protocol/adapters/native_adapter.cpp`

Missing implementation families in current tree:
- `cassandra`
- `mongodb`
- `neo4j`
- `redis`
- `milvus`

### 2.2 Required client tools are not implemented as binaries (P0)

Required alpha tools:
- `local_work/docs/specifications/30_Client_Tooling/CONNECTIVITY_PROFILES_AND_TOOL_RUNTIME.md:37`
- `local_work/docs/specifications/30_Client_Tooling/CONNECTIVITY_PROFILES_AND_TOOL_RUNTIME.md:45`

Current executable set does not include `sb_isql`, `sb_admin`, `sb_backup`, `sb_restore`, `sb_validate`, `sb_jobctl`, `sb_migrate`, `sb_replctl`.
- `ScratchBird/src/CMakeLists.txt:620`
- `ScratchBird/src/CMakeLists.txt:764`
- `ScratchBird/tools/CMakeLists.txt:4`

Tooling command contract that is currently unimplemented as binaries:
- `local_work/docs/specifications/30_Client_Tooling/TOOL_COMMAND_SURFACE_CONTRACTS.md:12`
- `local_work/docs/specifications/30_Client_Tooling/TOOL_COMMAND_SURFACE_CONTRACTS.md:100`

### 2.3 Query planner is placeholder (P0)

Planner file explicitly states placeholder state and returns no plan:
- `ScratchBird/src/optimizer/query_planner.cpp:13`
- `ScratchBird/src/optimizer/query_planner.cpp:32`
- `ScratchBird/src/optimizer/query_planner.cpp:42`

This conflicts with optimizer obligations:
- `local_work/docs/specifications/23_SBLR_VM_Compiler_and_Executor/TEST_CONTRACT.md:21`
- `local_work/docs/specifications/23_SBLR_VM_Compiler_and_Executor/TEST_CONTRACT.md:24`

### 2.4 Main executable is still minimal alpha REPL (P1)

`scratchbird` main is a minimal create/open/info REPL and explicitly says full SQL is future work:
- `ScratchBird/src/main.cpp:56`
- `ScratchBird/src/main.cpp:157`
- `ScratchBird/src/main.cpp:158`
- `ScratchBird/src/CMakeLists.txt:752`

### 2.5 Security auth provider stack is partially stubbed (P0 for full security parity)

LDAP/AD providers are explicit stubs; OAuth2/SAML/Kerberos/external providers return not implemented:
- `ScratchBird/src/core/auth_provider.cpp:800`
- `ScratchBird/src/core/auth_provider.cpp:821`
- `ScratchBird/src/core/auth_provider.cpp:847`
- `ScratchBird/src/core/auth_provider.cpp:868`
- `ScratchBird/src/core/auth_provider.cpp:924`
- `ScratchBird/src/core/auth_provider.cpp:928`

This is below required authn integration/security gate expectations:
- `local_work/docs/specifications/19_Security_Model/TEST_CONTRACT.md:5`
- `local_work/docs/specifications/19_Security_Model/TEST_CONTRACT.md:29`

### 2.6 Password hashing path is explicit placeholder/non-production crypto (P0 security)

Current password hash uses `std::hash` and comments call out placeholder/non-secure behavior:
- `ScratchBird/src/security/password_policy.cpp:263`
- `ScratchBird/src/security/password_policy.cpp:264`
- `ScratchBird/src/security/password_policy.cpp:269`
- `ScratchBird/src/security/password_policy.cpp:270`

### 2.7 Connection pool backend behavior is largely TODO/simulated (P1)

Connection pool has explicit placeholder handle and multiple TODOs for real connect/execute/validate/reset/close:
- `ScratchBird/src/pool/connection_pool.cpp:61`
- `ScratchBird/src/pool/connection_pool.cpp:194`
- `ScratchBird/src/pool/connection_pool.cpp:208`
- `ScratchBird/src/pool/connection_pool.cpp:223`
- `ScratchBird/src/pool/connection_pool.cpp:237`
- `ScratchBird/src/pool/connection_pool.cpp:261`
- `ScratchBird/src/pool/connection_pool.cpp:281`

### 2.8 IPC and transaction op coverage has explicit not-implemented paths (P1)

Engine IPC session handler:
- Unbound portal execution not implemented: `ScratchBird/src/ipc/external_agents/engine_ipc_session_handler.cpp:524`
- BEGIN opcode still TODO/placeholder: `ScratchBird/src/ipc/external_agents/engine_ipc_session_handler.cpp:670`
- SAVEPOINT opcode TODO and returns not implemented error: `ScratchBird/src/ipc/external_agents/engine_ipc_session_handler.cpp:757`, `ScratchBird/src/ipc/external_agents/engine_ipc_session_handler.cpp:766`

Executor fallback still reports missing V3 opcode implementations:
- `ScratchBird/src/sblr/executor.cpp:56016`

### 2.9 Dialect parser surface contains explicit stubs (P1)

Examples:
- PostgreSQL parser DDL: `ScratchBird/src/parser/postgresql/pg_parser_ddl.cpp:3634`
- Firebird parser clause parsing stubs: `ScratchBird/src/parser/firebird/firebird_parser.cpp:5015`

### 2.10 UDR connector surface not expanded to full planned engine set (P1)

Current UDR files cover native/firebird/mysql/postgresql only:
- `ScratchBird/src/udr/firebird_udr.cpp`
- `ScratchBird/src/udr/mysql_udr.cpp`
- `ScratchBird/src/udr/postgresql_udr.cpp`
- `ScratchBird/src/udr/scratchbird_udr.cpp`

Section 17 requires broader connector/fabric ABI validation:
- `local_work/docs/specifications/17_Functions_and_Procedures/TEST_CONTRACT.md:11`
- `local_work/docs/specifications/17_Functions_and_Procedures/TEST_CONTRACT.md:15`

## 3) Conformance/Gate Not-Implemented Gaps (Evidence & Test Harness)

### 3.1 Required gate evidence artifact tree is absent (P0)

Required artifact directories are defined in matrix:
- `local_work/docs/specifications/31_Conformance_Performance_and_Reliability_Gates/GATE_EVIDENCE_ARTIFACT_MATRIX.csv:2`
- `local_work/docs/specifications/31_Conformance_Performance_and_Reliability_Gates/GATE_EVIDENCE_ARTIFACT_MATRIX.csv:80`

Spec also requires release gate completion:
- `local_work/docs/specifications/31_Conformance_Performance_and_Reliability_Gates/TEST_CONTRACT.md:81`

Repo currently has no corresponding root:
- `MISSING: ScratchBird/docs/specifications/work/implementation_tracks/gate_evidence`
- `MISSING: ScratchBird/docs/specifications/work/conformance`

### 3.2 V3 conformance tests are mostly markdown TODO stubs (P0 for gate completion)

Current `tests/v3` files are mostly TODO placeholders, e.g.:
- `ScratchBird/tests/v3/dialect/test_dialect_gaps.md:7`
- `ScratchBird/tests/v3/executor/test_select_semantics.md:6`
- `ScratchBird/tests/v3/ipc/test_engine_parser_contract.md:6`
- `ScratchBird/tests/v3/protocol/test_wire_protocols.md:6`
- `ScratchBird/tests/v3/sblr/test_opcode_payloads.md:6`

Required suite coverage in parser contract is far broader and mandatory:
- `local_work/docs/specifications/28_Parser_Implementations/TEST_CONTRACT.md:8`
- `local_work/docs/specifications/28_Parser_Implementations/TEST_CONTRACT.md:236`

### 3.3 Test build excludes many tests and labels features as not fully implemented (P1)

Examples:
- API/refactor exclusions: `ScratchBird/tests/CMakeLists.txt:82`, `ScratchBird/tests/CMakeLists.txt:91`, `ScratchBird/tests/CMakeLists.txt:164`
- parser agent tests excluded: `ScratchBird/tests/CMakeLists.txt:166`, `ScratchBird/tests/CMakeLists.txt:168`
- explicit partial feature note: `ScratchBird/tests/CMakeLists.txt:281`, `ScratchBird/tests/CMakeLists.txt:282`

## 4) Section-Level Status Matrix (00..31)

Status keys:
- `NOT_IMPLEMENTED`: direct, concrete implementation gap observed.
- `PARTIAL/UNPROVEN`: code exists, but required contract evidence/gates not present in-tree.

| Section | Status | Basis |
|---|---|---|
| 00 | PARTIAL/UNPROVEN | No T31 gate evidence tree present. |
| 01 | PARTIAL/UNPROVEN | No T31 gate evidence tree present. |
| 02 | PARTIAL/UNPROVEN | No T31 gate evidence tree present. |
| 03 | PARTIAL/UNPROVEN | No T31 gate evidence tree present. |
| 04 | PARTIAL/UNPROVEN | No T31 gate evidence tree present. |
| 05 | PARTIAL/UNPROVEN | No T31 gate evidence tree present. |
| 06 | PARTIAL/UNPROVEN | No T31 gate evidence tree present. |
| 07 | PARTIAL/UNPROVEN | No T31 gate evidence tree present. |
| 08 | PARTIAL/UNPROVEN | No T31 gate evidence tree present. |
| 09 | PARTIAL/UNPROVEN | No T31 gate evidence tree present. |
| 10 | PARTIAL/UNPROVEN | No T31 gate evidence tree present. |
| 11 | PARTIAL/UNPROVEN | No T31 gate evidence tree present. |
| 12 | PARTIAL/UNPROVEN | No T31 gate evidence tree present. |
| 13 | PARTIAL/UNPROVEN | No T31 gate evidence tree present. |
| 14 | PARTIAL/UNPROVEN | No T31 gate evidence tree present. |
| 15 | PARTIAL/UNPROVEN | No T31 gate evidence tree present. |
| 16 | PARTIAL/UNPROVEN | No T31 gate evidence tree present. |
| 17 | NOT_IMPLEMENTED | Full connector/fabric ABI requirements not fully realized. |
| 18 | PARTIAL/UNPROVEN | No full gate evidence pack. |
| 19 | NOT_IMPLEMENTED | Auth provider stubs and placeholder password hashing. |
| 20 | PARTIAL/UNPROVEN | No full gate evidence pack. |
| 21 | PARTIAL/UNPROVEN | Parser surface contains explicit stubs; conformance corpus incomplete. |
| 22 | PARTIAL/UNPROVEN | Executor has opcode fallback not-implemented path. |
| 23 | NOT_IMPLEMENTED | Planner placeholder; optimizer gate obligations unmet. |
| 24 | PARTIAL/UNPROVEN | No full gate evidence pack. |
| 25 | NOT_IMPLEMENTED | Required 9 parser families not implemented. |
| 26 | PARTIAL/UNPROVEN | Protocol conformance evidence artifacts absent. |
| 27 | PARTIAL/UNPROVEN | Handshake conformance evidence artifacts absent. |
| 28 | NOT_IMPLEMENTED | Required parser suites/targets incomplete; required wire evidence paths absent. |
| 29 | PARTIAL/UNPROVEN | Listener/orchestration only for 4 families, not full target set. |
| 30 | NOT_IMPLEMENTED | Required CLI tools/command surfaces not implemented as binaries. |
| 31 | NOT_IMPLEMENTED | Required gate evidence bundles/directories absent; suites not evidenced. |

## 5) Practical “Still To Do” Summary

Minimum closure set before this can be called implemented against the authoritative spec:
1. Implement missing parser/listener/protocol/UDR families: `cassandra`, `mongodb`, `neo4j`, `redis`, `milvus`.
2. Implement required client tool binaries and command contracts (`sb_isql`, `sb_admin`, `sb_backup`, `sb_restore`, `sb_validate`, `sb_jobctl`, `sb_migrate`, `sb_replctl`).
3. Replace planner placeholder with full deterministic optimizer pipeline required by section 23.
4. Remove security stubs and placeholder hashing; complete full auth provider and crypto baseline.
5. Complete IPC transaction opcode coverage (`BEGIN`/`SAVEPOINT` paths and related semantics).
6. Replace V3 markdown TODO test placeholders with executable conformance suites.
7. Produce required section-31 gate evidence artifacts under the mandated output directories.

## 6) Notes on Audit Strictness

- This audit intentionally treated missing required evidence artifacts as non-implementation for release conformance.
- No claim was accepted from project notes unless a corresponding code/build/test artifact was found in-tree.
- Spec authority source used: `local_work/docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md:1`.
