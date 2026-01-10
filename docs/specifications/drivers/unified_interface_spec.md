# Unified C++ Database Interface Specification

## Overview

This specification defines a unified C++ interface that abstracts database-specific implementations, allowing applications to work with multiple database engines (MySQL, MariaDB, PostgreSQL, MSSQL) through a common API.

**Scope Note:** MSSQL/TDS support is post-gold; MSSQL entries are forward-looking.

## Architecture Design

### Layer Structure

```
┌─────────────────────────────────────────┐
│         Application Layer               │
├─────────────────────────────────────────┤
│    Database Interface Layer (Abstract)  │
├─────────────────────────────────────────┤
│      Database Factory & Manager         │
├─────────────────────────────────────────┤
│   Database Driver Implementations       │
│  ┌──────┬──────┬──────────┬─────────┐ │
│  │MySQL │PostG │  MSSQL   │MariaDB  │ │
│  │Driver│reSQL │  Driver  │Driver   │ │
│  └──────┴──────┴──────────┴─────────┘ │
├─────────────────────────────────────────┤
│        Native Database APIs             │
│  ┌──────┬──────┬──────────┬─────────┐ │
│  │MySQL │libpq │  ODBC    │MariaDB  │ │
│  │C API │libpqx│ FreeTDS  │Connector│ │
│  └──────┴──────┴──────────┴─────────┘ │
└─────────────────────────────────────────┘
```

## Core Interface Definitions

### 1. Database Types Enumeration

```cpp
#ifndef DB_INTERFACE_H
#define DB_INTERFACE_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <optional>
#include <variant>
#include <chrono>
#include <exception>

namespace dbinterface {

// Database engine types
enum class DatabaseType {
    MySQL,
    MariaDB,
    PostgreSQL,
    MSSQL,
    Unknown
};

// Data types for type-safe operations
enum class DataType {
    Null,
    Boolean,
    Integer,
    BigInt,
    Float,
    Double,
    Decimal,
    String,
    Binary,
    Date,
    Time,
    DateTime,
    Timestamp,
    Json,
    Xml,
    Array,
    Unknown
};

// Transaction isolation levels
enum class IsolationLevel {
    ReadUncommitted,
    ReadCommitted,
    RepeatableRead,
    Serializable,
    Snapshot
};

// Connection states
enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Executing,
    Fetching,
    Error
};
```

### 2. Value and Result Types

```cpp
// Type-safe value container
class DbValue {
private:
    using ValueType = std::variant<
        std::monostate,                    // NULL
        bool,                               // Boolean
        int32_t,                           // Integer
        int64_t,                           // BigInt
        float,                             // Float
        double,                            // Double
        std::string,                       // String/Text
        std::vector<uint8_t>,              // Binary/Blob
        std::chrono::system_clock::time_point  // DateTime
    >;
    
    ValueType value;
    DataType type;
    
public:
    DbValue() : value(std::monostate{}), type(DataType::Null) {}
    
    template<typename T>
    DbValue(T val) : value(val) {
        // Deduce type from T
        if constexpr (std::is_same_v<T, bool>) {
            type = DataType::Boolean;
        } else if constexpr (std::is_same_v<T, int32_t>) {
            type = DataType::Integer;
        } else if constexpr (std::is_same_v<T, int64_t>) {
            type = DataType::BigInt;
        } else if constexpr (std::is_same_v<T, float>) {
            type = DataType::Float;
        } else if constexpr (std::is_same_v<T, double>) {
            type = DataType::Double;
        } else if constexpr (std::is_same_v<T, std::string>) {
            type = DataType::String;
        } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            type = DataType::Binary;
        } else if constexpr (std::is_same_v<T, std::chrono::system_clock::time_point>) {
            type = DataType::DateTime;
        } else {
            type = DataType::Unknown;
        }
    }
    
    bool isNull() const { return std::holds_alternative<std::monostate>(value); }
    DataType getType() const { return type; }
    
    template<typename T>
    T as() const {
        if (auto* val = std::get_if<T>(&value)) {
            return *val;
        }
        throw std::runtime_error("Type conversion error");
    }
    
    std::string toString() const;
    int32_t toInt() const;
    int64_t toBigInt() const;
    double toDouble() const;
    bool toBool() const;
    std::vector<uint8_t> toBinary() const;
    std::chrono::system_clock::time_point toDateTime() const;
};

// Row representation
using DbRow = std::map<std::string, DbValue>;

// Result set
class DbResultSet {
private:
    std::vector<std::string> columns;
    std::vector<DataType> columnTypes;
    std::vector<DbRow> rows;
    size_t currentRow;
    
public:
    DbResultSet() : currentRow(0) {}
    
    // Navigation
    bool next() { 
        if (currentRow < rows.size() - 1) {
            currentRow++;
            return true;
        }
        return false;
    }
    
    bool previous() {
        if (currentRow > 0) {
            currentRow--;
            return true;
        }
        return false;
    }
    
    bool first() {
        currentRow = 0;
        return !rows.empty();
    }
    
    bool last() {
        if (!rows.empty()) {
            currentRow = rows.size() - 1;
            return true;
        }
        return false;
    }
    
    bool absolute(size_t row) {
        if (row < rows.size()) {
            currentRow = row;
            return true;
        }
        return false;
    }
    
    // Data access
    DbValue getValue(const std::string& column) const {
        if (currentRow < rows.size()) {
            auto it = rows[currentRow].find(column);
            if (it != rows[currentRow].end()) {
                return it->second;
            }
        }
        return DbValue();
    }
    
    DbValue getValue(size_t columnIndex) const {
        if (columnIndex < columns.size()) {
            return getValue(columns[columnIndex]);
        }
        return DbValue();
    }
    
    DbRow getCurrentRow() const {
        if (currentRow < rows.size()) {
            return rows[currentRow];
        }
        return DbRow();
    }
    
    // Metadata
    size_t getRowCount() const { return rows.size(); }
    size_t getColumnCount() const { return columns.size(); }
    std::vector<std::string> getColumnNames() const { return columns; }
    DataType getColumnType(size_t index) const {
        if (index < columnTypes.size()) {
            return columnTypes[index];
        }
        return DataType::Unknown;
    }
    
    // Bulk access
    const std::vector<DbRow>& getAllRows() const { return rows; }
    
    // Internal use
    void addColumn(const std::string& name, DataType type) {
        columns.push_back(name);
        columnTypes.push_back(type);
    }
    
    void addRow(const DbRow& row) {
        rows.push_back(row);
    }
};
```

