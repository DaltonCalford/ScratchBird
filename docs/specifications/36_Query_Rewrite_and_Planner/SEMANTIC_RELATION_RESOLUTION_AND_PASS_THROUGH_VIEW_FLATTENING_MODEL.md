# Semantic Relation Resolution and Pass-Through View Flattening Model

## Status

Current code-backed authority with reconstructed commercial-grade detail.

## Purpose

This document defines the semantic-analysis phase that converts parser-v3 relation references and simple scan predicates into planner-ready relation bindings. It also defines the bounded pass-through view flattening behavior that is currently implemented and must be preserved until a wider view-rewrite model is promoted.

## Scope

This section governs:

- relation binding from parser-v3 relation references into concrete catalog objects
- alias and qualifier resolution used by predicate matching
- stored pass-through view flattening into a physical base relation
- materialized-view substitution when a materialized table already exists
- extraction of simple planner-visible scan predicates from parser-v3 expressions
- early index-family candidate attachment for simple predicates

This section does not authorize:

- arbitrary view inlining
- decorrelation
- semantic flattening of complex stored view trees
- join reordering
- multi-index predicate synthesis beyond the current simple-predicate path

## Inputs

The semantic analyzer consumes:

- parser-v3 AST statements and expressions
- string-pool resolved identifier text
- committed catalog metadata
- current statistics manager index-family metrics packets

## Relation Resolution Contract

Each relation must be resolved into a `ResolvedRelation` object with these properties:

- concrete base relation identity
- stable table path string
- alias text used for later column-token matching
- column inventory
- index inventory
- view metadata when the original relation reference resolved through a view

Resolution must proceed in this order:

1. resolve the visible schema candidate set
2. resolve the relation name against committed catalog objects
3. if the target is a physical table, populate the resolved physical relation directly
4. if the target is a stored view, attempt bounded pass-through flattening
5. if flattening fails, retain the relation as a view-backed relation and do not pretend it is planner-equivalent to a base table

The analyzer must fail closed when relation identity is ambiguous.

## Schema Candidate Collection

Schema candidates are collected from:

- explicitly qualified schema names in the relation reference
- the relation owner's default schema when already known
- visible session search-path style schema candidates where the current parser/binder path provides them

Duplicate schema ids must be removed before lookup.

## Pass-Through View Flattening

Current flattening is intentionally narrow.

Stored view flattening is permitted only when all of the following hold:

- the catalog entry is a stored view
- recursive flattening does not revisit the same view id
- the stored view payload decodes to a simple `SELECT`
- every select item is one of:
  - `SELECT *`
  - `SELECT table.*`
  - direct column reference
- the `FROM` clause contains a single table target
- the stored view does not reference:
  - nested query targets
  - table functions
- the target schema path decodes cleanly

If the view is materialized and already has a materialized table id, semantic resolution must bind to that materialized table instead of recursively decoding the stored view text.

If the decoded target resolves to another view, one more flattening step may be attempted through the same guarded recursion path. Cycles must be rejected.

## Flattening Refusal Rules

Flattening must be refused when any of the following are present:

- explicit expressions in the select list
- computed projections
- aggregate projections
- function targets in the `FROM` clause
- nested subqueries in the `FROM` clause
- a missing or undecodable target schema path
- recursive revisit of the same view id

Refusal means the semantic analyzer retains view-backed semantics. It must not silently downgrade the object into a base-table relation.

## Column Token Resolution

Simple predicate extraction uses column tokens made from:

- unqualified column name
- optional qualifier taken from the last path component of the table qualifier

Relation matching must compare the lowered qualifier against:

- relation alias
- base table name
- full relation table path

A column token is resolvable only when exactly one relation exposes the matching column under the permitted qualifier rules. Multiple matches are ambiguous and must be rejected.

## Simple Predicate Extraction

The current planner-visible predicate extraction path is intentionally limited to simple scan predicates.

Supported simple predicates are:

- binary equality: `=`
- binary range: `<`, `<=`, `>`, `>=`
- prefix-like predicates: `LIKE` where the literal pattern is a prefix form or a parameter

The extractor must unwrap simple casts before checking the predicate operands.

For binary predicates, one side must resolve to a column token and the other side must resolve to:

- literal integer
- literal floating-point value
- literal string
- literal boolean
- bound parameter

If both sides are columns, both sides are expressions, or the literal side is not extractable, the predicate is not admitted to the simple scan-predicate path.

For `LIKE`, literal prefix matching is admitted only when:

- the string is non-empty
- the first character is not `%`
- the pattern contains `%`

Parameterized `LIKE` is admitted as a simple predicate but remains parameter-shaped rather than literal-shaped.

## Logical Composition Boundary

The simple extractor handles conjunction by recursively collecting predicates from `AND` trees.

The simple extractor does not admit:

- `OR`
- arbitrary boolean nests
- non-prefix pattern matching
- expression-to-expression comparison
- function-wrapped predicate columns that are not removable through cast unwrapping

Those forms remain outside this current simple predicate admission path and must not be represented as if they were ordinary scan-key predicates.

## Immediate Index Candidate Attachment

For each admitted simple predicate, semantic analysis may attach one best current index candidate before the main plan search proceeds.

Candidate selection is restricted to indexes whose leading column matches the resolved predicate column.

For each candidate index:

1. build a planner family lowering request from:
   - index type
   - predicate match shape
   - operator name
2. lower the request into a `PlannerFamilyLoweringResult`
3. reject the candidate if the lowering result is `INVALID`
4. if metrics are available, load the current `IndexFamilyMetricsPacket`
5. score the candidate by:
   - queryability state
   - exactness class
   - recheck requirement
   - ordering support for range or prefix-like predicates
   - covering support
   - coverage fraction
   - recheck ratio
   - absolute correlation
6. choose the single best candidate with deterministic tie-breaks

Deterministic tie-break order is:

1. higher composite score
2. lower recheck ratio
3. higher coverage fraction
4. higher absolute correlation
5. lexicographically smaller index name

## Planner-Facing Output

Each admitted predicate must carry:

- resolved relation index
- resolved column id
- resolved column name
- predicate kind
- operator name
- literal kind
- literal text or parameter placeholder
- original rendered predicate text

When a best index match exists, the predicate record must also carry:

- matched index identity
- matched lowered planner family
- matched lowered path name

This attachment is advisory planner input. It is not permission to skip later MGA visibility rules or later family-specific rejection.

## MGA Rule

Semantic relation resolution and predicate extraction do not redefine visibility.

Any admitted index candidate remains candidate-only. Final row acceptance is still controlled by:

- Firebird-style MGA visibility
- committed catalog state
- exact or recheck semantics of the lowered family

## Operator-Visible Improvement Captures

The following are known improvement lanes and are not current authority:

- broader view flattening beyond simple pass-through trees
- OR-aware predicate bundle extraction
- multi-index combination from semantic-analysis output
- richer function-index normalization during semantic admission

These lanes may be added later, but they must not be implied today.
