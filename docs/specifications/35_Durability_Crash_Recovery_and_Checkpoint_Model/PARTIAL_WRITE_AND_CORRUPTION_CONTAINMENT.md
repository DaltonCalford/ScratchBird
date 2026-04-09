# Partial Write and Corruption Containment

This file defines the bounded containment model for partial-write and corruption incidents.

## Detection anchors

| Detection anchor | Detection meaning | Default posture |
| --- | --- | --- |
| system-state page type mismatch | bootstrap control surface cannot be trusted | fail_closed |
| system-state checksum mismatch | bootstrap control surface cannot be trusted | fail_closed |
| page checksum mismatch on non-control page | page damage exists | bounded_containment unless escalation proves fatal |
| PAGE_CORRUPT | canonical page-level corruption detected | bounded_containment or fail_closed |
| DATA_CORRUPTED | payload or record corruption detected | bounded_containment or fail_closed |
| INDEX_CORRUPTED | index structure damage detected | bounded_containment |
| open writeback incident | previous durable publication may be incomplete | at least write_fenced |

## Containment decision algorithm

1. Validate the system-state page before trusting any persisted startup or checkpoint metadata.
2. If the control page fails type or checksum validation, classify the incident as fatal and refuse open.
3. Else classify page, data, and index findings into repairable, quarantinable, relinkable-only, or fatal buckets.
4. If quarantine action requires refusal to open, refuse open.
5. Else if writeback incident state remains open, escalate posture to at least write_fenced.
6. Else if repairable or relinkable-only damage exists, degrade the service state to at least DEGRADED_READ_WRITE.
7. Else if only dirty-shutdown normalization is required, remain in bounded recovery posture.
8. Never continue on the basis of assumed repair success.

## Containment action matrix

| Scenario | Required action | Forbidden action |
| --- | --- | --- |
| control-page corruption | refuse open | reconstruct control state from speculation |
| repairable page damage | open only in degraded posture and preserve findings | silently discard damage and report normal service |
| quarantinable page damage | isolate damaged scope and tighten service posture | route writes through the damaged scope as if healthy |
| writeback failure residue | preserve write fence or degraded posture until reconciled | clear write fence because a process restarted |
| index-only corruption | allow bounded containment only if data truth remains classifiable | claim full consistency of the damaged index family |

## Partial-write boundary

A partial write is treated as a durability-stage anomaly, not as permission to invent redo.
An implementation may only claim successful containment if it can do all of the following:

1. detect the anomaly through existing page or control validation
2. classify the anomaly into a bounded incident class
3. preserve or tighten service posture
4. avoid widening recovery claims beyond persisted truth

If any one of those conditions is missing, the implementation must fail closed.

## Repair boundary

1. Detection is not the same as repair.
2. Quarantine is not the same as repair completion.
3. Restart classification is not the same as post-repair certification.
4. Operator tools may exist later, but section 35 does not authorize automatic repair completion claims.

## Explicit non-guarantees

- no universal automatic repair guarantee
- no guarantee that degraded read-write service preserves all damaged structures
- no permission to treat restart as proof that corruption is gone
- no donor-style media-recovery implication
