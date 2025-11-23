# MERGE and RETURNING Implementation Plan

**Created:** November 22, 2025
**Status:** In Progress
**Priority:** HIGH (Final Advanced SQL Features for Alpha 1)

---

## Overview

This document tracks the implementation of the final two advanced SQL features required for Alpha 1 completion:
1. **MERGE Statement** - Complex upsert operations
2. **RETURNING Clause** - Return data from INSERT/UPDATE/DELETE operations

---

## Feature 1: MERGE Statement

### Specification

**SQL Syntax:**
```sql
MERGE INTO target_table
USING source_table
ON join_condition
WHEN MATCHED THEN
    UPDATE SET column = value [, ...]
WHEN NOT MATCHED THEN
    INSERT (columns) VALUES (values)
WHEN NOT MATCHED BY SOURCE THEN
    DELETE
```

###Step 1: Add Opcodes (opcodes.h)

**Location:** `/home/user/ScratchBird/include/scratchbird/sblr/opcodes.h`

**New Opcodes to Add (after EXT_XMLAGG at 0x4E):**

```cpp
// MERGE statement support (0x4F-0x53)
EXT_MERGE_START = 0x4F,           // Begin MERGE operation
EXT_MERGE_SOURCE = 0x50,          // Source query/table specification
EXT_MERGE_ON = 0x51,              // ON condition (join predicate)
EXT_MERGE_WHEN_MATCHED = 0x52,    // WHEN MATCHED THEN UPDATE
EXT_MERGE_WHEN_NOT_MATCHED = 0x53, // WHEN NOT MATCHED THEN INSERT
EXT_MERGE_WHEN_NOT_MATCHED_SOURCE = 0x54, // WHEN NOT MATCHED BY SOURCE THEN DELETE
EXT_MERGE_END = 0x55,             // End MERGE operation
```

**Estimated Lines:** ~10

---

### Step 2: Add AST Nodes

**Location:** `/home/user/ScratchBird/include/scratchbird/parser/ast.h`

**Add to ASTKind enum:**
```cpp
MERGE,  // MERGE statement
```

**Add MergeStmt class:**
```cpp
// MERGE statement (Alpha 1 - Advanced SQL)
class MergeStmt : public ASTNode
{
public:
    struct WhenClause
    {
        enum Type {
            MATCHED,              // WHEN MATCHED THEN UPDATE
            NOT_MATCHED,          // WHEN NOT MATCHED THEN INSERT
            NOT_MATCHED_BY_SOURCE // WHEN NOT MATCHED BY SOURCE THEN DELETE
        };

        Type type;
        Expression* condition;  // Optional additional condition

        // For UPDATE
        std::vector<std::pair<std::string, Expression*>> assignments;

        // For INSERT
        std::vector<std::string> insert_columns;
        std::vector<Expression*> insert_values;
    };

    MergeStmt(const SourceSpan& span,
              const std::string& target_table,
              Expression* source,  // Can be table or subquery
              Expression* on_condition,
              const std::vector<WhenClause>& when_clauses)
        : ASTNode(ASTKind::MERGE, span),
          target_table_(target_table),
          source_(source),
          on_condition_(on_condition),
          when_clauses_(when_clauses)
    {
    }

    const std::string& targetTable() const { return target_table_; }
    Expression* source() const { return source_; }
    Expression* onCondition() const { return on_condition_; }
    const std::vector<WhenClause>& whenClauses() const { return when_clauses_; }

    void accept(ASTVisitor* visitor) override;

private:
    std::string target_table_;
    Expression* source_;
    Expression* on_condition_;
    std::vector<WhenClause> when_clauses_;
};
```

**Estimated Lines:** ~50

---

### Step 3: Parser Implementation

**Location:** `/home/user/ScratchBird/src/parser/parser.cpp`

**Add parseMergeStatement() function:**

