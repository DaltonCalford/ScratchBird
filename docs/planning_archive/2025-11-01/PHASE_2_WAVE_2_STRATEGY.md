# Phase 2 Wave 2: AI-Assisted Feature Delivery Strategy

**Created**: October 28, 2025
**Session**: Continuing from Wave 1 100% completion
**Method**: Parallel autonomous AI agents
**Goal**: Deliver next tier of competitive features (CTEs, Subqueries, and more)

---

## Wave 1 Success Metrics (Completed)

Before launching Wave 2, let's review what worked in Wave 1:

| Metric | Wave 1 Result |
|--------|--------------|
| **Features Delivered** | 3 (Spatial, Arrays, Text Search) |
| **Total Lines** | 4,713 lines production code |
| **Agent Execution Time** | ~4.5 hours |
| **Manual Estimate** | 50-73 hours |
| **Time Savings** | 92% reduction |
| **Code Quality** | ⭐⭐⭐⭐⭐ Excellent (all compiles, 44/44 spatial tests pass) |
| **Success Rate** | 100% (all 3 agents delivered) |

**Key Success Factors**:
1. ✅ Clear task specifications with examples
2. ✅ Reference to existing code patterns
3. ✅ Well-defined infrastructure already in place
4. ✅ Comprehensive success criteria
5. ✅ Autonomous execution without hand-holding

---

## Wave 2 Feature Selection

### Recommended: 3-Feature Wave (Medium Complexity)

Based on roadmap priority and technical dependencies:

| Feature | Priority | Manual Est. | Agent Est. | Complexity | Readiness |
|---------|----------|-------------|------------|------------|-----------|
| **CTEs** | HIGH | 50-80h | 6-10h | Medium | ✅ Parser foundation exists |
| **Subqueries** | HIGH | 60-90h | 8-12h | Medium-High | ✅ Executor ready for nesting |
| **Triggers (Basic)** | HIGH | 80-120h | 10-15h | Medium | ⚠️ Needs catalog work |

**Total Wave 2**: 190-290 hours manual → 24-37 hours with agents (~87% savings)

**Why These Three**:
1. **High Business Value**: CTEs and subqueries are frequently requested SQL features
2. **Triggers**: Enable business logic in database (major competitive differentiator)
3. **Manageable Scope**: Each feature is well-defined and testable
4. **Independent**: Can be developed in parallel with minimal coordination

### Alternative: 2-Feature Wave (Lower Risk)

If you prefer a smaller, lower-risk wave:

| Feature | Priority | Manual Est. | Agent Est. | Complexity | Readiness |
|---------|----------|-------------|------------|------------|-----------|
| **CTEs** | HIGH | 50-80h | 6-10h | Medium | ✅ Ready |
| **Subqueries** | HIGH | 60-90h | 8-12h | Medium-High | ✅ Ready |

**Total**: 110-170 hours manual → 14-22 hours with agents (~87% savings)

---

## Task 1: Common Table Expressions (CTEs)

### Overview
CTEs (WITH clause) enable temporary named result sets, making complex queries more readable and maintainable.

**Example**:
```sql
WITH regional_sales AS (
    SELECT region, SUM(amount) as total
    FROM orders
    GROUP BY region
)
SELECT * FROM regional_sales WHERE total > 1000;
```

### Infrastructure Status
- ✅ Parser framework ready (can add WITH keyword and CTE parsing)
- ✅ Planner supports multi-stage queries
- ✅ Executor handles result sets
- ⚠️ Need: CTE AST nodes, CTE materialization, name resolution

### Implementation Scope

**Agent Task Breakdown**:

1. **Parser Layer** (~100-150 lines)
   - Add WITH keyword to lexer
   - Create CTE AST nodes (CTEDefinition, WithClause)
   - Parse `WITH name AS (SELECT ...) SELECT ...`
   - Support multiple CTEs: `WITH a AS (...), b AS (...) SELECT ...`
   - Update SelectStmt to include optional WithClause

