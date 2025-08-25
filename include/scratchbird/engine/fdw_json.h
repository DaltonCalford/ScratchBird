#pragma once

#include "scratchbird/engine/fdw.h"

#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace scratchbird::engine
{

    /// JSON-specific configuration options
    struct JsonOptions {
        std::string root_path = "$";           // JSONPath root for document iteration
        std::string encoding = "UTF-8";        // File encoding
        bool flatten_objects = false;          // Flatten nested objects to columns
        bool array_as_table = true;            // Treat JSON arrays as table rows
        std::string null_string = "null";      // String representation of null
        std::size_t max_depth = 10;            // Maximum nesting depth to process
        bool strict_mode = false;              // Strict JSON parsing vs lenient
    };

    /// Forward declaration for JSON document parser (to avoid JSON library headers)
    struct JsonDocument;

    /// JSON file result iterator
    class JsonResultIterator : public ForeignResultIterator
    {
      public:
        JsonResultIterator(const std::string& file_path, const JsonOptions& options);
        ~JsonResultIterator();

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
        JsonOptions options_;
        std::unique_ptr<JsonDocument> document_;
        std::vector<std::string> column_names_;
        std::vector<TypeKind> column_types_;
        std::vector<std::string> current_row_;
        std::uint64_t rows_processed_;
        std::size_t current_array_index_;
        bool parsed_;
        bool closed_;

        bool parse_json_file();
        bool extract_array_element(std::size_t index);
        void infer_schema_from_json();
        TypeKind json_value_to_type_kind(const std::string& json_value) const;
        std::string extract_json_value(const std::string& path) const;
        void flatten_json_object(const std::string& prefix = "");
    };

    /// JSON Foreign Data Wrapper implementation
    class JsonForeignDataWrapper : public ForeignDataWrapper
    {
      public:
        JsonForeignDataWrapper();
        ~JsonForeignDataWrapper() = default;

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

        // Transaction support (not supported for JSON files)
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
        bool connected_;

        JsonOptions parse_json_options(const std::unordered_map<std::string, std::string>& options) const;
        std::string get_file_path(const ForeignTableMetadata& table_metadata) const;
        bool file_exists(const std::string& file_path) const;
        std::size_t estimate_file_rows(const std::string& file_path) const;
        bool is_json_array_file(const std::string& file_path) const;
    };

} // namespace scratchbird::engine