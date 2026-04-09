# Schema Change and DDL Publication State Machine

## Purpose

This file defines the canonical publication state machine for schema-changing and security-changing metadata work. All `DDL` in ScratchBird is transaction-scoped. ScratchBird is always inside a transaction boundary.

## Governing invariants

- Schema mutation follows the same transaction rules as `DML`.
- Security mutation follows the same transaction rules as `DML`.
- Firebird-style `MGA` is the authority model.
- Uncommitted schema or security mutation is visible only inside the owning transaction context.
- Committed publication occurs only at the transaction commit boundary.
- Rollback must retire all uncommitted mutation and restore the pre-transaction publication anchors.

## Durable publication artifacts

The canonical publication chain is:
1. transactional catalog row mutation
2. canonical object-definition persistence when applicable
3. dependency refresh when applicable
4. transaction-local staging
5. commit-bound schema epoch append for schema-changing work
6. commit-bound security epoch advance for security-changing work
7. terminal runtime transaction-state persistence

## Dual-anchor rule

Publication may advance one or both committed anchors:
- schema epoch
- security policy epoch

Rules:
- ordinary schema `DDL` may advance schema epoch without advancing security epoch
- grant, revoke, membership, auth policy, MFA policy, or RLS metadata changes must advance the relevant security epoch anchors
- mixed statements that affect both schema and security must publish both anchors consistently at commit

## Refusal rules

The engine must refuse or fail closed if any implementation path would:
- publish schema or security metadata outside a transaction boundary
- make uncommitted security state visible as committed metadata
- advance security publication on rollback
- allow a failed autocommit statement to advance schema or security publication state
