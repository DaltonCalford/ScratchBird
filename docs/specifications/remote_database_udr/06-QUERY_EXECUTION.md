# Remote Database UDR - Query Execution Layer

> Reference-only: Canonical UDR and live-migration behavior now lives in
> `ScratchBird/docs/specifications/Alpha Phase 2/11-Remote-Database-UDR-Specification.md`
> and `ScratchBird/docs/specifications/Alpha Phase 2/11h-Live-Migration-Emulated-Listener.md`.

## 1. Overview

The Query Execution Layer sits between the ScratchBird query planner and the protocol adapters, handling:
- Query pushdown decisions
- SQL dialect translation
- Result aggregation from multiple sources
- Transaction coordination

**Scope Note:** MSSQL/TDS adapter support is post-gold; MSSQL references are forward-looking.
**WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional post-gold (replication/PITR).
Any WAL references in this document describe an optional post-gold stream for
replication/PITR only.

---

## 2. Architecture

```
┌─────────────────────────────────────────────────────────┐
│  ScratchBird Query Planner                              │
│  - Identifies foreign tables                            │
│  - Determines pushdown opportunities                    │
│  - Estimates costs                                      │
└─────────────────┬───────────────────────────────────────┘
                  │ ForeignScan node
┌─────────────────▼───────────────────────────────────────┐
│  RemoteQueryExecutor                                    │
│  ┌──────────────────────────────────────────────────┐  │
│  │  QueryPushdownAnalyzer                           │  │
│  │  - Analyzes which operations can be pushed       │  │
│  │  - Splits query into local/remote parts          │  │
│  └──────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────┐  │
│  │  SQLDialectTranslator                            │  │
│  │  - Converts ScratchBird SQL to remote dialect    │  │
│  │  - Handles identifier quoting                    │  │
│  │  - Translates functions and operators            │  │
│  └──────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────┐  │
│  │  ResultAggregator                                │  │
│  │  - Combines results from multiple sources        │  │
│  │  - Handles distributed JOINs                     │  │
│  │  - Applies local filtering if needed             │  │
│  └──────────────────────────────────────────────────┘  │
└─────────────────┬───────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────┐
│  Connection Pool → Protocol Adapters                    │
└─────────────────────────────────────────────────────────┘
```

---

## 3. RemoteQueryExecutor

### 3.1 Class Definition

```cpp
class RemoteQueryExecutor {
public:
    RemoteQueryExecutor(RemoteConnectionPoolRegistry* pool_registry);

    // Execute a query against a foreign table
    Result<RemoteQueryResult> execute(
        const ForeignScanContext& context,
        const std::string& server_name,
        const std::string& local_user);

    // Execute a raw SQL query against a server
    Result<RemoteQueryResult> executeRaw(
        const std::string& server_name,
        const std::string& local_user,
        const std::string& sql);

    // Execute parameterized query
    Result<RemoteQueryResult> executeParameterized(
        const std::string& server_name,
        const std::string& local_user,
        const std::string& sql,
        const std::vector<TypedValue>& params);

    // DML operations
    Result<uint64_t> executeInsert(
        const ForeignTableDefinition& table,
        const std::vector<std::string>& columns,
        const std::vector<std::vector<TypedValue>>& rows);

    Result<uint64_t> executeUpdate(
        const ForeignTableDefinition& table,
        const std::vector<std::pair<std::string, TypedValue>>& assignments,
        const Expression* where_clause);

    Result<uint64_t> executeDelete(
        const ForeignTableDefinition& table,
        const Expression* where_clause);

private:
    RemoteConnectionPoolRegistry* pool_registry_;
    std::unique_ptr<QueryPushdownAnalyzer> pushdown_analyzer_;
    std::unordered_map<RemoteDatabaseType, std::unique_ptr<SQLDialectTranslator>>
        dialect_translators_;

    Result<std::string> translateQuery(
        const ForeignScanContext& context,
        RemoteDatabaseType db_type);
};
```

### 3.2 ForeignScanContext

```cpp
struct ForeignScanContext {
    // Table info
    const ForeignTableDefinition* table;
    const ServerDefinition* server;

    // Columns to retrieve
    std::vector<std::string> target_columns;

    // Filters that may be pushed down
    std::vector<const Expression*> filter_clauses;

    // Sort specifications
    std::vector<SortSpec> sort_specs;

    // Limit/Offset
    std::optional<uint64_t> limit;
    std::optional<uint64_t> offset;

    // Aggregate operations
    std::vector<AggregateSpec> aggregates;

    // Group by
    std::vector<std::string> group_by_columns;

    // Having clause
    const Expression* having_clause = nullptr;

    // For JOINs with other foreign tables
    std::vector<const ForeignScanContext*> join_tables;
    std::vector<JoinSpec> join_conditions;

    // Execution hints
    uint32_t fetch_size = 1000;
    bool use_cursor = false;
    std::chrono::milliseconds timeout{30000};
};
```

