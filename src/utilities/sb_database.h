/*
 * ScratchBird Database Connection Framework
 * Provides unified database access for ScratchBird utilities
 */

#ifndef SB_DATABASE_H
#define SB_DATABASE_H

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <iostream>
#include <firebird/ibase.h>

class SBDatabase {
private:
    isc_db_handle database_handle;
    isc_tr_handle transaction_handle;
    ISC_STATUS status_vector[20];
    std::string database_name;
    std::string username;
    std::string password;
    std::string role;
    bool connected;
    bool in_transaction;
    bool trusted_auth;

public:
    SBDatabase();
    ~SBDatabase();

    // Connection management
    bool connect(const std::string& db_name, const std::string& user = "", 
                 const std::string& pass = "", const std::string& role_name = "", 
                 bool trusted = false);
    bool disconnect();
    bool isConnected() const { return connected; }

    // Transaction management
    bool startTransaction();
    bool commitTransaction();
    bool rollbackTransaction();
    bool isInTransaction() const { return in_transaction; }

    // Query execution
    bool executeQuery(const std::string& sql);
    bool executeUpdate(const std::string& sql, int& affected_rows);
    bool executeSelect(const std::string& sql, std::vector<std::vector<std::string>>& results, 
                       std::vector<std::string>& column_names);

    // Error handling
    std::string getLastError() const;
    bool hasError() const;
    void clearError();

    // Database information
    std::string getDatabaseName() const { return database_name; }
    std::string getUsername() const { return username; }
    std::string getRole() const { return role; }

    // Utility functions
    bool tableExists(const std::string& table_name, const std::string& schema_name = "");
    bool schemaExists(const std::string& schema_name);
    std::vector<std::string> getTableNames(const std::string& schema_name = "");
    std::vector<std::string> getSchemaNames();
    
    // Database statistics
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

private:
    bool buildDPB(std::string& dpb);
    void logError(const std::string& operation);
    std::string formatStatusVector() const;
    bool prepareAndExecute(const std::string& sql, isc_stmt_handle& stmt);
    bool fetchResults(isc_stmt_handle stmt, std::vector<std::vector<std::string>>& results,
                      std::vector<std::string>& column_names);
};

// Statement wrapper for complex queries
class SBStatement {
private:
    SBDatabase* database;
    isc_stmt_handle statement_handle;
    XSQLDA* input_sqlda;
    XSQLDA* output_sqlda;
    bool prepared;
    std::string sql_text;

public:
    SBStatement(SBDatabase* db);
    ~SBStatement();

    bool prepare(const std::string& sql);
    bool execute();
    bool fetch();
    bool close();

    // Parameter binding
    bool bindParameter(int index, const std::string& value);
    bool bindParameter(int index, int value);
    bool bindParameter(int index, double value);

    // Result retrieval
    std::string getString(int column);
    int getInt(int column);
    double getDouble(int column);
    bool isNull(int column);

    int getColumnCount() const;
    std::string getColumnName(int column) const;
    std::string getColumnType(int column) const;
};

// Error handling utility
class SBException : public std::exception {
private:
    std::string message;
    ISC_STATUS error_code;

public:
    SBException(const std::string& msg, ISC_STATUS code = 0) 
        : message(msg), error_code(code) {}
    
    const char* what() const noexcept override {
        return message.c_str();
    }
    
    ISC_STATUS getErrorCode() const { return error_code; }
};

#endif // SB_DATABASE_H