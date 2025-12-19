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

## Appendix D: Parser-Engine Architecture Clarifications

### D.1 CatalogInterface Abstraction

The parser library communicates with the catalog through an abstract `CatalogInterface`. This abstraction enables the same parser code to work in both embedded and server modes.

```cpp
class CatalogInterface {
public:
    virtual ~CatalogInterface() = default;

    // ===== Object Resolution =====
    // All lookups are transaction-isolated (MGA back-versioning applies)
    virtual UUID resolveObject(const SchemaPath& path, ObjectType type) = 0;
    virtual std::optional<TableInfo> getTableInfo(UUID uuid) = 0;
    virtual std::optional<ColumnInfo> getColumnInfo(UUID table_uuid, uint32_t col_idx) = 0;
    virtual std::vector<ColumnInfo> getTableColumns(UUID table_uuid) = 0;

    // ===== Session State (Server-Owned) =====
    virtual SchemaPath getCurrentSchema() = 0;
    virtual std::vector<SchemaPath> getSearchPath() = 0;
    virtual TransactionId getCurrentTransactionId() = 0;

    // ===== Catalog Version for Cache Invalidation =====
    virtual uint64_t getCatalogVersion() = 0;
};

// Embedded mode: Direct calls to engine
class EmbeddedCatalogInterface : public CatalogInterface { /* ... */ };

// Server mode via Unix socket
class SocketCatalogInterface : public CatalogInterface { /* ... */ };

// Server mode via network
class NetworkCatalogInterface : public CatalogInterface { /* ... */ };
```

### D.2 Session State Ownership

**Critical Design Decision:** Session state lives on the server, not the client/parser.

```
┌─────────────────────────────────────────────────────────────┐
│                    Server (Engine)                          │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                 Session State                        │   │
│  │  - Session ID (UUID)                                 │   │
│  │  - Current schema                                    │   │
│  │  - Search path                                       │   │
│  │  - Transaction context                               │   │
│  │  - User/Role/Group privileges                        │   │
│  │  - Catalog version at transaction start              │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              ▲
                              │ Session ID + Queries
                              │
┌─────────────────────────────────────────────────────────────┐
│                    Client (Parser)                          │
│  - Holds Session ID only                                    │
│  - Queries server for session state via CatalogInterface    │
│  - Local catalog cache (invalidated by version check)       │
└─────────────────────────────────────────────────────────────┘
```

**Session Attachment:** A client may crash and a new client instance may attach to an existing session using the Session ID. This enables:
- Recovery/cleanup of in-flight work
- Session continuity across client restarts
- Debugging by attaching a new client to observe session state

**Authentication:** Session attachment follows the security protocols defined in:
- `/docs/specifications/SECURITY_SYSTEM_SPECIFICATION.md`
- `/docs/specifications/SECURITY_IMPLIMENTATION_DETAILS.md`
- `/docs/specifications/Security Hardening Guide.md`

For local-only deployment (current phase): No authentication required.

### D.3 Caching Strategy

**Server-Side Query Plan Cache:**
```cpp
// Located on Server/Engine
class QueryPlanCache {
    // SBLR bytecode hash → Execution Plan
    std::unordered_map<size_t, ExecutionPlan> plans_;

    // Object UUID → Plan hashes (for invalidation)
    std::multimap<UUID, size_t> object_deps_;

    void invalidateByUUID(UUID object_uuid);
};
```

**Client-Side Catalog Cache:**
```cpp
// Located in Parser Library
class ParserCatalogCache {
    // Cached catalog metadata (schemas, tables, columns, types)
    std::unordered_map<UUID, TableInfo> table_cache_;
    std::unordered_map<UUID, std::vector<ColumnInfo>> column_cache_;

    // Cache version - checked on transaction start
    uint64_t cached_catalog_version_;

    // Invalidation on DDL
    void checkAndInvalidate(uint64_t server_catalog_version) {
        if (server_catalog_version != cached_catalog_version_) {
            // Clear all cached metadata
            table_cache_.clear();
            column_cache_.clear();
            cached_catalog_version_ = server_catalog_version;
        }
    }
};
```

**Cache Invalidation Protocol:**
1. Server maintains a monotonic `catalog_version` counter
2. DDL operations (CREATE, ALTER, DROP) increment `catalog_version`
3. On transaction start, client checks `catalog_version` via CatalogInterface
4. If version differs from cached version, client clears its local cache
5. This cuts down network traffic for common catalog lookups

### D.4 UUID-to-Name Mapping in Results

All error messages and results must use human-readable names. The execution result includes a UUID-to-name mapping:

```cpp
struct ExecutionResult {
    // Query results
    std::vector<Row> rows;
    std::vector<ColumnMetadata> columns;

    // UUID → Name mapping for client display
    struct ObjectNameMapping {
        UUID uuid;
        std::string schema_path;    // e.g., "hr.employees"
        std::string object_name;    // e.g., "employees"
        ObjectType type;
    };
    std::vector<ObjectNameMapping> object_names;

    // Column name mapping (column index → name)
    std::vector<std::string> column_names;

    // Errors use names, not UUIDs
    struct ErrorInfo {
        std::string message;        // Human-readable
        std::string object_name;    // If applicable
        SourceSpan location;        // Line/column in SQL
    };
    std::vector<ErrorInfo> errors;
};
```

The SBLR bytecode operates on UUIDs internally, but the result translator resolves UUIDs to names for client presentation.

### D.5 Transaction Isolation - ALWAYS

**Fundamental Rule:** ALL actions occur within a transaction. There is no "outside transaction" state.

```cpp
// Every CatalogInterface call is implicitly transaction-scoped
class CatalogInterface {
    // The implementation tracks current transaction ID
    // All catalog lookups respect MGA visibility rules

    virtual UUID resolveObject(const SchemaPath& path, ObjectType type) = 0;
    // Returns object UUID visible to current transaction (TIP lookup)
    // If object was created by uncommitted transaction → not visible
    // If object was dropped by uncommitted transaction → still visible
};
```

