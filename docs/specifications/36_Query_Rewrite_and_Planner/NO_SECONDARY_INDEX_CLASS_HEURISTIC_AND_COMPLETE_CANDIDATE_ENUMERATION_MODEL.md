Status: reconstructed_required

# No Secondary Index Class Heuristic and Complete Candidate Enumeration Model

## Purpose

This document defines the canonical prohibition on secondary-class index heuristics and requires complete candidate enumeration across implemented families.

## Canonical Rule

The optimizer shall not contain a heuristic classifying some implemented index families as secondary, advisory, or hint-only by default. Candidate enumeration begins from implemented semantic legality, not from historical family preference.

## Complete Enumeration Rule

For a given query shape, candidate enumeration shall:

1. determine the semantic requirements of the access path
2. enumerate every implemented family that can satisfy those requirements
3. request family-native metrics for each family
4. record explicit refusal for any family that cannot proceed further

## Prohibited Heuristics

The following are non-conforming:

- hidden “primary family” shortcuts that skip legal families
- ordered-family-only default enumeration
- family suppression based on historical implementation age
- treating non-B-tree families as advisory unless manually forced

## Required Explanation Rule

If a family does not appear in the final candidate bundle, the optimizer trace shall show exactly why:

- not semantically legal
- not implemented for the shape
- refused by explicit family rule
- refused by metrics, memory, accelerator, or safety policy

## Non-Guarantees

This file does not require all families to be cost-equivalent. It requires them to enter the same primary-class enumeration framework.
