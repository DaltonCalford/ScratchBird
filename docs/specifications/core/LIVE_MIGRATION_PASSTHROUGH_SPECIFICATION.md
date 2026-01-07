# Live Migration Passthrough Specification

## 1. Overview

### 1.1 Purpose

The Live Migration Passthrough feature enables zero-downtime database migration from legacy databases to ScratchBird. Applications connect to ScratchBird, which transparently routes queries to either the legacy source database or local ScratchBird storage based on per-table migration state.

### 1.2 Goals

- **Zero Downtime:** Applications continue operating throughout migration
- **Transparent:** No application code changes required
- **Incremental:** Tables migrate independently at their own pace
- **Reversible:** Full rollback capability at any stage
- **Validated:** Built-in data integrity verification

### 1.3 Supported Source Databases

| Database | Version | CDC Method |
|----------|---------|------------|
| PostgreSQL | 9.6+ | Logical replication |
| MySQL | 5.7+ / MariaDB 10+ | Binary log (binlog) |
| SQL Server | 2016+ | Change Tracking / CDC |
| Firebird | 2.5+ | Trigger-based capture |
| Oracle | 12c+ | LogMiner / GoldenGate |

### 1.4 Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│  Application Layer (unchanged)                                       │
│  - Connects to ScratchBird using standard protocol                  │
│  - PostgreSQL, MySQL, TDS, Firebird, or Native wire protocol        │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
┌─────────────────────▼───────────────────────────────────────────────┐
│  ScratchBird Migration Gateway                                       │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │  Query Router                                                 │  │
│  │  - Post-semantic analysis interception                        │  │
│  │  - Per-table routing decisions                                │  │
│  │  - Transaction boundary awareness                             │  │
│  └───────────────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │  Migration Controller                                         │  │
│  │  - Background bulk loader                                     │  │
│  │  - CDC stream processor                                       │  │
│  │  - Progress tracker                                           │  │
│  └───────────────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │  Dual-Write Coordinator                                       │  │
│  │  - Synchronous write replication                              │  │
│  │  - Conflict detection and resolution                          │  │
│  │  - 2PC transaction coordination                               │  │
│  └───────────────────────────────────────────────────────────────┘  │
└─────────────────────┬───────────────────────────────────────────────┘
                      │
         ┌────────────┴────────────┐
         │                         │
┌────────▼────────┐       ┌───────▼────────┐
│  ScratchBird    │       │  Legacy DB     │
│  Local Storage  │       │  (Source)      │
└─────────────────┘       └────────────────┘
```

---

## 2. Migration State Machine

### 2.1 Table Migration States

Each table has an independent migration state:

```
                    ┌─────────────┐
                    │ NOT_STARTED │
                    └──────┬──────┘
                           │ START MIGRATION
                           ▼
                    ┌─────────────┐
              ┌─────│ BULK_LOADING│─────┐
              │     └──────┬──────┘     │
              │            │            │ ABORT
              │            │ bulk complete
              │            ▼            │
              │     ┌─────────────┐     │
              │     │SYNCHRONIZING│─────┤
              │     └──────┬──────┘     │
              │            │            │
              │            │ CDC caught up
              │            ▼            │
              │     ┌─────────────┐     │
              │     │ DUAL_WRITE  │─────┤
              │     └──────┬──────┘     │
              │            │            │
              │            │ validation passed
              │            ▼            │
              │     ┌──────────────┐    │
              │     │CUTOVER_READY │────┤
              │     └──────┬───────┘    │
              │            │            │
              │            │ CUTOVER    │
              │            ▼            │
              │     ┌─────────────┐     │
              │     │ LOCAL_ONLY  │     │
              │     └─────────────┘     │
              │                         │
              │     ┌─────────────┐     │
              └────▶│  ROLLBACK   │◀────┘
                    └──────┬──────┘
                           │ complete
                           ▼
                    ┌─────────────┐
                    │ NOT_STARTED │
                    └─────────────┘
```

### 2.2 State Definitions

| State | Description | Query Routing | CDC Active |
|-------|-------------|---------------|------------|
| `NOT_STARTED` | Table exists only in source | All → Remote | No |
| `BULK_LOADING` | Initial data transfer in progress | Reads → Remote, Writes blocked | No |
| `SYNCHRONIZING` | CDC catching up to current state | Reads → Remote, Writes → Remote | Yes |
| `DUAL_WRITE` | Both systems receive writes | Reads → Local, Writes → Both | Yes |
| `CUTOVER_READY` | Validation passed, ready for cutover | Reads → Local, Writes → Both | Yes |
| `LOCAL_ONLY` | Migration complete | All → Local | No |
| `ROLLBACK` | Reverting to source | Reads → Remote, Writes → Remote | No |
| `PAUSED` | Migration temporarily halted | Per previous state | Paused |
| `ERROR` | Migration failed, intervention required | Reads → Remote | Stopped |

### 2.3 State Transitions

```cpp
enum class MigrationState {
    NOT_STARTED,
    BULK_LOADING,
    SYNCHRONIZING,
    DUAL_WRITE,
    CUTOVER_READY,
    LOCAL_ONLY,
    ROLLBACK,
    PAUSED,
    ERROR
};

struct MigrationStateTransition {
    MigrationState from;
    MigrationState to;
    std::string trigger;           // What caused the transition
    std::chrono::system_clock::time_point timestamp;
    std::optional<std::string> error_message;
};
```

### 2.4 State Persistence

Migration state is stored in `SYS$MIGRATION_STATE` system table:

```sql
CREATE TABLE SYS$MIGRATION_STATE (
    table_id            UUID PRIMARY KEY,
    source_server       VARCHAR(128) NOT NULL,
    source_schema       VARCHAR(128) NOT NULL,
    source_table        VARCHAR(128) NOT NULL,
    target_schema       VARCHAR(128) NOT NULL,
    target_table        VARCHAR(128) NOT NULL,
    state               VARCHAR(32) NOT NULL,
    state_changed_at    TIMESTAMP NOT NULL,
    bulk_rows_total     BIGINT,
    bulk_rows_migrated  BIGINT,
    cdc_position        VARCHAR(256),      -- Database-specific position
    cdc_lag_ms          INTEGER,
    last_error          VARCHAR(1024),
    options             JSON,
    created_at          TIMESTAMP NOT NULL,
    updated_at          TIMESTAMP NOT NULL
);

CREATE INDEX idx_migration_state_server ON SYS$MIGRATION_STATE(source_server);
CREATE INDEX idx_migration_state_state ON SYS$MIGRATION_STATE(state);
```

---

## 3. Query Routing Engine

### 3.1 Interception Point

Query routing occurs **after semantic analysis** but **before bytecode execution**:

```
SQL Text
    │
    ▼
