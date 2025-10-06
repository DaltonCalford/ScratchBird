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
#include "scratchbird/core/types.h"


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
                uint16_t default_tablespace_id = 0;  // Default tablespace for new tables
                uint16_t permissions = 0;            // Bitmask of schema permissions
                uint16_t default_charset = 0;        // Default character set (0 = inherit from database)
                uint16_t reserved = 0;
                uint32_t default_collation_id = 0;   // Default collation ID (0 = inherit from database)
                uint32_t acl_oid = 0;                // TOAST reference for ACL (access control list)
                uint32_t search_path_oid = 0;        // TOAST reference for search path
                uint64_t created_time = 0;
                uint64_t last_modified_time = 0;
            };

            // Table types
            enum class TableType : uint8_t
            {
                HEAP = 0,       // Regular heap table
                INDEX = 1,      // Index-organized table
                TEMPORARY = 2,  // Temporary table
                EXTERNAL = 3,   // External table
                MATERIALIZED_VIEW = 4, // Materialized view
                TOAST = 5       // TOAST table
            };

            // Table information
            struct TableInfo
            {
                ID table_id;
                ID schema_id;
                std::string table_name;
                uint32_t root_page = 0;     // Root page of table data
                uint32_t column_count = 0;
                uint64_t row_count = 0;     // Estimated row count
                TableType table_type = TableType::HEAP;
                bool has_toast = false;
                uint16_t tablespace_id = 0; // Tablespace ID (0 = default)
                uint16_t default_charset = 0;        // Default character set (0 = inherit from schema)
                uint32_t default_collation_id = 0;   // Default collation ID (0 = inherit from schema)
                uint32_t storage_params_oid = 0; // TOAST reference for storage parameters
                uint64_t created_time = 0;
                uint64_t last_modified_time = 0;
            };

            // Column information
            struct ColumnInfo
            {
                ID table_id;
                ID column_id;
                std::string column_name;
                uint16_t ordinal = 0;        // Column position in table
                uint16_t data_type = 0;      // Type code
                uint32_t type_precision = 0; // For DECIMAL, VECTOR dimensions, VARCHAR length
                uint32_t type_scale = 0;     // For DECIMAL scale
                uint32_t max_length = 0;     // Legacy field, use type_precision instead
                bool nullable = true;
                bool has_default = false;
                bool is_primary_key = false;
                bool is_unique = false;
                bool is_foreign_key = false;
                bool is_generated = false;
                uint8_t storage_type = 0;    // TOAST storage strategy
                bool with_timezone = false;  // For TIMESTAMP: WITH TIME ZONE
                uint16_t charset = 0;        // Character set (0 = inherit from table)
                uint16_t timezone_hint = 0;  // Timezone ID for display (0 = use connection default)
                uint32_t collation_id = 0;   // Collation ID (0 = inherit from table)
                std::string default_value;   // Serialized default
                uint32_t default_value_oid = 0; // TOAST reference for large defaults
                uint32_t check_expr_oid = 0;    // TOAST reference for check expressions
                uint64_t created_time = 0;
            };

            // Index types
            enum class IndexType : uint8_t
            {
                BTREE = 0,      // B-tree index (default)
                HASH = 1,       // Hash index
                VECTOR = 2,     // Vector similarity index (HNSW, IVF, etc.)
                FULLTEXT = 3,   // Full-text search index
                GIN = 4,        // Generalized Inverted Index
                GIST = 5,       // Generalized Search Tree
                BRIN = 6        // Block Range Index
            };

            // Index information
            struct IndexInfo
            {
                ID index_id;
                ID table_id;
                std::string index_name;
                uint32_t root_page = 0;
                IndexType index_type = IndexType::BTREE;
                bool is_unique = false;
                std::vector<ID> column_ids;
                uint32_t index_params_oid = 0; // TOAST reference for index parameters
                uint64_t created_time = 0;
                uint32_t collation_id = 101; // Default: utf8_general_ci (binary comparison)
                                             // TODO(Issue #50): Full integration of collation-aware
                                             // comparisons throughout B-tree and query evaluation
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
                                bool is_unique = false, IndexType index_type = IndexType::BTREE,
                                ErrorContext *ctx = nullptr) -> Status;

            auto getIndex(const ID &index_id, IndexInfo &info, ErrorContext *ctx = nullptr) -> Status;

            auto getIndex(const ID &table_id, const std::string &index_name, IndexInfo &info,
                             ErrorContext *ctx = nullptr) -> Status;

            auto listIndexesForTable(const ID &table_id, std::vector<IndexInfo> &indexes,
                                          ErrorContext *ctx = nullptr) -> Status;

            // Timezone operations (pg_timezone system table)
            struct TimezoneInfo
            {
                uint16_t timezone_id = 0;
                std::string name;
                std::string abbreviation;
                int32_t std_offset_minutes = 0;
                bool observes_dst = false;
                uint8_t dst_start_month = 0;
                uint8_t dst_start_week = 0;
                uint8_t dst_start_day = 0;
                uint8_t dst_start_hour = 0;
                uint8_t dst_end_month = 0;
                uint8_t dst_end_week = 0;
                uint8_t dst_end_day = 0;
                uint8_t dst_end_hour = 0;
                int32_t dst_offset_minutes = 0;
                uint64_t created_time = 0;
                uint64_t last_modified_time = 0;
            };

            auto createTimezone(const TimezoneInfo &tz_info, ErrorContext *ctx = nullptr) -> Status;
            auto updateTimezone(uint16_t timezone_id, const TimezoneInfo &tz_info, ErrorContext *ctx = nullptr) -> Status;
            auto getTimezone(uint16_t timezone_id, TimezoneInfo &info, ErrorContext *ctx = nullptr) -> Status;
            auto getTimezoneByName(const std::string &name, TimezoneInfo &info, ErrorContext *ctx = nullptr) -> Status;
            auto listTimezones(std::vector<TimezoneInfo> &timezones, ErrorContext *ctx = nullptr) -> Status;
            auto deleteTimezone(uint16_t timezone_id, ErrorContext *ctx = nullptr) -> Status;

            // Character set operations (pg_charset system table)
            struct CharsetInfo
            {
                uint16_t charset_id = 0;        // Character set ID (matches CharacterSet enum)
                std::string name;               // e.g., "utf8", "latin1"
                std::string description;        // Human-readable description
                uint8_t min_bytes = 1;          // Minimum bytes per character
                uint8_t max_bytes = 1;          // Maximum bytes per character
                uint8_t variable_width = 0;     // 1 = variable width, 0 = fixed width
                uint8_t reserved = 0;
                uint32_t default_collation_id = 0; // Default collation for this charset
                uint64_t created_time = 0;
                uint64_t last_modified_time = 0;
            };

            auto createCharset(const CharsetInfo &cs_info, ErrorContext *ctx = nullptr) -> Status;
            auto updateCharset(uint16_t charset_id, const CharsetInfo &cs_info, ErrorContext *ctx = nullptr) -> Status;
            auto getCharset(uint16_t charset_id, CharsetInfo &info, ErrorContext *ctx = nullptr) -> Status;
            auto getCharsetByName(const std::string &name, CharsetInfo &info, ErrorContext *ctx = nullptr) -> Status;
            auto listCharsets(std::vector<CharsetInfo> &charsets, ErrorContext *ctx = nullptr) -> Status;
            auto deleteCharset(uint16_t charset_id, ErrorContext *ctx = nullptr) -> Status;

            // Collation operations (pg_collation system table)
            struct CollationCatalogInfo
            {
                uint32_t collation_id = 0;
                std::string name;               // e.g., "utf8_general_ci"
                uint16_t charset_id = 0;        // Associated character set ID
                uint8_t collation_type = 0;     // CollationType enum value
                uint8_t strength = 0;           // CollationStrength enum value
                uint8_t pad_space = 1;          // 1 = PAD SPACE, 0 = NO PAD
                uint8_t is_default = 0;         // 1 = default for charset, 0 = not default
                uint16_t reserved = 0;
                char locale[32] = {0};          // Locale string (e.g., "en_US")
                uint64_t created_time = 0;
                uint64_t last_modified_time = 0;
            };

            auto createCollation(const CollationCatalogInfo &col_info, ErrorContext *ctx = nullptr) -> Status;
            auto updateCollation(uint32_t collation_id, const CollationCatalogInfo &col_info, ErrorContext *ctx = nullptr) -> Status;
            auto getCollation(uint32_t collation_id, CollationCatalogInfo &info, ErrorContext *ctx = nullptr) -> Status;
            auto getCollationByName(const std::string &name, CollationCatalogInfo &info, ErrorContext *ctx = nullptr) -> Status;
            auto listCollations(std::vector<CollationCatalogInfo> &collations, ErrorContext *ctx = nullptr) -> Status;
            auto listCollationsForCharset(uint16_t charset_id, std::vector<CollationCatalogInfo> &collations, ErrorContext *ctx = nullptr) -> Status;
            auto deleteCollation(uint32_t collation_id, ErrorContext *ctx = nullptr) -> Status;

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

            // Internal helper methods (assume mutex_ is already held by caller)
            auto createSchemaInternal(const std::string &schema_name, const std::string &owner,
                                 ID &schema_id, ErrorContext *ctx = nullptr) -> Status;

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
            uint32_t constraints_table_page_ = 0;  // Will be allocated during init
            uint32_t sequences_table_page_ = 0;    // Will be allocated during init
            uint32_t views_table_page_ = 0;        // Will be allocated during init
            uint32_t triggers_table_page_ = 0;     // Will be allocated during init
            uint32_t permissions_table_page_ = 0;  // Will be allocated during init
            uint32_t statistics_table_page_ = 0;   // Will be allocated during init
            uint32_t collations_table_page_ = 0;   // Will be allocated during init
            uint32_t timezones_table_page_ = 0;    // Will be allocated during init
            uint32_t charsets_table_page_ = 0;     // Will be allocated during init (pg_charset)
            uint32_t collation_defs_table_page_ = 0; // Will be allocated during init (pg_collation)

            // Internal methods
            auto writeCatalogRoot(ErrorContext *ctx) -> Status;
            auto readCatalogRoot(ErrorContext *ctx) -> Status;

            // Helper to write a record to a catalog heap page
            template <typename RecordType>
            auto writeRecordToHeapPage(uint32_t page_id, const RecordType &record,
                                             ErrorContext *ctx) -> Status;

            // Result structure for findRecordInHeapPage
            template <typename RecordType>
            struct FindResult
            {
                Status status;
                uint32_t slot_index;  // Index in the catalog page
                RecordType record;
            };

            // Helper to find a record in a catalog heap page matching a predicate
            template <typename RecordType, typename Predicate>
            auto findRecordInHeapPage(uint32_t page_id, Predicate predicate, ErrorContext *ctx)
                -> FindResult<RecordType>
            {
                BufferPool *bp = db_->buffer_pool();
                void *page_buffer;

                Status status = bp->pinPage(page_id, &page_buffer, ctx);
                if (status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, status, "Failed to pin catalog heap page");
                    return {status, 0, RecordType{}};
                }

                auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);
                uint32_t offset = sizeof(CatalogHeapPage);

                for (uint32_t i = 0; i < heap->record_count; i++)
                {
                    auto *record = reinterpret_cast<RecordType *>(
                        reinterpret_cast<uint8_t *>(page_buffer) + offset);

                    if (predicate(*record))
                    {
                        RecordType found = *record;
                        bp->unpinPage(page_id, false, ctx);
                        return {Status::OK, i, found};
                    }

                    offset += sizeof(RecordType);
                }

                bp->unpinPage(page_id, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Record not found in catalog page");
                return {Status::NOT_FOUND, 0, RecordType{}};
            }

            // Helper to scan all records in a catalog heap page
            template <typename RecordType, typename InfoType, typename Converter>
            auto scanHeapPage(uint32_t page_id, std::vector<InfoType> &results,
                             Converter converter, ErrorContext *ctx) -> Status
            {
                BufferPool *bp = db_->buffer_pool();
                void *page_buffer;

                Status status = bp->pinPage(page_id, &page_buffer, ctx);
                if (status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, status, "Failed to pin catalog heap page");
                    return status;
                }

                auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);
                uint32_t offset = sizeof(CatalogHeapPage);

                for (uint32_t i = 0; i < heap->record_count; i++)
                {
                    auto *record = reinterpret_cast<RecordType *>(
                        reinterpret_cast<uint8_t *>(page_buffer) + offset);

                    if (record->is_valid)
                    {
                        InfoType info;
                        converter(*record, info);
                        results.push_back(info);
                    }

                    offset += sizeof(RecordType);
                }

                return bp->unpinPage(page_id, false, ctx);
            }

            // Helper to update a record in a catalog heap page
            template <typename RecordType>
            auto updateRecordInHeapPage(uint32_t page_id, uint32_t slot_index,
                                       const RecordType &updated_record, ErrorContext *ctx) -> Status
            {
                BufferPool *bp = db_->buffer_pool();
                void *page_buffer;

                Status status = bp->pinPage(page_id, &page_buffer, ctx);
                if (status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, status, "Failed to pin catalog heap page");
                    return status;
                }

                auto *heap = reinterpret_cast<CatalogHeapPage *>(page_buffer);

                if (slot_index >= heap->record_count)
                {
                    bp->unpinPage(page_id, false, ctx);
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid slot index");
                    return Status::INVALID_ARGUMENT;
                }

                uint32_t offset = sizeof(CatalogHeapPage) + (slot_index * sizeof(RecordType));
                auto *record = reinterpret_cast<RecordType *>(
                    reinterpret_cast<uint8_t *>(page_buffer) + offset);

                *record = updated_record;

                return bp->unpinPage(page_id, true, ctx);  // Mark as dirty
            }

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
            auto deleteTableRecord(const ID &table_id, ErrorContext *ctx) -> Status;
            auto readTableRecords(ErrorContext *ctx) -> Status;
            auto writeColumnRecords(const ID &table_id, const std::vector<ColumnInfo> &columns,
                                        ErrorContext *ctx) -> Status;
            auto readColumnRecords(const ID &table_id, ErrorContext *ctx) -> Status;
            auto writeIndexRecord(const IndexInfo &index, ErrorContext *ctx) -> Status;
            auto readIndexRecords(ErrorContext *ctx) -> Status;

            // Helper to allocate catalog pages
            auto allocateCatalogPage(uint32_t &page_id, ErrorContext *ctx) -> Status;
        };

        // DataType enum is now defined in types.h

    } // namespace scratchbird::core

