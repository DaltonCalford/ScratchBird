# Character Sets and Collations Design Specification

**Date:** October 4, 2025
**Status:** Design
**Target:** Stage 1.1+

## Overview

ScratchBird will support multiple character sets and collations for internationalization and proper text handling. All system objects will support UTF-8 as the default, with support for additional character sets.

## Requirements

### Functional Requirements
1. Support multiple character sets (UTF-8, UTF-16, UTF-32, Latin1, ASCII)
2. Support multiple collations per character set
3. UTF-8 as default for all system objects
4. Character set specification at database, table, and column levels
5. Collation specification at database, table, column, and expression levels
6. Character set conversion between different encodings
7. Collation-aware string comparison and sorting
8. UTF-8 validation and normalization

### SQL Standard Compliance
- SQL:2023 character set and collation support
- `CHARACTER SET` clause in table/column definitions
- `COLLATE` clause for string comparisons
- `CONVERT()` function for character set conversion

### PostgreSQL Compatibility
- Similar to PostgreSQL's encoding and collation system
- Support for `LC_COLLATE` and `LC_CTYPE`
- ICU collation support (future)

## Architecture

### Character Set Structure

```cpp
enum class CharacterSet : uint16_t {
    ASCII = 0,          // 7-bit ASCII (1 byte per char)
    LATIN1 = 1,         // ISO-8859-1 (1 byte per char)
    UTF8 = 2,           // UTF-8 (1-4 bytes per char) - DEFAULT
    UTF16 = 3,          // UTF-16 (2-4 bytes per char)
    UTF32 = 4,          // UTF-32 (4 bytes per char)
    UTF8MB4 = 5,        // UTF-8 with full Unicode support (MySQL compatible)

    // Future extensions
    LATIN2 = 10,        // ISO-8859-2 (Central European)
    LATIN5 = 11,        // ISO-8859-9 (Turkish)
    LATIN7 = 12,        // ISO-8859-13 (Baltic)
    WIN1252 = 20,       // Windows-1252 (Western European)
    WIN1251 = 21,       // Windows-1251 (Cyrillic)
    SJIS = 30,          // Shift-JIS (Japanese)
    GBK = 31,           // GBK (Chinese)
    BIG5 = 32,          // Big5 (Traditional Chinese)
    EUC_KR = 33         // EUC-KR (Korean)
};

struct CharacterSetInfo {
    CharacterSet id;
    std::string name;           // e.g., "utf8", "latin1"
    std::string description;    // Human-readable description
    uint8_t min_bytes;          // Minimum bytes per character
    uint8_t max_bytes;          // Maximum bytes per character
    bool variable_width;        // True for multi-byte encodings
    std::string default_collation; // Default collation name
};
```

### Collation Structure

```cpp
enum class CollationStrength : uint8_t {
    PRIMARY = 1,        // Base characters only (e.g., 'a' = 'A' = 'á')
    SECONDARY = 2,      // Base + accents (e.g., 'a' = 'A', 'a' != 'á')
    TERTIARY = 3,       // Base + accents + case (e.g., 'a' != 'A')
    QUATERNARY = 4,     // Base + accents + case + punctuation
    IDENTICAL = 5       // All differences matter (binary)
};

enum class CollationType : uint8_t {
    BINARY = 0,         // Byte-by-byte comparison (fastest)
    CASE_SENSITIVE = 1, // Case-sensitive, accent-sensitive
    CASE_INSENSITIVE = 2, // Case-insensitive, accent-sensitive (ci)
    ACCENT_INSENSITIVE = 3, // Case-sensitive, accent-insensitive (ai)
    CI_AI = 4,          // Case-insensitive, accent-insensitive
    UNICODE = 5,        // Unicode Collation Algorithm (UCA)
    NATURAL = 6,        // Natural/human sorting (1, 2, 10 not 1, 10, 2)
    NUMERIC = 7         // Numeric substring sorting
};

struct CollationInfo {
    uint32_t id;
    std::string name;           // e.g., "utf8_general_ci", "utf8_unicode_ci"
    CharacterSet charset;       // Associated character set
    CollationType type;
    CollationStrength strength;
    bool pad_space;             // PAD SPACE vs NO PAD (SQL standard)
    std::string locale;         // Locale string (e.g., "en_US", "de_DE")
    bool is_default;            // Default for this character set
};
```

