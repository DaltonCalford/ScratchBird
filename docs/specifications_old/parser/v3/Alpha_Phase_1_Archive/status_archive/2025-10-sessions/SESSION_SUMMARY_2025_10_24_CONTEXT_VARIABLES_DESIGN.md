# Session Summary: Context Variables Design (Firebird Chapter 12)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: 2025-10-24
**Session Type**: Design & Research
**Status**: COMPLETE
**Duration**: ~3-4 hours
**Estimated Implementation**: 60-86 hours

---

## Executive Summary

This session completed the design for ScratchBird's context variables following Firebird's Chapter 12 pattern. Context variables provide SQL-level access to connection state, transaction metadata, date/time functions, and row identity.

**Key Deliverables**:
1. ✅ Comprehensive context variables design document (~1,200 lines)
2. ✅ Updated row identity design (v2.1)
3. ✅ Updated PROJECT_CONTEXT.md with new priorities
4. ✅ Firebird Chapter 12 research complete

---

## What Was Done

### 1. Firebird Chapter 12 Research

**Task**: Research Firebird's Context Variables pattern from Chapter 12 of FirebirdReferenceDocument.md

**Results**:
- Read 500+ lines of Firebird documentation (lines 26712-27509)
- Identified 24 Firebird context variables
- Documented key patterns:
  - No parentheses syntax (e.g., `CURRENT_DATE`, not `CURRENT_DATE()`)
  - Optional precision for time types (e.g., `CURRENT_TIME(3)`)
  - PSQL stability (constant within outermost module)
  - String literal mnemonics (`'NOW'`, `'TODAY'`, `'YESTERDAY'`, `'TOMORROW'`)
  - Progressing values in PSQL using string literals

**Firebird Context Variables Identified**:
1. `CURRENT_CONNECTION` - Connection ID (BIGINT)
2. `CURRENT_USER` / `USER` - Username (VARCHAR(63))
3. `CURRENT_ROLE` - Current role (VARCHAR(63))
4. `CURRENT_TRANSACTION` - Transaction ID (BIGINT)
5. `CURRENT_DATE` - Current date (DATE)
6. `CURRENT_TIME` - Current time with TZ (TIME WITH TIME ZONE)
7. `CURRENT_TIMESTAMP` - Current timestamp with TZ (TIMESTAMP WITH TIME ZONE)
8. `LOCALTIME` - Current time without TZ (TIME WITHOUT TIME ZONE)
9. `LOCALTIMESTAMP` - Current timestamp without TZ (TIMESTAMP WITHOUT TIME ZONE)
10. `INSERTING` - Trigger fired by INSERT (BOOLEAN)
11. `UPDATING` - Trigger fired by UPDATE (BOOLEAN)
12. `DELETING` - Trigger fired by DELETE (BOOLEAN)
13. `NEW` - New row values in trigger (Record)
14. `OLD` - Old row values in trigger (Record)
15. `'NOW'` - Current timestamp mnemonic (String literal)
16. `'TODAY'` - Current date mnemonic (String literal)
17. `'YESTERDAY'` - Yesterday's date mnemonic (String literal)
18. `'TOMORROW'` - Tomorrow's date mnemonic (String literal)
19. `RESETTING` - Session reset in progress (BOOLEAN)
20. `ROW_COUNT` - Affected rows count (INTEGER)
21. `GDSCODE` - Firebird error code (INTEGER)
22. `SQLSTATE` - SQL state code (CHAR(5))
23. `SQLCODE` - DEPRECATED SQL code (INTEGER)
24. `USER` - Alias for CURRENT_USER

---

### 2. Context Variables Design Document

**File Created**: `/docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/ALPHA_CONTEXT_VARIABLES_DESIGN.md` (~1,200 lines)

**Contents**:

#### Section 1: Context Variables Overview
- Firebird pattern characteristics
- ScratchBird extensions (3 new variables)

#### Section 2: Connection Context Variables
- `CURRENT_CONNECTION`: Unique connection ID
- `CURRENT_USER` / `USER`: Current username
- `CURRENT_ROLE`: Current role (stub for ALPHA)

#### Section 3: Transaction Context Variables
- `CURRENT_TRANSACTION`: Current transaction ID (64-bit)
- `rdb$xact_id` (NEW): Last-modifying transaction ID visible to current transaction

#### Section 4: Row Identity Context Variables
- `sdb$key` (NEW): Physical row identifier (hex string format: `TTTT-PPPPPPPPPPPP-SSSS`)
- `rdb$row_uuid` (NEW): Permanent UUID v7 row identifier