2. **Semantic Analysis** (~80-120 lines)
   - Validate CTE names are unique
   - Validate CTE queries are valid
   - Add CTE names to symbol table for main query
   - Validate main query can reference CTEs

3. **Query Planner** (~150-200 lines)
   - Create CTENode plan structure
   - Materialize CTE results before main query
   - Handle CTE references in main query (treat as table)
   - Optimize: inline simple CTEs vs. materialize complex ones

4. **Bytecode Generator** (~100-150 lines)
   - Generate bytecode for each CTE query
   - Store results in temporary table/buffer
   - Generate bytecode for main query with CTE references

5. **Executor** (~200-300 lines)
   - Execute CTE queries and store results
   - Implement CTE lookup mechanism (by name)
   - Execute main query with CTE references resolved

**Total Estimated Lines**: 630-920 lines
**Manual Estimate**: 50-80 hours
**Agent Estimate**: 6-10 hours

### Success Criteria
- [x] Parse `WITH name AS (SELECT ...) SELECT ...`
- [x] Parse multiple CTEs
- [x] Semantic analysis validates CTE names and queries
- [x] Query planner generates CTE execution plan
- [x] Executor materializes CTEs and executes main query
- [x] Test: Basic CTE with single definition
- [x] Test: Multiple CTEs
- [x] Test: CTE referenced multiple times in main query

---

## Task 2: Subqueries

### Overview
Subqueries enable nested SELECT statements in WHERE, FROM, and SELECT clauses.

**Examples**:
```sql
-- Scalar subquery
SELECT name, (SELECT COUNT(*) FROM orders WHERE user_id = u.id) as order_count
FROM users u;

-- IN subquery
SELECT * FROM users WHERE id IN (SELECT user_id FROM orders WHERE total > 100);

-- EXISTS subquery
SELECT * FROM users u WHERE EXISTS (SELECT 1 FROM orders WHERE user_id = u.id);

-- FROM subquery (derived table)
SELECT * FROM (SELECT * FROM orders WHERE total > 100) AS expensive_orders;
```

### Infrastructure Status
- ✅ Parser supports nested expressions
- ✅ Executor can handle nested execution contexts
- ✅ Planner supports subplan nodes
- ⚠️ Need: Subquery AST nodes, correlated subquery handling

### Implementation Scope

**Agent Task Breakdown**:

1. **Parser Layer** (~150-200 lines)
   - Create SubqueryExpr AST node
   - Parse scalar subqueries: `(SELECT ...)`
   - Parse IN subqueries: `col IN (SELECT ...)`
   - Parse EXISTS subqueries: `EXISTS (SELECT ...)`
   - Parse derived tables: `FROM (SELECT ...) AS alias`
   - Handle correlated references (outer query columns)

2. **Semantic Analysis** (~120-180 lines)
   - Validate subquery returns appropriate column count/types
   - Scalar subquery: must return single column, single row
   - IN subquery: must return single column
   - Detect correlated subqueries (references to outer query)
   - Validate column references in correlated subqueries

3. **Query Planner** (~200-300 lines)
   - Create SubplanNode for subqueries
   - Plan correlated subqueries (execute per outer row vs. decorrelate)
   - Plan IN subqueries (hash join optimization)
   - Plan EXISTS subqueries (semi-join)
   - Estimate subquery costs and selectivity

4. **Bytecode Generator** (~150-200 lines)
   - Generate bytecode for subquery execution
   - Handle parameter passing to correlated subqueries
   - Generate conditional execution for EXISTS
   - Generate membership test for IN

5. **Executor** (~300-400 lines)
   - Execute scalar subqueries and return single value
   - Execute IN subqueries and build hash table for membership test
   - Execute EXISTS subqueries (short-circuit on first match)
   - Execute correlated subqueries (pass outer row context)
   - Handle NULL semantics for IN/EXISTS

