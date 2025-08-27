#include "scratchbird/engine/provider_dispatch.h"

#include <algorithm>
#include <chrono>
#include <random>
#include <regex>
#include <sstream>
#include <thread>

namespace scratchbird::engine
{

    /// ProviderCapabilities implementation
    bool ProviderCapabilities::is_compatible_with(const ProviderCapabilities& required) const
    {
        if (required.supports_transactions && !supports_transactions)
            return false;
        if (required.supports_statements && !supports_statements)
            return false;
        if (required.supports_authentication && !supports_authentication)
            return false;
        if (required.supports_encryption && !supports_encryption)
            return false;
        if (required.supports_compression && !supports_compression)
            return false;
        if (required.supports_streaming && !supports_streaming)
            return false;
        if (required.supports_batch_operations && !supports_batch_operations)
            return false;
        if (required.supports_async_operations && !supports_async_operations)
            return false;

        if (required.max_connections > 0 && max_connections > 0 &&
            max_connections < required.max_connections)
            return false;
        if (required.max_databases > 0 && max_databases > 0 &&
            max_databases < required.max_databases)
            return false;

        return true;
    }

    /// ConnectionInfo implementation
    ConnectionInfo ConnectionInfo::parse_connection_string(const std::string& connection_string)
    {
        ConnectionInfo conn_info;

        // Parse connection strings like:
        // embedded:/path/to/database
        // tcp://hostname:port/database
        // inet://hostname/database
        // localhost:3050:/path/to/database

        std::regex connection_regex(
            R"(^(?:([^:]+)://)?(?:([^:@]+)(?::([^@]+))?@)?(?:([^:/]+)(?::(\d+))?[:/])?(.+)$)");
        std::smatch matches;

        if (std::regex_match(connection_string, matches, connection_regex)) {
            if (matches[1].matched) {
                conn_info.protocol = matches[1].str();
            }
            if (matches[2].matched) {
                conn_info.username = matches[2].str();
            }
            if (matches[3].matched) {
                conn_info.password = matches[3].str();
            }
            if (matches[4].matched) {
                conn_info.hostname = matches[4].str();
            }
            if (matches[5].matched) {
                conn_info.port = static_cast<std::uint16_t>(std::stoul(matches[5].str()));
            }
            if (matches[6].matched) {
                conn_info.database_path = matches[6].str();
            }
        } else {
            // Fallback: treat entire string as database path
            conn_info.database_path = connection_string;
        }

        // Set default protocol if not specified
        if (conn_info.protocol.empty()) {
            if (!conn_info.hostname.empty()) {
                conn_info.protocol = "tcp";
            } else {
                conn_info.protocol = "embedded";
            }
        }

        // Set default port if not specified
        if (conn_info.port == 0 && !conn_info.hostname.empty()) {
            conn_info.port = 3050; // Default Firebird port
        }

        return conn_info;
    }

    std::string ConnectionInfo::to_connection_string() const
    {
        std::ostringstream oss;

        if (!protocol.empty()) {
            oss << protocol << "://";
        }

        if (!username.empty()) {
            oss << username;
            if (!password.empty()) {
                oss << ":" << password;
            }
            oss << "@";
        }

        if (!hostname.empty()) {
            oss << hostname;
            if (port != 0) {
                oss << ":" << port;
            }
            oss << "/";
        }

        oss << database_path;

        return oss.str();
    }

    ProviderType ConnectionInfo::get_provider_type() const
    {
        if (protocol == "embedded" || hostname.empty()) {
            return ProviderType::Embedded;
        } else if (protocol == "tcp" || protocol == "inet" || !hostname.empty()) {
            return ProviderType::Remote;
        } else if (protocol == "legacy") {
            return ProviderType::Legacy;
        } else {
            return ProviderType::ThirdParty;
        }
    }

    /// ProviderStats implementation
    void ProviderStats::reset()
    {
        connections_created = 0;
        connections_active = 0;
        connections_failed = 0;
        requests_processed = 0;
        errors_encountered = 0;
        bytes_transferred = 0;
    }

    std::map<std::string, std::uint64_t> ProviderStats::get_metrics() const
    {
        return {{"connections_created", connections_created.load()},
                {"connections_active", connections_active.load()},
                {"connections_failed", connections_failed.load()},
                {"requests_processed", requests_processed.load()},
                {"errors_encountered", errors_encountered.load()},
                {"bytes_transferred", bytes_transferred.load()}};
    }

