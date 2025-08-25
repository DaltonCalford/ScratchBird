#include "scratchbird/engine/database_link.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <regex>
#include <sstream>

namespace scratchbird::engine
{

    /// DatabaseLinkManager implementation
    DatabaseLinkManager::DatabaseLinkManager(Catalog* catalog) : catalog_(catalog)
    {
    }

    bool DatabaseLinkManager::create_database_link(const DatabaseLinkConfig& config, std::string& error_msg)
    {
        if (config.link_name.empty()) {
            error_msg = "Database link name cannot be empty";
            return false;
        }

        if (config.target_host.empty()) {
            error_msg = "Target host cannot be empty";
            return false;
        }

        if (config.target_database.empty()) {
            error_msg = "Target database cannot be empty";
            return false;
        }

        if (config.fdw_name.empty()) {
            error_msg = "FDW name cannot be empty";
            return false;
        }

        // Check if link already exists
        auto it = link_metadata_.find(config.link_name);
        if (it != link_metadata_.end()) {
            error_msg = "Database link '" + config.link_name + "' already exists";
            return false;
        }

        // Validate FDW exists
        FdwRegistry& registry = FdwRegistry::instance();
        ForeignDataWrapper* fdw = registry.get_fdw(config.fdw_name);
        if (!fdw) {
            error_msg = "Foreign Data Wrapper '" + config.fdw_name + "' not found";
            return false;
        }

        // Create underlying foreign server and user mapping
        if (!create_foreign_server_for_link(config, error_msg)) {
            return false;
        }

        if (!create_user_mapping_for_link(config, error_msg)) {
            return false;
        }

        // Create link metadata
        DatabaseLinkMetadata metadata;
        metadata.config = config;
        metadata.status = DatabaseLinkStatus::Inactive;
        metadata.last_error = "";
        metadata.query_count = 0;
        metadata.error_count = 0;

        link_metadata_[config.link_name] = metadata;

        std::cout << "Database link '" << config.link_name << "' created successfully" << std::endl;
        return true;
    }

    bool DatabaseLinkManager::drop_database_link(const std::string& link_name, std::string& error_msg)
    {
        auto it = link_metadata_.find(link_name);
        if (it == link_metadata_.end()) {
            error_msg = "Database link '" + link_name + "' does not exist";
            return false;
        }

        // Disconnect if connected
        std::string disconnect_error;
        disconnect_database_link(link_name, disconnect_error);

        // Remove from metadata
        link_metadata_.erase(it);

        std::cout << "Database link '" << link_name << "' dropped successfully" << std::endl;
        return true;
    }

    bool DatabaseLinkManager::alter_database_link(const std::string& link_name, 
                                                  const DatabaseLinkConfig& new_config,
                                                  std::string& error_msg)
    {
        auto it = link_metadata_.find(link_name);
        if (it == link_metadata_.end()) {
            error_msg = "Database link '" + link_name + "' does not exist";
            return false;
        }

        // Disconnect current connection if active
        std::string disconnect_error;
        disconnect_database_link(link_name, disconnect_error);

        // Update configuration
        it->second.config = new_config;
        it->second.status = DatabaseLinkStatus::Inactive;
        it->second.last_error = "";

        std::cout << "Database link '" << link_name << "' altered successfully" << std::endl;
        return true;
    }

    bool DatabaseLinkManager::test_database_link(const std::string& link_name, std::string& error_msg)
    {
        auto it = link_metadata_.find(link_name);
        if (it == link_metadata_.end()) {
            error_msg = "Database link '" + link_name + "' does not exist";
            return false;
        }

        // Get FDW for this link
        auto fdw = get_fdw_for_link(link_name, error_msg);
        if (!fdw) {
            update_link_stats(link_name, false);
            return false;
        }

        // Test connection
        bool test_result = fdw->test_connection(error_msg);
        update_link_stats(link_name, test_result);

        if (test_result) {
            it->second.status = DatabaseLinkStatus::Connected;
            it->second.last_connected = std::chrono::steady_clock::now();
            std::cout << "Database link '" << link_name << "' test successful" << std::endl;
        } else {
            it->second.status = DatabaseLinkStatus::Error;
            it->second.last_error = error_msg;
            std::cout << "Database link '" << link_name << "' test failed: " << error_msg << std::endl;
        }

        return test_result;
    }

