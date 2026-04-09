# Background Maintenance and Recovery Interaction

This file defines how maintenance surfaces are allowed to interact with section 35 durability and recovery truth.

## Maintenance interaction matrix

| Maintenance surface | Allowed influence on durability or recovery | Forbidden interpretation |
| --- | --- | --- |
| background writer | may drain dirty pages and reduce checkpoint flush debt | not independent recovery truth |
| shutdown quiesce path | may stop concurrent writeback before clean-shutdown publication | not a substitute for checkpoint completion |
| page-manager FSM flush | persists allocation metadata after clean-shutdown publication | not equivalent to a full checkpoint |
| sweep or GC publication | may record sweep generation and checkpoint generation adjacency | not redo or replay authority |
| derivative write-after export | may support downstream audit or replication surfaces | not write-ahead recovery truth |
| catalog checkpoint history rows | provide evidence of checkpoint execution and failure reason | not stronger than system-state control-page truth |

## Shutdown ordering contract

On shutdown, the implementation must preserve this ordering:

1. determine whether clean-shutdown publication is eligible
2. if buffer pool exists, quiesce the background writer before clean-shutdown publication
3. if clean-shutdown eligible, execute clean-shutdown publication
4. else flush all dirty pages and sync without publishing a clean-shutdown marker
5. flush page-manager state after clean-shutdown publication or broad flush path so FSM state reflects shutdown-time allocation truth
6. if clean-shutdown eligible, drain dirty pages through the checkpoint boundary and sync again

## Maintenance-to-recovery rules

1. Maintenance surfaces may tighten recovery posture but may not relax it.
2. Background writer activity does not let startup skip persisted classification.
3. Sweep publication may carry checkpoint-generation adjacency for audit or cleanup reasoning, but it does not become recovery truth.
4. Derivative write-after artifacts remain downstream of committed MGA truth and recovery never replays them as redo.
5. Queue rebuild or writeback incidents discovered by maintenance must survive into startup classification.

## Required implementation separations

1. Background writer quiesce must be separate from checkpoint phase publication.
2. FSM flush must be separate from control-page checkpoint publication.
3. Sweep publication, audit emission, and derivative export must be separate from startup recovery logic.
4. Checkpoint history rows must be treated as evidence, not as the only source of checkpoint truth.

## Explicit non-guarantees

- maintenance does not replace the durability model
- sweep does not repair all persistence anomalies
- background writeback completion does not by itself authorize a clean-shutdown claim
- derivative export success or failure does not define restart correctness
