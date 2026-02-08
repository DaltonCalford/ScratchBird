# Wave 2 Agent Task Specifications

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Date**: October 28, 2025
**Wave**: Phase 2 Wave 2
**Features**: CTEs, Subqueries, Basic Triggers
**Method**: 3 Parallel Autonomous AI Agents

---

## Agent A: Common Table Expressions (CTEs)

### Task Summary

Implement WITH clause (Common Table Expressions) to enable temporary named result sets in SQL queries.

**Estimated Effort**: 6-10 hours agent time (50-80 hours manual)
**Expected Output**: ~630-920 lines across parser, semantic analyzer, planner, bytecode generator, executor

### Background

CTEs allow complex queries to be broken into readable, reusable components:

```sql
WITH regional_sales AS (
    SELECT region, SUM(amount) as total
    FROM orders
    GROUP BY region
)
SELECT * FROM regional_sales WHERE total > 1000;
```

### What's Already Done

✅ **Parser Framework**: Ready for WITH keyword and CTE AST nodes
✅ **Query Planner**: Supports multi-stage query execution
✅ **Executor**: Handles temporary result sets
✅ **Semantic Analyzer**: Can validate nested queries

### Your Task: Implement CTEs

#### 1. Parser Layer (~100-150 lines)

**Location**: `src/parser/`

**Add WITH keyword**:
- `include/scratchbird/parser/token.h`: Add `KW_WITH` token
- `src/parser/lexer.cpp`: Map "WITH" to KW_WITH

**Create CTE AST nodes**:
```cpp
// In include/scratchbird/parser/ast.h

struct CTEDefinition {
    std::string name;
    SelectStmt* query;
    std::vector<std::string> column_aliases;  // Optional column aliases
};

class WithClause {
public:
    WithClause(SourceSpan span, std::vector<CTEDefinition> ctes)
        : span_(span), ctes_(std::move(ctes)) {}

    const std::vector<CTEDefinition>& ctes() const { return ctes_; }

private:
    SourceSpan span_;
    std::vector<CTEDefinition> ctes_;
};

// Update SelectStmt to include optional WITH clause
class SelectStmt : public Statement {
    // ... existing fields ...
    WithClause* with_clause_;  // Add this

public:
    // Add accessor
    WithClause* withClause() const { return with_clause_; }
};
```

**Parse WITH clause**:
```cpp
// In src/parser/parser.cpp

// Call this from parseSelectStmt() at the beginning
WithClause* Parser::parseWithClause() {
    if (!match(TokenType::KW_WITH)) {
        return nullptr;
    }

    auto start_loc = previous().location;
    std::vector<CTEDefinition> ctes;

    do {
        // Parse CTE name
        if (!check(TokenType::IDENTIFIER)) {
            error("Expected CTE name after WITH");
            return nullptr;
        }
        std::string cte_name = advance().lexeme;

        // Optional column aliases: (col1, col2, ...)
        std::vector<std::string> column_aliases;
        if (match(TokenType::LEFT_PAREN)) {
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    if (!check(TokenType::IDENTIFIER)) {
                        error("Expected column name in CTE column list");
                        return nullptr;
                    }
                    column_aliases.push_back(advance().lexeme);
                } while (match(TokenType::COMMA));
            }

            if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after CTE column list")) {
                return nullptr;
            }
        }

        // Expect AS
        if (!consume(TokenType::KW_AS, "Expected AS after CTE name")) {
            return nullptr;
        }

        // Parse CTE query: (SELECT ...)
        if (!consume(TokenType::LEFT_PAREN, "Expected '(' after AS")) {
            return nullptr;
        }

        auto* cte_query = parseSelectStmt();
        if (!cte_query) {
            error("Expected SELECT statement in CTE definition");
            return nullptr;
        }

        if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after CTE query")) {
            return nullptr;
        }

        ctes.push_back(CTEDefinition{cte_name, cte_query, column_aliases});

    } while (match(TokenType::COMMA));

    auto span = makeSpan(start_loc, previous().location);
    return arena_.make<WithClause>(span, ctes);
}

// Update parseSelectStmt() to call parseWithClause():
SelectStmt* Parser::parseSelectStmt() {
    auto start_loc = current().location;

    // Parse optional WITH clause first
    auto* with_clause = parseWithClause();

    // Then expect SELECT
    if (!consume(TokenType::KW_SELECT, "Expected SELECT")) {
        return nullptr;
    }

    // ... rest of SELECT parsing ...

    // Pass with_clause to SelectStmt constructor
    return arena_.make<SelectStmt>(span, with_clause, /* other args */);
}
```

**Reference Patterns**:
- JOIN parsing in `src/parser/parser.cpp` lines 1800-2000
- Subquery parsing (if exists) or nested expression patterns

#### 2. Semantic Analysis (~80-120 lines)

**Location**: `src/parser/semantic_analyzer.cpp`

**Validate CTEs**:
```cpp
// In visit(SelectStmt* stmt)
Result<void> SemanticAnalyzer::visit(SelectStmt* stmt) {
    // Process WITH clause first
    if (stmt->withClause()) {
        for (const auto& cte : stmt->withClause()->ctes()) {
            // Check CTE name is unique
            if (cte_names_.count(cte.name) > 0) {
                return error("CTE name '" + cte.name + "' already defined");
            }

            // Analyze CTE query
            auto result = analyzeCTEQuery(cte.query);
            if (!result.success()) {
                return result;
            }

            // Store CTE name and column types
            cte_names_.insert(cte.name);
            cte_schemas_[cte.name] = result.schema();

            // If column aliases provided, validate count matches
            if (!cte.column_aliases.empty()) {
                if (cte.column_aliases.size() != result.schema().columns.size()) {
                    return error("CTE column alias count mismatch");
                }
            }
        }
    }

    // Now analyze main query (CTEs are in scope)
    // ... existing SELECT validation ...

    // Clean up CTE scope after main query
    if (stmt->withClause()) {
        for (const auto& cte : stmt->withClause()->ctes()) {
            cte_names_.erase(cte.name);
            cte_schemas_.erase(cte.name);
        }
    }

    return Result<void>::success();
}

// Add CTE lookup to table resolution
Result<TableSchema> SemanticAnalyzer::resolveTable(const std::string& name) {
    // Check if it's a CTE first
    if (cte_schemas_.count(name) > 0) {
        return Result<TableSchema>::success(cte_schemas_[name]);
    }

    // Otherwise, look up in catalog
    // ... existing table resolution ...
}
```

**Add to semantic_analyzer.h**:
```cpp
private:
    std::unordered_set<std::string> cte_names_;
    std::unordered_map<std::string, TableSchema> cte_schemas_;
```

**Reference Patterns**:
- Table validation in semantic_analyzer.cpp
- Scope management (add/remove symbols)

#### 3. Query Planner (~150-200 lines)

**Location**: `src/optimizer/query_planner.cpp`

**Create CTE plan nodes**:
```cpp
// In include/scratchbird/optimizer/plan_node.h

class CTENode : public PlanNode {
public:
    CTENode(std::string name, std::unique_ptr<PlanNode> subplan)
        : name_(std::move(name)), subplan_(std::move(subplan)) {}

    const std::string& name() const { return name_; }
    PlanNode* subplan() const { return subplan_.get(); }

    std::string toString(int indent = 0) const override {
        std::string s = std::string(indent, ' ') + "CTE: " + name_ + "\n";
        s += subplan_->toString(indent + 2);
        return s;
    }

private:
    std::string name_;
    std::unique_ptr<PlanNode> subplan_;
};

class CTEScanNode : public PlanNode {
public:
    CTEScanNode(std::string cte_name)
        : cte_name_(std::move(cte_name)) {}

    const std::string& cteName() const { return cte_name_; }

    std::string toString(int indent = 0) const override {
        return std::string(indent, ' ') + "CTE Scan: " + cte_name_;
    }

private:
    std::string cte_name_;
};
```

