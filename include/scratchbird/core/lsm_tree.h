#pragma once

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/index_gc_interface.h"
#include "scratchbird/core/tid.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <optional>

namespace scratchbird
{
    namespace core
    {
        // Forward declarations
        class Database;

        // LSM-Tree - Log-Structured Merge Tree for write-optimized storage
        // Optimized for high write throughput with eventual compaction
        // November 19, 2025
        class LSMTree : public IndexGCInterface
        {
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

            // Vacuum the index
            Status vacuum(ErrorContext *ctx = nullptr);

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
            Database *db_;
            UuidV7Bytes index_uuid_;
            uint32_t meta_page_;
        };

    } // namespace core
} // namespace scratchbird
