# Alpha 1 - High Priority Issues (P1) Implementation Plan

**Created:** November 23, 2025
**Status:** ✅ 100% COMPLETE (15/15 items) 🎉
**Priority:** P1 - HIGH
**Estimated Effort:** 80-120 hours (0 hours remaining)
**Target:** Beta 1
**Dependencies:** P0 items must be complete first
**Last Updated:** November 25, 2025 (ALL P1 COMPLETE - Bulk loading bottom-up construction implemented!)

---

## OVERVIEW

This plan covers 15 high-priority issues that improve functionality, performance, and PostgreSQL compatibility. These items are important for Beta 1 but not blocking Alpha 1 completion.

**Execution Strategy:** Split into 3 parallel work streams:
- **Agent A:** PSQL/SQL Execution (P1-1, P1-4, P1-5, P1-13, P1-14) - 59-78 hours
- **Agent B:** Performance & Optimization (P1-2, P1-7, P1-8, P1-11) - 32-51 hours
- **Agent C:** Constraints & Catalog (P1-3, P1-6, P1-9, P1-10, P1-12, P1-15) - 63-82 hours

## IMPLEMENTATION STATUS

**Agent A (PSQL/SQL):** ✅ 100% COMPLETE (5/5 items)
- ✅ P1-1: TRY/EXCEPT Exception Handling **ALREADY IMPLEMENTED! (executor.cpp:19142)**
- ✅ P1-4: Cursor Operations (DECLARE/OPEN/FETCH/CLOSE) **ALREADY IMPLEMENTED! (executor.cpp:18956+)**
- ✅ P1-5: Stored Procedure Invocation **ALREADY IMPLEMENTED! (executor.cpp:18321)**
- ✅ P1-13: MERGE Statement (commit 15de05f, Nov 23)
- ✅ P1-14: RETURNING Clause (commit ebd29a7, Nov 23)

**Agent B (Performance):** ✅ 100% COMPLETE (4/4 items)
- ✅ P1-2: XID Wraparound Prevention **ALREADY IMPLEMENTED!**
- ✅ P1-7: TIP Binary Search Optimization **N/A - Using CLOG (O(1) lookup)!**
- ✅ P1-8: Index-Based FK Lookups **ALREADY IMPLEMENTED!**
- ✅ P1-11: Bulk Index Loading **COMPLETE Nov 25! (bottom-up B-tree construction in btree.cpp:2837)**

**Agent C (Constraints/Catalog):** ✅ 100% COMPLETE (6/6 items)
- ✅ P1-3: SQLSTATE Error Codes (commit 9c35bb8, Nov 23)
- ✅ P1-6: Foreign Key Actions (CASCADE/SET NULL) **COMPLETED Nov 25!**
- ✅ P1-9: Constraints Table CRUD (commit a1ed4c8, Nov 23)
- ✅ P1-10: Statistics & ANALYZE **COMPLETED Nov 25! (commit 5676aae)**
- ✅ P1-12: Session Timeout Functionality (commit b54afd4, Nov 23)
- ✅ P1-15: Multi-Geometry Functions (commit eb59170, Nov 24)

---

## AGENT A: PSQL/SQL EXECUTION FEATURES

**Items:** P1-1, P1-4, P1-5, P1-13, P1-14
**Total Effort:** 59-78 hours
**Focus:** Complete PSQL language features and SQL statement execution

---

### P1-1: TRY/EXCEPT Exception Handling

**Effort:** 10-15 hours
**Component:** PSQL Executor
**Files:** `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:15090-15114`

#### Current State
- Infrastructure exists (ExceptionFrame, exception_stack_)
- RAISE throws C++ exceptions
- No TRY/EXCEPT execution logic

#### Implementation Plan

**Phase 1: Exception Types (2 hours)**

Define SQLSTATE exception types:

```cpp
enum class PSQLException {
    NO_DATA_FOUND,           // 02000
    TOO_MANY_ROWS,           // 21000
    DIVISION_BY_ZERO,        // 22012
    UNIQUE_VIOLATION,        // 23505
    FOREIGN_KEY_VIOLATION,   // 23503
    CHECK_VIOLATION,         // 23514
    NOT_NULL_VIOLATION,      // 23502
    // ... all PostgreSQL exception codes
};

struct ExceptionInfo {
    PSQLException exception_type;
    std::string sqlstate;
    std::string message;
    std::string detail;
    std::string hint;
};
```

**Phase 2: TRY Block Execution (4 hours)**

```cpp
Status Executor::executeTryStatement(const Instruction& instr, ErrorContext* ctx) {
    // Push exception frame
    ExceptionFrame frame;
    frame.try_block_start = instr.operand1;  // Jump to TRY block
    frame.except_handlers = instr.operand2;  // Exception handler table offset
    exception_stack_.push(frame);

    // Execute TRY block
    Status status = executeBlock(frame.try_block_start, ctx);

    if (status == Status::OK) {
        // No exception, skip handlers
        exception_stack_.pop();
        return Status::OK;
    }

    // Exception occurred, find matching handler
    return dispatchException(ctx);
}
```

**Phase 3: Exception Handler Dispatch (4 hours)**

```cpp
Status Executor::dispatchException(ErrorContext* ctx) {
    if (exception_stack_.empty()) {
        // Unhandled exception, propagate
        return ctx->status;
    }

    ExceptionFrame frame = exception_stack_.top();
    exception_stack_.pop();

    // Get exception info from context
    ExceptionInfo exc_info = extractExceptionInfo(ctx);

    // Find matching EXCEPT handler
    uint32_t handler_offset = findMatchingHandler(frame.except_handlers, exc_info);

    if (handler_offset == 0) {
        // No matching handler, re-raise
        return ctx->status;
    }

    // Clear error context (exception is being handled)
    ctx->status = Status::OK;
    ctx->message.clear();

    // Execute exception handler
    return executeBlock(handler_offset, ctx);
}

uint32_t Executor::findMatchingHandler(uint32_t handlers_offset,
                                        const ExceptionInfo& exc) {
    // Read exception handler table from bytecode
    // Match by exception name or WHEN OTHERS
    // Return handler block offset or 0 if no match
}
```

**Phase 4: Testing (3 hours)**

```sql
-- Test basic exception handling
CREATE FUNCTION test_exception() RETURNS INTEGER AS $$
BEGIN
    BEGIN
        -- Division by zero
        RETURN 10 / 0;
    EXCEPTION
        WHEN division_by_zero THEN
            RETURN -1;
    END;
END;
$$ LANGUAGE plpgsql;

SELECT test_exception();  -- Should return -1

-- Test multiple exception handlers
-- Test exception propagation
-- Test WHEN OTHERS
```

---

### P1-4: Cursor Operations (Full Implementation)

**Effort:** 20-25 hours
**Component:** PSQL Executor

#### Missing Features
- DECLARE CURSOR
- OPEN cursor
- FETCH (NEXT/PRIOR/FIRST/LAST/ABSOLUTE/RELATIVE)
- CLOSE cursor
- FOR SELECT loops

#### Implementation Plan

**Phase 1: Cursor State Management (4 hours)**

```cpp
struct CursorState {
    std::string name;
    std::string query;           // SQL query text
    std::vector<uint8_t> bytecode;  // Compiled query
    bool is_open = false;
    int64_t current_position = -1;  // -1 = before first row
    std::vector<TypedValue> current_row;
    ResultSet result_set;        // Cached results
    bool is_scrollable = false;  // SCROLL vs NO SCROLL
};

class CursorManager {
public:
    Status declareCursor(const std::string& name,
                        const std::string& query,
                        bool scrollable,
                        ErrorContext* ctx);

    Status openCursor(const std::string& name, ErrorContext* ctx);

    Status fetchCursor(const std::string& name,
                      FetchDirection direction,
                      int64_t count,
                      std::vector<TypedValue>& row,
                      ErrorContext* ctx);

    Status closeCursor(const std::string& name, ErrorContext* ctx);

    bool cursorExists(const std::string& name);

private:
    std::unordered_map<std::string, CursorState> cursors_;
    std::mutex mutex_;
};

enum class FetchDirection {
    NEXT,
    PRIOR,
    FIRST,
    LAST,
    ABSOLUTE,
    RELATIVE,
    FORWARD,
    BACKWARD
};
```

