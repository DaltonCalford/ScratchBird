/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <vector>
#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/monitoring_stats.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/gpid.h"

namespace scratchbird
{
    namespace core
    {

        // Database version constants
        constexpr uint32_t DB_VERSION_ALPHA_1_0_1 = 0x00010001;        // v0.1.0.1
        constexpr uint32_t DB_COMPAT_VERSION_ALPHA_1_0_1 = 0x00010001; // v0.1.0.1

        // TOAST MGA Compliance - TupleHeader-based chunk format
        constexpr uint32_t DB_VERSION_ALPHA_1_8_2 = 0x00010802;        // v0.1.8.2 - TOAST MGA format
        constexpr uint32_t DB_COMPAT_VERSION_ALPHA_1_8_2 = 0x00010802; // Requires v0.1.8.2+

        // Current version (updated for TOAST MGA compliance - Phase 1 complete)
        constexpr uint32_t DB_VERSION_CURRENT = DB_VERSION_ALPHA_1_8_2;
        constexpr uint32_t DB_COMPAT_VERSION_CURRENT = DB_COMPAT_VERSION_ALPHA_1_8_2;

        // Forward declarations
        class PageManager;
        class BufferPool;
        class CatalogManager;
        class StorageEngine;
        class TransactionManager;
        class ProcArrayManager;
        class LockManager;
        class GcManager;
        class Clog;
        class ConnectionContext;
        class SweepManager;
        class GarbageCollector;
        class LongTransactionMonitor;
        class DomainManager;
        class EncryptionKeyManager;
        class AuditLogger;
        class TIDResolver; // Sprint 4 Task 5.4.2
        class PermissionCache; // Security Phase 3.2.3
        class TableStatsManager;
        class JobScheduler;

    } // namespace core

    // Forward declarations for optimizer components
    namespace optimizer
    {
        class StatisticsManager;
    }

    namespace core
    {

// Database header structure for Page 0
#pragma pack(push, 1)
        struct DatabaseHeader
        {
            PageHeader page_header; // Standard 64-byte header

            // Database identification (64 bytes)
            char db_name[32];           // Database name (null-terminated)
            uint32_t db_version;        // ScratchBird version that created DB
            uint32_t db_compat_version; // Minimum version that can read DB
            uint64_t creation_time;     // Unix timestamp (microseconds)
            uint64_t last_checkpoint;   // Last checkpoint timestamp
            uint64_t reserved1[2];      // Reserved for future use

            // Configuration (32 bytes)
            uint32_t block_size;      // Must match page_header.page_size
            uint32_t wal_level;       // WAL level (0=none for Alpha)
            uint32_t max_connections; // Maximum connections
            uint32_t encoding;        // Database encoding (UTF8=1)
            uint32_t locale;          // Locale ID
            uint32_t timezone;        // Timezone offset
            uint32_t reserved2[2];    // Reserved

            // File layout (32 bytes)
            uint64_t total_pages;         // Total pages in main file
            uint64_t free_pages;          // Number of free pages
            uint64_t next_page_id;        // Next page ID to allocate
            uint64_t system_catalog_page; // Root of system catalog (usually 1)

            // Transaction info (56 bytes)
            uint64_t next_transaction_id;    // Next transaction ID to assign (NEXT)
            uint64_t oldest_transaction_id;  // Oldest Interesting Transaction (OIT)
            uint64_t oldest_active_xid;      // Oldest Active Transaction (OAT)
            uint64_t oldest_snapshot;        // Oldest Snapshot Transaction (OST)
            uint64_t latest_completed_xid;   // Latest completed transaction
            uint32_t tip_root_page;          // Root page of Transaction Inventory Pages
            uint32_t max_backends;           // Maximum concurrent backends
            uint32_t proc_array_initialized; // 1 if ProcArray initialized
            uint32_t reserved3;              // Reserved

            // Checksums for critical data (16 bytes)
            uint32_t catalog_checksum; // Checksum of system catalog
            uint32_t reserved4[3];     // Reserved

            // Padding to page boundary - calculated dynamically based on page_size
        };
#pragma pack(pop)

        // System catalog entry structure
        struct SystemCatalogEntry
        {
            uint8_t schema_uuid[16]; // UUID v7 for schema
            uint8_t parent_uuid[16]; // Parent schema UUID (all zeros for root)
            char name[64];           // Schema/table name
            uint32_t object_type;    // 0=schema, 1=table, 2=column
            uint32_t object_count;   // Number of child objects
            uint64_t created_time;   // Creation timestamp
        };

