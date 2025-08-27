#include "scratchbird/engine/embedded_provider.h"
#include "scratchbird/engine/provider_dispatch.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

using namespace scratchbird::engine;

namespace
{
    // Test configuration
    ProviderConfig create_test_config()
    {
        ProviderConfig config;
        config.provider_name = "TestEmbeddedProvider";
        config.provider_version = "1.0.0";
        config.provider_type = ProviderType::Embedded;
        config.connection_pool_size = 10;
        config.max_concurrent_transactions = 100;
        config.statement_cache_size = 50;
        config.timeout_ms = 5000;
        config.max_memory_mb = 64;
        config.max_open_databases = 5;
        config.max_prepared_statements = 100;
        return config;
    }

    ConnectionInfo create_test_connection_info()
    {
        ConnectionInfo conn_info;
        conn_info.set_provider_type(ProviderType::Embedded);
        conn_info.set_database_path("/tmp/test_embedded.db");
        conn_info.set_username("test_user");
        conn_info.set_password("test_password");
        return conn_info;
    }
} // namespace

// Test 1: EmbeddedDatabaseManager basic functionality
void test_embedded_database_manager()
{
    std::cout << "Test 1: EmbeddedDatabaseManager basic functionality..." << std::endl;

    EmbeddedDatabaseManager db_manager;

    // Test database creation
    auto conn_info = create_test_connection_info();
    std::uint32_t db_handle = db_manager.create_database("/tmp/test1.db", conn_info);
    assert(db_handle != 0);
    assert(db_manager.is_database_attached(db_handle));

    // Test database attachment
    std::uint32_t db_handle2 = db_manager.attach_database("/tmp/test2.db");
    assert(db_handle2 != 0);
    assert(db_manager.is_database_attached(db_handle2));

    // Test active databases list
    auto active_dbs = db_manager.get_active_databases();
    assert(active_dbs.size() == 2);

    // Test database statistics
    auto stats = db_manager.get_all_database_stats();
    assert(stats.size() == 2);

    // Test database detachment
    assert(db_manager.detach_database(db_handle));
    assert(!db_manager.is_database_attached(db_handle));
    assert(db_manager.get_database_count() == 1);

    assert(db_manager.detach_database(db_handle2));
    assert(db_manager.get_database_count() == 0);

    std::cout << "✅ EmbeddedDatabaseManager tests passed" << std::endl;
}

// Test 2: EmbeddedConnectionManager functionality
void test_embedded_connection_manager()
{
    std::cout << "Test 2: EmbeddedConnectionManager functionality..." << std::endl;

    EmbeddedDatabaseManager db_manager;
    EmbeddedConnectionManager conn_manager(&db_manager);

    // Create a database first
    auto conn_info = create_test_connection_info();
    std::uint32_t db_handle = db_manager.create_database("/tmp/test_conn.db", conn_info);
    assert(db_handle != 0);

    // Test connection creation
    std::uint32_t conn_handle = conn_manager.create_connection(db_handle);
    assert(conn_handle != 0);
    assert(conn_manager.is_connection_active(conn_handle));

    // Test connection info
    auto conn_info_result = conn_manager.get_connection_info(conn_handle);
    assert(conn_info_result.handle == conn_handle);
    assert(conn_info_result.database_handle == db_handle);

    // Test all connections
    auto all_connections = conn_manager.get_all_connections();
    assert(all_connections.size() == 1);

    // Test shared memory allocation
    void* shared_mem = conn_manager.allocate_shared_memory(1024);
    assert(shared_mem != nullptr);
    assert(conn_manager.get_shared_memory_usage() >= 1024);

    conn_manager.deallocate_shared_memory(shared_mem, 1024);
    assert(conn_manager.get_shared_memory_usage() == 0);

    // Test exclusive locking
    assert(conn_manager.acquire_exclusive_lock(conn_handle));
    assert(conn_manager.is_exclusively_locked());

    // Second connection should fail to acquire lock
    std::uint32_t conn_handle2 = conn_manager.create_connection(db_handle);
    assert(!conn_manager.acquire_exclusive_lock(conn_handle2));

    conn_manager.release_exclusive_lock(conn_handle);
    assert(!conn_manager.is_exclusively_locked());

    // Test connection cleanup
    assert(conn_manager.close_connection(conn_handle));
    assert(!conn_manager.is_connection_active(conn_handle));

    assert(conn_manager.close_connection(conn_handle2));
    assert(db_manager.detach_database(db_handle));

    std::cout << "✅ EmbeddedConnectionManager tests passed" << std::endl;
}

