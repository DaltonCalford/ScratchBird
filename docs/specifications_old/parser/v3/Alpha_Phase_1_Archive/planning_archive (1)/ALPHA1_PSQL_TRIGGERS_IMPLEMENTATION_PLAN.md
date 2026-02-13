# Alpha 1: PSQL/Stored Procedures & Triggers Implementation Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Created:** November 21, 2025
**Status:** Not Started (~15% of Alpha 1 remaining)
**Priority:** HIGH
**Dependencies:** None

---

## Overview

Implement execution infrastructure for PSQL procedural language and trigger firing mechanism. Parser and catalog support already exist; bytecode execution is ~90% stubbed.

---

## Current Status

### What's Complete ✅

**Parser Layer (100%)**
- CREATE PROCEDURE/FUNCTION syntax (`parser.cpp:1576, 1660`)
- CREATE TRIGGER syntax (`parser.cpp:1422`)
- PSQL control flow syntax (IF, LOOP, WHILE, EXIT, RETURN)
- Variable declarations (DECLARE...BEGIN...END)
- Exception handling syntax (RAISE EXCEPTION/NOTICE/WARNING)

**Catalog Layer (100%)**
- `StoredProcedureInfo` structure (`catalog_manager.h:758-772`)
- `ProcedureParameterInfo` structure (`catalog_manager.h:776-785`)
- `TriggerInfo` structure (referenced in executor)
- Procedure metadata storage

**Bytecode Layer (~90%)**
- Opcodes defined for PSQL operations
- Bytecode generation from AST to SBLR
- Executor stubs for control flow

### What's Missing ❌

**Executor Layer**
- Bytecode interpreter for PSQL operations
- Trigger firing mechanism (BEFORE/AFTER/INSTEAD OF)
- Exception handling (TRY/CATCH execution)
- Cursor operations (DECLARE, OPEN, FETCH, CLOSE)
- Variable scope management
- FOR SELECT loops

---

## Implementation Tasks

### Task 1: Variable Scope Management

**File:** `src/sblr/executor.cpp`
**Estimated Lines:** ~200

**Requirements:**
1. Variable stack for nested scopes
2. Variable lookup by name
3. Type checking during assignment
4. %ROWTYPE variable support
5. CONSTANT variable support

**Data Structures:**
```cpp
struct VariableScope {
    std::unordered_map<std::string, Value> variables;
    VariableScope* parent_scope;
    bool is_exception_handler;
};

class ScopeStack {
    std::vector<std::unique_ptr<VariableScope>> scopes;

    void pushScope(bool is_exception_handler = false);
    void popScope();
    Value* findVariable(const std::string& name);
    void setVariable(const std::string& name, const Value& value);
};
```

**Implementation Steps:**
1. Add `ScopeStack` member to `Executor` class
2. Implement `pushScope()` on DECLARE block entry
3. Implement `popScope()` on block exit
4. Modify variable assignment opcodes to use scope stack
5. Add %ROWTYPE variable creation from table definitions

**Testing:**
- Nested block variable shadowing
- Variable access across scopes
- Invalid variable references
- %ROWTYPE variable operations

---

### Task 2: Control Flow Execution

**File:** `src/sblr/executor.cpp`
**Estimated Lines:** ~300

**Requirements:**
1. IF...ELSIF...ELSE execution
2. CASE statement execution
3. LOOP execution
4. WHILE execution
5. EXIT [label] [WHEN condition]
6. RETURN [expression]

**Opcodes:**
- `IF_JUMP` - Conditional jump
- `JUMP` - Unconditional jump
- `LOOP_START` - Mark loop beginning
- `LOOP_END` - Mark loop end
- `EXIT_LOOP` - Break from loop
- `RETURN_VALUE` - Return from procedure/function

**Implementation Steps:**
1. Implement jump target resolution
2. Add loop stack for EXIT label tracking
3. Implement IF_JUMP execution
4. Implement LOOP_START/LOOP_END execution
5. Implement EXIT_LOOP with label support
6. Implement RETURN_VALUE with expression evaluation

**Testing:**
- Nested IF statements
- Complex CASE expressions
- Nested loops with labels
- Early returns
- EXIT WHEN conditions

---

### Task 3: Cursor Operations

**File:** `src/sblr/executor.cpp`
**Estimated Lines:** ~400

