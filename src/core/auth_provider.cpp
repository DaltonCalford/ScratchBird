/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/auth_provider.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/password_hash.h"
#include "scratchbird/core/login_attempt_tracker.h"  // P0-2: Account lockout
#include "scratchbird/core/audit_logger.h"           // P0-3: Security audit logging
#include "scratchbird/core/logger.h"
#include "scratchbird/core/telemetry.h"
#include "scratchbird/security/scram_auth.h"

#include <nlohmann/json.hpp>
#include <openssl/crypto.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/crypto.h>
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

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

bool isZeroUuidLocal(const ID& id) {
    for (uint8_t byte : id.bytes) {
        if (byte != 0) {
            return false;
        }
    }
    return true;
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

Counter* legacyAuthUsageCounter() {
    static Counter* counter = nullptr;
    static std::once_flag once;
    std::call_once(once, []() {
        auto& registry = MetricsRegistry::getInstance();
        counter = registry.registerCounter(
            "scratchbird_auth_legacy_method_total",
            "Legacy authentication method usage count",
            {"method"});
    });
    return counter;
}

void recordLegacyAuthUsage(const char* method) {
    Counter* counter = legacyAuthUsageCounter();
    if (!counter || !method || method[0] == '\0') {
        return;
    }
    counter->inc(1.0, {method});
}

void warnLegacyPasswordDeprecated() {
    static std::once_flag once;
    std::call_once(once, []() {
        LOG_WARNING(GENERAL,
                    "Legacy PASSWORD authentication is deprecated; use SCRAM-SHA-256/512 policies");
    });
}

void warnLegacyMd5Deprecated() {
    static std::once_flag once;
    std::call_once(once, []() {
        LOG_WARNING(GENERAL,
                    "Legacy MD5 authentication is deprecated; use SCRAM-SHA-256/512 policies");
    });
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

std::string toUpperAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

bool authKeyScopeAllowsToken(CatalogManager::AuthKeyScope scope) {
    using Scope = CatalogManager::AuthKeyScope;
    return scope == Scope::LOGIN_SESSION ||
           scope == Scope::API_TOKEN ||
           scope == Scope::REATTACH ||
           scope == Scope::SERVICE_ACCOUNT;
}

bool computeAuthKeyTokenProof(const std::string& token_secret,
                              const std::string& username,
                              const ID& authkey_id,
                              const std::vector<uint8_t>& binding,
                              std::vector<uint8_t>& proof_out) {
    proof_out.clear();
    if (token_secret.empty()) {
        return false;
    }

    static constexpr char kTokenProofPrefix[] = "SB-AUTHKEY-TOKEN-V1";

    std::vector<uint8_t> message;
    message.reserve(sizeof(kTokenProofPrefix) - 1 + username.size() +
                    authkey_id.bytes.size() + binding.size());
    message.insert(message.end(),
                   reinterpret_cast<const uint8_t*>(kTokenProofPrefix),
                   reinterpret_cast<const uint8_t*>(kTokenProofPrefix) + (sizeof(kTokenProofPrefix) - 1));
    message.insert(message.end(), username.begin(), username.end());
    message.insert(message.end(), authkey_id.bytes.begin(), authkey_id.bytes.end());
    message.insert(message.end(), binding.begin(), binding.end());

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len = 0;
    if (!HMAC(EVP_sha256(),
              token_secret.data(),
              static_cast<int>(token_secret.size()),
              message.data(),
              message.size(),
              digest,
              &digest_len)) {
        return false;
    }

    proof_out.assign(digest, digest + digest_len);
    return true;
}

std::string trimAscii(std::string value) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!value.empty() && is_space(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && is_space(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

std::string bootstrapTokenFilePath() {
    const char* configured = std::getenv("SCRATCHBIRD_BOOTSTRAP_TOKEN_FILE");
    if (configured && configured[0] != '\0') {
        return configured;
    }
    return "/var/lib/scratchbird/bootstrap.token";
}

std::mutex& bootstrapTokenMutex() {
    static std::mutex mutex;
    return mutex;
}

std::string bootstrapFailureReasonCode(const std::string& error_text) {
    const std::string lowered = [&]() {
        std::string value = error_text;
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }();

    if (lowered.find("mismatch") != std::string::npos) {
        return "proof_mismatch";
    }
    if (lowered.find("permissions") != std::string::npos) {
        return "token_permissions_invalid";
    }
    if (lowered.find("already consumed") != std::string::npos) {
        return "token_already_consumed";
    }
    if (lowered.find("not available") != std::string::npos) {
        return "token_not_available";
    }
    if (lowered.find("empty") != std::string::npos) {
        return "token_empty";
    }
    if (lowered.find("revoke") != std::string::npos) {
        return "token_revoke_failed";
    }
    if (lowered.find("stat failed") != std::string::npos) {
        return "token_stat_failed";
    }
    return "bootstrap_proof_failed";
}

void logBootstrapAuditEvent(AuditLogger* audit_logger,
                            AuditEventType event_type,
                            const std::string& username,
                            bool success,
                            const std::string& phase,
                            const std::string& reason,
                            const ID* user_id = nullptr,
                            const ID* authkey_id = nullptr) {
    if (!audit_logger) {
        return;
    }

    AuditEvent event;
    event.event_type = event_type;
    event.username = username;
    event.success = success;

    Json details = Json::object();
    details["phase"] = phase;
    details["reason"] = reason;
    event.details = details.dump();

    if (user_id) {
        event.user_id = *user_id;
    }
    if (authkey_id) {
        event.authkey_id = *authkey_id;
    }

    ErrorContext audit_ctx;
    (void)audit_logger->logEvent(event, &audit_ctx);
}

bool validateBootstrapTokenPermissions(const std::string& token_path, std::string& error_out) {
#ifdef _WIN32
    (void)token_path;
    return true;
#else
    struct stat st{};
    if (::stat(token_path.c_str(), &st) != 0) {
        error_out = "Bootstrap token file stat failed: " + std::string(std::strerror(errno));
        return false;
    }
    if (!S_ISREG(st.st_mode)) {
        error_out = "Bootstrap token path is not a regular file";
        return false;
    }
    if ((st.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
        error_out = "Bootstrap token file permissions must be 0600";
        return false;
    }
    return true;
#endif
}

bool consumeBootstrapTokenFile(const std::string& token_path, std::string& error_out) {
    std::fstream file(token_path, std::ios::in | std::ios::out | std::ios::binary);
    if (file.is_open()) {
        file.seekg(0, std::ios::end);
        const std::streamoff size = file.tellg();
        if (size > 0) {
            file.seekp(0, std::ios::beg);
            std::string zeros(static_cast<size_t>(size), '\0');
            file.write(zeros.data(), static_cast<std::streamsize>(zeros.size()));
            file.flush();
        }
    }

    std::error_code ec;
    const bool removed = std::filesystem::remove(token_path, ec);
    if (ec) {
        error_out = "Failed to revoke bootstrap token file: " + ec.message();
        return false;
    }
    if (!removed) {
        error_out = "Bootstrap token file already consumed";
        return false;
    }
    return true;
}

bool verifyAndConsumeBootstrapProof(const std::string& presented_token, std::string& error_out) {
    std::lock_guard<std::mutex> lock(bootstrapTokenMutex());

    const std::string token_path = bootstrapTokenFilePath();

    if (!validateBootstrapTokenPermissions(token_path, error_out)) {
        return false;
    }

    std::ifstream input(token_path);
    if (!input.is_open()) {
        error_out = "Bootstrap token file not available";
        return false;
    }

    std::string expected_token;
    std::getline(input, expected_token);
    expected_token = trimAscii(expected_token);
    if (expected_token.empty()) {
        error_out = "Bootstrap token file is empty";
        return false;
    }

    if (!timingSafeEqual(expected_token, presented_token)) {
        error_out = "Bootstrap proof mismatch";
        return false;
    }

    return consumeBootstrapTokenFile(token_path, error_out);
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
    warnLegacyPasswordDeprecated();
    recordLegacyAuthUsage("password");

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
    CatalogManager::BootstrapState bootstrap_state = CatalogManager::BootstrapState::UNINITIALIZED;

    Status bootstrap_state_status = catalog_->getBootstrapState(bootstrap_state, &ctx);
    if (bootstrap_state_status != Status::OK) {
        error_msg_out = "Authentication failed";
        return AuthResult::PROVIDER_ERROR;
    }

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
        bootstrap_allowed = only_system_user &&
                            bootstrap_state == CatalogManager::BootstrapState::UNINITIALIZED;

        // Use dummy hash for timing resistance (same format as bcrypt)
        // This ensures password verification takes same time whether user exists or not
        actual_hash = "$2a$10$DUMMY.HASH.FOR.TIMING.RESISTANCE.ONLY............................";
    }

    if (bootstrap_allowed) {
        logBootstrapAuditEvent(audit_logger_,
                               AuditEventType::BOOTSTRAP_ATTEMPT,
                               username,
                               false,
                               "auth",
                               "bootstrap_candidate");

        ErrorContext claim_ctx;
        Status claim_status = catalog_->claimBootstrapWindow(&claim_ctx);
        if (claim_status != Status::OK) {
            login_tracker_->recordFailedAttempt(username);
            const std::string reason = (claim_status == Status::CONSTRAINT_VIOLATION)
                ? "bootstrap_claim_conflict"
                : "bootstrap_claim_failed";
            logBootstrapAuditEvent(audit_logger_,
                                   AuditEventType::BOOTSTRAP_FAILURE,
                                   username,
                                   false,
                                   "claim",
                                   reason);
            LOG_WARNING(GENERAL,
                        "Bootstrap claim denied for user %s: %s",
                        username.c_str(),
                        claim_ctx.message.c_str());
            error_msg_out = "Invalid username or password";
            return AuthResult::INVALID_CREDENTIALS;
        }

        auto release_bootstrap_window = [&]() -> bool {
            ErrorContext release_ctx;
            Status release_status = catalog_->releaseBootstrapWindow(&release_ctx);
            if (release_status != Status::OK) {
                LOG_ERROR(GENERAL,
                          "Failed to release bootstrap claim window: %s",
                          release_ctx.message.c_str());
                logBootstrapAuditEvent(audit_logger_,
                                       AuditEventType::BOOTSTRAP_FAILURE,
                                       username,
                                       false,
                                       "release",
                                       "bootstrap_claim_release_failed");
                return false;
            }
            return true;
        };

        std::string bootstrap_error;
        if (!verifyAndConsumeBootstrapProof(password, bootstrap_error)) {
            (void)release_bootstrap_window();
            login_tracker_->recordFailedAttempt(username);
            logBootstrapAuditEvent(audit_logger_,
                                   AuditEventType::BOOTSTRAP_FAILURE,
                                   username,
                                   false,
                                   "proof",
                                   bootstrapFailureReasonCode(bootstrap_error));
            LOG_WARNING(GENERAL,
                        "Bootstrap authentication denied for user %s: %s",
                        username.c_str(),
                        bootstrap_error.c_str());
            error_msg_out = "Invalid username or password";
            return AuthResult::INVALID_CREDENTIALS;
        }

        logBootstrapAuditEvent(audit_logger_,
                               AuditEventType::BOOTSTRAP_REVOKED,
                               username,
                               true,
                               "token",
                               "bootstrap_token_consumed");

        // Fresh database bootstrap proof accepted.
        // Bind to SYSTEM user so the session can be created from catalog IDs.
        ID system_user_id = catalog_->getSystemUserId(&ctx);
        if (isZeroUuidLocal(system_user_id))
        {
            (void)release_bootstrap_window();
            logBootstrapAuditEvent(audit_logger_,
                                   AuditEventType::BOOTSTRAP_FAILURE,
                                   username,
                                   false,
                                   "resolve",
                                   "system_user_missing");
            error_msg_out = "Authentication failed";
            return AuthResult::PROVIDER_ERROR;
        }

        ID authkey_id{};
        CatalogManager::AuthKeyInfo authkey_info;
        authkey_info.issuer = "bootstrap";
        authkey_info.status = CatalogManager::AuthKeyStatus::ACTIVE;
        authkey_info.usage_type = CatalogManager::AuthKeyUsage::UNLIMITED;
        authkey_info.scope = CatalogManager::AuthKeyScope::LOGIN_SESSION;
        Status key_status = catalog_->createAuthKey(authkey_info, authkey_id, &ctx);
        if (key_status != Status::OK) {
            (void)release_bootstrap_window();
            logBootstrapAuditEvent(audit_logger_,
                                   AuditEventType::BOOTSTRAP_FAILURE,
                                   username,
                                   false,
                                   "authkey",
                                   "authkey_create_failed",
                                   &system_user_id);
            error_msg_out = "Authentication failed";
            return AuthResult::PROVIDER_ERROR;
        }

        if (!release_bootstrap_window()) {
            error_msg_out = "Authentication failed";
            return AuthResult::PROVIDER_ERROR;
        }

        login_tracker_->recordSuccessfulLogin(username);

        user_info_out.user_id = system_user_id;
        user_info_out.username = "SYSTEM";
        user_info_out.display_name = user_info_out.username;
        user_info_out.email.clear();
        user_info_out.external_groups.clear();
        user_info_out.external_id.clear();
        user_info_out.is_disabled = false;
        user_info_out.is_locked = false;
        user_info_out.is_superuser = true;
        user_info_out.authkey_id = authkey_id;

        logBootstrapAuditEvent(audit_logger_,
                               AuditEventType::BOOTSTRAP_SUCCESS,
                               username,
                               true,
                               "auth",
                               "bootstrap_authenticated",
                               &system_user_id,
                               &authkey_id);

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
    authkey_info.scope = CatalogManager::AuthKeyScope::LOGIN_SESSION;
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
    warnLegacyMd5Deprecated();
    recordLegacyAuthUsage("md5");

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
    authkey_info.scope = CatalogManager::AuthKeyScope::LOGIN_SESSION;
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
    authkey_info.scope = CatalogManager::AuthKeyScope::LOGIN_SESSION;
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

AuthResult LocalAuthProvider::authenticateToken(
    const std::string& username,
    const ID& authkey_id,
    const std::vector<uint8_t>& proof,
    const std::vector<uint8_t>& binding,
    AuthUserInfo& user_info_out,
    std::string& error_msg_out)
{
    if (login_tracker_->isAccountLocked(username)) {
        uint64_t remaining_ms = login_tracker_->getLockoutTimeRemaining(username);
        uint32_t remaining_minutes = static_cast<uint32_t>((remaining_ms + 59999) / 60000);

        LOG_WARNING(GENERAL, "Token login attempt for locked account: %s (locked for %u more minutes)",
                    username.c_str(), remaining_minutes);
        error_msg_out = "Account locked due to too many failed attempts. Try again in " +
                        std::to_string(remaining_minutes) + " minute(s)";
        return AuthResult::USER_LOCKED;
    }

    ErrorContext ctx;
    CatalogManager::AuthKeyInfo authkey{};
    Status authkey_status = catalog_->getAuthKey(authkey_id, authkey, &ctx);
    if (authkey_status != Status::OK ||
        authkey.status != CatalogManager::AuthKeyStatus::ACTIVE ||
        !authKeyScopeAllowsToken(authkey.scope)) {
        login_tracker_->recordFailedAttempt(username);
        LOG_WARNING(GENERAL, "Invalid token auth attempt for user: %s", username.c_str());
        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
    }

    if (authkey.binding_kind != CatalogManager::AuthKeyBindingKind::CLIENT_NONCE ||
        authkey.binding_value.empty()) {
        login_tracker_->recordFailedAttempt(username);
        LOG_WARNING(GENERAL,
                    "Token auth denied for user %s due to unsupported key binding policy",
                    username.c_str());
        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
    }

    CatalogManager::UserInfo db_user{};
    bool user_loaded = false;
    if (binding.size() == authkey_id.bytes.size()) {
        ID bound_user_id{};
        std::memcpy(bound_user_id.bytes.data(), binding.data(), binding.size());
        if (catalog_->getUser(bound_user_id, db_user, &ctx) == Status::OK) {
            user_loaded = true;
        }
    }
    if (!user_loaded) {
        if (catalog_->getUserByName(username, db_user, &ctx) == Status::OK) {
            user_loaded = true;
        }
    }

    if (!user_loaded ||
        toUpperAscii(db_user.username) != toUpperAscii(username) ||
        !db_user.is_active) {
        login_tracker_->recordFailedAttempt(username);
        LOG_WARNING(GENERAL, "Token auth failed user resolution for %s", username.c_str());
        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
    }

    std::vector<uint8_t> expected_proof;
    if (!computeAuthKeyTokenProof(authkey.binding_value,
                                  db_user.username,
                                  authkey_id,
                                  binding,
                                  expected_proof) ||
        !timingSafeEqual(expected_proof, proof)) {
        login_tracker_->recordFailedAttempt(username);
        LOG_WARNING(GENERAL, "Token proof mismatch for user: %s", username.c_str());
        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
    }

    Status consume_status = catalog_->consumeAuthKey(authkey_id, 1, &ctx);
    if (consume_status != Status::OK) {
        login_tracker_->recordFailedAttempt(username);
        LOG_WARNING(GENERAL, "Token consume denied for user %s: %s",
                    username.c_str(), ctx.message.c_str());
        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
    }

    login_tracker_->recordSuccessfulLogin(username);

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

    LOG_INFO(GENERAL, "Successful token authentication for user: %s", username.c_str());
    return AuthResult::SUCCESS;
}

AuthResult LocalAuthProvider::authenticatePeer(
    const std::string& username,
    uint32_t peer_uid,
    uint32_t peer_gid,
    uint32_t peer_pid,
    AuthUserInfo& user_info_out,
    std::string& error_msg_out)
{
    (void)peer_pid;
    if (login_tracker_->isAccountLocked(username)) {
        uint64_t remaining_ms = login_tracker_->getLockoutTimeRemaining(username);
        uint32_t remaining_minutes = static_cast<uint32_t>((remaining_ms + 59999) / 60000);

        LOG_WARNING(GENERAL,
                    "Peer login attempt for locked account: %s (locked for %u more minutes)",
                    username.c_str(), remaining_minutes);
        error_msg_out = "Account locked due to too many failed attempts. Try again in " +
                        std::to_string(remaining_minutes) + " minute(s)";
        return AuthResult::USER_LOCKED;
    }

    ErrorContext ctx;
    CatalogManager::UserInfo db_user{};
    if (catalog_->getUserByName(username, db_user, &ctx) != Status::OK || !db_user.is_active) {
        login_tracker_->recordFailedAttempt(username);
        LOG_WARNING(GENERAL, "Peer auth denied for unknown/disabled user: %s", username.c_str());
        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
    }

    CatalogManager::PrincipalResolutionRequest req{};
    req.presented_principal_name = db_user.username;
    req.has_peer_uid = true;
    req.peer_uid = peer_uid;
    req.has_peer_gid = true;
    req.peer_gid = peer_gid;

    CatalogManager::PrincipalAccountCatalogInfo account{};
    Status resolve_status = catalog_->resolvePrincipalAccount(req, account, &ctx);
    if (resolve_status != Status::OK ||
        (account.source_scope_kind != CatalogManager::SourceScopeKind::PEER_UID &&
         account.source_scope_kind != CatalogManager::SourceScopeKind::PEER_GID)) {
        login_tracker_->recordFailedAttempt(username);
        LOG_WARNING(GENERAL,
                    "Peer auth mapping denied for user %s (uid=%u gid=%u)",
                    username.c_str(),
                    static_cast<unsigned>(peer_uid),
                    static_cast<unsigned>(peer_gid));
        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
    }

    ID authkey_id{};
    CatalogManager::AuthKeyInfo authkey_info;
    authkey_info.issuer = "peer";
    authkey_info.status = CatalogManager::AuthKeyStatus::ACTIVE;
    authkey_info.usage_type = CatalogManager::AuthKeyUsage::UNLIMITED;
    authkey_info.scope = CatalogManager::AuthKeyScope::LOGIN_SESSION;
    Status key_status = catalog_->createAuthKey(authkey_info, authkey_id, &ctx);
    if (key_status != Status::OK) {
        error_msg_out = "Authentication failed";
        return AuthResult::PROVIDER_ERROR;
    }

    login_tracker_->recordSuccessfulLogin(username);

    user_info_out.user_id = db_user.user_id;
    user_info_out.username = db_user.username;
    user_info_out.display_name = db_user.username;
    user_info_out.email.clear();
    user_info_out.external_groups.clear();
    user_info_out.external_id.clear();
    user_info_out.is_disabled = !db_user.is_active;
    user_info_out.is_locked = false;
    user_info_out.is_superuser = db_user.is_superuser;
    user_info_out.authkey_id = authkey_id;

    LOG_INFO(GENERAL,
             "Successful peer authentication for user: %s (uid=%u gid=%u)",
             username.c_str(),
             static_cast<unsigned>(peer_uid),
             static_cast<unsigned>(peer_gid));
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
