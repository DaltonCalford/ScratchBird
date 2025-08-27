#include "scratchbird/engine/provider_dispatch.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

using namespace scratchbird::engine;

// Mock database provider for testing
class MockDatabaseProvider : public DatabaseProvider
{
  public:
    MockDatabaseProvider(const std::string& name, ProviderType type)
        : name_(name), type_(type), initialized_(false), active_connections_(0)
    {
        capabilities_.supports_transactions = true;
        capabilities_.supports_statements = true;
        capabilities_.supports_authentication = true;
        capabilities_.max_connections = 100;
    }

    ProviderType get_provider_type() const override
    {
        return type_;
    }
    std::string get_provider_name() const override
    {
        return name_;
    }
    std::string get_provider_version() const override
    {
        return "1.0.0";
    }
    ProviderCapabilities get_capabilities() const override
    {
        return capabilities_;
    }

    bool initialize() override
    {
        initialized_ = true;
        return true;
    }

    void shutdown() override
    {
        initialized_ = false;
        active_connections_ = 0;
    }

    bool is_initialized() const override
    {
        return initialized_;
    }

    bool can_handle_connection(const ConnectionInfo& conn_info) const override
    {
        ProviderType required_type = conn_info.get_provider_type();
        return required_type == type_;
    }

    std::unique_ptr<DatabaseOperations> create_database_operations() override
    {
        return std::make_unique<MockDatabaseOperations>();
    }

    std::unique_ptr<TransactionOperations> create_transaction_operations() override
    {
        return std::make_unique<MockTransactionOperations>();
    }

    std::unique_ptr<StatementOperations> create_statement_operations() override
    {
        return std::make_unique<MockStatementOperations>();
    }

    std::unique_ptr<SecurityOperations> create_security_operations() override
    {
        return std::make_unique<MockSecurityOperations>();
    }

    void cleanup_resources() override
    {
        // Mock cleanup
    }

    std::uint32_t get_active_connections() const override
    {
        return active_connections_.load();
    }

    ProviderStats get_statistics() const override
    {
        return stats_;
    }

    std::string get_last_error() const override
    {
        return last_error_;
    }

    void increment_connections()
    {
        active_connections_++;
    }
    void decrement_connections()
    {
        if (active_connections_ > 0)
            active_connections_--;
    }

  private:
    std::string name_;
    ProviderType type_;
    ProviderCapabilities capabilities_;
    bool initialized_;
    std::atomic<std::uint32_t> active_connections_;
    ProviderStats stats_;
    std::string last_error_;

    // Mock operation classes
    class MockDatabaseOperations : public DatabaseOperations
    {
      public:
        ProviderResult connect(const ConnectionInfo& conn_info,
                               std::uint32_t& connection_handle) override
        {
            connection_handle = next_handle_++;
            return ProviderResult::Success;
        }

        ProviderResult disconnect(std::uint32_t connection_handle) override
        {
            return ProviderResult::Success;
        }

        bool is_connected(std::uint32_t connection_handle) const override
        {
            return true;
        }

        ProviderResult attach_database(std::uint32_t connection_handle,
                                       const std::string& database_path,
                                       std::uint32_t& database_handle) override
        {
            database_handle = next_handle_++;
            return ProviderResult::Success;
        }

        ProviderResult detach_database(std::uint32_t database_handle) override
        {
            return ProviderResult::Success;
        }

        ProviderResult create_database(const std::string& database_path,
                                       const ConnectionInfo& conn_info,
                                       std::uint32_t& database_handle) override
        {
            database_handle = next_handle_++;
            return ProviderResult::Success;
        }

        ProviderResult start_transaction(std::uint32_t database_handle,
                                         std::uint32_t& transaction_handle) override
        {
            transaction_handle = next_handle_++;
            return ProviderResult::Success;
        }

        ProviderResult commit_transaction(std::uint32_t transaction_handle) override
        {
            return ProviderResult::Success;
        }

        ProviderResult rollback_transaction(std::uint32_t transaction_handle) override
        {
            return ProviderResult::Success;
        }

