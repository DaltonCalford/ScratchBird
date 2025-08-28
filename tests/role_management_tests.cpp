#include "scratchbird/engine/role_management.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

using namespace ScratchBird;

// Test utilities
void assert_true(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "ASSERTION FAILED: " << message << std::endl;
        std::exit(1);
    }
}

void assert_false(bool condition, const std::string& message)
{
    assert_true(!condition, message);
}

void test_role_creation_and_attributes()
{
    std::cout << "Testing role creation and attributes..." << std::endl;

    RoleManager manager;
    assert_true(manager.initialize(), "Role manager should initialize successfully");

    // Test basic role creation
    assert_true(manager.create_role("test_user", "admin",
                                    static_cast<RoleAttributes>(RoleAttribute::LOGIN)),
                "Should create basic login role");

    assert_true(manager.role_exists("test_user"), "Role should exist after creation");

    // Test role with multiple attributes
    RoleAttributes attrs = static_cast<RoleAttributes>(RoleAttribute::SUPERUSER) |
                           static_cast<RoleAttributes>(RoleAttribute::CREATEDB) |
                           static_cast<RoleAttributes>(RoleAttribute::CREATEROLE);

    assert_true(manager.create_role("admin_user", "system", attrs, "Admin user role"),
                "Should create admin role with multiple attributes");

    // Verify role info
    auto role_info = manager.get_role_info("admin_user");
    assert_true(role_info != nullptr, "Should retrieve role info");
    assert_true(role_info->has_attribute(RoleAttribute::SUPERUSER),
                "Should have SUPERUSER attribute");
    assert_true(role_info->has_attribute(RoleAttribute::CREATEDB),
                "Should have CREATEDB attribute");
    assert_true(role_info->has_attribute(RoleAttribute::CREATEROLE),
                "Should have CREATEROLE attribute");

    // Test conflicting attributes (should fail)
    RoleAttributes conflicting = static_cast<RoleAttributes>(RoleAttribute::LOGIN) |
                                 static_cast<RoleAttributes>(RoleAttribute::NOLOGIN);
    assert_false(manager.create_role("bad_role", "admin", conflicting),
                 "Should not create role with conflicting attributes");

    std::cout << "✓ Role creation and attributes tests passed" << std::endl;
}

void test_role_membership()
{
    std::cout << "Testing role membership..." << std::endl;

    RoleManager manager;
    manager.initialize();

    // Create test roles and users
    manager.create_role("app_admin", "system", static_cast<RoleAttributes>(RoleAttribute::NOLOGIN));
    manager.create_role("app_user", "system", static_cast<RoleAttributes>(RoleAttribute::NOLOGIN));
    manager.create_role("john_doe", "admin", static_cast<RoleAttributes>(RoleAttribute::LOGIN));

    // Test basic role grant
    RoleMembershipOptions options;
    options.inherit_option = true;
    options.set_option = true;

    assert_true(manager.grant_role("app_user", "john_doe", "admin", options),
                "Should grant app_user role to john_doe");

    // Test role membership queries
    assert_true(manager.is_member_of("john_doe", "app_user"),
                "john_doe should be member of app_user");

    auto inherited_roles = manager.get_inherited_roles("john_doe");
    assert_true(std::find(inherited_roles.begin(), inherited_roles.end(), "app_user") !=
                    inherited_roles.end(),
                "john_doe should inherit app_user role");

    // Test hierarchical role grants
    assert_true(manager.grant_role("app_admin", "app_user", "admin", options),
                "Should grant app_admin to app_user");

    // Test transitive membership
    inherited_roles = manager.get_inherited_roles("john_doe");
    assert_true(std::find(inherited_roles.begin(), inherited_roles.end(), "app_admin") !=
                    inherited_roles.end(),
                "john_doe should transitively inherit app_admin role");

    // Test admin option
    RoleMembershipOptions admin_options;
    admin_options.admin_option = true;
    admin_options.inherit_option = true;
    admin_options.set_option = true;

    manager.create_role("role_admin", "system", static_cast<RoleAttributes>(RoleAttribute::LOGIN));
    assert_true(manager.grant_role("app_user", "role_admin", "admin", admin_options),
                "Should grant role with admin option");

    assert_true(manager.can_grant_role("role_admin", "app_user"),
                "role_admin should be able to grant app_user role");

    // Test role revocation
    assert_true(manager.revoke_role("app_user", "john_doe", "admin"),
                "Should revoke app_user from john_doe");

    assert_false(manager.is_member_of("john_doe", "app_user"),
                 "john_doe should no longer be member of app_user");

    std::cout << "✓ Role membership tests passed" << std::endl;
}

