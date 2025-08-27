#include "scratchbird/engine/database_provider.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

namespace scratchbird::engine
{

    /// ProviderConfig implementation
    bool ProviderConfig::validate() const
    {
        if (provider_name.empty())
            return false;
        if (connection_pool_size == 0)
            return false;
        if (max_concurrent_transactions == 0)
            return false;
        if (timeout_ms == 0)
            return false;
        if (max_memory_mb == 0)
            return false;
        if (max_open_databases == 0)
            return false;
        if (max_prepared_statements == 0)
            return false;
        return true;
    }

    std::string ProviderConfig::to_string() const
    {
        std::ostringstream oss;
        oss << "ProviderConfig{name=" << provider_name << ", version=" << provider_version
            << ", type=" << static_cast<int>(provider_type)
            << ", pool_size=" << connection_pool_size
            << ", max_txns=" << max_concurrent_transactions << ", timeout_ms=" << timeout_ms
            << ", max_memory_mb=" << max_memory_mb << "}";
        return oss.str();
    }

    /// DatabaseProviderFactory implementation
    DatabaseProviderFactory& DatabaseProviderFactory::instance()
    {
        static DatabaseProviderFactory instance;
        return instance;
    }

    void DatabaseProviderFactory::register_embedded_provider()
    {
        std::lock_guard<std::mutex> lock(factory_mutex_);

        factories_["EmbeddedProvider"] =
            [](const ProviderConfig& config) -> std::unique_ptr<DatabaseProvider> {
            return std::make_unique<EmbeddedProvider>(config);
        };

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

        capabilities_registry_["EmbeddedProvider"] = caps;
    }

    void DatabaseProviderFactory::register_remote_provider()
    {
        std::lock_guard<std::mutex> lock(factory_mutex_);

        factories_["RemoteProvider"] =
            [](const ProviderConfig& config) -> std::unique_ptr<DatabaseProvider> {
            return std::make_unique<RemoteProvider>(config);
        };

        ProviderCapabilities caps;
        caps.supports_transactions = true;
        caps.supports_statements = true;
        caps.supports_authentication = true;
        caps.supports_encryption = true;
        caps.supports_compression = true;
        caps.supports_streaming = true;
        caps.supports_batch_operations = true;
        caps.supports_async_operations = true;
        caps.max_connections = 10000;
        caps.max_databases = 1000;

        capabilities_registry_["RemoteProvider"] = caps;
    }

    void DatabaseProviderFactory::register_legacy_provider()
    {
        std::lock_guard<std::mutex> lock(factory_mutex_);

        factories_["LegacyProvider"] =
            [](const ProviderConfig& config) -> std::unique_ptr<DatabaseProvider> {
            return std::make_unique<LegacyProvider>(config);
        };

        ProviderCapabilities caps;
        caps.supports_transactions = true;
        caps.supports_statements = true;
        caps.supports_authentication = false;
        caps.supports_encryption = false;
        caps.supports_compression = false;
        caps.supports_streaming = false;
        caps.supports_batch_operations = false;
        caps.supports_async_operations = false;
        caps.max_connections = 100;
        caps.max_databases = 10;

        capabilities_registry_["LegacyProvider"] = caps;
    }

    void DatabaseProviderFactory::register_third_party_provider(
        const std::string& name, std::function<std::unique_ptr<DatabaseProvider>()> factory)
    {
        std::lock_guard<std::mutex> lock(factory_mutex_);

        // Convert to ProviderConfig-aware factory
        factories_[name] = [factory](const ProviderConfig&) -> std::unique_ptr<DatabaseProvider> {
            return factory();
        };

        // Default capabilities for third-party providers
        ProviderCapabilities caps;
        caps.supports_transactions = true;
        caps.supports_statements = true;
        caps.supports_authentication = true;
        caps.max_connections = 1000;
        capabilities_registry_[name] = caps;
    }

    std::unique_ptr<DatabaseProvider>
    DatabaseProviderFactory::create_provider(ProviderType type, const ProviderConfig& config)
    {
        std::string provider_name;
        switch (type) {
        case ProviderType::Embedded:
            provider_name = "EmbeddedProvider";
            break;
        case ProviderType::Remote:
            provider_name = "RemoteProvider";
            break;
        case ProviderType::Legacy:
            provider_name = "LegacyProvider";
            break;
        default:
            return nullptr;
        }

        return create_provider(provider_name, config);
    }

