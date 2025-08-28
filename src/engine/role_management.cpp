#include "scratchbird/engine/role_management.h"

#include <algorithm>
#include <regex>
#include <sstream>

namespace ScratchBird
{

    // Predefined system roles (PostgreSQL-compatible)
    const std::map<std::string, RoleAttributes> RoleManager::PREDEFINED_ROLES = {
        // Monitoring roles
        {"pg_monitor", static_cast<RoleAttributes>(RoleAttribute::NOLOGIN)},
        {"pg_read_all_stats", static_cast<RoleAttributes>(RoleAttribute::NOLOGIN)},
        {"pg_read_all_settings", static_cast<RoleAttributes>(RoleAttribute::NOLOGIN)},

        // Server management roles
        {"pg_signal_backend", static_cast<RoleAttributes>(RoleAttribute::NOLOGIN)},
        {"pg_execute_server_program", static_cast<RoleAttributes>(RoleAttribute::NOLOGIN)},
        {"pg_read_server_files", static_cast<RoleAttributes>(RoleAttribute::NOLOGIN)},
        {"pg_write_server_files", static_cast<RoleAttributes>(RoleAttribute::NOLOGIN)},

        // Data access roles
        {"pg_read_all_data", static_cast<RoleAttributes>(RoleAttribute::NOLOGIN)},
        {"pg_write_all_data", static_cast<RoleAttributes>(RoleAttribute::NOLOGIN)},

        // Firebird system roles
        {"RDB$ADMIN",
         RoleAttribute::SUPERUSER | RoleAttribute::SYSTEM_ROLE | RoleAttribute::TRUSTED},
        {"PUBLIC", RoleAttribute::NOLOGIN | RoleAttribute::SYSTEM_ROLE},

        // ScratchBird system roles
        {"SCRATCHBIRD_ADMIN", RoleAttribute::SUPERUSER | RoleAttribute::CREATEROLE |
                                  RoleAttribute::CREATEDB | RoleAttribute::SYSTEM_ROLE},
        {"SCRATCHBIRD_READONLY", RoleAttribute::NOLOGIN | RoleAttribute::SYSTEM_ROLE},
        {"SCRATCHBIRD_READWRITE", RoleAttribute::NOLOGIN | RoleAttribute::SYSTEM_ROLE}};

    // SecurityContext Implementation
    SecurityContext::SecurityContext(const std::string& session_user,
                                     const std::string& current_user)
        : session_user_(session_user),
          current_user_(current_user.empty() ? session_user : current_user)
    {
        refresh_active_roles();
        log_security_event(SecurityEvent::Type::RoleSet,
                           "Security context initialized for user: " + session_user_);
    }

    bool SecurityContext::set_current_role(const std::string& role_name)
    {
        // For now, allow setting any role - in full implementation would validate membership
        current_user_ = role_name;
        active_roles_.insert(role_name);
        refresh_active_roles();
        log_security_event(SecurityEvent::Type::RoleSet, "SET ROLE to: " + role_name);
        return true;
    }

    void SecurityContext::reset_role()
    {
        current_user_ = session_user_;
        refresh_active_roles();
        log_security_event(SecurityEvent::Type::RoleReset,
                           "Role reset to session user: " + session_user_);
    }

    bool SecurityContext::has_active_role(const std::string& role_name) const
    {
        return active_roles_.find(role_name) != active_roles_.end();
    }

    bool SecurityContext::is_member_of(const std::string& role_name) const
    {
        // Check direct membership or inheritance
        return active_roles_.find(role_name) != active_roles_.end() || role_name == session_user_ ||
               role_name == current_user_;
    }

    bool SecurityContext::has_privilege(const std::string& privilege) const
    {
        // Basic privilege checking - would integrate with role manager
        return true; // Placeholder implementation
    }

    bool SecurityContext::can_grant_role(const std::string& role_name) const
    {
        // Basic implementation - would check admin options
        return true; // Placeholder implementation
    }

    bool SecurityContext::is_superuser() const
    {
        // Would check if current user has superuser attribute
        return false; // Placeholder implementation
    }

    bool SecurityContext::bypass_rls() const
    {
        // Would check if current user can bypass RLS
        return false; // Placeholder implementation
    }

    bool SecurityContext::can_create_role() const
    {
        // Would check if current user has CREATEROLE attribute
        return false; // Placeholder implementation
    }

