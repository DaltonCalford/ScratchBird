# ScratchBird Parser v2.0 Implementation Plan

**Version:** 2.0
**Status:** Planning
**Architecture:** Recursive Descent with State-Aware Dispatch
**Design Philosophy:** "Smart Parser, Dumb Lexer"
**Target:** < 50 Reserved Words (Gatekeepers)

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Architecture Overview](#2-architecture-overview)
3. [Lexer Specification](#3-lexer-specification)
4. [Gatekeeper Keywords](#4-gatekeeper-keywords)
5. [Parser State Machine](#5-parser-state-machine)
6. [Schema Path Syntax](#6-schema-path-syntax)
7. [Search Path Resolution](#7-search-path-resolution)
8. [UUID-Based Object Resolution](#8-uuid-based-object-resolution)
9. [AST Node Specifications](#9-ast-node-specifications)
10. [Statement Grammar Reference](#10-statement-grammar-reference)
11. [SBLR Integration](#11-sblr-integration)
12. [Implementation Phases](#12-implementation-phases)
13. [Testing Strategy](#13-testing-strategy)
14. [Migration Path](#14-migration-path)

---

## 1. Executive Summary

### 1.1 Goals

The ScratchBird Parser v2.0 is a complete rewrite of the SQL parser with the following objectives:

1. **Reduce reserved keywords from ~180+ to <50** - Enable common words like `name`, `type`, `value`, `data`, `user`, `path` as valid unquoted identifiers
2. **Context-sensitive parsing** - Keywords recognized only in appropriate syntactic positions
3. **Hierarchical schema navigation** - Native support for recursive schema paths
4. **UUID-based resolution** - Single name-to-UUID lookup, then UUID-only execution
5. **External parser architecture** - Parser exists outside the engine; engine accepts only SBLR bytecode or API calls
6. **Multi-dialect support** - Master parser plus separate emulation parsers (Firebird, PostgreSQL, etc.)

### 1.2 Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| "Dumb Lexer" | Lexer produces IDENTIFIER for most tokens; parser determines meaning from context |
| Gatekeeper model | Only ~35 words globally reserved (statement starters + logic operators) |
| State machine | Explicit ParserState class tracks context for keyword recognition |
| Path-based schemas | Familiar navigation: `.` (current), `..` (parent), absolute paths |
| UUID resolution | One-time name lookup at semantic analysis; execution uses UUIDs only |
| Quoted identifiers | Double quotes for spaces, case sensitivity, reserved word escape |

### 1.3 Non-Goals

- This plan does NOT cover emulation parsers (Firebird, PostgreSQL, etc.)
- This plan does NOT modify the SBLR bytecode format
- This plan does NOT change the storage engine internals

---

## 2. Architecture Overview

### 2.1 Compilation Pipeline

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   SQL Text  │────▶│    Lexer    │────▶│   Parser    │────▶│  Semantic   │────▶│    SBLR     │
│             │     │   (Dumb)    │     │  (Smart)    │     │  Analyzer   │     │  Generator  │
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
                           │                   │                   │                   │
                           ▼                   ▼                   ▼                   ▼
                       Tokens            Unresolved AST      Resolved AST         Bytecode
                    (mostly IDENT)       (StringIds)          (UUIDs)            (executable)
```

### 2.2 Component Responsibilities

| Component | Input | Output | Responsibility |
|-----------|-------|--------|----------------|
| Lexer | SQL text | Token stream | Tokenize literals, operators, identifiers; NO keyword semantics |
| Parser | Tokens | Unresolved AST | Build syntax tree using state-aware contextual matching |
| Semantic Analyzer | Unresolved AST | Resolved AST | Name resolution, type checking, UUID assignment |
| SBLR Generator | Resolved AST | Bytecode | Generate executable instructions |
| Engine | Bytecode/API | Results | Execute operations using UUIDs only |

### 2.3 External Parser Model

```
┌────────────────────────────────────────────────────────────┐
│                    External Parser Module                   │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │  Lexer   │  │  Parser  │  │ Semantic │  │   SBLR   │   │
│  │          │  │          │  │ Analyzer │  │Generator │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
└────────────────────────────────────────────────────────────┘
                              │
                              ▼ SBLR Bytecode / API Calls
┌────────────────────────────────────────────────────────────┐
│                     ScratchBird Engine                      │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │ Executor │  │ Catalog  │  │ Storage  │  │  Buffer  │   │
│  │          │  │ Manager  │  │  Engine  │  │   Pool   │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
└────────────────────────────────────────────────────────────┘
```

---

## 3. Lexer Specification

### 3.1 Design Principles

The lexer is deliberately "dumb" - it recognizes:
- Literals (strings, numbers, booleans)
- Operators and punctuation
- Quoted identifiers
- Gatekeeper keywords (only ~35)
- Everything else as IDENTIFIER

### 3.2 Token Types

```cpp
enum class TokenType {
    // === Literals ===
    INTEGER_LITERAL,        // 123, -456
    FLOAT_LITERAL,          // 3.14, -2.5e10
    STRING_LITERAL,         // 'hello', 'it''s'
    BLOB_LITERAL,           // X'DEADBEEF'

    // === Identifiers ===
    IDENTIFIER,             // unquoted: mytable, column1
    QUOTED_IDENTIFIER,      // double-quoted: "My Table", "SELECT"

    // === Operators ===
    PLUS,                   // +
    MINUS,                  // -
    STAR,                   // *
    SLASH,                  // /
    PERCENT,                // %
    EQUALS,                 // =
    NOT_EQUALS,             // <> or !=
    LESS_THAN,              // <
    GREATER_THAN,           // >
    LESS_EQUALS,            // <=
    GREATER_EQUALS,         // >=
    CONCAT,                 // ||

    // === Punctuation ===
    LPAREN,                 // (
    RPAREN,                 // )
    LBRACKET,               // [
    RBRACKET,               // ]
    COMMA,                  // ,
    SEMICOLON,              // ;
    DOT,                    // .
    DOUBLE_DOT,             // ..
    COLON,                  // :
    DOUBLE_COLON,           // ::

    // === Gatekeepers (see Section 4) ===
    KW_SELECT, KW_INSERT, KW_UPDATE, KW_DELETE, /* etc. */

    // === Special ===
    END_OF_FILE,
    ERROR
};
```

### 3.3 Lexer Rules

#### 3.3.1 Identifier Recognition

```
IDENTIFIER := [a-zA-Z_][a-zA-Z0-9_$]*
```

- Starts with letter or underscore
- Contains letters, digits, underscores, dollar signs
- Case-insensitive for matching (preserved for display)
- Maximum length: 128 characters

#### 3.3.2 Quoted Identifier Recognition

```
QUOTED_IDENTIFIER := " ( [^"] | "" )* "
```

- Enclosed in double quotes
- Embedded double quotes escaped as `""`
- Case-sensitive
- May contain any characters including spaces
- Maximum length: 128 characters

#### 3.3.3 String Literal Recognition

```
STRING_LITERAL := ' ( [^'] | '' )* '
```

- Enclosed in single quotes
- Embedded single quotes escaped as `''`

#### 3.3.4 Numeric Literal Recognition

```
INTEGER_LITERAL := [-+]? [0-9]+
FLOAT_LITERAL   := [-+]? [0-9]* . [0-9]+ ( [eE] [-+]? [0-9]+ )?
```

#### 3.3.5 Double-Dot Recognition

The lexer must recognize `..` as a single token (DOUBLE_DOT) for parent schema navigation:

```cpp
if (current == '.' && peek() == '.') {
    advance(); advance();
    return Token(DOUBLE_DOT, "..");
}
```

### 3.4 Gatekeeper Lookup

Only Gatekeeper keywords are recognized by the lexer:

```cpp
Token Lexer::scanIdentifierOrKeyword() {
    std::string text = scanIdentifierText();
    std::string upper = toUpper(text);

    // Check Gatekeeper table (hash map, ~35 entries)
    auto it = gatekeepers_.find(upper);
    if (it != gatekeepers_.end()) {
        return Token(it->second, text);  // Return keyword token
    }

    // Not a Gatekeeper - return as identifier
    return Token(IDENTIFIER, text);
}
```

---

## 4. Gatekeeper Keywords

### 4.1 Complete Gatekeeper List

These are the ONLY globally reserved keywords. They cannot be used as unquoted identifiers.

| Category | Keywords | Count |
|----------|----------|-------|
| **DDL Control** | `CREATE`, `ALTER`, `DROP`, `TRUNCATE`, `COMMENT` | 5 |
| **DML** | `SELECT`, `INSERT`, `UPDATE`, `DELETE`, `MERGE`, `WITH` | 6 |
| **Transaction** | `START`, `COMMIT`, `ROLLBACK`, `SAVEPOINT`, `RELEASE` | 5 |
| **Session** | `SET`, `RESET`, `SHOW`, `DESCRIBE` | 4 |
| **Security** | `GRANT`, `REVOKE` | 2 |
| **Logic** | `AND`, `OR`, `NOT`, `NULL`, `TRUE`, `FALSE` | 6 |
| **Utility** | `EXPLAIN`, `ANALYZE`, `REFRESH`, `SWEEP`, `ATTACH`, `DETACH`, `CALL` | 7 |
| **Total** | | **35** |

### 4.2 Demoted Keywords (Now Contextual)

These words are NO LONGER reserved. They are valid as unquoted identifiers:

#### 4.2.1 Object Types (contextual after CREATE/ALTER/DROP)
```
TABLE, INDEX, VIEW, PROCEDURE, FUNCTION, TRIGGER, SEQUENCE,
DOMAIN, TYPE, USER, ROLE, GROUP, POLICY, TABLESPACE, SCHEMA
```

#### 4.2.2 Clauses (contextual in specific statements)
```
FROM, WHERE, JOIN, ON, AS, INTO, VALUES, SET, ORDER, BY,
GROUP, HAVING, LIMIT, OFFSET, UNION, INTERSECT, EXCEPT
```

#### 4.2.3 Modifiers (contextual)
```
CASCADE, RESTRICT, ASYNC, SYNC, FORCE, IMMEDIATE, DEFERRED,
UNIQUE, PRIMARY, FOREIGN, KEY, REFERENCES, CHECK, DEFAULT,
CONSTRAINT, DEFERRABLE, INITIALLY
```

#### 4.2.4 Transaction (contextual after START/SET)
```
TRANSACTION, ISOLATION, LEVEL, SNAPSHOT, READ, WRITE,
COMMITTED, WAIT, RESERVING, PROTECTED, SHARED
```

#### 4.2.5 Common Names (always valid as identifiers)
```
name, type, value, data, date, time, user, role, path,
status, state, count, size, id, key, result, action
```

#### 4.2.6 PSQL Keywords (contextual in procedural blocks only)
```
BEGIN, END, IF, THEN, ELSE, ELSIF, WHILE, LOOP, FOR,
DECLARE, RETURN, RAISE, EXCEPTION, EXIT, CONTINUE,
WHEN, CURSOR, FETCH, OPEN, CLOSE, INTO
```

### 4.3 Special Case: CASE

`CASE` is special - it appears in expressions (DML context) but is NOT a statement starter:

```sql
SELECT CASE WHEN x > 0 THEN 'positive' ELSE 'non-positive' END FROM t;
```

`CASE` is recognized contextually when parsing expressions, not as a Gatekeeper.

### 4.4 Identifier Escape

To use a Gatekeeper as an identifier, quote it:

```sql
CREATE TABLE "SELECT" (
    "FROM" INT,
    "WHERE" VARCHAR(100)
);

INSERT INTO "SELECT" ("FROM", "WHERE") VALUES (1, 'test');
```

---

## 5. Parser State Machine

### 5.1 ParserState Class

```cpp
enum class ParseMode {
    STATEMENT,      // Top-level, expecting statement start
    DDL,            // After CREATE/ALTER/DROP
    DML_SELECT,     // Inside SELECT statement
    DML_INSERT,     // Inside INSERT statement
    DML_UPDATE,     // Inside UPDATE statement
    DML_DELETE,     // Inside DELETE statement
    DML_MERGE,      // Inside MERGE statement
    EXPRESSION,     // Parsing expression
    SESSION,        // After SET/RESET/SHOW
    TRANSACTION,    // After START TRANSACTION / SET TRANSACTION
    PSQL,           // Inside procedural block
    COLUMN_DEF,     // Parsing column definitions
    TABLE_REF,      // Parsing table reference (FROM, JOIN)
    SECURITY        // After GRANT/REVOKE
};

class ParserState {
public:
    void pushMode(ParseMode mode);
    void popMode();
    ParseMode currentMode() const;
    bool isInMode(ParseMode mode) const;

    // Contextual keyword matching
    bool matchContextual(const char* keyword);
    bool checkContextual(const char* keyword);
    void expectContextual(const char* keyword, const char* errorMsg);

private:
    std::stack<ParseMode> mode_stack_;
};
```

### 5.2 Contextual Matching Implementation

```cpp
bool ParserState::matchContextual(const char* keyword) {
    if (current().type != TokenType::IDENTIFIER) {
        return false;
    }

    std::string_view text = stringPool().get(current().value.string_id);
    if (caseInsensitiveEquals(text, keyword)) {
        advance();
        return true;
    }
    return false;
}

bool ParserState::checkContextual(const char* keyword) {
    if (current().type != TokenType::IDENTIFIER) {
        return false;
    }

    std::string_view text = stringPool().get(current().value.string_id);
    return caseInsensitiveEquals(text, keyword);
}

void ParserState::expectContextual(const char* keyword, const char* errorMsg) {
    if (!matchContextual(keyword)) {
        error(errorMsg);
    }
}
```

### 5.3 Statement Dispatch

```cpp
Statement* Parser::parseStatement() {
    state_.pushMode(ParseMode::STATEMENT);

    Statement* stmt = nullptr;

    // Gatekeepers dispatch to specific handlers
    if (match(KW_SELECT))    { stmt = parseSelect(); }
    else if (match(KW_INSERT))    { stmt = parseInsert(); }
    else if (match(KW_UPDATE))    { stmt = parseUpdate(); }
    else if (match(KW_DELETE))    { stmt = parseDelete(); }
    else if (match(KW_MERGE))     { stmt = parseMerge(); }
    else if (match(KW_WITH))      { stmt = parseWithStatement(); }
    else if (match(KW_CREATE))    { stmt = parseCreate(); }
    else if (match(KW_ALTER))     { stmt = parseAlter(); }
    else if (match(KW_DROP))      { stmt = parseDrop(); }
    else if (match(KW_TRUNCATE))  { stmt = parseTruncate(); }
    else if (match(KW_COMMENT))   { stmt = parseComment(); }
    else if (match(KW_SET))       { stmt = parseSet(); }
    else if (match(KW_RESET))     { stmt = parseReset(); }
    else if (match(KW_SHOW))      { stmt = parseShow(); }
    else if (match(KW_DESCRIBE))  { stmt = parseDescribe(); }
    else if (match(KW_GRANT))     { stmt = parseGrant(); }
    else if (match(KW_REVOKE))    { stmt = parseRevoke(); }
    else if (match(KW_START))     { stmt = parseStartTransaction(); }
    else if (match(KW_COMMIT))    { stmt = parseCommit(); }
    else if (match(KW_ROLLBACK))  { stmt = parseRollback(); }
    else if (match(KW_SAVEPOINT)) { stmt = parseSavepoint(); }
    else if (match(KW_RELEASE))   { stmt = parseReleaseSavepoint(); }
    else if (match(KW_EXPLAIN))   { stmt = parseExplain(); }
    else if (match(KW_ANALYZE))   { stmt = parseAnalyze(); }
    else if (match(KW_REFRESH))   { stmt = parseRefresh(); }
    else if (match(KW_SWEEP))     { stmt = parseSweep(); }
    else if (match(KW_ATTACH))    { stmt = parseAttach(); }
    else if (match(KW_DETACH))    { stmt = parseDetach(); }
    else if (match(KW_CALL))      { stmt = parseCall(); }
    else {
        // Not a Gatekeeper - in PSQL context, could be assignment
        if (state_.isInMode(ParseMode::PSQL)) {
            stmt = parseAssignmentOrExpression();
        } else {
            error("Expected SQL statement");
        }
    }

    state_.popMode();
    return stmt;
}
```

### 5.4 DDL Dispatch Example

```cpp
Statement* Parser::parseCreate() {
    state_.pushMode(ParseMode::DDL);

    // Check for OR REPLACE
    bool orReplace = false;
    if (checkContextual("OR")) {
        matchContextual("OR");
        expectContextual("REPLACE", "Expected REPLACE after OR");
        orReplace = true;
    }

    Statement* stmt = nullptr;

    // Contextual object type matching
    if (matchContextual("TABLE"))      { stmt = parseCreateTable(); }
    else if (matchContextual("INDEX")) { stmt = parseCreateIndex(); }
    else if (matchContextual("VIEW"))  { stmt = parseCreateView(orReplace); }
    else if (matchContextual("SEQUENCE"))   { stmt = parseCreateSequence(); }
    else if (matchContextual("PROCEDURE"))  { stmt = parseCreateProcedure(orReplace); }
    else if (matchContextual("FUNCTION"))   { stmt = parseCreateFunction(orReplace); }
    else if (matchContextual("TRIGGER"))    { stmt = parseCreateTrigger(); }
    else if (matchContextual("USER"))       { stmt = parseCreateUser(); }
    else if (matchContextual("ROLE"))       { stmt = parseCreateRole(); }
    else if (matchContextual("GROUP"))      { stmt = parseCreateGroup(); }
    else if (matchContextual("POLICY"))     { stmt = parseCreatePolicy(); }
    else if (matchContextual("TYPE"))       { stmt = parseCreateType(); }
    else if (matchContextual("DOMAIN"))     { stmt = parseCreateDomain(); }
    else if (matchContextual("TABLESPACE")) { stmt = parseCreateTablespace(); }
    else if (matchContextual("SCHEMA"))     { stmt = parseCreateSchema(); }
    else {
        error("Expected object type after CREATE");
    }

    state_.popMode();
    return stmt;
}
```

---

## 6. Schema Path Syntax

### 6.1 Path Types

| Syntax | Type | Meaning | Example |
|--------|------|---------|---------|
| `name` | UNQUALIFIED | Use search path | `orders` |
| `.name` | CURRENT | Current schema explicit | `.orders` |
| `..name` | PARENT | Parent schema | `..orders` |
| `.path.name` | RELATIVE | Relative from current | `.sub.orders` |
| `path.name` | ABSOLUTE | Absolute path | `sys.catalog.tables` |

### 6.2 Grammar (BNF)

```bnf
<table_reference> ::=
    <path_identifier> [ [ AS ] <alias> ]

<path_identifier> ::=
    <unqualified_name>
  | <current_path>
  | <parent_path>
  | <qualified_path>

<unqualified_name> ::=
    <identifier>

<current_path> ::=
    DOT <identifier>
  | DOT <identifier> ( DOT <identifier> )*

<parent_path> ::=
    DOUBLE_DOT <identifier>
  | DOUBLE_DOT <identifier> ( DOT <identifier> )*

<qualified_path> ::=
    <identifier> ( DOT <identifier> )+

<identifier> ::=
    IDENTIFIER
  | QUOTED_IDENTIFIER
```

### 6.3 Path Parsing Implementation

```cpp
struct SchemaPath {
    enum class Type {
        UNQUALIFIED,    // name (uses search path)
        CURRENT,        // .name or .path.name
        PARENT,         // ..name or ..path.name
        ABSOLUTE        // schema.name or schema.path.name
    };

    Type type;
    std::vector<StringPool::StringId> components;

    StringPool::StringId objectName() const {
        return components.back();
    }

    std::vector<StringPool::StringId> schemaPath() const {
        return std::vector<StringPool::StringId>(
            components.begin(),
            components.end() - 1
        );
    }
};

SchemaPath Parser::parseSchemaPath() {
    SchemaPath path;

    if (match(TokenType::DOUBLE_DOT)) {
        // Parent path: ..name or ..path.name
        path.type = SchemaPath::Type::PARENT;
        path.components.push_back(expectIdentifier("Expected identifier after .."));

        while (match(TokenType::DOT)) {
            path.components.push_back(expectIdentifier("Expected identifier after ."));
        }
    }
    else if (match(TokenType::DOT)) {
        // Current path: .name or .path.name
        path.type = SchemaPath::Type::CURRENT;
        path.components.push_back(expectIdentifier("Expected identifier after ."));

        while (match(TokenType::DOT)) {
            path.components.push_back(expectIdentifier("Expected identifier after ."));
        }
    }
    else {
        // Unqualified or absolute: name or schema.path.name
        path.components.push_back(expectIdentifier("Expected identifier"));

        if (check(TokenType::DOT) && !check(TokenType::DOUBLE_DOT)) {
            // Has more components - absolute path
            path.type = SchemaPath::Type::ABSOLUTE;

            while (match(TokenType::DOT)) {
                path.components.push_back(expectIdentifier("Expected identifier after ."));
            }
        } else {
            // Single name - unqualified
            path.type = SchemaPath::Type::UNQUALIFIED;
        }
    }

    return path;
}
```

### 6.4 Examples

```sql
-- Unqualified (uses search path)
SELECT * FROM orders;

-- Current schema explicit
SELECT * FROM .orders;

-- Parent schema
SELECT * FROM ..orders;

-- Relative path (current schema -> subschema -> table)
SELECT * FROM .reports.quarterly;

-- Absolute path
SELECT * FROM production.sales.orders;

-- Mixed in JOIN
SELECT o.id, c.name
FROM .orders o
JOIN ..customers c ON o.customer_id = c.id
JOIN system.catalog.tables t ON t.name = 'orders';
```

---

## 7. Search Path Resolution

### 7.1 Search Path Variable

```sql
-- View current search path
SHOW SEARCH PATH;

-- Set search path
SET SEARCH PATH ., .shared, system.public;

-- Clear search path (require explicit paths)
SET SEARCH PATH;
```

### 7.2 Resolution Algorithm

```cpp
UUID resolveUnqualifiedName(
    const StringPool::StringId& name,
    const std::vector<SchemaPath>& searchPath,
    const UUID& currentSchemaUUID
) {
    for (const auto& pathEntry : searchPath) {
        UUID schemaUUID = resolveSchemaPath(pathEntry, currentSchemaUUID);

        auto result = catalog.findObject(schemaUUID, name);
        if (result.found) {
            return result.uuid;
        }
    }

    throw SemanticError("Object not found: " + name);
}

UUID resolveSchemaPath(
    const SchemaPath& path,
    const UUID& currentSchemaUUID
) {
    switch (path.type) {
        case SchemaPath::Type::CURRENT:
            return resolveRelativePath(path.components, currentSchemaUUID);

        case SchemaPath::Type::PARENT:
            UUID parentUUID = catalog.getParentSchema(currentSchemaUUID);
            return resolveRelativePath(path.components, parentUUID);

        case SchemaPath::Type::ABSOLUTE:
            return resolveAbsolutePath(path.components);

        case SchemaPath::Type::UNQUALIFIED:
            // Single component treated as schema name from root
            return catalog.findSchema(path.components[0]);
    }
}
```

### 7.3 Resolution Order

Search path is processed left-to-right. First match wins.

```sql
SET SEARCH PATH ., .shared, system.public;

-- Resolution order for: SELECT * FROM orders
-- 1. Check {current_schema}/orders
-- 2. Check {current_schema}/shared/orders
-- 3. Check system/public/orders
-- 4. Error if not found in any
```

### 7.4 Shadowing

Objects in earlier path entries shadow those in later entries:

```sql
SET CURRENT SCHEMA user.dalton;
SET SEARCH PATH ., system.public;

-- If both user.dalton.orders and system.public.orders exist:
SELECT * FROM orders;  -- Uses user.dalton.orders (first in path)
```

### 7.5 Explicit Paths Bypass Search

Qualified paths do not use the search path:

```sql
SELECT * FROM .orders;           -- Always current schema
SELECT * FROM ..orders;          -- Always parent schema
SELECT * FROM system.public.orders;  -- Always absolute
```

---

## 8. UUID-Based Object Resolution

### 8.1 Resolution Flow

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  SQL Names  │────▶│  Semantic   │────▶│   Catalog   │────▶│    UUIDs    │
│             │     │  Analyzer   │     │   Lookup    │     │             │
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
     orders              │                    │              550e8400-...
                         │                    │
                         ▼                    ▼
                   Apply Search         Name → UUID
                      Path              (one lookup)
```

### 8.2 UUID Structure

```cpp
struct UUID {
    uint64_t high;  // Timestamp + version
    uint64_t low;   // Random + variant

    static UUID generate();
    static UUID fromString(const char* str);
    std::string toString() const;

    bool operator==(const UUID& other) const;
    size_t hash() const;
};
```

### 8.3 Catalog Entry

```cpp
struct CatalogEntry {
    UUID object_uuid;               // Primary identifier (immutable)
    StringPool::StringId name;      // Display name (can be renamed)
    UUID parent_schema_uuid;        // Parent in hierarchy
    ObjectType type;                // TABLE, VIEW, INDEX, etc.
    uint64_t created_timestamp;
    uint64_t modified_timestamp;
    // ... additional metadata
};
```

### 8.4 Catalog Indexes

```cpp
class Catalog {
    // Primary index: UUID → Entry
    std::unordered_map<UUID, CatalogEntry> entries_;

    // Name lookup index: (parent_uuid, name, type) → UUID
    std::map<std::tuple<UUID, StringId, ObjectType>, UUID> name_index_;

    // Schema children index: parent_uuid → set<UUID>
    std::unordered_map<UUID, std::set<UUID>> children_index_;
};
```

### 8.5 Unresolved vs Resolved AST

#### Unresolved (Parser Output)

```cpp
struct UnresolvedTableRef {
    SchemaPath path;            // As parsed
    StringPool::StringId alias; // Optional alias
    SourceSpan location;        // For error reporting
};

struct UnresolvedSelectStmt {
    std::vector<UnresolvedSelectItem> items;
    std::vector<UnresolvedTableRef> from_tables;
    UnresolvedExpr* where_clause;
    // ...
};
```

#### Resolved (Semantic Analyzer Output)

```cpp
struct ResolvedTableRef {
    UUID object_uuid;           // Resolved table/view UUID
    UUID schema_uuid;           // Schema containing the object
    StringPool::StringId alias; // Runtime alias
    ObjectType type;            // TABLE or VIEW
};

struct ResolvedSelectStmt {
    std::vector<ResolvedSelectItem> items;
    std::vector<ResolvedTableRef> from_tables;
    ResolvedExpr* where_clause;
    // ...
};
```

### 8.6 Benefits of UUID Resolution

1. **Single lookup** - Name resolved once at compile time
2. **Rename-safe** - Renaming object doesn't invalidate compiled queries
3. **No ambiguity** - UUID is globally unique
4. **Efficient execution** - Hash lookup by UUID, no string comparison
5. **Cache-friendly** - Fixed 16-byte key

---

## 9. AST Node Specifications

### 9.1 Base Classes

```cpp
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual ASTKind kind() const = 0;

    SourceSpan span;  // Source location for errors
};

class Statement : public ASTNode {
public:
    virtual void accept(StatementVisitor& visitor) = 0;
};

class Expression : public ASTNode {
public:
    virtual void accept(ExpressionVisitor& visitor) = 0;
};
```

### 9.2 Path Types

```cpp
enum class PathType {
    UNQUALIFIED,    // name
    CURRENT,        // .name or .path.name
    PARENT,         // ..name or ..path.name
    ABSOLUTE        // schema.name or schema.path.name
};

struct ObjectPath {
    PathType type;
    std::vector<StringPool::StringId> components;

    bool isQualified() const {
        return type != PathType::UNQUALIFIED;
    }
};
```

### 9.3 Table Reference

```cpp
struct TableRef : public ASTNode {
    ASTKind kind() const override { return ASTKind::TableRef; }

    ObjectPath path;
    StringPool::StringId alias;  // Optional
    bool has_alias;
};

struct ResolvedTableRef {
    UUID object_uuid;
    UUID schema_uuid;
    StringPool::StringId alias;
    ObjectType resolved_type;    // TABLE, VIEW, etc.
};
```

### 9.4 Column Reference

```cpp
struct ColumnRef : public Expression {
    ASTKind kind() const override { return ASTKind::ColumnRef; }

    StringPool::StringId table_qualifier;  // Optional: table.column
    StringPool::StringId column_name;
    bool has_qualifier;
};

struct ResolvedColumnRef {
    UUID table_uuid;
    uint32_t column_index;
    DataType data_type;
};
```

### 9.5 DDL Statements

```cpp
struct CreateTableStmt : public Statement {
    ASTKind kind() const override { return ASTKind::CreateTable; }

    ObjectPath table_path;
    std::vector<ColumnDef*> columns;
    std::vector<TableConstraint*> constraints;
    ObjectPath tablespace;       // Optional
    bool has_tablespace;
};

struct CreateViewStmt : public Statement {
    ASTKind kind() const override { return ASTKind::CreateView; }

    bool or_replace;
    bool materialized;
    ObjectPath view_path;
    std::vector<StringPool::StringId> column_names;  // Optional
    SelectStmt* query;
    bool with_check_option;
};
```

### 9.6 DML Statements

```cpp
struct SelectStmt : public Statement {
    ASTKind kind() const override { return ASTKind::Select; }

    bool distinct;
    std::vector<SelectItem*> items;
    FromClause* from;
    Expression* where;
    GroupByClause* group_by;
    Expression* having;
    std::vector<WindowDef*> windows;
    OrderByClause* order_by;
    LimitClause* limit;

    // For set operations
    SetOperationType set_op;     // UNION, INTERSECT, EXCEPT
    bool set_op_all;
    SelectStmt* set_op_right;
};

struct InsertStmt : public Statement {
    ASTKind kind() const override { return ASTKind::Insert; }

    ObjectPath table_path;
    std::vector<StringPool::StringId> columns;  // Optional

    enum class Source { VALUES, SELECT, DEFAULT };
    Source source;

    std::vector<std::vector<Expression*>> values_rows;
    SelectStmt* select_source;

    OnConflictClause* on_conflict;  // UPSERT
    ReturningClause* returning;
};
```

### 9.7 Session Statements

```cpp
struct SetCurrentSchemaStmt : public Statement {
    ASTKind kind() const override { return ASTKind::SetCurrentSchema; }

    enum class Navigation { UP, DEFAULT, PATH };
    Navigation nav_type;
    ObjectPath schema_path;  // For PATH type
};

struct SetSearchPathStmt : public Statement {
    ASTKind kind() const override { return ASTKind::SetSearchPath; }

    std::vector<ObjectPath> path_list;
};

struct ShowStmt : public Statement {
    ASTKind kind() const override { return ASTKind::Show; }

    enum class Target {
        CURRENT_SCHEMA,
        SEARCH_PATH,
        TABLES,
        VIEWS,
        INDEXES,
        SCHEMAS,
        // ...
    };
    Target target;
    ObjectPath scope;  // Optional: SHOW TABLES IN schema
};
```

---

## 10. Statement Grammar Reference

### 10.1 DDL Statements

#### CREATE TABLE

```bnf
<create_table> ::=
    CREATE TABLE <object_path>
    LPAREN <column_def_list> [ COMMA <table_constraint_list> ] RPAREN
    [ TABLESPACE <object_path> ]

<column_def_list> ::=
    <column_def> [ COMMA <column_def> ]*

<column_def> ::=
    <identifier> <data_type> [ <column_constraint> ]*

<column_constraint> ::=
    NOT NULL
  | NULL
  | DEFAULT <expression>
  | PRIMARY KEY
  | UNIQUE
  | CHECK LPAREN <expression> RPAREN
  | REFERENCES <object_path> [ LPAREN <identifier> RPAREN ]
      [ ON DELETE <ref_action> ] [ ON UPDATE <ref_action> ]
  | GENERATED { ALWAYS | BY DEFAULT } AS IDENTITY
  | GENERATED ALWAYS AS LPAREN <expression> RPAREN [ STORED | VIRTUAL ]
```

#### CREATE INDEX

```bnf
<create_index> ::=
    CREATE [ UNIQUE ] INDEX <identifier>
    ON <object_path>
    [ USING <identifier> ]
    LPAREN <index_column_list> RPAREN
    [ WHERE <expression> ]
    [ TABLESPACE <object_path> ]

<index_column_list> ::=
    <index_column> [ COMMA <index_column> ]*

<index_column> ::=
    <identifier>
  | LPAREN <expression> RPAREN
```

#### CREATE VIEW

```bnf
<create_view> ::=
    CREATE [ OR REPLACE ] [ MATERIALIZED ] VIEW <object_path>
    [ LPAREN <identifier_list> RPAREN ]
    AS <select_stmt>
    [ WITH CHECK OPTION ]
```

### 10.2 DML Statements

#### SELECT

```bnf
<select_stmt> ::=
    [ <with_clause> ]
    SELECT [ DISTINCT | ALL ] <select_list>
    [ FROM <from_clause> ]
    [ WHERE <expression> ]
    [ GROUP BY <group_by_clause> ]
    [ HAVING <expression> ]
    [ WINDOW <window_def_list> ]
    [ <set_operation> <select_stmt> ]
    [ ORDER BY <order_by_clause> ]
    [ LIMIT <expression> [ OFFSET <expression> ] ]

<from_clause> ::=
    <table_ref> [ COMMA <table_ref> ]*
  | <table_ref> [ <join_clause> ]*

<table_ref> ::=
    <object_path> [ [ AS ] <identifier> ]
  | LPAREN <select_stmt> RPAREN [ AS ] <identifier>
  | <identifier> LPAREN <expression_list> RPAREN [ AS ] <identifier>

<join_clause> ::=
    [ INNER | LEFT [ OUTER ] | RIGHT [ OUTER ] | FULL [ OUTER ] | CROSS ]
    JOIN <table_ref>
    [ ON <expression> | USING LPAREN <identifier_list> RPAREN ]
```

#### INSERT

```bnf
<insert_stmt> ::=
    INSERT INTO <object_path>
    [ LPAREN <identifier_list> RPAREN ]
    <insert_source>
    [ ON CONFLICT <conflict_target> <conflict_action> ]
    [ RETURNING <select_list> ]

<insert_source> ::=
    VALUES <values_list>
  | <select_stmt>
  | DEFAULT VALUES
```

### 10.3 Session Statements

#### SET

```bnf
<set_stmt> ::=
    SET <set_target>

<set_target> ::=
    CURRENT SCHEMA <schema_navigation>
  | SEARCH PATH [ <path_list> ]
  | TRANSACTION <transaction_options>
  | ROLE <identifier>
  | NAMES <identifier>
  | <identifier> { TO | EQUALS } <expression>

<schema_navigation> ::=
    UP
  | DEFAULT
  | <object_path>

<path_list> ::=
    <object_path> [ COMMA <object_path> ]*
```

#### SHOW

```bnf
<show_stmt> ::=
    SHOW <show_target> [ <show_options> ]

<show_target> ::=
    CURRENT SCHEMA
  | SEARCH PATH
  | TABLES [ IN <object_path> ]
  | VIEWS [ IN <object_path> ]
  | INDEXES [ ON <object_path> ]
  | SCHEMAS [ IN <object_path> ]
  | COLUMNS IN <object_path>
```

### 10.4 Transaction Statements

```bnf
<start_transaction> ::=
    START TRANSACTION [ <tx_options> ]

<tx_options> ::=
    [ <access_mode> ]
    [ <wait_mode> ]
    [ <isolation_level> ]
    [ <reservation> ]

<access_mode> ::=
    READ WRITE
  | READ ONLY

<wait_mode> ::=
    WAIT
  | NO WAIT

<isolation_level> ::=
    ISOLATION LEVEL <iso_type>

<iso_type> ::=
    READ COMMITTED
  | SNAPSHOT [ TABLE STABILITY ]

<reservation> ::=
    RESERVING <table_list> FOR <lock_mode>
```

### 10.5 PSQL Blocks

```bnf
<psql_block> ::=
    [ DECLARE <declaration_list> ]
    BEGIN
        <statement_list>
    [ EXCEPTION <exception_handlers> ]
    END

<declaration_list> ::=
    <declaration> SEMICOLON [ <declaration> SEMICOLON ]*

<declaration> ::=
    <identifier> <data_type> [ DEFAULT <expression> ]

<statement_list> ::=
    <psql_statement> SEMICOLON [ <psql_statement> SEMICOLON ]*

<psql_statement> ::=
    <assignment>
  | <if_stmt>
  | <while_stmt>
  | <loop_stmt>
  | <for_stmt>
  | <return_stmt>
  | <raise_stmt>
  | <exit_stmt>
  | <dml_stmt>
```

---

## 11. SBLR Integration

### 11.1 Bytecode Generation

The SBLR Generator takes a Resolved AST and produces executable bytecode.

```cpp
class SBLRGenerator : public ResolvedASTVisitor {
public:
    BytecodeModule generate(const ResolvedStatement* stmt);

private:
    void visit(const ResolvedSelectStmt* stmt) override;
    void visit(const ResolvedInsertStmt* stmt) override;
    // ...

    void emitOpcode(Opcode op);
    void emitUUID(const UUID& uuid);
    void emitRegister(uint8_t reg);
    void emitImmediate(int64_t value);

    BytecodeBuilder builder_;
};
```

### 11.2 UUID in Bytecode

All object references in bytecode use UUIDs:

```cpp
// Example: SELECT * FROM orders WHERE id = 1
//
// LOAD_TABLE   R0, <orders_uuid>      ; Load table by UUID
// SCAN_START  R0                      ; Begin scan
// LOAD_CONST  R1, 1                   ; Load constant 1
// FILTER      R0, COL[0] == R1        ; Filter where col[0] = 1
// PROJECT     R0, *                   ; Project all columns
// RETURN      R0                      ; Return result
```

### 11.3 Engine API

The engine accepts bytecode or direct API calls:

```cpp
class Engine {
public:
    // Execute compiled bytecode
    Result execute(const BytecodeModule& module, ExecutionContext& ctx);

    // Direct API for simple operations
    Result insertRow(const UUID& table_uuid, const Row& row);
    Result deleteRow(const UUID& table_uuid, const RowId& row_id);
    Result updateRow(const UUID& table_uuid, const RowId& row_id, const Row& new_row);

    // Catalog operations
    UUID createTable(const TableDefinition& def);
    void dropTable(const UUID& table_uuid);
    // ...
};
```

### 11.4 Query Compilation Cache

Compiled queries are cached by SQL text hash:

```cpp
class QueryCache {
public:
    BytecodeModule* lookup(const std::string& sql);
    void store(const std::string& sql, BytecodeModule module);
    void invalidate(const UUID& object_uuid);  // When object changes

private:
    std::unordered_map<size_t, BytecodeModule> cache_;  // hash → bytecode
    std::multimap<UUID, size_t> object_deps_;           // uuid → hash (for invalidation)
};
```

---

## 12. Implementation Phases

### Phase 1: Foundation (Weeks 1-2)

**Deliverables:**
- [ ] New lexer with Gatekeeper-only keyword recognition
- [ ] Token types for DOT, DOUBLE_DOT
- [ ] StringPool integration
- [ ] Basic tokenization tests

**Files:**
- `src/parser_v2/lexer.h`
- `src/parser_v2/lexer.cpp`
- `src/parser_v2/token.h`
- `tests/parser_v2/test_lexer.cpp`

### Phase 2: Parser State Machine (Weeks 3-4)

**Deliverables:**
- [ ] ParserState class with mode stack
- [ ] Contextual matching helpers
- [ ] Statement dispatch skeleton
- [ ] Basic statement parsing tests

**Files:**
- `src/parser_v2/parser_state.h`
- `src/parser_v2/parser_state.cpp`
- `src/parser_v2/parser.h`
- `src/parser_v2/parser.cpp`

### Phase 3: Schema Path Parsing (Week 5)

**Deliverables:**
- [ ] SchemaPath struct
- [ ] parseSchemaPath() implementation
- [ ] Path syntax tests
- [ ] Integration with table references

**Files:**
- `src/parser_v2/schema_path.h`
- `src/parser_v2/schema_path.cpp`
- `tests/parser_v2/test_schema_path.cpp`

### Phase 4: DDL Statements (Weeks 6-7)

**Deliverables:**
- [ ] CREATE TABLE, INDEX, VIEW, SEQUENCE
- [ ] CREATE PROCEDURE, FUNCTION, TRIGGER
- [ ] CREATE USER, ROLE, GROUP, POLICY
- [ ] ALTER statements
- [ ] DROP statements
- [ ] Comprehensive DDL tests

### Phase 5: DML Statements (Weeks 8-10)

**Deliverables:**
- [ ] SELECT with all clauses
- [ ] INSERT with UPSERT
- [ ] UPDATE, DELETE
- [ ] MERGE
- [ ] Expression parsing
- [ ] Comprehensive DML tests

### Phase 6: Session & Transaction (Week 11)

**Deliverables:**
- [ ] SET, RESET, SHOW statements
- [ ] START TRANSACTION
- [ ] COMMIT, ROLLBACK, SAVEPOINT
- [ ] Session statement tests

### Phase 7: Semantic Analyzer (Weeks 12-14)

**Deliverables:**
- [ ] Name resolution with search path
- [ ] UUID assignment
- [ ] Type checking
- [ ] Resolved AST generation
- [ ] Semantic analysis tests

### Phase 8: SBLR Generator (Weeks 15-17)

**Deliverables:**
- [ ] Bytecode generation from resolved AST
- [ ] All statement types
- [ ] Optimization passes
- [ ] Generator tests

### Phase 9: Integration (Weeks 18-20)

**Deliverables:**
- [ ] Query cache implementation
- [ ] Engine API integration
- [ ] End-to-end tests
- [ ] Performance benchmarks

### Phase 10: Migration (Weeks 21-24)

**Deliverables:**
- [ ] Parallel operation mode (both parsers)
- [ ] Feature parity verification
- [ ] Old parser removal
- [ ] Documentation updates

---

## 13. Testing Strategy

### 13.1 Unit Tests

Each component has dedicated unit tests:

```cpp
// Lexer tests
TEST(LexerV2, TokenizesGatekeepers) {
    Lexer lexer("SELECT INSERT UPDATE DELETE");
    EXPECT_EQ(lexer.nextToken().type, TokenType::KW_SELECT);
    EXPECT_EQ(lexer.nextToken().type, TokenType::KW_INSERT);
    // ...
}

TEST(LexerV2, DemotedKeywordsAreIdentifiers) {
    Lexer lexer("TABLE INDEX VIEW PROCEDURE");
    EXPECT_EQ(lexer.nextToken().type, TokenType::IDENTIFIER);
    EXPECT_EQ(lexer.nextToken().type, TokenType::IDENTIFIER);
    // ...
}

TEST(LexerV2, RecognizesDoubleDot) {
    Lexer lexer("..parent");
    EXPECT_EQ(lexer.nextToken().type, TokenType::DOUBLE_DOT);
    EXPECT_EQ(lexer.nextToken().type, TokenType::IDENTIFIER);
}
```

### 13.2 Parser Tests

```cpp
TEST(ParserV2, CreateTableWithDemotedKeywords) {
    auto stmt = parse("CREATE TABLE procedure (value INT, type VARCHAR(50))");
    auto* create = dynamic_cast<CreateTableStmt*>(stmt);

    EXPECT_EQ(create->table_path.components[0], "procedure");
    EXPECT_EQ(create->columns[0]->name, "value");
    EXPECT_EQ(create->columns[1]->name, "type");
}

TEST(ParserV2, SchemaPathCurrent) {
    auto stmt = parse("SELECT * FROM .orders");
    auto* select = dynamic_cast<SelectStmt*>(stmt);
    auto& ref = select->from->tables[0];

    EXPECT_EQ(ref.path.type, PathType::CURRENT);
    EXPECT_EQ(ref.path.components[0], "orders");
}

TEST(ParserV2, SchemaPathParent) {
    auto stmt = parse("SELECT * FROM ..customers");
    // ...
}

TEST(ParserV2, SchemaPathAbsolute) {
    auto stmt = parse("SELECT * FROM system.catalog.tables");
    // ...
}
```

### 13.3 Semantic Analysis Tests

```cpp
TEST(SemanticV2, ResolvesUnqualifiedWithSearchPath) {
    Catalog catalog;
    catalog.createSchema("public");
    UUID ordersUUID = catalog.createTable("public", "orders", /* def */);

    Session session;
    session.setSearchPath({"."}); // current schema
    session.setCurrentSchema("public");

    auto resolved = analyze("SELECT * FROM orders", catalog, session);

    EXPECT_EQ(resolved->from->tables[0].object_uuid, ordersUUID);
}

TEST(SemanticV2, FirstMatchInSearchPathWins) {
    // Create same table name in two schemas
    UUID uuid1 = catalog.createTable("user.dalton", "orders", /* def */);
    UUID uuid2 = catalog.createTable("system.public", "orders", /* def */);

    session.setSearchPath({".", "system.public"});
    session.setCurrentSchema("user.dalton");

    auto resolved = analyze("SELECT * FROM orders", catalog, session);

    // Should resolve to first match (user.dalton.orders)
    EXPECT_EQ(resolved->from->tables[0].object_uuid, uuid1);
}
```

### 13.4 End-to-End Tests

```cpp
TEST(E2EV2, FullQueryExecution) {
    Engine engine;
    Parser parser;
    SemanticAnalyzer analyzer(engine.catalog());
    SBLRGenerator generator;

    // Setup
    engine.execute("CREATE TABLE orders (id INT, total DECIMAL(10,2))");
    engine.execute("INSERT INTO orders VALUES (1, 100.00), (2, 200.00)");

    // Parse, analyze, generate, execute
    auto ast = parser.parse("SELECT * FROM orders WHERE id = 1");
    auto resolved = analyzer.analyze(ast);
    auto bytecode = generator.generate(resolved);
    auto result = engine.execute(bytecode);

    EXPECT_EQ(result.rowCount(), 1);
    EXPECT_EQ(result.getValue(0, 0), 1);
    EXPECT_EQ(result.getValue(0, 1), 100.00);
}
```

### 13.5 Regression Tests

All current parser tests (1123 tests) must pass after migration:

```cpp
// Run all existing tests against new parser
class ParserV2RegressionTest : public ExistingParserTest {
protected:
    Parser& getParser() override {
        return parser_v2_;  // Use new parser
    }

    ParserV2 parser_v2_;
};
```

---

## 14. Migration Path

### 14.1 Parallel Operation

During migration, both parsers operate in parallel:

```cpp
class ParserFacade {
public:
    ParseResult parse(const std::string& sql) {
        if (use_v2_) {
            return parser_v2_.parse(sql);
        } else {
            return parser_v1_.parse(sql);
        }
    }

    void setUseV2(bool use_v2) { use_v2_ = use_v2; }

private:
    ParserV1 parser_v1_;
    ParserV2 parser_v2_;
    bool use_v2_ = false;
};
```

### 14.2 Feature Flags

```sql
-- Enable V2 parser for session
SET PARSER_VERSION TO 2;

-- Check current parser version
SHOW PARSER_VERSION;
```

### 14.3 Compatibility Verification

Before removing V1:
1. All 1123 existing tests pass on V2
2. All new V2-specific tests pass
3. Performance benchmarks meet or exceed V1
4. No regressions in error messages

### 14.4 Removal Steps

1. Set V2 as default
2. Deprecation warnings for V1-specific syntax
3. Monitor production usage
4. Remove V1 parser code
5. Update documentation

---

## Appendix A: Gatekeeper Quick Reference

```
DDL:         CREATE  ALTER  DROP  TRUNCATE  COMMENT
DML:         SELECT  INSERT  UPDATE  DELETE  MERGE  WITH
Transaction: START  COMMIT  ROLLBACK  SAVEPOINT  RELEASE
Session:     SET  RESET  SHOW  DESCRIBE
Security:    GRANT  REVOKE
Logic:       AND  OR  NOT  NULL  TRUE  FALSE
Utility:     EXPLAIN  ANALYZE  REFRESH  SWEEP  ATTACH  DETACH  CALL

Total: 35 keywords
```

## Appendix B: Path Syntax Quick Reference

```
orders              Unqualified (uses search path)
.orders             Current schema
..orders            Parent schema
.sub.orders         Relative: current/sub/orders
schema.orders       Absolute: schema/orders
a.b.c.orders        Absolute: a/b/c/orders
```

## Appendix C: Common Patterns

```sql
-- Create table with demoted keywords as names
CREATE TABLE type (
    name VARCHAR(100),
    value INT,
    data BLOB
);

-- Navigate schemas
SET CURRENT SCHEMA user.dalton.projects;
SET CURRENT SCHEMA UP;           -- Go to parent
SET CURRENT SCHEMA DEFAULT;      -- Go to home

-- Search path with current schema first
SET SEARCH PATH ., .shared, system.public;

-- Explicit path bypasses search
SELECT * FROM system.catalog.tables;

-- Parent schema reference
SELECT * FROM ..common.lookup_table;
```

---

**End of Implementation Plan**