### Common Collations

#### UTF-8 Collations
- `utf8_bin` - Binary (fastest, byte comparison)
- `utf8_general_ci` - General purpose, case-insensitive (MySQL compatible)
- `utf8_unicode_ci` - Unicode Collation Algorithm, case-insensitive
- `utf8_unicode_cs` - Unicode Collation Algorithm, case-sensitive
- `utf8_en_US_ci` - English (US), case-insensitive
- `utf8_de_DE_ci` - German, case-insensitive
- `utf8_ja_JP_ci` - Japanese, case-insensitive
- `utf8_zh_CN_ci` - Chinese (Simplified), case-insensitive

#### Latin1 Collations
- `latin1_bin` - Binary
- `latin1_general_ci` - General purpose, case-insensitive
- `latin1_general_cs` - General purpose, case-sensitive
- `latin1_swedish_ci` - Swedish (MySQL default for latin1)

## Catalog Integration

### Database-Level Character Set

```cpp
struct DatabaseInfo {
    // Existing fields...
    CharacterSet default_charset = CharacterSet::UTF8;
    uint32_t default_collation_id;  // Reference to CollationInfo
};
```

### Schema-Level Character Set

```cpp
struct SchemaRecord {
    // Existing fields...
    uint16_t default_charset;       // CharacterSet enum, 0 = inherit from database
    uint32_t default_collation_id;  // Collation ID, 0 = inherit from database
};
```

### Table-Level Character Set

```cpp
struct TableRecord {
    // Existing fields...
    uint16_t default_charset;       // CharacterSet enum, 0 = inherit from schema
    uint32_t default_collation_id;  // Collation ID, 0 = inherit from schema
};
```

### Column-Level Character Set

```cpp
struct ColumnRecord {
    // Existing fields...
    uint16_t charset;               // CharacterSet enum, 0 = inherit from table
    uint32_t collation_id;          // Collation ID, 0 = inherit from table
};
```

### New System Table: sys_collations

```cpp
struct CollationRecord {
    uint32_t collation_id;
    char collation_name[128];       // e.g., "utf8_general_ci"
    uint16_t charset;               // CharacterSet enum
    uint8_t collation_type;         // CollationType enum
    uint8_t strength;               // CollationStrength enum
    uint8_t is_default;             // Default for this charset
    uint8_t pad_space;              // PAD SPACE attribute
    uint16_t reserved;
    char locale[32];                // Locale string (e.g., "en_US")
    uint64_t created_time;
    uint32_t is_valid;
};
```

## String Storage

### UTF-8 Storage Format

UTF-8 strings are stored as variable-length byte sequences:
- 1 byte: U+0000 to U+007F (ASCII)
- 2 bytes: U+0080 to U+07FF
- 3 bytes: U+0800 to U+FFFF
- 4 bytes: U+10000 to U+10FFFF

**Storage Considerations:**
- `VARCHAR(N)` specifies **character count**, not byte count
- Maximum byte length = N × max_bytes_per_char
- For UTF-8: `VARCHAR(100)` can store up to 400 bytes (100 chars × 4 bytes)
- Actual byte length stored in tuple header
- Canonical encoding uses a uint32 byte-length prefix; CHAR(n) values are padded to fixed length, VARCHAR/TEXT keep actual length

### String Length Calculation

```cpp
// Character count (not byte count)
uint32_t utf8_char_length(const uint8_t* str, uint32_t byte_len);

// Byte length for N characters
uint32_t utf8_byte_length(const uint8_t* str, uint32_t char_count);

// Validate UTF-8 encoding
bool utf8_validate(const uint8_t* str, uint32_t byte_len);
```

## String Comparison

### Comparison Levels

