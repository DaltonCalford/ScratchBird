# Specification: SBLR v3 Execution Model

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | sblr |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird SBLR v3 |
| **Authors** | Dalton Calford |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/executor.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_handler_registry.h:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_sblr_v3_handler_registry.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_sblr_jit_*.cpp` (various)

## Synopsis

This specification defines the execution model for SBLR v3 bytecode. The executor interprets the instruction stream, maintains execution state (stack, program counter, result sets), and dispatches to handlers for each opcode. It supports both direct interpretation and JIT-compiled execution paths.

## Scope

### In Scope

- Executor state machine and lifecycle
- Instruction dispatch mechanism
- Value stack operations
- Program counter advancement
- Result set construction
- Handler registration and lookup
- Execution context (CTE, row context, parameters)

### Out of Scope

- Specific opcode handler implementations (see individual handler specs)
- JIT compilation details (see JIT specifications)
- Storage engine interactions (see storage specifications)

## Background

The SBLR v3 executor is a stack-based virtual machine that processes compiled SQL statements. Key design principles:

1. **Stack-Based Evaluation**: Expressions are evaluated using an operand stack
2. **Canonical V3 Path**: Modern container-based execution (legacy stream retired)
3. **JIT Integration**: Hot paths can be compiled to native code
4. **Query Limits**: Resource governance through execution limits
5. **Cancellation**: Cooperative cancellation support

## Specification

### Data Structures

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/executor.h:63
using Value = core::TypedValue;
```

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/executor.h:66
class ResultSet {
    std::vector<std::string> column_names_;
    std::vector<core::DataType> column_types_;
    std::vector<std::vector<Value>> rows_;
    RowCallback row_callback_;
    bool store_rows_ = true;
};
```

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/executor.h:117
class ExecutionResult {
    enum ResultType { SUCCESS, ERROR, RESULT_SET };
    ResultType type_;
    std::string error_;
    std::unique_ptr<ResultSet> result_set_;
    int affected_count_ = 0;
};
```

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/executor.h:171
struct IndexMaintenanceStats {
    uint64_t entries_added = 0;
    uint64_t entries_removed = 0;
    uint64_t entries_updated = 0;
    uint64_t expression_evaluations = 0;
    uint64_t predicate_evaluations = 0;
    uint64_t invisible_skipped = 0;
    uint64_t indexes_maintained = 0;
    double total_eval_time_ms = 0.0;
    double total_insert_time_ms = 0.0;
    double total_remove_time_ms = 0.0;
};
```

### Executor State Machine

```
┌─────────┐    construct     ┌─────────┐
│  IDLE   │ ────────────────►│  INIT   │
└─────────┘                  └────┬────┘
                                  │
                                  │ setConnectionContext()
                                  ▼
┌─────────┐    execute()    ┌─────────┐
│  DONE   │ ◄────────────── │  READY  │
└────┬────┘                 └────┬────┘
     │                           │
     │ execute() returns         │ execute bytecode
     │                           ▼
     │                      ┌─────────┐
     └───────────────────── │RUNNING  │
                            └────┬────┘
                                 │
                    ┌────────────┼────────────┐
                    │            │            │
                    ▼            ▼            ▼
              ┌─────────┐  ┌─────────┐  ┌─────────┐
              │ SUCCESS │  │  ERROR  │  │CANCELLED│
              └────┬────┘  └────┬────┘  └────┬────┘
                   │            │            │
                   └────────────┴────────────┘
                                  │
                                  ▼
                           ┌─────────┐
                           │  DONE   │
                           └─────────┘
```

| Current State | Event | Action | Next State |
|---------------|-------|--------|------------|
| IDLE | construct | Initialize executor | INIT |
| INIT | setConnectionContext | Set security context | READY |
| READY | execute(bytecode) | Begin execution | RUNNING |
| RUNNING | Complete successfully | Return result | DONE |
| RUNNING | Error occurs | Set error message | DONE |
| RUNNING | Cancellation requested | Abort execution | DONE |
| DONE | - | - | READY (for next execute) |

### Execution State

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/executor.h:419
class Executor {
    // Bytecode being executed
    const uint8_t *bytecode_;
    size_t bytecode_size_;
    size_t pc_;  // Program counter
    
    // Evaluation stack
    std::stack<Value> stack_;
    
    // CTE context
    std::unordered_map<std::string, std::vector<std::vector<Value>>> cte_results_;
    
    // Row context for expression evaluation
    const std::vector<Value> *current_row_values_ = nullptr;
    
    // Bound parameters
    std::vector<std::string> parameter_values_;
    std::vector<bool> parameter_nulls_;
    
    // Cancellation flag
    std::atomic<bool> cancel_requested_{false};
};
```

Source: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/executor.h:419-461`

### Instruction Dispatch

The executor uses a dispatch loop to process instructions:

```
Input:  Bytecode stream, execution context
Output: ExecutionResult

1. Parse container header
2. For each instruction in bytecode stream:
   a. Read opcode (u16)
   b. Check cancellation flag
   c. Look up handler in registry
   d. Call handler with current context
   e. Advance PC
