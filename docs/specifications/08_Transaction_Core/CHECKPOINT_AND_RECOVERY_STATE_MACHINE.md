# Checkpoint And Recovery State Machine

## Status
Authoritative current checkpoint and restart state-machine contract.

## Ownership matrix
| Surface | Current owner | Canonical responsibility |
| --- | --- | --- |
| checkpoint phase persistence | `Database` | persist and recover checkpoint phase and queue-rebuild state |
| restart classification | `Database` | classify startup path before ordinary runtime admission |
| TIP normalization | `TransactionManager` | normalize transaction state and reconcile durable contradictions before visibility is trusted |
| write-admission fencing | `Database` | fence writes while startup state is incomplete or unsafe |
| durable header transaction-state persistence | `TransactionManager` | persist oldest-active, oldest-snapshot, and latest-commit restart metadata |

## Canonical checkpoint and recovery progression
| State or step | Primary authority | Required behavior |
| --- | --- | --- |
| startup checkpoint-state load | `Database` | load checkpoint state before ordinary runtime admission |
| queue rebuild or queue reuse decision | `Database` | rebuild or reuse restart queue deterministically |
| clean-checkpoint validity check | `Database` | trust clean state only after explicit validation |
| `CAPTURING_HORIZONS` checkpoint phase persistence | `Database` | persist phase before later checkpoint work |
| TIP normalization before ordinary visibility | `TransactionManager` | normalize transaction state before visibility is trusted |
| durable commit-sequence reconciliation | `TransactionManager` | reconcile durable commit order before ordinary runtime admission |
| repaired recovery downgrade on contradictory clean state | `Database` and `TransactionManager` | fail safe instead of trusting contradictory durable state |

## Negative-path contract
- invalid clean markers must be cleared rather than trusted
- contradictory checkpoint or restart state must not bypass repair classification
- transaction visibility must not be trusted until TIP normalization and durable commit-sequence reconciliation complete
- unrecoverable prepared contradictions must classify as corruption stop, not ordinary repaired recovery

## Implementation code map
- `ScratchBird/src/core/database.cpp:459`: checkpoint-state load during startup
- `ScratchBird/src/core/database.cpp:5344`: startup combines page scan and checkpoint state
- `ScratchBird/src/core/database.cpp:5349`: startup queue rebuild state
- `ScratchBird/src/core/database.cpp:5415`: startup outcome selection
- `ScratchBird/src/core/database.cpp:5502`: clean-checkpoint validity check
- `ScratchBird/src/core/database.cpp:5529`: write admission can be fenced during startup
- `ScratchBird/src/core/database.cpp:5547`: invalid clean marker can be cleared
- `ScratchBird/src/core/database.cpp:5569`: checkpoint run initialization and `CAPTURING_HORIZONS`
- `ScratchBird/src/core/transaction_manager.cpp:327`: durable commit-sequence reconciliation
- `ScratchBird/src/core/transaction_manager.cpp:380`: startup TIP normalization invoked
- `ScratchBird/src/core/transaction_manager.cpp:2335`: persist restart header transaction-state metadata
- `ScratchBird/src/core/transaction_manager.cpp:3854`: normalize startup TIP states
- `ScratchBird/src/core/transaction_manager.cpp:3963`: contradictory clean state downgraded to repaired recovery
- `ScratchBird/src/core/transaction_manager.cpp:3997`: invalid prepared contradiction treated as corruption

## Cross-file rule

`STARTUP_RECOVERY.md` owns startup condition and outcome vocabulary.
This file owns checkpoint phase, owner split, and ordered restart progression.