**Key Design Decisions**:
- `sdb$key` derived from TID (0 bytes overhead)
- `rdb$row_uuid` adds 16 bytes to TupleHeader (44 → 60 bytes)
- Both hidden from `SELECT *` unless explicitly addressed
- Hash indexes supported on `sdb$key` for O(1) lookups

#### Section 5: Date/Time Context Variables
- `CURRENT_DATE`, `CURRENT_TIME`, `CURRENT_TIMESTAMP`
- `LOCALTIME`, `LOCALTIMESTAMP` (without time zone)
- String literals: `'NOW'`, `'TODAY'`, `'YESTERDAY'`, `'TOMORROW'`

**PSQL Caching Mechanism**:
```cpp
class ConnectionContext {
    mutable std::optional<Date> current_date_cache_;
    mutable std::optional<TimeWithTZ> current_time_cache_;
    mutable std::optional<TimestampWithTZ> current_timestamp_cache_;
    uint32_t psql_depth_ = 0;

    void enterPsqlModule() {
        if (psql_depth_++ == 0) {
            resetPsqlCache();
        }
    }

    void exitPsqlModule() {
        if (--psql_depth_ == 0) {
            // Cache cleared on exit
        }
    }
};
```

#### Section 6: Security Context Variables
- `CURRENT_USER` / `USER`
- `CURRENT_ROLE` (stub for ALPHA)

#### Section 7: Trigger Context Variables
- `INSERTING`, `UPDATING`, `DELETING` (boolean flags)
- `NEW`, `OLD` (record access)

**Trigger Context Tracking**:
```cpp
class ConnectionContext {
    bool in_trigger_ = false;
    TriggerOperation trigger_op_ = TriggerOperation::NONE;
    const Tuple* new_tuple_ = nullptr;
    const Tuple* old_tuple_ = nullptr;

    void enterTrigger(TriggerOperation op) {
        in_trigger_ = true;
        trigger_op_ = op;
    }
};
```

#### Section 8: Error Handling Context Variables
- `GDSCODE`: Firebird error code
- `SQLSTATE`: SQL-compliant status code (CHAR(5))
- `ROW_COUNT`: Affected rows count

#### Section 9: Implementation Plan (6 Phases, 16-22 hours)

**Phase 1**: Core Infrastructure (4-6 hours)
- ContextVar enum
- Lexer/Parser integration
- Semantic analysis

**Phase 2**: Bytecode and Execution (3-5 hours)
- Bytecode generation (`OP_PUSH_CONTEXT_VAR`)
- Executor implementation

**Phase 3**: Row Identity Context Variables (4-6 hours)
- `sdb$key` implementation
- `rdb$row_uuid` implementation
- See ALPHA_ROW_IDENTITY_ENHANCED.md for details

**Phase 4**: Date/Time Context Variables (3-4 hours)
- PSQL caching mechanism
- String literal mnemonics (`'NOW'`, `'TODAY'`, etc.)

**Phase 5**: Trigger Context Variables (2-3 hours)
- Trigger context tracking
- NEW/OLD record access

**Phase 6**: Error Handling Context Variables (2-3 hours)
- Error context tracking
- ROW_COUNT tracking

#### Section 10: Testing Strategy (35 unit tests)
- Connection Context: 5 tests
- Transaction Context: 5 tests
- Row Identity: 8 tests
- Date/Time: 10 tests
- Trigger Context: 5 tests
- Error Handling: 2 tests

---

### 3. Updated Row Identity Design (v2.1)

**File Updated**: `/docs/Alpha_Phase_1_Archive/planning_archive/2025-11-01/older_deprecated_plan/ALPHA_ROW_IDENTITY_ENHANCED.md`

**Changes**:
- Added reference to `ALPHA_CONTEXT_VARIABLES_DESIGN.md`
- Noted that `sdb$key`, `rdb$row_uuid`, `rdb$xact_id` are now part of broader context variable system
- Updated version to 2.1
- Added document update history

---

### 4. Updated PROJECT_CONTEXT.md

**Changes Made**:

1. **Current Priorities** (Section 3):
   - Added new priority: "Context Variables & Row Identity" (60-86 hours) - IN DESIGN
   - Breakdown:
     - Context Variables (16-22 hours) - HIGH PRIORITY
     - Row Identity: sdb$key (8-10 hours) - HIGH PRIORITY
     - Row Identity: rdb$row_uuid (12-16 hours) - HIGH PRIORITY
     - Transaction Visibility: rdb$xact_id (8-12 hours) - HIGH PRIORITY
     - TRANSFER Command (16-24 hours) - MEDIUM PRIORITY
   - Updated total remaining for ALPHA: ~110-152 hours (was ~50-66 hours)

