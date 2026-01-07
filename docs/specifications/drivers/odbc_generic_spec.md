# Generic ODBC C++ Interface Specification

## Overview

ODBC (Open Database Connectivity) is a standard API for accessing database management systems (DBMS). It provides a universal data access interface, allowing applications to access data from a variety of database systems using the same code.

## Architecture

```
┌─────────────────────────────────────────┐
│         Application (C++)               │
├─────────────────────────────────────────┤
│         ODBC API Layer                  │
├─────────────────────────────────────────┤
│       ODBC Driver Manager               │
├─────────────────────────────────────────┤
│   Database-Specific ODBC Drivers        │
│  ┌──────┬──────┬──────────┬─────────┐ │
│  │MySQL │PostG │  Oracle  │   DB2   │ │
│  │Driver│reSQL │  Driver  │ Driver  │ │
│  └──────┴──────┴──────────┴─────────┘ │
├─────────────────────────────────────────┤
│        Database Systems                 │
└─────────────────────────────────────────┘
```

## Installation

### Linux (Debian/Ubuntu)
```bash
# Install unixODBC
sudo apt-get update
sudo apt-get install unixodbc unixodbc-dev

# Install database-specific ODBC drivers
sudo apt-get install odbc-postgresql  # PostgreSQL
sudo apt-get install libmyodbc        # MySQL
sudo apt-get install tdsodbc          # SQL Server/Sybase
sudo apt-get install libsqliteodbc    # SQLite
```

### Linux (RHEL/CentOS)
```bash
sudo yum install unixODBC unixODBC-devel

# Database-specific drivers
sudo yum install postgresql-odbc
sudo yum install mysql-connector-odbc
sudo yum install freetds
```

### Windows
ODBC is built into Windows. Install database-specific drivers:
- Download drivers from database vendor websites
- Use ODBC Data Source Administrator to configure DSNs

### macOS
```bash
brew install unixodbc

# Database-specific drivers
brew install psqlodbc
brew install mysql-connector-odbc
```

## Configuration Files

### /etc/odbcinst.ini (Driver Configuration)
```ini
[PostgreSQL]
Description = PostgreSQL ODBC driver
Driver = /usr/lib/x86_64-linux-gnu/odbc/psqlodbcw.so
Setup = /usr/lib/x86_64-linux-gnu/odbc/libodbcpsqlS.so
FileUsage = 1

[MySQL]
Description = MySQL ODBC Driver
Driver = /usr/lib/x86_64-linux-gnu/odbc/libmyodbc8w.so
Setup = /usr/lib/x86_64-linux-gnu/odbc/libmyodbc8S.so
FileUsage = 1

[SQLite3]
Description = SQLite3 ODBC Driver
Driver = /usr/lib/x86_64-linux-gnu/odbc/libsqlite3odbc.so
Setup = /usr/lib/x86_64-linux-gnu/odbc/libsqlite3odbc.so
FileUsage = 1

[FreeTDS]
Description = FreeTDS Driver for SQL Server
Driver = /usr/lib/x86_64-linux-gnu/odbc/libtdsodbc.so
Setup = /usr/lib/x86_64-linux-gnu/odbc/libtdsS.so
FileUsage = 1
```

### /etc/odbc.ini (Data Source Configuration)
```ini
[PostgreSQL-DSN]
Description = PostgreSQL Database
Driver = PostgreSQL
Database = testdb
Servername = localhost
Port = 5432
Username = postgres
Password = password
Protocol = 7.4
ReadOnly = No
RowVersioning = No
ShowSystemTables = No
ConnSettings =

[MySQL-DSN]
Description = MySQL Database
Driver = MySQL
Server = localhost
Port = 3306
Database = testdb
User = root
Password = password
Option = 3
Socket = /var/run/mysqld/mysqld.sock

[SQLServer-DSN]
Description = MS SQL Server
Driver = FreeTDS
Server = 192.168.1.100
Port = 1433
Database = testdb
TDS_Version = 7.4
```

## Header Files Required

```cpp
#ifdef _WIN32
    #include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <sqlucode.h>
```

## Connection String Formats

### DSN Connection
```
DSN=DataSourceName;UID=username;PWD=password;
```

### DSN-less Connection Examples

#### MySQL
```
Driver={MySQL ODBC 8.0 Driver};Server=localhost;Port=3306;Database=testdb;User=root;Password=password;Option=3;
```

#### PostgreSQL
```
Driver={PostgreSQL};Server=localhost;Port=5432;Database=testdb;Uid=postgres;Pwd=password;
```

#### SQL Server
```
Driver={ODBC Driver 17 for SQL Server};Server=localhost;Database=testdb;Uid=sa;Pwd=password;
```

#### SQLite
```
Driver={SQLite3};Database=/path/to/database.db;
```

#### Oracle
```
Driver={Oracle ODBC Driver};DBQ=localhost:1521/XE;Uid=system;Pwd=password;
```

