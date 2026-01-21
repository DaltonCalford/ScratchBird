# SBCLUSTER-06: Distributed Query Execution

## 1. Introduction

### 1.1 Purpose
This document specifies the distributed query execution architecture for ScratchBird clusters. Distributed queries enable read-only operations across multiple shards using a **coordinator/subplan model** that pushes computation to data, enforces security consistently, and merges results efficiently.

### 1.2 Scope
- Coordinator node responsibilities
- Subplan generation and distribution
- Push-compute-to-data execution model
- Result merging and aggregation strategies
- Security bundle enforcement gates
- Consistency models (shard-local snapshots)
- Query routing optimization
- Error handling and retry logic

### 1.3 Related Documents
- **SBCLUSTER-00**: Guiding Principles (push compute to data, no cross-shard transactions)
- **SBCLUSTER-04**: Security Bundle (enforcement gates)
- **SBCLUSTER-05**: Sharding (shard routing, shard-local MVCC)
- **SBCLUSTER-02**: Membership and Identity (node roles, COORDINATOR)
- **PARALLEL_EXECUTION_ARCHITECTURE.md**: Parallel execution model (Beta) - see `docs/specifications/query/`

### 1.4 Terminology
- **Coordinator**: Node that receives a query and orchestrates distributed execution
- **Subplan**: Query fragment executed on a single shard
- **Push Compute to Data**: Execute computation on shard nodes rather than transferring data
- **Result Merge**: Combining subplan results at the coordinator
- **Scatter-Gather**: Pattern where coordinator scatters subplans, then gathers results
- **Shard-Local Snapshot**: Each shard uses its own MVCC snapshot (no global snapshot)

---

## 2. Architecture Overview

### 2.1 Design Principles

1. **Push Compute to Data**: Minimize data transfer by executing filters, aggregations on shards
2. **Coordinator Orchestration**: Single coordinator node manages distributed execution
3. **Shard-Local Snapshots**: Each shard uses its own snapshot; no distributed transactions
4. **Security Enforcement Gates**: Verify identical security bundles before execution
5. **Read-Only Distributed Queries**: Only SELECT queries can span shards (no DML)
6. **Best-Effort Consistency**: Results reflect shard-local snapshots (eventual consistency)

### 2.2 Query Execution Flow

```
┌─────────────────────────────────────────────────────────────┐
│                       CLIENT                                │
│  SELECT region, COUNT(*), SUM(amount)                       │
│  FROM orders WHERE date >= '2026-01-01'                     │
│  GROUP BY region;                                           │
└─────────────────────┬───────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────┐
│                  COORDINATOR NODE                           │
│  1. Parse query                                             │
│  2. Check security bundle consistency                       │
│  3. Determine involved shards (all shards for this query)   │
│  4. Generate subplans                                       │
│  5. Distribute subplans to shard owners                     │
│  6. Collect results                                         │
│  7. Merge/aggregate                                         │
│  8. Return to client                                        │
└─────┬───────────────┬───────────────┬───────────────────────┘
      │               │               │
      ▼               ▼               ▼
┌─────────────┐ ┌─────────────┐ ┌─────────────┐
│  SHARD 1    │ │  SHARD 2    │ │  SHARD 3    │
│  Subplan:   │ │  Subplan:   │ │  Subplan:   │
│  SELECT     │ │  SELECT     │ │  SELECT     │
│  region,    │ │  region,    │ │  region,    │
│  COUNT(*),  │ │  COUNT(*),  │ │  COUNT(*),  │
│  SUM(amount)│ │  SUM(amount)│ │  SUM(amount)│
│  FROM       │ │  FROM       │ │  FROM       │
│  orders     │ │  orders     │ │  orders     │
│  WHERE ...  │ │  WHERE ...  │ │  WHERE ...  │
│  GROUP BY   │ │  GROUP BY   │ │  GROUP BY   │
│  region     │ │  region     │ │  region     │
└─────┬───────┘ └─────┬───────┘ └─────┬───────┘
      │               │               │
      └───────────────┴───────────────┘
                      │
                      ▼
              Merge Results:
              NORTH: 150 orders, $45,000
              SOUTH: 200 orders, $60,000
              WEST:  175 orders, $52,500
```