    /// YValveDispatcher implementation
    YValveDispatcher::YValveDispatcher()
        : load_balancing_strategy_(LoadBalancingStrategy::PriorityBased), next_connection_id_(1),
          round_robin_index_(0), health_monitoring_enabled_(false), health_monitor_running_(false),
          max_providers_per_type_(10), connection_timeout_ms_(30000),
          provider_isolation_enabled_(true)
    {
    }

    YValveDispatcher::~YValveDispatcher()
    {
        health_monitoring_enabled_ = false;
        health_monitor_running_ = false;

        if (health_monitor_thread_.joinable()) {
            health_monitor_thread_.join();
        }

        // Cleanup active connections
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);
        active_connections_.clear();
        providers_.clear();
    }

    bool YValveDispatcher::register_provider(const ProviderRegistration& registration)
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);

        // Check if provider already registered
        if (provider_index_.find(registration.name) != provider_index_.end()) {
            return false;
        }

        // Create provider instance
        auto provider = registration.factory();
        if (!provider || !provider->initialize()) {
            return false;
        }

        // Add to provider list
        std::size_t index = providers_.size();
        providers_.push_back(std::move(provider));
        provider_index_[registration.name] = index;
        provider_health_[registration.name] = true;

        // Sort providers by priority
        std::sort(providers_.begin(), providers_.end(),
                  [](const std::unique_ptr<DatabaseProvider>& a,
                     const std::unique_ptr<DatabaseProvider>& b) {
                      return a->get_capabilities().max_connections <
                             b->get_capabilities().max_connections;
                  });

        // Rebuild index after sorting
        provider_index_.clear();
        for (std::size_t i = 0; i < providers_.size(); ++i) {
            provider_index_[providers_[i]->get_provider_name()] = i;
        }

        return true;
    }

    bool YValveDispatcher::unregister_provider(const std::string& provider_name)
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);

        auto it = provider_index_.find(provider_name);
        if (it == provider_index_.end()) {
            return false;
        }

        std::size_t index = it->second;

        // Shutdown provider
        providers_[index]->shutdown();

        // Remove from collections
        providers_.erase(providers_.begin() + index);
        provider_index_.erase(it);
        provider_health_.erase(provider_name);

        // Rebuild index
        provider_index_.clear();
        for (std::size_t i = 0; i < providers_.size(); ++i) {
            provider_index_[providers_[i]->get_provider_name()] = i;
        }

        return true;
    }

    std::vector<std::string> YValveDispatcher::get_registered_providers() const
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);

        std::vector<std::string> names;
        names.reserve(providers_.size());

        for (const auto& provider : providers_) {
            names.push_back(provider->get_provider_name());
        }

        return names;
    }

    bool YValveDispatcher::is_provider_registered(const std::string& provider_name) const
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);
        return provider_index_.find(provider_name) != provider_index_.end();
    }

    DatabaseProvider* YValveDispatcher::get_provider_for_connection(const ConnectionInfo& conn_info)
    {
        std::vector<DatabaseProvider*> candidates = get_compatible_providers(conn_info);

        if (candidates.empty()) {
            return nullptr;
        }

        return select_provider_by_strategy(candidates);
    }

    std::vector<DatabaseProvider*>
    YValveDispatcher::get_compatible_providers(const ConnectionInfo& conn_info)
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);

        std::vector<DatabaseProvider*> compatible_providers;

        for (auto& provider : providers_) {
            if (provider->can_handle_connection(conn_info)) {
                // Check health if monitoring is enabled
                if (health_monitoring_enabled_) {
                    auto health_it = provider_health_.find(provider->get_provider_name());
                    if (health_it != provider_health_.end() && !health_it->second.load()) {
                        continue; // Skip unhealthy providers
                    }
                }

                compatible_providers.push_back(provider.get());
            }
        }

        return compatible_providers;
    }

    DatabaseProvider* YValveDispatcher::get_provider_by_name(const std::string& provider_name)
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);

        auto it = provider_index_.find(provider_name);
        if (it == provider_index_.end()) {
            return nullptr;
        }

        return providers_[it->second].get();
    }

    void YValveDispatcher::set_load_balancing_strategy(LoadBalancingStrategy strategy)
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);
        load_balancing_strategy_ = strategy;
    }

    LoadBalancingStrategy YValveDispatcher::get_load_balancing_strategy() const
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);
        return load_balancing_strategy_;
    }

    void YValveDispatcher::set_failover_config(const FailoverConfig& config)
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);
        failover_config_ = config;
    }

    FailoverConfig YValveDispatcher::get_failover_config() const
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);
        return failover_config_;
    }

    ProviderResult YValveDispatcher::route_connection(const ConnectionInfo& conn_info,
                                                      std::uint32_t& connection_id)
    {
        DatabaseProvider* provider = get_provider_for_connection(conn_info);
        if (!provider) {
            dispatcher_stats_.connections_failed++;
            return ProviderResult::ConnectionFailed;
        }

        // Create connection operations
        auto db_ops = provider->create_database_operations();
        auto txn_ops = provider->create_transaction_operations();
        auto stmt_ops = provider->create_statement_operations();
        auto sec_ops = provider->create_security_operations();

        if (!db_ops || !txn_ops || !stmt_ops || !sec_ops) {
            dispatcher_stats_.connections_failed++;
            return ProviderResult::ResourceExhausted;
        }

        // Attempt connection
        std::uint32_t provider_connection_handle;
        ProviderResult result = db_ops->connect(conn_info, provider_connection_handle);

        if (result == ProviderResult::Success) {
            // Create connection record
            connection_id = allocate_connection_id();
            auto record = std::make_unique<ConnectionRecord>();
            record->connection_id = connection_id;
            record->conn_info = conn_info;
            record->provider = provider;
            record->db_ops = std::move(db_ops);
            record->txn_ops = std::move(txn_ops);
            record->stmt_ops = std::move(stmt_ops);
            record->sec_ops = std::move(sec_ops);
            record->created_at = std::chrono::steady_clock::now();
            record->is_active = true;

            std::lock_guard<std::mutex> lock(dispatcher_mutex_);
            active_connections_[connection_id] = std::move(record);

            dispatcher_stats_.connections_created++;
            dispatcher_stats_.connections_active++;
        } else {
            dispatcher_stats_.connections_failed++;
        }

        return result;
    }

    ProviderResult YValveDispatcher::close_connection(std::uint32_t connection_id)
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);

        auto it = active_connections_.find(connection_id);
        if (it == active_connections_.end()) {
            return ProviderResult::Error;
        }

        auto& record = it->second;
        ProviderResult result = record->db_ops->disconnect(connection_id);

        active_connections_.erase(it);
        if (dispatcher_stats_.connections_active > 0) {
            dispatcher_stats_.connections_active--;
        }

        return result;
    }

    DatabaseOperations* YValveDispatcher::get_database_operations(std::uint32_t connection_id)
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);

        auto it = active_connections_.find(connection_id);
        if (it == active_connections_.end() || !it->second->is_active) {
            return nullptr;
        }

        return it->second->db_ops.get();
    }

    TransactionOperations* YValveDispatcher::get_transaction_operations(std::uint32_t connection_id)
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);

        auto it = active_connections_.find(connection_id);
        if (it == active_connections_.end() || !it->second->is_active) {
            return nullptr;
        }

        return it->second->txn_ops.get();
    }

    StatementOperations* YValveDispatcher::get_statement_operations(std::uint32_t connection_id)
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);

        auto it = active_connections_.find(connection_id);
        if (it == active_connections_.end() || !it->second->is_active) {
            return nullptr;
        }

        return it->second->stmt_ops.get();
    }

    SecurityOperations* YValveDispatcher::get_security_operations(std::uint32_t connection_id)
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);

        auto it = active_connections_.find(connection_id);
        if (it == active_connections_.end() || !it->second->is_active) {
            return nullptr;
        }

        return it->second->sec_ops.get();
    }

    void YValveDispatcher::enable_health_monitoring(bool enabled)
    {
        health_monitoring_enabled_ = enabled;

        if (enabled && !health_monitor_running_) {
            health_monitor_running_ = true;
            health_monitor_thread_ = std::thread(&YValveDispatcher::health_monitor_worker, this);
        } else if (!enabled && health_monitor_running_) {
            health_monitor_running_ = false;
            if (health_monitor_thread_.joinable()) {
                health_monitor_thread_.join();
            }
        }
    }

    bool YValveDispatcher::is_health_monitoring_enabled() const
    {
        return health_monitoring_enabled_;
    }

    std::map<std::string, bool> YValveDispatcher::get_provider_health_status() const
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);

        std::map<std::string, bool> status;
        for (const auto& pair : provider_health_) {
            status[pair.first] = pair.second.load();
        }

        return status;
    }

    void YValveDispatcher::force_health_check()
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);

        for (auto& provider : providers_) {
            bool healthy = check_provider_health(provider.get());
            provider_health_[provider->get_provider_name()] = healthy;
        }
    }

    std::map<std::string, ProviderStats> YValveDispatcher::get_all_provider_stats() const
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);

        std::map<std::string, ProviderStats> all_stats;
        for (const auto& provider : providers_) {
            ProviderStats stats = provider->get_statistics();
            all_stats.emplace(provider->get_provider_name(), std::move(stats));
        }

        return all_stats;
    }

    ProviderStats YValveDispatcher::get_dispatcher_stats() const
    {
        return dispatcher_stats_;
    }

    void YValveDispatcher::reset_statistics()
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);

        dispatcher_stats_.reset();
        // Provider stats reset would need provider-specific implementation
        (void)providers_; // Suppress unused variable warning
    }

    void YValveDispatcher::set_max_providers_per_type(std::uint32_t max_count)
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);
        max_providers_per_type_ = max_count;
    }

    void YValveDispatcher::set_connection_timeout(std::uint32_t timeout_ms)
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);
        connection_timeout_ms_ = timeout_ms;
    }

    void YValveDispatcher::enable_provider_isolation(bool enabled)
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);
        provider_isolation_enabled_ = enabled;
    }

    /// Private helper methods
    DatabaseProvider*
    YValveDispatcher::select_provider_by_strategy(const std::vector<DatabaseProvider*>& candidates)
    {
        if (candidates.empty()) {
            return nullptr;
        }

        switch (load_balancing_strategy_) {
        case LoadBalancingStrategy::RoundRobin:
            return select_round_robin(candidates);
        case LoadBalancingStrategy::LeastConnections:
            return select_least_connections(candidates);
        case LoadBalancingStrategy::Random:
            return select_random(candidates);
        case LoadBalancingStrategy::PriorityBased:
            return select_by_priority(candidates);
        case LoadBalancingStrategy::WeightedRoundRobin:
            // For now, fall back to round robin
            return select_round_robin(candidates);
        default:
            return candidates[0];
        }
    }

    DatabaseProvider*
    YValveDispatcher::select_round_robin(const std::vector<DatabaseProvider*>& candidates)
    {
        std::uint32_t index = round_robin_index_.fetch_add(1) % candidates.size();
        return candidates[index];
    }

    DatabaseProvider*
    YValveDispatcher::select_least_connections(const std::vector<DatabaseProvider*>& candidates)
    {
        DatabaseProvider* best_provider = candidates[0];
        std::uint32_t min_connections = best_provider->get_active_connections();

        for (std::size_t i = 1; i < candidates.size(); ++i) {
            std::uint32_t connections = candidates[i]->get_active_connections();
            if (connections < min_connections) {
                min_connections = connections;
                best_provider = candidates[i];
            }
        }

        return best_provider;
    }

    DatabaseProvider*
    YValveDispatcher::select_random(const std::vector<DatabaseProvider*>& candidates)
    {
        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(0, static_cast<int>(candidates.size() - 1));

        return candidates[dist(gen)];
    }

    DatabaseProvider*
    YValveDispatcher::select_by_priority(const std::vector<DatabaseProvider*>& candidates)
    {
        // Providers are already sorted by priority, return first healthy one
        return candidates[0];
    }

    void YValveDispatcher::health_monitor_worker()
    {
        while (health_monitor_running_) {
            force_health_check();
            cleanup_inactive_connections();

            std::this_thread::sleep_for(
                std::chrono::milliseconds(failover_config_.health_check_interval_ms));
        }
    }

    bool YValveDispatcher::check_provider_health(DatabaseProvider* provider)
    {
        try {
            return provider->is_initialized();
        } catch (...) {
            return false;
        }
    }

    std::uint32_t YValveDispatcher::allocate_connection_id()
    {
        return next_connection_id_.fetch_add(1);
    }

    void YValveDispatcher::cleanup_inactive_connections()
    {
        std::lock_guard<std::mutex> lock(dispatcher_mutex_);

        auto now = std::chrono::steady_clock::now();
        auto timeout = std::chrono::milliseconds(connection_timeout_ms_);

        for (auto it = active_connections_.begin(); it != active_connections_.end();) {
            auto& record = it->second;

            if (!record->is_active || (now - record->created_at) > timeout) {
                // Connection is inactive or timed out
                record->db_ops->disconnect(record->connection_id);
                if (dispatcher_stats_.connections_active > 0) {
                    dispatcher_stats_.connections_active--;
                }
                it = active_connections_.erase(it);
            } else {
                ++it;
            }
        }
    }

} // namespace scratchbird::engine
