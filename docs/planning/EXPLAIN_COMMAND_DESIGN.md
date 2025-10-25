# EXPLAIN Command Design
**Phase**: 1.1 - Query Optimizer Foundation
**Task**: 1.5 - EXPLAIN Command
**Estimated Effort**: 10-15 hours
**Created**: October 25, 2025
**Status**: In Progress

---

## Overview

The EXPLAIN command allows developers to view the query execution plan chosen by the optimizer, including cost estimates, row counts, and access methods. This is essential for:
- Understanding query optimization decisions
- Debugging slow queries
- Tuning query performance
- Validating optimizer behavior

---

## Goals

1. **Display Query Plans**: Show the execution plan tree with indentation
2. **Show Cost Estimates**: Display startup cost and total cost for each node
3. **Show Row Estimates**: Display estimated number of rows returned
4. **Show Access Methods**: Indicate SeqScan vs IndexScan and which index
5. **Show Filters**: Display WHERE clause conditions applied at each node
6. **PostgreSQL Compatibility**: Use familiar PostgreSQL-style output format

---

## SQL Syntax

### Basic EXPLAIN
```sql
EXPLAIN SELECT * FROM users WHERE id > 10;
```

### EXPLAIN with Options (Future)
```sql
EXPLAIN ANALYZE SELECT * FROM users WHERE id > 10;  -- Execute and show actual times
EXPLAIN VERBOSE SELECT * FROM users WHERE id > 10;  -- Show detailed plan info
EXPLAIN (FORMAT JSON) SELECT ...;                   -- JSON output format
```

**Phase 1.5 Scope**: Basic EXPLAIN only (text format, no ANALYZE, no VERBOSE)

---

## Output Format

### Example 1: Sequential Scan
```
EXPLAIN SELECT * FROM users WHERE age > 25;

                        QUERY PLAN
--------------------------------------------------------
SeqScan on users  (cost=0.00..225.00 rows=1000)
  Filter: age > 25
```

### Example 2: Index Scan
```
EXPLAIN SELECT * FROM users WHERE id = 42;

                        QUERY PLAN
--------------------------------------------------------
IndexScan on users using idx_users_id  (cost=0.01..8.03 rows=1)
  Index Cond: id = 42
```

### Example 3: Index Scan with Filter
```
EXPLAIN SELECT * FROM users WHERE id > 100 AND age > 25;

                        QUERY PLAN
--------------------------------------------------------
IndexScan on users using idx_users_id  (cost=0.01..45.67 rows=50)
  Index Cond: id > 100
  Filter: age > 25
```

### Example 4: Future - Join Plan
```
EXPLAIN SELECT * FROM users u JOIN orders o ON u.id = o.user_id;

                        QUERY PLAN
--------------------------------------------------------
NestedLoop  (cost=0.15..234.56 rows=1000)
  ->  SeqScan on users u  (cost=0.00..100.00 rows=1000)
  ->  IndexScan on orders o using idx_orders_user_id  (cost=0.15..0.13 rows=1)
        Index Cond: user_id = u.id
```

---

## Architecture

### Component Flow

```
SQL: EXPLAIN SELECT ...
      ↓
Parser: Recognizes EXPLAIN, parses SELECT
      ↓
AST: ExplainStmt { query: SelectStmt }
      ↓
BytecodeGenerator: Calls query planner, generates plan
      ↓
QueryPlanner: Returns PlanNode tree with costs
      ↓
ExplainFormatter: Converts PlanNode to text
      ↓
Output: Formatted EXPLAIN text
```

### Key Components

1. **Parser**: Recognize `EXPLAIN` keyword, parse SELECT statement
2. **AST**: `ExplainStmt` node containing the query to explain
3. **BytecodeGenerator**: Special handling for EXPLAIN (no bytecode generation)
4. **ExplainFormatter**: Convert PlanNode tree to human-readable text
5. **PlanNode**: Already has `toString()` method - enhance if needed

