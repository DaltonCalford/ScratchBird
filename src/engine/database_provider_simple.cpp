#include "scratchbird/engine/database_provider.h"

#include <algorithm>
#include <sstream>

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

    /// Implementation of SimpleProvider operation creation methods

    // Simple operation implementations
    class SimpleOperations
    {
      public:
        class SimpleDatabaseOperations : public DatabaseOperations
        {
          public:
            SimpleDatabaseOperations(SimpleProvider* provider) : provider_(provider) {}

            ProviderResult connect(const ConnectionInfo&, std::uint32_t& connection_handle) override
            {
                connection_handle = 1000;
                provider_->increment_active_connections();
                return ProviderResult::Success;
            }
            ProviderResult disconnect(std::uint32_t) override
            {
                provider_->decrement_active_connections();
                return ProviderResult::Success;
            }
            bool is_connected(std::uint32_t) const override
            {
                return true;
            }
            ProviderResult attach_database(std::uint32_t, const std::string&,
                                           std::uint32_t& database_handle) override
            {
                database_handle = 2000;
                return ProviderResult::Success;
            }
            ProviderResult detach_database(std::uint32_t) override
            {
                return ProviderResult::Success;
            }
            ProviderResult create_database(const std::string&, const ConnectionInfo&,
                                           std::uint32_t& database_handle) override
            {
                database_handle = 2000;
                return ProviderResult::Success;
            }
            ProviderResult start_transaction(std::uint32_t,
                                             std::uint32_t& transaction_handle) override
            {
                transaction_handle = 3000;
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
                statement_handle = 4000;
                return ProviderResult::Success;
            }
            ProviderResult execute_statement(std::uint32_t,
                                             const std::vector<std::string>&) override
            {
                return ProviderResult::Success;
            }
            ProviderResult fetch_results(std::uint32_t,
                                         std::vector<std::vector<std::string>>&) override
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

          private:
            SimpleProvider* provider_;
        };

        class SimpleTransactionOperations : public TransactionOperations
        {
          public:
            SimpleTransactionOperations(ProviderType provider_type) : provider_type_(provider_type)
            {
            }

            ProviderResult begin_transaction(std::uint32_t,
                                             std::uint32_t& transaction_handle) override
            {
                transaction_handle = 3000;
                return ProviderResult::Success;
            }
            ProviderResult prepare_transaction(std::uint32_t) override
            {
                // Legacy provider doesn't support prepared transactions
                return (provider_type_ == ProviderType::Legacy) ? ProviderResult::NotSupported
                                                                : ProviderResult::Success;
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
                // Legacy provider doesn't support savepoints
                return (provider_type_ == ProviderType::Legacy) ? ProviderResult::NotSupported
                                                                : ProviderResult::Success;
            }
            ProviderResult create_savepoint(std::uint32_t, const std::string&) override
            {
                // Legacy provider doesn't support savepoints
                return (provider_type_ == ProviderType::Legacy) ? ProviderResult::NotSupported
                                                                : ProviderResult::Success;
            }
            ProviderResult release_savepoint(std::uint32_t, const std::string&) override
            {
                // Legacy provider doesn't support savepoints
                return (provider_type_ == ProviderType::Legacy) ? ProviderResult::NotSupported
                                                                : ProviderResult::Success;
            }
            bool is_transaction_active(std::uint32_t) const override
            {
                return true;
            }
            std::string get_transaction_info(std::uint32_t) const override
            {
                switch (provider_type_) {
                case ProviderType::Embedded:
                    return "embedded_transaction";
                case ProviderType::Remote:
                    return "remote_transaction";
                case ProviderType::Legacy:
                    return "legacy_transaction";
                default:
                    return "unknown_transaction";
                }
            }

          private:
            ProviderType provider_type_;
        };

        class SimpleStatementOperations : public StatementOperations
        {
          public:
            ProviderResult prepare_statement(std::uint32_t, const std::string&,
                                             std::uint32_t& statement_handle) override
            {
                statement_handle = 4000;
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

        class SimpleSecurityOperations : public SecurityOperations
        {
          public:
            SimpleSecurityOperations(ProviderType provider_type)
                : provider_type_(provider_type),
                  supports_auth_(provider_type != ProviderType::Legacy)
            {
            }

            ProviderResult authenticate_user(const std::string&, const std::string&,
                                             std::uint32_t& user_context) override
            {
                if (!supports_auth_)
                    return ProviderResult::NotSupported;
                user_context = 5000;
                return ProviderResult::Success;
            }
            ProviderResult change_password(std::uint32_t, const std::string&,
                                           const std::string&) override
            {
                return supports_auth_ ? ProviderResult::Success : ProviderResult::NotSupported;
            }
            ProviderResult set_role(std::uint32_t, const std::string&) override
            {
                return supports_auth_ ? ProviderResult::Success : ProviderResult::NotSupported;
            }
            ProviderResult get_user_roles(std::uint32_t, std::vector<std::string>&) override
            {
                return supports_auth_ ? ProviderResult::Success : ProviderResult::NotSupported;
            }
            ProviderResult check_permission(std::uint32_t, const std::string&,
                                            const std::string&) override
            {
                return supports_auth_ ? ProviderResult::Success : ProviderResult::NotSupported;
            }
            bool is_authenticated(std::uint32_t) const override
            {
                return supports_auth_;
            }
            std::string get_current_user(std::uint32_t) const override
            {
                if (!supports_auth_)
                    return "";
                switch (provider_type_) {
                case ProviderType::Embedded:
                    return "embedded_user";
                case ProviderType::Remote:
                    return "remote_user";
                default:
                    return "";
                }
            }
            std::string get_current_role(std::uint32_t) const override
            {
                if (!supports_auth_)
                    return "";
                switch (provider_type_) {
                case ProviderType::Embedded:
                    return "embedded_role";
                case ProviderType::Remote:
                    return "remote_role";
                default:
                    return "";
                }
            }

          private:
            ProviderType provider_type_;
            bool supports_auth_;
        };
    };

    std::unique_ptr<DatabaseOperations> SimpleProvider::create_database_operations()
    {
        return std::make_unique<SimpleOperations::SimpleDatabaseOperations>(this);
    }

    std::unique_ptr<TransactionOperations> SimpleProvider::create_transaction_operations()
    {
        return std::make_unique<SimpleOperations::SimpleTransactionOperations>(type_);
    }

    std::unique_ptr<StatementOperations> SimpleProvider::create_statement_operations()
    {
        return std::make_unique<SimpleOperations::SimpleStatementOperations>();
    }

    std::unique_ptr<SecurityOperations> SimpleProvider::create_security_operations()
    {
        return std::make_unique<SimpleOperations::SimpleSecurityOperations>(type_);
    }

    /// Concrete provider implementations using simple base
    EmbeddedProvider::EmbeddedProvider(const ProviderConfig& config)
        : SimpleProvider(config, ProviderType::Embedded)
    {
    }

    RemoteProvider::RemoteProvider(const ProviderConfig& config)
        : SimpleProvider(config, ProviderType::Remote)
    {
    }

    LegacyProvider::LegacyProvider(const ProviderConfig& config)
        : SimpleProvider(config, ProviderType::Legacy)
    {
    }

} // namespace scratchbird::engine
