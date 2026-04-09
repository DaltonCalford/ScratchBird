# Cluster Security Quorum and Bootstrap Shard Model

## Purpose

Define the security-quorum and bootstrap-shard rules for cluster-aware security controls and encryption bootstrap.

## Security Quorum

The runtime security quorum has:

- required count
- total count
- failure mode

Current failure modes are:

- `FAIL_OPEN`
- `FAIL_CLOSED`
- `REQUIRE_REMOTE`

## Quorum Decision Model

The current quorum evaluator returns one of:

- `ALLOW_CACHE`
- `BYPASS_CACHE`
- `DENY`

This decision presently gates permission-cache behavior and related security-sensitive cache use.

## Failure-Mode Semantics

- satisfied quorum:
  - allow normal secured cache behavior
- unsatisfied quorum plus `FAIL_OPEN`:
  - bypass cache rather than deny outright
- unsatisfied quorum plus `FAIL_CLOSED` or `REQUIRE_REMOTE`:
  - deny security-sensitive cached behavior

## Configuration Surface

The service configuration exposes quorum controls under the security section, including `quorum_n`, `quorum_m`, and failure-mode selection.

## Bootstrap Shard Threshold

Encryption bootstrap profiles may require a minimum shard threshold.

Current code-backed unlock policies include:

- local-only local keystore
- OS keyring or manual quorum
- external KMS
- HSM quorum
- KMS with HSM escrow

Policies that require quorum-aware bootstrap shall reject configurations whose minimum shard threshold is weaker than the profile minimum.

## Provider Bridge and Unlock Outcome

External KMS and HSM policies perform provider evaluation and persist unlock outcome evidence. Availability and authorization are distinct from mere provider naming.

## Reconstructed Cluster-Secret Rule

The intended product rule is that cluster security material may be distributed across multiple guarded shards or snippets, with a configured threshold required for unlock or promotion-sensitive operations. Current code proves the threshold and provider-policy side of that model; broader cluster distribution orchestration remains a reconstructed required behavior where not yet fully expressed in code.

## Current Proof and Rebuild Boundary

Current code proves:

- security quorum evaluation
- service-level quorum configuration
- permission-cache quorum gating
- encryption bootstrap threshold enforcement
- KMS and HSM quorum-aware unlock policy checks

This specification reconstructs the broader product rule for cluster-sharded security material and fail-closed quorum-sensitive unlock behavior.