#### IBM DB2
```
Driver={IBM DB2 ODBC DRIVER};Database=testdb;Hostname=localhost;Port=50000;Protocol=TCPIP;Uid=db2admin;Pwd=password;
```

## Complete Generic ODBC Implementation

```cpp
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
    #include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

class ODBCConnection {
private:
    SQLHENV hEnv;
    SQLHDBC hDbc;
    bool connected;
    std::string lastError;
    std::string databaseType;
    
    // Extract error information
    std::string extractError(SQLHANDLE handle, SQLSMALLINT handleType) {
        SQLCHAR sqlState[6];
        SQLINTEGER nativeError;
        SQLCHAR message[SQL_MAX_MESSAGE_LENGTH];
        SQLSMALLINT messageLength;
        std::stringstream ss;
        
        SQLSMALLINT recNumber = 1;
        SQLRETURN ret;
        
        while ((ret = SQLGetDiagRec(handleType, handle, recNumber, sqlState, 
                                    &nativeError, message, sizeof(message), 
                                    &messageLength)) == SQL_SUCCESS || 
               ret == SQL_SUCCESS_WITH_INFO) {
            
            ss << "SQLState: " << sqlState << ", ";
            ss << "Native Error: " << nativeError << ", ";
            ss << "Message: " << message << std::endl;
            recNumber++;
        }
        
        return ss.str();
    }
    
    // Detect database type from connection
    void detectDatabaseType() {
        char dbmsName[256];
        SQLSMALLINT nameLength;
        
        SQLRETURN ret = SQLGetInfo(hDbc, SQL_DBMS_NAME, dbmsName, 
                                   sizeof(dbmsName), &nameLength);
        
        if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
            databaseType = std::string(dbmsName);
            std::transform(databaseType.begin(), databaseType.end(), 
                          databaseType.begin(), ::toupper);
        }
    }
    
    // Convert SQL type to string representation
    std::string sqlTypeToString(SQLSMALLINT sqlType) {
        switch (sqlType) {
            case SQL_CHAR: return "CHAR";
            case SQL_VARCHAR: return "VARCHAR";
            case SQL_LONGVARCHAR: return "LONGVARCHAR";
            case SQL_WCHAR: return "WCHAR";
            case SQL_WVARCHAR: return "WVARCHAR";
            case SQL_WLONGVARCHAR: return "WLONGVARCHAR";
            case SQL_DECIMAL: return "DECIMAL";
            case SQL_NUMERIC: return "NUMERIC";
            case SQL_SMALLINT: return "SMALLINT";
            case SQL_INTEGER: return "INTEGER";
            case SQL_REAL: return "REAL";
            case SQL_FLOAT: return "FLOAT";
            case SQL_DOUBLE: return "DOUBLE";
            case SQL_BIT: return "BIT";
            case SQL_TINYINT: return "TINYINT";
            case SQL_BIGINT: return "BIGINT";
            case SQL_BINARY: return "BINARY";
            case SQL_VARBINARY: return "VARBINARY";
            case SQL_LONGVARBINARY: return "LONGVARBINARY";
            case SQL_TYPE_DATE: return "DATE";
            case SQL_TYPE_TIME: return "TIME";
            case SQL_TYPE_TIMESTAMP: return "TIMESTAMP";
            case SQL_GUID: return "GUID";
            default: return "UNKNOWN(" + std::to_string(sqlType) + ")";
        }
    }
    
public:
    ODBCConnection() : hEnv(SQL_NULL_HENV), hDbc(SQL_NULL_HDBC), connected(false) {}
    
    ~ODBCConnection() {
        disconnect();
    }
    
    // Initialize ODBC environment
    bool initialize() {
        SQLRETURN ret;
        
        // Allocate environment handle
        ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
        if (!SQL_SUCCEEDED(ret)) {
            lastError = "Failed to allocate environment handle";
            return false;
        }
        
        // Set ODBC version to 3.x
        ret = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, 
                           (SQLPOINTER)SQL_OV_ODBC3, 0);
        if (!SQL_SUCCEEDED(ret)) {
            lastError = "Failed to set ODBC version: " + extractError(hEnv, SQL_HANDLE_ENV);
            SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
            hEnv = SQL_NULL_HENV;
            return false;
        }
        
        // Allocate connection handle
        ret = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
        if (!SQL_SUCCEEDED(ret)) {
            lastError = "Failed to allocate connection handle: " + 
                       extractError(hEnv, SQL_HANDLE_ENV);
            SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
            hEnv = SQL_NULL_HENV;
            return false;
        }
        
        return true;
    }
    
    // Connect using DSN
    bool connectDSN(const std::string& dsn, const std::string& username = "",
                    const std::string& password = "") {
        
        if (!initialize()) {
            return false;
        }
        
        std::string connStr = "DSN=" + dsn;
        if (!username.empty()) {
            connStr += ";UID=" + username;
        }
        if (!password.empty()) {
            connStr += ";PWD=" + password;
        }
        
        return connect(connStr);
    }
    
    // Connect using connection string
    bool connect(const std::string& connectionString) {
        if (hDbc == SQL_NULL_HDBC && !initialize()) {
            return false;
        }
        
        SQLCHAR outConnStr[1024];
        SQLSMALLINT outConnStrLen;
        
        // Set connection timeout
        SQLSetConnectAttr(hDbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)10, 0);
        
        // Set connection pooling
        SQLSetConnectAttr(hDbc, SQL_ATTR_CONNECTION_POOLING, 
                         (SQLPOINTER)SQL_CP_ONE_PER_DRIVER, 0);
        
        // Connect
        SQLRETURN ret = SQLDriverConnect(
            hDbc,
            NULL,
            (SQLCHAR*)connectionString.c_str(),
            SQL_NTS,
            outConnStr,
            sizeof(outConnStr),
            &outConnStrLen,
            SQL_DRIVER_NOPROMPT
        );
        
        if (!SQL_SUCCEEDED(ret)) {
            lastError = "Connection failed: " + extractError(hDbc, SQL_HANDLE_DBC);
            return false;
        }
        
        connected = true;
        
        // Detect database type
        detectDatabaseType();
        
        // Set connection attributes
        SQLSetConnectAttr(hDbc, SQL_ATTR_AUTOCOMMIT, 
                         (SQLPOINTER)SQL_AUTOCOMMIT_ON, 0);
        
        // Get connection info
        printConnectionInfo();
        
        return true;
    }
    
    // Get connection information
    void printConnectionInfo() {
        char buffer[256];
        SQLSMALLINT bufferLength;
        
        std::cout << "Connected to ODBC Data Source" << std::endl;
        
        // Database name
        if (SQL_SUCCEEDED(SQLGetInfo(hDbc, SQL_DATABASE_NAME, buffer, 
                                     sizeof(buffer), &bufferLength))) {
            std::cout << "  Database: " << buffer << std::endl;
        }
        
        // DBMS name and version
        if (SQL_SUCCEEDED(SQLGetInfo(hDbc, SQL_DBMS_NAME, buffer, 
                                     sizeof(buffer), &bufferLength))) {
            std::cout << "  DBMS: " << buffer;
            
            if (SQL_SUCCEEDED(SQLGetInfo(hDbc, SQL_DBMS_VER, buffer, 
                                         sizeof(buffer), &bufferLength))) {
                std::cout << " " << buffer;
            }
            std::cout << std::endl;
        }
        
        // Driver name and version
        if (SQL_SUCCEEDED(SQLGetInfo(hDbc, SQL_DRIVER_NAME, buffer, 
                                     sizeof(buffer), &bufferLength))) {
            std::cout << "  Driver: " << buffer;
            
            if (SQL_SUCCEEDED(SQLGetInfo(hDbc, SQL_DRIVER_VER, buffer, 
                                         sizeof(buffer), &bufferLength))) {
                std::cout << " " << buffer;
            }
            std::cout << std::endl;
        }
        
        // ODBC version
        if (SQL_SUCCEEDED(SQLGetInfo(hDbc, SQL_DRIVER_ODBC_VER, buffer, 
                                     sizeof(buffer), &bufferLength))) {
            std::cout << "  ODBC Version: " << buffer << std::endl;
        }
    }
    
    // Execute query and return results
    std::vector<std::map<std::string, std::string>> executeQuery(const std::string& query) {
        std::vector<std::map<std::string, std::string>> results;
        
        if (!connected) {
            lastError = "Not connected to database";
            return results;
        }
        
        SQLHSTMT hStmt;
        SQLRETURN ret;
        
        // Allocate statement handle
        ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
        if (!SQL_SUCCEEDED(ret)) {
            lastError = "Failed to allocate statement handle: " + 
                       extractError(hDbc, SQL_HANDLE_DBC);
            return results;
        }
        
        // Set statement attributes for better performance
        SQLSetStmtAttr(hStmt, SQL_ATTR_CURSOR_TYPE, 
                      (SQLPOINTER)SQL_CURSOR_FORWARD_ONLY, 0);
        SQLSetStmtAttr(hStmt, SQL_ATTR_CONCURRENCY, 
                      (SQLPOINTER)SQL_CONCUR_READ_ONLY, 0);
        
        // Execute query
        ret = SQLExecDirect(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
        if (!SQL_SUCCEEDED(ret)) {
            lastError = "Query execution failed: " + extractError(hStmt, SQL_HANDLE_STMT);
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            return results;
        }
        
        // Get column count
        SQLSMALLINT columnCount;
        SQLNumResultCols(hStmt, &columnCount);
        
        if (columnCount == 0) {
            // No result set (UPDATE, DELETE, etc.)
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            return results;
        }
        
        // Get column metadata
        struct ColumnInfo {
            std::string name;
            SQLSMALLINT type;
            SQLULEN size;
            SQLSMALLINT decimals;
            SQLSMALLINT nullable;
        };
        
        std::vector<ColumnInfo> columns;
        
        for (SQLSMALLINT i = 1; i <= columnCount; ++i) {
            ColumnInfo col;
            SQLCHAR columnName[256];
            SQLSMALLINT nameLength;
            
            SQLDescribeCol(hStmt, i, columnName, sizeof(columnName), &nameLength,
                          &col.type, &col.size, &col.decimals, &col.nullable);
            
            col.name = std::string((char*)columnName);
            columns.push_back(col);
        }
        
        // Fetch rows
        while (SQLFetch(hStmt) == SQL_SUCCESS) {
            std::map<std::string, std::string> row;
            
            for (SQLSMALLINT i = 0; i < columnCount; ++i) {
                SQLLEN indicator;
                char buffer[4096];
                
                ret = SQLGetData(hStmt, i + 1, SQL_C_CHAR, buffer, 
                                sizeof(buffer), &indicator);
                
                if (indicator == SQL_NULL_DATA) {
                    row[columns[i].name] = "NULL";
                } else if (SQL_SUCCEEDED(ret)) {
                    row[columns[i].name] = std::string(buffer);
                } else {
                    row[columns[i].name] = "ERROR";
                }
            }
            
            results.push_back(row);
        }
        
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return results;
    }
    
    // Execute prepared statement with parameters
    std::vector<std::map<std::string, std::string>> executePrepared(
        const std::string& query,
        const std::vector<std::pair<int, std::string>>& params) {
        
        std::vector<std::map<std::string, std::string>> results;
        
        if (!connected) {
            lastError = "Not connected to database";
            return results;
        }
        
        SQLHSTMT hStmt;
        SQLRETURN ret;
        
        // Allocate statement handle
        ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
        if (!SQL_SUCCEEDED(ret)) {
            lastError = "Failed to allocate statement handle";
            return results;
        }
        
        // Prepare statement
        ret = SQLPrepare(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
        if (!SQL_SUCCEEDED(ret)) {
            lastError = "Failed to prepare statement: " + 
                       extractError(hStmt, SQL_HANDLE_STMT);
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            return results;
        }
        
        // Bind parameters
        std::vector<SQLLEN> indicators(params.size());
        
        for (size_t i = 0; i < params.size(); ++i) {
            const auto& param = params[i];
            indicators[i] = SQL_NTS;
            
            ret = SQLBindParameter(hStmt, i + 1, SQL_PARAM_INPUT, 
                                  SQL_C_CHAR, param.first, 
                                  param.second.length(), 0,
                                  (SQLPOINTER)param.second.c_str(),
                                  param.second.length(), &indicators[i]);
            
            if (!SQL_SUCCEEDED(ret)) {
                lastError = "Failed to bind parameter " + std::to_string(i + 1);
                SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
                return results;
            }
        }
        
        // Execute prepared statement
        ret = SQLExecute(hStmt);
        if (!SQL_SUCCEEDED(ret)) {
            lastError = "Failed to execute prepared statement: " + 
                       extractError(hStmt, SQL_HANDLE_STMT);
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            return results;
        }
        
        // Process results (same as executeQuery)
        SQLSMALLINT columnCount;
        SQLNumResultCols(hStmt, &columnCount);
        
        if (columnCount > 0) {
            // Get column names
            std::vector<std::string> columnNames;
            
            for (SQLSMALLINT i = 1; i <= columnCount; ++i) {
                SQLCHAR columnName[256];
                SQLSMALLINT nameLength;
                SQLSMALLINT dataType;
                SQLULEN columnSize;
                SQLSMALLINT decimalDigits;
                SQLSMALLINT nullable;
                
                SQLDescribeCol(hStmt, i, columnName, sizeof(columnName), &nameLength,
                              &dataType, &columnSize, &decimalDigits, &nullable);
                
                columnNames.push_back(std::string((char*)columnName));
            }
            
            // Fetch rows
            while (SQLFetch(hStmt) == SQL_SUCCESS) {
                std::map<std::string, std::string> row;
                
                for (SQLSMALLINT i = 0; i < columnCount; ++i) {
                    SQLLEN indicator;
                    char buffer[4096];
                    
                    SQLGetData(hStmt, i + 1, SQL_C_CHAR, buffer, 
                              sizeof(buffer), &indicator);
                    
                    if (indicator == SQL_NULL_DATA) {
                        row[columnNames[i]] = "NULL";
                    } else {
                        row[columnNames[i]] = std::string(buffer);
                    }
                }
                
                results.push_back(row);
            }
        }
        
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return results;
    }
    
    // Execute command (INSERT, UPDATE, DELETE)
    int executeUpdate(const std::string& command) {
        if (!connected) {
            lastError = "Not connected to database";
            return -1;
        }
        
        SQLHSTMT hStmt;
        SQLRETURN ret;
        
        // Allocate statement handle
        ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
        if (!SQL_SUCCEEDED(ret)) {
            lastError = "Failed to allocate statement handle";
            return -1;
        }
        
        // Execute command
        ret = SQLExecDirect(hStmt, (SQLCHAR*)command.c_str(), SQL_NTS);
        if (!SQL_SUCCEEDED(ret)) {
            lastError = "Command execution failed: " + extractError(hStmt, SQL_HANDLE_STMT);
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            return -1;
        }
        
        // Get number of affected rows
        SQLLEN rowCount;
        SQLRowCount(hStmt, &rowCount);
        
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return (int)rowCount;
    }
    
    // Transaction management
    bool beginTransaction() {
        SQLRETURN ret = SQLSetConnectAttr(hDbc, SQL_ATTR_AUTOCOMMIT, 
                                          (SQLPOINTER)SQL_AUTOCOMMIT_OFF, 0);
        return SQL_SUCCEEDED(ret);
    }
    
    bool commit() {
        SQLRETURN ret = SQLEndTran(SQL_HANDLE_DBC, hDbc, SQL_COMMIT);
        if (SQL_SUCCEEDED(ret)) {
            // Re-enable autocommit
            SQLSetConnectAttr(hDbc, SQL_ATTR_AUTOCOMMIT, 
                             (SQLPOINTER)SQL_AUTOCOMMIT_ON, 0);
            return true;
        }
        return false;
    }
    
    bool rollback() {
        SQLRETURN ret = SQLEndTran(SQL_HANDLE_DBC, hDbc, SQL_ROLLBACK);
        if (SQL_SUCCEEDED(ret)) {
            // Re-enable autocommit
            SQLSetConnectAttr(hDbc, SQL_ATTR_AUTOCOMMIT, 
                             (SQLPOINTER)SQL_AUTOCOMMIT_ON, 0);
            return true;
        }
        return false;
    }
    
    // Catalog functions
    std::vector<std::string> getTables(const std::string& catalog = "",
                                       const std::string& schema = "",
                                       const std::string& tableType = "TABLE") {
        std::vector<std::string> tables;
        
        if (!connected) return tables;
        
        SQLHSTMT hStmt;
        SQLRETURN ret;
        
        ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
        if (!SQL_SUCCEEDED(ret)) return tables;
        
        // Get tables
        ret = SQLTables(hStmt,
                       catalog.empty() ? NULL : (SQLCHAR*)catalog.c_str(), SQL_NTS,
                       schema.empty() ? NULL : (SQLCHAR*)schema.c_str(), SQL_NTS,
                       (SQLCHAR*)"%", SQL_NTS,
                       (SQLCHAR*)tableType.c_str(), SQL_NTS);
        
        if (SQL_SUCCEEDED(ret)) {
            while (SQLFetch(hStmt) == SQL_SUCCESS) {
                SQLCHAR tableName[256];
                SQLLEN indicator;
                
                // Table name is in column 3
                SQLGetData(hStmt, 3, SQL_C_CHAR, tableName, 
                          sizeof(tableName), &indicator);
                
                if (indicator != SQL_NULL_DATA) {
                    tables.push_back(std::string((char*)tableName));
                }
            }
        }
        
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return tables;
    }
    
    // Get columns for a table
    std::vector<std::map<std::string, std::string>> getColumns(
        const std::string& table,
        const std::string& catalog = "",
        const std::string& schema = "") {
        
        std::vector<std::map<std::string, std::string>> columns;
        
        if (!connected) return columns;
        
        SQLHSTMT hStmt;
        SQLRETURN ret;
        
        ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
        if (!SQL_SUCCEEDED(ret)) return columns;
        
        // Get columns
        ret = SQLColumns(hStmt,
                        catalog.empty() ? NULL : (SQLCHAR*)catalog.c_str(), SQL_NTS,
                        schema.empty() ? NULL : (SQLCHAR*)schema.c_str(), SQL_NTS,
                        (SQLCHAR*)table.c_str(), SQL_NTS,
                        NULL, 0);
        
        if (SQL_SUCCEEDED(ret)) {
            while (SQLFetch(hStmt) == SQL_SUCCESS) {
                std::map<std::string, std::string> column;
                SQLCHAR buffer[256];
                SQLLEN indicator;
                
                // Column name (column 4)
                SQLGetData(hStmt, 4, SQL_C_CHAR, buffer, sizeof(buffer), &indicator);
                column["name"] = std::string((char*)buffer);
                
                // Data type name (column 6)
                SQLGetData(hStmt, 6, SQL_C_CHAR, buffer, sizeof(buffer), &indicator);
                column["type"] = std::string((char*)buffer);
                
                // Column size (column 7)
                SQLGetData(hStmt, 7, SQL_C_CHAR, buffer, sizeof(buffer), &indicator);
                column["size"] = std::string((char*)buffer);
                
                // Nullable (column 11)
                SQLGetData(hStmt, 11, SQL_C_CHAR, buffer, sizeof(buffer), &indicator);
                column["nullable"] = std::string((char*)buffer);
                
                // Default (column 13)
                SQLGetData(hStmt, 13, SQL_C_CHAR, buffer, sizeof(buffer), &indicator);
                if (indicator != SQL_NULL_DATA) {
                    column["default"] = std::string((char*)buffer);
                } else {
                    column["default"] = "NULL";
                }
                
                columns.push_back(column);
            }
        }
        
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return columns;
    }
    
    // Get primary keys
    std::vector<std::string> getPrimaryKeys(const std::string& table,
                                           const std::string& catalog = "",
                                           const std::string& schema = "") {
        std::vector<std::string> keys;
        
        if (!connected) return keys;
        
        SQLHSTMT hStmt;
        SQLRETURN ret;
        
        ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
        if (!SQL_SUCCEEDED(ret)) return keys;
        
        // Get primary keys
        ret = SQLPrimaryKeys(hStmt,
                           catalog.empty() ? NULL : (SQLCHAR*)catalog.c_str(), SQL_NTS,
                           schema.empty() ? NULL : (SQLCHAR*)schema.c_str(), SQL_NTS,
                           (SQLCHAR*)table.c_str(), SQL_NTS);
        
        if (SQL_SUCCEEDED(ret)) {
            while (SQLFetch(hStmt) == SQL_SUCCESS) {
                SQLCHAR columnName[256];
                SQLLEN indicator;
                
                // Column name is in column 4
                SQLGetData(hStmt, 4, SQL_C_CHAR, columnName, 
                          sizeof(columnName), &indicator);
                
                if (indicator != SQL_NULL_DATA) {
                    keys.push_back(std::string((char*)columnName));
                }
            }
        }
        
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return keys;
    }
    
    // Get foreign keys
    std::vector<std::map<std::string, std::string>> getForeignKeys(
        const std::string& table,
        const std::string& catalog = "",
        const std::string& schema = "") {
        
        std::vector<std::map<std::string, std::string>> foreignKeys;
        
        if (!connected) return foreignKeys;
        
        SQLHSTMT hStmt;
        SQLRETURN ret;
        
        ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
        if (!SQL_SUCCEEDED(ret)) return foreignKeys;
        
        // Get foreign keys
        ret = SQLForeignKeys(hStmt,
                           NULL, 0,  // PK catalog
                           NULL, 0,  // PK schema
                           NULL, 0,  // PK table
                           catalog.empty() ? NULL : (SQLCHAR*)catalog.c_str(), SQL_NTS,
                           schema.empty() ? NULL : (SQLCHAR*)schema.c_str(), SQL_NTS,
                           (SQLCHAR*)table.c_str(), SQL_NTS);
        
        if (SQL_SUCCEEDED(ret)) {
            while (SQLFetch(hStmt) == SQL_SUCCESS) {
                std::map<std::string, std::string> fk;
                SQLCHAR buffer[256];
                SQLLEN indicator;
                
                // PK table (column 3)
                SQLGetData(hStmt, 3, SQL_C_CHAR, buffer, sizeof(buffer), &indicator);
                fk["pk_table"] = std::string((char*)buffer);
                
                // PK column (column 4)
                SQLGetData(hStmt, 4, SQL_C_CHAR, buffer, sizeof(buffer), &indicator);
                fk["pk_column"] = std::string((char*)buffer);
                
                // FK column (column 8)
                SQLGetData(hStmt, 8, SQL_C_CHAR, buffer, sizeof(buffer), &indicator);
                fk["fk_column"] = std::string((char*)buffer);
                
                // FK name (column 12)
                SQLGetData(hStmt, 12, SQL_C_CHAR, buffer, sizeof(buffer), &indicator);
                if (indicator != SQL_NULL_DATA) {
                    fk["fk_name"] = std::string((char*)buffer);
                }
                
                foreignKeys.push_back(fk);
            }
        }
        
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return foreignKeys;
    }
    
    // Get stored procedures
    std::vector<std::string> getProcedures(const std::string& catalog = "",
                                          const std::string& schema = "") {
        std::vector<std::string> procedures;
        
        if (!connected) return procedures;
        
        SQLHSTMT hStmt;
        SQLRETURN ret;
        
        ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
        if (!SQL_SUCCEEDED(ret)) return procedures;
        
        // Get procedures
        ret = SQLProcedures(hStmt,
                          catalog.empty() ? NULL : (SQLCHAR*)catalog.c_str(), SQL_NTS,
                          schema.empty() ? NULL : (SQLCHAR*)schema.c_str(), SQL_NTS,
                          (SQLCHAR*)"%", SQL_NTS);
        
        if (SQL_SUCCEEDED(ret)) {
            while (SQLFetch(hStmt) == SQL_SUCCESS) {
                SQLCHAR procName[256];
                SQLLEN indicator;
                
                // Procedure name is in column 3
                SQLGetData(hStmt, 3, SQL_C_CHAR, procName, 
                          sizeof(procName), &indicator);
                
                if (indicator != SQL_NULL_DATA) {
                    procedures.push_back(std::string((char*)procName));
                }
            }
        }
        
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return procedures;
    }
    
    // Call stored procedure
    std::vector<std::map<std::string, std::string>> callProcedure(
        const std::string& procedureName,
        const std::vector<std::pair<int, std::string>>& params = {}) {
        
        // Build procedure call
        std::string call = "{CALL " + procedureName + "(";
        for (size_t i = 0; i < params.size(); ++i) {
            call += "?";
            if (i < params.size() - 1) call += ",";
        }
        call += ")}";
        
        return executePrepared(call, params);
    }
    
    // Bulk operations
    bool bulkInsert(const std::string& table,
                    const std::vector<std::string>& columns,
                    const std::vector<std::vector<std::string>>& data) {
        
        if (data.empty()) return true;
        
        // Build parameterized INSERT
        std::string query = "INSERT INTO " + table + " (";
        for (size_t i = 0; i < columns.size(); ++i) {
            query += columns[i];
            if (i < columns.size() - 1) query += ", ";
        }
        query += ") VALUES (";
        for (size_t i = 0; i < columns.size(); ++i) {
            query += "?";
            if (i < columns.size() - 1) query += ", ";
        }
        query += ")";
        
        SQLHSTMT hStmt;
        SQLRETURN ret;
        
        ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
        if (!SQL_SUCCEEDED(ret)) return false;
        
        // Prepare statement
        ret = SQLPrepare(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
        if (!SQL_SUCCEEDED(ret)) {
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            return false;
        }
        
        // Set array size for bulk operations
        SQLSetStmtAttr(hStmt, SQL_ATTR_PARAMSET_SIZE, 
                      (SQLPOINTER)data.size(), 0);
        
        // Bind parameters for bulk insert
        std::vector<std::vector<char>> buffers(columns.size());
        std::vector<SQLLEN> indicators(columns.size() * data.size());
        
        for (size_t col = 0; col < columns.size(); ++col) {
            buffers[col].resize(256 * data.size());
            
            for (size_t row = 0; row < data.size(); ++row) {
                const std::string& value = data[row][col];
                size_t offset = row * 256;
                
                strncpy(&buffers[col][offset], value.c_str(), 255);
                buffers[col][offset + 255] = '\0';
                indicators[col * data.size() + row] = SQL_NTS;
            }
            
            SQLBindParameter(hStmt, col + 1, SQL_PARAM_INPUT,
                           SQL_C_CHAR, SQL_VARCHAR, 255, 0,
                           buffers[col].data(), 256,
                           &indicators[col * data.size()]);
        }
        
        // Execute bulk insert
        ret = SQLExecute(hStmt);
        
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return SQL_SUCCEEDED(ret);
    }
    
    // Check if connected
    bool isConnected() const {
        if (!connected || hDbc == SQL_NULL_HDBC) {
            return false;
        }
        
        SQLUINTEGER dead;
        SQLRETURN ret = SQLGetConnectAttr(hDbc, SQL_ATTR_CONNECTION_DEAD, 
                                         &dead, 0, NULL);
        
        return SQL_SUCCEEDED(ret) && (dead == SQL_CD_FALSE);
    }
    
    // Get last error
    std::string getLastError() const {
        return lastError;
    }
    
    // Get database type
    std::string getDatabaseType() const {
        return databaseType;
    }
    
    // Disconnect
    void disconnect() {
        if (hDbc != SQL_NULL_HDBC) {
            SQLDisconnect(hDbc);
            SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
            hDbc = SQL_NULL_HDBC;
        }
        
        if (hEnv != SQL_NULL_HENV) {
            SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
            hEnv = SQL_NULL_HENV;
        }
        
        connected = false;
        std::cout << "Disconnected from ODBC data source" << std::endl;
    }
};

// Usage example
int main() {
    ODBCConnection odbc;
    
    // Example 1: Connect using DSN
    if (odbc.connectDSN("PostgreSQL-DSN", "postgres", "password")) {
        auto results = odbc.executeQuery("SELECT version()");
        
        for (const auto& row : results) {
            for (const auto& [key, value] : row) {
                std::cout << key << ": " << value << std::endl;
            }
        }
        
        odbc.disconnect();
    }
    
    // Example 2: Connect using connection string (MySQL)
    ODBCConnection mysqlOdbc;
    std::string mysqlConnStr = 
        "Driver={MySQL ODBC 8.0 Driver};"
        "Server=localhost;"
        "Port=3306;"
        "Database=testdb;"
        "User=root;"
        "Password=password;"
        "Option=3;";
    
    if (mysqlOdbc.connect(mysqlConnStr)) {
        // Create table
        mysqlOdbc.executeUpdate(
            "CREATE TABLE IF NOT EXISTS users ("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
            "name VARCHAR(100),"
            "email VARCHAR(100)"
            ")"
        );
        
        // Insert data
        mysqlOdbc.executeUpdate(
            "INSERT INTO users (name, email) VALUES "
            "('John Doe', 'john@example.com')"
        );
        
        // Query data
        auto users = mysqlOdbc.executeQuery("SELECT * FROM users");
        
        for (const auto& user : users) {
            std::cout << "User: ";
            for (const auto& [key, value] : user) {
                std::cout << key << "=" << value << " ";
            }
            std::cout << std::endl;
        }
        
        // Get table metadata
        auto columns = mysqlOdbc.getColumns("users");
        std::cout << "Table columns:" << std::endl;
        for (const auto& col : columns) {
            std::cout << "  " << col.at("name") << " " 
                     << col.at("type") << "(" << col.at("size") << ")" << std::endl;
        }
        
        mysqlOdbc.disconnect();
    }
    
    // Example 3: Connect to SQL Server
    ODBCConnection mssqlOdbc;
    std::string mssqlConnStr = 
        "Driver={ODBC Driver 17 for SQL Server};"
        "Server=localhost\\SQLEXPRESS;"
        "Database=testdb;"
        "Trusted_Connection=yes;";
    
    if (mssqlOdbc.connect(mssqlConnStr)) {
        // Transaction example
        mssqlOdbc.beginTransaction();
        
        mssqlOdbc.executeUpdate("UPDATE accounts SET balance = balance - 100 WHERE id = 1");
        mssqlOdbc.executeUpdate("UPDATE accounts SET balance = balance + 100 WHERE id = 2");
        
        mssqlOdbc.commit();
        
        mssqlOdbc.disconnect();
    }
    
    return 0;
}
```