### 2.3 Coordinator Roles

Any node with the **COORDINATOR** role (SBCLUSTER-02) can coordinate queries:
- Parse and plan distributed queries
- Enforce security bundle consistency
- Generate and distribute subplans
- Merge results from shards
- Handle errors and retries

---

## 3. Data Structures

### 3.1 DistributedQuery

```cpp
struct DistributedQuery {
    UUID query_uuid;                     // Unique query identifier (v7)
    UUID coordinator_node_uuid;          // Node coordinating this query
    UUID session_uuid;                   // Client session

    string original_sql;                 // Original SQL query
    QueryPlan global_plan;               // Global query plan

    bytes32 security_bundle_hash;        // Required security bundle hash
    timestamp_t initiated_at;

    DistributedQueryType type;           // SINGLE_SHARD | ALL_SHARDS | MULTI_SHARD
    UUID[] involved_shard_uuids;         // Shards involved in this query
};

enum DistributedQueryType {
    SINGLE_SHARD,    // Query routed to exactly one shard (e.g., WHERE customer_id = ?)
    ALL_SHARDS,      // Query requires all shards (e.g., full table scan)
    MULTI_SHARD      // Query requires subset of shards (future optimization)
};
```

### 3.2 Subplan

```cpp
struct Subplan {
    UUID subplan_uuid;                   // Unique subplan identifier (v7)
    UUID parent_query_uuid;              // Parent distributed query
    UUID shard_uuid;                     // Target shard
    UUID target_node_uuid;               // Node to execute on (shard owner)

    string subplan_sql;                  // SQL to execute on shard
    QueryPlan local_plan;                // Shard-local query plan

    SubplanRole role;                    // MAP | REDUCE
    bytes32 security_bundle_hash;        // Must match coordinator's bundle

    timestamp_t created_at;
    timestamp_t dispatched_at;
    timestamp_t completed_at;
};

enum SubplanRole {
    MAP,         // Execute on shard, return partial results
    REDUCE       // Merge/aggregate results (usually at coordinator)
};
```

### 3.3 SubplanResult

```cpp
struct SubplanResult {
    UUID subplan_uuid;
    UUID shard_uuid;
    UUID executor_node_uuid;             // Node that executed subplan

    SubplanStatus status;                // SUCCESS | FAILED | TIMEOUT

    // For SUCCESS
    ResultSet result_set;                // Query results (rows/columns)
    uint64_t row_count;
    uint64_t execution_time_micros;

    // For FAILED
    ErrorCode error_code;
    string error_message;

    timestamp_t completed_at;
};

enum SubplanStatus {
    SUCCESS,
    FAILED,
    TIMEOUT
};
```

### 3.4 MergeStrategy

```cpp
enum MergeStrategy {
    UNION,           // UNION of results (deduplicate if UNION DISTINCT)
    UNION_ALL,       // UNION ALL (no deduplication)
    MERGE_SORT,      // Merge sorted results (for ORDER BY)
    AGGREGATE,       // Re-aggregate partial aggregations (SUM, COUNT, AVG)
    TOP_N            // Merge top-N results (for LIMIT queries)
};

struct MergeOperation {
    MergeStrategy strategy;

    // For MERGE_SORT
    UUID[] sort_column_uuids;            // Columns to sort by
    SortOrder[] sort_orders;             // ASC/DESC for each column

    // For AGGREGATE
    AggregateFunction[] aggregate_functions; // Functions to re-aggregate
    UUID[] group_by_column_uuids;        // Columns to group by

    // For TOP_N
    uint64_t limit;                      // LIMIT value
    uint64_t offset;                     // OFFSET value
};
```

---

## 4. Query Planning

### 4.1 Shard Determination

**Algorithm**: Determine which shards are involved in a query.

```cpp
vector<UUID> determine_involved_shards(
    const QueryPlan& plan,
    const ShardMap& shard_map
) {
    // Case 1: Query has shard key equality filter
    if (has_shard_key_filter(plan)) {
        ShardKeyValue shard_key_value = extract_shard_key_value(plan);
        UUID shard_uuid = route_to_shard(shard_map, shard_key_value);
        return {shard_uuid};  // Single-shard query
    }

    // Case 2: Query requires all shards
    vector<UUID> all_shard_uuids;
    for (const Shard& shard : shard_map.shards) {
        if (shard.state == ShardState::ACTIVE) {
            all_shard_uuids.push_back(shard.shard_uuid);
        }
    }
    return all_shard_uuids;
}
```

