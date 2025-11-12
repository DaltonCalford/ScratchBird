#include "scratchbird/core/auth_provider.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/password_hash.h"
#include "scratchbird/core/logger.h"

namespace scratchbird {
namespace core {

// ============================================================================
// LocalAuthProvider Implementation (Alpha - Fully Implemented)
// ============================================================================

LocalAuthProvider::LocalAuthProvider(CatalogManager* catalog)
    : catalog_(catalog)
{
    if (catalog_ == nullptr) {
        throw std::invalid_argument("LocalAuthProvider: catalog cannot be null");
    }
}

LocalAuthProvider::~LocalAuthProvider() = default;

AuthResult LocalAuthProvider::authenticate(
    const std::string& username,
    const std::string& password,
    AuthUserInfo& user_info_out,
    std::string& error_msg_out)
{
    // Look up user in catalog
    CatalogManager::UserInfo db_user;
    ErrorContext ctx;
    Status status = catalog_->getUserByName(username, db_user, &ctx);

    if (status != Status::OK) {
        error_msg_out = "User not found: " + username;
        return AuthResult::INVALID_CREDENTIALS;
    }

    // Check if user is active
    if (!db_user.is_active) {
        error_msg_out = "User account is disabled";
        return AuthResult::USER_DISABLED;
    }

    // Verify password (Phase 3.0 - BCrypt)
    if (!db_user.password_hash.empty()) {
        try {
            bool password_valid = PasswordHash::verifyPassword(password, db_user.password_hash);
            if (!password_valid) {
                error_msg_out = "Invalid password";
                LOG_WARNING(GENERAL, "Failed login attempt for user: %s", username.c_str());
                return AuthResult::INVALID_CREDENTIALS;
            }
        } catch (const std::exception& e) {
            error_msg_out = std::string("Password verification error: ") + e.what();
            LOG_ERROR(GENERAL, "Password verification error for user %s: %s",
                     username.c_str(), e.what());
            return AuthResult::PROVIDER_ERROR;
        }
    } else if (!password.empty()) {
        // Password hash is empty but user provided password
        error_msg_out = "User has no password set";
        return AuthResult::INVALID_CREDENTIALS;
    }

    // Populate user info
    user_info_out.username = db_user.username;
    user_info_out.display_name = db_user.username; // No display name in catalog yet
    user_info_out.email = "";                      // No email in catalog yet
    user_info_out.external_groups.clear();
    user_info_out.external_id = "";
    user_info_out.is_disabled = !db_user.is_active;
    user_info_out.is_locked = false;

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
    user_info_out.username = db_user.username;
    user_info_out.display_name = db_user.username;
    user_info_out.email = "";
    user_info_out.external_groups.clear();
    user_info_out.external_id = "";
    user_info_out.is_disabled = !db_user.is_active;
    user_info_out.is_locked = false;

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
    CatalogManager* catalog)
{
    switch (type) {
        case AuthProviderType::LOCAL:
            if (catalog == nullptr) {
                LOG_ERROR(GENERAL, "Cannot create LocalAuthProvider: catalog is null");
                return nullptr;
            }
            return std::make_unique<LocalAuthProvider>(catalog);

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

std::unique_ptr<AuthProvider> AuthProviderFactory::createDefault(CatalogManager* catalog)
{
    return std::make_unique<LocalAuthProvider>(catalog);
}

} // namespace core
} // namespace scratchbird
