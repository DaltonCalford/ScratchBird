#include "scratchbird/core/config.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/error_context.h"
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

            // Insert tuple
            HeapPage heap_page(page_data, db_->page_size());
            uint16_t item_id;

            // Get current XID from transaction manager
            uint64_t current_xid =
                (db_->transaction_manager() != nullptr) ? db_->transaction_manager()->getActiveXid() : 0;
            if (current_xid == 0)
            {
                // No active transaction, use a default
                current_xid = (db_->transaction_manager() != nullptr)
                                  ? db_->transaction_manager()->getCurrentXid()
                                  : 100;
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

            return status;
        }

        auto StorageEngine::deleteTuple(uint32_t page_id, uint16_t item_id, ErrorContext *ctx) -> Status
        {
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

            // Get current XID from transaction manager
            uint64_t current_xid =
                (db_->transaction_manager() != nullptr) ? db_->transaction_manager()->getActiveXid() : 0;
            if (current_xid == 0)
            {
                // No active transaction, use a default
                current_xid = (db_->transaction_manager() != nullptr)
                                  ? db_->transaction_manager()->getCurrentXid()
                                  : 100;
            }

            status = heap_page.deleteTuple(item_id, current_xid, ctx);

            if (status == Status::OK)
            {
                // Mark page as dirty
                // Page will be marked dirty on unpin
            }

            // Unpin the page
            buffer_pool_->unpinPage(page_id, status == Status::OK, ctx);

            return status;
        }

        auto StorageEngine::createScan(const ID &table_id,
                                                                     ErrorContext *ctx) -> std::unique_ptr<HeapScanIterator>
        {
            // For now, we don't need table info - just return a scanner
            // In a real system, we'd track heap pages per table in the catalog

            // For now, assume heap pages start at page 7 (after catalog pages)
            // In a real system, we'd track this in the catalog
            uint32_t start_page = config::HEAP_SCAN_START_PAGE; // First heap page

            return std::unique_ptr<HeapScanIterator>(
                new (std::nothrow) HeapScanIterator(db_, this, table_id, start_page));
        }

        auto StorageEngine::isVisible(uint64_t xmin, uint64_t xmax, uint64_t current_xid) -> bool
        {
            // Use transaction manager for visibility if available
            if (db_->transaction_manager() != nullptr)
            {
                TransactionManager *tm = db_->transaction_manager();

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

            // Fallback to simple visibility rules
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
            if (db_->transaction_manager() != nullptr)
            {
                uint64_t xid = db_->transaction_manager()->getActiveXid();
                if (xid != 0)
                {
                    return xid;
                }
                return db_->transaction_manager()->getCurrentXid();
            }
            return 100; // Default if no transaction manager
        }

        auto StorageEngine::findFreePage(const ID &table_id, uint32_t tuple_size,
                                             uint32_t *page_id_out, ErrorContext *ctx) -> Status
        {
            // For simplicity, we'll scan existing heap pages linearly
            // In a real system, we'd maintain a free space map per table

            uint32_t total_pages = page_manager_->totalPages();
            // Start scanning from page 7 (after catalog pages)
            for (uint32_t page_id = config::HEAP_SCAN_START_PAGE; page_id < total_pages; page_id++)
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
            return deleteTuple(page_id, item_id, ctx);
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
            : db_(db), engine_(engine), index_id_(index_id), done_(false)
        {
        }

        IndexScanIterator::~IndexScanIterator() = default;

        auto IndexScanIterator::seek(const std::vector<uint8_t> &key, ErrorContext *ctx) -> Status
        {
            // TODO: Implement B-tree seek
            done_ = true;
            return Status::NOT_IMPLEMENTED;
        }

        auto IndexScanIterator::next(Tuple *tuple_out, ErrorContext *ctx) -> Status
        {
            // TODO: Implement B-tree next
            done_ = true;
            return Status::NOT_IMPLEMENTED;
        }

        auto StorageEngine::createIndexScan(const ID &index_id,
                                                                            ErrorContext *ctx) -> std::unique_ptr<IndexScanIterator>
        {
            return std::make_unique<IndexScanIterator>(db_, this, index_id);
        }

    } // namespace scratchbird::core
