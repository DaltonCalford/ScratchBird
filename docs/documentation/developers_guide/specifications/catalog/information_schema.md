# Specification: INFORMATION_SCHEMA (SQL Standard)

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

This specification defines the INFORMATION_SCHEMA views as defined by the SQL standard, providing portable database metadata access.

## Scope

### In Scope

- SQL standard INFORMATION_SCHEMA views
- View definitions and column mappings
- Data type mappings to standard types

### Out of Scope

- SQL-92 only views (superseded by later standards)
- Implementation-specific extensions

## Specification

### INFORMATION_SCHEMA Schema

```sql
-- INFORMATION_SCHEMA is automatically created
-- All views are read-only

SELECT * FROM INFORMATION_SCHEMA.TABLES;
SELECT * FROM INFORMATION_SCHEMA.COLUMNS;
```

### Core Views

#### TABLES

```sql
CREATE VIEW INFORMATION_SCHEMA.TABLES AS
SELECT 
    db.database_name AS TABLE_CATALOG,
    s.schema_name AS TABLE_SCHEMA,
    t.table_name AS TABLE_NAME,
    CASE t.table_type
        WHEN 'HEAP' THEN 'BASE TABLE'
        WHEN 'VIEW' THEN 'VIEW'
        WHEN 'TEMPORARY' THEN 'LOCAL TEMPORARY'
    END AS TABLE_TYPE,
    NULL AS SELF_REFERENCING_COLUMN_NAME,
    NULL AS REFERENCE_GENERATION
FROM sb_database db
CROSS JOIN sb_schema s
JOIN sb_tables t ON t.schema_id = s.schema_id
WHERE t.is_valid = 1;
```

#### COLUMNS

```sql
CREATE VIEW INFORMATION_SCHEMA.COLUMNS AS
SELECT 
    db.database_name AS TABLE_CATALOG,
    s.schema_name AS TABLE_SCHEMA,
    t.table_name AS TABLE_NAME,
    c.column_name AS COLUMN_NAME,
    c.ordinal AS ORDINAL_POSITION,
    CASE 
        WHEN c.default_value IS NOT NULL THEN c.default_value
        ELSE NULL
    END AS COLUMN_DEFAULT,
    CASE WHEN c.nullable THEN 'YES' ELSE 'NO' END AS IS_NULLABLE,
    dt.type_name AS DATA_TYPE,
    NULL AS CHARACTER_MAXIMUM_LENGTH,
    NULL AS CHARACTER_OCTET_LENGTH,
    CASE WHEN dt.type_name IN ('DECIMAL', 'NUMERIC') 
         THEN c.type_precision END AS NUMERIC_PRECISION,
    CASE WHEN dt.type_name IN ('DECIMAL', 'NUMERIC') 
         THEN c.type_scale END AS NUMERIC_SCALE,
    NULL AS DATETIME_PRECISION,
    CASE WHEN c.charset_id IS NOT NULL THEN cs.name END AS CHARACTER_SET_NAME,
    CASE WHEN c.collation_id IS NOT NULL THEN col.name END AS COLLATION_NAME,
    CASE WHEN c.domain_id IS NOT NULL THEN d.domain_name 
         ELSE dt.type_name END AS DOMAIN_NAME,
    NULL AS DOMAIN_CATALOG,
    NULL AS DOMAIN_SCHEMA,
    NULL AS UDT_CATALOG,
    NULL AS UDT_SCHEMA,
    NULL AS UDT_NAME,
    NULL AS SCOPE_CATALOG,
    NULL AS SCOPE_SCHEMA,
    NULL AS SCOPE_NAME,
    NULL AS MAXIMUM_CARDINALITY,
    1 AS DTD_IDENTIFIER,
    CASE WHEN c.is_generated THEN 'YES' ELSE 'NO' END AS IS_GENERATED,
    c.generation_expression AS GENERATION_EXPRESSION,
    CASE WHEN c.is_identity THEN 'YES' ELSE 'NO' END AS IS_IDENTITY,
    CASE WHEN c.identity_always THEN 'ALWAYS' ELSE 'BY DEFAULT' END AS IDENTITY_GENERATION,
    NULL AS IDENTITY_START,
    NULL AS IDENTITY_INCREMENT,
    NULL AS IDENTITY_MAXIMUM,
    NULL AS IDENTITY_MINIMUM,
    NULL AS IDENTITY_CYCLE,
    CASE WHEN c.is_generated THEN 'ALWAYS' ELSE 'NEVER' END AS IS_GENERATED_ALWAYS,
    c.generation_expression AS GENERATION_EXPRESSION_ORIGINAL
FROM sb_database db
CROSS JOIN sb_schema s
JOIN sb_tables t ON t.schema_id = s.schema_id
JOIN sb_columns c ON c.table_id = t.table_id
LEFT JOIN sb_types dt ON dt.type_id = c.data_type
LEFT JOIN sb_charsets cs ON cs.charset_id = c.charset_id
LEFT JOIN sb_collations col ON col.collation_id = c.collation_id
LEFT JOIN sb_domains d ON d.domain_id = c.domain_id
WHERE t.is_valid = 1 AND c.is_valid = 1;
```

