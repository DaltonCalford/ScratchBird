# Distributed Infrastructure Architecture

## Overview

ScratchBird's distributed architecture enables seamless scaling from single-node to enterprise clusters while maintaining compatibility with existing applications.

## Core Distributed Capabilities

### 1. Transparent Client Compatibility

```sql
-- Existing MySQL client connects normally
mysql -h scratchbird.company.com -P 3306 -u user -p

-- Behind the scenes:
-- 1. Connection hits ScratchBird cluster
-- 2. Y-Valve identifies MySQL protocol
-- 3. MySQL parser converts to BLR
-- 4. Query distributed across cluster
-- 5. Results returned in MySQL format
-- Client never knows it's not MySQL!
```

### 2. Foreign Data Wrapper (FDW) Architecture

```sql
-- Create foreign server connections
CREATE FOREIGN SERVER oracle_hr
    TYPE 'oracle'
    OPTIONS (
        host 'oracle.company.com',
        port '1521',
        database 'HRDB'
    );

CREATE FOREIGN SERVER postgres_sales
    TYPE 'postgresql'
    OPTIONS (
        host 'postgres.company.com',
        port '5432',
        database 'sales'
    );

CREATE FOREIGN SERVER remote_scratchbird
    TYPE 'scratchbird'
    OPTIONS (
        host 'scratchbird2.company.com',
        port '3050',
        database 'analytics'
    );

-- Map foreign tables
CREATE FOREIGN TABLE oracle_employees (
    emp_id INTEGER,
    name VARCHAR(100),
    salary DECIMAL(10,2)
) SERVER oracle_hr
OPTIONS (schema 'HR', table 'EMPLOYEES');

-- Transparent queries across systems
SELECT 
    e.name,
    e.salary,
    s.total_sales
FROM oracle_employees e
JOIN postgres_sales.public.sales_summary s ON e.emp_id = s.emp_id
WHERE e.salary > 50000;
```

### 3. ScratchBird-to-ScratchBird Federation

```cpp
namespace scratchbird::federation {

class FederationManager {
private:
    struct RemoteNode {
        string node_id;
        string hostname;
        IExecutionEngine* remote_engine;
        NetworkConnection connection;
        NodeCapabilities capabilities;
        LatencyStats latency;
    };
    
    map<string, RemoteNode> cluster_nodes;
    
public:
    // Push computation to data
    ExecutionResult execute_distributed(const BLRProgram& blr, const QueryPlan& plan) {
        // Analyze where data resides
        auto fragments = analyze_data_locality(plan);
        
        vector<future<PartialResult>> remote_executions;
        
        for (auto& [node_id, fragment] : fragments) {
            if (node_id == LOCAL_NODE) {
                // Execute locally
                continue;
            }
            
            // Ship BLR fragment to remote node
            remote_executions.push_back(
                async([this, node_id, fragment] {
                    auto& node = cluster_nodes[node_id];
                    
                    // Send BLR, not SQL - already optimized!
                    return node.remote_engine->execute_blr(
                        fragment.blr,
                        fragment.params
                    );
                })
            );
        }
        
        // Combine results
        return merge_distributed_results(remote_executions);
    }
    
    // Distributed transaction coordination
    void two_phase_commit(const vector<string>& participating_nodes) {
        // Phase 1: Prepare
        for (const auto& node_id : participating_nodes) {
            cluster_nodes[node_id].remote_engine->prepare_transaction();
        }
        
        // Phase 2: Commit if all prepared
        for (const auto& node_id : participating_nodes) {
            cluster_nodes[node_id].remote_engine->commit_transaction();
        }
    }
};

} // namespace scratchbird::federation
```

### 4. Multi-Tablespace Architecture

```sql
-- Create tablespaces on different storage tiers
CREATE TABLESPACE fast_ssd
    LOCATION '/mnt/nvme/scratchbird'
    OPTIONS (
        io_cost = 0.1,      -- Very fast I/O
        cache_priority = 'high',
        compression = 'none'
    );

CREATE TABLESPACE standard_ssd
    LOCATION '/mnt/ssd/scratchbird'
    OPTIONS (
        io_cost = 1.0,      -- Standard SSD
        cache_priority = 'normal',
        compression = 'lz4'
    );

CREATE TABLESPACE archive_hdd
    LOCATION '/mnt/hdd/scratchbird'
    OPTIONS (
        io_cost = 10.0,     -- Slow HDD
        cache_priority = 'low',
        compression = 'zstd_high'
    );

CREATE TABLESPACE remote_nas
    LOCATION 'nfs://nas.company.com/scratchbird'
    OPTIONS (
        io_cost = 100.0,    -- Network storage
        cache_priority = 'minimal',
        compression = 'zstd_ultra'
    );

-- Specify storage location for all objects
CREATE TABLE orders (
    order_id BIGINT,
    order_date DATE,
    customer_id INTEGER,
    total DECIMAL(10,2)
) TABLESPACE standard_ssd;

-- Indexes on fast storage
CREATE INDEX idx_orders_date ON orders(order_date) 
    TABLESPACE fast_ssd;

-- Partitioning with storage tiering
CREATE TABLE events (
    event_time TIMESTAMP,
    event_data JSONB
) PARTITION BY RANGE (event_time);

-- Current month on fast storage
CREATE TABLE events_2024_01 PARTITION OF events
    FOR VALUES FROM ('2024-01-01') TO ('2024-02-01')
    TABLESPACE fast_ssd;

-- Last month on standard storage
CREATE TABLE events_2023_12 PARTITION OF events
    FOR VALUES FROM ('2023-12-01') TO ('2024-01-01')
    TABLESPACE standard_ssd;

-- Old data on archive storage
CREATE TABLE events_2023_old PARTITION OF events
    FOR VALUES FROM ('2023-01-01') TO ('2023-12-01')
    TABLESPACE archive_hdd;

-- Even stored procedures can specify location
CREATE PROCEDURE process_orders()
    TABLESPACE fast_ssd  -- Keep hot procedures on fast storage
AS BEGIN
    -- procedure body
END;
```

