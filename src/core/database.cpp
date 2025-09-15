#include "scratchbird/core/config.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/debug.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <cstring>
#include <chrono>
#include <iostream>
#include <filesystem>
#include <string>

namespace scratchbird {
namespace core {

Database::~Database() {
    close();
}

void Database::close() {
    // Shut down transaction manager first
    if (transaction_manager_) {
        delete transaction_manager_;
        transaction_manager_ = nullptr;
    }
    
    // Shut down storage engine
    if (storage_engine_) {
        delete storage_engine_;
        storage_engine_ = nullptr;
    }
    
    // Shut down catalog manager
    if (catalog_manager_) {
        delete catalog_manager_;
        catalog_manager_ = nullptr;
    }
    
    // Flush page manager before shutting down buffer pool
    if (page_manager_) {
        ErrorContext ctx;
        page_manager_->flush(&ctx);
    }

    // Shut down buffer pool (flushes dirty pages)
    if (buffer_pool_) {
        buffer_pool_->shutdown();
        delete buffer_pool_;
        buffer_pool_ = nullptr;
    }
    
    // Now delete page manager
    if (page_manager_) {
        delete page_manager_;
        page_manager_ = nullptr;
    }
    
    if (header_) {
        delete[] reinterpret_cast<uint8_t*>(header_);
        header_ = nullptr;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

Status Database::create_catalog_page(int fd, uint32_t page_size, uint8_t* page_buffer, const UuidV7Bytes& db_uuid, uint64_t micros, ErrorContext* ctx) {
    memset(page_buffer, 0, page_size);
    PageHeader* catalog_header = reinterpret_cast<PageHeader*>(page_buffer);
    
    catalog_header->magic = kMagicSBRD;
    catalog_header->version = 1;
    catalog_header->page_type = PAGE_TYPE_SYSTEM_CATALOG;
    catalog_header->page_size = page_size;
    catalog_header->page_id = 1;
    catalog_header->flags = 0;
    catalog_header->lsn = 0;
    memcpy(catalog_header->database_uuid, db_uuid.bytes.data(), 16);
    catalog_header->generation = 1;
    catalog_header->free_space = page_size - sizeof(PageHeader) - sizeof(SystemCatalogEntry) * config::NUM_BASE_SCHEMAS;
    catalog_header->item_count = config::NUM_BASE_SCHEMAS;  // 8 base schemas
    catalog_header->free_offset = sizeof(PageHeader) + sizeof(SystemCatalogEntry) * config::NUM_BASE_SCHEMAS;
    catalog_header->special_size = 0;
    
    // Add system catalog entries for base schemas
    SystemCatalogEntry* entries = reinterpret_cast<SystemCatalogEntry*>(
        page_buffer + sizeof(PageHeader));
    
    // Define base schemas as per spec
    const char* schema_names[] = {
        "[root]", "[sys]", "[sec]", "[agents]", 
        "[app]", "[remote]", "[users]", "[roles]"
    };
    
    for (int i = 0; i < config::NUM_BASE_SCHEMAS; i++) {
        SystemCatalogEntry& entry = entries[i];
        
        // Generate UUID for this schema
        UuidV7Bytes schema_uuid = generate_uuid_v7();
        memcpy(entry.schema_uuid, schema_uuid.bytes.data(), 16);
        
        // Root has no parent, others have root as parent
        if (i == 0) {
            memset(entry.parent_uuid, 0, 16);
        } else {
            memcpy(entry.parent_uuid, entries[0].schema_uuid, 16);
        }
        
        strncpy(entry.name, schema_names[i], 63);
        entry.name[63] = '\0';
        entry.object_type = 0;  // Schema
        entry.object_count = 0;
        entry.created_time = static_cast<uint64_t>(micros);
    }
    
    // Calculate checksum for catalog page
    catalog_header->checksum = calculate_page_checksum(page_buffer, page_size);
    
    // Write catalog page
    ssize_t written = ::write(fd, page_buffer, page_size);
    if (written != static_cast<ssize_t>(page_size)) {
        SET_ERROR_CONTEXT(ctx, Status::IoError, "Failed to write catalog page");
        return Status::IoError;
    }
    
    return Status::Ok;
}

Status Database::create_fsm_page(int fd, uint32_t page_size, uint8_t* page_buffer, const UuidV7Bytes& db_uuid, ErrorContext* ctx) {
    memset(page_buffer, 0, page_size);
    PageHeader* fsm_header = reinterpret_cast<PageHeader*>(page_buffer);
    
    fsm_header->magic = kMagicSBRD;
    fsm_header->version = 1;
    fsm_header->page_type = PAGE_TYPE_FREE_SPACE_MAP;
    fsm_header->page_size = page_size;
    fsm_header->page_id = 2;
    fsm_header->flags = 0;
    fsm_header->lsn = 0;
    memcpy(fsm_header->database_uuid, db_uuid.bytes.data(), 16);
    fsm_header->generation = 1;
    
    // Initialize FSM data
    struct {
        uint32_t total_pages;
        uint32_t free_pages;
        uint32_t next_fsm_page;
        uint8_t bitmap[1];  // First byte of bitmap
    } *fsm_data = reinterpret_cast<decltype(fsm_data)>(page_buffer + sizeof(PageHeader));
    
    fsm_data->total_pages = 3;  // Header, catalog, FSM
    fsm_data->free_pages = 0;   // All system pages allocated
    fsm_data->next_fsm_page = 0;
    fsm_data->bitmap[0] = 0x07; // First 3 bits set (pages 0,1,2 allocated)
    
    // Update header fields
    fsm_header->free_space = page_size - sizeof(PageHeader) - sizeof(uint32_t) * 3 - 1;
    fsm_header->item_count = 1;
    fsm_header->free_offset = sizeof(PageHeader) + sizeof(uint32_t) * 3 + 1;
    fsm_header->special_size = 0;
    
    // Calculate checksum for FSM page
    fsm_header->checksum = calculate_page_checksum(page_buffer, page_size);
    
    // Write FSM page
    ssize_t written = ::write(fd, page_buffer, page_size);
    if (written != static_cast<ssize_t>(page_size)) {
        SET_ERROR_CONTEXT(ctx, Status::IoError, "Failed to write FSM page");
        return Status::IoError;
    }
    
    return Status::Ok;
}

Status Database::init_header_page(int fd, const std::string& path, uint32_t page_size, uint8_t* page_buffer, ErrorContext* ctx) {
    // Create database header
    DatabaseHeader* header = reinterpret_cast<DatabaseHeader*>(page_buffer);
    
    // Initialize page header
    header->page_header.magic = kMagicSBRD;
    header->page_header.version = 1;
    header->page_header.page_type = PAGE_TYPE_DATABASE_HEADER;
    header->page_header.page_size = page_size;
    header->page_header.page_id = 0;
    header->page_header.flags = 0;
    header->page_header.lsn = 0;
    
    // Generate and set database UUID
    UuidV7Bytes db_uuid = generate_uuid_v7();
    memcpy(header->page_header.database_uuid, db_uuid.bytes.data(), 16);
    
    // Set MVCC fields
    header->page_header.generation = 1;
    header->page_header.free_space = page_size - sizeof(DatabaseHeader);
    header->page_header.item_count = 0;
    header->page_header.free_offset = sizeof(DatabaseHeader);
    header->page_header.special_size = 0;
    
    // Initialize database identification with actual filename
    size_t last_slash = path.find_last_of("/\\\\");
    std::string db_name = (last_slash != std::string::npos) ? path.substr(last_slash + 1) : path;
    strncpy(header->db_name, db_name.c_str(), 31);
    header->db_name[31] = '\0';
    header->db_version = DB_VERSION_ALPHA_1_0_1;
    header->db_compat_version = DB_COMPAT_VERSION_ALPHA_1_0_1;
    
    // Get current time in microseconds
    uint64_t micros = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    header->creation_time = micros;
    header->last_checkpoint = 0;
    
    // Set configuration
    header->block_size = page_size;
    header->wal_level = 0;  // No WAL in Alpha
    header->max_connections = 1;
    header->encoding = 1;  // UTF-8
    header->locale = 0;
    header->timezone = 0;
    
    // Initialize file layout
    header->total_pages = 2;  // Start with header and catalog pages
    header->free_pages = 0;
    header->next_page_id = 2;
    header->system_catalog_page = 1;
    
    // Initialize transaction info
    header->next_transaction_id = 3;  // Start after FROZEN_XID (2)
    header->oldest_active_xid = 0;
    header->latest_completed_xid = 0;
    header->tip_root_page = 0;  // 0 means no TIP pages allocated yet
    
    // Calculate and set checksum
    header->page_header.checksum = calculate_page_checksum(page_buffer, page_size);
    
    // Write header page
    ssize_t written = ::write(fd, page_buffer, page_size);
    if (written != static_cast<ssize_t>(page_size)) {
        SET_ERROR_CONTEXT(ctx, Status::IoError, "Failed to write header page");
        return Status::IoError;
    }
    
    return Status::Ok;
}

Status Database::create(const std::string& path, uint32_t page_size, ErrorContext* ctx) {
    std::string canonical_path;
    Status status = Database::validate_db_path(path, canonical_path, ctx);
    if (status != Status::Ok) {
        // context is already set by validate_db_path
        return status;
    }
    
    // Validate page size
    if (!is_valid_alpha_page_size(page_size)) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "Invalid page size: must be 8192, 16384, 32768, 65536, or 131072");
        return Status::InvalidArgument;
    }
    
    // Check if file already exists
    struct stat st;
    if (stat(canonical_path.c_str(), &st) == 0) {
        SET_ERROR_CONTEXT(ctx, Status::FileExists, "Database file already exists");
        return Status::FileExists;
    }
    
    // Create and open file with exclusive create
    int fd = ::open(canonical_path.c_str(), O_RDWR | O_CREAT | O_EXCL, 0644);
    if (fd < 0) {
        SET_ERROR_CONTEXT(ctx, Status::IoError, "Failed to create database file");
        return Status::IoError;
    }
    
    // Lock file for exclusive access
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        ::close(fd);
        SET_ERROR_CONTEXT(ctx, Status::IoError, "Failed to lock database file");
        return Status::IoError;
    }
    
