# Catalog Correction Implementation Plan
**Date**: November 8, 2025
**Status**: READY TO IMPLEMENT
**Estimated Effort**: 270-370 hours (6-9 weeks)
**Migration Strategy**: Fresh Database Only (recommended for ALPHA)

---

## Overview

This document outlines the step-by-step implementation plan for correcting the ScratchBird system catalog to comply with the requirements defined in `CATALOG_DESIGN_REQUIREMENTS.md`.

---

## Phase 1: Critical Structure Changes (90-130 hours)

### 1.1 Update SchemaRecord Structure (8-12 hours)

**File**: `src/core/catalog_manager.cpp` line 52-68

**Changes Required**:
```cpp
// CURRENT (WRONG)
struct SchemaRecord {
    ID schema_id;
    char schema_name[512];
    char owner[512];                // ❌ REMOVE
    uint16_t default_tablespace_id;
    uint16_t permissions;
    uint16_t default_charset;
    uint16_t reserved;
    uint32_t default_collation_id;
    uint32_t acl_oid;
    uint32_t search_path_oid;       // ❌ REMOVE
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};

// NEW (CORRECT)
struct SchemaRecord {
    ID schema_id;
    ID parent_schema_id;            // ✅ ADD: Parent schema UUID (zero for root)
    char schema_name[512];
    ID owner_id;                    // ✅ CHANGE: UUID reference
    uint16_t default_tablespace_id;
    uint16_t permissions;
    uint16_t default_charset;
    uint16_t reserved;
    uint32_t default_collation_id;
    uint32_t acl_oid;               // ✅ Keep (TOAST implemented)
    // search_path_oid removed         ✅ REMOVE (session-only)
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Also Update**:
- `include/scratchbird/core/catalog_manager.h` SchemaInfo struct
- All code that reads/writes SchemaRecord

### 1.2 Update TableRecord Structure (6-10 hours)

**File**: `src/core/catalog_manager.cpp` line 82-101

**Changes Required**:
```cpp
// ADD to TableRecord (after table_name)
ID owner_id;  // ✅ Owner UUID reference
```

**Also Update**:
- `include/scratchbird/core/catalog_manager.h` TableInfo struct

### 1.3 Update All Other Catalog Records (12-18 hours)

Apply `ID owner_id` field to:
- ✅ ColumnRecord (if needed - columns typically owned by table)
- ✅ IndexRecord
- ✅ ConstraintRecord
- ✅ SequenceRecord
- ✅ ViewRecord
- ✅ TriggerRecord (when implemented)

### 1.4 Create Dependencies System Table (20-30 hours)

**File**: `src/core/catalog_manager.cpp` (add new structures)

```cpp
// Dependency types
enum class DependencyType : uint8_t {
    NORMAL = 0,    // User-created (views, procedures, FKs)
    AUTO = 1,      // System-created (auto indexes, sequences)
    INTERNAL = 2,  // System-critical (cannot be dropped)
    PIN = 3        // User-defined INTERNAL (admin-only unpin)
};

// Dependency record on disk
struct DependencyRecord {
    ID dependency_id;           // Unique dependency ID
    ID dependent_object_id;     // Object that depends ON something
    uint8_t dependent_type;     // ObjectType enum
    uint8_t reserved1[7];
    ID referenced_object_id;    // Object being depended upon
    uint8_t referenced_type;    // ObjectType enum
    uint8_t dependency_type;    // DependencyType enum
    uint8_t reserved2[6];
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Implementation**:
1. Add `dependencies_page` to CatalogRootPage
2. Create DependencyRecord structure
3. Add to catalog initialization
4. Implement CatalogManager::createDependency()
5. Implement CatalogManager::getDependencies()
6. Implement CatalogManager::checkDependencies()
7. Update DROP operations to check dependencies

### 1.5 Create Comments System Table (15-20 hours)

**File**: `src/core/catalog_manager.cpp` (add new structures)

```cpp
// Comment record on disk
struct CommentRecord {
    ID comment_id;
    ID object_id;               // Object being commented
    uint8_t object_type;        // ObjectType enum
    uint8_t reserved[7];
    ID owner_id;                // ✅ UUID reference
    uint32_t comment_text_oid;  // ✅ TOAST - unlimited size
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

**Implementation**:
1. Add `comments_page` to CatalogRootPage
2. Create CommentRecord structure
3. Add to catalog initialization
4. Implement COMMENT ON DDL parsing
5. Implement CatalogManager::setComment()
6. Implement CatalogManager::getComment()

### 1.6 Expand CatalogRootPage (6-10 hours)

**File**: `src/core/catalog_manager.cpp` line 29-49

**Current**: 16 table pointers
**Required**: 36+ table pointers

```cpp
struct CatalogRootPage {
    PageHeader header;
    uint32_t schema_count;
    uint32_t table_count;

