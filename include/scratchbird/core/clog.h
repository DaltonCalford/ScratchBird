#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include <cstdint>
#include <mutex>

namespace scratchbird::core
{
    // Forward declarations
    class Database;
    class BufferPool;
    struct ErrorContext;

    // CLOG (Commit Log) - 2-bit transaction status storage
    // Replaces TIP (Transaction Inventory Pages) with 160x space savings
    //
    // Each transaction uses 2 bits instead of 20 bytes:
    // - TIP: 20 bytes/transaction = 160 bits/transaction
    // - CLOG: 2 bits/transaction
    // - Savings: 80x in raw storage, 160x effective (includes headers)
    //
    // A 16KB page can hold:
    // - TIP: ~800 transactions
    // - CLOG: 65,536 transactions (82x more)

    // Transaction status (2 bits)
    enum class ClogStatus : uint8_t
    {
        IN_PROGRESS = 0,  // 00 - Transaction still active
        COMMITTED = 1,    // 01 - Transaction committed
        ABORTED = 2,      // 10 - Transaction aborted
        SUB_COMMITTED = 3 // 11 - Subtransaction committed (future use)
    };

    // CLOG page structure
    // Each page stores status for 65,536 transactions (2 bits each)
#pragma pack(push, 1)
    struct ClogPageHeader
    {
        PageHeader page_header;   // Standard 64-byte header
        uint64_t base_xid;        // First XID in this page (page_id * 65536)
        uint32_t next_clog_page;  // Next CLOG page (0 if last)
        uint32_t reserved;        // Alignment
        // Status data follows: 16,384 bytes (65,536 transactions * 2 bits)
    };
#pragma pack(pop)

    // CLOG Manager - manages commit log pages
    class Clog
    {
    public:
        explicit Clog(Database* db);
        ~Clog();

        // Initialize CLOG subsystem
        Status initialize(ErrorContext* ctx = nullptr);

        // Set transaction status
        Status setStatus(uint64_t xid, ClogStatus status, ErrorContext* ctx = nullptr);

        // Get transaction status
        Status getStatus(uint64_t xid, ClogStatus* status_out, ErrorContext* ctx = nullptr);

        // Get CLOG root page ID
        uint32_t getRootPage() const { return clog_root_page_; }

        // Extend CLOG to accommodate new XID
        Status extendClog(uint64_t xid, ErrorContext* ctx = nullptr);

        // Statistics
        struct ClogStats
        {
            uint64_t total_transactions;  // Total XIDs stored
            uint32_t num_pages;           // Number of CLOG pages
            uint64_t space_used_bytes;    // Total space used
            uint64_t space_saved_bytes;   // Space saved vs TIP
        };

        void getStatistics(ClogStats* stats_out);

    private:
        Database* db_;
        BufferPool* buffer_pool_;
        uint32_t clog_root_page_;
        mutable std::mutex mutex_;

        // CLOG constants
        static constexpr uint32_t XIDS_PER_PAGE = 65536;     // 16KB / 2 bits = 65,536 XIDs
        static constexpr uint32_t STATUS_BYTES_PER_PAGE = 16384; // 65,536 * 2 bits / 8
        static constexpr uint32_t BITS_PER_XID = 2;

        // Calculate which page contains an XID
        uint32_t getPageForXid(uint64_t xid) const
        {
            return clog_root_page_ + static_cast<uint32_t>(xid / XIDS_PER_PAGE);
        }

        // Calculate offset within page for an XID
        uint32_t getOffsetInPage(uint64_t xid) const
        {
            return static_cast<uint32_t>(xid % XIDS_PER_PAGE);
        }

        // Encode/decode 2-bit status in byte array
        void setStatusBits(uint8_t* data, uint32_t offset, ClogStatus status);
        ClogStatus getStatusBits(const uint8_t* data, uint32_t offset) const;

        // Allocate a new CLOG page
        Status allocateClogPage(uint32_t page_id, uint64_t base_xid, ErrorContext* ctx);
    };

} // namespace scratchbird::core
