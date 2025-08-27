#pragma once

#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/database_provider.h"
#include "scratchbird/engine/storage.h"

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <unordered_map>

namespace scratchbird::engine
{
    /// Forward declarations
    class Database;
    class Transaction;
    class Statement;

    /// Embedded database instance manager
    class EmbeddedDatabaseManager
    {
      public:
        EmbeddedDatabaseManager();
        ~EmbeddedDatabaseManager();

        // Database lifecycle
        std::uint32_t create_database(const std::string& database_path,
                                      const ConnectionInfo& conn_info);
        std::uint32_t attach_database(const std::string& database_path);
        bool detach_database(std::uint32_t database_handle);
        bool is_database_attached(std::uint32_t database_handle) const;

        // Database access
        Database* get_database(std::uint32_t database_handle) const;
        std::vector<std::uint32_t> get_active_databases() const;

        // Resource management
        void cleanup_inactive_databases();
        std::size_t get_database_count() const;

        // Statistics
        struct DatabaseStats {
            std::uint32_t handle;
            std::string path;
            std::size_t connection_count;
            std::size_t active_transactions;
            std::uint64_t memory_usage_bytes;
            std::chrono::steady_clock::time_point created_time;
            std::chrono::steady_clock::time_point last_accessed;
        };

        std::vector<DatabaseStats> get_all_database_stats() const;

      private:
        mutable std::shared_mutex databases_mutex_;
        std::unordered_map<std::uint32_t, std::unique_ptr<Database>> databases_;
        std::atomic<std::uint32_t> next_handle_;

        std::uint32_t generate_handle();
    };

    /// Embedded connection manager with shared memory optimization
    class EmbeddedConnectionManager
    {
      public:
        explicit EmbeddedConnectionManager(EmbeddedDatabaseManager* db_manager);
        ~EmbeddedConnectionManager();

        // Connection management
        std::uint32_t create_connection(std::uint32_t database_handle);
        bool close_connection(std::uint32_t connection_handle);
        bool is_connection_active(std::uint32_t connection_handle) const;

        // Connection info
        struct ConnectionInfo {
            std::uint32_t handle;
            std::uint32_t database_handle;
            std::chrono::steady_clock::time_point created_time;
            std::chrono::steady_clock::time_point last_used;
            std::size_t active_transactions;
            std::size_t prepared_statements;
        };

        ConnectionInfo get_connection_info(std::uint32_t connection_handle) const;
        std::vector<ConnectionInfo> get_all_connections() const;

        // Shared memory optimization
        void* allocate_shared_memory(std::size_t size);
        void deallocate_shared_memory(void* ptr, std::size_t size);
        std::size_t get_shared_memory_usage() const;

        // Single-user locking
        bool acquire_exclusive_lock(std::uint32_t connection_handle);
        void release_exclusive_lock(std::uint32_t connection_handle);
        bool is_exclusively_locked() const;

      private:
        EmbeddedDatabaseManager* db_manager_;
        mutable std::shared_mutex connections_mutex_;
        std::unordered_map<std::uint32_t, ConnectionInfo> connections_;
        std::atomic<std::uint32_t> next_connection_handle_;

        // Shared memory management
        struct SharedMemoryBlock {
            void* ptr;
            std::size_t size;
            std::chrono::steady_clock::time_point allocated_time;
        };
        mutable std::mutex shared_memory_mutex_;
        std::vector<SharedMemoryBlock> shared_memory_blocks_;
        std::atomic<std::size_t> total_shared_memory_;

        // Single-user locking
        mutable std::mutex exclusive_lock_mutex_;
        std::uint32_t exclusive_lock_owner_;
        std::atomic<bool> exclusively_locked_;

        std::uint32_t generate_connection_handle();
    };

    /// Embedded transaction manager
    class EmbeddedTransactionManager
    {
      public:
        explicit EmbeddedTransactionManager(EmbeddedDatabaseManager* db_manager);
        ~EmbeddedTransactionManager();

        // Transaction lifecycle
        std::uint32_t begin_transaction(std::uint32_t connection_handle);
        bool commit_transaction(std::uint32_t transaction_handle);
        bool rollback_transaction(std::uint32_t transaction_handle);

        // Transaction info
        struct TransactionInfo {
            std::uint32_t handle;
            std::uint32_t connection_handle;
            std::uint32_t database_handle;
            std::chrono::steady_clock::time_point start_time;
            bool is_read_only;
            std::size_t prepared_statements;
        };

        TransactionInfo get_transaction_info(std::uint32_t transaction_handle) const;
        std::vector<TransactionInfo> get_active_transactions() const;

        // Resource coordination
        void cleanup_expired_transactions();
        bool is_transaction_active(std::uint32_t transaction_handle) const;

