#pragma once

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <chrono>
#include <functional>
#include <atomic>

// Forward declarations for ScratchBird engine components
namespace jrd {
    class Attachment;
    class Database;
    class Transaction;
    class Service;
}

class SBEngineIntegration;

namespace SBEnhanced {

// Security management operation types
enum class SecurityOperation {
    USER_MANAGEMENT = 0,
    ROLE_MANAGEMENT = 1,
    PRIVILEGE_MANAGEMENT = 2,
    SECURITY_AUDIT = 3,
    PASSWORD_POLICY = 4,
    SESSION_MANAGEMENT = 5,
    AUTHENTICATION_CONFIG = 6,
    SECURITY_REPORTING = 7,
    DATABASE_ENCRYPTION = 8,
    ACCESS_CONTROL = 9,
    SECURITY_MONITORING = 10,
    COMPLIANCE_CHECK = 11
};

// User management operations (original GSEC compatibility)
enum class UserOperation {
    ADD_USER = 0,           // -add
    MODIFY_USER = 1,        // -modify
    DELETE_USER = 2,        // -delete
    DISPLAY_USER = 3,       // -display
    DISPLAY_ALL_USERS = 4   // -display (no username)
};

// Role management operations
enum class RoleOperation {
    CREATE_ROLE = 0,
    DROP_ROLE = 1,
    GRANT_ROLE = 2,
    REVOKE_ROLE = 3,
    LIST_ROLES = 4,
    DESCRIBE_ROLE = 5
};

// Authentication methods
enum class AuthenticationMethod {
    LEGACY = 0,             // Legacy password authentication
    SRP256 = 1,             // SRP-256 authentication (modern default)
    SRP = 2,                // SRP authentication
    WIN_SSPI = 3,           // Windows SSPI authentication
    MULTIFACTOR = 4,        // Multi-factor authentication
    CERTIFICATE = 5,        // Certificate-based authentication
    KERBEROS = 6,           // Kerberos authentication
    LDAP = 7                // LDAP authentication
};

// Password policy enforcement levels
enum class PasswordPolicyLevel {
    NONE = 0,               // No password policy
    BASIC = 1,              // Basic length requirements
    STANDARD = 2,           // Standard complexity requirements
    STRICT = 3,             // Strict security requirements
    ENTERPRISE = 4          // Enterprise-grade security
};

// Security audit levels
enum class SecurityAuditLevel {
    DISABLED = 0,           // No security auditing
    BASIC = 1,              // Basic login/logout events
    STANDARD = 2,           // Standard security events
    COMPREHENSIVE = 3,      // Comprehensive security monitoring
    FORENSIC = 4            // Forensic-level detail
};

// User account status
enum class UserAccountStatus {
    ACTIVE = 0,             // Active user account
    DISABLED = 1,           // Disabled user account
    LOCKED = 2,             // Locked due to failed attempts
    EXPIRED = 3,            // Password expired
    PENDING = 4             // Pending activation
};

// Database roles and privileges
enum class DatabasePrivilege {
    SELECT = 0,
    INSERT = 1,
    UPDATE = 2,
    DELETE = 3,
    EXECUTE = 4,
    REFERENCES = 5,
    USAGE = 6,
    CREATE = 7,
    ALTER = 8,
    DROP = 9,
    CONNECT = 10,
    RESOURCE = 11,
    DBA = 12,
    BACKUP = 13,
    RESTORE = 14,
    MONITOR = 15
};

// Security progress tracking
struct SecurityProgress {
    SecurityOperation current_operation = SecurityOperation::USER_MANAGEMENT;
    uint64_t total_operations = 0;
    uint64_t completed_operations = 0;
    uint64_t users_processed = 0;
    uint64_t roles_processed = 0;
    uint64_t security_violations_found = 0;
    uint64_t security_violations_resolved = 0;
    std::string current_user;
    std::string current_database;
    std::chrono::steady_clock::time_point start_time;
    bool operation_active = false;
    