    // Allocate buffer for header page with OOM check
    auto page_buffer = std::make_unique<uint8_t[]>(page_size);
    if (!page_buffer) {
        ::close(fd);
        unlink(canonical_path.c_str());  // Clean up file on failure
        SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate memory for page buffer");
        return Status::OOM;
    }
    memset(page_buffer.get(), 0, page_size);
    
    status = init_header_page(fd, canonical_path, page_size, page_buffer.get(), ctx);
    if (status != Status::Ok) {
        ::close(fd);
        unlink(canonical_path.c_str());
        return status;
    }
    
    DatabaseHeader* header = reinterpret_cast<DatabaseHeader*>(page_buffer.get());
    UuidV7Bytes db_uuid;
    memcpy(db_uuid.bytes.data(), header->page_header.database_uuid, 16);
    uint64_t micros = header->creation_time;

    status = create_catalog_page(fd, page_size, page_buffer.get(), db_uuid, micros, ctx);
    if (status != Status::Ok) {
        ::close(fd);
        unlink(canonical_path.c_str());
        return status;
    }
    
    // Create FSM page (Page 2)
    status = create_fsm_page(fd, page_size, page_buffer.get(), db_uuid, ctx);
    if (status != Status::Ok) {
        ::close(fd);
        unlink(canonical_path.c_str());
        return status;
    }
    
