#include "scratchbird/sblr/executor.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

namespace scratchbird
{
    namespace sblr
    {

        // ===== Value Implementation =====

        int64_t Value::toInt64() const
        {
            switch (type)
            {
                case INTEGER:
                    return std::get<int32_t>(data);
                case BIGINT:
                    return std::get<int64_t>(data);
                case DOUBLE:
                    return static_cast<int64_t>(std::get<double>(data));
                case BOOLEAN:
                    return std::get<bool>(data) ? 1 : 0;
                default:
                    return 0;
            }
        }

        double Value::toDouble() const
        {
            switch (type)
            {
                case INTEGER:
                    return std::get<int32_t>(data);
                case BIGINT:
                    return std::get<int64_t>(data);
                case DOUBLE:
                    return std::get<double>(data);
                case BOOLEAN:
                    return std::get<bool>(data) ? 1.0 : 0.0;
                default:
                    return 0.0;
            }
        }

        std::string Value::toString() const
        {
            switch (type)
            {
                case NULL_VALUE:
                    return "NULL";
                case INTEGER:
                    return std::to_string(std::get<int32_t>(data));
                case BIGINT:
                    return std::to_string(std::get<int64_t>(data));
                case DOUBLE:
                {
                    std::stringstream ss;
                    ss << std::fixed << std::setprecision(6) << std::get<double>(data);
                    return ss.str();
                }
                case STRING:
                    return std::get<std::string>(data);
                case BOOLEAN:
                    return std::get<bool>(data) ? "true" : "false";
                default:
                    return "";
            }
        }

        bool Value::toBoolean() const
        {
            switch (type)
            {
                case NULL_VALUE:
                    return false;
                case INTEGER:
                    return std::get<int32_t>(data) != 0;
                case BIGINT:
                    return std::get<int64_t>(data) != 0;
                case DOUBLE:
                    return std::get<double>(data) != 0.0;
                case STRING:
                    return !std::get<std::string>(data).empty();
                case BOOLEAN:
                    return std::get<bool>(data);
                default:
                    return false;
            }
        }

        // ===== ResultSet Implementation =====

        void ResultSet::addColumn(const std::string &name, core::DataType type)
        {
            column_names_.push_back(name);
            column_types_.push_back(type);
        }

        void ResultSet::addRow(std::vector<Value> row)
        {
            if (row.size() != column_names_.size())
            {
                throw std::runtime_error("Row column count mismatch");
            }
            rows_.push_back(std::move(row));
        }

        void ResultSet::print(std::ostream &out) const
        {
            // Print column headers
            for (size_t i = 0; i < column_names_.size(); i++)
            {
                if (i > 0)
                    out << " | ";
                out << std::setw(15) << column_names_[i];
            }
            out << "\n";

            // Print separator
            for (size_t i = 0; i < column_names_.size(); i++)
            {
                if (i > 0)
                    out << "-+-";
                out << std::string(15, '-');
            }
            out << "\n";

            // Print rows
            for (const auto &row : rows_)
            {
                for (size_t i = 0; i < row.size(); i++)
                {
                    if (i > 0)
                        out << " | ";
                    out << std::setw(15) << row[i].toString();
                }
                out << "\n";
            }

            out << "(" << rows_.size() << " rows)\n";
        }

        // ===== Executor Implementation =====

        Executor::Executor(core::Database *db) : db_(db), pc_(0)
        {
            if (!db_)
            {
                throw std::invalid_argument("Database pointer cannot be null");
            }
        }

        Executor::~Executor() = default;

