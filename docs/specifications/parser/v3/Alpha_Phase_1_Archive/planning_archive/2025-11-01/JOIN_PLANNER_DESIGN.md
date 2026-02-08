# JOIN Query Planner Design

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.

**Created**: October 25, 2025
**Purpose**: Design document for Phase 1, Task 3.2 - Query Planner for Joins
**Status**: Implementation Guide

---

## Overview

This document describes the design and implementation of the JOIN query planner for ScratchBird. The JOIN planner extends the existing single-table query planner to support multi-table queries with optimal join ordering and method selection.

**Goal**: Given a SELECT statement with JOINs, produce an optimal execution plan that:
1. Chooses the best join method for each join (nested loop, hash join)
2. Determines the optimal join order (which table to scan first)
3. Estimates costs accurately using statistics
4. Handles all join types (INNER, LEFT, RIGHT, FULL, CROSS)

---

## Architecture

### 1. Plan Node Structures

We extend the existing PlanNode hierarchy with join-specific nodes:

```cpp
// Base class (already exists in plan_node.h)
class PlanNode {
    double total_cost;
    double startup_cost;
    double rows;
    // ...
};

// NEW: Nested Loop Join Node
class NestedLoopJoinNode : public PlanNode {
    JoinType join_type;              // INNER, LEFT, RIGHT, FULL
    PlanNode* outer_plan;            // Left side of join
    PlanNode* inner_plan;            // Right side of join
    Expression* join_condition;      // ON clause expression

    // For USING/NATURAL joins
    std::vector<StringPool::StringId> join_columns;
};

// NEW: Hash Join Node
class HashJoinNode : public PlanNode {
    JoinType join_type;
    PlanNode* outer_plan;            // Build side (usually smaller table)
    PlanNode* inner_plan;            // Probe side (usually larger table)
    Expression* join_condition;      // ON clause (must contain equality)

    // Hash key columns (extracted from join_condition)
    std::vector<Expression*> hash_keys_outer;
    std::vector<Expression*> hash_keys_inner;
};
```

### 2. Path Structures

Paths are planning-time representations of possible join strategies:

```cpp
// NEW: Nested Loop Join Path
class NestedLoopJoinPath : public Path {
    JoinType join_type;
    Path* outer_path;
    Path* inner_path;
    Expression* join_condition;
    double join_selectivity;         // Estimated selectivity of join condition
};

// NEW: Hash Join Path
class HashJoinPath : public Path {
    JoinType join_type;
    Path* outer_path;                // Build side
    Path* inner_path;                // Probe side
    Expression* join_condition;
    std::vector<Expression*> hash_keys_outer;
    std::vector<Expression*> hash_keys_inner;
    double join_selectivity;
};
```

---

## Join Planning Algorithm

### Phase 1: Parse JOIN AST (Already Complete ✅)
The parser produces a `SelectStmt` with:
- `FromClause` containing base table + vector of `JoinClause`
- Each `JoinClause` has: join_type, right_table, condition_type, on_condition/using_columns

### Phase 2: Generate Base Relation Paths
For each table in the join (base table + all joined tables):
1. Generate sequential scan path
2. Generate index scan paths (if applicable indexes exist)
3. Select cheapest path for each base relation

This is already implemented in `QueryPlanner::generatePaths()` for single tables.

### Phase 3: Generate Join Paths
For each pair of relations to join:
1. **Check join method applicability**:
   - Nested loop: Always applicable
   - Hash join: Only for equi-joins (join condition contains `=`)

2. **Generate nested loop join path**:
   - Outer: Left relation
   - Inner: Right relation (re-scanned for each outer row)
   - Cost: `outer_cost + (outer_rows * inner_cost) + (outer_rows * inner_rows * cpu_tuple_cost)`

3. **Generate hash join path** (if applicable):
   - Outer (build): Smaller relation (by row count)
   - Inner (probe): Larger relation
   - Cost: `outer_cost + inner_cost + (outer_rows * hash_build_cost) + (inner_rows * hash_probe_cost)`

4. **Select cheapest join method**

### Phase 4: Join Ordering (Dynamic Programming)
For queries with N tables, there are (N-1)! possible join orders. We use dynamic programming:

**Algorithm** (Simplified for Phase 1):
```
For 2 tables: Only 1 join order (left -> right)
For 3 tables: Try both orders:
  - (T1 JOIN T2) JOIN T3
  - (T1 JOIN T3) JOIN T2
  Choose the one with lowest total cost

For 4+ tables: Use greedy heuristic (defer full DP to Phase 2)
  - Always join in the order specified by the query
  - Future: Implement full dynamic programming
```