**Plan CTEs**:
```cpp
// In src/optimizer/query_planner.cpp

std::unique_ptr<PlanNode> QueryPlanner::planQuery(const SelectStmt* stmt) {
    std::vector<std::unique_ptr<PlanNode>> cte_plans;

    // Plan each CTE first
    if (stmt->withClause()) {
        for (const auto& cte : stmt->withClause()->ctes()) {
            // Recursively plan CTE query
            auto cte_plan = planQuery(cte.query);

            // Wrap in CTENode
            cte_plans.push_back(
                std::make_unique<CTENode>(cte.name, std::move(cte_plan))
            );
        }
    }

    // Plan main query
    auto main_plan = planSelectBody(stmt);

    // If there are CTEs, wrap main plan with CTE execution
    if (!cte_plans.empty()) {
        // Create plan that executes CTEs then main query
        // (Implementation depends on your planner structure)
        return createCTEPlan(std::move(cte_plans), std::move(main_plan));
    }

    return main_plan;
}

// When encountering table reference, check if it's a CTE
std::unique_ptr<PlanNode> QueryPlanner::planTableScan(const std::string& table_name) {
    // Check if table_name is a CTE
    if (isCTEName(table_name)) {
        return std::make_unique<CTEScanNode>(table_name);
    }

    // Otherwise, regular table scan
    // ... existing logic ...
}
```

**Reference Patterns**:
- Subplan creation in query_planner.cpp
- Nested query planning

#### 4. Bytecode Generator (~100-150 lines)

**Location**: `src/sblr/bytecode_generator.cpp`

**Add CTE opcodes**:
```cpp
// In include/scratchbird/sblr/opcodes.h
enum class Opcode : uint8_t {
    // ... existing opcodes ...

    // CTE opcodes (0xD0-0xD3 range)
    CTE_START = 0xD0,      // Begin CTE materialization
    CTE_MATERIALIZE = 0xD1, // Store CTE result
    CTE_SCAN = 0xD2,        // Reference CTE by name
    CTE_END = 0xD3,         // End CTE scope
};
```

**Generate CTE bytecode**:
```cpp
// In bytecode_generator.cpp

void BytecodeGenerator::visit(SelectStmt* stmt) {
    // Generate CTEs first
    if (stmt->withClause()) {
        current_result_->writeByte(static_cast<uint8_t>(Opcode::CTE_START));
        current_result_->writeUInt16(stmt->withClause()->ctes().size());

        for (const auto& cte : stmt->withClause()->ctes()) {
            // Emit CTE name
            auto name_id = string_pool_.addString(cte.name);
            current_result_->writeUInt32(name_id);

            // Generate CTE query bytecode
            cte.query->accept(this);

            // Emit CTE_MATERIALIZE to store result
            current_result_->writeByte(static_cast<uint8_t>(Opcode::CTE_MATERIALIZE));
            current_result_->writeUInt32(name_id);
        }
    }

    // Generate main query bytecode
    // ... existing SELECT bytecode generation ...

    // End CTE scope
    if (stmt->withClause()) {
        current_result_->writeByte(static_cast<uint8_t>(Opcode::CTE_END));
    }
}

// When generating table scan, check for CTE references
void BytecodeGenerator::generateTableScan(const std::string& table_name) {
    // Check if table_name is a CTE
    if (isCTEReference(table_name)) {
        current_result_->writeByte(static_cast<uint8_t>(Opcode::CTE_SCAN));
        auto name_id = string_pool_.addString(table_name);
        current_result_->writeUInt32(name_id);
        return;
    }

    // Otherwise, regular table scan
    // ... existing logic ...
}
```

**Reference Patterns**:
- Subquery bytecode generation
- Temporary table handling

#### 5. Executor (~200-300 lines)

**Location**: `src/sblr/executor.cpp`

**Add CTE storage**:
```cpp
// In include/scratchbird/sblr/executor.h
class Executor {
    // ... existing fields ...

private:
    // CTE storage: name -> materialized result set
    std::unordered_map<std::string, std::vector<std::vector<Value>>> cte_results_;
};
```

**Implement CTE opcodes**:
```cpp
// In executor.cpp execute() switch statement

case Opcode::CTE_START: {
    uint16_t cte_count = readUInt16();
    // Initialize CTE storage
    cte_results_.clear();
    break;
}

case Opcode::CTE_MATERIALIZE: {
    uint32_t name_id = readUInt32();
    std::string cte_name = string_pool_.getString(name_id);

    // Pop result set from stack (assume CTE query pushed rows)
    // Store in cte_results_
    std::vector<std::vector<Value>> rows;

    // Collect all rows from current result set
    // (Implementation depends on how you handle result sets)
    while (hasMoreRows()) {
        std::vector<Value> row = popRow();
        rows.push_back(row);
    }

    cte_results_[cte_name] = std::move(rows);
    break;
}

case Opcode::CTE_SCAN: {
    uint32_t name_id = readUInt32();
    std::string cte_name = string_pool_.getString(name_id);

    // Look up CTE result
    if (cte_results_.count(cte_name) == 0) {
        throw std::runtime_error("CTE '" + cte_name + "' not found");
    }

    // Push CTE rows onto result set
    for (const auto& row : cte_results_[cte_name]) {
        pushRow(row);
    }
    break;
}

case Opcode::CTE_END: {
    // Clean up CTE storage
    cte_results_.clear();
    break;
}
```

**Reference Patterns**:
- Temporary result set handling in executor
- executeSelect method structure

#### 6. Testing (~100-150 lines)

**Create**: `tests/unit/test_cte.cpp`

```cpp
#include <gtest/gtest.h>
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/semantic_analyzer.h"
#include "scratchbird/sblr/bytecode_generator.h"

class CTETest : public ::testing::Test {
protected:
    void testCTE(const std::string& sql) {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "Parse failed: " << sql;

        // Semantic analysis (if catalog available)
        // SemanticAnalyzer analyzer(parser.stringPool());
        // auto semantic_result = analyzer.analyze(result.statement());
        // EXPECT_TRUE(semantic_result.success());

        // Bytecode generation
        BytecodeGenerator generator(parser.stringPool());
        auto bytecode_result = generator.generate(result.statement());
        EXPECT_TRUE(bytecode_result.success()) << "Bytecode generation failed";
    }
};

TEST_F(CTETest, SimpleCTE) {
    testCTE("WITH regional_sales AS (SELECT region, SUM(amount) FROM orders GROUP BY region) "
            "SELECT * FROM regional_sales");
}

TEST_F(CTETest, MultipleCTEs) {
    testCTE("WITH "
            "  sales AS (SELECT * FROM orders WHERE amount > 100), "
            "  customers AS (SELECT * FROM users WHERE active = true) "
            "SELECT * FROM sales JOIN customers ON sales.user_id = customers.id");
}

TEST_F(CTETest, CTEWithColumnAliases) {
    testCTE("WITH summary(total, count) AS (SELECT SUM(amount), COUNT(*) FROM orders) "
            "SELECT * FROM summary");
}

TEST_F(CTETest, NestedCTEReference) {
    testCTE("WITH base AS (SELECT * FROM orders), "
            "     filtered AS (SELECT * FROM base WHERE amount > 50) "
            "SELECT * FROM filtered");
}

TEST_F(CTETest, CTEInJoin) {
    testCTE("WITH recent_orders AS (SELECT * FROM orders WHERE date > '2024-01-01') "
            "SELECT u.name, r.amount FROM users u JOIN recent_orders r ON u.id = r.user_id");
}
```

### Success Criteria

- [x] Parse `WITH name AS (SELECT ...) SELECT ...`
- [x] Parse multiple CTEs
- [x] Parse CTE with column aliases
- [x] Semantic analysis validates CTE names and structure
- [x] Query planner generates CTE execution plan
- [x] Bytecode generator emits CTE opcodes
- [x] Executor materializes CTEs and executes main query
- [x] Test: Simple CTE
- [x] Test: Multiple CTEs
- [x] Test: CTE with column aliases
- [x] Test: CTE referenced multiple times
- [x] Test: Nested CTE references