// Test 3: EmbeddedTransactionManager functionality
void test_embedded_transaction_manager()
{
    std::cout << "Test 3: EmbeddedTransactionManager functionality..." << std::endl;

    EmbeddedDatabaseManager db_manager;
    EmbeddedTransactionManager txn_manager(&db_manager);

    // Test transaction creation
    std::uint32_t txn_handle = txn_manager.begin_transaction(1000);
    assert(txn_handle != 0);
    assert(txn_manager.is_transaction_active(txn_handle));

    // Test transaction info
    auto txn_info = txn_manager.get_transaction_info(txn_handle);
    assert(txn_info.handle == txn_handle);
    assert(txn_info.connection_handle == 1000);

    // Test active transactions
    auto active_txns = txn_manager.get_active_transactions();
    assert(active_txns.size() == 1);

    // Test transaction commit
    assert(txn_manager.commit_transaction(txn_handle));
    assert(!txn_manager.is_transaction_active(txn_handle));

    // Test transaction rollback
    std::uint32_t txn_handle2 = txn_manager.begin_transaction(1000);
    assert(txn_handle2 != 0);
    assert(txn_manager.rollback_transaction(txn_handle2));
    assert(!txn_manager.is_transaction_active(txn_handle2));

    std::cout << "✅ EmbeddedTransactionManager tests passed" << std::endl;
}

// Test 4: EmbeddedStatementManager functionality
void test_embedded_statement_manager()
{
    std::cout << "Test 4: EmbeddedStatementManager functionality..." << std::endl;

    EmbeddedDatabaseManager db_manager;
    EmbeddedStatementManager stmt_manager(&db_manager);

    // Test statement preparation
    std::string sql = "SELECT * FROM test_table";
    std::uint32_t stmt_handle = stmt_manager.prepare_statement(1000, sql);
    assert(stmt_handle != 0);

    // Test statement info
    auto stmt_info = stmt_manager.get_statement_info(stmt_handle);
    assert(stmt_info.handle == stmt_handle);
    assert(stmt_info.connection_handle == 1000);
    assert(stmt_info.sql == sql);
    assert(stmt_info.execution_count == 0);

    // Test prepared statements list
    auto prepared_stmts = stmt_manager.get_prepared_statements();
    assert(prepared_stmts.size() == 1);

    // Test statement execution
    std::vector<std::string> parameters = {"param1", "param2"};
    assert(stmt_manager.execute_statement(stmt_handle, parameters));

    // Verify execution count updated
    stmt_info = stmt_manager.get_statement_info(stmt_handle);
    assert(stmt_info.execution_count == 1);

    // Test result fetching
    std::vector<std::vector<std::string>> results;
    assert(stmt_manager.fetch_results(stmt_handle, results));

    // Test statement cleanup
    assert(stmt_manager.free_statement(stmt_handle));
    assert(stmt_manager.get_prepared_statements().empty());

    std::cout << "✅ EmbeddedStatementManager tests passed" << std::endl;
}

