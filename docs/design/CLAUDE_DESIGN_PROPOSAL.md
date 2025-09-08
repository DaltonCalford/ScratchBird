# ScratchBird Design Decisions Proposal

## Comprehensive Analysis and Recommendations for Outstanding Items

### Executive Summary

Based on my intensive review of the ScratchBird project, I've analyzed:

- **Implemented**: Basic database creation/opening, page management, buffer pool, storage engine, transaction foundation, and catalog management
- **Specifications**: Comprehensive SBLR bytecode (2000+ lines), detailed on-disk format, Y-Valve architecture, and network layer design
- **Architecture**: Universal database platform with multi-protocol support via Y-Valve, MGA-based transactions, and BLR/SBLR as intermediate representation

This document provides detailed proposals for the 9 outstanding design decisions with pro/con analyses.

---

## 1. Lexer Implementation

### Context

The lexer needs to tokenize SQL input for the parser, supporting context-aware parsing with minimal reserved words (only ~10 vs typical ~200).

### Proposal 1A: Hand-Written State Machine Lexer (RECOMMENDED)

```cpp
class Lexer {
    enum State { INITIAL, IN_IDENTIFIER, IN_NUMBER, IN_STRING, IN_COMMENT };
    State current_state;
    StringPool string_pool;  // String interning

    Token nextToken() {
        // Direct state machine implementation
        switch (current_state) {
            case INITIAL: return scanInitial();
            case IN_IDENTIFIER: return scanIdentifier();
            // ...
        }
    }
};
```

**Pros:**

- Maximum control over tokenization
- Easy context-aware keyword detection
- Optimal performance (no regex overhead)
- Natural integration with string interning
- Simpler debugging

**Cons:**

- More code to write initially
- Must handle all edge cases manually
- Maintenance requires understanding state machine

### Proposal 1B: Table-Driven Lexer Generator

Use a tool like re2c or custom table generator.

**Pros:**

- Declarative specification
- Automated state machine generation
- Proven correctness

**Cons:**

- Build dependency
- Less flexibility for context-aware parsing
- Harder to debug generated code
- May need post-processing for context

### Token Structure Recommendation

```cpp
struct Token {
    TokenType type;
    uint32_t line;
    uint32_t column;
    union {
        StringId string_id;     // For identifiers/strings (interned)
        NumericValue numeric;   // For numbers
        KeywordId keyword_id;   // For keywords
    } value;
    uint32_t length;           // Original text length
    uint32_t offset;           // Offset in source
};
```

### String Interning Strategy

```cpp
class StringPool {
    std::unordered_map<std::string_view, StringId> lookup;
    std::vector<std::unique_ptr<char[]>> buffers;

    StringId intern(std::string_view str) {
        if (auto it = lookup.find(str); it != lookup.end()) {
            return it->second;
        }
        // Allocate in current buffer or create new one
        return allocateAndIntern(str);
    }
};
```

### Number Precision Handling

```cpp
struct NumericValue {
    enum Type { INT64, UINT64, INT128, DECIMAL, FLOAT, DOUBLE };
    Type type;
    union {
        int64_t i64;
        uint64_t u64;
        __int128 i128;
        decimal128_t decimal;  // Custom decimal type
        double f64;
    } value;
};
```

---

## 2. AST Node Design

### Context

The AST represents parsed SQL statements before compilation to SBLR bytecode.

### Proposal 2A: Visitor Pattern with Shared Pointers (RECOMMENDED)

```cpp
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& visitor) = 0;

    // Common metadata
    SourceLocation location;
    std::vector<std::string> comments;
};

class SelectStmt : public ASTNode {
    std::vector<std::shared_ptr<ASTNode>> select_list;
    std::shared_ptr<ASTNode> from_clause;
    std::shared_ptr<ASTNode> where_clause;

    void accept(ASTVisitor& visitor) override {
        visitor.visit(*this);
    }
};
```

**Pros:**

- Clean separation of algorithms from data
- Easy to add new operations
- Memory safety with shared_ptr
- Natural for tree transformations