```cpp
ASTNode* Parser::parseMergeStatement()
{
    // MERGE INTO target_table
    // USING source
    // ON condition
    // WHEN clauses...

    auto start_span = current_token_.span;

    // MERGE keyword already consumed
    expect(TokenType::INTO);

    // Parse target table
    auto target = expect(TokenType::IDENTIFIER);
    std::string target_table = target.lexeme;

    // USING clause
    expect(TokenType::USING);
    Expression* source = parseTableReference();  // Can be table name or subquery

    // ON clause
    expect(TokenType::ON);
    Expression* on_condition = parseExpression();

    // WHEN clauses
    std::vector<MergeStmt::WhenClause> when_clauses;

    while (current_token_.type == TokenType::WHEN) {
        advance();  // consume WHEN

        MergeStmt::WhenClause clause;

        if (matchKeyword("MATCHED")) {
            clause.type = MergeStmt::WhenClause::MATCHED;
            expect(TokenType::THEN);
            expect(TokenType::UPDATE);
            expect(TokenType::SET);

            // Parse UPDATE assignments
            do {
                auto col = expect(TokenType::IDENTIFIER);
                expect(TokenType::EQUAL);
                Expression* value = parseExpression();
                clause.assignments.push_back({col.lexeme, value});
            } while (match(TokenType::COMMA));

        } else if (matchKeyword("NOT")) {
            advance();
            expect(TokenType::MATCHED);

            if (matchKeyword("BY")) {
                advance();
                expect(TokenType::SOURCE);
                clause.type = MergeStmt::WhenClause::NOT_MATCHED_BY_SOURCE;
                expect(TokenType::THEN);
                expect(TokenType::DELETE);
            } else {
                clause.type = MergeStmt::WhenClause::NOT_MATCHED;
                expect(TokenType::THEN);
                expect(TokenType::INSERT);

                // Parse optional column list
                if (match(TokenType::LEFT_PAREN)) {
                    do {
                        auto col = expect(TokenType::IDENTIFIER);
                        clause.insert_columns.push_back(col.lexeme);
                    } while (match(TokenType::COMMA));
                    expect(TokenType::RIGHT_PAREN);
                }

                expect(TokenType::VALUES);
                expect(TokenType::LEFT_PAREN);

                // Parse values
                do {
                    clause.insert_values.push_back(parseExpression());
                } while (match(TokenType::COMMA));

                expect(TokenType::RIGHT_PAREN);
            }
        }

        when_clauses.push_back(clause);
    }

    auto end_span = previous_token_.span;

    return arena_->make<MergeStmt>(
        SourceSpan(start_span.start, end_span.end),
        target_table,
        source,
        on_condition,
        when_clauses
    );
}
```

**Estimated Lines:** ~100

---

### Step 4: Bytecode Generation

**Location:** `/home/user/ScratchBird/src/sblr/bytecode_generator.cpp`

**Add generateMerge() function:**

```cpp
void BytecodeGenerator::generateMerge(const MergeStmt* stmt)
{
    // Emit MERGE_START
    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_MERGE_START));

    // Emit target table name
    writeString(stmt->targetTable());

    // Emit MERGE_SOURCE
    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_MERGE_SOURCE));
    generateExpression(stmt->source());

    // Emit MERGE_ON
    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_MERGE_ON));
    generateExpression(stmt->onCondition());

    // Emit WHEN clauses
    for (const auto& when_clause : stmt->whenClauses()) {
        switch (when_clause.type) {
            case MergeStmt::WhenClause::MATCHED:
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_MERGE_WHEN_MATCHED));

                // Write number of assignments
                writeInt32(when_clause.assignments.size());

                // Write each assignment (column_name + value_expr)
                for (const auto& [col, val] : when_clause.assignments) {
                    writeString(col);
                    generateExpression(val);
                }
                break;

            case MergeStmt::WhenClause::NOT_MATCHED:
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_MERGE_WHEN_NOT_MATCHED));

                // Write column count
                writeInt32(when_clause.insert_columns.size());

                // Write column names
                for (const auto& col : when_clause.insert_columns) {
                    writeString(col);
                }

                // Write values
                for (const auto& val : when_clause.insert_values) {
                    generateExpression(val);
                }
                break;

            case MergeStmt::WhenClause::NOT_MATCHED_BY_SOURCE:
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
                current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_MERGE_WHEN_NOT_MATCHED_SOURCE));
                break;
        }
    }

    // Emit MERGE_END
    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_MERGE_END));
}
```

