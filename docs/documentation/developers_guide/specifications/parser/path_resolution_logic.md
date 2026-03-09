# Specification: Path Resolution Logic

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | parser |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0 |
| **Authors** | Generated from source analysis |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/schema_path_v3.h:61` - PathType enum
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/schema_path_v3.h:78` - SchemaPath struct
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/schema_path_v3.cpp:83` - parseSchemaPath() implementation
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/schema_path_v3.cpp:185` - parseTableRef() implementation
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/schema_path_v3.cpp:222` - parseColumnRef() implementation
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/lexer_v3.h:136-138` - Path prefix tokens (DOT, DOUBLE_DOT, EXCLAIM_COLON)
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_state_v3.cpp` - Parser state tests

## Synopsis

This specification defines the hierarchical schema path resolution system for ScratchBird, implementing directory-like namespace navigation with four path types: unqualified (search path), current schema relative (.), parent schema relative (..), and absolute (schema.name).

## Scope

### In Scope

- Path type definitions (UNQUALIFIED, CURRENT, PARENT, ABSOLUTE)
- No-search-path prefix (!:)
- Schema path parsing and component extraction
- Table reference parsing with optional aliases
- Column reference parsing with table qualifiers

### Out of Scope

- Search path resolution algorithm (handled by catalog)
- Schema existence validation (handled by semantic analyzer)
- Path normalization for symlinks

## Background

ScratchBird uses a hierarchical namespace similar to filesystem paths:
- `.` prefix references current schema context
- `..` prefix references parent schema context
- No prefix uses search path resolution (unqualified)
- `schema.name` format provides absolute path from root
- `!:` prefix disables search path for unqualified names

## Specification

### Data Structures

#### PathType Enum

```cpp
// From include/scratchbird/parser/schema_path_v3.h:61
enum class PathType : uint8_t {
    UNQUALIFIED,    // name (uses search path)
    CURRENT,        // .name or .path.name (relative to current schema)
    PARENT,         // ..name or ..path.name (relative to parent schema)
    ABSOLUTE        // schema.name or schema.path.name (from root)
};
```

#### SchemaPath Structure

```cpp
// From include/scratchbird/parser/schema_path_v3.h:78
struct SchemaPath {
    PathType type = PathType::UNQUALIFIED;
    bool no_search_path = false;  // True when prefixed with !:
    std::vector<StringPool::StringId> components;
    SourceSpan span;  // Source location for error reporting

    // Query methods
    bool isQualified() const { return type != PathType::UNQUALIFIED; }
    bool isEmpty() const { return components.empty(); }
    size_t depth() const { return components.size(); }

    // Get the object name (last component)
    StringPool::StringId objectName() const {
        return components.empty() ? StringPool::INVALID_ID : components.back();
    }

    // Get the schema path (all but last component)
    std::vector<StringPool::StringId> schemaComponents() const {
        if (components.size() <= 1) return {};
        return std::vector<StringPool::StringId>(components.begin(), components.end() - 1);
    }
};
```

#### TableRef Structure

```cpp
// From include/scratchbird/parser/schema_path_v3.h:165
struct TableRef {
    SchemaPath path;
    StringPool::StringId alias = StringPool::INVALID_ID;
    bool has_alias = false;
    SourceSpan span;
};
```

#### ColumnRef Structure

```cpp
// From include/scratchbird/parser/schema_path_v3.h:188
struct ColumnRef {
    SchemaPath table_path;      // Empty for unqualified column references
    StringPool::StringId column_name = StringPool::INVALID_ID;
    bool has_table_qualifier = false;
    SourceSpan span;
};
```

### Path Syntax Grammar (EBNF)

```ebnf
<object_path> ::= [ <no_search_prefix> ] <schema_path>

<no_search_prefix> ::= "!" ":"

<schema_path> ::= <unqualified>
                | <current_path>
                | <parent_path>
                | <absolute_path>

<unqualified> ::= <identifier>

<current_path> ::= DOT <identifier> [ DOT <identifier> ]*

