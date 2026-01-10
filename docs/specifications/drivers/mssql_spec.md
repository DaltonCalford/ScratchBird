# Microsoft SQL Server C++ Interface Specification

## Overview

**Scope Note:** MSSQL/TDS support is post-gold. This driver spec is retained for future implementation and is not part of the current version.

Microsoft SQL Server can be accessed from C++ applications using multiple approaches: ODBC (cross-platform), FreeTDS (Unix/Linux), and SQL Server Native Client (Windows). This specification covers all approaches with complete implementation details.

## Connection Libraries

### 1. ODBC (Open Database Connectivity) - Cross-Platform

ODBC is the most portable solution for connecting to SQL Server from C++.

#### Installation

##### Linux (Debian/Ubuntu)
```bash
# Install Microsoft ODBC Driver 17 for SQL Server
curl https://packages.microsoft.com/keys/microsoft.asc | apt-key add -
curl https://packages.microsoft.com/config/ubuntu/20.04/prod.list > /etc/apt/sources.list.d/mssql-release.list
apt-get update
ACCEPT_EULA=Y apt-get install -y msodbcsql17
apt-get install -y unixodbc-dev
```

##### Linux (RHEL/CentOS)
```bash
curl https://packages.microsoft.com/config/rhel/8/prod.repo > /etc/yum.repos.d/mssql-release.repo
yum remove unixODBC-utf16 unixODBC-utf16-devel
ACCEPT_EULA=Y yum install -y msodbcsql17
yum install -y unixODBC-devel
```

##### Windows
Download ODBC Driver 17 for SQL Server from:
https://docs.microsoft.com/en-us/sql/connect/odbc/download-odbc-driver-for-sql-server

##### macOS
```bash
brew tap microsoft/mssql-release https://github.com/Microsoft/homebrew-mssql-release
brew update
ACCEPT_EULA=Y brew install msodbcsql17 mssql-tools
```

#### Header Files Required
```cpp
#ifdef _WIN32
    #include <windows.h>
#endif
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <sqlucode.h>
```

### 2. FreeTDS - Unix/Linux Alternative

FreeTDS is an open-source implementation of the TDS protocol used by SQL Server.

#### Installation

##### Linux (Debian/Ubuntu)
```bash
apt-get install freetds-dev freetds-bin tdsodbc
```

##### Linux (RHEL/CentOS)
```bash
yum install freetds freetds-devel
```

##### macOS
```bash
brew install freetds
```

#### Configuration (/etc/freetds/freetds.conf)
```ini
[global]
    tds version = 7.4
    client charset = UTF-8
    
[sqlserver]
    host = your_server.database.windows.net
    port = 1433
    tds version = 7.4
    encryption = required
```

### 3. SQL Server Native Client (Windows Only)

#### Installation
Download SQL Server Native Client from Microsoft Download Center.

#### Header Files Required
```cpp
#include <sqlncli.h>
```

## Connection Parameters

### Connection String Format

#### ODBC Connection String
```
Driver={ODBC Driver 17 for SQL Server};Server=server_name;Database=db_name;UID=username;PWD=password;
```

#### FreeTDS Connection String
```
Driver={FreeTDS};Server=server_name;Port=1433;Database=db_name;UID=username;PWD=password;TDS_Version=7.4;
```

### Parameters Table

| Parameter | Type | Description | Default |
|-----------|------|-------------|---------|
| Driver | string | ODBC driver name | - |
| Server | string | Server name or IP (hostname,port) | - |
| Database | string | Database name | master |
| UID/User | string | Username | - |
| PWD/Password | string | Password | - |
| Port | int | Server port | 1433 |
| Trusted_Connection | yes/no | Use Windows authentication | no |
| Encrypt | yes/no | Use SSL encryption | no |
| TrustServerCertificate | yes/no | Trust server certificate | no |
| Connection Timeout | int | Connection timeout in seconds | 15 |
| Command Timeout | int | Query timeout in seconds | 30 |
| ApplicationName | string | Application name | - |
| WSID | string | Workstation ID | hostname |
| Language | string | SQL Server language | us_english |
| MultiSubnetFailover | yes/no | Enable multi-subnet failover | no |
| ApplicationIntent | ReadWrite/ReadOnly | Connection intent | ReadWrite |
| Authentication | string | Authentication method | SqlPassword |
| ColumnEncryption | Enabled/Disabled | Always Encrypted | Disabled |

