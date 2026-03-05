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
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace {

constexpr char kPluginId[] = "scratchbird.auth.scram";
constexpr char kPluginVersion[] = "2.0.0";
constexpr char kMethodScramSha256[] = "scratchbird.auth.scram_sha_256";
constexpr char kMethodScramSha512[] = "scratchbird.auth.scram_sha_512";
constexpr char kPolicyCredentialRefPrefix[] = "auth.scram.credential_ref.";
constexpr char kPolicyDefaultCredentialRef[] = "auth.scram.default_credential_ref";
constexpr uint32_t kLegacyWireUnknown = 0xFFFFFFFFu;
constexpr uint32_t kMaxPayloadBytes = 2048;

constexpr char kErrBadRequest[] = "AUTH_SCRAM_BAD_REQUEST";
constexpr char kErrUnknownMethod[] = "AUTH_SCRAM_METHOD_UNKNOWN";
constexpr char kErrMissingClientFirst[] = "AUTH_SCRAM_CLIENT_FIRST_MISSING";
constexpr char kErrMissingClientProof[] = "AUTH_SCRAM_CLIENT_PROOF_MISSING";
constexpr char kErrMalformedClientFirst[] = "AUTH_SCRAM_CLIENT_FIRST_MALFORMED";
constexpr char kErrMalformedClientProof[] = "AUTH_SCRAM_CLIENT_PROOF_MALFORMED";
constexpr char kErrUnknownExchange[] = "AUTH_SCRAM_UNKNOWN_EXCHANGE";
constexpr char kErrReplayDetected[] = "AUTH_SCRAM_REPLAY_DETECTED";
constexpr char kErrExchangeExpired[] = "AUTH_SCRAM_EXCHANGE_EXPIRED";
constexpr char kErrCredentialUnavailable[] = "AUTH_SCRAM_CREDENTIAL_UNAVAILABLE";
constexpr char kErrCredentialMalformed[] = "AUTH_SCRAM_CREDENTIAL_MALFORMED";
constexpr char kErrProofInvalid[] = "AUTH_SCRAM_PROOF_INVALID";
constexpr char kErrResolverUnavailable[] = "AUTH_SCRAM_RESOLVER_UNAVAILABLE";
constexpr char kErrUserUnknown[] = "AUTH_SCRAM_USER_UNKNOWN";
constexpr char kErrNoContinuation[] = "AUTH_SCRAM_NO_CONTINUE";

const sb_auth_host_api_v1* g_host_api = nullptr;
std::atomic<uint64_t> g_next_instance{1};
std::atomic<uint64_t> g_next_exchange{1};
scratchbird::security::plugins::observability::OutcomeCounters g_counters{};

struct ScramCredential {
    std::string salt;
    std::string secret;
    uint32_t iterations = 0;
};

struct ScramExchangeState {
    sb_auth_plugin_instance_t instance = 0;
    std::string method;
    std::string username;
    std::string client_nonce;
    std::string server_nonce;
    ScramCredential credential;
};

struct ClientFirst {
    std::string username;
    std::string client_nonce;
};

struct ClientFinal {
    std::string proof;
    std::string combined_nonce;
};

scratchbird::security::plugins::challenge::ExchangeStore<ScramExchangeState> g_exchanges{
    300000u};
scratchbird::security::plugins::challenge::ReplayCache g_replay_cache{600000u};

thread_local std::string g_result_payload;
thread_local std::string g_health_payload;
thread_local std::string g_username_payload;

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

inline bool parseUInt32(std::string_view text, uint32_t* out_value) {
    if (!out_value || text.empty()) {
        return false;
    }
    uint64_t value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        value = value * 10u + static_cast<uint64_t>(ch - '0');
        if (value > 0xFFFFFFFFull) {
            return false;
        }
    }
    *out_value = static_cast<uint32_t>(value);
    return true;
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
            c == static_cast<uint8_t>(',');
        if (!allowed) {
            return false;
        }
    }

    return true;
}

inline bool parseKeyValuePayload(std::string_view payload,
                                 std::string* user,
                                 std::string* cnonce,
                                 std::string* proof,
                                 std::string* nonce) {
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
        if (key == "user" && user) {
            *user = value;
        } else if (key == "cnonce" && cnonce) {
            *cnonce = value;
        } else if (key == "proof" && proof) {
            *proof = value;
        } else if (key == "nonce" && nonce) {
            *nonce = value;
        }

        if (sep == std::string_view::npos) {
            break;
        }
        cursor = sep + 1;
    }
    return true;
}