      private:
        EmbeddedDatabaseManager* db_manager_;
        mutable std::shared_mutex transactions_mutex_;
        std::unordered_map<std::uint32_t, std::unique_ptr<Transaction>> transactions_;
        std::unordered_map<std::uint32_t, TransactionInfo> transaction_info_;
        std::atomic<std::uint32_t> next_transaction_handle_;

        std::uint32_t generate_transaction_handle();
    };

    /// Embedded statement manager
    class EmbeddedStatementManager
    {
      public:
        explicit EmbeddedStatementManager(EmbeddedDatabaseManager* db_manager);
        ~EmbeddedStatementManager();

        // Statement management
        std::uint32_t prepare_statement(std::uint32_t connection_handle, const std::string& sql);
        bool execute_statement(std::uint32_t statement_handle,
                               const std::vector<std::string>& parameters);
        bool fetch_results(std::uint32_t statement_handle,
                           std::vector<std::vector<std::string>>& results);
        bool free_statement(std::uint32_t statement_handle);

        // Statement info
        struct StatementInfo {
            std::uint32_t handle;
            std::uint32_t connection_handle;
            std::string sql;
            std::chrono::steady_clock::time_point prepared_time;
            std::size_t execution_count;
            std::uint64_t total_execution_time_ms;
        };

        StatementInfo get_statement_info(std::uint32_t statement_handle) const;
        std::vector<StatementInfo> get_prepared_statements() const;

      private:
        EmbeddedDatabaseManager* db_manager_;
        mutable std::shared_mutex statements_mutex_;
        std::unordered_map<std::uint32_t, std::unique_ptr<Statement>> statements_;
        std::unordered_map<std::uint32_t, StatementInfo> statement_info_;
        std::atomic<std::uint32_t> next_statement_handle_;

        std::uint32_t generate_statement_handle();
    };

    /// Enhanced embedded provider with direct engine integration
    class EnhancedEmbeddedProvider : public DatabaseProvider
    {
      public:
        explicit EnhancedEmbeddedProvider(const ProviderConfig& config);
        ~EnhancedEmbeddedProvider() override;

        // Provider interface
        ProviderType get_provider_type() const override
        {
            return ProviderType::Embedded;
        }
        std::string get_provider_name() const override
        {
            return "EnhancedEmbeddedProvider";
        }
        std::string get_provider_version() const override
        {
            return "1.0.0";
        }
        ProviderCapabilities get_capabilities() const override;

        // Lifecycle management
        bool initialize() override;
        void shutdown() override;
        bool is_initialized() const override
        {
            return initialized_;
        }

        // Connection handling
        bool can_handle_connection(const ConnectionInfo& conn_info) const override;

        // Operation factories
        std::unique_ptr<DatabaseOperations> create_database_operations() override;
        std::unique_ptr<TransactionOperations> create_transaction_operations() override;
        std::unique_ptr<StatementOperations> create_statement_operations() override;
        std::unique_ptr<SecurityOperations> create_security_operations() override;

        // Resource management
        void cleanup_resources() override;
        std::uint32_t get_active_connections() const override;
        ProviderStats get_statistics() const override;
        std::string get_last_error() const override
        {
            return last_error_;
        }

        // Embedded-specific features
        EmbeddedDatabaseManager* get_database_manager() const
        {
            return db_manager_.get();
        }
        EmbeddedConnectionManager* get_connection_manager() const
        {
            return conn_manager_.get();
        }
        EmbeddedTransactionManager* get_transaction_manager() const
        {
            return txn_manager_.get();
        }
        EmbeddedStatementManager* get_statement_manager() const
        {
            return stmt_manager_.get();
        }

      private:
        ProviderConfig config_;
        std::atomic<bool> initialized_;
        std::string last_error_;

        // Core managers
        std::unique_ptr<EmbeddedDatabaseManager> db_manager_;
        std::unique_ptr<EmbeddedConnectionManager> conn_manager_;
        std::unique_ptr<EmbeddedTransactionManager> txn_manager_;
        std::unique_ptr<EmbeddedStatementManager> stmt_manager_;

        // Statistics tracking
        mutable std::mutex stats_mutex_;
        ProviderStats statistics_;
        std::chrono::steady_clock::time_point start_time_;

        void update_statistics() const;
    };

    /// Embedded operations implementations
    class EmbeddedDatabaseOperations : public DatabaseOperations
    {
      public:
        explicit EmbeddedDatabaseOperations(EnhancedEmbeddedProvider* provider);
        ~EmbeddedDatabaseOperations() override = default;

        ProviderResult connect(const ConnectionInfo& conn_info,
                               std::uint32_t& connection_handle) override;
        ProviderResult disconnect(std::uint32_t connection_handle) override;
        bool is_connected(std::uint32_t connection_handle) const override;

        ProviderResult attach_database(std::uint32_t connection_handle,
                                       const std::string& database_path,
                                       std::uint32_t& database_handle) override;
        ProviderResult detach_database(std::uint32_t database_handle) override;
        ProviderResult create_database(const std::string& database_path,
                                       const ConnectionInfo& conn_info,
                                       std::uint32_t& database_handle) override;

