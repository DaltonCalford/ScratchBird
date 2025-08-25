#include "scratchbird/engine/fdw_csv.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <regex>

namespace scratchbird::engine
{

    /// CsvResultIterator implementation
    CsvResultIterator::CsvResultIterator(const std::string& file_path, const CsvOptions& options)
        : options_(options)
    {
        file_.open(file_path, std::ios::in);
        if (!file_.is_open()) {
            eof_reached_ = true;
            return;
        }

        // Parse header if present
        if (options_.has_header) {
            if (!parse_header_line()) {
                eof_reached_ = true;
                return;
            }
            header_parsed_ = true;
        } else {
            // Generate default column names
            std::string line;
            if (std::getline(file_, line)) {
                std::vector<std::string> fields;
                if (parse_csv_line(line, fields)) {
                    for (std::size_t i = 0; i < fields.size(); ++i) {
                        column_names_.push_back("column" + std::to_string(i + 1));
                    }
                    // Reset file to beginning for data parsing
                    file_.clear();
                    file_.seekg(0, std::ios::beg);
                }
            }
            header_parsed_ = true;
        }

        // Infer column types by examining first few rows
        if (!eof_reached_) {
            infer_column_types();
        }
    }

    CsvResultIterator::~CsvResultIterator()
    {
        close();
    }

    bool CsvResultIterator::next()
    {
        if (eof_reached_ || !file_.is_open()) {
            return false;
        }

        std::string line;
        while (std::getline(file_, line)) {
            if (options_.skip_blank_lines && line.empty()) {
                continue;
            }

            if (parse_csv_line(line, current_row_)) {
                // Ensure we have the right number of columns
                current_row_.resize(column_names_.size());
                ++rows_processed_;
                return true;
            }
        }

        eof_reached_ = true;
        return false;
    }

    bool CsvResultIterator::is_null(std::size_t column_index) const
    {
        if (column_index >= current_row_.size()) {
            return true;
        }
        return current_row_[column_index] == options_.null_string;
    }

    std::string CsvResultIterator::get_string(std::size_t column_index) const
    {
        if (column_index >= current_row_.size()) {
            return "";
        }
        return current_row_[column_index];
    }

    std::int64_t CsvResultIterator::get_int64(std::size_t column_index) const
    {
        if (is_null(column_index)) {
            return 0;
        }
        try {
            return std::stoll(current_row_[column_index]);
        } catch (const std::exception&) {
            return 0;
        }
    }

    double CsvResultIterator::get_double(std::size_t column_index) const
    {
        if (is_null(column_index)) {
            return 0.0;
        }
        try {
            return std::stod(current_row_[column_index]);
        } catch (const std::exception&) {
            return 0.0;
        }
    }

    bool CsvResultIterator::get_bool(std::size_t column_index) const
    {
        if (is_null(column_index)) {
            return false;
        }
        std::string value = current_row_[column_index];
        std::transform(value.begin(), value.end(), value.begin(), ::tolower);
        return value == "true" || value == "1" || value == "yes" || value == "on";
    }

    std::vector<std::uint8_t> CsvResultIterator::get_bytes(std::size_t column_index) const
    {
        std::string str = get_string(column_index);
        return std::vector<std::uint8_t>(str.begin(), str.end());
    }

    std::size_t CsvResultIterator::get_column_count() const
    {
        return column_names_.size();
    }

    std::string CsvResultIterator::get_column_name(std::size_t column_index) const
    {
        if (column_index >= column_names_.size()) {
            return "";
        }
        return column_names_[column_index];
    }

    TypeKind CsvResultIterator::get_column_type(std::size_t column_index) const
    {
        if (column_index >= column_types_.size()) {
            return TypeKind::VarChar;
        }
        return column_types_[column_index];
    }

    std::uint64_t CsvResultIterator::get_rows_processed() const
    {
        return rows_processed_;
    }

    void CsvResultIterator::close()
    {
        if (file_.is_open()) {
            file_.close();
        }
    }

    bool CsvResultIterator::parse_header_line()
    {
        std::string line;
        if (!std::getline(file_, line)) {
            return false;
        }

        return parse_csv_line(line, column_names_);
    }

