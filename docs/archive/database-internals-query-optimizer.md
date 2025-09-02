# Database Internals: Query Optimizer

## Table of Contents
1. [FirebirdSQL Query Optimizer](#firebirdsql-query-optimizer)
2. [PostgreSQL Query Optimizer](#postgresql-query-optimizer)
3. [MySQL/MariaDB Query Optimizer](#mysqlmariadb-query-optimizer)
4. [Microsoft SQL Server Query Optimizer](#microsoft-sql-server-query-optimizer)

---

# FirebirdSQL Query Optimizer

## Statistics Collection

### Firebird Statistics System
```c
// Index statistics structure
typedef struct index_statistics {
    ULONG   idx_id;             // Index ID
    double  idx_selectivity;    // Selectivity (0.0 to 1.0)
    ULONG   idx_depth;          // B-Tree depth
    ULONG   idx_leaf_buckets;   // Number of leaf pages
    ULONG   idx_nodes;          // Total number of nodes
    ULONG   idx_data_pages;     // Number of data pages
    double  idx_clustering;     // Clustering factor
    ULONG   idx_total_duplicates; // Total duplicate keys
    ULONG   idx_max_duplicates;   // Max duplicates for one key
    double  idx_average_key_length; // Average key length
    FB_UINT64 idx_total_key_length; // Total of all key lengths
} IndexStatistics;

// Histogram structure
typedef struct histogram {
    USHORT  hist_type;          // Histogram type
    USHORT  hist_buckets;       // Number of buckets
    double* hist_boundaries;    // Bucket boundaries
    ULONG*  hist_frequencies;   // Frequencies per bucket
    double  hist_null_fraction; // Fraction of NULLs
    ULONG   hist_distinct_values; // Number of distinct values
} Histogram;

// Update index statistics
void OPT_update_index_statistics(
    Database*   dbb,
    IndexDesc*  index)
{
    IndexStatistics stats;
    memset(&stats, 0, sizeof(stats));
    
    stats.idx_id = index->idx_id;
    
    // Scan index to collect statistics
    IndexScan scan;
    BTR_start_scan(&scan, index, NULL, NULL);
    
    IndexKey* prev_key = NULL;
    ULONG duplicate_count = 0;
    ULONG distinct_keys = 0;
    
    while (BTR_next_record(&scan)) {
        IndexKey* key = scan.current_key;
        
        stats.idx_nodes++;
        stats.idx_total_key_length += key->key_length;
        
        if (prev_key && BTR_compare_keys(prev_key, key) == 0) {
            duplicate_count++;
            stats.idx_total_duplicates++;
        } else {
            if (duplicate_count > stats.idx_max_duplicates) {
                stats.idx_max_duplicates = duplicate_count;
            }
            duplicate_count = 1;
            distinct_keys++;
        }
        
        prev_key = key;
    }
    
    BTR_end_scan(&scan);
    
    // Calculate derived statistics
    stats.idx_selectivity = (double)distinct_keys / stats.idx_nodes;
    stats.idx_average_key_length = (double)stats.idx_total_key_length / stats.idx_nodes;
    
    // Calculate clustering factor
    stats.idx_clustering = OPT_calculate_clustering_factor(index);
    
    // Store statistics
    MET_update_index_statistics(dbb, &stats);
}

// Build histogram
Histogram* OPT_build_histogram(
    Database*   dbb,
    FieldDesc*  field,
    USHORT      buckets)
{
    Histogram* hist = FB_NEW Histogram();
    hist->hist_type = HIST_EQUAL_HEIGHT;
    hist->hist_buckets = buckets;
    hist->hist_boundaries = FB_NEW double[buckets + 1];
    hist->hist_frequencies = FB_NEW ULONG[buckets];
    
    // Sample table data
    RecordSample* sample = OPT_sample_records(dbb, field->fld_relation, 
                                             SAMPLE_SIZE);
    
    // Sort sample values
    qsort(sample->values, sample->count, sizeof(double), compare_doubles);
    
    // Create equal-height buckets
    ULONG records_per_bucket = sample->count / buckets;
    
    for (USHORT i = 0; i < buckets; i++) {
        ULONG start_idx = i * records_per_bucket;
        ULONG end_idx = (i == buckets - 1) ? sample->count - 1 : 
                        (i + 1) * records_per_bucket - 1;
        
        hist->hist_boundaries[i] = sample->values[start_idx];
        hist->hist_frequencies[i] = end_idx - start_idx + 1;
    }
    
    hist->hist_boundaries[buckets] = sample->values[sample->count - 1];
    
    // Calculate NULL fraction
    hist->hist_null_fraction = (double)sample->null_count / sample->total_count;
    
    // Count distinct values
    hist->hist_distinct_values = OPT_count_distinct(sample);
    
    return hist;
}
```

## Cost Model

### Firebird Cost Calculation
```c
// Cost factors
typedef struct cost_factors {
    double  cpu_operator_cost;      // CPU cost per operator
    double  cpu_tuple_cost;         // CPU cost per tuple
    double  random_page_cost;       // Random I/O cost
    double  seq_page_cost;          // Sequential I/O cost
    double  index_tuple_cost;       // Cost per index tuple
    double  network_transfer_cost;  // Network transfer cost
} CostFactors;

// Default cost factors
static const CostFactors default_costs = {
    .cpu_operator_cost = 0.0025,
    .cpu_tuple_cost = 0.01,
    .random_page_cost = 4.0,
    .seq_page_cost = 1.0,
    .index_tuple_cost = 0.005,
    .network_transfer_cost = 0.1
};

// Query plan cost structure
typedef struct plan_cost {
    double  startup_cost;        // Cost before first tuple
    double  total_cost;         // Total cost
    double  rows;               // Estimated rows
    double  width;              // Average row width
} PlanCost;

// Calculate index scan cost
PlanCost OPT_index_scan_cost(
    Database*           dbb,
    IndexDesc*          index,
    double              selectivity,
    BooleanExpression*  quals)
{
    PlanCost cost;
    IndexStatistics* stats = MET_get_index_statistics(dbb, index->idx_id);
    
    // Estimate number of rows
    double total_rows = REL_get_row_count(index->idx_relation);
    cost.rows = total_rows * selectivity;
    
    // Calculate index pages to read
    double index_pages = stats->idx_leaf_buckets * selectivity;
    
    // Calculate data pages to read (considering clustering)
    double data_pages;
    if (stats->idx_clustering > 0.9) {
        // Well clustered - mostly sequential reads
        data_pages = cost.rows * PAGE_SIZE / AVG_RECORD_SIZE / RECORDS_PER_PAGE;
    } else {
        // Poorly clustered - mostly random reads
        data_pages = cost.rows;
    }
    
    // Calculate costs
    cost.startup_cost = stats->idx_depth * default_costs.random_page_cost;
    
    cost.total_cost = cost.startup_cost +
                     index_pages * default_costs.random_page_cost +
                     data_pages * (stats->idx_clustering * default_costs.seq_page_cost +
                                  (1 - stats->idx_clustering) * default_costs.random_page_cost) +
                     cost.rows * default_costs.cpu_tuple_cost;
    
    // Add qual evaluation cost
    if (quals) {
        cost.total_cost += cost.rows * OPT_qual_cost(quals);
    }
    
    cost.width = REL_get_average_record_length(index->idx_relation);
    
    return cost;
}

// Calculate join cost
PlanCost OPT_join_cost(
    JoinType    join_type,
    PlanCost*   outer_cost,
    PlanCost*   inner_cost,
    double      join_selectivity)
{
    PlanCost cost;
    
    switch (join_type) {
        case JOIN_NESTED_LOOP:
            // Nested loop join
            cost.startup_cost = outer_cost->startup_cost + inner_cost->startup_cost;
            cost.total_cost = outer_cost->total_cost +
                            outer_cost->rows * inner_cost->total_cost +
                            outer_cost->rows * inner_cost->rows * 
                            default_costs.cpu_operator_cost;
            cost.rows = outer_cost->rows * inner_cost->rows * join_selectivity;
            break;
            
        case JOIN_HASH:
            // Hash join
            double hash_build_cost = inner_cost->rows * default_costs.cpu_tuple_cost * 2;
            double hash_probe_cost = outer_cost->rows * default_costs.cpu_tuple_cost;
            
            cost.startup_cost = outer_cost->startup_cost + 
                              inner_cost->total_cost + hash_build_cost;
            cost.total_cost = cost.startup_cost + 
                            outer_cost->total_cost + hash_probe_cost;
            cost.rows = outer_cost->rows * inner_cost->rows * join_selectivity;
            break;
            
        case JOIN_SORT_MERGE:
            // Sort-merge join
            double outer_sort_cost = outer_cost->rows * log2(outer_cost->rows) * 
                                   default_costs.cpu_operator_cost;
            double inner_sort_cost = inner_cost->rows * log2(inner_cost->rows) * 
                                   default_costs.cpu_operator_cost;
            
            cost.startup_cost = outer_cost->total_cost + outer_sort_cost +
                              inner_cost->total_cost + inner_sort_cost;
            cost.total_cost = cost.startup_cost +
                            (outer_cost->rows + inner_cost->rows) * 
                            default_costs.cpu_tuple_cost;
            cost.rows = outer_cost->rows * inner_cost->rows * join_selectivity;
            break;
    }
    
    cost.width = outer_cost->width + inner_cost->width;
    
    return cost;
}
```

## Join Algorithms

### Firebird Join Implementation
```c
// Join node structure
typedef struct join_node {
    JoinType    join_type;       // Join algorithm
    RecordSource* outer_source;   // Outer relation
    RecordSource* inner_source;   // Inner relation
    BoolExpr*   join_condition;   // Join predicate
    PlanCost    cost;            // Estimated cost
} JoinNode;

// Nested loop join
RecordBuffer* EXEC_nested_loop_join(
    JoinNode*   join,
    Request*    request)
{
    RecordBuffer* result = FB_NEW RecordBuffer();
    
    // Open outer source
    RSB_open(join->outer_source);
    
    // Fetch each outer row
    while (RSB_get_record(join->outer_source, request)) {
        Record* outer_record = request->req_record;
        
        // Open inner source for each outer row
        RSB_open(join->inner_source);
        
        // Fetch each inner row
        while (RSB_get_record(join->inner_source, request)) {
            Record* inner_record = request->req_record;
            
            // Evaluate join condition
            if (EVL_boolean(join->join_condition, request)) {
                // Combine records
                Record* joined = combine_records(outer_record, inner_record);
                add_to_buffer(result, joined);
            }
        }
        
        // Close inner source
        RSB_close(join->inner_source);
    }
    
    // Close outer source
    RSB_close(join->outer_source);
    
    return result;
}

// Hash join
RecordBuffer* EXEC_hash_join(
    JoinNode*   join,
    Request*    request)
{
    RecordBuffer* result = FB_NEW RecordBuffer();
    HashTable* hash_table = FB_NEW HashTable();
    
    // Build phase - hash inner relation
    RSB_open(join->inner_source);
    
    while (RSB_get_record(join->inner_source, request)) {
        Record* inner_record = request->req_record;
        
        // Extract join key
        Value* key = extract_join_key(inner_record, join->join_condition);
        
        // Add to hash table
        hash_table_insert(hash_table, key, inner_record);
    }
    
    RSB_close(join->inner_source);
    
    // Probe phase - scan outer relation
    RSB_open(join->outer_source);
    
    while (RSB_get_record(join->outer_source, request)) {
        Record* outer_record = request->req_record;
        
        // Extract join key
        Value* key = extract_join_key(outer_record, join->join_condition);
        
        // Probe hash table
        HashBucket* bucket = hash_table_lookup(hash_table, key);
        
        for (Record* inner_record = bucket->first; 
             inner_record; 
             inner_record = inner_record->next) {
            
            // Evaluate full join condition
            if (EVL_boolean(join->join_condition, request)) {
                Record* joined = combine_records(outer_record, inner_record);
                add_to_buffer(result, joined);
            }
        }
    }
    
    RSB_close(join->outer_source);
    
    // Cleanup
    hash_table_destroy(hash_table);
    
    return result;
}

// Sort-merge join
RecordBuffer* EXEC_sort_merge_join(
    JoinNode*   join,
    Request*    request)
{
    RecordBuffer* result = FB_NEW RecordBuffer();
    
    // Sort both relations
    SortedStream* outer_sorted = sort_relation(join->outer_source, 
                                              join->join_condition);
    SortedStream* inner_sorted = sort_relation(join->inner_source,
                                              join->join_condition);
    
    // Merge phase
    Record* outer_record = get_next_sorted(outer_sorted);
    Record* inner_record = get_next_sorted(inner_sorted);
    
    while (outer_record && inner_record) {
        int cmp = compare_join_keys(outer_record, inner_record,
                                   join->join_condition);
        
        if (cmp < 0) {
            // Outer < Inner - advance outer
            outer_record = get_next_sorted(outer_sorted);
        } else if (cmp > 0) {
            // Outer > Inner - advance inner
            inner_record = get_next_sorted(inner_sorted);
        } else {
            // Match found - handle duplicates
            RecordList* outer_group = get_equal_group(outer_sorted, outer_record);
            RecordList* inner_group = get_equal_group(inner_sorted, inner_record);
            
            // Cartesian product of groups
            for (Record* o = outer_group->first; o; o = o->next) {
                for (Record* i = inner_group->first; i; i = i->next) {
                    if (EVL_boolean(join->join_condition, request)) {
                        Record* joined = combine_records(o, i);
                        add_to_buffer(result, joined);
                    }
                }
            }
            
            // Advance both streams
            outer_record = get_next_sorted(outer_sorted);
            inner_record = get_next_sorted(inner_sorted);
        }
    }
    
    return result;
}
```

---

# PostgreSQL Query Optimizer

## Statistics Collection

### PostgreSQL Statistics System
```c
// Statistics stored in pg_statistic
typedef struct FormData_pg_statistic {
    Oid         starelid;       // Relation OID
    int16       staattnum;      // Attribute number
    bool        stainherit;     // Includes inherited children
    float4      stanullfrac;    // Fraction of NULL values
    float4      stawidth;       // Average width in bytes
    float4      stadistinct;    // Number of distinct values
    int16       stakind[STATISTIC_NUM_SLOTS];  // Statistics kinds
    Oid         staop[STATISTIC_NUM_SLOTS];    // Operators
    Oid         stacoll[STATISTIC_NUM_SLOTS];  // Collations
    // Variable length arrays follow
} FormData_pg_statistic;

// Statistics kinds
#define STATISTIC_KIND_MCV      1   // Most common values
#define STATISTIC_KIND_HISTOGRAM 2   // Histogram
#define STATISTIC_KIND_CORRELATION 3 // Correlation

// ANALYZE command implementation
void
do_analyze_rel(Relation onerel, VacuumParams *params,
              List *va_cols, AcquireSampleRowsFunc acquirefunc,
              BlockNumber relpages, bool inh,
              bool in_outer_xact, int elevel)
{
    int         attr_cnt;
    VacAttrStats **vacattrstats;
    int         targrows;
    double      totalrows;
    HeapTuple  *rows;
    int         numrows;
    
    // Determine sample size
    targrows = (params->options & VACOPT_ANALYZE) ? 
               default_statistics_target * 300 : 30000;
    
    // Acquire sample rows
    rows = (HeapTuple *) palloc(targrows * sizeof(HeapTuple));
    numrows = (*acquirefunc) (onerel, elevel, rows, targrows,
                             &totalrows, &totaldeadrows);
    
    // Compute statistics for each column
    for (int i = 0; i < attr_cnt; i++) {
        VacAttrStats *stats = vacattrstats[i];
        
        // Compute basic statistics
        stats->stanullfrac = (double) stats->null_cnt / numrows;
        stats->stawidth = stats->total_width / stats->nonnull_cnt;
        
        // Compute distinct values
        if (stats->stadistinct > 0) {
            // Exact count
            stats->stadistinct = count_distinct(stats);
        } else {
            // Estimate using sample
            stats->stadistinct = estimate_distinct(stats, totalrows);
        }
        
        // Build MCV list
        compute_scalar_stats(stats, fetchfunc, samplerows, totalrows);
        
        // Build histogram
        if (stats->stadistinct > stats->num_mcv) {
            build_histogram(stats);
        }
        
        // Store statistics
        update_attstats(onerel->rd_id, stats->attr->attnum, stats);
    }
    
    // Update pg_class statistics
    vac_update_relstats(onerel, relpages, totalrows, 
                       visibilitymap_count(onerel),
                       hasindex, InvalidTransactionId,
                       InvalidMultiXactId, in_outer_xact);
}

// Build histogram
void
build_histogram(VacAttrStats *stats)
{
    int         num_hist = stats->attr->attstattarget;
    Datum      *hist_values;
    int         nvals = stats->nonnull_cnt - stats->num_mcv;
    
    // Sort non-MCV values
    qsort_arg(stats->values, nvals, sizeof(ScalarItem),
             compare_scalars, stats->tupDesc);
    
    // Create equi-depth histogram
    hist_values = (Datum *) palloc(num_hist * sizeof(Datum));
    
    for (int i = 0; i < num_hist; i++) {
        int pos = (i * nvals) / num_hist;
        hist_values[i] = stats->values[pos].value;
    }
    
    stats->stakind[0] = STATISTIC_KIND_HISTOGRAM;
    stats->stavalues[0] = hist_values;
    stats->numvalues[0] = num_hist;
}
```

## Cost Model

### PostgreSQL Cost Estimation
```c
// Cost parameters (from postgresql.conf)
typedef struct {
    double  seq_page_cost;          // Sequential page read
    double  random_page_cost;       // Random page read
    double  cpu_tuple_cost;         // Processing per tuple
    double  cpu_index_tuple_cost;   // Processing per index tuple
    double  cpu_operator_cost;      // Processing per operator
    double  parallel_setup_cost;    // Parallel query setup
    double  parallel_tuple_cost;    // Parallel tuple transfer
    double  effective_cache_size;   // Effective cache size
} CostParams;

// Path cost structure
typedef struct Path {
    NodeTag     type;
    RelOptInfo *parent;         // Relation this path is for
    PathTarget *pathtarget;     // Desired output columns
    ParamPathInfo *param_info;  // Parameterization info
    bool        parallel_aware;  // Parallel-aware path?
    bool        parallel_safe;   // Safe for parallel execution?
    int         parallel_workers; // Number of parallel workers
    Cost        startup_cost;    // Cost before first tuple
    Cost        total_cost;      // Total cost
    double      rows;           // Estimated number of rows
    List       *pathkeys;       // Sort ordering
} Path;

// Index scan cost estimation
void
cost_index(IndexPath *path, PlannerInfo *root, double loop_count,
          bool partial_path)
{
    IndexOptInfo *index = path->indexinfo;
    RelOptInfo *baserel = path->path.parent;
    Cost        startup_cost = 0;
    Cost        run_cost = 0;
    Cost        cpu_run_cost = 0;
    Cost        indexStartupCost;
    Cost        indexTotalCost;
    double      indexSelectivity;
    double      indexCorrelation;
    Selectivity indexSelectivity;
    double      index_pages;
    double      pages_fetched;
    double      spc_random_page_cost;
    double      spc_seq_page_cost;
    
    // Get index statistics
    get_index_stats(root, index, &indexSelectivity,
                   &indexCorrelation, &index_pages);
    
    // Estimate index pages accessed
    indexSelectivity = clauselist_selectivity(root, path->indexclauses,
                                             baserel->relid,
                                             JOIN_INNER, NULL);
    
    // Calculate index scan cost
    indexStartupCost = index_pages * spc_seq_page_cost;
    indexTotalCost = indexStartupCost +
                    path->path.rows * cpu_index_tuple_cost;
    
    // Estimate heap pages fetched
    pages_fetched = index_pages_fetched(path->path.rows,
                                       baserel->pages,
                                       (double) index->pages,
                                       root);
    
    // Apply Mackert-Lohman formula for cache effects
    pages_fetched = pages_fetched * (2.0 - pages_fetched / effective_cache_size);
    
    // Calculate total cost
    if (indexCorrelation < 0.5) {
        // Poor correlation - mostly random I/O
        run_cost = pages_fetched * spc_random_page_cost;
    } else {
        // Good correlation - mix of sequential and random
        run_cost = pages_fetched * 
                  (spc_seq_page_cost * indexCorrelation +
                   spc_random_page_cost * (1.0 - indexCorrelation));
    }
    
    // Add CPU costs
    cpu_run_cost = path->path.rows * cpu_tuple_cost;
    
    // Set path costs
    path->path.startup_cost = startup_cost + indexStartupCost;
    path->path.total_cost = startup_cost + indexTotalCost + 
                           run_cost + cpu_run_cost;
}

// Join cost estimation
void
initial_cost_hashjoin(PlannerInfo *root, JoinCostWorkspace *workspace,
                     JoinPath *path, RelOptInfo *outer_rel,
                     RelOptInfo *inner_rel, JoinPathExtraData *extra)
{
    Cost        startup_cost = 0;
    Cost        run_cost = 0;
    double      outer_path_rows = outer_rel->rows;
    double      inner_path_rows = inner_rel->rows;
    double      inner_path_rows_total = inner_path_rows;
    int         num_hashclauses = list_length(hashclauses);
    int         numbuckets;
    int         numbatches;
    double      virtualbuckets;
    Size        hashentrysize;
    
    // Estimate hash table size
    hashentrysize = MAXALIGN(sizeof(HashJoinTupleData)) +
                   MAXALIGN(inner_rel->reltarget->width);
    
    // Determine number of batches needed
    if (!ExecChooseHashTableSize(inner_path_rows_total,
                                inner_rel->reltarget->width,
                                OidIsValid(extra->inner_unique),
                                &numbuckets,
                                &numbatches,
                                &num_skew_mcvs)) {
        // Hash table too large
        workspace->total_cost = disable_cost;
        return;
    }
    
    // Cost of building hash table
    startup_cost = inner_rel->cheapest_total_path->total_cost;
    startup_cost += cpu_operator_cost * inner_path_rows_total;
    
    // Cost of probing hash table
    run_cost = cpu_operator_cost * outer_path_rows * num_hashclauses;
    
    // Account for hash bucket scanning
    virtualbuckets = (double) numbuckets * (double) numbatches;
    run_cost += cpu_tuple_cost * outer_path_rows *
               clamp_row_est(inner_path_rows_total / virtualbuckets) * 0.5;
    
    // Add cost of outer scan
    run_cost += outer_rel->cheapest_total_path->total_cost;
    
    // Account for multiple batches
    if (numbatches > 1) {
        double  spill_cost;
        
        // Cost of writing/reading spill files
        spill_cost = seq_page_cost * (inner_path_rows_total + outer_path_rows) *
                    (1.0 - 1.0 / numbatches);
        startup_cost += spill_cost;
    }
    
    workspace->startup_cost = startup_cost;
    workspace->total_cost = startup_cost + run_cost;
    workspace->run_cost = run_cost;
}
```

## Join Algorithms

### PostgreSQL Join Execution
```c
// Hash join state
typedef struct HashJoinState {
    JoinState   js;             // Base join state
    ExprState  *hashclauses;    // Hash join clauses
    List       *hj_OuterHashKeys; // Outer hash keys
    List       *hj_InnerHashKeys; // Inner hash keys
    HashJoinTable hj_HashTable;  // Hash table
    uint32      hj_CurHashValue; // Current hash value
    int         hj_CurBucketNo;  // Current bucket number
    int         hj_CurSkewBucketNo; // Current skew bucket
    HashJoinTuple hj_CurTuple;  // Current tuple in bucket
    TupleTableSlot *hj_OuterTupleSlot; // Outer tuple slot
    TupleTableSlot *hj_HashTupleSlot;  // Hash tuple slot
    TupleTableSlot *hj_NullOuterTupleSlot; // Null outer tuple
    TupleTableSlot *hj_NullInnerTupleSlot; // Null inner tuple
    TupleTableSlot *hj_FirstOuterTupleSlot; // First outer match
    int         hj_JoinState;   // Join state machine
    bool        hj_MatchedOuter; // Matched outer tuple?
    bool        hj_OuterNotEmpty; // Outer relation not empty?
} HashJoinState;

// Hash join execution
TupleTableSlot *
ExecHashJoin(PlanState *pstate)
{
    HashJoinState *node = castNode(HashJoinState, pstate);
    ExprContext *econtext = node->js.ps.ps_ExprContext;
    HashJoinTable hashtable;
    TupleTableSlot *outerTupleSlot;
    uint32      hashvalue;
    int         batchno;
    
    // State machine for hash join
    for (;;) {
        switch (node->hj_JoinState) {
            case HJ_BUILD_HASHTABLE:
                // Build hash table from inner relation
                hashtable = ExecHashTableCreate(node,
                                               hashOperators,
                                               node->js.ps.ps_ProjInfo != NULL);
                node->hj_HashTable = hashtable;
                
                // Execute inner plan and build hash table
                hashNode = (HashState *) innerPlanState(node);
                ExecHashBuild(hashNode);
                
                // Prepare for probing
                node->hj_JoinState = HJ_NEED_NEW_OUTER;
                
            case HJ_NEED_NEW_OUTER:
                // Get next outer tuple
                outerTupleSlot = ExecProcNode(outerPlanState(node));
                
                if (TupIsNull(outerTupleSlot)) {
                    // No more outer tuples
                    if (HJ_FILL_INNER(node)) {
                        // Need to emit unmatched inner tuples
                        node->hj_JoinState = HJ_FILL_INNER_TUPLES;
                        continue;
                    } else {
                        return NULL;
                    }
                }
                
                // Compute hash value for outer tuple
                econtext->ecxt_outertuple = outerTupleSlot;
                if (ExecHashGetHashValue(hashtable, econtext,
                                        node->hj_OuterHashKeys,
                                        true, false,
                                        &hashvalue)) {
                    // Null key - no match possible
                    node->hj_JoinState = HJ_NEED_NEW_OUTER;
                    continue;
                }
                
                // Find hash bucket
                node->hj_CurHashValue = hashvalue;
                ExecHashGetBucketAndBatch(hashtable, hashvalue,
                                         &node->hj_CurBucketNo,
                                         &batchno);
                
                // Start scanning bucket
                node->hj_CurTuple = hashtable->buckets[node->hj_CurBucketNo];
                node->hj_JoinState = HJ_SCAN_BUCKET;
                
            case HJ_SCAN_BUCKET:
                // Scan current bucket for matches
                for (;;) {
                    if (node->hj_CurTuple == NULL) {
                        // End of bucket - get new outer
                        node->hj_JoinState = HJ_NEED_NEW_OUTER;
                        break;
                    }
                    
                    // Check if hash values match
                    if (node->hj_CurTuple->hashvalue == node->hj_CurHashValue) {
                        // Hash match - check full join quals
                        TupleTableSlot *inntuple;
                        
                        inntuple = ExecStoreMinimalTuple(
                            HJTUPLE_MINTUPLE(node->hj_CurTuple),
                            node->hj_HashTupleSlot,
                            false);
                        
                        econtext->ecxt_innertuple = inntuple;
                        
                        // Test join condition
                        if (ExecQual(node->js.ps.qual, econtext)) {
                            // Match found - return joined tuple
                            TupleTableSlot *result;
                            
                            result = ExecProject(node->js.ps.ps_ProjInfo);
                            
                            // Advance to next tuple for next call
                            node->hj_CurTuple = node->hj_CurTuple->next;
                            
                            return result;
                        }
                    }
                    
                    // Advance to next tuple in bucket
                    node->hj_CurTuple = node->hj_CurTuple->next;
                }
                break;
        }
    }
}

// Merge join execution
TupleTableSlot *
ExecMergeJoin(PlanState *pstate)
{
    MergeJoinState *node = castNode(MergeJoinState, pstate);
    ExprContext *econtext = node->js.ps.ps_ExprContext;
    TupleTableSlot *outerTupleSlot;
    TupleTableSlot *innerTupleSlot;
    int         compareResult;
    
    for (;;) {
        switch (node->mj_JoinState) {
            case EXEC_MJ_INITIALIZE_OUTER:
                // Get first outer tuple
                outerTupleSlot = ExecProcNode(outerPlan);
                node->mj_OuterTupleSlot = outerTupleSlot;
                
                if (TupIsNull(outerTupleSlot)) {
                    node->mj_JoinState = EXEC_MJ_ENDOUTER;
                } else {
                    node->mj_JoinState = EXEC_MJ_INITIALIZE_INNER;
                }
                break;
                
            case EXEC_MJ_INITIALIZE_INNER:
                // Get first inner tuple
                innerTupleSlot = ExecProcNode(innerPlan);
                node->mj_InnerTupleSlot = innerTupleSlot;
                
                if (TupIsNull(innerTupleSlot)) {
                    node->mj_JoinState = EXEC_MJ_ENDINNER;
                } else {
                    node->mj_JoinState = EXEC_MJ_COMPARE;
                }
                break;
                
            case EXEC_MJ_COMPARE:
                // Compare current tuples
                compareResult = MJCompare(node);
                
                if (compareResult == 0) {
                    // Match - join tuples
                    node->mj_JoinState = EXEC_MJ_JOINTUPLES;
                } else if (compareResult < 0) {
                    // Outer < Inner - advance outer
                    node->mj_JoinState = EXEC_MJ_NEXTOUTER;
                } else {
                    // Outer > Inner - advance inner
                    node->mj_JoinState = EXEC_MJ_NEXTINNER;
                }
                break;
                
            case EXEC_MJ_JOINTUPLES:
                // Join matching tuples
                econtext->ecxt_outertuple = node->mj_OuterTupleSlot;
                econtext->ecxt_innertuple = node->mj_InnerTupleSlot;
                
                if (ExecQual(node->js.ps.qual, econtext)) {
                    // Return joined tuple
                    return ExecProject(node->js.ps.ps_ProjInfo);
                }
                
                // Check for more matches
                node->mj_JoinState = EXEC_MJ_NEXTINNER;
                break;
        }
    }
}
```

---

# MySQL/MariaDB Query Optimizer

## Statistics Collection

### InnoDB Statistics
```c
// Table statistics structure
typedef struct dict_table_stats_t {
    ulint       n_rows;         // Number of rows
    ulint       clustered_index_size; // Clustered index size in pages
    ulint       sum_of_other_index_sizes; // Other indexes size
    ib_uint64_t modified_counter; // Modification counter
    time_t      last_update;    // Last update time
    bool        persistent_stats; // Using persistent statistics?
} dict_table_stats_t;

// Index statistics
typedef struct dict_index_stat_t {
    ib_uint64_t n_diff_key_vals[MAX_KEY_PARTS]; // Distinct values per prefix
    ib_uint64_t n_sample_sizes[MAX_KEY_PARTS];  // Sample sizes
    ib_uint64_t n_non_null_key_vals[MAX_KEY_PARTS]; // Non-null values
    ulint       n_leaf_pages;   // Number of leaf pages
    ulint       stat_index_size; // Index size in pages
} dict_index_stat_t;

// Update table statistics
void
dict_stats_update(
    dict_table_t*   table,
    dict_stats_upd_option_t stats_upd_option)
{
    dict_index_t*   index;
    ulint           sum_of_index_sizes = 0;
    
    // Sample table for statistics
    if (table->persistent_stats) {
        // Use persistent statistics
        dict_stats_analyze_persistent(table);
    } else {
        // Use transient statistics
        dict_stats_analyze_transient(table);
    }
    
    // Update each index statistics
    for (index = dict_table_get_first_index(table);
         index != NULL;
         index = dict_table_get_next_index(index)) {
        
        if (dict_index_is_online_ddl(index)) {
            continue;
        }
        
        dict_stats_analyze_index(index);
        
        // Calculate cardinality for each column
        for (ulint i = 0; i < index->n_uniq; i++) {
            dict_stats_analyze_index_level(index, i);
        }
        
        sum_of_index_sizes += index->stat_index_size;
    }
    
    // Update table-level statistics
    table->stat_n_rows = dict_stats_estimate_rows(table);
    table->stat_clustered_index_size = 
        dict_table_get_first_index(table)->stat_index_size;
    table->stat_sum_of_other_index_sizes = 
        sum_of_index_sizes - table->stat_clustered_index_size;
    table->stat_modified_counter = 0;
    table->stats_last_recalc = time(NULL);
}

// Analyze index for statistics
void
dict_stats_analyze_index_level(
    dict_index_t*   index,
    ulint           level)
{
    btr_pcur_t      pcur;
    ulint           n_diff;
    ulint           n_sampled;
    ulint           n_non_null;
    
    // Open cursor at index level
    btr_pcur_open_at_index_side(true, index, BTR_SEARCH_LEAF,
                               &pcur, true, level, &mtr);
    
    // Sample pages
    n_diff = 0;
    n_sampled = 0;
    n_non_null = 0;
    
    for (ulint i = 0; i < N_SAMPLE_PAGES; i++) {
        buf_block_t*    block;
        page_t*         page;
        
        // Get random leaf page
        block = btr_pcur_get_block(&pcur);
        page = buf_block_get_frame(block);
        
        // Analyze records on page
        dict_stats_scan_page(index, level, page, 
                            &n_diff, &n_sampled, &n_non_null);
        
        // Move to next page
        if (!btr_pcur_move_to_next_page(&pcur, &mtr)) {
            break;
        }
    }
    
    // Calculate statistics
    if (n_sampled > 0) {
        index->stat_n_diff_key_vals[level] = 
            (n_diff * index->stat_n_leaf_pages) / n_sampled;
        index->stat_n_non_null_key_vals[level] = 
            (n_non_null * index->stat_n_leaf_pages) / n_sampled;
    }
    
    btr_pcur_close(&pcur);
}
```

## Cost Model

### MySQL Cost Estimation
```c
// Cost model constants
typedef struct Cost_model_constants {
    double row_evaluate_cost;      // Cost to evaluate row
    double key_compare_cost;       // Cost to compare keys
    double memory_temptable_create_cost; // Memory temp table creation
    double memory_temptable_row_cost;    // Memory temp table per row
    double disk_temptable_create_cost;   // Disk temp table creation
    double disk_temptable_row_cost;      // Disk temp table per row
} Cost_model_constants;

// Table access cost
double
handler::table_scan_cost()
{
    double io_cost;
    double cpu_cost;
    
    // Calculate I/O cost
    io_cost = scan_time() * DISK_SEEK_BASE_COST;
    
    // Calculate CPU cost
    cpu_cost = records() * ROW_EVALUATE_COST;
    
    return io_cost + cpu_cost;
}

// Index access cost
double
handler::index_scan_cost(uint index, double rows)
{
    KEY *key_info = table->key_info + index;
    double io_cost;
    double cpu_cost;
    
    // Calculate I/O cost based on index type
    if (key_info->algorithm == HA_KEY_ALG_BTREE) {
        // B-tree index
        double keys_per_block = (stats.block_size / key_info->key_length);
        double blocks = rows / keys_per_block;
        
        // Account for tree depth
        io_cost = (log2(stats.records) + blocks) * DISK_SEEK_BASE_COST;
    } else if (key_info->algorithm == HA_KEY_ALG_HASH) {
        // Hash index
        io_cost = rows * DISK_SEEK_BASE_COST * 0.5; // Assume 50% hit rate
    }
    
    // Add data access cost
    io_cost += rows * DISK_SEEK_BASE_COST;
    
    // Calculate CPU cost
    cpu_cost = rows * (KEY_COMPARE_COST + ROW_EVALUATE_COST);
    
    return io_cost + cpu_cost;
}

// Join cost estimation
double
JOIN::optimize_join_order()
{
    double best_cost = DBL_MAX;
    POSITION *best_positions = nullptr;
    
    // Try different join orders
    if (table_count < MAX_EXHAUSTIVE_SEARCH_TABLES) {
        // Exhaustive search for small number of tables
        best_cost = find_best_join_order_exhaustive(
            0, table_count, record_count, read_time, best_positions);
    } else {
        // Greedy search for large number of tables
        best_cost = greedy_search(best_positions);
    }
    
    return best_cost;
}

// Exhaustive join order search
double
find_best_join_order_exhaustive(
    uint idx,
    uint remaining_tables,
    double record_count,
    double read_time,
    POSITION *positions)
{
    if (remaining_tables == 0) {
        // All tables placed - calculate total cost
        return read_time;
    }
    
    double best_cost = DBL_MAX;
    
    for (uint i = idx; i < table_count; i++) {
        JOIN_TAB *tab = join_tab + i;
        
        if (tab->table->map & cur_embedding_map) {
            continue;  // Already used
        }
        
        // Calculate cost of accessing this table
        double rows = calculate_rows_after_filtering(tab);
        double cost = calculate_scan_cost(tab, rows);
        
        // Add join cost
        if (idx > 0) {
            cost += record_count * rows * JOIN_COST_FACTOR;
        }
        
        // Recursively try remaining tables
        double total_cost = find_best_join_order_exhaustive(
            idx + 1,
            remaining_tables - 1,
            record_count * rows,
            read_time + cost,
            positions);
        
        if (total_cost < best_cost) {
            best_cost = total_cost;
            positions[idx] = *tab->position;
        }
    }
    
    return best_cost;
}
```

## Join Algorithms

### MySQL Join Execution
```c
// Nested loop join variants
enum join_type {
    JT_UNKNOWN,
    JT_SYSTEM,      // System table (one row)
    JT_CONST,       // Constant table
    JT_EQ_REF,      // Unique index lookup
    JT_REF,         // Non-unique index lookup
    JT_REF_OR_NULL, // Ref with extra null check
    JT_RANGE,       // Index range scan
    JT_INDEX_SCAN,  // Full index scan
    JT_ALL          // Full table scan
};

// Block nested loop join
int
JOIN_CACHE::join_matching_records(bool skip_last)
{
    int error = 0;
    JOIN_TAB *tab = join_tab;
    
    // Read records from outer table into buffer
    while (!(error = fill_buffer())) {
        // Process all records in buffer against inner table
        
        // Reset inner table scan
        if ((error = tab->table->file->ha_rnd_init(true))) {
            return error;
        }
        
        // Scan inner table
        while (!(error = tab->table->file->ha_rnd_next(
                    tab->table->record[0]))) {
            
            // Check all buffered records
            for (uchar *rec_ptr = buffer;
                 rec_ptr < buffer + buffer_size;
                 rec_ptr += record_size) {
                
                // Restore outer record
                restore_record(tab - 1, rec_ptr);
                
                // Evaluate join condition
                if (check_match(rec_ptr)) {
                    // Found match - send to next join level
                    if ((error = generate_full_extensions(rec_ptr))) {
                        return error;
                    }
                }
            }
        }
        
        tab->table->file->ha_rnd_end();
        
        if (error != HA_ERR_END_OF_FILE) {
            return error;
        }
    }
    
    return 0;
}

// Hash join (MySQL 8.0+)
int
HashJoinIterator::Read()
{
    for (;;) {
        switch (m_state) {
            case State::READING_ROW_FROM_PROBE_ITERATOR:
                // Read row from probe (outer) iterator
                if (m_probe_input->Read() == 0) {
                    // Got outer row - look up in hash table
                    if (LookupInHashTable()) {
                        return 0;  // Found match
                    }
                    // No match - continue reading
                } else {
                    // No more outer rows
                    return -1;
                }
                break;
                
            case State::READING_FROM_HASH_TABLE:
                // Continue reading from current hash bucket
                if (ReadNextFromHashTable()) {
                    return 0;  // Found another match
                }
                // No more matches for this outer row
                m_state = State::READING_ROW_FROM_PROBE_ITERATOR;
                break;
        }
    }
}

bool
HashJoinIterator::BuildHashTable()
{
    // Read all rows from build (inner) input
    while (m_build_input->Read() == 0) {
        // Calculate hash
        uint64_t hash = CalculateHash(m_build_input->GetRow());
        
        // Add to hash table
        HashJoinRowBuffer::Row *row = m_row_buffer.AddRow(hash);
        
        if (row == nullptr) {
            // Out of memory - spill to disk
            if (!SpillToDisk()) {
                return false;
            }
        }
        
        // Store row data
        StoreFromTableBuffers(m_build_input->tables(), row);
    }
    
    return true;
}
```

---

# Microsoft SQL Server Query Optimizer

## Statistics Collection

### SQL Server Statistics
```c
// Statistics structure
typedef struct STATISTICS {
    ULONG       stats_id;           // Statistics ID
    ULONG       object_id;          // Table ID
    ULONG       index_id;           // Index ID (0 for column stats)
    BYTE        stats_type;         // Statistics type
    FILLFACTOR  filter_definition;  // Filter for filtered statistics
    ULONG       rows;               // Number of rows
    ULONG       rows_sampled;       // Rows sampled
    ULONG       steps;              // Histogram steps
    ULONG       unfiltered_rows;    // Unfiltered row count
    DATETIME    last_updated;       // Last update time
} STATISTICS;

// Histogram step
typedef struct HISTOGRAM_STEP {
    SQL_VARIANT range_hi_key;       // Upper bound value
    double      range_rows;         // Rows with value > previous < this
    double      eq_rows;            // Rows with value = range_hi_key
    ULONG       distinct_range_rows; // Distinct values in range
    double      avg_range_rows;     // Average rows per distinct value
} HISTOGRAM_STEP;

// Auto-create statistics
void
auto_create_statistics(
    TABLE*      table,
    COLUMN*     column,
    QUERY*      query)
{
    STATISTICS* stats;
    
    // Check if statistics needed
    if (!needs_statistics(column, query)) {
        return;
    }
    
    // Check auto-create setting
    if (!database->auto_create_statistics) {
        return;
    }
    
    // Create statistics
    stats = create_statistics(table, column);
    
    // Sample data
    if (table->row_count < FULLSCAN_THRESHOLD) {
        // Full scan for small tables
        stats->rows_sampled = table->row_count;
        sample_rate = 1.0;
    } else {
        // Sample for large tables
        stats->rows_sampled = calculate_sample_size(table->row_count);
        sample_rate = (double)stats->rows_sampled / table->row_count;
    }
    
    // Build histogram
    build_histogram(stats, column, sample_rate);
    
    // Store statistics
    store_statistics(stats);
}

// Build histogram
void
build_histogram(
    STATISTICS* stats,
    COLUMN*     column,
    double      sample_rate)
{
    HISTOGRAM_STEP* histogram;
    VALUE_LIST*     sampled_values;
    ULONG          target_steps = 200;  // Default max steps
    
    // Sample column values
    sampled_values = sample_column_values(column, stats->rows_sampled);
    
    // Sort values
    sort_values(sampled_values);
    
    // Create histogram steps
    histogram = allocate_histogram(target_steps);
    stats->steps = 0;
    
    // Build equal-height histogram
    ULONG values_per_step = sampled_values->count / target_steps;
    
    for (ULONG i = 0; i < target_steps && i * values_per_step < sampled_values->count; i++) {
        HISTOGRAM_STEP* step = &histogram[i];
        ULONG start_idx = i * values_per_step;
        ULONG end_idx = min((i + 1) * values_per_step - 1, sampled_values->count - 1);
        
        // Set range bounds
        step->range_hi_key = sampled_values->values[end_idx];
        
        // Count statistics for this step
        step->range_rows = (end_idx - start_idx) / sample_rate;
        step->eq_rows = count_equal_values(sampled_values, end_idx) / sample_rate;
        step->distinct_range_rows = count_distinct_values(sampled_values, start_idx, end_idx);
        step->avg_range_rows = step->range_rows / step->distinct_range_rows;
        
        stats->steps++;
    }
    
    stats->histogram = histogram;
}

// Incremental statistics update
void
update_statistics_incremental(
    STATISTICS*     stats,
    PARTITION*      partition)
{
    STATISTICS*     partition_stats;
    HISTOGRAM_STEP* merged_histogram;
    
    // Get partition statistics
    partition_stats = get_partition_statistics(partition);
    
    if (partition_stats == NULL) {
        // Create statistics for partition
        partition_stats = create_partition_statistics(partition);
    }
    
    // Merge histograms
    merged_histogram = merge_histograms(stats->histogram, stats->steps,
                                       partition_stats->histogram, 
                                       partition_stats->steps);
    
    // Update global statistics
    stats->histogram = merged_histogram;
    stats->rows += partition_stats->rows;
    stats->last_updated = GETDATE();
}
```

## Cost Model

### SQL Server Query Costing
```c
// Operator cost structure
typedef struct OPERATOR_COST {
    double  cpu_cost;           // CPU cost
    double  io_cost;            // I/O cost
    double  memory_cost;        // Memory grant required
    double  startup_cost;       // Startup cost
    double  total_cost;         // Total cost
    double  row_count;          // Estimated row count
    double  row_size;           // Average row size
} OPERATOR_COST;

// Cost model parameters
typedef struct COST_PARAMS {
    double  cpu_operator_cost;      // 0.0001 ms
    double  cpu_tuple_cost;         // 0.0001 ms
    double  random_io_cost;         // 0.003 ms
    double  sequential_io_cost;     // 0.0007 ms
    double  memory_grant_cost;      // Cost per KB
    double  parallelism_threshold;  // 5.0
} COST_PARAMS;

// Index seek cost
OPERATOR_COST
calculate_index_seek_cost(
    INDEX*          index,
    RANGE*          seek_range,
    double          selectivity)
{
    OPERATOR_COST cost;
    STATISTICS* stats = get_index_statistics(index);
    
    // Calculate rows returned
    cost.row_count = stats->rows * selectivity;
    cost.row_size = index->key_size + index->include_size;
    
    // Calculate I/O cost
    double index_depth = ceil(log2(stats->rows / index->keys_per_page));
    double leaf_pages = cost.row_count / index->keys_per_page;
    
    // Startup cost - traverse to first leaf
    cost.startup_cost = index_depth * random_io_cost;
    
    // Scan cost - read leaf pages
    if (is_covering_index(index, query)) {
        // Covering index - no bookmark lookups
        cost.io_cost = leaf_pages * sequential_io_cost;
    } else {
        // Need bookmark lookups
        cost.io_cost = leaf_pages * sequential_io_cost +
                      cost.row_count * random_io_cost;
    }
    
    // CPU cost
    cost.cpu_cost = cost.row_count * (cpu_tuple_cost + cpu_operator_cost);
    
    // Total cost
    cost.total_cost = cost.startup_cost + cost.io_cost + cost.cpu_cost;
    
    // Memory grant for sort
    if (needs_sort(query)) {
        cost.memory_cost = cost.row_count * cost.row_size / 1024.0;
    }
    
    return cost;
}

// Hash join cost
OPERATOR_COST
calculate_hash_join_cost(
    OPERATOR_COST*  build_cost,
    OPERATOR_COST*  probe_cost,
    double          join_selectivity)
{
    OPERATOR_COST cost;
    
    // Build phase cost
    double hash_build_cost = build_cost->total_cost +
                            build_cost->row_count * cpu_operator_cost * 2;
    
    // Memory grant for hash table
    double hash_table_size = build_cost->row_count * 
                           (build_cost->row_size + HASH_OVERHEAD);
    cost.memory_cost = hash_table_size / 1024.0;
    
    // Check if spilling needed
    if (hash_table_size > available_memory) {
        // Add spill cost
        double spill_pages = hash_table_size / PAGE_SIZE;
        hash_build_cost += spill_pages * sequential_io_cost * 2; // Write + read
    }
    
    // Probe phase cost
    double hash_probe_cost = probe_cost->total_cost +
                           probe_cost->row_count * cpu_operator_cost;
    
    // Output cardinality
    cost.row_count = build_cost->row_count * probe_cost->row_count * 
                    join_selectivity;
    cost.row_size = build_cost->row_size + probe_cost->row_size;
    
    // Total cost
    cost.startup_cost = hash_build_cost;
    cost.total_cost = hash_build_cost + hash_probe_cost;
    cost.cpu_cost = (build_cost->row_count + probe_cost->row_count) * 
                   cpu_operator_cost;
    cost.io_cost = build_cost->io_cost + probe_cost->io_cost;
    
    return cost;
}

// Parallel query cost adjustment
OPERATOR_COST
calculate_parallel_cost(
    OPERATOR_COST*  serial_cost,
    int            degree_of_parallelism)
{
    OPERATOR_COST parallel_cost = *serial_cost;
    
    // Add parallel startup overhead
    parallel_cost.startup_cost += PARALLEL_STARTUP_COST;
    
    // Divide work among threads (not perfectly linear)
    double efficiency = 1.0 - (degree_of_parallelism - 1) * 0.1; // 10% overhead per thread
    parallel_cost.cpu_cost /= (degree_of_parallelism * efficiency);
    
    // I/O might not parallelize well
    parallel_cost.io_cost /= sqrt(degree_of_parallelism);
    
    // Add exchange operator cost
    double exchange_cost = parallel_cost.row_count * EXCHANGE_COST_PER_ROW;
    parallel_cost.total_cost = parallel_cost.startup_cost +
                              parallel_cost.cpu_cost +
                              parallel_cost.io_cost +
                              exchange_cost;
    
    return parallel_cost;
}
```

## Join Algorithms

### SQL Server Join Implementation
```c
// Join algorithms
typedef enum {
    JOIN_NESTED_LOOP,
    JOIN_MERGE,
    JOIN_HASH,
    JOIN_ADAPTIVE  // SQL Server 2017+
} JOIN_ALGORITHM;

// Adaptive join (SQL Server 2017+)
typedef struct ADAPTIVE_JOIN {
    JOIN_ALGORITHM  initial_algorithm;  // Initially chosen algorithm
    JOIN_ALGORITHM  runtime_algorithm;  // Algorithm chosen at runtime
    double          threshold_rows;     // Threshold for switching
    OPERATOR*       hash_build_operator; // Hash build branch
    OPERATOR*       nested_loop_operator; // Nested loop branch
} ADAPTIVE_JOIN;

// Batch mode hash join
typedef struct BATCH_HASH_JOIN {
    HASH_TABLE*     hash_table;        // In-memory hash table
    BATCH*          build_batches;     // Build side batches
    BATCH*          probe_batch;       // Current probe batch
    int             batch_size;        // Rows per batch (typically 900)
    bool            batch_mode;        // Using batch mode?
} BATCH_HASH_JOIN;

// Execute batch mode hash join
BATCH*
execute_batch_hash_join(
    BATCH_HASH_JOIN*    join,
    EXECUTION_CONTEXT*  context)
{
    BATCH* output_batch = allocate_batch(join->batch_size);
    
    // Build phase (if not already built)
    if (join->hash_table == NULL) {
        join->hash_table = create_hash_table();
        
        // Process build side in batches
        while ((batch = get_next_batch(join->build_operator)) != NULL) {
            for (int i = 0; i < batch->row_count; i++) {
                HASH_KEY key = compute_hash_key(batch, i, join->hash_columns);
                insert_into_hash_table(join->hash_table, key, batch, i);
            }
        }
    }
    
    // Probe phase
    join->probe_batch = get_next_batch(join->probe_operator);
    if (join->probe_batch == NULL) {
        return NULL;  // No more input
    }
    
    // Process probe batch using SIMD
    for (int i = 0; i < join->probe_batch->row_count; i += VECTOR_SIZE) {
        // Compute hash for multiple rows at once
        __m256i hash_vector = compute_hash_vector(join->probe_batch, i);
        
        // Probe hash table
        for (int j = 0; j < VECTOR_SIZE && i + j < join->probe_batch->row_count; j++) {
            HASH_KEY key = extract_hash(hash_vector, j);
            HASH_BUCKET* bucket = lookup_hash_table(join->hash_table, key);
            
            // Check all entries in bucket
            for (HASH_ENTRY* entry = bucket->first; entry; entry = entry->next) {
                if (evaluate_join_predicate(join->probe_batch, i + j,
                                          entry->batch, entry->row_index)) {
                    // Add to output batch
                    add_joined_row(output_batch,
                                 join->probe_batch, i + j,
                                 entry->batch, entry->row_index);
                }
            }
        }
    }
    
    return output_batch;
}

// Merge join with optimizations
RESULT_SET*
execute_merge_join(
    MERGE_JOIN*         join,
    EXECUTION_CONTEXT*  context)
{
    RESULT_SET* result = create_result_set();
    
    // Ensure inputs are sorted
    if (!is_sorted(join->left_input)) {
        join->left_input = sort_operator(join->left_input, join->left_keys);
    }
    if (!is_sorted(join->right_input)) {
        join->right_input = sort_operator(join->right_input, join->right_keys);
    }
    
    // Many-to-many merge join handling
    ROW* left_row = get_next_row(join->left_input);
    ROW* right_row = get_next_row(join->right_input);
    ROW* right_mark = NULL;  // Mark position for duplicates
    
    while (left_row && right_row) {
        int cmp = compare_join_keys(left_row, right_row, join->join_keys);
        
        if (cmp < 0) {
            // Left < Right
            if (join->join_type == LEFT_OUTER || join->join_type == FULL_OUTER) {
                // Emit unmatched left row
                emit_row(result, left_row, NULL);
            }
            left_row = get_next_row(join->left_input);
        } else if (cmp > 0) {
            // Left > Right
            if (join->join_type == RIGHT_OUTER || join->join_type == FULL_OUTER) {
                // Emit unmatched right row
                emit_row(result, NULL, right_row);
            }
            right_row = get_next_row(join->right_input);
            right_mark = NULL;
        } else {
            // Match found - handle duplicates
            if (right_mark == NULL) {
                right_mark = right_row;  // Mark start of duplicate group
            }
            
            // Join all matching rows
            ROW* left_group_start = left_row;
            
            do {
                ROW* right_scan = right_mark;
                
                do {
                    if (evaluate_join_predicate(left_row, right_scan)) {
                        emit_row(result, left_row, right_scan);
                    }
                    
                    right_scan = get_next_row(join->right_input);
                } while (right_scan && 
                        compare_join_keys(left_row, right_scan, join->join_keys) == 0);
                
                // Reset right side for next left row
                reset_to_position(join->right_input, right_mark);
                
                left_row = get_next_row(join->left_input);
            } while (left_row && 
                    compare_join_keys(left_row, right_mark, join->join_keys) == 0);
            
            // Advance right side past duplicate group
            right_row = right_scan;
        }
    }
    
    // Handle remaining unmatched rows for outer joins
    while (left_row && (join->join_type == LEFT_OUTER || join->join_type == FULL_OUTER)) {
        emit_row(result, left_row, NULL);
        left_row = get_next_row(join->left_input);
    }
    
    while (right_row && (join->join_type == RIGHT_OUTER || join->join_type == FULL_OUTER)) {
        emit_row(result, NULL, right_row);
        right_row = get_next_row(join->right_input);
    }
    
    return result;
}