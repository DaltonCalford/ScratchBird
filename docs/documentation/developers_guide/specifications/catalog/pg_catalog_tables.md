# Specification: pg_catalog Tables (PostgreSQL Compatibility)

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

This specification defines the pg_catalog system tables that provide PostgreSQL compatibility, mapping ScratchBird catalog tables to PostgreSQL's pg_* naming convention.

## Scope

### In Scope

- pg_catalog table definitions
- pg_ to sb_* table mappings
- PostgreSQL-specific columns
- OID to UUID mappings

### Out of Scope

- PostgreSQL-specific features not in ScratchBird
- PostgreSQL system catalogs not applicable

## Specification

### pg_catalog Schema

PostgreSQL compatibility tables are provided in the `pg_catalog` schema:

```sql
-- Create pg_catalog schema
CREATE SCHEMA pg_catalog;

-- All pg_* tables/views live here
SELECT * FROM pg_catalog.pg_tables;
SELECT * FROM pg_catalog.pg_class;
```

### Core Table Mappings

| PostgreSQL Table | ScratchBird Table | Description |
|------------------|-------------------|-------------|
| pg_class | sb_tables + sb_indexes | Tables, indexes, sequences |
| pg_attribute | sb_columns | Table columns |
| pg_type | sb_types + sb_domains | Data types |
| pg_index | sb_indexes | Index info |
| pg_constraint | sb_constraints | Constraints |
| pg_trigger | sb_triggers | Triggers |
| pg_proc | sb_procedures + sb_functions | Procedures/functions |
| pg_namespace | sb_schema | Schemas |
| pg_database | sb_database | Databases |
| pg_authid | sb_users | Authentication IDs |
| pg_auth_members | sb_role_members | Role memberships |
| pg_statistic | sb_statistics | Table statistics |
| pg_description | sb_comments | Object comments |
| pg_depend | sb_dependencies | Dependencies |

### pg_class

```sql
CREATE VIRTUAL TABLE pg_catalog.pg_class (
    oid OID,                    -- Object ID (mapped from UUID)
    relname NAME,               -- Table/index name
    relnamespace OID,           -- Schema OID
    reltype OID,                -- Row type
    reloftype OID,              -- Typed table type
    relowner OID,               -- Owner
    relam OID,                  -- Access method
    relfilenode OID,            -- File node
    reltablespace OID,          -- Tablespace
    relpages INTEGER,           -- Page count estimate
    reltuples REAL,             -- Row count estimate
    relallvisible INTEGER,      -- All-visible pages
    reltoastrelid OID,          -- TOAST table
    relhasindex BOOLEAN,        -- Has indexes
    relisshared BOOLEAN,        -- Shared across DBs
    relpersistence CHAR,        -- Persistence (p/u/t)
    relkind CHAR,               -- r=table, i=index, S=sequence, v=view
    relnatts SMALLINT,          -- Number of columns
    relchecks SMALLINT,         -- CHECK constraints
    relhasoids BOOLEAN,         -- Has OIDs
    relhaspkey BOOLEAN,         -- Has PRIMARY KEY
    relhasrules BOOLEAN,        -- Has rules
    relhastriggers BOOLEAN,     -- Has triggers
    relhassubclass BOOLEAN,     -- Has inheritance
    relrowsecurity BOOLEAN,     -- Row security enabled
    relforcerowsecurity BOOLEAN,-- Force row security
    relispopulated BOOLEAN,     -- Is populated
    relreplident CHAR,          -- Replica identity
    relispartition BOOLEAN,     -- Is partition
    relrewrite OID,             -- Rewrite table
    relfrozenxid XID,           -- Frozen XID
    relminmxid XID,             -- Frozen multixact
    relacl ACLITEM[],           -- Access privileges
    reloptions TEXT[],          -- Storage options
    relpartbound PG_NODE_TREE   -- Partition bound
);
```

**Mapping:**
```
oid              -> hash(sb_tables.table_id) to simulate OID
relname          -> sb_tables.table_name
relnamespace     -> hash(sb_tables.schema_id)
relowner         -> hash(sb_tables.owner_id)
relkind          -> CASE sb_tables.table_type
                    WHEN 'HEAP' THEN 'r'
                    WHEN 'INDEX' THEN 'i'
                    WHEN 'SEQUENCE' THEN 'S'
                    WHEN 'VIEW' THEN 'v'
                   END
relnatts         -> sb_tables.column_count
relhasindex      -> EXISTS (SELECT 1 FROM sb_indexes WHERE table_id = ...)
relhastriggers   -> EXISTS (SELECT 1 FROM sb_triggers WHERE table_id = ...)
```

### pg_attribute

