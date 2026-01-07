# ScratchBird System Catalog Correction Plan
## Comprehensive Catalog Redesign and Migration Strategy

**Date**: November 8, 2025
**Status**: Design Phase
**Priority**: CRITICAL - Foundation for all future work

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Critical Design Issues Identified](#critical-design-issues-identified)
3. [Corrected Schema Hierarchy](#corrected-schema-hierarchy)
4. [Catalog Structure Changes](#catalog-structure-changes)
5. [New System Tables](#new-system-tables)
6. [Updated Existing Tables](#updated-existing-tables)
7. [Object Type Enumeration](#object-type-enumeration)
8. [TOAST Strategy](#toast-strategy)
9. [Implementation Phases](#implementation-phases)
10. [Migration Strategy](#migration-strategy)

---

## Executive Summary

### Design Principles Clarified

1. **UUID-Based References**: ALL object references use UUIDs, NEVER names
   - Names can change, UUIDs are immutable
   - Hash indexes for name→UUID lookups
   - B-tree indexes for ordered UUID operations

2. **Hierarchical Schema Namespace**: Schemas form a tree structure
   - Root schema (no parent)
   - System schemas under sys
   - Application schemas under app
   - User home directories under users
   - Emulation schemas under emulation

3. **Schema Resolution Path**: Configurable per-connection
   - Current schema → sys → public → (error)
   - Relative qualification: `myschema.mytable`
   - Absolute qualification: `.root.sys.sec.users.table`

4. **Dependency Tracking**: Explicit dependencies table
   - View → Table, FK → Table, Trigger → Table, etc.
   - CASCADE operations require dependency graph
   - Dependency types: NORMAL, AUTO, INTERNAL, PIN

5. **TOAST for Variable-Length Data**: Size threshold based on page size
   - Small data: Inline storage
   - Large data: TOAST reference
   - Threshold = pagesize / threshold_divisor (configurable)

---

## Critical Design Issues Identified

### Issue 1: Name-Based References (CRITICAL)

**Current Problem**:
```cpp
struct SchemaRecord {
    char owner[512];  // ❌ NAME stored as string
};
```

**Impact**:
- Renaming a user breaks all owned objects
- Cascade updates required across entire catalog
- Race conditions in concurrent rename operations

**Solution**:
```cpp
struct SchemaRecord {
    ID owner_id;      // ✅ UUID reference
};
```

**Affected Tables**: ALL catalog tables with owner references

---

### Issue 2: Missing Parent Schema Reference

**Current Problem**: No hierarchy tracking in SchemaRecord

**Impact**:
- Cannot navigate schema tree
- Cannot implement schema search paths
- Cannot validate circular references

**Solution**: Add parent_schema_id field

---

### Issue 3: No Dependency Tracking

**Current Problem**: No dependencies table

**Impact**:
- CASCADE operations impossible
- Cannot prevent breaking changes
- Cannot show object usage

**Solution**: New dependencies system table

---

### Issue 4: Incorrect Schema Hierarchy

**Current Bootstrap** (8 schemas, flat structure):
```
[root], [sys], [sec], [agents], [app], [remote], [users], [roles]
```

**Correct Hierarchy** (14+ schemas, tree structure):
```
root (no parent)
├── sys (parent: root)
│   ├── sec (parent: sys)
│   │   ├── srv (parent: sec)
│   │   ├── users (parent: sec) - User/role definitions
│   │   ├── roles (parent: sec)
│   │   └── groups (parent: sec)
│   ├── mon (parent: sys) - Monitoring tables
│   └── agents (parent: sys) - Database agents
├── app (parent: root) - Application schemas
├── users (parent: root) - User home directories
├── remote (parent: root) - Remote database mounts
└── emulation (parent: root)
    ├── mysql (parent: emulation)
    ├── postgres (parent: emulation)
    ├── mssql (parent: emulation)
    └── firebird (parent: emulation)
```

---

### Issue 5: Missing System Tables

**Not Implemented**:
- Dependencies
- Comments
- Users (sec.users schema, not just catalog)
- Roles (sec.roles schema)
- Groups (sec.groups schema)
- Role Memberships
- Domains
- UDR (User-Defined Resources)
- Emulation Types
- Emulation Servers
- Emulated Databases
- Procedures/Functions (incomplete)
- Composite Types
- Packages (Firebird)

---

### Issue 6: Incorrect Implementation Status

**Claimed as Partial/Not Implemented but Actually Complete**:
- TOAST (✅ Fully implemented)
- Full-text search indexes (✅ Fully implemented via GIN)
- GIN indexes (✅ Fully implemented)
- GiST indexes (✅ Fully implemented)
- SP-GiST indexes (✅ Fully implemented)
- BRIN indexes (✅ Fully implemented)

**Need Code Audit**: Many features marked "partial" may be complete

---

## Corrected Schema Hierarchy

### Bootstrap Schema Creation

**Initialization Order** (with hard-coded UUIDs):

```sql
-- 1. Root (UUID slot 0)
CREATE SCHEMA root WITH NO PARENT;

-- 2. System (UUID slot 1)
CREATE SCHEMA sys WITH PARENT root;

-- 3. Security (UUID slot 2)
CREATE SCHEMA sec WITH PARENT sys;

-- 4. Security subsystems (UUID slots 3-6)
CREATE SCHEMA srv WITH PARENT sec;    -- Server config
CREATE SCHEMA users WITH PARENT sec;  -- User definitions
CREATE SCHEMA roles WITH PARENT sec;  -- Role definitions
CREATE SCHEMA groups WITH PARENT sec; -- Group definitions

-- 5. System subsystems (UUID slots 7-8)
CREATE SCHEMA mon WITH PARENT sys;    -- Monitoring
CREATE SCHEMA agents WITH PARENT sys; -- Agents/scheduling

-- 6. Application area (UUID slot 9)
CREATE SCHEMA app WITH PARENT root;   -- Application schemas

-- 7. User directories (UUID slot 10)
CREATE SCHEMA users WITH PARENT root; -- User home dirs (different from sec.users!)

-- 8. Remote mounts (UUID slot 11)
CREATE SCHEMA remote WITH PARENT root;

-- 9. Emulation (UUID slots 12-16)
CREATE SCHEMA emulation WITH PARENT root;
CREATE SCHEMA mysql WITH PARENT emulation;
CREATE SCHEMA postgres WITH PARENT emulation;
CREATE SCHEMA mssql WITH PARENT emulation;
CREATE SCHEMA firebird WITH PARENT emulation;

-- 10. Public (UUID slot 17) - DEFAULT for user tables
CREATE SCHEMA public WITH PARENT root;
```

**Total**: 18 bootstrap schemas (vs current 8)

---

## Catalog Structure Changes

### Updated CatalogRootPage

**Current** (16 table pointers):
```cpp
struct CatalogRootPage {
    PageHeader header;
    uint32_t schema_count;
    uint32_t table_count;
    uint32_t schemas_page;
    uint32_t tables_page;
    uint32_t columns_page;
    uint32_t indexes_page;
    uint32_t constraints_page;
    uint32_t sequences_page;
    uint32_t views_page;
    uint32_t triggers_page;
    uint32_t permissions_page;
    uint32_t statistics_page;
    uint32_t collations_page;
    uint32_t timezones_page;
    uint32_t charsets_page;
    uint32_t collation_defs_page;
    uint8_t reserved[4024];
};
```

**Updated** (30+ table pointers):
```cpp
struct CatalogRootPage {
    PageHeader header;
    uint32_t version;           // Catalog version for migration
    uint32_t schema_count;
    uint32_t table_count;

    // Core catalog tables
    uint32_t schemas_page;
    uint32_t tables_page;
    uint32_t columns_page;
    uint32_t indexes_page;
    uint32_t constraints_page;

    // Sequences and Views
    uint32_t sequences_page;
    uint32_t views_page;

    // Procedures and Functions
    uint32_t procedures_page;      // NEW
    uint32_t procedure_params_page; // NEW
    uint32_t packages_page;         // NEW (Firebird compatibility)

    // Triggers
    uint32_t triggers_page;

    // Dependencies and Comments
    uint32_t dependencies_page;     // NEW
    uint32_t comments_page;         // NEW

    // Security
    uint32_t users_page;            // NEW
    uint32_t roles_page;            // NEW
    uint32_t groups_page;           // NEW
    uint32_t role_memberships_page; // NEW
    uint32_t permissions_page;

    // Types
    uint32_t domains_page;          // NEW
    uint32_t composite_types_page;  // NEW
    uint32_t udr_page;              // NEW (User-Defined Resources)

    // Statistics
    uint32_t statistics_page;

    // Character Sets and Collations
    uint32_t charsets_page;
    uint32_t collation_defs_page;
    uint32_t timezones_page;

    // Emulation Support
    uint32_t emulation_types_page;     // NEW
    uint32_t emulation_servers_page;   // NEW
    uint32_t emulated_databases_page;  // NEW

    // Extensibility
    uint32_t catalog_extension_page;   // NEW - For future additions
    uint32_t next_catalog_root_page;   // NEW - Chain to next root page if needed

    uint8_t reserved[3800];  // Reduced but still ample
};
```

**Size Analysis**:
- Current: 16 pointers × 4 bytes = 64 bytes + 4024 reserved = 4088 bytes
- Updated: 36 pointers × 4 bytes = 144 bytes + 3800 reserved = 3944 bytes
- Still fits in 16KB page with room for expansion

---

## New System Tables

### 1. Dependencies Table

**Purpose**: Track object dependencies for CASCADE operations

```cpp
struct DependencyRecord {
    ID dependency_id;              // UUIDv7 unique identifier
    ID dependent_object_id;        // Object that depends ON something
    uint8_t dependent_type;        // ObjectType enum
    ID referenced_object_id;       // Object being DEPENDED UPON
    uint8_t referenced_type;       // ObjectType enum
    uint8_t dependency_type;       // DependencyType enum (NORMAL, AUTO, INTERNAL, PIN)
    uint8_t reserved[5];
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~88 bytes per record (~186 per 16KB page)

**Indexes**:
- Hash: dependent_object_id → Find all dependencies of an object
- Hash: referenced_object_id → Find all dependents on an object
- B-tree: (dependent_object_id, referenced_object_id) → Unique constraint

**Example**:
```
VIEW v1 depends on TABLE t1:
  dependent_object_id = v1.view_id
  dependent_type = ObjectType::VIEW
  referenced_object_id = t1.table_id
  referenced_type = ObjectType::TABLE
  dependency_type = DependencyType::NORMAL
```

---

### 2. Comments Table

**Purpose**: Store comments on database objects (COMMENT ON statement)

```cpp
struct CommentRecord {
    ID object_id;                  // UUID of any database object
    uint8_t object_type;           // ObjectType enum
    uint8_t reserved[7];
    uint32_t comment_oid;          // TOAST reference for comment text
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~64 bytes per record (~256 per 16KB page)

**TOAST Strategy**: Always use TOAST (unlimited size comments)

**Indexes**:
- Hash: (object_id, object_type) → Primary lookup

---

### 3. Users Table

**Purpose**: User accounts and authentication

```cpp
struct UserRecord {
    ID user_id;                    // UUIDv7 unique identifier
    char username[512];            // UTF-8, max 128 characters
    ID owner_id;                   // User or role UUID who created this
    uint32_t password_hash_oid;    // TOAST ref for bcrypt/scrypt hash
    uint32_t auth_config_oid;      // TOAST ref for auth config (SSO, LDAP, etc.)
    uint8_t auth_method;           // AuthMethod enum (LOCAL, SSO, LDAP, KERBEROS, etc.)
    uint8_t is_superuser;          // 1 = superuser (bypass all checks)
    uint8_t can_login;             // 1 = can connect to database
    uint8_t is_active;             // 1 = active, 0 = disabled/locked
    uint32_t connection_limit;     // Max concurrent connections (0 = unlimited)
    uint64_t created_time;
    uint64_t last_login_time;
    uint64_t password_changed_time;
    uint64_t account_expires_time; // 0 = never expires
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~600 bytes per record (~27 per 16KB page)

**Indexes**:
- Hash: username → user_id (primary lookup)
- Hash: user_id → User record
- B-tree: last_login_time (for monitoring)

---

### 4. Roles Table

**Purpose**: Role definitions (Firebird SQL ROLE)

```cpp
struct RoleRecord {
    ID role_id;                    // UUIDv7 unique identifier
    char rolename[512];            // UTF-8, max 128 characters
    ID owner_id;                   // User or role UUID who created this
    uint8_t can_login;             // 1 = role can login (like PostgreSQL)
    uint8_t is_superuser;          // 1 = superuser role
    uint8_t is_system_role;        // 1 = system-defined, cannot drop
    uint8_t reserved[5];
    uint32_t connection_limit;     // Max connections if can_login
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~580 bytes per record (~28 per 16KB page)

**Indexes**:
- Hash: rolename → role_id
- Hash: role_id → Role record

---

### 5. Groups Table

**Purpose**: Network security group mappings

```cpp
struct GroupRecord {
    ID group_id;                   // UUIDv7 unique identifier
    char groupname[512];           // UTF-8, max 128 characters (e.g., "DOMAIN\DevTeam")
    ID owner_id;                   // User or role UUID who created this
    uint32_t external_group_oid;   // TOAST ref for external group metadata (AD, LDAP, etc.)
    uint8_t group_type;            // GroupType enum (LOCAL, ACTIVE_DIRECTORY, LDAP, etc.)
    uint8_t is_active;             // 1 = active, 0 = disabled
    uint8_t reserved[6];
    uint64_t created_time;
    uint64_t last_synced_time;     // For external groups
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~580 bytes per record

**Indexes**:
- Hash: groupname → group_id
- Hash: group_id → Group record

---

### 6. Role Memberships Table

**Purpose**: User/Role membership in roles/groups (Firebird GRANT ROLE)

```cpp
struct RoleMembershipRecord {
    ID membership_id;              // UUIDv7 unique identifier
    ID role_id;                    // Role or group being granted
    ID member_id;                  // User or role receiving membership
    uint8_t member_type;           // ObjectType::USER, ObjectType::ROLE, ObjectType::GROUP
    ID grantor_id;                 // User or role who granted membership
    uint8_t admin_option;          // 1 = WITH ADMIN OPTION (can grant to others)
    uint8_t is_default;            // 1 = Default role (auto-activated on login)
    uint8_t reserved[5];
    uint64_t granted_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~96 bytes per record

**Indexes**:
- Hash: role_id → Find all members of a role
- Hash: member_id → Find all roles for a user/role
- B-tree: (role_id, member_id) → Unique constraint

---

### 7. Procedures Table

**Purpose**: Stored procedures and functions (Firebird PSQL)

```cpp
struct ProcedureRecord {
    ID procedure_id;               // UUIDv7 unique identifier
    ID schema_id;                  // Parent schema
    char procedure_name[512];      // UTF-8, max 128 characters
    ID owner_id;                   // User or role UUID
    uint8_t procedure_type;        // ProcedureType enum (PROCEDURE, FUNCTION, TRIGGER_FUNCTION)
    uint8_t is_selectable;         // 1 = contains SUSPEND (can SELECT FROM)
    uint8_t language;              // Language enum (PSQL, SQL, PLPGSQL, EXTERNAL)
    uint8_t security_type;         // SecurityType enum (DEFINER, INVOKER)
    uint8_t deterministic;         // 1 = deterministic (safe for caching)
    uint8_t reserved[3];
    uint32_t input_params_oid;     // TOAST ref for input parameters
    uint32_t output_params_oid;    // TOAST ref for output parameters (RETURNS clause)
    uint32_t body_oid;             // TOAST ref for procedure body
    uint32_t dependencies_oid;     // TOAST ref for explicit dependencies list
    uint64_t created_time;
    uint64_t last_modified_time;
    uint64_t last_executed_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~604 bytes per record

**Indexes**:
- Hash: (schema_id, procedure_name) → procedure_id
- Hash: procedure_id → Procedure record

---

### 8. Procedure Parameters Table

**Purpose**: Store procedure/function parameters separately for fast lookup

```cpp
struct ProcedureParameterRecord {
    ID parameter_id;               // UUIDv7 unique identifier
    ID procedure_id;               // Parent procedure
    char parameter_name[512];      // UTF-8, max 128 characters
    uint16_t ordinal;              // Parameter position (0-based)
    uint8_t parameter_mode;        // ParameterMode enum (IN, OUT, INOUT)
    uint8_t reserved;
    uint16_t data_type;            // Type code
    uint32_t type_precision;       // For DECIMAL, VARCHAR, etc.
    uint32_t type_scale;           // For DECIMAL
    uint32_t default_value_oid;    // TOAST ref for default value
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~584 bytes per record

**Indexes**:
- Hash: procedure_id → Get all parameters
- B-tree: (procedure_id, ordinal) → Ordered parameters

---

### 9. Domains Table

**Purpose**: User-defined domain types (Firebird DOMAIN)

```cpp
struct DomainRecord {
    ID domain_id;                  // UUIDv7 unique identifier
    ID schema_id;                  // Parent schema
    char domain_name[512];         // UTF-8, max 128 characters
    ID owner_id;                   // User or role UUID
    uint16_t base_data_type;       // Underlying type code
    uint32_t type_precision;
    uint32_t type_scale;
    uint8_t nullable;              // 1 = NULL allowed
    uint8_t reserved[3];
    uint32_t default_value_oid;    // TOAST ref for default
    uint32_t check_expr_oid;       // TOAST ref for CHECK constraint
    uint32_t collation_id;
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~592 bytes per record

**Indexes**:
- Hash: (schema_id, domain_name) → domain_id
- Hash: domain_id → Domain record

---

### 10. UDR (User-Defined Resources) Table

**Purpose**: External functions/procedures (Firebird UDR)

```cpp
struct UDRRecord {
    ID udr_id;                     // UUIDv7 unique identifier
    ID schema_id;                  // Parent schema
    char udr_name[512];            // UTF-8, max 128 characters
    ID owner_id;                   // User or role UUID
    uint8_t udr_type;              // UDRType enum (FUNCTION, PROCEDURE, TRIGGER)
    uint8_t reserved[7];
    uint32_t library_path_oid;     // TOAST ref for .so/.dll path
    uint32_t entry_point_oid;      // TOAST ref for function name in library
    uint32_t input_params_oid;     // TOAST ref for input parameters
    uint32_t output_params_oid;    // TOAST ref for output parameters
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~592 bytes per record

**Indexes**:
- Hash: (schema_id, udr_name) → udr_id

---

### 11. Emulation Types Table

**Purpose**: Supported emulation types (mysql, postgres, firebird, mssql)

```cpp
struct EmulationTypeRecord {
    ID emulation_type_id;          // UUIDv7 unique identifier
    char type_name[64];            // "mysql", "postgres", "firebird", "mssql"
    ID emulation_schema_id;        // Schema where emulation views/tables live
    uint8_t is_enabled;            // 1 = emulation enabled
    uint8_t parser_available;      // 1 = parser installed
    uint8_t reserved[6];
    uint32_t config_oid;           // TOAST ref for emulation configuration
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~136 bytes per record

---

### 12. Emulation Servers Table

**Purpose**: Virtual emulated database servers

```cpp
struct EmulationServerRecord {
    ID server_id;                  // UUIDv7 unique identifier
    ID emulation_type_id;          // Type of emulation
    char server_name[512];         // UTF-8, max 128 characters
    ID owner_id;                   // User or role UUID
    uint32_t server_config_oid;    // TOAST ref for server settings
    uint8_t is_active;             // 1 = server active
    uint8_t reserved[7];
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~588 bytes per record

---

### 13. Emulated Databases Table

**Purpose**: Databases visible to emulated clients

```cpp
struct EmulatedDatabaseRecord {
    ID emulated_db_id;             // UUIDv7 unique identifier
    ID server_id;                  // Parent emulation server
    ID emulation_type_id;          // Type of emulation
    char database_name[512];       // UTF-8, max 128 characters
    ID schema_id;                  // Schema where DB objects live
    ID owner_id;                   // User or role UUID
    uint32_t connection_params_oid; // TOAST ref for connection settings
    uint8_t is_active;             // 1 = database active
    uint8_t reserved[7];
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~596 bytes per record

---

### 14. Packages Table (Firebird Compatibility)

**Purpose**: Package definitions (Firebird PACKAGE)

```cpp
struct PackageRecord {
    ID package_id;                 // UUIDv7 unique identifier
    ID schema_id;                  // Parent schema
    char package_name[512];        // UTF-8, max 128 characters
    ID owner_id;                   // User or role UUID
    uint32_t header_oid;           // TOAST ref for package header (interface)
    uint32_t body_oid;             // TOAST ref for package body (implementation)
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Size**: ~588 bytes per record

---

## Updated Existing Tables

### 1. SchemaRecord - CRITICAL CHANGES

**Before**:
```cpp
struct SchemaRecord {
    ID schema_id;
    char schema_name[512];
    char owner[512];               // ❌ NAME-BASED
    // ... no parent reference ...
};
```

**After**:
```cpp
struct SchemaRecord {
    ID schema_id;                  // UUIDv7 unique identifier
    ID parent_schema_id;           // ✅ NEW: Parent schema (zero for root)
    char schema_name[512];         // UTF-8, max 128 characters
    ID owner_id;                   // ✅ CHANGED: UUID reference
    uint16_t default_tablespace_id;
    uint16_t permissions;          // Bitmask (future use)
    uint16_t default_charset;
    uint16_t reserved;
    uint32_t default_collation_id;
    uint32_t acl_oid;              // ✅ TOAST (IMPLEMENTED)
    uint32_t search_path_oid;      // ❌ REMOVED (session-only concept)
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Changes**:
1. Added `parent_schema_id` for hierarchy
2. Changed `char owner[512]` → `ID owner_id`
3. Removed `search_path_oid` (not stored per-schema)
4. Mark `acl_oid` as implemented (TOAST active)

**Migration**: Lookup owner name → user_id/role_id, set parent_schema_id based on hierarchy

---

### 2. TableRecord - CRITICAL CHANGES

**Before**:
```cpp
struct TableRecord {
    ID table_id;
    ID schema_id;
    char table_name[512];
    // ... no owner field ...
};
```

**After**:
```cpp
struct TableRecord {
    ID table_id;
    ID schema_id;
    char table_name[512];
    ID owner_id;                   // ✅ NEW: Owner UUID
    uint32_t root_page;
    uint32_t column_count;
    uint64_t row_count;
    uint8_t table_type;            // TableType enum
    uint8_t has_toast;             // ✅ Mark as IMPLEMENTED
    uint16_t tablespace_id;
    uint16_t default_charset;
    uint16_t reserved1;
    uint32_t default_collation_id;
    uint32_t storage_params_oid;   // ✅ TOAST (IMPLEMENTED)
    ID toast_table_id;             // ✅ TOAST table UUID (IMPLEMENTED)
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Changes**:
1. Added `owner_id` field
2. Mark TOAST fields as implemented

---

### 3. ViewRecord - CRITICAL CHANGES

**Before**:
```cpp
struct ViewRecord {
    ID view_id;
    ID schema_id;
    char view_name[512];
    uint32_t definition_oid;       // ❌ NOT IMPLEMENTED
};
```

**After**:
```cpp
struct ViewRecord {
    ID view_id;
    ID schema_id;
    char view_name[512];
    ID owner_id;                   // ✅ NEW: Owner UUID
    uint32_t definition_oid;       // ✅ TOAST (NOW IMPLEMENTED)
    uint32_t column_names_oid;     // ✅ NEW: TOAST ref for explicit columns
    uint8_t is_materialized;       // 1 = materialized view (future)
    uint8_t check_option;          // 1 = WITH CHECK OPTION (future enforcement)
    uint8_t reserved[6];
    uint64_t created_time;
    uint64_t last_modified_time;
    uint64_t last_refreshed;       // For materialized views
    uint32_t is_valid;
    uint32_t padding;
};
```

**Changes**:
1. Added `owner_id`
2. Mark `definition_oid` as implemented
3. Added `column_names_oid` for TOAST storage
4. Added `check_option` flag

**Migration**: Persist view definitions from in-memory cache to TOAST

---

### 4. SequenceRecord - CRITICAL CHANGES

**Before**:
```cpp
struct SequenceRecord {
    ID sequence_id;
    ID schema_id;
    char sequence_name[512];
    // ... no owner field ...
};
```

**After**:
```cpp
struct SequenceRecord {
    ID sequence_id;
    ID schema_id;
    char sequence_name[512];
    ID owner_id;                   // ✅ NEW: Owner UUID
    int64_t current_value;
    int64_t increment_by;
    int64_t min_value;
    int64_t max_value;
    int64_t cache_size;
    uint8_t cycle;
    uint8_t reserved[7];
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Changes**: Added `owner_id`

---

### 5. IndexRecord - MARK AS IMPLEMENTED

**Changes**:
1. Mark all index types as ✅ FULLY IMPLEMENTED:
   - BTREE ✅
   - HASH ✅
   - HNSW ✅
   - FULLTEXT ✅ (via GIN)
   - GIN ✅
   - GIST ✅
   - BRIN ✅
   - RTREE ✅
   - SPGIST ✅
   - BITMAP ✅
   - COLUMNSTORE ✅
   - LSM ✅

2. Mark `index_params_oid` as ✅ TOAST (IMPLEMENTED)

---

### 6. ConstraintRecord - ADD FIELDS

**Before**:
```cpp
struct ConstraintRecord {
    ID constraint_id;
    ID table_id;
    char constraint_name[512];
    uint8_t constraint_type;
    // ... limited check support ...
};
```

**After**:
```cpp
enum class ConstraintType : uint8_t {
    PRIMARY_KEY = 0,
    FOREIGN_KEY = 1,
    UNIQUE = 2,
    CHECK = 3,
    NOT_NULL = 4,
    DEFAULT = 5,
    EXCLUSION = 6,
    IN_SUBQUERY = 7,        // ✅ NEW: IN (SELECT ...)
    NOT_IN_SUBQUERY = 8     // ✅ NEW: NOT IN (SELECT ...)
};

struct ConstraintRecord {
    ID constraint_id;
    ID table_id;
    ID owner_id;                   // ✅ NEW: Owner UUID
    char constraint_name[512];
    uint8_t constraint_type;       // ConstraintType enum (updated)
    uint8_t is_deferrable;         // ✅ NEW: Can defer to transaction end
    uint8_t initially_deferred;    // ✅ NEW: Start deferred or immediate
    uint8_t is_enabled;            // ✅ NEW: 1 = enforced, 0 = disabled
    uint8_t is_validated;          // ✅ NEW: 1 = data validated, 0 = not checked
    uint8_t reserved_flags[3];
    uint16_t column_count;
    ID column_ids[16];
    ID referenced_table_id;        // For FK
    uint16_t referenced_column_count;
    ID referenced_column_ids[16];
    uint32_t check_expr_oid;       // ✅ TOAST (IMPLEMENTED)
    uint32_t in_subquery_oid;      // ✅ NEW: TOAST ref for IN/NOT IN subquery
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Changes**:
1. Added owner_id
2. Added deferrable/deferred flags
3. Added enabled/validated flags
4. Added IN_SUBQUERY, NOT_IN_SUBQUERY constraint types
5. Added in_subquery_oid for TOAST storage

---

## Object Type Enumeration

### Complete ObjectType Enum

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
    PROCEDURE = 8,
    FUNCTION = 9,              // Alias for PROCEDURE (type distinction in record)
    DOMAIN = 10,
    COMPOSITE_TYPE = 11,       // User-defined composite types
    ROLE = 12,
    USER = 13,
    GROUP = 14,
    TABLESPACE = 15,
    DATABASE = 16,             // For multi-database support
    EMULATION_TYPE = 17,
    EMULATION_SERVER = 18,
    EMULATED_DATABASE = 19,
    COLLATION = 20,
    CHARSET = 21,
    PACKAGE = 22,              // Firebird PACKAGE
    UDR = 23,                  // User-Defined Resource (external function)
    COMMENT = 24,              // Comment object (for dependencies)
    DEPENDENCY = 25,           // Dependency object (for meta-dependencies)
    PERMISSION = 26,           // Permission grant
    STATISTIC = 27,            // Statistics object
    TIMEZONE = 28,
    EXTENSION = 29,            // Future: Database extensions
    FOREIGN_SERVER = 30,       // Future: Foreign data wrappers
    FOREIGN_TABLE = 31,        // Future: Foreign tables
    // Reserved for future use: 32-255
};
```

---

## TOAST Strategy

### Size Threshold Calculation

```cpp
// TOAST threshold based on page size
constexpr uint32_t TOAST_THRESHOLD_DIVISOR = 32;

uint32_t toast_threshold(uint32_t page_size) {
    return page_size / TOAST_THRESHOLD_DIVISOR;
}

// Examples:
// 4KB page:  threshold = 128 bytes
// 8KB page:  threshold = 256 bytes
// 16KB page: threshold = 512 bytes
// 32KB page: threshold = 1024 bytes
// 64KB page: threshold = 2048 bytes
```

### TOAST Usage by Field

| Field Type | Strategy | Reason |
|------------|----------|--------|
| View definitions | Always TOAST | Can be very large (complex queries) |
| Procedure bodies | Always TOAST | Can be very large (complex logic) |
| ACLs | Threshold | Small ACLs inline, large TOAST |
| Comments | Always TOAST | Unlimited size |
| Check expressions | Threshold | Most are small, some complex |
| Default values | Threshold | Most small literals, some expressions |
| Index parameters | Threshold | Small configs inline, large TOAST |

---

## Implementation Phases

### Phase 1: Critical Structure Changes (Week 1)

**Priority**: CRITICAL - Breaks existing code

1. Add `parent_schema_id` to SchemaRecord
2. Change `char owner[512]` → `ID owner_id` in ALL tables
3. Add Dependencies table
4. Add Comments table
5. Update CatalogRootPage with new pointers

**Estimated Effort**: 40-60 hours

**Blockers**: Must complete before any new features

---

### Phase 2: Security Tables (Week 2)

**Priority**: HIGH - Required for GRANT/REVOKE

1. Add Users table
2. Add Roles table
3. Add Groups table
4. Add Role Memberships table
5. Implement user/role lookups

**Estimated Effort**: 30-40 hours

**Dependencies**: Phase 1 complete

---

### Phase 3: Schema Hierarchy (Week 2-3)

**Priority**: HIGH - Required for proper organization

1. Update bootstrap to create correct hierarchy
2. Implement schema path resolution
3. Add relative/absolute path parsing
4. Update all schema lookups

Reference: `docs/specifications/SCHEMA_PATH_RESOLUTION.md`

**Estimated Effort**: 20-30 hours

**Dependencies**: Phase 1 complete

---

### Phase 4: Procedures and Types (Week 3-4)

**Priority**: MEDIUM - Required for PSQL

1. Add Procedures table
2. Add Procedure Parameters table
3. Add Domains table
4. Add UDR table
5. Add Packages table (Firebird)

**Estimated Effort**: 40-50 hours

**Dependencies**: Phase 1-2 complete

---

### Phase 5: Emulation Support (Week 4-5)

**Priority**: LOW - Future feature

1. Add Emulation Types table
2. Add Emulation Servers table
3. Add Emulated Databases table
4. Design emulation view mappings

**Estimated Effort**: 30-40 hours

**Dependencies**: Phase 1-4 complete

---

### Phase 6: TOAST Activation (Week 5-6)

**Priority**: MEDIUM - Performance optimization

1. Persist view definitions to TOAST
2. Store large ACLs in TOAST
3. Store procedure bodies in TOAST
4. Implement TOAST threshold logic

**Estimated Effort**: 20-30 hours

**Dependencies**: Phase 1, 4 complete

---

### Phase 7: Dependency Tracking (Week 6-7)

**Priority**: HIGH - Required for CASCADE

1. Populate dependencies on CREATE
2. Check dependencies on DROP
3. Implement CASCADE logic
4. Implement RESTRICT logic

**Estimated Effort**: 30-40 hours

**Dependencies**: Phase 1 complete

---

### Phase 8: Constraint Enforcement (Week 7-8)

**Priority**: HIGH - Data integrity

1. Implement PRIMARY KEY enforcement
2. Implement FOREIGN KEY enforcement
3. Implement CHECK constraint enforcement
4. Implement UNIQUE constraint enforcement
5. Add deferred constraint support

**Estimated Effort**: 60-80 hours

**Dependencies**: Phase 1, 7 complete

---

## Migration Strategy

### Option 1: In-Place Migration (Recommended for Development)

**Approach**: Upgrade existing databases

**Steps**:
1. Create catalog version field
2. Detect old catalog version on open
3. Run migration scripts
4. Convert owner names to UUIDs
5. Add parent_schema_id based on hard-coded hierarchy
6. Create new system tables
7. Mark catalog as upgraded

**Pros**:
- Preserves existing data
- No recreation needed

**Cons**:
- Complex migration logic
- Risk of partial migration failures

**Effort**: 40-60 hours

---

### Option 2: Fresh Database Only (Recommended for Alpha)

**Approach**: Only new databases get correct catalog

**Steps**:
1. Update Database::create() with new catalog
2. Add version check on Database::open()
3. Error if opening old-format database
4. Recommend recreation

**Pros**:
- Clean implementation
- No migration bugs
- Faster development

**Cons**:
- Cannot open existing databases
- Users lose data

**Effort**: 10-20 hours

**Recommendation**: Use for ALPHA phase (acceptable for development)

---

### Option 3: Dual-Version Support

**Approach**: Support both old and new catalogs

**Steps**:
1. Detect catalog version on open
2. Load appropriate structures
3. Provide upgrade path
4. Deprecate old version

**Pros**:
- Backward compatibility
- Gradual migration

**Cons**:
- Code complexity
- Maintenance burden

**Effort**: 80-100 hours

**Recommendation**: Defer until BETA

---

## Code Audit Recommendations

### Areas Requiring Audit

1. **Index Implementation Status**
   - Verify all 12 index types are complete
   - Update documentation
   - Mark FULLTEXT, GIN, GIST, SPGIST, BRIN as ✅

2. **TOAST Usage**
   - Confirm TOAST manager is working
   - Identify all `*_oid` fields
   - Activate TOAST storage

3. **Constraint Enforcement**
   - Check which constraints are enforced
   - Update enforcement flags
   - Document gaps

4. **View Persistence**
   - Confirm view definitions can be stored
   - Test TOAST for large views
   - Update catalog on CREATE VIEW

5. **Missing Features**
   - Scan for "NOT IMPLEMENTED" comments
   - Update documentation
   - Create issue tracker

---

## Summary

### Critical Changes

1. **UUID-based references** - ALL owner fields
2. **Schema hierarchy** - Add parent_schema_id
3. **Dependencies table** - Track all dependencies
4. **18 bootstrap schemas** - Correct hierarchy
5. **14 new system tables** - Users, Roles, etc.
6. **TOAST activation** - All `*_oid` fields
7. **Constraint enforcement** - Add flags

### Total Estimated Effort

- **Phase 1-3 (Critical)**: 90-130 hours (2-3 weeks)
- **Phase 4-6 (Important)**: 90-120 hours (2-3 weeks)
- **Phase 7-8 (Data Integrity)**: 90-120 hours (2-3 weeks)

**Total**: 270-370 hours (6-9 weeks for complete catalog redesign)

### Recommendation

**For ALPHA Phase**:
1. Implement Phase 1 (Critical) immediately
2. Implement Phase 2-3 (Security + Hierarchy)
3. Use "Fresh Database Only" migration
4. Defer emulation to post-ALPHA

**This gets the foundation correct while minimizing migration complexity.**

---

**End of Catalog Correction Plan**