2. **Planning Documents** (Section 5):
   - Added `ALPHA_CONTEXT_VARIABLES_DESIGN.md` (Context variables - Firebird Ch 12 - NEW)
   - Added `ALPHA_ROW_IDENTITY_ENHANCED.md` (Row identity & TRANSFER - NEW)
   - Added `ALPHA_ROW_IDENTITY_AND_TRANSACTION_VISIBILITY.md` (Original design v1.0)

3. **Update History** (Section 10):
   - Added entry: "2025-10-24: Context Variables design COMPLETE (Firebird Ch 12 pattern), Row Identity design updated (v2.1)"

---

## Key Technical Decisions

### 1. Context Variable Storage Format

**Decision**: Use `ContextVar` enum for bytecode, not string names.

**Rationale**:
- Smaller bytecode (1 byte vs 20+ bytes)
- Faster execution (enum switch vs string lookup)
- Type-safe at compile time

**Implementation**:
```cpp
enum class ContextVar : uint8_t {
    CURRENT_CONNECTION = 0,
    CURRENT_USER = 1,
    CURRENT_TRANSACTION = 3,
    SDB_KEY = 4,
    RDB_ROW_UUID = 5,
    // ...
};

// Bytecode: OP_PUSH_CONTEXT_VAR <var_id> <precision>
```

---

### 2. PSQL Stability Mechanism

**Decision**: Use depth counter + cache reset on outermost module entry.

**Rationale**:
- Matches Firebird behavior (constant within outermost PSQL module)
- Simple implementation (depth counter + optional cache)
- Efficient (no timestamp comparisons)

**Implementation**:
```cpp
class ConnectionContext {
    uint32_t psql_depth_ = 0;

    void enterPsqlModule() {
        if (psql_depth_++ == 0) {
            resetPsqlCache();  // Reset on outermost entry
        }
    }

    void exitPsqlModule() {
        --psql_depth_;
    }
};
```

---

### 3. Row Identity Hybrid Approach

**Decision**: Provide both `sdb$key` (physical) and `rdb$row_uuid` (logical).

**Rationale**:
- **sdb$key**: Fast (0 bytes overhead), good for positional updates
- **rdb$row_uuid**: Stable across backup/restore, good for permanent references
- Users choose based on use case

**Trade-offs**:
| Feature | sdb$key | rdb$row_uuid |
|---------|---------|--------------|
| Storage | 0 bytes (derived) | +16 bytes |
| Stability | Changes on migration | Never changes |
| Lookup Speed | O(1) with hash index | O(log N) with B-tree |
| Use Case | Positional updates | Permanent references |

---

### 4. UUID Storage Format

**Decision**: Store UUIDs as 16-byte binary, display as 36-character string.

**Rationale**:
- Matches user feedback: "stored as either a 16 byte binary field or a unsigned 128bit integer internally, while displaying formated UUIDs when displaying"
- Space-efficient (16 bytes vs 36 bytes)
- Accepts both formats for input (binary or string)

**Implementation**:
```cpp
struct UuidV7Bytes {
    std::array<uint8_t, 16> bytes{};

    [[nodiscard]] auto toString() const -> std::string;  // Format as string
    [[nodiscard]] static auto fromString(std::string_view) -> Result<UuidV7Bytes>;
    [[nodiscard]] static auto fromBytes(const uint8_t*) -> UuidV7Bytes;
};

// Accept both formats:
INSERT INTO customers (rdb$row_uuid) VALUES (X'018C3C5E...');  -- Binary
INSERT INTO customers (rdb$row_uuid) VALUES ('018c3c5e-...');   -- String
```

---

### 5. String Literal Mnemonics

**Decision**: Implement `'NOW'`, `'TODAY'`, etc. as special CAST behavior, not as keywords.

**Rationale**:
- Matches Firebird semantics
- Allows progressing values in PSQL (bypass cache)
- Backwards compatible (still valid string literals)

**Implementation**:
```cpp
// In semantic_analyzer.cpp
if (isCastToDateTimeType(target_type)) {
    std::string upper = toUpper(trim(value.getString()));

    if (upper == "NOW") {
        return Value::fromTimestamp(Clock::now());  // ALWAYS current
    } else if (upper == "TODAY") {
        return Value::fromDate(Clock::today());
    }
    // ...
}
```