    // Update database header with correct page count
    lseek(fd, 0, SEEK_SET);
    ::read(fd, page_buffer.get(), page_size);
    header = reinterpret_cast<DatabaseHeader*>(page_buffer.get());
    header->total_pages = 3;  // Now we have 3 pages
    header->page_header.checksum = calculate_page_checksum(page_buffer.get(), page_size);
    lseek(fd, 0, SEEK_SET);
    ::write(fd, page_buffer.get(), page_size);
    
    // Sync to disk
    fsync(fd);
    
    ::close(fd);
    
    return Status::Ok;
}

Status Database::open(const std::string& path, ErrorContext* ctx) {
    std::string canonical_path;
    Status status = Database::validate_db_path(path, canonical_path, ctx);
    if (status != Status::Ok) {
        return status;
    }
    
    // Close if already open
    close();
    
    // Open file
    fd_ = ::open(canonical_path.c_str(), O_RDWR);
    if (fd_ < 0) {
        SET_ERROR_CONTEXT(ctx, Status::FileNotFound, "Database file not found");
        return Status::FileNotFound;
    }
    
    // Lock file for exclusive access
    if (flock(fd_, LOCK_EX | LOCK_NB) != 0) {
        ::close(fd_);
        fd_ = -1;
        SET_ERROR_CONTEXT(ctx, Status::IoError, "Failed to lock database file");
        return Status::IoError;
    }
    
    // Read header to determine page size
    uint8_t temp_header[64];
    ssize_t bytes_read = ::read(fd_, temp_header, 64);
    if (bytes_read < 0) {
        ::close(fd_);
        fd_ = -1;
        SET_ERROR_CONTEXT(ctx, Status::IoError, "Failed to read database header");
        return Status::IoError;
    }
    if (bytes_read < 64) {
        ::close(fd_);
        fd_ = -1;
        SET_ERROR_CONTEXT(ctx, Status::IoError, "Short read: database file truncated");
        return Status::IoError;
    }
    
    PageHeader* ph = reinterpret_cast<PageHeader*>(temp_header);
    
    // Validate magic
    if (ph->magic != kMagicSBRD) {
        ::close(fd_);
        fd_ = -1;
        SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, "Invalid magic number in database header");
        return Status::PageCorrupt;
    }
    
