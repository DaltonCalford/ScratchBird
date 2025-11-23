# Query Planner Design Document

**Phase**: Phase 1, Task 1.3 - Query Optimizer Foundation
**Component**: Basic Query Planner
**Date**: October 25, 2025
**Status**: In Development (0% Complete)

---

## Overview

The query planner generates optimized execution plans for SQL queries. It uses statistics (Task 1.1) and the cost model (Task 1.2) to choose the best execution strategy among multiple alternatives.

**Scope for Task 1.3**: Single-table queries only
- SELECT with WHERE clauses
- Index selection
- Sequential vs index scan choice
- Future: joins, aggregates, subqueries

## Architecture

### Query Planning Pipeline

```
SQL Query → Parser → AST
  ↓
Semantic Analysis (validate tables/columns exist)
  ↓
Query Planner (THIS COMPONENT)
  ↓
  1. Generate Access Paths
     - Sequential scan path
     - Index scan paths (one per applicable index)
  ↓
  2. Cost Each Path
     - Use statistics to estimate rows
     - Use cost model to estimate cost
  ↓
  3. Choose Cheapest Path
     - Compare total costs
     - Select minimum cost path
  ↓
  4. Generate Plan Tree
     - Convert chosen path to execution plan
  ↓
Bytecode Generator → SBLR Bytecode
  ↓
Executor → Results
```

### Key Data Structures

#### 1. PlanNode - Execution Plan Node

```cpp
enum class PlanNodeType {
    SEQ_SCAN,      // Sequential table scan
    INDEX_SCAN,    // Index scan with heap fetch
    INDEX_ONLY_SCAN, // Index-only scan (future)
    NESTED_LOOP,   // Nested loop join (future)
    HASH_JOIN,     // Hash join (future)
    MERGE_JOIN,    // Merge join (future)
    SORT,          // Sort operation (future)
    AGGREGATE,     // Aggregation (future)
    LIMIT          // Limit/offset (future)
};

class PlanNode {
public:
    PlanNodeType type;
    CostEstimate cost;  // From Task 1.2

    // Output schema
    std::vector<ColumnInfo> output_columns;

    // Child nodes (for joins, sorts, etc.)
    std::vector<std::unique_ptr<PlanNode>> children;

    virtual ~PlanNode() = default;
};
```

#### 2. SeqScanNode - Sequential Scan Plan

```cpp
class SeqScanNode : public PlanNode {
public:
    ID table_id;
    std::string table_name;

    // Filter predicates (WHERE clause)
    std::vector<Expression*> filter_quals;

    // Estimated rows after filtering
    uint64_t estimated_rows;
};
```

#### 3. IndexScanNode - Index Scan Plan

```cpp
class IndexScanNode : public PlanNode {
public:
    ID table_id;
    std::string table_name;

    ID index_id;
    std::string index_name;

    // Index search conditions (e.g., id = 42)
    std::vector<Expression*> index_quals;

    // Additional filters (not usable by index)
    std::vector<Expression*> filter_quals;

    // Scan direction (forward/backward)
    ScanDirection scan_direction = FORWARD;

    // Estimated rows
    uint64_t estimated_rows;
};
```

#### 4. Path - Potential Execution Path

Paths are intermediate representations used during planning.
The cheapest path is converted to a PlanNode.

```cpp
enum class PathType {
    SEQ_SCAN_PATH,
    INDEX_SCAN_PATH,
    INDEX_ONLY_SCAN_PATH,
    // Future: JOIN_PATH, AGGREGATE_PATH, etc.
};

class Path {
public:
    PathType type;
    CostEstimate cost;
    uint64_t estimated_rows;

    // Parent relation (table being scanned)
    ID table_id;

    virtual ~Path() = default;
    virtual std::unique_ptr<PlanNode> createPlan() = 0;
};

class SeqScanPath : public Path {
public:
    std::vector<Expression*> quals;  // WHERE predicates

    std::unique_ptr<PlanNode> createPlan() override;
};

class IndexScanPath : public Path {
public:
    ID index_id;
    std::vector<Expression*> index_quals;  // Index-usable predicates
    std::vector<Expression*> filter_quals; // Other predicates

    std::unique_ptr<PlanNode> createPlan() override;
};
```

