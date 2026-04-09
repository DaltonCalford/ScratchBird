# MySQL Audit

## Architectural Summary

Current MySQL is a hybrid donor. It has a newer hypergraph optimizer and interesting-order framework, but it still coexists with long-lived range-optimizer and storage-engine paths. That makes it a strong donor for search-space ideas and access-path diversity, but also a warning about prolonged planner split ownership.

## Planning Flow

1. Query blocks are converted into relational expressions and a join hypergraph.
2. `join_optimizer.cc` drives search over `AccessPath` alternatives.
3. `build_interesting_orders.*` and `interesting_orders.h` track sort/group/order states using functional dependencies and order-state propagation.
4. Range and ref access are enumerated from range-analysis helpers and table metadata.
5. Costing in `cost_model.cc` compares scans, ref access, materialization, aggregation, joins, sorts, and temp-table paths.
6. The winning `AccessPath` tree is lowered to iterators.

## How MySQL Uses Indexes

MySQL’s access path vocabulary is broad:

- table scan
- index scan
- ref / eq_ref / ref_or_null
- index range scan
- dynamic index range scan
- index skip scan
- group index skip scan
- MRR
- index merge
- full text search

The important operational features are:

- Index Condition Pushdown so storage can filter earlier
- Multi-Range Read so rowid/PK lookups are batched more cache-efficiently
- interesting-order tracking so index order can eliminate filesort or improve merge viability
- distinct and duplicate-removal paths that can exploit index order

## InnoDB Storage and Visibility Interaction

For ScratchBird, the InnoDB storage contract matters as much as the SQL optimizer:

- clustered primary key is the base storage structure
- secondary indexes point back into clustered storage
- consistent reads use read views plus undo/version traversal
- purge cleans old versions and coordinates secondary-index cleanup

This means secondary indexes are not fully self-sufficient visibility authorities. They are acceleration structures over a clustered/versioned truth.

## What ScratchBird Should Borrow

- Access-path vocabulary discipline instead of treating “index scan” as one operator
- Interesting-order state propagation as a first-class optimizer property
- ICP and MRR style execution improvements where families remain exact but can reduce CPU or random I/O
- Explicit handling of clustered-vs-secondary lookup cost and shape

## What ScratchBird Should Avoid

- Long-lived split ownership between a new search layer and older planning subsystems
- Heavily duplicated access-path legality logic across planner eras

## ScratchBird Comparison Hooks

- Compare ScratchBird property-aware planning to MySQL’s interesting-order state machine, not just to raw join enumeration.
- Compare any clustered/secondary family behavior to InnoDB’s visibility and purge contract.
- Compare any future index-merge work to MySQL’s broad `AccessPath` family rather than to simplified academic bitmap examples.
