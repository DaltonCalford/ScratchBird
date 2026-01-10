# Schema-Driven Colocation for Multi-Table Atomicity

**Document:** 03_SCHEMA_DRIVEN_COLOCATION.md
**Status:** BETA SPECIFICATION
**Version:** 1.0
**Date:** 2026-01-09
**Authority:** Chief Architect

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Problem Statement](#problem-statement)
3. [Colocation Model](#colocation-model)
4. [Partition Key Declaration](#partition-key-declaration)
5. [Local Transaction Coordination](#local-transaction-coordination)
6. [Cross-Partition Transactions](#cross-partition-transactions)
7. [Foreign Key Optimization](#foreign-key-optimization)
8. [Integration with SBCLUSTER-05](#integration-with-sbcluster-05)
9. [Query Planning](#query-planning)
10. [Testing and Verification](#testing-and-verification)

---

## Executive Summary

### Purpose

This specification defines **schema-driven colocation**, a technique for achieving multi-table atomicity in distributed environments without global two-phase commit (2PC). By colocating related data on the same shard (guided by foreign key relationships), ScratchBird enables local transactions across multiple tables, preserving ACID guarantees while maintaining horizontal scalability.

**Key Innovation**: Unlike hash-based sharding (random distribution), schema-driven colocation uses **partition keys** to deterministically place related rows on the same physical shard, enabling local multi-table transactions.

### Design Principles

1. **Avoid Global 2PC**: Local transactions are fast, global 2PC kills availability
2. **Schema as Intent**: Foreign keys signal colocation intent (auto-optimization)
3. **User Control**: Users explicitly declare partition keys for critical relationships
4. **Fallback to Saga**: Cross-partition transactions use saga pattern (eventual consistency)
5. **Firebird MGA Compatible**: Local transactions use existing TIP-based coordination

### Use Cases

**Local Transactions (Colocated)**:
```sql
-- User and Orders colocated by user_id → Single shard, ACID transaction
BEGIN TRANSACTION;
INSERT INTO users (user_id, name) VALUES ('user_123', 'Alice');
INSERT INTO orders (order_id, user_id, amount) VALUES ('order_456', 'user_123', 100.0);
COMMIT;  -- Atomic, no 2PC needed
```

**Cross-Partition Transactions (Not Colocated)**:
```sql
-- Transfer between accounts on different shards → Saga pattern
BEGIN TRANSACTION;
UPDATE accounts SET balance = balance - 100 WHERE account_id = 'A';  -- Shard 1
UPDATE accounts SET balance = balance + 100 WHERE account_id = 'B';  -- Shard 2
COMMIT;  -- Uses saga (eventual consistency)
```

---

## Problem Statement

### The Distributed Transaction Dilemma

**Single-Node Databases**: All tables in one process, transactions are trivial (local locks, local commit).

**Distributed Databases**: Data partitioned across nodes, transactions spanning multiple partitions require coordination.

**Traditional Solution: Two-Phase Commit (2PC)**

**Phase 1: Prepare**
- Coordinator asks all shards: "Can you commit?"
- Each shard votes YES (prepared) or NO (abort)

**Phase 2: Commit**
- If all YES: Coordinator tells all shards "commit"
- If any NO: Coordinator tells all shards "abort"

**2PC Problems**:
1. **Latency**: 2 network round-trips per transaction (kills performance)
2. **Availability**: Coordinator SPOF (crash during 2PC → all shards blocked)
3. **Partition Intolerance**: Network partition → transaction blocks indefinitely

**CAP Theorem**: 2PC prioritizes Consistency + Partition Tolerance, sacrifices Availability (CP system).

### Alternative Approaches

**Option 1: Eventual Consistency (No ACID)**
- Each shard commits independently
- Use sagas/compensating transactions for rollback
- Pro: High availability, no coordinator SPOF
- Con: No ACID guarantees (hard to reason about)

**Option 2: Single-Shard Transactions Only**
- Restrict transactions to single shard (like Google Spanner entity groups)
- Pro: Local ACID, no coordination overhead
- Con: Application must manually partition data

**Option 3: Schema-Driven Colocation** (ScratchBird's Approach)
- Automatically colocate related tables on same shard (via partition keys)
- Local transactions for colocated data (ACID)
- Saga pattern for cross-partition data (eventual consistency)
- Pro: Best of both worlds (ACID when possible, availability when needed)
- Con: Requires schema modeling (partition key design)

---

## Colocation Model

### Partition Key Concept

**Partition Key**: Column(s) used to determine which shard stores a row.

**Hash-Based Sharding** (traditional):
```
shard_id = hash(primary_key) % num_shards
```
- **Problem**: Related rows (e.g., user and their orders) land on different shards randomly

**Partition-Key Sharding** (ScratchBird):
```
shard_id = hash(partition_key) % num_shards
```
- **Solution**: All rows with same partition key land on same shard

**Example**:

```sql
-- Table: users (partition key = user_id)
CREATE TABLE users (
    user_id UUID PRIMARY KEY,
    name VARCHAR(100),
    email VARCHAR(200)
) PARTITION BY user_id;

-- Table: orders (partition key = user_id, NOT order_id)
CREATE TABLE orders (
    order_id UUID PRIMARY KEY,
    user_id UUID NOT NULL,
    amount DECIMAL(10,2)
) PARTITION BY user_id;

-- Result: All data for user_id='X' is on the same shard
-- Shard assignment:
--   hash('user_123') % 16 = 7
--   users row (user_id='user_123') → Shard 7
--   orders rows (user_id='user_123') → Shard 7
```

### Colocation Group

**Definition**: Set of tables that share the same partition key, guaranteeing colocation.

**Example Colocation Group**:
```
Partition Key: customer_id
Tables: customers, orders, payments, shipments
Guarantee: All rows for customer_id='X' are on the same shard
```

**Transaction Scope**:
- **Local**: Transactions within colocation group (single shard, ACID)
- **Cross-Partition**: Transactions across colocation groups (saga pattern, eventual consistency)

### Partition Key Selection Guidelines

**Good Partition Keys**:
1. **High Cardinality**: Many distinct values (e.g., user_id, order_id)
   - Bad: country (few values → unbalanced shards)
   - Good: user_id (millions of values → balanced shards)

2. **Query Pattern Aligned**: Most queries filter by partition key
   - Example: `SELECT * FROM orders WHERE user_id = 'X'` (partition key in WHERE)
   - Anti-example: `SELECT * FROM orders WHERE status = 'pending'` (no partition key → scatter-gather)

3. **Transaction Boundary**: Related entities share partition key
   - Example: User and their orders (both partition by user_id)
   - Anti-example: Orders and products (different aggregates)

**Trade-offs**:
- **Too Granular** (order_id as partition key for orders):
  - Pro: Perfect distribution
  - Con: Joining orders with users requires cross-partition query

- **Too Coarse** (tenant_id for multi-tenant SaaS):
  - Pro: All tenant data colocated (great for transactions)
  - Con: Large tenants create hot shards (unbalanced)

**Recommendation**: Use **aggregate root** as partition key (DDD pattern).
- **Aggregate**: User + Orders + Payments (user_id as partition key)
- **Aggregate**: Product + Inventory (product_id as partition key)
- **Transactions**: Within aggregate → local, across aggregates → saga

---

## Partition Key Declaration

### SQL Syntax

**Basic Partition Key**:

```sql
CREATE TABLE users (
    user_id UUID PRIMARY KEY,
    name VARCHAR(100)
) PARTITION BY user_id;
```

**Composite Partition Key** (multi-column):

```sql
CREATE TABLE time_series_data (
    sensor_id UUID,
    timestamp BIGINT,
    value DOUBLE,
    PRIMARY KEY (sensor_id, timestamp)
) PARTITION BY (sensor_id, timestamp);
-- Note: Both columns used to compute shard hash
```

**Implicit Partition Key** (default = primary key):

```sql
CREATE TABLE products (
    product_id UUID PRIMARY KEY,
    name VARCHAR(100)
);
-- Implicitly: PARTITION BY product_id (primary key)
```

**Colocation with Foreign Key**:

```sql
CREATE TABLE users (
    user_id UUID PRIMARY KEY,
    name VARCHAR(100)
) PARTITION BY user_id;

CREATE TABLE orders (
    order_id UUID PRIMARY KEY,
    user_id UUID NOT NULL REFERENCES users(user_id),
    amount DECIMAL(10,2)
) PARTITION BY user_id;  -- Colocate with users
```

### Catalog Schema

**Extension to `sys_tables`**:

```sql
ALTER TABLE sys_tables ADD COLUMN partition_key_columns TEXT;
-- Comma-separated list: 'user_id' or 'sensor_id,timestamp'

ALTER TABLE sys_tables ADD COLUMN colocation_group_id UUID;
-- Tables with same colocation_group_id are colocated
```

**Example Catalog Entries**:

```
table_id  | table_name | partition_key_columns | colocation_group_id
----------+------------+----------------------+---------------------
uuid_001  | users      | user_id              | group_a
uuid_002  | orders     | user_id              | group_a  ← Same group
uuid_003  | products   | product_id           | group_b
uuid_004  | inventory  | product_id           | group_b  ← Same group
```

### Parser Extensions

**AST Node** (parser_v2.h):

```cpp
struct CreateTableStatement {
    std::string table_name;
    std::vector<ColumnDef> columns;
    std::vector<std::string> partition_key_columns;  // NEW: PARTITION BY columns

    // Optional: Explicit colocation group
    std::optional<std::string> colocation_group;
};
```

**Semantic Validation**:

```cpp
auto SemanticAnalyzer::validatePartitionKey(
    const CreateTableStatement& stmt
) -> Status {
    // 1. Verify partition key columns exist in table
    for (const auto& col : stmt.partition_key_columns) {
        if (!stmt.hasColumn(col)) {
            return Status::InvalidPartitionKey("Column not found: " + col);
        }
    }

    // 2. Warn if partition key not in primary key (performance issue)
    if (!stmt.primaryKeyContains(stmt.partition_key_columns)) {
        LOG_WARNING("Partition key {} not in primary key (may cause hot spots)",
                    fmt::join(stmt.partition_key_columns, ","));
    }

    return Status::OK();
}
```

### Partition Key Immutability

**Rule**: Partition key columns are **immutable** after INSERT.

**Rationale**: Changing partition key would require moving row to different shard (expensive, coordination overhead).

**Enforcement**:

```sql
-- Example: Try to update partition key
UPDATE orders SET user_id = 'new_user_456' WHERE order_id = 'order_123';

-- Error: Cannot update partition key column 'user_id'
```

**Implementation**:

```cpp
auto Executor::validateUpdate(const UpdateStatement& stmt) -> Status {
    auto table = catalog_->getTable(stmt.table_name);
    auto partition_keys = table->getPartitionKeyColumns();

    // Check if any updated column is a partition key
    for (const auto& assignment : stmt.assignments) {
        if (std::find(partition_keys.begin(), partition_keys.end(), assignment.column)
            != partition_keys.end()) {
            return Status::InvalidUpdate("Cannot update partition key column: " + assignment.column);
        }
    }

    return Status::OK();
}
```

**Workaround**: Delete old row, insert new row (requires application logic).

---

## Local Transaction Coordination

### Single-Shard Transactions

**Guarantee**: All data for a partition key is on one shard → Transaction is local.

**Workflow**:

```sql
-- Client: Transaction spanning users + orders (both partition by user_id)
BEGIN TRANSACTION;
INSERT INTO users (user_id, name) VALUES ('user_123', 'Alice');
INSERT INTO orders (order_id, user_id, amount) VALUES ('order_456', 'user_123', 100.0);
COMMIT;
```

**Execution**:

```
1. Client sends to any node (coordinator)
2. Coordinator determines shard:
   - Hash('user_123') % 16 = Shard 7
3. Coordinator forwards entire transaction to Shard 7
4. Shard 7 (local execution):
   - Begin transaction (allocate xid via TIP)
   - Execute INSERT into users
   - Execute INSERT into orders
   - Commit transaction (mark xid as COMMITTED in TIP)
5. Coordinator responds to client: SUCCESS
```

**Latency**: Single network hop + local transaction (< 5ms)

### Multi-Shard Detection

**Problem**: User may write transaction that spans multiple shards (unknowingly or intentionally).

**Detection** (at parse/plan time):

```cpp
auto QueryPlanner::detectShardSpan(const TransactionPlan& plan) -> ShardSpanInfo {
    std::set<ShardId> affected_shards;

    for (const auto& stmt : plan.statements) {
        auto table = catalog_->getTable(stmt.table_name);
        auto partition_key = table->getPartitionKey();

        // Extract partition key value from WHERE or VALUES clause
        auto partition_value = stmt.extractPartitionKeyValue(partition_key);

        if (!partition_value.has_value()) {
            // Partition key not specified → Full table scan (all shards)
            return ShardSpanInfo{.span_type = SpanType::ALL_SHARDS};
        }

        // Compute shard ID
        auto shard_id = hashPartitionKey(partition_value.value()) % num_shards_;
        affected_shards.insert(shard_id);
    }

    if (affected_shards.size() == 1) {
        return ShardSpanInfo{.span_type = SpanType::SINGLE_SHARD, .shards = affected_shards};
    } else {
        return ShardSpanInfo{.span_type = SpanType::MULTI_SHARD, .shards = affected_shards};
    }
}
```

**Example Detection**:

```sql
-- Single-shard transaction (user_id='user_123' in all statements)
BEGIN TRANSACTION;
INSERT INTO users (user_id, name) VALUES ('user_123', 'Alice');
INSERT INTO orders (order_id, user_id, amount) VALUES ('order_456', 'user_123', 100.0);
COMMIT;
-- Detection: Single shard (Shard 7)

-- Multi-shard transaction (different user_ids)
BEGIN TRANSACTION;
UPDATE orders SET status='shipped' WHERE user_id='user_123';  -- Shard 7
UPDATE orders SET status='shipped' WHERE user_id='user_789';  -- Shard 3
COMMIT;
-- Detection: Multi-shard (Shard 7, Shard 3) → Use saga or 2PC
```

### Firebird MGA Integration

**Local Transaction Uses TIP**:

```cpp
auto ShardTransactionManager::executeLocalTransaction(
    const std::vector<Statement>& statements
) -> Status {
    // 1. Allocate transaction ID (TIP)
    uint64_t xid = tip_manager_->allocateXid();

    // 2. Mark transaction as ACTIVE in TIP
    tip_manager_->setTransactionState(xid, TransactionState::ACTIVE);

    // 3. Execute statements (visibility via TIP)
    for (const auto& stmt : statements) {
        auto status = executor_.execute(stmt, xid);
        if (!status.ok()) {
            // Rollback: Mark TIP as ABORTED
            tip_manager_->setTransactionState(xid, TransactionState::ABORTED);
            return status;
        }
    }

    // 4. Commit: Mark TIP as COMMITTED (group commit optimization)
    tip_manager_->commitTransaction(xid);

    return Status::OK();
}
```

**Visibility** (MGA, not MVCC):
- Reader transaction checks TIP: `isVersionVisible(version_xid, reader_xid)`
- If version_xid COMMITTED and < reader_xid → Visible
- If version_xid ACTIVE or ABORTED → Invisible

**No Snapshots**: TIP-based visibility (Firebird MGA), not snapshot isolation (PostgreSQL MVCC).

---

## Cross-Partition Transactions

### When to Use Sagas

**Scenario 1: Cross-Aggregate Updates**

```sql
-- Transfer money between two users (different partition keys)
BEGIN TRANSACTION;
UPDATE accounts SET balance = balance - 100 WHERE user_id = 'user_A';  -- Shard 1
UPDATE accounts SET balance = balance + 100 WHERE user_id = 'user_B';  -- Shard 2
COMMIT;
```

**Problem**: Two shards involved, no colocation.

**Solution**: Saga pattern (eventual consistency).

**Scenario 2: Full Table Scans**

```sql
-- Update all orders (no partition key filter)
BEGIN TRANSACTION;
UPDATE orders SET discount = 0.1 WHERE status = 'pending';  -- All shards
COMMIT;
```

**Problem**: All shards involved (scatter-gather).

**Solution**: Execute as separate transactions per shard (eventual consistency).

### Saga Pattern

**Saga Definition**: Sequence of local transactions with compensating transactions for rollback.

**Example: Money Transfer Saga**

**Forward Steps**:
1. Debit Account A: `UPDATE accounts SET balance = balance - 100 WHERE user_id='A'`
2. Credit Account B: `UPDATE accounts SET balance = balance + 100 WHERE user_id='B'`

**Compensating Steps** (if failure occurs):
1. Rollback Credit B: `UPDATE accounts SET balance = balance - 100 WHERE user_id='B'`
2. Rollback Debit A: `UPDATE accounts SET balance = balance + 100 WHERE user_id='A'`

**Saga Execution**:

```cpp
class SagaCoordinator {
public:
    auto executeSaga(const Saga& saga) -> Status {
        std::vector<CompletedStep> completed_steps;

        // Execute forward steps
        for (const auto& step : saga.steps) {
            auto status = executeStep(step);
            if (!status.ok()) {
                // Failure: Execute compensating transactions
                for (auto it = completed_steps.rbegin(); it != completed_steps.rend(); ++it) {
                    executeCompensatingStep(*it);
                }
                return Status::SagaFailed(step.name);
            }
            completed_steps.push_back({step, status});
        }

        return Status::OK();
    }

private:
    auto executeStep(const SagaStep& step) -> Status {
        // Execute local transaction on target shard
        return shard_client_->executeTransaction(step.shard_id, step.statements);
    }

    auto executeCompensatingStep(const CompletedStep& step) -> void {
        // Execute compensating transaction (best-effort)
        shard_client_->executeTransaction(step.shard_id, step.compensating_statements);
    }
};
```

**Trade-offs**:
- **Pro**: High availability (no global lock), eventual consistency
- **Con**: Application complexity (define compensating transactions), no ACID

### Two-Phase Commit (Optional)

**Use Case**: Strong consistency required, willing to sacrifice availability.

**Configuration**:

```sql
-- Enable 2PC for specific transaction
BEGIN TRANSACTION WITH (consistency = 'strong');
UPDATE accounts SET balance = balance - 100 WHERE user_id='A';
UPDATE accounts SET balance = balance + 100 WHERE user_id='B';
COMMIT;
```

**Implementation** (reuse existing 2PC in transaction_manager.cpp):

```cpp
auto TransactionCoordinator::executeDistributedTransaction(
    const std::vector<Statement>& statements
) -> Status {
    // 1. Prepare phase
    std::vector<ShardId> participants;
    for (const auto& stmt : statements) {
        auto shard_id = determineShardId(stmt);
        auto status = shard_client_->sendPrepare(shard_id, stmt);
        if (!status.ok()) {
            // Abort all participants
            for (const auto& shard : participants) {
                shard_client_->sendAbort(shard);
            }
            return Status::PreparePhaseAborted();
        }
        participants.push_back(shard_id);
    }

    // 2. Commit phase
    for (const auto& shard : participants) {
        shard_client_->sendCommit(shard);
    }

    return Status::OK();
}
```

**Decision Matrix**:

| Transaction Type | Recommended Approach | Rationale |
|------------------|---------------------|-----------|
| Single-shard (colocated) | Local transaction | Fast, ACID, no coordination |
| Cross-shard (read-heavy) | Read from multiple shards, no transaction | Eventual consistency OK for reads |
| Cross-shard (write, eventual consistency OK) | Saga pattern | High availability, compensating transactions |
| Cross-shard (write, strong consistency required) | 2PC | ACID, but slow and blocks on failure |

---

## Foreign Key Optimization

### Automatic Colocation Suggestion

**Problem**: Users may not know optimal partition key for colocation.

**Solution**: Analyze foreign key relationships, suggest colocation.

**Example**:

```sql
-- User creates tables without explicit partition key
CREATE TABLE users (
    user_id UUID PRIMARY KEY,
    name VARCHAR(100)
);

CREATE TABLE orders (
    order_id UUID PRIMARY KEY,
    user_id UUID NOT NULL REFERENCES users(user_id),
    amount DECIMAL(10,2)
);

-- System detects FK: orders.user_id → users.user_id
-- Suggestion: PARTITION orders BY user_id (colocate with users)
```

**Catalog Analysis**:

```cpp
auto CatalogAnalyzer::suggestColocation(const std::string& table_name) -> std::vector<Suggestion> {
    auto table = catalog_->getTable(table_name);
    auto foreign_keys = table->getForeignKeys();

    std::vector<Suggestion> suggestions;

    for (const auto& fk : foreign_keys) {
        auto referenced_table = catalog_->getTable(fk.referenced_table);
        auto referenced_partition_key = referenced_table->getPartitionKey();

        // Suggest colocating on foreign key column
        suggestions.push_back({
            .type = SuggestionType::COLOCATION,
            .message = fmt::format(
                "Consider partitioning '{}' by '{}' to colocate with '{}'",
                table_name, fk.column, fk.referenced_table
            ),
            .sql = fmt::format(
                "ALTER TABLE {} SET PARTITION KEY ({})",
                table_name, fk.column
            )
        });
    }

    return suggestions;
}
```

**User Experience**:

```sql
-- User creates orders table
CREATE TABLE orders (
    order_id UUID PRIMARY KEY,
    user_id UUID NOT NULL REFERENCES users(user_id),
    amount DECIMAL(10,2)
);

-- System warning:
-- [INFO] Table 'orders' has foreign key to 'users' on column 'user_id'.
-- [SUGGESTION] Partition 'orders' by 'user_id' for multi-table transactions:
--   ALTER TABLE orders SET PARTITION KEY (user_id);
```

### Colocation Group Management

**Explicit Colocation Group**:

```sql
-- Create colocation group
CREATE COLOCATION GROUP user_aggregate;

-- Assign tables to group
CREATE TABLE users (...) COLOCATION GROUP user_aggregate;
CREATE TABLE orders (...) COLOCATION GROUP user_aggregate;
CREATE TABLE payments (...) COLOCATION GROUP user_aggregate;

-- Result: All three tables guaranteed colocated
```

**Catalog Schema**:

```sql
CREATE TABLE sys_colocation_groups (
    group_id UUID PRIMARY KEY,
    group_name VARCHAR(255) UNIQUE,
    partition_key_columns TEXT,  -- Enforced across all tables in group
    created_at BIGINT
);

CREATE TABLE sys_table_colocation (
    table_id UUID REFERENCES sys_tables(table_id),
    group_id UUID REFERENCES sys_colocation_groups(group_id),
    PRIMARY KEY (table_id)
);
```

**Validation**:

```cpp
auto CatalogManager::validateColocationGroup(
    const std::string& table_name,
    const std::string& group_name
) -> Status {
    auto group = getColocationGroup(group_name);
    auto table = getTable(table_name);

    // Verify partition key matches group's partition key
    if (table.partition_key_columns != group.partition_key_columns) {
        return Status::InvalidColocation(
            fmt::format("Table '{}' partition key '{}' does not match group '{}' partition key '{}'",
                        table_name, table.partition_key_columns, group_name, group.partition_key_columns)
        );
    }

    return Status::OK();
}
```

---

## Integration with SBCLUSTER-05

### SBCLUSTER-05 Sharding Overview

**Existing Specification**: `/docs/specifications/Cluster Specification Work/SBCLUSTER-05-SHARDING.md`

**Key Concepts**:
- **Shard**: Horizontal partition of data (shard_id = hash(key) % num_shards)
- **Shard Map**: Mapping from shard_id to node(s) storing replicas
- **Consistent Hashing**: Distribute shards across nodes, minimize rebalancing

**SBCLUSTER-05 Limitations** (without colocation):
- Sharding by primary key only (no custom partition key)
- No multi-table atomicity guarantees

### Extension: Partition-Key Sharding

**Enhancement**: Allow custom partition key (not just primary key).

**Shard Assignment Algorithm**:

```cpp
auto ShardRouter::computeShardId(
    const std::string& table_name,
    const TypedValue& partition_key_value
) -> ShardId {
    auto table = catalog_->getTable(table_name);
    auto num_shards = shard_map_->getNumShards();

    // Hash partition key value
    uint64_t hash = xxhash64(partition_key_value.serialize());

    // Modulo to get shard ID
    return hash % num_shards;
}
```

**Shard Map Update**:

```
Before (SBCLUSTER-05 only):
  Shard Map: {shard_id → node(s)}
  Sharding: shard_id = hash(primary_key) % num_shards

After (with colocation):
  Shard Map: {shard_id → node(s)}
  Sharding: shard_id = hash(partition_key) % num_shards
  Partition Key: user-defined (table-specific)
```

### Query Routing

**Single-Shard Query** (partition key in WHERE):

```sql
-- Query: SELECT * FROM orders WHERE user_id = 'user_123'
-- Routing:
--   1. Extract partition key: user_id='user_123'
--   2. Compute shard: hash('user_123') % 16 = Shard 7
--   3. Send query to Shard 7 only (no scatter-gather)
```

**Multi-Shard Query** (no partition key in WHERE):

```sql
-- Query: SELECT * FROM orders WHERE status = 'pending'
-- Routing:
--   1. No partition key in WHERE → All shards
--   2. Send query to all 16 shards (scatter-gather)
--   3. Merge results from all shards
```

**Routing Pseudocode**:

```cpp
auto QueryRouter::routeQuery(const SelectStatement& stmt) -> QueryRoute {
    auto table = catalog_->getTable(stmt.table_name);
    auto partition_key = table->getPartitionKey();

    // Extract partition key value from WHERE clause
    auto partition_value = stmt.whereClause.extractValue(partition_key);

    if (partition_value.has_value()) {
        // Single-shard query
        auto shard_id = computeShardId(stmt.table_name, partition_value.value());
        return QueryRoute{.type = RouteType::SINGLE_SHARD, .shards = {shard_id}};
    } else {
        // Multi-shard query (scatter-gather)
        auto all_shards = shard_map_->getAllShards();
        return QueryRoute{.type = RouteType::SCATTER_GATHER, .shards = all_shards};
    }
}
```

### Shard Rebalancing

**Challenge**: When adding/removing nodes, shards must be rebalanced (moved between nodes).

**Colocation Constraint**: All tables in colocation group must move together.

**Rebalancing Algorithm**:

```cpp
auto ShardRebalancer::rebalanceColocationGroup(
    const ColocationGroup& group,
    ShardId shard_id,
    NodeId from_node,
    NodeId to_node
) -> Status {
    // 1. Pause writes to this shard (read-only mode)
    shard_map_->setShard State(shard_id, ShardState::READ_ONLY);

    // 2. Move all tables in colocation group (atomic)
    for (const auto& table_id : group.tables) {
        auto status = moveShardData(table_id, shard_id, from_node, to_node);
        if (!status.ok()) {
            // Rollback: Move already-transferred tables back
            rollbackRebalance(group, shard_id, to_node, from_node);
            return status;
        }
    }

    // 3. Update shard map (atomic)
    shard_map_->updateShardLocation(shard_id, to_node);

    // 4. Resume writes
    shard_map_->setShardState(shard_id, ShardState::WRITABLE);

    return Status::OK();
}
```

---

## Query Planning

### Join Optimization

**Colocated Join** (same partition key):

```sql
-- Query: Join users and orders (both partition by user_id)
SELECT u.name, o.amount
FROM users u
JOIN orders o ON u.user_id = o.user_id
WHERE u.user_id = 'user_123';

-- Plan: Single-shard join (local)
-- 1. Route to Shard 7 (hash('user_123') % 16 = 7)
-- 2. Execute join locally on Shard 7 (no network shuffle)
```

**Non-Colocated Join** (different partition keys):

```sql
-- Query: Join orders and products (different partition keys)
SELECT o.order_id, p.product_name
FROM orders o
JOIN products p ON o.product_id = p.product_id
WHERE o.user_id = 'user_123';

-- Plan: Multi-shard join (shuffle)
-- 1. Fetch orders from Shard 7 (user_id='user_123')
-- 2. Extract product_ids from orders: ['prod_1', 'prod_2', ...]
-- 3. Fetch products from multiple shards (hash each product_id)
-- 4. Merge results on coordinator
```

**Query Planner Logic**:

```cpp
auto QueryPlanner::planJoin(const JoinNode& join) -> JoinPlan {
    auto left_table = catalog_->getTable(join.left_table);
    auto right_table = catalog_->getTable(join.right_table);

    // Check if join is on partition key
    bool colocated = (
        join.left_column == left_table.partition_key &&
        join.right_column == right_table.partition_key
    );

    if (colocated) {
        // Single-shard join
        return JoinPlan{.type = JoinType::LOCAL, .shuffle_required = false};
    } else {
        // Multi-shard join (shuffle)
        return JoinPlan{.type = JoinType::DISTRIBUTED, .shuffle_required = true};
    }
}
```

### Aggregate Pushdown

**Colocated Aggregation**:

```sql
-- Query: Aggregate orders per user (partition key = user_id)
SELECT user_id, SUM(amount)
FROM orders
WHERE user_id = 'user_123'
GROUP BY user_id;

-- Plan: Single-shard aggregation (local)
-- 1. Route to Shard 7
-- 2. Compute SUM locally on Shard 7
```

**Non-Colocated Aggregation**:

```sql
-- Query: Aggregate orders by status (not partition key)
SELECT status, COUNT(*)
FROM orders
GROUP BY status;

-- Plan: Multi-shard aggregation (map-reduce)
-- 1. Send query to all shards (scatter)
-- 2. Each shard computes local COUNT per status
-- 3. Coordinator merges results (reduce)
--    status='pending' → Shard 1: 100, Shard 2: 150 → Total: 250
```

---

## Testing and Verification

### Unit Tests

**Partition Key Detection**:

```cpp
TEST(SchemaColocation, PartitionKeyExtraction) {
    CatalogManager catalog;
    catalog.execute("CREATE TABLE users (user_id UUID PRIMARY KEY, name VARCHAR) PARTITION BY user_id");

    auto table = catalog.getTable("users");
    ASSERT_EQ(table.partition_key_columns, std::vector<std::string>{"user_id"});
}
```

**Shard Routing**:

```cpp
TEST(SchemaColocation, SingleShardRouting) {
    ShardRouter router(/*num_shards=*/16);

    auto shard_id = router.computeShardId("users", TypedValue("user_123"));
    ASSERT_EQ(shard_id, 7);  // hash('user_123') % 16 = 7

    // Same partition key → Same shard
    auto shard_id_2 = router.computeShardId("orders", TypedValue("user_123"));
    ASSERT_EQ(shard_id_2, 7);  // Colocated!
}
```

### Integration Tests

**Multi-Table Transaction (Colocated)**:

```cpp
TEST(SchemaColocationIntegration, ColocatedMultiTableTransaction) {
    Database db(/*num_shards=*/16);

    db.execute("CREATE TABLE users (user_id UUID PRIMARY KEY, name VARCHAR) PARTITION BY user_id");
    db.execute("CREATE TABLE orders (order_id UUID PRIMARY KEY, user_id UUID, amount DECIMAL) PARTITION BY user_id");

    // Transaction: Insert user + order (same partition key)
    auto status = db.executeTransaction({
        "BEGIN TRANSACTION",
        "INSERT INTO users (user_id, name) VALUES ('user_123', 'Alice')",
        "INSERT INTO orders (order_id, user_id, amount) VALUES ('order_456', 'user_123', 100.0)",
        "COMMIT"
    });

    ASSERT_TRUE(status.ok());

    // Verify both rows on same shard
    auto shard_id = db.getShardForRow("users", "user_123");
    ASSERT_EQ(shard_id, 7);
    ASSERT_TRUE(db.shard(7).hasRow("users", "user_123"));
    ASSERT_TRUE(db.shard(7).hasRow("orders", "order_456"));
}
```

**Cross-Partition Transaction (Saga)**:

```cpp
TEST(SchemaColocationIntegration, CrossPartitionSaga) {
    Database db(/*num_shards=*/16);

    db.execute("CREATE TABLE accounts (account_id UUID PRIMARY KEY, balance DECIMAL) PARTITION BY account_id");

    // Insert accounts on different shards
    db.execute("INSERT INTO accounts VALUES ('account_A', 1000.0)");  // Shard 3
    db.execute("INSERT INTO accounts VALUES ('account_B', 500.0)");   // Shard 9

    // Saga: Transfer 100 from A to B
    Saga saga;
    saga.addStep(SagaStep{
        .shard_id = 3,
        .statement = "UPDATE accounts SET balance = balance - 100 WHERE account_id='account_A'",
        .compensating = "UPDATE accounts SET balance = balance + 100 WHERE account_id='account_A'"
    });
    saga.addStep(SagaStep{
        .shard_id = 9,
        .statement = "UPDATE accounts SET balance = balance + 100 WHERE account_id='account_B'",
        .compensating = "UPDATE accounts SET balance = balance - 100 WHERE account_id='account_B'"
    });

    auto status = db.executeSaga(saga);
    ASSERT_TRUE(status.ok());

    // Verify balances
    auto balance_a = db.query("SELECT balance FROM accounts WHERE account_id='account_A'");
    ASSERT_EQ(balance_a, 900.0);
    auto balance_b = db.query("SELECT balance FROM accounts WHERE account_id='account_B'");
    ASSERT_EQ(balance_b, 600.0);
}
```

### Performance Tests

**Local Transaction Latency** (target: < 5ms):

```bash
# Benchmark: 1M colocated transactions (users + orders)
./scratchbird_benchmark --mode=colocated_transaction --transactions=1000000
# Expected: p99 < 5ms
```

**Join Performance** (colocated vs non-colocated):

```bash
# Colocated join: users JOIN orders ON user_id
./scratchbird_benchmark --mode=colocated_join --rows=10000000
# Expected: < 100ms per query

# Non-colocated join: orders JOIN products ON product_id
./scratchbird_benchmark --mode=distributed_join --rows=10000000
# Expected: 500ms - 2s per query (acceptable for cross-partition)
```

---

## Appendix: Industry Comparisons

### Google Spanner

**Approach**: Interleaved tables (explicit parent-child hierarchy)

```sql
-- Spanner syntax
CREATE TABLE users (user_id INT64, name STRING) PRIMARY KEY (user_id);
CREATE TABLE orders (
    user_id INT64,
    order_id INT64,
    amount FLOAT64
) PRIMARY KEY (user_id, order_id),
  INTERLEAVE IN PARENT users ON DELETE CASCADE;
```

**Similarities**:
- Colocation via schema declaration
- Multi-table atomicity for colocated tables

**Differences**:
- Spanner uses hierarchical interleaving (parent-child only)
- ScratchBird uses partition keys (flexible, multi-table groups)

### YugabyteDB

**Approach**: Colocated tables (all tables in database share same tablet)

```sql
-- YugabyteDB syntax
CREATE DATABASE my_db WITH colocated = true;
-- All tables in my_db colocated on same tablet
```

**Similarities**:
- Colocation for multi-table transactions

**Differences**:
- YugabyteDB colocation is database-level (all or nothing)
- ScratchBird colocation is table-level (fine-grained control)

### CockroachDB

**Approach**: Range partitioning with locality (no automatic colocation)

**User Responsibility**: Manually ensure related data has same partition key

**Similarities**:
- Partition key concept (user-defined)

**Differences**:
- CockroachDB no automatic foreign key colocation
- ScratchBird suggests colocation based on FK relationships

---

**Document Status:** DRAFT (Beta Specification Phase)
**Last Updated:** 2026-01-09
**Next Review:** Weekly during implementation
**Dependencies:** SBCLUSTER-05 (Sharding), transaction_manager.cpp (TIP-based transactions)

---

**End of Document 03**