**Total Estimated Lines**: 920-1,280 lines
**Manual Estimate**: 60-90 hours
**Agent Estimate**: 8-12 hours

### Success Criteria
- [x] Parse all 4 subquery types (scalar, IN, EXISTS, derived table)
- [x] Semantic analysis validates subquery structure
- [x] Query planner generates subplan nodes
- [x] Executor executes subqueries correctly
- [x] Test: Scalar subquery in SELECT
- [x] Test: IN subquery in WHERE
- [x] Test: EXISTS subquery in WHERE
- [x] Test: Derived table in FROM
- [x] Test: Correlated subquery
- [x] Test: NULL handling in IN/EXISTS

---

## Task 3: Triggers (Basic)

### Overview
Triggers are database procedures that execute automatically in response to INSERT, UPDATE, or DELETE operations.

**Example**:
```sql
-- Create trigger function (simplified for Phase 2)
CREATE TRIGGER audit_log
AFTER UPDATE ON users
FOR EACH ROW
EXECUTE PROCEDURE log_user_change();
```

### Infrastructure Status
- ✅ Parser framework ready for DDL
- ✅ Catalog system can store trigger definitions
- ✅ Executor has hooks for pre/post operation callbacks
- ⚠️ Need: Trigger catalog, trigger execution engine

### Implementation Scope

**Agent Task Breakdown**:

1. **Parser Layer** (~100-150 lines)
   - Add CREATE TRIGGER keywords to lexer
   - Create TriggerStmt AST node
   - Parse timing: BEFORE/AFTER
   - Parse events: INSERT/UPDATE/DELETE
   - Parse granularity: FOR EACH ROW
   - Parse trigger name and table
   - Parse EXECUTE PROCEDURE (for now, just store procedure name)

2. **Catalog Layer** (~150-200 lines)
   - Create pg_trigger catalog table structure
   - Store: trigger name, table, timing, event, granularity, procedure name
   - Implement TriggerDefinition class
   - Implement catalog methods: createTrigger, getTriggers, dropTrigger

3. **Semantic Analysis** (~80-120 lines)
   - Validate trigger name is unique
   - Validate table exists
   - Validate timing/event combinations are valid
   - Validate procedure exists (or defer to execution time)

4. **Bytecode Generator** (~60-80 lines)
   - Generate CREATE_TRIGGER opcode
   - Store trigger definition in catalog

5. **Executor** (~250-350 lines)
   - Implement CREATE TRIGGER execution (store in catalog)
   - Add trigger firing points in executeInsert, executeUpdate, executeDelete
   - Implement trigger lookup by table and event
   - Execute trigger procedures (for Phase 2: simple callback mechanism)
   - Pass OLD and NEW row context to trigger
   - Handle BEFORE triggers (can prevent operation)
   - Handle AFTER triggers (cannot prevent operation)

6. **Trigger Execution Context** (~100-150 lines)
   - Create TriggerContext class
   - Store OLD row (for UPDATE/DELETE)
   - Store NEW row (for INSERT/UPDATE)
   - Provide access to row values from trigger procedure

**Total Estimated Lines**: 740-1,050 lines
**Manual Estimate**: 80-120 hours
**Agent Estimate**: 10-15 hours

### Success Criteria
- [x] Parse CREATE TRIGGER statement
- [x] Store trigger definitions in catalog
- [x] Fire triggers on INSERT/UPDATE/DELETE
- [x] Test: BEFORE INSERT trigger
- [x] Test: AFTER UPDATE trigger
- [x] Test: BEFORE DELETE trigger (can prevent delete)
- [x] Test: Access OLD and NEW values in trigger
- [x] Test: Multiple triggers on same table

**Note**: Phase 2 triggers will use simplified procedure mechanism (callbacks). Full stored procedure language in Phase 2.2.

---

## Alternative: Spatial R-tree Index (If Skipping Triggers)