inline bool parseClientFirst(sb_auth_slice_t payload, ClientFirst* out_first) {
    if (!out_first || !payloadLooksSafe(payload)) {
        return false;
    }

    ClientFirst parsed{};
    if (!parseKeyValuePayload(toString(payload), &parsed.username, &parsed.client_nonce, nullptr,
                              nullptr)) {
        return false;
    }

    if (parsed.username.empty() || parsed.client_nonce.empty()) {
        return false;
    }

    *out_first = std::move(parsed);
    return true;
}

inline bool parseClientFinal(sb_auth_slice_t payload, ClientFinal* out_final) {
    if (!out_final || !payloadLooksSafe(payload)) {
        return false;
    }

    ClientFinal parsed{};
    if (!parseKeyValuePayload(toString(payload), nullptr, nullptr, &parsed.proof,
                              &parsed.combined_nonce)) {
        return false;
    }

    if (parsed.proof.empty() || parsed.combined_nonce.empty()) {
        return false;
    }

    *out_final = std::move(parsed);
    return true;
}

inline std::string computeProof(const ScramExchangeState& state) {
    const std::string message = state.method + "|" + state.username + "|" + state.client_nonce +
                                "|" + state.server_nonce + "|" + state.credential.salt + "|" +
                                std::to_string(state.credential.iterations);
    std::string proof;
    if (!scratchbird::security::plugins::crypto::hmacSha256Hex(state.credential.secret,
                                                                message,
                                                                &proof)) {
        return "";
    }
    return proof;
}

inline bool textsEqual(std::string_view lhs, std::string_view rhs) {
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

inline std::string randomNonceHex() {
    std::array<uint8_t, 16> nonce{};
    const bool used_host_rng =
        g_host_api && g_host_api->secure_random &&
        g_host_api->secure_random(nonce.data(), static_cast<uint32_t>(nonce.size())) == SB_AUTH_RC_OK;
    if (!used_host_rng) {
        const uint64_t seed = g_next_exchange.load(std::memory_order_relaxed);
        for (std::size_t i = 0; i < nonce.size(); ++i) {
            nonce[i] = static_cast<uint8_t>(((seed >> ((i % 8) * 8)) & 0xFFu) ^
                                            static_cast<uint8_t>(0x5Au + i * 11u));
        }
    }

    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(nonce.size() * 2);
    for (std::size_t i = 0; i < nonce.size(); ++i) {
        out[2 * i] = kHex[(nonce[i] >> 4) & 0x0F];
        out[2 * i + 1] = kHex[nonce[i] & 0x0F];
    }
    return out;
}

inline bool parseCredentialPayload(std::string_view payload, ScramCredential* out_credential) {
    if (!out_credential || payload.empty()) {
        return false;
    }

    ScramCredential parsed{};

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
        if (key == "salt") {
            parsed.salt = value;
        } else if (key == "secret") {
            parsed.secret = value;
        } else if (key == "iter") {
            if (!parseUInt32(value, &parsed.iterations)) {
                return false;
            }
        }

        if (sep == std::string_view::npos) {
            break;
        }
        cursor = sep + 1;
    }

    if (parsed.salt.empty() || parsed.secret.empty() || parsed.iterations == 0) {
        return false;
    }

    *out_credential = std::move(parsed);
    return true;
}

inline bool loadScramCredentialForUser(const std::string& username,
                                       ScramCredential* out_credential) {
    if (!g_host_api || !g_host_api->read_policy_value || !out_credential || username.empty()) {
        return false;
    }

    const std::string user_key = std::string(kPolicyCredentialRefPrefix) + username;
    std::string credential_ref;

    sb_auth_slice_t value{};
    const sb_auth_slice_t key_slice{
        reinterpret_cast<const uint8_t*>(user_key.data()),
        static_cast<uint32_t>(user_key.size())
    };
    if (g_host_api->read_policy_value(key_slice, &value) == SB_AUTH_RC_OK && value.ptr &&
        value.len > 0) {
        credential_ref = toString(value);
    } else {
        const sb_auth_slice_t default_key_slice{
            reinterpret_cast<const uint8_t*>(kPolicyDefaultCredentialRef),
            static_cast<uint32_t>(std::strlen(kPolicyDefaultCredentialRef))
        };
        if (g_host_api->read_policy_value(default_key_slice, &value) != SB_AUTH_RC_OK || !value.ptr ||
            value.len == 0) {
            return false;
        }
        credential_ref = toString(value);
    }

    scratchbird::security::plugins::secrets::SecretMaterial credential_secret{};
    if (scratchbird::security::plugins::secrets::resolveSecretReference(
            g_host_api,
            credential_ref,
            &credential_secret) != scratchbird::security::plugins::secrets::SecretResolveStatus::OK) {
        return false;
    }

    return parseCredentialPayload(credential_secret.value, out_credential);
}

