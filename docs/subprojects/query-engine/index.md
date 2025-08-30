
### SeqScan

- Inputs: relation `schema.table`, optional `predicate`, optional `projections`
- Outputs: rows as `Tuple` vector; columns per table or projection
- Invariants:
  - Emits each visible row once; preserves heap order
  - `open()` initializes column metadata; `next()` returns one row per call
- Memory behavior: streaming; buffers current row; mock data path for tests
- Spill policy: none
- Construction:
  - planner: `QueryPlanner::build_scan_node`
```241:247:/workspace/src/engine/query_planner.cpp
std::string schema = "public";
return std::make_unique<SeqScanNode>(
    schema, from_item.table, from_item.alias.empty() ? from_item.table : from_item.alias);
```
- Implementation: `next()`
```148:156:/workspace/src/engine/executor_nodes.cpp
bool SeqScanNode::next(Tuple& out)
{
    if (!opened_ || current_row_ >= rows_.size()) {
        return false;
    }

    out = rows_[current_row_++];
    instr_.output_rows++;
    return true;
}
```

### Filter

- Inputs: child iterator, boolean `predicate`
- Outputs: subset of child rows
- Invariants:
  - Preserves child order
  - Increments `filtered_rows` for rows failing predicate
- Memory behavior: streaming; uses compiled predicate tokens when available
- Spill policy: none
- Construction:
```249:254:/workspace/src/engine/query_planner.cpp
return std::make_unique<FilterNode>(std::move(child), predicate);
```
- Implementation: `next()`
```528:565:/workspace/src/engine/executor_nodes.cpp
bool FilterNode::next(Tuple& out)
{
    if (!opened_) {
        return false;
    }

    Tuple child_tuple;
    while (child_->next(child_tuple)) {
        instr_.input_rows++;
        // Apply filter predicate
        if (!predicate_.empty()) {
            // Build column index mapping
            std::unordered_map<std::string, std::size_t> col_index;
            auto child_cols = child_->columns();
            for (std::size_t i = 0; i < child_cols.size(); ++i) {
                col_index[child_cols[i]] = i;
            }
            bool matches;
            if (!compiled_predicate_.empty()) {
                matches =
                    evaluate_predicate_compiled(compiled_predicate_, col_index, child_tuple);
            } else {
                matches = evaluate_predicate(predicate_, col_index, child_tuple);
            }
            if (!matches) {
                instr_.filtered_rows++;
                continue;
            }
        }
        out = child_tuple;
        instr_.output_rows++;
        return true;
    }
    return false;
}
```
- Helpers:
```13:24:/workspace/include/scratchbird/engine/expr.h
bool evaluate_predicate(const std::string& expr,
                        const std::unordered_map<std::string, std::size_t>& col_index,
                        const std::vector<Value>& row);
std::vector<std::string> compile_predicate(const std::string& expr);
bool evaluate_predicate_compiled(const std::vector<std::string>& postfix,
                                 const std::unordered_map<std::string, std::size_t>& col_index,
                                 const std::vector<Value>& row);
```

### Project

- Inputs: child iterator, list of `projections`
- Outputs: reshaped rows per projection list
- Invariants: preserves child order; supports `*`, column names, ordinals, `AS` labels
- Memory behavior: streaming; converts projected strings back to `Value`
- Spill policy: none
- Construction:
```266:271:/workspace/src/engine/query_planner.cpp
return std::make_unique<ProjectNode>(std::move(child), projections);
```
- Implementation: `next()`
```608:647:/workspace/src/engine/executor_nodes.cpp
bool ProjectNode::next(Tuple& out)
{
    if (!opened_) {
        return false;
    }

    Tuple child_tuple;
    if (!child_->next(child_tuple)) {
        return false;
    }

    instr_.input_rows++;
    auto child_cols = child_->columns();
    std::unordered_map<std::string, std::size_t> col_index;
    for (std::size_t i = 0; i < child_cols.size(); ++i) {
        col_index[child_cols[i]] = i;
    }
    if (projections_.empty() || (projections_.size() == 1 && projections_[0] == "*")) {
        out = child_tuple;
    } else {
        auto projected_strings = project_row(projections_, child_cols, col_index, child_tuple);
        out.clear();
        for (const auto& str : projected_strings) {
            Value val;
            val.bytes = str;
            val.is_null = str.empty();
            out.push_back(val);
        }
    }
    instr_.output_rows++;
    return true;
}
```
- Helpers:
```441:509:/workspace/src/engine/expr.cpp
std::vector<std::string>
project_row(const std::vector<std::string>& projections,
            const std::vector<std::string>& colnames,
            const std::unordered_map<std::string, std::size_t>& col_index,
            const std::vector<Value>& row)
```

