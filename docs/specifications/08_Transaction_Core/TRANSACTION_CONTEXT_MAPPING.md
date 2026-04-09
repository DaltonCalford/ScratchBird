# Transaction Context Mapping

Status: current_authority

## 1. Attachment-owned transaction context

The connection context is the attachment-local owner of transaction execution
state.

It binds together:

- current `xid`
- current transaction UUID
- durable boundary state
- protocol session identity
- dormant detach or reattach anchors where active
- effective isolation level
- effective read-only mode
- effective read-committed mode
- effective wait policy and timeout
- retained transaction snapshot
- statement snapshot
- savepoint stack
- autocommit flags
- security and session context
- schema epoch and transactional `DDL` staging

## 2. Context layers

### Attachment layer

Owns:

- session identity
- protocol session id
- role and security context
- default next-transaction settings
- autocommit policy
- parser and schema synchronization anchor
- pending dormant-reattach identifiers during reconnect

### Transaction layer

Owns:

- current `xid`
- transaction UUID
- retained snapshot and inventory anchors
- savepoint root
- temp-object lifetime
- staged metadata and `DDL`
- dormant lease and dormant preservation state when explicitly detached

### Statement layer

Owns:

- statement identifier
- statement text or prepared identity
- statement snapshot when required
- implicit statement savepoint when required
- statement restart classification

## 3. Transition rules

The context transitions are:

1. initialize -> open initial transaction
2. commit -> end current transaction -> apply staged settings -> open next
   transaction
3. rollback -> end current transaction -> apply staged settings -> open next
   transaction
4. prepare -> durable prepared handoff -> allocate fresh active boundary
5. dormant detach -> preserve or record current transaction for explicit
   reattach
6. dormant reattach -> restore live dormant context or open replacement
   transaction from persisted state
7. shutdown -> end current transaction without replacement begin

There is no ordinary transition to an empty transaction context between
statements.

## 4. Staged next-transaction settings

The connection context owns staged next-transaction settings.

Required staged settings:

- next isolation level
- next read-only flag
- next wait policy
- next lock-timeout value
- next read-committed mode
- next explicit snapshot-anchor request where supported

### Application rule

These settings are applied only at transaction-boundary transitions unless
`START TRANSACTION` explicitly requests immediate application through
`commit_outstanding`.

## 5. Statement mapping

Statement context is nested inside transaction context.

It owns:

- statement identifier
- statement source and hash
- statement snapshot when required
- statement savepoint when required
- statement restart state

Statement failure does not destroy the outer transaction context.

## 6. Session and security mapping

Session, user, role, schema, and policy state are attachment-owned context, but
some changes may stage for the next transaction boundary.

Examples:

- role-switch policy may require commit before switch
- session-context changes may stage for next boundary
- schema epoch changes are recorded in transaction lineage and metadata staging
- dormant reattach requires authenticated user continuity before session state
  is rebound

## 7. Autocommit mapping

Autocommit is attachment state, not a different transaction model.

Effective autocommit is:

- `autocommit_mode = true`
- `autocommit_suspended = false`

Even in that state the connection context still owns a current transaction and
the next replacement transaction after each successful command commit.

Autocommit sessions do not create a no-transaction mode merely because they
disconnect or reconnect.

## 8. Parser and catalog mapping

The parser-side committed catalog anchor is attachment-owned context adjacent to
transaction context.

Rules:

1. committed parser cache anchor advances only on committed schema-epoch
   publication boundaries
2. local uncommitted metadata overlay is transaction-owned context
3. `COMMIT` publishes local metadata overlay into the committed baseline and
   starts the next transaction
4. `ROLLBACK` discards local metadata overlay and starts the next transaction

## 9. Dormant reattach mapping

Dormant detach and reattach are context operations layered on the same
attachment and transaction model.

Attachment-owned dormant mapping includes:
- pending reconnect identifiers
- protocol session id used for dormant diagnostics
- authenticated principal continuity checks

Transaction-owned dormant mapping includes:
- dormant transaction id
- lease-expiry state
- last statement diagnostics
- retained locks and `ProcArray` membership when live dormant context is still
  resident

Restart replacement reattach is a new active boundary materialized from
persisted dormant session state. It is not a parser-created transaction and not
WAL-based resurrection.

## 10. Negative requirements

The following are not canonical:

1. authoritative transaction settings stored only in parser state
2. empty transaction context between ordinary statements
3. autocommit bypassing transaction context
4. dormant reattach by ordinary reconnect without explicit identifiers
5. restart-time replay of the original transaction from WAL-like authority

## 11. Implementation contract

Any implementation against this file must prove:

1. connection context owns current transaction state
2. statement context nests inside transaction context
3. staged settings are applied at transaction boundaries
4. autocommit is attachment policy layered on top of always-in-transaction
   behavior
5. dormant detach and dormant reattach remain attachment and transaction-owned
   context transitions, not parser-owned shortcuts