        ProviderResult prepare_statement(std::uint32_t database_handle, const std::string& sql,
                                         std::uint32_t& statement_handle) override
        {
            statement_handle = next_handle_++;
            return ProviderResult::Success;
        }

        ProviderResult execute_statement(std::uint32_t statement_handle,
                                         const std::vector<std::string>& parameters) override
        {
            return ProviderResult::Success;
        }

        ProviderResult fetch_results(std::uint32_t statement_handle,
                                     std::vector<std::vector<std::string>>& results) override
        {
            return ProviderResult::Success;
        }

        ProviderResult free_statement(std::uint32_t statement_handle) override
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
        static std::uint32_t next_handle_;
    };

    class MockTransactionOperations : public TransactionOperations
    {
      public:
        ProviderResult begin_transaction(std::uint32_t database_handle,
                                         std::uint32_t& transaction_handle) override
        {
            transaction_handle = 100 + database_handle;
            return ProviderResult::Success;
        }

        ProviderResult prepare_transaction(std::uint32_t transaction_handle) override
        {
            return ProviderResult::Success;
        }
        ProviderResult commit_transaction(std::uint32_t transaction_handle) override
        {
            return ProviderResult::Success;
        }
        ProviderResult rollback_transaction(std::uint32_t transaction_handle) override
        {
            return ProviderResult::Success;
        }
        ProviderResult rollback_to_savepoint(std::uint32_t transaction_handle,
                                             const std::string& savepoint_name) override
        {
            return ProviderResult::Success;
        }
        ProviderResult create_savepoint(std::uint32_t transaction_handle,
                                        const std::string& savepoint_name) override
        {
            return ProviderResult::Success;
        }
        ProviderResult release_savepoint(std::uint32_t transaction_handle,
                                         const std::string& savepoint_name) override
        {
            return ProviderResult::Success;
        }

        bool is_transaction_active(std::uint32_t transaction_handle) const override
        {
            return true;
        }
        std::string get_transaction_info(std::uint32_t transaction_handle) const override
        {
            return "active";
        }
    };

    class MockStatementOperations : public StatementOperations
    {
      public:
        ProviderResult prepare_statement(std::uint32_t database_handle, const std::string& sql,
                                         std::uint32_t& statement_handle) override
        {
            statement_handle = 200 + database_handle;
            return ProviderResult::Success;
        }

        ProviderResult execute_prepared(std::uint32_t statement_handle,
                                        const std::vector<std::string>& parameters) override
        {
            return ProviderResult::Success;
        }
        ProviderResult execute_immediate(std::uint32_t database_handle,
                                         const std::string& sql) override
        {
            return ProviderResult::Success;
        }
        ProviderResult fetch_next(std::uint32_t statement_handle,
                                  std::vector<std::string>& row) override
        {
            return ProviderResult::Success;
        }
        ProviderResult fetch_all(std::uint32_t statement_handle,
                                 std::vector<std::vector<std::string>>& results) override
        {
            return ProviderResult::Success;
        }
        ProviderResult close_cursor(std::uint32_t statement_handle) override
        {
            return ProviderResult::Success;
        }
        ProviderResult free_statement(std::uint32_t statement_handle) override
        {
            return ProviderResult::Success;
        }

        bool has_more_results(std::uint32_t statement_handle) const override
        {
            return false;
        }
        std::size_t get_affected_rows(std::uint32_t statement_handle) const override
        {
            return 1;
        }
    };

    class MockSecurityOperations : public SecurityOperations
    {
      public:
        ProviderResult authenticate_user(const std::string& username, const std::string& password,
                                         std::uint32_t& user_context) override
        {
            user_context = 300;
            return ProviderResult::Success;
        }

        ProviderResult change_password(std::uint32_t user_context, const std::string& old_password,
                                       const std::string& new_password) override
        {
            return ProviderResult::Success;
        }
        ProviderResult set_role(std::uint32_t user_context, const std::string& role_name) override
        {
            return ProviderResult::Success;
        }
        ProviderResult get_user_roles(std::uint32_t user_context,
                                      std::vector<std::string>& roles) override
        {
            return ProviderResult::Success;
        }
        ProviderResult check_permission(std::uint32_t user_context, const std::string& object_name,
                                        const std::string& operation) override
        {
            return ProviderResult::Success;
        }

