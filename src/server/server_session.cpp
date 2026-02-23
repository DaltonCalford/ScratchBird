/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * ScratchBird Server Session Implementation
 *
 * Local Server Architecture - Phase 3
 */

#include "scratchbird/server/server_session.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/bytecode_validator.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/auth_provider.h"
#include "scratchbird/core/audit_logger.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/security/scram_auth.h"
#include "scratchbird/security/mfa_auth.h"

#include <cstring>
#include <cctype>
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <array>
#include <map>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstdio>
#include <functional>
#include <streambuf>
#include <stdexcept>
#include <random>
#include <openssl/sha.h>
#ifndef _WIN32
#include "scratchbird/core/posix_compat.h"
#endif

namespace scratchbird {
namespace server {

namespace {

struct CopyStreamState {
    uint64_t stream_id{0};
    uint64_t total_bytes{0};
    uint32_t in_window_bytes{0};
    uint32_t in_window_grant{65536};
    uint32_t in_low_watermark{32768};
    uint32_t out_window_bytes{0};
    bool out_paused{false};
};

class CopyOutStreambuf : public std::streambuf {
public:
    using WriteFn = std::function<bool(const uint8_t*, size_t, std::string&)>;

    CopyOutStreambuf(size_t buffer_size, WriteFn write_fn)
        : write_fn_(std::move(write_fn)) {
        if (buffer_size == 0) {
            buffer_size = 65536;
        }
        buffer_.resize(buffer_size);
        setp(buffer_.data(), buffer_.data() + buffer_.size());
    }

    ~CopyOutStreambuf() override {
        sync();
    }

protected:
    int_type overflow(int_type ch) override {
        if (ch != traits_type::eof()) {
            *pptr() = static_cast<char>(ch);
            pbump(1);
        }
        if (!flushBuffer()) {
            return traits_type::eof();
        }
        return ch;
    }

    int sync() override {
        return flushBuffer() ? 0 : -1;
    }

private:
    bool flushBuffer() {
        size_t len = static_cast<size_t>(pptr() - pbase());
        if (len == 0) {
            return true;
        }
        std::string error;
        bool ok = write_fn_(reinterpret_cast<const uint8_t*>(buffer_.data()), len, error);
        if (!ok) {
            throw std::runtime_error(error.empty() ? "COPY OUT stream error" : error);
        }
        setp(buffer_.data(), buffer_.data() + buffer_.size());
        return true;
    }

    std::vector<char> buffer_;
    WriteFn write_fn_;
};

class CopyInStreambuf : public std::streambuf {
public:
    using ReadFn = std::function<bool(std::string&, bool&, std::string&)>;

    explicit CopyInStreambuf(ReadFn read_fn)
        : read_fn_(std::move(read_fn)) {}

protected:
    int_type underflow() override {
        if (done_) {
            return traits_type::eof();
        }

        std::string chunk;
        std::string error;
        bool ok = read_fn_(chunk, done_, error);
        if (!ok) {
            throw std::runtime_error(error.empty() ? "COPY IN stream error" : error);
        }
        if (chunk.empty()) {
            return traits_type::eof();
        }

        buffer_ = std::move(chunk);
        setg(buffer_.data(), buffer_.data(), buffer_.data() + buffer_.size());
        return traits_type::to_int_type(*gptr());
    }

private:
    ReadFn read_fn_;
    std::string buffer_;
    bool done_ = false;
};

bool parseCopyQuery(const std::string& sql, bool& from_stdin, bool& to_stdout) {
    from_stdin = false;
    to_stdout = false;

    auto trim_left = [](const std::string& input) {
        size_t pos = 0;
        while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
            ++pos;
        }
        return input.substr(pos);
    };

    std::string trimmed = trim_left(sql);
    if (trimmed.size() < 4) {
        return false;
    }

    std::string upper = trimmed;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    if (upper.rfind("COPY", 0) != 0) {
        return false;
    }
    if (upper.size() > 4) {
        char next = upper[4];
        if (!std::isspace(static_cast<unsigned char>(next)) && next != '(') {
            return false;
        }
    }

