/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// =================================================================================================
// ScratchBird Database Engine
// Columnstore Index Implementation
// November 20, 2025
// =================================================================================================

#include "scratchbird/core/columnstore_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include <cstring>
#include <algorithm>
#include <unordered_set>

namespace scratchbird {
namespace core {

    ColumnstoreIndexSimple::ColumnstoreIndexSimple(Database *db, const UuidV7Bytes &index_uuid, GPID meta_gpid)
        : db_(db),
          index_uuid_(index_uuid),
          meta_page_(static_cast<uint32_t>(getPageNumber(meta_gpid))),
          peer_meta_page_(0),
          tablespace_id_(getTablespaceID(meta_gpid)),
          generation_(0)
    {
    }

    ColumnstoreIndexSimple::~ColumnstoreIndexSimple() = default;

    Status ColumnstoreIndexSimple::create(Database *db, const UuidV7Bytes &index_uuid,
                                    GPID meta_gpid, ErrorContext *ctx)
    {
        // Plan 01 Task C: Allocate dual meta pages for crash-safe catalog persistence
        auto page_mgr = db->page_manager();
        auto buffer_pool = db->buffer_pool();
        if (!page_mgr || !buffer_pool)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing database components");
            return Status::INVALID_ARGUMENT;
        }

        // Allocate meta_page_a (primary)
        if (meta_gpid == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Meta GPID cannot be zero");
            return Status::INVALID_ARGUMENT;
        }
        uint16_t tablespace_id = getTablespaceID(meta_gpid);
        uint32_t meta_page_a = static_cast<uint32_t>(getPageNumber(meta_gpid));

        // Allocate meta_page_b (secondary/peer)
        uint32_t meta_page_b = 0;
        GPID meta_page_b_gpid = 0;
        Status status = page_mgr->allocatePageInTablespace(tablespace_id, &meta_page_b_gpid, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to allocate meta_page_b");
            return status;
        }
        meta_page_b = static_cast<uint32_t>(getPageNumber(meta_page_b_gpid));

        // Initialize both meta pages with empty catalog (generation = 0)
        ColumnstoreMetaHeader header;
        header.magic = COLUMNSTORE_META_MAGIC;
        header.version = 1;
        header.reserved = 0;
        header.generation = 0;
        header.segment_count = 0;
        header.peer_page_id = meta_page_b;  // Store peer in meta_page_a
        header.checksum = 0;  // Will calculate below

        // Calculate checksum (CRC32C of everything after checksum field)
        // For empty catalog, this checksums fields after checksum in the header
        const uint8_t* checksum_data_start = reinterpret_cast<const uint8_t*>(&header) +
                                            offsetof(ColumnstoreMetaHeader, checksum) + sizeof(uint32_t);
        size_t checksum_data_len = sizeof(ColumnstoreMetaHeader) -
                                   offsetof(ColumnstoreMetaHeader, checksum) - sizeof(uint32_t);
        uint32_t crc = crc32cCompute(checksum_data_start, checksum_data_len, 0xFFFFFFFF);
        header.checksum = crc ^ 0xFFFFFFFF;

        // Write to meta_page_a
        void* buffer_a = nullptr;
        status = buffer_pool->pinPageGlobal(makeGPID(tablespace_id, meta_page_a), &buffer_a, ctx,
                                            BufferPool::AccessStrategy::BulkWrite);
        if (status != Status::OK)
        {
            page_mgr->freePageGlobal(makeGPID(tablespace_id, meta_page_a), nullptr);
            page_mgr->freePageGlobal(makeGPID(tablespace_id, meta_page_b), nullptr);
            return status;
        }
        memcpy(buffer_a, &header, sizeof(header));
        buffer_pool->unpinPageGlobal(makeGPID(tablespace_id, meta_page_a), true, ctx);

        // Write to meta_page_b (same content)
        void* buffer_b = nullptr;
        status = buffer_pool->pinPageGlobal(makeGPID(tablespace_id, meta_page_b), &buffer_b, ctx,
                                            BufferPool::AccessStrategy::BulkWrite);
        if (status != Status::OK)
        {
            page_mgr->freePageGlobal(makeGPID(tablespace_id, meta_page_a), nullptr);
            page_mgr->freePageGlobal(makeGPID(tablespace_id, meta_page_b), nullptr);
            return status;
        }
        memcpy(buffer_b, &header, sizeof(header));
        buffer_pool->unpinPageGlobal(makeGPID(tablespace_id, meta_page_b), true, ctx);

        return Status::OK;
    }