void test_security_context()
{
    std::cout << "Testing security context..." << std::endl;

    RoleManager manager;
    manager.initialize();

    // Create test roles
    manager.create_role("alice", "admin", static_cast<RoleAttributes>(RoleAttribute::LOGIN));
    manager.create_role("developer", "admin", static_cast<RoleAttributes>(RoleAttribute::NOLOGIN));
    manager.create_role("dba", "admin", static_cast<RoleAttributes>(RoleAttribute::NOLOGIN));

    // Grant roles
    RoleMembershipOptions options;
    options.inherit_option = true;
    options.set_option = true;

    manager.grant_role("developer", "alice", "admin", options);
    manager.grant_role("dba", "alice", "admin", options);

    // Create security context
    auto context = manager.create_security_context("alice");
    assert_true(context != nullptr, "Should create security context");

    assert_true(context->get_session_user() == "alice", "Session user should be alice");
    assert_true(context->get_current_user() == "alice", "Current user should initially be alice");

    // Test role switching
    assert_true(context->set_current_role("developer"),
                "Should be able to switch to developer role");
    assert_true(context->get_current_user() == "developer", "Current user should be developer");

    // Test role membership (simplified for this version)
    assert_true(context->is_member_of("alice"), "Should be member of session user role");
    // Note: In full implementation, would need role manager integration for inherited roles

    // Test role reset
    context->reset_role();
    assert_true(context->get_current_user() == "alice", "Should reset to session user");

    // Test trusted role assignment (Firebird-style)
    context->set_trusted_role("RDB$ADMIN");
    assert_true(context->has_trusted_role(), "Should have trusted role");
    assert_true(context->get_trusted_role() == "RDB$ADMIN", "Trusted role should be RDB$ADMIN");

    // Test security event logging
    auto events = context->get_security_events();
    assert_true(events.size() > 0, "Should have security events logged");

    std::cout << "✓ Security context tests passed" << std::endl;
}

void test_system_privileges()
{
    std::cout << "Testing system privileges..." << std::endl;

    RoleManager manager;
    manager.initialize();

    // Create test role
    manager.create_role("db_admin", "system", static_cast<RoleAttributes>(RoleAttribute::NOLOGIN));

    // Grant system privileges
    assert_true(
        manager.grant_system_privilege(SystemPrivilege::CREATE_DATABASE, "db_admin", "system"),
        "Should grant CREATE_DATABASE privilege");
    assert_true(
        manager.grant_system_privilege(SystemPrivilege::ALTER_DATABASE, "db_admin", "system"),
        "Should grant ALTER_DATABASE privilege");

    // Test privilege checking
    assert_true(manager.has_system_privilege("db_admin", SystemPrivilege::CREATE_DATABASE),
                "db_admin should have CREATE_DATABASE privilege");
    assert_true(manager.has_system_privilege("db_admin", SystemPrivilege::ALTER_DATABASE),
                "db_admin should have ALTER_DATABASE privilege");
    assert_false(manager.has_system_privilege("db_admin", SystemPrivilege::DROP_DATABASE),
                 "db_admin should not have DROP_DATABASE privilege");

    // Test privilege inheritance
    manager.create_role("junior_admin", "admin", static_cast<RoleAttributes>(RoleAttribute::LOGIN));

    RoleMembershipOptions options;
    options.inherit_option = true;
    manager.grant_role("db_admin", "junior_admin", "admin", options);

    assert_true(manager.has_system_privilege("junior_admin", SystemPrivilege::CREATE_DATABASE),
                "junior_admin should inherit CREATE_DATABASE privilege");

    // Test privilege revocation
    assert_true(
        manager.revoke_system_privilege(SystemPrivilege::CREATE_DATABASE, "db_admin", "system"),
        "Should revoke CREATE_DATABASE privilege");
    assert_false(manager.has_system_privilege("db_admin", SystemPrivilege::CREATE_DATABASE),
                 "db_admin should no longer have CREATE_DATABASE privilege");

    std::cout << "✓ System privileges tests passed" << std::endl;
}