## Distributed Query Execution

### Query Pushdown and Data Locality

```cpp
class DistributedQueryOptimizer {
    QueryPlan optimize_for_distribution(const QueryPlan& local_plan) {
        DistributedPlan dist_plan;
        
        // Identify data locations
        for (auto& operation : local_plan.operations) {
            if (operation.type == OpType::TABLE_SCAN) {
                TableLocation loc = catalog.get_table_location(operation.table);
                
                if (loc.is_remote()) {
                    // Push predicate to remote node
                    dist_plan.add_remote_operation(
                        loc.node_id,
                        push_down_predicates(operation)
                    );
                } else if (loc.is_partitioned()) {
                    // Parallel scan across partitions
                    for (auto& partition : loc.partitions) {
                        dist_plan.add_partition_scan(partition);
                    }
                }
            }
        }
        
        // Optimize join order for network traffic
        if (has_distributed_join(dist_plan)) {
            optimize_distributed_join_order(dist_plan);
        }
        
        return dist_plan;
    }
    
    // Smart data movement decisions
    JoinStrategy choose_distributed_join_strategy(
        const TableStats& left,
        const TableStats& right
    ) {
        if (left.size < BROADCAST_THRESHOLD) {
            return JoinStrategy::BROADCAST_LEFT;  // Send small table everywhere
        }
        if (right.size < BROADCAST_THRESHOLD) {
            return JoinStrategy::BROADCAST_RIGHT;
        }
        if (left.node_id == right.node_id) {
            return JoinStrategy::LOCAL_JOIN;  // Same node, no movement
        }
        return JoinStrategy::DISTRIBUTED_HASH;  // Partition both tables
    }
};
```

### Removing Middle Tiers

```cpp
// Traditional 3-tier architecture:
// Client → App Server → Database

// ScratchBird distributed architecture:
// Client → ScratchBird Cluster (with embedded business logic)

class EmbeddedBusinessLogic {
    // Business logic as stored procedures with rich capabilities
    void create_business_logic() {
        engine->execute(R"(
            CREATE PROCEDURE process_order(
                customer_id INTEGER,
                items JSON
            ) RETURNS JSON
            AS BEGIN
                -- Validation
                IF NOT exists_customer(customer_id) THEN
                    RETURN json_error('Invalid customer');
                END IF;
                
                -- Check inventory across distributed nodes
                FOR item IN items LOOP
                    DECLARE available INTEGER;
                    
                    -- Query remote inventory node
                    SELECT quantity INTO available
                    FROM remote_inventory.stock
                    WHERE product_id = item.product_id;
                    
                    IF available < item.quantity THEN
                        RETURN json_error('Insufficient stock');
                    END IF;
                END LOOP;
                
                -- Process payment via foreign table
                DECLARE payment_result JSON;
                SELECT process_payment(customer_id, total_amount) 
                INTO payment_result
                FROM payment_service.api;
                
                -- Create order
                INSERT INTO orders (...) VALUES (...);
                
                -- Update distributed inventory
                UPDATE remote_inventory.stock SET ...;
                
                -- Send notification
                POST EVENT 'order_placed' WITH order_details;
                
                RETURN json_success(order_id);
            END;
        )");
    }
};
```

## Architecture Issues and Considerations

### 1. ❗ Network Partition Handling

```cpp
class PartitionHandler {
    // CAP theorem: Choose CP or AP
    enum ConsistencyModel {
        STRONG_CONSISTENCY,    // CP: Reject writes during partition
        EVENTUAL_CONSISTENCY,  // AP: Allow writes, reconcile later
        BOUNDED_STALENESS     // Hybrid: Allow staleness up to N seconds
    };
    
    void handle_network_partition(const vector<string>& unreachable_nodes) {
        switch (consistency_model) {
            case STRONG_CONSISTENCY:
                if (unreachable_nodes.size() >= cluster_size / 2) {
                    enter_read_only_mode();  // Lost quorum
                }
                break;
                
            case EVENTUAL_CONSISTENCY:
                mark_nodes_disconnected(unreachable_nodes);
                enable_conflict_tracking();
                break;
        }
    }
};
```