### Sort

- Inputs: child iterator, list of sort keys `(column, asc, nulls_first)`
- Outputs: all child rows in sorted order
- Invariants: materializes full input, stable ordering over specified keys
- Memory behavior: in-memory vector materialization and sort
- Spill policy: not yet implemented (planned per Phase 5)
- Implementation: `open()` materializes and sorts; `next()` iterates
```670:707:/workspace/src/engine/executor_nodes.cpp
void SortNode::open(ExecutorContext& ctx)
{
    child_->open(ctx);
    columns_ = child_->columns();
    sorted_tuples_.clear();
    Tuple tuple;
    while (child_->next(tuple)) {
        instr_.input_rows++;
        sorted_tuples_.push_back(tuple);
    }
    std::sort(
        sorted_tuples_.begin(), sorted_tuples_.end(),
        [this](const Tuple& left, const Tuple& right) { return compare_tuples(left, right); });
    current_index_ = 0;
    materialized_ = true;
    opened_ = true;
}

bool SortNode::next(Tuple& out)
{
    if (!opened_ || !materialized_ || current_index_ >= sorted_tuples_.size()) {
        return false;
    }
    out = sorted_tuples_[current_index_++];
    instr_.output_rows++;
    return true;
}
```

### Aggregation (HashAgg)

- Inputs: child iterator, `group_by` columns, aggregate functions
- Outputs: one row per group with aggregate results
- Invariants: materializes groups; COUNT/SUM/AVG/MIN/MAX supported; COUNT(*) supported
- Memory behavior: in-memory hash table per group; planned spill per Phase 5
- Spill policy: not yet implemented (planned: grace hash partition + merge)
- Implementation: `open()` builds groups; `next()` iterates groups
```797:912:/workspace/src/engine/executor_nodes.cpp
void AggregationNode::open(ExecutorContext& ctx)
{
    child_->open(ctx);
    // build output headers and group states, then consume child
    groups_.clear();
    Tuple tuple;
    while (child_->next(tuple)) {
        instr_.input_rows++;
        std::string group_key = build_group_key(tuple);
        GroupState& state = groups_[group_key];
        auto agg_indices = get_aggregate_column_indices();
        for (std::size_t i = 0; i < aggregates_.size(); ++i) {
            const auto& agg = aggregates_[i];
            if (agg.type == AggregateFunction::CountStar) {
                state.count++;
            } else if (i < agg_indices.size()) {
                std::size_t col_idx = agg_indices[i];
                if (col_idx < tuple.size()) {
                    update_group_state(state, agg, tuple[col_idx]);
                }
            }
        }
    }
    current_group_ = groups_.begin();
    materialized_ = true;
    opened_ = true;
}

bool AggregationNode::next(Tuple& out)
{
    if (!opened_ || !materialized_ || current_group_ == groups_.end()) {
        return false;
    }
    out.clear();
    // decode key parts and append aggregate results, then advance iterator
    const GroupState& state = current_group_->second;
    for (const auto& agg : aggregates_) {
        Value result = compute_aggregate_result(state, agg);
        out.push_back(result);
    }
    ++current_group_;
    instr_.output_rows++;
    return true;
}
```

### Nested Loop Join (NLJ)