1. **Binary Comparison** (fastest)
   ```cpp
   int compare_binary(const uint8_t* s1, uint32_t len1,
                      const uint8_t* s2, uint32_t len2);
   ```

2. **Case-Insensitive Comparison**
   ```cpp
   int compare_ci(const uint8_t* s1, uint32_t len1,
                  const uint8_t* s2, uint32_t len2,
                  CharacterSet charset);
   ```

3. **Unicode Collation Algorithm** (most accurate)
   ```cpp
   int compare_unicode(const uint8_t* s1, uint32_t len1,
                       const uint8_t* s2, uint32_t len2,
                       const CollationInfo& collation);
   ```

### Collation-Aware Operations

All string operations must be collation-aware:
- `LIKE` operator
- `UPPER()`, `LOWER()` functions
- `LENGTH()`, `CHAR_LENGTH()` functions
- `SUBSTRING()` function
- `CONCAT()` function
- String sorting in `ORDER BY`
- String grouping in `GROUP BY`
- Index key comparison

## Character Set Conversion

### Conversion Functions

```cpp
// Convert between character sets
Status convert_charset(
    const uint8_t* input, uint32_t input_len, CharacterSet from_cs,
    std::vector<uint8_t>& output, CharacterSet to_cs,
    ErrorContext* ctx
);

// SQL CONVERT function
// CONVERT('text' USING utf8)
// CONVERT('text', CHAR(10) CHARACTER SET latin1)
```

### Conversion Rules

1. **Lossless Conversions**
   - ASCII → UTF-8, Latin1, UTF-16, UTF-32
   - Latin1 → UTF-8, UTF-16, UTF-32
   - UTF-8 ↔ UTF-16 ↔ UTF-32

2. **Lossy Conversions**
   - UTF-8 → Latin1 (unmappable chars → '?')
   - UTF-8 → ASCII (non-ASCII → '?')
   - With error handling options:
     - ERROR: Fail on unmappable character
     - IGNORE: Skip unmappable characters
     - REPLACE: Replace with '?' or U+FFFD

## SQL Syntax

### Database Creation

```sql
CREATE DATABASE mydb
    CHARACTER SET utf8
    COLLATE utf8_general_ci;
```

### Table Creation

```sql
CREATE TABLE users (
    id INTEGER,
    name VARCHAR(100) CHARACTER SET utf8 COLLATE utf8_general_ci,
    email VARCHAR(255) COLLATE utf8_bin,
    bio TEXT
) CHARACTER SET utf8 COLLATE utf8_unicode_ci;
```

### Column-Level Specification

```sql
ALTER TABLE users
    MODIFY COLUMN name VARCHAR(100)
    CHARACTER SET latin1
    COLLATE latin1_swedish_ci;
```

### Expression-Level Collation

```sql
SELECT * FROM users
    WHERE name COLLATE utf8_bin = 'John';

SELECT * FROM users
    ORDER BY name COLLATE utf8_unicode_ci;
```

### Character Set Conversion

```sql
-- CONVERT function
SELECT CONVERT(name USING utf8) FROM users;

-- CAST with charset
SELECT CAST(name AS VARCHAR(100) CHARACTER SET latin1) FROM users;
```

## Implementation Plan

### Phase 1: Core Infrastructure (Priority 1)
1. Define `CharacterSet` and `CollationType` enums
2. Create `CharacterSetInfo` and `CollationInfo` structures
3. Add charset/collation fields to catalog records
4. Create `sys_collations` system table
5. Implement built-in collation registry

### Phase 2: UTF-8 Support (Priority 1)
1. Implement UTF-8 validation functions
2. Implement UTF-8 length calculation (char vs byte)
3. Update VARCHAR/TEXT storage to track byte length
4. Implement `utf8_bin` and `utf8_general_ci` collations
5. Update string comparison operators

### Phase 3: String Functions (Priority 2)
1. Update `LENGTH()` for character count
2. Update `SUBSTRING()` for multi-byte characters
3. Update `UPPER()`, `LOWER()` for Unicode
4. Implement collation-aware `LIKE`
5. Add `CHARACTER_LENGTH()`, `OCTET_LENGTH()` functions