    bool DatabaseLinkManager::connect_database_link(const std::string& link_name, std::string& error_msg)
    {
        auto it = link_metadata_.find(link_name);
        if (it == link_metadata_.end()) {
            error_msg = "Database link '" + link_name + "' does not exist";
            return false;
        }

        // Check if already connected
        auto conn_it = active_connections_.find(link_name);
        if (conn_it != active_connections_.end()) {
            // Test existing connection
            if (conn_it->second->test_connection(error_msg)) {
                it->second.status = DatabaseLinkStatus::Connected;
                return true;
            } else {
                // Connection is stale, remove it
                active_connections_.erase(conn_it);
            }
        }

        // Get and connect FDW
        auto fdw = get_fdw_for_link(link_name, error_msg);
        if (!fdw) {
            update_link_stats(link_name, false);
            it->second.status = DatabaseLinkStatus::Error;
            it->second.last_error = error_msg;
            return false;
        }

        // Store active connection
        active_connections_[link_name] = fdw;
        it->second.status = DatabaseLinkStatus::Connected;
        it->second.last_connected = std::chrono::steady_clock::now();
        update_link_stats(link_name, true);

        std::cout << "Database link '" << link_name << "' connected successfully" << std::endl;
        return true;
    }

    bool DatabaseLinkManager::disconnect_database_link(const std::string& link_name, std::string& error_msg)
    {
        auto it = link_metadata_.find(link_name);
        if (it == link_metadata_.end()) {
            error_msg = "Database link '" + link_name + "' does not exist";
            return false;
        }

        // Remove active connection
        auto conn_it = active_connections_.find(link_name);
        if (conn_it != active_connections_.end()) {
            conn_it->second->close_connection();
            active_connections_.erase(conn_it);
        }

        it->second.status = DatabaseLinkStatus::Inactive;
        std::cout << "Database link '" << link_name << "' disconnected" << std::endl;
        return true;
    }

    bool DatabaseLinkManager::get_database_link_config(const std::string& link_name, DatabaseLinkConfig& config)
    {
        auto it = link_metadata_.find(link_name);
        if (it == link_metadata_.end()) {
            return false;
        }

        config = it->second.config;
        return true;
    }

    bool DatabaseLinkManager::get_database_link_metadata(const std::string& link_name, DatabaseLinkMetadata& metadata)
    {
        auto it = link_metadata_.find(link_name);
        if (it == link_metadata_.end()) {
            return false;
        }

        metadata = it->second;
        return true;
    }

    std::vector<std::string> DatabaseLinkManager::list_database_links() const
    {
        std::vector<std::string> links;
        for (const auto& pair : link_metadata_) {
            links.push_back(pair.first);
        }
        std::sort(links.begin(), links.end());
        return links;
    }

    std::unique_ptr<ForeignResultIterator> DatabaseLinkManager::execute_linked_query(
        const std::string& link_name, const std::string& table_name,
        const std::string& query, const std::vector<std::string>& parameters,
        const FdwExecutionContext& context, std::string& error_msg)
    {
        // Ensure link is connected
        if (!connect_database_link(link_name, error_msg)) {
            return nullptr;
        }

        auto conn_it = active_connections_.find(link_name);
        if (conn_it == active_connections_.end()) {
            error_msg = "No active connection for database link '" + link_name + "'";
            return nullptr;
        }

        // Execute query through FDW
        auto result = conn_it->second->execute_select(query, parameters, context, error_msg);
        
        // Update statistics
        auto it = link_metadata_.find(link_name);
        if (it != link_metadata_.end()) {
            it->second.query_count++;
            if (!result) {
                it->second.error_count++;
                it->second.last_error = error_msg;
            }
        }

        return result;
    }

