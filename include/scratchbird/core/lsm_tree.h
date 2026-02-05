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
#include "scratchbird/core/index_gc_interface.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/lsm_bloom_filter.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <optional>
#include <map>
#include <mutex>

namespace scratchbird
{
    namespace core
    {
        // Forward declarations
        class Database;

        // Forward declaration for iterator friend
        class LSMTreeIterator;

        // LSM-Tree - Log-Structured Merge Tree for write-optimized storage
        // Optimized for high write throughput with eventual compaction
        // November 19, 2025
        class LSMTree : public IndexGCInterface
        {
            // Friend class for iterator access to private members
            friend class LSMTreeIterator;

        public:
            // LSM entry for iteration
            struct Entry
            {
                std::vector<uint8_t> key;
                TID tid;
                uint64_t xmin;
                uint64_t xmax;
            };

            // Iterator interface for range scans
            class Iterator
            {
            public:
                virtual ~Iterator() = default;
                virtual bool hasNext() = 0;
                virtual std::optional<Entry> next() = 0;
            };

            // Constructor
            LSMTree(Database *db, const UuidV7Bytes &index_uuid, uint32_t meta_page);

            // Create a new LSM-Tree
            static Status create(Database *db, const UuidV7Bytes &index_uuid,
                                 uint32_t *meta_page_out, ErrorContext *ctx = nullptr);

            // Open an existing LSM-Tree
            static std::unique_ptr<LSMTree> open(Database *db, const UuidV7Bytes &index_uuid,
                                                 uint32_t meta_page, ErrorContext *ctx = nullptr);

            // Destructor
            ~LSMTree();

            // Put key-value pair (LSM uses put instead of insert)
            Status put(const std::vector<uint8_t> &key, const TID &tid,
                       uint64_t xmin, ErrorContext *ctx = nullptr);

            // Get values for a key
            Status get(const std::vector<uint8_t> &key, uint64_t current_xid,
                       std::vector<TID> *results_out, ErrorContext *ctx = nullptr);

            // Remove entry (MGA logical deletion)
            Status remove(const std::vector<uint8_t> &key, const TID &tid,
                          uint64_t xmax, ErrorContext *ctx = nullptr);

            // Range scan with iterator
            std::unique_ptr<Iterator> rangeScan(const std::vector<uint8_t> *start_key,
                                                const std::vector<uint8_t> *end_key,
                                                uint64_t current_xid,
                                                bool start_inclusive = true,
                                                bool end_inclusive = false,
                                                ErrorContext *ctx = nullptr);

            // Compact the LSM-Tree (merge levels)
            Status compact(ErrorContext *ctx = nullptr);

            // GC compaction (ScratchBird MGA GC, not PostgreSQL VACUUM)
            Status gcCompact(ErrorContext *ctx = nullptr);

            // IndexGCInterface implementation
            Status removeDeadEntries(const std::vector<TID> &dead_tids,
                                     uint64_t *entries_removed_out = nullptr,
                                     uint64_t *pages_modified_out = nullptr,
                                     ErrorContext *ctx = nullptr) override;

            const char *indexTypeName() const override { return "LSM"; }

            // Get index metadata
            const UuidV7Bytes &getIndexUuid() const { return index_uuid_; }
            uint32_t getMetaPage() const { return meta_page_; }

        private:
            // Internal entry with tombstone marker
            struct InternalEntry
            {
                TID tid;
                uint64_t xmin;
                uint64_t xmax; // 0 = not deleted
                bool is_tombstone; // Marks logical deletion
            };

            Database *db_;
            UuidV7Bytes index_uuid_;
            uint32_t meta_page_;

            // Memtable: in-memory sorted map (write buffer)
            // Maps key -> list of entries (to support multiple versions)
            std::map<std::vector<uint8_t>, std::vector<InternalEntry>> memtable_;
            mutable std::mutex memtable_mutex_;
            size_t memtable_size_bytes_; // Approximate size
            static constexpr size_t MAX_MEMTABLE_SIZE = 4 * 1024 * 1024; // 4MB

            // Helper: Compare keys
            static int compareKeys(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b);
        };

    } // namespace core
} // namespace scratchbird