    // Existing (16)
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

    // New - Phase 1 (2)
    uint32_t dependencies_page;      // ✅ ADD
    uint32_t comments_page;          // ✅ ADD

    // New - Phase 2 (4)
    uint32_t users_page;             // ✅ ADD
    uint32_t roles_page;             // ✅ ADD
    uint32_t groups_page;            // ✅ ADD
    uint32_t role_memberships_page;  // ✅ ADD

    // New - Phase 3 (5)
    uint32_t procedures_page;        // ✅ ADD
    uint32_t procedure_params_page;  // ✅ ADD
    uint32_t domains_page;           // ✅ ADD
    uint32_t udr_page;               // ✅ ADD
    uint32_t packages_page;          // ✅ ADD (Firebird)

    // New - Phase 4 (3)
    uint32_t emulation_types_page;   // ✅ ADD
    uint32_t emulation_servers_page; // ✅ ADD
    uint32_t emulated_dbs_page;      // ✅ ADD

    uint8_t reserved[3896];  // Adjusted for new fields
};
```

### 1.7 Update Schema Bootstrap (20-30 hours)

**File**: `src/core/database.cpp` line 196-227

**Current**: 8 schemas (flat)
**Required**: 18 schemas (hierarchical)

```cpp
// Schema bootstrap structure
struct SchemaBootstrap {
    const char* name;
    const char* parent_name;  // nullptr for root
};

const SchemaBootstrap DEFAULT_SCHEMAS[] = {
    // Root level
    {"root", nullptr},

    // System tree
    {"sys", "root"},
    {"sec", "sys"},
    {"srv", "sec"},
    {"sec_users", "sec"},      // sys.sec.users (security users)
    {"sec_roles", "sec"},      // sys.sec.roles
    {"sec_groups", "sec"},     // sys.sec.groups
    {"mon", "sys"},            // sys.mon
    {"agents", "sys"},         // sys.agents

    // Top-level schemas
    {"app", "root"},
    {"users", "root"},         // User home directories (NOT sys.sec.users)
    {"remote", "root"},
    {"emulation", "root"},
    {"mysql", "emulation"},
    {"postgres", "emulation"},
    {"mssql", "emulation"},
    {"firebird", "emulation"},
    {"public", "root"}         // ✅ ADD: Default user schema
};
```

**Implementation**:
1. Update DEFAULT_SCHEMAS with parent relationships
2. Create schemas in correct order (parents before children)
3. Set parent_schema_id correctly
4. Update schema lookup to handle hierarchy

---

## Phase 2: Security Tables (40-60 hours)

### 2.1 Create Users Table (12-18 hours)

```cpp
struct UserRecord {
    ID user_id;
    char username[512];
    uint32_t password_hash_oid;     // ✅ TOAST
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

### 2.2 Create Roles Table (10-15 hours)

```cpp
struct RoleRecord {
    ID role_id;
    char role_name[512];
    ID owner_id;                    // ✅ UUID reference
    uint32_t role_metadata_oid;     // ✅ TOAST
    uint8_t is_active;
    uint8_t reserved[7];
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### 2.3 Create Groups Table (10-15 hours)

```cpp
struct GroupRecord {
    ID group_id;
    char group_name[512];
    char external_id[512];          // AD/LDAP group ID
    uint8_t group_type;             // LOCAL, AD, LDAP
    uint8_t reserved[7];
    uint32_t group_metadata_oid;    // ✅ TOAST
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### 2.4 Create RoleMemberships Table (8-12 hours)

```cpp
struct RoleMembershipRecord {
    ID membership_id;
    ID user_id;                     // ✅ UUID reference
    ID role_id;                     // ✅ UUID reference
    ID granted_by_user_id;          // ✅ UUID reference
    uint8_t with_admin_option;      // Can grant role to others
    uint8_t reserved[7];
    uint64_t granted_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

## Phase 3: Stored Code (50-70 hours)

### 3.1 Create Procedures Table (15-20 hours)

```cpp
struct ProcedureRecord {
    ID procedure_id;
    ID schema_id;
    char procedure_name[512];
    uint8_t procedure_type;         // PROCEDURE vs FUNCTION
    uint8_t is_selectable;          // ✅ SUSPEND support (Firebird)
    uint8_t language;               // PSQL, SQL, UDR
    uint8_t reserved;
    ID owner_id;                    // ✅ UUID reference
    uint32_t parameter_count;
    uint32_t return_type_oid;       // ✅ TOAST (complex types)
    uint32_t body_oid;              // ✅ TOAST - procedure body
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### 3.2 Create ProcedureParameters Table (10-15 hours)

```cpp
struct ProcedureParameterRecord {
    ID parameter_id;
    ID procedure_id;                // ✅ UUID reference
    char parameter_name[512];
    uint16_t ordinal;               // Parameter position
    uint8_t parameter_mode;         // IN, OUT, INOUT
    uint8_t reserved;
    uint16_t data_type;
    uint16_t reserved2;
    uint32_t type_precision;
    uint32_t type_scale;
    uint32_t default_value_oid;     // ✅ TOAST
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### 3.3 Create Domains Table (12-18 hours)

```cpp
struct DomainRecord {
    ID domain_id;
    ID schema_id;
    char domain_name[512];
    ID owner_id;                    // ✅ UUID reference
    uint16_t base_data_type;
    uint16_t reserved;
    uint32_t type_precision;
    uint32_t type_scale;
    uint8_t nullable;
    uint8_t reserved2[7];
    uint32_t default_value_oid;     // ✅ TOAST
    uint32_t check_expr_oid;        // ✅ TOAST
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### 3.4 Create UDR Table (10-15 hours)

```cpp
struct UDRRecord {
    ID udr_id;
    ID schema_id;
    char udr_name[512];
    ID owner_id;                    // ✅ UUID reference
    uint8_t udr_type;               // FUNCTION, PROCEDURE, TRIGGER
    uint8_t reserved[7];
    char engine[64];                // UDR engine name
    char entry_point[256];          // Function/class name
    char module_name[256];          // Library/module path
    uint32_t metadata_oid;          // ✅ TOAST
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### 3.5 Create Packages Table (8-12 hours)

```cpp
struct PackageRecord {
    ID package_id;
    ID schema_id;
    char package_name[512];
    ID owner_id;                    // ✅ UUID reference
    uint32_t header_oid;            // ✅ TOAST - package header
    uint32_t body_oid;              // ✅ TOAST - package body
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

## Phase 4: Emulation Support (30-40 hours)

### 4.1 Create EmulationTypes Table (10-15 hours)

```cpp
struct EmulationTypeRecord {
    ID emulation_type_id;
    char emulation_name[64];        // "mysql", "postgres", etc.
    uint8_t version_major;
    uint8_t version_minor;
    uint16_t reserved;
    uint32_t mapping_rules_oid;     // ✅ TOAST - JSON rules
    uint64_t created_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### 4.2 Create EmulationServers Table (10-12 hours)

```cpp
struct EmulationServerRecord {
    ID emulation_server_id;
    char server_name[512];
    ID emulation_type_id;           // ✅ UUID reference
    ID owner_id;                    // ✅ UUID reference
    uint32_t server_config_oid;     // ✅ TOAST - JSON config
    uint8_t is_active;
    uint8_t reserved[7];
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### 4.3 Create EmulatedDatabases Table (10-13 hours)

```cpp
struct EmulatedDatabaseRecord {
    ID emulated_db_id;
    char database_name[512];
    ID emulation_server_id;         // ✅ UUID reference
    ID schema_id;                   // ✅ Target schema
    uint32_t mapping_oid;           // ✅ TOAST - custom mappings
    uint8_t is_active;
    uint8_t reserved[7];
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

---

## Phase 5: Code Updates (60-80 hours)

### 5.1 Update CatalogManager Methods (30-40 hours)

**Files to Update**:
- `src/core/catalog_manager.cpp`
- `include/scratchbird/core/catalog_manager.h`

**Methods Requiring Changes**:
```cpp
// Schema operations
Status createSchema(const std::string& name, ID parent_schema_id, ID owner_id, ...);
Status getSchema(const std::string& name, SchemaInfo& info);
Status getSchemaById(ID schema_id, SchemaInfo& info);
Status getSchemaHierarchy(ID schema_id, std::vector<SchemaInfo>& path);

// Dependency operations (NEW)
Status createDependency(ID dependent_id, uint8_t dependent_type,
                       ID referenced_id, uint8_t referenced_type,
                       DependencyType dep_type);
Status getDependencies(ID object_id, std::vector<DependencyInfo>& deps);
Status checkDependencies(ID object_id, bool allow_cascade);
Status dropDependencies(ID object_id);

// Comment operations (NEW)
Status setComment(ID object_id, uint8_t object_type, const std::string& comment);
Status getComment(ID object_id, uint8_t object_type, std::string& comment);

// Owner lookups (NEW)
Status resolveOwnerUUID(const std::string& owner_name, ID& owner_id);
```

### 5.2 Update BytecodeGenerator (15-20 hours)

**File**: `src/sblr/bytecode_generator.cpp`

**Changes**:
- Use UUID-based lookups instead of name-based
- Handle schema hierarchy in qualified names
- Add dependency tracking for views, procedures
- Support new DDL (COMMENT ON, CREATE ROLE, etc.)

### 5.3 Update Executor (15-20 hours)

**File**: `src/sblr/executor.cpp`

**Changes**:
- Use UUID-based references for all catalog operations
- Implement dependency checking in DROP operations
- Support CASCADE/RESTRICT
- Handle schema search path resolution

---

## Phase 6: Migration/Bootstrap (20-30 hours)

### 6.1 Fresh Database Bootstrap (15-20 hours)

**File**: `src/core/database.cpp`

**Tasks**:
1. Update Database::create() to initialize all 36 catalog tables
2. Create 18 default schemas with correct hierarchy
3. Create default system user/roles
4. Initialize emulation schemas
5. Set up default permissions

### 6.2 Testing (5-10 hours)

**Tests to Create/Update**:
1. Test schema hierarchy creation
2. Test UUID-based owner references
3. Test dependency tracking
4. Test CASCADE/RESTRICT operations
5. Test comment storage/retrieval
6. Test security objects creation

---

## Implementation Order

### Week 1-2: Phase 1.1-1.3 (Structure Updates)
- Update SchemaRecord, TableRecord, and all catalog records
- Change owner fields to UUID references
- Update in-memory structures (SchemaInfo, TableInfo, etc.)

### Week 2-3: Phase 1.4-1.5 (New Tables)
- Implement Dependencies table
- Implement Comments table
- Update CatalogRootPage

### Week 3-4: Phase 1.6-1.7 (Bootstrap)
- Expand CatalogRootPage to 36 tables
- Update schema bootstrap to 18 schemas
- Implement schema hierarchy

### Week 4-5: Phase 2 (Security)
- Create Users, Roles, Groups tables
- Create RoleMemberships table
- Basic GRANT/REVOKE support

### Week 5-6: Phase 3 (Stored Code)
- Create Procedures and Parameters tables
- Create Domains and UDR tables
- Create Packages table

### Week 6-7: Phase 4 (Emulation)
- Create emulation tables
- Basic emulation framework

### Week 7-8: Phase 5 (Code Updates)
- Update CatalogManager methods
- Update BytecodeGenerator
- Update Executor

### Week 8-9: Phase 6 (Testing & Polish)
- Fresh database bootstrap
- Comprehensive testing
- Documentation updates

---

## Migration Strategy: Fresh Database Only

**Rationale**: For ALPHA phase, requiring fresh database creation is acceptable and significantly faster to implement.

**Implementation**:
1. Bump database version number
2. Add version check in Database::open()
3. Return clear error if version mismatch
4. Provide migration guide in documentation

**User Impact**:
- Users must recreate databases
- Data must be exported/imported
- Acceptable for ALPHA (not BETA/production)

---

## Risk Mitigation

### High-Risk Changes
1. **Schema hierarchy**: Test thoroughly, impacts all DDL
2. **UUID references**: Ensure all lookups updated
3. **CatalogRootPage expansion**: Cannot easily undo

### Rollback Strategy
- Keep feature branch until fully tested
- Tag commit before starting changes
- Test with comprehensive DDL/DML suite

### Testing Strategy
1. Unit tests for each new catalog table
2. Integration tests for UUID lookups
3. End-to-end tests for schema hierarchy
4. Performance tests for catalog operations

---

## Success Criteria

### Phase 1 Complete
- ✅ All catalog records use UUID owner references
- ✅ Schema hierarchy working with 18 default schemas
- ✅ Dependencies table functional
- ✅ Comments table functional
- ✅ CatalogRootPage supports 36 tables

### Phase 2 Complete
- ✅ Security tables created and functional
- ✅ Basic GRANT/REVOKE implemented
- ✅ User authentication framework in place

### Phase 3 Complete
- ✅ Stored procedures structure in place
- ✅ Domains functional
- ✅ UDR framework ready

### Phase 4 Complete
- ✅ Emulation tables created
- ✅ Basic emulation schema framework

### All Phases Complete
- ✅ All existing tests pass
- ✅ New tests for catalog changes pass
- ✅ Documentation updated
- ✅ Fresh database bootstrap works
- ✅ Ready for ALPHA release

---

## Estimated Timeline

- **Optimistic**: 270 hours (6.75 weeks at 40 hrs/week)
- **Realistic**: 320 hours (8 weeks at 40 hrs/week)
- **Pessimistic**: 370 hours (9.25 weeks at 40 hrs/week)

**Recommended Schedule**: 9 weeks with buffer for testing and issues

---

## Next Steps

1. Review this plan with stakeholders
2. Set up feature branch: `feature/catalog-corrections`
3. Begin Phase 1.1: Update SchemaRecord structure
4. Proceed sequentially through phases
5. Regular testing after each phase

---

**Document Status**: Ready for Implementation
**Last Updated**: November 8, 2025
**Approved By**: [Pending Review]