    std::unique_ptr<ColumnstoreIndexSimple> ColumnstoreIndexSimple::open(Database *db,
                                                              const UuidV7Bytes &index_uuid,
                                                              GPID meta_gpid,
                                                              ErrorContext *ctx)
    {
        auto index = std::make_unique<ColumnstoreIndexSimple>(db, index_uuid, meta_gpid);

        // Load segment catalog from meta page
        Status status = index->loadSegmentCatalog(ctx);
        if (status == Status::INDEX_CORRUPTED)
        {
            // Both meta pages are corrupted - cannot open index
            SET_ERROR_CONTEXT(ctx, status, "Cannot open columnstore index: both meta pages corrupted");
            return nullptr;
        }
        else if (status != Status::OK)
        {
            // Empty index is OK
            index->column_segments_.clear();
        }

        return index;
    }

    GPID ColumnstoreIndexSimple::indexGPID(uint64_t page_num) const
    {
        return makeGPID(tablespace_id_, page_num);
    }

    Status ColumnstoreIndexSimple::pinIndexPage(uint64_t page_num, void **buffer, ErrorContext *ctx,
                                                BufferPool::AccessStrategy strategy) const
    {
        return db_->buffer_pool()->pinPageGlobal(indexGPID(page_num), buffer, ctx, strategy);
    }

