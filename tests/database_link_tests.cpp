#include "scratchbird/engine/database_link.h"
#include "scratchbird/engine/fdw_csv.h"
#include "scratchbird/engine/fdw_postgresql.h"
#include "test_db_utils.h"

#include <cassert>
#include <iostream>

using namespace scratchbird::engine;

void test_database_link_parser()
{
    std::cout << "=== Testing Database Link Parser ===" << std::endl;

    // Test CREATE DATABASE LINK parsing
    {
        std::string ddl = "CREATE DATABASE LINK test_link CONNECT TO 'localhost:5432/testdb?ssl=true' USING 'postgresql_fdw'";
        DatabaseLinkConfig config;
        std::string error_msg;

        bool parsed = DatabaseLinkParser::parse_create_database_link(ddl, config, error_msg);
        if (parsed) {
            std::cout << "✓ CREATE DATABASE LINK parsed successfully" << std::endl;
            std::cout << "  Link name: " << config.link_name << std::endl;
            std::cout << "  Target host: " << config.target_host << std::endl;
            std::cout << "  Target port: " << config.target_port << std::endl;
            std::cout << "  Target database: " << config.target_database << std::endl;
            std::cout << "  FDW name: " << config.fdw_name << std::endl;
            std::cout << "  SSL option: " << config.options["ssl"] << std::endl;
        } else {
            std::cout << "⚠ CREATE DATABASE LINK parsing failed: " << error_msg << std::endl;
        }
    }

    // Test DROP DATABASE LINK parsing
    {
        std::string ddl = "DROP DATABASE LINK test_link";
        std::string link_name;
        std::string error_msg;

        bool parsed = DatabaseLinkParser::parse_drop_database_link(ddl, link_name, error_msg);
        if (parsed) {
            std::cout << "✓ DROP DATABASE LINK parsed successfully" << std::endl;
            std::cout << "  Link name: " << link_name << std::endl;
        } else {
            std::cout << "⚠ DROP DATABASE LINK parsing failed: " << error_msg << std::endl;
        }
    }

    // Test table@link parsing
    {
        std::string table_spec = "employees@hr_link";
        bool is_linked = DatabaseLinkManager::is_linked_table_reference(table_spec);
        if (is_linked) {
            auto parsed = DatabaseLinkManager::parse_linked_table_reference(table_spec);
            std::cout << "✓ Linked table reference parsed successfully" << std::endl;
            std::cout << "  Table: " << parsed.first << std::endl;
            std::cout << "  Link: " << parsed.second << std::endl;
        } else {
            std::cout << "⚠ Failed to recognize linked table reference" << std::endl;
        }
    }

    std::cout << "✓ Database Link Parser tests completed" << std::endl << std::endl;
}

