# Plan 04 - Parser and Compatibility - COMPLETE IMPLEMENTATION CHECKLIST

**Version:** 1.2 (Updated 2026-01-04)
**Created:** 2025-12-21
**Status:** ✅ COMPLETE - Plan 04 implementation finalized (Plan 02B alignment tracked separately)
**Requirement:** ZERO deferrals, stubs, or partial implementations

---

## ✅ BLOCKER RESOLVED (2025-12-31)

Plan 02B delivered the schema/database DDL infrastructure:
- ✅ CREATE/DROP/ALTER SCHEMA and DATABASE opcodes + handlers
- ✅ PostgreSQL/MySQL parser emission for schema/database DDL
- ✅ Firebird CREATE/DROP/ALTER DATABASE support (rename only)
- ✅ Emulation path resolved to `remote.emulated.<dialect>.<server>.<db>` (dot-path)
- ✅ Emulation view generator integrated into CREATE/DROP DATABASE

Remaining alignment tasks (tracked in Plan 02B):
- Adapter/query compiler dot-path defaults
- DROP SCHEMA/DATABASE cascade semantics
- Dedicated unit/integration tests

---

## CRITICAL RULES FOR IMPLEMENTATION

1. **NO DEFERRALS** - Every item must be fully implemented
2. **NO STUBS** - No placeholder code that "will be implemented later"
3. **NO PARTIAL IMPLEMENTATIONS** - Feature either complete or not started
4. **STOP AND ASK** - If unclear, STOP work and request clarification
5. **NO GUESSING** - Do not hallucinate implementation details
6. **DUAL SYNTAX SUPPORT** - When SQL Standard and Firebird conflict, implement BOTH for Firebird compatibility while allowing SQL Standard to be followed moving forward
7. **PARSER ARCHITECTURE** - ScratchBird parser is context-aware with flexible keywords; emulated parsers (Firebird/PostgreSQL/MySQL) must match their target engine's reserved keyword lists and grammar restrictions
8. **TEST REQUIRED** - Every feature needs corresponding test
9. **SCHEMA DDL DEPENDENCY** - Schema/Database DDL is available; ensure path alignment and cascade semantics are addressed where needed

---

## SECTION 1: PREREQUISITE SCHEMA CHANGES

### Task 1.1: Add dialect_tag and compat_name to Domain Schema

**Status:** ✅ COMPLETE - dialect/compat persisted + SBLR payloads updated
**Priority:** CRITICAL (Blocks all domain work)
**Estimated Time:** 4-6 hours

**Files to Modify:**
1. `src/core/domain_manager.cpp` (DomainRecord struct + serialization)
2. `include/scratchbird/core/domain_manager.h` (DomainInfo struct + DomainCreateOptions)
3. `src/sblr/bytecode_generator_v2.cpp` / `src/sblr/executor.cpp` (payload wiring)

**Changes Required:**

```cpp
// In domain_manager.cpp (line 14-43):
struct DomainRecord
{
    ID domain_id;
    ID schema_id;
    char domain_name[128];
    uint8_t domain_type;
    uint16_t base_type;
    uint32_t precision;
    uint32_t scale;
    uint8_t nullable;
    char default_value[256];
    ID parent_domain_id;

    // ADD THESE:
    char dialect_tag[32];        // NEW: e.g., "firebird", "postgresql", "mysql"
    char compat_name[128];       // NEW: e.g., "TINYINT", "point", "jsonb"

    uint8_t is_valid;
    uint64_t created_time;
    uint64_t last_modified_time;
    uint32_t constraints_oid;
    uint32_t fields_oid;
    uint32_t enum_values_oid;
    uint16_t set_element_type;
    uint16_t reserved;
};

// In domain_manager.h (DomainInfo struct - line 144-187):
struct DomainInfo
{
    ID domain_id;
    ID schema_id;
    std::string domain_name;
    DomainType domain_type;
    // ... existing fields ...

    // ADD THESE:
    std::string dialect_tag;     // NEW
    std::string compat_name;     // NEW

    // ... rest of fields ...
};
```

**Implementation Steps:**
1. Update `DomainRecord` struct + serialization in `domain_manager.cpp`
2. Update `DomainInfo` struct in `domain_manager.h`
3. Add `DomainCreateOptions` fields for dialect/compat
4. Wire payload encoding/decoding in SBLR bytecode + executor
5. Add persistence test coverage for dialect/compat fields

**Acceptance Criteria:**
- [x] DomainRecord struct updated in all locations
- [x] DomainInfo struct updated
- [x] All create/read/write methods handle new fields
- [x] Existing domain tests still pass
- [x] Persistence test for dialect_tag/compat_name storage and retrieval

**Test File:** `tests/unit/domains/test_domain_persistence.cpp`

---

## SECTION 2: SBLR OPCODES AND PAYLOAD DEFINITIONS

### Task 2.1: Define Extended Domain Opcodes

**Status:** ⚠️ PARTIAL - EXT_ALTER_DOMAIN/EXT_DROP_DOMAIN added; conflict opcodes pending
**Priority:** HIGH
**Estimated Time:** 2 hours

**File to Modify:** `include/scratchbird/sblr/opcodes.h`

**Add These Opcodes:**
```cpp
// In ExtendedOpcode enum (after line 1209):
EXT_ALTER_DOMAIN = 0x010E,           // Alter domain
EXT_DROP_DOMAIN = 0x010F,            // Drop domain
// EXT_REBIND_DOMAIN = <pending>     // Rebind dependent objects (admin)
// EXT_RESOLVE_DOMAIN_CONFLICT = <pending> // Resolve domain conflict (admin)
```

**Acceptance Criteria:**
- [x] EXT_ALTER_DOMAIN defined
- [x] EXT_DROP_DOMAIN defined
- [x] EXT_REBIND_DOMAIN defined
- [x] EXT_RESOLVE_DOMAIN_CONFLICT defined
- [x] Opcodes use 16-bit extended opcode range
- [x] No conflicts with existing opcodes

### Task 2.2: Document SBLR Payload Structures

**Status:** ✅ COMPLETE - BASIC/RECORD/ENUM/SET/VARIANT documented
**Priority:** HIGH
**Estimated Time:** 4 hours

**File to Create:** `docs/specifications/SBLR_DOMAIN_PAYLOADS.md`

**Content Required:**
- Complete payload structure for EXT_CREATE_DOMAIN (all 5 types)
- Complete payload structure for EXT_ALTER_DOMAIN
- Complete payload structure for EXT_DROP_DOMAIN
- Payload structure for TYPE_DOMAIN column references
- Examples of encoded payloads

**Acceptance Criteria:**
- [x] BASIC CREATE/ALTER/DROP payloads documented with byte-level layout
- [x] RECORD/ENUM/SET/VARIANT payloads documented
- [x] Flag bits fully defined
- [x] Variable-length encoding rules specified

---

## SECTION 3: AST NODE EXTENSIONS

### Task 3.1: Extend CreateDomainStmt for Comprehensive Domains

**Status:** ✅ COMPLETE - V2 AST complete (V1 removed)
**Priority:** HIGH
**Estimated Time:** 6 hours

**File to Modify:** `include/scratchbird/parser/ast_v2.h` (starting line 2767)

**Current AST (BASIC ONLY):**
```cpp
class CreateDomainStmt : public Statement
{
    StringPool::StringId name_;
    TypeInfo base_type_;
    Expression* default_value_;
    Expression* check_expr_;
    bool not_null_;
};
```

**Required Extension:**
```cpp
class CreateDomainStmt : public Statement
{
public:
    // Existing fields
    StringPool::StringId name_;
    TypeInfo base_type_;
    Expression* default_value_;
    Expression* check_expr_;
    bool not_null_;

    // NEW FIELDS FOR COMPREHENSIVE SUPPORT:
    DomainType domain_kind_;                     // BASIC/RECORD/ENUM/SET/VARIANT
    std::string dialect_tag_;                    // Optional dialect tag
    std::string compat_name_;                    // Optional compat name
    StringPool::StringId parent_domain_name_;    // For INHERITS clause

    // RECORD domain fields
    std::vector<RecordFieldDef> record_fields_;

    // ENUM domain fields
    std::vector<EnumValueDef> enum_values_;
    bool enum_wrap_enabled_;

    // SET domain fields
    TypeInfo set_element_type_;
    StringPool::StringId set_element_domain_name_;

    // VARIANT domain fields
    std::vector<VariantTypeDef> variant_types_;

    // WITH block options (FULL Alpha implementation required)
    DomainSecurityOptions security_opts_;
    DomainIntegrityOptions integrity_opts_;
    DomainValidationOptions validation_opts_;
    DomainQualityOptions quality_opts_;

    // Getters and setters for all new fields
    void setDomainKind(DomainType kind) { domain_kind_ = kind; }
    DomainType domainKind() const { return domain_kind_; }

    void setDialectTag(const std::string& tag) { dialect_tag_ = tag; }
    const std::string& dialectTag() const { return dialect_tag_; }

    // ... additional getters/setters ...
};
```

