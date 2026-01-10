# Domain Support Audit (Gaps + Required Work)

## Scope
Audit of domain support vs `docs/specifications/DDL_DOMAINS.md` and `docs/specifications/03_TYPES_AND_DOMAINS.md`, with focus on verification, casting, array/sub-domain support, and named elements.

## Executive Summary
Domain support is split between `CatalogManager` (schema-scoped, minimal metadata) and `DomainManager` (in-memory, partially implemented advanced types). There is no end-to-end SQL path (parser -> SBLR -> executor -> catalog) for domains, no runtime enforcement of domain constraints during inserts/updates, and no persistence of domain usage in column records. Advanced domain types (RECORD/ENUM/SET/VARIANT), domain inheritance, casting, array/sub-domain semantics, and named element access are largely stubs or not integrated.

## Findings (By Area)

### 1) Catalog + Persistence (Mismatch and Missing Fields)
- **Two incompatible domain catalogs**:
  - `CatalogManager` domain table (`sb_domains`) stores only `base_type`, `check_expr`, `not_null` and is **schema-scoped**. See `include/scratchbird/core/catalog_manager.h` (DomainInfo) and `src/core/catalog_manager.cpp` (DomainRecord).
  - `DomainManager` uses its own `DomainRecord` on page 10 with different fields, stored only in memory (`writeDomainRecord` is a no-op). See `src/core/domain_manager.cpp`.
- **No persistence of domain usage**:
  - `ColumnInfo` includes `domain_id`, but `ColumnRecord` (on disk) does not. Domain usage is lost on reload, and dependency checks become inaccurate. See `include/scratchbird/core/catalog_manager.h` (ColumnInfo) vs `src/core/catalog_manager.cpp` (ColumnRecord).
- **Missing catalog columns required by spec**:
  - No dialect tag, compatibility name, domain state (ACTIVE/SHADOW/etc), definition hash, cast map, or origin metadata. Required by `docs/specifications/DDL_DOMAINS.md` and cluster domain plan.
- **`sb_isql` assumes schema-scoped domains**:
  - `sb_isql` queries `sb_domains` with `schema_id IS NOT NULL`. This conflicts with required global domains. See `src/cli/sb_isql.cpp`.

### 2) Parser + SBLR + Executor (No end-to-end DDL)
- **Scratchbird parser v2** does not implement `CREATE DOMAIN` or `ALTER/DROP DOMAIN`. Only `SHOW DOMAIN` is parsed. See `src/parser/parser_v2.cpp`.
- **Firebird parser** explicitly errors on `CREATE DOMAIN`. See `src/parser/firebird/firebird_parser.cpp`.
- **PostgreSQL parser** parses `CREATE DOMAIN` but does not emit usable payload:
  - `EXT_CREATE_DOMAIN` emitted, but base type/constraints are not serialized in a usable structure.
  - `typeToOpcode()` falls back to `TYPE_VARCHAR` for `DOMAIN`, so domain types are downgraded. See `src/parser/postgresql/pg_parser.cpp` and `src/parser/postgresql/pg_parser_ddl.cpp`.
- **Executor has no `EXT_CREATE_DOMAIN` handler**. `SHOW DOMAIN` is stubbed. See `src/sblr/executor.cpp`.
- **SBLR opcodes** only include `EXT_CREATE_DOMAIN` and `EXT_SHOW_DOMAIN`; no alter/drop/resolve/rebind opcodes. See `include/scratchbird/sblr/opcodes.h`.

### 3) Type System Integration (Domain is Unknown)
- `ResolvedType` has **no domain ID** or domain metadata. Domains are resolved in `SemanticAnalyzerV2` but assigned `DataType::UNKNOWN`, losing type info. See `src/sblr/semantic_analyzer_v2.cpp` and `include/scratchbird/sblr/resolved_ast_v2.h`.
- `CREATE TABLE` executor reads only primitive type opcodes and cannot store a domain ID. See `src/sblr/executor.cpp`.

### 4) Verification (Constraints, Defaults, Enforcement)
- **Domain constraints are not enforced during DML**.
  - `DomainManager::validateValue()` is never called by executor or storage code.
- **Constraint evaluation is minimal and string-parsed**:
  - Only simple `VALUE <op> literal` and limited `LIKE` patterns are supported; full expression evaluation is missing. See `src/core/domain_manager.cpp`.
