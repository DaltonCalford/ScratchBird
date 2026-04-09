# Beta 2 Property Graph Storage And Pattern Matching Model

## Purpose

Define the native property-graph overlay used to bind relational rows into
graph vertices and edges and expose deterministic pattern matching.

## Governing rules

1. Property-graph identity is cataloged and UUID-based.
2. Graph storage may reuse relational tables, but graph binding metadata is
   first-class.
3. Pattern matching is lowered into shared canonical operators; it is not a
   private execution engine.
4. Graph traversals must publish exactness and recursion limits.

## Canonical metadata

- `sb_graph_catalog`
  - `graph_uuid`
  - `graph_name`
  - `status`
- `sb_graph_vertex_binding`
  - `binding_uuid`
  - `graph_uuid`
  - `table_uuid`
  - `label_set`
  - `vertex_id_expr`
- `sb_graph_edge_binding`
  - `binding_uuid`
  - `graph_uuid`
  - `table_uuid`
  - `label_set`
  - `source_vertex_expr`
  - `target_vertex_expr`

## Pattern model

Admitted Beta 2 pattern classes:

- single-hop match
- bounded multi-hop match
- label and property filter
- path existence

Unbounded arbitrary graph-mining work remains outside this file.

## Lowering flow

1. Parser builds graph-pattern AST.
2. Catalog resolves graph bindings.
3. Planner lowers the pattern into bound vertex and edge scans, joins, and path
   expansion operators.
4. Runtime enforces hop bounds and exactness rules.

## Refusal rules

- `GRAPH_UNKNOWN`
- `GRAPH_BINDING_INVALID`
- `GRAPH_PATTERN_UNBOUNDED`
- `GRAPH_PATH_EXPANSION_REFUSED`

## Example

```sql
create graph social_graph;
bind vertices users as label person into graph social_graph;
bind edges follows as label follows into graph social_graph;
match (a:person)-[:follows]->(b:person) where a.user_id = 42 return b.user_id;
```

## Cross-section requirements

- section `24` owns graph catalogs and bindings
- section `21` owns graph pattern grammar
- section `17` owns graph-analytics UDR interplay
