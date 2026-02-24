# UDR-CAT-001 Delta Matrix (Baseline Freeze + Spec-vs-Impl)
Last-Modified: 2026-02-23

## Scope
Baseline freeze and strict contract delta matrix for remote connector catalog prerequisites against:
1. `local_work/docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_SCHEMA_REMOTE_ENGINE_CONNECTOR.md`
2. `local_work/docs/specifications/17_Functions_and_Procedures/NORMATIVE_UDR_REMOTE_ENGINE_CONNECTOR_CHECKLIST.md`
3. `local_work/docs/specifications/28_Parser_Implementations/NORMATIVE_PARSER_REMOTE_ENGINE_CONNECTOR_CHECKLIST.md`

## Frozen Baseline
1. Engine baseline commit: `4a2bfe115a6bd5b5ef3dcb1faae88f0e939b9756`.
2. Remote catalog families present in code:
   - `remote_connector`, `remote_connector_capability`, `remote_metadata_snapshot`, `remote_metadata_object`, `remote_metadata_column`, `remote_schema_mapping`, `remote_passthrough_policy`, `remote_prepared_statement`, `remote_txn_binding`, `remote_execution_audit`, `remote_error`.
3. Primary code surfaces frozen for this analysis:
   - `include/scratchbird/core/catalog_manager.h:1230`
   - `src/core/catalog_manager.cpp:78599`
   - `src/core/catalog_manager.cpp:79059`
   - `src/core/catalog_manager.cpp:80413`
   - `src/core/catalog_manager.cpp:80617`
   - `src/udr/udr_connector.cpp:258`
   - `src/sblr/v3_canonical_feature_map.generated.cpp:92`
   - `src/parser/parser_v3.cpp:4898`
4. Existing test baseline covering current behavior:
   - `tests/unit/test_catalog_remote_connector_extension_contract.cpp:80`
   - `tests/unit/test_udr_connector_factory.cpp:186`

## Baseline Observations
1. Remote catalog CRUD APIs exist end-to-end for all section-24 remote families.
2. Runtime helper entry points `sys_remote_exec/query/call` are stubs returning `NOT_IMPLEMENTED`.
3. Canonical feature map currently includes only four FDW create keys; section-21 remote surfaces are not yet represented.
4. Current remote catalog tests validate CRUD and explicit delete paths (including snapshot/audit/binding deletion), which conflicts with append-only and immutability contracts.