### Query Planner Class

```cpp
class QueryPlanner {
public:
    QueryPlanner(Database *db, StatisticsManager *stats_mgr,
                 CostModel *cost_model);

    // Main planning entry point
    std::unique_ptr<PlanNode> planQuery(SelectStmt *select_stmt,
                                        ErrorContext *ctx = nullptr);

private:
    // Path generation
    std::vector<std::unique_ptr<Path>> generateScanPaths(
        const ID &table_id,
        const std::vector<Expression*> &quals,
        ErrorContext *ctx);

    // Sequential scan path
    std::unique_ptr<SeqScanPath> createSeqScanPath(
        const ID &table_id,
        const std::vector<Expression*> &quals,
        ErrorContext *ctx);

    // Index scan paths (one per applicable index)
    std::vector<std::unique_ptr<IndexScanPath>> createIndexScanPaths(
        const ID &table_id,
        const std::vector<Expression*> &quals,
        ErrorContext *ctx);

    // Cost estimation
    void costSeqScanPath(SeqScanPath *path, ErrorContext *ctx);
    void costIndexScanPath(IndexScanPath *path, ErrorContext *ctx);

    // Path selection
    Path* chooseCheapestPath(const std::vector<std::unique_ptr<Path>> &paths);

    // Index applicability
    bool isIndexApplicable(const IndexInfo &index,
                          const std::vector<Expression*> &quals);

    // Selectivity estimation (Task 1.4 - stub for now)
    double estimateSelectivity(const std::vector<Expression*> &quals,
                              const ID &table_id);

private:
    Database *db_;
    StatisticsManager *stats_mgr_;
    CostModel *cost_model_;
    CatalogManager *catalog_;
};
```

## Implementation Plan

### Task 1.3.1: PlanNode Structures (6-8 hours)

Create `include/scratchbird/optimizer/plan_node.h`:

```cpp
namespace scratchbird::optimizer {

enum class PlanNodeType : uint8_t {
    SEQ_SCAN = 0,
    INDEX_SCAN = 1,
    // Future: joins, sorts, aggregates
};

enum class ScanDirection : uint8_t {
    FORWARD = 0,
    BACKWARD = 1
};

// Base class for all plan nodes
class PlanNode {
public:
    PlanNodeType type;
    CostEstimate cost;
    uint64_t estimated_rows;

    explicit PlanNode(PlanNodeType node_type)
        : type(node_type), estimated_rows(0) {}

    virtual ~PlanNode() = default;

    // For debugging/EXPLAIN
    virtual std::string toString() const = 0;
};

// Sequential scan node
class SeqScanNode : public PlanNode {
public:
    ID table_id;
    std::string table_name;
    std::vector<parser::Expression*> quals;

    SeqScanNode()
        : PlanNode(PlanNodeType::SEQ_SCAN) {}

    std::string toString() const override;
};

// Index scan node
class IndexScanNode : public PlanNode {
public:
    ID table_id;
    std::string table_name;
    ID index_id;
    std::string index_name;
    std::vector<parser::Expression*> index_quals;
    std::vector<parser::Expression*> filter_quals;
    ScanDirection direction = ScanDirection::FORWARD;

    IndexScanNode()
        : PlanNode(PlanNodeType::INDEX_SCAN) {}

    std::string toString() const override;
};

} // namespace
```

### Task 1.3.2: Path Structures (4-6 hours)

Create `include/scratchbird/optimizer/path.h`:

```cpp
namespace scratchbird::optimizer {

enum class PathType : uint8_t {
    SEQ_SCAN_PATH = 0,
    INDEX_SCAN_PATH = 1
};

class Path {
public:
    PathType type;
    CostEstimate cost;
    uint64_t estimated_rows;
    ID table_id;

    explicit Path(PathType path_type)
        : type(path_type), estimated_rows(0) {}

    virtual ~Path() = default;
    virtual std::unique_ptr<PlanNode> createPlan() = 0;
};

class SeqScanPath : public Path {
public:
    std::vector<parser::Expression*> quals;

    SeqScanPath() : Path(PathType::SEQ_SCAN_PATH) {}

    std::unique_ptr<PlanNode> createPlan() override {
        auto node = std::make_unique<SeqScanNode>();
        node->table_id = table_id;
        node->quals = quals;
        node->cost = cost;
        node->estimated_rows = estimated_rows;
        return node;
    }
};

class IndexScanPath : public Path {
public:
    ID index_id;
    std::vector<parser::Expression*> index_quals;
    std::vector<parser::Expression*> filter_quals;

    IndexScanPath() : Path(PathType::INDEX_SCAN_PATH) {}

    std::unique_ptr<PlanNode> createPlan() override {
        auto node = std::make_unique<IndexScanNode>();
        node->table_id = table_id;
        node->index_id = index_id;
        node->index_quals = index_quals;
        node->filter_quals = filter_quals;
        node->cost = cost;
        node->estimated_rows = estimated_rows;
        return node;
    }
};

} // namespace
```

### Task 1.3.3: Query Planner (12-18 hours)

Create `include/scratchbird/optimizer/query_planner.h` and implementation.

#### Path Generation Algorithm

```
generateScanPaths(table_id, quals):
    paths = []

    // Always consider sequential scan
    seq_path = createSeqScanPath(table_id, quals)
    costSeqScanPath(seq_path)
    paths.add(seq_path)

    // Consider each applicable index
    indexes = catalog.getIndexes(table_id)
    for index in indexes:
        if isIndexApplicable(index, quals):
            idx_path = createIndexScanPath(table_id, index, quals)
            costIndexScanPath(idx_path)
            paths.add(idx_path)

    return paths
```

#### Index Applicability

An index is applicable if:
1. Index column appears in WHERE clause
2. Operator is index-scannable (=, <, >, <=, >=, BETWEEN, IN)
3. B-tree index (future: hash, GiST, GIN)

```
isIndexApplicable(index, quals):
    for qual in quals:
        if qual.column == index.column:
            if qual.operator in [=, <, >, <=, >=, BETWEEN, IN]:
                return true
    return false
```

#### Cost Estimation Integration

```cpp
void QueryPlanner::costSeqScanPath(SeqScanPath *path, ErrorContext *ctx) {
    // Get table statistics
    TableStatistics table_stats;
    stats_mgr_->getTableStatistics(path->table_id, table_stats, ctx);

    // Estimate selectivity (Task 1.4 - use stub for now)
    double selectivity = estimateSelectivity(path->quals, path->table_id);

    // Estimate rows after filtering
    uint64_t estimated_rows = static_cast<uint64_t>(
        table_stats.num_rows * selectivity);
    path->estimated_rows = estimated_rows;

    // Calculate qual evaluation cost
    double qual_cost = 0.0;
    for (auto *qual : path->quals) {
        qual_cost += cost_model_->operatorCost(qual->op);
    }

    // Use cost model
    path->cost = cost_model_->costSeqScan(
        table_stats.num_pages,
        table_stats.num_rows,  // Scan all rows
        qual_cost
    );
}
```

