# MariaDB Audit

## Architectural Summary

MariaDB is still a major donor for mature range analysis, partition pruning, and storage-engine delegation. It is less of a donor for a single unified modern planner front door than current MySQL trunk, but stronger than many engines in formal interval analysis.

## Planning Flow

1. Conditions are normalized into interval and merge structures in `opt_range.cc`.
2. `SEL_TREE`, `SEL_ARG`, and `SEL_IMERGE` represent conjunctions, disjunctions, and index-merge candidates over index space.
3. Partition pruning runs through the same interval-analysis mindset.
4. Quick-select objects are built for range, index_merge, and group-by-minmax retrieval.
5. Filesort, ICP, and storage-engine cost/handler capabilities complete the access choice.

## How MariaDB Uses Indexes

MariaDB’s core optimizer is very strong at:

- reducing predicates to index-space intervals
- combining multiple interval trees into index_merge plans
- using group-by-minmax shortcuts
- partition pruning before full retrieval

The key strength is that the interval representation is explicit and reusable. This is useful for ScratchBird if it wants a formal legality layer for range-capable families.

## Storage-Engine Reality

MariaDB is not one index engine. The core optimizer delegates meaningfully to storage engines:

- InnoDB remains the main transactional donor
- ColumnStore, RocksDB, Mroonga, and other engines can expose different access behavior

So the MariaDB lesson is twofold:

- keep a strong core legality model
- let index families advertise real capabilities instead of faking one universal AM

## Transaction and Visibility Interaction

For InnoDB-backed transactional behavior, the same clustered/secondary consistent-read story as MySQL applies. For non-InnoDB engines, semantics vary. MariaDB therefore reinforces a design point ScratchBird also needs:

- the planner must know whether a family is exact, partially exact, or storage-engine-specific
- transaction semantics cannot be assumed uniform across all families unless the engine enforces them

## What ScratchBird Should Borrow

- Formal interval-tree/range-tree representation for range-capable families
- Partition-pruning-before-read discipline
- Explicit family capability contracts at the optimizer boundary

## ScratchBird Comparison Hooks

- Compare ScratchBird range-capable families against MariaDB `SEL_TREE` formalism.
- Compare future partition and chunk pruning logic against MariaDB’s “same reasoning, different target” pruning pattern.
- Use MariaDB as the donor for legality algebra, not as the donor for a completely unified planner front door.
