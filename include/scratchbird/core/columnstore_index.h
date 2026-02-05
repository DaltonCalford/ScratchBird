/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/index_gc_interface.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/gpid.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <map>
#include <mutex>

namespace scratchbird
{
    namespace core
    {
        // Forward declarations
        class Database;

        // ColumnstoreIndexSimple - Basic columnar storage for internal use
        // This is a simplified version for runtime index management (executor, storage_engine)
        // For full-featured columnstore with RLE/Dict/Bitpack compression, use columnstore.h
        // November 19, 2025
        class ColumnstoreIndexSimple : public IndexGCInterface
        {
        public:
            // Constructor
            ColumnstoreIndexSimple(Database *db, const UuidV7Bytes &index_uuid, GPID meta_gpid);

            // Create a new columnstore index
            static Status create(Database *db, const UuidV7Bytes &index_uuid,
                                 GPID meta_gpid, ErrorContext *ctx = nullptr);

            // Open an existing columnstore index
            static std::unique_ptr<ColumnstoreIndexSimple> open(Database *db, const UuidV7Bytes &index_uuid,
                                                          GPID meta_gpid, ErrorContext *ctx = nullptr);

            // Destructor
            ~ColumnstoreIndexSimple();

            // Insert column data (batch)
            Status insertColumn(uint16_t column_id, uint32_t row_count,
                                const std::vector<uint8_t> &column_data,
                                ErrorContext *ctx = nullptr);

            // Insert single row (STOR-M1: Row-Level OLTP)
            // Buffers individual row inserts and auto-flushes when threshold reached
            Status insertRow(uint16_t column_id, const TID &tid,
                             const void *value, size_t value_len,
                             bool is_null, ErrorContext *ctx = nullptr);

            // Flush any buffered rows to column segments
            Status flushRowBuffer(ErrorContext *ctx = nullptr);

            // Scan column range
            Status scanColumn(uint16_t column_id, uint32_t start_row, uint32_t end_row,
                              std::vector<uint8_t> *data_out, ErrorContext *ctx = nullptr);

            // GC compaction (ScratchBird MGA GC, not PostgreSQL VACUUM)
            Status gcCompact(ErrorContext *ctx = nullptr);

            // IndexGCInterface implementation
            Status removeDeadEntries(const std::vector<TID> &dead_tids,
                                     uint64_t *entries_removed_out = nullptr,
                                     uint64_t *pages_modified_out = nullptr,
                                     ErrorContext *ctx = nullptr) override;

            const char *indexTypeName() const override { return "ColumnstoreSimple"; }

            // Get index metadata
            const UuidV7Bytes &getIndexUuid() const { return index_uuid_; }
            uint32_t getMetaPage() const { return meta_page_; }

        private:
            // Plan 01 Task C: Dual meta page structure for crash-safe catalog persistence
            // Meta page format: [header][segment entries...]
            struct ColumnstoreMetaHeader
            {
                uint32_t magic;           // 0x43534D50 ('CSMP' = Columnstore Meta Page)
                uint16_t version;         // Format version (1 for now)
                uint16_t reserved;        // Alignment padding
                uint64_t generation;      // Incremented on each write
                uint32_t segment_count;   // Number of ColumnSegment entries
                uint32_t peer_page_id;    // The other meta page (for dual-page persistence)
                uint32_t checksum;        // CRC32C of everything after this field
            };

            static constexpr uint32_t COLUMNSTORE_META_MAGIC = 0x43534D50; // 'CSMP'

            // Column segment metadata (persisted to disk)
            struct ColumnSegment
            {
                uint16_t column_id;
                uint32_t start_row;
                uint32_t row_count;
                uint32_t page_number;      // Where compressed data is stored
                uint8_t compression_type;  // 0=none, 1=RLE, 2=dict, 3=bitpack
                int64_t min_value;        // For predicate pushdown
                int64_t max_value;        // For predicate pushdown
            };

            // Compression types
            enum class CompressionType : uint8_t
            {
                NONE = 0,
                RLE = 1,
                DICTIONARY = 2,
                BITPACK = 3
            };

            Database *db_;
            UuidV7Bytes index_uuid_;
            uint32_t meta_page_;         // meta_page_a (primary)
            uint32_t peer_meta_page_;    // meta_page_b (secondary for dual-page persistence)
            uint16_t tablespace_id_ = 0;
            uint64_t generation_;        // Current generation counter

            // In-memory segment catalog (loaded from meta page)
            std::map<uint16_t, std::vector<ColumnSegment>> column_segments_;

            // STOR-M1: Row-level OLTP buffering
            struct BufferedRow {
                TID tid;
                std::vector<uint8_t> data;
                bool is_null;
            };

            // Row buffer per column (column_id -> buffered rows)
            std::map<uint16_t, std::vector<BufferedRow>> row_buffer_;
            mutable std::mutex buffer_mutex_;
            static constexpr size_t ROW_BUFFER_THRESHOLD = 1000;  // Auto-flush at this count

            // Helper methods
            Status compressRLE(const std::vector<uint8_t>& data, std::vector<uint8_t>* compressed, ErrorContext* ctx);
            Status decompressRLE(const std::vector<uint8_t>& compressed, uint32_t row_count, std::vector<uint8_t>* data, ErrorContext* ctx);

            Status loadSegmentCatalog(ErrorContext* ctx);
            Status saveSegmentCatalog(ErrorContext* ctx);

            GPID indexGPID(uint64_t page_num) const;
            Status pinIndexPage(uint64_t page_num, void **buffer, ErrorContext *ctx = nullptr,
                                BufferPool::AccessStrategy strategy = BufferPool::AccessStrategy::Normal) const;
            Status unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext *ctx = nullptr) const;
            Status flushColumnBuffer(uint16_t column_id, ErrorContext* ctx);

            ColumnSegment* findSegment(uint16_t column_id, uint32_t row);
        };

    } // namespace core
} // namespace scratchbird