    bool DatabaseLinkManager::resolve_linked_table(const std::string& table_spec, std::string& table_name,
                                                   std::string& link_name, std::string& error_msg)
    {
        if (!is_linked_table_reference(table_spec)) {
            error_msg = "Not a linked table reference: " + table_spec;
            return false;
        }

        auto parsed = parse_linked_table_reference(table_spec);
        table_name = parsed.first;
        link_name = parsed.second;

        // Verify link exists
        if (link_metadata_.find(link_name) == link_metadata_.end()) {
            error_msg = "Database link '" + link_name + "' does not exist";
            return false;
        }

        return true;
    }

    bool DatabaseLinkManager::begin_distributed_transaction(const std::vector<std::string>& link_names,
                                                           const FdwExecutionContext& context, 
                                                           std::string& error_msg)
    {
        std::vector<std::string> started_links;
        
        for (const auto& link_name : link_names) {
            if (!connect_database_link(link_name, error_msg)) {
                // Rollback already started transactions
                for (const auto& started_link : started_links) {
                    std::string rollback_error;
                    auto conn_it = active_connections_.find(started_link);
                    if (conn_it != active_connections_.end()) {
                        conn_it->second->rollback_transaction(context, rollback_error);
                    }
                }
                return false;
            }

            auto conn_it = active_connections_.find(link_name);
            if (conn_it != active_connections_.end()) {
                if (!conn_it->second->begin_transaction(context, error_msg)) {
                    // Rollback already started transactions
                    for (const auto& started_link : started_links) {
                        std::string rollback_error;
                        auto started_conn_it = active_connections_.find(started_link);
                        if (started_conn_it != active_connections_.end()) {
                            started_conn_it->second->rollback_transaction(context, rollback_error);
                        }
                    }
                    return false;
                }
                started_links.push_back(link_name);
            }
        }

        std::cout << "Distributed transaction started across " << link_names.size() << " links" << std::endl;
        return true;
    }

    bool DatabaseLinkManager::commit_distributed_transaction(const std::vector<std::string>& link_names,
                                                            const FdwExecutionContext& context,
                                                            std::string& error_msg)
    {
        bool all_committed = true;
        std::vector<std::string> commit_errors;

        for (const auto& link_name : link_names) {
            auto conn_it = active_connections_.find(link_name);
            if (conn_it != active_connections_.end()) {
                std::string link_error;
                if (!conn_it->second->commit_transaction(context, link_error)) {
                    all_committed = false;
                    commit_errors.push_back(link_name + ": " + link_error);
                }
            }
        }

        if (!all_committed) {
            error_msg = "Distributed commit failed: ";
            for (const auto& err : commit_errors) {
                error_msg += err + "; ";
            }
            return false;
        }

        std::cout << "Distributed transaction committed across " << link_names.size() << " links" << std::endl;
        return true;
    }

    bool DatabaseLinkManager::rollback_distributed_transaction(const std::vector<std::string>& link_names,
                                                              const FdwExecutionContext& context,
                                                              std::string& error_msg)
    {
        bool all_rolled_back = true;
        std::vector<std::string> rollback_errors;

        for (const auto& link_name : link_names) {
            auto conn_it = active_connections_.find(link_name);
            if (conn_it != active_connections_.end()) {
                std::string link_error;
                if (!conn_it->second->rollback_transaction(context, link_error)) {
                    all_rolled_back = false;
                    rollback_errors.push_back(link_name + ": " + link_error);
                }
            }
        }

        if (!all_rolled_back) {
            error_msg = "Distributed rollback failed: ";
            for (const auto& err : rollback_errors) {
                error_msg += err + "; ";
            }
            return false;
        }

        std::cout << "Distributed transaction rolled back across " << link_names.size() << " links" << std::endl;
        return true;
    }

    bool DatabaseLinkManager::is_linked_table_reference(const std::string& table_spec)
    {
        return table_spec.find('@') != std::string::npos;
    }

    std::pair<std::string, std::string> DatabaseLinkManager::parse_linked_table_reference(const std::string& table_spec)
    {
        std::size_t at_pos = table_spec.find('@');
        if (at_pos == std::string::npos) {
            return std::make_pair(table_spec, "");
        }

        std::string table_name = table_spec.substr(0, at_pos);
        std::string link_name = table_spec.substr(at_pos + 1);
        return std::make_pair(table_name, link_name);
    }