### 4.2 Subplan Generation

**Algorithm**: Generate subplans from global query plan.

```cpp
vector<Subplan> generate_subplans(
    const DistributedQuery& query,
    const QueryPlan& global_plan,
    const ShardMap& shard_map
) {
    vector<Subplan> subplans;

    for (const UUID& shard_uuid : query.involved_shard_uuids) {
        Subplan subplan;
        subplan.subplan_uuid = UUID::v7();
        subplan.parent_query_uuid = query.query_uuid;
        subplan.shard_uuid = shard_uuid;
        subplan.role = SubplanRole::MAP;
        subplan.security_bundle_hash = query.security_bundle_hash;

        // Find shard owner node
        const Shard* shard = find_shard(shard_map, shard_uuid);
        subplan.target_node_uuid = shard->primary_owner_node_uuid;

        // Generate shard-local SQL
        subplan.subplan_sql = generate_shard_local_sql(global_plan, shard_uuid);
        subplan.local_plan = optimize_for_shard(global_plan, shard_uuid);

        subplan.created_at = now();
        subplans.push_back(subplan);
    }

    return subplans;
}
```

### 4.3 Shard-Local SQL Generation

**Push filters and aggregations to shards**:

```cpp
string generate_shard_local_sql(const QueryPlan& global_plan, const UUID& shard_uuid) {
    // Example: Global query
    //   SELECT region, COUNT(*), SUM(amount)
    //   FROM orders
    //   WHERE date >= '2026-01-01'
    //   GROUP BY region

    // Shard-local subplan (same as global for simple aggregations)
    //   SELECT region, COUNT(*) as cnt, SUM(amount) as sum_amt
    //   FROM orders_shard_1
    //   WHERE date >= '2026-01-01'
    //   GROUP BY region

    // For more complex queries, this function rewrites:
    // - Table names → shard-local table references
    // - LIMIT/OFFSET → removed (applied at coordinator)
    // - ORDER BY → preserved for merge-sort optimization

    return rewrite_plan_for_shard(global_plan);
}
```

---

## 5. Execution Protocol

### 5.1 Pre-Execution Security Check

Before distributing subplans, coordinator validates security bundle consistency:

```cpp
Status check_security_bundle_consistency(
    const DistributedQuery& query,
    const vector<UUID>& shard_owner_node_uuids
) {
    bytes32 coordinator_bundle_hash = get_current_bundle_hash();

    // Verify coordinator's bundle matches query's required bundle
    if (coordinator_bundle_hash != query.security_bundle_hash) {
        return Status::SECURITY_BUNDLE_MISMATCH(
            "Coordinator's security bundle has changed since query was planned"
        );
    }

    // Query all shard owner nodes for their bundle hash
    for (const UUID& node_uuid : shard_owner_node_uuids) {
        BundleHashResponse resp = rpc_client.get_bundle_hash(node_uuid);

        if (resp.bundle_hash != coordinator_bundle_hash) {
            return Status::SECURITY_BUNDLE_MISMATCH(
                fmt::format("Node {} has bundle hash {}, coordinator has {}",
                    node_uuid.to_string(),
                    hex(resp.bundle_hash),
                    hex(coordinator_bundle_hash))
            );
        }
    }

    return Status::OK();
}
```

**Critical**: If security bundles don't match, query is **aborted** before execution.

### 5.2 Subplan Dispatch

Coordinator dispatches subplans to shard owner nodes:

```cpp
struct SubplanDispatchRequest {
    Subplan subplan;
    UUID coordinator_node_uuid;
    bytes32 security_bundle_hash;
    timeout_t execution_timeout;         // Max execution time (e.g., 30s)
};

struct SubplanDispatchResponse {
    UUID subplan_uuid;
    SubplanStatus status;
    ResultSet result_set;                // For SUCCESS
    ErrorCode error_code;                // For FAILED
    string error_message;                // For FAILED
};

// RPC call
SubplanDispatchResponse execute_subplan(const SubplanDispatchRequest& request);
```

