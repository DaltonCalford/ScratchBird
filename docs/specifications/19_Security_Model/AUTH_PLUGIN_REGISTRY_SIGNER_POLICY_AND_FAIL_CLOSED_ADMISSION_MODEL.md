# Auth Plugin Registry Signer Policy and Fail Closed Admission Model

## Purpose

Define the signed auth-plugin registry, policy-controlled admission, and method-availability contract.

## Registry Inputs

The auth-plugin registry is controlled by:

- truststore material
- plugin policy file
- plugin root directory
- plugin manifests
- signed manifest envelopes

## Admission Modes

Current code-backed policy behavior includes signed-only admission and fail-on-unlisted control.

Required policy decisions include:

- whether unlisted plugins are rejected
- whether a plugin is required
- which signers are allowed for the plugin
- which method identifiers the plugin may expose

## Required Plugin Semantics

If a plugin is marked required and is missing or fails admission, startup fails closed.

This is current proven behavior for enterprise-style required plugins.

## Signer Enforcement

Plugin manifest signature and signer identity are part of admission. Unknown or unauthorized signers are deterministically rejected.

## Method Exposure

The registry resolves:

- plugin presence
- method availability
- `AuthType` to method identifier mapping

Only admitted plugins contribute methods to the runtime registry.

## Built-In and External Plugins

The same registry framework governs both built-in and dynamically rooted plugin surfaces. Missing API symbols, invalid descriptors, or policy violations are admission failures.

## Current Proof and Rebuild Boundary

Current code and tests prove:

- example policy and truststore loading
- deterministic signer rejection
- required-plugin startup failure
- method resolution for enterprise provider families

This specification reconstructs the product rule that authentication plugins are a signed, policy-governed extension surface rather than an unrestricted shared-library loader.
