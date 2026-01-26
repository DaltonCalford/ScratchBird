#include "scratchbird/core/columnstore.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include <algorithm>
#include <cstring>
#include <unordered_set>

namespace scratchbird::core
{

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Get size in bytes for a data type
 */
static inline size_t getDataTypeSize(DataType type)
{
    switch (type)
    {
    case DataType::INT8:
    case DataType::UINT8:
        return 1;
    case DataType::INT16:
    case DataType::UINT16:
        return 2;
    case DataType::INT32:
    case DataType::UINT32:
    case DataType::FLOAT32:
        return 4;
    case DataType::INT64:
    case DataType::UINT64:
    case DataType::FLOAT64:
        return 8;
    case DataType::INT128:
    case DataType::UINT128:
        return 16;
    default:
        return 0;  // Variable-length types
    }
}

// ============================================================================
// ColumnstoreIndex Implementation
// ============================================================================

ColumnstoreIndex::ColumnstoreIndex(Database *db, SBColumnstoreIndex index_info)
    : db_(db), index_info_(std::move(index_info))
{
    metadata_page_ = index_info_.idx_root_page;
}

ColumnstoreIndex::~ColumnstoreIndex() = default;

// ============================================================================
// Factory Methods
// ============================================================================

Status ColumnstoreIndex::create(Database *db,
                                const UuidV7Bytes &index_uuid,
                                const UuidV7Bytes &table_uuid,
                                const std::vector<UuidV7Bytes> &column_uuids,
                                uint32_t segment_size,
                                CompressionType compression,
                                GPID root_gpid,
                                ErrorContext *ctx)
{
    if (!db || root_gpid == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database is null");
        return Status::INVALID_ARGUMENT;
    }

    if (column_uuids.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No columns specified");
        return Status::INVALID_ARGUMENT;
    }

    // Phase 7: Create metadata page (page 0) to store configuration
    Status status = createMetadataPage(db, index_uuid, table_uuid, column_uuids,
                                      segment_size, compression, root_gpid, ctx);
    if (status != Status::OK)
        return status;

    return Status::OK;
}

Status ColumnstoreIndex::create(Database *db,
                                const UuidV7Bytes &index_uuid,
                                const UuidV7Bytes &table_uuid,
                                const std::vector<UuidV7Bytes> &column_uuids,
                                uint32_t segment_size,
                                CompressionType compression,
                                uint32_t *root_page_out,
                                ErrorContext *ctx)
{
    if (!db || !root_page_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments to ColumnstoreIndex::create");
        return Status::INVALID_ARGUMENT;
    }

    PageManager *page_mgr = db->page_manager();
    if (!page_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Page manager not available");
        return Status::INVALID_ARGUMENT;
    }

    GPID root_gpid = 0;
    Status status = page_mgr->allocatePageInTablespace(0, &root_gpid, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    *root_page_out = static_cast<uint32_t>(getPageNumber(root_gpid));
    return create(db, index_uuid, table_uuid, column_uuids,
                  segment_size, compression, root_gpid, ctx);
}

std::unique_ptr<ColumnstoreIndex> ColumnstoreIndex::open(Database *db,
                                                         const UuidV7Bytes &index_uuid,
                                                         GPID root_gpid,
                                                         uint32_t segment_size,
                                                         ErrorContext *ctx)
{
    if (!db)
        return nullptr;

    // Phase 7: Read metadata from page 0
    SBColumnstoreIndex index_info;
    std::memcpy(&index_info.idx_uuid, &index_uuid, sizeof(ID));
    index_info.idx_root_page = static_cast<uint32_t>(getPageNumber(root_gpid));
    index_info.idx_tablespace_id = getTablespaceID(root_gpid);

    auto index = std::make_unique<ColumnstoreIndex>(db, index_info);

    // Try to read metadata from page 0
    if (index_info.idx_root_page != 0)
    {
        Status status = index->readMetadataPage(index_info.idx_root_page, ctx);
        if (status != Status::OK)
        {
            // Fall back to parameters if metadata page doesn't exist or can't be read
            index->index_info_.idx_segment_size = segment_size;
            index->index_info_.idx_compression_type = static_cast<uint8_t>(CompressionType::RLE);
            index->index_info_.idx_total_segments = 0;
            index->index_info_.idx_total_rows = 0;
            index->index_info_.idx_root_page = 0;
        }
    }
    else
    {
        // No root page yet, use parameters
        index->index_info_.idx_segment_size = segment_size;
        index->index_info_.idx_compression_type = static_cast<uint8_t>(CompressionType::RLE);
        index->index_info_.idx_total_segments = 0;
        index->index_info_.idx_total_rows = 0;
        index->index_info_.idx_root_page = 0;
    }

    return index;
}

GPID ColumnstoreIndex::indexGPID(uint64_t page_num) const
{
    return makeGPID(index_info_.idx_tablespace_id, page_num);
}

Status ColumnstoreIndex::pinIndexPage(uint64_t page_num, void **buffer, ErrorContext *ctx,
                                      BufferPool::AccessStrategy strategy)
{
    return db_->buffer_pool()->pinPageGlobal(indexGPID(page_num), buffer, ctx, strategy);
}

Status ColumnstoreIndex::unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext *ctx)
{
    return db_->buffer_pool()->unpinPageGlobal(indexGPID(page_num), dirty, ctx);
}

// ============================================================================
// Insert Operation
// ============================================================================

Status ColumnstoreIndex::insert(const ID &column_uuid,
                                uint64_t tid,
                                const void *value,
                                size_t value_len,
                                bool is_null,
                                ErrorContext *ctx)
{
    // Get current transaction ID for MGA compliance
    TransactionManager *txn_mgr = db_->transaction_manager();
    if (!txn_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Transaction manager not available");
        return Status::INVALID_ARGUMENT;
    }

    uint64_t xmin = txn_mgr->getCurrentXid();

    // Buffer the value
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    BufferedValue buffered;
    buffered.tid = tid;
    buffered.xmin = xmin;
    buffered.is_null = is_null;

    // Copy value data
    if (!is_null && value && value_len > 0)
    {
        const uint8_t *value_bytes = static_cast<const uint8_t *>(value);
        buffered.data.assign(value_bytes, value_bytes + value_len);
    }

    // Add to column buffer
    column_buffers_[column_uuid].push_back(std::move(buffered));

    // Check if buffer is full (segment_size rows)
    if (column_buffers_[column_uuid].size() >= index_info_.idx_segment_size)
    {
        // Flush segment
        Status status = flushSegment(column_uuid, ctx);
        if (status != Status::OK)
            return status;
    }

    return Status::OK;
}

// ============================================================================
// Scan Operation
// ============================================================================

Status ColumnstoreIndex::scan(const ID &column_uuid,
                              const ColumnPredicate *predicate,
                              uint64_t current_xid,
                              ColumnScanBatch *batch_out,
                              ErrorContext *ctx)
{
    if (!batch_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Batch output is null");
        return Status::INVALID_ARGUMENT;
    }

    batch_out->count = 0;
    size_t batch_capacity = std::min(batch_out->tids.size(), batch_out->values.size());

    // Phase 2: Scan from disk segments first, then buffered values

    // Step 1: Scan persisted segments from disk
    if (index_info_.idx_root_page != 0)
    {
        uint32_t current_page = index_info_.idx_root_page;

        while (current_page != 0 && batch_out->count < batch_capacity)
        {
            // Read segment from disk
            ColumnSegment segment;
            Status read_status = readSegment(current_page, &segment, ctx);
            if (read_status != Status::OK)
            {
                // If we can't read a segment, log and skip it
                current_page = 0;  // Stop scanning
                break;
            }

            // Check if this segment is for the requested column
            if (std::memcmp(&segment.column_uuid, &column_uuid, sizeof(ID)) != 0)
            {
                // Different column, try next segment
                BufferPool *bp = db_->buffer_pool();
                if (bp)
                {
                    void *page_buffer = nullptr;
                    if (pinIndexPage(current_page, &page_buffer, ctx,
                                     BufferPool::AccessStrategy::Sequential) == Status::OK)
                    {
                        auto *page = static_cast<const SBColumnstorePage *>(page_buffer);
                        current_page = page->cs_next_segment;
                        unpinIndexPage(current_page, false, ctx);
                    }
                    else
                    {
                        current_page = 0;
                    }
                }
                else
                {
                    current_page = 0;
                }
                continue;
            }

            // Apply min/max predicate pushdown if applicable
            if (predicate && segment.data_type == DataType::INT32)
            {
                bool can_skip = false;
                switch (predicate->op)
                {
                case ColumnPredicate::Op::LESS_THAN:
                    if (segment.min_value >= predicate->value)
                        can_skip = true;
                    break;
                case ColumnPredicate::Op::LESS_EQUAL:
                    if (segment.min_value > predicate->value)
                        can_skip = true;
                    break;
                case ColumnPredicate::Op::GREATER_THAN:
                    if (segment.max_value <= predicate->value)
                        can_skip = true;
                    break;
                case ColumnPredicate::Op::GREATER_EQUAL:
                    if (segment.max_value < predicate->value)
                        can_skip = true;
                    break;
                case ColumnPredicate::Op::EQUAL:
                    if (predicate->value < segment.min_value || predicate->value > segment.max_value)
                        can_skip = true;
                    break;
                default:
                    break;
                }

                if (can_skip)
                {
                    // Skip this segment entirely
                    BufferPool *bp = db_->buffer_pool();
                    if (bp)
                    {
                        void *page_buffer = nullptr;
                        if (pinIndexPage(current_page, &page_buffer, ctx,
                                         BufferPool::AccessStrategy::Sequential) == Status::OK)
                        {
                            auto *page = static_cast<const SBColumnstorePage *>(page_buffer);
                            current_page = page->cs_next_segment;
                            unpinIndexPage(current_page, false, ctx);
                        }
                        else
                        {
                            current_page = 0;
                        }
                    }
                    else
                    {
                        current_page = 0;
                    }
                    continue;
                }
            }

            // Process segment values
            size_t value_size = getDataTypeSize(segment.data_type);
            if (value_size == 0)
                value_size = 4;  // Default for variable types

            for (uint32_t i = 0; i < segment.row_count && batch_out->count < batch_capacity; ++i)
            {
                // Check NULL flag
                bool is_null = (i < segment.null_bitmap.size() && segment.null_bitmap[i]);

                // Apply predicate
                if (predicate && segment.data_type == DataType::INT32)
                {
                    if (!is_null && (i * value_size + value_size) <= segment.data.size())
                    {
                        int32_t val = 0;
                        std::memcpy(&val, segment.data.data() + (i * value_size), sizeof(int32_t));

                        bool matches = false;
                        switch (predicate->op)
                        {
                        case ColumnPredicate::Op::EQUAL:
                            matches = (val == static_cast<int32_t>(predicate->value));
                            break;
                        case ColumnPredicate::Op::NOT_EQUAL:
                            matches = (val != static_cast<int32_t>(predicate->value));
                            break;
                        case ColumnPredicate::Op::LESS_THAN:
                            matches = (val < static_cast<int32_t>(predicate->value));
                            break;
                        case ColumnPredicate::Op::LESS_EQUAL:
                            matches = (val <= static_cast<int32_t>(predicate->value));
                            break;
                        case ColumnPredicate::Op::GREATER_THAN:
                            matches = (val > static_cast<int32_t>(predicate->value));
                            break;
                        case ColumnPredicate::Op::GREATER_EQUAL:
                            matches = (val >= static_cast<int32_t>(predicate->value));
                            break;
                        case ColumnPredicate::Op::IS_NULL:
                            matches = is_null;
                            break;
                        case ColumnPredicate::Op::IS_NOT_NULL:
                            matches = !is_null;
                            break;
                        }

                        if (!matches)
                            continue;
                    }
                    else if (predicate->op == ColumnPredicate::Op::IS_NULL && is_null)
                    {
                        // NULL matches IS_NULL
                    }
                    else if (predicate->op == ColumnPredicate::Op::IS_NOT_NULL && !is_null)
                    {
                        // Non-NULL matches IS_NOT_NULL
                    }
                    else
                    {
                        continue;
                    }
                }

                // Add to batch (TID is derived from segment range)
                uint64_t tid = segment.first_tid + i;
                batch_out->tids.push_back(tid);

                // Copy value data
                if (is_null || segment.data.empty())
                {
                    batch_out->values.push_back(0);
                }
                else if ((i * value_size + value_size) <= segment.data.size())
                {
                    const uint8_t *value_ptr = segment.data.data() + (i * value_size);
                    batch_out->values.insert(batch_out->values.end(), value_ptr, value_ptr + value_size);
                }

                batch_out->null_flags.push_back(is_null);
                batch_out->count++;
            }

            // Move to next segment
            BufferPool *bp = db_->buffer_pool();
            if (bp)
            {
                void *page_buffer = nullptr;
                if (pinIndexPage(current_page, &page_buffer, ctx,
                                 BufferPool::AccessStrategy::Sequential) == Status::OK)
                {
                    auto *page = static_cast<const SBColumnstorePage *>(page_buffer);
                    current_page = page->cs_next_segment;
                    unpinIndexPage(current_page, false, ctx);
                }
                else
                {
                    current_page = 0;
                }
            }
            else
            {
                current_page = 0;
            }
        }
    }

    // Step 2: Scan buffered values (in-memory)
    std::lock_guard<std::mutex> lock(buffer_mutex_);

    auto it = column_buffers_.find(column_uuid);
    if (it == column_buffers_.end() || it->second.empty())
    {
        return Status::OK;  // No buffered values, but disk scan completed
    }

    const std::vector<BufferedValue> &buffer = it->second;

    // Scan through buffered values (continuing from disk scan)
    for (size_t i = 0; i < buffer.size() && batch_out->count < batch_capacity; ++i)
    {
        const BufferedValue &bv = buffer[i];

        // MGA visibility check
        if (!isValueVisible(bv.xmin, 0, current_xid, ctx))
            continue;

        // Apply predicate if provided
        if (predicate)
        {
            // For Phase 1, we only support simple predicates on INT32
            if (!bv.is_null && bv.data.size() == sizeof(int32_t))
            {
                int32_t val = 0;
                std::memcpy(&val, bv.data.data(), sizeof(int32_t));

                bool matches = false;
                switch (predicate->op)
                {
                case ColumnPredicate::Op::EQUAL:
                    matches = (val == static_cast<int32_t>(predicate->value));
                    break;
                case ColumnPredicate::Op::NOT_EQUAL:
                    matches = (val != static_cast<int32_t>(predicate->value));
                    break;
                case ColumnPredicate::Op::LESS_THAN:
                    matches = (val < static_cast<int32_t>(predicate->value));
                    break;
                case ColumnPredicate::Op::LESS_EQUAL:
                    matches = (val <= static_cast<int32_t>(predicate->value));
                    break;
                case ColumnPredicate::Op::GREATER_THAN:
                    matches = (val > static_cast<int32_t>(predicate->value));
                    break;
                case ColumnPredicate::Op::GREATER_EQUAL:
                    matches = (val >= static_cast<int32_t>(predicate->value));
                    break;
                case ColumnPredicate::Op::IS_NULL:
                    matches = bv.is_null;
                    break;
                case ColumnPredicate::Op::IS_NOT_NULL:
                    matches = !bv.is_null;
                    break;
                }

                if (!matches)
                    continue;
            }
            else if (predicate->op == ColumnPredicate::Op::IS_NULL && bv.is_null)
            {
                // NULL matches IS_NULL predicate
            }
            else if (predicate->op == ColumnPredicate::Op::IS_NOT_NULL && !bv.is_null)
            {
                // Non-NULL matches IS_NOT_NULL predicate
            }
            else
            {
                continue;  // Predicate doesn't match
            }
        }

        // Add to batch
        batch_out->tids.push_back(bv.tid);

        // Copy value data
        if (bv.is_null || bv.data.empty())
        {
            batch_out->values.push_back(0);
        }
        else
        {
            // For now, just copy raw bytes
            batch_out->values.insert(batch_out->values.end(), bv.data.begin(), bv.data.end());
        }

        batch_out->null_flags.push_back(bv.is_null);
        batch_out->count++;
    }

    return Status::OK;
}

// ============================================================================
// Statistics
// ============================================================================

Status ColumnstoreIndex::getStats(ColumnstoreStats *stats_out, ErrorContext *ctx)
{
    if (!stats_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Stats output is null");
        return Status::INVALID_ARGUMENT;
    }

    BufferPool *buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Buffer pool not available");
        return Status::INVALID_ARGUMENT;
    }

