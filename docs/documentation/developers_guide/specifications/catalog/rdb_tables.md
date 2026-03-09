# Specification: RDB$ System Tables (Firebird Compatibility)

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | catalog |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Team |

## Synopsis

This specification defines the RDB$ system tables that provide Firebird compatibility, mapping ScratchBird catalog tables to Firebird's RDB$* naming convention.

## Scope

### In Scope

- RDB$ table definitions
- RDB$ to sb_* table mappings
- Firebird-specific columns
- Virtual table implementation

### Out of Scope

- Firebird SQL parser extensions
- Firebird-specific features not in ScratchBird

## Specification

### RDB$ Table Mappings

| Firebird Table | ScratchBird Table | Description |
|----------------|-------------------|-------------|
| RDB$DATABASE | sb_database | Database info |
| RDB$RELATIONS | sb_tables | Tables and views |
| RDB$RELATION_FIELDS | sb_columns | Table columns |
| RDB$FIELDS | sb_types + sb_domains | Domain definitions |
| RDB$INDEX_SEGMENTS | sb_index_columns | Index columns |
| RDB$INDICES | sb_indexes | Indexes |
| RDB$TRIGGERS | sb_triggers | Triggers |
| RDB$TRIGGER_MESSAGES | (virtual) | Trigger messages |
| RDB$PROCEDURES | sb_procedures | Procedures |
| RDB$PROCEDURE_PARAMETERS | (derived) | Procedure parameters |
| RDB$CHARACTER_SETS | sb_charsets | Character sets |
| RDB$COLLATIONS | sb_collations | Collations |
| RDB$GENERATORS | sb_sequences | Generators/sequences |
| RDB$FUNCTIONS | sb_functions | Functions |
| RDB$FUNCTION_ARGUMENTS | (derived) | Function arguments |
| RDB$USER_PRIVILEGES | sb_permissions | Grants |
| RDB$ROLES | sb_roles | Roles |
| RDB$SECURITY_CLASSES | (virtual) | Security classes |
| RDB$DEPENDENCIES | sb_dependencies | Dependencies |
| RDB$EXCEPTIONS | sb_exceptions | Exceptions |

### RDB$DATABASE

```sql
-- Virtual table mapping to sb_database
CREATE VIRTUAL TABLE RDB$DATABASE (
    RDB$DESCRIPTION BLOB SUB_TYPE TEXT,     -- Database description
    RDB$RELATION_ID INTEGER,                -- Current relation ID
    RDB$SECURITY_CLASS CHAR(31),            -- Security class
    RDB$CHARACTER_SET_NAME CHAR(31)         -- Default charset
);
```

**Mapping:**
```
RDB$DESCRIPTION     -> NULL (ScratchBird doesn't store)
RDB$RELATION_ID     -> MAX(sb_tables.table_id)
RDB$SECURITY_CLASS  -> NULL
RDB$CHARACTER_SET_NAME -> sb_database.default_charset_id -> name
```

### RDB$RELATIONS

```sql
CREATE VIRTUAL TABLE RDB$RELATIONS (
    RDB$VIEW_BLR BLOB,              -- View definition (bytecode)
    RDB$VIEW_SOURCE BLOB SUB_TYPE TEXT,  -- View source
    RDB$DESCRIPTION BLOB SUB_TYPE TEXT,
    RDB$RELATION_ID SMALLINT,
    RDB$SYSTEM_FLAG SMALLINT,       -- 1 = system table
    RDB$DBKEY_LENGTH SMALLINT,
    RDB$FORMAT SMALLINT,
    RDB$FIELD_ID SMALLINT,
    RDB$RELATION_NAME CHAR(31),     -- Table/view name
    RDB$SECURITY_CLASS CHAR(31),
    RDB$EXTERNAL_FILE VARCHAR(255),
    RDB$RUNTIME BLOB,
    RDB$EXTERNAL_DESCRIPTION BLOB SUB_TYPE TEXT,
    RDB$OWNER_NAME CHAR(31),        -- Owner
    RDB$DEFAULT_CLASS CHAR(31),
    RDB$FLAGS SMALLINT
);
```

**Mapping:**
```
RDB$RELATION_NAME   -> sb_tables.table_name
RDB$VIEW_SOURCE     -> sb_views.definition (if view)
RDB$RELATION_ID     -> Derived from table_id
RDB$SYSTEM_FLAG     -> 1 if schema_type = SYSTEM
RDB$OWNER_NAME      -> sb_tables.owner_id -> username
```