### 5.3 Subplan Execution on Shard

Each shard executes its subplan in isolation:

```cpp
SubplanDispatchResponse execute_subplan_on_shard(const SubplanDispatchRequest& request) {
    SubplanDispatchResponse response;
    response.subplan_uuid = request.subplan.subplan_uuid;

    // 1. Verify security bundle
    if (request.security_bundle_hash != get_current_bundle_hash()) {
        response.status = SubplanStatus::FAILED;
        response.error_code = ErrorCode::SECURITY_BUNDLE_MISMATCH;
        response.error_message = "Shard's security bundle does not match coordinator";
        return response;
    }

    // 2. Start shard-local transaction (read-only snapshot)
    Transaction txn = begin_transaction(TransactionMode::READ_ONLY);

    // 3. Execute subplan SQL
    try {
        ResultSet result = txn.execute_query(request.subplan.subplan_sql);

        response.status = SubplanStatus::SUCCESS;
        response.result_set = result;

        txn.commit();
    } catch (const Exception& e) {
        response.status = SubplanStatus::FAILED;
        response.error_code = e.code();
        response.error_message = e.message();

        txn.rollback();
    }

    return response;
}
```

### 5.4 Result Collection

Coordinator collects results from all subplans:

```cpp
vector<SubplanResult> collect_subplan_results(
    const vector<Subplan>& subplans,
    timeout_t timeout
) {
    vector<SubplanResult> results;

    // Dispatch all subplans concurrently
    vector<future<SubplanDispatchResponse>> futures;
    for (const Subplan& subplan : subplans) {
        SubplanDispatchRequest request;
        request.subplan = subplan;
        request.security_bundle_hash = subplan.security_bundle_hash;
        request.execution_timeout = timeout;

        // Async RPC call
        futures.push_back(rpc_client.execute_subplan_async(
            subplan.target_node_uuid, request));
    }

    // Wait for all results (with timeout)
    auto deadline = now() + timeout;
    for (size_t i = 0; i < futures.size(); ++i) {
        auto remaining_time = deadline - now();
        if (remaining_time <= 0) {
            // Timeout - some subplans didn't complete
            throw TimeoutException("Distributed query timed out");
        }

        SubplanDispatchResponse response = futures[i].get(remaining_time);

        SubplanResult result;
        result.subplan_uuid = subplans[i].subplan_uuid;
        result.shard_uuid = subplans[i].shard_uuid;
        result.status = response.status;
        result.result_set = response.result_set;
        result.error_code = response.error_code;
        result.error_message = response.error_message;
        result.completed_at = now();

        results.push_back(result);
    }

    return results;
}
```

---

## 6. Result Merging

### 6.1 UNION / UNION ALL

For simple SELECT queries without aggregation:

```cpp
ResultSet merge_union(const vector<SubplanResult>& subplan_results, bool distinct) {
    ResultSet merged;

    // Collect all rows from all shards
    for (const SubplanResult& result : subplan_results) {
        if (result.status != SubplanStatus::SUCCESS) {
            throw QueryException("Subplan failed: " + result.error_message);
        }

        for (const Row& row : result.result_set.rows) {
            merged.rows.push_back(row);
        }
    }

    // If UNION DISTINCT, deduplicate
    if (distinct) {
        deduplicate_rows(merged.rows);
    }

    return merged;
}
```

### 6.2 Aggregate Re-Aggregation

For aggregate queries (COUNT, SUM, AVG, MIN, MAX):

```cpp
ResultSet merge_aggregate(
    const vector<SubplanResult>& subplan_results,
    const MergeOperation& merge_op
) {
    // Example: Global query
    //   SELECT region, COUNT(*), SUM(amount), AVG(amount)
    //   FROM orders
    //   GROUP BY region

    // Shard results:
    //   Shard 1: {region: "NORTH", count: 50, sum: 15000}
    //   Shard 2: {region: "NORTH", count: 100, sum: 30000}
    //   Shard 3: {region: "SOUTH", count: 200, sum: 60000}

    // Merge step:
    //   Group by region, SUM(count), SUM(sum)
    //   NORTH: count = 150, sum = 45000, avg = 45000/150 = 300
    //   SOUTH: count = 200, sum = 60000, avg = 60000/200 = 300

    map<GroupKey, AggregateState> groups;

    for (const SubplanResult& result : subplan_results) {
        for (const Row& row : result.result_set.rows) {
            GroupKey group_key = extract_group_key(row, merge_op.group_by_column_uuids);

            AggregateState& state = groups[group_key];
            state.merge(row, merge_op.aggregate_functions);
        }
    }

    // Finalize aggregates (e.g., compute AVG from SUM/COUNT)
    ResultSet merged;
    for (const auto& [group_key, state] : groups) {
        Row row = state.finalize();
        merged.rows.push_back(row);
    }

    return merged;
}
```

