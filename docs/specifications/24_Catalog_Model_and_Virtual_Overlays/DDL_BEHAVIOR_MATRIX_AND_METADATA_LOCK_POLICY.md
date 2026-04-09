# DDL Behavior Matrix and Metadata Lock Policy

## Governing model

All `DDL` is transaction-scoped. ScratchBird is always inside a transaction. `COMMIT` and `ROLLBACK` immediately start the next transaction. `AUTOCOMMIT` means a successful statement is followed by `COMMIT`; if a statement errors, no commit occurs and the transaction remains active.

Metadata locking is not a side channel separate from transaction control. It is part of the same MGA and lock-manager model that governs `DML`.

## Behavioral matrix

| Concern | Required behavior |
| --- | --- |
| Transaction scope | `DDL` and `DML` share one transaction lifecycle; there is no `DDL` auto-commit exception |
| Visibility to same transaction | The owning transaction may observe its own uncommitted schema changes through its transaction-local overlay |
| Visibility to other transactions | Other transactions may observe only the last committed schema epoch |
| Commit boundary | Commit publishes the new schema epoch if transactional `DDL` batches exist |
| Rollback boundary | Rollback retires all uncommitted schema changes and restores the transaction-start epoch |
| Autocommit success | Statement succeeds, commit runs, publication occurs if needed, next transaction starts |
| Autocommit error | No commit occurs; the current transaction remains active and may be corrected or rolled back |
| Savepoints | `DDL` participates in the transaction model; if an operation cannot be safely rewound to an interior savepoint, it must fail closed rather than escape savepoint rules |
| Deadlock/timeout victim | Transaction rollback retires the uncommitted schema overlay and starts the next transaction |
| Recovery | Only committed schema epochs are durable publication truth |

## Metadata lock policy

### Lock ownership

- Metadata locks are transaction-scoped.
- Lock lifetime is bounded by transaction end or explicit savepoint-retreat handling, not by statement completion alone.
- Lock release on successful autocommit occurs only because the commit ends the transaction and starts the next one.

### Lock conflict handling

Conflict handling is delegated to the lock manager and transaction core:

- incompatible metadata activity may wait, fail immediately, or be selected as deadlock victim according to section `09`
- a victim rollback retires all uncommitted catalog mutation and schema publication state
- no conflict handler may expose half-published schema state to break a lock cycle

### Concurrent DDL and DML

The required behavior is:

1. readers and writers bind against a stable transaction-visible metadata baseline
2. schema-changing work acquires the necessary transaction-scoped metadata locks
3. same-transaction follow-on statements may use the local schema overlay
4. other transactions continue using their own committed snapshot until commit publication or restart/refusal logic applies
5. new binds after commit must use the newly committed schema epoch

This specification does not promise universal online `DDL`. Unsupported concurrent combinations must fail closed.

## Metadata lock classes

At minimum the implementation must distinguish these outcomes:

- `compatible`: work may proceed without retiring current metadata bindings
- `wait_required`: work must block until conflicting transaction outcome is known
- `restart_required`: work must abandon its current execution attempt and rebind against a fresh snapshot
- `refuse_required`: unsupported combination; do not guess

## Refusal rules

The engine must reject any implementation path that would:

- release metadata lock scope early while preserving uncommitted schema effects
- expose uncommitted schema mutation to another transaction through cache or helper surfaces
- auto-commit only because a statement is `DDL`
- leave a transaction after commit or rollback without immediately establishing the next transaction context
