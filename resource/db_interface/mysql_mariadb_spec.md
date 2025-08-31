# MySQL/MariaDB C++ Interface Specification

## Overview

MySQL and MariaDB share compatible wire protocols and can use similar client libraries. This specification covers both database systems with their respective C++ connectors.

## Connection Libraries

### 1. MariaDB Connector/C++ (Recommended)

**Compatibility**: Works with both MariaDB and MySQL databases

#### Installation

##### Linux (Debian/Ubuntu)
```bash
sudo apt-get update
sudo apt-get install libmariadb-dev libmariadb-dev-compat libmariadb3
```

##### Linux (RHEL/CentOS)
```bash
sudo yum install mariadb-connector-c mariadb-connector-c-devel
```

##### Windows
Download from: https://mariadb.com/downloads/connectors/

##### macOS
```bash
brew install mariadb-connector-c
```

#### Header Files Required
```cpp
#include <mariadb/conncpp.hpp>
#include <mariadb/conncpp/Driver.hpp>
#include <mariadb/conncpp/Connection.hpp>
#include <mariadb/conncpp/Statement.hpp>
#include <mariadb/conncpp/PreparedStatement.hpp>
#include <mariadb/conncpp/ResultSet.hpp>
#include <mariadb/conncpp/Exception.hpp>
#include <mariadb/conncpp/DatabaseMetaData.hpp>
```

#### Connection Parameters

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| host | string | Database server hostname or IP | localhost |
| port | int | Server port number | 3306 |
| user | string | Username for authentication | - |
| password | string | Password for authentication | - |
| database | string | Default database/schema | - |
| unix_socket | string | Unix socket path (Linux/macOS) | - |
| ssl_key | string | Path to SSL key file | - |
| ssl_cert | string | Path to SSL certificate | - |
| ssl_ca | string | Path to CA certificate | - |
| ssl_cipher | string | SSL cipher suite | - |
| connect_timeout | int | Connection timeout in seconds | 10 |
| read_timeout | int | Read timeout in seconds | 0 |
| write_timeout | int | Write timeout in seconds | 0 |
| auto_reconnect | bool | Enable automatic reconnection | false |
| multi_statements | bool | Allow multiple statements | false |
| compression | bool | Enable protocol compression | false |
| charset | string | Character set | utf8mb4 |

### 2. MySQL Connector/C++ 8.0

#### Installation

##### Linux
```bash
wget https://dev.mysql.com/get/Downloads/Connector-C++/mysql-connector-c++-8.0.33-linux-glibc2.12-x86-64bit.tar.gz
tar -xzf mysql-connector-c++-8.0.33-linux-glibc2.12-x86-64bit.tar.gz
sudo cp -r mysql-connector-c++-8.0.33-linux-glibc2.12-x86-64bit/include/* /usr/local/include/
sudo cp -r mysql-connector-c++-8.0.33-linux-glibc2.12-x86-64bit/lib64/* /usr/local/lib/
```

##### Windows
Download installer from: https://dev.mysql.com/downloads/connector/cpp/

#### Header Files Required
```cpp
#include <mysqlx/xdevapi.h>  // For X DevAPI
#include <mysql/jdbc.h>       // For legacy API
```

## Complete Implementation Example

