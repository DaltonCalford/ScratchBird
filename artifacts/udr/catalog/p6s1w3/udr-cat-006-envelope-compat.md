# UDR-CAT-006 Envelope Compatibility + Lookup Closure

Date: 2026-02-23
Owner: agent-udr-core
Task: `UDR-CAT-006`
Gate: `UDR-CAT-GATE-06`

## Scope

Validate parser/control-envelope compatibility for remote surfaces and confirm mandatory catalog lookup closures for runtime dispatch.

## Section-21 Remote Feature Key Matrix

Source of truth: `src/sblr/v3_canonical_feature_map.generated.cpp` (`F_REMOTE_*` rows).

| Feature Key | Opcode Symbol | Payload Schema | Result Shape | Identity Mapping | Payload Mapping | Executor Coverage | Status |
|---|---|---|---|---|---|---|---|
| `F_REMOTE_ANALYZE_METADATA` | `OP_STMT_REMOTE_ANALYZE_METADATA` | `PS_REMOTE_ANALYZE_METADATA` | `RS_DIAGNOSTIC_REPORT` | yes | yes | yes | OK |
| `F_REMOTE_REFRESH_METADATA` | `OP_STMT_REMOTE_REFRESH_METADATA` | `PS_REMOTE_REFRESH_METADATA` | `RS_COMMAND_STATUS` | yes | yes | yes | OK |
| `F_REMOTE_SHOW_CAPABILITIES` | `OP_STMT_REMOTE_SHOW_CAPABILITIES` | `PS_REMOTE_SHOW_CAPABILITIES` | `RS_ROWSET` | yes | yes | yes | OK |
| `F_REMOTE_SHOW_OBJECTS` | `OP_STMT_REMOTE_SHOW_OBJECTS` | `PS_REMOTE_SHOW_OBJECTS` | `RS_ROWSET` | yes | yes | yes | OK |
| `F_REMOTE_SHOW_COLUMNS` | `OP_STMT_REMOTE_SHOW_COLUMNS` | `PS_REMOTE_SHOW_COLUMNS` | `RS_ROWSET` | yes | yes | yes | OK |
| `F_REMOTE_SHOW_STATISTICS` | `OP_STMT_REMOTE_SHOW_STATISTICS` | `PS_REMOTE_SHOW_STATISTICS` | `RS_ROWSET` | yes | yes | yes | OK |
| `F_REMOTE_EXECUTE` | `OP_STMT_REMOTE_EXECUTE` | `PS_REMOTE_EXECUTE` | `RS_ROWSET_OR_MUTATION` | yes | yes | yes | OK |
| `F_REMOTE_PREPARE` | `OP_STMT_REMOTE_PREPARE` | `PS_REMOTE_PREPARE` | `RS_COMMAND_STATUS` | yes | yes | yes | OK |
| `F_REMOTE_EXECUTE_PREPARED` | `OP_STMT_REMOTE_EXECUTE_PREPARED` | `PS_REMOTE_EXECUTE_PREPARED` | `RS_ROWSET_OR_MUTATION` | yes | yes | yes | OK |
| `F_REMOTE_DEALLOCATE_PREPARED` | `OP_STMT_REMOTE_DEALLOCATE_PREPARED` | `PS_REMOTE_DEALLOCATE_PREPARED` | `RS_COMMAND_STATUS` | yes | yes | yes | OK |
| `F_REMOTE_BEGIN_TXN` | `OP_STMT_REMOTE_BEGIN_TXN` | `PS_REMOTE_BEGIN_TXN` | `RS_COMMAND_STATUS` | yes | yes | yes | OK |
| `F_REMOTE_COMMIT_TXN` | `OP_STMT_REMOTE_COMMIT_TXN` | `PS_REMOTE_COMMIT_TXN` | `RS_COMMAND_STATUS` | yes | yes | yes | OK |
| `F_REMOTE_ROLLBACK_TXN` | `OP_STMT_REMOTE_ROLLBACK_TXN` | `PS_REMOTE_ROLLBACK_TXN` | `RS_COMMAND_STATUS` | yes | yes | yes | OK |
| `F_REMOTE_SHOW_SESSION_STATE` | `OP_STMT_REMOTE_SHOW_SESSION_STATE` | `PS_REMOTE_SHOW_SESSION_STATE` | `RS_ROWSET` | yes | yes | yes | OK |

