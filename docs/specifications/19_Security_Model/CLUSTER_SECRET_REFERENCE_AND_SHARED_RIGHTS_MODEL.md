# Cluster Secret Reference and Shared Rights Model

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the current secret-indirection patterns already present in code and the required commercial-grade model built on those patterns.

## Current code-backed secret anchors

Current code already proves secret-bearing or secret-referencing surfaces such as:
- manager proxy configuration carrying `mcp_auth_secret`
- alert targets carrying `auth_secret_uuid`
- MFA enrollment secrets encrypted before persistence
- separation between endpoint/config identity and secret reference

Current code and tests also prove:
- manager start is refused when `mcp_auth_secret` is missing
- manager secret may come from explicit configuration or environment source
- manager MCP authentication is token-gated rather than anonymous
- native MCP handshake publishes a required token-auth challenge before the manager reports success

These anchors are sufficient to make secret indirection canonical.

## Secret indirection rule

Configuration objects must reference secrets by stable identity instead of embedding plaintext secret material directly.

Rules:
- endpoint or route identity is distinct from secret reference
- configuration inspection does not imply secret disclosure
- mutation privilege does not imply secret readback privilege
- rotation preserves secret object identity and version history

## Manager-control consequence

Current manager launch and controller code proves:
- manager proxy authentication secret is mandatory
- secret may come from explicit configuration or environment source
- control-plane launch is refused when the secret is missing

Current manager MCP tests also prove:
- the manager issues an `AUTH_CHALLENGE`
- the challenge requires `TOKEN`
- a nonce is published
- the client must answer with the configured manager secret payload
- only then does the manager return successful auth

That means manager control traffic is both:
- authenticated
- challenge-bound

It is not equivalent to a local unsecured health socket.

That means manager-control traffic is an authenticated administrative plane, not anonymous local control traffic.

## Shared-rights rule

Shared user, role, and group recognition across managed deployments must preserve:
- identity source
- scope
- effective policy version
- local allow, deny, and override state

Shared-rights propagation must not become a bypass for:
- row security
- column security
- domain masking
- emulated schema sandbox boundaries

## Current control-plane refusal anchors

Current code-backed manager and controller surfaces also prove fail-closed startup gates for:
- missing manager secret
- zero manager front-door port
- zero internal native port
- front-door and internal-native port collision

These are not optional deployment hygiene checks. They are part of the authenticated administrative-plane contract.

## Current-versus-required split

Current code-backed authority proves:
- mandatory manager control secret
- secret reference fields for alerting surfaces
- encrypted MFA secret storage
- separation between authentication and authorization

Current code-backed authority also proves:
- manager secret challenge or response flow on MCP
- controller-side refusal when the manager secret is absent
- controller-side refusal when the front-door and internal-native ports collide

Current code does not yet fully prove:
- full cluster secret fragmentation runtime
- full shared-rights propagation runtime
- complete remote cluster secret activation workflow

Those remain reconstructed required behavior, not current shipped claims.