    double getProgressPercentage() const {
        if (total_operations == 0) return 0.0;
        return (static_cast<double>(completed_operations) / total_operations) * 100.0;
    }
    
    std::chrono::seconds getElapsedTime() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
    }
    
    std::chrono::seconds getEstimatedTimeRemaining() const {
        if (completed_operations == 0) return std::chrono::seconds(0);
        auto elapsed = getElapsedTime();
        double progress = getProgressPercentage() / 100.0;
        if (progress <= 0.0) return std::chrono::seconds(0);
        double total_time = elapsed.count() / progress;
        return std::chrono::seconds(static_cast<long>(total_time - elapsed.count()));
    }
};

// User account information
struct UserAccount {
    std::string username;
    std::string first_name;
    std::string middle_name;
    std::string last_name;
    std::string description;
    std::string email;
    std::string phone;
    UserAccountStatus status = UserAccountStatus::ACTIVE;
    AuthenticationMethod auth_method = AuthenticationMethod::SRP256;
    std::chrono::system_clock::time_point created_date;
    std::chrono::system_clock::time_point last_login;
    std::chrono::system_clock::time_point password_changed;
    std::chrono::system_clock::time_point password_expires;
    uint32_t failed_login_attempts = 0;
    bool admin_privileges = false;
    bool password_must_change = false;
    std::vector<std::string> assigned_roles;
    std::vector<DatabasePrivilege> privileges;
    std::map<std::string, std::string> custom_attributes;
    
    bool isActive() const {
        return status == UserAccountStatus::ACTIVE;
    }
    
    bool isPasswordExpired() const {
        if (password_expires.time_since_epoch().count() == 0) return false;
        return std::chrono::system_clock::now() > password_expires;
    }
    
    bool isLocked() const {
        return status == UserAccountStatus::LOCKED;
    }
};

// Database role information
struct DatabaseRole {
    std::string role_name;
    std::string description;
    std::vector<DatabasePrivilege> privileges;
    std::vector<std::string> granted_users;
    std::vector<std::string> granted_roles;
    bool system_role = false;
    std::chrono::system_clock::time_point created_date;
    std::string created_by;
    std::map<std::string, std::string> role_attributes;
    
    bool hasPrivilege(DatabasePrivilege privilege) const {
        return std::find(privileges.begin(), privileges.end(), privilege) != privileges.end();
    }
};

// User management options
struct UserManagementOptions {
    UserOperation operation = UserOperation::DISPLAY_ALL_USERS;
    std::string username;
    std::string password;
    std::string first_name;
    std::string middle_name;
    std::string last_name;
    std::string description;
    std::string email;
    std::string phone;
    AuthenticationMethod auth_method = AuthenticationMethod::SRP256;
    bool admin_privileges = false;
    bool force_password_change = false;
    std::vector<std::string> assign_roles;
    std::vector<DatabasePrivilege> grant_privileges;
    std::function<void(const SecurityProgress&)> progress_callback;
};

// Role management options
struct RoleManagementOptions {
    RoleOperation operation = RoleOperation::LIST_ROLES;
    std::string role_name;
    std::string description;
    std::vector<DatabasePrivilege> privileges;
    std::vector<std::string> grant_to_users;
    std::vector<std::string> grant_to_roles;
    bool system_role = false;
    std::function<void(const SecurityProgress&)> progress_callback;
};

// Password policy configuration
struct PasswordPolicyOptions {
    PasswordPolicyLevel level = PasswordPolicyLevel::STANDARD;
    uint32_t minimum_length = 8;
    uint32_t maximum_length = 128;
    uint32_t minimum_uppercase = 1;
    uint32_t minimum_lowercase = 1;
    uint32_t minimum_digits = 1;
    uint32_t minimum_special_chars = 1;
    uint32_t password_history_size = 5;
    uint32_t password_expiry_days = 90;
    uint32_t lockout_threshold = 5;
    uint32_t lockout_duration_minutes = 30;
    bool require_unique_passwords = true;
    bool prevent_username_in_password = true;
    bool prevent_common_passwords = true;
    std::vector<std::string> forbidden_patterns;
    std::function<void(const SecurityProgress&)> progress_callback;
};