### 3. Connection Configuration

```cpp
// Connection configuration
struct ConnectionConfig {
    std::string host = "localhost";
    uint16_t port = 0;  // 0 means use default for database type
    std::string database;
    std::string username;
    std::string password;
    
    // SSL/TLS settings
    bool useSSL = false;
    std::string sslCert;
    std::string sslKey;
    std::string sslCA;
    bool verifyCertificate = true;
    
    // Connection pool settings
    size_t poolSize = 10;
    size_t maxPoolSize = 100;
    std::chrono::seconds connectionTimeout{30};
    std::chrono::seconds idleTimeout{600};
    
    // Additional options
    std::map<std::string, std::string> options;
    
    // Charset and collation
    std::string charset = "utf8mb4";
    std::string collation;
    
    // Application identification
    std::string applicationName;
    
    // Retry settings
    size_t maxRetries = 3;
    std::chrono::milliseconds retryDelay{1000};
    
    // Get default port for database type
    uint16_t getPort(DatabaseType type) const {
        if (port != 0) return port;
        
        switch (type) {
            case DatabaseType::MySQL:
            case DatabaseType::MariaDB:
                return 3306;
            case DatabaseType::PostgreSQL:
                return 5432;
            case DatabaseType::MSSQL:
                return 1433;
            default:
                return 0;
        }
    }
};
```

### 4. Abstract Database Interface

