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
#include "scratchbird/core/file_permissions.h"
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
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string_view>
#ifndef _WIN32
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
    std::string firebird_legacy_enc;
    ScramRecord scram256;
    ScramRecord scram512;
};

constexpr const char* kFirebirdLegacySecretPrefix = "{fb_legacy_enc}";

struct CatalogAuthContext {
    bool resolved = false;
    bool has_policy = false;
    CatalogManager::PrincipalAccountCatalogInfo account;
    CatalogManager::AuthPolicyCatalogInfo policy;
};

enum class AuthRateBucket : uint8_t {
    GENERAL = 0,
    SCRAM_BEGIN = 1,
    SCRAM_FINISH = 2
};

struct AuthAttemptScope {
    bool has_peer_identity = false;
    uint32_t peer_uid = 0;
    uint32_t peer_gid = 0;
    uint32_t peer_pid = 0;
    AuthRateBucket rate_bucket = AuthRateBucket::GENERAL;
};

struct ParsedLockoutReason {
    bool valid = false;
    uint64_t deadline = 0;
    bool has_scope = false;
    std::string scope;
    bool has_bucket = false;
    AuthRateBucket bucket = AuthRateBucket::GENERAL;
};

bool isZeroUuidLocal(const ID& id);

bool equalsIgnoreCaseAscii(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (size_t i = 0; i < lhs.size(); ++i) {
        const unsigned char a = static_cast<unsigned char>(lhs[i]);
        const unsigned char b = static_cast<unsigned char>(rhs[i]);
        if (std::tolower(a) != std::tolower(b)) {
            return false;
        }
    }
    return true;
}

uint64_t catalogNowTicks() {
    return static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());
}

uint64_t millisecondsToCatalogTicks(uint32_t ms) {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::milliseconds(ms))
            .count());
}

std::string rateBucketToken(AuthRateBucket bucket) {
    switch (bucket) {
        case AuthRateBucket::GENERAL:
            return "general";
        case AuthRateBucket::SCRAM_BEGIN:
            return "scram_begin";
        case AuthRateBucket::SCRAM_FINISH:
            return "scram_finish";
    }
    return "general";
}

bool parseRateBucketToken(std::string_view token, AuthRateBucket& bucket_out) {
    if (token == "general") {
        bucket_out = AuthRateBucket::GENERAL;
        return true;
    }
    if (token == "scram_begin") {
        bucket_out = AuthRateBucket::SCRAM_BEGIN;
        return true;
    }
    if (token == "scram_finish") {
        bucket_out = AuthRateBucket::SCRAM_FINISH;
        return true;
    }
    return false;
}

std::string scopeTokenForAttempt(const AuthAttemptScope& scope) {
    if (!scope.has_peer_identity) {
        return "none";
    }
    return "uid=" + std::to_string(scope.peer_uid) +
           ",gid=" + std::to_string(scope.peer_gid) +
           ",pid=" + std::to_string(scope.peer_pid);
}

std::string encodeFailureCodeWithScope(const std::string& failure_code,
                                       const AuthAttemptScope& scope) {
    if (failure_code.empty()) {
        return {};
    }
    return failure_code + "|SB_SCOPE=" + scopeTokenForAttempt(scope) +
           "|SB_BUCKET=" + rateBucketToken(scope.rate_bucket);
}

bool parseFailureScopeMetadata(const std::string& failure_code,
                               std::string& scope_out,
                               AuthRateBucket& bucket_out) {
    scope_out.clear();
    constexpr std::string_view kScopeMarker = "|SB_SCOPE=";
    constexpr std::string_view kBucketMarker = "|SB_BUCKET=";

    const size_t scope_pos = failure_code.rfind(kScopeMarker.data());
    const size_t bucket_pos = failure_code.rfind(kBucketMarker.data());
    if (scope_pos == std::string::npos ||
        bucket_pos == std::string::npos ||
        bucket_pos <= scope_pos) {
        return false;
    }

    const size_t scope_value_start = scope_pos + kScopeMarker.size();
    if (scope_value_start >= bucket_pos) {
        return false;
    }
    scope_out = failure_code.substr(scope_value_start, bucket_pos - scope_value_start);
    if (scope_out.empty()) {
        return false;
    }

    const size_t bucket_value_start = bucket_pos + kBucketMarker.size();
    if (bucket_value_start >= failure_code.size()) {
        return false;
    }
    const std::string bucket_token = failure_code.substr(bucket_value_start);
    return parseRateBucketToken(bucket_token, bucket_out);
}

bool parseLockoutReason(const std::string& reason, ParsedLockoutReason& parsed_out) {
    parsed_out = ParsedLockoutReason{};
    constexpr const char* kPrefix = "LOCKOUT_UNTIL=";
    const size_t pos = reason.find(kPrefix);
    if (pos == std::string::npos) {
        return false;
    }

    const char* value_ptr = reason.c_str() + pos + std::strlen(kPrefix);
    if (*value_ptr == '\0') {
        return false;
    }

    char* end_ptr = nullptr;
    errno = 0;
    const unsigned long long parsed = std::strtoull(value_ptr, &end_ptr, 10);
    if (errno != 0 || end_ptr == value_ptr) {
        return false;
    }
    parsed_out.valid = true;
    parsed_out.deadline = static_cast<uint64_t>(parsed);

    const size_t scope_pos = reason.find(";SCOPE=");
    if (scope_pos != std::string::npos) {
        const size_t scope_start = scope_pos + std::strlen(";SCOPE=");
        size_t scope_end = reason.find(';', scope_start);
        if (scope_end == std::string::npos) {
            scope_end = reason.size();
        }
        if (scope_end > scope_start) {
            parsed_out.scope = reason.substr(scope_start, scope_end - scope_start);
            parsed_out.has_scope = !parsed_out.scope.empty();
        }
    }

    const size_t bucket_pos = reason.find(";BUCKET=");
    if (bucket_pos != std::string::npos) {
        const size_t bucket_start = bucket_pos + std::strlen(";BUCKET=");
        size_t bucket_end = reason.find(';', bucket_start);
        if (bucket_end == std::string::npos) {
            bucket_end = reason.size();
        }
        if (bucket_end > bucket_start) {
            AuthRateBucket parsed_bucket = AuthRateBucket::GENERAL;
            const std::string bucket_token = reason.substr(bucket_start, bucket_end - bucket_start);
            if (parseRateBucketToken(bucket_token, parsed_bucket)) {
                parsed_out.has_bucket = true;
                parsed_out.bucket = parsed_bucket;
            }
        }
    }

    return parsed_out.valid;
}

