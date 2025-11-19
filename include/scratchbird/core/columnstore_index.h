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

namespace scratchbird
{
    namespace core
    {
        // Forward declarations
        class Database;

        // Columnstore Index - Columnar storage for analytical workloads
        // Stores data in column-oriented format for efficient scans
        // November 19, 2025
        class ColumnstoreIndex : public IndexGCInterface
        {
        public:
            // Constructor
            ColumnstoreIndex(Database *db, const UuidV7Bytes &index_uuid, uint32_t meta_page);

            // Create a new columnstore index
            static Status create(Database *db, const UuidV7Bytes &index_uuid,
                                 uint32_t *meta_page_out, ErrorContext *ctx = nullptr);

            // Open an existing columnstore index
            static std::unique_ptr<ColumnstoreIndex> open(Database *db, const UuidV7Bytes &index_uuid,
                                                          uint32_t meta_page, ErrorContext *ctx = nullptr);

            // Destructor
            ~ColumnstoreIndex();

            // Insert column data
            Status insertColumn(uint16_t column_id, uint32_t row_count,
                                const std::vector<uint8_t> &column_data,
                                ErrorContext *ctx = nullptr);

            // Scan column range
            Status scanColumn(uint16_t column_id, uint32_t start_row, uint32_t end_row,
                              std::vector<uint8_t> *data_out, ErrorContext *ctx = nullptr);

            // Vacuum the index
            Status vacuum(ErrorContext *ctx = nullptr);

            // IndexGCInterface implementation
            Status removeDeadEntries(const std::vector<TID> &dead_tids,
                                     uint64_t *entries_removed_out = nullptr,
                                     uint64_t *pages_modified_out = nullptr,
                                     ErrorContext *ctx = nullptr) override;

            const char *indexTypeName() const override { return "Columnstore"; }

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