## Complete ODBC Implementation

```cpp
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <sstream>
#include <iomanip>
#include <cstring>

#ifdef _WIN32
    #include <windows.h>
#endif

#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>

class MSSQLConnection {
private:
    SQLHENV hEnv;
    SQLHDBC hDbc;
    bool connected;
    
    // Helper function to extract error information
    std::string extractError(SQLHANDLE handle, SQLSMALLINT handleType) {
        SQLCHAR sqlState[6];
        SQLINTEGER nativeError;
        SQLCHAR message[SQL_MAX_MESSAGE_LENGTH];
        SQLSMALLINT messageLength;
        std::stringstream ss;
        
        SQLSMALLINT recNumber = 1;
        while (SQLGetDiagRec(handleType, handle, recNumber, sqlState, &nativeError,
                            message, sizeof(message), &messageLength) == SQL_SUCCESS) {
            ss << "SQLState: " << sqlState << ", ";
            ss << "Native Error: " << nativeError << ", ";
            ss << "Message: " << message << std::endl;
            recNumber++;
        }
        
        return ss.str();
    }
    
    // Convert SQL type to string
    std::string sqlTypeToString(SQLSMALLINT sqlType) {
        switch (sqlType) {
            case SQL_CHAR: return "CHAR";
            case SQL_VARCHAR: return "VARCHAR";
            case SQL_LONGVARCHAR: return "TEXT";
            case SQL_WCHAR: return "NCHAR";
            case SQL_WVARCHAR: return "NVARCHAR";
            case SQL_WLONGVARCHAR: return "NTEXT";
            case SQL_DECIMAL: return "DECIMAL";
            case SQL_NUMERIC: return "NUMERIC";
            case SQL_SMALLINT: return "SMALLINT";
            case SQL_INTEGER: return "INT";
            case SQL_REAL: return "REAL";
            case SQL_FLOAT: return "FLOAT";
            case SQL_DOUBLE: return "DOUBLE";
            case SQL_BIT: return "BIT";
            case SQL_TINYINT: return "TINYINT";
            case SQL_BIGINT: return "BIGINT";
            case SQL_BINARY: return "BINARY";
            case SQL_VARBINARY: return "VARBINARY";
            case SQL_LONGVARBINARY: return "IMAGE";
            case SQL_TYPE_DATE: return "DATE";
            case SQL_TYPE_TIME: return "TIME";
            case SQL_TYPE_TIMESTAMP: return "DATETIME";
            case SQL_GUID: return "UNIQUEIDENTIFIER";
            default: return "UNKNOWN";
        }
    }
    
public:
    MSSQLConnection() : hEnv(SQL_NULL_HENV), hDbc(SQL_NULL_HDBC), connected(false) {}
    
    ~MSSQLConnection() {
        disconnect();
    }
    
    // Initialize ODBC environment
    bool initializeODBC() {
        SQLRETURN ret;
        
        // Allocate environment handle
        ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            std::cerr << "Failed to allocate environment handle" << std::endl;
            return false;
        }
        
        // Set ODBC version to 3.x
        ret = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            std::cerr << "Failed to set ODBC version" << std::endl;
            SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
            hEnv = SQL_NULL_HENV;
            return false;
        }
        
        // Allocate connection handle
        ret = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            std::cerr << "Failed to allocate connection handle" << std::endl;
            SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
            hEnv = SQL_NULL_HENV;
            return false;
        }
        
        return true;
    }
    
    // Connect using connection string
    bool connect(const std::string& connectionString) {
        if (!initializeODBC()) {
            return false;
        }
        
        SQLCHAR outConnStr[1024];
        SQLSMALLINT outConnStrLen;
        
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
        
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            std::cerr << "Connection failed: " << extractError(hDbc, SQL_HANDLE_DBC) << std::endl;
            return false;
        }
        
        connected = true;
        std::cout << "Connected to SQL Server successfully" << std::endl;
        
        // Set connection attributes
        SQLSetConnectAttr(hDbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_ON, 0);
        
        return true;
    }
    
    // Connect with individual parameters
    bool connect(const std::string& server, const std::string& database,
                 const std::string& username, const std::string& password,
                 bool useWindowsAuth = false, bool encrypt = false) {
        
        std::stringstream connStr;
        connStr << "Driver={ODBC Driver 17 for SQL Server};";
        connStr << "Server=" << server << ";";
        connStr << "Database=" << database << ";";
        
        if (useWindowsAuth) {
            connStr << "Trusted_Connection=yes;";
        } else {
            connStr << "UID=" << username << ";";
            connStr << "PWD=" << password << ";";
        }
        
        if (encrypt) {
            connStr << "Encrypt=yes;TrustServerCertificate=yes;";
        }
        
        return connect(connStr.str());
    }
    
    // Execute query and return results
    std::vector<std::map<std::string, std::string>> executeQuery(const std::string& query) {
        std::vector<std::map<std::string, std::string>> results;
        
        if (!connected) {
            std::cerr << "Not connected to database" << std::endl;
            return results;
        }
        
        SQLHSTMT hStmt;
        SQLRETURN ret;
        
        // Allocate statement handle
        ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            std::cerr << "Failed to allocate statement handle" << std::endl;
            return results;
        }
        
        // Execute query
        ret = SQLExecDirect(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            std::cerr << "Query execution failed: " << extractError(hStmt, SQL_HANDLE_STMT) << std::endl;
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            return results;
        }
        
        // Get column count
        SQLSMALLINT columnCount;
        SQLNumResultCols(hStmt, &columnCount);
        
        // Get column metadata
        std::vector<std::string> columnNames;
        std::vector<SQLSMALLINT> columnTypes;
        
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
            columnTypes.push_back(dataType);
        }
        
        // Fetch rows
        while (SQLFetch(hStmt) == SQL_SUCCESS) {
            std::map<std::string, std::string> row;
            
            for (SQLSMALLINT i = 1; i <= columnCount; ++i) {
                SQLCHAR buffer[4096];
                SQLLEN indicator;
                
                ret = SQLGetData(hStmt, i, SQL_C_CHAR, buffer, sizeof(buffer), &indicator);
                
                if (indicator == SQL_NULL_DATA) {
                    row[columnNames[i-1]] = "NULL";
                } else if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
                    row[columnNames[i-1]] = std::string((char*)buffer);
                } else {
                    row[columnNames[i-1]] = "ERROR";
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
        const std::vector<std::string>& params) {
        
        std::vector<std::map<std::string, std::string>> results;
        
        if (!connected) {
            std::cerr << "Not connected to database" << std::endl;
            return results;
        }
        
        SQLHSTMT hStmt;
        SQLRETURN ret;
        
        // Allocate statement handle
        ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            std::cerr << "Failed to allocate statement handle" << std::endl;
            return results;
        }
        
        // Prepare statement
        ret = SQLPrepare(hStmt, (SQLCHAR*)query.c_str(), SQL_NTS);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            std::cerr << "Failed to prepare statement: " << extractError(hStmt, SQL_HANDLE_STMT) << std::endl;
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            return results;
        }
        
        // Bind parameters
        std::vector<SQLLEN> indicators(params.size());
        for (size_t i = 0; i < params.size(); ++i) {
            indicators[i] = SQL_NTS;
            ret = SQLBindParameter(hStmt, i + 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                                  params[i].length(), 0, (SQLPOINTER)params[i].c_str(),
                                  params[i].length(), &indicators[i]);
            
            if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
                std::cerr << "Failed to bind parameter " << (i + 1) << std::endl;
                SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
                return results;
            }
        }
        
        // Execute prepared statement
        ret = SQLExecute(hStmt);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            std::cerr << "Failed to execute prepared statement: " << extractError(hStmt, SQL_HANDLE_STMT) << std::endl;
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            return results;
        }
        
        // Get results (same as executeQuery)
        SQLSMALLINT columnCount;
        SQLNumResultCols(hStmt, &columnCount);
        
        if (columnCount > 0) {
            // Get column metadata
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
                
                for (SQLSMALLINT i = 1; i <= columnCount; ++i) {
                    SQLCHAR buffer[4096];
                    SQLLEN indicator;
                    
                    ret = SQLGetData(hStmt, i, SQL_C_CHAR, buffer, sizeof(buffer), &indicator);
                    
                    if (indicator == SQL_NULL_DATA) {
                        row[columnNames[i-1]] = "NULL";
                    } else if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
                        row[columnNames[i-1]] = std::string((char*)buffer);
                    }
                }
                
                results.push_back(row);
            }
        }
        
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return results;
    }
    
    // Execute command (INSERT, UPDATE, DELETE)
    int executeCommand(const std::string& command) {
        if (!connected) {
            std::cerr << "Not connected to database" << std::endl;
            return -1;
        }
        
        SQLHSTMT hStmt;
        SQLRETURN ret;
        
        // Allocate statement handle
        ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            std::cerr << "Failed to allocate statement handle" << std::endl;
            return -1;
        }
        
        // Execute command
        ret = SQLExecDirect(hStmt, (SQLCHAR*)command.c_str(), SQL_NTS);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            std::cerr << "Command execution failed: " << extractError(hStmt, SQL_HANDLE_STMT) << std::endl;
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
        return (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO);
    }
    
    bool commit() {
        SQLRETURN ret = SQLEndTran(SQL_HANDLE_DBC, hDbc, SQL_COMMIT);
        if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
            // Re-enable autocommit
            SQLSetConnectAttr(hDbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_ON, 0);
            return true;
        }
        return false;
    }
    
    bool rollback() {
        SQLRETURN ret = SQLEndTran(SQL_HANDLE_DBC, hDbc, SQL_ROLLBACK);
        if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
            // Re-enable autocommit
            SQLSetConnectAttr(hDbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_ON, 0);
            return true;
        }
        return false;
    }
    
    // Stored procedure call
    std::vector<std::map<std::string, std::string>> callProcedure(
        const std::string& procedureName,
        const std::vector<std::string>& params,
        std::map<std::string, std::string>& outputParams) {
        
        std::vector<std::map<std::string, std::string>> results;
        
        if (!connected) {
            std::cerr << "Not connected to database" << std::endl;
            return results;
        }
        
        // Build procedure call
        std::string call = "{CALL " + procedureName + "(";
        for (size_t i = 0; i < params.size(); ++i) {
            call += "?";
            if (i < params.size() - 1) call += ",";
        }
        call += ")}";
        
        SQLHSTMT hStmt;
        SQLRETURN ret;
        
        // Allocate statement handle
        ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            return results;
        }
        
        // Prepare call
        ret = SQLPrepare(hStmt, (SQLCHAR*)call.c_str(), SQL_NTS);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            return results;
        }
        
        // Bind parameters
        std::vector<SQLLEN> indicators(params.size());
        for (size_t i = 0; i < params.size(); ++i) {
            indicators[i] = SQL_NTS;
            SQLBindParameter(hStmt, i + 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           params[i].length(), 0, (SQLPOINTER)params[i].c_str(),
                           params[i].length(), &indicators[i]);
        }
        
        // Execute
        ret = SQLExecute(hStmt);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            std::cerr << "Failed to execute procedure: " << extractError(hStmt, SQL_HANDLE_STMT) << std::endl;
            SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            return results;
        }
        
        // Process result sets
        do {
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
                    
                    for (SQLSMALLINT i = 1; i <= columnCount; ++i) {
                        SQLCHAR buffer[4096];
                        SQLLEN indicator;
                        
                        SQLGetData(hStmt, i, SQL_C_CHAR, buffer, sizeof(buffer), &indicator);
                        
                        if (indicator == SQL_NULL_DATA) {
                            row[columnNames[i-1]] = "NULL";
                        } else {
                            row[columnNames[i-1]] = std::string((char*)buffer);
                        }
                    }
                    
                    results.push_back(row);
                }
            }
        } while (SQLMoreResults(hStmt) == SQL_SUCCESS);
        
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return results;
    }
    
    // Bulk insert using table-valued parameters
    bool bulkInsert(const std::string& tableName,
                    const std::vector<std::string>& columns,
                    const std::vector<std::vector<std::string>>& data) {
        
        if (data.empty()) return true;
        
        // Build INSERT statement with multiple value sets
        std::stringstream ss;
        ss << "INSERT INTO " << tableName << " (";
        for (size_t i = 0; i < columns.size(); ++i) {
            ss << columns[i];
            if (i < columns.size() - 1) ss << ", ";
        }
        ss << ") VALUES ";
        
        for (size_t i = 0; i < data.size(); ++i) {
            ss << "(";
            for (size_t j = 0; j < data[i].size(); ++j) {
                ss << "'" << data[i][j] << "'";
                if (j < data[i].size() - 1) ss << ", ";
            }
            ss << ")";
            if (i < data.size() - 1) ss << ", ";
        }
        
        return executeCommand(ss.str()) >= 0;
    }
    
    // Get table metadata
    std::vector<std::map<std::string, std::string>> getTableColumns(const std::string& tableName) {
        std::vector<std::map<std::string, std::string>> columns;
        
        if (!connected) {
            return columns;
        }
        
        SQLHSTMT hStmt;
        SQLRETURN ret;
        
        ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            return columns;
        }
        
        // Get columns for the table
        ret = SQLColumns(hStmt, NULL, 0, NULL, 0, 
                        (SQLCHAR*)tableName.c_str(), SQL_NTS, NULL, 0);
        
        if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
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
                
                // Default value (column 13)
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
    
    // Get list of tables
    std::vector<std::string> getTables() {
        std::vector<std::string> tables;
        
        if (!connected) {
            return tables;
        }
        
        SQLHSTMT hStmt;
        SQLRETURN ret;
        
        ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
        if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
            return tables;
        }
        
        // Get all tables
        ret = SQLTables(hStmt, NULL, 0, NULL, 0, NULL, 0, (SQLCHAR*)"TABLE", SQL_NTS);
        
        if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
            while (SQLFetch(hStmt) == SQL_SUCCESS) {
                SQLCHAR tableName[256];
                SQLLEN indicator;
                
                // Table name is in column 3
                SQLGetData(hStmt, 3, SQL_C_CHAR, tableName, sizeof(tableName), &indicator);
                tables.push_back(std::string((char*)tableName));
            }
        }
        
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return tables;
    }
    
    // Get last identity value
    std::string getLastIdentity() {
        auto result = executeQuery("SELECT SCOPE_IDENTITY() AS LastID");
        if (!result.empty() && result[0].find("LastID") != result[0].end()) {
            return result[0]["LastID"];
        }
        return "0";
    }
    
    // Check connection status
    bool isConnected() {
        if (!connected || hDbc == SQL_NULL_HDBC) {
            return false;
        }
        
        SQLINTEGER dead;
        SQLRETURN ret = SQLGetConnectAttr(hDbc, SQL_ATTR_CONNECTION_DEAD, 
                                          &dead, 0, NULL);
        
        return (ret == SQL_SUCCESS && dead == SQL_CD_FALSE);
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
        std::cout << "Disconnected from SQL Server" << std::endl;
    }
};

// Usage example
int main() {
    MSSQLConnection db;
    
    // Connect to SQL Server
    if (db.connect("localhost\\SQLEXPRESS", "testdb", "sa", "password", false, true)) {
        
        // Create table
        db.executeCommand(
            "IF NOT EXISTS (SELECT * FROM sysobjects WHERE name='users' AND xtype='U') "
            "CREATE TABLE users ("
            "id INT IDENTITY(1,1) PRIMARY KEY,"
            "name NVARCHAR(100) NOT NULL,"
            "email NVARCHAR(100) UNIQUE,"
            "age INT,"
            "created_at DATETIME DEFAULT GETDATE()"
            ")"
        );
        
        // Insert data with prepared statement
        std::vector<std::string> params = {"John Doe", "john@example.com", "30"};
        db.executePrepared(
            "INSERT INTO users (name, email, age) VALUES (?, ?, ?)",
            params
        );
        
        // Get last inserted ID
        std::cout << "Last inserted ID: " << db.getLastIdentity() << std::endl;
        
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
        db.executeCommand("UPDATE users SET age = age + 1 WHERE id = 1");
        db.executeCommand("INSERT INTO users (name, email, age) VALUES ('Jane Doe', 'jane@example.com', 25)");
        db.commit();
        
        // Call stored procedure
        std::map<std::string, std::string> outputParams;
        auto procResults = db.callProcedure("sp_GetUserById", {"1"}, outputParams);
        
        // Bulk insert
        std::vector<std::vector<std::string>> bulkData = {
            {"Alice", "alice@example.com", "28"},
            {"Bob", "bob@example.com", "35"},
            {"Charlie", "charlie@example.com", "42"}
        };
        db.bulkInsert("users", {"name", "email", "age"}, bulkData);
        
        // Get table metadata
        auto tables = db.getTables();
        for (const auto& table : tables) {
            std::cout << "Table: " << table << std::endl;
            
            auto columns = db.getTableColumns(table);
            for (const auto& col : columns) {
                std::cout << "  Column: " << col["name"] 
                         << " Type: " << col["type"]
                         << " Size: " << col["size"] << std::endl;
            }
        }
        
        // Disconnect
        db.disconnect();
    }
    
    return 0;
}
```