**PostgreSQL-style DP** (Future Enhancement):
```cpp
// Level 1: Single relations
for each table T:
    cheapest_path[{T}] = generate_base_paths(T)

// Level 2: Pairs
for each pair (T1, T2):
    cheapest_path[{T1, T2}] = generate_join_paths(T1, T2)

// Level 3: Triples
for each triple (T1, T2, T3):
    for each split ({T1}, {T2, T3}) and ({T1, T2}, {T3}):
        cost = cheapest_path[left_set] + cheapest_path[right_set] + join_cost
    cheapest_path[{T1, T2, T3}] = min(cost)

// Continue up to N tables
```

---

## Join Cost Estimation

### Nested Loop Join Cost

**Formula**:
```
Total Cost = outer_cost + (outer_rows * inner_cost) + qual_cost

where:
  outer_cost  = Cost to scan outer (left) relation
  outer_rows  = Number of rows from outer relation
  inner_cost  = Cost to scan inner (right) relation (per outer row)
  qual_cost   = (outer_rows * inner_rows * selectivity) * cpu_tuple_cost
```

**Example**:
```sql
SELECT * FROM users u INNER JOIN orders o ON u.id = o.user_id
```

Assuming:
- users: 10,000 rows, seq scan cost = 100
- orders: 100,000 rows, seq scan cost = 1,000
- Selectivity of `u.id = o.user_id`: 0.001 (estimate)

```
Nested Loop Cost:
  outer_cost  = 100 (scan users)
  inner_cost  = 1,000 (scan orders, done 10,000 times)
  qual_cost   = (10,000 * 100,000 * 0.001) * 0.01 = 10,000

  Total = 100 + (10,000 * 1,000) + 10,000 = 10,010,100
```

**Optimization**: If inner relation has an index on join key:
```
  inner_cost = index_scan_cost = 5 (per outer row)
  Total = 100 + (10,000 * 5) + 10,000 = 60,100  <-- Much better!
```

### Hash Join Cost

**Formula**:
```
Total Cost = build_cost + probe_cost + hash_cost

where:
  build_cost = outer_cost + (outer_rows * cpu_tuple_cost * hash_build_factor)
  probe_cost = inner_cost + (inner_rows * cpu_tuple_cost * hash_probe_factor)
  hash_cost  = (outer_rows + inner_rows) * cpu_operator_cost

  hash_build_factor = 2.0 (hash insertion overhead)
  hash_probe_factor = 1.5 (hash lookup overhead)
```

**Example** (same query):
```
Hash Join Cost:
  build_cost = 100 + (10,000 * 0.01 * 2.0) = 300
  probe_cost = 1,000 + (100,000 * 0.01 * 1.5) = 2,500
  hash_cost  = (10,000 + 100,000) * 0.0025 = 275

  Total = 300 + 2,500 + 275 = 3,075  <-- Best option!
```

**Applicability**: Hash join requires:
1. **Equi-join condition**: Must have `=` operator (e.g., `u.id = o.user_id`)
2. **Sufficient memory**: Hash table must fit in memory (check available memory)

### Join Selectivity Estimation

The selectivity of a join condition determines how many rows the join produces.

**Formula**:
```
output_rows = outer_rows * inner_rows * selectivity
```

**Selectivity Estimation**:

1. **Equi-join** (`T1.col = T2.col`):
   ```
   selectivity = 1 / MAX(n_distinct(T1.col), n_distinct(T2.col))
   ```
   Rationale: Each value in the smaller domain matches `1 / n_distinct` of the larger domain.

2. **Foreign key join** (detected via catalog):
   ```
   selectivity = 1 / n_distinct(primary_key_table.col)
   ```
   Rationale: Each foreign key value matches exactly one primary key value.

3. **Range join** (`T1.col > T2.col`):
   ```
   selectivity = DEFAULT_INEQ_SEL = 0.3333
   ```
   Conservative estimate without histograms.

4. **Compound join** (`T1.a = T2.a AND T1.b = T2.b`):
   ```
   selectivity = sel(T1.a = T2.a) * sel(T1.b = T2.b)
   ```
   Assumes independence (may underestimate).

**Example**:
```sql
SELECT * FROM users u JOIN orders o ON u.id = o.user_id
```
- n_distinct(users.id) = 10,000 (primary key)
- n_distinct(orders.user_id) = 5,000 (foreign key)
- selectivity = 1 / MAX(10,000, 5,000) = 1 / 10,000 = 0.0001
- output_rows = 10,000 * 100,000 * 0.0001 = 100,000 (same as orders table)

