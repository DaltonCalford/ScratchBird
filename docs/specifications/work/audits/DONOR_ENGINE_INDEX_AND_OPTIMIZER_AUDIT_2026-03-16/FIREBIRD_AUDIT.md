# FirebirdSQL Audit

## Architectural Summary

Firebird is the strongest donor for MGA-sensitive retrieval because optimizer, record visibility, backout, and index use all stay close to one another. Its optimizer is not as broad in access-method family count as PostgreSQL, but it is unusually unified for transactionally exact retrieval.

## Planning and Retrieval Flow

1. Query compilation feeds boolean predicates into the optimizer.
2. `Optimizer.cpp`, `InnerJoin.cpp`, and `OuterJoin.cpp` break the problem into streams and join rivers.
3. For each stream, `Retrieval.cpp` evaluates candidate index matches, sort navigation possibilities, and selectivity/cardinality.
4. The optimizer chooses stream order based on cost and independence, not on a blind “prefer primary key” rule.
5. Retrieval generates inversion trees that can compose AND/OR index usage instead of reducing every condition to one index choice.
6. Join execution can choose indexed navigation, sort/merge, or hash strategies depending on predicate shape and ordering opportunity.

## How Firebird Uses Indexes

- Index matching is segment-aware and compound-index-aware.
- Fully matched unique indexes are treated as dominant and can terminate broader index competition.
- Partial compound matches use improved segment selectivity rather than collapsing to a binary “usable or unusable” rule.
- `STARTING WITH`, `IS NULL`, OR-conjunction splitting, and index navigation for ORDER BY all feed directly into retrieval planning.
- `idx.cpp` and `btr.cpp` keep index maintenance tightly tied to transactional record operations.

The physical B-tree implementation includes:

- compressed jump-node search structures
- duplicate handling ordered by record number
- segment-level selectivity storage
- corruption checks and repair-safe invariants

## MGA and Visibility Interaction

This is the most important donor property:

- `tra.h` keeps savepoints, snapshot handles, snapshot numbers, undo, and transaction flags in the transaction core
- `vio_proto.h` exposes `VIO_backout`, `VIO_chase_record_version`, `VIO_modify`, `VIO_erase`, and garbage collection as central record lifecycle operations
- index maintenance is not allowed to invent its own visibility truth; it operates within VIO and transaction semantics

In practice this means:

- version chasing is native
- savepoint backout is native
- index cleanup and duplicate checking are transaction-aware
- MGA visibility is not layered on afterwards

## What ScratchBird Should Borrow

- Retrieval-first index planning where predicate analysis, ordering, and join order are solved together
- Native inversion composition for AND/OR index usage
- Central ownership of record lifecycle and backout
- Tight coupling between index validity and MGA truth

## What ScratchBird Should Exceed

- Broader family coverage than Firebird while keeping Firebird-grade semantic ownership
- Better observability and runtime-plan evidence around why an index was or was not trusted
- Explicit exact/recheck/approximate labeling for advanced families that Firebird never had to solve

## ScratchBird Comparison Hooks

- Compare ScratchBird savepoint/backout and visibility ownership against Firebird `tra.h` + `vio.cpp` style ownership, not against a lighter MVCC donor.
- Compare ScratchBird B-tree and uniqueness behavior against Firebird `idx.cpp` / `btr.cpp` rather than against generic B-tree literature.
- Use Firebird as the reference for “MGA-safe exact retrieval,” not for full AM breadth.
