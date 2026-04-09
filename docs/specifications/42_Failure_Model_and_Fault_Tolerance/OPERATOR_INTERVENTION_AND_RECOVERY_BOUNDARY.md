# Operator Intervention and Recovery Boundary

This file owns the bounded operator-recovery model.

## Operator-recovery matrix

| Topic | Current state | Current truth | Explicit exclusion |
| --- | --- | --- | --- |
| operator-assisted recovery | current_authority | operator intervention may be required for corruption, quarantine, reconcile, or evidence-lane blockage | not fully autonomous recovery |
| diagnostics and support evidence | current_authority | support bundles, diagnostics, shadow manifests, and derivative export summaries provide bounded recovery evidence | not a universal guided repair console |
| MGA restart and reconcile | current_authority | operators recover from durable database state and inventory reconciliation, not by replaying WAL | not log-rebuild workflow |
| recovery orchestration | fail_closed | no broad orchestrated fleet recovery workflow is implied | not a fleetwide incident manager |
| documented runbook completeness | current_bounded | bounded runbook and gate evidence may exist | not a complete operator handbook guarantee |

## Canonical rules

1. operator involvement must be explicit where automation is not current truth
2. diagnostics and evidence surfaces aid classification and recovery but do not
   replace MGA truth
3. logical shadow capture is a local evidence lane that may be required before
   prune continues
4. `wal_after` segments and remote archive delivery may aid forensics and live
   archive use, but they are derivative downstream artifacts
5. recovery starts from the current database plus transaction inventory, not
   from rebuilding state out of a replay log

## Explicit non-guarantees

- no autonomous recovery platform
- no full guided repair console
- no complete operator-runbook coverage guarantee
- no WAL-log rebuild workflow guarantee
