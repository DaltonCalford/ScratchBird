# SEQUENCE Implementation Plan

**Date**: November 7, 2025
**Status**: Design Phase
**Priority**: HIGH (Foundation for DEFAULT constraints)
**Estimated Effort**: 30-40 hours

---

## Overview

Implement SQL sequences (auto-incrementing values) with full DDL support and MGA compliance.

### Goals

1. **CREATE SEQUENCE** - Define new sequences with configuration
2. **ALTER SEQUENCE** - Modify existing sequences
3. **DROP SEQUENCE** - Remove sequences
4. **NEXTVAL()** - Get next value (atomic increment)
5. **CURRVAL()** - Get current value (session-local)
6. **SETVAL()** - Manually set sequence value

### SQL Syntax

```sql
-- CREATE
CREATE SEQUENCE sequence_name
    [INCREMENT BY increment]
    [MINVALUE minvalue | NO MINVALUE]
    [MAXVALUE maxvalue | NO MAXVALUE]
    [START WITH start]
    [CACHE cache]
    [CYCLE | NO CYCLE];

-- ALTER
ALTER SEQUENCE sequence_name
    [INCREMENT BY increment]
    [MINVALUE minvalue | NO MINVALUE]
    [MAXVALUE maxvalue | NO MAXVALUE]
    [RESTART [WITH restart]]
    [CACHE cache]
    [CYCLE | NO CYCLE];

-- DROP
DROP SEQUENCE [IF EXISTS] sequence_name [CASCADE | RESTRICT];

-- Functions
NEXTVAL('sequence_name')  -- Returns next value
CURRVAL('sequence_name')  -- Returns current session value
SETVAL('sequence_name', value [, is_called])
```

---

## Architecture Design

### Catalog Schema

**sys_sequences** table (new):
```cpp
struct SequenceInfo {
    ID sequence_id;              // UUID (16 bytes)
    ID schema_id;                // Schema UUID (16 bytes)
    char name[512];              // Sequence name (UTF-8)
    int64_t current_value;       // Current value (atomic)
    int64_t increment_by;        // Increment (default: 1)
    int64_t min_value;           // Minimum value
    int64_t max_value;           // Maximum value
    int64_t start_value;         // Initial value
    int64_t cache_size;          // Cache size (default: 1)
    bool cycle;                  // Wrap at min/max? (default: false)
    uint64_t created_time;       // Creation timestamp
    uint64_t last_modified_time; // Last modification timestamp
};
```

### Thread Safety & MGA Compliance

**Challenge**: Sequences must be:
1. **Atomic** - No duplicate values across concurrent transactions
2. **Non-blocking** - Don't wait for transaction commit
3. **Gap-tolerant** - Rolled-back transactions leave gaps (by design)

**Solution**: Use `std::atomic<int64_t>` for current_value

```cpp
class SequenceManager {
private:
    struct SequenceState {
        ID sequence_id;
        std::atomic<int64_t> current_value;  // Atomic increment
        int64_t increment_by;
        int64_t min_value;
        int64_t max_value;
        bool cycle;
        std::mutex config_mutex;             // Protect config changes
    };

    std::unordered_map<ID, std::shared_ptr<SequenceState>> sequences_;
    std::mutex sequences_mutex_;  // Protect map access
};
```

**MGA Compliance**:
- Sequence values are **NOT transactional** (by design, matches PostgreSQL/Firebird)
- `NEXTVAL()` consumes a value immediately (no rollback)
- This prevents blocking and ensures uniqueness
- Gaps are acceptable (rolled-back transactions waste values)

### Session-Local CURRVAL

**Challenge**: `CURRVAL()` must return the last value this session got via `NEXTVAL()`

**Solution**: Session-local cache:
```cpp
// In Executor or Session context
std::unordered_map<ID, int64_t> session_sequence_values_;  // sequence_id -> last_value
```

**Rules**:
- `CURRVAL()` without prior `NEXTVAL()` in session → ERROR
- Each session tracks its own last values
- Not affected by other sessions' `NEXTVAL()` calls

---

## Implementation Steps

### Step 1: Tokens & Keywords (30 minutes)

