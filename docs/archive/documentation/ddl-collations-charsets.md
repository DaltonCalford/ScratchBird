### DDL: Collations and Character Sets

**What it is**

Collations define how text data is compared and sorted, while character sets (charsets) determine how characters are encoded and stored. Together, they control text representation, comparison rules, and sorting behavior for different languages and locales. ScratchBird supports multiple collations and character sets for internationalization and specialized text handling.

**Why it matters**

- **Internationalization**: Proper sorting and comparison for different languages
- **Case Sensitivity**: Control case-sensitive vs case-insensitive comparisons
- **Performance**: Appropriate collations can optimize index usage
- **Data Integrity**: Correct character encoding prevents data corruption
- **Compliance**: Meet regional and linguistic requirements

**How to use it**

Specify character sets for proper encoding of international text. Apply collations to control sorting and comparison behavior. Use at the database, table, column, or expression level depending on granularity needed.

## Character Sets

### Understanding Character Sets

Character sets define the encoding of characters to bytes:
- **ASCII**: 7-bit, English characters only
- **UTF8/UTF-8**: Variable-width Unicode, supports all languages
- **LATIN1/ISO-8859-1**: Western European languages
- **WIN1252**: Windows Western European
- **UTF16**: Fixed/variable width Unicode
- **UNICODE_FSS**: Firebird Unicode

### CREATE CHARACTER SET

```sql
-- Basic character set creation
CREATE CHARACTER SET my_charset;

-- With specific attributes (captured raw)
CREATE CHARACTER SET app_charset
    DEFAULT COLLATION app_collation;

-- Based on existing charset
CREATE CHARACTER SET custom_utf8
    AS UTF8;
```

### Built-in Character Sets

```sql
-- Common character sets recognized by lexer
-- (from src/engine/lexer.cpp)

-- Unicode variants
'UTF8', 'UTF-8'      -- Standard Unicode
'UNICODE_FSS'        -- Firebird Unicode

-- Single-byte encodings
'ASCII'              -- 7-bit ASCII
'OCTETS'             -- Binary data

-- Windows code pages
'WIN1250'            -- Central European
'WIN1251'            -- Cyrillic
'WIN1252'            -- Western European
'WIN1253'            -- Greek
'WIN1254'            -- Turkish

-- ISO encodings
'ISO8859_1'          -- Western European
'ISO8859_2'          -- Central European
'ISO8859_5'          -- Cyrillic
'ISO8859_7'          -- Greek
'ISO8859_9'          -- Turkish

-- Asian encodings
'SJIS'               -- Shift-JIS (Japanese)
'EUCJIS'             -- EUC-JP (Japanese)
'BIG_5'              -- Traditional Chinese
'GBK'                -- Simplified Chinese
'GB18030'            -- Chinese (full)

-- Cyrillic
'KOI8R'              -- Russian
'KOI8U'              -- Ukrainian
```

### Using Character Sets

#### Database Level

```sql
-- Create database with default charset
CREATE DATABASE international_db
    DEFAULT CHARACTER SET UTF8;

-- Alter database charset
ALTER DATABASE mydb
    SET DEFAULT CHARACTER SET UTF8;
```

#### Table Level

```sql
-- Table with default charset
CREATE TABLE international_data (
    id INTEGER PRIMARY KEY,
    content TEXT
) DEFAULT CHARACTER SET UTF8;

-- Mixed charsets in one table
CREATE TABLE mixed_encoding (
    id INTEGER PRIMARY KEY,
    english_text VARCHAR(100) CHARACTER SET ASCII,
    japanese_text TEXT CHARACTER SET SJIS,
    russian_text TEXT CHARACTER SET WIN1251,
    universal_text TEXT CHARACTER SET UTF8
);
```

#### Column Level

```sql
-- Specify charset per column
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100) CHARACTER SET UTF8,
    code VARCHAR(20) CHARACTER SET ASCII,
    description TEXT CHARACTER SET UTF8
);

-- National character notation
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    name NVARCHAR(100),  -- National character varying (Unicode)
    bio NTEXT            -- National text (Unicode)
);
```

#### String Literals

```sql
-- Charset-prefixed strings (from lexer)
INSERT INTO international_data (content) VALUES
    (_UTF8'Hello, 世界'),
    (_ASCII'Plain ASCII text'),
    (_WIN1252'Windows específico'),
    (N'National Unicode text');

-- Introducer syntax
SELECT * FROM users
WHERE name = _UTF8'José García'
   OR name = _ISO8859_1'José García';
```

## Collations

### Understanding Collations

Collations determine:
- **Sort Order**: How strings are ordered
- **Comparison**: How equality is determined
- **Case Sensitivity**: Whether 'A' = 'a'
- **Accent Sensitivity**: Whether 'é' = 'e'
- **Locale Rules**: Language-specific sorting

