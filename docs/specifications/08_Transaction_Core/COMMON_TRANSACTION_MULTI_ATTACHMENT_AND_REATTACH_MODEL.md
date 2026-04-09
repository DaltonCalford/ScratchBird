Status: reconstructed_required_with_current_substrate

# Common Transaction Multi Attachment and Reattach Model

## Purpose

This document defines the canonical model for transaction reattachment, dormant transaction handles, and common-transaction use across more than one connection or attachment.

## Canonical Rule

ScratchBird is always in a transaction context. A connection may retain, detach from, or rebind to a transaction handle, but there is never a non-transactional execution state.

## Current Substrate and Reconstructed Expansion

The current substrate already proves:

- dormant detach from a live transaction context
- token-based reattach to the same transaction context
- restart-time replacement reattach semantics

The reconstructed required model extends this to explicit common-transaction use across two or more connections or attachments under controlled security and capability rules.

## Canonical Common-Transaction Handle

The common-transaction handle shall preserve:

- transaction identity
- creator principal identity
- transaction attributes and isolation policy
- snapshot identity or snapshot lineage
- owning database identity
- handle generation
- attach count
- dormant or active state

## Multi-Attachment Rule

Two or more connections may bind to the same common transaction only when all of the following hold:

- they target the same database identity
- they satisfy the principal-match and capability rules
- the common transaction handle is still valid
- the handle is not quarantined, retired, or replaced

Multi-attachment does not create multiple independent transactions. It creates multiple attachments to one transaction truth.

## Dormant Reattach Rule

A connection may detach from a transaction while leaving the transaction alive as a dormant common handle. Reattach restores the connection into that transaction context rather than creating a new transaction.

## Restart Replacement Reattach Rule

If server or process restart occurs, the original in-process attachment cannot be resurrected verbatim. The canonical behavior is restart replacement reattach:

1. validate the persistent reattach handle identity
2. validate principal and database identity
3. reconstruct or reopen the transaction context from canonical transaction truth
4. bind the new live attachment to the same transaction identity or its restart-safe replacement form

This is not log replay and not a new unrelated transaction.

## Visibility Rule

All attachments bound to the same common transaction share:

- transaction identity
- visibility context
- commit or rollback outcome
- savepoint lineage where admitted by the handle model

They do not receive independent commit histories.

## Commit and Rollback Rule

If any bound attachment commits the common transaction:

- that transaction is retired as committed
- all attachments to that transaction lose active transactional use of the retired handle
- each attachment immediately enters its next transaction context per the always-in-transaction rule

If any bound attachment rolls back the common transaction, the same retirement rule applies with rollback outcome.

## Handle Invalidity Rule

Reattach or multi-attachment shall fail closed when:

- the handle belongs to a different principal or database
- the handle generation is stale
- the transaction is already retired
- the transaction is quarantined after recovery or validation failure
- required restart reconstruction evidence is missing

## Non-Guarantees

This file does not claim that unrestricted arbitrary shared transactions are already fully implemented in current code. It defines the canonical required model, grounded by the current dormant-reattach and restart-rebind substrate.
