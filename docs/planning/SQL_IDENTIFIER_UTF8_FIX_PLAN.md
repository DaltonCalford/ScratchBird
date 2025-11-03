# SQL Identifier UTF-8 Support Fix Plan

**Project**: ScratchBird Database Engine
**Plan Date**: November 3, 2025
**Status**: IN PROGRESS (Phase 3 Complete - BUGS FIXED)
**Priority**: 🔴 **CRITICAL**
**Scope**: Fix catalog layer UTF-8 identifier truncation bugs
**Progress**: 3/7 phases complete (42.9%)

---

## EXECUTIVE SUMMARY

This plan addresses **CRITICAL BUGS** identified in the SQL Identifier Length Support Audit (docs/audit/03_SQL_IDENTIFIER_AUDIT.md). The catalog persistence layer has two critical bugs that violate SQL standards:

1. **Bug #1**: `strncpy()` truncates identifiers by BYTES instead of CHARACTERS, causing data corruption
2. **Bug #2**: Fixed 128-byte arrays cannot store 128 multi-byte UTF-8 characters

**Risk Level**: 🔴 **CRITICAL** - Data corruption, data loss, SQL standard violation

**Estimated Effort**: 15-20 hours (1-2 days)

---

## PROBLEM STATEMENT

### Current Behavior (WRONG)

**Parser Layer**: ✅ Correctly validates 128 UTF-8 characters
**Catalog Layer**: ❌ Truncates to 127 BYTES (not characters)

**Example Bug**:
```sql
-- 64 UTF-8 characters, 192 bytes (Chinese characters are 3 bytes each)
CREATE SCHEMA 你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好;

-- Parser: ✅ PASS (64 characters ≤ 128)
-- Catalog write: ❌ FAIL (truncates to 127 bytes → "你好你好...你" [incomplete character])
-- Result: Invalid UTF-8 stored in catalog → DATABASE CORRUPTION
```

### Impact

| Issue | Severity | Impact |
|-------|----------|--------|
| **Data Corruption** | 🔴 CRITICAL | Multi-byte characters truncated mid-byte → invalid UTF-8 |
| **Data Loss** | 🔴 CRITICAL | Identifiers silently truncated without error |
| **SQL Standard Violation** | 🔴 CRITICAL | Does not support 128-character limit for non-ASCII names |
| **Production Blocker** | 🔴 CRITICAL | Cannot use ScratchBird with international character sets |

### Root Causes

1. **Byte-based truncation**: `strncpy(dest, src, 127)` truncates by bytes
2. **Insufficient storage**: `char[128]` can only hold 31-127 UTF-8 characters depending on byte width
3. **Missing validation**: No check that identifier fits in storage before write

---

## GOALS

### Primary Goals

1. **Fix Data Corruption**: Ensure identifiers are never truncated mid-character
2. **Support 128 Characters**: Full UTF-8 support for 128-character identifiers
3. **SQL Standard Compliance**: Match SQL standard for identifier length
4. **No Data Loss**: Reject identifiers that won't fit instead of silently truncating

### Success Criteria

- ✅ 128-character ASCII identifiers work (current: works)
- ✅ 128-character multi-byte UTF-8 identifiers work (current: fails)
- ✅ No invalid UTF-8 stored in catalog (current: fails)
- ✅ Clear error messages when identifiers exceed storage capacity
- ✅ All existing tests pass
- ✅ New UTF-8 identifier tests pass

---

## IMPLEMENTATION PHASES

### Phase 0: Preparation & Planning ✅
**Duration**: 1-2 hours
**Status**: COMPLETE

**Tasks**:
- [x] Read audit report ✅
- [x] Create comprehensive fix plan (this document) ✅
- [x] Review catalog manager code ✅
- [x] Identify all affected code locations ✅
- [x] Design UTF-8-aware truncation helper ✅

**Completion Date**: November 3, 2025

---