┌───────────────────┐
│ Parser (V2)       │
└─────────┬─────────┘
          │ AST
          ▼
┌───────────────────┐
│ Semantic Analyzer │
└─────────┬─────────┘
          │ Resolved AST (tables, types, schemas)
          ▼
┌───────────────────┐
│ QUERY ROUTER      │ ◀── Interception point
│ - Check table states
│ - Determine routing
└─────────┬─────────┘
          │
    ┌─────┴─────┐
    │           │
    ▼           ▼
┌───────┐  ┌─────────────┐
│ Local │  │ Remote/Dual │
│ Exec  │  │ Execution   │
└───────┘  └─────────────┘
```

### 3.2 Routing Decision Matrix

| Query Type | NOT_STARTED | BULK_LOADING | SYNCHRONIZING | DUAL_WRITE | LOCAL_ONLY |
|------------|-------------|--------------|---------------|------------|------------|
| SELECT | Remote | Remote | Remote | Local | Local |
| INSERT | Remote | **Blocked** | Remote + Queue | Both | Local |
| UPDATE | Remote | **Blocked** | Remote + Queue | Both | Local |
| DELETE | Remote | **Blocked** | Remote + Queue | Both | Local |
| DDL | Remote | **Blocked** | **Blocked** | **Blocked** | Local |

### 3.3 QueryRouter Interface

```cpp
class QueryRouter {
public:
    enum class RoutingDecision {
        LOCAL,              // Execute on ScratchBird only
        REMOTE,             // Execute on source only
        DUAL_WRITE,         // Execute on both (writes)
        HYBRID,             // Split execution (cross-system JOIN)
        BLOCKED,            // Operation not allowed in current state
        QUEUED              // Queue for later application
    };

    struct RoutingResult {
        RoutingDecision decision;
        std::vector<TableRouting> table_routes;
        std::optional<std::string> block_reason;
        bool requires_2pc;
    };

    // Main routing decision method
    RoutingResult determineRouting(
        const SemanticResult& semantic_result,
        const ConnectionContext& conn_ctx);

    // Get tables affected by query
    std::vector<ResolvedTableRef> extractTableReferences(
        const SemanticResult& semantic_result);

    // Check if query can be fully pushed to remote
    bool canFullyPushdown(
        const SemanticResult& semantic_result,
        const std::string& source_server);

private:
    MigrationStateManager& state_manager_;

    RoutingDecision determineTableRouting(
        const ResolvedTableRef& table,
        QueryType query_type);

    RoutingDecision combineRoutingDecisions(
        const std::vector<RoutingDecision>& decisions,
        QueryType query_type);
};
```

### 3.4 Table Routing Structure

```cpp
struct TableRouting {
    ID table_id;
    std::string local_schema;
    std::string local_table;
    std::string remote_server;
    std::string remote_schema;
    std::string remote_table;
    MigrationState state;
    RoutingDecision routing;
};
```

### 3.5 Hybrid Query Execution

For queries spanning tables in different migration states:

```cpp
class HybridQueryExecutor {
public:
    // Execute query across local and remote systems
    ExecutionResult execute(
        const SemanticResult& semantic_result,
        const std::vector<TableRouting>& routes,
        const ConnectionContext& conn_ctx);

private:
    // Strategies for cross-system JOINs
    enum class JoinStrategy {
        FETCH_REMOTE,       // Fetch remote data, join locally
        PUSH_LOCAL,         // Push local data to remote, join there
        BROADCAST_SMALLER,  // Broadcast smaller table
        HASH_PARTITION      // Partition both sides by join key
    };

    JoinStrategy selectJoinStrategy(
        const TableRouting& left,
        const TableRouting& right,
        const JoinCondition& condition);
};
```

### 3.6 SQL Dialect Translation

When routing to remote, translate ScratchBird SQL to source dialect:

```cpp
class MigrationSQLTranslator {
public:
    // Translate query for remote execution
    std::string translateForRemote(
        const SemanticResult& semantic_result,
        DatabaseType target_db);

    // Translate INSERT/UPDATE/DELETE for dual-write
    std::pair<std::string, std::string> translateForDualWrite(
        const SemanticResult& semantic_result,
        DatabaseType source_db);

private:
    // Database-specific translators
    std::string translatePostgreSQL(const SemanticResult& result);
    std::string translateMySQL(const SemanticResult& result);
    std::string translateSQLServer(const SemanticResult& result);
    std::string translateFirebird(const SemanticResult& result);
};
```

---

## 4. Background Migration Worker

### 4.1 Migration Worker Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│  MigrationController (Singleton)                                     │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │  WorkerPool                                                   │  │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐             │  │
│  │  │BulkLoader[0]│ │BulkLoader[1]│ │BulkLoader[N]│             │  │
│  │  └─────────────┘ └─────────────┘ └─────────────┘             │  │
│  └───────────────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │  CDC Processors (per source server)                           │  │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐             │  │
│  │  │ PG Logical  │ │MySQL Binlog │ │ MSSQL CDC   │             │  │
│  │  └─────────────┘ └─────────────┘ └─────────────┘             │  │
│  └───────────────────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────────────────┐  │
│  │  Progress Tracker                                             │  │
│  │  - Per-table progress                                         │  │
│  │  - CDC lag monitoring                                         │  │
│  │  - ETA estimation                                             │  │
│  └───────────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────────┘
```

### 4.2 MigrationController Interface

```cpp
class MigrationController {
public:
    static MigrationController& instance();

    // Start migration for a table
    Result<MigrationHandle> startMigration(
        const MigrationConfig& config);

    // Control operations
    Result<void> pauseMigration(const ID& table_id);
    Result<void> resumeMigration(const ID& table_id);
    Result<void> abortMigration(const ID& table_id);
    Result<void> cutoverTable(const ID& table_id);
    Result<void> rollbackMigration(const ID& table_id);

    // Status queries
    MigrationStatus getStatus(const ID& table_id);
    std::vector<MigrationStatus> getAllStatus();
    MigrationProgress getProgress(const ID& table_id);

    // Configuration
    void setGlobalConfig(const MigrationGlobalConfig& config);

private:
    std::unique_ptr<BulkLoaderPool> bulk_pool_;
    std::map<std::string, std::unique_ptr<CDCProcessor>> cdc_processors_;
    MigrationStateManager state_manager_;
    ProgressTracker progress_tracker_;
};
```

### 4.3 Bulk Loading

