#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <shared_mutex>
#include <string>
#include <vector>

namespace ScratchBird
{

    /**
     * Role attributes compatible with Firebird and PostgreSQL
     */
    enum class RoleAttribute : std::uint64_t {
        // PostgreSQL-compatible attributes
        SUPERUSER = 0x0001,   // Bypass all permission checks
        INHERIT = 0x0002,     // Automatically inherit member role privileges (default)
        CREATEROLE = 0x0004,  // Can create, alter, drop roles
        CREATEDB = 0x0008,    // Can create databases
        LOGIN = 0x0010,       // Can be used for initial session authorization
        REPLICATION = 0x0020, // Can initiate streaming replication
        BYPASSRLS = 0x0040,   // Can bypass row-level security policies

        // Firebird-compatible attributes
        DEFAULT_ROLE = 0x0080, // Role automatically activated on connection
        ADMIN_OPTION = 0x0100, // Can grant role to others (WITH ADMIN OPTION)
        TRUSTED = 0x0200,      // Assigned by authentication plugin
        SYSTEM_ROLE = 0x0400,  // System-defined role (like RDB$ADMIN)

        // ScratchBird extensions
        CONNECT = 0x0800,     // Can connect to database (synonym for LOGIN)
        NOINHERIT = 0x1000,   // Explicitly disable inheritance (override default)
        NOLOGIN = 0x2000,     // Explicitly disable login capability
        NOSUPERUSER = 0x4000, // Explicitly disable superuser (for clarity)

        // Password and connection management
        PASSWORD_EXPIRE = 0x8000,  // Password can expire
        CONNECTION_LIMIT = 0x10000 // Has connection limits
    };

    using RoleAttributes = std::uint64_t;

    // Bitwise operators for RoleAttribute enum
    inline RoleAttributes operator|(RoleAttribute lhs, RoleAttribute rhs)
    {
        return static_cast<RoleAttributes>(lhs) | static_cast<RoleAttributes>(rhs);
    }

    inline RoleAttributes operator|(RoleAttributes lhs, RoleAttribute rhs)
    {
        return lhs | static_cast<RoleAttributes>(rhs);
    }

    /**
     * Role membership options (PostgreSQL-style granular control)
     */
    struct RoleMembershipOptions {
        bool admin_option = false;  // Can grant/revoke role to/from others
        bool inherit_option = true; // Exercise role privileges automatically
        bool set_option = true;     // Can use SET ROLE to switch to this role

        // Firebird compatibility
        bool default_role = false; // Activate automatically on connection (Firebird DEFAULT)

        bool operator==(const RoleMembershipOptions& other) const
        {
            return admin_option == other.admin_option && inherit_option == other.inherit_option &&
                   set_option == other.set_option && default_role == other.default_role;
        }
    };

    /**
     * Role information structure
     */
    struct RoleInfo {
        std::string role_name;
        std::string owner_name; // Role creator/owner
        RoleAttributes attributes = 0;
        std::string description;

        // Connection and password policies
        std::int32_t connection_limit = -1; // -1 = unlimited
        std::chrono::system_clock::time_point password_expires;
        std::chrono::system_clock::time_point created_time;
        std::chrono::system_clock::time_point modified_time;

        // Security properties
        bool is_system_role = false;
        bool is_trusted_role = false;

        // Membership tracking
        std::set<std::string> member_roles;  // Roles this role inherits from
        std::set<std::string> granted_roles; // Roles granted to this role

        bool has_attribute(RoleAttribute attr) const
        {
            return (attributes & static_cast<RoleAttributes>(attr)) != 0;
        }

        void set_attribute(RoleAttribute attr, bool enable = true)
        {
            if (enable) {
                attributes |= static_cast<RoleAttributes>(attr);
            } else {
                attributes &= ~static_cast<RoleAttributes>(attr);
            }
        }

        bool can_login() const
        {
            return has_attribute(RoleAttribute::LOGIN) && !has_attribute(RoleAttribute::NOLOGIN);
        }