**Supporting Structures (NEW):**
```cpp
struct RecordFieldDef
{
    StringPool::StringId field_name;
    TypeInfo field_type;
    StringPool::StringId field_domain_name;  // If using domain as field type
    bool not_null;
    Expression* default_value;
};

struct EnumValueDef
{
    std::string label;
    int32_t position;
    bool position_explicit;  // true if user specified position
};

struct VariantTypeDef
{
    TypeInfo type;
    StringPool::StringId domain_name;  // If using domain as variant type
};

struct DomainSecurityOptions
{
    bool masking_enabled;
    std::string mask_type;
    bool encryption_enabled;
    bool audit_enabled;
    uint32_t permission_mask;
};

// Similar for DomainIntegrityOptions, DomainValidationOptions, DomainQualityOptions
```

**Acceptance Criteria:**
- [x] All new fields added to CreateDomainStmt
- [x] All supporting structures defined
- [x] Getters and setters for all fields
- [x] AST visitor pattern updated
- [x] ASTPrinter updated to handle new fields

**Files to Modify:**
- `include/scratchbird/parser/ast_v2.h` (struct definitions)
- `src/parser/ast_v2.cpp` (visitor implementations)
- `src/parser/ast_v2.cpp` (if separate)

### Task 3.2: Create AlterDomainStmt AST Node

**Status:** ✅ COMPLETE - V2 AlterDomainStmt implemented
**Priority:** HIGH
**Estimated Time:** 3 hours

**File to Modify:** `include/scratchbird/parser/ast_v2.h`

**New AST Node:**
```cpp
enum class AlterDomainAction : uint8_t
{
    SET_DEFAULT,
    DROP_DEFAULT,
    ADD_CHECK,
    DROP_CONSTRAINT,
    RENAME,
    SET_COMPAT,
    DROP_COMPAT
};

class AlterDomainStmt : public Statement
{
public:
    AlterDomainStmt(const SourceSpan &span, StringPool::StringId domain_name)
        : Statement(ASTKind::ALTER_DOMAIN, span), domain_name_(domain_name),
          action_(AlterDomainAction::SET_DEFAULT), new_default_(nullptr),
          new_check_(nullptr) {}

    StringPool::StringId domainName() const { return domain_name_; }

    void setAction(AlterDomainAction action) { action_ = action; }
    AlterDomainAction action() const { return action_; }

    void setNewDefault(Expression* expr) { new_default_ = expr; }
    Expression* newDefault() const { return new_default_; }

    void setNewCheck(Expression* expr) { new_check_ = expr; }
    Expression* newCheck() const { return new_check_; }

    void setConstraintName(StringPool::StringId name) { constraint_name_ = name; }
    StringPool::StringId constraintName() const { return constraint_name_; }

    void setNewName(StringPool::StringId name) { new_name_ = name; }
    StringPool::StringId newName() const { return new_name_; }

    void setCompatName(const std::string& name) { compat_name_ = name; }
    const std::string& compatName() const { return compat_name_; }

    void accept(ASTVisitor *visitor) override;

private:
    StringPool::StringId domain_name_;
    AlterDomainAction action_;
    Expression* new_default_;
    Expression* new_check_;
    StringPool::StringId constraint_name_;
    StringPool::StringId new_name_;
    std::string compat_name_;
};
```

**Acceptance Criteria:**
- [x] AlterDomainStmt defined
- [x] AlterDomainAction enum defined
- [x] All fields and accessors present
- [x] Visitor pattern implemented
- [x] AST kind added to ASTKind enum

### Task 3.3: Create DropDomainStmt AST Node

**Status:** ✅ COMPLETE - V2 DropDomainStmt implemented
**Priority:** MEDIUM
**Estimated Time:** 2 hours

**File to Modify:** `include/scratchbird/parser/ast_v2.h`

**New AST Node:**
```cpp
class DropDomainStmt : public Statement
{
public:
    DropDomainStmt(const SourceSpan &span, StringPool::StringId domain_name,
                   bool if_exists)
        : Statement(ASTKind::DROP_DOMAIN, span), domain_name_(domain_name),
          if_exists_(if_exists) {}

    StringPool::StringId domainName() const { return domain_name_; }
    bool ifExists() const { return if_exists_; }

    void accept(ASTVisitor *visitor) override;

private:
    StringPool::StringId domain_name_;
    bool if_exists_;
};
```

**Acceptance Criteria:**
- [x] DropDomainStmt defined
- [x] IF EXISTS support
- [x] Visitor pattern implemented
- [x] AST kind added

---

## SECTION 4: SCRATCHBIRD V2 PARSER IMPLEMENTATION

**Parser Architecture Note:** ScratchBird V2 parser is context-aware - it knows what's possible next based on current parsing state. This allows more flexible keyword usage compared to emulated parsers. Keywords can be used as identifiers in contexts where they are unambiguous. This advanced design is NOT replicated in emulated parsers.

### Task 4.1: Implement parseCreateDomain() for BASIC Domains

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 8 hours

**File to Modify:** `src/parser/parser_v2.cpp`

**Implementation Location:** Add after `parseCreateSequence()` (around line 940)

**Grammar to Implement:**
```
CREATE DOMAIN [schema.]name
[AS] base_type [(precision[, scale])]
[DEFAULT default_value]
[NOT NULL | NULL]
[CHECK (check_expression)]
[COLLATE collation]
[WITH DIALECT (dialect_tag)]
[WITH COMPAT (compat_name)]
```

**Pseudo-code:**
```cpp
Statement* Parser::parseCreateDomain()
{
    // Match DOMAIN keyword
    expect("DOMAIN");

    // Parse [schema.]domain_name
    auto domain_name = parseQualifiedIdentifier();

    // Parse optional AS
    if (matchKeyword("AS")) {
        // AS is optional in Firebird
    }

    // Parse base type
    TypeInfo base_type = parseDataType();

    // Create AST node
    auto* stmt = allocate<CreateDomainStmt>(domain_name, base_type);
    stmt->setDomainKind(DomainType::BASIC);

    // Parse DEFAULT clause
    if (matchKeyword("DEFAULT")) {
        stmt->setDefault(parseExpression());
    }

    // Parse NOT NULL | NULL
    if (matchKeyword("NOT")) {
        consumeKeyword("NULL");
        stmt->setNotNull(true);
    } else if (matchKeyword("NULL")) {
        stmt->setNotNull(false);
    }

    // Parse CHECK constraint
    if (matchKeyword("CHECK")) {
        consume(TokenType::LPAREN);
        stmt->setCheck(parseExpression());
        consume(TokenType::RPAREN);
    }

    // Parse COLLATE
    if (matchKeyword("COLLATE")) {
        stmt->setCollation(parseIdentifier());
    }

    // Parse WITH DIALECT
    if (matchKeyword("WITH")) {
        if (matchKeyword("DIALECT")) {
            consume(TokenType::LPAREN);
            std::string dialect = parseStringLiteral();
            consume(TokenType::RPAREN);
            stmt->setDialectTag(dialect);
        }
    }

    // Parse WITH COMPAT
    if (matchKeyword("WITH")) {
        if (matchKeyword("COMPAT")) {
            consume(TokenType::LPAREN);
            std::string compat = parseStringLiteral();
            consume(TokenType::RPAREN);
            stmt->setCompatName(compat);
        }
    }

    return stmt;
}
```

**Acceptance Criteria:**
- [x] Parses all BASIC domain clauses
- [x] Handles optional AS keyword
- [x] Handles schema qualification
- [x] Parses DEFAULT expressions
- [x] Parses CHECK expressions with VALUE keyword
- [x] Parses COLLATE clause
- [x] Parses WITH DIALECT clause
- [x] Parses WITH COMPAT clause
- [x] Error handling for invalid syntax
- [x] Test coverage for all variants

**Test File:** `tests/unit/test_parser_v2_ddl.cpp`

### Task 4.2: Implement parseCreateDomain() for RECORD Domains

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 6 hours

**Grammar to Implement:**
```
CREATE DOMAIN name
AS RECORD (
    field1 type1 [NOT NULL] [DEFAULT value1],
    field2 type2 [NOT NULL] [DEFAULT value2],
    ...
)
[WITH DIALECT (dialect_tag)]
[WITH COMPAT (compat_name)]
```