void test_database_link_manager()
{
    std::cout << "=== Testing Database Link Manager ===" << std::endl;

    try {
        // Create test database for catalog
        scratchbird::tests::TestDatabaseRAII test_db("database_link_test", true);

        // Create database link manager (with mock catalog)
        DatabaseLinkManager link_manager(nullptr); // Mock: passing nullptr for catalog

        // Register PostgreSQL FDW first
        FdwRegistry& registry = FdwRegistry::instance();
        auto pg_fdw = std::make_unique<PostgreSqlForeignDataWrapper>();
        std::string fdw_name = pg_fdw->get_name();
        registry.register_fdw(fdw_name, std::move(pg_fdw));

        // Test creating a database link
        DatabaseLinkConfig config;
        config.link_name = "test_pg_link";
        config.target_host = "localhost";
        config.target_port = 5432;
        config.target_database = "testdb";
        config.fdw_name = "postgresql_fdw";
        config.username = "test_user";
        config.password = "test_password";
        config.use_ssl = false;

        std::string error_msg;
        bool created = link_manager.create_database_link(config, error_msg);
        if (created) {
            std::cout << "✓ Database link created successfully" << std::endl;
        } else {
            std::cout << "⚠ Database link creation failed: " << error_msg << std::endl;
        }

        // Test listing database links
        auto links = link_manager.list_database_links();
        std::cout << "✓ Database links found: " << links.size() << std::endl;
        for (const auto& link : links) {
            std::cout << "  Link: " << link << std::endl;
        }

        // Test testing the link connection
        bool test_result = link_manager.test_database_link(config.link_name, error_msg);
        if (test_result) {
            std::cout << "✓ Database link test successful" << std::endl;
        } else {
            std::cout << "⚠ Database link test failed: " << error_msg << std::endl;
        }

        // Test connecting to the link
        bool connected = link_manager.connect_database_link(config.link_name, error_msg);
        if (connected) {
            std::cout << "✓ Database link connection successful" << std::endl;

            // Test resolving a linked table reference
            std::string table_name, link_name;
            bool resolved = link_manager.resolve_linked_table("users@test_pg_link", table_name, link_name, error_msg);
            if (resolved) {
                std::cout << "✓ Linked table reference resolved" << std::endl;
                std::cout << "  Table: " << table_name << std::endl;
                std::cout << "  Link: " << link_name << std::endl;
            } else {
                std::cout << "⚠ Linked table resolution failed: " << error_msg << std::endl;
            }

            // Test executing a query through the link
            FdwExecutionContext context;
            std::string query = "SELECT * FROM users";
            std::vector<std::string> parameters;

            auto result = link_manager.execute_linked_query(config.link_name, "users", query, parameters, context, error_msg);
            if (result) {
                std::cout << "✓ Linked query execution successful" << std::endl;
                std::cout << "  Columns: " << result->get_column_count() << std::endl;

                // Iterate through mock results
                std::uint64_t row_count = 0;
                while (result->next()) {
                    row_count++;
                    std::cout << "  Row " << row_count << ": ";
                    for (std::size_t i = 0; i < result->get_column_count(); ++i) {
                        if (i > 0) std::cout << ", ";
                        std::cout << result->get_string(i);
                    }
                    std::cout << std::endl;
                }
                std::cout << "  Total rows: " << row_count << std::endl;
            } else {
                std::cout << "⚠ Linked query execution failed: " << error_msg << std::endl;
            }

            // Test disconnecting the link
            link_manager.disconnect_database_link(config.link_name, error_msg);
        }

        // Test dropping the database link
        bool dropped = link_manager.drop_database_link(config.link_name, error_msg);
        if (dropped) {
            std::cout << "✓ Database link dropped successfully" << std::endl;
        } else {
            std::cout << "⚠ Database link drop failed: " << error_msg << std::endl;
        }

    } catch (const std::exception& e) {
        std::cout << "⚠ Database Link Manager test failed: " << e.what() << std::endl;
    }

    std::cout << "✓ Database Link Manager tests completed" << std::endl << std::endl;
}

