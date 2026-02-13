# Alpha 1: Advanced SQL Features Implementation Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Created:** November 21, 2025
**Status:** Not Started (~20% of Alpha 1 remaining)
**Priority:** HIGH
**Dependencies:** None

---

## Overview

Implement critical advanced SQL features missing from Alpha 1:
1. Common Table Expressions (CTEs) with recursive support
2. MERGE statement (complex upsert)
3. RETURNING clause for INSERT/UPDATE/DELETE
4. SAVEPOINT for nested transaction control

---

## Feature 1: Common Table Expressions (CTEs)

### Current Status

**Parser Layer:**
- ✅ WITH clause syntax (`parser.cpp:2487`)
- ✅ Multiple CTEs support
- ✅ Recursive CTE syntax

**Bytecode Layer:**
- ✅ CTE opcodes defined
- ✅ Recursive depth tracking

**Executor Layer:**
- ⧗ Basic execution infrastructure (`executor.cpp:551-663`)
- ❌ Recursive CTE execution incomplete

### What's Missing

1. **CTE Materialization:**
   - Temporary result set storage
   - CTE result caching (avoid re-execution)
   - Memory management for large CTEs

2. **Recursive CTE Execution:**
   - Initial query execution
   - Recursive iteration until no new rows
   - Cycle detection
   - Depth limiting

3. **CTE Visibility:**
   - Scope management (CTE only visible in main query)
   - Multiple CTE references
   - CTE-to-CTE references

### Implementation Tasks

#### Task 1.1: CTE Materialization Engine

**File:** `src/sblr/executor.cpp`
**Estimated Lines:** ~200

```cpp
struct CTEResult {
    std::string cte_name;
    std::vector<std::string> column_names;
    std::vector<std::vector<Value>> rows;
    bool is_materialized;
    bool is_recursive;
};

class CTEManager {
    std::unordered_map<std::string, CTEResult> cte_cache;

    void materializeCTE(const std::string& name, const ASTNode* query, TransactionId xid);
    const CTEResult* getCTE(const std::string& name);
    void clearCTEs();
};
```

**Steps:**
1. Add `CTEManager` to `Executor` class
2. Implement CTE query execution
3. Store result set in memory
4. Implement column name mapping
5. Implement CTE lookup during main query execution

**Testing:**
- Simple CTE with single query
- Multiple CTEs
- CTE with complex query
- Large CTE result set (memory stress test)

---

#### Task 1.2: Recursive CTE Execution

**File:** `src/sblr/executor.cpp`
**Estimated Lines:** ~300

**Algorithm:**
```
1. Execute initial query (non-recursive part)
2. Store results in working table
3. LOOP:
   a. Execute recursive query with working table as input
   b. If no new rows returned, EXIT
   c. Add new rows to working table
   d. Check cycle detection
   e. Check depth limit
4. Return final working table
```

**Implementation:**
```cpp
Status executeRecursiveCTE(const std::string& cte_name,
                           const ASTNode* initial_query,
                           const ASTNode* recursive_query,
                           uint32_t max_depth,
                           CTEResult& result) {
    // Execute initial query
    std::vector<std::vector<Value>> working_table;
    execute(initial_query, working_table);

    std::unordered_set<std::string> seen_rows;  // Cycle detection
    uint32_t depth = 0;

    while (depth < max_depth) {
        // Execute recursive part with working_table as input
        std::vector<std::vector<Value>> new_rows;
        executeWithCTE(recursive_query, cte_name, working_table, new_rows);

        if (new_rows.empty()) break;  // No new rows

        // Check for cycles
        for (const auto& row : new_rows) {
            std::string row_hash = hashRow(row);
            if (seen_rows.contains(row_hash)) {
                return error("Recursive CTE cycle detected");
            }
            seen_rows.insert(row_hash);
            working_table.push_back(row);
        }

        depth++;
    }

    if (depth >= max_depth) {
        return error("Recursive CTE depth limit exceeded");
    }

    result.rows = std::move(working_table);
    return Status::OK;
}
```

**Testing:**
- Simple recursive query (parent-child hierarchy)
- Recursive CTE with multiple levels
- Cycle detection
- Depth limit enforcement
- Complex recursive graph traversal

---

#### Task 1.3: CTE Scope Management