**Cons:**

- Virtual dispatch overhead
- More boilerplate code
- Potential shared_ptr overhead

### Proposal 2B: Tagged Union Approach

```cpp
struct ASTNode {
    enum Type { SELECT, INSERT, EXPR_BINARY, ... };
    Type type;
    SourceLocation location;

    union {
        SelectData* select;
        InsertData* insert;
        BinaryExprData* binary_expr;
        // ...
    } data;
};
```

**Pros:**

- Cache-friendly layout
- No virtual dispatch
- Simpler memory model

**Cons:**

- Less extensible
- Type safety concerns
- Harder to maintain

### Memory Ownership Model

Use arena allocation for AST nodes:

```cpp
class ASTArena {
    std::vector<std::unique_ptr<uint8_t[]>> blocks;
    size_t current_offset;

    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        // Allocate from current block
        void* ptr = allocateRaw(sizeof(T), alignof(T));
        return new(ptr) T(std::forward<Args>(args)...);
    }
};
```

### Metadata Storage

```cpp
struct ASTMetadata {
    SourceLocation location;
    std::vector<Comment> leading_comments;
    std::vector<Comment> trailing_comments;
    std::optional<TypeInfo> resolved_type;  // After semantic analysis
    std::optional<UUID> resolved_object_id; // For table/column references
};
```

---

## 3. SBLR Module Layout

### Context

SBLR modules contain compiled bytecode with metadata, similar to Java class files or .NET assemblies.

### Proposal 3A: Segmented Format (RECOMMENDED)

```cpp
struct SBLRModule {
    // Header (64 bytes)
    struct Header {
        uint32_t magic;         // 'SBLR'
        uint16_t version_major;
        uint16_t version_minor;
        uint32_t flags;
        uint32_t checksum;
        uint64_t timestamp;
        UUID module_uuid;
        uint32_t total_size;
        uint32_t section_count;
    } header;

    // Section directory
    struct SectionEntry {
        uint32_t type;
        uint32_t offset;
        uint32_t size;
        uint32_t flags;
    } sections[];

    // Sections:
    // - Constants pool
    // - Code section
    // - Debug info
    // - Type info
    // - Symbol table
    // - Source map
};
```

**Pros:**

- Extensible format
- Easy to add new sections
- Supports streaming/partial loading
- Good for mmap usage

**Cons:**

- More complex parsing
- Potential for fragmentation

### Proposal 3B: Linear Format

All data in predefined order without section table.

**Pros:**

- Simpler to parse
- Guaranteed layout

**Cons:**

- Not extensible
- Must read entire module
- Version compatibility issues

### Constant Pool Format

```cpp
struct ConstantPool {
    uint32_t count;
    struct Entry {
        uint8_t tag;  // STRING, INT, FLOAT, UUID, etc.
        uint32_t size;
        uint8_t data[];  // Variable length
    } entries[];
};
```

### Debug Information

```cpp
struct DebugInfo {
    // Source mapping
    struct LineMapping {
        uint32_t bytecode_offset;
        uint32_t source_line;
        uint32_t source_column;
    } line_table[];

    // Variable info
    struct Variable {
        StringId name;
        TypeId type;
        uint32_t scope_start;
        uint32_t scope_end;
    } variables[];
};
```

---

## 4. Execution Context

### Context

The execution context manages state during query execution, interfacing with storage and transaction systems.

### Proposal 4A: Layered Context Design (RECOMMENDED)

```cpp
class ExecutionContext {
    // Core components
    TransactionContext* transaction;
    StorageEngine* storage;
    BufferPool* buffers;

    // Execution state
    MemoryContext* memory;
    ResultBuffer* results;
    ErrorContext* errors;

    // Statistics
    ExecutionStats stats;

public:
    // Storage interface
    HeapTuple fetchTuple(UUID table_id, TupleId tuple_id);
    void insertTuple(UUID table_id, HeapTuple tuple);

    // Transaction interface
    void beginSubTransaction();
    void commitSubTransaction();
    void rollbackSubTransaction();

    // Result handling
    void appendResult(HeapTuple tuple);
    void setError(const Error& error);
};
```