### CREATE COLLATION

```sql
-- Basic collation
CREATE COLLATION my_collation (
    LOCALE = 'en_US'
);

-- From external source (ICU)
CREATE COLLATION icu_german 
    FROM EXTERNAL 'icu:de_DE';

-- Based on existing collation
CREATE COLLATION case_insensitive
    FROM default_collation
    CASE INSENSITIVE;

-- With specific attributes
CREATE COLLATION custom_collation (
    LOCALE = 'fr_FR',
    CASE_SENSITIVE = FALSE,
    ACCENT_SENSITIVE = TRUE,
    PROVIDER = 'icu'
);
```

### Built-in Collations

```sql
-- Common collations

-- Default/Binary
'C'                  -- Byte-value ordering
'POSIX'              -- Same as C
'binary'             -- Binary comparison

-- Case variations
'unicode'            -- Unicode default
'unicode_ci'         -- Case-insensitive
'unicode_ci_ai'      -- Case and accent insensitive

-- Language-specific
'en_US'              -- US English
'en_GB'              -- British English
'de_DE'              -- German (phone book)
'de_DE@collation=phonebook'
'fr_FR'              -- French
'es_ES'              -- Spanish (traditional)
'es_ES@collation=traditional'
'ja_JP'              -- Japanese
'zh_CN'              -- Simplified Chinese
'ru_RU'              -- Russian
```

### Using Collations

#### Database Level

```sql
-- Set default collation
CREATE DATABASE mydb
    LC_COLLATE = 'en_US.UTF-8'
    LC_CTYPE = 'en_US.UTF-8';

-- Alter database collation (limited)
ALTER DATABASE mydb SET default_collation = 'unicode_ci';
```

#### Table Level

```sql
-- Table with default collation
CREATE TABLE names (
    id INTEGER PRIMARY KEY,
    name VARCHAR(100)
) COLLATE unicode_ci;
```

#### Column Level

```sql
-- Different collations per column
CREATE TABLE international_names (
    id INTEGER PRIMARY KEY,
    english_name VARCHAR(100) COLLATE "en_US",
    german_name VARCHAR(100) COLLATE "de_DE",
    french_name VARCHAR(100) COLLATE "fr_FR",
    japanese_name VARCHAR(100) COLLATE "ja_JP"
);

-- Case-insensitive columns
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    username VARCHAR(50) COLLATE "unicode_ci" UNIQUE,
    email VARCHAR(100) COLLATE "unicode_ci" UNIQUE,
    password_hash VARCHAR(255) COLLATE "C"  -- Binary comparison
);
```

#### Expression Level

```sql
-- Collation in queries
SELECT * FROM users
WHERE username COLLATE "unicode_ci" = 'JohnDoe';

-- Different collation for sorting
SELECT name 
FROM customers
ORDER BY name COLLATE "de_DE";  -- German phone book order

-- Collation in joins
SELECT *
FROM users u
JOIN profiles p ON u.username COLLATE "unicode_ci" = p.username COLLATE "unicode_ci";
```

## Collation Examples

### Case-Insensitive Searches

```sql
-- Create case-insensitive collation
CREATE COLLATION ci_collation 
    FROM unicode 
    CASE_INSENSITIVE;

-- Table using case-insensitive collation
CREATE TABLE products (
    id INTEGER PRIMARY KEY,
    name VARCHAR(200) COLLATE ci_collation,
    sku VARCHAR(50) COLLATE "C"  -- Exact match required
);

-- These would match with ci_collation
INSERT INTO products (name, sku) VALUES 
    ('Widget Pro', 'WGT-001'),
    ('WIDGET PRO', 'WGT-002');  -- Would violate unique if name was unique

SELECT * FROM products 
WHERE name = 'widget pro';  -- Matches both rows
```

### Language-Specific Sorting

```sql
-- German collation (ß sorts as 'ss')
CREATE TABLE german_words (
    word VARCHAR(100) COLLATE "de_DE"
);

INSERT INTO german_words VALUES 
    ('Müller'), ('Mueller'), ('Masse'), ('Maße');

SELECT word FROM german_words ORDER BY word;
-- Result order: Masse, Maße, Mueller, Müller

-- Spanish traditional (ch and ll as single letters)
CREATE TABLE spanish_words (
    word VARCHAR(100) COLLATE "es_ES@collation=traditional"
);

INSERT INTO spanish_words VALUES 
    ('luz'), ('llama'), ('lobo');

SELECT word FROM spanish_words ORDER BY word;
-- Result: lobo, luz, llama (ll comes after l)
```

### Accent-Sensitive vs Insensitive

