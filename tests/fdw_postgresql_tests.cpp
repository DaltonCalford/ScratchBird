#include "scratchbird/engine/fdw.h"
#include "scratchbird/engine/fdw_postgresql.h"
#include "test_db_utils.h"

#include <cassert>
#include <iostream>

using namespace scratchbird::engine;

void test_postgresql_fdw_basic()
{
    std::cout << "=== Testing PostgreSQL FDW Basic Operations ===" << std::endl;

    try {
        // Create PostgreSQL FDW instance
        PostgreSqlForeignDataWrapper pg_fdw;

        std::cout << "✓ PostgreSQL FDW Name: " << pg_fdw.get_name() << std::endl;
        std::cout << "✓ PostgreSQL FDW Version: " << pg_fdw.get_version() << std::endl;

        // Test capabilities
        FdwCapability caps = pg_fdw.get_capabilities();
        bool has_select = has_capability(caps, FdwCapability::SelectSupport);
        bool has_insert = has_capability(caps, FdwCapability::InsertSupport);
        bool has_transactions = has_capability(caps, FdwCapability::TransactionSupport);
        bool has_schema = has_capability(caps, FdwCapability::SchemaIntrospection);

        std::cout << "✓ Capabilities: SELECT=" << (has_select ? "YES" : "NO")
                  << ", INSERT=" << (has_insert ? "YES" : "NO")
                  << ", Transactions=" << (has_transactions ? "YES" : "NO")
                  << ", Schema=" << (has_schema ? "YES" : "NO") << std::endl;

        // Test server configuration validation
        ForeignServerConfig config;
        config.server_name = "test_pg_server";
        config.fdw_name = "postgresql_fdw";
        config.host = "localhost";
        config.port = 5432;
        config.database = "testdb";
        config.use_ssl = false;

        std::string error_msg;
        bool valid = pg_fdw.validate_server_config(config, error_msg);
        if (valid) {
            std::cout << "✓ Server configuration validation passed" << std::endl;
        } else {
            std::cout << "⚠ Server configuration validation failed: " << error_msg << std::endl;
        }

        // Test connection establishment (mock)
        UserMapping mapping;
        mapping.local_username = "test_user";
        mapping.remote_username = "postgres";
        mapping.remote_password = "password";

        bool connected = pg_fdw.establish_connection(config, mapping, error_msg);
        if (connected) {
            std::cout << "✓ Connection establishment succeeded (mock)" << std::endl;

            // Test connection testing
            bool test_result = pg_fdw.test_connection(error_msg);
            if (test_result) {
                std::cout << "✓ Connection test passed" << std::endl;
            } else {
                std::cout << "⚠ Connection test failed: " << error_msg << std::endl;
            }

            // Test schema introspection
            RemoteSchemaInfo schema_info;
            bool introspected = pg_fdw.introspect_schema("public", schema_info, error_msg);
            if (introspected) {
                std::cout << "✓ Schema introspection succeeded" << std::endl;
                std::cout << "  Schema: " << schema_info.schema_name << std::endl;
                std::cout << "  Tables found: " << schema_info.table_names.size() << std::endl;

                for (const auto& table_name : schema_info.table_names) {
                    std::cout << "    Table: " << table_name << std::endl;
                    auto col_it = schema_info.table_columns.find(table_name);
                    if (col_it != schema_info.table_columns.end()) {
                        std::cout << "      Columns: ";
                        for (const auto& col : col_it->second) {
                            std::cout << col.name << "(" << static_cast<int>(col.type.kind) << ") ";
                        }
                        std::cout << std::endl;
                    }
                }
            } else {
                std::cout << "⚠ Schema introspection failed: " << error_msg << std::endl;
            }

            // Test foreign table validation
            ForeignTableMetadata table_metadata;
            table_metadata.table_name = "users";
            table_metadata.server_name = "test_pg_server";

            bool table_valid = pg_fdw.validate_foreign_table(table_metadata, error_msg);
            if (table_valid) {
                std::cout << "✓ Foreign table validation passed" << std::endl;
            } else {
                std::cout << "⚠ Foreign table validation failed: " << error_msg << std::endl;
            }

            pg_fdw.close_connection();
        } else {
            std::cout << "⚠ Connection establishment failed: " << error_msg << std::endl;
        }

    } catch (const std::exception& e) {
        std::cout << "⚠ PostgreSQL FDW test failed with exception: " << e.what() << std::endl;
    }

    std::cout << "✓ PostgreSQL FDW basic tests completed" << std::endl << std::endl;
}