---

## 4. Query Pushdown Analysis

### 4.1 QueryPushdownAnalyzer

```cpp
class QueryPushdownAnalyzer {
public:
    struct PushdownDecision {
        // What can be pushed to remote
        std::vector<const Expression*> pushable_filters;
        std::vector<SortSpec> pushable_sorts;
        bool push_limit = false;
        bool push_offset = false;
        std::vector<AggregateSpec> pushable_aggregates;
        std::vector<std::string> pushable_group_by;
        const Expression* pushable_having = nullptr;

        // What must be done locally
        std::vector<const Expression*> local_filters;
        std::vector<SortSpec> local_sorts;
        std::optional<uint64_t> local_limit;
        std::optional<uint64_t> local_offset;
        std::vector<AggregateSpec> local_aggregates;

        // Cost estimate
        double remote_cost;
        double local_cost;
        double total_cost;
    };

    PushdownDecision analyze(
        const ForeignScanContext& context,
        PushdownCapability capabilities);

private:
    // Check if expression can be evaluated remotely
    bool isExpressionPushable(
        const Expression* expr,
        PushdownCapability caps,
        RemoteDatabaseType db_type);

    // Check if function is supported on remote
    bool isFunctionSupported(
        const std::string& func_name,
        RemoteDatabaseType db_type);

    // Check if operator is supported
    bool isOperatorSupported(
        const std::string& op_name,
        RemoteDatabaseType db_type);

    // Estimate rows returned by filter
    double estimateSelectivity(const Expression* filter);

    // Cost models
    double estimateRemoteCost(const PushdownDecision& decision,
                               const ForeignScanContext& context);
    double estimateLocalCost(const PushdownDecision& decision,
                              uint64_t estimated_rows);
};
```

### 4.2 Pushdown Rules

```cpp
// Per-database function support maps
const std::unordered_map<std::string, std::set<RemoteDatabaseType>>
    supported_functions = {
    // String functions
    {"UPPER", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"LOWER", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"LENGTH", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"SUBSTRING", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"TRIM", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"CONCAT", {POSTGRESQL, MYSQL, MSSQL, SCRATCHBIRD}},  // Firebird uses ||
    {"REPLACE", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"POSITION", {POSTGRESQL, MYSQL, FIREBIRD, SCRATCHBIRD}},  // MSSQL: CHARINDEX

    // Numeric functions
    {"ABS", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"ROUND", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"FLOOR", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"CEIL", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"POWER", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"SQRT", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"MOD", {POSTGRESQL, MYSQL, FIREBIRD, SCRATCHBIRD}},  // MSSQL: %

    // Date/Time functions
    {"CURRENT_DATE", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"CURRENT_TIME", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"CURRENT_TIMESTAMP", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"EXTRACT", {POSTGRESQL, MYSQL, FIREBIRD, SCRATCHBIRD}},  // MSSQL: DATEPART
    {"DATE_TRUNC", {POSTGRESQL, SCRATCHBIRD}},

    // Aggregate functions
    {"COUNT", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"SUM", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"AVG", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"MIN", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"MAX", {POSTGRESQL, MYSQL, MSSQL, FIREBIRD, SCRATCHBIRD}},
    {"ARRAY_AGG", {POSTGRESQL, SCRATCHBIRD}},
    {"STRING_AGG", {POSTGRESQL, MSSQL, SCRATCHBIRD}},  // MySQL: GROUP_CONCAT
};

// Operators that need translation
const std::unordered_map<std::pair<std::string, RemoteDatabaseType>, std::string>
    operator_translations = {
    {{"||", MYSQL}, "CONCAT"},           // String concatenation
    {{"||", MSSQL}, "+"},
    {{"ILIKE", MYSQL}, "LIKE"},          // Case-insensitive LIKE
    {{"ILIKE", MSSQL}, "LIKE"},          // + COLLATE
    {{"~", MYSQL}, "REGEXP"},            // Regex match
    {{"~*", MYSQL}, "REGEXP"},
    {{"@>", MYSQL}, "JSON_CONTAINS"},    // JSON containment
};
```

---

## 5. SQL Dialect Translation

### 5.1 SQLDialectTranslator Interface

