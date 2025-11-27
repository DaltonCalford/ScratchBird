#pragma once

/**
 * MySQL mysql.* Emulation Handler
 *
 * Phase D: Catalog Cleanup - MySQL system catalog emulation
 *
 * This module implements MySQL-compatible mysql.* system tables that allow
 * MySQL client tools (MySQL Workbench, mysql CLI, etc.) to introspect the
 * ScratchBird catalog as if it were a MySQL database.
 *
 * Implemented Views (MySQL-compatible):
 * - mysql.user: User accounts
 * - mysql.db: Database privileges
 * - mysql.tables_priv: Table privileges
 * - mysql.columns_priv: Column privileges
 * - mysql.proc: Stored procedures
 * - mysql.event: Events
 *
 * Created: November 26, 2025
 * Phase: Catalog Cleanup Phase D
 */

#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/typed_value.h"
#include <string>
#include <vector>

namespace scratchbird::catalog {

using namespace scratchbird::core;

/**
 * MySQLCatalogHandler - MySQL mysql.* schema emulation
 *
 * Provides virtual views that query the ScratchBird internal catalog and
 * present data in MySQL mysql.* format for tool compatibility.
 *
 * Usage:
 *   SELECT * FROM mysql.user WHERE User = 'admin';
 *   SELECT * FROM mysql.proc WHERE db = 'mydb';
 */
class MySQLCatalogHandler : public VirtualCatalogHandler {
public:
    /**
     * Constructor - takes catalog manager reference
     */
    explicit MySQLCatalogHandler(CatalogManager* catalog) {
        catalog_manager_ = catalog;
        initializeTableNames();
    }

    /**
     * Get the protocol type
     */
    ProtocolType getProtocolType() const override {
        return ProtocolType::MYSQL;
    }

    /**
     * Check if this handler owns the given schema
     * @return true for "mysql" (case-insensitive)
     */
    bool ownsSchema(const std::string& schema_name) const override {
        return equalsCaseInsensitive(schema_name, "mysql");
    }

    /**
     * Check if this handler owns the given table
     */
    bool ownsTable(const std::string& schema_name,
                   const std::string& table_name) const override {
        if (!ownsSchema(schema_name)) {
            return false;
        }
        for (const auto& name : table_names_) {
            if (equalsCaseInsensitive(table_name, name)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Execute a query against a virtual table
     */
    Status queryTable(const std::string& schema_name,
                      const std::string& table_name,
                      const std::string& where_clause,
                      VirtualResultSet& results,
                      ErrorContext* ctx = nullptr) override {
        if (!ownsSchema(schema_name)) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              "Schema not found: " + schema_name);
            return Status::NOT_FOUND;
        }

        if (equalsCaseInsensitive(table_name, "user")) {
            return queryMysqlUser(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "db")) {
            return queryMysqlDb(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "tables_priv")) {
            return queryMysqlTablesPriv(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "columns_priv")) {
            return queryMysqlColumnsPriv(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "proc")) {
            return queryMysqlProc(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "event")) {
            return queryMysqlEvent(where_clause, results, ctx);
        }

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          "Table not found: mysql." + table_name);
        return Status::NOT_FOUND;
    }

    /**
     * Get column definitions for a virtual table
     */
    Status getTableColumns(const std::string& schema_name,
                           const std::string& table_name,
                           std::vector<CatalogManager::ColumnInfo>& columns,
                           ErrorContext* ctx = nullptr) override {
        if (!ownsSchema(schema_name)) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              "Schema not found: " + schema_name);
            return Status::NOT_FOUND;
        }

        if (equalsCaseInsensitive(table_name, "user")) {
            return getMysqlUserColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "db")) {
            return getMysqlDbColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "tables_priv")) {
            return getMysqlTablesPrivColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "columns_priv")) {
            return getMysqlColumnsPrivColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "proc")) {
            return getMysqlProcColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "event")) {
            return getMysqlEventColumns(columns);
        }

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          "Table not found: mysql." + table_name);
        return Status::NOT_FOUND;
    }

    /**
     * List all virtual tables in mysql schema
     */
    Status listTables(const std::string& schema_name,
                      std::vector<std::string>& table_names,
                      ErrorContext* ctx = nullptr) override {
        if (!ownsSchema(schema_name)) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              "Schema not found: " + schema_name);
            return Status::NOT_FOUND;
        }

        table_names = table_names_;
        return Status::OK;
    }

    /**
     * List all schemas owned by this handler
     */
    Status listSchemas(std::vector<std::string>& schema_names,
                       ErrorContext* ctx = nullptr) override {
        schema_names.clear();
        schema_names.push_back("mysql");
        return Status::OK;
    }

