# Code Area Ownership Map

## Research And Specification Write Scopes

| Ticket Range | Primary Write Scope | Secondary Write Scope | Parallelization Notes |
| --- | --- | --- | --- |
| `NEQ-12-001` | `docs/work-plans/12-ScratchBird_Native_Equivalent_Feature_Closure/` | `docs/work-plans/README.md` | single-owner bootstrap |
| `NEQ-12-002..005` | `docs/specifications/25_Runtime_Modes/`, `docs/specifications/20_Diagnostics_Audit_and_Observability/` | `docs/reference/workspace_library/technical_specs/native_eventing/`, `docs/reference/workspace_library/third_party_implementations/native_eventing/`, `docs/reference/reference_library/` | jobs and alerts may depend on eventing research; avoid conflicting edits in section 25 |
| `NEQ-12-006..007` | `docs/specifications/17_Functions_and_Procedures/`, `docs/specifications/23_SBLR_VM_Compiler_and_Executor/` | `docs/reference/workspace_library/technical_specs/managed_extensibility/`, `docs/reference/workspace_library/third_party_implementations/managed_extensibility/`, `docs/reference/reference_library/` | coordinated multi-section work |
| `NEQ-12-008..013` | `docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/`, `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/`, `docs/specifications/20_Diagnostics_Audit_and_Observability/` | `docs/reference/workspace_library/technical_specs/changefeed_temporal_ledger/`, `docs/reference/workspace_library/whitepapers/changefeed_temporal_ledger/`, `docs/reference/reference_library/` | temporal and ledger tickets should not race on the same section files |
| `NEQ-12-014..015` | `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/`, `docs/specifications/21_V3_Dialect_Surface/` | `docs/reference/workspace_library/technical_specs/property_graph/`, `docs/reference/workspace_library/third_party_implementations/property_graph/`, `docs/reference/reference_library/` | graph query-surface work may touch parser section files |
| `NEQ-12-016..021` | `docs/specifications/36_Query_Rewrite_and_Planner/`, `docs/specifications/17_Functions_and_Procedures/`, `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/` | `docs/reference/workspace_library/technical_specs/federation_and_tuning/`, `docs/reference/workspace_library/third_party_implementations/federation_and_tuning/`, `docs/reference/reference_library/` | federation and plan-store closure may run in parallel if write sets are separated |
| `NEQ-12-018..019` | `docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/`, `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/` | `docs/reference/workspace_library/technical_specs/blob_namespace/`, `docs/reference/workspace_library/third_party_implementations/blob_namespace/`, `docs/reference/reference_library/` | coordinated multi-section work |
| `NEQ-12-022..029` | `docs/specifications/25_Runtime_Modes/`, `docs/specifications/38_Workload_Governance_and_Parallelism/`, `docs/specifications/31_Conformance_Performance_and_Reliability_Gates/`, `docs/specifications/18_Index_Framework/` | `docs/reference/workspace_library/technical_specs/service_tiers_oltp_geo/`, `docs/reference/workspace_library/whitepapers/service_tiers_oltp_geo/`, `docs/reference/reference_library/` | serverless depends on service-tier research; geo failover and OLTP lanes should not share closeout ownership |
| `NEQ-12-030..033` | `docs/specifications/42_Failure_Model_and_Fault_Tolerance/`, `docs/specifications/19_Security_Model/`, `docs/specifications/25_Runtime_Modes/` | `docs/reference/workspace_library/technical_specs/distributed_commit_and_identity/`, `docs/reference/workspace_library/third_party_implementations/distributed_commit_and_identity/`, `docs/reference/reference_library/` | distributed commit and identity federation may run in parallel |
| `NEQ-12-034..041` | `docs/specifications/19_Security_Model/`, `docs/specifications/18_Index_Framework/`, `docs/specifications/31_Conformance_Performance_and_Reliability_Gates/` | `docs/reference/workspace_library/technical_specs/implementation_closure/`, `docs/reference/workspace_library/whitepapers/implementation_closure/`, `docs/reference/reference_library/` | avoid concurrent edits inside the same section without explicit owner |
| `NEQ-12-042` | all touched section `README.md` files, `docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md`, package trackers | all touched reference indexes | closeout only after all spec lanes complete |

## Conflict Files

- `docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md`
- section `README.md` files for every touched numbered section
- this package's tracker files
- any shared research manifest under `docs/reference/reference_library/`

## Unsafe Parallelization Boundaries

- do not run two spec-writing tickets concurrently inside the same numbered
  section without explicit write-scope separation
- do not update `AUTHORITATIVE_SPEC_INVENTORY.md` concurrently from multiple
  tickets
- do not mix donor-only compatibility work into this package
- do not let a research ticket mutate canonical numbered specs before the
  companion spec ticket starts
