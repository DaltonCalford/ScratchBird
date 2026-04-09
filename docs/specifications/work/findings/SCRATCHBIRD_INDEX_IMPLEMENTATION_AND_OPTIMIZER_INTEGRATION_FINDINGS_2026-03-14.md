# ScratchBird Index Implementation And Optimizer Integration Findings

Status: Canonical findings

Date: 2026-03-14

## 1. Purpose

This findings note establishes the research boundary for the next specification wave focused on ScratchBird indexes and optimizer integration.

The purpose is not to replace section 18. The purpose is to:

- inventory the real index families currently present in ScratchBird
- distinguish implemented runtimes from compatibility-only labels
- identify where MGA semantics are already explicit in code
- identify where the optimizer currently understands and uses indexes
- define the exact gap between the authoritative index specs and the current engine/planner behavior

Section 18 remains the authoritative index framework:

- `docs/specifications/18_Index_Framework/`

This findings note and the related planning documents complement section 18; they do not supersede it.

## 2. Existing Canonical Spec Boundary

Section 18 is already a mature canonical specification family rather than a thin outline.

What section 18 already covers:

- index architecture, lifecycle states, build modes, and DML integration
- MGA visibility and candidate recheck rules
- metrics, usage counters, health counters, and cost formulas
- build, maintenance, vacuum, sweep, and rebuild workflows
- per-family canonical specs for classic relational, spatial, inverted, summary, write-optimized, columnar, and vector families

What the new research must add on top of section 18:

- implementation-depth review per actual engine family
- donor-engine research on the best implementation patterns per family
- per-family optimizer applicability and metric requirements
- the exact planner/executor gap between current code and section 18 expectations
- a conversion plan to produce integrated engineering specs for index-plus-optimizer behavior

Inference:

- the right target is not “new index specs”
- the right target is “research and specification closure between section 18, actual code, MGA rules, and optimizer behavior”

## 3. Current ScratchBird Index Family Inventory

Current code review indicates the following real engine families exist in ScratchBird:

- B-tree
- Hash
- R-tree
- GiST
- SP-GiST
- BRIN
- Bitmap
- LSM
- GIN
- Inverted/fulltext
- HNSW/vector
- Columnstore

Compatibility or dialect-derived labels often route into these engines rather than representing separate implementations.

Examples:

- `ART`, `STL_SORT`, `NEO4J_RANGE`, and related aliases route to B-tree
- `REDIS_STRING`, `REDIS_HASH`, and related aliases route to Hash
- `MONGODB_2D`, `MONGODB_2DSPHERE`, and `REDIS_GEO` route to R-tree
- many vector-family labels route to the HNSW runtime
- `FULLTEXT`, `INVERTED`, `TRIE`, `NGRAM`, `SPARSE_*`, and similar labels route to the inverted-index runtime

Practical implication:

- research lanes must be organized by actual runtime family first
- alias families should be treated as semantic or opclass variants unless code proves otherwise

## 4. MGA And Visibility Baseline

The codebase already shows explicit MGA-related handling across the major index families.

Observed patterns:

- `xmin`, `xmax`, or `current_xid` appear in B-tree, hash, R-tree, GiST, SP-GiST, BRIN, GIN, inverted/fulltext, HNSW, and columnstore family APIs
- bitmap indexes carry per-entry version semantics
- heap visibility recheck remains present in the storage layer
- section 18 already states that index scan results are candidate rows and must pass MGA visibility checks

Practical implication:

- the research wave should not reopen whether MGA applies to indexes
- it should verify family-by-family whether the current implementation actually satisfies the section 18 MGA contract and where it does not

## 5. Optimizer Integration Baseline

The current relational optimizer only uses a narrow subset of the implemented index surface.

What the optimizer currently uses directly:

- B-tree
- LSM

What the optimizer currently models around those families:

- index scan
- index-only scan
- LSM scan
- heuristic skip-scan
- a `BITMAP_INDEX_SCAN` path that is really a multi-B-tree combination, not the bitmap-index family

What is not yet integrated into the normal relational optimizer path:

- Hash
- R-tree
- GiST
- SP-GiST
- BRIN
- Bitmap-family indexes
- GIN and generic inverted/fulltext families
- HNSW/vector
- Columnstore

These families may still have:

- executor routing
- SBLR entry points
- direct runtime APIs

but they do not currently have first-class normal relational planner integration.

This is the central reason the new research wave is required.

## 6. Core Gap Statement

There are now two distinct truths in the system:

1. Section 18 specifies a broad and rich index ecosystem with MGA, metrics, maintenance, and costing contracts.
2. The current optimizer materially understands only a small subset of that ecosystem.

The new research wave must close this gap by answering, per real index family:

- what is actually implemented now
- what the best donor-engine implementation pattern is
- which metrics the optimizer truly needs
- how MGA correctness must be verified
- how the planner and executor should exploit the family

## 7. Required Research Families

The research program should be organized around the following actual runtime families and cross-cutting concerns:

- ordered exact-match and range families: B-tree, Hash, LSM
- lossy and summary families: BRIN, Bitmap, Columnstore
- generalized search and spatial families: R-tree, GiST, SP-GiST
- inverted and text families: GIN, inverted/fulltext, n-gram, sparse and trie-routed families
- vector and ANN families: HNSW and routed vector aliases
- MGA lifecycle and garbage-collection correctness across all families
- optimizer metrics, path applicability, costing, and access-path integration across all families

## 8. Immediate Conclusion

The new research wave should not be written as a second copy of section 18.

It should be written as:

- one implementation-and-gap research program
- one optimizer-integration research program for indexes
- one specification conversion workplan that translates the research into section-18-aligned engineering specs and optimizer-spec updates