    if (upper.find("FROM STDIN") != std::string::npos) {
        from_stdin = true;
    }
    if (upper.find("TO STDOUT") != std::string::npos) {
        to_stdout = true;
    }
    return from_stdin || to_stdout;
}

std::string toUpperAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

uint64_t fnv1a64(const std::string& value, uint64_t seed) {
    constexpr uint64_t kPrime = 1099511628211ull;
    uint64_t hash = seed;
    for (unsigned char c : value) {
        hash ^= static_cast<uint64_t>(c);
        hash *= kPrime;
    }
    return hash;
}

std::array<uint8_t, 16> deriveBindingDatabaseUuid(const std::string& database_name) {
    std::array<uint8_t, 16> out{};
    const uint64_t hi = fnv1a64(database_name, 1469598103934665603ull);
    const uint64_t lo = fnv1a64(database_name, 1099511628211ull);
    for (int i = 0; i < 8; ++i) {
        out[static_cast<size_t>(i)] = static_cast<uint8_t>((hi >> (i * 8)) & 0xFF);
        out[static_cast<size_t>(8 + i)] = static_cast<uint8_t>((lo >> (i * 8)) & 0xFF);
    }
    out[6] = static_cast<uint8_t>((out[6] & 0x0F) | 0x40);
    out[8] = static_cast<uint8_t>((out[8] & 0x3F) | 0x80);
    return out;
}

bool parseUint32Strict(const char* text, uint32_t& value_out) {
    if (!text || text[0] == '\0') {
        return false;
    }

    char* end_ptr = nullptr;
    errno = 0;
    unsigned long parsed = std::strtoul(text, &end_ptr, 10);
    if (errno != 0 || end_ptr == text || *end_ptr != '\0') {
        return false;
    }
    if (parsed > std::numeric_limits<uint32_t>::max()) {
        return false;
    }

    value_out = static_cast<uint32_t>(parsed);
    return true;
}

bool bootstrapOwnerUidGateEnabled() {
    const char* configured = std::getenv("SCRATCHBIRD_BOOTSTRAP_REQUIRE_OWNER_UID");
    if (!configured || configured[0] == '\0') {
        return true;
    }

    std::string normalized = toUpperAscii(configured);
    return normalized != "0" &&
           normalized != "FALSE" &&
           normalized != "NO" &&
           normalized != "OFF";
}

uint32_t resolveBootstrapOwnerUid() {
    uint32_t configured_uid = 0;
    const char* configured = std::getenv("SCRATCHBIRD_BOOTSTRAP_OWNER_UID");
    if (parseUint32Strict(configured, configured_uid)) {
        return configured_uid;
    }

#ifdef _WIN32
    return 0;
#else
    return static_cast<uint32_t>(::geteuid());
#endif
}

bool detectBootstrapPhaseA(core::CatalogManager* catalog,
                           bool& phase_a_out,
                           std::string& error_out) {
    phase_a_out = false;
    error_out.clear();

    if (!catalog) {
        return true;
    }

    core::CatalogManager::BootstrapState bootstrap_state =
        core::CatalogManager::BootstrapState::UNINITIALIZED;
    core::ErrorContext bootstrap_ctx;
    core::Status bootstrap_status = catalog->getBootstrapState(bootstrap_state, &bootstrap_ctx);
    if (bootstrap_status != core::Status::OK) {
        error_out = bootstrap_ctx.message.empty() ? "bootstrap_state_unavailable"
                                                  : bootstrap_ctx.message;
        return false;
    }

    if (bootstrap_state != core::CatalogManager::BootstrapState::UNINITIALIZED) {
        return true;
    }

    std::vector<core::CatalogManager::UserInfo> all_users;
    core::Status list_status = catalog->listUsers(all_users, &bootstrap_ctx);
    if (list_status != core::Status::OK) {
        error_out = bootstrap_ctx.message.empty() ? "bootstrap_user_scan_failed"
                                                  : bootstrap_ctx.message;
        return false;
    }

    if (all_users.empty()) {
        phase_a_out = true;
        return true;
    }

    phase_a_out = all_users.size() == 1 &&
                  toUpperAscii(all_users.front().username) == "SYSTEM";
    return true;
}

bool isBootstrapPeerEligible(const IPCConnection* connection, std::string& denial_reason_out) {
    denial_reason_out.clear();

    if (!bootstrapOwnerUidGateEnabled()) {
        return true;
    }

    if (!connection) {
        denial_reason_out = "connection_unavailable";
        return false;
    }

    if (connection->getMethod() != IPCMethod::UNIX_SOCKET) {
        denial_reason_out = "bootstrap_requires_local_ipc";
        return false;
    }

    PeerCredentials peer = connection->getPeerCredentials();
    if (!peer.available) {
        denial_reason_out = "peer_credentials_unavailable";
        return false;
    }

    const uint32_t owner_uid = resolveBootstrapOwnerUid();
    if (peer.uid == owner_uid || peer.uid == 0) {
        return true;
    }

    denial_reason_out = "peer_uid_not_eligible";
    return false;
}

constexpr const char* kAuthPolicyNegotiationRequiredCode = "AUTH_POLICY_NEGOTIATION_REQUIRED";
constexpr const char* kAuthPolicyMethodDeniedCode = "AUTH_POLICY_METHOD_DENIED";
constexpr const char* kAuthPolicyRequiredMethodCode = "AUTH_POLICY_REQUIRED_METHOD";
constexpr const char* kAuthPolicyTransportDeniedCode = "AUTH_POLICY_TRANSPORT_DENIED";
constexpr const char* kAuthPolicyUserMismatchCode = "AUTH_POLICY_USER_MISMATCH";
constexpr const char* kAuthPolicyPeerRequiredCode = "AUTH_POLICY_PEER_REQUIRED";
constexpr const char* kAuthPolicyPeerTransportCode = "AUTH_POLICY_PEER_TRANSPORT_UNSUPPORTED";
constexpr const char* kAuthMfaRequiredCode = "AUTH_MFA_REQUIRED";
constexpr const char* kAuthMfaInvalidCode = "AUTH_MFA_INVALID";
constexpr const char* kMfaPayloadPrefix = "SBMFA1";
constexpr const char* kAuthPolicyNegotiatedCode = "AUTH_POLICY_NEGOTIATED";

const char* authMethodToString(protocol::AuthMethod method) {
    switch (method) {
        case protocol::AuthMethod::PASSWORD:
            return "PASSWORD";
        case protocol::AuthMethod::MD5:
            return "MD5";
        case protocol::AuthMethod::SCRAM_SHA_256:
            return "SCRAM_SHA_256";
        case protocol::AuthMethod::SCRAM_SHA_512:
            return "SCRAM_SHA_512";
        case protocol::AuthMethod::TOKEN:
            return "TOKEN";
        case protocol::AuthMethod::PEER:
            return "PEER";
    }
    return "UNKNOWN";
}

const char* authPeerModeToString(core::CatalogManager::AuthPeerMode mode) {
    using PM = core::CatalogManager::AuthPeerMode;
    switch (mode) {
        case PM::DISABLED:
            return "DISABLED";
        case PM::REQUIRED:
            return "REQUIRED";
        case PM::REQUIRED_PLUS_SCRAM:
            return "REQUIRED_PLUS_SCRAM";
    }
    return "UNKNOWN";
}

std::string authMethodsToJsonArray(const std::vector<protocol::AuthMethod>& methods) {
    std::ostringstream out;
    out << "[";
    for (size_t i = 0; i < methods.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << "\"" << authMethodToString(methods[i]) << "\"";
    }
    out << "]";
    return out.str();
}

void logAuthPolicyDecision(core::AuditLogger* audit_logger,
                           const std::string& username,
                           bool success,
                           const std::string& decision_code,
                           protocol::AuthMethod requested_method,
                           const std::vector<protocol::AuthMethod>& allowed_methods,
                           bool has_required_method,
                           protocol::AuthMethod required_method,
                           uint8_t transport_mask,
                           core::CatalogManager::AuthPeerMode peer_mode) {
    if (!audit_logger) {
        return;
    }

    core::AuditEvent event;
    event.event_type = core::AuditEventType::AUTH_POLICY_DECISION;
    event.username = username;
    event.success = success;

    std::ostringstream details;
    details << "{"
            << "\"decision\":\"" << (success ? "allow" : "deny") << "\""
            << ",\"code\":\"" << decision_code << "\""
            << ",\"requested_method\":\"" << authMethodToString(requested_method) << "\""
            << ",\"allowed_methods\":" << authMethodsToJsonArray(allowed_methods)
            << ",\"has_required_method\":" << (has_required_method ? "true" : "false")
            << ",\"required_method\":\""
            << (has_required_method ? authMethodToString(required_method) : "NONE") << "\""
            << ",\"transport_mask\":" << static_cast<unsigned>(transport_mask)
            << ",\"peer_mode\":\"" << authPeerModeToString(peer_mode) << "\""
            << "}";
    event.details = details.str();

    core::ErrorContext audit_ctx;
    (void)audit_logger->logEvent(event, &audit_ctx);
}

std::vector<std::string> splitPipeFields(const std::string& input) {
    std::vector<std::string> fields;
    size_t start = 0;
    while (start <= input.size()) {
        const size_t delim = input.find('|', start);
        if (delim == std::string::npos) {
            fields.push_back(input.substr(start));
            break;
        }
        fields.push_back(input.substr(start, delim - start));
        start = delim + 1;
    }
    return fields;
}

std::vector<uint8_t> buildMfaChallengePayload(const std::string& challenge_id,
                                              const std::string& method_name,
                                              const std::vector<uint8_t>& server_final_payload) {
    std::string payload = std::string(kMfaPayloadPrefix) + "|" + challenge_id + "|" + method_name;
    if (!server_final_payload.empty()) {
        payload += "|" + security::base64Encode(server_final_payload);
    }
    return std::vector<uint8_t>(payload.begin(), payload.end());
}

bool parseMfaResponsePayload(const std::vector<uint8_t>& payload,
                             std::string& challenge_id_out,
                             std::string& code_out) {
    challenge_id_out.clear();
    code_out.clear();

    if (payload.empty()) {
        return false;
    }

    const std::string raw(payload.begin(), payload.end());
    const std::vector<std::string> parts = splitPipeFields(raw);
    if (parts.size() != 3 || parts[0] != kMfaPayloadPrefix) {
        return false;
    }
    if (parts[1].empty() || parts[2].empty()) {
        return false;
    }

    challenge_id_out = parts[1];
    code_out = parts[2];
    return true;
}

std::string sanitizeForEnvKey(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (const unsigned char ch : value) {
        if (std::isalnum(ch)) {
            out.push_back(static_cast<char>(std::toupper(ch)));
        } else {
            out.push_back('_');
        }
    }
    return out;
}

std::string resolveMfaTotpSecretFromEnv(const std::string& username) {
    const std::string user_key =
        std::string("SCRATCHBIRD_MFA_TOTP_SECRET_") + sanitizeForEnvKey(username);
    const char* per_user = std::getenv(user_key.c_str());
    if (per_user && per_user[0] != '\0') {
        return std::string(per_user);
    }

    const char* generic = std::getenv("SCRATCHBIRD_MFA_TOTP_SECRET_BASE32");
    if (generic && generic[0] != '\0') {
        return std::string(generic);
    }
    return std::string();
}

struct ResolvedMfaAuthConfig {
    bool mfa_required = false;
    core::ID account_id;
    core::ID mfa_policy_id;
    bool allow_recovery_codes = true;
    bool allow_break_glass = false;
    uint8_t max_challenge_attempts = 3;
    uint32_t step_up_ttl_ms = 0;
};

struct MfaVerificationResult {
    bool success = false;
    bool recovery_used = false;
    bool break_glass_used = false;
};

uint64_t currentTimeMs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
}

std::array<uint8_t, 32> hashMfaCodeSha256(const std::string& code) {
    std::array<uint8_t, 32> out{};
    SHA256(reinterpret_cast<const unsigned char*>(code.data()),
           static_cast<unsigned long>(code.size()),
           out.data());
    return out;
}

bool loadPrimaryTotpEnrollment(core::CatalogManager* catalog,
                               const core::ID& account_id,
                               const core::ID& mfa_policy_id,
                               core::CatalogManager::MfaEnrollmentCatalogInfo& enrollment_out,
                               std::string& error_out) {
    enrollment_out = core::CatalogManager::MfaEnrollmentCatalogInfo{};
    error_out.clear();

    std::vector<core::CatalogManager::MfaEnrollmentCatalogInfo> enrollments;
    core::ErrorContext ctx;
    const core::Status status =
        catalog->listMfaEnrollmentCatalogEntries(account_id, enrollments, &ctx);
    if (status != core::Status::OK) {
        error_out = ctx.message.empty() ? "mfa_enrollment_lookup_failed" : ctx.message;
        return false;
    }

    bool found_any = false;
    for (const auto& enrollment : enrollments) {
        if (!enrollment.is_enrolled ||
            enrollment.factor_type != core::CatalogManager::MfaFactorType::TOTP ||
            !enrollment.has_secret) {
            continue;
        }
        if (mfa_policy_id != core::ID{} &&
            enrollment.mfa_policy_id != mfa_policy_id &&
            enrollment.mfa_policy_id != core::ID{}) {
            continue;
        }
        if (enrollment.is_primary) {
            enrollment_out = enrollment;
            return true;
        }
        if (!found_any) {
            enrollment_out = enrollment;
            found_any = true;
        }
    }
    return found_any;
}

bool ensureCatalogMfaEnrollmentForUser(core::CatalogManager* catalog,
                                       const core::ID& account_id,
                                       const core::ID& mfa_policy_id,
                                       const std::string& username,
                                       core::CatalogManager::MfaEnrollmentCatalogInfo& enrollment_out,
                                       std::string& error_out) {
    error_out.clear();
    if (loadPrimaryTotpEnrollment(catalog, account_id, mfa_policy_id, enrollment_out, error_out)) {
        return true;
    }

    const std::string secret_base32 = resolveMfaTotpSecretFromEnv(username);
    if (secret_base32.empty()) {
        error_out = "mfa_not_configured";
        return false;
    }
    if (security::decodeBase32(secret_base32).empty()) {
        error_out = "mfa_secret_invalid";
        return false;
    }

    core::CatalogManager::MfaEnrollmentCatalogInfo enrollment{};
    enrollment.enrollment_id = core::generateUuidV7();
    enrollment.account_id = account_id;
    enrollment.mfa_policy_id = mfa_policy_id;
    enrollment.factor_type = core::CatalogManager::MfaFactorType::TOTP;
    enrollment.is_primary = true;
    enrollment.is_enrolled = true;
    enrollment.has_secret = true;
    enrollment.secret_base32 = secret_base32;
    enrollment.enrolled_time_utc = static_cast<uint64_t>(
        std::chrono::system_clock::now().time_since_epoch().count());

    uint32_t parsed = 0;
    if (parseUint32Strict(std::getenv("SCRATCHBIRD_MFA_TOTP_DIGITS"), parsed) &&
        parsed > 0 && parsed <= 10) {
        enrollment.totp_digits = static_cast<uint8_t>(parsed);
    }
    if (parseUint32Strict(std::getenv("SCRATCHBIRD_MFA_TOTP_PERIOD"), parsed) &&
        parsed > 0) {
        enrollment.totp_period = parsed;
    }

    core::ErrorContext upsert_ctx;
    const core::Status upsert_status = catalog->upsertMfaEnrollmentCatalogEntry(enrollment, &upsert_ctx);
    if (upsert_status != core::Status::OK) {
        error_out = upsert_ctx.message.empty() ? "mfa_enrollment_upsert_failed" : upsert_ctx.message;
        return false;
    }
    return loadPrimaryTotpEnrollment(catalog, account_id, mfa_policy_id, enrollment_out, error_out);
}

bool resolveMfaConfig(core::CatalogManager* catalog,
                      const IPCConnection* connection,
                      const std::string& username,
                      ResolvedMfaAuthConfig& config_out,
                      std::string& error_out) {
    config_out = ResolvedMfaAuthConfig{};
    error_out.clear();
    if (!catalog || username.empty()) {
        return true;
    }

    core::CatalogManager::PrincipalResolutionRequest request{};
    request.presented_principal_name = username;

    if (connection) {
        const PeerCredentials peer = connection->getPeerCredentials();
        if (peer.available) {
            request.has_peer_uid = true;
            request.peer_uid = peer.uid;
            request.has_peer_gid = true;
            request.peer_gid = peer.gid;
            request.has_peer_pid = true;
            request.peer_pid = peer.pid;
        }
        if (connection->getMethod() == IPCMethod::UNIX_SOCKET ||
            connection->getMethod() == IPCMethod::NAMED_PIPE) {
            request.source_socket = connection->getRemoteAddress();
        } else {
            request.source_ip = connection->getRemoteAddress();
        }
    }

    core::CatalogManager::PrincipalAccountCatalogInfo account;
    core::ErrorContext resolve_ctx;
    const core::Status resolve_status = catalog->resolvePrincipalAccount(request, account, &resolve_ctx);
    if (resolve_status == core::Status::INVALID_AUTHORIZATION ||
        resolve_status == core::Status::NOT_FOUND) {
        return true;
    }
    if (resolve_status != core::Status::OK) {
        error_out = resolve_ctx.message.empty() ? "mfa_policy_resolution_failed" : resolve_ctx.message;
        return false;
    }

    core::CatalogManager::AuthPolicyCatalogInfo policy;
    const core::Status policy_status =
        catalog->getAuthPolicyCatalogEntry(account.auth_policy_id, policy, &resolve_ctx);
    if (policy_status == core::Status::NOT_FOUND) {
        return true;
    }
    if (policy_status != core::Status::OK) {
        error_out = resolve_ctx.message.empty() ? "mfa_policy_lookup_failed" : resolve_ctx.message;
        return false;
    }

    config_out.account_id = account.account_id;
    config_out.mfa_required = policy.mfa_required;
    if (!policy.mfa_required) {
        return true;
    }

    if (policy.has_mfa_policy && policy.mfa_policy_id != core::ID{}) {
        core::CatalogManager::MfaPolicyCatalogInfo mfa_policy;
        const core::Status mfa_policy_status =
            catalog->getMfaPolicyCatalogEntry(policy.mfa_policy_id, mfa_policy, &resolve_ctx);
        if (mfa_policy_status != core::Status::OK) {
            error_out = resolve_ctx.message.empty() ? "mfa_policy_lookup_failed" : resolve_ctx.message;
            return false;
        }
        config_out.mfa_policy_id = mfa_policy.mfa_policy_id;
        config_out.allow_recovery_codes = mfa_policy.allow_recovery_codes;
        config_out.allow_break_glass = mfa_policy.allow_break_glass;
        config_out.max_challenge_attempts = std::max<uint8_t>(1u, mfa_policy.max_challenge_attempts);
        config_out.step_up_ttl_ms = mfa_policy.step_up_ttl_ms;
    }

    return true;
}

bool verifyCatalogMfaCode(core::CatalogManager* catalog,
                          const ResolvedMfaAuthConfig& config,
                          const std::string& username,
                          const std::string& code,
                          MfaVerificationResult& verify_out,
                          std::string& error_out) {
    verify_out = MfaVerificationResult{};
    error_out.clear();
    if (!catalog) {
        error_out = "mfa_catalog_unavailable";
        return false;
    }

    core::CatalogManager::MfaEnrollmentCatalogInfo enrollment;
    if (!ensureCatalogMfaEnrollmentForUser(catalog,
                                           config.account_id,
                                           config.mfa_policy_id,
                                           username,
                                           enrollment,
                                           error_out)) {
        return false;
    }

    security::OtpConfig otp;
    otp.digits = enrollment.totp_digits;
    otp.period = enrollment.totp_period;
    otp.look_ahead = enrollment.totp_look_ahead;
    otp.look_behind = enrollment.totp_look_behind;

    const std::vector<uint8_t> secret = security::decodeBase32(enrollment.secret_base32);
    if (secret.empty()) {
        error_out = "mfa_secret_invalid";
        return false;
    }

    if (security::verifyTotp(secret, code, otp)) {
        enrollment.has_last_verified_time = true;
        enrollment.last_verified_time_utc = static_cast<uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count());
        core::ErrorContext update_ctx;
        const core::Status update_status = catalog->upsertMfaEnrollmentCatalogEntry(enrollment, &update_ctx);
        if (update_status != core::Status::OK) {
            error_out = update_ctx.message.empty() ? "mfa_enrollment_update_failed" : update_ctx.message;
            return false;
        }
        verify_out.success = true;
        return true;
    }

    if (!config.allow_recovery_codes) {
        return true;
    }

    core::CatalogManager::MfaRecoveryCodeCatalogInfo consumed{};
    core::ErrorContext consume_ctx;
    const core::Status consume_status = catalog->consumeMfaRecoveryCode(
        config.account_id,
        hashMfaCodeSha256(code),
        config.allow_break_glass,
        consumed,
        &consume_ctx);
    if (consume_status == core::Status::OK) {
        verify_out.success = true;
        verify_out.recovery_used = true;
        verify_out.break_glass_used = consumed.is_break_glass;
        return true;
    }
    if (consume_status == core::Status::INVALID_AUTHORIZATION ||
        consume_status == core::Status::PERMISSION_DENIED ||
        consume_status == core::Status::NOT_FOUND) {
        return true;
    }

    error_out = consume_ctx.message.empty() ? "mfa_recovery_validation_failed" : consume_ctx.message;
    return false;
}

std::vector<std::string> leadingSqlKeywords(const std::string& sql, size_t limit = 4) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < sql.size() && out.size() < limit) {
        while (i < sql.size() && std::isspace(static_cast<unsigned char>(sql[i]))) {
            ++i;
        }
        if (i + 1 < sql.size() && sql[i] == '-' && sql[i + 1] == '-') {
            i += 2;
            while (i < sql.size() && sql[i] != '\n') {
                ++i;
            }
            continue;
        }
        if (i + 1 < sql.size() && sql[i] == '/' && sql[i + 1] == '*') {
            i += 2;
            while (i + 1 < sql.size() && !(sql[i] == '*' && sql[i + 1] == '/')) {
                ++i;
            }
            if (i + 1 < sql.size()) {
                i += 2;
            }
            continue;
        }
        if (i >= sql.size() || !std::isalpha(static_cast<unsigned char>(sql[i]))) {
            break;
        }
        const size_t start = i;
        while (i < sql.size()) {
            const unsigned char c = static_cast<unsigned char>(sql[i]);
            if (!std::isalnum(c) && c != '_') {
                break;
            }
            ++i;
        }
        out.push_back(toUpperAscii(sql.substr(start, i - start)));
    }
    return out;
}

bool requiresMfaStepUpForSql(const std::string& sql) {
    const std::vector<std::string> tokens = leadingSqlKeywords(sql, 4);
    if (tokens.empty()) {
        return false;
    }

    const std::string first = tokens[0];
    const std::string second = tokens.size() > 1 ? tokens[1] : std::string();
    const std::string third = tokens.size() > 2 ? tokens[2] : std::string();

    if ((first == "SET" || first == "RESET") && second == "ROLE") {
        return true;
    }
    if (first == "GRANT" || first == "REVOKE") {
        return true;
    }
    if (first == "CREATE" || first == "ALTER" || first == "DROP") {
        if (second == "USER" || second == "ROLE" || second == "GROUP" ||
            second == "POLICY" || second == "TOKEN") {
            return true;
        }
        if (second == "AUTH" && third == "POLICY") {
            return true;
        }
        if (second == "CONNECTION" && third == "RULE") {
            return true;
        }
    }
    return false;
}

bool mapConnectionAuthMethodToProtocol(core::CatalogManager::ConnectionAuthMethod method,
                                       protocol::AuthMethod& out) {
    using CAM = core::CatalogManager::ConnectionAuthMethod;
    switch (method) {
        case CAM::PASSWORD:
            out = protocol::AuthMethod::PASSWORD;
            return true;
        case CAM::MD5:
            out = protocol::AuthMethod::MD5;
            return true;
        case CAM::SCRAM_SHA_256:
            out = protocol::AuthMethod::SCRAM_SHA_256;
            return true;
        case CAM::SCRAM_SHA_512:
            out = protocol::AuthMethod::SCRAM_SHA_512;
            return true;
        case CAM::TOKEN:
            out = protocol::AuthMethod::TOKEN;
            return true;
        case CAM::PEER:
            out = protocol::AuthMethod::PEER;
            return true;
        default:
            return false;
    }
}

