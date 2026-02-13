# OLAP Tier Specification

## Overview

The OLAP (Online Analytical Processing) Tier provides long-term storage and analytical query capabilities. It receives consolidated, validated data from the Ingestion Layer and optimizes it for complex analytical workloads including aggregations, joins, and time-series analysis.

## Architecture

### OLAP Engine Components

```
┌──────────────────────────────────────────────────┐
│  OLAP Shard                                      │
│                                                  │
│  ┌────────────────────────────────────────────┐ │
│  │  Ingestion Interface                       │ │
│  │  - Receive batches from Ingestion Layer   │ │
│  └──────────────┬─────────────────────────────┘ │
│                 │                                │
│  ┌──────────────▼─────────────────────────────┐ │
│  │  Partition Manager                         │ │
│  │  - Time-based partitioning                 │ │
│  │  - Partition pruning                       │ │
│  └──────────────┬─────────────────────────────┘ │
│                 │                                │
│  ┌──────────────▼─────────────────────────────┐ │
│  │  Columnar Storage Engine                   │ │
│  │  - Parquet/ORC format                      │ │
│  │  - Compression (Zstd, LZ4)                 │ │
│  │  - Column pruning                          │ │
│  └──────────────┬─────────────────────────────┘ │
│                 │                                │
│  ┌──────────────▼─────────────────────────────┐ │
│  │  Query Processor                           │ │
│  │  - Vectorized execution                    │ │
│  │  - Parallel processing                     │ │
│  │  - Materialized views                      │ │
│  └────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────┘
```

## Storage Format

### Columnar Layout

Data is stored in columnar format for optimal compression and query performance:

```
Traditional Row Format (OLTP):
[id=1, name="Alice", age=30, city="NYC"]
[id=2, name="Bob", age=25, city="SF"]
[id=3, name="Carol", age=35, city="NYC"]

Columnar Format (OLAP):
Column: id       [1, 2, 3]
Column: name     ["Alice", "Bob", "Carol"]
Column: age      [30, 25, 35]
Column: city     ["NYC", "SF", "NYC"]
```

**Benefits**:
- Better compression (similar values grouped)
- Column pruning (only read needed columns)
- Vectorized processing (SIMD operations)
- Cache efficiency (sequential access)

### File Format

```c
// Parquet-based storage format
struct ColumnChunk {
    char column_name[128];
    DataType type;
    CompressionCodec compression;
    
    // Statistics for query optimization
    ColumnStatistics stats;
    
    // Encodings
    Encoding encoding;  // Dictionary, RLE, Delta, etc.
    
    // Data pages
    List<DataPage*> pages;
    
    // Dictionary (if dictionary encoding used)
    Dictionary* dictionary;
};

struct DataPage {
    uint32_t num_values;
    bytes compressed_data;
    uint32_t uncompressed_size;
    
    // Page-level statistics
    PageStatistics stats;
};

struct ColumnStatistics {
    uint64_t num_values;
    uint64_t num_nulls;
    
    // Min/max (for numeric types)
    bytes min_value;
    bytes max_value;
    
    // Distinct count (approximate)
    uint64_t distinct_count;
    
    // Bloom filter for membership tests
    BloomFilter bloom_filter;
};
```

### Compression Strategies

```c
enum CompressionCodec {
    COMPRESSION_NONE = 0,
    COMPRESSION_SNAPPY,      // Fast, moderate compression
    COMPRESSION_ZSTD,        // Balanced speed/ratio
    COMPRESSION_LZ4,         // Very fast, lower compression
    COMPRESSION_GZIP,        // Slower, higher compression
    COMPRESSION_BROTLI,      // Highest compression, slowest
};

// Choose compression based on column type and access pattern
CompressionCodec select_compression(Column* col) {
    // High-cardinality string columns: use Zstd or Brotli
    if (col->type == TYPE_STRING && col->distinct_ratio > 0.8) {
        return is_hot_partition() ? COMPRESSION_ZSTD : COMPRESSION_BROTLI;
    }
    
    // Low-cardinality columns: use dictionary encoding + Snappy
    if (col->distinct_ratio < 0.1) {
        return COMPRESSION_SNAPPY;  // Dictionary already compressed
    }
    
    // Numeric columns with good locality: use Delta + LZ4
    if (is_numeric(col->type) && has_good_locality(col)) {
        return COMPRESSION_LZ4;
    }
    
    // Default: Zstd (good balance)
    return COMPRESSION_ZSTD;
}
```

