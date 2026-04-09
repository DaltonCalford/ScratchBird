# PostgreSQL Audit

## Architectural Summary

PostgreSQL remains the clearest donor for a complete SQL cost-based optimizer with mature access-method contracts. Its planner is built around `RelOptInfo` and `Path` generation, not around one-off scan selection. The important point for ScratchBird is that PostgreSQL does not treat “using an index” as one thing. It distinguishes:

- exact ordered scans
- bitmap-producing scans
- lossy page-pruning scans
- index-only scans gated by visibility support
- exact AMs that still require heap recheck

## Planning Flow

1. `planner.c` and `planmain.c` normalize the query and build the top-level planning problem.
2. `allpaths.c` creates one `RelOptInfo` per base relation and calls `set_rel_pathlist()`.
3. For ordinary tables, `create_index_paths()` in `indxpath.c` adds index, bitmap, and parameterized access alternatives to the base relation.
4. Join enumeration proceeds in dynamic-programming passes over subsets in `joinrels.c`, while `add_paths_to_joinrel()` in `joinpath.c` constructs nested-loop, merge, and hash alternatives.
5. `costsize.c` assigns comparable costs to candidate paths.
6. The cheapest path that satisfies required ordering or upper-stage needs becomes the eventual `Plan`.
7. If join search explodes, GEQO supplies alternate tree search while still reusing the regular per-join path generation.

## How PostgreSQL Uses Indexes

### B-tree

- B-tree is the default exact ordered access method.
- `nbtree` implements a Lehman-Yao high-concurrency tree with right links, high keys, suffix truncation, deduplication, and scan-safe split behavior.
- Planner uses B-tree for equality, range, ordering, prefix ordering, and index-only scans when visibility-map conditions allow it.

### Bitmap Paths

- Planner can combine multiple indexes into bitmap paths before heap access.
- Bitmap heap scan is explicitly lossy-capable; it prunes page ranges first and rechecks tuples later.

### GIN

- GIN is a generalized inverted index storing keys plus posting lists or posting trees.
- It is built for full-text, arrays, and “many keys per row” workloads.
- Fast update buffers pending entries and later merges them into the main structure.
- Planner treats GIN as an AM with its own exactness and recheck expectations, not as a disguised B-tree.

### BRIN

- BRIN stores summaries for heap page ranges, not tuple pointers.
- It only supports bitmap output because it is fundamentally lossy.
- Query execution must recheck tuples after BRIN narrows the heap pages.

### GiST and SP-GiST

- These are extensible operator-class frameworks for geometric, nearest, text, and space-partitioned workloads.
- Planner relies on operator-class metadata to know when these AMs are exact, lossy, or recheck-heavy.

## Visibility and Transaction Interaction

PostgreSQL’s index contract is inseparable from heap visibility:

- tuple visibility is determined in `heapam_visibility.c`
- committed state is resolved through snapshot checks, proc array state, and transaction status storage
- hint bits and visibility map reduce future cost
- index-only scan is only valid when the page-level visibility information says the heap does not need to be consulted

That means PostgreSQL’s optimizer and executor understand that “index says maybe” is often different from “tuple is visible and matches.”

## What ScratchBird Should Borrow

- A single canonical planner front door that always produces the same path vocabulary
- A clear split between exact access, bitmap/lossy access, and index-only access
- Property-aware path competition instead of raw scan-type competition
- AM contracts that say whether the executor may trust the index answer directly, must recheck the heap, or must treat it as page pruning only
- Visibility support surfaces that make index-only semantics explicit, not accidental

## What ScratchBird Should Exceed

- Family-specific diagnostics in runtime plan metadata
- MGA-aware access-path selection rather than a pure heap-visibility afterthought
- Native planner treatment for vector, text, summary, and approximate families beyond PostgreSQL’s historical core strengths

## ScratchBird Comparison Hooks

- Compare ScratchBird planner front door and path vocabulary against PostgreSQL `planner.c` -> `allpaths.c` -> `indxpath.c` -> `joinpath.c`.
- Compare ScratchBird B-tree, summary, inverted, and vector families against PostgreSQL’s exact/recheck/lossy classification.
- Compare ScratchBird index-only ambitions against PostgreSQL visibility-map discipline rather than against marketing-level “covering index” claims.