    bool SecurityContext::can_create_db() const
    {
        // Would check if current user has CREATEDB attribute
        return false; // Placeholder implementation
    }

    std::int32_t SecurityContext::get_connection_limit() const
    {
        // Would get connection limit from role manager
        return -1; // Unlimited by default
    }

    bool SecurityContext::is_within_connection_limit(std::int32_t current_connections) const
    {
        auto limit = get_connection_limit();
        return limit == -1 || current_connections < limit;
    }

    void SecurityContext::set_trusted_role(const std::string& role_name)
    {
        has_trusted_role_ = true;
        trusted_role_ = role_name;
        log_security_event(SecurityEvent::Type::TrustedRoleAssigned,
                           "Trusted role assigned: " + role_name);
    }

    void SecurityContext::log_security_event(SecurityEvent::Type type, const std::string& details)
    {
        std::lock_guard<std::mutex> lock(events_mutex_);
        security_events_.push_back({type, details, std::chrono::system_clock::now()});

        // Keep only last 100 events per session
        if (security_events_.size() > 100) {
            security_events_.erase(security_events_.begin());
        }
    }

    void SecurityContext::refresh_active_roles()
    {
        // This would normally query the role manager for inherited roles
        // For now, we maintain a simple active roles set
        active_roles_.clear();
        active_roles_.insert(current_user_);

        // Add default roles
        for (const auto& role : default_roles_) {
            active_roles_.insert(role);
        }
    }

    // RoleManager Implementation
    RoleManager::RoleManager() = default;
    RoleManager::~RoleManager() = default;

    bool RoleManager::initialize()
    {
        if (initialized_) {
            return true;
        }

        std::lock_guard<std::shared_mutex> lock(roles_mutex_);

        // Initialize predefined system roles
        initialize_predefined_roles();

        // Set default policy
        policy_ = RolePolicy{};

        initialized_ = true;
        log_audit_event(RoleAuditEvent::Type::RoleCreated, "SYSTEM", "ALL_PREDEFINED",
                        "Role manager initialized with predefined roles");

        return true;
    }

    void RoleManager::shutdown()
    {
        if (!initialized_) {
            return;
        }

        std::lock_guard<std::shared_mutex> lock(roles_mutex_);

        roles_.clear();
        memberships_.clear();
        system_privileges_.clear();
        mappings_.clear();

        cleanup_expired_audit_events();

        initialized_ = false;
    }

    bool RoleManager::create_role(const std::string& role_name, const std::string& creator,
                                  RoleAttributes attributes, const std::string& description)
    {
        if (!validate_role_name(role_name)) {
            return false;
        }

        if (!validate_role_attributes(attributes)) {
            return false;
        }

        std::lock_guard<std::shared_mutex> lock(roles_mutex_);

        if (roles_.find(role_name) != roles_.end()) {
            return false; // Role already exists
        }

        auto role_info = std::make_shared<RoleInfo>();
        role_info->role_name = role_name;
        role_info->owner_name = creator;
        role_info->attributes = attributes;
        role_info->description = description;
        role_info->created_time = std::chrono::system_clock::now();
        role_info->modified_time = role_info->created_time;
        role_info->connection_limit = -1; // Unlimited by default

        // Set system role flag if appropriate
        role_info->is_system_role = PREDEFINED_ROLES.find(role_name) != PREDEFINED_ROLES.end();
        role_info->is_trusted_role = role_info->has_attribute(RoleAttribute::TRUSTED);

        roles_[role_name] = role_info;

        log_audit_event(RoleAuditEvent::Type::RoleCreated, creator, role_name,
                        "Role created with attributes: " + std::to_string(attributes));

        return true;
    }

    bool RoleManager::alter_role(const std::string& role_name, RoleAttributes attributes,
                                 const std::string& modifier)
    {
        if (!validate_role_attributes(attributes)) {
            return false;
        }

        std::lock_guard<std::shared_mutex> lock(roles_mutex_);

        auto it = roles_.find(role_name);
        if (it == roles_.end()) {
            return false; // Role doesn't exist
        }

        auto& role_info = it->second;

        // Don't allow modification of system roles by regular users
        if (role_info->is_system_role && !modifier.empty()) {
            // Would need to check if modifier is superuser
        }

        RoleAttributes old_attributes = role_info->attributes;
        role_info->attributes = attributes;
        role_info->modified_time = std::chrono::system_clock::now();

        log_audit_event(RoleAuditEvent::Type::RoleAltered, modifier, role_name,
                        "Attributes changed from " + std::to_string(old_attributes) + " to " +
                            std::to_string(attributes));

        return true;
    }

