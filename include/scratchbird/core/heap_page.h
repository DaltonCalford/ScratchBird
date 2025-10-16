#pragma once

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/transaction_manager.h"
#include <cstdint>
#include <vector>
#include <memory>

namespace scratchbird::core
{

    // Forward declarations
    struct ErrorContext;
    class ToastManager;
    class Database;
    class TransactionManager;

    // Type alias for UUID-based IDs
    using ID = UuidV7Bytes;

// Heap page item pointer (line pointer)
// Points to actual tuple data within the page
#pragma pack(push, 1)
    struct ItemPointer
    {
        uint32_t offset;      // Offset from start of page (supports up to 4GB pages)
        uint32_t length : 31; // Length of tuple (max ~2GB)
        uint32_t flags : 1;   // 0 = normal, 1 = deleted/unused

        static constexpr uint32_t FLAG_DELETED = 0x80000000;
        static constexpr uint32_t LP_UNUSED = 0; // Offset 0 means unused

        [[nodiscard]] auto isDeleted() const -> bool
        {
            return (flags & 1) != 0;
        }
        void setDeleted(bool deleted)
        {
            flags = deleted ? 1 : 0;
        }

        [[nodiscard]] auto isUnused() const -> bool
        {
            return offset == LP_UNUSED && length == 0;
        }
        void setUnused()
        {
            offset = LP_UNUSED;
            length = 0;
            flags = 0;
        }

        // Validate item pointer bounds
        [[nodiscard]] auto isValid(uint32_t page_size) const -> bool
        {
            // Check offset is within page bounds
            if (offset >= page_size)
            {
                return false;
            }
            // Check that offset + length doesn't overflow page
            if (offset + length > page_size)
            {
                return false;
            }
            // Check minimum offset (must be after standard page header)
            if (offset < sizeof(PageHeader))
            {
                return false;
            }
            return true;
        }
    };
#pragma pack(pop)

// Tuple header - metadata for each tuple (MGA Phase 3: Version Chains)
#pragma pack(push, 1)
    struct TupleHeader
    {
        // Transaction info (16 bytes)
        uint64_t xmin; // Transaction ID that inserted this tuple
        uint64_t xmax; // Transaction ID that deleted/updated this tuple (or 0)

        // Version chain (8 bytes)
        uint64_t next_version_tid; // TID of next version (0 if latest)
                                   // Format: (page_id << 32) | (item_id << 16)

        // Tuple metadata (8 bytes)
        uint32_t ctid_page; // Current tuple ID: page number
        uint16_t ctid_item; // Current tuple ID: item number
        uint16_t infomask;  // Tuple state flags (replaces old 'flags')

        // Null bitmap (4 bytes)
        uint16_t null_bitmap_offset; // Offset to null bitmap (0 if no nulls)
        uint16_t padding;            // Alignment padding

        // Total: 36 bytes (up from 18 bytes)

        // Infomask flags (PostgreSQL-compatible)
        static constexpr uint16_t HEAP_HAS_NULLS = 0x0001;
        static constexpr uint16_t HEAP_XMIN_COMMITTED = 0x0002;
        static constexpr uint16_t HEAP_XMIN_INVALID = 0x0004;
        static constexpr uint16_t HEAP_XMAX_COMMITTED = 0x0008;
        static constexpr uint16_t HEAP_XMAX_INVALID = 0x0010;
        static constexpr uint16_t HEAP_XMAX_IS_MULTI = 0x0020; // Future: Multi-XID
        static constexpr uint16_t HEAP_UPDATED = 0x0040;       // Tuple was updated
        static constexpr uint16_t HEAP_MOVED = 0x0080;         // Tuple moved to new page
        static constexpr uint16_t HEAP_XMIN_FROZEN = 0x0100;   // xmin is frozen (FROZEN_XID)
        static constexpr uint16_t HEAP_HOT_UPDATED = 0x0200;   // HOT update (no index update needed)

        // Backward compatibility
        static constexpr uint16_t FLAG_HAS_NULLS = HEAP_HAS_NULLS;
        static constexpr uint16_t FLAG_DELETED = HEAP_XMAX_COMMITTED;