**Phase 2: DECLARE CURSOR (4 hours)**

```cpp
Status Executor::executeDeclareC cursor(const Instruction& instr, ErrorContext* ctx) {
    std::string cursor_name = getString(instr.operand1);
    std::string query = getString(instr.operand2);
    bool scrollable = (instr.flags & CURSOR_SCROLLABLE) != 0;

    return cursor_manager_->declareCursor(cursor_name, query, scrollable, ctx);
}
```

**Phase 3: OPEN CURSOR (4 hours)**

```cpp
Status CursorManager::openCursor(const std::string& name, ErrorContext* ctx) {
    auto it = cursors_.find(name);
    if (it == cursors_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Cursor not found: " + name);
        return Status::NOT_FOUND;
    }

    CursorState& cursor = it->second;
    if (cursor.is_open) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_STATE, "Cursor already open: " + name);
        return Status::INVALID_STATE;
    }

    // Execute query and cache results
    Status status = executor_->executeQuery(cursor.bytecode, &cursor.result_set, ctx);
    if (status != Status::OK) {
        return status;
    }

    cursor.is_open = true;
    cursor.current_position = -1;  // Before first row
    return Status::OK;
}
```

**Phase 4: FETCH Operations (6 hours)**

Implement all fetch directions:

```cpp
Status CursorManager::fetchCursor(const std::string& name,
                                  FetchDirection direction,
                                  int64_t count,
                                  std::vector<TypedValue>& row,
                                  ErrorContext* ctx) {
    auto it = cursors_.find(name);
    if (it == cursors_.end()) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Cursor not found");
        return Status::NOT_FOUND;
    }

    CursorState& cursor = it->second;
    if (!cursor.is_open) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_STATE, "Cursor not open");
        return Status::INVALID_STATE;
    }

    int64_t new_position = cursor.current_position;

    switch (direction) {
        case FetchDirection::NEXT:
            new_position++;
            break;
        case FetchDirection::PRIOR:
            if (!cursor.is_scrollable) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                    "Cannot fetch PRIOR on non-scrollable cursor");
                return Status::INVALID_ARGUMENT;
            }
            new_position--;
            break;
        case FetchDirection::FIRST:
            new_position = 0;
            break;
        case FetchDirection::LAST:
            new_position = cursor.result_set.rows.size() - 1;
            break;
        case FetchDirection::ABSOLUTE:
            new_position = count - 1;  // 1-indexed to 0-indexed
            break;
        case FetchDirection::RELATIVE:
            new_position = cursor.current_position + count;
            break;
    }

    // Check bounds
    if (new_position < 0 || new_position >= cursor.result_set.rows.size()) {
        // Return NO_DATA (not an error)
        return Status::NO_DATA;
    }

    // Fetch row
    cursor.current_position = new_position;
    row = cursor.result_set.rows[new_position];
    return Status::OK;
}
```

**Phase 5: FOR SELECT Loops (4 hours)**