        ExecutionResult Executor::execute(const std::vector<uint8_t> &bytecode)
        {
            // Reset execution state
            // IMPORTANT: bytecode_ stores a raw pointer to the input vector's data.
            // The caller MUST ensure the bytecode vector remains valid for the
            // duration of this execute() call. This is safe because we take a const
            // reference and complete execution within this function.
            bytecode_ = bytecode.data();
            bytecode_size_ = bytecode.size();
            pc_ = 0;

            // Clear stack efficiently by replacing with empty stack
            stack_ = std::stack<Value>();

            current_table_.clear();
            current_columns_.clear();
            current_result_set_.reset();

            try
            {
                // Check version
                if (readByte() != static_cast<uint8_t>(Opcode::VERSION))
                {
                    return ExecutionResult("Invalid bytecode: missing version");
                }

                uint8_t version = readByte();
                if (version != SBLR_VERSION)
                {
                    return ExecutionResult("Unsupported bytecode version: " +
                                           std::to_string(version));
                }

                // Execute main statement
                Opcode op = static_cast<Opcode>(readByte());
                switch (op)
                {
                    case Opcode::CREATE_TABLE:
                        executeCreateTable();
                        return ExecutionResult();

                    case Opcode::INSERT:
                        executeInsert();
                        return ExecutionResult();

                    case Opcode::SELECT:
                        executeSelect();
                        return ExecutionResult(std::move(current_result_set_));

                    default:
                        return ExecutionResult("Unknown statement opcode: " +
                                               std::to_string(static_cast<int>(op)));
                }
            }
            catch (const std::exception &e)
            {
                return ExecutionResult(std::string("Execution error: ") + e.what());
            }
        }

        uint8_t Executor::readByte()
        {
            if (pc_ >= bytecode_size_)
            {
                throw std::runtime_error("Bytecode underflow");
            }
            return bytecode_[pc_++];
        }

        uint32_t Executor::readInt32()
        {
            if (pc_ + 4 > bytecode_size_)
            {
                throw std::runtime_error("Bytecode underflow");
            }
            uint32_t value = sblr::readInt32(&bytecode_[pc_]);
            pc_ += 4;
            return value;
        }

        uint64_t Executor::readInt64()
        {
            if (pc_ + 8 > bytecode_size_)
            {
                throw std::runtime_error("Bytecode underflow");
            }
            uint64_t value = sblr::readInt64(&bytecode_[pc_]);
            pc_ += 8;
            return value;
        }

        double Executor::readDouble()
        {
            if (pc_ + 8 > bytecode_size_)
            {
                throw std::runtime_error("Bytecode underflow");
            }
            // Read as little-endian 64-bit value, then convert to double
            uint64_t bits = sblr::readInt64(&bytecode_[pc_]);
            double value;
            std::memcpy(&value, &bits, sizeof(double));
            pc_ += 8;
            return value;
        }

        std::string Executor::readString()
        {
            uint32_t length = readInt32();

            // Validate reasonable string length (prevent malicious huge allocations)
            // Maximum reasonable string: 16MB
            constexpr uint32_t MAX_STRING_LENGTH = 16 * 1024 * 1024;
            if (length > MAX_STRING_LENGTH)
            {
                throw std::runtime_error("String length exceeds maximum allowed (16MB)");
            }

            if (pc_ + length > bytecode_size_)
            {
                throw std::runtime_error("Bytecode underflow");
            }
            std::string str(reinterpret_cast<const char *>(&bytecode_[pc_]), length);
            pc_ += length;
            return str;
        }

        Value Executor::pop()
        {
            if (stack_.empty())
            {
                throw std::runtime_error("Stack underflow");
            }
            Value v = stack_.top();
            stack_.pop();
            return v;
        }

        // Helper: Convert parser::DataType to core::DataType
        static core::DataType convertDataType(Opcode type_opcode, uint32_t precision = 0)
        {
            switch (type_opcode)
            {
                case Opcode::TYPE_INTEGER:
                    return core::DataType::INT32;
                case Opcode::TYPE_BIGINT:
                    return core::DataType::INT64;
                case Opcode::TYPE_DOUBLE:
                    return core::DataType::FLOAT64;
                case Opcode::TYPE_VARCHAR:
                    return core::DataType::VARCHAR;
                default:
                    throw std::runtime_error("Unknown data type opcode");
            }
        }

