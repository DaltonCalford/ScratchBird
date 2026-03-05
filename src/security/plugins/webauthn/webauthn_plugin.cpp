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
#include "../challenge_state_store.h"

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

constexpr char kPluginId[] = "scratchbird.auth.webauthn";
constexpr char kPluginVersion[] = "2.0.0";
constexpr char kMethodWebAuthnAssertion[] = "scratchbird.auth.webauthn_assertion";
constexpr char kWebAuthnIssuer[] = "webauthn";
constexpr uint32_t kLegacyWireUnknown = 0xFFFFFFFFu;
constexpr uint32_t kMaxPayloadBytes = 2048;

constexpr char kPolicySigningKeyRef[] = "policy:auth.webauthn.signing_key_ref";
constexpr char kPolicyRpId[] = "auth.webauthn.rp_id";
constexpr char kPolicyAllowedOrigin[] = "auth.webauthn.allowed_origin";
constexpr char kPolicyRequireUv[] = "auth.webauthn.require_uv";
constexpr char kDefaultRpId[] = "scratchbird";

constexpr char kErrBadRequest[] = "AUTH_WEBAUTHN_BAD_REQUEST";
constexpr char kErrUnknownMethod[] = "AUTH_WEBAUTHN_METHOD_UNKNOWN";
constexpr char kErrMissingChallengeRequest[] = "AUTH_WEBAUTHN_CHALLENGE_REQUEST_MISSING";
constexpr char kErrMalformedChallengeRequest[] = "AUTH_WEBAUTHN_CHALLENGE_REQUEST_MALFORMED";
constexpr char kErrMissingAssertion[] = "AUTH_WEBAUTHN_ASSERTION_MISSING";
constexpr char kErrMalformedAssertion[] = "AUTH_WEBAUTHN_ASSERTION_MALFORMED";
constexpr char kErrUnknownExchange[] = "AUTH_WEBAUTHN_UNKNOWN_EXCHANGE";
constexpr char kErrChallengeMismatch[] = "AUTH_WEBAUTHN_CHALLENGE_MISMATCH";
constexpr char kErrReplayDetected[] = "AUTH_WEBAUTHN_REPLAY_DETECTED";
constexpr char kErrExchangeExpired[] = "AUTH_WEBAUTHN_EXCHANGE_EXPIRED";
constexpr char kErrSigningKeyUnavailable[] = "AUTH_WEBAUTHN_SIGNING_KEY_UNAVAILABLE";
constexpr char kErrRpMismatch[] = "AUTH_WEBAUTHN_RP_MISMATCH";
constexpr char kErrOriginMismatch[] = "AUTH_WEBAUTHN_ORIGIN_MISMATCH";
constexpr char kErrCredentialMismatch[] = "AUTH_WEBAUTHN_CREDENTIAL_MISMATCH";
constexpr char kErrUserVerificationRequired[] = "AUTH_WEBAUTHN_UV_REQUIRED";
constexpr char kErrAssertionExpired[] = "AUTH_WEBAUTHN_ASSERTION_EXPIRED";
constexpr char kErrSignatureInvalid[] = "AUTH_WEBAUTHN_SIGNATURE_INVALID";
constexpr char kErrResolverUnavailable[] = "AUTH_WEBAUTHN_RESOLVER_UNAVAILABLE";
constexpr char kErrSubjectUnknown[] = "AUTH_WEBAUTHN_SUBJECT_UNKNOWN";

const sb_auth_host_api_v1* g_host_api = nullptr;
std::atomic<uint64_t> g_next_instance{1};
std::atomic<uint64_t> g_next_exchange{1};
scratchbird::security::plugins::observability::OutcomeCounters g_counters{};

struct WebAuthnExchangeState {
    sb_auth_plugin_instance_t instance = 0;
    std::array<uint8_t, 16> challenge{};
    std::string username;
    std::string credential_id;
};

struct BeginRequest {
    std::string username;
    std::string credential_id;
};

struct ParsedAssertion {
    std::string challenge_hex;
    std::string rp_id;
    std::string origin;
    std::string credential_id;
    std::string subject;
    bool user_verified = false;
    bool user_verified_set = false;
    uint64_t exp_unix_ms = 0;
    std::string signature;
};

scratchbird::security::plugins::challenge::ExchangeStore<WebAuthnExchangeState> g_exchanges{
    300000u};
