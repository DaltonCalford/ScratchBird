#include "scratchbird/core/toast.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/compression.h"
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

namespace scratchbird::core
{

    ToastManager::ToastManager(Database *db, const ID &table_id)
        : db_(db), table_id_(table_id), toast_table_id_(), next_value_id_(1)
    {
    }

    ToastManager::~ToastManager() = default;

    auto ToastManager::initializeNextValueId(ErrorContext *ctx) -> Status
    {
        StorageEngine *storage = db_->storage_engine();

        // Scan TOAST table to find maximum value_id
        auto scan = storage->createScan(toast_table_id_, ctx);
        if (!scan)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Failed to scan TOAST table");
            return Status::INVALID_ARGUMENT;
        }

        uint32_t max_value_id = 0;
        Tuple tuple;
        Status status;
        while ((status = scan->next(&tuple, ctx)) == Status::OK)
        {
            // Parse tuple format: chunk_id | chunk_seq | chunk_size | data
            if ((tuple.data != nullptr) && tuple.data_size >= 4)
            {
                uint32_t chunk_id = *reinterpret_cast<const uint32_t *>(tuple.data);
                if (chunk_id > max_value_id)
                {
                    max_value_id = chunk_id;
                }
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

        // TOAST table naming convention: pg_toast_<table_id>
        std::string toast_name = "pg_toast_" + table_id_.toString();

        // Get the schema of the parent table first
        CatalogManager::TableInfo parent_info;
        Status status = catalog->getTable(table_id_, parent_info, ctx);
        if (status != Status::OK)
        {
            // If we can't get parent table info, something is wrong
            SET_ERROR_CONTEXT(ctx, status, "Failed to get parent table info");
            return status;
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
        return createToastTable(ctx);
    }

    auto ToastManager::createToastTable(ErrorContext *ctx) -> Status
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

        // chunk_data column
        CatalogManager::ColumnInfo col3;
        col3.column_name = "chunk_data";
        col3.data_type = static_cast<uint16_t>(DataType::BYTEA);
        col3.max_length = TOAST_MAX_CHUNK_SIZE;
        col3.nullable = false;
        col3.has_default = false;
        columns.push_back(col3);

        std::string toast_name = "pg_toast_" + table_id_.toString();

        // Get the schema of the parent table
        CatalogManager::TableInfo parent_info;
        Status status = catalog->getTable(table_id_, parent_info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to get parent table info");
            return status;
        }

        status =
            catalog->createTable(parent_info.schema_id, toast_name, columns, toast_table_id_, ctx);
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
        status = catalog->createIndex(toast_table_id_, index_name, index_columns, index_id, false,
                                      IndexType::BTREE, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status,
                              "Failed to create TOAST index - index is required for performance");
            return status;
        }

        return Status::OK;
    }

    auto ToastManager::toastValue(const uint8_t *data, uint32_t size, ToastStrategy strategy,
                                  uint64_t xmin, ToastPointer *pointer_out, ErrorContext *ctx)
        -> Status
    {
        if ((data == nullptr) || (pointer_out == nullptr))
        {
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
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "PLAIN strategy used for TOAST");
                return Status::INVALID_ARGUMENT;

            case ToastStrategy::EXTENDED:
                // Store out-of-line, uncompressed
                pointer_out->va_extsize = size;
                return writeToastChunks(value_id, data, size, xmin, ctx);

            case ToastStrategy::COMPRESSED:
                // This strategy would store compressed inline - not for TOAST
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
                    return writeToastChunks(value_id, data, size, xmin, ctx);
                }

