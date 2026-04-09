# System Catalog and Metadata Ownership

## Ownership table

| Surface | Canonical role |
| --- | --- |
| System catalog rows | Canonical object identity and durable metadata truth |
| Canonical object-definition rows | Canonical normalized definition and compile/bind metadata truth |
| Dependency rows | Canonical inter-object dependency truth |
| Schema-epoch rows | Canonical committed schema publication-order truth |
| Security-policy epoch rows | Canonical committed security publication-order truth |
| Publication catalog rows | Canonical publication/subscription metadata truth for that feature family |
| Runtime transaction rows | Canonical evidence of in-flight and terminal transaction state |
| Metadata caches | Derived acceleration only |
| Permission caches | Derived security acceleration only |
| Parser-assist bulk cache | Derived committed mirror only |
| Statistics | Derived planning aid only |

## Transaction ownership rules

All catalog and metadata ownership is transaction-scoped:
- there is always an active transaction
- catalog mutation belongs to the active transaction
- publication occurs only when that transaction commits
- rollback retires all uncommitted metadata ownership effects and begins the next transaction from the prior committed baseline

## Security ownership boundary

Security metadata ownership includes at least:
- user, role, and group rows
- object and column permission rows
- row policy rows
- auth provider and auth policy rows
- MFA rows
- secret-reference rows
- security-policy epoch rows

These rows own durable security metadata truth.
They do not own cache answers or parser-side heuristics.

## Refusal rules

The system must reject any design that would:
- let a cache or statistics row override committed catalog truth
- let a security cache outrank committed security epoch truth
- let a parser bulk mirror become authoritative for current-transaction uncommitted `DDL`
- publish security ownership effects without a committed security epoch when the feature family requires one