```cpp
struct BulkLoadConfig {
    uint32_t batch_size = 10000;          // Rows per batch
    uint32_t parallel_workers = 4;         // Concurrent workers
    uint32_t rate_limit_rows_sec = 0;      // 0 = unlimited
    bool create_indexes_after = true;      // Defer index creation
    bool disable_triggers = true;          // Skip triggers during load
    std::string order_by_column;           // For resumable loads
    std::optional<std::string> where_clause; // Partial migration filter
};

class BulkLoader {
public:
    // Load entire table
    Result<BulkLoadResult> loadTable(
        const std::string& source_server,
        const std::string& source_table,
        const std::string& target_table,
        const BulkLoadConfig& config);

    // Load with resumption
    Result<BulkLoadResult> resumeLoad(
        const ID& migration_id,
        const BulkLoadCheckpoint& checkpoint);

    // Progress callback
    using ProgressCallback = std::function<void(const BulkLoadProgress&)>;
    void setProgressCallback(ProgressCallback cb);

private:
    struct BulkLoadProgress {
        uint64_t rows_total;
        uint64_t rows_loaded;
        uint64_t bytes_transferred;
        std::chrono::milliseconds elapsed;
        double rows_per_second;
        std::chrono::milliseconds estimated_remaining;
    };

    // Batch extraction from source
    Result<std::vector<Row>> extractBatch(
        RemoteConnection& conn,
        const std::string& query,
        uint64_t offset,
        uint32_t batch_size);

    // Batch insertion to target
    Result<void> insertBatch(
        Transaction& txn,
        const std::string& target_table,
        const std::vector<Row>& rows);
};
```

### 4.4 Checkpoint and Resume

```cpp
struct BulkLoadCheckpoint {
    ID migration_id;
    uint64_t rows_completed;
    std::string last_key_value;           // For keyset pagination
    std::chrono::system_clock::time_point checkpoint_time;
    std::string source_query_hash;        // Detect schema changes
};

// Checkpoint stored in SYS$MIGRATION_CHECKPOINTS
CREATE TABLE SYS$MIGRATION_CHECKPOINTS (
    migration_id        UUID PRIMARY KEY,
    checkpoint_data     BLOB NOT NULL,
    created_at          TIMESTAMP NOT NULL
);
```

---

## 5. Change Data Capture (CDC)

### 5.1 CDC Processor Interface

```cpp
class ICDCProcessor {
public:
    virtual ~ICDCProcessor() = default;

    // Start consuming changes
    virtual Result<void> start(const CDCConfig& config) = 0;

    // Stop consuming
    virtual Result<void> stop() = 0;

    // Get current position
    virtual CDCPosition getCurrentPosition() = 0;

    // Set position for resume
    virtual Result<void> setPosition(const CDCPosition& pos) = 0;

    // Change callback
    using ChangeCallback = std::function<void(const CDCChange&)>;
    virtual void setChangeCallback(ChangeCallback cb) = 0;

    // Get lag metrics
    virtual CDCLagMetrics getLagMetrics() = 0;
};

struct CDCChange {
    enum class Operation { INSERT, UPDATE, DELETE };

    Operation operation;
    std::string table_name;
    std::vector<TypedValue> old_values;   // For UPDATE/DELETE
    std::vector<TypedValue> new_values;   // For INSERT/UPDATE
    CDCPosition position;
    std::chrono::system_clock::time_point source_timestamp;
};

struct CDCPosition {
    std::string position_type;            // "lsn", "gtid", "binlog", etc.
    std::string position_value;           // Database-specific position
};
```

### 5.2 PostgreSQL Logical Replication

```cpp
class PostgreSQLCDCProcessor : public ICDCProcessor {
public:
    PostgreSQLCDCProcessor(
        const std::string& server_name,
        const std::string& slot_name);

    Result<void> start(const CDCConfig& config) override;

private:
    // Create replication slot
    Result<void> createReplicationSlot();

    // Start streaming
    Result<void> startStreaming(const std::string& lsn);

    // Decode logical messages
    CDCChange decodeMessage(const PGLogicalMessage& msg);

    // Acknowledge processed LSN
    Result<void> acknowledgePosition(const std::string& lsn);
};

// PostgreSQL-specific configuration
struct PostgreSQLCDCConfig {
    std::string publication_name;          // CREATE PUBLICATION
    std::string slot_name;                 // Replication slot name
    std::string output_plugin = "pgoutput"; // or "wal2json"
    bool include_transaction_ids = true;
    bool include_timestamps = true;
};
```

### 5.3 MySQL Binary Log

```cpp
class MySQLCDCProcessor : public ICDCProcessor {
public:
    MySQLCDCProcessor(
        const std::string& server_name,
        const MySQLCDCConfig& config);

private:
    // Register as replica
    Result<void> registerAsReplica();

    // Request binlog dump
    Result<void> requestBinlogDump(
        const std::string& binlog_file,
        uint64_t binlog_position);

    // Decode binlog events
    CDCChange decodeBinlogEvent(const BinlogEvent& event);

    // Handle GTID mode
    Result<void> requestGTIDDump(const std::string& gtid_set);
};

struct MySQLCDCConfig {
    uint32_t server_id;                    // Unique replica server ID
    bool use_gtid = true;                  // GTID-based replication
    std::string binlog_file;               // Starting binlog file
    uint64_t binlog_position = 4;          // Starting position
    std::string gtid_set;                  // For GTID mode
};
```

### 5.4 SQL Server Change Tracking

```cpp
class SQLServerCDCProcessor : public ICDCProcessor {
public:
    SQLServerCDCProcessor(
        const std::string& server_name,
        const SQLServerCDCConfig& config);

private:
    // Poll for changes using Change Tracking
    Result<std::vector<CDCChange>> pollChanges(
        uint64_t last_sync_version);

    // Or use CDC tables
    Result<std::vector<CDCChange>> readCDCTable(
        const std::string& capture_instance,
        const std::string& from_lsn,
        const std::string& to_lsn);
};

struct SQLServerCDCConfig {
    enum class Mode { CHANGE_TRACKING, CDC_TABLES };
    Mode mode = Mode::CHANGE_TRACKING;
    uint32_t poll_interval_ms = 1000;
    std::string capture_instance;          // For CDC mode
};
```

### 5.5 Firebird Trigger-Based Capture

```cpp
class FirebirdCDCProcessor : public ICDCProcessor {
public:
    FirebirdCDCProcessor(
        const std::string& server_name,
        const FirebirdCDCConfig& config);

private:
    // Install capture triggers
    Result<void> installCaptureTriggers(
        const std::string& table_name);

    // Read from shadow table
    Result<std::vector<CDCChange>> readShadowTable(
        uint64_t last_sequence);

    // Cleanup processed changes
    Result<void> cleanupShadowTable(uint64_t up_to_sequence);
};

// Shadow table schema (created on source)
// CREATE TABLE {table}_$CHANGES (
//     change_seq BIGINT GENERATED BY DEFAULT AS IDENTITY,
//     change_type CHAR(1),  -- 'I', 'U', 'D'
//     change_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
//     old_data BLOB,
//     new_data BLOB,
//     PRIMARY KEY (change_seq)
// );
```

