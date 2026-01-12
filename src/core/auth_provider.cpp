#include "scratchbird/core/auth_provider.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/password_hash.h"
#include "scratchbird/core/login_attempt_tracker.h"  // P0-2: Account lockout
#include "scratchbird/core/audit_logger.h"           // P0-3: Security audit logging
#include "scratchbird/core/logger.h"
#include "scratchbird/security/scram_auth.h"

#include <nlohmann/json.hpp>
#include <openssl/crypto.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>

namespace scratchbird {
namespace core {
namespace {

using Json = nlohmann::json;

struct ScramRecord {
    bool valid = false;
    uint32_t iterations = 0;
    std::vector<uint8_t> salt;
    std::vector<uint8_t> stored_key;
    std::vector<uint8_t> server_key;
};

struct ParsedPasswordHashes {
    bool is_json = false;
    std::string bcrypt;
    std::string md5;
    ScramRecord scram256;
    ScramRecord scram512;
};

std::string toHexLower(const unsigned char* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

std::string computeMd5Hex(const std::string& input) {
    unsigned char hash[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);
    return toHexLower(hash, MD5_DIGEST_LENGTH);
}

std::string computeMd5ResponseFromStored(const std::string& stored_md5, const uint8_t salt[4]) {
    if (stored_md5.size() != 35 || stored_md5.rfind("md5", 0) != 0) {
        return {};
    }
    std::string input = stored_md5.substr(3) + std::string(reinterpret_cast<const char*>(salt), 4);
    return "md5" + computeMd5Hex(input);
}

void parseScramEntry(const Json& entry, size_t key_len, ScramRecord& out) {
    if (!entry.is_object()) {
        return;
    }
    if (!entry.contains("iterations") || !entry["iterations"].is_number_unsigned()) {
        return;
    }
    if (!entry.contains("salt") || !entry.contains("stored_key") || !entry.contains("server_key")) {
        return;
    }
    if (!entry["salt"].is_string() || !entry["stored_key"].is_string() || !entry["server_key"].is_string()) {
        return;
    }

    out.iterations = entry["iterations"].get<uint32_t>();
    out.salt = security::base64Decode(entry["salt"].get<std::string>());
    out.stored_key = security::base64Decode(entry["stored_key"].get<std::string>());
    out.server_key = security::base64Decode(entry["server_key"].get<std::string>());

    if (out.salt.empty() || out.stored_key.size() != key_len || out.server_key.size() != key_len) {
        out = ScramRecord{};
        return;
    }
    out.valid = true;
}

ParsedPasswordHashes parsePasswordHashes(const std::string& password_hash) {
    ParsedPasswordHashes out;
    if (password_hash.empty()) {
        return out;
    }

    if (password_hash.front() != '{') {
        out.bcrypt = password_hash;
        return out;
    }

    try {
        Json doc = Json::parse(password_hash);
        if (!doc.is_object()) {
            out.bcrypt = password_hash;
            return out;
        }
        out.is_json = true;
        if (doc.contains("bcrypt") && doc["bcrypt"].is_string()) {
            out.bcrypt = doc["bcrypt"].get<std::string>();
        }
        if (doc.contains("md5") && doc["md5"].is_string()) {
            out.md5 = doc["md5"].get<std::string>();
        }
        if (doc.contains("scram") && doc["scram"].is_object()) {
            const auto& scram = doc["scram"];
            if (scram.contains("sha256")) {
                parseScramEntry(scram["sha256"], 32, out.scram256);
            }
            if (scram.contains("sha512")) {
                parseScramEntry(scram["sha512"], 64, out.scram512);
            }
        }
    } catch (const Json::exception&) {
        out.bcrypt = password_hash;
    }

    return out;
}

const EVP_MD* scramDigest(security::ScramAlgorithm algorithm) {
    return (algorithm == security::ScramAlgorithm::SHA_256) ? EVP_sha256() : EVP_sha512();
}

bool scramHmac(const std::vector<uint8_t>& key,
               const std::string& message,
               security::ScramAlgorithm algorithm,
               std::vector<uint8_t>& out) {
    unsigned int len = 0;
    unsigned char buf[EVP_MAX_MD_SIZE];
    if (!HMAC(scramDigest(algorithm),
              key.data(),
              static_cast<int>(key.size()),
              reinterpret_cast<const unsigned char*>(message.data()),
              message.size(),
              buf,
              &len)) {
        return false;
    }
    out.assign(buf, buf + len);
    return true;
}

bool scramHash(const std::vector<uint8_t>& input,
               security::ScramAlgorithm algorithm,
               std::vector<uint8_t>& out) {
    unsigned char buf[SHA512_DIGEST_LENGTH];
    if (algorithm == security::ScramAlgorithm::SHA_256) {
        SHA256(input.data(), input.size(), buf);
        out.assign(buf, buf + SHA256_DIGEST_LENGTH);
        return true;
    }
    SHA512(input.data(), input.size(), buf);
    out.assign(buf, buf + SHA512_DIGEST_LENGTH);
    return true;
}

bool timingSafeEqual(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

bool timingSafeEqual(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) {
        return false;
    }
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

ScramRecord makeDummyScramRecord(security::ScramAlgorithm algorithm) {
    ScramRecord rec;
    size_t key_len = (algorithm == security::ScramAlgorithm::SHA_256) ? 32 : 64;
    rec.iterations = 4096;
    rec.salt.resize(16);
    rec.stored_key.resize(key_len);
    rec.server_key.resize(key_len);
    RAND_bytes(rec.salt.data(), static_cast<int>(rec.salt.size()));
    RAND_bytes(rec.stored_key.data(), static_cast<int>(rec.stored_key.size()));
    RAND_bytes(rec.server_key.data(), static_cast<int>(rec.server_key.size()));
    rec.valid = true;
    return rec;
}

}  // namespace

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
        auto parsed = parsePasswordHashes(actual_hash);
        if (parsed.is_json && !parsed.bcrypt.empty()) {
            actual_hash = parsed.bcrypt;
        }
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

AuthResult LocalAuthProvider::authenticateMd5(
    const std::string& username,
    const uint8_t salt[4],
    const std::string& client_response,
    AuthUserInfo& user_info_out,
    std::string& error_msg_out)
{
    if (login_tracker_->isAccountLocked(username)) {
        uint64_t remaining_ms = login_tracker_->getLockoutTimeRemaining(username);
        uint32_t remaining_minutes = static_cast<uint32_t>((remaining_ms + 59999) / 60000);

        LOG_WARNING(GENERAL, "Login attempt for locked account: %s (locked for %u more minutes)",
                   username.c_str(), remaining_minutes);

        if (audit_logger_) {
            AuditEvent event = AuditLogger::createLoginFailureEvent(username, "account_locked");
            ErrorContext audit_ctx;
            audit_logger_->logEvent(event, &audit_ctx);
        }

        error_msg_out = "Account locked due to too many failed attempts. Try again in " +
                       std::to_string(remaining_minutes) + " minute(s)";
        return AuthResult::USER_LOCKED;
    }

    CatalogManager::UserInfo db_user;
    ErrorContext ctx;
    Status status = catalog_->getUserByName(username, db_user, &ctx);
    bool user_exists = (status == Status::OK);

    std::string stored_md5;
    bool md5_available = false;
    if (user_exists) {
        auto parsed = parsePasswordHashes(db_user.password_hash);
        stored_md5 = parsed.md5;
        md5_available = !stored_md5.empty();
    }
    if (!md5_available) {
        stored_md5 = "md5" + std::string(32, '0');
    }

    std::string expected = computeMd5ResponseFromStored(stored_md5, salt);
    bool md5_valid = !expected.empty() && timingSafeEqual(expected, client_response);

    if (!user_exists || !md5_valid) {
        login_tracker_->recordFailedAttempt(username);

        if (!user_exists) {
            LOG_WARNING(GENERAL, "Login attempt for non-existent user: %s", username.c_str());
        } else {
            LOG_WARNING(GENERAL, "Invalid MD5 response for user: %s (failed attempts: %u)",
                       username.c_str(), login_tracker_->getFailedAttemptCount(username));
        }

        if (audit_logger_) {
            std::string reason = user_exists ? "invalid_password" : "invalid_username";
            AuditEvent event = AuditLogger::createLoginFailureEvent(username, reason);
            ErrorContext audit_ctx;
            audit_logger_->logEvent(event, &audit_ctx);
        }

        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
    }

    if (!db_user.is_active) {
        login_tracker_->recordFailedAttempt(username);
        LOG_WARNING(GENERAL, "Login attempt for disabled user: %s", username.c_str());
        error_msg_out = "Invalid username or password";
        return AuthResult::USER_DISABLED;
    }

    if (!md5_available) {
        login_tracker_->recordFailedAttempt(username);
        LOG_WARNING(GENERAL, "Login attempt for user without MD5 credential: %s", username.c_str());
        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
    }

    login_tracker_->recordSuccessfulLogin(username);

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

    user_info_out.user_id = db_user.user_id;
    user_info_out.username = db_user.username;
    user_info_out.display_name = db_user.username;
    user_info_out.email = "";
    user_info_out.external_groups.clear();
    user_info_out.external_id = "";
    user_info_out.is_disabled = !db_user.is_active;
    user_info_out.is_locked = false;
    user_info_out.is_superuser = db_user.is_superuser;
    user_info_out.authkey_id = authkey_id;

    LOG_INFO(GENERAL, "Successful MD5 authentication for user: %s", username.c_str());
    return AuthResult::SUCCESS;
}

AuthResult LocalAuthProvider::beginScramAuth(
    const std::string& username,
    const std::string& client_first,
    security::ScramAlgorithm algorithm,
    ScramAuthState& state_out,
    std::string& server_first_out,
    std::string& error_msg_out)
{
    if (login_tracker_->isAccountLocked(username)) {
        uint64_t remaining_ms = login_tracker_->getLockoutTimeRemaining(username);
        uint32_t remaining_minutes = static_cast<uint32_t>((remaining_ms + 59999) / 60000);

        LOG_WARNING(GENERAL, "Login attempt for locked account: %s (locked for %u more minutes)",
                   username.c_str(), remaining_minutes);

        if (audit_logger_) {
            AuditEvent event = AuditLogger::createLoginFailureEvent(username, "account_locked");
            ErrorContext audit_ctx;
            audit_logger_->logEvent(event, &audit_ctx);
        }

        error_msg_out = "Account locked due to too many failed attempts. Try again in " +
                       std::to_string(remaining_minutes) + " minute(s)";
        return AuthResult::USER_LOCKED;
    }

    security::ScramClientFirst parsed;
    if (!security::parseClientFirst(client_first, parsed)) {
        error_msg_out = "Invalid SCRAM message";
        return AuthResult::INVALID_CREDENTIALS;
    }

    if (!username.empty() && parsed.username != username) {
        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
    }

    if (parsed.gs2_flag == 'p') {
        error_msg_out = "SCRAM channel binding not supported";
        return AuthResult::NOT_IMPLEMENTED;
    }

    CatalogManager::UserInfo db_user;
    ErrorContext ctx;
    Status status = catalog_->getUserByName(parsed.username, db_user, &ctx);
    bool user_exists = (status == Status::OK);

    ScramRecord record;
    if (user_exists) {
        auto parsed_hashes = parsePasswordHashes(db_user.password_hash);
        record = (algorithm == security::ScramAlgorithm::SHA_256)
            ? parsed_hashes.scram256
            : parsed_hashes.scram512;
    }
    if (!record.valid) {
        record = makeDummyScramRecord(algorithm);
        user_exists = false;
    }

    std::string server_nonce = security::generateNonce();
    std::string full_nonce = parsed.client_nonce + server_nonce;
    std::string salt_b64 = security::base64Encode(record.salt);
    std::string server_first = "r=" + full_nonce + ",s=" + salt_b64 +
                               ",i=" + std::to_string(record.iterations);

    state_out.username = parsed.username;
    state_out.algorithm = algorithm;
    state_out.iterations = record.iterations;
    state_out.salt = record.salt;
    state_out.stored_key = record.stored_key;
    state_out.server_key = record.server_key;
    state_out.client_first_bare = parsed.client_first_bare;
    state_out.server_first = server_first;
    state_out.full_nonce = full_nonce;
    state_out.user_exists = user_exists;
    if (user_exists) {
        state_out.user_id = db_user.user_id;
        state_out.is_active = db_user.is_active;
        state_out.is_superuser = db_user.is_superuser;
    }

    server_first_out = server_first;
    return AuthResult::SUCCESS;
}

AuthResult LocalAuthProvider::finishScramAuth(
    ScramAuthState& state,
    const std::string& client_final,
    AuthUserInfo& user_info_out,
    std::string& server_final_out,
    std::string& error_msg_out)
{
    security::ScramClientFinal parsed;
    if (!security::parseClientFinal(client_final, parsed)) {
        error_msg_out = "Invalid SCRAM message";
        return AuthResult::INVALID_CREDENTIALS;
    }

    std::string auth_message = state.client_first_bare + "," +
                               state.server_first + "," +
                               parsed.without_proof;

    std::vector<uint8_t> client_signature;
    scramHmac(state.stored_key, auth_message, state.algorithm, client_signature);

    std::vector<uint8_t> client_proof = security::base64Decode(parsed.proof);
    if (client_proof.size() != client_signature.size()) {
        client_proof.assign(client_signature.size(), 0);
    }

    std::vector<uint8_t> client_key = client_proof;
    security::xorBytes(client_key, client_signature);

    std::vector<uint8_t> stored_key_check;
    scramHash(client_key, state.algorithm, stored_key_check);

    bool nonce_ok = (parsed.nonce == state.full_nonce);
    bool stored_match = timingSafeEqual(stored_key_check, state.stored_key);
    bool auth_ok = nonce_ok && stored_match && state.user_exists;

    std::vector<uint8_t> server_signature;
    scramHmac(state.server_key, auth_message, state.algorithm, server_signature);
    server_final_out = "v=" + security::base64Encode(server_signature);

    if (!auth_ok) {
        login_tracker_->recordFailedAttempt(state.username);
        if (!state.user_exists) {
            LOG_WARNING(GENERAL, "SCRAM login attempt for non-existent user: %s",
                       state.username.c_str());
        } else {
            LOG_WARNING(GENERAL, "Invalid SCRAM proof for user: %s (failed attempts: %u)",
                       state.username.c_str(),
                       login_tracker_->getFailedAttemptCount(state.username));
        }
        if (audit_logger_) {
            std::string reason = state.user_exists ? "invalid_password" : "invalid_username";
            AuditEvent event = AuditLogger::createLoginFailureEvent(state.username, reason);
            ErrorContext audit_ctx;
            audit_logger_->logEvent(event, &audit_ctx);
        }
        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
    }

    if (!state.is_active) {
        login_tracker_->recordFailedAttempt(state.username);
        LOG_WARNING(GENERAL, "SCRAM login attempt for disabled user: %s", state.username.c_str());
        error_msg_out = "Invalid username or password";
        return AuthResult::USER_DISABLED;
    }

    login_tracker_->recordSuccessfulLogin(state.username);

    ErrorContext ctx;
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

    user_info_out.user_id = state.user_id;
    user_info_out.username = state.username;
    user_info_out.display_name = state.username;
    user_info_out.email = "";
    user_info_out.external_groups.clear();
    user_info_out.external_id = "";
    user_info_out.is_disabled = !state.is_active;
    user_info_out.is_locked = false;
    user_info_out.is_superuser = state.is_superuser;
    user_info_out.authkey_id = authkey_id;

    LOG_INFO(GENERAL, "Successful SCRAM authentication for user: %s", state.username.c_str());
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