**File**: `include/scratchbird/parser/token.h`
```cpp
KW_SEQUENCE,    // SEQUENCE
KW_INCREMENT,   // INCREMENT
KW_MINVALUE,    // MINVALUE
KW_MAXVALUE,    // MAXVALUE
KW_START,       // START
KW_CACHE,       // CACHE
KW_CYCLE,       // CYCLE
KW_RESTART,     // RESTART
KW_CURRVAL,     // CURRVAL (function)
KW_NEXTVAL,     // NEXTVAL (function)
KW_SETVAL,      // SETVAL (function)
```

**File**: `src/parser/lexer.cpp`
```cpp
{"SEQUENCE", TokenType::KW_SEQUENCE},
{"INCREMENT", TokenType::KW_INCREMENT},
{"MINVALUE", TokenType::KW_MINVALUE},
// ... etc
```

### Step 2: AST Nodes (2-3 hours)

**File**: `include/scratchbird/parser/ast.h`

```cpp
// AST kinds
CREATE_SEQUENCE,
ALTER_SEQUENCE,
DROP_SEQUENCE,

// CreateSequenceStmt
class CreateSequenceStmt : public Statement {
public:
    CreateSequenceStmt(const SourceSpan& span, StringPool::StringId name)
        : Statement(ASTKind::CREATE_SEQUENCE, span), name_(name) {}

    StringPool::StringId name() const { return name_; }

    // Optional parameters (nullptr if not specified)
    void setIncrementBy(Expression* expr) { increment_by_ = expr; }
    void setMinValue(Expression* expr) { min_value_ = expr; }
    void setMaxValue(Expression* expr) { max_value_ = expr; }
    void setStartWith(Expression* expr) { start_with_ = expr; }
    void setCache(Expression* expr) { cache_ = expr; }
    void setCycle(bool cycle) { cycle_ = cycle; }
    void setNoMinValue(bool no_min) { no_min_value_ = no_min; }
    void setNoMaxValue(bool no_max) { no_max_value_ = no_max; }

    Expression* incrementBy() const { return increment_by_; }
    Expression* minValue() const { return min_value_; }
    Expression* maxValue() const { return max_value_; }
    Expression* startWith() const { return start_with_; }
    Expression* cache() const { return cache_; }
    bool cycle() const { return cycle_; }
    bool noMinValue() const { return no_min_value_; }
    bool noMaxValue() const { return no_max_value_; }

    void accept(ASTVisitor* visitor) override;

private:
    StringPool::StringId name_;
    Expression* increment_by_ = nullptr;
    Expression* min_value_ = nullptr;
    Expression* max_value_ = nullptr;
    Expression* start_with_ = nullptr;
    Expression* cache_ = nullptr;
    bool cycle_ = false;
    bool no_min_value_ = false;
    bool no_max_value_ = false;
};

// AlterSequenceStmt
class AlterSequenceStmt : public Statement {
public:
    AlterSequenceStmt(const SourceSpan& span, StringPool::StringId name)
        : Statement(ASTKind::ALTER_SEQUENCE, span), name_(name) {}

    StringPool::StringId name() const { return name_; }

    // Same setters as CreateSequenceStmt
    void setIncrementBy(Expression* expr) { increment_by_ = expr; }
    void setMinValue(Expression* expr) { min_value_ = expr; }
    void setMaxValue(Expression* expr) { max_value_ = expr; }
    void setRestart(Expression* expr) { restart_ = expr; }
    void setCache(Expression* expr) { cache_ = expr; }
    void setCycle(bool cycle) { cycle_ = cycle; has_cycle_ = true; }
    void setNoMinValue(bool no_min) { no_min_value_ = no_min; }
    void setNoMaxValue(bool no_max) { no_max_value_ = no_max; }

    Expression* incrementBy() const { return increment_by_; }
    Expression* minValue() const { return min_value_; }
    Expression* maxValue() const { return max_value_; }
    Expression* restart() const { return restart_; }
    Expression* cache() const { return cache_; }
    bool hasCycle() const { return has_cycle_; }
    bool cycle() const { return cycle_; }
    bool noMinValue() const { return no_min_value_; }
    bool noMaxValue() const { return no_max_value_; }

    void accept(ASTVisitor* visitor) override;

private:
    StringPool::StringId name_;
    Expression* increment_by_ = nullptr;
    Expression* min_value_ = nullptr;
    Expression* max_value_ = nullptr;
    Expression* restart_ = nullptr;
    Expression* cache_ = nullptr;
    bool has_cycle_ = false;
    bool cycle_ = false;
    bool no_min_value_ = false;
    bool no_max_value_ = false;
};

// DropSequenceStmt
class DropSequenceStmt : public Statement {
public:
    DropSequenceStmt(const SourceSpan& span, StringPool::StringId name,
                     bool if_exists, bool cascade)
        : Statement(ASTKind::DROP_SEQUENCE, span),
          name_(name), if_exists_(if_exists), cascade_(cascade) {}

    StringPool::StringId name() const { return name_; }
    bool ifExists() const { return if_exists_; }
    bool cascade() const { return cascade_; }

    void accept(ASTVisitor* visitor) override;

private:
    StringPool::StringId name_;
    bool if_exists_;
    bool cascade_;
};

// Sequence functions (NEXTVAL, CURRVAL, SETVAL)
enum class SequenceFunctionType : uint8_t {
    NEXTVAL,
    CURRVAL,
    SETVAL
};

class SequenceFunctionExpr : public Expression {
public:
    SequenceFunctionExpr(const SourceSpan& span, SequenceFunctionType func_type,
                         Expression* sequence_name, Expression* value = nullptr,
                         Expression* is_called = nullptr)
        : Expression(ASTKind::SEQUENCE_FUNCTION, span),
          func_type_(func_type), sequence_name_(sequence_name),
          value_(value), is_called_(is_called) {}

    SequenceFunctionType functionType() const { return func_type_; }
    Expression* sequenceName() const { return sequence_name_; }
    Expression* value() const { return value_; }  // For SETVAL
    Expression* isCalled() const { return is_called_; }  // For SETVAL

    void accept(ASTVisitor* visitor) override;

private:
    SequenceFunctionType func_type_;
    Expression* sequence_name_;
    Expression* value_;       // For SETVAL(seq, value)
    Expression* is_called_;   // For SETVAL(seq, value, is_called)
};

// Add to ASTVisitor
virtual void visit(CreateSequenceStmt* node) = 0;
virtual void visit(AlterSequenceStmt* node) = 0;
virtual void visit(DropSequenceStmt* node) = 0;
virtual void visit(SequenceFunctionExpr* node) = 0;
```