    Status ColumnstoreIndexSimple::unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext *ctx) const
    {
        return db_->buffer_pool()->unpinPageGlobal(indexGPID(page_num), dirty, ctx);
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
        GPID data_gpid = 0;
        status = page_mgr->allocatePageInTablespace(tablespace_id_, &data_gpid, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        data_page = static_cast<uint32_t>(getPageNumber(data_gpid));

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

    Status ColumnstoreIndexSimple::insertRow(uint16_t column_id, const TID &tid,
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

    Status ColumnstoreIndexSimple::gcCompact(ErrorContext *ctx)
    {
        // GC compaction: Compact segments, remove deleted data (ScratchBird MGA GC)
        // In production:
        // 1. Merge small segments
        // 2. Remove expired data (MGA-based)
        // 3. Recompress with better algorithms
        // 4. Rebuild segment catalog

        // For now, return OK as GC compaction is optional
        return Status::OK;
    }

    Status ColumnstoreIndexSimple::removeDeadEntries(const std::vector<TID> &dead_tids,
                                               uint64_t *entries_removed_out,
                                               uint64_t *pages_modified_out,
                                               ErrorContext *ctx)
    {
        if (entries_removed_out) *entries_removed_out = 0;
        if (pages_modified_out) *pages_modified_out = 0;

        if (dead_tids.empty())
        {
            return Status::OK;
        }

        std::unordered_set<TID> dead_set;
        dead_set.reserve(dead_tids.size());
        for (const auto &tid : dead_tids)
        {
            dead_set.insert(tid);
        }

        if (dead_set.empty())
        {
            return Status::OK;
        }

        uint64_t removed = 0;
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            for (auto &pair : row_buffer_)
            {
                auto &rows = pair.second;
                size_t before = rows.size();
                rows.erase(std::remove_if(rows.begin(), rows.end(),
                                          [&](const BufferedRow &row)
                                          {
                                              return dead_set.count(row.tid) > 0;
                                          }),
                           rows.end());
                removed += (before - rows.size());
            }
        }

        if (entries_removed_out)
        {
            *entries_removed_out = removed;
        }
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
        // Plan 01 Task C: Read from both meta pages, select newest valid based on generation
        auto buffer_pool = db_->buffer_pool();

        // Helper lambda to read and validate a meta page
        auto readMetaPage = [&](uint32_t page_id, ColumnstoreMetaHeader* header_out, std::vector<ColumnSegment>* segments_out) -> bool
        {
            void* buffer = nullptr;
            Status status = pinIndexPage(page_id, &buffer, nullptr);
            if (status != Status::OK)
            {
                return false;
            }

            // Read header
            memcpy(header_out, buffer, sizeof(ColumnstoreMetaHeader));

            // Validate magic
            if (header_out->magic != COLUMNSTORE_META_MAGIC)
            {
                unpinIndexPage(page_id, false, nullptr);
                return false;
            }

            // Validate checksum
            const uint8_t* checksum_data_start = static_cast<const uint8_t*>(buffer) + offsetof(ColumnstoreMetaHeader, checksum) + sizeof(uint32_t);
            size_t checksum_data_len = sizeof(ColumnstoreMetaHeader) + header_out->segment_count * sizeof(ColumnSegment) - offsetof(ColumnstoreMetaHeader, checksum) - sizeof(uint32_t);
            uint32_t calculated_crc = crc32cCompute(checksum_data_start, checksum_data_len, 0xFFFFFFFF) ^ 0xFFFFFFFF;

            if (calculated_crc != header_out->checksum)
            {
                unpinIndexPage(page_id, false, nullptr);
                return false;
            }

            // Read segments
            if (header_out->segment_count > 0)
            {
                const ColumnSegment* segments_ptr = reinterpret_cast<const ColumnSegment*>(static_cast<const uint8_t*>(buffer) + sizeof(ColumnstoreMetaHeader));
                segments_out->assign(segments_ptr, segments_ptr + header_out->segment_count);
            }

            unpinIndexPage(page_id, false, nullptr);
            return true;
        };

        // Read meta_page_a
        ColumnstoreMetaHeader header_a;
        std::vector<ColumnSegment> segments_a;
        bool valid_a = readMetaPage(meta_page_, &header_a, &segments_a);

        // Get peer page ID from header_a if valid
        if (valid_a && header_a.peer_page_id != 0)
        {
            peer_meta_page_ = header_a.peer_page_id;
        }

        // Read meta_page_b if we know where it is
        ColumnstoreMetaHeader header_b;
        std::vector<ColumnSegment> segments_b;
        bool valid_b = false;
        if (peer_meta_page_ != 0)
        {
            valid_b = readMetaPage(peer_meta_page_, &header_b, &segments_b);
        }

        // Select newest valid page based on generation
        if (!valid_a && !valid_b)
        {
            // Both invalid - mark index failed
            SET_ERROR_CONTEXT(ctx, Status::INDEX_CORRUPTED, "Both columnstore meta pages invalid");
            return Status::INDEX_CORRUPTED;
        }
        else if (valid_a && !valid_b)
        {
            // Only A valid
            generation_ = header_a.generation;
            // Rebuild column_segments_ map from flat array
            column_segments_.clear();
            for (const auto& seg : segments_a)
            {
                column_segments_[seg.column_id].push_back(seg);
            }
        }
        else if (!valid_a && valid_b)
        {
            // Only B valid
            generation_ = header_b.generation;
            column_segments_.clear();
            for (const auto& seg : segments_b)
            {
                column_segments_[seg.column_id].push_back(seg);
            }
        }
        else
        {
            // Both valid - pick newest generation
            if (header_a.generation >= header_b.generation)
            {
                generation_ = header_a.generation;
                column_segments_.clear();
                for (const auto& seg : segments_a)
                {
                    column_segments_[seg.column_id].push_back(seg);
                }
            }
            else
            {
                generation_ = header_b.generation;
                column_segments_.clear();
                for (const auto& seg : segments_b)
                {
                    column_segments_[seg.column_id].push_back(seg);
                }
            }
        }

        return Status::OK;
    }

    Status ColumnstoreIndexSimple::saveSegmentCatalog(ErrorContext* ctx)
    {
        // Plan 01 Task C: Write segment catalog to dual meta pages with generation increment
        auto buffer_pool = db_->buffer_pool();

        // Count total segments
        uint32_t total_segments = 0;
        for (const auto& [column_id, segments] : column_segments_)
        {
            total_segments += segments.size();
        }

        // Increment generation
        generation_++;

        // Build catalog data: [header][segment1][segment2]...
        std::vector<uint8_t> catalog_data;
        catalog_data.resize(sizeof(ColumnstoreMetaHeader) + total_segments * sizeof(ColumnSegment));

        // Fill header
        ColumnstoreMetaHeader* header = reinterpret_cast<ColumnstoreMetaHeader*>(catalog_data.data());
        header->magic = COLUMNSTORE_META_MAGIC;
        header->version = 1;
        header->reserved = 0;
        header->generation = generation_;
        header->segment_count = total_segments;
        header->peer_page_id = peer_meta_page_;
        header->checksum = 0;  // Will calculate below

        // Fill segments
        ColumnSegment* segments_ptr = reinterpret_cast<ColumnSegment*>(catalog_data.data() + sizeof(ColumnstoreMetaHeader));
        size_t idx = 0;
        for (const auto& [column_id, segments] : column_segments_)
        {
            for (const auto& seg : segments)
            {
                segments_ptr[idx++] = seg;
            }
        }

        // Calculate checksum (CRC32C of everything after checksum field)
        const uint8_t* checksum_data_start = catalog_data.data() + offsetof(ColumnstoreMetaHeader, checksum) + sizeof(uint32_t);
        size_t checksum_data_len = catalog_data.size() - offsetof(ColumnstoreMetaHeader, checksum) - sizeof(uint32_t);
        uint32_t crc = crc32cCompute(checksum_data_start, checksum_data_len, 0xFFFFFFFF);
        header->checksum = crc ^ 0xFFFFFFFF;

        // Write to meta_page_a (primary)
        void* buffer_a = nullptr;
        Status status = pinIndexPage(meta_page_, &buffer_a, ctx,
                                     BufferPool::AccessStrategy::BulkWrite);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin meta_page_a for write");
            return status;
        }
        memcpy(buffer_a, catalog_data.data(), catalog_data.size());
        unpinIndexPage(meta_page_, true, ctx);

        // Write to meta_page_b (peer)
        void* buffer_b = nullptr;
        status = pinIndexPage(peer_meta_page_, &buffer_b, ctx,
                              BufferPool::AccessStrategy::BulkWrite);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin meta_page_b for write");
            // Note: meta_page_a already written, but that's OK - it has newer generation
            return status;
        }
        memcpy(buffer_b, catalog_data.data(), catalog_data.size());
        unpinIndexPage(peer_meta_page_, true, ctx);

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
