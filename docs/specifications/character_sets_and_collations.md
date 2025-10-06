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

## Notes

- All system catalog objects (table names, column names, etc.) must be UTF-8
- String literals in SQL are assumed to be in the connection character set
- Client applications should send/receive data in UTF-8 by default
- Binary data (BLOB, BYTEA) is NOT affected by character sets
- **For timezone handling**: All timestamps stored in GMT, timezone affects display only