### Deliverables

**Return a summary including**:
1. Files modified with line counts
2. Test results (all tests passing)
3. Example SQL queries that now work
4. Any issues encountered and how resolved

### Notes

- Follow existing code patterns (JOIN, subquery handling)
- Handle NULL properly in CTE results
- Use existing temporary storage mechanisms
- For now, materialize all CTEs (optimization later)
- **RECURSIVE CTEs are out of scope** - save for future phase

---

## Agent B: Subqueries

### Task Summary

Implement subqueries in SELECT, WHERE, and FROM clauses (scalar, IN, EXISTS, derived tables).

**Estimated Effort**: 8-12 hours agent time (60-90 hours manual)
**Expected Output**: ~920-1,280 lines across parser, semantic analyzer, planner, bytecode generator, executor

### Background

Subqueries enable nested SELECT statements:

```sql
-- Scalar subquery
SELECT name, (SELECT COUNT(*) FROM orders WHERE user_id = u.id) FROM users u;

-- IN subquery
SELECT * FROM users WHERE id IN (SELECT user_id FROM orders WHERE total > 100);

-- EXISTS subquery
SELECT * FROM users WHERE EXISTS (SELECT 1 FROM orders WHERE user_id = users.id);

-- Derived table
SELECT * FROM (SELECT * FROM orders WHERE total > 100) AS expensive;
```

### What's Already Done

✅ **Parser**: Supports nested expressions
✅ **Executor**: Can handle nested execution contexts
✅ **Planner**: Supports subplan nodes
✅ **Expression Evaluation**: Ready for scalar values

### Your Task: Implement Subqueries

#### 1. Parser Layer (~150-200 lines)

**Location**: `src/parser/parser.cpp`

**Create SubqueryExpr AST node**:
```cpp
// In include/scratchbird/parser/ast.h

enum class SubqueryType {
    SCALAR,      // Returns single value: (SELECT col FROM ...)
    EXISTS,      // Returns boolean: EXISTS (SELECT ...)
    IN,          // Membership test: col IN (SELECT ...)
    ARRAY        // Returns array: ARRAY(SELECT ...)
};

class SubqueryExpr : public Expression {
public:
    SubqueryExpr(SourceSpan span, SelectStmt* query, SubqueryType type)
        : Expression(span), query_(query), type_(type) {}

    SelectStmt* query() const { return query_; }
    SubqueryType type() const { return type_; }

    void accept(ASTVisitor* visitor) override { visitor->visit(this); }

private:
    SelectStmt* query_;
    SubqueryType type_;
};
```

**Parse subqueries in expressions**:
```cpp
// In src/parser/parser.cpp

// Update parsePrimary() to handle subqueries
Expression* Parser::parsePrimary() {
    // ... existing cases ...

    // Subquery: (SELECT ...)
    if (check(TokenType::LEFT_PAREN)) {
        auto start_loc = current().location;
        advance();  // consume '('

        // Check if it's a SELECT subquery
        if (check(TokenType::KW_SELECT)) {
            auto* subquery = parseSelectStmt();
            if (!subquery) return nullptr;

            if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after subquery")) {
                return nullptr;
            }

            auto span = makeSpan(start_loc, previous().location);
            return arena_.make<SubqueryExpr>(span, subquery, SubqueryType::SCALAR);
        }

        // Otherwise, it's a grouped expression
        auto* expr = parseExpression();
        // ... existing logic ...
    }

    // EXISTS subquery
    if (match(TokenType::KW_EXISTS)) {
        auto start_loc = previous().location;

        if (!consume(TokenType::LEFT_PAREN, "Expected '(' after EXISTS")) {
            return nullptr;
        }

        auto* subquery = parseSelectStmt();
        if (!subquery) return nullptr;

        if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after EXISTS subquery")) {
            return nullptr;
        }

        auto span = makeSpan(start_loc, previous().location);
        return arena_.make<SubqueryExpr>(span, subquery, SubqueryType::EXISTS);
    }

    // ... rest of parsePrimary ...
}

// Update parseComparison() to handle IN subquery
Expression* Parser::parseComparison() {
    auto* expr = parseAddition();
    if (!expr) return nullptr;

    auto start_loc = expr->span().start;

    while (true) {
        // ... existing comparison operators ...

        // IN operator
        if (match(TokenType::KW_IN)) {
            if (!consume(TokenType::LEFT_PAREN, "Expected '(' after IN")) {
                return nullptr;
            }

            // Check if it's a subquery
            if (check(TokenType::KW_SELECT)) {
                auto* subquery = parseSelectStmt();
                if (!subquery) return nullptr;

                if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after IN subquery")) {
                    return nullptr;
                }

                auto* subquery_expr = arena_.make<SubqueryExpr>(
                    subquery->span(), subquery, SubqueryType::IN
                );

                expr = arena_.make<BinaryOp>(
                    makeSpan(start_loc, previous().location),
                    BinaryOpType::IN, expr, subquery_expr
                );
            } else {
                // IN with value list: IN (1, 2, 3)
                // ... existing logic or implement ...
            }
            continue;
        }

        break;
    }

    return expr;
}
```

**Parse derived tables in FROM**:
```cpp
// Update parseTableRef() to handle subqueries
TableRef* Parser::parseTableRef() {
    auto start_loc = current().location;

    // Derived table: (SELECT ...) AS alias
    if (match(TokenType::LEFT_PAREN)) {
        if (check(TokenType::KW_SELECT)) {
            auto* subquery = parseSelectStmt();
            if (!subquery) return nullptr;

            if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after derived table")) {
                return nullptr;
            }

            // Alias is required for derived tables
            if (!consume(TokenType::KW_AS, "Expected AS after derived table")) {
                return nullptr;
            }

            if (!check(TokenType::IDENTIFIER)) {
                error("Expected alias name after derived table");
                return nullptr;
            }

            std::string alias = advance().lexeme;
            auto span = makeSpan(start_loc, previous().location);

            return arena_.make<DerivedTableRef>(span, subquery, alias);
        } else {
            // Not a subquery, backtrack
            // ... handle error or parenthesized table reference ...
        }
    }

    // Regular table reference
    // ... existing logic ...
}
```

**Add keywords**:
- `include/scratchbird/parser/token.h`: Add `KW_EXISTS` token
- `src/parser/lexer.cpp`: Map "EXISTS" keyword

**Reference Patterns**:
- Function call parsing (similar structure)
- Expression nesting

#### 2. Semantic Analysis (~120-180 lines)

**Location**: `src/parser/semantic_analyzer.cpp`

**Validate subqueries**:
```cpp
// Add visitor for SubqueryExpr
Result<DataType> SemanticAnalyzer::visit(SubqueryExpr* expr) {
    // Save current context
    auto saved_outer_context = current_context_;

    // Analyze subquery
    auto result = visit(expr->query());
    if (!result.success()) {
        return result;
    }

    // Validate based on subquery type
    switch (expr->type()) {
        case SubqueryType::SCALAR: {
            // Must return exactly one column
            if (result.column_count() != 1) {
                return error("Scalar subquery must return exactly one column");
            }
            // Type is the column type
            return Result<DataType>::success(result.column_types()[0]);
        }

        case SubqueryType::EXISTS: {
            // Always returns boolean
            return Result<DataType>::success(DataType::BOOLEAN);
        }

        case SubqueryType::IN: {
            // Must return exactly one column
            if (result.column_count() != 1) {
                return error("IN subquery must return exactly one column");
            }
            // Returns boolean (for IN expression)
            return Result<DataType>::success(DataType::BOOLEAN);
        }

        case SubqueryType::ARRAY: {
            // Returns array of subquery result type
            return Result<DataType>::success(DataType::ARRAY);
        }
    }

    // Restore context
    current_context_ = saved_outer_context;

    return result;
}

// Handle correlated subqueries
void SemanticAnalyzer::visit(SelectStmt* stmt) {
    // If this is a correlated subquery, outer columns are accessible
    if (is_in_subquery_ && stmt->hasCorrelatedReferences()) {
        // Validate correlated column references
        // ... check outer_context_ for column availability ...
    }

    // ... existing SELECT validation ...
}
```