- Inputs: left child, right child, join predicate, join type (inner, left outer)
- Outputs: joined rows per predicate
- Invariants: naive NLJ; right child not rewound per left tuple yet (placeholder)
- Memory behavior: streaming; no hash/materialization yet
- Spill policy: none
- Construction:
```256:263:/workspace/src/engine/query_planner.cpp
return std::make_unique<NestedLoopJoinNode>(std::move(left), std::move(right),
                                            join_condition);
```
- Implementation: `next()`
```403:455:/workspace/src/engine/executor_nodes.cpp
bool NestedLoopJoinNode::next(Tuple& out)
{
    if (!opened_) {
        return false;
    }
    while (true) {
        if (!has_current_left_) {
            if (!left_child_->next(current_left_tuple_)) {
                return false;
            }
            has_current_left_ = true;
            left_matched_ = false;
            instr_.input_rows++;
        }
        Tuple right_tuple;
        if (right_child_->next(right_tuple)) {
            instr_.input_rows++;
            if (evaluate_join_predicate(current_left_tuple_, right_tuple)) {
                left_matched_ = true;
                out = current_left_tuple_;
                out.insert(out.end(), right_tuple.begin(), right_tuple.end());
                instr_.output_rows++;
                return true;
            }
        } else {
            if (join_type_ == LeftOuter && !left_matched_) {
                out = current_left_tuple_;
                for (std::size_t i = 0; i < right_child_->columns().size(); ++i) {
                    out.emplace_back();
                }
                instr_.output_rows++;
                has_current_left_ = false;
                return true;
            }
            has_current_left_ = false;
        }
    }
}
```

### Hash Join

- Inputs: left child, right child, join keys, join type, optional extra predicate
- Outputs: joined rows; builds hash on right, probes with left
- Invariants: equi-join keys; additional predicate evaluated per match
- Memory behavior: builds in-memory hash table of right side
- Spill policy: not yet implemented (planned: partitioned hash join)
- Implementation: `open()` builds hash; `next()` probes
```185:246:/workspace/src/engine/executor_nodes.cpp
void HashJoinNode::open(ExecutorContext& ctx)
{
    // Build combined columns; open children
    // Build phase: scan right and insert into hash_table_
}

bool HashJoinNode::next(Tuple& out)
{
    if (!opened_) {
        return false;
    }
    // return pending matches, else pull next left and probe hash_table_
}
```

### Index Scan (planned)

- Status: Planned in Phase 5; not implemented yet
- Behavior: point/range scans on B-Tree V1; residual predicate; index-only when covered
- Spec Trace:
```132:135:/workspace/ProjectPlan/Phase 5 — SQL executor (scan to results): detailed implementation plan.md
Integrate with B-Tree V1 traversal for ordered iteration; optional backward scan.
Apply residual predicates and projection. Use index-only fast path when covered.
```

### Limit (planned)

- Status: Planned in Phase 5; not implemented yet
- Behavior: LIMIT/OFFSET over child
- Spec Trace:
```145:148:/workspace/ProjectPlan/Phase 5 — SQL executor (scan to results): detailed implementation plan.md
LimitNode
- Implement straightforward LIMIT/OFFSET over child.
```

### Window (subset, planned)

- Status: Planned in Phase 5/17; not implemented yet
- Behavior: ROW_NUMBER, RANK, DENSE_RANK; SUM/AVG with default frame over PARTITION BY/ORDER BY
- Spec Trace:
```160:162:/workspace/ProjectPlan/Phase 5 — SQL executor (scan to results): detailed implementation plan.md
WindowNode (subset)
-- Implement partition buffering and per-partition computation for ROW_NUMBER, RANK, DENSE_RANK; SUM/AVG with default frame.
```

### Optimizer cost cross-links

- SeqScan cost
```287:291:/workspace/src/engine/query_planner.cpp
double QueryPlanner::estimate_seq_scan_cost(const std::string& /* table */)
{
    return 1000.0;
}
```
- Filter cost
```299:303:/workspace/src/engine/query_planner.cpp
double QueryPlanner::estimate_filter_cost(double input_rows, double /* selectivity */)
{
    return input_rows * 0.1;
}
```
- Hash Join cost
```293:297:/workspace/src/engine/query_planner.cpp
double QueryPlanner::estimate_hash_join_cost(double left_rows, double right_rows)
{
    return right_rows * 1.5 + left_rows * 1.0;
}
```