    std::unique_ptr<DatabaseProvider>
    DatabaseProviderFactory::create_provider(const std::string& provider_name,
                                             const ProviderConfig& config)
    {
        std::lock_guard<std::mutex> lock(factory_mutex_);

        auto it = factories_.find(provider_name);
        if (it == factories_.end()) {
            return nullptr;
        }

        if (!config.validate()) {
            return nullptr;
        }

        return it->second(config);
    }

    std::vector<std::string> DatabaseProviderFactory::get_registered_providers() const
    {
        std::lock_guard<std::mutex> lock(factory_mutex_);

        std::vector<std::string> providers;
        providers.reserve(factories_.size());

        for (const auto& pair : factories_) {
            providers.push_back(pair.first);
        }

        return providers;
    }

    bool DatabaseProviderFactory::is_provider_registered(const std::string& provider_name) const
    {
        std::lock_guard<std::mutex> lock(factory_mutex_);
        return factories_.find(provider_name) != factories_.end();
    }

    ProviderCapabilities
    DatabaseProviderFactory::get_provider_capabilities(const std::string& provider_name) const
    {
        std::lock_guard<std::mutex> lock(factory_mutex_);

        auto it = capabilities_registry_.find(provider_name);
        if (it != capabilities_registry_.end()) {
            return it->second;
        }

        return ProviderCapabilities{}; // Default empty capabilities
    }

    /// EmbeddedProvider implementation
    EmbeddedProvider::EmbeddedProvider(const ProviderConfig& config)
        : config_(config), initialized_(false), active_connections_(0)
    {

        // Initialize embedded database components
        catalog_manager_ = std::make_unique<CatalogManager>("/tmp/default.db");
    }

    EmbeddedProvider::~EmbeddedProvider()
    {
        if (initialized_) {
            shutdown();
        }
    }

    ProviderType EmbeddedProvider::get_provider_type() const
    {
        return ProviderType::Embedded;
    }

    std::string EmbeddedProvider::get_provider_name() const
    {
        return config_.provider_name.empty() ? "EmbeddedProvider" : config_.provider_name;
    }

    std::string EmbeddedProvider::get_provider_version() const
    {
        return config_.provider_version;
    }

    ProviderCapabilities EmbeddedProvider::get_capabilities() const
    {
        return config_.capabilities;
    }

    bool EmbeddedProvider::initialize()
    {
        std::lock_guard<std::mutex> lock(provider_mutex_);

        if (initialized_) {
            return true;
        }

        try {
            // Initialize embedded provider - catalog manager doesn't need explicit initialization

            initialized_ = true;
            return true;

        } catch (const std::exception& e) {
            last_error_ = std::string("Initialization error: ") + e.what();
            return false;
        }
    }

    void EmbeddedProvider::shutdown()
    {
        std::lock_guard<std::mutex> lock(provider_mutex_);

        if (!initialized_) {
            return;
        }

        try {
            // Shutdown embedded provider - catalog manager cleanup handled in destructor

            initialized_ = false;
            active_connections_ = 0;

        } catch (const std::exception& e) {
            last_error_ = std::string("Shutdown error: ") + e.what();
        }
    }

    bool EmbeddedProvider::is_initialized() const
    {
        return initialized_;
    }

    bool EmbeddedProvider::can_handle_connection(const ConnectionInfo& conn_info) const
    {
        // Embedded provider handles local connections without hostnames
        return conn_info.get_provider_type() == ProviderType::Embedded;
    }

    std::unique_ptr<DatabaseOperations> EmbeddedProvider::create_database_operations()
    {
        if (!initialized_) {
            return nullptr;
        }
        return std::make_unique<EmbeddedDatabaseOperations>(*this);
    }

    std::unique_ptr<TransactionOperations> EmbeddedProvider::create_transaction_operations()
    {
        if (!initialized_) {
            return nullptr;
        }
        return std::make_unique<EmbeddedTransactionOperations>(*this);
    }

    std::unique_ptr<StatementOperations> EmbeddedProvider::create_statement_operations()
    {
        if (!initialized_) {
            return nullptr;
        }
        return std::make_unique<EmbeddedStatementOperations>(*this);
    }

    std::unique_ptr<SecurityOperations> EmbeddedProvider::create_security_operations()
    {
        if (!initialized_) {
            return nullptr;
        }
        return std::make_unique<EmbeddedSecurityOperations>(*this);
    }

