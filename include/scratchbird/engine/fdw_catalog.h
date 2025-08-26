#pragma once

#include "scratchbird/engine/fdw.h"
#include "scratchbird/engine/fdw_security.h"
#include "scratchbird/engine/types.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    /// Catalog table names for FDW system
    namespace FdwCatalogTables {
        constexpr const char* FOREIGN_DATA_WRAPPERS = "SDB$FOREIGN_DATA_WRAPPERS";
        constexpr const char* FOREIGN_SERVERS = "SDB$FOREIGN_SERVERS";
        constexpr const char* USER_MAPPINGS = "SDB$USER_MAPPINGS";
        constexpr const char* FOREIGN_TABLES = "SDB$FOREIGN_TABLES";
        constexpr const char* FOREIGN_TABLE_COLUMNS = "SDB$FOREIGN_TABLE_COLUMNS";
        constexpr const char* DATABASE_LINKS = "SDB$DATABASE_LINKS";
        constexpr const char* FDW_STATISTICS = "SDB$FDW_STATISTICS";
        constexpr const char* FDW_OPTIONS = "SDB$FDW_OPTIONS";
    }

    /// Foreign Data Wrapper catalog entry
    struct FdwCatalogEntry {
        std::string fdw_name;
        std::string fdw_version;
        std::string fdw_library;        // Path to FDW library/plugin
        std::string fdw_handler;        // Handler function name
        std::string fdw_validator;      // Validator function name
        std::int32_t fdw_capabilities;  // Bitmask of FdwCapability
        std::string description;
        std::int64_t created_time;
        std::string created_by;
        bool is_active;
        std::unordered_map<std::string, std::string> options;
    };

    /// Foreign Server catalog entry
    struct ForeignServerCatalogEntry {
        std::string server_name;
        std::string fdw_name;
        std::string server_type;    // e.g., "postgresql", "mysql", "csv", "json"
        std::string server_version;
        std::string connection_string;
        std::int32_t max_connections;
        std::int32_t current_connections;
        std::string health_status;  // "HEALTHY", "DEGRADED", "FAILED", "UNKNOWN"
        std::int64_t last_health_check;
        std::int64_t created_time;
        std::string created_by;
        bool is_active;
        std::unordered_map<std::string, std::string> options;
    };

    /// User Mapping catalog entry
    struct UserMappingCatalogEntry {
        std::string server_name;
        std::string local_username;
        std::string remote_username;
        std::string auth_method;        // "password", "certificate", "kerberos", "trust"
        std::string credential_id;      // Reference to encrypted credential storage
        std::int64_t created_time;
        std::int64_t last_used_time;
        std::int32_t use_count;
        bool is_active;
        std::unordered_map<std::string, std::string> options;
    };

    /// Foreign Table catalog entry
    struct ForeignTableCatalogEntry {
        std::string table_name;
        std::string schema_name;
        std::string server_name;
        std::string remote_schema;
        std::string remote_table;
        std::int32_t column_count;
        std::int64_t estimated_rows;
        std::int64_t last_analyzed;
        std::string table_type;         // "TABLE", "VIEW", "MATERIALIZED_VIEW"
        std::int64_t created_time;
        std::string created_by;
        bool is_active;
        std::unordered_map<std::string, std::string> options;
    };

    /// Foreign Table Column catalog entry
    struct ForeignTableColumnCatalogEntry {
        std::string table_name;
        std::string schema_name;
        std::string column_name;
        std::int32_t column_position;
        TypeKind data_type;
        std::int32_t max_length;
        std::int32_t precision;
        std::int32_t scale;
        bool is_nullable;
        std::string default_value;
        std::string remote_column_name;
        std::string remote_data_type;
        std::string collation;
        std::string description;
        bool is_key_column;
        std::unordered_map<std::string, std::string> options;
    };

    /// Database Link catalog entry
    struct DatabaseLinkCatalogEntry {
        std::string link_name;
        std::string target_server;
        std::string connection_type;    // "SHARED", "DEDICATED", "POOLED"
        std::string authentication;     // "CURRENT_USER", "DEFINER", "EXPLICIT"
        std::string connect_string;
        std::int32_t max_connections;
        std::int32_t timeout_seconds;
        std::string health_status;
        std::int64_t last_used_time;
        std::int64_t created_time;
        std::string created_by;
        bool is_active;
        std::unordered_map<std::string, std::string> options;
    };

    /// FDW Statistics catalog entry
    struct FdwStatisticsCatalogEntry {
        std::string object_type;        // "SERVER", "TABLE", "COLUMN"
        std::string object_name;
        std::string stat_name;
        std::string stat_value;
        std::string stat_type;          // "COUNT", "ESTIMATE", "HISTOGRAM", "CORRELATION"
        std::int64_t collection_time;
        std::int64_t expiry_time;
        bool is_current;
    };

    /// Generic FDW options catalog entry
    struct FdwOptionsCatalogEntry {
        std::string object_type;        // "FDW", "SERVER", "USER_MAPPING", "FOREIGN_TABLE"
        std::string object_name;
        std::string option_name;
        std::string option_value;
        std::string option_type;        // "STRING", "INTEGER", "BOOLEAN", "ENUM"
        bool is_required;
        std::string description;
        std::string valid_values;       // Comma-separated list for ENUMs
    };

    /// FDW catalog manager for persistent metadata storage
    class FdwCatalogManager
    {
      public:
        FdwCatalogManager();
        ~FdwCatalogManager();

        // Catalog initialization
        bool initialize_catalog(std::string& error_msg);
        bool verify_catalog_schema(std::string& error_msg);
        bool upgrade_catalog_schema(const std::string& from_version, const std::string& to_version,
                                   std::string& error_msg);

        // FDW management
        bool register_fdw(const FdwCatalogEntry& fdw_entry, std::string& error_msg);
        bool unregister_fdw(const std::string& fdw_name, std::string& error_msg);
        bool get_fdw_entry(const std::string& fdw_name, FdwCatalogEntry& entry, std::string& error_msg);
        std::vector<FdwCatalogEntry> list_fdw_entries();

        // Foreign server management
        bool create_foreign_server(const ForeignServerCatalogEntry& server_entry, std::string& error_msg);
        bool drop_foreign_server(const std::string& server_name, bool cascade, std::string& error_msg);
        bool get_foreign_server(const std::string& server_name, ForeignServerCatalogEntry& entry,
                               std::string& error_msg);
        std::vector<ForeignServerCatalogEntry> list_foreign_servers(const std::string& fdw_name = "");
        bool update_server_health_status(const std::string& server_name, const std::string& status,
                                        std::string& error_msg);

        // User mapping management
        bool create_user_mapping(const UserMappingCatalogEntry& mapping_entry, std::string& error_msg);
        bool drop_user_mapping(const std::string& server_name, const std::string& local_username,
                              std::string& error_msg);
        bool get_user_mapping(const std::string& server_name, const std::string& local_username,
                             UserMappingCatalogEntry& entry, std::string& error_msg);
        std::vector<UserMappingCatalogEntry> list_user_mappings(const std::string& server_name = "");

        // Foreign table management
        bool create_foreign_table(const ForeignTableCatalogEntry& table_entry,
                                 const std::vector<ForeignTableColumnCatalogEntry>& columns,
                                 std::string& error_msg);
        bool drop_foreign_table(const std::string& table_name, const std::string& schema_name,
                               std::string& error_msg);
        bool get_foreign_table(const std::string& table_name, const std::string& schema_name,
                              ForeignTableCatalogEntry& entry, std::string& error_msg);
        bool get_foreign_table_columns(const std::string& table_name, const std::string& schema_name,
                                      std::vector<ForeignTableColumnCatalogEntry>& columns,
                                      std::string& error_msg);
        std::vector<ForeignTableCatalogEntry> list_foreign_tables(const std::string& server_name = "",
                                                                  const std::string& schema_name = "");

        // Database link management
        bool create_database_link(const DatabaseLinkCatalogEntry& link_entry, std::string& error_msg);
        bool drop_database_link(const std::string& link_name, std::string& error_msg);
        bool get_database_link(const std::string& link_name, DatabaseLinkCatalogEntry& entry,
                              std::string& error_msg);
        std::vector<DatabaseLinkCatalogEntry> list_database_links();

        // Statistics management
        bool update_statistics(const FdwStatisticsCatalogEntry& stats_entry, std::string& error_msg);
        bool get_statistics(const std::string& object_type, const std::string& object_name,
                           std::vector<FdwStatisticsCatalogEntry>& stats, std::string& error_msg);
        bool cleanup_expired_statistics(std::string& error_msg);

        // Options management
        bool set_fdw_option(const std::string& object_type, const std::string& object_name,
                           const std::string& option_name, const std::string& option_value,
                           std::string& error_msg);
        bool get_fdw_options(const std::string& object_type, const std::string& object_name,
                            std::unordered_map<std::string, std::string>& options, std::string& error_msg);
        bool remove_fdw_option(const std::string& object_type, const std::string& object_name,
                              const std::string& option_name, std::string& error_msg);

        // Schema import operations
        bool import_foreign_schema(const std::string& server_name, const std::string& remote_schema,
                                  const std::string& local_schema, 
                                  const std::vector<std::string>& table_filters,
                                  bool exclude_mode, std::string& error_msg);

        // Dependency management
        bool check_dependencies(const std::string& object_type, const std::string& object_name,
                               std::vector<std::string>& dependencies, std::string& error_msg);
        bool resolve_dependencies(const std::string& object_type, const std::string& object_name,
                                 bool cascade, std::string& error_msg);

        // Catalog queries and metadata
        std::vector<std::string> get_catalog_table_names();
        bool export_catalog_metadata(const std::string& file_path, std::string& error_msg);
        bool import_catalog_metadata(const std::string& file_path, bool replace_existing,
                                    std::string& error_msg);

      private:
        class Impl;
        std::unique_ptr<Impl> pImpl_;
    };

    /// FDW catalog DDL executor for CREATE/DROP statements
    class FdwCatalogDDLExecutor
    {
      public:
        FdwCatalogDDLExecutor(FdwCatalogManager& catalog_manager);
        ~FdwCatalogDDLExecutor();

        // DDL statement execution
        bool execute_create_foreign_data_wrapper(const std::string& ddl_statement, std::string& error_msg);
        bool execute_drop_foreign_data_wrapper(const std::string& ddl_statement, std::string& error_msg);

        bool execute_create_foreign_server(const std::string& ddl_statement, std::string& error_msg);
        bool execute_alter_foreign_server(const std::string& ddl_statement, std::string& error_msg);
        bool execute_drop_foreign_server(const std::string& ddl_statement, std::string& error_msg);

        bool execute_create_user_mapping(const std::string& ddl_statement, std::string& error_msg);
        bool execute_alter_user_mapping(const std::string& ddl_statement, std::string& error_msg);
        bool execute_drop_user_mapping(const std::string& ddl_statement, std::string& error_msg);

        bool execute_create_foreign_table(const std::string& ddl_statement, std::string& error_msg);
        bool execute_alter_foreign_table(const std::string& ddl_statement, std::string& error_msg);
        bool execute_drop_foreign_table(const std::string& ddl_statement, std::string& error_msg);

        bool execute_create_database_link(const std::string& ddl_statement, std::string& error_msg);
        bool execute_drop_database_link(const std::string& ddl_statement, std::string& error_msg);

        bool execute_import_foreign_schema(const std::string& ddl_statement, std::string& error_msg);

        // Grant/Revoke operations
        bool execute_grant_on_foreign_server(const std::string& ddl_statement, std::string& error_msg);
        bool execute_revoke_on_foreign_server(const std::string& ddl_statement, std::string& error_msg);
        bool execute_grant_on_database_link(const std::string& ddl_statement, std::string& error_msg);
        bool execute_revoke_on_database_link(const std::string& ddl_statement, std::string& error_msg);

        // Utility functions
        bool parse_ddl_options(const std::string& options_clause,
                              std::unordered_map<std::string, std::string>& options,
                              std::string& error_msg);
        std::string generate_create_statement(const std::string& object_type, const std::string& object_name);

      private:
        class Impl;
        std::unique_ptr<Impl> pImpl_;
        FdwCatalogManager& catalog_manager_;
    };

    /// FDW catalog information schema views
    class FdwInformationSchema
    {
      public:
        FdwInformationSchema(FdwCatalogManager& catalog_manager);
        ~FdwInformationSchema();

        // Information schema view structures
        struct ForeignDataWrapperInfo {
            std::string fdw_name;
            std::string fdw_version;
            std::string fdw_library;
            std::string capabilities;
            std::string description;
        };

        struct ForeignServerInfo {
            std::string server_name;
            std::string fdw_name;
            std::string server_type;
            std::string connection_string;
            std::string health_status;
            std::int32_t max_connections;
        };

        struct ForeignTableInfo {
            std::string table_name;
            std::string schema_name;
            std::string server_name;
            std::string remote_schema;
            std::string remote_table;
            std::int32_t column_count;
        };

        struct DatabaseLinkInfo {
            std::string link_name;
            std::string target_server;
            std::string connection_type;
            std::string health_status;
        };

        // Information schema queries
        std::vector<ForeignDataWrapperInfo> get_foreign_data_wrappers();
        std::vector<ForeignServerInfo> get_foreign_servers(const std::string& fdw_name = "");
        std::vector<ForeignTableInfo> get_foreign_tables(const std::string& schema_name = "");
        std::vector<DatabaseLinkInfo> get_database_links();

        // Column information
        struct ColumnInfo {
            std::string table_name;
            std::string column_name;
            std::string data_type;
            bool is_nullable;
            std::string default_value;
            std::int32_t position;
        };

        std::vector<ColumnInfo> get_foreign_table_columns(const std::string& table_name,
                                                          const std::string& schema_name = "");

        // Statistics information
        struct StatisticsInfo {
            std::string object_name;
            std::string stat_name;
            std::string stat_value;
            std::int64_t collection_time;
        };

        std::vector<StatisticsInfo> get_foreign_table_statistics(const std::string& table_name,
                                                                const std::string& schema_name = "");

      private:
        FdwCatalogManager& catalog_manager_;
    };

    /// FDW catalog query optimizer integration
    class FdwCatalogOptimizer
    {
      public:
        FdwCatalogOptimizer(FdwCatalogManager& catalog_manager);
        ~FdwCatalogOptimizer();

        // Cost estimation support
        struct TableStatistics {
            std::int64_t estimated_rows;
            std::int64_t estimated_size_bytes;
            double selectivity_factor;
            std::unordered_map<std::string, std::string> column_statistics;
        };

        bool get_table_statistics(const std::string& table_name, const std::string& schema_name,
                                 TableStatistics& stats, std::string& error_msg);

        // Index information for query planning
        struct IndexInfo {
            std::string index_name;
            std::string index_type;
            std::vector<std::string> key_columns;
            bool is_unique;
            bool is_primary;
        };

        std::vector<IndexInfo> get_foreign_table_indexes(const std::string& table_name,
                                                        const std::string& schema_name);

        // Query optimization hints
        bool get_pushdown_capabilities(const std::string& server_name, FdwCapability& capabilities,
                                      std::string& error_msg);
        double estimate_remote_query_cost(const std::string& server_name, const std::string& query,
                                         std::int64_t estimated_rows);

      private:
        FdwCatalogManager& catalog_manager_;
    };

} // namespace scratchbird::engine