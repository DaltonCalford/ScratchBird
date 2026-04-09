# Engine Profile Compatibility and Enum Freeze Certification Model

## Purpose

Define the certification gates that protect the public engine profile contract from silent drift.

## Required Gates

The clean build and unit-test cycle shall certify all of the following:

1. Engine profile enum ordinal freeze.
2. Engine profile symbolic-name freeze.
3. Engine profile serialized-token freeze where serialization is public or semi-public.
4. Public engine compatibility API compilation compatibility.
5. Refusal of unknown or unsupported profile identities.
6. Language runtime admission refusal when engine profile or capability requirements are not met.

## Minimum Evidence Sources

Certification shall include code-backed proof equivalent to:

- compile-time or unit-test checks that freeze the public profile enum
- compile-time or unit-test checks that freeze the public compatibility API surface
- runtime admission tests that reject mismatched profile or capability combinations

## Required Negative Tests

The certification suite shall include refusal cases for:

- unknown profile identity
- stale compatibility table
- runtime package allowed-profile mismatch
- capability requirement mismatch
- restart or reattach path that attempts to reuse stale profile-sensitive state

## Release Gate

A release shall not ship if any public profile identity changes meaning without an explicit major compatibility decision and corresponding canonical specification update.

## Rebuild Boundary

Current code proves that the engine profile contract is already guarded by compatibility and enum-freeze tests. This specification reconstructs the product-level release rule that those checks are a mandatory certification gate, not merely local developer tests.