**Pros:**

- Clear separation of concerns
- Easy to mock for testing
- Natural transaction boundaries
- Good for parallel execution

**Cons:**

- More indirection
- Potential overhead for simple queries

### Proposal 4B: Monolithic Context

Single context object with all functionality.

**Pros:**

- Less indirection
- Simpler for basic queries

**Cons:**

- Hard to test
- Poor separation of concerns
- Difficult parallelization

### Storage Engine Interface

```cpp
class StorageInterface {
public:
    virtual TableHandle openTable(UUID table_id) = 0;
    virtual IndexHandle openIndex(UUID index_id) = 0;
    virtual HeapTuple fetchTuple(TableHandle table, TupleId tid) = 0;
    virtual TupleId insertTuple(TableHandle table, HeapTuple tuple) = 0;
    virtual void updateTuple(TableHandle table, TupleId tid, HeapTuple tuple) = 0;
    virtual void deleteTuple(TableHandle table, TupleId tid) = 0;
    virtual TableScanHandle beginScan(TableHandle table, ScanOptions opts) = 0;
};
```

### Result Buffering Strategy

```cpp
class ResultBuffer {
    static constexpr size_t BATCH_SIZE = 1000;

    std::vector<HeapTuple> current_batch;
    std::function<void(std::vector<HeapTuple>&&)> flush_callback;

    void append(HeapTuple tuple) {
        current_batch.push_back(std::move(tuple));
        if (current_batch.size() >= BATCH_SIZE) {
            flush();
        }
    }
};
```

---

## 5. Schema Validation

### Context

Schema validation ensures queries reference valid objects and have correct types.

### Proposal 5A: Two-Phase Validation (RECOMMENDED)

```cpp
class SchemaValidator {
    // Phase 1: Parse-time validation (syntax only)
    ParseResult validateSyntax(const ASTNode* ast) {
        // Check syntax correctness
        // Don't resolve names yet
    }

    // Phase 2: Execution-time validation
    ValidationResult validateSemantics(const ASTNode* ast, CatalogSnapshot* catalog) {
        // Resolve table/column names to UUIDs
        // Check permissions
        // Verify types
    }
};
```

**Pros:**

- Can parse without catalog lock
- Better error messages
- Supports prepared statements
- Natural for multi-phase compilation

**Cons:**

- Two-pass overhead
- More complex implementation

### Proposal 5B: Single-Phase Validation

Validate everything during parsing.

**Pros:**

- Simpler implementation
- Fail fast

**Cons:**

- Requires catalog lock during parse
- Can't prepare statements offline
- Poor for caching

### Catalog Locking Strategy

```cpp
class CatalogSnapshot {
    uint64_t snapshot_version;
    std::shared_ptr<const CatalogData> data;

    // Optimistic validation
    bool isStillValid() const {
        return catalog->getCurrentVersion() == snapshot_version;
    }
};
```

### Version Tracking

```cpp
struct SchemaVersion {
    UUID object_id;
    uint64_t version_number;
    uint64_t transaction_id;
    Timestamp modified_time;
    UUID modified_by;
};
```

---

## 6. Type System

### Context

Map SQL types to internal representations with proper coercion rules.

### Proposal 6A: Rich Type System with Traits (RECOMMENDED)

```cpp
class Type {
public:
    virtual TypeId id() const = 0;
    virtual size_t size() const = 0;
    virtual bool isFixedSize() const = 0;
    virtual bool isNumeric() const = 0;
    virtual bool isComparable() const = 0;
    virtual bool canCoerceTo(const Type* other) const = 0;
};

class TypeRegistry {
    std::unordered_map<TypeId, std::unique_ptr<Type>> types;
    std::unordered_map<std::pair<TypeId, TypeId>, CoercionPath> coercions;

    CoercionResult coerce(Value& value, TypeId from, TypeId to) {
        if (auto path = findCoercionPath(from, to)) {
            return path->apply(value);
        }
        return CoercionResult::Impossible;
    }
};
```