---

## Completeness Checklist

### Design Documents
- [x] Context variables design (ALPHA_CONTEXT_VARIABLES_DESIGN.md) - 1,200 lines
- [x] Updated row identity design (ALPHA_ROW_IDENTITY_ENHANCED.md) - v2.1
- [x] Firebird Chapter 12 research complete

### PROJECT_CONTEXT.md Updates
- [x] Current Priorities updated with new phase
- [x] Planning Documents section updated
- [x] Update History updated

### Implementation Plan
- [x] 6 phases defined (16-22 hours total)
- [x] 35 unit tests specified
- [x] Success criteria defined
- [x] Risks and mitigations documented

---

## Statistics

### Documentation Created
- **New Files**: 1 (ALPHA_CONTEXT_VARIABLES_DESIGN.md)
- **Updated Files**: 2 (ALPHA_ROW_IDENTITY_ENHANCED.md, PROJECT_CONTEXT.md)
- **Total Lines Written**: ~1,250 lines

### Context Variables Designed
- **Firebird Standard**: 24 context variables
- **ScratchBird Extensions**: 3 new variables (`sdb$key`, `rdb$row_uuid`, `rdb$xact_id`)
- **Total**: 27 context variables

### Implementation Estimates
- **Context Variables Core**: 16-22 hours
- **Row Identity (sdb$key + rdb$row_uuid)**: 20-26 hours
- **Transaction Visibility (rdb$xact_id)**: 8-12 hours
- **TRANSFER Command**: 16-24 hours
- **Total**: 60-86 hours

---

## Next Steps

### Immediate (High Priority)
1. **Review Design** with stakeholders
   - Confirm Firebird compatibility requirements
   - Validate context variable list completeness
   - Approve row identity hybrid approach

2. **Prioritize Implementation Order**:
   - **Option A**: Context variables first (foundation for everything)
   - **Option B**: Row identity first (more user-visible features)
   - **Recommendation**: Context variables first (foundational)

### Near-Term (Next 1-2 Weeks)
3. **Begin Phase 1 Implementation** (Core Infrastructure)
   - Create `context_variables.h` with `ContextVar` enum
   - Add keywords to lexer
   - Implement parser for context variable expressions

4. **Create Unit Test Stubs** (35 tests)
   - Set up test infrastructure
   - Create skeleton tests with TODO comments

5. **Update SPRINT7_PHASE7_PREPARATION.md**
   - Add Context Variables & Row Identity as prerequisite
   - Adjust Phase 7 timeline

---

## Lessons Learned

### What Went Well
1. **Comprehensive Firebird Research**: Reading Chapter 12 provided complete understanding of context variable patterns
2. **Clear Design Structure**: 14-section design document covers all aspects (syntax, semantics, implementation, testing)
3. **User Feedback Integration**: Successfully incorporated UUID storage format and context variable pattern from user feedback

### Challenges
1. **Scope Expansion**: Adding context variables increased ALPHA effort by 60-86 hours (~110-152 hours total remaining)
2. **TupleHeader Size Growth**: Adding `row_uuid` increases tuple header from 44 → 60 bytes (+36% size increase)

### Future Improvements
1. **Consider Optional Row UUID**: Make `rdb$row_uuid` optional per-table to avoid +16 byte overhead
2. **Compression**: Implement tuple header compression to mitigate size increase
3. **Context Variable Caching**: Consider more aggressive caching strategies for date/time variables

---

## References

### Firebird Documentation
- **Chapter 12**: Context Variables (lines 26712-27509 of FirebirdReferenceDocument.md)
- **24 context variables** documented

### Design Documents
- `ALPHA_CONTEXT_VARIABLES_DESIGN.md` (NEW)
- `ALPHA_ROW_IDENTITY_ENHANCED.md` (v2.1)
- `ALPHA_ROW_IDENTITY_AND_TRANSACTION_VISIBILITY.md` (v1.0)

### Implementation References
- `heap_page.h` - TupleHeader structure
- `uuidv7.h` - UUID v7 implementation (already exists)
- `connection_context.h` - Connection state management

---

## Approval Status

- [ ] Design reviewed by stakeholders
- [ ] Implementation priority confirmed
- [ ] Resource allocation approved
- [ ] Timeline integrated into ALPHA roadmap

---

**Document Version**: 1.0
**Last Updated**: 2025-10-24
**Status**: COMPLETE
**Next Session**: Begin Phase 1 Implementation (Core Infrastructure)
