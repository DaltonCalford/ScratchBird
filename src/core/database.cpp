#include "scratchbird/core/config.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/vacuum.h"
#include "scratchbird/core/clog.h"
#include "scratchbird/core/sweep_manager.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/long_transaction_monitor.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/system_uuids.h"
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


    namespace scratchbird::core
    {

        Database::Database() = default;

        Database::~Database()
        {
            close();
        }

        Database::Database(Database&&) noexcept = default;

        Database& Database::operator=(Database&&) noexcept = default;

        void Database::close()
        {
            // Stop long transaction monitor (before shutting down other components)
            if (long_transaction_monitor_ && long_transaction_monitor_->isMonitoring()) {
                ErrorContext ctx;
                long_transaction_monitor_->stopMonitoring(&ctx);
            }
            long_transaction_monitor_.reset();

            // Shut down garbage collector first (before sweep manager)
            garbage_collector_.reset();

            // Shut down sweep manager (before transaction manager)
            sweep_manager_.reset();

            // Shut down lock manager (before transaction manager)
            lock_manager_.reset();

            // Shut down ProcArray (before transaction manager)
            if (header_ && header_->proc_array_initialized) {
                ErrorContext ctx;
                shutdownProcArray(&ctx);
            }

            // Shut down transaction manager
            transaction_manager_.reset();

            // Shut down storage engine
            storage_engine_.reset();

            // Shut down catalog manager
            catalog_manager_.reset();

            // Flush page manager before shutting down buffer pool
            if (page_manager_ != nullptr)
            {
                ErrorContext ctx;
                page_manager_->flush(&ctx);
            }

            // Shut down buffer pool (flushes dirty pages)
            if (buffer_pool_ != nullptr)
            {
                buffer_pool_->shutdown();
            }
            buffer_pool_.reset();

            // Now delete page manager
            page_manager_.reset();

            if (header_ != nullptr)
            {
                delete[] reinterpret_cast<uint8_t *>(header_);
                header_ = nullptr;
            }
            if (fd_ >= 0)
            {
                ::close(fd_);
                fd_ = -1;
            }
        }

        auto Database::connect(std::unique_ptr<ConnectionContext>& connection_out,
                             ErrorContext* ctx) -> Status
        {
            if (!is_open())
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Database is not open");
                return Status::NOT_FOUND;
            }

            // Initialize ProcArray if not already done
            if (header_ && !header_->proc_array_initialized)
            {
                uint32_t max_backends = header_->max_backends;
                if (max_backends == 0)
                {
                    max_backends = config::DEFAULT_MAX_BACKENDS;
                }

                Status s = initializeProcArray(max_backends, ctx);
                if (s != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, s, "Failed to initialize ProcArray");
                    return s;
                }
            }

            // Register this backend and get a proc_id
            uint32_t proc_id = 0;
            Status s = ProcArrayManager::registerBackend(&proc_id, ctx);
            if (s != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, s, "Failed to register backend in ProcArray");
                return s;
            }

            // Create connection context
            try
            {
                auto connection = std::make_unique<ConnectionContext>(this, proc_id);

                // Initialize the connection (starts initial transaction)
                s = connection->initialize(ctx);
                if (s != Status::OK)
                {
                    // Unregister backend on failure
                    ProcArrayManager::unregisterBackend(proc_id, nullptr);
                    SET_ERROR_CONTEXT(ctx, s, "Failed to initialize connection context");
                    return s;
                }

                connection_out = std::move(connection);
                return Status::OK;
            }
            catch (const std::bad_alloc&)
            {
                // Unregister backend on OOM
                ProcArrayManager::unregisterBackend(proc_id, nullptr);
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate ConnectionContext");
                return Status::OOM;
            }
        }

                auto Database::create_catalog_page(int fd, uint32_t page_size, uint8_t *page_buffer,
                                             const ID &db_uuid, uint64_t micros,
                                             ErrorContext *ctx) -> Status
        {
            memset(page_buffer, 0, page_size);
            auto *catalog_header = reinterpret_cast<PageHeader *>(page_buffer);

            catalog_header->magic = K_MAGIC_SBRD;
            catalog_header->version = 1;
            catalog_header->page_type = PAGE_TYPE_SYSTEM_CATALOG;
            catalog_header->page_size = page_size;
            catalog_header->page_id = 1;
            catalog_header->flags = 0;
            catalog_header->lsn = 0;
            memcpy(catalog_header->database_uuid, db_uuid.bytes.data(), 16);
            catalog_header->generation = 1;
            catalog_header->free_space = page_size - sizeof(PageHeader) -
                                         sizeof(SystemCatalogEntry) * config::NUM_BASE_SCHEMAS;
            catalog_header->item_count = config::NUM_BASE_SCHEMAS; // 8 base schemas
            catalog_header->free_offset =
                sizeof(PageHeader) + sizeof(SystemCatalogEntry) * config::NUM_BASE_SCHEMAS;
            catalog_header->special_size = 0;

            // Add system catalog entries for base schemas
            auto *entries =
                reinterpret_cast<SystemCatalogEntry *>(page_buffer + sizeof(PageHeader));

            // Define base schemas as per spec
            const char *schema_names[] = {"[root]", "[sys]",    "[sec]",   "[agents]",
                                          "[app]",  "[remote]", "[users]", "[roles]"};

            for (int i = 0; i < config::NUM_BASE_SCHEMAS; i++)
            {
                SystemCatalogEntry &entry = entries[i];

                // Use fixed, well-known UUIDs for system schemas (not random)
                ID schema_uuid = system_uuids::SYSTEM_SCHEMA_UUIDS[i];
                memcpy(entry.schema_uuid, schema_uuid.bytes.data(), 16);

                // Root has no parent, others have root as parent
                if (i == 0)
                {
                    memset(entry.parent_uuid, 0, 16);
                }
                else
                {
                    memcpy(entry.parent_uuid, entries[0].schema_uuid, 16);
                }

                strncpy(entry.name, schema_names[i], 63);
                entry.name[63] = '\0';
                entry.object_type = 0; // Schema
                entry.object_count = 0;
                entry.created_time = micros;
            }

            // Calculate checksum for catalog page
            catalog_header->checksum = calculatePageChecksum(page_buffer, page_size);

            // Write catalog page
            ssize_t written = ::write(fd, page_buffer, page_size);
            if (written != static_cast<ssize_t>(page_size))
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write catalog page");
                return Status::IO_ERROR;
            }

            return Status::OK;
        }

        auto Database::create_fsm_page(int fd, uint32_t page_size, uint8_t *page_buffer,
                                         const ID &db_uuid, ErrorContext *ctx) -> Status
        {
            memset(page_buffer, 0, page_size);
            auto *fsm_header = reinterpret_cast<PageHeader *>(page_buffer);

            fsm_header->magic = K_MAGIC_SBRD;
            fsm_header->version = 1;
            fsm_header->page_type = PAGE_TYPE_FREE_SPACE_MAP;
            fsm_header->page_size = page_size;
            fsm_header->page_id = 2;
            fsm_header->flags = 0;
            fsm_header->lsn = 0;
            memcpy(fsm_header->database_uuid, db_uuid.bytes.data(), 16);
            fsm_header->generation = 1;

            // Initialize FSM data
            struct
            {
                uint32_t total_pages;
                uint32_t free_pages;
                uint32_t next_fsm_page;
                uint8_t bitmap[1]; // First byte of bitmap
            } *fsm_data = reinterpret_cast<decltype(fsm_data)>(page_buffer + sizeof(PageHeader));

            fsm_data->total_pages = 3; // Header, catalog, FSM
            fsm_data->free_pages = 0;  // All system pages allocated
            fsm_data->next_fsm_page = 0;
            fsm_data->bitmap[0] = 0x07; // First 3 bits set (pages 0,1,2 allocated)

            // Update header fields
            fsm_header->free_space = page_size - sizeof(PageHeader) - sizeof(uint32_t) * 3 - 1;
            fsm_header->item_count = 1;
            fsm_header->free_offset = sizeof(PageHeader) + sizeof(uint32_t) * 3 + 1;
            fsm_header->special_size = 0;

            // Calculate checksum for FSM page
            fsm_header->checksum = calculatePageChecksum(page_buffer, page_size);

            // Write FSM page
            ssize_t written = ::write(fd, page_buffer, page_size);
            if (written != static_cast<ssize_t>(page_size))
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write FSM page");
                return Status::IO_ERROR;
            }

            return Status::OK;
        }

        auto Database::init_header_page(int fd, const std::string &path, uint32_t page_size,
                                          uint8_t *page_buffer, ErrorContext *ctx) -> Status
        {
            // Create database header
            auto *header = reinterpret_cast<DatabaseHeader *>(page_buffer);

            // Initialize page header
            header->page_header.magic = K_MAGIC_SBRD;
            header->page_header.version = 1;
            header->page_header.page_type = PAGE_TYPE_DATABASE_HEADER;
            header->page_header.page_size = page_size;
            header->page_header.page_id = 0;
            header->page_header.flags = 0;
            header->page_header.lsn = 0;

            // Generate and set database UUID
            ID db_uuid = generateUuidV7();
            memcpy(header->page_header.database_uuid, db_uuid.bytes.data(), 16);

            // Set MVCC fields
            header->page_header.generation = 1;
            header->page_header.free_space = page_size - sizeof(DatabaseHeader);
            header->page_header.item_count = 0;
            header->page_header.free_offset = sizeof(DatabaseHeader);
            header->page_header.special_size = 0;

            // Initialize database identification with actual filename
            size_t last_slash = path.find_last_of("/\\\\");
            std::string db_name =
                (last_slash != std::string::npos) ? path.substr(last_slash + 1) : path;
            strncpy(header->db_name, db_name.c_str(), 31);
            header->db_name[31] = '\0';
            header->db_version = DB_VERSION_ALPHA_1_0_1;
            header->db_compat_version = DB_COMPAT_VERSION_ALPHA_1_0_1;

            // Get current time in microseconds
            uint64_t micros = std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
            header->creation_time = micros;
            header->last_checkpoint = 0;

            // Set configuration
            header->block_size = page_size;
            header->wal_level = 0; // No WAL in Alpha
            header->max_connections = 1;
            header->encoding = 1; // UTF-8
            header->locale = 0;
            header->timezone = 0;

            // Initialize file layout
            header->total_pages = 2; // Start with header and catalog pages
            header->free_pages = 0;
            header->next_page_id = 2;
            header->system_catalog_page = 1;

            // Initialize transaction info
            header->next_transaction_id = 3; // Start after FROZEN_XID (2)
            header->oldest_transaction_id = 3; // OIT - initially same as next
            header->oldest_active_xid = 0;     // OAT - 0 means no active transactions
            header->oldest_snapshot = 0;       // OST - 0 means no snapshot transactions
            header->latest_completed_xid = 0;
            header->tip_root_page = 0; // 0 means no TIP pages allocated yet

            // Calculate and set checksum
            header->page_header.checksum = calculatePageChecksum(page_buffer, page_size);

            // Write header page
            ssize_t written = ::write(fd, page_buffer, page_size);
            if (written != static_cast<ssize_t>(page_size))
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write header page");
                return Status::IO_ERROR;
            }

            return Status::OK;
        }

        auto Database::create(const std::string &path, uint32_t page_size, ErrorContext *ctx) -> Status
        {
            std::string canonical_path;
            Status status = Database::validate_db_path(path, canonical_path, ctx);
            if (status != Status::OK)
            {
                // context is already set by validate_db_path
                return status;
            }

            // Validate page size
            if (!isValidAlphaPageSize(page_size))
            {
                SET_ERROR_CONTEXT(
                    ctx, Status::INVALID_ARGUMENT,
                    "Invalid page size: must be 8192, 16384, 32768, 65536, or 131072");
                return Status::INVALID_ARGUMENT;
            }

            // Check if file already exists
            struct stat st;
            if (stat(canonical_path.c_str(), &st) == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS, "Database file already exists");
                return Status::FILE_EXISTS;
            }

            // Create and open file with exclusive create
            int fd = ::open(canonical_path.c_str(), O_RDWR | O_CREAT | O_EXCL, 0644);
            if (fd < 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to create database file");
                return Status::IO_ERROR;
            }

            // Lock file for exclusive access
            if (flock(fd, LOCK_EX | LOCK_NB) != 0)
            {
                ::close(fd);
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to lock database file");
                return Status::IO_ERROR;
            }

            // Allocate buffer for header page with OOM check
            auto page_buffer = std::make_unique<uint8_t[]>(page_size);
            if (!page_buffer)
            {
                ::close(fd);
                unlink(canonical_path.c_str()); // Clean up file on failure
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate memory for page buffer");
                return Status::OOM;
            }
            memset(page_buffer.get(), 0, page_size);

            status = init_header_page(fd, canonical_path, page_size, page_buffer.get(), ctx);
            if (status != Status::OK)
            {
                ::close(fd);
                unlink(canonical_path.c_str());
                return status;
            }

            auto *header = reinterpret_cast<DatabaseHeader *>(page_buffer.get());
            ID db_uuid;
            memcpy(db_uuid.bytes.data(), header->page_header.database_uuid, 16);
            uint64_t micros = header->creation_time;

            status = create_catalog_page(fd, page_size, page_buffer.get(), db_uuid, micros, ctx);
            if (status != Status::OK)
            {
                ::close(fd);
                unlink(canonical_path.c_str());
                return status;
            }

            // Create FSM page (Page 2)
            status = create_fsm_page(fd, page_size, page_buffer.get(), db_uuid, ctx);
            if (status != Status::OK)
            {
                ::close(fd);
                unlink(canonical_path.c_str());
                return status;
            }

            // Update database header with correct page count
            lseek(fd, 0, SEEK_SET);
            ssize_t bytes_read = ::read(fd, page_buffer.get(), page_size);
            if (bytes_read != static_cast<ssize_t>(page_size))
            {
                ::close(fd);
                unlink(canonical_path.c_str());
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read database header");
                return Status::IO_ERROR;
            }
            header = reinterpret_cast<DatabaseHeader *>(page_buffer.get());
            header->total_pages = 3; // Now we have 3 pages
            header->page_header.checksum = calculatePageChecksum(page_buffer.get(), page_size);
            lseek(fd, 0, SEEK_SET);
            ssize_t bytes_written = ::write(fd, page_buffer.get(), page_size);
            if (bytes_written != static_cast<ssize_t>(page_size))
            {
                ::close(fd);
                unlink(canonical_path.c_str());
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write database header");
                return Status::IO_ERROR;
            }

            // Sync to disk
            fsync(fd);

            ::close(fd);

            return Status::OK;
        }

        auto Database::open(const std::string &path, ErrorContext *ctx) -> Status
        {
            std::string canonical_path;
            Status status = Database::validate_db_path(path, canonical_path, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Close if already open
            close();

            // Open file
            fd_ = ::open(canonical_path.c_str(), O_RDWR);
            if (fd_ < 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND, "Database file not found");
                return Status::FILE_NOT_FOUND;
            }

            // Lock file for exclusive access
            if (flock(fd_, LOCK_EX | LOCK_NB) != 0)
            {
                ::close(fd_);
                fd_ = -1;
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to lock database file");
                return Status::IO_ERROR;
            }

            // Read header to determine page size
            uint8_t temp_header[64];
            ssize_t bytes_read = ::read(fd_, temp_header, 64);
            if (bytes_read < 0)
            {
                ::close(fd_);
                fd_ = -1;
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to read database header");
                return Status::IO_ERROR;
            }
            if (bytes_read < 64)
            {
                ::close(fd_);
                fd_ = -1;
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Short read: database file truncated");
                return Status::IO_ERROR;
            }

            auto *ph = reinterpret_cast<PageHeader *>(temp_header);

            // Validate magic
            if (ph->magic != K_MAGIC_SBRD)
            {
                ::close(fd_);
                fd_ = -1;
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Invalid magic number in database header");
                return Status::PAGE_CORRUPT;
            }

            // Validate page size
            if (!isValidAlphaPageSize(ph->page_size))
            {
                ::close(fd_);
                fd_ = -1;
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid page size in database header");
                return Status::PAGE_CORRUPT;
            }

            page_size_ = ph->page_size;

            // Allocate full header buffer with OOM check
            header_ = reinterpret_cast<DatabaseHeader *>(new (std::nothrow) uint8_t[page_size_]);
            if (header_ == nullptr)
            {
                ::close(fd_);
                fd_ = -1;
                SET_ERROR_CONTEXT(ctx, Status::OOM,
                                  "Failed to allocate memory for database header");
                return Status::OOM;
            }

            // Read full header page
            lseek(fd_, 0, SEEK_SET);
            bytes_read = ::read(fd_, header_, page_size_);
            if (bytes_read != static_cast<ssize_t>(page_size_))
            {
                close();
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Short read on full header");
                return Status::IO_ERROR;
            }

            // Validate header
            status = validate_header();
            if (status != Status::OK)
            {
                close();
                return status;
            }

            // Store database UUID
            memcpy(db_uuid_.bytes.data(), header_->page_header.database_uuid, 16);
            path_ = canonical_path;

            // Initialize page manager
            try {
                page_manager_ = std::make_unique<PageManager>(this, page_size_);
            } catch (const std::bad_alloc&) {
                close();
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate PageManager");
                return Status::OOM;
            }
            status = page_manager_->load(ctx);
            if (status != Status::OK)
            {
                close();
                return status;
            }

            // Initialize buffer pool
            BufferPool::Config bp_config;
            bp_config.pool_size = Config::getInstance().getUInt("memory", "buffer_pool_size", 128);
            bp_config.page_size = page_size_;
            try {
                buffer_pool_ = std::make_unique<BufferPool>(this, bp_config);
            } catch (const std::bad_alloc&) {
                close();
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate BufferPool");
                return Status::OOM;
            }
            status = buffer_pool_->initialize(ctx);
            if (status != Status::OK)
            {
                close();
                return status;
            }

            // Initialize catalog manager
            try {
                catalog_manager_ = std::make_unique<CatalogManager>(this);
            } catch (const std::bad_alloc&) {
                close();
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate CatalogManager");
                return Status::OOM;
            }
            status = catalog_manager_->load(ctx);
            if (status != Status::OK && status != Status::PAGE_CORRUPT)
            {
                // PageCorrupt means catalog not initialized yet, which is OK
                close();
                return status;
            }

            // Initialize storage engine
            try {
                storage_engine_ = std::make_unique<StorageEngine>(this);
            } catch (const std::bad_alloc&) {
                close();
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate StorageEngine");
                return Status::OOM;
            }

            // Initialize transaction manager
            try {
                transaction_manager_ = std::make_unique<TransactionManager>(this);
            } catch (const std::bad_alloc&) {
                close();
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate TransactionManager");
                return Status::OOM;
            }
            status = transaction_manager_->load(ctx);
            if (status != Status::OK)
            {
                close();
                return status;
            }

            // Initialize lock manager
            try {
                lock_manager_ = std::make_unique<LockManager>(this);
            } catch (const std::bad_alloc&) {
                close();
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate LockManager");
                return Status::OOM;
            }
            status = lock_manager_->initialize(ctx);
            if (status != Status::OK)
            {
                close();
                return status;
            }

            // Initialize vacuum manager
            try {
                vacuum_ = std::make_unique<Vacuum>(this);
            } catch (const std::bad_alloc&) {
                close();
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate Vacuum");
                return Status::OOM;
            }

            // Initialize CLOG manager
            try {
                clog_ = std::make_unique<Clog>(this);
            } catch (const std::bad_alloc&) {
                close();
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate Clog");
                return Status::OOM;
            }
            status = clog_->initialize(ctx);
            if (status != Status::OK) {
                close();
                return status;
            }

            // Initialize sweep manager
            try {
                sweep_manager_ = std::make_unique<SweepManager>(this);
            } catch (const std::bad_alloc&) {
                close();
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate SweepManager");
                return Status::OOM;
            }
            status = sweep_manager_->initialize(ctx);
            if (status != Status::OK) {
                close();
                return status;
            }

            // Initialize garbage collector
            try {
                garbage_collector_ = std::make_unique<GarbageCollector>(this);
            } catch (const std::bad_alloc&) {
                close();
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate GarbageCollector");
                return Status::OOM;
            }
            status = garbage_collector_->initialize(ctx);
            if (status != Status::OK) {
                close();
                return status;
            }

            // Initialize long transaction monitor
            try {
                long_transaction_monitor_ = std::make_unique<LongTransactionMonitor>(this);
            } catch (const std::bad_alloc&) {
                close();
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate LongTransactionMonitor");
                return Status::OOM;
            }
            status = long_transaction_monitor_->initialize(ctx);
            if (status != Status::OK) {
                close();
                return status;
            }

            // Start long transaction monitoring thread
            status = long_transaction_monitor_->startMonitoring(ctx);
            if (status != Status::OK) {
                close();
                return status;
            }

            DEBUG_LOG_DB("Database opened successfully");
            return Status::OK;
        }

        auto Database::validate_db_path(const std::string &path, std::string &canonical_path,
                                          ErrorContext *ctx) -> Status
        {
            if (path.empty())
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_PATH, "Database path cannot be empty.");
                return Status::INVALID_PATH;
            }

            char *real_path_buf = realpath(path.c_str(), nullptr);
            if (real_path_buf == nullptr)
            {
                // If realpath fails, it might be because the file doesn't exist yet (for create).
                // In that case, we'll resolve the directory path and append the filename.
                std::filesystem::path p(path);
                std::string parent_path_str = p.parent_path().string();
                if (parent_path_str.empty())
                {
                    parent_path_str = ".";
                }
                char *real_parent_path_buf = realpath(parent_path_str.c_str(), nullptr);
                if (real_parent_path_buf == nullptr)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_PATH,
                                      "Could not resolve database path directory.");
                    return Status::INVALID_PATH;
                }
                canonical_path = std::string(real_parent_path_buf) + "/" + p.filename().string();
                free(real_parent_path_buf);
            }
            else
            {
                canonical_path = std::string(real_path_buf);
                free(real_path_buf);
            }

            char *cwd_buf = getcwd(nullptr, 0);
            if (cwd_buf == nullptr)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Could not get current working directory.");
                return Status::IO_ERROR;
            }
            std::string cwd(cwd_buf);
            free(cwd_buf);

            if (canonical_path.rfind(cwd, 0) != 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_PATH,
                                  "Database path is outside the allowed directory.");
                return Status::INVALID_PATH;
            }

            return Status::OK;
        }

        auto Database::validate_header() -> Status
        {
            if (header_ == nullptr)
            {
                return Status::PAGE_CORRUPT;
            }

            // Check magic
            if (header_->page_header.magic != K_MAGIC_SBRD)
            {
                return Status::PAGE_CORRUPT;
            }

            // Check page type
            if (header_->page_header.page_type != PAGE_TYPE_DATABASE_HEADER)
            {
                return Status::PAGE_CORRUPT;
            }

            // Check page ID
            if (header_->page_header.page_id != 0)
            {
                return Status::PAGE_CORRUPT;
            }

            // Check block size consistency
            if (header_->block_size != header_->page_header.page_size)
            {
                return Status::PAGE_CORRUPT;
            }

            // Validate checksum
            if (!validatePageChecksum(reinterpret_cast<uint8_t *>(header_), page_size_))
            {
                return Status::CHECKSUM_MISMATCH;
            }

            return Status::OK;
        }

        auto Database::read_page(uint32_t page_id, void *buffer, ErrorContext *ctx) const -> Status
        {
            if (fd_ < 0 || (buffer == nullptr))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments to read_page");
                return Status::INVALID_ARGUMENT;
            }

            off_t offset = static_cast<off_t>(page_id) * page_size_;
            if (lseek(fd_, offset, SEEK_SET) != offset)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to seek to page");
                return Status::IO_ERROR;
            }

            ssize_t bytes_read = ::read(fd_, buffer, page_size_);
            if (bytes_read != static_cast<ssize_t>(page_size_))
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Short read on page");
                return Status::IO_ERROR;
            }

            // Validate page
            auto *header = reinterpret_cast<PageHeader *>(buffer);

            if (header->magic != K_MAGIC_SBRD)
            {
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid magic number");
                return Status::PAGE_CORRUPT;
            }

            if (header->page_id != page_id)
            {
                char msg[256];
                snprintf(msg, sizeof(msg), "Page ID mismatch: expected %u, got %u", page_id,
                         header->page_id);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, msg);
                return Status::PAGE_CORRUPT;
            }

            if (!validatePageChecksum(reinterpret_cast<uint8_t *>(buffer), page_size_))
            {
                SET_ERROR_CONTEXT(ctx, Status::CHECKSUM_MISMATCH, "Checksum validation failed");
                return Status::CHECKSUM_MISMATCH;
            }

            return Status::OK;
        }

        auto Database::write_page(uint32_t page_id, const void *buffer, ErrorContext *ctx) -> Status
        {
            if (fd_ < 0 || (buffer == nullptr))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments to write_page");
                return Status::INVALID_ARGUMENT;
            }

            // Update checksum before writing (const_cast is safe here as we own the buffer)
            auto *page = const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(buffer));
            auto *header = reinterpret_cast<PageHeader *>(page);
            header->checksum = calculatePageChecksum(page, page_size_);

            off_t offset = static_cast<off_t>(page_id) * page_size_;
            if (lseek(fd_, offset, SEEK_SET) != offset)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to seek to page");
                return Status::IO_ERROR;
            }

            ssize_t bytes_written = ::write(fd_, buffer, page_size_);
            if (bytes_written != static_cast<ssize_t>(page_size_))
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Short write on page");
                return Status::IO_ERROR;
            }

            return Status::OK;
        }

        auto Database::sync(ErrorContext *ctx) const -> Status
        {
            if (fd_ < 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not open");
                return Status::INVALID_ARGUMENT;
            }

            if (fsync(fd_) != 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to sync database file");
                return Status::IO_ERROR;
            }

            return Status::OK;
        }

        auto Database::read_page_partial(uint32_t page_id, void *buffer, uint32_t size,
                                           uint32_t offset, ErrorContext *ctx) const -> Status
        {
            if (fd_ < 0 || (buffer == nullptr))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Invalid arguments to read_page_partial");
                return Status::INVALID_ARGUMENT;
            }

            if (offset + size > page_size_)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Partial read exceeds page boundary");
                return Status::INVALID_ARGUMENT;
            }

            off_t file_offset = (static_cast<off_t>(page_id) * page_size_) + offset;
            if (lseek(fd_, file_offset, SEEK_SET) != file_offset)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to seek to page for partial read");
                return Status::IO_ERROR;
            }

            ssize_t bytes_read = ::read(fd_, buffer, size);
            if (bytes_read != static_cast<ssize_t>(size))
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Short read on partial page");
                return Status::IO_ERROR;
            }

            return Status::OK;
        }

        auto Database::update_header_total_pages(uint32_t total_pages, ErrorContext *ctx) -> Status
        {
            if (fd_ < 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not open");
                return Status::INVALID_ARGUMENT;
            }

            // Update in-memory header
            if (header_ != nullptr)
            {
                header_->total_pages = total_pages;
            }

            // Pin header page through buffer pool to update it
            if (buffer_pool_ != nullptr)
            {
                void *header_buffer;
                Status status = buffer_pool_->pinPage(0, &header_buffer, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
                db_header->total_pages = total_pages;

                // Unpin as dirty
                buffer_pool_->unpinPage(0, true, ctx);
            }

            return Status::OK;
        }

        auto Database::initializeProcArray(uint32_t max_backends, ErrorContext* ctx) -> Status
        {
            if (!is_open()) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not open");
                return Status::INVALID_ARGUMENT;
            }

            if (header_->proc_array_initialized) {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "ProcArray already initialized");
                return Status::INVALID_ARGUMENT;
            }

            // Initialize ProcArray
            Status status = ProcArrayManager::initialize(this, max_backends, ctx);
            if (status != Status::OK) {
                return status;
            }

            // Update database header
            header_->max_backends = max_backends;
            header_->proc_array_initialized = 1;

            // Persist header changes
            void *header_buffer;
            status = buffer_pool_->pinPage(0, &header_buffer, ctx);
            if (status != Status::OK) {
                ProcArrayManager::shutdown(ctx);
                return status;
            }

            auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
            db_header->max_backends = max_backends;
            db_header->proc_array_initialized = 1;

            buffer_pool_->unpinPage(0, true, ctx);

            return sync(ctx);
        }

        auto Database::shutdownProcArray(ErrorContext* ctx) -> Status
        {
            if (!header_->proc_array_initialized) {
                return Status::OK;
            }

            Status status = ProcArrayManager::shutdown(ctx);
            if (status != Status::OK) {
                return status;
            }

            // Update header
            header_->proc_array_initialized = 0;

            return Status::OK;
        }

    } // namespace scratchbird::core