**Estimated Lines:** ~70

---

### Step 5: Executor Implementation

**Location:** `/home/user/ScratchBird/src/sblr/executor.cpp`

**Add executeMerge() function:**

```cpp
Status Executor::executeMerge(TransactionId xid, ErrorContext* ctx)
{
    // Read target table name
    std::string target_table = readString();

    // Get table metadata
    Table* target = getTable(target_table);
    if (!target) {
        return error("Table not found: " + target_table, ctx);
    }

    // Execute source query (next opcode is MERGE_SOURCE)
    std::vector<std::vector<Value>> source_rows;
    Status status = executeSourceQuery(source_rows, xid, ctx);
    if (status != Status::OK) return status;

    // Read ON condition (next opcode is MERGE_ON)
    // This will be bytecode for the join condition

    // Track which target rows were matched
    std::unordered_set<TID> matched_target_tids;

    // Process each source row
    for (const auto& source_row : source_rows) {
        // Evaluate ON condition to find matching target rows
        std::vector<TID> matching_tids;
        findMatchingTargetRows(target, source_row, matching_tids, xid, ctx);

        if (!matching_tids.empty()) {
            // WHEN MATCHED case
            for (TID tid : matching_tids) {
                // Execute UPDATE if WHEN MATCHED clause exists
                status = executeWhenMatched(target, tid, source_row, xid, ctx);
                if (status != Status::OK) return status;

                matched_target_tids.insert(tid);
            }
        } else {
            // WHEN NOT MATCHED case
            status = executeWhenNotMatched(target, source_row, xid, ctx);
            if (status != Status::OK) return status;
        }
    }

    // WHEN NOT MATCHED BY SOURCE case
    // Find all target rows not in matched_target_tids and delete them
    status = executeWhenNotMatchedBySource(target, matched_target_tids, xid, ctx);
    if (status != Status::OK) return status;

    return Status::OK;
}

Status Executor::executeWhenMatched(Table* target, TID tid,
                                     const std::vector<Value>& source_row,
                                     TransactionId xid, ErrorContext* ctx)
{
    // Read number of assignments
    uint32_t num_assignments = readInt32();

    // Build update record
    std::vector<std::pair<std::string, Value>> updates;

    for (uint32_t i = 0; i < num_assignments; ++i) {
        std::string col_name = readString();
        Value new_value = evaluateExpression(source_row, xid, ctx);
        updates.push_back({col_name, new_value});
    }

    // Perform the UPDATE (using existing updateTuple infrastructure)
    return updateTuple(target, tid, updates, xid, ctx);
}

Status Executor::executeWhenNotMatched(Table* target,
                                        const std::vector<Value>& source_row,
                                        TransactionId xid, ErrorContext* ctx)
{
    // Read column count
    uint32_t col_count = readInt32();

    // Read column names
    std::vector<std::string> col_names;
    for (uint32_t i = 0; i < col_count; ++i) {
        col_names.push_back(readString());
    }

    // Evaluate value expressions
    std::vector<Value> values;
    for (uint32_t i = 0; i < col_count; ++i) {
        values.push_back(evaluateExpression(source_row, xid, ctx));
    }

    // Perform the INSERT (using existing insertTuple infrastructure)
    return insertTuple(target, col_names, values, xid, ctx);
}

Status Executor::executeWhenNotMatchedBySource(Table* target,
                                                 const std::unordered_set<TID>& matched_tids,
                                                 TransactionId xid, ErrorContext* ctx)
{
    // Scan all target rows
    for (TID tid : getAllTargetRows(target, xid)) {
        if (matched_tids.find(tid) == matched_tids.end()) {
            // This row was not matched - delete it
            Status status = deleteTuple(target, tid, xid, ctx);
            if (status != Status::OK) return status;
        }
    }

    return Status::OK;
}
```

