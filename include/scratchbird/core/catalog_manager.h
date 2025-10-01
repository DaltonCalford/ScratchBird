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


    namespace scratchbird::core
    {

        // Forward declarations
        class PageManager;

        using ID = UuidV7Bytes;

        // Simple heap page for catalog tables
        struct CatalogHeapPage
        {
            PageHeader header;
            uint32_t record_count;
            uint32_t free_offset;
            uint8_t data[]; // Variable length records
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
        class CatalogManager
        {
        public:
            // Schema information
            struct SchemaInfo
            {
                ID schema_id;
                std::string schema_name;
                std::string owner;
                uint64_t created_time;
            };

            // Table information
            struct TableInfo
            {
                ID table_id;
                ID schema_id;
                std::string table_name;
                uint32_t root_page; // Root page of table data
                uint32_t column_count;
                uint64_t row_count; // Estimated row count
                uint64_t created_time;
            };

            // Column information
            struct ColumnInfo
            {
                ID table_id;
                ID column_id;
                std::string column_name;
                uint16_t data_type;  // Type code
                uint32_t max_length; // For variable length types
                bool nullable;
                bool has_default;
                std::string default_value; // Serialized default
            };

            // Index information
            struct IndexInfo
            {
                ID index_id;
                ID table_id;
                std::string index_name;
                uint32_t root_page;
                bool is_unique;
                std::vector<ID> column_ids;
                uint64_t created_time;
            };

            CatalogManager(Database *db);
            ~CatalogManager();

            // Initialize catalog for new database
            auto initialize(ErrorContext *ctx = nullptr) -> Status;

            // Load catalog from existing database
            auto load(ErrorContext *ctx = nullptr) -> Status;

            // Schema operations
            auto createSchema(const std::string &schema_name, const std::string &owner,
                                 ID &schema_id, ErrorContext *ctx = nullptr) -> Status;

            auto getSchema(const ID &schema_id, SchemaInfo &info, ErrorContext *ctx = nullptr) -> Status;

            auto getSchema(const std::string &schema_name, SchemaInfo &info,
                              ErrorContext *ctx = nullptr) -> Status;

            auto listSchemas(std::vector<SchemaInfo> &schemas, ErrorContext *ctx = nullptr) -> Status;

            // Table operations
            auto createTable(const ID &schema_id, const std::string &table_name,
                                const std::vector<ColumnInfo> &columns, ID &table_id,
                                ErrorContext *ctx = nullptr) -> Status;

            auto getTable(const ID &table_id, TableInfo &info, ErrorContext *ctx = nullptr) -> Status;

            auto getTable(const ID &schema_id, const std::string &table_name, TableInfo &info,
                             ErrorContext *ctx = nullptr) -> Status;

            auto listTables(const ID &schema_id, std::vector<TableInfo> &tables,
                               ErrorContext *ctx = nullptr) -> Status;

            // Column operations
            auto getColumns(const ID &table_id, std::vector<ColumnInfo> &columns,
                               ErrorContext *ctx = nullptr) -> Status;

            auto getColumn(const ID &table_id, const std::string &column_name, ColumnInfo &info,
                              ErrorContext *ctx = nullptr) -> Status;

            // Index operations
            auto createIndex(const ID &table_id, const std::string &index_name,
                                const std::vector<std::string> &column_names, ID &index_id,
                                bool is_unique = false, ErrorContext *ctx = nullptr) -> Status;

            auto getIndex(const ID &index_id, IndexInfo &info, ErrorContext *ctx = nullptr) -> Status;

            auto getIndex(const ID &table_id, const std::string &index_name, IndexInfo &info,
                             ErrorContext *ctx = nullptr) -> Status;

            auto listIndexesForTable(const ID &table_id, std::vector<IndexInfo> &indexes,
                                          ErrorContext *ctx = nullptr) -> Status;

            // Catalog statistics
            auto schemaCount() const -> uint32_t
            {
                std::lock_guard<std::mutex> lock(mutex_);
                return schema_count_;
            }
            auto tableCount() const -> uint32_t
            {
                std::lock_guard<std::mutex> lock(mutex_);
                return table_count_;
            }

        private:
            Database *db_;
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
            auto writeCatalogRoot(ErrorContext *ctx) -> Status;
            auto readCatalogRoot(ErrorContext *ctx) -> Status;

            // Helper to write a record to a catalog heap page
            template <typename RecordType>
            auto writeRecordToHeapPage(uint32_t page_id, const RecordType &record,
                                             ErrorContext *ctx) -> Status;

            // Helper to read records from a catalog heap page
            template <typename RecordType, typename InfoType, typename KeyType, typename Converter,
                      typename KeyExtractor>
            auto readRecordsFromHeapPage(uint32_t page_id,
                                               std::unordered_map<KeyType, InfoType> &cache,
                                               Converter converter, KeyExtractor key_extractor,
                                               ErrorContext *ctx) -> Status
            {
                BufferPool *bp = db_->buffer_pool();
                void *page_buffer;
                Status status = bp->pinPage(page_id, &page_buffer, ctx);
                if (status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, status, "Failed to read catalog heap page");
                    return status;
                }

                auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);

                cache.clear();
                uint32_t offset = sizeof(CatalogHeapPage);

                for (uint32_t i = 0; i < heap->record_count; i++)
                {
                    auto *record = reinterpret_cast<RecordType *>(
                        reinterpret_cast<uint8_t *>(page_buffer) + offset);

                    if (record->is_valid)
                    {
                        InfoType info;
                        converter(*record, info);
                        cache[key_extractor(info)] = info;
                    }

                    offset += sizeof(RecordType);
                }

                return bp->unpinPage(page_id, false, ctx);
            }

            // Helper to read records from a catalog heap page
            template <typename RecordType, typename InfoType>
            auto
            readRecordsToVector(uint32_t page_id, std::vector<InfoType> &results,
                                   std::function<bool(const RecordType &)> filter,
                                   std::function<void(const RecordType &, InfoType &)> converter,
                                   ErrorContext *ctx) -> Status;

            // Specific write/read methods using the generic helpers
            auto writeSchemaRecord(const SchemaInfo &schema, ErrorContext *ctx) -> Status;
            auto readSchemaRecords(ErrorContext *ctx) -> Status;
            auto writeTableRecord(const TableInfo &table, ErrorContext *ctx) -> Status;
            auto readTableRecords(ErrorContext *ctx) -> Status;
            auto writeColumnRecords(const ID &table_id, const std::vector<ColumnInfo> &columns,
                                        ErrorContext *ctx) -> Status;
            auto readColumnRecords(const ID &table_id, ErrorContext *ctx) -> Status;
            auto writeIndexRecord(const IndexInfo &index, ErrorContext *ctx) -> Status;
            auto readIndexRecords(ErrorContext *ctx) -> Status;

            // Helper to allocate catalog pages
            auto allocateCatalogPage(uint32_t &page_id, ErrorContext *ctx) -> Status;
        };

        // Data type codes
        enum class DataType : uint16_t
        {
            UNKNOWN = 0,

            // Numeric types
            INT8 = 1,
            INT16 = 2,
            INT32 = 3,
            INT = 3, // Alias for Int32
            INT64 = 4,
            FLOAT32 = 5,
            FLOAT64 = 6,
            DECIMAL = 7,

            // String types
            CHAR = 10,    // Fixed length
            VARCHAR = 11, // Variable length
            TEXT = 12,    // Unlimited length

            // Binary types
            BINARY = 20,    // Fixed length
            VARBINARY = 21, // Variable length
            BLOB = 22,      // Binary large object
            BYTEA = 23,     // PostgreSQL-style binary data

            // Date/Time types
            DATE = 30,
            TIME = 31,
            TIMESTAMP = 32,

            // Boolean
            BOOLEAN = 40,

            // Special types
            UUID = 50,
            JSON = 51,

            // Future types...
        };

    } // namespace scratchbird::core