```cpp
void QueryPlanner::costIndexScanPath(IndexScanPath *path, ErrorContext *ctx) {
    // Get table statistics
    TableStatistics table_stats;
    stats_mgr_->getTableStatistics(path->table_id, table_stats, ctx);

    // Get index statistics
    // For now, estimate index size as 10% of table size
    uint64_t index_pages = table_stats.num_pages / 10;

    // Estimate selectivity of index conditions
    double index_selectivity = estimateSelectivity(path->index_quals, path->table_id);

    // Estimate rows returned by index scan
    uint64_t estimated_rows = static_cast<uint64_t>(
        table_stats.num_rows * index_selectivity);
    path->estimated_rows = estimated_rows;

    // Calculate qual costs
    double index_qual_cost = 0.0;
    for (auto *qual : path->index_quals) {
        index_qual_cost += cost_model_->operatorCost(qual->op);
    }

    double filter_qual_cost = 0.0;
    for (auto *qual : path->filter_quals) {
        filter_qual_cost += cost_model_->operatorCost(qual->op);
    }

    // Estimate B-tree height (log base 100 of rows)
    uint64_t index_height = static_cast<uint64_t>(
        std::max(2.0, std::ceil(std::log(table_stats.num_rows) / std::log(100.0))));

    // Get correlation (0.0 for now, Task 1.4 will improve this)
    double correlation = 0.0;

    // Use cost model
    path->cost = cost_model_->costIndexScan(
        index_height,
        index_pages,
        estimated_rows,  // Index tuples scanned
        estimated_rows,  // Heap pages (worst case)
        estimated_rows,  // Heap tuples
        filter_qual_cost,
        correlation
    );
}
```

#### Cheapest Path Selection

```cpp
Path* QueryPlanner::chooseCheapestPath(
    const std::vector<std::unique_ptr<Path>> &paths) {

    if (paths.empty()) {
        return nullptr;
    }

    Path *cheapest = paths[0].get();
    double min_cost = cheapest->cost.total_cost;

    for (size_t i = 1; i < paths.size(); i++) {
        double cost = paths[i]->cost.total_cost;
        if (cost < min_cost) {
            min_cost = cost;
            cheapest = paths[i].get();
        }
    }

    DEBUG_LOG_DB("Chose path with cost " + std::to_string(min_cost));
    return cheapest;
}
```

### Task 1.3.4: Selectivity Estimation Stub (2-4 hours)

For Task 1.3, use simple selectivity estimates:
- No WHERE clause: 1.0 (all rows)
- Equality (=): 0.01 (1%)
- Inequality (>, <, >=, <=): 0.33 (33%)
- LIKE: 0.1 (10%)
- Default: 0.5 (50%)

Task 1.4 will implement proper histogram-based selectivity.

```cpp
double QueryPlanner::estimateSelectivity(
    const std::vector<Expression*> &quals,
    const ID &table_id) {

    if (quals.empty()) {
        return 1.0;  // No filtering
    }

    // Simple estimates for now
    double selectivity = 1.0;

    for (auto *qual : quals) {
        if (qual->op == "=") {
            selectivity *= 0.01;  // 1%
        } else if (qual->op == ">" || qual->op == "<" ||
                   qual->op == ">=" || qual->op == "<=") {
            selectivity *= 0.33;  // 33%
        } else if (qual->op == "LIKE") {
            selectivity *= 0.1;   // 10%
        } else {
            selectivity *= 0.5;   // 50% default
        }
    }

    return std::max(0.0001, selectivity);  // Min 0.01%
}
```

### Task 1.3.5: Integration with Parser (4-6 hours)

Modify query execution flow:

```
OLD:
  Parser → SelectStmt → BytecodeGenerator → Executor

NEW:
  Parser → SelectStmt → QueryPlanner → PlanNode → BytecodeGenerator → Executor
```

Integration point in executor or a new planner entry point:

```cpp
Status executeSelect(SelectStmt *stmt, ErrorContext *ctx) {
    // 1. Plan the query
    auto planner = std::make_unique<QueryPlanner>(db_, stats_mgr_, cost_model_);
    auto plan = planner->planQuery(stmt, ctx);

    if (!plan) {
        return Status::INVALID_ARGUMENT;
    }

    // 2. Generate bytecode from plan
    auto bytecode = bytecode_gen_->generateFromPlan(plan.get());

    // 3. Execute bytecode
    return executor_->execute(bytecode);
}
```

## Testing Strategy

### Unit Tests
- [ ] PlanNode creation and toString()
- [ ] Path creation and cost assignment
- [ ] Sequential scan path generation
- [ ] Index scan path generation
- [ ] Index applicability detection
- [ ] Cheapest path selection