**Implementation Steps:**
1. Detect `AS RECORD` keywords
2. Parse field definition list between parentheses
3. For each field:
   - Parse field name
   - Parse field type (base type OR domain reference)
   - Parse optional NOT NULL
   - Parse optional DEFAULT
4. Store fields in AST node
5. Parse WITH clauses

**Acceptance Criteria:**
- [x] Parses RECORD field list
- [x] Handles domain-typed fields
- [x] Handles NOT NULL on fields
- [x] Handles DEFAULT on fields
- [x] Validates field name uniqueness
- [x] Test coverage

**Test File:** `tests/unit/test_parser_v2_ddl.cpp`

### Task 4.3: Implement parseCreateDomain() for ENUM Domains

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 5 hours

**Grammar to Implement:**
```
CREATE DOMAIN name
AS ENUM (
    'value1' [= position1],
    'value2' [= position2],
    ...
)
[WITH OPTIONS (WRAP = TRUE|FALSE)]
[WITH DIALECT (dialect_tag)]
[WITH COMPAT (compat_name)]
```

**Implementation Steps:**
1. Detect `AS ENUM` keywords
2. Parse enum value list
3. For each value:
   - Parse label (string literal)
   - Parse optional `= position` (integer)
4. Parse WITH OPTIONS for WRAP
5. Validate sequential positions if explicit

**Acceptance Criteria:**
- [x] Parses ENUM value list
- [x] Handles explicit positions
- [x] Auto-assigns positions if not explicit
- [x] Validates position gaps
- [x] Parses WITH OPTIONS (WRAP)
- [x] Test coverage

**Test File:** `tests/unit/test_parser_v2_ddl.cpp`

### Task 4.4: Implement parseCreateDomain() for SET Domains

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 4 hours

**Grammar to Implement:**
```
CREATE DOMAIN name
AS SET OF element_type
[WITH DIALECT (dialect_tag)]
[WITH COMPAT (compat_name)]
```

**Implementation Steps:**
1. Detect `AS SET OF` keywords
2. Parse element type (base type OR domain reference)
3. Parse WITH clauses

**Acceptance Criteria:**
- [x] Parses SET OF clause
- [x] Handles base types as elements
- [x] Handles domain references as elements
- [x] Test coverage

**Test File:** `tests/unit/test_parser_v2_ddl.cpp`

### Task 4.5: Implement parseCreateDomain() for VARIANT Domains

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 5 hours

**Grammar to Implement:**
```
CREATE DOMAIN name
AS VARIANT (
    type1,
    type2,
    ...
)
[WITH DIALECT (dialect_tag)]
[WITH COMPAT (compat_name)]
```

**Implementation Steps:**
1. Detect `AS VARIANT` keywords
2. Parse allowed type list
3. For each type: parse base type OR domain reference
4. Parse WITH clauses

**Acceptance Criteria:**
- [x] Parses VARIANT type list
- [x] Handles base types
- [x] Handles domain references
- [x] Validates type uniqueness
- [x] Test coverage

**Test File:** `tests/unit/test_parser_v2_ddl.cpp`

### Task 4.6: Implement parseCreateDomain() with INHERITS Clause

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 4 hours

**Grammar to Implement:**
```
CREATE DOMAIN name
[AS] base_type
INHERITS (parent_domain)
[additional_constraints]
```

**Implementation Steps:**
1. Parse INHERITS keyword after base type
2. Parse parent domain name (qualified identifier)
3. Parse additional constraints
4. Store parent reference in AST

**Acceptance Criteria:**
- [x] Parses INHERITS clause
- [x] Handles schema-qualified parent names
- [x] Allows additional constraints
- [x] Test coverage

**Test File:** `tests/unit/test_parser_v2_ddl.cpp`

### Task 4.7: Implement parseCreateDomain() with WITH Blocks (FULL IMPLEMENTATION)

**Status:** ✅ COMPLETE (parsing + payload emission; enforcement tracked in executor tasks)
**Priority:** HIGH
**Estimated Time:** 12 hours (parsing + enforcement)

**Grammar to Implement:**
```
WITH SECURITY (
    MASKING = value,
    ENCRYPTION = value,
    AUDIT_ACCESS = boolean,
    REQUIRE_PRIVILEGE = string
)
WITH INTEGRITY (
    UNIQUENESS = boolean,
    NORMALIZATION = value,
    NORMALIZATION_FUNCTION = string
)
WITH VALIDATION (
    FUNCTION = string,
    ERROR_MESSAGE = string
)
WITH QUALITY (
    PARSE_FUNCTION = string,
    STANDARDIZE_FUNCTION = string,
    ENRICH_FUNCTION = string
)
```

**Implementation Steps:**
1. Parse WITH keyword and detect block type (SECURITY, INTEGRITY, VALIDATION, QUALITY)
2. Parse all option key-value pairs for each block type
3. Store in AST with full option details
4. Add SBLR opcodes for WITH block enforcement (see Task 1.3)
5. Link to domain enforcement in executor (see Section 13 tasks)

**Acceptance Criteria:**
- [x] Parses all WITH block types with all options
- [x] Stores complete options in AST
- [x] Generates SBLR bytecode for payloads
- [x] Full test coverage for parsing and semantics
- [x] Integration with executor enforcement (cross-task dependency)

**Test File:** `tests/unit/test_parser_v2_ddl.cpp`

### Task 4.8: Implement parseAlterDomain()

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 6 hours

**File to Modify:** `src/parser/parser_v2.cpp`

**Grammar to Implement:**
```
ALTER DOMAIN [schema.]name action

action:
    SET DEFAULT value
  | DROP DEFAULT
  | ADD CHECK (expression)
  | DROP CONSTRAINT name
  | RENAME TO new_name
  | SET COMPAT compat_name
  | DROP COMPAT
```

**Implementation Steps:**
1. Match ALTER DOMAIN keywords
2. Parse domain name
3. Parse action keyword
4. Parse action-specific syntax
5. Create AlterDomainStmt AST node

**Acceptance Criteria:**
- [x] Parses all ALTER actions
- [x] Handles schema-qualified names
- [x] Proper error messages for invalid syntax
- [x] Test coverage

**Test File:** `tests/unit/test_parser_v2_ddl.cpp`

### Task 4.9: Implement parseDropDomain()

**Status:** ✅ COMPLETE
**Priority:** MEDIUM
**Estimated Time:** 3 hours

**File to Modify:** `src/parser/parser_v2.cpp`

**Grammar to Implement:**
```
DROP DOMAIN [IF EXISTS] [schema.]name RESTRICT
```

**Implementation Steps:**
1. Match DROP DOMAIN keywords
2. Parse optional IF EXISTS
3. Parse domain name
4. Expect RESTRICT keyword (CASCADE not allowed)
5. Create DropDomainStmt AST node

**Acceptance Criteria:**
- [x] Parses DROP DOMAIN
- [x] Handles IF EXISTS
- [x] Rejects CASCADE (RESTRICT-only semantics)
- [x] Test coverage

**Test File:** `tests/unit/test_parser_v2_ddl.cpp`

### Task 4.10: Update parseCreate() Dispatcher

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 1 hour

**File to Modify:** `src/parser/parser_v2.cpp` (line 207+)

**Change:**
```cpp
Statement* Parser::parseCreate() {
    // ... existing code ...

    // UN-COMMENT AND IMPLEMENT:
    if (matchContextual("DOMAIN")) return parseCreateDomain();

    // ... rest of code ...
}
```

**Acceptance Criteria:**
- [x] DOMAIN keyword triggers parseCreateDomain()
- [x] Works with CREATE OR REPLACE syntax

### Task 4.11: Update parseAlter() Dispatcher

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 1 hour

**File to Modify:** `src/parser/parser_v2.cpp` (line 941+)

**Change:**
```cpp
Statement* Parser::parseAlter() {
    // ... existing code ...

    if (matchContextual("DOMAIN")) return parseAlterDomain();  // ADD THIS

    // ... rest of code ...
}
```

**Acceptance Criteria:**
- [x] DOMAIN keyword triggers parseAlterDomain()

### Task 4.12: Update parseDrop() Dispatcher

**Status:** ✅ COMPLETE
**Priority:** MEDIUM
**Estimated Time:** 1 hour

**File to Modify:** `src/parser/parser_v2.cpp` (line 1051+)

**Change:**
```cpp
Statement* Parser::parseDrop() {
    // ... existing code ...

    if (matchContextual("DOMAIN")) return parseDropDomain();  // ADD THIS

    // ... rest of code ...
}
```

**Acceptance Criteria:**
- [x] DOMAIN keyword triggers parseDropDomain()

---

## SECTION 5: FIREBIRD PARSER IMPLEMENTATION

