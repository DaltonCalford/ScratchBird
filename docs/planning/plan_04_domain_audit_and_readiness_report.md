# Plan 04 - Domain Implementation Audit and Readiness Report

**Report Date:** 2025-12-21
**Purpose:** Comprehensive audit of domain implementation and Plan 04 readiness assessment
**Scope:** Domain types, transaction control, parser coverage, and Plan 02/03 dependencies

---

## EXECUTIVE SUMMARY

### Overall Readiness: 🟡 MOSTLY READY WITH GAPS

| Component | Implementation Status | Parser Status | Plan 04 Impact |
|-----------|----------------------|---------------|----------------|
| **Domain Backend** | ✅ COMPREHENSIVE (2119 lines) | ❌ NOT CONNECTED | HIGH - Parser work needed |
| **Transaction Control** | ✅ COMPLETE (see transaction report) | ✅ PARTIAL | MEDIUM - Extension needed |
| **ScratchBird V2 Parser** | ⚠️ STUBBED | ❌ TODO comments | HIGH - Primary work item |
| **Emulated Parsers** | ⚠️ STUBBED | ❌ Not implemented | HIGH - Per-dialect work |
| **Plan 02 Dependency** | N/A | ✅ CAN PROCEED | LOW - Optional for now |
| **Plan 03 Dependency** | N/A | ✅ READY | NONE - SBLR v2 complete |

**VERDICT:** Plan 04 can proceed with focus on **connecting existing domain backend to parsers**. The heavy lifting (domain engine) is done; need parser→SBLR→executor wiring.

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

### 3.1 ScratchBird V2 Parser - PARTIAL ⚠️

**Location:** `src/parser/parser_v2.cpp` lines 279-285

```cpp
// TODO: Add more CREATE types
// if (matchContextual("FUNCTION"))   return parseCreateFunction(or_replace);
// if (matchContextual("PROCEDURE"))  return parseCreateProcedure(or_replace);
// if (matchContextual("TRIGGER"))    return parseCreateTrigger();
```

**Domain Parsing:** ⚠️ BASIC + WITH blocks implemented
- `parseCreateDomain()` (basic + WITH SECURITY/INTEGRITY/VALIDATION/QUALITY)
- `parseAlterDomain()` implemented
- `parseDropDomain()` implemented

**Status:** ⚠️ V2 domain parsing present; advanced domain kinds pending

### 3.2 Firebird Parser - STUBBED ❌

**Location:** `src/parser/firebird/firebird_parser.cpp` lines 1723-1726

```cpp
Statement* Parser::parseCreateDomain() {
    error("CREATE DOMAIN not yet implemented");
    return nullptr;
}
```

**Status:** ❌ STUB THROWS ERROR

### 3.3 PostgreSQL Parser - STUBBED ❌

**Location:** `src/parser/postgresql/pg_parser_ddl.cpp` line 1238

```cpp
void Parser::parseCreateDomain() {
    error("CREATE DOMAIN not yet implemented in PostgreSQL parser");
    // Stub
}
```

**Status:** ❌ STUB THROWS ERROR

### 3.4 MySQL Parser - NO DOMAIN SUPPORT ✅ (Correct)

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

### 4.2 Missing AST Fields - CRITICAL ❌

**Required by Plan 04:**

```cpp
// Plan 04 requirement:
// "Extend CreateDomainStmt to include domain_kind, dialect_tag, compat_name, inherits, WITH blocks"
```

**Missing Fields:**
- ❌ `DomainType domain_kind` (BASIC/RECORD/ENUM/SET/VARIANT)
- ❌ `std::string dialect_tag`
- ❌ `std::string compat_name`
- ❌ `ID parent_domain_id` (for INHERITS clause)
- ❌ `std::vector<RecordField> record_fields` (for RECORD domains)
- ❌ `std::vector<EnumValue> enum_values` (for ENUM domains)
- ❌ `DataType set_element_type` (for SET domains)
- ❌ `std::vector<DataType> variant_types` (for VARIANT domains)
- ❌ `DomainSecurity security_opts` (for WITH SECURITY block)
- ❌ `DomainIntegrity integrity_opts` (for WITH INTEGRITY block)
- ❌ `DomainValidation validation_opts` (for WITH VALIDATION block)
- ❌ `DomainQuality quality_opts` (for WITH QUALITY block)

