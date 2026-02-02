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

#include <cstdint>
#include <vector>
#include <mutex>
#include <unordered_map>
#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/gpid.h"

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

        // Allocate a new page (legacy API - uses tablespace 0)
        auto allocatePage(uint32_t &page_id, ErrorContext *ctx = nullptr) -> Status;

        // Free a page (legacy API - uses tablespace 0)
        auto freePage(uint32_t page_id, ErrorContext *ctx = nullptr) -> Status;

        // Check if a page is allocated (legacy API - uses tablespace 0)
        auto isAllocated(uint32_t page_id) const -> bool;

        // === NEW: GPID-based API (Phase 1, Task 1.2.2) ===

        /**
         * allocatePageInTablespace - Allocate a page in a specific tablespace
         *
         * @param tablespace_id Tablespace ID (0 = primary, 1-65535 = custom)
         * @param gpid_out Output GPID of allocated page
         * @param allow_uuid_mismatch If true, skip database_uuid mismatch failure
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         *
         * Note: For tablespace 0 (primary), this is equivalent to allocatePage()
         *       but returns a GPID instead of uint32_t page_id.
         */
        auto allocatePageInTablespace(uint16_t tablespace_id, GPID *gpid_out,
                                     ErrorContext *ctx = nullptr) -> Status;

        /**
         * freePageGlobal - Free a page identified by GPID
         *
         * @param gpid Global Page ID of page to free
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         */
        auto freePageGlobal(GPID gpid, ErrorContext *ctx = nullptr) -> Status;

        /**
         * isAllocatedGlobal - Check if a page is allocated (GPID version)
         *
         * @param gpid Global Page ID to check
         * @return true if allocated, false if free
         */
        auto isAllocatedGlobal(GPID gpid) const -> bool;

        // === NEW: Tablespace File Management (Phase 1, Task 1.3) ===

        /**
         * createTablespace - Create a new tablespace file
         *
         * @param tablespace_id Tablespace ID (1-65535, 0 = primary reserved)
         * @param name Tablespace name (max 31 chars)
         * @param path Absolute path to .sbts file to create
         * @param config Tablespace configuration (autoextend, prealloc, etc.)
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         *
         * Performs:
         * 1. Validates inputs (tablespace_id, name, path)
         * 2. Creates .sbts file at specified path (O_RDWR | O_CREAT | O_EXCL)
         * 3. Initializes TablespaceHeader (page 0) with metadata
         * 4. Initializes tablespace FSM (page 1)
         * 5. Preallocates pages if config.prealloc_pages > 0
         * 6. Opens file and registers in Database
         *
         * Thread-safe: Acquires Database::tablespace_mutex_ during registration.
         * Note: Catalog insertion deferred to CatalogManager (caller responsible).
         */
        auto createTablespace(uint16_t tablespace_id, const std::string &name,
                             const std::string &path, const struct TablespaceConfig &config,
                             ErrorContext *ctx = nullptr) -> Status;

        /**
         * openTablespace - Open an existing tablespace file and validate header
         *
         * @param tablespace_id Tablespace ID (1-65535, 0 = primary reserved)
         * @param path Absolute path to .sbts file
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         *
         * Performs:
         * 1. Opens .sbts file at specified path
         * 2. Reads and validates TablespaceHeader (page 0)
         * 3. Validates database_uuid matches current database (fails unless allow_uuid_mismatch)
         * 4. Validates page_size matches (errors if mismatch)
         * 5. Loads tablespace FSM into memory (page 1)
         * 6. Registers file descriptor in Database::tablespace_fds_
         *
         * Thread-safe: Acquires Database::tablespace_mutex_ during registration.
         */
        auto openTablespace(uint16_t tablespace_id, const std::string &path,
                           bool allow_uuid_mismatch,
                           ErrorContext *ctx = nullptr) -> Status;

        /**
         * closeTablespace - Close a tablespace file and cleanup resources
         *
         * @param tablespace_id Tablespace ID (1-65535, 0 = primary reserved)
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         *
         * Performs:
         * 1. Validates tablespace_id != 0 (primary cannot be closed)
         * 2. Flushes dirty FSM pages for this tablespace (deferred to Task 1.3.5)
         * 3. Syncs tablespace file to disk
         * 4. Unregisters and closes file descriptor
         *
         * Thread-safe: Acquires Database::tablespace_mutex_ during unregistration.
         * Note: Caller must ensure no active transactions using this tablespace.
         */
        auto closeTablespace(uint16_t tablespace_id, ErrorContext *ctx = nullptr) -> Status;

        /**
         * extendTablespace - Extend a tablespace file when space is exhausted
         *
         * @param tablespace_id Tablespace ID (1-65535, 0 = primary reserved)
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         *
         * Performs:
         * 1. Calculates extension size from tablespace's autoextend_size_mb
         * 2. Checks MAXSIZE limit before extending (returns error if exceeded)
         * 3. Uses ftruncate() to grow the file
         * 4. Initializes new pages as free in FSM bitmap
         * 5. Updates TablespaceHeader.total_pages and free_pages
         * 6. Marks FSM as dirty for flush
         *
         * Thread-safe: Acquires tablespace_fsm_mutex_ during FSM update.
         * Note: Called automatically by allocatePageInTablespace when no free pages.
         */
        auto extendTablespace(uint16_t tablespace_id, ErrorContext *ctx = nullptr) -> Status;

        /**
         * preallocatePages - Preallocate pages during tablespace creation
         *
         * @param tablespace_id Tablespace ID (1-65535, 0 = primary reserved)
         * @param num_pages Number of pages to preallocate
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         *
         * Performs:
         * 1. Validates inputs (tablespace_id, num_pages)
         * 2. Gets file descriptor for tablespace
         * 3. Reads current TablespaceHeader to get current size
         * 4. Calculates new file size (current + num_pages)
         * 5. Uses fallocate()/posix_fallocate() for efficient allocation (Linux)
         * 6. Fallback: Writes zeroed pages in 10MB batches (portability)
         * 7. Updates FSM bitmap to mark new pages as free
         * 8. Updates TablespaceHeader.total_pages and free_pages
         * 9. Syncs changes to disk
         *
         * Thread-safe: Acquires tablespace_fsm_mutex_ during FSM update.
         * Note: Called from createTablespace() if prealloc_pages > 0.
         *       Uses efficient fallocate() on Linux, falls back to manual zeroing.
         */
        auto preallocatePages(uint16_t tablespace_id, uint32_t num_pages,
                             ErrorContext *ctx = nullptr) -> Status;

        /**
         * updateTablespaceHeader - Update tablespace header (page 0) with new settings
         *
         * WP-2 CAT-M4/M5: Persist autoextend settings and name to tablespace file
         *
         * @param tablespace_id Tablespace ID (1-65535, 0 = primary reserved)
         * @param name New tablespace name (nullptr = no change)
         * @param autoextend_enabled New autoextend setting (nullptr = no change)
         * @param autoextend_size_mb New autoextend size in MB (nullptr = no change)
         * @param max_size_mb New max size in MB (nullptr = no change)
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         */
        auto updateTablespaceHeader(uint16_t tablespace_id,
                                   const char* name,
                                   const uint32_t* autoextend_enabled,
                                   const uint32_t* autoextend_size_mb,
                                   const uint64_t* max_size_mb,
                                   ErrorContext *ctx = nullptr) -> Status;

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

        // === NEW: Tablespace Metrics (Phase 3, Task 3.1.5) ===

        /**
         * TablespaceMetrics - Extension frequency and timing metrics
         *
         * Tracks statistics about tablespace extensions for monitoring and diagnostics.
         * Returned by getTablespaceMetrics() for external monitoring tools.
         */
        struct TablespaceMetrics
        {
            uint64_t extension_count = 0;          // Total number of extensions
            uint64_t total_pages_added = 0;        // Total pages added across all extensions
            uint64_t last_extension_time = 0;      // Timestamp of last extension (microseconds)
            uint64_t first_extension_time = 0;     // Timestamp of first extension (microseconds)
            uint64_t failed_extension_count = 0;   // Number of failed extension attempts
        };

        /**
         * getTablespaceMetrics - Get extension metrics for a tablespace
         *
         * @param tablespace_id Tablespace ID (1-65535)
         * @param metrics_out Output structure to receive metrics
         * @return true if metrics found, false if tablespace_id not found or no extensions yet
         *
         * Thread-safe: Acquires tablespace_fsm_mutex_ for reading.
         * Note: Returns false if tablespace has never been extended (no metrics exist).
         */
        auto getTablespaceMetrics(uint16_t tablespace_id, TablespaceMetrics *metrics_out) const -> bool;

        // === NEW: Page Enumeration API (Phase 5, Task 5.1.1) ===

        /**
         * getAllocatedPages - Get all allocated pages in a tablespace
         *
         * @param tablespace_id Tablespace ID (0 = primary, 1-65535 = custom)
         * @param pages_out Output vector to receive GPIDs of allocated pages
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         *
         * Scans the FSM bitmap for the specified tablespace and returns a list
         * of all allocated page GPIDs. This is used during table migration to
         * enumerate all pages that need to be examined.
         *
         * Thread-safe: Acquires tablespace_fsm_mutex_ (or mutex_ for primary).
         * Performance: O(total_pages) scan of bitmap, typically fast for SSDs.
         *
         * Phase 5 Task 5.1.1
         */
        auto getAllocatedPages(uint16_t tablespace_id,
                              std::vector<GPID> &pages_out,
                              ErrorContext *ctx = nullptr) -> Status;

        /**
         * getTablespaceTotalPages - Get total pages for a tablespace
         *
         * @param tablespace_id Tablespace ID (0 = primary, 1-65535 = custom)
         * @param total_pages_out Output total page count
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         */
        auto getTablespaceTotalPages(uint16_t tablespace_id,
                                     uint32_t *total_pages_out,
                                     ErrorContext *ctx = nullptr) const -> Status;

    protected:
        Database *db_;       // Database instance
        uint32_t page_size_; // Page size

    private:
        // Primary database FSM (tablespace 0)
        uint32_t total_pages_;        // Total pages in database
        uint32_t free_pages_;         // Number of free pages
        std::vector<uint8_t> bitmap_; // Allocation bitmap
        bool dirty_;                  // FSM needs flush
        mutable std::mutex mutex_;    // Thread safety (future)

        // Eager FSM flush counters (protected by mutex_)
        uint32_t alloc_counter_ = 0;  // Count allocations for periodic FSM flush
        uint32_t free_counter_ = 0;   // Count frees for periodic FSM flush

        // === PHASE 1, TASK 1.3.5: Tablespace-specific FSM ===
        /**
         * TablespaceFSM - In-memory Free Space Map for a tablespace
         *
         * Each tablespace has its own FSM tracking free pages within that tablespace.
         * The FSM is stored on page 1 of the tablespace file.
         */
        struct TablespaceFSM
        {
            uint32_t total_pages = 0;        // Total pages in tablespace
            uint32_t free_pages = 0;         // Number of free pages
            std::vector<uint8_t> bitmap;     // Allocation bitmap (0=free, 1=allocated)
            bool dirty = false;              // FSM needs flush
        };

        // Map of tablespace_id -> FSM (for custom tablespaces 1-65535)
        std::unordered_map<uint16_t, TablespaceFSM> tablespace_fsms_;
        mutable std::mutex tablespace_fsm_mutex_; // Protects tablespace_fsms_

        // === PHASE 3, TASK 3.1.2: Tablespace Extension Mutex ===
        /**
         * tablespace_extend_mutex_ - Prevents concurrent tablespace extensions
         *
         * This mutex is acquired before checking if extension is needed and released
         * after extension completes. This ensures only one thread extends a tablespace
         * at a time, preventing races where multiple threads try to extend simultaneously.
         */
        mutable std::mutex tablespace_extend_mutex_; // Protects tablespace extension operations

        // === PHASE 3, TASK 3.1.5: Tablespace Extension Metrics ===
        // Map of tablespace_id -> metrics (for custom tablespaces 1-65535)
        // Protected by tablespace_fsm_mutex_ (shared lock for simplicity)
        std::unordered_map<uint16_t, TablespaceMetrics> tablespace_metrics_;

        // Helper methods
        void setBit(uint32_t page_id, bool allocated);
        auto getBit(uint32_t page_id) const -> bool;
        auto findFreePage() const -> uint32_t;
        void buildFsmPageBuffer(uint8_t *buffer);

        /**
         * flushUnlocked - Internal flush method (caller must hold mutex_)
         *
         * This is used by allocatePage() and freePage() for periodic FSM flushing
         * when the caller already holds the mutex.
         */
        auto flushUnlocked(ErrorContext *ctx = nullptr) -> Status;

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