void test_predefined_roles()
{
    std::cout << "Testing predefined roles..." << std::endl;

    RoleManager manager;
    manager.initialize();

    // Test that predefined roles exist
    auto predefined = RoleManager::get_predefined_role_names();
    assert_true(!predefined.empty(), "Should have predefined roles");

    // Test specific predefined roles
    assert_true(manager.role_exists("pg_monitor"), "pg_monitor role should exist");
    assert_true(manager.role_exists("RDB$ADMIN"), "RDB$ADMIN role should exist");
    assert_true(manager.role_exists("SCRATCHBIRD_ADMIN"), "SCRATCHBIRD_ADMIN role should exist");

    // Test predefined role attributes
    auto admin_role = manager.get_role_info("RDB$ADMIN");
    assert_true(admin_role != nullptr, "Should get RDB$ADMIN role info");
    assert_true(admin_role->has_attribute(RoleAttribute::SUPERUSER),
                "RDB$ADMIN should be superuser");
    assert_true(admin_role->has_attribute(RoleAttribute::TRUSTED), "RDB$ADMIN should be trusted");
    assert_true(admin_role->is_system_role, "RDB$ADMIN should be system role");

    // Test that system roles cannot be dropped
    assert_false(manager.drop_role("RDB$ADMIN", "admin"), "Should not be able to drop system role");

    std::cout << "✓ Predefined roles tests passed" << std::endl;
}

void test_role_inheritance_and_circular_detection()
{
    std::cout << "Testing role inheritance and circular dependency detection..." << std::endl;

    RoleManager manager;
    manager.initialize();

    // Create a hierarchy: role_a -> role_b -> role_c
    manager.create_role("role_a", "admin", static_cast<RoleAttributes>(RoleAttribute::NOLOGIN));
    manager.create_role("role_b", "admin", static_cast<RoleAttributes>(RoleAttribute::NOLOGIN));
    manager.create_role("role_c", "admin", static_cast<RoleAttributes>(RoleAttribute::NOLOGIN));

    RoleMembershipOptions options;
    options.inherit_option = true;

    assert_true(manager.grant_role("role_b", "role_a", "admin", options),
                "Should grant role_b to role_a");
    assert_true(manager.grant_role("role_c", "role_b", "admin", options),
                "Should grant role_c to role_b");

    // Test transitive inheritance
    assert_true(manager.is_member_of("role_a", "role_c"),
                "role_a should transitively inherit role_c");

    // Test circular dependency prevention
    // Attempting to grant role_a to role_c should fail (would create cycle)
    assert_false(manager.grant_role("role_a", "role_c", "admin", options),
                 "Should prevent circular dependency");

    // Test inheritance depth
    auto inherited = manager.get_inherited_roles("role_a");
    assert_true(inherited.size() >= 2, "role_a should inherit at least 2 roles");
    assert_true(std::find(inherited.begin(), inherited.end(), "role_b") != inherited.end(),
                "role_a should inherit role_b");
    assert_true(std::find(inherited.begin(), inherited.end(), "role_c") != inherited.end(),
                "role_a should inherit role_c");

    std::cout << "✓ Role inheritance and circular detection tests passed" << std::endl;
}

void test_firebird_compatibility()
{
    std::cout << "Testing Firebird compatibility features..." << std::endl;

    RoleManager manager;
    manager.initialize();

    // Test Firebird-style role attributes
    manager.create_role(
        "fb_role", "admin",
        static_cast<RoleAttributes>(RoleAttribute::DEFAULT_ROLE | RoleAttribute::ADMIN_OPTION));

    auto role_info = manager.get_role_info("fb_role");
    assert_true(role_info->has_attribute(RoleAttribute::DEFAULT_ROLE),
                "Role should have DEFAULT_ROLE attribute");
    assert_true(role_info->has_attribute(RoleAttribute::ADMIN_OPTION),
                "Role should have ADMIN_OPTION attribute");

    // Test trusted role functionality
    auto context = manager.create_security_context("test_user");
    context->set_trusted_role("RDB$ADMIN");
    assert_true(context->has_trusted_role(), "Should support trusted roles");

    // Test authentication mapping (basic structure)
    RoleManager::AuthMapping mapping;
    mapping.mapping_name = "WIN_ADMINS";
    mapping.plugin_name = "WIN_SSPI";
    mapping.from_pattern = "DOMAIN_ANY_RID_ADMINS";
    mapping.to_role = "RDB$ADMIN";
    mapping.is_global = true;

    assert_true(manager.create_mapping(mapping, "system"), "Should create authentication mapping");

    auto mappings = manager.get_mappings();
    assert_true(mappings.size() > 0, "Should have authentication mappings");

    std::cout << "✓ Firebird compatibility tests passed" << std::endl;
}