```cpp
class SQLDialectTranslator {
public:
    virtual ~SQLDialectTranslator() = default;

    // Main translation entry point
    virtual std::string translate(const ForeignScanContext& context) = 0;

    // Component translation
    virtual std::string translateExpression(const Expression* expr) = 0;
    virtual std::string translateFunction(const FunctionCall* func) = 0;
    virtual std::string translateOperator(const BinaryExpr* op) = 0;
    virtual std::string translateCast(const CastExpr* cast) = 0;

    // Identifier quoting
    virtual std::string quoteIdentifier(const std::string& name) = 0;
    virtual std::string quoteLiteral(const std::string& value) = 0;

    // Type name translation
    virtual std::string translateTypeName(ScratchBirdType type) = 0;
};
```

### 5.2 PostgreSQL Translator

```cpp
class PostgreSQLDialectTranslator : public SQLDialectTranslator {
public:
    std::string translate(const ForeignScanContext& context) override {
        std::stringstream sql;

        // SELECT clause
        sql << "SELECT ";
        if (!context.aggregates.empty()) {
            sql << translateAggregates(context.aggregates);
        } else {
            sql << translateColumns(context.target_columns);
        }

        // FROM clause
        sql << " FROM ";
        sql << quoteIdentifier(context.table->remote_schema) << ".";
        sql << quoteIdentifier(context.table->remote_name);

        // WHERE clause
        if (!context.filter_clauses.empty()) {
            sql << " WHERE ";
            for (size_t i = 0; i < context.filter_clauses.size(); ++i) {
                if (i > 0) sql << " AND ";
                sql << translateExpression(context.filter_clauses[i]);
            }
        }

        // GROUP BY
        if (!context.group_by_columns.empty()) {
            sql << " GROUP BY ";
            for (size_t i = 0; i < context.group_by_columns.size(); ++i) {
                if (i > 0) sql << ", ";
                sql << quoteIdentifier(context.group_by_columns[i]);
            }
        }

        // HAVING
        if (context.having_clause) {
            sql << " HAVING " << translateExpression(context.having_clause);
        }

        // ORDER BY
        if (!context.sort_specs.empty()) {
            sql << " ORDER BY ";
            for (size_t i = 0; i < context.sort_specs.size(); ++i) {
                if (i > 0) sql << ", ";
                sql << quoteIdentifier(context.sort_specs[i].column);
                sql << (context.sort_specs[i].ascending ? " ASC" : " DESC");
                sql << (context.sort_specs[i].nulls_first ? " NULLS FIRST" : " NULLS LAST");
            }
        }

        // LIMIT/OFFSET
        if (context.limit) {
            sql << " LIMIT " << *context.limit;
        }
        if (context.offset) {
            sql << " OFFSET " << *context.offset;
        }

        return sql.str();
    }

    std::string quoteIdentifier(const std::string& name) override {
        // PostgreSQL uses double quotes
        std::string result = "\"";
        for (char c : name) {
            if (c == '"') result += "\"\"";
            else result += c;
        }
        return result + "\"";
    }

    std::string translateFunction(const FunctionCall* func) override {
        // Most functions work as-is in PostgreSQL
        return func->name + "(" + translateArgs(func->arguments) + ")";
    }
};
```

### 5.3 MySQL Translator

