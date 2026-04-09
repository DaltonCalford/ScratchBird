Status: current_authority

# Auth Plugin Truststore, Policy, and Enterprise Signer Admission Model

## Purpose

This file defines the current signed-plugin admission model for authentication
plugins, including truststore roots, policy allowlists, and enterprise-method
admission rules.

## Governing rule

Authentication plugin admission requires both:

1. truststore acceptance
2. policy acceptance

Truststore success alone is insufficient.

## Current truststore model

The current example truststore is a JWKS-style document with key rows carrying:

- `kid`
- `kty`
- `crv`
- `use`
- `alg`
- public key material
- `active`

The current example trust roots separate signer classes including:

1. release signer
2. security signer
3. enterprise signer

## Current policy model

The current auth plugin policy document carries:

- global mode
- `fail_on_unlisted_plugins`
- per-plugin allowlist entries

Each per-plugin entry currently carries:

- required flag
- allowed signers
- minimum version
- maximum version or major-band allowance
- allowed method IDs

## Admission rule

A plugin is admissible only when all of the following are true:

1. the manifest is structurally valid
2. the manifest JWS signer is trusted by the truststore
3. the plugin is allowed by policy
4. the signer is allowed for that plugin by policy
5. the method IDs exposed by the plugin are allowed by policy
6. the plugin version satisfies policy bounds

## Enterprise-method signer partitioning

The current policy example partitions enterprise methods under enterprise
signers, including methods such as:

1. LDAP bind
2. Kerberos GSSAPI
3. Ident RFC1413
4. RADIUS PAP
5. PAM conversation

These methods are not admitted merely because a release or security signer is
trusted. They require the enterprise signer class allowed by policy.

## Current code-backed refusal classes

Current tests prove at least these refusal classes:

1. untrusted signer:
   - plugin signer not trusted by the truststore
   - deterministic reason class `AUTH_PLUGIN_SIGNER_UNTRUSTED`
2. policy signer mismatch:
   - signer trusted globally but not allowed for the specific plugin policy row
   - deterministic reason class `AUTH_PLUGIN_POLICY_DENIED`

## Required admission sequencing

The admission sequence shall be:

1. load policy
2. load truststore
3. validate manifest shape and signature material
4. validate signer trust
5. validate plugin allowlisting
6. validate signer allowlisting for that plugin
7. validate version bounds
8. validate method-ID allowlisting
9. admit plugin methods

The implementation shall not admit methods early and revoke them later.

## Required method-ID binding rule

Method IDs are part of the policy contract.

An admitted plugin may expose only the method IDs allowed for its policy row.

This prevents a trusted signer from smuggling a different auth method family
under an allowed plugin identity.

## Fail-closed rules

The auth subsystem shall reject:

1. untrusted signers
2. trusted-but-policy-disallowed signers
3. disallowed method IDs
4. version mismatches
5. unlisted plugins when fail-on-unlisted mode is active

## Reconstructed required expansion

The rebuild requires future operator-visible inspection rows for:

1. signer class
2. truststore key ID used for admission
3. policy row matched
4. version band matched
5. rejected method IDs, if any