```cpp
// Opcode: EXT_FOR_SELECT
// Bytecode: FOR var IN query LOOP ... END LOOP
Status Executor::executeForSelectLoop(const Instruction& instr, ErrorContext* ctx) {
    std::string var_name = getString(instr.operand1);
    std::vector<uint8_t> query_bytecode = getBytes(instr.operand2);
    uint32_t loop_body_offset = instr.operand3;

    // Execute query
    ResultSet results;
    Status status = executeQuery(query_bytecode, &results, ctx);
    if (status != Status::OK) {
        return status;
    }

    // Iterate over results
    for (const auto& row : results.rows) {
        // Assign row to loop variable
        setVariable(var_name, row);

        // Execute loop body
        status = executeBlock(loop_body_offset, ctx);
        if (status == Status::EXIT_LOOP) {
            break;  // EXIT statement
        } else if (status != Status::OK) {
            return status;
        }
    }

    return Status::OK;
}
```

**Phase 6: Testing (3 hours)**

Test all cursor operations, scrollable/non-scrollable, FOR loops.

---

### P1-5: Stored Procedure Invocation

**Effort:** 15-20 hours
**Component:** PSQL Executor
**Files:** `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:14521+`

#### Missing Features
- Complete function execution
- Parameter binding (IN/OUT/INOUT)
- Return value handling
- SQL SECURITY DEFINER/INVOKER

#### Implementation Plan

**Phase 1: Function Call Stack (3 hours)**

```cpp
struct FunctionFrame {
    std::string function_name;
    ID function_id;
    std::unordered_map<std::string, TypedValue> parameters;  // IN params
    std::unordered_map<std::string, TypedValue*> out_params;  // OUT/INOUT params
    std::optional<TypedValue> return_value;
    ID original_user;  // For SECURITY DEFINER
    bool security_definer = false;
};

class FunctionCallStack {
public:
    void pushFrame(const FunctionFrame& frame);
    FunctionFrame& topFrame();
    void popFrame();
    size_t depth() const { return stack_.size(); }

private:
    std::vector<FunctionFrame> stack_;
    static constexpr size_t MAX_RECURSION_DEPTH = 100;
};
```

**Phase 2: Parameter Binding (4 hours)**

```cpp
Status Executor::bindFunctionParameters(
    const FunctionInfo& func,
    const std::vector<TypedValue>& args,
    FunctionFrame& frame,
    ErrorContext* ctx
) {
    if (args.size() != func.parameters.size()) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "Function parameter count mismatch");
        return Status::INVALID_ARGUMENT;
    }

    for (size_t i = 0; i < func.parameters.size(); i++) {
        const auto& param = func.parameters[i];

        if (param.mode == ParameterMode::IN ||
            param.mode == ParameterMode::INOUT) {
            // Copy IN value
            frame.parameters[param.name] = args[i];
        }

        if (param.mode == ParameterMode::OUT ||
            param.mode == ParameterMode::INOUT) {
            // Store reference for OUT parameter
            frame.out_params[param.name] = const_cast<TypedValue*>(&args[i]);
        }
    }

    return Status::OK;
}
```

**Phase 3: SECURITY DEFINER/INVOKER (4 hours)**

```cpp
Status Executor::invokeProcedure(const std::string& proc_name,
                                const std::vector<TypedValue>& args,
                                ErrorContext* ctx) {
    // Look up function in catalog
    auto func_opt = catalog_->getFunction(proc_name, ctx);
    if (!func_opt) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Function not found: " + proc_name);
        return Status::NOT_FOUND;
    }

    FunctionInfo func = *func_opt;

    // Create stack frame
    FunctionFrame frame;
    frame.function_name = proc_name;
    frame.function_id = func.id;
    frame.security_definer = (func.security_type == SecurityType::DEFINER);

    // Bind parameters
    Status status = bindFunctionParameters(func, args, frame, ctx);
    if (status != Status::OK) {
        return status;
    }

    // Handle SECURITY DEFINER
    if (frame.security_definer) {
        frame.original_user = current_user_;
        current_user_ = func.owner_id;  // Switch to function owner
    }

    // Push frame
    function_stack_->pushFrame(frame);

    // Execute function bytecode
    status = executeBlock(func.bytecode_offset, ctx);

    // Handle SECURITY DEFINER cleanup
    if (frame.security_definer) {
        current_user_ = frame.original_user;  // Restore original user
    }

    // Copy OUT parameter values
    if (status == Status::OK) {
        for (const auto& [name, ptr] : frame.out_params) {
            *ptr = frame.parameters[name];
        }
    }

    // Pop frame
    function_stack_->popFrame();

    return status;
}
```