**Estimated Lines:** ~150

**Total for MERGE:** ~380 lines

---

## Feature 2: RETURNING Clause

### Specification

**SQL Syntax:**
```sql
INSERT INTO table (columns) VALUES (values) RETURNING *;
INSERT INTO table (columns) VALUES (values) RETURNING id, name;
UPDATE table SET column = value WHERE condition RETURNING *;
DELETE FROM table WHERE condition RETURNING *;
```

### Step 1: Add Opcodes

**Location:** `/home/user/ScratchBird/include/scratchbird/sblr/opcodes.h`

**New Opcode to Add:**

```cpp
// RETURNING clause support (0x56)
EXT_RETURNING = 0x56,  // RETURNING clause marker (followed by column list or *)
```

**Estimated Lines:** ~2

---

### Step 2: Modify Existing AST Nodes

**Location:** `/home/user/ScratchBird/include/scratchbird/parser/ast.h`

**Modify InsertStmt, UpdateStmt, DeleteStmt classes:**

Add to each:
```cpp
// Add to existing statement classes
private:
    std::vector<std::string> returning_columns_;  // Empty if no RETURNING, "*" for RETURNING *
    bool has_returning_;

public:
    const std::vector<std::string>& returningColumns() const { return returning_columns_; }
    bool hasReturning() const { return has_returning_; }
```

**Estimated Lines:** ~15 (modifications to 3 classes)

---

### Step 3: Parser Modifications

**Location:** `/home/user/ScratchBird/src/parser/parser.cpp`

**Modify parseInsertStatement(), parseUpdateStatement(), parseDeleteStatement():**

Add at the end of each function:
```cpp
// Check for RETURNING clause
std::vector<std::string> returning_columns;
bool has_returning = false;

if (matchKeyword("RETURNING")) {
    advance();
    has_returning = true;

    if (match(TokenType::STAR)) {
        returning_columns.push_back("*");
    } else {
        do {
            auto col = expect(TokenType::IDENTIFIER);
            returning_columns.push_back(col.lexeme);
        } while (match(TokenType::COMMA));
    }
}

// Pass to statement constructor
```

**Estimated Lines:** ~50 (modifications to 3 functions)

---

### Step 4: Bytecode Generation

**Location:** `/home/user/ScratchBird/src/sblr/bytecode_generator.cpp`

**Modify generateInsert(), generateUpdate(), generateDelete():**

Add at the end of each function:
```cpp
// Emit RETURNING clause if present
if (stmt->hasReturning()) {
    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
    current_result_->writeByte(static_cast<uint8_t>(Opcode::EXT_RETURNING));

    // Write column count
    writeInt32(stmt->returningColumns().size());

    // Write column names
    for (const auto& col : stmt->returningColumns()) {
        writeString(col);
    }
}
```

**Estimated Lines:** ~40 (modifications to 3 functions)

---

### Step 5: Executor Implementation

**Location:** `/home/user/ScratchBird/src/sblr/executor.cpp`

**Modify executeInsert(), executeUpdate(), executeDelete():**

Add handling for RETURNING:
```cpp
// After performing INSERT/UPDATE/DELETE operations, check for RETURNING
if (nextOpcode() == Opcode::EXT_RETURNING) {
    advance();  // consume EXT_RETURNING

    uint32_t col_count = readInt32();
    std::vector<std::string> ret_cols;

    for (uint32_t i = 0; i < col_count; ++i) {
        ret_cols.push_back(readString());
    }

    // Build result set from affected rows
    std::vector<std::vector<Value>> result_rows;

    for (TID tid : affected_tids) {
        // Fetch the tuple (may need to get from old version for DELETE)
        Record* rec = fetchRecord(tid);

        std::vector<Value> row;
        if (ret_cols[0] == "*") {
            // Return all columns
            row = getAllColumnValues(rec);
        } else {
            // Return specific columns
            for (const auto& col : ret_cols) {
                row.push_back(getColumnValue(rec, col));
            }
        }

        result_rows.push_back(row);
    }

    // Store result rows for output
    result_rows_ = result_rows;
    result_column_names_ = ret_cols;
}
```