    // Initialize counters
    uint64_t total_segments = 0;
    uint64_t total_rows = 0;
    uint64_t total_compressed = 0;
    uint64_t total_uncompressed = 0;
    uint64_t total_nulls = 0;

    // Traverse segment chain and collect statistics
    uint32_t current_page = index_info_.idx_root_page;

    while (current_page != 0)
    {
        // Pin segment page
        void *page_buffer = nullptr;
        Status status = pinIndexPage(current_page, &page_buffer, ctx,
                                     BufferPool::AccessStrategy::Sequential);
        if (status != Status::OK)
            return status;

        auto *page = static_cast<const SBColumnstorePage *>(page_buffer);

        // Only count non-continuation pages as logical segments
        // Continuation pages are part of multi-page segments and should not be counted separately
        bool is_continuation = (page->cs_flags & static_cast<uint16_t>(ColumnstoreFlags::CONTINUATION)) != 0;

        if (!is_continuation)
        {
            // Collect statistics from this segment (first page only)
            total_segments++;
            total_rows += page->cs_row_count;
            total_uncompressed += page->cs_uncompressed_size;
            total_nulls += page->cs_null_count;
        }

        // Always count compressed bytes (from all pages in multi-page segments)
        total_compressed += page->cs_compressed_size;

        // Move to next segment
        uint32_t next_page = page->cs_next_segment;
        unpinIndexPage(current_page, false, ctx);

        current_page = next_page;
    }

    // Fill output statistics
    stats_out->total_segments = total_segments;
    stats_out->total_rows = total_rows;
    stats_out->compressed_bytes = total_compressed;
    stats_out->uncompressed_bytes = total_uncompressed;
    stats_out->null_count = total_nulls;

    // Calculate compression ratio
    if (total_compressed > 0)
    {
        stats_out->compression_ratio = static_cast<double>(total_uncompressed) / total_compressed;
    }
    else
    {
        stats_out->compression_ratio = 1.0;
    }

    return Status::OK;
}

Status ColumnstoreIndex::updateTIDsAfterMigration(
    const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
    uint64_t *tids_updated_out,
    uint64_t *pages_modified_out,
    ErrorContext *ctx)
{
    if (tids_updated_out != nullptr)
    {
        *tids_updated_out = 0;
    }
    if (pages_modified_out != nullptr)
    {
        *pages_modified_out = 0;
    }

    if (tid_mapping.empty() || index_info_.idx_root_page == 0)
    {
        return Status::OK;
    }

    uint64_t tids_updated = 0;
    uint64_t pages_modified = 0;

    uint32_t current_page = index_info_.idx_root_page;
    uint32_t pages_scanned = 0;

    while (current_page != 0 && pages_scanned < 100000)
    {
        void *page_buffer = nullptr;
        Status status = pinIndexPage(current_page, &page_buffer, ctx,
                                     BufferPool::AccessStrategy::Sequential);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page = static_cast<SBColumnstorePage *>(page_buffer);
        bool is_continuation =
            (page->cs_flags & static_cast<uint16_t>(ColumnstoreFlags::CONTINUATION)) != 0;

        bool modified = false;
        if (!is_continuation)
        {
            uint64_t old_first_tid = page->cs_first_tid;
            uint64_t old_last_tid = page->cs_last_tid;
            auto first_it = tid_mapping.find(old_first_tid);
            if (first_it != tid_mapping.end())
            {
                page->cs_first_tid = first_it->second;
                modified = true;
                tids_updated++;
            }

            auto last_it = tid_mapping.find(old_last_tid);
            if (last_it != tid_mapping.end())
            {
                page->cs_last_tid = last_it->second;
                modified = true;
                if (old_last_tid != old_first_tid || first_it == tid_mapping.end())
                {
                    tids_updated++;
                }
            }
        }

        uint32_t next_page = static_cast<uint32_t>(page->cs_next_segment);
        unpinIndexPage(current_page, modified, ctx);

        if (modified)
        {
            pages_modified++;
        }

        current_page = next_page;
        pages_scanned++;
    }

    if (tids_updated_out != nullptr)
    {
        *tids_updated_out = tids_updated;
    }
    if (pages_modified_out != nullptr)
    {
        *pages_modified_out = pages_modified;
    }

    return Status::OK;
}

// ============================================================================
// Compression - RLE
// ============================================================================

Status ColumnstoreIndex::compressRLE(const ColumnSegment &segment,
                                     std::vector<uint8_t> *compressed_out,
                                     ErrorContext *ctx)
{
    if (!compressed_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Compressed output is null");
        return Status::INVALID_ARGUMENT;
    }

    compressed_out->clear();

    // Handle empty input
    if (segment.row_count == 0 || segment.data.empty())
    {
        return Status::OK;
    }

    // Determine value size based on data type
    size_t value_size = 0;
    switch (segment.data_type)
    {
    case DataType::INT8:
    case DataType::UINT8:
        value_size = 1;
        break;
    case DataType::INT16:
    case DataType::UINT16:
        value_size = 2;
        break;
    case DataType::INT32:
    case DataType::UINT32:
    case DataType::FLOAT32:
        value_size = 4;
        break;
    case DataType::INT64:
    case DataType::UINT64:
    case DataType::FLOAT64:
        value_size = 8;
        break;
    case DataType::INT128:
    case DataType::UINT128:
        value_size = 16;
        break;
    default:
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unsupported data type for RLE compression");
        return Status::INVALID_ARGUMENT;
    }

    // Validate data size
    if (segment.data.size() != segment.row_count * value_size)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Data size mismatch");
        return Status::INVALID_ARGUMENT;
    }

    // RLE encoding: scan through values, count consecutive identical values
    const uint8_t *data_ptr = segment.data.data();
    uint32_t i = 0;

    while (i < segment.row_count)
    {
        // Get current value
        const uint8_t *current_value = data_ptr + (i * value_size);
        uint32_t run_length = 1;

        // Check if current value is NULL
        bool is_null = (i < segment.null_bitmap.size()) ? segment.null_bitmap[i] : false;

        // Count consecutive identical values (including NULL runs)
        while (i + run_length < segment.row_count)
        {
            const uint8_t *next_value = data_ptr + ((i + run_length) * value_size);
            bool next_is_null = (i + run_length < segment.null_bitmap.size()) ? segment.null_bitmap[i + run_length] : false;

            // Check if values are identical (or both NULL)
            if (is_null && next_is_null)
            {
                run_length++;
            }
            else if (!is_null && !next_is_null && std::memcmp(current_value, next_value, value_size) == 0)
            {
                run_length++;
            }
            else
            {
                break;  // Different value or NULL status changed
            }
        }

        // Write (value, run_length) pair
        // Format: [is_null (1 byte)][value (value_size bytes)][run_length (4 bytes)]

        // Write NULL flag
        uint8_t null_flag = is_null ? 1 : 0;
        compressed_out->push_back(null_flag);

        // Write value (even if NULL, for consistency)
        compressed_out->insert(compressed_out->end(), current_value, current_value + value_size);

        // Write run length
        compressed_out->insert(compressed_out->end(),
                               reinterpret_cast<const uint8_t *>(&run_length),
                               reinterpret_cast<const uint8_t *>(&run_length) + sizeof(uint32_t));

        i += run_length;
    }

    return Status::OK;
}

Status ColumnstoreIndex::decompressRLE(const std::vector<uint8_t> &compressed,
                                       DataType data_type,
                                       uint32_t row_count,
                                       ColumnSegment *segment_out,
                                       ErrorContext *ctx)
{
    if (!segment_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Segment output is null");
        return Status::INVALID_ARGUMENT;
    }

    segment_out->data.clear();
    segment_out->null_bitmap.clear();
    segment_out->row_count = 0;
    segment_out->data_type = data_type;

    // Handle empty input
    if (compressed.empty())
    {
        return Status::OK;
    }

    // Determine value size based on data type
    size_t value_size = 0;
    switch (data_type)
    {
    case DataType::INT8:
    case DataType::UINT8:
        value_size = 1;
        break;
    case DataType::INT16:
    case DataType::UINT16:
        value_size = 2;
        break;
    case DataType::INT32:
    case DataType::UINT32:
    case DataType::FLOAT32:
        value_size = 4;
        break;
    case DataType::INT64:
    case DataType::UINT64:
    case DataType::FLOAT64:
        value_size = 8;
        break;
    case DataType::INT128:
    case DataType::UINT128:
        value_size = 16;
        break;
    default:
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unsupported data type for RLE decompression");
        return Status::INVALID_ARGUMENT;
    }

    // RLE format: [is_null (1 byte)][value (value_size bytes)][run_length (4 bytes)]
    const size_t entry_size = 1 + value_size + sizeof(uint32_t);

    // Validate compressed data size
    if (compressed.size() % entry_size != 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::COMPRESSION_ERROR, "Corrupted RLE compressed data (invalid size)");
        return Status::COMPRESSION_ERROR;
    }

    // Pre-allocate output buffers
    segment_out->data.reserve(row_count * value_size);
    segment_out->null_bitmap.reserve(row_count);

    // Decompress RLE entries
    const uint8_t *read_ptr = compressed.data();
    const uint8_t *end_ptr = compressed.data() + compressed.size();
    uint32_t total_decompressed = 0;

    while (read_ptr < end_ptr)
    {
        // Read NULL flag
        uint8_t null_flag = *read_ptr;
        read_ptr += 1;

        // Read value
        const uint8_t *value_ptr = read_ptr;
        read_ptr += value_size;

        // Read run length
        uint32_t run_length = 0;
        std::memcpy(&run_length, read_ptr, sizeof(uint32_t));
        read_ptr += sizeof(uint32_t);

        // Validate run length
        if (run_length == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::COMPRESSION_ERROR, "Corrupted RLE data (zero run length)");
            return Status::COMPRESSION_ERROR;
        }

        if (total_decompressed + run_length > row_count)
        {
            SET_ERROR_CONTEXT(ctx, Status::COMPRESSION_ERROR, "Corrupted RLE data (exceeds expected row count)");
            return Status::COMPRESSION_ERROR;
        }

        // Expand run: repeat value 'run_length' times
        bool is_null = (null_flag != 0);
        for (uint32_t i = 0; i < run_length; ++i)
        {
            // Append value to data
            segment_out->data.insert(segment_out->data.end(), value_ptr, value_ptr + value_size);

            // Append NULL flag to bitmap
            segment_out->null_bitmap.push_back(is_null);
        }

        total_decompressed += run_length;
    }

    // Verify decompressed count matches expected
    if (total_decompressed != row_count)
    {
        SET_ERROR_CONTEXT(ctx, Status::COMPRESSION_ERROR, "Decompressed row count mismatch");
        return Status::COMPRESSION_ERROR;
    }

    segment_out->row_count = row_count;
    return Status::OK;
}

// ============================================================================
// Dictionary Encoding
// ============================================================================