Identity mapping evidence:
- `src/sblr/v3_opcode_identity.cpp:87`
- `src/sblr/v3_opcode_identity.cpp:100`
- `tests/unit/test_sblr_v3_opcode_identity.cpp:85`
- `tests/unit/test_sblr_v3_opcode_identity.cpp:312`

Payload mapping evidence:
- `src/sblr/v3_payload_map.generated.cpp:259`
- `src/sblr/v3_payload_map.generated.cpp:272`
- `tests/unit/test_sblr_vnext_payload_schema_mapping_contract.cpp:105`
- `tests/unit/test_sblr_vnext_payload_schema_mapping_contract.cpp:118`

Executor dispatch evidence:
- `tests/unit/test_sblr_vnext_executor_dispatch_contract.cpp:821`
- `tests/unit/test_sblr_vnext_executor_dispatch_contract.cpp:834`

## Mandatory Catalog Lookup Closure Matrix (PR00..PR12 Coverage)

PR checklist source file is not separately published in-tree; closure validated against implemented remote dispatch flow and contracts.

| Checklist Anchor | Required Lookup/Write Path | Evidence | Status |
|---|---|---|---|
| PR00 | Resolve remote server target | `src/sblr/executor.cpp:59585` | OK |
| PR01 | Resolve user mapping for runtime credentials | `src/sblr/executor.cpp:59605` | OK |
| PR02 | Resolve connector + policy rows | `src/sblr/executor.cpp:59628`, `src/sblr/executor.cpp:59683` | OK |
| PR03 | Metadata snapshot read paths | `src/sblr/executor.cpp:60408`, `src/sblr/executor.cpp:60526` | OK |
| PR04 | Runtime dispatch + remote error persistence | `src/sblr/executor.cpp:61557` | OK |
| PR05 | Prepared lifecycle persistence | `src/sblr/executor.cpp:61777` | OK |
| PR06 | Remote transaction binding persistence | `src/sblr/executor.cpp:61894` | OK |
| PR07 | Execution audit persistence | `src/sblr/executor.cpp:62049` | OK |
| PR08 | Result-shape projection fallback | `src/sblr/executor.cpp:61978` | OK |
| PR09 | Control-envelope payload schema enforcement | `tests/unit/test_sblr_vnext_payload_schema_mapping_contract.cpp:42` | OK |
| PR10 | Canonical opcode identity closure | `tests/unit/test_sblr_v3_opcode_identity.cpp:16` | OK |
| PR11 | Request/session/txn correlation fields in audit | `src/sblr/executor.cpp:61992` | OK |
| PR12 | Deterministic reject/failure codes for unsupported runtime states | `tests/unit/test_sblr_vnext_executor_dispatch_contract.cpp:847` | OK |

## Unresolved Rows

None for mandatory section-21 remote feature keys and mandatory runtime lookup closures in this cycle.

## Gate Evidence (Executed)

1. `build/tests/scratchbird_tests --gtest_filter='SBLRV3OpcodeIdentity.*:SBLRV3CanonicalFeatureMap.*'`
   - PASS (`5/5`)
2. `build/tests/scratchbird_tests --gtest_filter='SBLRVNextPayloadSchemaMappingContractTest.*'`
   - PASS (`6/6`)
3. `build/tests/scratchbird_tests --gtest_filter='SBLRVNextExecutorDispatchContractTest.*'`
   - PASS (`22/22`)