## FreeTDS Implementation

```cpp
#include <sybfront.h>
#include <sybdb.h>
#include <string>
#include <vector>
#include <map>
#include <iostream>

class FreeTDSConnection {
private:
    DBPROCESS* dbproc;
    LOGINREC* login;
    
    static int err_handler(DBPROCESS* dbproc, int severity, int dberr, 
                          int oserr, char* dberrstr, char* oserrstr) {
        if (dberr) {
            std::cerr << "DB-Library error " << dberr << ": " << dberrstr << std::endl;
        }
        if (oserr) {
            std::cerr << "Operating system error " << oserr << ": " << oserrstr << std::endl;
        }
        return INT_CANCEL;
    }
    
    static int msg_handler(DBPROCESS* dbproc, DBINT msgno, int msgstate, 
                          int severity, char* msgtext, char* srvname, 
                          char* procname, int line) {
        std::cerr << "SQL Server message " << msgno << ", severity " << severity 
                  << ", state " << msgstate << std::endl;
        if (msgtext) {
            std::cerr << "Message: " << msgtext << std::endl;
        }
        return 0;
    }
    
public:
    FreeTDSConnection() : dbproc(nullptr), login(nullptr) {
        // Initialize DB-Library
        if (dbinit() == FAIL) {
            std::cerr << "Failed to initialize DB-Library" << std::endl;
        }
        
        // Install error handlers
        dberrhandle(err_handler);
        dbmsghandle(msg_handler);
    }
    
    ~FreeTDSConnection() {
        disconnect();
        dbexit();
    }
    
    bool connect(const std::string& server, const std::string& username,
                 const std::string& password, const std::string& database) {
        
        // Get login record
        login = dblogin();
        if (!login) {
            std::cerr << "Failed to allocate login structure" << std::endl;
            return false;
        }
        
        // Set login parameters
        DBSETLUSER(login, username.c_str());
        DBSETLPWD(login, password.c_str());
        DBSETLAPP(login, "FreeTDS App");
        
        // Set timeout
        dbsetlogintime(10);  // 10 seconds
        
        // Connect to server
        dbproc = dbopen(login, server.c_str());
        if (!dbproc) {
            std::cerr << "Failed to connect to server" << std::endl;
            dbloginfree(login);
            login = nullptr;
            return false;
        }
        
        // Select database
        if (!database.empty()) {
            if (dbuse(dbproc, database.c_str()) == FAIL) {
                std::cerr << "Failed to select database: " << database << std::endl;
                dbclose(dbproc);
                dbloginfree(login);
                dbproc = nullptr;
                login = nullptr;
                return false;
            }
        }
        
        std::cout << "Connected via FreeTDS successfully" << std::endl;
        return true;
    }
    
    std::vector<std::map<std::string, std::string>> executeQuery(const std::string& query) {
        std::vector<std::map<std::string, std::string>> results;
        
        if (!dbproc) {
            std::cerr << "Not connected" << std::endl;
            return results;
        }
        
        // Send query
        if (dbcmd(dbproc, query.c_str()) == FAIL) {
            std::cerr << "Failed to set command" << std::endl;
            return results;
        }
        
        if (dbsqlexec(dbproc) == FAIL) {
            std::cerr << "Failed to execute query" << std::endl;
            return results;
        }
        
        // Process results
        RETCODE erc;
        while ((erc = dbresults(dbproc)) != NO_MORE_RESULTS) {
            if (erc == FAIL) {
                std::cerr << "Failed to get results" << std::endl;
                break;
            }
            
            int numcols = dbnumcols(dbproc);
            
            // Get column names
            std::vector<std::string> columnNames;
            for (int i = 1; i <= numcols; ++i) {
                char* colname = dbcolname(dbproc, i);
                columnNames.push_back(colname ? colname : "");
            }
            
            // Fetch rows
            while (dbnextrow(dbproc) != NO_MORE_ROWS) {
                std::map<std::string, std::string> row;
                
                for (int i = 1; i <= numcols; ++i) {
                    BYTE* data = dbdata(dbproc, i);
                    int len = dbdatlen(dbproc, i);
                    
                    if (data && len > 0) {
                        std::string value((char*)data, len);
                        row[columnNames[i-1]] = value;
                    } else {
                        row[columnNames[i-1]] = "NULL";
                    }
                }
                
                results.push_back(row);
            }
        }
        
        return results;
    }
    
    void disconnect() {
        if (dbproc) {
            dbclose(dbproc);
            dbproc = nullptr;
        }
        if (login) {
            dbloginfree(login);
            login = nullptr;
        }
    }
};
```