        bool is_authenticated(std::uint32_t user_context) const override
        {
            return true;
        }
        std::string get_current_user(std::uint32_t user_context) const override
        {
            return "test_user";
        }
        std::string get_current_role(std::uint32_t user_context) const override
        {
            return "test_role";
        }
    };
};

std::uint32_t MockDatabaseProvider::MockDatabaseOperations::next_handle_ = 1;

// Test functions
void test_provider_capabilities()
{
    std::cout << "Testing ProviderCapabilities..." << std::endl;

    ProviderCapabilities full_caps;
    full_caps.supports_transactions = true;
    full_caps.supports_statements = true;
    full_caps.supports_authentication = true;
    full_caps.supports_encryption = true;

    ProviderCapabilities basic_caps;
    basic_caps.supports_transactions = true;
    basic_caps.supports_statements = true;
    basic_caps.supports_authentication = true;
    basic_caps.supports_encryption = false;

    ProviderCapabilities required_caps;
    required_caps.supports_transactions = true;
    required_caps.supports_statements = true;

    assert(full_caps.is_compatible_with(required_caps));
    assert(basic_caps.is_compatible_with(required_caps));

    required_caps.supports_encryption = true;
    assert(full_caps.is_compatible_with(required_caps));
    assert(!basic_caps.is_compatible_with(required_caps));

    std::cout << "✓ ProviderCapabilities tests passed" << std::endl;
}

void test_connection_info_parsing()
{
    std::cout << "Testing ConnectionInfo parsing..." << std::endl;

    // Test embedded connection
    auto conn1 = ConnectionInfo::parse_connection_string("/path/to/database.fdb");
    assert(conn1.protocol == "embedded");
    assert(conn1.database_path == "/path/to/database.fdb");
    assert(conn1.hostname.empty());
    assert(conn1.get_provider_type() == ProviderType::Embedded);

    // Test TCP connection
    auto conn2 = ConnectionInfo::parse_connection_string("tcp://server:3050/database.fdb");
    assert(conn2.protocol == "tcp");
    assert(conn2.hostname == "server");
    assert(conn2.port == 3050);
    assert(conn2.database_path == "database.fdb");
    assert(conn2.get_provider_type() == ProviderType::Remote);

    // Test with credentials
    auto conn3 = ConnectionInfo::parse_connection_string("tcp://user:pass@server/database.fdb");
    assert(conn3.protocol == "tcp");
    assert(conn3.username == "user");
    assert(conn3.password == "pass");
    assert(conn3.hostname == "server");
    assert(conn3.database_path == "database.fdb");
    assert(conn3.port == 3050); // Default port

    // Test connection string generation
    std::string conn_str = conn3.to_connection_string();
    assert(conn_str.find("tcp://") != std::string::npos);
    assert(conn_str.find("user:pass@") != std::string::npos);
    assert(conn_str.find("server") != std::string::npos);
    assert(conn_str.find("database.fdb") != std::string::npos);

    std::cout << "✓ ConnectionInfo parsing tests passed" << std::endl;
}

void test_provider_stats()
{
    std::cout << "Testing ProviderStats..." << std::endl;

    ProviderStats stats;

    stats.connections_created = 10;
    stats.connections_active = 5;
    stats.requests_processed = 100;

    auto metrics = stats.get_metrics();
    assert(metrics["connections_created"] == 10);
    assert(metrics["connections_active"] == 5);
    assert(metrics["requests_processed"] == 100);

    stats.reset();
    assert(stats.connections_created == 0);
    assert(stats.connections_active == 0);
    assert(stats.requests_processed == 0);

    std::cout << "✓ ProviderStats tests passed" << std::endl;
}