### Phase 4: Additional Character Sets (Priority 3)
1. Implement Latin1 support
2. Implement UTF-16/UTF-32 support
3. Implement character set conversion
4. Add conversion functions to SQL

### Phase 5: Advanced Collations (Priority 4)
1. Implement locale-specific collations
2. Implement natural sorting
3. Implement accent-insensitive collations
4. ICU library integration (optional)

## Performance Considerations

### String Length Optimization

For UTF-8 strings, we need to store both:
1. **Byte length** - for storage allocation
2. **Character count** - for SQL semantics (optional, can be calculated)

**Optimization:** Cache character count in tuple for frequently accessed columns.

### Collation Performance

Collation comparison costs (relative):
- Binary: 1x (fastest, memcmp)
- ASCII case-insensitive: 2x (simple table lookup)
- UTF-8 case-insensitive: 5x (Unicode case folding)
- UCA (full Unicode): 20x (complex algorithm)

**Strategy:** Use binary collation for indexes, convert to user collation for display.

### Index Optimization

For string indexes:
1. Store normalized form in index (e.g., lowercase for CI collations)
2. Store original value in heap
3. Use collation-aware comparison in index operations

## Default Configuration

### System Default
- **Database charset:** UTF-8
- **Database collation:** `utf8_general_ci`
- **System catalog charset:** UTF-8 (fixed)
- **System catalog collation:** `utf8_bin` (fixed for performance)

### Compatibility Modes
- **MySQL Mode:** `utf8mb4_general_ci` as default
- **PostgreSQL Mode:** `en_US.UTF-8` as default
- **SQL Server Mode:** `SQL_Latin1_General_CP1_CI_AS` equivalent

## Testing Requirements

1. **Character Set Tests**
   - ASCII storage and retrieval
   - UTF-8 1-byte, 2-byte, 3-byte, 4-byte characters
   - Latin1 extended characters
   - UTF-8 validation (reject invalid sequences)

2. **Collation Tests**
   - Binary comparison (case-sensitive)
   - Case-insensitive comparison
   - Locale-specific sorting (e.g., German ß, Spanish ñ)
   - Emoji and multi-byte character sorting

3. **Conversion Tests**
   - UTF-8 ↔ Latin1
   - UTF-8 ↔ UTF-16
   - Lossy conversion handling
   - Error handling for unmappable characters

4. **Length Tests**
   - `LENGTH()` vs `CHAR_LENGTH()` vs `OCTET_LENGTH()`
   - `SUBSTRING()` with multi-byte characters
   - VARCHAR length limits (character vs byte)

## References

- SQL:2023 - Character Sets and Collations
- Unicode Standard 15.0
- Unicode Collation Algorithm (UCA)
- RFC 3629 - UTF-8 encoding
- PostgreSQL: Character Set Support
- MySQL: Character Sets and Collations
- ICU (International Components for Unicode)

## Related Specifications

### Timezone Support
See **[TIMEZONE_SYSTEM_CATALOG.md](TIMEZONE_SYSTEM_CATALOG.md)** for timezone handling:
- `TIMESTAMP WITH TIME ZONE` type support
- Timezone-aware timestamp parsing and formatting
- `AT TIME ZONE` operator
- `pg_timezone` system catalog table
- Connection and column-level timezone hints

Character sets and timezones work together:
- All timestamps stored in GMT (no charset conversion)
- Timezone names and abbreviations stored as UTF-8 strings
- Display formatting respects both charset and timezone settings

## Implementation Status

**Implemented (2025-10-04)**:
- ✅ Character set support (UTF-8, Latin1, ASCII, UTF-16, UTF-32, UTF8MB4)
- ✅ Collation support (15 collations including binary, case-insensitive, Unicode)
- ✅ UTF-8 validation and character length calculation
- ✅ Character set conversion (lossless and lossy)
- ✅ CharsetManager for centralized charset/collation management
- ✅ Column-level charset and collation metadata
- ✅ SBLR executor charset-aware string operations
- ✅ Parser support for CHARACTER SET and COLLATE clauses
- ✅ 30 comprehensive tests (all passing)

