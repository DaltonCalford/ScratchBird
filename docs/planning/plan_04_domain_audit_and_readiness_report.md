# Plan 04 - Domain Implementation Audit and Readiness Report

**Report Date:** 2026-01-03
**Purpose:** Comprehensive audit of domain implementation and Plan 04 readiness assessment
**Scope:** Domain types, transaction control, parser coverage, and Plan 02/03 dependencies

---

## EXECUTIVE SUMMARY

### Overall Readiness: 🟡 MOSTLY READY WITH GAPS

| Component | Implementation Status | Parser Status | Plan 04 Impact |
|-----------|----------------------|---------------|----------------|
| **Domain Backend** | ✅ COMPREHENSIVE (2119 lines) | ✅ CONNECTED (V2 pipeline) | LOW - Core wired |
| **Transaction Control** | ✅ COMPLETE | ✅ COMPLETE (V2) | LOW - Emulated guardrails pending |
| **ScratchBird V2 Parser** | ✅ COMPLETE | ✅ Domain DDL + transactions wired | LOW - V2 coverage done |
| **Emulated Parsers** | ⚠️ PARTIAL | ❌ Domain DDL pending | HIGH - Per-dialect work |
| **Plan 02 Dependency** | N/A | ⚠️ Alignment pending | MEDIUM - Path defaults/tests |
| **Plan 03 Dependency** | N/A | ✅ COMPLETE | NONE - WITH infra wired |

**VERDICT:** Plan 04 core pipeline is complete for ScratchBird V2. Remaining work is emulated parser domain DDL, semantic guardrails, conflict opcodes, and comprehensive tests.

---

## PART 1: DOMAIN IMPLEMENTATION AUDIT

### 1.1 Domain Backend - COMPREHENSIVE IMPLEMENTATION ✅

**Location:** `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/domain_manager.h`
**Implementation:** `/home/dcalford/CliWork/ScratchBird/src/core/domain_manager.cpp` (2119 lines)

#### Domain Types Supported (ALL 5 TYPES)

```cpp
enum class DomainType : uint8_t
{
    BASIC = 0,    // ✅ IMPLEMENTED - Basic domain (wraps base type + constraints)
    RECORD = 1,   // ✅ IMPLEMENTED - RECORD/ROW type with named fields
    ENUM = 2,     // ✅ IMPLEMENTED - ENUM type with ordered values
    SET = 3,      // ✅ IMPLEMENTED - SET type with unique unordered values
    VARIANT = 4   // ✅ IMPLEMENTED - VARIANT type (runtime polymorphic)
};
```

**Status:** ✅ ALL FIVE DOMAIN TYPES FULLY IMPLEMENTED

#### Domain Features Implemented

| Feature | Status | API Method | Line Count |
|---------|--------|------------|------------|
| **Basic Domains** | ✅ COMPLETE | `createBasicDomain()` | Lines 130-177 |
| **RECORD Domains** | ✅ COMPLETE | `createRecordDomain()` | Full implementation |
| **ENUM Domains** | ✅ COMPLETE | `createEnumDomain()` | Full implementation |
| **SET Domains** | ✅ COMPLETE | `createSetDomain()` | Full implementation |
| **VARIANT Domains** | ✅ COMPLETE | `createVariantDomain()` | Full implementation |
| **Constraints** | ✅ COMPLETE | `validateValue()` | With CHECK, NOT NULL |
| **Inheritance** | ✅ COMPLETE | `setParentDomain()` | Parent linkage |
| **Security Options** | ✅ DEFINED | `setSecurityOptions()` | Masking, encryption, audit |
| **Integrity Options** | ✅ DEFINED | `setIntegrityOptions()` | Uniqueness, normalization |
| **Validation Options** | ✅ DEFINED | `setValidationOptions()` | Custom validation functions |
| **Quality Options** | ✅ DEFINED | `setQualityOptions()` | Parse, standardize, enrich |

#### Domain Storage - COMPLETE ✅

**Catalog Structure** (`src/core/catalog_manager.cpp` lines 672-683):
```cpp
struct DomainRecord
{
    ID domain_id;
    ID schema_id;
    char domain_name[512];
    ID owner_id;
    uint32_t base_type_oid;     // TOAST reference for base data type
    uint32_t check_expr_oid;    // TOAST reference for CHECK constraint
    uint8_t not_null;
    uint8_t reserved[7];
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t default_expr_oid;  // TOAST reference for default value
    uint8_t is_valid;
};
```

