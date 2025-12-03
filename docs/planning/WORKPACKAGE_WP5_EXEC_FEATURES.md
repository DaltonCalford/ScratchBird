# Work Package 5: SQL Executor - Features

**Status:** IN PROGRESS (1/16 tasks complete)
**Priority:** P0-P2 Mixed
**Estimated Hours:** 28-36
**File:** src/sblr/executor.cpp

---

## Overview

Core SQL execution features including GENERATED columns, GiST indexes, window functions, and various SQL commands have incomplete implementations.

---

## Tasks

### EXEC-9: GENERATED columns evaluation (HIGH)
**Lines:** 4678-4694
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Phase 3 Enhancement: Deserialize generation_expression bytecode and evaluate against current row values
// For now, insert NULL as placeholder for GENERATED columns
values.push_back(TypedValue::makeNull());
```

**Required Changes:**
1. Deserialize generation_expression from hex string
2. Create ExpressionEvaluator with current row context
3. Evaluate expression bytecode
4. Use result instead of NULL

**Algorithm:**
```cpp
if (!col.generation_expression.empty()) {
    auto bytecode = deserializeBytecode(col.generation_expression);
    ExpressionEvaluator eval(current_row_values);
    TypedValue result = eval.evaluate(bytecode);
    values.push_back(result);
}
```

**Verification:**
- [ ] CREATE TABLE t (a INT, b INT GENERATED ALWAYS AS (a * 2) STORED)
- [ ] INSERT INTO t (a) VALUES (5) produces b = 10

---

### EXEC-10: Complex DEFAULT expressions (HIGH)
**Lines:** 23223-23255
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Phase 3 Enhancement: Evaluate default_expr bytecode for complex defaults (NOW(), RANDOM(), etc.)
// Currently only handles literals
```

**Required Changes:**
1. If default is bytecode (not literal), evaluate it
2. Support common functions: NOW(), CURRENT_TIMESTAMP, RANDOM(), etc.

**Verification:**
- [ ] Column with DEFAULT NOW() gets current timestamp
- [ ] Column with DEFAULT RANDOM() gets random value

---

### EXEC-11: GiST operations (HIGH)
**Lines:** 25619-26135
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// All operations return NOT_SUPPORTED
return Status::NOT_SUPPORTED;
```

**Options:**
1. **Full implementation:** Complete GiST index integration
2. **Remove GiST:** Remove from index type enum if not supporting

**If implementing:**
1. Wire to gist_index.cpp operations
2. Handle GiST-specific predicates
3. Support spatial and other GiST use cases

**Verification:**
- [ ] CREATE INDEX ... USING GIST works
- [ ] Queries use GiST index

---

### EXEC-12: Window function arguments (HIGH)
**Lines:** 8776-8781
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
for (uint32_t a = 0; a < arg_count; a++) {
    // Skip argument expressions for now
    error("Window function argument parsing not fully implemented");
}
```

**Required Changes:**
1. Parse argument expressions from bytecode
2. Store in WindowSpec structure
3. Evaluate arguments during window function execution

**Verification:**
- [ ] NTH_VALUE(col, 2) returns 2nd value
- [ ] LEAD(col, 3) returns value 3 rows ahead

---

### EXEC-13: NTH_VALUE evaluation (HIGH)
**Lines:** 9003-9009
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// TODO: Implement once window function argument parsing is available
return TypedValue::makeNull();  // Placeholder
```

**Required Changes:**
1. Get nth argument from window spec
2. Return nth value from window frame

**Depends on:** EXEC-12

**Verification:**
- [ ] NTH_VALUE(col, 1) returns first value
- [ ] NTH_VALUE(col, 5) returns fifth value or NULL

---

### EXEC-14: Aggregates in scalar context (HIGH)
**Lines:** 11361-11362
**Status:** [x] COMPLETE (December 3, 2025)

**Implementation:**
Added `executeScalarAggregate()` helper function to executor that:
1. Detects aggregate opcodes (AGG_SUM, AGG_MAX, AGG_MIN, AGG_AVG, AGG_COUNT, ARRAY_AGG) in scalar expression context
2. Creates an AggregateAccumulator with appropriate function type
3. Iterates over current table rows using catalog manager to get table/column info
4. Accumulates values and returns finalized scalar result

**Files Modified:**
- `include/scratchbird/sblr/executor.h` - Added `executeScalarAggregate(uint8_t func_type, size_t arg_expr_pc)` declaration
- `src/sblr/executor.cpp` - Added implementation (~100 lines)

**Key Code:**
```cpp
// EXEC-14: Scalar aggregate execution helper
Value Executor::executeScalarAggregate(uint8_t func_type, size_t arg_expr_pc) {
    AggregateAccumulator::AggFunc func;
    switch (func_type) {
        case 0: func = AggregateAccumulator::AggFunc::COUNT; break;
        case 1: func = AggregateAccumulator::AggFunc::SUM; break;
        case 2: func = AggregateAccumulator::AggFunc::AVG; break;
        case 3: func = AggregateAccumulator::AggFunc::MIN; break;
        case 4: func = AggregateAccumulator::AggFunc::MAX; break;
        case 5: func = AggregateAccumulator::AggFunc::ARRAY_AGG; break;
        default: func = AggregateAccumulator::AggFunc::COUNT;
    }
    AggregateAccumulator acc(func);
    // ... iterate rows and accumulate
    return acc.finalize();
}
```

**Verification:**
- [x] SELECT (SELECT MAX(x) FROM t) works
- [x] Build passes
- [x] Related aggregate tests pass (SQLToBytecodeTest: 13/13, Aggregate tests: 3/3)

---

### EXEC-M3: SET/RESET SESSION AUTHORIZATION (MEDIUM)
**Lines:** 20530, 20536
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
error("SET SESSION AUTHORIZATION not yet implemented (requires session user tracking)");
error("RESET SESSION AUTHORIZATION not yet implemented (requires session user tracking)");
```

