# Remote Database UDR - Core Types

> Reference-only: Canonical UDR and live-migration behavior now lives in
> `ScratchBird/docs/specifications/Alpha Phase 2/11-Remote-Database-UDR-Specification.md`
> and `ScratchBird/docs/specifications/Alpha Phase 2/11h-Live-Migration-Emulated-Listener.md`.

**Scope Note:** MSSQL/TDS adapter support is post-gold; MSSQL references are forward-looking.

## 1. Enumerations

### 1.1 RemoteDatabaseType

Identifies the type of remote database being connected to.

```cpp
enum class RemoteDatabaseType : uint8_t {
    POSTGRESQL = 1,    // PostgreSQL 9.6+
    MYSQL      = 2,    // MySQL 5.7+, MariaDB 10+
    MSSQL      = 3,    // Microsoft SQL Server 2016+ (post-gold)
    FIREBIRD   = 4,    // Firebird 2.5+
    SCRATCHBIRD = 5,   // ScratchBird (federated)
    ORACLE     = 6,    // Oracle 12c+ (future)
    SQLITE     = 7,    // SQLite 3.x (future)
};
```

### 1.2 ConnectionState

Connection lifecycle states.

```cpp
enum class ConnectionState : uint8_t {
    DISCONNECTED  = 0,   // Not connected
    CONNECTING    = 1,   // Connection in progress
    AUTHENTICATING = 2,  // Authentication in progress
    CONNECTED     = 3,   // Ready for queries
    IN_TRANSACTION = 4,  // Transaction active
    EXECUTING     = 5,   // Query executing
    FETCHING      = 6,   // Fetching results
    CLOSING       = 7,   // Graceful close in progress
    FAILED        = 8,   // Connection failed
};
```

### 1.3 RemoteTypeOid

Remote database type identifiers (used for type mapping).

```cpp
// PostgreSQL OIDs (subset - full list in adapter)
namespace pg_types {
    constexpr uint32_t BOOL     = 16;
    constexpr uint32_t INT2     = 21;
    constexpr uint32_t INT4     = 23;
    constexpr uint32_t INT8     = 20;
    constexpr uint32_t FLOAT4   = 700;
    constexpr uint32_t FLOAT8   = 701;
    constexpr uint32_t NUMERIC  = 1700;
    constexpr uint32_t VARCHAR  = 1043;
    constexpr uint32_t TEXT     = 25;
    constexpr uint32_t BYTEA    = 17;
    constexpr uint32_t DATE     = 1082;
    constexpr uint32_t TIME     = 1083;
    constexpr uint32_t TIMESTAMP = 1114;
    constexpr uint32_t TIMESTAMPTZ = 1184;
    constexpr uint32_t UUID     = 2950;
    constexpr uint32_t JSON     = 114;
    constexpr uint32_t JSONB    = 3802;
}

// MySQL type codes
namespace mysql_types {
    constexpr uint8_t DECIMAL    = 0x00;
    constexpr uint8_t TINY      = 0x01;
    constexpr uint8_t SHORT     = 0x02;
    constexpr uint8_t LONG      = 0x03;
    constexpr uint8_t FLOAT     = 0x04;
    constexpr uint8_t DOUBLE    = 0x05;
    constexpr uint8_t LONGLONG  = 0x08;
    constexpr uint8_t INT24     = 0x09;
    constexpr uint8_t DATE      = 0x0A;
    constexpr uint8_t TIME      = 0x0B;
    constexpr uint8_t DATETIME  = 0x0C;
    constexpr uint8_t TIMESTAMP = 0x07;
    constexpr uint8_t VARCHAR   = 0x0F;
    constexpr uint8_t BLOB      = 0xFC;
    constexpr uint8_t VAR_STRING = 0xFD;
    constexpr uint8_t STRING    = 0xFE;
    constexpr uint8_t JSON      = 0xF5;
}
```

### 1.4 PushdownCapability

Indicates what operations can be pushed to the remote database.

