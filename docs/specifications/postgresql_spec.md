# PostgreSQL C++ Interface Specification

## Overview

PostgreSQL provides two primary C/C++ interfaces: libpq (C library) and libpqxx (C++ wrapper). This specification covers both approaches with complete implementation details.

## Connection Libraries

### 1. libpq - PostgreSQL C Library

The official PostgreSQL C API that can be used directly in C++ applications.

#### Installation

##### Linux (Debian/Ubuntu)
```bash
sudo apt-get update
sudo apt-get install libpq-dev postgresql-client
```

##### Linux (RHEL/CentOS)
```bash
sudo yum install postgresql-devel
```

##### Windows
Download PostgreSQL installer from: https://www.postgresql.org/download/windows/
The libpq library is included with the installation.

##### macOS
```bash
brew install libpq
# Add to PATH
echo 'export PATH="/usr/local/opt/libpq/bin:$PATH"' >> ~/.zshrc
```

#### Header Files Required
```cpp
extern "C" {
    #include <libpq-fe.h>
    #include <postgres_ext.h>
    #include <pg_config_ext.h>
}
#include <arpa/inet.h>  // For network byte order conversion
```

### 2. libpqxx - C++ Wrapper Library

A modern C++ client API for PostgreSQL.

#### Installation

##### Linux (Debian/Ubuntu)
```bash
sudo apt-get install libpqxx-dev
```

##### Linux (RHEL/CentOS)
```bash
sudo yum install libpqxx-devel
```

##### Build from Source
```bash
git clone https://github.com/jtv/libpqxx.git
cd libpqxx
./configure --disable-documentation
make
sudo make install
```

#### Header Files Required
```cpp
#include <pqxx/pqxx>
#include <pqxx/connection>
#include <pqxx/transaction>
#include <pqxx/nontransaction>
#include <pqxx/result>
#include <pqxx/prepared_statement>
#include <pqxx/pipeline>
#include <pqxx/notification>
#include <pqxx/stream_from>
#include <pqxx/stream_to>
```

## Connection Parameters

### Connection String Format
```
postgresql://[user[:password]@][host][:port][/dbname][?param1=value1&...]
```

### Parameters Table

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| host | string | Database server hostname or IP | localhost |
| hostaddr | string | IP address (bypasses DNS lookup) | - |
| port | int | Server port number | 5432 |
| dbname | string | Database name | same as user |
| user | string | PostgreSQL username | OS username |
| password | string | User password | - |
| connect_timeout | int | Connection timeout in seconds | 10 |
| client_encoding | string | Client character encoding | database encoding |
| options | string | Command-line options to send | - |
| application_name | string | Application name for pg_stat_activity | - |
| fallback_application_name | string | Fallback application name | - |
| keepalives | int | Use TCP keepalives (0=no, 1=yes) | 1 |
| keepalives_idle | int | Seconds before sending keepalive | - |
| keepalives_interval | int | Interval between keepalives | - |
| keepalives_count | int | Number of keepalives before timeout | - |
| sslmode | string | SSL connection mode | prefer |
| requiressl | int | Require SSL (deprecated, use sslmode) | 0 |
| sslcompression | int | Enable SSL compression | 0 |
| sslcert | string | Client SSL certificate file | ~/.postgresql/postgresql.crt |
| sslkey | string | Client SSL key file | ~/.postgresql/postgresql.key |
| sslrootcert | string | Root certificate file | ~/.postgresql/root.crt |
| sslcrl | string | Certificate revocation list | - |
| requirepeer | string | Required peer CN for Unix socket | - |
| krbsrvname | string | Kerberos service name | postgres |
| gsslib | string | GSS library to use | - |
| service | string | Service name in pg_service.conf | - |
| target_session_attrs | string | Session properties | any |

### SSL Modes

| Mode | Description | Eavesdropping Protection | MITM Protection |
|------|-------------|-------------------------|-----------------|
| disable | No SSL | No | No |
| allow | First try non-SSL, then SSL | Maybe | No |
| prefer | First try SSL, then non-SSL | Maybe | No |
| require | Require SSL | Yes | No |
| verify-ca | Require SSL and verify CA | Yes | Depends on CA |
| verify-full | Require SSL, verify CA and hostname | Yes | Yes |