### Encoding Strategies

```c
enum Encoding {
    ENCODING_PLAIN,           // No encoding
    ENCODING_DICTIONARY,      // Dictionary encoding
    ENCODING_RLE,            // Run-length encoding
    ENCODING_DELTA,          // Delta encoding (for sequences)
    ENCODING_DELTA_BINARY,   // Binary delta encoding
    ENCODING_BYTE_STREAM_SPLIT, // Split bytes for better compression
};

// Example: Dictionary encoding for low-cardinality columns
struct DictionaryEncoding {
    // Dictionary: unique values
    List<bytes> dictionary;
    
    // Indexes: reference to dictionary
    List<uint32_t> indexes;
    
    // Benefits: 
    // - "USA" appears 1M times → store once, reference 1M times
    // - Indexes compress well with RLE
};

// Example: Delta encoding for sorted numeric sequences
struct DeltaEncoding {
    int64_t base_value;           // First value
    List<int32_t> deltas;         // Differences from previous
    
    // Example:
    // Original: [1000, 1001, 1002, 1005, 1006]
    // Encoded:  base=1000, deltas=[1, 1, 3, 1]
    // Savings: 60% size reduction
};
```

## Partitioning Strategy

### Time-Based Partitioning

```c
// Partition configuration
struct PartitionConfig {
    PartitionScheme scheme;
    uint32_t partition_size_days;    // e.g., 1, 7, 30
    uint32_t retention_days;         // How long to keep partitions
    bool enable_partition_pruning;
    bool enable_compaction;
};

enum PartitionScheme {
    PARTITION_BY_DAY,       // Daily partitions
    PARTITION_BY_WEEK,      // Weekly partitions
    PARTITION_BY_MONTH,     // Monthly partitions
    PARTITION_BY_QUARTER,   // Quarterly partitions
    PARTITION_BY_YEAR,      // Yearly partitions
};

// Partition structure
struct Partition {
    char partition_key[128];      // e.g., "2024-01-15"
    uint64_t start_timestamp;
    uint64_t end_timestamp;
    
    // Storage files
    List<ColumnFile*> column_files;
    
    // Metadata
    uint64_t num_rows;
    uint64_t total_bytes;
    uint64_t compressed_bytes;
    
    // State
    PartitionState state;
    uint64_t created_at;
    uint64_t last_modified;
};

enum PartitionState {
    PARTITION_ACTIVE,       // Currently receiving writes
    PARTITION_COMPACTING,   // Being compacted
    PARTITION_IMMUTABLE,    // Read-only, fully compacted
    PARTITION_ARCHIVED,     // Moved to cold storage
    PARTITION_EXPIRED,      // Ready for deletion
};
```

### Partition Pruning

```c
// Determine which partitions to scan for query
List<Partition*> select_partitions(Query* query) {
    TimeRange query_range = extract_time_range(query);
    
    List<Partition*> selected;
    
    for (Partition* partition : all_partitions) {
        // Skip if partition is outside query range
        if (partition->end_timestamp < query_range.start ||
            partition->start_timestamp > query_range.end) {
            continue;
        }
        
        // Check if partition statistics match query predicates
        if (!partition_matches_predicates(partition, query->predicates)) {
            continue;
        }
        
        selected.push_back(partition);
    }
    
    log_info("Query scans %lu / %lu partitions (%.1f%% pruned)",
             selected.size(),
             all_partitions.size(),
             (1.0 - (double)selected.size() / all_partitions.size()) * 100);
    
    return selected;
}

// Check if partition matches query predicates using statistics
bool partition_matches_predicates(Partition* partition, List<Predicate*> predicates) {
    for (Predicate* pred : predicates) {
        ColumnStatistics* stats = partition->get_column_stats(pred->column_name);
        
        if (!stats) {
            continue;  // No stats, can't prune
        }
        
        // Example: WHERE age > 30
        if (pred->operator == OP_GREATER_THAN) {
            if (stats->max_value <= pred->value) {
                return false;  // All values in partition <= 30, skip it
            }
        }
        
        // Example: WHERE city = 'NYC'
        if (pred->operator == OP_EQUALS) {
            if (stats->bloom_filter && !stats->bloom_filter.contains(pred->value)) {
                return false;  // Value definitely not in partition
            }
        }
    }
    
    return true;  // Partition might contain matching rows
}
```