scratchbird::security::plugins::challenge::ReplayCache g_replay_cache{600000u};

thread_local std::string g_result_payload;
thread_local std::string g_health_payload;
thread_local std::string g_username_payload;
thread_local std::string g_external_subject_payload;

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

inline std::string toString(sb_auth_slice_t value) {
    if (!value.ptr || value.len == 0) {
        return "";
    }
    const char* begin = reinterpret_cast<const char*>(value.ptr);
    return std::string(begin, begin + value.len);
}

inline std::string trimAscii(std::string_view value) {
    std::size_t begin = 0;
    while (begin < value.size() && (value[begin] == ' ' || value[begin] == '\t' ||
                                    value[begin] == '\n' || value[begin] == '\r')) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t' ||
                           value[end - 1] == '\n' || value[end - 1] == '\r')) {
        --end;
    }

    return std::string(value.substr(begin, end - begin));
}

inline bool parseUInt64(std::string_view text, uint64_t* out_value) {
    if (!out_value || text.empty()) {
        return false;
    }

    uint64_t value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        const uint64_t digit = static_cast<uint64_t>(ch - '0');
        const uint64_t next = value * 10u + digit;
        if (next < value) {
            return false;
        }
        value = next;
    }

    *out_value = value;
    return true;
}

inline bool parseBool(std::string_view text, bool* out_value) {
    if (!out_value) {
        return false;
    }
    if (text == "1" || text == "true" || text == "yes" || text == "on") {
        *out_value = true;
        return true;
    }
    if (text == "0" || text == "false" || text == "no" || text == "off") {
        *out_value = false;
        return true;
    }
    return false;
}

inline bool payloadLooksSafe(sb_auth_slice_t payload) {
    if (!payload.ptr || payload.len == 0 || payload.len > kMaxPayloadBytes) {
        return false;
    }

    for (uint32_t i = 0; i < payload.len; ++i) {
        const uint8_t c = payload.ptr[i];
        const bool allowed =
            (c >= static_cast<uint8_t>('a') && c <= static_cast<uint8_t>('z')) ||
            (c >= static_cast<uint8_t>('A') && c <= static_cast<uint8_t>('Z')) ||
            (c >= static_cast<uint8_t>('0') && c <= static_cast<uint8_t>('9')) ||
            c == static_cast<uint8_t>('_') || c == static_cast<uint8_t>('-') ||
            c == static_cast<uint8_t>('.') || c == static_cast<uint8_t>(':') ||
            c == static_cast<uint8_t>('=') || c == static_cast<uint8_t>(';') ||
            c == static_cast<uint8_t>(',') || c == static_cast<uint8_t>('/') ||
            c == static_cast<uint8_t>('@');
        if (!allowed) {
            return false;
        }
    }

    return true;
}

inline bool parseBeginRequest(std::string_view payload, BeginRequest* out_request) {
    if (!out_request || payload.empty()) {
        return false;
    }

    BeginRequest parsed{};

    std::size_t cursor = 0;
    while (cursor < payload.size()) {
        const std::size_t sep = payload.find(';', cursor);
        const std::size_t end = (sep == std::string_view::npos) ? payload.size() : sep;
        if (end <= cursor) {
            return false;
        }

        const std::string_view pair = payload.substr(cursor, end - cursor);
        const std::size_t eq = pair.find('=');
        if (eq == std::string_view::npos || eq == 0 || eq + 1 >= pair.size()) {
            return false;
        }

        const std::string key = trimAscii(pair.substr(0, eq));
        const std::string value = trimAscii(pair.substr(eq + 1));

        if (key == "user") {
            parsed.username = value;
        } else if (key == "cred") {
            parsed.credential_id = value;
        }

        if (sep == std::string_view::npos) {
            break;
        }
        cursor = sep + 1;
    }

    if (parsed.username.empty() || parsed.credential_id.empty()) {
        return false;
    }

    *out_request = std::move(parsed);
    return true;
}