### 5.6 CDC Change Applier

```cpp
class CDCChangeApplier {
public:
    // Apply a batch of changes
    Result<ApplyResult> applyChanges(
        Transaction& txn,
        const std::vector<CDCChange>& changes);

    // Conflict handling
    void setConflictResolver(ConflictResolver resolver);

private:
    struct ApplyResult {
        uint64_t applied_count;
        uint64_t conflict_count;
        uint64_t skip_count;
        std::vector<ConflictRecord> conflicts;
    };

    // Apply single change with conflict detection
    Result<bool> applySingleChange(
        Transaction& txn,
        const CDCChange& change);

    // Detect conflicts
    std::optional<Conflict> detectConflict(
        Transaction& txn,
        const CDCChange& change);
};
```

---

## 6. Dual-Write Coordination

### 6.1 Dual-Write Coordinator

```cpp
class DualWriteCoordinator {
public:
    // Execute write on both systems
    Result<DualWriteResult> executeWrite(
        const SemanticResult& semantic_result,
        const ConnectionContext& conn_ctx);

    // Configure conflict resolution
    void setConflictStrategy(
        const ID& table_id,
        ConflictStrategy strategy);

private:
    struct DualWriteResult {
        bool local_success;
        bool remote_success;
        uint64_t local_affected;
        uint64_t remote_affected;
        std::optional<Conflict> conflict;
    };

    // Execute with 2PC if needed
    Result<DualWriteResult> executeWith2PC(
        const std::string& local_sql,
        const std::string& remote_sql,
        Transaction& local_txn,
        RemoteTransaction& remote_txn);

    // Execute without 2PC (best effort)
    Result<DualWriteResult> executeBestEffort(
        const std::string& local_sql,
        const std::string& remote_sql,
        Transaction& local_txn,
        RemoteConnection& remote_conn);
};
```

### 6.2 Conflict Detection and Resolution

```cpp
enum class ConflictStrategy {
    SOURCE_WINS,            // Source database value wins
    TARGET_WINS,            // ScratchBird value wins
    TIMESTAMP_WINS,         // Most recent timestamp wins
    CUSTOM_MERGE,           // User-defined merge function
    FAIL_ON_CONFLICT        // Abort transaction on conflict
};

struct Conflict {
    ID table_id;
    std::vector<TypedValue> primary_key;
    std::vector<TypedValue> local_values;
    std::vector<TypedValue> remote_values;
    std::chrono::system_clock::time_point local_timestamp;
    std::chrono::system_clock::time_point remote_timestamp;
    ConflictStrategy resolution_used;
    std::vector<TypedValue> resolved_values;
};

class ConflictResolver {
public:
    // Resolve a conflict
    Result<std::vector<TypedValue>> resolve(
        const Conflict& conflict,
        ConflictStrategy strategy);

    // Register custom merge function
    void registerMergeFunction(
        const ID& table_id,
        MergeFunction func);

private:
    using MergeFunction = std::function<
        std::vector<TypedValue>(
            const std::vector<TypedValue>& local,
            const std::vector<TypedValue>& remote)>;

    std::map<ID, MergeFunction> custom_mergers_;
};
```

### 6.3 Conflict Logging

```sql
CREATE TABLE SYS$MIGRATION_CONFLICTS (
    conflict_id         UUID PRIMARY KEY,
    table_id            UUID NOT NULL,
    primary_key_json    JSON NOT NULL,
    local_values_json   JSON,
    remote_values_json  JSON,
    local_timestamp     TIMESTAMP,
    remote_timestamp    TIMESTAMP,
    resolution_strategy VARCHAR(32) NOT NULL,
    resolved_values_json JSON,
    detected_at         TIMESTAMP NOT NULL,
    resolved_at         TIMESTAMP,
    FOREIGN KEY (table_id) REFERENCES SYS$MIGRATION_STATE(table_id)
);
```

### 6.4 Write Ordering Guarantees

```cpp
class WriteOrderingManager {
public:
    // Ensure writes are applied in order
    Result<void> ensureOrdering(
        const std::vector<CDCChange>& changes);

    // Get ordering constraints
    std::vector<OrderingConstraint> getConstraints(
        const ID& table_id);

private:
    // Track last applied change per table
    std::map<ID, CDCPosition> last_applied_positions_;

    // Detect out-of-order changes
    bool isOutOfOrder(
        const CDCChange& change,
        const CDCPosition& last_position);
};
```

---

## 7. Cutover Process

### 7.1 Cutover State Machine

```
┌─────────────────┐
│  DUAL_WRITE     │
└────────┬────────┘
         │ Validation passes
         ▼
┌─────────────────┐
│ CUTOVER_READY   │
└────────┬────────┘
         │ CUTOVER command
         ▼
┌─────────────────┐
│ CUTOVER_STARTED │ (brief)
│ - Quiesce writes│
│ - Drain CDC     │
│ - Final sync    │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ LOCAL_ONLY      │
└─────────────────┘
```

### 7.2 Cutover Coordinator

```cpp
class CutoverCoordinator {
public:
    // Pre-cutover validation
    Result<ValidationReport> validateForCutover(const ID& table_id);

    // Execute cutover
    Result<CutoverResult> executeCutover(
        const ID& table_id,
        const CutoverOptions& options);

    // Rollback from LOCAL_ONLY to DUAL_WRITE
    Result<void> initiateRollback(const ID& table_id);

private:
    struct CutoverResult {
        bool success;
        std::chrono::milliseconds total_duration;
        std::chrono::milliseconds quiesce_duration;
        std::chrono::milliseconds drain_duration;
        uint64_t final_changes_applied;
        std::optional<std::string> error;
    };

    // Quiesce writes (block new writes, wait for in-flight)
    Result<void> quiesceWrites(const ID& table_id);

    // Drain CDC queue
    Result<void> drainCDC(const ID& table_id);

    // Final data validation
    Result<bool> validateDataConsistency(const ID& table_id);

    // Atomic state switch
    Result<void> switchToLocalOnly(const ID& table_id);
};
```

### 7.3 Pre-Cutover Validation