### Step 3: Parser (4-6 hours)

**File**: `include/scratchbird/parser/parser.h`
```cpp
Statement* parseCreateSequence();
Statement* parseAlterSequence();
Statement* parseDropSequence();
Expression* parseSequenceFunction(TokenType func_type);
```

**File**: `src/parser/parser.cpp`

Implement parsers with full syntax support:
- Optional parameters with defaults
- NO MINVALUE / NO MAXVALUE handling
- Expression parsing for numeric values
- Error handling for invalid combinations

### Step 4: Bytecode Opcodes (1-2 hours)

**File**: `include/scratchbird/sblr/opcodes.h`
```cpp
CREATE_SEQUENCE = 0x23,
ALTER_SEQUENCE = 0x24,
DROP_SEQUENCE = 0x25,
SEQUENCE_NEXTVAL = 0x26,
SEQUENCE_CURRVAL = 0x27,
SEQUENCE_SETVAL = 0x28,
```

**File**: `src/sblr/bytecode_generator.cpp`

Generate bytecode for:
- Sequence DDL (name + config parameters)
- Sequence functions (sequence name + optional value)

### Step 5: Catalog Manager (8-12 hours)

**File**: `include/scratchbird/core/catalog_manager.h`

```cpp
// Sequence info structure
struct SequenceInfo {
    ID sequence_id;
    ID schema_id;
    std::string name;
    int64_t current_value;
    int64_t increment_by;
    int64_t min_value;
    int64_t max_value;
    int64_t start_value;
    int64_t cache_size;
    bool cycle;
    uint64_t created_time;
    uint64_t last_modified_time;
};

// In-memory sequence state (for atomic operations)
struct SequenceState {
    ID sequence_id;
    std::atomic<int64_t> current_value;
    int64_t increment_by;
    int64_t min_value;
    int64_t max_value;
    bool cycle;
    std::mutex config_mutex;  // Protect ALTER SEQUENCE changes
};

// Methods
auto createSequence(const ID& schema_id, const std::string& name,
                    int64_t increment_by, int64_t min_value, int64_t max_value,
                    int64_t start_value, int64_t cache_size, bool cycle,
                    ErrorContext* ctx = nullptr) -> Status;

auto alterSequence(const ID& sequence_id, const std::optional<int64_t>& increment_by,
                   const std::optional<int64_t>& min_value, const std::optional<int64_t>& max_value,
                   const std::optional<int64_t>& restart, const std::optional<int64_t>& cache_size,
                   const std::optional<bool>& cycle, ErrorContext* ctx = nullptr) -> Status;

auto dropSequence(const ID& sequence_id, bool cascade, ErrorContext* ctx = nullptr) -> Status;

auto getSequence(const ID& schema_id, const std::string& name,
                 SequenceInfo& info_out, ErrorContext* ctx = nullptr) -> Status;

auto sequenceNextVal(const ID& sequence_id, int64_t& value_out,
                     ErrorContext* ctx = nullptr) -> Status;

auto sequenceSetVal(const ID& sequence_id, int64_t value, bool is_called,
                    ErrorContext* ctx = nullptr) -> Status;

private:
    std::unordered_map<ID, std::shared_ptr<SequenceState>> sequence_cache_;
    std::mutex sequence_cache_mutex_;
```