**Validate IN subquery type compatibility**:
```cpp
// In visit(BinaryOp* expr) for IN operator
if (expr->op() == BinaryOpType::IN) {
    auto left_type = visit(expr->left());
    auto right_type = visit(expr->right());

    // If right is subquery, check type compatibility
    if (auto* subquery = dynamic_cast<SubqueryExpr*>(expr->right())) {
        // Subquery column type must match left expression type
        if (!typesCompatible(left_type, right_type)) {
            return error("Type mismatch in IN subquery");
        }
    }

    return Result<DataType>::success(DataType::BOOLEAN);
}
```

**Reference Patterns**:
- Nested query validation
- Type checking for expressions

#### 3. Query Planner (~200-300 lines)

**Location**: `src/optimizer/query_planner.cpp`

**Create subquery plan nodes**:
```cpp
// In include/scratchbird/optimizer/plan_node.h

class SubplanNode : public PlanNode {
public:
    SubplanNode(SubqueryType type, std::unique_ptr<PlanNode> subplan)
        : type_(type), subplan_(std::move(subplan)) {}

    SubqueryType type() const { return type_; }
    PlanNode* subplan() const { return subplan_.get(); }

    std::string toString(int indent = 0) const override {
        std::string type_str;
        switch (type_) {
            case SubqueryType::SCALAR: type_str = "Scalar"; break;
            case SubqueryType::EXISTS: type_str = "Exists"; break;
            case SubqueryType::IN: type_str = "In"; break;
        }

        std::string s = std::string(indent, ' ') + type_str + " Subquery:\n";
        s += subplan_->toString(indent + 2);
        return s;
    }

private:
    SubqueryType type_;
    std::unique_ptr<PlanNode> subplan_;
};
```

**Plan subqueries**:
```cpp
// In planExpression()
std::unique_ptr<PlanNode> QueryPlanner::planExpression(Expression* expr) {
    if (auto* subquery = dynamic_cast<SubqueryExpr*>(expr)) {
        // Recursively plan subquery
        auto subplan = planQuery(subquery->query());

        // Wrap in SubplanNode
        return std::make_unique<SubplanNode>(subquery->type(), std::move(subplan));
    }

    // ... handle other expression types ...
}

// Optimize IN subquery to hash join
std::unique_ptr<PlanNode> QueryPlanner::optimizeInSubquery(
    Expression* left_expr, SubqueryExpr* subquery) {

    // If subquery is not correlated, use hash join
    if (!subquery->isCorrelated()) {
        // Plan as semi-join with hash table
        auto subplan = planQuery(subquery->query());
        return createHashSemiJoin(left_expr, std::move(subplan));
    } else {
        // Correlated: must execute per outer row
        return createCorrelatedSubplan(left_expr, subquery);
    }
}

// Optimize EXISTS to semi-join
std::unique_ptr<PlanNode> QueryPlanner::optimizeExistsSubquery(SubqueryExpr* subquery) {
    auto subplan = planQuery(subquery->query());

    // EXISTS can short-circuit after first match
    return createExistsSemiJoin(std::move(subplan));
}
```

**Cost estimation for subqueries**:
```cpp
double CostModel::costSubquery(SubqueryType type, double outer_rows, double subquery_rows) {
    switch (type) {
        case SubqueryType::SCALAR:
            // Execute once per outer row
            return outer_rows * (subquery_rows * cpu_tuple_cost);

        case SubqueryType::EXISTS:
            // Can short-circuit, estimate 50% of rows checked
            return outer_rows * (subquery_rows * 0.5 * cpu_tuple_cost);

        case SubqueryType::IN:
            // Hash table build + probe
            return (subquery_rows * cpu_tuple_cost) +  // build hash table
                   (outer_rows * cpu_operator_cost);    // probe
    }
}
```

**Reference Patterns**:
- JOIN planning (similar to semi-join)
- Nested query cost estimation

#### 4. Bytecode Generator (~150-200 lines)

**Location**: `src/sblr/bytecode_generator.cpp`

**Add subquery opcodes**:
```cpp
// In include/scratchbird/sblr/opcodes.h
enum class Opcode : uint8_t {
    // ... existing opcodes ...

    // Subquery opcodes (0xD4-0xD8 range)
    SUBQUERY_SCALAR = 0xD4,    // Execute scalar subquery, push result
    SUBQUERY_EXISTS = 0xD5,    // Execute EXISTS subquery, push boolean
    SUBQUERY_IN = 0xD6,         // Execute IN subquery, push boolean
    SUBQUERY_START_CORR = 0xD7, // Start correlated subquery (pass parameters)
    SUBQUERY_END = 0xD8,        // End subquery
};
```

**Generate subquery bytecode**:
```cpp
void BytecodeGenerator::visit(SubqueryExpr* expr) {
    // Emit subquery start
    switch (expr->type()) {
        case SubqueryType::SCALAR:
            current_result_->writeByte(static_cast<uint8_t>(Opcode::SUBQUERY_SCALAR));
            break;
        case SubqueryType::EXISTS:
            current_result_->writeByte(static_cast<uint8_t>(Opcode::SUBQUERY_EXISTS));
            break;
        case SubqueryType::IN:
            current_result_->writeByte(static_cast<uint8_t>(Opcode::SUBQUERY_IN));
            break;
    }

    // If correlated, emit parameters
    if (expr->isCorrelated()) {
        current_result_->writeByte(static_cast<uint8_t>(Opcode::SUBQUERY_START_CORR));

        // Emit correlated parameter info
        for (const auto& param : expr->correlatedParameters()) {
            emitParameterReference(param);
        }
    }

    // Generate subquery bytecode
    expr->query()->accept(this);

    // Emit subquery end
    current_result_->writeByte(static_cast<uint8_t>(Opcode::SUBQUERY_END));
}

// For IN operator with subquery
void BytecodeGenerator::visit(BinaryOp* expr) {
    if (expr->op() == BinaryOpType::IN) {
        // Generate left expression
        expr->left()->accept(this);

        // If right is subquery, generate subquery bytecode
        if (auto* subquery = dynamic_cast<SubqueryExpr*>(expr->right())) {
            subquery->accept(this);
            // Executor will handle membership test
        } else {
            // IN with value list
            expr->right()->accept(this);
        }

        return;
    }

    // ... other binary operators ...
}
```

**Reference Patterns**:
- Function call bytecode generation
- Nested expression evaluation

#### 5. Executor (~300-400 lines)

**Location**: `src/sblr/executor.cpp`

