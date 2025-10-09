#include "scratchbird/sblr/executor.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/charset.h"
#include "scratchbird/core/timezone.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/connection_context.h"
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstring>
#include <algorithm>
#include <chrono>

namespace scratchbird
{
    namespace sblr
    {

        // ===== Value Implementation =====
        // Value is now an alias for core::TypedValue, so no implementation needed here

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

            // Statement snapshot management for READ_COMMITTED_READ_CONSISTENCY
            core::ConnectionContext* conn_ctx = core::ConnectionContext::getCurrent();
            bool created_stmt_snapshot = false;

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

                // Create statement snapshot for READ_COMMITTED_READ_CONSISTENCY
                if (conn_ctx && conn_ctx->getIsolationLevel() == core::IsolationLevel::READ_COMMITTED_READ_CONSISTENCY)
                {
                    core::ErrorContext err_ctx;
                    core::Status status = conn_ctx->createStatementSnapshot(&err_ctx);
                    if (status == core::Status::OK)
                    {
                        created_stmt_snapshot = true;
                    }
                    // Non-fatal if snapshot creation fails - fall back to READ COMMITTED semantics
                }

                // Execute main statement
                Opcode op = static_cast<Opcode>(readByte());
                ExecutionResult result;

                switch (op)
                {
                    case Opcode::CREATE_TABLE:
                        executeCreateTable();
                        result = ExecutionResult();
                        break;

                    case Opcode::INSERT:
                        executeInsert();
                        result = ExecutionResult();
                        break;

                    case Opcode::SELECT:
                        executeSelect();
                        result = ExecutionResult(std::move(current_result_set_));
                        break;

                    default:
                        result = ExecutionResult("Unknown statement opcode: " +
                                               std::to_string(static_cast<int>(op)));
                        break;
                }

                // Clear statement snapshot after successful execution
                if (created_stmt_snapshot && conn_ctx)
                {
                    conn_ctx->clearStatementSnapshot();
                }