---

## Implementation Plan

### Part 1: Parser Support (3-4 hours)

#### 1.1 Add EXPLAIN Token
**File**: `include/scratchbird/parser/lexer.h`
- Add `EXPLAIN` keyword to token list

#### 1.2 Implement ExplainStmt AST Node
**File**: `include/scratchbird/parser/ast.h`

```cpp
class ExplainStmt : public Statement
{
public:
    ExplainStmt(Statement *query)
        : Statement(ASTKind::EXPLAIN), query_(query) {}

    Statement *query() const { return query_; }

    void accept(ASTVisitor *visitor) override {
        visitor->visit(this);
    }

private:
    Statement *query_;  // The statement to explain (SELECT, INSERT, etc.)
};
```

#### 1.3 Update ASTVisitor
**File**: `include/scratchbird/parser/ast.h`
- Add `virtual void visit(ExplainStmt *node) = 0;` to ASTVisitor interface

#### 1.4 Implement Parser::parseExplain()
**File**: `src/parser/parser.cpp`

```cpp
auto Parser::parseExplain() -> ParseResult
{
    // EXPLAIN SELECT ...
    expectKeyword("EXPLAIN");

    // Parse the query to explain
    auto query_result = parseStatement();
    if (!query_result.success()) {
        return query_result;
    }

    // Only SELECT supported in Phase 1.5
    if (query_result.statement()->kind() != ASTKind::SELECT) {
        addError("EXPLAIN only supports SELECT statements");
        return ParseResult::error(errors_);
    }

    auto *explain_stmt = arena_.alloc<ExplainStmt>(query_result.statement());
    return ParseResult::success(explain_stmt);
}
```

**Deliverable**: Parser recognizes `EXPLAIN SELECT ...` syntax

---

### Part 2: EXPLAIN Formatter (4-5 hours)

#### 2.1 Create ExplainFormatter Class
**File**: `include/scratchbird/optimizer/explain_formatter.h`

```cpp
namespace scratchbird::optimizer {

/**
 * ExplainFormatter - Convert PlanNode tree to EXPLAIN text output
 *
 * Generates PostgreSQL-style EXPLAIN output showing:
 * - Node type (SeqScan, IndexScan, etc.)
 * - Target table/index names
 * - Cost estimates (startup..total)
 * - Row estimates
 * - Filters and conditions
 */
class ExplainFormatter
{
public:
    /**
     * Format a plan node tree as EXPLAIN text
     *
     * @param plan Root of plan node tree
     * @return Formatted EXPLAIN output
     */
    static std::string formatPlan(const PlanNode *plan);

private:
    /**
     * Format a single plan node
     *
     * @param plan Plan node to format
     * @param indent Indentation level (0 = root)
     * @return Formatted lines for this node
     */
    static std::string formatNode(const PlanNode *plan, int indent);

    /**
     * Format cost and row estimates
     *
     * @param startup_cost Startup cost
     * @param total_cost Total cost
     * @param rows Estimated rows
     * @return Formatted string like "(cost=0.00..100.00 rows=1000)"
     */
    static std::string formatCostRows(double startup_cost, double total_cost, uint64_t rows);
};

} // namespace scratchbird::optimizer
```

#### 2.2 Implement ExplainFormatter
**File**: `src/optimizer/explain_formatter.cpp`