```cpp
struct ValidationReport {
    bool ready_for_cutover;

    // Data validation
    uint64_t local_row_count;
    uint64_t remote_row_count;
    bool row_counts_match;

    // Checksum validation (sample-based)
    bool checksums_valid;
    double sample_percentage;
    uint64_t mismatched_rows;

    // CDC validation
    std::chrono::milliseconds cdc_lag;
    bool cdc_lag_acceptable;          // < threshold
    uint64_t pending_changes;

    // Constraint validation
    bool all_constraints_valid;
    std::vector<std::string> constraint_violations;

    // Index validation
    bool all_indexes_built;
    std::vector<std::string> missing_indexes;

    // Blocking issues
    std::vector<std::string> blocking_issues;
    std::vector<std::string> warnings;
};

class CutoverValidator {
public:
    Result<ValidationReport> validate(
        const ID& table_id,
        const ValidationOptions& options);

private:
    // Row count comparison
    Result<std::pair<uint64_t, uint64_t>> compareRowCounts(
        const ID& table_id);

    // Sample-based checksum
    Result<bool> validateChecksums(
        const ID& table_id,
        double sample_percentage);

    // Full scan comparison (expensive)
    Result<std::vector<RowDiff>> fullComparison(
        const ID& table_id);
};
```

### 7.4 Rollback Procedure

```cpp
class RollbackCoordinator {
public:
    // Initiate rollback from any state
    Result<void> initiateRollback(
        const ID& table_id,
        const RollbackOptions& options);

    // Get rollback status
    RollbackStatus getStatus(const ID& table_id);

private:
    struct RollbackOptions {
        bool preserve_local_data = true;   // Keep local copy
        bool replay_local_changes = false; // Push local changes to source
        std::chrono::seconds timeout{300};
    };

    // Rollback from LOCAL_ONLY
    Result<void> rollbackFromLocalOnly(const ID& table_id);

    // Rollback from DUAL_WRITE
    Result<void> rollbackFromDualWrite(const ID& table_id);

    // Rollback from SYNCHRONIZING
    Result<void> rollbackFromSynchronizing(const ID& table_id);
};
```

---

## 8. SQL Interface

### 8.1 Migration Source Definition

```sql
-- Create a migration source (references existing foreign server)
CREATE MIGRATION SOURCE source_name
    FROM SERVER server_name
    OPTIONS (
        cdc_mode 'logical_replication',  -- 'logical_replication', 'binlog',
                                         -- 'change_tracking', 'trigger'
        batch_size 10000,
        parallel_workers 4,
        rate_limit 50000,                -- rows/second, 0 = unlimited
        conflict_strategy 'source_wins', -- 'source_wins', 'target_wins',
                                         -- 'timestamp', 'fail'
        validation_sample 0.01           -- 1% sample for validation
    );

-- Alter migration source
ALTER MIGRATION SOURCE source_name
    SET OPTION batch_size = 50000;

-- Drop migration source
DROP MIGRATION SOURCE source_name [CASCADE];

-- Show migration sources
SHOW MIGRATION SOURCES;
```

### 8.2 Migration Control

```sql
-- Start migration for a table
START MIGRATION FOR TABLE [schema.]table_name
    FROM source_name
    [OPTIONS (
        where_clause '...',              -- Filter source rows
        order_by 'column_name',          -- For resumable loads
        target_table '[schema.]name',    -- Different target name
        defer_indexes TRUE               -- Create indexes after load
    )];

-- Start migration for entire schema
START MIGRATION FOR SCHEMA schema_name
    FROM source_name
    [EXCLUDING (table1, table2)]
    [INCLUDING ONLY (table3, table4)];

-- Pause migration
PAUSE MIGRATION FOR TABLE [schema.]table_name;
PAUSE MIGRATION FOR SCHEMA schema_name;
PAUSE MIGRATION ALL;

-- Resume migration
RESUME MIGRATION FOR TABLE [schema.]table_name;
RESUME MIGRATION FOR SCHEMA schema_name;
RESUME MIGRATION ALL;

-- Abort migration (discard progress)
ABORT MIGRATION FOR TABLE [schema.]table_name;

-- Cutover (switch to local-only)
CUTOVER TABLE [schema.]table_name
    [FORCE]                              -- Skip validation
    [TIMEOUT seconds];

CUTOVER SCHEMA schema_name
    [FORCE]
    [TIMEOUT seconds];

-- Rollback to source
ROLLBACK MIGRATION FOR TABLE [schema.]table_name
    [PRESERVE LOCAL DATA]
    [TIMEOUT seconds];
```

### 8.3 Migration Status

```sql
-- Show overall migration status
SHOW MIGRATION STATUS;
/*
+------------------+---------+-------------+------------+----------+--------+
| Table            | State   | Progress    | CDC Lag    | ETA      | Source |
+------------------+---------+-------------+------------+----------+--------+
| public.users     | DUAL_WR | 100%        | 0.5s       | -        | legacy |
| public.orders    | SYNC    | 85.2%       | 12.3s      | 00:15:00 | legacy |
| public.products  | BULK    | 45.0%       | -          | 01:30:00 | legacy |
| public.inventory | NOT_ST  | 0%          | -          | -        | legacy |
+------------------+---------+-------------+------------+----------+--------+
*/

-- Show detailed status for a table
SHOW MIGRATION STATUS FOR TABLE public.orders;
/*
Table:           public.orders
Source:          legacy_pg.public.orders
State:           SYNCHRONIZING
Started:         2024-03-01 10:00:00
State Changed:   2024-03-01 12:30:00

Bulk Load:
  Total Rows:    10,000,000
  Migrated:      8,520,000 (85.2%)
  Rate:          15,234 rows/sec
  ETA:           00:15:00

CDC:
  Mode:          logical_replication
  Position:      0/1A234B8
  Lag:           12.3 seconds
  Pending:       1,234 changes
  Applied:       45,678 changes

Validation:
  Last Run:      2024-03-01 12:00:00
  Row Count:     Match (10,000,000)
  Checksum:      Pass (1% sample)
  Conflicts:     3 (resolved)

Performance:
  Reads (Local): 0 (all routed to source)
  Writes (Both): 1,234 (dual-write active)
*/

-- Show migration progress over time
SHOW MIGRATION PROGRESS FOR TABLE public.orders
    INTERVAL '1 hour'
    LAST '24 hours';

-- Show migration conflicts
SHOW MIGRATION CONFLICTS [FOR TABLE table_name]
    [UNRESOLVED]
    [LIMIT n];

-- Show migration errors
SHOW MIGRATION ERRORS [FOR TABLE table_name]
    [LAST 'interval'];
```

### 8.4 Migration Validation

```sql
-- Validate a table for cutover
VALIDATE MIGRATION FOR TABLE [schema.]table_name
    [FULL]                               -- Full row comparison (slow)
    [SAMPLE percentage];                 -- Sample-based validation

-- Validate entire schema
VALIDATE MIGRATION FOR SCHEMA schema_name;

-- Compare row counts
SELECT * FROM MIGRATION_ROW_COUNTS(source_name);
/*
+------------------+-------------+--------------+-------+
| Table            | Local Count | Remote Count | Match |
+------------------+-------------+--------------+-------+
| public.users     | 1,000,000   | 1,000,000    | Yes   |
| public.orders    | 8,520,000   | 10,000,000   | No    |
+------------------+-------------+--------------+-------+
*/
```

