#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/error_context.h"

namespace scratchbird {
namespace core {

// Forward declarations
class Database;
class BufferPool;
class PageManager;

/**
 * System Catalog Manager
 * 
 * Manages the system catalog which tracks all database metadata including:
 * - Schemas
 * - Tables
 * - Columns
 * - Indexes (future)
 * - Constraints (future)
 */
class CatalogManager {
public:
    // Schema information
    struct SchemaInfo {
        uint32_t schema_id;
        std::string schema_name;
        std::string owner;
        uint64_t created_time;
    };
    
    // Table information
    struct TableInfo {
        uint32_t table_id;
        uint32_t schema_id;
        std::string table_name;
        uint32_t root_page;     // Root page of table data
        uint32_t column_count;
        uint64_t row_count;     // Estimated row count
        uint64_t created_time;
    };
    
    // Column information
    struct ColumnInfo {
        uint32_t table_id;
        uint16_t column_id;
        std::string column_name;
        uint16_t data_type;     // Type code
        uint32_t max_length;    // For variable length types
        bool nullable;
        bool has_default;
        std::string default_value;  // Serialized default
    };
    
    CatalogManager(Database* db);
    ~CatalogManager();
    
    // Initialize catalog for new database
    Status initialize(ErrorContext* ctx = nullptr);
    
    // Load catalog from existing database
    Status load(ErrorContext* ctx = nullptr);
    
    // Schema operations
    Status create_schema(const std::string& schema_name, 
                        const std::string& owner,
                        uint32_t& schema_id,
                        ErrorContext* ctx = nullptr);
    
    Status get_schema(uint32_t schema_id, SchemaInfo& info, 
                     ErrorContext* ctx = nullptr);
    
    Status get_schema(const std::string& schema_name, SchemaInfo& info,
                     ErrorContext* ctx = nullptr);
    
    Status list_schemas(std::vector<SchemaInfo>& schemas,
                       ErrorContext* ctx = nullptr);
    
    // Table operations
    Status create_table(uint32_t schema_id,
                       const std::string& table_name,
                       const std::vector<ColumnInfo>& columns,
                       uint32_t& table_id,
                       ErrorContext* ctx = nullptr);
    
    Status get_table(uint32_t table_id, TableInfo& info,
                    ErrorContext* ctx = nullptr);
    
    Status get_table(uint32_t schema_id, const std::string& table_name,
                    TableInfo& info, ErrorContext* ctx = nullptr);
    
    Status list_tables(uint32_t schema_id, std::vector<TableInfo>& tables,
                      ErrorContext* ctx = nullptr);
    
    // Column operations
    Status get_columns(uint32_t table_id, std::vector<ColumnInfo>& columns,
                      ErrorContext* ctx = nullptr);
    
    Status get_column(uint32_t table_id, const std::string& column_name,
                     ColumnInfo& info, ErrorContext* ctx = nullptr);
    
    // Catalog statistics
    uint32_t schema_count() const { return schema_count_; }
    uint32_t table_count() const { return table_count_; }
    
private:
    Database* db_;
    
    // Catalog page layout - using higher page numbers to avoid conflict
    // with existing system catalog on page 1
    static constexpr uint32_t CATALOG_ROOT_PAGE = 3;
    static constexpr uint32_t SCHEMAS_TABLE_PAGE = 4;
    static constexpr uint32_t TABLES_TABLE_PAGE = 5;
    static constexpr uint32_t COLUMNS_TABLE_PAGE = 6;
    
    // In-memory cache of catalog data
    std::unordered_map<uint32_t, SchemaInfo> schema_cache_;
    std::unordered_map<uint32_t, TableInfo> table_cache_;
    std::unordered_map<uint32_t, std::vector<ColumnInfo>> column_cache_;
    
    // Counters
    uint32_t schema_count_ = 0;
    uint32_t table_count_ = 0;
    uint32_t next_schema_id_ = 1000;  // Start user schemas at 1000
    uint32_t next_table_id_ = 1000;   // Start user tables at 1000
    
    // Actual page numbers (may differ from constants during init)
    uint32_t schemas_table_page_ = SCHEMAS_TABLE_PAGE;
    uint32_t tables_table_page_ = TABLES_TABLE_PAGE;
    uint32_t columns_table_page_ = COLUMNS_TABLE_PAGE;
    
    // Internal methods
    Status write_catalog_root(ErrorContext* ctx);
    Status read_catalog_root(ErrorContext* ctx);
    
    // Helper to write a record to a catalog heap page
    template<typename RecordType>
    Status write_record_to_heap_page(uint32_t page_id, const RecordType& record, ErrorContext* ctx);

    // Helper to read records from a catalog heap page
    template<typename RecordType, typename InfoType>
    Status read_records_from_heap_page(uint32_t page_id, std::unordered_map<uint32_t, InfoType>& cache, 
                                       std::function<void(const RecordType&, InfoType&)> converter, ErrorContext* ctx);

    // Specific converters for read_records_from_heap_page
    static void convert_schema_record(const SchemaRecord& record, SchemaInfo& info);
    static void convert_table_record(const TableRecord& record, TableInfo& info);

    // Specific write/read methods using the generic helpers
    Status write_schema_record(const SchemaInfo& schema, ErrorContext* ctx);
    Status read_schema_records(ErrorContext* ctx);
    Status write_table_record(const TableInfo& table, ErrorContext* ctx);
    Status read_table_records(ErrorContext* ctx);
    Status write_column_records(uint32_t table_id, 
                               const std::vector<ColumnInfo>& columns,
                               ErrorContext* ctx);
    Status read_column_records(uint32_t table_id, ErrorContext* ctx);
    
    // Helper to allocate catalog pages
    Status allocate_catalog_page(uint32_t& page_id, ErrorContext* ctx);
};

// Data type codes
enum class DataType : uint16_t {
    Unknown = 0,
    
    // Numeric types
    Int8 = 1,
    Int16 = 2,
    Int32 = 3,
    Int = 3,        // Alias for Int32
    Int64 = 4,
    Float32 = 5,
    Float64 = 6,
    Decimal = 7,
    
    // String types
    Char = 10,      // Fixed length
    Varchar = 11,   // Variable length
    Text = 12,      // Unlimited length
    
    // Binary types
    Binary = 20,    // Fixed length
    Varbinary = 21, // Variable length
    Blob = 22,      // Binary large object
    Bytea = 23,     // PostgreSQL-style binary data
    
    // Date/Time types
    Date = 30,
    Time = 31,
    Timestamp = 32,
    
    // Boolean
    Boolean = 40,
    
    // Special types
    Uuid = 50,
    Json = 51,
    
    // Future types...
};

} // namespace core
} // namespace scratchbird