## Complete libpq Implementation

```cpp
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstring>
#include <sstream>

extern "C" {
    #include <libpq-fe.h>
}

class PostgreSQLConnection {
private:
    PGconn* conn;
    
    // Helper function to build connection string
    std::string buildConnString(const std::map<std::string, std::string>& params) {
        std::stringstream ss;
        for (const auto& [key, value] : params) {
            ss << key << "=" << value << " ";
        }
        return ss.str();
    }
    
public:
    PostgreSQLConnection() : conn(nullptr) {}
    
    ~PostgreSQLConnection() {
        disconnect();
    }
    
    // Connection establishment with parameters
    bool connect(const std::string& host, int port, const std::string& dbname,
                 const std::string& user, const std::string& password,
                 bool use_ssl = false, int connect_timeout = 10) {
        
        std::map<std::string, std::string> params;
        params["host"] = host;
        params["port"] = std::to_string(port);
        params["dbname"] = dbname;
        params["user"] = user;
        params["password"] = password;
        params["connect_timeout"] = std::to_string(connect_timeout);
        
        if (use_ssl) {
            params["sslmode"] = "require";
        } else {
            params["sslmode"] = "disable";
        }
        
        std::string connStr = buildConnString(params);
        
        // Establish connection
        conn = PQconnectdb(connStr.c_str());
        
        if (PQstatus(conn) != CONNECTION_OK) {
            std::cerr << "Connection failed: " << PQerrorMessage(conn) << std::endl;
            PQfinish(conn);
            conn = nullptr;
            return false;
        }
        
        // Set client encoding to UTF8
        if (PQsetClientEncoding(conn, "UTF8") != 0) {
            std::cerr << "Failed to set client encoding" << std::endl;
        }
        
        std::cout << "Connected to PostgreSQL successfully" << std::endl;
        std::cout << "Server version: " << PQserverVersion(conn) << std::endl;
        std::cout << "Protocol version: " << PQprotocolVersion(conn) << std::endl;
        
        return true;
    }
    
    // Connection with connection URI
    bool connectURI(const std::string& uri) {
        conn = PQconnectdb(uri.c_str());
        
        if (PQstatus(conn) != CONNECTION_OK) {
            std::cerr << "Connection failed: " << PQerrorMessage(conn) << std::endl;
            PQfinish(conn);
            conn = nullptr;
            return false;
        }
        
        return true;
    }
    
    // Asynchronous connection
    bool connectAsync(const std::string& connStr) {
        conn = PQconnectStart(connStr.c_str());
        
        if (!conn) {
            return false;
        }
        
        PostgresPollingStatusType status;
        
        do {
            status = PQconnectPoll(conn);
            
            if (status == PGRES_POLLING_FAILED) {
                std::cerr << "Async connection failed: " << PQerrorMessage(conn) << std::endl;
                PQfinish(conn);
                conn = nullptr;
                return false;
            }
            
            // In real application, you would use select() or poll() here
            if (status == PGRES_POLLING_READING || status == PGRES_POLLING_WRITING) {
                // Wait for socket to be ready
                usleep(10000); // 10ms
            }
            
        } while (status != PGRES_POLLING_OK);
        
        return true;
    }
    
    // Execute query and return results
    std::vector<std::map<std::string, std::string>> executeQuery(const std::string& query) {
        std::vector<std::map<std::string, std::string>> results;
        
        if (!conn) {
            std::cerr << "Not connected to database" << std::endl;
            return results;
        }
        
        PGresult* res = PQexec(conn, query.c_str());
        
        ExecStatusType status = PQresultStatus(res);
        
        if (status != PGRES_TUPLES_OK && status != PGRES_SINGLE_TUPLE) {
            std::cerr << "Query failed: " << PQerrorMessage(conn) << std::endl;
            std::cerr << "Query status: " << PQresStatus(status) << std::endl;
            PQclear(res);
            return results;
        }
        
        int numRows = PQntuples(res);
        int numCols = PQnfields(res);
        
        for (int i = 0; i < numRows; ++i) {
            std::map<std::string, std::string> row;
            
            for (int j = 0; j < numCols; ++j) {
                std::string colName = PQfname(res, j);
                
                if (PQgetisnull(res, i, j)) {
                    row[colName] = "NULL";
                } else {
                    char* value = PQgetvalue(res, i, j);
                    row[colName] = std::string(value);
                }
            }
            
            results.push_back(row);
        }
        
        PQclear(res);
        return results;
    }
    
    // Execute prepared statement with parameters
    std::vector<std::map<std::string, std::string>> executePrepared(
        const std::string& stmtName,
        const std::string& query,
        const std::vector<std::string>& params) {
        
        std::vector<std::map<std::string, std::string>> results;
        
        if (!conn) {
            std::cerr << "Not connected to database" << std::endl;
            return results;
        }
        
        // Prepare statement
        PGresult* prepRes = PQprepare(conn, stmtName.c_str(), query.c_str(), 
                                      params.size(), nullptr);
        
        if (PQresultStatus(prepRes) != PGRES_COMMAND_OK) {
            std::cerr << "Prepare failed: " << PQerrorMessage(conn) << std::endl;
            PQclear(prepRes);
            return results;
        }
        PQclear(prepRes);
        
        // Convert parameters to C-style array
        std::vector<const char*> paramValues;
        for (const auto& param : params) {
            paramValues.push_back(param.c_str());
        }
        
        // Execute prepared statement
        PGresult* res = PQexecPrepared(conn, stmtName.c_str(), params.size(),
                                       paramValues.data(), nullptr, nullptr, 0);
        
        if (PQresultStatus(res) != PGRES_TUPLES_OK) {
            std::cerr << "Execute prepared failed: " << PQerrorMessage(conn) << std::endl;
            PQclear(res);
            return results;
        }
        
        // Process results
        int numRows = PQntuples(res);
        int numCols = PQnfields(res);
        
        for (int i = 0; i < numRows; ++i) {
            std::map<std::string, std::string> row;
            
            for (int j = 0; j < numCols; ++j) {
                std::string colName = PQfname(res, j);
                
                if (PQgetisnull(res, i, j)) {
                    row[colName] = "NULL";
                } else {
                    row[colName] = PQgetvalue(res, i, j);
                }
            }
            
            results.push_back(row);
        }
        
        PQclear(res);
        return results;
    }
    
    // Execute command (INSERT, UPDATE, DELETE)
    int executeCommand(const std::string& command) {
        if (!conn) {
            std::cerr << "Not connected to database" << std::endl;
            return -1;
        }
        
        PGresult* res = PQexec(conn, command.c_str());
        
        ExecStatusType status = PQresultStatus(res);
        
        if (status != PGRES_COMMAND_OK) {
            std::cerr << "Command failed: " << PQerrorMessage(conn) << std::endl;
            PQclear(res);
            return -1;
        }
        
        char* affectedRows = PQcmdTuples(res);
        int numAffected = affectedRows ? std::stoi(affectedRows) : 0;
        
        PQclear(res);
        return numAffected;
    }
    
    // Transaction management
    bool beginTransaction() {
        PGresult* res = PQexec(conn, "BEGIN");
        bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
        PQclear(res);
        return success;
    }
    
    bool commit() {
        PGresult* res = PQexec(conn, "COMMIT");
        bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
        PQclear(res);
        return success;
    }
    
    bool rollback() {
        PGresult* res = PQexec(conn, "ROLLBACK");
        bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
        PQclear(res);
        return success;
    }
    
    // Savepoint management
    bool setSavepoint(const std::string& name) {
        std::string query = "SAVEPOINT " + name;
        PGresult* res = PQexec(conn, query.c_str());
        bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
        PQclear(res);
        return success;
    }
    
    bool releaseSavepoint(const std::string& name) {
        std::string query = "RELEASE SAVEPOINT " + name;
        PGresult* res = PQexec(conn, query.c_str());
        bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
        PQclear(res);
        return success;
    }
    
    bool rollbackToSavepoint(const std::string& name) {
        std::string query = "ROLLBACK TO SAVEPOINT " + name;
        PGresult* res = PQexec(conn, query.c_str());
        bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
        PQclear(res);
        return success;
    }
    
    // COPY operations for bulk data
    bool copyDataFrom(const std::string& table, const std::string& columns,
                      const std::vector<std::string>& data) {
        std::string query = "COPY " + table + " (" + columns + ") FROM STDIN WITH (FORMAT csv)";
        
        PGresult* res = PQexec(conn, query.c_str());
        
        if (PQresultStatus(res) != PGRES_COPY_IN) {
            std::cerr << "COPY FROM failed to start: " << PQerrorMessage(conn) << std::endl;
            PQclear(res);
            return false;
        }
        PQclear(res);
        
        // Send data
        for (const auto& row : data) {
            if (PQputCopyData(conn, row.c_str(), row.length()) != 1) {
                std::cerr << "Failed to send COPY data: " << PQerrorMessage(conn) << std::endl;
                PQputCopyEnd(conn, "Copy aborted");
                return false;
            }
        }
        
        // End COPY
        if (PQputCopyEnd(conn, nullptr) != 1) {
            std::cerr << "COPY FROM failed to complete: " << PQerrorMessage(conn) << std::endl;
            return false;
        }
        
        // Get final result
        res = PQgetResult(conn);
        bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
        PQclear(res);
        
        return success;
    }
    
    // Listen/Notify for async notifications
    bool listen(const std::string& channel) {
        std::string query = "LISTEN " + channel;
        PGresult* res = PQexec(conn, query.c_str());
        bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
        PQclear(res);
        return success;
    }
    
    bool notify(const std::string& channel, const std::string& payload = "") {
        std::string query = "NOTIFY " + channel;
        if (!payload.empty()) {
            query += ", '" + payload + "'";
        }
        
        PGresult* res = PQexec(conn, query.c_str());
        bool success = (PQresultStatus(res) == PGRES_COMMAND_OK);
        PQclear(res);
        return success;
    }
    
    std::vector<std::pair<std::string, std::string>> getNotifications() {
        std::vector<std::pair<std::string, std::string>> notifications;
        
        PQconsumeInput(conn);
        
        PGnotify* notify;
        while ((notify = PQnotifies(conn)) != nullptr) {
            notifications.push_back({notify->relname, 
                                   notify->extra ? notify->extra : ""});
            PQfreemem(notify);
        }
        
        return notifications;
    }
    
    // Escape string for SQL
    std::string escapeString(const std::string& str) {
        size_t len = str.length();
        std::vector<char> escaped(2 * len + 1);
        
        int error;
        size_t escapedLen = PQescapeStringConn(conn, escaped.data(), 
                                               str.c_str(), len, &error);
        
        if (error) {
            std::cerr << "String escape failed" << std::endl;
            return "";
        }
        
        return std::string(escaped.data(), escapedLen);
    }
    
    // Escape bytea data
    std::string escapeBytea(const unsigned char* data, size_t len) {
        size_t escapedLen;
        unsigned char* escaped = PQescapeByteaConn(conn, data, len, &escapedLen);
        
        if (!escaped) {
            return "";
        }
        
        std::string result(reinterpret_cast<char*>(escaped), escapedLen - 1);
        PQfreemem(escaped);
        
        return result;
    }
    
    // Get table information
    std::vector<std::map<std::string, std::string>> getTableInfo(const std::string& table) {
        std::string query = 
            "SELECT column_name, data_type, character_maximum_length, "
            "is_nullable, column_default "
            "FROM information_schema.columns "
            "WHERE table_name = '" + escapeString(table) + "' "
            "ORDER BY ordinal_position";
        
        return executeQuery(query);
    }
    
    // Get database size
    std::string getDatabaseSize(const std::string& dbname) {
        std::string query = "SELECT pg_size_pretty(pg_database_size('" + 
                           escapeString(dbname) + "'))";
        
        auto result = executeQuery(query);
        if (!result.empty() && result[0].size() > 0) {
            return result[0].begin()->second;
        }
        
        return "Unknown";
    }
    
    // Connection status
    bool isConnected() {
        return conn && (PQstatus(conn) == CONNECTION_OK);
    }
    
    // Ping server
    PGPing ping() {
        if (!conn) {
            return PQPING_NO_ATTEMPT;
        }
        
        std::string conninfo = PQconninfo(conn);
        return PQping(conninfo.c_str());
    }
    
    // Reset connection
    bool reset() {
        if (!conn) {
            return false;
        }
        
        PQreset(conn);
        return (PQstatus(conn) == CONNECTION_OK);
    }
    
    // Get connection info
    void printConnectionInfo() {
        if (!conn) {
            std::cout << "Not connected" << std::endl;
            return;
        }
        
        std::cout << "Database: " << PQdb(conn) << std::endl;
        std::cout << "User: " << PQuser(conn) << std::endl;
        std::cout << "Host: " << PQhost(conn) << std::endl;
        std::cout << "Port: " << PQport(conn) << std::endl;
        std::cout << "Server PID: " << PQbackendPID(conn) << std::endl;
        std::cout << "SSL in use: " << (PQsslInUse(conn) ? "Yes" : "No") << std::endl;
        
        if (PQsslInUse(conn)) {
            const char* sslAttribute = PQsslAttribute(conn, "protocol");
            if (sslAttribute) {
                std::cout << "SSL Protocol: " << sslAttribute << std::endl;
            }
        }
    }
    
    // Disconnect
    void disconnect() {
        if (conn) {
            PQfinish(conn);
            conn = nullptr;
            std::cout << "Disconnected from PostgreSQL" << std::endl;
        }
    }
};
```

