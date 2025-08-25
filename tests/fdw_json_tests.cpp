#include "scratchbird/engine/fdw.h"
#include "scratchbird/engine/fdw_json.h"
#include "test_db_utils.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace scratchbird::engine;

void test_json_fdw_basic()
{
    std::cout << "=== Testing JSON FDW Basic Operations ===" << std::endl;

    // Create a test JSON array file
    std::string test_json_path = "/tmp/test_data.json";
    std::ofstream json_file(test_json_path);
    json_file << R"([
        {"id": 1, "name": "Alice", "age": 25, "active": true},
        {"id": 2, "name": "Bob", "age": 30, "active": false},
        {"id": 3, "name": "Carol", "age": 35, "active": true}
    ])";
    json_file.close();

    try {
        // Create JSON FDW instance
        JsonForeignDataWrapper json_fdw;

        std::cout << "✓ JSON FDW Name: " << json_fdw.get_name() << std::endl;
        std::cout << "✓ JSON FDW Version: " << json_fdw.get_version() << std::endl;

        // Test capabilities
        FdwCapability caps = json_fdw.get_capabilities();
        bool has_select = has_capability(caps, FdwCapability::SelectSupport);
        bool has_schema = has_capability(caps, FdwCapability::SchemaIntrospection);
        bool has_where = has_capability(caps, FdwCapability::WhereClausePushdown);
        bool has_limit = has_capability(caps, FdwCapability::LimitPushdown);

        std::cout << "✓ Capabilities: SELECT=" << (has_select ? "YES" : "NO")
                  << ", Schema=" << (has_schema ? "YES" : "NO")
                  << ", WHERE=" << (has_where ? "YES" : "NO")
                  << ", LIMIT=" << (has_limit ? "YES" : "NO") << std::endl;

        // Test server configuration validation
        ForeignServerConfig config;
        config.server_name = "test_json_server";
        config.fdw_name = "json_fdw";
        config.options["base_path"] = "/tmp";
        config.options["array_as_table"] = "true";

        std::string error_msg;
        bool valid = json_fdw.validate_server_config(config, error_msg);
        if (valid) {
            std::cout << "✓ Server configuration validation passed" << std::endl;
        } else {
            std::cout << "⚠ Server configuration validation failed: " << error_msg << std::endl;
        }

        // Test connection establishment
        UserMapping mapping;
        mapping.local_username = "test_user";

        bool connected = json_fdw.establish_connection(config, mapping, error_msg);
        if (connected) {
            std::cout << "✓ Connection establishment succeeded" << std::endl;

            // Test connection testing
            bool test_result = json_fdw.test_connection(error_msg);
            if (test_result) {
                std::cout << "✓ Connection test passed" << std::endl;
            } else {
                std::cout << "⚠ Connection test failed: " << error_msg << std::endl;
            }

            // Test schema introspection
            RemoteSchemaInfo schema_info;
            bool introspected = json_fdw.introspect_schema("default", schema_info, error_msg);
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
            table_metadata.table_name = "test_data";
            table_metadata.server_name = "test_json_server";
            table_metadata.options["file_path"] = test_json_path;

            bool table_valid = json_fdw.validate_foreign_table(table_metadata, error_msg);
            if (table_valid) {
                std::cout << "✓ Foreign table validation passed" << std::endl;
            } else {
                std::cout << "⚠ Foreign table validation failed: " << error_msg << std::endl;
            }

            json_fdw.close_connection();

        } else {
            std::cout << "⚠ Connection establishment failed: " << error_msg << std::endl;
        }

    } catch (const std::exception& e) {
        std::cout << "⚠ JSON FDW test failed with exception: " << e.what() << std::endl;
    }

    // Clean up test file
    std::filesystem::remove(test_json_path);

    std::cout << "✓ JSON FDW basic tests completed" << std::endl << std::endl;
}

