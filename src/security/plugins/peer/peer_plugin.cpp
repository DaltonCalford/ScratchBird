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

#include "scratchbird/security/auth_plugin_abi_v1.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace {

constexpr char kPluginId[] = "scratchbird.auth.peer";
constexpr char kPluginVersion[] = "1.0.0";
constexpr char kMethodPeerUid[] = "scratchbird.auth.peer_uid";
constexpr uint32_t kLegacyWireUnknown = 0xFFFFFFFFu;
constexpr char kPolicyAcceptIpc[] = "auth.peer.accept_ipc";
constexpr char kPolicyAllowUidZero[] = "auth.peer.allow_uid_zero";

constexpr char kErrBadRequest[] = "AUTH_PEER_BAD_REQUEST";
constexpr char kErrUnknownMethod[] = "AUTH_PEER_METHOD_UNKNOWN";
constexpr char kErrRemoteTransport[] = "AUTH_PEER_REMOTE_TRANSPORT";
constexpr char kErrIpcDisabled[] = "AUTH_PEER_IPC_DISABLED";
constexpr char kErrRootUidDenied[] = "AUTH_PEER_UID_ZERO_DENIED";
constexpr char kErrPeerPidMissing[] = "AUTH_PEER_PID_MISSING";
constexpr char kErrNoContinuation[] = "AUTH_PEER_NO_CONTINUE";
constexpr char kErrPayloadNotEmpty[] = "AUTH_PEER_PAYLOAD_NOT_EMPTY";

const sb_auth_host_api_v1* g_host_api = nullptr;
std::atomic<uint64_t> g_next_instance{1};
scratchbird::security::plugins::observability::OutcomeCounters g_counters{};
thread_local std::string g_health_payload;

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

inline bool parsePolicyBoolean(const std::string& value, bool* out_bool) {
    if (!out_bool) {
        return false;
    }

    std::string normalized;
    normalized.reserve(value.size());
    for (char ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            normalized.push_back(static_cast<char>(ch - 'A' + 'a'));
        } else if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
            normalized.push_back(ch);
        }
    }

    if (normalized == "1" || normalized == "true" || normalized == "yes" ||
        normalized == "on") {
        *out_bool = true;
        return true;
    }
    if (normalized == "0" || normalized == "false" || normalized == "no" ||
        normalized == "off") {
        *out_bool = false;
        return true;
    }
    return false;
}

inline bool readBooleanPolicy(const sb_auth_host_api_v1* host_api,
                              const char* key_text,
                              bool default_value) {
    if (!host_api || !host_api->read_policy_value || !key_text) {
        return default_value;
    }

    const sb_auth_slice_t key{
        reinterpret_cast<const uint8_t*>(key_text),
        static_cast<uint32_t>(std::strlen(key_text))
    };
    sb_auth_slice_t value{};
    if (host_api->read_policy_value(key, &value) != SB_AUTH_RC_OK || !value.ptr ||
        value.len == 0) {
        return default_value;
    }

    const std::string text(reinterpret_cast<const char*>(value.ptr),
                           reinterpret_cast<const char*>(value.ptr) + value.len);
    bool parsed = default_value;
    if (!parsePolicyBoolean(text, &parsed)) {
        return default_value;
    }
    return parsed;
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
    auto finish = [&](sb_auth_rc_t rc, const char* error_code) {
        scratchbird::security::plugins::observability::recordOutcome(g_counters, rc);
        scratchbird::security::plugins::observability::emitAuditEvent(
            g_host_api,
            "auth_plugin.peer.begin",
            kPluginId,
            kMethodPeerUid,
            rc,
            error_code);
        return rc;
    };

    if (!inout_exchange || !out_result || !conn) {
        fillDeniedResult(out_result, 2001, kErrBadRequest);
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, kErrBadRequest);
    }
    *inout_exchange = 0;

    if (!sliceEqualsLiteral(method_id, kMethodPeerUid)) {
        fillDeniedResult(out_result, 2002, kErrUnknownMethod);
        return finish(SB_AUTH_RC_UNSUPPORTED, kErrUnknownMethod);
    }

    if (client_payload.len != 0) {
        fillDeniedResult(out_result, 2007, kErrPayloadNotEmpty);
        return finish(SB_AUTH_RC_INVALID_ARGUMENT, kErrPayloadNotEmpty);
    }

    if (conn->transport == SB_AUTH_TRANSPORT_INET) {
        fillDeniedResult(out_result, 2003, kErrRemoteTransport);
        return finish(SB_AUTH_RC_DENY, kErrRemoteTransport);
    }

    if (conn->transport == SB_AUTH_TRANSPORT_IPC &&
        !readBooleanPolicy(g_host_api, kPolicyAcceptIpc, true)) {
        fillDeniedResult(out_result, 2006, kErrIpcDisabled);
        return finish(SB_AUTH_RC_DENY, kErrIpcDisabled);
    }

    if (conn->peer_uid == 0) {
        if (!readBooleanPolicy(g_host_api, kPolicyAllowUidZero, false)) {
            fillDeniedResult(out_result, 2004, kErrRootUidDenied);
            return finish(SB_AUTH_RC_DENY, kErrRootUidDenied);
        }
    }

    if (conn->peer_pid == 0) {
        fillDeniedResult(out_result, 2005, kErrPeerPidMissing);
        return finish(SB_AUTH_RC_DENY, kErrPeerPidMissing);
    }

    zeroAndPrimeResult(out_result);
    out_result->rc = SB_AUTH_RC_OK;
    out_result->principal.assurance_level = 30;
    copyCStr(out_result->sqlstate, sizeof(out_result->sqlstate), "00000");

    if (conn->username.ptr && conn->username.len > 0) {
        out_result->principal.resolved_username = conn->username;
        if (g_host_api && g_host_api->resolve_user_by_name) {
            (void) g_host_api->resolve_user_by_name(conn->username,
                                                    out_result->principal.principal_uuid);
        }
    }

    return finish(SB_AUTH_RC_OK, nullptr);
}

inline sb_auth_rc_t continueAuth(sb_auth_plugin_instance_t /*instance*/,
                                 sb_auth_exchange_t /*exchange*/,
                                 sb_auth_slice_t /*client_payload*/,
                                 sb_auth_step_result_v1* out_result) {
    fillDeniedResult(out_result, 2005, kErrNoContinuation);
    scratchbird::security::plugins::observability::recordOutcome(g_counters, SB_AUTH_RC_DENY);
    scratchbird::security::plugins::observability::emitAuditEvent(
        g_host_api,
        "auth_plugin.peer.continue",
        kPluginId,
        kMethodPeerUid,
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
    g_health_payload = "{\"status\":\"ok\",\"plugin\":\"peer\",\"allow_count\":" +
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
        copyCStr(methods[0].method_id, sizeof(methods[0].method_id), kMethodPeerUid);
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