---

## 9. Monitoring and Metrics

### 9.1 Prometheus Metrics

```prometheus
# Migration state
# HELP scratchbird_migration_table_state Current migration state per table
# TYPE scratchbird_migration_table_state gauge
scratchbird_migration_table_state{table="users",state="dual_write"} 1
scratchbird_migration_table_state{table="orders",state="synchronizing"} 1

# Bulk load progress
# HELP scratchbird_migration_bulk_rows_total Total rows to migrate
# TYPE scratchbird_migration_bulk_rows_total gauge
scratchbird_migration_bulk_rows_total{table="orders"} 10000000

# HELP scratchbird_migration_bulk_rows_migrated Rows migrated so far
# TYPE scratchbird_migration_bulk_rows_migrated gauge
scratchbird_migration_bulk_rows_migrated{table="orders"} 8520000

# HELP scratchbird_migration_bulk_rate Rows migrated per second
# TYPE scratchbird_migration_bulk_rate gauge
scratchbird_migration_bulk_rate{table="orders"} 15234

# CDC metrics
# HELP scratchbird_migration_cdc_lag_seconds CDC replication lag
# TYPE scratchbird_migration_cdc_lag_seconds gauge
scratchbird_migration_cdc_lag_seconds{table="users",source="legacy_pg"} 0.5

# HELP scratchbird_migration_cdc_pending_changes Pending CDC changes
# TYPE scratchbird_migration_cdc_pending_changes gauge
scratchbird_migration_cdc_pending_changes{table="users"} 123

# HELP scratchbird_migration_cdc_applied_total CDC changes applied
# TYPE scratchbird_migration_cdc_applied_total counter
scratchbird_migration_cdc_applied_total{table="users",operation="insert"} 45678
scratchbird_migration_cdc_applied_total{table="users",operation="update"} 12345
scratchbird_migration_cdc_applied_total{table="users",operation="delete"} 678

# Query routing
# HELP scratchbird_migration_queries_routed_total Queries routed by destination
# TYPE scratchbird_migration_queries_routed_total counter
scratchbird_migration_queries_routed_total{table="users",route="local"} 100000
scratchbird_migration_queries_routed_total{table="orders",route="remote"} 50000
scratchbird_migration_queries_routed_total{table="orders",route="dual"} 5000

# Conflicts
# HELP scratchbird_migration_conflicts_total Conflicts detected
# TYPE scratchbird_migration_conflicts_total counter
scratchbird_migration_conflicts_total{table="users",resolution="source_wins"} 3

# Errors
# HELP scratchbird_migration_errors_total Migration errors
# TYPE scratchbird_migration_errors_total counter
scratchbird_migration_errors_total{table="orders",error="connection_timeout"} 2
```

### 9.2 Alert Rules

```yaml
groups:
  - name: scratchbird_migration
    rules:
      - alert: MigrationCDCLagHigh
        expr: scratchbird_migration_cdc_lag_seconds > 60
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "CDC lag > 60s for {{ $labels.table }}"

      - alert: MigrationCDCLagCritical
        expr: scratchbird_migration_cdc_lag_seconds > 300
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "CDC lag > 5 minutes for {{ $labels.table }}"

      - alert: MigrationBulkLoadStalled
        expr: rate(scratchbird_migration_bulk_rows_migrated[5m]) == 0
              and scratchbird_migration_table_state{state="bulk_loading"} == 1
        for: 10m
        labels:
          severity: warning
        annotations:
          summary: "Bulk load stalled for {{ $labels.table }}"

      - alert: MigrationConflictsHigh
        expr: rate(scratchbird_migration_conflicts_total[1h]) > 10
        for: 15m
        labels:
          severity: warning
        annotations:
          summary: "High conflict rate for {{ $labels.table }}"

      - alert: MigrationInErrorState
        expr: scratchbird_migration_table_state{state="error"} == 1
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Migration in error state for {{ $labels.table }}"
```

### 9.3 System Tables for Monitoring

```sql
-- Migration history
CREATE TABLE SYS$MIGRATION_HISTORY (
    event_id            UUID PRIMARY KEY,
    table_id            UUID NOT NULL,
    event_type          VARCHAR(32) NOT NULL,  -- 'state_change', 'error',
                                               -- 'checkpoint', 'validation'
    old_state           VARCHAR(32),
    new_state           VARCHAR(32),
    details             JSON,
    created_at          TIMESTAMP NOT NULL,
    FOREIGN KEY (table_id) REFERENCES SYS$MIGRATION_STATE(table_id)
);

-- CDC positions (for resume)
CREATE TABLE SYS$MIGRATION_CDC_POSITIONS (
    source_server       VARCHAR(128) NOT NULL,
    table_name          VARCHAR(256) NOT NULL,
    cdc_type            VARCHAR(32) NOT NULL,
    position_data       JSON NOT NULL,
    updated_at          TIMESTAMP NOT NULL,
    PRIMARY KEY (source_server, table_name)
);
```

---

## 10. Error Handling and Recovery

### 10.1 Error Categories

```cpp
enum class MigrationErrorCategory {
    CONNECTION,         // Network/connection errors
    AUTHENTICATION,     // Auth failures
    PERMISSION,         // Insufficient privileges
    SCHEMA_MISMATCH,    // Schema differences
    DATA_TYPE,          // Type conversion errors
    CONSTRAINT,         // Constraint violations
    CDC,                // CDC-specific errors
    TIMEOUT,            // Operation timeouts
    RESOURCE,           // Resource exhaustion
    CONFLICT,           // Unresolvable conflicts
    INTERNAL            // Internal errors
};

struct MigrationError {
    MigrationErrorCategory category;
    std::string code;
    std::string message;
    std::string table_name;
    std::optional<std::string> row_key;
    std::optional<std::string> source_error;
    bool retryable;
    std::chrono::system_clock::time_point timestamp;
};
```

### 10.2 Retry Logic

```cpp
struct RetryPolicy {
    uint32_t max_retries = 3;
    std::chrono::milliseconds initial_delay{1000};
    std::chrono::milliseconds max_delay{60000};
    double backoff_multiplier = 2.0;

    // Error categories to retry
    std::set<MigrationErrorCategory> retryable_categories = {
        MigrationErrorCategory::CONNECTION,
        MigrationErrorCategory::TIMEOUT,
        MigrationErrorCategory::RESOURCE
    };
};

class MigrationRetryHandler {
public:
    template<typename Func>
    Result<typename std::invoke_result_t<Func>> executeWithRetry(
        Func&& operation,
        const RetryPolicy& policy);

private:
    bool shouldRetry(
        const MigrationError& error,
        uint32_t attempt,
        const RetryPolicy& policy);

    std::chrono::milliseconds calculateDelay(
        uint32_t attempt,
        const RetryPolicy& policy);
};
```