**Implement subquery execution**:
```cpp
// In executor.cpp execute() switch statement

case Opcode::SUBQUERY_SCALAR: {
    // Save current execution context
    auto saved_context = saveContext();

    // Execute subquery (next instructions until SUBQUERY_END)
    std::vector<std::vector<Value>> result_rows;
    executeSubquery(result_rows);

    // Validate: must return exactly one row and one column
    if (result_rows.empty() || result_rows.size() > 1) {
        throw std::runtime_error("Scalar subquery must return exactly one row");
    }
    if (result_rows[0].size() != 1) {
        throw std::runtime_error("Scalar subquery must return exactly one column");
    }

    // Push scalar result onto stack
    push(result_rows[0][0]);

    // Restore context
    restoreContext(saved_context);
    break;
}

case Opcode::SUBQUERY_EXISTS: {
    auto saved_context = saveContext();

    // Execute subquery
    std::vector<std::vector<Value>> result_rows;
    executeSubquery(result_rows);

    // EXISTS returns true if any rows returned
    bool exists = !result_rows.empty();
    push(Value::makeBoolean(exists));

    restoreContext(saved_context);
    break;
}

case Opcode::SUBQUERY_IN: {
    // Pop value to test
    Value test_value = pop();

    auto saved_context = saveContext();

    // Execute subquery
    std::vector<std::vector<Value>> result_rows;
    executeSubquery(result_rows);

    // Build hash table for membership test
    std::unordered_set<Value> value_set;
    for (const auto& row : result_rows) {
        if (row.size() != 1) {
            throw std::runtime_error("IN subquery must return exactly one column");
        }
        value_set.insert(row[0]);
    }

    // Test membership
    bool is_member = (value_set.count(test_value) > 0);
    push(Value::makeBoolean(is_member));

    restoreContext(saved_context);
    break;
}

case Opcode::SUBQUERY_START_CORR: {
    // Read correlated parameters
    // Store in subquery context for use in subquery execution
    // ... implementation depends on your parameter passing mechanism ...
    break;
}

case Opcode::SUBQUERY_END: {
    // Mark end of subquery bytecode
    // (Used by executeSubquery to know when to stop)
    break;
}

// Helper method
void Executor::executeSubquery(std::vector<std::vector<Value>>& result_rows) {
    // Execute bytecode until SUBQUERY_END
    while (true) {
        uint8_t opcode = readByte();

        if (opcode == static_cast<uint8_t>(Opcode::SUBQUERY_END)) {
            break;
        }

        // Execute opcode
        executeOpcode(static_cast<Opcode>(opcode));

        // If opcode produced a row, collect it
        if (hasRowResult()) {
            result_rows.push_back(popRow());
        }
    }
}
```

**NULL handling for IN/EXISTS**:
```cpp
// IN with NULL semantics
// NULL IN (1, 2, 3) => NULL
// 1 IN (NULL, 2, 3) => NULL if 1 not in set, otherwise true
bool Executor::evaluateInSubquery(const Value& test_value,
                                   const std::vector<std::vector<Value>>& rows) {
    if (test_value.isNull()) {
        return null_result;  // Return NULL
    }

    bool has_null = false;
    for (const auto& row : rows) {
        if (row[0].isNull()) {
            has_null = true;
            continue;
        }

        if (test_value == row[0]) {
            return true;
        }
    }

    // If not found and no NULLs, return false
    // If not found and has NULLs, return NULL
    return has_null ? null_result : false;
}
```

**Reference Patterns**:
- Nested execution contexts
- Result set handling

#### 6. Testing (~150-200 lines)

**Create**: `tests/unit/test_subquery.cpp`

```cpp
#include <gtest/gtest.h>
#include "scratchbird/parser/parser.h"
#include "scratchbird/sblr/bytecode_generator.h"

class SubqueryTest : public ::testing::Test {
protected:
    void testSubquery(const std::string& sql) {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "Parse failed: " << sql;

        BytecodeGenerator generator(parser.stringPool());
        auto bytecode_result = generator.generate(result.statement());
        EXPECT_TRUE(bytecode_result.success()) << "Bytecode generation failed";
    }
};

TEST_F(SubqueryTest, ScalarSubquery) {
    testSubquery("SELECT name, (SELECT COUNT(*) FROM orders WHERE user_id = u.id) "
                 "FROM users u");
}

TEST_F(SubqueryTest, InSubquery) {
    testSubquery("SELECT * FROM users WHERE id IN (SELECT user_id FROM orders)");
}

TEST_F(SubqueryTest, ExistsSubquery) {
    testSubquery("SELECT * FROM users WHERE EXISTS "
                 "(SELECT 1 FROM orders WHERE orders.user_id = users.id)");
}

TEST_F(SubqueryTest, DerivedTable) {
    testSubquery("SELECT * FROM (SELECT * FROM orders WHERE total > 100) AS expensive");
}

TEST_F(SubqueryTest, CorrelatedSubquery) {
    testSubquery("SELECT u.name, "
                 "(SELECT MAX(amount) FROM orders o WHERE o.user_id = u.id) as max_order "
                 "FROM users u");
}

TEST_F(SubqueryTest, NestedSubqueries) {
    testSubquery("SELECT * FROM users WHERE id IN "
                 "(SELECT user_id FROM orders WHERE total > "
                 "  (SELECT AVG(total) FROM orders))");
}

TEST_F(SubqueryTest, SubqueryInHaving) {
    testSubquery("SELECT user_id, COUNT(*) FROM orders GROUP BY user_id "
                 "HAVING COUNT(*) > (SELECT AVG(order_count) FROM user_stats)");
}

TEST_F(SubqueryTest, NotInSubquery) {
    testSubquery("SELECT * FROM users WHERE id NOT IN (SELECT user_id FROM blocked_users)");
}

TEST_F(SubqueryTest, MultipleSubqueries) {
    testSubquery("SELECT "
                 "(SELECT COUNT(*) FROM orders) as order_count, "
                 "(SELECT COUNT(*) FROM users) as user_count");
}
```

### Success Criteria

- [x] Parse scalar subqueries in SELECT
- [x] Parse IN subqueries in WHERE
- [x] Parse EXISTS subqueries in WHERE
- [x] Parse derived tables in FROM
- [x] Detect correlated subqueries
- [x] Semantic analysis validates subquery structure
- [x] Query planner generates subplan nodes
- [x] Executor executes all subquery types
- [x] NULL semantics for IN/EXISTS work correctly
- [x] Test: Scalar subquery
- [x] Test: IN subquery
- [x] Test: EXISTS subquery
- [x] Test: Derived table
- [x] Test: Correlated subquery
- [x] Test: Nested subqueries

### Deliverables

**Return a summary including**:
1. Files modified with line counts
2. Test results
3. Example SQL queries working
4. Performance notes (correlated vs. uncorrelated)

### Notes

- Start with uncorrelated subqueries (simpler)
- Correlated subqueries execute per outer row
- IN/EXISTS should optimize to semi-joins when possible
- Handle NULL semantics correctly (especially for IN)
- **Decorrelation optimization is out of scope** - save for future

---

## Agent C: Basic Triggers

### Task Summary

Implement BEFORE/AFTER INSERT/UPDATE/DELETE triggers with FOR EACH ROW granularity.

**Estimated Effort**: 10-15 hours agent time (80-120 hours manual)
**Expected Output**: ~740-1,050 lines across parser, catalog, semantic analyzer, bytecode generator, executor

### Background

Triggers execute automatically when data changes:

```sql
CREATE TRIGGER audit_log
AFTER UPDATE ON users
FOR EACH ROW
EXECUTE PROCEDURE log_user_change();
```

### What's Already Done

✅ **Parser Framework**: Ready for CREATE TRIGGER
✅ **Catalog System**: Can store trigger definitions
✅ **Executor**: Has insert/update/delete methods
✅ **MGA/MVCC**: Handles data modifications

### Your Task: Implement Basic Triggers

#### 1. Parser Layer (~100-150 lines)

**Location**: `src/parser/`

**Add trigger keywords**:
```cpp
// In include/scratchbird/parser/token.h
enum class TokenType {
    // ... existing tokens ...

    // Trigger keywords
    KW_TRIGGER,
    KW_BEFORE,
    KW_AFTER,
    KW_EACH,
    KW_ROW,
    KW_EXECUTE,
    KW_PROCEDURE,
    KW_OLD,
    KW_NEW,
};

// In src/parser/lexer.cpp
keyword_map_ = {
    // ... existing keywords ...
    {"TRIGGER", TokenType::KW_TRIGGER},
    {"BEFORE", TokenType::KW_BEFORE},
    {"AFTER", TokenType::KW_AFTER},
    {"EACH", TokenType::KW_EACH},
    {"ROW", TokenType::KW_ROW},
    {"EXECUTE", TokenType::KW_EXECUTE},
    {"PROCEDURE", TokenType::KW_PROCEDURE},
    {"OLD", TokenType::KW_OLD},
    {"NEW", TokenType::KW_NEW},
};
```