**Impact:** HIGH - Cannot parse comprehensive domain definitions

**Fix Required:**
1. Extend `CreateDomainStmt` with all missing fields
2. Add setters/getters for each field
3. Update AST visitor pattern
4. Update bytecode generator to emit extended fields

**Estimated Effort:** 1-2 days

### 4.3 Missing AST Nodes - PARTIAL ⚠️

**Required:**
- ✅ `AlterDomainStmt` - For ALTER DOMAIN operations (v2 AST)
- ✅ `DropDomainStmt` - For DROP DOMAIN operations (v2 AST)
- ❌ `RebindDomainStmt` - For REBIND DOMAIN (admin operation)

**Impact:** MEDIUM - REBIND DOMAIN still blocked

**Estimated Effort:** 1 day

---

## PART 5: EXECUTOR IMPLEMENTATION

### 5.1 Executor Domain Handlers - PARTIAL ⚠️

**Status:** ⚠️ CREATE/ALTER/DROP handlers implemented; SHOW pending

**Implemented Handlers:**
- ✅ EXT_CREATE_DOMAIN → executeCreateDomain()
- ✅ EXT_ALTER_DOMAIN → executeAlterDomain()
- ✅ EXT_DROP_DOMAIN → executeDropDomain()

**Missing Handler:**
- ❌ EXT_SHOW_DOMAIN → executeShowDomain()

**Impact:** MEDIUM - SHOW DOMAIN still unavailable

**Estimated Effort:** 1-2 days (SHOW handler + formatting)

---

## PART 6: SEMANTIC ANALYZER STATUS

### 6.1 Domain Analysis - PARTIAL ⚠️

**Location:** `src/sblr/semantic_analyzer_v2.cpp`

**Status:** ⚠️ Basic analyzeCreate/Alter/Drop wired; validation pending

**Required:**
- ✅ `analyzeCreateDomain()` / `analyzeAlterDomain()` / `analyzeDropDomain()`
- ❌ `validateDomainConstraints()` - Semantic validation of CHECK expressions
- ❌ `resolveDomainInheritance()` - Validate INHERITS clause (no cycles)
- ❌ `collectDomainDependencies()` - Populate object_uuids for dependency tracking

**Impact:** MEDIUM - Can defer to executor for initial implementation

**Estimated Effort:** 2-3 days (validation + dependency tracking)

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

## PART 8: PLAN 04 READINESS ASSESSMENT

### 8.1 Can Plan 04 Start? ✅ YES (With Caveats)

**GREEN LIGHT ✅**
| Task | Readiness | Notes |
|------|-----------|-------|
| Transaction control extension | ✅ READY | Parser extensions straightforward |
| Firebird window specs | ✅ READY | Clear parsing patterns |
| MySQL NULL-safe equality | ✅ READY | Simple opcode addition |
| PostgreSQL ESCAPE | ✅ READY | Simple opcode addition |
| Placeholder handling | ✅ READY | Standard pattern |
| GROUP BY validation | ✅ READY | Semantic analyzer work |

**YELLOW LIGHT ⚠️**
| Task | Readiness | Blocker | Fix Time |
|------|-----------|---------|----------|
| Basic domain DDL | ⚠️ NEEDS SCHEMA FIX | Missing dialect_tag/compat_name | 4-6 hours |
| Domain opcodes | ⚠️ NEEDS DEFINITION | Only 2 opcodes exist | 2-3 days |
| Parser AST extension | ⚠️ NEEDS EXPANSION | Basic AST too simple | 1-2 days |
| Executor handlers | ⚠️ NEEDS IMPLEMENTATION | Zero handlers exist | 3-4 days |

**RED LIGHT ❌**
| Task | Readiness | Blocker | Fix Time |
|------|-----------|---------|----------|
| Comprehensive domain spec | ❌ MISSING | No BNF grammar for advanced features | 1-2 weeks |
| Domain payload encoding spec | ❌ MISSING | SBLR payload structure undefined | 1 week |
| Dialect-aware resolution | ⚠️ NEEDS IMPLEMENTATION | Plan 02 complete; implement domain lookup rules | 1-2 days |

