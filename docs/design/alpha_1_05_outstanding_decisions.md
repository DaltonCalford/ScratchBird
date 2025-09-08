# Outstanding Design Decisions for Alpha 1.05 - SQL Parser

## Review of Current Decisions

After reviewing the design documents, most major decisions have been made. However, there are several areas that need clarification or additional detail before implementation can begin effectively.

## Outstanding Decisions Needed

### 1. Lexer Implementation Details

**Need to Decide:**
- Token representation structure
- String handling (intern all strings or just keywords?)
- Number parsing precision requirements
- Comment handling (SQL standard `--` and `/* */`)
- Maximum identifier length
- Reserved vs non-reserved keywords list

**Proposed Decision:**
```cpp
struct Token {
    TokenType type;
    std::string_view text;
    Location location;
    union {
        int64_t int_value;
        double float_value;
        uint32_t string_id;  // For interned strings
    } value;
};

// Maximum identifier length: 128 characters (PostgreSQL compatible)
// Comments: Support both -- and /* */ styles
// String interning: Keywords and identifiers only
```

### 2. AST Node Hierarchy

**Need to Decide:**
- Visitor pattern vs virtual dispatch for traversal
- Node ownership model (unique_ptr, arena, or hybrid)
- Location tracking granularity
- Metadata storage approach

**Proposed Decision:**
```cpp
// Use visitor pattern for flexibility
class ASTNode {
    Location location;
    virtual void accept(ASTVisitor* visitor) = 0;
    virtual ~ASTNode() = default;
};

// Arena allocation with RAII wrapper
template<typename T>
class ArenaPtr {
    T* ptr;
    Arena* arena;
    // Custom destructor registration
};
```

### 3. SBLR Module Memory Layout

**Need to Decide:**
- Constant pool organization
- String table format
- Bytecode alignment requirements
- Module versioning strategy
- Debug information inclusion

**Proposed Decision:**
```cpp
struct SBLRModule {
    // Header
    uint32_t magic;           // 'SBLR'
    uint16_t version;         // 0x0100
    uint16_t flags;           // Debug, optimized, etc.
    
    // Sections (file offsets)
    uint32_t constant_offset;
    uint32_t constant_size;
    uint32_t string_offset;
    uint32_t string_size;
    uint32_t code_offset;
    uint32_t code_size;
    uint32_t debug_offset;    // Optional
    uint32_t debug_size;
    
    // Data follows...
};
```

### 4. Execution Context Interface

**Need to Decide:**
- How SBLR executor interfaces with storage engine
- Transaction context passing
- Result set buffering strategy
- Error propagation mechanism
- Resource limits (memory, time)

**Proposed Decision:**
```cpp
class ExecutionContext {
    Database* database;
    TransactionManager::Transaction* txn;
    ResultBuffer* result_buffer;
    ErrorContext* error_ctx;
    ResourceLimits limits;
    
    // Statistics
    uint64_t rows_processed;
    uint64_t bytes_read;
    std::chrono::steady_clock::time_point start_time;
};

class SBLRExecutor {
    Status executeModule(SBLRModule* module, ExecutionContext* ctx);
    Status executeInstruction(uint8_t opcode, ExecutionContext* ctx);
};
```

### 5. Schema Validation Integration

**Need to Decide:**
- When to validate (parse time vs execution time)
- Catalog locking strategy during validation
- Schema version tracking
- Permission checking hooks

**Proposed Decision:**
- Two-phase validation:
  1. Syntax validation during parsing
  2. Schema validation during semantic analysis
- Read-lock catalog during validation
- Snapshot schema version at statement start
- Permission checks deferred to Alpha 2.x

### 6. Type System Mapping

**Need to Decide:**
- SQL type to internal type mapping
- Type coercion rules
- NULL handling in expressions
- Default values for columns

**Proposed Decision:**
```cpp
enum class SQLType {
    INTEGER,    // Maps to int32_t
    BIGINT,     // Maps to int64_t
    DOUBLE,     // Maps to double
    VARCHAR,    // Maps to variable string
    // Future: DATE, TIME, TIMESTAMP, etc.
};

struct TypeInfo {
    SQLType sql_type;
    uint32_t precision;    // For VARCHAR length
    bool nullable;
    Value default_value;   // Optional
};
```

### 7. Testing Infrastructure

**Need to Decide:**
- Test data generation approach
- SQL conformance test suite
- Performance benchmarking harness
- Fuzzing integration

**Proposed Decision:**
- Use GoogleTest for unit tests
- Create SQL test case files (input/expected output)
- Benchmark against SQLite for reference
- AFL++ for fuzzing integration

### 8. Build System Integration

**Need to Decide:**
- Header organization
- Library structure (static vs shared)
- Optional components (e.g., debug tools)
- Installation layout

**Proposed Decision:**
```
include/scratchbird/sql/
    parser.h
    lexer.h
    ast.h
    sblr_generator.h
    executor.h

src/sql/
    lexer.cpp
    parser.cpp
    ast.cpp
    sblr_generator.cpp
    executor.cpp
    
tests/sql/
    test_lexer.cpp
    test_parser.cpp
    test_sblr_gen.cpp
    test_executor.cpp
    test_integration.cpp
```

## Items Sufficiently Detailed

These aspects are well-defined in existing documents:

1. ✅ Parser approach (recursive descent)
2. ✅ SBLR bytecode format (comprehensive spec exists)
3. ✅ Basic SQL grammar subset
4. ✅ Error handling structure
5. ✅ Implementation phases
6. ✅ Integration architecture

## Recommended Next Steps

1. **Create detailed header files** with the proposed structures
2. **Write lexer specification** with complete token list
3. **Define AST node classes** for all supported statements
4. **Create SBLR executor interface** specification
5. **Set up test framework** with example SQL files

## Risk Areas

1. **Performance**: Hand-written parser may be slower than generated
   - Mitigation: Profile and optimize hot paths
   
2. **Compatibility**: SBLR format lock-in
   - Mitigation: Version field allows evolution
   
3. **Complexity**: Semantic analysis may grow complex
   - Mitigation: Start simple, iterate

### 9. Network Protocol (Deferred)

**Current Understanding:**
- The plan mentions "ScratchBird native parser & protocol"
- Y-Valve architecture suggests network handling
- But Alpha 1.05 can start with direct API calls

**Proposed Decision:**
- Defer network protocol to Alpha 1.06 or later
- Alpha 1.05 focuses on SQL parsing and execution via API
- Create clean interface that can be wrapped by network layer later
- Document protocol requirements for future implementation

```cpp
// Future protocol interface
class SQLSession {
    virtual Status executeSQL(const std::string& sql, ResultSet& result) = 0;
};

// Alpha 1.05 implementation
class LocalSQLSession : public SQLSession {
    Database* db_;
    // Direct execution without network
};
```

## Conclusion

While the high-level design is solid, these implementation details should be decided before coding begins. Most decisions are straightforward and follow established patterns from other databases. The proposed decisions above provide a concrete starting point that can be refined during implementation.

**Key Insight**: The network protocol aspect can be deferred since Alpha 1.05 can focus on the core SQL parsing and execution engine, with the network layer added in a subsequent phase.