        void Executor::executeCreateTable()
        {
            // Read TABLE_REF opcode
            if (readByte() != static_cast<uint8_t>(Opcode::TABLE_REF))
            {
                error("Expected TABLE_REF in CREATE TABLE");
            }

            std::string table_name = readString();

            // Read BEGIN_LIST opcode for columns
            if (readByte() != static_cast<uint8_t>(Opcode::BEGIN_LIST))
            {
                error("Expected BEGIN_LIST for columns");
            }

            uint32_t column_count = readInt32();

            // Read column definitions
            std::vector<core::CatalogManager::ColumnInfo> columns;

            for (uint32_t i = 0; i < column_count; i++)
            {
                // Read COLUMN_DEF opcode
                if (readByte() != static_cast<uint8_t>(Opcode::COLUMN_DEF))
                {
                    error("Expected COLUMN_DEF");
                }

                // Read COLUMN_REF (column name)
                if (readByte() != static_cast<uint8_t>(Opcode::COLUMN_REF))
                {
                    error("Expected COLUMN_REF in column definition");
                }
                std::string col_name = readString();

                // Read data type
                Opcode type_op = static_cast<Opcode>(readByte());
                uint32_t precision = 0;
                if (type_op == Opcode::TYPE_VARCHAR)
                {
                    precision = readInt32();
                }

                core::DataType col_type = convertDataType(type_op, precision);

                // Check for NOT_NULL constraint
                bool nullable = true;
                if (pc_ < bytecode_size_ &&
                    bytecode_[pc_] == static_cast<uint8_t>(Opcode::NOT_NULL))
                {
                    nullable = false;
                    readByte(); // Consume NOT_NULL opcode
                }

                // Build ColumnInfo (table_id and column_id will be set by catalog)
                core::CatalogManager::ColumnInfo col_info;
                col_info.column_name = col_name;
                col_info.data_type = static_cast<uint16_t>(col_type);
                col_info.max_length = precision;
                col_info.nullable = nullable;
                col_info.has_default = false;
                columns.push_back(col_info);
            }

            // Read END_LIST opcode
            if (readByte() != static_cast<uint8_t>(Opcode::END_LIST))
            {
                error("Expected END_LIST after columns");
            }

            // Get default schema (PUBLIC)
            core::CatalogManager::SchemaInfo schema_info;
            auto status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, nullptr);
            if (status != core::Status::OK)
            {
                error("Failed to get default schema");
            }

            // Create table in catalog
            core::ID table_id;
            status = db_->catalog_manager()->createTable(schema_info.schema_id, table_name, columns,
                                                          table_id, nullptr);
            if (status != core::Status::OK)
            {
                error("Failed to create table");
            }
        }

        void Executor::executeInsert()
        {
            // Read TABLE_REF opcode
            if (readByte() != static_cast<uint8_t>(Opcode::TABLE_REF))
            {
                error("Expected TABLE_REF in INSERT");
            }

            std::string table_name = readString();

            // Get default schema (PUBLIC)
            core::CatalogManager::SchemaInfo schema_info;
            auto status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, nullptr);
            if (status != core::Status::OK)
            {
                error("Failed to get default schema");
            }

            // Get table from catalog
            core::CatalogManager::TableInfo table_info;
            status = db_->catalog_manager()->getTable(schema_info.schema_id, table_name, table_info,
                                                       nullptr);
            if (status != core::Status::OK)
            {
                error("Table not found: " + table_name);
            }
            core::ID table_id = table_info.table_id;

            // Read column list
            if (readByte() != static_cast<uint8_t>(Opcode::BEGIN_LIST))
            {
                error("Expected BEGIN_LIST for columns");
            }

            uint32_t col_count = readInt32();
            std::vector<std::string> col_names;

            for (uint32_t i = 0; i < col_count; i++)
            {
                if (readByte() != static_cast<uint8_t>(Opcode::COLUMN_REF))
                {
                    error("Expected COLUMN_REF in column list");
                }
                col_names.push_back(readString());
            }

            if (readByte() != static_cast<uint8_t>(Opcode::END_LIST))
            {
                error("Expected END_LIST after column list");
            }

            // Read value list
            if (readByte() != static_cast<uint8_t>(Opcode::BEGIN_LIST))
            {
                error("Expected BEGIN_LIST for values");
            }

            uint32_t value_count = readInt32();
            if (value_count != col_count)
            {
                error("Column count doesn't match value count");
            }

            // Evaluate each expression and push to stack
            for (uint32_t i = 0; i < value_count; i++)
            {
                evaluateExpression();
            }

            if (readByte() != static_cast<uint8_t>(Opcode::END_LIST))
            {
                error("Expected END_LIST after value list");
            }

            // Pop values from stack in reverse order
            std::vector<Value> values;
            for (uint32_t i = 0; i < value_count; i++)
            {
                values.push_back(pop());
            }
            std::reverse(values.begin(), values.end());

            // Get column information to validate and serialize properly
            std::vector<core::CatalogManager::ColumnInfo> all_columns;
            auto status2 = db_->catalog_manager()->getColumns(table_id, all_columns, nullptr);
            if (status2 != core::Status::OK)
            {
                error("Failed to get table columns");
            }

