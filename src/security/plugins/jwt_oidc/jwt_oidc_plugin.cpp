/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "../auth_plugin_observability.h"
#include "../auth_plugin_crypto.h"
#include "../auth_plugin_secret_provider.h"

#include "scratchbird/security/auth_plugin_abi_v1.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace {

constexpr char kPluginId[] = "scratchbird.auth.jwt_oidc";
constexpr char kPluginVersion[] = "1.0.0";
constexpr char kMethodJwtBearer[] = "scratchbird.auth.jwt_bearer";
constexpr char kMethodOidcIdToken[] = "scratchbird.auth.oidc_id_token";
constexpr char kIssuerJwt[] = "jwt";
constexpr char kIssuerOidc[] = "oidc";
constexpr char kDefaultAudience[] = "scratchbird";
constexpr char kDefaultRequiredAlg[] = "HS256";

constexpr char kJwtSigningKeyPolicyRef[] = "policy:auth.jwt_oidc.jwt_signing_key_ref";
constexpr char kOidcSigningKeyPolicyRef[] = "policy:auth.jwt_oidc.oidc_signing_key_ref";
constexpr char kPolicyJwtExpectedIssuer[] = "auth.jwt_oidc.jwt_expected_issuer";
constexpr char kPolicyJwtExpectedAudience[] = "auth.jwt_oidc.jwt_expected_audience";
constexpr char kPolicyJwtRequiredAlg[] = "auth.jwt_oidc.jwt_required_alg";
constexpr char kPolicyOidcExpectedIssuer[] = "auth.jwt_oidc.oidc_expected_issuer";
constexpr char kPolicyOidcExpectedAudience[] = "auth.jwt_oidc.oidc_expected_audience";
constexpr char kPolicyOidcRequiredAlg[] = "auth.jwt_oidc.oidc_required_alg";

constexpr uint32_t kLegacyWireUnknown = 0xFFFFFFFFu;
constexpr uint32_t kMaxTokenBytes = 8192;
constexpr uint32_t kMaxDecodedHeaderBytes = 2048;
constexpr uint32_t kMaxDecodedPayloadBytes = 6144;
constexpr uint32_t kExpectedSignatureHexBytes = 64;
constexpr uint32_t kMaxAlgBytes = 32;
constexpr uint32_t kMaxIssuerBytes = 512;
constexpr uint32_t kMaxAudienceBytes = 512;
constexpr uint32_t kMaxSubjectBytes = 512;

constexpr char kErrBadRequest[] = "AUTH_JWT_BAD_REQUEST";
constexpr char kErrUnknownMethod[] = "AUTH_JWT_METHOD_UNKNOWN";
constexpr char kErrMissingToken[] = "AUTH_JWT_TOKEN_MISSING";
constexpr char kErrOversizedToken[] = "AUTH_JWT_TOKEN_OVERSIZED";
constexpr char kErrMalformedToken[] = "AUTH_JWT_TOKEN_MALFORMED";
constexpr char kErrSigningKeyUnavailable[] = "AUTH_JWT_SIGNING_KEY_UNAVAILABLE";
constexpr char kErrResolverUnavailable[] = "AUTH_JWT_RESOLVER_UNAVAILABLE";
constexpr char kErrSubjectUnknown[] = "AUTH_JWT_SUBJECT_UNKNOWN";
constexpr char kErrNoContinuation[] = "AUTH_JWT_NO_CONTINUE";
constexpr char kErrAlgMismatch[] = "AUTH_JWT_ALG_MISMATCH";
constexpr char kErrIssuerMismatch[] = "AUTH_JWT_ISSUER_MISMATCH";
constexpr char kErrAudienceMismatch[] = "AUTH_JWT_AUDIENCE_MISMATCH";
constexpr char kErrExpired[] = "AUTH_JWT_EXPIRED";
constexpr char kErrSignatureInvalid[] = "AUTH_JWT_SIGNATURE_INVALID";