#### VIEWS

```sql
CREATE VIEW INFORMATION_SCHEMA.VIEWS AS
SELECT 
    db.database_name AS TABLE_CATALOG,
    s.schema_name AS TABLE_SCHEMA,
    v.name AS TABLE_NAME,
    v.definition AS VIEW_DEFINITION,
    CASE WHEN v.check_option THEN 'CASCADED' ELSE 'NONE' END AS CHECK_OPTION,
    CASE WHEN v.check_option THEN 'YES' ELSE 'NO' END AS IS_UPDATABLE,
    NULL AS INSERTABLE_INTO,
    NULL AS IS_TRIGGER_UPDATABLE,
    NULL AS IS_TRIGGER_DELETABLE,
    NULL AS IS_TRIGGER_INSERTABLE_INTO
FROM sb_database db
CROSS JOIN sb_schema s
JOIN sb_views v ON v.schema_id = s.schema_id
WHERE v.is_valid = 1 AND v.materialized = 0;
```

#### TABLE_CONSTRAINTS

```sql
CREATE VIEW INFORMATION_SCHEMA.TABLE_CONSTRAINTS AS
SELECT 
    db.database_name AS CONSTRAINT_CATALOG,
    s.schema_name AS CONSTRAINT_SCHEMA,
    c.constraint_name AS CONSTRAINT_NAME,
    db.database_name AS TABLE_CATALOG,
    s.schema_name AS TABLE_SCHEMA,
    t.table_name AS TABLE_NAME,
    CASE c.constraint_type
        WHEN 'PRIMARY_KEY' THEN 'PRIMARY KEY'
        WHEN 'UNIQUE' THEN 'UNIQUE'
        WHEN 'FOREIGN_KEY' THEN 'FOREIGN KEY'
        WHEN 'CHECK' THEN 'CHECK'
    END AS CONSTRAINT_TYPE,
    CASE WHEN c.is_deferrable THEN 'YES' ELSE 'NO' END AS IS_DEFERRABLE,
    CASE WHEN c.initially_deferred THEN 'YES' ELSE 'NO' END AS INITIALLY_DEFERRED,
    'NO' AS ENFORCED
FROM sb_database db
CROSS JOIN sb_schema s
JOIN sb_tables t ON t.schema_id = s.schema_id
JOIN sb_constraints c ON c.table_id = t.table_id
WHERE c.is_valid = 1;
```

#### KEY_COLUMN_USAGE

```sql
CREATE VIEW INFORMATION_SCHEMA.KEY_COLUMN_USAGE AS
SELECT 
    db.database_name AS CONSTRAINT_CATALOG,
    s.schema_name AS CONSTRAINT_SCHEMA,
    c.constraint_name AS CONSTRAINT_NAME,
    db.database_name AS TABLE_CATALOG,
    s.schema_name AS TABLE_SCHEMA,
    t.table_name AS TABLE_NAME,
    col.column_name AS COLUMN_NAME,
    col.ordinal AS ORDINAL_POSITION,
    NULL AS POSITION_IN_UNIQUE_CONSTRAINT
FROM sb_database db
CROSS JOIN sb_schema s
JOIN sb_tables t ON t.schema_id = s.schema_id
JOIN sb_constraints c ON c.table_id = t.table_id
JOIN sb_columns col ON col.table_id = t.table_id 
    AND col.column_id IN (SELECT column_id FROM sb_constraint_columns 
                          WHERE constraint_id = c.constraint_id)
WHERE c.constraint_type IN ('PRIMARY_KEY', 'UNIQUE', 'FOREIGN_KEY')
  AND c.is_valid = 1;
```

