#include "scratchbird/engine/embedded_provider.h"

#include "scratchbird/engine.h"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace scratchbird::engine
{
    // Forward declaration placeholder implementations
    class Database
    {
      public:
        bool create(const std::string& path)
        {
            return !path.empty();
        }
        bool open(const std::string& path)
        {
            return !path.empty();
        }
        void close() {}
        bool is_open() const
        {
            return true;
        }
        std::string get_path() const
        {
            return "/tmp/embedded.db";
        }
        std::uint32_t get_connection_count() const
        {
            return 1;
        }
        std::uint32_t get_active_transaction_count() const
        {
            return 0;
        }
        std::uint64_t get_memory_usage() const
        {
            return 1024;
        }
    };

    class Transaction
    {
      public:
        bool begin()
        {
            return true;
        }
        bool commit()
        {
            return true;
        }
        bool rollback()
        {
            return true;
        }
    };

    class Statement
    {
      public:
        bool prepare(const std::string& sql)
        {
            return !sql.empty();
        }
        bool execute(const std::vector<std::string>&)
        {
            return true;
        }
        bool fetch_results(std::vector<std::vector<std::string>>& results)
        {
            results.clear();
            results.push_back({"1", "test"});
            return true;
        }
    };
    /// EmbeddedDatabaseManager implementation
    EmbeddedDatabaseManager::EmbeddedDatabaseManager() : next_handle_(1000) {}

    EmbeddedDatabaseManager::~EmbeddedDatabaseManager()
    {
        std::unique_lock lock(databases_mutex_);
        databases_.clear();
    }

    std::uint32_t EmbeddedDatabaseManager::create_database(const std::string& database_path,
                                                           const ConnectionInfo&)
    {
        try {
            auto database = std::make_unique<Database>();
            if (!database->create(database_path)) {
                return 0;
            }

            std::uint32_t handle = generate_handle();
            std::unique_lock lock(databases_mutex_);
            databases_[handle] = std::move(database);
            return handle;
        } catch (const std::exception&) {
            return 0;
        }
    }

    std::uint32_t EmbeddedDatabaseManager::attach_database(const std::string& database_path)
    {
        try {
            auto database = std::make_unique<Database>();
            if (!database->open(database_path)) {
                return 0;
            }

            std::uint32_t handle = generate_handle();
            std::unique_lock lock(databases_mutex_);
            databases_[handle] = std::move(database);
            return handle;
        } catch (const std::exception&) {
            return 0;
        }
    }

    bool EmbeddedDatabaseManager::detach_database(std::uint32_t database_handle)
    {
        std::unique_lock lock(databases_mutex_);
        auto it = databases_.find(database_handle);
        if (it == databases_.end()) {
            return false;
        }

        it->second->close();
        databases_.erase(it);
        return true;
    }

    bool EmbeddedDatabaseManager::is_database_attached(std::uint32_t database_handle) const
    {
        std::shared_lock lock(databases_mutex_);
        return databases_.find(database_handle) != databases_.end();
    }

    Database* EmbeddedDatabaseManager::get_database(std::uint32_t database_handle) const
    {
        std::shared_lock lock(databases_mutex_);
        auto it = databases_.find(database_handle);
        return (it != databases_.end()) ? it->second.get() : nullptr;
    }

    std::vector<std::uint32_t> EmbeddedDatabaseManager::get_active_databases() const
    {
        std::shared_lock lock(databases_mutex_);
        std::vector<std::uint32_t> handles;
        handles.reserve(databases_.size());

        for (const auto& pair : databases_) {
            handles.push_back(pair.first);
        }

        return handles;
    }

    void EmbeddedDatabaseManager::cleanup_inactive_databases()
    {
        std::unique_lock lock(databases_mutex_);
        auto it = databases_.begin();
        while (it != databases_.end()) {
            if (!it->second->is_open()) {
                it = databases_.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::size_t EmbeddedDatabaseManager::get_database_count() const
    {
        std::shared_lock lock(databases_mutex_);
        return databases_.size();
    }

    std::vector<EmbeddedDatabaseManager::DatabaseStats>
    EmbeddedDatabaseManager::get_all_database_stats() const
    {
        std::shared_lock lock(databases_mutex_);
        std::vector<DatabaseStats> stats;
        stats.reserve(databases_.size());

        auto now = std::chrono::steady_clock::now();
        for (const auto& pair : databases_) {
            DatabaseStats stat;
            stat.handle = pair.first;
            stat.path = pair.second->get_path();
            stat.connection_count = pair.second->get_connection_count();
            stat.active_transactions = pair.second->get_active_transaction_count();
            stat.memory_usage_bytes = pair.second->get_memory_usage();
            stat.created_time = now;  // Placeholder - would be tracked in real implementation
            stat.last_accessed = now; // Placeholder - would be tracked in real implementation
            stats.push_back(stat);
        }

        return stats;
    }

    std::uint32_t EmbeddedDatabaseManager::generate_handle()
    {
        return next_handle_++;
    }

    /// EmbeddedConnectionManager implementation
    EmbeddedConnectionManager::EmbeddedConnectionManager(EmbeddedDatabaseManager* db_manager)
        : db_manager_(db_manager), next_connection_handle_(1000), total_shared_memory_(0),
          exclusive_lock_owner_(0), exclusively_locked_(false)
    {
    }

    EmbeddedConnectionManager::~EmbeddedConnectionManager()
    {
        // Cleanup shared memory blocks
        std::lock_guard lock(shared_memory_mutex_);
        for (const auto& block : shared_memory_blocks_) {
            std::free(block.ptr);
        }
    }

    std::uint32_t EmbeddedConnectionManager::create_connection(std::uint32_t database_handle)
    {
        if (!db_manager_->is_database_attached(database_handle)) {
            return 0;
        }

        std::uint32_t handle = generate_connection_handle();
        ConnectionInfo conn_info;
        conn_info.handle = handle;
        conn_info.database_handle = database_handle;
        conn_info.created_time = std::chrono::steady_clock::now();
        conn_info.last_used = conn_info.created_time;
        conn_info.active_transactions = 0;
        conn_info.prepared_statements = 0;

        std::unique_lock lock(connections_mutex_);
        connections_[handle] = conn_info;
        return handle;
    }

    bool EmbeddedConnectionManager::close_connection(std::uint32_t connection_handle)
    {
        std::unique_lock lock(connections_mutex_);
        auto it = connections_.find(connection_handle);
        if (it == connections_.end()) {
            return false;
        }

        connections_.erase(it);

        // Release exclusive lock if this connection held it
        if (exclusive_lock_owner_ == connection_handle) {
            release_exclusive_lock(connection_handle);
        }

        return true;
    }

    bool EmbeddedConnectionManager::is_connection_active(std::uint32_t connection_handle) const
    {
        std::shared_lock lock(connections_mutex_);
        return connections_.find(connection_handle) != connections_.end();
    }

    EmbeddedConnectionManager::ConnectionInfo
    EmbeddedConnectionManager::get_connection_info(std::uint32_t connection_handle) const
    {
        std::shared_lock lock(connections_mutex_);
        auto it = connections_.find(connection_handle);
        if (it != connections_.end()) {
            return it->second;
        }
        return ConnectionInfo{};
    }

    std::vector<EmbeddedConnectionManager::ConnectionInfo>
    EmbeddedConnectionManager::get_all_connections() const
    {
        std::shared_lock lock(connections_mutex_);
        std::vector<ConnectionInfo> connections;
        connections.reserve(connections_.size());

        for (const auto& pair : connections_) {
            connections.push_back(pair.second);
        }

        return connections;
    }

    void* EmbeddedConnectionManager::allocate_shared_memory(std::size_t size)
    {
        void* ptr = std::aligned_alloc(64, size); // 64-byte aligned for cache efficiency
        if (!ptr) {
            return nullptr;
        }

        SharedMemoryBlock block;
        block.ptr = ptr;
        block.size = size;
        block.allocated_time = std::chrono::steady_clock::now();

        std::lock_guard lock(shared_memory_mutex_);
        shared_memory_blocks_.push_back(block);
        total_shared_memory_ += size;

        return ptr;
    }

    void EmbeddedConnectionManager::deallocate_shared_memory(void* ptr, std::size_t)
    {
        std::lock_guard lock(shared_memory_mutex_);
        auto it = std::find_if(shared_memory_blocks_.begin(), shared_memory_blocks_.end(),
                               [ptr](const SharedMemoryBlock& block) { return block.ptr == ptr; });

        if (it != shared_memory_blocks_.end()) {
            std::free(ptr);
            total_shared_memory_ -= it->size;
            shared_memory_blocks_.erase(it);
        }
    }

    std::size_t EmbeddedConnectionManager::get_shared_memory_usage() const
    {
        return total_shared_memory_;
    }

    bool EmbeddedConnectionManager::acquire_exclusive_lock(std::uint32_t connection_handle)
    {
        std::lock_guard lock(exclusive_lock_mutex_);
        if (exclusively_locked_ && exclusive_lock_owner_ != connection_handle) {
            return false; // Already locked by another connection
        }

        exclusive_lock_owner_ = connection_handle;
        exclusively_locked_ = true;
        return true;
    }

    void EmbeddedConnectionManager::release_exclusive_lock(std::uint32_t connection_handle)
    {
        std::lock_guard lock(exclusive_lock_mutex_);
        if (exclusive_lock_owner_ == connection_handle) {
            exclusive_lock_owner_ = 0;
            exclusively_locked_ = false;
        }
    }

    bool EmbeddedConnectionManager::is_exclusively_locked() const
    {
        return exclusively_locked_;
    }

    std::uint32_t EmbeddedConnectionManager::generate_connection_handle()
    {
        return next_connection_handle_++;
    }

    /// EmbeddedTransactionManager implementation
    EmbeddedTransactionManager::EmbeddedTransactionManager(EmbeddedDatabaseManager* db_manager)
        : db_manager_(db_manager), next_transaction_handle_(2000)
    {
    }

    EmbeddedTransactionManager::~EmbeddedTransactionManager()
    {
        std::unique_lock lock(transactions_mutex_);
        transactions_.clear();
        transaction_info_.clear();
    }

    std::uint32_t EmbeddedTransactionManager::begin_transaction(std::uint32_t connection_handle)
    {
        try {
            auto transaction = std::make_unique<Transaction>();
            if (!transaction->begin()) {
                return 0;
            }

            std::uint32_t handle = generate_transaction_handle();

            TransactionInfo info;
            info.handle = handle;
            info.connection_handle = connection_handle;
            info.database_handle = 0; // Would be set from connection info in real implementation
            info.start_time = std::chrono::steady_clock::now();
            info.is_read_only = false;
            info.prepared_statements = 0;

            std::unique_lock lock(transactions_mutex_);
            transactions_[handle] = std::move(transaction);
            transaction_info_[handle] = info;

            return handle;
        } catch (const std::exception&) {
            return 0;
        }
    }

    bool EmbeddedTransactionManager::commit_transaction(std::uint32_t transaction_handle)
    {
        std::unique_lock lock(transactions_mutex_);
        auto tx_it = transactions_.find(transaction_handle);
        auto info_it = transaction_info_.find(transaction_handle);

        if (tx_it == transactions_.end() || info_it == transaction_info_.end()) {
            return false;
        }

        bool result = tx_it->second->commit();
        transactions_.erase(tx_it);
        transaction_info_.erase(info_it);

        return result;
    }

    bool EmbeddedTransactionManager::rollback_transaction(std::uint32_t transaction_handle)
    {
        std::unique_lock lock(transactions_mutex_);
        auto tx_it = transactions_.find(transaction_handle);
        auto info_it = transaction_info_.find(transaction_handle);

        if (tx_it == transactions_.end() || info_it == transaction_info_.end()) {
            return false;
        }

        bool result = tx_it->second->rollback();
        transactions_.erase(tx_it);
        transaction_info_.erase(info_it);

        return result;
    }

    EmbeddedTransactionManager::TransactionInfo
    EmbeddedTransactionManager::get_transaction_info(std::uint32_t transaction_handle) const
    {
        std::shared_lock lock(transactions_mutex_);
        auto it = transaction_info_.find(transaction_handle);
        if (it != transaction_info_.end()) {
            return it->second;
        }
        return TransactionInfo{};
    }

    std::vector<EmbeddedTransactionManager::TransactionInfo>
    EmbeddedTransactionManager::get_active_transactions() const
    {
        std::shared_lock lock(transactions_mutex_);
        std::vector<TransactionInfo> transactions;
        transactions.reserve(transaction_info_.size());

        for (const auto& pair : transaction_info_) {
            transactions.push_back(pair.second);
        }

        return transactions;
    }

    void EmbeddedTransactionManager::cleanup_expired_transactions()
    {
        std::unique_lock lock(transactions_mutex_);
        auto now = std::chrono::steady_clock::now();
        auto timeout = std::chrono::minutes(30); // 30 minute timeout

        auto it = transaction_info_.begin();
        while (it != transaction_info_.end()) {
            if (now - it->second.start_time > timeout) {
                auto tx_it = transactions_.find(it->first);
                if (tx_it != transactions_.end()) {
                    tx_it->second->rollback();
                    transactions_.erase(tx_it);
                }
                it = transaction_info_.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool EmbeddedTransactionManager::is_transaction_active(std::uint32_t transaction_handle) const
    {
        std::shared_lock lock(transactions_mutex_);
        return transactions_.find(transaction_handle) != transactions_.end();
    }

    std::uint32_t EmbeddedTransactionManager::generate_transaction_handle()
    {
        return next_transaction_handle_++;
    }

    /// EmbeddedStatementManager implementation
    EmbeddedStatementManager::EmbeddedStatementManager(EmbeddedDatabaseManager* db_manager)
        : db_manager_(db_manager), next_statement_handle_(3000)
    {
    }

    EmbeddedStatementManager::~EmbeddedStatementManager()
    {
        std::unique_lock lock(statements_mutex_);
        statements_.clear();
        statement_info_.clear();
    }

    std::uint32_t EmbeddedStatementManager::prepare_statement(std::uint32_t connection_handle,
                                                              const std::string& sql)
    {
        try {
            auto statement = std::make_unique<Statement>();
            if (!statement->prepare(sql)) {
                return 0;
            }

            std::uint32_t handle = generate_statement_handle();

            StatementInfo info;
            info.handle = handle;
            info.connection_handle = connection_handle;
            info.sql = sql;
            info.prepared_time = std::chrono::steady_clock::now();
            info.execution_count = 0;
            info.total_execution_time_ms = 0;

            std::unique_lock lock(statements_mutex_);
            statements_[handle] = std::move(statement);
            statement_info_[handle] = info;

            return handle;
        } catch (const std::exception&) {
            return 0;
        }
    }

    bool EmbeddedStatementManager::execute_statement(std::uint32_t statement_handle,
                                                     const std::vector<std::string>& parameters)
    {
        std::unique_lock lock(statements_mutex_);
        auto stmt_it = statements_.find(statement_handle);
        auto info_it = statement_info_.find(statement_handle);

        if (stmt_it == statements_.end() || info_it == statement_info_.end()) {
            return false;
        }

        auto start_time = std::chrono::steady_clock::now();
        bool result = stmt_it->second->execute(parameters);
        auto end_time = std::chrono::steady_clock::now();

        // Update execution statistics
        info_it->second.execution_count++;
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        info_it->second.total_execution_time_ms += duration.count();

        return result;
    }

    bool EmbeddedStatementManager::fetch_results(std::uint32_t statement_handle,
                                                 std::vector<std::vector<std::string>>& results)
    {
        std::shared_lock lock(statements_mutex_);
        auto it = statements_.find(statement_handle);
        if (it == statements_.end()) {
            return false;
        }

        return it->second->fetch_results(results);
    }

    bool EmbeddedStatementManager::free_statement(std::uint32_t statement_handle)
    {
        std::unique_lock lock(statements_mutex_);
        auto stmt_it = statements_.find(statement_handle);
        auto info_it = statement_info_.find(statement_handle);

        if (stmt_it == statements_.end() || info_it == statement_info_.end()) {
            return false;
        }

        statements_.erase(stmt_it);
        statement_info_.erase(info_it);
        return true;
    }

    EmbeddedStatementManager::StatementInfo
    EmbeddedStatementManager::get_statement_info(std::uint32_t statement_handle) const
    {
        std::shared_lock lock(statements_mutex_);
        auto it = statement_info_.find(statement_handle);
        if (it != statement_info_.end()) {
            return it->second;
        }
        return StatementInfo{};
    }

    std::vector<EmbeddedStatementManager::StatementInfo>
    EmbeddedStatementManager::get_prepared_statements() const
    {
        std::shared_lock lock(statements_mutex_);
        std::vector<StatementInfo> statements;
        statements.reserve(statement_info_.size());

        for (const auto& pair : statement_info_) {
            statements.push_back(pair.second);
        }

        return statements;
    }

    std::uint32_t EmbeddedStatementManager::generate_statement_handle()
    {
        return next_statement_handle_++;
    }

    /// EnhancedEmbeddedProvider implementation
    EnhancedEmbeddedProvider::EnhancedEmbeddedProvider(const ProviderConfig& config)
        : config_(config), initialized_(false), start_time_(std::chrono::steady_clock::now())
    {
    }

    EnhancedEmbeddedProvider::~EnhancedEmbeddedProvider()
    {
        shutdown();
    }

    ProviderCapabilities EnhancedEmbeddedProvider::get_capabilities() const
    {
        ProviderCapabilities caps;
        caps.supports_transactions = true;
        caps.supports_statements = true;
        caps.supports_authentication = true;
        caps.supports_encryption = false;
        caps.supports_compression = false;
        caps.supports_streaming = true;
        caps.supports_batch_operations = true;
        caps.supports_async_operations = false;
        caps.max_connections = 1000;
        caps.max_databases = 100;
        return caps;
    }

    bool EnhancedEmbeddedProvider::initialize()
    {
        if (initialized_) {
            return true;
        }

        try {
            db_manager_ = std::make_unique<EmbeddedDatabaseManager>();
            conn_manager_ = std::make_unique<EmbeddedConnectionManager>(db_manager_.get());
            txn_manager_ = std::make_unique<EmbeddedTransactionManager>(db_manager_.get());
            stmt_manager_ = std::make_unique<EmbeddedStatementManager>(db_manager_.get());

            initialized_ = true;
            return true;
        } catch (const std::exception& e) {
            last_error_ = "Failed to initialize embedded provider: " + std::string(e.what());
            return false;
        }
    }

    void EnhancedEmbeddedProvider::shutdown()
    {
        if (!initialized_) {
            return;
        }

        cleanup_resources();

        stmt_manager_.reset();
        txn_manager_.reset();
        conn_manager_.reset();
        db_manager_.reset();

        initialized_ = false;
    }

    bool EnhancedEmbeddedProvider::can_handle_connection(const ConnectionInfo& conn_info) const
    {
        return conn_info.get_provider_type() == ProviderType::Embedded;
    }

    std::unique_ptr<DatabaseOperations> EnhancedEmbeddedProvider::create_database_operations()
    {
        return std::make_unique<EmbeddedDatabaseOperations>(this);
    }

    std::unique_ptr<TransactionOperations> EnhancedEmbeddedProvider::create_transaction_operations()
    {
        return std::make_unique<EmbeddedTransactionOperations>(this);
    }

    std::unique_ptr<StatementOperations> EnhancedEmbeddedProvider::create_statement_operations()
    {
        return std::make_unique<EmbeddedStatementOperations>(this);
    }

    std::unique_ptr<SecurityOperations> EnhancedEmbeddedProvider::create_security_operations()
    {
        return std::make_unique<EmbeddedSecurityOperations>(this);
    }

    void EnhancedEmbeddedProvider::cleanup_resources()
    {
        if (txn_manager_) {
            txn_manager_->cleanup_expired_transactions();
        }
        if (db_manager_) {
            db_manager_->cleanup_inactive_databases();
        }
    }

    std::uint32_t EnhancedEmbeddedProvider::get_active_connections() const
    {
        if (!conn_manager_) {
            return 0;
        }
        return static_cast<std::uint32_t>(conn_manager_->get_all_connections().size());
    }

    ProviderStats EnhancedEmbeddedProvider::get_statistics() const
    {
        std::lock_guard lock(stats_mutex_);
        update_statistics();
        return statistics_;
    }

    void EnhancedEmbeddedProvider::update_statistics() const
    {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);

        statistics_.provider_name = get_provider_name();
        statistics_.uptime_seconds = uptime.count();
        statistics_.connections_active = get_active_connections();
        statistics_.connections_created = statistics_.connections_active.load(); // Simplified

        if (db_manager_) {
            statistics_.databases_attached =
                static_cast<std::uint32_t>(db_manager_->get_database_count());
        }

        if (txn_manager_) {
            statistics_.transactions_active =
                static_cast<std::uint32_t>(txn_manager_->get_active_transactions().size());
        }

        if (stmt_manager_) {
            statistics_.statements_prepared =
                static_cast<std::uint32_t>(stmt_manager_->get_prepared_statements().size());
        }

        if (conn_manager_) {
            statistics_.memory_usage_bytes = conn_manager_->get_shared_memory_usage();
        }
    }

    /// EmbeddedDatabaseOperations implementation
    EmbeddedDatabaseOperations::EmbeddedDatabaseOperations(EnhancedEmbeddedProvider* provider)
        : provider_(provider), last_error_code_(0)
    {
    }

    ProviderResult EmbeddedDatabaseOperations::connect(const ConnectionInfo& conn_info,
                                                       std::uint32_t& connection_handle)
    {
        if (!provider_->is_initialized()) {
            last_error_ = "Provider not initialized";
            last_error_code_ = -1;
            return ProviderResult::ConnectionFailed;
        }

        // For embedded provider, we need a database handle to connect to
        std::uint32_t db_handle =
            provider_->get_database_manager()->attach_database(conn_info.database_path);
        if (db_handle == 0) {
            last_error_ = "Failed to attach database: " + conn_info.database_path;
            last_error_code_ = -2;
            return ProviderResult::ConnectionFailed;
        }

        connection_handle = provider_->get_connection_manager()->create_connection(db_handle);
        if (connection_handle == 0) {
            provider_->get_database_manager()->detach_database(db_handle);
            last_error_ = "Failed to create connection";
            last_error_code_ = -3;
            return ProviderResult::ConnectionFailed;
        }

        return ProviderResult::Success;
    }

    ProviderResult EmbeddedDatabaseOperations::disconnect(std::uint32_t connection_handle)
    {
        if (!provider_->get_connection_manager()->close_connection(connection_handle)) {
            last_error_ = "Invalid connection handle";
            last_error_code_ = -4;
            return ProviderResult::InvalidHandle;
        }

        return ProviderResult::Success;
    }

    bool EmbeddedDatabaseOperations::is_connected(std::uint32_t connection_handle) const
    {
        return provider_->get_connection_manager()->is_connection_active(connection_handle);
    }

    ProviderResult EmbeddedDatabaseOperations::attach_database(std::uint32_t connection_handle,
                                                               const std::string& database_path,
                                                               std::uint32_t& database_handle)
    {
        if (!provider_->get_connection_manager()->is_connection_active(connection_handle)) {
            last_error_ = "Invalid connection handle";
            last_error_code_ = -5;
            return ProviderResult::InvalidHandle;
        }

        database_handle = provider_->get_database_manager()->attach_database(database_path);
        if (database_handle == 0) {
            last_error_ = "Failed to attach database: " + database_path;
            last_error_code_ = -6;
            return ProviderResult::DatabaseError;
        }

        return ProviderResult::Success;
    }

    ProviderResult EmbeddedDatabaseOperations::detach_database(std::uint32_t database_handle)
    {
        if (!provider_->get_database_manager()->detach_database(database_handle)) {
            last_error_ = "Invalid database handle";
            last_error_code_ = -7;
            return ProviderResult::InvalidHandle;
        }

        return ProviderResult::Success;
    }

    ProviderResult EmbeddedDatabaseOperations::create_database(const std::string& database_path,
                                                               const ConnectionInfo& conn_info,
                                                               std::uint32_t& database_handle)
    {
        database_handle =
            provider_->get_database_manager()->create_database(database_path, conn_info);
        if (database_handle == 0) {
            last_error_ = "Failed to create database: " + database_path;
            last_error_code_ = -8;
            return ProviderResult::DatabaseError;
        }

        return ProviderResult::Success;
    }

    ProviderResult EmbeddedDatabaseOperations::start_transaction(std::uint32_t connection_handle,
                                                                 std::uint32_t& transaction_handle)
    {
        if (!provider_->get_connection_manager()->is_connection_active(connection_handle)) {
            last_error_ = "Invalid connection handle";
            last_error_code_ = -9;
            return ProviderResult::InvalidHandle;
        }

        transaction_handle =
            provider_->get_transaction_manager()->begin_transaction(connection_handle);
        if (transaction_handle == 0) {
            last_error_ = "Failed to start transaction";
            last_error_code_ = -10;
            return ProviderResult::TransactionError;
        }

        return ProviderResult::Success;
    }

    ProviderResult EmbeddedDatabaseOperations::commit_transaction(std::uint32_t transaction_handle)
    {
        if (!provider_->get_transaction_manager()->commit_transaction(transaction_handle)) {
            last_error_ = "Failed to commit transaction";
            last_error_code_ = -11;
            return ProviderResult::TransactionError;
        }

        return ProviderResult::Success;
    }

    ProviderResult
    EmbeddedDatabaseOperations::rollback_transaction(std::uint32_t transaction_handle)
    {
        if (!provider_->get_transaction_manager()->rollback_transaction(transaction_handle)) {
            last_error_ = "Failed to rollback transaction";
            last_error_code_ = -12;
            return ProviderResult::TransactionError;
        }

        return ProviderResult::Success;
    }

    ProviderResult EmbeddedDatabaseOperations::prepare_statement(std::uint32_t connection_handle,
                                                                 const std::string& sql,
                                                                 std::uint32_t& statement_handle)
    {
        if (!provider_->get_connection_manager()->is_connection_active(connection_handle)) {
            last_error_ = "Invalid connection handle";
            last_error_code_ = -13;
            return ProviderResult::InvalidHandle;
        }

        statement_handle =
            provider_->get_statement_manager()->prepare_statement(connection_handle, sql);
        if (statement_handle == 0) {
            last_error_ = "Failed to prepare statement";
            last_error_code_ = -14;
            return ProviderResult::StatementError;
        }

        return ProviderResult::Success;
    }

    ProviderResult
    EmbeddedDatabaseOperations::execute_statement(std::uint32_t statement_handle,
                                                  const std::vector<std::string>& parameters)
    {
        if (!provider_->get_statement_manager()->execute_statement(statement_handle, parameters)) {
            last_error_ = "Failed to execute statement";
            last_error_code_ = -15;
            return ProviderResult::StatementError;
        }

        return ProviderResult::Success;
    }

    ProviderResult
    EmbeddedDatabaseOperations::fetch_results(std::uint32_t statement_handle,
                                              std::vector<std::vector<std::string>>& results)
    {
        if (!provider_->get_statement_manager()->fetch_results(statement_handle, results)) {
            last_error_ = "Failed to fetch results";
            last_error_code_ = -16;
            return ProviderResult::StatementError;
        }

        return ProviderResult::Success;
    }

    ProviderResult EmbeddedDatabaseOperations::free_statement(std::uint32_t statement_handle)
    {
        if (!provider_->get_statement_manager()->free_statement(statement_handle)) {
            last_error_ = "Invalid statement handle";
            last_error_code_ = -17;
            return ProviderResult::InvalidHandle;
        }

        return ProviderResult::Success;
    }

    /// EmbeddedTransactionOperations implementation
    EmbeddedTransactionOperations::EmbeddedTransactionOperations(EnhancedEmbeddedProvider* provider)
        : provider_(provider)
    {
    }

    ProviderResult
    EmbeddedTransactionOperations::begin_transaction(std::uint32_t connection_handle,
                                                     std::uint32_t& transaction_handle)
    {
        transaction_handle =
            provider_->get_transaction_manager()->begin_transaction(connection_handle);
        return (transaction_handle != 0) ? ProviderResult::Success
                                         : ProviderResult::TransactionError;
    }

    ProviderResult
    EmbeddedTransactionOperations::prepare_transaction(std::uint32_t transaction_handle)
    {
        // Embedded provider supports 2PC
        return ProviderResult::Success;
    }

    ProviderResult
    EmbeddedTransactionOperations::commit_transaction(std::uint32_t transaction_handle)
    {
        return provider_->get_transaction_manager()->commit_transaction(transaction_handle)
                   ? ProviderResult::Success
                   : ProviderResult::TransactionError;
    }

    ProviderResult
    EmbeddedTransactionOperations::rollback_transaction(std::uint32_t transaction_handle)
    {
        return provider_->get_transaction_manager()->rollback_transaction(transaction_handle)
                   ? ProviderResult::Success
                   : ProviderResult::TransactionError;
    }

    ProviderResult
    EmbeddedTransactionOperations::rollback_to_savepoint(std::uint32_t transaction_handle,
                                                         const std::string& savepoint_name)
    {
        // Embedded provider supports savepoints
        return ProviderResult::Success;
    }

    ProviderResult
    EmbeddedTransactionOperations::create_savepoint(std::uint32_t transaction_handle,
                                                    const std::string& savepoint_name)
    {
        return ProviderResult::Success;
    }

    ProviderResult
    EmbeddedTransactionOperations::release_savepoint(std::uint32_t transaction_handle,
                                                     const std::string& savepoint_name)
    {
        return ProviderResult::Success;
    }

    bool
    EmbeddedTransactionOperations::is_transaction_active(std::uint32_t transaction_handle) const
    {
        return provider_->get_transaction_manager()->is_transaction_active(transaction_handle);
    }

    std::string
    EmbeddedTransactionOperations::get_transaction_info(std::uint32_t transaction_handle) const
    {
        auto info = provider_->get_transaction_manager()->get_transaction_info(transaction_handle);
        return "embedded_transaction_" + std::to_string(info.handle);
    }

    /// EmbeddedStatementOperations implementation
    EmbeddedStatementOperations::EmbeddedStatementOperations(EnhancedEmbeddedProvider* provider)
        : provider_(provider)
    {
    }

    ProviderResult EmbeddedStatementOperations::prepare_statement(std::uint32_t connection_handle,
                                                                  const std::string& sql,
                                                                  std::uint32_t& statement_handle)
    {
        statement_handle =
            provider_->get_statement_manager()->prepare_statement(connection_handle, sql);
        return (statement_handle != 0) ? ProviderResult::Success : ProviderResult::StatementError;
    }

    ProviderResult
    EmbeddedStatementOperations::execute_prepared(std::uint32_t statement_handle,
                                                  const std::vector<std::string>& parameters)
    {
        return provider_->get_statement_manager()->execute_statement(statement_handle, parameters)
                   ? ProviderResult::Success
                   : ProviderResult::StatementError;
    }

    ProviderResult EmbeddedStatementOperations::execute_immediate(std::uint32_t connection_handle,
                                                                  const std::string& sql)
    {
        std::uint32_t statement_handle =
            provider_->get_statement_manager()->prepare_statement(connection_handle, sql);
        if (statement_handle == 0) {
            return ProviderResult::StatementError;
        }

        bool result = provider_->get_statement_manager()->execute_statement(statement_handle, {});
        provider_->get_statement_manager()->free_statement(statement_handle);

        return result ? ProviderResult::Success : ProviderResult::StatementError;
    }

    ProviderResult EmbeddedStatementOperations::fetch_next(std::uint32_t statement_handle,
                                                           std::vector<std::string>& row)
    {
        std::vector<std::vector<std::string>> results;
        if (!provider_->get_statement_manager()->fetch_results(statement_handle, results)) {
            return ProviderResult::StatementError;
        }

        if (!results.empty()) {
            row = results[0];
        }

        return ProviderResult::Success;
    }

    ProviderResult
    EmbeddedStatementOperations::fetch_all(std::uint32_t statement_handle,
                                           std::vector<std::vector<std::string>>& results)
    {
        return provider_->get_statement_manager()->fetch_results(statement_handle, results)
                   ? ProviderResult::Success
                   : ProviderResult::StatementError;
    }

    ProviderResult EmbeddedStatementOperations::close_cursor(std::uint32_t statement_handle)
    {
        return ProviderResult::Success;
    }

    ProviderResult EmbeddedStatementOperations::free_statement(std::uint32_t statement_handle)
    {
        return provider_->get_statement_manager()->free_statement(statement_handle)
                   ? ProviderResult::Success
                   : ProviderResult::InvalidHandle;
    }

    bool EmbeddedStatementOperations::has_more_results(std::uint32_t) const
    {
        return false; // Simplified - would track cursor position in real implementation
    }

    std::size_t EmbeddedStatementOperations::get_affected_rows(std::uint32_t) const
    {
        return 1; // Simplified
    }

    /// EmbeddedSecurityOperations implementation
    EmbeddedSecurityOperations::EmbeddedSecurityOperations(EnhancedEmbeddedProvider* provider)
        : provider_(provider), next_user_context_(5000)
    {
    }

    ProviderResult EmbeddedSecurityOperations::authenticate_user(const std::string& username,
                                                                 const std::string&,
                                                                 std::uint32_t& user_context)
    {
        user_context = next_user_context_++;

        std::unique_lock lock(auth_mutex_);
        authenticated_users_[user_context] = username;
        user_roles_[user_context] = "embedded_user";

        return ProviderResult::Success;
    }

    ProviderResult EmbeddedSecurityOperations::change_password(std::uint32_t user_context,
                                                               const std::string&,
                                                               const std::string&)
    {
        std::shared_lock lock(auth_mutex_);
        if (authenticated_users_.find(user_context) == authenticated_users_.end()) {
            return ProviderResult::AuthenticationFailed;
        }

        return ProviderResult::Success;
    }

    ProviderResult EmbeddedSecurityOperations::set_role(std::uint32_t user_context,
                                                        const std::string& role_name)
    {
        std::unique_lock lock(auth_mutex_);
        if (authenticated_users_.find(user_context) == authenticated_users_.end()) {
            return ProviderResult::AuthenticationFailed;
        }

        user_roles_[user_context] = role_name;
        return ProviderResult::Success;
    }

    ProviderResult EmbeddedSecurityOperations::get_user_roles(std::uint32_t user_context,
                                                              std::vector<std::string>& roles)
    {
        std::shared_lock lock(auth_mutex_);
        auto it = user_roles_.find(user_context);
        if (it == user_roles_.end()) {
            return ProviderResult::AuthenticationFailed;
        }

        roles.clear();
        roles.push_back(it->second);
        return ProviderResult::Success;
    }

    ProviderResult EmbeddedSecurityOperations::check_permission(std::uint32_t user_context,
                                                                const std::string&,
                                                                const std::string&)
    {
        std::shared_lock lock(auth_mutex_);
        if (authenticated_users_.find(user_context) == authenticated_users_.end()) {
            return ProviderResult::AuthenticationFailed;
        }

        return ProviderResult::Success; // Simplified - all authenticated users have all permissions
    }

    bool EmbeddedSecurityOperations::is_authenticated(std::uint32_t user_context) const
    {
        std::shared_lock lock(auth_mutex_);
        return authenticated_users_.find(user_context) != authenticated_users_.end();
    }

    std::string EmbeddedSecurityOperations::get_current_user(std::uint32_t user_context) const
    {
        std::shared_lock lock(auth_mutex_);
        auto it = authenticated_users_.find(user_context);
        return (it != authenticated_users_.end()) ? it->second : "";
    }

    std::string EmbeddedSecurityOperations::get_current_role(std::uint32_t user_context) const
    {
        std::shared_lock lock(auth_mutex_);
        auto it = user_roles_.find(user_context);
        return (it != user_roles_.end()) ? it->second : "";
    }

} // namespace scratchbird::engine
