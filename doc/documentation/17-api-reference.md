# ScratchBird API Reference 🟡

The ScratchBird API provides comprehensive programming interfaces for database connectivity, management, and administration. This reference covers the complete API surface including the enhanced SBDatabase framework.

## 🔗 API Overview

### **Programming Interfaces**
- **SBDatabase Framework**: High-level C++ database connectivity
- **Native C API**: Low-level Firebird-compatible API
- **REST API**: HTTP-based database operations (ScratchBird enhancement)
- **Administrative API**: Database management operations

### **Language Support**
- **C++**: Native SBDatabase classes with modern C++17 features
- **C**: Standard Firebird-compatible C API
- **Python**: Python bindings via ctypes and native modules
- **Node.js**: JavaScript bindings for server-side applications
- **REST**: HTTP/JSON for any language

---

## 🚀 SBDatabase Framework

### **Core Classes**

#### **SBDatabase Class**
Primary database connection and management class.

```cpp
#include "sb_database.h"

class SBDatabase {
public:
    // Constructors
    SBDatabase();
    ~SBDatabase();
    
    // Connection management
    bool connect(const std::string& db_name, 
                 const std::string& user = "", 
                 const std::string& pass = "", 
                 const std::string& role_name = "", 
                 bool trusted = false);
    bool disconnect();
    bool isConnected() const;
    
    // Transaction management
    bool startTransaction();
    bool commitTransaction();
    bool rollbackTransaction();
    bool isInTransaction() const;
    
    // Query execution
    bool executeQuery(const std::string& sql);
    bool executeUpdate(const std::string& sql, int& affected_rows);
    bool executeSelect(const std::string& sql, 
                       std::vector<std::vector<std::string>>& results,
                       std::vector<std::string>& column_names);
    
    // Database information
    std::string getDatabaseName() const;
    std::string getUsername() const;
    std::string getRole() const;
    
    // Utility functions
    bool tableExists(const std::string& table_name, 
                     const std::string& schema_name = "");
    bool schemaExists(const std::string& schema_name);
    std::vector<std::string> getTableNames(const std::string& schema_name = "");
    std::vector<std::string> getSchemaNames();
    
    // Error handling
    std::string getLastError() const;
    bool hasError() const;
    void clearError();
    
    // Statistics and monitoring
    struct DatabaseStats {
        int page_size;
        int page_count;
        int allocated_pages;
        int free_pages;
        std::string database_version;
        std::string creation_date;
        bool read_only;
        bool force_writes;
    };
    bool getDatabaseStats(DatabaseStats& stats);
    
    // Backup/Restore support
    bool backupDatabase(const std::string& backup_file, bool verbose = false);
    bool restoreDatabase(const std::string& backup_file, bool verbose = false);
};
```

#### **SBStatement Class**
Prepared statement management for complex queries.

```cpp
class SBStatement {
public:
    SBStatement(SBDatabase* db);
    ~SBStatement();
    
    // Statement lifecycle
    bool prepare(const std::string& sql);
    bool execute();
    bool fetch();
    bool close();
    
    // Parameter binding
    bool bindParameter(int index, const std::string& value);
    bool bindParameter(int index, int value);
    bool bindParameter(int index, double value);
    bool bindParameter(int index, const std::vector<uint8_t>& blob_data);
    
    // Result retrieval
    std::string getString(int column);
    int getInt(int column);
    double getDouble(int column);
    bool isNull(int column);
    std::vector<uint8_t> getBlob(int column);
    
    // Metadata
    int getColumnCount() const;
    std::string getColumnName(int column) const;
    std::string getColumnType(int column) const;
    int getColumnSize(int column) const;
};
```

#### **SBException Class**
Enhanced error handling with detailed information.

```cpp
class SBException : public std::exception {
public:
    SBException(const std::string& msg, ISC_STATUS code = 0);
    
    const char* what() const noexcept override;
    ISC_STATUS getErrorCode() const;
    std::string getDetailedMessage() const;
    std::string getSQLState() const;
};
```

---

## 💻 Quick Start Examples