### RDB$RELATION_FIELDS

```sql
CREATE VIRTUAL TABLE RDB$RELATION_FIELDS (
    RDB$FIELD_NAME CHAR(31),        -- Column name
    RDB$RELATION_NAME CHAR(31),     -- Table name
    RDB$FIELD_SOURCE CHAR(31),      -- Domain name
    RDB$DESCRIPTION BLOB SUB_TYPE TEXT,
    RDB$DEFAULT_VALUE BLOB,         -- Default value
    RDB$SYSTEM_FLAG SMALLINT,
    RDB$UPDATE_FLAG SMALLINT,
    RDB$FIELD_POSITION SMALLINT,    -- Ordinal position
    RDB$UPDATE_SOURCE BLOB SUB_TYPE TEXT,
    RDB$VIEW_CONTEXT SMALLINT
);
```

**Mapping:**
```
RDB$FIELD_NAME      -> sb_columns.column_name
RDB$RELATION_NAME   -> sb_tables.table_name (join)
RDB$FIELD_SOURCE    -> sb_domains.domain_name (if domain-based)
RDB$DEFAULT_VALUE   -> sb_columns.default_value
RDB$FIELD_POSITION  -> sb_columns.ordinal
```

### RDB$INDICES

```sql
CREATE VIRTUAL TABLE RDB$INDICES (
    RDB$INDEX_NAME CHAR(31),        -- Index name
    RDB$RELATION_NAME CHAR(31),     -- Table name
    RDB$INDEX_ID SMALLINT,
    RDB$UNIQUE_FLAG SMALLINT,       -- 1 = unique
    RDB$DESCRIPTION BLOB SUB_TYPE TEXT,
    RDB$SEGMENT_COUNT SMALLINT,     -- Number of columns
    RDB$INDEX_INACTIVE SMALLINT,    -- 1 = inactive
    RDB$INDEX_TYPE SMALLINT,        -- 1 = descending
    RDB$FOREIGN_KEY CHAR(31),       -- FK constraint name
    RDB$SYSTEM_FLAG SMALLINT,
    RDB$STATISTICS DOUBLE PRECISION -- Selectivity
);
```

**Mapping:**
```
RDB$INDEX_NAME      -> sb_indexes.index_name
RDB$RELATION_NAME   -> sb_tables.table_name (join)
RDB$UNIQUE_FLAG     -> sb_indexes.is_unique
RDB$SEGMENT_COUNT   -> sb_indexes.column_count
RDB$INDEX_INACTIVE  -> sb_indexes.state != ACTIVE
```

### RDB$GENERATORS

```sql
CREATE VIRTUAL TABLE RDB$GENERATORS (
    RDB$GENERATOR_NAME CHAR(31),    -- Generator name
    RDB$GENERATOR_ID SMALLINT,
    RDB$SYSTEM_FLAG SMALLINT,
    RDB$DESCRIPTION BLOB SUB_TYPE TEXT
);
```

**Mapping:**
```
RDB$GENERATOR_NAME  -> sb_sequences.name
RDB$GENERATOR_ID    -> Derived from sequence_id
```

## Virtual Table Implementation

```cpp
class RDBSystemTableHandler {
public:
    // Map RDB$ query to sb_* query
    Status queryRDBRelations(const QueryCriteria& criteria,
                             ResultSet& results);
    
    Status queryRDBRelationFields(const QueryCriteria& criteria,
                                  ResultSet& results);
    
    Status queryRDBIndices(const QueryCriteria& criteria,
                           ResultSet& results);
    
    // ... other tables
};
```

## Firebird Compatibility Notes

1. **Name Length:** Firebird uses 31 chars, ScratchBird uses 128
   - Truncate or error on longer names
   
2. **Character Sets:** Firebird charset names may differ
   - Map UTF8 -> UTF8, ISO8859_1 -> LATIN1, etc.
   
3. **System Flags:** Firebird uses system_flag = 1
   - Map from schema_type = SYSTEM
   
4. **NULL Handling:** Some columns always NULL in compatibility layer

## Related Specifications

- [catalog_table_layouts.md](./catalog_table_layouts.md) - Underlying tables
- [information_schema.md](./information_schema.md) - SQL standard views

## Appendix

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