// Test 5: EnhancedEmbeddedProvider initialization and basic functionality
void test_enhanced_embedded_provider_basic()
{
    std::cout << "Test 5: EnhancedEmbeddedProvider basic functionality..." << std::endl;

    auto config = create_test_config();
    EnhancedEmbeddedProvider provider(config);

    // Test provider info
    assert(provider.get_provider_type() == ProviderType::Embedded);
    assert(provider.get_provider_name() == "EnhancedEmbeddedProvider");
    assert(provider.get_provider_version() == "1.0.0");

    // Test capabilities
    auto caps = provider.get_capabilities();
    assert(caps.supports_transactions);
    assert(caps.supports_statements);
    assert(caps.supports_authentication);
    assert(caps.supports_streaming);
    assert(!caps.supports_encryption);

    // Test initialization
    assert(!provider.is_initialized());
    assert(provider.initialize());
    assert(provider.is_initialized());

    // Test managers are created
    assert(provider.get_database_manager() != nullptr);
    assert(provider.get_connection_manager() != nullptr);
    assert(provider.get_transaction_manager() != nullptr);
    assert(provider.get_statement_manager() != nullptr);

    // Test connection handling
    auto conn_info = create_test_connection_info();
    assert(provider.can_handle_connection(conn_info));

    // Test operation factories
    auto db_ops = provider.create_database_operations();
    assert(db_ops != nullptr);

    auto txn_ops = provider.create_transaction_operations();
    assert(txn_ops != nullptr);

    auto stmt_ops = provider.create_statement_operations();
    assert(stmt_ops != nullptr);

    auto sec_ops = provider.create_security_operations();
    assert(sec_ops != nullptr);

    // Test statistics
    auto stats = provider.get_statistics();
    assert(stats.provider_name == "EnhancedEmbeddedProvider");

    provider.shutdown();
    assert(!provider.is_initialized());

    std::cout << "✅ EnhancedEmbeddedProvider basic tests passed" << std::endl;
}

// Test 6: EmbeddedDatabaseOperations full workflow
void test_embedded_database_operations()
{
    std::cout << "Test 6: EmbeddedDatabaseOperations workflow..." << std::endl;

    auto config = create_test_config();
    EnhancedEmbeddedProvider provider(config);
    assert(provider.initialize());

    auto db_ops = provider.create_database_operations();
    auto conn_info = create_test_connection_info();

    // Test database connection
    std::uint32_t conn_handle;
    auto result = db_ops->connect(conn_info, conn_handle);
    assert(result == ProviderResult::Success);
    assert(conn_handle != 0);
    assert(db_ops->is_connected(conn_handle));

    // Test database creation
    std::uint32_t db_handle;
    result = db_ops->create_database("/tmp/test_create.db", conn_info, db_handle);
    assert(result == ProviderResult::Success);
    assert(db_handle != 0);

    // Test database attachment/detachment
    std::uint32_t db_handle2;
    result = db_ops->attach_database(conn_handle, "/tmp/test_attach.db", db_handle2);
    assert(result == ProviderResult::Success);

    result = db_ops->detach_database(db_handle2);
    assert(result == ProviderResult::Success);

    // Test transaction operations
    std::uint32_t txn_handle;
    result = db_ops->start_transaction(conn_handle, txn_handle);
    assert(result == ProviderResult::Success);

    result = db_ops->commit_transaction(txn_handle);
    assert(result == ProviderResult::Success);

    // Test statement operations
    std::string sql = "INSERT INTO test VALUES (?, ?)";
    std::uint32_t stmt_handle;
    result = db_ops->prepare_statement(conn_handle, sql, stmt_handle);
    assert(result == ProviderResult::Success);

    std::vector<std::string> params = {"value1", "value2"};
    result = db_ops->execute_statement(stmt_handle, params);
    assert(result == ProviderResult::Success);

    std::vector<std::vector<std::string>> results;
    result = db_ops->fetch_results(stmt_handle, results);
    assert(result == ProviderResult::Success);

    result = db_ops->free_statement(stmt_handle);
    assert(result == ProviderResult::Success);

    // Test disconnection
    result = db_ops->disconnect(conn_handle);
    assert(result == ProviderResult::Success);
    assert(!db_ops->is_connected(conn_handle));

    provider.shutdown();

    std::cout << "✅ EmbeddedDatabaseOperations workflow tests passed" << std::endl;
}