```cpp
enum class PushdownCapability : uint32_t {
    NONE           = 0x0000,
    WHERE_CLAUSE   = 0x0001,   // Filter pushdown
    ORDER_BY       = 0x0002,   // Sorting pushdown
    LIMIT_OFFSET   = 0x0004,   // Pagination pushdown
    AGGREGATE      = 0x0008,   // COUNT, SUM, etc.
    GROUP_BY       = 0x0010,   // Grouping pushdown
    HAVING         = 0x0020,   // HAVING clause
    JOIN           = 0x0040,   // Remote joins
    SUBQUERY       = 0x0080,   // Subquery pushdown
    CTE            = 0x0100,   // WITH clause
    WINDOW_FUNC    = 0x0200,   // Window functions
    DISTINCT       = 0x0400,   // DISTINCT pushdown
    UNION          = 0x0800,   // Set operations

    // Common combinations
    BASIC = WHERE_CLAUSE | ORDER_BY | LIMIT_OFFSET,
    STANDARD = BASIC | AGGREGATE | GROUP_BY | HAVING | DISTINCT,
    FULL = 0xFFFF,
};
```

---

## 2. Core Structures

### 2.1 ServerDefinition

Represents a CREATE SERVER definition stored in the catalog.

```cpp
struct ServerDefinition {
    // Identity
    uint64_t server_id;              // Unique server ID
    std::string server_name;          // User-defined name
    RemoteDatabaseType db_type;       // Database type

    // Connection
    std::string host;                 // Hostname or IP
    uint16_t port;                    // Port number
    std::string database;             // Remote database name

    // Pool settings
    uint32_t min_connections;         // Minimum pool size (default: 1)
    uint32_t max_connections;         // Maximum pool size (default: 10)
    uint32_t connection_timeout_ms;   // Connect timeout (default: 5000)
    uint32_t query_timeout_ms;        // Query timeout (default: 30000)
    uint32_t idle_timeout_ms;         // Idle connection timeout (default: 300000)

    // SSL/TLS settings
    bool use_ssl;                     // Enable TLS
    std::string ssl_mode;             // disable/allow/prefer/require/verify-ca/verify-full
    std::string ssl_ca_cert;          // CA certificate path
    std::string ssl_client_cert;      // Client certificate path
    std::string ssl_client_key;       // Client key path

    // Additional options (key-value pairs)
    std::unordered_map<std::string, std::string> options;

    // Metadata
    uint64_t created_at;              // Creation timestamp
    uint64_t modified_at;             // Last modification
    std::string owner;                // Owner user
};
```

### 2.2 UserMapping

Maps local users to remote database credentials.

```cpp
struct UserMapping {
    uint64_t mapping_id;              // Unique mapping ID
    uint64_t server_id;               // References ServerDefinition
    std::string local_user;           // Local username (or "public" for default)

    // Remote credentials
    std::string remote_user;          // Remote username
    std::string remote_password;      // Remote password (encrypted in catalog)

    // Authentication options
    std::string auth_type;            // password, kerberos, certificate
    std::string kerberos_principal;   // For Kerberos auth
    std::string client_certificate;   // For certificate auth

    // Additional options
    std::unordered_map<std::string, std::string> options;
};
```

### 2.3 ForeignTableDefinition

Represents a foreign table that maps to a remote table.

```cpp
struct ForeignTableDefinition {
    uint64_t table_id;                // Local table ID
    uint64_t server_id;               // References ServerDefinition
    std::string local_schema;         // Local schema name
    std::string local_name;           // Local table name

    std::string remote_schema;        // Remote schema name
    std::string remote_name;          // Remote table name

    // Column mappings (local_name -> remote_name)
    std::vector<ColumnMapping> columns;

    // Pushdown hints
    PushdownCapability capabilities;  // What can be pushed down
    bool updatable;                   // Allow INSERT/UPDATE/DELETE

    // Cost hints for query planner
    double estimated_row_count;       // Estimated rows
    double startup_cost;              // Connection overhead
    double per_row_cost;              // Per-row fetch cost

    // Options
    std::unordered_map<std::string, std::string> options;
};

struct ColumnMapping {
    std::string local_name;           // Column name in ScratchBird
    std::string remote_name;          // Column name in remote DB
    ScratchBirdType local_type;       // ScratchBird type
    uint32_t remote_type_oid;         // Remote type identifier
    bool nullable;                    // NULL allowed
    std::string default_value;        // Default expression
};
```

### 2.4 RemoteQueryResult

Result set from a remote query.

```cpp
struct RemoteQueryResult {
    // Status
    bool success;
    std::string error_message;
    std::string sql_state;            // 5-char SQLSTATE code

    // Metadata
    std::vector<RemoteColumnDesc> columns;
    uint64_t rows_affected;           // For DML statements

    // Data (row-oriented for flexibility)
    std::vector<std::vector<TypedValue>> rows;

    // Statistics
    uint64_t execution_time_us;       // Remote execution time
    uint64_t fetch_time_us;           // Data transfer time
    uint64_t bytes_transferred;       // Network bytes

    // Cursor for large results
    bool has_more;                    // More rows available
    std::string cursor_name;          // Server-side cursor ID
};

struct RemoteColumnDesc {
    std::string name;                 // Column name
    uint32_t type_oid;                // Remote type OID
    int32_t type_modifier;            // Type modifier (e.g., varchar length)
    bool nullable;                    // NULL allowed
    ScratchBirdType mapped_type;      // Mapped ScratchBird type
};
```

