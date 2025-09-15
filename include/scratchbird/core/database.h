#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/error_context.h"

namespace scratchbird {
namespace core {

// Database version constants
constexpr uint32_t DB_VERSION_ALPHA_1_0_1 = 0x00010001; // v0.1.0.1
constexpr uint32_t DB_COMPAT_VERSION_ALPHA_1_0_1 = 0x00010001; // v0.1.0.1

// Forward declarations
class PageManager;
class BufferPool;
class CatalogManager;
class StorageEngine;
class TransactionManager;

// Database header structure for Page 0
#pragma pack(push, 1)
struct DatabaseHeader {
    PageHeader page_header;      // Standard 64-byte header
    
    // Database identification (64 bytes)
    char     db_name[32];        // Database name (null-terminated)
    uint32_t db_version;         // ScratchBird version that created DB
    uint32_t db_compat_version;  // Minimum version that can read DB
    uint64_t creation_time;      // Unix timestamp (microseconds)
    uint64_t last_checkpoint;    // Last checkpoint timestamp
    uint64_t reserved1[2];       // Reserved for future use
    
    // Configuration (32 bytes)
    uint32_t block_size;         // Must match page_header.page_size
    uint32_t wal_level;          // WAL level (0=none for Alpha)
    uint32_t max_connections;    // Maximum connections
    uint32_t encoding;           // Database encoding (UTF8=1)
    uint32_t locale;             // Locale ID
    uint32_t timezone;           // Timezone offset
    uint32_t reserved2[2];       // Reserved
    
    // File layout (32 bytes)
    uint64_t total_pages;        // Total pages in main file
    uint64_t free_pages;         // Number of free pages
    uint64_t next_page_id;       // Next page ID to allocate
    uint64_t system_catalog_page; // Root of system catalog (usually 1)
    
    // Transaction info (32 bytes)
    uint64_t next_transaction_id; // Next transaction ID to assign
    uint64_t oldest_active_xid;   // Oldest active transaction
    uint64_t latest_completed_xid; // Latest completed transaction
    uint32_t tip_root_page;       // Root page of Transaction Inventory Pages
    uint32_t reserved3;           // Reserved
    
    // Checksums for critical data (16 bytes)
    uint32_t catalog_checksum;   // Checksum of system catalog
    uint32_t reserved4[3];       // Reserved
    
    // Padding to page boundary - calculated dynamically based on page_size
};
#pragma pack(pop)

// System catalog entry structure
struct SystemCatalogEntry {
    uint8_t  schema_uuid[16];    // UUID v7 for schema
    uint8_t  parent_uuid[16];    // Parent schema UUID (all zeros for root)
    char     name[64];            // Schema/table name
    uint32_t object_type;         // 0=schema, 1=table, 2=column
    uint32_t object_count;        // Number of child objects
    uint64_t created_time;        // Creation timestamp
};

// Database class for managing database files
class Database {
public:
    Database() = default;
    ~Database();  // Defined in cpp file due to unique_ptr of forward declared types
    
    // Create a new database file
    static Status create(const std::string& path, uint32_t page_size = 16384, ErrorContext* ctx = nullptr);
    
    // Open an existing database file
    Status open(const std::string& path, ErrorContext* ctx = nullptr);
    
    // Close the database
    void close();
    
    // Get database information
    bool is_open() const { return fd_ >= 0; }
    uint32_t page_size() const { return page_size_; }
    const UuidV7Bytes& uuid() const { return db_uuid_; }
    
    // Read/write pages
    Status read_page(uint32_t page_id, void* buffer, ErrorContext* ctx = nullptr);
    Status write_page(uint32_t page_id, const void* buffer, ErrorContext* ctx = nullptr);
    
    // Read partial page data
    Status read_page_partial(uint32_t page_id, void* buffer, uint32_t size, 
                           uint32_t offset, ErrorContext* ctx = nullptr);
    
    // Get page manager
    PageManager* page_manager() { return page_manager_; }
    
    // Get buffer pool
    BufferPool* buffer_pool() { return buffer_pool_; }
    
    // Get catalog manager
    CatalogManager* catalog_manager() { return catalog_manager_; }
    
    // Get storage engine
    StorageEngine* storage_engine() { return storage_engine_; }
    
    // Get transaction manager
    TransactionManager* transaction_manager() { return transaction_manager_; }
    
    // Get file descriptor (for internal use)
    int fd() const { return fd_; }
    
    // Sync database file to disk
    Status sync(ErrorContext* ctx = nullptr);
    
    // Update header total pages (for internal use by PageManager)
    Status update_header_total_pages(uint32_t total_pages, ErrorContext* ctx = nullptr);
    
private:
    int fd_ = -1;                    // File descriptor
    std::string path_;               // Database file path
    uint32_t page_size_ = 0;         // Page size
    UuidV7Bytes db_uuid_;            // Database UUID
    DatabaseHeader* header_ = nullptr; // Cached header
    
    // Forward declared pointers - managed manually to avoid header dependencies
    PageManager* page_manager_ = nullptr;  // Page allocation manager (owned)
    BufferPool* buffer_pool_ = nullptr;    // Buffer pool manager (owned)
    CatalogManager* catalog_manager_ = nullptr; // System catalog manager (owned)
    StorageEngine* storage_engine_ = nullptr; // Storage engine (owned)
    TransactionManager* transaction_manager_ = nullptr; // Transaction manager (owned)
    

    
    // Validate database header
    Status validate_header();

    // Create helpers
    static Status init_header_page(int fd, const std::string& path, uint32_t page_size, uint8_t* page_buffer, ErrorContext* ctx);
    static Status create_catalog_page(int fd, uint32_t page_size, uint8_t* page_buffer, const UuidV7Bytes& db_uuid, uint64_t micros, ErrorContext* ctx);
    static Status create_fsm_page(int fd, uint32_t page_size, uint8_t* page_buffer, const UuidV7Bytes& db_uuid, ErrorContext* ctx);
    static Status validate_db_path(const std::string& path, std::string& canonical_path, ErrorContext* ctx);
};

} // namespace core
} // namespace scratchbird