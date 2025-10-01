#pragma once

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/uuidv7.h"
#include <cstdint>
#include <vector>
#include <memory>


    namespace scratchbird::core
    {

        // Forward declarations
        struct ErrorContext;
        class ToastManager;
        class Database;

        // Type alias for UUID-based IDs
        using ID = UuidV7Bytes;

// Heap page item pointer (line pointer)
// Points to actual tuple data within the page
#pragma pack(push, 1)
        struct ItemPointer
        {
            uint32_t offset;      // Offset from start of page (supports up to 4GB pages)
            uint32_t length : 31; // Length of tuple (max ~2GB)
            uint32_t flags : 1;   // 0 = normal, 1 = deleted

            static constexpr uint32_t FLAG_DELETED = 0x80000000;

            [[nodiscard]] auto isDeleted() const -> bool
            {
                return (flags & 1) != 0;
            }
            void setDeleted(bool deleted)
            {
                flags = deleted ? 1 : 0;
            }

            // Validate item pointer bounds
            [[nodiscard]] auto isValid(uint32_t page_size) const -> bool
            {
                // Check offset is within page bounds
                if (offset >= page_size) {
                    return false;
                }
                // Check that offset + length doesn't overflow page
                if (offset + length > page_size) {
                    return false;
                }
                // Check minimum offset (must be after standard page header)
                if (offset < sizeof(PageHeader)) {
                    return false;
                }
                return true;
            }
        };
#pragma pack(pop)

// Tuple header - metadata for each tuple
#pragma pack(push, 1)
        struct TupleHeader
        {
            uint64_t xmin;               // Transaction ID that inserted this tuple
            uint64_t xmax;               // Transaction ID that deleted this tuple (or 0)
            uint16_t flags;              // Various flags
            uint16_t null_bitmap_offset; // Offset to null bitmap (0 if no nulls)

            static constexpr uint16_t FLAG_HAS_NULLS = 0x0001;
            static constexpr uint16_t FLAG_DELETED = 0x0002;

            [[nodiscard]] auto hasNulls() const -> bool
            {
                return (flags & FLAG_HAS_NULLS) != 0;
            }
            [[nodiscard]] auto isDeleted() const -> bool
            {
                return (flags & FLAG_DELETED) != 0;
            }
        };
#pragma pack(pop)

// Special area at the end of heap pages
#pragma pack(push, 1)
        struct HeapPageSpecial
        {
            uint16_t pd_flags;     // Page flags
            uint16_t reserved;     // Reserved for alignment
            uint32_t pd_lower;     // Offset to start of free space (supports up to 4GB pages)
            uint32_t pd_upper;     // Offset to end of free space
            uint32_t pd_special;   // Offset to start of special area
            uint64_t pd_prune_xid; // Oldest XID for pruning
        };
#pragma pack(pop)

        // Heap page class - manages tuple storage within a page
        class HeapPage
        {
        public:
            // Constructor wraps an existing page buffer
            explicit HeapPage(uint8_t *page_data, uint32_t page_size);

            // Constructor with TOAST support
            HeapPage(uint8_t *page_data, uint32_t page_size, ToastManager *toast_mgr, Database *db,
                     const ID &table_id);

            // Initialize a new heap page
                         auto initialize(uint32_t page_id, ErrorContext *ctx = nullptr) -> Status;

            // Insert a tuple into the page (with automatic TOASTing)
            // Returns the item ID (slot number) on success
            auto insertTuple(const uint8_t *tuple_data, uint32_t tuple_size, uint64_t xmin,
                                uint16_t *item_id_out, ErrorContext *ctx = nullptr) -> Status;

            // Get tuple data by item ID (with automatic detoasting)
            auto getTuple(uint16_t item_id, const uint8_t **data_out, uint32_t *size_out,
                             ErrorContext *ctx = nullptr) -> Status;

            // Get tuple data by item ID with detoasting into provided buffer
            auto getTupleDetoasted(uint16_t item_id, std::vector<uint8_t> *buffer,
                                       uint64_t xmin, ErrorContext *ctx = nullptr) -> Status;

            // Mark tuple as deleted (and clean up TOAST if present)
            auto deleteTuple(uint16_t item_id, uint64_t xmax, ErrorContext *ctx = nullptr) -> Status;

            // Check if there's enough space for a tuple
            [[nodiscard]] auto hasFreeSpace(uint32_t tuple_size) const -> bool;

            // Get number of tuples (including deleted)
            [[nodiscard]] auto getItemCount() const -> uint16_t;

            // Get free space available
            [[nodiscard]] auto getFreeSpace() const -> uint32_t;

            // Validate page structure
            auto validate(ErrorContext *ctx = nullptr) const -> Status;

            // Get page header
            auto header() -> PageHeader *
            {
                return reinterpret_cast<PageHeader *>(page_data_);
            }
            [[nodiscard]] auto header() const -> const PageHeader *
            {
                return reinterpret_cast<const PageHeader *>(page_data_);
            }

        private:
            uint8_t *page_data_;
            uint32_t page_size_;
            ToastManager *toast_mgr_; // Optional TOAST manager
            Database *db_;            // Database for TOAST operations
            ID table_id_;    // Table ID for TOAST operations

            // Get pointer to item array (starts after PageHeader)
            auto getItemArray() -> ItemPointer *
            {
                return reinterpret_cast<ItemPointer *>(page_data_ + sizeof(PageHeader));
            }
            [[nodiscard]] auto getItemArray() const -> const ItemPointer *
            {
                return reinterpret_cast<const ItemPointer *>(page_data_ + sizeof(PageHeader));
            }

            // Get pointer to special area
            auto getSpecial() -> HeapPageSpecial *
            {
                return reinterpret_cast<HeapPageSpecial *>(page_data_ + page_size_ -
                                                           sizeof(HeapPageSpecial));
            }
            [[nodiscard]] auto getSpecial() const -> const HeapPageSpecial *
            {
                return reinterpret_cast<const HeapPageSpecial *>(page_data_ + page_size_ -
                                                                 sizeof(HeapPageSpecial));
            }

            // Update page header after modifications
            void updateHeaderStats();
        };

    } // namespace scratchbird::core