```sql
CREATE VIRTUAL TABLE pg_catalog.pg_attribute (
    attrelid OID,               -- Table OID
    attname NAME,               -- Column name
    atttypid OID,               -- Type OID
    attstattarget INTEGER,      -- Statistics target
    attlen SMALLINT,            -- Type length
    attnum SMALLINT,            -- Column number
    attndims INTEGER,           -- Number of dimensions
    attcacheoff INTEGER,        -- Cache offset
    atttypmod INTEGER,          -- Type modifier
    attbyval BOOLEAN,           -- Pass by value
    attstorage CHAR,            -- Storage type
    attalign CHAR,              // Alignment
    attnotnull BOOLEAN,         -- NOT NULL
    atthasdef BOOLEAN,          -- Has default
    attidentity CHAR,           -- Identity (a=always, d=default)
    attgenerated CHAR,          -- Generated (s=stored)
    attisdropped BOOLEAN,       -- Is dropped
    attislocal BOOLEAN,         -- Is local
    attinhcount INTEGER,        -- Inheritance count
    attcollation OID,           -- Collation
    attacl ACLITEM[],           -- Access privileges
    attoptions TEXT[],          -- Attribute options
    attfdwoptions TEXT[],       -- FDW options
    attmissingval ANYARRAY      -- Missing value
);
```

**Mapping:**
```
attrelid         -> hash(sb_columns.table_id)
attname          -> sb_columns.column_name
atttypid         -> hash(sb_columns.data_type)
attnum           -> sb_columns.ordinal
attnotnull       -> NOT sb_columns.nullable
atthasdef        -> sb_columns.has_default
attidentity      -> CASE WHEN sb_columns.is_identity THEN 
                         CASE WHEN sb_columns.identity_always THEN 'a' ELSE 'd' END
                    END
attgenerated     -> CASE sb_columns.generated_type
                    WHEN 'STORED' THEN 's'
                    WHEN 'VIRTUAL' THEN 'v'
                   END
```

### pg_type

```sql
CREATE VIRTUAL TABLE pg_catalog.pg_type (
    oid OID,                    -- Type OID
    typname NAME,               -- Type name
    typnamespace OID,           -- Schema OID
    typowner OID,               -- Owner
    typlen SMALLINT,            -- Type length
    typbyval BOOLEAN,           -- Pass by value
    typtype CHAR,               -- b=base, c=composite, d=domain, e=enum
    typcategory CHAR,           -- Category
    typispreferred BOOLEAN,     -- Preferred in category
    typisdefined BOOLEAN,       -- Is defined
    typdelim CHAR,              -- Array delimiter
    typrelid OID,               -- Relation OID
    typelem OID,                -- Element type
    typarray OID,               -- Array type
    typinput REGPROC,           -- Input function
    typoutput REGPROC,          -- Output function
    typreceive REGPROC,         -- Receive function
    typsend REGPROC,            -- Send function
    typmodin REGPROC,           -- Type modifier input
    typmodout REGPROC,          -- Type modifier output
    typanalyze REGPROC,         -- Analyze function
    typalign CHAR,              -- Alignment
    typstorage CHAR,            -- Storage type
    typnotnull BOOLEAN,         -- NOT NULL
    typbasetype OID,            -- Base type (domains)
    typtypmod INTEGER,          -- Type modifier
    typndims INTEGER,           -- Number of dimensions
    typcollation OID,           -- Collation
    typdefaultbin PG_NODE_TREE, -- Default expression
    typdefault TEXT,            -- Default value
    typacl ACLITEM[]            -- Access privileges
);
```

**Mapping:**
```
oid              -> hash(sb_types.type_id) or hash(sb_domains.domain_id)
typname          -> sb_types.type_name or sb_domains.domain_name
typtype          -> CASE 
                    WHEN domain THEN 'd'
                    WHEN composite THEN 'c'
                    WHEN enum THEN 'e'
                    ELSE 'b'
                   END
typbasetype      -> For domains: hash(base_type_id)
```

### pg_index

```sql
CREATE VIRTUAL TABLE pg_catalog.pg_index (
    indexrelid OID,             -- Index OID
    indrelid OID,               -- Table OID
    indnatts SMALLINT,          -- Number of columns
    indnkeyatts SMALLINT,       -- Number of key columns
    indisunique BOOLEAN,        -- Is unique
    indisprimary BOOLEAN,       -- Is primary key
    indisexclusion BOOLEAN,     -- Is exclusion
    indimmediate BOOLEAN,       -- Immediate check
    indisclustered BOOLEAN,     -- Is clustered
    indisvalid BOOLEAN,         -- Is valid
    indcheckxmin BOOLEAN,       -- Check xmin
    indisready BOOLEAN,         -- Is ready
    indislive BOOLEAN,          -- Is live
    indisreplident BOOLEAN,     -- Is replica identity
    indkey INT2VECTOR,          -- Column numbers
    indcollation OIDVECTOR,     -- Collations
    indclass OIDVECTOR,         -- Operator classes
    indoption INT2VECTOR,       -- Per-column options
    indexprs PG_NODE_TREE,      -- Expression
    indpred PG_NODE_TREE        -- Partial index predicate
);
```

