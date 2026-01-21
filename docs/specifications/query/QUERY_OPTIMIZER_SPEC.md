# ScratchBird Query Optimizer Specification
## Comprehensive Technical Specification for Query Optimization

## Overview

ScratchBird's query optimizer combines PostgreSQL's sophisticated planning algorithms, SQL Server's adaptive query processing, MySQL's query result caching, and Firebird's simplicity. The optimizer works with BLR (Binary Language Representation) for efficient plan caching and reuse. This specification aligns with Phase 13 (Query Optimization) of the implementation plan.

## 1. Statistics System

### 1.1 Table and Column Statistics

```c
// Comprehensive statistics structure
typedef struct sb_statistics {
    // Object identification
    UUID            stat_object_uuid;   // Table or index UUID
    ObjectType      stat_object_type;   // TABLE or INDEX
    
    // Basic statistics
    uint64_t        n_tuples;          // Number of tuples
    uint64_t        n_dead_tuples;     // Dead tuples (for sweep/GC)
    uint64_t        n_pages;           // Number of pages
    uint64_t        n_all_visible_pages; // Pages with all visible tuples
    
    // Column statistics
    uint16_t        n_columns;         // Number of columns
    ColumnStats**   column_stats;      // Per-column statistics
    
    // Multi-column statistics (PostgreSQL 10+ style)
    uint16_t        n_multi_stats;     // Number of multi-column stats
    MultiColumnStats** multi_stats;    // Multi-column statistics
    
    // Modification tracking
    uint64_t        n_mod_since_analyze; // Modifications since ANALYZE
    time_t          last_analyze_time; // Last ANALYZE time
    time_t          last_autoanalyze_time; // Last auto-analyze
    
    // Adaptive statistics (SQL Server style)
    bool            stats_are_stale;   // Need refresh
    double          stats_staleness_ratio; // How stale (0.0-1.0)
    
    // Query feedback statistics
    QueryFeedback*  feedback_stats;    // Runtime feedback
} SBStatistics;

// Per-column statistics
typedef struct column_stats {
    // Column identification
    uint16_t        col_attnum;        // Column number
    UUID            col_uuid;          // Column UUID
    
    // Basic statistics
    double          null_fraction;     // Fraction of NULLs
    uint64_t        n_distinct;        // Number of distinct values
    double          avg_width;         // Average width in bytes
    
    // Data distribution
    HistogramType   histogram_type;    // EQUAL_HEIGHT or EQUAL_WIDTH
    uint16_t        n_histogram_buckets; // Number of buckets
    Datum*          histogram_bounds;  // Histogram boundaries
    float4*         histogram_freqs;   // Frequencies per bucket
    
    // Most common values (MCV)
    uint16_t        n_mcv;            // Number of MCVs
    Datum*          mcv_values;       // Most common values
    float4*         mcv_freqs;        // MCV frequencies
    
    // Correlation with physical order
    double          correlation;       // -1.0 to 1.0
    
    // Extended statistics
    double          n_distinct_inherited; // Including child tables
    bool            stats_valid;       // Statistics are valid
    
    // Data type specific
    void*           type_specific_stats; // Type-specific statistics
} ColumnStats;

// Multi-column statistics
typedef struct multi_column_stats {
    // Columns involved
    uint16_t        n_columns;         // Number of columns
    uint16_t*       column_numbers;    // Column numbers
    
    // Dependency statistics
    double          functional_dependency; // Functional dependency degree
    MVDependencies* mv_dependencies;   // Multivariate dependencies
    
    // N-dimensional histogram
    NDHistogram*    nd_histogram;      // N-dimensional histogram
    
    // Distinct count statistics
    uint64_t        n_distinct_combined; // Combined distinct count
    
    // Correlation matrix
    double**        correlation_matrix; // Correlation between columns
} MultiColumnStats;
```

### 1.2 Histogram Implementation