```cpp
// Forward declarations
class IPreparedStatement;
class ITransaction;

// Main database interface
class IDatabase {
public:
    virtual ~IDatabase() = default;
    
    // Connection management
    virtual bool connect(const ConnectionConfig& config) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual ConnectionState getState() const = 0;
    virtual bool ping() = 0;
    virtual bool reconnect() = 0;
    
    // Query execution
    virtual DbResultSet executeQuery(const std::string& query) = 0;
    virtual int64_t executeUpdate(const std::string& query) = 0;
    virtual bool execute(const std::string& query) = 0;
    
    // Prepared statements
    virtual std::unique_ptr<IPreparedStatement> prepare(const std::string& query) = 0;
    
    // Transactions
    virtual std::unique_ptr<ITransaction> beginTransaction(
        IsolationLevel level = IsolationLevel::ReadCommitted) = 0;
    
    // Batch operations
    virtual bool executeBatch(const std::vector<std::string>& queries) = 0;
    
    // Metadata
    virtual std::vector<std::string> getTables(const std::string& schema = "") = 0;
    virtual std::vector<std::string> getDatabases() = 0;
    virtual DbResultSet getTableColumns(const std::string& table, 
                                        const std::string& schema = "") = 0;
    virtual DbResultSet getTableIndexes(const std::string& table,
                                        const std::string& schema = "") = 0;
    
    // Database-specific features
    virtual std::string getServerVersion() = 0;
    virtual DatabaseType getDatabaseType() const = 0;
    virtual std::string escapeString(const std::string& str) = 0;
    virtual std::string quoteIdentifier(const std::string& identifier) = 0;
    
    // Last insert ID (for auto-increment columns)
    virtual int64_t getLastInsertId() = 0;
    
    // Error handling
    virtual std::string getLastError() const = 0;
    virtual int getLastErrorCode() const = 0;
    virtual std::string getLastSQLState() const = 0;
};

// Prepared statement interface
class IPreparedStatement {
public:
    virtual ~IPreparedStatement() = default;
    
    // Parameter binding
    virtual void setNull(size_t paramIndex) = 0;
    virtual void setBool(size_t paramIndex, bool value) = 0;
    virtual void setInt(size_t paramIndex, int32_t value) = 0;
    virtual void setBigInt(size_t paramIndex, int64_t value) = 0;
    virtual void setFloat(size_t paramIndex, float value) = 0;
    virtual void setDouble(size_t paramIndex, double value) = 0;
    virtual void setString(size_t paramIndex, const std::string& value) = 0;
    virtual void setBinary(size_t paramIndex, const std::vector<uint8_t>& value) = 0;
    virtual void setDateTime(size_t paramIndex, 
                            const std::chrono::system_clock::time_point& value) = 0;
    
    // Generic parameter binding
    virtual void setParameter(size_t paramIndex, const DbValue& value) = 0;
    
    // Batch parameter binding
    virtual void addBatch() = 0;
    virtual void clearBatch() = 0;
    
    // Execution
    virtual DbResultSet executeQuery() = 0;
    virtual int64_t executeUpdate() = 0;
    virtual std::vector<int64_t> executeBatch() = 0;
    
    // Clear parameters
    virtual void clearParameters() = 0;
};

// Transaction interface
class ITransaction {
public:
    virtual ~ITransaction() = default;
    
    virtual void commit() = 0;
    virtual void rollback() = 0;
    
    // Savepoints
    virtual void setSavepoint(const std::string& name) = 0;
    virtual void releaseSavepoint(const std::string& name) = 0;
    virtual void rollbackToSavepoint(const std::string& name) = 0;
    
    // Transaction state
    virtual bool isActive() const = 0;
    virtual IsolationLevel getIsolationLevel() const = 0;
};
```

### 5. Database Factory

```cpp
// Database factory for creating instances
class DatabaseFactory {
public:
    using CreatorFunc = std::function<std::unique_ptr<IDatabase>()>;
    
private:
    static std::map<DatabaseType, CreatorFunc> creators;
    
public:
    // Register a database implementation
    static void registerDatabase(DatabaseType type, CreatorFunc creator) {
        creators[type] = creator;
    }
    
    // Create a database instance
    static std::unique_ptr<IDatabase> create(DatabaseType type) {
        auto it = creators.find(type);
        if (it != creators.end()) {
            return it->second();
        }
        throw std::runtime_error("Unsupported database type");
    }
    
    // Create from connection string
    static std::unique_ptr<IDatabase> createFromConnectionString(
        const std::string& connectionString) {
        
        // Parse connection string to determine type
        DatabaseType type = parseConnectionString(connectionString);
        return create(type);
    }
    
private:
    static DatabaseType parseConnectionString(const std::string& connStr) {
        if (connStr.find("mysql://") == 0) return DatabaseType::MySQL;
        if (connStr.find("mariadb://") == 0) return DatabaseType::MariaDB;
        if (connStr.find("postgresql://") == 0) return DatabaseType::PostgreSQL;
        if (connStr.find("mssql://") == 0) return DatabaseType::MSSQL;
        if (connStr.find("sqlserver://") == 0) return DatabaseType::MSSQL;
        return DatabaseType::Unknown;
    }
};

// Initialize static member
std::map<DatabaseType, DatabaseFactory::CreatorFunc> DatabaseFactory::creators;
```

### 6. Connection Pool

