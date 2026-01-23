#include "scratchbird/core/toast.h"
#include "scratchbird/core/toast_visibility.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/btree.h"
#include "scratchbird/core/compression.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/gpid.h"
#include <cstring>
#include <algorithm>

// Simple value structure for TOAST usage

namespace scratchbird::core
{
    using IndexType = CatalogManager::IndexType;

    struct Value
    {
        enum Type
        {
            INT32,
            BYTEA
        };
        Type type;
        union
        {
            int32_t int_val;
            struct
            {
                const uint8_t *data;
                uint32_t size;
            } bytea_val;
        };

        Value() : type(INT32), int_val(0) {}
        Value(int32_t val) : type(INT32), int_val(val) {}
        Value(const std::vector<uint8_t> &data) : type(BYTEA)
        {
            bytea_val.data = data.data();
            bytea_val.size = data.size();
        }
        Value(std::vector<uint8_t> &&data) : type(BYTEA)
        {
            bytea_val.data = data.data();
            bytea_val.size = data.size();
        }
    };

} // namespace scratchbird::core

namespace
{
    bool isZeroIdLocal(const scratchbird::core::ID& id)
    {
        for (uint8_t byte : id.bytes)
        {
            if (byte != 0)
            {
                return false;
            }
        }
        return true;
    }

    auto resolveToastIndexById(scratchbird::core::Database* db,
                               const scratchbird::core::ID& toast_table_id,
                               scratchbird::core::CatalogManager::IndexInfo& index_info_out,
                               scratchbird::core::ErrorContext* ctx) -> scratchbird::core::Status
    {
        if (!db)
        {
            SET_ERROR_CONTEXT(ctx, scratchbird::core::Status::INVALID_ARGUMENT,
                              "Database not available");
            return scratchbird::core::Status::INVALID_ARGUMENT;
        }
        auto* catalog = db->catalog_manager();
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, scratchbird::core::Status::INVALID_ARGUMENT,
                              "CatalogManager not available");
            return scratchbird::core::Status::INVALID_ARGUMENT;
        }

        std::vector<scratchbird::core::CatalogManager::ColumnInfo> columns;
        auto status = catalog->getColumns(toast_table_id, columns, ctx);
        if (status != scratchbird::core::Status::OK)
        {
            return status;
        }

        scratchbird::core::ID chunk_id_col{};
        scratchbird::core::ID chunk_seq_col{};
        for (const auto& col : columns)
        {
            if (col.ordinal == 0)
            {
                chunk_id_col = col.column_id;
            }
            else if (col.ordinal == 1)
            {
                chunk_seq_col = col.column_id;
            }
        }

        if (isZeroIdLocal(chunk_id_col) || isZeroIdLocal(chunk_seq_col))
        {
            SET_ERROR_CONTEXT(ctx, scratchbird::core::Status::INVALID_ARGUMENT,
                              "TOAST column IDs missing");
            return scratchbird::core::Status::INVALID_ARGUMENT;
        }

        std::vector<scratchbird::core::CatalogManager::IndexInfo> indexes;
        status = catalog->listIndexesForTable(toast_table_id, indexes, ctx, true);
        if (status != scratchbird::core::Status::OK)
        {
            return status;
        }

        for (const auto& index_info : indexes)
        {
            if (index_info.index_type != scratchbird::core::CatalogManager::IndexType::BTREE)
            {
                continue;
            }
            if (index_info.column_ids.size() < 2)
            {
                continue;
            }
            if (index_info.column_ids[0] == chunk_id_col &&
                index_info.column_ids[1] == chunk_seq_col)
            {
                index_info_out = index_info;
                return scratchbird::core::Status::OK;
            }
        }

        SET_ERROR_CONTEXT(ctx, scratchbird::core::Status::NOT_FOUND, "TOAST index not found");
        return scratchbird::core::Status::NOT_FOUND;
    }
} // namespace

namespace scratchbird::core
{

    ToastManager::ToastManager(Database *db, const ID &table_id)
        : db_(db), table_id_(table_id), toast_table_id_(), next_value_id_(1)
    {
    }

    ToastManager::~ToastManager() = default;

    auto ToastManager::initializeNextValueId(ErrorContext *ctx) -> Status
    {
        CatalogManager *catalog = db_->catalog_manager();
        if (catalog == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "CatalogManager not available");
            return Status::INVALID_ARGUMENT;
        }

        // Use the TOAST index to find the maximum chunk_id without heap scans.
        CatalogManager::IndexInfo index_info;
        Status status = resolveToastIndexById(db_, toast_table_id_, index_info, ctx);
        if (status != Status::OK)
        {
            next_value_id_ = 1;
            return Status::OK;
        }

        SBBTreeIndex btree_info;
        btree_info.idx_uuid = index_info.index_id;
        btree_info.idx_table_uuid = index_info.table_id;
        btree_info.idx_root_page = static_cast<uint32_t>(getPageNumber(index_info.root_gpid));
        btree_info.idx_tablespace_id = getTablespaceID(index_info.root_gpid);

        BTree btree(db_, btree_info);
        auto iter = btree.rangeScan(nullptr, nullptr, 0, true, true, ctx);
        if (!iter)
        {
            next_value_id_ = 1;
            return Status::OK;
        }