        bool inherits_roles() const
        {
            return has_attribute(RoleAttribute::INHERIT) &&
                   !has_attribute(RoleAttribute::NOINHERIT);
        }

        bool is_superuser() const
        {
            return has_attribute(RoleAttribute::SUPERUSER) &&
                   !has_attribute(RoleAttribute::NOSUPERUSER);
        }
    };

    /**
     * Security context for session management
     */
    class SecurityContext
    {
      public:
        SecurityContext(const std::string& session_user, const std::string& current_user = "");
        ~SecurityContext() = default;

        // Session identity (cannot be changed during session)
        const std::string& get_session_user() const
        {
            return session_user_;
        }

        // Current effective user (changeable via SET ROLE)
        const std::string& get_current_user() const
        {
            return current_user_;
        }
        bool set_current_role(const std::string& role_name);
        void reset_role(); // Reset to session user

        // Active roles (inherited + explicitly set)
        const std::set<std::string>& get_active_roles() const
        {
            return active_roles_;
        }
        bool has_active_role(const std::string& role_name) const;

        // Permission checking
        bool has_privilege(const std::string& privilege) const;
        bool is_member_of(const std::string& role_name) const;
        bool can_grant_role(const std::string& role_name) const;

        // Security context properties
        bool is_superuser() const;
        bool bypass_rls() const;
        bool can_create_role() const;
        bool can_create_db() const;

        // Connection management
        std::int32_t get_connection_limit() const;
        bool is_within_connection_limit(std::int32_t current_connections) const;

        // Firebird compatibility
        const std::set<std::string>& get_default_roles() const
        {
            return default_roles_;
        }
        bool has_trusted_role() const
        {
            return has_trusted_role_;
        }
        const std::string& get_trusted_role() const
        {
            return trusted_role_;
        }
        void set_trusted_role(const std::string& role_name);

        // Audit trail
        struct SecurityEvent {
            enum Type { RoleSet, RoleReset, TrustedRoleAssigned, PrivilegeCheck, AccessDenied };
            Type type;
            std::string details;
            std::chrono::system_clock::time_point timestamp;
        };

        const std::vector<SecurityEvent>& get_security_events() const
        {
            return security_events_;
        }
        void log_security_event(SecurityEvent::Type type, const std::string& details);

        // Allow RoleManager to access private members
        friend class RoleManager;

      private:
        std::string session_user_;            // Original login identity
        std::string current_user_;            // Current effective identity
        std::set<std::string> active_roles_;  // Currently active roles
        std::set<std::string> default_roles_; // Automatically activated roles

        // Firebird trusted role support
        bool has_trusted_role_ = false;
        std::string trusted_role_;

        // Security event log
        std::vector<SecurityEvent> security_events_;
        mutable std::mutex events_mutex_;

        void refresh_active_roles();
    };

    /**
     * Role membership relationship
     */
    struct RoleMembership {
        std::string role_name;    // The role being granted
        std::string member_name;  // User or role receiving the grant
        std::string grantor_name; // Who granted the role
        RoleMembershipOptions options;
        std::chrono::system_clock::time_point granted_time;

        bool operator<(const RoleMembership& other) const
        {
            if (role_name != other.role_name)
                return role_name < other.role_name;
            return member_name < other.member_name;
        }
    };

    /**
     * System privileges (Firebird-style)
     */
    enum class SystemPrivilege : std::uint64_t {
        // User and role management
        USER_MANAGEMENT = 0x0001,
        CREATE_USER = 0x0002,
        ALTER_USER = 0x0004,
        DROP_USER = 0x0008,

        // Database management
        CREATE_DATABASE = 0x0010,
        ALTER_DATABASE = 0x0020,
        DROP_DATABASE = 0x0040,

