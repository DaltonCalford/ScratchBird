#pragma once

#include "scratchbird/engine/fdw.h"

#include <fstream>
#include <sstream>

namespace scratchbird::engine
{

    /// CSV-specific configuration options
    struct CsvOptions {
        std::string delimiter = ",";
        std::string quote_char = "\"";
        std::string escape_char = "\\";
        bool has_header = true;
        std::string encoding = "UTF-8";
        bool skip_blank_lines = true;
        std::string null_string = "";
    };

    /// CSV file result iterator
    class CsvResultIterator : public ForeignResultIterator
    {
      public:
        CsvResultIterator(const std::string& file_path, const CsvOptions& options);
        ~CsvResultIterator();

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
        std::ifstream file_;
        CsvOptions options_;
        std::vector<std::string> column_names_;
        std::vector<TypeKind> column_types_;
        std::vector<std::string> current_row_;
        std::uint64_t rows_processed_ = 0;
        bool header_parsed_ = false;
        bool eof_reached_ = false;

        bool parse_header_line();
        bool parse_csv_line(const std::string& line, std::vector<std::string>& fields);
        TypeKind infer_column_type(const std::string& value) const;
        void infer_column_types();
    };

    /// CSV Foreign Data Wrapper implementation
    class CsvForeignDataWrapper : public ForeignDataWrapper
    {
      public:
        CsvForeignDataWrapper();
        ~CsvForeignDataWrapper() = default;

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

        // Transaction support (not supported for CSV files)
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
        ForeignServerConfig current_server_config_;
        bool connected_ = false;

        CsvOptions
        parse_csv_options(const std::unordered_map<std::string, std::string>& options) const;
        std::string get_file_path(const ForeignTableMetadata& table_metadata) const;
        bool file_exists(const std::string& file_path) const;
        std::size_t estimate_file_rows(const std::string& file_path) const;
    };

} // namespace scratchbird::engine