**File:** `src/sblr/executor.cpp`
**Estimated Lines:** ~100

**Steps:**
1. Push CTE scope on WITH clause entry
2. Pop CTE scope on query completion
3. Implement CTE name resolution
4. Support CTE-to-CTE references (WITH a AS (...), b AS (SELECT * FROM a))

**Testing:**
- CTE visibility in main query
- CTE not visible outside main query
- CTE-to-CTE references
- Nested WITH clauses

---

### Feature 1 Completion Criteria

- [  ] Non-recursive CTEs working
- [  ] Recursive CTEs with depth limiting
- [  ] Cycle detection functional
- [  ] Memory management efficient (no leaks)
- [  ] All CTE tests passing

---

## Feature 2: MERGE Statement

### Specification

**Syntax:**
```sql
MERGE INTO target_table USING source_table
ON join_condition
WHEN MATCHED THEN
    UPDATE SET column = value [, ...]
WHEN NOT MATCHED THEN
    INSERT (columns) VALUES (values)
WHEN NOT MATCHED BY SOURCE THEN
    DELETE
```

### Current Status

- ❌ No parser support (`00_GRAMMAR_BNF.md:461-480`)
- ❌ No bytecode support
- ❌ No executor support

### Implementation Tasks

#### Task 2.1: Parser Extension

**File:** `src/parser/parser.cpp`
**Estimated Lines:** ~200

**Steps:**
1. Add MERGE keyword to lexer
2. Implement `parseMergeStatement()` function
3. Parse USING clause (source table or subquery)
4. Parse ON condition
5. Parse WHEN clauses (MATCHED, NOT MATCHED, NOT MATCHED BY SOURCE)
6. Create MergeStmt AST node

**Testing:**
- Simple MERGE with UPDATE and INSERT
- MERGE with all three WHEN clauses
- MERGE with complex ON condition
- MERGE with subquery as source

---

#### Task 2.2: Bytecode Generation

**File:** `src/sblr/bytecode_generator.cpp`
**Estimated Lines:** ~250

**Opcodes:**
- `MERGE_START` - Begin merge operation
- `MERGE_MATCH_UPDATE` - Update matched rows
- `MERGE_NOMATCH_INSERT` - Insert non-matched rows
- `MERGE_NOMATCH_SOURCE_DELETE` - Delete not matched by source
- `MERGE_END` - Complete merge operation

**Steps:**
1. Define MERGE opcodes
2. Generate bytecode for ON condition
3. Generate bytecode for each WHEN clause
4. Optimize for simple cases (single WHEN clause)

**Testing:**
- Bytecode correctness verification
- Bytecode size optimization

---

#### Task 2.3: Executor Implementation

**File:** `src/sblr/executor.cpp`
**Estimated Lines:** ~400

**Algorithm:**
```
1. Execute source query, get source rows
2. For each source row:
   a. Evaluate join condition against target table
   b. If matched:
      - Execute UPDATE (if WHEN MATCHED clause exists)
   c. If not matched:
      - Execute INSERT (if WHEN NOT MATCHED clause exists)
3. For each target row not matched by source:
   a. Execute DELETE (if WHEN NOT MATCHED BY SOURCE clause exists)
```

**Implementation:**
```cpp
Status executeMerge(const MergeStmt* stmt, TransactionId xid) {
    // 1. Get source rows
    std::vector<std::vector<Value>> source_rows;
    executeQuery(stmt->using_clause, source_rows);

    // Track matched target rows
    std::unordered_set<TID> matched_tids;

    // 2. Process each source row
    for (const auto& source_row : source_rows) {
        // Find matching target row(s)
        std::vector<TID> matching_tids;
        findMatchingRows(stmt->on_condition, source_row, matching_tids);

        if (!matching_tids.empty()) {
            // WHEN MATCHED
            if (stmt->when_matched_update) {
                for (TID tid : matching_tids) {
                    executeUpdate(tid, stmt->update_values, xid);
                    matched_tids.insert(tid);
                }
            }
        } else {
            // WHEN NOT MATCHED
            if (stmt->when_not_matched_insert) {
                executeInsert(stmt->insert_values, xid);
            }
        }
    }

    // 3. WHEN NOT MATCHED BY SOURCE
    if (stmt->when_not_matched_by_source_delete) {
        for (TID tid : getAllTargetRows()) {
            if (!matched_tids.contains(tid)) {
                executeDelete(tid, xid);
            }
        }
    }

    return Status::OK;
}
```