```c
// N-dimensional histogram for multi-column statistics
typedef struct nd_histogram {
    uint16_t        n_dimensions;      // Number of dimensions
    uint16_t*       dimension_sizes;   // Size per dimension
    
    // Bucket structure
    uint32_t        n_buckets;         // Total buckets
    NDBucket*       buckets;           // Bucket array
    
    // Bucket index for fast lookup
    void*           bucket_index;      // R-tree or grid index
} NDHistogram;

// N-dimensional bucket
typedef struct nd_bucket {
    // Bucket boundaries (per dimension)
    Datum*          lower_bounds;      // Lower bounds
    Datum*          upper_bounds;      // Upper bounds
    
    // Statistics
    uint64_t        tuple_count;       // Tuples in bucket
    double          selectivity;       // Bucket selectivity
    
    // Split information
    bool            can_split;         // Can be split further
    uint16_t        split_dimension;   // Best dimension to split
} NDBucket;

// Build histogram from sample
NDHistogram* build_nd_histogram(
    TupleSample* sample,
    uint16_t* columns,
    uint16_t n_columns,
    uint32_t target_buckets)
{
    NDHistogram* hist = allocate(sizeof(NDHistogram));
    hist->n_dimensions = n_columns;
    hist->dimension_sizes = allocate(sizeof(uint16_t) * n_columns);
    
    // Start with single bucket covering all data
    hist->n_buckets = 1;
    hist->buckets = allocate(sizeof(NDBucket) * target_buckets);
    
    // Initialize first bucket
    NDBucket* root = &hist->buckets[0];
    calculate_bounds(sample, columns, n_columns, 
                    root->lower_bounds, root->upper_bounds);
    root->tuple_count = sample->n_tuples;
    root->selectivity = 1.0;
    
    // Iteratively split buckets
    while (hist->n_buckets < target_buckets) {
        // Find best bucket to split
        NDBucket* to_split = find_best_split_bucket(hist);
        
        if (to_split == NULL || !to_split->can_split) {
            break;  // No more splits possible
        }
        
        // Split bucket
        split_bucket(hist, to_split, sample);
    }
    
    // Build index for fast bucket lookup
    hist->bucket_index = build_bucket_index(hist);
    
    return hist;
}
```

### 1.3 Statistics Collection (ANALYZE)

```c
// Analyze table to collect statistics
Status analyze_table(
    UUID table_uuid,
    AnalyzeOptions* options)
{
    Table* table = get_table(table_uuid);
    SBStatistics* stats = allocate(sizeof(SBStatistics));
    
    stats->stat_object_uuid = table_uuid;
    stats->stat_object_type = OBJECT_TABLE;
    
    // Phase 1: Collect basic statistics
    collect_basic_stats(table, stats);
    
    // Phase 2: Sample rows for detailed statistics
    TupleSample* sample = sample_table(table, options->sample_size);
    
    // Phase 3: Compute per-column statistics
    stats->n_columns = table->n_columns;
    stats->column_stats = allocate(sizeof(ColumnStats*) * stats->n_columns);
    
    for (uint16_t col = 0; col < stats->n_columns; col++) {
        stats->column_stats[col] = analyze_column(sample, col, options);
    }
    
    // Phase 4: Compute multi-column statistics if requested
    if (options->compute_multi_column_stats) {
        compute_multi_column_stats(sample, stats, options);
    }
    
    // Phase 5: Store statistics in catalog
    store_statistics(stats);
    
    // Phase 6: Invalidate cached plans that use this table
    invalidate_cached_plans(table_uuid);
    
    stats->last_analyze_time = time(NULL);
    stats->n_mod_since_analyze = 0;
    
    return STATUS_OK;
}

// Sample table for statistics
TupleSample* sample_table(
    Table* table,
    uint32_t target_rows)
{
    TupleSample* sample = allocate(sizeof(TupleSample));
    
    // Use Vitter's Algorithm S for reservoir sampling
    uint64_t rows_seen = 0;
    uint64_t rows_to_skip = 0;
    
    TableScan* scan = begin_table_scan(table);
    Tuple* tuple;
    
    // Fill initial reservoir
    while (rows_seen < target_rows && 
           (tuple = get_next_tuple(scan)) != NULL) {
        add_to_sample(sample, tuple);
        rows_seen++;
    }
    
    // Continue with probabilistic sampling
    while ((tuple = get_next_tuple(scan)) != NULL) {
        rows_seen++;
        
        // Decide whether to include this row
        uint64_t random_pos = random() % rows_seen;
        
        if (random_pos < target_rows) {
            // Replace random element in reservoir
            replace_sample_element(sample, random_pos, tuple);
        }
    }
    
    sample->total_rows = rows_seen;
    sample->sample_rows = MIN(target_rows, rows_seen);
    
    return sample;
}
```

## 2. Cost Model

### 2.1 Cost Configuration

