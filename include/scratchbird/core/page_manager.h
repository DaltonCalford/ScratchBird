#pragma once

#include <cstdint>
#include <vector>
#include <mutex>
#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/error_context.h"

namespace scratchbird::core
{

    // Forward declarations
    class Database;
    class BufferPool;

    /**
     * Page Manager - Handles page allocation and free space tracking
     *
     * The Free Space Map (FSM) is stored on page 2 and uses a bitmap
     * to track allocated/free pages. Each bit represents one page:
     * 0 = free, 1 = allocated
     */
    class PageManager
    {
    public:
        PageManager(Database *db, uint32_t page_size);
        ~PageManager();

        // Initialize FSM for a new database
        auto initialize(ErrorContext *ctx = nullptr) -> Status;

        // Load FSM from existing database
        auto load(ErrorContext *ctx = nullptr) -> Status;

        // Allocate a new page
        auto allocatePage(uint32_t &page_id, ErrorContext *ctx = nullptr) -> Status;

        // Free a page
        auto freePage(uint32_t page_id, ErrorContext *ctx = nullptr) -> Status;

        // Check if a page is allocated
        auto isAllocated(uint32_t page_id) const -> bool;

        // Get total number of pages
        auto totalPages() const -> uint32_t
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return total_pages_;
        }

        // Get number of free pages
        auto freePages() const -> uint32_t
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return free_pages_;
        }

        // Extend the database file
        auto extendFile(uint32_t num_pages, ErrorContext *ctx = nullptr) -> Status;

        // Flush FSM to disk
        auto flush(ErrorContext *ctx = nullptr) -> Status;

        // Reconstruct FSM from actual page state (MGA-style recovery)
        auto reconstructFromPages(ErrorContext *ctx = nullptr) -> Status;

    protected:
        Database *db_;       // Database instance
        uint32_t page_size_; // Page size

    private:
        uint32_t total_pages_;        // Total pages in database
        uint32_t free_pages_;         // Number of free pages
        std::vector<uint8_t> bitmap_; // Allocation bitmap
        bool dirty_;                  // FSM needs flush
        mutable std::mutex mutex_;    // Thread safety (future)

        // Helper methods
        void setBit(uint32_t page_id, bool allocated);
        auto getBit(uint32_t page_id) const -> bool;
        auto findFreePage() const -> uint32_t;
        void buildFsmPageBuffer(uint8_t *buffer);

        // FSM page structure
        struct FSMPage
        {
            PageHeader header;
            uint32_t total_pages;
            uint32_t free_pages;
            uint32_t next_fsm_page; // For future expansion
            uint8_t bitmap[];       // Variable length bitmap
        };

        static constexpr uint32_t FSM_PAGE_ID = 2; // FSM is always page 2
    };

} // namespace scratchbird::core
