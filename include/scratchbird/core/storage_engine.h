#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/uuidv7.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>


    namespace scratchbird::core
    {

        using ID = UuidV7Bytes;

        // Forward declarations
        class Database;
        class BufferPool;
        class PageManager;
        class CatalogManager;
        class HeapPage;
        class StorageEngine;
        class ToastManager;
        struct ErrorContext;
        struct TableInfo;

        // Tuple data structure
        struct Tuple
        {
            const uint8_t *data; // Pointer to tuple data
            uint32_t data_size;  // Size of tuple data
            uint64_t tid;        // Tuple ID (page_id << 16 | item_id)
            uint16_t item_id;    // Item ID within the page
            uint32_t page_id;    // Page containing this tuple
        };

        // Iterator for sequential scan
        class HeapScanIterator
        {
        public:
            HeapScanIterator(Database *db, StorageEngine *engine, const ID &table_id,
                             uint32_t start_page);
            ~HeapScanIterator();

            // Move to next tuple
            auto next(Tuple *tuple_out, ErrorContext *ctx = nullptr) -> Status;

            // Check if scan is complete
            [[nodiscard]] auto isDone() const -> bool
            {
                return done_;
            }

        private:
            Database *db_;
            StorageEngine *engine_;
            ID table_id_;
            uint32_t current_page_;
            uint16_t current_item_;
            uint32_t last_page_;
            bool done_;

            // Current page data
            uint8_t *page_data_ = nullptr;

            // Load next page
            auto loadPage(uint32_t page_id, ErrorContext *ctx) -> Status;
        };

        // Iterator for index scan
        class IndexScanIterator
        {
        public:
            IndexScanIterator(Database *db, StorageEngine *engine, const ID &index_id);
            ~IndexScanIterator();

            // Move to the first entry >= key
            auto seek(const std::vector<uint8_t> &key, ErrorContext *ctx = nullptr) -> Status;

            // Move to the next entry
            auto next(Tuple *tuple_out, ErrorContext *ctx = nullptr) -> Status;

            // Check if scan is complete
            [[nodiscard]] auto isDone() const -> bool
            {
                return done_;
            }

        private:
            Database *db_;
            StorageEngine *engine_;
            ID index_id_;
            bool done_;

            // B-tree traversal state
            std::vector<uint64_t> current_tuple_ids_;  // Tuple IDs from current key
            size_t current_tuple_index_;                 // Index within current_tuple_ids_
            std::vector<uint8_t> current_key_;          // Current key being scanned
            bool initialized_;                           // Whether seek() has been called
        };

        // Storage engine for heap storage
        class StorageEngine
        {
        public:
            explicit StorageEngine(Database *db);
            ~StorageEngine();

            // Insert a tuple into a table
            // Returns the tuple ID (page_id, item_id) on success
            auto insertTuple(const ID &table_id, const uint8_t *tuple_data, uint32_t tuple_size,
                                uint32_t *page_id_out, uint16_t *item_id_out,
                                ErrorContext *ctx = nullptr) -> Status;

            // Get a specific tuple by ID
            auto getTuple(uint32_t page_id, uint16_t item_id, Tuple *tuple_out,
                             ErrorContext *ctx = nullptr) -> Status;

            // Delete a tuple (mark as deleted)
            auto deleteTuple(const ID &table_id, uint32_t page_id, uint16_t item_id,
                           ErrorContext *ctx = nullptr) -> Status;

            // Delete a tuple by TID
            auto deleteTuple(const ID &table_id, uint64_t tid, uint64_t xmax,
                                ErrorContext *ctx = nullptr) -> Status;

            // Update a tuple (MGA Phase 3: Version Chains)
            // Creates a new version and links it to the old version
            auto updateTuple(const ID &table_id, uint32_t page_id, uint16_t item_id,
                           const uint8_t *new_tuple_data, uint32_t new_tuple_size,
                           uint32_t *new_page_id_out, uint16_t *new_item_id_out,
                           ErrorContext *ctx = nullptr) -> Status;

            // Create a sequential scan iterator
            auto createScan(const ID &table_id,
                                                          ErrorContext *ctx = nullptr) -> std::unique_ptr<HeapScanIterator>;

            // Create an index scan iterator
            auto createIndexScan(const ID &index_id,
                                                                 ErrorContext *ctx = nullptr) -> std::unique_ptr<IndexScanIterator>;

            // Create a sequential scan iterator with visibility
            auto sequentialScan(const ID &table_id,
                                                              const std::vector<uint32_t> &columns,
                                                              uint64_t xmin,
                                                              ErrorContext *ctx = nullptr) -> std::unique_ptr<HeapScanIterator>;

            // Check if a tuple is visible (basic visibility for single connection)
            auto isVisible(uint64_t xmin, uint64_t xmax, uint64_t current_xid) -> bool;

            // Get current transaction ID from TransactionManager
            [[nodiscard]] auto getCurrentXid() const -> uint64_t;

        private:
            Database *db_;
            BufferPool *buffer_pool_;
            PageManager *page_manager_;
            CatalogManager *catalog_manager_;

            // ToastManager cache (per-table)
            std::unordered_map<ID, std::unique_ptr<ToastManager>> toast_managers_;
            std::mutex toast_mutex_; // Protects toast_managers_ map

            // Find a page with free space for a tuple
            auto findFreePage(const ID &table_id, uint32_t tuple_size, uint32_t *page_id_out,
                                  ErrorContext *ctx) -> Status;

            // Allocate a new heap page for a table
            auto allocateHeapPage(const ID &table_id, uint32_t *page_id_out, ErrorContext *ctx) -> Status;

            // Get or create ToastManager for a table
            auto getOrCreateToastManager(const ID &table_id, ErrorContext *ctx) -> ToastManager*;

            // Lock management helpers
            auto acquireTupleLock(const ID &table_id, uint32_t page_id, uint16_t item_id,
                                 uint32_t proc_id, bool wait, ErrorContext *ctx) -> Status;
            auto releaseTupleLock(const ID &table_id, uint32_t page_id, uint16_t item_id,
                                 uint32_t proc_id, ErrorContext *ctx) -> Status;

            // Index update helper for cross-page relocations
            auto updateIndexesForRelocation(const ID &table_id,
                                          uint32_t old_page_id, uint16_t old_item_id,
                                          uint32_t new_page_id, uint16_t new_item_id,
                                          const uint8_t *tuple_data, uint32_t tuple_size,
                                          ErrorContext *ctx) -> Status;
        };

    } // namespace scratchbird::core