---

## Join Type Handling

### INNER JOIN
- **Commutativity**: `A INNER JOIN B` = `B INNER JOIN A` (can reorder)
- **Optimization**: Choose smaller table as outer (for nested loop) or build side (for hash)

### LEFT OUTER JOIN
- **Non-commutative**: `A LEFT JOIN B` ≠ `B LEFT JOIN A`
- **Constraint**: A must be outer (preserved side), B must be inner
- **NULL handling**: Unmatched A rows produce NULL for B columns

### RIGHT OUTER JOIN
- **Equivalent to LEFT**: `A RIGHT JOIN B` = `B LEFT JOIN A`
- **Implementation**: Convert to LEFT JOIN during planning

### FULL OUTER JOIN
- **Non-commutative**: Order matters for NULL padding
- **Implementation**: Nested loop with NULL tracking, or hash join with full outer mode
- **Cost**: Higher than INNER (must track matched rows)

### CROSS JOIN
- **Cartesian product**: No join condition
- **Cost**: `outer_rows * inner_rows * cpu_tuple_cost`
- **Optimization**: Always use nested loop (hash join not applicable)

### NATURAL JOIN
- **Implicit join condition**: Equi-join on all common column names
- **Planning**: Convert to INNER JOIN with explicit ON clause during semantic analysis

---

## Implementation Plan

### Step 1: Add Plan Node Classes
**File**: `include/scratchbird/optimizer/plan_node.h`

```cpp
class NestedLoopJoinNode : public PlanNode {
public:
    NestedLoopJoinNode(parser::JoinType join_type,
                       PlanNode* outer,
                       PlanNode* inner,
                       parser::Expression* condition,
                       double cost,
                       double rows)
        : PlanNode(PlanNodeType::NESTED_LOOP_JOIN, cost, cost, rows),
          join_type_(join_type),
          outer_plan_(outer),
          inner_plan_(inner),
          join_condition_(condition)
    {}

    parser::JoinType joinType() const { return join_type_; }
    PlanNode* outerPlan() const { return outer_plan_; }
    PlanNode* innerPlan() const { return inner_plan_; }
    parser::Expression* joinCondition() const { return join_condition_; }

    std::string toString(int indent = 0) const override;

private:
    parser::JoinType join_type_;
    PlanNode* outer_plan_;
    PlanNode* inner_plan_;
    parser::Expression* join_condition_;
};

class HashJoinNode : public PlanNode {
public:
    HashJoinNode(parser::JoinType join_type,
                 PlanNode* outer,
                 PlanNode* inner,
                 parser::Expression* condition,
                 const std::vector<parser::Expression*>& hash_keys_outer,
                 const std::vector<parser::Expression*>& hash_keys_inner,
                 double cost,
                 double rows)
        : PlanNode(PlanNodeType::HASH_JOIN, cost, cost, rows),
          join_type_(join_type),
          outer_plan_(outer),
          inner_plan_(inner),
          join_condition_(condition),
          hash_keys_outer_(hash_keys_outer),
          hash_keys_inner_(hash_keys_inner)
    {}

    parser::JoinType joinType() const { return join_type_; }
    PlanNode* outerPlan() const { return outer_plan_; }
    PlanNode* innerPlan() const { return inner_plan_; }
    parser::Expression* joinCondition() const { return join_condition_; }
    const std::vector<parser::Expression*>& hashKeysOuter() const { return hash_keys_outer_; }
    const std::vector<parser::Expression*>& hashKeysInner() const { return hash_keys_inner_; }

    std::string toString(int indent = 0) const override;

private:
    parser::JoinType join_type_;
    PlanNode* outer_plan_;
    PlanNode* inner_plan_;
    parser::Expression* join_condition_;
    std::vector<parser::Expression*> hash_keys_outer_;
    std::vector<parser::Expression*> hash_keys_inner_;
};
```

### Step 2: Add Path Classes
**File**: `include/scratchbird/optimizer/path.h`