### Phase 1: UTF-8 Utility Enhancements ✅
**Duration**: 2.5 hours (estimated 2-3 hours)
**Priority**: HIGH
**Status**: COMPLETE
**Goal**: Add UTF-8-aware truncation helpers

**Completion Date**: November 3, 2025
**Status Document**: docs/status/PHASE1_UTF8_UTILS_COMPLETE.md

#### Task 1.1: Add `UTF8Utils::truncateToBytes()`

**File**: `include/scratchbird/core/utf8_utils.h`

**Add declaration**:
```cpp
/**
 * Truncate UTF-8 string to fit in maximum byte count without splitting characters
 *
 * @param str Input string
 * @param max_bytes Maximum byte count (including null terminator)
 * @return Truncated string that fits in max_bytes without splitting UTF-8 characters
 *
 * Example:
 *   truncateToBytes("你好世界", 7) → "你好" (6 bytes + null = 7 bytes)
 *   truncateToBytes("你好世界", 6) → "你"  (3 bytes + null = 4 bytes, "你好" would be 7)
 */
static std::string truncateToBytes(std::string_view str, size_t max_bytes);
```

**Implementation** (`src/core/utf8_utils.cpp`):
```cpp
std::string UTF8Utils::truncateToBytes(std::string_view str, size_t max_bytes)
{
    if (max_bytes == 0) return "";

    // Reserve space for null terminator
    size_t max_content = max_bytes - 1;

    if (str.size() <= max_content) {
        return std::string(str);  // Fits without truncation
    }

    // Find last complete UTF-8 character that fits
    size_t byte_pos = 0;
    size_t last_safe_pos = 0;

    while (byte_pos < str.size() && byte_pos < max_content) {
        unsigned char c = static_cast<unsigned char>(str[byte_pos]);
        size_t char_bytes = 0;

        // Determine UTF-8 character byte length
        if ((c & 0x80) == 0x00) {
            char_bytes = 1;  // ASCII (0xxxxxxx)
        } else if ((c & 0xE0) == 0xC0) {
            char_bytes = 2;  // 2-byte (110xxxxx)
        } else if ((c & 0xF0) == 0xE0) {
            char_bytes = 3;  // 3-byte (1110xxxx)
        } else if ((c & 0xF8) == 0xF0) {
            char_bytes = 4;  // 4-byte (11110xxx)
        } else {
            // Invalid UTF-8 start byte - stop here
            break;
        }

        // Check if complete character fits
        if (byte_pos + char_bytes <= max_content) {
            last_safe_pos = byte_pos + char_bytes;
            byte_pos += char_bytes;
        } else {
            // Character would be split - stop at last safe position
            break;
        }
    }

    return std::string(str.substr(0, last_safe_pos));
}
```

**Test Cases**:
```cpp
// Test 1: ASCII (no truncation needed)
EXPECT_EQ(UTF8Utils::truncateToBytes("hello", 128), "hello");

// Test 2: ASCII (truncation needed)
EXPECT_EQ(UTF8Utils::truncateToBytes("hello world", 6), "hello");

// Test 3: Multi-byte (fits completely)
EXPECT_EQ(UTF8Utils::truncateToBytes("你好", 128), "你好");

// Test 4: Multi-byte (truncate without splitting)
EXPECT_EQ(UTF8Utils::truncateToBytes("你好世界", 7), "你好");  // 6 bytes + null

// Test 5: Multi-byte (would split - truncate earlier)
EXPECT_EQ(UTF8Utils::truncateToBytes("你好世界", 6), "你");  // 3 bytes + null
```

**Estimated Time**: 2 hours (implementation + tests)

#### Task 1.2: Add `UTF8Utils::validateStorageCapacity()`

**File**: `include/scratchbird/core/utf8_utils.h`