```cpp
class MySQLDialectTranslator : public SQLDialectTranslator {
public:
    std::string translate(const ForeignScanContext& context) override {
        // Similar to PostgreSQL but with MySQL-specific syntax
        std::stringstream sql;
        // ... build query ...

        // MySQL uses backticks for identifiers
        // Different LIMIT syntax (LIMIT offset, count) possible
        return sql.str();
    }

    std::string quoteIdentifier(const std::string& name) override {
        // MySQL uses backticks
        std::string result = "`";
        for (char c : name) {
            if (c == '`') result += "``";
            else result += c;
        }
        return result + "`";
    }

    std::string translateFunction(const FunctionCall* func) override {
        // Translate function names that differ
        std::string name = func->name;

        if (name == "STRING_AGG") {
            // MySQL uses GROUP_CONCAT
            return "GROUP_CONCAT(" + translateArgs(func->arguments) +
                   " SEPARATOR " + quoteLiteral(", ") + ")";
        }
        if (name == "POSITION") {
            // MySQL: LOCATE(substr, str)
            return "LOCATE(" + translateArgs(func->arguments) + ")";
        }

        return name + "(" + translateArgs(func->arguments) + ")";
    }

    std::string translateOperator(const BinaryExpr* op) override {
        if (op->op == "||") {
            // MySQL: CONCAT(left, right)
            return "CONCAT(" + translateExpression(op->left) + ", " +
                   translateExpression(op->right) + ")";
        }
        if (op->op == "ILIKE") {
            // Case-insensitive LIKE (depends on collation)
            return translateExpression(op->left) + " LIKE " +
                   translateExpression(op->right);
        }
        return translateExpression(op->left) + " " + op->op + " " +
               translateExpression(op->right);
    }
};
```

### 5.4 MSSQL Translator (post-gold)

```cpp
class MSSQLDialectTranslator : public SQLDialectTranslator {
public:
    std::string translate(const ForeignScanContext& context) override {
        std::stringstream sql;

        // MSSQL uses TOP instead of LIMIT (or OFFSET FETCH in newer versions)
        sql << "SELECT ";

        // TOP N for simple LIMIT without OFFSET
        if (context.limit && !context.offset && context.sort_specs.empty()) {
            sql << "TOP " << *context.limit << " ";
        }

        // ... rest of query ...

        // OFFSET FETCH syntax (SQL Server 2012+)
        if (context.offset || (context.limit && !context.sort_specs.empty())) {
            // ORDER BY is required for OFFSET FETCH
            if (context.sort_specs.empty()) {
                sql << " ORDER BY (SELECT NULL)";  // Arbitrary order
            }
            sql << " OFFSET " << context.offset.value_or(0) << " ROWS";
            if (context.limit) {
                sql << " FETCH NEXT " << *context.limit << " ROWS ONLY";
            }
        }

        return sql.str();
    }

    std::string quoteIdentifier(const std::string& name) override {
        // MSSQL uses square brackets
        return "[" + name + "]";
    }

    std::string translateFunction(const FunctionCall* func) override {
        std::string name = func->name;

        if (name == "LENGTH") return "LEN(" + translateArgs(func->arguments) + ")";
        if (name == "POSITION") {
            return "CHARINDEX(" + translateArgs(func->arguments) + ")";
        }
        if (name == "EXTRACT") {
            // DATEPART(part, date)
            return "DATEPART(" + translateArgs(func->arguments) + ")";
        }
        if (name == "CONCAT") {
            // MSSQL supports CONCAT but also + for strings
            return "CONCAT(" + translateArgs(func->arguments) + ")";
        }

        return name + "(" + translateArgs(func->arguments) + ")";
    }
};
```

---

## 6. Result Aggregation

### 6.1 ResultAggregator

```cpp
class ResultAggregator {
public:
    // Combine results from a single source with local processing
    Result<RemoteQueryResult> aggregate(
        RemoteQueryResult&& remote_result,
        const PushdownDecision& decision,
        const ForeignScanContext& context);

    // Combine results from multiple foreign tables (distributed JOIN)
    Result<RemoteQueryResult> aggregateJoin(
        std::vector<RemoteQueryResult>&& results,
        const std::vector<JoinSpec>& joins,
        const ForeignScanContext& context);

private:
    // Apply local filters to remote results
    void applyLocalFilters(
        RemoteQueryResult& result,
        const std::vector<const Expression*>& filters);

    // Apply local sorting
    void applyLocalSort(
        RemoteQueryResult& result,
        const std::vector<SortSpec>& sorts);

    // Apply local LIMIT/OFFSET
    void applyLocalLimitOffset(
        RemoteQueryResult& result,
        std::optional<uint64_t> limit,
        std::optional<uint64_t> offset);

    // Apply local aggregates
    Result<RemoteQueryResult> applyLocalAggregates(
        RemoteQueryResult& result,
        const std::vector<AggregateSpec>& aggregates,
        const std::vector<std::string>& group_by);

    // Hash join for distributed JOINs
    Result<RemoteQueryResult> hashJoin(
        const RemoteQueryResult& left,
        const RemoteQueryResult& right,
        const JoinSpec& spec);

    // Nested loop join (fallback)
    Result<RemoteQueryResult> nestedLoopJoin(
        const RemoteQueryResult& left,
        const RemoteQueryResult& right,
        const JoinSpec& spec);
};
```

### 6.2 Distributed JOIN Strategies

```cpp
enum class DistributedJoinStrategy {
    // Fetch all data and join locally
    LOCAL_JOIN,

    // Push join to remote if both tables on same server
    REMOTE_JOIN,

    // Hash join with smaller table broadcast
    BROADCAST_JOIN,

    // Partition both tables and join partitions
    PARTITION_JOIN,

    // Use semi-join to reduce data transfer
    SEMI_JOIN,
};

