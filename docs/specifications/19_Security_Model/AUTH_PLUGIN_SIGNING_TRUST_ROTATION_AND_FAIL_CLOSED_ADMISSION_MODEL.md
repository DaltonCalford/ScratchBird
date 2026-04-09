Status: reconstructed_required

# Auth Plugin Signing Trust Rotation and Fail-Closed Admission Model

## Purpose

This document defines the canonical trust and admission model for authentication plugins, including signature verification, trust-anchor rotation, and refusal semantics.

## Canonical Rule

Authentication plugins are admitted only through a fail-closed trust process. Unsigned, unverifiable, revoked, or incompatible plugins shall not be loaded for active authentication use.

## Admission Checks

Plugin admission shall verify:

- plugin identity
- plugin version or build identity
- signature presence
- signature validity
- trust-anchor validity
- revocation or deny state
- ABI compatibility
- policy permission to enable the plugin

## Trust Rotation Rule

Trust-anchor rotation shall preserve:

- old anchor set
- new anchor set
- effective transition window
- affected plugin identities
- audit evidence of the rotation decision

During rotation, the engine shall never enter a state where an unverifiable plugin is silently accepted.

## Refusal Classes

Plugin admission refusal shall be classified explicitly, including:

- `UNSIGNED`
- `INVALID_SIGNATURE`
- `REVOKED`
- `UNKNOWN_TRUST_ANCHOR`
- `ABI_INCOMPATIBLE`
- `POLICY_DISABLED`

## Runtime Rule

If a currently loaded plugin becomes non-admissible after trust rotation or revocation, policy shall define whether:

- existing authenticated sessions continue until expiry
- the plugin is disabled for new sessions only
- emergency disable occurs for all sessions

The chosen behavior shall be explicit and audited.

## Non-Guarantees

This file does not require one specific signing technology. It requires fail-closed admission, rotation traceability, and explicit refusal semantics.