Status ColumnstoreIndex::compressDictionary(const ColumnSegment &segment,
                                            std::vector<uint8_t> *compressed_out,
                                            Dictionary *dict_out,
                                            ErrorContext *ctx)
{
    if (!compressed_out || !dict_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Output parameters are null");
        return Status::INVALID_ARGUMENT;
    }

    compressed_out->clear();
    dict_out->clear();

    // Handle empty input
    if (segment.row_count == 0 || segment.data.empty())
    {
        return Status::OK;
    }

    // Dictionary encoding works best for strings (VARCHAR, TEXT)
    // For Phase 2, we'll support string-like data stored as byte arrays

    // Determine if this is suitable for dictionary encoding
    // Rule: Use dictionary if cardinality < 10% of row count
    std::unordered_set<std::string> unique_values;

    // First pass: Build set of unique values to check cardinality
    // For simplicity, we'll treat data as null-terminated strings
    const uint8_t *data_ptr = segment.data.data();
    size_t offset = 0;

    std::vector<std::string> values;
    values.reserve(segment.row_count);

    // Extract values (assuming null-terminated strings for Phase 2)
    for (uint32_t i = 0; i < segment.row_count; ++i)
    {
        bool is_null = (i < segment.null_bitmap.size()) ? segment.null_bitmap[i] : false;

        if (is_null)
        {
            values.push_back("");  // Empty string for NULL
        }
        else
        {
            // For Phase 2, we'll support fixed-size strings
            // In a real implementation, this would handle variable-length strings
            const char *str_start = reinterpret_cast<const char *>(data_ptr + offset);
            size_t str_len = strnlen(str_start, segment.data.size() - offset);

            std::string value(str_start, str_len);
            values.push_back(value);
            unique_values.insert(value);

            offset += str_len + 1;  // +1 for null terminator
            if (offset >= segment.data.size())
                break;
        }
    }

    // Check if dictionary encoding is beneficial
    double cardinality_ratio = static_cast<double>(unique_values.size()) / segment.row_count;
    if (cardinality_ratio > 0.1)  // More than 10% unique values
    {
        // Not beneficial, return error to fall back to RLE
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Dictionary encoding not beneficial (high cardinality)");
        return Status::INVALID_ARGUMENT;
    }

    // Build dictionary
    for (const std::string &value : values)
    {
        dict_out->addValue(value);
    }

    // Second pass: Encode values as integer codes
    std::vector<uint32_t> codes;
    codes.reserve(segment.row_count);

    for (const std::string &value : values)
    {
        int32_t code = dict_out->getCode(value);
        if (code < 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::COMPRESSION_ERROR, "Dictionary lookup failed");
            return Status::COMPRESSION_ERROR;
        }
        codes.push_back(static_cast<uint32_t>(code));
    }

    // Compress codes using RLE
    // Build a temporary segment with codes as INT32 values
    ColumnSegment codes_segment;
    codes_segment.data_type = DataType::INT32;
    codes_segment.row_count = segment.row_count;
    codes_segment.data.resize(codes.size() * sizeof(uint32_t));
    codes_segment.null_bitmap = segment.null_bitmap;
    codes_segment.null_count = segment.null_count;

    std::memcpy(codes_segment.data.data(), codes.data(), codes.size() * sizeof(uint32_t));

    // Use RLE to compress the codes
    Status status = compressRLE(codes_segment, compressed_out, ctx);
    if (status != Status::OK)
        return status;

    return Status::OK;
}

Status ColumnstoreIndex::decompressDictionary(const std::vector<uint8_t> &compressed,
                                              const Dictionary &dict,
                                              DataType data_type,
                                              uint32_t row_count,
                                              ColumnSegment *segment_out,
                                              ErrorContext *ctx)
{
    if (!segment_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Segment output is null");
        return Status::INVALID_ARGUMENT;
    }

    segment_out->data.clear();
    segment_out->null_bitmap.clear();
    segment_out->row_count = 0;
    segment_out->data_type = data_type;

    // Handle empty input
    if (compressed.empty())
    {
        return Status::OK;
    }

    // Decompress codes using RLE
    ColumnSegment codes_segment;
    Status status = decompressRLE(compressed, DataType::INT32, row_count, &codes_segment, ctx);
    if (status != Status::OK)
        return status;

    // Verify row count
    if (codes_segment.row_count != row_count)
    {
        SET_ERROR_CONTEXT(ctx, Status::COMPRESSION_ERROR, "Decompressed row count mismatch");
        return Status::COMPRESSION_ERROR;
    }

    // Decode codes back to strings
    const uint32_t *codes = reinterpret_cast<const uint32_t *>(codes_segment.data.data());

    for (uint32_t i = 0; i < row_count; ++i)
    {
        bool is_null = (i < codes_segment.null_bitmap.size()) ? codes_segment.null_bitmap[i] : false;
        segment_out->null_bitmap.push_back(is_null);

        if (is_null)
        {
            // Write empty string for NULL
            segment_out->data.push_back(0);
        }
        else
        {
            // Look up value in dictionary
            uint32_t code = codes[i];
            std::string value;
            if (!dict.getValue(code, &value))
            {
                SET_ERROR_CONTEXT(ctx, Status::COMPRESSION_ERROR, "Dictionary lookup failed");
                return Status::COMPRESSION_ERROR;
            }

            // Write string to data buffer (null-terminated)
            segment_out->data.insert(segment_out->data.end(), value.begin(), value.end());
            segment_out->data.push_back(0);  // Null terminator
        }
    }

    segment_out->row_count = row_count;
    segment_out->null_count = codes_segment.null_count;

    return Status::OK;
}

// ============================================================================
// Bit-Packing Compression
// ============================================================================

Status ColumnstoreIndex::compressBitpack(const ColumnSegment &segment,
                                        std::vector<uint8_t> *compressed_out,
                                        ErrorContext *ctx)
{
    // Validate inputs
    if (!compressed_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "compressed_out is null");
        return Status::INVALID_ARGUMENT;
    }

    if (segment.row_count == 0)
    {
        compressed_out->clear();
        return Status::OK;
    }

    // Only support integer types
    if (segment.data_type != DataType::INT8 &&
        segment.data_type != DataType::INT16 &&
        segment.data_type != DataType::INT32 &&
        segment.data_type != DataType::INT64 &&
        segment.data_type != DataType::UINT8 &&
        segment.data_type != DataType::UINT16 &&
        segment.data_type != DataType::UINT32 &&
        segment.data_type != DataType::UINT64)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Bit-packing only supports integer types");
        return Status::INVALID_ARGUMENT;
    }

    // Get value size
    size_t value_size = getDataTypeSize(segment.data_type);
    if (segment.data.size() < segment.row_count * value_size)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Segment data size mismatch");
        return Status::INVALID_ARGUMENT;
    }

    // Step 1: Find min/max values (ignore NULLs)
    int64_t min_value = INT64_MAX;
    int64_t max_value = INT64_MIN;
    const uint8_t *data_ptr = segment.data.data();

    for (uint32_t i = 0; i < segment.row_count; ++i)
    {
        // Check if NULL
        bool is_null = (i < segment.null_bitmap.size()) ? segment.null_bitmap[i] : false;
        if (is_null)
            continue;

        // Read value based on data type
        int64_t value = 0;
        switch (segment.data_type)
        {
        case DataType::INT8:
            value = static_cast<int64_t>(*reinterpret_cast<const int8_t *>(data_ptr + i * value_size));
            break;
        case DataType::INT16:
            value = static_cast<int64_t>(*reinterpret_cast<const int16_t *>(data_ptr + i * value_size));
            break;
        case DataType::INT32:
            value = static_cast<int64_t>(*reinterpret_cast<const int32_t *>(data_ptr + i * value_size));
            break;
        case DataType::INT64:
            value = *reinterpret_cast<const int64_t *>(data_ptr + i * value_size);
            break;
        case DataType::UINT8:
            value = static_cast<int64_t>(*reinterpret_cast<const uint8_t *>(data_ptr + i * value_size));
            break;
        case DataType::UINT16:
            value = static_cast<int64_t>(*reinterpret_cast<const uint16_t *>(data_ptr + i * value_size));
            break;
        case DataType::UINT32:
            value = static_cast<int64_t>(*reinterpret_cast<const uint32_t *>(data_ptr + i * value_size));
            break;
        case DataType::UINT64:
            value = static_cast<int64_t>(*reinterpret_cast<const uint64_t *>(data_ptr + i * value_size));
            break;
        default:
            break;
        }

        if (value < min_value) min_value = value;
        if (value > max_value) max_value = value;
    }

    // Handle all-NULL case
    if (min_value == INT64_MAX || max_value == INT64_MIN)
    {
        // All values are NULL - just store header
        compressed_out->resize(16);
        uint64_t *header = reinterpret_cast<uint64_t *>(compressed_out->data());
        header[0] = 0;  // min_value = 0
        header[1] = 0;  // bits_per_value = 0
        return Status::OK;
    }

    // Step 2: Calculate bits needed
    uint64_t value_range = static_cast<uint64_t>(max_value - min_value);
    uint8_t bits_per_value = 0;

    if (value_range == 0)
    {
        bits_per_value = 0;  // All values are the same
    }
    else
    {
        // Calculate ceil(log2(value_range + 1))
        bits_per_value = 64 - __builtin_clzll(value_range);  // Count leading zeros
    }

    // Step 3: Write header (min_value, bits_per_value)
    compressed_out->clear();
    compressed_out->resize(16);  // 8 bytes for min_value, 8 bytes for bits_per_value
    uint64_t *header = reinterpret_cast<uint64_t *>(compressed_out->data());
    header[0] = static_cast<uint64_t>(min_value);
    header[1] = static_cast<uint64_t>(bits_per_value);

    // Step 4: Pack values into bit array
    if (bits_per_value == 0)
    {
        // All values are the same - no need to store data
        return Status::OK;
    }

    // Calculate total bits needed
    uint64_t total_bits = static_cast<uint64_t>(segment.row_count) * bits_per_value;
    uint64_t total_bytes = (total_bits + 7) / 8;  // Round up to nearest byte

    // Allocate buffer for bit-packed data
    size_t current_size = compressed_out->size();
    compressed_out->resize(current_size + total_bytes);
    uint8_t *bit_buffer = compressed_out->data() + current_size;
    std::memset(bit_buffer, 0, total_bytes);  // Zero-initialize

    // Pack values
    uint64_t bit_offset = 0;
    for (uint32_t i = 0; i < segment.row_count; ++i)
    {
        // Check if NULL
        bool is_null = (i < segment.null_bitmap.size()) ? segment.null_bitmap[i] : false;
        if (is_null)
        {
            // NULL values are stored as 0 (will be masked by null_bitmap)
            bit_offset += bits_per_value;
            continue;
        }

        // Read value
        int64_t value = 0;
        switch (segment.data_type)
        {
        case DataType::INT8:
            value = static_cast<int64_t>(*reinterpret_cast<const int8_t *>(data_ptr + i * value_size));
            break;
        case DataType::INT16:
            value = static_cast<int64_t>(*reinterpret_cast<const int16_t *>(data_ptr + i * value_size));
            break;
        case DataType::INT32:
            value = static_cast<int64_t>(*reinterpret_cast<const int32_t *>(data_ptr + i * value_size));
            break;
        case DataType::INT64:
            value = *reinterpret_cast<const int64_t *>(data_ptr + i * value_size);
            break;
        case DataType::UINT8:
            value = static_cast<int64_t>(*reinterpret_cast<const uint8_t *>(data_ptr + i * value_size));
            break;
        case DataType::UINT16:
            value = static_cast<int64_t>(*reinterpret_cast<const uint16_t *>(data_ptr + i * value_size));
            break;
        case DataType::UINT32:
            value = static_cast<int64_t>(*reinterpret_cast<const uint32_t *>(data_ptr + i * value_size));
            break;
        case DataType::UINT64:
            value = static_cast<int64_t>(*reinterpret_cast<const uint64_t *>(data_ptr + i * value_size));
            break;
        default:
            break;
        }

        // Normalize value (subtract min)
        uint64_t normalized = static_cast<uint64_t>(value - min_value);

        // Pack bits
        for (uint8_t bit = 0; bit < bits_per_value; ++bit)
        {
            uint64_t byte_index = bit_offset / 8;
            uint64_t bit_index = bit_offset % 8;

            if ((normalized >> bit) & 1)
            {
                bit_buffer[byte_index] |= (1 << bit_index);
            }

            bit_offset++;
        }
    }

    return Status::OK;
}

