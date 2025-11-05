#include "scratchbird/core/columnstore.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/database.h"
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
}

ColumnstoreIndex::~ColumnstoreIndex()
{
}

// ============================================================================
// Factory Methods
// ============================================================================

Status ColumnstoreIndex::create(Database *db,
                                const UuidV7Bytes &index_uuid,
                                const UuidV7Bytes &table_uuid,
                                const std::vector<UuidV7Bytes> &column_uuids,
                                uint32_t segment_size,
                                CompressionType compression,
                                uint32_t *root_page_out,
                                ErrorContext *ctx)
{
    if (!db)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database is null");
        return Status::INVALID_ARGUMENT;
    }

    if (column_uuids.empty())
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No columns specified");
        return Status::INVALID_ARGUMENT;
    }

    // Get managers
    PageManager *page_mgr = db->page_manager();
    BufferPool *buffer_pool = db->buffer_pool();
    TransactionManager *txn_mgr = db->transaction_manager();

    if (!page_mgr || !buffer_pool || !txn_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Required managers not available");
        return Status::INVALID_ARGUMENT;
    }

    // Allocate root segment page
    uint32_t root_page = 0;
    Status status = page_mgr->allocatePage(root_page, ctx);
    if (status != Status::OK)
        return status;

    // Pin and initialize root page
    void *page_buffer = nullptr;
    status = buffer_pool->pinPage(root_page, &page_buffer, ctx);
    if (status != Status::OK)
        return status;

    auto *root = static_cast<SBColumnstorePage *>(page_buffer);
    std::memset(root, 0, sizeof(SBColumnstorePage));

    // Initialize header - manual initialization (no helper exists)
    root->cs_header.magic = K_MAGIC_SBRD;
    root->cs_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1);
    root->cs_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_COLUMNSTORE);
    root->cs_header.page_size = 8192;
    root->cs_header.page_id = root_page;
    root->cs_header.checksum = 0;
    root->cs_header.lsn = 0;
    root->cs_header.flags = 0;
    std::memcpy(root->cs_header.database_uuid, db->uuid().bytes.data(), 16);
    root->cs_header.generation = 0;
    root->cs_header.free_space = 0;
    root->cs_header.item_count = 0;
    root->cs_header.free_offset = 0;
    root->cs_header.special_size = 0;

    // Initialize metadata
    std::memcpy(&root->cs_index_uuid, &index_uuid, sizeof(ID));
    std::memcpy(&root->cs_table_uuid, &table_uuid, sizeof(ID));
    std::memcpy(&root->cs_column_uuid, &column_uuids[0], sizeof(ID));  // First column

    root->cs_flags = 0;
    root->cs_row_count = 0;
    root->cs_null_count = 0;
    root->cs_compression_type = static_cast<uint8_t>(compression);
    root->cs_compressed_size = 0;
    root->cs_uncompressed_size = 0;

    // MGA compliance
    root->cs_xmin = txn_mgr->getCurrentXid();
    root->cs_xmax = 0;
    root->cs_lsn = 0;

    // Sibling pointers
    root->cs_prev_segment = 0;
    root->cs_next_segment = 0;

    // Unpin page
    buffer_pool->unpinPage(root_page, true, ctx);

    if (root_page_out)
        *root_page_out = root_page;

    return Status::OK;
}