#### REFERENTIAL_CONSTRAINTS

```sql
CREATE VIEW INFORMATION_SCHEMA.REFERENTIAL_CONSTRAINTS AS
SELECT 
    db.database_name AS CONSTRAINT_CATALOG,
    s.schema_name AS CONSTRAINT_SCHEMA,
    c.constraint_name AS CONSTRAINT_NAME,
    db.database_name AS UNIQUE_CONSTRAINT_CATALOG,
    s2.schema_name AS UNIQUE_CONSTRAINT_SCHEMA,
    c2.constraint_name AS UNIQUE_CONSTRAINT_NAME,
    CASE c.match_type
        WHEN 'SIMPLE' THEN 'NONE'
        WHEN 'FULL' THEN 'FULL'
        WHEN 'PARTIAL' THEN 'PARTIAL'
    END AS MATCH_OPTION,
    CASE c.on_update
        WHEN 'CASCADE' THEN 'CASCADE'
        WHEN 'SET_NULL' THEN 'SET NULL'
        WHEN 'SET_DEFAULT' THEN 'SET DEFAULT'
        WHEN 'RESTRICT' THEN 'RESTRICT'
        WHEN 'NO_ACTION' THEN 'NO ACTION'
    END AS UPDATE_RULE,
    CASE c.on_delete
        WHEN 'CASCADE' THEN 'CASCADE'
        WHEN 'SET_NULL' THEN 'SET NULL'
        WHEN 'SET_DEFAULT' THEN 'SET DEFAULT'
        WHEN 'RESTRICT' THEN 'RESTRICT'
        WHEN 'NO_ACTION' THEN 'NO ACTION'
    END AS DELETE_RULE
FROM sb_database db
CROSS JOIN sb_schema s
JOIN sb_constraints c ON c.table_id IN (SELECT table_id FROM sb_tables 
                                        WHERE schema_id = s.schema_id)
JOIN sb_constraints c2 ON c2.table_id = c.referenced_table_id 
    AND c2.constraint_type = 'PRIMARY_KEY'
JOIN sb_schema s2 ON s2.schema_id = (SELECT schema_id FROM sb_tables 
                                     WHERE table_id = c2.table_id)
WHERE c.constraint_type = 'FOREIGN_KEY' AND c.is_valid = 1;
```

#### CHECK_CONSTRAINTS

```sql
CREATE VIEW INFORMATION_SCHEMA.CHECK_CONSTRAINTS AS
SELECT 
    db.database_name AS CONSTRAINT_CATALOG,
    s.schema_name AS CONSTRAINT_SCHEMA,
    c.constraint_name AS CONSTRAINT_NAME,
    c.check_expression AS CHECK_CLAUSE,
    'NO' AS IS_DEFERRABLE,
    'NO' AS INITIALLY_DEFERRED
FROM sb_database db
CROSS JOIN sb_schema s
JOIN sb_tables t ON t.schema_id = s.schema_id
JOIN sb_constraints c ON c.table_id = t.table_id
WHERE c.constraint_type = 'CHECK' AND c.is_valid = 1;
```

#### SCHEMATA

```sql
CREATE VIEW INFORMATION_SCHEMA.SCHEMATA AS
SELECT 
    db.database_name AS CATALOG_NAME,
    s.schema_name AS SCHEMA_NAME,
    u.username AS SCHEMA_OWNER,
    NULL AS DEFAULT_CHARACTER_SET_CATALOG,
    NULL AS DEFAULT_CHARACTER_SET_SCHEMA,
    cs.name AS DEFAULT_CHARACTER_SET_NAME,
    NULL AS SQL_PATH
FROM sb_database db
CROSS JOIN sb_schema s
JOIN sb_users u ON u.user_id = s.owner_id
LEFT JOIN sb_charsets cs ON cs.charset_id = s.default_charset
WHERE s.is_valid = 1;
```

