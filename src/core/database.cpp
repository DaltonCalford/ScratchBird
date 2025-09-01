#include "scratchbird/core/database.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <cstring>
#include <chrono>
#include <iostream>

namespace scratchbird {
namespace core {

Database::~Database() {
    close();
}

void Database::close() {
    if (header_) {
        delete[] reinterpret_cast<uint8_t*>(header_);
        header_ = nullptr;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

Status Database::create(const std::string& path, uint32_t page_size) {
    // Validate page size
    if (!is_valid_alpha_page_size(page_size)) {
        return Status::IoError;
    }
    
    // Check if file already exists
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return Status::FileExists;
    }
    
    // Create and open file with exclusive create
    int fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_EXCL, 0644);
    if (fd < 0) {
        return Status::IoError;
    }
    
    // Lock file for exclusive access
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        ::close(fd);
        return Status::IoError;
    }
    
    // Allocate buffer for header page
    uint8_t* page_buffer = new uint8_t[page_size];
    memset(page_buffer, 0, page_size);
    
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
    
    // Initialize database identification
    strncpy(header->db_name, "scratchbird.db", 31);
    header->db_name[31] = '\0';
    header->db_version = 0x00010001;  // v0.1.0.1
    header->db_compat_version = 0x00010001;
    
    // Get current time in microseconds
    auto now = std::chrono::system_clock::now();
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
    header->creation_time = static_cast<uint64_t>(micros);
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
    header->next_transaction_id = 1;
    header->oldest_active_xid = 0;
    header->latest_completed_xid = 0;
    
    // Calculate and set checksum
    header->page_header.checksum = calculate_page_checksum(page_buffer, page_size);
    
    // Write header page
    ssize_t written = ::write(fd, page_buffer, page_size);
    if (written != static_cast<ssize_t>(page_size)) {
        delete[] page_buffer;
        ::close(fd);
        unlink(path.c_str());
        return Status::IoError;
    }
    
    // Create system catalog page (Page 1)
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
    catalog_header->free_space = page_size - sizeof(PageHeader) - sizeof(SystemCatalogEntry) * 8;
    catalog_header->item_count = 8;  // 8 base schemas
    catalog_header->free_offset = sizeof(PageHeader) + sizeof(SystemCatalogEntry) * 8;
    catalog_header->special_size = 0;
    
    // Add system catalog entries for base schemas
    SystemCatalogEntry* entries = reinterpret_cast<SystemCatalogEntry*>(
        page_buffer + sizeof(PageHeader));
    
    // Define base schemas as per spec
    const char* schema_names[] = {
        "[root]", "[sys]", "[sec]", "[agents]", 
        "[app]", "[remote]", "[users]", "[roles]"
    };
    
    for (int i = 0; i < 8; i++) {
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
    written = ::write(fd, page_buffer, page_size);
    if (written != static_cast<ssize_t>(page_size)) {
        delete[] page_buffer;
        ::close(fd);
        unlink(path.c_str());
        return Status::IoError;
    }
    
    // Sync to disk
    fsync(fd);
    
    delete[] page_buffer;
    ::close(fd);
    
    return Status::Ok;
}

Status Database::open(const std::string& path) {
    // Close if already open
    close();
    
    // Open file
    fd_ = ::open(path.c_str(), O_RDWR);
    if (fd_ < 0) {
        return Status::FileNotFound;
    }
    
    // Lock file for exclusive access
    if (flock(fd_, LOCK_EX | LOCK_NB) != 0) {
        ::close(fd_);
        fd_ = -1;
        return Status::IoError;
    }
    
    // Read header to determine page size
    uint8_t temp_header[64];
    ssize_t bytes_read = ::read(fd_, temp_header, 64);
    if (bytes_read != 64) {
        ::close(fd_);
        fd_ = -1;
        return Status::IoError;
    }
    
    PageHeader* ph = reinterpret_cast<PageHeader*>(temp_header);
    
    // Validate magic
    if (ph->magic != kMagicSBRD) {
        ::close(fd_);
        fd_ = -1;
        return Status::PageCorrupt;
    }
    
    // Validate page size
    if (!is_valid_alpha_page_size(ph->page_size)) {
        ::close(fd_);
        fd_ = -1;
        return Status::PageCorrupt;
    }
    
    page_size_ = ph->page_size;
    
    // Allocate full header buffer
    header_ = reinterpret_cast<DatabaseHeader*>(new uint8_t[page_size_]);
    
    // Read full header page
    lseek(fd_, 0, SEEK_SET);
    bytes_read = ::read(fd_, header_, page_size_);
    if (bytes_read != static_cast<ssize_t>(page_size_)) {
        close();
        return Status::IoError;
    }
    
    // Validate header
    Status status = validate_header();
    if (status != Status::Ok) {
        close();
        return status;
    }
    
    // Store database UUID
    memcpy(db_uuid_.bytes.data(), header_->page_header.database_uuid, 16);
    path_ = path;
    
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

Status Database::read_page(uint32_t page_id, void* buffer) {
    if (fd_ < 0) {
        return Status::IoError;
    }
    
    off_t offset = static_cast<off_t>(page_id) * page_size_;
    if (lseek(fd_, offset, SEEK_SET) != offset) {
        return Status::IoError;
    }
    
    ssize_t bytes_read = ::read(fd_, buffer, page_size_);
    if (bytes_read != static_cast<ssize_t>(page_size_)) {
        return Status::IoError;
    }
    
    // Validate page
    PageHeader* header = reinterpret_cast<PageHeader*>(buffer);
    
    if (header->magic != kMagicSBRD) {
        return Status::PageCorrupt;
    }
    
    if (header->page_id != page_id) {
        return Status::PageCorrupt;
    }
    
    if (!validate_page_checksum(reinterpret_cast<uint8_t*>(buffer), page_size_)) {
        return Status::ChecksumMismatch;
    }
    
    return Status::Ok;
}

Status Database::write_page(uint32_t page_id, const void* buffer) {
    if (fd_ < 0) {
        return Status::IoError;
    }
    
    // Update checksum before writing
    uint8_t* page = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(buffer));
    PageHeader* header = reinterpret_cast<PageHeader*>(page);
    header->checksum = calculate_page_checksum(page, page_size_);
    
    off_t offset = static_cast<off_t>(page_id) * page_size_;
    if (lseek(fd_, offset, SEEK_SET) != offset) {
        return Status::IoError;
    }
    
    ssize_t bytes_written = ::write(fd_, buffer, page_size_);
    if (bytes_written != static_cast<ssize_t>(page_size_)) {
        return Status::IoError;
    }
    
    return Status::Ok;
}

} // namespace core
} // namespace scratchbird