const sb_auth_host_api_v1* g_host_api = nullptr;
std::atomic<uint64_t> g_next_instance{1};
scratchbird::security::plugins::observability::OutcomeCounters g_counters{};
thread_local std::string g_health_payload;
thread_local std::string g_external_subject_payload;

struct ParsedJwtToken {
    std::string header_segment;
    std::string payload_segment;
    std::string signature_segment;
    std::string algorithm;
    std::string issuer;
    std::string audience;
    std::string subject;
    uint64_t exp_unix_ms = 0;
};

struct MethodPolicy {
    const char* method_id;
    const char* issuer_literal;
    const char* signing_key_policy_ref;
    const char* expected_issuer_policy_key;
    const char* expected_audience_policy_key;
    const char* required_alg_policy_key;
    uint32_t assurance_level;
};

inline void copyCStr(char* out, std::size_t out_size, const char* value) {
    if (!out || out_size == 0) {
        return;
    }
    if (!value) {
        out[0] = '\0';
        return;
    }
    std::strncpy(out, value, out_size - 1);
    out[out_size - 1] = '\0';
}

inline bool sliceEqualsLiteral(sb_auth_slice_t value, const char* literal) {
    if (!literal || !value.ptr) {
        return false;
    }
    const std::size_t literal_len = std::strlen(literal);
    if (value.len != literal_len) {
        return false;
    }
    return std::memcmp(value.ptr, literal, literal_len) == 0;
}

inline bool isBase64UrlByte(uint8_t c) {
    return (c >= static_cast<uint8_t>('a') && c <= static_cast<uint8_t>('z')) ||
           (c >= static_cast<uint8_t>('A') && c <= static_cast<uint8_t>('Z')) ||
           (c >= static_cast<uint8_t>('0') && c <= static_cast<uint8_t>('9')) ||
           c == static_cast<uint8_t>('-') || c == static_cast<uint8_t>('_');
}

inline bool tokenLooksJwtLike(sb_auth_slice_t token) {
    if (!token.ptr || token.len == 0 || token.len > kMaxTokenBytes) {
        return false;
    }

    uint32_t dot_count = 0;
    uint32_t segment_len = 0;
    for (uint32_t i = 0; i < token.len; ++i) {
        const uint8_t c = token.ptr[i];
        if (c == static_cast<uint8_t>('.')) {
            if (segment_len == 0) {
                return false;
            }
            ++dot_count;
            segment_len = 0;
            continue;
        }

        if (!isBase64UrlByte(c)) {
            return false;
        }
        ++segment_len;
    }

    return dot_count == 2 && segment_len > 0;
}

inline std::string toString(sb_auth_slice_t value) {
    if (!value.ptr || value.len == 0) {
        return "";
    }
    const char* begin = reinterpret_cast<const char*>(value.ptr);
    return std::string(begin, begin + value.len);
}

inline uint64_t nowUnixMs() {
    if (g_host_api && g_host_api->now_unix_ms) {
        return g_host_api->now_unix_ms();
    }
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

inline bool parseUInt64(std::string_view text, uint64_t* out_value) {
    if (!out_value || text.empty()) {
        return false;
    }

    uint64_t acc = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        const uint64_t digit = static_cast<uint64_t>(ch - '0');
        const uint64_t next = acc * 10u + digit;
        if (next < acc) {
            return false;
        }
        acc = next;
    }
    *out_value = acc;
    return true;
}

inline int decodeBase64UrlValue(char ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return ch - 'A';
    }
    if (ch >= 'a' && ch <= 'z') {
        return (ch - 'a') + 26;
    }
    if (ch >= '0' && ch <= '9') {
        return (ch - '0') + 52;
    }
    if (ch == '-') {
        return 62;
    }
    if (ch == '_') {
        return 63;
    }
    return -1;
}

inline bool isLowerHex(char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
}