        [[nodiscard]] auto hasNulls() const -> bool
        {
            return (infomask & HEAP_HAS_NULLS) != 0;
        }

        [[nodiscard]] auto isDeleted() const -> bool
        {
            // Deleted if xmax is committed and not an update
            return (infomask & HEAP_XMAX_COMMITTED) != 0 && (infomask & HEAP_UPDATED) == 0;
        }

        [[nodiscard]] auto isUpdated() const -> bool
        {
            return (infomask & HEAP_UPDATED) != 0;
        }

        [[nodiscard]] auto hasNextVersion() const -> bool
        {
            return next_version_tid != 0;
        }

        // Get TID of this tuple
        [[nodiscard]] auto getTID() const -> uint64_t
        {
            return (static_cast<uint64_t>(ctid_page) << 32) |
                   (static_cast<uint64_t>(ctid_item) << 16);
        }

        // Set TID of this tuple
        void setTID(uint32_t page_id, uint16_t item_id)
        {
            ctid_page = page_id;
            ctid_item = item_id;
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
        auto getTupleDetoasted(uint16_t item_id, std::vector<uint8_t> *buffer, uint64_t xmin,
                               ErrorContext *ctx = nullptr) -> Status;

        // Mark tuple as deleted (and clean up TOAST if present)
        auto deleteTuple(uint16_t item_id, uint64_t xmax, ErrorContext *ctx = nullptr) -> Status;

        // Update tuple (MGA Phase 3: Version Chains)
        // Creates a new version and links it to the old version
        // Returns the new tuple's item_id
        auto updateTuple(uint16_t old_item_id, const uint8_t *new_tuple_data,
                         uint32_t new_tuple_size, uint64_t xmax, uint64_t new_xmin,
                         uint16_t *new_item_id_out, ErrorContext *ctx = nullptr) -> Status;

        // Find visible version of tuple by traversing version chain
        // REQUIRES: snapshot must not be null - cross-page pins are registered with it (Option 3:
        // MVCC Snapshot) Snapshot owns all cross-page pins and cleans them up on transaction
        // commit/rollback
        auto findVisibleVersion(uint16_t item_id, uint64_t snapshot_xid, const uint8_t **data_out,
                                uint32_t *size_out, TransactionManager::Snapshot *snapshot,
                                ErrorContext *ctx = nullptr) -> Status;

        // Check if there's enough space for a tuple
        [[nodiscard]] auto hasFreeSpace(uint32_t tuple_size) const -> bool;

        // Get number of tuples (including deleted)
        [[nodiscard]] auto getItemCount() const -> uint16_t;

        // Get free space available
        [[nodiscard]] auto getFreeSpace() const -> uint32_t;

        // Validate page structure
        auto validate(ErrorContext *ctx = nullptr) const -> Status;

        // Freeze old tuples to prevent XID wraparound
        // Sets xmin to FROZEN_XID for tuples older than freeze_limit
        // Returns number of tuples frozen
        auto freezeTuples(uint64_t freeze_limit, uint32_t *frozen_count_out,
                          ErrorContext *ctx = nullptr) -> Status;

        // Physical tuple removal for garbage collection
        // Mark tuple as LP_UNUSED (permanently dead, can be overwritten)
        // Returns Status::OK if tuple was marked unused
        auto markTupleUnused(uint16_t item_id, ErrorContext *ctx = nullptr) -> Status;

        // Defragment page by compacting tuples and reclaiming free space
        // Moves live tuples together, updates item pointers, reclaims holes
        // Returns number of bytes reclaimed
        auto defragmentPage(uint32_t *bytes_reclaimed_out, ErrorContext *ctx = nullptr) -> Status;

        // Prune dead tuples from page (GC helper)
        // Marks garbage tuples as LP_UNUSED based on provided OIT
        // Returns number of tuples pruned
        auto prunePage(uint64_t oit, uint32_t *tuples_pruned_out, uint32_t *space_reclaimed_out,
                       ErrorContext *ctx = nullptr) -> Status;

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
        ID table_id_;             // Table ID for TOAST operations

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