**MGA Back-Versioning Applies to Catalog:**
- Catalog metadata has transaction IDs (xmin)
- Parser sees catalog state as of its transaction start
- DDL by concurrent transactions is invisible until:
  1. That transaction commits
  2. Parser starts a new transaction AFTER the commit

**Embedded Mode:** Even in embedded/single-user mode, a transaction is always active. This ensures:
- ACID compliance
- Crash recovery
- Consistent behavior between embedded and server modes

### D.6 Deployment Modes

```
┌─────────────────────────────────────────────────────────────┐
│                    EMBEDDED MODE                            │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              Application Process                      │  │
│  │  ┌─────────────┐     ┌─────────────────────────────┐ │  │
│  │  │   Parser    │────▶│         Engine              │ │  │
│  │  │   Library   │     │  (Direct function calls)    │ │  │
│  │  └─────────────┘     └─────────────────────────────┘ │  │
│  │        │                         │                    │  │
│  │        └─────────┬───────────────┘                    │  │
│  │                  ▼                                    │  │
│  │           Database File                               │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    SERVER MODE (Local)                      │
│  ┌─────────────────┐          ┌───────────────────────────┐│
│  │ Client Process  │          │    Server Process         ││
│  │ ┌─────────────┐ │  Socket  │ ┌───────────────────────┐ ││
│  │ │   Parser    │─┼──────────┼▶│       Engine          │ ││
│  │ │   Library   │ │          │ │  (Session state here) │ ││
│  │ └─────────────┘ │          │ └───────────────────────┘ ││
│  └─────────────────┘          │           │               ││
│                               │           ▼               ││
│                               │    Database File          ││
│                               └───────────────────────────┘│
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    SERVER MODE (Network)                    │
│  ┌─────────────────┐          ┌───────────────────────────┐│
│  │ Client Machine  │ Network  │    Server Machine         ││
│  │ ┌─────────────┐ │   TCP    │ ┌───────────────────────┐ ││
│  │ │   Parser    │─┼──────────┼▶│       Engine          │ ││
│  │ │   Library   │ │          │ │  (Session state here) │ ││
│  │ └─────────────┘ │          │ └───────────────────────┘ ││
│  └─────────────────┘          │           │               ││
│                               │           ▼               ││
│                               │    Database File          ││
│                               └───────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

### D.7 SBLR Output Modes

The SBLR generator produces different output structures depending on the statement type and complexity.

#### D.7.1 Single Statement → Multiple Opcodes

A single SQL statement may compile to multiple SBLR opcodes:

```sql
INSERT INTO employees (id, name, dept) VALUES
    (1, 'Alice', 'Eng'),
    (2, 'Bob', 'Sales'),
    (3, 'Carol', 'Eng');
```

Could compile to:
- **Series of INSERT opcodes**: Individual row insertions
- **Batch INSERT opcode**: Single opcode with row array parameter
- **Compiled procedure block**: For complex logic with triggers

The SBLR generator chooses the most efficient representation based on:
- Row count
- Presence of triggers on target table
- Transaction isolation requirements
- Server-side optimization hints

#### D.7.2 Multi-Statement Blocks

Complex operations compile to SBLR procedure blocks:

```sql
BEGIN
    INSERT INTO audit_log VALUES (...);
    UPDATE accounts SET balance = balance - 100 WHERE id = 1;
    UPDATE accounts SET balance = balance + 100 WHERE id = 2;
    INSERT INTO transfers VALUES (...);
END;
```

Compiles to a single SBLR procedure block containing:
- Statement sequence
- Local variable declarations
- Control flow (if any)
- Exception handlers (if any)

This executes atomically within the current transaction.

#### D.7.3 EXECUTE PROCEDURE / Dynamic Execution

For `EXECUTE PROCEDURE` and dynamic SQL:

```sql
EXECUTE PROCEDURE transfer_funds(from_account := 1, to_account := 2, amount := 100);
```

The SBLR contains:
- Procedure UUID reference
- Parameter bindings
- Execution mode flags

The engine resolves the procedure at execution time, enabling:
- Late binding (procedure can be replaced)
- Parameter validation
- Privilege checking

### D.8 Source Code Preservation

Source SQL can optionally be preserved alongside SBLR bytecode for debugging, logging, analysis, and trigger context.

#### D.8.1 SBLRModule Structure

```cpp
struct SBLRModule {
    // ===== Required =====
    std::vector<uint8_t> bytecode;          // Executable SBLR
    UUID module_id;                          // Unique identifier
    std::vector<UUID> referenced_objects;    // For cache invalidation

    // ===== Optional - Source Preservation =====
    std::optional<std::string> source_sql;   // Original SQL text
    bool source_stored;                      // Was source preserved?

    // ===== Metadata =====
    uint64_t compiled_timestamp;
    uint64_t catalog_version;                // Catalog state at compile time
};
```

#### D.8.2 DDL Source Preservation

For DDL statements (`CREATE VIEW`, `CREATE PROCEDURE`, `CREATE TRIGGER`, etc.), source preservation enables:

- `SHOW CREATE VIEW view_name` - Retrieve original definition
- `SHOW CREATE PROCEDURE proc_name` - Retrieve procedure source
- Schema migration tools - Extract DDL for replication
- Documentation generation - Auto-document database objects

**Control:**
```sql
-- Database-level default
SET DATABASE OPTION STORE_DDL_SOURCE = TRUE;

-- Per-object override
CREATE VIEW my_view AS SELECT * FROM t WITH SOURCE;
CREATE PROCEDURE my_proc ... WITHOUT SOURCE;  -- Obfuscate/protect IP
```

**Catalog Storage:**
```cpp
struct ViewRecord {
    ID view_id;
    // ... existing fields ...
    uint32_t source_oid;         // TOAST reference to source SQL (0 if not stored)
    uint32_t sblr_oid;           // TOAST reference to compiled SBLR
};