// Security audit options
struct SecurityAuditOptions {
    SecurityAuditLevel level = SecurityAuditLevel::STANDARD;
    std::chrono::system_clock::time_point start_date;
    std::chrono::system_clock::time_point end_date;
    std::vector<std::string> target_users;
    std::vector<std::string> target_databases;
    std::vector<std::string> event_types;
    bool include_failed_attempts = true;
    bool include_privilege_changes = true;
    bool include_schema_changes = true;
    bool include_data_access = false;  // Can be expensive
    bool include_password_policy_check = true;
    bool include_access_control_check = true;
    std::string output_file_path;
    std::string report_format = "TEXT";  // TEXT, CSV, JSON, XML
    std::function<void(const SecurityProgress&)> progress_callback;
};

// Session management options
struct SessionManagementOptions {
    bool list_active_sessions = false;
    bool kill_user_sessions = false;
    std::string target_username;
    std::string target_database;
    uint32_t max_concurrent_sessions = 0;  // 0 = unlimited
    uint32_t session_timeout_minutes = 0;  // 0 = no timeout
    bool force_disconnect = false;
    std::function<void(const SecurityProgress&)> progress_callback;
};

// Authentication configuration options
struct AuthenticationConfigOptions {
    AuthenticationMethod default_method = AuthenticationMethod::SRP256;
    bool enable_multifactor = false;
    bool require_secure_connection = false;
    std::string certificate_path;
    std::string ldap_server;
    std::string ldap_base_dn;
    std::string kerberos_realm;
    uint32_t session_timeout_minutes = 480;  // 8 hours default
    bool enable_single_signon = false;
    std::map<std::string, std::string> auth_plugins;
    std::function<void(const SecurityProgress&)> progress_callback;
};

// Database encryption options
struct DatabaseEncryptionOptions {
    bool enable_encryption = false;
    std::string encryption_algorithm = "AES256";
    std::string key_file_path;
    std::string key_holder_name;
    bool encrypt_backups = true;
    bool encrypt_wire_protocol = true;
    std::string certificate_path;
    std::function<void(const SecurityProgress&)> progress_callback;
};

// Access control options
struct AccessControlOptions {
    bool enable_row_level_security = false;
    bool enable_column_level_security = false;
    bool enable_data_masking = false;
    std::vector<std::string> sensitive_tables;
    std::vector<std::string> sensitive_columns;
    std::map<std::string, std::string> masking_rules;
    std::string default_schema;
    bool restrict_ddl_operations = false;
    std::function<void(const SecurityProgress&)> progress_callback;
};

// Security compliance options
struct SecurityComplianceOptions {
    std::vector<std::string> compliance_standards;  // GDPR, HIPAA, SOX, PCI-DSS
    bool check_password_policies = true;
    bool check_access_controls = true;
    bool check_audit_logging = true;
    bool check_encryption_status = true;
    bool check_user_privileges = true;
    bool generate_compliance_report = true;
    std::string report_output_path;
    std::function<void(const SecurityProgress&)> progress_callback;
};

// Security operation result
struct SecurityOperationResult {
    SecurityOperation operation_type;
    bool operation_successful = false;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    std::vector<std::string> messages;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::vector<std::string> security_recommendations;
    SecurityStatistics detailed_stats;
    
    std::chrono::milliseconds getDuration() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    }
    
    std::string generateOperationReport() const;
};

// Security statistics tracking
struct SecurityStatistics {
    std::chrono::steady_clock::time_point operation_start;
    std::chrono::steady_clock::time_point operation_end;
    SecurityOperation operation_type;
    
    // User management statistics
    uint64_t users_created = 0;
    uint64_t users_modified = 0;
    uint64_t users_deleted = 0;
    uint64_t users_locked = 0;
    uint64_t users_unlocked = 0;
    uint64_t password_resets = 0;
    
