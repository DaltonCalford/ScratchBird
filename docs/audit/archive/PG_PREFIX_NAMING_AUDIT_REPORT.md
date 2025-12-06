# ScratchBird System Table Naming Audit Report
## Incorrect `pg_` Prefix Usage (Should Be `sb_`)

**Date:** 2025-12-06
**Issue:** AI-generated documentation incorrectly used PostgreSQL `pg_` prefix instead of ScratchBird `sb_` prefix for system catalog tables.

---

## Executive Summary

The ScratchBird codebase correctly uses the `sb_` prefix for system catalog tables in the actual implementation. However, documentation files (particularly `SYSTEM_TABLES.md`) incorrectly use the PostgreSQL `pg_` prefix. This creates confusion and inconsistency.

**Key Finding:** The code is CORRECT. The documentation is WRONG.

---

## Correct Naming Convention

| Incorrect (pg_) | Correct (sb_) | Purpose |
|-----------------|---------------|---------|
| pg_schema | sb_schema | Schema definitions |
| pg_tables | sb_tables | Table metadata |
| pg_columns | sb_columns | Column definitions |
| pg_indexes | sb_indexes | Index metadata |
| pg_constraints | sb_constraints | Constraint definitions |
| pg_sequences | sb_sequences | Sequence objects |
| pg_views | sb_views | View definitions |
| pg_triggers | sb_triggers | Trigger definitions |
| pg_permissions | sb_permissions | Object permissions |
| pg_statistics | sb_statistics | Column statistics |
| pg_charset | sb_charset | Character sets |
| pg_collation | sb_collation | Collation definitions |
| pg_timezone | sb_timezone | Timezone data |
| pg_users | sb_users | User accounts |
| pg_roles | sb_roles | Role definitions |
| pg_groups | sb_groups | Group definitions |
| pg_role_members | sb_role_members | Role memberships |
| pg_group_members | sb_group_members | Group memberships |
| pg_procedures | sb_procedures | Stored procedures |
| pg_procedure_params | sb_procedure_params | Procedure parameters |
| pg_domains | sb_domains | User-defined domains |
| pg_dependencies | sb_dependencies | Object dependencies |
| pg_comments | sb_comments | Object comments |
| pg_foreign_keys | sb_foreign_keys | Foreign key details |
| pg_policies | sb_policies | RLS policies |
| pg_column_permissions | sb_column_permissions | Column permissions |
| pg_packages | sb_packages | Firebird packages |
| pg_udr | sb_udr | User-defined resources |
| pg_emulation_types | sb_emulation_types | Emulation types |
| pg_emulation_servers | sb_emulation_servers | Emulation servers |
| pg_tablespaces | sb_tablespaces | Tablespace definitions |
| pg_extensions | sb_extensions | Extensions |
| pg_synonyms | sb_synonyms | Schema synonyms |
| pg_foreign_servers | sb_foreign_servers | FDW servers |
| pg_foreign_tables | sb_foreign_tables | FDW tables |
| pg_user_mappings | sb_user_mappings | FDW user mappings |
| pg_group_mappings | sb_group_mappings | External group mappings |
| pg_server_registry | sb_server_registry | Distributed MVCC |
| pg_migration_history | sb_migration_history | Migration history |

---

## Files Requiring Correction

### PRIMARY: Documentation Files with Incorrect Naming

#### 1. `/docs/planning/current_parser/SYSTEM_TABLES.md` (CRITICAL)
**Status:** Entire file uses incorrect `pg_` prefix
**Lines affected:** Approximately 100+ occurrences

Incorrect table names throughout:
- `pg_schema` → `sb_schema`
- `pg_tables` → `sb_tables`
- `pg_columns` → `sb_columns`
- `pg_indexes` → `sb_indexes`
- `pg_constraints` → `sb_constraints`
- `pg_sequences` → `sb_sequences`
- `pg_views` → `sb_views`
- `pg_triggers` → `sb_triggers`
- `pg_permissions` → `sb_permissions`
- `pg_statistics` → `sb_statistics`
- `pg_charset` → `sb_charset`
- `pg_collation` → `sb_collation`
- `pg_timezone` → `sb_timezone`
- `pg_users` → `sb_users`
- `pg_roles` → `sb_roles`
- `pg_groups` → `sb_groups`
- `pg_role_members` → `sb_role_members`
- `pg_group_members` → `sb_group_members`
- `pg_procedures` → `sb_procedures`
- `pg_procedure_params` → `sb_procedure_params`
- `pg_domains` → `sb_domains`
- `pg_dependencies` → `sb_dependencies`
- `pg_comments` → `sb_comments`
- `pg_foreign_keys` → `sb_foreign_keys`
- `pg_policies` → `sb_policies`
- And all other `pg_*` tables

#### 2. `/docs/planning/SB_ISQL_SERVER_VS_CLIENT_COMMANDS.md`
**Status:** Uses incorrect `pg_` prefix for table references
**Lines affected:** ~50 occurrences
**Action:** Replace all `pg_*` table references with `sb_*`

### SECONDARY: Archive Documentation (Lower Priority)

These are in archive folders and have less urgency but should be noted:

1. `/docs/planning/archive/CATALOG_CLEANUP_PHASE_D_VIRTUAL.md`
2. `/docs/planning/archive/SCHEMA_NAVIGATION_AND_SEARCH_PATH.md`
3. `/docs/planning/archive/IMPROVEMENTS_P0_CRITICAL_PLAN.md`
4. `/docs/planning/archive/CRUD_IMPLEMENTATION_PLAN.md`
5. `/docs/Alpha_Phase_1_Archive/planning_archive/SECURITY_SYSTEM_IMPLEMENTATION_PLAN.md`
6. Multiple files in `/docs/Alpha_Phase_1_Archive/`

---

## Code That Is CORRECTLY Named (No Changes Needed)

### Catalog Index Header
`include/scratchbird/catalog/catalog_index.h:48-59`
```cpp
TABLES_BY_NAME,          // sb_tables(table_name) -> table_id
TABLES_BY_SCHEMA,        // sb_tables(schema_id) -> table_ids
COLUMNS_BY_TABLE,        // sb_columns(table_id) -> column_ids
COLUMNS_BY_NAME,         // sb_columns(table_id, column_name) -> column_id
INDEXES_BY_NAME,         // sb_indexes(index_name) -> index_id
INDEXES_BY_TABLE,        // sb_indexes(table_id) -> index_ids
SEQUENCES_BY_NAME,       // sb_sequences(sequence_name) -> sequence_id
VIEWS_BY_NAME,           // sb_views(view_name) -> view_id
TRIGGERS_BY_TABLE,       // sb_triggers(table_id) -> trigger_ids
```

### Catalog Manager Header
`include/scratchbird/core/catalog_manager.h`
```cpp
// P1-9: Unified constraint information for sb_constraints table
static constexpr uint32_t TABLESPACES_TABLE_PAGE = 8;       // sb_tablespace
static constexpr uint32_t TABLESPACE_FILES_TABLE_PAGE = 9;  // sb_tablespace_files
```

### Catalog Manager Implementation
`src/core/catalog_manager.cpp`
```cpp
// Pin sb_tablespace page
// Compacting sb_tablespace catalog page
// Compacting sb_schema catalog page
// Compacting sb_table catalog page
// Compacting sb_column catalog page
// Compacting sb_index catalog page
```

---

## Legitimate `pg_` Usage (PostgreSQL Emulation - DO NOT CHANGE)

The following `pg_` prefixes are **CORRECT** because they are part of PostgreSQL wire protocol emulation:

### `/include/scratchbird/catalog/pg_catalog.h`
This file implements PostgreSQL-compatible `pg_catalog` views for tool compatibility. These should remain as `pg_*`:
- `pg_namespace` - Emulated PostgreSQL schema view
- `pg_class` - Emulated PostgreSQL tables/indexes view
- `pg_attribute` - Emulated PostgreSQL columns view
- `pg_type` - Emulated PostgreSQL types view
- `pg_index` - Emulated PostgreSQL index view
- `pg_constraint` - Emulated PostgreSQL constraints view
- `pg_proc` - Emulated PostgreSQL procedures view
- `pg_trigger` - Emulated PostgreSQL triggers view
- `pg_user` - Emulated PostgreSQL users view
- `pg_roles` - Emulated PostgreSQL roles view
- `pg_database` - Emulated PostgreSQL database view
- `pg_tablespace` - Emulated PostgreSQL tablespace view

### `/src/catalog/virtual_catalog.cpp`
```cpp
// Register pg_catalog (PostgreSQL wire protocol)
```
This is correct - it's registering the PostgreSQL emulation catalog.

### `/include/scratchbird/catalog/emulation_view_generator.h`
```cpp
// PostgreSQL pg_catalog view definitions (on-demand emulation)
"pg_tables",
"pg_views",
```
This is correct - these are PostgreSQL emulation views, not ScratchBird native tables.

---

## Recommended Actions

### Immediate (High Priority)

1. **Update `/docs/planning/current_parser/SYSTEM_TABLES.md`**
   - Replace all `pg_` prefixes with `sb_` prefixes
   - Update the "Complete Table List" section
   - ~100+ replacements needed

2. **Update `/docs/planning/SB_ISQL_SERVER_VS_CLIENT_COMMANDS.md`**
   - Replace all `pg_*` table references with `sb_*`
   - ~50 replacements needed

### Deferred (Lower Priority)

3. **Archive files** - Can be updated when accessed, but are historical

---

## Verification Commands

After corrections, verify no incorrect usage remains:

```bash
# Should return only pg_catalog emulation files
grep -r "pg_schema\|pg_tables\|pg_columns\|pg_indexes" \
  --include="*.md" docs/planning/ | \
  grep -v "pg_catalog" | grep -v "archive"

# Verify sb_ prefix is used in code
grep -r "sb_tables\|sb_columns\|sb_indexes\|sb_schema" \
  --include="*.cpp" --include="*.h" src/ include/
```

---

## Summary

| Category | Status |
|----------|--------|
| Source Code (*.cpp, *.h) | ✅ CORRECT - Uses `sb_` prefix |
| Catalog Index | ✅ CORRECT - Uses `sb_` prefix |
| Catalog Manager | ✅ CORRECT - Uses `sb_` prefix |
| PostgreSQL Emulation | ✅ CORRECT - Uses `pg_` for emulation |
| SYSTEM_TABLES.md | ❌ INCORRECT - Uses `pg_` prefix |
| SB_ISQL_SERVER_VS_CLIENT_COMMANDS.md | ❌ INCORRECT - Uses `pg_` prefix |
| Archive Documentation | ⚠️ MIXED - Lower priority |

**Total files needing correction:** 2 active + ~60 archive files
**Total replacements needed:** ~150+ in active files
