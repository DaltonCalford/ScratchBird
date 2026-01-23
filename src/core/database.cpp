#include "scratchbird/core/config.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/tid_resolver.h" // Sprint 4 Task 5.4.2
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/vacuum.h"
#include "scratchbird/core/clog.h"
#include "scratchbird/core/sweep_manager.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/long_transaction_monitor.h"
#include "scratchbird/core/job_scheduler.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/encryption_key_manager.h"
#include "scratchbird/core/audit_logger.h"
#include "scratchbird/core/table_stats_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/permission_cache.h" // Security Phase 3.2.3
#include "scratchbird/core/password_hash.h"
#include "scratchbird/core/debug.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/optimizer/statistics_manager.h"
#include "scratchbird/security/scram_auth.h"
#include <nlohmann/json.hpp>
#include <openssl/md5.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <cstring>
#include <chrono>
#include <iostream>
#include <filesystem>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>

namespace scratchbird::core
{
    namespace {
    constexpr uint32_t kSysarchScramIterations = 4096;
    constexpr const char* kSysarchUser = "SYSARCH";
    constexpr const char* kSysarchPassword = "ScratchBirdBeta1!";

    std::string toHexLower(const unsigned char* data, size_t len)
    {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (size_t i = 0; i < len; ++i)
        {
            oss << std::setw(2) << static_cast<int>(data[i]);
        }
        return oss.str();
    }

    std::string computePgMd5StoredHash(const std::string& username,
                                       const std::string& password)
    {
        std::string input = password + username;
        unsigned char hash[MD5_DIGEST_LENGTH];
        MD5(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);
        return "md5" + toHexLower(hash, MD5_DIGEST_LENGTH);
    }

    core::Status buildPasswordHashPayload(const std::string& username,
                                          const std::string& password,
                                          std::string& out)
    {
        using json = nlohmann::json;
        json payload = json::object();
        payload["bcrypt"] = core::PasswordHash::hashPassword(password);
        payload["md5"] = computePgMd5StoredHash(username, password);

        json scram = json::object();
        auto add_scram = [&](security::ScramAlgorithm algo, const char* key) -> core::Status
        {
            std::vector<uint8_t> salt;
            std::vector<uint8_t> stored_key;
            std::vector<uint8_t> server_key;
            auto status = security::generateScramCredentials(
                password, algo, kSysarchScramIterations, salt, stored_key, server_key);
            if (status != core::Status::OK)
            {
                return status;
            }

            json entry = json::object();
            entry["iterations"] = kSysarchScramIterations;
            entry["salt"] = security::base64Encode(salt);
            entry["stored_key"] = security::base64Encode(stored_key);
            entry["server_key"] = security::base64Encode(server_key);
            scram[key] = entry;
            return core::Status::OK;
        };

        auto status = add_scram(security::ScramAlgorithm::SHA_256, "sha256");
        if (status != core::Status::OK)
        {
            return status;
        }
        status = add_scram(security::ScramAlgorithm::SHA_512, "sha512");
        if (status != core::Status::OK)
        {
            return status;
        }

        payload["scram"] = scram;
        out = payload.dump();
        return core::Status::OK;
    }

    core::Status ensureSysarchUser(CatalogManager* catalog, ErrorContext* ctx)
    {
        std::string password_hash;
        auto status = buildPasswordHashPayload(kSysarchUser, kSysarchPassword, password_hash);
        if (status != Status::OK)
        {
            if (ctx)
            {
                ctx->set(status, "Failed to build SYSARCH password hash",
                         __FILE__, __LINE__, __func__);
            }
            return status;
        }

        ID user_id;
        status = catalog->ensureUserExists(kSysarchUser, password_hash, ID(), true, user_id, ctx);
        return status;
    }
    } // namespace

    Database::Database()
    {
        table_stats_manager_ = std::make_unique<TableStatsManager>(this);
    }

    Database::~Database()
    {
        close();
    }

    // NOTE: Move operations deleted in header because Database contains std::mutex (non-movable)