**Phase 4: RETURN Statement (2 hours)**

```cpp
Status Executor::executeReturn(const Instruction& instr, ErrorContext* ctx) {
    if (function_stack_->depth() == 0) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_STATE, "RETURN outside function");
        return Status::INVALID_STATE;
    }

    TypedValue return_val = evaluateExpression(instr.operand1, ctx);
    function_stack_->topFrame().return_value = return_val;

    return Status::RETURN_FROM_FUNCTION;  // Special status to exit function
}
```

**Phase 5: Testing (4 hours)**

Test IN/OUT/INOUT parameters, SECURITY DEFINER, recursion limits, return values.

---

### P1-13: MERGE Statement Completion

**Effort:** 8-10 hours
**Component:** SQL Executor
**Files:** `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:5532-5570`

#### Current State
- Basic structure exists
- Missing full matching logic with ON condition
- Missing INSERT logic for unmatched source rows
- Missing DELETE logic for unmatched target rows

#### Implementation Plan

**Phase 1: Match Classification (3 hours)**

```cpp
struct MergeMatch {
    TID target_tid;
    std::vector<TypedValue> source_row;
    bool matched;  // true = matched, false = not matched
};

Status Executor::classifyMergeMatches(
    const std::string& target_table,
    const std::string& source_query,
    const Expression& on_condition,
    std::vector<MergeMatch>& matches,
    ErrorContext* ctx
) {
    // Execute source query
    ResultSet source_results;
    Status status = executeQuery(source_query, &source_results, ctx);
    if (status != Status::OK) {
        return status;
    }

    // For each source row, find matching target rows
    for (const auto& source_row : source_results.rows) {
        // Evaluate ON condition for all target rows
        bool found_match = false;

        // Scan target table
        for (TID target_tid : scanTable(target_table)) {
            std::vector<TypedValue> target_row = fetchRow(target_tid);

            // Evaluate ON condition
            TypedValue match_result = evaluateJoinCondition(
                on_condition, source_row, target_row, ctx);

            if (match_result.getBool()) {
                // Matched
                matches.push_back({target_tid, source_row, true});
                found_match = true;
                break;  // Process only first match
            }
        }

        if (!found_match) {
            // Not matched by target
            matches.push_back({TID_INVALID, source_row, false});
        }
    }

    return Status::OK;
}
```

**Phase 2: WHEN MATCHED THEN UPDATE (2 hours)**

```cpp
Status Executor::executeMergeMatched(
    const std::string& target_table,
    const MergeMatch& match,
    const std::vector<Assignment>& assignments,
    ErrorContext* ctx
) {
    // Update target row
    std::vector<TypedValue> new_values;

    for (const auto& assignment : assignments) {
        TypedValue value = evaluateExpression(assignment.expression, ctx);
        new_values.push_back(value);
    }

    return updateRow(target_table, match.target_tid, new_values, ctx);
}
```

**Phase 3: WHEN NOT MATCHED THEN INSERT (2 hours)**

```cpp
Status Executor::executeMergeNotMatched(
    const std::string& target_table,
    const MergeMatch& match,
    const std::vector<std::string>& columns,
    const std::vector<Expression>& values,
    ErrorContext* ctx
) {
    // Insert new row from source
    std::vector<TypedValue> insert_values;

    for (const auto& value_expr : values) {
        TypedValue value = evaluateExpression(value_expr, ctx);
        insert_values.push_back(value);
    }

    return insertRow(target_table, columns, insert_values, ctx);
}
```