uint16_t authMethodMaskBit(protocol::AuthMethod method) {
    using CM = core::CatalogManager;
    switch (method) {
        case protocol::AuthMethod::PASSWORD:
            return CM::AUTH_POLICY_METHOD_PASSWORD;
        case protocol::AuthMethod::MD5:
            return CM::AUTH_POLICY_METHOD_MD5;
        case protocol::AuthMethod::SCRAM_SHA_256:
            return CM::AUTH_POLICY_METHOD_SCRAM_SHA_256;
        case protocol::AuthMethod::SCRAM_SHA_512:
            return CM::AUTH_POLICY_METHOD_SCRAM_SHA_512;
        case protocol::AuthMethod::TOKEN:
            return CM::AUTH_POLICY_METHOD_TOKEN;
        case protocol::AuthMethod::PEER:
            return CM::AUTH_POLICY_METHOD_PEER;
    }
    return 0;
}

bool isAuthMethodAllowedByMask(protocol::AuthMethod method, uint16_t mask) {
    const uint16_t bit = authMethodMaskBit(method);
    return bit != 0 && (mask & bit) != 0;
}

core::CatalogManager::ConnectionTransport resolveConnectionTransport(const IPCConnection* connection) {
    using CT = core::CatalogManager::ConnectionTransport;
    if (!connection) {
        return CT::LOCAL;
    }
    switch (connection->getMethod()) {
        case IPCMethod::UNIX_SOCKET:
        case IPCMethod::NAMED_PIPE:
            return CT::IPC;
        case IPCMethod::TCP_LOCALHOST:
            return CT::INET;
        case IPCMethod::AUTO:
        default:
            return CT::LOCAL;
    }
}

bool isTrustedLocalIpc(const IPCConnection* connection) {
    if (!connection) {
        return false;
    }
    const IPCMethod method = connection->getMethod();
    return method == IPCMethod::UNIX_SOCKET || method == IPCMethod::NAMED_PIPE;
}

uint8_t transportMaskBit(core::CatalogManager::ConnectionTransport transport) {
    using CM = core::CatalogManager;
    switch (transport) {
        case CM::ConnectionTransport::LOCAL:
            return CM::AUTH_POLICY_TRANSPORT_LOCAL;
        case CM::ConnectionTransport::IPC:
            return CM::AUTH_POLICY_TRANSPORT_IPC;
        case CM::ConnectionTransport::INET:
            return CM::AUTH_POLICY_TRANSPORT_INET;
    }
    return 0;
}

bool isTransportAllowedByMask(core::CatalogManager::ConnectionTransport transport, uint8_t mask) {
    const uint8_t bit = transportMaskBit(transport);
    return bit != 0 && (mask & bit) != 0;
}

core::CatalogManager::ConnectionRuleTransportKind resolveConnectionRuleTransportKind(
    const IPCConnection* connection,
    bool trusted_proxy_channel) {
    using CRT = core::CatalogManager::ConnectionRuleTransportKind;
    if (trusted_proxy_channel) {
        return CRT::MTLS;
    }
    if (!connection) {
        return CRT::IPC_LOCAL;
    }

    switch (connection->getMethod()) {
        case IPCMethod::UNIX_SOCKET:
            return CRT::UNIX_SOCKET;
        case IPCMethod::NAMED_PIPE:
            return CRT::IPC_LOCAL;
        case IPCMethod::TCP_LOCALHOST:
            return CRT::TCP;
        case IPCMethod::AUTO:
        default:
            return CRT::IPC_LOCAL;
    }
}

std::string resolveConnectionRuleProfileScope() {
    const char* env = std::getenv("SCRATCHBIRD_CONNECTION_RULE_PROFILE");
    if (!env || env[0] == '\0') {
        return "native";
    }
    return env;
}

bool evaluateIngressConnectionRules(core::CatalogManager* catalog,
                                    const IPCConnection* connection,
                                    const std::string& target_database,
                                    bool trusted_proxy_channel,
                                    std::string& reject_code_out,
                                    std::string& reject_reason_out) {
    reject_code_out.clear();
    reject_reason_out.clear();
    if (!catalog) {
        return true;
    }

    const std::string profile_scope = resolveConnectionRuleProfileScope();
    std::vector<core::CatalogManager::ConnectionRuleCatalogInfo> rules;
    core::ErrorContext list_ctx;
    const core::Status list_status =
        catalog->listConnectionRuleCatalogEntries(profile_scope, rules, &list_ctx);
    if (list_status == core::Status::NOT_FOUND ||
        (list_status == core::Status::OK && rules.empty())) {
        return true;
    }
    if (list_status != core::Status::OK) {
        reject_code_out = list_ctx.vnext_code.empty() ? "SEC_1230" : list_ctx.vnext_code;
        reject_reason_out = list_ctx.message.empty()
            ? "ingress_connection_rule_lookup_failed"
            : list_ctx.message;
        return false;
    }

    core::CatalogManager::ConnectionRuleEvaluationRequest request{};
    request.profile_scope = profile_scope;
    request.transport_kind =
        resolveConnectionRuleTransportKind(connection, trusted_proxy_channel);
    request.target_db = target_database;
    if (connection) {
        request.remote_address = connection->getRemoteAddress();
        const IPCMethod method = connection->getMethod();
        if (method == IPCMethod::UNIX_SOCKET || method == IPCMethod::NAMED_PIPE) {
            request.source_socket = request.remote_address;
            request.source_host = request.remote_address;
        } else {
            request.source_ip = request.remote_address;
            request.source_host = request.remote_address;
        }
    }
    if (trusted_proxy_channel) {
        request.has_forwarded_identity = true;
        request.trusted_proxy_channel = true;
        request.proxy_identity = "manager_proxy";
    }

    core::CatalogManager::ConnectionRuleEvaluationDecision decision{};
    core::ErrorContext eval_ctx;
    const core::Status eval_status =
        catalog->evaluateConnectionRuleChain(request, decision, &eval_ctx);
    if (eval_status != core::Status::OK) {
        reject_code_out = eval_ctx.vnext_code.empty() ? "SEC_1231" : eval_ctx.vnext_code;
        reject_reason_out = eval_ctx.message.empty()
            ? "ingress_connection_denied"
            : eval_ctx.message;
        return false;
    }
    return true;
}

std::vector<protocol::AuthMethod> authMethodsFromMask(uint16_t mask) {
    std::vector<protocol::AuthMethod> methods;
    if ((mask & core::CatalogManager::AUTH_POLICY_METHOD_PASSWORD) != 0) {
        methods.push_back(protocol::AuthMethod::PASSWORD);
    }
    if ((mask & core::CatalogManager::AUTH_POLICY_METHOD_MD5) != 0) {
        methods.push_back(protocol::AuthMethod::MD5);
    }
    if ((mask & core::CatalogManager::AUTH_POLICY_METHOD_SCRAM_SHA_256) != 0) {
        methods.push_back(protocol::AuthMethod::SCRAM_SHA_256);
    }
    if ((mask & core::CatalogManager::AUTH_POLICY_METHOD_SCRAM_SHA_512) != 0) {
        methods.push_back(protocol::AuthMethod::SCRAM_SHA_512);
    }
    if ((mask & core::CatalogManager::AUTH_POLICY_METHOD_TOKEN) != 0) {
        methods.push_back(protocol::AuthMethod::TOKEN);
    }
    if ((mask & core::CatalogManager::AUTH_POLICY_METHOD_PEER) != 0) {
        methods.push_back(protocol::AuthMethod::PEER);
    }
    return methods;
}

std::vector<uint8_t> generateNegotiationNonce(size_t bytes = 16) {
    std::vector<uint8_t> nonce(bytes, 0);
    std::random_device rd;
    for (size_t i = 0; i < bytes; ++i) {
        nonce[i] = static_cast<uint8_t>(rd() & 0xFF);
    }
    return nonce;
}

bool resolveAuthNegotiationPolicy(core::CatalogManager* catalog,
                                  const IPCConnection* connection,
                                  std::vector<protocol::AuthMethod>& allowed_methods_out,
                                  bool& has_required_method_out,
                                  protocol::AuthMethod& required_method_out,
                                  uint8_t& allowed_transport_mask_out,
                                  core::CatalogManager::AuthPeerMode& peer_mode_out,
                                  std::string& policy_deny_code_out) {
    using CM = core::CatalogManager;
    policy_deny_code_out.clear();

    constexpr uint16_t kLegacyMethodMask =
        CM::AUTH_POLICY_METHOD_PASSWORD | CM::AUTH_POLICY_METHOD_MD5;

    // Safe fallback if auth policy catalog rows are not configured yet.
    uint16_t allowed_method_mask =
        CM::AUTH_POLICY_METHOD_SCRAM_SHA_256 |
        CM::AUTH_POLICY_METHOD_SCRAM_SHA_512 |
        CM::AUTH_POLICY_METHOD_TOKEN;
    allowed_transport_mask_out = CM::AUTH_POLICY_TRANSPORT_ALL;
    has_required_method_out = true;
    required_method_out = protocol::AuthMethod::SCRAM_SHA_256;
    peer_mode_out = CM::AuthPeerMode::DISABLED;
    bool allow_legacy_password_fallback = false;

    if (catalog) {
        std::vector<CM::AuthPolicyCatalogInfo> policies;
        core::ErrorContext list_ctx;
        const core::Status list_status = catalog->listAuthPolicyCatalogEntries(policies, &list_ctx);
        if (list_status == core::Status::OK && !policies.empty()) {
            const char* policy_name_env = std::getenv("SCRATCHBIRD_AUTH_POLICY_NAME");
            const std::string requested_policy =
                (policy_name_env && policy_name_env[0] != '\0')
                    ? toUpperAscii(policy_name_env)
                    : std::string();

            const CM::AuthPolicyCatalogInfo* selected = nullptr;
            if (!requested_policy.empty()) {
                for (const auto& policy : policies) {
                    if (toUpperAscii(policy.policy_name) == requested_policy) {
                        selected = &policy;
                        break;
                    }
                }
            }
            if (!selected) {
                for (const auto& policy : policies) {
                    if (toUpperAscii(policy.policy_name) == "DEFAULT") {
                        selected = &policy;
                        break;
                    }
                }
            }
            if (!selected) {
                selected = &policies.front();
            }

            allowed_method_mask = selected->allowed_auth_method_mask;
            allowed_transport_mask_out = selected->allowed_transport_mask;
            has_required_method_out = selected->has_required_auth_method;
            peer_mode_out = selected->peer_mode;
            allow_legacy_password_fallback = selected->allow_password_fallback;
            if (has_required_method_out &&
                !mapConnectionAuthMethodToProtocol(selected->required_auth_method, required_method_out)) {
                policy_deny_code_out = kAuthPolicyMethodDeniedCode;
                return false;
            }
        } else if (list_status != core::Status::NOT_FOUND &&
                   list_status != core::Status::OK) {
            policy_deny_code_out = kAuthPolicyNegotiationRequiredCode;
            return false;
        }
    }

    bool bootstrap_phase_a = false;
    std::string bootstrap_phase_error;
    if (!detectBootstrapPhaseA(catalog, bootstrap_phase_a, bootstrap_phase_error)) {
        policy_deny_code_out = kAuthPolicyNegotiationRequiredCode;
        return false;
    }

    if (bootstrap_phase_a) {
        // Bootstrap login relies on one-time token proof in PASSWORD payload.
        allowed_method_mask = CM::AUTH_POLICY_METHOD_PASSWORD;
        has_required_method_out = true;
        required_method_out = protocol::AuthMethod::PASSWORD;
        peer_mode_out = CM::AuthPeerMode::DISABLED;
    } else {
        // Legacy PASSWORD/MD5 are only legal when policy explicitly allows fallback
        // and the transport is a trusted local IPC channel.
        if (!allow_legacy_password_fallback || !isTrustedLocalIpc(connection)) {
            allowed_method_mask &= static_cast<uint16_t>(~kLegacyMethodMask);
        }

        if (peer_mode_out != CM::AuthPeerMode::DISABLED) {
            if (!connection || connection->getMethod() != IPCMethod::UNIX_SOCKET) {
                policy_deny_code_out = kAuthPolicyPeerTransportCode;
                return false;
            }

            const PeerCredentials peer = connection->getPeerCredentials();
            if (!peer.available) {
                policy_deny_code_out = kAuthPolicyPeerRequiredCode;
                return false;
            }

            if (peer_mode_out == CM::AuthPeerMode::REQUIRED_PLUS_SCRAM) {
                const uint16_t scram_mask =
                    CM::AUTH_POLICY_METHOD_SCRAM_SHA_256 |
                    CM::AUTH_POLICY_METHOD_SCRAM_SHA_512;
                allowed_method_mask &= scram_mask;

                if ((allowed_method_mask & scram_mask) == 0) {
                    policy_deny_code_out = kAuthPolicyMethodDeniedCode;
                    return false;
                }

                has_required_method_out = true;
                if ((allowed_method_mask & CM::AUTH_POLICY_METHOD_SCRAM_SHA_256) != 0) {
                    required_method_out = protocol::AuthMethod::SCRAM_SHA_256;
                } else {
                    required_method_out = protocol::AuthMethod::SCRAM_SHA_512;
                }
            }
        }
    }

    allowed_methods_out = authMethodsFromMask(allowed_method_mask);
    if (allowed_methods_out.empty()) {
        policy_deny_code_out = kAuthPolicyMethodDeniedCode;
        return false;
    }

    if (has_required_method_out &&
        !isAuthMethodAllowedByMask(required_method_out, allowed_method_mask)) {
        policy_deny_code_out = kAuthPolicyRequiredMethodCode;
        return false;
    }

    const CM::ConnectionTransport transport = resolveConnectionTransport(connection);
    if (!isTransportAllowedByMask(transport, allowed_transport_mask_out)) {
        policy_deny_code_out = kAuthPolicyTransportDeniedCode;
        return false;
    }

    return true;
}

