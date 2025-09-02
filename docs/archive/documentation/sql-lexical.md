### Lexical Structure and Literals

**What it is**

The lexical analyzer (lexer) is the first stage of SQL processing in ScratchBird. It transforms raw SQL text into a stream of tokens - the basic building blocks that the parser uses to construct abstract syntax trees. The lexer handles everything from simple identifiers to complex string literals with character set prefixes.

**Why it matters**

- **Syntax Foundation**: Understanding token rules helps you write valid SQL and avoid common pitfalls
- **String Handling**: Learn the various string literal formats including dollar-quoting and charset prefixes
- **Diagnostics**: Lexer warnings catch issues early, like unknown character set tags
- **Special Literals**: Know how DATE, TIME, TIMESTAMP, and UUID literals are recognized

**How to use it**

This page documents all token types and recognition rules. Use these patterns to construct valid SQL literals, identifiers, and expressions. When debugging parsing issues, check lexer warnings via the `lexer_warnings()` API.

## Token Types

The lexer (`include/scratchbird/engine/lexer.h`) recognizes these token kinds:

```cpp
enum class TokenKind {
    Identifier,       // Regular identifiers: table_name, columnName
    QuotedIdentifier, // Quoted identifiers: "Table Name", "SELECT"
    Integer,          // Integer literals: 42, -17
    Decimal,          // Decimal literals: 3.14, -0.5, 1.23E-4
    String,           // String literals: 'text', $$text$$
    Date,             // Date literals: DATE '2024-01-15'
    Time,             // Time literals: TIME '14:30:00'
    Timestamp,        // Timestamp literals: TIMESTAMP '2024-01-15 14:30:00'
    Uuid,             // UUID literals: UUID 'a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11'
    Symbol,           // Operators and punctuation: +, -, *, /, ::, !=
    Keyword,          // Reserved words: SELECT, FROM, WHERE
    End               // End of input marker
};
```

## Whitespace and Comments

**Whitespace**: Spaces, tabs, newlines, and carriage returns are skipped between tokens.

**Comments**: Two styles are supported:
- **Line comments**: Start with `--` and continue to end of line
- **Block comments**: Enclosed in `/* */`, can span multiple lines, no nesting

```sql
-- This is a line comment
SELECT id, /* inline comment */ name
FROM users /* This comment
   spans multiple
   lines */ WHERE active = true;
```

## Identifiers

### Regular Identifiers
- Start with a letter (a-z, A-Z) or underscore (_)
- Continue with letters, digits (0-9), underscores (_), or dollar signs ($)
- Case-insensitive by default (normalized to uppercase internally)
- Maximum length depends on implementation (typically 63 characters)

```sql
-- Valid identifiers
table_name
_private_var
user$id
table123
CamelCase  -- treated same as CAMELCASE
```

### Quoted Identifiers
- Enclosed in double quotes: `"identifier"`
- Preserve exact case and can contain any characters except NUL
- Allow using reserved words as identifiers
- Quotes are escaped by doubling: `""`

```sql
-- Quoted identifiers preserve case and allow special characters
"Table Name"
"SELECT"  -- reserved word as identifier
"user@domain.com"
"Column""With""Quotes"  -- contains literal quotes
```

## String Literals

ScratchBird supports multiple string literal formats:

### Standard SQL Strings
- Enclosed in single quotes: `'text'`
- Single quotes escaped by doubling: `''`
- Can span multiple lines

```sql
'Simple string'
'String with ''quotes'' inside'
'Multi-line
string literal'
```

### Dollar-Quoted Strings
- Format: `$tag$text$tag$` where tag is optional
- No escaping needed inside
- Tag can contain letters, digits, underscores
- Useful for embedding SQL or avoiding quote escaping

```sql
$$Simple dollar string$$
$tag$String with 'quotes' and "double quotes"$tag$
$sql$
  SELECT * FROM users WHERE name = 'John'
$sql$
```

### Character Set Prefixed Strings
- National character: `N'text'`
- Explicit charset: `_UTF8'text'`, `_ASCII'text'`
- Sets the `charset` field on the token
- Unknown charsets generate warnings

```sql
N'National character string'
_UTF8'Unicode text: 你好'
_WIN1252'Windows encoded'
_ISO8859_1'Latin-1 text'
```

**Recognized Character Sets** (from `src/engine/lexer.cpp`):
- Standard: `UTF8`, `UTF-8`, `ASCII`, `UNICODE_FSS`, `OCTETS`
- Windows: `WIN*` (e.g., WIN1252, WIN1251)
- ISO: `ISO*` (e.g., ISO8859_1, ISO8859_15)
- Asian: `SJIS`, `EUCJIS`, `BIG_5`, `GBK`, `GB18030`
- Cyrillic: `KOI8*` (e.g., KOI8R, KOI8U)

## Numeric Literals

### Integers
- Sequence of digits: `42`, `0`, `999999`
- Optional sign handled by parser: `-42`, `+17`

### Decimals
- Digits with decimal point: `3.14`, `0.5`
- Scientific notation: `1.23E10`, `4.56e-7`
- Leading decimal point: `.5` (equivalent to `0.5`)

```sql
SELECT 
    42,           -- integer
    3.14159,      -- decimal
    1.23E10,      -- scientific notation
    .5,           -- leading decimal
    123.456E-7    -- decimal with negative exponent
FROM dual;
```

## Temporal Literals

Temporal literals are recognized when a string literal immediately follows the appropriate keyword:

### DATE Literals
- Format: `DATE 'YYYY-MM-DD'` or `DATE 'DD-MM-YYYY'` or `DATE 'MM-DD-YYYY'`
- Separator can be `-` or `/`
- Validated for basic format (not date validity)

