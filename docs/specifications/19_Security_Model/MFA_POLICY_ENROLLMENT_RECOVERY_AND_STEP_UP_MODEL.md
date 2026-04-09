# MFA Policy Enrollment Recovery and Step Up Model

## Purpose

Define the multi-factor authentication model across catalog policy, stored enrollments, recovery codes, and runtime challenge handling.

## MFA Policy

An MFA policy may define:

- primary factor type
- recovery-code allowance
- break-glass allowance
- maximum challenge attempts
- challenge TTL
- step-up TTL

## Enrollment Model

An MFA enrollment binds:

- enrollment identity
- account identity
- MFA policy identity
- factor type
- primary enrollment status
- enrolled status
- secret presence
- factor-specific parameters such as TOTP digits, period, and look-ahead window

## Secret Handling

Enrollment secrets are stored through secured catalog persistence and shall not appear as plaintext on disk.

Current proof exists for encrypted or protected storage of TOTP seed material rather than raw plaintext persistence.

## Recovery Codes

Recovery codes carry:

- account identity
- MFA policy identity
- code hash
- break-glass flag
- use count
- max uses
- cooldown

Consumption of a recovery code is a state transition, not a stateless comparison.

## Runtime MFA Resolution

At runtime the server shall:

1. resolve the principal account from connection context
2. load the bound authentication policy
3. determine whether MFA is required
4. load the specific MFA policy when present
5. determine challenge limits and step-up TTL
6. ensure a primary enrollment exists or fail closed
7. issue a challenge when MFA is required

## TOTP Flow

Current code-backed TOTP runtime behavior includes:

- primary enrollment selection
- environment-backed enrollment bootstrap when catalog enrollment is absent in controlled cases
- TOTP verification
- enrollment last-verified update on success

## Recovery and Break-Glass Flow

When TOTP verification fails and policy allows recovery codes, hashed recovery-code validation may succeed instead.

Break-glass recovery is separately flagged and auditable.

## Challenge Session State

Pending MFA session state includes:

- active flag
- username
- completed first-factor method
- account identity
- MFA policy identity
- recovery and break-glass allowances
- step-up TTL
- challenge identity
- challenge method name
- current attempts
- maximum attempts
- challenge payload

## Fail-Closed Rules

The engine shall fail closed when:

- MFA policy resolution fails
- MFA policy lookup fails
- enrollment lookup or bootstrap fails
- secret material is invalid
- challenge limits are exceeded
- recovery validation errors occur outside ordinary invalid-code outcomes

## Current Proof and Rebuild Boundary

Current code and tests prove:

- catalog-backed MFA policy CRUD
- enrollment CRUD
- recovery-code consumption rules
- runtime MFA challenge issuance
- TOTP verification and last-verified update
- step-up TTL tracking

This specification reconstructs the broader product rule that MFA is a first-class authentication stage, not an optional UI adornment.