```c
// Configurable cost parameters
typedef struct sb_cost_config {
    // I/O costs
    double          seq_page_cost;     // Sequential page read (1.0)
    double          random_page_cost;  // Random page read (4.0 HDD, 1.1 SSD)
    
    // CPU costs
    double          cpu_tuple_cost;    // Process one tuple (0.01)
    double          cpu_index_tuple_cost; // Process index tuple (0.005)
    double          cpu_operator_cost; // Execute operator (0.0025)
    
    // Parallel execution costs
    double          parallel_setup_cost; // Setup parallel workers (1000.0)
    double          parallel_tuple_cost; // Per-tuple parallel overhead (0.1)
    
    // Network costs (for distributed)
    double          network_latency_ms; // Network latency (0.5)
    double          network_bandwidth_mbps; // Bandwidth (1000.0)
    double          network_tuple_cost; // Send tuple over network (0.02)
    
    // Memory costs
    double          memory_operator_cost; // In-memory operation (0.001)
    double          hash_build_cost;   // Build hash table entry (0.005)
    double          sort_comparison_cost; // Sort comparison (0.002)
    
    // Cache effects
    double          cache_hit_factor;  // Discount for cached pages (0.5)
    double          effective_cache_size_gb; // Effective cache size
} SBCostConfig;

// Global cost configuration
static SBCostConfig cost_config = {
    .seq_page_cost = 1.0,
    .random_page_cost = 4.0,
    .cpu_tuple_cost = 0.01,
    .cpu_index_tuple_cost = 0.005,
    .cpu_operator_cost = 0.0025,
    .parallel_setup_cost = 1000.0,
    .parallel_tuple_cost = 0.1,
    .network_latency_ms = 0.5,
    .network_bandwidth_mbps = 1000.0,
    .network_tuple_cost = 0.02,
    .memory_operator_cost = 0.001,
    .hash_build_cost = 0.005,
    .sort_comparison_cost = 0.002,
    .cache_hit_factor = 0.5,
    .effective_cache_size_gb = 4.0
};
```

### 2.2 Cost Estimation Functions

```c
// Plan node cost structure
typedef struct plan_cost {
    double          startup_cost;      // Cost before first tuple
    double          total_cost;        // Total cost
    double          run_cost;          // total_cost - startup_cost
    
    // Estimates
    double          plan_rows;         // Estimated rows
    double          plan_width;        // Average row width
    
    // Parallel execution
    uint16_t        parallel_workers;  // Number of parallel workers
    bool            parallel_aware;    // Node is parallel-aware
    
    // Memory usage
    uint64_t        memory_usage;      // Estimated memory usage
} PlanCost;

// Estimate sequential scan cost
PlanCost* cost_seqscan(
    Table* table,
    List* quals,
    SBStatistics* stats)
{
    PlanCost* cost = allocate(sizeof(PlanCost));
    
    // Calculate selectivity
    double selectivity = 1.0;
    for (Qual* qual : quals) {
        selectivity *= estimate_qual_selectivity(qual, stats);
    }
    
    // I/O cost
    double pages = stats->n_pages;
    double io_cost = pages * cost_config.seq_page_cost;
    
    // Adjust for cache effects
    double cache_pages = MIN(pages, 
        cost_config.effective_cache_size_gb * 1024 * 128); // 128 pages/MB
    double cache_discount = cache_pages * 
        (cost_config.seq_page_cost * (1 - cost_config.cache_hit_factor));
    io_cost -= cache_discount;
    
    // CPU cost
    double cpu_cost = stats->n_tuples * cost_config.cpu_tuple_cost +
                     stats->n_tuples * selectivity * 
                     cost_config.cpu_operator_cost;
    
    // Fill cost structure
    cost->startup_cost = 0;
    cost->total_cost = io_cost + cpu_cost;
    cost->run_cost = cost->total_cost;
    cost->plan_rows = stats->n_tuples * selectivity;
    cost->plan_width = estimate_row_width(table);
    
    return cost;
}

// Estimate index scan cost
PlanCost* cost_indexscan(
    Index* index,
    List* quals,
    SBStatistics* table_stats,
    IndexStatistics* index_stats)
{
    PlanCost* cost = allocate(sizeof(PlanCost));
    
    // Calculate selectivity
    double selectivity = estimate_index_selectivity(index, quals, 
                                                   table_stats, index_stats);
    
    // Index pages to read
    double index_pages = ceil(index_stats->stat_leaf_pages * selectivity);
    double index_io_cost = index_pages * cost_config.seq_page_cost;
    
    // Table pages to read (considering correlation)
    double table_pages = ceil(table_stats->n_pages * selectivity * 
                            (1.0 - index_stats->stat_correlation * 0.5));
    double table_io_cost = table_pages * cost_config.random_page_cost;
    
    // CPU cost
    double index_cpu_cost = index_stats->stat_num_entries * selectivity * 
                          cost_config.cpu_index_tuple_cost;
    double table_cpu_cost = table_stats->n_tuples * selectivity * 
                          cost_config.cpu_tuple_cost;
    
    // Fill cost structure
    cost->startup_cost = index_pages * cost_config.seq_page_cost * 0.1;
    cost->total_cost = index_io_cost + table_io_cost + 
                      index_cpu_cost + table_cpu_cost;
    cost->run_cost = cost->total_cost - cost->startup_cost;
    cost->plan_rows = table_stats->n_tuples * selectivity;
    cost->plan_width = estimate_row_width(index->table);
    
    return cost;
}
```

