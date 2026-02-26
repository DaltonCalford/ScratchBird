/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "scratchbird/security/auth_plugin_abi_v1.h"

namespace scratchbird {
namespace security {
namespace plugins {
namespace stub {

template <std::size_t N>
struct StaticPluginDefinition {
    const char* plugin_id;
    const char* plugin_version;
    std::array<const char*, N> method_ids;
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

inline void fillDeniedResult(sb_auth_step_result_v1* out_result,
                             const char* error_code) {
    if (!out_result) {
        return;
    }
    std::memset(out_result, 0, sizeof(*out_result));
    out_result->struct_size = sizeof(sb_auth_step_result_v1);
    out_result->rc = SB_AUTH_RC_DENY;
    out_result->plugin_error_numeric = 1;
    copyCStr(out_result->plugin_error_code,
             sizeof(out_result->plugin_error_code),
             error_code);
    copyCStr(out_result->sqlstate, sizeof(out_result->sqlstate), "28000");
}

inline sb_auth_rc_t createInstance(sb_auth_plugin_instance_t* out_instance) {
    if (!out_instance) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }
    *out_instance = 1;
    return SB_AUTH_RC_OK;
}

inline void destroyInstance(sb_auth_plugin_instance_t /*instance*/) {}

inline sb_auth_rc_t configureInstance(sb_auth_plugin_instance_t /*instance*/,
                                      sb_auth_slice_t /*config_json*/) {
    return SB_AUTH_RC_OK;
}

inline sb_auth_rc_t beginAuth(sb_auth_plugin_instance_t /*instance*/,
                              sb_auth_slice_t /*method_id*/,
                              const sb_auth_connection_ctx_v1* /*conn*/,
                              sb_auth_slice_t /*client_payload*/,
                              sb_auth_exchange_t* /*inout_exchange*/,
                              sb_auth_step_result_v1* out_result) {
    fillDeniedResult(out_result, "AUTH_PLUGIN_STUB_DENY");
    return SB_AUTH_RC_DENY;
}

inline sb_auth_rc_t continueAuth(sb_auth_plugin_instance_t /*instance*/,
                                 sb_auth_exchange_t /*exchange*/,
                                 sb_auth_slice_t /*client_payload*/,
                                 sb_auth_step_result_v1* out_result) {
    fillDeniedResult(out_result, "AUTH_PLUGIN_STUB_DENY");
    return SB_AUTH_RC_DENY;
}

inline void abortAuth(sb_auth_plugin_instance_t /*instance*/,
                      sb_auth_exchange_t /*exchange*/) {}

inline sb_auth_rc_t healthCheck(sb_auth_plugin_instance_t /*instance*/,
                                sb_auth_slice_t* out_json) {
    if (!out_json) {
        return SB_AUTH_RC_INVALID_ARGUMENT;
    }
    static const char kHealth[] = "{\"status\":\"stub\"}";
    out_json->ptr = reinterpret_cast<const uint8_t*>(kHealth);
    out_json->len = static_cast<uint32_t>(sizeof(kHealth) - 1);
    return SB_AUTH_RC_OK;
}

template <std::size_t N>
inline const sb_auth_method_descriptor_v1* buildMethodDescriptors(
    const StaticPluginDefinition<N>& definition) {
    static std::array<sb_auth_method_descriptor_v1, N> descriptors{};
    static bool initialized = false;
    if (!initialized) {
        for (std::size_t i = 0; i < N; ++i) {
            std::memset(&descriptors[i], 0, sizeof(descriptors[i]));
            copyCStr(descriptors[i].method_id,
                     sizeof(descriptors[i].method_id),
                     definition.method_ids[i]);
            descriptors[i].method_flags = 0;
            descriptors[i].legacy_wire_code = 0xFFFFFFFFu;
        }
        initialized = true;
    }
    return descriptors.data();
}

template <std::size_t N>
inline const sb_auth_plugin_descriptor_v1* buildDescriptor(
    const StaticPluginDefinition<N>& definition) {
    static sb_auth_plugin_descriptor_v1 descriptor{};
    static sb_auth_slice_t version_slice{};
    static bool initialized = false;
    if (!initialized) {
        std::memset(&descriptor, 0, sizeof(descriptor));
        descriptor.struct_size = sizeof(sb_auth_plugin_descriptor_v1);
        copyCStr(descriptor.plugin_id,
                 sizeof(descriptor.plugin_id),
                 definition.plugin_id);
        version_slice.ptr = reinterpret_cast<const uint8_t*>(
            definition.plugin_version ? definition.plugin_version : "");
        version_slice.len = static_cast<uint32_t>(std::strlen(
            definition.plugin_version ? definition.plugin_version : ""));
        descriptor.plugin_version = version_slice;
        descriptor.abi_major = SB_AUTH_ABI_MAJOR;
        descriptor.abi_minor = SB_AUTH_ABI_MINOR;
        descriptor.method_count = static_cast<uint32_t>(N);
        descriptor.methods = buildMethodDescriptors(definition);
        initialized = true;
    }
    return &descriptor;
}

template <std::size_t N>
inline sb_auth_rc_t exportStubPlugin(
    const StaticPluginDefinition<N>& definition,
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

    static const sb_auth_plugin_api_v1 api = {
        sizeof(sb_auth_plugin_api_v1),
        &createInstance,
        &destroyInstance,
        &configureInstance,
        &beginAuth,
        &continueAuth,
        &abortAuth,
        &healthCheck,
    };

    *out_descriptor = buildDescriptor(definition);
    *out_api = &api;
    return SB_AUTH_RC_OK;
}

}  // namespace stub
}  // namespace plugins
}  // namespace security
}  // namespace scratchbird