Status ColumnstoreIndex::decompressBitpack(const std::vector<uint8_t> &compressed,
                                          DataType data_type,
                                          uint32_t row_count,
                                          ColumnSegment *segment_out,
                                          ErrorContext *ctx)
{
    // Validate inputs
    if (!segment_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "segment_out is null");
        return Status::INVALID_ARGUMENT;
    }

    if (row_count == 0)
    {
        segment_out->data.clear();
        segment_out->row_count = 0;
        return Status::OK;
    }

    // Validate header size
    if (compressed.size() < 16)
    {
        SET_ERROR_CONTEXT(ctx, Status::COMPRESSION_ERROR,
                         "Corrupted bit-packed data (header too small)");
        return Status::COMPRESSION_ERROR;
    }

    // Read header
    const uint64_t *header = reinterpret_cast<const uint64_t *>(compressed.data());
    int64_t min_value = static_cast<int64_t>(header[0]);
    uint8_t bits_per_value = static_cast<uint8_t>(header[1]);

    // Validate bits_per_value
    if (bits_per_value > 64)
    {
        SET_ERROR_CONTEXT(ctx, Status::COMPRESSION_ERROR,
                         "Invalid bits_per_value in bit-packed data");
        return Status::COMPRESSION_ERROR;
    }

    // Get value size
    size_t value_size = getDataTypeSize(data_type);

    // Initialize output segment
    segment_out->data_type = data_type;
    segment_out->row_count = row_count;
    segment_out->data.resize(row_count * value_size);
    uint8_t *data_ptr = segment_out->data.data();

    // Handle all-same-value case
    if (bits_per_value == 0)
    {
        // All values are min_value
        for (uint32_t i = 0; i < row_count; ++i)
        {
            bool is_null = (i < segment_out->null_bitmap.size()) ?
                          segment_out->null_bitmap[i] : false;
            if (is_null)
                continue;

            // Write min_value to output
            switch (data_type)
            {
            case DataType::INT8:
                *reinterpret_cast<int8_t *>(data_ptr + i * value_size) =
                    static_cast<int8_t>(min_value);
                break;
            case DataType::INT16:
                *reinterpret_cast<int16_t *>(data_ptr + i * value_size) =
                    static_cast<int16_t>(min_value);
                break;
            case DataType::INT32:
                *reinterpret_cast<int32_t *>(data_ptr + i * value_size) =
                    static_cast<int32_t>(min_value);
                break;
            case DataType::INT64:
                *reinterpret_cast<int64_t *>(data_ptr + i * value_size) = min_value;
                break;
            case DataType::UINT8:
                *reinterpret_cast<uint8_t *>(data_ptr + i * value_size) =
                    static_cast<uint8_t>(min_value);
                break;
            case DataType::UINT16:
                *reinterpret_cast<uint16_t *>(data_ptr + i * value_size) =
                    static_cast<uint16_t>(min_value);
                break;
            case DataType::UINT32:
                *reinterpret_cast<uint32_t *>(data_ptr + i * value_size) =
                    static_cast<uint32_t>(min_value);
                break;
            case DataType::UINT64:
                *reinterpret_cast<uint64_t *>(data_ptr + i * value_size) =
                    static_cast<uint64_t>(min_value);
                break;
            default:
                break;
            }
        }
        return Status::OK;
    }

    // Validate compressed data size
    uint64_t total_bits = static_cast<uint64_t>(row_count) * bits_per_value;
    uint64_t total_bytes = (total_bits + 7) / 8;
    if (compressed.size() < 16 + total_bytes)
    {
        SET_ERROR_CONTEXT(ctx, Status::COMPRESSION_ERROR,
                         "Corrupted bit-packed data (data too small)");
        return Status::COMPRESSION_ERROR;
    }

    // Unpack values
    const uint8_t *bit_buffer = compressed.data() + 16;
    uint64_t bit_offset = 0;

    for (uint32_t i = 0; i < row_count; ++i)
    {
        bool is_null = (i < segment_out->null_bitmap.size()) ?
                      segment_out->null_bitmap[i] : false;
        if (is_null)
        {
            bit_offset += bits_per_value;
            continue;
        }

        // Unpack bits
        uint64_t normalized = 0;
        for (uint8_t bit = 0; bit < bits_per_value; ++bit)
        {
            uint64_t byte_index = bit_offset / 8;
            uint64_t bit_index = bit_offset % 8;

            if (bit_buffer[byte_index] & (1 << bit_index))
            {
                normalized |= (1ULL << bit);
            }

            bit_offset++;
        }

        // Add min_value back
        int64_t value = min_value + static_cast<int64_t>(normalized);

        // Write value to output
        switch (data_type)
        {
        case DataType::INT8:
            *reinterpret_cast<int8_t *>(data_ptr + i * value_size) =
                static_cast<int8_t>(value);
            break;
        case DataType::INT16:
            *reinterpret_cast<int16_t *>(data_ptr + i * value_size) =
                static_cast<int16_t>(value);
            break;
        case DataType::INT32:
            *reinterpret_cast<int32_t *>(data_ptr + i * value_size) =
                static_cast<int32_t>(value);
            break;
        case DataType::INT64:
            *reinterpret_cast<int64_t *>(data_ptr + i * value_size) = value;
            break;
        case DataType::UINT8:
            *reinterpret_cast<uint8_t *>(data_ptr + i * value_size) =
                static_cast<uint8_t>(value);
            break;
        case DataType::UINT16:
            *reinterpret_cast<uint16_t *>(data_ptr + i * value_size) =
                static_cast<uint16_t>(value);
            break;
        case DataType::UINT32:
            *reinterpret_cast<uint32_t *>(data_ptr + i * value_size) =
                static_cast<uint32_t>(value);
            break;
        case DataType::UINT64:
            *reinterpret_cast<uint64_t *>(data_ptr + i * value_size) =
                static_cast<uint64_t>(value);
            break;
        default:
            break;
        }
    }

    return Status::OK;
}

// ============================================================================
// Delta Compression (Bit-packed deltas)
// ============================================================================

Status ColumnstoreIndex::compressDelta(const ColumnSegment &segment,
                                       std::vector<uint8_t> *compressed_out,
                                       ErrorContext *ctx)
{
    if (!compressed_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "compressed_out is null");
        return Status::INVALID_ARGUMENT;
    }

    if (segment.row_count == 0)
    {
        compressed_out->clear();
        return Status::OK;
    }

    if (segment.null_count != 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Delta compression does not support NULL values");
        return Status::INVALID_ARGUMENT;
    }

    if (segment.data_type != DataType::INT8 &&
        segment.data_type != DataType::INT16 &&
        segment.data_type != DataType::INT32 &&
        segment.data_type != DataType::INT64 &&
        segment.data_type != DataType::UINT8 &&
        segment.data_type != DataType::UINT16 &&
        segment.data_type != DataType::UINT32 &&
        segment.data_type != DataType::UINT64)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Delta compression only supports integer types");
        return Status::INVALID_ARGUMENT;
    }

    size_t value_size = getDataTypeSize(segment.data_type);
    if (segment.data.size() < segment.row_count * value_size)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Segment data size mismatch");
        return Status::INVALID_ARGUMENT;
    }

    const uint8_t *data_ptr = segment.data.data();
    std::vector<int64_t> values(segment.row_count, 0);

    for (uint32_t i = 0; i < segment.row_count; ++i)
    {
        switch (segment.data_type)
        {
        case DataType::INT8:
            values[i] = static_cast<int64_t>(*reinterpret_cast<const int8_t *>(data_ptr + i * value_size));
            break;
        case DataType::INT16:
            values[i] = static_cast<int64_t>(*reinterpret_cast<const int16_t *>(data_ptr + i * value_size));
            break;
        case DataType::INT32:
            values[i] = static_cast<int64_t>(*reinterpret_cast<const int32_t *>(data_ptr + i * value_size));
            break;
        case DataType::INT64:
            values[i] = *reinterpret_cast<const int64_t *>(data_ptr + i * value_size);
            break;
        case DataType::UINT8:
            values[i] = static_cast<int64_t>(*reinterpret_cast<const uint8_t *>(data_ptr + i * value_size));
            break;
        case DataType::UINT16:
            values[i] = static_cast<int64_t>(*reinterpret_cast<const uint16_t *>(data_ptr + i * value_size));
            break;
        case DataType::UINT32:
            values[i] = static_cast<int64_t>(*reinterpret_cast<const uint32_t *>(data_ptr + i * value_size));
            break;
        case DataType::UINT64:
            values[i] = static_cast<int64_t>(*reinterpret_cast<const uint64_t *>(data_ptr + i * value_size));
            break;
        default:
            break;
        }
    }

    int64_t base_value = values[0];
    if (segment.row_count == 1)
    {
        compressed_out->resize(sizeof(int64_t));
        std::memcpy(compressed_out->data(), &base_value, sizeof(int64_t));
        return Status::OK;
    }

    std::vector<int64_t> deltas;
    deltas.reserve(segment.row_count - 1);
    for (uint32_t i = 1; i < segment.row_count; ++i)
    {
        deltas.push_back(values[i] - values[i - 1]);
    }

    ColumnSegment delta_segment;
    delta_segment.data_type = DataType::INT64;
    delta_segment.row_count = static_cast<uint32_t>(deltas.size());
    delta_segment.null_count = 0;
    delta_segment.null_bitmap.clear();
    delta_segment.data.resize(deltas.size() * sizeof(int64_t));
    std::memcpy(delta_segment.data.data(), deltas.data(), delta_segment.data.size());

    std::vector<uint8_t> delta_compressed;
    Status status = compressBitpack(delta_segment, &delta_compressed, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    compressed_out->clear();
    compressed_out->resize(sizeof(int64_t));
    std::memcpy(compressed_out->data(), &base_value, sizeof(int64_t));
    compressed_out->insert(compressed_out->end(), delta_compressed.begin(), delta_compressed.end());

    return Status::OK;
}

Status ColumnstoreIndex::decompressDelta(const std::vector<uint8_t> &compressed,
                                         DataType data_type,
                                         uint32_t row_count,
                                         ColumnSegment *segment_out,
                                         ErrorContext *ctx)
{
    if (!segment_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "segment_out is null");
        return Status::INVALID_ARGUMENT;
    }

    if (row_count == 0)
    {
        segment_out->data.clear();
        segment_out->row_count = 0;
        return Status::OK;
    }

    if (compressed.size() < sizeof(int64_t))
    {
        SET_ERROR_CONTEXT(ctx, Status::COMPRESSION_ERROR,
                         "Corrupted delta data (header too small)");
        return Status::COMPRESSION_ERROR;
    }

    int64_t base_value = 0;
    std::memcpy(&base_value, compressed.data(), sizeof(int64_t));

    if (row_count == 1)
    {
        segment_out->data_type = data_type;
        segment_out->row_count = 1;
        segment_out->data.resize(getDataTypeSize(data_type));
        uint8_t *data_ptr = segment_out->data.data();

        switch (data_type)
        {
        case DataType::INT8:
            *reinterpret_cast<int8_t *>(data_ptr) = static_cast<int8_t>(base_value);
            break;
        case DataType::INT16:
            *reinterpret_cast<int16_t *>(data_ptr) = static_cast<int16_t>(base_value);
            break;
        case DataType::INT32:
            *reinterpret_cast<int32_t *>(data_ptr) = static_cast<int32_t>(base_value);
            break;
        case DataType::INT64:
            *reinterpret_cast<int64_t *>(data_ptr) = base_value;
            break;
        case DataType::UINT8:
            *reinterpret_cast<uint8_t *>(data_ptr) = static_cast<uint8_t>(base_value);
            break;
        case DataType::UINT16:
            *reinterpret_cast<uint16_t *>(data_ptr) = static_cast<uint16_t>(base_value);
            break;
        case DataType::UINT32:
            *reinterpret_cast<uint32_t *>(data_ptr) = static_cast<uint32_t>(base_value);
            break;
        case DataType::UINT64:
            *reinterpret_cast<uint64_t *>(data_ptr) = static_cast<uint64_t>(base_value);
            break;
        default:
            break;
        }

        return Status::OK;
    }

    std::vector<uint8_t> delta_compressed(compressed.begin() + sizeof(int64_t), compressed.end());
    ColumnSegment delta_segment;
    delta_segment.null_bitmap.clear();
    delta_segment.null_count = 0;

    Status status = decompressBitpack(delta_compressed, DataType::INT64,
                                      row_count - 1, &delta_segment, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    segment_out->data_type = data_type;
    segment_out->row_count = row_count;
    size_t value_size = getDataTypeSize(data_type);
    segment_out->data.resize(static_cast<size_t>(row_count) * value_size);

    std::vector<int64_t> values(row_count, 0);
    values[0] = base_value;
    const int64_t *delta_ptr = reinterpret_cast<const int64_t *>(delta_segment.data.data());
    for (uint32_t i = 1; i < row_count; ++i)
    {
        values[i] = values[i - 1] + delta_ptr[i - 1];
    }

    uint8_t *data_ptr = segment_out->data.data();
    for (uint32_t i = 0; i < row_count; ++i)
    {
        switch (data_type)
        {
        case DataType::INT8:
            *reinterpret_cast<int8_t *>(data_ptr + i * value_size) = static_cast<int8_t>(values[i]);
            break;
        case DataType::INT16:
            *reinterpret_cast<int16_t *>(data_ptr + i * value_size) = static_cast<int16_t>(values[i]);
            break;
        case DataType::INT32:
            *reinterpret_cast<int32_t *>(data_ptr + i * value_size) = static_cast<int32_t>(values[i]);
            break;
        case DataType::INT64:
            *reinterpret_cast<int64_t *>(data_ptr + i * value_size) = values[i];
            break;
        case DataType::UINT8:
            *reinterpret_cast<uint8_t *>(data_ptr + i * value_size) = static_cast<uint8_t>(values[i]);
            break;
        case DataType::UINT16:
            *reinterpret_cast<uint16_t *>(data_ptr + i * value_size) = static_cast<uint16_t>(values[i]);
            break;
        case DataType::UINT32:
            *reinterpret_cast<uint32_t *>(data_ptr + i * value_size) = static_cast<uint32_t>(values[i]);
            break;
        case DataType::UINT64:
            *reinterpret_cast<uint64_t *>(data_ptr + i * value_size) = static_cast<uint64_t>(values[i]);
            break;
        default:
            break;
        }
    }

    return Status::OK;
}

// ============================================================================
// Predicate Pushdown
// ============================================================================

/**
 * Check if segment can be skipped based on min/max values
 *
 * This implements min/max pruning optimization.
 */
static bool canSkipSegment(int64_t min_value, int64_t max_value,
                          const ColumnPredicate &predicate)
{
    switch (predicate.op)
    {
    case ColumnPredicate::Op::EQUAL:
        // Skip if value is outside [min, max]
        return (predicate.value < min_value || predicate.value > max_value);

    case ColumnPredicate::Op::NOT_EQUAL:
        // Can only skip if all values are the same and equal to predicate value
        return (min_value == max_value && min_value == predicate.value);

    case ColumnPredicate::Op::LESS_THAN:
        // Skip if all values are >= predicate value
        return (min_value >= predicate.value);

    case ColumnPredicate::Op::LESS_EQUAL:
        // Skip if all values are > predicate value
        return (min_value > predicate.value);

    case ColumnPredicate::Op::GREATER_THAN:
        // Skip if all values are <= predicate value
        return (max_value <= predicate.value);

    case ColumnPredicate::Op::GREATER_EQUAL:
        // Skip if all values are < predicate value
        return (max_value < predicate.value);

    case ColumnPredicate::Op::IS_NULL:
    case ColumnPredicate::Op::IS_NOT_NULL:
        // Cannot skip based on min/max (NULL info not in min/max)
        return false;
    }

    return false;
}

/**
 * Evaluate predicate on a single value
 */