### 2. ❗ Distributed Transaction Complexity

```cpp
// Distributed deadlock detection
class DistributedDeadlockDetector {
    // Need global wait-for graph
    void detect_distributed_deadlock() {
        // Collect wait-for graphs from all nodes
        // Build global graph
        // Detect cycles
        // Choose victim
    }
};
```

### 3. ❗ Data Locality Optimization

```cpp
class DataLocalityOptimizer {
    // Moving computation vs moving data
    Decision decide_execution_location(
        const QueryFragment& fragment,
        const DataLocation& data_loc
    ) {
        size_t data_size = estimate_data_size(fragment);
        size_t result_size = estimate_result_size(fragment);
        
        if (result_size < data_size * 0.1) {
            // Push computation to data
            return Decision::EXECUTE_REMOTE;
        } else {
            // Pull data to computation
            return Decision::FETCH_DATA;
        }
    }
};
```

### 4. ❗ Schema Synchronization

```cpp
class SchemaReplication {
    // Keep schemas in sync across cluster
    void propagate_ddl(const DDLStatement& ddl) {
        // Two-phase DDL execution
        // 1. Prepare on all nodes
        // 2. Execute atomically
        
        // Handle node failures during DDL
        // Rollback if any node fails
    }
};
```

### 5. ❗ Security Across Nodes

```cpp
class DistributedSecurity {
    // Authentication and authorization across cluster
    struct SecurityToken {
        string user_id;
        vector<Permission> permissions;
        timestamp expiry;
        string signature;  // Cryptographic signature
    };
    
    // Propagate security context
    void forward_security_context(
        const SecurityToken& token,
        const string& target_node
    ) {
        // Validate token
        // Check permissions for cross-node access
        // Audit cross-node operations
    }
};
```

## How Data is Presented to Users

### 1. Unified Global Namespace

```sql
-- Users see single logical database
SELECT * FROM customers;  -- Don't care where it's stored

-- But can be explicit if needed
SELECT * FROM remote_node.schema.table;

-- Or use hints
SELECT /*+ USE_NODE(analytics_node) */ 
    * FROM large_analytics_table;
```

### 2. Transparent Sharding

```sql
-- Automatic sharding invisible to users
CREATE TABLE users (
    user_id BIGINT,
    email VARCHAR(255),
    region VARCHAR(50)
) DISTRIBUTE BY HASH(user_id);

-- Query works normally
SELECT * FROM users WHERE email = 'user@example.com';
-- System automatically queries all shards
```

### 3. Location Transparency

```sql
-- Users don't need to know storage tiers
SELECT * FROM events WHERE event_time > '2024-01-01';
-- Automatically queries fast_ssd for recent, archive_hdd for old
```

### 4. Federated Views

```sql
CREATE VIEW global_employees AS
    SELECT 'oracle' as source, * FROM oracle_employees
    UNION ALL
    SELECT 'postgres' as source, * FROM postgres_employees
    UNION ALL
    SELECT 'scratchbird' as source, * FROM local_employees;

-- Single query across all systems
SELECT * FROM global_employees WHERE salary > 100000;
```

## Advantages of This Architecture

### 1. ✅ **No Application Changes Required**
- Existing clients connect normally
- Protocol emulation handles compatibility
- Transparent distribution

### 2. ✅ **Unified Data Platform**
- Query across any data source
- Single point of truth
- Consistent security model

### 3. ✅ **Intelligent Resource Usage**
- Data tiering (hot/warm/cold)
- Computation pushdown
- Network traffic minimization

### 4. ✅ **Simplified Architecture**
- Fewer moving parts
- No middle tier needed
- Business logic in database

## Challenges to Address

### 1. 🔴 **Distributed Consistency**
- Need clear consistency model
- Handle network partitions
- Conflict resolution strategy

### 2. 🟡 **Performance Monitoring**
- Cross-node query tracing
- Distributed explain plans
- Network latency impact

### 3. 🟡 **Operational Complexity**
- Rolling upgrades
- Node addition/removal
- Backup coordination

### 4. 🔴 **Security Boundaries**
- Cross-node authentication
- Encryption in transit
- Audit trail across nodes

### 5. 🟡 **Query Optimization**
- Cost-based optimization across nodes
- Statistics synchronization
- Join strategy selection

## Recommendations

### 1. Start with Federation Basics
- Single-node with foreign tables first
- Add node-to-node later
- Gradual complexity increase

### 2. Clear Consistency Model
- Default to strong consistency
- Allow eventual for specific tables
- Document trade-offs clearly

### 3. Robust Monitoring
- Distributed tracing from day one
- Network traffic analytics
- Query performance across nodes

### 4. Security First
- End-to-end encryption
- Token-based authentication
- Comprehensive audit logs

This distributed architecture would make ScratchBird a true **Data Fabric** - transparently connecting all data sources while maintaining performance and consistency!