struct ProcedureRecord {
    ID procedure_id;
    // ... existing fields ...
    uint32_t source_oid;         // TOAST reference to source SQL
    uint32_t sblr_oid;           // TOAST reference to compiled SBLR
};
```

#### D.8.3 DML Source Preservation

For DML statements (`SELECT`, `INSERT`, `UPDATE`, `DELETE`), source preservation enables:

- **Debugging**: Log actual SQL executed
- **Audit logging**: Record original statements with timestamps
- **Query analysis**: Profiling with actual SQL text
- **Trigger context**: Triggers can access the statement that fired them

**Execution Context:**
```cpp
struct ExecutionContext {
    SBLRModule* module;
    TransactionId xid;
    Session* session;

    // ===== Optional DML Source =====
    std::optional<std::string> source_sql;

    // ===== Trigger Context =====
    struct TriggerContext {
        std::string triggering_statement;    // SQL that caused trigger
        TriggerEvent event;                  // INSERT/UPDATE/DELETE
        std::optional<Row> old_row;          // For UPDATE/DELETE
        std::optional<Row> new_row;          // For INSERT/UPDATE
    };
    std::optional<TriggerContext> trigger_ctx;
};
```

**Control:**
```sql
-- Database-level (usually FALSE - too verbose for production)
SET DATABASE OPTION STORE_DML_SOURCE = FALSE;

-- Session-level (for debugging)
SET SESSION OPTION LOG_STATEMENTS = TRUE;

-- Query-level hint
SELECT /*+ PRESERVE_SOURCE */ * FROM employees WHERE dept = 'Eng';
```

#### D.8.4 Source Access in Triggers

SELECT triggers and audit triggers can access the triggering statement:

```sql
CREATE TRIGGER audit_changes
    AFTER INSERT OR UPDATE OR DELETE ON employees
    FOR EACH ROW
BEGIN
    INSERT INTO audit_log (
        table_name,
        operation,
        old_data,
        new_data,
        executed_sql,      -- From trigger context
        executed_by,
        executed_at
    ) VALUES (
        'employees',
        TG_OP,
        OLD,
        NEW,
        TG_SQL,            -- Built-in variable: triggering SQL
        CURRENT_USER,
        CURRENT_TIMESTAMP
    );
END;
```

#### D.8.5 Configuration Summary

| Setting | Scope | Default | Purpose |
|---------|-------|---------|---------|
| `STORE_DDL_SOURCE` | Database | TRUE | Preserve CREATE statement source |
| `STORE_DML_SOURCE` | Database | FALSE | Preserve DML statement source |
| `LOG_STATEMENTS` | Session | FALSE | Log all statements to debug log |
| `AUDIT_STATEMENTS` | Database | FALSE | Record statements in audit table |
| `WITH SOURCE` | Statement | (default) | Force source preservation |
| `WITHOUT SOURCE` | Statement | - | Force source omission |

### D.9 Prepared Statements / Parameterized Queries

#### D.9.1 Parameter Placeholders in SBLR

The parser generates SBLR with typed parameter placeholders:

```sql
PREPARE get_employee AS SELECT * FROM employees WHERE id = $1 AND dept = $2;
```

Compiles to SBLR containing:
```cpp
struct SBLRParameter {
    uint16_t param_index;        // $1 = 0, $2 = 1, etc.
    TypeId expected_type;        // INTEGER, VARCHAR, etc.
    bool nullable;               // Can parameter be NULL?
};
```

#### D.9.2 Plan Caching

- SBLR is cached on the server by **statement UUID**
- Plan cache checks for existing matching plans before generating new ones
- Plan cache has configurable:
  - Time-to-live (TTL)
  - Maximum entries
  - Eviction policy
  - Cross-client optimization (similar queries share plans)

#### D.9.3 Type Verification

- Parser defines the expected datatype for each parameter
- Engine verifies at execution time that provided values match expected types
- Each datatype has a `TypeId` that identifies it
- Type mismatch returns detailed error

#### D.9.4 Cross-Session Persistence

Prepared statements can span sessions:
- Reduces re-planning overhead
- Enables cross-client optimization of similar code
- Session attachment can access previously prepared statements

### D.10 Multi-Dialect Parser Architecture

#### D.10.1 Shared Infrastructure

All dialect parsers share:
- Same `CatalogInterface` abstraction
- Same SBLR bytecode format
- Same semantic analyzer core

#### D.10.2 Dialect-Specific Catalog Views

Each dialect has views that map expected system tables to the common catalog:

```
┌─────────────────────────────────────────────────────────────┐
│                 Common ScratchBird Catalog                  │
│  sys.catalog.tables, sys.catalog.columns, etc.              │
└─────────────────────────────────────────────────────────────┘
        │                   │                   │
        ▼                   ▼                   ▼