        // Database class for managing database files
        class Database
        {
        public:
            Database();
            ~Database(); // Defined in cpp file due to unique_ptr of forward declared types

            // Explicitly delete copy operations (Database is non-copyable)
            Database(const Database &) = delete;
            Database &operator=(const Database &) = delete;

            // Delete move operations (Database contains std::mutex which is non-movable)
            Database(Database &&) noexcept = delete;
            Database &operator=(Database &&) noexcept = delete;

            // Create a new database file
            static Status create(const std::string &path, uint32_t page_size = 16384,
                                 ErrorContext *ctx = nullptr);

            // Open an existing database file
            Status open(const std::string &path, ErrorContext *ctx = nullptr);

            // Close the database
            void close();

            // Apply runtime scheduler configuration from Config
            Status applySchedulerConfig(ErrorContext *ctx = nullptr);

            // Create a new connection context
            // This registers a backend in ProcArray and creates a ConnectionContext
            // The caller is responsible for managing the ConnectionContext lifetime
            Status connect(std::unique_ptr<class ConnectionContext> &connection_out,
                           ErrorContext *ctx = nullptr);

            // Server instance ID (ephemeral, per process). Used to invalidate stale dormant records.
            const ID& server_instance_id() const
            {
                return server_instance_id_;
            }

            struct ConnectionIoSnapshot
            {
                uint32_t proc_id = 0;
                ID session_id{};
                uint64_t transaction_id = 0;
                uint64_t statement_id = 0;
                bool statement_active = false;
                IOStatsSnapshot connection_io;
                IOStatsSnapshot transaction_io;
                IOStatsSnapshot statement_io;
            };

            struct ConnectionSecuritySnapshot
            {
                uint32_t proc_id = 0;
                ID session_id{};
                uint64_t statement_id = 0;
                uint64_t statement_time = 0;
                int32_t statement_line = 0;
                int32_t statement_column = 0;
                std::vector<ConnectionContext::SecurityContext> security_stack;
            };

            // Dormant transaction handling (reattach support).
            // The ConnectionContext is retained to preserve locks and ProcArray visibility.
            Status detachToDormant(std::unique_ptr<class ConnectionContext> &connection,
                                   ID &dormant_id_out,
                                   ErrorContext *ctx = nullptr);

            Status reattachDormant(const ID &dormant_id,
                                   std::unique_ptr<class ConnectionContext> &connection_out,
                                   ErrorContext *ctx = nullptr);

            std::vector<ConnectionSecuritySnapshot> snapshotConnectionSecurityStacks() const;

            // Get database information
            bool is_open() const
            {
                return fd_ >= 0;
            }
            uint32_t page_size() const
            {
                return page_size_;
            }
            const ID &uuid() const
            {
                return db_uuid_;
            }
            uint64_t total_pages() const
            {
                return header_ ? header_->total_pages : 0;
            }

            // LSM Integration: Get database path (for LSM-Tree index directories)
            const std::string &path() const
            {
                return path_;
            }

            // === LEGACY API: tablespace 0 only ===
            // Read/write pages
            Status read_page(uint32_t page_id, void *buffer, ErrorContext *ctx = nullptr) const;
            Status write_page(uint32_t page_id, const void *buffer, ErrorContext *ctx = nullptr);

            // Read partial page data
            Status read_page_partial(uint32_t page_id, void *buffer, uint32_t size, uint32_t offset,
                                     ErrorContext *ctx = nullptr) const;

            // === NEW: GPID-based API (Phase 1, Task 1.2.4) ===

            /**
             * read_page_global - Read a page identified by GPID
             *
             * @param gpid Global Page ID of page to read
             * @param buffer Buffer to read page data into (must be page_size bytes)
             * @param ctx Error context
             * @return Status::OK on success, error status otherwise
             *
             * Supports both primary (tablespace 0) and custom tablespaces (1-65535).
             */
            Status read_page_global(GPID gpid, void *buffer, ErrorContext *ctx = nullptr) const;

