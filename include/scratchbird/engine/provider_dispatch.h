#pragma once

#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/protocol_handler.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace scratchbird::engine
{
    /// Provider types supported by Y-Valve
    enum class ProviderType {
        Embedded,  // Local in-process database access
        Remote,    // Network-based database connections
        Legacy,    // Backward compatibility provider
        ThirdParty // Plugin-based provider
    };

    /// Provider capability flags
    struct ProviderCapabilities {
        bool supports_transactions = true;
        bool supports_statements = true;
        bool supports_authentication = true;
        bool supports_encryption = false;
        bool supports_compression = false;
        bool supports_streaming = false;
        bool supports_batch_operations = false;
        bool supports_async_operations = false;
        std::uint32_t max_connections = 0; // 0 = unlimited
        std::uint32_t max_databases = 0;   // 0 = unlimited

        bool is_compatible_with(const ProviderCapabilities& required) const;
    };

    /// Connection string components
    struct ConnectionInfo {
        std::string protocol;      // "embedded", "tcp", "inet", etc.
        std::string hostname;      // hostname for network connections
        std::uint16_t port = 0;    // port number (0 = default)
        std::string database_path; // database file or logical name
        std::string username;
        std::string password;
        std::string role;
        std::map<std::string, std::string> parameters; // Additional connection parameters

        static ConnectionInfo parse_connection_string(const std::string& connection_string);
        std::string to_connection_string() const;
        ProviderType get_provider_type() const;

        // Setter methods
        void set_provider_type(ProviderType type)
        {
            if (type == ProviderType::Embedded)
                protocol = "embedded";
            else if (type == ProviderType::Remote)
                protocol = "tcp";
            else if (type == ProviderType::Legacy)
                protocol = "legacy";
        }
        void set_database_path(const std::string& path)
        {
            database_path = path;
        }
        void set_username(const std::string& user)
        {
            username = user;
        }
        void set_password(const std::string& pass)
        {
            password = pass;
        }
    };

    /// Provider statistics for monitoring
    struct ProviderStats {
        mutable std::atomic<std::uint64_t> connections_created{0};
        mutable std::atomic<std::uint64_t> connections_active{0};
        mutable std::atomic<std::uint64_t> connections_failed{0};
        mutable std::atomic<std::uint64_t> requests_processed{0};
        mutable std::atomic<std::uint64_t> errors_encountered{0};
        mutable std::atomic<std::uint64_t> bytes_transferred{0};

        // Additional embedded provider statistics
        mutable std::string provider_name;
        mutable std::uint64_t uptime_seconds{0};
        mutable std::atomic<std::uint32_t> databases_attached{0};
        mutable std::atomic<std::uint32_t> transactions_active{0};
        mutable std::atomic<std::uint32_t> statements_prepared{0};
        mutable std::atomic<std::uint64_t> memory_usage_bytes{0};

        // Custom copy constructor and assignment operator for atomic members
        ProviderStats() = default;
        ProviderStats(const ProviderStats& other)
            : connections_created(other.connections_created.load()),
              connections_active(other.connections_active.load()),
              connections_failed(other.connections_failed.load()),
              requests_processed(other.requests_processed.load()),
              errors_encountered(other.errors_encountered.load()),
              bytes_transferred(other.bytes_transferred.load()), provider_name(other.provider_name),
              uptime_seconds(other.uptime_seconds),
              databases_attached(other.databases_attached.load()),
              transactions_active(other.transactions_active.load()),
              statements_prepared(other.statements_prepared.load()),
              memory_usage_bytes(other.memory_usage_bytes.load())
        {
        }

        ProviderStats& operator=(const ProviderStats& other)
        {
            if (this != &other) {
                connections_created = other.connections_created.load();
                connections_active = other.connections_active.load();
                connections_failed = other.connections_failed.load();
                requests_processed = other.requests_processed.load();
                errors_encountered = other.errors_encountered.load();
                bytes_transferred = other.bytes_transferred.load();
                provider_name = other.provider_name;
                uptime_seconds = other.uptime_seconds;
                databases_attached = other.databases_attached.load();
                transactions_active = other.transactions_active.load();
                statements_prepared = other.statements_prepared.load();
                memory_usage_bytes = other.memory_usage_bytes.load();
            }
            return *this;
        }

        void reset();
        std::map<std::string, std::uint64_t> get_metrics() const;
    };

    /// Forward declarations
    class DatabaseProvider;
    class YValveDispatcher;

    /// Database operation result
    enum class ProviderResult {
        Success,
        Error,
        NotSupported,
        ConnectionFailed,
        AuthenticationFailed,
        ResourceExhausted,
        Timeout,
        InvalidHandle,
        DatabaseError,
        TransactionError,
        StatementError
    };

    /// Generic database operations interface
    class DatabaseOperations
    {
      public:
        virtual ~DatabaseOperations() = default;

        // Connection management
        virtual ProviderResult connect(const ConnectionInfo& conn_info,
                                       std::uint32_t& connection_handle) = 0;
        virtual ProviderResult disconnect(std::uint32_t connection_handle) = 0;
        virtual bool is_connected(std::uint32_t connection_handle) const = 0;

        // Database operations
        virtual ProviderResult attach_database(std::uint32_t connection_handle,
                                               const std::string& database_path,
                                               std::uint32_t& database_handle) = 0;
        virtual ProviderResult detach_database(std::uint32_t database_handle) = 0;
        virtual ProviderResult create_database(const std::string& database_path,
                                               const ConnectionInfo& conn_info,
                                               std::uint32_t& database_handle) = 0;

        // Transaction management
        virtual ProviderResult start_transaction(std::uint32_t database_handle,
                                                 std::uint32_t& transaction_handle) = 0;
        virtual ProviderResult commit_transaction(std::uint32_t transaction_handle) = 0;
        virtual ProviderResult rollback_transaction(std::uint32_t transaction_handle) = 0;

        // Statement execution
        virtual ProviderResult prepare_statement(std::uint32_t database_handle,
                                                 const std::string& sql,
                                                 std::uint32_t& statement_handle) = 0;
        virtual ProviderResult execute_statement(std::uint32_t statement_handle,
                                                 const std::vector<std::string>& parameters) = 0;
        virtual ProviderResult fetch_results(std::uint32_t statement_handle,
                                             std::vector<std::vector<std::string>>& results) = 0;
        virtual ProviderResult free_statement(std::uint32_t statement_handle) = 0;

        // Error handling
        virtual std::string get_last_error() const = 0;
        virtual std::int32_t get_last_error_code() const = 0;
    };

    /// Transaction management interface
    class TransactionOperations
    {
      public:
        virtual ~TransactionOperations() = default;

        virtual ProviderResult begin_transaction(std::uint32_t database_handle,
                                                 std::uint32_t& transaction_handle) = 0;
        virtual ProviderResult prepare_transaction(std::uint32_t transaction_handle) = 0;
        virtual ProviderResult commit_transaction(std::uint32_t transaction_handle) = 0;
        virtual ProviderResult rollback_transaction(std::uint32_t transaction_handle) = 0;
        virtual ProviderResult rollback_to_savepoint(std::uint32_t transaction_handle,
                                                     const std::string& savepoint_name) = 0;
        virtual ProviderResult create_savepoint(std::uint32_t transaction_handle,
                                                const std::string& savepoint_name) = 0;
        virtual ProviderResult release_savepoint(std::uint32_t transaction_handle,
                                                 const std::string& savepoint_name) = 0;

        virtual bool is_transaction_active(std::uint32_t transaction_handle) const = 0;
        virtual std::string get_transaction_info(std::uint32_t transaction_handle) const = 0;
    };

    /// Statement execution interface
    class StatementOperations
    {
      public:
        virtual ~StatementOperations() = default;

        virtual ProviderResult prepare_statement(std::uint32_t database_handle,
                                                 const std::string& sql,
                                                 std::uint32_t& statement_handle) = 0;
        virtual ProviderResult execute_prepared(std::uint32_t statement_handle,
                                                const std::vector<std::string>& parameters) = 0;
        virtual ProviderResult execute_immediate(std::uint32_t database_handle,
                                                 const std::string& sql) = 0;
        virtual ProviderResult fetch_next(std::uint32_t statement_handle,
                                          std::vector<std::string>& row) = 0;
        virtual ProviderResult fetch_all(std::uint32_t statement_handle,
                                         std::vector<std::vector<std::string>>& results) = 0;
        virtual ProviderResult close_cursor(std::uint32_t statement_handle) = 0;
        virtual ProviderResult free_statement(std::uint32_t statement_handle) = 0;

        virtual bool has_more_results(std::uint32_t statement_handle) const = 0;
        virtual std::size_t get_affected_rows(std::uint32_t statement_handle) const = 0;
    };

    /// Security and authentication interface
    class SecurityOperations
    {
      public:
        virtual ~SecurityOperations() = default;

        virtual ProviderResult authenticate_user(const std::string& username,
                                                 const std::string& password,
                                                 std::uint32_t& user_context) = 0;
        virtual ProviderResult change_password(std::uint32_t user_context,
                                               const std::string& old_password,
                                               const std::string& new_password) = 0;
        virtual ProviderResult set_role(std::uint32_t user_context,
                                        const std::string& role_name) = 0;
        virtual ProviderResult get_user_roles(std::uint32_t user_context,
                                              std::vector<std::string>& roles) = 0;
        virtual ProviderResult check_permission(std::uint32_t user_context,
                                                const std::string& object_name,
                                                const std::string& operation) = 0;

        virtual bool is_authenticated(std::uint32_t user_context) const = 0;
        virtual std::string get_current_user(std::uint32_t user_context) const = 0;
        virtual std::string get_current_role(std::uint32_t user_context) const = 0;
    };

    /// Base database provider interface
    class DatabaseProvider
    {
      public:
        virtual ~DatabaseProvider() = default;

        /// Provider identification
        virtual ProviderType get_provider_type() const = 0;
        virtual std::string get_provider_name() const = 0;
        virtual std::string get_provider_version() const = 0;
        virtual ProviderCapabilities get_capabilities() const = 0;

        /// Provider lifecycle
        virtual bool initialize() = 0;
        virtual void shutdown() = 0;
        virtual bool is_initialized() const = 0;

        /// Connection management
        virtual bool can_handle_connection(const ConnectionInfo& conn_info) const = 0;
        virtual std::unique_ptr<DatabaseOperations> create_database_operations() = 0;
        virtual std::unique_ptr<TransactionOperations> create_transaction_operations() = 0;
        virtual std::unique_ptr<StatementOperations> create_statement_operations() = 0;
        virtual std::unique_ptr<SecurityOperations> create_security_operations() = 0;

        /// Resource management
        virtual void cleanup_resources() = 0;
        virtual std::uint32_t get_active_connections() const = 0;
        virtual ProviderStats get_statistics() const = 0;

        /// Error handling
        virtual std::string get_last_error() const = 0;
    };

    /// Provider factory function type
    using ProviderFactory = std::function<std::unique_ptr<DatabaseProvider>()>;

    /// Provider registration information
    struct ProviderRegistration {
        ProviderType type;
        std::string name;
        std::string version;
        ProviderCapabilities capabilities;
        ProviderFactory factory;
        std::uint32_t priority = 100; // Lower numbers = higher priority
        bool enabled = true;

        bool operator<(const ProviderRegistration& other) const
        {
            return priority < other.priority;
        }
    };

    /// Load balancing strategy for providers
    enum class LoadBalancingStrategy {
        RoundRobin,        // Rotate through providers
        LeastConnections,  // Use provider with fewest active connections
        Random,            // Random selection
        PriorityBased,     // Use highest priority available provider
        WeightedRoundRobin // Round robin with provider weights
    };

    /// Failover configuration
    struct FailoverConfig {
        bool enabled = false;
        std::uint32_t max_retries = 3;
        std::uint32_t retry_delay_ms = 1000;
        std::uint32_t health_check_interval_ms = 30000;
        bool auto_recovery = true;
    };

    /// Y-Valve dispatcher for routing database operations
    class YValveDispatcher
    {
      public:
        YValveDispatcher();
        ~YValveDispatcher();

        /// Provider management
        bool register_provider(const ProviderRegistration& registration);
        bool unregister_provider(const std::string& provider_name);
        std::vector<std::string> get_registered_providers() const;
        bool is_provider_registered(const std::string& provider_name) const;

        /// Provider lookup and selection
        DatabaseProvider* get_provider_for_connection(const ConnectionInfo& conn_info);
        std::vector<DatabaseProvider*> get_compatible_providers(const ConnectionInfo& conn_info);
        DatabaseProvider* get_provider_by_name(const std::string& provider_name);

        /// Routing configuration
        void set_load_balancing_strategy(LoadBalancingStrategy strategy);
        LoadBalancingStrategy get_load_balancing_strategy() const;
        void set_failover_config(const FailoverConfig& config);
        FailoverConfig get_failover_config() const;

        /// Connection routing
        ProviderResult route_connection(const ConnectionInfo& conn_info,
                                        std::uint32_t& connection_id);
        ProviderResult close_connection(std::uint32_t connection_id);
        DatabaseOperations* get_database_operations(std::uint32_t connection_id);
        TransactionOperations* get_transaction_operations(std::uint32_t connection_id);
        StatementOperations* get_statement_operations(std::uint32_t connection_id);
        SecurityOperations* get_security_operations(std::uint32_t connection_id);

        /// Health monitoring
        void enable_health_monitoring(bool enabled = true);
        bool is_health_monitoring_enabled() const;
        std::map<std::string, bool> get_provider_health_status() const;
        void force_health_check();

        /// Statistics and monitoring
        std::map<std::string, ProviderStats> get_all_provider_stats() const;
        ProviderStats get_dispatcher_stats() const;
        void reset_statistics();

        /// Configuration
        void set_max_providers_per_type(std::uint32_t max_count);
        void set_connection_timeout(std::uint32_t timeout_ms);
        void enable_provider_isolation(bool enabled = true);

      private:
        struct ConnectionRecord {
            std::uint32_t connection_id;
            ConnectionInfo conn_info;
            DatabaseProvider* provider;
            std::unique_ptr<DatabaseOperations> db_ops;
            std::unique_ptr<TransactionOperations> txn_ops;
            std::unique_ptr<StatementOperations> stmt_ops;
            std::unique_ptr<SecurityOperations> sec_ops;
            std::chrono::steady_clock::time_point created_at;
            bool is_active;
        };

        mutable std::mutex dispatcher_mutex_;
        std::vector<std::unique_ptr<DatabaseProvider>> providers_;
        std::map<std::string, std::size_t> provider_index_;
        std::map<std::uint32_t, std::unique_ptr<ConnectionRecord>> active_connections_;

        LoadBalancingStrategy load_balancing_strategy_;
        FailoverConfig failover_config_;
        std::atomic<std::uint32_t> next_connection_id_;
        std::atomic<std::uint32_t> round_robin_index_;

        // Health monitoring
        std::atomic<bool> health_monitoring_enabled_;
        std::map<std::string, std::atomic<bool>> provider_health_;
        std::thread health_monitor_thread_;
        std::atomic<bool> health_monitor_running_;

        // Configuration
        std::uint32_t max_providers_per_type_;
        std::uint32_t connection_timeout_ms_;
        bool provider_isolation_enabled_;

        // Statistics
        ProviderStats dispatcher_stats_;

        // Helper methods
        DatabaseProvider*
        select_provider_by_strategy(const std::vector<DatabaseProvider*>& candidates);
        DatabaseProvider* select_round_robin(const std::vector<DatabaseProvider*>& candidates);
        DatabaseProvider*
        select_least_connections(const std::vector<DatabaseProvider*>& candidates);
        DatabaseProvider* select_random(const std::vector<DatabaseProvider*>& candidates);
        DatabaseProvider* select_by_priority(const std::vector<DatabaseProvider*>& candidates);

        void health_monitor_worker();
        bool check_provider_health(DatabaseProvider* provider);
        std::uint32_t allocate_connection_id();
        void cleanup_inactive_connections();
    };

} // namespace scratchbird::engine