## 3. Query Planning

### 3.1 Query Plan Node Structure

```c
// Query plan node
typedef struct plan_node {
    NodeType        type;              // Node type
    
    // Cost and estimates
    PlanCost        cost;              // Cost estimates
    
    // Target list
    List*           targetlist;        // Projection list
    List*           quals;             // Filter conditions
    
    // Child nodes
    struct plan_node* left_child;      // Left child
    struct plan_node* right_child;     // Right child
    
    // Parallel execution
    bool            parallel_safe;     // Can run in parallel
    uint16_t        parallel_workers;  // Number of workers
    
    // Execution hints
    List*           hints;             // Optimizer hints
    
    // Node-specific data
    void*           node_data;         // Type-specific data
} PlanNode;

// Node types
typedef enum node_type {
    T_SeqScan,
    T_IndexScan,
    T_IndexOnlyScan,
    T_BitmapIndexScan,
    T_BitmapHeapScan,
    T_TidScan,
    T_NestLoop,
    T_HashJoin,
    T_MergeJoin,
    T_Hash,
    T_Sort,
    T_Aggregate,
    T_WindowAgg,
    T_Material,
    T_Unique,
    T_Limit,
    T_Append,
    T_MergeAppend,
    T_Result,
    T_ProjectSet,
    T_ModifyTable,
    T_LockRows,
    T_ValuesScan,
    T_CteScan,
    T_ForeignScan,
    T_CustomScan
} NodeType;
```

### 3.2 Query Planner

```c
// Main query planner
typedef struct query_planner {
    // Planning context
    PlannerContext* context;           // Planner context
    
    // Statistics cache
    StatisticsCache* stats_cache;      // Cached statistics
    
    // Cost configuration
    SBCostConfig*   cost_config;       // Cost parameters
    
    // Plan cache
    PlanCache*      plan_cache;        // Cached plans
    
    // Optimizer settings
    OptimizerSettings settings;        // Optimizer settings
} QueryPlanner;

// Generate query plan
PlanNode* generate_plan(
    QueryPlanner* planner,
    ParsedQuery* query)
{
    // Step 1: Pull up subqueries
    query = pull_up_subqueries(query);
    
    // Step 2: Flatten joins
    query = flatten_joins(query);
    
    // Step 3: Reduce outer joins
    query = reduce_outer_joins(query);
    
    // Step 4: Generate possible paths
    RelOptInfo* rel = make_rel_from_query(query);
    generate_base_paths(planner, rel);
    
    // Step 5: Join planning
    if (has_joins(query)) {
        rel = plan_joins(planner, rel, query->joins);
    }
    
    // Step 6: Upper planning (GROUP BY, ORDER BY, etc.)
    if (has_aggregates(query)) {
        rel = plan_aggregates(planner, rel, query);
    }
    
    if (has_window_functions(query)) {
        rel = plan_window_functions(planner, rel, query);
    }
    
    if (query->order_by != NULL) {
        rel = plan_sort(planner, rel, query->order_by);
    }
    
    if (query->limit != NULL) {
        rel = plan_limit(planner, rel, query->limit);
    }
    
    // Step 7: Convert best path to plan
    PlanNode* plan = create_plan_from_path(rel->cheapest_path);
    
    // Step 8: Apply final optimizations
    plan = optimize_plan(plan);
    
    return plan;
}
```

### 3.3 Join Ordering