---

## 3. Type Mapping Tables

### 3.1 PostgreSQL to ScratchBird

```cpp
// Type mapping from PostgreSQL OIDs to ScratchBird types
const std::unordered_map<uint32_t, ScratchBirdType> pg_type_map = {
    // Boolean
    {16, ScratchBirdType::BOOLEAN},

    // Integers
    {21, ScratchBirdType::SMALLINT},      // int2
    {23, ScratchBirdType::INTEGER},       // int4
    {20, ScratchBirdType::BIGINT},        // int8

    // Floating point
    {700, ScratchBirdType::REAL},         // float4
    {701, ScratchBirdType::DOUBLE},       // float8
    {1700, ScratchBirdType::DECIMAL},     // numeric

    // Character types
    {25, ScratchBirdType::TEXT},          // text
    {1043, ScratchBirdType::VARCHAR},     // varchar
    {1042, ScratchBirdType::CHAR},        // bpchar
    {18, ScratchBirdType::CHAR},          // "char"

    // Binary
    {17, ScratchBirdType::BLOB},          // bytea

    // Date/Time
    {1082, ScratchBirdType::DATE},        // date
    {1083, ScratchBirdType::TIME},        // time
    {1114, ScratchBirdType::TIMESTAMP},   // timestamp
    {1184, ScratchBirdType::TIMESTAMP_TZ}, // timestamptz
    {1186, ScratchBirdType::INTERVAL},    // interval

    // UUID
    {2950, ScratchBirdType::UUID},

    // JSON
    {114, ScratchBirdType::JSON},         // json
    {3802, ScratchBirdType::JSON},        // jsonb

    // Network
    {869, ScratchBirdType::INET},         // inet
    {650, ScratchBirdType::CIDR},         // cidr
    {829, ScratchBirdType::MACADDR},      // macaddr

    // Geometric (map to ScratchBird equivalents)
    {600, ScratchBirdType::POINT},        // point
    {601, ScratchBirdType::LSEG},         // lseg
    {602, ScratchBirdType::PATH},         // path
    {603, ScratchBirdType::BOX},          // box
    {604, ScratchBirdType::POLYGON},      // polygon
    {718, ScratchBirdType::CIRCLE},       // circle

    // Arrays (handled specially - base type + array flag)
    {1007, ScratchBirdType::ARRAY_INT4},  // int4[]
    {1009, ScratchBirdType::ARRAY_TEXT},  // text[]

    // Range types
    {3904, ScratchBirdType::INT4RANGE},
    {3906, ScratchBirdType::NUMRANGE},
    {3908, ScratchBirdType::TSRANGE},
    {3910, ScratchBirdType::TSTZRANGE},
    {3912, ScratchBirdType::DATERANGE},
    {3926, ScratchBirdType::INT8RANGE},
};
```

### 3.2 MySQL to ScratchBird

```cpp
const std::unordered_map<uint8_t, ScratchBirdType> mysql_type_map = {
    // Integers
    {0x01, ScratchBirdType::SMALLINT},    // TINY (with UNSIGNED check)
    {0x02, ScratchBirdType::SMALLINT},    // SHORT
    {0x03, ScratchBirdType::INTEGER},     // LONG
    {0x08, ScratchBirdType::BIGINT},      // LONGLONG
    {0x09, ScratchBirdType::INTEGER},     // INT24

    // Floating point
    {0x04, ScratchBirdType::REAL},        // FLOAT
    {0x05, ScratchBirdType::DOUBLE},      // DOUBLE
    {0x00, ScratchBirdType::DECIMAL},     // DECIMAL

    // String types
    {0x0F, ScratchBirdType::VARCHAR},     // VARCHAR
    {0xFD, ScratchBirdType::VARCHAR},     // VAR_STRING
    {0xFE, ScratchBirdType::CHAR},        // STRING

    // Binary/BLOB
    {0xFC, ScratchBirdType::BLOB},        // BLOB (all sizes)

    // Date/Time
    {0x0A, ScratchBirdType::DATE},        // DATE
    {0x0B, ScratchBirdType::TIME},        // TIME
    {0x0C, ScratchBirdType::TIMESTAMP},   // DATETIME
    {0x07, ScratchBirdType::TIMESTAMP},   // TIMESTAMP
    {0x0D, ScratchBirdType::SMALLINT},    // YEAR

    // JSON
    {0xF5, ScratchBirdType::JSON},        // JSON

    // Bit
    {0x10, ScratchBirdType::VARBIT},      // BIT

    // Spatial (map to geometry)
    {0xFF, ScratchBirdType::GEOMETRY},    // GEOMETRY
};
```