inline bool isTokenFieldChar(char ch) {
    if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
        (ch >= '0' && ch <= '9')) {
        return true;
    }
    return ch == '_' || ch == '-' || ch == '.';
}

inline bool isBoundedTokenField(std::string_view value, uint32_t max_len) {
    if (value.empty() || value.size() > max_len) {
        return false;
    }
    for (char ch : value) {
        if (!isTokenFieldChar(ch)) {
            return false;
        }
    }
    return true;
}

inline bool decodeBase64Url(std::string_view encoded,
                            std::string* out_decoded,
                            uint32_t max_decoded_bytes) {
    if (!out_decoded || encoded.empty()) {
        return false;
    }

    out_decoded->clear();
    uint64_t buffer = 0;
    uint32_t bits = 0;
    for (char ch : encoded) {
        const int value = decodeBase64UrlValue(ch);
        if (value < 0) {
            return false;
        }
        buffer = (buffer << 6u) | static_cast<uint64_t>(value);
        bits += 6;

        while (bits >= 8u) {
            bits -= 8u;
            const uint8_t byte = static_cast<uint8_t>((buffer >> bits) & 0xFFu);
            out_decoded->push_back(static_cast<char>(byte));
            if (out_decoded->size() > max_decoded_bytes) {
                return false;
            }
        }
    }

    if (bits > 0u) {
        const uint64_t mask = (static_cast<uint64_t>(1u) << bits) - 1u;
        if ((buffer & mask) != 0u) {
            return false;
        }
    }
    return !out_decoded->empty();
}

inline void skipJsonWhitespace(std::string_view text, std::size_t* cursor) {
    if (!cursor) {
        return;
    }
    while (*cursor < text.size()) {
        const char ch = text[*cursor];
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            ++(*cursor);
            continue;
        }
        break;
    }
}

inline bool parseJsonStringLiteral(std::string_view json,
                                   std::size_t* cursor,
                                   std::string* out_value) {
    if (!cursor || !out_value || *cursor >= json.size() || json[*cursor] != '"') {
        return false;
    }

    ++(*cursor);
    out_value->clear();
    while (*cursor < json.size()) {
        const char ch = json[*cursor];
        ++(*cursor);
        if (ch == '"') {
            return true;
        }
        if (ch == '\\') {
            if (*cursor >= json.size()) {
                return false;
            }
            const char escaped = json[*cursor];
            ++(*cursor);
            switch (escaped) {
                case '"':
                case '\\':
                case '/':
                    out_value->push_back(escaped);
                    break;
                case 'b':
                    out_value->push_back('\b');
                    break;
                case 'f':
                    out_value->push_back('\f');
                    break;
                case 'n':
                    out_value->push_back('\n');
                    break;
                case 'r':
                    out_value->push_back('\r');
                    break;
                case 't':
                    out_value->push_back('\t');
                    break;
                default:
                    return false;
            }
            continue;
        }

        if (static_cast<unsigned char>(ch) < 0x20u) {
            return false;
        }
        out_value->push_back(ch);
    }
    return false;
}

inline bool findJsonFieldValueStart(std::string_view json,
                                    const char* field_name,
                                    std::size_t* out_cursor) {
    if (!field_name || !out_cursor) {
        return false;
    }

    const std::string needle = std::string("\"") + field_name + "\"";
    std::size_t cursor = json.find(needle);
    if (cursor == std::string_view::npos) {
        return false;
    }

    cursor += needle.size();
    skipJsonWhitespace(json, &cursor);
    if (cursor >= json.size() || json[cursor] != ':') {
        return false;
    }
    ++cursor;
    skipJsonWhitespace(json, &cursor);
    if (cursor >= json.size()) {
        return false;
    }

    *out_cursor = cursor;
    return true;
}