void test_provider_registration()
{
    std::cout << "Testing provider registration..." << std::endl;

    YValveDispatcher dispatcher;

    // Create provider registration
    ProviderRegistration reg;
    reg.type = ProviderType::Embedded;
    reg.name = "MockEmbedded";
    reg.version = "1.0.0";
    reg.capabilities.supports_transactions = true;
    reg.factory = []() {
        return std::make_unique<MockDatabaseProvider>("MockEmbedded", ProviderType::Embedded);
    };
    reg.priority = 100;
    reg.enabled = true;

    // Register provider
    assert(dispatcher.register_provider(reg));
    assert(dispatcher.is_provider_registered("MockEmbedded"));

    auto providers = dispatcher.get_registered_providers();
    assert(providers.size() == 1);
    assert(providers[0] == "MockEmbedded");

    // Test duplicate registration
    assert(!dispatcher.register_provider(reg)); // Should fail

    // Register another provider
    ProviderRegistration reg2;
    reg2.type = ProviderType::Remote;
    reg2.name = "MockRemote";
    reg2.version = "1.0.0";
    reg2.capabilities.supports_transactions = true;
    reg2.factory = []() {
        return std::make_unique<MockDatabaseProvider>("MockRemote", ProviderType::Remote);
    };

    assert(dispatcher.register_provider(reg2));
    assert(dispatcher.is_provider_registered("MockRemote"));

    providers = dispatcher.get_registered_providers();
    assert(providers.size() == 2);

    // Test unregistration
    assert(dispatcher.unregister_provider("MockEmbedded"));
    assert(!dispatcher.is_provider_registered("MockEmbedded"));

    providers = dispatcher.get_registered_providers();
    assert(providers.size() == 1);
    assert(providers[0] == "MockRemote");

    std::cout << "✓ Provider registration tests passed" << std::endl;
}

void test_provider_selection()
{
    std::cout << "Testing provider selection..." << std::endl;

    YValveDispatcher dispatcher;

    // Register embedded provider
    ProviderRegistration embedded_reg;
    embedded_reg.type = ProviderType::Embedded;
    embedded_reg.name = "MockEmbedded";
    embedded_reg.factory = []() {
        return std::make_unique<MockDatabaseProvider>("MockEmbedded", ProviderType::Embedded);
    };
    embedded_reg.priority = 10;

    // Register remote provider
    ProviderRegistration remote_reg;
    remote_reg.type = ProviderType::Remote;
    remote_reg.name = "MockRemote";
    remote_reg.factory = []() {
        return std::make_unique<MockDatabaseProvider>("MockRemote", ProviderType::Remote);
    };
    remote_reg.priority = 20;

    assert(dispatcher.register_provider(embedded_reg));
    assert(dispatcher.register_provider(remote_reg));

    // Test embedded connection selection
    ConnectionInfo embedded_conn = ConnectionInfo::parse_connection_string("/path/to/db.fdb");
    auto provider = dispatcher.get_provider_for_connection(embedded_conn);
    assert(provider != nullptr);
    assert(provider->get_provider_type() == ProviderType::Embedded);

    // Test remote connection selection
    ConnectionInfo remote_conn =
        ConnectionInfo::parse_connection_string("tcp://server:3050/db.fdb");
    provider = dispatcher.get_provider_for_connection(remote_conn);
    assert(provider != nullptr);
    assert(provider->get_provider_type() == ProviderType::Remote);

    // Test provider lookup by name
    provider = dispatcher.get_provider_by_name("MockEmbedded");
    assert(provider != nullptr);
    assert(provider->get_provider_name() == "MockEmbedded");

    provider = dispatcher.get_provider_by_name("NonExistent");
    assert(provider == nullptr);

    std::cout << "✓ Provider selection tests passed" << std::endl;
}