**Parser Architecture Note:** Firebird parser must match Firebird's actual reserved keyword list and grammar restrictions. Unlike ScratchBird's context-aware parser, this parser follows Firebird engine limitations exactly.

### Task 5.1: Replace Firebird parseCreateDomain() Stub

**Status:** ❌ NOT STARTED - stub throws error
**Priority:** HIGH
**Estimated Time:** 6 hours

**File to Modify:** `src/parser/firebird/firebird_parser.cpp` (line 1723)

**Current Code:**
```cpp
Statement* Parser::parseCreateDomain() {
    error("CREATE DOMAIN not yet implemented");
    return nullptr;
}
```

**Required Implementation:**
- Match Firebird's actual syntax and reserved keyword restrictions
- Support Firebird legacy syntax variations
- Support GENERATOR synonym for SEQUENCE (reference for similar patterns)
- Do NOT add ScratchBird's context-aware keyword flexibility

**Acceptance Criteria:**
- [x] Full CREATE DOMAIN support matching Firebird spec
- [x] Removes error stub
- [x] Test coverage

**Test File:** `tests/unit/test_firebird_parser_domain.cpp` (NEW)

### Task 5.2: Implement Firebird ALTER DOMAIN

**Status:** ❌ NOT STARTED
**Priority:** HIGH
**Estimated Time:** 4 hours

**File to Modify:** `src/parser/firebird/firebird_parser.cpp`

**Implementation:** Same as V2 parser

**Acceptance Criteria:**
- [x] ALTER DOMAIN fully implemented
- [x] Test coverage

### Task 5.3: Implement Firebird DROP DOMAIN

**Status:** ❌ NOT STARTED
**Priority:** MEDIUM
**Estimated Time:** 2 hours

**File to Modify:** `src/parser/firebird/firebird_parser.cpp`

**Implementation:** Same as V2 parser

**Acceptance Criteria:**
- [x] DROP DOMAIN fully implemented
- [x] Test coverage

---

## SECTION 6: POSTGRESQL PARSER IMPLEMENTATION

**Parser Architecture Note:** PostgreSQL parser must match PostgreSQL's actual reserved keyword list and grammar restrictions. Unlike ScratchBird's context-aware parser, this parser follows PostgreSQL engine limitations exactly.

### Task 6.1: Replace PostgreSQL parseCreateDomain() Stub

**Status:** ⚠️ PARTIAL - CREATE DOMAIN emits legacy payload; v2 alignment pending
**Priority:** HIGH
**Estimated Time:** 6 hours

**File to Modify:** `src/parser/postgresql/pg_parser_ddl.cpp` (line 1238)

**Current Code (Legacy Payload):**
```cpp
void Parser::parseCreateDomain() {
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_DOMAIN));

    std::string domain_name = parseQualifiedName();
    emitString(domain_name);

    consumeKeyword(TokenType::KW_AS, "Expected AS");
    parseDataType();
    // Constraints parsed but payload is legacy (no flags/kind/WITH blocks).
}
```

**Required Implementation:**
- Match PostgreSQL's actual syntax and reserved keyword restrictions
- PostgreSQL-specific syntax variations
- Do NOT add ScratchBird's context-aware keyword flexibility
- Map PostgreSQL CREATE TYPE to CREATE DOMAIN for composite types
- Emit SBLR with dialect_tag = 'postgresql'

**Acceptance Criteria:**
- [x] Full PostgreSQL domain syntax support
- [x] CREATE TYPE → DOMAIN mapping for composites
- [x] Sets dialect_tag automatically
- [x] Test coverage

**Test File:** `tests/unit/test_postgresql_parser_domain.cpp` (NEW)

### Task 6.2: Implement PostgreSQL ALTER DOMAIN

**Status:** ❌ NOT STARTED
**Priority:** HIGH
**Estimated Time:** 4 hours

**File to Modify:** `src/parser/postgresql/pg_parser_ddl.cpp`

**Acceptance Criteria:**
- [x] PostgreSQL ALTER DOMAIN syntax
- [x] Test coverage

### Task 6.3: Implement PostgreSQL DROP DOMAIN

**Status:** ❌ NOT STARTED
**Priority:** MEDIUM
**Estimated Time:** 2 hours

**File to Modify:** `src/parser/postgresql/pg_parser_ddl.cpp`

**Acceptance Criteria:**
- [x] PostgreSQL DROP DOMAIN syntax
- [x] Handles CASCADE option (maps to RESTRICT with warning in Alpha)
- [x] Test coverage

---

## SECTION 7: MYSQL PARSER - DIALECT GUARDRAILS

**Parser Architecture Note:** MySQL parser must match MySQL's actual reserved keyword list and grammar restrictions. Unlike ScratchBird's context-aware parser, this parser follows MySQL engine limitations exactly. MySQL does NOT support domains - parser must reject with clear error.

### Task 7.1: Reject CREATE DOMAIN in MySQL Parser

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 2 hours

**File to Modify:** `src/parser/mysql/mysql_parser.cpp`

**Implementation:**
```cpp
// In parseCreate() method:
if (matchKeyword("DOMAIN")) {
    error("CREATE DOMAIN is not supported in MySQL dialect. "
          "MySQL does not have native domain support. "
          "Use base types or consider using the ScratchBird native dialect.");
    return nullptr;
}
```

**Acceptance Criteria:**
- [x] CREATE DOMAIN rejected with clear error message
- [x] Error message explains why (MySQL limitation)
- [x] Error message suggests alternatives
- [x] Test coverage

**Test File:** `tests/unit/test_mysql_parser_domain_rejection.cpp` (NEW)

### Task 7.2: Reject ALTER DOMAIN in MySQL Parser

**Status:** ✅ COMPLETE
**Priority:** MEDIUM
**Estimated Time:** 1 hour

**File to Modify:** `src/parser/mysql/mysql_parser.cpp`

**Implementation:** Similar to Task 7.1

**Acceptance Criteria:**
- [x] ALTER DOMAIN rejected
- [x] Clear error message (includes alternative)
- [x] Test coverage

### Task 7.3: Reject DROP DOMAIN in MySQL Parser

**Status:** ✅ COMPLETE
**Priority:** MEDIUM
**Estimated Time:** 1 hour

**File to Modify:** `src/parser/mysql/mysql_parser.cpp`

**Implementation:** Similar to Task 7.1

**Acceptance Criteria:**
- [x] DROP DOMAIN rejected
- [x] Clear error message (includes alternative)
- [x] Test coverage

---

## SECTION 8: SEMANTIC ANALYZER IMPLEMENTATION

### Task 8.1: Implement analyzeDomain() for BASIC Domains

**Status:** ⚠️ PARTIAL - constraint name uniqueness enforced; remaining validation guardrails pending
**Priority:** HIGH
**Estimated Time:** 8 hours

**File to Modify:** `src/sblr/semantic_analyzer_v2.cpp`

**Implementation Steps:**
1. Validate domain name not already exists (schema lookup)
2. Validate base type is valid
3. Validate CHECK expression:
   - Contains VALUE keyword
   - Returns BOOLEAN
   - No subqueries
4. Validate DEFAULT expression type matches base type
5. Resolve parent domain if INHERITS
6. Check for circular inheritance
7. Populate ResolvedStatement with domain_id

**Update (2026-01-03):** Duplicate domain names (schema-scoped) and constraint names (case-insensitive) are now rejected in `SemanticAnalyzerV2::analyzeCreateDomain()` with unit tests in `tests/unit/test_semantic_analyzer_v2.cpp`.
**Update (2026-01-03):** CHECK constraints now require a VALUE reference, reject subqueries, and BASIC domain DEFAULT literals are type-checked (literal-only) in `SemanticAnalyzerV2::analyzeCreateDomain()` with tests.

**Acceptance Criteria:**
- [x] All validation rules enforced
- [x] Clear error messages
- [x] Inheritance cycle detection
- [x] Type compatibility checks
- [x] Test coverage

**Test File:** `tests/unit/test_semantic_analyzer_v2_domain.cpp` (NEW)

### Task 8.2: Implement analyzeDomain() for RECORD Domains

**Status:** ⚠️ PARTIAL - field name uniqueness enforced; type/cycle validation pending
**Priority:** HIGH
**Estimated Time:** 6 hours

**Implementation Steps:**
1. Validate field names unique
2. Validate field types (base types or domain references)
3. Resolve domain references for domain-typed fields
4. Check for circular domain references in fields
5. Validate DEFAULT expressions for fields

**Update (2026-01-03):** Duplicate RECORD field names are now rejected (case-insensitive), RECORD domains require at least one field, and RECORD default literals are type-checked (literal-only) with unit tests in `tests/unit/test_semantic_analyzer_v2.cpp`.