### 10.3 Recovery Procedures

```cpp
class MigrationRecovery {
public:
    // Resume after crash
    Result<void> recoverFromCrash();

    // Resume specific table migration
    Result<void> resumeTableMigration(const ID& table_id);

    // Repair inconsistent state
    Result<void> repairState(const ID& table_id);

    // Skip problematic rows
    Result<void> skipRows(
        const ID& table_id,
        const std::vector<std::string>& row_keys);

private:
    // Check for incomplete transactions
    std::vector<IncompleteTransaction> findIncompleteTransactions();

    // Rollback incomplete transactions
    Result<void> rollbackIncomplete(const IncompleteTransaction& txn);

    // Verify checkpoint consistency
    Result<bool> verifyCheckpoint(const BulkLoadCheckpoint& checkpoint);
};
```

### 10.4 Data Validation and Repair

```cpp
class DataValidator {
public:
    // Find mismatched rows
    Result<std::vector<RowMismatch>> findMismatches(
        const ID& table_id,
        const ValidationOptions& options);

    // Repair mismatched rows
    Result<RepairResult> repairMismatches(
        const ID& table_id,
        const std::vector<RowMismatch>& mismatches,
        RepairStrategy strategy);

private:
    struct RowMismatch {
        std::vector<TypedValue> primary_key;
        std::optional<std::vector<TypedValue>> local_values;
        std::optional<std::vector<TypedValue>> remote_values;
        MismatchType type;  // MISSING_LOCAL, MISSING_REMOTE, VALUE_DIFF
    };

    enum class RepairStrategy {
        COPY_FROM_SOURCE,   // Overwrite local with source
        COPY_TO_SOURCE,     // Overwrite source with local
        SKIP,               // Log and skip
        MANUAL              // Flag for manual review
    };
};
```

---

## 11. Security Considerations

### 11.1 Credential Management

```cpp
// Credentials stored encrypted in catalog
struct MigrationCredentials {
    std::string server_name;
    std::string username;
    EncryptedString password;           // AES-256 encrypted
    std::optional<std::string> ssl_cert_path;
    std::optional<std::string> ssl_key_path;
    std::optional<std::string> kerberos_principal;
};

class MigrationCredentialManager {
public:
    // Store credentials (encrypted)
    Result<void> storeCredentials(
        const std::string& server_name,
        const std::string& user,
        const std::string& password,
        const ID& owner_user_id);

    // Retrieve credentials (decrypted)
    Result<MigrationCredentials> getCredentials(
        const std::string& server_name,
        const ID& requesting_user_id);

    // Rotate credentials
    Result<void> rotateCredentials(
        const std::string& server_name,
        const std::string& new_password);

private:
    // Check access permission
    bool canAccessCredentials(
        const ID& user_id,
        const std::string& server_name);
};
```

### 11.2 Audit Logging

```sql
-- Migration audit log
CREATE TABLE SYS$MIGRATION_AUDIT (
    audit_id            UUID PRIMARY KEY,
    timestamp           TIMESTAMP NOT NULL,
    user_id             UUID NOT NULL,
    action              VARCHAR(64) NOT NULL,
    table_id            UUID,
    source_server       VARCHAR(128),
    details             JSON,
    client_ip           VARCHAR(45),
    session_id          UUID
);

-- Audit events logged:
-- MIGRATION_START, MIGRATION_PAUSE, MIGRATION_RESUME, MIGRATION_ABORT
-- CUTOVER_START, CUTOVER_COMPLETE, CUTOVER_FAILED
-- ROLLBACK_START, ROLLBACK_COMPLETE
-- CREDENTIALS_ACCESSED, CREDENTIALS_ROTATED
-- CONFLICT_DETECTED, CONFLICT_RESOLVED
-- ERROR_OCCURRED, RECOVERY_PERFORMED
```

### 11.3 Row-Level Security During Migration

```cpp
class MigrationRLSHandler {
public:
    // Apply RLS to remote queries
    std::string applyRLSToRemoteQuery(
        const std::string& query,
        const ConnectionContext& conn_ctx);

    // Verify RLS consistency between systems
    Result<bool> verifyRLSConsistency(
        const ID& table_id,
        const ID& user_id);

private:
    // Get RLS policies for table
    std::vector<RLSPolicy> getRLSPolicies(const ID& table_id);

    // Translate RLS predicate for remote dialect
    std::string translateRLSPredicate(
        const RLSPolicy& policy,
        DatabaseType target_db);
};
```

### 11.4 Encryption in Transit

- All connections to source databases use TLS 1.2+
- CDC streams are encrypted
- Credentials never logged or exposed in error messages
- Sensitive data masked in SHOW commands

---

## 12. Performance Tuning

### 12.1 Bulk Load Optimization

```cpp
struct BulkLoadTuning {
    // Batch sizing
    uint32_t batch_size = 10000;          // Rows per INSERT
    uint32_t commit_batch_size = 100000;  // Rows per COMMIT

    // Parallelism
    uint32_t extract_workers = 4;         // Source extraction threads
    uint32_t load_workers = 2;            // Target insert threads
    uint32_t transform_workers = 2;       // Type conversion threads

    // Memory
    size_t row_buffer_size = 100 * 1024 * 1024;  // 100MB per worker

    // I/O
    bool use_copy_protocol = true;        // PostgreSQL COPY
    bool use_load_data = true;            // MySQL LOAD DATA
    bool disable_indexes = true;          // Disable during load
    bool disable_constraints = true;      // Defer constraint checks

    // Rate limiting
    uint32_t max_rows_per_second = 0;     // 0 = unlimited
    uint32_t max_bytes_per_second = 0;    // 0 = unlimited
};
```

### 12.2 CDC Tuning

```cpp
struct CDCTuning {
    // Batching
    uint32_t batch_size = 1000;           // Changes per batch
    std::chrono::milliseconds max_batch_wait{100};  // Max wait for batch

    // Processing
    uint32_t apply_workers = 4;           // Parallel appliers
    bool order_by_table = true;           // Group changes by table

    // Memory
    size_t change_buffer_size = 50 * 1024 * 1024;  // 50MB buffer

    // Lag management
    std::chrono::seconds max_lag_before_throttle{30};
    double throttle_factor = 0.5;         // Reduce write rate when lagging
};
```

### 12.3 Index Creation Strategy

