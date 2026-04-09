Status: reconstructed_required

# Row Column Domain Security Masking Evaluation Order Model

## Purpose

This document defines the required evaluation order for row security, column visibility, domain security, and masking so result publication remains deterministic and auditable.

## Canonical Evaluation Order

For every read or write request, ScratchBird shall evaluate policy in this order:

1. principal binding and effective authorization expansion
2. object invocation or direct-object access validation
3. row-level eligibility
4. column-level visibility or mutation eligibility
5. domain-level policy
6. masking transformation
7. result publication or mutation execution

## Row-Level Rule

Row-level policy decides whether a row is eligible to participate in the request at all. Ineligible rows shall not leak through column masking as placeholder rows unless a separately defined redaction contract requires it.

## Column-Level Rule

For rows that survive row-level evaluation, column-level policy decides:

- column visible
- column writable
- column referenceable
- column executable if derived through a callable surface

## Domain-Level Rule

Domain policy applies after object and row or column admission but before final publication. Domain policy may:

- refuse access
- require masking
- require additional capability

## Masking Rule

Masking transforms visible data only after the row and column have been admitted. Masking shall never be used to bypass a row-level or column-level deny.

## Write Rule

For writes, the same evaluation order applies conceptually:

1. caller authorized
2. target rows eligible
3. target columns mutable
4. domain policy admits the new value
5. mutation executes under MGA transaction rules

## Sandbox Interaction

Security-definer or schema-sandbox surfaces may change which object grants are consulted, but they shall not reorder row, column, domain, and masking evaluation.

## Audit Requirements

The engine shall be able to report which stage caused:

- row removal
- column suppression
- domain refusal
- masking transformation
- final request refusal

## Non-Guarantees

This file does not define the concrete policy language. It defines the canonical evaluation order that all policy languages and emulation layers must obey.