            /**
             * write_page_global - Write a page identified by GPID
             *
             * @param gpid Global Page ID of page to write
             * @param buffer Buffer containing page data to write (must be page_size bytes)
             * @param ctx Error context
             * @return Status::OK on success, error status otherwise
             *
             * Supports both primary (tablespace 0) and custom tablespaces (1-65535).
             */
            Status write_page_global(GPID gpid, const void *buffer, ErrorContext *ctx = nullptr);

            // Get page manager
            PageManager *page_manager()
            {
                return page_manager_.get();
            }
            const PageManager *page_manager() const
            {
                return page_manager_.get();
            }

            // Get buffer pool
            BufferPool *buffer_pool()
            {
                return buffer_pool_.get();
            }
            const BufferPool *buffer_pool() const
            {
                return buffer_pool_.get();
            }

            // Get catalog manager
            CatalogManager *catalog_manager()
            {
                return catalog_manager_.get();
            }
            const CatalogManager *catalog_manager() const
            {
                return catalog_manager_.get();
            }

            // Get storage engine
            StorageEngine *storage_engine()
            {
                return storage_engine_.get();
            }
            const StorageEngine *storage_engine() const
            {
                return storage_engine_.get();
            }

            // Get optimizer statistics manager (Phase 1, Task 1.3)
            optimizer::StatisticsManager *statistics_manager()
            {
                return statistics_manager_.get();
            }
            const optimizer::StatisticsManager *statistics_manager() const
            {
                return statistics_manager_.get();
            }

            // Get table stats manager (monitoring)
            TableStatsManager *table_stats_manager()
            {
                return table_stats_manager_.get();
            }
            const TableStatsManager *table_stats_manager() const
            {
                return table_stats_manager_.get();
            }

            // Get permission cache (Security Phase 3.2.3)
            PermissionCache *permission_cache()
            {
                return permission_cache_.get();
            }
            const PermissionCache *permission_cache() const
            {
                return permission_cache_.get();
            }

            // Get job scheduler
            JobScheduler *job_scheduler()
            {
                std::lock_guard<std::mutex> lock(scheduler_mutex_);
                return job_scheduler_.get();
            }
            const JobScheduler *job_scheduler() const
            {
                std::lock_guard<std::mutex> lock(scheduler_mutex_);
                return job_scheduler_.get();
            }

            void registerConnectionContext(ConnectionContext* ctx);
            void unregisterConnectionContext(ConnectionContext* ctx);
            void rebindConnectionContext(uint32_t proc_id, ConnectionContext* ctx);
            std::vector<ConnectionIoSnapshot> snapshotConnectionIoStats() const;

            void setRoleSwitchPolicy(ConnectionContext::RoleSwitchPolicy policy)
            {
                role_switch_policy_ = policy;
            }
            ConnectionContext::RoleSwitchPolicy roleSwitchPolicy() const
            {
                return role_switch_policy_;
            }

            // Get transaction manager
            TransactionManager *transaction_manager()
            {
                return transaction_manager_.get();
            }
            const TransactionManager *transaction_manager() const
            {
                return transaction_manager_.get();
            }

            // Get TID resolver (Sprint 4 Task 5.4.2)
            TIDResolver *tid_resolver()
            {
                return tid_resolver_.get();
            }
            const TIDResolver *tid_resolver() const
            {
                return tid_resolver_.get();
            }

            // Timezone context for connections
            // Get/set connection timezone (defaults to database timezone, then UTC)
            uint16_t getConnectionTimezone() const
            {
                return connection_timezone_;
            }
            void setConnectionTimezone(uint16_t tz_id)
            {
                connection_timezone_ = tz_id;
            }
            uint16_t getDatabaseTimezone() const
            {
                return header_ ? header_->timezone : 1;
            } // 1 = UTC

            // Get lock manager
            LockManager *lock_manager()
            {
                return lock_manager_.get();
            }
            const LockManager *lock_manager() const
            {
                return lock_manager_.get();
            }

            // Get GC manager (ScratchBird MGA GC, not PostgreSQL VACUUM)
            GcManager *gc_manager()
            {
                return gc_manager_.get();
            }
            const GcManager *gc_manager() const
            {
                return gc_manager_.get();
            }

            // Get CLOG (commit log) manager
            Clog *clog()
            {
                return clog_.get();
            }
            const Clog *clog() const
            {
                return clog_.get();
            }

