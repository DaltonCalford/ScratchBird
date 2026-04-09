# Cluster Secret Partitioning and Shared Rights Model

Status: reconstructed_required_with_current_substrate

## Purpose

This file defines the required ScratchBird model for:
- distributed management and cluster secret handling
- secret fragmentation or partitioning
- shared user, role, group, and policy state across managed deployments

## Canonical cluster-secret rule

Shared encryption, decryption, or management secrets must not default to full plaintext fan-out.

The canonical model is:
- local sealed share retained by the managed database or server-local manager
- additional sealed fragments, references, or recovery materials retained by cluster management substrate
- controlled reconstruction only under authorized recovery or deployment procedure
- audit evidence for activation, rotation, recovery, and retirement

## Current code-backed floor

Current shipped authority is not full threshold fragmentation, but it is stronger than “just store plaintext everywhere”.

Current code-backed floor includes:
- mandatory manager control-plane secret rather than anonymous cluster control
- secret-indirection surfaces such as `auth_secret_uuid`
- encrypted MFA secret persistence
- separation between endpoint or configuration identity and secret-bearing material

Canon consequence:
- any future fragmentation or shard model must extend these indirection and fail-closed anchors
- it must not regress them into plaintext fan-out or unauthenticated control

## Secret operation privileges

Separate privileges are required for:
- inspect secret metadata
- rotate secret version
- stage secret deployment
- activate secret version
- reconstruct or break-glass recover secret material
- retire or revoke secret version

Inspection privilege does not imply plaintext disclosure privilege.

## Shared-rights rule

Cluster-wide or management-wide shared authorization state may include:
- users
- roles
- groups
- grant bundles
- policy objects
- external-identity mapping state

But effective authorization must still be computed locally against canonical security objects, local policy version, and local refusal state.

Current code-backed consequence:
- local refusal state remains authoritative even when remote or shared identity inputs exist
- provider-returned identity attributes and external groups are mapping inputs, not direct grants

## Non-negotiable rules

- a cluster participant must not self-grant rights merely by cluster membership
- secret-fragment metadata is not equivalent to plaintext secret authority
- shared-rights propagation must preserve deny, masking, and sandbox semantics
- remote management privilege is separate from local data-path privilege

## Current-versus-required split

Current code-backed anchors prove secret reference handling and authenticated manager control-plane secrets.
Full threshold or fragment runtime is still reconstructed required behavior.

Current code-backed anchors also prove that:
- manager startup and MCP auth are already fail-closed on missing secret material
- shared-rights propagation is not current permission to bypass local row, column, domain, or sandbox controls