**Create trigger AST node**:
```cpp
// In include/scratchbird/parser/ast.h

enum class TriggerTiming {
    BEFORE,
    AFTER
};

enum class TriggerEvent {
    INSERT,
    UPDATE,
    DELETE
};

enum class TriggerGranularity {
    FOR_EACH_ROW,
    FOR_EACH_STATEMENT  // Optional, can defer
};

class CreateTriggerStmt : public Statement {
public:
    CreateTriggerStmt(
        SourceSpan span,
        std::string trigger_name,
        std::string table_name,
        TriggerTiming timing,
        TriggerEvent event,
        TriggerGranularity granularity,
        std::string procedure_name
    ) : Statement(span),
        trigger_name_(std::move(trigger_name)),
        table_name_(std::move(table_name)),
        timing_(timing),
        event_(event),
        granularity_(granularity),
        procedure_name_(std::move(procedure_name)) {}

    const std::string& triggerName() const { return trigger_name_; }
    const std::string& tableName() const { return table_name_; }
    TriggerTiming timing() const { return timing_; }
    TriggerEvent event() const { return event_; }
    TriggerGranularity granularity() const { return granularity_; }
    const std::string& procedureName() const { return procedure_name_; }

    void accept(ASTVisitor* visitor) override { visitor->visit(this); }

private:
    std::string trigger_name_;
    std::string table_name_;
    TriggerTiming timing_;
    TriggerEvent event_;
    TriggerGranularity granularity_;
    std::string procedure_name_;
};
```

**Parse CREATE TRIGGER**:
```cpp
// In src/parser/parser.cpp

// Add to parseStatement()
Statement* Parser::parseStatement() {
    // ... existing cases ...

    if (match(TokenType::KW_CREATE)) {
        if (match(TokenType::KW_TRIGGER)) {
            return parseCreateTrigger();
        }
        // ... other CREATE statements ...
    }

    // ... rest of parseStatement ...
}

CreateTriggerStmt* Parser::parseCreateTrigger() {
    auto start_loc = previous().location;

    // Trigger name
    if (!check(TokenType::IDENTIFIER)) {
        error("Expected trigger name");
        return nullptr;
    }
    std::string trigger_name = advance().lexeme;

    // Timing: BEFORE or AFTER
    TriggerTiming timing;
    if (match(TokenType::KW_BEFORE)) {
        timing = TriggerTiming::BEFORE;
    } else if (match(TokenType::KW_AFTER)) {
        timing = TriggerTiming::AFTER;
    } else {
        error("Expected BEFORE or AFTER");
        return nullptr;
    }

    // Event: INSERT, UPDATE, or DELETE
    TriggerEvent event;
    if (match(TokenType::KW_INSERT)) {
        event = TriggerEvent::INSERT;
    } else if (match(TokenType::KW_UPDATE)) {
        event = TriggerEvent::UPDATE;
    } else if (match(TokenType::KW_DELETE)) {
        event = TriggerEvent::DELETE;
    } else {
        error("Expected INSERT, UPDATE, or DELETE");
        return nullptr;
    }

    // ON table_name
    if (!consume(TokenType::KW_ON, "Expected ON after trigger event")) {
        return nullptr;
    }

    if (!check(TokenType::IDENTIFIER)) {
        error("Expected table name");
        return nullptr;
    }
    std::string table_name = advance().lexeme;

    // FOR EACH ROW
    if (!consume(TokenType::KW_FOR, "Expected FOR EACH ROW")) {
        return nullptr;
    }
    if (!consume(TokenType::KW_EACH, "Expected FOR EACH ROW")) {
        return nullptr;
    }
    if (!consume(TokenType::KW_ROW, "Expected FOR EACH ROW")) {
        return nullptr;
    }

    TriggerGranularity granularity = TriggerGranularity::FOR_EACH_ROW;

    // EXECUTE PROCEDURE procedure_name()
    if (!consume(TokenType::KW_EXECUTE, "Expected EXECUTE PROCEDURE")) {
        return nullptr;
    }
    if (!consume(TokenType::KW_PROCEDURE, "Expected EXECUTE PROCEDURE")) {
        return nullptr;
    }

    if (!check(TokenType::IDENTIFIER)) {
        error("Expected procedure name");
        return nullptr;
    }
    std::string procedure_name = advance().lexeme;

    // Optional parentheses: procedure_name()
    if (match(TokenType::LEFT_PAREN)) {
        if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after procedure name")) {
            return nullptr;
        }
    }

    // Optional semicolon
    match(TokenType::SEMICOLON);

    auto span = makeSpan(start_loc, previous().location);
    return arena_.make<CreateTriggerStmt>(
        span, trigger_name, table_name, timing, event, granularity, procedure_name
    );
}
```

**Reference Patterns**:
- CREATE TABLE parsing
- Statement parsing structure

#### 2. Catalog Layer (~150-200 lines)

**Location**: `src/core/catalog_manager.cpp`

**Create trigger catalog structure**:
```cpp
// In include/scratchbird/core/catalog_manager.h

struct TriggerDefinition {
    std::string trigger_name;
    std::string table_name;
    TriggerTiming timing;
    TriggerEvent event;
    TriggerGranularity granularity;
    std::string procedure_name;
    bool enabled;  // Can be disabled without dropping

    // Metadata
    uint64_t trigger_oid;  // Unique ID
    uint64_t created_at;
};

class CatalogManager {
public:
    // ... existing methods ...

    // Trigger management
    Result<void> createTrigger(const TriggerDefinition& trigger);
    Result<void> dropTrigger(const std::string& trigger_name);
    Result<std::vector<TriggerDefinition>> getTriggersForTable(
        const std::string& table_name,
        TriggerEvent event,
        TriggerTiming timing
    );
    Result<TriggerDefinition> getTrigger(const std::string& trigger_name);
    Result<void> enableTrigger(const std::string& trigger_name, bool enable);

private:
    // Trigger storage (in-memory for Phase 2)
    std::unordered_map<std::string, TriggerDefinition> triggers_;
    std::unordered_multimap<std::string, std::string> table_triggers_;  // table_name -> trigger_name

    std::mutex trigger_mutex_;
};
```

**Implement trigger catalog methods**:
```cpp
// In src/core/catalog_manager.cpp

Result<void> CatalogManager::createTrigger(const TriggerDefinition& trigger) {
    std::lock_guard<std::mutex> lock(trigger_mutex_);

    // Check trigger name is unique
    if (triggers_.count(trigger.trigger_name) > 0) {
        return Result<void>::error("Trigger '" + trigger.trigger_name + "' already exists");
    }

    // Check table exists
    auto table_result = getTable(trigger.table_name);
    if (!table_result.success()) {
        return Result<void>::error("Table '" + trigger.table_name + "' does not exist");
    }

    // Assign trigger OID
    TriggerDefinition trigger_copy = trigger;
    trigger_copy.trigger_oid = next_oid_++;
    trigger_copy.created_at = getCurrentTimestamp();
    trigger_copy.enabled = true;

    // Store trigger
    triggers_[trigger.trigger_name] = trigger_copy;
    table_triggers_.insert({trigger.table_name, trigger.trigger_name});

    return Result<void>::success();
}

Result<std::vector<TriggerDefinition>> CatalogManager::getTriggersForTable(
    const std::string& table_name,
    TriggerEvent event,
    TriggerTiming timing) {

    std::lock_guard<std::mutex> lock(trigger_mutex_);

    std::vector<TriggerDefinition> result;

    // Find all triggers for this table
    auto range = table_triggers_.equal_range(table_name);
    for (auto it = range.first; it != range.second; ++it) {
        const auto& trigger = triggers_.at(it->second);

        // Filter by event and timing
        if (trigger.event == event && trigger.timing == timing && trigger.enabled) {
            result.push_back(trigger);
        }
    }

    return Result<std::vector<TriggerDefinition>>::success(result);
}

Result<void> CatalogManager::dropTrigger(const std::string& trigger_name) {
    std::lock_guard<std::mutex> lock(trigger_mutex_);

    auto it = triggers_.find(trigger_name);
    if (it == triggers_.end()) {
        return Result<void>::error("Trigger '" + trigger_name + "' does not exist");
    }

    // Remove from table index
    const auto& table_name = it->second.table_name;
    auto range = table_triggers_.equal_range(table_name);
    for (auto tit = range.first; tit != range.second; ++tit) {
        if (tit->second == trigger_name) {
            table_triggers_.erase(tit);
            break;
        }
    }

    // Remove trigger
    triggers_.erase(it);

    return Result<void>::success();
}
```