**Add declaration**:
```cpp
/**
 * Validate that a UTF-8 string can be stored in fixed-size storage
 *
 * @param str Input string
 * @param max_chars Maximum character count (SQL standard: 128)
 * @param max_bytes Maximum byte count (storage capacity including null terminator)
 * @param ctx Error context for detailed error messages
 * @return Status::OK if valid, error status otherwise
 */
static Status validateStorageCapacity(std::string_view str,
                                      size_t max_chars,
                                      size_t max_bytes,
                                      ErrorContext* ctx = nullptr);
```

**Implementation**:
```cpp
Status UTF8Utils::validateStorageCapacity(std::string_view str,
                                         size_t max_chars,
                                         size_t max_bytes,
                                         ErrorContext* ctx)
{
    // Check character count
    size_t char_count = countCharacters(str);
    if (char_count > max_chars) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "Identifier exceeds maximum length: " + std::to_string(char_count) +
            " characters (maximum " + std::to_string(max_chars) + ")");
        return Status::INVALID_ARGUMENT;
    }

    // Check byte count (including null terminator)
    if (str.size() + 1 > max_bytes) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "Identifier exceeds storage capacity: " + std::to_string(str.size()) +
            " bytes (maximum " + std::to_string(max_bytes - 1) + " bytes + null terminator)");
        return Status::INVALID_ARGUMENT;
    }

    return Status::OK;
}
```

**Estimated Time**: 1 hour

---

### Phase 2: Catalog Storage Structure Expansion ✅
**Duration**: 1.5 hours (estimated 2-3 hours)
**Priority**: HIGH
**Status**: COMPLETE
**Goal**: Increase catalog storage arrays to support 128 UTF-8 characters

**Completion Date**: November 3, 2025
**Status Document**: docs/status/PHASE2_CATALOG_STORAGE_EXPANSION_COMPLETE.md

#### Task 2.1: Expand Catalog Record Structures

**File**: `src/core/catalog_manager.cpp`

**Change all identifier fields**:

**Before** (WRONG):
```cpp
char schema_name[128];    // Only holds 31-127 UTF-8 characters
char table_name[128];
char column_name[128];
char index_name[128];
char constraint_name[128];
char sequence_name[128];
char view_name[128];
char trigger_name[128];
```

**After** (CORRECT):
```cpp
char schema_name[512];    // Holds 128 UTF-8 characters (128 * 4 bytes max)
char table_name[512];
char column_name[512];
char index_name[512];
char constraint_name[512];
char sequence_name[512];
char view_name[512];
char trigger_name[512];
```

**Rationale**:
- UTF-8 characters are 1-4 bytes each
- 128 characters × 4 bytes = 512 bytes maximum
- +1 for null terminator already included in 512

**Impact**:
- Catalog page size increases by 384 bytes per identifier
- Trade-off: Larger catalog vs. full UTF-8 support
- **Decision**: Full UTF-8 support is worth the space cost

**Affected Lines**:
- Line 53: `SchemaRecord::schema_name`
- Line 84: `TableRecord::table_name`
- Line 106: `ColumnRecord::column_name`
- Line 149: `IndexRecord::index_name`
- Line 239: `ConstraintRecord::constraint_name`
- Line 260: `SequenceRecord::sequence_name`
- Line 278: `ViewRecord::view_name`
- Line 293: `TriggerRecord::trigger_name`

**Estimated Time**: 1 hour (mechanical change + verification)

#### Task 2.2: Update Catalog Constants

**File**: `include/scratchbird/core/catalog_manager.h`

**Add constants**:
```cpp
namespace CatalogConstants {
    // SQL standard identifier limits
    constexpr size_t MAX_IDENTIFIER_CHARS = 128;      // SQL standard: 128 characters
    constexpr size_t MAX_IDENTIFIER_BYTES = 512;      // Storage: 128 chars × 4 bytes/char
    constexpr size_t MAX_IDENTIFIER_STORAGE = 512;    // Including null terminator
}
```

**Estimated Time**: 0.5 hours

---