- **DEFAULT/UNIQUE constraints** exist in `DomainConstraint` enum but are not enforced. `DEFAULT` is not applied to columns or variables at runtime.
- **Inheritance** only merges CHECK constraints; no inheritance for defaults, NOT NULL, or advanced settings.

### 5) Casting (Domain <-> Base / Domain <-> Domain)
- No cast map or domain-specific casting rules exist. Domain-to-base and domain-to-domain casts are not modeled in `TypeInfo`, `ResolvedType`, or conversion utilities.
- `TypeSystem` conversions are not domain-aware. `DomainManager` does not expose casting rules.

### 6) Arrays + Sub-Domain Support
- Parser can read `[]` array syntax, but resolved types do not preserve domain IDs for array elements.
- Domains based on `ARRAY` or domains that reference other domains (sub-domains) are not supported by catalog or executor.
- `RecordField` supports `domain_id`, but `createRecordDomain()` does not validate domain references or store them in any persistent form.

### 7) Named Elements (Record/Enum/Set)
- **RECORD**: field access (`extractField`) is `NOT_IMPLEMENTED`; no SQL support for `record.field` or `EXTRACT(field FROM record)`.
- **ENUM**: ordering ops and `SET NEXT VALUE` are not integrated into SQL; only in-memory operations exist.
- **SET**: operations (`contains`, `overlap`, `union`, etc.) are `NOT_IMPLEMENTED` pending `TypedValue::VECTOR` element access.
- **VARIANT**: runtime type extraction and casting are `NOT_IMPLEMENTED`.

### 8) Security/Integrity/Validation/Quality Options
- Domain security/integrity/validation/quality structures exist in `DomainManager`, but:
  - Not persisted.
  - Not integrated with permission system or audit logging.
  - Masking is placeholder-only.

### 9) Compatibility Catalogs + Information Schema
- Firebird catalog `RDB$FIELDS` returns empty results for domains. See `src/catalog/firebird_catalog.cpp`.
- `information_schema` is a stub and does not expose domains. See `include/scratchbird/catalog/information_schema.h`.

### 10) Tests
- Domain tests only verify that stubs exist and return expected error codes; they do **not** assert SQL-level behavior, persistence, or runtime enforcement.
- No tests for:
  - CREATE/ALTER/DROP DOMAIN via SQL
  - Domain-based columns (including check/default enforcement)
  - Domain inheritance or cast rules
  - Array-of-domain or record field domain usage

## Required Work (High-Level)
1) **Unify domain catalog**: adopt global domains with dialect/compat metadata and cluster conflict tracking. Eliminate duplicated DomainManager catalog or make it a runtime wrapper over CatalogManager data.
2) **Persist domain usage**: add `domain_id` to `ColumnRecord`, ensure dependency tracking survives reload.
3) **Implement full DDL path**: parser -> SBLR -> executor -> catalog for CREATE/ALTER/DROP DOMAIN (all dialects required for alpha compatibility).
4) **Type system integration**: extend `ResolvedType` to include `domain_id` and element domain metadata; propagate into executor and storage.
5) **Runtime enforcement**: ensure inserts/updates validate domain constraints and apply defaults, including inherited constraints.
6) **Advanced types**: implement RECORD/ENUM/SET/VARIANT runtime behavior and SQL syntax (ROW/EXTRACT, SET operators, enum ordering).
7) **Casting rules**: define and enforce domain -> base and domain -> domain cast compatibility, with storage hash checks.
8) **Arrays and sub-domains**: support arrays of domains and domains based on arrays; resolve element types and enforce constraints.
9) **Compatibility catalogs**: implement domain exposure for Firebird catalogs and information_schema.
10) **Test coverage**: add SQL and runtime tests for domain DDL, constraints, persistence, and advanced types.

## Decisions Confirmed
1) **ALTER DOMAIN validation**: full validation is required. If any dependent rows violate the new definition, ALTER fails and must report table_id and primary-key values for each violating row. In a cluster, local validation success marks the change as pending until all members confirm; final activation waits for cluster reports.
2) **DROP DOMAIN**: RESTRICT-only. Domain cannot be dropped while dependencies exist; users must resolve dependencies first via dependency tables.
3) **Array domains**: constraints apply on assignment and may be element-level or whole-array depending on definition. For simple arrays, follow Firebird dialect behavior.
4) **Nested domain/record depth**: maximum depth is 50 levels; deeper nesting is rejected to prevent CPU abuse.
