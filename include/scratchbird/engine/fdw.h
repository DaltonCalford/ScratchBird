#pragma once

#include "scratchbird/engine/executor_nodes.h"
#include "scratchbird/engine/types.h"

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    /// Forward declarations
    class ExecutorContext;
    class Catalog;

    /// Column definition for foreign tables
    struct ColumnDef {
        std::string name;
        TypeDescriptor type;
        bool nullable = true;
        std::string default_value;
        std::string comment;
    };

    /// FDW capability flags
    enum class FdwCapability : std::uint32_t {
        None = 0,
        SelectSupport = 1 << 0,
        InsertSupport = 1 << 1,
        UpdateSupport = 1 << 2,
        DeleteSupport = 1 << 3,
        WhereClausePushdown = 1 << 4,
        JoinPushdown = 1 << 5,
        AggregatePushdown = 1 << 6,
        LimitPushdown = 1 << 7,
        TransactionSupport = 1 << 8,
        BulkOperations = 1 << 9,
        SchemaIntrospection = 1 << 10,
        AsyncOperations = 1 << 11
    };

    inline FdwCapability operator|(FdwCapability a, FdwCapability b)
    {
        return static_cast<FdwCapability>(static_cast<std::uint32_t>(a) |
                                          static_cast<std::uint32_t>(b));
    }

    inline FdwCapability operator&(FdwCapability a, FdwCapability b)
    {
        return static_cast<FdwCapability>(static_cast<std::uint32_t>(a) &
                                          static_cast<std::uint32_t>(b));
    }

    inline bool has_capability(FdwCapability capabilities, FdwCapability flag)
    {
        return (capabilities & flag) == flag;
    }

    /// Foreign server configuration
    struct ForeignServerConfig {
        std::string server_name;
        std::string fdw_name;
        std::string host;
        std::uint16_t port = 0;
        std::string database;
        std::unordered_map<std::string, std::string> options;
        bool use_ssl = false;
        std::string ssl_cert_path;
        std::string ssl_key_path;
        std::string ssl_ca_path;
    };

    /// User mapping credentials
    struct UserMapping {
        std::string local_username;
        std::string remote_username;
        std::string remote_password;
        std::unordered_map<std::string, std::string> options;
    };

    /// Foreign table metadata
    struct ForeignTableMetadata {
        std::string table_name;
        std::string server_name;
        std::vector<ColumnDef> columns;
        std::unordered_map<std::string, std::string> options;
        std::string remote_schema;
        std::string remote_table;
    };

    /// Schema introspection result
    struct RemoteSchemaInfo {
        std::string schema_name;
        std::vector<std::string> table_names;
        std::unordered_map<std::string, std::vector<ColumnDef>> table_columns;
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> table_options;
    };

    /// Query execution context for FDW operations
    struct FdwExecutionContext {
        ExecutorContext* executor_ctx = nullptr;
        std::string query_id;
        bool in_transaction = false;
        std::uint64_t transaction_id = 0;
        std::unordered_map<std::string, std::string> session_options;
    };

    /// Result iterator for foreign queries
    class ForeignResultIterator
    {
      public:
        virtual ~ForeignResultIterator() = default;

        virtual bool next() = 0;
        virtual bool is_null(std::size_t column_index) const = 0;
        virtual std::string get_string(std::size_t column_index) const = 0;
        virtual std::int64_t get_int64(std::size_t column_index) const = 0;
        virtual double get_double(std::size_t column_index) const = 0;
        virtual bool get_bool(std::size_t column_index) const = 0;
        virtual std::vector<std::uint8_t> get_bytes(std::size_t column_index) const = 0;

        virtual std::size_t get_column_count() const = 0;
        virtual std::string get_column_name(std::size_t column_index) const = 0;
        virtual TypeKind get_column_type(std::size_t column_index) const = 0;

        virtual std::uint64_t get_rows_processed() const = 0;
        virtual void close() = 0;
    };

    /// Foreign Data Wrapper base interface
    class ForeignDataWrapper
    {
      public:
        virtual ~ForeignDataWrapper() = default;

        /// Plugin metadata
        virtual std::string get_name() const = 0;
        virtual std::string get_version() const = 0;
        virtual FdwCapability get_capabilities() const = 0;

        /// Server management
        virtual bool validate_server_config(const ForeignServerConfig& config,
                                            std::string& error_msg) = 0;
        virtual bool establish_connection(const ForeignServerConfig& server_config,
                                          const UserMapping& user_mapping,
                                          std::string& error_msg) = 0;
        virtual void close_connection() = 0;
        virtual bool test_connection(std::string& error_msg) = 0;

        /// Schema introspection
        virtual bool introspect_schema(const std::string& remote_schema,
                                       RemoteSchemaInfo& schema_info, std::string& error_msg) = 0;
        virtual bool validate_foreign_table(const ForeignTableMetadata& table_metadata,
                                            std::string& error_msg) = 0;

        /// Query operations
        virtual std::unique_ptr<ForeignResultIterator>
        execute_select(const std::string& query, const std::vector<std::string>& parameters,
                       const FdwExecutionContext& context, std::string& error_msg) = 0;

        virtual bool execute_insert(const std::string& table_name,
                                    const std::vector<std::string>& column_names,
                                    const std::vector<std::vector<std::string>>& rows,
                                    const FdwExecutionContext& context,
                                    std::uint64_t& rows_affected, std::string& error_msg) = 0;

        virtual bool execute_update(const std::string& table_name,
                                    const std::vector<std::string>& column_names,
                                    const std::vector<std::string>& values,
                                    const std::string& where_clause,
                                    const std::vector<std::string>& where_parameters,
                                    const FdwExecutionContext& context,
                                    std::uint64_t& rows_affected, std::string& error_msg) = 0;

        virtual bool execute_delete(const std::string& table_name, const std::string& where_clause,
                                    const std::vector<std::string>& where_parameters,
                                    const FdwExecutionContext& context,
                                    std::uint64_t& rows_affected, std::string& error_msg) = 0;

        /// Transaction support
        virtual bool begin_transaction(const FdwExecutionContext& context,
                                       std::string& error_msg) = 0;
        virtual bool commit_transaction(const FdwExecutionContext& context,
                                        std::string& error_msg) = 0;
        virtual bool rollback_transaction(const FdwExecutionContext& context,
                                          std::string& error_msg) = 0;

        /// Optimization support
        virtual bool can_pushdown_where_clause(const std::string& where_clause) const = 0;
        virtual bool can_pushdown_join(const std::string& join_condition) const = 0;
        virtual bool can_pushdown_aggregate(const std::string& aggregate_expr) const = 0;
        virtual bool can_pushdown_limit(std::int64_t limit, std::int64_t offset) const = 0;

        /// Cost estimation
        virtual double estimate_scan_cost(const ForeignTableMetadata& table_metadata,
                                          std::int64_t estimated_rows) const = 0;
        virtual double estimate_join_cost(const ForeignTableMetadata& left_table,
                                          const ForeignTableMetadata& right_table,
                                          std::int64_t estimated_rows) const = 0;
    };

    /// FDW plugin registration and management
    class FdwRegistry
    {
      public:
        static FdwRegistry& instance();

        /// Plugin management
        bool register_fdw(const std::string& name, std::unique_ptr<ForeignDataWrapper> fdw);
        bool unregister_fdw(const std::string& name);

        ForeignDataWrapper* get_fdw(const std::string& name);
        std::vector<std::string> list_fdw_names() const;

        /// Dynamic library loading
        bool load_fdw_plugin(const std::string& library_path, std::string& error_msg);
        bool unload_fdw_plugin(const std::string& name);

      private:
        FdwRegistry() = default;
        std::unordered_map<std::string, std::unique_ptr<ForeignDataWrapper>> fdw_implementations_;
        std::unordered_map<std::string, void*> loaded_libraries_;
    };

    /// FDW manager for coordinating foreign operations
    class FdwManager
    {
      public:
        FdwManager(Catalog* catalog);
        ~FdwManager();

        /// Server management
        bool create_foreign_server(const ForeignServerConfig& config, std::string& error_msg);
        bool drop_foreign_server(const std::string& server_name, std::string& error_msg);
        bool get_foreign_server(const std::string& server_name, ForeignServerConfig& config);
        std::vector<std::string> list_foreign_servers() const;

        /// User mapping management
        bool create_user_mapping(const std::string& server_name, const UserMapping& mapping,
                                 std::string& error_msg);
        bool drop_user_mapping(const std::string& server_name, const std::string& username,
                               std::string& error_msg);
        bool get_user_mapping(const std::string& server_name, const std::string& username,
                              UserMapping& mapping);

        /// Foreign table management
        bool create_foreign_table(const ForeignTableMetadata& table_metadata,
                                  std::string& error_msg);
        bool drop_foreign_table(const std::string& table_name, std::string& error_msg);
        bool get_foreign_table_metadata(const std::string& table_name,
                                        ForeignTableMetadata& metadata);

        /// Schema import
        bool import_foreign_schema(const std::string& server_name, const std::string& remote_schema,
                                   const std::string& local_schema,
                                   const std::vector<std::string>& table_filter,
                                   std::string& error_msg);

        /// Query execution
        std::unique_ptr<ForeignResultIterator>
        execute_foreign_query(const std::string& table_name, const std::string& query,
                              const std::vector<std::string>& parameters,
                              const FdwExecutionContext& context, std::string& error_msg);

        /// Connection management
        bool test_foreign_server_connection(const std::string& server_name, std::string& error_msg);
        void close_all_connections();

      private:
        Catalog* catalog_;
        std::unordered_map<std::string, std::unique_ptr<ForeignDataWrapper>> active_fdws_;

        bool load_fdw_for_server(const std::string& server_name, std::string& error_msg);
        ForeignDataWrapper* get_fdw_for_table(const std::string& table_name);
    };

} // namespace scratchbird::engine
