# ROLLUP/CUBE/GROUPING SETS Executor Implementation Guide

**Status:** Ready for Implementation
**Estimated Effort:** 18-24 hours
**Dependencies:** Parser ✅, Optimizer ✅, Bytecode ✅

---

## Overview

This document provides a detailed implementation guide for the final piece of ROLLUP/CUBE/GROUPING SETS support: the executor layer.

**Current Status:**
- ✅ Parser: Complete (recognizes SQL syntax)
- ✅ Optimizer: Complete (propagates grouping type)
- ✅ Bytecode: Complete (expands into grouping sets)
- ❌ Executor: **TODO** (this document)

---

## Bytecode Format (Reference)

The bytecode generator emits the following format:

```
EXTENDED_OPCODE (0xFF)
EXT_GROUP_ROLLUP/CUBE/GROUPING_SETS (0x45/0x46/0x47)
num_grouping_sets (uint32)          -- e.g., 4 for ROLLUP(a,b,c)
total_grouping_columns (uint32)     -- e.g., 3 for ROLLUP(a,b,c)

For each grouping set:
  GROUP_BY (0xC9)
  num_columns_in_set (uint32)       -- e.g., 2, 1, 0 for sets (a,b), (a), ()
  column_expression_bytecode...

[Followed by standard AGG_INIT, aggregate functions, HAVING, AGG_FINALIZE]
```

**Example: ROLLUP(region, product)**
```
0xFF 0x45         -- EXTENDED_OPCODE, EXT_GROUP_ROLLUP
03 00 00 00       -- 3 grouping sets
02 00 00 00       -- 2 total grouping columns

0xC9              -- GROUP_BY for set (region, product)
02 00 00 00       -- 2 columns
[region expression bytecode]
[product expression bytecode]

0xC9              -- GROUP_BY for set (region)
01 00 00 00       -- 1 column
[region expression bytecode]

0xC9              -- GROUP_BY for set ()
00 00 00 00       -- 0 columns (grand total)

[AGG_INIT, aggregates, HAVING, AGG_FINALIZE]
```

---

## Implementation Location

**File:** `src/sblr/executor.cpp`
**Function:** `Executor::executeAggregation(...)` (around line 6522)

**Approach:** Detect extended grouping opcodes before standard GROUP_BY processing.

---

## Implementation Steps

### Step 1: Detect Advanced Grouping (5 minutes)

**Location:** `executeAggregation` function, before line 6528

```cpp
// Check for advanced grouping BEFORE standard GROUP_BY check
if (pc_ < bytecode_size_ && bytecode_[pc_] == static_cast<uint8_t>(Opcode::EXTENDED_OPCODE))
{
    size_t saved_pc = pc_;
    readByte(); // consume EXTENDED_OPCODE

    uint8_t ext_op = readByte();
    if (ext_op == static_cast<uint8_t>(Opcode::EXT_GROUP_ROLLUP) ||
        ext_op == static_cast<uint8_t>(Opcode::EXT_GROUP_CUBE) ||
        ext_op == static_cast<uint8_t>(Opcode::EXT_GROUP_GROUPING_SETS))
    {
        // Advanced grouping detected - handle in separate function
        executeAdvancedGrouping(table_info, all_columns, select_items,
                               is_select_star, has_where, where_start_pc, where_end_pc);
        return;
    }
    else
    {
        // Not advanced grouping - restore PC and continue
        pc_ = saved_pc;
    }
}

// Standard GROUP BY processing continues here...
```

### Step 2: Parse Grouping Set Metadata (30 minutes)

**New Function:** `Executor::executeAdvancedGrouping(...)`