```cpp
#include "scratchbird/optimizer/explain_formatter.h"
#include "scratchbird/optimizer/plan_node.h"
#include <sstream>
#include <iomanip>

namespace scratchbird::optimizer {

std::string ExplainFormatter::formatPlan(const PlanNode *plan)
{
    if (!plan) {
        return "No plan available";
    }

    std::stringstream ss;
    ss << "                        QUERY PLAN\n";
    ss << "--------------------------------------------------------\n";
    ss << formatNode(plan, 0);

    return ss.str();
}

std::string ExplainFormatter::formatNode(const PlanNode *plan, int indent)
{
    std::stringstream ss;

    // Indentation (2 spaces per level)
    std::string prefix(indent * 2, ' ');

    switch (plan->type())
    {
    case PlanNodeType::SEQ_SCAN:
    {
        auto *seq_scan = static_cast<const SeqScanNode*>(plan);
        ss << prefix << "SeqScan on " << seq_scan->tableName();
        ss << "  " << formatCostRows(plan->startupCost(), plan->totalCost(), plan->rows());
        ss << "\n";

        if (!seq_scan->filter().empty()) {
            ss << prefix << "  Filter: " << seq_scan->filter() << "\n";
        }
        break;
    }

    case PlanNodeType::INDEX_SCAN:
    {
        auto *idx_scan = static_cast<const IndexScanNode*>(plan);
        ss << prefix << "IndexScan on " << idx_scan->tableName();
        ss << " using " << idx_scan->indexName();
        ss << "  " << formatCostRows(plan->startupCost(), plan->totalCost(), plan->rows());
        ss << "\n";

        if (!idx_scan->indexCond().empty()) {
            ss << prefix << "  Index Cond: " << idx_scan->indexCond() << "\n";
        }

        if (!idx_scan->filter().empty()) {
            ss << prefix << "  Filter: " << idx_scan->filter() << "\n";
        }
        break;
    }

    default:
        ss << prefix << "Unknown plan node type\n";
        break;
    }

    // Format child nodes (for future JOINs, SORT, etc.)
    for (const auto &child : plan->children()) {
        ss << formatNode(child.get(), indent + 1);
    }

    return ss.str();
}

std::string ExplainFormatter::formatCostRows(double startup_cost, double total_cost, uint64_t rows)
{
    std::stringstream ss;
    ss << "(cost=";
    ss << std::fixed << std::setprecision(2) << startup_cost;
    ss << "..";
    ss << std::fixed << std::setprecision(2) << total_cost;
    ss << " rows=" << rows;
    ss << ")";
    return ss.str();
}

} // namespace scratchbird::optimizer
```

**Deliverable**: ExplainFormatter converts PlanNode to readable text

---

### Part 3: BytecodeGenerator Integration (2-3 hours)

#### 3.1 Implement visit(ExplainStmt)
**File**: `include/scratchbird/sblr/bytecode_generator.h`
- Add `void visit(parser::ExplainStmt *node) override;`

**File**: `src/sblr/bytecode_generator.cpp`

```cpp
void BytecodeGenerator::visit(parser::ExplainStmt *node)
{
    // EXPLAIN doesn't generate bytecode - it just shows the plan

    // Check if database available for planning
    if (!database_ || !database_->query_planner()) {
        current_result_->addError("EXPLAIN requires database with query planner");
        return;
    }

    // Only SELECT supported in Phase 1.5
    auto *select_stmt = dynamic_cast<parser::SelectStmt*>(node->query());
    if (!select_stmt) {
        current_result_->addError("EXPLAIN only supports SELECT statements");
        return;
    }

    // Generate query plan
    core::ErrorContext ctx;
    auto plan = database_->query_planner()->planQuery(select_stmt, &ctx);

    if (!plan) {
        std::string error = "Query planning failed";
        if (!ctx.message.empty()) {
            error += ": " + ctx.message;
        }
        current_result_->addError(error);
        return;
    }

    // Format plan as EXPLAIN text
    std::string explain_output = optimizer::ExplainFormatter::formatPlan(plan.get());

    // Store EXPLAIN output in bytecode result
    // Option 1: Add to errors (displayed to user)
    // Option 2: Add special field to BytecodeResult for EXPLAIN output

    // For now, use a special bytecode sequence to encode EXPLAIN output
    current_result_->writeOpcode(Opcode::EXPLAIN_PLAN);
    current_result_->writeString(explain_output);
}
```