            // Validate that columns exist and build column index map
            std::vector<size_t> col_indices;
            for (const auto &col_name_str : col_names)
            {
                auto it = std::find_if(all_columns.begin(), all_columns.end(),
                                        [&col_name_str](const auto &c)
                                        { return c.column_name == col_name_str; });

                if (it == all_columns.end())
                {
                    error("Column not found: " + col_name_str);
                }

                col_indices.push_back(std::distance(all_columns.begin(), it));
            }

            // Build tuple in binary format
            // Tuple format: TupleHeader + null bitmap (if needed) + column data
            std::vector<uint8_t> tuple_data;

            // Reserve space for TupleHeader
            size_t header_offset = tuple_data.size();
            tuple_data.resize(tuple_data.size() + sizeof(core::TupleHeader));

            // Determine if we need a null bitmap
            bool has_nulls = false;
            for (const auto &val : values)
            {
                if (val.isNull())
                {
                    has_nulls = true;
                    break;
                }
            }

            // Add null bitmap if needed (one bit per column)
            size_t null_bitmap_offset = 0;
            if (has_nulls)
            {
                null_bitmap_offset = tuple_data.size();
                size_t bitmap_bytes = (all_columns.size() + 7) / 8;
                tuple_data.resize(tuple_data.size() + bitmap_bytes);
                // Initialize bitmap to zero
                std::fill(tuple_data.begin() + null_bitmap_offset,
                          tuple_data.begin() + null_bitmap_offset + bitmap_bytes, 0);
            }

            // Serialize each column value
            for (size_t i = 0; i < values.size(); i++)
            {
                const auto &value = values[i];
                size_t col_idx = col_indices[i];
                const auto &col_info = all_columns[col_idx];

                if (value.isNull())
                {
                    // Set null bit in bitmap
                    size_t bit_offset = col_idx;
                    size_t byte_offset = null_bitmap_offset + (bit_offset / 8);
                    size_t bit_pos = bit_offset % 8;
                    tuple_data[byte_offset] |= (1 << bit_pos);
                    // Don't write any data for null values
                    continue;
                }

                // Serialize value based on column type
                core::DataType col_type = static_cast<core::DataType>(col_info.data_type);

                switch (col_type)
                {
                    case core::DataType::INT32:
                    {
                        int32_t val = static_cast<int32_t>(value.toInt64());
                        size_t offset = tuple_data.size();
                        tuple_data.resize(offset + sizeof(int32_t));
                        std::memcpy(&tuple_data[offset], &val, sizeof(int32_t));
                        break;
                    }
                    case core::DataType::INT64:
                    {
                        int64_t val = value.toInt64();
                        size_t offset = tuple_data.size();
                        tuple_data.resize(offset + sizeof(int64_t));
                        std::memcpy(&tuple_data[offset], &val, sizeof(int64_t));
                        break;
                    }
                    case core::DataType::FLOAT64:
                    {
                        double val = value.toDouble();
                        size_t offset = tuple_data.size();
                        tuple_data.resize(offset + sizeof(double));
                        std::memcpy(&tuple_data[offset], &val, sizeof(double));
                        break;
                    }
                    case core::DataType::VARCHAR:
                    {
                        std::string str = value.toString();
                        // Write length prefix (4 bytes) then data
                        uint32_t len = static_cast<uint32_t>(str.size());
                        size_t offset = tuple_data.size();
                        tuple_data.resize(offset + sizeof(uint32_t) + len);
                        std::memcpy(&tuple_data[offset], &len, sizeof(uint32_t));
                        std::memcpy(&tuple_data[offset + sizeof(uint32_t)], str.data(), len);
                        break;
                    }
                    default:
                        error("Unsupported column type for serialization");
                }
            }

            // Fill in TupleHeader
            auto *header = reinterpret_cast<core::TupleHeader *>(&tuple_data[header_offset]);
            header->xmin = 1; // Transaction ID (simplified - use 1 for now)
            header->xmax = 0; // Not deleted
            header->flags = has_nulls ? core::TupleHeader::FLAG_HAS_NULLS : 0;
            header->null_bitmap_offset =
                has_nulls ? static_cast<uint16_t>(null_bitmap_offset) : 0;

