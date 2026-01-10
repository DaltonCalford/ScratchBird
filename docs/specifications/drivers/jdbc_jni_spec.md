# JDBC/JNI C++ Interface Specification

## Overview

JDBC (Java Database Connectivity) is primarily a Java API. To use JDBC from C++, we need to use JNI (Java Native Interface) to bridge between C++ and Java. This specification covers how to integrate JDBC functionality into C++ applications.

**Scope Note:** SQL Server JDBC references here are for external connectivity; MSSQL/TDS server emulation remains post-gold.

## Architecture

```
┌─────────────────────────────────────────┐
│         C++ Application                 │
├─────────────────────────────────────────┤
│         JNI Layer (C++)                 │
├─────────────────────────────────────────┤
│         JVM (Java Virtual Machine)      │
├─────────────────────────────────────────┤
│         JDBC API (Java)                 │
├─────────────────────────────────────────┤
│      JDBC Drivers (Java)                │
│  ┌──────┬──────┬──────────┬─────────┐ │
│  │MySQL │PostG │  Oracle  │   DB2   │ │
│  │Driver│reSQL │  Driver  │ Driver  │ │
│  └──────┴──────┴──────────┴─────────┘ │
├─────────────────────────────────────────┤
│        Database Systems                 │
└─────────────────────────────────────────┘
```

## Prerequisites

### 1. Java Development Kit (JDK)

#### Linux Installation
```bash
sudo apt-get install default-jdk
# or
sudo apt-get install openjdk-11-jdk
```

#### Windows Installation
Download from: https://www.oracle.com/java/technologies/downloads/

#### macOS Installation
```bash
brew install openjdk
```

### 2. JDBC Drivers

Download JDBC drivers (JAR files) for your databases:
- MySQL: mysql-connector-java-8.x.x.jar
- PostgreSQL: postgresql-42.x.x.jar
- Oracle: ojdbc8.jar
- SQL Server: mssql-jdbc-x.x.x.jar
- SQLite: sqlite-jdbc-x.x.x.jar

### 3. Environment Setup

```bash
# Set JAVA_HOME
export JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64

# Set CLASSPATH for JDBC drivers
export CLASSPATH=/path/to/mysql-connector-java.jar:/path/to/postgresql.jar:$CLASSPATH

# Add JVM library path
export LD_LIBRARY_PATH=$JAVA_HOME/lib/server:$LD_LIBRARY_PATH
```

## Header Files Required

```cpp
#include <jni.h>
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <memory>
```

## Complete JNI/JDBC Implementation