### 8.2 Recommended Implementation Sequence

**Phase 1: Quick Wins (Start Immediately) - 3-5 days**
1. ✅ Firebird window specs
2. ✅ MySQL NULL-safe equality
3. ✅ PostgreSQL/MySQL ESCAPE handling
4. ✅ Placeholder handling
5. ✅ GROUP BY validation

**Phase 2: Basic Domain Support (After Schema Fix) - 1 week**
1. Add `dialect_tag` and `compat_name` to domain schema
2. Extend `CreateDomainStmt` AST for basic domains
3. Implement `parseCreateDomain()` for ScratchBird V2 (basic form only)
4. Add domain opcodes (EXT_ALTER_DOMAIN, EXT_DROP_DOMAIN)
5. Implement executor handlers for basic domains
6. Tests for basic domain DDL

**Phase 3: Transaction Control Extension - 1 week**
1. Parse Firebird legacy clauses (WAIT/NO WAIT, LOCK TIMEOUT)
2. Parse ON CONFLICT clause
3. Parse AUTOCOMMIT statements
4. Parse RETAINING and 2PC statements
5. Emit extended transaction payloads
6. Tests for transaction control

**Phase 4: Emulated Parser Extensions - 1 week**
1. Firebird domain parsing (basic form)
2. PostgreSQL domain parsing (basic form)
3. Dialect guardrails (reject unsupported features)
4. Cross-dialect compatibility tests

**Phase 5: Advanced Domains (DEFER or Create Spec First) - 2-4 weeks**
1. Create comprehensive domain specification document
2. Define SBLR payload structures
3. Extend AST for RECORD/ENUM/SET/VARIANT
4. Implement parser support for advanced forms
5. Implement executor handlers for advanced forms
6. Comprehensive tests

**Total Time:**
- **Without Advanced Domains:** 4-6 weeks
- **With Advanced Domains (spec exists):** 6-10 weeks
- **With Advanced Domains (create spec):** 8-12 weeks

---

## PART 9: CRITICAL QUESTIONS FOR DECISION MAKERS

### 9.1 Domain Scope for Alpha

**Q1:** Is comprehensive domain support (RECORD/ENUM/SET/VARIANT) required for Alpha, or is basic domain support (type alias + constraints) acceptable?

**Options:**
- **A)** Basic domains only (SQL-92 style) - 1 week implementation
- **B)** Full domain support (all 5 types) - 4-6 weeks implementation + 1-2 weeks spec

**Recommendation:** Option A for Alpha; defer comprehensive to Beta

### 9.2 dialect_tag and compat_name Schema Change

**Q2:** Can we proceed with schema migration to add dialect_tag/compat_name fields now, or defer to Plan 02?

**Impact:**
- **If NOW:** Enables dialect-aware domains immediately, 4-6 hour effort
- **If DEFER:** Basic domains work, but multi-dialect broken until Plan 02

**Recommendation:** DO NOW (small effort, high value)

### 9.3 Plan 02 Coordination

**Q3:** Does Plan 02 work need to complete before Plan 04 domain work, or can they proceed in parallel?

**Analysis:**
- Parser work: ✅ Independent
- Executor work: ✅ Independent
- Semantic analysis: ⚠️ Needs resolver API (can stub)

**Recommendation:** Proceed in parallel; use temporary CatalogManager lookups until Plan 02 provides resolver API

### 9.4 Advanced Domain Specification

**Q4:** Who will create the comprehensive domain specification if needed?

**Required Specification Components:**
1. Complete BNF grammar for all domain types
2. SBLR payload encoding for each type
3. Semantic validation rules
4. Inheritance semantics
5. Constraint composition rules

**Estimated Effort:** 40-60 hours (1-2 weeks)

**Recommendation:** If advanced domains required for Alpha, assign specification owner immediately

---

## PART 10: FINAL VERDICT

### Implementation Readiness Matrix