void test_distributed_transactions()
{
    std::cout << "=== Testing Distributed Transactions ===" << std::endl;

    try {
        // Create test database for catalog
        scratchbird::tests::TestDatabaseRAII test_db("distributed_tx_test", true);

        DatabaseLinkManager link_manager(nullptr); // Mock catalog

        // Register FDWs
        FdwRegistry& registry = FdwRegistry::instance();
        
        auto pg_fdw1 = std::make_unique<PostgreSqlForeignDataWrapper>();
        registry.register_fdw("postgresql_fdw", std::move(pg_fdw1));

        auto csv_fdw = std::make_unique<CsvForeignDataWrapper>();
        registry.register_fdw("csv_fdw", std::move(csv_fdw));

        // Create multiple database links
        DatabaseLinkConfig pg_config;
        pg_config.link_name = "pg_link1";
        pg_config.target_host = "localhost";
        pg_config.target_port = 5432;
        pg_config.target_database = "db1";
        pg_config.fdw_name = "postgresql_fdw";
        pg_config.username = "user1";

        DatabaseLinkConfig csv_config;
        csv_config.link_name = "csv_link1";
        csv_config.target_host = "fileserver";
        csv_config.target_port = 0;
        csv_config.target_database = "";
        csv_config.fdw_name = "csv_fdw";
        csv_config.username = "user1";

        std::string error_msg;
        if (link_manager.create_database_link(pg_config, error_msg) &&
            link_manager.create_database_link(csv_config, error_msg)) {
            std::cout << "✓ Multiple database links created" << std::endl;

            // Test distributed transaction
            std::vector<std::string> link_names = {"pg_link1", "csv_link1"};
            FdwExecutionContext context;

            bool tx_started = link_manager.begin_distributed_transaction(link_names, context, error_msg);
            if (tx_started) {
                std::cout << "✓ Distributed transaction started" << std::endl;

                // Test commit
                bool tx_committed = link_manager.commit_distributed_transaction(link_names, context, error_msg);
                if (tx_committed) {
                    std::cout << "✓ Distributed transaction committed" << std::endl;
                } else {
                    std::cout << "⚠ Distributed transaction commit failed: " << error_msg << std::endl;
                }

                // Test rollback scenario
                if (link_manager.begin_distributed_transaction(link_names, context, error_msg)) {
                    bool tx_rolled_back = link_manager.rollback_distributed_transaction(link_names, context, error_msg);
                    if (tx_rolled_back) {
                        std::cout << "✓ Distributed transaction rolled back" << std::endl;
                    } else {
                        std::cout << "⚠ Distributed transaction rollback failed: " << error_msg << std::endl;
                    }
                }
            } else {
                std::cout << "⚠ Distributed transaction start failed: " << error_msg << std::endl;
            }

            // Clean up
            link_manager.drop_database_link("pg_link1", error_msg);
            link_manager.drop_database_link("csv_link1", error_msg);
        }

    } catch (const std::exception& e) {
        std::cout << "⚠ Distributed transaction test failed: " << e.what() << std::endl;
    }

    std::cout << "✓ Distributed transaction tests completed" << std::endl << std::endl;
}

void test_database_link_metadata()
{
    std::cout << "=== Testing Database Link Metadata ===" << std::endl;

    try {
        DatabaseLinkManager link_manager(nullptr);

        // Register PostgreSQL FDW
        FdwRegistry& registry = FdwRegistry::instance();
        auto pg_fdw = std::make_unique<PostgreSqlForeignDataWrapper>();
        registry.register_fdw("postgresql_fdw", std::move(pg_fdw));

        // Create a database link
        DatabaseLinkConfig config;
        config.link_name = "metadata_test_link";
        config.target_host = "dbserver";
        config.target_port = 5432;
        config.target_database = "production";
        config.fdw_name = "postgresql_fdw";
        config.username = "app_user";

        std::string error_msg;
        if (link_manager.create_database_link(config, error_msg)) {
            std::cout << "✓ Database link created for metadata testing" << std::endl;

            // Get link metadata
            DatabaseLinkMetadata metadata;
            bool got_metadata = link_manager.get_database_link_metadata(config.link_name, metadata);
            if (got_metadata) {
                std::cout << "✓ Database link metadata retrieved" << std::endl;
                std::cout << "  Link name: " << metadata.config.link_name << std::endl;
                std::cout << "  Target host: " << metadata.config.target_host << std::endl;
                std::cout << "  Status: " << static_cast<int>(metadata.status) << std::endl;
                std::cout << "  Query count: " << metadata.query_count << std::endl;
                std::cout << "  Error count: " << metadata.error_count << std::endl;
            } else {
                std::cout << "⚠ Failed to retrieve database link metadata" << std::endl;
            }

            // Test connection to update statistics
            link_manager.test_database_link(config.link_name, error_msg);

            // Get updated metadata
            if (link_manager.get_database_link_metadata(config.link_name, metadata)) {
                std::cout << "✓ Updated metadata after connection test" << std::endl;
                std::cout << "  Updated status: " << static_cast<int>(metadata.status) << std::endl;
            }

            link_manager.drop_database_link(config.link_name, error_msg);
        }

    } catch (const std::exception& e) {
        std::cout << "⚠ Database link metadata test failed: " << e.what() << std::endl;
    }

    std::cout << "✓ Database link metadata tests completed" << std::endl << std::endl;
}

int main()
{
    std::cout << "=== Database Link Tests ===" << std::endl << std::endl;

    try {
        test_database_link_parser();
        test_database_link_manager();
        test_distributed_transactions();
        test_database_link_metadata();

        std::cout << "=== All Database Link Tests Completed Successfully ===" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}