**CatalogManager Operations** (`src/core/catalog_manager.cpp` lines 16110-16517):
- ✅ `createDomain()` - Lines 16110-16181
- ✅ `getDomain()` - Lines 16183-16220
- ✅ `getDomainByName()` - Lines 16222-16261
- ✅ `updateDomain()` - Lines 16263-16352
- ✅ `dropDomain()` - Lines 16354-16422
- ✅ `listDomains()` - Lines 16424-16457
- ✅ `findColumnsByDomain()` - Lines 16459-16517 (dependency check)

**Status:** ✅ FULL CRUD OPERATIONS IMPLEMENTED

#### DomainManager Record Structure (On-Disk)

**Location:** `src/core/domain_manager.cpp` lines 14-43

```cpp
struct DomainRecord
{
    ID domain_id;
    ID schema_id;
    char domain_name[128];
    uint8_t domain_type;         // DomainType enum (BASIC/RECORD/ENUM/SET/VARIANT)
    uint16_t base_type;          // DataType enum
    uint32_t precision;
    uint32_t scale;
    uint8_t nullable;
    char default_value[256];
    ID parent_domain_id;         // For inheritance (INHERITS clause)
    uint8_t is_valid;           // Soft delete flag
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t constraints_oid;   // TOAST reference for constraints
    uint32_t fields_oid;        // TOAST reference for RECORD fields
    uint32_t enum_values_oid;   // TOAST reference for ENUM values
    uint16_t set_element_type;  // For SET domains
    uint16_t reserved;
};
```

**Status:** ✅ COMPREHENSIVE ON-DISK STRUCTURE SUPPORTS ALL FEATURES

#### Test Coverage - COMPREHENSIVE ✅

**Test Files:**
- ✅ `tests/unit/domains/test_domain_manager.cpp` - Basic domain tests
- ✅ `tests/unit/domains/test_record_domain.cpp` - RECORD type tests
- ✅ `tests/unit/domains/test_enum_domain.cpp` - ENUM type tests
- ✅ `tests/unit/domains/test_set_domain.cpp` - SET type tests
- ✅ `tests/unit/domains/test_variant_domain.cpp` - VARIANT type tests
- ✅ `tests/unit/domains/test_advanced_domain.cpp` - Inheritance, security, etc.

**Total:** 6 comprehensive test files

---

### 1.2 What's MISSING from Domain Implementation

#### ❌ CRITICAL MISSING: dialect_tag and compat_name

**Required by Plan 02 and Plan 04:**
```
Plan 02: "Domains must expose dialect_tag and compat_name for resolution"
Plan 04: "Parse WITH DIALECT(<tag>) and WITH COMPAT(<name>) clauses"
```

**Current DomainInfo Structure:**
```cpp
struct DomainInfo
{
    ID domain_id;
    ID schema_id;
    std::string domain_name;
    DomainType domain_type;
    // ... many fields ...

    // ❌ MISSING:
    // std::string dialect_tag;
    // std::string compat_name;
};
```

**Current DomainRecord Structure:**
```cpp
struct DomainRecord
{
    ID domain_id;
    ID schema_id;
    char domain_name[128];
    // ... many fields ...

    // ❌ MISSING:
    // char dialect_tag[32];
    // char compat_name[128];
};
```

**Impact:** HIGH
- Plan 02 resolver cannot distinguish domains by dialect
- Multi-dialect domain compatibility broken
- Parser cannot emit dialect_tag/compat_name to SBLR

**Fix Required:**
1. Add fields to `DomainRecord` and `DomainInfo`
2. Update `createDomain()` to accept dialect_tag/compat_name parameters
3. Update TOAST serialization for domain records
4. Update domain cache lookups to support dialect-aware resolution

**Estimated Effort:** 4-6 hours (schema change + migration)

---

## PART 2: SBLR OPCODES FOR DOMAINS

### 2.1 Existing Opcodes - PARTIAL ⚠️

**Location:** `include/scratchbird/sblr/opcodes.h`

```cpp
// Lines 827, 840 (duplicated in 1201, 1209):
EXT_CREATE_DOMAIN = 0x5C,      // CREATE DOMAIN domain_name AS type [constraints]
EXT_ALTER_DOMAIN = 0x010E,     // ALTER DOMAIN
EXT_DROP_DOMAIN = 0x010F,      // DROP DOMAIN
EXT_SHOW_DOMAIN = 0x64,        // SHOW DOMAIN object_name - domain definition
```

**Status:** ⚠️ CREATE/ALTER/DROP defined; conflict opcodes pending

### 2.2 Missing Domain Opcodes - PARTIAL ⚠️

**Required by Plan 04:**
```
Plan 04: "Add SBLR opcodes for ALTER/DROP DOMAIN and conflict resolution operations"
```