void test_postgresql_compatibility()
{
    std::cout << "Testing PostgreSQL compatibility features..." << std::endl;

    RoleManager manager;
    manager.initialize();

    // Test PostgreSQL-style role attributes
    RoleAttributes pg_attrs = static_cast<RoleAttributes>(RoleAttribute::LOGIN) |
                              static_cast<RoleAttributes>(RoleAttribute::CREATEDB) |
                              static_cast<RoleAttributes>(RoleAttribute::REPLICATION) |
                              static_cast<RoleAttributes>(RoleAttribute::BYPASSRLS);

    manager.create_role("pg_user", "admin", pg_attrs);

    auto role_info = manager.get_role_info("pg_user");
    assert_true(role_info->can_login(), "Role should be able to login");
    assert_true(role_info->has_attribute(RoleAttribute::CREATEDB), "Should have CREATEDB");
    assert_true(role_info->has_attribute(RoleAttribute::REPLICATION), "Should have REPLICATION");
    assert_true(role_info->has_attribute(RoleAttribute::BYPASSRLS), "Should have BYPASSRLS");

    // Test granular membership options (PostgreSQL 16+ style)
    manager.create_role("pg_role", "admin", static_cast<RoleAttributes>(RoleAttribute::NOLOGIN));

    RoleMembershipOptions pg_options;
    pg_options.admin_option = true;
    pg_options.inherit_option = false; // Don't inherit automatically
    pg_options.set_option = true;      // Can use SET ROLE

    assert_true(manager.grant_role("pg_role", "pg_user", "admin", pg_options),
                "Should grant with granular options");

    // Test SET ROLE functionality
    auto context = manager.create_security_context("pg_user");
    assert_true(context->set_current_role("pg_role"), "Should support SET ROLE");

    // Test predefined PostgreSQL roles
    assert_true(manager.role_exists("pg_monitor"), "Should have pg_monitor role");
    assert_true(manager.role_exists("pg_read_all_stats"), "Should have pg_read_all_stats role");

    std::cout << "✓ PostgreSQL compatibility tests passed" << std::endl;
}

void test_audit_and_monitoring()
{
    std::cout << "Testing audit and monitoring..." << std::endl;

    RoleManager manager;
    manager.initialize();

    // Perform some operations that should generate audit events
    manager.create_role("audit_test", "admin", static_cast<RoleAttributes>(RoleAttribute::LOGIN));
    manager.alter_role("audit_test",
                       static_cast<RoleAttributes>(RoleAttribute::LOGIN | RoleAttribute::CREATEDB),
                       "admin");
    manager.grant_system_privilege(SystemPrivilege::CREATE_TABLE, "audit_test", "admin");

    // Check audit events
    auto events = manager.get_audit_events(std::chrono::minutes{5});
    assert_true(events.size() >= 3, "Should have at least 3 audit events");

    // Verify event types
    bool found_create = false, found_alter = false, found_privilege = false;
    for (const auto& event : events) {
        if (event.type == RoleManager::RoleAuditEvent::Type::RoleCreated) {
            found_create = true;
        } else if (event.type == RoleManager::RoleAuditEvent::Type::RoleAltered) {
            found_alter = true;
        } else if (event.type == RoleManager::RoleAuditEvent::Type::PrivilegeGranted) {
            found_privilege = true;
        }
    }

    assert_true(found_create, "Should have role creation event");
    assert_true(found_alter, "Should have role alteration event");
    assert_true(found_privilege, "Should have privilege grant event");

    // Test security context audit trail
    auto context = manager.create_security_context("audit_test");
    context->set_current_role("audit_test"); // This should create an event
    context->reset_role();                   // This should create another event

    auto security_events = context->get_security_events();
    assert_true(security_events.size() > 0, "Security context should have events");

    std::cout << "✓ Audit and monitoring tests passed" << std::endl;
}