    void Database::close()
    {
        // Stop long transaction monitor (before shutting down other components)
        if (long_transaction_monitor_ && long_transaction_monitor_->isMonitoring())
        {
            ErrorContext ctx;
            long_transaction_monitor_->stopMonitoring(&ctx);
        }
        long_transaction_monitor_.reset();

        if (job_scheduler_)
        {
            job_scheduler_->stop();
        }
        job_scheduler_.reset();

        {
            std::lock_guard<std::mutex> lock(dormant_mutex_);
            // Tear down dormant contexts while lock/txn managers are still available.
            dormant_contexts_.clear();
        }
        {
            std::lock_guard<std::mutex> lock(connection_registry_mutex_);
            connection_registry_.clear();
        }

        // Shut down domain manager
        domain_manager_.reset();

        // Shut down encryption key manager
        encryption_key_manager_.reset();

        // Shut down audit logger before catalog manager
        audit_logger_.reset();

        // Shut down garbage collector first (before sweep manager)
        garbage_collector_.reset();

        // Shut down sweep manager (before transaction manager)
        sweep_manager_.reset();

        // Shut down lock manager (before transaction manager)
        lock_manager_.reset();

        // Shut down ProcArray (before transaction manager)
        if (header_ && header_->proc_array_initialized)
        {
            ErrorContext ctx;
            shutdownProcArray(&ctx);
        }

        // Shut down transaction manager
        transaction_manager_.reset();

        // Shut down storage engine
        storage_engine_.reset();

        // Shut down catalog manager
        catalog_manager_.reset();

        table_stats_manager_.reset();

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

        // Flush the database header page (page 0) before closing
        // The header is stored in header_buffer_ and may have been modified during operation
        // (e.g., next_xid, next_oid, catalog_initialized, etc.)
        // write_page() will recalculate the checksum before writing
        if (header_buffer_ && fd_ >= 0)
        {
            ErrorContext ctx;
            write_page(0, header_buffer_.get(), &ctx);
            // Sync to ensure data is persisted
            sync(&ctx);
        }

        // LOW-1 FIX: header_buffer_ automatically cleaned up by std::unique_ptr
        header_ = nullptr;        // Invalidate pointer (was pointing into header_buffer_)
        header_buffer_.reset();   // Release memory (automatic, but explicit for clarity)

        if (fd_ >= 0)
        {
            ::close(fd_);
            fd_ = -1;
        }
    }

    auto Database::connect(std::unique_ptr<ConnectionContext> &connection_out, ErrorContext *ctx)
        -> Status
    {
        if (!is_open())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Database is not open");
            return Status::NOT_FOUND;
        }

        // Initialize ProcArray if not already done
        if (header_)
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

            connection->setRoleSwitchPolicy(role_switch_policy_);