**Supported Aggregate Functions**:
- **COUNT(*)**: SUM of counts
- **SUM(x)**: SUM of sums
- **MIN(x)**: MIN of mins
- **MAX(x)**: MAX of maxs
- **AVG(x)**: SUM(sum) / SUM(count)

### 6.3 Merge Sort (ORDER BY)

For queries with ORDER BY:

```cpp
ResultSet merge_sort(
    const vector<SubplanResult>& subplan_results,
    const MergeOperation& merge_op
) {
    // Each shard returns results sorted by ORDER BY columns
    // Coordinator performs K-way merge

    priority_queue<RowWithSource, ComparatorType> heap;

    // Initialize heap with first row from each shard
    for (size_t i = 0; i < subplan_results.size(); ++i) {
        if (!subplan_results[i].result_set.rows.empty()) {
            heap.push({subplan_results[i].result_set.rows[0], i, 0});
        }
    }

    ResultSet merged;

    // K-way merge
    while (!heap.empty()) {
        RowWithSource top = heap.top();
        heap.pop();

        merged.rows.push_back(top.row);

        // Push next row from same shard
        size_t shard_idx = top.shard_index;
        size_t row_idx = top.row_index + 1;
        if (row_idx < subplan_results[shard_idx].result_set.rows.size()) {
            heap.push({subplan_results[shard_idx].result_set.rows[row_idx], shard_idx, row_idx});
        }
    }

    return merged;
}
```

### 6.4 Top-N (LIMIT)

For queries with LIMIT:

```cpp
ResultSet merge_top_n(
    const vector<SubplanResult>& subplan_results,
    const MergeOperation& merge_op
) {
    // Each shard returns top N rows
    // Coordinator merges and selects global top N

    ResultSet all_results = merge_union(subplan_results, false);

    // Sort if ORDER BY present
    if (!merge_op.sort_column_uuids.empty()) {
        sort_rows(all_results.rows, merge_op.sort_column_uuids, merge_op.sort_orders);
    }

    // Apply LIMIT and OFFSET
    ResultSet merged;
    size_t start = merge_op.offset;
    size_t end = std::min(start + merge_op.limit, all_results.rows.size());

    for (size_t i = start; i < end; ++i) {
        merged.rows.push_back(all_results.rows[i]);
    }

    return merged;
}
```

---

## 7. Consistency Model

### 7.1 Shard-Local Snapshots

**Key Limitation**: No global snapshot across shards.

Each shard executes its subplan in its own read-only transaction:
- Shard 1 snapshot at T1 = 10:00:00.123
- Shard 2 snapshot at T2 = 10:00:00.456
- Shard 3 snapshot at T3 = 10:00:00.789

**Implication**: Results may reflect inconsistent states across shards.

**Example**:
```sql
-- Query: SELECT COUNT(*) FROM orders;
-- Shard 1 snapshot: 1000 orders
-- Shard 2 snapshot: 2000 orders
-- Shard 3 snapshot: 1500 orders
-- Total: 4500 orders

-- But if an order was moved from Shard 1 to Shard 2 between snapshots:
-- Shard 1 might have counted it (before move)
-- Shard 2 might have counted it (after move)
-- → Double counting (4501 orders)
```

**Mitigation**: ScratchBird does NOT move data between shards during normal operation. Resharding is offline.

### 7.2 Read-After-Write Consistency

**Problem**: Client writes to Shard 1, then immediately reads from all shards. Shard 1's new data is visible, but other shards may have stale views.

**Solution**: Session affinity - route client's reads to the shard where they wrote.