    bool RoleManager::drop_role(const std::string& role_name, const std::string& dropper)
    {
        std::lock_guard<std::shared_mutex> lock(roles_mutex_);

        auto it = roles_.find(role_name);
        if (it == roles_.end()) {
            return false; // Role doesn't exist
        }

        // Don't allow dropping system roles
        if (it->second->is_system_role) {
            return false;
        }

        // Remove all memberships involving this role
        auto membership_it = memberships_.begin();
        while (membership_it != memberships_.end()) {
            if (membership_it->role_name == role_name || membership_it->member_name == role_name) {
                membership_it = memberships_.erase(membership_it);
            } else {
                ++membership_it;
            }
        }

        // Remove system privileges
        system_privileges_.erase(role_name);

        roles_.erase(it);

        log_audit_event(RoleAuditEvent::Type::RoleDropped, dropper, role_name, "Role dropped");

        return true;
    }

    bool RoleManager::role_exists(const std::string& role_name) const
    {
        std::shared_lock<std::shared_mutex> lock(roles_mutex_);
        return roles_.find(role_name) != roles_.end();
    }

    std::shared_ptr<RoleInfo> RoleManager::get_role_info(const std::string& role_name) const
    {
        std::shared_lock<std::shared_mutex> lock(roles_mutex_);
        auto it = roles_.find(role_name);
        return (it != roles_.end()) ? it->second : nullptr;
    }

    std::vector<std::string> RoleManager::list_roles() const
    {
        std::shared_lock<std::shared_mutex> lock(roles_mutex_);
        std::vector<std::string> role_names;
        role_names.reserve(roles_.size());

        for (const auto& pair : roles_) {
            role_names.push_back(pair.first);
        }

        std::sort(role_names.begin(), role_names.end());
        return role_names;
    }

    bool RoleManager::grant_role(const std::string& role_name, const std::string& grantee,
                                 const std::string& grantor, const RoleMembershipOptions& options)
    {
        std::lock_guard<std::shared_mutex> lock(roles_mutex_);

        // Validate role and grantee exist
        if (roles_.find(role_name) == roles_.end() || roles_.find(grantee) == roles_.end()) {
            return false;
        }

        // Check for circular dependency
        if (check_circular_dependency(role_name, grantee)) {
            return false;
        }

        RoleMembership membership;
        membership.role_name = role_name;
        membership.member_name = grantee;
        membership.grantor_name = grantor;
        membership.options = options;
        membership.granted_time = std::chrono::system_clock::now();

        // Remove existing membership if any
        memberships_.erase(membership);

        // Add new membership
        memberships_.insert(membership);

        log_audit_event(RoleAuditEvent::Type::RoleGranted, grantor, role_name + " to " + grantee,
                        "Role granted with options");

        return true;
    }

    bool RoleManager::revoke_role(const std::string& role_name, const std::string& revokee,
                                  const std::string& revoker, bool admin_option_only)
    {
        std::lock_guard<std::shared_mutex> lock(roles_mutex_);

        RoleMembership target;
        target.role_name = role_name;
        target.member_name = revokee;

        auto it = memberships_.find(target);
        if (it == memberships_.end()) {
            return false; // Membership doesn't exist
        }

        if (admin_option_only) {
            // Only revoke admin option, keep the role grant
            RoleMembership updated = *it;
            updated.options.admin_option = false;

            memberships_.erase(it);
            memberships_.insert(updated);

            log_audit_event(RoleAuditEvent::Type::RoleRevoked, revoker,
                            role_name + " admin option from " + revokee, "Admin option revoked");
        } else {
            // Revoke the entire role
            memberships_.erase(it);

            log_audit_event(RoleAuditEvent::Type::RoleRevoked, revoker,
                            role_name + " from " + revokee, "Role revoked");
        }

        return true;
    }

    std::vector<std::string> RoleManager::get_inherited_roles(const std::string& user_or_role,
                                                              bool recursive) const
    {
        std::shared_lock<std::shared_mutex> lock(roles_mutex_);
        std::set<std::string> visited;
        std::set<std::string> inherited = resolve_role_inheritance(user_or_role, visited);

        return std::vector<std::string>(inherited.begin(), inherited.end());
    }