### **Basic Database Connection**
```cpp
#include "sb_database.h"
#include <iostream>

int main() {
    try {
        // Create database connection
        SBDatabase db;
        
        // Connect to database
        if (!db.connect("mydatabase.fdb", "SYSDBA", "masterkey")) {
            std::cerr << "Connection failed: " << db.getLastError() << std::endl;
            return 1;
        }
        
        std::cout << "Connected to: " << db.getDatabaseName() << std::endl;
        
        // Execute a simple query
        std::vector<std::vector<std::string>> results;
        std::vector<std::string> columns;
        
        if (db.executeSelect("SELECT * FROM customers", results, columns)) {
            // Display results
            for (const auto& col : columns) {
                std::cout << col << "\t";
            }
            std::cout << std::endl;
            
            for (const auto& row : results) {
                for (const auto& field : row) {
                    std::cout << field << "\t";
                }
                std::cout << std::endl;
            }
        }
        
        // Cleanup is automatic
        
    } catch (const SBException& e) {
        std::cerr << "Database error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

### **Transaction Management**
```cpp
#include "sb_database.h"

bool transferFunds(SBDatabase& db, int from_account, int to_account, double amount) {
    try {
        // Start transaction
        if (!db.startTransaction()) {
            return false;
        }
        
        // Check sufficient funds
        std::vector<std::vector<std::string>> results;
        std::vector<std::string> columns;
        
        std::string check_sql = "SELECT balance FROM accounts WHERE account_id = " + 
                               std::to_string(from_account);
        
        if (!db.executeSelect(check_sql, results, columns) || results.empty()) {
            db.rollbackTransaction();
            return false;
        }
        
        double current_balance = std::stod(results[0][0]);
        if (current_balance < amount) {
            db.rollbackTransaction();
            return false;  // Insufficient funds
        }
        
        // Debit from account
        int affected;
        std::string debit_sql = "UPDATE accounts SET balance = balance - " + 
                               std::to_string(amount) + 
                               " WHERE account_id = " + std::to_string(from_account);
        
        if (!db.executeUpdate(debit_sql, affected) || affected != 1) {
            db.rollbackTransaction();
            return false;
        }
        
        // Credit to account
        std::string credit_sql = "UPDATE accounts SET balance = balance + " + 
                                std::to_string(amount) + 
                                " WHERE account_id = " + std::to_string(to_account);
        
        if (!db.executeUpdate(credit_sql, affected) || affected != 1) {
            db.rollbackTransaction();
            return false;
        }
        
        // Commit transaction
        return db.commitTransaction();
        
    } catch (const SBException& e) {
        db.rollbackTransaction();
        std::cerr << "Transfer failed: " << e.what() << std::endl;
        return false;
    }
}
```

### **Prepared Statements**
```cpp
#include "sb_database.h"

bool insertCustomer(SBDatabase& db, const std::string& name, 
                   const std::string& email, const std::string& phone) {
    try {
        // Create prepared statement
        SBStatement stmt(&db);
        
        // Prepare INSERT statement
        if (!stmt.prepare("INSERT INTO customers (name, email, phone) VALUES (?, ?, ?)")) {
            return false;
        }
        
        // Bind parameters
        stmt.bindParameter(1, name);
        stmt.bindParameter(2, email);
        stmt.bindParameter(3, phone);
        
        // Execute statement
        return stmt.execute();
        
    } catch (const SBException& e) {
        std::cerr << "Insert failed: " << e.what() << std::endl;
        return false;
    }
}

// Usage
int main() {
    SBDatabase db;
    db.connect("mydatabase.fdb", "SYSDBA", "masterkey");
    
    // Insert multiple customers efficiently
    db.startTransaction();
    
    insertCustomer(db, "John Smith", "john@example.com", "555-1234");
    insertCustomer(db, "Jane Doe", "jane@example.com", "555-5678");
    insertCustomer(db, "Bob Wilson", "bob@example.com", "555-9012");
    
    db.commitTransaction();
    
    return 0;
}
```

---

## 🏗️ Advanced Features

### **Schema-Aware Operations**
```cpp
#include "sb_database.h"

class SchemaManager {
private:
    SBDatabase& db;
    
public:
    SchemaManager(SBDatabase& database) : db(database) {}
    
    // Create hierarchical schema
    bool createSchema(const std::string& schema_path) {
        try {
            std::string sql = "CREATE SCHEMA " + schema_path;
            return db.executeQuery(sql);
        } catch (const SBException& e) {
            std::cerr << "Schema creation failed: " << e.what() << std::endl;
            return false;
        }
    }
    
    // List all schemas
    std::vector<std::string> getSchemas() {
        return db.getSchemaNames();
    }
    
    // Get tables in specific schema
    std::vector<std::string> getTablesInSchema(const std::string& schema) {
        return db.getTableNames(schema);
    }
    