    void EmbeddedProvider::cleanup_resources()
    {
        std::lock_guard<std::mutex> lock(provider_mutex_);

        // Cleanup any cached resources - catalog manager cleanup handled automatically
    }

    std::uint32_t EmbeddedProvider::get_active_connections() const
    {
        return active_connections_;
    }

    ProviderStats EmbeddedProvider::get_statistics() const
    {
        return statistics_;
    }

    std::string EmbeddedProvider::get_last_error() const
    {
        std::lock_guard<std::mutex> lock(provider_mutex_);
        return last_error_;
    }

    // Embedded operation classes would be implemented here
    // For now, providing stub implementations to avoid compilation errors

    class EmbeddedProvider::EmbeddedDatabaseOperations : public DatabaseOperations
    {
      private:
        EmbeddedProvider& provider_;

      public:
        explicit EmbeddedDatabaseOperations(EmbeddedProvider& provider) : provider_(provider) {}

        ProviderResult connect(const ConnectionInfo&, std::uint32_t& connection_handle) override
        {
            connection_handle = ++provider_.active_connections_;
            provider_.statistics_.connections_created++;
            provider_.statistics_.connections_active++;
            return ProviderResult::Success;
        }

        ProviderResult disconnect(std::uint32_t) override
        {
            if (provider_.active_connections_ > 0) {
                --provider_.active_connections_;
                provider_.statistics_.connections_active--;
            }
            return ProviderResult::Success;
        }