## Sharding

### Sharding Strategies

```c
enum ShardingStrategy {
    SHARD_BY_TIME,          // Time-based sharding (most common)
    SHARD_BY_HASH,          // Hash-based sharding
    SHARD_BY_RANGE,         // Range-based sharding
    SHARD_BY_GEOGRAPHY,     // Geographic sharding
    SHARD_BY_TENANT,        // Multi-tenant sharding
};

// Time-based sharding example
struct TimeBasedSharding {
    uint32_t num_shards;
    uint32_t time_range_per_shard_days;
    
    // Shard assignment
    uint32_t get_shard_id(uint64_t timestamp) {
        uint32_t day = timestamp / (24 * 3600 * 1000000000ULL);
        uint32_t shard_day = day / time_range_per_shard_days;
        return shard_day % num_shards;
    }
};

// Example configuration
Shards:
  Shard 0: 2024-01-01 to 2024-03-31 (Q1)
  Shard 1: 2024-04-01 to 2024-06-30 (Q2)
  Shard 2: 2024-07-01 to 2024-09-30 (Q3)
  Shard 3: 2024-10-01 to 2024-12-31 (Q4)
```

### Geographic Sharding

For multi-region deployments:

```c
// Geographic sharding configuration
struct GeographicSharding {
    Map<string, ShardID> region_to_shard;
    
    // Example:
    // region_to_shard["US"] = 0
    // region_to_shard["EU"] = 1
    // region_to_shard["APAC"] = 2
};

// Query routing with geographic awareness
ShardID route_query_by_geography(Query* query) {
    string user_region = extract_region(query);
    
    if (region_to_shard.contains(user_region)) {
        return region_to_shard[user_region];
    }
    
    // Default to closest shard
    return select_closest_shard(query->origin_ip);
}
```

## Query Processing

### Vectorized Execution

```c
// Process data in batches using SIMD
struct VectorBatch {
    uint32_t num_rows;
    Column* columns[MAX_COLUMNS];
};

// Example: Vectorized filter operation
VectorBatch* filter_vectorized(VectorBatch* input, Predicate* pred) {
    // Assume AVX2 SIMD (256-bit vectors = 8 x 32-bit ints)
    const uint32_t VECTOR_SIZE = 8;
    
    VectorBatch* output = allocate_vector_batch();
    uint32_t out_idx = 0;
    
    Column* col = input->columns[pred->column_index];
    int32_t* values = (int32_t*)col->data;
    int32_t threshold = pred->value;
    
    // Process 8 values at a time with SIMD
    for (uint32_t i = 0; i < input->num_rows; i += VECTOR_SIZE) {
        // Load 8 values
        __m256i vec = _mm256_loadu_si256((__m256i*)&values[i]);
        
        // Compare: create mask of matching values
        __m256i threshold_vec = _mm256_set1_epi32(threshold);
        __m256i mask = _mm256_cmpgt_epi32(vec, threshold_vec);
        
        // Extract matching values
        int bitmask = _mm256_movemask_ps((__m256)mask);
        
        for (int j = 0; j < VECTOR_SIZE; j++) {
            if (bitmask & (1 << j)) {
                // Copy matching row to output
                copy_row(output, out_idx++, input, i + j);
            }
        }
    }
    
    output->num_rows = out_idx;
    return output;
}
```

### Parallel Query Execution

```c
// Parallel scan of partitions
struct ParallelScanPlan {
    List<Partition*> partitions;
    uint32_t num_threads;
    Predicate* predicates;
    Projection* projection;
};

// Execute parallel scan
ResultSet* execute_parallel_scan(ParallelScanPlan* plan) {
    ThreadPool* pool = create_thread_pool(plan->num_threads);
    
    // Submit one task per partition
    List<Future<PartitionResult*>> futures;
    
    for (Partition* partition : plan->partitions) {
        futures.push_back(pool->submit([partition, plan]() {
            return scan_partition(partition, plan->predicates, plan->projection);
        }));
    }
    
    // Merge results from all partitions
    ResultSet* final_result = create_result_set();
    
    for (Future<PartitionResult*>& future : futures) {
        PartitionResult* partial = future.get();
        merge_results(final_result, partial);
    }
    
    return final_result;
}
```

### Aggregation Optimization