#### 3.2 Add EXPLAIN_PLAN Opcode
**File**: `include/scratchbird/sblr/opcodes.h`

```cpp
enum class Opcode : uint8_t {
    // ... existing opcodes ...

    // EXPLAIN command (Phase 1 Task 1.5)
    EXPLAIN_PLAN = 0xC2,  // EXPLAIN output (string)
};
```

**Deliverable**: EXPLAIN statement generates formatted plan output

---

### Part 4: Enhance PlanNode toString() (1-2 hours)

The PlanNode classes already have `toString()` methods, but they may need enhancement for better EXPLAIN output.

#### 4.1 Review Existing toString() Methods
**Files**:
- `include/scratchbird/optimizer/plan_node.h` (lines 218-234 for SeqScanNode)
- `include/scratchbird/optimizer/plan_node.h` (lines 381-405 for IndexScanNode)

**Current Output**:
```cpp
// SeqScanNode::toString()
SeqScan on users (cost=0.00..225.00 rows=10000)
  Filter: age > 25

// IndexScanNode::toString()
IndexScan on users using idx_users_id (cost=0.01..16.02 rows=1)
  Index Cond: id = 42
  Filter: age > 25
```

**Analysis**: Existing `toString()` methods are already well-formatted for EXPLAIN output!

**Decision**: Use existing `toString()` methods instead of creating separate ExplainFormatter. Simply enhance the format slightly for consistency with PostgreSQL.

**Deliverable**: PlanNode toString() methods produce EXPLAIN-ready output

---

### Part 5: Testing (2-3 hours)

#### 5.1 Unit Tests
**File**: `tests/unit/test_explain_command.cpp`

Test cases:
1. Parse EXPLAIN SELECT syntax
2. Reject EXPLAIN INSERT (not supported in Phase 1.5)
3. Generate EXPLAIN output for SeqScan
4. Generate EXPLAIN output for IndexScan
5. Show filters correctly
6. Show index conditions correctly
7. Format costs and row estimates correctly

#### 5.2 Integration Tests
**File**: `tests/integration/test_explain_integration.cpp`

Test cases:
1. EXPLAIN with real database
2. EXPLAIN shows SeqScan for table without indexes
3. EXPLAIN shows IndexScan for indexed column
4. EXPLAIN shows cost differences between plans
5. Verify cost model affects EXPLAIN output

**Deliverable**: Comprehensive test coverage for EXPLAIN command

---

## Implementation Order

### Session 1 (4-5 hours): Parser Support
1. Add EXPLAIN keyword to lexer
2. Create ExplainStmt AST node
3. Update ASTVisitor interface
4. Implement Parser::parseExplain()
5. Add basic parser tests

### Session 2 (3-4 hours): EXPLAIN Output
1. Review existing PlanNode::toString() methods
2. Enhance toString() format if needed
3. Add EXPLAIN_PLAN opcode
4. Implement visit(ExplainStmt) in BytecodeGenerator

### Session 3 (3-4 hours): Testing & Polish
1. Create unit tests for EXPLAIN parsing
2. Create integration tests for EXPLAIN execution
3. Test with various query patterns
4. Document EXPLAIN command usage
5. Update roadmap

**Total**: 10-13 hours

---

## Future Enhancements (Phase 2+)

### EXPLAIN ANALYZE
- Execute query and show actual row counts
- Show actual execution time vs estimated
- Compare estimated vs actual costs

### EXPLAIN VERBOSE
- Show output column list
- Show join types in detail
- Show sort keys
- Show statistics used for estimates

### EXPLAIN (FORMAT JSON)
- Machine-readable JSON output
- Nested structure for plan tree
- Include all plan node metadata

### EXPLAIN for Other Statements
- EXPLAIN UPDATE
- EXPLAIN DELETE
- EXPLAIN INSERT

---

## PostgreSQL Compatibility

Our EXPLAIN output format matches PostgreSQL's text format:

**PostgreSQL Example**:
```
postgres=# EXPLAIN SELECT * FROM users WHERE id = 42;
                                 QUERY PLAN
----------------------------------------------------------------------------
Index Scan using users_pkey on users  (cost=0.15..8.17 rows=1 width=32)
  Index Cond: (id = 42)
```

**ScratchBird Example**:
```
scratchbird> EXPLAIN SELECT * FROM users WHERE id = 42;
                        QUERY PLAN
--------------------------------------------------------
IndexScan on users using idx_users_id  (cost=0.01..8.03 rows=1)
  Index Cond: id = 42
```

**Differences**:
- We don't show "width" (avg row width) in Phase 1.5
- Our cost values may differ (different cost model parameters)
- Our node names are CamelCase (IndexScan vs "Index Scan")

---

## Success Criteria

✅ `EXPLAIN SELECT ...` syntax is recognized
✅ EXPLAIN shows plan node type (SeqScan or IndexScan)
✅ EXPLAIN shows table and index names
✅ EXPLAIN shows cost estimates (startup..total)
✅ EXPLAIN shows row estimates
✅ EXPLAIN shows filters and index conditions
✅ Output format matches PostgreSQL style
✅ Comprehensive test coverage

---

## Example Usage

```sql
-- Create table
CREATE TABLE users (
    id INTEGER NOT NULL,
    name VARCHAR(100),
    age INTEGER
);

-- Create index
CREATE INDEX idx_users_id ON users(id);

-- Analyze table (collect statistics)
ANALYZE users;

-- EXPLAIN without index
EXPLAIN SELECT * FROM users WHERE age > 25;
-- Output: SeqScan on users (cost=0.00..225.00 rows=333)
--           Filter: age > 25

-- EXPLAIN with index
EXPLAIN SELECT * FROM users WHERE id = 42;
-- Output: IndexScan on users using idx_users_id (cost=0.01..8.03 rows=1)
--           Index Cond: id = 42

-- EXPLAIN with index + filter
EXPLAIN SELECT * FROM users WHERE id > 100 AND age > 25;
-- Output: IndexScan on users using idx_users_id (cost=0.01..45.67 rows=50)
--           Index Cond: id > 100
--           Filter: age > 25
```

---

## Dependencies

- ✅ Query Planner (Task 1.3) - COMPLETE
- ✅ Cost Model (Task 1.2) - COMPLETE
- ✅ Statistics Collection (Task 1.1) - COMPLETE
- ✅ PlanNode structures with toString() - COMPLETE

**No blockers** - Ready to implement!

---

## Files to Create/Modify

### New Files (2):
1. `include/scratchbird/optimizer/explain_formatter.h` (optional - may use toString() instead)
2. `src/optimizer/explain_formatter.cpp` (optional)
3. `tests/unit/test_explain_command.cpp`

### Modified Files (5):
1. `include/scratchbird/parser/lexer.h` - Add EXPLAIN keyword
2. `include/scratchbird/parser/ast.h` - Add ExplainStmt + visitor method
3. `src/parser/parser.cpp` - Implement parseExplain()
4. `include/scratchbird/sblr/bytecode_generator.h` - Add visit(ExplainStmt)
5. `src/sblr/bytecode_generator.cpp` - Implement visit(ExplainStmt)
6. `include/scratchbird/sblr/opcodes.h` - Add EXPLAIN_PLAN opcode

**Estimated Lines of Code**: ~400-600 lines (including tests)

---

## References

- PostgreSQL EXPLAIN: https://www.postgresql.org/docs/current/sql-explain.html
- PostgreSQL EXPLAIN Output: https://www.postgresql.org/docs/current/using-explain.html
- MySQL EXPLAIN: https://dev.mysql.com/doc/refman/8.0/en/explain.html
- SQL Server Execution Plans: https://learn.microsoft.com/en-us/sql/relational-databases/performance/execution-plans