            connection_out = std::move(connection);
            return Status::OK;
        }
        catch (const std::bad_alloc &)
        {
            // Unregister backend on OOM
            ProcArrayManager::unregisterBackend(proc_id, nullptr);
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate ConnectionContext");
            return Status::OOM;
        }
    }

    void Database::registerConnectionContext(ConnectionContext* ctx)
    {
        if (!ctx)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(connection_registry_mutex_);
        connection_registry_[ctx->getProcId()] = ctx;
    }

    void Database::unregisterConnectionContext(ConnectionContext* ctx)
    {
        if (!ctx)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(connection_registry_mutex_);
        auto it = connection_registry_.find(ctx->getProcId());
        if (it != connection_registry_.end() && it->second == ctx)
        {
            connection_registry_.erase(it);
        }
    }

    void Database::rebindConnectionContext(uint32_t proc_id, ConnectionContext* ctx)
    {
        if (!ctx)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(connection_registry_mutex_);
        connection_registry_[proc_id] = ctx;
    }

    std::vector<Database::ConnectionIoSnapshot> Database::snapshotConnectionIoStats() const
    {
        std::vector<ConnectionIoSnapshot> out;
        std::lock_guard<std::mutex> lock(connection_registry_mutex_);
        out.reserve(connection_registry_.size());
        for (const auto& [proc_id, ctx] : connection_registry_)
        {
            if (!ctx)
            {
                continue;
            }
            ConnectionIoSnapshot snap;
            snap.proc_id = proc_id;
            snap.session_id = ctx->effectiveSessionId();
            snap.transaction_id = ctx->getCurrentXid();
            snap.statement_id = ctx->currentStatementId();
            snap.statement_active = ctx->statementIoActive();
            snap.connection_io = ctx->snapshotConnectionIoStats();
            snap.transaction_io = ctx->snapshotTransactionIoStats();
            snap.statement_io = ctx->snapshotStatementIoStats();
            out.push_back(snap);
        }
        return out;
    }

    Status Database::detachToDormant(std::unique_ptr<ConnectionContext> &connection,
                                     ID &dormant_id_out,
                                     ErrorContext *ctx)
    {
        if (!connection)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Connection context is null");
            return Status::INVALID_ARGUMENT;
        }

        auto *catalog = catalog_manager_.get();
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        CatalogManager::DormantTransactionInfo info;
        info.attachment_id = connection->attachmentId();
        info.proc_id = connection->getProcId();
        info.txn_id = connection->getCurrentXid();
        info.session_id = connection->protocolSessionId();
        info.user_id = connection->getCurrentUserId();
        info.session_user_id = connection->getSessionUserId();
        info.role_id = connection->getActiveRoleId();
        info.isolation_level = static_cast<uint8_t>(connection->getIsolationLevel());
        info.access_mode = connection->isReadOnly()
            ? CatalogManager::DormantAccessMode::READ_ONLY
            : CatalogManager::DormantAccessMode::READ_WRITE;
        info.wait_mode = connection->getWaitForLocks()
            ? CatalogManager::DormantWaitMode::WAIT
            : CatalogManager::DormantWaitMode::NO_WAIT;
        info.autocommit_mode = connection->autocommitMode();
        info.lock_timeout_seconds = connection->getLockTimeout();
        info.current_schema_id = connection->getCurrentSchemaId();
        info.session_settings = connection->sessionSettingsJson();
        info.last_statement_text = connection->lastStatementText();
        info.last_statement_hash = connection->lastStatementHash();
        info.last_statement_type = static_cast<CatalogManager::DormantStatementType>(
            connection->lastStatementType());
        info.last_statement_status = static_cast<CatalogManager::DormantStatementStatus>(
            connection->lastStatementStatus());
        info.state = CatalogManager::DormantTransactionState::DORMANT;
        info.start_time = static_cast<uint64_t>(connection->getTransactionStartTime().count());

        uint64_t now_micros = std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
        info.last_activity_time = connection->lastActivityTime();
        if (info.last_activity_time == 0)
        {
            info.last_activity_time = now_micros;
        }
        info.dormant_since = now_micros;

        uint64_t lease_seconds = Config::getInstance().getUInt(
            "transactions", "dormant_lease_seconds",
            config::DEFAULT_DORMANT_TXN_LEASE_SECONDS);
        if (lease_seconds == 0)
        {
            info.lease_expires_at = 0;
        }
        else
        {
            info.lease_expires_at = now_micros + lease_seconds * 1000000ULL;
        }

        info.last_statement_time = connection->lastStatementTime();
        info.last_rows_affected = connection->lastRowsAffected();
        info.last_error_code = connection->lastErrorCode();
        info.last_sqlstate = connection->lastSqlstate();
        info.server_instance_id = server_instance_id_;
        info.is_valid = true;

        Status status = catalog->createDormantTransaction(info, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        dormant_id_out = info.dormant_id;

        DormantContextEntry entry;
        entry.dormant_id = info.dormant_id;
        entry.lease_expires_at = info.lease_expires_at;
        entry.connection = std::move(connection);

        {
            std::lock_guard<std::mutex> lock(dormant_mutex_);
            // Keep the ConnectionContext alive so locks + ProcArray visibility remain intact.
            dormant_contexts_.emplace(entry.dormant_id, std::move(entry));
        }

        return Status::OK;
    }

    Status Database::reattachDormant(const ID &dormant_id,
                                     std::unique_ptr<ConnectionContext> &connection_out,
                                     ErrorContext *ctx)
    {
        std::lock_guard<std::mutex> lock(dormant_mutex_);

        auto it = dormant_contexts_.find(dormant_id);
        if (it == dormant_contexts_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Dormant transaction not found");
            return Status::NOT_FOUND;
        }

        if (catalog_manager_)
        {
            CatalogManager::DormantTransactionInfo info;
            Status status = catalog_manager_->getDormantTransaction(dormant_id, info, ctx);
            if (status == Status::OK)
            {
                if (info.server_instance_id != server_instance_id_)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "Dormant transaction belongs to a different server instance");
                    return Status::INVALID_ARGUMENT;
                }

                info.state = CatalogManager::DormantTransactionState::REATTACHED;
                info.last_activity_time = std::chrono::duration_cast<std::chrono::microseconds>(
                                              std::chrono::system_clock::now().time_since_epoch())
                                              .count();
                catalog_manager_->updateDormantTransaction(info, ctx);
            }
        }

        connection_out = std::move(it->second.connection);
        dormant_contexts_.erase(it);
        return Status::OK;
    }

    auto Database::create_catalog_page(int fd, uint32_t page_size, uint8_t *page_buffer,
                                       const ID &db_uuid, uint64_t micros, ErrorContext *ctx)
        -> Status
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
        catalog_header->free_space =
            page_size - sizeof(PageHeader) - sizeof(SystemCatalogEntry) * config::NUM_BASE_SCHEMAS;
        catalog_header->item_count = config::NUM_BASE_SCHEMAS; // 8 base schemas
        catalog_header->free_offset =
            sizeof(PageHeader) + sizeof(SystemCatalogEntry) * config::NUM_BASE_SCHEMAS;
        catalog_header->special_size = 0;

        // Add system catalog entries for base schemas
        auto *entries = reinterpret_cast<SystemCatalogEntry *>(page_buffer + sizeof(PageHeader));

        // Define base schemas as per spec
        const char *schema_names[] = {"[root]", "[sys]",    "[sec]",   "[agents]",
                                      "[app]",  "[remote]", "[users]", "[roles]"};

        for (int i = 0; i < config::NUM_BASE_SCHEMAS; i++)
        {
            SystemCatalogEntry &entry = entries[i];

            // Root schema UUID aligns with the database UUID; others are generated per database.
            ID schema_uuid = (i == 0) ? db_uuid : generateUuidV7();
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
        // EXCEPTION SAFETY (ERROR-CRITICAL-2 Priority 3): Protect string operations
        try
        {
            size_t last_slash = path.find_last_of("/\\\\");
            std::string db_name =
                (last_slash != std::string::npos) ? path.substr(last_slash + 1) : path;
            strncpy(header->db_name, db_name.c_str(), 31);
            header->db_name[31] = '\0';
        }
        catch (const std::bad_alloc &)
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM,
                              "Out of memory extracting database name from path");
            return Status::OOM;
        }
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
        // Use DEFAULT_INITIAL_XID + 1 for next_xid so that DEFAULT_INITIAL_XID is a valid fallback
        // This ensures tuples inserted without a proper transaction (fallback to DEFAULT_INITIAL_XID)
        // will pass visibility checks (xid < next_xid)
        header->next_transaction_id = config::DEFAULT_INITIAL_XID + 1;
        header->oldest_transaction_id = config::DEFAULT_INITIAL_XID; // OIT - lowest valid user XID
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
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
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
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                SET_ERROR_CONTEXT(ctx, Status::LOCK_CONFLICT,
                    "Database file is already in use by another process");
            } else {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to lock database file");
            }
            return Status::LOCK_CONFLICT;
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
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                SET_ERROR_CONTEXT(ctx, Status::LOCK_CONFLICT,
                    "Database file is already in use by another process");
            } else {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to lock database file");
            }
            return Status::LOCK_CONFLICT;
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
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid magic number in database header");
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

        // LOW-1 FIX: Allocate full header buffer using std::unique_ptr for RAII
        try
        {
            header_buffer_ = std::make_unique<uint8_t[]>(page_size_);
        }
        catch (const std::bad_alloc &)
        {
            ::close(fd_);
            fd_ = -1;
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate memory for database header");
            return Status::OOM;
        }
        header_ = reinterpret_cast<DatabaseHeader *>(header_buffer_.get());

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
        status = validate_header(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Store database UUID
        memcpy(db_uuid_.bytes.data(), header_->page_header.database_uuid, 16);
        // Per-process instance identifier (used to invalidate stale dormant records on restart).
        server_instance_id_ = generateUuidV7();
        path_ = canonical_path;

        // Initialize logger
        Logger::getInstance().initialize();

        // Initialize page manager
        try
        {
            page_manager_ = std::make_unique<PageManager>(this, page_size_);
        }
        catch (const std::bad_alloc &)
        {
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

        // Reconstruct FSM from actual pages (MGA-style recovery)
        // This ensures FSM is always consistent with actual page state,
        // supporting full transaction recovery without WAL
        status = page_manager_->reconstructFromPages(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Initialize buffer pool
        BufferPool::Config bp_config;
        bp_config.pool_size = Config::getInstance().getUInt("memory", "buffer_pool_size", 128);
        bp_config.page_size = page_size_;
        try
        {
            buffer_pool_ = std::make_unique<BufferPool>(this, bp_config);
        }
        catch (const std::bad_alloc &)
        {
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
        try
        {
            catalog_manager_ = std::make_unique<CatalogManager>(this);
        }
        catch (const std::bad_alloc &)
        {
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
        if (status == Status::OK)
        {
            // On restart we cannot reattach previous in-memory contexts, so purge stale records.
            std::vector<CatalogManager::DormantTransactionInfo> dormants;
            Status list_status = catalog_manager_->listDormantTransactions(dormants, ctx);
            if (list_status == Status::OK)
            {
                for (const auto &entry : dormants)
                {
                    if (entry.server_instance_id != server_instance_id_)
                    {
                        catalog_manager_->deleteDormantTransaction(entry.dormant_id, ctx);
                    }
                }
            }
        }

        // Initialize audit logger
        try
        {
            audit_logger_ = std::make_unique<AuditLogger>(catalog_manager_.get());
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate AuditLogger");
            return Status::OOM;
        }

        // Initialize storage engine
        try
        {
            storage_engine_ = std::make_unique<StorageEngine>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate StorageEngine");
            return Status::OOM;
        }

        // Initialize transaction manager
        try
        {
            transaction_manager_ = std::make_unique<TransactionManager>(this);
        }
        catch (const std::bad_alloc &)
        {
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

        status = catalog_manager_->initializePolicyToastIfNeeded(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }
        status = ensureSysarchUser(catalog_manager_.get(), ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Initialize TID resolver (Sprint 4 Task 5.4.2)
        try
        {
            tid_resolver_ = std::make_unique<TIDResolver>();
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate TIDResolver");
            return Status::OOM;
        }

        // Initialize lock manager
        try
        {
            lock_manager_ = std::make_unique<LockManager>(this);
        }
        catch (const std::bad_alloc &)
        {
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
        try
        {
            vacuum_ = std::make_unique<Vacuum>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate Vacuum");
            return Status::OOM;
        }

        // Initialize CLOG manager
        try
        {
            clog_ = std::make_unique<Clog>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate Clog");
            return Status::OOM;
        }
        status = clog_->initialize(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Mark DEFAULT_INITIAL_XID as committed in CLOG
        // This allows tuples inserted without a proper transaction (using fallback XID)
        // to be visible after recovery/restart
        status = clog_->setStatus(config::DEFAULT_INITIAL_XID, ClogStatus::COMMITTED, ctx);
        if (status != Status::OK)
        {
            LOG_WARNING(STORAGE, "Failed to mark DEFAULT_INITIAL_XID as committed in CLOG");
            // Non-fatal - continue initialization
        }

        // Initialize sweep manager
        try
        {
            sweep_manager_ = std::make_unique<SweepManager>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate SweepManager");
            return Status::OOM;
        }
        status = sweep_manager_->initialize(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Initialize garbage collector
        try
        {
            garbage_collector_ = std::make_unique<GarbageCollector>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate GarbageCollector");
            return Status::OOM;
        }
        status = garbage_collector_->initialize(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Initialize long transaction monitor
        try
        {
            long_transaction_monitor_ = std::make_unique<LongTransactionMonitor>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate LongTransactionMonitor");
            return Status::OOM;
        }
        status = long_transaction_monitor_->initialize(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Start long transaction monitoring thread
        status = long_transaction_monitor_->startMonitoring(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Initialize domain manager
        try
        {
            domain_manager_ = std::make_unique<DomainManager>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate DomainManager");
            return Status::OOM;
        }
        status = domain_manager_->initialize(ctx);
        if (status != Status::OK)
        {
            // Domain catalog not initialized yet is OK
            if (status != Status::NOT_FOUND && status != Status::PAGE_CORRUPT)
            {
                close();
                return status;
            }
        }
        else
        {
            status = domain_manager_->load(ctx);
            if (status != Status::OK)
            {
                close();
                return status;
            }
        }

        // Initialize encryption key manager
        try
        {
            encryption_key_manager_ = std::make_unique<EncryptionKeyManager>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate EncryptionKeyManager");
            return Status::OOM;
        }
        status = encryption_key_manager_->initialize(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Initialize optimizer runtime components (Phase 1, Task 1.3)
        try
        {
            statistics_manager_ = std::make_unique<optimizer::StatisticsManager>(this);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate StatisticsManager");
            return Status::OOM;
        }

        // Initialize permission cache (Security Phase 3.2.3)
        try
        {
            permission_cache_ = std::make_unique<PermissionCache>(
                1000,                           // max_entries
                std::chrono::seconds(60));      // TTL
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate PermissionCache");
            return Status::OOM;
        }

        // Initialize job scheduler (WS-4 Scheduler)
        try
        {
            JobScheduler::Config sched_config;
            sched_config.polling_interval_seconds =
                Config::getInstance().getUInt("scheduler", "polling_interval_seconds", 10);
            sched_config.max_jobs_per_tick =
                Config::getInstance().getUInt("scheduler", "max_jobs_per_tick", 16);
            sched_config.cron_fallback_seconds =
                Config::getInstance().getUInt("scheduler", "cron_fallback_seconds", 60);
            job_scheduler_ = std::make_unique<JobScheduler>(this, sched_config);
        }
        catch (const std::bad_alloc &)
        {
            close();
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate JobScheduler");
            return Status::OOM;
        }
        status = job_scheduler_->start(ctx);
        if (status != Status::OK)
        {
            close();
            return status;
        }

        // Initialize virtual catalog handlers (information_schema, pg_catalog, mysql, firebird).
        scratchbird::catalog::initializeVirtualCatalogs(catalog_manager_.get());

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

        // EXCEPTION SAFETY (ERROR-CRITICAL-2 Priority 3): Protect path string operations
        std::string cwd;
        try
        {
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

            // Note: We no longer restrict paths to current working directory
            // The server process needs to open databases in any location specified by the user
            // Security is enforced at the OS level through file permissions
        }
        catch (const std::bad_alloc &)
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM,
                              "Out of memory during path validation");
            return Status::OOM;
        }

        // Basic validation: ensure path is absolute after resolution
        // and doesn't contain suspicious patterns
        if (canonical_path.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_PATH,
                              "Database path resolved to empty string.");
            return Status::INVALID_PATH;
        }

        // Ensure the parent directory exists and is writable
        std::filesystem::path p(canonical_path);
        std::filesystem::path parent_dir = p.parent_path();
        if (!parent_dir.empty() && !std::filesystem::exists(parent_dir))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_PATH,
                              "Parent directory does not exist.");
            return Status::INVALID_PATH;
        }

        return Status::OK;
    }

    auto Database::validate_header(ErrorContext *ctx) -> Status
    {
        if (header_ == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Database header is null");
            return Status::PAGE_CORRUPT;
        }

        // Check magic
        if (header_->page_header.magic != K_MAGIC_SBRD)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid magic number in database header");
            return Status::PAGE_CORRUPT;
        }

        // Check page type
        if (header_->page_header.page_type != PAGE_TYPE_DATABASE_HEADER)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid page type in database header");
            return Status::PAGE_CORRUPT;
        }

        // Check page ID
        if (header_->page_header.page_id != 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Database header page ID must be 0");
            return Status::PAGE_CORRUPT;
        }

        // Check block size consistency
        if (header_->block_size != header_->page_header.page_size)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Block size inconsistency in database header");
            return Status::PAGE_CORRUPT;
        }

        // Validate checksum
        if (!validatePageChecksum(reinterpret_cast<uint8_t *>(header_), page_size_))
        {
            SET_ERROR_CONTEXT(ctx, Status::CHECKSUM_MISMATCH, "Database header checksum validation failed");
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

        // NEW ISSUE FIX: Use pread() instead of lseek()+read() for thread-safe I/O
        // pread() is atomic and doesn't modify the file offset, preventing race conditions
        // when multiple threads access different pages concurrently
        off_t offset = static_cast<off_t>(page_id) * page_size_;
        ssize_t bytes_read = ::pread(fd_, buffer, page_size_, offset);
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

        // NEW ISSUE FIX: Use pwrite() instead of lseek()+write() for thread-safe I/O
        // pwrite() is atomic and doesn't modify the file offset, preventing race conditions
        // when multiple threads write different pages concurrently (e.g., background flush + active writes)
        off_t offset = static_cast<off_t>(page_id) * page_size_;
        ssize_t bytes_written = ::pwrite(fd_, buffer, page_size_, offset);
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

    auto Database::read_page_partial(uint32_t page_id, void *buffer, uint32_t size, uint32_t offset,
                                     ErrorContext *ctx) const -> Status
    {
        if (fd_ < 0 || (buffer == nullptr))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Invalid arguments to read_page_partial");
            return Status::INVALID_ARGUMENT;
        }

        if (offset + size > page_size_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Partial read exceeds page boundary");
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

    // === PHASE 1, TASK 1.2.4: GPID-based I/O methods ===

    auto Database::read_page_global(GPID gpid, void *buffer, ErrorContext *ctx) const -> Status
    {
        if (fd_ < 0 || (buffer == nullptr))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments to read_page_global");
            return Status::INVALID_ARGUMENT;
        }

        // Extract tablespace_id and page_number from GPID
        uint16_t tablespace_id = getTablespaceID(gpid);
        uint64_t page_number = getPageNumber(gpid);

        // Get file descriptor for the tablespace
        int tablespace_fd = -1;
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            // Primary tablespace uses main database file descriptor
            tablespace_fd = fd_;

            // Validate page_number fits in uint32_t for primary tablespace
            if (page_number > UINT32_MAX)
            {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Page number %lu exceeds uint32_t maximum for primary tablespace",
                         page_number);
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, msg);
                return Status::INVALID_ARGUMENT;
            }
        }
        else
        {
            // Custom tablespace - get file descriptor from registry
            tablespace_fd = getTablespaceFd(tablespace_id);
            if (tablespace_fd < 0)
            {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Tablespace %u not found or not open", tablespace_id);
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, msg);
                return Status::INVALID_ARGUMENT;
            }
        }

        // Calculate offset in file (page_number * page_size_)
        off_t offset = static_cast<off_t>(page_number) * static_cast<off_t>(page_size_);

        // Read page from tablespace file
        ssize_t bytes_read = ::pread(tablespace_fd, buffer, page_size_, offset);
        if (bytes_read != static_cast<ssize_t>(page_size_))
        {
            char msg[256];
            if (bytes_read < 0)
            {
                snprintf(msg, sizeof(msg),
                         "Failed to read page %lu from tablespace %u: %s",
                         page_number, tablespace_id, std::strerror(errno));
            }
            else
            {
                snprintf(msg, sizeof(msg),
                         "Partial read: expected %u bytes, got %ld bytes for page %lu in tablespace %u",
                         page_size_, bytes_read, page_number, tablespace_id);
            }
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, msg);
            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    auto Database::write_page_global(GPID gpid, const void *buffer, ErrorContext *ctx) -> Status
    {
        if (buffer == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Buffer cannot be null");
            return Status::INVALID_ARGUMENT;
        }

        // Extract tablespace_id and page_number from GPID
        uint16_t tablespace_id = getTablespaceID(gpid);
        uint64_t page_number = getPageNumber(gpid);

        // Get file descriptor for the tablespace
        int tablespace_fd = -1;
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            // Primary tablespace uses main database file descriptor
            if (fd_ < 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not open");
                return Status::INVALID_ARGUMENT;
            }
            tablespace_fd = fd_;

            // Validate page_number fits in uint32_t for primary tablespace
            if (page_number > UINT32_MAX)
            {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Page number %lu exceeds uint32_t maximum for primary tablespace",
                         page_number);
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, msg);
                return Status::INVALID_ARGUMENT;
            }
        }
        else
        {
            // Custom tablespace - get file descriptor from registry
            tablespace_fd = getTablespaceFd(tablespace_id);
            if (tablespace_fd < 0)
            {
                char msg[256];
                snprintf(msg, sizeof(msg),
                         "Tablespace %u not found or not open", tablespace_id);
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, msg);
                return Status::INVALID_ARGUMENT;
            }
        }

        // Calculate offset in file (page_number * page_size_)
        off_t offset = static_cast<off_t>(page_number) * static_cast<off_t>(page_size_);

        // Write page to tablespace file
        ssize_t bytes_written = ::pwrite(tablespace_fd, buffer, page_size_, offset);
        if (bytes_written != static_cast<ssize_t>(page_size_))
        {
            char msg[256];
            if (bytes_written < 0)
            {
                snprintf(msg, sizeof(msg),
                         "Failed to write page %lu to tablespace %u: %s",
                         page_number, tablespace_id, std::strerror(errno));
            }
            else
            {
                snprintf(msg, sizeof(msg),
                         "Partial write: expected %u bytes, wrote %ld bytes for page %lu in tablespace %u",
                         page_size_, bytes_written, page_number, tablespace_id);
            }
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, msg);
            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    auto Database::allocate_page_id_global(uint16_t tablespace_id, GPID *gpid_out,
                                          ErrorContext *ctx) -> Status
    {
        if (!is_open())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not open");
            return Status::INVALID_ARGUMENT;
        }

        if (gpid_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "gpid_out cannot be null");
            return Status::INVALID_ARGUMENT;
        }

        // Delegate to PageManager for GPID allocation (handles both primary and custom tablespaces)
        return page_manager_->allocatePageInTablespace(tablespace_id, gpid_out, ctx);
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

    auto Database::initializeProcArray(uint32_t max_backends, ErrorContext *ctx) -> Status
    {
        if (!is_open())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not open");
            return Status::INVALID_ARGUMENT;
        }

        if (header_->proc_array_initialized)
        {
            uint32_t requested_max = header_->max_backends ? header_->max_backends : max_backends;
            return ProcArrayManager::initialize(this, requested_max, ctx);
        }

        // Initialize ProcArray
        Status status = ProcArrayManager::initialize(this, max_backends, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Update database header
        header_->max_backends = max_backends;
        header_->proc_array_initialized = 1;

        // Persist header changes
        void *header_buffer;
        status = buffer_pool_->pinPage(0, &header_buffer, ctx);
        if (status != Status::OK)
        {
            ProcArrayManager::shutdown(ctx);
            return status;
        }

        auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
        db_header->max_backends = max_backends;
        db_header->proc_array_initialized = 1;

        buffer_pool_->unpinPage(0, true, ctx);

        return sync(ctx);
    }

    auto Database::shutdownProcArray(ErrorContext *ctx) -> Status
    {
        if (!header_->proc_array_initialized)
        {
            return Status::OK;
        }

        Status status = ProcArrayManager::shutdown(ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Update header
        header_->proc_array_initialized = 0;

        return Status::OK;
    }

    auto Database::allocate_page_id(uint32_t *page_id_out, ErrorContext *ctx) -> Status
    {
        if (!is_open())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database not open");
            return Status::INVALID_ARGUMENT;
        }

        if (page_id_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "page_id_out cannot be null");
            return Status::INVALID_ARGUMENT;
        }

        if (header_ == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database header not loaded");
            return Status::INVALID_ARGUMENT;
        }

        // Atomically allocate a new page ID by incrementing next_page_id
        // NOTE: This is a simple sequential allocation strategy
        // Future enhancements: free list, page recycling, etc.
        uint32_t new_page_id = header_->next_page_id;

        // Check for overflow (page_id is uint32_t, max 4 billion pages)
        if (new_page_id == UINT32_MAX)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Page ID overflow: maximum number of pages reached");
            return Status::INVALID_ARGUMENT;
        }

        // Increment next_page_id for next allocation
        header_->next_page_id++;
        header_->total_pages++;

        // Persist the updated header to disk
        // Pin header page through buffer pool to update it
        if (buffer_pool_ != nullptr)
        {
            void *header_buffer;
            Status status = buffer_pool_->pinPage(0, &header_buffer, ctx);
            if (status != Status::OK)
            {
                // Rollback on failure
                header_->next_page_id--;
                header_->total_pages--;
                return status;
            }

            auto *db_header = static_cast<DatabaseHeader *>(header_buffer);
            db_header->next_page_id = header_->next_page_id;
            db_header->total_pages = header_->total_pages;

            // Unpin as dirty
            buffer_pool_->unpinPage(0, true, ctx);
        }

        *page_id_out = new_page_id;
        return Status::OK;
    }

    // ========================================================================
    // PHASE 1, TASK 1.3.4: Tablespace File Descriptor Management
    // ========================================================================

    Status Database::registerTablespaceFile(uint16_t tablespace_id, int fd, ErrorContext *ctx)
    {
        // Validate tablespace ID
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Cannot register tablespace 0 (primary database file)");
            return Status::INVALID_ARGUMENT;
        }

        if (fd < 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             ("Invalid file descriptor: " + std::to_string(fd)).c_str());
            return Status::INVALID_ARGUMENT;
        }

        // Thread-safe insertion
        std::lock_guard<std::mutex> lock(tablespace_mutex_);

        // Check if already registered
        if (tablespace_fds_.find(tablespace_id) != tablespace_fds_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::FILE_EXISTS,
                             ("Tablespace " + std::to_string(tablespace_id) +
                              " is already registered").c_str());
            return Status::FILE_EXISTS;
        }

        // Register file descriptor
        tablespace_fds_[tablespace_id] = fd;
        return Status::OK;
    }

    Status Database::unregisterTablespaceFile(uint16_t tablespace_id, ErrorContext *ctx)
    {
        // Validate tablespace ID
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                             "Cannot unregister tablespace 0 (primary database file)");
            return Status::INVALID_ARGUMENT;
        }

        // Thread-safe removal
        std::lock_guard<std::mutex> lock(tablespace_mutex_);

        auto it = tablespace_fds_.find(tablespace_id);
        if (it == tablespace_fds_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                             ("Tablespace " + std::to_string(tablespace_id) + " not found").c_str());
            return Status::NOT_FOUND;
        }

        // Close file descriptor
        int fd = it->second;
        if (fd >= 0)
        {
            ::close(fd);
        }

        // Remove from map
        tablespace_fds_.erase(it);
        return Status::OK;
    }

    int Database::getTablespaceFd(uint16_t tablespace_id) const
    {
        // PRIMARY_TABLESPACE_ID (0) uses primary database fd_
        if (tablespace_id == PRIMARY_TABLESPACE_ID)
        {
            return fd_;
        }

        // Thread-safe lookup
        std::lock_guard<std::mutex> lock(tablespace_mutex_);

        auto it = tablespace_fds_.find(tablespace_id);
        if (it == tablespace_fds_.end())
        {
            return -1; // Not found
        }

        return it->second;
    }

} // namespace scratchbird::core