## Complete libpqxx Implementation

```cpp
#include <iostream>
#include <pqxx/pqxx>
#include <vector>
#include <map>
#include <memory>

class PostgreSQLConnectionPQXX {
private:
    std::unique_ptr<pqxx::connection> conn;
    
public:
    PostgreSQLConnectionPQXX() = default;
    
    ~PostgreSQLConnectionPQXX() {
        disconnect();
    }
    
    // Connect using connection string
    bool connect(const std::string& connStr) {
        try {
            conn = std::make_unique<pqxx::connection>(connStr);
            
            if (conn->is_open()) {
                std::cout << "Connected to database: " << conn->dbname() << std::endl;
                return true;
            }
            
            return false;
            
        } catch (const pqxx::connection_failure& e) {
            std::cerr << "Connection failed: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Connect with individual parameters
    bool connect(const std::string& host, int port, const std::string& dbname,
                 const std::string& user, const std::string& password) {
        
        std::stringstream connStr;
        connStr << "host=" << host 
                << " port=" << port 
                << " dbname=" << dbname 
                << " user=" << user 
                << " password=" << password;
        
        return connect(connStr.str());
    }
    
    // Execute query with automatic transaction
    std::vector<std::map<std::string, std::string>> executeQuery(const std::string& query) {
        std::vector<std::map<std::string, std::string>> results;
        
        try {
            pqxx::work txn(*conn);
            pqxx::result res = txn.exec(query);
            txn.commit();
            
            for (const auto& row : res) {
                std::map<std::string, std::string> rowMap;
                
                for (size_t i = 0; i < row.size(); ++i) {
                    std::string colName = res.column_name(i);
                    
                    if (row[i].is_null()) {
                        rowMap[colName] = "NULL";
                    } else {
                        rowMap[colName] = row[i].as<std::string>();
                    }
                }
                
                results.push_back(rowMap);
            }
            
        } catch (const pqxx::sql_error& e) {
            std::cerr << "SQL error: " << e.what() << std::endl;
            std::cerr << "Query: " << e.query() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
        }
        
        return results;
    }
    
    // Execute prepared statement
    template<typename... Args>
    std::vector<std::map<std::string, std::string>> executePrepared(
        const std::string& name, 
        const std::string& query, 
        Args... args) {
        
        std::vector<std::map<std::string, std::string>> results;
        
        try {
            // Prepare statement
            conn->prepare(name, query);
            
            // Execute with parameters
            pqxx::work txn(*conn);
            pqxx::result res = txn.exec_prepared(name, args...);
            txn.commit();
            
            // Process results
            for (const auto& row : res) {
                std::map<std::string, std::string> rowMap;
                
                for (size_t i = 0; i < row.size(); ++i) {
                    std::string colName = res.column_name(i);
                    rowMap[colName] = row[i].is_null() ? "NULL" : row[i].c_str();
                }
                
                results.push_back(rowMap);
            }
            
            // Unprepare statement
            conn->unprepare(name);
            
        } catch (const std::exception& e) {
            std::cerr << "Prepared statement error: " << e.what() << std::endl;
        }
        
        return results;
    }
    
    // Stream large result sets
    void streamQuery(const std::string& query, 
                     std::function<void(const pqxx::row&)> callback) {
        try {
            pqxx::work txn(*conn);
            
            // Create stream
            pqxx::stream_from stream = pqxx::stream_from::query(txn, query);
            
            // Process rows one by one
            for (auto row : stream) {
                callback(row);
            }
            
            stream.complete();
            txn.commit();
            
        } catch (const std::exception& e) {
            std::cerr << "Stream error: " << e.what() << std::endl;
        }
    }
    
    // Bulk insert using COPY
    bool bulkInsert(const std::string& table, 
                    const std::vector<std::string>& columns,
                    const std::vector<std::vector<std::string>>& data) {
        try {
            pqxx::work txn(*conn);
            
            // Create stream
            pqxx::stream_to stream = pqxx::stream_to::table(txn, table, columns);
            
            // Write data
            for (const auto& row : data) {
                stream << row;
            }
            
            // Complete stream
            stream.complete();
            txn.commit();
            
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "Bulk insert error: " << e.what() << std::endl;
            return false;
        }
    }
    
    // Pipeline for batch operations
    void executeBatch(const std::vector<std::string>& queries) {
        try {
            pqxx::work txn(*conn);
            pqxx::pipeline pipe(txn);
            
            // Queue all queries
            std::vector<pqxx::pipeline::query_id> query_ids;
            for (const auto& query : queries) {
                query_ids.push_back(pipe.insert(query));
            }
            
            // Execute pipeline
            pipe.complete();
            
            // Retrieve results
            for (auto id : query_ids) {
                auto res = pipe.retrieve(id);
                std::cout << "Query " << id << " affected " 
                         << res.affected_rows() << " rows" << std::endl;
            }
            
            txn.commit();
            
        } catch (const std::exception& e) {
            std::cerr << "Batch execution error: " << e.what() << std::endl;
        }
    }
    
    // Notifications
    class NotificationReceiver : public pqxx::notification_receiver {
    private:
        std::function<void(const std::string&, const std::string&)> callback;
        
    public:
        NotificationReceiver(pqxx::connection& c, const std::string& channel,
                           std::function<void(const std::string&, const std::string&)> cb)
            : pqxx::notification_receiver(c, channel), callback(cb) {}
        
        void operator()(const std::string& payload, int backend_pid) override {
            callback(channel(), payload);
        }
    };
    
    std::unique_ptr<NotificationReceiver> listenForNotifications(
        const std::string& channel,
        std::function<void(const std::string&, const std::string&)> callback) {
        
        return std::make_unique<NotificationReceiver>(*conn, channel, callback);
    }
    
    // Connection pooling example
    class ConnectionPool {
    private:
        std::vector<std::unique_ptr<pqxx::connection>> pool;
        std::queue<pqxx::connection*> available;
        std::mutex mutex;
        std::condition_variable cv;
        std::string connStr;
        size_t maxSize;
        
    public:
        ConnectionPool(const std::string& connectionString, size_t poolSize)
            : connStr(connectionString), maxSize(poolSize) {
            
            for (size_t i = 0; i < poolSize; ++i) {
                auto conn = std::make_unique<pqxx::connection>(connStr);
                available.push(conn.get());
                pool.push_back(std::move(conn));
            }
        }
        
        pqxx::connection* acquire() {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [this] { return !available.empty(); });
            
            auto conn = available.front();
            available.pop();
            return conn;
        }
        
        void release(pqxx::connection* conn) {
            std::lock_guard<std::mutex> lock(mutex);
            available.push(conn);
            cv.notify_one();
        }
    };
    
    // Disconnect
    void disconnect() {
        if (conn && conn->is_open()) {
            conn->close();
            std::cout << "Disconnected from PostgreSQL" << std::endl;
        }
    }
    
    bool isConnected() {
        return conn && conn->is_open();
    }
};

// Usage example
int main() {
    // Using libpq wrapper
    PostgreSQLConnection db;
    
    if (db.connect("localhost", 5432, "testdb", "postgres", "password", false, 10)) {
        // Create table
        db.executeCommand(
            "CREATE TABLE IF NOT EXISTS users ("
            "id SERIAL PRIMARY KEY,"
            "name VARCHAR(100) NOT NULL,"
            "email VARCHAR(100) UNIQUE,"
            "age INTEGER,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ")"
        );
        
        // Insert data with prepared statement
        auto result = db.executePrepared("insert_user",
            "INSERT INTO users (name, email, age) VALUES ($1, $2, $3) RETURNING id",
            {"John Doe", "john@example.com", "30"}
        );
        
        // Query data
        auto users = db.executeQuery("SELECT * FROM users");
        for (const auto& user : users) {
            for (const auto& [key, value] : user) {
                std::cout << key << ": " << value << " ";
            }
            std::cout << std::endl;
        }
        
        // Transaction example
        db.beginTransaction();
        db.executeCommand("UPDATE users SET age = age + 1");
        db.setSavepoint("sp1");
        db.executeCommand("DELETE FROM users WHERE age > 100");
        db.rollbackToSavepoint("sp1");
        db.commit();
        
        // Listen for notifications
        db.listen("user_updates");
        db.notify("user_updates", "User updated");
        
        auto notifications = db.getNotifications();
        for (const auto& [channel, payload] : notifications) {
            std::cout << "Notification on " << channel << ": " << payload << std::endl;
        }
        
        db.disconnect();
    }
    
    // Using libpqxx
    PostgreSQLConnectionPQXX dbxx;
    
    if (dbxx.connect("host=localhost port=5432 dbname=testdb user=postgres password=password")) {
        // Execute query
        auto results = dbxx.executeQuery("SELECT version()");
        
        // Prepared statement
        auto users = dbxx.executePrepared("get_user", 
            "SELECT * FROM users WHERE id = $1", 1);
        
        // Stream large results
        dbxx.streamQuery("SELECT * FROM large_table", 
            [](const pqxx::row& row) {
                std::cout << "Processing row: " << row[0].c_str() << std::endl;
            });
        
        // Bulk insert
        dbxx.bulkInsert("users", {"name", "email", "age"},
            {{"Alice", "alice@example.com", "25"},
             {"Bob", "bob@example.com", "35"}});
        
        dbxx.disconnect();
    }
    
    return 0;
}
```