┌───────────────┐   ┌───────────────┐   ┌───────────────┐
│ PostgreSQL    │   │ MySQL Views   │   │ Firebird      │
│ Views         │   │               │   │ Views         │
│ pg_catalog.*  │   │ information_  │   │ RDB$*         │
│ information_  │   │ schema.*      │   │               │
│ schema.*      │   │               │   │               │
└───────────────┘   └───────────────┘   └───────────────┘
```

The recursive schema system isolates each dialect's namespace while providing the interfaces clients expect.

#### D.10.3 SBLR Equivalence

All dialects produce **identical SBLR** for equivalent statements:

| PostgreSQL | MySQL | Firebird | SBLR Result |
|------------|-------|----------|-------------|
| `LIMIT 10 OFFSET 5` | `LIMIT 5, 10` | `FIRST 10 SKIP 5` | Same SBLR |
| `SERIAL` | `AUTO_INCREMENT` | `GENERATOR` | Same SBLR |
| `||` concat | `CONCAT()` | `||` concat | Same SBLR |

The functionality is implemented in the engine; dialects map to SBLR that produces the expected result.

### D.11 Parser Error Handling

#### D.11.1 Error Recovery

- Parser does **NOT** attempt to recover and continue after syntax errors
- Parsing stops at first error
- Clear, actionable error message returned

#### D.11.2 Partial AST Support

Partial AST for error-tolerant tools:
- **ScratchBird native**: Supports partial AST return
- **Emulated dialects**: Depends on emulated server's behavior

#### D.11.3 Error Location Detail

Errors include:
- Line number
- Column number
- Highlighted problem text (source excerpt)

```cpp
struct ParseError {
    uint32_t line;
    uint32_t column;
    std::string message;
    std::string source_excerpt;      // Highlighted problem text
    std::string suggestion;          // Optional: "Did you mean...?"
};
```

### D.12 Expression Type Inference

#### D.12.1 Full Type Checking

The parser performs **full type checking** - offloading this work from the engine:

```sql
SELECT a + b FROM t WHERE c = 'hello'
```

Parser:
1. Queries catalog for types of columns `a`, `b`, `c`
2. Verifies `a + b` is valid (numeric types)
3. Verifies `c = 'hello'` is valid (string comparison)
4. Annotates AST with resolved types
5. Caches catalog lookups per D.3

#### D.12.2 Function Overload Resolution

Function overloads are resolved using **Firebird's resolution mechanics**:

1. Exact match preferred
2. Implicit conversions ranked by "distance"
3. Ambiguous overloads produce error

```sql
SELECT add(1, 2.5);  -- add(INT, INT) or add(FLOAT, FLOAT)?
-- Resolves to add(FLOAT, FLOAT) via implicit INT→FLOAT conversion
```

### D.13 Dependency Tracking

#### D.13.1 Parser Records Dependencies

When creating views, procedures, triggers, the parser records all dependencies:

```sql
CREATE VIEW v AS SELECT * FROM t1 JOIN t2 ON t1.id = t2.fk;
```

Dependencies recorded:
- `v → t1` (UUID)
- `v → t2` (UUID)
- `v → t1.id` (column UUID)
- `v → t2.fk` (column UUID)

#### D.13.2 UUID-Based Storage

Dependencies are stored by **UUID**, not name:
- Survives object renames
- Names resolved at display time from UUID→Name mapping

#### D.13.3 Drop Verification

`DROP` statements verify no objects depend on the target:

```sql
DROP TABLE t1;
-- Error: Cannot drop table t1: view v depends on it
-- Hint: Use DROP TABLE t1 CASCADE to drop dependent objects
```

With `CASCADE`:
- All dependent objects are dropped
- Dependency graph traversed recursively
- User informed of all dropped objects

### D.14 Dynamic SQL in Procedures

#### D.14.1 Dynamic SBLR Generation

Dynamic SQL is converted to **dynamic SBLR** - the equivalent using SBLR as the dynamic language:

```sql
EXECUTE STATEMENT 'SELECT * FROM ' || table_name || ' WHERE ' || col_name || ' = 1';
```

Compiles to SBLR that:
1. Concatenates SBLR fragments at runtime
2. Validates the resulting SBLR
3. Executes with current transaction context

#### D.14.2 Security Verification

SQL injection protection is verified at **both** parser and server levels:
- Parser validates structure of dynamic fragments
- Server validates final SBLR before execution
- See `/docs/specifications/SECURITY_SYSTEM_SPECIFICATION.md` for details

#### D.14.3 Parse-at-Runtime Instructions

The parser generates SBLR containing "parse at runtime" instructions:

```cpp
enum SBLROpcode {
    // ... existing opcodes ...
    OP_DYNAMIC_PARSE,       // Parse string as SBLR at runtime
    OP_DYNAMIC_EXECUTE,     // Execute dynamically generated SBLR
    OP_VALIDATE_SBLR,       // Security validation of dynamic SBLR
};
```

### D.15 Transaction Boundaries

#### D.15.1 Parser Transaction Awareness

The parser **must know** when transactions start/end:
- Catalog cache validity tied to transaction
- Session state changes on transaction boundaries
- Catalog version checked on transaction start

#### D.15.2 Auto-Commit Mode

Auto-commit is controlled via `SET` commands:

```sql
SET AUTOCOMMIT ON;   -- Each statement is its own transaction
SET AUTOCOMMIT OFF;  -- Manual COMMIT/ROLLBACK required
```

When auto-commit is ON:
- Each statement wrapped in implicit BEGIN/COMMIT
- Errors cause implicit ROLLBACK

#### D.15.3 SET TRANSACTION Behavior

`SET TRANSACTION` affects transaction boundaries:

```sql
SET TRANSACTION READ ONLY ISOLATION LEVEL SNAPSHOT;
```

Behavior:
1. If transaction is active: COMMIT (or ROLLBACK if dirty)
2. Start new transaction with specified settings
3. New settings apply to the new transaction

### D.16 SBLR Versioning

#### D.16.1 Version Number in Bytecode

SBLR bytecode includes a version header:

```cpp
struct SBLRHeader {
    uint32_t magic;              // 'SBLR'
    uint16_t version_major;      // Breaking changes
    uint16_t version_minor;      // Backward-compatible additions
    uint32_t flags;              // Feature flags
    uint32_t bytecode_length;
    // ... followed by bytecode
};
```

#### D.16.2 Version Compatibility

| Engine Version | SBLR Version | Result |
|----------------|--------------|--------|
| 2.0 | 1.x | Migration required |
| 2.0 | 2.0 | Execute directly |
| 2.0 | 2.1 | Execute (backward compatible) |
| 2.0 | 3.0 | Migration required |

#### D.16.3 Migration Tools

When SBLR format changes:
1. **If source preserved**: Recompile from source using new parser
2. **If source not preserved**: Migration tool reads old SBLR, generates new SBLR
3. Database upgrade process:
   - Scan all stored procedures, views, triggers
   - Recompile or migrate each
   - Update catalog with new SBLR OIDs

### D.17 Parser Hints / Optimizer Directives

#### D.17.1 Current Release

For this release:
- Parser does **NOT** extract hints from comments
- Full hint syntax support deferred to future release

#### D.17.2 Plan Suggestions

The parser can suggest execution strategies:

```cpp
struct SBLRModule {
    // ... existing fields ...