core::Status sendCopyInPreamble(protocol::ProtocolSession* session,
                                CopyStreamState& state,
                                core::ErrorContext* ctx) {
    auto status = session->sendMessage(
        protocol::ProtocolCodec::buildCopyInResponse(protocol::CopyFormat::TEXT, {}), ctx);
    if (status != core::Status::OK) {
        return status;
    }
    status = session->sendMessage(
        protocol::ProtocolCodec::buildStreamReady(state.stream_id, 0, 0), ctx);
    if (status != core::Status::OK) {
        return status;
    }
    state.in_window_bytes += state.in_window_grant;
    status = session->sendMessage(
        protocol::ProtocolCodec::buildStreamControl(protocol::StreamControlType::START,
                                                    state.in_window_grant, 0),
        ctx);
    return status;
}

core::Status sendCopyOutPreamble(protocol::ProtocolSession* session,
                                 CopyStreamState& state,
                                 core::ErrorContext* ctx) {
    auto status = session->sendMessage(
        protocol::ProtocolCodec::buildCopyOutResponse(protocol::CopyFormat::TEXT, {}), ctx);
    if (status != core::Status::OK) {
        return status;
    }
    return session->sendMessage(
        protocol::ProtocolCodec::buildStreamReady(state.stream_id, 0, 0), ctx);
}

bool readCopyInChunk(protocol::ProtocolSession* session,
                     CopyStreamState& state,
                     std::string& out,
                     bool& done,
                     std::string& error) {
    out.clear();
    done = false;

    while (true) {
        protocol::Message msg;
        core::ErrorContext ctx;
        auto status = session->receiveMessage(msg, &ctx);
        if (status != core::Status::OK) {
            error = ctx.message.empty() ? "Failed to receive COPY data" : ctx.message;
            return false;
        }

        switch (msg.getType()) {
            case protocol::MessageType::COPY_DATA: {
                const uint8_t* data = nullptr;
                size_t len = 0;
                if (protocol::ProtocolCodec::parseCopyData(msg, &data, &len, nullptr) !=
                    core::Status::OK) {
                    error = "Malformed COPY_DATA payload";
                    return false;
                }
                out.assign(reinterpret_cast<const char*>(data), len);
                state.total_bytes += len;

                if (state.in_window_bytes > 0) {
                    if (len >= state.in_window_bytes) {
                        state.in_window_bytes = 0;
                    } else {
                        state.in_window_bytes -= static_cast<uint32_t>(len);
                    }
                }
                if (state.in_window_grant > 0 &&
                    state.in_window_bytes <= state.in_low_watermark) {
                    state.in_window_bytes += state.in_window_grant;
                    session->sendMessage(
                        protocol::ProtocolCodec::buildStreamControl(
                            protocol::StreamControlType::RESUME,
                            state.in_window_grant,
                            0),
                        nullptr);
                }
                return true;
            }
            case protocol::MessageType::COPY_DONE:
                done = true;
                return true;
            case protocol::MessageType::COPY_FAIL: {
                std::string fail;
                protocol::ProtocolCodec::parseCopyFail(msg, fail, nullptr);
                error = fail.empty() ? "COPY failed" : fail;
                return false;
            }
            case protocol::MessageType::STREAM_CONTROL: {
                protocol::StreamControlType control;
                uint32_t window = 0;
                uint32_t timeout_ms = 0;
                if (protocol::ProtocolCodec::parseStreamControl(
                        msg, control, window, timeout_ms) != core::Status::OK) {
                    error = "Malformed STREAM_CONTROL message";
                    return false;
                }
                (void)timeout_ms;
                if (control == protocol::StreamControlType::CANCEL) {
                    error = "COPY canceled by client";
                    return false;
                }
                break;
            }
            case protocol::MessageType::QUERY_CANCEL:
                error = "COPY canceled by client";
                return false;
            default:
                error = "Unexpected message during COPY IN";
                return false;
        }
    }
}

bool waitForCopyOutWindow(protocol::ProtocolSession* session,
                          CopyStreamState& state,
                          std::string& error) {
    while (state.out_window_bytes == 0 || state.out_paused) {
        protocol::Message msg;
        core::ErrorContext ctx;
        auto status = session->receiveMessage(msg, &ctx);
        if (status != core::Status::OK) {
            error = ctx.message.empty() ? "Failed to receive STREAM_CONTROL" : ctx.message;
            return false;
        }

        switch (msg.getType()) {
            case protocol::MessageType::STREAM_CONTROL: {
                protocol::StreamControlType control;
                uint32_t window = 0;
                uint32_t timeout_ms = 0;
                if (protocol::ProtocolCodec::parseStreamControl(
                        msg, control, window, timeout_ms) != core::Status::OK) {
                    error = "Malformed STREAM_CONTROL message";
                    return false;
                }
                (void)timeout_ms;
                if (control == protocol::StreamControlType::PAUSE) {
                    state.out_paused = true;
                    break;
                }
                if (control == protocol::StreamControlType::RESUME ||
                    control == protocol::StreamControlType::START ||
                    control == protocol::StreamControlType::ACK) {
                    state.out_paused = false;
                    state.out_window_bytes += window;
                    break;
                }
                if (control == protocol::StreamControlType::CANCEL) {
                    error = "COPY canceled by client";
                    return false;
                }
                break;
            }
            case protocol::MessageType::QUERY_CANCEL:
                error = "COPY canceled by client";
                return false;
            default:
                error = "Unexpected message during COPY OUT";
                return false;
        }
    }
    return true;
}

bool sendCopyOutChunk(protocol::ProtocolSession* session,
                      CopyStreamState& state,
                      const uint8_t* data,
                      size_t len,
                      std::string& error) {
    size_t offset = 0;
    while (offset < len) {
        if (state.out_window_bytes == 0 || state.out_paused) {
            if (!waitForCopyOutWindow(session, state, error)) {
                return false;
            }
        }

        size_t chunk = len - offset;
        if (state.out_window_bytes > 0) {
            chunk = std::min<size_t>(chunk, state.out_window_bytes);
        }

        auto status = session->sendMessage(
            protocol::ProtocolCodec::buildCopyData(data + offset, chunk), nullptr);
        if (status != core::Status::OK) {
            error = "Failed to send COPY data";
            return false;
        }

        state.out_window_bytes = (state.out_window_bytes > chunk)
            ? (state.out_window_bytes - static_cast<uint32_t>(chunk))
            : 0;
        state.total_bytes += chunk;
        offset += chunk;
    }
    return true;
}

} // namespace

// ============================================================================
// ServerSession Implementation
// ============================================================================

ServerSession::ServerSession(IPCConnection* connection,
                             core::Database* database,
                             const uint8_t session_id[16])
    : connection_(connection)
    , database_(database)
    , state_(SessionState::CREATED)
    , shutdown_requested_(false)
{
    std::memcpy(session_id_, session_id, 16);

    // Create protocol session
    protocol_session_ = std::make_unique<protocol::ProtocolSession>(connection);

    // Initialize statistics
    stats_.created_at = std::chrono::steady_clock::now();
    stats_.last_activity = stats_.created_at;

    // Get client info if available
    peer_credentials_ = connection->getPeerCredentials();
    peer_credentials_available_ = peer_credentials_.available;
    if (peer_credentials_available_) {
        std::ostringstream oss;
        oss << "pid=" << peer_credentials_.pid
            << ",uid=" << peer_credentials_.uid
            << ",gid=" << peer_credentials_.gid;
        client_info_ = oss.str();
    } else if (peer_credentials_.pid != 0) {
        client_info_ = "pid=" + std::to_string(peer_credentials_.pid) +
                       ",mapping=unavailable";
    } else {
        client_info_ = "unknown";
    }

    // Create executor
    executor_ = std::make_unique<sblr::Executor>(database_);
}

ServerSession::~ServerSession() {
    // Ensure session is closed
    if (state_ != SessionState::CLOSED) {
        state_ = SessionState::CLOSED;
    }
}

std::string ServerSession::sessionIdString() const {
    return protocol::sessionIdToString(session_id_);
}

core::Status ServerSession::run() {
    state_ = SessionState::CREATED;

    protocol::Message msg;
    core::ErrorContext ctx;

    while (!shutdown_requested_ && connection_->isOpen()) {
        // Receive next message
        core::Status status = protocol_session_->receiveMessage(msg, &ctx);

        if (status != core::Status::OK) {
            if (status == core::Status::CONNECTION_FAILURE) {
                // Clean disconnect
                std::fprintf(stderr,
                             "[ipc_debug] server session %s disconnect: %s\n",
                             sessionIdString().c_str(),
                             ctx.message.empty() ? "connection failure" : ctx.message.c_str());
                break;
            }
            // Error receiving message
            std::fprintf(stderr,
                         "[ipc_debug] server session %s receive error: %d %s\n",
                         sessionIdString().c_str(),
                         static_cast<int>(status),
                         ctx.message.empty() ? "none" : ctx.message.c_str());
            sendError("Protocol error: " + ctx.message);
            break;
        }

        // Update activity timestamp
        stats_.last_activity = std::chrono::steady_clock::now();

        // Process the message
        status = processMessage(msg, &ctx);

        if (status != core::Status::OK) {
            // Don't break on most errors, just continue
            // Only break on serious protocol/connection errors
            if (status == core::Status::CONNECTION_FAILURE ||
                status == core::Status::PROTOCOL_VIOLATION) {
                break;
            }
        }

        // Check for disconnect
        if (state_ == SessionState::CLOSING || state_ == SessionState::CLOSED) {
            break;
        }
    }

    // If the client vanished without a DISCONNECT message, preserve the transaction if possible.
    if (conn_ctx_ && state_ != SessionState::CLOSED && state_ != SessionState::CLOSING) {
        fireDatabaseTriggers(core::CatalogManager::DatabaseTriggerEvent::ON_DISCONNECT);

        core::ErrorContext detach_ctx;
        core::ID dormant_id;
        core::Status detach_status = database_->detachToDormant(conn_ctx_, dormant_id, &detach_ctx);
        if (detach_status != core::Status::OK) {
            conn_ctx_->shutdownTransaction(&detach_ctx);
            conn_ctx_.reset();
        }
    }

    static const core::ID zero_id{};
    if (database_ && database_->catalog_manager() && session_id_uuid_ != zero_id) {
        core::ErrorContext close_ctx;
        database_->catalog_manager()->closeSession(session_id_uuid_, &close_ctx);
    }

    state_ = SessionState::CLOSED;
    return core::Status::OK;
}

void ServerSession::requestShutdown() {
    shutdown_requested_ = true;
    state_ = SessionState::CLOSING;
}

core::Status ServerSession::processMessage(const protocol::Message& msg, core::ErrorContext* ctx) {
    protocol::MessageType type = msg.getType();

    switch (type) {
        case protocol::MessageType::CONNECT_REQUEST:
            return handleConnect(msg, ctx);

        case protocol::MessageType::AUTH_REQUEST:
            return handleAuth(msg, ctx);

        case protocol::MessageType::DISCONNECT:
            return handleDisconnect(msg, ctx);

        case protocol::MessageType::QUERY:
            return handleQuery(msg, ctx);

        case protocol::MessageType::BEGIN_TRANSACTION:
        case protocol::MessageType::COMMIT:
        case protocol::MessageType::ROLLBACK:
        case protocol::MessageType::SAVEPOINT:
        case protocol::MessageType::RELEASE_SAVEPOINT:
        case protocol::MessageType::ROLLBACK_TO:
            return handleTransaction(msg, ctx);

        case protocol::MessageType::PING:
            return handlePing(msg, ctx);

        case protocol::MessageType::QUERY_CANCEL:
            return handleCancel(msg, ctx);

        default:
            sendError("Unsupported message type: " +
                     std::string(protocol::messageTypeToString(type)));
            return core::Status::INVALID_ARGUMENT;
    }
}

core::Status ServerSession::handleConnect(const protocol::Message& msg, core::ErrorContext* ctx) {
    // Parse connect request using the ProtocolCodec
    std::string database, client_name;
    uint32_t client_pid;
    uint16_t client_flags = 0;
    std::array<uint8_t, 16> bound_db_uuid{};
    bool has_bound_db_uuid = false;

    core::Status status = protocol::ProtocolCodec::parseConnectRequest(
        msg, database, client_name, client_pid, &client_flags, ctx, &bound_db_uuid,
        &has_bound_db_uuid);

    if (status != core::Status::OK) {
        sendError("Invalid connect request");
        return status;
    }

    const bool manager_bound_connect =
        (client_flags & protocol::CONNECT_FLAG_MANAGER_DBBT) != 0;
    const bool requires_bound_uuid =
        manager_bound_connect ||
        (client_flags & protocol::CONNECT_FLAG_BOUND_DB_UUID) != 0;

    if (requires_bound_uuid && !has_bound_db_uuid) {
        const char* message = manager_bound_connect
            ? "Manager-bound connect missing database UUID"
            : "Connection missing required database UUID binding";
        protocol::Message response = protocol::ProtocolCodec::buildConnectResponse(
            false, session_id_, message);
        protocol_session_->sendMessage(response, ctx);
        return core::Status::INVALID_AUTHORIZATION;
    }

    if (requires_bound_uuid) {
        const std::array<uint8_t, 16> resolved_db_uuid =
            deriveBindingDatabaseUuid(database);
        const bool matches_engine_uuid =
            database_ && (bound_db_uuid == database_->uuid().bytes);
        const bool matches_resolved_uuid = bound_db_uuid == resolved_db_uuid;
        if (!matches_engine_uuid && !matches_resolved_uuid) {
            const char* message = manager_bound_connect
                ? "Manager-bound connect database UUID mismatch"
                : "Connection database UUID mismatch";
            protocol::Message response = protocol::ProtocolCodec::buildConnectResponse(
                false, session_id_, message);
            protocol_session_->sendMessage(response, ctx);
            return core::Status::INVALID_AUTHORIZATION;
        }
    }

    {
        std::string reject_code;
        std::string reject_reason;
        const bool trusted_proxy_channel = manager_bound_connect;
        if (!evaluateIngressConnectionRules(
                database_ ? database_->catalog_manager() : nullptr,
                connection_,
                database,
                trusted_proxy_channel,
                reject_code,
                reject_reason)) {
            std::string error = reject_reason.empty()
                ? "Connection denied by ingress policy"
                : reject_reason;
            if (!reject_code.empty()) {
                error = reject_code + ": " + error;
            }
            protocol::Message response = protocol::ProtocolCodec::buildConnectResponse(
                false, session_id_, error);
            protocol_session_->sendMessage(response, ctx);
            return core::Status::INVALID_AUTHORIZATION;
        }
    }

    client_info_ = client_name + " (pid=" + std::to_string(client_pid) + ")";
    if (peer_credentials_available_) {
        client_info_ += " [peer uid=" + std::to_string(peer_credentials_.uid) +
                        " gid=" + std::to_string(peer_credentials_.gid) +
                        " pid=" + std::to_string(peer_credentials_.pid) + "]";
    } else if (connection_ &&
               connection_->getMethod() == IPCMethod::NAMED_PIPE &&
               peer_credentials_.pid != 0) {
        client_info_ += " [peer pid=" + std::to_string(peer_credentials_.pid) +
                        " mapping=unavailable]";
    }

    // Check if database is open
    if (!database_->is_open()) {
        // Send failure response
    protocol::Message response = protocol::ProtocolCodec::buildConnectResponse(
        false, session_id_, "Database is not open");
        protocol_session_->sendMessage(response, ctx);
        return core::Status::IO_ERROR;
    }

    // Send success response with session ID
    protocol::Message response = protocol::ProtocolCodec::buildConnectResponse(
        true, session_id_, "Connected to ScratchBird");
    status = protocol_session_->sendMessage(response, ctx);

    if (status == core::Status::OK) {
        state_ = SessionState::AUTHENTICATING;
    }

    return status;
}