**Timezone Implementation**:
- ✅ Timezone system catalog (`pg_timezone`)
- ✅ CRUD operations for timezone management
- ✅ GMT storage with display hints
- ✅ DST rule support
- ✅ TimezoneManager with parsing/formatting
- ✅ Connection and column-level timezone context
- ✅ 22 comprehensive tests (all passing)

## SQL Identifier UTF-8 Support

**Implementation Date**: November 3, 2025
**Status**: COMPLETE ✅
**Reference**: docs/planning/SQL_IDENTIFIER_UTF8_FIX_PLAN.md

### Overview

ScratchBird fully supports UTF-8 identifiers (schema names, table names, column names, index names) with SQL standard compliance and proper multi-byte character handling.

### Limits

- **Character Limit**: 128 UTF-8 characters (SQL:2016 §5.2)
- **Byte Limit**: 512 bytes (storage capacity)
- **Encoding**: UTF-8 only

**Rationale**: SQL standard specifies 128-character maximum for identifiers. UTF-8 characters can be 1-4 bytes each, so 128 characters × 4 bytes = 512 bytes maximum storage requirement.

### Storage

Catalog records use fixed-size `char[512]` arrays to support:
- 128 ASCII characters (1 byte each = 128 bytes)
- 128 2-byte UTF-8 characters (256 bytes) - e.g., Latin-1 extended: é, ñ, ü
- 128 3-byte UTF-8 characters (384 bytes) - e.g., CJK: 你好, 日本語, 한글
- 128 4-byte UTF-8 characters (512 bytes maximum) - e.g., Emoji: 😀, 🎉

**Previous Implementation (BROKEN)**:
- Used `char[128]` arrays (only 128 bytes)
- Could only store 32 4-byte UTF-8 characters
- Multi-byte identifiers truncated or corrupted

**Fixed Implementation**:
- Uses `char[512]` arrays (512 bytes)
- Supports full 128-character SQL standard limit
- All UTF-8 characters handled correctly

### Validation

Identifiers are validated at two levels:

1. **UTF8Utils Validation** (`src/core/utf8_utils.cpp`)
   - Validates UTF-8 encoding correctness (RFC 3629)
   - Rejects overlong encodings (security)
   - Rejects UTF-16 surrogates (invalid in UTF-8)
   - Checks character count ≤ 128
   - Checks byte count ≤ 512

2. **Catalog Layer** (`src/core/catalog_manager.cpp`)
   - Calls UTF8Utils::writeToBuffer() for all identifiers
   - Ensures null-termination at position 511
   - Validates storage capacity before writing
   - Returns clear error messages on validation failure

### Examples

**Valid Identifiers**:
```sql
-- ASCII (4 characters, 5 bytes including 'é')
CREATE SCHEMA café;

-- Chinese (9 characters, 15 bytes: 6 ASCII + 6 bytes for 北京)
CREATE TABLE 北京_table (id INT);

-- Emoji (5 characters, 8 bytes: 4 ASCII + 4 bytes for 😀)
CREATE INDEX idx_😀 ON test_table (col);

-- Mixed UTF-8 (25 characters, ~40 bytes)
CREATE TABLE global_café_北京_😀 (
    id INT,
    name_名前 VARCHAR(100),
    status_😀 VARCHAR(50)
);

-- Maximum ASCII (128 characters, 128 bytes)
CREATE SCHEMA aaaaaaa...aaaaaa; -- 128 'a' characters

-- Maximum 2-byte chars (128 characters, 256 bytes)
CREATE SCHEMA ééééééé...éééééé; -- 128 'é' characters

-- Maximum 3-byte chars (128 characters, 384 bytes)
CREATE SCHEMA 你你你你...你你你你; -- 128 '你' characters
```