```sql
-- Accent-insensitive collation
CREATE COLLATION ai_collation
    ACCENT_INSENSITIVE;

-- Accent-sensitive (default)
CREATE TABLE cities_accent_sensitive (
    name VARCHAR(100) COLLATE "unicode"
);

-- Accent-insensitive
CREATE TABLE cities_accent_insensitive (
    name VARCHAR(100) COLLATE ai_collation
);

-- Different behavior
INSERT INTO cities_accent_sensitive VALUES ('São Paulo'), ('Sao Paulo');
INSERT INTO cities_accent_insensitive VALUES ('São Paulo');
-- This would fail if unique: INSERT INTO cities_accent_insensitive VALUES ('Sao Paulo');

SELECT * FROM cities_accent_sensitive WHERE name = 'Sao Paulo';  -- No match
SELECT * FROM cities_accent_insensitive WHERE name = 'Sao Paulo';  -- Matches 'São Paulo'
```

## ALTER COLLATION

```sql
-- Rename collation
ALTER COLLATION old_name RENAME TO new_name;

-- Change owner
ALTER COLLATION my_collation OWNER TO new_owner;

-- Refresh version (ICU collations)
ALTER COLLATION icu_collation REFRESH VERSION;
```

## ALTER CHARACTER SET

```sql
-- Rename character set
ALTER CHARACTER SET old_charset RENAME TO new_charset;

-- Change default collation
ALTER CHARACTER SET my_charset 
    SET DEFAULT COLLATION my_collation;
```

## DROP COLLATION/CHARACTER SET

```sql
-- Drop collation
DROP COLLATION my_collation;
DROP COLLATION IF EXISTS temp_collation;
DROP COLLATION unused_collation CASCADE;

-- Drop character set
DROP CHARACTER SET my_charset;
DROP CHARACTER SET IF EXISTS temp_charset;
DROP CHARACTER SET old_charset CASCADE;
```

## Collation and Index Performance

### Index Optimization

```sql
-- Indexes use column collation
CREATE TABLE users (
    username VARCHAR(50) COLLATE "unicode_ci"
);

-- Index uses unicode_ci collation
CREATE INDEX idx_username ON users(username);

-- This query uses the index
SELECT * FROM users WHERE username = 'JohnDoe';

-- This query might not use the index (different collation)
SELECT * FROM users WHERE username COLLATE "C" = 'JohnDoe';

-- Create specific index for binary searches
CREATE INDEX idx_username_binary ON users(username COLLATE "C");
```

### Pattern Matching

```sql
-- Pattern matching with collations
CREATE TABLE documents (
    title VARCHAR(200) COLLATE "unicode_ci",
    content TEXT COLLATE "unicode_ci"
);

-- Case-insensitive LIKE
SELECT * FROM documents 
WHERE title LIKE '%IMPORTANT%';  -- Matches 'Important', 'important', etc.

-- Force case-sensitive
SELECT * FROM documents 
WHERE title COLLATE "C" LIKE '%IMPORTANT%';  -- Only matches exact case
```

## Conversion and Migration

### Character Set Conversion

```sql
-- Convert column charset
ALTER TABLE old_table 
    ALTER COLUMN text_column TYPE VARCHAR(500) CHARACTER SET UTF8;

-- Convert entire table
ALTER TABLE legacy_table 
    CONVERT TO CHARACTER SET UTF8 COLLATE unicode_ci;

-- Explicit conversion in queries
SELECT CONVERT(text_column USING UTF8) 
FROM legacy_table;
```

### Collation Changes

```sql
-- Change column collation
ALTER TABLE users 
    ALTER COLUMN username TYPE VARCHAR(50) COLLATE "unicode_ci";

-- Rebuild indexes after collation change
REINDEX TABLE users;

-- Temporary collation change
SELECT DISTINCT name COLLATE "C" 
FROM products 
ORDER BY name COLLATE "C";
```

## Best Practices

### Choosing Character Sets

```sql
-- Use UTF8 for international applications
CREATE DATABASE international_app
    DEFAULT CHARACTER SET UTF8;

-- Use ASCII for codes and identifiers
CREATE TABLE system_codes (
    code VARCHAR(20) CHARACTER SET ASCII PRIMARY KEY,
    description TEXT CHARACTER SET UTF8
);

-- Use OCTETS for binary data
CREATE TABLE files (
    id INTEGER PRIMARY KEY,
    filename VARCHAR(255) CHARACTER SET UTF8,
    content BLOB CHARACTER SET OCTETS
);
```

### Choosing Collations

```sql
-- Case-insensitive for user-facing data
CREATE TABLE user_content (
    title VARCHAR(200) COLLATE "unicode_ci",
    slug VARCHAR(200) COLLATE "unicode_ci" UNIQUE
);

-- Binary for system identifiers
CREATE TABLE api_keys (
    key VARCHAR(64) COLLATE "C" PRIMARY KEY,
    description TEXT COLLATE "unicode_ci"
);

-- Locale-specific for regional data
CREATE TABLE german_products (
    name VARCHAR(200) COLLATE "de_DE",
    description TEXT COLLATE "de_DE"
);
```

