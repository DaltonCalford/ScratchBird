#pragma once

#include "scratchbird/engine/types.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace scratchbird::engine
{

    /// FDW permissions that can be granted to users/roles
    enum class FdwPermission : std::uint32_t {
        None = 0,
        Usage = 1 << 0,   // Permission to use foreign server
        Connect = 1 << 1, // Permission to establish connections
        Select = 1 << 2,  // Permission to select from foreign tables
        Insert = 1 << 3,  // Permission to insert into foreign tables
        Update = 1 << 4,  // Permission to update foreign tables
        Delete = 1 << 5,  // Permission to delete from foreign tables
        Create = 1 << 6,  // Permission to create foreign tables
        Drop = 1 << 7,    // Permission to drop foreign tables
        Alter = 1 << 8,   // Permission to alter foreign tables
        Grant = 1 << 9,   // Permission to grant permissions to others
        All = 0xFFFFFFFF  // All permissions
    };

    /// Bitwise operations for FdwPermission
    inline FdwPermission operator|(FdwPermission a, FdwPermission b)
    {
        return static_cast<FdwPermission>(static_cast<std::uint32_t>(a) |
                                          static_cast<std::uint32_t>(b));
    }

    inline FdwPermission operator&(FdwPermission a, FdwPermission b)
    {
        return static_cast<FdwPermission>(static_cast<std::uint32_t>(a) &
                                          static_cast<std::uint32_t>(b));
    }

    inline bool has_permission(FdwPermission permissions, FdwPermission check)
    {
        return (permissions & check) == check;
    }

    /// Security context for FDW operations
    struct FdwSecurityContext {
        std::string current_user;
        std::unordered_set<std::string> current_roles;
        std::string session_id;
        std::string client_ip;
        std::int64_t session_start_time;
        bool is_superuser;
    };

    /// Credential storage entry
    struct FdwCredential {
        std::string server_name;
        std::string local_username;
        std::string remote_username;
        std::string encrypted_password;
        std::string encryption_key_id;
        std::unordered_map<std::string, std::string> options;
        std::int64_t created_time;
        std::int64_t last_used_time;
        std::int32_t use_count;
        bool is_active;
    };

    /// Permission grant entry
    struct FdwPermissionGrant {
        std::string object_type; // "SERVER", "FOREIGN_TABLE", "DATABASE_LINK"
        std::string object_name;
        std::string grantee; // Username or role name
        FdwPermission permissions;
        std::string grantor;
        std::int64_t grant_time;
        bool with_grant_option;
    };

    /// Row-level security policy for foreign tables
    struct FdwRowSecurityPolicy {
        std::string policy_name;
        std::string table_name;
        std::string command_type; // "SELECT", "INSERT", "UPDATE", "DELETE", "ALL"
        std::string using_expression;
        std::string with_check_expression;
        bool is_permissive;
        std::vector<std::string> roles;
        bool is_enabled;
    };

    /// FDW credential manager for secure credential storage and rotation
    class FdwCredentialManager
    {
      public:
        FdwCredentialManager();
        ~FdwCredentialManager();

        // Credential management
        bool store_credential(const FdwCredential& credential, std::string& error_msg);
        bool retrieve_credential(const std::string& server_name, const std::string& local_username,
                                 FdwCredential& credential, std::string& error_msg);
        bool update_credential(const FdwCredential& credential, std::string& error_msg);
        bool delete_credential(const std::string& server_name, const std::string& local_username,
                               std::string& error_msg);
        bool list_credentials(const std::string& server_name,
                              std::vector<FdwCredential>& credentials, std::string& error_msg);

        // Credential encryption/decryption
        bool encrypt_password(const std::string& plain_password, const std::string& key_id,
                              std::string& encrypted_password, std::string& error_msg);
        bool decrypt_password(const std::string& encrypted_password, const std::string& key_id,
                              std::string& plain_password, std::string& error_msg);

        // Key management
        bool generate_encryption_key(std::string& key_id, std::string& error_msg);
        bool rotate_encryption_key(const std::string& old_key_id, std::string& new_key_id,
                                   std::string& error_msg);

        // Usage tracking
        bool update_credential_usage(const std::string& server_name,
                                     const std::string& local_username, std::string& error_msg);

      private:
        class Impl;
        std::unique_ptr<Impl> pImpl_;
    };

    /// FDW permission manager for access control
    class FdwPermissionManager
    {
      public:
        FdwPermissionManager();
        ~FdwPermissionManager();

        // Permission grants
        bool grant_permission(const std::string& object_type, const std::string& object_name,
                              const std::string& grantee, FdwPermission permissions,
                              const std::string& grantor, bool with_grant_option,
                              std::string& error_msg);

        bool revoke_permission(const std::string& object_type, const std::string& object_name,
                               const std::string& grantee, FdwPermission permissions,
                               const std::string& grantor, std::string& error_msg);

        bool check_permission(const std::string& object_type, const std::string& object_name,
                              const FdwSecurityContext& context, FdwPermission required_permission,
                              std::string& error_msg);

        // Row-level security
        bool create_row_security_policy(const FdwRowSecurityPolicy& policy, std::string& error_msg);
        bool drop_row_security_policy(const std::string& policy_name, const std::string& table_name,
                                      std::string& error_msg);
        bool enable_row_security(const std::string& table_name, std::string& error_msg);
        bool disable_row_security(const std::string& table_name, std::string& error_msg);

        bool get_row_security_filter(const std::string& table_name, const std::string& command_type,
                                     const FdwSecurityContext& context,
                                     std::string& filter_expression, std::string& error_msg);

        // Permission queries
        bool list_permissions(const std::string& object_type, const std::string& object_name,
                              std::vector<FdwPermissionGrant>& grants, std::string& error_msg);

        bool get_user_permissions(const std::string& username,
                                  std::vector<FdwPermissionGrant>& grants, std::string& error_msg);

      private:
        class Impl;
        std::unique_ptr<Impl> pImpl_;
    };

    /// FDW audit logger for security auditing
    class FdwAuditLogger
    {
      public:
        FdwAuditLogger();
        ~FdwAuditLogger();

        // Audit event types
        enum class AuditEventType {
            Connection,
            Authentication,
            Authorization,
            Query,
            DataAccess,
            Configuration,
            Error
        };

        struct AuditEvent {
            AuditEventType event_type;
            std::string event_name;
            std::string user_name;
            std::string session_id;
            std::string client_ip;
            std::string server_name;
            std::string object_name;
            std::string query_text;
            std::string result_status;
            std::string error_message;
            std::int64_t event_time;
            std::unordered_map<std::string, std::string> additional_data;
        };

        // Audit logging
        bool log_event(const AuditEvent& event, std::string& error_msg);
        bool log_connection_attempt(const std::string& server_name, const std::string& username,
                                    const std::string& client_ip, bool success,
                                    const std::string& error_msg_if_failed);
        bool log_query_execution(const std::string& server_name, const std::string& username,
                                 const std::string& query, bool success,
                                 std::uint64_t rows_affected,
                                 const std::string& error_msg_if_failed);

        // Audit configuration
        bool set_audit_level(const std::string& level, std::string& error_msg);
        bool enable_audit_for_server(const std::string& server_name, std::string& error_msg);
        bool disable_audit_for_server(const std::string& server_name, std::string& error_msg);

      private:
        class Impl;
        std::unique_ptr<Impl> pImpl_;
    };

    /// FDW security manager - main interface for FDW security operations
    class FdwSecurityManager
    {
      public:
        FdwSecurityManager();
        ~FdwSecurityManager();

        // Security context
        bool create_security_context(const std::string& username, const std::string& session_id,
                                     const std::string& client_ip, FdwSecurityContext& context,
                                     std::string& error_msg);

        bool validate_security_context(const FdwSecurityContext& context, std::string& error_msg);

        // Integrated security operations
        bool authorize_server_access(const std::string& server_name,
                                     const FdwSecurityContext& context, std::string& error_msg);

        bool authorize_table_operation(const std::string& table_name, const std::string& operation,
                                       const FdwSecurityContext& context, std::string& error_msg);

        bool get_user_credentials(const std::string& server_name, const FdwSecurityContext& context,
                                  FdwCredential& credential, std::string& error_msg);

        // SQL injection prevention
        bool validate_query_safety(const std::string& query, std::string& error_msg);
        std::string sanitize_identifier(const std::string& identifier);
        std::string escape_string_literal(const std::string& literal);

        // Network security validation
        bool validate_connection_security(const std::string& connection_string, bool require_ssl,
                                          std::string& error_msg);

        // Component access
        FdwCredentialManager& get_credential_manager();
        FdwPermissionManager& get_permission_manager();
        FdwAuditLogger& get_audit_logger();

      private:
        std::unique_ptr<FdwCredentialManager> credential_manager_;
        std::unique_ptr<FdwPermissionManager> permission_manager_;
        std::unique_ptr<FdwAuditLogger> audit_logger_;
    };

} // namespace scratchbird::engine