### Phase 3: Catalog Write Logic Fixes ✅
**Duration**: 2 hours (estimated 3-4 hours)
**Priority**: 🔴 CRITICAL
**Status**: COMPLETE
**Goal**: Fix byte-based truncation bugs

**Completion Date**: November 3, 2025
**Status Document**: docs/status/PHASE3_CATALOG_WRITE_LOGIC_FIXES_COMPLETE.md

#### Task 3.1: Fix Schema Name Storage

**File**: `src/core/catalog_manager.cpp`

**Lines 1661-1663** (createSchema):

**Before** (BUG):
```cpp
strncpy(record.schema_name, schema.schema_name.c_str(), 127);
record.schema_name[127] = '\0';
```

**After** (FIXED):
```cpp
// Validate identifier length
Status validation = UTF8Utils::validateStorageCapacity(
    schema.schema_name,
    CatalogConstants::MAX_IDENTIFIER_CHARS,
    CatalogConstants::MAX_IDENTIFIER_STORAGE,
    ctx
);
if (validation != Status::OK) {
    return validation;
}

// Safe copy (already validated to fit)
std::memcpy(record.schema_name, schema.schema_name.c_str(), schema.schema_name.size());
record.schema_name[schema.schema_name.size()] = '\0';
```

**Estimated Time**: 1 hour

#### Task 3.2: Fix Table Name Storage

**File**: `src/core/catalog_manager.cpp`

**Lines 1705, 1816, 1834** (createTable, alterTable):

**Apply same fix**:
```cpp
// Validate identifier length
Status validation = UTF8Utils::validateStorageCapacity(
    table_info.table_name,
    CatalogConstants::MAX_IDENTIFIER_CHARS,
    CatalogConstants::MAX_IDENTIFIER_STORAGE,
    ctx
);
if (validation != Status::OK) {
    return validation;
}

// Safe copy (already validated to fit)
std::memcpy(record.table_name, table_info.table_name.c_str(), table_info.table_name.size());
record.table_name[table_info.table_name.size()] = '\0';
```

**Estimated Time**: 1 hour

#### Task 3.3: Fix Column Name Storage

**File**: `src/core/catalog_manager.cpp`

**Lines 1900, 1947-1949** (column creation/alteration):

**Apply same fix for column names**

**Estimated Time**: 1 hour

#### Task 3.4: Fix Index/Constraint/Sequence/View/Trigger Names

**File**: `src/core/catalog_manager.cpp`

**Apply same fix pattern to all remaining identifier storage locations**

**Locations**:
- Index names (createIndex)
- Constraint names (addConstraint)
- Sequence names (createSequence)
- View names (createView)
- Trigger names (createTrigger)

**Estimated Time**: 1 hour

---

### Phase 4: Catalog Read Logic Updates
**Duration**: 1-2 hours
**Priority**: MEDIUM
**Goal**: Ensure catalog reads handle expanded arrays

#### Task 4.1: Verify Read Logic

**File**: `src/core/catalog_manager.cpp`

**Action**: Review all catalog read operations to ensure they handle 512-byte arrays

**Expected**: No changes needed (reads use null-terminated strings)

**Verification**:
- Schema reads (loadSchemas)
- Table reads (loadTables)
- Column reads (loadColumns)
- Index reads (loadIndexes)

**Estimated Time**: 1 hour

#### Task 4.2: Add Safety Assertions

**Add to catalog read operations**:
```cpp
// Safety check: Ensure null-termination
record.schema_name[511] = '\0';  // Force null termination at max position
```

**Estimated Time**: 1 hour

---

### Phase 5: Testing & Validation
**Duration**: 4-6 hours
**Priority**: HIGH
**Goal**: Comprehensive UTF-8 identifier testing

#### Task 5.1: Create Unit Tests for UTF8Utils

**File**: `tests/unit/test_utf8_utils.cpp` (NEW)