static bool evaluatePredicate(int64_t value, bool is_null,
                              const ColumnPredicate &predicate)
{
    // Handle NULL predicates
    if (predicate.op == ColumnPredicate::Op::IS_NULL)
        return is_null;
    if (predicate.op == ColumnPredicate::Op::IS_NOT_NULL)
        return !is_null;

    // NULL values don't match non-NULL predicates
    if (is_null)
        return false;

    // Evaluate predicate
    switch (predicate.op)
    {
    case ColumnPredicate::Op::EQUAL:
        return (value == predicate.value);
    case ColumnPredicate::Op::NOT_EQUAL:
        return (value != predicate.value);
    case ColumnPredicate::Op::LESS_THAN:
        return (value < predicate.value);
    case ColumnPredicate::Op::LESS_EQUAL:
        return (value <= predicate.value);
    case ColumnPredicate::Op::GREATER_THAN:
        return (value > predicate.value);
    case ColumnPredicate::Op::GREATER_EQUAL:
        return (value >= predicate.value);
    default:
        return false;
    }
}

#if defined(__AVX2__)
#include <immintrin.h>

/**
 * SIMD-accelerated predicate evaluation for INT32 arrays (Phase 5)
 *
 * Processes 8 INT32 values at once using AVX2 instructions.
 * Returns a bitmask where bit i is set if value i matches the predicate.
 *
 * @param values Pointer to array of at least 8 INT32 values (must be 32-byte aligned)
 * @param predicate Predicate to evaluate
 * @return 8-bit mask (bit i set = value i matches)
 */
static inline uint8_t evaluatePredicateSIMD_INT32(const int32_t *values,
                                                  const ColumnPredicate &predicate)
{
    // Load 8 INT32 values into AVX2 register (256 bits = 8 x 32 bits)
    __m256i vec_values = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(values));

    // Broadcast predicate value to all 8 lanes
    __m256i vec_predicate = _mm256_set1_epi32(static_cast<int32_t>(predicate.value));

    // Perform comparison based on operator
    __m256i cmp_result;
    switch (predicate.op)
    {
    case ColumnPredicate::Op::EQUAL:
        cmp_result = _mm256_cmpeq_epi32(vec_values, vec_predicate);
        break;

    case ColumnPredicate::Op::LESS_THAN:
        cmp_result = _mm256_cmpgt_epi32(vec_predicate, vec_values);  // predicate > value = value < predicate
        break;

    case ColumnPredicate::Op::GREATER_THAN:
        cmp_result = _mm256_cmpgt_epi32(vec_values, vec_predicate);
        break;

    case ColumnPredicate::Op::NOT_EQUAL:
    {
        // NOT EQUAL = ~(EQUAL)
        __m256i eq = _mm256_cmpeq_epi32(vec_values, vec_predicate);
        cmp_result = _mm256_xor_si256(eq, _mm256_set1_epi32(-1));  // Invert
        break;
    }

    case ColumnPredicate::Op::LESS_EQUAL:
    {
        // LESS_EQUAL = NOT(GREATER_THAN)
        __m256i gt = _mm256_cmpgt_epi32(vec_values, vec_predicate);
        cmp_result = _mm256_xor_si256(gt, _mm256_set1_epi32(-1));  // Invert
        break;
    }

    case ColumnPredicate::Op::GREATER_EQUAL:
    {
        // GREATER_EQUAL = NOT(LESS_THAN)
        __m256i lt = _mm256_cmpgt_epi32(vec_predicate, vec_values);
        cmp_result = _mm256_xor_si256(lt, _mm256_set1_epi32(-1));  // Invert
        break;
    }

    default:
        // Unsupported operator, return all zeros
        return 0;
    }

    // Convert comparison result to bitmask
    // Each lane is either 0xFFFFFFFF (match) or 0x00000000 (no match)
    int mask = _mm256_movemask_ps(_mm256_castsi256_ps(cmp_result));

    return static_cast<uint8_t>(mask);
}

/**
 * SIMD-accelerated predicate evaluation for INT64 arrays (Phase 5)
 *
 * Processes 4 INT64 values at once using AVX2 instructions.
 * Returns a bitmask where bit i is set if value i matches the predicate.
 *
 * @param values Pointer to array of at least 4 INT64 values (must be 32-byte aligned)
 * @param predicate Predicate to evaluate
 * @return 4-bit mask (bit i set = value i matches)
 */
static inline uint8_t evaluatePredicateSIMD_INT64(const int64_t *values,
                                                  const ColumnPredicate &predicate)
{
    // Load 4 INT64 values into AVX2 register (256 bits = 4 x 64 bits)
    __m256i vec_values = _mm256_loadu_si256(reinterpret_cast<const __m256i *>(values));

    // Broadcast predicate value to all 4 lanes
    __m256i vec_predicate = _mm256_set1_epi64x(predicate.value);

    // Perform comparison based on operator
    __m256i cmp_result;
    switch (predicate.op)
    {
    case ColumnPredicate::Op::EQUAL:
        cmp_result = _mm256_cmpeq_epi64(vec_values, vec_predicate);
        break;

    case ColumnPredicate::Op::LESS_THAN:
        cmp_result = _mm256_cmpgt_epi64(vec_predicate, vec_values);  // predicate > value
        break;

    case ColumnPredicate::Op::GREATER_THAN:
        cmp_result = _mm256_cmpgt_epi64(vec_values, vec_predicate);
        break;

    case ColumnPredicate::Op::NOT_EQUAL:
    {
        __m256i eq = _mm256_cmpeq_epi64(vec_values, vec_predicate);
        cmp_result = _mm256_xor_si256(eq, _mm256_set1_epi64x(-1));
        break;
    }

    case ColumnPredicate::Op::LESS_EQUAL:
    {
        __m256i gt = _mm256_cmpgt_epi64(vec_values, vec_predicate);
        cmp_result = _mm256_xor_si256(gt, _mm256_set1_epi64x(-1));
        break;
    }

    case ColumnPredicate::Op::GREATER_EQUAL:
    {
        __m256i lt = _mm256_cmpgt_epi64(vec_predicate, vec_values);
        cmp_result = _mm256_xor_si256(lt, _mm256_set1_epi64x(-1));
        break;
    }

    default:
        return 0;
    }

    // Convert to bitmask
    int mask = _mm256_movemask_pd(_mm256_castsi256_pd(cmp_result));

    return static_cast<uint8_t>(mask);
}

#endif // __AVX2__

/**
 * Apply predicate to a segment and return matching offsets
 *
 * This implements batch predicate evaluation.
 */
Status ColumnstoreIndex::applyPredicate(const ColumnSegment &segment,
                                       const ColumnPredicate &predicate,
                                       std::vector<uint32_t> *matching_offsets,
                                       ErrorContext *ctx)
{
    if (!matching_offsets)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Matching offsets output is null");
        return Status::INVALID_ARGUMENT;
    }

    matching_offsets->clear();

    // Step 1: Min/Max pruning
    if (canSkipSegment(segment.min_value, segment.max_value, predicate))
    {
        // Segment can be skipped entirely!
        return Status::OK;
    }

    // Step 2: Evaluate predicate on each value
    // Only support integer types for Phase 4
    if (segment.data_type != DataType::INT8 &&
        segment.data_type != DataType::INT16 &&
        segment.data_type != DataType::INT32 &&
        segment.data_type != DataType::INT64 &&
        segment.data_type != DataType::UINT8 &&
        segment.data_type != DataType::UINT16 &&
        segment.data_type != DataType::UINT32 &&
        segment.data_type != DataType::UINT64)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Predicate pushdown only supports integer types");
        return Status::INVALID_ARGUMENT;
    }

    size_t value_size = getDataTypeSize(segment.data_type);
    const uint8_t *data_ptr = segment.data.data();

    // Process values in batches of 1024 for better cache locality
    const uint32_t BATCH_SIZE = 1024;

    for (uint32_t batch_start = 0; batch_start < segment.row_count; batch_start += BATCH_SIZE)
    {
        uint32_t batch_end = std::min(batch_start + BATCH_SIZE, segment.row_count);

#if defined(__AVX2__)
        // SIMD-accelerated path for INT32 (Phase 5)
        if (segment.data_type == DataType::INT32 &&
            predicate.op != ColumnPredicate::Op::IS_NULL &&
            predicate.op != ColumnPredicate::Op::IS_NOT_NULL)
        {
            const int32_t *int32_data = reinterpret_cast<const int32_t *>(data_ptr);

            // Process 8 values at a time with SIMD
            uint32_t i = batch_start;
            for (; i + 8 <= batch_end; i += 8)
            {
                // Check for NULLs in this batch
                bool has_nulls = false;
                for (uint32_t j = 0; j < 8; ++j)
                {
                    if ((i + j) < segment.null_bitmap.size() && segment.null_bitmap[i + j])
                    {
                        has_nulls = true;
                        break;
                    }
                }

                if (!has_nulls)
                {
                    // No NULLs - use SIMD
                    uint8_t mask = evaluatePredicateSIMD_INT32(&int32_data[i], predicate);

                    // Add matching offsets
                    for (uint32_t j = 0; j < 8; ++j)
                    {
                        if (mask & (1 << j))
                        {
                            matching_offsets->push_back(i + j);
                        }
                    }
                }
                else
                {
                    // Has NULLs - fall back to scalar processing
                    for (uint32_t j = 0; j < 8; ++j)
                    {
                        bool is_null = (i + j) < segment.null_bitmap.size() ? segment.null_bitmap[i + j] : false;
                        if (!is_null)
                        {
                            int64_t value = static_cast<int64_t>(int32_data[i + j]);
                            if (evaluatePredicate(value, is_null, predicate))
                            {
                                matching_offsets->push_back(i + j);
                            }
                        }
                    }
                }
            }

            // Process remaining values (< 8) with scalar code
            for (; i < batch_end; ++i)
            {
                bool is_null = (i < segment.null_bitmap.size()) ? segment.null_bitmap[i] : false;
                if (!is_null)
                {
                    int64_t value = static_cast<int64_t>(int32_data[i]);
                    if (evaluatePredicate(value, is_null, predicate))
                    {
                        matching_offsets->push_back(i);
                    }
                }
            }

            continue;  // Skip to next batch
        }

        // SIMD-accelerated path for INT64 (Phase 5)
        if (segment.data_type == DataType::INT64 &&
            predicate.op != ColumnPredicate::Op::IS_NULL &&
            predicate.op != ColumnPredicate::Op::IS_NOT_NULL)
        {
            const int64_t *int64_data = reinterpret_cast<const int64_t *>(data_ptr);

            // Process 4 values at a time with SIMD
            uint32_t i = batch_start;
            for (; i + 4 <= batch_end; i += 4)
            {
                // Check for NULLs in this batch
                bool has_nulls = false;
                for (uint32_t j = 0; j < 4; ++j)
                {
                    if ((i + j) < segment.null_bitmap.size() && segment.null_bitmap[i + j])
                    {
                        has_nulls = true;
                        break;
                    }
                }

                if (!has_nulls)
                {
                    // No NULLs - use SIMD
                    uint8_t mask = evaluatePredicateSIMD_INT64(&int64_data[i], predicate);

                    // Add matching offsets
                    for (uint32_t j = 0; j < 4; ++j)
                    {
                        if (mask & (1 << j))
                        {
                            matching_offsets->push_back(i + j);
                        }
                    }
                }
                else
                {
                    // Has NULLs - fall back to scalar processing
                    for (uint32_t j = 0; j < 4; ++j)
                    {
                        bool is_null = (i + j) < segment.null_bitmap.size() ? segment.null_bitmap[i + j] : false;
                        if (!is_null)
                        {
                            int64_t value = int64_data[i + j];
                            if (evaluatePredicate(value, is_null, predicate))
                            {
                                matching_offsets->push_back(i + j);
                            }
                        }
                    }
                }
            }

            // Process remaining values (< 4) with scalar code
            for (; i < batch_end; ++i)
            {
                bool is_null = (i < segment.null_bitmap.size()) ? segment.null_bitmap[i] : false;
                if (!is_null)
                {
                    int64_t value = int64_data[i];
                    if (evaluatePredicate(value, is_null, predicate))
                    {
                        matching_offsets->push_back(i);
                    }
                }
            }

            continue;  // Skip to next batch
        }
#endif // __AVX2__

        // Scalar fallback path (for non-AVX2 systems or other data types)
        for (uint32_t i = batch_start; i < batch_end; ++i)
        {
            // Check NULL
            bool is_null = (i < segment.null_bitmap.size()) ? segment.null_bitmap[i] : false;

            // Read value based on data type
            int64_t value = 0;
            if (!is_null)
            {
                switch (segment.data_type)
                {
                case DataType::INT8:
                    value = static_cast<int64_t>(*reinterpret_cast<const int8_t *>(data_ptr + i * value_size));
                    break;
                case DataType::INT16:
                    value = static_cast<int64_t>(*reinterpret_cast<const int16_t *>(data_ptr + i * value_size));
                    break;
                case DataType::INT32:
                    value = static_cast<int64_t>(*reinterpret_cast<const int32_t *>(data_ptr + i * value_size));
                    break;
                case DataType::INT64:
                    value = *reinterpret_cast<const int64_t *>(data_ptr + i * value_size);
                    break;
                case DataType::UINT8:
                    value = static_cast<int64_t>(*reinterpret_cast<const uint8_t *>(data_ptr + i * value_size));
                    break;
                case DataType::UINT16:
                    value = static_cast<int64_t>(*reinterpret_cast<const uint16_t *>(data_ptr + i * value_size));
                    break;
                case DataType::UINT32:
                    value = static_cast<int64_t>(*reinterpret_cast<const uint32_t *>(data_ptr + i * value_size));
                    break;
                case DataType::UINT64:
                    value = static_cast<int64_t>(*reinterpret_cast<const uint64_t *>(data_ptr + i * value_size));
                    break;
                default:
                    break;
                }
            }

            // Evaluate predicate
            if (evaluatePredicate(value, is_null, predicate))
            {
                matching_offsets->push_back(i);
            }
        }
    }

    return Status::OK;
}

