// =================================================================================================
// ScratchBird Database Engine
// Columnstore Index Implementation
// November 20, 2025
// =================================================================================================

#include "scratchbird/core/columnstore_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/page_manager.h"
#include <cstring>
#include <algorithm>

namespace scratchbird {
namespace core {

    ColumnstoreIndexSimple::ColumnstoreIndexSimple(Database *db, const UuidV7Bytes &index_uuid, uint32_t meta_page)
        : db_(db), index_uuid_(index_uuid), meta_page_(meta_page)
    {
    }

    ColumnstoreIndexSimple::~ColumnstoreIndexSimple() = default;

    Status ColumnstoreIndexSimple::create(Database *db, const UuidV7Bytes &index_uuid,
                                    uint32_t *meta_page_out, ErrorContext *ctx)
    {
        // Allocate meta page for segment catalog
        auto page_mgr = db->page_manager();
        uint32_t meta_page;
        Status status = page_mgr->allocatePage(meta_page, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Initialize empty segment catalog
        // In production: write empty catalog to meta_page
        // For now, catalog is in-memory

        if (meta_page_out)
        {
            *meta_page_out = meta_page;
        }

        return Status::OK;
    }

    std::unique_ptr<ColumnstoreIndexSimple> ColumnstoreIndexSimple::open(Database *db,
                                                              const UuidV7Bytes &index_uuid,
                                                              uint32_t meta_page,
                                                              ErrorContext *ctx)
    {
        auto index = std::make_unique<ColumnstoreIndexSimple>(db, index_uuid, meta_page);

        // Load segment catalog from meta page
        Status status = index->loadSegmentCatalog(ctx);
        if (status != Status::OK)
        {
            // Empty index is OK
            index->column_segments_.clear();
        }

        return index;
    }

    Status ColumnstoreIndexSimple::insertColumn(uint16_t column_id, uint32_t row_count,
                                          const std::vector<uint8_t> &column_data,
                                          ErrorContext *ctx)
    {
        if (column_data.empty() || row_count == 0)
        {
            if (ctx)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Empty column data");
            }
            return Status::INVALID_ARGUMENT;
        }

        // Compress column data using RLE
        std::vector<uint8_t> compressed;
        Status status = compressRLE(column_data, &compressed, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Allocate page for compressed data
        auto page_mgr = db_->page_manager();
        uint32_t data_page;
        status = page_mgr->allocatePage(data_page, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // In production: write compressed data to page
        // For now, we track the page number in metadata

        // Calculate min/max for predicate pushdown
        // Assumes column_data contains int64_t values
        int64_t min_val = 0;
        int64_t max_val = 0;
        if (column_data.size() >= 8)
        {
            // Extract first value as min/max estimate
            std::memcpy(&min_val, column_data.data(), 8);
            max_val = min_val;

            // Scan for actual min/max (simplified: check every 8 bytes)
            for (size_t i = 8; i + 8 <= column_data.size(); i += 8)
            {
                int64_t val;
                std::memcpy(&val, column_data.data() + i, 8);
                min_val = std::min(min_val, val);
                max_val = std::max(max_val, val);
            }
        }

        // Determine start row for this segment
        uint32_t start_row = 0;
        auto& segments = column_segments_[column_id];
        if (!segments.empty())
        {
            // Append after last segment
            const auto& last = segments.back();
            start_row = last.start_row + last.row_count;
        }

        // Create segment metadata
        ColumnSegment segment;
        segment.column_id = column_id;
        segment.start_row = start_row;
        segment.row_count = row_count;
        segment.page_number = data_page;
        segment.compression_type = static_cast<uint8_t>(CompressionType::RLE);
        segment.min_value = min_val;
        segment.max_value = max_val;

        // Add to catalog
        segments.push_back(segment);

        // Save catalog to disk
        status = saveSegmentCatalog(ctx);

        return status;
    }

    // ========================================================================
    // STOR-M1: Row-Level OLTP Insert
    // ========================================================================

    Status ColumnstoreIndexSimple::insertRow(uint16_t column_id, uint64_t tid,
                                             const void *value, size_t value_len,
                                             bool is_null, ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);

        // Create buffered row
        BufferedRow row;
        row.tid = tid;
        row.is_null = is_null;

        // Copy value data
        if (!is_null && value && value_len > 0)
        {
            const uint8_t *bytes = static_cast<const uint8_t *>(value);
            row.data.assign(bytes, bytes + value_len);
        }

        // Add to column buffer
        row_buffer_[column_id].push_back(std::move(row));

        // Auto-flush if buffer reaches threshold
        if (row_buffer_[column_id].size() >= ROW_BUFFER_THRESHOLD)
        {
            return flushColumnBuffer(column_id, ctx);
        }

        return Status::OK;
    }

    Status ColumnstoreIndexSimple::flushRowBuffer(ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);

        // Flush all column buffers
        for (auto &pair : row_buffer_)
        {
            if (!pair.second.empty())
            {
                Status status = flushColumnBuffer(pair.first, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
            }
        }

        return Status::OK;
    }

    Status ColumnstoreIndexSimple::flushColumnBuffer(uint16_t column_id, ErrorContext *ctx)
    {
        // Called with buffer_mutex_ held
        auto it = row_buffer_.find(column_id);
        if (it == row_buffer_.end() || it->second.empty())
        {
            return Status::OK;
        }

        auto &rows = it->second;

        // Convert buffered rows to column-major format
        // Determine row size from first non-null row
        size_t row_size = 8; // Default to 8 bytes (int64_t)
        for (const auto &row : rows)
        {
            if (!row.is_null && !row.data.empty())
            {
                row_size = row.data.size();
                break;
            }
        }

        // Build column data vector
        std::vector<uint8_t> column_data;
        column_data.reserve(rows.size() * row_size);

        for (const auto &row : rows)
        {
            if (row.is_null || row.data.empty())
            {
                // Insert null placeholder (zeros)
                column_data.insert(column_data.end(), row_size, 0);
            }
            else if (row.data.size() >= row_size)
            {
                column_data.insert(column_data.end(), row.data.begin(), row.data.begin() + row_size);
            }
            else
            {
                // Pad short data with zeros
                column_data.insert(column_data.end(), row.data.begin(), row.data.end());
                column_data.insert(column_data.end(), row_size - row.data.size(), 0);
            }
        }

        // Insert using batch method
        uint32_t row_count = static_cast<uint32_t>(rows.size());
        Status status = insertColumn(column_id, row_count, column_data, ctx);

        // Clear buffer on success
        if (status == Status::OK)
        {
            rows.clear();
        }

        return status;
    }

    Status ColumnstoreIndexSimple::scanColumn(uint16_t column_id, uint32_t start_row, uint32_t end_row,
                                        std::vector<uint8_t> *data_out, ErrorContext *ctx)
    {
        if (data_out)
        {
            data_out->clear();
        }

        if (start_row >= end_row)
        {
            if (ctx)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid row range");
            }
            return Status::INVALID_ARGUMENT;
        }

        // Find segments covering the requested range
        auto it = column_segments_.find(column_id);
        if (it == column_segments_.end())
        {
            // Column not found - return empty result
            return Status::OK;
        }

        const auto& segments = it->second;
        std::vector<uint8_t> result;

        for (const auto& segment : segments)
        {
            uint32_t seg_end = segment.start_row + segment.row_count;

            // Skip segments before range
            if (seg_end <= start_row)
            {
                continue;
            }

            // Stop if segment starts after range
            if (segment.start_row >= end_row)
            {
                break;
            }

            // Segment overlaps with requested range
            // In production: read compressed data from segment.page_number
            // Decompress and extract relevant rows

            // For now, create placeholder data
            std::vector<uint8_t> segment_data;
            Status status = decompressRLE(std::vector<uint8_t>(), segment.row_count, &segment_data, ctx);

            // Determine which rows from this segment to include
            uint32_t copy_start = std::max(start_row, segment.start_row) - segment.start_row;
            uint32_t copy_end = std::min(end_row, seg_end) - segment.start_row;

            // Assume each row is 8 bytes (int64_t)
            size_t bytes_per_row = segment_data.size() / segment.row_count;
            if (bytes_per_row == 0) bytes_per_row = 8; // Default

            size_t copy_offset = copy_start * bytes_per_row;
            size_t copy_length = (copy_end - copy_start) * bytes_per_row;

            if (copy_offset + copy_length <= segment_data.size())
            {
                result.insert(result.end(),
                             segment_data.begin() + copy_offset,
                             segment_data.begin() + copy_offset + copy_length);
            }
        }

        if (data_out)
        {
            *data_out = std::move(result);
        }

        return Status::OK;
    }