**Pros:**

- Extensible type system
- Clear coercion rules
- Supports user-defined types
- Good for optimization

**Cons:**

- More complex than fixed types
- Virtual dispatch overhead

### Proposal 6B: Fixed Type Enumeration

Simple enum with switch statements.

**Pros:**

- Simple and fast
- No virtual dispatch

**Cons:**

- Not extensible
- Hard to add new types
- Coercion logic scattered

### SQL to Internal Type Mapping

```cpp
// Core type mappings
TypeId mapSQLType(const std::string& sql_type) {
    static const std::unordered_map<std::string, TypeId> mappings = {
        {"INTEGER", TypeId::Int32},
        {"BIGINT", TypeId::Int64},
        {"DECIMAL", TypeId::Decimal128},
        {"VARCHAR", TypeId::VarChar},
        {"TEXT", TypeId::Text},
        {"TIMESTAMP", TypeId::TimestampTz},
        {"UUID", TypeId::Uuid},
        // ...
    };
    return mappings.at(sql_type);
}
```

### NULL Handling Strategy

```cpp
struct Value {
    bool is_null;
    union {
        int64_t i64;
        double f64;
        StringId str;
        // ...
    } data;

    static Value null() { return {true, {}}; }
};

// Three-valued logic for NULL
enum class TriBool { True, False, Unknown };
TriBool compareWithNull(const Value& a, const Value& b);
```

---

## 7. Testing Infrastructure

### Context

Need comprehensive testing at multiple levels: unit, integration, conformance.

### Proposal 7A: Multi-Framework Approach (RECOMMENDED)

```cpp
// Unit tests: GoogleTest
TEST_F(LexerTest, TokenizesSimpleSelect) {
    Lexer lexer("SELECT * FROM users");
    EXPECT_EQ(lexer.nextToken().type, TokenType::SELECT);
    EXPECT_EQ(lexer.nextToken().type, TokenType::STAR);
    // ...
}

// Integration tests: Custom framework
class SQLTest : public TestBase {
    void run() {
        execute("CREATE TABLE test (id INTEGER)");
        execute("INSERT INTO test VALUES (1), (2), (3)");
        auto result = query("SELECT COUNT(*) FROM test");
        ASSERT_EQ(result[0][0].asInt(), 3);
    }
};

// Conformance tests: SQL standard tests
class ConformanceTest {
    void runSuite(const std::string& suite_path) {
        for (auto& test : loadTests(suite_path)) {
            runConformanceTest(test);
        }
    }
};
```

**Pros:**

- Best tool for each job
- Comprehensive coverage
- Industry standard tools
- Good CI integration

**Cons:**

- Multiple frameworks to learn
- More complex build

### Proposal 7B: Single Framework

Use only GoogleTest for everything.

**Pros:**

- Simpler setup
- One framework to learn

**Cons:**

- Not ideal for all test types
- May need custom extensions

### SQL Conformance Suite

```yaml
# Test specification format
test_name: "Basic SELECT with WHERE"
sql: |
  CREATE TABLE users (id INTEGER, name VARCHAR(50));
  INSERT INTO users VALUES (1, 'Alice'), (2, 'Bob');
  SELECT name FROM users WHERE id = 2;
expected:
  - ["Bob"]
conformance:
  - SQL-92
  - SQL:1999
```

### Performance Benchmarks

```cpp
// Micro-benchmarks with Google Benchmark
BENCHMARK(BM_ParseSimpleSelect)->Range(1, 1000);
BENCHMARK(BM_ExecuteIndexScan)->RangeMultiplier(10)->Range(10, 10000);

// Macro-benchmarks
class TPCHBenchmark : public Benchmark {
    void setup() override {
        loadTPCHData(scale_factor);
    }

    void run() override {
        for (int i = 1; i <= 22; i++) {
            auto query = loadTPCHQuery(i);
            timer.start();
            execute(query);
            timer.stop();
            recordResult(i, timer.elapsed());
        }
    }
};
```

