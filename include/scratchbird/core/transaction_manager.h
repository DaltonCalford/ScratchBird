#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <vector>


    namespace scratchbird::core
    {

        // Forward declarations
        class Database;
        class BufferPool;
        class PageManager;
        struct ErrorContext;

        // Transaction states
        enum class TransactionState : uint8_t
        {
            ACTIVE = 0,
            COMMITTED = 1,
            ABORTED = 2,
            PREPARED = 3, // For future 2PC support
        };

        // Transaction information
        struct TransactionInfo
        {
            uint64_t xid;
            TransactionState state;
            uint64_t start_time; // Microseconds since epoch
            uint64_t end_time;   // Microseconds since epoch (0 if active)
        };

// Transaction Inventory Page (TIP) format
// TIP pages track transaction states for MVCC visibility
#pragma pack(push, 1)
        struct TIPPageHeader
        {
            PageHeader page_header;    // Standard page header
            uint64_t min_xid;          // Minimum XID in this page
            uint64_t max_xid;          // Maximum XID in this page
            uint32_t num_transactions; // Number of transactions in this page
            uint32_t next_tip_page;    // Next TIP page ID (0 if last)
            uint8_t reserved[20];      // Reserved for future use
        };

        // Each transaction entry in TIP page
        struct TIPEntry
        {
            uint64_t xid;         // Transaction ID
            uint8_t state;        // TransactionState
            uint8_t flags;        // Reserved flags
            uint16_t reserved;    // Alignment padding
            uint64_t commit_time; // Commit/abort timestamp
        };
#pragma pack(pop)

        // Transaction Manager - handles transaction lifecycle and visibility
        class TransactionManager
        {
        public:
            explicit TransactionManager(Database *db);
            ~TransactionManager();

            // Initialize transaction subsystem
            auto initialize(ErrorContext *ctx = nullptr) -> Status;

            // Load existing transaction state from disk
            auto load(ErrorContext *ctx = nullptr) -> Status;

            // Begin a new transaction
            auto beginTransaction(uint32_t proc_id, uint64_t &xid_out, ErrorContext *ctx = nullptr) -> Status;

            // Commit a transaction
            auto commitTransaction(uint32_t proc_id, uint64_t xid, ErrorContext *ctx = nullptr) -> Status;

            // Rollback a transaction
            auto rollbackTransaction(uint32_t proc_id, uint64_t xid, ErrorContext *ctx = nullptr) -> Status;

            // Get transaction state
            auto getTransactionState(uint64_t xid, TransactionState &state_out,
                                         ErrorContext *ctx = nullptr) -> Status;

            // Check if a transaction is visible to another transaction
            auto isTransactionVisible(uint64_t xid, uint64_t snapshot_xid) -> bool;

            // Get current transaction ID (for read-only operations)
            auto getCurrentXid() const -> uint64_t
            {
                std::lock_guard<std::mutex> lock(mutex_);
                return next_xid_;
            }

            // Get active transaction for a specific backend
            auto getBackendXid(uint32_t proc_id) const -> uint64_t;

            // Snapshot isolation support (future)
            struct Snapshot
            {
                uint64_t xmin;                     // Oldest active XID
                uint64_t xmax;                     // Next XID to be assigned
                std::vector<uint64_t> active_xids; // Active XIDs at snapshot time
            };

            // Get current snapshot (for future MVCC)
            auto getSnapshot(Snapshot &snapshot_out, ErrorContext *ctx = nullptr) -> Status;

        private:
            Database *db_;
            BufferPool *buffer_pool_;
            PageManager *page_manager_;

            // Transaction state
            uint64_t next_xid_ = 100;    // Next XID to allocate (start at 100)
            uint32_t tip_root_page_ = 0; // Root TIP page ID

            // In-memory cache of recent transactions
            std::unordered_map<uint64_t, TransactionState> transaction_cache_;
            mutable std::mutex mutex_; // Thread safety for future

            // Special transaction IDs
            static constexpr uint64_t INVALID_XID = 0;
            static constexpr uint64_t BOOTSTRAP_XID = 1;
            static constexpr uint64_t FROZEN_XID = 2;

            // TIP page management - calculate based on actual page size
            [[nodiscard]] auto getTipEntriesPerPage() const -> uint32_t;

            // Helper methods
            auto loadTipPage(uint32_t page_id, ErrorContext *ctx) -> Status;
            auto allocateTipPage(uint32_t &page_id_out, ErrorContext *ctx) -> Status;
            auto writeTipEntry(uint64_t xid, TransactionState state, ErrorContext *ctx) -> Status;
            auto findTipEntry(uint64_t xid, TIPEntry &entry_out, ErrorContext *ctx) -> Status;
            auto flushTransactionState(ErrorContext *ctx) -> Status;
        };

    } // namespace scratchbird::core