**Reference Patterns**:
- Table catalog management
- Catalog locking patterns

#### 3. Semantic Analysis (~80-120 lines)

**Location**: `src/parser/semantic_analyzer.cpp`

**Validate CREATE TRIGGER**:
```cpp
// Add visitor for CreateTriggerStmt
Result<void> SemanticAnalyzer::visit(CreateTriggerStmt* stmt) {
    // Validate table exists
    auto table_result = catalog_->getTable(stmt->tableName());
    if (!table_result.success()) {
        return error("Table '" + stmt->tableName() + "' does not exist");
    }

    // Validate timing/event combination
    // (All combinations are valid for basic triggers)

    // Check if trigger name already exists
    auto trigger_result = catalog_->getTrigger(stmt->triggerName());
    if (trigger_result.success()) {
        return error("Trigger '" + stmt->triggerName() + "' already exists");
    }

    // For now, we don't validate procedure exists
    // (Will be checked at execution time or when procedures are implemented)

    return Result<void>::success();
}
```

**Reference Patterns**:
- CREATE TABLE validation
- Catalog lookup patterns

#### 4. Bytecode Generator (~60-80 lines)

**Location**: `src/sblr/bytecode_generator.cpp`

**Add trigger opcodes**:
```cpp
// In include/scratchbird/sblr/opcodes.h
enum class Opcode : uint8_t {
    // ... existing opcodes ...

    // Trigger opcodes (0xD9-0xDA range)
    CREATE_TRIGGER = 0xD9,
    DROP_TRIGGER = 0xDA,
};
```

**Generate CREATE TRIGGER bytecode**:
```cpp
void BytecodeGenerator::visit(CreateTriggerStmt* stmt) {
    // Emit CREATE_TRIGGER opcode
    current_result_->writeByte(static_cast<uint8_t>(Opcode::CREATE_TRIGGER));

    // Emit trigger name
    auto trigger_name_id = string_pool_.addString(stmt->triggerName());
    current_result_->writeUInt32(trigger_name_id);

    // Emit table name
    auto table_name_id = string_pool_.addString(stmt->tableName());
    current_result_->writeUInt32(table_name_id);

    // Emit timing (1 byte)
    current_result_->writeByte(static_cast<uint8_t>(stmt->timing()));

    // Emit event (1 byte)
    current_result_->writeByte(static_cast<uint8_t>(stmt->event()));

    // Emit granularity (1 byte)
    current_result_->writeByte(static_cast<uint8_t>(stmt->granularity()));

    // Emit procedure name
    auto proc_name_id = string_pool_.addString(stmt->procedureName());
    current_result_->writeUInt32(proc_name_id);
}
```

**Reference Patterns**:
- CREATE TABLE bytecode generation
- String pool usage

#### 5. Executor (~250-350 lines)

**Location**: `src/sblr/executor.cpp`

**Implement CREATE TRIGGER execution**:
```cpp
// In executor.cpp execute() switch statement

case Opcode::CREATE_TRIGGER: {
    // Read trigger definition
    uint32_t trigger_name_id = readUInt32();
    std::string trigger_name = string_pool_.getString(trigger_name_id);

    uint32_t table_name_id = readUInt32();
    std::string table_name = string_pool_.getString(table_name_id);

    auto timing = static_cast<TriggerTiming>(readByte());
    auto event = static_cast<TriggerEvent>(readByte());
    auto granularity = static_cast<TriggerGranularity>(readByte());

    uint32_t proc_name_id = readUInt32();
    std::string procedure_name = string_pool_.getString(proc_name_id);

    // Create trigger definition
    TriggerDefinition trigger{
        trigger_name,
        table_name,
        timing,
        event,
        granularity,
        procedure_name,
        true  // enabled
    };

    // Store in catalog
    auto result = catalog_->createTrigger(trigger);
    if (!result.success()) {
        throw std::runtime_error("Failed to create trigger: " + result.error());
    }

    break;
}
```

**Add trigger firing to data modification operations**:
```cpp
// In executeInsert()
void Executor::executeInsert(const std::string& table_name,
                              const std::vector<Value>& values) {
    // Fire BEFORE INSERT triggers
    auto before_triggers = catalog_->getTriggersForTable(
        table_name, TriggerEvent::INSERT, TriggerTiming::BEFORE
    );

    if (before_triggers.success()) {
        for (const auto& trigger : before_triggers.value()) {
            bool should_continue = fireTrigger(trigger, nullptr, &values);
            if (!should_continue) {
                // BEFORE trigger prevented operation
                return;
            }
        }
    }

    // Perform insert
    // ... existing insert logic ...

    // Fire AFTER INSERT triggers
    auto after_triggers = catalog_->getTriggersForTable(
        table_name, TriggerEvent::INSERT, TriggerTiming::AFTER
    );

    if (after_triggers.success()) {
        for (const auto& trigger : after_triggers.value()) {
            fireTrigger(trigger, nullptr, &values);
        }
    }
}

// Similar for executeUpdate and executeDelete
void Executor::executeUpdate(const std::string& table_name,
                              const std::vector<Value>& old_values,
                              const std::vector<Value>& new_values) {
    // Fire BEFORE UPDATE triggers
    auto before_triggers = catalog_->getTriggersForTable(
        table_name, TriggerEvent::UPDATE, TriggerTiming::BEFORE
    );

    if (before_triggers.success()) {
        for (const auto& trigger : before_triggers.value()) {
            bool should_continue = fireTrigger(trigger, &old_values, &new_values);
            if (!should_continue) {
                return;  // Prevented
            }
        }
    }

    // Perform update
    // ... existing update logic ...

    // Fire AFTER UPDATE triggers
    auto after_triggers = catalog_->getTriggersForTable(
        table_name, TriggerEvent::UPDATE, TriggerTiming::AFTER
    );

    if (after_triggers.success()) {
        for (const auto& trigger : after_triggers.value()) {
            fireTrigger(trigger, &old_values, &new_values);
        }
    }
}
```

