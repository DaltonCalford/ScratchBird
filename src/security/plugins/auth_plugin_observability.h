/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

#include "scratchbird/security/auth_plugin_abi_v1.h"

namespace scratchbird {
namespace security {
namespace plugins {
namespace observability {

struct OutcomeCounters {
    std::atomic<uint64_t> allow{0};
    std::atomic<uint64_t> deny{0};
    std::atomic<uint64_t> cont{0};
    std::atomic<uint64_t> error{0};
};

inline const char* rcToString(sb_auth_rc_t rc) {
    switch (rc) {
        case SB_AUTH_RC_OK:
            return "ok";
        case SB_AUTH_RC_CONTINUE:
            return "continue";
        case SB_AUTH_RC_DENY:
            return "deny";
        case SB_AUTH_RC_ERROR:
            return "error";
        case SB_AUTH_RC_UNSUPPORTED:
            return "unsupported";
        case SB_AUTH_RC_INVALID_ARGUMENT:
            return "invalid_argument";
        case SB_AUTH_RC_POLICY_VIOLATION:
            return "policy_violation";
        case SB_AUTH_RC_SIGNATURE_INVALID:
            return "signature_invalid";
        case SB_AUTH_RC_UNAUTHORIZED_PLUGIN:
            return "unauthorized_plugin";
        default:
            return "unknown";
    }
}

inline void recordOutcome(OutcomeCounters& counters, sb_auth_rc_t rc) {
    switch (rc) {
        case SB_AUTH_RC_OK:
            counters.allow.fetch_add(1, std::memory_order_relaxed);
            break;
        case SB_AUTH_RC_CONTINUE:
            counters.cont.fetch_add(1, std::memory_order_relaxed);
            break;
        case SB_AUTH_RC_DENY:
            counters.deny.fetch_add(1, std::memory_order_relaxed);
            break;
        default:
            counters.error.fetch_add(1, std::memory_order_relaxed);
            break;
    }
}

inline std::string escapeJson(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out.push_back(ch);
                break;
        }
    }
    return out;
}

inline void emitAuditEvent(const sb_auth_host_api_v1* host_api,
                           const char* event_name,
                           const char* plugin_id,
                           const char* method_id,
                           sb_auth_rc_t rc,
                           const char* error_code) {
    if (!host_api || !host_api->emit_audit_event || !event_name || !plugin_id) {
        return;
    }

    const std::string plugin = plugin_id;
    const std::string method = method_id ? method_id : "";
    const std::string code = error_code ? error_code : "";

    const std::string payload =
        std::string("{\"plugin_id\":\"") + escapeJson(plugin) +
        "\",\"method_id\":\"" + escapeJson(method) +
        "\",\"rc\":\"" + rcToString(rc) +
        "\",\"error_code\":\"" + escapeJson(code) + "\"}";

    const sb_auth_slice_t event_slice{
        reinterpret_cast<const uint8_t*>(event_name),
        static_cast<uint32_t>(std::strlen(event_name))
    };
    const sb_auth_slice_t json_slice{
        reinterpret_cast<const uint8_t*>(payload.data()),
        static_cast<uint32_t>(payload.size())
    };

    (void) host_api->emit_audit_event(event_slice, json_slice);
}

inline uint64_t loadCounter(const std::atomic<uint64_t>& value) {
    return value.load(std::memory_order_relaxed);
}

}  // namespace observability
}  // namespace plugins
}  // namespace security
}  // namespace scratchbird