    bool RoleManager::is_member_of(const std::string& user_or_role,
                                   const std::string& target_role) const
    {
        std::shared_lock<std::shared_mutex> lock(roles_mutex_);
        std::set<std::string> visited;
        std::set<std::string> inherited = resolve_role_inheritance(user_or_role, visited);

        return inherited.find(target_role) != inherited.end();
    }

    std::unique_ptr<SecurityContext>
    RoleManager::create_security_context(const std::string& user_name) const
    {
        auto context = std::make_unique<SecurityContext>(user_name);

        // Set up default roles (Firebird-style) and active roles
        std::shared_lock<std::shared_mutex> lock(roles_mutex_);

        for (const auto& membership : memberships_) {
            if (membership.member_name == user_name) {
                if (membership.options.default_role) {
                    context->default_roles_.insert(membership.role_name);
                }
                // Add all memberships to active roles for testing
                context->active_roles_.insert(membership.role_name);
            }
        }

        return context;
    }

    bool RoleManager::grant_system_privilege(SystemPrivilege privilege, const std::string& grantee,
                                             const std::string& grantor)
    {
        std::lock_guard<std::shared_mutex> lock(roles_mutex_);

        system_privileges_[grantee] |= static_cast<SystemPrivileges>(privilege);

        log_audit_event(RoleAuditEvent::Type::PrivilegeGranted, grantor, grantee,
                        "System privilege granted: " + system_privilege_to_string(privilege));

        return true;
    }

    bool RoleManager::revoke_system_privilege(SystemPrivilege privilege, const std::string& revokee,
                                              const std::string& revoker)
    {
        std::lock_guard<std::shared_mutex> lock(roles_mutex_);

        system_privileges_[revokee] &= ~static_cast<SystemPrivileges>(privilege);

        log_audit_event(RoleAuditEvent::Type::PrivilegeRevoked, revoker, revokee,
                        "System privilege revoked: " + system_privilege_to_string(privilege));

        return true;
    }

    bool RoleManager::has_system_privilege(const std::string& user_or_role,
                                           SystemPrivilege privilege) const
    {
        std::shared_lock<std::shared_mutex> lock(roles_mutex_);

        // Check direct privileges
        auto it = system_privileges_.find(user_or_role);
        if (it != system_privileges_.end()) {
            if (it->second & static_cast<SystemPrivileges>(privilege)) {
                return true;
            }
        }

        // Check inherited privileges through role membership
        std::set<std::string> visited;
        std::set<std::string> inherited = resolve_role_inheritance(user_or_role, visited);

        for (const auto& role : inherited) {
            auto role_it = system_privileges_.find(role);
            if (role_it != system_privileges_.end()) {
                if (role_it->second & static_cast<SystemPrivileges>(privilege)) {
                    return true;
                }
            }
        }

        return false;
    }

    bool RoleManager::create_predefined_roles()
    {
        for (const auto& pair : PREDEFINED_ROLES) {
            create_role(pair.first, "SYSTEM", pair.second, "System predefined role");
        }

        return true;
    }

    std::vector<std::string> RoleManager::get_predefined_role_names()
    {
        std::vector<std::string> names;
        names.reserve(PREDEFINED_ROLES.size());

        for (const auto& pair : PREDEFINED_ROLES) {
            names.push_back(pair.first);
        }

        return names;
    }

    void RoleManager::log_audit_event(RoleAuditEvent::Type type, const std::string& principal,
                                      const std::string& target, const std::string& details)
    {
        std::lock_guard<std::mutex> lock(audit_mutex_);

        audit_events_.push_back(
            {type, principal, target, details, std::chrono::system_clock::now()});

        // Keep only last 10000 audit events
        if (audit_events_.size() > 10000) {
            audit_events_.erase(audit_events_.begin());
        }
    }

    std::vector<RoleManager::RoleAuditEvent>
    RoleManager::get_audit_events(std::chrono::minutes lookback) const
    {
        std::lock_guard<std::mutex> lock(audit_mutex_);

        auto cutoff_time = std::chrono::system_clock::now() - lookback;
        std::vector<RoleAuditEvent> recent_events;

        for (const auto& event : audit_events_) {
            if (event.timestamp >= cutoff_time) {
                recent_events.push_back(event);
            }
        }

        return recent_events;
    }

