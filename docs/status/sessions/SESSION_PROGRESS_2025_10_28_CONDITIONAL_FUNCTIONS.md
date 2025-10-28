# Session Progress: Phase 1 Task 8 - Conditional Functions
**Date**: October 28, 2025
**Status**: IN PROGRESS (40% complete)
**Focus**: COALESCE, NULLIF, and CASE expressions

---

## Progress Summary

### ✅ Completed (40%)

#### 1. Lexer - Keywords (100% complete)
- Added 7 new keywords to TokenType enum:
  - KW_COALESCE
  - KW_NULLIF
  - KW_CASE, KW_WHEN, KW_THEN, KW_ELSE, KW_END
- Added keyword mappings in lexer.cpp
- Added string representations in token.cpp

#### 2. AST Nodes (100% complete)
- Added 3 new ASTKind enums: COALESCE, NULLIF, CASE
- Implemented CoalesceExpr class:
  - Stores vector of expressions (variadic)
  - Returns first non-null value
- Implemented NullIfExpr class:
  - Stores two expressions
  - Returns NULL if expr1 == expr2, else expr1
- Implemented CaseExpr class:
  - WhenClause struct with condition and result
  - Support for simple CASE (with case_operand)
  - Support for searched CASE (without case_operand)
  - Optional else_result
  - isSimpleCase() method to distinguish forms

#### 3. Visitor Pattern (100% complete)
- Added pure virtual methods to ASTVisitor
- Implemented accept() methods in all 3 expression classes
- Implemented ASTPrinter::visit() for debug output

#### 4. Semantic Analysis (100% complete)
- Implemented SemanticAnalyzer::visit() for all 3 expressions
- Type checking:
  - COALESCE: Uses type of first argument
  - NULLIF: Uses type of first argument, always nullable
  - CASE: Uses type of first THEN result, always nullable
- All expressions properly validate their arguments

#### 5. Build Verification (100% complete)
- scratchbird_parser builds successfully
- No compilation errors or warnings

---

### ⏳ Remaining (60%)

#### 6. Parser Implementation (Not Started)
- [ ] parseCoalesce() - Parse COALESCE(expr, expr, ...)
- [ ] parseNullIf() - Parse NULLIF(expr1, expr2)
- [ ] parseCaseExpression() - Parse CASE expressions
  - [ ] Simple CASE: CASE expr WHEN value THEN result ... END
  - [ ] Searched CASE: CASE WHEN condition THEN result ... END
  - [ ] Handle optional ELSE clause
- [ ] Integrate into parsePrimary() or parseExpression()

#### 7. Bytecode Generation (Not Started)
- [ ] Add SBLR opcodes:
  - [ ] Opcode::COALESCE
  - [ ] Opcode::NULLIF
  - [ ] Opcode::CASE_SIMPLE
  - [ ] Opcode::CASE_SEARCHED
- [ ] Implement BytecodeGenerator::visit() for all 3 expressions
- [ ] Generate bytecode with proper jump labels for CASE

#### 8. Executor Implementation (Not Started)
- [ ] Implement COALESCE execution:
  - [ ] Evaluate arguments left to right
  - [ ] Return first non-null value
  - [ ] Return NULL if all null
- [ ] Implement NULLIF execution:
  - [ ] Evaluate both arguments
  - [ ] Compare for equality
  - [ ] Return NULL if equal, expr1 otherwise
- [ ] Implement CASE execution:
  - [ ] Simple CASE: evaluate case_operand once, compare with WHEN values
  - [ ] Searched CASE: evaluate each WHEN condition
  - [ ] Return first matching THEN result
  - [ ] Return ELSE result if no match
  - [ ] Return NULL if no match and no ELSE

#### 9. Testing (Not Started)
- [ ] Unit tests for parser
- [ ] Unit tests for bytecode generation
- [ ] Integration tests for executor
- [ ] Edge case tests (all NULL, no ELSE clause, etc.)

#### 10. Documentation (Not Started)
- [ ] Update FEATURE_PARITY_ROADMAP.md
- [ ] Create session summary document

---

## Implementation Plan

### Next Steps (in order):

1. **Parser Implementation** (2-3 hours)
   - Add COALESCE/NULLIF parsing in parsePrimary()
   - Add CASE parsing (handle both simple and searched forms)
   - Handle proper precedence and error recovery

2. **Bytecode Generation** (1-2 hours)
   - Add 4 opcodes to opcodes.h
   - Implement visit methods in BytecodeGenerator
   - Generate efficient bytecode for CASE (use jump instructions)

3. **Executor Implementation** (2-3 hours)
   - Implement opcode handlers in executor.cpp
   - Handle NULL propagation correctly
   - Optimize CASE evaluation (short-circuit)

4. **Testing** (2-3 hours)
   - Write comprehensive test suite
   - Test all edge cases
   - Verify MySQL/PostgreSQL compatibility

5. **Documentation & Commit** (1 hour)
   - Update roadmap
   - Create session summary
   - Commit complete implementation

**Estimated Time Remaining**: 8-12 hours

---

## SQL Syntax Support

### COALESCE
```sql
SELECT COALESCE(col1, col2, 'default') FROM table;
-- Returns first non-NULL value
```

### NULLIF
```sql
SELECT NULLIF(col1, col2) FROM table;
-- Returns NULL if col1 = col2, else col1
```

### CASE - Simple Form
```sql
SELECT CASE status
    WHEN 'active' THEN 1
    WHEN 'inactive' THEN 0
    ELSE NULL
END FROM table;
```

### CASE - Searched Form
```sql
SELECT CASE
    WHEN amount > 1000 THEN 'high'
    WHEN amount > 100 THEN 'medium'
    ELSE 'low'
END FROM table;
```

---

## Commit History

1. **bb03ce6** - Phase 1 Task 8: Add AST and Semantic Analysis for Conditional Functions
   - 7 keywords added
   - 3 AST node classes
   - Semantic analysis
   - Build successful

---

## Notes

- All conditional functions return nullable types
- CASE expressions can have multiple WHEN clauses
- Simple CASE is syntactic sugar for searched CASE
- Need to handle NULL comparisons properly in NULLIF
- CASE needs proper jump instruction support in bytecode

---

**Next Session**: Continue with parser implementation
