#include "scratchbird/engine/fdw.h"

#include "scratchbird/engine/catalog_manager.h"
#include "scratchbird/engine/executor.h"

#include <dlfcn.h>
#include <iostream>

namespace scratchbird::engine
{

    /// FdwRegistry implementation
    FdwRegistry& FdwRegistry::instance()
    {
        static FdwRegistry registry;
        return registry;
    }

    bool FdwRegistry::register_fdw(const std::string& name, std::unique_ptr<ForeignDataWrapper> fdw)
    {
        if (!fdw) {
            return false;
        }

        auto [it, inserted] = fdw_implementations_.emplace(name, std::move(fdw));
        return inserted;
    }

    bool FdwRegistry::unregister_fdw(const std::string& name)
    {
        auto it = fdw_implementations_.find(name);
        if (it == fdw_implementations_.end()) {
            return false;
        }

        fdw_implementations_.erase(it);
        return true;
    }

    ForeignDataWrapper* FdwRegistry::get_fdw(const std::string& name)
    {
        auto it = fdw_implementations_.find(name);
        if (it == fdw_implementations_.end()) {
            return nullptr;
        }

        return it->second.get();
    }

    std::vector<std::string> FdwRegistry::list_fdw_names() const
    {
        std::vector<std::string> names;
        names.reserve(fdw_implementations_.size());

        for (const auto& [name, fdw] : fdw_implementations_) {
            names.push_back(name);
        }

        return names;
    }

    bool FdwRegistry::load_fdw_plugin(const std::string& library_path, std::string& error_msg)
    {
        void* handle = dlopen(library_path.c_str(), RTLD_LAZY);
        if (!handle) {
            error_msg = "Failed to load FDW plugin: ";
            error_msg += dlerror();
            return false;
        }

        // Look for the plugin registration function
        typedef ForeignDataWrapper* (*create_fdw_func)();
        create_fdw_func create_fdw = (create_fdw_func)dlsym(handle, "create_fdw_plugin");

        if (!create_fdw) {
            error_msg = "FDW plugin missing create_fdw_plugin function";
            dlclose(handle);
            return false;
        }

        try {
            std::unique_ptr<ForeignDataWrapper> fdw(create_fdw());
            if (!fdw) {
                error_msg = "FDW plugin create_fdw_plugin returned null";
                dlclose(handle);
                return false;
            }

            std::string fdw_name = fdw->get_name();
            if (fdw_implementations_.find(fdw_name) != fdw_implementations_.end()) {
                error_msg = "FDW with name '" + fdw_name + "' already registered";
                dlclose(handle);
                return false;
            }

            loaded_libraries_[fdw_name] = handle;
            fdw_implementations_[fdw_name] = std::move(fdw);

            return true;
        } catch (const std::exception& e) {
            error_msg = "Exception loading FDW plugin: ";
            error_msg += e.what();
            dlclose(handle);
            return false;
        }
    }

    bool FdwRegistry::unload_fdw_plugin(const std::string& name)
    {
        auto lib_it = loaded_libraries_.find(name);
        if (lib_it == loaded_libraries_.end()) {
            return false;
        }

        // Remove from registry
        fdw_implementations_.erase(name);

        // Unload library
        dlclose(lib_it->second);
        loaded_libraries_.erase(lib_it);

        return true;
    }

    /// FdwManager implementation
    FdwManager::FdwManager(Catalog* catalog) : catalog_(catalog) {}

    FdwManager::~FdwManager()
    {
        close_all_connections();
    }

