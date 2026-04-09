# Startup Recovery

## Status
Authoritative current startup-recovery contract.

## Canonical startup decisions

Startup recovery must classify into one of these canonical outcomes:

- `CLEAN_CHECKPOINT_PATH`
- `CHECKPOINT_REBUILD_REQUIRED`
- `REPAIRED_RECOVERY`
- `FENCE_WRITES_UNTIL_SAFE`
- `CORRUPTION_STOP`

## Required behavior

The current implementation must satisfy these rules:
- startup loads checkpoint state and queue-rebuild state before full runtime admission
- clean-checkpoint fast-path trust is conditional, not assumed
- invalid clean-shutdown state is cleared and restart classification is downgraded to repaired recovery
- TIP normalization and durable commit-sequence reconciliation run before transaction visibility is trusted
- write admission can be fenced while startup state is incomplete or unsafe

## Startup decision table
| Condition | Canonical outcome | Required behavior |
| --- | --- | --- |
| checkpoint state loaded and valid | `CLEAN_CHECKPOINT_PATH` or `CHECKPOINT_REBUILD_REQUIRED` | rebuild or reuse queue state before full runtime |
| clean-shutdown marker and checkpoint state both valid | `CLEAN_CHECKPOINT_PATH` | do not bypass explicit validity checks |
| clean-shutdown marker invalid or contradicted | `REPAIRED_RECOVERY` | clear marker and downgrade instead of trusting stale clean state |
| TIP or commit-sequence repair required | `REPAIRED_RECOVERY` | normalize transaction state before ordinary visibility |
| write admission not yet safe | `FENCE_WRITES_UNTIL_SAFE` | unsafe startup must not admit writes |
| unresolved prepared contradiction or unrecoverable durable contradiction | `CORRUPTION_STOP` | do not admit ordinary runtime writes or visibility |

## Negative-path contract
- invalid clean markers must not produce a clean-start fast path
- missing or duplicate durable commit-sequence metadata must be reconciled before normal visibility is trusted
- unresolved prepared contradictions are corruption or repair events, not ordinary visible committed state

## Implementation code map
- `ScratchBird/src/core/database.cpp:459`: checkpoint-state load during startup
- `ScratchBird/src/core/database.cpp:5344`: startup combines page scan and checkpoint state
- `ScratchBird/src/core/database.cpp:5349`: queue rebuild state
- `ScratchBird/src/core/database.cpp:5415`: startup outcome selection
- `ScratchBird/src/core/database.cpp:5502`: clean-checkpoint validity
- `ScratchBird/src/core/database.cpp:5529`: write admission can be fenced during startup
- `ScratchBird/src/core/database.cpp:5547`: invalid clean marker can be cleared
- `ScratchBird/src/core/database.cpp:5569`: checkpoint phase persistence begins with `CAPTURING_HORIZONS`
- `ScratchBird/src/core/transaction_manager.cpp:327`: durable commit-sequence reconciliation
- `ScratchBird/src/core/transaction_manager.cpp:380`: startup TIP normalization invoked
- `ScratchBird/src/core/transaction_manager.cpp:3854`: normalize startup TIP states
- `ScratchBird/src/core/transaction_manager.cpp:3963`: repaired recovery downgrade
- `ScratchBird/src/core/transaction_manager.cpp:3997`: invalid prepared contradiction treated as corruption

## Ownership rule

`STARTUP_RECOVERY.md` owns the startup condition-to-outcome contract.
`CHECKPOINT_AND_RECOVERY_STATE_MACHINE.md` owns the phase and owner split.