    // Check if schema exists
    bool schemaExists(const std::string& schema) {
        return db.schemaExists(schema);
    }
    
    // Create table in specific schema
    bool createTableInSchema(const std::string& schema, 
                           const std::string& table_name,
                           const std::string& table_definition) {
        try {
            std::string sql = "CREATE TABLE " + schema + "." + table_name + 
                             " " + table_definition;
            return db.executeQuery(sql);
        } catch (const SBException& e) {
            std::cerr << "Table creation failed: " << e.what() << std::endl;
            return false;
        }
    }
};

// Usage example
int main() {
    SBDatabase db;
    db.connect("mydatabase.fdb", "SYSDBA", "masterkey");
    
    SchemaManager manager(db);
    
    // Create hierarchical schema structure
    manager.createSchema("company");
    manager.createSchema("company.finance");
    manager.createSchema("company.finance.accounting");
    
    // Create table in specific schema
    manager.createTableInSchema("company.finance.accounting", 
                               "transactions",
                               "(id INTEGER PRIMARY KEY, amount DECIMAL(10,2), description VARCHAR(200))");
    
    // List schemas
    auto schemas = manager.getSchemas();
    for (const auto& schema : schemas) {
        std::cout << "Schema: " << schema << std::endl;
    }
    
    return 0;
}
```

### **Connection Pooling**
```cpp
#include "sb_database.h"
#include <memory>
#include <vector>
#include <mutex>
#include <condition_variable>

class SBConnectionPool {
private:
    struct PoolConnection {
        std::unique_ptr<SBDatabase> db;
        bool in_use;
        std::chrono::steady_clock::time_point last_used;
    };
    
    std::vector<PoolConnection> connections;
    std::mutex pool_mutex;
    std::condition_variable pool_cv;
    
    std::string db_name;
    std::string username;
    std::string password;
    std::string role;
    size_t max_connections;
    
public:
    SBConnectionPool(const std::string& database, const std::string& user,
                     const std::string& pass, size_t max_conn = 10)
        : db_name(database), username(user), password(pass), max_connections(max_conn) {
        
        // Initialize pool with minimum connections
        for (size_t i = 0; i < std::min(max_conn, size_t(3)); ++i) {
            createConnection();
        }
    }
    
    ~SBConnectionPool() {
        std::lock_guard<std::mutex> lock(pool_mutex);
        connections.clear();
    }
    
    // Get connection from pool
    std::shared_ptr<SBDatabase> getConnection() {
        std::unique_lock<std::mutex> lock(pool_mutex);
        
        // Wait for available connection
        pool_cv.wait(lock, [this] {
            return std::any_of(connections.begin(), connections.end(),
                              [](const PoolConnection& conn) { return !conn.in_use; });
        });
        
        // Find available connection
        for (auto& conn : connections) {
            if (!conn.in_use) {
                conn.in_use = true;
                conn.last_used = std::chrono::steady_clock::now();
                
                // Return shared_ptr with custom deleter to return to pool
                return std::shared_ptr<SBDatabase>(conn.db.get(), 
                    [this](SBDatabase* db) {
                        returnConnection(db);
                    });
            }
        }
        
        return nullptr;  // Should not reach here
    }
    
private:
    void createConnection() {
        auto conn = std::make_unique<SBDatabase>();
        if (conn->connect(db_name, username, password, role)) {
            connections.push_back({std::move(conn), false, std::chrono::steady_clock::now()});
        }
    }
    
    void returnConnection(SBDatabase* db) {
        std::lock_guard<std::mutex> lock(pool_mutex);
        
        for (auto& conn : connections) {
            if (conn.db.get() == db) {
                conn.in_use = false;
                pool_cv.notify_one();
                break;
            }
        }
    }
};

// Usage
int main() {
    SBConnectionPool pool("mydatabase.fdb", "SYSDBA", "masterkey", 5);
    
    // Get connection from pool
    auto db = pool.getConnection();
    
    // Use connection
    std::vector<std::vector<std::string>> results;
    std::vector<std::string> columns;
    db->executeSelect("SELECT COUNT(*) FROM customers", results, columns);
    
    // Connection automatically returned to pool when db goes out of scope
    
    return 0;
}
```

### **Async Operations**
```cpp
#include "sb_database.h"
#include <future>
#include <thread>