    // Role management statistics
    uint64_t roles_created = 0;
    uint64_t roles_dropped = 0;
    uint64_t roles_granted = 0;
    uint64_t roles_revoked = 0;
    uint64_t privilege_grants = 0;
    uint64_t privilege_revokes = 0;
    
    // Security audit statistics
    uint64_t security_events_found = 0;
    uint64_t failed_login_attempts = 0;
    uint64_t successful_logins = 0;
    uint64_t privilege_escalations = 0;
    uint64_t suspicious_activities = 0;
    uint64_t security_violations = 0;
    
    // Session management statistics
    uint64_t active_sessions = 0;
    uint64_t sessions_terminated = 0;
    uint64_t concurrent_session_limit_hits = 0;
    
    // Compliance statistics
    uint64_t compliance_checks_performed = 0;
    uint64_t compliance_violations_found = 0;
    uint64_t policy_violations = 0;
    
    // Error tracking
    std::vector<std::string> errors_encountered;
    std::vector<std::string> warnings_generated;
    std::vector<std::string> actions_performed;
    
    std::chrono::milliseconds getDuration() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(operation_end - operation_start);
    }
    
    std::string generateSummaryReport() const;
    std::string generateDetailedReport() const;
};

// Security audit result
struct SecurityAuditResult {
    bool audit_successful = false;
    uint64_t total_events_analyzed = 0;
    uint64_t security_violations_found = 0;
    uint64_t policy_violations_found = 0;
    uint64_t suspicious_activities_found = 0;
    std::vector<std::string> violation_details;
    std::vector<std::string> recommendations;
    std::vector<std::string> compliance_issues;
    SecurityStatistics detailed_stats;
    std::string audit_report_path;
    
    bool hasSecurityIssues() const {
        return security_violations_found > 0 || policy_violations_found > 0;
    }
    
    bool requiresImmediateAttention() const {
        return security_violations_found > 0 || suspicious_activities_found > 10;
    }
};

// User management result
struct UserManagementResult {
    bool operation_successful = false;
    UserOperation operation_performed;
    std::string affected_username;
    std::vector<UserAccount> user_accounts;
    uint64_t users_processed = 0;
    uint64_t operations_completed = 0;
    std::vector<std::string> operation_messages;
    SecurityStatistics detailed_stats;
    
    std::string generateUserReport() const;
};

// Role management result
struct RoleManagementResult {
    bool operation_successful = false;
    RoleOperation operation_performed;
    std::string affected_role;
    std::vector<DatabaseRole> database_roles;
    uint64_t roles_processed = 0;
    uint64_t operations_completed = 0;
    std::vector<std::string> operation_messages;
    SecurityStatistics detailed_stats;
    
    std::string generateRoleReport() const;
};

} // namespace SBEnhanced

// Main enhanced GSEC utility class
class GSecEnhanced {
private:
    std::unique_ptr<SBEngineIntegration> engine;
    std::unique_ptr<jrd::Service> security_service;
    std::atomic<bool> operation_active{false};
    SBEnhanced::SecurityProgress current_progress;
    std::vector<std::string> error_log;
    std::vector<std::string> warning_log;
    std::string last_error;

public:
    // Constructor and destructor
    GSecEnhanced();
    ~GSecEnhanced();
    
    // === ORIGINAL GSEC FUNCTIONALITY (100% Compatible) ===
    
    // User management (original GSEC operations)
    bool addUser(const std::string& database_path,
                 const std::string& username,
                 const std::string& password,
                 const SBEnhanced::UserManagementOptions& options,
                 SBEnhanced::SecurityOperationResult& result);
    
    bool modifyUser(const std::string& database_path,
                    const std::string& username,
                    const SBEnhanced::UserManagementOptions& options,
                    SBEnhanced::SecurityOperationResult& result);
    
    bool deleteUser(const std::string& database_path,
                    const std::string& username,
                    SBEnhanced::SecurityOperationResult& result);
    
