# ScratchBird Catalog Design Requirements
**Date**: November 8, 2025
**Status**: CRITICAL REQUIREMENTS - Must be implemented before ALPHA release

---

## Executive Summary

This document captures the authoritative design requirements for ScratchBird's system catalog structure based on the project owner's specifications. These requirements override any current implementation that conflicts with them.

---

## 1. UUID-Based References (CRITICAL)

### Requirement
**ALL object references MUST use UUIDs, NEVER names.**

### Rationale
"If we rename a table/object the system will break as the dependant pointers will be pointing to a name that no longer exists. Owners are pointers to the owner's UUID, never to the name."

### Current Issues
- ❌ `char owner[512]` fields in SchemaRecord, TableRecord, etc.
- ❌ String-based owner references throughout catalog

### Required Changes
```cpp
// WRONG (current implementation)
struct SchemaRecord {
    char owner[512];  // ❌ Name-based reference
    // ...
};

// CORRECT (required implementation)
struct SchemaRecord {
    ID owner_id;      // ✅ UUID-based reference
    // ...
};
```

### Implementation Strategy
- Use UUID fields for ALL object references
- Create hash indexes for fast name→UUID lookups in catalog
- Names are ONLY for user/SQL parsing
- UUIDs are for ALL internal operations and references

---

## 2. Schema Hierarchy Structure

### Requirement
**Schemas form a tree structure with explicit parent relationships.**

### Default Schema Tree
```
root (top-level)
├── sys (system catalogs)
│   ├── sec (security)
│   │   ├── srv (servers)
│   │   ├── users (security users - NOT home directories)
│   │   ├── roles (security roles)
│   │   └── groups (AD/LDAP groups)
│   ├── mon (monitoring)
│   └── agents (background agents)
├── app (application data)
├── users (user home directories - DIFFERENT from sys.sec.users)
├── remote (remote/federated objects)
├── emulation (database emulation layer)
│   ├── mysql (MySQL compatibility)
│   ├── postgres (PostgreSQL compatibility)
│   ├── mssql (SQL Server compatibility)
│   └── firebird (Firebird compatibility)
└── public (default user schema)
```

### Critical Notes
- There are TWO different "users" schemas with different parents and purposes:
  - `sys.sec.users`: Security/authentication users
  - `users`: User home directories for data storage
- Schema is a namespace/directory concept
- Search path resolution: current schema → sys → public

### Schema Qualification
- **Relative**: `myschema.mytable` (searches current context)
- **Absolute**: `.root.sys.sec.table` (starts from root with leading dot)

### Required Schema Fields
```cpp
struct SchemaRecord {
    ID schema_id;
    ID parent_schema_id;     // ✅ REQUIRED: Parent schema UUID (zero for root)
    char schema_name[512];
    ID owner_id;             // ✅ UUID reference, not char owner[512]
    // ... other fields
    // ❌ REMOVE: search_path_oid (session-only concept, not stored per-schema)
};
```

---

## 3. Dependencies System

### Requirement
**Single dependency table with two-way tracking (parent→child relationships only).**

### Design (Option A - Approved)
```cpp
struct DependencyRecord {
    ID dependency_id;           // Unique dependency record ID
    ID dependent_object_id;     // Object that depends ON something
    uint8_t dependent_type;     // VIEW, TRIGGER, FK, PROCEDURE, etc.
    ID referenced_object_id;    // Object being depended upon
    uint8_t referenced_type;    // TABLE, VIEW, SEQUENCE, etc.
    uint8_t dependency_type;    // NORMAL, AUTO, INTERNAL, PIN
    uint8_t reserved[5];
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Dependency Types
- **NORMAL**: User-created dependency (views, procedures, FKs)
- **AUTO**: System-created (auto-generated indexes, sequences)
- **INTERNAL**: System-critical (cannot be dropped)
- **PIN**: User-defined INTERNAL (only admin can unpin)

### Usage
- Track ALL SQL objects that reference other objects
- Used for CASCADE operations on DROP
- Prevent dropping objects with dependencies (unless CASCADE)
- Only parent→child relationships (no child→parent tracking)

---

## 4. TOAST Configuration

### Requirement
**TOAST is fully implemented. All `*_oid` fields should be activated.**

### TOAST Threshold
```
threshold = pagesize / 32

