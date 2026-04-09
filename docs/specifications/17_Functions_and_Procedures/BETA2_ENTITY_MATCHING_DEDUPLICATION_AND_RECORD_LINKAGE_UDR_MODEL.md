# Beta 2 Entity Matching Deduplication And Record Linkage UDR Model

## Purpose

This document defines the matching UDR family for entity resolution,
deduplication, survivorship scoring, and record linkage.

This group is the ScratchBird-native replacement target for the highest-value
operational portions of `RapidFuzz`-style matching, together with the existing
graph and probability substrate.

## Owning package

- `sb_pkg_match_udr`

## Dependencies

This package depends on:

- `sb_pkg_text_udr`
- `sb_pkg_graph_udr`
- `sb_pkg_prob_udr`
- `sb_pkg_rules_udr`

## Mandatory surfaces

The package shall provide:

- fuzzy similarity scoring
- blocking-key generation
- candidate generation
- pairwise match scoring
- clustering/grouping for probable duplicates
- survivorship and golden-record helper routines
- explainable match traces

## Required routine families

- `sb_match.score_*`
- `sb_match.block_keys(...)`
- `sb_match.candidates(...)`
- `sb_match.link_pairs(...)`
- `sb_match.cluster(...)`
- `sb_match.survivorship(...)`
- `sb_match.explain(...)`

## Example contract

```sql
select *
from sb_match.link_pairs(
    left_query => 'select id, name, address from crm.customers',
    right_query => 'select id, name, address from billing.customers'
);
```

## Operational rules

1. Matching pipelines must expose threshold policy and scoring breakdowns.
2. Deterministic blocking and candidate generation are required for the same
   inputs and policy version.
3. Probabilistic scoring must emit confidence values and feature contributions.

## Explicit exclusions

- unrestricted black-box entity resolution services
- remote API dependence for core matching logic
