# Alpha 1.05 - SQL Parser Design Decisions

## Executive Summary

This document outlines the key design decisions for implementing the SQL Parser in Alpha 1.05. Based on the requirements and existing architecture, we will implement a hand-written recursive descent parser that generates SBLR (ScratchBird Language Representation) bytecode.

## Design Decisions

### 1. Parser Implementation Approach

**Decision: Hand-written Recursive Descent Parser**

**Rationale:**
- Full control over error messages and recovery
- Easier to implement context-aware features later
- No external tool dependencies
- Better integration with C++ codebase
- Proven approach (PostgreSQL, SQLite use hand-written parsers)

**Alternatives Considered:**
- ANTLR: Too heavyweight, Java dependency
- Bison/Yacc: Limited error recovery, harder to maintain
- Flex/Lex: Would still need parser generator

### 2. Bytecode Format

**Decision: Use SBLR (ScratchBird Language Representation)**

**Details:**
- Based on FirebirdSQL's BLR with modern extensions
- Comprehensive specification already exists in `/docs/archive/scratchbird-bytecode-complete-specification.md`
- Stack-based bytecode format
- Compact binary representation
- Extensible for future optimization

**Initial Opcodes for Alpha 1.05:**
```cpp
// Core control
SBLR_NOP            = 0x00
SBLR_VERSION        = 0x01
SBLR_BEGIN          = 0x02
SBLR_END            = 0xFF
SBLR_EOC            = 0x4C

// Data types
SBLR_SHORT          = 0x07  // 16-bit integer
SBLR_LONG           = 0x08  // 32-bit integer
SBLR_INT64          = 0x10  // 64-bit integer
SBLR_DOUBLE         = 0x1B  // Double precision
SBLR_TEXT           = 0x0E  // Fixed text
SBLR_VARYING        = 0x25  // Varying string

// Database operations
SBLR_STORE          = 0x46  // INSERT
SBLR_FETCH          = 0x4C  // SELECT
SBLR_FOR_SELECT     = 0x4D  // SELECT loop

// Expressions
SBLR_LITERAL        = 0x39  // Literal value
SBLR_FIELD          = 0x38  // Field reference
SBLR_PARAMETER      = 0x37  // Parameter reference
```

### 3. SQL Dialect

**Decision: ScratchBird Native SQL (Simplified Standard SQL)**

**Features for Alpha 1.05:**
- Case-insensitive keywords
- Standard identifier rules (alphanumeric + underscore)
- Double-quoted identifiers for special cases
- Single-quoted strings
- Basic numeric literals
- NULL keyword

**Grammar Subset:**
```ebnf
statement ::= create_table_stmt | insert_stmt | select_stmt

create_table_stmt ::= CREATE TABLE identifier '(' column_def_list ')'
column_def_list ::= column_def (',' column_def)*
column_def ::= identifier data_type [NULL | NOT NULL]
data_type ::= INTEGER | BIGINT | DOUBLE | VARCHAR '(' integer ')'

insert_stmt ::= INSERT INTO identifier '(' column_list ')' VALUES '(' value_list ')'
column_list ::= identifier (',' identifier)*
value_list ::= value (',' value)*
value ::= literal | NULL

select_stmt ::= SELECT select_list FROM identifier [WHERE condition]
select_list ::= '*' | column_list
condition ::= simple_condition
simple_condition ::= identifier comparison_op value
comparison_op ::= '=' | '<>' | '<' | '>' | '<=' | '>='
```

### 4. Error Handling

**Decision: Structured Error Reporting with Recovery**

**Error Structure:**
```cpp
struct ParseError {
    uint32_t line;
    uint32_t column;
    uint32_t position;
    ErrorCode code;
    std::string message;
    std::string hint;
    std::string context;  // Line of SQL with error marked
};
```

**Error Recovery Strategy:**
- Synchronize on statement boundaries (semicolon)
- Skip to next keyword on syntax error
- Collect multiple errors before failing
- Provide helpful hints for common mistakes

### 5. Architecture Integration

**Decision: Direct Engine API Integration**

**Components:**
```cpp
// Parser entry point
class SQLParser {
    ParseResult parse(const std::string& sql);
    SBLRModule* compile(const ParseTree& tree);
};

// AST nodes
class ASTNode {
    virtual void accept(ASTVisitor* visitor) = 0;
};

// SBLR code generator
class SBLRGenerator : public ASTVisitor {
    void visit(CreateTableNode* node);
    void visit(InsertNode* node);
    void visit(SelectNode* node);
};

// Execution interface
class SQLExecutor {
    Status execute(const SBLRModule* module, ExecutionContext* ctx);
};
```

### 6. Memory Management

**Decision: Arena Allocation for Parse Trees**

**Approach:**
- Arena allocator for AST nodes
- Single deallocation after compilation
- SBLR modules use standard allocation
- String interning for identifiers

### 7. Testing Strategy

**Decision: Comprehensive Test Suite**

**Test Categories:**
1. **Lexer Tests**: Token recognition
2. **Parser Tests**: AST generation
3. **Semantic Tests**: Type checking, name resolution
4. **Codegen Tests**: SBLR generation
5. **Integration Tests**: End-to-end SQL execution
6. **Error Tests**: Error reporting and recovery
7. **Fuzzing**: Random SQL generation

## Implementation Phases

### Phase 1: Lexer and Token Stream (Week 1)
- Token definitions
- Lexical analyzer
- Token stream with lookahead
- Basic error reporting

### Phase 2: Parser and AST (Week 2)
- AST node definitions
- Recursive descent parser
- CREATE TABLE parsing
- INSERT parsing
- SELECT parsing

### Phase 3: Semantic Analysis (Week 3)
- Symbol table
- Type checking
- Name resolution
- Schema validation

### Phase 4: Code Generation (Week 4)
- SBLR generator
- Bytecode emission
- Constant pool management
- Debug information

### Phase 5: Integration and Testing (Week 5)
- Engine integration
- Execution testing
- Performance optimization
- Documentation

## Open Questions Resolved

1. **BLR format**: Use existing SBLR specification
2. **Parser generator**: Hand-written recursive descent
3. **SQL dialect**: Simplified standard SQL
4. **Error format**: Structured with recovery

## Dependencies

- ErrorContext (existing)
- CatalogManager (for schema validation)
- StorageEngine (for execution)
- TransactionManager (for transaction context)

## Example Usage

```cpp
// Parse SQL
SQLParser parser;
auto result = parser.parse("CREATE TABLE users (id INTEGER NOT NULL, name VARCHAR(50))");

if (!result.success()) {
    for (const auto& error : result.errors()) {
        std::cerr << error.format() << std::endl;
    }
    return Status::InvalidArgument;
}

// Generate bytecode
SBLRGenerator generator;
auto module = generator.generate(result.ast());

// Execute
SQLExecutor executor(database);
ErrorContext ctx;
return executor.execute(module, &ctx);
```

## Success Criteria

1. Parse all required SQL statements
2. Generate valid SBLR bytecode
3. Integrate with existing engine
4. Comprehensive error messages
5. 100% test coverage
6. Performance: < 1ms for typical statements