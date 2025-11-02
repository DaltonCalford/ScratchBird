# JOIN Implementation Completion Guide
**Created**: October 26, 2025
**Purpose**: Guide to complete remaining JOIN implementation (Task 3.2 integration + Task 3.3 executor)
**Status**: ~90% Complete - Needs QueryPlanner integration + Executor

---

## Current Status

### ✅ Completed (~90%)

**Task 3.1: Parser + Semantic + Bytecode** (100% ✅)
- All JOIN syntax parsing (INNER, LEFT, RIGHT, FULL, CROSS, NATURAL)
- Qualified column names (table.column, alias.column)
- Semantic validation and type checking
- Bytecode generation for qualified references
- 10 parsing tests passing

**Task 3.2: Query Planner Infrastructure** (90% ✅)
- NestedLoopJoinNode and HashJoinNode plan nodes
- NestedLoopJoinPath and HashJoinPath classes
- JOIN cost estimation (costNestedLoopJoin, costHashJoin)
- JOIN selectivity estimation (estimateJoinSelectivity, estimateEquiJoinSelectivity)
- Comprehensive design document

### ⏸️ Remaining (~10%)

**Task 3.2 Integration** (~5 hours):
- Hash key extraction from join conditions
- Join path generation in QueryPlanner
- Path to plan node conversion for joins
- Integration with planQuery() method

**Task 3.3: JOIN Executor** (~10-15 hours):
- JOIN opcodes in SBLR bytecode
- Nested loop join execution
- Hash join execution
- NULL handling for outer joins

---

## Part 1: Complete Task 3.2 - QueryPlanner Integration

### Overview

The QueryPlanner needs to be extended to:
1. Detect when a SELECT has JOINs (check `SelectStmt::hasJoins()`)
2. Generate base relation paths for each table
3. Generate join paths (nested loop + hash) for each join
4. Convert the cheapest join path to a plan node
5. Return the join plan to the bytecode generator

### Step 1: Add Hash Key Extraction

**File**: `include/scratchbird/optimizer/query_planner.h`

Add private helper methods:

```cpp
private:
    // Existing methods...

    /**
     * isHashJoinApplicable - Check if hash join can be used
     *
     * Hash join requires equi-join condition (contains = operator)
     *
     * @param join_condition JOIN ON expression
     * @return true if hash join applicable
     */
    bool isHashJoinApplicable(parser::Expression* join_condition) const;

    /**
     * extractHashKeys - Extract hash key expressions from equi-join
     *
     * Example: "u.id = o.user_id AND u.type = o.type"
     * → left_keys = [u.id, u.type]
     * → right_keys = [o.user_id, o.type]
     *
     * @param join_condition JOIN ON expression
     * @param left_keys Output: hash keys from left table
     * @param right_keys Output: hash keys from right table
     * @return true if hash keys extracted successfully
     */
    bool extractHashKeys(
        parser::Expression* join_condition,
        std::vector<parser::Expression*>& left_keys,
        std::vector<parser::Expression*>& right_keys) const;
```

**File**: `src/optimizer/query_planner.cpp`

Implementation:

```cpp
bool QueryPlanner::isHashJoinApplicable(parser::Expression* join_condition) const
{
    if (!join_condition)
        return false;

    // Check if condition contains equality
    if (join_condition->kind() == parser::ASTKind::BINARY_OP)
    {
        auto* binary_op = static_cast<parser::BinaryOpExpr*>(join_condition);

        if (binary_op->op() == parser::BinaryOp::EQ)
        {
            // Simple equality - hash join applicable
            return true;
        }
        else if (binary_op->op() == parser::BinaryOp::AND)
        {
            // Compound condition - check if both sides have equality
            return isHashJoinApplicable(binary_op->left()) &&
                   isHashJoinApplicable(binary_op->right());
        }
    }

    return false;
}

bool QueryPlanner::extractHashKeys(
    parser::Expression* join_condition,
    std::vector<parser::Expression*>& left_keys,
    std::vector<parser::Expression*>& right_keys) const
{
    if (!join_condition)
        return false;

    if (join_condition->kind() == parser::ASTKind::BINARY_OP)
    {
        auto* binary_op = static_cast<parser::BinaryOpExpr*>(join_condition);

        if (binary_op->op() == parser::BinaryOp::EQ)
        {
            // Extract left and right column references
            left_keys.push_back(binary_op->left());
            right_keys.push_back(binary_op->right());
            return true;
        }
        else if (binary_op->op() == parser::BinaryOp::AND)
        {
            // Recursively extract from both sides
            bool left_ok = extractHashKeys(binary_op->left(), left_keys, right_keys);
            bool right_ok = extractHashKeys(binary_op->right(), left_keys, right_keys);
            return left_ok && right_ok;
        }
    }

    return false;
}
```

### Step 2: Generate Base Relation Paths

**File**: `src/optimizer/query_planner.cpp`

Add helper method to generate paths for a single table:

```cpp
std::vector<std::shared_ptr<Path>> QueryPlanner::generateBaseRelationPaths(
    const parser::TableRef& table_ref,
    parser::Expression* where_clause,
    core::ErrorContext* ctx)
{
    std::vector<std::shared_ptr<Path>> paths;

    // Look up table in catalog
    auto table_id_opt = db_->getCatalogManager().lookupTable(
        pool_.get(table_ref.table_name), ctx);

    if (!table_id_opt)
    {
        // Table not found
        return paths;
    }

    core::ID table_id = *table_id_opt;

    // Generate sequential scan path
    // (Use existing generatePaths logic for single table)

    // For simplicity in Phase 1, generate only sequential scan
    // Full implementation would also generate index scan paths

    // Get table statistics
    uint64_t num_pages = 100;  // TODO: Get from catalog
    uint64_t num_tuples = 10000;  // TODO: Get from catalog

    double selectivity = 1.0;
    if (where_clause)
    {
        selectivity = selectivity_estimator_->estimateWhereClause(
            where_clause, table_id, ctx);
    }

    uint64_t output_rows = static_cast<uint64_t>(num_tuples * selectivity);
    double qual_cost = 0.01;  // TODO: Calculate from WHERE clause

    CostEstimate cost = cost_model_->costSeqScan(num_pages, num_tuples, qual_cost, ctx);

    auto seq_scan_path = std::make_shared<SeqScanPath>(
        table_id,
        pool_.get(table_ref.table_name),
        num_pages,
        output_rows,
        qual_cost,
        cost);

    paths.push_back(seq_scan_path);

    return paths;
}
```

### Step 3: Generate Join Paths

**File**: `src/optimizer/query_planner.cpp`

Add method to generate join paths for two relations:

```cpp
std::vector<std::shared_ptr<Path>> QueryPlanner::generateJoinPaths(
    std::shared_ptr<Path> left_path,
    std::shared_ptr<Path> right_path,
    const parser::JoinClause& join_clause,
    core::ErrorContext* ctx)
{
    std::vector<std::shared_ptr<Path>> join_paths;

    // Get table IDs (for selectivity estimation)
    core::ID left_table_id;  // TODO: Extract from left_path
    core::ID right_table_id;  // TODO: Extract from right_path

    // Estimate join selectivity
    double selectivity = selectivity_estimator_->estimateJoinSelectivity(
        join_clause.on_condition,
        left_table_id,
        right_table_id,
        ctx);

    uint64_t left_rows = left_path->rows();
    uint64_t right_rows = right_path->rows();

    // 1. Generate Nested Loop Join Path (always applicable)
    {
        CostEstimate nl_cost = cost_model_->costNestedLoopJoin(
            left_path->cost(),
            right_path->cost(),
            left_rows,
            right_rows,
            selectivity,
            ctx);

        auto nl_path = std::make_shared<NestedLoopJoinPath>(
            join_clause.join_type,
            left_path,
            right_path,
            join_clause.on_condition,
            selectivity,
            nl_cost);

        join_paths.push_back(nl_path);

        DEBUG_LOG_DB("Generated Nested Loop Join path: cost=" +
                   std::to_string(nl_cost.total_cost) +
                   ", rows=" + std::to_string(nl_cost.rows));
    }

    // 2. Generate Hash Join Path (only if equi-join)
    if (isHashJoinApplicable(join_clause.on_condition))
    {
        std::vector<parser::Expression*> hash_keys_left;
        std::vector<parser::Expression*> hash_keys_right;

        if (extractHashKeys(join_clause.on_condition, hash_keys_left, hash_keys_right))
        {
            // Choose smaller relation as build side
            std::shared_ptr<Path> build_path = (left_rows < right_rows) ? left_path : right_path;
            std::shared_ptr<Path> probe_path = (left_rows < right_rows) ? right_path : left_path;
            uint64_t build_rows = build_path->rows();
            uint64_t probe_rows = probe_path->rows();

            CostEstimate hash_cost = cost_model_->costHashJoin(
                build_path->cost(),
                probe_path->cost(),
                build_rows,
                probe_rows,
                selectivity,
                ctx);

            auto hash_path = std::make_shared<HashJoinPath>(
                join_clause.join_type,
                build_path,
                probe_path,
                join_clause.on_condition,
                hash_keys_left,
                hash_keys_right,
                selectivity,
                hash_cost);

            join_paths.push_back(hash_path);

            DEBUG_LOG_DB("Generated Hash Join path: cost=" +
                       std::to_string(hash_cost.total_cost) +
                       ", rows=" + std::to_string(hash_cost.rows));
        }
    }

    return join_paths;
}
```

### Step 4: Plan Join Query

**File**: `src/optimizer/query_planner.cpp`

Add main join planning method:

```cpp
PlanNode* QueryPlanner::planJoinQuery(parser::SelectStmt* select, core::ErrorContext* ctx)
{
    DEBUG_LOG_DB("Planning JOIN query with " +
               std::to_string(select->fromClause().joins.size()) + " joins");

    // For Phase 1: Greedy join ordering (join in query order)
    // Phase 2 will implement dynamic programming for optimal ordering

    // Step 1: Generate path for base table
    auto base_path = generateBaseRelationPaths(
        select->fromClause().base_table,
        select->whereClause(),
        ctx)[0];  // Use first (cheapest) path

    std::shared_ptr<Path> current_path = base_path;

    // Step 2: For each join, generate paths and select cheapest
    for (const auto& join : select->fromClause().joins)
    {
        // Generate paths for right table
        auto right_paths = generateBaseRelationPaths(
            join.right_table,
            nullptr,  // No WHERE clause for joined table
            ctx);

        if (right_paths.empty())
            continue;

        auto right_path = right_paths[0];  // Use first (cheapest) path

        // Generate join paths
        auto join_paths = generateJoinPaths(current_path, right_path, join, ctx);

        if (join_paths.empty())
            continue;

        // Select cheapest join path
        std::shared_ptr<Path> cheapest_join = join_paths[0];
        for (const auto& jp : join_paths)
        {
            if (jp->totalCost() < cheapest_join->totalCost())
            {
                cheapest_join = jp;
            }
        }

        current_path = cheapest_join;

        DEBUG_LOG_DB("Selected cheapest join path: type=" +
                   std::to_string(static_cast<int>(cheapest_join->type())) +
                   ", cost=" + std::to_string(cheapest_join->totalCost()));
    }

    // Step 3: Convert final path to plan node
    return joinPathToPlanNode(current_path);
}
```

### Step 5: Convert Path to Plan Node

**File**: `src/optimizer/query_planner.cpp`

Add conversion method:

```cpp
PlanNode* QueryPlanner::joinPathToPlanNode(std::shared_ptr<Path> path)
{
    if (path->type() == PathType::SEQ_SCAN)
    {
        // Leaf node - convert to SeqScanNode
        auto* seq_path = static_cast<SeqScanPath*>(path.get());
        auto* scan_node = new SeqScanNode(
            seq_path->tableId(),
            seq_path->tableName(),
            ScanDirection::FORWARD);
        scan_node->setCost(
            path->startupCost(),
            path->totalCost(),
            path->rows());
        return scan_node;
    }
    else if (path->type() == PathType::NESTED_LOOP_JOIN)
    {
        auto* nl_path = static_cast<NestedLoopJoinPath*>(path.get());

        // Recursively convert children
        auto outer_node = std::shared_ptr<PlanNode>(
            joinPathToPlanNode(nl_path->outerPath()));
        auto inner_node = std::shared_ptr<PlanNode>(
            joinPathToPlanNode(nl_path->innerPath()));

        auto* join_node = new NestedLoopJoinNode(
            nl_path->joinType(),
            outer_node,
            inner_node,
            nl_path->joinCondition());

        join_node->setCost(
            path->startupCost(),
            path->totalCost(),
            path->rows());

        return join_node;
    }
    else if (path->type() == PathType::HASH_JOIN)
    {
        auto* hash_path = static_cast<HashJoinPath*>(path.get());

        // Recursively convert children
        auto outer_node = std::shared_ptr<PlanNode>(
            joinPathToPlanNode(hash_path->outerPath()));
        auto inner_node = std::shared_ptr<PlanNode>(
            joinPathToPlanNode(hash_path->innerPath()));

        auto* join_node = new HashJoinNode(
            hash_path->joinType(),
            outer_node,
            inner_node,
            hash_path->joinCondition(),
            hash_path->hashKeysOuter(),
            hash_path->hashKeysInner());

        join_node->setCost(
            path->startupCost(),
            path->totalCost(),
            path->rows());

        return join_node;
    }

    // Unknown path type
    return nullptr;
}
```

### Step 6: Integrate with planQuery()

**File**: `src/optimizer/query_planner.cpp`

Modify existing `planQuery()` method:

```cpp
PlanNode* QueryPlanner::planQuery(
    parser::SelectStmt* select_stmt,
    core::ErrorContext* ctx)
{
    // Check if this is a join query
    if (select_stmt->hasJoins())
    {
        return planJoinQuery(select_stmt, ctx);
    }

    // Existing single-table planning logic...
    // (keep existing code for single-table queries)
}
```

---

## Part 2: Complete Task 3.3 - JOIN Executor

### Overview

The executor needs to:
1. Add JOIN opcodes to SBLR bytecode format
2. Generate bytecode for join plans
3. Implement join execution logic
4. Handle NULL values for outer joins

### Step 1: Add JOIN Opcodes

**File**: `include/scratchbird/sblr/opcodes.h`

Add new opcodes:

```cpp
enum class Opcode : uint8_t
{
    // Existing opcodes...

    // JOIN opcodes (Phase 1, Task 3.3)
    NESTED_LOOP_JOIN = 30,    // Nested loop join
    HASH_JOIN = 31,           // Hash join
    JOIN_FILTER = 32,         // Evaluate join condition
    EMIT_JOIN_ROW = 33,       // Emit joined row
    HASH_BUILD = 34,          // Build hash table (for hash join)
    HASH_PROBE = 35,          // Probe hash table (for hash join)
};
```

### Step 2: Generate JOIN Bytecode

**File**: `src/sblr/bytecode_generator.cpp`

Add method to generate bytecode from join plan nodes:

```cpp
void BytecodeGenerator::visitJoinPlanNode(optimizer::PlanNode* node)
{
    using namespace optimizer;

    if (node->type() == PlanNodeType::NESTED_LOOP_JOIN)
    {
        auto* nl_node = static_cast<NestedLoopJoinNode*>(node);

        // Generate NESTED_LOOP_JOIN opcode
        current_result_->writeOpcode(Opcode::NESTED_LOOP_JOIN);

        // Write join type
        current_result_->writeUint8(static_cast<uint8_t>(nl_node->joinType()));

        // Generate bytecode for outer (left) plan
        visitJoinPlanNode(nl_node->outerPlan().get());

        // Generate bytecode for inner (right) plan
        visitJoinPlanNode(nl_node->innerPlan().get());

        // Generate bytecode for join condition
        if (nl_node->joinCondition())
        {
            current_result_->writeOpcode(Opcode::JOIN_FILTER);
            // Visit join condition expression
            nl_node->joinCondition()->accept(this);
        }

        // Generate EMIT_JOIN_ROW
        current_result_->writeOpcode(Opcode::EMIT_JOIN_ROW);
    }
    else if (node->type() == PlanNodeType::HASH_JOIN)
    {
        auto* hash_node = static_cast<HashJoinNode*>(node);

        // Generate HASH_JOIN opcode
        current_result_->writeOpcode(Opcode::HASH_JOIN);

        // Write join type
        current_result_->writeUint8(static_cast<uint8_t>(hash_node->joinType()));

        // Generate HASH_BUILD phase
        current_result_->writeOpcode(Opcode::HASH_BUILD);

        // Write number of hash keys
        current_result_->writeUint32(
            static_cast<uint32_t>(hash_node->hashKeysOuter().size()));

        // Write hash key expressions
        for (auto* key_expr : hash_node->hashKeysOuter())
        {
            key_expr->accept(this);
        }

        // Generate bytecode for outer (build) plan
        visitJoinPlanNode(hash_node->outerPlan().get());

        // Generate HASH_PROBE phase
        current_result_->writeOpcode(Opcode::HASH_PROBE);

        // Write hash key expressions for probe
        for (auto* key_expr : hash_node->hashKeysInner())
        {
            key_expr->accept(this);
        }

        // Generate bytecode for inner (probe) plan
        visitJoinPlanNode(hash_node->innerPlan().get());

        // Generate join condition check
        if (hash_node->joinCondition())
        {
            current_result_->writeOpcode(Opcode::JOIN_FILTER);
            hash_node->joinCondition()->accept(this);
        }

        // Generate EMIT_JOIN_ROW
        current_result_->writeOpcode(Opcode::EMIT_JOIN_ROW);
    }
    else if (node->type() == PlanNodeType::SEQ_SCAN)
    {
        // Leaf node - generate scan bytecode
        auto* scan_node = static_cast<SeqScanNode*>(node);

        current_result_->writeOpcode(Opcode::SEQ_SCAN);
        writeStringId(string_pool_.intern(scan_node->tableName()));
        // ... rest of scan bytecode
    }
}
```

### Step 3: Implement Nested Loop Join Executor

**File**: `src/sblr/executor.cpp`

Add nested loop join execution:

```cpp
void Executor::executeNestedLoopJoin()
{
    // Read join type
    uint8_t join_type_byte = readUint8();
    JoinType join_type = static_cast<JoinType>(join_type_byte);

    // Execute outer relation (get all rows)
    std::vector<Tuple> outer_tuples;
    // ... execute outer plan and collect tuples

    // For each outer tuple
    for (const auto& outer_tuple : outer_tuples)
    {
        // Save current tuple context
        current_outer_tuple_ = outer_tuple;

        bool found_match = false;

        // Execute inner relation (for each outer row)
        // ... execute inner plan

        // For each inner tuple
        // (This would be in a callback/iterator pattern)
        void processInnerTuple(const Tuple& inner_tuple)
        {
            current_inner_tuple_ = inner_tuple;

            // Evaluate join condition
            bool condition_met = evaluateJoinCondition();

            if (condition_met)
            {
                // Emit joined row (outer + inner)
                emitJoinRow(outer_tuple, inner_tuple);
                found_match = true;
            }
        }

        // Handle outer joins
        if (!found_match)
        {
            if (join_type == JoinType::LEFT || join_type == JoinType::FULL)
            {
                // Emit outer row with NULL for inner columns
                emitJoinRow(outer_tuple, Tuple::null());
            }
        }
    }

    // Handle RIGHT OUTER JOIN: emit unmatched inner rows with NULL outer
    // (Would need to track which inner rows were matched)
}
```