```c
// Join ordering using dynamic programming
RelOptInfo* plan_joins(
    QueryPlanner* planner,
    RelOptInfo* base_rels,
    List* joins)
{
    uint32_t n_rels = list_length(base_rels);
    
    if (n_rels <= JOIN_SEARCH_THRESHOLD) {
        // Use dynamic programming for small joins
        return dynamic_programming_join_search(planner, base_rels, joins);
    } else {
        // Use genetic algorithm for large joins
        return genetic_algorithm_join_search(planner, base_rels, joins);
    }
}

// Dynamic programming join search
RelOptInfo* dynamic_programming_join_search(
    QueryPlanner* planner,
    List* base_rels,
    List* joins)
{
    uint32_t n_rels = list_length(base_rels);
    
    // Level 1: Base relations
    List** join_rel_level = allocate(sizeof(List*) * (n_rels + 1));
    join_rel_level[1] = base_rels;
    
    // Build joins level by level
    for (uint32_t level = 2; level <= n_rels; level++) {
        join_rel_level[level] = NIL;
        
        // Consider all ways to build level-k joins
        for (uint32_t k = 1; k < level; k++) {
            List* left_rels = join_rel_level[k];
            List* right_rels = join_rel_level[level - k];
            
            // Try joining each pair
            for (RelOptInfo* left : left_rels) {
                for (RelOptInfo* right : right_rels) {
                    if (can_join(left, right, joins)) {
                        RelOptInfo* join_rel = make_join_rel(planner, 
                                                            left, right);
                        
                        // Generate join paths
                        generate_join_paths(planner, join_rel, left, right);
                        
                        // Add to current level
                        join_rel_level[level] = 
                            lappend(join_rel_level[level], join_rel);
                    }
                }
            }
        }
    }
    
    // Return cheapest complete join
    return find_cheapest_rel(join_rel_level[n_rels]);
}

// Generate join paths for two relations
void generate_join_paths(
    QueryPlanner* planner,
    RelOptInfo* joinrel,
    RelOptInfo* outer_rel,
    RelOptInfo* inner_rel)
{
    // Nested loop join
    Path* nl_path = create_nestloop_path(planner, joinrel, 
                                        outer_rel->cheapest_path,
                                        inner_rel->cheapest_path);
    add_path(joinrel, nl_path);
    
    // Hash join (if applicable)
    if (can_hash_join(joinrel)) {
        Path* hash_path = create_hashjoin_path(planner, joinrel,
                                              outer_rel->cheapest_path,
                                              inner_rel->cheapest_path);
        add_path(joinrel, hash_path);
    }
    
    // Merge join (if applicable)
    if (can_merge_join(joinrel)) {
        // May need to sort
        Path* outer_sorted = ensure_sorted_path(outer_rel->cheapest_path,
                                               joinrel->join_clauses);
        Path* inner_sorted = ensure_sorted_path(inner_rel->cheapest_path,
                                               joinrel->join_clauses);
        
        Path* merge_path = create_mergejoin_path(planner, joinrel,
                                                outer_sorted,
                                                inner_sorted);
        add_path(joinrel, merge_path);
    }
}
```

## 4. Selectivity Estimation

### 4.1 Selectivity Functions

```c
// Estimate selectivity of a qualification
double estimate_qual_selectivity(
    Qual* qual,
    SBStatistics* stats)
{
    switch (qual->type) {
        case QUAL_EQUALS:
            return estimate_equality_selectivity(qual, stats);
            
        case QUAL_RANGE:
            return estimate_range_selectivity(qual, stats);
            
        case QUAL_LIKE:
            return estimate_like_selectivity(qual, stats);
            
        case QUAL_IN:
            return estimate_in_selectivity(qual, stats);
            
        case QUAL_AND:
            return estimate_and_selectivity(qual->children, stats);
            
        case QUAL_OR:
            return estimate_or_selectivity(qual->children, stats);
            
        case QUAL_NOT:
            return 1.0 - estimate_qual_selectivity(qual->child, stats);
            
        default:
            return DEFAULT_SELECTIVITY;
    }
}

// Estimate equality selectivity
double estimate_equality_selectivity(
    Qual* qual,
    SBStatistics* stats)
{
    ColumnStats* colstats = get_column_stats(stats, qual->column);
    
    if (colstats == NULL) {
        return DEFAULT_EQUALITY_SELECTIVITY;
    }
    
    // Check if value is in MCV list
    for (uint16_t i = 0; i < colstats->n_mcv; i++) {
        if (datum_equals(qual->value, colstats->mcv_values[i])) {
            return colstats->mcv_freqs[i];
        }
    }
    
    // Not in MCV - use uniform distribution over other values
    double mcv_total_freq = 0;
    for (uint16_t i = 0; i < colstats->n_mcv; i++) {
        mcv_total_freq += colstats->mcv_freqs[i];
    }
    
    double other_distinct = colstats->n_distinct - colstats->n_mcv;
    if (other_distinct > 0) {
        return (1.0 - mcv_total_freq - colstats->null_fraction) / 
               other_distinct;
    }
    
    return DEFAULT_EQUALITY_SELECTIVITY;
}

// Estimate range selectivity using histogram
double estimate_range_selectivity(
    Qual* qual,
    SBStatistics* stats)
{
    ColumnStats* colstats = get_column_stats(stats, qual->column);
    
    if (colstats == NULL || colstats->histogram_bounds == NULL) {
        return DEFAULT_RANGE_SELECTIVITY;
    }
    
    // Find position in histogram
    double low_sel = 0.0;
    double high_sel = 1.0;
    
    if (qual->has_lower_bound) {
        low_sel = find_histogram_position(colstats->histogram_bounds,
                                         colstats->n_histogram_buckets,
                                         qual->lower_bound);
    }
    
    if (qual->has_upper_bound) {
        high_sel = find_histogram_position(colstats->histogram_bounds,
                                          colstats->n_histogram_buckets,
                                          qual->upper_bound);
    }
    
    double selectivity = high_sel - low_sel;
    
    // Adjust for NULL fraction
    selectivity *= (1.0 - colstats->null_fraction);
    
    return selectivity;
}
```