**Required Changes:**
1. Add original_user_id to ConnectionContext
2. SET SESSION AUTHORIZATION changes effective_user_id
3. RESET restores to original_user_id

**Verification:**
- [ ] SET SESSION AUTHORIZATION 'user' changes context
- [ ] RESET SESSION AUTHORIZATION restores original

---

### EXEC-M4: SET CONSTRAINTS named (MEDIUM)
**Lines:** 20570-20579
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
error("SET CONSTRAINTS with named constraints not yet fully implemented. "
      "Use 'SET CONSTRAINTS ALL DEFERRED' or 'SET CONSTRAINTS ALL IMMEDIATE' for now.");
```

**Required Changes:**
1. Build constraint name index or use qualified names
2. Look up constraint by name
3. Set deferred status for specific constraints

**Verification:**
- [ ] SET CONSTRAINTS fk_name DEFERRED works

---

### EXEC-M5: REVOKE CASCADE (MEDIUM)
**Lines:** 20301, 20426
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Phase 2 Enhancement: CASCADE option (revoke from grantees of this grantee)
```

**Required Changes:**
1. Track grant chains in catalog
2. On CASCADE, revoke from all downstream grantees

**Verification:**
- [ ] REVOKE ... CASCADE removes transitive grants

---

### EXEC-M6: TOAST CHECK expressions (MEDIUM)
**Lines:** 22227-22233
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
error("CHECK constraint ... uses TOAST storage which is not yet supported.");
```

**Required Changes:**
1. Construct ToastPointer from check_expr_oid
2. Call detoastValue() to retrieve bytecode
3. Evaluate bytecode

**Verification:**
- [ ] Large CHECK expressions work correctly

---

### EXEC-M7: Object type lookup (MEDIUM)
**Lines:** 20050, 20229
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
error("Object type not yet supported: " + std::to_string(object_type_byte));
```

**Required Changes:**
Support GRANT/REVOKE on:
- SEQUENCE
- FUNCTION
- PROCEDURE
- SCHEMA
- DATABASE

**Verification:**
- [ ] GRANT SELECT ON SEQUENCE seq TO user works

---

### EXEC-M8: PARTITION BY expressions (MEDIUM)
**Lines:** 8799-8804
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
spec.partition_cols.push_back(p); // Placeholder
// TODO: Full expression support
```

**Required Changes:**
1. Parse partition expression (not just column reference)
2. Evaluate expression for each row
3. Group by expression result

**Verification:**
- [ ] PARTITION BY (col1 + col2) works

---

### EXEC-M9/L6: DISTINCT for 2-variable aggregates (MEDIUM/LOW)
**Line:** 6976
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Phase 4 Enhancement: Handle DISTINCT for 2-variable functions if needed
```

**Required Changes:**
Handle DISTINCT for CORR, COVAR_SAMP, COVAR_POP, REGR_* functions.

**Verification:**
- [ ] CORR(DISTINCT x, y) works correctly

---

### EXEC-M10/L5: Schema-qualified GRANT names (MEDIUM/LOW)
**Lines:** 20002, 20181
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// Note: Schema-qualified name handling (schema.table) requires parser updates (Phase 2 Enhancement)
```

**Required Changes:**
1. Parser: Accept schema.object syntax in GRANT/REVOKE
2. Executor: Resolve schema-qualified names

**Verification:**
- [ ] GRANT SELECT ON myschema.mytable TO user works

---

## Completion Checklist

- [ ] All 16 tasks implemented (1/16 complete)
- [x] All 1020 existing tests pass
- [ ] New tests for each feature
- [x] Code compiles without warnings

### Completed Tasks
- [x] EXEC-14: Aggregates in scalar context (December 3, 2025)

---

**Last Updated:** December 3, 2025
