# Decision Record

Status: current_authority

## Current decisions

1. lock-manager authority lives in the engine core and is owned by Database
2. the implemented lock-mode vocabulary is PostgreSQL-like, not Firebird-style
3. the proven lock targets are DATABASE, TABLE, PAGE, and TUPLE
4. tuple locks are handled as strict-conflict resources in the audited lock-manager implementation
5. READ_COMMITTED_READ_CONSISTENCY statement restart is the canonical response for audited tuple-write conflicts in that mode
6. deadlock detection, wait history, and metadata lock observability are part of the implemented contract
7. predicate or range locking and certified serializable phantom prevention are not current implementation truth

## Rejected alternatives

- treating older Firebird lock-semantics prose as current authority
- advertising unsupported isolation guarantees through implication
- assuming a broader metadata-object lock family than the code proves
- inventing multi-node distributed lock-manager guarantees

## Change surface

- engine core LockManager
- Database startup and shutdown lifecycle
- TransactionManager restart and isolation handling
- StorageEngine tuple update locking
- observability and virtual metadata-lock surfaces

## Non-guarantees

- no predicate or range locking guarantee is made here
- no repeatable-read or serializable certification is made here
- no multi-node distributed lock-manager guarantee is made here