private:
    std::vector<std::string> table_names_;

    void initializeTableNames() {
        table_names_ = {
            "user",
            "db",
            "tables_priv",
            "columns_priv",
            "proc",
            "event"
        };
    }

    // ========================================================================
    // View Query Implementations
    // ========================================================================

    /**
     * mysql.user - user accounts
     */
    Status queryMysqlUser(const std::string& where_clause,
                          VirtualResultSet& results,
                          ErrorContext* ctx = nullptr) {
        results.column_names = {
            "Host", "User", "Select_priv", "Insert_priv", "Update_priv",
            "Delete_priv", "Create_priv", "Drop_priv", "Reload_priv",
            "Shutdown_priv", "Process_priv", "File_priv", "Grant_priv",
            "References_priv", "Index_priv", "Alter_priv", "Show_db_priv",
            "Super_priv", "Create_tmp_table_priv", "Lock_tables_priv",
            "Execute_priv", "Repl_slave_priv", "Repl_client_priv",
            "Create_view_priv", "Show_view_priv", "Create_routine_priv",
            "Alter_routine_priv", "Create_user_priv", "Event_priv",
            "Trigger_priv", "Create_tablespace_priv", "ssl_type",
            "ssl_cipher", "x509_issuer", "x509_subject", "max_questions",
            "max_updates", "max_connections", "max_user_connections",
            "plugin", "authentication_string", "password_expired",
            "password_last_changed", "password_lifetime", "account_locked",
            "Create_role_priv", "Drop_role_priv", "Password_reuse_history",
            "Password_reuse_time", "Password_require_current", "User_attributes"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);

        std::vector<CatalogManager::UserInfo> users;
        Status s = catalog_manager_->listUsers(users, ctx);
        if (!s.ok()) return s;

        for (const auto& user : users) {
            std::string allPriv = user.is_superuser ? "Y" : "N";

            VirtualRow row;
            row.columns = {
                {"Host", TypedValue::makeString("%")},  // Accept from any host
                {"User", TypedValue::makeString(user.username)},
                {"Select_priv", TypedValue::makeString(allPriv)},
                {"Insert_priv", TypedValue::makeString(allPriv)},
                {"Update_priv", TypedValue::makeString(allPriv)},
                {"Delete_priv", TypedValue::makeString(allPriv)},
                {"Create_priv", TypedValue::makeString(user.can_create_db ? "Y" : "N")},
                {"Drop_priv", TypedValue::makeString(allPriv)},
                {"Reload_priv", TypedValue::makeString(allPriv)},
                {"Shutdown_priv", TypedValue::makeString(allPriv)},
                {"Process_priv", TypedValue::makeString(allPriv)},
                {"File_priv", TypedValue::makeString(allPriv)},
                {"Grant_priv", TypedValue::makeString(allPriv)},
                {"References_priv", TypedValue::makeString(allPriv)},
                {"Index_priv", TypedValue::makeString(allPriv)},
                {"Alter_priv", TypedValue::makeString(allPriv)},
                {"Show_db_priv", TypedValue::makeString(allPriv)},
                {"Super_priv", TypedValue::makeString(allPriv)},
                {"Create_tmp_table_priv", TypedValue::makeString(allPriv)},
                {"Lock_tables_priv", TypedValue::makeString(allPriv)},
                {"Execute_priv", TypedValue::makeString(allPriv)},
                {"Repl_slave_priv", TypedValue::makeString("N")},
                {"Repl_client_priv", TypedValue::makeString("N")},
                {"Create_view_priv", TypedValue::makeString(allPriv)},
                {"Show_view_priv", TypedValue::makeString(allPriv)},
                {"Create_routine_priv", TypedValue::makeString(allPriv)},
                {"Alter_routine_priv", TypedValue::makeString(allPriv)},
                {"Create_user_priv", TypedValue::makeString(allPriv)},
                {"Event_priv", TypedValue::makeString(allPriv)},
                {"Trigger_priv", TypedValue::makeString(allPriv)},
                {"Create_tablespace_priv", TypedValue::makeString(allPriv)},
                {"ssl_type", TypedValue::makeString("")},
                {"ssl_cipher", TypedValue::makeString("")},
                {"x509_issuer", TypedValue::makeString("")},
                {"x509_subject", TypedValue::makeString("")},
                {"max_questions", TypedValue::makeString("0")},
                {"max_updates", TypedValue::makeString("0")},
                {"max_connections", TypedValue::makeString("0")},
                {"max_user_connections", TypedValue::makeString("0")},
                {"plugin", TypedValue::makeString("caching_sha2_password")},
                {"authentication_string", TypedValue::makeString("********")},
                {"password_expired", TypedValue::makeString("N")},
                {"password_last_changed", TypedValue::makeNull()},
                {"password_lifetime", TypedValue::makeNull()},
                {"account_locked", TypedValue::makeString(user.is_locked ? "Y" : "N")},
                {"Create_role_priv", TypedValue::makeString(allPriv)},
                {"Drop_role_priv", TypedValue::makeString(allPriv)},
                {"Password_reuse_history", TypedValue::makeNull()},
                {"Password_reuse_time", TypedValue::makeNull()},
                {"Password_require_current", TypedValue::makeString("N")},
                {"User_attributes", TypedValue::makeNull()}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    /**
     * mysql.db - database privileges
     */
    Status queryMysqlDb(const std::string& where_clause,
                        VirtualResultSet& results,
                        ErrorContext* ctx = nullptr) {
        results.column_names = {
            "Host", "Db", "User", "Select_priv", "Insert_priv", "Update_priv",
            "Delete_priv", "Create_priv", "Drop_priv", "Grant_priv",
            "References_priv", "Index_priv", "Alter_priv",
            "Create_tmp_table_priv", "Lock_tables_priv", "Create_view_priv",
            "Show_view_priv", "Create_routine_priv", "Alter_routine_priv",
            "Execute_priv", "Event_priv", "Trigger_priv"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);

        // Get schemas (treated as MySQL databases)
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        std::vector<CatalogManager::UserInfo> users;
        s = catalog_manager_->listUsers(users, ctx);
        if (!s.ok()) return s;

        // Create a row for each user-schema combination (simplified)
        for (const auto& schema : schemas) {
            VirtualRow row;
            row.columns = {
                {"Host", TypedValue::makeString("%")},
                {"Db", TypedValue::makeString(schema.schema_name)},
                {"User", TypedValue::makeString(schema.owner_name)},
                {"Select_priv", TypedValue::makeString("Y")},
                {"Insert_priv", TypedValue::makeString("Y")},
                {"Update_priv", TypedValue::makeString("Y")},
                {"Delete_priv", TypedValue::makeString("Y")},
                {"Create_priv", TypedValue::makeString("Y")},
                {"Drop_priv", TypedValue::makeString("Y")},
                {"Grant_priv", TypedValue::makeString("Y")},
                {"References_priv", TypedValue::makeString("Y")},
                {"Index_priv", TypedValue::makeString("Y")},
                {"Alter_priv", TypedValue::makeString("Y")},
                {"Create_tmp_table_priv", TypedValue::makeString("Y")},
                {"Lock_tables_priv", TypedValue::makeString("Y")},
                {"Create_view_priv", TypedValue::makeString("Y")},
                {"Show_view_priv", TypedValue::makeString("Y")},
                {"Create_routine_priv", TypedValue::makeString("Y")},
                {"Alter_routine_priv", TypedValue::makeString("Y")},
                {"Execute_priv", TypedValue::makeString("Y")},
                {"Event_priv", TypedValue::makeString("Y")},
                {"Trigger_priv", TypedValue::makeString("Y")}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    /**
     * mysql.tables_priv - table privileges
     */
    Status queryMysqlTablesPriv(const std::string& where_clause,
                                VirtualResultSet& results,
                                ErrorContext* ctx = nullptr) {
        results.column_names = {
            "Host", "Db", "User", "Table_name", "Grantor", "Timestamp",
            "Table_priv", "Column_priv"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[5] = DataType::TIMESTAMP;  // Timestamp

        // Return empty - detailed table privileges not tracked yet
        return Status::OK;
    }

    /**
     * mysql.columns_priv - column privileges
     */
    Status queryMysqlColumnsPriv(const std::string& where_clause,
                                 VirtualResultSet& results,
                                 ErrorContext* ctx = nullptr) {
        results.column_names = {
            "Host", "Db", "User", "Table_name", "Column_name", "Timestamp",
            "Column_priv"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[5] = DataType::TIMESTAMP;

        // Return empty - detailed column privileges not tracked yet
        return Status::OK;
    }

    /**
     * mysql.proc - stored procedures and functions
     */
    Status queryMysqlProc(const std::string& where_clause,
                          VirtualResultSet& results,
                          ErrorContext* ctx = nullptr) {
        results.column_names = {
            "db", "name", "type", "specific_name", "language", "sql_data_access",
            "is_deterministic", "security_type", "param_list", "returns",
            "body", "definer", "created", "modified", "sql_mode",
            "comment", "character_set_client", "collation_connection",
            "db_collation", "body_utf8"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[12] = DataType::TIMESTAMP;  // created
        results.column_types[13] = DataType::TIMESTAMP;  // modified

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            // Functions
            std::vector<CatalogManager::FunctionInfo> functions;
            s = catalog_manager_->listFunctions(schema.schema_id, functions, ctx);
            if (s.ok()) {
                for (const auto& func : functions) {
                    // Build parameter list
                    std::string params;
                    for (size_t i = 0; i < func.parameters.size(); ++i) {
                        if (i > 0) params += ", ";
                        params += func.parameters[i].param_name + " " +
                                  dataTypeToMySQLType(func.parameters[i].data_type);
                    }

                    VirtualRow row;
                    row.columns = {
                        {"db", TypedValue::makeString(schema.schema_name)},
                        {"name", TypedValue::makeString(func.function_name)},
                        {"type", TypedValue::makeString("FUNCTION")},
                        {"specific_name", TypedValue::makeString(func.function_name)},
                        {"language", TypedValue::makeString("SQL")},
                        {"sql_data_access", TypedValue::makeString("CONTAINS_SQL")},
                        {"is_deterministic", TypedValue::makeString(func.is_deterministic ? "YES" : "NO")},
                        {"security_type", TypedValue::makeString("DEFINER")},
                        {"param_list", TypedValue::makeString(params)},
                        {"returns", TypedValue::makeString(dataTypeToMySQLType(func.return_type))},
                        {"body", TypedValue::makeString(func.body)},
                        {"definer", TypedValue::makeString(func.owner_name + "@%")},
                        {"created", TypedValue::makeNull()},
                        {"modified", TypedValue::makeNull()},
                        {"sql_mode", TypedValue::makeString("")},
                        {"comment", TypedValue::makeString("")},
                        {"character_set_client", TypedValue::makeString("utf8mb4")},
                        {"collation_connection", TypedValue::makeString("utf8mb4_general_ci")},
                        {"db_collation", TypedValue::makeString("utf8mb4_general_ci")},
                        {"body_utf8", TypedValue::makeString(func.body)}
                    };
                    results.rows.push_back(std::move(row));
                }
            }

            // Procedures
            std::vector<CatalogManager::ProcedureInfo> procedures;
            s = catalog_manager_->listProcedures(schema.schema_id, procedures, ctx);
            if (s.ok()) {
                for (const auto& proc : procedures) {
                    std::string params;
                    for (size_t i = 0; i < proc.parameters.size(); ++i) {
                        if (i > 0) params += ", ";
                        std::string mode = "IN";
                        if (proc.parameters[i].mode == CatalogManager::ParamMode::OUT) mode = "OUT";
                        else if (proc.parameters[i].mode == CatalogManager::ParamMode::INOUT) mode = "INOUT";
                        params += mode + " " + proc.parameters[i].param_name + " " +
                                  dataTypeToMySQLType(proc.parameters[i].data_type);
                    }

                    VirtualRow row;
                    row.columns = {
                        {"db", TypedValue::makeString(schema.schema_name)},
                        {"name", TypedValue::makeString(proc.procedure_name)},
                        {"type", TypedValue::makeString("PROCEDURE")},
                        {"specific_name", TypedValue::makeString(proc.procedure_name)},
                        {"language", TypedValue::makeString("SQL")},
                        {"sql_data_access", TypedValue::makeString("MODIFIES_SQL_DATA")},
                        {"is_deterministic", TypedValue::makeString("NO")},
                        {"security_type", TypedValue::makeString("DEFINER")},
                        {"param_list", TypedValue::makeString(params)},
                        {"returns", TypedValue::makeString("")},
                        {"body", TypedValue::makeString(proc.body)},
                        {"definer", TypedValue::makeString(proc.owner_name + "@%")},
                        {"created", TypedValue::makeNull()},
                        {"modified", TypedValue::makeNull()},
                        {"sql_mode", TypedValue::makeString("")},
                        {"comment", TypedValue::makeString("")},
                        {"character_set_client", TypedValue::makeString("utf8mb4")},
                        {"collation_connection", TypedValue::makeString("utf8mb4_general_ci")},
                        {"db_collation", TypedValue::makeString("utf8mb4_general_ci")},
                        {"body_utf8", TypedValue::makeString(proc.body)}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        return Status::OK;
    }

    /**
     * mysql.event - scheduled events
     */
    Status queryMysqlEvent(const std::string& where_clause,
                           VirtualResultSet& results,
                           ErrorContext* ctx = nullptr) {
        results.column_names = {
            "db", "name", "body", "definer", "execute_at", "interval_value",
            "interval_field", "created", "modified", "last_executed",
            "starts", "ends", "status", "on_completion", "sql_mode",
            "comment", "originator", "time_zone", "character_set_client",
            "collation_connection", "db_collation", "body_utf8"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);

        // Events not implemented yet - return empty
        return Status::OK;
    }

    // ========================================================================
    // Column Definition Helpers
    // ========================================================================

    Status getMysqlUserColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("Host", DataType::VARCHAR, 1),
            makeColumnInfo("User", DataType::VARCHAR, 2),
            makeColumnInfo("Select_priv", DataType::VARCHAR, 3),
            makeColumnInfo("Insert_priv", DataType::VARCHAR, 4),
            makeColumnInfo("Super_priv", DataType::VARCHAR, 5)
        };
        return Status::OK;
    }

    Status getMysqlDbColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("Host", DataType::VARCHAR, 1),
            makeColumnInfo("Db", DataType::VARCHAR, 2),
            makeColumnInfo("User", DataType::VARCHAR, 3),
            makeColumnInfo("Select_priv", DataType::VARCHAR, 4),
            makeColumnInfo("Insert_priv", DataType::VARCHAR, 5)
        };
        return Status::OK;
    }

    Status getMysqlTablesPrivColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("Host", DataType::VARCHAR, 1),
            makeColumnInfo("Db", DataType::VARCHAR, 2),
            makeColumnInfo("User", DataType::VARCHAR, 3),
            makeColumnInfo("Table_name", DataType::VARCHAR, 4),
            makeColumnInfo("Table_priv", DataType::VARCHAR, 5)
        };
        return Status::OK;
    }

    Status getMysqlColumnsPrivColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("Host", DataType::VARCHAR, 1),
            makeColumnInfo("Db", DataType::VARCHAR, 2),
            makeColumnInfo("User", DataType::VARCHAR, 3),
            makeColumnInfo("Table_name", DataType::VARCHAR, 4),
            makeColumnInfo("Column_name", DataType::VARCHAR, 5),
            makeColumnInfo("Column_priv", DataType::VARCHAR, 6)
        };
        return Status::OK;
    }

    Status getMysqlProcColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("db", DataType::VARCHAR, 1),
            makeColumnInfo("name", DataType::VARCHAR, 2),
            makeColumnInfo("type", DataType::VARCHAR, 3),
            makeColumnInfo("body", DataType::VARCHAR, 4),
            makeColumnInfo("definer", DataType::VARCHAR, 5)
        };
        return Status::OK;
    }

    Status getMysqlEventColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("db", DataType::VARCHAR, 1),
            makeColumnInfo("name", DataType::VARCHAR, 2),
            makeColumnInfo("body", DataType::VARCHAR, 3),
            makeColumnInfo("status", DataType::VARCHAR, 4)
        };
        return Status::OK;
    }

    // ========================================================================
    // Helper Functions
    // ========================================================================

    CatalogManager::ColumnInfo makeColumnInfo(const std::string& name,
                                               DataType type,
                                               int ordinal) {
        CatalogManager::ColumnInfo col;
        col.column_name = name;
        col.data_type = type;
        col.ordinal = ordinal;
        col.nullable = true;
        col.max_length = (type == DataType::VARCHAR) ? 256 : 0;
        return col;
    }

    static const char* dataTypeToMySQLType(DataType type) {
        switch (type) {
            case DataType::BOOLEAN:   return "tinyint(1)";
            case DataType::INT16:     return "smallint";
            case DataType::INT32:     return "int";
            case DataType::INT64:     return "bigint";
            case DataType::FLOAT32:   return "float";
            case DataType::FLOAT64:   return "double";
            case DataType::DECIMAL:   return "decimal";
            case DataType::VARCHAR:   return "varchar(255)";
            case DataType::CHAR:      return "char(1)";
            case DataType::TEXT:      return "text";
            case DataType::BLOB:      return "blob";
            case DataType::DATE:      return "date";
            case DataType::TIME:      return "time";
            case DataType::TIMESTAMP: return "timestamp";
            case DataType::UUID:      return "binary(16)";
            default:                  return "varchar(255)";
        }
    }
};

} // namespace scratchbird::catalog