## Compilation

### Using ODBC on Linux
```bash
g++ -std=c++17 -o mssql_app mssql_app.cpp \
    -lodbc \
    -I/usr/include \
    -L/usr/lib/x86_64-linux-gnu
```

### Using ODBC on Windows
```bash
cl /EHsc mssql_app.cpp odbc32.lib user32.lib
```

### Using FreeTDS
```bash
g++ -std=c++17 -o mssql_app mssql_app.cpp \
    -lsybdb \
    -I/usr/include/freetds \
    -L/usr/lib/x86_64-linux-gnu
```

### CMake Configuration
```cmake
cmake_minimum_required(VERSION 3.10)
project(MSSQLApp)

set(CMAKE_CXX_STANDARD 17)

# For ODBC
find_package(ODBC REQUIRED)

add_executable(mssql_app main.cpp)

if(WIN32)
    target_link_libraries(mssql_app odbc32)
else()
    target_link_libraries(mssql_app odbc)
endif()

# For FreeTDS (Linux/Unix only)
if(UNIX)
    find_library(SYBDB_LIB sybdb PATHS /usr/lib /usr/local/lib)
    if(SYBDB_LIB)
        target_link_libraries(mssql_app ${SYBDB_LIB})
        target_include_directories(mssql_app PRIVATE /usr/include/freetds)
    endif()
endif()
```

