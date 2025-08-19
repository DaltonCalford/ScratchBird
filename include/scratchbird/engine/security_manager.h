#ifndef SCRATCHBIRD_ENGINE_SECURITY_MANAGER_H
#define SCRATCHBIRD_ENGINE_SECURITY_MANAGER_H

#include "scratchbird/engine/system_oids.h"

#include <ctime>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace scratchbird::engine
{

    // Database security and access control system

    // User authentication and authorization
    struct UserInfo {
        UuidBytes oid{};
        std::string username;
        std::string password_hash;
        bool is_active{true};
        bool is_superuser{false};
        std::time_t created_time{0};
        std::time_t last_login{0};
        std::string comment;

        // Account policies
        bool can_login{true};
        bool can_create_db{false};
        bool can_create_role{false};
        std::int32_t connection_limit{-1}; // -1 = unlimited
        std::time_t password_expires{0};   // 0 = never

        std::string to_sql() const;
    };

    // Role-based access control
    struct RoleInfo {
        UuidBytes oid{};
        std::string rolename;
        bool is_superuser{false};
        bool can_inherit{true};
        bool can_create_db{false};
        bool can_create_role{false};
        bool can_login{false};
        std::int32_t connection_limit{-1};
        std::time_t created_time{0};
        std::string comment;

        std::string to_sql() const;
    };

    // Permission types
    enum class PermissionType {
        SELECT,
        INSERT,
        UPDATE,
        DELETE,
        TRUNCATE,
        REFERENCES,
        TRIGGER,
        CREATE,
        CONNECT,
        TEMPORARY,
        EXECUTE,
        USAGE,
        ALL
    };

    // Grant/Revoke operations
    struct Permission {
        PermissionType type;
        std::string object_type; // TABLE, SCHEMA, DATABASE, FUNCTION, etc.
        std::string object_name;
        std::string schema_name;
        bool with_grant_option{false};

        std::string to_sql() const;
    };

    // Access control entry
    struct AccessControlEntry {
        UuidBytes grantee_oid{}; // User or role receiving permission
        std::string grantee_name;
        UuidBytes grantor_oid{}; // User granting permission
        std::string grantor_name;
        Permission permission;
        std::time_t granted_time{0};

        std::string to_sql() const;
    };

    // Security context for current session
    struct SecurityContext {
        UuidBytes user_oid{};
        std::string username;
        std::vector<UuidBytes> role_oids;
        std::vector<std::string> role_names;
        bool is_superuser{false};
        std::time_t session_start{0};
        std::string client_address;

        bool has_role(const std::string& rolename) const;
        bool has_permission(const Permission& perm) const;
    };

    // Main security manager
    class SecurityManager
    {
      public:
        SecurityManager(const std::string& database_path);
        ~SecurityManager();

        // User management
        bool create_user(const std::string& username, const std::string& password,
                         const UserInfo& options = UserInfo{});
        bool drop_user(const std::string& username);
        bool alter_user(const std::string& username, const UserInfo& new_options);

        UserInfo get_user_info(const std::string& username) const;
        std::vector<UserInfo> list_users() const;

        // Role management
        bool create_role(const std::string& rolename, const RoleInfo& options = RoleInfo{});
        bool drop_role(const std::string& rolename);
        bool alter_role(const std::string& rolename, const RoleInfo& new_options);

        RoleInfo get_role_info(const std::string& rolename) const;
        std::vector<RoleInfo> list_roles() const;

        // Role membership
        bool grant_role(const std::string& rolename, const std::string& username);
        bool revoke_role(const std::string& rolename, const std::string& username);
        std::vector<std::string> get_user_roles(const std::string& username) const;
        std::vector<std::string> get_role_members(const std::string& rolename) const;

        // Permission management
        bool grant_permission(const std::string& grantee, const Permission& permission,
                              const std::string& grantor, bool with_grant_option = false);
        bool revoke_permission(const std::string& grantee, const Permission& permission,
                               const std::string& grantor);

        std::vector<AccessControlEntry> get_permissions(const std::string& object_type,
                                                        const std::string& object_name) const;
        std::vector<AccessControlEntry> get_user_permissions(const std::string& username) const;

        // Authentication
        bool authenticate_user(const std::string& username, const std::string& password);
        SecurityContext create_security_context(const std::string& username,
                                                const std::string& client_address = "");

        // Authorization
        bool check_permission(const SecurityContext& context, const Permission& permission) const;
        bool is_superuser(const std::string& username) const;
        bool can_access_database(const std::string& username,
                                 const std::string& database_name) const;

        // Security policies
        bool set_password_policy(const std::string& policy_name, const std::string& policy_value);
        std::string get_password_policy(const std::string& policy_name) const;

        // Audit and logging
        void log_authentication_attempt(const std::string& username, bool success,
                                        const std::string& client_address = "");
        void log_permission_check(const SecurityContext& context, const Permission& permission,
                                  bool granted);

        // Configuration
        void set_password_encryption(const std::string& method)
        {
            password_encryption_ = method;
        }
        void set_audit_logging(bool enabled)
        {
            audit_logging_ = enabled;
        }

      private:
        std::string database_path_;
        std::string password_encryption_;
        bool audit_logging_;

        // Internal user/role storage (would be persistent in real implementation)
        std::unordered_map<std::string, UserInfo> users_;
        std::unordered_map<std::string, RoleInfo> roles_;
        std::unordered_map<std::string, std::unordered_set<std::string>> user_roles_;
        std::vector<AccessControlEntry> access_control_list_;

        // Security policies
        std::unordered_map<std::string, std::string> password_policies_;

        // Helper methods
        std::string hash_password(const std::string& password) const;
        bool verify_password(const std::string& password, const std::string& hash) const;
        UuidBytes generate_user_oid() const;
        UuidBytes generate_role_oid() const;

        bool user_exists(const std::string& username) const;
        bool role_exists(const std::string& rolename) const;

        void initialize_default_roles();
        void initialize_default_policies();

        // Catalog integration
        bool store_user_to_catalog(const UserInfo& user);
        bool store_role_to_catalog(const RoleInfo& role);
        bool store_permission_to_catalog(const AccessControlEntry& ace);

        bool load_users_from_catalog();
        bool load_roles_from_catalog();
        bool load_permissions_from_catalog();
    };

    // High-level security utilities

    // Parse SQL security statements
    struct SecurityStatement {
        enum Type { CREATE_USER, CREATE_ROLE, GRANT, REVOKE, ALTER_USER, ALTER_ROLE };

        Type type;
        std::string target_name;
        std::string password;
        std::vector<Permission> permissions;
        std::vector<std::string> roles;
        std::unordered_map<std::string, std::string> options;

        std::string to_sql() const;
    };

    SecurityStatement parse_security_sql(const std::string& sql);

    // Execute security statements
    bool execute_security_statement(SecurityManager& manager, const SecurityStatement& stmt);

    // Default security setup
    bool initialize_database_security(const std::string& database_path,
                                      const std::string& admin_username = "admin",
                                      const std::string& admin_password = "admin");

    // Security validation
    bool validate_password_strength(const std::string& password);
    bool validate_username(const std::string& username);
    bool validate_rolename(const std::string& rolename);

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_SECURITY_MANAGER_H
