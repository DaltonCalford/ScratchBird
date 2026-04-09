# Neo4j Audit

## Architectural Summary

Neo4j is the strongest donor here for graph-pattern planning. Its planner is cost-based, statistics-backed, and property-aware, but the search space is graph-operator-centric rather than generic relational-family centric.

## Planning Flow

1. Cypher compilation phases normalize the query into an internal planner query.
2. `QueryPlanner.scala` builds a `LogicalPlanningContext` with:
   - graph statistics
   - cardinalities
   - provided orders
   - logical plan producer
   - query graph solver
3. Statistics-backed cardinality models estimate graph-pattern expansions, filters, horizons, and subqueries.
4. IDP-style component connectors construct joins and pattern expansions.
5. Interesting-order logic keeps ordering usefulness alive through planning.
6. Runtime pipes lower logical choices into node and relationship scans/seeks.

## How Neo4j Uses Indexes

Neo4j distinguishes graph-specific access operators:

- node index seek
- node index scan
- relationship index seek
- relationship index scan
- unique index seek
- nested index joins

This is important: index access is not a generic table scan substitute. It is tied to graph entities and pattern semantics.

Underlying schema indexes are backed by GB+Tree and related schema-index infrastructure, while planner/runtime speak in graph-native operators.

## Transaction and Visibility Interaction

Neo4j’s index runtime is transaction-state aware:

- node and relationship index transaction-state updaters exist
- unique index seeks are tied to locking semantics
- runtime tests explicitly cover tx-state-aware reads and ordering/value guarantees

So Neo4j is a useful donor for entity-specific index visibility under transactional state changes.

## What ScratchBird Should Borrow

- Family-specific access operators instead of forcing every index through one generic scan surface
- Statistics-backed cardinality models that understand domain structure
- Interesting-order reasoning within a non-relational planner
- Transaction-state-aware index updates and visibility testing

## ScratchBird Comparison Hooks

- Compare ScratchBird graph-style or path/vector families to Neo4j’s entity-specific operator design.
- Compare any future nested index join or graph pattern work to Neo4j rather than to plain SQL join donors.