    bool displayUser(const std::string& database_path,
                     const std::string& username,
                     SBEnhanced::UserAccount& user_info,
                     SBEnhanced::SecurityOperationResult& result);
    
    bool displayAllUsers(const std::string& database_path,
                         std::vector<SBEnhanced::UserAccount>& users,
                         SBEnhanced::SecurityOperationResult& result);
    
    // User management operations
    bool performUserManagement(const std::string& database_path,
                              const SBEnhanced::UserManagementOptions& options,
                              SBEnhanced::UserManagementResult& result);
    
    // Role management operations
    bool performRoleManagement(const std::string& database_path,
                              const SBEnhanced::RoleManagementOptions& options,
                              SBEnhanced::RoleManagementResult& result);
    
    // Password policy management
    bool configurePasswordPolicy(const std::string& database_path,
                                const SBEnhanced::PasswordPolicyOptions& options,
                                SBEnhanced::SecurityOperationResult& result);
    
    bool validatePasswordPolicy(const std::string& password,
                               const SBEnhanced::PasswordPolicyOptions& policy,
                               std::vector<std::string>& violations);
    
    // Security auditing
    bool performSecurityAudit(const std::string& database_path,
                             const SBEnhanced::SecurityAuditOptions& options,
                             SBEnhanced::SecurityAuditResult& result);
    
    bool generateSecurityReport(const std::string& database_path,
                               const SBEnhanced::SecurityAuditOptions& options,
                               const std::string& report_path);
    
    // Session management
    bool manageUserSessions(const std::string& database_path,
                           const SBEnhanced::SessionManagementOptions& options,
                           SBEnhanced::SecurityOperationResult& result);
    
    bool listActiveSessions(const std::string& database_path,
                           std::vector<std::map<std::string, std::string>>& sessions);
    
    bool terminateUserSessions(const std::string& database_path,
                              const std::string& username,
                              bool force_disconnect = false);
    
    // Authentication configuration
    bool configureAuthentication(const std::string& database_path,
                                const SBEnhanced::AuthenticationConfigOptions& options,
                                SBEnhanced::SecurityOperationResult& result);
    
    bool setDefaultAuthenticationMethod(const std::string& database_path,
                                       SBEnhanced::AuthenticationMethod method);
    
    // Database encryption
    bool configureDatabaseEncryption(const std::string& database_path,
                                    const SBEnhanced::DatabaseEncryptionOptions& options,
                                    SBEnhanced::SecurityOperationResult& result);
    
    bool encryptDatabase(const std::string& database_path,
                        const std::string& key_file_path,
                        const std::string& algorithm = "AES256");
    
    bool decryptDatabase(const std::string& database_path,
                        const std::string& key_file_path);
    
    // Access control
    bool configureAccessControl(const std::string& database_path,
                               const SBEnhanced::AccessControlOptions& options,
                               SBEnhanced::SecurityOperationResult& result);
    
    bool enableRowLevelSecurity(const std::string& database_path,
                               const std::string& table_name,
                               const std::string& policy_expression);
    
    bool enableColumnLevelSecurity(const std::string& database_path,
                                  const std::string& table_name,
                                  const std::vector<std::string>& sensitive_columns);
    
    // Security compliance
    bool performComplianceCheck(const std::string& database_path,
                               const SBEnhanced::SecurityComplianceOptions& options,
                               SBEnhanced::SecurityAuditResult& result);
    
    bool generateComplianceReport(const std::string& database_path,
                                 const std::vector<std::string>& standards,
                                 const std::string& report_path);
    
    // Privilege management
    bool grantUserPrivileges(const std::string& database_path,
                            const std::string& username,
                            const std::vector<SBEnhanced::DatabasePrivilege>& privileges,
                            SBEnhanced::SecurityOperationResult& result);
    
    bool revokeUserPrivileges(const std::string& database_path,
                             const std::string& username,
                             const std::vector<SBEnhanced::DatabasePrivilege>& privileges,
                             SBEnhanced::SecurityOperationResult& result);
    