**Testing:**
- Simple upsert (UPDATE or INSERT)
- MERGE with DELETE
- Complex ON condition
- Multiple matched rows handling
- Constraint enforcement during MERGE

---

### Feature 2 Completion Criteria

- [  ] Parser supports full MERGE syntax
- [  ] Bytecode generation complete
- [  ] Executor correctly handles all WHEN clauses
- [  ] Constraint enforcement working
- [  ] Trigger firing for MERGE operations
- [  ] All MERGE tests passing

---

## Feature 3: RETURNING Clause

### Specification

**Syntax:**
```sql
INSERT INTO table (columns) VALUES (values) RETURNING *;
INSERT INTO table (columns) VALUES (values) RETURNING id, name;
UPDATE table SET column = value WHERE condition RETURNING *;
DELETE FROM table WHERE condition RETURNING *;
```

### Current Status

- ❌ No parser support
- ❌ No bytecode support
- ❌ No executor support

### Implementation Tasks

#### Task 3.1: Parser Extension

**File:** `src/parser/parser.cpp`
**Estimated Lines:** ~100

**Steps:**
1. Add RETURNING keyword to lexer
2. Extend `parseInsertStatement()` to accept RETURNING clause
3. Extend `parseUpdateStatement()` to accept RETURNING clause
4. Extend `parseDeleteStatement()` to accept RETURNING clause
5. Store RETURNING column list in AST node

**Testing:**
- INSERT ... RETURNING *
- INSERT ... RETURNING specific columns
- UPDATE ... RETURNING
- DELETE ... RETURNING

---

#### Task 3.2: Executor Implementation

**File:** `src/sblr/executor.cpp`
**Estimated Lines:** ~200

**Implementation:**
```cpp
Status executeInsertReturning(const InsertStmt* stmt,
                               std::vector<std::vector<Value>>& result_rows) {
    for (const auto& row : stmt->values) {
        // Execute insert
        TID inserted_tid = insertTuple(row, xid);

        // Fetch inserted row (to get default values, generated columns, etc.)
        Record* inserted_record = fetch_record(inserted_tid);

        // Build RETURNING result
        std::vector<Value> returning_row;
        for (const auto& col : stmt->returning_columns) {
            if (col == "*") {
                // Return all columns
                returning_row = getAllColumns(inserted_record);
            } else {
                // Return specific column
                returning_row.push_back(getColumn(inserted_record, col));
            }
        }

        result_rows.push_back(returning_row);
    }

    return Status::OK;
}
```

**Testing:**
- INSERT ... RETURNING with auto-increment column
- INSERT ... RETURNING with DEFAULT values
- UPDATE ... RETURNING showing old and new values
- DELETE ... RETURNING deleted rows
- RETURNING with expressions

---

### Feature 3 Completion Criteria

- [  ] Parser supports RETURNING for INSERT/UPDATE/DELETE
- [  ] Executor returns correct result rows
- [  ] Result set format matches SELECT
- [  ] All RETURNING tests passing

---

## Feature 4: SAVEPOINT (Nested Transaction Control)

### Specification

**Syntax:**
```sql
SAVEPOINT savepoint_name;
ROLLBACK TO SAVEPOINT savepoint_name;
RELEASE SAVEPOINT savepoint_name;
```

### Current Status

- ❌ No parser support
- ❌ No bytecode support
- ❌ No executor support
- ❌ **CRITICAL GAP** for nested transaction control

### Why This Is Critical

- Partial rollback in complex operations
- Error recovery in stored procedures
- Nested business logic transactions
- Required for many database applications

### Implementation Tasks

#### Task 4.1: Transaction Manager Extension

**File:** `src/core/transaction_manager.cpp`
**Estimated Lines:** ~300

**Data Structures:**
```cpp
struct Savepoint {
    std::string name;
    TransactionId xid;
    uint64_t undo_log_position;  // Position in undo log
    std::vector<std::pair<TID, Record>> modified_records;  // Records modified since savepoint
};

class SavepointManager {
    std::vector<Savepoint> savepoint_stack;

    void createSavepoint(const std::string& name, TransactionId xid);
    void rollbackToSavepoint(const std::string& name);
    void releaseSavepoint(const std::string& name);
};
```