```cpp
void Executor::executeAdvancedGrouping(
    const core::CatalogManager::TableInfo& table_info,
    const std::vector<core::CatalogManager::ColumnInfo>& all_columns,
    const std::vector<std::pair<std::string, std::string>>& select_items,
    bool is_select_star,
    bool has_where,
    size_t where_start_pc,
    size_t where_end_pc)
{
    // Read metadata
    uint32_t num_grouping_sets = readInt32();
    uint32_t total_grouping_columns = readInt32();

    // Parse each grouping set's GROUP BY specification
    struct GroupingSet
    {
        std::vector<size_t> column_expr_pcs;  // Bytecode positions
        std::vector<size_t> column_indices;    // For GROUPING() function
    };

    std::vector<GroupingSet> grouping_sets;

    for (uint32_t set_idx = 0; set_idx < num_grouping_sets; set_idx++)
    {
        // Read GROUP_BY opcode
        if (readByte() != static_cast<uint8_t>(Opcode::GROUP_BY))
            error("Expected GROUP_BY in grouping set");

        uint32_t num_cols = readInt32();
        GroupingSet set;

        // Parse each column expression
        for (uint32_t col_idx = 0; col_idx < num_cols; col_idx++)
        {
            size_t expr_start = pc_;
            set.column_expr_pcs.push_back(expr_start);

            // Skip expression (reuse existing expression-skipping logic)
            skipExpression();

            // Track column index for GROUPING() function
            set.column_indices.push_back(col_idx);
        }

        grouping_sets.push_back(set);
    }

    // Continue to Step 3...
}
```

### Step 3: Execute Aggregation Per Set (4-6 hours)

**Core Loop:**

```cpp
// Parse aggregate definitions (same as standard aggregation)
// ... (reuse code from executeAggregation)

// Create combined result set
current_result_set_ = std::make_unique<ResultSet>();

// Add grouping columns
for (size_t i = 0; i < total_grouping_columns; i++)
{
    // Use column names from first (complete) grouping set
    current_result_set_->addColumn(
        grouping_column_names[i],
        core::DataType::VARCHAR);  // Will hold actual values or NULL
}

// Add aggregate columns
for (const auto& agg_def : agg_defs)
{
    current_result_set_->addColumn(
        agg_column_names[agg_def_idx],
        agg_def.result_type);
}

// Execute aggregation for each grouping set
for (size_t set_idx = 0; set_idx < grouping_sets.size(); set_idx++)
{
    const auto& set = grouping_sets[set_idx];

    // Build GROUP BY key using only columns in this set
    GroupMap group_map;

    // Scan table and accumulate aggregates
    auto table_iter = db_->getTableManager()->getTableIterator(table_info.name);

    while (table_iter->hasNext())
    {
        auto row = table_iter->next();

        // Apply WHERE clause if present
        if (has_where)
        {
            // ... (reuse WHERE evaluation logic)
            if (!where_result.toBool()) continue;
        }

        // Build group key using ONLY columns in this set
        GroupKey key;
        for (size_t expr_pc : set.column_expr_pcs)
        {
            // Evaluate grouping expression
            size_t saved_pc = pc_;
            pc_ = expr_pc;
            evaluateExpression();
            Value key_value = pop();
            key.values.push_back(key_value);
            pc_ = saved_pc;
        }

        // Get or create aggregate state for this group
        auto it = group_map.find(key);
        if (it == group_map.end())
        {
            AggregateState state;
            for (const auto& agg_def : agg_defs)
            {
                state.push_back(AggregateAccumulator(agg_def.func));
            }
            group_map[key] = state;
            it = group_map.find(key);
        }

        // Accumulate aggregate values
        for (size_t agg_idx = 0; agg_idx < agg_defs.size(); agg_idx++)
        {
            // Evaluate aggregate expression
            size_t saved_pc = pc_;
            pc_ = agg_defs[agg_idx].expr_start_pc;
            evaluateExpression();
            Value agg_value = pop();
            pc_ = saved_pc;

            it->second[agg_idx].accumulate(agg_value);
        }
    }

    // Finalize and add rows to result set
    for (const auto& [group_key, agg_state] : group_map)
    {
        std::vector<Value> result_row;

        // Add grouping column values
        // Columns in set: use actual values
        // Columns NOT in set: use NULL (they're aggregated)
        for (size_t col_idx = 0; col_idx < total_grouping_columns; col_idx++)
        {
            bool col_in_set = (col_idx < set.column_indices.size());

            if (col_in_set)
            {
                result_row.push_back(group_key.values[col_idx]);
            }
            else
            {
                // This column is aggregated in this set - emit NULL
                result_row.push_back(core::TypedValue::makeNull());
            }
        }

        // Add aggregate values
        for (size_t agg_idx = 0; agg_idx < agg_defs.size(); agg_idx++)
        {
            result_row.push_back(agg_state[agg_idx].finalize());
        }

        current_result_set_->addRow(std::move(result_row));
    }
}

// Apply HAVING if present
// ... (reuse HAVING logic)
```