### Step 4: Implement Hash Join Executor

**File**: `src/sblr/executor.cpp`

Add hash join execution:

```cpp
void Executor::executeHashJoin()
{
    // Read join type
    uint8_t join_type_byte = readUint8();
    JoinType join_type = static_cast<JoinType>(join_type_byte);

    // Phase 1: Build hash table from outer (build side)
    std::unordered_multimap<uint64_t, Tuple> hash_table;

    // Read number of hash keys
    uint32_t num_keys = readUint32();

    // Read hash key expressions
    std::vector<Expression*> hash_key_exprs;
    for (uint32_t i = 0; i < num_keys; ++i)
    {
        hash_key_exprs.push_back(readExpression());
    }

    // Execute outer plan and build hash table
    // ... execute outer plan

    void processBuildTuple(const Tuple& tuple)
    {
        // Compute hash from hash key values
        uint64_t hash_value = 0;
        for (auto* key_expr : hash_key_exprs)
        {
            Value key_val = evaluateExpression(key_expr, tuple);
            hash_value ^= std::hash<Value>{}(key_val);
        }

        // Insert into hash table
        hash_table.insert({hash_value, tuple});
    }

    // Phase 2: Probe hash table with inner (probe side)
    // ... execute inner plan

    void processProbeTuple(const Tuple& inner_tuple)
    {
        // Compute hash from inner tuple
        uint64_t hash_value = 0;
        for (auto* key_expr : probe_key_exprs)
        {
            Value key_val = evaluateExpression(key_expr, inner_tuple);
            hash_value ^= std::hash<Value>{}(key_val);
        }

        // Lookup in hash table
        auto range = hash_table.equal_range(hash_value);

        bool found_match = false;

        for (auto it = range.first; it != range.second; ++it)
        {
            const Tuple& outer_tuple = it->second;

            // Evaluate join condition (to handle hash collisions)
            if (evaluateJoinCondition(outer_tuple, inner_tuple))
            {
                emitJoinRow(outer_tuple, inner_tuple);
                found_match = true;
            }
        }

        // Handle outer joins
        if (!found_match && (join_type == JoinType::RIGHT || join_type == JoinType::FULL))
        {
            emitJoinRow(Tuple::null(), inner_tuple);
        }
    }
}
```

### Step 5: NULL Handling for Outer Joins

**File**: `src/sblr/executor.cpp`

Add helper to create NULL-padded tuples:

```cpp
Tuple Executor::createNullPaddedTuple(const Tuple& tuple, size_t null_columns)
{
    Tuple padded = tuple;

    // Append NULL values for missing columns
    for (size_t i = 0; i < null_columns; ++i)
    {
        padded.addNull();
    }

    return padded;
}

void Executor::emitJoinRow(const Tuple& left, const Tuple& right)
{
    // Combine left and right tuples
    Tuple joined;

    // Copy values from left tuple
    for (size_t i = 0; i < left.columnCount(); ++i)
    {
        joined.addValue(left.getValue(i));
    }

    // Copy values from right tuple (or NULLs for outer joins)
    if (right.isNull())
    {
        // Right side is NULL (LEFT OUTER JOIN)
        for (size_t i = 0; i < right_column_count_; ++i)
        {
            joined.addNull();
        }
    }
    else if (left.isNull())
    {
        // Left side is NULL (RIGHT OUTER JOIN)
        // Already have NULLs from left, just add right values
        for (size_t i = 0; i < right.columnCount(); ++i)
        {
            joined.addValue(right.getValue(i));
        }
    }
    else
    {
        // Both sides present (INNER JOIN)
        for (size_t i = 0; i < right.columnCount(); ++i)
        {
            joined.addValue(right.getValue(i));
        }
    }

    // Emit joined tuple to result set
    result_set_.addTuple(joined);
}
```

