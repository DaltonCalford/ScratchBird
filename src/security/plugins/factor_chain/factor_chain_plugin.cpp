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
#include <vector>

namespace {

constexpr char kPluginId[] = "scratchbird.auth.factor_chain";
constexpr char kPluginVersion[] = "2.0.0";
constexpr char kMethodFactorChain2fa[] = "scratchbird.auth.factor_chain_2fa";
constexpr char kMethodFactorChain3fa[] = "scratchbird.auth.factor_chain_3fa";
constexpr char kIssuerFactorChain[] = "factor_chain";
constexpr uint32_t kLegacyWireUnknown = 0xFFFFFFFFu;
constexpr uint32_t kMaxPayloadBytes = 2048;

constexpr char kPolicySigningKeyRef[] = "policy:auth.factor_chain.signing_key_ref";
constexpr char kPolicySequence2fa[] = "auth.factor_chain.2fa.sequence";
constexpr char kPolicySequence3fa[] = "auth.factor_chain.3fa.sequence";
constexpr char kPolicyRequireDistinct[] = "auth.factor_chain.require_distinct_factors";

constexpr char kErrBadRequest[] = "AUTH_FACTOR_CHAIN_BAD_REQUEST";
constexpr char kErrUnknownMethod[] = "AUTH_FACTOR_CHAIN_METHOD_UNKNOWN";
constexpr char kErrMissingBeginPayload[] = "AUTH_FACTOR_CHAIN_BEGIN_MISSING";
constexpr char kErrMalformedBeginPayload[] = "AUTH_FACTOR_CHAIN_BEGIN_MALFORMED";
constexpr char kErrMissingFactor[] = "AUTH_FACTOR_CHAIN_FACTOR_MISSING";
constexpr char kErrMalformedFactor[] = "AUTH_FACTOR_CHAIN_FACTOR_MALFORMED";
constexpr char kErrUnknownExchange[] = "AUTH_FACTOR_CHAIN_UNKNOWN_EXCHANGE";
constexpr char kErrExchangeExpired[] = "AUTH_FACTOR_CHAIN_EXCHANGE_EXPIRED";
constexpr char kErrReplayDetected[] = "AUTH_FACTOR_CHAIN_REPLAY_DETECTED";
constexpr char kErrPolicyInvalid[] = "AUTH_FACTOR_CHAIN_POLICY_INVALID";
constexpr char kErrFactorOrder[] = "AUTH_FACTOR_CHAIN_FACTOR_ORDER";
constexpr char kErrFactorRejected[] = "AUTH_FACTOR_CHAIN_FACTOR_REJECTED";
constexpr char kErrSubjectMismatch[] = "AUTH_FACTOR_CHAIN_SUBJECT_MISMATCH";
constexpr char kErrNonceMismatch[] = "AUTH_FACTOR_CHAIN_NONCE_MISMATCH";
constexpr char kErrProofInvalid[] = "AUTH_FACTOR_CHAIN_PROOF_INVALID";
constexpr char kErrSigningKeyUnavailable[] = "AUTH_FACTOR_CHAIN_SIGNING_KEY_UNAVAILABLE";
constexpr char kErrResolverUnavailable[] = "AUTH_FACTOR_CHAIN_RESOLVER_UNAVAILABLE";
constexpr char kErrSubjectUnknown[] = "AUTH_FACTOR_CHAIN_SUBJECT_UNKNOWN";

const sb_auth_host_api_v1* g_host_api = nullptr;
std::atomic<uint64_t> g_next_instance{1};
std::atomic<uint64_t> g_next_exchange{1};
scratchbird::security::plugins::observability::OutcomeCounters g_counters{};

struct FactorExchangeState {
    sb_auth_plugin_instance_t instance = 0;
    std::string method;
    std::string username;
    std::string subject;
    std::string nonce;
    std::vector<std::string> factor_sequence;
    uint8_t completed_factors = 0;
};

struct BeginRequest {
    std::string username;
    std::string subject;
};

struct FactorSubmission {
    std::string factor;
    std::string status;
    std::string subject;
    std::string nonce;
    std::string proof;
};

scratchbird::security::plugins::challenge::ExchangeStore<FactorExchangeState> g_exchanges{
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
            c == static_cast<uint8_t>(',') || c == static_cast<uint8_t>('@');
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
        } else if (key == "sub") {
            parsed.subject = value;
        }