inline bool extractJsonStringField(std::string_view json,
                                   const char* field_name,
                                   std::string* out_value) {
    std::size_t cursor = 0;
    if (!findJsonFieldValueStart(json, field_name, &cursor)) {
        return false;
    }
    return parseJsonStringLiteral(json, &cursor, out_value);
}

inline bool extractJsonUInt64Field(std::string_view json,
                                   const char* field_name,
                                   uint64_t* out_value) {
    std::size_t cursor = 0;
    if (!findJsonFieldValueStart(json, field_name, &cursor)) {
        return false;
    }

    const std::size_t begin = cursor;
    while (cursor < json.size() && json[cursor] >= '0' && json[cursor] <= '9') {
        ++cursor;
    }
    if (cursor == begin) {
        return false;
    }

    return parseUInt64(json.substr(begin, cursor - begin), out_value);
}

inline bool extractAudienceField(std::string_view payload_json, std::string* out_audience) {
    if (extractJsonStringField(payload_json, "aud", out_audience)) {
        return !out_audience->empty() && out_audience->size() <= kMaxAudienceBytes;
    }

    std::size_t cursor = 0;
    if (!findJsonFieldValueStart(payload_json, "aud", &cursor)) {
        return false;
    }
    if (cursor >= payload_json.size() || payload_json[cursor] != '[') {
        return false;
    }

    ++cursor;
    skipJsonWhitespace(payload_json, &cursor);
    std::string array_value;
    if (!parseJsonStringLiteral(payload_json, &cursor, &array_value) || array_value.empty()) {
        return false;
    }
    skipJsonWhitespace(payload_json, &cursor);
    if (cursor >= payload_json.size() || payload_json[cursor] != ']') {
        return false;
    }
    if (array_value.size() > kMaxAudienceBytes) {
        return false;
    }

    *out_audience = std::move(array_value);
    return true;
}

inline bool parseCompactJwt(sb_auth_slice_t token_slice, ParsedJwtToken* out_token) {
    if (!out_token || !tokenLooksJwtLike(token_slice)) {
        return false;
    }

    const std::string token = toString(token_slice);
    const std::size_t dot1 = token.find('.');
    if (dot1 == std::string::npos) {
        return false;
    }
    const std::size_t dot2 = token.find('.', dot1 + 1);
    if (dot2 == std::string::npos || token.find('.', dot2 + 1) != std::string::npos) {
        return false;
    }

    ParsedJwtToken parsed{};
    parsed.header_segment = token.substr(0, dot1);
    parsed.payload_segment = token.substr(dot1 + 1, dot2 - dot1 - 1);
    parsed.signature_segment = token.substr(dot2 + 1);
    if (parsed.header_segment.empty() || parsed.payload_segment.empty() ||
        parsed.signature_segment.empty()) {
        return false;
    }
    if (parsed.signature_segment.size() != kExpectedSignatureHexBytes) {
        return false;
    }
    for (char ch : parsed.signature_segment) {
        if (!isLowerHex(ch)) {
            return false;
        }
    }

    std::string header_json;
    std::string payload_json;
    if (!decodeBase64Url(parsed.header_segment, &header_json, kMaxDecodedHeaderBytes) ||
        !decodeBase64Url(parsed.payload_segment, &payload_json, kMaxDecodedPayloadBytes)) {
        return false;
    }

    if (!extractJsonStringField(header_json, "alg", &parsed.algorithm) || parsed.algorithm.empty()) {
        return false;
    }
    if (!extractJsonStringField(payload_json, "iss", &parsed.issuer) || parsed.issuer.empty()) {
        return false;
    }
    if (!extractAudienceField(payload_json, &parsed.audience) || parsed.audience.empty()) {
        return false;
    }
    if (!extractJsonStringField(payload_json, "sub", &parsed.subject) || parsed.subject.empty()) {
        return false;
    }
    if (!extractJsonUInt64Field(payload_json, "exp", &parsed.exp_unix_ms) ||
        parsed.exp_unix_ms == 0) {
        return false;
    }
    if (!isBoundedTokenField(parsed.algorithm, kMaxAlgBytes)) {
        return false;
    }
    if (parsed.issuer.size() > kMaxIssuerBytes || parsed.audience.size() > kMaxAudienceBytes ||
        parsed.subject.size() > kMaxSubjectBytes) {
        return false;
    }

    *out_token = std::move(parsed);
    return true;
}