```cpp
#include <jni.h>
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <cstring>

class JDBCConnection {
private:
    JavaVM* jvm;
    JNIEnv* env;
    jobject connection;
    bool connected;
    
    // Java class references
    jclass classConnection;
    jclass classDriverManager;
    jclass classStatement;
    jclass classResultSet;
    jclass classResultSetMetaData;
    jclass classPreparedStatement;
    jclass classSQLException;
    
    // Initialize JVM
    bool initializeJVM(const std::string& classpath = "") {
        JavaVMInitArgs vm_args;
        JavaVMOption options[3];
        
        // Set classpath
        std::string classpathOption = "-Djava.class.path=";
        if (!classpath.empty()) {
            classpathOption += classpath;
        } else {
            // Default classpath with common JDBC drivers
            classpathOption += ".:/usr/share/java/mysql-connector-java.jar:"
                              "/usr/share/java/postgresql.jar";
        }
        
        options[0].optionString = const_cast<char*>(classpathOption.c_str());
        options[1].optionString = const_cast<char*>("-Xmx512m");
        options[2].optionString = const_cast<char*>("-Xms256m");
        
        vm_args.version = JNI_VERSION_1_8;
        vm_args.nOptions = 3;
        vm_args.options = options;
        vm_args.ignoreUnrecognized = JNI_FALSE;
        
        // Create JVM
        jint result = JNI_CreateJavaVM(&jvm, (void**)&env, &vm_args);
        
        if (result != JNI_OK) {
            std::cerr << "Failed to create JVM: " << result << std::endl;
            return false;
        }
        
        // Load Java classes
        loadJavaClasses();
        
        return true;
    }
    
    // Load required Java classes
    void loadJavaClasses() {
        classDriverManager = env->FindClass("java/sql/DriverManager");
        classConnection = env->FindClass("java/sql/Connection");
        classStatement = env->FindClass("java/sql/Statement");
        classResultSet = env->FindClass("java/sql/ResultSet");
        classResultSetMetaData = env->FindClass("java/sql/ResultSetMetaData");
        classPreparedStatement = env->FindClass("java/sql/PreparedStatement");
        classSQLException = env->FindClass("java/sql/SQLException");
        
        // Make global references
        classDriverManager = (jclass)env->NewGlobalRef(classDriverManager);
        classConnection = (jclass)env->NewGlobalRef(classConnection);
        classStatement = (jclass)env->NewGlobalRef(classStatement);
        classResultSet = (jclass)env->NewGlobalRef(classResultSet);
        classResultSetMetaData = (jclass)env->NewGlobalRef(classResultSetMetaData);
        classPreparedStatement = (jclass)env->NewGlobalRef(classPreparedStatement);
        classSQLException = (jclass)env->NewGlobalRef(classSQLException);
    }
    
    // Check and handle Java exceptions
    bool checkException() {
        if (env->ExceptionCheck()) {
            jthrowable exception = env->ExceptionOccurred();
            env->ExceptionClear();
            
            // Get exception message
            jclass throwableClass = env->FindClass("java/lang/Throwable");
            jmethodID getMessage = env->GetMethodID(throwableClass, "getMessage", 
                                                   "()Ljava/lang/String;");
            
            jstring message = (jstring)env->CallObjectMethod(exception, getMessage);
            
            if (message) {
                const char* msgStr = env->GetStringUTFChars(message, nullptr);
                std::cerr << "Java Exception: " << msgStr << std::endl;
                env->ReleaseStringUTFChars(message, msgStr);
            }
            
            env->DeleteLocalRef(exception);
            return true;
        }
        return false;
    }
    
    // Convert Java string to C++ string
    std::string jstringToString(jstring jstr) {
        if (!jstr) return "";
        
        const char* str = env->GetStringUTFChars(jstr, nullptr);
        std::string result(str);
        env->ReleaseStringUTFChars(jstr, str);
        return result;
    }
    
public:
    JDBCConnection() : jvm(nullptr), env(nullptr), connection(nullptr), connected(false) {}
    
    ~JDBCConnection() {
        disconnect();
        if (jvm) {
            jvm->DestroyJavaVM();
        }
    }
    
    // Initialize JDBC with classpath
    bool initialize(const std::string& classpath = "") {
        return initializeJVM(classpath);
    }
    
    // Load JDBC driver
    bool loadDriver(const std::string& driverClass) {
        if (!env) {
            std::cerr << "JVM not initialized" << std::endl;
            return false;
        }
        
        // Load driver class
        jclass driverClazz = env->FindClass(driverClass.c_str());
        if (!driverClazz || checkException()) {
            std::cerr << "Failed to load driver class: " << driverClass << std::endl;
            return false;
        }
        
        // Get Class.forName method
        jclass classClass = env->FindClass("java/lang/Class");
        jmethodID forName = env->GetStaticMethodID(classClass, "forName",
                                                   "(Ljava/lang/String;)Ljava/lang/Class;");
        
        jstring driverName = env->NewStringUTF(driverClass.c_str());
        jobject driver = env->CallStaticObjectMethod(classClass, forName, driverName);
        
        if (checkException()) {
            std::cerr << "Failed to instantiate driver" << std::endl;
            return false;
        }
        
        env->DeleteLocalRef(driverName);
        std::cout << "Loaded JDBC driver: " << driverClass << std::endl;
        return true;
    }
    
    // Connect to database
    bool connect(const std::string& url, const std::string& username, 
                 const std::string& password) {
        
        if (!env) {
            std::cerr << "JVM not initialized" << std::endl;
            return false;
        }
        
        // Get DriverManager.getConnection method
        jmethodID getConnection = env->GetStaticMethodID(
            classDriverManager, "getConnection",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/sql/Connection;"
        );
        
        if (!getConnection || checkException()) {
            std::cerr << "Failed to get getConnection method" << std::endl;
            return false;
        }
        
        // Create Java strings
        jstring jurl = env->NewStringUTF(url.c_str());
        jstring juser = env->NewStringUTF(username.c_str());
        jstring jpass = env->NewStringUTF(password.c_str());
        
        // Get connection
        connection = env->CallStaticObjectMethod(classDriverManager, getConnection,
                                                 jurl, juser, jpass);
        
        env->DeleteLocalRef(jurl);
        env->DeleteLocalRef(juser);
        env->DeleteLocalRef(jpass);
        
        if (!connection || checkException()) {
            std::cerr << "Failed to establish connection" << std::endl;
            return false;
        }
        
        // Make global reference
        connection = env->NewGlobalRef(connection);
        connected = true;
        
        std::cout << "Connected via JDBC successfully" << std::endl;
        return true;
    }
    
    // Execute query
    std::vector<std::map<std::string, std::string>> executeQuery(const std::string& query) {
        std::vector<std::map<std::string, std::string>> results;
        
        if (!connected) {
            std::cerr << "Not connected to database" << std::endl;
            return results;
        }
        
        // Create statement
        jmethodID createStatement = env->GetMethodID(classConnection, "createStatement",
                                                     "()Ljava/sql/Statement;");
        jobject statement = env->CallObjectMethod(connection, createStatement);
        
        if (!statement || checkException()) {
            std::cerr << "Failed to create statement" << std::endl;
            return results;
        }
        
        // Execute query
        jmethodID executeQuery = env->GetMethodID(classStatement, "executeQuery",
                                                  "(Ljava/lang/String;)Ljava/sql/ResultSet;");
        jstring jquery = env->NewStringUTF(query.c_str());
        jobject resultSet = env->CallObjectMethod(statement, executeQuery, jquery);
        
        env->DeleteLocalRef(jquery);
        
        if (!resultSet || checkException()) {
            std::cerr << "Failed to execute query" << std::endl;
            env->DeleteLocalRef(statement);
            return results;
        }
        
        // Get metadata
        jmethodID getMetaData = env->GetMethodID(classResultSet, "getMetaData",
                                                 "()Ljava/sql/ResultSetMetaData;");
        jobject metaData = env->CallObjectMethod(resultSet, getMetaData);
        
        // Get column count
        jmethodID getColumnCount = env->GetMethodID(classResultSetMetaData, 
                                                    "getColumnCount", "()I");
        jint columnCount = env->CallIntMethod(metaData, getColumnCount);
        
        // Get column names
        jmethodID getColumnName = env->GetMethodID(classResultSetMetaData,
                                                   "getColumnName", "(I)Ljava/lang/String;");
        std::vector<std::string> columnNames;
        
        for (int i = 1; i <= columnCount; ++i) {
            jstring colName = (jstring)env->CallObjectMethod(metaData, getColumnName, i);
            columnNames.push_back(jstringToString(colName));
            env->DeleteLocalRef(colName);
        }
        
        // Fetch rows
        jmethodID next = env->GetMethodID(classResultSet, "next", "()Z");
        jmethodID getString = env->GetMethodID(classResultSet, "getString",
                                               "(I)Ljava/lang/String;");
        jmethodID wasNull = env->GetMethodID(classResultSet, "wasNull", "()Z");
        
        while (env->CallBooleanMethod(resultSet, next)) {
            std::map<std::string, std::string> row;
            
            for (int i = 1; i <= columnCount; ++i) {
                jstring value = (jstring)env->CallObjectMethod(resultSet, getString, i);
                
                if (env->CallBooleanMethod(resultSet, wasNull)) {
                    row[columnNames[i-1]] = "NULL";
                } else {
                    row[columnNames[i-1]] = jstringToString(value);
                }
                
                if (value) env->DeleteLocalRef(value);
            }
            
            results.push_back(row);
        }
        
        // Close resources
        jmethodID close = env->GetMethodID(classResultSet, "close", "()V");
        env->CallVoidMethod(resultSet, close);
        
        close = env->GetMethodID(classStatement, "close", "()V");
        env->CallVoidMethod(statement, close);
        
        env->DeleteLocalRef(resultSet);
        env->DeleteLocalRef(metaData);
        env->DeleteLocalRef(statement);
        
        return results;
    }
    
    // Execute update (INSERT, UPDATE, DELETE)
    int executeUpdate(const std::string& query) {
        if (!connected) {
            std::cerr << "Not connected to database" << std::endl;
            return -1;
        }
        
        // Create statement
        jmethodID createStatement = env->GetMethodID(classConnection, "createStatement",
                                                     "()Ljava/sql/Statement;");
        jobject statement = env->CallObjectMethod(connection, createStatement);
        
        if (!statement || checkException()) {
            std::cerr << "Failed to create statement" << std::endl;
            return -1;
        }
        
        // Execute update
        jmethodID executeUpdate = env->GetMethodID(classStatement, "executeUpdate",
                                                   "(Ljava/lang/String;)I");
        jstring jquery = env->NewStringUTF(query.c_str());
        jint rowsAffected = env->CallIntMethod(statement, executeUpdate, jquery);
        
        env->DeleteLocalRef(jquery);
        
        if (checkException()) {
            std::cerr << "Failed to execute update" << std::endl;
            env->DeleteLocalRef(statement);
            return -1;
        }
        
        // Close statement
        jmethodID close = env->GetMethodID(classStatement, "close", "()V");
        env->CallVoidMethod(statement, close);
        env->DeleteLocalRef(statement);
        
        return rowsAffected;
    }
    
    // Execute prepared statement
    std::vector<std::map<std::string, std::string>> executePreparedQuery(
        const std::string& query,
        const std::vector<std::string>& params) {
        
        std::vector<std::map<std::string, std::string>> results;
        
        if (!connected) {
            std::cerr << "Not connected to database" << std::endl;
            return results;
        }
        
        // Prepare statement
        jmethodID prepareStatement = env->GetMethodID(classConnection, "prepareStatement",
                                                      "(Ljava/lang/String;)Ljava/sql/PreparedStatement;");
        jstring jquery = env->NewStringUTF(query.c_str());
        jobject pstmt = env->CallObjectMethod(connection, prepareStatement, jquery);
        
        env->DeleteLocalRef(jquery);
        
        if (!pstmt || checkException()) {
            std::cerr << "Failed to prepare statement" << std::endl;
            return results;
        }
        
        // Set parameters
        jmethodID setString = env->GetMethodID(classPreparedStatement, "setString",
                                               "(ILjava/lang/String;)V");
        
        for (size_t i = 0; i < params.size(); ++i) {
            jstring param = env->NewStringUTF(params[i].c_str());
            env->CallVoidMethod(pstmt, setString, i + 1, param);
            env->DeleteLocalRef(param);
        }
        
        // Execute query
        jmethodID executeQuery = env->GetMethodID(classPreparedStatement, "executeQuery",
                                                  "()Ljava/sql/ResultSet;");
        jobject resultSet = env->CallObjectMethod(pstmt, executeQuery);
        
        if (!resultSet || checkException()) {
            std::cerr << "Failed to execute prepared query" << std::endl;
            env->DeleteLocalRef(pstmt);
            return results;
        }
        
        // Process results (same as executeQuery)
        jmethodID getMetaData = env->GetMethodID(classResultSet, "getMetaData",
                                                 "()Ljava/sql/ResultSetMetaData;");
        jobject metaData = env->CallObjectMethod(resultSet, getMetaData);
        
        jmethodID getColumnCount = env->GetMethodID(classResultSetMetaData,
                                                    "getColumnCount", "()I");
        jint columnCount = env->CallIntMethod(metaData, getColumnCount);
        
        jmethodID getColumnName = env->GetMethodID(classResultSetMetaData,
                                                   "getColumnName", "(I)Ljava/lang/String;");
        std::vector<std::string> columnNames;
        
        for (int i = 1; i <= columnCount; ++i) {
            jstring colName = (jstring)env->CallObjectMethod(metaData, getColumnName, i);
            columnNames.push_back(jstringToString(colName));
            env->DeleteLocalRef(colName);
        }
        
        jmethodID next = env->GetMethodID(classResultSet, "next", "()Z");
        jmethodID getString = env->GetMethodID(classResultSet, "getString",
                                               "(I)Ljava/lang/String;");
        
        while (env->CallBooleanMethod(resultSet, next)) {
            std::map<std::string, std::string> row;
            
            for (int i = 1; i <= columnCount; ++i) {
                jstring value = (jstring)env->CallObjectMethod(resultSet, getString, i);
                row[columnNames[i-1]] = jstringToString(value);
                if (value) env->DeleteLocalRef(value);
            }
            
            results.push_back(row);
        }
        
        // Close resources
        jmethodID close = env->GetMethodID(classResultSet, "close", "()V");
        env->CallVoidMethod(resultSet, close);
        
        close = env->GetMethodID(classPreparedStatement, "close", "()V");
        env->CallVoidMethod(pstmt, close);
        
        env->DeleteLocalRef(resultSet);
        env->DeleteLocalRef(metaData);
        env->DeleteLocalRef(pstmt);
        
        return results;
    }
    
    // Transaction management
    bool beginTransaction() {
        if (!connected) return false;
        
        jmethodID setAutoCommit = env->GetMethodID(classConnection, "setAutoCommit", "(Z)V");
        env->CallVoidMethod(connection, setAutoCommit, JNI_FALSE);
        
        return !checkException();
    }
    
    bool commit() {
        if (!connected) return false;
        
        jmethodID commit = env->GetMethodID(classConnection, "commit", "()V");
        env->CallVoidMethod(connection, commit);
        
        // Re-enable auto-commit
        jmethodID setAutoCommit = env->GetMethodID(classConnection, "setAutoCommit", "(Z)V");
        env->CallVoidMethod(connection, setAutoCommit, JNI_TRUE);
        
        return !checkException();
    }
    
    bool rollback() {
        if (!connected) return false;
        
        jmethodID rollback = env->GetMethodID(classConnection, "rollback", "()V");
        env->CallVoidMethod(connection, rollback);
        
        // Re-enable auto-commit
        jmethodID setAutoCommit = env->GetMethodID(classConnection, "setAutoCommit", "(Z)V");
        env->CallVoidMethod(connection, setAutoCommit, JNI_TRUE);
        
        return !checkException();
    }
    
    // Get database metadata
    std::map<std::string, std::string> getDatabaseInfo() {
        std::map<std::string, std::string> info;
        
        if (!connected) return info;
        
        // Get DatabaseMetaData
        jmethodID getMetaData = env->GetMethodID(classConnection, "getMetaData",
                                                 "()Ljava/sql/DatabaseMetaData;");
        jobject dbMetaData = env->CallObjectMethod(connection, getMetaData);
        
        if (!dbMetaData || checkException()) {
            return info;
        }
        
        jclass classDBMetaData = env->FindClass("java/sql/DatabaseMetaData");
        
        // Get various metadata
        jmethodID method;
        jstring result;
        
        // Database product name
        method = env->GetMethodID(classDBMetaData, "getDatabaseProductName",
                                  "()Ljava/lang/String;");
        result = (jstring)env->CallObjectMethod(dbMetaData, method);
        info["product_name"] = jstringToString(result);
        env->DeleteLocalRef(result);
        
        // Database product version
        method = env->GetMethodID(classDBMetaData, "getDatabaseProductVersion",
                                  "()Ljava/lang/String;");
        result = (jstring)env->CallObjectMethod(dbMetaData, method);
        info["product_version"] = jstringToString(result);
        env->DeleteLocalRef(result);
        
        // Driver name
        method = env->GetMethodID(classDBMetaData, "getDriverName",
                                  "()Ljava/lang/String;");
        result = (jstring)env->CallObjectMethod(dbMetaData, method);
        info["driver_name"] = jstringToString(result);
        env->DeleteLocalRef(result);
        
        // Driver version
        method = env->GetMethodID(classDBMetaData, "getDriverVersion",
                                  "()Ljava/lang/String;");
        result = (jstring)env->CallObjectMethod(dbMetaData, method);
        info["driver_version"] = jstringToString(result);
        env->DeleteLocalRef(result);
        
        // JDBC version
        method = env->GetMethodID(classDBMetaData, "getJDBCMajorVersion", "()I");
        jint majorVersion = env->CallIntMethod(dbMetaData, method);
        
        method = env->GetMethodID(classDBMetaData, "getJDBCMinorVersion", "()I");
        jint minorVersion = env->CallIntMethod(dbMetaData, method);
        
        info["jdbc_version"] = std::to_string(majorVersion) + "." + std::to_string(minorVersion);
        
        env->DeleteLocalRef(dbMetaData);
        
        return info;
    }
    
    // Get tables
    std::vector<std::string> getTables() {
        std::vector<std::string> tables;
        
        if (!connected) return tables;
        
        // Get DatabaseMetaData
        jmethodID getMetaData = env->GetMethodID(classConnection, "getMetaData",
                                                 "()Ljava/sql/DatabaseMetaData;");
        jobject dbMetaData = env->CallObjectMethod(connection, getMetaData);
        
        if (!dbMetaData || checkException()) {
            return tables;
        }
        
        jclass classDBMetaData = env->FindClass("java/sql/DatabaseMetaData");
        
        // Get tables
        jmethodID getTables = env->GetMethodID(classDBMetaData, "getTables",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;)Ljava/sql/ResultSet;");
        
        // Create table types array
        jclass stringClass = env->FindClass("java/lang/String");
        jobjectArray tableTypes = env->NewObjectArray(1, stringClass, nullptr);
        jstring tableType = env->NewStringUTF("TABLE");
        env->SetObjectArrayElement(tableTypes, 0, tableType);
        
        jobject resultSet = env->CallObjectMethod(dbMetaData, getTables,
                                                  nullptr, nullptr, nullptr, tableTypes);
        
        env->DeleteLocalRef(tableType);
        env->DeleteLocalRef(tableTypes);
        
        if (resultSet && !checkException()) {
            jmethodID next = env->GetMethodID(classResultSet, "next", "()Z");
            jmethodID getString = env->GetMethodID(classResultSet, "getString",
                                                   "(Ljava/lang/String;)Ljava/lang/String;");
            
            jstring columnName = env->NewStringUTF("TABLE_NAME");
            
            while (env->CallBooleanMethod(resultSet, next)) {
                jstring tableName = (jstring)env->CallObjectMethod(resultSet, getString, columnName);
                tables.push_back(jstringToString(tableName));
                env->DeleteLocalRef(tableName);
            }
            
            env->DeleteLocalRef(columnName);
            
            jmethodID close = env->GetMethodID(classResultSet, "close", "()V");
            env->CallVoidMethod(resultSet, close);
            env->DeleteLocalRef(resultSet);
        }
        
        env->DeleteLocalRef(dbMetaData);
        
        return tables;
    }
    
    // Call stored procedure
    std::vector<std::map<std::string, std::string>> callProcedure(
        const std::string& procedureName,
        const std::vector<std::string>& params) {
        
        // Build procedure call
        std::string call = "{call " + procedureName + "(";
        for (size_t i = 0; i < params.size(); ++i) {
            call += "?";
            if (i < params.size() - 1) call += ",";
        }
        call += ")}";
        
        return executePreparedQuery(call, params);
    }
    
    // Batch operations
    bool executeBatch(const std::vector<std::string>& queries) {
        if (!connected) return false;
        
        // Create statement
        jmethodID createStatement = env->GetMethodID(classConnection, "createStatement",
                                                     "()Ljava/sql/Statement;");
        jobject statement = env->CallObjectMethod(connection, createStatement);
        
        if (!statement || checkException()) {
            return false;
        }
        
        // Add batch
        jmethodID addBatch = env->GetMethodID(classStatement, "addBatch",
                                              "(Ljava/lang/String;)V");
        
        for (const auto& query : queries) {
            jstring jquery = env->NewStringUTF(query.c_str());
            env->CallVoidMethod(statement, addBatch, jquery);
            env->DeleteLocalRef(jquery);
        }
        
        // Execute batch
        jmethodID executeBatch = env->GetMethodID(classStatement, "executeBatch", "()[I");
        jintArray results = (jintArray)env->CallObjectMethod(statement, executeBatch);
        
        bool success = !checkException();
        
        if (results) {
            env->DeleteLocalRef(results);
        }
        
        // Close statement
        jmethodID close = env->GetMethodID(classStatement, "close", "()V");
        env->CallVoidMethod(statement, close);
        env->DeleteLocalRef(statement);
        
        return success;
    }
    
    // Check connection
    bool isConnected() {
        if (!connected || !connection) return false;
        
        jmethodID isClosed = env->GetMethodID(classConnection, "isClosed", "()Z");
        jboolean closed = env->CallBooleanMethod(connection, isClosed);
        
        return !closed && !checkException();
    }
    
    // Disconnect
    void disconnect() {
        if (connected && connection) {
            jmethodID close = env->GetMethodID(classConnection, "close", "()V");
            env->CallVoidMethod(connection, close);
            env->DeleteGlobalRef(connection);
            connection = nullptr;
            connected = false;
            std::cout << "Disconnected from JDBC database" << std::endl;
        }
        
        // Clean up global references
        if (env) {
            if (classDriverManager) env->DeleteGlobalRef(classDriverManager);
            if (classConnection) env->DeleteGlobalRef(classConnection);
            if (classStatement) env->DeleteGlobalRef(classStatement);
            if (classResultSet) env->DeleteGlobalRef(classResultSet);
            if (classResultSetMetaData) env->DeleteGlobalRef(classResultSetMetaData);
            if (classPreparedStatement) env->DeleteGlobalRef(classPreparedStatement);
            if (classSQLException) env->DeleteGlobalRef(classSQLException);
        }
    }
};

// Helper class for automatic JVM attachment in threads
class JNIThreadAttacher {
private:
    JavaVM* jvm;
    JNIEnv* env;
    bool attached;
    
public:
    JNIThreadAttacher(JavaVM* vm) : jvm(vm), env(nullptr), attached(false) {
        if (jvm) {
            jint result = jvm->AttachCurrentThread((void**)&env, nullptr);
            attached = (result == JNI_OK);
        }
    }
    
    ~JNIThreadAttacher() {
        if (jvm && attached) {
            jvm->DetachCurrentThread();
        }
    }
    
    JNIEnv* getEnv() { return env; }
    bool isAttached() { return attached; }
};

// Usage example
int main() {
    JDBCConnection jdbc;
    
    // Initialize JVM with classpath containing JDBC drivers
    std::string classpath = ".:"
                           "/usr/share/java/mysql-connector-java-8.0.33.jar:"
                           "/usr/share/java/postgresql-42.6.0.jar:"
                           "/usr/share/java/ojdbc8.jar";
    
    if (!jdbc.initialize(classpath)) {
        std::cerr << "Failed to initialize JVM" << std::endl;
        return 1;
    }
    
    // Example 1: MySQL connection
    if (jdbc.loadDriver("com.mysql.cj.jdbc.Driver")) {
        if (jdbc.connect("jdbc:mysql://localhost:3306/testdb", "root", "password")) {
            
            // Create table
            jdbc.executeUpdate(
                "CREATE TABLE IF NOT EXISTS users ("
                "id INT AUTO_INCREMENT PRIMARY KEY,"
                "name VARCHAR(100),"
                "email VARCHAR(100))"
            );
            
            // Insert data
            jdbc.executeUpdate(
                "INSERT INTO users (name, email) VALUES ('John Doe', 'john@example.com')"
            );
            
            // Query data
            auto results = jdbc.executeQuery("SELECT * FROM users");
            
            for (const auto& row : results) {
                for (const auto& [key, value] : row) {
                    std::cout << key << ": " << value << " ";
                }
                std::cout << std::endl;
            }
            
            // Get database info
            auto info = jdbc.getDatabaseInfo();
            std::cout << "Database: " << info["product_name"] 
                     << " " << info["product_version"] << std::endl;
            
            jdbc.disconnect();
        }
    }
    
    // Example 2: PostgreSQL connection
    if (jdbc.loadDriver("org.postgresql.Driver")) {
        if (jdbc.connect("jdbc:postgresql://localhost:5432/testdb", "postgres", "password")) {
            
            // Prepared statement example
            std::vector<std::string> params = {"Jane Doe", "jane@example.com"};
            auto results = jdbc.executePreparedQuery(
                "INSERT INTO users (name, email) VALUES (?, ?) RETURNING id",
                params
            );
            
            if (!results.empty()) {
                std::cout << "Inserted user with ID: " << results[0]["id"] << std::endl;
            }
            
            // Transaction example
            jdbc.beginTransaction();
            jdbc.executeUpdate("UPDATE users SET name = 'Updated Name' WHERE id = 1");
            jdbc.executeUpdate("DELETE FROM old_users");
            jdbc.commit();
            
            // Get tables
            auto tables = jdbc.getTables();
            std::cout << "Tables in database:" << std::endl;
            for (const auto& table : tables) {
                std::cout << "  " << table << std::endl;
            }
            
            jdbc.disconnect();
        }
    }
    
    // Example 3: Oracle connection
    if (jdbc.loadDriver("oracle.jdbc.driver.OracleDriver")) {
        if (jdbc.connect("jdbc:oracle:thin:@localhost:1521:XE", "system", "password")) {
            
            // Call stored procedure
            std::vector<std::string> params = {"1"};
            auto results = jdbc.callProcedure("GET_USER_BY_ID", params);
            
            // Batch operations
            std::vector<std::string> batch = {
                "INSERT INTO logs (message) VALUES ('Log 1')",
                "INSERT INTO logs (message) VALUES ('Log 2')",
                "INSERT INTO logs (message) VALUES ('Log 3')"
            };
            
            if (jdbc.executeBatch(batch)) {
                std::cout << "Batch executed successfully" << std::endl;
            }
            
            jdbc.disconnect();
        }
    }
    
    return 0;
}
```