        if (sep == std::string_view::npos) {
            break;
        }
        cursor = sep + 1;
    }

    if (parsed.username.empty() || parsed.subject.empty()) {
        return false;
    }

    *out_request = std::move(parsed);
    return true;
}

inline bool parseFactorSubmission(std::string_view payload, FactorSubmission* out_submission) {
    if (!out_submission || payload.empty()) {
        return false;
    }

    FactorSubmission parsed{};

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

        if (key == "factor") {
            parsed.factor = value;
        } else if (key == "status") {
            parsed.status = value;
        } else if (key == "sub") {
            parsed.subject = value;
        } else if (key == "nonce") {
            parsed.nonce = value;
        } else if (key == "proof") {
            parsed.proof = value;
        }

        if (sep == std::string_view::npos) {
            break;
        }
        cursor = sep + 1;
    }

    if (parsed.factor.empty() || parsed.status.empty() || parsed.subject.empty() ||
        parsed.nonce.empty() || parsed.proof.empty()) {
        return false;
    }

    *out_submission = std::move(parsed);
    return true;
}

inline std::string computeFactorProof(const FactorExchangeState& state,
                                      std::string_view factor,
                                      uint8_t stage_index,
                                      const std::string& signing_key) {
    const std::string payload = state.method + "|" + state.subject + "|" +
                                std::string(factor) + "|" +
                                std::to_string(static_cast<unsigned>(stage_index)) + "|" +
                                state.nonce;
    std::string proof;
    if (!scratchbird::security::plugins::crypto::hmacSha256Hex(signing_key, payload, &proof)) {
        return "";
    }
    return proof;
}

