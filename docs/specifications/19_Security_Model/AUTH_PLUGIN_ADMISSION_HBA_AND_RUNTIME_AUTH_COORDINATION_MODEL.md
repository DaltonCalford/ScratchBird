# Auth Plugin Admission, HBA, and Runtime Auth Coordination Model

## Status

Current code-backed authority with reconstructed commercial-grade detail.

## Purpose

This document defines the authoritative inbound authentication path in ScratchBird, including HBA matching, authentication-manager coordination, plugin admission, runtime dispatch, rate limiting, and audit hooks.

## Top-Level Ownership

Inbound authentication is coordinated by:

- `security::AuthManager`

Plugin admission and runtime method lookup are coordinated by:

- `security::AuthPluginManager`

The two layers are distinct:

- `AuthManager` decides how a connection is authenticated
- `AuthPluginManager` decides which plugin-backed methods are admissible and callable

## HBA Rule Model

Current HBA rules support:

- connection types:
  - `LOCAL`
  - `HOST`
  - `HOSTSSL`
  - `HOSTNOSSL`
  - `HOSTGSSENC`
- database matching
- user matching
- address matching
- auth method and auth options

Database and user matching currently support:

- explicit names
- `all`
- role-like or file-expanded forms
- optional regex handling

Address matching supports:

- any
- single address
- CIDR
- samehost
- samenet

The HBA layer is therefore first-class current authority, not a placeholder.

## Auth Manager Responsibilities

`AuthManager` is the authoritative inbound authentication coordinator and currently owns:

- HBA rule evaluation
- authentication method and plugin dispatch
- rate limiting
- audit-event capture
- user credential coordination

This means connection admission is policy-first, not direct-plugin-first.

## Rate Limiting Model

Current rate limiter configuration supports:

- maximum failed attempts
- lockout duration
- sliding window
- per-user tracking
- per-address tracking

A successful authentication resets counters for the tracked principal/address combination.

Rate limiting is therefore part of the authentication correctness path, not an optional add-on.

## Plugin Admission Model

External and builtin authentication plugins are admitted through:

- trust store
- policy file
- plugin root
- fail-on-unlisted behavior

Admission checks include:

- manifest validity
- signature validity
- trusted signer check
- policy allow-list check
- method allow-list check
- ABI compatibility
- load success

Reject reasons are explicit and typed. Admission failure is fail-closed.

## Builtin Phase-1 Method Registry

Current builtin plugin admission templates include these plugin families:

- trust/reject
- password compatibility
- SCRAM
- token authkey
- peer
- certificate mTLS
- JWT/OIDC
- WebAuthn
- factor chain
- workload identity
- OAuth validator
- proxy assertion
- LDAP
- Kerberos
- ident
- RADIUS
- PAM

These are current runtime method families, not speculative placeholders.

## Runtime Plugin Exchange Model

The plugin runtime exchange path currently supports:

- begin-auth
- continue-auth
- abort-auth

Connection context passed into plugins currently includes:

- transport class
- connection flags
- username
- client address
- server address
- peer uid
- peer gid
- optional peer pid

Plugin runtime return codes are mapped into stable ScratchBird authentication failure codes. Plugin runtime semantics are therefore bounded and normalized before they escape into the wider engine.

## Trust and Policy Files

Current defaults are file-backed and expected at fixed system paths unless overridden by config:

- plugin trust store
- plugin policy file

This means plugin admission is configuration-authoritative and fail-closed by default.

## Shared and External Identity Boundary

The authentication surface currently includes built-in support for:

- external identity providers
- proxy assertion methods
- workload identity methods
- group- or role-adjacent admission through HBA and auth policy

However, identity proof and authorization proof remain separate concerns. Successful authentication does not imply permission to mutate security, routing, or management state.

## Partial-Implementation Boundary

This document is current code-backed authority for authentication coordination.

It does not by itself prove complete production use of every plugin family in every deployment. The canonical meaning is:

- the admission/runtime framework exists
- builtin families are declared and routable through the plugin manager
- deployment maturity of individual methods may still vary by provider and rollout state

That maturity variation is implementation/deployment state, not absence from the authentication specification.