```cpp
// Client session tracks recent writes
struct SessionState {
    UUID session_uuid;
    map<UUID, timestamp_t> shard_last_write_timestamps;  // shard_uuid -> timestamp
};

// Coordinator checks: Can this query be satisfied with session's writes visible?
bool is_read_after_write_consistent(const SessionState& session, const DistributedQuery& query) {
    // If query involves shards the session hasn't written to, it's consistent
    for (const UUID& shard_uuid : query.involved_shard_uuids) {
        if (session.shard_last_write_timestamps.count(shard_uuid) > 0) {
            // Query touches a shard the session wrote to
            // Ensure snapshot is after write timestamp
            // (Implementation depends on timestamp propagation)
        }
    }
    return true;
}
```

---

## 8. Error Handling

### 8.1 Subplan Failure

If any subplan fails, the entire distributed query fails:

```cpp
void handle_subplan_failure(const SubplanResult& failed_result) {
    log_error("Subplan {} failed: {}",
        failed_result.subplan_uuid.to_string(),
        failed_result.error_message);

    // Abort entire distributed query
    throw DistributedQueryException(
        "Distributed query failed due to subplan failure on shard " +
        failed_result.shard_uuid.to_string() + ": " +
        failed_result.error_message
    );
}
```

**No Partial Results**: ScratchBird does NOT return partial results if some shards fail.

### 8.2 Timeout Handling

If subplan exceeds timeout:

```cpp
void handle_subplan_timeout(const Subplan& subplan) {
    log_warning("Subplan {} timed out after {}s",
        subplan.subplan_uuid.to_string(),
        timeout.seconds());

    // Cancel subplan execution on shard (send cancellation RPC)
    rpc_client.cancel_subplan(subplan.target_node_uuid, subplan.subplan_uuid);

    // Fail entire distributed query
    throw TimeoutException("Distributed query timed out");
}
```

### 8.3 Retry Logic

**Transient Failures** (network errors, node unavailability):

```cpp
SubplanResult execute_subplan_with_retry(const Subplan& subplan, uint32_t max_retries) {
    uint32_t attempt = 0;

    while (attempt < max_retries) {
        try {
            SubplanDispatchResponse response = rpc_client.execute_subplan(
                subplan.target_node_uuid, subplan);

            if (response.status == SubplanStatus::SUCCESS) {
                return convert_to_result(response);
            } else if (is_retryable_error(response.error_code)) {
                // Retry after backoff
                attempt++;
                sleep(exponential_backoff(attempt));
                continue;
            } else {
                // Non-retryable error
                throw QueryException(response.error_message);
            }
        } catch (const NetworkException& e) {
            // Network error - retry with backoff
            attempt++;
            if (attempt >= max_retries) {
                throw;
            }
            sleep(exponential_backoff(attempt));
        }
    }

    throw MaxRetriesExceededException("Subplan failed after max retries");
}
```

---

## 9. Optimization Techniques

### 9.1 Subplan Pruning

If query has shard key filter, route to single shard only:

```sql
-- This query can be pruned to a single shard
SELECT * FROM orders WHERE customer_id = '01234567-89ab-cdef-0123-456789abcdef';
```

**Savings**: Query 1 shard instead of all N shards.

### 9.2 Predicate Pushdown

Push filters to shards to reduce data transfer:

```sql
-- Global query
SELECT region, SUM(amount) FROM orders WHERE date >= '2026-01-01' GROUP BY region;

-- Subplan (filter pushed down)
SELECT region, SUM(amount) FROM orders_shard WHERE date >= '2026-01-01' GROUP BY region;
```

**Savings**: Each shard filters locally, returns only aggregates (not raw rows).

### 9.3 Projection Pushdown

Push column selection to shards:

```sql
-- Global query
SELECT order_id, customer_id FROM orders;

-- Subplan (only select needed columns)
SELECT order_id, customer_id FROM orders_shard;  -- Not SELECT *
```

**Savings**: Reduced network bandwidth, faster shard execution.

### 9.4 Parallel Execution

Execute all subplans concurrently:

```cpp
// Sequential execution: T = T1 + T2 + T3
// Parallel execution: T = max(T1, T2, T3)

// With 3 shards, each taking 100ms:
// Sequential: 300ms
// Parallel: 100ms (3x speedup)
```