// Test 7: EmbeddedTransactionOperations functionality
void test_embedded_transaction_operations()
{
    std::cout << "Test 7: EmbeddedTransactionOperations functionality..." << std::endl;

    auto config = create_test_config();
    EnhancedEmbeddedProvider provider(config);
    assert(provider.initialize());

    auto txn_ops = provider.create_transaction_operations();

    // Test transaction lifecycle
    std::uint32_t txn_handle;
    auto result = txn_ops->begin_transaction(1000, txn_handle);
    assert(result == ProviderResult::Success);
    assert(txn_handle != 0);
    assert(txn_ops->is_transaction_active(txn_handle));

    // Test transaction info
    std::string txn_info = txn_ops->get_transaction_info(txn_handle);
    assert(!txn_info.empty());
    assert(txn_info.find("embedded_transaction") != std::string::npos);

    // Test prepare transaction (2PC support)
    result = txn_ops->prepare_transaction(txn_handle);
    assert(result == ProviderResult::Success);

    // Test savepoint operations
    result = txn_ops->create_savepoint(txn_handle, "savepoint1");
    assert(result == ProviderResult::Success);

    result = txn_ops->rollback_to_savepoint(txn_handle, "savepoint1");
    assert(result == ProviderResult::Success);

    result = txn_ops->release_savepoint(txn_handle, "savepoint1");
    assert(result == ProviderResult::Success);

    // Test commit
    result = txn_ops->commit_transaction(txn_handle);
    assert(result == ProviderResult::Success);
    assert(!txn_ops->is_transaction_active(txn_handle));

    // Test rollback
    result = txn_ops->begin_transaction(1000, txn_handle);
    assert(result == ProviderResult::Success);

    result = txn_ops->rollback_transaction(txn_handle);
    assert(result == ProviderResult::Success);
    assert(!txn_ops->is_transaction_active(txn_handle));

    provider.shutdown();

    std::cout << "✅ EmbeddedTransactionOperations tests passed" << std::endl;
}

// Test 8: EmbeddedStatementOperations functionality
void test_embedded_statement_operations()
{
    std::cout << "Test 8: EmbeddedStatementOperations functionality..." << std::endl;

    auto config = create_test_config();
    EnhancedEmbeddedProvider provider(config);
    assert(provider.initialize());

    auto stmt_ops = provider.create_statement_operations();

    // Test statement preparation
    std::string sql = "SELECT id, name FROM users WHERE age > ?";
    std::uint32_t stmt_handle;
    auto result = stmt_ops->prepare_statement(1000, sql, stmt_handle);
    assert(result == ProviderResult::Success);
    assert(stmt_handle != 0);

    // Test prepared execution
    std::vector<std::string> params = {"25"};
    result = stmt_ops->execute_prepared(stmt_handle, params);
    assert(result == ProviderResult::Success);

    // Test result fetching
    std::vector<std::vector<std::string>> all_results;
    result = stmt_ops->fetch_all(stmt_handle, all_results);
    assert(result == ProviderResult::Success);

    std::vector<std::string> single_row;
    result = stmt_ops->fetch_next(stmt_handle, single_row);
    assert(result == ProviderResult::Success);

    // Test cursor operations
    result = stmt_ops->close_cursor(stmt_handle);
    assert(result == ProviderResult::Success);

    // Test immediate execution
    result = stmt_ops->execute_immediate(1000, "CREATE TABLE test (id INT, name TEXT)");
    assert(result == ProviderResult::Success);

    // Test statement metadata
    assert(!stmt_ops->has_more_results(stmt_handle));
    assert(stmt_ops->get_affected_rows(stmt_handle) > 0);

    // Test statement cleanup
    result = stmt_ops->free_statement(stmt_handle);
    assert(result == ProviderResult::Success);

    provider.shutdown();

    std::cout << "✅ EmbeddedStatementOperations tests passed" << std::endl;
}