    Status ColumnstoreIndexSimple::vacuum(ErrorContext *ctx)
    {
        // Vacuum: Compact segments, remove deleted data
        // In production:
        // 1. Merge small segments
        // 2. Remove expired data (MGA-based)
        // 3. Recompress with better algorithms
        // 4. Rebuild segment catalog

        // For now, return OK as vacuum is optional
        return Status::OK;
    }

    Status ColumnstoreIndexSimple::removeDeadEntries(const std::vector<TID> &dead_tids,
                                               uint64_t *entries_removed_out,
                                               uint64_t *pages_modified_out,
                                               ErrorContext *ctx)
    {
        // Garbage collection: Remove rows corresponding to dead TIDs
        // This is complex for columnstore as TIDs map to row numbers

        if (entries_removed_out) *entries_removed_out = 0;
        if (pages_modified_out) *pages_modified_out = 0;

        if (dead_tids.empty())
        {
            return Status::OK;
        }

        // In production:
        // 1. Convert TIDs to row numbers
        // 2. Mark rows as deleted in segments
        // 3. Recompress segments without deleted rows
        // 4. Update segment catalog

        // For now, stub implementation
        return Status::OK;
    }

    // ========================================================================
    // Helper Methods
    // ========================================================================

    Status ColumnstoreIndexSimple::compressRLE(const std::vector<uint8_t>& data,
                                         std::vector<uint8_t>* compressed,
                                         ErrorContext* ctx)
    {
        if (!compressed)
        {
            return Status::INVALID_ARGUMENT;
        }

        compressed->clear();

        if (data.empty())
        {
            return Status::OK;
        }

        // Simple RLE compression for byte sequences
        // Format: [count:4][value:1] repeated

        size_t i = 0;
        while (i < data.size())
        {
            uint8_t current = data[i];
            uint32_t run_length = 1;

            // Count consecutive identical bytes
            while (i + run_length < data.size() &&
                   data[i + run_length] == current &&
                   run_length < UINT32_MAX)
            {
                run_length++;
            }

            // Write (count, value) pair
            compressed->push_back(static_cast<uint8_t>(run_length & 0xFF));
            compressed->push_back(static_cast<uint8_t>((run_length >> 8) & 0xFF));
            compressed->push_back(static_cast<uint8_t>((run_length >> 16) & 0xFF));
            compressed->push_back(static_cast<uint8_t>((run_length >> 24) & 0xFF));
            compressed->push_back(current);

            i += run_length;
        }

        return Status::OK;
    }