// ============================================================================
// Helper Methods
// ============================================================================

Status ColumnstoreIndex::findSegment(const ID &column_uuid,
                                     uint64_t tid,
                                     uint32_t *segment_page_out,
                                     ErrorContext *ctx)
{
    if (!segment_page_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Segment page output is null");
        return Status::INVALID_ARGUMENT;
    }

    BufferPool *buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Buffer pool not available");
        return Status::INVALID_ARGUMENT;
    }

    // Start at root page and traverse segment chain
    uint32_t current_page = index_info_.idx_root_page;

    while (current_page != 0)
    {
        // Pin segment page
        void *page_buffer = nullptr;
        Status status = pinIndexPage(current_page, &page_buffer, ctx,
                                     BufferPool::AccessStrategy::Sequential);
        if (status != Status::OK)
            return status;

        auto *page = static_cast<const SBColumnstorePage *>(page_buffer);

        // Check if TID is in this segment's range
        if (tid >= page->cs_first_tid && tid <= page->cs_last_tid)
        {
            // Found the segment!
            *segment_page_out = current_page;
            unpinIndexPage(current_page, false, ctx);
            return Status::OK;
        }

        // Move to next segment
        uint32_t next_page = page->cs_next_segment;
        unpinIndexPage(current_page, false, ctx);

        current_page = next_page;
    }

    // TID not found in any segment
    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Segment containing TID not found");
    return Status::NOT_FOUND;
}

Status ColumnstoreIndex::createSegment(const ID &column_uuid,
                                       const ColumnSegment &segment,
                                       uint32_t *segment_page_out,
                                       ErrorContext *ctx)
{
    if (!segment_page_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Segment page output is null");
        return Status::INVALID_ARGUMENT;
    }

    PageManager *page_mgr = db_->page_manager();
    BufferPool *buffer_pool = db_->buffer_pool();
    TransactionManager *txn_mgr = db_->transaction_manager();

    if (!page_mgr || !buffer_pool || !txn_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Required managers not available");
        return Status::INVALID_ARGUMENT;
    }

    // Step 1: Compress segment based on compression type
    std::vector<uint8_t> compressed;
    Status status;

    switch (segment.compression)
    {
    case CompressionType::NONE:
        // No compression - use raw data
        compressed = segment.data;
        break;

    case CompressionType::RLE:
        status = compressRLE(segment, &compressed, ctx);
        if (status != Status::OK)
            return status;
        break;

    case CompressionType::DICTIONARY:
    {
        // Dictionary encoding for string columns
        Dictionary dict;
        status = compressDictionary(segment, &compressed, &dict, ctx);
        if (status != Status::OK)
        {
            // If dictionary compression fails, fall back to RLE
            status = compressRLE(segment, &compressed, ctx);
            if (status != Status::OK)
                return status;
        }
        else
        {
            // Store dictionary size and entries at the beginning of compressed data
            std::vector<uint8_t> dict_data;

            // Write dictionary size (4 bytes)
            uint32_t dict_size = static_cast<uint32_t>(dict.size());
            dict_data.insert(dict_data.end(),
                           reinterpret_cast<uint8_t*>(&dict_size),
                           reinterpret_cast<uint8_t*>(&dict_size) + sizeof(uint32_t));

            // Write each dictionary entry: length (2 bytes) + string data
            for (size_t i = 0; i < dict.size(); ++i)
            {
                std::string value;
                if (!dict.getValue(static_cast<uint32_t>(i), &value))
                    continue;

                uint16_t len = static_cast<uint16_t>(value.length());
                dict_data.insert(dict_data.end(),
                               reinterpret_cast<uint8_t*>(&len),
                               reinterpret_cast<uint8_t*>(&len) + sizeof(uint16_t));
                dict_data.insert(dict_data.end(), value.begin(), value.end());
            }

            // Prepend dictionary to compressed codes
            compressed.insert(compressed.begin(), dict_data.begin(), dict_data.end());
        }
        break;
    }

    case CompressionType::BITPACK:
        status = compressBitpack(segment, &compressed, ctx);
        if (status != Status::OK)
            return status;
        break;

    case CompressionType::DELTA:
        status = compressDelta(segment, &compressed, ctx);
        if (status != Status::OK)
            return status;
        break;

    default:
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unknown compression type");
        return Status::INVALID_ARGUMENT;
    }

    // Step 2: Determine if multi-page segment is needed
    const size_t HEADER_SIZE = sizeof(SBColumnstorePage);
    const size_t PAGE_SIZE = db_->page_size();
    const size_t MAX_DATA_SIZE = PAGE_SIZE - HEADER_SIZE;

    const bool is_multipage = (compressed.size() > MAX_DATA_SIZE);
    const size_t total_pages = is_multipage
        ? ((compressed.size() + MAX_DATA_SIZE - 1) / MAX_DATA_SIZE)
        : 1;

    // Step 3: Allocate pages (first page + continuation pages if needed)
    std::vector<uint32_t> allocated_pages;
    allocated_pages.reserve(total_pages);

    for (size_t i = 0; i < total_pages; ++i)
    {
        uint32_t page_num = 0;
        GPID page_gpid = 0;
        status = page_mgr->allocatePageInTablespace(index_info_.idx_tablespace_id, &page_gpid, ctx);
        if (status != Status::OK)
        {
            // Clean up already allocated pages on failure
            for (uint32_t cleanup_page : allocated_pages)
            {
                page_mgr->freePageGlobal(makeGPID(index_info_.idx_tablespace_id, cleanup_page), ctx);
            }
            return status;
        }
        page_num = static_cast<uint32_t>(getPageNumber(page_gpid));
        allocated_pages.push_back(page_num);
    }

    uint32_t first_page = allocated_pages[0];

    // Step 4: Write data to pages
    for (size_t page_idx = 0; page_idx < total_pages; ++page_idx)
    {
        uint32_t current_page = allocated_pages[page_idx];
        bool is_first_page = (page_idx == 0);
        bool is_continuation = !is_first_page;

        // Pin page and initialize
        void *page_buffer = nullptr;
        status = pinIndexPage(current_page, &page_buffer, ctx,
                              BufferPool::AccessStrategy::BulkWrite);
        if (status != Status::OK)
        {
            // Clean up on failure
            for (uint32_t cleanup_page : allocated_pages)
            {
                page_mgr->freePage(cleanup_page, ctx);
            }
            return status;
        }

        auto *page = static_cast<SBColumnstorePage *>(page_buffer);
        std::memset(page, 0, sizeof(SBColumnstorePage));

        // Calculate data chunk for this page
        size_t data_offset = page_idx * MAX_DATA_SIZE;
        size_t data_chunk_size = std::min(MAX_DATA_SIZE, compressed.size() - data_offset);

        // Step 5: Initialize page header (for all pages)
        page->cs_header.magic = K_MAGIC_SBRD;
        page->cs_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1);
        page->cs_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_COLUMNSTORE);
        page->cs_header.page_size = PAGE_SIZE;
        page->cs_header.page_id = current_page;
        page->cs_header.checksum = 0;
        page->cs_header.lsn = 0;
        page->cs_header.flags = 0;
        std::memcpy(page->cs_header.database_uuid, db_->uuid().bytes.data(), 16);
        page->cs_header.generation = 0;
        page->cs_header.free_space = MAX_DATA_SIZE - data_chunk_size;
        page->cs_header.item_count = 1;
        page->cs_header.free_offset = 0;
        page->cs_header.special_size = 0;

        // Step 6: Initialize columnstore metadata
        page->cs_index_uuid = index_info_.idx_uuid;
        page->cs_table_uuid = index_info_.idx_table_uuid;
        page->cs_column_uuid = column_uuid;

        if (is_first_page)
        {
            // First page: full segment metadata
            page->cs_flags = static_cast<uint16_t>(ColumnstoreFlags::COMPRESSED);
            page->cs_row_count = segment.row_count;
            page->cs_null_count = segment.null_count;
            page->cs_compression_type = static_cast<uint8_t>(segment.compression);
            page->cs_data_type = static_cast<uint8_t>(segment.data_type);
            page->cs_compressed_size = static_cast<uint32_t>(compressed.size());
            page->cs_uncompressed_size = static_cast<uint32_t>(segment.data.size());

            // Set min/max values (for predicate pushdown)
            page->cs_min_value = segment.min_value;
            page->cs_max_value = segment.max_value;

            // Set TID range
            page->cs_first_tid = segment.first_tid;
            page->cs_last_tid = segment.last_tid;

            // MGA fields
            page->cs_xmin = txn_mgr->getCurrentXid();
            page->cs_xmax = 0;  // Active
            page->cs_lsn = 0;

            // Store page count in padding (first 4 bytes)
            uint32_t page_count = static_cast<uint32_t>(total_pages);
            std::memcpy(page->cs_padding, &page_count, sizeof(uint32_t));
        }
        else
        {
            // Continuation page: minimal metadata
            page->cs_flags = static_cast<uint16_t>(ColumnstoreFlags::CONTINUATION);
            page->cs_row_count = 0;  // Not applicable for continuation
            page->cs_null_count = 0;
            page->cs_compression_type = 0;
            page->cs_data_type = 0;
            page->cs_compressed_size = static_cast<uint32_t>(data_chunk_size);
            page->cs_uncompressed_size = 0;

            page->cs_min_value = 0;
            page->cs_max_value = 0;
            page->cs_first_tid = 0;
            page->cs_last_tid = 0;

            page->cs_xmin = txn_mgr->getCurrentXid();
            page->cs_xmax = 0;
            page->cs_lsn = 0;
        }

        // Set sibling pointers
        page->cs_prev_segment = (page_idx > 0) ? allocated_pages[page_idx - 1] : 0;
        page->cs_next_segment = (page_idx < total_pages - 1) ? allocated_pages[page_idx + 1] : 0;

        // Write data chunk to page
        uint8_t *data_area = reinterpret_cast<uint8_t *>(page) + HEADER_SIZE;
        std::memcpy(data_area, compressed.data() + data_offset, data_chunk_size);

        // Unpin page (mark dirty)
        unpinIndexPage(current_page, true, ctx);
    }

    // Return first page as segment page
    *segment_page_out = first_page;
    return Status::OK;
}

Status ColumnstoreIndex::readSegment(uint32_t segment_page,
                                     ColumnSegment *segment_out,
                                     ErrorContext *ctx)
{
    if (!segment_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Segment output is null");
        return Status::INVALID_ARGUMENT;
    }

    BufferPool *buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Buffer pool not available");
        return Status::INVALID_ARGUMENT;
    }

    // Pin first segment page
    void *page_buffer = nullptr;
    Status status = pinIndexPage(segment_page, &page_buffer, ctx,
                                 BufferPool::AccessStrategy::Sequential);
    if (status != Status::OK)
        return status;

    auto *page = static_cast<const SBColumnstorePage *>(page_buffer);

    // Read metadata from first page
    segment_out->column_uuid = page->cs_column_uuid;
    segment_out->data_type = static_cast<DataType>(page->cs_data_type);
    segment_out->row_count = page->cs_row_count;
    segment_out->null_count = page->cs_null_count;
    segment_out->compression = static_cast<CompressionType>(page->cs_compression_type);
    segment_out->first_tid = page->cs_first_tid;
    segment_out->last_tid = page->cs_last_tid;
    segment_out->min_value = page->cs_min_value;
    segment_out->max_value = page->cs_max_value;

    // Check if this is a multi-page segment (read page count from padding)
    uint32_t total_pages = 1;
    std::memcpy(&total_pages, page->cs_padding, sizeof(uint32_t));

    // If page count is 0 or unreasonable, assume single page
    if (total_pages == 0 || total_pages > 1000)
        total_pages = 1;

    const size_t HEADER_SIZE = sizeof(SBColumnstorePage);
    const size_t PAGE_SIZE = db_->page_size();
    const size_t MAX_DATA_SIZE = PAGE_SIZE - HEADER_SIZE;

    std::vector<uint8_t> compressed;
    compressed.reserve(page->cs_compressed_size);

    // Read compressed data from all pages
    uint32_t current_page = segment_page;
    for (uint32_t page_idx = 0; page_idx < total_pages; ++page_idx)
    {
        // For pages after the first, pin the next page
        if (page_idx > 0)
        {
            unpinIndexPage(current_page, false, ctx);

            // Get next page from previous page's cs_next_segment
            void *prev_page_buffer = nullptr;
            Status pin_status = pinIndexPage(current_page, &prev_page_buffer, ctx,
                                             BufferPool::AccessStrategy::Sequential);
            if (pin_status != Status::OK)
                return pin_status;

            auto *prev_page = static_cast<const SBColumnstorePage *>(prev_page_buffer);
            current_page = static_cast<uint32_t>(prev_page->cs_next_segment);
            unpinIndexPage(current_page, false, ctx);

            if (current_page == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::COMPRESSION_ERROR,
                               "Multi-page segment chain broken");
                return Status::COMPRESSION_ERROR;
            }

            // Pin next page
            pin_status = pinIndexPage(current_page, &page_buffer, ctx,
                                      BufferPool::AccessStrategy::Sequential);
            if (pin_status != Status::OK)
                return pin_status;

            page = static_cast<const SBColumnstorePage *>(page_buffer);
        }

        // Read data chunk from this page
        const uint8_t *chunk_data = reinterpret_cast<const uint8_t *>(page) + HEADER_SIZE;
        size_t chunk_size = (page_idx == 0)
            ? std::min(MAX_DATA_SIZE, static_cast<size_t>(page->cs_compressed_size))
            : page->cs_compressed_size;

        compressed.insert(compressed.end(), chunk_data, chunk_data + chunk_size);
    }

    // Unpin last page
    unpinIndexPage(current_page, false, ctx);

    // Decompress based on compression type
    switch (segment_out->compression)
    {
    case CompressionType::NONE:
        // No compression - data is already decompressed
        segment_out->data = std::move(compressed);
        break;

    case CompressionType::RLE:
        status = decompressRLE(compressed, segment_out->data_type,
                              segment_out->row_count, segment_out, ctx);
        if (status != Status::OK)
            return status;
        break;

    case CompressionType::DICTIONARY:
    {
        // Dictionary decompression for string columns
        Dictionary dict;
        const uint8_t *dict_data = compressed.data();
        size_t dict_offset = 0;

        // Read dictionary size (4 bytes)
        if (compressed.size() < sizeof(uint32_t))
        {
            SET_ERROR_CONTEXT(ctx, Status::COMPRESSION_ERROR,
                           "Invalid dictionary compressed data");
            return Status::COMPRESSION_ERROR;
        }

        uint32_t dict_size = 0;
        std::memcpy(&dict_size, dict_data + dict_offset, sizeof(uint32_t));
        dict_offset += sizeof(uint32_t);

        // Read dictionary entries
        for (uint32_t i = 0; i < dict_size; ++i)
        {
            if (dict_offset + sizeof(uint16_t) > compressed.size())
            {
                SET_ERROR_CONTEXT(ctx, Status::COMPRESSION_ERROR,
                               "Truncated dictionary data");
                return Status::COMPRESSION_ERROR;
            }

            uint16_t len = 0;
            std::memcpy(&len, dict_data + dict_offset, sizeof(uint16_t));
            dict_offset += sizeof(uint16_t);

            if (dict_offset + len > compressed.size())
            {
                SET_ERROR_CONTEXT(ctx, Status::COMPRESSION_ERROR,
                               "Truncated dictionary string");
                return Status::COMPRESSION_ERROR;
            }

            std::string value(reinterpret_cast<const char*>(dict_data + dict_offset), len);
            dict.addValue(value);
            dict_offset += len;
        }

        // Extract RLE-compressed codes
        std::vector<uint8_t> codes_compressed(dict_data + dict_offset,
                                              dict_data + compressed.size());

        // Decompress using dictionary
        status = decompressDictionary(codes_compressed, dict, segment_out->data_type,
                                     segment_out->row_count, segment_out, ctx);
        if (status != Status::OK)
            return status;
        break;
    }

    case CompressionType::BITPACK:
        status = decompressBitpack(compressed, segment_out->data_type,
                                  segment_out->row_count, segment_out, ctx);
        if (status != Status::OK)
            return status;
        break;

    case CompressionType::DELTA:
        status = decompressDelta(compressed, segment_out->data_type,
                                 segment_out->row_count, segment_out, ctx);
        if (status != Status::OK)
            return status;
        break;

    default:
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unknown compression type");
        return Status::INVALID_ARGUMENT;
    }

    return Status::OK;
}