---

## 10. Security Considerations

### 10.1 Security Bundle Enforcement

**Critical**: All nodes must have identical security bundle before execution.

**Enforcement Points**:
1. Coordinator checks before dispatching subplans
2. Each shard checks when receiving subplan
3. If mismatch detected, query aborted immediately

### 10.2 RLS/CLS Application

Each shard applies RLS/CLS policies from the security bundle:

```sql
-- User's query
SELECT * FROM orders;

-- Shard applies RLS policy (tenant isolation)
SELECT * FROM orders WHERE tenant_id = current_tenant_id();

-- Shard applies CLS policy (mask SSN column)
SELECT order_id, customer_id, mask_ssn(customer_ssn) as customer_ssn, ...
FROM orders WHERE tenant_id = current_tenant_id();
```

**Guarantee**: Because all shards have identical security bundle, RLS/CLS is applied identically.

### 10.3 Audit Logging

Distributed queries generate audit records:

```cpp
void log_distributed_query_audit(const DistributedQuery& query) {
    AuditRecord record;
    record.event_type = AuditEventType::DISTRIBUTED_QUERY;
    record.query_uuid = query.query_uuid;
    record.session_uuid = query.session_uuid;
    record.sql = query.original_sql;
    record.involved_shards = query.involved_shard_uuids;
    record.coordinator_node = query.coordinator_node_uuid;
    record.timestamp = now();

    audit_log.write(record);
}
```

---

## 11. Testing Procedures

### 11.1 Functional Tests

**Test: Single-Shard Routing**
```cpp
TEST(DistributedQuery, SingleShardRouting) {
    auto cluster = create_test_cluster(3, 16);  // 3 nodes, 16 shards

    UUID customer_id = UUID::random();

    auto query = cluster.coordinator().execute_query(
        fmt::format("SELECT * FROM orders WHERE customer_id = '{}'", customer_id)
    );

    // Verify query was routed to exactly one shard
    EXPECT_EQ(query.involved_shard_uuids.size(), 1);
}
```

**Test: Aggregate Merge**
```cpp
TEST(DistributedQuery, AggregateMerge) {
    auto cluster = create_test_cluster(3, 16);

    // Insert test data across shards
    insert_test_orders(cluster, 10000);

    // Execute distributed aggregate query
    ResultSet result = cluster.coordinator().execute_query(
        "SELECT region, COUNT(*), SUM(amount) FROM orders GROUP BY region"
    );

    // Verify results are correct
    EXPECT_EQ(result.rows.size(), 4);  // 4 regions
    uint64_t total_count = 0;
    for (const Row& row : result.rows) {
        total_count += row.get_uint64("count");
    }
    EXPECT_EQ(total_count, 10000);
}
```

### 11.2 Security Tests

**Test: Security Bundle Mismatch**
```cpp
TEST(DistributedQuery, SecurityBundleMismatch) {
    auto cluster = create_test_cluster(3, 16);

    // Artificially create bundle mismatch
    cluster.node(1).apply_bundle(create_test_bundle_v1());
    cluster.node(2).apply_bundle(create_test_bundle_v2());  // Different!

    // Attempt distributed query
    EXPECT_THROW(
        cluster.coordinator().execute_query("SELECT COUNT(*) FROM orders"),
        SecurityBundleMismatchException
    );
}
```

### 11.3 Performance Tests

**Test: Parallel Speedup**
```cpp
TEST(DistributedQuery, ParallelSpeedup) {
    auto cluster = create_test_cluster(10, 100);  // 10 nodes, 100 shards

    insert_large_dataset(cluster, 100000000);  // 100M rows

    auto start = now();
    ResultSet result = cluster.coordinator().execute_query(
        "SELECT region, COUNT(*) FROM orders GROUP BY region"
    );
    auto elapsed = now() - start;

    // Expect near-linear speedup (100 shards in parallel)
    // If single-shard execution takes 10s, expect ~0.1-0.2s with 100 parallel shards
    EXPECT_LT(elapsed, 1s);
}
```

---

## 12. Examples

### 12.1 Single-Shard Query

