# SQL Parser Design Synthesis - Alpha 1.05

## Overview

This document synthesizes the design decisions from three sources:
1. My initial design decisions and outstanding questions
2. The Design Decisions Report (more recent, concise)
3. Claude's Design Proposal (comprehensive analysis)

## Consensus Decisions

All three documents agree on these fundamental decisions:

### 1. Parser Implementation: Hand-Written
- **Unanimous**: Hand-written recursive descent parser
- **Rationale**: Control, no dependencies, better error messages
- **Design Decisions Report**: "Option A + identifier interning"
- **Claude Proposal**: "Hand-written state machine lexer (RECOMMENDED)"

### 2. Bytecode Format: SBLR
- **Unanimous**: Use SBLR (ScratchBird Language Representation)
- **Based on**: Existing 2000+ line specification
- **Design Decisions Report**: "BLR-compatible module"
- **Claude Proposal**: "Segmented format with extensible sections"

### 3. SQL Dialect: Simplified Standard SQL
- **Unanimous**: Basic SQL subset for Alpha
- **Features**: CREATE TABLE, INSERT (single row), SELECT (no joins)
- **Context-aware**: Minimal reserved words (10 vs typical 200)

### 4. Architecture: API-First
- **Unanimous**: Defer network protocol
- **Focus**: Direct API execution for Alpha 1.05
- **Future**: Clean interface for protocol wrapper

## Resolved Design Decisions

Based on the newer documents, these decisions are now clear:

### 1. Lexer Implementation Details
**Decision**: Hand-written DFA scanner
```cpp
struct Token {
    TokenType type;
    std::string_view lexeme;  // Points into source
    uint32_t line, column, offset;
    union {
        int64_t int_value;
        double float_value;
        StringId string_id;    // For interned strings
    } value;
};
```
- **String interning**: Identifiers only
- **Numbers**: int64/double for Alpha
- **Comments**: Support `--` and `/* */`

### 2. AST Design
**Decision**: Tagged union with arena allocator
```cpp
// From Design Decisions Report
enum ASTKind { CREATE_TABLE, INSERT, SELECT, ... };
struct ASTNode {
    ASTKind kind;
    SourceSpan span;
    union {
        CreateTableData create_table;
        InsertData insert;
        SelectData select;
    } data;
};
```
- **Memory**: Arena allocator per parse
- **Traversal**: Switch-based visitor
- **Metadata**: Source spans, type info

### 3. SBLR Module Layout
**Decision**: Minimal BLR-compatible for Alpha
```cpp
struct SBLRModule {
    // Header
    uint32_t magic;      // 'SBLR'
    uint16_t version;    // 0x0100
    
    // Sections (minimal for Alpha)
    uint32_t code_offset;
    uint32_t code_size;
    uint32_t constants_offset;
    uint32_t constants_size;
    uint32_t relations_offset;
    uint32_t relations_size;
    
    // Optional debug
    uint32_t debug_offset;
    uint32_t debug_size;
};
```

### 4. Execution Context
**Decision**: Simple per-statement context
```cpp
struct ExecutionContext {
    Database* db;
    StorageEngine* se;
    TransactionManager* tm;
    uint64_t current_xid;
    ErrorContext* error_ctx;
};

struct ResultSet {
    std::vector<ColumnDef> columns;
    std::vector<Row> rows;  // Row = vector<Value>
};
```

### 5. Schema Validation
**Decision**: Parse-time UUID binding with generation check
- Bind names to UUIDs during parsing
- Store catalog generation
- Verify generation at execution
- Fail-fast on schema changes

### 6. Type System
**Decision**: Map to existing DataType enum
- INTEGER → int32_t
- BIGINT → int64_t  
- DOUBLE → double
- VARCHAR(n) → variable string
- NULL handling: three-valued logic
- Coercion: Only int→double automatic

### 7. Testing
**Decision**: GoogleTest with golden tests
```
tests/sql/alpha/
    create_table.sql
    create_table.expected
    insert_basic.sql
    insert_basic.expected
    select_all.sql
    select_all.expected
```

### 8. Build Organization
**Decision**: Modular static libraries
```
src/parser/     → libscratchbird_parser.a
src/sblr/       → libscratchbird_sblr.a  
src/executor/   → libscratchbird_executor.a
```

## Outstanding Clarifications

From the Design Decisions Report's "Clarifications Requested":

### 1. Decimal Exactness
**Question**: Literal-text preservation vs int128+scale?
**Recommendation**: Defer to Beta phase, use double for Alpha

### 2. External Dependencies
**Question**: Header-only libs acceptable?
**Recommendation**: No external dependencies for Alpha core

### 3. Error Handling
**Question**: Status+ErrorContext vs exceptions?
**Recommendation**: Continue with Status+ErrorContext (consistent with codebase)

### 4. Spec Updates
**Need to add**:
- `PAGE_TYPE_CATALOG_ROOT` to on-disk spec
- TIP root field in DatabaseHeader

## Implementation Priority

1. **Immediate**: Update specs with missing page types
2. **Week 1**: Lexer with token stream
3. **Week 2**: Parser producing AST
4. **Week 3**: Semantic analysis and binding
5. **Week 4**: SBLR code generation
6. **Week 5**: Executor and integration

## Key Insights from Synthesis

1. **Strong Consensus**: All three analyses independently arrived at similar conclusions
2. **Pragmatic Choices**: Focus on simplicity for Alpha, extensibility for future
3. **Clear Path**: With these decisions, implementation can proceed without blockers
4. **Technical Debt**: Minimal - decisions support future growth

## Next Steps

1. Update on-disk specifications per clarifications
2. Create header files based on these decisions
3. Begin lexer implementation
4. Set up test infrastructure

The design is now sufficiently detailed for implementation to begin.