## Java Helper Class (Optional)

For complex operations, you might want to create a Java helper class:

```java
// JDBCHelper.java
import java.sql.*;
import java.util.*;

public class JDBCHelper {
    private Connection connection;
    
    public boolean connect(String url, String user, String password) {
        try {
            connection = DriverManager.getConnection(url, user, password);
            return true;
        } catch (SQLException e) {
            e.printStackTrace();
            return false;
        }
    }
    
    public List<Map<String, Object>> executeQuery(String query) {
        List<Map<String, Object>> results = new ArrayList<>();
        
        try (Statement stmt = connection.createStatement();
             ResultSet rs = stmt.executeQuery(query)) {
            
            ResultSetMetaData meta = rs.getMetaData();
            int columnCount = meta.getColumnCount();
            
            while (rs.next()) {
                Map<String, Object> row = new HashMap<>();
                for (int i = 1; i <= columnCount; i++) {
                    row.put(meta.getColumnName(i), rs.getObject(i));
                }
                results.add(row);
            }
        } catch (SQLException e) {
            e.printStackTrace();
        }
        
        return results;
    }
    
    public void disconnect() {
        try {
            if (connection != null && !connection.isClosed()) {
                connection.close();
            }
        } catch (SQLException e) {
            e.printStackTrace();
        }
    }
}
```