// Test 9: EmbeddedSecurityOperations functionality
void test_embedded_security_operations()
{
    std::cout << "Test 9: EmbeddedSecurityOperations functionality..." << std::endl;

    auto config = create_test_config();
    EnhancedEmbeddedProvider provider(config);
    assert(provider.initialize());

    auto sec_ops = provider.create_security_operations();

    // Test user authentication
    std::uint32_t user_context;
    auto result = sec_ops->authenticate_user("test_user", "password123", user_context);
    assert(result == ProviderResult::Success);
    assert(user_context != 0);
    assert(sec_ops->is_authenticated(user_context));

    // Test user info
    std::string current_user = sec_ops->get_current_user(user_context);
    assert(current_user == "test_user");

    std::string current_role = sec_ops->get_current_role(user_context);
    assert(current_role == "embedded_user");

    // Test role management
    result = sec_ops->set_role(user_context, "admin");
    assert(result == ProviderResult::Success);

    current_role = sec_ops->get_current_role(user_context);
    assert(current_role == "admin");

    std::vector<std::string> user_roles;
    result = sec_ops->get_user_roles(user_context, user_roles);
    assert(result == ProviderResult::Success);
    assert(user_roles.size() == 1);
    assert(user_roles[0] == "admin");

    // Test permission checking
    result = sec_ops->check_permission(user_context, "database", "read");
    assert(result == ProviderResult::Success);

    // Test password change
    result = sec_ops->change_password(user_context, "password123", "newpassword456");
    assert(result == ProviderResult::Success);

    provider.shutdown();

    std::cout << "✅ EmbeddedSecurityOperations tests passed" << std::endl;
}

// Test 10: Provider integration with factory system
void test_embedded_provider_factory_integration()
{
    std::cout << "Test 10: Embedded provider factory integration..." << std::endl;

    auto& factory = DatabaseProviderFactory::instance();
    factory.register_embedded_provider();

    // Test enhanced embedded provider registration
    factory.register_third_party_provider(
        "EnhancedEmbeddedProvider", []() -> std::unique_ptr<DatabaseProvider> {
            auto config = create_test_config();
            return std::make_unique<EnhancedEmbeddedProvider>(config);
        });

    auto config = create_test_config();

    // Test enhanced provider creation
    auto enhanced_provider = factory.create_provider("EnhancedEmbeddedProvider", config);
    assert(enhanced_provider != nullptr);
    assert(enhanced_provider->get_provider_name() == "EnhancedEmbeddedProvider");
    assert(enhanced_provider->get_provider_type() == ProviderType::Embedded);

    // Test initialization and capabilities
    assert(enhanced_provider->initialize());
    auto caps = enhanced_provider->get_capabilities();
    assert(caps.supports_transactions);
    assert(caps.supports_authentication);

    enhanced_provider->shutdown();

    std::cout << "✅ Embedded provider factory integration tests passed" << std::endl;
}