void test_load_balancing_strategies()
{
    std::cout << "Testing load balancing strategies..." << std::endl;

    YValveDispatcher dispatcher;

    // Register multiple providers of same type
    for (int i = 1; i <= 3; ++i) {
        ProviderRegistration reg;
        reg.type = ProviderType::Remote;
        reg.name = "MockRemote" + std::to_string(i);
        reg.factory = [i]() {
            return std::make_unique<MockDatabaseProvider>("MockRemote" + std::to_string(i),
                                                          ProviderType::Remote);
        };
        reg.priority = i * 10;
        assert(dispatcher.register_provider(reg));
    }

    ConnectionInfo remote_conn =
        ConnectionInfo::parse_connection_string("tcp://server:3050/db.fdb");

    // Test priority-based selection (default)
    assert(dispatcher.get_load_balancing_strategy() == LoadBalancingStrategy::PriorityBased);
    auto provider = dispatcher.get_provider_for_connection(remote_conn);
    assert(provider != nullptr);

    // Test round-robin selection
    dispatcher.set_load_balancing_strategy(LoadBalancingStrategy::RoundRobin);
    assert(dispatcher.get_load_balancing_strategy() == LoadBalancingStrategy::RoundRobin);

    provider = dispatcher.get_provider_for_connection(remote_conn);
    assert(provider != nullptr);

    // Test random selection
    dispatcher.set_load_balancing_strategy(LoadBalancingStrategy::Random);
    provider = dispatcher.get_provider_for_connection(remote_conn);
    assert(provider != nullptr);

    // Test least connections selection
    dispatcher.set_load_balancing_strategy(LoadBalancingStrategy::LeastConnections);
    provider = dispatcher.get_provider_for_connection(remote_conn);
    assert(provider != nullptr);

    std::cout << "✓ Load balancing strategy tests passed" << std::endl;
}

void test_connection_routing()
{
    std::cout << "Testing connection routing..." << std::endl;

    YValveDispatcher dispatcher;

    // Register provider
    ProviderRegistration reg;
    reg.type = ProviderType::Embedded;
    reg.name = "MockEmbedded";
    reg.factory = []() {
        return std::make_unique<MockDatabaseProvider>("MockEmbedded", ProviderType::Embedded);
    };

    assert(dispatcher.register_provider(reg));

    // Test connection routing
    ConnectionInfo conn_info = ConnectionInfo::parse_connection_string("/path/to/db.fdb");
    conn_info.username = "test_user";
    conn_info.password = "test_pass";

    std::uint32_t connection_id;
    ProviderResult result = dispatcher.route_connection(conn_info, connection_id);
    assert(result == ProviderResult::Success);
    assert(connection_id > 0);

    // Test operation interfaces
    auto db_ops = dispatcher.get_database_operations(connection_id);
    assert(db_ops != nullptr);

    auto txn_ops = dispatcher.get_transaction_operations(connection_id);
    assert(txn_ops != nullptr);

    auto stmt_ops = dispatcher.get_statement_operations(connection_id);
    assert(stmt_ops != nullptr);

    auto sec_ops = dispatcher.get_security_operations(connection_id);
    assert(sec_ops != nullptr);

    // Test connection close
    result = dispatcher.close_connection(connection_id);
    assert(result == ProviderResult::Success);

    // Test operations after close
    db_ops = dispatcher.get_database_operations(connection_id);
    assert(db_ops == nullptr);

    std::cout << "✓ Connection routing tests passed" << std::endl;
}

void test_failover_config()
{
    std::cout << "Testing failover configuration..." << std::endl;

    YValveDispatcher dispatcher;

    // Test default failover config
    auto config = dispatcher.get_failover_config();
    assert(!config.enabled);
    assert(config.max_retries == 3);
    assert(config.retry_delay_ms == 1000);

    // Test custom failover config
    FailoverConfig new_config;
    new_config.enabled = true;
    new_config.max_retries = 5;
    new_config.retry_delay_ms = 2000;
    new_config.health_check_interval_ms = 60000;
    new_config.auto_recovery = false;

    dispatcher.set_failover_config(new_config);
    config = dispatcher.get_failover_config();

    assert(config.enabled);
    assert(config.max_retries == 5);
    assert(config.retry_delay_ms == 2000);
    assert(config.health_check_interval_ms == 60000);
    assert(!config.auto_recovery);

    std::cout << "✓ Failover configuration tests passed" << std::endl;
}