## Strict Delta Matrix
| delta_id | contract_ref | requirement | code_evidence | status | delta_tag | notes |
|---|---|---|---|---|---|---|
| RCAT-001 | 17 checklist state machine; 24 hard invariants | Connector lifecycle states must be `REGISTERED/VALIDATED/READY/DEGRADED/DISABLED` | `include/scratchbird/core/catalog_manager.h:1230` | FAIL | ALTER | Current enum is `DISABLED/PROBING/READY/DEGRADED/FAILED`. |
| RCAT-002 | 17 checklist state machine | Enforce allowed/forbidden transition edges | `src/core/catalog_manager.cpp:78599` | FAIL | CONSTRAINT,RUNTIME_ENFORCEMENT | No transition validator is applied when connector state is updated. |
| RCAT-003 | 24 snapshot contract | `COMPLETE` requires hash + completed timestamp | `src/core/catalog_manager.cpp:79070` | PASS | CONSTRAINT | Explicitly validated at write and read paths. |
| RCAT-004 | 24 hard invariant | Snapshot immutable after `COMPLETE` | `src/core/catalog_manager.cpp:79059` | FAIL | CONSTRAINT,RUNTIME_ENFORCEMENT | Upsert path allows overwrite of existing completed snapshot rows by id. |
| RCAT-005 | 24 deterministic persistence rules | Snapshot rows should not be deleted as mutable control records | `src/core/catalog_manager.cpp:79242` | FAIL | RUNTIME_ENFORCEMENT | Public delete API exists and tests exercise deletion. |
| RCAT-006 | 24 deterministic persistence rules | Metadata object rows must reference committed complete snapshot | `src/core/catalog_manager.cpp:79274` | PASS | CONSTRAINT | Object upsert enforces snapshot status `COMPLETE`. |
| RCAT-007 | 24 hard invariant | Execution audit rows append-only terminal writes | `src/core/catalog_manager.cpp:80617` | FAIL | CONSTRAINT,RUNTIME_ENFORCEMENT | Upsert model allows update-by-id, not strict insert-only terminal write. |
| RCAT-008 | 24 table contract | Execution audit rows should not be deletable from control plane API | `src/core/catalog_manager.cpp:80818` | FAIL | RUNTIME_ENFORCEMENT | Delete API exists and is used by unit tests. |
| RCAT-009 | 24/28 PR11 | Request correlation id required for every remote execution audit row | `src/core/catalog_manager.cpp:80622` | PASS | CONSTRAINT | `request_id` is mandatory and uniqueness is enforced. |
| RCAT-010 | 24 deterministic persistence rules | Remote txn bindings terminal states immutable once terminal time set | `src/core/catalog_manager.cpp:80413` | FAIL | CONSTRAINT,RUNTIME_ENFORCEMENT | No immutability check prevents terminal row mutation. |
| RCAT-011 | 24 hard invariant | Remote txn bindings must be cleared on local terminal transaction events | `src/core/catalog_manager.cpp:80413` | PARTIAL | RUNTIME_ENFORCEMENT | Binding row stores `txid/state/time`; no automatic terminal cleanup invariant enforcement exists in catalog layer. |
| RCAT-012 | 24 deterministic persistence rules | Connector disable + policy disable must commit atomically | `src/core/catalog_manager.cpp:78599`, `src/core/catalog_manager.cpp:80024` | FAIL | ADD,RUNTIME_ENFORCEMENT | No single atomic API exists for connector+policy coordinated disable transition. |
| RCAT-013 | catalog prereq workplan C4 | Capability lineage/version chain must be replayable | `include/scratchbird/core/catalog_manager.h:1351` | PARTIAL | ADD,ALTER | Capability rows have key/group/value and discovered time but no explicit capability snapshot lineage key. |
| RCAT-014 | catalog prereq workplan C4 | Module attestation fields for allowlist/signature evidence | `include/scratchbird/core/catalog_manager.h:1325` | PARTIAL | ADD,ALTER | Only `module_checksum` is persisted; no signer/signature/attestation status fields. |
| RCAT-015 | 24 hard invariants | Secrets write-only; no clear-text readback | `src/core/catalog_manager.cpp:39892`, `src/core/catalog_manager.cpp:39949` | FAIL | CONSTRAINT,RUNTIME_ENFORCEMENT | User mapping credentials are persisted and loaded as plain text TOAST payloads; no mandatory encryption+write-only enforcement. |
| RCAT-016 | 24 hard invariants | Error/audit rows must avoid private endpoint and secret leaks | `src/core/catalog_manager.cpp:80840`, `src/core/catalog_manager.cpp:80617` | PARTIAL | ADD,RUNTIME_ENFORCEMENT | No deterministic redaction policy flagging is enforced at catalog write boundary. |
| RCAT-017 | 21/28 parser contract | Full `F_FDW_*` + `F_REMOTE_*` feature family closure for remote control surfaces | `src/sblr/v3_canonical_feature_map.generated.cpp:92` | FAIL | ADD | Only four `F_FDW_CREATE_*` style rows exist; alter/drop/import/remote execute metadata control keys absent. |
| RCAT-018 | catalog prereq baseline | Runtime helper entry points must be implemented for remote exec/query/call path | `src/udr/udr_connector.cpp:258` | FAIL | RUNTIME_ENFORCEMENT | `sys_remote_exec/query/call` currently return `NOT_IMPLEMENTED`. |

## Delta Totals
1. `PASS`: 4
2. `PARTIAL`: 3
3. `FAIL`: 11

## UDR-CAT-001 Gate Decision
1. `UDR-CAT-GATE-01`: PASS (baseline frozen + strict matrix produced).
2. Mandatory blockers for `UDR-CAT-GATE-03` and beyond are identified in `RCAT-001/002/004/007/010/012/015/017/018`.
3. Migration design required to close `UDR-CAT-GATE-02` is defined in `artifacts/udr/catalog/p6s1w1/udr-cat-002-migrations.md`.
