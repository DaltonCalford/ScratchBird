#include "scratchbird/engine/fdw_security.h"
#include "test_db_utils.h"

#include <cassert>
#include <iostream>

using namespace scratchbird::engine;

void test_fdw_credential_manager()
{
    std::cout << "=== Testing FDW Credential Manager ===\n";

    FdwCredentialManager cred_manager;

    // Test credential storage
    FdwCredential credential;
    credential.server_name = "test_server";
    credential.local_username = "test_user";
    credential.remote_username = "remote_user";
    credential.encrypted_password = "encrypted_password_123";
    credential.encryption_key_id = "key_001";
    credential.is_active = true;
    credential.use_count = 0;

    std::string error_msg;
    bool stored = cred_manager.store_credential(credential, error_msg);
    assert(stored && "Should store credential successfully");
    std::cout << "✓ Credential storage test passed\n";

    // Test credential retrieval
    FdwCredential retrieved_credential;
    bool retrieved = cred_manager.retrieve_credential("test_server", "test_user",
                                                      retrieved_credential, error_msg);
    assert(retrieved && "Should retrieve credential successfully");
    assert(retrieved_credential.remote_username == "remote_user" &&
           "Should retrieve correct credential");
    std::cout << "✓ Credential retrieval test passed\n";

    // Test password encryption/decryption
    std::string plain_password = "secret123";
    std::string key_id;
    bool key_generated = cred_manager.generate_encryption_key(key_id, error_msg);
    assert(key_generated && "Should generate encryption key");

    std::string encrypted_password;
    bool encrypted =
        cred_manager.encrypt_password(plain_password, key_id, encrypted_password, error_msg);
    assert(encrypted && "Should encrypt password");

    std::string decrypted_password;
    bool decrypted =
        cred_manager.decrypt_password(encrypted_password, key_id, decrypted_password, error_msg);
    assert(decrypted && "Should decrypt password");
    assert(decrypted_password == plain_password && "Should decrypt to original password");
    std::cout << "✓ Password encryption/decryption test passed\n";

    // Test credential usage tracking
    bool usage_updated =
        cred_manager.update_credential_usage("test_server", "test_user", error_msg);
    assert(usage_updated && "Should update credential usage");
    std::cout << "✓ Credential usage tracking test passed\n";

    // Test credential deletion
    bool deleted = cred_manager.delete_credential("test_server", "test_user", error_msg);
    assert(deleted && "Should delete credential successfully");

    bool not_found = cred_manager.retrieve_credential("test_server", "test_user",
                                                      retrieved_credential, error_msg);
    assert(!not_found && "Should not find deleted credential");
    std::cout << "✓ Credential deletion test passed\n";

    std::cout << "✓ All FDW Credential Manager tests passed\n\n";
}

void test_fdw_permission_manager()
{
    std::cout << "=== Testing FDW Permission Manager ===\n";

    FdwPermissionManager perm_manager;

    // Test permission granting
    std::string error_msg;
    bool granted = perm_manager.grant_permission("SERVER", "test_server", "test_user",
                                                 FdwPermission::Usage | FdwPermission::Select,
                                                 "admin", false, error_msg);
    assert(granted && "Should grant permission successfully");
    std::cout << "✓ Permission granting test passed\n";

    // Test permission checking
    FdwSecurityContext context;
    context.current_user = "test_user";
    context.current_roles = {"public"};
    context.is_superuser = false;

    bool has_usage = perm_manager.check_permission("SERVER", "test_server", context,
                                                   FdwPermission::Usage, error_msg);
    assert(has_usage && "Should have USAGE permission");

    bool has_select = perm_manager.check_permission("SERVER", "test_server", context,
                                                    FdwPermission::Select, error_msg);
    assert(has_select && "Should have SELECT permission");

    bool has_insert = perm_manager.check_permission("SERVER", "test_server", context,
                                                    FdwPermission::Insert, error_msg);
    assert(!has_insert && "Should not have INSERT permission");
    std::cout << "✓ Permission checking test passed\n";

    // Test superuser permissions
    FdwSecurityContext admin_context;
    admin_context.current_user = "admin";
    admin_context.is_superuser = true;

    bool admin_has_all = perm_manager.check_permission("SERVER", "any_server", admin_context,
                                                       FdwPermission::All, error_msg);
    assert(admin_has_all && "Superuser should have all permissions");
    std::cout << "✓ Superuser permission test passed\n";

    // Test row-level security
    FdwRowSecurityPolicy policy;
    policy.policy_name = "user_filter";
    policy.table_name = "user_data";
    policy.command_type = "SELECT";
    policy.using_expression = "user_id = current_user_id()";
    policy.is_permissive = true;
    policy.is_enabled = true;
    policy.roles = {"public"};

    bool policy_created = perm_manager.create_row_security_policy(policy, error_msg);
    assert(policy_created && "Should create row security policy");

    bool rls_enabled = perm_manager.enable_row_security("user_data", error_msg);
    assert(rls_enabled && "Should enable row-level security");

    std::string filter_expression;
    bool filter_retrieved = perm_manager.get_row_security_filter("user_data", "SELECT", context,
                                                                 filter_expression, error_msg);
    assert(filter_retrieved && "Should retrieve row security filter");
    assert(!filter_expression.empty() && "Should have non-empty filter expression");
    std::cout << "✓ Row-level security test passed\n";

    // Test permission revocation
    bool revoked = perm_manager.revoke_permission("SERVER", "test_server", "test_user",
                                                  FdwPermission::Usage, "admin", error_msg);
    assert(revoked && "Should revoke permission successfully");

    bool no_usage_after_revoke = perm_manager.check_permission("SERVER", "test_server", context,
                                                               FdwPermission::Usage, error_msg);
    assert(!no_usage_after_revoke && "Should not have USAGE permission after revocation");
    std::cout << "✓ Permission revocation test passed\n";

    std::cout << "✓ All FDW Permission Manager tests passed\n\n";
}