## SQL Server Specific Features

### 1. Table-Valued Parameters
```cpp
// Create table type
db.executeCommand("CREATE TYPE dbo.UserTableType AS TABLE (Name NVARCHAR(100), Age INT)");

// Use in stored procedure
db.executeCommand(
    "CREATE PROCEDURE InsertUsers @Users dbo.UserTableType READONLY AS "
    "INSERT INTO users (name, age) SELECT Name, Age FROM @Users"
);
```

### 2. JSON Support
```cpp
// Query JSON data
auto result = db.executeQuery(
    "SELECT id, name, age "
    "FROM users "
    "FOR JSON PATH"
);

// Parse JSON column
auto jsonData = db.executeQuery(
    "SELECT JSON_VALUE(data, '$.name') AS name "
    "FROM json_table"
);
```

### 3. Temporal Tables
```cpp
// Create temporal table
db.executeCommand(
    "CREATE TABLE users_history ("
    "id INT NOT NULL PRIMARY KEY,"
    "name NVARCHAR(100),"
    "ValidFrom DATETIME2 GENERATED ALWAYS AS ROW START,"
    "ValidTo DATETIME2 GENERATED ALWAYS AS ROW END,"
    "PERIOD FOR SYSTEM_TIME (ValidFrom, ValidTo)"
    ") WITH (SYSTEM_VERSIONING = ON)"
);

// Query historical data
auto history = db.executeQuery(
    "SELECT * FROM users_history "
    "FOR SYSTEM_TIME AS OF '2023-01-01'"
);
```