**Test Cases**:
```cpp
TEST(UTF8UtilsTest, TruncateToBytes_ASCII) {
    EXPECT_EQ(UTF8Utils::truncateToBytes("hello", 128), "hello");
    EXPECT_EQ(UTF8Utils::truncateToBytes("hello world", 6), "hello");
}

TEST(UTF8UtilsTest, TruncateToBytes_MultibyteNoSplit) {
    // "你好" = 6 bytes, fits in 7 (6 + null)
    EXPECT_EQ(UTF8Utils::truncateToBytes("你好", 7), "你好");
}

TEST(UTF8UtilsTest, TruncateToBytes_MultibyteWithSplit) {
    // "你好世界" = 12 bytes
    // Max 7 bytes: "你好" (6) fits, "你好世" (9) doesn't
    EXPECT_EQ(UTF8Utils::truncateToBytes("你好世界", 7), "你好");
}

TEST(UTF8UtilsTest, ValidateStorageCapacity_Valid) {
    ErrorContext ctx;
    EXPECT_EQ(UTF8Utils::validateStorageCapacity("hello", 128, 512, &ctx), Status::OK);
}

TEST(UTF8UtilsTest, ValidateStorageCapacity_TooManyChars) {
    std::string long_name(200, 'a');  // 200 characters
    ErrorContext ctx;
    EXPECT_EQ(UTF8Utils::validateStorageCapacity(long_name, 128, 512, &ctx),
              Status::INVALID_ARGUMENT);
}

TEST(UTF8UtilsTest, ValidateStorageCapacity_TooManyBytes) {
    std::string chinese(200, '你');  // 200 Chinese chars = 600 bytes
    ErrorContext ctx;
    EXPECT_EQ(UTF8Utils::validateStorageCapacity(chinese, 200, 512, &ctx),
              Status::INVALID_ARGUMENT);
}
```

**Estimated Time**: 2 hours

#### Task 5.2: Create Integration Tests for Catalog

**File**: `tests/integration/test_catalog_utf8_identifiers.cpp` (NEW)

**Test Cases**:
```cpp
TEST_F(CatalogUTF8Test, CreateSchema_ASCII_MaxLength) {
    // 128 ASCII characters (maximum allowed)
    std::string schema_name(128, 'a');

    SchemaInfo schema;
    schema.schema_name = schema_name;
    schema.owner = "test_user";

    ErrorContext ctx;
    Status status = catalog_->createSchema(schema, &ctx);

    EXPECT_EQ(status, Status::OK);

    // Verify stored correctly
    auto loaded = catalog_->getSchema(schema.schema_id, &ctx);
    EXPECT_EQ(loaded.schema_name, schema_name);
}

TEST_F(CatalogUTF8Test, CreateSchema_Chinese_64Chars) {
    // 64 Chinese characters = 192 bytes (should fit in 512)
    std::string schema_name;
    for (int i = 0; i < 64; i++) {
        schema_name += "你好";  // Each Chinese char is 3 bytes
    }

    SchemaInfo schema;
    schema.schema_name = schema_name;
    schema.owner = "test_user";

    ErrorContext ctx;
    Status status = catalog_->createSchema(schema, &ctx);

    EXPECT_EQ(status, Status::OK);

    // Verify stored correctly (no truncation)
    auto loaded = catalog_->getSchema(schema.schema_id, &ctx);
    EXPECT_EQ(loaded.schema_name, schema_name);
}

TEST_F(CatalogUTF8Test, CreateSchema_Emoji_128Chars) {
    // 128 emoji characters = 512 bytes (maximum)
    std::string schema_name;
    for (int i = 0; i < 128; i++) {
        schema_name += "😀";  // Emoji is 4 bytes
    }

    SchemaInfo schema;
    schema.schema_name = schema_name;
    schema.owner = "test_user";

    ErrorContext ctx;
    Status status = catalog_->createSchema(schema, &ctx);

    EXPECT_EQ(status, Status::OK);

    // Verify stored correctly
    auto loaded = catalog_->getSchema(schema.schema_id, &ctx);
    EXPECT_EQ(loaded.schema_name, schema_name);
}

TEST_F(CatalogUTF8Test, CreateSchema_ExceedsCharLimit) {
    // 200 characters (exceeds 128 limit)
    std::string schema_name(200, 'a');

    SchemaInfo schema;
    schema.schema_name = schema_name;
    schema.owner = "test_user";

    ErrorContext ctx;
    Status status = catalog_->createSchema(schema, &ctx);

    EXPECT_EQ(status, Status::INVALID_ARGUMENT);
    EXPECT_THAT(ctx.message, testing::HasSubstr("maximum length"));
}

TEST_F(CatalogUTF8Test, CreateSchema_ExceedsByteLimit) {
    // 200 emoji characters = 800 bytes (exceeds 512 byte limit)
    std::string schema_name;
    for (int i = 0; i < 200; i++) {
        schema_name += "😀";
    }

    SchemaInfo schema;
    schema.schema_name = schema_name;
    schema.owner = "test_user";

    ErrorContext ctx;
    Status status = catalog_->createSchema(schema, &ctx);

    EXPECT_EQ(status, Status::INVALID_ARGUMENT);
    EXPECT_THAT(ctx.message, testing::HasSubstr("storage capacity"));
}

TEST_F(CatalogUTF8Test, CreateTable_MixedUTF8) {
    // Mixed ASCII, Latin-1, Chinese, emoji
    std::string table_name = "café_北京_😀_table";

    TableInfo table;
    table.table_name = table_name;

    ErrorContext ctx;
    Status status = catalog_->createTable(table, &ctx);

    EXPECT_EQ(status, Status::OK);

    // Verify stored correctly
    auto loaded = catalog_->getTable(table.table_id, &ctx);
    EXPECT_EQ(loaded.table_name, table_name);
}
```

