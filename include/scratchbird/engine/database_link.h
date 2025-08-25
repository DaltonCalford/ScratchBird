#pragma once

#include "scratchbird/engine/fdw.h"
#include "scratchbird/engine/types.h"

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

namespace scratchbird::engine
{

    /// Forward declarations
    class Catalog;
    class ExecutorContext;

    /// Database link configuration
    struct DatabaseLinkConfig {
        std::string link_name;
        std::string target_host;
        std::uint16_t target_port = 0;
        std::string target_database;
        std::string fdw_name; // Which FDW to use for this link
        std::string username;
        std::string password;
        bool use_ssl = false;
        std::string ssl_cert_path;
        std::string ssl_key_path;
        std::string ssl_ca_path;
        std::unordered_map<std::string, std::string> options;
    };

    /// Database link status
    enum class DatabaseLinkStatus {
        Inactive,
        Connected,
        Error,
        Connecting
    };

    /// Database link metadata
    struct DatabaseLinkMetadata {
        DatabaseLinkConfig config;
        DatabaseLinkStatus status;
        std::string last_error;
        std::chrono::time_point<std::chrono::steady_clock> last_connected;
        std::uint64_t query_count = 0;
        std::uint64_t error_count = 0;
    };

    /// Database Link Manager - manages database links and provides table@link functionality
    class DatabaseLinkManager
    {
      public:
        explicit DatabaseLinkManager(Catalog* catalog);
        ~DatabaseLinkManager() = default;

        // Database Link DDL operations
        bool create_database_link(const DatabaseLinkConfig& config, std::string& error_msg);
        bool drop_database_link(const std::string& link_name, std::string& error_msg);
        bool alter_database_link(const std::string& link_name, const DatabaseLinkConfig& new_config,
                                 std::string& error_msg);

        // Link management
        bool test_database_link(const std::string& link_name, std::string& error_msg);
        bool connect_database_link(const std::string& link_name, std::string& error_msg);
        bool disconnect_database_link(const std::string& link_name, std::string& error_msg);
        
        // Link information
        bool get_database_link_config(const std::string& link_name, DatabaseLinkConfig& config);
        bool get_database_link_metadata(const std::string& link_name, DatabaseLinkMetadata& metadata);
        std::vector<std::string> list_database_links() const;

        // Query execution through links
        std::unique_ptr<ForeignResultIterator> execute_linked_query(
            const std::string& link_name, const std::string& table_name, 
            const std::string& query, const std::vector<std::string>& parameters,
            const FdwExecutionContext& context, std::string& error_msg);

        // table@link resolution
        bool resolve_linked_table(const std::string& table_spec, std::string& table_name, 
                                  std::string& link_name, std::string& error_msg);

        // Transaction coordination for linked operations
        bool begin_distributed_transaction(const std::vector<std::string>& link_names,
                                          const FdwExecutionContext& context, std::string& error_msg);
        bool commit_distributed_transaction(const std::vector<std::string>& link_names,
                                           const FdwExecutionContext& context, std::string& error_msg);
        bool rollback_distributed_transaction(const std::vector<std::string>& link_names,
                                             const FdwExecutionContext& context, std::string& error_msg);

        // Utility functions
        static bool is_linked_table_reference(const std::string& table_spec);
        static std::pair<std::string, std::string> parse_linked_table_reference(const std::string& table_spec);

      private:
        Catalog* catalog_;
        std::unordered_map<std::string, DatabaseLinkMetadata> link_metadata_;
        std::unordered_map<std::string, std::shared_ptr<ForeignDataWrapper>> active_connections_;

        bool create_foreign_server_for_link(const DatabaseLinkConfig& config, std::string& error_msg);
        bool create_user_mapping_for_link(const DatabaseLinkConfig& config, std::string& error_msg);
        std::shared_ptr<ForeignDataWrapper> get_fdw_for_link(const std::string& link_name, std::string& error_msg);
        void update_link_stats(const std::string& link_name, bool success);
    };

    /// Database Link DDL Parser - parses CREATE DATABASE LINK statements
    class DatabaseLinkParser
    {
      public:
        static bool parse_create_database_link(const std::string& ddl_sql, DatabaseLinkConfig& config, std::string& error_msg);
        static bool parse_drop_database_link(const std::string& ddl_sql, std::string& link_name, std::string& error_msg);
        static bool parse_alter_database_link(const std::string& ddl_sql, std::string& link_name, 
                                             DatabaseLinkConfig& new_config, std::string& error_msg);

      private:
        static bool parse_connect_string(const std::string& connect_str, DatabaseLinkConfig& config, std::string& error_msg);
        static std::unordered_map<std::string, std::string> parse_options(const std::string& options_str);
    };

} // namespace scratchbird::engine