<parent_path> ::= DOUBLE_DOT <identifier> [ DOT <identifier> ]*

<absolute_path> ::= <identifier> DOT <identifier> [ DOT <identifier> ]*

<table_ref> ::= <object_path> [ [ AS ] <identifier> ]

<column_ref> ::= <object_path> DOT <identifier>   (* fully qualified *)
               | <identifier> [ DOT <identifier> ]  (* unqualified or table-qualified *)
```

### Interface Contracts

#### Function: `parseSchemaPath()`

```cpp
// Source: src/parser/schema_path_v3.cpp:83
SchemaPath parseSchemaPath(ParserState& state);
```

**Preconditions:**
- ParserState positioned at potential path start
- Current token is one of: DOT, DOUBLE_DOT, EXCLAIM_COLON, or IDENTIFIER

**Postconditions:**
- Returns populated SchemaPath with type and components
- ParserState advanced past end of path
- Span set to cover entire path text

**Algorithm:**
```
Input:  ParserState positioned at path start
Output: SchemaPath structure

1. Check for !: prefix
   if state.check(TokenType::EXCLAIM_COLON):
       path.no_search_path = true
       state.advance()

2. Determine path type based on starting token:
   
   Case A: state.check(TokenType::DOT)
       path.type = PathType::CURRENT
       state.advance()  // consume DOT
       
       // Must have at least one identifier after DOT
       if !state.isIdentifier():
           return path  // Empty path (error case)
       
       // Collect path components: .name or .path.name
       while state.isIdentifier():
           path.components.push_back(state.currentStringId())
           state.advance()
           if state.check(TokenType::DOT):
               state.advance()
           else:
               break
   
   Case B: state.check(TokenType::DOUBLE_DOT)
       path.type = PathType::PARENT
       state.advance()  // consume DOUBLE_DOT
       
       // Must have at least one identifier after ..
       if !state.isIdentifier():
           return path
       
       // Collect path components: ..name or ..path.name
       while state.isIdentifier():
           path.components.push_back(state.currentStringId())
           state.advance()
           if state.check(TokenType::DOT):
               state.advance()
           else:
               break
   
   Case C: state.isIdentifier()
       // Get first identifier
       path.components.push_back(state.currentStringId())
       state.advance()
       
       // Check if followed by DOT (making it absolute)
       if state.check(TokenType::DOT):
           path.type = PathType::ABSOLUTE
           
           // Collect remaining components: schema.name.name
           while state.check(TokenType::DOT):
               state.advance()
               if !state.isIdentifier():
                   break  // Error: expected identifier
               path.components.push_back(state.currentStringId())
               state.advance()
       else:
           // Just a single identifier - unqualified
           path.type = PathType::UNQUALIFIED

3. Set span from start to current position
   path.span = SourceSpan(start, current_offset - start_offset)
   
4. Return path
```

#### Function: `parseTableRef()`

```cpp
// Source: src/parser/schema_path_v3.cpp:185
TableRef parseTableRef(ParserState& state);
```

**Preconditions:**
- ParserState positioned at table reference start

**Postconditions:**
- Returns TableRef with path and optional alias
- Alias recognized with or without AS keyword

**Algorithm:**
```
Input:  ParserState at table reference
Output: TableRef structure

1. Record start location
   SourceLocation start = state.currentLocation()

2. Parse the schema path
   SchemaPath path = parseSchemaPath(state)

3. Initialize TableRef
   TableRef ref
   ref.path = path

4. Check for optional alias: [ AS ] identifier
   if state.match(TokenType::KW_AS):
       // Explicit AS keyword
       if state.isIdentifier():
           ref.alias = state.currentStringId()
           ref.has_alias = true
           state.advance()
   else if state.isIdentifier():
       // Implicit alias (no AS keyword)
       ref.alias = state.currentStringId()
       ref.has_alias = true
       state.advance()

5. Set span
   ref.span = SourceSpan(start, current_offset - start_offset)
   return ref