                pointer_out->va_extsize = compressed.size();
                return writeToastChunks(value_id, compressed.data(), compressed.size(), xmin, ctx);
            }

            default:
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
        CatalogManager *catalog = db_->catalog_manager();

        // Get the index ID for the TOAST table
        std::string toast_name = "pg_toast_" + table_id_.toString();
        std::string index_name = toast_name + "_idx";
        CatalogManager::IndexInfo index_info;
        Status status = catalog->getIndex(toast_table_id_, index_name, index_info, ctx);
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
            if ((tuple.data == nullptr) || tuple.data_size < 4)
            {
                continue;
            }

            uint32_t chunk_id = *reinterpret_cast<const uint32_t *>(tuple.data);
            if (chunk_id != value_id)
            {
                // We've moved past our value_id in the index
                break;
            }

            // Delete this chunk
            Status delete_status =
                storage->deleteTuple(toast_table_id_, tuple.page_id, tuple.item_id, ctx);
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
            if ((tuple.data == nullptr) || tuple.data_size < 4)
            {
                continue;
            }

            uint32_t chunk_id = *reinterpret_cast<const uint32_t *>(tuple.data);
            if (chunk_id == value_id)
            {
                // Delete this chunk
                Status delete_status =
                    storage->deleteTuple(toast_table_id_, tuple.page_id, tuple.item_id, ctx);
                if (delete_status != Status::OK)
                {
                    return delete_status;
                }
            }
        }

        return Status::OK;
    }

    auto ToastManager::chooseStrategy(const uint8_t *data, uint32_t size, bool compress_enabled)
        -> ToastStrategy
    {
        // Simple strategy selection
        if (size <= TOAST_TUPLE_THRESHOLD)
        {
            return ToastStrategy::PLAIN; // Don't TOAST small values
        }

        if (!compress_enabled)
        {
            return ToastStrategy::EXTENDED; // Out-of-line, uncompressed
        }

        // For large values, try compression
        // In a real implementation, we might sample the data to estimate
        // compressibility before deciding
        if (size > TOAST_TUPLE_THRESHOLD * 2)
        {
            return ToastStrategy::EXTERNAL; // Out-of-line, compressed
        }

        return ToastStrategy::EXTENDED; // Out-of-line, uncompressed
    }

    auto ToastManager::writeToastChunks(uint32_t value_id, const uint8_t *data, uint32_t size,
                                        uint64_t xmin, ErrorContext *ctx) -> Status
    {
        StorageEngine *storage = db_->storage_engine();

        // MEDIUM-2 FIX: Check for integer overflow before chunk calculation
        // If size is close to UINT32_MAX, adding TOAST_MAX_CHUNK_SIZE would overflow
        if (size > UINT32_MAX - TOAST_MAX_CHUNK_SIZE + 1)
        {
            SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE,
                              "TOAST value too large - would cause integer overflow in chunk calculation");
            return Status::OUT_OF_RANGE;
        }

        // Split data into chunks
        uint32_t chunks_needed = (size + TOAST_MAX_CHUNK_SIZE - 1) / TOAST_MAX_CHUNK_SIZE;
        uint32_t offset = 0;

        // Track inserted chunks for cleanup on failure
        std::vector<std::pair<uint32_t, uint16_t>> inserted_chunks;
        inserted_chunks.reserve(chunks_needed);

        for (uint32_t seq = 0; seq < chunks_needed; seq++)
        {
            uint32_t chunk_size = std::min(TOAST_MAX_CHUNK_SIZE, size - offset);

            // Build tuple data manually
            // Format: chunk_id (4 bytes) | chunk_seq (4 bytes) | chunk_size (4 bytes) | data
            std::vector<uint8_t> tuple_data;
            tuple_data.reserve(12 + chunk_size);

            // Add chunk_id
            uint32_t id = value_id;
            tuple_data.insert(tuple_data.end(), reinterpret_cast<uint8_t *>(&id),
                              reinterpret_cast<uint8_t *>(&id) + 4);

            // Add chunk_seq
            tuple_data.insert(tuple_data.end(), reinterpret_cast<uint8_t *>(&seq),
                              reinterpret_cast<uint8_t *>(&seq) + 4);

            // Add chunk_size
            tuple_data.insert(tuple_data.end(), reinterpret_cast<uint8_t *>(&chunk_size),
                              reinterpret_cast<uint8_t *>(&chunk_size) + 4);

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
                    storage->deleteTuple(toast_table_id_, chunk.first, chunk.second, ctx);
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
        CatalogManager *catalog = db_->catalog_manager();

        // Get the index ID for the TOAST table
        std::string toast_name = "pg_toast_" + table_id_.toString();
        std::string index_name = toast_name + "_idx";
        CatalogManager::IndexInfo index_info;
        Status status = catalog->getIndex(toast_table_id_, index_name, index_info, ctx);
        if (status != Status::OK)
        {
            // Fall back to heap scan if index not found
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
            SET_ERROR_CONTEXT(ctx, status, "TOAST value not found");
            return status;
        }

        // Collect chunks in order
        struct ChunkData
        {
            uint32_t seq;
            std::vector<uint8_t> data;
        };
        std::vector<ChunkData> chunks;

        Tuple tuple;
        while ((status = scan->next(&tuple, ctx)) == Status::OK)
        {
            // Parse tuple format: chunk_id | chunk_seq | chunk_size | data
            if ((tuple.data == nullptr) || tuple.data_size < 12)
            {
                continue;
            }

            const uint8_t *ptr = tuple.data;
            uint32_t chunk_id = *reinterpret_cast<const uint32_t *>(ptr);

            if (chunk_id != value_id)
            {
                // We've moved past our value_id in the index
                break;
            }

            ptr += 4;
            uint32_t chunk_seq = *reinterpret_cast<const uint32_t *>(ptr);
            ptr += 4;
            uint32_t chunk_size = *reinterpret_cast<const uint32_t *>(ptr);
            ptr += 4;

            // Validate chunk size
            if (chunk_size > TOAST_MAX_CHUNK_SIZE || 12 + chunk_size > tuple.data_size)
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
            // Parse tuple format: chunk_id | chunk_seq | chunk_size | data
            if ((tuple.data == nullptr) || tuple.data_size < 12)
            {
                continue;
            }

            const uint8_t *ptr = tuple.data;
            uint32_t chunk_id = *reinterpret_cast<const uint32_t *>(ptr);
            ptr += 4;

            if (chunk_id != value_id)
            {
                continue;
            }

            uint32_t chunk_seq = *reinterpret_cast<const uint32_t *>(ptr);
            ptr += 4;

            uint32_t chunk_size = *reinterpret_cast<const uint32_t *>(ptr);
            ptr += 4;

            // Validate chunk size
            if (chunk_size > TOAST_MAX_CHUNK_SIZE || 12 + chunk_size > tuple.data_size)
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

} // namespace scratchbird::core
// namespace scratchbirde scratchbird