# Code Area Ownership Map

## Research and Specification Write Scopes

| Ticket Range | Primary Write Scope | Secondary Write Scope | Parallelization Notes |
| --- | --- | --- | --- |
| `CPG-10-001` | `docs/work-plans/10-Commercial_Parity_Missing_Element_Research_and_Beta2_Closure/` | `docs/work-plans/README.md` | single-owner bootstrap |
| `CPG-10-002..003` | `docs/specifications/19_Security_Model/` | `docs/reference/workspace_library/technical_specs/security_tde/`, `docs/reference/workspace_library/whitepapers/security_tde/`, `docs/reference/reference_library/` | may run in parallel with non-section-19 tickets |
| `CPG-10-004..005` | `docs/specifications/19_Security_Model/` | `docs/reference/workspace_library/technical_specs/security_protected_query/`, `docs/reference/workspace_library/whitepapers/security_protected_query/`, `docs/reference/reference_library/` | avoid concurrent edits with `CPG-10-002..003` when touching same section files |
| `CPG-10-006..007` | `docs/specifications/25_Runtime_Modes/`, `docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/`, `docs/specifications/42_Failure_Model_and_Fault_Tolerance/` | `docs/reference/workspace_library/technical_specs/ha_dr_pitr/`, `docs/reference/workspace_library/whitepapers/ha_dr_pitr/`, `docs/reference/reference_library/` | coordinated multi-section work; do not split spec writes casually |
| `CPG-10-008..009` | `docs/specifications/38_Workload_Governance_and_Parallelism/`, `docs/specifications/33_Memory_Management/` | `docs/reference/workspace_library/technical_specs/tenant_isolation_qos/`, `docs/reference/workspace_library/whitepapers/tenant_isolation_qos/`, `docs/reference/reference_library/` | may run in parallel with other gaps |
| `CPG-10-010..011` | `docs/specifications/10_GC_and_Sweep/`, `docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/` | `docs/reference/workspace_library/technical_specs/archive_ilm/`, `docs/reference/workspace_library/whitepapers/archive_ilm/`, `docs/reference/reference_library/` | coordinated multi-section work |
| `CPG-10-012..013` | `docs/specifications/31_Conformance_Performance_and_Reliability_Gates/`, `docs/specifications/20_Diagnostics_Audit_and_Observability/` | `docs/reference/workspace_library/technical_specs/workload_replay/`, `docs/reference/workspace_library/whitepapers/workload_replay/`, `docs/reference/reference_library/` | may run in parallel with other gaps |
| `CPG-10-014..015` | `docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/`, `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/` | `docs/reference/workspace_library/technical_specs/open_table_formats/`, `docs/reference/workspace_library/whitepapers/open_table_formats/`, `docs/reference/reference_library/` | coordinated multi-section work |
| `CPG-10-016` | all touched section `README.md` files, `docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md`, package trackers | `docs/reference/...` indexes | closeout only after all spec lanes complete |

## Conflict Files

- `docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md`
- section `README.md` files for every touched numbered section
- this package's tracker files

## Unsafe Parallelization Boundaries

- do not run two spec-writing tickets concurrently inside the same numbered
  section without explicit write-scope separation
- do not update `AUTHORITATIVE_SPEC_INVENTORY.md` concurrently from multiple
  tickets
- do not modify the same reference manifest from parallel tickets without an
  explicit merge owner
