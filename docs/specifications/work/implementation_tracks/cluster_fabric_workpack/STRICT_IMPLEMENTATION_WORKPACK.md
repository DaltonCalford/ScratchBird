# Strict Implementation Workpack: Cluster Fabric (Alpha)

## Scope
Implement parserless ScratchBird `<->` ScratchBird UDR fabric exactly as defined by sections `17/21/24/25/26/27/28/29`.

## Non-Negotiable Execution Rules
1. Execute tickets in numeric order only.
2. Do not start a ticket until all dependencies are complete and evidence files exist.
3. Do not skip required artifacts; filenames are case-sensitive.
4. Engine path remains parserless and SQL-free for fabric data plane.
5. Passthrough query tasks must carry SBLR artifact references only.
6. One fabric link must support many sessions; each session must enforce isolated transaction ownership.
7. Cluster fabric channels require mTLS.

## Common Evidence Files (Required For Every Ticket)
- `RUN_MANIFEST.json`
- `SPEC_TRACEABILITY.csv`
- `IMPLEMENTATION_NOTES.md`
- `TEST_RESULTS.md`
- `CHECKSUMS.sha256`

Common evidence output directory pattern:
- `docs/specifications/work/implementation_tracks/cluster_fabric_workpack/evidence/<ticket_id>/`

## Ordered Ticket List
Use `ORDERED_TASK_TICKETS.csv` as the execution sequence source of truth.

## Gate Execution Rule
After all tickets pass local exit criteria, execute and publish gate evidence for:
- `P17-FABRIC-01` through `P17-FABRIC-04`
- `T26-I`
- `T27-G`
- `P28-FABRIC-01` through `P28-FABRIC-03`
- `R29-FABRIC-01` through `R29-FABRIC-04`

Use `GATE_EVIDENCE_MATRIX.csv` for exact filenames and directories.

## Stop Conditions (Must Fail Fast)
1. Any attempt to route fabric data-plane execution through parser SQL execution path.
2. Any ticket output missing one or more required evidence files.
3. Any non-deterministic state transition for link/session/transaction/task states.
4. Any ticket that violates ordering or missing dependency checks.
