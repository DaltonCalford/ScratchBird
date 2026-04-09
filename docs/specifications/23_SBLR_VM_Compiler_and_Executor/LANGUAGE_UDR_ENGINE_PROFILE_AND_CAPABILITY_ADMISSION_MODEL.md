# Language UDR Engine Profile and Capability Admission Model

## Purpose

Define how optional language UDR runtimes are admitted against the current engine profile and capability set.

## Governing Principles

1. Language runtimes are optional.
2. A runtime is admitted only when the engine profile and current capability envelope explicitly allow it.
3. Admission is fail-closed.
4. Runtime availability does not widen the engine profile contract.
5. Profile mismatch, trust failure, or capability mismatch shall refuse execution rather than degrade silently.

## Runtime Declaration Requirements

Each optional runtime or runtime package shall declare:

- runtime identity
- runtime version
- supported callable kinds
- allowed engine profile set
- required engine capability flags
- prohibited engine capability combinations, if any
- required trust or signer state
- required JIT or native backend dependency, if any
- marshalling constraints for arguments and results

## Admission Algorithm

When a language UDR callable is first bound or revalidated, the engine shall execute the following sequence:

1. Resolve the current engine profile.
2. Resolve the runtime declaration for the requested language runtime.
3. Verify signer, trust, and policy admission.
4. Verify that the current engine profile is in the runtime's allowed profile set.
5. Verify all required capability flags.
6. Verify callable-kind support for the requested object.
7. Verify that required backend resources are available if the runtime requires JIT, native compilation, or another optional execution backend.
8. Verify that argument and result marshalling rules are supported under the current profile and compatibility mode.
9. Bind the runtime only if every prior step succeeds.

Failure at any step shall refuse the bind.

## Revalidation Triggers

The engine shall invalidate cached runtime admission decisions when any of the following changes:

- engine profile
- capability table
- runtime package version
- runtime trust state
- signer or policy material
- required backend availability

## Interaction with SBLR and V3

Language runtime admission is downstream of SBLR and planner validation. A callable that passes parser and SBLR validation still shall not execute if the runtime admission rules fail under the current engine profile.

## Restart and Reattach Boundary

Dormant session reattach or restart replacement reattach shall revalidate language runtime admission. Prior successful admission in an earlier process lifetime is not authoritative after restart.

## Unsupported Behavior

The engine shall not:

- admit a runtime because another runtime under a different profile succeeded
- reuse one profile's runtime compatibility decision for a different profile
- widen callable support based on listener dialect mode
- treat optional runtime presence as proof of backend or trust eligibility

## Current Proof and Rebuild Boundary

Current code proof exists that the language runtime path uses engine-profile and capability gating during admission and execution setup. This specification reconstructs the complete product rule for runtime declarations, revalidation, and fail-closed capability matching.