    Status ColumnstoreIndexSimple::decompressRLE(const std::vector<uint8_t>& compressed,
                                           uint32_t row_count,
                                           std::vector<uint8_t>* data,
                                           ErrorContext* ctx)
    {
        if (!data)
        {
            return Status::INVALID_ARGUMENT;
        }

        data->clear();

        if (compressed.empty())
        {
            // Return placeholder data
            data->resize(row_count * 8, 0); // 8 bytes per row
            return Status::OK;
        }

        // Decompress RLE format: [count:4][value:1] repeated
        size_t i = 0;
        while (i + 4 < compressed.size())
        {
            // Read count (4 bytes, little-endian)
            uint32_t count = static_cast<uint32_t>(compressed[i]) |
                           (static_cast<uint32_t>(compressed[i+1]) << 8) |
                           (static_cast<uint32_t>(compressed[i+2]) << 16) |
                           (static_cast<uint32_t>(compressed[i+3]) << 24);

            // Read value (1 byte)
            uint8_t value = compressed[i+4];

            // Expand run
            for (uint32_t j = 0; j < count; j++)
            {
                data->push_back(value);
            }

            i += 5;
        }

        return Status::OK;
    }

    Status ColumnstoreIndexSimple::loadSegmentCatalog(ErrorContext* ctx)
    {
        // In production: read segment catalog from meta_page_
        // Format: [num_columns:2] [for each column: column_id:2, num_segments:4, segments...]

        // For now, start with empty catalog
        column_segments_.clear();

        return Status::OK;
    }

    Status ColumnstoreIndexSimple::saveSegmentCatalog(ErrorContext* ctx)
    {
        // In production: write segment catalog to meta_page_
        // Serialize all segments to disk

        // For now, catalog is in-memory only
        return Status::OK;
    }

    ColumnstoreIndexSimple::ColumnSegment* ColumnstoreIndexSimple::findSegment(uint16_t column_id, uint32_t row)
    {
        auto it = column_segments_.find(column_id);
        if (it == column_segments_.end())
        {
            return nullptr;
        }

        // Binary search for segment containing row
        auto& segments = it->second;
        for (auto& segment : segments)
        {
            if (row >= segment.start_row && row < segment.start_row + segment.row_count)
            {
                return &segment;
            }
        }

        return nullptr;
    }

} // namespace core
} // namespace scratchbird