**Implement trigger execution**:
```cpp
// Create trigger context class
class TriggerContext {
public:
    TriggerContext(
        const TriggerDefinition& trigger,
        const std::vector<Value>* old_row,
        const std::vector<Value>* new_row
    ) : trigger_(trigger), old_row_(old_row), new_row_(new_row) {}

    const TriggerDefinition& trigger() const { return trigger_; }
    const std::vector<Value>* oldRow() const { return old_row_; }
    const std::vector<Value>* newRow() const { return new_row_; }

    // Access OLD.column_name or NEW.column_name
    Value getOldValue(size_t column_index) const {
        if (!old_row_) return Value::makeNull();
        return (*old_row_)[column_index];
    }

    Value getNewValue(size_t column_index) const {
        if (!new_row_) return Value::makeNull();
        return (*new_row_)[column_index];
    }

private:
    const TriggerDefinition& trigger_;
    const std::vector<Value>* old_row_;
    const std::vector<Value>* new_row_;
};

// Fire trigger (basic implementation for Phase 2)
bool Executor::fireTrigger(
    const TriggerDefinition& trigger,
    const std::vector<Value>* old_row,
    const std::vector<Value>* new_row) {

    // Create trigger context
    TriggerContext context(trigger, old_row, new_row);

    // For Phase 2: Simple callback mechanism
    // Look up registered trigger procedures
    auto procedure_it = trigger_procedures_.find(trigger.procedure_name);
    if (procedure_it == trigger_procedures_.end()) {
        // Procedure not found - for now, just log warning
        std::cerr << "Warning: Trigger procedure '"
                  << trigger.procedure_name << "' not found\n";
        return true;  // Continue operation
    }

    // Execute procedure callback
    try {
        bool should_continue = procedure_it->second(context);
        return should_continue;
    } catch (const std::exception& e) {
        std::cerr << "Trigger '" << trigger.trigger_name
                  << "' failed: " << e.what() << "\n";

        // For BEFORE triggers, failure prevents operation
        if (trigger.timing == TriggerTiming::BEFORE) {
            return false;
        }

        // For AFTER triggers, log error but continue
        return true;
    }
}

// Trigger procedure registry (for Phase 2 testing)
using TriggerProcedure = std::function<bool(const TriggerContext&)>;
std::unordered_map<std::string, TriggerProcedure> trigger_procedures_;

// Method to register trigger procedures (for testing)
void Executor::registerTriggerProcedure(
    const std::string& name,
    TriggerProcedure procedure) {
    trigger_procedures_[name] = std::move(procedure);
}
```

**Reference Patterns**:
- INSERT/UPDATE/DELETE execution
- Callback mechanism

#### 6. Testing (~100-150 lines)

**Create**: `tests/unit/test_triggers.cpp`

```cpp
#include <gtest/gtest.h>
#include "scratchbird/parser/parser.h"
#include "scratchbird/sblr/executor.h"

class TriggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test database and catalog
        // ... initialization ...
    }

    void testTrigger(const std::string& sql) {
        Lexer lexer(sql);
        ASTArena arena;
        Parser parser(lexer, arena);

        auto result = parser.parseStatement();
        ASSERT_TRUE(result.success()) << "Parse failed: " << sql;

        BytecodeGenerator generator(parser.stringPool());
        auto bytecode_result = generator.generate(result.statement());
        EXPECT_TRUE(bytecode_result.success());
    }
};

TEST_F(TriggerTest, CreateTriggerAfterInsert) {
    testTrigger("CREATE TRIGGER log_insert AFTER INSERT ON users "
                "FOR EACH ROW EXECUTE PROCEDURE log_user_insert()");
}

TEST_F(TriggerTest, CreateTriggerBeforeUpdate) {
    testTrigger("CREATE TRIGGER validate_update BEFORE UPDATE ON users "
                "FOR EACH ROW EXECUTE PROCEDURE validate_user_update()");
}

TEST_F(TriggerTest, CreateTriggerAfterDelete) {
    testTrigger("CREATE TRIGGER audit_delete AFTER DELETE ON users "
                "FOR EACH ROW EXECUTE PROCEDURE audit_user_delete()");
}

TEST_F(TriggerTest, TriggerFiring) {
    // Parse and execute CREATE TRIGGER
    // ... create trigger ...

    // Register test procedure
    bool procedure_called = false;
    executor.registerTriggerProcedure("test_proc",
        [&](const TriggerContext& ctx) {
            procedure_called = true;
            // Verify OLD/NEW values are accessible
            EXPECT_TRUE(ctx.newRow() != nullptr);
            return true;  // Allow operation
        });

    // Execute INSERT (should fire trigger)
    executor.executeInsert("test_table", {Value::makeInteger(1), Value::makeVarChar("test")});

    EXPECT_TRUE(procedure_called);
}

TEST_F(TriggerTest, BeforeTriggerPreventsOperation) {
    // Register procedure that returns false
    executor.registerTriggerProcedure("prevent_insert",
        [](const TriggerContext& ctx) {
            return false;  // Prevent operation
        });

    // Create BEFORE trigger
    // ... create trigger ...

    // Attempt insert
    executor.executeInsert("test_table", {Value::makeInteger(1)});

    // Verify row was NOT inserted
    // ... check table ...
}

TEST_F(TriggerTest, OldAndNewValues) {
    // Register procedure that accesses OLD/NEW
    executor.registerTriggerProcedure("check_values",
        [](const TriggerContext& ctx) {
            // For UPDATE, both OLD and NEW should be available
            EXPECT_TRUE(ctx.oldRow() != nullptr);
            EXPECT_TRUE(ctx.newRow() != nullptr);

            auto old_val = ctx.getOldValue(0);
            auto new_val = ctx.getNewValue(0);

            EXPECT_NE(old_val, new_val);
            return true;
        });

    // Create BEFORE UPDATE trigger
    // ... create trigger ...

    // Execute UPDATE
    // ... update statement ...
}

TEST_F(TriggerTest, MultipleTriggers) {
    // Create multiple triggers on same table
    testTrigger("CREATE TRIGGER trigger1 BEFORE INSERT ON users "
                "FOR EACH ROW EXECUTE PROCEDURE proc1()");
    testTrigger("CREATE TRIGGER trigger2 AFTER INSERT ON users "
                "FOR EACH ROW EXECUTE PROCEDURE proc2()");

    // Both should fire on INSERT
    // ... test execution order ...
}
```

### Success Criteria

- [x] Parse CREATE TRIGGER statement
- [x] Store trigger definitions in catalog
- [x] Fire BEFORE INSERT triggers
- [x] Fire AFTER INSERT triggers
- [x] Fire BEFORE UPDATE triggers
- [x] Fire AFTER UPDATE triggers
- [x] Fire BEFORE DELETE triggers
- [x] Fire AFTER DELETE triggers
- [x] BEFORE triggers can prevent operation
- [x] Access OLD values (UPDATE/DELETE)
- [x] Access NEW values (INSERT/UPDATE)
- [x] Test: CREATE TRIGGER parses correctly
- [x] Test: Trigger fires on data modification
- [x] Test: BEFORE trigger prevents operation
- [x] Test: Multiple triggers fire in order

### Deliverables

**Return a summary including**:
1. Files modified with line counts
2. Test results
3. Example trigger usage
4. Notes on trigger procedure mechanism

### Notes

- Phase 2 uses **simple callback mechanism** for procedures
- Full procedural language (PL/ScratchBird) deferred to Phase 2.2
- Triggers execute in **creation order** (for same timing/event)
- BEFORE triggers that return false **prevent the operation**
- AFTER triggers cannot prevent the operation
- Thread-safety: Triggers execute within transaction context
- **FOR EACH STATEMENT triggers are out of scope** - only FOR EACH ROW

---

## Launch Coordination

### Parallel Execution

All three agents will work independently and can be launched in parallel:

**No Dependencies**:
- CTEs don't depend on subqueries or triggers
- Subqueries don't depend on CTEs or triggers
- Triggers don't depend on CTEs or subqueries

**Shared Resources**:
- All modify parser/executor (but different parts)
- All add opcodes (different ranges reserved)
- Minimal merge conflicts expected

### Success Metrics

**Per-Agent Metrics**:
- All tests passing
- Code compiles without errors
- Example SQL queries work
- Production-quality code (follows existing patterns)

**Wave 2 Overall Metrics**:
- 2,290-3,250 lines delivered
- 60+ tests passing
- 24-37 hours agent time (~87% savings)
- All three features integrated and functional

---

## Ready to Launch!

All three agent specifications are complete. Each agent has:
- ✅ Clear scope and objectives
- ✅ Detailed implementation guidance
- ✅ Code examples and patterns
- ✅ Reference to existing code
- ✅ Comprehensive test requirements
- ✅ Success criteria

**Launch all three agents in parallel for maximum efficiency!**
