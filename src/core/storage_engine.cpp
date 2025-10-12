#include "scratchbird/core/config.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/btree.h"
#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/toast.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/logger.h"
#include <cstring>
#include <new>


    namespace scratchbird::core
    {

        StorageEngine::StorageEngine(Database *db)
            : db_(db), buffer_pool_(db->buffer_pool()), page_manager_(db->page_manager()),
              catalog_manager_(db->catalog_manager())
        {
        }

        StorageEngine::~StorageEngine() = default;

        auto StorageEngine::insertTuple(const ID &table_id, const uint8_t *tuple_data,
                                           uint32_t tuple_size, uint32_t *page_id_out,
                                           uint16_t *item_id_out, ErrorContext *ctx) -> Status
        {
            // Find a page with free space
            uint32_t page_id;
            Status status = findFreePage(table_id, tuple_size, &page_id, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Pin the page
            void *page_buffer;
            status = buffer_pool_->pinPage(page_id, &page_buffer, ctx);
            auto *page_data = static_cast<uint8_t *>(page_buffer);
            if (status != Status::OK)
            {
                return status;
            }

            // Get or create ToastManager for this table
            ToastManager* toast_mgr = getOrCreateToastManager(table_id, ctx);
            // Note: toast_mgr can be nullptr if TOAST table doesn't exist
            // HeapPage will handle this gracefully by not TOASTing

            // Insert tuple with TOAST support
            HeapPage heap_page(page_data, db_->page_size(), toast_mgr, db_, table_id);
            uint16_t item_id;

            // Get current XID from connection context
            uint64_t current_xid = ConnectionContext::getCurrentTransactionId();
            if (current_xid == 0) {
                // No active connection context - use fallback XID
                current_xid = config::DEFAULT_INITIAL_XID;
            }

            status = heap_page.insertTuple(tuple_data, tuple_size, current_xid, &item_id, ctx);

            if (status == Status::OK)
            {
                // Mark page as dirty
                // Page will be marked dirty on unpin

                if (page_id_out != nullptr) {
                    *page_id_out = page_id;
}
                if (item_id_out != nullptr) {
                    *item_id_out = item_id;
}
            }

            // Unpin the page
            buffer_pool_->unpinPage(page_id, status == Status::OK, ctx);

            return status;
        }

        auto StorageEngine::getTuple(uint32_t page_id, uint16_t item_id, Tuple *tuple_out,
                                        ErrorContext *ctx) -> Status
        {
            // Pin the page
            void *page_buffer;
            Status status = buffer_pool_->pinPage(page_id, &page_buffer, ctx);
            auto *page_data = static_cast<uint8_t *>(page_buffer);
            if (status != Status::OK)
            {
                return status;
            }

            // Get tuple
            HeapPage heap_page(page_data, db_->page_size());
            const uint8_t *tuple_data;
            uint32_t tuple_size;

            status = heap_page.getTuple(item_id, &tuple_data, &tuple_size, ctx);

            if (status == Status::OK && (tuple_out != nullptr))
            {
                // Check visibility
                const auto *hdr = reinterpret_cast<const TupleHeader *>(tuple_data);
                if (!isVisible(hdr->xmin, hdr->xmax, getCurrentXid()))
                {
                    status = Status::NOT_FOUND;
                    SET_ERROR_CONTEXT(ctx, status, "Tuple not visible");
                }
                else
                {
                    // Set tuple data pointer (includes header for now)
                    tuple_out->data = tuple_data;
                    tuple_out->data_size = tuple_size;
                    tuple_out->item_id = item_id;
                    tuple_out->page_id = page_id;
                    tuple_out->tid = (static_cast<uint64_t>(page_id) << 16) | item_id;
                }
            }

            // Unpin the page
            buffer_pool_->unpinPage(page_id, status == Status::OK, ctx);

            // Cooperative GC hook - opportunistic cleanup
            if (db_->garbage_collector() != nullptr)
            {
                db_->garbage_collector()->processPageCooperative(page_id, ctx);
            }

            return status;
        }

        auto StorageEngine::deleteTuple(const ID &table_id, uint32_t page_id, uint16_t item_id,
                                       ErrorContext *ctx) -> Status
        {
            // Get proc_id from ConnectionContext (Phase 2 complete)
            int32_t proc_id_signed = ConnectionContext::getCurrentProcId();
            uint32_t proc_id = (proc_id_signed >= 0) ? static_cast<uint32_t>(proc_id_signed) : 0;

            // Acquire tuple lock (Phase 2.5 complete)
            bool wait = true; // TODO: Get from ConnectionContext::getWaitForLocks()
            Status lock_status = acquireTupleLock(table_id, page_id, item_id, proc_id, wait, ctx);
            if (lock_status != Status::OK) {
                return lock_status;
            }

            // Pin the page
            void *page_buffer;
            Status status = buffer_pool_->pinPage(page_id, &page_buffer, ctx);
            auto *page_data = static_cast<uint8_t *>(page_buffer);
            if (status != Status::OK)
            {
                return status;
            }

            // Delete tuple
            HeapPage heap_page(page_data, db_->page_size());

            // Get current XID from connection context
            uint64_t current_xid = ConnectionContext::getCurrentTransactionId();
            if (current_xid == 0) {
                // No active connection context - use fallback XID
                current_xid = config::DEFAULT_INITIAL_XID;
            }

            status = heap_page.deleteTuple(item_id, current_xid, ctx);

            if (status == Status::OK)
            {
                // Mark page as dirty for GC
                if (db_->garbage_collector() != nullptr)
                {
                    db_->garbage_collector()->markPageDirty(page_id);
                }
            }

            // Unpin the page
            buffer_pool_->unpinPage(page_id, status == Status::OK, ctx);

            // Future lock release:
            // releaseTupleLock(table_id, page_id, item_id, proc_id, ctx);
            // Note: Locks are normally held until transaction end, not released here

            return status;
        }

        auto StorageEngine::createScan(const ID &table_id,
                                                                     ErrorContext *ctx) -> std::unique_ptr<HeapScanIterator>
        {
            // For now, we don't need table info - just return a scanner
            // In a real system, we'd track heap pages per table in the catalog

            // For now, assume heap pages start after catalog pages
            // In a real system, we'd track this in the catalog
            uint32_t start_page = Config::getInstance().getUInt("storage", "heap_scan_start_page", 7);

            return std::unique_ptr<HeapScanIterator>(
                new (std::nothrow) HeapScanIterator(db_, this, table_id, start_page));
        }

        auto StorageEngine::isVisible(uint64_t xmin, uint64_t xmax, uint64_t current_xid) -> bool
        {
            // Use transaction manager for visibility if available
            if (db_->transaction_manager() != nullptr)
            {
                TransactionManager *tm = db_->transaction_manager();

                // VALIDATE XIDs FIRST - protect against corrupted tuple headers
                if (!tm->isXidInRange(xmin))
                {
                    // CORRUPTION LOGGING: Invalid xmin detected
                    LOG_ERROR(STORAGE, "Invalid xmin %lu in StorageEngine::isVisible", xmin);
                    return false; // Invalid xmin - tuple is invisible
                }

                if (xmax != 0 && !tm->isXidInRange(xmax))
                {
                    // CORRUPTION LOGGING: Invalid xmax detected
                    LOG_WARNING(STORAGE, "Invalid xmax %lu in StorageEngine::isVisible - treating as not deleted", xmax);
                    // Invalid xmax - treat as if not deleted
                    xmax = 0;
                }

                // Get current connection context to determine isolation level
                ConnectionContext* conn_ctx = ConnectionContext::getCurrent();

                // "See own changes" logic - transaction always sees its own modifications
                if (xmin == current_xid)
                {
                    // Created by current transaction - check if deleted by current transaction
                    if (xmax == current_xid)
                    {
                        return false; // Deleted by current transaction - not visible
                    }
                    return true; // Created by current transaction and not yet deleted - visible
                }

                // If no connection context, fall back to READ COMMITTED semantics
                if (conn_ctx == nullptr)
                {
                    // Check if creating transaction is visible
                    if (!tm->isTransactionVisible(xmin, current_xid))
                    {
                        return false;
                    }

                    // If deleted, check if deleting transaction is visible
                    if (xmax != 0 && tm->isTransactionVisible(xmax, current_xid))
                    {
                        return false;
                    }

                    return true;
                }

                // Isolation-level-aware visibility checking
                IsolationLevel iso_level = conn_ctx->getIsolationLevel();

                switch (iso_level)
                {
                    case IsolationLevel::READ_COMMITTED:
                    {
                        // READ COMMITTED: See latest committed data
                        // Check if creating transaction is visible
                        if (!tm->isTransactionVisible(xmin, current_xid))
                        {
                            return false;
                        }

                        // If deleted, check if deleting transaction is visible
                        if (xmax != 0 && tm->isTransactionVisible(xmax, current_xid))
                        {
                            return false;
                        }

                        return true;
                    }

                    case IsolationLevel::SNAPSHOT:
                    case IsolationLevel::SNAPSHOT_TABLE_STABILITY:
                    {
                        // SNAPSHOT: Use snapshot-based visibility
                        const TransactionManager::Snapshot* snapshot = conn_ctx->getSnapshot();

                        if (snapshot == nullptr)
                        {
                            LOG_WARNING(STORAGE, "SNAPSHOT isolation without snapshot - falling back to READ COMMITTED");
                            // Fallback to READ COMMITTED semantics
                            if (!tm->isTransactionVisible(xmin, current_xid))
                            {
                                return false;
                            }

                            if (xmax != 0 && tm->isTransactionVisible(xmax, current_xid))
                            {
                                return false;
                            }

                            return true;
                        }

                        // Check if creating transaction is visible in snapshot
                        if (!tm->isSnapshotVisible(xmin, snapshot))
                        {
                            return false;
                        }

                        // If deleted, check if deleting transaction is visible in snapshot
                        if (xmax != 0)
                        {
                            // Special case: deleted by current transaction
                            if (xmax == current_xid)
                            {
                                return false; // We deleted it - not visible
                            }

                            // Check if deletion is visible in snapshot
                            if (tm->isSnapshotVisible(xmax, snapshot))
                            {
                                return false; // Deletion committed before snapshot - not visible
                            }
                        }

                        return true;
                    }

                    case IsolationLevel::READ_COMMITTED_READ_CONSISTENCY:
                    {
                        // READ COMMITTED READ CONSISTENCY: Statement-level snapshot
                        // If statement snapshot exists, use it; otherwise fall back to READ COMMITTED
                        const TransactionManager::Snapshot* stmt_snapshot = conn_ctx->getStatementSnapshot();

                        if (stmt_snapshot != nullptr)
                        {
                            // Use statement snapshot (similar to SNAPSHOT isolation)
                            // Check if creating transaction is visible in statement snapshot
                            if (!tm->isSnapshotVisible(xmin, stmt_snapshot))
                            {
                                return false;
                            }

                            // If deleted, check if deleting transaction is visible in statement snapshot
                            if (xmax != 0)
                            {
                                // Special case: deleted by current transaction
                                if (xmax == current_xid)
                                {
                                    return false; // We deleted it - not visible
                                }

                                // Check if deletion is visible in statement snapshot
                                if (tm->isSnapshotVisible(xmax, stmt_snapshot))
                                {
                                    return false; // Deletion committed before statement - not visible
                                }
                            }

                            return true;
                        }
                        else
                        {
                            // No statement snapshot - fall back to READ COMMITTED semantics
                            // This happens between statements (normal READ COMMITTED behavior)
                            if (!tm->isTransactionVisible(xmin, current_xid))
                            {
                                return false;
                            }

                            if (xmax != 0 && tm->isTransactionVisible(xmax, current_xid))
                            {
                                return false;
                            }

                            return true;
                        }
                    }

                    default:
                    {
                        LOG_ERROR(STORAGE, "Unknown isolation level: %d", static_cast<int>(iso_level));
                        return false;
                    }
                }
            }

            // Fallback to simple visibility rules (still validate XIDs)
            if (!TransactionManager::isValidXid(xmin))
            {
                // CORRUPTION LOGGING: Invalid xmin in fallback path
                LOG_ERROR(STORAGE, "Invalid xmin %lu in fallback visibility check", xmin);
                return false; // Invalid xmin
            }

            if (xmax != 0 && !TransactionManager::isValidXid(xmax))
            {
                // CORRUPTION LOGGING: Invalid xmax in fallback path
                LOG_WARNING(STORAGE, "Invalid xmax %lu in fallback visibility check - treating as not deleted", xmax);
                xmax = 0; // Invalid xmax - treat as not deleted
            }

            if (xmin > current_xid)
            {
                return false; // Created by future transaction
            }

            if (xmax > 0 && xmax < current_xid)
            {
                return false; // Deleted by committed transaction
            }

            return true;
        }

        auto StorageEngine::getCurrentXid() const -> uint64_t
        {
            // Get current XID from ConnectionContext (Phase 2 complete)
            uint64_t xid = ConnectionContext::getCurrentTransactionId();
            if (xid != 0) {
                return xid;
            }

            // Fallback if no connection context
            if (db_->transaction_manager() != nullptr)
            {
                return db_->transaction_manager()->getCurrentXid();
            }
            return config::DEFAULT_INITIAL_XID; // Default if no transaction manager
        }

        auto StorageEngine::findFreePage(const ID &table_id, uint32_t tuple_size,
                                             uint32_t *page_id_out, ErrorContext *ctx) -> Status
        {
            // For simplicity, we'll scan existing heap pages linearly
            // In a real system, we'd maintain a free space map per table

            uint32_t total_pages = page_manager_->totalPages();
            // Start scanning after catalog pages
            uint32_t heap_start = Config::getInstance().getUInt("storage", "heap_scan_start_page", 7);
            for (uint32_t page_id = heap_start; page_id < total_pages; page_id++)
            { // Arbitrary limit
                void *page_buffer;
                Status status = buffer_pool_->pinPage(page_id, &page_buffer, ctx);
                auto *page_data = static_cast<uint8_t *>(page_buffer);

                if (status == Status::IO_ERROR)
                {
                    // Page doesn't exist, allocate it
                    status = allocateHeapPage(table_id, page_id_out, ctx);
                    return status;
                }

                if (status != Status::OK)
                {
                    continue;
                }

                // Check if this is a heap page for our table
                auto *hdr = reinterpret_cast<PageHeader *>(page_data);
                if (hdr->page_type == PAGE_TYPE_HEAP)
                {
                    HeapPage heap_page(page_data, db_->page_size());

                    if (heap_page.hasFreeSpace(tuple_size + sizeof(TupleHeader)))
                    {
                        buffer_pool_->unpinPage(page_id, false, ctx);
                        *page_id_out = page_id;
                        return Status::OK;
                    }
                }

                buffer_pool_->unpinPage(page_id, false, ctx);
            }

            // No existing page has space, allocate a new one
            return allocateHeapPage(table_id, page_id_out, ctx);
        }

        auto StorageEngine::allocateHeapPage(const ID &table_id, uint32_t *page_id_out,
                                                 ErrorContext *ctx) -> Status
        {
            // Allocate a new page
            uint32_t page_id;
            Status status = page_manager_->allocatePage(page_id, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Pin and initialize the page
            void *page_buffer;
            status = buffer_pool_->pinPage(page_id, &page_buffer, ctx);
            auto *page_data = static_cast<uint8_t *>(page_buffer);
            if (status != Status::OK)
            {
                // Free the allocated page
                page_manager_->freePage(page_id, ctx);
                return status;
            }

            // Initialize as heap page
            HeapPage heap_page(page_data, db_->page_size());
            status = heap_page.initialize(page_id, ctx);

            if (status == Status::OK)
            {
                // Page will be marked dirty on unpin
                *page_id_out = page_id;
            }

            buffer_pool_->unpinPage(page_id, ctx != nullptr);

            return status;
        }

        // HeapScanIterator implementation

        HeapScanIterator::HeapScanIterator(Database *db, StorageEngine *engine, const ID &table_id,
                                           uint32_t start_page)
            : db_(db), engine_(engine), table_id_(table_id), current_page_(start_page),
              current_item_(0), last_page_(100), done_(false)
        {
        } // Arbitrary limit

        HeapScanIterator::~HeapScanIterator()
        {
            if (page_data_ != nullptr)
            {
                db_->buffer_pool()->unpinPage(current_page_, false, nullptr);
            }
        }

        auto HeapScanIterator::next(Tuple *tuple_out, ErrorContext *ctx) -> Status
        {
            if (done_)
            {
                return Status::NOT_FOUND;
            }

            while (current_page_ <= last_page_)
            {
                // Load current page if needed
                if (page_data_ == nullptr)
                {
                    Status status = loadPage(current_page_, ctx);
                    if (status == Status::IO_ERROR)
                    {
                        // Page doesn't exist, we're done
                        done_ = true;
                        return Status::NOT_FOUND;
                    }
                    if (status != Status::OK)
                    {
                        // Try next page
                        current_page_++;
                        current_item_ = 0;
                        continue;
                    }
                }

                // Check if this is a heap page
                auto *hdr = reinterpret_cast<PageHeader *>(page_data_);
                if (hdr->page_type != PAGE_TYPE_HEAP)
                {
                    // Not a heap page, try next
                    db_->buffer_pool()->unpinPage(current_page_, false, ctx);
                    page_data_ = nullptr;
                    current_page_++;
                    current_item_ = 0;
                    continue;
                }

                // Scan items in current page
                HeapPage heap_page(page_data_, db_->page_size());

                while (current_item_ < heap_page.getItemCount())
                {
                    const uint8_t *tuple_data;
                    uint32_t tuple_size;

                    Status status =
                        heap_page.getTuple(current_item_, &tuple_data, &tuple_size, nullptr);
                    current_item_++;

                    if (status == Status::OK)
                    {
                        // Check visibility
                        const auto *hdr = reinterpret_cast<const TupleHeader *>(tuple_data);

                        if (engine_->isVisible(hdr->xmin, hdr->xmax, engine_->getCurrentXid()))
                        {
                            // Found visible tuple
                            if (tuple_out != nullptr)
                            {
                                tuple_out->data = tuple_data;
                                tuple_out->data_size = tuple_size;
                                tuple_out->item_id = current_item_ - 1;
                                tuple_out->page_id = current_page_;
                                tuple_out->tid = (static_cast<uint64_t>(current_page_) << 16) |
                                                 (current_item_ - 1);
                            }
                            return Status::OK;
                        }
                    }
                }

                // Move to next page
                db_->buffer_pool()->unpinPage(current_page_, false, ctx);
                page_data_ = nullptr;
                current_page_++;
                current_item_ = 0;
            }

            done_ = true;
            return Status::NOT_FOUND;
        }

        auto HeapScanIterator::loadPage(uint32_t page_id, ErrorContext *ctx) -> Status
        {
            void *page_buffer;
            Status status = db_->buffer_pool()->pinPage(page_id, &page_buffer, ctx);
            if (status == Status::OK)
            {
                page_data_ = static_cast<uint8_t *>(page_buffer);
            }
            return status;
        }

        auto StorageEngine::deleteTuple(const ID &table_id, uint64_t tid, uint64_t xmax,
                                           ErrorContext *ctx) -> Status
        {
            // Extract page_id and item_id from TID
            uint32_t page_id = tid >> 16;
            uint16_t item_id = tid & 0xFFFF;

            // Use existing delete_tuple method
            return deleteTuple(table_id, page_id, item_id, ctx);
        }

        // MGA Phase 3: Version Chains

        auto StorageEngine::updateTuple(const ID &table_id, uint32_t page_id, uint16_t item_id,
                                        const uint8_t *new_tuple_data, uint32_t new_tuple_size,
                                        uint32_t *new_page_id_out, uint16_t *new_item_id_out,
                                        ErrorContext *ctx) -> Status
        {
            // Get proc_id from ConnectionContext (Phase 2 complete)
            int32_t proc_id_signed = ConnectionContext::getCurrentProcId();
            uint32_t proc_id = (proc_id_signed >= 0) ? static_cast<uint32_t>(proc_id_signed) : 0;

            // Acquire lock on old tuple (Phase 2.5 complete)
            bool wait = true; // TODO: Get from ConnectionContext::getWaitForLocks()
            Status lock_status = acquireTupleLock(table_id, page_id, item_id, proc_id, wait, ctx);
            if (lock_status != Status::OK) {
                return lock_status;
            }

            // Get current XID from transaction manager
            uint64_t xmax = (db_->transaction_manager() != nullptr)
                               ? db_->transaction_manager()->getCurrentXid()
                               : config::DEFAULT_INITIAL_XID;
            uint64_t new_xmin = xmax; // New version gets same XID as update

            // Pin the page
            void *page_buffer;
            Status status = buffer_pool_->pinPage(page_id, &page_buffer, ctx);
            auto *page_data = static_cast<uint8_t *>(page_buffer);
            if (status != Status::OK)
            {
                return status;
            }

            // Try to update tuple on same page
            HeapPage heap_page(page_data, db_->page_size());
            uint16_t new_item_id;

            status = heap_page.updateTuple(item_id, new_tuple_data, new_tuple_size,
                                          xmax, new_xmin, &new_item_id, ctx);

            if (status == Status::OK)
            {
                // Success - new version on same page
                if (new_page_id_out != nullptr)
                {
                    *new_page_id_out = page_id;
                }
                if (new_item_id_out != nullptr)
                {
                    *new_item_id_out = new_item_id;
                }

                // Mark page as dirty for GC
                if (db_->garbage_collector() != nullptr)
                {
                    db_->garbage_collector()->markPageDirty(page_id);
                }

                // Unpin with dirty flag
                buffer_pool_->unpinPage(page_id, true, ctx);
                return Status::OK;
            }
            else if (status == Status::PAGE_FULL)
            {
                // CROSS-PAGE UPDATE: Old page is full, need to place new version on different page
                // This implements cross-page version chains for MVCC

                // Unpin old page (no modifications made yet)
                buffer_pool_->unpinPage(page_id, false, ctx);

                // Find or allocate a new page with sufficient free space
                uint32_t new_page_id;
                status = findFreePage(table_id, new_tuple_size + sizeof(TupleHeader),
                                     &new_page_id, ctx);
                if (status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, status, "Failed to find free page for cross-page update");
                    return status;
                }

                // Pin the new page
                void *new_page_buffer;
                status = buffer_pool_->pinPage(new_page_id, &new_page_buffer, ctx);
                if (status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, status, "Failed to pin new page for cross-page update");
                    return status;
                }

                auto *new_page_data = static_cast<uint8_t *>(new_page_buffer);
                HeapPage new_heap_page(new_page_data, db_->page_size());

                // Insert new tuple version on the new page
                uint16_t new_item_id;
                status = new_heap_page.insertTuple(new_tuple_data, new_tuple_size, new_xmin,
                                                   &new_item_id, ctx);

                if (status != Status::OK)
                {
                    buffer_pool_->unpinPage(new_page_id, false, ctx);
                    SET_ERROR_CONTEXT(ctx, status, "Failed to insert tuple on new page for cross-page update");
                    return status;
                }

                // Acquire lock on new tuple location
                // Note: We already hold lock on old tuple from earlier in this function
                status = acquireTupleLock(table_id, new_page_id, new_item_id, proc_id, wait, ctx);
                if (status != Status::OK)
                {
                    buffer_pool_->unpinPage(new_page_id, false, ctx);
                    SET_ERROR_CONTEXT(ctx, status, "Failed to acquire lock on new tuple for cross-page update");
                    return status;
                }

                // Unpin new page (mark as dirty since we inserted a tuple)
                buffer_pool_->unpinPage(new_page_id, true, ctx);

                // Now pin the old page again to update the version chain
                status = buffer_pool_->pinPage(page_id, &page_buffer, ctx);
                if (status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, status, "Failed to re-pin old page for cross-page update");
                    return status;
                }

                page_data = static_cast<uint8_t *>(page_buffer);
                HeapPage old_heap_page(page_data, db_->page_size());

                // Get old tuple to update its version chain pointer
                const ItemPointer *items = reinterpret_cast<const ItemPointer *>(
                    page_data + sizeof(PageHeader));

                if (item_id >= old_heap_page.getItemCount())
                {
                    buffer_pool_->unpinPage(page_id, false, ctx);
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid old item ID after cross-page insert");
                    return Status::INVALID_ARGUMENT;
                }

                uint32_t old_offset = items[item_id].offset;
                auto *old_tuple_hdr = reinterpret_cast<TupleHeader *>(page_data + old_offset);

                // Build TID for new tuple on different page
                uint64_t new_tid = (static_cast<uint64_t>(new_page_id) << 32) |
                                   (static_cast<uint64_t>(new_item_id) << 16);

                // Update old tuple's version chain to point to new page
                old_tuple_hdr->xmax = xmax;
                old_tuple_hdr->next_version_tid = new_tid;
                old_tuple_hdr->infomask |= TupleHeader::HEAP_UPDATED;
                old_tuple_hdr->infomask |= TupleHeader::HEAP_MOVED;  // Mark as moved to different page
                old_tuple_hdr->infomask |= TupleHeader::HEAP_XMAX_COMMITTED;

                // Mark old page as dirty for GC
                if (db_->garbage_collector() != nullptr)
                {
                    db_->garbage_collector()->markPageDirty(page_id);
                }

                // Mark new page as dirty for GC
                if (db_->garbage_collector() != nullptr)
                {
                    db_->garbage_collector()->markPageDirty(new_page_id);
                }

                // Unpin old page (mark as dirty since we updated old tuple)
                buffer_pool_->unpinPage(page_id, true, ctx);

                // Return new location
                if (new_page_id_out != nullptr)
                {
                    *new_page_id_out = new_page_id;
                }
                if (new_item_id_out != nullptr)
                {
                    *new_item_id_out = new_item_id;
                }

                // Update indexes to point to new tuple location
                // We need to update all index entries from (old_page_id, old_item_id)
                // to (new_page_id, new_item_id) since the tuple has relocated
                status = updateIndexesForRelocation(table_id, page_id, item_id,
                                                   new_page_id, new_item_id,
                                                   new_tuple_data, new_tuple_size, ctx);
                if (status != Status::OK)
                {
                    // Log warning but don't fail the update
                    // The tuple update succeeded, index update is best-effort
                    LOG_WARNING(STORAGE, "Failed to update indexes after cross-page tuple relocation: %s",
                               ctx ? ctx->message.c_str() : "unknown error");
                    // Note: In production, we might want to mark indexes as needing rebuild
                }

                return Status::OK;
            }
            else
            {
                // Other error
                buffer_pool_->unpinPage(page_id, false, ctx);
                return status;
            }
        }

        auto
        StorageEngine::sequentialScan(const ID &table_id, const std::vector<uint32_t> &columns,
                                       uint64_t xmin, ErrorContext *ctx) -> std::unique_ptr<HeapScanIterator>
        {
            // For now, just use the existing create_scan method
            // In a real implementation, we would filter by columns and visibility
            return createScan(table_id, ctx);
        }

        // IndexScanIterator implementation

        IndexScanIterator::IndexScanIterator(Database *db, StorageEngine *engine,
                                             const ID &index_id)
            : db_(db), engine_(engine), index_id_(index_id), done_(false),
              current_tuple_index_(0), initialized_(false)
        {
        }

        IndexScanIterator::~IndexScanIterator() = default;

        auto IndexScanIterator::seek(const std::vector<uint8_t> &key, ErrorContext *ctx) -> Status
        {
            // Get index information from catalog
            CatalogManager::IndexInfo index_info;
            Status status = db_->catalog_manager()->getIndex(index_id_, index_info, ctx);
            if (status != Status::OK)
            {
                done_ = true;
                return status;
            }

            // Create a BTree instance for this index
            SBBTreeIndex btree_info;
            btree_info.idx_uuid = index_info.index_id;
            btree_info.idx_table_uuid = index_info.table_id;
            btree_info.idx_root_page = index_info.root_page;

            BTree btree(db_, btree_info);

            // Search for the key in the B-tree
            current_tuple_ids_.clear();
            current_tuple_index_ = 0;
            current_key_ = key;

            status = btree.search(key, &current_tuple_ids_, ctx);
            if (status == Status::NOT_FOUND)
            {
                // No matching key found, mark as done
                done_ = true;
                initialized_ = true;
                return Status::OK;
            }
            else if (status != Status::OK)
            {
                done_ = true;
                return status;
            }

            initialized_ = true;
            done_ = current_tuple_ids_.empty();
            return Status::OK;
        }

        auto IndexScanIterator::next(Tuple *tuple_out, ErrorContext *ctx) -> Status
        {
            if (!initialized_)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Must call seek() before next()");
                return Status::INVALID_ARGUMENT;
            }

            if (done_ || current_tuple_index_ >= current_tuple_ids_.size())
            {
                done_ = true;
                return Status::NOT_FOUND;
            }

            // Get the next tuple ID
            uint64_t tid = current_tuple_ids_[current_tuple_index_++];

            // Extract page_id and item_id from tuple ID
            // Assuming tuple ID format: upper 32 bits = page_id, lower 32 bits = item_id
            uint32_t page_id = static_cast<uint32_t>(tid >> 32);
            uint16_t item_id = static_cast<uint16_t>(tid & 0xFFFF);

            // Fill tuple_out with the location information
            if (tuple_out != nullptr)
            {
                tuple_out->tid = tid;
                tuple_out->page_id = page_id;
                tuple_out->item_id = item_id;
                tuple_out->data = nullptr;  // Caller must fetch actual tuple data
                tuple_out->data_size = 0;
            }

            // Check if we've exhausted this key's tuples
            if (current_tuple_index_ >= current_tuple_ids_.size())
            {
                done_ = true;
            }

            return Status::OK;
        }

        auto StorageEngine::createIndexScan(const ID &index_id,
                                                                            ErrorContext *ctx) -> std::unique_ptr<IndexScanIterator>
        {
            return std::make_unique<IndexScanIterator>(db_, this, index_id);
        }

        // Lock management helpers

        auto StorageEngine::acquireTupleLock(const ID &table_id, uint32_t page_id,
                                             uint16_t item_id, uint32_t proc_id, bool wait,
                                             ErrorContext *ctx) -> Status
        {
            // Build lock tag for tuple
            LockTag tag{};
            tag.target_type = LockTarget::LOCK_TARGET_TUPLE;
            tag.object_uuid = table_id;
            tag.page_num = page_id;
            tag.offset_num = item_id;
            tag.padding = 0;

            // Acquire ROW_EXCLUSIVE lock (for UPDATE/DELETE)
            LockManager *lock_mgr = db_->lock_manager();
            if (lock_mgr == nullptr)
            {
                // No lock manager, skip locking (single-connection mode)
                return Status::OK;
            }

            return lock_mgr->acquireLock(proc_id, tag, LockMode::LOCK_ROW_EXCLUSIVE, wait,
                                        0, ctx);
        }

        auto StorageEngine::releaseTupleLock(const ID &table_id, uint32_t page_id,
                                             uint16_t item_id, uint32_t proc_id,
                                             ErrorContext *ctx) -> Status
        {
            // Build lock tag for tuple
            LockTag tag{};
            tag.target_type = LockTarget::LOCK_TARGET_TUPLE;
            tag.object_uuid = table_id;
            tag.page_num = page_id;
            tag.offset_num = item_id;
            tag.padding = 0;

            // Release ROW_EXCLUSIVE lock
            LockManager *lock_mgr = db_->lock_manager();
            if (lock_mgr == nullptr)
            {
                // No lock manager, nothing to release
                return Status::OK;
            }

            return lock_mgr->releaseLock(proc_id, tag, LockMode::LOCK_ROW_EXCLUSIVE, ctx);
        }

        // Helper function to extract indexed column values and build an index key
        // This is a simplified implementation that assumes basic column layout
        // TODO: Implement proper tuple deserialization with support for:
        // - Variable-length columns
        // - NULL values and null bitmaps
        // - Complex data types (arrays, json, etc.)
        static Status buildIndexKey(const uint8_t *tuple_data, uint32_t tuple_size,
                                   const std::vector<CatalogManager::ColumnInfo> &all_columns,
                                   const std::vector<ID> &indexed_column_ids,
                                   std::vector<uint8_t> *key_out, ErrorContext *ctx)
        {
            // Skip tuple header to get to actual data
            if (tuple_size < sizeof(TupleHeader))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Tuple size too small");
                return Status::INVALID_ARGUMENT;
            }

            const uint8_t *data = tuple_data + sizeof(TupleHeader);
            uint32_t data_size = tuple_size - sizeof(TupleHeader);

            // Build a map of column_id to column_info for quick lookup
            std::unordered_map<ID, const CatalogManager::ColumnInfo*> column_map;
            for (const auto &col : all_columns)
            {
                column_map[col.column_id] = &col;
            }

            // For now, use a simplified approach: concatenate raw column values
            // This assumes fixed-width columns in order
            // TODO: Implement proper column value extraction based on column types and offsets

            key_out->clear();

            // Simple approach: for single-column indexes, just use the first few bytes
            // For multi-column indexes, concatenate the values
            // This is a placeholder that works for simple integer keys
            if (data_size == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Empty tuple data");
                return Status::INVALID_ARGUMENT;
            }

            // For now, copy the raw data as the key (simplified)
            // In a real implementation, we would:
            // 1. Parse the tuple layout
            // 2. Extract each indexed column value by ordinal position
            // 3. Serialize them in order into the key
            key_out->assign(data, data + std::min(data_size, static_cast<uint32_t>(256)));

            return Status::OK;
        }

        // Helper function to update all indexes when a tuple relocates to a different page
        auto StorageEngine::updateIndexesForRelocation(const ID &table_id,
                                                       uint32_t old_page_id, uint16_t old_item_id,
                                                       uint32_t new_page_id, uint16_t new_item_id,
                                                       const uint8_t *tuple_data, uint32_t tuple_size,
                                                       ErrorContext *ctx) -> Status
        {
            // Get all indexes for this table
            std::vector<CatalogManager::IndexInfo> indexes;
            Status status = catalog_manager_->listIndexesForTable(table_id, indexes, ctx);
            if (status != Status::OK)
            {
                // If no indexes or error, just warn and continue
                if (status == Status::NOT_FOUND)
                {
                    // No indexes on this table - this is fine
                    return Status::OK;
                }
                LOG_WARNING(STORAGE, "Failed to list indexes for table during cross-page update: %s",
                           ctx ? ctx->message.c_str() : "unknown error");
                return Status::OK; // Don't fail the update if we can't update indexes
            }

            if (indexes.empty())
            {
                // No indexes to update
                return Status::OK;
            }

            // Get column information for the table (needed to build index keys)
            std::vector<CatalogManager::ColumnInfo> columns;
            status = catalog_manager_->getColumns(table_id, columns, ctx);
            if (status != Status::OK)
            {
                LOG_WARNING(STORAGE, "Failed to get column info for table during index update");
                return Status::OK; // Don't fail the update
            }

            // Calculate old and new TIDs
            uint64_t old_tid = (static_cast<uint64_t>(old_page_id) << 32) |
                              (static_cast<uint64_t>(old_item_id) << 16);
            uint64_t new_tid = (static_cast<uint64_t>(new_page_id) << 32) |
                              (static_cast<uint64_t>(new_item_id) << 16);

            // Update each index
            for (const auto &index_info : indexes)
            {
                // Build the index key from tuple data
                std::vector<uint8_t> key;
                status = buildIndexKey(tuple_data, tuple_size, columns,
                                     index_info.column_ids, &key, ctx);
                if (status != Status::OK)
                {
                    LOG_WARNING(STORAGE, "Failed to build index key for index %s during cross-page update",
                               index_info.index_name.c_str());
                    continue; // Skip this index, try others
                }

                // Update based on index type
                if (index_info.index_type == CatalogManager::IndexType::BTREE)
                {
                    // Open the BTree index
                    auto btree = BTree::open(db_, index_info.index_id, index_info.root_page, ctx);
                    if (!btree)
                    {
                        LOG_WARNING(STORAGE, "Failed to open BTree index %s for update",
                                   index_info.index_name.c_str());
                        continue;
                    }

                    // Remove old entry
                    status = btree->remove(key, old_tid, ctx);
                    if (status != Status::OK && status != Status::NOT_FOUND)
                    {
                        LOG_WARNING(STORAGE, "Failed to remove old entry from BTree index %s: %s",
                                   index_info.index_name.c_str(),
                                   ctx ? ctx->message.c_str() : "unknown error");
                        // Continue anyway - try to insert new entry
                    }

                    // Insert new entry
                    status = btree->insert(key, new_tid, ctx);
                    if (status != Status::OK)
                    {
                        LOG_ERROR(STORAGE, "Failed to insert new entry into BTree index %s: %s",
                                 index_info.index_name.c_str(),
                                 ctx ? ctx->message.c_str() : "unknown error");
                        // This is a problem - index is now inconsistent
                        // In a real system, we might need to mark the index as needing rebuild
                    }
                }
                else if (index_info.index_type == CatalogManager::IndexType::HASH)
                {
                    // Open the Hash index
                    auto hash_index = HashIndex::open(db_, index_info.index_id,
                                                     index_info.root_page, ctx);
                    if (!hash_index)
                    {
                        LOG_WARNING(STORAGE, "Failed to open Hash index %s for update",
                                   index_info.index_name.c_str());
                        continue;
                    }

                    // Remove old entry
                    status = hash_index->remove(key.data(), key.size(), old_tid, ctx);
                    if (status != Status::OK && status != Status::NOT_FOUND)
                    {
                        LOG_WARNING(STORAGE, "Failed to remove old entry from Hash index %s: %s",
                                   index_info.index_name.c_str(),
                                   ctx ? ctx->message.c_str() : "unknown error");
                        // Continue anyway - try to insert new entry
                    }

                    // Insert new entry
                    status = hash_index->insert(key.data(), key.size(), new_tid, ctx);
                    if (status != Status::OK)
                    {
                        LOG_ERROR(STORAGE, "Failed to insert new entry into Hash index %s: %s",
                                 index_info.index_name.c_str(),
                                 ctx ? ctx->message.c_str() : "unknown error");
                        // This is a problem - index is now inconsistent
                    }
                }
                else
                {
                    // Unsupported index type
                    LOG_WARNING(STORAGE, "Unsupported index type %d for index %s during cross-page update",
                               static_cast<int>(index_info.index_type),
                               index_info.index_name.c_str());
                }
            }

            return Status::OK; // Always return OK - don't fail the update if index updates have issues
        }

        auto StorageEngine::getOrCreateToastManager(const ID &table_id, ErrorContext *ctx) -> ToastManager*
        {
            // Check if we already have a ToastManager for this table
            {
                std::lock_guard<std::mutex> lock(toast_mutex_);
                auto it = toast_managers_.find(table_id);
                if (it != toast_managers_.end())
                {
                    return it->second.get();
                }
            }

            // Create new ToastManager (outside the lock to avoid holding it during initialization)
            auto toast_mgr = std::make_unique<ToastManager>(db_, table_id);

            // Initialize the ToastManager
            Status status = toast_mgr->initialize(ctx);
            if (status != Status::OK)
            {
                // Initialization failed - return nullptr
                // This can happen if TOAST table doesn't exist yet
                // In production, we might want to create it automatically
                return nullptr;
            }

            // Store it in the map
            ToastManager* result = toast_mgr.get();
            {
                std::lock_guard<std::mutex> lock(toast_mutex_);
                // Check again in case another thread created it
                auto it = toast_managers_.find(table_id);
                if (it != toast_managers_.end())
                {
                    return it->second.get();
                }
                toast_managers_[table_id] = std::move(toast_mgr);
            }

            return result;
        }

    } // namespace scratchbird::core