```sql
DATE '2024-01-15'
DATE '15-01-2024'
DATE '01/15/2024'
```

### TIME Literals
- Format: `TIME 'HH:MM[:SS[.fff]][TZ]'`
- Optional seconds and fractional seconds
- Optional timezone: `Z` for UTC, or `±HH[:MM]` offset

```sql
TIME '14:30'
TIME '14:30:45'
TIME '14:30:45.123'
TIME '14:30:45Z'
TIME '14:30:45+05:30'
TIME '14:30:45-08:00'
```

### TIMESTAMP Literals
- Format: `TIMESTAMP 'YYYY-MM-DD HH:MM:SS[.fff][TZ]'`
- Combines date and time formats
- Optional fractional seconds and timezone

```sql
TIMESTAMP '2024-01-15 14:30:45'
TIMESTAMP '2024-01-15 14:30:45.123'
TIMESTAMP '2024-01-15 14:30:45.123456Z'
TIMESTAMP '2024-01-15 14:30:45+05:30'
```

### UUID Literals
- Format: `UUID 'xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx'`
- Standard UUID format with hyphens
- Case-insensitive hex digits

```sql
UUID 'a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11'
UUID 'A0EEBC99-9C0B-4EF8-BB6D-6BB9BD380A11'  -- uppercase also valid
```

## Operators and Symbols

The lexer recognizes both single and multi-character operators:

### Single Character Symbols
```sql
+ - * / % ( ) [ ] { } , . ; : ? @ # $ & | ^ ~ < > =
```

### Multi-Character Symbols
```sql
!=  <>  -- not equal
<=  >=  -- comparison
::      -- cast operator
||      -- concatenation
--      -- comment (special handling)
/*      -- block comment start
*/      -- block comment end
```

## Keywords

Keywords are case-insensitive identifiers with special meaning. The lexer uses two keyword sources:

1. **Generated keywords** (`include/scratchbird/engine/keywords_generated.h`) - Primary source
2. **Fallback keywords** - Used when generated list unavailable

### Core Keywords (Fallback Set)
```sql
SELECT, FROM, WHERE, JOIN, ON, AS, AND, OR, NOT, 
IN, EXISTS, BETWEEN, LIKE, IS, NULL, TRUE, FALSE,
INSERT, INTO, VALUES, UPDATE, SET, DELETE,
CREATE, ALTER, DROP, TABLE, INDEX, VIEW,
ORDER, BY, GROUP, HAVING, UNION, INTERSECT, EXCEPT,
WITH, RECURSIVE, DISTINCT, ALL, ANY, SOME,
CASE, WHEN, THEN, ELSE, END, CAST, COLLATE
```

To use a keyword as an identifier, quote it:
```sql
CREATE TABLE "SELECT" (  -- SELECT as table name
    "FROM" INTEGER,       -- FROM as column name
    "WHERE" VARCHAR(100)  -- WHERE as column name
);
```

## Lexer Warnings and Diagnostics

The lexer accumulates thread-local warnings accessible via `lexer_warnings()`:

### Unknown Character Set Warning
```sql
_UNKNOWN_CHARSET'text'  -- Generates: "Unknown charset tag: UNKNOWN_CHARSET"
```

### Recovery from Errors
The lexer attempts to continue after encountering issues:
- Unknown tokens become symbols
- Malformed strings are recovered where possible
- Invalid numeric formats fall back to identifiers

## Examples

### Complex String Literals
```sql
-- Various string formats in one query
SELECT 
    'Simple string' as standard,
    'String with ''quotes''' as escaped,
    $$Dollar quoted with 'quotes' and "double"$$ as dollar,
    $tag$
        Multi-line text
        with special chars: @#$%
    $tag$ as tagged,
    N'National: Zürich' as national,
    _UTF8'Unicode: 你好世界' as utf8_text
FROM dual;
```

### Temporal Literal Examples
```sql
-- Date/time literals with various formats
INSERT INTO events (event_date, event_time, created_at, uuid)
VALUES (
    DATE '2024-01-15',
    TIME '14:30:45.123Z',
    TIMESTAMP '2024-01-15 14:30:45.123456+05:30',
    UUID 'a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11'
);
```

### Mixed Token Types
```sql
-- Query demonstrating various token types
WITH RECURSIVE /* block comment */ hierarchy AS (
    SELECT id,                    -- Identifier
           "Parent ID",           -- Quoted identifier
           level::INTEGER,        -- Cast with :: symbol
           DATE '2024-01-01',     -- Date literal
           1.5E2 as score,        -- Scientific notation
           N'Department' as name  -- National string
    FROM departments
    WHERE parent_id IS NULL      -- Keywords and operators
      AND active != false         -- Multi-char operator
)
SELECT * FROM hierarchy;
```

## Implementation Details

**Source Files**:
- Header: `include/scratchbird/engine/lexer.h` - Token types and Lexer class
- Implementation: `src/engine/lexer.cpp` - Tokenization logic
- Keywords: `include/scratchbird/engine/keywords_generated.h` - Generated keyword list

**Key Functions**:
- `Lexer::lex()` - Main tokenization loop
- `lex_string()` - Handle quoted strings
- `lex_dollar_string()` - Handle dollar-quoted strings  
- `lex_ident_or_kw()` - Distinguish identifiers from keywords
- `lex_number()` - Parse numeric literals
- `lex_symbol()` - Handle operators and punctuation

**Thread Safety**: Lexer warnings are stored in thread-local storage, making the lexer thread-safe for parallel parsing.

## See also

- [Operators](./sql-operators.md) - Operator precedence and evaluation
- [Data Types](./sql-data-types.md) - Type specifications and literals
- [Reserved Words](./sql-reserved-words.md) - Complete keyword reference
- [SQL Overview](./sql-overview.md) - Overall language structure