// Test 11: Multi-threaded embedded provider stress test
void test_embedded_provider_concurrent_access()
{
    std::cout << "Test 11: Embedded provider concurrent access..." << std::endl;

    auto config = create_test_config();
    EnhancedEmbeddedProvider provider(config);
    assert(provider.initialize());

    const int num_threads = 4;
    const int operations_per_thread = 50;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    // Launch multiple threads performing operations
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&provider, &success_count, operations_per_thread, t]() {
            auto db_ops = provider.create_database_operations();
            auto conn_info = create_test_connection_info();

            for (int i = 0; i < operations_per_thread; ++i) {
                // Connect
                std::uint32_t conn_handle;
                if (db_ops->connect(conn_info, conn_handle) == ProviderResult::Success) {
                    // Start transaction
                    std::uint32_t txn_handle;
                    if (db_ops->start_transaction(conn_handle, txn_handle) ==
                        ProviderResult::Success) {
                        // Prepare and execute statement
                        std::uint32_t stmt_handle;
                        std::string sql = "SELECT " + std::to_string(t) + " * " + std::to_string(i);
                        if (db_ops->prepare_statement(conn_handle, sql, stmt_handle) ==
                            ProviderResult::Success) {
                            if (db_ops->execute_statement(stmt_handle, {}) ==
                                ProviderResult::Success) {
                                success_count++;
                            }
                            db_ops->free_statement(stmt_handle);
                        }
                        db_ops->commit_transaction(txn_handle);
                    }
                    db_ops->disconnect(conn_handle);
                }

                // Small delay to allow other threads
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    assert(success_count > 0);
    std::cout << "Completed " << success_count.load() << " concurrent operations" << std::endl;

    provider.shutdown();

    std::cout << "✅ Embedded provider concurrent access tests passed" << std::endl;
}

// Test 12: Resource management and cleanup
void test_embedded_provider_resource_management()
{
    std::cout << "Test 12: Embedded provider resource management..." << std::endl;

    auto config = create_test_config();
    EnhancedEmbeddedProvider provider(config);
    assert(provider.initialize());

    auto db_ops = provider.create_database_operations();
    auto conn_info = create_test_connection_info();

    // Create multiple connections and resources
    std::vector<std::uint32_t> connections;
    std::vector<std::uint32_t> transactions;
    std::vector<std::uint32_t> statements;

    for (int i = 0; i < 5; ++i) {
        std::uint32_t conn_handle;
        if (db_ops->connect(conn_info, conn_handle) == ProviderResult::Success) {
            connections.push_back(conn_handle);

            std::uint32_t txn_handle;
            if (db_ops->start_transaction(conn_handle, txn_handle) == ProviderResult::Success) {
                transactions.push_back(txn_handle);
            }

            std::uint32_t stmt_handle;
            if (db_ops->prepare_statement(conn_handle, "SELECT 1", stmt_handle) ==
                ProviderResult::Success) {
                statements.push_back(stmt_handle);
            }
        }
    }

    // Verify resources were created
    assert(!connections.empty());
    assert(!transactions.empty());
    assert(!statements.empty());

    // Test statistics collection
    auto stats = provider.get_statistics();
    assert(stats.connections_active > 0);
    assert(stats.transactions_active > 0);
    assert(stats.statements_prepared > 0);

    // Test resource cleanup
    provider.cleanup_resources();

    // Verify provider is still functional after cleanup
    std::uint32_t test_conn;
    assert(db_ops->connect(conn_info, test_conn) == ProviderResult::Success);
    db_ops->disconnect(test_conn);

    provider.shutdown();

    std::cout << "✅ Embedded provider resource management tests passed" << std::endl;
}

int main()
{
    std::cout << "🚀 Starting Embedded Provider Tests..." << std::endl;

    try {
        test_embedded_database_manager();             // Test 1
        test_embedded_connection_manager();           // Test 2
        test_embedded_transaction_manager();          // Test 3
        test_embedded_statement_manager();            // Test 4
        test_enhanced_embedded_provider_basic();      // Test 5
        test_embedded_database_operations();          // Test 6
        test_embedded_transaction_operations();       // Test 7
        test_embedded_statement_operations();         // Test 8
        test_embedded_security_operations();          // Test 9
        test_embedded_provider_factory_integration(); // Test 10
        test_embedded_provider_concurrent_access();   // Test 11
        test_embedded_provider_resource_management(); // Test 12

        std::cout << "\n🎉 All Embedded Provider Tests Passed! (12/12)" << std::endl;
        std::cout << "✅ Phase 11.3.3: Embedded Provider - Direct engine integration COMPLETE"
                  << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}