### 3.3 SQL Server to ScratchBird (post-gold)

```cpp
const std::unordered_map<uint8_t, ScratchBirdType> tds_type_map = {
    // Fixed-length types
    {0x30, ScratchBirdType::SMALLINT},    // INT1 (tinyint)
    {0x34, ScratchBirdType::SMALLINT},    // INT2 (smallint)
    {0x38, ScratchBirdType::INTEGER},     // INT4 (int)
    {0x7F, ScratchBirdType::BIGINT},      // INT8 (bigint)
    {0x3E, ScratchBirdType::REAL},        // FLT4 (real)
    {0x3D, ScratchBirdType::DOUBLE},      // FLT8 (float)
    {0x32, ScratchBirdType::BOOLEAN},     // BIT

    // Variable-length types
    {0x6D, ScratchBirdType::DECIMAL},     // DECIMALN
    {0x6E, ScratchBirdType::DECIMAL},     // NUMERICN
    {0xAF, ScratchBirdType::VARCHAR},     // BIGVARCHAR
    {0xE7, ScratchBirdType::VARCHAR},     // NVARCHAR
    {0xA7, ScratchBirdType::CHAR},        // BIGCHAR
    {0xEF, ScratchBirdType::CHAR},        // NCHAR
    {0xA5, ScratchBirdType::BLOB},        // BIGVARBINARY
    {0xAD, ScratchBirdType::BLOB},        // BIGBINARY

    // Date/Time
    {0x28, ScratchBirdType::DATE},        // DATE
    {0x29, ScratchBirdType::TIME},        // TIME
    {0x2A, ScratchBirdType::TIMESTAMP},   // DATETIME2
    {0x2B, ScratchBirdType::TIMESTAMP_TZ}, // DATETIMEOFFSET
    {0x3D, ScratchBirdType::TIMESTAMP},   // DATETIME (legacy)

    // GUID
    {0x24, ScratchBirdType::UUID},        // UNIQUEIDENTIFIER

    // XML
    {0xF1, ScratchBirdType::TEXT},        // XML (as text)
};
```

### 3.4 Firebird to ScratchBird

```cpp
// Firebird SQL types (from ibase.h)
const std::unordered_map<int16_t, ScratchBirdType> firebird_type_map = {
    // Integers
    {SQL_SHORT, ScratchBirdType::SMALLINT},
    {SQL_LONG, ScratchBirdType::INTEGER},
    {SQL_INT64, ScratchBirdType::BIGINT},
    {SQL_INT128, ScratchBirdType::DECIMAL},   // 128-bit as DECIMAL

    // Floating point
    {SQL_FLOAT, ScratchBirdType::REAL},
    {SQL_DOUBLE, ScratchBirdType::DOUBLE},
    {SQL_D_FLOAT, ScratchBirdType::DOUBLE},   // VAX D_FLOAT

    // Fixed point
    {SQL_DEC_FIXED, ScratchBirdType::DECIMAL},
    {SQL_DEC64, ScratchBirdType::DECIMAL},
    {SQL_DEC128, ScratchBirdType::DECIMAL},

    // String types
    {SQL_TEXT, ScratchBirdType::CHAR},
    {SQL_VARYING, ScratchBirdType::VARCHAR},

    // Binary
    {SQL_BLOB, ScratchBirdType::BLOB},

    // Date/Time
    {SQL_TYPE_DATE, ScratchBirdType::DATE},
    {SQL_TYPE_TIME, ScratchBirdType::TIME},
    {SQL_TIMESTAMP, ScratchBirdType::TIMESTAMP},
    {SQL_TIME_TZ, ScratchBirdType::TIME_TZ},
    {SQL_TIMESTAMP_TZ, ScratchBirdType::TIMESTAMP_TZ},

    // Boolean
    {SQL_BOOLEAN, ScratchBirdType::BOOLEAN},

    // NULL type
    {SQL_NULL, ScratchBirdType::UNKNOWN},
};
```