class AsyncSBDatabase {
private:
    SBDatabase db;
    
public:
    AsyncSBDatabase() = default;
    
    // Async connection
    std::future<bool> connectAsync(const std::string& db_name, 
                                  const std::string& user,
                                  const std::string& password) {
        return std::async(std::launch::async, [this, db_name, user, password]() {
            return db.connect(db_name, user, password);
        });
    }
    
    // Async query execution
    std::future<bool> executeSelectAsync(const std::string& sql,
                                        std::vector<std::vector<std::string>>& results,
                                        std::vector<std::string>& columns) {
        return std::async(std::launch::async, [this, sql, &results, &columns]() {
            return db.executeSelect(sql, results, columns);
        });
    }
    
    // Async backup
    std::future<bool> backupAsync(const std::string& backup_file) {
        return std::async(std::launch::async, [this, backup_file]() {
            return db.backupDatabase(backup_file, true);
        });
    }
    
    // Get underlying database for synchronous operations
    SBDatabase& getDatabase() { return db; }
};

// Usage
int main() {
    AsyncSBDatabase async_db;
    
    // Start async connection
    auto connect_future = async_db.connectAsync("mydatabase.fdb", "SYSDBA", "masterkey");
    
    // Do other work while connecting...
    std::cout << "Connecting to database..." << std::endl;
    
    // Wait for connection
    if (connect_future.get()) {
        std::cout << "Connected successfully!" << std::endl;
        
        // Start async query
        std::vector<std::vector<std::string>> results;
        std::vector<std::string> columns;
        
        auto query_future = async_db.executeSelectAsync("SELECT * FROM large_table", 
                                                        results, columns);
        
        // Do other work while query executes...
        std::cout << "Query executing..." << std::endl;
        
        // Wait for results
        if (query_future.get()) {
            std::cout << "Query completed. Rows: " << results.size() << std::endl;
        }
    }
    
    return 0;
}
```

---

## 🌐 REST API

### **HTTP Endpoints**

#### **Database Operations**
```http
# Connect to database
POST /api/v1/connect
Content-Type: application/json

{
    "database": "mydatabase.fdb",
    "username": "SYSDBA",
    "password": "masterkey",
    "role": "optional_role"
}

# Response
{
    "success": true,
    "session_id": "uuid-session-id",
    "database_info": {
        "name": "mydatabase.fdb",
        "version": "SB-T0.5.0.1",
        "page_size": 8192
    }
}
```

#### **Query Execution**
```http
# Execute SELECT query
POST /api/v1/query
Content-Type: application/json
Authorization: Bearer session-token

{
    "sql": "SELECT * FROM customers WHERE city = ?",
    "parameters": ["New York"],
    "format": "json"
}

# Response
{
    "success": true,
    "columns": ["id", "name", "email", "city"],
    "rows": [
        [1, "John Smith", "john@example.com", "New York"],
        [2, "Jane Doe", "jane@example.com", "New York"]
    ],
    "row_count": 2,
    "execution_time_ms": 15
}
```

#### **Schema Operations**
```http
# Create schema
POST /api/v1/schema
Content-Type: application/json
Authorization: Bearer session-token

{
    "action": "create",
    "schema_name": "company.finance.accounting"
}

# List schemas
GET /api/v1/schemas
Authorization: Bearer session-token

# Response
{
    "success": true,
    "schemas": [
        "company",
        "company.finance", 
        "company.finance.accounting"
    ]
}
```

### **JavaScript/Node.js Example**
```javascript
const axios = require('axios');

class ScratchBirdAPI {
    constructor(baseURL) {
        this.baseURL = baseURL;
        this.sessionToken = null;
    }
    
    async connect(database, username, password, role) {
        try {
            const response = await axios.post(`${this.baseURL}/api/v1/connect`, {
                database,
                username,
                password,
                role
            });
            
            if (response.data.success) {
                this.sessionToken = response.data.session_id;
                return response.data.database_info;
            }
            
            throw new Error('Connection failed');
        } catch (error) {
            throw new Error(`Connection error: ${error.message}`);
        }
    }
    
    async query(sql, parameters = []) {
        try {
            const response = await axios.post(`${this.baseURL}/api/v1/query`, {
                sql,
                parameters,
                format: 'json'
            }, {
                headers: {
                    'Authorization': `Bearer ${this.sessionToken}`
                }
            });
            
            return response.data;
        } catch (error) {
            throw new Error(`Query error: ${error.message}`);
        }
    }
    