## Compilation

### Using g++ with libpq
```bash
g++ -std=c++17 -o pg_app pg_app.cpp \
    -lpq \
    -I/usr/include/postgresql \
    -L/usr/lib/x86_64-linux-gnu
```

### Using g++ with libpqxx
```bash
g++ -std=c++17 -o pg_app pg_app.cpp \
    -lpqxx -lpq \
    -I/usr/include/pqxx \
    -I/usr/include/postgresql
```

### CMake Configuration
```cmake
cmake_minimum_required(VERSION 3.10)
project(PostgreSQLApp)

set(CMAKE_CXX_STANDARD 17)

# Find PostgreSQL
find_package(PostgreSQL REQUIRED)

# Find libpqxx
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

add_executable(pg_app main.cpp)

# For libpq only
target_include_directories(pg_app PRIVATE ${PostgreSQL_INCLUDE_DIRS})
target_link_libraries(pg_app ${PostgreSQL_LIBRARIES})

# For libpqxx
target_include_directories(pg_app PRIVATE ${PQXX_INCLUDE_DIRS})
target_link_libraries(pg_app ${PQXX_LIBRARIES})
```

## Error Handling

### SQLSTATE Error Codes

| Class | Code | Description |
|-------|------|-------------|
| 00 | 00000 | Successful completion |
| 01 | 01000 | Warning |
| 02 | 02000 | No data |
| 03 | 03000 | SQL statement not yet complete |
| 08 | 08000 | Connection exception |
| 08 | 08003 | Connection does not exist |
| 08 | 08006 | Connection failure |
| 08 | 08001 | SQL client unable to establish connection |
| 08 | 08004 | SQL server rejected connection |
| 09 | 09000 | Triggered action exception |
| 0A | 0A000 | Feature not supported |
| 22 | 22000 | Data exception |
| 22 | 22001 | String data right truncation |
| 22 | 22003 | Numeric value out of range |
| 22 | 22007 | Invalid datetime format |
| 22 | 22012 | Division by zero |
| 22 | 22P02 | Invalid text representation |
| 23 | 23000 | Integrity constraint violation |
| 23 | 23502 | Not null violation |
| 23 | 23503 | Foreign key violation |
| 23 | 23505 | Unique violation |
| 23 | 23514 | Check violation |
| 25 | 25000 | Invalid transaction state |
| 25 | 25001 | Active SQL transaction |
| 25 | 25P02 | In failed SQL transaction |
| 28 | 28000 | Invalid authorization specification |
| 28 | 28P01 | Invalid password |
| 40 | 40000 | Transaction rollback |
| 40 | 40001 | Serialization failure |
| 40 | 40P01 | Deadlock detected |
| 42 | 42000 | Syntax error or access rule violation |
| 42 | 42601 | Syntax error |
| 42 | 42703 | Undefined column |
| 42 | 42883 | Undefined function |
| 42 | 42P01 | Undefined table |
| 53 | 53000 | Insufficient resources |
| 53 | 53100 | Disk full |
| 53 | 53200 | Out of memory |
| 53 | 53300 | Too many connections |
| 54 | 54000 | Program limit exceeded |
| 54 | 54001 | Statement too complex |
| 54 | 54011 | Too many columns |
| 54 | 54023 | Too many arguments |
| 55 | 55000 | Object not in prerequisite state |
| 55 | 55006 | Object in use |
| 55 | 55P03 | Lock not available |
| 57 | 57000 | Operator intervention |
| 57 | 57014 | Query canceled |
| 57 | 57P01 | Admin shutdown |
| 57 | 57P02 | Crash shutdown |
| 57 | 57P03 | Cannot connect now |
| 58 | 58000 | System error |
| 58 | 58030 | IO error |