        uint32_t max_value_id = 0;
        std::vector<uint8_t> key;
        TID tid;
        while (iter->hasNext())
        {
            Status next_status = iter->next(&key, &tid, ctx);
            if (next_status != Status::OK)
            {
                break;
            }

            if (key.size() < sizeof(uint32_t))
            {
                continue;
            }

            uint32_t chunk_id = 0;
            std::memcpy(&chunk_id, key.data(), sizeof(uint32_t));
            if (chunk_id > max_value_id)
            {
                max_value_id = chunk_id;
            }
        }

        // Set next_value_id_ to one more than the maximum found
        next_value_id_ = max_value_id + 1;

        return Status::OK;
    }

    auto ToastManager::initialize(ErrorContext *ctx) -> Status
    {
        // Check if TOAST table already exists
        CatalogManager *catalog = db_->catalog_manager();

        // TOAST table naming convention: sb_toast_<table_id>
        std::string toast_name = "sb_toast_" + table_id_.toString();

        // Get the schema of the parent table first (policy TOAST uses a sys fallback).
        CatalogManager::TableInfo parent_info;
        Status status = catalog->getTable(table_id_, parent_info, ctx);
        if (status != Status::OK)
        {
            if (catalog && table_id_ == catalog->policyToastTableId())
            {
                CatalogManager::SchemaInfo schema_info;
                Status schema_status = catalog->getSchema("sys", schema_info, ctx);
                if (schema_status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, schema_status,
                                      "Failed to resolve sys schema for policy TOAST");
                    return schema_status;
                }
                parent_info.schema_id = schema_info.schema_id;
                parent_info.tablespace_id = 0;
            }
            else
            {
                // If we can't get parent table info, something is wrong
                SET_ERROR_CONTEXT(ctx, status, "Failed to get parent table info");
                return status;
            }
        }
        else if (catalog && table_id_ == catalog->policyToastTableId())
        {
            // Policy TOAST table ID is the TOAST table itself.
            toast_table_id_ = table_id_;
            status = initializeNextValueId(ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to initialize next_value_id");
                return status;
            }
            return Status::OK;
        }

        CatalogManager::TableInfo info;
        status = catalog->getTable(parent_info.schema_id, toast_name, info, ctx);
        if (status == Status::OK)
        {
            // TOAST table already exists
            toast_table_id_ = info.table_id;

            // Read max value_id from TOAST table to prevent collisions
            status = initializeNextValueId(ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to initialize next_value_id");
                return status;
            }

            return Status::OK;
        }

        // Create TOAST table if it doesn't exist
        return createToastTableWithParent(parent_info.schema_id, parent_info.tablespace_id, ctx);
    }

    auto ToastManager::createToastTableWithParent(const ID& schema_id, uint16_t tablespace_id,
                                                 ErrorContext *ctx) -> Status
    {
        CatalogManager *catalog = db_->catalog_manager();

        // TOAST table schema:
        // chunk_id: INT (TOAST value ID)
        // chunk_seq: INT (sequence number)
        // chunk_data: BYTEA (actual data)
        std::vector<CatalogManager::ColumnInfo> columns;

        // chunk_id column
        CatalogManager::ColumnInfo col1;
        col1.column_name = "chunk_id";
        col1.data_type = static_cast<uint16_t>(DataType::INT32);
        col1.max_length = 4;
        col1.nullable = false;
        col1.has_default = false;
        columns.push_back(col1);

        // chunk_seq column
        CatalogManager::ColumnInfo col2;
        col2.column_name = "chunk_seq";
        col2.data_type = static_cast<uint16_t>(DataType::INT32);
        col2.max_length = 4;
        col2.nullable = false;
        col2.has_default = false;
        columns.push_back(col2);

        // chunk_data column - use page-size-based max chunk size
        uint32_t max_chunk_size = ToastSettings::getMaxChunkSize(db_->page_size());
        CatalogManager::ColumnInfo col3;
        col3.column_name = "chunk_data";
        col3.data_type = static_cast<uint16_t>(DataType::BYTEA);
        col3.max_length = max_chunk_size;
        col3.nullable = false;
        col3.has_default = false;
        columns.push_back(col3);

        std::string toast_name = "sb_toast_" + table_id_.toString();

        // Create TOAST table in same tablespace as parent (Phase 2 Task 2.3)
        CatalogManager::TableCreateOptions options;
        options.table_type = CatalogManager::TableType::TOAST;
        if (catalog && table_id_ == catalog->policyToastTableId())
        {
            options.force_table_id = true;
            options.forced_table_id = table_id_;
        }
        Status status = catalog->createTable(schema_id, toast_name, columns, toast_table_id_,
                                             tablespace_id, ctx, &options);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to create TOAST table");
            return status;
        }

        // Create index on (chunk_id, chunk_seq) for efficient retrieval
        // CRITICAL: Index is required for performance - TOAST lookups without index
        // would degrade to O(N) heap scans instead of O(log N) index lookups
        std::vector<std::string> index_columns = {"chunk_id", "chunk_seq"};
        ID index_id;
        std::string index_name = toast_name + "_idx";
        // Create TOAST index in same tablespace as parent (Phase 2 Task 2.3)
        status = catalog->createIndex(toast_table_id_, index_name, index_columns,
                                      std::vector<std::string>{}, index_id, false,
                                      IndexType::BTREE, tablespace_id, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status,
                              "Failed to create TOAST index - index is required for performance");
            return status;
        }

        return Status::OK;
    }

    auto ToastManager::createToastTable(ErrorContext *ctx) -> Status
    {
        CatalogManager *catalog = db_->catalog_manager();

        // Get the schema of the parent table
        CatalogManager::TableInfo parent_info;
        Status status = catalog->getTable(table_id_, parent_info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to get parent table info");
            return status;
        }

        return createToastTableWithParent(parent_info.schema_id, parent_info.tablespace_id, ctx);
    }

    auto ToastManager::toastValue(const uint8_t *data, uint32_t size, ToastStrategy strategy,
                                  uint64_t xmin, ToastPointer *pointer_out, ErrorContext *ctx)
        -> Status
    {
        if ((data == nullptr) || (pointer_out == nullptr))
        {
            LOG_ERROR(STORAGE, "ToastManager::toastValue null input: data=%p pointer=%p",
                      static_cast<const void *>(data),
                      static_cast<void *>(pointer_out));
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null pointer in toast_value");
            return Status::INVALID_ARGUMENT;
        }

        // Assign unique value ID with atomic fetch_add for thread safety
        // Check for wraparound - if we're approaching UINT32_MAX, we need to handle it
        uint32_t value_id = next_value_id_.fetch_add(1, std::memory_order_relaxed);

        // Overflow protection: Check if we've wrapped around to 0
        // Value ID 0 is reserved/invalid, so if we hit it, we have a problem
        if (value_id == 0 || value_id == UINT32_MAX)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL,
                              "TOAST value ID exhausted - too many TOAST values created");
            return Status::PAGE_FULL;
        }

        // Initialize pointer
        pointer_out->va_header = 0x01; // TOAST marker
        pointer_out->va_tag = static_cast<uint8_t>(strategy);
        pointer_out->va_rawsize = size;
        pointer_out->va_valueid = value_id;
        pointer_out->va_toastrelid = 0; // toast_table_id_;

        // Handle based on strategy
        switch (strategy)
        {
            case ToastStrategy::PLAIN:
                // Shouldn't happen - PLAIN means no TOAST
                LOG_ERROR(STORAGE, "ToastManager::toastValue invalid strategy: PLAIN");
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "PLAIN strategy used for TOAST");
                return Status::INVALID_ARGUMENT;

            case ToastStrategy::EXTENDED:
                // Store out-of-line, uncompressed
                pointer_out->va_extsize = size;
                {
                    Status status = writeToastChunks(value_id, data, size, xmin, ctx);
                    if (status != Status::OK)
                    {
                        LOG_ERROR(STORAGE, "ToastManager::toastValue failed to write chunks (table=%s): %d %s",
                                  toast_table_id_.toString().c_str(),
                                  static_cast<int>(status),
                                  ctx ? ctx->message.c_str() : "");
                    }
                    return status;
                }

            case ToastStrategy::COMPRESSED:
                // This strategy would store compressed inline - not for TOAST
                LOG_ERROR(STORAGE, "ToastManager::toastValue invalid strategy: COMPRESSED");
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "COMPRESSED strategy not supported for TOAST");
                return Status::INVALID_ARGUMENT;

            case ToastStrategy::EXTERNAL:
            {
                // Store out-of-line, compressed
                std::vector<uint8_t> compressed;
                Status status = compressData(data, size, &compressed, ctx);
                if (status != Status::OK)
                {
                    // Fall back to uncompressed
                    pointer_out->va_tag = static_cast<uint8_t>(ToastStrategy::EXTENDED);
                    pointer_out->va_extsize = size;
                {
                    Status status = writeToastChunks(value_id, data, size, xmin, ctx);
                    if (status != Status::OK)
                    {
                        LOG_ERROR(STORAGE, "ToastManager::toastValue failed to write chunks (table=%s): %d %s",
                                  toast_table_id_.toString().c_str(),
                                  static_cast<int>(status),
                                  ctx ? ctx->message.c_str() : "");
                    }
                    return status;
                }
            }

                pointer_out->va_extsize = compressed.size();
                return writeToastChunks(value_id, compressed.data(), compressed.size(), xmin, ctx);
            }

            default:
                LOG_ERROR(STORAGE, "ToastManager::toastValue invalid strategy: %u",
                          static_cast<unsigned int>(strategy));
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unknown TOAST strategy");
                return Status::INVALID_ARGUMENT;
        }
    }

    auto ToastManager::detoastValue(const ToastPointer *pointer, std::vector<uint8_t> *data_out,
                                    uint64_t xmin, ErrorContext *ctx) -> Status
    {
        if ((pointer == nullptr) || (data_out == nullptr))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null pointer in detoast_value");
            return Status::INVALID_ARGUMENT;
        }

        // Verify it's a TOAST pointer
        if (pointer->va_header != 0x01)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Not a TOAST pointer");
            return Status::INVALID_ARGUMENT;
        }

        auto strategy = static_cast<ToastStrategy>(pointer->va_tag);

        switch (strategy)
        {
            case ToastStrategy::EXTENDED:
            {
                // Read uncompressed chunks
                data_out->clear();
                return readToastChunks(pointer->va_valueid, data_out, xmin, ctx);
            }

            case ToastStrategy::EXTERNAL:
            {
                // Read compressed chunks and decompress
                std::vector<uint8_t> compressed;
                Status status = readToastChunks(pointer->va_valueid, &compressed, xmin, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                return decompressData(compressed.data(), compressed.size(), pointer->va_rawsize,
                                      data_out, ctx);
            }

            default:
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Invalid TOAST strategy in pointer");
                return Status::INVALID_ARGUMENT;
        }
    }

    auto ToastManager::deleteToastValue(uint32_t value_id, uint64_t xmax, ErrorContext *ctx)
        -> Status
    {
        StorageEngine *storage = db_->storage_engine();

        // Get the index ID for the TOAST table
        CatalogManager::IndexInfo index_info;
        Status status = resolveToastIndexById(db_, toast_table_id_, index_info, ctx);
        if (status != Status::OK)
        {
            // Fall back to heap scan if index not found
            return deleteToastValueHeapScan(value_id, xmax, ctx);
        }

        // Create an index scan
        auto scan = storage->createIndexScan(index_info.index_id, ctx);
        if (!scan)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Failed to create index scan for TOAST table");
            return Status::INVALID_ARGUMENT;
        }

        // Seek to the first chunk for this value_id
        std::vector<uint8_t> key;
        key.insert(key.end(), reinterpret_cast<const uint8_t *>(&value_id),
                   reinterpret_cast<const uint8_t *>(&value_id) + 4);
        status = scan->seek(key, ctx);
        if (status != Status::OK)
        {
            // Value not found is OK - might have been already deleted
            if (status == Status::NOT_FOUND)
            {
                return Status::OK;
            }
            return status;
        }

        // Delete all chunks with this value_id
        Tuple tuple;
        while ((status = scan->next(&tuple, ctx)) == Status::OK)
        {
            // Parse tuple to verify it's for our value_id
            if ((tuple.data == nullptr) ||
                tuple.data_size < sizeof(TupleHeader) + sizeof(uint32_t))
            {
                continue;
            }

            const uint8_t *ptr = tuple.data + sizeof(TupleHeader);
            uint32_t chunk_id = *reinterpret_cast<const uint32_t *>(ptr);
            if (chunk_id != value_id)
            {
                // We've moved past our value_id in the index
                break;
            }

            // MGA-compliant soft delete: Update xmax field only (do not mark item pointer as deleted)
            // This allows transactions that started before this delete to still see the chunk
            uint32_t page_id = static_cast<uint32_t>(getPageNumber(tuple.tid));
            uint16_t item_id = getSlot(tuple.tid);
            Status delete_status = markToastChunkDeleted(page_id, item_id, xmax, ctx);
            if (delete_status != Status::OK)
            {
                return delete_status;
            }
        }

        return Status::OK;
    }

    auto ToastManager::deleteToastValueHeapScan(uint32_t value_id, uint64_t xmax, ErrorContext *ctx)
        -> Status
    {
        StorageEngine *storage = db_->storage_engine();

        // Mark all chunks of this value as deleted
        // In a real implementation, this would use an index on chunk_id
        // For now, we'll scan the TOAST table (inefficient but correct)

        auto scan = storage->createScan(toast_table_id_, ctx);
        if (!scan)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Failed to scan TOAST table");
            return Status::INVALID_ARGUMENT;
        }

        Tuple tuple;
        Status status;
        while ((status = scan->next(&tuple, ctx)) == Status::OK)
        {
            if ((tuple.data == nullptr) ||
                tuple.data_size < sizeof(TupleHeader) + sizeof(uint32_t))
            {
                continue;
            }

            const uint8_t *ptr = tuple.data + sizeof(TupleHeader);
            uint32_t chunk_id = *reinterpret_cast<const uint32_t *>(ptr);
            if (chunk_id == value_id)
            {
                // MGA-compliant soft delete: Update xmax field only (do not mark item pointer as deleted)
                // This allows transactions that started before this delete to still see the chunk
                uint32_t page_id = static_cast<uint32_t>(getPageNumber(tuple.tid));
                uint16_t item_id = getSlot(tuple.tid);
                Status delete_status = markToastChunkDeleted(page_id, item_id, xmax, ctx);
                if (delete_status != Status::OK)
                {
                    return delete_status;
                }
            }
        }

        return Status::OK;
    }

    auto ToastManager::chooseStrategy(const uint8_t *data, uint32_t size, uint32_t page_size,
                                      bool compress_enabled) -> ToastStrategy
    {
        // Use page-size-based threshold for optimal performance
        uint32_t threshold = ToastSettings::getThreshold(page_size);
        uint32_t target = ToastSettings::getTarget(page_size);

        // Don't TOAST small values
        if (size <= threshold)
        {
            return ToastStrategy::PLAIN;
        }

        // If compression is disabled, use uncompressed out-of-line storage
        if (!compress_enabled)
        {
            return ToastStrategy::EXTENDED;
        }

        // For large values (>2x target), try compression
        // In a real implementation, we might sample the data to estimate
        // compressibility before deciding
        if (size > target * 2)
        {
            return ToastStrategy::EXTERNAL; // Out-of-line, compressed
        }

        return ToastStrategy::EXTENDED; // Out-of-line, uncompressed
    }

    // Phase 3: Index TOAST Integration - Helper methods
    auto ToastManager::isToastPointer(const uint8_t *data, size_t size) -> bool
    {
        // TOAST pointer is exactly 18 bytes
        if (size != sizeof(ToastPointer))
        {
            return false;
        }

        // Check magic bytes (first 2 bytes of ToastPointer)
        // ToastPointer format: va_header (2) | va_rawsize (4) | va_extsize (4) |
        //                      va_valueid (4) | va_toastrelid (4)
        const ToastPointer *ptr = reinterpret_cast<const ToastPointer *>(data);

        // Check if va_header indicates external storage
        // TOAST strategies: PLAIN=0, EXTENDED=1, COMPRESSED=2, EXTERNAL=3
        uint16_t header = ptr->va_header;
        ToastStrategy strategy = static_cast<ToastStrategy>(header & 0x03);

        return (strategy == ToastStrategy::EXTENDED ||
                strategy == ToastStrategy::EXTERNAL ||
                strategy == ToastStrategy::COMPRESSED);
    }

    auto ToastManager::detoastIfNeeded(const uint8_t *data, size_t size,
                                       std::vector<uint8_t> *result, uint64_t xid,
                                       ErrorContext *ctx) -> Status
    {
        if (isToastPointer(data, size))
        {
            // Data is a TOAST pointer, detoast it
            const ToastPointer *pointer = reinterpret_cast<const ToastPointer *>(data);
            return detoastValue(pointer, result, xid, ctx);
        }
        else
        {
            // Not a TOAST pointer, return original data
            result->assign(data, data + size);
            return Status::OK;
        }
    }

    auto ToastManager::writeToastChunks(uint32_t value_id, const uint8_t *data, uint32_t size,
                                        uint64_t xmin, ErrorContext *ctx) -> Status
    {
        StorageEngine *storage = db_->storage_engine();

        // Use page-size-based chunk size for optimal performance
        uint32_t max_chunk_size = ToastSettings::getMaxChunkSize(db_->page_size());

        // MEDIUM-2 FIX: Check for integer overflow before chunk calculation
        // If size is close to UINT32_MAX, adding max_chunk_size would overflow
        if (size > UINT32_MAX - max_chunk_size + 1)
        {
            SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE,
                              "TOAST value too large - would cause integer overflow in chunk calculation");
            return Status::OUT_OF_RANGE;
        }

        uint64_t effective_xmin = xmin;
        if (effective_xmin == 0)
        {
            static constexpr uint64_t kFrozenXid = 2;
            effective_xmin = kFrozenXid;
        }

        // Split data into chunks
        uint32_t chunks_needed = (size + max_chunk_size - 1) / max_chunk_size;
        uint32_t offset = 0;

        // Track inserted chunks for cleanup on failure
        std::vector<std::pair<uint32_t, uint16_t>> inserted_chunks;
        inserted_chunks.reserve(chunks_needed);

        for (uint32_t seq = 0; seq < chunks_needed; seq++)
        {
            // MEDIUM-3 FIX: Validate offset before calculating chunk_size
            // Prevent underflow if offset exceeds size (defensive check)
            if (offset > size)
            {
                SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE,
                                  "TOAST offset exceeds data size - internal error");
                return Status::OUT_OF_RANGE;
            }

            uint32_t chunk_size = std::min(max_chunk_size, size - offset);

            // Build tuple data with TupleHeader + TOAST chunk metadata
            // Format: TupleHeader | chunk_id (4) | chunk_seq (4) | chunk_size (4) | data
            std::vector<uint8_t> tuple_data;
            tuple_data.reserve(sizeof(TupleHeader) + 12 + chunk_size);

            TupleHeader toast_hdr{};
            toast_hdr.xmin = effective_xmin;
            toast_hdr.xmax = 0;
            toast_hdr.back_version_gpid = INVALID_GPID;
            toast_hdr.back_version_slot = 0;
            toast_hdr.reserved1 = 0;
            toast_hdr.ctid_gpid = INVALID_GPID;
            toast_hdr.ctid_slot = 0;
            toast_hdr.infomask = 0;
            toast_hdr.null_bitmap_offset = 0;
            toast_hdr.padding = 0;

            tuple_data.insert(tuple_data.end(),
                              reinterpret_cast<const uint8_t *>(&toast_hdr),
                              reinterpret_cast<const uint8_t *>(&toast_hdr) + sizeof(TupleHeader));

            // Add chunk_id
            uint32_t id = value_id;
            tuple_data.insert(tuple_data.end(), reinterpret_cast<const uint8_t *>(&id),
                              reinterpret_cast<const uint8_t *>(&id) + 4);

            // Add chunk_seq
            tuple_data.insert(tuple_data.end(), reinterpret_cast<const uint8_t *>(&seq),
                              reinterpret_cast<const uint8_t *>(&seq) + 4);

            // Add chunk_size
            tuple_data.insert(tuple_data.end(), reinterpret_cast<const uint8_t *>(&chunk_size),
                              reinterpret_cast<const uint8_t *>(&chunk_size) + 4);

            // Add chunk data
            tuple_data.insert(tuple_data.end(), data + offset, data + offset + chunk_size);

            // Insert tuple
            uint32_t page_id;
            uint16_t item_id;
            Status status = storage->insertTuple(toast_table_id_, tuple_data.data(),
                                                 tuple_data.size(), &page_id, &item_id, ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to insert TOAST chunk");

                // Clean up any chunks we already inserted
                for (const auto &chunk : inserted_chunks)
                {
                    storage->deleteTuple(toast_table_id_, chunk.first, chunk.second,
                                         UINT16_MAX, ctx);
                }

                return status;
            }

            // Track this chunk for potential cleanup
            inserted_chunks.push_back({page_id, item_id});

            offset += chunk_size;
        }

        return Status::OK;
    }

    auto ToastManager::readToastChunks(uint32_t value_id, std::vector<uint8_t> *data_out,
                                       uint64_t xmin, ErrorContext *ctx) -> Status
    {
        StorageEngine *storage = db_->storage_engine();
        BufferPool *buffer_pool = db_->buffer_pool();

        // Use page-size-based chunk size for validation (allow legacy chunk sizing)
        uint32_t max_chunk_size = ToastSettings::getMaxChunkSize(db_->page_size());
        uint32_t max_chunk_size_allowed = std::max(
            max_chunk_size, ToastSettings::getLegacyMaxChunkSize(db_->page_size()));

        // Get the index ID for the TOAST table
        CatalogManager::IndexInfo index_info;
        Status status = resolveToastIndexById(db_, toast_table_id_, index_info, ctx);
        if (status != Status::OK)
        {
            // Fall back to heap scan if index not found
            return readToastChunksHeapScan(value_id, data_out, xmin, ctx);
        }
        // The index scan only supports exact key lookup. For composite keys, we
        // cannot search by prefix (chunk_id) yet, so fall back to heap scan.
        if (index_info.column_ids.size() != 1)
        {
            return readToastChunksHeapScan(value_id, data_out, xmin, ctx);
        }

        // Create an index scan
        auto scan = storage->createIndexScan(index_info.index_id, ctx);
        if (!scan)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Failed to create index scan for TOAST table");
            return Status::INVALID_ARGUMENT;
        }

        // Seek to the first chunk for this value_id
        std::vector<uint8_t> key;
        key.insert(key.end(), reinterpret_cast<const uint8_t *>(&value_id),
                   reinterpret_cast<const uint8_t *>(&value_id) + 4);
        status = scan->seek(key, ctx);
        if (status != Status::OK)
        {
            return readToastChunksHeapScan(value_id, data_out, xmin, ctx);
        }

        // ========================================================================
        // P2-3: TOAST Chunk Prefetching
        // Phase 1: Collect all TIDs for this value_id from index
        // ========================================================================
        std::vector<TID> chunk_tids;
        Tuple tid_tuple;
        while ((status = scan->next(&tid_tuple, ctx)) == Status::OK)
        {
            // Index scan returns TID in tuple.tid
            // We need to verify this is still our value_id by checking the key
            // Since we can't easily check the key from just the TID, we'll collect
            // all TIDs returned by the index scan for this key
            chunk_tids.push_back(tid_tuple.tid);
        }

        if (chunk_tids.empty())
        {
            return readToastChunksHeapScan(value_id, data_out, xmin, ctx);
        }

        // ========================================================================
        // P2-3: Phase 2: Prefetch all unique pages containing chunks
        // ========================================================================
        std::vector<GPID> gpids;
        gpids.reserve(chunk_tids.size());
        for (const TID &tid : chunk_tids)
        {
            // TID contains GPID (which includes tablespace + page number)
            gpids.push_back(tid.gpid);
        }

        // Prefetch all pages - this reads them into buffer pool cache
        buffer_pool->prefetchPagesGlobal(gpids, ctx);

        // ========================================================================
        // P2-3: Phase 3: Read chunks from (now cached) pages
        // ========================================================================
        struct ChunkData
        {
            uint32_t seq;
            std::vector<uint8_t> data;
        };
        std::vector<ChunkData> chunks;

        for (const TID &tid : chunk_tids)
        {
            // Get tuple data using storage engine
            Tuple tuple;
            status = storage->getTuple(toast_table_id_, tid, &tuple, ctx);
            if (status != Status::OK)
            {
                continue; // Skip failed reads
            }

            // Parse tuple format: TupleHeader | chunk_id | chunk_seq | chunk_size | data
            if ((tuple.data == nullptr) ||
                tuple.data_size < sizeof(TupleHeader) + 12)
            {
                continue;
            }

            const uint8_t *ptr = tuple.data;
            auto *tuple_hdr = reinterpret_cast<const TupleHeader *>(ptr);
            uint64_t chunk_xmin = tuple_hdr->xmin;
            uint64_t chunk_xmax = tuple_hdr->xmax;
            ptr += sizeof(TupleHeader);

            // Parse chunk_id
            uint32_t chunk_id = *reinterpret_cast<const uint32_t *>(ptr);

            if (chunk_id != value_id)
            {
                // Not our value_id, skip
                continue;
            }

            ptr += 4;
            // Parse chunk_seq
            uint32_t chunk_seq = *reinterpret_cast<const uint32_t *>(ptr);
            ptr += 4;
            // Parse chunk_size
            uint32_t chunk_size = *reinterpret_cast<const uint32_t *>(ptr);
            ptr += 4;

            // Validate chunk size against page-size-based maximum
            if (chunk_size > max_chunk_size_allowed ||
                sizeof(TupleHeader) + 12 + chunk_size > tuple.data_size)
            {
                continue;
            }

            if (xmin != 0)
            {
                // Phase 2: TIP-based visibility check (Firebird MGA)
                // Check if this chunk is visible to the current transaction
                TransactionManager *tm = db_->transaction_manager();
                if (!ToastVisibility::isChunkVisible(chunk_xmin, chunk_xmax, xmin, tm))
                {
                    continue; // Skip invisible chunk
                }
            }

            // Extract chunk data
            std::vector<uint8_t> chunk_data(chunk_size);
            memcpy(chunk_data.data(), ptr, chunk_size);

            chunks.push_back({chunk_seq, std::move(chunk_data)});
        }

        if (chunks.empty())
        {
            return readToastChunksHeapScan(value_id, data_out, xmin, ctx);
        }

        // Sort chunks by sequence number (index should return them in order, but be safe)
        std::sort(chunks.begin(), chunks.end(),
                  [](const ChunkData &a, const ChunkData &b) { return a.seq < b.seq; });

        // Reassemble data
        data_out->clear();
        for (const auto &chunk : chunks)
        {
            data_out->insert(data_out->end(), chunk.data.begin(), chunk.data.end());
        }

        return Status::OK;
    }

    auto ToastManager::readToastChunksHeapScan(uint32_t value_id, std::vector<uint8_t> *data_out,
                                               uint64_t xmin, ErrorContext *ctx) -> Status
    {
        StorageEngine *storage = db_->storage_engine();

        // Use page-size-based chunk size for validation
        uint32_t max_chunk_size = ToastSettings::getMaxChunkSize(db_->page_size());
        uint32_t max_chunk_size_allowed = std::max(
            max_chunk_size, ToastSettings::getLegacyMaxChunkSize(db_->page_size()));

        // Scan TOAST table for chunks with this value_id
        // In a real implementation, we'd use an index
        auto scan = storage->createScan(toast_table_id_, ctx);
        if (!scan)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Failed to scan TOAST table");
            return Status::INVALID_ARGUMENT;
        }

        // Collect chunks
        struct ChunkData
        {
            uint32_t seq;
            std::vector<uint8_t> data;
        };
        std::vector<ChunkData> chunks;

        Tuple tuple;
        Status status;
        while ((status = scan->next(&tuple, ctx)) == Status::OK)
        {
            // Parse tuple format: TupleHeader | chunk_id | chunk_seq | chunk_size | data
            if ((tuple.data == nullptr) ||
                tuple.data_size < sizeof(TupleHeader) + 12)
            {
                continue;
            }

            const uint8_t *ptr = tuple.data;
            auto *tuple_hdr = reinterpret_cast<const TupleHeader *>(ptr);
            uint64_t chunk_xmin = tuple_hdr->xmin;
            uint64_t chunk_xmax = tuple_hdr->xmax;
            ptr += sizeof(TupleHeader);

            // Parse chunk_id
            uint32_t chunk_id = *reinterpret_cast<const uint32_t *>(ptr);
            ptr += 4;

            if (chunk_id != value_id)
            {
                continue;
            }

            // Parse chunk_seq
            uint32_t chunk_seq = *reinterpret_cast<const uint32_t *>(ptr);
            ptr += 4;

            // Parse chunk_size
            uint32_t chunk_size = *reinterpret_cast<const uint32_t *>(ptr);
            ptr += 4;

            if (xmin != 0)
            {
                // Phase 2: TIP-based visibility check (Firebird MGA)
                // Check if this chunk is visible to the current transaction
                TransactionManager *tm = db_->transaction_manager();
                if (!ToastVisibility::isChunkVisible(chunk_xmin, chunk_xmax, xmin, tm))
                {
                    continue; // Skip invisible chunk
                }
            }

            // Validate chunk size against page-size-based maximum
            if (chunk_size > max_chunk_size_allowed ||
                sizeof(TupleHeader) + 12 + chunk_size > tuple.data_size)
            {
                continue;
            }

            // Extract chunk data
            std::vector<uint8_t> chunk_data(chunk_size);
            memcpy(chunk_data.data(), ptr, chunk_size);

            chunks.push_back({chunk_seq, std::move(chunk_data)});
        }

        if (chunks.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "TOAST value not found");
            return Status::NOT_FOUND;
        }

        // Sort chunks by sequence number
        std::sort(chunks.begin(), chunks.end(),
                  [](const ChunkData &a, const ChunkData &b) { return a.seq < b.seq; });

        // Reassemble data
        data_out->clear();
        for (const auto &chunk : chunks)
        {
            data_out->insert(data_out->end(), chunk.data.begin(), chunk.data.end());
        }

        return Status::OK;
    }

    auto ToastManager::compressData(const uint8_t *src, uint32_t src_size,
                                    std::vector<uint8_t> *dst, ErrorContext *ctx) -> Status
    {
        // Use compression framework if available
        auto codec = CompressionFactory::create(CompressionType::LZ4);
        if (!codec)
        {
            // No compression available
            return Status::INVALID_ARGUMENT;
        }

        // Reserve space for compressed header + data
        uint32_t max_size = codec->maxCompressedSize(src_size);
        dst->resize(sizeof(ToastCompressHeader) + max_size);

        // Write header
        auto *header = reinterpret_cast<ToastCompressHeader *>(dst->data());
        header->rawsize = src_size;
        header->compression = static_cast<uint8_t>(CompressionType::LZ4);

        // Compress data
        uint32_t compressed_size;
        Status status = codec->compress(src, src_size, dst->data() + sizeof(ToastCompressHeader),
                                        max_size, &compressed_size);
        if (status != Status::OK)
        {
            return status;
        }

        // Resize to actual size
        dst->resize(sizeof(ToastCompressHeader) + compressed_size);

        // Only use compression if it saves space
        if (dst->size() >= src_size * 0.9)
        {
            return Status::INVALID_ARGUMENT; // Not worth compressing
        }

        return Status::OK;
    }

    auto ToastManager::decompressData(const uint8_t *src, uint32_t src_size,
                                      uint32_t uncompressed_size, std::vector<uint8_t> *dst,
                                      ErrorContext *ctx) -> Status
    {
        if (src_size < sizeof(ToastCompressHeader))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid compressed TOAST data");
            return Status::INVALID_ARGUMENT;
        }

        const auto *header = reinterpret_cast<const ToastCompressHeader *>(src);

        if (header->rawsize != uncompressed_size)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Compressed size mismatch");
            return Status::INVALID_ARGUMENT;
        }

        auto comp_type = static_cast<CompressionType>(header->compression);
        auto codec = CompressionFactory::create(comp_type);
        if (!codec)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Compression type not supported");
            return Status::INVALID_ARGUMENT;
        }

        dst->resize(uncompressed_size);
        uint32_t decompressed_size;

        Status status = codec->decompress(src + sizeof(ToastCompressHeader),
                                          src_size - sizeof(ToastCompressHeader), dst->data(),
                                          uncompressed_size, &decompressed_size);

        if (status != Status::OK || decompressed_size != uncompressed_size)
        {
            SET_ERROR_CONTEXT(ctx, Status::COMPRESSION_ERROR, "Failed to decompress TOAST data");
            return Status::COMPRESSION_ERROR;
        }

        return Status::OK;
    }

    auto ToastManager::markToastChunkDeleted(uint32_t page_id, uint16_t item_id, uint64_t xmax,
                                             ErrorContext *ctx) -> Status
    {
        // MGA-compliant soft delete: Update ONLY the xmax field in the tuple header
        // Do NOT mark the item pointer as deleted - this allows older transactions
        // that started before this delete to still see the chunk according to MGA rules

        BufferPool *buffer_pool = db_->buffer_pool();
        if (buffer_pool == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Buffer pool not available");
            return Status::INVALID_ARGUMENT;
        }

        // Pin the page containing the chunk
        uint8_t *page_buffer = nullptr;
        Status status = buffer_pool->pinPage(page_id, reinterpret_cast<void**>(&page_buffer), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // RAII guard to ensure page is unpinned on all exit paths
        struct PageUnpinGuard
        {
            BufferPool *pool;
            uint32_t pid;
            ~PageUnpinGuard()
            {
                pool->unpinPage(pid, false);
            }
        };
        PageUnpinGuard guard{buffer_pool, page_id};

        // Wrap the page with HeapPage for structured access
        HeapPage heap_page(page_buffer, db_->page_size());

        // Get the tuple data to access its header
        const uint8_t *tuple_data;
        uint32_t tuple_size;
        status = heap_page.getTuple(item_id, &tuple_data, &tuple_size, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Validate tuple has at least a TupleHeader
        if (tuple_size < sizeof(TupleHeader))
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "TOAST chunk too small for header");
            return Status::PAGE_CORRUPT;
        }

        // Update xmax field directly in the page buffer (bytes 8-15 of TupleHeader)
        // SAFETY: We have exclusive access via pin, and we're writing to a valid buffer
        auto *tuple_hdr = const_cast<TupleHeader *>(reinterpret_cast<const TupleHeader *>(tuple_data));
        tuple_hdr->xmax = xmax;

        // Set hint bit to indicate xmax is set (but NOT the deleted flag on item pointer)
        tuple_hdr->infomask |= TupleHeader::HEAP_XMAX_COMMITTED;

        // Mark page as dirty so changes are persisted
        buffer_pool->markDirty(page_id);

        // Page will be automatically unpinned by RAII guard
        return Status::OK;
    }

} // namespace scratchbird::core