### Performance Considerations

```sql
-- Consistent collations for joins
-- Good: Same collation
SELECT * FROM users u
JOIN profiles p ON u.id = p.user_id
WHERE u.name COLLATE "unicode_ci" = p.name COLLATE "unicode_ci";

-- Bad: Mixed collations (requires conversion)
SELECT * FROM users u
JOIN profiles p ON u.id = p.user_id
WHERE u.name COLLATE "unicode_ci" = p.name COLLATE "C";

-- Index-friendly queries
-- Use column's default collation when possible
SELECT * FROM users WHERE username = 'john';  -- Uses index

-- Avoid collation overrides in WHERE
SELECT * FROM users WHERE username COLLATE "C" = 'john';  -- Might skip index
```

## Complex Examples

### Multi-Language Application

```sql
-- Comprehensive international schema
CREATE SCHEMA international;

-- Language-specific tables
CREATE TABLE international.translations (
    id INTEGER PRIMARY KEY,
    language_code CHAR(2) CHARACTER SET ASCII,
    -- Different collations per language
    text_en TEXT CHARACTER SET UTF8 COLLATE "en_US",
    text_de TEXT CHARACTER SET UTF8 COLLATE "de_DE",
    text_fr TEXT CHARACTER SET UTF8 COLLATE "fr_FR",
    text_ja TEXT CHARACTER SET UTF8 COLLATE "ja_JP",
    text_ar TEXT CHARACTER SET UTF8 COLLATE "ar_SA"
);

-- Search across languages
CREATE FUNCTION search_translations(search_term TEXT)
RETURNS TABLE(id INTEGER, language CHAR(2), content TEXT) AS $$
BEGIN
    RETURN QUERY
    SELECT id, 'en', text_en FROM international.translations 
        WHERE text_en ILIKE '%' || search_term || '%'
    UNION ALL
    SELECT id, 'de', text_de FROM international.translations 
        WHERE text_de ILIKE '%' || search_term || '%'
    -- ... other languages
    ;
END;
$$ LANGUAGE plpgsql;
```

### Collation-Aware Full-Text Search

```sql
-- Create text search configuration per language
CREATE TEXT SEARCH CONFIGURATION german (COPY = german);
CREATE TEXT SEARCH CONFIGURATION french (COPY = french);

-- Multi-language content table
CREATE TABLE articles (
    id INTEGER PRIMARY KEY,
    language CHAR(2),
    title VARCHAR(500) CHARACTER SET UTF8,
    content TEXT CHARACTER SET UTF8,
    -- Appropriate collation based on language
    CONSTRAINT chk_collation CHECK (
        CASE language
            WHEN 'de' THEN title COLLATE "de_DE" IS NOT NULL
            WHEN 'fr' THEN title COLLATE "fr_FR" IS NOT NULL
            ELSE title COLLATE "en_US" IS NOT NULL
        END
    )
);

-- Language-specific indexes
CREATE INDEX idx_articles_de ON articles(title COLLATE "de_DE") 
    WHERE language = 'de';
CREATE INDEX idx_articles_fr ON articles(title COLLATE "fr_FR") 
    WHERE language = 'fr';
```

## Implementation Details

**Parser Implementation** (`src/engine/parser_ddl.cpp`):
- `parse_ddl_collation`: Handles CREATE/ALTER collation
- `parse_ddl_charset`: Handles CREATE/ALTER character set
- Captures FROM EXTERNAL for ICU collations
- Stores raw attributes for flexibility

**AST Structure** (`include/scratchbird/engine/ast.h`):
```cpp
struct DdlCollationAst {
    std::string name;
    std::string based_on;      // Base collation
    std::string from_external; // ICU reference
    std::string attributes;    // Raw attributes
};

struct DdlCharsetAst {
    std::string name;
    std::string attributes;    // Raw attributes
    std::string default_collation;
};
```

**Lexer Support** (`src/engine/lexer.cpp`):
- Recognizes charset prefixes: `_CHARSET'string'`, `N'string'`
- Validates known character sets
- Generates warnings for unknown charsets

**Code Anchors**:
- Collation parser: `src/engine/parser_ddl.cpp` (parse_ddl_collation)
- Charset parser: `src/engine/parser_ddl.cpp` (parse_ddl_charset)
- Charset recognition: `src/engine/lexer.cpp` (charset prefix handling)
- AST definitions: `include/scratchbird/engine/ast.h`

## See also

- [Lexical Structure](./sql-lexical.md) - Character set prefixes in strings
- [Data Types](./sql-data-types.md) - Text types with charset/collation
- [Tables](./ddl-tables.md) - Column-level charset/collation
- [Domains](./ddl-domains.md) - Domains with collation
- [Indexes](./ddl-indexes.md) - Collation effects on indexing