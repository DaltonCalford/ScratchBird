#pragma once

#include "scratchbird/core/status.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace scratchbird {
namespace core {

// Forward declarations
class Database;
class BufferPool;
class PageManager;
class CatalogManager;
class HeapPage;
class StorageEngine;
struct ErrorContext;
struct TableInfo;

// Tuple data structure
struct Tuple {
    std::vector<uint8_t> data;
    uint32_t size;
    uint16_t item_id;  // Item ID within the page
    uint32_t page_id;  // Page containing this tuple
};

// Iterator for sequential scan
class HeapScanIterator {
public:
    HeapScanIterator(Database* db, StorageEngine* engine, uint32_t table_id, uint32_t start_page);
    ~HeapScanIterator();
    
    // Move to next tuple
    Status next(Tuple* tuple_out, ErrorContext* ctx = nullptr);
    
    // Check if scan is complete
    bool is_done() const { return done_; }
    
private:
    Database* db_;
    StorageEngine* engine_;
    uint32_t table_id_;
    uint32_t current_page_;
    uint16_t current_item_;
    uint32_t last_page_;
    bool done_;
    
    // Current page data
    uint8_t* page_data_ = nullptr;
    
    // Load next page
    Status load_page(uint32_t page_id, ErrorContext* ctx);
};

// Storage engine for heap storage
class StorageEngine {
public:
    explicit StorageEngine(Database* db);
    ~StorageEngine();
    
    // Insert a tuple into a table
    // Returns the tuple ID (page_id, item_id) on success
    Status insert_tuple(uint32_t table_id, const uint8_t* tuple_data,
                       uint32_t tuple_size, uint32_t* page_id_out,
                       uint16_t* item_id_out, ErrorContext* ctx = nullptr);
    
    // Get a specific tuple by ID
    Status get_tuple(uint32_t page_id, uint16_t item_id,
                    Tuple* tuple_out, ErrorContext* ctx = nullptr);
    
    // Delete a tuple (mark as deleted)
    Status delete_tuple(uint32_t page_id, uint16_t item_id,
                       ErrorContext* ctx = nullptr);
    
    // Create a sequential scan iterator
    std::unique_ptr<HeapScanIterator> create_scan(uint32_t table_id,
                                                  ErrorContext* ctx = nullptr);
    
    // Check if a tuple is visible (basic visibility for single connection)
    bool is_visible(uint64_t xmin, uint64_t xmax, uint64_t current_xid);
    
    // Get current transaction ID from TransactionManager
    uint64_t get_current_xid() const;
    
private:
    Database* db_;
    BufferPool* buffer_pool_;
    PageManager* page_manager_;
    CatalogManager* catalog_manager_;
    
    // Find a page with free space for a tuple
    Status find_free_page(uint32_t table_id, uint32_t tuple_size,
                         uint32_t* page_id_out, ErrorContext* ctx);
    
    // Allocate a new heap page for a table
    Status allocate_heap_page(uint32_t table_id, uint32_t* page_id_out,
                             ErrorContext* ctx);
};

} // namespace core
} // namespace scratchbird