## 5. Adaptive Query Execution

### 5.1 Runtime Statistics Collection

```c
// Runtime statistics for adaptive execution
typedef struct runtime_stats {
    // Actual vs estimated
    uint64_t        actual_rows;       // Actual rows processed
    double          estimated_rows;    // Estimated rows
    double          estimation_error;  // Error ratio
    
    // Execution metrics
    double          actual_time_ms;    // Actual execution time
    double          estimated_cost;    // Estimated cost
    
    // Memory usage
    uint64_t        peak_memory;       // Peak memory usage
    uint64_t        temp_space_used;   // Temp space used
    
    // I/O statistics
    uint64_t        pages_read;        // Pages read
    uint64_t        pages_written;     // Pages written
    uint64_t        cache_hits;        // Cache hits
    uint64_t        cache_misses;      // Cache misses
} RuntimeStats;

// Adaptive execution context
typedef struct adaptive_context {
    // Original plan
    PlanNode*       original_plan;     // Original plan
    
    // Runtime statistics
    RuntimeStats**  node_stats;        // Stats per node
    
    // Adaptation decisions
    bool            should_replan;     // Should replan
    AdaptationReason replan_reason;    // Why replan
    
    // Alternative plans
    PlanNode*       alternative_plan;  // Alternative plan
    
    // Thresholds
    double          error_threshold;   // Error threshold for replan
    uint64_t        row_threshold;     // Row threshold for adaptation
} AdaptiveContext;

// Check if replanning needed
bool check_replan_needed(
    AdaptiveContext* ctx,
    PlanNode* node,
    RuntimeStats* stats)
{
    // Check estimation error
    double error = fabs(stats->actual_rows - stats->estimated_rows) / 
                  MAX(stats->estimated_rows, 1.0);
    
    if (error > ctx->error_threshold) {
        ctx->should_replan = true;
        ctx->replan_reason = REASON_ESTIMATION_ERROR;
        return true;
    }
    
    // Check if wrong join method
    if (node->type == T_NestLoop && 
        stats->actual_rows > HASH_JOIN_THRESHOLD) {
        ctx->should_replan = true;
        ctx->replan_reason = REASON_WRONG_JOIN_METHOD;
        return true;
    }
    
    // Check memory usage
    if (stats->peak_memory > available_memory() * 0.8) {
        ctx->should_replan = true;
        ctx->replan_reason = REASON_MEMORY_PRESSURE;
        return true;
    }
    
    return false;
}
```

### 5.2 Plan Adaptation

```c
// Adapt plan during execution
PlanNode* adapt_plan(
    AdaptiveContext* ctx,
    PlanNode* current_node,
    RuntimeStats* stats)
{
    // Update statistics with runtime information
    update_statistics_from_runtime(stats);
    
    // Replan remaining portion
    PlanNode* new_plan = replan_subtree(current_node, stats);
    
    // Compare costs
    double remaining_cost = estimate_remaining_cost(current_node);
    double new_plan_cost = new_plan->cost.total_cost;
    double switch_cost = estimate_switch_cost(current_node, new_plan);
    
    if (new_plan_cost + switch_cost < remaining_cost * ADAPTATION_THRESHOLD) {
        // Switch to new plan
        return switch_to_plan(current_node, new_plan);
    }
    
    return current_node;  // Keep current plan
}

// Switch execution to new plan
PlanNode* switch_to_plan(
    PlanNode* old_plan,
    PlanNode* new_plan)
{
    // Save intermediate results if needed
    TupleStore* intermediate = NULL;
    
    if (needs_intermediate_results(old_plan, new_plan)) {
        intermediate = save_intermediate_results(old_plan);
    }
    
    // Initialize new plan
    initialize_plan_state(new_plan);
    
    // Restore intermediate results
    if (intermediate != NULL) {
        restore_intermediate_results(new_plan, intermediate);
    }
    
    return new_plan;
}
```

## 6. Query Plan Caching

### 6.1 Plan Cache Structure