            // Get sweep manager
            SweepManager *sweep_manager()
            {
                return sweep_manager_.get();
            }
            const SweepManager *sweep_manager() const
            {
                return sweep_manager_.get();
            }

            // Get garbage collector
            GarbageCollector *garbage_collector()
            {
                return garbage_collector_.get();
            }
            const GarbageCollector *garbage_collector() const
            {
                return garbage_collector_.get();
            }

            // Get long transaction monitor
            LongTransactionMonitor *long_transaction_monitor()
            {
                return long_transaction_monitor_.get();
            }
            const LongTransactionMonitor *long_transaction_monitor() const
            {
                return long_transaction_monitor_.get();
            }

            // Get domain manager
            DomainManager *domain_manager()
            {
                return domain_manager_.get();
            }
            const DomainManager *domain_manager() const
            {
                return domain_manager_.get();
            }

            // Get encryption key manager
            EncryptionKeyManager *encryption_key_manager()
            {
                return encryption_key_manager_.get();
            }
            const EncryptionKeyManager *encryption_key_manager() const
            {
                return encryption_key_manager_.get();
            }

            // Get audit logger
            AuditLogger *audit_logger()
            {
                return audit_logger_.get();
            }
            const AuditLogger *audit_logger() const
            {
                return audit_logger_.get();
            }

            // Initialize ProcArray for multi-connection support
            Status initializeProcArray(uint32_t max_backends, ErrorContext *ctx = nullptr);

            // Shutdown ProcArray
            Status shutdownProcArray(ErrorContext *ctx = nullptr);

            // Get file descriptor (for internal use)
            int fd() const
            {
                return fd_;
            }

            // Sync database file to disk
            Status sync(ErrorContext *ctx = nullptr) const;

            // Update header total pages (for internal use by PageManager)
            Status update_header_total_pages(uint32_t total_pages, ErrorContext *ctx = nullptr);

            // Update header next transaction id (for internal use by TransactionManager)
            Status update_header_next_xid(uint64_t next_xid, ErrorContext *ctx = nullptr);

            // === LEGACY API: tablespace 0 only ===
            // Allocate a new page ID (for internal use by BufferPool/PageManager)
            // Thread-safe: atomically increments next_page_id in database header
            Status allocate_page_id(uint32_t *page_id_out, ErrorContext *ctx = nullptr);

            // === NEW: GPID-based API (Phase 1, Task 1.2.4) ===

            /**
             * allocate_page_id_global - Allocate a new page ID in a specific tablespace
             *
             * @param tablespace_id Tablespace ID (0 = primary, 1-65535 = custom)
             * @param gpid_out Output GPID of allocated page
             * @param ctx Error context
             * @return Status::OK on success, error status otherwise
             *
             * Note: Custom tablespaces are supported when the tablespace file is registered.
             *
             * Thread-safe: atomically increments next_page_id in database header.
             */
            Status allocate_page_id_global(uint16_t tablespace_id, GPID *gpid_out,
                                          ErrorContext *ctx = nullptr);

            // === NEW: Tablespace File Management (Phase 1, Task 1.3.4) ===

            /**
             * registerTablespaceFile - Register an open tablespace file descriptor
             *
             * @param tablespace_id Tablespace ID
             * @param fd File descriptor for the tablespace file
             * @param ctx Error context
             * @return Status::OK on success, error if tablespace_id already registered
             *
             * Thread-safe: Acquires tablespace_mutex_.
             */
            Status registerTablespaceFile(uint16_t tablespace_id, int fd,
                                         ErrorContext *ctx = nullptr);

            /**
             * unregisterTablespaceFile - Unregister and close tablespace file descriptor
             *
             * @param tablespace_id Tablespace ID
             * @param ctx Error context
             * @return Status::OK on success, error if tablespace_id not found
             *
             * Thread-safe: Acquires tablespace_mutex_.
             * Note: Caller responsible for flushing before calling this.
             */
            Status unregisterTablespaceFile(uint16_t tablespace_id,
                                           ErrorContext *ctx = nullptr);

            /**
             * getTablespaceFd - Get file descriptor for a tablespace
             *
             * @param tablespace_id Tablespace ID (0 = primary, 1-65535 = custom)
             * @return File descriptor, or -1 if not found
             *
             * Thread-safe: Acquires tablespace_mutex_.
             */
            int getTablespaceFd(uint16_t tablespace_id) const;

