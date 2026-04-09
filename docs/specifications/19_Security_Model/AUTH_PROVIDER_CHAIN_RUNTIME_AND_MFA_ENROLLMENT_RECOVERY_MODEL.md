# Auth Provider Chain Runtime and MFA Enrollment Recovery Model

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines:
- provider-chain evaluation
- auth policy intersection
- transport and method gating
- MFA policy application
- MFA enrollment and recovery-code handling

## Current auth-policy state

Current catalog auth policy state includes at least:
- `provider_chain`
- `mfa_required`
- `mfa_policy_id`
- lockout threshold, window, and duration
- password fallback policy
- allowed auth method mask and allowed method ids
- required auth method state
- allowed transport mask
- peer mode
- minimum SCRAM iterations
- weak-SCRAM upgrade marking
- client-pinning required and forbidden method lists
- channel-binding requirement flag

## Provider-chain runtime algorithm

Current code-backed runtime follows this structure:
1. resolve the catalog auth context for the principal
2. require a valid policy
3. apply lock-state and unlock-window checks
4. load auth providers from catalog
5. walk the policy provider chain in stable stored order
6. skip provider rows not found, disabled, or mismatched for the requested method
7. execute provider-specific adapter attempts
8. collect accept, reject, timeout, unavailable, or policy-deny outcomes
9. ask catalog runtime policy to evaluate the attempt set
10. require a selected accepted provider with a resolved principal
11. issue a login-session authkey
12. return authenticated user information plus issued authkey

Provider-chain order is authoritative policy state, not advisory metadata.

## Hard-fail and fallback rules

Providers or policies may distinguish:
- reject
- timeout
- unavailable
- hard-fail termination
- fallback-allowed continuation

A hard-fail provider may terminate the attempt on timeout or unavailability.
A fallback-permitted provider may allow evaluation to continue.
No tooling or client surface may soften a hard-fail policy.

## Transport and method gating

Auth policy currently defines transport classes:
- local
- IPC
- INET

It also defines method-family gating and optional required methods.

Rules:
- disallowed transport paths fail before provider success can authorize the session
- required methods must not be silently replaced with weaker methods
- client pinning and channel-binding requirements are policy gates, not hints
- weak SCRAM states may be marked for upgrade without silently downgrading enforcement

## MFA gate

After primary-provider success, MFA policy is applied.

Current code-backed session state proves:
- pending MFA challenge state per session
- per-session MFA verified status
- recovery-code and break-glass tracking
- step-up TTL state

Current catalog MFA state proves:
- MFA policy rows
- MFA enrollment rows
- MFA recovery-code rows
- attempt counts, cooldown, and break-glass controls

Rules:
- provider success is necessary but not sufficient when MFA is required
- an unverified MFA state is a deny state, not a warning state
- recovery codes are hashed and consumption-tracked
- exhausted, cooled-down, or disallowed recovery-code use fails closed

## MFA enrollment secret rule

MFA enrollment secrets are protected secret material.
They must not become ordinary plaintext catalog values.

Current code-backed rules include:
- accepted secrets must be valid base32
- encryption requires an active key manager
- stored form is encrypted, encoded, and TOAST-backed
- malformed decode or decrypt paths fail closed
- primary-enrollment uniqueness per account is enforced

## Audit consequence

At minimum the engine must signal:
- provider-chain success and failure
- lockout application and unlock
- hard-fail provider timeout or unavailability
- MFA-required denial
- recovery-code use
- break-glass use
- MFA secret storage or retrieval failure

## Non-guarantees

This file does not claim every external provider family is fully shipped today.
It defines the policy and runtime contract that the current auth policy catalog, provider runtime, and MFA session state already prove.
