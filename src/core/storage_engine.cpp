#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include <cstring>
#include <new>

namespace scratchbird {
namespace core {

StorageEngine::StorageEngine(Database* db)
    : db_(db),
      buffer_pool_(db->buffer_pool()),
      page_manager_(db->page_manager()),
      catalog_manager_(db->catalog_manager()) {}

StorageEngine::~StorageEngine() {}

Status StorageEngine::insert_tuple(uint32_t table_id, const uint8_t* tuple_data,
                                  uint32_t tuple_size, uint32_t* page_id_out,
                                  uint16_t* item_id_out, ErrorContext* ctx) {
    // Find a page with free space
    uint32_t page_id;
    Status status = find_free_page(table_id, tuple_size, &page_id, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    // Pin the page
    void* page_buffer;
    status = buffer_pool_->pin_page(page_id, &page_buffer, ctx);
    uint8_t* page_data = static_cast<uint8_t*>(page_buffer);
    if (status != Status::Ok) {
        return status;
    }
    
    // Insert tuple
    HeapPage heap_page(page_data, db_->page_size());
    uint16_t item_id;
    status = heap_page.insert_tuple(tuple_data, tuple_size, current_xid_,
                                   &item_id, ctx);
    
    if (status == Status::Ok) {
        // Mark page as dirty
        // Page will be marked dirty on unpin
        
        if (page_id_out) *page_id_out = page_id;
        if (item_id_out) *item_id_out = item_id;
    }
    
    // Unpin the page
    buffer_pool_->unpin_page(page_id, status == Status::Ok, ctx);
    
    return status;
}

Status StorageEngine::get_tuple(uint32_t page_id, uint16_t item_id,
                               Tuple* tuple_out, ErrorContext* ctx) {
    // Pin the page
    void* page_buffer;
    Status status = buffer_pool_->pin_page(page_id, &page_buffer, ctx);
    uint8_t* page_data = static_cast<uint8_t*>(page_buffer);
    if (status != Status::Ok) {
        return status;
    }
    
    // Get tuple
    HeapPage heap_page(page_data, db_->page_size());
    const uint8_t* tuple_data;
    uint32_t tuple_size;
    
    status = heap_page.get_tuple(item_id, &tuple_data, &tuple_size, ctx);
    
    if (status == Status::Ok && tuple_out) {
        // Check visibility
        const TupleHeader* hdr = reinterpret_cast<const TupleHeader*>(tuple_data);
        if (!is_visible(hdr->xmin, hdr->xmax, current_xid_)) {
            status = Status::NotFound;
            SET_ERROR_CONTEXT(ctx, status, "Tuple not visible");
        } else {
            // Copy tuple data (skip header for user data)
            tuple_out->data.resize(tuple_size - sizeof(TupleHeader));
            memcpy(tuple_out->data.data(), tuple_data + sizeof(TupleHeader),
                   tuple_size - sizeof(TupleHeader));
            tuple_out->size = tuple_size - sizeof(TupleHeader);
            tuple_out->item_id = item_id;
            tuple_out->page_id = page_id;
        }
    }
    
    // Unpin the page
    buffer_pool_->unpin_page(page_id, status == Status::Ok, ctx);
    
    return status;
}

Status StorageEngine::delete_tuple(uint32_t page_id, uint16_t item_id,
                                  ErrorContext* ctx) {
    // Pin the page
    void* page_buffer;
    Status status = buffer_pool_->pin_page(page_id, &page_buffer, ctx);
    uint8_t* page_data = static_cast<uint8_t*>(page_buffer);
    if (status != Status::Ok) {
        return status;
    }
    
    // Delete tuple
    HeapPage heap_page(page_data, db_->page_size());
    status = heap_page.delete_tuple(item_id, current_xid_, ctx);
    
    if (status == Status::Ok) {
        // Mark page as dirty
        // Page will be marked dirty on unpin
    }
    
    // Unpin the page
    buffer_pool_->unpin_page(page_id, status == Status::Ok, ctx);
    
    return status;
}

std::unique_ptr<HeapScanIterator> StorageEngine::create_scan(uint32_t table_id,
                                                            ErrorContext* ctx) {
    // For now, we don't need table info - just return a scanner
    // In a real system, we'd track heap pages per table in the catalog
    
    // For now, assume heap pages start at page 7 (after catalog pages)
    // In a real system, we'd track this in the catalog
    uint32_t start_page = 7;  // First heap page
    
    return std::unique_ptr<HeapScanIterator>(
        new(std::nothrow) HeapScanIterator(db_, this, table_id, start_page));
}

bool StorageEngine::is_visible(uint64_t xmin, uint64_t xmax, uint64_t current_xid) {
    // Simple visibility rules for single connection:
    // - Tuple is visible if xmin <= current_xid
    // - Tuple is not visible if xmax > 0 and xmax <= current_xid
    
    if (xmin > current_xid) {
        return false;  // Created by future transaction
    }
    
    if (xmax > 0 && xmax <= current_xid) {
        return false;  // Deleted by committed transaction
    }
    
    return true;
}

Status StorageEngine::find_free_page(uint32_t table_id, uint32_t tuple_size,
                                    uint32_t* page_id_out, ErrorContext* ctx) {
    // For simplicity, we'll scan existing heap pages linearly
    // In a real system, we'd maintain a free space map per table
    
    // Start scanning from page 7 (after catalog pages)
    for (uint32_t page_id = 7; page_id < 100; page_id++) {  // Arbitrary limit
        void* page_buffer;
        Status status = buffer_pool_->pin_page(page_id, &page_buffer, ctx);
        uint8_t* page_data = static_cast<uint8_t*>(page_buffer);
        
        if (status == Status::IoError) {
            // Page doesn't exist, allocate it
            status = allocate_heap_page(table_id, page_id_out, ctx);
            return status;
        }
        
        if (status != Status::Ok) {
            continue;
        }
        
        // Check if this is a heap page for our table
        PageHeader* hdr = reinterpret_cast<PageHeader*>(page_data);
        if (hdr->page_type == PAGE_TYPE_HEAP) {
            HeapPage heap_page(page_data, db_->page_size());
            
            if (heap_page.has_free_space(tuple_size + sizeof(TupleHeader))) {
                buffer_pool_->unpin_page(page_id, false, ctx);
                *page_id_out = page_id;
                return Status::Ok;
            }
        }
        
        buffer_pool_->unpin_page(page_id, false, ctx);
    }
    
    // No existing page has space, allocate a new one
    return allocate_heap_page(table_id, page_id_out, ctx);
}

Status StorageEngine::allocate_heap_page(uint32_t table_id, uint32_t* page_id_out,
                                        ErrorContext* ctx) {
    // Allocate a new page
    uint32_t page_id;
    Status status = page_manager_->allocate_page(page_id, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    // Pin and initialize the page
    void* page_buffer;
    status = buffer_pool_->pin_page(page_id, &page_buffer, ctx);
    uint8_t* page_data = static_cast<uint8_t*>(page_buffer);
    if (status != Status::Ok) {
        // Free the allocated page
        page_manager_->free_page(page_id, ctx);
        return status;
    }
    
    // Initialize as heap page
    HeapPage heap_page(page_data, db_->page_size());
    status = heap_page.initialize(page_id, ctx);
    
    if (status == Status::Ok) {
        // Page will be marked dirty on unpin
        *page_id_out = page_id;
    }
    
    buffer_pool_->unpin_page(page_id, ctx);
    
    return status;
}

// HeapScanIterator implementation

HeapScanIterator::HeapScanIterator(Database* db, StorageEngine* engine,
                                   uint32_t table_id, uint32_t start_page)
    : db_(db), engine_(engine), table_id_(table_id), current_page_(start_page),
      current_item_(0), last_page_(100), done_(false) {}  // Arbitrary limit

HeapScanIterator::~HeapScanIterator() {
    if (page_data_) {
        db_->buffer_pool()->unpin_page(current_page_, false, nullptr);
    }
}

Status HeapScanIterator::next(Tuple* tuple_out, ErrorContext* ctx) {
    if (done_) {
        return Status::NotFound;
    }
    
    while (current_page_ <= last_page_) {
        // Load current page if needed
        if (!page_data_) {
            Status status = load_page(current_page_, ctx);
            if (status == Status::IoError) {
                // Page doesn't exist, we're done
                done_ = true;
                return Status::NotFound;
            }
            if (status != Status::Ok) {
                // Try next page
                current_page_++;
                current_item_ = 0;
                continue;
            }
        }
        
        // Check if this is a heap page
        PageHeader* hdr = reinterpret_cast<PageHeader*>(page_data_);
        if (hdr->page_type != PAGE_TYPE_HEAP) {
            // Not a heap page, try next
            db_->buffer_pool()->unpin_page(current_page_, false, ctx);
            page_data_ = nullptr;
            current_page_++;
            current_item_ = 0;
            continue;
        }
        
        // Scan items in current page
        HeapPage heap_page(page_data_, db_->page_size());
        
        while (current_item_ < heap_page.get_item_count()) {
            const uint8_t* tuple_data;
            uint32_t tuple_size;
            
            Status status = heap_page.get_tuple(current_item_, &tuple_data,
                                               &tuple_size, nullptr);
            current_item_++;
            
            if (status == Status::Ok) {
                // Check visibility
                const TupleHeader* hdr = reinterpret_cast<const TupleHeader*>(tuple_data);
                
                if (engine_->is_visible(hdr->xmin, hdr->xmax, engine_->get_current_xid())) {
                    // Found visible tuple
                    if (tuple_out) {
                        tuple_out->data.resize(tuple_size - sizeof(TupleHeader));
                        memcpy(tuple_out->data.data(),
                               tuple_data + sizeof(TupleHeader),
                               tuple_size - sizeof(TupleHeader));
                        tuple_out->size = tuple_size - sizeof(TupleHeader);
                        tuple_out->item_id = current_item_ - 1;
                        tuple_out->page_id = current_page_;
                    }
                    return Status::Ok;
                }
            }
        }
        
        // Move to next page
        db_->buffer_pool()->unpin_page(current_page_, false, ctx);
        page_data_ = nullptr;
        current_page_++;
        current_item_ = 0;
    }
    
    done_ = true;
    return Status::NotFound;
}

Status HeapScanIterator::load_page(uint32_t page_id, ErrorContext* ctx) {
    void* page_buffer;
    Status status = db_->buffer_pool()->pin_page(page_id, &page_buffer, ctx);
    if (status == Status::Ok) {
        page_data_ = static_cast<uint8_t*>(page_buffer);
    }
    return status;
}

} // namespace core
} // namespace scratchbird