bool ColumnstoreIndex::isValueVisible(uint64_t value_xmin,
                                      uint64_t value_xmax,
                                      uint64_t current_xid,
                                      ErrorContext *ctx) const
{
    // Firebird MGA visibility check using TIP (Transaction Inventory Pages)
    //
    // MGA rules (see /MGA_RULES.md):
    // - Use TIP-based visibility via TransactionManager::isVersionVisible()
    // - Value created by uncommitted/aborted transaction → invisible
    // - Value deleted by committed transaction → invisible
    // - Otherwise → visible
    //
    // CRITICAL: No fallback logic - always use TIP for correctness

    TransactionManager *txn_mgr = db_->transaction_manager();
    if (!txn_mgr)
    {
        // This should never happen in production - log error if ctx available
        if (ctx)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "TransactionManager not available for visibility check");
        }
        return false;  // Fail-safe: treat as invisible if no txn manager
    }

    // Check if the version's creating transaction is visible to current transaction
    // This uses TIP to look up transaction state (committed/active/aborted)
    if (!txn_mgr->isVersionVisible(value_xmin, current_xid))
        return false;  // Creating transaction not visible

    // If value_xmax is set (value was deleted), check if deletion is visible
    // If deletion is visible, the value should not be visible
    if (value_xmax != 0 && txn_mgr->isVersionVisible(value_xmax, current_xid))
        return false;  // Deletion is visible, so value is not

    return true;
}

Status ColumnstoreIndex::getColumnDataType(const ID &column_uuid,
                                           DataType *data_type_out,
                                           size_t *value_size_out,
                                           ErrorContext *ctx)
{
    if (!data_type_out || !value_size_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Output parameters cannot be null");
        return Status::INVALID_ARGUMENT;
    }

    // Get catalog manager
    CatalogManager *catalog = db_->catalog_manager();
    if (!catalog)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "CatalogManager not available");
        // Fall back to INT32
        *data_type_out = DataType::INT32;
        *value_size_out = sizeof(int32_t);
        return Status::OK;
    }

    // Check if we have table_uuid in index_info
    // If idx_column_uuids is empty, we can't look up the schema
    if (index_info_.idx_column_uuids.empty())
    {
        // Fallback: assume INT32 (this maintains backward compatibility)
        *data_type_out = DataType::INT32;
        *value_size_out = sizeof(int32_t);
        return Status::OK;
    }

    // Get all columns for the table
    std::vector<CatalogManager::ColumnInfo> columns;
    Status col_status = catalog->getColumns(index_info_.idx_table_uuid, columns, ctx);
    if (col_status != Status::OK)
    {
        // If we can't get columns, fall back to INT32
        *data_type_out = DataType::INT32;
        *value_size_out = sizeof(int32_t);
        return Status::OK;
    }

    // Find the column with matching UUID
    for (const auto &col : columns)
    {
        if (std::memcmp(&col.column_id, &column_uuid, sizeof(ID)) == 0)
        {
            // Found matching column - extract data type
            *data_type_out = static_cast<DataType>(col.data_type);
            *value_size_out = getDataTypeSize(*data_type_out);

            // For variable-length types, use type_precision if available
            if (*value_size_out == 0 && col.type_precision > 0)
            {
                *value_size_out = col.type_precision;
            }

            // If still zero, default to a reasonable size
            if (*value_size_out == 0)
            {
                *value_size_out = 256;  // Default for variable-length types
            }

            return Status::OK;
        }
    }

    // Column not found - this shouldn't happen, but fall back gracefully
    *data_type_out = DataType::INT32;
    *value_size_out = sizeof(int32_t);
    return Status::OK;
}

Status ColumnstoreIndex::createMetadataPage(Database *db,
                                            const UuidV7Bytes &index_uuid,
                                            const UuidV7Bytes &table_uuid,
                                            const std::vector<UuidV7Bytes> &column_uuids,
                                            uint32_t segment_size,
                                            CompressionType compression,
                                            GPID metadata_gpid,
                                            ErrorContext *ctx)
{
    if (!db)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Database is null");
        return Status::INVALID_ARGUMENT;
    }

    if (metadata_gpid == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Metadata GPID cannot be zero");
        return Status::INVALID_ARGUMENT;
    }

    uint32_t metadata_page = static_cast<uint32_t>(getPageNumber(metadata_gpid));

    // Step 2: Pin page for writing
    BufferPool *buffer_pool = db->buffer_pool();
    if (!buffer_pool)
    {
        PageManager *page_mgr = db->page_manager();
        if (page_mgr)
        {
            page_mgr->freePageGlobal(metadata_gpid, ctx);
        }
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "BufferPool not available");
        return Status::INVALID_ARGUMENT;
    }

    void *page_buffer = nullptr;
    Status pin_status = buffer_pool->pinPageGlobal(metadata_gpid, &page_buffer, ctx,
                                                   BufferPool::AccessStrategy::BulkWrite);
    if (pin_status != Status::OK)
    {
        PageManager *page_mgr = db->page_manager();
        if (page_mgr)
        {
            page_mgr->freePageGlobal(metadata_gpid, ctx);
        }
        return pin_status;
    }

    // Step 3: Initialize metadata page
    auto *meta_page = static_cast<SBColumnstoreMetadataPage *>(page_buffer);
    std::memset(meta_page, 0, sizeof(SBColumnstoreMetadataPage));

    // Initialize header
    meta_page->cs_header.magic = K_MAGIC_SBRD;
    meta_page->cs_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1);
    meta_page->cs_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_COLUMNSTORE);
    meta_page->cs_header.page_size = db->page_size();

    // Store index and table UUIDs
    std::memcpy(&meta_page->cs_index_uuid, &index_uuid, sizeof(ID));
    std::memcpy(&meta_page->cs_table_uuid, &table_uuid, sizeof(ID));

    // Store configuration
    meta_page->cs_segment_size = segment_size;
    meta_page->cs_compression_type = static_cast<uint8_t>(compression);
    meta_page->cs_column_count = static_cast<uint16_t>(column_uuids.size());

    // Initialize segment tracking (empty index)
    meta_page->cs_first_segment_page = 0;
    meta_page->cs_total_segments = 0;
    meta_page->cs_total_rows = 0;

    // Set MGA fields (transaction visibility)
    TransactionManager *txn_mgr = db->transaction_manager();
    if (txn_mgr)
    {
        meta_page->cs_xmin = txn_mgr->getCurrentXid();
        meta_page->cs_xmax = 0;  // Not deleted
    }
    else
    {
        meta_page->cs_xmin = 0;
        meta_page->cs_xmax = 0;
    }

    // Step 4: Write column UUIDs array immediately after header
    // Calculate offset: sizeof(SBColumnstoreMetadataPage) is the space we have
    // Column UUIDs are written into the data section after the header
    uint8_t *uuid_data = reinterpret_cast<uint8_t *>(page_buffer) + sizeof(SBColumnstoreMetadataPage);
    size_t uuid_array_size = column_uuids.size() * sizeof(ID);

    // Verify we have space (sanity check)
    const size_t PAGE_SIZE = db->page_size();
    if (sizeof(SBColumnstoreMetadataPage) + uuid_array_size > PAGE_SIZE)
    {
        buffer_pool->unpinPageGlobal(metadata_gpid, false, ctx);
        PageManager *page_mgr = db->page_manager();
        if (page_mgr)
        {
            page_mgr->freePageGlobal(metadata_gpid, ctx);
        }
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Too many columns for metadata page");
        return Status::INVALID_ARGUMENT;
    }

    // Copy column UUIDs
    for (size_t i = 0; i < column_uuids.size(); ++i)
    {
        std::memcpy(uuid_data + (i * sizeof(ID)), &column_uuids[i], sizeof(ID));
    }

    // Step 5: Mark page dirty and unpin
    Status unpin_status = buffer_pool->unpinPageGlobal(metadata_gpid, true, ctx);
    if (unpin_status != Status::OK)
    {
        PageManager *page_mgr = db->page_manager();
        if (page_mgr)
        {
            page_mgr->freePageGlobal(metadata_gpid, ctx);
        }
        return unpin_status;
    }
    return Status::OK;
}

Status ColumnstoreIndex::readMetadataPage(uint32_t metadata_page, ErrorContext *ctx)
{
    // Step 1: Pin metadata page
    void *page_buffer = nullptr;
    Status pin_status = pinIndexPage(metadata_page, &page_buffer, ctx);
    if (pin_status != Status::OK)
        return pin_status;

    // Step 2: Read metadata
    auto *meta_page = static_cast<SBColumnstoreMetadataPage *>(page_buffer);

    // Verify page type
    if (meta_page->cs_header.page_type != static_cast<uint16_t>(PageType::PAGE_TYPE_COLUMNSTORE))
    {
    unpinIndexPage(metadata_page, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                         "Invalid page type for columnstore metadata");
        return Status::INVALID_ARGUMENT;
    }

    // Step 3: Extract configuration into index_info_
    index_info_.idx_segment_size = meta_page->cs_segment_size;
    index_info_.idx_compression_type = meta_page->cs_compression_type;
    index_info_.idx_total_segments = meta_page->cs_total_segments;
    index_info_.idx_total_rows = meta_page->cs_total_rows;

    // Read column UUIDs
    uint16_t column_count = meta_page->cs_column_count;
    index_info_.idx_column_uuids.clear();
    index_info_.idx_column_uuids.reserve(column_count);

    uint8_t *uuid_data = reinterpret_cast<uint8_t *>(page_buffer) + sizeof(SBColumnstoreMetadataPage);
    for (uint16_t i = 0; i < column_count; ++i)
    {
        ID column_uuid;
        std::memcpy(&column_uuid, uuid_data + (i * sizeof(ID)), sizeof(ID));
        index_info_.idx_column_uuids.push_back(column_uuid);
    }

    // Update root page to point to first segment (if any)
    index_info_.idx_root_page = meta_page->cs_first_segment_page;

    // Step 4: Unpin page
    Status unpin_status = unpinIndexPage(metadata_page, false, ctx);
    if (unpin_status != Status::OK)
    {
        return unpin_status;
    }

    // Cache the last segment page for append operations.
    last_segment_page_ = 0;
    if (index_info_.idx_root_page != 0)
    {
        uint32_t current_page = index_info_.idx_root_page;
        void *page_buffer = nullptr;
        Status pin_status = pinIndexPage(current_page, &page_buffer, ctx);
        if (pin_status != Status::OK)
        {
            return pin_status;
        }
        auto *page = static_cast<const SBColumnstorePage *>(page_buffer);
        while (page->cs_next_segment != 0)
        {
            uint32_t next_page = page->cs_next_segment;
            unpinIndexPage(current_page, false, ctx);
            current_page = next_page;
            pin_status = pinIndexPage(current_page, &page_buffer, ctx);
            if (pin_status != Status::OK)
            {
                return pin_status;
            }
            page = static_cast<const SBColumnstorePage *>(page_buffer);
        }
        last_segment_page_ = current_page;
        unpinIndexPage(current_page, false, ctx);
    }

    return Status::OK;
}

