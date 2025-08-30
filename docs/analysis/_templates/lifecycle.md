---
title: <Object> Lifecycle
status: draft
spec_refs: []
---

## States
- Created → Registered → Active → Modified → Checkpointed → Dropped → GC’d

## Transitions
- Create: <validation, catalog, WAL, page alloc, locks>
- Alter: <catalog change, data migration>
- Drop: <ref checks, WAL tombstone, reclaim/GC>
- Recovery: <redo/undo implications>

## Diagrams
```mermaid
stateDiagram-v2
  [*] --> Created
  Created --> Registered
  Registered --> Active
  Active --> Modified
  Modified --> Checkpointed
  Active --> Dropped
  Dropped --> GCd
  GCd --> [*]
```

## Observability

## Implementation References
- `ScratchBird/<path>:<start>-<end>` — <what>

## Spec Trace
- [REQ-...](../../traceability/spec/requirements.md#...)