Examples:
- 4KB page:  128 byte threshold
- 16KB page: 512 byte threshold
- 64KB page: 2048 byte threshold
```

### Fields Using TOAST
- ❌ Currently marked "NOT IMPLEMENTED" but actually ✅ IMPLEMENTED:
  - `acl_oid` (access control lists)
  - `storage_params_oid` (storage parameters)
  - `check_expr_oid` (check expressions)
  - `definition_oid` (view definitions, procedure bodies)
  - `comment_text_oid` (comments)

### Required Changes
- Mark all TOAST fields as ✅ IMPLEMENTED
- Activate TOAST storage for large data
- Store view definitions in TOAST (not in-memory only)
- Comments ALWAYS use TOAST (unlimited size)

---

## 5. Index Types - Full Implementation

### Requirement
**ALL index types are fully implemented, not partial.**

### Complete Index Types (11/11)
1. ✅ B-tree (complete)
2. ✅ Hash (complete)
3. ✅ Bitmap (complete)
4. ✅ GIN (complete)
5. ✅ GiST (complete)
6. ✅ SP-GiST (complete)
7. ✅ BRIN (complete)
8. ✅ HNSW (complete)
9. ✅ R-tree (complete via GiST)
10. ✅ LSM-tree (complete)
11. ✅ Columnstore (complete)

### Hash Indexes for Catalog
- Create hash indexes for name→UUID lookups
- B-tree indexes for UUID-based operations
- Fast catalog lookups by name or ID

---

## 6. Object Types Enumeration

### Complete ObjectType Enum (32 types)
```cpp
enum class ObjectType : uint8_t {
    SCHEMA = 0,
    TABLE = 1,
    COLUMN = 2,
    INDEX = 3,
    VIEW = 4,
    SEQUENCE = 5,
    CONSTRAINT = 6,
    TRIGGER = 7,
    PROCEDURE = 8,      // Includes selectable procedures (SUSPEND)
    FUNCTION = 9,       // Same table as procedures
    DOMAIN = 10,
    COMPOSITE_TYPE = 11,
    ROLE = 12,
    USER = 13,
    GROUP = 14,
    TABLESPACE = 15,
    DATABASE = 16,
    EMULATION_TYPE = 17,
    EMULATION_SERVER = 18,
    EMULATED_DATABASE = 19,
    COLLATION = 20,
    CHARSET = 21,
    PACKAGE = 22,       // Firebird packages
    UDR = 23,           // User-Defined Resources
    COMMENT = 24,
    DEPENDENCY = 25,
    PERMISSION = 26,
    STATISTIC = 27,
    TIMEZONE = 28,
    EXTENSION = 29,
    FOREIGN_SERVER = 30,
    FOREIGN_TABLE = 31
};
```

---

## 7. Procedures and Functions

### Requirement
**Functions and procedures stored in same table with selectable flag.**

```cpp
struct ProcedureRecord {
    ID procedure_id;
    ID schema_id;
    char procedure_name[512];
    uint8_t procedure_type;      // PROCEDURE vs FUNCTION
    uint8_t is_selectable;       // ✅ For SUSPEND statement (Firebird)
    uint8_t language;            // PSQL, SQL, UDR, etc.
    uint8_t reserved;
    ID owner_id;                 // ✅ UUID reference
    uint32_t parameter_count;
    uint32_t return_type_oid;
    uint32_t body_oid;           // ✅ TOAST - procedure body
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Selectable Procedures
- Procedures with SUSPEND can be queried like views
- Firebird-specific feature
- Flag indicates whether procedure can be used in SELECT

---

## 8. Constraints System

### Requirement
**All constraint types in one table with enable/disable and deferred flags.**

```cpp
struct ConstraintRecord {
    ID constraint_id;
    ID table_id;
    char constraint_name[512];
    uint8_t constraint_type;    // PRIMARY_KEY, FOREIGN_KEY, UNIQUE, CHECK,
                                // NOT_NULL, DEFAULT, EXCLUDE,
                                // IN_SUBQUERY, NOT_IN_SUBQUERY ✅
    uint8_t is_deferrable;      // ✅ Can defer to transaction end
    uint8_t initially_deferred; // ✅ Deferred by default
    uint8_t is_enabled;         // ✅ Enable/disable enforcement
    uint8_t is_validated;       // ✅ Validated vs not validated
    uint8_t reserved_flags[3];
    uint16_t column_count;
    ID column_ids[16];
    ID referenced_table_id;
    uint16_t referenced_column_count;
    ID referenced_column_ids[16];
    uint32_t check_expr_oid;    // ✅ TOAST - for CHECK constraints
    uint32_t in_subquery_oid;   // ✅ TOAST - for IN/NOT IN subquery
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Constraint Types
- PRIMARY KEY
- FOREIGN KEY (with CASCADE/RESTRICT)
- UNIQUE
- CHECK
- NOT NULL
- DEFAULT
- EXCLUDE
- **IN (subquery)** - ✅ NEW
- **NOT IN (subquery)** - ✅ NEW

---

## 9. Comments System

### Requirement
**Separate comments table, always TOAST storage (unlimited size).**

```cpp
struct CommentRecord {
    ID comment_id;
    ID object_id;               // Object being commented
    uint8_t object_type;        // TABLE, COLUMN, VIEW, etc.
    uint8_t reserved[7];
    ID owner_id;                // ✅ UUID reference
    uint32_t comment_text_oid;  // ✅ TOAST - unlimited size
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Usage
- Comments on ANY catalog object
- Stored separately from object definition
- Always use TOAST (no size limit)

---

## 10. Security Objects

### Users Table
```cpp
struct UserRecord {
    ID user_id;
    char username[512];
    uint32_t password_hash_oid;     // ✅ TOAST - hashed password
    uint32_t user_metadata_oid;     // ✅ TOAST - JSON metadata
    ID default_schema_id;           // ✅ UUID reference
    uint8_t is_active;
    uint8_t is_superuser;
    uint8_t reserved[6];
    uint64_t created_time;
    uint64_t last_login_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Roles Table
```cpp
struct RoleRecord {
    ID role_id;
    char role_name[512];
    ID owner_id;                    // ✅ UUID reference
    uint32_t role_metadata_oid;     // ✅ TOAST - JSON metadata
    uint8_t is_active;
    uint8_t reserved[7];
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Groups Table
```cpp
struct GroupRecord {
    ID group_id;
    char group_name[512];
    char external_id[512];          // AD/LDAP group ID
    uint8_t group_type;             // LOCAL, AD, LDAP
    uint8_t reserved[7];
    uint32_t group_metadata_oid;    // ✅ TOAST - JSON metadata
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

## 11. Emulation Support

### Requirement
**Multi-step emulation process with CREATE EMULATION commands.**

### Emulation Schemas
- Emulation schemas contain VIEWS mapping to core catalog
- Protocol-specific system table emulation
- Allow external tools to query catalog in native format

### Emulation Types Table
```cpp
struct EmulationTypeRecord {
    ID emulation_type_id;
    char emulation_name[64];        // "mysql", "postgres", "mssql", "firebird"
    uint8_t version_major;
    uint8_t version_minor;
    uint16_t reserved;
    uint32_t mapping_rules_oid;     // ✅ TOAST - JSON mapping rules
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Emulation Flow
1. `CREATE EMULATION TYPE mysql VERSION 8.0`
2. `CREATE EMULATION SERVER my_mysql_emu TYPE mysql`
3. `CREATE EMULATED DATABASE mydb ON SERVER my_mysql_emu`
4. System creates views in emulation.mysql schema mapping to core catalog

---

## 12. Missing System Tables (14 Required)

### Required New Tables
1. ✅ Dependencies (dependency tracking)
2. ✅ Comments (object comments)
3. ✅ Users (authentication/authorization)
4. ✅ Roles (role-based access control)
5. ✅ Groups (AD/LDAP groups)
6. ✅ Role Memberships (user→role mapping)
7. ✅ Procedures (stored procedures/functions)
8. ✅ Procedure Parameters (parameter definitions)
9. ✅ Domains (user-defined types)
10. ✅ UDR (User-Defined Resources - external functions)
11. ✅ Emulation Types (database emulation types)
12. ✅ Emulation Servers (emulation server instances)
13. ✅ Emulated Databases (databases under emulation)
14. ✅ Packages (Firebird packages)

---

## 13. Search Path and Session State

### Search Path
- **Connection/session-level**: SET SEARCH_PATH command
- **User preference**: Stored in user metadata
- **Default**: current schema → sys → public

### Important Notes
- `search_path_oid` should NOT be in SchemaRecord
- Search path is session-only, not stored per-schema
- Relative qualification: `myschema.mytable`
- Absolute qualification: `.root.sys.sec.table` (leading dot)

---

## 14. Reference Documentation

### When in Doubt
**"Follow the FirebirdSQL structure as defined in FirebirdReferenceDocument.md"**

### Key Firebird Features to Support
- PSQL (Procedural SQL)
- Selectable procedures (SUSPEND)
- Packages
- UDR (User-Defined Resources)
- Multi-generational architecture (MGA)
- Deferred constraints
- Domains with CHECK constraints

---

## 15. Implementation Priority

### Phase 1: Critical Fixes (MUST DO)
1. Change ALL `char owner[512]` to `ID owner_id`
2. Add `ID parent_schema_id` to SchemaRecord
3. Create Dependencies table
4. Create Comments table
5. Update bootstrap to create 18 default schemas (not 8)
6. Remove `search_path_oid` from SchemaRecord

### Phase 2: Security (HIGH PRIORITY)
7. Create Users, Roles, Groups, Role Memberships tables
8. Implement GRANT/REVOKE (currently 0% complete)

### Phase 3: Stored Code (HIGH PRIORITY)
9. Create Procedures and Procedure Parameters tables
10. Create Domains table
11. Create UDR table
12. Create Packages table

### Phase 4: Emulation (MEDIUM PRIORITY)
13. Create Emulation Types, Servers, Emulated Databases tables
14. Implement emulation schema views

### Phase 5: TOAST Activation (ONGOING)
15. Persist view definitions to TOAST
16. Mark all TOAST fields as IMPLEMENTED
17. Activate TOAST for large data

---

## 16. Current vs Required State

### Current State (WRONG)
```cpp
struct SchemaRecord {
    ID schema_id;
    char schema_name[512];
    char owner[512];              // ❌ Name-based
    // ... no parent_schema_id     // ❌ Missing
    uint32_t search_path_oid;     // ❌ Should not exist
    // ...
};
```

### Required State (CORRECT)
```cpp
struct SchemaRecord {
    ID schema_id;
    ID parent_schema_id;          // ✅ Parent relationship
    char schema_name[512];
    ID owner_id;                  // ✅ UUID-based
    // ... no search_path_oid      // ✅ Removed (session-only)
    uint32_t acl_oid;             // ✅ Mark as IMPLEMENTED
    // ...
};
```

---

## 17. Estimated Effort

### Total Implementation
- **270-370 hours** (6-9 weeks)
- **Phase 1-3 (Critical)**: 90-130 hours
- **Phase 4-6 (Important)**: 90-120 hours
- **Phase 7-8 (Integrity)**: 90-120 hours

### Migration Strategy
- **Recommended**: Fresh database only (10-20 hours)
- **Alternative**: In-place migration (40-60 hours)
- **Not recommended**: Dual-version support (80-100 hours)

---

## 18. Critical Blockers for ALPHA

The following MUST be implemented before ALPHA release:

1. ❌ UUID-based references (currently name-based)
2. ❌ Schema hierarchy with parent relationships
3. ❌ Dependencies table and tracking
4. ❌ Security tables (Users, Roles, Groups)
5. ❌ GRANT/REVOKE (currently 0% complete)
6. ❌ Proper schema bootstrap (18 schemas, not 8)
7. ✅ TOAST implementation (complete, needs activation)
8. ✅ All index types (complete)
9. ✅ Views (complete as of Nov 8, 2025)
10. ✅ Sequences (complete as of Nov 3, 2025)

---

## Document Authority

This document represents the authoritative design requirements from the project owner. Any current implementation that conflicts with these requirements MUST be corrected.

**Last Updated**: November 8, 2025
**Next Review**: Before Phase 1 implementation begins