        private:
            int fd_ = -1;                                    // File descriptor (primary database)
            std::string path_;                               // Database file path
            uint32_t page_size_ = 0;                         // Page size
            ID db_uuid_;                                     // Database UUID
            ID server_instance_id_;                          // Ephemeral server instance UUID
            std::unique_ptr<uint8_t[]> header_buffer_;       // Header buffer (LOW-1 FIX: Modern RAII)
            DatabaseHeader *header_ = nullptr;               // Cached header (points into header_buffer_)
            uint16_t connection_timezone_ = 1;               // Connection timezone (1 = UTC)

            // Forward declared pointers - managed via unique_ptr for RAII
            std::unique_ptr<PageManager> page_manager_;       // Page allocation manager (owned)
            std::unique_ptr<BufferPool> buffer_pool_;         // Buffer pool manager (owned)
            std::unique_ptr<CatalogManager> catalog_manager_; // System catalog manager (owned)
            std::unique_ptr<StorageEngine> storage_engine_;   // Storage engine (owned)
            std::unique_ptr<TransactionManager> transaction_manager_; // Transaction manager (owned)
            std::unique_ptr<TIDResolver> tid_resolver_;               // TID resolver (Sprint 4, owned)
            std::unique_ptr<LockManager> lock_manager_;               // Lock manager (owned)
            std::unique_ptr<GcManager> gc_manager_;                  // GC manager (owned)
            std::unique_ptr<Clog> clog_;                              // Commit log manager (owned)
            std::unique_ptr<SweepManager> sweep_manager_;             // Sweep manager (owned)
            std::unique_ptr<GarbageCollector> garbage_collector_;     // Garbage collector (owned)
            std::unique_ptr<LongTransactionMonitor>
                long_transaction_monitor_;                     // Long transaction monitor (owned)
            std::unique_ptr<JobScheduler> job_scheduler_;     // Scheduler job runner (owned)
            mutable std::mutex scheduler_mutex_;

            // Optimizer runtime components (Phase 1, Task 1.3)
            std::unique_ptr<optimizer::StatisticsManager> statistics_manager_; // Statistics manager (owned)
            std::unique_ptr<TableStatsManager> table_stats_manager_;           // Runtime monitoring stats (owned)
            std::unique_ptr<DomainManager> domain_manager_;                   // Domain manager (owned)
            std::unique_ptr<EncryptionKeyManager> encryption_key_manager_;    // Encryption key manager (owned)
            std::unique_ptr<AuditLogger> audit_logger_;                       // Audit logger (owned)

            // Security components (Phase 3.2.3)
            std::unique_ptr<PermissionCache> permission_cache_; // Permission cache (owned)
            ConnectionContext::RoleSwitchPolicy role_switch_policy_ =
                ConnectionContext::RoleSwitchPolicy::ERROR;

            // Dormant connection registry (reattach support)
            struct DormantContextEntry
            {
                ID dormant_id;
                uint64_t lease_expires_at = 0;
                std::unique_ptr<ConnectionContext> connection;
            };
            std::unordered_map<ID, DormantContextEntry, IDHash> dormant_contexts_;
            std::mutex dormant_mutex_;

            mutable std::mutex connection_registry_mutex_;
            std::unordered_map<uint32_t, ConnectionContext*> connection_registry_;

            // === Tablespace File Descriptors (Phase 1, Task 1.3.4) ===
            std::unordered_map<uint16_t, int> tablespace_fds_; // Map: tablespace_id -> file descriptor
            mutable std::mutex tablespace_mutex_;              // Protects tablespace_fds_ access

            // Validate database header
            Status validate_header(ErrorContext *ctx);

            // Create helpers
            static Status init_header_page(int fd, const std::string &path, uint32_t page_size,
                                           uint8_t *page_buffer, ErrorContext *ctx);
            static Status create_catalog_page(int fd, uint32_t page_size, uint8_t *page_buffer,
                                              const ID &db_uuid, uint64_t micros,
                                              ErrorContext *ctx);
            static Status create_fsm_page(int fd, uint32_t page_size, uint8_t *page_buffer,
                                          const ID &db_uuid, ErrorContext *ctx);
            static Status validate_db_path(const std::string &path, std::string &canonical_path,
                                           ErrorContext *ctx);
        };

    } // namespace core
} // namespace scratchbird