**Missing Opcodes:**
- ❌ `EXT_REBIND_DOMAIN` - Repoint dependent objects to new domain UUID
- ❌ `EXT_RESOLVE_DOMAIN_CONFLICT` - Admin conflict resolution

**Impact:** MEDIUM - Conflict resolution workflow not yet defined

**Fix Required:**
1. Add conflict-resolution opcodes
2. Define payload structures for each opcode
3. Implement executor handlers

**Estimated Effort:** 2-3 days

---

## PART 3: PARSER IMPLEMENTATION STATUS

### 3.1 ScratchBird V2 Parser - COMPLETE ✅

**Location:** `src/parser/parser_v2.cpp`

**Note:** V2 still has TODOs for CREATE FUNCTION/PROCEDURE/TRIGGER; those are outside Plan 04 domain scope.

**Domain Parsing:** ✅ All domain kinds + WITH blocks implemented
- `parseCreateDomain()` (BASIC/RECORD/ENUM/SET/VARIANT + WITH SECURITY/INTEGRITY/VALIDATION/QUALITY + WITH OPTIONS)
- `parseAlterDomain()` implemented
- `parseDropDomain()` implemented
- CREATE/ALTER/DROP dispatchers wired

**Status:** ✅ V2 domain parsing complete

### 3.2 Firebird Parser - STUBBED ❌

**Location:** `src/parser/firebird/firebird_parser.cpp` lines 1723-1726

```cpp
Statement* Parser::parseCreateDomain() {
    error("CREATE DOMAIN not yet implemented");
    return nullptr;
}
```

**Status:** ❌ STUB THROWS ERROR

### 3.3 PostgreSQL Parser - PARTIAL ⚠️

**Location:** `src/parser/postgresql/pg_parser_ddl.cpp`

**Current:** `parseCreateDomain()` exists and emits `EXT_CREATE_DOMAIN`, but uses legacy payload (no flags/domain kind/WITH blocks). `ALTER DOMAIN` / `DROP DOMAIN` not implemented.

**Status:** ⚠️ Payload alignment + ALTER/DROP coverage pending

### 3.4 MySQL Parser - PENDING ❌

**Current:** MySQL has no DOMAIN support; parser must explicitly reject CREATE/ALTER/DROP DOMAIN with clear errors. No explicit guardrail yet.

**Status:** ✅ CORRECTLY ABSENT (MySQL has no native DOMAIN support)

**Dialect Guardrail Needed:**
```cpp
// In mysql_parser.cpp
if (matchContextual("DOMAIN")) {
    error("CREATE DOMAIN is not supported in MySQL dialect. Use base types.");
    return nullptr;
}
```

---

## PART 4: AST NODES FOR DOMAINS

### 4.1 Basic CreateDomainStmt - EXISTS ✅

**Location:** `include/scratchbird/parser/ast_v2.h` lines 2767-2794

```cpp
class CreateDomainStmt : public Statement
{
public:
    CreateDomainStmt(const SourceSpan &span, StringPool::StringId name, TypeInfo base_type)
        : Statement(ASTKind::CREATE_DOMAIN, span), name_(name), base_type_(base_type),
          default_value_(nullptr), check_expr_(nullptr), not_null_(false) {}

    StringPool::StringId name() const { return name_; }
    const TypeInfo& baseType() const { return base_type_; }

    void setDefault(Expression* expr) { default_value_ = expr; }
    Expression* defaultValue() const { return default_value_; }

    void setCheck(Expression* expr) { check_expr_ = expr; }
    Expression* checkExpr() const { return check_expr_; }

    void setNotNull(bool not_null) { not_null_ = not_null; }
    bool isNotNull() const { return not_null_; }

private:
    StringPool::StringId name_;
    TypeInfo base_type_;
    Expression* default_value_;
    Expression* check_expr_;
    bool not_null_;
};
```

**Status:** ✅ BASIC AST NODE EXISTS

### 4.2 AST Fields - COMPLETE ✅

**Status:** ✅ CreateDomainStmt includes domain_kind, dialect_tag/compat_name, inherits, record/enum/set/variant fields, and WITH block options.

**Impact:** None - AST expansion complete for Plan 04 domain scope.

### 4.3 AST Nodes - PARTIAL ⚠️

**Status:**
- ✅ `AlterDomainStmt` (V2)
- ✅ `DropDomainStmt` (V2)
- ❌ `RebindDomainStmt` (admin conflict workflow)

**Impact:** MEDIUM - Conflict opcodes still blocked

---