```cpp
class NestedLoopJoinPath : public Path {
public:
    NestedLoopJoinPath(parser::JoinType join_type,
                       Path* outer,
                       Path* inner,
                       parser::Expression* condition,
                       double selectivity)
        : Path(PathType::NESTED_LOOP_JOIN, 0.0, 0.0),
          join_type_(join_type),
          outer_path_(outer),
          inner_path_(inner),
          join_condition_(condition),
          selectivity_(selectivity)
    {}

    parser::JoinType joinType() const { return join_type_; }
    Path* outerPath() const { return outer_path_; }
    Path* innerPath() const { return inner_path_; }
    parser::Expression* joinCondition() const { return join_condition_; }
    double selectivity() const { return selectivity_; }

private:
    parser::JoinType join_type_;
    Path* outer_path_;
    Path* inner_path_;
    parser::Expression* join_condition_;
    double selectivity_;
};

class HashJoinPath : public Path {
public:
    HashJoinPath(parser::JoinType join_type,
                 Path* outer,
                 Path* inner,
                 parser::Expression* condition,
                 const std::vector<parser::Expression*>& hash_keys_outer,
                 const std::vector<parser::Expression*>& hash_keys_inner,
                 double selectivity)
        : Path(PathType::HASH_JOIN, 0.0, 0.0),
          join_type_(join_type),
          outer_path_(outer),
          inner_path_(inner),
          join_condition_(condition),
          hash_keys_outer_(hash_keys_outer),
          hash_keys_inner_(hash_keys_inner),
          selectivity_(selectivity)
    {}

    parser::JoinType joinType() const { return join_type_; }
    Path* outerPath() const { return outer_path_; }
    Path* innerPath() const { return inner_path_; }
    parser::Expression* joinCondition() const { return join_condition_; }
    const std::vector<parser::Expression*>& hashKeysOuter() const { return hash_keys_outer_; }
    const std::vector<parser::Expression*>& hashKeysInner() const { return hash_keys_inner_; }
    double selectivity() const { return selectivity_; }

private:
    parser::JoinType join_type_;
    Path* outer_path_;
    Path* inner_path_;
    parser::Expression* join_condition_;
    std::vector<parser::Expression*> hash_keys_outer_;
    std::vector<parser::Expression*> hash_keys_inner_;
    double selectivity_;
};
```

### Step 3: Extend CostModel
**File**: `include/scratchbird/optimizer/cost_model.h`

```cpp
class CostModel {
public:
    // Existing methods...

    // NEW: Join cost estimation
    CostEstimate costNestedLoopJoin(
        const CostEstimate& outer_cost,
        const CostEstimate& inner_cost,
        double outer_rows,
        double inner_rows,
        double selectivity) const;

    CostEstimate costHashJoin(
        const CostEstimate& outer_cost,
        const CostEstimate& inner_cost,
        double outer_rows,
        double inner_rows,
        double selectivity) const;
};
```

### Step 4: Add Join Selectivity Estimation
**File**: `include/scratchbird/optimizer/selectivity_estimator.h`

```cpp
class SelectivityEstimator {
public:
    // Existing methods...

    // NEW: Join selectivity
    double estimateJoinSelectivity(
        parser::Expression* join_condition,
        const std::string& left_table,
        const std::string& right_table) const;

private:
    double estimateEquiJoinSelectivity(
        StringPool::StringId left_col,
        StringPool::StringId right_col,
        const std::string& left_table,
        const std::string& right_table) const;
};
```

### Step 5: Extend QueryPlanner
**File**: `include/scratchbird/optimizer/query_planner.h`

```cpp
class QueryPlanner {
public:
    // Existing methods...

private:
    // NEW: Join planning methods
    PlanNode* planJoinQuery(parser::SelectStmt* select);

    // Generate all paths for a base relation
    std::vector<Path*> generateBaseRelationPaths(
        const parser::TableRef& table_ref);

    // Generate join paths for two relations
    std::vector<Path*> generateJoinPaths(
        Path* left_path,
        Path* right_path,
        const parser::JoinClause& join_clause);

    // Check if hash join is applicable
    bool isHashJoinApplicable(parser::Expression* join_condition) const;

    // Extract hash keys from equi-join condition
    bool extractHashKeys(
        parser::Expression* join_condition,
        std::vector<parser::Expression*>& left_keys,
        std::vector<parser::Expression*>& right_keys) const;

    // Convert Path to PlanNode
    PlanNode* joinPathToPlanNode(Path* path);
};
```

---

## Example Planning Scenarios

### Scenario 1: Simple 2-Table Join

**Query**:
```sql
SELECT * FROM users u INNER JOIN orders o ON u.id = o.user_id
```

**Planning Steps**:
1. Generate base paths:
   - users: SeqScan (cost=100, rows=10,000)
   - orders: SeqScan (cost=1,000, rows=100,000), IndexScan on user_id (cost=5 per lookup)

2. Generate join paths:
   - **Nested Loop** (users outer, orders seq scan inner):
     - Cost: 100 + (10,000 * 1,000) = 10,000,100
   - **Nested Loop** (users outer, orders index scan inner):
     - Cost: 100 + (10,000 * 5) = 50,100
   - **Hash Join** (users build, orders probe):
     - Cost: 300 + 2,500 + 275 = 3,075 ✅ **WINNER**