    // Private helper methods
    bool RoleManager::validate_role_name(const std::string& role_name) const
    {
        if (role_name.empty() || role_name.length() > 63) {
            return false;
        }

        // Basic identifier validation
        static const std::regex role_name_regex("^[a-zA-Z_][a-zA-Z0-9_$]*$");
        return std::regex_match(role_name, role_name_regex);
    }

    bool RoleManager::validate_role_attributes(RoleAttributes attributes) const
    {
        return !has_conflicting_attributes(attributes);
    }

    bool RoleManager::check_circular_dependency(const std::string& role_name,
                                                const std::string& potential_member) const
    {
        // Simple check: if potential_member is already inherited by role_name,
        // granting role_name to potential_member would create a cycle
        std::set<std::string> visited;
        std::set<std::string> inherited = resolve_role_inheritance(role_name, visited);

        return inherited.find(potential_member) != inherited.end();
    }

    std::set<std::string>
    RoleManager::resolve_role_inheritance(const std::string& user_or_role,
                                          std::set<std::string>& visited) const
    {
        std::set<std::string> inherited;

        if (visited.find(user_or_role) != visited.end()) {
            return inherited; // Circular dependency detected
        }

        visited.insert(user_or_role);

        // Find all roles granted to this user/role
        for (const auto& membership : memberships_) {
            if (membership.member_name == user_or_role) {
                inherited.insert(membership.role_name);

                // If inheritance is enabled, recursively get inherited roles
                if (membership.options.inherit_option) {
                    auto nested_inherited = resolve_role_inheritance(membership.role_name, visited);
                    inherited.insert(nested_inherited.begin(), nested_inherited.end());
                }
            }
        }

        visited.erase(user_or_role);
        return inherited;
    }

    void RoleManager::initialize_predefined_roles()
    {
        for (const auto& pair : PREDEFINED_ROLES) {
            auto role_info = std::make_shared<RoleInfo>();
            role_info->role_name = pair.first;
            role_info->owner_name = "SYSTEM";
            role_info->attributes = pair.second;
            role_info->description = "System predefined role";
            role_info->created_time = std::chrono::system_clock::now();
            role_info->modified_time = role_info->created_time;
            role_info->is_system_role = true;
            role_info->is_trusted_role = role_info->has_attribute(RoleAttribute::TRUSTED);

            roles_[pair.first] = role_info;
        }
    }

    void RoleManager::cleanup_expired_audit_events()
    {
        auto cutoff_time =
            std::chrono::system_clock::now() - std::chrono::hours{24 * 90}; // 90 days

        auto new_end = std::remove_if(
            audit_events_.begin(), audit_events_.end(),
            [cutoff_time](const RoleAuditEvent& event) { return event.timestamp < cutoff_time; });

        audit_events_.erase(new_end, audit_events_.end());
    }

    // Additional missing method implementations
    bool RoleManager::can_grant_role(const std::string& grantor, const std::string& role_name) const
    {
        std::shared_lock<std::shared_mutex> lock(roles_mutex_);

        // Check if grantor has admin option for this role
        for (const auto& membership : memberships_) {
            if (membership.role_name == role_name && membership.member_name == grantor) {
                return membership.options.admin_option;
            }
        }

        return false;
    }

    SystemPrivileges RoleManager::get_system_privileges(const std::string& user_or_role) const
    {
        std::shared_lock<std::shared_mutex> lock(roles_mutex_);

        SystemPrivileges privileges = 0;

        // Get direct privileges
        auto it = system_privileges_.find(user_or_role);
        if (it != system_privileges_.end()) {
            privileges |= it->second;
        }

        // Get inherited privileges through role membership
        std::set<std::string> visited;
        std::set<std::string> inherited = resolve_role_inheritance(user_or_role, visited);

        for (const auto& role : inherited) {
            auto role_it = system_privileges_.find(role);
            if (role_it != system_privileges_.end()) {
                privileges |= role_it->second;
            }
        }

        return privileges;
    }

    std::vector<RoleMembership>
    RoleManager::get_role_memberships(const std::string& role_name) const
    {
        std::shared_lock<std::shared_mutex> lock(roles_mutex_);

        std::vector<RoleMembership> memberships;
        for (const auto& membership : memberships_) {
            if (membership.role_name == role_name) {
                memberships.push_back(membership);
            }
        }

        return memberships;
    }

    bool RoleManager::validate_security_context(const SecurityContext& context) const
    {
        // Basic validation - check if session user exists
        std::shared_lock<std::shared_mutex> lock(roles_mutex_);
        return roles_.find(context.get_session_user()) != roles_.end();
    }