## Compilation

### Linux
```bash
# Find JNI headers
JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64

# Compile
g++ -std=c++17 -o jdbc_app jdbc_app.cpp \
    -I${JAVA_HOME}/include \
    -I${JAVA_HOME}/include/linux \
    -L${JAVA_HOME}/lib/server \
    -ljvm \
    -Wl,-rpath,${JAVA_HOME}/lib/server
```

### Windows
```bash
# Set paths
set JAVA_HOME=C:\Program Files\Java\jdk-11
set PATH=%JAVA_HOME%\bin;%PATH%

# Compile
cl /EHsc jdbc_app.cpp /I"%JAVA_HOME%\include" /I"%JAVA_HOME%\include\win32" /link "%JAVA_HOME%\lib\jvm.lib"
```

### macOS
```bash
# Find JNI headers
JAVA_HOME=$(/usr/libexec/java_home)

# Compile
g++ -std=c++17 -o jdbc_app jdbc_app.cpp \
    -I${JAVA_HOME}/include \
    -I${JAVA_HOME}/include/darwin \
    -L${JAVA_HOME}/lib/server \
    -ljvm \
    -Wl,-rpath,${JAVA_HOME}/lib/server
```

### CMake Configuration
```cmake
cmake_minimum_required(VERSION 3.10)
project(JDBCApp)

set(CMAKE_CXX_STANDARD 17)

# Find JNI
find_package(JNI REQUIRED)

if(JNI_FOUND)
    message(STATUS "JNI include dirs: ${JNI_INCLUDE_DIRS}")
    message(STATUS "JNI libraries: ${JNI_LIBRARIES}")
endif()

add_executable(jdbc_app main.cpp)

target_include_directories(jdbc_app PRIVATE ${JNI_INCLUDE_DIRS})
target_link_libraries(jdbc_app ${JNI_LIBRARIES})

# Set runtime library path
if(UNIX AND NOT APPLE)
    set_target_properties(jdbc_app PROPERTIES
        INSTALL_RPATH "${JAVA_HOME}/lib/server"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
elseif(APPLE)
    set_target_properties(jdbc_app PROPERTIES
        INSTALL_RPATH "${JAVA_HOME}/lib/server"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()
```

