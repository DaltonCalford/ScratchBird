#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"


namespace scratchbird {
namespace core {

// Forward declarations
class PageManager;

using ID = UuidV7Bytes;

// Simple heap page for catalog tables
struct CatalogHeapPage {
    PageHeader header;
    uint32_t record_count;
    uint32_t free_offset;
    uint8_t data[];  // Variable length records
};

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
        ID schema_id;
        std::string schema_name;
        std::string owner;
        uint64_t created_time;
    };
    
    // Table information
    struct TableInfo {
        ID table_id;
        ID schema_id;
        std::string table_name;
        uint32_t root_page;     // Root page of table data
        uint32_t column_count;
        uint64_t row_count;     // Estimated row count
        uint64_t created_time;
    };
    
    // Column information
    struct ColumnInfo {
        ID table_id;
        uint16_t column_id;
        std::string column_name;
        uint16_t data_type;     // Type code
        uint32_t max_length;    // For variable length types
        bool nullable;
        bool has_default;
        std::string default_value;  // Serialized default
    };
    
    // Index information
    struct IndexInfo {
        ID index_id;
        ID table_id;
        std::string index_name;
        uint32_t root_page;
        bool is_unique;
        std::vector<uint16_t> column_ids;
        uint64_t created_time;
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
                        ID& schema_id,
                        ErrorContext* ctx = nullptr);
    
    Status get_schema(const ID& schema_id, SchemaInfo& info, 
                     ErrorContext* ctx = nullptr);
    
    Status get_schema(const std::string& schema_name, SchemaInfo& info,
                     ErrorContext* ctx = nullptr);
    
    Status list_schemas(std::vector<SchemaInfo>& schemas,
                       ErrorContext* ctx = nullptr);
    
    // Table operations
    Status create_table(const ID& schema_id,
                       const std::string& table_name,
                       const std::vector<ColumnInfo>& columns,
                       ID& table_id,
                       ErrorContext* ctx = nullptr);
    
    Status get_table(const ID& table_id, TableInfo& info,
                    ErrorContext* ctx = nullptr);
    
    Status get_table(const ID& schema_id, const std::string& table_name,
                    TableInfo& info, ErrorContext* ctx = nullptr);
    
    Status list_tables(const ID& schema_id, std::vector<TableInfo>& tables,
                      ErrorContext* ctx = nullptr);
    
    // Column operations
    Status get_columns(const ID& table_id, std::vector<ColumnInfo>& columns,
                      ErrorContext* ctx = nullptr);
    
    Status get_column(const ID& table_id, const std::string& column_name,
                     ColumnInfo& info, ErrorContext* ctx = nullptr);
    
    // Index operations
    Status create_index(const ID& table_id,
                        const std::string& index_name,
                        const std::vector<std::string>& column_names,
                        ID& index_id,
                        bool is_unique = false,
                        ErrorContext* ctx = nullptr);

    Status get_index(const ID& index_id, IndexInfo& info,
                     ErrorContext* ctx = nullptr);

    Status get_index(const ID& table_id, const std::string& index_name,
                     IndexInfo& info, ErrorContext* ctx = nullptr);

    Status list_indexes_for_table(const ID& table_id,
                                  std::vector<IndexInfo>& indexes,
                                  ErrorContext* ctx = nullptr);
    
    // Catalog statistics
    uint32_t schema_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return schema_count_;
    }
    uint32_t table_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return table_count_;
    }
    
private:
    Database* db_;
    mutable std::mutex mutex_;
    
    // Catalog page layout - using higher page numbers to avoid conflict
    // with existing system catalog on page 1
    static constexpr uint32_t CATALOG_ROOT_PAGE = 3;
    static constexpr uint32_t SCHEMAS_TABLE_PAGE = 4;
    static constexpr uint32_t TABLES_TABLE_PAGE = 5;
    static constexpr uint32_t COLUMNS_TABLE_PAGE = 6;
    static constexpr uint32_t INDEXES_TABLE_PAGE = 7;
    
    // In-memory cache of catalog data
    std::unordered_map<ID, SchemaInfo> schema_cache_;
    std::unordered_map<ID, TableInfo> table_cache_;
    std::unordered_map<ID, std::vector<ColumnInfo>> column_cache_;
    std::unordered_map<ID, IndexInfo> index_cache_;
    
    // Counters
    uint32_t schema_count_ = 0;
    uint32_t table_count_ = 0;
    
    // Actual page numbers (may differ from constants during init)
    uint32_t schemas_table_page_ = SCHEMAS_TABLE_PAGE;
    uint32_t tables_table_page_ = TABLES_TABLE_PAGE;
    uint32_t columns_table_page_ = COLUMNS_TABLE_PAGE;
    uint32_t indexes_table_page_ = INDEXES_TABLE_PAGE;
    
    // Internal methods
    Status write_catalog_root(ErrorContext* ctx);
    Status read_catalog_root(ErrorContext* ctx);
    
    // Helper to write a record to a catalog heap page
    template<typename RecordType>
    Status write_record_to_heap_page(uint32_t page_id, const RecordType& record, ErrorContext* ctx);

    // Helper to read records from a catalog heap page
    template<typename RecordType, typename InfoType, typename KeyType, typename Converter, typename KeyExtractor>
    Status read_records_from_heap_page(uint32_t page_id, std::unordered_map<KeyType, InfoType>& cache,
                                       Converter converter,
                                       KeyExtractor key_extractor, ErrorContext* ctx) {
        BufferPool* bp = db_->buffer_pool();
        void* page_buffer;
        Status status = bp->pin_page(page_id, &page_buffer, ctx);
        if (status != Status::Ok) {
            SET_ERROR_CONTEXT(ctx, status, "Failed to read catalog heap page");
            return status;
        }

        CatalogHeapPage* heap = reinterpret_cast<CatalogHeapPage*>(page_buffer);

        cache.clear();
        uint32_t offset = sizeof(CatalogHeapPage);

        for (uint32_t i = 0; i < heap->record_count; i++) {
            RecordType* record = reinterpret_cast<RecordType*>(
                reinterpret_cast<uint8_t*>(page_buffer) + offset);

            if (record->is_valid) {
                InfoType info;
                converter(*record, info);
                cache[key_extractor(info)] = info;
            }

            offset += sizeof(RecordType);
        }

        return bp->unpin_page(page_id, false, ctx);
    }

    // Helper to read records from a catalog heap page
    template<typename RecordType, typename InfoType>
    Status read_records_to_vector(uint32_t page_id, std::vector<InfoType>& results,
                                  std::function<bool(const RecordType&)> filter,
                                  std::function<void(const RecordType&, InfoType&)> converter,
                                  ErrorContext* ctx);

    // Specific write/read methods using the generic helpers
    Status write_schema_record(const SchemaInfo& schema, ErrorContext* ctx);
    Status read_schema_records(ErrorContext* ctx);
    Status write_table_record(const TableInfo& table, ErrorContext* ctx);
    Status read_table_records(ErrorContext* ctx);
    Status write_column_records(const ID& table_id, 
                               const std::vector<ColumnInfo>& columns,
                               ErrorContext* ctx);
    Status read_column_records(const ID& table_id, ErrorContext* ctx);
    Status write_index_record(const IndexInfo& index, ErrorContext* ctx);
    Status read_index_records(ErrorContext* ctx);
    
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