**Invalid Identifiers**:
```sql
-- Exceeds character limit (129 > 128)
CREATE SCHEMA aaaaaaa...aaaaaaa_EXTRA; -- 134 characters
-- ERROR: Identifier exceeds maximum length of 128 characters

-- Exceeds byte limit (130 × 4 = 520 bytes > 512)
CREATE SCHEMA 😀😀😀...😀😀😀; -- 130 emoji
-- ERROR: Identifier exceeds storage capacity of 512 bytes (520 bytes)

-- Empty identifier
CREATE TABLE "" (id INT);
-- ERROR: Identifier cannot be empty

-- Invalid UTF-8 sequence
CREATE TABLE "\xFF\xFE" (id INT);
-- ERROR: Invalid UTF-8 encoding in identifier
```

### Catalog Structure

All identifier fields in catalog records use `char[512]`:

```cpp
// Schema Record (src/core/catalog_manager.cpp)
struct SchemaRecord {
    uint64_t schema_id;
    char schema_name[512];  // 128 chars × 4 bytes/char max
    char owner[512];        // 128 chars × 4 bytes/char max
    // ...
};

// Table Record
struct TableRecord {
    uint64_t table_id;
    uint64_t schema_id;
    char table_name[512];   // 128 chars × 4 bytes/char max
    // ...
};

// Column Record
struct ColumnRecord {
    uint64_t table_id;
    uint16_t column_id;
    char column_name[512];  // 128 chars × 4 bytes/char max
    // ...
};

// Index Record
struct IndexRecord {
    uint64_t index_id;
    uint64_t table_id;
    char index_name[512];   // 128 chars × 4 bytes/char max
    // ...
};
```

### UTF-8 Utility Functions

**Core Functions** (`include/scratchbird/core/utf8_utils.h`):

```cpp
// Count UTF-8 characters (not bytes)
static size_t countCharacters(const std::string& str);

// Validate UTF-8 encoding
static bool isValidUTF8(const std::string& str);

// Truncate to byte boundary (preserves character integrity)
static std::string truncateToBytes(const std::string& str, size_t max_bytes);

// Validate identifier (128 char limit, valid UTF-8)
static Status isValidIdentifier(const std::string& str, ErrorContext* ctx);

// Validate storage capacity (char and byte limits)
static Status validateStorageCapacity(
    const std::string& str,
    size_t max_chars,
    size_t max_bytes,
    ErrorContext* ctx
);

// Write to buffer with validation
static Status writeToBuffer(
    const std::string& str,
    char* buffer,
    size_t buffer_size,
    size_t max_chars,
    size_t max_bytes,
    ErrorContext* ctx
);
```

### Character Boundary Integrity

**Critical**: Multi-byte UTF-8 characters must never be split.

Example:
```cpp
// "你好世界" = 4 Chinese chars, 12 bytes (3 bytes each)

// WRONG: Truncate at byte 10 (splits "世" character)
std::string wrong = str.substr(0, 10); // CORRUPTED UTF-8

// RIGHT: Truncate at character boundary
std::string correct = UTF8Utils::truncateToBytes("你好世界", 10);
// Returns: "你好世" (9 bytes, respects character boundary)
```

All UTF-8 functions preserve character boundaries to prevent corruption.

### SQL Parser Integration

**Quoted Identifiers** (case-sensitive):
```sql
-- UTF-8 identifiers must be quoted
CREATE SCHEMA "café_schema";    -- Preserves exact case and UTF-8
CREATE TABLE "北京" (id INT);    -- Chinese characters
CREATE INDEX "idx_😀" ON t (c);  -- Emoji characters
```

**Unquoted Identifiers** (normalized to lowercase):
```sql
-- ASCII only, normalized to lowercase
CREATE SCHEMA MySchema;  -- Stored as: myschema
CREATE TABLE Users (id INT); -- Stored as: users
```

**Note**: Non-ASCII characters in unquoted identifiers may require parser updates (future enhancement).

### Testing

**Test Coverage**: 86 test cases (1,868 lines)

1. **Unit Tests** (`tests/unit/test_utf8_utils.cpp`)
   - 38 tests for UTF8Utils functions
   - Character counting, validation, truncation
   - All character sets: ASCII, Latin-1, CJK, Emoji