core::Status ServerSession::handleAuth(const protocol::Message& msg, core::ErrorContext* ctx) {
    if (state_ != SessionState::AUTHENTICATING && state_ != SessionState::CREATED) {
        sendError("Authentication not expected in current state");
        return core::Status::INVALID_ARGUMENT;
    }

    // Parse auth request using ProtocolCodec
    uint8_t session_id[16];
    std::string username;
    protocol::AuthMethod auth_method = protocol::AuthMethod::PASSWORD;
    std::vector<uint8_t> auth_payload;

    core::Status status = protocol::ProtocolCodec::parseAuthRequest(
        msg, session_id, username, auth_method, auth_payload, ctx);

    if (status != core::Status::OK) {
        sendError("Invalid auth request");
        return status;
    }

    auto* catalog = database_ ? database_->catalog_manager() : nullptr;
    auto* audit_logger = database_ ? database_->audit_logger() : nullptr;

    auto clear_auth_negotiation = [&]() {
        auth_negotiation_ready_ = false;
        auth_negotiation_username_.clear();
        auth_negotiation_allowed_methods_.clear();
        auth_negotiation_has_required_method_ = false;
        auth_negotiation_required_method_ = protocol::AuthMethod::SCRAM_SHA_256;
        auth_negotiation_transport_mask_ = 0;
        auth_negotiation_nonce_.clear();
        auth_negotiation_peer_mode_ = core::CatalogManager::AuthPeerMode::DISABLED;
    };

    auto clear_pending_mfa = [&]() {
        pending_mfa_auth_ = PendingMfaAuthState{};
    };

    auto clear_session_mfa_state = [&]() {
        session_mfa_verified_ = false;
        session_mfa_used_recovery_ = false;
        session_mfa_used_break_glass_ = false;
        session_mfa_verified_time_ms_ = 0;
        session_mfa_step_up_ttl_ms_ = 0;
    };

    auto log_auth_policy_decision = [&](bool success, const char* code) {
        const std::string decision_code =
            (code && code[0] != '\0') ? code : kAuthPolicyMethodDeniedCode;
        logAuthPolicyDecision(audit_logger,
                              username,
                              success,
                              decision_code,
                              auth_method,
                              auth_negotiation_allowed_methods_,
                              auth_negotiation_has_required_method_,
                              auth_negotiation_required_method_,
                              auth_negotiation_transport_mask_,
                              auth_negotiation_peer_mode_);
    };

    auto send_auth_policy_error = [&](const char* code) -> core::Status {
        log_auth_policy_decision(false, code);
        protocol::Message response = protocol::ProtocolCodec::buildAuthResponse(
            protocol::AuthStatus::FAILURE, 0, code ? code : kAuthPolicyMethodDeniedCode);
        protocol_session_->sendMessage(response, ctx);
        return core::Status::INVALID_AUTHORIZATION;
    };

    if (!pending_mfa_auth_.active) {
        clear_session_mfa_state();
    }

    auto finish_auth = [&](const core::AuthUserInfo& user_info,
                           const std::vector<uint8_t>& response_data) -> core::Status {
        username_ = user_info.username.empty() ? username : user_info.username;
        state_ = SessionState::AUTHENTICATED;
        clear_auth_negotiation();
        clear_pending_mfa();
        scram_state_.reset();

        // Create connection context
        core::Status connect_status = database_->connect(conn_ctx_, ctx);
        if (connect_status != core::Status::OK || !conn_ctx_) {
            sendError("Failed to initialize connection context");
            return connect_status;
        }
        conn_ctx_->setAutocommitMode(true);

        // Preserve the protocol session UUID for dormant reattach diagnostics.
        core::ID protocol_session_id;
        std::memcpy(protocol_session_id.bytes.data(), session_id_, 16);
        conn_ctx_->setProtocolSessionId(protocol_session_id);

        // Set user with ID and superuser flag
        conn_ctx_->setCurrentUser(user_info.user_id, user_info.is_superuser);
        if (peer_credentials_available_) {
            conn_ctx_->setSessionVariable("SB$PEER_UID", std::to_string(peer_credentials_.uid));
            conn_ctx_->setSessionVariable("SB$PEER_GID", std::to_string(peer_credentials_.gid));
            conn_ctx_->setSessionVariable("SB$PEER_PID", std::to_string(peer_credentials_.pid));
        } else if (connection_ &&
                   connection_->getMethod() == IPCMethod::NAMED_PIPE &&
                   peer_credentials_.pid != 0) {
            conn_ctx_->setSessionVariable("SB$PEER_PID", std::to_string(peer_credentials_.pid));
        }

        // Create and bind catalog session
        auto* catalog = database_->catalog_manager();
        if (catalog) {
            core::CatalogManager::SessionInfo session_info;
            core::Status session_status = catalog->createSession(
                user_info.user_id, user_info.authkey_id, conn_ctx_->emulationMode(),
                session_info, ctx);
            if (session_status != core::Status::OK) {
                sendError("Failed to create session");
                return session_status;
            }

            session_id_uuid_ = session_info.session_id;
            authkey_id_ = user_info.authkey_id;

            conn_ctx_->setCurrentSchemaId(session_info.current_schema_id);
            if (!session_info.search_path.empty())
            {
                conn_ctx_->set_search_path(session_info.search_path);
            }

            core::CatalogManager::SchemaInfo schema_info;
            if (catalog->getSchema(session_info.current_schema_id, schema_info, nullptr) == core::Status::OK)
            {
                conn_ctx_->set_current_schema(schema_info.schema_name);
            }

            conn_ctx_->setSessionContext(session_info.session_id,
                                         user_info.authkey_id,
                                         session_info.emulation_mode,
                                         session_info.policy_epoch_global,
                                         session_info.policy_epoch_table);
        } else {
            authkey_id_ = user_info.authkey_id;
            session_id_uuid_ = core::generateUuidV7();
            conn_ctx_->setSessionContext(session_id_uuid_,
                                         authkey_id_,
                                         conn_ctx_->emulationMode(),
                                         0,
                                         0);
        }

        executor_->setConnectionContext(conn_ctx_.get());

        // Fire ON CONNECT database triggers (Firebird-style)
        fireDatabaseTriggers(core::CatalogManager::DatabaseTriggerEvent::ON_CONNECT);

        // Audit login success with session/authkey context
        auto* audit_logger = database_->audit_logger();
        if (audit_logger) {
            core::AuditEvent event = core::AuditLogger::createLoginSuccessEvent(
                user_info.user_id, username_);
            event.session_id = session_id_uuid_;
            event.authkey_id = authkey_id_;
            core::ErrorContext audit_ctx;
            audit_logger->logEvent(event, &audit_ctx);
        }

        // Send success response (use first 4 bytes of UUID as uint32 for wire protocol)
        uint32_t user_id_wire = 0;
        std::memcpy(&user_id_wire, user_info.user_id.bytes.data(),
                    std::min<size_t>(4, user_info.user_id.bytes.size()));
        protocol::Message response = protocol::ProtocolCodec::buildAuthResponse(
            protocol::AuthStatus::OK, user_id_wire, "", response_data);
        return protocol_session_->sendMessage(response, ctx);
    };

    core::AuthUserInfo user_info;
    std::string auth_error;

    std::unique_ptr<core::AuthProvider> provider;
    if (catalog) {
        provider = core::AuthProviderFactory::createDefault(catalog, audit_logger);
        if (provider) {
            if (auto* local_provider = dynamic_cast<core::LocalAuthProvider*>(provider.get())) {
                local_provider->setPeerIdentityContext(peer_credentials_available_,
                                                       peer_credentials_.uid,
                                                       peer_credentials_.gid,
                                                       peer_credentials_.pid);
            }
        }
    }

    auto issue_mfa_challenge_if_required =
        [&](const core::AuthUserInfo& authenticated_user,
            protocol::AuthMethod completed_method,
            const std::vector<uint8_t>& completed_method_payload)
        -> std::optional<core::Status> {
        ResolvedMfaAuthConfig mfa_config;
        std::string mfa_policy_error;
        const std::string effective_username =
            authenticated_user.username.empty() ? username : authenticated_user.username;
        if (!resolveMfaConfig(catalog,
                              connection_,
                              effective_username,
                              mfa_config,
                              mfa_policy_error)) {
            protocol::Message response = protocol::ProtocolCodec::buildAuthResponse(
                protocol::AuthStatus::FAILURE, 0, kAuthMfaInvalidCode);
            protocol_session_->sendMessage(response, ctx);
            return core::Status::INVALID_AUTHORIZATION;
        }

        if (!mfa_config.mfa_required) {
            return std::nullopt;
        }

        core::CatalogManager::MfaEnrollmentCatalogInfo enrollment;
        std::string enrollment_error;
        if (!catalog ||
            !ensureCatalogMfaEnrollmentForUser(catalog,
                                               mfa_config.account_id,
                                               mfa_config.mfa_policy_id,
                                               effective_username,
                                               enrollment,
                                               enrollment_error)) {
            protocol::Message response = protocol::ProtocolCodec::buildAuthResponse(
                protocol::AuthStatus::FAILURE, 0, kAuthMfaRequiredCode);
            protocol_session_->sendMessage(response, ctx);
            return core::Status::INVALID_AUTHORIZATION;
        }

        const std::string challenge_id = security::generateNonce();
        const std::string challenge_method_name = "totp";

        pending_mfa_auth_.active = true;
        pending_mfa_auth_.username = effective_username;
        pending_mfa_auth_.auth_method = completed_method;
        pending_mfa_auth_.user_info = authenticated_user;
        pending_mfa_auth_.account_id = mfa_config.account_id;
        pending_mfa_auth_.mfa_policy_id = mfa_config.mfa_policy_id;
        pending_mfa_auth_.allow_recovery_codes = mfa_config.allow_recovery_codes;
        pending_mfa_auth_.allow_break_glass = mfa_config.allow_break_glass;
        pending_mfa_auth_.step_up_ttl_ms = mfa_config.step_up_ttl_ms;
        pending_mfa_auth_.challenge_id = challenge_id;
        pending_mfa_auth_.challenge_method_name = challenge_method_name;
        pending_mfa_auth_.attempts = 0;
        pending_mfa_auth_.max_attempts = std::max<uint32_t>(
            1u, static_cast<uint32_t>(mfa_config.max_challenge_attempts));
        pending_mfa_auth_.challenge_payload = buildMfaChallengePayload(
            challenge_id,
            pending_mfa_auth_.challenge_method_name,
            completed_method_payload);

        protocol::Message response = protocol::ProtocolCodec::buildAuthResponse(
            protocol::AuthStatus::CONTINUE, 0, "", pending_mfa_auth_.challenge_payload);
        return protocol_session_->sendMessage(response, ctx);
    };

    const bool negotiation_request =
        auth_method == protocol::AuthMethod::PASSWORD &&
        auth_payload.empty() &&
        !scram_state_.has_value() &&
        !auth_negotiation_ready_;
    if (negotiation_request) {
        std::string policy_deny_code;
        if (!resolveAuthNegotiationPolicy(catalog,
                                          connection_,
                                          auth_negotiation_allowed_methods_,
                                          auth_negotiation_has_required_method_,
                                          auth_negotiation_required_method_,
                                          auth_negotiation_transport_mask_,
                                          auth_negotiation_peer_mode_,
                                          policy_deny_code)) {
            clear_auth_negotiation();
            return send_auth_policy_error(
                policy_deny_code.empty() ? kAuthPolicyMethodDeniedCode
                                         : policy_deny_code.c_str());
        }
        auth_negotiation_ready_ = true;
        auth_negotiation_username_ = username;
        auth_negotiation_nonce_ = generateNegotiationNonce();
        protocol::Message challenge = protocol::ProtocolCodec::buildAuthChallenge(
            session_id_,
            username,
            auth_negotiation_allowed_methods_,
            auth_negotiation_has_required_method_,
            auth_negotiation_required_method_,
            auth_negotiation_transport_mask_,
            auth_negotiation_nonce_);
        log_auth_policy_decision(true, kAuthPolicyNegotiatedCode);
        return protocol_session_->sendMessage(challenge, ctx);
    }

    if (!auth_negotiation_ready_) {
        return send_auth_policy_error(kAuthPolicyNegotiationRequiredCode);
    }
    if (auth_negotiation_username_ != username) {
        return send_auth_policy_error(kAuthPolicyUserMismatchCode);
    }
    if (!isTransportAllowedByMask(resolveConnectionTransport(connection_),
                                  auth_negotiation_transport_mask_)) {
        return send_auth_policy_error(kAuthPolicyTransportDeniedCode);
    }
    if (auth_negotiation_peer_mode_ != core::CatalogManager::AuthPeerMode::DISABLED) {
        if (!connection_ || connection_->getMethod() != IPCMethod::UNIX_SOCKET) {
            return send_auth_policy_error(kAuthPolicyPeerTransportCode);
        }
        if (!peer_credentials_available_) {
            return send_auth_policy_error(kAuthPolicyPeerRequiredCode);
        }
    }

    const bool method_allowed = std::find(auth_negotiation_allowed_methods_.begin(),
                                          auth_negotiation_allowed_methods_.end(),
                                          auth_method) != auth_negotiation_allowed_methods_.end();
    if (!method_allowed) {
        return send_auth_policy_error(kAuthPolicyMethodDeniedCode);
    }
    if (auth_negotiation_has_required_method_ &&
        auth_method != auth_negotiation_required_method_) {
        return send_auth_policy_error(kAuthPolicyRequiredMethodCode);
    }
    const bool legacy_method =
        auth_method == protocol::AuthMethod::PASSWORD ||
        auth_method == protocol::AuthMethod::MD5;
    if (legacy_method && !isTrustedLocalIpc(connection_)) {
        return send_auth_policy_error(kAuthPolicyTransportDeniedCode);
    }

    if (pending_mfa_auth_.active) {
        if (username != pending_mfa_auth_.username ||
            auth_method != pending_mfa_auth_.auth_method) {
            clear_pending_mfa();
            protocol::Message response = protocol::ProtocolCodec::buildAuthResponse(
                protocol::AuthStatus::FAILURE, 0, kAuthMfaInvalidCode);
            protocol_session_->sendMessage(response, ctx);
            return core::Status::INVALID_AUTHORIZATION;
        }

        std::string challenge_id;
        std::string mfa_code;
        if (!parseMfaResponsePayload(auth_payload, challenge_id, mfa_code) ||
            challenge_id != pending_mfa_auth_.challenge_id) {
            clear_pending_mfa();
            protocol::Message response = protocol::ProtocolCodec::buildAuthResponse(
                protocol::AuthStatus::FAILURE, 0, kAuthMfaInvalidCode);
            protocol_session_->sendMessage(response, ctx);
            return core::Status::INVALID_AUTHORIZATION;
        }

        ResolvedMfaAuthConfig mfa_config;
        mfa_config.mfa_required = true;
        mfa_config.account_id = pending_mfa_auth_.account_id;
        mfa_config.mfa_policy_id = pending_mfa_auth_.mfa_policy_id;
        mfa_config.allow_recovery_codes = pending_mfa_auth_.allow_recovery_codes;
        mfa_config.allow_break_glass = pending_mfa_auth_.allow_break_glass;

        MfaVerificationResult verify_result;
        std::string verify_error;
        if (!verifyCatalogMfaCode(catalog,
                                  mfa_config,
                                  pending_mfa_auth_.username,
                                  mfa_code,
                                  verify_result,
                                  verify_error)) {
            clear_pending_mfa();
            protocol::Message response = protocol::ProtocolCodec::buildAuthResponse(
                protocol::AuthStatus::FAILURE, 0, kAuthMfaInvalidCode);
            protocol_session_->sendMessage(response, ctx);
            return core::Status::INVALID_AUTHORIZATION;
        }

        if (verify_result.success) {
            session_mfa_verified_ = true;
            session_mfa_used_recovery_ = verify_result.recovery_used;
            session_mfa_used_break_glass_ = verify_result.break_glass_used;
            session_mfa_verified_time_ms_ = currentTimeMs();
            session_mfa_step_up_ttl_ms_ = pending_mfa_auth_.step_up_ttl_ms;
            core::AuthUserInfo verified_user = pending_mfa_auth_.user_info;

            if (audit_logger) {
                core::AuditEvent event = core::AuditLogger::createLoginSuccessEvent(
                    verified_user.user_id, pending_mfa_auth_.username);
                if (verify_result.recovery_used) {
                    event.details = verify_result.break_glass_used
                        ? "{\"mfa\":\"break_glass\"}"
                        : "{\"mfa\":\"recovery_code\"}";
                } else {
                    event.details = "{\"mfa\":\"totp\"}";
                }
                core::ErrorContext audit_ctx;
                audit_logger->logEvent(event, &audit_ctx);
            }

            clear_pending_mfa();
            return finish_auth(verified_user, {});
        }

        pending_mfa_auth_.attempts++;
        if (pending_mfa_auth_.attempts >= pending_mfa_auth_.max_attempts) {
            clear_pending_mfa();
            protocol::Message response = protocol::ProtocolCodec::buildAuthResponse(
                protocol::AuthStatus::FAILURE, 0, kAuthMfaInvalidCode);
            protocol_session_->sendMessage(response, ctx);
            return core::Status::INVALID_AUTHORIZATION;
        }

        protocol::Message response = protocol::ProtocolCodec::buildAuthResponse(
            protocol::AuthStatus::CONTINUE, 0, "", pending_mfa_auth_.challenge_payload);
        return protocol_session_->sendMessage(response, ctx);
    }

    if (auth_method == protocol::AuthMethod::PASSWORD) {
        bool bootstrap_phase_a = false;
        std::string bootstrap_phase_error;
        if (!detectBootstrapPhaseA(catalog, bootstrap_phase_a, bootstrap_phase_error)) {
            std::fprintf(stderr,
                         "[auth] failed to resolve bootstrap phase for user %s: %s\n",
                         username.c_str(),
                         bootstrap_phase_error.c_str());
            auth_error = "Authentication failed";
        } else if (bootstrap_phase_a) {
            std::string denial_reason;
            if (!isBootstrapPeerEligible(connection_, denial_reason)) {
                std::fprintf(stderr,
                             "[auth] bootstrap auth denied by peer UID gate for user %s (reason=%s)\n",
                             username.c_str(),
                             denial_reason.c_str());

                if (audit_logger) {
                    core::AuditEvent event;
                    event.event_type = core::AuditEventType::BOOTSTRAP_FAILURE;
                    event.username = username;
                    event.success = false;
                    event.details = std::string("{\"phase\":\"gate\",\"reason\":\"") +
                                    denial_reason + "\"}";
                    core::ErrorContext audit_ctx;
                    audit_logger->logEvent(event, &audit_ctx);
                }

                auth_error = "Authentication failed";
            }
        }

        if (auth_error.empty()) {
            std::string password(reinterpret_cast<const char*>(auth_payload.data()),
                                 auth_payload.size());
            core::AuthResult auth_result = authenticate(username, password, user_info, auth_error);

            if (auth_result == core::AuthResult::SUCCESS) {
                if (auto mfa_status = issue_mfa_challenge_if_required(
                        user_info, auth_method, {}); mfa_status.has_value()) {
                    return *mfa_status;
                }
                return finish_auth(user_info, {});
            }
        }
    } else if (auth_method == protocol::AuthMethod::MD5) {
        if (auth_payload.size() < 4 || !provider) {
            auth_error = "Authentication failed";
        } else {
            uint8_t salt[4];
            std::memcpy(salt, auth_payload.data(), 4);
            std::string response(reinterpret_cast<const char*>(auth_payload.data() + 4),
                                 auth_payload.size() - 4);
            core::AuthResult auth_result = provider->authenticateMd5(
                username, salt, response, user_info, auth_error);
            if (auth_result == core::AuthResult::SUCCESS) {
                if (auto mfa_status = issue_mfa_challenge_if_required(
                        user_info, auth_method, {}); mfa_status.has_value()) {
                    return *mfa_status;
                }
                return finish_auth(user_info, {});
            }
        }
    } else if (auth_method == protocol::AuthMethod::SCRAM_SHA_256 ||
               auth_method == protocol::AuthMethod::SCRAM_SHA_512) {
        if (!provider) {
            auth_error = "Authentication failed";
        } else if (!scram_state_) {
            core::ScramAuthState state;
            std::string server_first;
            std::string client_first(reinterpret_cast<const char*>(auth_payload.data()),
                                     auth_payload.size());
            auto algo = (auth_method == protocol::AuthMethod::SCRAM_SHA_256)
                ? security::ScramAlgorithm::SHA_256
                : security::ScramAlgorithm::SHA_512;
            core::AuthResult auth_result = provider->beginScramAuth(
                username, client_first, algo, state, server_first, auth_error);
            if (auth_result == core::AuthResult::SUCCESS) {
                scram_state_ = std::move(state);
                std::vector<uint8_t> response_data(server_first.begin(), server_first.end());
                protocol::Message response = protocol::ProtocolCodec::buildAuthResponse(
                    protocol::AuthStatus::CONTINUE, 0, "", response_data);
                return protocol_session_->sendMessage(response, ctx);
            }
        } else {
            std::string server_final;
            std::string client_final(reinterpret_cast<const char*>(auth_payload.data()),
                                     auth_payload.size());
            core::AuthResult auth_result = provider->finishScramAuth(
                *scram_state_, client_final, user_info, server_final, auth_error);
            scram_state_.reset();
            if (auth_result == core::AuthResult::SUCCESS) {
                std::vector<uint8_t> response_data(server_final.begin(), server_final.end());
                if (auto mfa_status = issue_mfa_challenge_if_required(
                        user_info, auth_method, response_data); mfa_status.has_value()) {
                    return *mfa_status;
                }
                return finish_auth(user_info, response_data);
            }
        }
    } else if (auth_method == protocol::AuthMethod::TOKEN) {
        if (!provider) {
            auth_error = "Authentication failed";
        } else {
            uint8_t authkey_id_bytes[16]{};
            std::vector<uint8_t> proof;
            std::vector<uint8_t> binding;
            core::Status parse_status = protocol::ProtocolCodec::parseTokenAuthPayload(
                auth_payload, authkey_id_bytes, proof, binding, ctx);
            if (parse_status != core::Status::OK) {
                auth_error = "Authentication failed";
            } else {
                core::ID authkey_id{};
                std::memcpy(authkey_id.bytes.data(), authkey_id_bytes, sizeof(authkey_id_bytes));
                core::AuthResult auth_result = provider->authenticateToken(
                    username, authkey_id, proof, binding, user_info, auth_error);
                if (auth_result == core::AuthResult::SUCCESS) {
                    if (auto mfa_status = issue_mfa_challenge_if_required(
                            user_info, auth_method, {}); mfa_status.has_value()) {
                        return *mfa_status;
                    }
                    return finish_auth(user_info, {});
                }
            }
        }
    } else if (auth_method == protocol::AuthMethod::PEER) {
        if (!provider) {
            auth_error = "Authentication failed";
        } else if (!peer_credentials_available_) {
            return send_auth_policy_error(kAuthPolicyPeerRequiredCode);
        } else {
            core::AuthResult auth_result = provider->authenticatePeer(
                username,
                peer_credentials_.uid,
                peer_credentials_.gid,
                peer_credentials_.pid,
                user_info,
                auth_error);
            if (auth_result == core::AuthResult::SUCCESS) {
                if (auto mfa_status = issue_mfa_challenge_if_required(
                        user_info, auth_method, {}); mfa_status.has_value()) {
                    return *mfa_status;
                }
                return finish_auth(user_info, {});
            }
        }
    } else {
        auth_error = "Authentication failed";
    }

    stats_.queries_failed++;

    protocol::Message response = protocol::ProtocolCodec::buildAuthResponse(
        protocol::AuthStatus::FAILURE, 0, "Authentication failed");
    protocol_session_->sendMessage(response, ctx);
    return core::Status::INVALID_PASSWORD;
}