#### CHARACTER_SETS

```sql
CREATE VIEW INFORMATION_SCHEMA.CHARACTER_SETS AS
SELECT 
    NULL AS CHARACTER_SET_CATALOG,
    NULL AS CHARACTER_SET_SCHEMA,
    cs.name AS CHARACTER_SET_NAME,
    cs.description AS FORM_OF_USE,
    cs.charset_id AS NUMBER_OF_CHARACTERS,
    cs.name AS DEFAULT_COLLATE_CATALOG,
    NULL AS DEFAULT_COLLATE_SCHEMA,
    col.name AS DEFAULT_COLLATE_NAME
FROM sb_charsets cs
LEFT JOIN sb_collations col ON col.collation_id = cs.default_collation_id
WHERE cs.is_valid = 1;
```

#### COLLATIONS

```sql
CREATE VIEW INFORMATION_SCHEMA.COLLATIONS AS
SELECT 
    NULL AS COLLATION_CATALOG,
    NULL AS COLLATION_SCHEMA,
    col.name AS COLLATION_NAME,
    cs.name AS CHARACTER_SET_NAME,
    1 AS PAD_ATTRIBUTE
FROM sb_collations col
JOIN sb_charsets cs ON cs.charset_id = col.charset_id
WHERE col.is_valid = 1;
```

#### ROUTINES