    async createSchema(schemaName) {
        try {
            const response = await axios.post(`${this.baseURL}/api/v1/schema`, {
                action: 'create',
                schema_name: schemaName
            }, {
                headers: {
                    'Authorization': `Bearer ${this.sessionToken}`
                }
            });
            
            return response.data.success;
        } catch (error) {
            throw new Error(`Schema creation error: ${error.message}`);
        }
    }
}

// Usage
async function example() {
    const api = new ScratchBirdAPI('http://localhost:8080');
    
    try {
        // Connect to database
        const dbInfo = await api.connect('mydatabase.fdb', 'SYSDBA', 'masterkey');
        console.log('Connected to:', dbInfo.name);
        
        // Create hierarchical schema
        await api.createSchema('ecommerce.customers');
        
        // Execute query
        const result = await api.query('SELECT COUNT(*) FROM customers');
        console.log('Customer count:', result.rows[0][0]);
        
    } catch (error) {
        console.error('Error:', error.message);
    }
}

example();
```

---

## 🔧 Administrative API

### **Database Management**
```cpp
#include "sb_database.h"

class SBDatabaseAdmin {
private:
    SBDatabase db;
    
public:
    // Database creation
    bool createDatabase(const std::string& db_path, 
                       const std::string& username,
                       const std::string& password,
                       int page_size = 8192) {
        try {
            std::string sql = "CREATE DATABASE '" + db_path + "' " +
                             "USER '" + username + "' " +
                             "PASSWORD '" + password + "' " +
                             "PAGE_SIZE " + std::to_string(page_size) +
                             " DEFAULT CHARACTER SET UTF8";
            
            return db.executeQuery(sql);
        } catch (const SBException& e) {
            std::cerr << "Database creation failed: " << e.what() << std::endl;
            return false;
        }
    }
    
