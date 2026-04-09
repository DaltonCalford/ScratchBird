# Decision Record: TOAST and LOB Storage

## Current authoritative decisions

Table-owned TOAST is the current authoritative oversized-value implementation.

The canonical pointer contract is `ToastPointer`, not an abstract future LOB locator.

Chunk visibility and reclaim classification are MGA-aware and TIP-backed through `ToastVisibility`.

Delete paths are MGA-safe soft deletes that publish `xmax` rather than immediately removing chunk tuples.

Old oversized values may be retired through deferred cleanup during update and delete flows.

## Narrowed interpretations

`LOB`-named diagnostics and page-family support do not prove a complete standalone LOB subsystem.

The presence of `lob_uuid` inside `ToastPointer` does not widen the current section to a standalone LOB locator family.

The presence of `TOAST_ENCRYPTED` does not by itself prove a complete encryption policy, operator contract, or key-management model for standalone LOBs.

## Ruled-out claims

This section does not currently authorize a generic seek, read, write, or handle-oriented LOB API.

This section does not currently authorize standalone LOB relocate, resume, abort, or validate operations.

This section does not currently authorize a second oversized-value control plane outside the TOAST-first runtime.
