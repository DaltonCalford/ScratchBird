#pragma once

#include <cstdint>
#include <vector>
#include <mutex>
#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/error_context.h"

namespace scratchbird {
namespace core {

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
class PageManager {
public:
    PageManager(Database* db, uint32_t page_size);
    ~PageManager();
    
    // Initialize FSM for a new database
    Status initialize(ErrorContext* ctx = nullptr);
    
    // Load FSM from existing database
    Status load(ErrorContext* ctx = nullptr);
    
    // Allocate a new page
    Status allocate_page(uint32_t& page_id, ErrorContext* ctx = nullptr);
    
    // Free a page
    Status free_page(uint32_t page_id, ErrorContext* ctx = nullptr);
    
    // Check if a page is allocated
    bool is_allocated(uint32_t page_id) const;
    
    // Get total number of pages
    uint32_t total_pages() const { return total_pages_; }
    
    // Get number of free pages
    uint32_t free_pages() const { return free_pages_; }
    
    // Extend the database file
    Status extend_file(uint32_t num_pages, ErrorContext* ctx = nullptr);
    
    // Flush FSM to disk
    Status flush(ErrorContext* ctx = nullptr);
    
protected:
    Database* db_;                    // Database instance
    uint32_t page_size_;             // Page size
    
private:
    uint32_t total_pages_;           // Total pages in database
    uint32_t free_pages_;            // Number of free pages
    std::vector<uint8_t> bitmap_;    // Allocation bitmap
    bool dirty_;                     // FSM needs flush
    mutable std::mutex mutex_;       // Thread safety (future)
    
    // Helper methods
    void set_bit(uint32_t page_id, bool allocated);
    bool get_bit(uint32_t page_id) const;
    uint32_t find_free_page() const;
    
    // FSM page structure
    struct FSMPage {
        PageHeader header;
        uint32_t total_pages;
        uint32_t free_pages;
        uint32_t next_fsm_page;  // For future expansion
        uint8_t bitmap[];        // Variable length bitmap
    };
    
    static constexpr uint32_t FSM_PAGE_ID = 2;  // FSM is always page 2
};

} // namespace core
} // namespace scratchbird