# Audit and Forensic Access Policy

Status: current_authority

## Audit rules

- security-relevant bootstrap, authentication, and channel incidents must emit audit events
- audit events must preserve stable incident class while allowing wording changes
- secrets, password proofs, and reusable key material must never be logged
- redaction is mandatory for sensitive fields

## Forensic access rules

- forensic access is privileged and explicitly gated
- forensic capture is evidence-oriented and read-only with respect to production truth
- forensic channels must not become general-purpose administrative bypass paths

## Current Beta 1 local-engine closure

Current Beta 1 authority in package `06` is bounded to:

- append-only local audit evidence with mandatory redaction
- privileged replay-style forensic inspection that is read-only and bounded to
  retained local evidence
- fail-closed refusal when replay or derivative evidence is unavailable or the
  caller lacks the required privilege boundary

## Derivative and shadow operational privilege rules

The following privilege boundary is mandatory:

- derivative queue, shadow-group, restore-boundary, and failback-boundary
  inspection is privileged read access
- derivative retry or quarantine-release actions are privileged mutating access
- shadow promotion is privileged route-changing access
- failback inspection is privileged read access

Required rules:

- inspection privilege does not imply mutation privilege
- derivative retry does not imply shadow promotion privilege
- shadow promotion and any future failback execution privilege must be narrower
  than generic diagnostic access
- all such accesses must be auditable

## Audit requirements for derivative and shadow operations

The following operations must emit audit events:

- derivative retry request
- derivative quarantine release request
- shadow promotion request
- failback inspection request
- derivative or shadow access denial

Minimum audit fields:

- actor identity
- privilege class used
- target database identity
- target sink profile or shadow group identity when applicable
- action class
- allow or deny result
- stable incident class
