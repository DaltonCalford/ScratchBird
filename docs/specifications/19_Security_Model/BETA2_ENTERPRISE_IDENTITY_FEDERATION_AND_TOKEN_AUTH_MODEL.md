# Beta 2 Enterprise Identity Federation And Token Auth Model

## Purpose

Define first-class external identity federation, token authentication, and
claim-to-role mapping over the existing ScratchBird auth-plugin framework.

## Governing rules

1. External identity is admitted through cataloged federation providers.
2. Token validation and claim mapping are explicit policy.
3. Local recovery authority always remains available unless a stronger operator
   policy explicitly supersedes it.
4. No provider may silently create broad administrative rights.

## Canonical metadata

- `sb_identity_provider`
  - `provider_uuid`
  - `provider_name`
  - `provider_kind`
  - `issuer_uri`
  - `jwks_uri`
  - `status`
- `sb_identity_claim_map`
  - `map_uuid`
  - `provider_uuid`
  - `claim_name`
  - `match_rule`
  - `role_uuid`
  - `priority`
- `sb_external_principal_binding`
  - `binding_uuid`
  - `provider_uuid`
  - `subject_value`
  - `local_principal_uuid`
  - `status`

## Authentication flow

1. Client presents token or federated assertion.
2. Provider metadata and signing keys are resolved.
3. Token validation runs.
4. Claim mapping resolves effective roles and groups.
5. Session is admitted or refused.

## Refusal rules

- `IDENTITY_PROVIDER_UNKNOWN`
- `IDENTITY_TOKEN_INVALID`
- `IDENTITY_CLAIM_MAPPING_FAILED`
- `IDENTITY_RECOVERY_PATH_REQUIRED`

## Metrics

- federated logins
- token validation failures
- claim-map misses
- recovery-path usage

## Cross-section requirements

- section `19` owns provider, claim, and binding policy
- section `30` owns installer bootstrap surfaces for provider setup