std::unique_ptr<ColumnstoreIndex> ColumnstoreIndex::open(Database *db,
                                                         const UuidV7Bytes &index_uuid,
                                                         uint32_t root_page,
                                                         ErrorContext *ctx)
{
    if (!db)
        return nullptr;

    // TODO: Read metadata from catalog
    SBColumnstoreIndex index_info;
    std::memcpy(&index_info.idx_uuid, &index_uuid, sizeof(ID));
    index_info.idx_root_page = root_page;
    index_info.idx_segment_size = 1024;
    index_info.idx_compression_type = static_cast<uint8_t>(CompressionType::RLE);
    index_info.idx_total_segments = 0;
    index_info.idx_total_rows = 0;

    return std::make_unique<ColumnstoreIndex>(db, index_info);
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

    // Phase 1: Scan from buffered values
    // TODO: In future phases, also scan from disk segments

    std::lock_guard<std::mutex> lock(buffer_mutex_);

    auto it = column_buffers_.find(column_uuid);
    if (it == column_buffers_.end() || it->second.empty())
    {
        return Status::OK;  // No buffered values
    }

    const std::vector<BufferedValue> &buffer = it->second;

    // Scan through buffered values
    // Limit to avoid overflow (use size of vectors as capacity)
    size_t batch_capacity = std::min(batch_out->tids.size(), batch_out->values.size());
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

    // Phase 1: Return basic stats
    stats_out->total_segments = index_info_.idx_total_segments;
    stats_out->total_rows = index_info_.idx_total_rows;
    stats_out->compressed_bytes = 0;
    stats_out->uncompressed_bytes = 0;
    stats_out->compression_ratio = 1.0;
    stats_out->null_count = 0;

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

    // Phase 1: Stub implementation
    // TODO: Traverse segment chain to find segment containing TID
    //
    // Algorithm:
    // 1. Start at root_page
    // 2. Follow cs_next_segment pointers
    // 3. Check if tid is in range [cs_first_tid, cs_last_tid]
    // 4. Return segment page when found

    *segment_page_out = index_info_.idx_root_page;
    return Status::OK;
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

    // Phase 1: Stub implementation
    // TODO: Create new segment page and write compressed data
    //
    // Algorithm:
    // 1. Compress segment using compressRLE()
    // 2. Allocate new page
    // 3. Write compressed data to page
    // 4. Update min/max values
    // 5. Set xmin for MGA
    // 6. Link to previous segment

    PageManager *page_mgr = db_->page_manager();
    if (!page_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Page manager not available");
        return Status::INVALID_ARGUMENT;
    }

    uint32_t new_page = 0;
    Status status = page_mgr->allocatePage(new_page, ctx);
    if (status != Status::OK)
        return status;

    *segment_page_out = new_page;
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

    // Phase 1: Stub implementation
    // TODO: Read segment page and decompress
    //
    // Algorithm:
    // 1. Pin segment page
    // 2. Read SBColumnstorePage header
    // 3. Read compressed data
    // 4. Decompress based on compression_type
    // 5. Unpin page

    segment_out->row_count = 0;
    return Status::OK;
}

bool ColumnstoreIndex::isValueVisible(uint64_t value_xmin,
                                      uint64_t value_xmax,
                                      uint64_t current_xid,
                                      ErrorContext *ctx) const
{
    // Phase 1: Basic MGA visibility check
    //
    // Firebird MGA rules:
    // - Value created after current transaction → invisible
    // - Value deleted before current transaction → invisible
    // - Otherwise → visible

    if (value_xmin > current_xid)
        return false;

    if (value_xmax != 0 && value_xmax <= current_xid)
        return false;

    // TODO: Use TransactionManager for full TIP-based visibility
    TransactionManager *txn_mgr = db_->transaction_manager();
    if (!txn_mgr)
        return true;  // Fallback: assume visible

    if (!txn_mgr->isVersionVisible(value_xmin, current_xid))
        return false;

    if (value_xmax != 0 && txn_mgr->isVersionVisible(value_xmax, current_xid))
        return false;

    return true;
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

    // Determine data type from first non-null value
    DataType data_type = DataType::INT32;  // Default, should be determined from table schema
    size_t value_size = sizeof(int32_t);   // Default

    // For Phase 1, we'll assume INT32. In a real implementation, this would come from table metadata
    // TODO: Get actual data type from table schema

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

    // Compress segment using RLE
    std::vector<uint8_t> compressed;
    Status status = compressRLE(segment, &compressed, ctx);
    if (status != Status::OK)
        return status;

    // Create segment page and write compressed data
    // TODO: Implement createSegment() fully to write to disk
    // For Phase 1, we'll just clear the buffer (compression tested separately)

    // Clear buffer now that it's flushed
    buffer.clear();

    return Status::OK;
}

} // namespace scratchbird::core
