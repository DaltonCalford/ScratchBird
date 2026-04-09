Status: reconstructed_required

# Common Transaction Attachment Security and Principal Match Model

## Purpose

This document defines the security requirements for reattaching to a transaction handle or binding multiple connections to one common transaction.

## Canonical Rule

A common transaction handle is a security-sensitive capability. Reattach or additional bind is allowed only when principal, database, and policy checks succeed.

## Principal Match Rule

The canonical principal match shall validate:

- local bound principal identity
- required role or capability state for the transaction
- database identity
- any step-up or elevated capability required when the handle was issued

## Cross-Principal Rule

Binding a second connection from a different principal to an existing common transaction is refused unless there is an explicit canonical policy allowing delegated attachment for that handle class.

## Audit Rule

The engine shall audit:

- handle issued
- dormant detach
- reattach accepted
- reattach refused
- additional attachment accepted
- additional attachment refused
- retirement by commit or rollback

## Refusal Classes

The canonical refusal classes are:

- principal mismatch
- database mismatch
- expired elevated state
- handle retired
- handle quarantined
- delegated attachment not permitted

## Non-Guarantees

This file does not require delegated multi-principal transactions by default. The safe default is same-principal binding only.