```cpp
#include <mariadb/conncpp.hpp>
#include <iostream>
#include <memory>
#include <vector>
#include <map>

class MySQLConnection {
private:
    std::unique_ptr<sql::Connection> conn;
    sql::Driver* driver;
    
public:
    MySQLConnection() : conn(nullptr), driver(nullptr) {}
    
    ~MySQLConnection() {
        disconnect();
    }
    
    // Connection establishment
    bool connect(const std::string& host, int port, const std::string& user,
                 const std::string& password, const std::string& database,
                 bool use_ssl = false) {
        try {
            // Get driver instance
            driver = sql::mariadb::get_driver_instance();
            
            // Build connection URL
            std::string url = "jdbc:mariadb://" + host + ":" + 
                             std::to_string(port) + "/" + database;
            
            // Set connection properties
            sql::Properties properties;
            properties["user"] = user;
            properties["password"] = password;
            
            if (use_ssl) {
                properties["useSsl"] = "true";
                properties["trustServerCertificate"] = "false";
                properties["requireSSL"] = "true";
            }
            
            // Additional properties
            properties["autoReconnect"] = "true";
            properties["connectTimeout"] = "10000";  // 10 seconds
            properties["socketTimeout"] = "30000";   // 30 seconds
            properties["useCompression"] = "true";
            properties["useUnicode"] = "true";
            properties["characterEncoding"] = "UTF-8";
            
            // Establish connection
            conn.reset(driver->connect(url, properties));
            
            if (conn->isValid()) {
                std::cout << "Connected to MySQL/MariaDB successfully" << std::endl;
                return true;
            }
            
            return false;
            
        } catch (sql::SQLException& e) {
            std::cerr << "Connection failed: " << e.what() << std::endl;
            std::cerr << "Error code: " << e.getErrorCode() << std::endl;
            std::cerr << "SQLState: " << e.getSQLState() << std::endl;
            return false;
        }
    }
    
    // Execute SELECT query
    std::vector<std::map<std::string, std::string>> executeQuery(const std::string& query) {
        std::vector<std::map<std::string, std::string>> results;
        
        try {
            std::unique_ptr<sql::Statement> stmt(conn->createStatement());
            std::unique_ptr<sql::ResultSet> res(stmt->executeQuery(query));
            
            // Get metadata
            sql::ResultSetMetaData* meta = res->getMetaData();
            unsigned int columnCount = meta->getColumnCount();
            
            // Process results
            while (res->next()) {
                std::map<std::string, std::string> row;
                
                for (unsigned int i = 1; i <= columnCount; ++i) {
                    std::string columnName = meta->getColumnName(i);
                    std::string value;
                    
                    // Handle NULL values
                    if (res->isNull(i)) {
                        value = "NULL";
                    } else {
                        // Handle different data types
                        switch (meta->getColumnType(i)) {
                            case sql::DataType::INTEGER:
                            case sql::DataType::SMALLINT:
                            case sql::DataType::TINYINT:
                            case sql::DataType::BIGINT:
                                value = std::to_string(res->getInt64(i));
                                break;
                            case sql::DataType::REAL:
                            case sql::DataType::DOUBLE:
                            case sql::DataType::DECIMAL:
                            case sql::DataType::NUMERIC:
                                value = std::to_string(res->getDouble(i));
                                break;
                            case sql::DataType::DATE:
                            case sql::DataType::TIME:
                            case sql::DataType::TIMESTAMP:
                            case sql::DataType::DATETIME:
                            case sql::DataType::YEAR:
                            case sql::DataType::CHAR:
                            case sql::DataType::VARCHAR:
                            case sql::DataType::LONGVARCHAR:
                            case sql::DataType::SET:
                            case sql::DataType::ENUM:
                            case sql::DataType::SQLNULL:
                            default:
                                value = res->getString(i);
                                break;
                        }
                    }
                    
                    row[columnName] = value;
                }
                
                results.push_back(row);
            }
            
        } catch (sql::SQLException& e) {
            std::cerr << "Query execution failed: " << e.what() << std::endl;
            std::cerr << "Error code: " << e.getErrorCode() << std::endl;
            std::cerr << "SQLState: " << e.getSQLState() << std::endl;
        }
        
        return results;
    }
    
    // Execute prepared statement with parameters
    std::vector<std::map<std::string, std::string>> executePreparedQuery(
        const std::string& query, 
        const std::vector<std::string>& params) {
        
        std::vector<std::map<std::string, std::string>> results;
        
        try {
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(query));
            
            // Bind parameters
            for (size_t i = 0; i < params.size(); ++i) {
                pstmt->setString(i + 1, params[i]);
            }
            
            std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
            
            // Get metadata
            sql::ResultSetMetaData* meta = res->getMetaData();
            unsigned int columnCount = meta->getColumnCount();
            
            // Process results
            while (res->next()) {
                std::map<std::string, std::string> row;
                
                for (unsigned int i = 1; i <= columnCount; ++i) {
                    std::string columnName = meta->getColumnName(i);
                    std::string value = res->isNull(i) ? "NULL" : res->getString(i);
                    row[columnName] = value;
                }
                
                results.push_back(row);
            }
            
        } catch (sql::SQLException& e) {
            std::cerr << "Prepared query execution failed: " << e.what() << std::endl;
        }
        
        return results;
    }
    
    // Execute INSERT, UPDATE, DELETE
    int executeUpdate(const std::string& query) {
        try {
            std::unique_ptr<sql::Statement> stmt(conn->createStatement());
            return stmt->executeUpdate(query);
            
        } catch (sql::SQLException& e) {
            std::cerr << "Update execution failed: " << e.what() << std::endl;
            return -1;
        }
    }
    
    // Execute prepared INSERT, UPDATE, DELETE with parameters
    int executePreparedUpdate(const std::string& query, 
                              const std::vector<std::string>& params) {
        try {
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(query));
            
            // Bind parameters
            for (size_t i = 0; i < params.size(); ++i) {
                pstmt->setString(i + 1, params[i]);
            }
            
            return pstmt->executeUpdate();
            
        } catch (sql::SQLException& e) {
            std::cerr << "Prepared update execution failed: " << e.what() << std::endl;
            return -1;
        }
    }
    
    // Transaction management
    bool beginTransaction() {
        try {
            conn->setAutoCommit(false);
            return true;
        } catch (sql::SQLException& e) {
            std::cerr << "Failed to begin transaction: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool commit() {
        try {
            conn->commit();
            conn->setAutoCommit(true);
            return true;
        } catch (sql::SQLException& e) {
            std::cerr << "Failed to commit transaction: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool rollback() {
        try {
            conn->rollback();
            conn->setAutoCommit(true);
            return true;
        } catch (sql::SQLException& e) {
            std::cerr << "Failed to rollback transaction: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Batch operations
    bool executeBatch(const std::vector<std::string>& queries) {
        try {
            beginTransaction();
            
            std::unique_ptr<sql::Statement> stmt(conn->createStatement());
            
            for (const auto& query : queries) {
                stmt->execute(query);
            }
            
            return commit();
            
        } catch (sql::SQLException& e) {
            rollback();
            std::cerr << "Batch execution failed: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Stored procedure call
    std::vector<std::map<std::string, std::string>> callProcedure(
        const std::string& procedureName, 
        const std::vector<std::string>& params) {
        
        std::string query = "CALL " + procedureName + "(";
        for (size_t i = 0; i < params.size(); ++i) {
            query += "?";
            if (i < params.size() - 1) query += ",";
        }
        query += ")";
        
        return executePreparedQuery(query, params);
    }
    
    // Database metadata
    std::vector<std::string> getTables() {
        std::vector<std::string> tables;
        
        try {
            sql::DatabaseMetaData* meta = conn->getMetaData();
            std::unique_ptr<sql::ResultSet> res(meta->getTables(nullptr, nullptr, "%", nullptr));
            
            while (res->next()) {
                tables.push_back(res->getString("TABLE_NAME"));
            }
            
        } catch (sql::SQLException& e) {
            std::cerr << "Failed to get tables: " << e.what() << std::endl;
        }
        
        return tables;
    }
    
    // Get table columns
    std::vector<std::map<std::string, std::string>> getColumns(const std::string& tableName) {
        std::vector<std::map<std::string, std::string>> columns;
        
        try {
            sql::DatabaseMetaData* meta = conn->getMetaData();
            std::unique_ptr<sql::ResultSet> res(meta->getColumns(nullptr, nullptr, tableName, "%"));
            
            while (res->next()) {
                std::map<std::string, std::string> column;
                column["name"] = res->getString("COLUMN_NAME");
                column["type"] = res->getString("TYPE_NAME");
                column["size"] = std::to_string(res->getInt("COLUMN_SIZE"));
                column["nullable"] = res->getString("IS_NULLABLE");
                column["default"] = res->getString("COLUMN_DEF");
                columns.push_back(column);
            }
            
        } catch (sql::SQLException& e) {
            std::cerr << "Failed to get columns: " << e.what() << std::endl;
        }
        
        return columns;
    }
    
    // Connection status
    bool isConnected() {
        return conn && conn->isValid();
    }
    
    // Disconnect
    void disconnect() {
        if (conn) {
            conn->close();
            conn.reset();
        }
    }
    
    // Get last insert ID
    uint64_t getLastInsertId() {
        try {
            std::unique_ptr<sql::Statement> stmt(conn->createStatement());
            std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT LAST_INSERT_ID()"));
            
            if (res->next()) {
                return res->getUInt64(1);
            }
            
        } catch (sql::SQLException& e) {
            std::cerr << "Failed to get last insert ID: " << e.what() << std::endl;
        }
        
        return 0;
    }
    
    // Escape string for SQL
    std::string escapeString(const std::string& str) {
        return conn->escapeString(str);
    }
};

// Usage example
int main() {
    MySQLConnection db;
    
    // Connect to database
    if (db.connect("localhost", 3306, "root", "password", "testdb", false)) {
        
        // Create table
        db.executeUpdate(
            "CREATE TABLE IF NOT EXISTS users ("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
            "name VARCHAR(100) NOT NULL,"
            "email VARCHAR(100) UNIQUE,"
            "age INT,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ")"
        );
        
        // Insert data using prepared statement
        db.executePreparedUpdate(
            "INSERT INTO users (name, email, age) VALUES (?, ?, ?)",
            {"John Doe", "john@example.com", "30"}
        );
        
        // Get last insert ID
        std::cout << "Last insert ID: " << db.getLastInsertId() << std::endl;
        
        // Query data
        auto results = db.executeQuery("SELECT * FROM users");
        
        for (const auto& row : results) {
            for (const auto& [key, value] : row) {
                std::cout << key << ": " << value << " ";
            }
            std::cout << std::endl;
        }
        
        // Transaction example
        db.beginTransaction();
        db.executeUpdate("UPDATE users SET age = age + 1 WHERE id = 1");
        db.executeUpdate("INSERT INTO users (name, email, age) VALUES ('Jane Doe', 'jane@example.com', 25)");
        db.commit();
        
        // Get table metadata
        auto tables = db.getTables();
        for (const auto& table : tables) {
            std::cout << "Table: " << table << std::endl;
            
            auto columns = db.getColumns(table);
            for (const auto& col : columns) {
                std::cout << "  Column: " << col.at("name") 
                         << " Type: " << col.at("type") 
                         << " Size: " << col.at("size") << std::endl;
            }
        }
        
        // Disconnect
        db.disconnect();
    }
    
    return 0;
}
```

