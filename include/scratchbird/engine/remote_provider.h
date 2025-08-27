#pragma once

#include "scratchbird/engine/database_provider.h"
#include "scratchbird/engine/firebird_protocol.h"
#include "scratchbird/engine/protocol_handler.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{
    /// Forward declarations
    class NetworkConnection;
    class RemoteProtocolHandler;

    /// Network connection configuration
    struct NetworkConfig {
        std::string hostname = "localhost";
        std::uint16_t port = 3050;
        std::uint32_t connect_timeout_ms = 30000;
        std::uint32_t read_timeout_ms = 60000;
        std::uint32_t write_timeout_ms = 30000;
        std::uint32_t keepalive_interval_ms = 60000;
        bool enable_compression = false;
        bool enable_encryption = false;
        std::uint32_t max_packet_size = 32768;
        std::uint32_t buffer_size = 8192;

        bool validate() const;
        std::string to_string() const;
    };

    /// Network connection state
    enum class ConnectionState {
        Disconnected,
        Connecting,
        Connected,
        Authenticated,
        Error,
        Closed
    };

    /// Remote connection statistics
    struct RemoteConnectionStats {
        std::chrono::steady_clock::time_point created_time;
        std::chrono::steady_clock::time_point last_activity;
        std::uint64_t bytes_sent = 0;
        std::uint64_t bytes_received = 0;
        std::uint64_t messages_sent = 0;
        std::uint64_t messages_received = 0;
        std::uint64_t errors_encountered = 0;
        std::uint32_t reconnection_attempts = 0;
        ConnectionState current_state = ConnectionState::Disconnected;
        std::string last_error;
    };

    /// Network connection implementation
    class NetworkConnection
    {
      public:
        NetworkConnection(const NetworkConfig& config);
        ~NetworkConnection();

        // Connection management
        bool connect();
        void disconnect();
        bool is_connected() const;
        ConnectionState get_state() const;

        // Data transfer
        bool send_data(const std::vector<std::uint8_t>& data);
        bool receive_data(std::vector<std::uint8_t>& data, std::uint32_t timeout_ms = 0);
        bool send_message(const ProtocolMessage& message);
        bool receive_message(ProtocolMessage& message, std::uint32_t timeout_ms = 0);

        // Connection properties
        const NetworkConfig& get_config() const
        {
            return config_;
        }
        RemoteConnectionStats get_statistics() const;
        std::string get_last_error() const
        {
            return last_error_;
        }

        // Advanced features
        void enable_keepalive(bool enabled);
        void set_compression(bool enabled);
        bool test_connectivity();

      private:
        NetworkConfig config_;
        mutable std::mutex connection_mutex_;
        std::atomic<ConnectionState> state_;
        int socket_fd_;
        std::string last_error_;
        RemoteConnectionStats stats_;

        // Internal connection methods
        bool create_socket();
        bool configure_socket();
        void cleanup_socket();
        bool handle_connection_error(const std::string& error);
        void update_statistics();
    };

    /// Remote protocol message handler
    class RemoteProtocolHandler
    {
      public:
        explicit RemoteProtocolHandler(NetworkConnection* connection);
        ~RemoteProtocolHandler();

        // Protocol operations
        bool negotiate_protocol_version();
        bool authenticate(const std::string& username, const std::string& password);
        bool attach_database(const std::string& database_path, std::uint32_t& database_handle);
        bool detach_database(std::uint32_t database_handle);

        // Transaction operations
        bool begin_transaction(std::uint32_t database_handle, std::uint32_t& transaction_handle);
        bool commit_transaction(std::uint32_t transaction_handle);
        bool rollback_transaction(std::uint32_t transaction_handle);

        // Statement operations
        bool prepare_statement(std::uint32_t database_handle, const std::string& sql,
                               std::uint32_t& statement_handle);
        bool execute_statement(std::uint32_t statement_handle,
                               const std::vector<std::string>& parameters);
        bool fetch_results(std::uint32_t statement_handle,
                           std::vector<std::vector<std::string>>& results);
        bool free_statement(std::uint32_t statement_handle);

        // Protocol state
        bool is_authenticated() const
        {
            return authenticated_;
        }
        std::string get_server_version() const
        {
            return server_version_;
        }
        FirebirdProtocolVersion get_negotiated_version() const
        {
            return negotiated_version_;
        }

        // Error handling
        std::string get_last_error() const
        {
            return last_error_;
        }
        std::int32_t get_last_error_code() const
        {
            return last_error_code_;
        }

      private:
        NetworkConnection* connection_;
        mutable std::mutex handler_mutex_;
        bool authenticated_;
        std::string server_version_;
        FirebirdProtocolVersion negotiated_version_;
        std::string last_error_;
        std::atomic<std::int32_t> last_error_code_;

        // Message handling helpers
        bool send_protocol_message(std::uint32_t operation,
                                   const std::vector<std::uint8_t>& data = {});
        bool receive_protocol_response(std::uint32_t& operation, std::vector<std::uint8_t>& data);
        bool handle_error_response(const std::vector<std::uint8_t>& error_data);
        void set_error(std::int32_t code, const std::string& message);
    };

    /// Connection pool for managing multiple remote connections
    class RemoteConnectionPool
    {
      public:
        explicit RemoteConnectionPool(const NetworkConfig& config,
                                      std::uint32_t max_connections = 10);
        ~RemoteConnectionPool();

        // Connection management
        std::uint32_t acquire_connection();
        void release_connection(std::uint32_t connection_id);
        bool is_connection_valid(std::uint32_t connection_id) const;

        // Pool operations
        NetworkConnection* get_connection(std::uint32_t connection_id);
        void cleanup_idle_connections();
        void close_all_connections();

        // Pool statistics
        struct PoolStats {
            std::uint32_t total_connections = 0;
            std::uint32_t active_connections = 0;
            std::uint32_t idle_connections = 0;
            std::uint32_t failed_connections = 0;
            std::uint64_t total_bytes_sent = 0;
            std::uint64_t total_bytes_received = 0;
        };

        PoolStats get_pool_statistics() const;

      private:
        NetworkConfig config_;
        std::uint32_t max_connections_;
        mutable std::mutex pool_mutex_;
        std::unordered_map<std::uint32_t, std::unique_ptr<NetworkConnection>> connections_;
        std::unordered_map<std::uint32_t, std::chrono::steady_clock::time_point> last_used_;
        std::atomic<std::uint32_t> next_connection_id_;

        std::uint32_t generate_connection_id();
        void remove_connection(std::uint32_t connection_id);
    };

    /// Enhanced remote provider implementation
    class EnhancedRemoteProvider : public DatabaseProvider
    {
      public:
        explicit EnhancedRemoteProvider(const ProviderConfig& config);
        ~EnhancedRemoteProvider() override;

        // Provider interface
        ProviderType get_provider_type() const override
        {
            return ProviderType::Remote;
        }
        std::string get_provider_name() const override
        {
            return "EnhancedRemoteProvider";
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

        // Remote-specific features
        RemoteConnectionPool* get_connection_pool() const
        {
            return connection_pool_.get();
        }
        NetworkConfig get_network_config() const
        {
            return network_config_;
        }
        void set_network_config(const NetworkConfig& config);

        // Connection management helpers
        std::uint32_t acquire_connection();
        void release_connection(std::uint32_t connection_id);
        NetworkConnection* get_connection(std::uint32_t connection_id);

      private:
        ProviderConfig config_;
        NetworkConfig network_config_;
        std::atomic<bool> initialized_;
        std::string last_error_;

        // Core components
        std::unique_ptr<RemoteConnectionPool> connection_pool_;

        // Statistics tracking
        mutable std::mutex stats_mutex_;
        ProviderStats statistics_;
        std::chrono::steady_clock::time_point start_time_;

        void update_statistics() const;
        void extract_network_config_from_provider_config();
    };

    /// Remote database operations implementation
    class RemoteDatabaseOperations : public DatabaseOperations
    {
      public:
        explicit RemoteDatabaseOperations(EnhancedRemoteProvider* provider);
        ~RemoteDatabaseOperations() override = default;

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
        EnhancedRemoteProvider* provider_;
        mutable std::string last_error_;
        std::atomic<std::int32_t> last_error_code_;

        // Connection handle mapping
        mutable std::mutex handle_mutex_;
        std::unordered_map<std::uint32_t, std::uint32_t> connection_map_; // logical -> physical
        std::unordered_map<std::uint32_t, std::unique_ptr<RemoteProtocolHandler>>
            protocol_handlers_;
        std::atomic<std::uint32_t> next_logical_handle_;

        std::uint32_t generate_logical_handle();
        RemoteProtocolHandler* get_protocol_handler(std::uint32_t logical_handle);
    };

    class RemoteTransactionOperations : public TransactionOperations
    {
      public:
        explicit RemoteTransactionOperations(EnhancedRemoteProvider* provider);
        ~RemoteTransactionOperations() override = default;

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
        EnhancedRemoteProvider* provider_;

        // Transaction handle tracking
        mutable std::mutex transaction_mutex_;
        std::unordered_map<std::uint32_t, std::uint32_t> transaction_map_; // logical -> remote
        std::unordered_map<std::uint32_t, std::uint32_t>
            connection_for_transaction_; // txn -> connection
        std::atomic<std::uint32_t> next_transaction_handle_;

        std::uint32_t generate_transaction_handle();
    };

    class RemoteStatementOperations : public StatementOperations
    {
      public:
        explicit RemoteStatementOperations(EnhancedRemoteProvider* provider);
        ~RemoteStatementOperations() override = default;

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
        EnhancedRemoteProvider* provider_;

        // Statement handle tracking
        mutable std::mutex statement_mutex_;
        std::unordered_map<std::uint32_t, std::uint32_t> statement_map_; // logical -> remote
        std::unordered_map<std::uint32_t, std::uint32_t>
            connection_for_statement_; // stmt -> connection
        std::atomic<std::uint32_t> next_statement_handle_;

        std::uint32_t generate_statement_handle();
    };

    class RemoteSecurityOperations : public SecurityOperations
    {
      public:
        explicit RemoteSecurityOperations(EnhancedRemoteProvider* provider);
        ~RemoteSecurityOperations() override = default;

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
        EnhancedRemoteProvider* provider_;

        // Authentication state
        mutable std::mutex auth_mutex_;
        std::unordered_map<std::uint32_t, std::string> authenticated_users_;
        std::unordered_map<std::uint32_t, std::string> user_roles_;
        std::unordered_map<std::uint32_t, std::uint32_t>
            user_connections_; // user_context -> connection
        std::atomic<std::uint32_t> next_user_context_;

        std::uint32_t generate_user_context();
    };

} // namespace scratchbird::engine