inline bool parseAssertionPayload(std::string_view payload, ParsedAssertion* out_assertion) {
    if (!out_assertion || payload.empty()) {
        return false;
    }

    ParsedAssertion parsed{};

    std::size_t cursor = 0;
    while (cursor < payload.size()) {
        const std::size_t sep = payload.find(';', cursor);
        const std::size_t end = (sep == std::string_view::npos) ? payload.size() : sep;
        if (end <= cursor) {
            return false;
        }

        const std::string_view pair = payload.substr(cursor, end - cursor);
        const std::size_t eq = pair.find('=');
        if (eq == std::string_view::npos || eq == 0 || eq + 1 >= pair.size()) {
            return false;
        }

        const std::string key = trimAscii(pair.substr(0, eq));
        const std::string value = trimAscii(pair.substr(eq + 1));

        if (key == "challenge") {
            parsed.challenge_hex = value;
        } else if (key == "rp") {
            parsed.rp_id = value;
        } else if (key == "origin") {
            parsed.origin = value;
        } else if (key == "cred") {
            parsed.credential_id = value;
        } else if (key == "sub") {
            parsed.subject = value;
        } else if (key == "uv") {
            parsed.user_verified_set = parseBool(value, &parsed.user_verified);
            if (!parsed.user_verified_set) {
                return false;
            }
        } else if (key == "exp") {
            if (!parseUInt64(value, &parsed.exp_unix_ms)) {
                return false;
            }
        } else if (key == "sig") {
            parsed.signature = value;
        }

        if (sep == std::string_view::npos) {
            break;
        }
        cursor = sep + 1;
    }

    if (parsed.challenge_hex.empty() || parsed.rp_id.empty() || parsed.origin.empty() ||
        parsed.credential_id.empty() || parsed.subject.empty() || !parsed.user_verified_set ||
        parsed.exp_unix_ms == 0 || parsed.signature.empty()) {
        return false;
    }

    *out_assertion = std::move(parsed);
    return true;
}