    // Private helper methods
    bool DatabaseLinkManager::create_foreign_server_for_link(const DatabaseLinkConfig& config, std::string& error_msg)
    {
        // Create foreign server configuration
        ForeignServerConfig server_config;
        server_config.server_name = config.link_name + "_server";
        server_config.fdw_name = config.fdw_name;
        server_config.host = config.target_host;
        server_config.port = config.target_port;
        server_config.database = config.target_database;
        server_config.use_ssl = config.use_ssl;
        server_config.ssl_cert_path = config.ssl_cert_path;
        server_config.ssl_key_path = config.ssl_key_path;
        server_config.ssl_ca_path = config.ssl_ca_path;
        server_config.options = config.options;

        // For now, just store the config - in a full implementation, this would be stored in catalog
        std::cout << "Foreign server '" << server_config.server_name << "' created for database link" << std::endl;
        return true;
    }

    bool DatabaseLinkManager::create_user_mapping_for_link(const DatabaseLinkConfig& config, std::string& error_msg)
    {
        // Create user mapping
        UserMapping mapping;
        mapping.local_username = "current_user"; // Would be determined from execution context
        mapping.remote_username = config.username;
        mapping.remote_password = config.password;

        // For now, just log - in a full implementation, this would be stored in catalog
        std::cout << "User mapping created for database link '" << config.link_name << "'" << std::endl;
        return true;
    }

    std::shared_ptr<ForeignDataWrapper> DatabaseLinkManager::get_fdw_for_link(const std::string& link_name, 
                                                                              std::string& error_msg)
    {
        auto it = link_metadata_.find(link_name);
        if (it == link_metadata_.end()) {
            error_msg = "Database link '" + link_name + "' not found";
            return nullptr;
        }

        const DatabaseLinkConfig& config = it->second.config;

        // Get FDW from registry
        FdwRegistry& registry = FdwRegistry::instance();
        ForeignDataWrapper* fdw = registry.get_fdw(config.fdw_name);
        if (!fdw) {
            error_msg = "FDW '" + config.fdw_name + "' not found";
            return nullptr;
        }

        // Create shared_ptr wrapper (Note: this is a simplified approach)
        // In a production implementation, you'd want proper shared ownership
        std::shared_ptr<ForeignDataWrapper> shared_fdw(fdw, [](ForeignDataWrapper*) {
            // No-op deleter since FDW is managed by registry
        });

        // Create server config and user mapping for connection
        ForeignServerConfig server_config;
        server_config.server_name = config.link_name + "_server";
        server_config.fdw_name = config.fdw_name;
        server_config.host = config.target_host;
        server_config.port = config.target_port;
        server_config.database = config.target_database;
        server_config.use_ssl = config.use_ssl;
        server_config.options = config.options;

        UserMapping user_mapping;
        user_mapping.local_username = "current_user";
        user_mapping.remote_username = config.username;
        user_mapping.remote_password = config.password;

        // Establish connection
        if (!shared_fdw->establish_connection(server_config, user_mapping, error_msg)) {
            return nullptr;
        }

        return shared_fdw;
    }

    void DatabaseLinkManager::update_link_stats(const std::string& link_name, bool success)
    {
        auto it = link_metadata_.find(link_name);
        if (it != link_metadata_.end()) {
            if (!success) {
                it->second.error_count++;
            }
        }
    }

    /// DatabaseLinkParser implementation
    bool DatabaseLinkParser::parse_create_database_link(const std::string& ddl_sql, 
                                                        DatabaseLinkConfig& config, 
                                                        std::string& error_msg)
    {
        // Simple regex-based parsing for CREATE DATABASE LINK statement
        // Format: CREATE DATABASE LINK link_name CONNECT TO 'connect_string' USING 'fdw_name'
        std::regex create_regex(
            R"(CREATE\s+DATABASE\s+LINK\s+(\w+)\s+CONNECT\s+TO\s+'([^']+)'\s+USING\s+'([^']+)')",
            std::regex_constants::icase
        );

        std::smatch match;
        if (!std::regex_search(ddl_sql, match, create_regex)) {
            error_msg = "Invalid CREATE DATABASE LINK syntax";
            return false;
        }

        config.link_name = match[1].str();
        std::string connect_string = match[2].str();
        config.fdw_name = match[3].str();

        // Parse connect string
        if (!parse_connect_string(connect_string, config, error_msg)) {
            return false;
        }

        return true;
    }