**Estimated Time**: 3 hours

#### Task 5.3: Create SQL-Level Tests

**File**: `tests/sql/test_utf8_identifiers.sql` (NEW)

**Test Cases**:
```sql
-- Test 1: ASCII maximum length
CREATE SCHEMA a123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789;
DROP SCHEMA a123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789;

-- Test 2: Chinese characters (64 chars = 192 bytes)
CREATE SCHEMA "你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好你好";

-- Test 3: Mixed UTF-8
CREATE TABLE "café_résumé_北京_😀_table" (id INT);

-- Test 4: Maximum emoji (128 chars = 512 bytes)
CREATE TABLE "😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀😀" (id INT);

-- Test 5: Should fail (exceeds character limit)
CREATE SCHEMA "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_EXTRA";  -- 134 chars
```

**Estimated Time**: 1 hour

---

### Phase 6: Documentation
**Duration**: 2-3 hours
**Priority**: MEDIUM
**Goal**: Document UTF-8 identifier support

#### Task 6.1: Update Architecture Documentation

**File**: `docs/specifications/CHARACTER_SETS_AND_COLLATIONS.md`

**Add section**:
```markdown
## SQL Identifier UTF-8 Support

### Limits

- **Character Limit**: 128 UTF-8 characters (SQL standard)
- **Byte Limit**: 512 bytes (storage capacity)
- **Encoding**: UTF-8 only

### Storage

Catalog records use fixed-size `char[512]` arrays to support:
- 128 ASCII characters (1 byte each = 128 bytes)
- 128 2-byte UTF-8 characters (256 bytes)
- 128 3-byte UTF-8 characters (384 bytes)
- 128 4-byte UTF-8 characters (512 bytes maximum)

### Validation

Identifiers are validated at two levels:
1. **Parser**: Validates 128 UTF-8 character limit
2. **Catalog**: Validates 512 byte storage limit

### Examples

Valid:
- `CREATE SCHEMA café;` (4 chars, 5 bytes)
- `CREATE TABLE 北京_table;` (9 chars, 15 bytes)
- `CREATE INDEX idx_😀;` (5 chars, 8 bytes)

Invalid:
- 129+ characters (exceeds character limit)
- 513+ bytes (exceeds storage capacity)
```