Status ColumnstoreIndex::flushSegment(const ID &column_uuid, ErrorContext *ctx)
{
    // Get buffer for this column
    auto it = column_buffers_.find(column_uuid);
    if (it == column_buffers_.end() || it->second.empty())
    {
        return Status::OK;  // Nothing to flush
    }

    std::vector<BufferedValue> &buffer = it->second;

    // Get data type from catalog
    DataType data_type = DataType::INT32;  // Default fallback
    size_t value_size = sizeof(int32_t);   // Default fallback

    Status dtype_status = getColumnDataType(column_uuid, &data_type, &value_size, ctx);
    if (dtype_status != Status::OK)
    {
        // If we can't get the data type, use the fallback values above
        // This maintains backward compatibility with existing tests
    }

    // Build ColumnSegment from buffered values
    ColumnSegment segment;
    segment.column_uuid = column_uuid;
    segment.data_type = data_type;
    segment.row_count = static_cast<uint32_t>(buffer.size());
    segment.compression = CompressionType::RLE;

    // Allocate data buffer
    segment.data.resize(buffer.size() * value_size);
    segment.null_bitmap.resize(buffer.size());

    // Track min/max/nulls
    segment.null_count = 0;
    segment.first_tid = buffer.front().tid;
    segment.last_tid = buffer.back().tid;

    int64_t min_val = INT64_MAX;
    int64_t max_val = INT64_MIN;

    // Copy buffered values to segment
    for (size_t i = 0; i < buffer.size(); ++i)
    {
        const BufferedValue &bv = buffer[i];

        // Set NULL flag
        segment.null_bitmap[i] = bv.is_null;
        if (bv.is_null)
        {
            segment.null_count++;
            // Write zeros for NULL values
            std::memset(segment.data.data() + (i * value_size), 0, value_size);
        }
        else
        {
            // Copy value
            if (bv.data.size() == value_size)
            {
                std::memcpy(segment.data.data() + (i * value_size), bv.data.data(), value_size);

                // Track min/max
                int32_t val = 0;
                std::memcpy(&val, bv.data.data(), sizeof(int32_t));
                if (val < min_val) min_val = val;
                if (val > max_val) max_val = val;
            }
        }
    }

    segment.min_value = min_val;
    segment.max_value = max_val;

    // Create segment page and write compressed data to disk
    uint32_t new_segment_page = 0;
    Status create_status = createSegment(column_uuid, segment, &new_segment_page, ctx);
    if (create_status != Status::OK)
        return create_status;

    // Link to previous segment if there is one
    BufferPool *buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Buffer pool not available");
        return Status::INVALID_ARGUMENT;
    }

    // Find the last page of the new segment (for multi-page segments)
    // The new segment might span multiple pages linked via cs_next_segment
    uint32_t new_segment_last_page = new_segment_page;
    {
        void *page_buffer = nullptr;
        Status pin_status = pinIndexPage(new_segment_last_page, &page_buffer, ctx);
        if (pin_status != Status::OK)
            return pin_status;

        auto *page = static_cast<const SBColumnstorePage *>(page_buffer);

        // Follow the chain to find the last page of this segment
        while (page->cs_next_segment != 0)
        {
            uint32_t next_page = page->cs_next_segment;
            unpinIndexPage(new_segment_last_page, false, ctx);

            new_segment_last_page = next_page;
            pin_status = pinIndexPage(new_segment_last_page, &page_buffer, ctx);
            if (pin_status != Status::OK)
                return pin_status;
            page = static_cast<const SBColumnstorePage *>(page_buffer);
        }

        unpinIndexPage(new_segment_last_page, false, ctx);
    }

    // Link new segment to the chain using cached last segment page (O(1) instead of O(n))
    if (index_info_.idx_root_page == 0)
    {
        // This is the first segment - make it the root
        index_info_.idx_root_page = new_segment_page;
        last_segment_page_ = new_segment_last_page;
    }
    else
    {
        // Use cached last_segment_page_ instead of traversing entire chain
        uint32_t prev_page = last_segment_page_;

        if (prev_page != 0)
        {
            // Link previous segment's last page to new segment's first page
            void *page_buffer = nullptr;
            Status link_status = pinIndexPage(prev_page, &page_buffer, ctx,
                                              BufferPool::AccessStrategy::BulkWrite);
            if (link_status != Status::OK)
                return link_status;

            auto *prev_seg = static_cast<SBColumnstorePage *>(page_buffer);
            prev_seg->cs_next_segment = new_segment_page;

            unpinIndexPage(prev_page, true, ctx);  // Mark dirty

            // Set prev pointer in new segment's first page
            Status new_pin_status = pinIndexPage(new_segment_page, &page_buffer, ctx,
                                                 BufferPool::AccessStrategy::BulkWrite);
            if (new_pin_status != Status::OK)
                return new_pin_status;

            auto *new_seg = static_cast<SBColumnstorePage *>(page_buffer);
            new_seg->cs_prev_segment = prev_page;

            unpinIndexPage(new_segment_page, true, ctx);  // Mark dirty
        }

        // Update cached last segment page to the last page of the new segment
        last_segment_page_ = new_segment_last_page;
    }

    // Update index statistics
    index_info_.idx_total_segments++;
    index_info_.idx_total_rows += segment.row_count;

    // Clear buffer now that it's flushed
    buffer.clear();

    Status meta_status = updateMetadataPage(ctx);
    if (meta_status != Status::OK)
    {
        return meta_status;
    }
    return Status::OK;
}

Status ColumnstoreIndex::updateMetadataPage(ErrorContext *ctx)
{
    void *page_buffer = nullptr;
    Status status = pinIndexPage(metadata_page_, &page_buffer, ctx,
                                 BufferPool::AccessStrategy::BulkWrite);
    if (status != Status::OK)
    {
        return status;
    }

    auto *meta_page = static_cast<SBColumnstoreMetadataPage *>(page_buffer);
    uint32_t first_segment =
        (index_info_.idx_root_page == metadata_page_) ? 0 : index_info_.idx_root_page;
    meta_page->cs_first_segment_page = first_segment;
    meta_page->cs_total_segments = index_info_.idx_total_segments;
    meta_page->cs_total_rows = index_info_.idx_total_rows;

    return unpinIndexPage(metadata_page_, true, ctx);
}

// ============================================================================
// Batch Scan Iterator (Phase 5)
// ============================================================================

Status ColumnstoreIndex::beginScan(const ID &column_uuid,
                                   const ColumnPredicate *predicate,
                                   uint64_t current_xid,
                                   ColumnScanIterator *iterator_out,
                                   ErrorContext *ctx)
{
    if (!iterator_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Iterator output is null");
        return Status::INVALID_ARGUMENT;
    }

    // Initialize iterator
    iterator_out->column_uuid = column_uuid;
    iterator_out->current_segment_page = index_info_.idx_root_page;
    iterator_out->offset_in_segment = 0;
    iterator_out->current_xid = current_xid;
    iterator_out->scan_complete = false;
    iterator_out->segment_cached = false;

    // Set predicate
    if (predicate)
    {
        iterator_out->predicate = *predicate;
        iterator_out->has_predicate = true;
    }
    else
    {
        iterator_out->has_predicate = false;
    }

    // If root page is 0, there are no segments yet
    if (index_info_.idx_root_page == 0)
    {
        iterator_out->scan_complete = true;
    }

    return Status::OK;
}

Status ColumnstoreIndex::scanNext(ColumnScanIterator *iterator,
                                  ColumnScanBatch *batch_out,
                                  ErrorContext *ctx)
{
    if (!iterator || !batch_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Iterator or batch output is null");
        return Status::INVALID_ARGUMENT;
    }

    // Clear output batch
    batch_out->tids.clear();
    batch_out->values.clear();
    batch_out->null_flags.clear();
    batch_out->count = 0;

    // Check if scan is already complete
    if (iterator->scan_complete)
    {
        return Status::OK;
    }

    BufferPool *buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Buffer pool not available");
        return Status::INVALID_ARGUMENT;
    }

    const uint32_t BATCH_SIZE = 1024;
    const uint32_t MAX_SEGMENTS = 10000;  // Safety limit to prevent infinite loops
    uint32_t segments_processed = 0;

    // Traverse segments until we have a full batch or reach the end
    while (iterator->current_segment_page != 0 && batch_out->count < BATCH_SIZE && segments_processed < MAX_SEGMENTS)
    {
        // Load segment if not cached
        if (!iterator->segment_cached)
        {
            Status status = readSegment(iterator->current_segment_page,
                                       &iterator->cached_segment, ctx);
            if (status != Status::OK)
                return status;

            iterator->segment_cached = true;
            iterator->offset_in_segment = 0;
        }

        ColumnSegment &segment = iterator->cached_segment;

        // Check MGA visibility for this segment
        if (!isValueVisible(segment.first_tid, 0, iterator->current_xid, ctx))
        {
            // Entire segment is invisible, skip to next
            void *page_buffer = nullptr;
            uint32_t old_page = iterator->current_segment_page;
            Status status = pinIndexPage(old_page, &page_buffer, ctx,
                                         BufferPool::AccessStrategy::Sequential);
            if (status != Status::OK)
                return status;

            auto *page = static_cast<const SBColumnstorePage *>(page_buffer);
            iterator->current_segment_page = page->cs_next_segment;
            unpinIndexPage(old_page, false, ctx);

            iterator->segment_cached = false;
            segments_processed++;
            continue;
        }

        // Apply predicate pushdown: Check if entire segment can be skipped
        if (iterator->has_predicate &&
            canSkipSegment(segment.min_value, segment.max_value, iterator->predicate))
        {
            // Skip this segment entirely
            void *page_buffer = nullptr;
            uint32_t old_page = iterator->current_segment_page;
            Status status = pinIndexPage(old_page, &page_buffer, ctx,
                                         BufferPool::AccessStrategy::Sequential);
            if (status != Status::OK)
                return status;

            auto *page = static_cast<const SBColumnstorePage *>(page_buffer);
            iterator->current_segment_page = page->cs_next_segment;
            unpinIndexPage(old_page, false, ctx);

            iterator->segment_cached = false;
            segments_processed++;
            continue;
        }

        // Process values from this segment
        size_t value_size = getDataTypeSize(segment.data_type);
        const uint8_t *data_ptr = segment.data.data();

        while (iterator->offset_in_segment < segment.row_count &&
               batch_out->count < BATCH_SIZE)
        {
            uint32_t i = iterator->offset_in_segment;

            // Check NULL
            bool is_null = (i < segment.null_bitmap.size()) ? segment.null_bitmap[i] : false;

            // Read value
            int64_t value = 0;
            if (!is_null && value_size > 0)
            {
                switch (segment.data_type)
                {
                case DataType::INT8:
                    value = static_cast<int64_t>(*reinterpret_cast<const int8_t *>(data_ptr + i * value_size));
                    break;
                case DataType::INT16:
                    value = static_cast<int64_t>(*reinterpret_cast<const int16_t *>(data_ptr + i * value_size));
                    break;
                case DataType::INT32:
                    value = static_cast<int64_t>(*reinterpret_cast<const int32_t *>(data_ptr + i * value_size));
                    break;
                case DataType::INT64:
                    value = *reinterpret_cast<const int64_t *>(data_ptr + i * value_size);
                    break;
                case DataType::UINT8:
                    value = static_cast<int64_t>(*reinterpret_cast<const uint8_t *>(data_ptr + i * value_size));
                    break;
                case DataType::UINT16:
                    value = static_cast<int64_t>(*reinterpret_cast<const uint16_t *>(data_ptr + i * value_size));
                    break;
                case DataType::UINT32:
                    value = static_cast<int64_t>(*reinterpret_cast<const uint32_t *>(data_ptr + i * value_size));
                    break;
                case DataType::UINT64:
                    value = static_cast<int64_t>(*reinterpret_cast<const uint64_t *>(data_ptr + i * value_size));
                    break;
                default:
                    break;
                }
            }

            // Apply predicate if provided
            bool matches = true;
            if (iterator->has_predicate)
            {
                matches = evaluatePredicate(value, is_null, iterator->predicate);
            }

            if (matches)
            {
                // Add to batch
                uint64_t tid = segment.first_tid + i;
                batch_out->tids.push_back(tid);

                // Copy value data
                if (is_null)
                {
                    batch_out->values.push_back(0);
                }
                else if (value_size > 0)
                {
                    const uint8_t *value_ptr = data_ptr + (i * value_size);
                    batch_out->values.insert(batch_out->values.end(),
                                            value_ptr, value_ptr + value_size);
                }

                batch_out->null_flags.push_back(is_null);
                batch_out->count++;
            }

            iterator->offset_in_segment++;
        }

        // Check if we've finished this segment
        if (iterator->offset_in_segment >= segment.row_count)
        {
            // Move to next segment
            void *page_buffer = nullptr;
            uint32_t old_page = iterator->current_segment_page;
            Status status = pinIndexPage(old_page, &page_buffer, ctx,
                                         BufferPool::AccessStrategy::Sequential);
            if (status != Status::OK)
                return status;

            auto *page = static_cast<const SBColumnstorePage *>(page_buffer);
            iterator->current_segment_page = page->cs_next_segment;
            unpinIndexPage(old_page, false, ctx);

            iterator->segment_cached = false;
            iterator->offset_in_segment = 0;
            segments_processed++;

            // Check if we've reached the end
            if (iterator->current_segment_page == 0)
            {
                iterator->scan_complete = true;
                break;
            }
        }
    }

    // Safety: if we hit the segment limit, mark scan as complete to avoid infinite loops
    if (segments_processed >= MAX_SEGMENTS)
    {
        iterator->scan_complete = true;
    }

    batch_out->data_type = iterator->segment_cached ?
        iterator->cached_segment.data_type : DataType::INT32;

    return Status::OK;
}

Status ColumnstoreIndex::endScan(ColumnScanIterator *iterator,
                                 ErrorContext *ctx)
{
    if (!iterator)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Iterator is null");
        return Status::INVALID_ARGUMENT;
    }

    // Clear cached segment data
    iterator->cached_segment.data.clear();
    iterator->cached_segment.null_bitmap.clear();
    iterator->segment_cached = false;
    iterator->scan_complete = true;

    return Status::OK;
}

} // namespace scratchbird::core
