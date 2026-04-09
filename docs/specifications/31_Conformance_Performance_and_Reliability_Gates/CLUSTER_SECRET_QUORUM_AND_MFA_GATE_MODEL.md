# Cluster Secret, Quorum, and MFA Gate Model

## Scope

This file defines the certification model for:

- cluster-secret shard handling
- unlock quorum decisions
- break-glass and recovery posture
- MFA policy, enrollment, recovery, and step-up behavior

## Current code-backed authority

Current code-backed recovery proves:

1. sharded key and bootstrap unlock catalog substrate exists
2. security-quorum runtime distinguishes `ALLOW_CACHE`, `BYPASS_CACHE`, and
   `DENY`
3. MFA policy, enrollment, recovery-code, and step-up substrate exists
4. MFA runtime challenge and verification behavior exists

## Gate objective

The gate must prove that cluster-secret and MFA lanes remain:

- fail-closed
- structured
- auditable
- distinct from each other

## Required evidence families

The gate must retain, at minimum:

- cluster-secret profile and key identity summary
- shard-threshold summary
- quorum-decision summary
- unlock-attempt summary
- MFA policy summary
- MFA enrollment summary
- challenge or step-up summary
- recovery or break-glass summary, when used

## Pass criteria

The gate passes only if:

1. quorum posture is enforced according to failure mode
2. insufficient shard posture does not silently unlock
3. break-glass or recovery use is explicitly auditable
4. MFA challenge and step-up posture remain deterministic
5. secret and seed material remain redacted from evidence outputs

## Fail-closed rules

The gate fails if:

1. quorum deny posture is bypassed without canonical allowance
2. unlock succeeds while threshold posture is unsatisfied
3. MFA enrollment or policy resolution failure silently degrades to success
4. raw secret, shard, seed, or recovery-code material appears in evidence

## Reconstructed-required behavior

The rebuild requires later promotion of:

- stronger executed gate bundles for shard collection and unlock orchestration
- stronger executed gate bundles for MFA recovery and break-glass paths
- tighter alignment between admin inspection rows and gate evidence rows