        // Object management
        CREATE_TABLE = 0x0080,
        ALTER_ANY_TABLE = 0x0100,
        DROP_ANY_TABLE = 0x0200,
        SELECT_ANY_TABLE = 0x0400,
        INSERT_ANY_TABLE = 0x0800,
        UPDATE_ANY_TABLE = 0x1000,
        DELETE_ANY_TABLE = 0x2000,

        // Advanced privileges
        READ_RAW_PAGES = 0x4000,
        WRITE_RAW_PAGES = 0x8000,
        IGNORE_DB_TRIGGERS = 0x10000,
        CHANGE_SHUTDOWN_MODE = 0x20000,
        TRACE_ANY_ATTACHMENT = 0x40000,
        MONITOR_ANY_ATTACHMENT = 0x80000,

        // Security and maintenance
        ACCESS_SHUTDOWN_DATABASE = 0x100000,
        CREATE_PRIVILEGED_ROLES = 0x200000,
        GET_CONTEXT = 0x400000,
        SET_CONTEXT = 0x800000,
        TRUSTED_AUTHENTICATION = 0x1000000,

        // Replication and backup
        REPLICATION = 0x2000000,
        PHYSICAL_BACKUP = 0x4000000,
        LOGICAL_BACKUP = 0x8000000
    };

    using SystemPrivileges = std::uint64_t;

    /**
     * Role manager for comprehensive role and security management
     */
    class RoleManager
    {
      public:
        RoleManager();
        ~RoleManager();

        // Initialization and configuration
        bool initialize();
        void shutdown();
        bool is_initialized() const
        {
            return initialized_;
        }

        // Role lifecycle management
        bool create_role(const std::string& role_name, const std::string& creator,
                         RoleAttributes attributes = 0, const std::string& description = "");
        bool alter_role(const std::string& role_name, RoleAttributes attributes,
                        const std::string& modifier);
        bool drop_role(const std::string& role_name, const std::string& dropper);
        bool role_exists(const std::string& role_name) const;

        // Role information queries
        std::shared_ptr<RoleInfo> get_role_info(const std::string& role_name) const;
        std::vector<std::string> list_roles() const;
        std::vector<std::string> list_roles_for_user(const std::string& user_name) const;

        // Role membership management
        bool grant_role(const std::string& role_name, const std::string& grantee,
                        const std::string& grantor, const RoleMembershipOptions& options = {});
        bool revoke_role(const std::string& role_name, const std::string& revokee,
                         const std::string& revoker, bool admin_option_only = false);

        // Role membership queries
        std::vector<RoleMembership> get_role_memberships(const std::string& role_name) const;
        std::vector<std::string> get_inherited_roles(const std::string& user_or_role,
                                                     bool recursive = true) const;
        bool is_member_of(const std::string& user_or_role, const std::string& target_role) const;
        bool can_grant_role(const std::string& grantor, const std::string& role_name) const;

        // Security context management
        std::unique_ptr<SecurityContext>
        create_security_context(const std::string& user_name) const;
        bool validate_security_context(const SecurityContext& context) const;

        // System privileges
        bool grant_system_privilege(SystemPrivilege privilege, const std::string& grantee,
                                    const std::string& grantor);
        bool revoke_system_privilege(SystemPrivilege privilege, const std::string& revokee,
                                     const std::string& revoker);
        bool has_system_privilege(const std::string& user_or_role, SystemPrivilege privilege) const;
        SystemPrivileges get_system_privileges(const std::string& user_or_role) const;

        // Predefined system roles (PostgreSQL-style)
        bool create_predefined_roles();
        static std::vector<std::string> get_predefined_role_names();

        // Connection and session management
        bool validate_connection_limit(const std::string& user_name,
                                       std::int32_t current_connections) const;
        bool is_password_expired(const std::string& user_name) const;

        // Administrative functions
        std::vector<std::string> find_roles_with_attribute(RoleAttribute attribute) const;
        std::vector<std::string> find_circular_dependencies() const;
        bool validate_role_hierarchy() const;
        void cleanup_expired_sessions();