**Steps:**
1. Add `SavepointManager` to `TransactionManager`
2. Track modified records after each savepoint
3. Implement `createSavepoint()` - capture current state
4. Implement `rollbackToSavepoint()` - restore to saved state
5. Implement `releaseSavepoint()` - discard savepoint

**Algorithm for Rollback:**
```
1. Find savepoint by name
2. For each modified record since savepoint:
   a. Restore old version
   b. Mark new version as invalid
3. Update transaction undo log position
4. Pop savepoints created after this one
```

**Testing:**
- Simple savepoint creation and release
- Rollback to savepoint
- Nested savepoints
- Rollback to middle savepoint
- Savepoint across multiple tables

---

#### Task 4.2: Parser Extension

**File:** `src/parser/parser.cpp`
**Estimated Lines:** ~80

**Steps:**
1. Add SAVEPOINT keyword
2. Implement `parseSavepointStatement()`
3. Implement `parseRollbackToSavepoint()`
4. Implement `parseReleaseSavepoint()`

**Testing:**
- SAVEPOINT name_validation
- ROLLBACK TO SAVEPOINT syntax
- RELEASE SAVEPOINT syntax

---

#### Task 4.3: Executor Integration

**File:** `src/sblr/executor.cpp`
**Estimated Lines:** ~150

**Steps:**
1. Implement `executeSavepoint()` opcode
2. Implement `executeRollbackToSavepoint()` opcode
3. Implement `executeReleaseSavepoint()` opcode
4. Integrate with constraint checking (re-validate after rollback)
5. Integrate with triggers (rollback trigger effects)

**Testing:**
- SAVEPOINT in stored procedure
- Nested SAVEPOINTs with rollback
- Constraint violation with rollback to savepoint
- Trigger firing with savepoint rollback

---

### Feature 4 Completion Criteria

- [  ] SAVEPOINT, ROLLBACK TO, RELEASE all working
- [  ] Nested savepoints supported
- [  ] Constraint re-validation after rollback
- [  ] Trigger effects properly rolled back
- [  ] All savepoint tests passing
- [  ] No memory leaks

---

## Integration Testing

### Combined Feature Tests

**Directory:** `tests/integration/`

1. `test_cte_with_merge.cpp` - CTEs used in MERGE operations
2. `test_returning_with_triggers.cpp` - RETURNING with trigger effects
3. `test_savepoint_in_procedures.cpp` - SAVEPOINTs in stored procedures
4. `test_complex_sql_scenarios.cpp` - All features combined

---

## Completion Criteria

### All Features Complete

- [  ] CTEs (non-recursive and recursive) working
- [  ] MERGE statement fully functional
- [  ] RETURNING clause for INSERT/UPDATE/DELETE
- [  ] SAVEPOINT for nested transactions
- [  ] All unit tests passing (100% coverage)
- [  ] All integration tests passing
- [  ] No memory leaks
- [  ] Performance meets targets

### Documentation

- [  ] SQL reference updated with CTEs
- [  ] SQL reference updated with MERGE
- [  ] SQL reference updated with RETURNING
- [  ] SQL reference updated with SAVEPOINT
- [  ] Examples for all features

---

## Estimated Effort

**Total Estimated Lines:** ~2,580 lines
**Estimated Time:** 80-100 hours
**Priority:** HIGH (blocking Alpha 1 completion)

**Breakdown:**
- Feature 1 (CTEs): 30 hours
- Feature 2 (MERGE): 30 hours
- Feature 3 (RETURNING): 15 hours
- Feature 4 (SAVEPOINT): 25 hours
- Integration Testing: 20 hours

---

## Dependencies

**Blocked By:** None
**Blocks:** Alpha 1 completion

---

## Notes

- SAVEPOINT is the most critical missing feature (required for nested transaction control)
- CTEs with recursive support are complex but well-specified
- MERGE can build on existing INSERT/UPDATE/DELETE infrastructure
- RETURNING is relatively straightforward, builds on existing result set logic

---

**Last Updated:** November 21, 2025
**Next Review:** After Feature 1 (CTEs) completion
