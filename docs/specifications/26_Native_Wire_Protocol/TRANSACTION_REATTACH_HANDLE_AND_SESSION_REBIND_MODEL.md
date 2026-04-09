Status: reconstructed_required_with_current_substrate

# Transaction Reattach Handle and Session Rebind Model

## Purpose

This document defines the canonical wire and session-binding model for reconnecting a client session to an existing transaction handle.

## Canonical Rule

Wire-level reconnect may carry transaction reattach intent, but the wire protocol does not redefine transaction truth. It only transports the rebind request into the engine-owned transaction model.

## Required Reattach Inputs

The rebind request shall preserve:

- database identity
- session identity or capability token
- transaction reattach handle
- principal identity evidence
- requested mode: dormant reattach or common-handle bind

## Rebind States

The protocol-facing result shall classify rebind as:

- `REBIND_ACCEPTED`
- `REBIND_REFUSED_HANDLE_UNKNOWN`
- `REBIND_REFUSED_PRINCIPAL_MISMATCH`
- `REBIND_REFUSED_DATABASE_MISMATCH`
- `REBIND_REFUSED_TRANSACTION_RETIRED`
- `REBIND_REFUSED_HANDLE_QUARANTINED`
- `REBIND_REFUSED_RESTART_RECONSTRUCTION_FAILED`

## Session Rule

Successful session rebind attaches the new live session to an existing transaction handle. It does not start an unrelated transaction and does not infer a commit boundary.

## Multi-Attachment Rule

If common-handle multi-attachment is admitted, the wire protocol shall transport only:

- the request to bind
- the resulting bind outcome

It shall not invent independent per-session transaction identities once binding succeeds.

## Restart Rule

After restart, the session may request rebind through the same wire path, but the server may return restart-specific refusal if canonical transaction reconstruction fails.

## Non-Guarantees

This file does not require one public SQL syntax. It defines the transport and result contract for driver and native-protocol rebind behavior.