```c
// Multi-level aggregation for GROUP BY queries
struct AggregationPlan {
    // Stage 1: Local aggregation per partition
    List<LocalAggregation*> local_aggs;
    
    // Stage 2: Global aggregation across partitions
    GlobalAggregation* global_agg;
};

// Example: SUM aggregation
ResultSet* execute_sum_aggregation(Query* query) {
    // Stage 1: Parallel sum per partition
    List<Future<PartialSum*>> partial_sums;
    
    for (Partition* partition : selected_partitions) {
        partial_sums.push_back(pool->submit([partition, query]() {
            return compute_local_sum(partition, query->group_by_column);
        }));
    }
    
    // Stage 2: Merge partial sums
    Map<GroupKey, int64_t> final_sums;
    
    for (Future<PartialSum*>& future : partial_sums) {
        PartialSum* partial = future.get();
        
        for (auto& [key, sum] : partial->sums) {
            final_sums[key] += sum;
        }
    }
    
    return convert_to_result_set(final_sums);
}
```

## Materialized Views

### Pre-Aggregated Views

```c
// Materialized view definition
struct MaterializedView {
    char name[128];
    char source_table[128];
    
    // Aggregation definition
    List<AggregateFunction*> aggregates;
    List<char*> group_by_columns;
    
    // Refresh strategy
    RefreshStrategy refresh_strategy;
    uint32_t refresh_interval_seconds;
    
    // Storage
    List<Partition*> materialized_partitions;
    
    // Metadata
    uint64_t last_refresh_timestamp;
    uint64_t rows_materialized;
};

enum RefreshStrategy {
    REFRESH_MANUAL,          // Manual refresh only
    REFRESH_SCHEDULED,       // Periodic refresh
    REFRESH_INCREMENTAL,     // Incremental updates
    REFRESH_ON_COMMIT,       // Update on every commit (expensive)
};

// Example: Daily sales summary view
MaterializedView* create_daily_sales_view() {
    MaterializedView* view = allocate_view();
    
    strcpy(view->name, "daily_sales_summary");
    strcpy(view->source_table, "orders");
    
    // SELECT date, SUM(amount), COUNT(*), AVG(amount)
    // FROM orders
    // GROUP BY date
    view->aggregates.push_back(create_sum_agg("amount"));
    view->aggregates.push_back(create_count_agg());
    view->aggregates.push_back(create_avg_agg("amount"));
    
    view->group_by_columns.push_back("date");
    
    view->refresh_strategy = REFRESH_SCHEDULED;
    view->refresh_interval_seconds = 3600;  // Hourly
    
    return view;
}
```

### Incremental View Maintenance

```c
// Incrementally update materialized view with new data
void refresh_view_incremental(MaterializedView* view, List<RowVersion*> new_rows) {
    // Group new rows by materialized view key
    Map<GroupKey, List<RowVersion*>> groups;
    
    for (RowVersion* row : new_rows) {
        GroupKey key = extract_group_key(row, view->group_by_columns);
        groups[key].push_back(row);
    }
    
    // Update aggregates for each group
    for (auto& [key, rows] : groups) {
        for (AggregateFunction* agg : view->aggregates) {
            // Read current aggregate value
            AggregateValue current = read_aggregate(view, key, agg);
            
            // Compute delta from new rows
            AggregateValue delta = compute_aggregate_delta(agg, rows);
            
            // Update aggregate
            AggregateValue updated = merge_aggregate(current, delta);
            write_aggregate(view, key, agg, updated);
        }
    }
    
    view->last_refresh_timestamp = now();
}
```

## Indexing

### Zone Maps

Zone maps store min/max values per data block for fast pruning:

```c
struct ZoneMap {
    char column_name[128];
    
    // Per-block statistics
    struct Block {
        uint32_t block_id;
        bytes min_value;
        bytes max_value;
        uint32_t num_rows;
    };
    
    List<Block> blocks;
};

// Use zone map for query optimization
List<Block*> prune_blocks_with_zone_map(ZoneMap* zm, Predicate* pred) {
    List<Block*> matching_blocks;
    
    for (Block& block : zm->blocks) {
        // Example: WHERE age > 30
        if (pred->operator == OP_GREATER_THAN) {
            if (block.max_value <= pred->value) {
                continue;  // All values in block <= 30, skip
            }
        }
        
        matching_blocks.push_back(&block);
    }
    
    return matching_blocks;
}
```

### Bitmap Indexes

For low-cardinality columns (e.g., status, country):