3. Select cheapest: Hash Join

**Output Plan**:
```
Hash Join (cost=3,075 rows=100,000)
  Hash Cond: (u.id = o.user_id)
  -> Seq Scan on users u (cost=100 rows=10,000)
  -> Seq Scan on orders o (cost=1,000 rows=100,000)
```

### Scenario 2: LEFT OUTER JOIN

**Query**:
```sql
SELECT * FROM users u LEFT JOIN orders o ON u.id = o.user_id
```

**Constraints**:
- users MUST be outer (preserved side)
- Cannot swap join order

**Planning**:
1. Generate paths (same as above)
2. **Only consider paths with users as outer**:
   - Nested Loop (users outer, orders index inner): cost=50,100
   - Hash Join (users build, orders probe): cost=3,075 ✅ **WINNER**

3. Output rows = 10,000 (all users, some with NULL orders)

**Output Plan**:
```
Hash Left Join (cost=3,075 rows=10,000)
  Hash Cond: (u.id = o.user_id)
  -> Seq Scan on users u (cost=100 rows=10,000)
  -> Seq Scan on orders o (cost=1,000 rows=100,000)
```

### Scenario 3: 3-Table Join

**Query**:
```sql
SELECT *
FROM users u
INNER JOIN orders o ON u.id = o.user_id
INNER JOIN products p ON o.product_id = p.id
```

**Planning** (Greedy for Phase 1):
1. Join users + orders (same as Scenario 1):
   - Best: Hash Join (cost=3,075, rows=100,000)

2. Join result with products:
   - Intermediate result: 100,000 rows
   - products: 1,000 rows, IndexScan on id
   - **Nested Loop** (intermediate outer, products index inner):
     - Cost: 3,075 + (100,000 * 5) = 503,075
   - **Hash Join**:
     - Cost: 3,075 + hash_join_cost(100,000, 1,000) ≈ 6,000 ✅ **WINNER**

**Output Plan**:
```
Hash Join (cost=6,000 rows=100,000)
  Hash Cond: (o.product_id = p.id)
  -> Hash Join (cost=3,075 rows=100,000)
       Hash Cond: (u.id = o.user_id)
       -> Seq Scan on users u (cost=100 rows=10,000)
       -> Seq Scan on orders o (cost=1,000 rows=100,000)
  -> Seq Scan on products p (cost=10 rows=1,000)
```

---

## Testing Strategy

### Unit Tests
1. **Test hash key extraction**:
   - `u.id = o.user_id` → extract (u.id, o.user_id) ✅
   - `u.id = o.user_id AND u.type = o.type` → extract both pairs ✅
   - `u.id > o.user_id` → not applicable (no hash keys) ✅

2. **Test join selectivity**:
   - Equi-join with known n_distinct
   - Foreign key join
   - Range join

3. **Test cost estimation**:
   - Nested loop vs. hash join
   - Compare with expected formulas

### Integration Tests
1. **2-table join**: Verify correct plan selection
2. **LEFT/RIGHT OUTER JOIN**: Verify order constraints
3. **3-table join**: Verify greedy join ordering
4. **CROSS JOIN**: Verify nested loop selection

### EXPLAIN Tests
Run `EXPLAIN` on join queries and verify output format:
```
EXPLAIN SELECT * FROM users u JOIN orders o ON u.id = o.user_id;
```

---

## Performance Considerations

### Optimization Opportunities
1. **Index-nested loop joins**: Already considered via generateIndexScanPaths()
2. **Hash join build side selection**: Choose smaller relation as build side
3. **Join reordering**: Defer to Phase 2 (full dynamic programming)

### Limitations (Phase 1)
1. **No join reordering**: Joins execute in query order (except for commutative inner joins)
2. **No merge join**: Only nested loop and hash join
3. **No parallel joins**: Single-threaded execution
4. **Memory management**: Assume hash tables fit in memory

---

## Summary

**Deliverable**: Query planner produces optimal join plans
- ✅ Nested loop join and hash join plan nodes
- ✅ Cost-based join method selection
- ✅ Join selectivity estimation
- ✅ All join types supported (INNER, LEFT, RIGHT, FULL, CROSS)
- ✅ Integration with existing QueryPlanner
- ⏸️ Full join ordering optimization (deferred to Phase 2)

**Estimated Effort**: 15-25 hours
**Next Step**: Task 3.3 - JOIN Execution

---

**Document Status**: Ready for Implementation
**Last Updated**: October 25, 2025