void test_fdw_audit_logger()
{
    std::cout << "=== Testing FDW Audit Logger ===\n";

    FdwAuditLogger audit_logger;

    // Test connection audit logging
    bool conn_logged =
        audit_logger.log_connection_attempt("test_server", "test_user", "192.168.1.100", true, "");
    assert(conn_logged && "Should log connection attempt");
    std::cout << "✓ Connection audit logging test passed\n";

    // Test query audit logging
    bool query_logged = audit_logger.log_query_execution("test_server", "test_user",
                                                         "SELECT * FROM test_table", true, 5, "");
    assert(query_logged && "Should log query execution");
    std::cout << "✓ Query audit logging test passed\n";

    // Test audit level configuration
    std::string error_msg;
    bool level_set = audit_logger.set_audit_level("ERROR", error_msg);
    assert(level_set && "Should set audit level");
    std::cout << "✓ Audit level configuration test passed\n";

    // Test server-specific audit enable/disable
    bool audit_enabled = audit_logger.enable_audit_for_server("test_server", error_msg);
    assert(audit_enabled && "Should enable audit for server");

    bool audit_disabled = audit_logger.disable_audit_for_server("test_server", error_msg);
    assert(audit_disabled && "Should disable audit for server");
    std::cout << "✓ Server-specific audit configuration test passed\n";

    // Test custom audit event logging
    FdwAuditLogger::AuditEvent event;
    event.event_type = FdwAuditLogger::AuditEventType::Configuration;
    event.event_name = "SERVER_CREATED";
    event.user_name = "admin";
    event.server_name = "new_server";
    event.result_status = "SUCCESS";
    event.event_time = std::chrono::duration_cast<std::chrono::seconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();

    bool custom_logged = audit_logger.log_event(event, error_msg);
    assert(custom_logged && "Should log custom audit event");
    std::cout << "✓ Custom audit event logging test passed\n";

    std::cout << "✓ All FDW Audit Logger tests passed\n\n";
}