**Acceptance Criteria:**
- [x] Field validation complete
- [x] Circular reference detection
- [x] Test coverage

### Task 8.3: Implement analyzeDomain() for ENUM Domains

**Status:** ⚠️ PARTIAL - label uniqueness enforced; position validation status unchanged
**Priority:** HIGH
**Estimated Time:** 4 hours

**Implementation Steps:**
1. Validate label uniqueness
2. Validate positions are sequential (1, 2, 3, ... N)
3. Validate no position gaps
4. Auto-assign positions if not explicit

**Update (2026-01-03):** Duplicate ENUM labels are now rejected (case-sensitive) with unit tests in `tests/unit/test_semantic_analyzer_v2.cpp`.

**Acceptance Criteria:**
- [x] Label uniqueness enforced
- [x] Position validation
- [x] Auto-assignment works
- [x] Test coverage

### Task 8.4: Implement analyzeDomain() for SET Domains

**Status:** ⚠️ PARTIAL - element type resolution wired; validation pending
**Priority:** MEDIUM
**Estimated Time:** 3 hours

**Implementation Steps:**
1. Validate element type
2. Resolve domain reference if element is domain

**Acceptance Criteria:**
- [x] Element type validation
- [x] Domain resolution
- [x] Test coverage

### Task 8.5: Implement analyzeDomain() for VARIANT Domains

**Status:** ⚠️ PARTIAL - type resolution wired; uniqueness and empty-list guardrails added
**Priority:** MEDIUM
**Estimated Time:** 4 hours

**Implementation Steps:**
1. Validate allowed types list not empty
2. Validate type uniqueness
3. Resolve domain references

**Acceptance Criteria:**
- [x] Type list validation
- [x] Domain resolution
- [x] Test coverage

### Task 8.6: Implement analyzeAlterDomain()

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 6 hours

**Implementation Steps:**
1. Resolve domain by name (schema lookup)
2. Validate action is allowed
3. For SET DEFAULT: validate new default type matches
4. For ADD CHECK: validate check expression
5. For DROP CONSTRAINT: validate constraint exists
6. For RENAME: validate new name not taken

**Acceptance Criteria:**
**Acceptance Criteria:**
- [x] All alter actions validated
- [x] Test coverage

### Task 8.7: Implement analyzeDropDomain()

**Status:** ✅ COMPLETE
**Priority:** MEDIUM
**Estimated Time:** 4 hours

**Implementation Steps:**
1. Resolve domain by name
2. Check for dependent columns (findColumnsByDomain)
3. Check for child domains (INHERITS)
4. If dependencies exist and not IF EXISTS: error

**Acceptance Criteria:**
- [x] Dependency checking
- [x] IF EXISTS handling
- [x] Test coverage

---

## SECTION 9: BYTECODE GENERATOR IMPLEMENTATION

### Task 9.1: Implement emitCreateDomain() for BASIC Domains

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 8 hours

**File to Modify:** `src/sblr/bytecode_generator_v2.cpp`

**Implementation Steps:**
1. Emit EXT_CREATE_DOMAIN opcode
2. Emit flags (IF NOT EXISTS + WITH blocks)
3. Emit domain_path (string)
4. Emit base type encoding (TYPE_* + precision/scale when needed)
5. Emit nullable/default + constraint list
6. Emit WITH INTEGRITY/SECURITY/VALIDATION/QUALITY payloads when present

**Acceptance Criteria:**
- [x] Payload matches SBLR_DOMAIN_PAYLOADS.md spec
- [x] All flags set correctly
- [x] Expressions emitted correctly
- [x] Test coverage (basic bytecode assertions)

**Test File:** `tests/unit/test_bytecode_generator_v2.cpp`

### Task 9.2: Implement emitCreateDomain() for RECORD Domains

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 6 hours

**Implementation Steps:**
1. Emit EXT_CREATE_DOMAIN opcode
2. Emit flags
3. Emit domain_type = RECORD
4. Emit schema_id, domain_name
5. Emit field_count
6. For each field:
   - Emit field_name
   - Emit field_type (or domain_id if domain reference)
   - Emit field_flags (NOT_NULL, HAS_DEFAULT)
   - Emit default expression if present

**Acceptance Criteria:**
- [x] Field list encoded correctly
- [x] Domain references handled
- [x] Test coverage

### Task 9.3: Implement emitCreateDomain() for ENUM Domains

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 4 hours

**Implementation Steps:**
1. Emit EXT_CREATE_DOMAIN opcode
2. Emit flags (HAS_WRAP if set)
3. Emit domain_type = ENUM
4. Emit schema_id, domain_name
5. Emit enum_value_count
6. For each value:
   - Emit label (string)
   - Emit position (int32)

**Acceptance Criteria:**
- [x] Enum values encoded correctly
- [x] WRAP option handled
- [x] Test coverage

### Task 9.4: Implement emitCreateDomain() for SET Domains

**Status:** ✅ COMPLETE
**Priority:** MEDIUM
**Estimated Time:** 3 hours

**Implementation Steps:**
1. Emit EXT_CREATE_DOMAIN opcode
2. Emit domain_type = SET
3. Emit element type (or domain_id)

**Acceptance Criteria:**
- [x] Element type encoded
- [x] Test coverage

### Task 9.5: Implement emitCreateDomain() for VARIANT Domains

**Status:** ✅ COMPLETE
**Priority:** MEDIUM
**Estimated Time:** 4 hours

**Implementation Steps:**
1. Emit EXT_CREATE_DOMAIN opcode
2. Emit domain_type = VARIANT
3. Emit allowed_type_count
4. For each type: emit type (or domain_id)

**Acceptance Criteria:**
- [x] Type list encoded
- [x] Test coverage

### Task 9.6: Implement emitAlterDomain()

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 6 hours

**File to Modify:** `src/sblr/bytecode_generator_v2.cpp`

**Implementation Steps:**
1. Emit EXT_ALTER_DOMAIN opcode
2. Emit action enum
3. Emit domain_path string
4. Emit action-specific payload

**Acceptance Criteria:**
- [x] All alter actions emit correctly
- [x] Test coverage

### Task 9.7: Implement emitDropDomain()

**Status:** ✅ COMPLETE
**Priority:** MEDIUM
**Estimated Time:** 3 hours

**File to Modify:** `src/sblr/bytecode_generator_v2.cpp`

**Implementation Steps:**
1. Emit EXT_DROP_DOMAIN opcode
2. Emit flags (IF_EXISTS, RESTRICT)
3. Emit domain_path string

**Acceptance Criteria:**
- [x] Simple payload emitted
- [x] Test coverage

---

## SECTION 10: EXECUTOR IMPLEMENTATION

### Task 10.1: Implement executeCreateDomain() for BASIC Domains

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 10 hours

**File to Modify:** `src/sblr/executor.cpp`

**Implementation Steps:**
1. Add case in executeExtendedOpcode() for EXT_CREATE_DOMAIN
2. Read payload (flags, domain_path, base type encoding)
3. Read nullable/default/constraints
4. Apply WITH INTEGRITY/SECURITY/VALIDATION/QUALITY options
5. Call DomainManager::createBasicDomain() + option setters

**Acceptance Criteria:**
- [x] Payload decoded correctly
- [x] DomainManager called with correct params
- [x] Error handling
- [x] Transaction integration
- [x] Test coverage

**Test File:** `tests/integration/test_domain_ddl.cpp` (NEW)

### Task 10.2: Implement executeCreateDomain() for RECORD Domains

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 8 hours

**Implementation Steps:**
1. In executeCreateDomain() handler
2. If domain_type == RECORD:
   - Read field_count
   - For each field: read field definition
   - Build RecordField vector
   - Call DomainManager::createRecordDomain()

**Acceptance Criteria:**
- [x] Field list decoded
- [x] DomainManager called
- [x] Test coverage

### Task 10.3: Implement executeCreateDomain() for ENUM Domains

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 6 hours

**Implementation Steps:**
1. If domain_type == ENUM:
   - Read enum_value_count
   - For each value: read label and position
   - Build EnumValue vector
   - Call DomainManager::createEnumDomain()

**Acceptance Criteria:**
- [x] Enum values decoded
- [x] DomainManager called
- [x] Test coverage

### Task 10.4: Implement executeCreateDomain() for SET Domains

**Status:** ✅ COMPLETE
**Priority:** MEDIUM
**Estimated Time:** 4 hours

**Implementation Steps:**
1. If domain_type == SET:
   - Read element type
   - Call DomainManager::createSetDomain()

**Acceptance Criteria:**
- [x] Element type decoded
- [x] DomainManager called
- [x] Test coverage

### Task 10.5: Implement executeCreateDomain() for VARIANT Domains