**File**: `src/core/catalog_manager.cpp`

Implement methods with:
- Atomic increment using `std::atomic<int64_t>::fetch_add()`
- Cycle handling (wrap to min_value when exceeding max_value)
- Thread-safe ALTER SEQUENCE (lock config_mutex)
- Persistent storage in catalog pages
- Cache loading on first access

### Step 6: Executor (4-6 hours)

**File**: `include/scratchbird/sblr/executor.h`
```cpp
void executeCreateSequence();
void executeAlterSequence();
void executeDropSequence();
int64_t executeSequenceNextVal();
int64_t executeSequenceCurrVal();
int64_t executeSequenceSetVal();

// Session state
std::unordered_map<ID, int64_t> session_sequence_currval_;
```

**File**: `src/sblr/executor.cpp`

Implement executors:
- CREATE: Call catalog_manager->createSequence()
- ALTER: Call catalog_manager->alterSequence()
- DROP: Call catalog_manager->dropSequence()
- NEXTVAL: Call catalog_manager->sequenceNextVal(), store in session_sequence_currval_
- CURRVAL: Look up in session_sequence_currval_, error if not found
- SETVAL: Call catalog_manager->sequenceSetVal()

### Step 7: Expression Evaluator (2-3 hours)

**File**: `src/sblr/expression_evaluator.cpp`

Add sequence function evaluation in `evaluate()`:
- Recognize SEQUENCE_NEXTVAL, SEQUENCE_CURRVAL, SEQUENCE_SETVAL opcodes
- Call executor methods
- Return INT64 value

### Step 8: Testing (4-6 hours)

**File**: `test_sequences.sql`

```sql
-- Test CREATE SEQUENCE with defaults
CREATE SEQUENCE seq1;
SELECT NEXTVAL('seq1');  -- 1
SELECT NEXTVAL('seq1');  -- 2
SELECT CURRVAL('seq1');  -- 2

-- Test CREATE SEQUENCE with custom parameters
CREATE SEQUENCE seq2
    INCREMENT BY 5
    MINVALUE 10
    MAXVALUE 50
    START WITH 10
    CYCLE;

SELECT NEXTVAL('seq2');  -- 10
SELECT NEXTVAL('seq2');  -- 15
SELECT NEXTVAL('seq2');  -- 20

-- Test cycle
CREATE SEQUENCE seq3
    INCREMENT BY 1
    MINVALUE 1
    MAXVALUE 3
    START WITH 1
    CYCLE;
SELECT NEXTVAL('seq3');  -- 1
SELECT NEXTVAL('seq3');  -- 2
SELECT NEXTVAL('seq3');  -- 3
SELECT NEXTVAL('seq3');  -- 1 (cycles)

-- Test ALTER SEQUENCE
ALTER SEQUENCE seq1 INCREMENT BY 10;
SELECT NEXTVAL('seq1');  -- 13 (current 3 + increment 10)

ALTER SEQUENCE seq1 RESTART WITH 100;
SELECT NEXTVAL('seq1');  -- 100

-- Test SETVAL
SELECT SETVAL('seq1', 200);
SELECT NEXTVAL('seq1');  -- 210

SELECT SETVAL('seq1', 300, false);  -- Mark as not called
SELECT NEXTVAL('seq1');  -- 300 (not 310)

-- Test DROP SEQUENCE
DROP SEQUENCE seq1;
DROP SEQUENCE IF EXISTS seq1;  -- No error

-- Test CASCADE
CREATE TABLE test_table (
    id INTEGER DEFAULT NEXTVAL('seq2'),
    name VARCHAR(100)
);
DROP SEQUENCE seq2 RESTRICT;  -- ERROR: dependent objects exist
DROP SEQUENCE seq2 CASCADE;   -- Drops sequence and removes DEFAULT
```