```c
// Cached query plan
typedef struct cached_plan {
    // Plan identification
    uint64_t        plan_hash;         // Plan hash
    char*           query_text;        // Original query
    
    // The plan
    PlanNode*       plan_tree;         // Plan tree
    BLR*            compiled_blr;      // Compiled BLR
    
    // Statistics snapshot
    StatSnapshot*   stats_snapshot;    // Statistics when planned
    
    // Usage statistics
    uint64_t        use_count;         // Times used
    uint64_t        total_time_saved;  // Time saved by caching
    time_t          created_time;      // Creation time
    time_t          last_used_time;    // Last use time
    
    // Performance history
    double          avg_execution_time; // Average execution time
    double          min_execution_time; // Best execution time
    double          max_execution_time; // Worst execution time
    
    // Validity
    bool            is_valid;          // Plan is valid
    List*           dependencies;      // Tables/indexes used
} CachedPlan;

// Plan cache
typedef struct plan_cache {
    // Cache storage
    HashTable*      cache_table;       // Hash table
    LRUList*        lru_list;          // LRU list
    
    // Cache configuration
    uint32_t        max_entries;       // Maximum entries
    uint64_t        max_memory;        // Maximum memory
    uint32_t        ttl_seconds;       // Time to live
    
    // Statistics
    uint64_t        hits;              // Cache hits
    uint64_t        misses;            // Cache misses
    uint64_t        evictions;         // Evictions
    uint64_t        invalidations;     // Invalidations
    
    // Synchronization
    pthread_rwlock_t lock;             // Read-write lock
} PlanCache;

// Look up plan in cache
CachedPlan* lookup_cached_plan(
    PlanCache* cache,
    const char* query,
    ParamValue* params)
{
    uint64_t hash = hash_query_with_params(query, params);
    
    pthread_rwlock_rdlock(&cache->lock);
    
    CachedPlan* plan = hash_table_get(cache->cache_table, hash);
    
    if (plan != NULL) {
        // Check validity
        if (plan->is_valid && !stats_significantly_changed(plan)) {
            // Cache hit
            plan->use_count++;
            plan->last_used_time = time(NULL);
            cache->hits++;
            
            // Move to front of LRU
            lru_move_to_front(cache->lru_list, plan);
            
            pthread_rwlock_unlock(&cache->lock);
            return plan;
        } else {
            // Invalid - remove from cache
            hash_table_remove(cache->cache_table, hash);
            lru_remove(cache->lru_list, plan);
            free_cached_plan(plan);
            cache->invalidations++;
        }
    }
    
    cache->misses++;
    pthread_rwlock_unlock(&cache->lock);
    return NULL;
}
```

## 7. Parallel Query Execution

### 7.1 Parallel Planning

```c
// Parallel execution context
typedef struct parallel_context {
    // Worker configuration
    uint16_t        n_workers;         // Number of workers
    uint16_t        n_launched;        // Workers launched
    
    // Shared memory
    dsm_segment*    dsm_seg;           // Dynamic shared memory
    
    // Work distribution
    ParallelHeapScan* heap_scan;       // Parallel heap scan
    ParallelIndexScan* index_scan;     // Parallel index scan
    
    // Coordination
    Barrier         barrier;           // Synchronization barrier
    
    // Statistics
    RuntimeStats*   worker_stats;      // Per-worker statistics
} ParallelContext;

// Generate parallel plan
PlanNode* generate_parallel_plan(
    QueryPlanner* planner,
    PlanNode* serial_plan)
{
    // Check if parallelism beneficial
    if (!should_parallelize(serial_plan)) {
        return serial_plan;
    }
    
    // Determine degree of parallelism
    uint16_t n_workers = determine_parallel_degree(serial_plan);
    
    // Create gather node
    PlanNode* gather = create_gather_node(n_workers);
    
    // Parallelize scan nodes
    PlanNode* parallel_plan = parallelize_plan(serial_plan, n_workers);
    
    gather->left_child = parallel_plan;
    
    // Recalculate costs
    recalculate_parallel_costs(gather, n_workers);
    
    return gather;
}

// Determine degree of parallelism
uint16_t determine_parallel_degree(PlanNode* plan) {
    // Consider table size
    double table_size_gb = estimate_table_size(plan) / (1024.0 * 1024.0 * 1024.0);
    
    // Base workers on size
    uint16_t size_based = (uint16_t)(table_size_gb / 1.0);  // 1 worker per GB
    
    // Consider available CPUs
    uint16_t cpu_count = get_cpu_count();
    uint16_t cpu_based = cpu_count / 2;  // Use half of CPUs
    
    // Consider cost
    double serial_cost = plan->cost.total_cost;
    uint16_t cost_based = (uint16_t)(serial_cost / 10000.0);
    
    // Take minimum
    uint16_t n_workers = MIN(size_based, MIN(cpu_based, cost_based));
    
    // Apply limits
    n_workers = MAX(n_workers, 1);
    n_workers = MIN(n_workers, MAX_PARALLEL_WORKERS);
    
    return n_workers;
}
```

## 8. Query Optimization Rules

### 8.1 Transformation Rules