    // Plan suggestions from parser
    enum PlanHint {
        HINT_NONE,
        HINT_FORCE_NEW_PLAN,      // Don't use cached plan
        HINT_PREFER_CACHED,       // Use cached if available
        HINT_INDEX_SCAN,          // Suggest index scan
        HINT_SEQ_SCAN,            // Suggest sequential scan
    };
    std::vector<PlanHint> hints;
};
```

#### D.17.3 Future Hint Syntax

Hint syntax will be defined as the system evolves. Potential formats:
- Comment hints: `/*+ HINT */`
- Clause hints: `SELECT ... WITH (HINT)`
- Session hints: `SET OPTIMIZER_HINT = ...`

### D.18 Collation and Character Sets

#### D.18.1 Collation Clause Parsing

Parser handles collation clauses:

```sql
SELECT * FROM t WHERE name = 'café' COLLATE utf8_general_ci;
CREATE TABLE t (name VARCHAR(100) COLLATE utf8_bin);
```

#### D.18.2 String Literal Tagging

String literals in AST are tagged with character set:

```cpp
struct StringLiteral : public Expression {
    std::string value;
    CharacterSet charset;        // UTF8, LATIN1, etc.
    std::optional<Collation> collation;  // If explicitly specified
};
```

#### D.18.3 Default Collation Discovery

Parser retrieves database default collation via `CatalogInterface`:

```cpp
class CatalogInterface {
    // ... existing methods ...

    // Character set and collation
    virtual CharacterSet getDefaultCharset() = 0;
    virtual Collation getDefaultCollation() = 0;
    virtual std::vector<Collation> getAvailableCollations() = 0;
};
```

Collation can be:
- Queried: `SHOW COLLATION`
- Changed: `SET NAMES utf8 COLLATE utf8_general_ci`

### D.19 Complete CatalogInterface Specification

The `CatalogInterface` is the parser's abstraction for querying catalog metadata. This interface must support all operations needed for semantic analysis, type checking, and function overload resolution.

#### D.19.1 Core Interface Definition

```cpp
#include "scratchbird/core/types.h"
#include "scratchbird/core/uuidv7.h"
#include <vector>
#include <optional>
#include <string>

namespace scratchbird::parser {

using UUID = core::UuidV7Bytes;
using TypeId = core::DataType;

// Schema path representation (e.g., "sales.customers" or "sys.catalog.tables")
struct SchemaPath {
    std::vector<std::string> components;
    bool is_absolute;  // Starts with root schema

    std::string toString() const;
    static SchemaPath parse(const std::string& path);
    static SchemaPath current();     // "."
    static SchemaPath parent();      // ".."
};

// Object types in the catalog
enum class ObjectType : uint8_t {
    TABLE,
    VIEW,
    MATERIALIZED_VIEW,
    SEQUENCE,
    FUNCTION,
    PROCEDURE,
    TRIGGER,
    INDEX,
    TYPE,
    DOMAIN,
    SCHEMA,
    PACKAGE
};

// Function parameter mode
enum class ParamMode : uint8_t {
    IN,
    OUT,
    INOUT
};

// Function signature for overload resolution
struct FunctionSignature {
    UUID function_uuid;
    std::string name;
    std::vector<TypeId> param_types;
    std::vector<ParamMode> param_modes;
    std::vector<std::string> param_names;
    std::vector<std::optional<std::string>> param_defaults;  // Default value expressions
    TypeId return_type;
    bool is_aggregate;
    bool is_window;
    bool is_variadic;        // Last param accepts multiple values
    bool is_deterministic;   // Same inputs always produce same output
};

// Column information
struct ColumnInfo {
    UUID column_uuid;
    std::string name;
    TypeId type;
    uint32_t precision;
    uint32_t scale;
    bool nullable;
    std::optional<std::string> default_expr;
    bool is_generated;
    bool is_identity;
    uint32_t ordinal_position;
};

// Table/View information
struct TableInfo {
    UUID table_uuid;
    std::string name;
    SchemaPath schema_path;
    ObjectType type;  // TABLE, VIEW, MATERIALIZED_VIEW
    std::vector<ColumnInfo> columns;
    UUID owner_uuid;
    uint64_t last_ddl_xid;  // Transaction ID of last DDL modification
};

// Index information
struct IndexInfo {
    UUID index_uuid;
    std::string name;
    UUID table_uuid;
    std::vector<UUID> column_uuids;
    std::vector<std::string> expressions;  // For expression indexes
    bool is_unique;
    bool is_primary;
    std::string index_type;  // "btree", "hash", "gin", etc.
};

// Type conversion rule
struct TypeConversion {
    TypeId from_type;
    TypeId to_type;
    uint8_t distance;        // Lower = better match (0 = exact)
    bool is_implicit;        // Can convert without explicit CAST
    bool is_assignment;      // Can convert in assignment context
};

// Collation information
struct CollationInfo {
    uint32_t collation_id;
    std::string name;
    std::string provider;    // "libc", "icu"
    std::string locale;
};

// Character set information
struct CharsetInfo {
    uint16_t charset_id;
    std::string name;
    uint8_t bytes_per_char;
};

/**
 * CatalogInterface - Abstract interface for catalog access
 *
 * All operations are transaction-isolated (MGA back-versioning applies).
 * The implementation tracks the current transaction ID and uses TIP-based
 * visibility to return only committed, visible catalog entries.
 */
class CatalogInterface {
public:
    virtual ~CatalogInterface() = default;

    // ===== Transaction Context =====

    // Get current transaction ID (for visibility checks)
    virtual uint64_t getCurrentTransactionId() = 0;

    // Get catalog version for cache invalidation
    // Monotonically increasing; bumped by any DDL operation
    virtual uint64_t getCatalogVersion() = 0;

    // ===== Object Resolution =====

    // Resolve an object name to UUID using search path
    // Returns nullopt if not found or not visible to current transaction
    virtual std::optional<UUID> resolveObject(
        const std::string& name,
        ObjectType type
    ) = 0;

    // Resolve with explicit schema path (ignores search path)
    virtual std::optional<UUID> resolveObjectInSchema(
        const SchemaPath& schema,
        const std::string& name,
        ObjectType type
    ) = 0;

    // ===== Table/View Operations =====