## Performance Optimization

### Connection Pooling Best Practices
1. **Pool Size**: Set to number of CPU cores * 2-4
2. **Connection Lifetime**: Limit to 30-60 minutes
3. **Idle Timeout**: Close idle connections after 10 minutes
4. **Validation Query**: Use "SELECT 1" to validate connections

### Query Optimization
1. **Use EXPLAIN ANALYZE** to understand query plans
2. **Create appropriate indexes** for frequently queried columns
3. **Use prepared statements** to reduce parsing overhead
4. **Batch operations** when possible
5. **Use COPY** for bulk data operations
6. **Enable query statistics** with pg_stat_statements

### PostgreSQL-Specific Features
1. **Arrays**: Native array support for complex data
2. **JSON/JSONB**: Document storage within relational tables
3. **Full-text search**: Built-in text search capabilities
4. **Table partitioning**: For large datasets
5. **Parallel queries**: Automatic query parallelization
6. **Logical replication**: For read scaling

## Security Considerations

1. **Always use SSL/TLS** in production (sslmode=require or verify-full)
2. **Use prepared statements** to prevent SQL injection
3. **Implement row-level security** when appropriate
4. **Use pg_hba.conf** for connection authentication
5. **Regular security updates** for PostgreSQL and client libraries
6. **Audit logging** with pgAudit extension
7. **Encrypt sensitive data** at rest and in transit