void test_health_monitoring()
{
    std::cout << "Testing health monitoring..." << std::endl;

    YValveDispatcher dispatcher;

    // Register provider
    ProviderRegistration reg;
    reg.type = ProviderType::Embedded;
    reg.name = "MockEmbedded";
    reg.factory = []() {
        return std::make_unique<MockDatabaseProvider>("MockEmbedded", ProviderType::Embedded);
    };

    assert(dispatcher.register_provider(reg));

    // Test health monitoring disabled by default
    assert(!dispatcher.is_health_monitoring_enabled());

    // Test health status
    auto health_status = dispatcher.get_provider_health_status();
    assert(health_status.size() == 1);
    assert(health_status["MockEmbedded"] == true);

    // Test force health check
    dispatcher.force_health_check();
    health_status = dispatcher.get_provider_health_status();
    assert(health_status["MockEmbedded"] == true);

    // Enable health monitoring
    dispatcher.enable_health_monitoring(true);
    assert(dispatcher.is_health_monitoring_enabled());

    // Wait a bit for health monitor to run
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Disable health monitoring
    dispatcher.enable_health_monitoring(false);
    assert(!dispatcher.is_health_monitoring_enabled());

    std::cout << "✓ Health monitoring tests passed" << std::endl;
}

void test_statistics_collection()
{
    std::cout << "Testing statistics collection..." << std::endl;

    YValveDispatcher dispatcher;

    // Register provider
    ProviderRegistration reg;
    reg.type = ProviderType::Embedded;
    reg.name = "MockEmbedded";
    reg.factory = []() {
        return std::make_unique<MockDatabaseProvider>("MockEmbedded", ProviderType::Embedded);
    };

    assert(dispatcher.register_provider(reg));

    // Test initial dispatcher stats
    auto dispatcher_stats = dispatcher.get_dispatcher_stats();
    assert(dispatcher_stats.connections_created == 0);
    assert(dispatcher_stats.connections_active == 0);

    // Create a connection to generate stats
    ConnectionInfo conn_info = ConnectionInfo::parse_connection_string("/path/to/db.fdb");
    std::uint32_t connection_id;
    ProviderResult result = dispatcher.route_connection(conn_info, connection_id);
    assert(result == ProviderResult::Success);

    // Check updated stats
    dispatcher_stats = dispatcher.get_dispatcher_stats();
    assert(dispatcher_stats.connections_created == 1);
    assert(dispatcher_stats.connections_active == 1);

    // Test all provider stats
    auto all_stats = dispatcher.get_all_provider_stats();
    assert(all_stats.size() == 1);
    assert(all_stats.find("MockEmbedded") != all_stats.end());

    // Clean up
    dispatcher.close_connection(connection_id);

    std::cout << "✓ Statistics collection tests passed" << std::endl;
}

void test_configuration_settings()
{
    std::cout << "Testing configuration settings..." << std::endl;

    YValveDispatcher dispatcher;

    // Test configuration methods
    dispatcher.set_max_providers_per_type(5);
    dispatcher.set_connection_timeout(60000);
    dispatcher.enable_provider_isolation(false);

    // Configuration settings don't have getters in the public interface,
    // but we can verify they don't crash

    std::cout << "✓ Configuration settings tests passed" << std::endl;
}

int main()
{
    try {
        std::cout << "Running Y-Valve Provider Dispatch Tests..." << std::endl;
        std::cout << "============================================" << std::endl;

        test_provider_capabilities();
        test_connection_info_parsing();
        test_provider_stats();
        test_provider_registration();
        test_provider_selection();
        test_load_balancing_strategies();
        test_connection_routing();
        test_failover_config();
        test_health_monitoring();
        test_statistics_collection();
        test_configuration_settings();

        std::cout << std::endl;
        std::cout << "============================================" << std::endl;
        std::cout << "✅ All Y-Valve Provider Dispatch tests passed!" << std::endl;
        std::cout << "   - Provider capabilities validation" << std::endl;
        std::cout << "   - Connection string parsing and routing" << std::endl;
        std::cout << "   - Provider registration and lifecycle" << std::endl;
        std::cout << "   - Load balancing strategies" << std::endl;
        std::cout << "   - Connection routing and management" << std::endl;
        std::cout << "   - Failover configuration" << std::endl;
        std::cout << "   - Health monitoring system" << std::endl;
        std::cout << "   - Statistics collection" << std::endl;
        std::cout << "   - Configuration management" << std::endl;

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}