---

## 4. Interface Definitions

### 4.1 IProtocolAdapter

Abstract interface for database-specific protocol adapters.

```cpp
class IProtocolAdapter {
public:
    virtual ~IProtocolAdapter() = default;

    // Connection lifecycle
    virtual Result<void> connect(const ServerDefinition& server,
                                  const UserMapping& mapping) = 0;
    virtual Result<void> disconnect() = 0;
    virtual ConnectionState getState() const = 0;
    virtual bool isConnected() const = 0;

    // Health check
    virtual Result<bool> ping() = 0;
    virtual Result<void> reset() = 0;  // Reset connection state

    // Query execution
    virtual Result<RemoteQueryResult> executeQuery(const std::string& sql) = 0;
    virtual Result<RemoteQueryResult> executeQueryWithParams(
        const std::string& sql,
        const std::vector<TypedValue>& params) = 0;

    // Prepared statements
    virtual Result<uint64_t> prepare(const std::string& sql) = 0;
    virtual Result<RemoteQueryResult> executePrepared(
        uint64_t stmt_id,
        const std::vector<TypedValue>& params) = 0;
    virtual Result<void> deallocatePrepared(uint64_t stmt_id) = 0;

    // Transaction control
    virtual Result<void> beginTransaction() = 0;
    virtual Result<void> commit() = 0;
    virtual Result<void> rollback() = 0;
    virtual Result<void> setSavepoint(const std::string& name) = 0;
    virtual Result<void> rollbackToSavepoint(const std::string& name) = 0;

    // Cursor operations (for large result sets)
    virtual Result<std::string> declareCursor(const std::string& name,
                                               const std::string& sql) = 0;
    virtual Result<RemoteQueryResult> fetchFromCursor(const std::string& name,
                                                       uint32_t count) = 0;
    virtual Result<void> closeCursor(const std::string& name) = 0;

    // Schema introspection
    virtual Result<std::vector<std::string>> listSchemas() = 0;
    virtual Result<std::vector<std::string>> listTables(const std::string& schema) = 0;
    virtual Result<std::vector<RemoteColumnDesc>> describeTable(
        const std::string& schema,
        const std::string& table) = 0;
    virtual Result<std::vector<RemoteIndexDesc>> describeIndexes(
        const std::string& schema,
        const std::string& table) = 0;
    virtual Result<std::vector<RemoteForeignKey>> describeForeignKeys(
        const std::string& schema,
        const std::string& table) = 0;

    // Metadata
    virtual RemoteDatabaseType getDatabaseType() const = 0;
    virtual std::string getServerVersion() const = 0;
    virtual PushdownCapability getCapabilities() const = 0;

    // Type conversion
    virtual ScratchBirdType mapRemoteType(uint32_t remote_oid,
                                           int32_t modifier) const = 0;
    virtual TypedValue convertToLocal(const void* data, size_t len,
                                       uint32_t remote_oid) const = 0;
    virtual std::vector<uint8_t> convertToRemote(const TypedValue& value,
                                                  uint32_t remote_oid) const = 0;

    // Statistics
    virtual ConnectionStats getStats() const = 0;
};
```

### 4.2 IConnectionPool

Interface for the remote connection pool.

```cpp
class IConnectionPool {
public:
    virtual ~IConnectionPool() = default;

    // Lifecycle
    virtual Result<void> initialize(const ServerDefinition& server) = 0;
    virtual Result<void> shutdown() = 0;

    // Connection acquisition
    virtual Result<std::shared_ptr<IProtocolAdapter>> acquire(
        const std::string& local_user,
        std::chrono::milliseconds timeout) = 0;
    virtual void release(std::shared_ptr<IProtocolAdapter> conn) = 0;

    // Pool management
    virtual void setMinConnections(uint32_t min) = 0;
    virtual void setMaxConnections(uint32_t max) = 0;
    virtual Result<void> resize(uint32_t target) = 0;
    virtual Result<void> evictIdle() = 0;

    // Health
    virtual Result<void> validateAll() = 0;
    virtual PoolStats getStats() const = 0;
    virtual bool isHealthy() const = 0;
};

struct PoolStats {
    uint32_t total_connections;
    uint32_t active_connections;
    uint32_t idle_connections;
    uint32_t waiting_requests;
    uint64_t total_acquires;
    uint64_t total_releases;
    uint64_t acquire_timeouts;
    uint64_t connection_errors;
    uint64_t avg_acquire_time_us;
    uint64_t max_acquire_time_us;
};
```