    bool DatabaseLinkParser::parse_drop_database_link(const std::string& ddl_sql, 
                                                      std::string& link_name, 
                                                      std::string& error_msg)
    {
        // Format: DROP DATABASE LINK link_name
        std::regex drop_regex(R"(DROP\s+DATABASE\s+LINK\s+(\w+))", std::regex_constants::icase);

        std::smatch match;
        if (!std::regex_search(ddl_sql, match, drop_regex)) {
            error_msg = "Invalid DROP DATABASE LINK syntax";
            return false;
        }

        link_name = match[1].str();
        return true;
    }

    bool DatabaseLinkParser::parse_alter_database_link(const std::string& ddl_sql, 
                                                       std::string& link_name,
                                                       DatabaseLinkConfig& new_config, 
                                                       std::string& error_msg)
    {
        // Format: ALTER DATABASE LINK link_name CONNECT TO 'new_connect_string'
        std::regex alter_regex(
            R"(ALTER\s+DATABASE\s+LINK\s+(\w+)\s+CONNECT\s+TO\s+'([^']+)')",
            std::regex_constants::icase
        );

        std::smatch match;
        if (!std::regex_search(ddl_sql, match, alter_regex)) {
            error_msg = "Invalid ALTER DATABASE LINK syntax";
            return false;
        }

        link_name = match[1].str();
        new_config.link_name = link_name;
        std::string connect_string = match[2].str();

        // Parse connect string
        if (!parse_connect_string(connect_string, new_config, error_msg)) {
            return false;
        }

        return true;
    }

    bool DatabaseLinkParser::parse_connect_string(const std::string& connect_str, 
                                                  DatabaseLinkConfig& config, 
                                                  std::string& error_msg)
    {
        // Simple connect string format: host:port/database?options
        // Example: localhost:5432/testdb?ssl=true&timeout=30
        
        std::string remaining = connect_str;
        
        // Extract options if present
        std::size_t question_pos = remaining.find('?');
        if (question_pos != std::string::npos) {
            std::string options_str = remaining.substr(question_pos + 1);
            config.options = parse_options(options_str);
            remaining = remaining.substr(0, question_pos);
        }

        // Extract database
        std::size_t slash_pos = remaining.find('/');
        if (slash_pos != std::string::npos) {
            config.target_database = remaining.substr(slash_pos + 1);
            remaining = remaining.substr(0, slash_pos);
        } else {
            error_msg = "Database name required in connect string";
            return false;
        }

        // Extract host and port
        std::size_t colon_pos = remaining.find(':');
        if (colon_pos != std::string::npos) {
            config.target_host = remaining.substr(0, colon_pos);
            std::string port_str = remaining.substr(colon_pos + 1);
            try {
                config.target_port = static_cast<std::uint16_t>(std::stoul(port_str));
            } catch (const std::exception&) {
                error_msg = "Invalid port number in connect string";
                return false;
            }
        } else {
            config.target_host = remaining;
            config.target_port = 5432; // Default PostgreSQL port
        }

        if (config.target_host.empty()) {
            error_msg = "Host required in connect string";
            return false;
        }

        return true;
    }

    std::unordered_map<std::string, std::string> DatabaseLinkParser::parse_options(const std::string& options_str)
    {
        std::unordered_map<std::string, std::string> options;
        
        std::istringstream ss(options_str);
        std::string option;
        
        while (std::getline(ss, option, '&')) {
            std::size_t eq_pos = option.find('=');
            if (eq_pos != std::string::npos) {
                std::string key = option.substr(0, eq_pos);
                std::string value = option.substr(eq_pos + 1);
                options[key] = value;
            }
        }
        
        return options;
    }

} // namespace scratchbird::engine