bool lockoutReasonMatchesScope(const ParsedLockoutReason& reason,
                               const AuthAttemptScope& scope) {
    if (reason.has_scope && reason.scope != scopeTokenForAttempt(scope)) {
        return false;
    }
    if (reason.has_bucket && reason.bucket != scope.rate_bucket) {
        return false;
    }
    return true;
}

bool attemptMatchesScopeAndBucket(const CatalogManager::AuthAttemptLogCatalogInfo& attempt,
                                  const AuthAttemptScope& expected_scope) {
    std::string attempt_scope;
    AuthRateBucket attempt_bucket = AuthRateBucket::GENERAL;
    if (parseFailureScopeMetadata(attempt.failure_code, attempt_scope, attempt_bucket)) {
        return attempt_scope == scopeTokenForAttempt(expected_scope) &&
               attempt_bucket == expected_scope.rate_bucket;
    }

    // Legacy failures without metadata are treated as account-global general bucket.
    return !expected_scope.has_peer_identity &&
           expected_scope.rate_bucket == AuthRateBucket::GENERAL;
}

bool resolveCatalogAuthContext(CatalogManager* catalog,
                               const std::string& username,
                               const std::string* auth_database_context,
                               CatalogAuthContext& out) {
    out = CatalogAuthContext{};
    if (!catalog || username.empty()) {
        return false;
    }

    CatalogManager::PrincipalResolutionRequest request{};
    request.presented_principal_name = username;
    if (auth_database_context != nullptr && !auth_database_context->empty()) {
        request.has_auth_database_context = true;
        request.auth_database_context = *auth_database_context;
    }

    ErrorContext ctx;
    CatalogManager::PrincipalAccountCatalogInfo account;
    Status resolve_status = catalog->resolvePrincipalAccount(request, account, &ctx);
    if (resolve_status != Status::OK) {
        // resolvePrincipalAccount filters out locked identities. For lockout enforcement
        // we still need to read the backing account row so lock state can be checked.
        std::vector<CatalogManager::PrincipalAccountCatalogInfo> accounts;
        if (catalog->listPrincipalAccountCatalogEntries(accounts, &ctx) != Status::OK) {
            return false;
        }

        const bool has_auth_database_context =
            auth_database_context != nullptr && !auth_database_context->empty();
        const CatalogManager::PrincipalAccountCatalogInfo* fallback = nullptr;
        uint8_t fallback_auth_rank = 2;
        for (const auto& row : accounts) {
            if (!equalsIgnoreCaseAscii(row.principal_name, username)) {
                continue;
            }
            uint8_t auth_rank = 0;
            if (row.has_auth_database) {
                if (!has_auth_database_context ||
                    !equalsIgnoreCaseAscii(row.auth_database, *auth_database_context)) {
                    continue;
                }
                auth_rank = 0;
            } else {
                auth_rank = has_auth_database_context ? 1 : 0;
            }

            if (row.source_scope_kind != CatalogManager::SourceScopeKind::ANY) {
                if (fallback == nullptr || auth_rank < fallback_auth_rank) {
                    fallback = &row;
                    fallback_auth_rank = auth_rank;
                }
                continue;
            }
            if (row.has_source_scope_value || !row.source_scope_value.empty()) {
                if (fallback == nullptr || auth_rank < fallback_auth_rank) {
                    fallback = &row;
                    fallback_auth_rank = auth_rank;
                }
                continue;
            }
            fallback = &row;
            fallback_auth_rank = auth_rank;
            if (fallback_auth_rank == 0) {
                break;
            }
        }
        if (fallback == nullptr) {
            return false;
        }
        account = *fallback;
    }

    out.resolved = true;
    out.account = account;

    if (!isZeroUuidLocal(account.auth_policy_id)) {
        CatalogManager::AuthPolicyCatalogInfo policy;
        if (catalog->getAuthPolicyCatalogEntry(account.auth_policy_id, policy, &ctx) == Status::OK) {
            out.policy = policy;
            out.has_policy = true;
        }
    }

    return true;
}

bool persistPrincipalAccount(CatalogManager* catalog,
                             const CatalogManager::PrincipalAccountCatalogInfo& account) {
    if (!catalog) {
        return false;
    }
    ErrorContext ctx;
    return catalog->upsertPrincipalAccountCatalogEntry(account, &ctx) == Status::OK;
}

bool isCatalogAccountLocked(CatalogManager* catalog,
                            CatalogAuthContext& context,
                            const AuthAttemptScope& scope,
                            uint32_t& remaining_minutes_out) {
    remaining_minutes_out = 0;
    if (!catalog || !context.resolved || !context.account.is_locked) {
        return false;
    }

    ParsedLockoutReason parsed_reason;
    const bool parsed_ok = parseLockoutReason(context.account.locked_reason, parsed_reason);
    const uint64_t now_ticks = catalogNowTicks();

    if (parsed_ok) {
        if (now_ticks >= parsed_reason.deadline) {
            context.account.is_locked = false;
            context.account.has_locked_reason = false;
            context.account.locked_reason.clear();
            context.account.last_modified_time = now_ticks;
            (void)persistPrincipalAccount(catalog, context.account);
            return false;
        }

        if (!lockoutReasonMatchesScope(parsed_reason, scope)) {
            return false;
        }

        const auto remaining_duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::duration(parsed_reason.deadline - now_ticks));
        const uint64_t remaining_ms = remaining_duration.count() > 0
            ? static_cast<uint64_t>(remaining_duration.count())
            : 0;
        remaining_minutes_out = static_cast<uint32_t>((remaining_ms + 59999) / 60000);
        return true;
    }

    // Legacy lock reasons are account-global.
    return context.account.is_locked;
}