```sql
-- Query routed to single shard (customer_id is shard key)
SELECT order_id, order_date, total
FROM orders
WHERE customer_id = '01234567-89ab-cdef-0123-456789abcdef'
ORDER BY order_date DESC
LIMIT 10;
```

**Execution**:
- Coordinator routes to shard owning this customer_id
- Single shard executes entire query locally
- Results returned directly to client
- **No result merging needed**

### 12.2 Distributed Aggregation

```sql
-- Query requires all shards
SELECT product_category, COUNT(*), AVG(price), SUM(quantity)
FROM orders
WHERE order_date >= '2026-01-01'
GROUP BY product_category;
```

**Execution**:
1. Coordinator generates subplan for each shard:
   ```sql
   SELECT product_category, COUNT(*) as cnt, SUM(price) as sum_price,
          COUNT(price) as count_price, SUM(quantity) as sum_qty
   FROM orders_shard_N
   WHERE order_date >= '2026-01-01'
   GROUP BY product_category;
   ```

2. Each shard executes and returns partial aggregates:
   ```
   Shard 1: {category: "Electronics", cnt: 500, sum_price: 150000, count_price: 500, sum_qty: 2000}
   Shard 2: {category: "Electronics", cnt: 300, sum_price: 90000, count_price: 300, sum_qty: 1200}
   Shard 3: {category: "Books", cnt: 1000, sum_price: 25000, count_price: 1000, sum_qty: 5000}
   ```

3. Coordinator merges:
   ```
   Electronics: COUNT = 800, AVG = 240000/800 = 300, SUM(qty) = 3200
   Books: COUNT = 1000, AVG = 25000/1000 = 25, SUM(qty) = 5000
   ```

### 12.3 Distributed ORDER BY with LIMIT

```sql
-- Top 100 most expensive orders across all shards
SELECT order_id, customer_id, total
FROM orders
ORDER BY total DESC
LIMIT 100;
```

**Execution**:
1. Each shard returns its top 100 orders (sorted by total DESC)
2. Coordinator performs 100-way merge sort
3. Returns global top 100

**Optimization**: Each shard only returns 100 rows (not all rows), minimizing data transfer.

---

## 13. Performance Characteristics

### 13.1 Latency Components

```
Total Latency = Network RTT + Planning + Execution + Merge

Network RTT:   ~1-5ms (intra-datacenter)
Planning:      ~1-10ms (depends on query complexity)
Execution:     Variable (depends on data size, filters)
Merge:         ~1-100ms (depends on result size, merge strategy)
```

### 13.2 Throughput Scaling

With N shards executing in parallel:
- **Ideal speedup**: N×
- **Actual speedup**: 0.7-0.9N× (due to coordinator overhead)

### 13.3 Network Bandwidth

**Example**: 100 shards, each returns 10 KB aggregates
- Total transfer: 100 × 10 KB = 1 MB
- With 10 Gbps network: ~0.8 ms transfer time

**Optimization**: Push aggregation to shards to minimize transfer.

---

## 14. References

### 14.1 Internal References
- **SBCLUSTER-00**: Guiding Principles
- **SBCLUSTER-04**: Security Bundle
- **SBCLUSTER-05**: Sharding
- **SBCLUSTER-02**: Membership and Identity
- **PARALLEL_EXECUTION_ARCHITECTURE.md**: Parallel execution model (Beta)

### 14.2 External References
- **Google Spanner**: "Spanner: Google's Globally Distributed Database", OSDI 2012
- **Apache Calcite**: Query planning and optimization framework
- **Presto/Trino**: Distributed SQL query engine architecture
- **ClickHouse**: "ClickHouse Distributed Query Execution", ClickHouse Blog

### 14.3 Research Papers
- *"Parallelizing Query Optimization"* - Ono & Lohman, VLDB 1990
- *"Distributed Query Processing in Relational Database Systems"* - Kossmann, ACM Computing Surveys 2000

---

## 15. Revision History

| Version | Date       | Author       | Changes                                    |
|---------|------------|--------------|--------------------------------------------|
| 1.0     | 2026-01-02 | D. Calford   | Initial comprehensive specification        |

---

**Document Status**: DRAFT (Beta Specification Phase)
**Next Review**: Before Beta Implementation Phase
**Approval Required**: Chief Architect, Query Engine Lead, Distributed Systems Lead