```c
struct BitmapIndex {
    char column_name[128];
    
    // Map: value → bitmap of matching rows
    Map<bytes, RoaringBitmap*> value_to_bitmap;
};

// Example: WHERE country = 'USA'
RoaringBitmap* lookup_bitmap(BitmapIndex* idx, bytes value) {
    return idx->value_to_bitmap.get(value);
}

// Example: WHERE country IN ('USA', 'Canada')
RoaringBitmap* lookup_multi_value(BitmapIndex* idx, List<bytes> values) {
    RoaringBitmap* result = create_bitmap();
    
    for (bytes value : values) {
        RoaringBitmap* bitmap = idx->value_to_bitmap.get(value);
        if (bitmap) {
            result->or(*bitmap);  // Union bitmaps
        }
    }
    
    return result;
}
```

### Inverted Index

For full-text search on text columns:

```c
struct InvertedIndex {
    char column_name[128];
    
    // Map: term → posting list (document IDs + positions)
    Map<string, PostingList*> term_to_postings;
};

struct PostingList {
    List<Posting> postings;
};

struct Posting {
    uint64_t document_id;      // Row ID
    List<uint32_t> positions;  // Positions of term in document
};

// Example: Search for "database performance"
List<uint64_t> search_inverted_index(InvertedIndex* idx, string query) {
    List<string> terms = tokenize(query);  // ["database", "performance"]
    
    // Get posting lists for each term
    List<PostingList*> postings;
    for (string term : terms) {
        postings.push_back(idx->term_to_postings.get(term));
    }
    
    // Intersect posting lists (rows containing ALL terms)
    Set<uint64_t> matching_docs;
    intersect_posting_lists(postings, &matching_docs);
    
    return matching_docs.to_list();
}
```

## Compaction

### Partition Compaction

Over time, partitions accumulate many small files. Compaction merges them:

```c
// Compaction configuration
struct CompactionConfig {
    uint32_t min_files_to_compact;      // e.g., 10
    uint32_t target_file_size_mb;       // e.g., 128 MB
    uint32_t max_concurrent_compactions; // e.g., 4
    CompactionStrategy strategy;
};

enum CompactionStrategy {
    COMPACT_SIZE_TIERED,    // Merge files of similar size
    COMPACT_LEVELED,        // LSM-tree style leveled compaction
    COMPACT_TIME_WINDOW,    // Compact by time window
};

// Execute compaction
void compact_partition(Partition* partition, CompactionConfig* config) {
    // 1. Select files to compact
    List<ColumnFile*> input_files = select_compaction_files(
        partition,
        config->strategy
    );
    
    if (input_files.size() < config->min_files_to_compact) {
        return;  // Not enough files to warrant compaction
    }
    
    log_info("Compacting partition %s: %lu files → 1 file",
             partition->partition_key,
             input_files.size());
    
    // 2. Merge files
    ColumnFile* output_file = merge_column_files(
        input_files,
        config->target_file_size_mb
    );
    
    // 3. Replace old files with new file (atomic operation)
    atomic_replace_files(partition, input_files, output_file);
    
    // 4. Delete old files
    delete_files(input_files);
    
    log_info("Compaction complete: saved %.1f MB",
             (total_size(input_files) - output_file->size) / 1024.0 / 1024.0);
}
```

### Sort Order Optimization

Reorder data during compaction for better query performance:

```c
// Reorder rows during compaction
ColumnFile* merge_with_sort(List<ColumnFile*> input_files, SortOrder* sort_order) {
    // 1. Read all rows from input files
    List<Row*> all_rows;
    for (ColumnFile* file : input_files) {
        all_rows.append(read_all_rows(file));
    }
    
    // 2. Sort rows according to sort order
    sort_rows(all_rows, sort_order);
    
    // 3. Write sorted rows to new file
    ColumnFile* output = create_column_file();
    write_rows(output, all_rows);
    
    // Benefits:
    // - Range queries on sorted column are faster
    // - Better compression (similar values adjacent)
    // - More effective zone maps
    
    return output;
}
```

## Data Lifecycle Management

### Tiered Storage

Move older data to cheaper storage tiers:

```c
enum StorageTier {
    TIER_HOT,       // Fast SSD/NVMe (recent data, frequently accessed)
    TIER_WARM,      // Standard SSD (older data, occasionally accessed)
    TIER_COLD,      // HDD or object storage (archive data, rarely accessed)
    TIER_FROZEN,    // Compressed archive (compliance data, never accessed)
};

struct TieringPolicy {
    uint32_t hot_to_warm_days;      // e.g., 30 days
    uint32_t warm_to_cold_days;     // e.g., 90 days
    uint32_t cold_to_frozen_days;   // e.g., 365 days
    uint32_t frozen_retention_days; // e.g., 2555 days (7 years)
};

// Automatic tiering based on age and access patterns
void apply_tiering_policy(Partition* partition, TieringPolicy* policy) {
    uint32_t age_days = (now() - partition->created_at) / (24 * 3600);
    
    StorageTier current_tier = partition->storage_tier;
    StorageTier target_tier = current_tier;
    
    // Determine target tier based on age
    if (age_days >= policy->cold_to_frozen_days) {
        target_tier = TIER_FROZEN;
    } else if (age_days >= policy->warm_to_cold_days) {
        target_tier = TIER_COLD;
    } else if (age_days >= policy->hot_to_warm_days) {
        target_tier = TIER_WARM;
    } else {
        target_tier = TIER_HOT;
    }
    
    // Move to target tier if different
    if (target_tier != current_tier) {
        log_info("Moving partition %s from %s to %s",
                 partition->partition_key,
                 tier_name(current_tier),
                 tier_name(target_tier));
        
        move_to_tier(partition, target_tier);
    }
}
```

### Data Retention and Deletion

```c
// Retention policy
struct RetentionPolicy {
    uint32_t retention_days;
    bool enable_soft_delete;       // Mark as deleted vs hard delete
    uint32_t soft_delete_grace_period_days;
};

// Apply retention policy
void apply_retention_policy(RetentionPolicy* policy) {
    uint32_t cutoff_days = policy->retention_days;
    uint64_t cutoff_timestamp = now() - (cutoff_days * 24 * 3600 * 1000000000ULL);
    
    for (Partition* partition : all_partitions) {
        if (partition->end_timestamp < cutoff_timestamp) {
            if (policy->enable_soft_delete) {
                // Soft delete: mark partition as expired
                partition->state = PARTITION_EXPIRED;
                partition->expiry_timestamp = now();
            } else {
                // Hard delete: immediately remove
                delete_partition(partition);
            }
        }
    }
}

// Purge soft-deleted partitions after grace period
void purge_expired_partitions(RetentionPolicy* policy) {
    uint64_t grace_period_ns = policy->soft_delete_grace_period_days * 24 * 3600 * 1000000000ULL;
    
    for (Partition* partition : all_partitions) {
        if (partition->state == PARTITION_EXPIRED) {
            uint64_t time_since_expiry = now() - partition->expiry_timestamp;
            
            if (time_since_expiry >= grace_period_ns) {
                log_info("Purging expired partition: %s", partition->partition_key);
                delete_partition(partition);
            }
        }
    }
}
```

## Query Optimization

### Statistics-Based Optimization

```c
// Table statistics for cost-based optimization
struct TableStatistics {
    uint64_t num_rows;
    uint64_t num_partitions;
    uint64_t total_bytes;
    
    // Per-column statistics
    Map<string, ColumnStatistics> column_stats;
};

// Cost estimation for query plan
double estimate_query_cost(QueryPlan* plan, TableStatistics* stats) {
    double cost = 0.0;
    
    // Cost of scanning
    uint64_t rows_to_scan = estimate_rows_scanned(plan, stats);
    cost += rows_to_scan * SCAN_COST_PER_ROW;
    
    // Cost of filtering
    if (plan->has_filter) {
        double selectivity = estimate_selectivity(plan->predicates, stats);
        cost += rows_to_scan * selectivity * FILTER_COST_PER_ROW;
    }
    
    // Cost of sorting
    if (plan->has_sort) {
        uint64_t rows_to_sort = rows_to_scan * selectivity;
        cost += rows_to_sort * log2(rows_to_sort) * SORT_COST_PER_ROW;
    }
    
    // Cost of aggregation
    if (plan->has_aggregation) {
        cost += estimate_aggregation_cost(plan, rows_to_scan, selectivity);
    }
    
    return cost;
}
```

### Query Plan Caching