        ProviderResult start_transaction(std::uint32_t connection_handle,
                                         std::uint32_t& transaction_handle) override;
        ProviderResult commit_transaction(std::uint32_t transaction_handle) override;
        ProviderResult rollback_transaction(std::uint32_t transaction_handle) override;

        ProviderResult prepare_statement(std::uint32_t connection_handle, const std::string& sql,
                                         std::uint32_t& statement_handle) override;
        ProviderResult execute_statement(std::uint32_t statement_handle,
                                         const std::vector<std::string>& parameters) override;
        ProviderResult fetch_results(std::uint32_t statement_handle,
                                     std::vector<std::vector<std::string>>& results) override;
        ProviderResult free_statement(std::uint32_t statement_handle) override;

        std::string get_last_error() const override
        {
            return last_error_;
        }
        std::int32_t get_last_error_code() const override
        {
            return last_error_code_;
        }

      private:
        EnhancedEmbeddedProvider* provider_;
        mutable std::string last_error_;
        std::atomic<std::int32_t> last_error_code_;
    };

    class EmbeddedTransactionOperations : public TransactionOperations
    {
      public:
        explicit EmbeddedTransactionOperations(EnhancedEmbeddedProvider* provider);
        ~EmbeddedTransactionOperations() override = default;

        ProviderResult begin_transaction(std::uint32_t connection_handle,
                                         std::uint32_t& transaction_handle) override;
        ProviderResult prepare_transaction(std::uint32_t transaction_handle) override;
        ProviderResult commit_transaction(std::uint32_t transaction_handle) override;
        ProviderResult rollback_transaction(std::uint32_t transaction_handle) override;

        ProviderResult rollback_to_savepoint(std::uint32_t transaction_handle,
                                             const std::string& savepoint_name) override;
        ProviderResult create_savepoint(std::uint32_t transaction_handle,
                                        const std::string& savepoint_name) override;
        ProviderResult release_savepoint(std::uint32_t transaction_handle,
                                         const std::string& savepoint_name) override;

        bool is_transaction_active(std::uint32_t transaction_handle) const override;
        std::string get_transaction_info(std::uint32_t transaction_handle) const override;

      private:
        EnhancedEmbeddedProvider* provider_;
    };

    class EmbeddedStatementOperations : public StatementOperations
    {
      public:
        explicit EmbeddedStatementOperations(EnhancedEmbeddedProvider* provider);
        ~EmbeddedStatementOperations() override = default;

        ProviderResult prepare_statement(std::uint32_t connection_handle, const std::string& sql,
                                         std::uint32_t& statement_handle) override;
        ProviderResult execute_prepared(std::uint32_t statement_handle,
                                        const std::vector<std::string>& parameters) override;
        ProviderResult execute_immediate(std::uint32_t connection_handle,
                                         const std::string& sql) override;

        ProviderResult fetch_next(std::uint32_t statement_handle,
                                  std::vector<std::string>& row) override;
        ProviderResult fetch_all(std::uint32_t statement_handle,
                                 std::vector<std::vector<std::string>>& results) override;
        ProviderResult close_cursor(std::uint32_t statement_handle) override;
        ProviderResult free_statement(std::uint32_t statement_handle) override;

        bool has_more_results(std::uint32_t statement_handle) const override;
        std::size_t get_affected_rows(std::uint32_t statement_handle) const override;

      private:
        EnhancedEmbeddedProvider* provider_;
    };

    class EmbeddedSecurityOperations : public SecurityOperations
    {
      public:
        explicit EmbeddedSecurityOperations(EnhancedEmbeddedProvider* provider);
        ~EmbeddedSecurityOperations() override = default;

        ProviderResult authenticate_user(const std::string& username, const std::string& password,
                                         std::uint32_t& user_context) override;
        ProviderResult change_password(std::uint32_t user_context, const std::string& old_password,
                                       const std::string& new_password) override;
        ProviderResult set_role(std::uint32_t user_context, const std::string& role_name) override;
        ProviderResult get_user_roles(std::uint32_t user_context,
                                      std::vector<std::string>& roles) override;
        ProviderResult check_permission(std::uint32_t user_context, const std::string& resource,
                                        const std::string& action) override;

        bool is_authenticated(std::uint32_t user_context) const override;
        std::string get_current_user(std::uint32_t user_context) const override;
        std::string get_current_role(std::uint32_t user_context) const override;

      private:
        EnhancedEmbeddedProvider* provider_;
        mutable std::shared_mutex auth_mutex_;
        std::unordered_map<std::uint32_t, std::string> authenticated_users_;
        std::unordered_map<std::uint32_t, std::string> user_roles_;
        std::atomic<std::uint32_t> next_user_context_;
    };

} // namespace scratchbird::engine