    bool grantRoleToUser(const std::string& database_path,
                        const std::string& username,
                        const std::string& role_name,
                        SBEnhanced::SecurityOperationResult& result);
    
    bool revokeRoleFromUser(const std::string& database_path,
                           const std::string& username,
                           const std::string& role_name,
                           SBEnhanced::SecurityOperationResult& result);
    
    // Security monitoring
    bool enableSecurityMonitoring(const std::string& database_path,
                                 SBEnhanced::SecurityAuditLevel level);
    
    bool disableSecurityMonitoring(const std::string& database_path);
    
    bool getSecurityStatus(const std::string& database_path,
                          std::map<std::string, std::string>& status_info);
    
    // Progress monitoring
    SBEnhanced::SecurityProgress getCurrentProgress() const;
    bool isOperationActive() const;
    void cancelCurrentOperation();
    
    // Error handling and logging
    std::vector<std::string> getErrors() const;
    std::vector<std::string> getWarnings() const;
    std::string getLastError() const;
    void clearErrorLog();
    
    // Statistics and reporting
    std::string generateSecurityReport(const SBEnhanced::SecurityStatistics& stats) const;
    std::string generateUserManagementReport(const SBEnhanced::UserManagementResult& result) const;
    std::string generateRoleManagementReport(const SBEnhanced::RoleManagementResult& result) const;
    std::string generateAuditReport(const SBEnhanced::SecurityAuditResult& result) const;
    
    // Database information
    bool getDatabaseSecurityInfo(const std::string& database_path,
                                std::map<std::string, std::string>& security_info);
    
    bool getUserStatistics(const std::string& database_path,
                          std::map<std::string, uint64_t>& statistics);
    
    bool getRoleStatistics(const std::string& database_path,
                          std::map<std::string, uint64_t>& statistics);

private:
    // Internal initialization
    bool initializeEngine();
    bool initializeSecurityService(const std::string& database_path);
    
    // Internal user management helpers
    bool createUserAccount(const std::string& database_path,
                          const SBEnhanced::UserAccount& user_info,
                          SBEnhanced::SecurityOperationResult& result);
    
    bool updateUserAccount(const std::string& database_path,
                          const SBEnhanced::UserAccount& user_info,
                          SBEnhanced::SecurityOperationResult& result);
    
    bool removeUserAccount(const std::string& database_path,
                          const std::string& username,
                          SBEnhanced::SecurityOperationResult& result);
    
    bool getUserAccountInfo(const std::string& database_path,
                           const std::string& username,
                           SBEnhanced::UserAccount& user_info);
    
    // Internal role management helpers
    bool createDatabaseRole(const std::string& database_path,
                           const SBEnhanced::DatabaseRole& role_info,
                           SBEnhanced::SecurityOperationResult& result);
    
    bool dropDatabaseRole(const std::string& database_path,
                         const std::string& role_name,
                         SBEnhanced::SecurityOperationResult& result);
    
    bool getRoleInfo(const std::string& database_path,
                    const std::string& role_name,
                    SBEnhanced::DatabaseRole& role_info);
    
    // Internal security helpers
    bool validateUserCredentials(const std::string& username,
                                const std::string& password,
                                const SBEnhanced::PasswordPolicyOptions& policy);
    
    bool hashPassword(const std::string& password,
                     SBEnhanced::AuthenticationMethod method,
                     std::string& password_hash);
    
    bool verifyPasswordHash(const std::string& password,
                           const std::string& password_hash,
                           SBEnhanced::AuthenticationMethod method);
    
    // Internal audit helpers
    bool collectSecurityEvents(const std::string& database_path,
                              const SBEnhanced::SecurityAuditOptions& options,
                              std::vector<std::map<std::string, std::string>>& events);
    
    bool analyzeSecurityEvents(const std::vector<std::map<std::string, std::string>>& events,
                              SBEnhanced::SecurityAuditResult& result);
    