```cpp
// Connection pool for efficient resource management
template<typename T>
class ConnectionPool {
private:
    struct PooledConnection {
        std::unique_ptr<T> connection;
        std::chrono::steady_clock::time_point lastUsed;
        bool inUse;
    };
    
    std::vector<PooledConnection> pool;
    ConnectionConfig config;
    size_t minSize;
    size_t maxSize;
    std::chrono::seconds idleTimeout;
    std::mutex poolMutex;
    std::condition_variable poolCV;
    
    // Create new connection
    std::unique_ptr<T> createConnection() {
        auto conn = std::make_unique<T>();
        if (!conn->connect(config)) {
            throw std::runtime_error("Failed to create connection");
        }
        return conn;
    }
    
    // Check if connection is still valid
    bool isConnectionValid(T* conn) {
        return conn && conn->isConnected() && conn->ping();
    }
    
    // Remove idle connections
    void cleanupIdleConnections() {
        auto now = std::chrono::steady_clock::now();
        
        for (auto it = pool.begin(); it != pool.end();) {
            if (!it->inUse && 
                std::chrono::duration_cast<std::chrono::seconds>(
                    now - it->lastUsed) > idleTimeout) {
                it = pool.erase(it);
            } else {
                ++it;
            }
        }
    }
    
public:
    ConnectionPool(const ConnectionConfig& cfg, size_t min = 5, size_t max = 20,
                   std::chrono::seconds timeout = std::chrono::seconds(600))
        : config(cfg), minSize(min), maxSize(max), idleTimeout(timeout) {
        
        // Pre-create minimum connections
        for (size_t i = 0; i < minSize; ++i) {
            pool.push_back({createConnection(), 
                           std::chrono::steady_clock::now(), false});
        }
    }
    
    // Get connection from pool
    std::unique_ptr<T> acquire() {
        std::unique_lock<std::mutex> lock(poolMutex);
        
        // Clean up idle connections
        cleanupIdleConnections();
        
        // Find available connection
        for (auto& pc : pool) {
            if (!pc.inUse && isConnectionValid(pc.connection.get())) {
                pc.inUse = true;
                pc.lastUsed = std::chrono::steady_clock::now();
                return std::move(pc.connection);
            }
        }
        
        // Create new connection if under max size
        if (pool.size() < maxSize) {
            auto conn = createConnection();
            pool.push_back({nullptr, std::chrono::steady_clock::now(), true});
            return conn;
        }
        
        // Wait for available connection
        poolCV.wait(lock, [this] {
            for (const auto& pc : pool) {
                if (!pc.inUse) return true;
            }
            return false;
        });
        
        // Retry after wait
        return acquire();
    }
    
    // Return connection to pool
    void release(std::unique_ptr<T> conn) {
        if (!conn) return;
        
        std::lock_guard<std::mutex> lock(poolMutex);
        
        // Check if connection is still valid
        if (isConnectionValid(conn.get())) {
            // Find slot or add new
            bool found = false;
            for (auto& pc : pool) {
                if (!pc.connection) {
                    pc.connection = std::move(conn);
                    pc.inUse = false;
                    pc.lastUsed = std::chrono::steady_clock::now();
                    found = true;
                    break;
                }
            }
            
            if (!found && pool.size() < maxSize) {
                pool.push_back({std::move(conn), 
                               std::chrono::steady_clock::now(), false});
            }
        }
        
        poolCV.notify_one();
    }
    
    // Get pool statistics
    struct PoolStats {
        size_t totalConnections;
        size_t activeConnections;
        size_t idleConnections;
    };
    
    PoolStats getStats() const {
        std::lock_guard<std::mutex> lock(poolMutex);
        
        PoolStats stats{};
        stats.totalConnections = pool.size();
        
        for (const auto& pc : pool) {
            if (pc.inUse) {
                stats.activeConnections++;
            } else {
                stats.idleConnections++;
            }
        }
        
        return stats;
    }
};
```

### 7. Query Builder