### Integration Tests
- [ ] Plan simple SELECT (no WHERE)
- [ ] Plan SELECT with WHERE (equality)
- [ ] Plan SELECT with WHERE (range)
- [ ] Choose seqscan for low selectivity
- [ ] Choose indexscan for high selectivity
- [ ] Handle table with no indexes

### Performance Tests
- [ ] Planning completes in < 10ms
- [ ] Memory usage reasonable (< 1MB for simple query)

## Example Planning Scenarios

### Scenario 1: No Indexes

```sql
SELECT * FROM users WHERE age > 25
```

**Paths Generated**:
1. SeqScan(users) - filter: age > 25

**Cost Estimation**:
- pages = 100, rows = 10,000
- selectivity = 0.33 (range query)
- estimated_rows = 3,300
- cost = 100 * 1.0 + 10,000 * (0.01 + 0.0025) = 225.0

**Chosen Plan**: SeqScan (only option)

### Scenario 2: Applicable Index

```sql
SELECT * FROM users WHERE id = 42
```

Assume B-tree index on `id` column.

**Paths Generated**:
1. SeqScan(users) - filter: id = 42
2. IndexScan(idx_users_id) - index_qual: id = 42

**Cost Estimation**:

SeqScan:
- selectivity = 0.01 (equality)
- estimated_rows = 100
- cost = 100 * 1.0 + 10,000 * 0.0125 = 225.0

IndexScan:
- selectivity = 0.01
- estimated_rows = 100
- index_height = 3
- index_pages = 10
- heap_pages = 100 (worst case)
- cost = 0.0075 + 10 * 4.0 + 100 * 4.0 = 440.0075

Wait, this doesn't look right! The index scan should be cheaper.

**Issue**: We're assuming 100 heap pages for 100 rows. Should be ~1 page.

**Fix**: Better heap page estimation:
```
heap_pages = estimated_rows / 100  (100 tuples per page)
           = 100 / 100 = 1
```

IndexScan (corrected):
- cost = 0.0075 + 10 * 4.0 + 1 * 4.0 + 100 * 0.0125 = 45.2575

**Chosen Plan**: IndexScan (cheaper!)

### Scenario 3: Index Not Applicable

```sql
SELECT * FROM users WHERE name LIKE '%john%'
```

Assume B-tree index on `name` column.

**Issue**: LIKE with leading wildcard cannot use index.

**Paths Generated**:
1. SeqScan(users) - filter: name LIKE '%john%'

**Chosen Plan**: SeqScan (index not applicable)

## Design Decisions

### 1. Path-based Planning

**Choice**: Generate multiple paths, cost each, choose cheapest

**Rationale**:
- PostgreSQL-proven approach
- Extensible to joins (cross product of paths)
- Clean separation: path generation → costing → selection

### 2. Single-table Only (Phase 1)

**Choice**: Defer joins to Phase 2

**Rationale**:
- Simplifies initial implementation
- Single-table queries are common
- Provides immediate value

### 3. Simple Selectivity Estimates

**Choice**: Use constants for Task 1.3, improve in Task 1.4

**Rationale**:
- Unblocks planner development
- Task 1.4 will add histogram-based estimates
- Good enough for basic testing

## Future Enhancements (Phase 2+)

1. **Join Planning**
   - Nested loop, hash join, merge join
   - Join order optimization
   - Multi-table path generation

2. **Aggregate Pushdown**
   - Index-only scans for COUNT(*)
   - Grouped aggregates

3. **Sort Optimization**
   - Index-ordered scans
   - Sort avoidance

4. **Parallel Query**
   - Parallel sequential scan
   - Parallel aggregate

## References

1. PostgreSQL Source: `src/backend/optimizer/path/`
   - `allpaths.c` - Path generation
   - `costsize.c` - Cost estimation
   - `indxpath.c` - Index path creation

2. "Access Path Selection in a Relational Database Management System"
   Selinger et al., SIGMOD 1979
   Classic paper on cost-based optimization

---

**Document Version**: 1.0
**Last Updated**: October 25, 2025
**Author**: Claude Code
**Status**: Design Complete - Ready for Implementation