            // Insert tuple via storage engine
            uint32_t page_id;
            uint16_t item_id;
            auto insert_status = db_->storage_engine()->insertTuple(
                table_id, tuple_data.data(), static_cast<uint32_t>(tuple_data.size()), &page_id,
                &item_id, nullptr);

            if (insert_status != core::Status::OK)
            {
                error("Failed to insert tuple into storage");
            }

            // Success - tuple inserted
        }

        void Executor::executeSelect()
        {
            // Read select list
            if (readByte() != static_cast<uint8_t>(Opcode::BEGIN_LIST))
            {
                error("Expected BEGIN_LIST for select items");
            }

            uint32_t select_count = readInt32();
            bool is_select_star = false;
            std::vector<std::pair<std::string, std::string>> select_items; // (column_name, alias)

            for (uint32_t i = 0; i < select_count; i++)
            {
                Opcode op = static_cast<Opcode>(readByte());

                if (op == Opcode::SELECT_STAR)
                {
                    is_select_star = true;
                }
                else
                {
                    // For now, we only support simple column references
                    // Full expression evaluation would require row context
                    if (op != Opcode::COLUMN_REF)
                    {
                        error("Complex expressions in SELECT not yet supported");
                    }

                    std::string col_name = readString();
                    std::string alias;

                    // Check for optional alias
                    if (pc_ < bytecode_size_ &&
                        bytecode_[pc_] == static_cast<uint8_t>(Opcode::COLUMN_REF))
                    {
                        readByte(); // Consume COLUMN_REF
                        alias = readString();
                    }
                    else
                    {
                        alias = col_name; // Use column name as default
                    }

                    select_items.push_back({col_name, alias});
                }
            }

            if (readByte() != static_cast<uint8_t>(Opcode::END_LIST))
            {
                error("Expected END_LIST after select items");
            }

            // Read table reference
            if (readByte() != static_cast<uint8_t>(Opcode::TABLE_REF))
            {
                error("Expected TABLE_REF");
            }

            std::string table_name = readString();

            // Get default schema and table info
            core::CatalogManager::SchemaInfo schema_info;
            auto status = db_->catalog_manager()->getSchema("PUBLIC", schema_info, nullptr);
            if (status != core::Status::OK)
            {
                error("Failed to get default schema");
            }

            core::CatalogManager::TableInfo table_info;
            status = db_->catalog_manager()->getTable(schema_info.schema_id, table_name, table_info,
                                                       nullptr);
            if (status != core::Status::OK)
            {
                error("Table not found: " + table_name);
            }

            // Get column information
            std::vector<core::CatalogManager::ColumnInfo> all_columns;
            status = db_->catalog_manager()->getColumns(table_info.table_id, all_columns, nullptr);
            if (status != core::Status::OK)
            {
                error("Failed to get table columns");
            }

            // Build result set structure
            current_result_set_ = std::make_unique<ResultSet>();

            if (is_select_star)
            {
                // SELECT * - add all columns
                for (const auto &col : all_columns)
                {
                    current_result_set_->addColumn(col.column_name,
                                                    static_cast<core::DataType>(col.data_type));
                }
            }
            else
            {
                // Add selected columns
                for (const auto &[col_name, alias] : select_items)
                {
                    // Find column in table
                    auto it = std::find_if(all_columns.begin(), all_columns.end(),
                                            [&col_name](const auto &c)
                                            { return c.column_name == col_name; });

                    if (it == all_columns.end())
                    {
                        error("Column not found: " + col_name);
                    }

                    current_result_set_->addColumn(alias,
                                                    static_cast<core::DataType>(it->data_type));
                }
            }

            // Check for WHERE clause and save bytecode position
            size_t where_start_pc = 0;
            size_t where_end_pc = 0;
            bool has_where = false;

            if (pc_ < bytecode_size_ && bytecode_[pc_] == static_cast<uint8_t>(Opcode::WHERE_CLAUSE))
            {
                has_where = true;
                readByte(); // Consume WHERE_CLAUSE opcode
                where_start_pc = pc_;

                // Skip over WHERE expression to find end
                // We'll re-parse it for each row
                int depth = 1; // Track expression nesting
                while (pc_ < bytecode_size_ && depth > 0)
                {
                    Opcode op = static_cast<Opcode>(readByte());

                    // Literals push one value
                    if (op == Opcode::LITERAL_INT32)
                    {
                        pc_ += 4;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_INT64)
                    {
                        pc_ += 8;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_DOUBLE)
                    {
                        pc_ += 8;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_STRING || op == Opcode::COLUMN_REF)
                    {
                        uint32_t len = readInt32();
                        pc_ += len;
                        depth++;
                    }
                    else if (op == Opcode::LITERAL_NULL)
                    {
                        depth++;
                    }
                    // Binary operators consume 2, produce 1
                    else if (op >= Opcode::EXPR_ADD && op <= Opcode::EXPR_OR)
                    {
                        depth--; // Net effect: consume 2, produce 1 = -1
                    }
                }
                where_end_pc = pc_;
            }

            // Create table scan iterator
            auto scan_iter = db_->storage_engine()->createScan(table_info.table_id, nullptr);
            if (!scan_iter)
            {
                error("Failed to create table scan iterator");
            }

            // Scan all tuples
            core::Tuple tuple;
            while (scan_iter->next(&tuple, nullptr) == core::Status::OK)
            {
                // Deserialize tuple data
                std::vector<Value> row_values;
                if (!deserializeTuple(tuple.data, tuple.data_size, all_columns, row_values))
                {
                    continue; // Skip malformed tuples
                }

                // Evaluate WHERE clause if present
                if (has_where)
                {
                    // Save current PC and evaluate WHERE expression
                    size_t saved_pc = pc_;
                    pc_ = where_start_pc;

                    // Set up row context for column references
                    current_row_values_ = &row_values;
                    current_row_columns_ = &all_columns;

                    try
                    {
                        evaluateExpression();
                        Value where_result = pop();

                        current_row_values_ = nullptr;
                        current_row_columns_ = nullptr;

                        // Restore PC
                        pc_ = saved_pc;

                        // Check if WHERE clause evaluated to true
                        if (!where_result.toBoolean())
                        {
                            continue; // Skip this row
                        }
                    }
                    catch (...)
                    {
                        current_row_values_ = nullptr;
                        current_row_columns_ = nullptr;
                        pc_ = saved_pc;
                        throw;
                    }
                }

                // Project selected columns
                std::vector<Value> result_row;

                if (is_select_star)
                {
                    result_row = row_values;
                }
                else
                {
                    for (const auto &[col_name, alias] : select_items)
                    {
                        // Find column index
                        auto it = std::find_if(all_columns.begin(), all_columns.end(),
                                                [&col_name](const auto &c)
                                                { return c.column_name == col_name; });

                        if (it != all_columns.end())
                        {
                            size_t col_idx = std::distance(all_columns.begin(), it);
                            result_row.push_back(row_values[col_idx]);
                        }
                    }
                }

                current_result_set_->addRow(std::move(result_row));
            }
        }

        void Executor::evaluateExpression()
        {
            Opcode op = static_cast<Opcode>(readByte());

            switch (op)
            {
                case Opcode::LITERAL_NULL:
                    push(Value());
                    break;

                case Opcode::LITERAL_INT32:
                    push(Value(static_cast<int32_t>(readInt32())));
                    break;

                case Opcode::LITERAL_INT64:
                    push(Value(static_cast<int64_t>(readInt64())));
                    break;

                case Opcode::LITERAL_DOUBLE:
                    push(Value(readDouble()));
                    break;

                case Opcode::LITERAL_STRING:
                    push(Value(readString()));
                    break;

                case Opcode::COLUMN_REF:
                {
                    // Column reference - lookup value from current row context
                    std::string col_name = readString();

                    if (!current_row_values_ || !current_row_columns_)
                    {
                        error("Column reference outside of row context");
                    }

                    // Find column in current row
                    auto it = std::find_if(current_row_columns_->begin(), current_row_columns_->end(),
                                            [&col_name](const auto &c)
                                            { return c.column_name == col_name; });

                    if (it == current_row_columns_->end())
                    {
                        error("Column not found in row: " + col_name);
                    }

                    size_t col_idx = std::distance(current_row_columns_->begin(), it);
                    push((*current_row_values_)[col_idx]);
                    break;
                }

                // Arithmetic operators
                case Opcode::EXPR_ADD:
                case Opcode::EXPR_SUBTRACT:
                case Opcode::EXPR_MULTIPLY:
                case Opcode::EXPR_DIVIDE:
                case Opcode::EXPR_MODULO:
                // Comparison operators
                case Opcode::EXPR_EQ:
                case Opcode::EXPR_NE:
                case Opcode::EXPR_LT:
                case Opcode::EXPR_GT:
                case Opcode::EXPR_LE:
                case Opcode::EXPR_GE:
                // Logical operators
                case Opcode::EXPR_AND:
                case Opcode::EXPR_OR:
                    executeBinaryOp(op);
                    break;

                default:
                    error("Unknown expression opcode: " + std::to_string(static_cast<int>(op)));
            }
        }

        void Executor::executeBinaryOp(Opcode op)
        {
            // Pop right operand first (stack order)
            Value right = pop();
            Value left = pop();

            // Handle NULL propagation
            if (left.isNull() || right.isNull())
            {
                push(Value()); // NULL result
                return;
            }

            switch (op)
            {
                case Opcode::EXPR_ADD:
                {
                    if (left.type == Value::DOUBLE || right.type == Value::DOUBLE)
                        push(Value(left.toDouble() + right.toDouble()));
                    else
                        push(Value(left.toInt64() + right.toInt64()));
                    break;
                }
                case Opcode::EXPR_SUBTRACT:
                {
                    if (left.type == Value::DOUBLE || right.type == Value::DOUBLE)
                        push(Value(left.toDouble() - right.toDouble()));
                    else
                        push(Value(left.toInt64() - right.toInt64()));
                    break;
                }
                case Opcode::EXPR_MULTIPLY:
                {
                    if (left.type == Value::DOUBLE || right.type == Value::DOUBLE)
                        push(Value(left.toDouble() * right.toDouble()));
                    else
                        push(Value(left.toInt64() * right.toInt64()));
                    break;
                }
                case Opcode::EXPR_DIVIDE:
                {
                    if (right.toDouble() == 0.0)
                        error("Division by zero");
                    if (left.type == Value::DOUBLE || right.type == Value::DOUBLE)
                        push(Value(left.toDouble() / right.toDouble()));
                    else
                        push(Value(left.toInt64() / right.toInt64()));
                    break;
                }
                case Opcode::EXPR_MODULO:
                {
                    if (right.toInt64() == 0)
                        error("Modulo by zero");
                    push(Value(left.toInt64() % right.toInt64()));
                    break;
                }

                // Comparison operators
                case Opcode::EXPR_EQ:
                {
                    bool result;
                    if (left.type == Value::STRING || right.type == Value::STRING)
                        result = left.toString() == right.toString();
                    else if (left.type == Value::DOUBLE || right.type == Value::DOUBLE)
                        result = left.toDouble() == right.toDouble();
                    else
                        result = left.toInt64() == right.toInt64();
                    push(Value(result));
                    break;
                }
                case Opcode::EXPR_NE:
                {
                    bool result;
                    if (left.type == Value::STRING || right.type == Value::STRING)
                        result = left.toString() != right.toString();
                    else if (left.type == Value::DOUBLE || right.type == Value::DOUBLE)
                        result = left.toDouble() != right.toDouble();
                    else
                        result = left.toInt64() != right.toInt64();
                    push(Value(result));
                    break;
                }
                case Opcode::EXPR_LT:
                {
                    bool result;
                    if (left.type == Value::STRING || right.type == Value::STRING)
                        result = left.toString() < right.toString();
                    else if (left.type == Value::DOUBLE || right.type == Value::DOUBLE)
                        result = left.toDouble() < right.toDouble();
                    else
                        result = left.toInt64() < right.toInt64();
                    push(Value(result));
                    break;
                }
                case Opcode::EXPR_GT:
                {
                    bool result;
                    if (left.type == Value::STRING || right.type == Value::STRING)
                        result = left.toString() > right.toString();
                    else if (left.type == Value::DOUBLE || right.type == Value::DOUBLE)
                        result = left.toDouble() > right.toDouble();
                    else
                        result = left.toInt64() > right.toInt64();
                    push(Value(result));
                    break;
                }
                case Opcode::EXPR_LE:
                {
                    bool result;
                    if (left.type == Value::STRING || right.type == Value::STRING)
                        result = left.toString() <= right.toString();
                    else if (left.type == Value::DOUBLE || right.type == Value::DOUBLE)
                        result = left.toDouble() <= right.toDouble();
                    else
                        result = left.toInt64() <= right.toInt64();
                    push(Value(result));
                    break;
                }
                case Opcode::EXPR_GE:
                {
                    bool result;
                    if (left.type == Value::STRING || right.type == Value::STRING)
                        result = left.toString() >= right.toString();
                    else if (left.type == Value::DOUBLE || right.type == Value::DOUBLE)
                        result = left.toDouble() >= right.toDouble();
                    else
                        result = left.toInt64() >= right.toInt64();
                    push(Value(result));
                    break;
                }

                // Logical operators
                case Opcode::EXPR_AND:
                    push(Value(left.toBoolean() && right.toBoolean()));
                    break;
                case Opcode::EXPR_OR:
                    push(Value(left.toBoolean() || right.toBoolean()));
                    break;

                default:
                    error("Unknown binary operator: " + std::to_string(static_cast<int>(op)));
            }
        }

        void Executor::error(const std::string &msg)
        {
            throw std::runtime_error(msg);
        }

        bool Executor::deserializeTuple(const uint8_t *tuple_data, uint32_t tuple_size,
                                         const std::vector<core::CatalogManager::ColumnInfo> &columns,
                                         std::vector<Value> &values_out)
        {
            if (tuple_size < sizeof(core::TupleHeader))
            {
                return false; // Malformed tuple
            }

            // Read TupleHeader
            const auto *header = reinterpret_cast<const core::TupleHeader *>(tuple_data);

            // Check if tuple is deleted
            if (header->isDeleted())
            {
                return false;
            }

            // Get null bitmap if present
            const uint8_t *null_bitmap = nullptr;
            if (header->hasNulls() && header->null_bitmap_offset > 0 &&
                header->null_bitmap_offset < tuple_size)
            {
                null_bitmap = tuple_data + header->null_bitmap_offset;
            }

            // Read column data
            size_t data_offset = sizeof(core::TupleHeader);
            if (header->hasNulls() && null_bitmap)
            {
                // Skip past null bitmap
                size_t bitmap_bytes = (columns.size() + 7) / 8;
                data_offset = header->null_bitmap_offset + bitmap_bytes;
            }

            values_out.clear();
            values_out.reserve(columns.size());

            for (size_t i = 0; i < columns.size(); i++)
            {
                // Check if column is null
                if (null_bitmap)
                {
                    size_t byte_offset = i / 8;
                    size_t bit_pos = i % 8;
                    if (null_bitmap[byte_offset] & (1 << bit_pos))
                    {
                        values_out.push_back(Value()); // NULL value
                        continue;
                    }
                }

                // Deserialize value based on column type
                core::DataType col_type = static_cast<core::DataType>(columns[i].data_type);

                switch (col_type)
                {
                    case core::DataType::INT32:
                    {
                        if (data_offset + sizeof(int32_t) > tuple_size)
                            return false;

                        int32_t val;
                        std::memcpy(&val, tuple_data + data_offset, sizeof(int32_t));
                        values_out.push_back(Value(val));
                        data_offset += sizeof(int32_t);
                        break;
                    }
                    case core::DataType::INT64:
                    {
                        if (data_offset + sizeof(int64_t) > tuple_size)
                            return false;

                        int64_t val;
                        std::memcpy(&val, tuple_data + data_offset, sizeof(int64_t));
                        values_out.push_back(Value(val));
                        data_offset += sizeof(int64_t);
                        break;
                    }
                    case core::DataType::FLOAT64:
                    {
                        if (data_offset + sizeof(double) > tuple_size)
                            return false;

                        double val;
                        std::memcpy(&val, tuple_data + data_offset, sizeof(double));
                        values_out.push_back(Value(val));
                        data_offset += sizeof(double);
                        break;
                    }
                    case core::DataType::VARCHAR:
                    {
                        if (data_offset + sizeof(uint32_t) > tuple_size)
                            return false;

                        uint32_t len;
                        std::memcpy(&len, tuple_data + data_offset, sizeof(uint32_t));
                        data_offset += sizeof(uint32_t);

                        if (data_offset + len > tuple_size)
                            return false;

                        std::string str(reinterpret_cast<const char *>(tuple_data + data_offset), len);
                        values_out.push_back(Value(str));
                        data_offset += len;
                        break;
                    }
                    default:
                        return false; // Unsupported type
                }
            }

            return true;
        }

    } // namespace sblr
} // namespace scratchbird