---

## 5. Error Codes

### 5.1 Remote Database SQLSTATE Codes

```cpp
// Custom SQLSTATE codes for remote database errors
// Class RD = Remote Database
namespace remote_sqlstate {
    // Connection errors (RD001-RD009)
    constexpr const char* CONNECTION_FAILED     = "RD001";  // Cannot connect
    constexpr const char* AUTH_FAILED           = "RD002";  // Authentication failed
    constexpr const char* SSL_ERROR             = "RD003";  // TLS/SSL error
    constexpr const char* TIMEOUT               = "RD004";  // Connection/query timeout
    constexpr const char* POOL_EXHAUSTED        = "RD005";  // No available connections
    constexpr const char* SERVER_UNAVAILABLE    = "RD006";  // Server not responding
    constexpr const char* NETWORK_ERROR         = "RD007";  // Network failure
    constexpr const char* PROTOCOL_ERROR        = "RD008";  // Protocol violation
    constexpr const char* VERSION_MISMATCH      = "RD009";  // Unsupported server version

    // Query errors (RD010-RD019)
    constexpr const char* REMOTE_SYNTAX_ERROR   = "RD010";  // Remote SQL syntax error
    constexpr const char* REMOTE_PERMISSION     = "RD011";  // Remote permission denied
    constexpr const char* REMOTE_OBJECT_NOT_FOUND = "RD012";  // Table/column not found
    constexpr const char* TYPE_CONVERSION       = "RD013";  // Type mapping failed
    constexpr const char* PUSHDOWN_FAILED       = "RD014";  // Pushdown not supported
    constexpr const char* CURSOR_ERROR          = "RD015";  // Cursor operation failed
    constexpr const char* PREPARED_STMT_ERROR   = "RD016";  // Prepared statement error

    // Transaction errors (RD020-RD029)
    constexpr const char* REMOTE_DEADLOCK       = "RD020";  // Remote deadlock detected
    constexpr const char* REMOTE_SERIALIZATION  = "RD021";  // Serialization failure
    constexpr const char* REMOTE_CONSTRAINT     = "RD022";  // Remote constraint violation
    constexpr const char* TRANSACTION_ABORTED   = "RD023";  // Remote transaction aborted

    // Configuration errors (RD030-RD039)
    constexpr const char* SERVER_NOT_FOUND      = "RD030";  // Foreign server not defined
    constexpr const char* USER_MAPPING_MISSING  = "RD031";  // No user mapping
    constexpr const char* INVALID_OPTION        = "RD032";  // Invalid server option
    constexpr const char* FOREIGN_TABLE_ERROR   = "RD033";  // Foreign table definition error
}
```

---

## 6. Constants and Limits

```cpp
namespace remote_db_limits {
    // Connection pool
    constexpr uint32_t DEFAULT_MIN_CONNECTIONS = 1;
    constexpr uint32_t DEFAULT_MAX_CONNECTIONS = 10;
    constexpr uint32_t MAX_CONNECTIONS_PER_SERVER = 100;

    // Timeouts (milliseconds)
    constexpr uint32_t DEFAULT_CONNECT_TIMEOUT = 5000;
    constexpr uint32_t DEFAULT_QUERY_TIMEOUT = 30000;
    constexpr uint32_t DEFAULT_IDLE_TIMEOUT = 300000;  // 5 minutes
    constexpr uint32_t MAX_QUERY_TIMEOUT = 3600000;     // 1 hour

    // Buffer sizes
    constexpr size_t DEFAULT_RECEIVE_BUFFER = 65536;
    constexpr size_t DEFAULT_SEND_BUFFER = 65536;
    constexpr size_t MAX_PACKET_SIZE = 16777216;  // 16 MB

    // Batch sizes
    constexpr uint32_t DEFAULT_FETCH_SIZE = 1000;
    constexpr uint32_t MAX_FETCH_SIZE = 100000;
    constexpr uint32_t DEFAULT_BATCH_INSERT_SIZE = 1000;

    // Query limits
    constexpr size_t MAX_SQL_LENGTH = 16777216;  // 16 MB
    constexpr uint32_t MAX_PARAMETERS = 65535;
    constexpr uint32_t MAX_COLUMNS = 1664;

    // Health check
    constexpr uint32_t HEALTH_CHECK_INTERVAL_MS = 30000;
    constexpr uint32_t HEALTH_CHECK_TIMEOUT_MS = 5000;
}
```