```cpp
// SQL Query Builder for type-safe query construction
class QueryBuilder {
private:
    DatabaseType dbType;
    std::stringstream query;
    std::vector<DbValue> parameters;
    
    std::string quoteIdentifier(const std::string& identifier) {
        switch (dbType) {
            case DatabaseType::MySQL:
            case DatabaseType::MariaDB:
                return "`" + identifier + "`";
            case DatabaseType::PostgreSQL:
                return "\"" + identifier + "\"";
            case DatabaseType::MSSQL:
                return "[" + identifier + "]";
            default:
                return identifier;
        }
    }
    
public:
    QueryBuilder(DatabaseType type) : dbType(type) {}
    
    // SELECT
    QueryBuilder& select(const std::vector<std::string>& columns = {"*"}) {
        query << "SELECT ";
        for (size_t i = 0; i < columns.size(); ++i) {
            if (columns[i] == "*") {
                query << "*";
            } else {
                query << quoteIdentifier(columns[i]);
            }
            if (i < columns.size() - 1) query << ", ";
        }
        return *this;
    }
    
    QueryBuilder& from(const std::string& table, const std::string& alias = "") {
        query << " FROM " << quoteIdentifier(table);
        if (!alias.empty()) {
            query << " AS " << alias;
        }
        return *this;
    }
    
    QueryBuilder& join(const std::string& table, const std::string& condition,
                       const std::string& type = "INNER") {
        query << " " << type << " JOIN " << quoteIdentifier(table) 
              << " ON " << condition;
        return *this;
    }
    
    QueryBuilder& where(const std::string& condition) {
        query << " WHERE " << condition;
        return *this;
    }
    
    QueryBuilder& whereEqual(const std::string& column, const DbValue& value) {
        query << " WHERE " << quoteIdentifier(column) << " = ?";
        parameters.push_back(value);
        return *this;
    }
    
    QueryBuilder& andWhere(const std::string& condition) {
        query << " AND " << condition;
        return *this;
    }
    
    QueryBuilder& orWhere(const std::string& condition) {
        query << " OR " << condition;
        return *this;
    }
    
    QueryBuilder& groupBy(const std::vector<std::string>& columns) {
        query << " GROUP BY ";
        for (size_t i = 0; i < columns.size(); ++i) {
            query << quoteIdentifier(columns[i]);
            if (i < columns.size() - 1) query << ", ";
        }
        return *this;
    }
    
    QueryBuilder& having(const std::string& condition) {
        query << " HAVING " << condition;
        return *this;
    }
    
    QueryBuilder& orderBy(const std::string& column, bool ascending = true) {
        query << " ORDER BY " << quoteIdentifier(column) 
              << (ascending ? " ASC" : " DESC");
        return *this;
    }
    
    QueryBuilder& limit(size_t limit, size_t offset = 0) {
        switch (dbType) {
            case DatabaseType::MySQL:
            case DatabaseType::MariaDB:
            case DatabaseType::PostgreSQL:
                query << " LIMIT " << limit;
                if (offset > 0) {
                    query << " OFFSET " << offset;
                }
                break;
            case DatabaseType::MSSQL:
                if (offset == 0) {
                    // Use TOP for simple limit
                    std::string q = query.str();
                    size_t selectPos = q.find("SELECT");
                    if (selectPos != std::string::npos) {
                        q.insert(selectPos + 6, " TOP " + std::to_string(limit));
                        query.str(q);
                    }
                } else {
                    // Use OFFSET-FETCH for pagination
                    query << " OFFSET " << offset << " ROWS "
                          << "FETCH NEXT " << limit << " ROWS ONLY";
                }
                break;
        }
        return *this;
    }
    
    // INSERT
    QueryBuilder& insertInto(const std::string& table, 
                             const std::vector<std::string>& columns) {
        query << "INSERT INTO " << quoteIdentifier(table) << " (";
        for (size_t i = 0; i < columns.size(); ++i) {
            query << quoteIdentifier(columns[i]);
            if (i < columns.size() - 1) query << ", ";
        }
        query << ")";
        return *this;
    }
    
    QueryBuilder& values(const std::vector<DbValue>& values) {
        query << " VALUES (";
        for (size_t i = 0; i < values.size(); ++i) {
            query << "?";
            parameters.push_back(values[i]);
            if (i < values.size() - 1) query << ", ";
        }
        query << ")";
        return *this;
    }
    
    // UPDATE
    QueryBuilder& update(const std::string& table) {
        query << "UPDATE " << quoteIdentifier(table);
        return *this;
    }
    
    QueryBuilder& set(const std::map<std::string, DbValue>& assignments) {
        query << " SET ";
        size_t i = 0;
        for (const auto& [column, value] : assignments) {
            query << quoteIdentifier(column) << " = ?";
            parameters.push_back(value);
            if (++i < assignments.size()) query << ", ";
        }
        return *this;
    }
    
    // DELETE
    QueryBuilder& deleteFrom(const std::string& table) {
        query << "DELETE FROM " << quoteIdentifier(table);
        return *this;
    }
    
    // Build final query
    std::string build() const {
        return query.str();
    }
    
    const std::vector<DbValue>& getParameters() const {
        return parameters;
    }
    
    void addParameter(const DbValue& value) {
        parameters.push_back(value);
    }
};
```

### 8. Exception Hierarchy

```cpp
// Database exception hierarchy
class DatabaseException : public std::exception {
protected:
    std::string message;
    int errorCode;
    std::string sqlState;
    
public:
    DatabaseException(const std::string& msg, int code = 0, 
                     const std::string& state = "")
        : message(msg), errorCode(code), sqlState(state) {}
    
    const char* what() const noexcept override {
        return message.c_str();
    }
    
    int getErrorCode() const { return errorCode; }
    const std::string& getSQLState() const { return sqlState; }
};

class ConnectionException : public DatabaseException {
public:
    using DatabaseException::DatabaseException;
};

class QueryException : public DatabaseException {
private:
    std::string query;
    
public:
    QueryException(const std::string& msg, const std::string& q, 
                  int code = 0, const std::string& state = "")
        : DatabaseException(msg, code, state), query(q) {}
    
    const std::string& getQuery() const { return query; }
};

class TransactionException : public DatabaseException {
public:
    using DatabaseException::DatabaseException;
};

class TimeoutException : public DatabaseException {
public:
    using DatabaseException::DatabaseException;
};

class DataException : public DatabaseException {
public:
    using DatabaseException::DatabaseException;
};

} // namespace dbinterface

#endif // DB_INTERFACE_H
```