inline std::string computeCompactJwtSignature(const ParsedJwtToken& token,
                                              const std::string& signing_key) {
    const std::string signing_input = token.header_segment + "." + token.payload_segment;
    std::string signature;
    if (!scratchbird::security::plugins::crypto::hmacSha256Hex(signing_key,
                                                                signing_input,
                                                                &signature)) {
        return "";
    }
    return signature;
}

inline bool signaturesEqual(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    if (g_host_api && g_host_api->const_time_equal) {
        return g_host_api->const_time_equal(
                   reinterpret_cast<const uint8_t*>(lhs.data()),
                   reinterpret_cast<const uint8_t*>(rhs.data()),
                   static_cast<uint32_t>(lhs.size())) != 0;
    }
    return std::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}

inline bool loadPolicyString(const char* key, std::string* out) {
    if (!key || !out || !g_host_api || !g_host_api->read_policy_value) {
        return false;
    }
    const sb_auth_slice_t key_slice{
        reinterpret_cast<const uint8_t*>(key),
        static_cast<uint32_t>(std::strlen(key))
    };
    sb_auth_slice_t value{};
    if (g_host_api->read_policy_value(key_slice, &value) != SB_AUTH_RC_OK || !value.ptr ||
        value.len == 0) {
        return false;
    }
    *out = toString(value);
    return !out->empty();
}

inline void zeroAndPrimeResult(sb_auth_step_result_v1* out_result) {
    if (!out_result) {
        return;
    }
    std::memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = sizeof(sb_auth_step_result_v1);
    out_result->principal.struct_size = sizeof(sb_auth_principal_v1);
}

inline void fillDeniedResult(sb_auth_step_result_v1* out_result,
                             uint32_t plugin_error_numeric,
                             const char* plugin_error_code) {
    if (!out_result) {
        return;
    }
    zeroAndPrimeResult(out_result);
    out_result->rc = SB_AUTH_RC_DENY;
    out_result->plugin_error_numeric = plugin_error_numeric;
    copyCStr(out_result->plugin_error_code,
             sizeof(out_result->plugin_error_code),
             plugin_error_code);
    copyCStr(out_result->sqlstate, sizeof(out_result->sqlstate), "28000");
}

inline const MethodPolicy* resolveMethodPolicy(sb_auth_slice_t method_id,
                                               const char** out_audit_method) {
    static const MethodPolicy kJwtPolicy = {
        kMethodJwtBearer,
        kIssuerJwt,
        kJwtSigningKeyPolicyRef,
        kPolicyJwtExpectedIssuer,
        kPolicyJwtExpectedAudience,
        kPolicyJwtRequiredAlg,
        65,
    };

    static const MethodPolicy kOidcPolicy = {
        kMethodOidcIdToken,
        kIssuerOidc,
        kOidcSigningKeyPolicyRef,
        kPolicyOidcExpectedIssuer,
        kPolicyOidcExpectedAudience,
        kPolicyOidcRequiredAlg,
        70,
    };

    if (sliceEqualsLiteral(method_id, kMethodJwtBearer)) {
        if (out_audit_method) {
            *out_audit_method = kMethodJwtBearer;
        }
        return &kJwtPolicy;
    }
    if (sliceEqualsLiteral(method_id, kMethodOidcIdToken)) {
        if (out_audit_method) {
            *out_audit_method = kMethodOidcIdToken;
        }
        return &kOidcPolicy;
    }
    return nullptr;
}