    virtual std::optional<TableInfo> getTableInfo(UUID table_uuid) = 0;
    virtual std::vector<ColumnInfo> getTableColumns(UUID table_uuid) = 0;
    virtual std::optional<ColumnInfo> getColumnByName(UUID table_uuid, const std::string& name) = 0;
    virtual std::optional<ColumnInfo> getColumnByIndex(UUID table_uuid, uint32_t index) = 0;

    // ===== Index Operations =====

    virtual std::vector<IndexInfo> getTableIndexes(UUID table_uuid) = 0;
    virtual std::optional<IndexInfo> getIndexInfo(UUID index_uuid) = 0;

    // ===== Function/Procedure Operations =====

    // Get all overloads of a function by name
    virtual std::vector<FunctionSignature> getFunctionOverloads(
        const std::string& name
    ) = 0;

    // Get overloads in specific schema
    virtual std::vector<FunctionSignature> getFunctionOverloadsInSchema(
        const SchemaPath& schema,
        const std::string& name
    ) = 0;

    // Resolve best matching overload for given argument types
    // Returns nullopt if no match or ambiguous
    // Uses ScratchBird function resolution rules (Section D.20)
    virtual std::optional<UUID> resolveOverload(
        const std::string& name,
        const std::vector<TypeId>& arg_types
    ) = 0;

    // Get function signature by UUID
    virtual std::optional<FunctionSignature> getFunctionSignature(UUID function_uuid) = 0;

    // ===== Type System =====

    // Get type information
    virtual std::optional<core::TypeInfo> getTypeInfo(TypeId type_id) = 0;

    // Get domain information (user-defined type with constraints)
    virtual std::optional<UUID> resolveDomain(const std::string& name) = 0;
    virtual std::optional<TypeId> getDomainBaseType(UUID domain_uuid) = 0;

    // Type conversion rules
    virtual std::vector<TypeConversion> getImplicitConversions(TypeId from_type) = 0;
    virtual bool canImplicitConvert(TypeId from_type, TypeId to_type) = 0;
    virtual uint8_t getConversionDistance(TypeId from_type, TypeId to_type) = 0;

    // ===== Session State =====

    virtual SchemaPath getCurrentSchema() = 0;
    virtual std::vector<SchemaPath> getSearchPath() = 0;
    virtual void setCurrentSchema(const SchemaPath& schema) = 0;
    virtual void setSearchPath(const std::vector<SchemaPath>& paths) = 0;

    // ===== Character Set and Collation =====

    virtual CharsetInfo getDefaultCharset() = 0;
    virtual CollationInfo getDefaultCollation() = 0;
    virtual std::vector<CollationInfo> getAvailableCollations() = 0;
    virtual std::optional<CollationInfo> getCollationByName(const std::string& name) = 0;

    // ===== Dependency Tracking =====

    // Record that object A depends on object B
    virtual void recordDependency(UUID dependent, UUID referenced) = 0;

    // Get all objects that depend on a given object
    virtual std::vector<UUID> getDependents(UUID object_uuid) = 0;

    // Get all objects that a given object depends on
    virtual std::vector<UUID> getReferences(UUID object_uuid) = 0;

    // ===== Privilege Checking =====

    // Check if current user has privilege on object
    virtual bool hasPrivilege(UUID object_uuid, const std::string& privilege) = 0;

    // Get current user UUID
    virtual UUID getCurrentUserUUID() = 0;

    // Get active role UUID
    virtual std::optional<UUID> getActiveRoleUUID() = 0;
};

// Concrete implementations
class EmbeddedCatalogInterface : public CatalogInterface { /* ... */ };
class SocketCatalogInterface : public CatalogInterface { /* ... */ };
class NetworkCatalogInterface : public CatalogInterface { /* ... */ };

} // namespace scratchbird::parser
```

### D.20 Function Resolution Rules (ScratchBird Approach)

ScratchBird implements its own function resolution system that is **more flexible than Firebird** (which does not support overloading) but **follows similar type conversion rules**.

**Key difference from Firebird:** ScratchBird DOES support function overloading. Firebird does not allow multiple functions with the same name. ScratchBird allows overloads and resolves them using type distance calculations.

#### D.20.1 Core Resolution Algorithm

```cpp
/**
 * Function Overload Resolution Algorithm
 *
 * Given: function name and list of argument types
 * Returns: Best matching function UUID, or error if none/ambiguous
 */
std::optional<UUID> resolveOverload(
    const std::string& name,
    const std::vector<TypeId>& arg_types,
    CatalogInterface* catalog
) {
    // Step 1: Get all overloads
    auto overloads = catalog->getFunctionOverloads(name);
    if (overloads.empty()) {
        return std::nullopt;  // No function with this name
    }

    // Step 2: Filter by argument count compatibility
    std::vector<std::pair<FunctionSignature, uint32_t>> candidates;
    for (const auto& sig : overloads) {
        if (isArgumentCountCompatible(sig, arg_types.size())) {
            uint32_t total_distance = calculateTotalDistance(sig, arg_types, catalog);
            if (total_distance != INCOMPATIBLE) {
                candidates.push_back({sig, total_distance});
            }
        }
    }

    if (candidates.empty()) {
        return std::nullopt;  // No compatible overload
    }

    // Step 3: Sort by total conversion distance
    std::sort(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) {
            return a.second < b.second;  // Lower distance is better
        });

    // Step 4: Check for ambiguity
    if (candidates.size() > 1 && candidates[0].second == candidates[1].second) {
        throw AmbiguousOverloadError(name, arg_types);
    }

    return candidates[0].first.function_uuid;
}
```

#### D.20.2 Type Conversion Distance Rules

Type conversion distances are used to rank overload candidates:

| Conversion | Distance | Notes |
|------------|----------|-------|
| Exact match | 0 | No conversion needed |
| INT8 → INT16 | 1 | Safe widening |
| INT16 → INT32 | 1 | Safe widening |
| INT32 → INT64 | 1 | Safe widening |
| INT32 → FLOAT64 | 2 | May lose precision |
| INT64 → FLOAT64 | 3 | May lose precision |
| FLOAT32 → FLOAT64 | 1 | Safe widening |
| VARCHAR(n) → TEXT | 1 | Safe widening |
| CHAR(n) → VARCHAR(m) | 2 | If m >= n |
| Any → VARCHAR | 3 | String conversion |
| Domain → Base type | 0 | Domain unwrapping |
| NULL → Any | 0 | NULL matches any type |

#### D.20.3 Resolution Priority

1. **Exact match**: All argument types match parameter types exactly
2. **Implicit conversions only**: Uses implicit conversions (no explicit CAST)
3. **Lower total distance wins**: Sum of conversion distances across all arguments
4. **Ambiguity error**: If two candidates have equal distance

#### D.20.4 Special Cases

**Variadic functions:**
```sql
-- format(text, ...) accepts any number of arguments after the first
SELECT format('Name: %s, Age: %d', name, age) FROM users;
```

**Named arguments (future enhancement):**
```sql
-- Named arguments skip positional matching
SELECT my_func(param2 => 'value', param1 => 42);
```

**Default parameters:**
```sql
CREATE FUNCTION greet(name VARCHAR, greeting VARCHAR DEFAULT 'Hello')
    RETURNS VARCHAR AS ...