### 4. Always Encrypted
```cpp
// Connection string with Always Encrypted
std::string connStr = 
    "Driver={ODBC Driver 17 for SQL Server};"
    "Server=localhost;"
    "Database=testdb;"
    "ColumnEncryption=Enabled;"
    "KeyStoreAuthentication=KeyVaultPassword;"
    "KeyStorePrincipalId=myuser;"
    "KeyStoreSecret=mypassword;";
```

## Error Handling

### ODBC SQLSTATE Codes

| SQLSTATE | Description |
|----------|-------------|
| 00000 | Success |
| 01000 | General warning |
| 01004 | String data truncated |
| 08001 | Unable to connect to data source |
| 08003 | Connection not open |
| 08004 | Data source rejected connection |
| 08007 | Connection failure during transaction |
| 08S01 | Communication link failure |
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
| HYT00 | Timeout expired |
| HYT01 | Connection timeout expired |
| IM001 | Driver does not support this function |
| IM002 | Data source not found |

## Performance Optimization

### 1. Connection Pooling
```cpp
// Set connection pooling attributes
SQLSetEnvAttr(hEnv, SQL_ATTR_CONNECTION_POOLING, 
             (SQLPOINTER)SQL_CP_ONE_PER_DRIVER, 0);
SQLSetEnvAttr(hEnv, SQL_ATTR_CP_MATCH, 
             (SQLPOINTER)SQL_CP_RELAXED_MATCH, 0);
```

