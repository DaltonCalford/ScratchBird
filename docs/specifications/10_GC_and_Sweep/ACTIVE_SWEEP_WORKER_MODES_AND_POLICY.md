# Active Sweep Worker Modes and Policy

Status: current_authority_with_reconstructed_expansion

## Purpose

Define the canonical active worker model for sweep and garbage collection,
including policy-driven audit, schema-change evidence, page spot audit, and
shadow-capture lanes.

## Hard invariants

1. MGA visibility, `OIT`, `OAT`, `OST`, and TIP state remain the only source of
   GC eligibility truth.
2. Active sweep workers may add evidence duties, but they do not change
   eligibility rules.
3. External export and remote delivery are not part of the sweep hot path.
4. Transactional DDL and retired schema artifacts are handled under the same
   sweep policy framework as data-version retirement.
5. Strict audit modes may block maintenance completion or cutover transitions,
   but they do not retroactively alter committed transaction truth.

## Worker roles

The active sweep model is split into deterministic roles:

1. `SWEEP_COORDINATOR`
   - owns scheduling, object selection, throttle state, and worker admission
2. `SWEEP_CORE_WORKER`
   - evaluates garbage eligibility, prunes record/index/LOB state, and emits
     bounded work items
3. `PAGE_AUDIT_WORKER`
   - performs configured page spot audit during or immediately after sweep scan
4. `EVIDENCE_HANDOFF_WORKER`
   - persists local immutable evidence manifests and queue records
5. `EXPORT_WORKER`
   - performs downstream sink delivery outside the sweep hot path

## Runtime realization rule

One `SweepManager` implementation may host multiple roles in one subsystem or
one process, but the behavioral boundaries, stage transitions, failure classes,
and audit surfaces of the roles above must remain explicit.

## Policy lanes

Supported policy lanes:

1. `NORMAL`
2. `LINEAGE_RETENTION`
3. `OBJECT_TOUCH_AUDIT`
4. `SCHEMA_CHANGE_AUDIT`
5. `WAL_AFTER_EXPORT`
6. `PAGE_SPOT_AUDIT`
7. `SHADOW_CAPTURE`
8. `COMPOSITE`

## Policy binding authority

Persisted policy bindings are catalog-backed control state owned with section
`24` metadata authority.

Bindings may exist by:

- object scope
- table scope
- schema scope
- object family
- retention class

Resolution precedence is:

1. object scope
2. table scope
3. schema scope
4. object family
5. retention class
6. default `NORMAL`

The resolved binding publishes at minimum:

- `lane_mask`
- `strict_audit`
- binding source/preference

## Scheduling and throttling

1. `SWEEP_COORDINATOR` admits work based on OIT lag, dead-version pressure,
   explicit admin request, and foreground pressure.
2. Core worker count and I/O budget are bounded by configuration.
3. Evidence queue backlog may throttle new sweep admission, but never invalidates
   already-committed cleanup decisions.
4. Page-spot audit intensity may downgrade from diagnostic to light under
   foreground pressure.
5. Export workers may continue after a sweep pass completes.
6. Page-audit findings must persist chosen scan mode and trigger source.

## Failure rules

1. Failure to persist mandatory local evidence blocks prune of the affected
   item.
2. Failure of downstream export does not restore pruned garbage if mandatory
   local evidence was already persisted.
3. `STRICT_AUDIT` failure opens a maintenance incident and blocks maintenance
   completion for the affected scope.
4. No failure path may mutate transaction outcome, commit ordering, or
   visibility truth.
5. Page-spot audit findings are read-only evidence; no inline repair or page
   rewrite is allowed on the audit lane.

## Deterministic error classes

| Code | Condition |
| --- | --- |
| `SWEEP_POLICY_INVALID` | resolved policy lane or binding is invalid |
| `SWEEP_EVIDENCE_LOCAL_PERSIST_FAILED` | mandatory local evidence could not be persisted |
| `SWEEP_EXPORT_QUEUE_SATURATED` | backlog prevents further export enqueue under configured bounds |
| `SWEEP_SCHEMA_RETIREMENT_BLOCKED` | retired schema artifact cannot be pruned under active retention policy |
| `SWEEP_PAGE_AUDIT_MODE_INVALID` | configured page-audit mode is unsupported |

## Required tests

1. policy resolution determinism by object/table/schema/family/retention scope
2. `lane_mask` and `strict_audit` persistence across restart
3. local evidence persistence before prune for non-`NORMAL` lanes
4. external export failure does not alter committed cleanup truth
5. transactional DDL retirement obeys schema-retention policy
6. page-audit downgrade under foreground pressure follows policy and remains
   read-only
7. role/stage transitions remain audit-visible even when one `SweepManager`
   hosts multiple roles

## Cross-section references

- `GC_SWEEP_ALGORITHM.md`
- `SWEEP_AUDIT_EXPORT_AND_SHADOW_CAPTURE.md`
- `../20_Diagnostics_Audit_and_Observability/AUDIT_EXPORT_SINKS_RETENTION_AND_IMMUTABILITY.md`
- `../24_Catalog_Model_and_Virtual_Overlays/CATALOG_TABLE_SCHEMA_FORENSIC_AUDIT_AND_SHADOW_CAPTURE.md`
