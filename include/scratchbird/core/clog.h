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
    //
    // IMPORTANT: This enum MUST have exactly 4 values (0-3) to fit in 2-bit storage.
    // The CLOG uses 2 bits per transaction, allowing 4 possible states (2^2 = 4).
    // Static assertions in clog.cpp enforce this constraint at compile time.
    //
    // DO NOT ADD MORE VALUES without expanding storage to 3 bits and implementing
    // database version migration. See clog.cpp for detailed instructions.
    enum class ClogStatus : uint8_t
    {
        IN_PROGRESS = 0,  // 00 - Transaction still active
        COMMITTED = 1,    // 01 - Transaction committed
        ABORTED = 2,      // 10 - Transaction aborted
        PREPARED = 3      // 11 - Transaction prepared (2PC limbo)
    };

    // CLOG page structure
    // Each page stores status for 65,536 transactions (2 bits each)
#pragma pack(push, 1)
    struct ClogPageHeader
    {
        PageHeader page_header;  // Standard 64-byte header
        uint64_t base_xid;       // First XID in this page (page_id * 65536)
        uint32_t next_clog_page; // Next CLOG page (0 if last)
        uint32_t reserved;       // Alignment
        // Status data follows: 16,384 bytes (65,536 transactions * 2 bits)
    };
#pragma pack(pop)

    // CLOG Manager - manages commit log pages
    class Clog
    {
    public:
        explicit Clog(Database *db);
        ~Clog();

        // Initialize CLOG subsystem
        Status initialize(ErrorContext *ctx = nullptr);

        // Set transaction status
        Status setStatus(uint64_t xid, ClogStatus status, ErrorContext *ctx = nullptr);

        // Get transaction status
        Status getStatus(uint64_t xid, ClogStatus *status_out, ErrorContext *ctx = nullptr);

        // Get CLOG root page ID
        uint32_t getRootPage() const
        {
            return clog_root_page_;
        }

        // Extend CLOG to accommodate new XID
        Status extendClog(uint64_t xid, ErrorContext *ctx = nullptr);

        // Statistics
        struct ClogStats
        {
            uint64_t total_transactions; // Total XIDs stored
            uint32_t num_pages;          // Number of CLOG pages
            uint64_t space_used_bytes;   // Total space used
            uint64_t space_saved_bytes;  // Space saved vs TIP
        };

        void getStatistics(ClogStats *stats_out) const;

    private:
        Database *db_;
        BufferPool *buffer_pool_;
        uint32_t clog_root_page_;
        mutable std::mutex mutex_;

        // CLOG constants
        static constexpr uint32_t BITS_PER_XID = 2;

        // Dynamic capacity calculations based on page size
        // Each XID uses 2 bits (4 states: ACTIVE, COMMITTED, ABORTED, PREPARED)
        // Formula: XIDS_PER_PAGE = (page_size - header_size) * 8 / 2 = (page_size - header_size) * 4
        uint32_t getXidsPerPage() const;

        uint32_t getStatusBytesPerPage() const;

        // Calculate which page contains an XID
        uint32_t getPageForXid(uint64_t xid) const
        {
            return clog_root_page_ + static_cast<uint32_t>(xid / getXidsPerPage());
        }

        // Calculate offset within page for an XID
        uint32_t getOffsetInPage(uint64_t xid) const
        {
            return static_cast<uint32_t>(xid % getXidsPerPage());
        }

        // Encode/decode 2-bit status in byte array
        void setStatusBits(uint8_t *data, uint32_t offset, ClogStatus status);
        ClogStatus getStatusBits(const uint8_t *data, uint32_t offset) const;

        // Allocate a new CLOG page
        Status allocateClogPage(uint32_t page_id, uint64_t base_xid, ErrorContext *ctx);
    };

} // namespace scratchbird::core