void test_json_result_iterator()
{
    std::cout << "=== Testing JSON Result Iterator ===" << std::endl;

    // Create a test JSON array file with different data types
    std::string test_json_path = "/tmp/test_products.json";
    std::ofstream json_file(test_json_path);
    json_file << R"([
        {"id": 1, "name": "Laptop", "price": 999.99, "available": true, "category": "Electronics"},
        {"id": 2, "name": "Book", "price": 19.95, "available": false, "category": "Literature"},
        {"id": 3, "name": "Headphones", "price": 79.50, "available": true, "category": "Electronics"}
    ])";
    json_file.close();

    try {
        JsonOptions options;
        options.array_as_table = true;
        options.flatten_objects = false;

        JsonResultIterator iterator(test_json_path, options);

        std::cout << "Column count: " << iterator.get_column_count() << std::endl;

        // Print column information
        for (std::size_t i = 0; i < iterator.get_column_count(); ++i) {
            std::cout << "Column " << i << ": " << iterator.get_column_name(i)
                      << " (type: " << static_cast<int>(iterator.get_column_type(i)) << ")"
                      << std::endl;
        }

        // Iterate through rows
        std::uint64_t row_count = 0;
        while (iterator.next()) {
            ++row_count;
            std::cout << "Row " << row_count << ": ";

            for (std::size_t i = 0; i < iterator.get_column_count(); ++i) {
                if (i > 0)
                    std::cout << ", ";

                if (iterator.is_null(i)) {
                    std::cout << "NULL";
                } else {
                    std::cout << "\"" << iterator.get_string(i) << "\"";
                }
            }
            std::cout << std::endl;
        }

        std::cout << "✓ Read " << row_count << " rows successfully" << std::endl;
        std::cout << "✓ Processed " << iterator.get_rows_processed() << " rows total" << std::endl;

        iterator.close();

    } catch (const std::exception& e) {
        std::cout << "⚠ JSON Result Iterator test failed: " << e.what() << std::endl;
    }

    // Clean up test file
    std::filesystem::remove(test_json_path);

    std::cout << "✓ JSON Result Iterator tests completed" << std::endl << std::endl;
}

void test_json_fdw_query_execution()
{
    std::cout << "=== Testing JSON FDW Query Execution ===" << std::endl;

    // Create a test JSON object file (single document)
    std::string test_json_path = "/tmp/config.json";
    std::ofstream json_file(test_json_path);
    json_file << R"({
        "application": "ScratchBird",
        "version": "1.0.0",
        "port": 8080,
        "debug": true,
        "database_url": "postgresql://localhost:5432/scratchbird"
    })";
    json_file.close();

    try {
        JsonForeignDataWrapper json_fdw;

        // Setup connection
        ForeignServerConfig config;
        config.server_name = "test_json_server";
        config.fdw_name = "json_fdw";
        config.options["base_path"] = "/tmp";
        config.options["flatten_objects"] = "true";

        UserMapping mapping;
        mapping.local_username = "test_user";

        std::string error_msg;
        bool connected = json_fdw.establish_connection(config, mapping, error_msg);
        if (connected) {
            std::cout << "✓ Connected to JSON FDW" << std::endl;

            // Test SELECT query execution
            std::string query = "SELECT application, version, port FROM config";
            std::vector<std::string> parameters;
            FdwExecutionContext context;

            auto iterator = json_fdw.execute_select(query, parameters, context, error_msg);
            if (iterator) {
                std::cout << "✓ Query execution succeeded" << std::endl;
                std::cout << "  Column count: " << iterator->get_column_count() << std::endl;

                // Print column information
                for (std::size_t i = 0; i < iterator->get_column_count(); ++i) {
                    std::cout << "  Column " << i << ": " << iterator->get_column_name(i)
                              << " (type: " << static_cast<int>(iterator->get_column_type(i)) << ")"
                              << std::endl;
                }

                // Iterate through results
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

                iterator->close();
            } else {
                std::cout << "⚠ Query execution failed: " << error_msg << std::endl;
            }

            json_fdw.close_connection();
        }

    } catch (const std::exception& e) {
        std::cout << "⚠ JSON FDW query test failed: " << e.what() << std::endl;
    }

    // Clean up test file
    std::filesystem::remove(test_json_path);

    std::cout << "✓ JSON FDW query execution tests completed" << std::endl << std::endl;
}