    bool CsvResultIterator::parse_csv_line(const std::string& line,
                                           std::vector<std::string>& fields)
    {
        fields.clear();

        if (line.empty()) {
            return false;
        }

        std::string current_field;
        bool in_quotes = false;
        bool escaped = false;

        for (std::size_t i = 0; i < line.length(); ++i) {
            char c = line[i];

            if (escaped) {
                current_field += c;
                escaped = false;
                continue;
            }

            if (c == options_.escape_char[0] && options_.escape_char.length() > 0) {
                escaped = true;
                continue;
            }

            if (c == options_.quote_char[0] && options_.quote_char.length() > 0) {
                in_quotes = !in_quotes;
                continue;
            }

            if (!in_quotes && c == options_.delimiter[0]) {
                fields.push_back(current_field);
                current_field.clear();
                continue;
            }

            current_field += c;
        }

        // Add the last field
        fields.push_back(current_field);

        return true;
    }

    TypeKind CsvResultIterator::infer_column_type(const std::string& value) const
    {
        if (value.empty() || value == options_.null_string) {
            return TypeKind::VarChar; // Default for null values
        }

        // Try integer
        try {
            std::stoll(value);
            return TypeKind::BigInt;
        } catch (const std::exception&) {
        }

        // Try double
        try {
            std::stod(value);
            return TypeKind::DoublePrecision;
        } catch (const std::exception&) {
        }

        // Check boolean
        std::string lower_value = value;
        std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);
        if (lower_value == "true" || lower_value == "false" || lower_value == "1" ||
            lower_value == "0" || lower_value == "yes" || lower_value == "no") {
            return TypeKind::Boolean;
        }

