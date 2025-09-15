#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/uuidv7.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace scratchbird {
namespace core {

using ID = UuidV7Bytes;

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
    const uint8_t* data;     // Pointer to tuple data
    uint32_t data_size;      // Size of tuple data
    uint64_t tid;            // Tuple ID (page_id << 16 | item_id)
    uint16_t item_id;        // Item ID within the page
    uint32_t page_id;        // Page containing this tuple
};

// Iterator for sequential scan
class HeapScanIterator {
public:
    HeapScanIterator(Database* db, StorageEngine* engine, const ID& table_id, uint32_t start_page);
    ~HeapScanIterator();
    
    // Move to next tuple
    Status next(Tuple* tuple_out, ErrorContext* ctx = nullptr);
    
    // Check if scan is complete
    bool is_done() const { return done_; }
    
private:
    Database* db_;
    StorageEngine* engine_;
    ID table_id_;
    uint32_t current_page_;
    uint16_t current_item_;
    uint32_t last_page_;
    bool done_;
    
    // Current page data
    uint8_t* page_data_ = nullptr;
    
    // Load next page
    Status load_page(uint32_t page_id, ErrorContext* ctx);
};

// Iterator for index scan
class IndexScanIterator {
public:
    IndexScanIterator(Database* db, StorageEngine* engine, const ID& index_id);
    ~IndexScanIterator();

    // Move to the first entry >= key
    Status seek(const std::vector<uint8_t>& key, ErrorContext* ctx = nullptr);

    // Move to the next entry
    Status next(Tuple* tuple_out, ErrorContext* ctx = nullptr);

    // Check if scan is complete
    bool is_done() const { return done_; }

private:
    Database* db_;
    StorageEngine* engine_;
    ID index_id_;
    bool done_;
    // TODO: Add B-tree traversal state
};

// Storage engine for heap storage
class StorageEngine {
public:
    explicit StorageEngine(Database* db);
    ~StorageEngine();
    
    // Insert a tuple into a table
    // Returns the tuple ID (page_id, item_id) on success
    Status insert_tuple(const ID& table_id, const uint8_t* tuple_data,
                       uint32_t tuple_size, uint32_t* page_id_out,
                       uint16_t* item_id_out, ErrorContext* ctx = nullptr);
    
    // Get a specific tuple by ID
    Status get_tuple(uint32_t page_id, uint16_t item_id,
                    Tuple* tuple_out, ErrorContext* ctx = nullptr);
    
    // Delete a tuple (mark as deleted)
    Status delete_tuple(uint32_t page_id, uint16_t item_id,
                       ErrorContext* ctx = nullptr);
    
    // Delete a tuple by TID
    Status delete_tuple(const ID& table_id, uint64_t tid, uint64_t xmax,
                       ErrorContext* ctx = nullptr);
    
    // Create a sequential scan iterator
    std::unique_ptr<HeapScanIterator> create_scan(const ID& table_id,
                                                  ErrorContext* ctx = nullptr);

    // Create an index scan iterator
    std::unique_ptr<IndexScanIterator> create_index_scan(const ID& index_id,
                                                       ErrorContext* ctx = nullptr);
    
    // Create a sequential scan iterator with visibility
    std::unique_ptr<HeapScanIterator> sequential_scan(const ID& table_id,
                                                      const std::vector<uint32_t>& columns,
                                                      uint64_t xmin,
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
    Status find_free_page(const ID& table_id, uint32_t tuple_size,
                         uint32_t* page_id_out, ErrorContext* ctx);
    
    // Allocate a new heap page for a table
    Status allocate_heap_page(const ID& table_id, uint32_t* page_id_out,
                             ErrorContext* ctx);
};

} // namespace core
} // namespace scratchbird