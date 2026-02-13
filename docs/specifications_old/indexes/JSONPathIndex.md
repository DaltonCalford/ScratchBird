# JSON Path Index Specification (Beta)


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)
**Status:** Authoritative (V3)

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
- **Scope:** Core (document store / JSON-heavy workloads)

---

## Authoritative Algorithm (Normative, 2026-02-07)

This section is the implementation source of truth. If any other section in
this document conflicts with the steps below, this section wins.

### Tokenization & Path Extraction

1. Parse JSON document into a token stream (object/array/scalar).  
2. For each scalar value, emit `(path, value)` pairs:  
   - Path is canonical JSONPath (e.g., `$.a.b[3].c`).  
   - If `array_mode=elements`, normalize all array entries to `[*]`.  
3. Apply `path_mode`:
   - `explicit`: only include listed paths.  
   - `all`: include all extracted paths.  

### Value Normalization

1. Strings: apply collation rules; case fold if configured.  
2. Numbers: canonical decimal string (normalize exponent/scale).  
3. Booleans/null: literal tokens (`true/false/null`).  

### Index Entry Format

```
key = (path_hash, value_token_hash)
value = record_uuid
```

Optionally store a secondary map from `path_hash` → `path_text` for debugging.

### Insert

1. Extract `(path,value)` pairs.  
2. Normalize path and value.  
3. For each pair, insert `(path_hash,value_hash) → record_uuid` into posting list.

### Query

1. Normalize query path + value as above.  
2. Lookup posting list by `(path_hash,value_hash)`.  
3. Filter candidates by MGA visibility and (if needed) recheck JSON value.  

### Existence / Range Queries

1. For existence `json_exists(path)`:
   - Lookup by `path_hash` and return all docs with any value token.  
2. For range predicates (numeric):
   - Store numeric values in sortable form or maintain a secondary B-tree per path.  
   - Otherwise fallback to candidate recheck.

### MGA / Versioning

- Posting entries are versioned; remove via GC when obsolete.  

### Complexity Targets

- Insert: `O(k)` where `k` is number of path/value pairs.  
- Lookup: `O(log P + postings)` where `P` is #paths.  

### References (for algorithmic definitions)

- SQL/JSON standard: JSONPath semantics (ISO/IEC 9075).  
- PostgreSQL JSONPath / GIN indexing behaviors (for practical reference).  

---

## Data Model

Each indexed document contributes multiple index entries:

- `(path_id, value_token) -> list of record_uuid`

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
- Optional helper table (required): `sys.index_path_defs`

**sys.index_path_defs (required)**
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

## Record Identity Requirements

Posting lists must store `record_uuid` with optional `SBRecordPtr` cache hints.
Legacy TID encodings are not permitted.

## Observability

Expose JSON path index details via:

- `sys.indexes` (index type, params)
- `sys.index_path_defs` (if implemented)

## Dependencies

- `ScratchBird/docs/specifications/parser/v3/indexes/INDEX_ARCHITECTURE.md`
- `ScratchBird/docs/specifications/parser/v3/dml/DML_XML_JSON_TABLES.md`
- `ScratchBird/docs/specifications/parser/v3/types/03_TYPES_AND_DOMAINS.md`

## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
