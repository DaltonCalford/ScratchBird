# Engine Profile Enum Freeze and Runtime Capability Contract

## Purpose

Define the stable public engine-profile contract used by drivers, emulation layers, optional language runtimes, and runtime admission logic. This contract exists to prevent silent feature drift when engine capabilities evolve.

## Normative Rules

1. `EngineProfile` is a frozen public compatibility enum.
2. Existing profile numeric identities, symbolic names, and externally serialized tokens are append-only.
3. Existing profile identities shall not be renumbered, reused, repurposed, or silently aliased.
4. New profiles may only be added at the end of the contract and shall not change the meaning of prior profiles.
5. Unknown profile values are a hard refusal condition for tooling, drivers, and optional runtime surfaces.
6. The engine profile is the public capability envelope. Listener family, proxy front-door mode, or driver dialect mode do not override it.
7. A profile defines a bounded capability set, not a best-effort hint.
8. Any consumer that depends on profile-specific behavior shall gate that behavior on the published profile contract before use.

## Contract Contents

Each engine profile shall define, directly or by a stable companion capability table:

- profile identity
- public compatibility family
- exposed SQL and protocol compatibility boundary
- optional feature flags
- callable surface flags
- language-runtime admission flags
- JIT or accelerator eligibility flags
- security and trust preconditions
- unsupported or fail-closed exclusions

## Consumer Admission Algorithm

Any public or semi-public consumer of the engine profile contract shall execute the following sequence:

1. Read the engine-advertised profile identity and contract version.
2. Verify that the local consumer knows that profile identity.
3. Verify that the local consumer's compatibility table explicitly permits that profile.
4. Verify any required companion capability flags.
5. Enable only the behaviors explicitly allowed for that profile.
6. Refuse activation if any step fails.

The consumer shall not:

- downgrade an unknown profile to a nearest known profile
- assume PostgreSQL, MySQL, Firebird, or native behavior from listener or parser mode alone
- infer optional callable, JIT, or accelerator support without explicit profile or capability proof

## Stability Rules

The engine profile contract is subject to the following freeze rules:

- enum ordinal freeze
- symbolic-name freeze
- serialized-token freeze
- public compatibility API freeze

These freeze rules exist because drivers, tooling, optional runtimes, and compatibility tests compile against the contract.

## Runtime Capability Binding

Runtime subsystems that rely on profile-specific capabilities shall bind through explicit capability checks rather than string comparison or ad hoc feature probing.

Required examples include:

- driver feature enablement
- emulation-surface routing
- language UDR admission
- JIT provider or backend admission
- accelerator-assisted operator admission

## Fail-Closed Requirements

The engine shall refuse or reduce capability when:

- the profile identity is unknown
- the local compatibility table is older than the profile contract
- a required capability flag is absent
- an optional runtime requires a stronger profile than the current engine exposes

Best-effort fallback is only allowed when a separate canonical specification explicitly permits that fallback path.

## Current Proof and Rebuild Boundary

Current code proof exists for:

- frozen engine profile enum identity
- compatibility contract compilation checks
- runtime use of profile-based gating in optional language execution paths

This specification reconstructs the broader product rule that the engine profile contract is a long-lived public compatibility boundary across engine, tooling, drivers, and optional runtimes.
