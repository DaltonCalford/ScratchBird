# Parsers and Emulation

**Purpose:** Documents ScratchBird's SQL parser architecture - how each dialect is handled by its own parser that generates SBLR bytecode.

**Status:** Alpha documentation (in progress)

---

## Overview

ScratchBird supports multiple SQL dialects through isolated parser implementations. Each parser:

1. **Parses** dialect-specific SQL syntax
2. **Generates** dialect-agnostic SBLR bytecode
3. **Formats** results back to dialect-specific format

```
┌─────────────────────────────────────────────────────────────┐
│                     PARSER LAYER                             │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐          │
│  │ PostgreSQL  │  │   MySQL     │  │  Firebird   │          │
│  │   Parser    │  │   Parser    │  │   Parser    │          │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘          │
│         │                │                │                  │
│         └────────────────┴────────────────┘                  │
│                          │                                   │
│                          ▼                                   │
│                    SBLR Bytecode                             │
│                  (dialect-agnostic)                          │
└─────────────────────────────────────────────────────────────┘
```

---

## Critical Rules

### Rule 1: Emulated parsers are COMPLETELY SEPARATE from V2 parser

```cpp
// WRONG - mixing dialects
if (emulating_mysql) {
    parse_mysql_syntax();
}

// CORRECT - separate parser files
src/parser/mysql/mysql_parser.cpp    // MySQL parser
src/parser/postgresql/pg_parser.cpp  // PostgreSQL parser
src/parser/firebird/firebird_parser.cpp // Firebird parser
src/parser/parser_v2.cpp             // Native V2 parser
```

### Rule 2: Emulated parsers must NOT expose ScratchBird-only features

If a feature doesn't exist in the emulated database, it should not be available through that parser.

### Rule 3: All parsers output SBLR bytecode

No parser generates dialect-specific bytecode. All parsers compile to the same SBLR format.

### Rule 4: Result formatting belongs in the parser layer

The parser layer handles converting native results to dialect-specific format.

---

## Parser Architecture

### V2 Native Parser

**Location:** `src/parser/parser_v2.cpp`

The native ScratchBird parser - full feature set including all extensions.

**Pipeline:**
```
SQL Text
    │
    ▼
┌──────────────────┐
│  V2 Parser       │  src/parser/parser_v2.cpp
│  (lexer + parser)│
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  AST v2          │  include/scratchbird/parser/ast_v2.h
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Semantic        │  src/sblr/semantic_analyzer_v2.cpp
│  Analyzer        │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Bytecode        │  src/sblr/bytecode_generator_v2.cpp
│  Generator       │
└────────┬─────────┘
         │
         ▼
SBLR Bytecode
```

---

### PostgreSQL Parser

**Location:** `src/parser/postgresql/`

Handles PostgreSQL-specific syntax and pg_catalog functions.

**Key Files:**
- `pg_parser.cpp` - Main PostgreSQL parser
- `pg_parser_ddl.cpp` - DDL statements
- `pg_catalog.cpp` - pg_catalog.* functions

**Supported Features:**
- PostgreSQL-style casts (`::type`)
- Dollar-quoted strings (`$$string$$`)
- `pg_catalog.*` functions
- Information schema views
- PostgreSQL-style arrays

**Example Mapping:**
```sql
-- PostgreSQL syntax
SELECT pg_catalog.version();

-- Becomes SBLR
OP_CALL_FUNCTION "scratchbird_version"
```

---

### MySQL Parser

**Location:** `src/parser/mysql/`

Handles MySQL-specific syntax and SHOW commands.

**Key Files:**
- `mysql_parser.cpp` - Main MySQL parser
- `show_commands.cpp` - SHOW statement handling

**Supported Features:**
- Backtick identifiers (`` `table` ``)
- SHOW commands (SHOW TABLES, SHOW DATABASES, etc.)
- AUTO_INCREMENT syntax
- MySQL-style comments (`#`, `--`)
- LIMIT without OFFSET syntax

**Example Mapping:**
```sql
-- MySQL syntax
SHOW TABLES;

-- Becomes SBLR
OP_SELECT
  OP_COLUMN_REF "table_name"
OP_FROM
  OP_TABLE_REF "sb_catalog.tables"
```

---

### Firebird Parser

**Location:** `src/parser/firebird/`

Handles Firebird-specific syntax and RDB$ system tables.

**Key Files:**
- `firebird_parser.cpp` - Main Firebird parser

