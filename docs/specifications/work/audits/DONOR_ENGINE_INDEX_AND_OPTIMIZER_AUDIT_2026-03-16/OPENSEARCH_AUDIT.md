# OpenSearch Audit

## Architectural Summary

OpenSearch is not a relational optimizer donor. It is a donor for rewrite-heavy query planning, collector orchestration, and per-family search execution over Lucene index structures. Its planning strength is not join order; it is query rewriting plus efficient shard-local execution.

## Search Flow

1. `SearchService` builds a `SearchContext`, validates PIT/scroll/sort state, and resolves shard-local execution needs.
2. Query builders are rewritten through OpenSearch and Lucene rewrite stages.
3. Query rewriters simplify nested boolean structures and other shapes before execution.
4. `QueryPhase` executes the already-rewritten query over a `ContextIndexSearcher`.
5. Collector contexts determine top docs, total hit counting, early termination, aggregations, rescoring, and profiling.
6. Fetch phase materializes final fields after shard-local top hits are known.

## How OpenSearch Uses Indexes

Index use is Lucene-centric:

- inverted postings for text and boolean retrieval
- doc values for sorting, grouping, aggregations, and collapse
- BKD/point trees for numeric, date, and geo range predicates
- vector and completion families via specialized query builders and segment readers
- star-tree and aggregation rewrite helpers for some aggregation workloads

## Important optimization behaviors

- bool query flattening and other rewrites reduce collector overhead and query complexity
- search-after and PIT contexts stabilize repeated reads
- if query sort is a prefix of index sort, `QueryPhase` can early terminate or skip more work
- aggregations and collectors are assembled per query shape rather than through one generic path

## Transaction and Visibility Model

OpenSearch is near-real-time:

- refresh publishes new segments
- PIT/search contexts stabilize view consistency
- there is no row-MVCC visibility model comparable to PostgreSQL or Firebird

So the donor value is not transactional parity. It is:

- rewrite discipline
- collector specialization
- per-family query lowering

## What ScratchBird Should Borrow

- Query rewrite pipeline before final access-path comparison
- Explicit collector/executor specialization for top-k, filter-only, aggregation-heavy, and search-after workloads
- Better exploitation of index order for early termination and partial result pruning

## ScratchBird Comparison Hooks

- Compare ScratchBird text/vector/search-family lowering against OpenSearch’s rewrite -> query-phase -> collector flow.
- Compare future exact-vs-approximate vector and text families against Lucene-backed query builders, not against SQL B-tree intuition.