inline std::string computeSignature(const ParsedAssertion& assertion,
                                    const std::string& signing_key) {
    const std::string payload = assertion.challenge_hex + "|" + assertion.rp_id + "|" +
                                assertion.origin + "|" + assertion.credential_id + "|" +
                                assertion.subject + "|" +
                                (assertion.user_verified ? "1" : "0") + "|" +
                                std::to_string(assertion.exp_unix_ms);
    std::string signature;
    if (!scratchbird::security::plugins::crypto::hmacSha256Hex(signing_key, payload, &signature)) {
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

inline uint64_t nowUnixMs() {
    if (g_host_api && g_host_api->now_unix_ms) {
        return g_host_api->now_unix_ms();
    }
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

inline bool loadPolicyString(const char* key, std::string* out_value) {
    if (!key || !out_value || !g_host_api || !g_host_api->read_policy_value) {
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

    *out_value = toString(value);
    return !out_value->empty();
}

inline bool loadPolicyBool(const char* key, bool fallback) {
    std::string value;
    if (!loadPolicyString(key, &value)) {
        return fallback;
    }

    bool parsed = fallback;
    if (!parseBool(value, &parsed)) {
        return fallback;
    }
    return parsed;
}

inline void copySliceString(sb_auth_slice_t* out_slice,
                            std::string* storage,
                            const std::string& value) {
    if (!out_slice || !storage) {
        return;
    }
    *storage = value;
    out_slice->ptr = reinterpret_cast<const uint8_t*>(storage->data());
    out_slice->len = static_cast<uint32_t>(storage->size());
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

inline void setPayload(sb_auth_step_result_v1* out_result, std::string payload) {
    if (!out_result) {
        return;
    }
    g_result_payload = std::move(payload);
    out_result->payload.ptr = reinterpret_cast<const uint8_t*>(g_result_payload.data());
    out_result->payload.len = static_cast<uint32_t>(g_result_payload.size());
}

inline std::string bytesToHex(const std::array<uint8_t, 16>& bytes) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(bytes.size() * 2);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        out[2 * i] = kHex[(bytes[i] >> 4) & 0x0F];
        out[2 * i + 1] = kHex[bytes[i] & 0x0F];
    }
    return out;
}

inline std::string exchangeReplayKey(sb_auth_exchange_t exchange) {
    return "webauthn:exchange:" + std::to_string(exchange);
}

inline std::string challengeReplayKey(std::string_view challenge_hex) {
    return "webauthn:challenge:" + std::string(challenge_hex);
}

inline std::string assertionReplayKey(const ParsedAssertion& assertion) {
    return "webauthn:assertion:" + assertion.credential_id + ":" + assertion.signature;
}

inline void makeChallenge(sb_auth_exchange_t exchange_id,
                          std::array<uint8_t, 16>& out_challenge) {
    if (g_host_api && g_host_api->secure_random &&
        g_host_api->secure_random(out_challenge.data(),
                                  static_cast<uint32_t>(out_challenge.size())) == SB_AUTH_RC_OK) {
        return;
    }

    for (std::size_t i = 0; i < out_challenge.size(); ++i) {
        const uint8_t seed = static_cast<uint8_t>((exchange_id >> ((i % 8) * 8)) & 0xFFu);
        out_challenge[i] = static_cast<uint8_t>(seed ^ static_cast<uint8_t>(0x5Au + i * 17u));
    }
}

inline sb_auth_rc_t createInstance(sb_auth_plugin_instance_t* out_instance) {
    if (!out_instance) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }
    *out_instance = g_next_instance.fetch_add(1, std::memory_order_relaxed);
    return SB_AUTH_RC_OK;
}

inline void destroyInstance(sb_auth_plugin_instance_t instance) {
    (void) g_exchanges.eraseIf([&](const WebAuthnExchangeState& state) {
        return state.instance == instance;
    });
}

inline sb_auth_rc_t configureInstance(sb_auth_plugin_instance_t /*instance*/,
                                      sb_auth_slice_t /*config_json*/) {
    return SB_AUTH_RC_OK;
}

inline sb_auth_rc_t beginAuth(sb_auth_plugin_instance_t instance,
                              sb_auth_slice_t method_id,
                              const sb_auth_connection_ctx_v1* /*conn*/,
                              sb_auth_slice_t client_payload,
                              sb_auth_exchange_t* inout_exchange,
                              sb_auth_step_result_v1* out_result) {
    auto finish = [&](sb_auth_rc_t rc, const char* error_code) {
        scratchbird::security::plugins::observability::recordOutcome(g_counters, rc);
        scratchbird::security::plugins::observability::emitAuditEvent(
            g_host_api,
            "auth_plugin.webauthn.begin",
            kPluginId,
            kMethodWebAuthnAssertion,
            rc,
            error_code);
        return rc;
    };

    if (!inout_exchange || !out_result) {
        fillDeniedResult(out_result, 12001, kErrBadRequest);
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, kErrBadRequest);
    }
    *inout_exchange = 0;

    if (!sliceEqualsLiteral(method_id, kMethodWebAuthnAssertion)) {
        fillDeniedResult(out_result, 12002, kErrUnknownMethod);
        return finish(SB_AUTH_RC_UNSUPPORTED, kErrUnknownMethod);
    }

    if (!client_payload.ptr || client_payload.len == 0) {
        fillDeniedResult(out_result, 12003, kErrMissingChallengeRequest);
        return finish(SB_AUTH_RC_DENY, kErrMissingChallengeRequest);
    }
    if (!payloadLooksSafe(client_payload)) {
        fillDeniedResult(out_result, 12004, kErrMalformedChallengeRequest);
        return finish(SB_AUTH_RC_DENY, kErrMalformedChallengeRequest);
    }

    BeginRequest begin_request{};
    if (!parseBeginRequest(toString(client_payload), &begin_request)) {
        fillDeniedResult(out_result, 12004, kErrMalformedChallengeRequest);
        return finish(SB_AUTH_RC_DENY, kErrMalformedChallengeRequest);
    }

    const sb_auth_exchange_t exchange_id = g_next_exchange.fetch_add(1, std::memory_order_relaxed);
    WebAuthnExchangeState state{};
    state.instance = instance;
    state.username = begin_request.username;
    state.credential_id = begin_request.credential_id;
    makeChallenge(exchange_id, state.challenge);
    g_exchanges.put(exchange_id, state);

    std::string rp_id = kDefaultRpId;
    (void) loadPolicyString(kPolicyRpId, &rp_id);

    const std::string challenge_hex = bytesToHex(state.challenge);
    const std::string challenge_payload =
        "type=webauthn_challenge;exchange=" + std::to_string(exchange_id) +
        ";challenge=" + challenge_hex + ";rp=" + rp_id + ";cred=" + state.credential_id;

    *inout_exchange = exchange_id;
    zeroAndPrimeResult(out_result);
    out_result->rc = SB_AUTH_RC_CONTINUE;
    copyCStr(out_result->sqlstate, sizeof(out_result->sqlstate), "00000");
    setPayload(out_result, challenge_payload);
    return finish(SB_AUTH_RC_CONTINUE, nullptr);
}

inline sb_auth_rc_t continueAuth(sb_auth_plugin_instance_t /*instance*/,
                                 sb_auth_exchange_t exchange,
                                 sb_auth_slice_t client_payload,
                                 sb_auth_step_result_v1* out_result) {
    auto finish = [&](sb_auth_rc_t rc, const char* error_code) {
        scratchbird::security::plugins::observability::recordOutcome(g_counters, rc);
        scratchbird::security::plugins::observability::emitAuditEvent(
            g_host_api,
            "auth_plugin.webauthn.continue",
            kPluginId,
            kMethodWebAuthnAssertion,
            rc,
            error_code);
        return rc;
    };

    if (!out_result) {
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, kErrBadRequest);
    }

    if (!client_payload.ptr || client_payload.len == 0) {
        fillDeniedResult(out_result, 12005, kErrMissingAssertion);
        return finish(SB_AUTH_RC_DENY, kErrMissingAssertion);
    }
    if (!payloadLooksSafe(client_payload)) {
        fillDeniedResult(out_result, 12006, kErrMalformedAssertion);
        return finish(SB_AUTH_RC_DENY, kErrMalformedAssertion);
    }

    WebAuthnExchangeState state{};
    const auto take_status = g_exchanges.take(exchange, &state);
    if (take_status == scratchbird::security::plugins::challenge::ExchangeStore<
                           WebAuthnExchangeState>::TakeStatus::Missing) {
        if (g_replay_cache.contains(exchangeReplayKey(exchange))) {
            fillDeniedResult(out_result, 12008, kErrReplayDetected);
            return finish(SB_AUTH_RC_DENY, kErrReplayDetected);
        }
        fillDeniedResult(out_result, 12007, kErrUnknownExchange);
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, kErrUnknownExchange);
    }
    if (take_status == scratchbird::security::plugins::challenge::ExchangeStore<
                           WebAuthnExchangeState>::TakeStatus::Expired) {
        fillDeniedResult(out_result, 12009, kErrExchangeExpired);
        return finish(SB_AUTH_RC_DENY, kErrExchangeExpired);
    }

    const std::string exchange_key = exchangeReplayKey(exchange);
    if (!g_replay_cache.remember(exchange_key)) {
        fillDeniedResult(out_result, 12008, kErrReplayDetected);
        return finish(SB_AUTH_RC_DENY, kErrReplayDetected);
    }

    ParsedAssertion assertion{};
    if (!parseAssertionPayload(toString(client_payload), &assertion)) {
        fillDeniedResult(out_result, 12006, kErrMalformedAssertion);
        return finish(SB_AUTH_RC_DENY, kErrMalformedAssertion);
    }

    const std::string expected_challenge = bytesToHex(state.challenge);
    if (assertion.challenge_hex != expected_challenge) {
        fillDeniedResult(out_result, 12010, kErrChallengeMismatch);
        return finish(SB_AUTH_RC_DENY, kErrChallengeMismatch);
    }

    const std::string challenge_key = challengeReplayKey(assertion.challenge_hex);
    if (!g_replay_cache.remember(challenge_key)) {
        fillDeniedResult(out_result, 12008, kErrReplayDetected);
        return finish(SB_AUTH_RC_DENY, kErrReplayDetected);
    }

    std::string expected_rp = kDefaultRpId;
    (void) loadPolicyString(kPolicyRpId, &expected_rp);
    if (assertion.rp_id != expected_rp) {
        fillDeniedResult(out_result, 12012, kErrRpMismatch);
        return finish(SB_AUTH_RC_DENY, kErrRpMismatch);
    }

    std::string allowed_origin;
    if (loadPolicyString(kPolicyAllowedOrigin, &allowed_origin) &&
        assertion.origin != allowed_origin) {
        fillDeniedResult(out_result, 12013, kErrOriginMismatch);
        return finish(SB_AUTH_RC_DENY, kErrOriginMismatch);
    }

    if (assertion.credential_id != state.credential_id) {
        fillDeniedResult(out_result, 12014, kErrCredentialMismatch);
        return finish(SB_AUTH_RC_DENY, kErrCredentialMismatch);
    }

    if (loadPolicyBool(kPolicyRequireUv, true) && !assertion.user_verified) {
        fillDeniedResult(out_result, 12015, kErrUserVerificationRequired);
        return finish(SB_AUTH_RC_DENY, kErrUserVerificationRequired);
    }

    if (assertion.exp_unix_ms <= nowUnixMs()) {
        fillDeniedResult(out_result, 12016, kErrAssertionExpired);
        return finish(SB_AUTH_RC_DENY, kErrAssertionExpired);
    }

    scratchbird::security::plugins::secrets::SecretMaterial signing_key{};
    if (scratchbird::security::plugins::secrets::resolveSecretReference(
            g_host_api,
            kPolicySigningKeyRef,
            &signing_key) != scratchbird::security::plugins::secrets::SecretResolveStatus::OK) {
        fillDeniedResult(out_result, 12011, kErrSigningKeyUnavailable);
        return finish(SB_AUTH_RC_DENY, kErrSigningKeyUnavailable);
    }

    const std::string expected_signature = computeSignature(assertion, signing_key.value);
    if (!signaturesEqual(expected_signature, assertion.signature)) {
        fillDeniedResult(out_result, 12017, kErrSignatureInvalid);
        out_result->rc = SB_AUTH_RC_SIGNATURE_INVALID;
        return finish(SB_AUTH_RC_SIGNATURE_INVALID, kErrSignatureInvalid);
    }

    const std::string assertion_key = assertionReplayKey(assertion);
    if (!g_replay_cache.remember(assertion_key)) {
        fillDeniedResult(out_result, 12008, kErrReplayDetected);
        return finish(SB_AUTH_RC_DENY, kErrReplayDetected);
    }

    if (!g_host_api || !g_host_api->resolve_user_by_external_subject) {
        fillDeniedResult(out_result, 12018, kErrResolverUnavailable);
        return finish(SB_AUTH_RC_DENY, kErrResolverUnavailable);
    }

    const sb_auth_slice_t issuer_slice{
        reinterpret_cast<const uint8_t*>(kWebAuthnIssuer),
        static_cast<uint32_t>(std::strlen(kWebAuthnIssuer))
    };
    const sb_auth_slice_t subject_slice{
        reinterpret_cast<const uint8_t*>(assertion.subject.data()),
        static_cast<uint32_t>(assertion.subject.size())
    };

    zeroAndPrimeResult(out_result);
    const sb_auth_rc_t resolve_rc = g_host_api->resolve_user_by_external_subject(
        issuer_slice,
        subject_slice,
        out_result->principal.principal_uuid);
    if (resolve_rc != SB_AUTH_RC_OK) {
        fillDeniedResult(out_result, 12019, kErrSubjectUnknown);
        return finish(SB_AUTH_RC_DENY, kErrSubjectUnknown);
    }

    out_result->rc = SB_AUTH_RC_OK;
    out_result->principal.assurance_level = 95;
    copySliceString(&out_result->principal.resolved_username, &g_username_payload, state.username);
    copySliceString(&out_result->principal.external_subject,
                    &g_external_subject_payload,
                    assertion.subject);
    copyCStr(out_result->sqlstate, sizeof(out_result->sqlstate), "00000");
    return finish(SB_AUTH_RC_OK, nullptr);
}

inline void abortAuth(sb_auth_plugin_instance_t /*instance*/, sb_auth_exchange_t exchange) {
    (void) g_exchanges.erase(exchange);
}

inline sb_auth_rc_t healthCheck(sb_auth_plugin_instance_t /*instance*/,
                                sb_auth_slice_t* out_json) {
    if (!out_json) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }

    g_health_payload =
        "{\"status\":\"ok\",\"plugin\":\"webauthn\",\"active_exchanges\":" +
        std::to_string(g_exchanges.size()) +
        ",\"replay_cache_entries\":" + std::to_string(g_replay_cache.size()) +
        ",\"allow_count\":" +
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
    static const std::array<sb_auth_method_descriptor_v1, 1> kMethods = [] {
        std::array<sb_auth_method_descriptor_v1, 1> methods{};
        std::memset(&methods[0], 0, sizeof(methods[0]));
        copyCStr(methods[0].method_id,
                 sizeof(methods[0].method_id),
                 kMethodWebAuthnAssertion);
        methods[0].method_flags = 0;
        methods[0].legacy_wire_code = kLegacyWireUnknown;
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