**Supported Features:**
- RDB$* system table queries
- Firebird-style generators (sequences)
- EXECUTE BLOCK syntax
- Firebird stored procedure syntax

**Example Mapping:**
```sql
-- Firebird syntax
SELECT RDB$RELATION_NAME FROM RDB$RELATIONS;

-- Becomes SBLR
OP_SELECT
  OP_COLUMN_REF "table_name"
OP_FROM
  OP_TABLE_REF "sb_catalog.tables"
```

---

## Result Formatting

The parser layer also handles converting native results to dialect-specific format.

### Request Path (Incoming)

```
SQL Text → Parse → SBLR → Engine
```

### Response Path (Outgoing)

```
Native Results → Format → Dialect-specific Results → Wire Protocol
```

### Example: Type OID Mapping

PostgreSQL clients expect results with type OIDs:

```cpp
// Native result from engine
{ column: "name", type: STRING }

// PostgreSQL-formatted result
{ column: "name", type_oid: 25 /* TEXT */ }
```

---

## Source Code Structure

```
src/parser/
├── parser_v2.cpp              # Native V2 parser
├── ast_v2.cpp                 # AST implementation
├── firebird/
│   └── firebird_parser.cpp    # Firebird parser
├── postgresql/
│   ├── pg_parser.cpp          # PostgreSQL parser main
│   └── pg_parser_ddl.cpp      # PostgreSQL DDL
└── mysql/
    └── mysql_parser.cpp       # MySQL parser
```

---

## Adding a New Dialect

To add support for a new SQL dialect:

1. **Create parser directory:** `src/parser/<dialect>/`

2. **Implement parser:**
   ```cpp
   class DialectParser {
       AST parse(const std::string& sql);
       SBLR generateSBLR(const AST& ast);
       Result formatResult(const NativeResult& result);
   };
   ```

3. **Map dialect-specific functions to SBLR:**
   - Identify dialect-specific functions
   - Map to existing SBLR opcodes or request new ones

4. **Implement result formatter:**
   - Convert native types to dialect types
   - Format column names as expected

5. **Add wire protocol handler** (separate from parser)

---

## Testing Parsers

### Unit Tests

```cpp
// Test that MySQL SHOW TABLES parses correctly
TEST(MySQLParser, ShowTables) {
    MySQLParser parser;
    auto ast = parser.parse("SHOW TABLES");
    EXPECT_EQ(ast.type, AST_SHOW_TABLES);
}
```

### Integration Tests

```cpp
// Test full round-trip through MySQL parser
TEST(MySQLIntegration, CreateTable) {
    auto conn = connect_mysql_dialect();
    conn.execute("CREATE TABLE test (id INT)");
    auto result = conn.execute("SHOW TABLES");
    EXPECT_CONTAINS(result, "test");
}
```

---

## Dialect Compatibility Matrix

| Feature | Native | PostgreSQL | MySQL | Firebird |
|---------|--------|------------|-------|----------|
| Basic DDL | Yes | Yes | Yes | Yes |
| Basic DML | Yes | Yes | Yes | Yes |
| Transactions | Yes | Yes | Yes | Yes |
| Stored procedures | Yes | Partial | Partial | Partial |
| System catalog | sb_catalog | pg_catalog | information_schema | RDB$ |
| Array types | Yes | Yes | No | No |
| JSON/JSONB | Yes | Yes | Yes | No |
| Spatial | Yes | PostGIS-style | No | No |

---

## Source Code Reference

| Component | Header | Implementation |
|-----------|--------|----------------|
| V2 Parser | `include/scratchbird/parser/parser_v2.h` | `src/parser/parser_v2.cpp` |
| AST | `include/scratchbird/parser/ast_v2.h` | `src/parser/ast_v2.cpp` |
| PostgreSQL Parser | `include/scratchbird/parser/postgresql/pg_parser.h` | `src/parser/postgresql/pg_parser.cpp` |
| Firebird Parser | | `src/parser/firebird/firebird_parser.cpp` |
| MySQL Parser | | `src/parser/mysql/mysql_parser.cpp` |

---

## Related Documents

- [Architecture](Architecture.md) - Where parsers fit in the system
- [SBLR](SBLR.md) - Bytecode that parsers generate
- [Network and Listeners](Network-Listeners.md) - Wire protocols that feed parsers
- [Language Guides](../language-guides/README.md) - Supported SQL syntax per dialect