**Phase 4: WHEN NOT MATCHED BY SOURCE THEN DELETE (1 hour)**

```cpp
// Identify target rows not matched by any source row
// Execute DELETE
```

**Phase 5: Testing (2 hours)**

---

### P1-14: RETURNING Clause Implementation

**Effort:** 6-8 hours
**Component:** SQL Executor
**Files:** `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp:4398`

#### Current State
- Opcode exists but implementation incomplete

#### Implementation Plan

**Phase 1: RETURNING Structure (2 hours)**

```cpp
struct ReturningClause {
    std::vector<Expression> expressions;  // Columns to return
    std::vector<std::string> aliases;     // Optional aliases
};

// Store RETURNING results
struct ReturningResults {
    std::vector<std::vector<TypedValue>> rows;
    std::vector<std::string> column_names;
};
```

**Phase 2: INSERT RETURNING (2 hours)**

```cpp
Status Executor::executeInsertReturning(
    const std::string& table,
    const std::vector<TypedValue>& values,
    const ReturningClause& returning,
    ReturningResults& results,
    ErrorContext* ctx
) {
    // Execute INSERT
    TID new_tid;
    Status status = insertRow(table, values, &new_tid, ctx);
    if (status != Status::OK) {
        return status;
    }

    // Fetch inserted row
    std::vector<TypedValue> inserted_row = fetchRow(new_tid);

    // Evaluate RETURNING expressions
    std::vector<TypedValue> returning_row;
    for (const auto& expr : returning.expressions) {
        TypedValue value = evaluateExpression(expr, inserted_row, ctx);
        returning_row.push_back(value);
    }

    results.rows.push_back(returning_row);
    return Status::OK;
}
```

**Phase 3: UPDATE/DELETE RETURNING (2 hours)**

Similar implementation for UPDATE and DELETE.

**Phase 4: Testing (2 hours)**

---

## AGENT B: PERFORMANCE & OPTIMIZATION

**Items:** P1-2, P1-7, P1-8, P1-11
**Total Effort:** 32-51 hours
**Focus:** Performance improvements and optimization

---

### P1-2: XID Wraparound Prevention

**Effort:** 3-5 hours
**Component:** Transaction Manager
**Files:** `/home/dcalford/CliWork/ScratchBird/src/core/transaction_manager.cpp:232-241`

#### Issue
Database stops accepting transactions at wraparound (XID = 2^32)

#### Implementation Plan

```cpp
Status TransactionManager::checkXIDWraparound(ErrorContext* ctx) {
    TransactionId next_xid = header_->next_transaction;
    TransactionId oit = header_->oldest_transaction;

    // Calculate age
    uint64_t age = next_xid - oit;

    // Warning at 1 billion XIDs
    if (age > 1'000'000'000) {
        LOG_WARNING("XID age is " + std::to_string(age) + " - consider running VACUUM");
    }

    // Critical at 1.8 billion (90% of 2^31)
    if (age > 1'800'000'000) {
        LOG_ERROR("XID wraparound imminent - forcing autovacuum");
        // Trigger emergency sweep
        triggerEmergencySweep(ctx);
    }

    // Prevent wraparound
    if (age > 2'000'000'000) {
        SET_ERROR_CONTEXT(ctx, Status::XID_WRAPAROUND,
            "Database must be vacuumed to prevent XID wraparound");
        return Status::XID_WRAPAROUND;
    }

    return Status::OK;
}

// Call in beginTransaction()
Status TransactionManager::beginTransaction(IsolationLevel level, bool read_only) {
    // Check wraparound BEFORE allocating new XID
    Status status = checkXIDWraparound(nullptr);
    if (status != Status::OK) {
        return status;
    }

    // Existing logic...
}
```

---

### P1-7: TIP Binary Search Optimization

**Effort:** 4-6 hours
**Component:** Transaction Manager
**Files:** `/home/dcalford/CliWork/ScratchBird/src/core/transaction_manager.cpp:1102-1128`