## JDBC URL Formats

### MySQL
```
jdbc:mysql://hostname:port/database[?property1=value1&property2=value2...]
jdbc:mysql://localhost:3306/testdb?useSSL=false&serverTimezone=UTC
```

### PostgreSQL
```
jdbc:postgresql://hostname:port/database[?property1=value1&property2=value2...]
jdbc:postgresql://localhost:5432/testdb?ssl=true&sslfactory=org.postgresql.ssl.NonValidatingFactory
```

### Oracle
```
jdbc:oracle:thin:@hostname:port:SID
jdbc:oracle:thin:@hostname:port/service_name
jdbc:oracle:thin:@//hostname:port/service_name
jdbc:oracle:thin:@(DESCRIPTION=(ADDRESS=(PROTOCOL=TCP)(HOST=hostname)(PORT=port))(CONNECT_DATA=(SERVICE_NAME=service)))
```

### SQL Server
```
jdbc:sqlserver://hostname:port;databaseName=database[;property1=value1;property2=value2...]
jdbc:sqlserver://localhost:1433;databaseName=testdb;integratedSecurity=true
```

### SQLite
```
jdbc:sqlite:path/to/database.db
jdbc:sqlite::memory:
jdbc:sqlite:/absolute/path/to/database.db
```

### H2
```
jdbc:h2:~/test
jdbc:h2:mem:test
jdbc:h2:tcp://localhost/~/test
```

