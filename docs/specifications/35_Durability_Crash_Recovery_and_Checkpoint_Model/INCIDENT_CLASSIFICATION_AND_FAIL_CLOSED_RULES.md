# Incident Classification and Fail-Closed Rules

Status: current_authority

## 1. Governing rule

Durability and recovery incidents must be classified explicitly and handled fail closed.
The engine must not invent permissive success when durable transaction or checkpoint truth is missing.

## 2. Canonical incident classes

The current durability and recovery surface includes at least these incident classes:

1. invalid transaction state transition
2. TIP and CLOG disagreement requiring reconciliation
3. incomplete active transaction found at restart
4. prepared state without durable prepared evidence
5. startup quarantine forcing read-only admission
6. checkpoint-marker debt discovered during reconstruction
7. page corruption or malformed page family
8. XID wraparound pressure or hard block
9. lock timeout and deadlock in transaction-end or statement-restart paths
10. missing statement snapshot for read-consistency restart

## 3. Fail-closed rules

The engine must fail closed when:

1. commit or rollback is requested for a non-active transaction
2. a required bootstrap page is malformed or missing
3. transaction inventory cannot be reconciled safely
4. read-write admission is requested while startup quarantine is active
5. a statement restart requires snapshot scope but no valid snapshot exists
6. a page family cannot be validated from its declared type
7. XID wraparound has reached hard-block threshold

## 4. Repair versus admission boundary

Some incidents allow repair or normalization before admission.
Others require refusal.

Examples of repairable or normalizable cases:

1. startup conversion of stale `ACTIVE` to terminal state
2. CLOG synchronization to TIP truth
3. checkpoint candidate queue rebuild from page debt

Examples of refusal cases:

1. malformed bootstrap map
2. unrecoverable page corruption in required control structures
3. hard XID wraparound block
4. transaction-mode changes during forensic replay

## 5. Negative requirements

The following are not canonical:

1. silently treating unknown durability state as committed
2. retrying around missing durable state without classification
3. downgrading hard corruption to advisory only
4. invoking WAL replay as the recovery answer

## 6. Implementation contract

Any implementation against this file must prove:

1. incidents are classified explicitly
2. hard durability and recovery errors refuse admission or acknowledgement
3. repairable cases are normalized through published state-reconciliation procedures
4. WAL is not used as the escape hatch for undefined behavior
