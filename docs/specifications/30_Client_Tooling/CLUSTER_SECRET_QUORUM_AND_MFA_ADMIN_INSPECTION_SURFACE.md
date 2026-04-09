# Cluster Secret, Quorum, and MFA Admin Inspection Surface

## Scope

This file defines the operator-facing admin surface for inspecting:

- cluster-secret shard posture
- unlock quorum posture
- unlock audit posture
- MFA policy posture
- MFA enrollment and step-up posture

## Canonical surface rule

Admin tooling may present these results through SQL, native tooling, or manager
inspection paths, but the canonical meanings and fields must remain stable.

## Required inspection families

The admin surface must remain able to inspect:

- cluster-secret profiles
- active key identity
- shard threshold and current collected count
- quorum failure mode and current decision
- last unlock result and time
- shard holder posture
- MFA policy posture
- MFA enrollment posture
- step-up posture
- recovery and break-glass allowances

## Result-shape rule

Admin tooling may render rows, tables, or structured documents, but it must
preserve the same field meanings as the canonical result contracts.

The admin surface must not degrade these families into unstructured prose only.

## Mutation boundary

Inspection privilege is distinct from mutation privilege.

Canonical rule:

- viewing shard or quorum posture does not authorize unlock mutation
- viewing MFA posture does not authorize enrollment mutation or break-glass use

## Security rule

The admin inspection surface must not disclose:

- raw shard material
- reconstructed secret material
- raw MFA seeds
- raw recovery codes

## Fail-closed rules

The admin surface shall not:

1. report unlock-ready while hiding quorum posture
2. report MFA-complete while hiding missing enrollment or step-up requirements
3. treat inspection authority as equivalent to mutation authority