---

## 8. Build System

### Context

Project uses CMake, need to organize for growth while maintaining standards.

### Proposal 8A: Modular CMake Structure (RECOMMENDED)

```
scratchbird/
├── CMakeLists.txt          # Root CMake
├── cmake/                  # CMake modules
│   ├── CompilerOptions.cmake
│   ├── Dependencies.cmake
│   └── Testing.cmake
├── src/
│   ├── CMakeLists.txt      # Src CMake
│   ├── core/              # Core engine
│   │   ├── CMakeLists.txt
│   │   └── ...
│   ├── parser/            # SQL parser
│   │   ├── CMakeLists.txt
│   │   └── ...
│   └── ...
├── include/scratchbird/    # Public headers
│   ├── core/
│   ├── parser/
│   └── ...
├── tests/
│   ├── CMakeLists.txt
│   └── ...
└── third_party/           # Dependencies
    ├── CMakeLists.txt
    └── ...
```

**Pros:**

- Clear module boundaries
- Parallel builds
- Easy to add components
- Good for large projects

**Cons:**

- More CMake files
- Need to understand structure

### Proposal 8B: Flat Structure

Single CMakeLists.txt with everything.

**Pros:**

- Simple for small projects
- Everything in one place

**Cons:**

- Doesn't scale
- Slow rebuilds
- Hard to maintain

### Library Organization

```cmake
# Core libraries
add_library(sb_core STATIC
    core/database.cpp
    core/page_manager.cpp
    core/buffer_pool.cpp
)

add_library(sb_parser STATIC
    parser/lexer.cpp
    parser/parser.cpp
    parser/ast.cpp
)

add_library(sb_executor STATIC
    executor/executor.cpp
    executor/operators.cpp
)

# Main executable
add_executable(scratchbird
    main.cpp
)
target_link_libraries(scratchbird
    sb_core
    sb_parser
    sb_executor
)
```

### Header Layout

```cpp
// Public API headers: include/scratchbird/
#include <scratchbird/database.h>
#include <scratchbird/result_set.h>

// Internal headers: src/*/
#include "core/internal/page_impl.h"
#include "parser/internal/lexer_impl.h"
```

---

## 9. Network Protocol (Deferred but Important Context)

### Context

While deferred to later phases, the parser design must accommodate future protocol needs.

### Current Design

- Y-Valve spawns parser processes per connection
- Parser speaks client protocol, translates to BLR
- Engine only sees BLR, not protocols

### Considerations for Parser Design

1. **Protocol State Machine**: Parser must maintain protocol state
2. **Streaming**: Handle partial SQL statements
3. **Prepared Statements**: Cache parsed BLR
4. **Protocol Extensions**: COPY, LISTEN/NOTIFY, etc.

### Recommendation

Design parser with clean interface for future protocol integration:

```cpp
class IProtocolHandler {
    virtual void handleData(const uint8_t* data, size_t len) = 0;
    virtual bool hasCompleteStatement() = 0;
    virtual std::string getStatement() = 0;
    virtual void sendResponse(const Response& resp) = 0;
};
```

---

## Summary of Recommendations

1. **Lexer**: Hand-written state machine with string interning
2. **AST**: Visitor pattern with arena allocation
3. **SBLR Module**: Segmented format with extensible sections
4. **Execution Context**: Layered design with clear interfaces
5. **Schema Validation**: Two-phase validation with snapshots
6. **Type System**: Rich type system with traits
7. **Testing**: Multi-framework with conformance suite
8. **Build System**: Modular CMake structure
9. **Network Protocol**: Clean interface for future integration

These recommendations balance:

- Implementation complexity vs maintainability
- Performance vs flexibility
- Current needs vs future extensibility
- Industry best practices vs innovative features

The design maintains ScratchBird's unique advantages (UUID-based catalog, MGA, multi-protocol) while learning from successful database implementations.