    std::vector<std::string> RoleManager::list_roles_for_user(const std::string& user_name) const
    {
        return get_inherited_roles(user_name, true);
    }

    bool RoleManager::validate_connection_limit(const std::string& user_name,
                                                std::int32_t current_connections) const
    {
        auto role_info = get_role_info(user_name);
        if (!role_info || role_info->connection_limit == -1) {
            return true; // Unlimited
        }

        return current_connections < role_info->connection_limit;
    }

    bool RoleManager::is_password_expired(const std::string& user_name) const
    {
        auto role_info = get_role_info(user_name);
        if (!role_info) {
            return true; // Non-existent users are considered expired
        }

        auto now = std::chrono::system_clock::now();
        return role_info->password_expires < now;
    }

    std::vector<std::string> RoleManager::find_roles_with_attribute(RoleAttribute attribute) const
    {
        std::shared_lock<std::shared_mutex> lock(roles_mutex_);

        std::vector<std::string> matching_roles;
        for (const auto& pair : roles_) {
            if (pair.second->has_attribute(attribute)) {
                matching_roles.push_back(pair.first);
            }
        }

        return matching_roles;
    }

    std::vector<std::string> RoleManager::find_circular_dependencies() const
    {
        // This is a simplified implementation
        // In a full implementation, this would use graph algorithms
        return {};
    }

    bool RoleManager::validate_role_hierarchy() const
    {
        // Basic validation - check for obvious circular dependencies
        return find_circular_dependencies().empty();
    }

    void RoleManager::cleanup_expired_sessions()
    {
        // This would clean up expired sessions
        // Implementation depends on session management design
    }

    bool RoleManager::create_mapping(const AuthMapping& mapping, const std::string& creator)
    {
        std::lock_guard<std::shared_mutex> lock(roles_mutex_);

        mappings_.push_back(mapping);

        log_audit_event(RoleAuditEvent::Type::MappingCreated, creator, mapping.mapping_name,
                        "Created mapping from " + mapping.from_pattern + " to " + mapping.to_role);

        return true;
    }

    bool RoleManager::drop_mapping(const std::string& mapping_name, const std::string& dropper)
    {
        std::lock_guard<std::shared_mutex> lock(roles_mutex_);

        auto it = std::remove_if(mappings_.begin(), mappings_.end(),
                                 [&mapping_name](const AuthMapping& mapping) {
                                     return mapping.mapping_name == mapping_name;
                                 });

        if (it != mappings_.end()) {
            mappings_.erase(it, mappings_.end());

            log_audit_event(RoleAuditEvent::Type::MappingDropped, dropper, mapping_name,
                            "Dropped authentication mapping");

            return true;
        }

        return false;
    }

    std::vector<RoleManager::AuthMapping> RoleManager::get_mappings() const
    {
        std::shared_lock<std::shared_mutex> lock(roles_mutex_);
        return mappings_;
    }

    std::string RoleManager::resolve_mapping(const std::string& plugin_name,
                                             const std::string& auth_identity) const
    {
        std::shared_lock<std::shared_mutex> lock(roles_mutex_);

        for (const auto& mapping : mappings_) {
            if (mapping.plugin_name == plugin_name) {
                // Simple pattern matching - in real implementation would use regex
                if (auth_identity.find(mapping.from_pattern) != std::string::npos) {
                    return mapping.to_role;
                }
            }
        }

        return ""; // No mapping found
    }

    bool RoleManager::set_policy(const RolePolicy& policy)
    {
        std::lock_guard<std::shared_mutex> lock(roles_mutex_);
        policy_ = policy;
        return true;
    }