**Requirements:**
1. DECLARE CURSOR
2. OPEN cursor
3. FETCH cursor INTO variables
4. CLOSE cursor
5. FOR SELECT loops (implicit cursors)

**Data Structures:**
```cpp
struct Cursor {
    std::string name;
    std::string query_sql;
    ResultSet result_set;
    size_t current_row;
    bool is_open;
};

class CursorManager {
    std::unordered_map<std::string, Cursor> cursors;

    void declareCursor(const std::string& name, const std::string& sql);
    void openCursor(const std::string& name, TransactionId xid);
    bool fetchCursor(const std::string& name, std::vector<Value>& row);
    void closeCursor(const std::string& name);
};
```

**Implementation Steps:**
1. Add `CursorManager` to `Executor` class
2. Implement `declareCursor()` - store query SQL
3. Implement `openCursor()` - execute query, store result set
4. Implement `fetchCursor()` - return next row, update position
5. Implement `closeCursor()` - free resources
6. Implement FOR SELECT loop sugar (implicit cursor)

**Testing:**
- Simple cursor iteration
- Nested cursors
- Cursor re-opening
- Cursor closing without opening
- FOR SELECT loop

---

### Task 4: Exception Handling

**File:** `src/sblr/executor.cpp`
**Estimated Lines:** ~250

**Requirements:**
1. RAISE EXCEPTION execution
2. RAISE NOTICE execution
3. RAISE WARNING execution
4. TRY/EXCEPT blocks (if implemented)
5. Exception propagation

**Data Structures:**
```cpp
struct ExceptionHandler {
    uint32_t handler_offset;  // Bytecode offset for EXCEPT block
    std::string exception_code;  // Exception code to catch (or "ALL")
    VariableScope* handler_scope;
};

class ExceptionStack {
    std::vector<ExceptionHandler> handlers;

    void pushHandler(uint32_t offset, const std::string& code);
    void popHandler();
    uint32_t findHandler(const std::string& code);
};
```

**Implementation Steps:**
1. Add `ExceptionStack` to `Executor` class
2. Implement RAISE opcode execution
3. Implement exception propagation up call stack
4. Implement TRY block entry (push exception handler)
5. Implement EXCEPT block exit (pop exception handler)
6. Add exception variable (%SQLCODE, %SQLERRM equivalent)

**Testing:**
- Simple exception raising
- Exception catching
- Exception re-raising
- Nested exception handlers
- Unhandled exceptions

---

### Task 5: Trigger Firing Mechanism

**File:** `src/sblr/executor.cpp`
**Estimated Lines:** ~500

**Requirements:**
1. Trigger discovery (find triggers for table+operation)
2. Trigger ordering (BEFORE before AFTER)
3. Trigger execution (call trigger procedure)
4. OLD/NEW pseudo-records access
5. INSTEAD OF trigger support (for views)
6. FOR EACH ROW vs FOR EACH STATEMENT

**Data Structures:**
```cpp
struct TriggerContext {
    TriggerTiming timing;  // BEFORE, AFTER, INSTEAD OF
    TriggerEvent event;    // INSERT, UPDATE, DELETE
    TriggerLevel level;    // ROW, STATEMENT
    Value old_row;         // OLD pseudo-record
    Value new_row;         // NEW pseudo-record
    bool is_modified;      // Did trigger modify NEW?
};

class TriggerManager {
    std::vector<TriggerInfo> findTriggers(uint64_t table_id, TriggerEvent event);
    Status fireTriggers(const std::vector<TriggerInfo>& triggers,
                       TriggerTiming timing,
                       TriggerContext& context);
};
```

**Implementation Steps:**
1. Add `TriggerManager` to `Executor` class
2. Modify `executeInsert()` to call triggers (BEFORE, AFTER)
3. Modify `executeUpdate()` to call triggers (BEFORE, AFTER)
4. Modify `executeDelete()` to call triggers (BEFORE, AFTER)
5. Implement trigger procedure invocation
6. Implement OLD/NEW variable binding in trigger scope
7. Implement NEW modification support (UPDATE/INSERT triggers)
8. Implement INSTEAD OF triggers for views

**Trigger Firing Order:**
```
INSERT/UPDATE/DELETE operation:
1. BEFORE STATEMENT triggers
2. For each affected row:
   a. BEFORE ROW triggers
   b. Actual DML operation (or skip if INSTEAD OF)
   c. AFTER ROW triggers
3. AFTER STATEMENT triggers
```

