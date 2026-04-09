# Section 35 Test Contract

## Status

- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-30

## Current implementation-backed proof

- `tests/unit/test_executor_transaction_payload.cpp` proves checkpoint control
  persistence, clean-versus-dirty startup classification, queue rebuild capture,
  writeback-incident persistence, and fenced reopen behavior
- `tests/unit/test_mga_failpoint_replay.cpp` proves commit pre-TIP and
  post-TIP crash windows plus sweep checkpoint-loss replay behavior
- `tests/unit/test_transaction_manager.cpp` proves transaction begin and
  rollback refuse write-admission-fenced publication before unsafe durable
  mutation is claimed

Required evidence for future hardening:
- startup recovery evidence
- bounded checkpoint/restart interaction evidence
- corruption/partial-write classification evidence
- maintenance/recovery interaction evidence
- crash-window evidence for:
  - commit before durable page publication
  - commit after durable page publication and before terminal inventory state
  - commit after terminal inventory state and before acknowledgement
  - rollback during terminal publication
  - checkpoint phase interruption before and after dirty-drain boundaries
- prepared or limbo contradiction evidence covering:
  - durable prepared evidence retained
  - stale prepared evidence discarded
  - unresolved prepared contradictions fenced or refused
- explicit proof that restart is state reconciliation from durable page and
  transaction-inventory truth, not WAL or redo replay
- explicit proof that forced-write and ordered-write fences gate safe commit
  acknowledgement
- explicit proof that writeback incidents, disk-full conditions, and shadow
  durability failures do not permit illegal safe-mode commit success
- checkpoint maintenance markers in `kPrepared` are discarded on restart
- checkpoint maintenance markers in `kBoundaryDurable` or `kMergePending`
  recreate the admitted merge debt after restart
- marker and merged-target disagreement fences the object fail closed
- exact-family unique or primary-key work cannot enter checkpoint-bound delta
  reconciliation

Fail-closed rule:
- durability and recovery claims remain bounded until directly supported by executed artifacts