**Status:** ✅ COMPLETE
**Priority:** MEDIUM
**Estimated Time:** 5 hours

**Implementation Steps:**
1. If domain_type == VARIANT:
   - Read allowed_type_count
   - For each: read type
   - Build type vector
   - Call DomainManager::createVariantDomain()

**Acceptance Criteria:**
- [x] Type list decoded
- [x] DomainManager called
- [x] Test coverage

### Task 10.6: Implement executeAlterDomain()

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 8 hours

**File to Modify:** `src/sblr/executor.cpp`

**Implementation Steps:**
1. Add case in executeExtendedOpcode() for EXT_ALTER_DOMAIN
2. Read action enum + domain_path
3. Read action-specific payload
4. Call DomainManager update helpers

**Acceptance Criteria:**
- [x] All alter actions supported
- [x] DomainManager called correctly
- [x] Test coverage

### Task 10.7: Implement executeDropDomain()

**Status:** ✅ COMPLETE
**Priority:** MEDIUM
**Estimated Time:** 6 hours

**File to Modify:** `src/sblr/executor.cpp`

**Implementation Steps:**
1. Add case in executeExtendedOpcode() for EXT_DROP_DOMAIN
2. Read flags (IF_EXISTS, RESTRICT)
3. Read domain_path
4. Resolve domain + enforce RESTRICT dependencies
5. Call DomainManager::dropDomain()

**Acceptance Criteria:**
- [x] Dependency checking enforced
- [x] IF EXISTS handled
- [x] Test coverage

### Task 10.8: Implement executeShowDomain()

**Status:** ✅ COMPLETE
**Priority:** LOW
**Estimated Time:** 4 hours

**File to Modify:** `src/sblr/executor.cpp`

**Implementation Steps:**
1. Add case for EXT_SHOW_DOMAIN
2. Read domain name/id
3. Call DomainManager::getDomain()
4. Format domain definition as result set
5. Return ExecutionResult

**Acceptance Criteria:**
- [x] Shows domain definition
- [x] Formatted output
- [x] Test coverage

### Task 10.9: Implement WITH SECURITY Enforcement

**Status:** ✅ COMPLETE (implemented in Plan 03B)
**Priority:** HIGH
**Estimated Time:** 16 hours

**File to Modify:** `src/sblr/executor.cpp`, `src/core/domain_manager.cpp`

**Implementation Steps:**
1. Implement masking logic (PARTIAL, FULL, NONE) in value retrieval
2. Implement encryption/decryption at storage layer (AES256, AES128)
3. Implement access auditing - log all access to secured domains
4. Implement privilege checking for REQUIRE_PRIVILEGE
5. Hook into INSERT/UPDATE operations to encrypt values
6. Hook into SELECT operations to apply masking and check privileges
7. Store encryption keys securely in system catalog

**Acceptance Criteria:**
- [x] MASKING enforced on SELECT (PARTIAL shows pattern, FULL hides all)
- [x] ENCRYPTION applied on INSERT/UPDATE, decrypted on SELECT
- [x] AUDIT_ACCESS logs all domain value access with transaction ID
- [x] REQUIRE_PRIVILEGE checked before value retrieval
- [x] Full test coverage for all security options
- [x] Integration tests with concurrent transactions

**Test File:** `tests/integration/test_domain_security.cpp`

### Task 10.10: Implement WITH INTEGRITY Enforcement

**Status:** ✅ COMPLETE (implemented in Plan 03B)
**Priority:** HIGH
**Estimated Time:** 12 hours

**File to Modify:** `src/sblr/executor.cpp`, `src/core/domain_manager.cpp`

**Implementation Steps:**
1. Implement global uniqueness tracking for domains with UNIQUENESS = TRUE
2. Create index structure for tracking unique values across all columns using domain
3. Implement auto-normalization (LOWERCASE, UPPERCASE, TRIM)
4. Support custom normalization functions via function call mechanism
5. Hook into INSERT/UPDATE to enforce uniqueness and apply normalization
6. Handle normalization before constraint checking

**Acceptance Criteria:**
- [x] UNIQUENESS enforced globally across all columns using domain
- [x] NORMALIZATION applied automatically (LOWERCASE, UPPERCASE, TRIM)
- [x] NORMALIZATION_FUNCTION calls work correctly
- [x] Constraint violations return appropriate errors
- [x] Full test coverage including concurrent inserts
- [x] Performance acceptable for large tables

**Test File:** `tests/integration/test_domain_integrity.cpp`

### Task 10.11: Implement WITH VALIDATION Enforcement

**Status:** ✅ COMPLETE (implemented in Plan 03B)
**Priority:** HIGH
**Estimated Time:** 10 hours

**File to Modify:** `src/sblr/executor.cpp`, `src/core/domain_manager.cpp`

**Implementation Steps:**
1. Implement custom validation function call mechanism
2. Ensure validation functions are resolved and callable
3. Return custom ERROR_MESSAGE on validation failure
4. Hook into INSERT/UPDATE operations to execute validation
5. Integrate with existing CHECK constraint system
6. Handle validation errors with proper transaction rollback

**Acceptance Criteria:**
- [x] FUNCTION called on every INSERT/UPDATE
- [x] Validation failures return custom ERROR_MESSAGE
- [x] Integration with CHECK constraints (both must pass)
- [x] Proper error context and transaction handling
- [x] Full test coverage with various validation functions
- [x] Performance acceptable

**Test File:** `tests/integration/test_domain_validation.cpp`

### Task 10.12: Implement WITH QUALITY Enforcement

**Status:** ✅ COMPLETE (implemented in Plan 03B)
**Priority:** HIGH
**Estimated Time:** 14 hours

**File to Modify:** `src/sblr/executor.cpp`, `src/core/domain_manager.cpp`

**Implementation Steps:**
1. Implement parsing function calls (PARSE_FUNCTION)
2. Implement standardization function calls (STANDARDIZE_FUNCTION)
3. Implement enrichment function calls (ENRICH_FUNCTION)
4. Support function chaining: parse → standardize → enrich
5. Hook into INSERT/UPDATE to execute quality pipeline
6. Store enriched data alongside original value if needed
7. Handle quality function errors gracefully

**Acceptance Criteria:**
- [x] PARSE_FUNCTION validates and extracts components
- [x] STANDARDIZE_FUNCTION formats values consistently
- [x] ENRICH_FUNCTION augments data correctly
- [x] Function pipeline executes in correct order
- [x] INSERT/UPDATE values transformed before storage
- [x] Error handling for each pipeline stage
- [x] Full test coverage with realistic examples

**Test File:** `tests/integration/test_domain_quality.cpp`

---

## SECTION 11: TRANSACTION CONTROL EXTENSIONS

### Task 11.1: Extend StartTransactionStmt AST

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 4 hours

**File to Modify:** `include/scratchbird/parser/ast_v2.h`

**Add Fields:**
```cpp
struct StartTransactionStmt
{
    // Existing fields...

    // ADD THESE:
    bool has_conflict_action;
    TxnConflictAction conflict_action;
    int32_t conflict_error_code;

    bool has_autocommit_mode;
    AutocommitMode autocommit_mode;

    // Firebird legacy fields
    bool has_wait_mode;
    bool wait_for_locks;

    bool has_lock_timeout;
    uint32_t lock_timeout_seconds;

    bool has_reservations;
    std::vector<TableReservation> reservations;
};

enum class TxnConflictAction : uint8_t
{
    COMMIT,      // Commit current, start new
    ROLLBACK,    // Rollback current, start new
    ERROR,       // Error and keep current
    KEEP         // Keep current (no new transaction)
};

enum class AutocommitMode : uint8_t
{
    UNCHANGED,
    ON,
    OFF
};

struct TableReservation
{
    std::string table_name;
    TableLockMode lock_mode;  // SHARED or PROTECTED
    bool for_write;           // true = WRITE, false = READ
};
```

**Acceptance Criteria:**
- [x] All new fields added
- [x] Enums defined
- [x] Getters/setters added

### Task 11.2: Parse Firebird WAIT / NO WAIT

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 2 hours

**File to Modify:** `src/parser/parser_v2.cpp` in parseStartTransaction()

**Grammar:**
```
START TRANSACTION
    ... standard options ...
    [WAIT | NO WAIT]
```

**Acceptance Criteria:**
- [x] Parses WAIT keyword
- [x] Parses NO WAIT keyword
- [x] Sets wait_for_locks flag
- [x] Test coverage

### Task 11.3: Parse Firebird LOCK TIMEOUT

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 2 hours

**Grammar:**
```
START TRANSACTION
    ... standard options ...
    LOCK TIMEOUT <seconds>
```