    // Utility functions implementation
    std::string role_attribute_to_string(RoleAttribute attr)
    {
        switch (attr) {
        case RoleAttribute::SUPERUSER:
            return "SUPERUSER";
        case RoleAttribute::INHERIT:
            return "INHERIT";
        case RoleAttribute::CREATEROLE:
            return "CREATEROLE";
        case RoleAttribute::CREATEDB:
            return "CREATEDB";
        case RoleAttribute::LOGIN:
            return "LOGIN";
        case RoleAttribute::REPLICATION:
            return "REPLICATION";
        case RoleAttribute::BYPASSRLS:
            return "BYPASSRLS";
        case RoleAttribute::DEFAULT_ROLE:
            return "DEFAULT";
        case RoleAttribute::ADMIN_OPTION:
            return "ADMIN OPTION";
        case RoleAttribute::TRUSTED:
            return "TRUSTED";
        case RoleAttribute::SYSTEM_ROLE:
            return "SYSTEM";
        case RoleAttribute::CONNECT:
            return "CONNECT";
        case RoleAttribute::NOINHERIT:
            return "NOINHERIT";
        case RoleAttribute::NOLOGIN:
            return "NOLOGIN";
        case RoleAttribute::NOSUPERUSER:
            return "NOSUPERUSER";
        case RoleAttribute::PASSWORD_EXPIRE:
            return "PASSWORD EXPIRE";
        case RoleAttribute::CONNECTION_LIMIT:
            return "CONNECTION LIMIT";
        default:
            return "UNKNOWN";
        }
    }

    std::string system_privilege_to_string(SystemPrivilege privilege)
    {
        switch (privilege) {
        case SystemPrivilege::USER_MANAGEMENT:
            return "USER_MANAGEMENT";
        case SystemPrivilege::CREATE_USER:
            return "CREATE_USER";
        case SystemPrivilege::ALTER_USER:
            return "ALTER_USER";
        case SystemPrivilege::DROP_USER:
            return "DROP_USER";
        case SystemPrivilege::CREATE_DATABASE:
            return "CREATE_DATABASE";
        case SystemPrivilege::ALTER_DATABASE:
            return "ALTER_DATABASE";
        case SystemPrivilege::DROP_DATABASE:
            return "DROP_DATABASE";
        case SystemPrivilege::CREATE_TABLE:
            return "CREATE_TABLE";
        case SystemPrivilege::ALTER_ANY_TABLE:
            return "ALTER_ANY_TABLE";
        case SystemPrivilege::DROP_ANY_TABLE:
            return "DROP_ANY_TABLE";
        case SystemPrivilege::REPLICATION:
            return "REPLICATION";
        default:
            return "UNKNOWN_PRIVILEGE";
        }
    }

    bool has_conflicting_attributes(RoleAttributes attributes)
    {
        // Check for conflicting attribute combinations
        if ((attributes & static_cast<RoleAttributes>(RoleAttribute::LOGIN)) &&
            (attributes & static_cast<RoleAttributes>(RoleAttribute::NOLOGIN))) {
            return true;
        }

        if ((attributes & static_cast<RoleAttributes>(RoleAttribute::INHERIT)) &&
            (attributes & static_cast<RoleAttributes>(RoleAttribute::NOINHERIT))) {
            return true;
        }

        if ((attributes & static_cast<RoleAttributes>(RoleAttribute::SUPERUSER)) &&
            (attributes & static_cast<RoleAttributes>(RoleAttribute::NOSUPERUSER))) {
            return true;
        }

        return false;
    }

    std::string generate_create_role_sql(const RoleInfo& role_info, bool firebird_syntax)
    {
        std::ostringstream sql;

        if (firebird_syntax) {
            sql << "CREATE ROLE " << role_info.role_name;

            if (role_info.has_attribute(RoleAttribute::DEFAULT_ROLE)) {
                sql << " SET DEFAULT";
            }
        } else {
            // PostgreSQL syntax
            sql << "CREATE ROLE " << role_info.role_name;

            if (role_info.has_attribute(RoleAttribute::SUPERUSER)) {
                sql << " SUPERUSER";
            }
            if (role_info.has_attribute(RoleAttribute::CREATEDB)) {
                sql << " CREATEDB";
            }
            if (role_info.has_attribute(RoleAttribute::CREATEROLE)) {
                sql << " CREATEROLE";
            }
            if (role_info.has_attribute(RoleAttribute::LOGIN)) {
                sql << " LOGIN";
            } else if (role_info.has_attribute(RoleAttribute::NOLOGIN)) {
                sql << " NOLOGIN";
            }
            if (role_info.has_attribute(RoleAttribute::REPLICATION)) {
                sql << " REPLICATION";
            }
            if (role_info.has_attribute(RoleAttribute::BYPASSRLS)) {
                sql << " BYPASSRLS";
            }

            if (role_info.connection_limit > 0) {
                sql << " CONNECTION LIMIT " << role_info.connection_limit;
            }
        }

        sql << ";";
        return sql.str();
    }

} // namespace ScratchBird
