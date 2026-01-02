#include "scratchbird/core/auth_provider.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/password_hash.h"
#include "scratchbird/core/login_attempt_tracker.h"  // P0-2: Account lockout
#include "scratchbird/core/audit_logger.h"           // P0-3: Security audit logging
#include "scratchbird/core/logger.h"

namespace scratchbird {
namespace core {

// ============================================================================
// LocalAuthProvider Implementation (Alpha - Fully Implemented)
// ============================================================================

LocalAuthProvider::LocalAuthProvider(CatalogManager* catalog, AuditLogger* audit_logger)
    : catalog_(catalog),
      login_tracker_(nullptr),
      audit_logger_(audit_logger)
{
    if (catalog_ == nullptr) {
        throw std::invalid_argument("LocalAuthProvider: catalog cannot be null");
    }

    // P0-2: Initialize login attempt tracker with default policy
    LockoutPolicy policy;
    login_tracker_ = new LoginAttemptTracker(policy);

}

LocalAuthProvider::~LocalAuthProvider()
{
    // P0-2: Cleanup login tracker
    delete login_tracker_;
}

AuthResult LocalAuthProvider::authenticate(
    const std::string& username,
    const std::string& password,
    AuthUserInfo& user_info_out,
    std::string& error_msg_out)
{
    // P0-2: Check if account is locked FIRST (before any DB lookups)
    if (login_tracker_->isAccountLocked(username)) {
        uint64_t remaining_ms = login_tracker_->getLockoutTimeRemaining(username);
        uint32_t remaining_minutes = static_cast<uint32_t>((remaining_ms + 59999) / 60000);  // Round up

        LOG_WARNING(GENERAL, "Login attempt for locked account: %s (locked for %u more minutes)",
                   username.c_str(), remaining_minutes);

        // P0-3: Audit log - account locked
        if (audit_logger_) {
            AuditEvent event = AuditLogger::createLoginFailureEvent(username, "account_locked");
            ErrorContext audit_ctx;
            audit_logger_->logEvent(event, &audit_ctx);
        }

        error_msg_out = "Account locked due to too many failed attempts. Try again in " +
                       std::to_string(remaining_minutes) + " minute(s)";
        return AuthResult::USER_LOCKED;
    }

    // Look up user in catalog
    CatalogManager::UserInfo db_user;
    ErrorContext ctx;
    Status status = catalog_->getUserByName(username, db_user, &ctx);

    // SECURITY FIX (CRITICAL-1): Always verify password hash even if user doesn't exist
    // This prevents timing attacks and user enumeration
    std::string actual_hash;
    bool user_exists = (status == Status::OK);
    bool bootstrap_allowed = false;

    if (user_exists) {
        actual_hash = db_user.password_hash;
    } else {
        // Check for bootstrap state (only SYSTEM user or empty catalog)
        std::vector<CatalogManager::UserInfo> all_users;
        Status list_status = catalog_->listUsers(all_users, &ctx);
        bool only_system_user = false;
        if (list_status == Status::OK) {
            if (all_users.empty()) {
                only_system_user = true;
            } else if (all_users.size() == 1 && all_users[0].username == "SYSTEM") {
                only_system_user = true;
            }
        }
        bootstrap_allowed = only_system_user;

        // Use dummy hash for timing resistance (same format as bcrypt)
        // This ensures password verification takes same time whether user exists or not
        actual_hash = "$2a$10$DUMMY.HASH.FOR.TIMING.RESISTANCE.ONLY............................";
    }

    if (bootstrap_allowed) {
        // Fresh database with no real users - allow bootstrap authentication
        ID authkey_id{};
        CatalogManager::AuthKeyInfo authkey_info;
        authkey_info.issuer = "bootstrap";
        authkey_info.status = CatalogManager::AuthKeyStatus::ACTIVE;
        authkey_info.usage_type = CatalogManager::AuthKeyUsage::UNLIMITED;
        Status key_status = catalog_->createAuthKey(authkey_info, authkey_id, &ctx);
        if (key_status != Status::OK) {
            error_msg_out = "Authentication failed";
            return AuthResult::PROVIDER_ERROR;
        }

        login_tracker_->recordSuccessfulLogin(username);

        user_info_out.user_id = generateUuidV7();
        user_info_out.username = username;
        user_info_out.display_name = username;
        user_info_out.email.clear();
        user_info_out.external_groups.clear();
        user_info_out.external_id.clear();
        user_info_out.is_disabled = false;
        user_info_out.is_locked = false;
        user_info_out.is_superuser = true;
        user_info_out.authkey_id = authkey_id;

        LOG_INFO(GENERAL, "Bootstrap authentication for user: %s", username.c_str());
        return AuthResult::SUCCESS;
    }

    // Always verify password (even with dummy hash if user doesn't exist)
    bool password_valid = false;
    try {
        if (!actual_hash.empty()) {
            password_valid = PasswordHash::verifyPassword(password, actual_hash);
        }
    } catch (const std::exception& e) {
        // Log detailed error internally for administrators
        LOG_ERROR(GENERAL, "Password verification error for authentication attempt '%s': %s",
                 username.c_str(), e.what());
        // Return generic error to client (don't reveal internal details)
        error_msg_out = "Authentication failed";
        return AuthResult::PROVIDER_ERROR;
    }

    // Check authentication result
    if (!user_exists || !password_valid) {
        // P0-2: Record failed attempt BEFORE returning error
        login_tracker_->recordFailedAttempt(username);

        // Log detailed error internally (for administrators)
        if (!user_exists) {
            LOG_WARNING(GENERAL, "Login attempt for non-existent user: %s", username.c_str());
        } else {
            LOG_WARNING(GENERAL, "Invalid password for user: %s (failed attempts: %u)",
                       username.c_str(), login_tracker_->getFailedAttemptCount(username));
        }

        // P0-3: Audit log - login failure
        if (audit_logger_) {
            std::string reason = user_exists ? "invalid_password" : "invalid_username";
            AuditEvent event = AuditLogger::createLoginFailureEvent(username, reason);
            ErrorContext audit_ctx;
            audit_logger_->logEvent(event, &audit_ctx);
        }

        // SECURITY FIX (CRITICAL-1): Return GENERIC error message for both cases
        // This prevents user enumeration attacks
        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
    }

    // Check if user is active
    if (!db_user.is_active) {
        // P0-2: Record failed attempt for disabled accounts too
        login_tracker_->recordFailedAttempt(username);

        LOG_WARNING(GENERAL, "Login attempt for disabled user: %s", username.c_str());
        // Return generic error (don't reveal user status)
        error_msg_out = "Invalid username or password";
        return AuthResult::USER_DISABLED;
    }

    // Check if user has password hash set
    if (db_user.password_hash.empty()) {
        // P0-2: Record failed attempt
        login_tracker_->recordFailedAttempt(username);

        LOG_WARNING(GENERAL, "Login attempt for user with no password: %s", username.c_str());
        // Return generic error (don't reveal password status)
        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
    }

    // P0-2: Successful authentication - clear failed attempts
    login_tracker_->recordSuccessfulLogin(username);

    // Create AuthKey for this authentication
    ID authkey_id{};
    CatalogManager::AuthKeyInfo authkey_info;
    authkey_info.issuer = "local";
    authkey_info.status = CatalogManager::AuthKeyStatus::ACTIVE;
    authkey_info.usage_type = CatalogManager::AuthKeyUsage::UNLIMITED;
    Status key_status = catalog_->createAuthKey(authkey_info, authkey_id, &ctx);
    if (key_status != Status::OK) {
        error_msg_out = "Authentication failed";
        return AuthResult::PROVIDER_ERROR;
    }

    // Populate user info
    user_info_out.user_id = db_user.user_id;
    user_info_out.username = db_user.username;
    user_info_out.display_name = db_user.username; // No display name in catalog yet
    user_info_out.email = "";                      // No email in catalog yet
    user_info_out.external_groups.clear();
    user_info_out.external_id = "";
    user_info_out.is_disabled = !db_user.is_active;
    user_info_out.is_locked = false;
    user_info_out.is_superuser = db_user.is_superuser;
    user_info_out.authkey_id = authkey_id;

    LOG_INFO(GENERAL, "Successful authentication for user: %s", username.c_str());
    return AuthResult::SUCCESS;
}

bool LocalAuthProvider::userExists(
    const std::string& username,
    AuthUserInfo& user_info_out)
{
    CatalogManager::UserInfo db_user;
    ErrorContext ctx;
    Status status = catalog_->getUserByName(username, db_user, &ctx);

    if (status != Status::OK) {
        return false;
    }

    // Populate user info
    user_info_out.user_id = db_user.user_id;
    user_info_out.username = db_user.username;
    user_info_out.display_name = db_user.username;
    user_info_out.email = "";
    user_info_out.external_groups.clear();
    user_info_out.external_id = "";
    user_info_out.is_disabled = !db_user.is_active;
    user_info_out.is_locked = false;
    user_info_out.is_superuser = db_user.is_superuser;
    user_info_out.authkey_id = ID{};

    return true;
}

bool LocalAuthProvider::getUserGroups(
    const std::string& username,
    std::vector<std::string>& groups_out)
{
    // Get user ID
    CatalogManager::UserInfo db_user;
    ErrorContext ctx;
    Status status = catalog_->getUserByName(username, db_user, &ctx);

    if (status != Status::OK) {
        return false;
    }

    // Get groups for user
    std::vector<ID> group_ids;
    status = catalog_->getUserGroups(db_user.user_id, group_ids, &ctx);

    if (status != Status::OK) {
        return false;
    }

    // Convert group IDs to names
    groups_out.clear();
    for (const auto& group_id : group_ids) {
        CatalogManager::GroupInfo group_info;
        status = catalog_->getGroup(group_id, group_info, &ctx);
        if (status == Status::OK) {
            groups_out.push_back(group_info.group_name);
        }
    }

    return true;
}

// P0-2: Admin functions for login attempt management
void LocalAuthProvider::clearLoginAttempts(const std::string& username)
{
    if (login_tracker_) {
        login_tracker_->clearAttempts(username);
        LOG_INFO(GENERAL, "Cleared login attempts for user: %s", username.c_str());
    }
}

uint32_t LocalAuthProvider::getFailedAttemptCount(const std::string& username)
{
    if (login_tracker_) {
        return login_tracker_->getFailedAttemptCount(username);
    }
    return 0;
}

// ============================================================================
// LDAPAuthProvider Stub Implementation (Beta - Infrastructure Only)
// ============================================================================

LDAPAuthProvider::LDAPAuthProvider(const Config& config)
    : config_(config)
{
    LOG_INFO(GENERAL, "LDAP authentication provider created (stub - Beta feature)");
    LOG_WARNING(GENERAL, "LDAP authentication is not implemented in Alpha. "
                       "All authentication attempts will fail.");
}

LDAPAuthProvider::~LDAPAuthProvider() = default;

AuthResult LDAPAuthProvider::authenticate(
    const std::string& username,
    const std::string& password,
    AuthUserInfo& user_info_out,
    std::string& error_msg_out)
{
    error_msg_out = "LDAP authentication not implemented (Beta feature)";
    LOG_WARNING(GENERAL, "LDAP authentication attempted but not implemented: %s", username.c_str());
    return AuthResult::NOT_IMPLEMENTED;
}

bool LDAPAuthProvider::userExists(
    const std::string& username,
    AuthUserInfo& user_info_out)
{
    LOG_WARNING(GENERAL, "LDAP userExists() not implemented (Beta feature)");
    return false;
}

bool LDAPAuthProvider::getUserGroups(
    const std::string& username,
    std::vector<std::string>& groups_out)
{
    LOG_WARNING(GENERAL, "LDAP getUserGroups() not implemented (Beta feature)");
    return false;
}

bool LDAPAuthProvider::testConnection(std::string& error_msg_out)
{
    error_msg_out = "LDAP connection not implemented (Beta feature)";
    return false;
}

// ============================================================================
// ActiveDirectoryAuthProvider Stub Implementation (Beta - Infrastructure Only)
// ============================================================================

ActiveDirectoryAuthProvider::ActiveDirectoryAuthProvider(const Config& config)
    : config_(config)
{
    LOG_INFO(GENERAL, "Active Directory authentication provider created (stub - Beta feature)");
    LOG_WARNING(GENERAL, "AD authentication is not implemented in Alpha. "
                       "All authentication attempts will fail.");
}

ActiveDirectoryAuthProvider::~ActiveDirectoryAuthProvider() = default;

AuthResult ActiveDirectoryAuthProvider::authenticate(
    const std::string& username,
    const std::string& password,
    AuthUserInfo& user_info_out,
    std::string& error_msg_out)
{
    error_msg_out = "Active Directory authentication not implemented (Beta feature)";
    LOG_WARNING(GENERAL, "AD authentication attempted but not implemented: %s", username.c_str());
    return AuthResult::NOT_IMPLEMENTED;
}

bool ActiveDirectoryAuthProvider::userExists(
    const std::string& username,
    AuthUserInfo& user_info_out)
{
    LOG_WARNING(GENERAL, "AD userExists() not implemented (Beta feature)");
    return false;
}

bool ActiveDirectoryAuthProvider::getUserGroups(
    const std::string& username,
    std::vector<std::string>& groups_out)
{
    LOG_WARNING(GENERAL, "AD getUserGroups() not implemented (Beta feature)");
    return false;
}

bool ActiveDirectoryAuthProvider::testConnection(std::string& error_msg_out)
{
    error_msg_out = "AD connection not implemented (Beta feature)";
    return false;
}

// ============================================================================
// AuthProviderFactory Implementation
// ============================================================================

std::unique_ptr<AuthProvider> AuthProviderFactory::create(
    AuthProviderType type,
    const std::string& config_json,
    CatalogManager* catalog,
    AuditLogger* audit_logger)
{
    switch (type) {
        case AuthProviderType::LOCAL:
            if (catalog == nullptr) {
                LOG_ERROR(GENERAL, "Cannot create LocalAuthProvider: catalog is null");
                return nullptr;
            }
            return std::make_unique<LocalAuthProvider>(catalog, audit_logger);

        case AuthProviderType::LDAP:
            LOG_WARNING(GENERAL, "LDAP auth provider requested but not implemented (Beta feature)");
            // Parse config_json and create LDAPAuthProvider::Config
            // For now, return stub with default config
            return std::make_unique<LDAPAuthProvider>(LDAPAuthProvider::Config{});

        case AuthProviderType::ACTIVE_DIRECTORY:
            LOG_WARNING(GENERAL, "AD auth provider requested but not implemented (Beta feature)");
            // Parse config_json and create ActiveDirectoryAuthProvider::Config
            // For now, return stub with default config
            return std::make_unique<ActiveDirectoryAuthProvider>(
                ActiveDirectoryAuthProvider::Config{});

        case AuthProviderType::OAUTH2:
        case AuthProviderType::SAML:
        case AuthProviderType::KERBEROS:
        case AuthProviderType::EXTERNAL_SCRIPT:
            LOG_ERROR(GENERAL, "Auth provider type %d not implemented", static_cast<int>(type));
            return nullptr;

        default:
            LOG_ERROR(GENERAL, "Unknown auth provider type: %d", static_cast<int>(type));
            return nullptr;
    }
}

std::unique_ptr<AuthProvider> AuthProviderFactory::createDefault(CatalogManager* catalog,
                                                                  AuditLogger* audit_logger)
{
    return std::make_unique<LocalAuthProvider>(catalog, audit_logger);
}

} // namespace core
} // namespace scratchbird