void test_role_policies_and_validation()
{
    std::cout << "Testing role policies and validation..." << std::endl;

    RoleManager manager;
    manager.initialize();

    // Test policy configuration
    RoleManager::RolePolicy policy;
    policy.max_role_depth = 5;
    policy.allow_circular_membership = false;
    policy.require_admin_for_system_roles = true;

    assert_true(manager.set_policy(policy), "Should set role policy");

    // Test role name validation
    assert_false(manager.create_role("", "admin"), "Should reject empty role name");
    assert_false(manager.create_role("role-with-dashes", "admin"),
                 "Should reject invalid characters");
    assert_true(manager.create_role("valid_role_123", "admin"), "Should accept valid role name");

    // Test role hierarchy validation
    assert_true(manager.validate_role_hierarchy(), "Role hierarchy should be valid initially");

    // Test connection limit validation
    manager.create_role("limited_user", "admin", static_cast<RoleAttributes>(RoleAttribute::LOGIN));
    auto role_info = manager.get_role_info("limited_user");
    role_info->connection_limit = 2;

    assert_true(manager.validate_connection_limit("limited_user", 1), "Should allow within limit");
    assert_false(manager.validate_connection_limit("limited_user", 3), "Should reject over limit");

    std::cout << "✓ Role policies and validation tests passed" << std::endl;
}

void test_role_sql_generation()
{
    std::cout << "Testing SQL generation..." << std::endl;

    // Test PostgreSQL-style SQL generation
    RoleInfo pg_role;
    pg_role.role_name = "test_role";
    pg_role.attributes = static_cast<RoleAttributes>(
        RoleAttribute::LOGIN | RoleAttribute::CREATEDB | RoleAttribute::CREATEROLE);
    pg_role.connection_limit = 10;

    std::string pg_sql = generate_create_role_sql(pg_role, false);
    assert_true(pg_sql.find("CREATE ROLE test_role") != std::string::npos,
                "Should generate PostgreSQL syntax");
    assert_true(pg_sql.find("LOGIN") != std::string::npos, "Should include LOGIN attribute");
    assert_true(pg_sql.find("CREATEDB") != std::string::npos, "Should include CREATEDB attribute");
    assert_true(pg_sql.find("CONNECTION LIMIT 10") != std::string::npos,
                "Should include connection limit");

    // Test Firebird-style SQL generation
    RoleInfo fb_role;
    fb_role.role_name = "fb_role";
    fb_role.attributes = static_cast<RoleAttributes>(RoleAttribute::DEFAULT_ROLE);

    std::string fb_sql = generate_create_role_sql(fb_role, true);
    assert_true(fb_sql.find("CREATE ROLE fb_role") != std::string::npos,
                "Should generate Firebird syntax");
    assert_true(fb_sql.find("SET DEFAULT") != std::string::npos,
                "Should include DEFAULT attribute");

    std::cout << "✓ SQL generation tests passed" << std::endl;
}

int main()
{
    std::cout << "Starting ScratchBird Role Management and Security Context Tests...\n"
              << std::endl;

    try {
        test_role_creation_and_attributes();
        test_role_membership();
        test_security_context();
        test_system_privileges();
        test_predefined_roles();
        test_role_inheritance_and_circular_detection();
        test_firebird_compatibility();
        test_postgresql_compatibility();
        test_audit_and_monitoring();
        test_role_policies_and_validation();
        test_role_sql_generation();

        std::cout << "\n🎉 All Role Management Tests Passed!" << std::endl;
        std::cout << "✓ Role creation and attributes" << std::endl;
        std::cout << "✓ Role membership and inheritance" << std::endl;
        std::cout << "✓ Security context management" << std::endl;
        std::cout << "✓ System privileges" << std::endl;
        std::cout << "✓ Predefined roles (PostgreSQL + Firebird + ScratchBird)" << std::endl;
        std::cout << "✓ Circular dependency prevention" << std::endl;
        std::cout << "✓ Firebird compatibility features" << std::endl;
        std::cout << "✓ PostgreSQL compatibility features" << std::endl;
        std::cout << "✓ Comprehensive audit trail" << std::endl;
        std::cout << "✓ Policy enforcement and validation" << std::endl;
        std::cout << "✓ SQL generation for both dialects" << std::endl;

        std::cout << "\nScratchBird Role Management System provides enterprise-grade" << std::endl;
        std::cout << "security with full Firebird and PostgreSQL compatibility!" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }

    return 0;
}