## PART 5: EXECUTOR IMPLEMENTATION

### 5.1 Executor Domain Handlers - COMPLETE ✅

**Status:** ✅ CREATE/ALTER/DROP/SHOW handlers implemented

**Implemented Handlers:**
- ✅ EXT_CREATE_DOMAIN → executeCreateDomain()
- ✅ EXT_ALTER_DOMAIN → executeAlterDomain()
- ✅ EXT_DROP_DOMAIN → executeDropDomain()
- ✅ EXT_SHOW_DOMAIN → executeShowDomain()

---

## PART 6: SEMANTIC ANALYZER STATUS

### 6.1 Domain Analysis - PARTIAL ⚠️

**Location:** `src/sblr/semantic_analyzer_v2.cpp`

**Status:** ⚠️ Core analyzeCreate/Alter/Drop wired (all domain kinds); guardrails/tests pending

**Required (Remaining):**
- ⚠️ Validate CHECK/default expressions (type compatibility + semantic checks)
- ⚠️ Inheritance cycle detection + dependency checks
- ⚠️ Populate dependency metadata for downstream tooling

**Impact:** MEDIUM - Core behavior works; hardening still needed

---

## PART 7: DEPENDENCY ANALYSIS

### 7.1 Plan 02 (UUID Resolution) Dependencies

**Required from Plan 02:**
1. `resolveObjectPath()` API for domain resolution ⚠️
2. Resolver cache for domain lookups ⚠️
3. `dialect_tag` and `compat_name` support in resolver ❌

**Can We Proceed Without Plan 02?**
- ✅ YES for basic domain CREATE/ALTER/DROP (use schema_id + name lookup)
- ⚠️ PARTIAL for multi-schema domain references
- ❌ NO for dialect-aware domain resolution (requires dialect_tag/compat_name)

**Mitigation:**
```cpp
// Temporary: Direct CatalogManager lookups
Status status = catalog_->getDomainByName(schema_id, domain_name, domain_info, ctx);

// Future (Plan 02): Resolver-based lookup
Status status = catalog_->resolveObjectPath(path, ObjectType::DOMAIN, opts, domain_id, ctx);
```

**Verdict:** ✅ CAN PROCEED with basic implementation; defer dialect resolution to Plan 02 completion

### 7.2 Plan 03 (SBLR v2) Dependencies

**Required from Plan 03:**
- ✅ SBLR_VERSION = 2 (already set)
- ✅ 16-bit extended opcodes (already supported)
- ✅ Transaction payload v2 (already implemented)

**Status:** ✅ ZERO BLOCKERS FROM PLAN 03

### 7.3 Plan 03 (Security) Dependencies

**Required:** NONE - Security context is executor concern, not parser

**Status:** ✅ ZERO BLOCKERS

---

## PART 8: PLAN 04 READINESS ASSESSMENT (Updated 2026-01-03)

### 8.1 Current Readiness

**Green (Complete):**
- V2 domain DDL parser/semantic/bytecode/executor wiring (all domain kinds + WITH blocks)
- WITH block enforcement opcodes + executor handlers
- Extended transaction grammar + bytecode emission
- SHOW DOMAIN support

**Yellow (Partial):**
- Semantic validation guardrails (inheritance cycles, dependency checks, label uniqueness)
- Plan 02B alignment (dot-path defaults, cascade semantics, tests)

**Red (Pending):**
- Emulated parser domain DDL (Firebird/PostgreSQL)
- MySQL explicit DOMAIN rejection (clear errors)
- Conflict opcodes (EXT_REBIND_DOMAIN / EXT_RESOLVE_DOMAIN_CONFLICT)
- Comprehensive test coverage for emulated dialects and negative cases

### 8.2 Recommended Execution Order (Remaining)

1. Emulated parser domain DDL + dialect guardrails (Firebird/PostgreSQL; MySQL rejection)
2. Semantic analyzer guardrails + dependency checks
3. Conflict opcodes definition + payload spec
4. Comprehensive tests (dialect guardrails, negative tests, emulation fixtures)
5. Plan 02B alignment tests

---

## PART 9: OPEN QUESTIONS

1. Define conflict opcode semantics for REBIND/RESOLVE (payload + executor behavior).
2. PostgreSQL payload mapping: align to SBLR v2 payload or add compatibility shim.
3. MySQL error messaging policy for unsupported DOMAIN statements.

---

## CONCLUSION

Plan 04 is effectively complete for ScratchBird V2. Remaining work is concentrated in emulated parser coverage, semantic guardrails, conflict opcodes, and test/compatibility alignment.

**END OF REPORT**