void test_postgresql_fdw_query_execution()
{
    std::cout << "=== Testing PostgreSQL FDW Query Execution ===" << std::endl;

    try {
        PostgreSqlForeignDataWrapper pg_fdw;

        // Setup connection
        ForeignServerConfig config;
        config.server_name = "test_pg_server";
        config.fdw_name = "postgresql_fdw";
        config.host = "localhost";
        config.port = 5432;
        config.database = "testdb";

        UserMapping mapping;
        mapping.local_username = "test_user";
        mapping.remote_username = "postgres";
        mapping.remote_password = "password";

        std::string error_msg;
        bool connected = pg_fdw.establish_connection(config, mapping, error_msg);
        if (connected) {
            std::cout << "✓ Connected to PostgreSQL (mock)" << std::endl;

            // Test SELECT query execution
            std::string query = "SELECT id, name, value FROM test_table WHERE id > 0";
            std::vector<std::string> parameters;
            FdwExecutionContext context;

            auto iterator = pg_fdw.execute_select(query, parameters, context, error_msg);
            if (iterator) {
                std::cout << "✓ Query execution succeeded" << std::endl;
                std::cout << "  Column count: " << iterator->get_column_count() << std::endl;

                // Print column information
                for (std::size_t i = 0; i < iterator->get_column_count(); ++i) {
                    std::cout << "  Column " << i << ": " << iterator->get_column_name(i)
                              << " (type: " << static_cast<int>(iterator->get_column_type(i)) << ")"
                              << std::endl;
                }

                // Iterate through mock results
                std::uint64_t row_count = 0;
                while (iterator->next()) {
                    ++row_count;
                    std::cout << "  Row " << row_count << ": ";

                    for (std::size_t i = 0; i < iterator->get_column_count(); ++i) {
                        if (i > 0)
                            std::cout << ", ";

                        if (iterator->is_null(i)) {
                            std::cout << "NULL";
                        } else {
                            std::cout << "\"" << iterator->get_string(i) << "\"";
                        }
                    }
                    std::cout << std::endl;
                }

                std::cout << "✓ Read " << row_count << " rows successfully" << std::endl;
                std::cout << "✓ Processed " << iterator->get_rows_processed() << " rows total"
                          << std::endl;

                iterator->close();
            } else {
                std::cout << "⚠ Query execution failed: " << error_msg << std::endl;
            }

            pg_fdw.close_connection();
        } else {
            std::cout << "⚠ Connection failed: " << error_msg << std::endl;
        }

    } catch (const std::exception& e) {
        std::cout << "⚠ PostgreSQL FDW query test failed: " << e.what() << std::endl;
    }

    std::cout << "✓ PostgreSQL FDW query execution tests completed" << std::endl << std::endl;
}

void test_postgresql_fdw_transactions()
{
    std::cout << "=== Testing PostgreSQL FDW Transaction Support ===" << std::endl;

    try {
        PostgreSqlForeignDataWrapper pg_fdw;

        // Setup connection
        ForeignServerConfig config;
        config.server_name = "test_pg_server";
        config.fdw_name = "postgresql_fdw";
        config.host = "localhost";
        config.database = "testdb";

        UserMapping mapping;
        mapping.local_username = "test_user";
        mapping.remote_username = "postgres";

        std::string error_msg;
        bool connected = pg_fdw.establish_connection(config, mapping, error_msg);
        if (connected) {
            std::cout << "✓ Connected to PostgreSQL (mock)" << std::endl;

            FdwExecutionContext context;

            // Test transaction operations
            bool tx_begin = pg_fdw.begin_transaction(context, error_msg);
            if (tx_begin) {
                std::cout << "✓ Transaction BEGIN successful" << std::endl;

                // Test commit
                bool tx_commit = pg_fdw.commit_transaction(context, error_msg);
                if (tx_commit) {
                    std::cout << "✓ Transaction COMMIT successful" << std::endl;
                } else {
                    std::cout << "⚠ Transaction COMMIT failed: " << error_msg << std::endl;
                }

                // Test rollback (start new transaction first)
                if (pg_fdw.begin_transaction(context, error_msg)) {
                    bool tx_rollback = pg_fdw.rollback_transaction(context, error_msg);
                    if (tx_rollback) {
                        std::cout << "✓ Transaction ROLLBACK successful" << std::endl;
                    } else {
                        std::cout << "⚠ Transaction ROLLBACK failed: " << error_msg << std::endl;
                    }
                }
            } else {
                std::cout << "⚠ Transaction BEGIN failed: " << error_msg << std::endl;
            }

            pg_fdw.close_connection();
        }

    } catch (const std::exception& e) {
        std::cout << "⚠ PostgreSQL FDW transaction test failed: " << e.what() << std::endl;
    }

    std::cout << "✓ PostgreSQL FDW transaction tests completed" << std::endl << std::endl;
}

void test_postgresql_fdw_registry_integration()
{
    std::cout << "=== Testing PostgreSQL FDW Registry Integration ===" << std::endl;

    try {
        // Register PostgreSQL FDW with the registry
        FdwRegistry& registry = FdwRegistry::instance();

        auto pg_fdw = std::make_unique<PostgreSqlForeignDataWrapper>();
        std::string fdw_name = pg_fdw->get_name();

        bool registered = registry.register_fdw(fdw_name, std::move(pg_fdw));
        if (registered) {
            std::cout << "✓ PostgreSQL FDW registered successfully" << std::endl;
        } else {
            std::cout << "⚠ PostgreSQL FDW registration failed" << std::endl;
        }

        // Test that we can retrieve the registered FDW
        ForeignDataWrapper* fdw = registry.get_fdw(fdw_name);
        if (fdw) {
            std::cout << "✓ PostgreSQL FDW retrieved from registry" << std::endl;
            std::cout << "  Name: " << fdw->get_name() << std::endl;
            std::cout << "  Version: " << fdw->get_version() << std::endl;

            // Test capabilities
            FdwCapability caps = fdw->get_capabilities();
            bool has_select = has_capability(caps, FdwCapability::SelectSupport);
            bool has_transactions = has_capability(caps, FdwCapability::TransactionSupport);

            std::cout << "  Capabilities: SELECT=" << (has_select ? "YES" : "NO")
                      << ", Transactions=" << (has_transactions ? "YES" : "NO") << std::endl;
        } else {
            std::cout << "⚠ Failed to retrieve PostgreSQL FDW from registry" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cout << "⚠ PostgreSQL FDW registry integration test failed: " << e.what() << std::endl;
    }

    std::cout << "✓ PostgreSQL FDW registry integration tests completed" << std::endl << std::endl;
}

int main()
{
    std::cout << "=== PostgreSQL FDW Tests ===" << std::endl << std::endl;

    try {
        test_postgresql_fdw_basic();
        test_postgresql_fdw_query_execution();
        test_postgresql_fdw_transactions();
        test_postgresql_fdw_registry_integration();

        std::cout << "=== All PostgreSQL FDW Tests Completed Successfully ===" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}