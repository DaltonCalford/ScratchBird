Status: reconstructed_required

# Weak Donor Divergence Scan Quiesce and Final Cutover Model

## Purpose

This document defines the canonical final-cutover procedure for weak donors whose transactional guarantees are insufficient for native zero-gap replication.

## Canonical Rule

Final cutover for a weak donor requires one of:

- donor quiesce with verified no-write window
- application fence with verified no-write window
- divergence scan proving no unaccounted differences remain

If none of these can be proven, automatic cutover is non-conforming.

## Divergence Scan Inputs

The divergence scan shall compare:

- source object identity
- extracted row or object counts
- target ingest counts
- source-side high-water markers if available
- target-side replay high-water markers
- sampled or full data digests where configured

## Canonical Final-Cutover Sequence

1. freeze the intended cutover target set
2. drain replayable backlog
3. establish donor quiesce or fence if available
4. execute divergence scan
5. classify remaining uncertainty
6. admit or refuse final cutover
7. preserve the cutover decision and evidence

## Quiesce Rule

Quiesce shall be recorded with:

- start time
- end time
- scope
- actor or mechanism establishing the fence
- proof that writes were blocked or absent during the protected window

## Divergence Classes

The divergence scan shall classify results as:

- `NO_DIVERGENCE`
- `MINOR_REPLAYABLE_DIVERGENCE`
- `NON_REPLAYABLE_DIVERGENCE`
- `UNVERIFIABLE`

`NON_REPLAYABLE_DIVERGENCE` and `UNVERIFIABLE` forbid automatic final cutover.

## Operator Override Rule

If policy permits manual override, the override shall:

- preserve the divergence class
- preserve the source of override
- preserve the reason for accepting residual risk

The override does not erase the original divergence evidence.

## Target-Side Rule

Once cutover is committed on ScratchBird, the target state becomes ordinary MGA-managed truth. The weak-donor classification remains historical migration evidence only.

## Non-Guarantees

This file does not promise every weak donor can reach `NO_DIVERGENCE`. It defines the evidence required to decide honestly.