        bool is_connected(std::uint32_t) const override
        {
            return true;
        }
        ProviderResult attach_database(std::uint32_t, const std::string&,
                                       std::uint32_t& database_handle) override
        {
            database_handle = 1000;
            return ProviderResult::Success;
        }
        ProviderResult detach_database(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult create_database(const std::string&, const ConnectionInfo&,
                                       std::uint32_t& database_handle) override
        {
            database_handle = 1000;
            return ProviderResult::Success;
        }
        ProviderResult start_transaction(std::uint32_t, std::uint32_t& transaction_handle) override
        {
            transaction_handle = 2000;
            return ProviderResult::Success;
        }
        ProviderResult commit_transaction(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult rollback_transaction(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult prepare_statement(std::uint32_t, const std::string&,
                                         std::uint32_t& statement_handle) override
        {
            statement_handle = 3000;
            return ProviderResult::Success;
        }
        ProviderResult execute_statement(std::uint32_t, const std::vector<std::string>&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult fetch_results(std::uint32_t, std::vector<std::vector<std::string>>&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult free_statement(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        std::string get_last_error() const override
        {
            return "";
        }
        std::int32_t get_last_error_code() const override
        {
            return 0;
        }
    };

    class EmbeddedProvider::EmbeddedTransactionOperations : public TransactionOperations
    {
      private:
        EmbeddedProvider& provider_;

      public:
        explicit EmbeddedTransactionOperations(EmbeddedProvider& provider) : provider_(provider) {}

        ProviderResult begin_transaction(std::uint32_t, std::uint32_t& transaction_handle) override
        {
            transaction_handle = 2000;
            return ProviderResult::Success;
        }
        ProviderResult prepare_transaction(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult commit_transaction(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult rollback_transaction(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult rollback_to_savepoint(std::uint32_t, const std::string&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult create_savepoint(std::uint32_t, const std::string&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult release_savepoint(std::uint32_t, const std::string&) override
        {
            return ProviderResult::Success;
        }
        bool is_transaction_active(std::uint32_t) const override
        {
            return true;
        }
        std::string get_transaction_info(std::uint32_t) const override
        {
            return "embedded_transaction";
        }
    };

    class EmbeddedProvider::EmbeddedStatementOperations : public StatementOperations
    {
      private:
        EmbeddedProvider& provider_;

      public:
        explicit EmbeddedStatementOperations(EmbeddedProvider& provider) : provider_(provider) {}

        ProviderResult prepare_statement(std::uint32_t, const std::string&,
                                         std::uint32_t& statement_handle) override
        {
            statement_handle = 3000;
            return ProviderResult::Success;
        }
        ProviderResult execute_prepared(std::uint32_t, const std::vector<std::string>&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult execute_immediate(std::uint32_t, const std::string&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult fetch_next(std::uint32_t, std::vector<std::string>&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult fetch_all(std::uint32_t, std::vector<std::vector<std::string>>&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult close_cursor(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult free_statement(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        bool has_more_results(std::uint32_t) const override
        {
            return false;
        }
        std::size_t get_affected_rows(std::uint32_t) const override
        {
            return 1;
        }
    };

    class EmbeddedProvider::EmbeddedSecurityOperations : public SecurityOperations
    {
      private:
        EmbeddedProvider& provider_;

      public:
        explicit EmbeddedSecurityOperations(EmbeddedProvider& provider) : provider_(provider) {}

        ProviderResult authenticate_user(const std::string&, const std::string&,
                                         std::uint32_t& user_context) override
        {
            user_context = 4000;
            return ProviderResult::Success;
        }
        ProviderResult change_password(std::uint32_t, const std::string&,
                                       const std::string&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult set_role(std::uint32_t, const std::string&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult get_user_roles(std::uint32_t, std::vector<std::string>&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult check_permission(std::uint32_t, const std::string&,
                                        const std::string&) override
        {
            return ProviderResult::Success;
        }
        bool is_authenticated(std::uint32_t) const override
        {
            return true;
        }
        std::string get_current_user(std::uint32_t) const override
        {
            return "embedded_user";
        }
        std::string get_current_role(std::uint32_t) const override
        {
            return "embedded_role";
        }
    };

    /// RemoteProvider implementation
    RemoteProvider::RemoteProvider(const ProviderConfig& config)
        : config_(config), initialized_(false), active_connections_(0)
    {
    }

    RemoteProvider::~RemoteProvider()
    {
        if (initialized_) {
            shutdown();
        }
    }

    ProviderType RemoteProvider::get_provider_type() const
    {
        return ProviderType::Remote;
    }

    std::string RemoteProvider::get_provider_name() const
    {
        return config_.provider_name.empty() ? "RemoteProvider" : config_.provider_name;
    }

    std::string RemoteProvider::get_provider_version() const
    {
        return config_.provider_version;
    }

    ProviderCapabilities RemoteProvider::get_capabilities() const
    {
        return config_.capabilities;
    }

    bool RemoteProvider::initialize()
    {
        std::lock_guard<std::mutex> lock(provider_mutex_);

        if (initialized_) {
            return true;
        }

        try {
            // Initialize remote provider components
            initialized_ = true;
            return true;

        } catch (const std::exception& e) {
            last_error_ = std::string("Initialization error: ") + e.what();
            return false;
        }
    }

    void RemoteProvider::shutdown()
    {
        std::lock_guard<std::mutex> lock(provider_mutex_);

        if (!initialized_) {
            return;
        }

        initialized_ = false;
        active_connections_ = 0;
    }

    bool RemoteProvider::is_initialized() const
    {
        return initialized_;
    }

    bool RemoteProvider::can_handle_connection(const ConnectionInfo& conn_info) const
    {
        // Remote provider handles network connections with hostnames
        return conn_info.get_provider_type() == ProviderType::Remote;
    }

    std::unique_ptr<DatabaseOperations> RemoteProvider::create_database_operations()
    {
        if (!initialized_) {
            return nullptr;
        }
        return std::make_unique<RemoteDatabaseOperations>(*this);
    }

    std::unique_ptr<TransactionOperations> RemoteProvider::create_transaction_operations()
    {
        if (!initialized_) {
            return nullptr;
        }
        return std::make_unique<RemoteTransactionOperations>(*this);
    }

    std::unique_ptr<StatementOperations> RemoteProvider::create_statement_operations()
    {
        if (!initialized_) {
            return nullptr;
        }
        return std::make_unique<RemoteStatementOperations>(*this);
    }

    std::unique_ptr<SecurityOperations> RemoteProvider::create_security_operations()
    {
        if (!initialized_) {
            return nullptr;
        }
        return std::make_unique<RemoteSecurityOperations>(*this);
    }

    void RemoteProvider::cleanup_resources()
    {
        // Cleanup remote connections and caches
    }

    std::uint32_t RemoteProvider::get_active_connections() const
    {
        return active_connections_;
    }

    ProviderStats RemoteProvider::get_statistics() const
    {
        return statistics_;
    }

    std::string RemoteProvider::get_last_error() const
    {
        std::lock_guard<std::mutex> lock(provider_mutex_);
        return last_error_;
    }

    // Remote operation classes - stub implementations
    class RemoteProvider::RemoteDatabaseOperations : public DatabaseOperations
    {
      private:
        RemoteProvider& provider_;

      public:
        explicit RemoteDatabaseOperations(RemoteProvider& provider) : provider_(provider) {}

        ProviderResult connect(const ConnectionInfo&, std::uint32_t& connection_handle) override
        {
            connection_handle = ++provider_.active_connections_;
            return ProviderResult::Success;
        }
        ProviderResult disconnect(std::uint32_t) override
        {
            if (provider_.active_connections_ > 0)
                --provider_.active_connections_;
            return ProviderResult::Success;
        }
        bool is_connected(std::uint32_t) const override
        {
            return true;
        }
        ProviderResult attach_database(std::uint32_t, const std::string&,
                                       std::uint32_t& database_handle) override
        {
            database_handle = 1001;
            return ProviderResult::Success;
        }
        ProviderResult detach_database(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult create_database(const std::string&, const ConnectionInfo&,
                                       std::uint32_t& database_handle) override
        {
            database_handle = 1001;
            return ProviderResult::Success;
        }
        ProviderResult start_transaction(std::uint32_t, std::uint32_t& transaction_handle) override
        {
            transaction_handle = 2001;
            return ProviderResult::Success;
        }
        ProviderResult commit_transaction(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult rollback_transaction(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult prepare_statement(std::uint32_t, const std::string&,
                                         std::uint32_t& statement_handle) override
        {
            statement_handle = 3001;
            return ProviderResult::Success;
        }
        ProviderResult execute_statement(std::uint32_t, const std::vector<std::string>&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult fetch_results(std::uint32_t, std::vector<std::vector<std::string>>&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult free_statement(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        std::string get_last_error() const override
        {
            return "";
        }
        std::int32_t get_last_error_code() const override
        {
            return 0;
        }
    };

    class RemoteProvider::RemoteTransactionOperations : public TransactionOperations
    {
      private:
        RemoteProvider& provider_;

      public:
        explicit RemoteTransactionOperations(RemoteProvider& provider) : provider_(provider) {}

        ProviderResult begin_transaction(std::uint32_t, std::uint32_t& transaction_handle) override
        {
            transaction_handle = 2001;
            return ProviderResult::Success;
        }
        ProviderResult prepare_transaction(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult commit_transaction(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult rollback_transaction(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult rollback_to_savepoint(std::uint32_t, const std::string&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult create_savepoint(std::uint32_t, const std::string&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult release_savepoint(std::uint32_t, const std::string&) override
        {
            return ProviderResult::Success;
        }
        bool is_transaction_active(std::uint32_t) const override
        {
            return true;
        }
        std::string get_transaction_info(std::uint32_t) const override
        {
            return "remote_transaction";
        }
    };

    class RemoteProvider::RemoteStatementOperations : public StatementOperations
    {
      private:
        RemoteProvider& provider_;

      public:
        explicit RemoteStatementOperations(RemoteProvider& provider) : provider_(provider) {}

        ProviderResult prepare_statement(std::uint32_t, const std::string&,
                                         std::uint32_t& statement_handle) override
        {
            statement_handle = 3001;
            return ProviderResult::Success;
        }
        ProviderResult execute_prepared(std::uint32_t, const std::vector<std::string>&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult execute_immediate(std::uint32_t, const std::string&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult fetch_next(std::uint32_t, std::vector<std::string>&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult fetch_all(std::uint32_t, std::vector<std::vector<std::string>>&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult close_cursor(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult free_statement(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        bool has_more_results(std::uint32_t) const override
        {
            return false;
        }
        std::size_t get_affected_rows(std::uint32_t) const override
        {
            return 1;
        }
    };

    class RemoteProvider::RemoteSecurityOperations : public SecurityOperations
    {
      private:
        RemoteProvider& provider_;

      public:
        explicit RemoteSecurityOperations(RemoteProvider& provider) : provider_(provider) {}

        ProviderResult authenticate_user(const std::string&, const std::string&,
                                         std::uint32_t& user_context) override
        {
            user_context = 4001;
            return ProviderResult::Success;
        }
        ProviderResult change_password(std::uint32_t, const std::string&,
                                       const std::string&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult set_role(std::uint32_t, const std::string&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult get_user_roles(std::uint32_t, std::vector<std::string>&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult check_permission(std::uint32_t, const std::string&,
                                        const std::string&) override
        {
            return ProviderResult::Success;
        }
        bool is_authenticated(std::uint32_t) const override
        {
            return true;
        }
        std::string get_current_user(std::uint32_t) const override
        {
            return "remote_user";
        }
        std::string get_current_role(std::uint32_t) const override
        {
            return "remote_role";
        }
    };

    /// LegacyProvider implementation
    LegacyProvider::LegacyProvider(const ProviderConfig& config)
        : config_(config), initialized_(false), active_connections_(0)
    {
    }

    LegacyProvider::~LegacyProvider()
    {
        if (initialized_) {
            shutdown();
        }
    }

    ProviderType LegacyProvider::get_provider_type() const
    {
        return ProviderType::Legacy;
    }

    std::string LegacyProvider::get_provider_name() const
    {
        return config_.provider_name.empty() ? "LegacyProvider" : config_.provider_name;
    }

    std::string LegacyProvider::get_provider_version() const
    {
        return config_.provider_version;
    }

    ProviderCapabilities LegacyProvider::get_capabilities() const
    {
        return config_.capabilities;
    }

    bool LegacyProvider::initialize()
    {
        std::lock_guard<std::mutex> lock(provider_mutex_);

        if (initialized_) {
            return true;
        }

        initialized_ = true;
        return true;
    }

    void LegacyProvider::shutdown()
    {
        std::lock_guard<std::mutex> lock(provider_mutex_);

        if (!initialized_) {
            return;
        }

        initialized_ = false;
        active_connections_ = 0;
    }

    bool LegacyProvider::is_initialized() const
    {
        return initialized_;
    }

    bool LegacyProvider::can_handle_connection(const ConnectionInfo& conn_info) const
    {
        return conn_info.get_provider_type() == ProviderType::Legacy;
    }

    std::unique_ptr<DatabaseOperations> LegacyProvider::create_database_operations()
    {
        if (!initialized_) {
            return nullptr;
        }
        return std::make_unique<LegacyDatabaseOperations>(*this);
    }

    std::unique_ptr<TransactionOperations> LegacyProvider::create_transaction_operations()
    {
        if (!initialized_) {
            return nullptr;
        }
        return std::make_unique<LegacyTransactionOperations>(*this);
    }

    std::unique_ptr<StatementOperations> LegacyProvider::create_statement_operations()
    {
        if (!initialized_) {
            return nullptr;
        }
        return std::make_unique<LegacyStatementOperations>(*this);
    }

    std::unique_ptr<SecurityOperations> LegacyProvider::create_security_operations()
    {
        if (!initialized_) {
            return nullptr;
        }
        return std::make_unique<LegacySecurityOperations>(*this);
    }

    void LegacyProvider::cleanup_resources()
    {
        // Legacy resource cleanup
    }

    std::uint32_t LegacyProvider::get_active_connections() const
    {
        return active_connections_;
    }

    ProviderStats LegacyProvider::get_statistics() const
    {
        return statistics_;
    }

    std::string LegacyProvider::get_last_error() const
    {
        std::lock_guard<std::mutex> lock(provider_mutex_);
        return last_error_;
    }

    // Legacy operation classes - minimal stub implementations
    class LegacyProvider::LegacyDatabaseOperations : public DatabaseOperations
    {
      private:
        LegacyProvider& provider_;

      public:
        explicit LegacyDatabaseOperations(LegacyProvider& provider) : provider_(provider) {}

        ProviderResult connect(const ConnectionInfo&, std::uint32_t& connection_handle) override
        {
            connection_handle = ++provider_.active_connections_;
            return ProviderResult::Success;
        }
        ProviderResult disconnect(std::uint32_t) override
        {
            if (provider_.active_connections_ > 0)
                --provider_.active_connections_;
            return ProviderResult::Success;
        }
        bool is_connected(std::uint32_t) const override
        {
            return true;
        }
        ProviderResult attach_database(std::uint32_t, const std::string&,
                                       std::uint32_t& database_handle) override
        {
            database_handle = 1002;
            return ProviderResult::Success;
        }
        ProviderResult detach_database(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult create_database(const std::string&, const ConnectionInfo&,
                                       std::uint32_t& database_handle) override
        {
            database_handle = 1002;
            return ProviderResult::Success;
        }
        ProviderResult start_transaction(std::uint32_t, std::uint32_t& transaction_handle) override
        {
            transaction_handle = 2002;
            return ProviderResult::Success;
        }
        ProviderResult commit_transaction(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult rollback_transaction(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult prepare_statement(std::uint32_t, const std::string&,
                                         std::uint32_t& statement_handle) override
        {
            statement_handle = 3002;
            return ProviderResult::Success;
        }
        ProviderResult execute_statement(std::uint32_t, const std::vector<std::string>&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult fetch_results(std::uint32_t, std::vector<std::vector<std::string>>&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult free_statement(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        std::string get_last_error() const override
        {
            return "";
        }
        std::int32_t get_last_error_code() const override
        {
            return 0;
        }
    };

    class LegacyProvider::LegacyTransactionOperations : public TransactionOperations
    {
      private:
        LegacyProvider& provider_;

      public:
        explicit LegacyTransactionOperations(LegacyProvider& provider) : provider_(provider) {}

        ProviderResult begin_transaction(std::uint32_t, std::uint32_t& transaction_handle) override
        {
            transaction_handle = 2002;
            return ProviderResult::Success;
        }
        ProviderResult prepare_transaction(std::uint32_t) override
        {
            return ProviderResult::NotSupported;
        }
        ProviderResult commit_transaction(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult rollback_transaction(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult rollback_to_savepoint(std::uint32_t, const std::string&) override
        {
            return ProviderResult::NotSupported;
        }
        ProviderResult create_savepoint(std::uint32_t, const std::string&) override
        {
            return ProviderResult::NotSupported;
        }
        ProviderResult release_savepoint(std::uint32_t, const std::string&) override
        {
            return ProviderResult::NotSupported;
        }
        bool is_transaction_active(std::uint32_t) const override
        {
            return true;
        }
        std::string get_transaction_info(std::uint32_t) const override
        {
            return "legacy_transaction";
        }
    };

    class LegacyProvider::LegacyStatementOperations : public StatementOperations
    {
      private:
        LegacyProvider& provider_;

      public:
        explicit LegacyStatementOperations(LegacyProvider& provider) : provider_(provider) {}

        ProviderResult prepare_statement(std::uint32_t, const std::string&,
                                         std::uint32_t& statement_handle) override
        {
            statement_handle = 3002;
            return ProviderResult::Success;
        }
        ProviderResult execute_prepared(std::uint32_t, const std::vector<std::string>&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult execute_immediate(std::uint32_t, const std::string&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult fetch_next(std::uint32_t, std::vector<std::string>&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult fetch_all(std::uint32_t, std::vector<std::vector<std::string>>&) override
        {
            return ProviderResult::Success;
        }
        ProviderResult close_cursor(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        ProviderResult free_statement(std::uint32_t) override
        {
            return ProviderResult::Success;
        }
        bool has_more_results(std::uint32_t) const override
        {
            return false;
        }
        std::size_t get_affected_rows(std::uint32_t) const override
        {
            return 1;
        }
    };

    class LegacyProvider::LegacySecurityOperations : public SecurityOperations
    {
      private:
        LegacyProvider& provider_;

      public:
        explicit LegacySecurityOperations(LegacyProvider& provider) : provider_(provider) {}

        ProviderResult authenticate_user(const std::string&, const std::string&,
                                         std::uint32_t&) override
        {
            return ProviderResult::NotSupported; // Legacy provider doesn't support authentication
        }
        ProviderResult change_password(std::uint32_t, const std::string&,
                                       const std::string&) override
        {
            return ProviderResult::NotSupported;
        }
        ProviderResult set_role(std::uint32_t, const std::string&) override
        {
            return ProviderResult::NotSupported;
        }
        ProviderResult get_user_roles(std::uint32_t, std::vector<std::string>&) override
        {
            return ProviderResult::NotSupported;
        }
        ProviderResult check_permission(std::uint32_t, const std::string&,
                                        const std::string&) override
        {
            return ProviderResult::NotSupported;
        }
        bool is_authenticated(std::uint32_t) const override
        {
            return false;
        }
        std::string get_current_user(std::uint32_t) const override
        {
            return "";
        }
        std::string get_current_role(std::uint32_t) const override
        {
            return "";
        }
    };

} // namespace scratchbird::engine