### Step 4: Implement GROUPING() Function (2-3 hours)

**Location:** In expression evaluation (EXTENDED_OPCODE handler)

```cpp
case Opcode::EXTENDED_OPCODE:
{
    uint8_t ext_op = readByte();

    if (ext_op == static_cast<uint8_t>(Opcode::EXT_GROUPING_FUNC))
    {
        // GROUPING(column_expr) execution
        // Pop the column reference from stack
        Value column_ref = pop();

        // Determine if this column is in the current grouping set
        // This requires tracking current_grouping_set_index during execution
        bool is_aggregated = !isColumnInCurrentSet(column_ref);

        // Return 1 if aggregated (NULL), 0 if grouped (has value)
        push(core::TypedValue::makeInt32(is_aggregated ? 1 : 0));
    }
    // ... other extended opcodes
}
```

**Track Current Set (add to executor state):**

```cpp
// In Executor class:
size_t current_grouping_set_index_ = 0;
std::vector<size_t> current_set_column_indices_;

// Helper function:
bool Executor::isColumnInCurrentSet(const Value& column_ref)
{
    // Extract column index from column_ref
    // Check if it's in current_set_column_indices_
    for (size_t idx : current_set_column_indices_)
    {
        if (/* column matches */)
            return true;
    }
    return false;
}
```

### Step 5: Helper Functions (1-2 hours)

**Expression Skipping:**

```cpp
void Executor::skipExpression()
{
    int depth = 0;
    while (pc_ < bytecode_size_)
    {
        Opcode op = static_cast<Opcode>(readByte());

        if (op == Opcode::LITERAL_INT32)
        {
            pc_ += 4;
            depth++;
        }
        else if (op == Opcode::LITERAL_INT64)
        {
            pc_ += 8;
            depth++;
        }
        else if (op == Opcode::LITERAL_DOUBLE)
        {
            pc_ += 8;
            depth++;
        }
        else if (op == Opcode::LITERAL_STRING || op == Opcode::COLUMN_REF)
        {
            uint32_t len = readInt32();
            pc_ += len;
            depth++;
        }
        else if (op == Opcode::LITERAL_NULL)
        {
            depth++;
        }
        else if (op >= Opcode::EXPR_ADD && op <= Opcode::EXPR_OR)
        {
            depth--;
        }

        if (depth == 1)
            break;
    }
}
```

---

## Testing Strategy

### Unit Tests

1. **ROLLUP(single_column):**
   ```sql
   SELECT region, SUM(sales) FROM sales GROUP BY ROLLUP(region);
   ```
   Expected: Rows for each region + grand total row

2. **ROLLUP(multiple_columns):**
   ```sql
   SELECT region, product, SUM(sales)
   FROM sales
   GROUP BY ROLLUP(region, product);
   ```
   Expected: (region,product), (region,NULL), (NULL,NULL)

3. **CUBE(two_columns):**
   ```sql
   SELECT region, product, SUM(sales)
   FROM sales
   GROUP BY CUBE(region, product);
   ```
   Expected: 4 grouping levels (all 2^2 combinations)

4. **GROUPING() Function:**
   ```sql
   SELECT
     region,
     product,
     GROUPING(region) as r_grp,
     GROUPING(product) as p_grp,
     SUM(sales)
   FROM sales
   GROUP BY ROLLUP(region, product);
   ```
   Expected: GROUPING returns 0 for grouped, 1 for aggregated

