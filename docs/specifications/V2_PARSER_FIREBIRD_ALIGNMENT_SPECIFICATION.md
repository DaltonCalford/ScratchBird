# V2 Parser Firebird Alignment Specification

**Version:** 1.0
**Date:** 2026-01-07
**Status:** DRAFT - Requires Implementation
**Related:** `/docs/specifications/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md`, `/docs/audit/parsers/COMPARISON_MATRIX.md`

---

## Executive Summary

This specification addresses **PostgreSQL-style syntax contamination** in the V2 parser and provides a migration plan to align V2 with **Firebird style and semantics**.

**Project Requirement:**
> "The V2 parser which is the core parser of the project, will have many more expansions than any of the other three but in Style/Formatting it should follow the FirebirdSQL standard."

**Current State:** V2 parser contains PostgreSQL-specific syntax that conflicts with Firebird style
**Target State:** V2 is a clean superset of Firebird, using Firebird-style syntax for all features

---

## Table of Contents

1. [PostgreSQL Contamination Analysis](#postgresql-contamination-analysis)
2. [Firebird-Style Alternatives](#firebird-style-alternatives)
3. [Migration Strategy](#migration-strategy)
4. [Implementation Plan](#implementation-plan)
5. [Deprecation Timeline](#deprecation-timeline)

---

## PostgreSQL Contamination Analysis

### Issue #1: INSERT ... ON CONFLICT (PostgreSQL Upsert)

**Location:** `src/parser/parser_v2.cpp:2688-2741`

**Current PostgreSQL-Style Syntax:**
```sql
-- PostgreSQL 9.5+ syntax (currently supported in V2)
INSERT INTO users (id, name, email)
VALUES (1, 'Alice', 'alice@example.com')
ON CONFLICT (email)
DO UPDATE SET name = EXCLUDED.name, updated_at = CURRENT_TIMESTAMP
WHERE users.active = true;

-- With constraint target
INSERT INTO products (sku, name, price)
VALUES ('ABC123', 'Widget', 19.99)
ON CONFLICT ON CONSTRAINT products_sku_key
DO NOTHING;

-- With partial index target
INSERT INTO logs (timestamp, message)
VALUES (CURRENT_TIMESTAMP, 'test')
ON CONFLICT (timestamp) WHERE archived = false
DO UPDATE SET message = EXCLUDED.message;
```

**Why This is PostgreSQL-Specific:**
1. `ON CONFLICT` clause is PostgreSQL 9.5+ extension
2. `EXCLUDED` pseudo-table is PostgreSQL-specific
3. `DO NOTHING` / `DO UPDATE` syntax is PostgreSQL-specific
4. Firebird has different upsert mechanisms: `UPDATE OR INSERT` and `MERGE`

**Code Location:**
```cpp
// src/parser/parser_v2.cpp:2688-2741
void Parser::parseOnConflict(InsertStmt* stmt) {
    stmt->on_conflict = arena_.create<OnConflictClause>();

    // Conflict target (optional)
    if (check(TokenType::LEFT_PAREN)) {
        advance();
        do {
            stmt->on_conflict->columns.push_back(expectIdentifier("Expected column name"));
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after conflict columns");
    } else if (match(TokenType::KW_ON)) {
        expectContextual("CONSTRAINT", "Expected CONSTRAINT after ON");
        stmt->on_conflict->constraint_name = expectIdentifier("Expected constraint name");
    }

    // WHERE clause for partial index
    if (match(TokenType::KW_WHERE)) {
        stmt->on_conflict->where_target = parseExpression();
    }

    // DO action
    expectContextual("DO", "Expected DO after conflict target");

    if (matchContextual("NOTHING")) {
        stmt->on_conflict->action = ConflictAction::NOTHING;
    } else if (match(TokenType::KW_UPDATE)) {
        stmt->on_conflict->action = ConflictAction::UPDATE;
        expect(TokenType::KW_SET, "Expected SET after UPDATE");

        // Parse SET assignments
        do {
            StringPool::StringId column = expectIdentifier("Expected column name");
            expect(TokenType::EQUAL, "Expected '=' in SET clause");
            Expression* value = nullptr;
            if (matchContextual("EXCLUDED")) {
                expect(TokenType::DOT, "Expected '.' after EXCLUDED");
                // EXCLUDED pseudo-table (PostgreSQL-specific)
                auto* ref = arena_.create<ColumnRefExpr>();
                ref->column.column_name = expectIdentifier("Expected column name");
                ref->column.has_table_qualifier = true;
                value = ref;
            } else {
                value = parseExpression();
            }
            stmt->on_conflict->set_items.push_back({column, value});
        } while (match(TokenType::COMMA));

        // Optional WHERE clause for UPDATE
        if (match(TokenType::KW_WHERE)) {
            stmt->on_conflict->where_action = parseExpression();
        }
    }
}
```

**AST Definition:**
```cpp
// include/scratchbird/parser/ast_v2.h:1855-1875
enum class ConflictAction : uint8_t {
    NOTHING,    // DO NOTHING
    UPDATE,     // DO UPDATE SET ...
};

struct OnConflictClause {
    // Conflict target
    std::vector<StringPool::StringId> columns;      // (col1, col2)
    StringPool::StringId constraint_name = StringPool::INVALID_ID;  // ON CONSTRAINT name
    Expression* where_target = nullptr;             // WHERE for partial index

    // Action
    ConflictAction action = ConflictAction::NOTHING;

    // For DO UPDATE
    std::vector<std::pair<StringPool::StringId, Expression*>> set_items;
    Expression* where_action = nullptr;  // WHERE clause for DO UPDATE
};
```

---

### Issue #2: UPDATE ... FROM (PostgreSQL Extension)

**Location:** `src/parser/parser_v2.cpp:2781-2799`

**Current PostgreSQL-Style Syntax:**
```sql
-- PostgreSQL extension (currently supported in V2)
UPDATE employees e
SET salary = s.new_salary
FROM salary_adjustments s
WHERE e.emp_id = s.emp_id
  AND s.effective_date = CURRENT_DATE;

-- With JOIN syntax
UPDATE products p
SET stock = stock - oi.quantity
FROM order_items oi
JOIN orders o ON oi.order_id = o.id
WHERE p.product_id = oi.product_id
  AND o.status = 'shipped';
```

**Why This is PostgreSQL-Specific:**
1. `FROM` clause in UPDATE is PostgreSQL extension (not in SQL standard)
2. Firebird does NOT support `UPDATE ... FROM` syntax
3. Firebird uses correlated subqueries for multi-table updates

**Code Location:**
```cpp
// src/parser/parser_v2.cpp:2781-2799
// FROM clause (PostgreSQL extension)
if (match(TokenType::KW_FROM)) {
    ParseModeGuard fromGuard(state_, ParseMode::TABLE_REF);
    stmt->from = parseTableRef();

    // Parse joins in FROM clause
    while (check(TokenType::KW_JOIN) ||
           checkContextual("LEFT") ||
           checkContextual("RIGHT") ||
           checkContextual("INNER") ||
           checkContextual("FULL") ||
           checkContextual("CROSS") ||
           checkContextual("NATURAL")) {
        JoinNode* join = parseJoin(stmt->from);
        if (join) {
            stmt->joins.push_back(join);
        }
    }
}
```

---

### Issue #3: DELETE ... USING (PostgreSQL Extension)

**Location:** `src/parser/parser_v2.cpp:2867-2885`

**Current PostgreSQL-Style Syntax:**
```sql
-- PostgreSQL extension (currently supported in V2)
DELETE FROM employees e
USING departments d
WHERE e.dept_id = d.id
  AND d.closed_date IS NOT NULL;

-- With JOIN syntax
DELETE FROM order_items oi
USING orders o
JOIN customers c ON o.customer_id = c.id
WHERE oi.order_id = o.id
  AND c.account_status = 'closed';
```

**Why This is PostgreSQL-Specific:**
1. `USING` clause in DELETE is PostgreSQL extension
2. Firebird does NOT support `DELETE ... USING` syntax
3. Firebird uses correlated subqueries for multi-table deletes

**Code Location:**
```cpp
// src/parser/parser_v2.cpp:2867-2885
// USING clause (PostgreSQL extension)
if (match(TokenType::KW_USING)) {
    ParseModeGuard usingGuard(state_, ParseMode::TABLE_REF);
    stmt->using_clause = parseTableRef();

    // Parse joins in USING clause
    while (check(TokenType::KW_JOIN) ||
           checkContextual("LEFT") ||
           checkContextual("RIGHT") ||
           checkContextual("INNER") ||
           checkContextual("FULL") ||
           checkContextual("CROSS") ||
           checkContextual("NATURAL")) {
        JoinNode* join = parseJoin(stmt->using_clause);
        if (join) {
            stmt->using_joins.push_back(join);
        }
    }
}
```

---

### Issue #4: DROP ... CASCADE Semantics

**Status:** ⚠️ PARTIAL ISSUE

**Current V2 Syntax:**
```sql
-- Both PostgreSQL and Firebird support CASCADE
DROP TABLE orders CASCADE;
DROP SCHEMA accounting CASCADE;
```

**PostgreSQL CASCADE Semantics:**
- Automatically drops dependent objects
- Cascades through foreign key references
- Drops views, functions, triggers that depend on object

**Firebird CASCADE Semantics:**
- More limited cascade behavior
- Primarily for foreign key constraints (`ON DELETE CASCADE`)
- Does NOT automatically drop dependent objects like PostgreSQL

**Action Required:**
- Document CASCADE behavior differences
- Ensure V2 follows Firebird CASCADE semantics, not PostgreSQL

---

## Firebird-Style Alternatives

### Solution #1: Replace INSERT ... ON CONFLICT with Firebird Upsert

Firebird provides two mechanisms for upsert operations:

#### Option A: UPDATE OR INSERT (Firebird 2.1+)

**Firebird-Style Syntax:**
```sql
-- Firebird UPDATE OR INSERT (V2 should support this)
UPDATE OR INSERT INTO users (id, name, email)
VALUES (1, 'Alice', 'alice@example.com')
MATCHING (id)
RETURNING id, name;

-- Alternative: use all columns as matching keys (primary key assumed)
UPDATE OR INSERT INTO products (sku, name, price)
VALUES ('ABC123', 'Widget', 19.99);

-- With explicit RETURNING clause
UPDATE OR INSERT INTO logs (timestamp, message)
VALUES (CURRENT_TIMESTAMP, 'test')
MATCHING (timestamp)
RETURNING log_id;
```

**How UPDATE OR INSERT Works:**
1. If matching record exists (by `MATCHING` columns or PK), perform UPDATE
2. If no matching record exists, perform INSERT
3. Atomic operation - no race conditions
4. Optional `RETURNING` clause returns affected row data

**AST Definition Needed:**
```cpp
// Proposed addition to ast_v2.h
class UpdateOrInsertStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::UpdateOrInsertStmt; }
    void accept(ASTVisitor& visitor) override;

    // Target table
    SchemaPath table_path;

    // Column list
    std::vector<StringPool::StringId> columns;

    // Values
    std::vector<Expression*> values;

    // MATCHING clause (optional - defaults to primary key)
    std::vector<StringPool::StringId> matching_columns;
    bool has_matching = false;

    // RETURNING clause
    std::vector<SelectItem*> returning;
};
```

**SBLR Opcode:** Firebird-specific opcode (needs definition or use MERGE opcodes)

#### Option B: MERGE Statement (Firebird 5.0+, SQL Standard)

**Firebird-Style Syntax:**
```sql
-- SQL standard MERGE (Firebird 5.0+)
MERGE INTO users u
USING (VALUES (1, 'Alice', 'alice@example.com')) AS source(id, name, email)
ON u.id = source.id
WHEN MATCHED THEN
    UPDATE SET name = source.name, email = source.email
WHEN NOT MATCHED THEN
    INSERT (id, name, email) VALUES (source.id, source.name, source.email)
RETURNING u.id, u.name;

-- With source table
MERGE INTO inventory i
USING shipments s ON i.product_id = s.product_id
WHEN MATCHED THEN
    UPDATE SET quantity = i.quantity + s.quantity
WHEN NOT MATCHED THEN
    INSERT (product_id, quantity) VALUES (s.product_id, s.quantity);

-- Firebird 5.0+: WHEN NOT MATCHED BY SOURCE (delete unmatched target rows)
MERGE INTO products p
USING active_products ap ON p.id = ap.id
WHEN MATCHED THEN
    UPDATE SET last_updated = CURRENT_TIMESTAMP
WHEN NOT MATCHED BY SOURCE THEN
    DELETE;
```

**SBLR Opcodes:** Already defined (EXT_MERGE_START=0x4F through EXT_MERGE_END=0x55)

**Current Status:**
- ✅ Firebird parser: Implemented
- ❌ V2 parser: Not implemented (CRITICAL GAP)
- ❌ Executor: No support for MERGE opcodes

---

### Solution #2: Replace UPDATE ... FROM with Subqueries

**Firebird-Style Approach:**

Instead of PostgreSQL's `UPDATE ... FROM`, Firebird uses correlated subqueries:

```sql
-- WRONG: PostgreSQL style (currently in V2)
UPDATE employees e
SET salary = s.new_salary
FROM salary_adjustments s
WHERE e.emp_id = s.emp_id
  AND s.effective_date = CURRENT_DATE;

-- RIGHT: Firebird style (V2 should use this)
UPDATE employees e
SET salary = (
    SELECT s.new_salary
    FROM salary_adjustments s
    WHERE s.emp_id = e.emp_id
      AND s.effective_date = CURRENT_DATE
)
WHERE EXISTS (
    SELECT 1
    FROM salary_adjustments s
    WHERE s.emp_id = e.emp_id
      AND s.effective_date = CURRENT_DATE
);
```

**Multi-column Updates:**
```sql
-- WRONG: PostgreSQL style
UPDATE products p
SET stock = stock - oi.quantity,
    last_sold = o.order_date
FROM order_items oi
JOIN orders o ON oi.order_id = o.id
WHERE p.product_id = oi.product_id
  AND o.status = 'shipped';

-- RIGHT: Firebird style (option 1: multiple subqueries)
UPDATE products p
SET stock = stock - (
        SELECT oi.quantity
        FROM order_items oi
        JOIN orders o ON oi.order_id = o.id
        WHERE oi.product_id = p.product_id
          AND o.status = 'shipped'
    ),
    last_sold = (
        SELECT o.order_date
        FROM order_items oi
        JOIN orders o ON oi.order_id = o.id
        WHERE oi.product_id = p.product_id
          AND o.status = 'shipped'
    )
WHERE EXISTS (
    SELECT 1
    FROM order_items oi
    JOIN orders o ON oi.order_id = o.id
    WHERE oi.product_id = p.product_id
      AND o.status = 'shipped'
);

-- RIGHT: Firebird style (option 2: use MERGE for complex cases)
MERGE INTO products p
USING (
    SELECT oi.product_id, oi.quantity, o.order_date
    FROM order_items oi
    JOIN orders o ON oi.order_id = o.id
    WHERE o.status = 'shipped'
) s ON p.product_id = s.product_id
WHEN MATCHED THEN
    UPDATE SET stock = p.stock - s.quantity, last_sold = s.order_date;
```

**Implementation:**
- ✅ V2 already supports subqueries in UPDATE SET clause
- ❌ Remove `from` field from `UpdateStmt` AST
- ❌ Remove FROM clause parsing in UPDATE

---

### Solution #3: Replace DELETE ... USING with Subqueries

**Firebird-Style Approach:**

Instead of PostgreSQL's `DELETE ... USING`, Firebird uses subqueries in WHERE clause:

```sql
-- WRONG: PostgreSQL style (currently in V2)
DELETE FROM employees e
USING departments d
WHERE e.dept_id = d.id
  AND d.closed_date IS NOT NULL;

-- RIGHT: Firebird style (V2 should use this)
DELETE FROM employees e
WHERE EXISTS (
    SELECT 1
    FROM departments d
    WHERE d.id = e.dept_id
      AND d.closed_date IS NOT NULL
);

-- Or with IN subquery
DELETE FROM employees e
WHERE e.dept_id IN (
    SELECT id
    FROM departments
    WHERE closed_date IS NOT NULL
);
```

**Complex Multi-table Deletes:**
```sql
-- WRONG: PostgreSQL style
DELETE FROM order_items oi
USING orders o
JOIN customers c ON o.customer_id = c.id
WHERE oi.order_id = o.id
  AND c.account_status = 'closed';

-- RIGHT: Firebird style
DELETE FROM order_items oi
WHERE EXISTS (
    SELECT 1
    FROM orders o
    JOIN customers c ON o.customer_id = c.id
    WHERE oi.order_id = o.id
      AND c.account_status = 'closed'
);
```

**Implementation:**
- ✅ V2 already supports subqueries in WHERE clause
- ❌ Remove `using_clause` field from `DeleteStmt` AST
- ❌ Remove USING clause parsing in DELETE

---

### Solution #4: Document CASCADE Semantics

**Firebird CASCADE Behavior:**

```sql
-- Foreign key CASCADE (supported in Firebird)
CREATE TABLE orders (
    id INTEGER PRIMARY KEY,
    customer_id INTEGER REFERENCES customers(id) ON DELETE CASCADE
);
-- When customer is deleted, all their orders are automatically deleted

-- DROP with CASCADE (limited in Firebird vs PostgreSQL)
DROP TABLE orders CASCADE;
-- Firebird: Drops table if no dependencies exist
-- PostgreSQL: Drops table AND all dependent views/functions/triggers

-- V2 should follow Firebird semantics:
-- 1. CASCADE on foreign keys works as expected
-- 2. CASCADE on DROP is more restrictive than PostgreSQL
-- 3. Document differences clearly
```

**Action Required:**
- ✅ Keep CASCADE support (compatible with Firebird)
- ❌ Document that CASCADE behavior follows Firebird, not PostgreSQL
- ⚠️ Consider adding `RESTRICT` as default (Firebird default)

---

## Migration Strategy

### Phase 1: Add Firebird-Style Alternatives (Non-Breaking)

**Priority:** HIGH - Enables users to migrate their code

1. **Implement UPDATE OR INSERT**
   - Add `UpdateOrInsertStmt` to AST
   - Implement parser for `UPDATE OR INSERT` syntax
   - Generate SBLR opcodes
   - Test with Firebird examples

2. **Implement MERGE Statement**
   - Complete MERGE parsing in V2 (currently missing)
   - Implement executor support for MERGE opcodes
   - Test with SQL standard examples

3. **Document Firebird-Style Patterns**
   - Publish migration guide for PostgreSQL → Firebird syntax
   - Provide examples of subquery alternatives
   - Document best practices

**Timeline:** Implement before any deprecation

---

### Phase 2: Deprecate PostgreSQL-Style Syntax

**Priority:** MEDIUM - After Firebird alternatives are stable

1. **Add Deprecation Warnings**
   ```cpp
   // In parseOnConflict()
   if (matchContextual("ON") && matchContextual("CONFLICT")) {
       // Emit deprecation warning
       warning("INSERT ... ON CONFLICT is deprecated. Use UPDATE OR INSERT or MERGE instead.");
       // Continue parsing for backwards compatibility
   }
   ```

2. **Update Documentation**
   - Mark PostgreSQL syntax as deprecated
   - Provide migration examples
   - Set removal timeline

3. **Notify Users**
   - Release notes document deprecation
   - Provide migration period (e.g., 6-12 months)

**Timeline:** After Phase 1 complete + 3 months stabilization

---

### Phase 3: Remove PostgreSQL-Style Syntax (Breaking Change)

**Priority:** LOW - Long-term goal

1. **Remove AST Fields**
   ```cpp
   // Remove from InsertStmt
   OnConflictClause* on_conflict = nullptr;  // REMOVE THIS

   // Remove from UpdateStmt
   TableRef* from = nullptr;                 // REMOVE THIS
   std::vector<JoinNode*> joins;             // REMOVE THIS

   // Remove from DeleteStmt
   TableRef* using_clause = nullptr;         // REMOVE THIS
   std::vector<JoinNode*> using_joins;       // REMOVE THIS
   ```

2. **Remove Parser Code**
   - Remove `parseOnConflict()` function
   - Remove FROM clause parsing in UPDATE
   - Remove USING clause parsing in DELETE

3. **Remove Opcode Support**
   - Remove or repurpose `EXT_ON_CONFLICT*` opcodes (0x82-0x87)
   - Update SBLR opcode mapping documentation

**Timeline:** After deprecation period + major version bump

---

## Implementation Plan

### Step 1: Implement UPDATE OR INSERT (Immediate)

**File:** `src/parser/parser_v2.cpp`

**New Function:**
```cpp
UpdateOrInsertStmt* Parser::parseUpdateOrInsert() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<UpdateOrInsertStmt>();

    // UPDATE keyword already consumed
    expectContextual("OR", "Expected OR in UPDATE OR INSERT");
    expect(TokenType::KW_INSERT, "Expected INSERT after UPDATE OR");

    expect(TokenType::KW_INTO, "Expected INTO after INSERT");

    // Table name
    stmt->table_path = parseSchemaPath(state_);

    // Column list
    expect(TokenType::LEFT_PAREN, "Expected '(' before column list");
    do {
        stmt->columns.push_back(expectIdentifier("Expected column name"));
    } while (match(TokenType::COMMA));
    expect(TokenType::RIGHT_PAREN, "Expected ')' after column list");

    // VALUES clause
    expect(TokenType::KW_VALUES, "Expected VALUES");
    expect(TokenType::LEFT_PAREN, "Expected '(' before values");
    do {
        stmt->values.push_back(parseExpression());
    } while (match(TokenType::COMMA));
    expect(TokenType::RIGHT_PAREN, "Expected ')' after values");

    // MATCHING clause (optional)
    if (matchContextual("MATCHING")) {
        stmt->has_matching = true;
        expect(TokenType::LEFT_PAREN, "Expected '(' after MATCHING");
        do {
            stmt->matching_columns.push_back(expectIdentifier("Expected column name"));
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after matching columns");
    }

    // RETURNING clause (optional)
    if (matchContextual("RETURNING")) {
        parseReturningClause(stmt->returning);
    }

    stmt->span = makeSpan(start);
    return stmt;
}
```

**Update parseStatement():**
```cpp
Statement* Parser::parseStatement() {
    // ... existing code ...

    if (match(TokenType::KW_UPDATE)) {
        // Check for UPDATE OR INSERT
        if (checkContextual("OR")) {
            return parseUpdateOrInsert();
        }
        return parseUpdate();
    }

    // ... rest of code ...
}
```

---

### Step 2: Complete MERGE Implementation (High Priority)

**Current Status:**
- ✅ Firebird parser: Implemented
- ❌ V2 parser: Not implemented
- ❌ Executor: No MERGE support

**Action Required:**
1. Port MERGE parsing from Firebird parser to V2 parser
2. Implement executor support for MERGE opcodes (0x4F-0x55)
3. Add comprehensive tests

**Reference:** See Firebird parser implementation for complete example

---

### Step 3: Add Deprecation Warnings (After Step 1 & 2)

**File:** `src/parser/parser_v2.cpp`

```cpp
// In parseInsert(), after parsing table name:
if (matchContextual("ON") && checkContextual("CONFLICT")) {
    // Emit deprecation warning
    diagnostic(DiagnosticLevel::WARNING, currentLocation(),
        "INSERT ... ON CONFLICT is deprecated and will be removed in a future version. "
        "Use UPDATE OR INSERT or MERGE instead. "
        "See migration guide at: /docs/migration/postgresql-to-firebird-syntax.md");

    // Continue parsing for backwards compatibility
    parseOnConflict(stmt);
}

// In parseUpdate():
if (match(TokenType::KW_FROM)) {
    diagnostic(DiagnosticLevel::WARNING, currentLocation(),
        "UPDATE ... FROM is deprecated and will be removed in a future version. "
        "Use subqueries or MERGE instead. "
        "See migration guide at: /docs/migration/postgresql-to-firebird-syntax.md");

    // Continue parsing for backwards compatibility
    ParseModeGuard fromGuard(state_, ParseMode::TABLE_REF);
    stmt->from = parseTableRef();
    // ... rest of parsing ...
}

// In parseDelete():
if (match(TokenType::KW_USING)) {
    diagnostic(DiagnosticLevel::WARNING, currentLocation(),
        "DELETE ... USING is deprecated and will be removed in a future version. "
        "Use subqueries instead. "
        "See migration guide at: /docs/migration/postgresql-to-firebird-syntax.md");

    // Continue parsing for backwards compatibility
    ParseModeGuard usingGuard(state_, ParseMode::TABLE_REF);
    stmt->using_clause = parseTableRef();
    // ... rest of parsing ...
}
```

---

### Step 4: Create Migration Documentation

**File:** `/docs/migration/postgresql-to-firebird-syntax.md`

Contents:
- Side-by-side examples of PostgreSQL vs Firebird syntax
- Performance considerations
- Edge cases and limitations
- Complete migration checklist

---

### Step 5: Remove PostgreSQL Syntax (Future Major Version)

**Breaking Change - Requires Major Version Bump**

1. Remove AST fields
2. Remove parser functions
3. Remove opcodes or mark as reserved
4. Update all documentation
5. Remove backwards compatibility code

**Timeline:** Not before 12 months after deprecation warnings added

---

## Deprecation Timeline

### Phase 1: Current → Release 1.0 (Immediate)
- ✅ Keep all PostgreSQL syntax (backwards compatibility)
- ✅ Document as "temporary compatibility layer"
- 🔧 Implement UPDATE OR INSERT
- 🔧 Complete MERGE implementation
- 📖 Create migration guide

### Phase 2: Release 1.0 → Release 2.0 (6-12 months)
- ⚠️ Add deprecation warnings for PostgreSQL syntax
- ⚠️ Mark as "deprecated" in documentation
- ✅ Keep functionality (backwards compatible)
- 📖 Publish migration examples
- 📣 Announce deprecation in release notes

### Phase 3: Release 2.0 → Release 3.0 (6-12 months)
- ⚠️ Escalate warnings to errors with `--strict` flag
- ⚠️ Final warning period
- ✅ Still backwards compatible by default
- 📖 Update all examples to use Firebird syntax

### Phase 4: Release 3.0+ (Breaking Change)
- ❌ Remove PostgreSQL syntax support
- ❌ Remove AST fields
- ❌ Remove parser code
- 📖 Update documentation to remove all PostgreSQL references
- 🎉 V2 is pure Firebird-style superset

---

## Compliance Checklist

### Firebird-Style Requirements

- [ ] **Upsert Operations**
  - [ ] UPDATE OR INSERT implemented
  - [ ] MERGE statement implemented
  - [ ] ON CONFLICT deprecated
  - [ ] Migration guide published

- [ ] **Multi-table UPDATE**
  - [ ] Subquery approach documented
  - [ ] MERGE alternative documented
  - [ ] UPDATE ... FROM deprecated
  - [ ] Examples provided

- [ ] **Multi-table DELETE**
  - [ ] Subquery approach documented
  - [ ] Examples provided
  - [ ] DELETE ... USING deprecated

- [ ] **CASCADE Behavior**
  - [ ] Firebird CASCADE semantics documented
  - [ ] Differences from PostgreSQL noted
  - [ ] RESTRICT default considered

### Parser Alignment

- [ ] V2 supports all Firebird context variables
- [ ] V2 supports all Firebird transaction modes
- [ ] V2 supports Firebird PSQL syntax
  - [ ] PSQL CASE statement (simple + searched forms)
- [ ] V2 uses Firebird-style DDL (RECREATE, etc.)
- [ ] V2 uses Firebird-style operators
- [ ] No PostgreSQL-specific keywords in reserved list

### Documentation

- [ ] Migration guide complete
- [ ] Side-by-side examples provided
- [ ] Performance implications documented
- [ ] Deprecation timeline published
- [ ] Release notes updated

---

## Example Migrations

### Example 1: Simple Upsert

**Before (PostgreSQL style):**
```sql
INSERT INTO users (id, name, email)
VALUES (1, 'Alice', 'alice@example.com')
ON CONFLICT (email)
DO UPDATE SET name = EXCLUDED.name;
```

**After (Firebird style - Option 1):**
```sql
UPDATE OR INSERT INTO users (id, name, email)
VALUES (1, 'Alice', 'alice@example.com')
MATCHING (email);
```

**After (Firebird style - Option 2):**
```sql
MERGE INTO users u
USING (VALUES (1, 'Alice', 'alice@example.com')) AS s(id, name, email)
ON u.email = s.email
WHEN MATCHED THEN
    UPDATE SET name = s.name
WHEN NOT MATCHED THEN
    INSERT (id, name, email) VALUES (s.id, s.name, s.email);
```

---

### Example 2: Multi-table Update

**Before (PostgreSQL style):**
```sql
UPDATE employees e
SET salary = s.new_salary,
    department = d.name
FROM salary_adjustments s
JOIN departments d ON s.dept_id = d.id
WHERE e.emp_id = s.emp_id
  AND s.effective_date = CURRENT_DATE;
```

**After (Firebird style - Subqueries):**
```sql
UPDATE employees e
SET salary = (
        SELECT s.new_salary
        FROM salary_adjustments s
        WHERE s.emp_id = e.emp_id
          AND s.effective_date = CURRENT_DATE
    ),
    department = (
        SELECT d.name
        FROM salary_adjustments s
        JOIN departments d ON s.dept_id = d.id
        WHERE s.emp_id = e.emp_id
          AND s.effective_date = CURRENT_DATE
    )
WHERE EXISTS (
    SELECT 1
    FROM salary_adjustments s
    WHERE s.emp_id = e.emp_id
      AND s.effective_date = CURRENT_DATE
);
```

**After (Firebird style - MERGE):**
```sql
MERGE INTO employees e
USING (
    SELECT s.emp_id, s.new_salary, d.name as dept_name
    FROM salary_adjustments s
    JOIN departments d ON s.dept_id = d.id
    WHERE s.effective_date = CURRENT_DATE
) src ON e.emp_id = src.emp_id
WHEN MATCHED THEN
    UPDATE SET salary = src.new_salary, department = src.dept_name;
```

---

### Example 3: Multi-table Delete

**Before (PostgreSQL style):**
```sql
DELETE FROM order_items oi
USING orders o
WHERE oi.order_id = o.id
  AND o.status = 'cancelled';
```

**After (Firebird style):**
```sql
DELETE FROM order_items oi
WHERE EXISTS (
    SELECT 1
    FROM orders o
    WHERE o.id = oi.order_id
      AND o.status = 'cancelled'
);
```

**Alternative (Firebird style):**
```sql
DELETE FROM order_items oi
WHERE oi.order_id IN (
    SELECT id
    FROM orders
    WHERE status = 'cancelled'
);
```

---

## References

### Firebird Documentation
- [Firebird 5.0 Language Reference](https://firebirdsql.org/file/documentation/html/en/refdocs/fblangref50/firebird-50-language-reference.html)
- [UPDATE OR INSERT Documentation](https://firebirdsql.org/file/documentation/chunk/en/refdocs/fblangref50/fblangref50-dml.html)
- [MERGE Statement Documentation](https://firebirdsql.org/file/documentation/chunk/en/refdocs/fblangref50/fblangref50-dml.html)

### PostgreSQL Documentation (for comparison)
- [INSERT ... ON CONFLICT](https://www.postgresql.org/docs/current/sql-insert.html)
- [UPDATE ... FROM](https://www.postgresql.org/docs/current/sql-update.html)
- [DELETE ... USING](https://www.postgresql.org/docs/current/sql-delete.html)

### Project Documentation
- `/docs/specifications/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md`
- `/docs/audit/parsers/COMPARISON_MATRIX.md`
- `/docs/audit/parsers/V2/SUMMARY.md`
- `/docs/audit/parsers/CRITICAL_FINDINGS.md`

---

## Summary

**Current Contamination:**
1. ❌ INSERT ... ON CONFLICT (lines 2688-2741)
2. ❌ UPDATE ... FROM (lines 2781-2799)
3. ❌ DELETE ... USING (lines 2867-2885)
4. ⚠️ CASCADE semantics (needs documentation)

**Firebird-Style Solutions:**
1. ✅ UPDATE OR INSERT (needs implementation)
2. ✅ MERGE statement (needs implementation in V2)
3. ✅ Subqueries in UPDATE/DELETE (already supported)
4. ✅ Firebird CASCADE (document differences)

**Implementation Priority:**
1. **CRITICAL:** Implement UPDATE OR INSERT
2. **CRITICAL:** Complete MERGE implementation
3. **HIGH:** Add deprecation warnings
4. **MEDIUM:** Create migration documentation
5. **LOW:** Remove PostgreSQL syntax (future major version)

---

**End of Specification**
**Next Steps:** Review, approve, and begin Phase 1 implementation
