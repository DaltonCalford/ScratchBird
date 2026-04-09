# Auth Provider Negotiation and External Group Sync Model

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the negotiation-time provider contract and the rule by which external provider identity is translated into ScratchBird authorization.

## Provider output contract

Current provider output already includes:
- user UUID
- username
- display name
- email
- external groups
- external provider user id
- disabled flag
- locked flag
- superuser flag
- authkey id

External identity data is part of the resolved principal, not free-form session decoration.

## Negotiation rule

A provider may expose only a subset of method families, but it must:
- advertise supported methods honestly
- reject unsupported methods explicitly
- honor transport restrictions
- honor client pinning and required-method policy
- fail closed on malformed negotiation payloads

Current code-backed policy anchors also prove:
- negotiated identity may be subject to `no_login_direct` style refusal
- proxy-assertion verification may be required before direct login is accepted
- provider success does not end evaluation when additional policy gates still apply

## External-group synchronization rule

External groups are identity input, not direct permission grants.

Algorithm:
1. accept provider-returned external group names only from an admitted provider
2. treat them as identity attributes of the resolved principal
3. map them through canonical ScratchBird security objects and policy state
4. derive effective roles, groups, or shared-rights bundles only through that mapping
5. preserve local deny, masking, and sandbox semantics after mapping

This prevents provider-returned group names from becoming uncontrolled authorization bypass.

## Current policy-bound negotiation consequence

Current code-backed security catalog and session behavior make these negotiation rules explicit:
- resolved principal identity is distinct from granted authority
- policy flags may still deny login after provider success
- proxy-assertion-only or no-login-direct posture is an admission rule, not a cosmetic tag

## Shared-rights interaction

When shared rights exist across managed deployments:
- identity source must be preserved
- policy version must be preserved
- local refusal or override state must remain visible
- remote-management authorization must remain separate from local data-plane access

## MFA and session interaction

Negotiated provider success may still be followed by:
- MFA step-up
- recovery-code challenge
- break-glass gating
- session transport restrictions

Provider negotiation therefore ends identity selection, not the full authorization decision.