core::Status ServerSession::handleDisconnect(const protocol::Message& msg, core::ErrorContext* ctx) {
    std::fprintf(stderr,
                 "[ipc_debug] server session %s received DISCONNECT\n",
                 sessionIdString().c_str());
    state_ = SessionState::CLOSING;
    pending_mfa_auth_ = PendingMfaAuthState{};

    // Fire ON DISCONNECT database triggers (Firebird-style) before closing
    fireDatabaseTriggers(core::CatalogManager::DatabaseTriggerEvent::ON_DISCONNECT);

    // Detach to dormant transaction so a privileged client can reattach later.
    if (conn_ctx_) {
        core::ID dormant_id;
        core::Status detach_status = database_->detachToDormant(conn_ctx_, dormant_id, ctx);
        if (detach_status != core::Status::OK) {
            // Fallback to hard rollback if we cannot persist dormant state.
            conn_ctx_->shutdownTransaction(ctx);
            stats_.transactions_rolled_back++;
            conn_ctx_.reset();
        }
    }

    // Close catalog session
    static const core::ID zero_id{};
    if (database_ && database_->catalog_manager() && session_id_uuid_ != zero_id) {
        database_->catalog_manager()->closeSession(session_id_uuid_, ctx);
    }

    state_ = SessionState::CLOSED;
    return core::Status::OK;
}