    bool FdwManager::create_foreign_server(const ForeignServerConfig& config,
                                           std::string& error_msg)
    {
        if (config.server_name.empty()) {
            error_msg = "Foreign server name cannot be empty";
            return false;
        }

        if (config.fdw_name.empty()) {
            error_msg = "FDW name cannot be empty";
            return false;
        }

        // Validate that the FDW exists
        ForeignDataWrapper* fdw = FdwRegistry::instance().get_fdw(config.fdw_name);
        if (!fdw) {
            error_msg = "Unknown FDW: " + config.fdw_name;
            return false;
        }

        // Validate server configuration
        if (!fdw->validate_server_config(config, error_msg)) {
            return false;
        }

        // Store server configuration in catalog
        try {
            std::string sql = R"(
            INSERT INTO SDB$FOREIGN_SERVER
            (server_name, fdw_name, host, port, database_name, options, use_ssl, ssl_cert_path, ssl_key_path, ssl_ca_path)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

            // TODO: Execute SQL with parameters
            // This would need integration with the catalog system

            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to store foreign server: ";
            error_msg += e.what();
            return false;
        }
    }

    bool FdwManager::drop_foreign_server(const std::string& server_name, std::string& error_msg)
    {
        if (server_name.empty()) {
            error_msg = "Server name cannot be empty";
            return false;
        }

        try {
            // Close any active connections
            auto fdw_it = active_fdws_.find(server_name);
            if (fdw_it != active_fdws_.end()) {
                fdw_it->second->close_connection();
                active_fdws_.erase(fdw_it);
            }

            // Remove from catalog
            std::string sql = "DELETE FROM SDB$FOREIGN_SERVER WHERE server_name = ?";
            // TODO: Execute SQL with server_name parameter

            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to drop foreign server: ";
            error_msg += e.what();
            return false;
        }
    }

