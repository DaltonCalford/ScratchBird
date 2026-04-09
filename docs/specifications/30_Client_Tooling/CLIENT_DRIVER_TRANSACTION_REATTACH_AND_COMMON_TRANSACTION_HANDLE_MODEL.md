Status: reconstructed_required_with_current_substrate

# Client Driver Transaction Reattach and Common Transaction Handle Model

## Purpose

This document defines the canonical client-driver behavior for preserving, reusing, and rebinding transaction handles.

## Canonical Rule

Drivers may preserve transaction-handle identity across disconnect and reconnect boundaries, but they shall treat the engine as authoritative for whether rebind is valid.

## Driver Responsibilities

The driver or client layer shall:

- preserve reattach-handle identity where policy allows
- preserve bound database identity
- preserve principal identity context needed for rebind
- distinguish dormant reconnect from ordinary new-transaction use

## Common-Handle Rule

If the client attaches multiple connections to one common transaction handle, the driver shall treat:

- commit as retiring the shared handle for all bound connections
- rollback as retiring the shared handle for all bound connections
- savepoint operations as operating inside the one shared transaction context

## Restart Rule

If reconnect occurs after server restart:

- the driver may present the saved reattach handle
- the driver shall accept that the server may refuse or replace the live attachment
- the driver shall not claim that the old process-local attachment was resurrected unchanged

## Error Handling Rule

A refused reattach shall not be silently converted into:

- a new unrelated transaction with the same client-visible identity
- an implicit commit
- an implicit rollback hidden from the caller

The refusal must be surfaced explicitly.

## Non-Guarantees

This file does not claim every current driver path already supports the full common-handle model. It defines the required client-side behavior for the feature family.