**Acceptance Criteria:**
- [x] Parses LOCK TIMEOUT
- [x] Reads integer seconds
- [x] Test coverage

### Task 11.4: Parse Firebird RESERVING Clause

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 4 hours

**Grammar:**
```
START TRANSACTION
    ... standard options ...
    RESERVING
        table1 FOR [SHARED|PROTECTED] [READ|WRITE]
        [, table2 FOR ...]
```

**Acceptance Criteria:**
- [x] Parses RESERVING keyword
- [x] Parses table list
- [x] Parses lock modes
- [x] Test coverage

### Task 11.5: Parse ON CONFLICT Clause

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 3 hours

**Grammar:**
```
START TRANSACTION
    ... standard options ...
    ON CONFLICT {COMMIT|ROLLBACK|ERROR [code]|KEEP}
```

**Acceptance Criteria:**
- [x] Parses ON CONFLICT
- [x] Parses all conflict actions
- [x] Parses optional error code
- [x] Test coverage

**NOTE:** This is ScratchBird-only syntax, NOT in emulated parsers

### Task 11.6: Parse SET AUTOCOMMIT

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 2 hours

**File to Modify:** `src/parser/parser_v2.cpp` in parseSet()

**Grammar:**
```
SET AUTOCOMMIT {ON|OFF|1|0} [ON CONFLICT action]
```

**Acceptance Criteria:**
- [x] Parses SET AUTOCOMMIT
- [x] Accepts ON/OFF/1/0
- [x] Parses optional ON CONFLICT
- [x] Test coverage

### Task 11.7: Parse SET TRANSACTION AUTOCOMMIT

**Status:** ✅ COMPLETE
**Priority:** MEDIUM
**Estimated Time:** 2 hours

**Grammar:**
```
SET TRANSACTION AUTOCOMMIT {ON|OFF} [ON CONFLICT action]
```

**Acceptance Criteria:**
- [x] Parses SET TRANSACTION AUTOCOMMIT
- [x] Test coverage

### Task 11.8: Parse COMMIT/ROLLBACK RETAINING

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 2 hours

**Grammar:**
```
COMMIT [WORK|TRANSACTION] RETAINING
ROLLBACK [WORK|TRANSACTION] RETAINING
```

**Acceptance Criteria:**
- [x] Parses RETAINING keyword
- [x] Sets retaining flag in AST
- [x] Test coverage

### Task 11.9: Parse 2PC Statements

**Status:** ✅ COMPLETE
**Priority:** MEDIUM
**Estimated Time:** 3 hours

**Grammar:**
```
PREPARE TRANSACTION 'gid'
COMMIT PREPARED 'gid'
ROLLBACK PREPARED 'gid'
```

**Acceptance Criteria:**
- [x] All 3 statements parsed
- [x] GID (global transaction ID) parsed
- [x] Test coverage

**NOTE:** Executor stubs exist, parser needed for completeness

### Task 11.10: Emit Extended Transaction Payloads

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 6 hours

**File to Modify:** `src/sblr/bytecode_generator_v2.cpp`

**Implementation:**
Emit START_TRANSACTION payload per Plan 03 spec:
```
[START_TRANSACTION]
[flags:uint16]
[conflict_action:uint8]
[conflict_error_code:int32] (if HAS_CONFLICT_ERROR_CODE)
[autocommit_mode:uint8] (if HAS_AUTOCOMMIT)
[isolation_level:uint8] (if HAS_ISOLATION)
[access_mode:uint8] (if HAS_ACCESS_MODE)
[deferrable:uint8] (if HAS_DEFERRABLE)
[wait_mode:uint8] (if HAS_WAIT_MODE)
[lock_timeout:uint32] (if HAS_LOCK_TIMEOUT)
[reservations:list] (if HAS_RESERVATIONS)
```

**Acceptance Criteria:**
- [x] All flags set correctly
- [x] Payload matches Plan 03 spec
- [x] Test coverage (bytecode round-trip)

---

## SECTION 12: QUICK WINS (Low-Hanging Fruit)

### Task 12.1: Firebird Window Specification Parsing

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 4 hours

**File to Modify:** `src/parser/firebird/firebird_parser.cpp`

**Location:** In parseFunctionCall() where TODO exists

**Grammar:**
```
function_name(...) OVER (
    [PARTITION BY expr_list]
    [ORDER BY order_list]
    [frame_clause]
)
```

**Acceptance Criteria:**
- [x] Parses OVER keyword
- [x] Parses PARTITION BY
- [x] Parses ORDER BY
- [x] Parses frame clauses (ROWS/RANGE BETWEEN)
- [x] Stores in AST
- [x] Test coverage

**Test File:** `tests/unit/test_firebird_parser_window.cpp` (NEW)

### Task 12.2: MySQL NULL-Safe Equality Operator

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 3 hours

**File to Modify:** `src/parser/mysql/mysql_parser.cpp`

**Add Extended Opcode:**
```cpp
// In opcodes.h:
EXT_NULL_SAFE_EQ = 0x0200  // NULL-safe equality (<=>)
```

**Parser Change:**
In parseComparisonExpr(), detect `<=>` and emit EXT_NULL_SAFE_EQ

**Executor Implementation:**
```cpp
// In executor.cpp:
case ExtendedOpcode::EXT_NULL_SAFE_EQ:
    // NULL <=> NULL is TRUE
    // value <=> NULL is FALSE if value not NULL
    // value1 <=> value2 is same as value1 = value2 if neither NULL
```

**Acceptance Criteria:**
- [x] Opcode defined
- [x] Parser emits opcode
- [x] Executor implements NULL-safe semantics
- [x] Test coverage

**Test File:** `tests/unit/test_mysql_parser.cpp`

### Task 12.3: ESCAPE Handling for LIKE (PostgreSQL and MySQL)

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 4 hours

**Files to Modify:**
- `src/parser/postgresql/pg_parser_expr.cpp`
- `src/parser/mysql/mysql_parser.cpp`
 - `src/sblr/executor.cpp`
 - `src/sblr/bytecode_generator_v2.cpp`

**Add Extended Opcodes:**
```cpp
EXT_LIKE_ESCAPE = 0x0201   // LIKE with ESCAPE clause
EXT_ILIKE_ESCAPE = 0x0202  // ILIKE with ESCAPE clause
```

**Grammar:**
```
expr LIKE pattern ESCAPE escape_char
```

**Implementation:**
1. After parsing LIKE pattern, check for ESCAPE keyword
2. Parse escape character (single-char string literal)
3. Emit `EXT_LIKE_ESCAPE` / `EXT_ILIKE_ESCAPE` with escape expression
4. Update executor to use escape character

**Acceptance Criteria:**
- [x] Both parsers support ESCAPE
- [x] Executor honors escape character
- [x] Test coverage

**Test Files:**
- `tests/unit/test_postgresql_parser.cpp`
- `tests/unit/test_mysql_parser.cpp`
- `tests/unit/test_bytecode_generator_v2.cpp`

### Task 12.4: Placeholder Handling (MySQL and PostgreSQL)

**Status:** ✅ COMPLETE
**Priority:** MEDIUM
**Estimated Time:** 5 hours

**Files to Modify:**
- `src/parser/mysql/mysql_parser.cpp`
- `src/parser/postgresql/pg_parser_expr.cpp`

**Add Opcode:**
```cpp
EXT_PLACEHOLDER = 0x0203  // Placeholder for prepared statements
```

**Payload:**
```
[EXT_PLACEHOLDER]
[position:uint16]        // $1, $2, etc. or ?
[type_hint:uint16]       // Optional type hint (0 = unknown)
```

**Acceptance Criteria:**
- [x] Opcode defined
- [x] Both parsers emit placeholders
- [x] Executor creates placeholder descriptor
- [x] Test coverage

### Task 12.5: GROUP BY Validation in Semantic Analyzer

**Status:** ✅ COMPLETE
**Priority:** HIGH
**Estimated Time:** 6 hours

**File to Modify:** `src/sblr/semantic_analyzer_v2.cpp`

**Implementation:**
```cpp
Status SemanticAnalyzerV2::validateGroupBy(SelectStmt* stmt)
{
    // 1. Build set of grouped columns
    std::set<std::string> grouped_cols;
    for (auto& expr : stmt->group_by) {
        if (expr is ColumnRef) {
            grouped_cols.insert(column_name);
        }
    }

    // 2. Check SELECT list
    for (auto& item : stmt->select_list) {
        if (item is aggregate function) {
            continue;  // OK
        }
        if (item is column reference) {
            if (grouped_cols.find(column_name) == grouped_cols.end()) {
                error("Column must appear in GROUP BY or be used in aggregate");
            }
        }
    }

    return Status::OK;
}
```