    // User management
    bool createUser(const std::string& username, 
                   const std::string& password,
                   const std::string& first_name = "",
                   const std::string& last_name = "") {
        try {
            std::string sql = "CREATE USER " + username + 
                             " PASSWORD '" + password + "'";
            
            if (!first_name.empty()) {
                sql += " FIRSTNAME '" + first_name + "'";
            }
            
            if (!last_name.empty()) {
                sql += " LASTNAME '" + last_name + "'";
            }
            
            return db.executeQuery(sql);
        } catch (const SBException& e) {
            std::cerr << "User creation failed: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Role management
    bool createRole(const std::string& role_name) {
        try {
            std::string sql = "CREATE ROLE " + role_name;
            return db.executeQuery(sql);
        } catch (const SBException& e) {
            std::cerr << "Role creation failed: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Grant permissions
    bool grantPrivileges(const std::string& privileges,
                        const std::string& object,
                        const std::string& user_or_role) {
        try {
            std::string sql = "GRANT " + privileges + " ON " + object + 
                             " TO " + user_or_role;
            return db.executeQuery(sql);
        } catch (const SBException& e) {
            std::cerr << "Grant failed: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Database statistics
    struct AdminStats {
        int total_tables;
        int total_indexes;
        int total_users;
        int total_roles;
        std::vector<std::string> active_schemas;
        double database_size_mb;
    };
    
    AdminStats getDatabaseStatistics() {
        AdminStats stats = {};
        
        try {
            // Count tables
            std::vector<std::vector<std::string>> results;
            std::vector<std::string> columns;
            
            if (db.executeSelect("SELECT COUNT(*) FROM RDB$RELATIONS WHERE RDB$SYSTEM_FLAG = 0", 
                                results, columns)) {
                stats.total_tables = std::stoi(results[0][0]);
            }
            
            // Count indexes
            if (db.executeSelect("SELECT COUNT(*) FROM RDB$INDICES WHERE RDB$SYSTEM_FLAG = 0", 
                                results, columns)) {
                stats.total_indexes = std::stoi(results[0][0]);
            }
            
            // Get schemas
            stats.active_schemas = db.getSchemaNames();
            
        } catch (const SBException& e) {
            std::cerr << "Statistics gathering failed: " << e.what() << std::endl;
        }
        
        return stats;
    }
};
```

---

## 📊 Monitoring and Diagnostics

### **Performance Monitoring**
```cpp
#include "sb_database.h"
#include <chrono>

class SBPerformanceMonitor {
private:
    SBDatabase& db;
    
public:
    SBPerformanceMonitor(SBDatabase& database) : db(database) {}
    
    struct QueryPerformance {
        std::string sql;
        std::chrono::milliseconds execution_time;
        int rows_affected;
        bool success;
    };
    
    // Monitor query execution
    QueryPerformance executeWithMonitoring(const std::string& sql) {
        QueryPerformance perf;
        perf.sql = sql;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            int affected_rows = 0;
            perf.success = db.executeUpdate(sql, affected_rows);
            perf.rows_affected = affected_rows;
        } catch (const SBException& e) {
            perf.success = false;
            std::cerr << "Query failed: " << e.what() << std::endl;
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        perf.execution_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        return perf;
    }
    
    // Get connection statistics
    struct ConnectionStats {
        int active_connections;
        int total_connections;
        std::vector<std::string> active_users;
    };
    
    ConnectionStats getConnectionStats() {
        ConnectionStats stats = {};
        
        try {
            std::vector<std::vector<std::string>> results;
            std::vector<std::string> columns;
            
            // Get active connections
            if (db.executeSelect("SELECT COUNT(*) FROM MON$ATTACHMENTS", results, columns)) {
                stats.active_connections = std::stoi(results[0][0]);
            }
            
            // Get active users
            if (db.executeSelect("SELECT DISTINCT MON$USER FROM MON$ATTACHMENTS", results, columns)) {
                for (const auto& row : results) {
                    stats.active_users.push_back(row[0]);
                }
            }
            
        } catch (const SBException& e) {
            std::cerr << "Connection stats failed: " << e.what() << std::endl;
        }
        
        return stats;
    }
};
```

---

## 🆘 Error Handling

### **Exception Hierarchy**
```cpp
// Base exception class
class SBException : public std::exception {
public:
    SBException(const std::string& msg, ISC_STATUS code = 0);
    const char* what() const noexcept override;
    ISC_STATUS getErrorCode() const;
};

// Connection-specific exceptions
class SBConnectionException : public SBException {
public:
    SBConnectionException(const std::string& msg, const std::string& database);
    std::string getDatabaseName() const;
};

// SQL execution exceptions
class SBSQLException : public SBException {
public:
    SBSQLException(const std::string& msg, const std::string& sql);
    std::string getSQL() const;
};

// Transaction exceptions
class SBTransactionException : public SBException {
public:
    SBTransactionException(const std::string& msg, int transaction_state);
    int getTransactionState() const;
};
```

### **Error Handling Patterns**
```cpp
// Comprehensive error handling
void robustDatabaseOperation() {
    try {
        SBDatabase db;
        
        if (!db.connect("mydatabase.fdb", "SYSDBA", "masterkey")) {
            throw SBConnectionException("Failed to connect", "mydatabase.fdb");
        }
        
        if (!db.startTransaction()) {
            throw SBTransactionException("Failed to start transaction", 0);
        }
        
        // Perform operations
        int affected_rows;
        if (!db.executeUpdate("UPDATE accounts SET balance = 1000", affected_rows)) {
            throw SBSQLException("Update failed", "UPDATE accounts SET balance = 1000");
        }
        
        if (!db.commitTransaction()) {
            throw SBTransactionException("Failed to commit", 1);
        }
        
    } catch (const SBConnectionException& e) {
        std::cerr << "Connection error for " << e.getDatabaseName() 
                  << ": " << e.what() << std::endl;
    } catch (const SBSQLException& e) {
        std::cerr << "SQL error in: " << e.getSQL() 
                  << " - " << e.what() << std::endl;
    } catch (const SBTransactionException& e) {
        std::cerr << "Transaction error (state " << e.getTransactionState() 
                  << "): " << e.what() << std::endl;
    } catch (const SBException& e) {
        std::cerr << "Database error: " << e.what() 
                  << " (code: " << e.getErrorCode() << ")" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
    }
}
```

---

## 🎯 Next Steps

- **[SBDatabase Class](18-sbdatabase-class.md)** - Detailed SBDatabase reference
- **[Error Handling](19-error-handling.md)** - Comprehensive error management
- **[Performance Tuning](20-performance.md)** - API performance optimization
- **[Integration Examples](README.md)** - Real-world integration patterns

## 📚 Related Documentation

- **[Installation Guide](03-installation.md)** - Setting up development environment
- **[Database Engine](05-database-engine.md)** - Understanding the underlying engine
- **[Best Practices](28-best-practices.md)** - Recommended API usage patterns