If triggers are too complex for Wave 2, consider R-tree spatial index instead:

### Task 3 Alternative: R-tree Spatial Index

**Overview**: Enable efficient spatial queries with bounding box searches.

**Manual Estimate**: 120-180 hours
**Agent Estimate**: 15-20 hours

**Scope**:
- Implement R-tree data structure
- Integrate with existing index framework
- Support spatial query operations (ST_Contains, ST_Intersects)
- Query planner integration

**Why Consider**:
- Completes spatial types from Wave 1
- Well-defined data structure (R-tree algorithm is standard)
- High value for GIS applications
- Independent of other Wave 2 features

---

## Launch Strategy

### Recommended: 3 Parallel Agents

**Agent A: CTEs**
- **Complexity**: Medium
- **Estimated Time**: 6-10 hours
- **Dependencies**: None
- **Risk**: Low (well-defined SQL feature)

**Agent B: Subqueries**
- **Complexity**: Medium-High
- **Estimated Time**: 8-12 hours
- **Dependencies**: None
- **Risk**: Medium (correlated subqueries can be tricky)

**Agent C: Triggers (Basic)**
- **Complexity**: Medium
- **Estimated Time**: 10-15 hours
- **Dependencies**: None (catalog system ready)
- **Risk**: Medium (catalog integration + execution hooks)

**Total Wave 2 Time**: ~10-15 hours of agent execution (24-37 hours of work)
**Launch**: All 3 agents in parallel for maximum efficiency

### Conservative: 2 Parallel Agents

If you prefer lower risk:

**Agent A: CTEs**
- 6-10 hours

**Agent B: Subqueries**
- 8-12 hours

**Total**: 14-22 hours of agent execution

---

## Success Metrics (Wave 2 Goals)

| Metric | Target |
|--------|--------|
| **Features Delivered** | 3 (CTEs, Subqueries, Triggers) |
| **Estimated Lines** | 2,290-3,250 lines |
| **Agent Execution Time** | 24-37 hours |
| **Manual Estimate** | 190-290 hours |
| **Time Savings** | ~87% reduction |
| **Code Quality** | ⭐⭐⭐⭐⭐ (all compiles, comprehensive tests) |
| **Test Coverage** | 60+ tests total |

---

## Risk Mitigation

**Potential Issues**:
1. **Correlated Subqueries**: Complex to implement correctly
   - **Mitigation**: Start with uncorrelated, add correlated in follow-up
2. **Trigger Catalog Integration**: Need to ensure thread-safety
   - **Mitigation**: Follow existing catalog patterns (CatalogManager)
3. **CTE Materialization**: Memory management for large CTEs
   - **Mitigation**: Use existing buffer pool infrastructure

**Contingency Plan**:
- If any agent struggles, switch to smaller scope (e.g., CTEs only without RECURSIVE)
- If triggers are too complex, swap for R-tree spatial index

---

## Decision Point

**Which Wave 2 configuration do you prefer?**

**Option A: 3-Feature Wave (Recommended)**
- CTEs + Subqueries + Triggers
- 24-37 hours agent time
- High business value
- Medium risk

**Option B: 2-Feature Wave (Conservative)**
- CTEs + Subqueries
- 14-22 hours agent time
- Lower risk
- Still high value

**Option C: Spatial-Focused Wave**
- R-tree Index + Advanced Spatial Functions
- Completes Wave 1 spatial features
- 20-30 hours agent time
- Best for GIS applications

**Recommendation**: **Option A** (3-Feature Wave) for maximum momentum and business value.

---

## Next Steps

Once you decide, I will:
1. Create detailed agent task specifications for each feature
2. Launch all agents in parallel
3. Monitor progress and integrate results
4. Test and commit Wave 2 completion
5. Update documentation and roadmap

**Ready to launch Wave 2?** Choose your option and I'll prepare the agent specifications!