inline sb_auth_rc_t createInstance(sb_auth_plugin_instance_t* out_instance) {
    if (!out_instance) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }
    *out_instance = g_next_instance.fetch_add(1, std::memory_order_relaxed);
    return SB_AUTH_RC_OK;
}

inline void destroyInstance(sb_auth_plugin_instance_t /*instance*/) {}

inline sb_auth_rc_t configureInstance(sb_auth_plugin_instance_t /*instance*/,
                                      sb_auth_slice_t config_json) {
    if (config_json.len == 0) {
        return SB_AUTH_RC_OK;
    }
    if (!config_json.ptr) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }
    return SB_AUTH_RC_UNSUPPORTED;
}

inline sb_auth_rc_t beginAuth(sb_auth_plugin_instance_t /*instance*/,
                              sb_auth_slice_t method_id,
                              const sb_auth_connection_ctx_v1* conn,
                              sb_auth_slice_t client_payload,
                              sb_auth_exchange_t* inout_exchange,
                              sb_auth_step_result_v1* out_result) {
    auto finish = [&](sb_auth_rc_t rc, const char* method, const char* error_code) {
        scratchbird::security::plugins::observability::recordOutcome(g_counters, rc);
        scratchbird::security::plugins::observability::emitAuditEvent(
            g_host_api,
            "auth_plugin.jwt_oidc.begin",
            kPluginId,
            method,
            rc,
            error_code);
        return rc;
    };

    if (!inout_exchange || !out_result) {
        fillDeniedResult(out_result, 7001, kErrBadRequest);
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, nullptr, kErrBadRequest);
    }
    *inout_exchange = 0;

    const char* audit_method = nullptr;
    const MethodPolicy* method_policy = resolveMethodPolicy(method_id, &audit_method);
    if (!method_policy) {
        fillDeniedResult(out_result, 7002, kErrUnknownMethod);
        return finish(SB_AUTH_RC_UNSUPPORTED, nullptr, kErrUnknownMethod);
    }

    if (!client_payload.ptr || client_payload.len == 0) {
        fillDeniedResult(out_result, 7003, kErrMissingToken);
        return finish(SB_AUTH_RC_DENY, audit_method, kErrMissingToken);
    }
    if (client_payload.len > kMaxTokenBytes) {
        fillDeniedResult(out_result, 7004, kErrOversizedToken);
        return finish(SB_AUTH_RC_DENY, audit_method, kErrOversizedToken);
    }
    if (!tokenLooksJwtLike(client_payload)) {
        fillDeniedResult(out_result, 7005, kErrMalformedToken);
        return finish(SB_AUTH_RC_DENY, audit_method, kErrMalformedToken);
    }

    scratchbird::security::plugins::secrets::SecretMaterial signing_key{};
    if (scratchbird::security::plugins::secrets::resolveSecretReference(
            g_host_api,
            method_policy->signing_key_policy_ref,
            &signing_key) != scratchbird::security::plugins::secrets::SecretResolveStatus::OK) {
        fillDeniedResult(out_result, 7009, kErrSigningKeyUnavailable);
        return finish(SB_AUTH_RC_DENY, audit_method, kErrSigningKeyUnavailable);
    }

    ParsedJwtToken token{};
    if (!parseCompactJwt(client_payload, &token)) {
        fillDeniedResult(out_result, 7005, kErrMalformedToken);
        return finish(SB_AUTH_RC_DENY, audit_method, kErrMalformedToken);
    }

    std::string required_alg = kDefaultRequiredAlg;
    std::string expected_issuer = method_policy->issuer_literal;
    std::string expected_audience = kDefaultAudience;
    (void) loadPolicyString(method_policy->required_alg_policy_key, &required_alg);
    (void) loadPolicyString(method_policy->expected_issuer_policy_key, &expected_issuer);
    (void) loadPolicyString(method_policy->expected_audience_policy_key, &expected_audience);

    if (token.algorithm != required_alg) {
        fillDeniedResult(out_result, 7010, kErrAlgMismatch);
        return finish(SB_AUTH_RC_DENY, audit_method, kErrAlgMismatch);
    }
    if (token.issuer != expected_issuer) {
        fillDeniedResult(out_result, 7011, kErrIssuerMismatch);
        return finish(SB_AUTH_RC_DENY, audit_method, kErrIssuerMismatch);
    }
    if (token.audience != expected_audience) {
        fillDeniedResult(out_result, 7012, kErrAudienceMismatch);
        return finish(SB_AUTH_RC_DENY, audit_method, kErrAudienceMismatch);
    }
    if (token.exp_unix_ms <= nowUnixMs()) {
        fillDeniedResult(out_result, 7013, kErrExpired);
        return finish(SB_AUTH_RC_DENY, audit_method, kErrExpired);
    }

    const std::string expected_signature = computeCompactJwtSignature(token, signing_key.value);
    if (!signaturesEqual(expected_signature, token.signature_segment)) {
        fillDeniedResult(out_result, 7014, kErrSignatureInvalid);
        out_result->rc = SB_AUTH_RC_SIGNATURE_INVALID;
        return finish(SB_AUTH_RC_SIGNATURE_INVALID, audit_method, kErrSignatureInvalid);
    }

    if (!g_host_api || !g_host_api->resolve_user_by_external_subject) {
        fillDeniedResult(out_result, 7006, kErrResolverUnavailable);
        return finish(SB_AUTH_RC_DENY, audit_method, kErrResolverUnavailable);
    }

    const sb_auth_slice_t issuer{
        reinterpret_cast<const uint8_t*>(method_policy->issuer_literal),
        static_cast<uint32_t>(std::strlen(method_policy->issuer_literal))
    };
    const sb_auth_slice_t subject{
        reinterpret_cast<const uint8_t*>(token.subject.data()),
        static_cast<uint32_t>(token.subject.size())
    };

    zeroAndPrimeResult(out_result);
    const sb_auth_rc_t resolve_rc = g_host_api->resolve_user_by_external_subject(
        issuer,
        subject,
        out_result->principal.principal_uuid);
    if (resolve_rc != SB_AUTH_RC_OK) {
        fillDeniedResult(out_result, 7007, kErrSubjectUnknown);
        return finish(SB_AUTH_RC_DENY, audit_method, kErrSubjectUnknown);
    }

    out_result->rc = SB_AUTH_RC_OK;
    out_result->principal.assurance_level = method_policy->assurance_level;

    g_external_subject_payload = token.subject;
    out_result->principal.external_subject = sb_auth_slice_t{
        reinterpret_cast<const uint8_t*>(g_external_subject_payload.data()),
        static_cast<uint32_t>(g_external_subject_payload.size())
    };

    if (conn && conn->username.ptr && conn->username.len > 0) {
        out_result->principal.resolved_username = conn->username;
    }
    copyCStr(out_result->sqlstate, sizeof(out_result->sqlstate), "00000");
    return finish(SB_AUTH_RC_OK, audit_method, nullptr);
}