                return result;
            }
            catch (const std::exception &e)
            {
                // Clear statement snapshot on error
                if (created_stmt_snapshot && conn_ctx)
                {
                    conn_ctx->clearStatementSnapshot();
                }
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

        uint16_t Executor::readInt16()
        {
            if (pc_ + 2 > bytecode_size_)
            {
                throw std::runtime_error("Bytecode underflow");
            }
            uint16_t value = sblr::readInt16(&bytecode_[pc_]);
            pc_ += 2;
            return value;
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
                // Integer types
                case Opcode::TYPE_INT8:
                    return core::DataType::INT8;
                case Opcode::TYPE_INT16:
                    return core::DataType::INT16;
                case Opcode::TYPE_INTEGER:
                    return core::DataType::INT32;
                case Opcode::TYPE_BIGINT:
                    return core::DataType::INT64;

                // Floating point types
                case Opcode::TYPE_FLOAT32:
                    return core::DataType::FLOAT32;
                case Opcode::TYPE_DOUBLE:
                    return core::DataType::FLOAT64;

                // Boolean
                case Opcode::TYPE_BOOLEAN:
                    return core::DataType::BOOLEAN;

                // String types
                case Opcode::TYPE_CHAR:
                    return core::DataType::CHAR;
                case Opcode::TYPE_VARCHAR:
                    return core::DataType::VARCHAR;
                case Opcode::TYPE_TEXT:
                    return core::DataType::TEXT;

                // Date/Time types
                case Opcode::TYPE_DATE:
                    return core::DataType::DATE;
                case Opcode::TYPE_TIME:
                    return core::DataType::TIME;
                case Opcode::TYPE_TIMESTAMP:
                    return core::DataType::TIMESTAMP;

                // Binary types
                case Opcode::TYPE_BINARY:
                    return core::DataType::BINARY;
                case Opcode::TYPE_VARBINARY:
                    return core::DataType::VARBINARY;
                case Opcode::TYPE_BLOB:
                    return core::DataType::BLOB;
                case Opcode::TYPE_BYTEA:
                    return core::DataType::BYTEA;

                // Other types
                case Opcode::TYPE_UUID:
                    return core::DataType::UUID;
                case Opcode::TYPE_DECIMAL:
                    return core::DataType::DECIMAL;
                case Opcode::TYPE_JSON:
                    return core::DataType::JSON;

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
            // Format: TupleHeader + null bitmap (if needed) + column data
            // HeapPage will overwrite some TupleHeader fields (xmin, xmax, ctid, etc.)
            std::vector<uint8_t> tuple_data;

            // Reserve space for TupleHeader (HeapPage expects it)
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

            // Initialize TupleHeader (HeapPage will overwrite xmin, xmax, ctid later)
            auto *header = reinterpret_cast<core::TupleHeader *>(&tuple_data[header_offset]);
            // Initialize all fields to zero first
            std::memset(header, 0, sizeof(core::TupleHeader));

            // Set the fields we know about
            header->infomask = has_nulls ? core::TupleHeader::HEAP_HAS_NULLS : 0;
            header->null_bitmap_offset = has_nulls ? static_cast<uint16_t>(null_bitmap_offset) : 0;

            // HeapPage::insertTuple() will set:
            // - xmin (from transaction manager)
            // - xmax = 0
            // - next_version_tid = 0
            // - ctid_page, ctid_item (from final item position)

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

            // Check if this is a monitoring/system table (MON_ prefix)
            // Note: Using MON_ instead of MON$ because $ is not supported in identifiers yet
            if (table_name.size() >= 4 && table_name.substr(0, 4) == "MON_")
            {
                executeMonitoringQuery(table_name);
                return;
            }

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
                    // CAST consumes 1, produces 1, plus reads try_cast flag and type
                    else if (op == Opcode::EXPR_CAST)
                    {
                        // Skip try_cast flag (1 byte)
                        readByte();
                        // Read and skip type opcode
                        Opcode type_op = static_cast<Opcode>(readByte());
                        if (type_op == Opcode::TYPE_VARCHAR)
                        {
                            pc_ += 4; // Skip precision
                        }
                        // depth unchanged (consume 1, produce 1)
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

        void Executor::executeMonitoringQuery(const std::string &table_name)
        {
            // Handle monitoring/system table queries (MON_ tables)
            current_result_set_ = std::make_unique<ResultSet>();

            if (table_name == "MON_DATABASE")
            {
                // Add columns for transaction markers
                current_result_set_->addColumn("MON$DATABASE_NAME", core::DataType::VARCHAR);
                current_result_set_->addColumn("MON$NEXT_TRANSACTION", core::DataType::INT64);
                current_result_set_->addColumn("MON$OLDEST_TRANSACTION", core::DataType::INT64);
                current_result_set_->addColumn("MON$OLDEST_ACTIVE", core::DataType::INT64);
                current_result_set_->addColumn("MON$OLDEST_SNAPSHOT", core::DataType::INT64);

                // Get transaction markers from transaction manager
                auto txn_mgr = db_->transaction_manager();
                uint64_t next_xid = txn_mgr->getCurrentXid();
                uint64_t oit = txn_mgr->getOldestXid();
                uint64_t oat = txn_mgr->getOldestActiveXid();
                uint64_t ost = txn_mgr->getOldestSnapshot();

                // Create the result row
                std::vector<Value> row;
                row.push_back(Value::makeVarchar("SCRATCHBIRD"));
                row.push_back(Value::makeInt64(static_cast<int64_t>(next_xid)));
                row.push_back(Value::makeInt64(static_cast<int64_t>(oit)));
                row.push_back(Value::makeInt64(static_cast<int64_t>(oat)));
                row.push_back(Value::makeInt64(static_cast<int64_t>(ost)));

                current_result_set_->addRow(std::move(row));
            }
            else
            {
                error("Unknown monitoring table: " + table_name);
            }
        }

        void Executor::evaluateExpression()
        {
            Opcode op = static_cast<Opcode>(readByte());

            switch (op)
            {
                case Opcode::LITERAL_NULL:
                    push(Value::makeNull());
                    break;

                case Opcode::LITERAL_INT32:
                    push(Value::makeInt32(static_cast<int32_t>(readInt32())));
                    break;

                case Opcode::LITERAL_INT64:
                    push(Value::makeInt64(static_cast<int64_t>(readInt64())));
                    break;

                case Opcode::LITERAL_DOUBLE:
                    push(Value::makeFloat64(readDouble()));
                    break;

                case Opcode::LITERAL_STRING:
                    push(Value::makeVarchar(readString()));
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

                // Type conversion
                case Opcode::EXPR_CAST:
                {
                    // Read try_cast flag
                    bool is_try_cast = readByte() != 0;

                    // Read target type
                    Opcode type_op = static_cast<Opcode>(readByte());
                    core::DataType target_type = core::DataType::UNKNOWN;
                    uint32_t precision = 0;

                    switch (type_op)
                    {
                        case Opcode::TYPE_INTEGER:
                            target_type = core::DataType::INT32;
                            break;
                        case Opcode::TYPE_BIGINT:
                            target_type = core::DataType::INT64;
                            break;
                        case Opcode::TYPE_DOUBLE:
                            target_type = core::DataType::FLOAT64;
                            break;
                        case Opcode::TYPE_VARCHAR:
                            target_type = core::DataType::VARCHAR;
                            precision = readInt32();
                            break;
                        default:
                            error("Unknown type in CAST");
                    }

                    // Pop value to cast (Value is already TypedValue)
                    Value value = pop();

                    // Perform cast using TypedValue conversion
                    auto converted = value.convertTo(target_type);

                    if (!converted)
                    {
                        if (is_try_cast)
                        {
                            // TRY_CAST returns NULL on failure
                            push(Value::makeNull());
                        }
                        else
                        {
                            // CAST throws error on failure
                            error("Failed to cast value to target type");
                        }
                    }
                    else
                    {
                        // Push converted value
                        push(*converted);
                    }
                    break;
                }

                // String functions
                case Opcode::FUNC_LENGTH:
                {
                    // LENGTH returns character count (charset-aware)
                    uint8_t arg_count = readByte();
                    if (arg_count != 1)
                    {
                        error("LENGTH expects 1 argument, got " + std::to_string(arg_count));
                    }

                    Value arg = pop();
                    if (arg.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        std::string str = arg.toString();
                        // Default to UTF-8 for string values
                        uint32_t char_len = charset_manager_.getCharLength(
                            reinterpret_cast<const uint8_t*>(str.data()),
                            str.length(),
                            core::CharacterSet::UTF8
                        );
                        push(Value::makeInt32(static_cast<int32_t>(char_len)));
                    }
                    break;
                }

                case Opcode::FUNC_SUBSTRING:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 3)
                    {
                        error("SUBSTRING expects 3 arguments, got " + std::to_string(arg_count));
                    }

                    // Pop args in reverse order (length, start, str)
                    Value length_val = pop();
                    Value start_val = pop();
                    Value str_val = pop();

                    if (str_val.isNull() || start_val.isNull() || length_val.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        std::string str = str_val.toString();
                        int32_t char_start = static_cast<int32_t>(start_val.toInt64());
                        int32_t char_length = static_cast<int32_t>(length_val.toInt64());

                        // SQL uses 1-based indexing
                        if (char_start < 1)
                            char_start = 1;
                        char_start--; // Convert to 0-based

                        const uint8_t* str_bytes = reinterpret_cast<const uint8_t*>(str.data());
                        uint32_t total_chars = charset_manager_.getCharLength(
                            str_bytes, str.length(), core::CharacterSet::UTF8);

                        if (char_start >= static_cast<int32_t>(total_chars) || char_length <= 0)
                        {
                            push(Value::makeVarchar(""));
                        }
                        else
                        {
                            // Find byte offset for start position
                            uint32_t byte_start = core::utf8::byte_length(str_bytes, char_start);

                            // Find byte length for the substring
                            uint32_t remaining_chars = std::min(
                                static_cast<uint32_t>(char_length),
                                total_chars - char_start
                            );
                            uint32_t byte_length = core::utf8::byte_length(
                                str_bytes + byte_start, remaining_chars);

                            std::string result = str.substr(byte_start, byte_length);
                            push(Value::makeVarchar(result));
                        }
                    }
                    break;
                }

                case Opcode::FUNC_UPPER:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 1)
                    {
                        error("UPPER expects 1 argument, got " + std::to_string(arg_count));
                    }

                    Value arg = pop();
                    if (arg.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        std::string str = arg.toString();
                        // Use UTF-8 aware uppercase function
                        std::string result = core::utf8::to_upper(str);
                        push(Value::makeVarchar(result));
                    }
                    break;
                }

                case Opcode::FUNC_LOWER:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 1)
                    {
                        error("LOWER expects 1 argument, got " + std::to_string(arg_count));
                    }