## Implementation Example

```cpp
#include "db_interface.h"
#include <iostream>

using namespace dbinterface;

// Example implementation for MySQL
class MySQLDatabase : public IDatabase {
private:
    // MySQL-specific members
    void* conn;  // Would be MYSQL* in real implementation
    ConnectionState state;
    std::string lastError;
    int lastErrorCode;
    
public:
    MySQLDatabase() : conn(nullptr), state(ConnectionState::Disconnected) {}
    
    bool connect(const ConnectionConfig& config) override {
        // MySQL-specific connection logic
        // This would use the MySQL C API
        state = ConnectionState::Connected;
        return true;
    }
    
    void disconnect() override {
        // MySQL-specific disconnection
        state = ConnectionState::Disconnected;
    }
    
    bool isConnected() const override {
        return state == ConnectionState::Connected;
    }
    
    ConnectionState getState() const override {
        return state;
    }
    
    bool ping() override {
        // MySQL ping implementation
        return true;
    }
    
    bool reconnect() override {
        // MySQL reconnection logic
        return true;
    }
    
    DbResultSet executeQuery(const std::string& query) override {
        DbResultSet result;
        // MySQL query execution
        return result;
    }
    
    int64_t executeUpdate(const std::string& query) override {
        // MySQL update execution
        return 0;
    }
    
    bool execute(const std::string& query) override {
        // MySQL generic execution
        return true;
    }
    
    std::unique_ptr<IPreparedStatement> prepare(const std::string& query) override {
        // Return MySQL prepared statement implementation
        return nullptr;
    }
    
    std::unique_ptr<ITransaction> beginTransaction(IsolationLevel level) override {
        // Return MySQL transaction implementation
        return nullptr;
    }
    
    bool executeBatch(const std::vector<std::string>& queries) override {
        // MySQL batch execution
        return true;
    }
    
    std::vector<std::string> getTables(const std::string& schema) override {
        // MySQL table listing
        return {};
    }
    
    std::vector<std::string> getDatabases() override {
        // MySQL database listing
        return {};
    }
    
    DbResultSet getTableColumns(const std::string& table, 
                                const std::string& schema) override {
        // MySQL column metadata
        return DbResultSet();
    }
    
    DbResultSet getTableIndexes(const std::string& table,
                                const std::string& schema) override {
        // MySQL index metadata
        return DbResultSet();
    }
    
    std::string getServerVersion() override {
        return "MySQL 8.0.0";
    }
    
    DatabaseType getDatabaseType() const override {
        return DatabaseType::MySQL;
    }
    
    std::string escapeString(const std::string& str) override {
        // MySQL string escaping
        return str;
    }
    
    std::string quoteIdentifier(const std::string& identifier) override {
        return "`" + identifier + "`";
    }
    
    int64_t getLastInsertId() override {
        // MySQL last insert ID
        return 0;
    }
    
    std::string getLastError() const override {
        return lastError;
    }
    
    int getLastErrorCode() const override {
        return lastErrorCode;
    }
    
    std::string getLastSQLState() const override {
        return "00000";
    }
};

// Register implementations
void registerDatabases() {
    DatabaseFactory::registerDatabase(DatabaseType::MySQL, 
        []() { return std::make_unique<MySQLDatabase>(); });
    
    // Register other database implementations
    // DatabaseFactory::registerDatabase(DatabaseType::PostgreSQL, ...);
    // DatabaseFactory::registerDatabase(DatabaseType::MSSQL, ...);
    // DatabaseFactory::registerDatabase(DatabaseType::MariaDB, ...);
}

// Usage example
int main() {
    // Register database implementations
    registerDatabases();
    
    // Create database instance
    auto db = DatabaseFactory::create(DatabaseType::MySQL);
    
    // Configure connection
    ConnectionConfig config;
    config.host = "localhost";
    config.port = 3306;
    config.database = "testdb";
    config.username = "root";
    config.password = "password";
    config.useSSL = true;
    
    // Connect
    if (db->connect(config)) {
        std::cout << "Connected to " << db->getServerVersion() << std::endl;
        
        // Use query builder
        QueryBuilder qb(db->getDatabaseType());
        std::string query = qb.select({"id", "name", "email"})
                             .from("users")
                             .where("age > 18")
                             .orderBy("name")
                             .limit(10)
                             .build();
        
        // Execute query
        auto result = db->executeQuery(query);
        
        // Process results
        while (result.next()) {
            auto id = result.getValue("id").toInt();
            auto name = result.getValue("name").toString();
            auto email = result.getValue("email").toString();
            
            std::cout << "User: " << id << ", " << name << ", " << email << std::endl;
        }
        
        // Use prepared statement
        auto stmt = db->prepare("INSERT INTO users (name, email, age) VALUES (?, ?, ?)");
        stmt->setString(1, "John Doe");
        stmt->setString(2, "john@example.com");
        stmt->setInt(3, 30);
        auto affected = stmt->executeUpdate();
        
        std::cout << "Inserted " << affected << " rows" << std::endl;
        std::cout << "Last insert ID: " << db->getLastInsertId() << std::endl;
        
        // Use transaction
        auto txn = db->beginTransaction(IsolationLevel::ReadCommitted);
        try {
            db->executeUpdate("UPDATE users SET age = age + 1");
            db->executeUpdate("DELETE FROM old_users WHERE age > 100");
            txn->commit();
        } catch (const DatabaseException& e) {
            txn->rollback();
            std::cerr << "Transaction failed: " << e.what() << std::endl;
        }
        
        // Use connection pool
        ConnectionPool<MySQLDatabase> pool(config, 5, 20);
        
        auto pooledConn = pool.acquire();
        // Use connection...
        pool.release(std::move(pooledConn));
        
        // Get pool statistics
        auto stats = pool.getStats();
        std::cout << "Pool: " << stats.totalConnections << " total, "
                  << stats.activeConnections << " active, "
                  << stats.idleConnections << " idle" << std::endl;
        
        // Disconnect
        db->disconnect();
    }
    
    return 0;
}
```

