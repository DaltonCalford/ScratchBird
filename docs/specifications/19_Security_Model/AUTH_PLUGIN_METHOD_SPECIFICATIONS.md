# Auth Plugin Method Specifications

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the canonical authentication method families, provider classes, plugin admission rules, and the separation between authentication and authorization.

## Provider classes

Current code-backed or declared provider classes include:
- `LOCAL`
- `LDAP`
- `ACTIVE_DIRECTORY`
- `OAUTH2`
- `SAML`
- `KERBEROS`
- `EXTERNAL_SCRIPT`

Presence in enums or registries is not sufficient proof of full implementation maturity.
Runtime authority comes from admitted provider rows, enabled-state policy, and adapter execution paths.

## Beta 1 admitted provider scope

For Beta 1 package `06`, the admitted success-path provider scope is bounded to
the local-engine and single-node service subset:

- local account and password-backed authentication
- signed builtin plugin methods for password compatibility, SCRAM,
  token or authkey, peer, and certificate mTLS
- manager or control-plane token authentication using the same bounded secret
  indirection model
- MFA continuation layered on top of those admitted primary methods

Declared or benchmarked enterprise and external families outside that set are
not implicit Beta 1 runtime support. If they are not explicitly admitted by a
later canon update, they must remain unavailable or fail closed rather than
silently downgrading to weaker behavior.

Current Beta 1 fail-closed examples include negotiated enterprise plugin
payload methods such as:
- `scratchbird.auth.ldap_bind`
- `scratchbird.auth.kerberos_gssapi`
- `scratchbird.auth.jwt_bearer`
- `scratchbird.auth.oidc_id_token`

## Method families

The canonical authentication method families include:
- password authentication
- MD5-style challenge response for compatibility surfaces
- MySQL wire proof methods
- SCRAM begin and finish
- token proof methods
- negotiated plugin-payload methods
- peer identity methods

Within package `06` Beta 1 scope, successful runtime use is bounded to the
admitted local-engine set above. Negotiated plugin-payload exchange remains the
transport envelope, but it does not automatically promote every declared plugin
family into a supported Beta 1 success path.

Unsupported methods must fail closed. They must not silently downgrade to weaker methods.

## Admission rules

An authentication plugin or provider method may be admitted only if:
- the provider row is enabled
- the provider kind matches the requested method family
- negotiation does not violate auth policy restrictions
- required client pinning or channel-binding rules are satisfied
- the module-admission policy permits the plugin or provider family
- successful authentication does not itself grant authorization bypass

## Rights separation

Authentication establishes identity only.
Authorization remains separate and must still govern:
- object access
- row filtering
- column disclosure
- masking bypass
- domain access
- remote-management mutation
- secret inspection or reconstruction

## Current-versus-required split

Current code-backed proof is strongest for:
- local and enterprise provider abstraction
- plugin-method dispatch
- provider-chain evaluation against catalog policy
- hard-fail versus fallback behavior
- manager and control-plane authentication secret requirements

Required reconstructed canon extends that into:
- commercial-grade provider plugin lifecycle and signing discipline
- shared user, role, and group mapping across managed deployments
- emulated-engine authentication mapping without security weakening

For package `06`, the first implementation closure targets only the admitted
local-engine Beta 1 families. Broader enterprise and managed-deployment
provider families remain explicit future-expansion or fail-closed surfaces.

## Donor-alignment rule

Firebird-style plugin configurability and multi-provider user-management ideas may inform extension behavior, but ScratchBird canon is defined by its own provider-chain, policy-intersection, MFA, and secret-indirection model.