```

#### Function: `parseColumnRef()`

```cpp
// Source: src/parser/schema_path_v3.cpp:222
ColumnRef parseColumnRef(ParserState& state);
```

**Preconditions:**
- ParserState positioned at column reference start

**Postconditions:**
- Returns ColumnRef with column name and optional table path
- Last component is always column name

**Algorithm:**
```
Input:  ParserState at column reference
Output: ColumnRef structure

1. Record start location

2. Special case: path-qualified column (.table.column, ..table.column)
   if state.check(TokenType::DOT) || state.check(TokenType::DOUBLE_DOT):
       SchemaPath full_path = parseSchemaPath(state)
       
       if full_path.components.empty():
           return empty ColumnRef (error)
       
       // Last component is column name, rest is table path
       ColumnRef ref
       ref.column_name = full_path.components.back()
       
       if full_path.components.size() > 1:
           ref.has_table_qualifier = true
           ref.table_path.type = full_path.type
           ref.table_path.components = full_path.components[0..n-1]
       
       ref.span = computed span
       return ref

3. Must start with identifier
   if !state.isIdentifier():
       return empty ColumnRef

4. Collect identifiers separated by dots
   vector<StringPool::StringId> parts
   parts.push_back(state.currentStringId())
   state.advance()
   
   while state.check(TokenType::DOT):
       state.advance()
       if !state.isIdentifier():
           break
       parts.push_back(state.currentStringId())
       state.advance()

5. Build ColumnRef from parts
   ColumnRef ref
   ref.span = computed span
   
   if parts.size() == 1:
       // Simple column: column_name
       ref.column_name = parts[0]
       ref.has_table_qualifier = false
   else:
       // Qualified: table.column or schema.table.column
       ref.column_name = parts.back()
       ref.has_table_qualifier = true
       
       if parts.size() == 2:
           // table.column - unqualified table reference
           ref.table_path.type = PathType::UNQUALIFIED
           ref.table_path.components.push_back(parts[0])
       else:
           // schema.table.column - absolute
           ref.table_path.type = PathType::ABSOLUTE
           ref.table_path.components = parts[0..n-2]
   
   return ref
```

### Path Type Resolution Examples

| Input Path | PathType | no_search_path | Components | Meaning |
|------------|----------|----------------|------------|---------|
| `orders` | UNQUALIFIED | false | ["orders"] | Search path resolution |
| `!:orders` | UNQUALIFIED | true | ["orders"] | Current schema only |
| `.orders` | CURRENT | false | ["orders"] | Current schema relative |
| `.sub.orders` | CURRENT | false | ["sub", "orders"] | Nested current schema |
| `..orders` | PARENT | false | ["orders"] | Parent schema relative |
| `..sibling.tbl` | PARENT | false | ["sibling", "tbl"] | Parent then nested |
| `sys.users` | ABSOLUTE | false | ["sys", "users"] | Absolute from root |
| `!:sys.users` | ABSOLUTE | true | ["sys", "users"] | Absolute, no search |

### State Machines

#### Path Parsing State Machine

```
                         ┌──────────────────┐
                         │      Start       │
                         └────────┬─────────┘
                                  │
          ┌───────────────────────┼───────────────────────┐
          │                       │                       │
          ▼                       ▼                       ▼
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│  EXCLAIM_COLON  │    │      DOT        │    │   IDENTIFIER    │
│     (!:)        │    │      (.)        │    │                 │
└────────┬────────┘    └────────┬────────┘    └────────┬────────┘
         │                      │                      │
         │ no_search=true       │ type=CURRENT         │ Lookahead
         │                      │                      │ for DOT
         ▼                      ▼                      ▼
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│  Check DOT or   │    │ Require IDENT   │    │  Next=DOT?      │
│  DOUBLE_DOT or  │    │ Collect comps   │    │  Yes → ABSOLUTE │
│  IDENTIFIER     │    │                 │    │  No  → UNQUAL   │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

### Decision Tree: Path Type Selection