                    Value arg = pop();
                    if (arg.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        std::string str = arg.toString();
                        // Use UTF-8 aware lowercase function
                        std::string result = core::utf8::to_lower(str);
                        push(Value::makeVarchar(result));
                    }
                    break;
                }

                case Opcode::FUNC_TRIM:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 1)
                    {
                        error("TRIM expects 1 argument, got " + std::to_string(arg_count));
                    }

                    Value arg = pop();
                    if (arg.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        std::string str = arg.toString();

                        // Trim leading whitespace
                        size_t start = 0;
                        while (start < str.length() && std::isspace(static_cast<unsigned char>(str[start])))
                        {
                            start++;
                        }

                        // Trim trailing whitespace
                        size_t end = str.length();
                        while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1])))
                        {
                            end--;
                        }

                        std::string result = str.substr(start, end - start);
                        push(Value::makeVarchar(result));
                    }
                    break;
                }

                case Opcode::FUNC_CHAR_LENGTH:
                {
                    // CHAR_LENGTH returns character count (same as LENGTH for now)
                    uint8_t arg_count = readByte();
                    if (arg_count != 1)
                    {
                        error("CHAR_LENGTH expects 1 argument, got " + std::to_string(arg_count));
                    }

                    Value arg = pop();
                    if (arg.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        std::string str = arg.toString();
                        uint32_t char_len = charset_manager_.getCharLength(
                            reinterpret_cast<const uint8_t*>(str.data()),
                            str.length(),
                            core::CharacterSet::UTF8
                        );
                        push(Value::makeInt32(static_cast<int32_t>(char_len)));
                    }
                    break;
                }

                case Opcode::FUNC_OCTET_LENGTH:
                {
                    // OCTET_LENGTH returns byte count
                    uint8_t arg_count = readByte();
                    if (arg_count != 1)
                    {
                        error("OCTET_LENGTH expects 1 argument, got " + std::to_string(arg_count));
                    }

                    Value arg = pop();
                    if (arg.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        std::string str = arg.toString();
                        push(Value::makeInt32(static_cast<int32_t>(str.length())));
                    }
                    break;
                }

                case Opcode::FUNC_CONVERT:
                {
                    // CONVERT(str, from_charset, to_charset)
                    uint8_t arg_count = readByte();
                    if (arg_count != 3)
                    {
                        error("CONVERT expects 3 arguments, got " + std::to_string(arg_count));
                    }

                    // Pop args: to_charset, from_charset, str
                    Value to_cs_val = pop();
                    Value from_cs_val = pop();
                    Value str_val = pop();

                    if (str_val.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        std::string str = str_val.toString();
                        auto from_cs = static_cast<core::CharacterSet>(from_cs_val.toInt64());
                        auto to_cs = static_cast<core::CharacterSet>(to_cs_val.toInt64());

                        std::vector<uint8_t> output;
                        auto status = charset_manager_.convert(
                            reinterpret_cast<const uint8_t*>(str.data()),
                            str.length(),
                            from_cs,
                            output,
                            to_cs,
                            nullptr
                        );

                        if (status != core::Status::OK)
                        {
                            error("Character set conversion failed");
                        }

                        std::string result(output.begin(), output.end());
                        push(Value::makeVarchar(result));
                    }
                    break;
                }

                case Opcode::FUNC_COLLATE:
                {
                    // COLLATE applies a collation to an expression (metadata only, actual comparison elsewhere)
                    // For now, we'll just pass through the value but could store collation metadata
                    uint8_t arg_count = readByte();
                    if (arg_count != 2)
                    {
                        error("COLLATE expects 2 arguments (expr, collation_id), got " + std::to_string(arg_count));
                    }

                    Value collation_id = pop();
                    Value expr = pop();

                    // For now, just return the expression value
                    // In a full implementation, we'd attach collation metadata to the value
                    push(expr);
                    break;
                }

                // Aggregate functions (Note: proper aggregation requires SELECT-level support)
                // These implementations assume aggregation context is handled by caller
                case Opcode::AGG_SUM:
                case Opcode::AGG_AVG:
                case Opcode::AGG_MIN:
                case Opcode::AGG_MAX:
                case Opcode::AGG_COUNT:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 1)
                    {
                        error("Aggregate function expects 1 argument");
                    }

                    // For now, just evaluate the argument expression
                    // Full aggregation support requires refactoring SELECT execution
                    // to accumulate values across rows
                    error("Aggregate functions require full aggregation support (not yet implemented in executor)");
                    break;
                }

                // Temporal functions
                case Opcode::FUNC_DATE_ADD:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 2)
                    {
                        error("DATE_ADD expects 2 arguments");
                    }

                    Value days_val = pop();
                    Value date_val = pop();

                    if (date_val.isNull() || days_val.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        // Treat date as Unix timestamp (seconds since epoch)
                        // Add days * 86400 seconds
                        int64_t timestamp = date_val.toInt64();
                        int64_t days = days_val.toInt64();
                        int64_t result = timestamp + (days * 86400);
                        push(Value::makeInt64(result));
                    }
                    break;
                }

                case Opcode::FUNC_DATE_SUB:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 2)
                    {
                        error("DATE_SUB expects 2 arguments");
                    }

                    Value days_val = pop();
                    Value date_val = pop();

                    if (date_val.isNull() || days_val.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        int64_t timestamp = date_val.toInt64();
                        int64_t days = days_val.toInt64();
                        int64_t result = timestamp - (days * 86400);
                        push(Value::makeInt64(result));
                    }
                    break;
                }

                case Opcode::FUNC_DATE_DIFF:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 2)
                    {
                        error("DATE_DIFF expects 2 arguments");
                    }

                    Value date2_val = pop();
                    Value date1_val = pop();

                    if (date1_val.isNull() || date2_val.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        int64_t timestamp1 = date1_val.toInt64();
                        int64_t timestamp2 = date2_val.toInt64();
                        int64_t diff_days = (timestamp1 - timestamp2) / 86400;
                        push(Value::makeInt64(diff_days));
                    }
                    break;
                }

                case Opcode::FUNC_NOW:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 0)
                    {
                        error("NOW expects 0 arguments");
                    }

                    // Return current Unix timestamp
                    auto now = std::chrono::system_clock::now();
                    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                        now.time_since_epoch()).count();
                    push(Value::makeInt64(timestamp));
                    break;
                }

                case Opcode::FUNC_AT_TIME_ZONE:
                {
                    // timestamp AT TIME ZONE timezone_id
                    // Converts GMT timestamp to specified timezone for display
                    // Returns formatted string in target timezone
                    uint8_t arg_count = readByte();
                    if (arg_count != 2)
                    {
                        error("AT TIME ZONE expects 2 arguments (timestamp, timezone_id)");
                    }

                    Value tz_val = pop();        // timezone_id
                    Value timestamp_val = pop(); // timestamp in GMT

                    if (timestamp_val.isNull() || tz_val.isNull())
                    {
                        push(Value::makeNull());
                    }
                    else
                    {
                        int64_t gmt_microseconds = timestamp_val.toInt64();
                        uint16_t timezone_id = static_cast<uint16_t>(tz_val.toInt64());

                        // Format timestamp in target timezone
                        std::string result = timezone_manager_.formatTimestamp(
                            gmt_microseconds, timezone_id, true);
                        push(Value::makeVarchar(result));
                    }
                    break;
                }

                case Opcode::FUNC_CURRENT_DATE:
                {
                    uint8_t arg_count = readByte();
                    if (arg_count != 0)
                    {
                        error("CURRENT_DATE expects 0 arguments");
                    }

                    // Return current date as Unix timestamp (midnight)
                    auto now = std::chrono::system_clock::now();
                    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                        now.time_since_epoch()).count();
                    // Round down to midnight
                    timestamp = (timestamp / 86400) * 86400;
                    push(Value::makeInt64(timestamp));
                    break;
                }

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
                push(Value::makeNull()); // NULL result
                return;
            }

            switch (op)
            {
                case Opcode::EXPR_ADD:
                {
                    if (left.type() == core::DataType::FLOAT64 || right.type() == core::DataType::FLOAT64)
                        push(Value::makeFloat64(left.toDouble() + right.toDouble()));
                    else
                        push(Value::makeInt64(left.toInt64() + right.toInt64()));
                    break;
                }
                case Opcode::EXPR_SUBTRACT:
                {
                    if (left.type() == core::DataType::FLOAT64 || right.type() == core::DataType::FLOAT64)
                        push(Value::makeFloat64(left.toDouble() - right.toDouble()));
                    else
                        push(Value::makeInt64(left.toInt64() - right.toInt64()));
                    break;
                }
                case Opcode::EXPR_MULTIPLY:
                {
                    if (left.type() == core::DataType::FLOAT64 || right.type() == core::DataType::FLOAT64)
                        push(Value::makeFloat64(left.toDouble() * right.toDouble()));
                    else
                        push(Value::makeInt64(left.toInt64() * right.toInt64()));
                    break;
                }
                case Opcode::EXPR_DIVIDE:
                {
                    if (right.toDouble() == 0.0)
                        error("Division by zero");
                    if (left.type() == core::DataType::FLOAT64 || right.type() == core::DataType::FLOAT64)
                        push(Value::makeFloat64(left.toDouble() / right.toDouble()));
                    else
                        push(Value::makeInt64(left.toInt64() / right.toInt64()));
                    break;
                }
                case Opcode::EXPR_MODULO:
                {
                    if (right.toInt64() == 0)
                        error("Modulo by zero");
                    push(Value::makeInt64(left.toInt64() % right.toInt64()));
                    break;
                }

                // Comparison operators (collation-aware for strings)
                case Opcode::EXPR_EQ:
                {
                    bool result;
                    if (core::TypeSystem::isString(left.type()) || core::TypeSystem::isString(right.type()))
                        result = compareStrings(left.toString(), right.toString()) == 0;
                    else if (left.type() == core::DataType::FLOAT64 || right.type() == core::DataType::FLOAT64)
                        result = left.toDouble() == right.toDouble();
                    else
                        result = left.toInt64() == right.toInt64();
                    push(Value::makeBoolean(result));
                    break;
                }
                case Opcode::EXPR_NE:
                {
                    bool result;
                    if (core::TypeSystem::isString(left.type()) || core::TypeSystem::isString(right.type()))
                        result = compareStrings(left.toString(), right.toString()) != 0;
                    else if (left.type() == core::DataType::FLOAT64 || right.type() == core::DataType::FLOAT64)
                        result = left.toDouble() != right.toDouble();
                    else
                        result = left.toInt64() != right.toInt64();
                    push(Value::makeBoolean(result));
                    break;
                }
                case Opcode::EXPR_LT:
                {
                    bool result;
                    if (core::TypeSystem::isString(left.type()) || core::TypeSystem::isString(right.type()))
                        result = compareStrings(left.toString(), right.toString()) < 0;
                    else if (left.type() == core::DataType::FLOAT64 || right.type() == core::DataType::FLOAT64)
                        result = left.toDouble() < right.toDouble();
                    else
                        result = left.toInt64() < right.toInt64();
                    push(Value::makeBoolean(result));
                    break;
                }
                case Opcode::EXPR_GT:
                {
                    bool result;
                    if (core::TypeSystem::isString(left.type()) || core::TypeSystem::isString(right.type()))
                        result = compareStrings(left.toString(), right.toString()) > 0;
                    else if (left.type() == core::DataType::FLOAT64 || right.type() == core::DataType::FLOAT64)
                        result = left.toDouble() > right.toDouble();
                    else
                        result = left.toInt64() > right.toInt64();
                    push(Value::makeBoolean(result));
                    break;
                }
                case Opcode::EXPR_LE:
                {
                    bool result;
                    if (core::TypeSystem::isString(left.type()) || core::TypeSystem::isString(right.type()))
                        result = compareStrings(left.toString(), right.toString()) <= 0;
                    else if (left.type() == core::DataType::FLOAT64 || right.type() == core::DataType::FLOAT64)
                        result = left.toDouble() <= right.toDouble();
                    else
                        result = left.toInt64() <= right.toInt64();
                    push(Value::makeBoolean(result));
                    break;
                }
                case Opcode::EXPR_GE:
                {
                    bool result;
                    if (core::TypeSystem::isString(left.type()) || core::TypeSystem::isString(right.type()))
                        result = compareStrings(left.toString(), right.toString()) >= 0;
                    else if (left.type() == core::DataType::FLOAT64 || right.type() == core::DataType::FLOAT64)
                        result = left.toDouble() >= right.toDouble();
                    else
                        result = left.toInt64() >= right.toInt64();
                    push(Value::makeBoolean(result));
                    break;
                }

                // Logical operators
                case Opcode::EXPR_AND:
                    push(Value::makeBoolean(left.toBoolean() && right.toBoolean()));
                    break;
                case Opcode::EXPR_OR:
                    push(Value::makeBoolean(left.toBoolean() || right.toBoolean()));
                    break;

                // Pattern matching
                case Opcode::EXPR_LIKE:
                {
                    std::string text = left.toString();
                    std::string pattern = right.toString();
                    bool result = matchPattern(text, pattern, false);
                    push(Value::makeBoolean(result));
                    break;
                }
                case Opcode::EXPR_ILIKE:
                {
                    std::string text = left.toString();
                    std::string pattern = right.toString();
                    bool result = matchPattern(text, pattern, true);
                    push(Value::makeBoolean(result));
                    break;
                }

                default:
                    error("Unknown binary operator: " + std::to_string(static_cast<int>(op)));
            }
        }

        void Executor::error(const std::string &msg)
        {
            throw std::runtime_error(msg);
        }

        bool Executor::matchPattern(const std::string &text, const std::string &pattern, bool case_insensitive)
        {
            // Convert to lowercase for case-insensitive matching
            std::string t = text;
            std::string p = pattern;

            if (case_insensitive)
            {
                for (char &c : t)
                    c = std::tolower(static_cast<unsigned char>(c));
                for (char &c : p)
                    c = std::tolower(static_cast<unsigned char>(c));
            }

            // Simple pattern matching with % (any chars) and _ (single char)
            return matchPatternRecursive(t, 0, p, 0);
        }

        bool Executor::matchPatternRecursive(const std::string &text, size_t text_pos,
                                             const std::string &pattern, size_t pattern_pos)
        {
            // End of pattern
            if (pattern_pos == pattern.length())
            {
                return text_pos == text.length();
            }

            // % wildcard - matches zero or more characters
            if (pattern[pattern_pos] == '%')
            {
                // Skip consecutive % wildcards
                while (pattern_pos < pattern.length() && pattern[pattern_pos] == '%')
                {
                    pattern_pos++;
                }

                // If % is at the end, match rest of text
                if (pattern_pos == pattern.length())
                {
                    return true;
                }

                // Try matching at different positions
                for (size_t i = text_pos; i <= text.length(); i++)
                {
                    if (matchPatternRecursive(text, i, pattern, pattern_pos))
                    {
                        return true;
                    }
                }
                return false;
            }

            // End of text but pattern remains
            if (text_pos == text.length())
            {
                return false;
            }

            // _ wildcard - matches exactly one character
            if (pattern[pattern_pos] == '_')
            {
                return matchPatternRecursive(text, text_pos + 1, pattern, pattern_pos + 1);
            }

            // Regular character match
            if (text[text_pos] == pattern[pattern_pos])
            {
                return matchPatternRecursive(text, text_pos + 1, pattern, pattern_pos + 1);
            }

            return false;
        }

        int Executor::compareStrings(const std::string& left, const std::string& right, uint32_t collation_id) const
        {
            // Use charset manager for collation-aware comparison
            // Default collation_id = 101 (utf8_general_ci - case insensitive)
            return charset_manager_.compare(
                reinterpret_cast<const uint8_t*>(left.data()), left.length(),
                reinterpret_cast<const uint8_t*>(right.data()), right.length(),
                collation_id
            );
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
                        values_out.push_back(Value::makeNull()); // NULL value
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
                        values_out.push_back(Value::makeInt32(val));
                        data_offset += sizeof(int32_t);
                        break;
                    }
                    case core::DataType::INT64:
                    {
                        if (data_offset + sizeof(int64_t) > tuple_size)
                            return false;

                        int64_t val;
                        std::memcpy(&val, tuple_data + data_offset, sizeof(int64_t));
                        values_out.push_back(Value::makeInt64(val));
                        data_offset += sizeof(int64_t);
                        break;
                    }
                    case core::DataType::FLOAT64:
                    {
                        if (data_offset + sizeof(double) > tuple_size)
                            return false;

                        double val;
                        std::memcpy(&val, tuple_data + data_offset, sizeof(double));
                        values_out.push_back(Value::makeFloat64(val));
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
                        values_out.push_back(Value::makeVarchar(str));
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