---

## Error Handling

1. **Sequence not found**: "Sequence 'name' does not exist"
2. **CURRVAL before NEXTVAL**: "CURRVAL of sequence 'name' is not yet defined in this session"
3. **Sequence exhausted (no CYCLE)**: "Sequence 'name' has reached its maximum value"
4. **Invalid range**: "MINVALUE (10) must be less than MAXVALUE (5)"
5. **Invalid START**: "START value (100) cannot be less than MINVALUE (1)"
6. **Duplicate sequence**: "Sequence 'name' already exists"
7. **CASCADE dependency**: "Cannot drop sequence 'name' because other objects depend on it"

---

## Performance Considerations

1. **Atomic increment**: `std::atomic<int64_t>::fetch_add()` is lock-free on modern CPUs
2. **Cache size**: Future optimization - pre-allocate values in batches
3. **Session CURRVAL**: O(1) lookup in unordered_map
4. **Persistence**: Write current_value to catalog on clean shutdown or periodically

---

## Files to Modify

**Parser** (6 files):
- `include/scratchbird/parser/token.h` - Add tokens
- `src/parser/lexer.cpp` - Add keyword mappings
- `include/scratchbird/parser/ast.h` - Add AST nodes
- `src/parser/ast.cpp` - Add accept() methods
- `include/scratchbird/parser/parser.h` - Add method declarations
- `src/parser/parser.cpp` - Implement parsers

**Semantic Analyzer** (2 files):
- `include/scratchbird/parser/semantic_analyzer.h` - Add visitor declarations
- `src/parser/semantic_analyzer.cpp` - Add visitor implementations (stubs)

**Bytecode** (2 files):
- `include/scratchbird/sblr/opcodes.h` - Add opcodes
- `src/sblr/bytecode_generator.cpp` - Add visitors

**Executor** (3 files):
- `include/scratchbird/sblr/executor.h` - Add method declarations
- `src/sblr/executor.cpp` - Implement executors
- `src/sblr/expression_evaluator.cpp` - Add sequence function evaluation

**Catalog** (2 files):
- `include/scratchbird/core/catalog_manager.h` - Add SequenceInfo, SequenceState, methods
- `src/core/catalog_manager.cpp` - Implement sequence methods

**Tests** (1 file):
- `test_sequences.sql` - Comprehensive test cases

**Total**: 16 files

---

## Milestones

1. ✅ Design complete (this document)
2. ⏳ Parser & AST (4-6 hours)
3. ⏳ Bytecode generation (1-2 hours)
4. ⏳ Catalog manager (8-12 hours)
5. ⏳ Executor (4-6 hours)
6. ⏳ Expression evaluator (2-3 hours)
7. ⏳ Testing (4-6 hours)
8. ⏳ Documentation (2-3 hours)

**Total**: 30-40 hours

---

## MGA Compliance Notes

**Sequences are intentionally non-transactional**:
- This matches PostgreSQL, Firebird, and SQL standard behavior
- `NEXTVAL()` consumes a value immediately (no rollback)
- Rolled-back transactions leave gaps (acceptable)
- Ensures uniqueness without blocking
- No snapshot/TIP checks needed for sequence operations

**Why this is correct**:
1. Sequences are for generating unique values, not for transaction consistency
2. Blocking NEXTVAL on transaction commit would cause severe contention
3. Gaps from rollbacks are acceptable (auto-increment IDs can have gaps)
4. MGA's TIP system handles tuple visibility; sequences handle ID generation

---

**Author**: Claude Code Assistant
**Date**: November 7, 2025
**Status**: Ready for implementation