DistributedJoinStrategy chooseJoinStrategy(
    const ForeignTableDefinition& left,
    const ForeignTableDefinition& right,
    const JoinSpec& spec)
{
    // Same server - push entire join remotely
    if (left.server_id == right.server_id) {
        return DistributedJoinStrategy::REMOTE_JOIN;
    }

    // Estimate sizes
    double left_rows = left.estimated_row_count;
    double right_rows = right.estimated_row_count;

    // Small table - broadcast to other side
    if (left_rows < 10000 || right_rows < 10000) {
        return DistributedJoinStrategy::BROADCAST_JOIN;
    }

    // Large tables - try semi-join first
    if (left_rows > 100000 && right_rows > 100000) {
        return DistributedJoinStrategy::SEMI_JOIN;
    }

    // Default to local join
    return DistributedJoinStrategy::LOCAL_JOIN;
}
```

---

## 7. Transaction Coordination

### 7.1 Distributed Transaction Support

```cpp
class DistributedTransactionCoordinator {
public:
    // Begin distributed transaction across multiple servers
    Result<DistributedTransactionId> begin(
        const std::vector<std::string>& server_names);

    // Prepare all participants (2PC phase 1)
    Result<void> prepare(DistributedTransactionId txn_id);

    // Commit all participants (2PC phase 2)
    Result<void> commit(DistributedTransactionId txn_id);

    // Rollback all participants
    Result<void> rollback(DistributedTransactionId txn_id);

    // Check transaction status
    TransactionStatus status(DistributedTransactionId txn_id);

private:
    struct ParticipantState {
        std::string server_name;
        PooledRemoteConnection connection;
        enum { ACTIVE, PREPARED, COMMITTED, ABORTED } state;
    };

    std::unordered_map<DistributedTransactionId,
                       std::vector<ParticipantState>> transactions_;

    // Optional post-gold write-after log (WAL) logging for replication/PITR
    void logPrepare(DistributedTransactionId txn_id,
                    const std::vector<std::string>& servers);
    void logCommit(DistributedTransactionId txn_id);
    void logAbort(DistributedTransactionId txn_id);

    // Recovery on startup
    void recover();
};
```

### 7.2 Read-Only Optimization

```cpp
// For read-only queries, skip 2PC and use snapshot isolation
Result<RemoteQueryResult> executeReadOnly(
    const std::vector<ForeignScanContext>& contexts)
{
    std::vector<std::future<RemoteQueryResult>> futures;

    // Execute all queries in parallel
    for (const auto& ctx : contexts) {
        futures.push_back(std::async([&ctx, this]() {
            auto conn = pool_registry_->acquire(
                ctx.server->server_name,
                current_user_);
            return conn->executeQuery(translateQuery(ctx));
        }));
    }

    // Collect results
    std::vector<RemoteQueryResult> results;
    for (auto& f : futures) {
        results.push_back(f.get());
    }

    // Aggregate if needed
    return result_aggregator_->aggregate(std::move(results));
}
```

---

## 8. Error Handling and Recovery

### 8.1 Error Categories

```cpp
enum class RemoteErrorCategory {
    CONNECTION,      // Network/connection errors
    AUTHENTICATION,  // Auth failures
    QUERY,           // SQL syntax/semantic errors
    TRANSACTION,     // Transaction aborted/deadlock
    TIMEOUT,         // Query/connection timeout
    RESOURCE,        // Resource exhaustion
    INTERNAL,        // Internal/unexpected errors
};

RemoteErrorCategory categorizeError(const RemoteQueryResult& result) {
    if (result.sql_state.substr(0, 2) == "08") return CONNECTION;
    if (result.sql_state.substr(0, 2) == "28") return AUTHENTICATION;
    if (result.sql_state.substr(0, 2) == "42") return QUERY;
    if (result.sql_state.substr(0, 2) == "40") return TRANSACTION;
    // ... more mappings
    return INTERNAL;
}
```

### 8.2 Retry Logic

```cpp
Result<RemoteQueryResult> executeWithRetry(
    const ForeignScanContext& context,
    const std::string& server_name,
    uint32_t max_retries = 3)
{
    uint32_t attempt = 0;
    std::chrono::milliseconds backoff(100);

    while (attempt < max_retries) {
        auto result = execute(context, server_name, current_user_);

        if (result) {
            return result;
        }

        auto category = categorizeError(*result);

        // Only retry for transient errors
        if (category != CONNECTION && category != TIMEOUT) {
            return result;
        }

        attempt++;
        std::this_thread::sleep_for(backoff);
        backoff *= 2;  // Exponential backoff
    }

    return Error(remote_sqlstate::CONNECTION_FAILED,
                 "Max retries exceeded");
}
```