    // Validate page size
    if (!is_valid_alpha_page_size(ph->page_size)) {
        ::close(fd_);
        fd_ = -1;
        SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, "Invalid page size in database header");
        return Status::PageCorrupt;
    }
    
    page_size_ = ph->page_size;
    
    // Allocate full header buffer with OOM check
    header_ = reinterpret_cast<DatabaseHeader*>(new(std::nothrow) uint8_t[page_size_]);
    if (!header_) {
        ::close(fd_);
        fd_ = -1;
        SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate memory for database header");
        return Status::OOM;
    }
    
    // Read full header page
    lseek(fd_, 0, SEEK_SET);
    bytes_read = ::read(fd_, header_, page_size_);
    if (bytes_read != static_cast<ssize_t>(page_size_)) {
        close();
        SET_ERROR_CONTEXT(ctx, Status::IoError, "Short read on full header");
        return Status::IoError;
    }
    
    // Validate header
    status = validate_header();
    if (status != Status::Ok) {
        close();
        return status;
    }
    
    // Store database UUID
    memcpy(db_uuid_.bytes.data(), header_->page_header.database_uuid, 16);
    path_ = canonical_path;
    
    // Initialize page manager
    page_manager_ = new(std::nothrow) PageManager(this, page_size_);
    if (!page_manager_) {
        close();
        SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate PageManager");
        return Status::OOM;
    }
    status = page_manager_->load(ctx);
    if (status != Status::Ok) {
        close();
        return status;
    }
    
    // Initialize buffer pool
    BufferPool::Config bp_config;
    bp_config.pool_size = config::DEFAULT_BUFFER_POOL_SIZE;
    bp_config.page_size = page_size_;
    buffer_pool_ = new(std::nothrow) BufferPool(this, bp_config);
    if (!buffer_pool_) {
        close();
        SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate BufferPool");
        return Status::OOM;
    }
    status = buffer_pool_->initialize(ctx);
    if (status != Status::Ok) {
        close();
        return status;
    }
    
    // Initialize catalog manager
    catalog_manager_ = new(std::nothrow) CatalogManager(this);
    if (!catalog_manager_) {
        close();
        SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate CatalogManager");
        return Status::OOM;
    }
    status = catalog_manager_->load(ctx);
    if (status != Status::Ok && status != Status::PageCorrupt) {
        // PageCorrupt means catalog not initialized yet, which is OK
        close();
        return status;
    }
    
    // Initialize storage engine
    storage_engine_ = new(std::nothrow) StorageEngine(this);
    if (!storage_engine_) {
        close();
        SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate StorageEngine");
        return Status::OOM;
    }
    
    // Initialize transaction manager
    transaction_manager_ = new(std::nothrow) TransactionManager(this);
    if (!transaction_manager_) {
        close();
        SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate TransactionManager");
        return Status::OOM;
    }
    status = transaction_manager_->load(ctx);
    if (status != Status::Ok) {
        close();
        return status;
    }
    
    DEBUG_LOG_DB("Database opened successfully");
    return Status::Ok;
}

