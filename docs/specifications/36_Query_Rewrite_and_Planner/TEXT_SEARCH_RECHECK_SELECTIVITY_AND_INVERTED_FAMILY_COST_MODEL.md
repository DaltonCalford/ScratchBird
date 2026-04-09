Status: current_authority_with_reconstructed_expansion

# Text Search Recheck, Selectivity, and Inverted-Family Cost Model

## Purpose

This file defines the planner contract for text-search and inverted-family
access paths, especially where candidate retrieval and exact recheck differ.

## Governing rule

Inverted/text families are candidate producers first.

Planner and executor must distinguish:

1. key-match candidate discovery cost
2. exact recheck cost
3. MGA visibility reject cost

## Current code-backed baseline

The current code proves:

1. query analysis distinguishes `NEED_ALL`, `NEED_ANY`, and `NEED_RECHECK`
2. selectivity estimation exists at the GIN `tsvector` operator-class layer
3. the full-text wrapper currently returns candidates for higher-layer exact verification

## Planner consequences

The planner shall treat:

1. `NEED_ALL` as an all-key constrained inverted lookup
2. `NEED_ANY` as an any-key candidate expansion path
3. `NEED_RECHECK` as a two-stage path:
   - candidate retrieval
   - exact verification

## Required cost components

The text-search family cost model shall account for:

1. query key count
2. strategy class
3. candidate set size
4. expected recheck rate
5. phrase or negation recheck burden
6. visibility reject rate
7. dead-entry burden
8. posting compression and posting-tree traversal effects

## No-ignored-index rule for text search

The optimizer shall not ignore inverted/text families merely because they need
recheck.

Instead it shall:

1. cost the recheck explicitly
2. compare that cost against competing access paths
3. select the best legal path under the parity rules

## Reconstructed required expansion

The rebuild requires future family-native metrics for:

1. lexeme fanout
2. phrase recheck rate
3. negation recheck rate
4. fuzzy-match expansion rate
5. posting compression hit rate
6. visibility reject rate by text-search family

## Fail-closed rule

The planner shall not cost a text-search family as if candidate hits are final
matches when strategy is `NEED_RECHECK`.