core::Status ServerSession::handleQuery(const protocol::Message& msg, core::ErrorContext* ctx) {
    if (state_ != SessionState::AUTHENTICATED && state_ != SessionState::IN_TRANSACTION) {
        sendError("Not authenticated", "28000");
        return core::Status::INVALID_AUTHORIZATION;
    }

    // Parse query using ProtocolCodec
    uint8_t session_id[16];
    std::string sql;
    uint8_t flags;
    std::vector<uint8_t> bytecode;

    core::Status status = protocol::ProtocolCodec::parseQuery(
        msg, session_id, sql, flags, &bytecode, ctx);
    if (status != core::Status::OK) {
        sendError("Invalid query");
        return status;
    }

    if (requiresMfaStepUpForSql(sql)) {
        auto* catalog = database_ ? database_->catalog_manager() : nullptr;
        ResolvedMfaAuthConfig mfa_config;
        std::string mfa_error;
        if (!resolveMfaConfig(catalog, connection_, username_, mfa_config, mfa_error)) {
            sendError("MFA policy resolution failed", "28000", ctx);
            return core::Status::INVALID_AUTHORIZATION;
        }

        if (mfa_config.mfa_required) {
            const uint64_t now_ms = currentTimeMs();
            bool step_up_valid = session_mfa_verified_;
            if (step_up_valid && session_mfa_step_up_ttl_ms_ > 0) {
                if (session_mfa_verified_time_ms_ == 0 ||
                    now_ms < session_mfa_verified_time_ms_ ||
                    (now_ms - session_mfa_verified_time_ms_) > session_mfa_step_up_ttl_ms_) {
                    step_up_valid = false;
                }
            }

            if (!step_up_valid) {
                session_mfa_verified_ = false;
                session_mfa_used_recovery_ = false;
                session_mfa_used_break_glass_ = false;
                session_mfa_verified_time_ms_ = 0;
                session_mfa_step_up_ttl_ms_ = 0;

                auto* audit_logger = database_ ? database_->audit_logger() : nullptr;
                if (audit_logger && conn_ctx_) {
                    core::AuditEvent event = core::AuditLogger::createPermissionDeniedEvent(
                        conn_ctx_->getCurrentUserId(),
                        username_,
                        "AUTH_MFA_STEP_UP",
                        "PRIVILEGED_QUERY",
                        "STEP_UP");
                    event.details = "{\"reason\":\"mfa_step_up_required\"}";
                    core::ErrorContext audit_ctx;
                    audit_logger->logEvent(event, &audit_ctx);
                }

                sendError("MFA step-up required for privileged command", "28000", ctx);
                return core::Status::INVALID_AUTHORIZATION;
            }
        }
    }

    // Execute the query
    SessionState prev_state = state_;
    state_ = SessionState::EXECUTING;
    if (flags & static_cast<uint8_t>(protocol::QueryFlags::BYTECODE)) {
        status = executeBytecode(bytecode, sql, ctx);
    } else {
        status = sendError("SQL text execution is not supported; parser must submit SBLR bytecode",
                           "0A000",
                           ctx);
    }

    // Restore state
    state_ = prev_state;

    return status;
}

core::Status ServerSession::handleTransaction(const protocol::Message& msg, core::ErrorContext* ctx) {
    if (state_ != SessionState::AUTHENTICATED && state_ != SessionState::IN_TRANSACTION) {
        sendError("Not authenticated", "28000");
        return core::Status::INVALID_AUTHORIZATION;
    }

    protocol::MessageType type = msg.getType();
    core::Status status = core::Status::OK;
    std::string result_msg;

    switch (type) {
        case protocol::MessageType::BEGIN_TRANSACTION:
            // In ScratchBird's Firebird MGA model, transactions are always active
            // BEGIN just marks the session as explicitly in transaction mode
            if (conn_ctx_) {
                // Start a new transaction (will commit any existing)
                status = conn_ctx_->startTransaction(false, core::IsolationLevel::SNAPSHOT, true, ctx);
                if (status == core::Status::OK) {
                    state_ = SessionState::IN_TRANSACTION;
                    // Fire ON TRANSACTION START database triggers (Firebird-style)
                    fireDatabaseTriggers(core::CatalogManager::DatabaseTriggerEvent::ON_TRANSACTION_START);
                    result_msg = "BEGIN";
                }
            }
            break;

        case protocol::MessageType::COMMIT:
            if (conn_ctx_) {
                // Fire ON TRANSACTION COMMIT triggers BEFORE commit (Firebird-style)
                fireDatabaseTriggers(core::CatalogManager::DatabaseTriggerEvent::ON_TRANSACTION_COMMIT);
                status = conn_ctx_->commit(ctx);
                if (status == core::Status::OK) {
                    state_ = SessionState::AUTHENTICATED;
                    stats_.transactions_committed++;
                    result_msg = "COMMIT";
                }
            }
            break;

        case protocol::MessageType::ROLLBACK:
            if (conn_ctx_) {
                // Fire ON TRANSACTION ROLLBACK triggers BEFORE rollback (Firebird-style)
                fireDatabaseTriggers(core::CatalogManager::DatabaseTriggerEvent::ON_TRANSACTION_ROLLBACK);
                status = conn_ctx_->rollback(ctx);
                state_ = SessionState::AUTHENTICATED;
                stats_.transactions_rolled_back++;
                result_msg = "ROLLBACK";
            }
            break;

        case protocol::MessageType::SAVEPOINT:
            // NET-2: Handle SAVEPOINT
            if (conn_ctx_) {
                // Parse savepoint name from message payload
                if (msg.getPayloadSize() >= sizeof(protocol::SavepointPayload)) {
                    const auto* sp_payload = reinterpret_cast<const protocol::SavepointPayload*>(msg.getPayload());
                    // Extract savepoint name (null-terminated, max 63 chars)
                    std::string sp_name(sp_payload->savepoint_name,
                                       strnlen(sp_payload->savepoint_name, sizeof(sp_payload->savepoint_name)));
                    status = conn_ctx_->createSavepoint(sp_name, ctx);
                    if (status == core::Status::OK) {
                        result_msg = "SAVEPOINT";
                    }
                } else {
                    sendError("Invalid savepoint message");
                    return core::Status::INVALID_ARGUMENT;
                }
            }
            break;

        case protocol::MessageType::RELEASE_SAVEPOINT:
            // NET-2: Handle RELEASE SAVEPOINT
            if (conn_ctx_) {
                if (msg.getPayloadSize() >= sizeof(protocol::SavepointPayload)) {
                    const auto* sp_payload = reinterpret_cast<const protocol::SavepointPayload*>(msg.getPayload());
                    std::string sp_name(sp_payload->savepoint_name,
                                       strnlen(sp_payload->savepoint_name, sizeof(sp_payload->savepoint_name)));
                    status = conn_ctx_->releaseSavepoint(sp_name, ctx);
                    if (status == core::Status::OK) {
                        result_msg = "RELEASE";
                    }
                } else {
                    sendError("Invalid release savepoint message");
                    return core::Status::INVALID_ARGUMENT;
                }
            }
            break;

        case protocol::MessageType::ROLLBACK_TO:
            // NET-2: Handle ROLLBACK TO SAVEPOINT
            if (conn_ctx_) {
                if (msg.getPayloadSize() >= sizeof(protocol::SavepointPayload)) {
                    const auto* sp_payload = reinterpret_cast<const protocol::SavepointPayload*>(msg.getPayload());
                    std::string sp_name(sp_payload->savepoint_name,
                                       strnlen(sp_payload->savepoint_name, sizeof(sp_payload->savepoint_name)));
                    status = conn_ctx_->rollbackToSavepoint(sp_name, ctx);
                    if (status == core::Status::OK) {
                        result_msg = "ROLLBACK";
                    }
                } else {
                    sendError("Invalid rollback to savepoint message");
                    return core::Status::INVALID_ARGUMENT;
                }
            }
            break;

        default:
            sendError("Unknown transaction type");
            return core::Status::INVALID_ARGUMENT;
    }

    if (status != core::Status::OK) {
        sendError("Transaction operation failed");
        return status;
    }

    // Send COMMAND_COMPLETE
    protocol::Message response = protocol::ProtocolCodec::buildCommandComplete(result_msg, 0);
    return protocol_session_->sendMessage(response, ctx);
}

core::Status ServerSession::handlePing(const protocol::Message& msg, core::ErrorContext* ctx) {
    // Parse ping to get timestamp and sequence
    uint64_t timestamp = 0;
    uint32_t sequence = 0;
    protocol::ProtocolCodec::parsePing(msg, timestamp, sequence, ctx);

    // Send PONG response with same timestamp/sequence
    protocol::Message response = protocol::ProtocolCodec::buildPong(timestamp, sequence);
    return protocol_session_->sendMessage(response, ctx);
}

core::Status ServerSession::handleCancel(const protocol::Message& msg, core::ErrorContext* ctx) {
    // NET-M1: Query cancellation support
    if (query_executing_.load(std::memory_order_acquire)) {
        // A query is currently executing - request cancellation
        if (executor_) {
            executor_->requestCancellation();
            // Send acknowledgement - the actual cancellation will happen asynchronously
            // The executing query will detect the cancellation flag and abort
            protocol::Message response = protocol::ProtocolCodec::buildCommandComplete("CANCEL", 0);
            return protocol_session_->sendMessage(response, ctx);
        }
    }

    // No query executing, nothing to cancel
    protocol::Message response = protocol::ProtocolCodec::buildCommandComplete("CANCEL", 0);
    return protocol_session_->sendMessage(response, ctx);
}

