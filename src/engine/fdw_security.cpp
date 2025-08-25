#include "scratchbird/engine/fdw_security.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>

namespace scratchbird::engine
{

    //=============================================================================
    // FdwCredentialManager Implementation
    //=============================================================================

    class FdwCredentialManager::Impl
    {
      public:
        std::unordered_map<std::string, FdwCredential> credentials_;
        std::unordered_map<std::string, std::string> encryption_keys_;
        std::string master_key_;

        Impl()
        {
            // Initialize with a simple master key for demonstration
            master_key_ = "scratchbird_fdw_master_key_2024";
        }

        std::string simple_encrypt(const std::string& plaintext, const std::string& key)
        {
            // Simple XOR encryption for demonstration (not production-ready)
            std::string encrypted = plaintext;
            for (size_t i = 0; i < encrypted.size(); ++i) {
                encrypted[i] ^= key[i % key.size()];
            }
            return encrypted;
        }

        std::string simple_decrypt(const std::string& ciphertext, const std::string& key)
        {
            // XOR decryption (same as encryption for XOR)
            return simple_encrypt(ciphertext, key);
        }

        std::string generate_key_id()
        {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1000, 9999);
            return "key_" + std::to_string(dis(gen));
        }

        std::string make_credential_key(const std::string& server_name, const std::string& username)
        {
            return server_name + ":" + username;
        }
    };

    FdwCredentialManager::FdwCredentialManager() : pImpl_(std::make_unique<Impl>()) {}

    FdwCredentialManager::~FdwCredentialManager() = default;

    bool FdwCredentialManager::store_credential(const FdwCredential& credential,
                                                std::string& error_msg)
    {
        try {
            std::string key =
                pImpl_->make_credential_key(credential.server_name, credential.local_username);
            pImpl_->credentials_[key] = credential;

            std::cout << "✓ Stored credential for user '" << credential.local_username
                      << "' on server '" << credential.server_name << "'" << std::endl;
            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to store credential: " + std::string(e.what());
            return false;
        }
    }

    bool FdwCredentialManager::retrieve_credential(const std::string& server_name,
                                                   const std::string& local_username,
                                                   FdwCredential& credential,
                                                   std::string& error_msg)
    {
        try {
            std::string key = pImpl_->make_credential_key(server_name, local_username);
            auto it = pImpl_->credentials_.find(key);
            if (it != pImpl_->credentials_.end()) {
                credential = it->second;
                return true;
            } else {
                error_msg = "Credential not found for user '" + local_username + "' on server '" +
                            server_name + "'";
                return false;
            }
        } catch (const std::exception& e) {
            error_msg = "Failed to retrieve credential: " + std::string(e.what());
            return false;
        }
    }

    bool FdwCredentialManager::update_credential(const FdwCredential& credential,
                                                 std::string& error_msg)
    {
        return store_credential(credential, error_msg);
    }

    bool FdwCredentialManager::delete_credential(const std::string& server_name,
                                                 const std::string& local_username,
                                                 std::string& error_msg)
    {
        try {
            std::string key = pImpl_->make_credential_key(server_name, local_username);
            auto it = pImpl_->credentials_.find(key);
            if (it != pImpl_->credentials_.end()) {
                pImpl_->credentials_.erase(it);
                std::cout << "✓ Deleted credential for user '" << local_username << "' on server '"
                          << server_name << "'" << std::endl;
                return true;
            } else {
                error_msg = "Credential not found for deletion";
                return false;
            }
        } catch (const std::exception& e) {
            error_msg = "Failed to delete credential: " + std::string(e.what());
            return false;
        }
    }

    bool FdwCredentialManager::list_credentials(const std::string& server_name,
                                                std::vector<FdwCredential>& credentials,
                                                std::string& error_msg)
    {
        try {
            credentials.clear();
            for (const auto& pair : pImpl_->credentials_) {
                if (pair.second.server_name == server_name) {
                    credentials.push_back(pair.second);
                }
            }
            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to list credentials: " + std::string(e.what());
            return false;
        }
    }

    bool FdwCredentialManager::encrypt_password(const std::string& plain_password,
                                                const std::string& key_id,
                                                std::string& encrypted_password,
                                                std::string& error_msg)
    {
        try {
            auto key_it = pImpl_->encryption_keys_.find(key_id);
            std::string encryption_key =
                (key_it != pImpl_->encryption_keys_.end()) ? key_it->second : pImpl_->master_key_;

            encrypted_password = pImpl_->simple_encrypt(plain_password, encryption_key);
            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to encrypt password: " + std::string(e.what());
            return false;
        }
    }

    bool FdwCredentialManager::decrypt_password(const std::string& encrypted_password,
                                                const std::string& key_id,
                                                std::string& plain_password, std::string& error_msg)
    {
        try {
            auto key_it = pImpl_->encryption_keys_.find(key_id);
            std::string encryption_key =
                (key_it != pImpl_->encryption_keys_.end()) ? key_it->second : pImpl_->master_key_;

            plain_password = pImpl_->simple_decrypt(encrypted_password, encryption_key);
            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to decrypt password: " + std::string(e.what());
            return false;
        }
    }

    bool FdwCredentialManager::generate_encryption_key(std::string& key_id, std::string& error_msg)
    {
        try {
            key_id = pImpl_->generate_key_id();

            // Generate a random key
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 255);

            std::string key;
            for (int i = 0; i < 32; ++i) {
                key += static_cast<char>(dis(gen));
            }

            pImpl_->encryption_keys_[key_id] = key;
            std::cout << "✓ Generated encryption key: " << key_id << std::endl;
            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to generate encryption key: " + std::string(e.what());
            return false;
        }
    }

    bool FdwCredentialManager::rotate_encryption_key(const std::string& old_key_id,
                                                     std::string& new_key_id,
                                                     std::string& error_msg)
    {
        try {
            // Generate new key
            if (!generate_encryption_key(new_key_id, error_msg)) {
                return false;
            }

            // Re-encrypt all credentials using the old key with the new key
            for (auto& pair : pImpl_->credentials_) {
                if (pair.second.encryption_key_id == old_key_id) {
                    // Decrypt with old key
                    std::string plain_password;
                    if (!decrypt_password(pair.second.encrypted_password, old_key_id,
                                          plain_password, error_msg)) {
                        return false;
                    }

                    // Encrypt with new key
                    std::string new_encrypted_password;
                    if (!encrypt_password(plain_password, new_key_id, new_encrypted_password,
                                          error_msg)) {
                        return false;
                    }

                    pair.second.encrypted_password = new_encrypted_password;
                    pair.second.encryption_key_id = new_key_id;
                }
            }

            // Remove old key
            pImpl_->encryption_keys_.erase(old_key_id);
            std::cout << "✓ Rotated encryption key from " << old_key_id << " to " << new_key_id
                      << std::endl;
            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to rotate encryption key: " + std::string(e.what());
            return false;
        }
    }

    bool FdwCredentialManager::update_credential_usage(const std::string& server_name,
                                                       const std::string& local_username,
                                                       std::string& error_msg)
    {
        try {
            std::string key = pImpl_->make_credential_key(server_name, local_username);
            auto it = pImpl_->credentials_.find(key);
            if (it != pImpl_->credentials_.end()) {
                it->second.use_count++;
                it->second.last_used_time = std::chrono::duration_cast<std::chrono::seconds>(
                                                std::chrono::system_clock::now().time_since_epoch())
                                                .count();
                return true;
            } else {
                error_msg = "Credential not found for usage update";
                return false;
            }
        } catch (const std::exception& e) {
            error_msg = "Failed to update credential usage: " + std::string(e.what());
            return false;
        }
    }

    //=============================================================================
    // FdwPermissionManager Implementation
    //=============================================================================

    class FdwPermissionManager::Impl
    {
      public:
        std::vector<FdwPermissionGrant> permission_grants_;
        std::vector<FdwRowSecurityPolicy> row_security_policies_;
        std::unordered_set<std::string> row_security_enabled_tables_;

        bool has_role_permission(const std::string& object_type, const std::string& object_name,
                                 const FdwSecurityContext& context,
                                 FdwPermission required_permission)
        {
            // Check direct user permissions
            for (const auto& grant : permission_grants_) {
                if (grant.object_type == object_type && grant.object_name == object_name &&
                    grant.grantee == context.current_user) {
                    if (has_permission(grant.permissions, required_permission)) {
                        return true;
                    }
                }
            }

            // Check role-based permissions
            for (const auto& role : context.current_roles) {
                for (const auto& grant : permission_grants_) {
                    if (grant.object_type == object_type && grant.object_name == object_name &&
                        grant.grantee == role) {
                        if (has_permission(grant.permissions, required_permission)) {
                            return true;
                        }
                    }
                }
            }

            return false;
        }
    };

    FdwPermissionManager::FdwPermissionManager() : pImpl_(std::make_unique<Impl>()) {}

    FdwPermissionManager::~FdwPermissionManager() = default;

    bool FdwPermissionManager::grant_permission(const std::string& object_type,
                                                const std::string& object_name,
                                                const std::string& grantee,
                                                FdwPermission permissions,
                                                const std::string& grantor, bool with_grant_option,
                                                std::string& error_msg)
    {
        try {
            FdwPermissionGrant grant;
            grant.object_type = object_type;
            grant.object_name = object_name;
            grant.grantee = grantee;
            grant.permissions = permissions;
            grant.grantor = grantor;
            grant.with_grant_option = with_grant_option;
            grant.grant_time = std::chrono::duration_cast<std::chrono::seconds>(
                                   std::chrono::system_clock::now().time_since_epoch())
                                   .count();

            pImpl_->permission_grants_.push_back(grant);

            std::cout << "✓ Granted permission on " << object_type << " '" << object_name
                      << "' to '" << grantee << "'" << std::endl;
            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to grant permission: " + std::string(e.what());
            return false;
        }
    }

    bool FdwPermissionManager::revoke_permission(const std::string& object_type,
                                                 const std::string& object_name,
                                                 const std::string& grantee,
                                                 FdwPermission permissions,
                                                 const std::string& grantor, std::string& error_msg)
    {
        try {
            auto it =
                std::remove_if(pImpl_->permission_grants_.begin(), pImpl_->permission_grants_.end(),
                               [&](const FdwPermissionGrant& grant) {
                                   return grant.object_type == object_type &&
                                          grant.object_name == object_name &&
                                          grant.grantee == grantee &&
                                          (grant.permissions & permissions) == permissions;
                               });

            if (it != pImpl_->permission_grants_.end()) {
                pImpl_->permission_grants_.erase(it, pImpl_->permission_grants_.end());
                std::cout << "✓ Revoked permission on " << object_type << " '" << object_name
                          << "' from '" << grantee << "'" << std::endl;
                return true;
            } else {
                error_msg = "Permission grant not found for revocation";
                return false;
            }
        } catch (const std::exception& e) {
            error_msg = "Failed to revoke permission: " + std::string(e.what());
            return false;
        }
    }

    bool FdwPermissionManager::check_permission(const std::string& object_type,
                                                const std::string& object_name,
                                                const FdwSecurityContext& context,
                                                FdwPermission required_permission,
                                                std::string& error_msg)
    {
        try {
            // Superuser has all permissions
            if (context.is_superuser) {
                return true;
            }

            // Check if user/roles have the required permission
            if (pImpl_->has_role_permission(object_type, object_name, context,
                                            required_permission)) {
                return true;
            }

            error_msg = "Permission denied for " + object_type + " '" + object_name +
                        "' to user '" + context.current_user + "'";
            return false;
        } catch (const std::exception& e) {
            error_msg = "Failed to check permission: " + std::string(e.what());
            return false;
        }
    }

    bool FdwPermissionManager::create_row_security_policy(const FdwRowSecurityPolicy& policy,
                                                          std::string& error_msg)
    {
        try {
            pImpl_->row_security_policies_.push_back(policy);
            std::cout << "✓ Created row security policy '" << policy.policy_name << "' on table '"
                      << policy.table_name << "'" << std::endl;
            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to create row security policy: " + std::string(e.what());
            return false;
        }
    }

    bool FdwPermissionManager::drop_row_security_policy(const std::string& policy_name,
                                                        const std::string& table_name,
                                                        std::string& error_msg)
    {
        try {
            auto it = std::remove_if(
                pImpl_->row_security_policies_.begin(), pImpl_->row_security_policies_.end(),
                [&](const FdwRowSecurityPolicy& policy) {
                    return policy.policy_name == policy_name && policy.table_name == table_name;
                });

            if (it != pImpl_->row_security_policies_.end()) {
                pImpl_->row_security_policies_.erase(it, pImpl_->row_security_policies_.end());
                std::cout << "✓ Dropped row security policy '" << policy_name << "' from table '"
                          << table_name << "'" << std::endl;
                return true;
            } else {
                error_msg = "Row security policy not found";
                return false;
            }
        } catch (const std::exception& e) {
            error_msg = "Failed to drop row security policy: " + std::string(e.what());
            return false;
        }
    }

    bool FdwPermissionManager::enable_row_security(const std::string& table_name,
                                                   std::string& error_msg)
    {
        try {
            pImpl_->row_security_enabled_tables_.insert(table_name);
            std::cout << "✓ Enabled row security on table '" << table_name << "'" << std::endl;
            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to enable row security: " + std::string(e.what());
            return false;
        }
    }

    bool FdwPermissionManager::disable_row_security(const std::string& table_name,
                                                    std::string& error_msg)
    {
        try {
            pImpl_->row_security_enabled_tables_.erase(table_name);
            std::cout << "✓ Disabled row security on table '" << table_name << "'" << std::endl;
            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to disable row security: " + std::string(e.what());
            return false;
        }
    }

    bool FdwPermissionManager::get_row_security_filter(const std::string& table_name,
                                                       const std::string& command_type,
                                                       const FdwSecurityContext& context,
                                                       std::string& filter_expression,
                                                       std::string& error_msg)
    {
        try {
            // Check if row security is enabled for the table
            if (pImpl_->row_security_enabled_tables_.find(table_name) ==
                pImpl_->row_security_enabled_tables_.end()) {
                filter_expression.clear();
                return true;
            }

            // Build filter expression from applicable policies
            std::vector<std::string> policy_expressions;
            for (const auto& policy : pImpl_->row_security_policies_) {
                if (policy.table_name == table_name && policy.is_enabled &&
                    (policy.command_type == "ALL" || policy.command_type == command_type)) {

                    // Check if policy applies to current user/roles
                    bool policy_applies = false;
                    if (policy.roles.empty()) {
                        policy_applies = true; // Policy applies to all users
                    } else {
                        // Check if current user or any of their roles matches policy roles
                        if (std::find(policy.roles.begin(), policy.roles.end(),
                                      context.current_user) != policy.roles.end()) {
                            policy_applies = true;
                        } else {
                            // Check if any of the user's roles match policy roles
                            for (const auto& user_role : context.current_roles) {
                                if (std::find(policy.roles.begin(), policy.roles.end(),
                                              user_role) != policy.roles.end()) {
                                    policy_applies = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (policy_applies && !policy.using_expression.empty()) {
                        policy_expressions.push_back("(" + policy.using_expression + ")");
                    }
                }
            }

            // Combine policy expressions
            if (!policy_expressions.empty()) {
                filter_expression = policy_expressions[0];
                for (size_t i = 1; i < policy_expressions.size(); ++i) {
                    filter_expression += " AND " + policy_expressions[i];
                }
            } else {
                filter_expression.clear();
            }

            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to get row security filter: " + std::string(e.what());
            return false;
        }
    }

    bool FdwPermissionManager::list_permissions(const std::string& object_type,
                                                const std::string& object_name,
                                                std::vector<FdwPermissionGrant>& grants,
                                                std::string& error_msg)
    {
        try {
            grants.clear();
            for (const auto& grant : pImpl_->permission_grants_) {
                if (grant.object_type == object_type && grant.object_name == object_name) {
                    grants.push_back(grant);
                }
            }
            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to list permissions: " + std::string(e.what());
            return false;
        }
    }

    bool FdwPermissionManager::get_user_permissions(const std::string& username,
                                                    std::vector<FdwPermissionGrant>& grants,
                                                    std::string& error_msg)
    {
        try {
            grants.clear();
            for (const auto& grant : pImpl_->permission_grants_) {
                if (grant.grantee == username) {
                    grants.push_back(grant);
                }
            }
            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to get user permissions: " + std::string(e.what());
            return false;
        }
    }

    //=============================================================================
    // FdwAuditLogger Implementation
    //=============================================================================

    class FdwAuditLogger::Impl
    {
      public:
        std::vector<AuditEvent> audit_log_;
        std::unordered_set<std::string> audited_servers_;
        std::string audit_level_ = "INFO";

        bool should_audit_event(const AuditEvent& event)
        {
            if (audit_level_ == "OFF")
                return false;
            if (audit_level_ == "ERROR" && event.result_status == "SUCCESS")
                return false;
            return true;
        }
    };

    FdwAuditLogger::FdwAuditLogger() : pImpl_(std::make_unique<Impl>()) {}

    FdwAuditLogger::~FdwAuditLogger() = default;

    bool FdwAuditLogger::log_event(const AuditEvent& event, std::string& error_msg)
    {
        try {
            if (!pImpl_->should_audit_event(event)) {
                return true;
            }

            pImpl_->audit_log_.push_back(event);

            // Simple console logging for demonstration
            std::cout << "[AUDIT] " << static_cast<int>(event.event_type) << " " << event.event_name
                      << " user=" << event.user_name << " server=" << event.server_name
                      << " status=" << event.result_status;
            if (!event.error_message.empty()) {
                std::cout << " error=" << event.error_message;
            }
            std::cout << std::endl;

            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to log audit event: " + std::string(e.what());
            return false;
        }
    }

    bool FdwAuditLogger::log_connection_attempt(const std::string& server_name,
                                                const std::string& username,
                                                const std::string& client_ip, bool success,
                                                const std::string& error_msg_if_failed)
    {
        AuditEvent event;
        event.event_type = AuditEventType::Connection;
        event.event_name = "CONNECTION_ATTEMPT";
        event.user_name = username;
        event.client_ip = client_ip;
        event.server_name = server_name;
        event.result_status = success ? "SUCCESS" : "FAILURE";
        event.error_message = error_msg_if_failed;
        event.event_time = std::chrono::duration_cast<std::chrono::seconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();

        std::string dummy_error;
        return log_event(event, dummy_error);
    }

    bool FdwAuditLogger::log_query_execution(const std::string& server_name,
                                             const std::string& username, const std::string& query,
                                             bool success, std::uint64_t rows_affected,
                                             const std::string& error_msg_if_failed)
    {
        AuditEvent event;
        event.event_type = AuditEventType::Query;
        event.event_name = "QUERY_EXECUTION";
        event.user_name = username;
        event.server_name = server_name;
        event.query_text = query;
        event.result_status = success ? "SUCCESS" : "FAILURE";
        event.error_message = error_msg_if_failed;
        event.additional_data["rows_affected"] = std::to_string(rows_affected);
        event.event_time = std::chrono::duration_cast<std::chrono::seconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();

        std::string dummy_error;
        return log_event(event, dummy_error);
    }

    bool FdwAuditLogger::set_audit_level(const std::string& level, std::string& error_msg)
    {
        if (level == "OFF" || level == "ERROR" || level == "INFO" || level == "DEBUG") {
            pImpl_->audit_level_ = level;
            std::cout << "✓ Set audit level to: " << level << std::endl;
            return true;
        } else {
            error_msg = "Invalid audit level. Valid levels: OFF, ERROR, INFO, DEBUG";
            return false;
        }
    }

    bool FdwAuditLogger::enable_audit_for_server(const std::string& server_name,
                                                 std::string& error_msg)
    {
        pImpl_->audited_servers_.insert(server_name);
        std::cout << "✓ Enabled audit for server: " << server_name << std::endl;
        return true;
    }

    bool FdwAuditLogger::disable_audit_for_server(const std::string& server_name,
                                                  std::string& error_msg)
    {
        pImpl_->audited_servers_.erase(server_name);
        std::cout << "✓ Disabled audit for server: " << server_name << std::endl;
        return true;
    }

    //=============================================================================
    // FdwSecurityManager Implementation
    //=============================================================================

    FdwSecurityManager::FdwSecurityManager()
        : credential_manager_(std::make_unique<FdwCredentialManager>()),
          permission_manager_(std::make_unique<FdwPermissionManager>()),
          audit_logger_(std::make_unique<FdwAuditLogger>())
    {
    }

    FdwSecurityManager::~FdwSecurityManager() = default;

    bool FdwSecurityManager::create_security_context(const std::string& username,
                                                     const std::string& session_id,
                                                     const std::string& client_ip,
                                                     FdwSecurityContext& context,
                                                     std::string& error_msg)
    {
        try {
            context.current_user = username;
            context.session_id = session_id;
            context.client_ip = client_ip;
            context.session_start_time = std::chrono::duration_cast<std::chrono::seconds>(
                                             std::chrono::system_clock::now().time_since_epoch())
                                             .count();

            // Mock role assignment - in real system would query user roles
            context.current_roles = {"public"};
            if (username == "admin" || username == "postgres") {
                context.current_roles.insert("admin");
                context.is_superuser = true;
            } else {
                context.is_superuser = false;
            }

            return true;
        } catch (const std::exception& e) {
            error_msg = "Failed to create security context: " + std::string(e.what());
            return false;
        }
    }

    bool FdwSecurityManager::validate_security_context(const FdwSecurityContext& context,
                                                       std::string& error_msg)
    {
        if (context.current_user.empty()) {
            error_msg = "Security context missing username";
            return false;
        }

        if (context.session_id.empty()) {
            error_msg = "Security context missing session ID";
            return false;
        }

        return true;
    }

    bool FdwSecurityManager::authorize_server_access(const std::string& server_name,
                                                     const FdwSecurityContext& context,
                                                     std::string& error_msg)
    {
        return permission_manager_->check_permission("SERVER", server_name, context,
                                                     FdwPermission::Usage, error_msg);
    }

    bool FdwSecurityManager::authorize_table_operation(const std::string& table_name,
                                                       const std::string& operation,
                                                       const FdwSecurityContext& context,
                                                       std::string& error_msg)
    {
        FdwPermission required_perm = FdwPermission::None;
        if (operation == "SELECT")
            required_perm = FdwPermission::Select;
        else if (operation == "INSERT")
            required_perm = FdwPermission::Insert;
        else if (operation == "UPDATE")
            required_perm = FdwPermission::Update;
        else if (operation == "DELETE")
            required_perm = FdwPermission::Delete;
        else {
            error_msg = "Unknown operation: " + operation;
            return false;
        }

        return permission_manager_->check_permission("FOREIGN_TABLE", table_name, context,
                                                     required_perm, error_msg);
    }

    bool FdwSecurityManager::get_user_credentials(const std::string& server_name,
                                                  const FdwSecurityContext& context,
                                                  FdwCredential& credential, std::string& error_msg)
    {
        return credential_manager_->retrieve_credential(server_name, context.current_user,
                                                        credential, error_msg);
    }

    bool FdwSecurityManager::validate_query_safety(const std::string& query, std::string& error_msg)
    {
        // Basic SQL injection checks
        std::string query_upper = query;
        std::transform(query_upper.begin(), query_upper.end(), query_upper.begin(), ::toupper);

        // Check for dangerous patterns
        std::vector<std::string> dangerous_patterns = {
            "; DROP ",   "'; DROP ",   "; DELETE ",    "'; DELETE ", "; UPDATE ", "'; UPDATE ",
            "; INSERT ", "'; INSERT ", "UNION SELECT", "/*",         "*/"};

        for (const auto& pattern : dangerous_patterns) {
            if (query_upper.find(pattern) != std::string::npos) {
                error_msg = "Query contains potentially dangerous pattern: " + pattern;
                return false;
            }
        }

        return true;
    }

    std::string FdwSecurityManager::sanitize_identifier(const std::string& identifier)
    {
        // Remove any non-alphanumeric characters except underscores
        std::string sanitized;
        for (char c : identifier) {
            if (std::isalnum(c) || c == '_') {
                sanitized += c;
            }
        }
        return sanitized;
    }

    std::string FdwSecurityManager::escape_string_literal(const std::string& literal)
    {
        std::string escaped;
        for (char c : literal) {
            if (c == '\'') {
                escaped += "''";
            } else {
                escaped += c;
            }
        }
        return "'" + escaped + "'";
    }

    bool FdwSecurityManager::validate_connection_security(const std::string& connection_string,
                                                          bool require_ssl, std::string& error_msg)
    {
        if (require_ssl) {
            if (connection_string.find("sslmode=") == std::string::npos ||
                connection_string.find("sslmode=disable") != std::string::npos) {
                error_msg = "SSL is required but connection string does not specify SSL mode";
                return false;
            }
        }

        // Check for plain text passwords in connection string
        if (connection_string.find("password=") != std::string::npos) {
            error_msg = "Plain text passwords in connection strings are not recommended";
            // Return true but log warning
        }

        return true;
    }

    FdwCredentialManager& FdwSecurityManager::get_credential_manager()
    {
        return *credential_manager_;
    }

    FdwPermissionManager& FdwSecurityManager::get_permission_manager()
    {
        return *permission_manager_;
    }

    FdwAuditLogger& FdwSecurityManager::get_audit_logger()
    {
        return *audit_logger_;
    }

} // namespace scratchbird::engine
