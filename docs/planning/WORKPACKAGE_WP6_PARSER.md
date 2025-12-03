# Work Package 6: Parser & Bytecode Generator

**Status:** NOT STARTED
**Priority:** P0-P1 Mixed
**Estimated Hours:** 18-26
**Files:** src/parser/parser.cpp, src/sblr/bytecode_generator.cpp, src/sblr/expression_evaluator.cpp, include/scratchbird/parser/ast.h

---

## Overview

Several SQL syntax elements fail to parse or generate bytecode. This includes CHECK constraints, assignment operators, IN value lists, expression evaluation infrastructure, and GRANT WITH ADMIN OPTION syntax.

---

## Tasks

### PARSE-1: CHECK table constraints (HIGH)
**File:** src/parser/parser.cpp
**Line:** 1433-1434
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
// CHECK constraints - not yet implemented
error("CHECK table constraints not yet implemented");
```

**Required Changes:**
1. Parse CHECK (expression) syntax
2. Create CheckConstraint AST node
3. Generate bytecode for constraint expression
4. Wire to catalog constraint storage

**Grammar:**
```
table_constraint
    : CHECK '(' expression ')'
    ;
```

**Verification:**
- [ ] CREATE TABLE t (a INT, CHECK (a > 0)) works
- [ ] Constraint enforced on INSERT

---

### PARSE-2: Assignment := operator (HIGH)
**File:** src/parser/parser.cpp
**Line:** 2337
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
error("Assignment statements require := operator (not yet implemented in lexer)");
```

**Required Changes:**
1. Add COLON_EQUALS token to lexer (lexer.cpp)
2. Parse assignment statement: identifier := expression
3. Generate bytecode for assignment

**Lexer Addition:**
```cpp
case ':':
    if (peek() == '=') {
        advance();
        return makeToken(TokenType::COLON_EQUALS);
    }
    // ... existing colon handling
```

**Verification:**
- [ ] PL/pgSQL: x := 5; compiles
- [ ] PL/pgSQL: x := x + 1; compiles

---

### PARSE-3: IN (value list) (HIGH)
**File:** src/parser/parser.cpp
**Line:** 5601
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
error("IN with value list not yet implemented - use subquery");
```

**Required Changes:**
1. Parse IN (expr, expr, expr, ...)
2. Create InListExpr AST node (distinct from InSubqueryExpr)
3. Generate bytecode to check membership

**Implementation:**
```cpp
// Transform: x IN (1, 2, 3)
// Into: x = 1 OR x = 2 OR x = 3
// Or use dedicated IN_LIST opcode
```

**Verification:**
- [ ] SELECT * FROM t WHERE a IN (1, 2, 3) works
- [ ] IN with mixed types works
- [ ] NOT IN works

---

### PARSE-4: evaluateForTuple (HIGH)
**File:** src/sblr/expression_evaluator.cpp
**Lines:** 545-577
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
throw std::runtime_error("ExpressionEvaluator::evaluateForTuple not yet fully implemented - requires table context");
```

**Required Changes:**
1. Accept table_id and row data
2. Look up column values from row
3. Evaluate expression with column bindings

**Interface:**
```cpp
TypedValue evaluateForTuple(
    const ID& table_id,
    const std::vector<std::string>& column_names,
    const std::vector<TypedValue>& column_values,
    TransactionId xid
);
```

**Verification:**
- [ ] Index creation with expression evaluates correctly
- [ ] CHECK constraints evaluate against row

---

### PARSE-5: evaluatePredicateForTuple (HIGH)
**File:** src/sblr/expression_evaluator.cpp
**Lines:** 579-611
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
throw std::runtime_error("ExpressionEvaluator::evaluatePredicateForTuple not yet fully implemented - requires table context");
```

**Required Changes:**
Same as PARSE-4 but returns boolean.

**Verification:**
- [ ] Row-level security predicates evaluate correctly

---

### PARSE-M1: Window function direct codegen (MEDIUM)
**File:** src/sblr/bytecode_generator.cpp
**Line:** 5140
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
current_result_->addError("Direct window function bytecode generation not yet supported");
```

**Required Changes:**
Generate bytecode for window functions in non-optimized path.

**Note:** Optimized path works; this is for fallback/edge cases.

**Verification:**
- [ ] Window functions work in all query patterns

---

### PARSE-M2: Window spec direct codegen (MEDIUM)
**File:** src/sblr/bytecode_generator.cpp
**Line:** 5147
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
current_result_->addError("Direct window spec bytecode generation not yet supported");
```

**Required Changes:**
Generate bytecode for window specifications (PARTITION BY, ORDER BY, frame).

**Verification:**
- [ ] Complex window specs work

---

### PARSE-M3: ARRAY subqueries (MEDIUM)
**File:** src/sblr/bytecode_generator.cpp
**Line:** 5325
**Status:** [ ] NOT STARTED

**Current Code:**
```cpp
current_result_->addError("ARRAY subqueries not yet supported");
```

**Required Changes:**
1. Recognize ARRAY(subquery) syntax
2. Execute subquery
3. Collect results into array

**Verification:**
- [ ] SELECT ARRAY(SELECT id FROM t) works

---

### PARSE-L1: GRANT WITH ADMIN OPTION (LOW)
**Files:** include/scratchbird/parser/ast.h, src/parser/parser.cpp, src/sblr/bytecode_generator.cpp, src/sblr/executor.cpp
**Status:** [ ] NOT STARTED
**Blocked From:** WP-4 EXEC-L1

**Current Code:**
```cpp
// In executor.cpp line 20479:
// Phase 2 Enhancement: WITH ADMIN OPTION requires bytecode generator update
bool with_admin_option = false;  // Hardcoded
```

**Required Changes:**
1. **AST (ast.h):** Add `with_admin_option_` member to `GrantRoleStmt` class
   ```cpp
   class GrantRoleStmt : public Statement {
       // ... existing members ...
       bool with_admin_option_ = false;
   public:
       bool withAdminOption() const { return with_admin_option_; }
   };
   ```

2. **Parser (parser.cpp):** Parse "WITH ADMIN OPTION" after grantee
   ```cpp
   // After parsing grantee_name:
   bool with_admin = false;
   if (match(TokenType::WITH) && match(TokenType::ADMIN) && match(TokenType::OPTION)) {
       with_admin = true;
   }
   ```

3. **Bytecode Generator:** Emit WITH ADMIN flag byte
   ```cpp
   // In visit(GrantRoleStmt*):
   current_result_->writeByte(node->withAdminOption() ? 1 : 0);
   ```

4. **Executor:** Read and use WITH ADMIN flag
   ```cpp
   bool with_admin_option = readByte() != 0;
   // Pass to grantRole()
   ```

**Verification:**
- [ ] GRANT role TO user WITH ADMIN OPTION parses correctly
- [ ] WITH ADMIN OPTION is stored in grant record
- [ ] User with ADMIN option can grant role to others

---

## Testing Plan

1. Parser tests for each new syntax element
2. Bytecode generation verification
3. Execution tests for each feature
4. Regression testing (1053 tests)

---

## Completion Checklist

- [ ] All 9 tasks implemented
- [ ] Lexer updated (COLON_EQUALS token)
- [ ] AST updated (GrantRoleStmt with_admin_option_)
- [ ] All 1053 existing tests pass
- [ ] New syntax tests added
- [ ] Code compiles without warnings

---

**Last Updated:** December 3, 2025
