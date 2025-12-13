# ScratchBird Audit: Data Types, Catalogs, and DDL Coverage

Scope: snapshot of implemented data types, system catalogs, and DDL surface with dependency handling status. Source basis: `include/scratchbird/core/types.h`, catalog root in `src/core/catalog_manager.cpp`, and current executor/parser wiring (Alpha 3 in progress).

## Data Types (core::DataType)
- Numeric: INT8/16/32/64/128, UINT8/16/32/64, FLOAT32/64, DECIMAL, MONEY
- Strings: CHAR, VARCHAR, TEXT
- Binary: BINARY, VARBINARY, BLOB, BYTEA
- Temporal: DATE, TIME, TIMESTAMP (with/without tz flag), INTERVAL
- Boolean: BOOLEAN
- Special: UUID, JSON, JSONB, XML, VECTOR
- Spatial: POINT, LINESTRING, POLYGON, MULTIPOINT, MULTILINESTRING, MULTIPOLYGON, GEOMETRYCOLLECTION
- Arrays/Composite: ARRAY, COMPOSITE
- Text search: TSVECTOR, TSQUERY
- Ranges: INT4RANGE, INT8RANGE, NUMRANGE, TSRANGE, TSTZRANGE, DATERANGE
- Network: INET, CIDR, MACADDR, MACADDR8
- Polymorphic: VARIANT
- NULL_TYPE

### Type support audit (cast/convert/extract/manipulation)
- Implemented/verified: core expression evaluator covers numerics, strings, booleans, basic temporal, UUID; tests exist for v2 data types (`tests/unit/test_v2_datatypes_comprehensive.cpp`, range/network/text-search suites).
- Partially/unknown: VECTOR, JSON/JSONB advanced ops, full spatial ops, polymorphic VARIANT, complex range operators, full temporal extract/interval arithmetic. Needs explicit coverage review in evaluator and parser function tables.
- Action: create per-type checklist against evaluator/operator registry and add missing tests/op mappings.

## System Catalogs (root pages persisted)
- Core: schemas, tables, columns, indexes, tablespaces, tablespace_files, constraints, sequences, views, triggers, permissions, statistics, collations, timezones, charsets, collation_defs.
- Security/ACL: users, roles, groups, role_memberships, group_memberships, group_mappings, object_permissions, column_permissions, policies.
- Dependencies/comments: dependencies, comments.
- Stored code: procedures/functions, procedure_params, domains, UDR, packages, exceptions.
- Emulation: emulation_types, emulation_servers, emulated_dbs.
- Phase B/FDW: synonyms, foreign_servers, foreign_tables, user_mappings, server_registry, udr_engines, udr_modules.
- Migration/history: migration_history.

Notes:
- Exceptions table now allocated and persisted in root.
- Verify on-disk enums and caches include new `ObjectType::EXCEPTION`.
- Optimization targets: dependency lookups/indexing for heavy DDL; catalog read paths for missing caches.

## DDL Coverage Matrix (snapshot)
Legend: [x]=implemented; [ ]=missing/unknown; Depends On=validates references on create/alter; Depended On=blocks drop when dependents exist (currently partial).

| Object            | Create | Alter | Drop | Depends On | Depended On | Notes |
|-------------------|:------:|:-----:|:----:|:----------:|:-----------:|-------|
| Schema            | [x]    | [x]   | [x]  | [ ]        | [partial]   | Drop may not scan dependents uniformly |
| Table             | [x]    | [x]   | [x]  | [partial]  | [partial]   | FK/columns checked; dependency block incomplete |
| View              | [x]    | [ ]   | [x]  | [partial]  | [partial]   | Needs full dep record on create/alter, drop block |
| Index             | [x]    | [partial] | [x] | [partial] | [partial] | Expression/partial index deps not enforced |
| Sequence          | [x]    | [x]   | [x]  | [ ]        | [partial]   | Drops not blocked if referenced defaults |
| Domain            | [x]    | [ ]   | [x]  | [ ]        | [partial]   | Depends/depended handling not wired |
| Constraint (PK/UK/CK) | [x] | [x] | [x] | [partial] | [partial] | FK parent blocking partial; checks not linked in deps table |
| Foreign Key       | [x]    | [x]   | [x]  | [partial]  | [partial]   | Needs dependency entries and drop blocking |
| Trigger           | [x]    | [partial] | [x] | [partial] | [partial] | Function/proc dependency needs enforcement |
| Function          | [x]    | [ ]   | [x]  | [partial]  | [partial]   | Stored-code deps recorded; alter refresh missing |
| Procedure         | [x]    | [ ]   | [x]  | [partial]  | [partial]   | Same as functions |
| Package           | [x]    | [ ]   | [x]  | [partial]  | [partial]   | Best-effort pkg dep; needs full enforcement |
| UDR               | [x]    | [ ]   | [x]  | [partial]  | [partial]   | Library/entry deps not enforced |
| Exception         | [x]    | [ ]   | [x]  | [ ]        | [partial]   | Catalog present; dependencies not linked |
| Sequence-owned defaults | [x] | [ ] | [x] | [partial] | [partial] | Defaults not blocked on drop |
| Synonym           | [ ]    | [ ]   | [ ]  | [ ]        | [ ]         | Exists in catalog; feature not wired |
| FDW objects       | [ ]    | [ ]   | [ ]  | [ ]        | [ ]         | Catalog entries exist; runtime/DDL TBD |
| Emulation entries | [x]    | [ ]   | [x]  | [ ]        | [ ]         | Minimal validation |
| Tablespace        | [x]    | [x]   | [x]  | [ ]        | [partial]   | Drop blocks primary only; dependent objects not fully enforced |

## Dependency Integrity Gaps (high level)
- No uniform create/alter validation against catalog (missing-object check) across object types.
- Drop-time protection is partial; dependencies table not consulted for most drop paths.
- Alter paths generally do not refresh dependency rows.
- Exception dependencies not recorded; ObjectType introduced but not leveraged for DDL.
- Sequence usage (defaults/nextval) detection is literal-based; needs tighter linkage.

## Actions to Close Gaps
1) Per-object DDL audit: wire dependency validation + drop blocking; refresh deps on alter.  
2) Expand dependency extraction to views/triggers/constraints/defaults/expressions.  
3) Add tests per object type (create/alter/drop + dependency enforcement).  
4) Verify catalog persistence for new ObjectType entries and add indexes where needed.  
5) Tighten type/function operator coverage: build matrix vs evaluator/operator registry; add casts/extracts/tests for types marked partial/unknown.  