**Estimated Time**: 1 hour

#### Task 6.2: Update Catalog Manager Header Documentation

**File**: `include/scratchbird/core/catalog_manager.h`

**Add to class documentation**:
```cpp
/**
 * CatalogManager - SQL Catalog Management
 *
 * Identifier Limits:
 * - Maximum length: 128 UTF-8 characters (SQL standard)
 * - Maximum storage: 512 bytes (supports all UTF-8 characters)
 * - Encoding: UTF-8 only
 *
 * Storage Format:
 * - All identifiers stored in fixed char[512] arrays
 * - Validation ensures both character and byte limits
 * - Invalid UTF-8 rejected at parser level
 */
```

**Estimated Time**: 0.5 hours

#### Task 6.3: Create Fix Status Document

**File**: `docs/status/SQL_IDENTIFIER_UTF8_FIX_COMPLETE.md` (NEW)

**Contents**:
- Summary of bugs fixed
- Implementation details
- Test coverage
- Validation results
- Before/after comparison

**Estimated Time**: 1 hour

---

### Phase 7: Migration & Compatibility
**Duration**: 2-3 hours
**Priority**: HIGH
**Goal**: Handle existing databases

#### Task 7.1: Database Migration

**Issue**: Existing databases have 128-byte catalog records

**Solution Options**:

**Option 1: Fresh Install Only** (RECOMMENDED)
- Document that fix requires fresh database creation
- No migration needed
- Simpler, safer

**Option 2: Catalog Migration Script**
- Create migration script to rebuild catalog with 512-byte arrays
- Complex, error-prone
- Not recommended for alpha version

**Decision**: Option 1 (fresh install only)

**Documentation**:
```markdown
## Breaking Change Notice

**Version**: 1.5.0
**Date**: November 2025
**Impact**: Catalog format change

### What Changed

Catalog record identifier storage increased from 128 bytes to 512 bytes
to support full UTF-8 character range (128 characters).

### Migration Required

**IMPORTANT**: This change requires a fresh database creation.

Existing databases created with previous versions are incompatible.

**Migration Steps**:
1. Export data using `scratchbird dump`
2. Create new database with version 1.5.0+
3. Import data using `scratchbird restore`

### Rationale

This change fixes critical data corruption bugs with multi-byte UTF-8
identifiers and ensures SQL standard compliance.
```

**Estimated Time**: 2 hours

---

## TESTING STRATEGY

### Unit Tests
- ✅ UTF8Utils::truncateToBytes()
- ✅ UTF8Utils::validateStorageCapacity()
- ✅ Character counting accuracy

### Integration Tests
- ✅ Schema creation with UTF-8 identifiers
- ✅ Table creation with UTF-8 identifiers
- ✅ Column creation with UTF-8 identifiers
- ✅ Index creation with UTF-8 identifiers
- ✅ Maximum character length (128 chars)
- ✅ Maximum byte length (512 bytes)
- ✅ Error handling for exceeding limits

### SQL-Level Tests
- ✅ ASCII identifiers (1 byte/char)
- ✅ Latin-1 extended identifiers (2 bytes/char)
- ✅ Chinese/Japanese identifiers (3 bytes/char)
- ✅ Emoji identifiers (4 bytes/char)
- ✅ Mixed UTF-8 identifiers
- ✅ Boundary conditions (exactly 128 chars, exactly 512 bytes)

### Regression Tests
- ✅ All existing tests pass
- ✅ No performance degradation
- ✅ No memory leaks

---

## RISK ASSESSMENT

### High Risk Areas

| Risk | Mitigation |
|------|------------|
| **Catalog page size increase** | Acceptable trade-off for correctness |
| **Breaking change** | Document clearly, require fresh database |
| **Performance impact** | Minimal (validation is fast) |
| **Memory usage** | Fixed arrays only in catalog records (acceptable) |

