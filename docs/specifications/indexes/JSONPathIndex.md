# JSON Path Index Specification (Beta)

**Status:** Draft (Beta)

## Purpose

Define a JSON path index that accelerates predicates over JSON/JSONB columns.
The index stores path/value pairs to support fast existence, equality, and
range filtering without full document scans.

This is a storage and catalog specification. Query syntax is defined in the
native SQL and parser specs.

## Summary

- **Index name:** `JSON_PATH` (Beta index type)
- **Primary use:** JSON/JSONB path predicates
- **Storage style:** GIN-like inverted index keyed by path + value token
- **Compatible columns:** JSON, JSONB
- **Optional:** Beta (document store / JSON-heavy workloads)

## Data Model

Each indexed document contributes multiple index entries:

- `(path_id, value_token) -> list of TID`

Where:
- `path_id` is the normalized JSON path (or path hash)
- `value_token` is the normalized scalar value (string/number/bool/null)

Arrays contribute one entry per element (configurable).

## Path Syntax and Normalization

- **Path language:** JSONPath (aligned with JSON_TABLE spec)
- **Normalization:**
  - Normalize path tokens to lowercase unless `case_sensitive_paths` is true
  - Encode array traversal as `[*]` unless index configured for strict indices

## Value Normalization

- Strings: normalized by collation rules (default UTF-8)
- Numbers: normalized to canonical decimal string
- Booleans: `true` / `false`
- Null: literal token

## Index Options (index_params_oid)

Store JSON parameters in `index_params_oid` (TOAST). Suggested shape:

```
{
  "path_mode": "explicit" | "all",
  "paths": ["$.user.id", "$.tags[*]"],
  "array_mode": "elements" | "ignore" | "strict",
  "case_sensitive_paths": false,
  "case_sensitive_values": false,
  "include_nulls": true
}
```

- `path_mode=all` indexes all discovered paths (high cost)
- `paths` is required when `path_mode=explicit`

## Catalog Wiring

### Index Type

Add `JSON_PATH` as a Beta index type in the index enum and parser index type
mapping. This is a distinct type (not an alias of GIN) to make intent explicit.

### Catalog Tables

- `sys.indexes` stores `index_type=JSON_PATH`
- `index_params_oid` stores JSON path config
- Optional helper table (planned): `sys.index_path_defs`

**sys.index_path_defs (planned)**
- `index_id` UUID (FK -> sys.indexes)
- `path_text` TEXT
- `path_hash` BINARY(8)
- `is_array_path` BOOLEAN

## DDL Stub (Syntax Placeholder)

```
CREATE INDEX idx_user_path
ON users
USING JSON_PATH (profile)
WITH (
  paths = ('$.user.id', '$.tags[*]'),
  array_mode = 'elements'
);
```

## Query Behavior (Targeted)

The index should accelerate:

- Existence checks: `json_exists(col, '$.path')`
- Equality checks: `json_value(col, '$.path') = 'x'`
- Membership checks: `json_value(col, '$.path') IN (...)`
- Array membership: `json_path(col, '$.tags[*]') CONTAINS 'x'`

When the predicate uses non-indexed paths, the planner should fall back to
full JSON evaluation.

## Maintenance and GC

- Index entries follow MGA visibility rules
- Delete/Update must remove prior path/value tokens
- GC uses the standard index GC protocol

## Observability

Expose JSON path index details via:

- `sys.indexes` (index type, params)
- `sys.index_path_defs` (if implemented)

## Dependencies

- `ScratchBird/docs/specifications/indexes/INDEX_ARCHITECTURE.md`
- `ScratchBird/docs/specifications/dml/DML_XML_JSON_TABLES.md`
- `ScratchBird/docs/specifications/types/03_TYPES_AND_DOMAINS.md`