**Mapping:**
```
indexrelid       -> hash(sb_indexes.index_id)
indrelid         -> hash(sb_indexes.table_id)
indnatts         -> sb_indexes.column_count
indisunique      -> sb_indexes.is_unique
indisprimary     -> EXISTS (SELECT 1 FROM sb_constraints 
                            WHERE table_id = ... AND type = PRIMARY_KEY)
indexprs         -> sb_indexes.expression_data
indpred          -> sb_indexes.predicate_data
```

### pg_constraint

```sql
CREATE VIRTUAL TABLE pg_catalog.pg_constraint (
    oid OID,                    -- Constraint OID
    conname NAME,               -- Constraint name
    connamespace OID,           -- Schema OID
    contype CHAR,               -- c=check, f=FK, p=PK, u=unique, x=exclusion
    condeferrable BOOLEAN,      -- Is deferrable
    condeferred BOOLEAN,        -- Initially deferred
    convalidated BOOLEAN,       -- Is validated
    conrelid OID,               -- Table OID
    contypid OID,               -- Domain OID
    conindid OID,               -- Index OID
    conparentid OID,            -- Parent constraint
    confrelid OID,              -- Referenced table (FK)
    confupdtype CHAR,           -- FK update action
    confdeltype CHAR,           -- FK delete action
    confmatchtype CHAR,         -- FK match type
    conislocal BOOLEAN,         -- Is local
    coninhcount INTEGER,        -- Inheritance count
    connoinherit BOOLEAN,       -- No inherit
    conkey INT2VECTOR,          -- Constrained columns
    confkey INT2VECTOR,         -- Referenced columns (FK)
    conpfeqop OIDVECTOR,        -- Equality operators
    conppeqop OIDVECTOR,        -- Parent equality operators
    conffeqop OIDVECTOR,        -- Foreign equality operators
    conexclop OIDVECTOR,        -- Exclusion operators
    conbin PG_NODE_TREE,        -- Expression tree
    consrc TEXT                 -- Expression source
);
```

**Mapping:**
```
oid              -> hash(sb_constraints.constraint_id)
conname          -> sb_constraints.constraint_name
contype          -> CASE sb_constraints.constraint_type
                    WHEN PRIMARY_KEY THEN 'p'
                    WHEN UNIQUE THEN 'u'
                    WHEN FOREIGN_KEY THEN 'f'
                    WHEN CHECK THEN 'c'
                    WHEN EXCLUSION THEN 'x'
                   END
conrelid         -> hash(sb_constraints.table_id)
condeferrable    -> sb_constraints.is_deferrable
condeferred      -> sb_constraints.initially_deferred
confrelid        -> hash(sb_constraints.referenced_table_id)
conkey           -> sb_constraints.column_ordinals
confupdtype      -> Map FKAction to char
confdeltype      -> Map FKAction to char
```

### pg_namespace

```sql
CREATE VIRTUAL TABLE pg_catalog.pg_namespace (
    oid OID,                    -- Schema OID
    nspname NAME,               -- Schema name
    nspowner OID,               -- Owner
    nspacl ACLITEM[]            -- Access privileges
);
```

**Mapping:**
```
oid              -> hash(sb_schema.schema_id)
nspname          -> sb_schema.schema_name
nspowner         -> hash(sb_schema.owner_id)
```

## OID Generation

PostgreSQL uses 32-bit OIDs. ScratchBird uses UUIDs. For compatibility:

```cpp
// Generate pseudo-OID from UUID
OID uuidToOid(const ID& uuid) {
    // Use first 4 bytes of UUID as OID
    // Note: May have collisions, but acceptable for compatibility
    return ntohl(*reinterpret_cast<const uint32_t*>(uuid.bytes.data()));
}

// Reverse lookup (for pg_class.oid -> sb_tables lookup)
ID oidToUuid(OID oid, ObjectType type) {
    // Query appropriate catalog table with OID hash match
    // Return matching UUID
}
```

## Compatibility Limitations

1. **OID Collisions:** UUID-to-OID conversion may have collisions
2. **Missing Columns:** Some PostgreSQL-specific columns return NULL
3. **Behavioral Differences:** Some PostgreSQL features not implemented
4. **System Tables:** Not all pg_catalog tables provided

## Related Specifications

- [catalog_table_layouts.md](./catalog_table_layouts.md) - Underlying tables
- [rdb_tables.md](./rdb_tables.md) - Firebird compatibility
- [information_schema.md](./information_schema.md) - SQL standard views

## Appendix

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
