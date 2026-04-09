# Code Area Ownership Map

## Research and specification write scopes

| Ticket Range | Primary Write Scope | Secondary Write Scope | Parallelization Notes |
| --- | --- | --- | --- |
| `DQO-11-001` | `docs/work-plans/11-Distributed_Query_OLTP_Sharding_and_OLAP_Beta2_Closure/` | `docs/work-plans/README.md` | single-owner bootstrap |
| `DQO-11-002..003` | `docs/specifications/25_Runtime_Modes/`, `docs/specifications/36_Query_Rewrite_and_Planner/`, `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/` | `docs/reference/workspace_library/technical_specs/distributed_query/`, `docs/reference/workspace_library/whitepapers/distributed_query/`, `docs/reference/workspace_library/third_party_implementations/distributed_query/`, `docs/reference/reference_library/` | coordinated multi-section work |
| `DQO-11-004..005` | `docs/specifications/25_Runtime_Modes/`, `docs/specifications/36_Query_Rewrite_and_Planner/`, `docs/specifications/31_Conformance_Performance_and_Reliability_Gates/` | `docs/reference/workspace_library/technical_specs/high_performance_oltp/`, `docs/reference/workspace_library/whitepapers/high_performance_oltp/`, `docs/reference/workspace_library/third_party_implementations/high_performance_oltp/`, `docs/reference/reference_library/` | may run in parallel with non-overlapping topic families |
| `DQO-11-006..007` | `docs/specifications/25_Runtime_Modes/`, `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/`, `docs/specifications/42_Failure_Model_and_Fault_Tolerance/` | `docs/reference/workspace_library/technical_specs/sharding/`, `docs/reference/workspace_library/whitepapers/sharding/`, `docs/reference/workspace_library/third_party_implementations/sharding/`, `docs/reference/reference_library/` | coordinated multi-section work |
| `DQO-11-008..009` | `docs/specifications/18_Index_Framework/`, `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/`, `docs/specifications/36_Query_Rewrite_and_Planner/`, `docs/specifications/31_Conformance_Performance_and_Reliability_Gates/` | `docs/reference/workspace_library/technical_specs/olap_cubes/`, `docs/reference/workspace_library/whitepapers/olap_cubes/`, `docs/reference/workspace_library/third_party_implementations/olap_cubes/`, `docs/reference/reference_library/` | coordinated multi-section work |
| `DQO-11-010` | all touched section `README.md` files, `docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md`, package trackers | `docs/reference/...` indexes | closeout only after all spec lanes complete |

## Conflict files

- `docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md`
- section `README.md` files for every touched numbered section
- this package's tracker files

## Unsafe parallelization boundaries

- do not run two spec-writing tickets concurrently inside the same numbered
  section without explicit write-scope separation
- do not update `AUTHORITATIVE_SPEC_INVENTORY.md` concurrently from multiple
  tickets
- do not modify the same reference manifest from parallel tickets without an
  explicit merge owner