        // Role mapping (Firebird-style authentication mapping)
        struct AuthMapping {
            std::string mapping_name;
            std::string plugin_name;
            std::string from_pattern; // Authentication source pattern
            std::string to_role;      // Target role
            bool is_global = false;   // Global vs database-specific
        };

        bool create_mapping(const AuthMapping& mapping, const std::string& creator);
        bool drop_mapping(const std::string& mapping_name, const std::string& dropper);
        std::vector<AuthMapping> get_mappings() const;
        std::string resolve_mapping(const std::string& plugin_name,
                                    const std::string& auth_identity) const;

        // Audit and monitoring
        struct RoleAuditEvent {
            enum Type {
                RoleCreated,
                RoleAltered,
                RoleDropped,
                RoleGranted,
                RoleRevoked,
                PrivilegeGranted,
                PrivilegeRevoked,
                MappingCreated,
                MappingDropped,
                SecurityViolation
            };

            Type type;
            std::string principal; // User performing action
            std::string target;    // Target role/user
            std::string details;
            std::chrono::system_clock::time_point timestamp;
        };

        void log_audit_event(RoleAuditEvent::Type type, const std::string& principal,
                             const std::string& target, const std::string& details = "");
        std::vector<RoleAuditEvent>
        get_audit_events(std::chrono::minutes lookback = std::chrono::minutes{60}) const;

        // Configuration and policy
        struct RolePolicy {
            bool enforce_password_expiry = true;
            std::chrono::hours default_password_lifetime{8760}; // 1 year
            std::int32_t max_role_depth = 10;                   // Prevent deep inheritance chains
            bool allow_circular_membership = false;
            bool require_admin_for_system_roles = true;
            std::int32_t max_concurrent_connections_per_user = 100;
        };

        bool set_policy(const RolePolicy& policy);
        const RolePolicy& get_policy() const
        {
            return policy_;
        }

      private:
        // Core state
        bool initialized_ = false;
        RolePolicy policy_;

        // Role storage
        std::map<std::string, std::shared_ptr<RoleInfo>> roles_;
        std::set<RoleMembership> memberships_;
        std::map<std::string, SystemPrivileges> system_privileges_;
        std::vector<AuthMapping> mappings_;

        // Thread safety
        mutable std::shared_mutex roles_mutex_;
        mutable std::mutex audit_mutex_;

        // Audit log
        std::vector<RoleAuditEvent> audit_events_;

        // Internal helper methods
        bool validate_role_name(const std::string& role_name) const;
        bool validate_role_attributes(RoleAttributes attributes) const;
        bool check_circular_dependency(const std::string& role_name,
                                       const std::string& potential_member) const;
        std::set<std::string> resolve_role_inheritance(const std::string& user_or_role,
                                                       std::set<std::string>& visited) const;
        void initialize_predefined_roles();
        void cleanup_expired_audit_events();

        // Predefined role definitions
        static const std::map<std::string, RoleAttributes> PREDEFINED_ROLES;
    };

    // Utility functions
    std::string role_attribute_to_string(RoleAttribute attr);
    RoleAttribute parse_role_attribute(const std::string& attr_str);
    std::string system_privilege_to_string(SystemPrivilege privilege);
    SystemPrivilege parse_system_privilege(const std::string& privilege_str);

    // Role attribute operations
    RoleAttributes combine_attributes(const std::vector<RoleAttribute>& attributes);
    bool has_conflicting_attributes(RoleAttributes attributes);
    std::vector<std::string> validate_attribute_combination(RoleAttributes attributes);

    // SQL command compatibility functions
    std::string generate_create_role_sql(const RoleInfo& role_info, bool firebird_syntax = false);
    std::string generate_grant_role_sql(const RoleMembership& membership,
                                        bool firebird_syntax = false);
    bool parse_role_sql(const std::string& sql, RoleInfo& role_info);

} // namespace ScratchBird
