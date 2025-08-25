#pragma once

#include "scratchbird/engine/fdw.h"

#include <memory>
#include <string>

namespace scratchbird::engine
{

    /// Forward declaration for PostgreSQL connection (to avoid libpq headers in public interface)
    struct PgConnection;

    /// PostgreSQL FDW result iterator
    class PostgreSqlResultIterator : public ForeignResultIterator
    {
      public:
        PostgreSqlResultIterator(std::shared_ptr<PgConnection> conn, const std::string& query);
        ~PostgreSqlResultIterator();

        bool next() override;
        bool is_null(std::size_t column_index) const override;
        std::string get_string(std::size_t column_index) const override;
        std::int64_t get_int64(std::size_t column_index) const override;
        double get_double(std::size_t column_index) const override;
        bool get_bool(std::size_t column_index) const override;
        std::vector<std::uint8_t> get_bytes(std::size_t column_index) const override;

        std::size_t get_column_count() const override;
        std::string get_column_name(std::size_t column_index) const override;
        TypeKind get_column_type(std::size_t column_index) const override;

        std::uint64_t get_rows_processed() const override;
        void close() override;

      private:
        std::shared_ptr<PgConnection> conn_;
        void* result_; // PGresult* (opaque to avoid libpq dependency)
        int current_row_;
        int total_rows_;
        std::uint64_t rows_processed_;
        std::vector<std::string> column_names_;
        std::vector<TypeKind> column_types_;
        bool executed_;
        bool closed_;

        void execute_query(const std::string& query);
        TypeKind map_postgresql_type(const std::string& pg_type) const;
    };

    /// PostgreSQL Foreign Data Wrapper implementation
    class PostgreSqlForeignDataWrapper : public ForeignDataWrapper
    {
      public:
        PostgreSqlForeignDataWrapper();
        ~PostgreSqlForeignDataWrapper();

        // Plugin metadata
        std::string get_name() const override;
        std::string get_version() const override;
        FdwCapability get_capabilities() const override;

        // Server management
        bool validate_server_config(const ForeignServerConfig& config,
                                    std::string& error_msg) override;
        bool establish_connection(const ForeignServerConfig& server_config,
                                  const UserMapping& user_mapping, std::string& error_msg) override;
        void close_connection() override;
        bool test_connection(std::string& error_msg) override;

        // Schema introspection
        bool introspect_schema(const std::string& remote_schema, RemoteSchemaInfo& schema_info,
                               std::string& error_msg) override;
        bool validate_foreign_table(const ForeignTableMetadata& table_metadata,
                                    std::string& error_msg) override;

        // Query operations
        std::unique_ptr<ForeignResultIterator>
        execute_select(const std::string& query, const std::vector<std::string>& parameters,
                       const FdwExecutionContext& context, std::string& error_msg) override;

        bool execute_insert(const std::string& table_name,
                            const std::vector<std::string>& column_names,
                            const std::vector<std::vector<std::string>>& rows,
                            const FdwExecutionContext& context, std::uint64_t& rows_affected,
                            std::string& error_msg) override;

        bool execute_update(const std::string& table_name,
                            const std::vector<std::string>& column_names,
                            const std::vector<std::string>& values, const std::string& where_clause,
                            const std::vector<std::string>& where_parameters,
                            const FdwExecutionContext& context, std::uint64_t& rows_affected,
                            std::string& error_msg) override;

        bool execute_delete(const std::string& table_name, const std::string& where_clause,
                            const std::vector<std::string>& where_parameters,
                            const FdwExecutionContext& context, std::uint64_t& rows_affected,
                            std::string& error_msg) override;

        // Transaction support
        bool begin_transaction(const FdwExecutionContext& context, std::string& error_msg) override;
        bool commit_transaction(const FdwExecutionContext& context,
                                std::string& error_msg) override;
        bool rollback_transaction(const FdwExecutionContext& context,
                                  std::string& error_msg) override;

        // Optimization support
        bool can_pushdown_where_clause(const std::string& where_clause) const override;
        bool can_pushdown_join(const std::string& join_condition) const override;
        bool can_pushdown_aggregate(const std::string& aggregate_expr) const override;
        bool can_pushdown_limit(std::int64_t limit, std::int64_t offset) const override;

        // Cost estimation
        double estimate_scan_cost(const ForeignTableMetadata& table_metadata,
                                  std::int64_t estimated_rows) const override;
        double estimate_join_cost(const ForeignTableMetadata& left_table,
                                  const ForeignTableMetadata& right_table,
                                  std::int64_t estimated_rows) const override;

      private:
        std::shared_ptr<PgConnection> connection_;
        ForeignServerConfig current_server_config_;
        UserMapping current_user_mapping_;
        bool connected_;
        bool in_transaction_;

        std::shared_ptr<PgConnection> create_connection(const ForeignServerConfig& server_config,
                                                        const UserMapping& user_mapping,
                                                        std::string& error_msg);
        bool test_postgresql_connection(std::shared_ptr<PgConnection> conn, std::string& error_msg);
        std::string build_connection_string(const ForeignServerConfig& server_config,
                                            const UserMapping& user_mapping) const;
        std::string escape_sql_identifier(const std::string& identifier) const;
        std::string escape_sql_string(const std::string& value) const;
    };

} // namespace scratchbird::engine