---

## Testing Strategy

### Unit Tests

**Test 1: Hash Key Extraction**
```cpp
// Test: u.id = o.user_id
Expression* cond = /* parse "u.id = o.user_id" */;
std::vector<Expression*> left, right;
ASSERT_TRUE(planner.extractHashKeys(cond, left, right));
ASSERT_EQ(left.size(), 1);
ASSERT_EQ(right.size(), 1);

// Test: u.id = o.user_id AND u.type = o.type
// Should extract 2 pairs
```

**Test 2: Join Path Generation**
```cpp
// Test: Generate paths for 2-table join
// Verify both nested loop and hash join paths created
// Verify cost estimates are reasonable
```

**Test 3: Path Selection**
```cpp
// Test: Hash join should be cheaper for large tables
// Test: Nested loop with index should be cheaper for small lookups
```

### Integration Tests

**Test 1: Simple 2-Table Join**
```sql
SELECT * FROM users u INNER JOIN orders o ON u.id = o.user_id
```

Expected plan:
```
Hash Join (cost=3,075 rows=100,000)
  Hash Cond: (u.id = o.user_id)
  -> Seq Scan on users u (cost=100 rows=10,000)
  -> Seq Scan on orders o (cost=1,000 rows=100,000)
```

**Test 2: LEFT OUTER JOIN**
```sql
SELECT * FROM users u LEFT JOIN orders o ON u.id = o.user_id
```

Verify:
- All user rows appear in result
- Users with no orders have NULL for order columns

**Test 3: 3-Table Join**
```sql
SELECT *
FROM users u
INNER JOIN orders o ON u.id = o.user_id
INNER JOIN products p ON o.product_id = p.id
```

Expected plan: Nested joins
```
Hash Join
  Hash Cond: (o.product_id = p.id)
  -> Hash Join
      Hash Cond: (u.id = o.user_id)
      -> Seq Scan on users u
      -> Seq Scan on orders o
  -> Seq Scan on products p
```

---

## Performance Benchmarks

After implementation, benchmark:

1. **Nested Loop vs Hash Join**:
   - Small tables (< 1,000 rows): Nested loop may win
   - Large tables (> 10,000 rows): Hash join should win

2. **Index-Nested Loop**:
   - With index on join key: Should beat hash join

3. **3+ Table Joins**:
   - Verify join ordering affects cost
   - Verify greedy ordering is reasonable

---

## Estimated Effort

**Task 3.2 Integration** (Remaining):
- Hash key extraction: 1 hour
- Join path generation: 2 hours
- Path to node conversion: 1 hour
- Integration testing: 1 hour
- **Total**: ~5 hours

**Task 3.3 Executor**:
- Opcode definitions: 0.5 hours
- Bytecode generation: 2-3 hours
- Nested loop execution: 3-4 hours
- Hash join execution: 4-5 hours
- NULL handling: 2 hours
- Testing: 2-3 hours
- **Total**: ~10-15 hours

**Grand Total**: ~15-20 hours to complete JOIN support end-to-end

---

## Summary

**Current Progress**: ~90%

**What Works**:
- ✅ Complete JOIN parsing
- ✅ Semantic validation
- ✅ Bytecode generation (qualified names)
- ✅ JOIN plan infrastructure (nodes, paths, costs, selectivity)
- ✅ Comprehensive design

**What Remains**:
- ❌ QueryPlanner integration (~5 hours)
- ❌ JOIN executor (~10-15 hours)

**Next Actions**:
1. Fix pre-existing query_planner.cpp compilation errors
2. Implement QueryPlanner integration (Part 1 above)
3. Implement JOIN executor (Part 2 above)
4. End-to-end testing with real queries
5. Update roadmap to 100%

---

**Document Status**: Implementation Guide
**Last Updated**: October 26, 2025