```c
// Cache compiled query plans
struct QueryPlanCache {
    // Key: SQL text hash
    // Value: Compiled query plan
    LRUCache<uint64_t, QueryPlan*> cache;
    
    uint64_t cache_hits;
    uint64_t cache_misses;
};

// Get cached plan or compile new one
QueryPlan* get_or_compile_plan(string sql_text, QueryPlanCache* cache) {
    uint64_t sql_hash = hash(sql_text);
    
    QueryPlan* plan = cache->cache.get(sql_hash);
    
    if (plan) {
        cache->cache_hits++;
        return plan;
    }
    
    // Cache miss - compile new plan
    cache->cache_misses++;
    
    plan = compile_query_plan(sql_text);
    cache->cache.put(sql_hash, plan);
    
    return plan;
}
```

## Performance Monitoring

### Key Metrics

```c
struct OLAPMetrics {
    // Query performance
    uint64_t queries_per_second;
    uint64_t p50_query_latency_ms;
    uint64_t p99_query_latency_ms;
    
    // Storage
    uint64_t total_rows;
    uint64_t total_bytes_uncompressed;
    uint64_t total_bytes_compressed;
    double compression_ratio;
    
    // Partitions
    uint32_t num_partitions;
    uint32_t num_hot_partitions;
    uint32_t num_cold_partitions;
    
    // I/O
    uint64_t bytes_read_per_second;
    uint64_t bytes_written_per_second;
    
    // Compaction
    uint32_t compactions_running;
    uint64_t bytes_compacted_per_hour;
    
    // Cache
    double cache_hit_rate;
    uint64_t cache_size_bytes;
};
```

### Dashboard Queries

```sql
-- Query performance by table
SELECT 
    table_name,
    COUNT(*) as query_count,
    AVG(execution_time_ms) as avg_time_ms,
    PERCENTILE_CONT(0.99) WITHIN GROUP (ORDER BY execution_time_ms) as p99_time_ms,
    AVG(rows_scanned) as avg_rows_scanned,
    AVG(rows_returned) as avg_rows_returned,
    AVG(rows_scanned::float / NULLIF(rows_returned, 0)) as scan_efficiency
FROM query_log
WHERE timestamp > NOW() - INTERVAL '1 hour'
GROUP BY table_name
ORDER BY p99_time_ms DESC;

-- Partition statistics
SELECT 
    partition_key,
    storage_tier,
    num_rows,
    total_bytes / 1024 / 1024 as size_mb,
    compressed_bytes / 1024 / 1024 as compressed_mb,
    total_bytes::float / compressed_bytes as compression_ratio,
    state,
    EXTRACT(EPOCH FROM NOW() - created_at) / 86400 as age_days
FROM partitions
ORDER BY age_days DESC
LIMIT 100;

-- Compaction effectiveness
SELECT 
    DATE_TRUNC('hour', timestamp) as hour,
    COUNT(*) as compactions_completed,
    SUM(input_files) as files_compacted,
    SUM(bytes_saved) / 1024 / 1024 / 1024 as gb_saved,
    AVG(duration_seconds) as avg_duration_seconds
FROM compaction_log
WHERE timestamp > NOW() - INTERVAL '7 days'
GROUP BY hour
ORDER BY hour DESC;
```

## Configuration Example

```yaml
olap:
  # Storage format
  storage:
    format: parquet
    compression: zstd
    compression_level: 3
    block_size_mb: 128
    
  # Partitioning
  partitioning:
    scheme: by_day
    partition_size_days: 1
    retention_days: 365
    enable_partition_pruning: true
    enable_compaction: true
    
  # Sharding
  sharding:
    strategy: by_time
    num_shards: 12
    time_range_per_shard_days: 30
    
  # Indexing
  indexes:
    enable_zone_maps: true
    enable_bitmap_indexes: true
    bitmap_cardinality_threshold: 100
    enable_bloom_filters: true
    
  # Query processing
  query:
    enable_vectorization: true
    max_parallel_workers: 16
    enable_query_plan_cache: true
    plan_cache_size: 10000
    
  # Compaction
  compaction:
    strategy: size_tiered
    min_files_to_compact: 10
    target_file_size_mb: 256
    max_concurrent_compactions: 4
    compaction_interval_seconds: 3600
    
  # Tiered storage
  tiering:
    enable: true
    hot_to_warm_days: 30
    warm_to_cold_days: 90
    cold_to_frozen_days: 365
    
  # Materialized views
  materialized_views:
    enable: true
    refresh_strategy: scheduled
    default_refresh_interval_seconds: 3600
