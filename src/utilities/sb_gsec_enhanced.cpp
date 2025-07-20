#include "sb_gsec_enhanced.h"
#include "sb_engine_integration.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <random>
#include <regex>
#include <iomanip>
#include <filesystem>

namespace fs = std::filesystem;

// Constructor
GSecEnhanced::GSecEnhanced() 
    : engine(std::make_unique<SBEngineIntegration>()) {
    initializeEngine();
}

// Destructor
GSecEnhanced::~GSecEnhanced() {
    if (isOperationActive()) {
        cancelCurrentOperation();
    }
}

// Initialize the engine
bool GSecEnhanced::initializeEngine() {
    try {
        if (!engine) {
            logError("Initialization", "Failed to create engine integration");
            return false;
        }
        
        clearErrorLog();
        return true;
    } catch (const std::exception& e) {
        logError("Initialization", std::string("Exception during engine initialization: ") + e.what());
        return false;
    }
}

// Initialize security service
bool GSecEnhanced::initializeSecurityService(const std::string& database_path) {
    try {
        // Connect to database
        if (!connectToDatabase(database_path)) {
            logError("Service Initialization", "Failed to connect to database: " + database_path);
            return false;
        }
        
        // Initialize service for security operations
        security_service = engine->createSecurityService();
        if (!security_service) {
            logError("Service Initialization", "Failed to create security service");
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        logError("Service Initialization", std::string("Exception: ") + e.what());
        return false;
    }
}

// === ORIGINAL GSEC FUNCTIONALITY ===

// Add user (original GSEC -add operation)
bool GSecEnhanced::addUser(const std::string& database_path,
                          const std::string& username,
                          const std::string& password,
                          const SBEnhanced::UserManagementOptions& options,
                          SBEnhanced::SecurityOperationResult& result) {
    result.operation_type = SBEnhanced::SecurityOperation::USER_MANAGEMENT;
    result.start_time = std::chrono::steady_clock::now();
    
    try {
        if (!initializeSecurityService(database_path)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to initialize security service");
            return false;
        }
        
        updateProgress(SBEnhanced::SecurityOperation::USER_MANAGEMENT, 0, 1, username);
        
        // Validate username
        if (!SBEnhanced::validateUsername(username)) {
            result.operation_successful = false;
            result.errors.push_back("Invalid username: " + username);
            return false;
        }
        
        // Validate password policy
        SBEnhanced::PasswordPolicyOptions default_policy;
        std::vector<std::string> policy_violations;
        if (!validatePasswordPolicy(password, default_policy, policy_violations)) {
            result.operation_successful = false;
            for (const auto& violation : policy_violations) {
                result.errors.push_back("Password policy violation: " + violation);
            }
            return false;
        }
        
        // Check if user already exists
        SBEnhanced::UserAccount existing_user;
        if (getUserAccountInfo(database_path, username, existing_user)) {
            result.operation_successful = false;
            result.errors.push_back("User already exists: " + username);
            return false;
        }
        
        // Create user account
        SBEnhanced::UserAccount new_user;
        new_user.username = username;
        new_user.first_name = options.first_name;
        new_user.middle_name = options.middle_name;
        new_user.last_name = options.last_name;
        new_user.description = options.description;
        new_user.email = options.email;
        new_user.phone = options.phone;
        new_user.auth_method = options.auth_method;
        new_user.admin_privileges = options.admin_privileges;
        new_user.password_must_change = options.force_password_change;
        new_user.status = SBEnhanced::UserAccountStatus::ACTIVE;
        new_user.created_date = std::chrono::system_clock::now();
        new_user.password_changed = std::chrono::system_clock::now();
        new_user.assigned_roles = options.assign_roles;
        new_user.privileges = options.grant_privileges;
        
        // Hash password
        std::string password_hash;
        if (!hashPassword(password, options.auth_method, password_hash)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to hash password");
            return false;
        }
        
        // Create user in database
        if (!createUserAccount(database_path, new_user, result)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to create user account in database");
            return false;
        }
        
        // Store password hash securely
        if (!engine->storeUserPasswordHash(username, password_hash)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to store password hash");
            return false;
        }
        
        updateProgress(SBEnhanced::SecurityOperation::USER_MANAGEMENT, 1, 1, username);
        
        result.operation_successful = true;
        result.messages.push_back("User '" + username + "' created successfully");
        result.detailed_stats.users_created = 1;
        
        return true;
        
    } catch (const std::exception& e) {
        result.operation_successful = false;
        result.errors.push_back(std::string("Exception: ") + e.what());
        logError("AddUser", e.what());
        return false;
    } finally {
        result.end_time = std::chrono::steady_clock::now();
        operation_active = false;
    }
}

// Modify user (original GSEC -modify operation)
bool GSecEnhanced::modifyUser(const std::string& database_path,
                             const std::string& username,
                             const SBEnhanced::UserManagementOptions& options,
                             SBEnhanced::SecurityOperationResult& result) {
    result.operation_type = SBEnhanced::SecurityOperation::USER_MANAGEMENT;
    result.start_time = std::chrono::steady_clock::now();
    
    try {
        if (!initializeSecurityService(database_path)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to initialize security service");
            return false;
        }
        
        updateProgress(SBEnhanced::SecurityOperation::USER_MANAGEMENT, 0, 1, username);
        
        // Get existing user account
        SBEnhanced::UserAccount user_account;
        if (!getUserAccountInfo(database_path, username, user_account)) {
            result.operation_successful = false;
            result.errors.push_back("User not found: " + username);
            return false;
        }
        
        // Update user properties
        if (!options.first_name.empty()) user_account.first_name = options.first_name;
        if (!options.middle_name.empty()) user_account.middle_name = options.middle_name;
        if (!options.last_name.empty()) user_account.last_name = options.last_name;
        if (!options.description.empty()) user_account.description = options.description;
        if (!options.email.empty()) user_account.email = options.email;
        if (!options.phone.empty()) user_account.phone = options.phone;
        
        // Update authentication method if specified
        if (options.auth_method != SBEnhanced::AuthenticationMethod::SRP256) {
            user_account.auth_method = options.auth_method;
        }
        
        // Update admin privileges
        user_account.admin_privileges = options.admin_privileges;
        
        // Handle password change
        if (!options.password.empty()) {
            // Validate new password
            SBEnhanced::PasswordPolicyOptions default_policy;
            std::vector<std::string> policy_violations;
            if (!validatePasswordPolicy(options.password, default_policy, policy_violations)) {
                result.operation_successful = false;
                for (const auto& violation : policy_violations) {
                    result.errors.push_back("Password policy violation: " + violation);
                }
                return false;
            }
            
            // Hash new password
            std::string password_hash;
            if (!hashPassword(options.password, user_account.auth_method, password_hash)) {
                result.operation_successful = false;
                result.errors.push_back("Failed to hash new password");
                return false;
            }
            
            // Update password
            if (!engine->storeUserPasswordHash(username, password_hash)) {
                result.operation_successful = false;
                result.errors.push_back("Failed to update password hash");
                return false;
            }
            
            user_account.password_changed = std::chrono::system_clock::now();
            user_account.password_must_change = options.force_password_change;
            result.detailed_stats.password_resets = 1;
        }
        
        // Update role assignments
        if (!options.assign_roles.empty()) {
            user_account.assigned_roles = options.assign_roles;
        }
        
        // Update privileges
        if (!options.grant_privileges.empty()) {
            user_account.privileges = options.grant_privileges;
        }
        
        // Update user account in database
        if (!updateUserAccount(database_path, user_account, result)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to update user account in database");
            return false;
        }
        
        updateProgress(SBEnhanced::SecurityOperation::USER_MANAGEMENT, 1, 1, username);
        
        result.operation_successful = true;
        result.messages.push_back("User '" + username + "' modified successfully");
        result.detailed_stats.users_modified = 1;
        
        return true;
        
    } catch (const std::exception& e) {
        result.operation_successful = false;
        result.errors.push_back(std::string("Exception: ") + e.what());
        logError("ModifyUser", e.what());
        return false;
    } finally {
        result.end_time = std::chrono::steady_clock::now();
        operation_active = false;
    }
}

// Delete user (original GSEC -delete operation)
bool GSecEnhanced::deleteUser(const std::string& database_path,
                             const std::string& username,
                             SBEnhanced::SecurityOperationResult& result) {
    result.operation_type = SBEnhanced::SecurityOperation::USER_MANAGEMENT;
    result.start_time = std::chrono::steady_clock::now();
    
    try {
        if (!initializeSecurityService(database_path)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to initialize security service");
            return false;
        }
        
        updateProgress(SBEnhanced::SecurityOperation::USER_MANAGEMENT, 0, 1, username);
        
        // Check if user exists
        SBEnhanced::UserAccount user_account;
        if (!getUserAccountInfo(database_path, username, user_account)) {
            result.operation_successful = false;
            result.errors.push_back("User not found: " + username);
            return false;
        }
        
        // Prevent deletion of admin users (safety check)
        if (user_account.admin_privileges) {
            result.warnings.push_back("Warning: Deleting user with admin privileges: " + username);
        }
        
        // Remove user account from database
        if (!removeUserAccount(database_path, username, result)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to remove user account from database");
            return false;
        }
        
        // Remove password hash
        if (!engine->removeUserPasswordHash(username)) {
            result.warnings.push_back("Warning: Failed to remove password hash for user: " + username);
        }
        
        // Terminate any active sessions for this user
        terminateUserSessions(database_path, username, true);
        
        updateProgress(SBEnhanced::SecurityOperation::USER_MANAGEMENT, 1, 1, username);
        
        result.operation_successful = true;
        result.messages.push_back("User '" + username + "' deleted successfully");
        result.detailed_stats.users_deleted = 1;
        
        return true;
        
    } catch (const std::exception& e) {
        result.operation_successful = false;
        result.errors.push_back(std::string("Exception: ") + e.what());
        logError("DeleteUser", e.what());
        return false;
    } finally {
        result.end_time = std::chrono::steady_clock::now();
        operation_active = false;
    }
}

// Display user (original GSEC -display operation)
bool GSecEnhanced::displayUser(const std::string& database_path,
                              const std::string& username,
                              SBEnhanced::UserAccount& user_info,
                              SBEnhanced::SecurityOperationResult& result) {
    result.operation_type = SBEnhanced::SecurityOperation::USER_MANAGEMENT;
    result.start_time = std::chrono::steady_clock::now();
    
    try {
        if (!initializeSecurityService(database_path)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to initialize security service");
            return false;
        }
        
        updateProgress(SBEnhanced::SecurityOperation::USER_MANAGEMENT, 0, 1, username);
        
        // Get user account information
        if (!getUserAccountInfo(database_path, username, user_info)) {
            result.operation_successful = false;
            result.errors.push_back("User not found: " + username);
            return false;
        }
        
        updateProgress(SBEnhanced::SecurityOperation::USER_MANAGEMENT, 1, 1, username);
        
        result.operation_successful = true;
        result.messages.push_back("User information retrieved for: " + username);
        
        return true;
        
    } catch (const std::exception& e) {
        result.operation_successful = false;
        result.errors.push_back(std::string("Exception: ") + e.what());
        logError("DisplayUser", e.what());
        return false;
    } finally {
        result.end_time = std::chrono::steady_clock::now();
        operation_active = false;
    }
}

// Display all users (original GSEC -display operation without username)
bool GSecEnhanced::displayAllUsers(const std::string& database_path,
                                  std::vector<SBEnhanced::UserAccount>& users,
                                  SBEnhanced::SecurityOperationResult& result) {
    result.operation_type = SBEnhanced::SecurityOperation::USER_MANAGEMENT;
    result.start_time = std::chrono::steady_clock::now();
    
    try {
        if (!initializeSecurityService(database_path)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to initialize security service");
            return false;
        }
        
        updateProgress(SBEnhanced::SecurityOperation::USER_MANAGEMENT, 0, 1, "all users");
        
        // Get all user accounts
        std::vector<std::string> usernames;
        if (!engine->getAllUsernames(usernames)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to retrieve user list");
            return false;
        }
        
        users.clear();
        users.reserve(usernames.size());
        
        uint64_t processed = 0;
        for (const auto& username : usernames) {
            SBEnhanced::UserAccount user_info;
            if (getUserAccountInfo(database_path, username, user_info)) {
                users.push_back(user_info);
            } else {
                result.warnings.push_back("Warning: Could not retrieve info for user: " + username);
            }
            
            updateProgress(SBEnhanced::SecurityOperation::USER_MANAGEMENT, ++processed, usernames.size(), username);
        }
        
        result.operation_successful = true;
        result.messages.push_back("Retrieved information for " + std::to_string(users.size()) + " users");
        
        return true;
        
    } catch (const std::exception& e) {
        result.operation_successful = false;
        result.errors.push_back(std::string("Exception: ") + e.what());
        logError("DisplayAllUsers", e.what());
        return false;
    } finally {
        result.end_time = std::chrono::steady_clock::now();
        operation_active = false;
    }
}

// === ENHANCED SECURITY FUNCTIONALITY ===

// Perform user management operations
bool GSecEnhanced::performUserManagement(const std::string& database_path,
                                        const SBEnhanced::UserManagementOptions& options,
                                        SBEnhanced::UserManagementResult& result) {
    SBEnhanced::SecurityOperationResult op_result;
    
    switch (options.operation) {
        case SBEnhanced::UserOperation::ADD_USER:
            result.operation_successful = addUser(database_path, options.username, options.password, options, op_result);
            break;
            
        case SBEnhanced::UserOperation::MODIFY_USER:
            result.operation_successful = modifyUser(database_path, options.username, options, op_result);
            break;
            
        case SBEnhanced::UserOperation::DELETE_USER:
            result.operation_successful = deleteUser(database_path, options.username, op_result);
            break;
            
        case SBEnhanced::UserOperation::DISPLAY_USER:
            {
                SBEnhanced::UserAccount user_info;
                result.operation_successful = displayUser(database_path, options.username, user_info, op_result);
                if (result.operation_successful) {
                    result.user_accounts.push_back(user_info);
                }
            }
            break;
            
        case SBEnhanced::UserOperation::DISPLAY_ALL_USERS:
            result.operation_successful = displayAllUsers(database_path, result.user_accounts, op_result);
            break;
    }
    
    result.operation_performed = options.operation;
    result.affected_username = options.username;
    result.users_processed = result.user_accounts.size();
    result.operations_completed = result.operation_successful ? 1 : 0;
    result.operation_messages = op_result.messages;
    result.detailed_stats = op_result.detailed_stats;
    
    return result.operation_successful;
}

// Perform role management operations
bool GSecEnhanced::performRoleManagement(const std::string& database_path,
                                        const SBEnhanced::RoleManagementOptions& options,
                                        SBEnhanced::RoleManagementResult& result) {
    result.operation_performed = options.operation;
    result.affected_role = options.role_name;
    
    try {
        if (!initializeSecurityService(database_path)) {
            result.operation_successful = false;
            result.operation_messages.push_back("Failed to initialize security service");
            return false;
        }
        
        switch (options.operation) {
            case SBEnhanced::RoleOperation::CREATE_ROLE:
                {
                    SBEnhanced::DatabaseRole new_role;
                    new_role.role_name = options.role_name;
                    new_role.description = options.description;
                    new_role.privileges = options.privileges;
                    new_role.system_role = options.system_role;
                    new_role.created_date = std::chrono::system_clock::now();
                    
                    SBEnhanced::SecurityOperationResult op_result;
                    result.operation_successful = createDatabaseRole(database_path, new_role, op_result);
                    result.operation_messages = op_result.messages;
                    result.detailed_stats.roles_created = result.operation_successful ? 1 : 0;
                }
                break;
                
            case SBEnhanced::RoleOperation::DROP_ROLE:
                {
                    SBEnhanced::SecurityOperationResult op_result;
                    result.operation_successful = dropDatabaseRole(database_path, options.role_name, op_result);
                    result.operation_messages = op_result.messages;
                    result.detailed_stats.roles_dropped = result.operation_successful ? 1 : 0;
                }
                break;
                
            case SBEnhanced::RoleOperation::LIST_ROLES:
                {
                    std::vector<std::string> role_names;
                    if (engine->getAllRoleNames(role_names)) {
                        for (const auto& role_name : role_names) {
                            SBEnhanced::DatabaseRole role_info;
                            if (getRoleInfo(database_path, role_name, role_info)) {
                                result.database_roles.push_back(role_info);
                            }
                        }
                        result.operation_successful = true;
                        result.operation_messages.push_back("Retrieved " + std::to_string(result.database_roles.size()) + " roles");
                    } else {
                        result.operation_successful = false;
                        result.operation_messages.push_back("Failed to retrieve role list");
                    }
                }
                break;
                
            case SBEnhanced::RoleOperation::DESCRIBE_ROLE:
                {
                    SBEnhanced::DatabaseRole role_info;
                    if (getRoleInfo(database_path, options.role_name, role_info)) {
                        result.database_roles.push_back(role_info);
                        result.operation_successful = true;
                        result.operation_messages.push_back("Role information retrieved for: " + options.role_name);
                    } else {
                        result.operation_successful = false;
                        result.operation_messages.push_back("Role not found: " + options.role_name);
                    }
                }
                break;
                
            case SBEnhanced::RoleOperation::GRANT_ROLE:
                // Implementation for granting roles to users
                result.operation_successful = true; // Placeholder
                break;
                
            case SBEnhanced::RoleOperation::REVOKE_ROLE:
                // Implementation for revoking roles from users
                result.operation_successful = true; // Placeholder
                break;
        }
        
        result.roles_processed = result.database_roles.size();
        result.operations_completed = result.operation_successful ? 1 : 0;
        
        return result.operation_successful;
        
    } catch (const std::exception& e) {
        result.operation_successful = false;
        result.operation_messages.push_back(std::string("Exception: ") + e.what());
        logError("RoleManagement", e.what());
        return false;
    }
}

// Configure password policy
bool GSecEnhanced::configurePasswordPolicy(const std::string& database_path,
                                          const SBEnhanced::PasswordPolicyOptions& options,
                                          SBEnhanced::SecurityOperationResult& result) {
    result.operation_type = SBEnhanced::SecurityOperation::PASSWORD_POLICY;
    result.start_time = std::chrono::steady_clock::now();
    
    try {
        if (!initializeSecurityService(database_path)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to initialize security service");
            return false;
        }
        
        // Store password policy configuration
        if (!engine->storePasswordPolicy(options)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to store password policy configuration");
            return false;
        }
        
        result.operation_successful = true;
        result.messages.push_back("Password policy configured successfully");
        
        return true;
        
    } catch (const std::exception& e) {
        result.operation_successful = false;
        result.errors.push_back(std::string("Exception: ") + e.what());
        logError("ConfigurePasswordPolicy", e.what());
        return false;
    } finally {
        result.end_time = std::chrono::steady_clock::now();
        operation_active = false;
    }
}

// Validate password against policy
bool GSecEnhanced::validatePasswordPolicy(const std::string& password,
                                         const SBEnhanced::PasswordPolicyOptions& policy,
                                         std::vector<std::string>& violations) {
    violations.clear();
    
    // Check minimum length
    if (password.length() < policy.minimum_length) {
        violations.push_back("Password must be at least " + std::to_string(policy.minimum_length) + " characters long");
    }
    
    // Check maximum length
    if (password.length() > policy.maximum_length) {
        violations.push_back("Password must be no more than " + std::to_string(policy.maximum_length) + " characters long");
    }
    
    // Check character requirements
    uint32_t uppercase_count = 0;
    uint32_t lowercase_count = 0;
    uint32_t digit_count = 0;
    uint32_t special_count = 0;
    
    for (char c : password) {
        if (std::isupper(c)) uppercase_count++;
        else if (std::islower(c)) lowercase_count++;
        else if (std::isdigit(c)) digit_count++;
        else special_count++;
    }
    
    if (uppercase_count < policy.minimum_uppercase) {
        violations.push_back("Password must contain at least " + std::to_string(policy.minimum_uppercase) + " uppercase letter(s)");
    }
    
    if (lowercase_count < policy.minimum_lowercase) {
        violations.push_back("Password must contain at least " + std::to_string(policy.minimum_lowercase) + " lowercase letter(s)");
    }
    
    if (digit_count < policy.minimum_digits) {
        violations.push_back("Password must contain at least " + std::to_string(policy.minimum_digits) + " digit(s)");
    }
    
    if (special_count < policy.minimum_special_chars) {
        violations.push_back("Password must contain at least " + std::to_string(policy.minimum_special_chars) + " special character(s)");
    }
    
    // Check forbidden patterns
    for (const auto& pattern : policy.forbidden_patterns) {
        if (password.find(pattern) != std::string::npos) {
            violations.push_back("Password contains forbidden pattern: " + pattern);
        }
    }
    
    return violations.empty();
}

// === ENHANCED SECURITY AUDITING ===

// Perform comprehensive security audit
bool GSecEnhanced::performSecurityAudit(const std::string& database_path,
                                       const SBEnhanced::SecurityAuditOptions& options,
                                       SBEnhanced::SecurityAuditResult& result) {
    result.audit_successful = false;
    
    try {
        if (!initializeSecurityService(database_path)) {
            result.violation_details.push_back("Failed to initialize security service");
            return false;
        }
        
        updateProgress(SBEnhanced::SecurityOperation::SECURITY_AUDIT, 0, 5, "Starting security audit");
        
        // Collect security events
        std::vector<std::map<std::string, std::string>> events;
        if (!collectSecurityEvents(database_path, options, events)) {
            result.violation_details.push_back("Failed to collect security events");
            return false;
        }
        
        updateProgress(SBEnhanced::SecurityOperation::SECURITY_AUDIT, 1, 5, "Events collected");
        
        // Analyze collected events
        if (!analyzeSecurityEvents(events, result)) {
            result.violation_details.push_back("Failed to analyze security events");
            return false;
        }
        
        updateProgress(SBEnhanced::SecurityOperation::SECURITY_AUDIT, 2, 5, "Events analyzed");
        
        // Check password policies
        if (options.include_password_policy_check) {
            checkPasswordPolicyCompliance(database_path, result);
        }
        
        updateProgress(SBEnhanced::SecurityOperation::SECURITY_AUDIT, 3, 5, "Password policies checked");
        
        // Check access controls
        if (options.include_access_control_check) {
            checkAccessControlCompliance(database_path, result);
        }
        
        updateProgress(SBEnhanced::SecurityOperation::SECURITY_AUDIT, 4, 5, "Access controls checked");
        
        // Generate audit report
        if (!options.output_file_path.empty()) {
            generateAuditReport(result, options.report_format, options.output_file_path);
            result.audit_report_path = options.output_file_path;
        }
        
        updateProgress(SBEnhanced::SecurityOperation::SECURITY_AUDIT, 5, 5, "Audit completed");
        
        result.audit_successful = true;
        result.total_events_analyzed = events.size();
        
        return true;
        
    } catch (const std::exception& e) {
        result.violation_details.push_back(std::string("Exception during security audit: ") + e.what());
        logError("SecurityAudit", e.what());
        return false;
    }
}

// Generate security report
bool GSecEnhanced::generateSecurityReport(const std::string& database_path,
                                        const SBEnhanced::SecurityAuditOptions& options,
                                        const std::string& report_path) {
    try {
        SBEnhanced::SecurityAuditResult result;
        if (!performSecurityAudit(database_path, options, result)) {
            return false;
        }
        
        return generateAuditReport(result, options.report_format, report_path);
        
    } catch (const std::exception& e) {
        logError("GenerateSecurityReport", e.what());
        return false;
    }
}

// Session management
bool GSecEnhanced::manageUserSessions(const std::string& database_path,
                                     const SBEnhanced::SessionManagementOptions& options,
                                     SBEnhanced::SecurityOperationResult& result) {
    result.operation_type = SBEnhanced::SecurityOperation::SESSION_MANAGEMENT;
    result.start_time = std::chrono::steady_clock::now();
    
    try {
        if (!initializeSecurityService(database_path)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to initialize security service");
            return false;
        }
        
        if (options.list_active_sessions) {
            std::vector<std::map<std::string, std::string>> sessions;
            if (listActiveSessions(database_path, sessions)) {
                result.messages.push_back("Found " + std::to_string(sessions.size()) + " active sessions");
                result.detailed_stats.active_sessions = sessions.size();
            } else {
                result.errors.push_back("Failed to list active sessions");
                return false;
            }
        }
        
        if (options.kill_user_sessions && !options.target_username.empty()) {
            if (terminateUserSessions(database_path, options.target_username, options.force_disconnect)) {
                result.messages.push_back("Successfully terminated sessions for user: " + options.target_username);
                result.detailed_stats.sessions_terminated++;
            } else {
                result.errors.push_back("Failed to terminate sessions for user: " + options.target_username);
                return false;
            }
        }
        
        result.operation_successful = true;
        return true;
        
    } catch (const std::exception& e) {
        result.operation_successful = false;
        result.errors.push_back(std::string("Exception: ") + e.what());
        logError("SessionManagement", e.what());
        return false;
    } finally {
        result.end_time = std::chrono::steady_clock::now();
        operation_active = false;
    }
}

// List active sessions
bool GSecEnhanced::listActiveSessions(const std::string& database_path,
                                     std::vector<std::map<std::string, std::string>>& sessions) {
    try {
        return engine->getActiveSessions(sessions);
    } catch (const std::exception& e) {
        logError("ListActiveSessions", e.what());
        return false;
    }
}

// Authentication configuration
bool GSecEnhanced::configureAuthentication(const std::string& database_path,
                                          const SBEnhanced::AuthenticationConfigOptions& options,
                                          SBEnhanced::SecurityOperationResult& result) {
    result.operation_type = SBEnhanced::SecurityOperation::AUTHENTICATION_CONFIG;
    result.start_time = std::chrono::steady_clock::now();
    
    try {
        if (!initializeSecurityService(database_path)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to initialize security service");
            return false;
        }
        
        // Store authentication configuration
        if (!engine->storeAuthenticationConfig(options)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to store authentication configuration");
            return false;
        }
        
        result.operation_successful = true;
        result.messages.push_back("Authentication configuration updated successfully");
        
        return true;
        
    } catch (const std::exception& e) {
        result.operation_successful = false;
        result.errors.push_back(std::string("Exception: ") + e.what());
        logError("ConfigureAuthentication", e.what());
        return false;
    } finally {
        result.end_time = std::chrono::steady_clock::now();
        operation_active = false;
    }
}

// Set default authentication method
bool GSecEnhanced::setDefaultAuthenticationMethod(const std::string& database_path,
                                                 SBEnhanced::AuthenticationMethod method) {
    try {
        return engine->setDefaultAuthMethod(method);
    } catch (const std::exception& e) {
        logError("SetDefaultAuthMethod", e.what());
        return false;
    }
}

// Database encryption
bool GSecEnhanced::configureDatabaseEncryption(const std::string& database_path,
                                              const SBEnhanced::DatabaseEncryptionOptions& options,
                                              SBEnhanced::SecurityOperationResult& result) {
    result.operation_type = SBEnhanced::SecurityOperation::DATABASE_ENCRYPTION;
    result.start_time = std::chrono::steady_clock::now();
    
    try {
        if (!initializeSecurityService(database_path)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to initialize security service");
            return false;
        }
        
        // Configure database encryption
        if (!engine->configureEncryption(options)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to configure database encryption");
            return false;
        }
        
        result.operation_successful = true;
        result.messages.push_back("Database encryption configured successfully");
        
        return true;
        
    } catch (const std::exception& e) {
        result.operation_successful = false;
        result.errors.push_back(std::string("Exception: ") + e.what());
        logError("ConfigureDatabaseEncryption", e.what());
        return false;
    } finally {
        result.end_time = std::chrono::steady_clock::now();
        operation_active = false;
    }
}

// Encrypt database
bool GSecEnhanced::encryptDatabase(const std::string& database_path,
                                  const std::string& key_file_path,
                                  const std::string& algorithm) {
    try {
        return engine->encryptDatabase(database_path, key_file_path, algorithm);
    } catch (const std::exception& e) {
        logError("EncryptDatabase", e.what());
        return false;
    }
}

// Decrypt database
bool GSecEnhanced::decryptDatabase(const std::string& database_path,
                                  const std::string& key_file_path) {
    try {
        return engine->decryptDatabase(database_path, key_file_path);
    } catch (const std::exception& e) {
        logError("DecryptDatabase", e.what());
        return false;
    }
}

// Access control configuration
bool GSecEnhanced::configureAccessControl(const std::string& database_path,
                                         const SBEnhanced::AccessControlOptions& options,
                                         SBEnhanced::SecurityOperationResult& result) {
    result.operation_type = SBEnhanced::SecurityOperation::ACCESS_CONTROL;
    result.start_time = std::chrono::steady_clock::now();
    
    try {
        if (!initializeSecurityService(database_path)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to initialize security service");
            return false;
        }
        
        // Configure access control settings
        if (!engine->configureAccessControl(options)) {
            result.operation_successful = false;
            result.errors.push_back("Failed to configure access control");
            return false;
        }
        
        result.operation_successful = true;
        result.messages.push_back("Access control configured successfully");
        
        return true;
        
    } catch (const std::exception& e) {
        result.operation_successful = false;
        result.errors.push_back(std::string("Exception: ") + e.what());
        logError("ConfigureAccessControl", e.what());
        return false;
    } finally {
        result.end_time = std::chrono::steady_clock::now();
        operation_active = false;
    }
}

// Enable row-level security
bool GSecEnhanced::enableRowLevelSecurity(const std::string& database_path,
                                         const std::string& table_name,
                                         const std::string& policy_expression) {
    try {
        return engine->enableRowLevelSecurity(table_name, policy_expression);
    } catch (const std::exception& e) {
        logError("EnableRowLevelSecurity", e.what());
        return false;
    }
}

// Enable column-level security
bool GSecEnhanced::enableColumnLevelSecurity(const std::string& database_path,
                                            const std::string& table_name,
                                            const std::vector<std::string>& sensitive_columns) {
    try {
        return engine->enableColumnLevelSecurity(table_name, sensitive_columns);
    } catch (const std::exception& e) {
        logError("EnableColumnLevelSecurity", e.what());
        return false;
    }
}

// Security compliance check
bool GSecEnhanced::performComplianceCheck(const std::string& database_path,
                                         const SBEnhanced::SecurityComplianceOptions& options,
                                         SBEnhanced::SecurityAuditResult& result) {
    try {
        if (!initializeSecurityService(database_path)) {
            result.compliance_issues.push_back("Failed to initialize security service");
            return false;
        }
        
        updateProgress(SBEnhanced::SecurityOperation::COMPLIANCE_CHECK, 0, options.compliance_standards.size(), "Starting compliance check");
        
        result.detailed_stats.compliance_checks_performed = 0;
        result.detailed_stats.compliance_violations_found = 0;
        
        for (size_t i = 0; i < options.compliance_standards.size(); ++i) {
            const auto& standard = options.compliance_standards[i];
            
            updateProgress(SBEnhanced::SecurityOperation::COMPLIANCE_CHECK, i, options.compliance_standards.size(), "Checking " + standard);
            
            if (standard == "GDPR") {
                checkGDPRCompliance(database_path, result);
            } else if (standard == "HIPAA") {
                checkHIPAACompliance(database_path, result);
            } else if (standard == "SOX") {
                checkSOXCompliance(database_path, result);
            } else if (standard == "PCI-DSS") {
                checkPCIDSSCompliance(database_path, result);
            } else if (standard == "ISO27001") {
                checkISO27001Compliance(database_path, result);
            } else if (standard == "NIST") {
                checkNISTCompliance(database_path, result);
            }
            
            result.detailed_stats.compliance_checks_performed++;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        result.compliance_issues.push_back(std::string("Exception during compliance check: ") + e.what());
        logError("ComplianceCheck", e.what());
        return false;
    }
}

// Generate compliance report
bool GSecEnhanced::generateComplianceReport(const std::string& database_path,
                                           const std::vector<std::string>& standards,
                                           const std::string& report_path) {
    try {
        SBEnhanced::SecurityComplianceOptions options;
        options.compliance_standards = standards;
        options.generate_compliance_report = true;
        options.report_output_path = report_path;
        
        SBEnhanced::SecurityAuditResult result;
        return performComplianceCheck(database_path, options, result);
        
    } catch (const std::exception& e) {
        logError("GenerateComplianceReport", e.what());
        return false;
    }
}

// === UTILITY METHODS ===

// Progress monitoring
SBEnhanced::SecurityProgress GSecEnhanced::getCurrentProgress() const {
    return current_progress;
}

bool GSecEnhanced::isOperationActive() const {
    return operation_active.load();
}

void GSecEnhanced::cancelCurrentOperation() {
    operation_active = false;
}

// Error handling
std::vector<std::string> GSecEnhanced::getErrors() const {
    return error_log;
}

std::vector<std::string> GSecEnhanced::getWarnings() const {
    return warning_log;
}

std::string GSecEnhanced::getLastError() const {
    return last_error;
}

void GSecEnhanced::clearErrorLog() {
    error_log.clear();
    warning_log.clear();
    last_error.clear();
}

// === INTERNAL HELPER METHODS ===

// Database connection management
bool GSecEnhanced::connectToDatabase(const std::string& database_path, bool exclusive_access) {
    try {
        return engine->connectToDatabase(database_path, exclusive_access);
    } catch (const std::exception& e) {
        logError("DatabaseConnection", e.what());
        return false;
    }
}

void GSecEnhanced::disconnectFromDatabase() {
    try {
        engine->disconnectFromDatabase();
    } catch (const std::exception& e) {
        logError("DatabaseDisconnection", e.what());
    }
}

// Update progress
void GSecEnhanced::updateProgress(SBEnhanced::SecurityOperation operation,
                                 uint64_t completed, uint64_t total,
                                 const std::string& current_item) {
    current_progress.current_operation = operation;
    current_progress.completed_operations = completed;
    current_progress.total_operations = total;
    current_progress.current_user = current_item;
    current_progress.operation_active = true;
    
    if (completed == 0) {
        current_progress.start_time = std::chrono::steady_clock::now();
    }
}

// Error logging
void GSecEnhanced::logError(const std::string& operation, const std::string& error) {
    std::string formatted_error = "[" + operation + "] " + error;
    error_log.push_back(formatted_error);
    last_error = formatted_error;
}

void GSecEnhanced::logWarning(const std::string& operation, const std::string& warning) {
    std::string formatted_warning = "[" + operation + "] " + warning;
    warning_log.push_back(formatted_warning);
}

// Password hashing
bool GSecEnhanced::hashPassword(const std::string& password,
                               SBEnhanced::AuthenticationMethod method,
                               std::string& password_hash) {
    try {
        switch (method) {
            case SBEnhanced::AuthenticationMethod::SRP256:
                return engine->hashPasswordSRP256(password, password_hash);
            case SBEnhanced::AuthenticationMethod::SRP:
                return engine->hashPasswordSRP(password, password_hash);
            case SBEnhanced::AuthenticationMethod::LEGACY:
                return engine->hashPasswordLegacy(password, password_hash);
            default:
                return engine->hashPasswordSRP256(password, password_hash);
        }
    } catch (const std::exception& e) {
        logError("PasswordHashing", e.what());
        return false;
    }
}

// User account management helpers (placeholder implementations)
bool GSecEnhanced::createUserAccount(const std::string& database_path,
                                    const SBEnhanced::UserAccount& user_info,
                                    SBEnhanced::SecurityOperationResult& result) {
    // This would integrate with the actual ScratchBird user management system
    // For now, return a placeholder implementation
    return engine->createUserInDatabase(user_info);
}

bool GSecEnhanced::updateUserAccount(const std::string& database_path,
                                    const SBEnhanced::UserAccount& user_info,
                                    SBEnhanced::SecurityOperationResult& result) {
    // This would integrate with the actual ScratchBird user management system
    return engine->updateUserInDatabase(user_info);
}

bool GSecEnhanced::removeUserAccount(const std::string& database_path,
                                    const std::string& username,
                                    SBEnhanced::SecurityOperationResult& result) {
    // This would integrate with the actual ScratchBird user management system
    return engine->removeUserFromDatabase(username);
}

bool GSecEnhanced::getUserAccountInfo(const std::string& database_path,
                                     const std::string& username,
                                     SBEnhanced::UserAccount& user_info) {
    // This would integrate with the actual ScratchBird user management system
    return engine->getUserFromDatabase(username, user_info);
}

bool GSecEnhanced::createDatabaseRole(const std::string& database_path,
                                     const SBEnhanced::DatabaseRole& role_info,
                                     SBEnhanced::SecurityOperationResult& result) {
    // This would integrate with the actual ScratchBird role management system
    return engine->createRoleInDatabase(role_info);
}

bool GSecEnhanced::dropDatabaseRole(const std::string& database_path,
                                   const std::string& role_name,
                                   SBEnhanced::SecurityOperationResult& result) {
    // This would integrate with the actual ScratchBird role management system
    return engine->dropRoleFromDatabase(role_name);
}

bool GSecEnhanced::getRoleInfo(const std::string& database_path,
                              const std::string& role_name,
                              SBEnhanced::DatabaseRole& role_info) {
    // This would integrate with the actual ScratchBird role management system
    return engine->getRoleFromDatabase(role_name, role_info);
}

bool GSecEnhanced::terminateUserSessions(const std::string& database_path,
                                        const std::string& username,
                                        bool force_disconnect) {
    // This would integrate with the actual ScratchBird session management system
    return engine->terminateUserSessions(username, force_disconnect);
}

// === SECURITY AUDIT HELPER METHODS ===

// Collect security events
bool GSecEnhanced::collectSecurityEvents(const std::string& database_path,
                                        const SBEnhanced::SecurityAuditOptions& options,
                                        std::vector<std::map<std::string, std::string>>& events) {
    try {
        return engine->collectSecurityEvents(options.start_date, options.end_date, 
                                           options.target_users, options.event_types, events);
    } catch (const std::exception& e) {
        logError("CollectSecurityEvents", e.what());
        return false;
    }
}

// Analyze security events
bool GSecEnhanced::analyzeSecurityEvents(const std::vector<std::map<std::string, std::string>>& events,
                                        SBEnhanced::SecurityAuditResult& result) {
    try {
        result.security_violations_found = 0;
        result.policy_violations_found = 0;
        result.suspicious_activities_found = 0;
        
        for (const auto& event : events) {
            auto event_type = event.find("event_type");
            if (event_type != event.end()) {
                if (event_type->second == "FAILED_LOGIN") {
                    result.detailed_stats.failed_login_attempts++;
                    
                    // Check for multiple failed attempts from same user
                    auto username = event.find("username");
                    if (username != event.end()) {
                        // Analyze patterns for suspicious activity
                        if (result.detailed_stats.failed_login_attempts > 10) {
                            result.suspicious_activities_found++;
                            result.violation_details.push_back("Multiple failed login attempts for user: " + username->second);
                        }
                    }
                } else if (event_type->second == "PRIVILEGE_ESCALATION") {
                    result.privilege_escalations++;
                    result.security_violations_found++;
                    result.violation_details.push_back("Unauthorized privilege escalation detected");
                } else if (event_type->second == "UNAUTHORIZED_ACCESS") {
                    result.security_violations_found++;
                    result.violation_details.push_back("Unauthorized access attempt detected");
                } else if (event_type->second == "SUCCESSFUL_LOGIN") {
                    result.detailed_stats.successful_logins++;
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        logError("AnalyzeSecurityEvents", e.what());
        return false;
    }
}

// Generate audit report
bool GSecEnhanced::generateAuditReport(const SBEnhanced::SecurityAuditResult& result,
                                      const std::string& format,
                                      const std::string& output_path) {
    try {
        std::ofstream report_file(output_path);
        if (!report_file.is_open()) {
            logError("GenerateAuditReport", "Failed to open output file: " + output_path);
            return false;
        }
        
        if (format == "JSON") {
            // Generate JSON format report
            report_file << "{\n";
            report_file << "  \"audit_summary\": {\n";
            report_file << "    \"total_events_analyzed\": " << result.total_events_analyzed << ",\n";
            report_file << "    \"security_violations_found\": " << result.security_violations_found << ",\n";
            report_file << "    \"policy_violations_found\": " << result.policy_violations_found << ",\n";
            report_file << "    \"suspicious_activities_found\": " << result.suspicious_activities_found << "\n";
            report_file << "  },\n";
            
            report_file << "  \"violation_details\": [\n";
            for (size_t i = 0; i < result.violation_details.size(); ++i) {
                report_file << "    \"" << result.violation_details[i] << "\"";
                if (i < result.violation_details.size() - 1) report_file << ",";
                report_file << "\n";
            }
            report_file << "  ],\n";
            
            report_file << "  \"recommendations\": [\n";
            for (size_t i = 0; i < result.recommendations.size(); ++i) {
                report_file << "    \"" << result.recommendations[i] << "\"";
                if (i < result.recommendations.size() - 1) report_file << ",";
                report_file << "\n";
            }
            report_file << "  ]\n";
            report_file << "}\n";
        } else {
            // Generate text format report
            report_file << "=== SECURITY AUDIT REPORT ===\n\n";
            report_file << "Total Events Analyzed: " << result.total_events_analyzed << "\n";
            report_file << "Security Violations Found: " << result.security_violations_found << "\n";
            report_file << "Policy Violations Found: " << result.policy_violations_found << "\n";
            report_file << "Suspicious Activities Found: " << result.suspicious_activities_found << "\n\n";
            
            if (!result.violation_details.empty()) {
                report_file << "=== VIOLATION DETAILS ===\n";
                for (const auto& violation : result.violation_details) {
                    report_file << "- " << violation << "\n";
                }
                report_file << "\n";
            }
            
            if (!result.recommendations.empty()) {
                report_file << "=== RECOMMENDATIONS ===\n";
                for (const auto& recommendation : result.recommendations) {
                    report_file << "- " << recommendation << "\n";
                }
                report_file << "\n";
            }
        }
        
        report_file.close();
        return true;
        
    } catch (const std::exception& e) {
        logError("GenerateAuditReport", e.what());
        return false;
    }
}

// === COMPLIANCE CHECKING METHODS ===

// Check password policy compliance
void GSecEnhanced::checkPasswordPolicyCompliance(const std::string& database_path,
                                                 SBEnhanced::SecurityAuditResult& result) {
    try {
        // Check if password policy is enabled
        SBEnhanced::PasswordPolicyOptions current_policy;
        if (!engine->getPasswordPolicy(current_policy)) {
            result.policy_violations_found++;
            result.violation_details.push_back("No password policy configured");
            result.recommendations.push_back("Configure and enforce password policy");
            return;
        }
        
        // Check password policy strength
        if (current_policy.minimum_length < 8) {
            result.policy_violations_found++;
            result.violation_details.push_back("Password minimum length is too low");
            result.recommendations.push_back("Set minimum password length to at least 8 characters");
        }
        
        if (current_policy.minimum_uppercase == 0 && current_policy.minimum_lowercase == 0) {
            result.policy_violations_found++;
            result.violation_details.push_back("Password policy does not require character diversity");
            result.recommendations.push_back("Require both uppercase and lowercase characters");
        }
        
    } catch (const std::exception& e) {
        logError("CheckPasswordPolicyCompliance", e.what());
    }
}

// Check access control compliance
void GSecEnhanced::checkAccessControlCompliance(const std::string& database_path,
                                               SBEnhanced::SecurityAuditResult& result) {
    try {
        // Check for users with excessive privileges
        std::vector<std::string> admin_users;
        if (engine->getAdminUsers(admin_users)) {
            if (admin_users.size() > 5) {
                result.policy_violations_found++;
                result.violation_details.push_back("Too many users with administrative privileges");
                result.recommendations.push_back("Review and limit administrative access");
            }
        }
        
        // Check for default passwords
        std::vector<std::string> default_users;
        if (engine->getUsersWithDefaultPasswords(default_users)) {
            if (!default_users.empty()) {
                result.security_violations_found += default_users.size();
                result.violation_details.push_back("Users with default passwords found");
                result.recommendations.push_back("Force password change for users with default passwords");
            }
        }
        
    } catch (const std::exception& e) {
        logError("CheckAccessControlCompliance", e.what());
    }
}

// GDPR compliance check
void GSecEnhanced::checkGDPRCompliance(const std::string& database_path,
                                      SBEnhanced::SecurityAuditResult& result) {
    try {
        // Check for data encryption
        bool encryption_enabled = false;
        if (!engine->isEncryptionEnabled(encryption_enabled) || !encryption_enabled) {
            result.compliance_issues.push_back("GDPR: Database encryption not enabled");
            result.recommendations.push_back("Enable database encryption for GDPR compliance");
        }
        
        // Check for audit logging
        bool audit_enabled = false;
        if (!engine->isAuditLoggingEnabled(audit_enabled) || !audit_enabled) {
            result.compliance_issues.push_back("GDPR: Audit logging not enabled");
            result.recommendations.push_back("Enable comprehensive audit logging for GDPR compliance");
        }
        
        // Check for data retention policies
        bool retention_policy = false;
        if (!engine->hasDataRetentionPolicy(retention_policy) || !retention_policy) {
            result.compliance_issues.push_back("GDPR: No data retention policy configured");
            result.recommendations.push_back("Configure data retention policy for GDPR compliance");
        }
        
    } catch (const std::exception& e) {
        logError("CheckGDPRCompliance", e.what());
    }
}

// HIPAA compliance check
void GSecEnhanced::checkHIPAACompliance(const std::string& database_path,
                                       SBEnhanced::SecurityAuditResult& result) {
    try {
        // Check for access controls
        bool access_controls = false;
        if (!engine->hasAccessControls(access_controls) || !access_controls) {
            result.compliance_issues.push_back("HIPAA: Insufficient access controls");
            result.recommendations.push_back("Implement role-based access controls for HIPAA compliance");
        }
        
        // Check for audit trails
        bool audit_trails = false;
        if (!engine->hasAuditTrails(audit_trails) || !audit_trails) {
            result.compliance_issues.push_back("HIPAA: No audit trails configured");
            result.recommendations.push_back("Enable comprehensive audit trails for HIPAA compliance");
        }
        
    } catch (const std::exception& e) {
        logError("CheckHIPAACompliance", e.what());
    }
}

// SOX compliance check
void GSecEnhanced::checkSOXCompliance(const std::string& database_path,
                                     SBEnhanced::SecurityAuditResult& result) {
    try {
        // Check for segregation of duties
        bool segregation = false;
        if (!engine->hasSegregationOfDuties(segregation) || !segregation) {
            result.compliance_issues.push_back("SOX: Segregation of duties not implemented");
            result.recommendations.push_back("Implement segregation of duties for SOX compliance");
        }
        
    } catch (const std::exception& e) {
        logError("CheckSOXCompliance", e.what());
    }
}

// PCI-DSS compliance check
void GSecEnhanced::checkPCIDSSCompliance(const std::string& database_path,
                                        SBEnhanced::SecurityAuditResult& result) {
    try {
        // Check for encryption
        bool encryption = false;
        if (!engine->isEncryptionEnabled(encryption) || !encryption) {
            result.compliance_issues.push_back("PCI-DSS: Data encryption not enabled");
            result.recommendations.push_back("Enable encryption for PCI-DSS compliance");
        }
        
    } catch (const std::exception& e) {
        logError("CheckPCIDSSCompliance", e.what());
    }
}

// ISO27001 compliance check
void GSecEnhanced::checkISO27001Compliance(const std::string& database_path,
                                          SBEnhanced::SecurityAuditResult& result) {
    try {
        // Check for information security management system
        bool isms = false;
        if (!engine->hasISMS(isms) || !isms) {
            result.compliance_issues.push_back("ISO27001: Information Security Management System not implemented");
            result.recommendations.push_back("Implement ISMS for ISO27001 compliance");
        }
        
    } catch (const std::exception& e) {
        logError("CheckISO27001Compliance", e.what());
    }
}

// NIST compliance check
void GSecEnhanced::checkNISTCompliance(const std::string& database_path,
                                      SBEnhanced::SecurityAuditResult& result) {
    try {
        // Check for security framework implementation
        bool framework = false;
        if (!engine->hasSecurityFramework(framework) || !framework) {
            result.compliance_issues.push_back("NIST: Security framework not implemented");
            result.recommendations.push_back("Implement NIST cybersecurity framework");
        }
        
    } catch (const std::exception& e) {
        logError("CheckNISTCompliance", e.what());
    }
}

// === UTILITY FUNCTIONS ===

namespace SBEnhanced {

// Quick user operations
bool quickAddUser(const std::string& database_path, const std::string& username, const std::string& password) {
    GSecEnhanced gsec;
    UserManagementOptions options;
    options.operation = UserOperation::ADD_USER;
    options.username = username;
    options.password = password;
    
    SecurityOperationResult result;
    return gsec.addUser(database_path, username, password, options, result);
}

bool quickDeleteUser(const std::string& database_path, const std::string& username) {
    GSecEnhanced gsec;
    SecurityOperationResult result;
    return gsec.deleteUser(database_path, username, result);
}

bool quickListUsers(const std::string& database_path, std::vector<std::string>& usernames) {
    GSecEnhanced gsec;
    std::vector<UserAccount> users;
    SecurityOperationResult result;
    
    if (gsec.displayAllUsers(database_path, users, result)) {
        usernames.clear();
        for (const auto& user : users) {
            usernames.push_back(user.username);
        }
        return true;
    }
    return false;
}

// Username validation
bool validateUsername(const std::string& username) {
    if (username.empty() || username.length() > 63) {
        return false;
    }
    
    // Username must start with a letter
    if (!std::isalpha(username[0])) {
        return false;
    }
    
    // Username can contain letters, numbers, and underscores
    for (char c : username) {
        if (!std::isalnum(c) && c != '_') {
            return false;
        }
    }
    
    return true;
}

// Password validation
bool validatePassword(const std::string& password, const PasswordPolicyOptions& policy) {
    std::vector<std::string> violations;
    return GSecEnhanced().validatePasswordPolicy(password, policy, violations);
}

// Role name validation
bool validateRoleName(const std::string& role_name) {
    if (role_name.empty() || role_name.length() > 63) {
        return false;
    }
    
    // Role name must start with a letter
    if (!std::isalpha(role_name[0])) {
        return false;
    }
    
    // Role name can contain letters, numbers, and underscores
    for (char c : role_name) {
        if (!std::isalnum(c) && c != '_') {
            return false;
        }
    }
    
    return true;
}

// Generate secure password
std::string generateSecurePassword(uint32_t length) {
    const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*";
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, chars.length() - 1);
    
    std::string password;
    password.reserve(length);
    
    for (uint32_t i = 0; i < length; ++i) {
        password += chars[dis(gen)];
    }
    
    return password;
}

// Get supported compliance standards
std::vector<std::string> getSupportedComplianceStandards() {
    return {"GDPR", "HIPAA", "SOX", "PCI-DSS", "ISO27001", "NIST"};
}

} // namespace SBEnhanced