5. **GROUPING SETS:**
   ```sql
   SELECT region, product, SUM(sales)
   FROM sales
   GROUP BY GROUPING SETS ((region, product), (region), ());
   ```
   Expected: Only specified grouping sets

### Integration Tests

- ROLLUP with HAVING clause
- CUBE with WHERE clause
- GROUPING SETS with ORDER BY
- Nested expressions in grouping columns
- Multiple aggregates with ROLLUP
- GROUPING() in HAVING clause
- GROUPING() in ORDER BY

---

## Performance Considerations

1. **Memory Usage:**
   - Each grouping set maintains its own hash table
   - For CUBE(n), memory = O(2^n × rows × aggregate_size)
   - Limit CUBE to ≤5 columns in production

2. **Execution Time:**
   - Time = O(num_sets × table_scan × hash_lookup)
   - ROLLUP is O(n) sets - very efficient
   - CUBE is O(2^n) sets - use sparingly

3. **Optimizations (future):**
   - Reuse partial aggregates across sets
   - Sort-based grouping for large datasets
   - Parallel set processing

---

## Common Pitfalls

1. **NULL Handling:**
   - Aggregated columns emit NULL (not empty string)
   - GROUPING() must distinguish NULL from missing value

2. **Column Order:**
   - Result columns: grouping columns first, then aggregates
   - ROLLUP respects column order (a,b,c) ≠ (c,b,a)

3. **GROUPING() Context:**
   - Must track current set during execution
   - Column index mapping must be correct

4. **Empty Sets:**
   - ROLLUP includes empty set () for grand total
   - Empty set aggregates ALL rows (no grouping)

---

## Debugging Tips

1. **Enable Debug Logging:**
   ```cpp
   DEBUG_LOG("Processing grouping set " + std::to_string(set_idx) +
             " with " + std::to_string(set.column_expr_pcs.size()) + " columns");
   ```

2. **Validate Bytecode:**
   - Print opcode sequence during parsing
   - Verify num_grouping_sets matches expected

3. **Check Group Keys:**
   - Log group key values during accumulation
   - Verify NULL vs actual value placement

4. **Test Incrementally:**
   - Start with ROLLUP(single_column)
   - Add second column
   - Then try CUBE
   - Finally GROUPING SETS

---

## Estimated Time Breakdown

| Task | Hours | Notes |
|------|-------|-------|
| Step 1: Detection | 0.5 | Simple opcode check |
| Step 2: Parsing | 1-2 | Parse grouping sets |
| Step 3: Execution Loop | 8-12 | Core aggregation logic |
| Step 4: GROUPING() | 2-3 | Track current set context |
| Step 5: Helpers | 1-2 | Expression skipping, etc. |
| Testing | 4-6 | Unit + integration tests |
| Debugging | 2-4 | Fix edge cases |
| **Total** | **18-29** | **Average: 23 hours** |

---

## Success Criteria

- ✅ ROLLUP generates correct number of rows (n+1 sets)
- ✅ CUBE generates 2^n rows for each unique combination
- ✅ GROUPING SETS generates only specified sets
- ✅ Aggregated columns show NULL
- ✅ GROUPING() returns correct 0/1 values
- ✅ HAVING clause works correctly
- ✅ ORDER BY works with GROUPING()
- ✅ All unit tests pass
- ✅ Integration tests pass
- ✅ No memory leaks (valgrind clean)
- ✅ Performance acceptable for reasonable inputs

---

## References

- SQL Standard: ISO/IEC 9075-2:2016 Section 7.10 (ROLLUP/CUBE/GROUPING SETS)
- PostgreSQL Implementation: `src/backend/executor/nodeAgg.c`
- Bytecode Format: `src/sblr/bytecode_generator.cpp` lines 4684-4810
- Parser: `src/parser/parser.cpp` lines 6122-6254
- Optimizer: `src/optimizer/query_planner.cpp` lines 1684-1706

---

**Status:** Ready for implementation
**Next Session:** Implement executor layer
**Blocked By:** None - all dependencies complete!