Status Database::validate_db_path(const std::string& path, std::string& canonical_path, ErrorContext* ctx) {
    if (path.empty()) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidPath, "Database path cannot be empty.");
        return Status::InvalidPath;
    }

    char* real_path_buf = realpath(path.c_str(), NULL);
    if (real_path_buf == NULL) {
        // If realpath fails, it might be because the file doesn't exist yet (for create).
        // In that case, we'll resolve the directory path and append the filename.
        std::filesystem::path p(path);
        std::string parent_path_str = p.parent_path().string();
        if (parent_path_str.empty()) {
            parent_path_str = ".";
        }
        char* real_parent_path_buf = realpath(parent_path_str.c_str(), NULL);
        if (real_parent_path_buf == NULL) {
            SET_ERROR_CONTEXT(ctx, Status::InvalidPath, "Could not resolve database path directory.");
            return Status::InvalidPath;
        }
        canonical_path = std::string(real_parent_path_buf) + "/" + p.filename().string();
        free(real_parent_path_buf);
    } else {
        canonical_path = std::string(real_path_buf);
        free(real_path_buf);
    }

    char* cwd_buf = getcwd(NULL, 0);
    if (cwd_buf == NULL) {
        SET_ERROR_CONTEXT(ctx, Status::IoError, "Could not get current working directory.");
        return Status::IoError;
    }
    std::string cwd(cwd_buf);
    free(cwd_buf);

    if (canonical_path.rfind(cwd, 0) != 0) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidPath, "Database path is outside the allowed directory.");
        return Status::InvalidPath;
    }

    return Status::Ok;
}


Status Database::validate_header() {
    if (!header_) {
        return Status::PageCorrupt;
    }
    
    // Check magic
    if (header_->page_header.magic != kMagicSBRD) {
        return Status::PageCorrupt;
    }
    
    // Check page type
    if (header_->page_header.page_type != PAGE_TYPE_DATABASE_HEADER) {
        return Status::PageCorrupt;
    }
    
    // Check page ID
    if (header_->page_header.page_id != 0) {
        return Status::PageCorrupt;
    }
    
    // Check block size consistency
    if (header_->block_size != header_->page_header.page_size) {
        return Status::PageCorrupt;
    }
    
    // Validate checksum
    if (!validate_page_checksum(reinterpret_cast<uint8_t*>(header_), page_size_)) {
        return Status::ChecksumMismatch;
    }
    
    return Status::Ok;
}

Status Database::read_page(uint32_t page_id, void* buffer, ErrorContext* ctx) {
    if (fd_ < 0 || !buffer) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "Invalid arguments to read_page");
        return Status::InvalidArgument;
    }
    
    off_t offset = static_cast<off_t>(page_id) * page_size_;
    if (lseek(fd_, offset, SEEK_SET) != offset) {
        SET_ERROR_CONTEXT(ctx, Status::IoError, "Failed to seek to page");
        return Status::IoError;
    }
    
    ssize_t bytes_read = ::read(fd_, buffer, page_size_);
    if (bytes_read != static_cast<ssize_t>(page_size_)) {
        SET_ERROR_CONTEXT(ctx, Status::IoError, "Short read on page");
        return Status::IoError;
    }
    
    // Validate page
    PageHeader* header = reinterpret_cast<PageHeader*>(buffer);
    
    if (header->magic != kMagicSBRD) {
        SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, "Invalid magic number");
        return Status::PageCorrupt;
    }
    
    if (header->page_id != page_id) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Page ID mismatch: expected %u, got %u", page_id, header->page_id);
        SET_ERROR_CONTEXT(ctx, Status::PageCorrupt, msg);
        return Status::PageCorrupt;
    }
    
    if (!validate_page_checksum(reinterpret_cast<uint8_t*>(buffer), page_size_)) {
        SET_ERROR_CONTEXT(ctx, Status::ChecksumMismatch, "Checksum validation failed");
        return Status::ChecksumMismatch;
    }
    
    return Status::Ok;
}