| Component | Backend | Parser | SBLR | Executor | Tests | Overall |
|-----------|---------|--------|------|----------|-------|---------|
| **Basic Domains** | ✅ 100% | ❌ 0% | ⚠️ 50% | ❌ 0% | ✅ 100% | 🟡 50% |
| **Advanced Domains** | ✅ 100% | ❌ 0% | ❌ 0% | ❌ 0% | ✅ 100% | 🟡 40% |
| **Transaction Control** | ✅ 100% | ⚠️ 60% | ✅ 100% | ✅ 80% | ✅ 90% | 🟢 86% |
| **Window Functions** | ✅ 100% | ❌ 0% | ✅ 80% | ✅ 80% | ✅ 80% | 🟡 68% |
| **Dialect Guardrails** | N/A | ❌ 0% | N/A | N/A | ❌ 0% | 🔴 0% |

### Can Plan 04 Start?

**Answer:** ✅ **YES, BUT...**

**Proceed Immediately With:**
1. Quick wins (window specs, NULL-safe, ESCAPE, placeholders, GROUP BY validation)
2. Transaction control extensions
3. Basic domain schema fix (dialect_tag/compat_name)

**Defer Until Decisions Made:**
1. Advanced domain support (RECORD/ENUM/SET/VARIANT)
2. Comprehensive domain specification
3. Dialect guardrail implementation strategy

**Total Estimated Time for Plan 04 (Excluding Advanced Domains):**
- **Optimistic:** 4-5 weeks
- **Realistic:** 6-7 weeks
- **Conservative:** 8-9 weeks

**With Advanced Domains:**
- Add 2-4 weeks (if spec exists)
- Add 4-6 weeks (if spec must be created)

---

## PART 11: ACTION ITEMS

### Immediate (Week 1)

1. **Schema Migration:** Add dialect_tag/compat_name to DomainRecord (4-6 hours)
2. **Quick Wins:** Implement Phase 1 tasks (window specs, NULL-safe, etc.) (3-5 days)
3. **Decision Meeting:** Determine domain scope for Alpha (basic vs comprehensive)

### Short-Term (Weeks 2-3)

4. **Domain Opcodes:** Define EXT_ALTER_DOMAIN, EXT_DROP_DOMAIN, etc. (2-3 days)
5. **AST Extension:** Extend CreateDomainStmt for basic domains (1-2 days)
6. **Parser Implementation:** Implement parseCreateDomain() for V2 (2-3 days)
7. **Executor Handlers:** Implement domain opcode handlers (3-4 days)

### Medium-Term (Weeks 4-6)

8. **Transaction Extensions:** Parse Firebird legacy + ON CONFLICT + AUTOCOMMIT (1 week)
9. **Emulated Parsers:** Domain support for Firebird/PostgreSQL (1 week)
10. **Dialect Guardrails:** Implement systematic feature rejection (3-4 days)
11. **Testing:** Comprehensive test coverage (ongoing)

### Long-Term (If Advanced Domains Required)

12. **Specification:** Create comprehensive domain spec (1-2 weeks)
13. **Implementation:** RECORD/ENUM/SET/VARIANT support (2-4 weeks)

---

## CONCLUSION

**Plan 04 wiring is IN PROGRESS with focus on completing advanced domain kinds and enforcement.**

**Key Findings:**
1. ⚠️ Domain backend supports BASIC + WITH block storage; advanced domain kinds pending
2. ✅ Transaction control is COMPLETE (just needs parser extension)
3. ⚠️ Parser-to-executor wiring is present for BASIC + WITH blocks
4. ⚠️ Schema needs minor fix (dialect_tag/compat_name)
5. ⚠️ Plan 03B enforcement work still pending (normalization/validation/quality)

**Critical Path:**
```
Week 1: Schema fix + Quick wins
Week 2-3: Basic domain parser→SBLR→executor wiring
Week 4-5: Transaction extensions + emulated parsers
Week 6+: Testing + dialect guardrails

Total: 6-8 weeks for core Plan 04 scope
```

**Recommendation:** **START PLAN 04 IMMEDIATELY** with Phase 1 (quick wins) while clarifying advanced domain requirements.

---

**END OF REPORT**