    bool generateAuditReport(const SBEnhanced::SecurityAuditResult& result,
                            const std::string& format,
                            const std::string& output_path);
    
    // Compliance checking helpers
    void checkPasswordPolicyCompliance(const std::string& database_path,
                                      SBEnhanced::SecurityAuditResult& result);
    
    void checkAccessControlCompliance(const std::string& database_path,
                                     SBEnhanced::SecurityAuditResult& result);
    
    void checkGDPRCompliance(const std::string& database_path,
                           SBEnhanced::SecurityAuditResult& result);
    
    void checkHIPAACompliance(const std::string& database_path,
                            SBEnhanced::SecurityAuditResult& result);
    
    void checkSOXCompliance(const std::string& database_path,
                          SBEnhanced::SecurityAuditResult& result);
    
    void checkPCIDSSCompliance(const std::string& database_path,
                             SBEnhanced::SecurityAuditResult& result);
    
    void checkISO27001Compliance(const std::string& database_path,
                               SBEnhanced::SecurityAuditResult& result);
    
    void checkNISTCompliance(const std::string& database_path,
                           SBEnhanced::SecurityAuditResult& result);
    
    // Internal utility helpers
    void updateProgress(SBEnhanced::SecurityOperation operation,
                       uint64_t completed, uint64_t total,
                       const std::string& current_item);
    
    void logError(const std::string& operation, const std::string& error);
    void logWarning(const std::string& operation, const std::string& warning);
    
    // Database connection management
    bool connectToDatabase(const std::string& database_path, bool exclusive_access = false);
    void disconnectFromDatabase();
    
    // Service integration helpers
    bool startSecurityService(SBEnhanced::SecurityOperation operation);
    void stopSecurityService();
    bool isSecurityServiceActive() const;
    
    // Progress callback management
    void notifyProgress(const std::function<void(const SBEnhanced::SecurityProgress&)>& callback);
    
    // Statistics collection helpers
    void collectUserStatistics(SBEnhanced::SecurityStatistics& stats);
    void collectRoleStatistics(SBEnhanced::SecurityStatistics& stats);
    void collectAuditStatistics(SBEnhanced::SecurityStatistics& stats);
    void collectSessionStatistics(SBEnhanced::SecurityStatistics& stats);
};

// Utility functions for enhanced GSEC
namespace SBEnhanced {

// Quick user operations
bool quickAddUser(const std::string& database_path, const std::string& username, const std::string& password);
bool quickDeleteUser(const std::string& database_path, const std::string& username);
bool quickListUsers(const std::string& database_path, std::vector<std::string>& usernames);

// Quick role operations
bool quickCreateRole(const std::string& database_path, const std::string& role_name);
bool quickDropRole(const std::string& database_path, const std::string& role_name);
bool quickGrantRole(const std::string& database_path, const std::string& username, const std::string& role_name);

// Security validation helpers
bool validateUsername(const std::string& username);
bool validatePassword(const std::string& password, const PasswordPolicyOptions& policy);
bool validateRoleName(const std::string& role_name);

// Security utility functions
std::string generateSecurePassword(uint32_t length = 12);
std::string hashPasswordSecure(const std::string& password, AuthenticationMethod method);
bool isPasswordCompromised(const std::string& password);

// Compliance helpers
std::vector<std::string> getSupportedComplianceStandards();
bool checkGDPRCompliance(const std::string& database_path);
bool checkHIPAACompliance(const std::string& database_path);
bool checkSOXCompliance(const std::string& database_path);
bool checkPCIDSSCompliance(const std::string& database_path);

// Compatibility helpers for command-line usage
int parseGSecCommandLine(int argc, char* argv[], 
                        std::string& database_path,
                        UserManagementOptions& user_opts,
                        RoleManagementOptions& role_opts,
                        SecurityAuditOptions& audit_opts);

bool executeClassicGSecCommand(const std::string& command_line);

} // namespace SBEnhanced