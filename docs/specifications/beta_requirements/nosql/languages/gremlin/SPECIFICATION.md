# Gremlin (Apache TinkerPop) Specification

**Status:** Draft (Beta)

## 1. Purpose

Define the Gremlin traversal language as a step-based pipeline. This
specification focuses on traversal syntax, step semantics, and usage guidance
for emulation.

## 2. Core Concepts

- **Traversal**: A pipeline of steps applied to a graph.
- **Traverser**: The unit of data that flows through steps.
- **Side effects**: Steps may produce side effects (aggregates, sacks).
- **Barriers**: Steps that require collecting traversers (e.g., `count()`).

## 3. EBNF

Gremlin is typically expressed as chained method calls. The following EBNF
captures the canonical traversal form:

```ebnf
traversal     = source { "." step } ;
source        = identifier ;  // typically g

step          = step_name "(" [ arg_list ] ")" ;
step_name     = identifier ;

arg_list      = arg { "," arg } ;
arg           = literal | identifier | traversal | map_literal | list_literal ;
```

## 4. Canonical Step Groups

### 4.1 Starting Steps

- `V(ids...)` - start from vertices
- `E(ids...)` - start from edges
- `addV(label)` - add vertex
- `addE(label)` - add edge

### 4.2 Filter Steps

- `has(key, value)`
- `hasLabel(label)`
- `hasId(id)`
- `where(predicate)`
- `filter(traversal)`
- `dedup()`

### 4.3 Traversal Steps

- `out(label)` / `in(label)` / `both(label)`
- `outE(label)` / `inE(label)` / `bothE(label)`
- `otherV()`
- `path()`

### 4.4 Map/Projection Steps

- `values(keys...)`
- `valueMap(keys...)`
- `properties(keys...)`
- `project(keys...).by(traversal)`
- `select(labels...)`

### 4.5 Aggregation/Barrier Steps

- `count()`
- `sum()` / `min()` / `max()` / `mean()`
- `group()` / `groupCount()`
- `order().by(...)`
- `limit(n)` / `range(low, high)`

### 4.6 Branching and Repetition

- `choose(traversal, then, else)`
- `coalesce(traversal...)`
- `union(traversal...)`
- `repeat(traversal).until(traversal).emit(traversal)`

## 5. Semantics

- Traversals begin at a traversal source (`g`) and transform traversers.
- Steps may filter traversers, expand them (graph traversal), or aggregate.
- `repeat` applies a sub-traversal; `until` stops; `emit` controls output.
- `project` creates structured outputs from traverser bindings.

## 6. Usage Guidance

**When to use Gremlin**
- Graph queries requiring fine-grained traversal control.
- Multi-hop graph exploration with conditionals.

**Avoid when**
- Simple pattern matching is preferred (Cypher is often simpler).

## 7. Examples

**Simple traversal**
```gremlin
g.V().hasLabel('User').out('FOLLOWS').values('name')
```

**Aggregation**
```gremlin
g.V().hasLabel('Order').groupCount().by('status')
```

**Repeat traversal**
```gremlin
g.V('user/123').repeat(out('FOLLOWS')).times(3).dedup().count()
```