#### Current State
O(N) linear search within TIP page (up to 32K entries)

#### Improvement
O(log N) binary search

#### Implementation

```cpp
TxState TransactionManager::getTransactionState(TransactionId xid) {
    // Calculate TIP page
    uint32_t page_num = xid / TRANSACTIONS_PER_TIP_PAGE;
    uint32_t offset = xid % TRANSACTIONS_PER_TIP_PAGE;

    // Pin TIP page
    BufferFrame* frame = buffer_pool_->pinPage(getTIPPageNum(page_num), nullptr);
    SBTipPage* tip_page = reinterpret_cast<SBTipPage*>(frame->data);

    // OPTIMIZATION: Binary search instead of linear scan
    // Since TIP pages are sorted by XID, use binary search

    TxState state;
    if (xid >= tip_page->tip_min && xid <= tip_page->tip_max) {
        // XID is on this page - direct index
        uint32_t local_offset = xid - tip_page->tip_min;
        uint8_t byte = tip_page->tip_transactions[local_offset / 4];
        uint8_t shift = (local_offset % 4) * 2;
        state = (TxState)((byte >> shift) & 0x03);
    } else {
        // XID not on this page - binary search across TIP pages
        state = binarySearchTIP(xid);
    }

    buffer_pool_->unpinPage(getTIPPageNum(page_num));
    return state;
}

// Binary search for TIP page containing XID
TxState TransactionManager::binarySearchTIP(TransactionId xid) {
    uint32_t left = 0;
    uint32_t right = getTIPPageCount() - 1;

    while (left <= right) {
        uint32_t mid = (left + right) / 2;

        BufferFrame* frame = buffer_pool_->pinPage(getTIPPageNum(mid), nullptr);
        SBTipPage* page = reinterpret_cast<SBTipPage*>(frame->data);

        if (xid < page->tip_min) {
            right = mid - 1;
        } else if (xid > page->tip_max) {
            left = mid + 1;
        } else {
            // Found page containing XID
            uint32_t local_offset = xid - page->tip_min;
            uint8_t byte = page->tip_transactions[local_offset / 4];
            uint8_t shift = (local_offset % 4) * 2;
            TxState state = (TxState)((byte >> shift) & 0x03);
            buffer_pool_->unpinPage(getTIPPageNum(mid));
            return state;
        }

        buffer_pool_->unpinPage(getTIPPageNum(mid));
    }

    // XID not found - assume ABORTED
    return TxState::TX_ABORTED;
}
```

**Impact:** 10-100x speedup for transaction state lookups

---

### P1-8: Index-Based FK Lookups

**Effort:** 10-15 hours
**Component:** Constraint Enforcement

#### Current State
O(N) table scan to find parent/child rows

#### Improvement
Use index for O(log N) lookup

#### Implementation

```cpp
Status Executor::checkForeignKeyConstraint(
    const std::string& child_table,
    const std::string& parent_table,
    const std::vector<std::string>& fk_columns,
    const std::vector<TypedValue>& values,
    ErrorContext* ctx
) {
    // OPTIMIZATION: Use index on parent table if available
    std::string parent_index = findIndexForColumns(parent_table, fk_columns);

    if (!parent_index.empty()) {
        // Use index lookup - O(log N)
        Key search_key = constructKey(values);
        std::vector<TID> results;
        Status status = searchIndex(parent_index, search_key, &results, ctx);

        if (results.empty()) {
            SET_ERROR_CONTEXT(ctx, Status::FK_VIOLATION,
                "Foreign key constraint violated: parent row not found");
            return Status::FK_VIOLATION;
        }
    } else {
        // Fallback to table scan - O(N)
        bool found = false;
        for (TID tid : scanTable(parent_table)) {
            std::vector<TypedValue> parent_row = fetchRow(tid);
            if (rowMatchesValues(parent_row, fk_columns, values)) {
                found = true;
                break;
            }
        }

        if (!found) {
            SET_ERROR_CONTEXT(ctx, Status::FK_VIOLATION,
                "Foreign key constraint violated");
            return Status::FK_VIOLATION;
        }
    }

    return Status::OK;
}
```