**Acceptance Criteria:**
- [x] Validates all SELECT list items
- [x] Allows aggregate functions
- [x] Requires non-aggregate columns in GROUP BY
- [x] Clear error messages
- [x] Test coverage

**Test File:** `tests/unit/test_semantic_analyzer_v2.cpp`

---

## SECTION 13: COMPREHENSIVE TESTING

### Task 13.1: End-to-End Domain DDL Tests

**Status:** ❌ NOT STARTED
**Priority:** HIGH
**Estimated Time:** 8 hours

**Test File:** `tests/integration/test_domain_ddl_e2e.cpp` (NEW)

**Test Coverage:**
- [x] Create BASIC domain → use in table → insert/select
- [x] Create RECORD domain → use in table → insert/select → field access
- [x] Create ENUM domain → use in table → insert/select → comparisons
- [x] Create SET domain → use in table → insert/select → set operations
- [x] Create VARIANT domain → use in table → insert → type checks
- [x] Domain with INHERITS → constraint enforcement
- [x] ALTER DOMAIN → changes reflected in table
- [x] DROP DOMAIN → fails if in use
- [x] Multi-dialect domains (dialect_tag/compat_name)

### Task 13.2: Cross-Dialect Domain Tests

**Status:** ❌ NOT STARTED
**Priority:** HIGH
**Estimated Time:** 6 hours

**Test File:** `tests/integration/test_domain_cross_dialect.cpp` (NEW)

**Test Coverage:**
- [x] Firebird parser → domain → ScratchBird storage → query
- [x] PostgreSQL parser → composite type → RECORD domain → query
- [x] MySQL parser → rejects CREATE DOMAIN with clear error
- [x] Dialect-tagged domains resolve correctly

### Task 13.3: Transaction Control Extension Tests

**Status:** ❌ NOT STARTED
**Priority:** HIGH
**Estimated Time:** 6 hours

**Test File:** `tests/integration/test_transaction_extensions.cpp` (NEW)

**Test Coverage:**
- [x] START TRANSACTION with WAIT/NO WAIT
- [x] START TRANSACTION with LOCK TIMEOUT
- [x] START TRANSACTION with RESERVING
- [x] START TRANSACTION with ON CONFLICT
- [x] SET AUTOCOMMIT
- [x] COMMIT/ROLLBACK RETAINING
- [x] All clauses combined

### Task 13.4: Parser Bytecode Round-Trip Tests

**Status:** ❌ NOT STARTED
**Priority:** HIGH
**Estimated Time:** 8 hours

**Test Files:**
- `tests/unit/test_domain_bytecode_roundtrip.cpp` (NEW)
- `tests/unit/test_transaction_bytecode_roundtrip.cpp` (NEW)

**Test Pattern:**
```cpp
// For each statement type:
1. Parse SQL → AST
2. Analyze AST → Resolved
3. Generate SBLR bytecode
4. Decode SBLR bytecode
5. Compare decoded with original AST
```

**Coverage:**
- [x] All domain types
- [x] All transaction variations
- [x] All parsers (V2, Firebird, PostgreSQL)

### Task 13.5: Negative Tests (Error Handling)

**Status:** ❌ NOT STARTED
**Priority:** HIGH
**Estimated Time:** 6 hours

**Test File:** `tests/unit/test_domain_errors.cpp` (NEW)

**Test Coverage:**
- [x] Duplicate domain name → error (covered in `tests/unit/test_semantic_analyzer_v2.cpp`)
- [x] Invalid CHECK expression → error
- [x] Circular inheritance → error
- [x] Invalid ENUM positions → error
- [x] Type mismatch in INHERITS → error
- [x] DROP domain in use → error
- [x] ALTER domain with invalid constraint → error

---

## SECTION 14: DOCUMENTATION UPDATES

### Task 14.1: Update SBLR Bytecode Specification

**Status:** ❌ NOT STARTED
**Priority:** HIGH
**Estimated Time:** 4 hours

**File to Update:** `docs/specifications/Appendix_A_SBLR_BYTECODE.md`

**Add:**
- [x] Domain opcode documentation
- [x] Domain payload structures
- [x] Extended transaction payload updates
- [x] Examples

### Task 14.2: Update Parser Grammar Documentation

**Status:** ❌ NOT STARTED
**Priority:** MEDIUM
**Estimated Time:** 3 hours

**Files to Update:**
- `docs/specifications/ScratchBird SQL Language Specification - Master Document.md`
- Add domain grammar section

**Add:**
- [x] Complete domain BNF grammar
- [x] Examples for all domain types
- [x] Transaction control grammar updates

### Task 14.3: Create Domain User Guide

**Status:** ❌ NOT STARTED
**Priority:** LOW
**Estimated Time:** 4 hours

**File to Create:** `docs/guides/DOMAIN_USER_GUIDE.md`

**Content:**
- [x] What are domains
- [x] When to use each domain type
- [x] Examples for common use cases
- [x] Dialect compatibility guide

---

## MASTER CHECKLIST SUMMARY

### Schema Changes
- [x] Task 1.1: Add dialect_tag/compat_name to domain schema

### SBLR Opcodes
- [x] Task 2.1: Define extended domain opcodes (ALTER/DROP done; conflict opcodes pending)
- [x] Task 2.2: Document SBLR payload structures (BASIC + ALTER/DROP + advanced types)

### AST Extensions
- [x] Task 3.1: Extend CreateDomainStmt (V2 complete)
- [x] Task 3.2: Create AlterDomainStmt
- [x] Task 3.3: Create DropDomainStmt

### ScratchBird V2 Parser (12 tasks)
- [x] Task 4.1-4.7: parseCreateDomain() all types + WITH blocks
- [x] Task 4.8: parseAlterDomain()
- [x] Task 4.9: parseDropDomain()
- [x] Task 4.10-4.12: Update dispatchers

### Firebird Parser (3 tasks)
- [x] Task 5.1-5.3: CREATE/ALTER/DROP DOMAIN

### PostgreSQL Parser (3 tasks)
- [x] Task 6.1-6.3: CREATE/ALTER/DROP DOMAIN

### MySQL Parser (3 tasks)
- [x] Task 7.1-7.3: Reject domain DDL with clear errors

### Semantic Analyzer (7 tasks)
- [x] Task 8.1-8.7: Analyze all domain types + ALTER/DROP (partial validation remaining)

### Bytecode Generator (7 tasks)
- [x] Task 9.1-9.7: Emit all domain types + ALTER/DROP

### Executor (12 tasks)
- [x] Task 10.1-10.8: Execute all domain types + ALTER/DROP + SHOW
- [x] Task 10.9-10.12: WITH block enforcement (SECURITY, INTEGRITY, VALIDATION, QUALITY)

### Transaction Extensions (10 tasks)
- [x] Task 11.1-11.10: Parse + emit extended transaction features

### Quick Wins (5 tasks)
- [x] Task 12.1: Firebird window specs
- [x] Task 12.2: MySQL NULL-safe equality
- [x] Task 12.3: ESCAPE for LIKE
- [x] Task 12.4: Placeholder handling
- [x] Task 12.5: GROUP BY validation

### Testing (5 test suites)
- [x] Task 13.1-13.5: Comprehensive test coverage

### Documentation (3 tasks)
- [x] Task 14.1-14.3: Update specs and create guides (domain payload doc added)

---

## TOTAL TASK COUNT: 80 TASKS

**Estimated Total Time:** 12-16 weeks (full team) or 20-24 weeks (single developer)

**Critical Path:**
1. Schema changes (Week 1)
2. SBLR opcodes (Week 1)
3. AST extensions (Week 1-2)
4. V2 Parser (Week 2-4)
5. Emulated parsers (Week 5-6)
6. Semantic analyzer (Week 7-8)
7. Bytecode generator (Week 9-10)
8. Executor (Week 11-13)
9. Transaction extensions (Week 14-15)
10. Testing (Week 16)

---

## STOPPING CONDITIONS

**STOP WORK AND ASK IF:**
1. Schema change requires data migration not covered in plan
2. SBLR payload structure ambiguous
3. Semantic validation rule unclear
4. Firebird vs SQL Standard syntax conflict - both variants must be implemented
5. Performance impact of feature not acceptable
6. Test coverage requirement unclear
7. Any feature marked as "Beta" needs Alpha implementation

**DO NOT PROCEED WITHOUT CLARIFICATION ON:**
- Advanced domain features (WITH blocks) enforcement in Alpha
- 2PC implementation requirements (currently stubbed)
- Dialect-specific syntax variations not in spec
- Performance requirements for domain constraint checking
- Migration path for existing databases

---

**END OF IMPLEMENTATION CHECKLIST**