```cpp
enum class IndexCreationStrategy {
    IMMEDIATE,          // Create indexes during bulk load (slower)
    DEFERRED,           // Create after bulk load completes
    CONCURRENT,         // Create concurrently (PostgreSQL-style)
    INCREMENTAL         // Build incrementally during CDC
};

struct IndexCreationConfig {
    IndexCreationStrategy strategy = IndexCreationStrategy::DEFERRED;
    uint32_t parallel_index_builders = 2;
    bool online_rebuild = true;           // Allow queries during build
    std::chrono::seconds max_build_time{3600};  // 1 hour timeout
};
```

### 12.4 Memory Management

```cpp
struct MigrationMemoryConfig {
    // Global limits
    size_t max_migration_memory = 1 * 1024 * 1024 * 1024;  // 1GB total

    // Per-table limits
    size_t max_per_table_memory = 256 * 1024 * 1024;  // 256MB per table

    // Buffer pools
    size_t bulk_load_buffer = 100 * 1024 * 1024;   // 100MB
    size_t cdc_buffer = 50 * 1024 * 1024;          // 50MB
    size_t transform_buffer = 50 * 1024 * 1024;    // 50MB

    // Spill to disk threshold
    double spill_threshold = 0.9;         // Spill at 90% memory usage
};
```

---

## 13. Configuration Reference

### 13.1 Server Configuration (sb_server.conf)

```ini
[migration]
# Enable migration feature
enabled = true

# Maximum concurrent table migrations
max_concurrent_migrations = 10

# Default batch size for bulk load
default_batch_size = 10000

# Default parallel workers
default_parallel_workers = 4

# Memory limit for all migrations
max_memory_mb = 1024

# CDC default settings
cdc_batch_size = 1000
cdc_max_lag_seconds = 60

# Validation settings
validation_sample_percent = 1.0
validation_checksum_enabled = true

# Conflict resolution default
default_conflict_strategy = source_wins

# Audit logging
audit_enabled = true
audit_level = full  # minimal, standard, full

# Retry settings
max_retries = 3
retry_initial_delay_ms = 1000
retry_max_delay_ms = 60000
```

### 13.2 Per-Migration Configuration

```sql
-- All available options for START MIGRATION
START MIGRATION FOR TABLE source_table
    FROM source_name
    OPTIONS (
        -- Target configuration
        target_schema 'schema_name',
        target_table 'table_name',

        -- Filtering
        where_clause 'created_at > ''2024-01-01''',
        columns 'id, name, email, created_at',  -- Subset of columns

        -- Bulk load
        batch_size 50000,
        parallel_workers 8,
        rate_limit 100000,                       -- rows/sec
        order_by 'id',                           -- For resumable loads

        -- Indexes
        defer_indexes TRUE,
        concurrent_indexes TRUE,

        -- CDC
        cdc_mode 'logical_replication',
        cdc_batch_size 5000,

        -- Validation
        validation_sample 0.05,                  -- 5%
        validation_interval '1 hour',

        -- Conflict handling
        conflict_strategy 'timestamp_wins',
        conflict_timestamp_column 'updated_at',

        -- Error handling
        max_errors 100,
        skip_on_error FALSE,

        -- Cutover
        auto_cutover FALSE,                      -- Manual cutover required
        cutover_lag_threshold 5                  -- Max CDC lag for auto-cutover
    );
```

---

## 14. Implementation Roadmap

### 14.1 Phase 1: Core Infrastructure (4 weeks)

- Migration state machine
- Query router (basic routing)
- Bulk loader (single-threaded)
- SQL interface (basic commands)

### 14.2 Phase 2: CDC Integration (4 weeks)

- PostgreSQL logical replication
- MySQL binlog parsing
- CDC change applier
- Lag monitoring

### 14.3 Phase 3: Dual-Write (3 weeks)

- Dual-write coordinator
- Conflict detection
- Basic conflict resolution
- 2PC integration

### 14.4 Phase 4: Cutover & Validation (2 weeks)

- Cutover coordinator
- Pre-cutover validation
- Rollback procedures
- Data comparison tools

### 14.5 Phase 5: Polish & Performance (3 weeks)

- Parallel bulk loading
- Advanced CDC tuning
- SQL Server and Firebird CDC
- Monitoring and alerting
- Documentation

**Total: ~16 weeks**

---

## 15. Appendix A: CDC Position Formats

### PostgreSQL LSN

```json
{
  "type": "postgresql_lsn",
  "slot_name": "scratchbird_migration_slot",
  "lsn": "0/1A234B8",
  "timeline_id": 1
}
```

### MySQL GTID

```json
{
  "type": "mysql_gtid",
  "gtid_set": "3E11FA47-71CA-11E1-9E33-C80AA9429562:1-5",
  "binlog_file": "mysql-bin.000003",
  "binlog_position": 12345
}
```

### SQL Server Change Tracking

```json
{
  "type": "sqlserver_ct",
  "sync_version": 12345678,
  "min_valid_version": 12340000
}
```

### Firebird Sequence

```json
{
  "type": "firebird_sequence",
  "table_name": "users",
  "shadow_table": "users_$CHANGES",
  "last_sequence": 98765
}
```

---

## 16. Appendix B: Error Codes

| Code | Category | Description |
|------|----------|-------------|
| MIG001 | CONNECTION | Cannot connect to source server |
| MIG002 | CONNECTION | Source connection lost during operation |
| MIG003 | AUTHENTICATION | Invalid credentials for source |
| MIG004 | PERMISSION | Insufficient privileges on source |
| MIG005 | PERMISSION | Cannot create replication slot |
| MIG010 | SCHEMA | Source table not found |
| MIG011 | SCHEMA | Schema mismatch detected |
| MIG012 | SCHEMA | Unsupported column type |
| MIG013 | SCHEMA | Primary key required for migration |
| MIG020 | CDC | Replication slot does not exist |
| MIG021 | CDC | Binary log position not found |
| MIG022 | CDC | CDC lag exceeded threshold |
| MIG023 | CDC | CDC stream disconnected |
| MIG030 | CONFLICT | Unresolvable conflict detected |
| MIG031 | CONFLICT | Conflict resolution failed |
| MIG040 | VALIDATION | Row count mismatch |
| MIG041 | VALIDATION | Checksum mismatch |
| MIG042 | VALIDATION | Missing rows in target |
| MIG050 | CUTOVER | Cutover validation failed |
| MIG051 | CUTOVER | Cutover timeout exceeded |
| MIG052 | CUTOVER | Cannot quiesce writes |
| MIG060 | ROLLBACK | Rollback failed |
| MIG061 | ROLLBACK | Cannot rollback from current state |
| MIG090 | INTERNAL | Internal migration error |
| MIG091 | INTERNAL | State corruption detected |