void appendCatalogAuthAttempt(CatalogManager* catalog,
                              const CatalogAuthContext& context,
                              CatalogManager::AuthAttemptOutcome outcome,
                              const std::string& failure_code,
                              const AuthAttemptScope& scope) {
    if (!catalog || !context.resolved) {
        return;
    }

    CatalogManager::AuthAttemptLogCatalogInfo attempt{};
    attempt.attempt_id = generateUuidV7();
    attempt.connection_id = generateUuidV7();
    attempt.has_account_id = true;
    attempt.account_id = context.account.account_id;
    attempt.has_provider_id = false;
    attempt.provider_id = ID{};
    attempt.outcome = outcome;
    attempt.failure_code = encodeFailureCodeWithScope(failure_code, scope);
    attempt.has_failure_code = !attempt.failure_code.empty();
    attempt.attempt_time_utc = catalogNowTicks();
    attempt.latency_us = 0;
    attempt.is_valid = true;
    attempt.created_time = 0;
    attempt.last_modified_time = 0;

    ErrorContext ctx;
    (void)catalog->upsertAuthAttemptLogCatalogEntry(attempt, &ctx);
}

size_t countRecentCatalogFailures(CatalogManager* catalog,
                                  const CatalogAuthContext& context,
                                  uint64_t now_ticks,
                                  const AuthAttemptScope& scope) {
    if (!catalog || !context.resolved) {
        return 0;
    }

    std::vector<CatalogManager::AuthAttemptLogCatalogInfo> attempts;
    ErrorContext ctx;
    if (catalog->listAuthAttemptLogCatalogEntries(context.account.account_id, attempts, &ctx) != Status::OK) {
        return 0;
    }

    const uint64_t window_ticks = millisecondsToCatalogTicks(context.policy.lockout_window_ms);
    const uint64_t window_start =
        (window_ticks == 0 || now_ticks < window_ticks) ? 0 : (now_ticks - window_ticks);

    size_t failures = 0;
    for (const auto& attempt : attempts) {
        if (attempt.outcome == CatalogManager::AuthAttemptOutcome::SUCCESS) {
            continue;
        }
        if (context.policy.lockout_window_ms > 0 && attempt.attempt_time_utc < window_start) {
            continue;
        }
        if (!attemptMatchesScopeAndBucket(attempt, scope)) {
            continue;
        }
        ++failures;
    }
    return failures;
}

void applyCatalogLockoutOnFailure(CatalogManager* catalog,
                                  CatalogAuthContext& context,
                                  const std::string& failure_code,
                                  const AuthAttemptScope& scope) {
    if (!catalog || !context.resolved) {
        return;
    }

    appendCatalogAuthAttempt(
        catalog,
        context,
        CatalogManager::AuthAttemptOutcome::FAIL,
        failure_code,
        scope);

    if (context.policy.lockout_threshold == 0) {
        return;
    }

    const uint64_t now_ticks = catalogNowTicks();
    const size_t failures = countRecentCatalogFailures(catalog, context, now_ticks, scope);
    if (failures < static_cast<size_t>(context.policy.lockout_threshold)) {
        return;
    }

    const uint64_t lockout_duration_ticks =
        millisecondsToCatalogTicks(context.policy.lockout_duration_ms);
    const uint64_t lockout_until = now_ticks + lockout_duration_ticks;

    context.account.is_locked = true;
    context.account.has_locked_reason = true;
    context.account.locked_reason = "LOCKOUT_UNTIL=" + std::to_string(lockout_until) +
                                    ";SCOPE=" + scopeTokenForAttempt(scope) +
                                    ";BUCKET=" + rateBucketToken(scope.rate_bucket);
    context.account.last_modified_time = now_ticks;
    (void)persistPrincipalAccount(catalog, context.account);
}