**Estimated Lines:** ~100 (modifications to 3 functions)

**Total for RETURNING:** ~207 lines

---

## Implementation Order

1. **MERGE Statement:**
   1. Add opcodes to `opcodes.h` (10 lines)
   2. Add AST nodes to `ast.h` (50 lines)
   3. Add `accept()` implementation to `ast.cpp` (5 lines)
   4. Implement `parseMergeStatement()` in `parser.cpp` (100 lines)
   5. Implement `generateMerge()` in `bytecode_generator.cpp` (70 lines)
   6. Implement `executeMerge()` in `executor.cpp` (150 lines)
   7. Add test cases (100 lines)

2. **RETURNING Clause:**
   1. Add opcode to `opcodes.h` (2 lines)
   2. Modify AST nodes in `ast.h` (15 lines)
   3. Modify parser functions in `parser.cpp` (50 lines)
   4. Modify bytecode generator in `bytecode_generator.cpp` (40 lines)
   5. Modify executor functions in `executor.cpp` (100 lines)
   6. Add test cases (100 lines)

---

## Testing Strategy

### MERGE Tests

```sql
-- Test 1: Simple upsert
MERGE INTO inventory AS target
USING updates AS source
ON target.product_id = source.product_id
WHEN MATCHED THEN
    UPDATE SET quantity = source.quantity
WHEN NOT MATCHED THEN
    INSERT (product_id, quantity) VALUES (source.product_id, source.quantity);

-- Test 2: All three WHEN clauses
MERGE INTO products AS target
USING new_products AS source
ON target.id = source.id
WHEN MATCHED THEN
    UPDATE SET name = source.name, price = source.price
WHEN NOT MATCHED THEN
    INSERT (id, name, price) VALUES (source.id, source.name, source.price)
WHEN NOT MATCHED BY SOURCE THEN
    DELETE;
```

### RETURNING Tests

```sql
-- Test 1: INSERT RETURNING
INSERT INTO users (name, email)
VALUES ('Alice', 'alice@example.com')
RETURNING id, name;

-- Test 2: UPDATE RETURNING
UPDATE products
SET price = price * 1.1
WHERE category = 'Electronics'
RETURNING id, name, price;

-- Test 3: DELETE RETURNING
DELETE FROM sessions
WHERE expires_at < NOW()
RETURNING session_id, user_id;

-- Test 4: RETURNING *
INSERT INTO orders (user_id, total)
VALUES (1, 99.99)
RETURNING *;
```

---

## Completion Criteria

**MERGE:**
- [  ] Opcodes defined
- [  ] AST nodes implemented
- [  ] Parser handles all three WHEN clause types
- [  ] Bytecode generation complete
- [  ] Executor correctly matches and merges rows
- [  ] All test cases pass
- [  ] No memory leaks (Valgrind clean)

**RETURNING:**
- [  ] Opcode defined
- [  ] AST nodes modified
- [  ] Parser handles RETURNING in INSERT/UPDATE/DELETE
- [  ] Bytecode generation complete
- [  ] Executor returns correct result sets
- [  ] All test cases pass
- [  ] No memory leaks (Valgrind clean)

---

## Estimated Effort

**MERGE Statement:**
- Implementation: ~380 lines
- Testing: ~100 lines
- **Total: ~480 lines / ~15-20 hours**

**RETURNING Clause:**
- Implementation: ~207 lines
- Testing: ~100 lines
- **Total: ~307 lines / ~10-12 hours**

**Grand Total: ~787 lines / ~25-32 hours**

---

**Last Updated:** November 22, 2025
**Status:** Planning Complete, Implementation Starting
