# Auth Provider Chain Lockout and Enterprise Mapping Model

## Purpose

Define the runtime authentication-provider chain, lockout policy, and enterprise identity mapping model.

## Provider Chain Model

An authentication policy binds a principal account to an ordered provider chain.

Each provider in the chain carries:

- provider identity
- provider kind
- enabled state
- priority rank
- fail mode
- provider configuration payload

## Evaluation Order

Provider chain evaluation is deterministic and ordered.

For each attempted login:

1. resolve the principal account
2. load the bound authentication policy
3. verify allowed method and transport constraints
4. evaluate providers in chain order
5. stop on success
6. continue or fail according to the provider fail mode and policy result

## Runtime Decision Fields

The runtime decision model includes:

- success or failure
- selected provider identity
- attempted provider list
- whether MFA is required
- failure classification

## Lockout

Authentication policy may define:

- lockout threshold
- lockout window
- lockout duration

When repeated failures exceed policy, the account is locked and subsequent attempts fail closed until the lockout condition clears administratively or by policy timing.

## Enterprise Provider Mapping

Enterprise or external providers may map successful external identities into database users through explicit authentication mappings.

Current proof covers mapping from provider-authenticated external subject into a database user and recording attempt logs with the provider identity.

## Validation Rules

Provider configuration is validated before admission. Invalid provider payloads are rejected at catalog-ingress time rather than being deferred to runtime best effort.

Current proof includes fail-closed validation for unsupported or insecure LDAP payload shapes.

## Audit Trail

Authentication attempts are recorded as catalog-backed attempt log entries with outcome and provider attribution.

## Current Proof and Rebuild Boundary

Current code proves:

- deterministic provider chain evaluation
- enterprise provider-to-user mapping
- runtime attempt logging
- lockout enforcement
- provider payload validation

This specification reconstructs the product rule that authentication is a catalog-driven provider policy chain, not a hardcoded single-method gate.