-- Can call with 1 or 2 arguments
SELECT greet('World');           -- Uses default for greeting
SELECT greet('World', 'Hi');     -- Explicit greeting
```

#### D.20.5 Firebird Type Coercion Rules (Adopted)

ScratchBird adopts Firebird's strict type coercion rules from SQL Dialect 3:

1. **Minimal implicit conversion** in expressions - CAST is usually required
2. **Comparison exception**: In predicates, implicit string→other conversion is allowed
3. **String concatenation exception**: Non-strings implicitly convert to string for `||`

```sql
-- Dialect 3 behavior
SELECT 1 + '2';        -- Error: Cannot add integer and string
SELECT 1 + CAST('2' AS INTEGER);  -- OK: 3
SELECT 'value: ' || 42;            -- OK: 'value: 42' (implicit conversion)
SELECT * FROM t WHERE id = '123';  -- OK: '123' converts to integer for comparison
```

### D.21 TypeId System with Fixed UUIDs

#### D.21.1 Built-in Type UUIDs

Built-in data types have **fixed UUIDs** that are constant across all databases. This ensures SBLR bytecode is portable and type checks are consistent.

```cpp
namespace scratchbird::core {

// Fixed UUIDs for built-in types
// Format: 00000000-0000-0000-0000-0000000000XX where XX = DataType value
//
// These UUIDs are embedded in SBLR bytecode and must NEVER change.
// New types receive the next sequential XX value.

constexpr UuidV7Bytes TYPE_UUID_INT8      = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x01};
constexpr UuidV7Bytes TYPE_UUID_INT16     = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x02};
constexpr UuidV7Bytes TYPE_UUID_INT32     = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x03};
constexpr UuidV7Bytes TYPE_UUID_INT64     = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x04};
constexpr UuidV7Bytes TYPE_UUID_INT128    = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x05};
constexpr UuidV7Bytes TYPE_UUID_UINT8     = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x06};
constexpr UuidV7Bytes TYPE_UUID_UINT16    = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x07};
constexpr UuidV7Bytes TYPE_UUID_UINT32    = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x08};
constexpr UuidV7Bytes TYPE_UUID_UINT64    = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x09};
constexpr UuidV7Bytes TYPE_UUID_FLOAT32   = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x0A};
constexpr UuidV7Bytes TYPE_UUID_FLOAT64   = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x0B};
constexpr UuidV7Bytes TYPE_UUID_DECIMAL   = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x0C};
constexpr UuidV7Bytes TYPE_UUID_MONEY     = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x0D};
// ... continuing for all DataType values ...
constexpr UuidV7Bytes TYPE_UUID_CHAR      = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x14};
constexpr UuidV7Bytes TYPE_UUID_VARCHAR   = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x15};
constexpr UuidV7Bytes TYPE_UUID_TEXT      = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x16};
constexpr UuidV7Bytes TYPE_UUID_DATE      = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x28};
constexpr UuidV7Bytes TYPE_UUID_TIME      = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x29};
constexpr UuidV7Bytes TYPE_UUID_TIMESTAMP = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x2A};
constexpr UuidV7Bytes TYPE_UUID_BOOLEAN   = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x32};
constexpr UuidV7Bytes TYPE_UUID_UUID      = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x3C};
constexpr UuidV7Bytes TYPE_UUID_JSON      = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x3D};
constexpr UuidV7Bytes TYPE_UUID_JSONB     = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x3E};
// etc.

// Lookup functions
UuidV7Bytes getTypeUUID(DataType type);
DataType getTypeFromUUID(const UuidV7Bytes& uuid);

} // namespace scratchbird::core
```

#### D.21.2 User-Defined Types and Domains

Domains and user-defined types receive **dynamically generated UUIDs** (standard UUIDv7):

```cpp
// Creating a domain generates a new UUID
CREATE DOMAIN email_address AS VARCHAR(255)
    CHECK (VALUE LIKE '%@%.%');
// uuid = <generated UUIDv7>

// The SBLR references this UUID
// If database is migrated, the UUID travels with the domain
```

#### D.21.3 Type Validation and Casting

```cpp
struct TypeValidator {
    UUID type_uuid;

    // Validation function pointer (for domains with CHECK constraints)
    using ValidateFn = bool (*)(const TypedValue& value);
    ValidateFn validate;

    // Cast function pointer (for custom types)
    using CastFn = TypedValue (*)(const TypedValue& value, TypeId target_type);
    CastFn cast_to;
    CastFn cast_from;
};
```

### D.22 Plan Cache Matching Algorithm

#### D.22.1 Exact SBLR Byte Match

Plan cache matching uses **exact SBLR bytecode hash match**:

```cpp
struct PlanCacheKey {
    // SHA-256 hash of SBLR bytecode
    std::array<uint8_t, 32> sblr_hash;

    // Database UUID (plans are database-specific)
    UUID database_uuid;

    bool operator==(const PlanCacheKey& other) const {
        return sblr_hash == other.sblr_hash && database_uuid == other.database_uuid;
    }
};