Status Database::write_page(uint32_t page_id, const void* buffer, ErrorContext* ctx) {
    if (fd_ < 0 || !buffer) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "Invalid arguments to write_page");
        return Status::InvalidArgument;
    }
    
    // Update checksum before writing (const_cast is safe here as we own the buffer)
    uint8_t* page = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(buffer));
    PageHeader* header = reinterpret_cast<PageHeader*>(page);
    header->checksum = calculate_page_checksum(page, page_size_);
    
    off_t offset = static_cast<off_t>(page_id) * page_size_;
    if (lseek(fd_, offset, SEEK_SET) != offset) {
        SET_ERROR_CONTEXT(ctx, Status::IoError, "Failed to seek to page");
        return Status::IoError;
    }
    
    ssize_t bytes_written = ::write(fd_, buffer, page_size_);
    if (bytes_written != static_cast<ssize_t>(page_size_)) {
        SET_ERROR_CONTEXT(ctx, Status::IoError, "Short write on page");
        return Status::IoError;
    }
    
    return Status::Ok;
}

Status Database::sync(ErrorContext* ctx) {
    if (fd_ < 0) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "Database not open");
        return Status::InvalidArgument;
    }
    
    if (fsync(fd_) != 0) {
        SET_ERROR_CONTEXT(ctx, Status::IoError, "Failed to sync database file");
        return Status::IoError;
    }
    
    return Status::Ok;
}

Status Database::read_page_partial(uint32_t page_id, void* buffer, uint32_t size, 
                                 uint32_t offset, ErrorContext* ctx) {
    if (fd_ < 0 || !buffer) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "Invalid arguments to read_page_partial");
        return Status::InvalidArgument;
    }
    
    if (offset + size > page_size_) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "Partial read exceeds page boundary");
        return Status::InvalidArgument;
    }
    
    off_t file_offset = static_cast<off_t>(page_id) * page_size_ + offset;
    if (lseek(fd_, file_offset, SEEK_SET) != file_offset) {
        SET_ERROR_CONTEXT(ctx, Status::IoError, "Failed to seek to page for partial read");
        return Status::IoError;
    }
    
    ssize_t bytes_read = ::read(fd_, buffer, size);
    if (bytes_read != static_cast<ssize_t>(size)) {
        SET_ERROR_CONTEXT(ctx, Status::IoError, "Short read on partial page");
        return Status::IoError;
    }
    
    return Status::Ok;
}

Status Database::update_header_total_pages(uint32_t total_pages, ErrorContext* ctx) {
    if (fd_ < 0) {
        SET_ERROR_CONTEXT(ctx, Status::InvalidArgument, "Database not open");
        return Status::InvalidArgument;
    }
    
    // Update in-memory header
    if (header_) {
        header_->total_pages = total_pages;
    }
    
    // Pin header page through buffer pool to update it
    if (buffer_pool_) {
        void* header_buffer;
        Status status = buffer_pool_->pin_page(0, &header_buffer, ctx);
        if (status != Status::Ok) {
            return status;
        }
        
        DatabaseHeader* db_header = static_cast<DatabaseHeader*>(header_buffer);
        db_header->total_pages = total_pages;
        
        // Unpin as dirty
        buffer_pool_->unpin_page(0, true, ctx);
    }
    
    return Status::Ok;
}

} // namespace core
} // namespace scratchbird