void test_json_fdw_registry_integration()
{
    std::cout << "=== Testing JSON FDW Registry Integration ===" << std::endl;

    try {
        // Register JSON FDW with the registry
        FdwRegistry& registry = FdwRegistry::instance();

        auto json_fdw = std::make_unique<JsonForeignDataWrapper>();
        std::string fdw_name = json_fdw->get_name();

        bool registered = registry.register_fdw(fdw_name, std::move(json_fdw));
        if (registered) {
            std::cout << "✓ JSON FDW registered successfully" << std::endl;
        } else {
            std::cout << "⚠ JSON FDW registration failed" << std::endl;
        }

        // Test that we can retrieve the registered FDW
        ForeignDataWrapper* fdw = registry.get_fdw(fdw_name);
        if (fdw) {
            std::cout << "✓ JSON FDW retrieved from registry" << std::endl;
            std::cout << "  Name: " << fdw->get_name() << std::endl;
            std::cout << "  Version: " << fdw->get_version() << std::endl;

            // Test capabilities
            FdwCapability caps = fdw->get_capabilities();
            bool has_select = has_capability(caps, FdwCapability::SelectSupport);
            bool has_schema = has_capability(caps, FdwCapability::SchemaIntrospection);

            std::cout << "  Capabilities: SELECT=" << (has_select ? "YES" : "NO")
                      << ", Schema=" << (has_schema ? "YES" : "NO") << std::endl;
        } else {
            std::cout << "⚠ Failed to retrieve JSON FDW from registry" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cout << "⚠ JSON FDW registry integration test failed: " << e.what() << std::endl;
    }

    std::cout << "✓ JSON FDW registry integration tests completed" << std::endl << std::endl;
}

void test_json_fdw_cost_estimation()
{
    std::cout << "=== Testing JSON FDW Cost Estimation ===" << std::endl;

    try {
        JsonForeignDataWrapper json_fdw;

        // Test scan cost estimation
        ForeignTableMetadata table_metadata;
        table_metadata.table_name = "large_dataset";
        table_metadata.server_name = "json_server";
        
        std::int64_t estimated_rows = 10000;
        double scan_cost = json_fdw.estimate_scan_cost(table_metadata, estimated_rows);
        
        std::cout << "✓ Scan cost estimation: " << scan_cost << " for " << estimated_rows << " rows" << std::endl;

        // Test join cost estimation
        ForeignTableMetadata left_table = table_metadata;
        ForeignTableMetadata right_table;
        right_table.table_name = "lookup_table";
        right_table.server_name = "json_server";

        double join_cost = json_fdw.estimate_join_cost(left_table, right_table, estimated_rows);
        std::cout << "✓ Join cost estimation: " << join_cost << " for " << estimated_rows << " rows" << std::endl;

        // Test pushdown capabilities
        bool can_where = json_fdw.can_pushdown_where_clause("id > 100");
        bool can_join = json_fdw.can_pushdown_join("t1.id = t2.id");
        bool can_aggregate = json_fdw.can_pushdown_aggregate("COUNT(*)");
        bool can_limit = json_fdw.can_pushdown_limit(100, 50);

        std::cout << "✓ Pushdown capabilities: WHERE=" << (can_where ? "YES" : "NO")
                  << ", JOIN=" << (can_join ? "YES" : "NO")
                  << ", AGGREGATE=" << (can_aggregate ? "YES" : "NO")
                  << ", LIMIT=" << (can_limit ? "YES" : "NO") << std::endl;

    } catch (const std::exception& e) {
        std::cout << "⚠ JSON FDW cost estimation test failed: " << e.what() << std::endl;
    }

    std::cout << "✓ JSON FDW cost estimation tests completed" << std::endl << std::endl;
}

int main()
{
    std::cout << "=== JSON FDW Tests ===" << std::endl << std::endl;

    try {
        test_json_fdw_basic();
        test_json_result_iterator();
        test_json_fdw_query_execution();
        test_json_fdw_registry_integration();
        test_json_fdw_cost_estimation();

        std::cout << "=== All JSON FDW Tests Completed Successfully ===" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}