inline sb_auth_rc_t continueAuth(sb_auth_plugin_instance_t /*instance*/,
                                 sb_auth_exchange_t /*exchange*/,
                                 sb_auth_slice_t /*client_payload*/,
                                 sb_auth_step_result_v1* out_result) {
    fillDeniedResult(out_result, 7008, kErrNoContinuation);
    scratchbird::security::plugins::observability::recordOutcome(g_counters, SB_AUTH_RC_DENY);
    scratchbird::security::plugins::observability::emitAuditEvent(
        g_host_api,
        "auth_plugin.jwt_oidc.continue",
        kPluginId,
        nullptr,
        SB_AUTH_RC_DENY,
        kErrNoContinuation);
    return SB_AUTH_RC_DENY;
}

inline void abortAuth(sb_auth_plugin_instance_t /*instance*/,
                      sb_auth_exchange_t /*exchange*/) {}

inline sb_auth_rc_t healthCheck(sb_auth_plugin_instance_t /*instance*/,
                                sb_auth_slice_t* out_json) {
    if (!out_json) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }
    g_health_payload = "{\"status\":\"ok\",\"plugin\":\"jwt_oidc\",\"allow_count\":" +
                       std::to_string(
                           scratchbird::security::plugins::observability::loadCounter(g_counters.allow)) +
                       ",\"deny_count\":" +
                       std::to_string(
                           scratchbird::security::plugins::observability::loadCounter(g_counters.deny)) +
                       ",\"continue_count\":" +
                       std::to_string(
                           scratchbird::security::plugins::observability::loadCounter(g_counters.cont)) +
                       ",\"error_count\":" +
                       std::to_string(
                           scratchbird::security::plugins::observability::loadCounter(g_counters.error)) +
                       "}";
    out_json->ptr = reinterpret_cast<const uint8_t*>(g_health_payload.data());
    out_json->len = static_cast<uint32_t>(g_health_payload.size());
    return SB_AUTH_RC_OK;
}