void clearCatalogLockoutOnSuccess(CatalogManager* catalog,
                                  CatalogAuthContext& context,
                                  const AuthAttemptScope& scope) {
    if (!catalog || !context.resolved) {
        return;
    }

    appendCatalogAuthAttempt(
        catalog,
        context,
        CatalogManager::AuthAttemptOutcome::SUCCESS,
        "",
        scope);

    if (!context.account.is_locked && !context.account.has_locked_reason) {
        return;
    }

    ParsedLockoutReason parsed_reason;
    if (parseLockoutReason(context.account.locked_reason, parsed_reason) &&
        !lockoutReasonMatchesScope(parsed_reason, scope)) {
        return;
    }

    context.account.is_locked = false;
    context.account.has_locked_reason = false;
    context.account.locked_reason.clear();
    context.account.last_modified_time = catalogNowTicks();
    (void)persistPrincipalAccount(catalog, context.account);
}

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
        if (doc.contains("firebird_legacy_enc") && doc["firebird_legacy_enc"].is_string()) {
            out.firebird_legacy_enc = doc["firebird_legacy_enc"].get<std::string>();
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

const char* scramAlgorithmToken(security::ScramAlgorithm algorithm) {
    return (algorithm == security::ScramAlgorithm::SHA_256) ? "sha256" : "sha512";
}

uint32_t effectiveMinScramIterations(const CatalogAuthContext* context) {
    uint32_t min_iterations = CatalogManager::AUTH_POLICY_MIN_SCRAM_ITERATIONS_DEFAULT;
    if (context && context->has_policy) {
        min_iterations = context->policy.min_scram_iterations;
    }
    if (min_iterations < CatalogManager::AUTH_POLICY_MIN_SCRAM_ITERATIONS_DEFAULT) {
        min_iterations = CatalogManager::AUTH_POLICY_MIN_SCRAM_ITERATIONS_DEFAULT;
    }
    return min_iterations;
}

bool policyMarksWeakScramForUpgrade(const CatalogAuthContext* context) {
    if (!context || !context->has_policy) {
        return false;
    }
    return context->policy.mark_weak_scram_for_upgrade;
}

bool markWeakScramCredentialForUpgrade(CatalogManager* catalog,
                                       const CatalogManager::UserInfo& user,
                                       security::ScramAlgorithm algorithm,
                                       uint32_t observed_iterations,
                                       uint32_t required_iterations) {
    if (!catalog || isZeroUuidLocal(user.user_id)) {
        return false;
    }

    Json metadata = Json::object();
    if (!user.user_metadata.empty()) {
        try {
            metadata = Json::parse(user.user_metadata);
        } catch (const Json::exception&) {
            return false;
        }
        if (!metadata.is_object()) {
            return false;
        }
    }

    Json auth = Json::object();
    if (metadata.contains("auth") && metadata["auth"].is_object()) {
        auth = metadata["auth"];
    }

    const bool already_marked =
        auth.value("scram_upgrade_required", false) &&
        auth.value("scram_observed_iterations", 0u) == observed_iterations &&
        auth.value("scram_min_required_iterations", 0u) == required_iterations &&
        auth.value("scram_algorithm", std::string{}) == scramAlgorithmToken(algorithm);
    if (already_marked) {
        return true;
    }

    auth["scram_upgrade_required"] = true;
    auth["scram_algorithm"] = scramAlgorithmToken(algorithm);
    auth["scram_observed_iterations"] = observed_iterations;
    auth["scram_min_required_iterations"] = required_iterations;
    auth["scram_marked_at"] = catalogNowTicks();
    metadata["auth"] = auth;

    ErrorContext ctx;
    return catalog->updateUserMetadata(user.user_id, metadata.dump(), &ctx) == Status::OK;
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

bool bootstrapForceEnabled() {
    const char* configured = std::getenv("SCRATCHBIRD_BOOTSTRAP_FORCE");
    if (!configured || configured[0] == '\0') {
        return false;
    }

    std::string normalized = toUpperAscii(configured);
    return normalized != "0" &&
           normalized != "FALSE" &&
           normalized != "NO" &&
           normalized != "OFF";
}

std::mutex& bootstrapTokenMutex() {
    static std::mutex mutex;
    return mutex;
}

FilePermissionsControl& bootstrapFilePermissionsControl() {
    static std::unique_ptr<FilePermissionsControl> control = createDefaultFilePermissionsControl();
    return *control;
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
    ErrorContext ctx;
    FileMetadata metadata;
    Status status = bootstrapFilePermissionsControl().readMetadata(token_path, &metadata, &ctx);
    if (status != Status::OK || !metadata.exists) {
        if (!ctx.message.empty()) {
            error_out = "Bootstrap token file stat failed: " + ctx.message;
        } else {
            error_out = "Bootstrap token file stat failed";
        }
        return false;
    }
    if (!metadata.is_regular) {
        error_out = "Bootstrap token path is not a regular file";
        return false;
    }

    // Windows mode bits are ACL-based and not represented through chmod-style values.
    if (metadata.mode_supported && metadata.mode_bits != 0600u) {
        error_out = "Bootstrap token file permissions must be 0600";
        return false;
    }
    return true;
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

AuthAttemptScope makeAuthAttemptScope(
    const LocalAuthProvider::PeerIdentityContext& peer,
    AuthRateBucket bucket) {
    AuthAttemptScope scope{};
    scope.rate_bucket = bucket;
    scope.has_peer_identity = peer.available;
    scope.peer_uid = peer.peer_uid;
    scope.peer_gid = peer.peer_gid;
    scope.peer_pid = peer.peer_pid;
    return scope;
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

void LocalAuthProvider::setPeerIdentityContext(bool available,
                                               uint32_t peer_uid,
                                               uint32_t peer_gid,
                                               uint32_t peer_pid)
{
    peer_identity_context_.available = available;
    peer_identity_context_.peer_uid = available ? peer_uid : 0;
    peer_identity_context_.peer_gid = available ? peer_gid : 0;
    peer_identity_context_.peer_pid = available ? peer_pid : 0;
}

void LocalAuthProvider::clearPeerIdentityContext()
{
    peer_identity_context_ = PeerIdentityContext{};
}

void LocalAuthProvider::setAuthDatabaseContext(const std::string& auth_database_context)
{
    auth_database_context_ = auth_database_context;
}

void LocalAuthProvider::clearAuthDatabaseContext()
{
    auth_database_context_.clear();
}

const std::string* LocalAuthProvider::authDatabaseContextPtr() const
{
    return auth_database_context_.empty() ? nullptr : &auth_database_context_;
}

AuthResult LocalAuthProvider::authenticate(
    const std::string& username,
    const std::string& password,
    AuthUserInfo& user_info_out,
    std::string& error_msg_out)
{
    warnLegacyPasswordDeprecated();
    recordLegacyAuthUsage("password");

    CatalogAuthContext catalog_auth_ctx;
    const bool has_catalog_auth_ctx =
        resolveCatalogAuthContext(catalog_, username, authDatabaseContextPtr(), catalog_auth_ctx);
    const AuthAttemptScope auth_scope =
        makeAuthAttemptScope(peer_identity_context_, AuthRateBucket::GENERAL);

    // Prefer catalog-backed lockout for cross-process consistency.
    if (has_catalog_auth_ctx) {
        uint32_t remaining_minutes = 0;
        if (isCatalogAccountLocked(catalog_, catalog_auth_ctx, auth_scope, remaining_minutes)) {
            LOG_WARNING(GENERAL, "Login attempt for locked account: %s", username.c_str());

            if (audit_logger_) {
                AuditEvent event = AuditLogger::createLoginFailureEvent(username, "account_locked");
                ErrorContext audit_ctx;
                audit_logger_->logEvent(event, &audit_ctx);
            }

            if (remaining_minutes == 0) {
                error_msg_out = "Account locked due to too many failed attempts.";
            } else {
                error_msg_out = "Account locked due to too many failed attempts. Try again in " +
                               std::to_string(remaining_minutes) + " minute(s)";
            }
            return AuthResult::USER_LOCKED;
        }
    } else if (login_tracker_->isAccountLocked(username)) {
        uint64_t remaining_ms = login_tracker_->getLockoutTimeRemaining(username);
        uint32_t remaining_minutes = static_cast<uint32_t>((remaining_ms + 59999) / 60000);

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
    std::string firebird_legacy_enc;
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
        firebird_legacy_enc = parsed.firebird_legacy_enc;
        if (parsed.is_json && !parsed.bcrypt.empty()) {
            actual_hash = parsed.bcrypt;
        }
    } else {
        // Check for bootstrap state (only bootstrap-internal principals).
        std::vector<CatalogManager::UserInfo> all_users;
        Status list_status = catalog_->listUsers(all_users, &ctx);
        bool only_bootstrap_internal_users = false;
        if (list_status == Status::OK) {
            only_bootstrap_internal_users = true;
            for (const auto& user : all_users) {
                const std::string normalized = IdentifierUtils::toUpper(user.username);
                if (normalized != "SYSTEM" && normalized != "SYSARCH") {
                    only_bootstrap_internal_users = false;
                    break;
                }
            }
        }
        bootstrap_allowed = only_bootstrap_internal_users &&
                            bootstrap_state == CatalogManager::BootstrapState::UNINITIALIZED;

        // Use dummy hash for timing resistance (same format as bcrypt)
        // This ensures password verification takes same time whether user exists or not
        actual_hash = "$2a$10$DUMMY.HASH.FOR.TIMING.RESISTANCE.ONLY............................";
    }

    if (!user_exists && bootstrapForceEnabled()) {
        bootstrap_allowed = true;
    }

    if (bootstrap_allowed) {
        logBootstrapAuditEvent(audit_logger_,
                               AuditEventType::BOOTSTRAP_ATTEMPT,
                               username,
                               false,
                               "auth",
                               "bootstrap_candidate");

        const bool force_bootstrap = bootstrapForceEnabled();
        ErrorContext claim_ctx;
        Status claim_status = catalog_->claimBootstrapWindow(&claim_ctx);
        bool claim_acquired = (claim_status == Status::OK);
        if (!claim_acquired &&
            force_bootstrap &&
            claim_status == Status::CONSTRAINT_VIOLATION) {
            // Test/override mode: allow token bootstrap even if state is not UNINITIALIZED.
            claim_acquired = false;
        } else if (!claim_acquired) {
            if (has_catalog_auth_ctx) {
                applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
            } else {
                login_tracker_->recordFailedAttempt(username);
            }
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
            if (!claim_acquired) {
                return true;
            }
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
            if (has_catalog_auth_ctx) {
                applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
            } else {
                login_tracker_->recordFailedAttempt(username);
            }
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

        if (has_catalog_auth_ctx) {
            clearCatalogLockoutOnSuccess(catalog_, catalog_auth_ctx, auth_scope);
        } else {
            login_tracker_->recordSuccessfulLogin(username);
        }

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
        if (!password_valid &&
            user_exists &&
            !firebird_legacy_enc.empty() &&
            password.rfind(kFirebirdLegacySecretPrefix, 0) == 0) {
            const std::string provided =
                password.substr(std::strlen(kFirebirdLegacySecretPrefix));
            password_valid = timingSafeEqual(provided, firebird_legacy_enc);
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
        if (has_catalog_auth_ctx) {
            applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
        } else {
            login_tracker_->recordFailedAttempt(username);
        }

        // Log detailed error internally (for administrators)
        if (!user_exists) {
            LOG_WARNING(GENERAL, "Login attempt for non-existent user: %s", username.c_str());
        } else {
            if (has_catalog_auth_ctx) {
                LOG_WARNING(GENERAL, "Invalid password for user: %s", username.c_str());
            } else {
                LOG_WARNING(GENERAL, "Invalid password for user: %s (failed attempts: %u)",
                           username.c_str(), login_tracker_->getFailedAttemptCount(username));
            }
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
        if (has_catalog_auth_ctx) {
            applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
        } else {
            login_tracker_->recordFailedAttempt(username);
        }

        LOG_WARNING(GENERAL, "Login attempt for disabled user: %s", username.c_str());
        // Return generic error (don't reveal user status)
        error_msg_out = "Invalid username or password";
        return AuthResult::USER_DISABLED;
    }

    // Check if user has password hash set
    if (db_user.password_hash.empty()) {
        // P0-2: Record failed attempt
        if (has_catalog_auth_ctx) {
            applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
        } else {
            login_tracker_->recordFailedAttempt(username);
        }

        LOG_WARNING(GENERAL, "Login attempt for user with no password: %s", username.c_str());
        // Return generic error (don't reveal password status)
        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
    }

    // P0-2: Successful authentication - clear failed attempts
    if (has_catalog_auth_ctx) {
        clearCatalogLockoutOnSuccess(catalog_, catalog_auth_ctx, auth_scope);
    } else {
        login_tracker_->recordSuccessfulLogin(username);
    }

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

    CatalogAuthContext catalog_auth_ctx;
    const bool has_catalog_auth_ctx =
        resolveCatalogAuthContext(catalog_, username, authDatabaseContextPtr(), catalog_auth_ctx);
    const AuthAttemptScope auth_scope =
        makeAuthAttemptScope(peer_identity_context_, AuthRateBucket::GENERAL);

    if (has_catalog_auth_ctx) {
        uint32_t remaining_minutes = 0;
        if (isCatalogAccountLocked(catalog_, catalog_auth_ctx, auth_scope, remaining_minutes)) {
            LOG_WARNING(GENERAL, "Login attempt for locked account: %s", username.c_str());

            if (audit_logger_) {
                AuditEvent event = AuditLogger::createLoginFailureEvent(username, "account_locked");
                ErrorContext audit_ctx;
                audit_logger_->logEvent(event, &audit_ctx);
            }

            if (remaining_minutes == 0) {
                error_msg_out = "Account locked due to too many failed attempts.";
            } else {
                error_msg_out = "Account locked due to too many failed attempts. Try again in " +
                               std::to_string(remaining_minutes) + " minute(s)";
            }
            return AuthResult::USER_LOCKED;
        }
    } else if (login_tracker_->isAccountLocked(username)) {
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
        if (has_catalog_auth_ctx) {
            applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
        } else {
            login_tracker_->recordFailedAttempt(username);
        }

        if (!user_exists) {
            LOG_WARNING(GENERAL, "Login attempt for non-existent user: %s", username.c_str());
        } else {
            if (has_catalog_auth_ctx) {
                LOG_WARNING(GENERAL, "Invalid MD5 response for user: %s", username.c_str());
            } else {
                LOG_WARNING(GENERAL, "Invalid MD5 response for user: %s (failed attempts: %u)",
                           username.c_str(), login_tracker_->getFailedAttemptCount(username));
            }
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
        if (has_catalog_auth_ctx) {
            applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
        } else {
            login_tracker_->recordFailedAttempt(username);
        }
        LOG_WARNING(GENERAL, "Login attempt for disabled user: %s", username.c_str());
        error_msg_out = "Invalid username or password";
        return AuthResult::USER_DISABLED;
    }

    if (!md5_available) {
        if (has_catalog_auth_ctx) {
            applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
        } else {
            login_tracker_->recordFailedAttempt(username);
        }
        LOG_WARNING(GENERAL, "Login attempt for user without MD5 credential: %s", username.c_str());
        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
    }

    if (has_catalog_auth_ctx) {
        clearCatalogLockoutOnSuccess(catalog_, catalog_auth_ctx, auth_scope);
    } else {
        login_tracker_->recordSuccessfulLogin(username);
    }

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
    CatalogAuthContext catalog_auth_ctx;
    const bool has_catalog_auth_ctx =
        resolveCatalogAuthContext(catalog_, username, authDatabaseContextPtr(), catalog_auth_ctx);
    const AuthAttemptScope auth_scope =
        makeAuthAttemptScope(peer_identity_context_, AuthRateBucket::SCRAM_BEGIN);
    if (has_catalog_auth_ctx) {
        uint32_t remaining_minutes = 0;
        if (isCatalogAccountLocked(catalog_, catalog_auth_ctx, auth_scope, remaining_minutes)) {
            LOG_WARNING(GENERAL, "Login attempt for locked account: %s", username.c_str());

            if (audit_logger_) {
                AuditEvent event = AuditLogger::createLoginFailureEvent(username, "account_locked");
                ErrorContext audit_ctx;
                audit_logger_->logEvent(event, &audit_ctx);
            }

            if (remaining_minutes == 0) {
                error_msg_out = "Account locked due to too many failed attempts.";
            } else {
                error_msg_out = "Account locked due to too many failed attempts. Try again in " +
                               std::to_string(remaining_minutes) + " minute(s)";
            }
            return AuthResult::USER_LOCKED;
        }
    } else if (login_tracker_->isAccountLocked(username)) {
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
        if (has_catalog_auth_ctx) {
            applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
        } else {
            login_tracker_->recordFailedAttempt(username);
        }
        error_msg_out = "Invalid SCRAM message";
        return AuthResult::INVALID_CREDENTIALS;
    }

    if (!username.empty() && parsed.username != username) {
        if (has_catalog_auth_ctx) {
            applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
        } else {
            login_tracker_->recordFailedAttempt(username);
        }
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

    const uint32_t min_scram_iterations =
        effectiveMinScramIterations(has_catalog_auth_ctx ? &catalog_auth_ctx : nullptr);
    if (user_exists && record.valid && record.iterations < min_scram_iterations) {
        if (policyMarksWeakScramForUpgrade(has_catalog_auth_ctx ? &catalog_auth_ctx : nullptr)) {
            if (!markWeakScramCredentialForUpgrade(catalog_,
                                                   db_user,
                                                   algorithm,
                                                   record.iterations,
                                                   min_scram_iterations)) {
                LOG_WARNING(GENERAL,
                            "Unable to persist weak SCRAM upgrade marker for user: %s",
                            parsed.username.c_str());
            }
        }

        if (audit_logger_) {
            AuditEvent event = AuditLogger::createLoginFailureEvent(parsed.username,
                                                                    "weak_scram_iterations");
            ErrorContext audit_ctx;
            audit_logger_->logEvent(event, &audit_ctx);
        }

        LOG_WARNING(GENERAL,
                    "SCRAM credential below policy minimum for user %s: stored=%u required=%u",
                    parsed.username.c_str(),
                    record.iterations,
                    min_scram_iterations);
        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
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
    CatalogAuthContext catalog_auth_ctx;
    const bool has_catalog_auth_ctx =
        resolveCatalogAuthContext(catalog_, state.username, authDatabaseContextPtr(), catalog_auth_ctx);
    const AuthAttemptScope auth_scope =
        makeAuthAttemptScope(peer_identity_context_, AuthRateBucket::SCRAM_FINISH);

    if (has_catalog_auth_ctx) {
        uint32_t remaining_minutes = 0;
        if (isCatalogAccountLocked(catalog_, catalog_auth_ctx, auth_scope, remaining_minutes)) {
            LOG_WARNING(GENERAL, "Login attempt for locked account: %s", state.username.c_str());

            if (audit_logger_) {
                AuditEvent event = AuditLogger::createLoginFailureEvent(state.username, "account_locked");
                ErrorContext audit_ctx;
                audit_logger_->logEvent(event, &audit_ctx);
            }

            if (remaining_minutes == 0) {
                error_msg_out = "Account locked due to too many failed attempts.";
            } else {
                error_msg_out = "Account locked due to too many failed attempts. Try again in " +
                                std::to_string(remaining_minutes) + " minute(s)";
            }
            return AuthResult::USER_LOCKED;
        }
    } else if (login_tracker_->isAccountLocked(state.username)) {
        uint64_t remaining_ms = login_tracker_->getLockoutTimeRemaining(state.username);
        uint32_t remaining_minutes = static_cast<uint32_t>((remaining_ms + 59999) / 60000);

        LOG_WARNING(GENERAL, "Login attempt for locked account: %s (locked for %u more minutes)",
                    state.username.c_str(), remaining_minutes);
        error_msg_out = "Account locked due to too many failed attempts. Try again in " +
                        std::to_string(remaining_minutes) + " minute(s)";
        return AuthResult::USER_LOCKED;
    }

    security::ScramClientFinal parsed;
    if (!security::parseClientFinal(client_final, parsed)) {
        if (has_catalog_auth_ctx) {
            applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
        } else {
            login_tracker_->recordFailedAttempt(state.username);
        }
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
        if (has_catalog_auth_ctx) {
            applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
        } else {
            login_tracker_->recordFailedAttempt(state.username);
        }
        if (!state.user_exists) {
            LOG_WARNING(GENERAL, "SCRAM login attempt for non-existent user: %s",
                       state.username.c_str());
        } else {
            if (has_catalog_auth_ctx) {
                LOG_WARNING(GENERAL, "Invalid SCRAM proof for user: %s",
                           state.username.c_str());
            } else {
                LOG_WARNING(GENERAL, "Invalid SCRAM proof for user: %s (failed attempts: %u)",
                           state.username.c_str(),
                           login_tracker_->getFailedAttemptCount(state.username));
            }
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
        if (has_catalog_auth_ctx) {
            applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
        } else {
            login_tracker_->recordFailedAttempt(state.username);
        }
        LOG_WARNING(GENERAL, "SCRAM login attempt for disabled user: %s", state.username.c_str());
        error_msg_out = "Invalid username or password";
        return AuthResult::USER_DISABLED;
    }

    if (has_catalog_auth_ctx) {
        clearCatalogLockoutOnSuccess(catalog_, catalog_auth_ctx, auth_scope);
    } else {
        login_tracker_->recordSuccessfulLogin(state.username);
    }

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
    CatalogAuthContext catalog_auth_ctx;
    const bool has_catalog_auth_ctx =
        resolveCatalogAuthContext(catalog_, username, authDatabaseContextPtr(), catalog_auth_ctx);
    const AuthAttemptScope auth_scope =
        makeAuthAttemptScope(peer_identity_context_, AuthRateBucket::GENERAL);
    if (has_catalog_auth_ctx) {
        uint32_t remaining_minutes = 0;
        if (isCatalogAccountLocked(catalog_, catalog_auth_ctx, auth_scope, remaining_minutes)) {
            LOG_WARNING(GENERAL, "Token login attempt for locked account: %s", username.c_str());
            if (remaining_minutes == 0) {
                error_msg_out = "Account locked due to too many failed attempts.";
            } else {
                error_msg_out = "Account locked due to too many failed attempts. Try again in " +
                                std::to_string(remaining_minutes) + " minute(s)";
            }
            return AuthResult::USER_LOCKED;
        }
    } else if (login_tracker_->isAccountLocked(username)) {
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
        if (audit_logger_ && authkey_status == Status::OK &&
            authkey.status == CatalogManager::AuthKeyStatus::REVOKED) {
            AuditEvent event;
            event.event_type = AuditEventType::TOKEN_AUTH_REVOKED;
            event.username = username;
            event.authkey_id = authkey_id;
            event.success = false;
            event.details = "{\"reason\":\"token_revoked\"}";
            ErrorContext audit_ctx;
            audit_logger_->logEvent(event, &audit_ctx);
        }
        if (has_catalog_auth_ctx) {
            applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
        } else {
            login_tracker_->recordFailedAttempt(username);
        }
        LOG_WARNING(GENERAL, "Invalid token auth attempt for user: %s", username.c_str());
        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
    }

    if (authkey.binding_kind != CatalogManager::AuthKeyBindingKind::CLIENT_NONCE ||
        authkey.binding_value.empty()) {
        if (has_catalog_auth_ctx) {
            applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
        } else {
            login_tracker_->recordFailedAttempt(username);
        }
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
        if (has_catalog_auth_ctx) {
            applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
        } else {
            login_tracker_->recordFailedAttempt(username);
        }
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
        if (has_catalog_auth_ctx) {
            applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
        } else {
            login_tracker_->recordFailedAttempt(username);
        }
        LOG_WARNING(GENERAL, "Token proof mismatch for user: %s", username.c_str());
        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
    }

    Status consume_status = catalog_->consumeAuthKey(authkey_id, 1, &ctx);
    if (consume_status != Status::OK) {
        if (has_catalog_auth_ctx) {
            applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
        } else {
            login_tracker_->recordFailedAttempt(username);
        }
        LOG_WARNING(GENERAL, "Token consume denied for user %s: %s",
                    username.c_str(), ctx.message.c_str());
        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
    }

    if (has_catalog_auth_ctx) {
        clearCatalogLockoutOnSuccess(catalog_, catalog_auth_ctx, auth_scope);
    } else {
        login_tracker_->recordSuccessfulLogin(username);
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

    if (audit_logger_) {
        AuditEvent event;
        event.event_type = AuditEventType::TOKEN_AUTH_USED;
        event.username = db_user.username;
        event.user_id = db_user.user_id;
        event.authkey_id = authkey_id;
        event.success = true;
        std::ostringstream details;
        details << "{\"scope\":\"" << static_cast<int>(authkey.scope)
                << "\",\"usage_count\":" << authkey.usage_count + 1 << "}";
        event.details = details.str();
        ErrorContext audit_ctx;
        audit_logger_->logEvent(event, &audit_ctx);
    }

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
    AuthAttemptScope auth_scope{};
    auth_scope.has_peer_identity = true;
    auth_scope.peer_uid = peer_uid;
    auth_scope.peer_gid = peer_gid;
    auth_scope.peer_pid = peer_pid;
    auth_scope.rate_bucket = AuthRateBucket::GENERAL;

    CatalogAuthContext catalog_auth_ctx;
    const bool has_catalog_auth_ctx =
        resolveCatalogAuthContext(catalog_, username, authDatabaseContextPtr(), catalog_auth_ctx);
    if (has_catalog_auth_ctx) {
        uint32_t remaining_minutes = 0;
        if (isCatalogAccountLocked(catalog_, catalog_auth_ctx, auth_scope, remaining_minutes)) {
            LOG_WARNING(GENERAL,
                        "Peer login attempt for locked account: %s",
                        username.c_str());
            if (remaining_minutes == 0) {
                error_msg_out = "Account locked due to too many failed attempts.";
            } else {
                error_msg_out = "Account locked due to too many failed attempts. Try again in " +
                                std::to_string(remaining_minutes) + " minute(s)";
            }
            return AuthResult::USER_LOCKED;
        }
    } else if (login_tracker_->isAccountLocked(username)) {
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
        if (has_catalog_auth_ctx) {
            applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
        } else {
            login_tracker_->recordFailedAttempt(username);
        }
        LOG_WARNING(GENERAL, "Peer auth denied for unknown/disabled user: %s", username.c_str());
        error_msg_out = "Invalid username or password";
        return AuthResult::INVALID_CREDENTIALS;
    }

    CatalogManager::PrincipalResolutionRequest req{};
    req.presented_principal_name = db_user.username;
    if (const std::string* auth_db_context = authDatabaseContextPtr();
        auth_db_context != nullptr) {
        req.has_auth_database_context = true;
        req.auth_database_context = *auth_db_context;
    }
    req.has_peer_uid = true;
    req.peer_uid = peer_uid;
    req.has_peer_gid = true;
    req.peer_gid = peer_gid;

    CatalogManager::PrincipalAccountCatalogInfo account{};
    Status resolve_status = catalog_->resolvePrincipalAccount(req, account, &ctx);
    if (resolve_status != Status::OK ||
        (account.source_scope_kind != CatalogManager::SourceScopeKind::PEER_UID &&
         account.source_scope_kind != CatalogManager::SourceScopeKind::PEER_GID)) {
        if (has_catalog_auth_ctx) {
            applyCatalogLockoutOnFailure(catalog_, catalog_auth_ctx, "SEC_1213", auth_scope);
        } else {
            login_tracker_->recordFailedAttempt(username);
        }
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

    if (has_catalog_auth_ctx) {
        clearCatalogLockoutOnSuccess(catalog_, catalog_auth_ctx, auth_scope);
    } else {
        login_tracker_->recordSuccessfulLogin(username);
    }

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
    CatalogAuthContext catalog_auth_ctx;
    if (resolveCatalogAuthContext(catalog_, username, authDatabaseContextPtr(), catalog_auth_ctx)) {
        std::vector<CatalogManager::AuthAttemptLogCatalogInfo> attempts;
        ErrorContext ctx;
        if (catalog_->listAuthAttemptLogCatalogEntries(
                catalog_auth_ctx.account.account_id, attempts, &ctx) == Status::OK) {
            for (const auto& attempt : attempts) {
                ErrorContext delete_ctx;
                (void)catalog_->deleteAuthAttemptLogCatalogEntry(attempt.attempt_id, &delete_ctx);
            }
        }

        if (catalog_auth_ctx.account.is_locked || catalog_auth_ctx.account.has_locked_reason) {
            catalog_auth_ctx.account.is_locked = false;
            catalog_auth_ctx.account.has_locked_reason = false;
            catalog_auth_ctx.account.locked_reason.clear();
            catalog_auth_ctx.account.last_modified_time = catalogNowTicks();
            (void)persistPrincipalAccount(catalog_, catalog_auth_ctx.account);
        }
    }

    if (login_tracker_) {
        login_tracker_->clearAttempts(username);
        LOG_INFO(GENERAL, "Cleared login attempts for user: %s", username.c_str());
    }
}

uint32_t LocalAuthProvider::getFailedAttemptCount(const std::string& username)
{
    CatalogAuthContext catalog_auth_ctx;
    if (resolveCatalogAuthContext(catalog_, username, authDatabaseContextPtr(), catalog_auth_ctx)) {
        std::vector<CatalogManager::AuthAttemptLogCatalogInfo> attempts;
        ErrorContext ctx;
        if (catalog_->listAuthAttemptLogCatalogEntries(
                catalog_auth_ctx.account.account_id, attempts, &ctx) == Status::OK) {
            uint32_t failures = 0;
            for (const auto& attempt : attempts) {
                if (attempt.outcome != CatalogManager::AuthAttemptOutcome::SUCCESS) {
                    ++failures;
                }
            }
            return failures;
        }
    }

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