```sql
CREATE VIEW INFORMATION_SCHEMA.ROUTINES AS
SELECT 
    db.database_name AS ROUTINE_CATALOG,
    s.schema_name AS ROUTINE_SCHEMA,
    p.name AS ROUTINE_NAME,
    'PROCEDURE' AS ROUTINE_TYPE,
    NULL AS MODULE_CATALOG,
    NULL AS MODULE_SCHEMA,
    NULL AS MODULE_NAME,
    NULL AS UDT_CATALOG,
    NULL AS UDT_SCHEMA,
    NULL AS UDT_NAME,
    NULL AS DATA_TYPE,
    NULL AS CHARACTER_MAXIMUM_LENGTH,
    NULL AS CHARACTER_OCTET_LENGTH,
    NULL AS CHARACTER_SET_CATALOG,
    NULL AS CHARACTER_SET_SCHEMA,
    NULL AS CHARACTER_SET_NAME,
    NULL AS COLLATION_CATALOG,
    NULL AS COLLATION_SCHEMA,
    NULL AS COLLATION_NAME,
    NULL AS NUMERIC_PRECISION,
    NULL AS NUMERIC_SCALE,
    NULL AS DATETIME_PRECISION,
    NULL AS INTERVAL_TYPE,
    NULL AS INTERVAL_PRECISION,
    NULL AS TYPE_UDT_CATALOG,
    NULL AS TYPE_UDT_SCHEMA,
    NULL AS TYPE_UDT_NAME,
    NULL AS SCOPE_CATALOG,
    NULL AS SCOPE_SCHEMA,
    NULL AS SCOPE_NAME,
    NULL AS MAXIMUM_CARDINALITY,
    1 AS DTD_IDENTIFIER,
    CASE WHEN p.sql_security = 'INVOKER' THEN 'INVOKER' 
         ELSE 'DEFINER' END AS SECURITY_TYPE,
    p.source_text AS ROUTINE_DEFINITION,
    NULL AS EXTERNAL_NAME,
    NULL AS EXTERNAL_LANGUAGE,
    NULL AS PARAMETER_STYLE,
    NULL AS IS_DETERMINISTIC,
    NULL AS SQL_DATA_ACCESS,
    NULL AS IS_NULL_CALL,
    NULL AS SQL_PATH,
    'NO' AS SCHEMA_LEVEL_ROUTINE,
    0 AS MAX_DYNAMIC_RESULT_SETS,
    'NO' AS IS_USER_DEFINED_CAST,
    'NO' AS IS_IMPLICITLY_INVOCABLE
FROM sb_database db
CROSS JOIN sb_schema s
JOIN sb_procedures p ON p.schema_id = s.schema_id
WHERE p.is_valid = 1

UNION ALL

SELECT 
    db.database_name AS ROUTINE_CATALOG,
    s.schema_name AS ROUTINE_SCHEMA,
    f.name AS ROUTINE_NAME,
    'FUNCTION' AS ROUTINE_TYPE,
    NULL AS MODULE_CATALOG,
    NULL AS MODULE_SCHEMA,
    NULL AS MODULE_NAME,
    NULL AS UDT_CATALOG,
    NULL AS UDT_SCHEMA,
    NULL AS UDT_NAME,
    dt.type_name AS DATA_TYPE,
    NULL AS CHARACTER_MAXIMUM_LENGTH,
    NULL AS CHARACTER_OCTET_LENGTH,
    NULL AS CHARACTER_SET_CATALOG,
    NULL AS CHARACTER_SET_SCHEMA,
    NULL AS CHARACTER_SET_NAME,
    NULL AS COLLATION_CATALOG,
    NULL AS COLLATION_SCHEMA,
    NULL AS COLLATION_NAME,
    CASE WHEN dt.type_name IN ('DECIMAL', 'NUMERIC') 
         THEN f.return_type_precision END AS NUMERIC_PRECISION,
    CASE WHEN dt.type_name IN ('DECIMAL', 'NUMERIC') 
         THEN f.return_type_scale END AS NUMERIC_SCALE,
    NULL AS DATETIME_PRECISION,
    NULL AS INTERVAL_TYPE,
    NULL AS INTERVAL_PRECISION,
    NULL AS TYPE_UDT_CATALOG,
    NULL AS TYPE_UDT_SCHEMA,
    NULL AS TYPE_UDT_NAME,
    NULL AS SCOPE_CATALOG,
    NULL AS SCOPE_SCHEMA,
    NULL AS SCOPE_NAME,
    NULL AS MAXIMUM_CARDINALITY,
    1 AS DTD_IDENTIFIER,
    CASE WHEN f.sql_security = 'INVOKER' THEN 'INVOKER' 
         ELSE 'DEFINER' END AS SECURITY_TYPE,
    f.source_text AS ROUTINE_DEFINITION,
    NULL AS EXTERNAL_NAME,
    NULL AS EXTERNAL_LANGUAGE,
    NULL AS PARAMETER_STYLE,
    CASE WHEN f.deterministic THEN 'YES' ELSE 'NO' END AS IS_DETERMINISTIC,
    NULL AS SQL_DATA_ACCESS,
    NULL AS IS_NULL_CALL,
    NULL AS SQL_PATH,
    'NO' AS SCHEMA_LEVEL_ROUTINE,
    0 AS MAX_DYNAMIC_RESULT_SETS,
    'NO' AS IS_USER_DEFINED_CAST,
    'NO' AS IS_IMPLICITLY_INVOCABLE
FROM sb_database db
CROSS JOIN sb_schema s
JOIN sb_functions f ON f.schema_id = s.schema_id
LEFT JOIN sb_types dt ON dt.type_id = f.return_type
WHERE f.is_valid = 1;
```

## SQL Standard Compliance

### SQL:2016 Core Views

| View | Status | Notes |
|------|--------|-------|
| TABLES | ✅ Complete | All columns |
| COLUMNS | ✅ Complete | All columns |
| VIEWS | ✅ Complete | Limited to standard columns |
| TABLE_CONSTRAINTS | ✅ Complete | |
| KEY_COLUMN_USAGE | ✅ Complete | |
| REFERENTIAL_CONSTRAINTS | ✅ Complete | |
| CHECK_CONSTRAINTS | ✅ Complete | |
| SCHEMATA | ✅ Complete | |
| CHARACTER_SETS | ✅ Complete | |
| COLLATIONS | ✅ Complete | |
| ROUTINES | ⚠️ Partial | Some columns NULL |
| PARAMETERS | ❌ Not implemented | |
| DOMAINS | ⚠️ Partial | Basic columns only |

## Related Specifications

- [catalog_table_layouts.md](./catalog_table_layouts.md) - Underlying tables
- [rdb_tables.md](./rdb_tables.md) - Firebird compatibility
- [pg_catalog_tables.md](./pg_catalog_tables.md) - PostgreSQL compatibility

## Appendix

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