3. Return result
```

Source: `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp` (dispatch loop)

### Stack Operations

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/executor.h:612
void push(const Value &v) {
    stack_.push(v);
}

Value pop();  // Throws if stack empty

Value getStackValueAtOffset(uint16_t offset);

void setStackValueAtOffset(uint16_t offset, const Value& value);
```

### Execution Context

The executor maintains multiple layers of context:

1. **Statement Context**: Current table, columns being operated on
2. **Row Context**: Values for the current row during SELECT/UPDATE
3. **CTE Context**: Materialized common table expressions
4. **Insert Context**: VALUES() references for ON CONFLICT
5. **Aggregate Context**: Grouping and aggregation state
6. **Window Context**: Window function evaluation state

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/executor.h:434
std::string current_table_;
std::vector<std::string> current_columns_;
std::unique_ptr<ResultSet> current_result_set_;

// Row context
const std::vector<Value> *current_row_values_ = nullptr;
const std::vector<core::CatalogManager::ColumnInfo> *current_row_columns_ = nullptr;

// Insert context
const std::vector<Value> *current_insert_values_ = nullptr;

// Aggregate context
bool aggregate_scan_active_ = false;
bool aggregate_scan_found_ = false;

// Grouping context
size_t current_grouping_set_index_ = 0;
std::vector<size_t> current_grouping_set_column_pcs_;
```

### Handler Registry

Handlers are registered by opcode:

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/v3_handler_registry.h
using OpcodeHandler = std::function<void(
    uint16_t opcode,
    uint16_t flags,
    const uint8_t* payload,
    uint32_t payload_len,
    Executor* executor,
    core::Database* db,
    core::ErrorContext* ctx
)>;

void registerHandler(uint16_t opcode, OpcodeHandler handler);
OpcodeHandler lookupHandler(uint16_t opcode);
```

### Interface Contracts

#### Executor::execute()

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/executor.h:211
ExecutionResult execute(const std::vector<uint8_t> &bytecode);
```

**Preconditions:**
- Database pointer is valid
- Bytecode is valid SBLR v3 container

**Postconditions:**
- Returns SUCCESS, ERROR, or RESULT_SET
- ResultSet populated for SELECT statements
- affected_count set for DML

**Thread Safety:**
- Cancellation flag is atomic
- Stack is per-executor

#### Executor::requestCancellation()

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/executor.h:269
void requestCancellation();
```

**Preconditions:**
- Executor is in RUNNING state

**Postconditions:**
- cancel_requested_ flag set
- Execution will terminate at next cancellation check

### Algorithms

#### Expression Evaluation

```
Input:  Expression bytecode range
Output: Value result

1. Save current PC
2. Set PC to expression start
3. While PC < expression end:
   a. Read opcode
   b. Dispatch to expression handler
   c. Handler pushes result to stack
4. Pop final value from stack
5. Restore PC
6. Return value
```

Source: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/executor.h:842`

#### Aggregate Execution

```
Input:  Table info, SELECT items, WHERE clause
Output: ResultSet with aggregated rows

1. Initialize aggregate accumulators
2. Scan table rows:
   a. Evaluate WHERE clause
   b. Build group key from GROUP BY
   c. Locate or create accumulator group
   d. Accumulate values into group
3. Finalize each group
4. Build result rows from finalized values
```

Source: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/executor.h:808`

#### Window Function Execution

```
Input:  Input result set, window specs
Output: ResultSet with window columns

1. For each partition:
   a. Identify partition rows
   b. Sort within partition
   c. For each row:
      i. Determine frame bounds
      ii. Compute window function
      iii. Store result
2. Return modified result set
```

Source: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/sblr/executor.h:839`

## Invariants

1. **Stack Balance**: Expression evaluation leaves exactly one value on stack
   - Verification: pop() in evaluateExpressionRange

2. **PC Validity**: Program counter always within bytecode bounds
   - Verification: Bounds checks before read

3. **Cancellation Responsiveness**: Long-running operations check cancel flag
   - Verification: Atomic load in hot loops

4. **Result Set Validity**: Column count matches row values
   - Verification: addRow validates vector size

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| Stack underflow | pop() on empty stack | Return execution error |
| Invalid opcode | Unknown opcode in stream | Return execution error |
| Division by zero | DIVIDE with zero divisor | Return execution error |
| Null constraint violation | NOT NULL check fails | Return execution error |
| Query limit exceeded | Rows/time exceeds limits | Cancel execution |
| Cancellation | cancel_requested_ set | Abort and return error |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `test_sblr_v3_handler_registry.cpp` | Handler registration/lookup |
| `test_sblr_jit_*.cpp` | JIT integration tests |
| `test_sblr_v3_payload_codec.cpp` | Instruction decoding |
| `test_sblr_v3_container.cpp` | Container execution |

## Appendix

### Glossary

| Term | Definition |
|------|------------|
| PC | Program Counter - current instruction position |
| CTE | Common Table Expression - named subquery |
| Handler | Function that executes a specific opcode |
| Dispatch | Selecting and calling the appropriate handler |
| Frame | Window function row range specification |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | SBLR Team |