```c
// Query transformation rule
typedef struct transform_rule {
    RuleType        type;              // Rule type
    bool            (*can_apply)(PlanNode* node); // Check if applicable
    PlanNode*       (*apply)(PlanNode* node);     // Apply transformation
    double          benefit;           // Expected benefit
} TransformRule;

// Transformation rules
static TransformRule transform_rules[] = {
    // Predicate pushdown
    {
        .type = RULE_PREDICATE_PUSHDOWN,
        .can_apply = can_push_predicate,
        .apply = push_predicate_down,
        .benefit = 0.5
    },
    
    // Join reordering
    {
        .type = RULE_JOIN_REORDER,
        .can_apply = can_reorder_joins,
        .apply = reorder_joins,
        .benefit = 0.3
    },
    
    // Subquery unnesting
    {
        .type = RULE_UNNEST_SUBQUERY,
        .can_apply = can_unnest_subquery,
        .apply = unnest_subquery,
        .benefit = 0.4
    },
    
    // Index usage
    {
        .type = RULE_USE_INDEX,
        .can_apply = can_use_index,
        .apply = convert_to_index_scan,
        .benefit = 0.6
    },
    
    // Materialization
    {
        .type = RULE_MATERIALIZE,
        .can_apply = should_materialize,
        .apply = add_materialize_node,
        .benefit = 0.2
    }
};

// Apply transformation rules
PlanNode* apply_transformations(PlanNode* plan) {
    bool changed = true;
    
    while (changed) {
        changed = false;
        
        for (TransformRule* rule : transform_rules) {
            if (rule->can_apply(plan)) {
                PlanNode* new_plan = rule->apply(plan);
                
                // Check if improvement
                if (new_plan->cost.total_cost < 
                    plan->cost.total_cost * (1.0 - rule->benefit * 0.1)) {
                    plan = new_plan;
                    changed = true;
                }
            }
        }
    }
    
    return plan;
}
```

## 9. EXPLAIN Output

### 9.1 EXPLAIN Implementation

```c
// EXPLAIN output format
typedef enum explain_format {
    EXPLAIN_FORMAT_TEXT,
    EXPLAIN_FORMAT_JSON,
    EXPLAIN_FORMAT_XML,
    EXPLAIN_FORMAT_YAML
} ExplainFormat;

// EXPLAIN options
typedef struct explain_options {
    bool            analyze;           // Include runtime statistics
    bool            verbose;           // Verbose output
    bool            costs;             // Include costs
    bool            buffers;           // Include buffer usage
    bool            timing;            // Include timing
    ExplainFormat   format;           // Output format
} ExplainOptions;

// Generate EXPLAIN output
char* explain_plan(
    PlanNode* plan,
    ExplainOptions* options)
{
    StringBuilder* output = create_string_builder();
    
    if (options->format == EXPLAIN_FORMAT_JSON) {
        append_string(output, "{\n");
        explain_node_json(output, plan, 1, options);
        append_string(output, "}\n");
    } else {
        explain_node_text(output, plan, 0, "", options);
    }
    
    return finish_string_builder(output);
}

// Explain node in text format
void explain_node_text(
    StringBuilder* output,
    PlanNode* node,
    int indent,
    const char* prefix,
    ExplainOptions* options)
{
    // Indentation
    for (int i = 0; i < indent; i++) {
        append_string(output, "  ");
    }
    
    append_string(output, prefix);
    
    // Node type
    append_string(output, get_node_type_string(node->type));
    
    // Costs
    if (options->costs) {
        append_format(output, "  (cost=%.2f..%.2f rows=%.0f width=%d)",
                     node->cost.startup_cost,
                     node->cost.total_cost,
                     node->cost.plan_rows,
                     node->cost.plan_width);
    }
    
    // Runtime statistics
    if (options->analyze && node->runtime_stats != NULL) {
        RuntimeStats* stats = node->runtime_stats;
        append_format(output, " (actual time=%.3f..%.3f rows=%lu loops=%lu)",
                     stats->startup_time_ms,
                     stats->total_time_ms,
                     stats->actual_rows,
                     stats->loops);
    }
    
    append_string(output, "\n");
    
    // Node-specific details
    explain_node_details(output, node, indent + 1, options);
    
    // Explain children
    if (node->left_child != NULL) {
        explain_node_text(output, node->left_child, indent + 1, 
                         "->  ", options);
    }
    
    if (node->right_child != NULL) {
        explain_node_text(output, node->right_child, indent + 1,
                         "->  ", options);
    }
}
```

## Implementation Timeline

Following the ProjectPlan phases:

1. **Phase 13**: Basic cost-based optimizer with statistics
2. **Enhancement**: Multi-column statistics and histograms
3. **Enhancement**: Adaptive query execution
4. **Future**: Machine learning cost model

This specification provides a complete blueprint for ScratchBird's query optimizer, combining proven techniques with modern adaptive features.

## Related Specifications

- [PARALLEL_EXECUTION_ARCHITECTURE.md](PARALLEL_EXECUTION_ARCHITECTURE.md) - Parallel execution model (Beta)