## Compilation

### Linux
```bash
g++ -std=c++17 -o odbc_app odbc_app.cpp \
    -lodbc \
    -I/usr/include \
    -L/usr/lib/x86_64-linux-gnu
```

### Windows
```bash
cl /EHsc odbc_app.cpp odbc32.lib user32.lib
```

### CMake Configuration
```cmake
cmake_minimum_required(VERSION 3.10)
project(ODBCApp)

set(CMAKE_CXX_STANDARD 17)

# Find ODBC
if(WIN32)
    set(ODBC_LIBRARIES odbc32 odbccp32)
else()
    find_package(ODBC REQUIRED)
endif()

add_executable(odbc_app main.cpp)

if(WIN32)
    target_link_libraries(odbc_app ${ODBC_LIBRARIES})
else()
    target_link_libraries(odbc_app ${ODBC_LIBRARIES})
    target_include_directories(odbc_app PRIVATE ${ODBC_INCLUDE_DIRS})
endif()
```

## ODBC SQL State Codes

| SQLSTATE | Description |
|----------|-------------|
| 00000 | Success |
| 01000 | General warning |
| 01004 | String data truncated |
| 07009 | Invalid descriptor index |
| 08001 | Unable to connect to data source |
| 08003 | Connection not open |
| 08004 | Data source rejected connection |
| 08007 | Connection failure during transaction |
| 08S01 | Communication link failure |
| 21S01 | Insert value list does not match column list |
| 22001 | String data right truncation |
| 22003 | Numeric value out of range |
| 22007 | Invalid datetime format |
| 22008 | Datetime field overflow |
| 23000 | Integrity constraint violation |
| 24000 | Invalid cursor state |
| 25000 | Invalid transaction state |
| 28000 | Invalid authorization specification |
| 34000 | Invalid cursor name |
| 3D000 | Invalid catalog name |
| 3F000 | Invalid schema name |
| 40001 | Serialization failure |
| 42000 | Syntax error or access violation |
| 42S01 | Table already exists |
| 42S02 | Table not found |
| 42S22 | Column not found |
| HY000 | General error |
| HY001 | Memory allocation error |
| HY008 | Operation canceled |
| HY009 | Invalid use of null pointer |
| HY010 | Function sequence error |
| HY013 | Memory management error |
| HY090 | Invalid string or buffer length |
| HYC00 | Optional feature not implemented |
| HYT00 | Timeout expired |
| HYT01 | Connection timeout expired |
| IM001 | Driver does not support this function |
| IM002 | Data source not found |

## Performance Optimization

1. **Connection Pooling**: Enable connection pooling in Driver Manager
2. **Prepared Statements**: Use for repeated queries
3. **Bulk Operations**: Use array binding for batch operations
4. **Cursor Types**: Use forward-only cursors when possible
5. **Fetch Size**: Adjust row set size for optimal performance
6. **Async Operations**: Use asynchronous execution for long queries

## Security Considerations

1. **Use DSN-less connections** with encrypted credentials
2. **Enable SSL/TLS** when supported by the driver
3. **Use prepared statements** to prevent SQL injection
4. **Implement connection timeouts**
5. **Use Windows Authentication** when available
6. **Encrypt connection strings** in configuration files
7. **Regularly update ODBC drivers**