        // Default to varchar
        return TypeKind::VarChar;
    }

    void CsvResultIterator::infer_column_types()
    {
        column_types_.resize(column_names_.size(), TypeKind::VarChar);

        // Save current position
        std::streampos pos = file_.tellg();

        // Read first few lines to infer types
        const int sample_lines = 10;
        std::vector<std::vector<TypeKind>> type_samples(column_names_.size());

        for (int line_count = 0; line_count < sample_lines; ++line_count) {
            std::string line;
            if (!std::getline(file_, line)) {
                break;
            }

            std::vector<std::string> fields;
            if (parse_csv_line(line, fields)) {
                for (std::size_t i = 0; i < fields.size() && i < column_names_.size(); ++i) {
                    TypeKind type = infer_column_type(fields[i]);
                    type_samples[i].push_back(type);
                }
            }
        }

        // Determine most common type for each column
        for (std::size_t i = 0; i < column_names_.size(); ++i) {
            if (type_samples[i].empty()) {
                continue;
            }

            // Count type occurrences
            std::map<TypeKind, int> type_counts;
            for (TypeKind type : type_samples[i]) {
                type_counts[type]++;
            }

            // Find most common type
            TypeKind most_common = TypeKind::VarChar;
            int max_count = 0;
            for (const auto& [type, count] : type_counts) {
                if (count > max_count) {
                    max_count = count;
                    most_common = type;
                }
            }

            column_types_[i] = most_common;
        }

        // Restore file position
        file_.clear();
        file_.seekg(pos);
    }

    /// CsvForeignDataWrapper implementation
    CsvForeignDataWrapper::CsvForeignDataWrapper() {}

    std::string CsvForeignDataWrapper::get_name() const
    {
        return "csv_fdw";
    }

    std::string CsvForeignDataWrapper::get_version() const
    {
        return "1.0.0";
    }

    FdwCapability CsvForeignDataWrapper::get_capabilities() const
    {
        return FdwCapability::SelectSupport | FdwCapability::SchemaIntrospection;
    }

    bool CsvForeignDataWrapper::validate_server_config(const ForeignServerConfig& config,
                                                       std::string& error_msg)
    {
        if (config.fdw_name != "csv_fdw") {
            error_msg = "Invalid FDW name for CSV FDW: " + config.fdw_name;
            return false;
        }

        // CSV FDW doesn't require host/port/database
        return true;
    }

    bool CsvForeignDataWrapper::establish_connection(const ForeignServerConfig& server_config,
                                                     const UserMapping& user_mapping,
                                                     std::string& error_msg)
    {
        current_server_config_ = server_config;
        connected_ = true;
        return true;
    }

    void CsvForeignDataWrapper::close_connection()
    {
        connected_ = false;
    }

    bool CsvForeignDataWrapper::test_connection(std::string& error_msg)
    {
        return connected_;
    }

    bool CsvForeignDataWrapper::introspect_schema(const std::string& remote_schema,
                                                  RemoteSchemaInfo& schema_info,
                                                  std::string& error_msg)
    {
        if (!connected_) {
            error_msg = "Not connected to CSV FDW";
            return false;
        }

        // For CSV FDW, we'll look in a directory for CSV files
        std::string base_path = ".";

        // Check if base_path option is provided in server config
        auto it = current_server_config_.options.find("base_path");
        if (it != current_server_config_.options.end()) {
            base_path = it->second;
        }

        try {
            schema_info.schema_name = remote_schema;

            if (!std::filesystem::exists(base_path)) {
                error_msg = "Base path does not exist: " + base_path;
                return false;
            }

            // Find CSV files in the directory
            for (const auto& entry : std::filesystem::directory_iterator(base_path)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    if (filename.length() >= 4 &&
                        filename.substr(filename.length() - 4) == ".csv") {
                        std::string table_name = filename.substr(0, filename.length() - 4);
                        schema_info.table_names.push_back(table_name);

                        // Try to introspect columns from the first few lines
                        CsvOptions options = parse_csv_options(current_server_config_.options);
                        CsvResultIterator iterator(entry.path().string(), options);

                        std::vector<ColumnDef> columns;
                        for (std::size_t i = 0; i < iterator.get_column_count(); ++i) {
                            ColumnDef col;
                            col.name = iterator.get_column_name(i);
                            col.type.kind = iterator.get_column_type(i);
                            col.type.length = -1;
                            col.type.precision = -1;
                            col.type.scale = -1;
                            col.type.vector_dims = -1;
                            col.type.collation = "";
                            col.type.charset = "";
                            col.nullable = true; // CSV columns are generally nullable
                            columns.push_back(std::move(col));
                        }

                        schema_info.table_columns[table_name] = columns;
                    }
                }
            }

            return true;
        } catch (const std::exception& e) {
            error_msg = "Error introspecting CSV schema: ";
            error_msg += e.what();
            return false;
        }
    }

    bool CsvForeignDataWrapper::validate_foreign_table(const ForeignTableMetadata& table_metadata,
                                                       std::string& error_msg)
    {
        std::string file_path = get_file_path(table_metadata);
        if (!file_exists(file_path)) {
            error_msg = "CSV file not found: " + file_path;
            return false;
        }

        return true;
    }

    std::unique_ptr<ForeignResultIterator> CsvForeignDataWrapper::execute_select(
        const std::string& query, const std::vector<std::string>& parameters,
        const FdwExecutionContext& context, std::string& error_msg)
    {

        if (!connected_) {
            error_msg = "Not connected to CSV FDW";
            return nullptr;
        }

        // For now, we'll assume the query contains a table name
        // In a real implementation, this would parse the query properly

        // Extract table name from query (simplified)
        std::regex table_regex(R"(FROM\s+(\w+))", std::regex_constants::icase);
        std::smatch match;
        std::string table_name;

        if (std::regex_search(query, match, table_regex)) {
            table_name = match[1].str();
        } else {
            error_msg = "Could not extract table name from query";
            return nullptr;
        }

        // Get file path for table
        ForeignTableMetadata metadata;
        metadata.table_name = table_name;
        metadata.server_name = current_server_config_.server_name;

        std::string file_path = get_file_path(metadata);
        if (!file_exists(file_path)) {
            error_msg = "CSV file not found: " + file_path;
            return nullptr;
        }

        CsvOptions options = parse_csv_options(current_server_config_.options);
        auto iterator = std::make_unique<CsvResultIterator>(file_path, options);

        return std::move(iterator);
    }

    bool CsvForeignDataWrapper::execute_insert(const std::string& table_name,
                                               const std::vector<std::string>& column_names,
                                               const std::vector<std::vector<std::string>>& rows,
                                               const FdwExecutionContext& context,
                                               std::uint64_t& rows_affected, std::string& error_msg)
    {
        error_msg = "INSERT operation not supported by CSV FDW";
        return false;
    }

    bool CsvForeignDataWrapper::execute_update(const std::string& table_name,
                                               const std::vector<std::string>& column_names,
                                               const std::vector<std::string>& values,
                                               const std::string& where_clause,
                                               const std::vector<std::string>& where_parameters,
                                               const FdwExecutionContext& context,
                                               std::uint64_t& rows_affected, std::string& error_msg)
    {
        error_msg = "UPDATE operation not supported by CSV FDW";
        return false;
    }

    bool CsvForeignDataWrapper::execute_delete(const std::string& table_name,
                                               const std::string& where_clause,
                                               const std::vector<std::string>& where_parameters,
                                               const FdwExecutionContext& context,
                                               std::uint64_t& rows_affected, std::string& error_msg)
    {
        error_msg = "DELETE operation not supported by CSV FDW";
        return false;
    }

    bool CsvForeignDataWrapper::begin_transaction(const FdwExecutionContext& context,
                                                  std::string& error_msg)
    {
        error_msg = "Transactions not supported by CSV FDW";
        return false;
    }

    bool CsvForeignDataWrapper::commit_transaction(const FdwExecutionContext& context,
                                                   std::string& error_msg)
    {
        error_msg = "Transactions not supported by CSV FDW";
        return false;
    }

    bool CsvForeignDataWrapper::rollback_transaction(const FdwExecutionContext& context,
                                                     std::string& error_msg)
    {
        error_msg = "Transactions not supported by CSV FDW";
        return false;
    }

    bool CsvForeignDataWrapper::can_pushdown_where_clause(const std::string& where_clause) const
    {
        return false; // CSV FDW doesn't support pushdown
    }

    bool CsvForeignDataWrapper::can_pushdown_join(const std::string& join_condition) const
    {
        return false;
    }

    bool CsvForeignDataWrapper::can_pushdown_aggregate(const std::string& aggregate_expr) const
    {
        return false;
    }

    bool CsvForeignDataWrapper::can_pushdown_limit(std::int64_t limit, std::int64_t offset) const
    {
        return false;
    }

    double CsvForeignDataWrapper::estimate_scan_cost(const ForeignTableMetadata& table_metadata,
                                                     std::int64_t estimated_rows) const
    {
        // Simple cost model for CSV files
        std::string file_path = get_file_path(table_metadata);
        std::size_t file_rows = estimate_file_rows(file_path);

        // Cost = base cost + (rows * per-row cost)
        return 1.0 + (static_cast<double>(file_rows) * 0.01);
    }

    double CsvForeignDataWrapper::estimate_join_cost(const ForeignTableMetadata& left_table,
                                                     const ForeignTableMetadata& right_table,
                                                     std::int64_t estimated_rows) const
    {
        // CSV FDW doesn't support join pushdown, so cost is very high
        return 1000000.0;
    }

    CsvOptions CsvForeignDataWrapper::parse_csv_options(
        const std::unordered_map<std::string, std::string>& options) const
    {
        CsvOptions csv_opts;

        auto it = options.find("delimiter");
        if (it != options.end()) {
            csv_opts.delimiter = it->second;
        }

        it = options.find("quote_char");
        if (it != options.end()) {
            csv_opts.quote_char = it->second;
        }

        it = options.find("escape_char");
        if (it != options.end()) {
            csv_opts.escape_char = it->second;
        }

        it = options.find("has_header");
        if (it != options.end()) {
            csv_opts.has_header = (it->second == "true" || it->second == "1");
        }

        it = options.find("encoding");
        if (it != options.end()) {
            csv_opts.encoding = it->second;
        }

        it = options.find("null_string");
        if (it != options.end()) {
            csv_opts.null_string = it->second;
        }

        return csv_opts;
    }

    std::string
    CsvForeignDataWrapper::get_file_path(const ForeignTableMetadata& table_metadata) const
    {
        // Check if file_path is specified in table options
        auto it = table_metadata.options.find("file_path");
        if (it != table_metadata.options.end()) {
            return it->second;
        }

        // Check for base_path in server config
        std::string base_path = ".";
        auto server_it = current_server_config_.options.find("base_path");
        if (server_it != current_server_config_.options.end()) {
            base_path = server_it->second;
        }

        // Construct file path from table name
        std::string table_name = table_metadata.remote_table.empty() ? table_metadata.table_name
                                                                     : table_metadata.remote_table;

        return base_path + "/" + table_name + ".csv";
    }

    bool CsvForeignDataWrapper::file_exists(const std::string& file_path) const
    {
        return std::filesystem::exists(file_path) && std::filesystem::is_regular_file(file_path);
    }

    std::size_t CsvForeignDataWrapper::estimate_file_rows(const std::string& file_path) const
    {
        if (!file_exists(file_path)) {
            return 0;
        }

        try {
            std::ifstream file(file_path);
            std::size_t line_count = 0;
            std::string line;

            // Count lines in file
            while (std::getline(file, line)) {
                ++line_count;
            }

            // Subtract header line if present
            CsvOptions options = parse_csv_options(current_server_config_.options);
            if (options.has_header && line_count > 0) {
                --line_count;
            }

            return line_count;
        } catch (const std::exception&) {
            return 0;
        }
    }

} // namespace scratchbird::engine