inline const char* resolveMethod(sb_auth_slice_t method_id) {
    if (sliceEqualsLiteral(method_id, kMethodScramSha256)) {
        return kMethodScramSha256;
    }
    if (sliceEqualsLiteral(method_id, kMethodScramSha512)) {
        return kMethodScramSha512;
    }
    return nullptr;
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

inline std::string exchangeReplayKey(sb_auth_exchange_t exchange) {
    return "scram:exchange:" + std::to_string(exchange);
}

inline std::string proofReplayKey(const std::string& proof) {
    return "scram:proof:" + proof;
}

inline sb_auth_rc_t createInstance(sb_auth_plugin_instance_t* out_instance) {
    if (!out_instance) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }
    *out_instance = g_next_instance.fetch_add(1, std::memory_order_relaxed);
    return SB_AUTH_RC_OK;
}

inline void destroyInstance(sb_auth_plugin_instance_t instance) {
    (void) g_exchanges.eraseIf([&](const ScramExchangeState& state) {
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
    auto finish = [&](sb_auth_rc_t rc, const char* method, const char* error_code) {
        scratchbird::security::plugins::observability::recordOutcome(g_counters, rc);
        scratchbird::security::plugins::observability::emitAuditEvent(
            g_host_api,
            "auth_plugin.scram.begin",
            kPluginId,
            method,
            rc,
            error_code);
        return rc;
    };

    if (!inout_exchange || !out_result) {
        fillDeniedResult(out_result, 5001, kErrBadRequest);
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, nullptr, kErrBadRequest);
    }

    const char* resolved_method = resolveMethod(method_id);
    if (!resolved_method) {
        *inout_exchange = 0;
        fillDeniedResult(out_result, 5002, kErrUnknownMethod);
        return finish(SB_AUTH_RC_UNSUPPORTED, nullptr, kErrUnknownMethod);
    }

    if (!client_payload.ptr || client_payload.len == 0) {
        *inout_exchange = 0;
        fillDeniedResult(out_result, 5003, kErrMissingClientFirst);
        return finish(SB_AUTH_RC_DENY, resolved_method, kErrMissingClientFirst);
    }

    ClientFirst first{};
    if (!parseClientFirst(client_payload, &first)) {
        *inout_exchange = 0;
        fillDeniedResult(out_result, 5004, kErrMalformedClientFirst);
        return finish(SB_AUTH_RC_DENY, resolved_method, kErrMalformedClientFirst);
    }

    ScramCredential credential{};
    if (!loadScramCredentialForUser(first.username, &credential)) {
        *inout_exchange = 0;
        fillDeniedResult(out_result, 5005, kErrCredentialUnavailable);
        return finish(SB_AUTH_RC_DENY, resolved_method, kErrCredentialUnavailable);
    }

    const sb_auth_exchange_t exchange_id =
        g_next_exchange.fetch_add(1, std::memory_order_relaxed);

    ScramExchangeState state{};
    state.instance = instance;
    state.method = resolved_method;
    state.username = first.username;
    state.client_nonce = first.client_nonce;
    state.server_nonce = randomNonceHex();
    state.credential = std::move(credential);

    g_exchanges.put(exchange_id, state);

    const std::string combined_nonce = state.client_nonce + state.server_nonce;
    const std::string challenge_payload =
        "snonce=" + state.server_nonce + ";salt=" + state.credential.salt +
        ";iter=" + std::to_string(state.credential.iterations) +
        ";nonce=" + combined_nonce;

    *inout_exchange = exchange_id;
    zeroAndPrimeResult(out_result);
    out_result->rc = SB_AUTH_RC_CONTINUE;
    copyCStr(out_result->sqlstate, sizeof(out_result->sqlstate), "00000");
    setPayload(out_result, challenge_payload);
    return finish(SB_AUTH_RC_CONTINUE, resolved_method, nullptr);
}

inline sb_auth_rc_t continueAuth(sb_auth_plugin_instance_t /*instance*/,
                                 sb_auth_exchange_t exchange,
                                 sb_auth_slice_t client_payload,
                                 sb_auth_step_result_v1* out_result) {
    auto finish = [&](sb_auth_rc_t rc, const char* method, const char* error_code) {
        scratchbird::security::plugins::observability::recordOutcome(g_counters, rc);
        scratchbird::security::plugins::observability::emitAuditEvent(
            g_host_api,
            "auth_plugin.scram.continue",
            kPluginId,
            method,
            rc,
            error_code);
        return rc;
    };

    if (!out_result) {
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, nullptr, kErrBadRequest);
    }

    ScramExchangeState state{};
    const auto take_status =
        g_exchanges.take(exchange, &state);
    if (take_status == scratchbird::security::plugins::challenge::ExchangeStore<
                           ScramExchangeState>::TakeStatus::Missing) {
        if (g_replay_cache.contains(exchangeReplayKey(exchange))) {
            fillDeniedResult(out_result, 5008, kErrReplayDetected);
            return finish(SB_AUTH_RC_DENY, nullptr, kErrReplayDetected);
        }
        fillDeniedResult(out_result, 5006, kErrUnknownExchange);
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, nullptr, kErrUnknownExchange);
    }
    if (take_status == scratchbird::security::plugins::challenge::ExchangeStore<
                           ScramExchangeState>::TakeStatus::Expired) {
        fillDeniedResult(out_result, 5009, kErrExchangeExpired);
        return finish(SB_AUTH_RC_DENY, nullptr, kErrExchangeExpired);
    }

    if (!client_payload.ptr || client_payload.len == 0) {
        fillDeniedResult(out_result, 5010, kErrMissingClientProof);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrMissingClientProof);
    }

    const std::string exchange_key = exchangeReplayKey(exchange);
    if (!g_replay_cache.remember(exchange_key)) {
        fillDeniedResult(out_result, 5008, kErrReplayDetected);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrReplayDetected);
    }

    ClientFinal final_message{};
    if (!parseClientFinal(client_payload, &final_message)) {
        fillDeniedResult(out_result, 5011, kErrMalformedClientProof);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrMalformedClientProof);
    }

    const std::string expected_nonce = state.client_nonce + state.server_nonce;
    if (final_message.combined_nonce != expected_nonce) {
        fillDeniedResult(out_result, 5012, kErrProofInvalid);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrProofInvalid);
    }

    const std::string expected_proof = computeProof(state);
    if (!textsEqual(final_message.proof, expected_proof)) {
        fillDeniedResult(out_result, 5012, kErrProofInvalid);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrProofInvalid);
    }

    if (!g_replay_cache.remember(proofReplayKey(final_message.proof))) {
        fillDeniedResult(out_result, 5008, kErrReplayDetected);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrReplayDetected);
    }

    if (!g_host_api || !g_host_api->resolve_user_by_name) {
        fillDeniedResult(out_result, 5013, kErrResolverUnavailable);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrResolverUnavailable);
    }

    g_username_payload = state.username;
    const sb_auth_slice_t username_slice{
        reinterpret_cast<const uint8_t*>(g_username_payload.data()),
        static_cast<uint32_t>(g_username_payload.size())
    };

    zeroAndPrimeResult(out_result);
    const sb_auth_rc_t resolve_rc =
        g_host_api->resolve_user_by_name(username_slice, out_result->principal.principal_uuid);
    if (resolve_rc != SB_AUTH_RC_OK) {
        fillDeniedResult(out_result, 5014, kErrUserUnknown);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrUserUnknown);
    }

    out_result->rc = SB_AUTH_RC_OK;
    out_result->principal.assurance_level = 60;
    out_result->principal.resolved_username = username_slice;
    copyCStr(out_result->sqlstate, sizeof(out_result->sqlstate), "00000");
    return finish(SB_AUTH_RC_OK, state.method.c_str(), nullptr);
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
        "{\"status\":\"ok\",\"plugin\":\"scram\",\"active_exchanges\":" +
        std::to_string(g_exchanges.size()) +
        ",\"replay_cache_entries\":" +
        std::to_string(g_replay_cache.size()) +
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
    static const std::array<sb_auth_method_descriptor_v1, 2> kMethods = [] {
        std::array<sb_auth_method_descriptor_v1, 2> methods{};

        std::memset(&methods[0], 0, sizeof(methods[0]));
        copyCStr(methods[0].method_id, sizeof(methods[0].method_id), kMethodScramSha256);
        methods[0].method_flags = 0;
        methods[0].legacy_wire_code = kLegacyWireUnknown;

        std::memset(&methods[1], 0, sizeof(methods[1]));
        copyCStr(methods[1].method_id, sizeof(methods[1].method_id), kMethodScramSha512);
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