```
Starts with !:?
├── Yes → no_search_path = true, continue to prefix check
└── No  → no_search_path = false, continue to prefix check

Prefix check:
├── Starts with DOT (.)?
│   └── Yes → PathType::CURRENT
├── Starts with DOUBLE_DOT (..)?
│   └── Yes → PathType::PARENT
└── Starts with IDENTIFIER?
    ├── Followed by DOT?
    │   └── Yes → PathType::ABSOLUTE
    └── No → PathType::UNQUALIFIED
```

### Token Definitions

Path prefixes are defined as lexer tokens:

```cpp
// From include/scratchbird/parser/lexer_v3.h:136-138
DOT,                  // .  - current schema navigation
DOUBLE_DOT,           // .. - parent schema navigation
EXCLAIM_COLON,        // !: - no search path prefix
```

### Utility Functions

#### pathTypeToString()

```cpp
// Source: src/parser/schema_path_v3.cpp:27
const char* pathTypeToString(PathType type) {
    switch (type) {
        case PathType::UNQUALIFIED: return "UNQUALIFIED";
        case PathType::CURRENT:     return "CURRENT";
        case PathType::PARENT:      return "PARENT";
        case PathType::ABSOLUTE:    return "ABSOLUTE";
    }
    return "UNKNOWN";
}
```

#### schemaPathToString()

```cpp
// Source: src/parser/schema_path_v3.cpp:37
std::string schemaPathToString(const SchemaPath& path, const StringPool& pool) {
    std::ostringstream ss;
    
    if (path.no_search_path) {
        ss << "!:";
    }
    
    // Add prefix based on type
    switch (path.type) {
        case PathType::CURRENT:  ss << "."; break;
        case PathType::PARENT:   ss << ".."; break;
        default: break;
    }
    
    // Add components separated by dots
    bool first = true;
    for (auto id : path.components) {
        if (!first) ss << ".";
        first = false;
        ss << pool.get(id);
    }
    
    return ss.str();
}
```

#### canStartSchemaPath()

```cpp
// Source: src/parser/schema_path_v3.cpp:71
bool canStartSchemaPath(const ParserState& state) {
    TokenType type = state.current().type;
    return type == TokenType::EXCLAIM_COLON ||
           type == TokenType::DOT ||
           type == TokenType::DOUBLE_DOT ||
           type == TokenType::IDENTIFIER;
}
```

## Invariants

1. **Component Non-Empty Invariant**: Parsed paths always have at least one component
   - Verification: Parser checks for identifier after prefixes before returning

2. **Last Component Invariant**: For ColumnRef, the last path component is always the column name
   - Verification: parseColumnRef extracts components[-1] as column_name

3. **No Search Path Flag Invariant**: no_search_path only applies to UNQUALIFIED type
   - Verification: !: prefix can be combined with any path type, but primarily affects unqualified resolution

4. **Span Coverage Invariant**: SourceSpan covers exactly the text consumed for the path
   - Verification: Span start recorded before parsing, end computed after

## Error Handling

| Error Condition | Behavior | Recovery |
|-----------------|----------|----------|
| Empty path after DOT | Return empty SchemaPath | Caller reports error |
| Empty path after DOUBLE_DOT | Return empty SchemaPath | Caller reports error |
| Missing identifier after DOT in absolute path | Break loop, return partial | Partial path resolution |
| Unexpected token in path | Stop parsing | Return path parsed so far |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_parser_state_v3.cpp` | Parser state and path parsing |
| `tests/v3/parser/test_ambiguity_resolution.md` | Path ambiguity handling |

## Related Specifications

- [V3 Canonical Grammar](./v3_canonical_grammar.md) - Statement-level grammar
- [Semantic Binding Flow](./semantic_binding_flow.md) - Path to UUID resolution

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| Schema Path | Hierarchical reference to a database object |
| Path Type | Classification: UNQUALIFIED, CURRENT, PARENT, ABSOLUTE |
| Search Path | Ordered list of schemas for unqualified name resolution |
| No Search Prefix (!:) | Disables search path, forces current schema |

### References

- ScratchBird PARSER_V3_IMPLEMENTATION_PLAN.md Section 6
- PostgreSQL Schema Documentation (for comparison)

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | Source Analysis |