## Build Configuration (CMake)

```cmake
cmake_minimum_required(VERSION 3.14)
project(DatabaseInterface VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)

# MySQL/MariaDB
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(MARIADB libmariadb)
endif()

# PostgreSQL
find_package(PostgreSQL)

# ODBC for MSSQL
if(WIN32)
    set(ODBC_LIBRARIES odbc32)
else()
    find_package(ODBC)
endif()

# Library target
add_library(dbinterface STATIC
    src/database_factory.cpp
    src/connection_pool.cpp
    src/query_builder.cpp
    src/mysql_database.cpp
    src/postgresql_database.cpp
    src/mssql_database.cpp
    src/mariadb_database.cpp
)

target_include_directories(dbinterface PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link libraries based on availability
target_link_libraries(dbinterface PUBLIC Threads::Threads)

if(MARIADB_FOUND)
    target_link_libraries(dbinterface PRIVATE ${MARIADB_LIBRARIES})
    target_include_directories(dbinterface PRIVATE ${MARIADB_INCLUDE_DIRS})
    target_compile_definitions(dbinterface PRIVATE HAS_MARIADB)
endif()

if(PostgreSQL_FOUND)
    target_link_libraries(dbinterface PRIVATE PostgreSQL::PostgreSQL)
    target_compile_definitions(dbinterface PRIVATE HAS_POSTGRESQL)
endif()

if(ODBC_FOUND OR WIN32)
    target_link_libraries(dbinterface PRIVATE ${ODBC_LIBRARIES})
    target_compile_definitions(dbinterface PRIVATE HAS_ODBC)
endif()

# Installation
install(TARGETS dbinterface
    EXPORT DatabaseInterfaceTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/
    DESTINATION include
)

# Export targets
install(EXPORT DatabaseInterfaceTargets
    FILE DatabaseInterfaceTargets.cmake
    NAMESPACE DatabaseInterface::
    DESTINATION lib/cmake/DatabaseInterface
)

# Example executable
add_executable(db_example examples/main.cpp)
target_link_libraries(db_example PRIVATE dbinterface)
```

## Testing Framework