## Compilation

### Using g++
```bash
g++ -std=c++17 -o mysql_app mysql_app.cpp \
    -lmariadbcpp \
    -L/usr/lib/x86_64-linux-gnu \
    -I/usr/include/mariadb \
    -pthread
```

### Using CMake
```cmake
cmake_minimum_required(VERSION 3.10)
project(MySQLApp)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MARIADB REQUIRED libmariadb)

add_executable(mysql_app main.cpp)

target_include_directories(mysql_app PRIVATE ${MARIADB_INCLUDE_DIRS})
target_link_libraries(mysql_app ${MARIADB_LIBRARIES} mariadbcpp)
target_compile_options(mysql_app PRIVATE ${MARIADB_CFLAGS_OTHER})
```

## Error Codes

| Code | SQLState | Description |
|------|----------|-------------|
| 1045 | 28000 | Access denied for user |
| 1049 | 42000 | Unknown database |
| 1054 | 42S22 | Unknown column |
| 1062 | 23000 | Duplicate entry |
| 1064 | 42000 | SQL syntax error |
| 1146 | 42S02 | Table doesn't exist |
| 1216 | 23000 | Foreign key constraint fails |
| 1451 | 23000 | Cannot delete parent row |
| 2002 | HY000 | Can't connect to server |
| 2003 | HY000 | Can't connect to server on host |
| 2006 | HY000 | Server has gone away |
| 2013 | HY000 | Lost connection during query |