**Impact:** 100-1000x speedup for FK checks

---

### P1-11: Bulk Loading for Indexes

**Effort:** 15-20 hours per index type
**Component:** Index System

#### Current State
O(N log N) individual inserts

#### Improvement
O(N) bottom-up construction

#### Implementation (B-Tree Example)

```cpp
Status BTreeIndex::bulkLoad(const std::vector<std::pair<Key, TID>>& entries,
                             ErrorContext* ctx) {
    // Sort entries by key - O(N log N)
    std::vector<std::pair<Key, TID>> sorted = entries;
    std::sort(sorted.begin(), sorted.end());

    // Build B-tree bottom-up - O(N)
    std::vector<PageNum> leaf_pages;

    // Phase 1: Create leaf pages
    size_t entries_per_leaf = (PAGE_SIZE - sizeof(BTreePage)) / sizeof(BTreeEntry);
    for (size_t i = 0; i < sorted.size(); i += entries_per_leaf) {
        PageNum leaf_page = allocateLeafPage();
        size_t count = std::min(entries_per_leaf, sorted.size() - i);

        for (size_t j = 0; j < count; j++) {
            insertIntoLeaf(leaf_page, sorted[i + j].first, sorted[i + j].second);
        }

        leaf_pages.push_back(leaf_page);
    }

    // Phase 2: Build internal nodes bottom-up
    std::vector<PageNum> current_level = leaf_pages;
    while (current_level.size() > 1) {
        std::vector<PageNum> next_level;

        size_t children_per_internal = (PAGE_SIZE - sizeof(BTreePage)) / sizeof(BTreeInternal);
        for (size_t i = 0; i < current_level.size(); i += children_per_internal) {
            PageNum internal_page = allocateInternalPage();
            size_t count = std::min(children_per_internal, current_level.size() - i);

            for (size_t j = 0; j < count; j++) {
                addChild(internal_page, current_level[i + j]);
            }

            next_level.push_back(internal_page);
        }

        current_level = next_level;
    }

    // Set root
    root_page_ = current_level[0];
    return Status::OK;
}
```

**Impact:** 3-5x faster initial index build

---

## AGENT C: CONSTRAINTS & CATALOG

**Items:** P1-3, P1-6, P1-9, P1-10, P1-12, P1-15
**Total Effort:** 63-82 hours
**Focus:** Constraint completion, catalog CRUD, spatial functions

(Continue with similar detailed plans for remaining P1 items...)

---

## EXECUTION TIMELINE

**Total Duration:** 8-10 weeks with 3 agents in parallel

### Weeks 1-2 (Agent A)
- P1-1 TRY/EXCEPT
- Start P1-4 Cursors

### Weeks 1-3 (Agent B)
- P1-2 XID Wraparound
- P1-7 TIP Binary Search
- P1-8 Index FK Lookups

### Weeks 1-4 (Agent C)
- P1-3 SQLSTATE
- P1-9 Constraints Table
- P1-12 Session Timeout

### Weeks 3-6 (Agent A)
- Complete P1-4 Cursors
- P1-5 Stored Procedures
- P1-13 MERGE
- P1-14 RETURNING

### Weeks 4-6 (Agent B)
- P1-11 Bulk Loading (B-Tree)

### Weeks 5-8 (Agent C)
- P1-6 Foreign Key Actions
- P1-10 Statistics & ANALYZE
- P1-15 Multi-Geometry Functions

### Weeks 9-10 (All Agents)
- Integration testing
- Bug fixes
- Performance validation

---

**Document Status:** READY FOR IMPLEMENTATION
**Last Updated:** November 23, 2025