### 2. Bulk Operations
```cpp
// Use bulk row operations
SQLSetStmtAttr(hStmt, SQL_ATTR_ROW_ARRAY_SIZE, (SQLPOINTER)100, 0);
SQLSetStmtAttr(hStmt, SQL_ATTR_ROW_STATUS_PTR, RowStatusArray, 0);
SQLBulkOperations(hStmt, SQL_ADD);
```

### 3. Asynchronous Execution
```cpp
// Enable async execution
SQLSetStmtAttr(hStmt, SQL_ATTR_ASYNC_ENABLE, 
              (SQLPOINTER)SQL_ASYNC_ENABLE_ON, 0);

// Execute asynchronously
SQLRETURN ret = SQLExecDirect(hStmt, (SQLCHAR*)query, SQL_NTS);
while (ret == SQL_STILL_EXECUTING) {
    ret = SQLExecDirect(hStmt, (SQLCHAR*)query, SQL_NTS);
    // Do other work here
}
```

## Security Best Practices

1. **Use Windows Authentication** when possible
2. **Enable SSL/TLS encryption** for all connections
3. **Use parameterized queries** to prevent SQL injection
4. **Implement connection string encryption**
5. **Use Always Encrypted** for sensitive data
6. **Enable auditing** for compliance
7. **Implement row-level security**
8. **Regular security updates** for drivers and SQL Server