inline const sb_auth_plugin_descriptor_v1* descriptor() {
    static const std::array<sb_auth_method_descriptor_v1, 2> kMethods = [] {
        std::array<sb_auth_method_descriptor_v1, 2> methods{};

        std::memset(&methods[0], 0, sizeof(methods[0]));
        copyCStr(methods[0].method_id, sizeof(methods[0].method_id), kMethodJwtBearer);
        methods[0].method_flags = 0;
        methods[0].legacy_wire_code = kLegacyWireUnknown;

        std::memset(&methods[1], 0, sizeof(methods[1]));
        copyCStr(methods[1].method_id, sizeof(methods[1].method_id), kMethodOidcIdToken);
        methods[1].method_flags = 0;
        methods[1].legacy_wire_code = kLegacyWireUnknown;

        return methods;
    }();

    static const sb_auth_slice_t kVersion{
        reinterpret_cast<const uint8_t*>(kPluginVersion),
        static_cast<uint32_t>(sizeof(kPluginVersion) - 1)
    };

    static const sb_auth_plugin_descriptor_v1 kDescriptor = [] {
        sb_auth_plugin_descriptor_v1 desc{};
        std::memset(&desc, 0, sizeof(desc));
        desc.struct_size = sizeof(sb_auth_plugin_descriptor_v1);
        copyCStr(desc.plugin_id, sizeof(desc.plugin_id), kPluginId);
        desc.plugin_version = kVersion;
        desc.abi_major = SB_AUTH_ABI_MAJOR;
        desc.abi_minor = SB_AUTH_ABI_MINOR;
        desc.method_count = static_cast<uint32_t>(kMethods.size());
        desc.methods = kMethods.data();
        return desc;
    }();

    return &kDescriptor;
}

inline const sb_auth_plugin_api_v1* api() {
    static const sb_auth_plugin_api_v1 kApi = {
        sizeof(sb_auth_plugin_api_v1),
        &createInstance,
        &destroyInstance,
        &configureInstance,
        &beginAuth,
        &continueAuth,
        &abortAuth,
        &healthCheck,
    };
    return &kApi;
}

}  // namespace

extern "C" sb_auth_rc_t sb_auth_plugin_get_api_v1(
    uint32_t requested_abi_major,
    const sb_auth_host_api_v1* host_api,
    const sb_auth_plugin_descriptor_v1** out_descriptor,
    const sb_auth_plugin_api_v1** out_api) {
    if (!host_api || !out_descriptor || !out_api) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }
    if (requested_abi_major != SB_AUTH_ABI_MAJOR) {
        return SB_AUTH_RC_UNSUPPORTED;
    }

    g_host_api = host_api;
    *out_descriptor = descriptor();
    *out_api = api();
    return SB_AUTH_RC_OK;
}