struct PlanCacheKeyHash {
    size_t operator()(const PlanCacheKey& key) const {
        // Use first 8 bytes of SHA-256 as hash
        return *reinterpret_cast<const size_t*>(key.sblr_hash.data());
    }
};

class PlanCache {
    std::unordered_map<PlanCacheKey, CachedPlan, PlanCacheKeyHash> cache_;

    // TTL and eviction settings
    std::chrono::seconds ttl_ = std::chrono::hours(1);
    size_t max_entries_ = 10000;

    // Object UUID → Plan keys (for invalidation)
    std::multimap<UUID, PlanCacheKey> object_dependencies_;
};
```

#### D.22.2 Cache Invalidation

Plans are invalidated when referenced objects change:

```cpp
void PlanCache::invalidateByObject(UUID object_uuid) {
    auto range = object_dependencies_.equal_range(object_uuid);
    for (auto it = range.first; it != range.second; ++it) {
        cache_.erase(it->second);
    }
    object_dependencies_.erase(object_uuid);
}
```

### D.23 Catalog Version and Cache Invalidation

#### D.23.1 Per-Object Modification Tracking

Each catalog object stores the **transaction ID of its last DDL modification**:

```cpp
struct CatalogEntry {
    UUID object_uuid;
    uint64_t xmin;           // Transaction that created this version
    uint64_t xmax;           // Transaction that deleted this version (0 = active)
    uint64_t last_ddl_xid;   // Last DDL transaction (for cache invalidation)
    // ... other fields
};
```

#### D.23.2 Client-Side Cache Invalidation

```cpp
class ParserCatalogCache {
    struct CachedEntry {
        TableInfo info;
        uint64_t cached_at_xid;  // Transaction ID when cached
    };

    std::unordered_map<UUID, CachedEntry> table_cache_;

    std::optional<TableInfo> getTableInfo(UUID uuid, CatalogInterface* catalog) {
        auto it = table_cache_.find(uuid);
        if (it != table_cache_.end()) {
            // Check if cache is still valid
            auto current_info = catalog->getTableInfo(uuid);
            if (current_info && current_info->last_ddl_xid <= it->second.cached_at_xid) {
                return it->second.info;  // Cache hit
            }
            // Cache stale, remove
            table_cache_.erase(it);
        }

        // Cache miss, fetch from catalog
        auto info = catalog->getTableInfo(uuid);
        if (info) {
            table_cache_[uuid] = {*info, catalog->getCurrentTransactionId()};
        }
        return info;
    }
};
```

### D.24 Monitoring Tables (MON$-style)

ScratchBird provides memory-only monitoring tables similar to Firebird's MON$ tables. These are virtual tables that reflect runtime state.

#### D.24.1 Session Monitoring

```sql
-- MON$SESSIONS - Active and detached sessions
CREATE VIEW mon$sessions AS ...

Columns:
- session_id: UUID - Unique session identifier
- user_id: UUID - User who owns the session
- status: VARCHAR - 'ACTIVE', 'DETACHED', 'IDLE'
- attached_client_id: UUID NULL - Currently attached client (NULL if detached)
- connect_time: TIMESTAMP - When session was created
- last_activity_time: TIMESTAMP - Last activity timestamp
- current_transaction_id: BIGINT NULL - Active transaction ID
- current_schema: VARCHAR - Current schema path
- client_address: VARCHAR - Client IP address
- client_application: VARCHAR - Application name
```

#### D.24.2 Transaction Monitoring

```sql
-- MON$TRANSACTIONS - Active transactions
CREATE VIEW mon$transactions AS ...

Columns:
- transaction_id: BIGINT - Transaction ID (XID)
- session_id: UUID - Owning session
- state: VARCHAR - 'ACTIVE', 'COMMITTED', 'ROLLED_BACK', 'LIMBO'
- start_time: TIMESTAMP - Transaction start time
- isolation_level: VARCHAR - 'READ_COMMITTED', 'REPEATABLE_READ', 'SERIALIZABLE'
- read_only: BOOLEAN - Is read-only transaction
- lock_timeout: INTEGER - Lock wait timeout in seconds
- oldest_active: BIGINT - Oldest active transaction at start (OAT)
```

#### D.24.3 Statement Monitoring

```sql
-- MON$STATEMENTS - Active and recent statements
CREATE VIEW mon$statements AS ...

Columns:
- statement_id: UUID - Unique statement identifier
- session_id: UUID - Owning session
- transaction_id: BIGINT - Transaction context
- sql_text: TEXT NULL - Original SQL (if preserved)
- sblr_hash: VARCHAR - Hash of SBLR bytecode
- state: VARCHAR - 'EXECUTING', 'COMPLETED', 'FAILED'
- start_time: TIMESTAMP - Execution start time
- end_time: TIMESTAMP NULL - Execution end time
- rows_affected: BIGINT - Rows affected/returned
- plan_cache_hit: BOOLEAN - Was plan from cache
```

#### D.24.4 Session Attachment

Orphaned sessions can be found and attached:

```sql
-- Find detached sessions for current user
SELECT session_id, connect_time, last_activity_time, current_schema
FROM mon$sessions
WHERE user_id = CURRENT_USER_ID
  AND status = 'DETACHED';

-- Attach to session (via client API, not SQL)
-- Client calls: session.attach(session_id)
```

Session timeout and cleanup is controlled by database configuration settings.

### D.25 Security Reference

For full authentication and authorization details, see:
- `/docs/specifications/SECURITY_SYSTEM_SPECIFICATION.md` - Core security model
- `/docs/specifications/SECURITY_IMPLIMENTATION_DETAILS.md` - Implementation details
- `/docs/specifications/Security Hardening Guide.md` - Hardening recommendations

Key points relevant to parser:
- Session state includes User ID, active Role ID, Group IDs
- Privilege checks happen BEFORE execution (not during MGA visibility)
- Parser may need to check CREATE privilege before parsing DDL
- SHOW commands are permission-filtered (parser may need privilege info)
- Dynamic SQL has dual validation (parser + server) for SQL injection protection

---

**End of Implementation Plan**
