# Redis Audit

## Architectural Summary

Core Redis is not a query optimizer in the relational sense. It is a command-specialized execution engine over chosen data structures. That makes it a poor donor for cost-based planning and a very good donor for “pick the right physical structure and make the hot path brutally simple.”

## Execution Model

- There is no generic logical planner.
- Each command knows the data structure it targets.
- Performance comes from direct algorithm selection, encoding choices, and single-threaded deterministic execution.

## How Redis Uses “Indexes”

### Sorted sets and GEO

- GEO commands encode geohashes into sorted-set scores.
- Radius or box queries prune candidates using score ranges/geohash neighborhoods first.
- Exact distance filtering happens afterwards.

### SORT

- `SORT` parses a command-specific mini-language and uses direct sorting, lookup-by-pattern, and partial quicksort for `LIMIT`.
- This is command-level optimization, not a reusable plan search framework.

### Modules

- `module.c` shows that Redis exposes a strong module boundary.
- Advanced query/index systems are expected to arrive through modules rather than through one core optimizer.

## Transaction and Visibility Model

- single-threaded command serialization
- MULTI/EXEC and scripting, not MVCC
- no multi-version visibility engine for indexes to integrate with

## What ScratchBird Should Borrow

- Ruthless specialization of hot access paths once the family is known
- Encoded-key search spaces like GEO/geohash where score space can cheaply prune candidates
- Module-style separation between core engine truth and optional advanced family logic

## What ScratchBird Should Not Borrow

- Lack of a generic planner
- Overreliance on command-specific logic where a reusable optimizer is required

## Scope Note

This audit covers core Redis only. It does not cover RediSearch or Redis vector search modules. If ScratchBird wants a direct donor for Lucene-like or ANN-like Redis behavior, that requires a separate RediSearch-focused audit.

## ScratchBird Comparison Hooks

- Use Redis as a donor for “fast physical operator once family is chosen,” not as a donor for planner completeness.