## Performance Considerations

1. **JVM Overhead**: Starting JVM has significant overhead
2. **Memory Usage**: JVM requires substantial memory
3. **Type Conversion**: Converting between Java and C++ types adds overhead
4. **Thread Safety**: JNI calls must be made from attached threads
5. **Connection Pooling**: Implement pooling at the Java level
6. **Batch Operations**: Use JDBC batch operations for better performance

## Advantages and Disadvantages

### Advantages
- Access to all JDBC drivers
- Consistent API across all databases
- Rich Java ecosystem (connection pools, ORMs)
- Platform independence

### Disadvantages
- JVM overhead
- Complex setup
- Performance overhead
- Debugging complexity
- Memory management complexity

## Alternative: JNI Wrapper Library

Consider using existing JNI wrapper libraries:
- **SWIG**: Simplified Wrapper and Interface Generator
- **JavaCPP**: Efficient access to native C++ from Java
- **JNA**: Java Native Access (simpler than JNI)

## Security Considerations

1. **Classpath Security**: Ensure JDBC drivers are from trusted sources
2. **JVM Security**: Configure JVM security policies
3. **Connection Strings**: Never hardcode credentials
4. **SQL Injection**: Use prepared statements
5. **Resource Management**: Properly close all JDBC resources
6. **Thread Safety**: Ensure proper thread attachment/detachment