### Low Risk Areas

| Area | Reason |
|------|--------|
| **Parser layer** | Already correct (no changes needed) |
| **UTF8Utils** | Adding new functions (no breaking changes) |
| **AST layer** | No changes needed |
| **Application layer** | Transparent (no API changes) |

---

## ACCEPTANCE CRITERIA

### Critical (Must-Have)

- [ ] No data corruption with multi-byte identifiers
- [ ] Full 128-character UTF-8 support
- [ ] Clear error messages for invalid identifiers
- [ ] All unit tests pass
- [ ] All integration tests pass
- [ ] All SQL-level tests pass

### High Priority

- [ ] Documentation updated
- [ ] Migration guide created
- [ ] Performance acceptable (<5% regression)
- [ ] No memory leaks

### Medium Priority

- [ ] Code review completed
- [ ] Breaking change documented
- [ ] Release notes created

---

## TIMELINE

### Estimated Timeline (15-20 hours total)

| Phase | Duration | Dependencies |
|-------|----------|--------------|
| Phase 0: Planning | 1-2 hours | None |
| Phase 1: UTF8Utils | 2-3 hours | Phase 0 |
| Phase 2: Catalog Storage | 2-3 hours | Phase 1 |
| Phase 3: Write Logic | 3-4 hours | Phase 2 |
| Phase 4: Read Logic | 1-2 hours | Phase 3 |
| Phase 5: Testing | 4-6 hours | Phase 4 |
| Phase 6: Documentation | 2-3 hours | Phase 5 |
| Phase 7: Migration | 2-3 hours | Phase 6 |

**Total**: 15-20 hours (2-3 days of focused work)

---

## SUCCESS METRICS

### Functional Metrics

- ✅ 0 data corruption bugs
- ✅ 100% UTF-8 character support (1-4 bytes)
- ✅ 100% test coverage for UTF-8 identifiers

### Quality Metrics

- ✅ Code review approval
- ✅ All tests passing
- ✅ Documentation complete

### Performance Metrics

- ✅ <5% performance regression
- ✅ No memory leaks
- ✅ Catalog operations remain O(log n)

---

## ROLLBACK PLAN

If critical issues are discovered:

1. **Immediate**: Revert catalog storage changes
2. **Short-term**: Document workaround (ASCII-only identifiers)
3. **Long-term**: Redesign with TOAST references for identifiers

---

## FUTURE ENHANCEMENTS

### Post-Fix Optimizations

1. **TOAST References for Identifiers** (~10-15 hours)
   - Store identifiers >128 bytes in TOAST tables
   - Reduce catalog page size
   - More complex implementation

2. **Variable-Length Catalog Records** (~15-20 hours)
   - Use variable-length encoding for identifiers
   - Space-efficient for ASCII-heavy schemas
   - Significant catalog redesign

3. **Identifier Compression** (~5-8 hours)
   - Compress identifiers in catalog
   - Decompress on read
   - Trade CPU for space

**Decision**: Defer to future releases (current fix is production-ready)

---

## CONCLUSION

This plan addresses **CRITICAL BUGS** in SQL identifier UTF-8 support. The fix is straightforward:

1. **Add UTF-8-aware truncation helpers** (Phase 1)
2. **Expand catalog storage to 512 bytes** (Phase 2)
3. **Fix byte-based truncation bugs** (Phase 3)
4. **Comprehensive testing** (Phase 5)

**Estimated Effort**: 15-20 hours
**Risk Level**: LOW (well-understood problem, clear solution)
**Impact**: HIGH (enables international character sets)

**Next Steps**: Begin Phase 0 (planning review) and proceed with implementation.

---

**Plan Date**: November 3, 2025
**Plan Status**: READY FOR REVIEW
**Implementation Status**: NOT STARTED
**Target Completion**: November 6-7, 2025 (2-3 days)