**Testing:**
- BEFORE INSERT trigger modifying NEW
- AFTER INSERT trigger logging
- BEFORE UPDATE trigger validation
- AFTER DELETE trigger cascade
- Trigger ordering (multiple triggers on same event)
- INSTEAD OF triggers on views
- Trigger error propagation

---

### Task 6: Stored Procedure Invocation

**File:** `src/sblr/executor.cpp`
**Estimated Lines:** ~300

**Requirements:**
1. Procedure/function lookup by name
2. Parameter binding (IN, OUT, INOUT)
3. Procedure bytecode execution
4. Return value handling
5. OUT parameter return
6. SQL SECURITY DEFINER/INVOKER context switching

**Implementation Steps:**
1. Implement `callProcedure()` opcode handler
2. Load procedure bytecode from catalog
3. Create parameter scope
4. Bind IN parameters
5. Execute procedure bytecode
6. Extract OUT parameters
7. Return function result
8. Implement security context switching

**Testing:**
- Simple function call
- Procedure with OUT parameters
- Procedure with INOUT parameters
- Recursive procedure calls
- SQL SECURITY DEFINER procedures
- Error handling in procedures

---

## Integration Points

### Parser Integration
- **Status:** ✅ Complete
- **Files:** `src/parser/parser.cpp`
- **No changes needed**

### Bytecode Generator Integration
- **Status:** ✅ Complete
- **Files:** `src/sblr/bytecode_generator.cpp`
- **No changes needed**

### Catalog Integration
- **Status:** ✅ Complete
- **Files:** `src/core/catalog_manager.cpp`
- **No changes needed**

### Executor Integration
- **Status:** ❌ Needs implementation
- **Files:** `src/sblr/executor.cpp`
- **Changes:**
  - Add variable scope stack
  - Add cursor manager
  - Add exception stack
  - Add trigger manager
  - Implement PSQL opcodes
  - Integrate trigger firing into DML operations

---

## Testing Strategy

### Unit Tests
**Directory:** `tests/unit/`

1. `test_psql_variables.cpp` - Variable scope and assignment
2. `test_psql_control_flow.cpp` - IF, LOOP, WHILE, EXIT, RETURN
3. `test_psql_cursors.cpp` - Cursor operations
4. `test_psql_exceptions.cpp` - Exception handling
5. `test_trigger_firing.cpp` - Trigger execution logic

### Integration Tests
**Directory:** `tests/integration/`

1. `test_stored_procedure_execution.cpp` - End-to-end procedure calls
2. `test_trigger_integration.cpp` - Triggers with DML operations
3. `test_psql_edge_cases.cpp` - Complex PSQL scenarios

### Test Coverage Goals
- ✅ 100% of PSQL control flow paths
- ✅ 100% of cursor states
- ✅ 100% of exception scenarios
- ✅ 100% of trigger types (BEFORE/AFTER, ROW/STATEMENT, INSERT/UPDATE/DELETE)

---

## Completion Criteria

### Code Complete
- [  ] All 6 tasks implemented
- [  ] All unit tests passing
- [  ] All integration tests passing
- [  ] No memory leaks (Valgrind clean)
- [  ] Thread-safe (if required)

### Documentation Complete
- [  ] PSQL language reference updated
- [  ] Trigger documentation updated
- [  ] Code comments complete
- [  ] Examples added

### Performance
- [  ] Procedure call overhead < 10 μs
- [  ] Trigger firing overhead < 5 μs per trigger
- [  ] Cursor iteration overhead < 1 μs per row

---

## Estimated Effort

**Total Estimated Lines:** ~1,950 lines
**Estimated Time:** 60-80 hours
**Priority:** HIGH (blocking Alpha 1 completion)

**Breakdown:**
- Task 1 (Variables): 15 hours
- Task 2 (Control Flow): 20 hours
- Task 3 (Cursors): 25 hours
- Task 4 (Exceptions): 15 hours
- Task 5 (Triggers): 30 hours
- Task 6 (Procedures): 20 hours
- Testing: 20 hours

---

## Dependencies

**Blocked By:** None
**Blocks:** Alpha 1 completion

---

## Notes

- Bytecode execution is already ~90% stubbed, so integration should be straightforward
- Trigger support is critical for many database applications
- PSQL is a core feature for stored business logic
- This is one of the largest remaining components in Alpha 1

---

**Last Updated:** November 21, 2025
**Next Review:** After Task 1 completion
