# Auth Provider Chain and MFA Policy Runtime Model

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the current ScratchBird authentication chain model beyond plugin admission:
- provider-chain evaluation
- provider priority and fail-mode behavior
- MFA-required policy handling
- lockout and cooldown behavior
- MFA policy and enrollment catalog semantics
- recovery-code and break-glass behavior
- default provider safety rules already visible in code

## Provider-chain runtime model

Authentication policy is not a single-method toggle. The current runtime already supports a provider chain bound to an auth policy.

The current provider-chain policy surface includes at minimum:
- ordered provider chain
- provider priority
- provider state
- provider fail mode
- allowed credential kinds
- client capability checks
- MFA-required flag
- password fallback policy
- lockout threshold
- lockout window
- lockout duration
- allowed auth method mask
- required auth method
- allowed transport mask

## Beta 1 admitted provider subset

For package `06`, provider-chain and MFA machinery is current substrate, but the
required Beta 1 success paths are bounded to:

- local password or compatibility-password methods
- MD5 compatibility where a shipped protocol surface still requires it
- MySQL wire proof where that current compatibility listener resolves into the
  local account model
- SCRAM
- token or authkey
- peer
- certificate mTLS
- MFA overlays on top of those admitted primary methods

Enterprise or external providers may still exist in catalogs, selftests,
benchmarks, or plugin registries. In Beta 1 package `06` they are validation
and refusal surfaces unless a later canon update explicitly promotes them into
the admitted support set.

Current Beta 1 runtime now enforces that bounded scope directly:
- non-admitted enterprise plugin payload methods remain catalog-valid substrate
  only
- negotiation does not emit them as admitted Beta 1 runtime methods
- direct runtime attempts on `ldap_bind`, `kerberos_gssapi`, `jwt_bearer`, and
  `oidc_id_token` fail closed through the provider-chain refusal path

## Deterministic provider-chain evaluation

Current code-backed catalog tests prove deterministic evaluation of a provider chain.

Current rules:
- providers are attempted in chain order
- attempted provider identity is preserved in the runtime decision object
- an accepted provider may still be insufficient if higher policy requirements are not satisfied
- attempted-provider history must remain auditable

The runtime decision must preserve at minimum:
- success or failure
- selected provider identity
- attempted provider identities
- whether policy required MFA

## Provider states and fail modes

Current policy objects expose at least:
- provider state
  - enabled or disabled admission posture
- provider fail mode
  - current tests prove `TRY_NEXT`

Canon rule:
- a provider that fails under a “try next” posture may permit evaluation to continue
- a provider that is disabled or invalid is not a silent success
- provider ordering and fail mode are part of the authentication policy contract, not implementation trivia

## MFA-required rule

Current code-backed tests prove:
- a provider may return an accepted adapter outcome
- the overall runtime must still fail if policy requires MFA and MFA completion is absent

Therefore:
- provider acceptance is not equivalent to authentication success
- MFA-required policy is a hard gate
- successful completion must preserve `policy_requires_mfa = true` in the runtime decision when MFA is part of the success path

## Lockout model

Current code-backed tests prove:
- lockout threshold
- lockout window
- lockout duration
- persisted account lock state after threshold breach
- repeat failure under a locked account returns a different refusal path than the initial threshold breach

Current stable refusal-code expectations recovered from tests:
- `SEC_1213`
  - invalid-authorization failure on provider-chain rejection under current policy
- `SEC_1215`
  - refusal because the account is already locked

Lockout is therefore part of current runtime truth, not reconstructed policy only.

## Provider validation rules

Current provider validation tests already prove fail-closed configuration validation.

Recovered examples:
- invalid LDAP provider configuration is rejected
- plaintext LDAP while STARTTLS is required is rejected

Current stable refusal-code expectations recovered from tests:
- `SEC_1216`
  - invalid LDAP-style provider configuration
- `SEC_1217`
  - plaintext LDAP transport denied while stronger transport policy is required

## Enterprise provider safety baselines outside admitted Beta 1 success paths

### LDAP default provider

Current default LDAP provider behavior includes:
- timeout failure when connect timeout is zero
- invalid configuration failure when `ldap_uri` is absent
- endpoint allowlist enforcement
- refusal of plain `ldap://` transport when `require_starttls` is true
- explicit bind-failed outcome for failed bind simulation
- success path that returns resolved principal on valid input

### Kerberos default provider

Current default Kerberos provider behavior includes:
- timeout failure when connect timeout is zero
- service-principal and keytab requirement
- KDC endpoint allowlist enforcement
- replay detection outcome
- invalid-ticket outcome
- success path that returns resolved principal on valid input

These default-provider behaviors are canonical safety baselines for provider
adapters and test doubles. In package `06` Beta 1 scope they prove validation
and refusal posture, not required production-success parity for every
enterprise provider family.

## MFA policy catalog model

Current code-backed tests prove first-class MFA policy catalog objects with fields including:
- policy identity
- policy name
- primary factor type
- allow recovery codes
- allow break glass
- maximum challenge attempts
- challenge TTL
- step-up TTL

Current policy CRUD is canonical:
- create or update
- get
- list
- delete

## MFA enrollment model

Current code-backed tests prove first-class MFA enrollment objects with fields including:
- enrollment identity
- account identity
- MFA policy identity
- factor type
- primary-factor indicator
- enrolled indicator
- secret presence indicator
- TOTP secret material
- TOTP digits
- TOTP period
- TOTP look-ahead
- TOTP look-behind
- enrollment timestamp

Current enrollment CRUD is canonical:
- create or update
- get
- list by account
- delete

## MFA secret handling

Current runtime and tests prove:
- TOTP secret material is stored and round-tripped through the catalog
- the persisted secret must not appear in plaintext in the database file

That means the commercial-grade canon is:
- secret material may be available to the trusted runtime for challenge evaluation
- secret material must not be written to disk in raw plaintext form

## TOTP and HOTP runtime behavior

Current MFA runtime already implements:
- TOTP secret generation
- Base32 encode and decode
- HOTP generation
- TOTP generation
- TOTP verification across look-behind and look-ahead windows
- URI generation for TOTP enrollment
- support for:
  - SHA1
  - SHA256
  - SHA512

The current canonical MFA factor runtime is therefore stronger than “MFA policy rows exist.”

## Recovery-code and break-glass model

Current code-backed tests prove first-class recovery-code objects with:
- recovery identity
- account identity
- MFA policy identity
- break-glass indicator
- code hash
- maximum uses
- current uses
- cooldown

Canonical runtime consequences:
- recovery codes are consumed against usage limits
- break-glass is a distinct policy surface, not ordinary recovery-code usage
- cooldown and exhaustion are part of current runtime semantics

## Current-versus-required split

Current code-backed authority is strongest for:
- provider-chain runtime decisions
- MFA-required gating
- lockout behavior
- MFA policy and enrollment catalog CRUD
- recovery-code persistence and exhaustion
- TOTP or HOTP runtime behavior

Required reconstructed canon extends this into a full commercial-grade model for:
- richer enterprise provider chaining
- cluster-wide MFA policy synchronization
- recovery and break-glass operator workflow across managed deployments

## Non-authority

This file does not claim that every provider implementation is already production-complete.
It defines the current provider-chain and MFA runtime contract that implementation must satisfy.

Broader enterprise providers remain outside the admitted Beta 1 success-path set
for package `06` unless canon is updated again.