void test_fdw_security_manager()
{
    std::cout << "=== Testing FDW Security Manager ===\n";

    FdwSecurityManager security_manager;

    // Test security context creation
    FdwSecurityContext context;
    std::string error_msg;
    bool context_created = security_manager.create_security_context(
        "test_user", "session_123", "192.168.1.100", context, error_msg);
    assert(context_created && "Should create security context");
    assert(context.current_user == "test_user" && "Should set correct username");
    assert(!context.is_superuser && "Regular user should not be superuser");
    std::cout << "✓ Security context creation test passed\n";

    // Test admin context creation
    FdwSecurityContext admin_context;
    bool admin_context_created = security_manager.create_security_context(
        "admin", "session_456", "192.168.1.101", admin_context, error_msg);
    assert(admin_context_created && "Should create admin security context");
    assert(admin_context.is_superuser && "Admin should be superuser");
    std::cout << "✓ Admin security context test passed\n";

    // Test security context validation
    bool context_valid = security_manager.validate_security_context(context, error_msg);
    assert(context_valid && "Should validate security context");

    FdwSecurityContext invalid_context;
    bool invalid_context_rejected =
        security_manager.validate_security_context(invalid_context, error_msg);
    assert(!invalid_context_rejected && "Should reject invalid security context");
    std::cout << "✓ Security context validation test passed\n";

    // Test query safety validation
    bool safe_query_valid =
        security_manager.validate_query_safety("SELECT * FROM users WHERE id = 1", error_msg);
    assert(safe_query_valid && "Should validate safe query");

    bool unsafe_query_rejected =
        security_manager.validate_query_safety("SELECT * FROM users; DROP TABLE users;", error_msg);
    assert(!unsafe_query_rejected && "Should reject unsafe query");
    std::cout << "✓ Query safety validation test passed\n";

    // Test identifier sanitization
    std::string sanitized = security_manager.sanitize_identifier("user_table_123");
    assert(sanitized == "user_table_123" && "Should preserve valid identifier");

    std::string dangerous_sanitized =
        security_manager.sanitize_identifier("user'; DROP TABLE users; --");
    assert(dangerous_sanitized == "userDROPTABLEusers" && "Should sanitize dangerous identifier");
    std::cout << "✓ Identifier sanitization test passed\n";

    // Test string literal escaping
    std::string escaped = security_manager.escape_string_literal("O'Reilly");
    assert(escaped == "'O''Reilly'" && "Should escape single quotes");

    std::string normal_escaped = security_manager.escape_string_literal("normal text");
    assert(normal_escaped == "'normal text'" && "Should wrap normal text in quotes");
    std::cout << "✓ String literal escaping test passed\n";

    // Test connection security validation
    bool secure_conn_valid = security_manager.validate_connection_security(
        "host=localhost port=5432 dbname=test sslmode=require", true, error_msg);
    assert(secure_conn_valid && "Should validate secure connection");

    bool insecure_conn_rejected = security_manager.validate_connection_security(
        "host=localhost port=5432 dbname=test sslmode=disable", true, error_msg);
    assert(!insecure_conn_rejected && "Should reject insecure connection when SSL required");
    std::cout << "✓ Connection security validation test passed\n";

    // Test component access
    FdwCredentialManager& cred_mgr = security_manager.get_credential_manager();
    FdwPermissionManager& perm_mgr = security_manager.get_permission_manager();
    FdwAuditLogger& audit_log = security_manager.get_audit_logger();

    assert(&cred_mgr != nullptr && "Should provide credential manager access");
    assert(&perm_mgr != nullptr && "Should provide permission manager access");
    assert(&audit_log != nullptr && "Should provide audit logger access");
    std::cout << "✓ Component access test passed\n";

    std::cout << "✓ All FDW Security Manager tests passed\n\n";
}

void test_fdw_permission_bitwise_operations()
{
    std::cout << "=== Testing FDW Permission Bitwise Operations ===\n";

    // Test individual permissions
    FdwPermission usage = FdwPermission::Usage;
    FdwPermission select = FdwPermission::Select;
    FdwPermission insert = FdwPermission::Insert;

    // Test combining permissions
    FdwPermission read_write = usage | select | insert;
    assert(has_permission(read_write, usage) && "Should have USAGE permission");
    assert(has_permission(read_write, select) && "Should have SELECT permission");
    assert(has_permission(read_write, insert) && "Should have INSERT permission");
    assert(!has_permission(read_write, FdwPermission::Delete) &&
           "Should not have DELETE permission");
    std::cout << "✓ Permission combination test passed\n";

    // Test permission intersection
    FdwPermission intersection = read_write & (usage | FdwPermission::Delete);
    assert(has_permission(intersection, usage) && "Intersection should include USAGE");
    assert(!has_permission(intersection, select) && "Intersection should not include SELECT");
    assert(!has_permission(intersection, FdwPermission::Delete) &&
           "Intersection should not include DELETE");
    std::cout << "✓ Permission intersection test passed\n";

    // Test ALL permissions
    FdwPermission all_perms = FdwPermission::All;
    assert(has_permission(all_perms, usage) && "ALL should include USAGE");
    assert(has_permission(all_perms, select) && "ALL should include SELECT");
    assert(has_permission(all_perms, insert) && "ALL should include INSERT");
    assert(has_permission(all_perms, FdwPermission::Delete) && "ALL should include DELETE");
    std::cout << "✓ ALL permissions test passed\n";

    std::cout << "✓ All FDW Permission bitwise operation tests passed\n\n";
}

int main()
{
    std::cout << "=== FDW Security Tests ===\n\n";

    try {
        test_fdw_permission_bitwise_operations();
        test_fdw_credential_manager();
        test_fdw_permission_manager();
        test_fdw_audit_logger();
        test_fdw_security_manager();

        std::cout << "=== All FDW Security Tests Completed Successfully ===\n";
    } catch (const std::exception& e) {
        std::cout << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