## Performance Optimization

### Connection Pooling
```cpp
class ConnectionPool {
private:
    std::vector<std::unique_ptr<MySQLConnection>> pool;
    std::queue<MySQLConnection*> available;
    std::mutex pool_mutex;
    std::condition_variable pool_cv;
    size_t max_connections;
    
public:
    ConnectionPool(size_t size, const std::string& host, int port, 
                   const std::string& user, const std::string& password, 
                   const std::string& database) : max_connections(size) {
        
        for (size_t i = 0; i < size; ++i) {
            auto conn = std::make_unique<MySQLConnection>();
            if (conn->connect(host, port, user, password, database)) {
                available.push(conn.get());
                pool.push_back(std::move(conn));
            }
        }
    }
    
    MySQLConnection* getConnection() {
        std::unique_lock<std::mutex> lock(pool_mutex);
        
        pool_cv.wait(lock, [this] { return !available.empty(); });
        
        MySQLConnection* conn = available.front();
        available.pop();
        
        return conn;
    }
    
    void releaseConnection(MySQLConnection* conn) {
        std::lock_guard<std::mutex> lock(pool_mutex);
        available.push(conn);
        pool_cv.notify_one();
    }
};
```

### Query Optimization Tips

1. **Use Prepared Statements**: Reduces parsing overhead and prevents SQL injection
2. **Batch Operations**: Group multiple operations in a single transaction
3. **Connection Reuse**: Maintain persistent connections instead of creating new ones
4. **Result Set Streaming**: Use cursor-based fetching for large result sets
5. **Index Usage**: Ensure queries use appropriate indexes
6. **Query Caching**: Enable query cache when appropriate

## Security Best Practices

1. **Always use SSL/TLS** for production connections
2. **Use prepared statements** to prevent SQL injection
3. **Implement connection timeouts** to prevent resource exhaustion
4. **Store credentials securely** using environment variables or secure vaults
5. **Use least privilege principle** for database users
6. **Enable audit logging** for sensitive operations
7. **Regularly update** connector libraries and database servers