2. **Integration Tests** (`tests/integration/test_catalog_utf8_identifiers.cpp`)
   - 22 tests for catalog layer
   - Schema, table, column, index creation
   - Round-trip persistence verification

3. **SQL Tests** (`tests/sql/test_utf8_identifiers.sql`)
   - 26 SQL scenarios
   - Real-world use cases
   - Error cases and boundary conditions

**Character Sets Tested**:
- ASCII, Latin-1 (é, ñ, ü)
- Chinese (你好), Japanese (日本語), Korean (한글)
- Emoji (😀, 🎉, 💯)
- Cyrillic (данные), Arabic (بيانات)
- Mixed scripts

### Migration Notes

**Breaking Change**: Catalog format change (128 bytes → 512 bytes)

**Impact**: Existing databases created before November 2025 are incompatible.

**Migration Path**:
1. Export data: `scratchbird dump mydb > backup.sql`
2. Create new database with updated version
3. Import data: `scratchbird restore mydb < backup.sql`

**Rationale**: This change fixes critical data corruption bugs with multi-byte UTF-8 identifiers and ensures SQL standard compliance.

### Performance Impact

**Catalog Storage**: +384 bytes per identifier field
- Schema record: 128→512 bytes (2 fields) = +768 bytes
- Table record: 128→512 bytes (1 field) = +384 bytes
- Column record: 128→512 bytes (1 field) = +384 bytes
- Index record: 128→512 bytes (1 field) = +384 bytes

**Memory Impact**: Minimal (catalog cached in memory)

**Validation Overhead**: Negligible
- UTF-8 validation: O(n) where n = byte length
- Character counting: O(n) where n = byte length
- Typical identifier: <50 bytes, <1μs validation time

### Implementation Phases

- ✅ **Phase 1**: UTF8Utils enhancements (truncateToBytes, validateStorageCapacity)
- ✅ **Phase 2**: Catalog storage expansion (char[128] → char[512])
- ✅ **Phase 3**: Catalog write logic fixes (strncpy → UTF-8 validation)
- ✅ **Phase 4**: Catalog read logic safety (defensive null-termination)
- ✅ **Phase 5**: Testing & validation (86 test cases)
- ✅ **Phase 6**: Documentation (this section)

### Related Issues Fixed

1. **Multi-byte identifier truncation** - Fixed by 512-byte storage
2. **strncpy data corruption** - Fixed by UTF-8 aware writing
3. **Missing character count validation** - Fixed by UTF8Utils::isValidIdentifier
4. **Missing byte capacity validation** - Fixed by validateStorageCapacity
5. **No round-trip testing** - Fixed by comprehensive integration tests

### References

- SQL:2016 §5.2 - Identifier length limits (128 characters)
- RFC 3629 - UTF-8, a transformation format of ISO 10646
- Unicode Standard 15.0 - Character encoding
- docs/planning/SQL_IDENTIFIER_UTF8_FIX_PLAN.md - Implementation plan
- docs/status/PHASE1_UTF8_UTILS_ENHANCEMENTS_COMPLETE.md - Phase 1 status
- docs/status/PHASE2_CATALOG_STORAGE_EXPANSION_COMPLETE.md - Phase 2 status
- docs/status/PHASE3_CATALOG_WRITE_LOGIC_FIXES_COMPLETE.md - Phase 3 status
- docs/status/PHASE4_CATALOG_READ_SAFETY_COMPLETE.md - Phase 4 status
- docs/status/PHASE5_SQL_UTF8_TESTING_COMPLETE.md - Phase 5 status

## Notes

- All system catalog objects (table names, column names, etc.) must be UTF-8
- **SQL identifiers now support full 128-character UTF-8 names (512-byte storage)**
- String literals in SQL are assumed to be in the connection character set
- Client applications should send/receive data in UTF-8 by default
- Binary data (BLOB, BYTEA) is NOT affected by character sets
- **For timezone handling**: All timestamps stored in GMT, timezone affects display only