core::Status ServerSession::executeBytecode(const std::vector<uint8_t>& bytecode,
                                            const std::string& sql,
                                            core::ErrorContext* ctx) {
    stats_.queries_executed++;

    if (executor_) {
        executor_->resetCancellation();
    }
    query_executing_.store(true, std::memory_order_release);

    if (conn_ctx_) {
        conn_ctx_->beginStatementTracking(sql.empty() ? "SBLR" : sql);
    }

    struct QueryExecutingGuard {
        std::atomic<bool>& flag;
        explicit QueryExecutingGuard(std::atomic<bool>& f) : flag(f) {}
        ~QueryExecutingGuard() { flag.store(false, std::memory_order_release); }
    } guard(query_executing_);

    struct ConnectionContextGuard {
        core::ConnectionContext* previous = nullptr;
        bool changed = false;

        explicit ConnectionContextGuard(core::ConnectionContext* current)
            : previous(core::ConnectionContext::getCurrent())
        {
            if (current && current != previous) {
                core::ConnectionContext::setCurrent(current);
                changed = true;
            }
        }

        ~ConnectionContextGuard() {
            if (changed) {
                core::ConnectionContext::setCurrent(previous);
            }
        }
    } ctx_guard(conn_ctx_.get());

    if (executor_) {
        core::ErrorContext validate_ctx;
        core::Status validate_status = sblr::validateBytecode(bytecode, &validate_ctx);
        if (validate_status != core::Status::OK) {
            stats_.queries_failed++;
            if (conn_ctx_) {
                conn_ctx_->endStatementTrackingFailure(
                    static_cast<uint32_t>(validate_status), "0A000");
            }
            std::string err = validate_ctx.message.empty()
                ? "Invalid bytecode"
                : validate_ctx.message;
            return sendError(err, "0A000", ctx);
        }
    }

    bool copy_from_stdin = false;
    bool copy_to_stdout = false;
    bool copy_active = !sql.empty() && parseCopyQuery(sql, copy_from_stdin, copy_to_stdout);
    CopyStreamState copy_state;
    std::unique_ptr<CopyInStreambuf> copy_in_buf;
    std::unique_ptr<CopyOutStreambuf> copy_out_buf;
    std::unique_ptr<std::istream> copy_in_stream;
    std::unique_ptr<std::ostream> copy_out_stream;

    if (copy_active) {
        copy_state.stream_id = next_stream_id_++;
        if (copy_from_stdin) {
            auto status = sendCopyInPreamble(protocol_session_.get(), copy_state, ctx);
            if (status != core::Status::OK) {
                return status;
            }
            auto read_fn = [this, &copy_state](std::string& out, bool& done, std::string& error) {
                return readCopyInChunk(protocol_session_.get(), copy_state, out, done, error);
            };
            copy_in_buf = std::make_unique<CopyInStreambuf>(std::move(read_fn));
            copy_in_stream = std::make_unique<std::istream>(copy_in_buf.get());
            executor_->setCopyInputStream(copy_in_stream.get());
        } else if (copy_to_stdout) {
            auto status = sendCopyOutPreamble(protocol_session_.get(), copy_state, ctx);
            if (status != core::Status::OK) {
                return status;
            }
            auto write_fn = [this, &copy_state](const uint8_t* data, size_t len, std::string& error) {
                return sendCopyOutChunk(protocol_session_.get(), copy_state, data, len, error);
            };
            copy_out_buf = std::make_unique<CopyOutStreambuf>(65536, std::move(write_fn));
            copy_out_stream = std::make_unique<std::ostream>(copy_out_buf.get());
            executor_->setCopyOutputStream(copy_out_stream.get());
        }
    }

    struct CopyStreamGuard {
        sblr::Executor* executor = nullptr;
        explicit CopyStreamGuard(sblr::Executor* exec) : executor(exec) {}
        ~CopyStreamGuard() {
            if (executor) {
                executor->setCopyInputStream(nullptr);
                executor->setCopyOutputStream(nullptr);
            }
        }
    } copy_guard(copy_active ? executor_.get() : nullptr);

    sblr::ExecutionResult exec_result;
    try {
        exec_result = executor_->execute(bytecode);
    } catch (const std::exception& ex) {
        stats_.queries_failed++;
        if (copy_active) {
            protocol_session_->sendMessage(
                protocol::ProtocolCodec::buildCopyFail(ex.what()), nullptr);
        }
        if (conn_ctx_) {
            conn_ctx_->endStatementTrackingFailure(
                static_cast<uint32_t>(core::Status::INTERNAL_ERROR), "42000");
        }
        return sendError(ex.what(), "42000", ctx);
    }

    if (!exec_result.success()) {
        stats_.queries_failed++;
        if (copy_active) {
            protocol_session_->sendMessage(
                protocol::ProtocolCodec::buildCopyFail(exec_result.error()), nullptr);
        }
        if (conn_ctx_) {
            conn_ctx_->endStatementTrackingFailure(
                static_cast<uint32_t>(core::Status::INTERNAL_ERROR), "42000");
        }
        return sendError(exec_result.error(), "42000", ctx);
    }

    if (copy_active) {
        if (copy_out_stream) {
            try {
                copy_out_stream->flush();
            } catch (const std::exception& ex) {
                protocol_session_->sendMessage(
                    protocol::ProtocolCodec::buildCopyFail(ex.what()), nullptr);
                if (conn_ctx_) {
                    conn_ctx_->endStatementTrackingFailure(
                        static_cast<uint32_t>(core::Status::IO_ERROR), "42000");
                }
                return sendError(ex.what(), "42000", ctx);
            }
        }

        int64_t rows = exec_result.affectedCount();
        if (copy_to_stdout) {
            auto status = protocol_session_->sendMessage(
                protocol::ProtocolCodec::buildCopyDone(), ctx);
            if (status != core::Status::OK) {
                return status;
            }
        }

        auto status = protocol_session_->sendMessage(
            protocol::ProtocolCodec::buildStreamEnd(copy_state.stream_id,
                                                    static_cast<uint64_t>(rows),
                                                    copy_state.total_bytes),
            ctx);
        if (status != core::Status::OK) {
            return status;
        }

        std::string tag = "COPY " + std::to_string(rows);
        protocol::Message response = protocol::ProtocolCodec::buildCommandComplete(tag, rows);
        status = protocol_session_->sendMessage(response, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        protocol::Message end_msg = protocol::ProtocolCodec::buildEndOfResults();
        status = protocol_session_->sendMessage(end_msg, ctx);
        if (conn_ctx_) {
            conn_ctx_->endStatementTrackingSuccess(rows);
        }
        return status;
    }

    if (exec_result.hasResultSet()) {
        return sendResultSet(exec_result.resultSet(), ctx);
    }

    std::string command = "OK";
    if (!sql.empty()) {
        std::string sql_upper = sql;
        for (auto& c : sql_upper) c = static_cast<char>(std::toupper(c));

        if (sql_upper.find("INSERT") == 0) command = "INSERT";
        else if (sql_upper.find("UPDATE") == 0) command = "UPDATE";
        else if (sql_upper.find("DELETE") == 0) command = "DELETE";
        else if (sql_upper.find("CREATE") == 0) command = "CREATE";
        else if (sql_upper.find("DROP") == 0) command = "DROP";
        else if (sql_upper.find("ALTER") == 0) command = "ALTER";
    }

    protocol::Message response = protocol::ProtocolCodec::buildCommandComplete(
        command, exec_result.affectedCount());
    if (conn_ctx_) {
        conn_ctx_->endStatementTrackingSuccess(exec_result.affectedCount());
    }
    core::Status send_status = protocol_session_->sendMessage(response, ctx);
    if (send_status != core::Status::OK) {
        return send_status;
    }
    protocol::Message end_msg = protocol::ProtocolCodec::buildEndOfResults();
    return protocol_session_->sendMessage(end_msg, ctx);
}

// Helper function to convert DataType to WireType
static protocol::WireType dataTypeToWireType(core::DataType type) {
    switch (type) {
        case core::DataType::BOOLEAN:   return protocol::WireType::BOOLEAN;
        case core::DataType::INT16:     return protocol::WireType::INT16;
        case core::DataType::INT32:     return protocol::WireType::INT32;
        case core::DataType::INT64:     return protocol::WireType::INT64;
        case core::DataType::FLOAT32:   return protocol::WireType::FLOAT32;
        case core::DataType::FLOAT64:   return protocol::WireType::FLOAT64;
        case core::DataType::DECIMAL:   return protocol::WireType::DECIMAL;
        case core::DataType::DECFLOAT16: return protocol::WireType::DECIMAL;
        case core::DataType::DECFLOAT34: return protocol::WireType::DECIMAL;
        case core::DataType::CHAR:      return protocol::WireType::CHAR;
        case core::DataType::VARCHAR:   return protocol::WireType::VARCHAR;
        case core::DataType::TEXT:      return protocol::WireType::VARCHAR;
        case core::DataType::BYTEA:     return protocol::WireType::BYTEA;
        case core::DataType::DATE:      return protocol::WireType::DATE;
        case core::DataType::TIME:      return protocol::WireType::TIME;
        case core::DataType::TIMESTAMP: return protocol::WireType::TIMESTAMP;
        case core::DataType::INTERVAL:  return protocol::WireType::INTERVAL;
        case core::DataType::UUID:      return protocol::WireType::UUID;
        case core::DataType::JSON:      return protocol::WireType::JSON;
        case core::DataType::JSONB:     return protocol::WireType::JSONB;
        case core::DataType::ARRAY:     return protocol::WireType::ARRAY;
        case core::DataType::MONEY:     return protocol::WireType::MONEY;
        case core::DataType::XML:       return protocol::WireType::XML;
        case core::DataType::INET:      return protocol::WireType::INET;
        case core::DataType::CIDR:      return protocol::WireType::CIDR;
        case core::DataType::MACADDR:   return protocol::WireType::MACADDR;
        case core::DataType::TSVECTOR:  return protocol::WireType::TSVECTOR;
        case core::DataType::TSQUERY:   return protocol::WireType::TSQUERY;
        case core::DataType::NULL_TYPE: return protocol::WireType::NULL_TYPE;
        default:                        return protocol::WireType::UNKNOWN;
    }
}

core::Status ServerSession::sendResultSet(const sblr::ResultSet* results, core::ErrorContext* ctx) {
    if (!results) {
        return sendError("Internal error: null result set", "XX000", ctx);
    }

    // Build column descriptions using ColumnInfo
    std::vector<protocol::ProtocolCodec::ColumnInfo> columns;
    for (size_t i = 0; i < results->columnCount(); i++) {
        protocol::ProtocolCodec::ColumnInfo col;
        col.name = results->columnName(i);
        col.type = dataTypeToWireType(results->columnType(i));
        col.type_modifier = 0;
        columns.push_back(col);
    }

    // Send ROW_DESCRIPTION
    protocol::Message desc_msg = protocol::ProtocolCodec::buildRowDescription(columns);
    core::Status status = protocol_session_->sendMessage(desc_msg, ctx);
    if (status != core::Status::OK) return status;

    // Send ROW_DATA for each row
    for (size_t row = 0; row < results->rowCount(); row++) {
        std::vector<protocol::ProtocolCodec::ColumnValue> values;
        for (size_t col = 0; col < results->columnCount(); col++) {
            const sblr::Value& val = results->getValue(row, col);
            if (val.isNull()) {
                values.push_back(protocol::ProtocolCodec::ColumnValue(nullptr));
            } else {
                // Convert value to binary format based on type
                switch (val.type()) {
                    case core::DataType::INT32:
                        values.push_back(protocol::ProtocolCodec::ColumnValue::fromInt32(val.getInt32()));
                        break;
                    case core::DataType::INT64:
                        values.push_back(protocol::ProtocolCodec::ColumnValue::fromInt64(val.getInt64()));
                        break;
                    case core::DataType::FLOAT32:
                        values.push_back(protocol::ProtocolCodec::ColumnValue::fromDouble(static_cast<double>(val.getFloat32())));
                        break;
                    case core::DataType::FLOAT64:
                        values.push_back(protocol::ProtocolCodec::ColumnValue::fromDouble(val.getFloat64()));
                        break;
                    case core::DataType::BOOLEAN:
                        values.push_back(protocol::ProtocolCodec::ColumnValue::fromBool(val.getBool()));
                        break;
                    case core::DataType::VARCHAR:
                    case core::DataType::TEXT:
                    case core::DataType::CHAR:
                    default:
                        // Fall back to string representation for text and other types
                        values.push_back(protocol::ProtocolCodec::ColumnValue::fromString(val.toString()));
                        break;
                }
            }
        }

        protocol::Message row_msg = protocol::ProtocolCodec::buildRowData(values);
        status = protocol_session_->sendMessage(row_msg, ctx);
        if (status != core::Status::OK) return status;

        stats_.rows_returned++;
    }

    if (conn_ctx_) {
        conn_ctx_->endStatementTrackingSuccess(static_cast<int64_t>(results->rowCount()));
    }

    // Send COMMAND_COMPLETE
    std::string command = "SELECT " + std::to_string(results->rowCount());
    protocol::Message complete_msg = protocol::ProtocolCodec::buildCommandComplete(
        command, static_cast<int64_t>(results->rowCount()));
    status = protocol_session_->sendMessage(complete_msg, ctx);
    if (status != core::Status::OK) return status;

    // Send END_OF_RESULTS
    protocol::Message end_msg = protocol::ProtocolCodec::buildEndOfResults();
    return protocol_session_->sendMessage(end_msg, ctx);
}

core::Status ServerSession::sendError(const std::string& message,
                                       const std::string& sqlstate,
                                       core::ErrorContext* ctx) {
    // buildQueryError(error_code, sqlstate, message, detail, hint)
    protocol::Message error_msg = protocol::ProtocolCodec::buildQueryError(
        static_cast<uint32_t>(core::Status::INTERNAL_ERROR), sqlstate, message, "", "");
    return protocol_session_->sendMessage(error_msg, ctx);
}

core::AuthResult ServerSession::authenticate(const std::string& username,
                                             const std::string& password,
                                             core::AuthUserInfo& user_info,
                                             std::string& error_msg_out) {
    // Get catalog manager from database
    auto* catalog = database_ ? database_->catalog_manager() : nullptr;
    if (!catalog) {
        error_msg_out = "Authentication catalog unavailable";
        return core::AuthResult::PROVIDER_ERROR;
    }

    auto* audit_logger = database_ ? database_->audit_logger() : nullptr;
    auto provider = core::AuthProviderFactory::createDefault(catalog, audit_logger);
    if (!provider) {
        error_msg_out = "Authentication provider unavailable";
        return core::AuthResult::PROVIDER_ERROR;
    }
    if (auto* local_provider = dynamic_cast<core::LocalAuthProvider*>(provider.get())) {
        local_provider->setPeerIdentityContext(peer_credentials_available_,
                                               peer_credentials_.uid,
                                               peer_credentials_.gid,
                                               peer_credentials_.pid);
    }

    return provider->authenticate(username, password, user_info, error_msg_out);
}

bool ServerSession::fireDatabaseTriggers(core::CatalogManager::DatabaseTriggerEvent event) {
    // Get catalog manager
    auto* catalog = database_->catalog_manager();
    if (!catalog) {
        // No catalog - no triggers to fire
        return true;
    }

    // Get all triggers for this event (already sorted by position)
    std::vector<core::CatalogManager::DatabaseTriggerInfo> triggers;
    core::ErrorContext ctx;
    core::Status status = catalog->listDatabaseTriggers(event, triggers, &ctx);

    if (status != core::Status::OK) {
        // Failed to list triggers - log warning but don't fail the operation
        std::cerr << "Warning: Failed to list database triggers for event\n";
        return true;
    }

    // Execute each active trigger in order
    for (const auto& trigger : triggers) {
        if (!trigger.active) {
            continue;  // Skip inactive triggers
        }

        // Execute the trigger procedure using the Executor
        // Create a temporary executor for trigger execution
        sblr::Executor trigger_executor(database_);

        // Set connection context if available (for security and transaction state)
        if (conn_ctx_) {
            trigger_executor.setConnectionContext(conn_ctx_.get());
        }

        // Call the procedure by name (database trigger procedures have no arguments)
        auto result = trigger_executor.callProcedureByName(trigger.procedure_name);

        if (!result.success()) {
            // Trigger execution failed
            std::cerr << "Database trigger '" << trigger.trigger_name
                      << "' failed: " << result.error() << "\n";
            return false;  // Abort the operation
        }

        // Log successful trigger fire (can be removed in production)
        std::cerr << "Database trigger '" << trigger.trigger_name
                  << "' executed procedure: " << trigger.procedure_name << "()\n";
    }

    return true;  // All triggers executed successfully
}

// ============================================================================
// SessionManager Implementation
// ============================================================================

SessionManager::SessionManager() = default;

SessionManager::~SessionManager() {
    shutdownAll();
}

ServerSession* SessionManager::createSession(IPCConnection* connection, core::Database* database) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Generate session ID
    uint8_t session_id[16];
    protocol::generateSessionId(session_id);

    // Create session
    auto session = std::make_unique<ServerSession>(connection, database, session_id);
    ServerSession* ptr = session.get();

    // Store in map
    std::string id_str = protocol::sessionIdToString(session_id);
    sessions_[id_str] = std::move(session);

    return ptr;
}

void SessionManager::removeSession(const uint8_t session_id[16]) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id_str = protocol::sessionIdToString(session_id);
    sessions_.erase(id_str);
}

ServerSession* SessionManager::getSession(const uint8_t session_id[16]) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id_str = protocol::sessionIdToString(session_id);
    auto it = sessions_.find(id_str);
    if (it != sessions_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<ServerSession*> SessionManager::getAllSessions() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ServerSession*> result;
    result.reserve(sessions_.size());
    for (auto& pair : sessions_) {
        result.push_back(pair.second.get());
    }
    return result;
}

size_t SessionManager::sessionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

void SessionManager::shutdownAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : sessions_) {
        pair.second->requestShutdown();
    }
}

bool SessionManager::waitForShutdown(uint32_t timeout_ms) {
    auto start = std::chrono::steady_clock::now();

    while (true) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            bool all_closed = true;
            for (auto& pair : sessions_) {
                if (pair.second->isRunning()) {
                    all_closed = false;
                    break;
                }
            }
            if (all_closed) return true;
        }

        // Check timeout
        if (timeout_ms > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeout_ms) {
                return false;
            }
        }

        // Sleep briefly
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

SessionStats SessionManager::getAggregateStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    SessionStats aggregate;
    aggregate.created_at = std::chrono::steady_clock::now();
    aggregate.last_activity = std::chrono::steady_clock::time_point::min();

    for (const auto& pair : sessions_) {
        const auto& s = pair.second->stats();
        aggregate.queries_executed += s.queries_executed;
        aggregate.queries_failed += s.queries_failed;
        aggregate.rows_returned += s.rows_returned;
        aggregate.bytes_sent += s.bytes_sent;
        aggregate.bytes_received += s.bytes_received;
        aggregate.transactions_committed += s.transactions_committed;
        aggregate.transactions_rolled_back += s.transactions_rolled_back;

        if (s.created_at < aggregate.created_at) {
            aggregate.created_at = s.created_at;
        }
        if (s.last_activity > aggregate.last_activity) {
            aggregate.last_activity = s.last_activity;
        }
    }

    return aggregate;
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* sessionStateToString(SessionState state) {
    switch (state) {
        case SessionState::CREATED:        return "CREATED";
        case SessionState::AUTHENTICATING: return "AUTHENTICATING";
        case SessionState::AUTHENTICATED:  return "AUTHENTICATED";
        case SessionState::EXECUTING:      return "EXECUTING";
        case SessionState::IN_TRANSACTION: return "IN_TRANSACTION";
        case SessionState::CLOSING:        return "CLOSING";
        case SessionState::CLOSED:         return "CLOSED";
        default:                           return "UNKNOWN";
    }
}

}  // namespace server
}  // namespace scratchbird