inline bool proofsEqual(std::string_view lhs, std::string_view rhs) {
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

inline bool splitCsvList(std::string_view text,
                         std::vector<std::string>* out_values) {
    if (!out_values) {
        return false;
    }
    out_values->clear();
    if (text.empty()) {
        return false;
    }

    std::size_t cursor = 0;
    while (cursor <= text.size()) {
        const std::size_t comma = text.find(',', cursor);
        const std::size_t end = (comma == std::string_view::npos) ? text.size() : comma;
        const std::string item = trimAscii(text.substr(cursor, end - cursor));
        if (item.empty()) {
            return false;
        }
        out_values->push_back(item);
        if (comma == std::string_view::npos) {
            break;
        }
        cursor = comma + 1;
    }

    return !out_values->empty();
}

inline bool hasDuplicates(const std::vector<std::string>& values) {
    for (std::size_t i = 0; i < values.size(); ++i) {
        for (std::size_t j = i + 1; j < values.size(); ++j) {
            if (values[i] == values[j]) {
                return true;
            }
        }
    }
    return false;
}

inline bool resolveFactorSequence(const char* method,
                                  uint8_t required_factors,
                                  std::vector<std::string>* out_sequence) {
    if (!method || !out_sequence || required_factors == 0) {
        return false;
    }

    std::vector<std::string> sequence;
    const char* policy_key = (required_factors == 2u) ? kPolicySequence2fa : kPolicySequence3fa;
    std::string sequence_policy;
    if (loadPolicyString(policy_key, &sequence_policy)) {
        if (!splitCsvList(sequence_policy, &sequence)) {
            return false;
        }
    } else {
        if (required_factors == 2u) {
            sequence = {"password", "otp"};
        } else if (required_factors == 3u) {
            sequence = {"password", "otp", "webauthn"};
        } else {
            return false;
        }
    }

    if (sequence.size() != required_factors) {
        return false;
    }

    const bool require_distinct = loadPolicyBool(kPolicyRequireDistinct, true);
    if (require_distinct && hasDuplicates(sequence)) {
        return false;
    }

    *out_sequence = std::move(sequence);
    return true;
}

inline const char* resolveMethod(sb_auth_slice_t method_id, uint8_t* out_required_factors) {
    if (!out_required_factors) {
        return nullptr;
    }
    if (sliceEqualsLiteral(method_id, kMethodFactorChain2fa)) {
        *out_required_factors = 2;
        return kMethodFactorChain2fa;
    }
    if (sliceEqualsLiteral(method_id, kMethodFactorChain3fa)) {
        *out_required_factors = 3;
        return kMethodFactorChain3fa;
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

inline std::string randomNonceHex() {
    std::array<uint8_t, 16> nonce{};
    const bool used_host_rng =
        g_host_api && g_host_api->secure_random &&
        g_host_api->secure_random(nonce.data(), static_cast<uint32_t>(nonce.size())) == SB_AUTH_RC_OK;
    if (!used_host_rng) {
        const uint64_t seed = g_next_exchange.load(std::memory_order_relaxed);
        for (std::size_t i = 0; i < nonce.size(); ++i) {
            nonce[i] = static_cast<uint8_t>(((seed >> ((i % 8) * 8)) & 0xFFu) ^
                                            static_cast<uint8_t>(0x3Bu + i * 13u));
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

inline std::string exchangeReplayKey(sb_auth_exchange_t exchange) {
    return "factor_chain:exchange:" + std::to_string(exchange);
}

inline std::string proofReplayKey(std::string_view proof) {
    return "factor_chain:proof:" + std::string(proof);
}

inline std::string buildProgressPayload(uint8_t completed_factors,
                                        uint8_t required_factors,
                                        sb_auth_exchange_t exchange,
                                        std::string_view next_factor,
                                        std::string_view nonce) {
    return "type=factor_challenge;exchange=" + std::to_string(exchange) +
           ";completed=" + std::to_string(completed_factors) +
           ";required=" + std::to_string(required_factors) +
           ";next=" + std::string(next_factor) +
           ";nonce=" + std::string(nonce);
}

inline void markExchangeTerminal(sb_auth_exchange_t exchange) {
    (void) g_replay_cache.remember(exchangeReplayKey(exchange));
}

inline sb_auth_rc_t createInstance(sb_auth_plugin_instance_t* out_instance) {
    if (!out_instance) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }
    *out_instance = g_next_instance.fetch_add(1, std::memory_order_relaxed);
    return SB_AUTH_RC_OK;
}

inline void destroyInstance(sb_auth_plugin_instance_t instance) {
    (void) g_exchanges.eraseIf([&](const FactorExchangeState& state) {
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
            "auth_plugin.factor_chain.begin",
            kPluginId,
            method,
            rc,
            error_code);
        return rc;
    };

    if (!inout_exchange || !out_result) {
        fillDeniedResult(out_result, 11001, kErrBadRequest);
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, nullptr, kErrBadRequest);
    }
    *inout_exchange = 0;

    uint8_t required_factors = 0;
    const char* resolved_method = resolveMethod(method_id, &required_factors);
    if (!resolved_method || required_factors == 0) {
        fillDeniedResult(out_result, 11002, kErrUnknownMethod);
        return finish(SB_AUTH_RC_UNSUPPORTED, nullptr, kErrUnknownMethod);
    }

    if (!client_payload.ptr || client_payload.len == 0) {
        fillDeniedResult(out_result, 11003, kErrMissingBeginPayload);
        return finish(SB_AUTH_RC_DENY, resolved_method, kErrMissingBeginPayload);
    }
    if (!payloadLooksSafe(client_payload)) {
        fillDeniedResult(out_result, 11004, kErrMalformedBeginPayload);
        return finish(SB_AUTH_RC_DENY, resolved_method, kErrMalformedBeginPayload);
    }

    BeginRequest begin_request{};
    if (!parseBeginRequest(toString(client_payload), &begin_request)) {
        fillDeniedResult(out_result, 11004, kErrMalformedBeginPayload);
        return finish(SB_AUTH_RC_DENY, resolved_method, kErrMalformedBeginPayload);
    }

    std::vector<std::string> factor_sequence;
    if (!resolveFactorSequence(resolved_method, required_factors, &factor_sequence)) {
        fillDeniedResult(out_result, 11010, kErrPolicyInvalid);
        return finish(SB_AUTH_RC_DENY, resolved_method, kErrPolicyInvalid);
    }

    const sb_auth_exchange_t exchange_id = g_next_exchange.fetch_add(1, std::memory_order_relaxed);

    FactorExchangeState state{};
    state.instance = instance;
    state.method = resolved_method;
    state.username = begin_request.username;
    state.subject = begin_request.subject;
    state.nonce = randomNonceHex();
    state.factor_sequence = std::move(factor_sequence);
    state.completed_factors = 0;

    g_exchanges.put(exchange_id, state);

    *inout_exchange = exchange_id;
    zeroAndPrimeResult(out_result);
    out_result->rc = SB_AUTH_RC_CONTINUE;
    copyCStr(out_result->sqlstate, sizeof(out_result->sqlstate), "00000");
    setPayload(out_result,
               buildProgressPayload(0,
                                    static_cast<uint8_t>(state.factor_sequence.size()),
                                    exchange_id,
                                    state.factor_sequence[0],
                                    state.nonce));
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
            "auth_plugin.factor_chain.continue",
            kPluginId,
            method,
            rc,
            error_code);
        return rc;
    };

    if (!out_result) {
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, nullptr, kErrBadRequest);
    }

    if (!client_payload.ptr || client_payload.len == 0) {
        fillDeniedResult(out_result, 11005, kErrMissingFactor);
        return finish(SB_AUTH_RC_DENY, nullptr, kErrMissingFactor);
    }
    if (!payloadLooksSafe(client_payload)) {
        fillDeniedResult(out_result, 11006, kErrMalformedFactor);
        return finish(SB_AUTH_RC_DENY, nullptr, kErrMalformedFactor);
    }

    FactorExchangeState state{};
    const auto take_status = g_exchanges.take(exchange, &state);
    if (take_status == scratchbird::security::plugins::challenge::ExchangeStore<
                           FactorExchangeState>::TakeStatus::Missing) {
        if (g_replay_cache.contains(exchangeReplayKey(exchange))) {
            fillDeniedResult(out_result, 11009, kErrReplayDetected);
            return finish(SB_AUTH_RC_DENY, nullptr, kErrReplayDetected);
        }
        fillDeniedResult(out_result, 11007, kErrUnknownExchange);
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, nullptr, kErrUnknownExchange);
    }
    if (take_status == scratchbird::security::plugins::challenge::ExchangeStore<
                           FactorExchangeState>::TakeStatus::Expired) {
        fillDeniedResult(out_result, 11008, kErrExchangeExpired);
        markExchangeTerminal(exchange);
        return finish(SB_AUTH_RC_DENY, nullptr, kErrExchangeExpired);
    }

    FactorSubmission submission{};
    if (!parseFactorSubmission(toString(client_payload), &submission)) {
        fillDeniedResult(out_result, 11006, kErrMalformedFactor);
        markExchangeTerminal(exchange);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrMalformedFactor);
    }

    if (submission.subject != state.subject) {
        fillDeniedResult(out_result, 11013, kErrSubjectMismatch);
        markExchangeTerminal(exchange);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrSubjectMismatch);
    }

    if (submission.nonce != state.nonce) {
        fillDeniedResult(out_result, 11014, kErrNonceMismatch);
        markExchangeTerminal(exchange);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrNonceMismatch);
    }

    const std::size_t stage_index = static_cast<std::size_t>(state.completed_factors);
    if (stage_index >= state.factor_sequence.size()) {
        fillDeniedResult(out_result, 11010, kErrPolicyInvalid);
        markExchangeTerminal(exchange);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrPolicyInvalid);
    }

    const std::string& expected_factor = state.factor_sequence[stage_index];
    if (submission.factor != expected_factor) {
        fillDeniedResult(out_result, 11011, kErrFactorOrder);
        markExchangeTerminal(exchange);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrFactorOrder);
    }

    if (submission.status != "ok") {
        fillDeniedResult(out_result, 11012, kErrFactorRejected);
        markExchangeTerminal(exchange);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrFactorRejected);
    }

    scratchbird::security::plugins::secrets::SecretMaterial signing_key{};
    if (scratchbird::security::plugins::secrets::resolveSecretReference(
            g_host_api,
            kPolicySigningKeyRef,
            &signing_key) != scratchbird::security::plugins::secrets::SecretResolveStatus::OK) {
        fillDeniedResult(out_result, 11016, kErrSigningKeyUnavailable);
        markExchangeTerminal(exchange);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrSigningKeyUnavailable);
    }

    const std::string expected_proof = computeFactorProof(
        state,
        submission.factor,
        static_cast<uint8_t>(stage_index + 1),
        signing_key.value);
    if (!proofsEqual(submission.proof, expected_proof)) {
        fillDeniedResult(out_result, 11015, kErrProofInvalid);
        markExchangeTerminal(exchange);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrProofInvalid);
    }

    if (!g_replay_cache.remember(proofReplayKey(submission.proof))) {
        fillDeniedResult(out_result, 11009, kErrReplayDetected);
        markExchangeTerminal(exchange);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrReplayDetected);
    }

    state.completed_factors = static_cast<uint8_t>(stage_index + 1);
    if (state.completed_factors < state.factor_sequence.size()) {
        state.nonce = randomNonceHex();
        g_exchanges.put(exchange, state);

        zeroAndPrimeResult(out_result);
        out_result->rc = SB_AUTH_RC_CONTINUE;
        copyCStr(out_result->sqlstate, sizeof(out_result->sqlstate), "00000");
        setPayload(out_result,
                   buildProgressPayload(state.completed_factors,
                                        static_cast<uint8_t>(state.factor_sequence.size()),
                                        exchange,
                                        state.factor_sequence[state.completed_factors],
                                        state.nonce));
        return finish(SB_AUTH_RC_CONTINUE, state.method.c_str(), nullptr);
    }

    if (!g_host_api) {
        fillDeniedResult(out_result, 11017, kErrResolverUnavailable);
        markExchangeTerminal(exchange);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrResolverUnavailable);
    }

    zeroAndPrimeResult(out_result);
    sb_auth_rc_t resolve_rc = SB_AUTH_RC_DENY;
    if (g_host_api->resolve_user_by_external_subject) {
        const sb_auth_slice_t issuer_slice{
            reinterpret_cast<const uint8_t*>(kIssuerFactorChain),
            static_cast<uint32_t>(std::strlen(kIssuerFactorChain))
        };
        const sb_auth_slice_t subject_slice{
            reinterpret_cast<const uint8_t*>(state.subject.data()),
            static_cast<uint32_t>(state.subject.size())
        };
        resolve_rc = g_host_api->resolve_user_by_external_subject(
            issuer_slice,
            subject_slice,
            out_result->principal.principal_uuid);
    } else if (g_host_api->resolve_user_by_name && !state.username.empty()) {
        const sb_auth_slice_t username_slice{
            reinterpret_cast<const uint8_t*>(state.username.data()),
            static_cast<uint32_t>(state.username.size())
        };
        resolve_rc = g_host_api->resolve_user_by_name(
            username_slice,
            out_result->principal.principal_uuid);
    } else {
        fillDeniedResult(out_result, 11017, kErrResolverUnavailable);
        markExchangeTerminal(exchange);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrResolverUnavailable);
    }

    if (resolve_rc != SB_AUTH_RC_OK) {
        fillDeniedResult(out_result, 11018, kErrSubjectUnknown);
        markExchangeTerminal(exchange);
        return finish(SB_AUTH_RC_DENY, state.method.c_str(), kErrSubjectUnknown);
    }

    out_result->rc = SB_AUTH_RC_OK;
    out_result->principal.assurance_level =
        static_cast<uint16_t>(65u + static_cast<uint16_t>(state.factor_sequence.size() * 10u));

    g_username_payload = state.username;
    out_result->principal.resolved_username = sb_auth_slice_t{
        reinterpret_cast<const uint8_t*>(g_username_payload.data()),
        static_cast<uint32_t>(g_username_payload.size())
    };

    g_external_subject_payload = state.subject;
    out_result->principal.external_subject = sb_auth_slice_t{
        reinterpret_cast<const uint8_t*>(g_external_subject_payload.data()),
        static_cast<uint32_t>(g_external_subject_payload.size())
    };

    copyCStr(out_result->sqlstate, sizeof(out_result->sqlstate), "00000");
    markExchangeTerminal(exchange);
    return finish(SB_AUTH_RC_OK, state.method.c_str(), nullptr);
}

inline void abortAuth(sb_auth_plugin_instance_t /*instance*/, sb_auth_exchange_t exchange) {
    (void) g_exchanges.erase(exchange);
    markExchangeTerminal(exchange);
}

inline sb_auth_rc_t healthCheck(sb_auth_plugin_instance_t /*instance*/,
                                sb_auth_slice_t* out_json) {
    if (!out_json) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }

    g_health_payload =
        "{\"status\":\"ok\",\"plugin\":\"factor_chain\",\"active_exchanges\":" +
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
    static const std::array<sb_auth_method_descriptor_v1, 2> kMethods = [] {
        std::array<sb_auth_method_descriptor_v1, 2> methods{};

        std::memset(&methods[0], 0, sizeof(methods[0]));
        copyCStr(methods[0].method_id, sizeof(methods[0].method_id), kMethodFactorChain2fa);
        methods[0].method_flags = 0;
        methods[0].legacy_wire_code = kLegacyWireUnknown;

        std::memset(&methods[1], 0, sizeof(methods[1]));
        copyCStr(methods[1].method_id, sizeof(methods[1].method_id), kMethodFactorChain3fa);
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
