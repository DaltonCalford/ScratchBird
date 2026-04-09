Status: current_authority

# GIN TSVector Operator Class and Full-Text Runtime Model

## Purpose

This file defines the current full-text runtime built on the GIN index family
and the `tsvector`/`tsquery` operator class.

## Current architecture

The current full-text runtime is a wrapper over the GIN family.

Current code-backed architectural facts:

1. backend family: `GIN`
2. indexed value type: `TSVector`
3. query value type: `TSQuery`
4. operator-class surface: `GINTSVectorOps`
5. query operator model: text-search match over lexeme sets and Boolean structure

## Current full-text wrapper contract

The current full-text wrapper:

1. requires at least one indexed column
2. creates and opens an underlying GIN index
3. extracts lexeme keys from serialized `TSVector`
4. inserts and removes posting entries through the underlying GIN runtime
5. delegates GC and dead-entry removal to the underlying GIN runtime

## MGA and deletion rule

Current code and headers explicitly bind the full-text path to Firebird-style
MGA semantics:

1. posting entries carry `xmin`
2. posting entries carry `xmax`
3. deletion is logical first, not destructive first
4. search remains subordinate to current transaction visibility

## TSVector key extraction rule

Current operator-class behavior proves:

1. each unique lexeme becomes an index key
2. positional and weight annotations are discarded for index keys
3. the original `TSVector` retains the richer positional data outside the GIN key set

Example current behavior:

- input `TSVector`: lexemes with positions and weights
- output key set: unique lexeme byte keys only

## TSQuery key extraction rule

Current operator-class behavior proves:

1. query-key extraction returns lexeme terms only
2. Boolean operators are not emitted as keys
3. phrase and negation queries still contribute lexeme keys for candidate search

## Query-strategy rule

The current operator class already distinguishes:

1. `NEED_ALL`
2. `NEED_ANY`
3. `NEED_RECHECK`

This means:

1. AND-style queries can use all-key candidate intersection
2. OR-style queries can use any-key candidate union
3. complex queries, including negation and phrase-sensitive paths, require candidate recheck

## Search algorithm rule

Current full-text search behavior is:

1. extract query keys
2. choose strategy from query structure
3. fetch candidates using:
   - `findAll` for all-key strategy
   - `findAny` for any-key or recheck strategy
4. return the candidate set for exact verification

The current wrapper explicitly documents that exact recheck against the full
`TSQuery` requires table-side fetch and higher-layer evaluation.

## Fuzzy-search structure

Current GIN implementation also includes a BK-tree structure for efficient fuzzy
matching over string-like keys, using edit distance as the metric.

This proves that the current inverted/text family is not limited to simple exact
lexeme lookup; it already contains a bounded metric-space auxiliary structure
for approximate key search.

## Posting structure and compression rule

Current GIN posting behavior proves:

1. posting lists may be stored compressed
2. compressed posting lists are validated by size and format
3. posting lists may be upgraded into posting trees beyond threshold
4. posting entries carry stable `TID` plus MGA version fields

## Capacity and page-size rule

The GIN family currently uses dynamic, page-size-based capacity calculations for:

1. pending entries per page
2. posting entries per page
3. posting-tree internal entries per page
4. posting-tree leaf TIDs per page

The current family explicitly deprecates fixed 16KB-only capacity constants.

## Full-text runtime truth rule

The full-text family returns candidate `TID`s only.

Final acceptance still depends on:

1. heap tuple fetch
2. MGA visibility
3. exact `TSQuery` consistency when recheck is required

## Fail-closed rules

The full-text path shall not:

1. treat position or weight data as directly index-key material
2. skip exact recheck for `NEED_RECHECK` queries
3. treat GIN candidate hits as final truth
4. remove dead entries before heap reclaim proof