    bool FdwManager::get_foreign_server(const std::string& server_name, ForeignServerConfig& config)
    {
        try {
            std::string sql =
                "SELECT fdw_name, host, port, database_name, options, use_ssl, ssl_cert_path, "
                "ssl_key_path, ssl_ca_path FROM SDB$FOREIGN_SERVER WHERE server_name = ?";

            // TODO: Execute query and populate config
            // This would need integration with the catalog system

            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    std::vector<std::string> FdwManager::list_foreign_servers() const
    {
        std::vector<std::string> servers;

        try {
            std::string sql = "SELECT server_name FROM SDB$FOREIGN_SERVER ORDER BY server_name";

            // TODO: Execute query and populate servers list

        } catch (const std::exception& e) {
            // Return empty list on error
        }

        return servers;
    }

    bool FdwManager::create_user_mapping(const std::string& server_name, const UserMapping& mapping,
                                         std::string& error_msg)
    {
        if (server_name.empty()) {
            error_msg = "Server name cannot be empty";
            return false;
        }

        if (mapping.local_username.empty()) {
            error_msg = "Local username cannot be empty";
            return false;
        }

        try {
            std::string sql = R"(
            INSERT INTO SDB$USER_MAPPING
            (server_name, local_username, remote_username, remote_password, options)
            VALUES (?, ?, ?, ?, ?)
        )";

            // TODO: Execute SQL with encrypted password storage

            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to create user mapping: ";
            error_msg += e.what();
            return false;
        }
    }

    bool FdwManager::drop_user_mapping(const std::string& server_name, const std::string& username,
                                       std::string& error_msg)
    {
        try {
            std::string sql =
                "DELETE FROM SDB$USER_MAPPING WHERE server_name = ? AND local_username = ?";

            // TODO: Execute SQL

            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to drop user mapping: ";
            error_msg += e.what();
            return false;
        }
    }

    bool FdwManager::get_user_mapping(const std::string& server_name, const std::string& username,
                                      UserMapping& mapping)
    {
        try {
            std::string sql = "SELECT remote_username, remote_password, options FROM "
                              "SDB$USER_MAPPING WHERE server_name = ? AND local_username = ?";

            // TODO: Execute query and populate mapping with decrypted password

            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    bool FdwManager::create_foreign_table(const ForeignTableMetadata& table_metadata,
                                          std::string& error_msg)
    {
        if (table_metadata.table_name.empty()) {
            error_msg = "Foreign table name cannot be empty";
            return false;
        }

        if (table_metadata.server_name.empty()) {
            error_msg = "Server name cannot be empty";
            return false;
        }

        // Validate that server exists
        ForeignServerConfig server_config;
        if (!get_foreign_server(table_metadata.server_name, server_config)) {
            error_msg = "Foreign server not found: " + table_metadata.server_name;
            return false;
        }

        // Get FDW and validate table
        ForeignDataWrapper* fdw = FdwRegistry::instance().get_fdw(server_config.fdw_name);
        if (!fdw) {
            error_msg = "FDW not found: " + server_config.fdw_name;
            return false;
        }

        if (!fdw->validate_foreign_table(table_metadata, error_msg)) {
            return false;
        }

        try {
            // Store in catalog
            std::string sql = R"(
            INSERT INTO SDB$FOREIGN_TABLE
            (table_name, server_name, remote_schema, remote_table, options)
            VALUES (?, ?, ?, ?, ?)
        )";

            // TODO: Execute SQL and store column definitions

            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to create foreign table: ";
            error_msg += e.what();
            return false;
        }
    }

    bool FdwManager::drop_foreign_table(const std::string& table_name, std::string& error_msg)
    {
        try {
            std::string sql = "DELETE FROM SDB$FOREIGN_TABLE WHERE table_name = ?";

            // TODO: Execute SQL and clean up column definitions

            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to drop foreign table: ";
            error_msg += e.what();
            return false;
        }
    }

    bool FdwManager::get_foreign_table_metadata(const std::string& table_name,
                                                ForeignTableMetadata& metadata)
    {
        try {
            std::string sql = "SELECT server_name, remote_schema, remote_table, options FROM "
                              "SDB$FOREIGN_TABLE WHERE table_name = ?";

            // TODO: Execute query and load column definitions

            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    bool FdwManager::import_foreign_schema(const std::string& server_name,
                                           const std::string& remote_schema,
                                           const std::string& local_schema,
                                           const std::vector<std::string>& table_filter,
                                           std::string& error_msg)
    {
        // Get server configuration
        ForeignServerConfig server_config;
        if (!get_foreign_server(server_name, server_config)) {
            error_msg = "Foreign server not found: " + server_name;
            return false;
        }

        // Get FDW
        ForeignDataWrapper* fdw = FdwRegistry::instance().get_fdw(server_config.fdw_name);
        if (!fdw) {
            error_msg = "FDW not found: " + server_config.fdw_name;
            return false;
        }

        // Check capability
        if (!has_capability(fdw->get_capabilities(), FdwCapability::SchemaIntrospection)) {
            error_msg = "FDW does not support schema introspection: " + server_config.fdw_name;
            return false;
        }

        // Establish connection
        UserMapping mapping;
        if (!get_user_mapping(server_name, "current_user", mapping)) {
            error_msg = "No user mapping found for current user on server: " + server_name;
            return false;
        }

        if (!fdw->establish_connection(server_config, mapping, error_msg)) {
            return false;
        }

        try {
            // Introspect remote schema
            RemoteSchemaInfo schema_info;
            if (!fdw->introspect_schema(remote_schema, schema_info, error_msg)) {
                fdw->close_connection();
                return false;
            }

            // Filter tables if requested
            std::vector<std::string> tables_to_import;
            if (table_filter.empty()) {
                tables_to_import = schema_info.table_names;
            } else {
                for (const auto& table_name : schema_info.table_names) {
                    for (const auto& filter : table_filter) {
                        // Simple wildcard matching - could be enhanced
                        if (table_name.find(filter) != std::string::npos) {
                            tables_to_import.push_back(table_name);
                            break;
                        }
                    }
                }
            }

            // Create foreign tables
            for (const auto& table_name : tables_to_import) {
                ForeignTableMetadata table_metadata;
                table_metadata.table_name =
                    local_schema.empty() ? table_name : local_schema + "." + table_name;
                table_metadata.server_name = server_name;
                table_metadata.remote_schema = remote_schema;
                table_metadata.remote_table = table_name;

                auto column_it = schema_info.table_columns.find(table_name);
                if (column_it != schema_info.table_columns.end()) {
                    table_metadata.columns = column_it->second;
                }

                auto options_it = schema_info.table_options.find(table_name);
                if (options_it != schema_info.table_options.end()) {
                    table_metadata.options = options_it->second;
                }

                std::string create_error;
                if (!create_foreign_table(table_metadata, create_error)) {
                    // Log error but continue with other tables
                    std::cerr << "Failed to create foreign table " << table_name << ": "
                              << create_error << std::endl;
                }
            }

            fdw->close_connection();
            return true;

        } catch (const std::exception& e) {
            error_msg = "Exception during schema import: ";
            error_msg += e.what();
            fdw->close_connection();
            return false;
        }
    }

    std::unique_ptr<ForeignResultIterator>
    FdwManager::execute_foreign_query(const std::string& table_name, const std::string& query,
                                      const std::vector<std::string>& parameters,
                                      const FdwExecutionContext& context, std::string& error_msg)
    {

        ForeignDataWrapper* fdw = get_fdw_for_table(table_name);
        if (!fdw) {
            error_msg = "No FDW found for table: " + table_name;
            return nullptr;
        }

        return fdw->execute_select(query, parameters, context, error_msg);
    }

    bool FdwManager::test_foreign_server_connection(const std::string& server_name,
                                                    std::string& error_msg)
    {
        ForeignServerConfig server_config;
        if (!get_foreign_server(server_name, server_config)) {
            error_msg = "Foreign server not found: " + server_name;
            return false;
        }

        ForeignDataWrapper* fdw = FdwRegistry::instance().get_fdw(server_config.fdw_name);
        if (!fdw) {
            error_msg = "FDW not found: " + server_config.fdw_name;
            return false;
        }

        UserMapping mapping;
        if (!get_user_mapping(server_name, "current_user", mapping)) {
            error_msg = "No user mapping found for current user";
            return false;
        }

        if (!fdw->establish_connection(server_config, mapping, error_msg)) {
            return false;
        }

        bool test_result = fdw->test_connection(error_msg);
        fdw->close_connection();

        return test_result;
    }

    void FdwManager::close_all_connections()
    {
        for (auto& [server_name, fdw] : active_fdws_) {
            try {
                fdw->close_connection();
            } catch (const std::exception& e) {
                std::cerr << "Error closing FDW connection for " << server_name << ": " << e.what()
                          << std::endl;
            }
        }
        active_fdws_.clear();
    }

    bool FdwManager::load_fdw_for_server(const std::string& server_name, std::string& error_msg)
    {
        ForeignServerConfig server_config;
        if (!get_foreign_server(server_name, server_config)) {
            error_msg = "Foreign server not found: " + server_name;
            return false;
        }

        ForeignDataWrapper* fdw = FdwRegistry::instance().get_fdw(server_config.fdw_name);
        if (!fdw) {
            error_msg = "FDW not found: " + server_config.fdw_name;
            return false;
        }

        // Store reference for this server
        active_fdws_[server_name] = std::unique_ptr<ForeignDataWrapper>(fdw);

        return true;
    }

    ForeignDataWrapper* FdwManager::get_fdw_for_table(const std::string& table_name)
    {
        ForeignTableMetadata metadata;
        if (!get_foreign_table_metadata(table_name, metadata)) {
            return nullptr;
        }

        auto fdw_it = active_fdws_.find(metadata.server_name);
        if (fdw_it != active_fdws_.end()) {
            return fdw_it->second.get();
        }

        // Load FDW if not active
        std::string error_msg;
        if (!load_fdw_for_server(metadata.server_name, error_msg)) {
            return nullptr;
        }

        auto new_fdw_it = active_fdws_.find(metadata.server_name);
        return new_fdw_it != active_fdws_.end() ? new_fdw_it->second.get() : nullptr;
    }

} // namespace scratchbird::engine