```cpp
#include <gtest/gtest.h>
#include "db_interface.h"

using namespace dbinterface;

class DatabaseTest : public ::testing::Test {
protected:
    std::unique_ptr<IDatabase> db;
    ConnectionConfig config;
    
    void SetUp() override {
        // Setup test database connection
        config.host = "localhost";
        config.database = "test_db";
        config.username = "test_user";
        config.password = "test_pass";
    }
    
    void TearDown() override {
        if (db && db->isConnected()) {
            db->disconnect();
        }
    }
};

TEST_F(DatabaseTest, Connection) {
    db = DatabaseFactory::create(DatabaseType::MySQL);
    ASSERT_TRUE(db->connect(config));
    EXPECT_TRUE(db->isConnected());
    EXPECT_EQ(db->getState(), ConnectionState::Connected);
}

TEST_F(DatabaseTest, QueryExecution) {
    db = DatabaseFactory::create(DatabaseType::MySQL);
    ASSERT_TRUE(db->connect(config));
    
    auto result = db->executeQuery("SELECT 1 AS test");
    EXPECT_EQ(result.getRowCount(), 1);
    EXPECT_EQ(result.getColumnCount(), 1);
    
    result.first();
    EXPECT_EQ(result.getValue("test").toInt(), 1);
}

TEST_F(DatabaseTest, PreparedStatement) {
    db = DatabaseFactory::create(DatabaseType::MySQL);
    ASSERT_TRUE(db->connect(config));
    
    auto stmt = db->prepare("SELECT ? + ? AS sum");
    stmt->setInt(1, 5);
    stmt->setInt(2, 3);
    
    auto result = stmt->executeQuery();
    result.first();
    EXPECT_EQ(result.getValue("sum").toInt(), 8);
}

TEST_F(DatabaseTest, Transaction) {
    db = DatabaseFactory::create(DatabaseType::MySQL);
    ASSERT_TRUE(db->connect(config));
    
    auto txn = db->beginTransaction();
    ASSERT_TRUE(txn->isActive());
    
    db->executeUpdate("CREATE TEMPORARY TABLE test_txn (id INT)");
    db->executeUpdate("INSERT INTO test_txn VALUES (1)");
    
    txn->rollback();
    
    auto result = db->executeQuery("SELECT COUNT(*) AS cnt FROM test_txn");
    result.first();
    EXPECT_EQ(result.getValue("cnt").toInt(), 0);
}

// Performance benchmarks
TEST_F(DatabaseTest, BenchmarkQueries) {
    db = DatabaseFactory::create(DatabaseType::MySQL);
    ASSERT_TRUE(db->connect(config));
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; ++i) {
        auto result = db->executeQuery("SELECT 1");
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "1000 queries executed in " << duration.count() << "ms" << std::endl;
    EXPECT_LT(duration.count(), 5000);  // Should complete within 5 seconds
}
```

## Performance Considerations

1. **Connection Pooling**: Always use connection pools for production applications
2. **Prepared Statements**: Use for repeated queries and to prevent SQL injection
3. **Batch Operations**: Group multiple operations when possible
4. **Async Operations**: Consider async interfaces for I/O-bound operations
5. **Result Streaming**: Use streaming for large result sets
6. **Query Optimization**: Use EXPLAIN/query plans to optimize queries
7. **Caching**: Implement query result caching where appropriate
8. **Connection Reuse**: Minimize connection creation/destruction overhead

## Security Guidelines

1. **Always use prepared statements** for user input
2. **Implement connection encryption** (SSL/TLS)
3. **Use least privilege principle** for database users
4. **Sanitize error messages** before displaying to users
5. **Implement query timeouts** to prevent DoS
6. **Log all database access** for audit trails
7. **Regularly update database drivers**
8. **Use connection string encryption** in configuration files

## Thread Safety

The interface is designed to be thread-safe with the following considerations:

1. **Connection objects are NOT thread-safe**: Each thread should use its own connection
2. **Connection pools ARE thread-safe**: Multiple threads can acquire/release connections
3. **Prepared statements are NOT thread-safe**: Create separate instances per thread
4. **The factory IS thread-safe**: Can be called from multiple threads

## Migration Guide

### From Raw Database APIs

```cpp
// Before (MySQL C API)
MYSQL* mysql = mysql_init(NULL);
mysql_real_connect(mysql, "localhost", "user", "pass", "db", 0, NULL, 0);
mysql_query(mysql, "SELECT * FROM users");
MYSQL_RES* result = mysql_store_result(mysql);
// Process results...
mysql_free_result(result);
mysql_close(mysql);

// After (Unified Interface)
auto db = DatabaseFactory::create(DatabaseType::MySQL);
db->connect(config);
auto result = db->executeQuery("SELECT * FROM users");
// Process results...
db->disconnect();
```

## Future Enhancements

1. **Async/Await Support**: C++20 coroutines for async operations
2. **ORM Layer**: Object-relational mapping on top of the interface
3. **Migration System**: Database schema versioning and migrations
4. **Query Cache**: Built-in query result caching
5. **Monitoring**: Performance metrics and monitoring hooks
6. **NoSQL Support**: Extend to support NoSQL databases
7. **GraphQL Integration**: GraphQL query translation
8. **Distributed Transactions**: XA transaction support
