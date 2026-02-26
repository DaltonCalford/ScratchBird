/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "scratchbird/security/auth_plugin_stub_runtime.h"

namespace {

constexpr scratchbird::security::plugins::stub::StaticPluginDefinition<1> kPluginDefinition{
    "scratchbird.auth.radius",
    "2.0.0",
    {"scratchbird.auth.radius_pap"}
};

}  // namespace

extern "C" sb_auth_rc_t sb_auth_plugin_get_api_v1(
    uint32_t requested_abi_major,
    const sb_auth_host_api_v1* host_api,
    const sb_auth_plugin_descriptor_v1** out_descriptor,
    const sb_auth_plugin_api_v1** out_api) {
    return scratchbird::security::plugins::stub::exportStubPlugin(
        kPluginDefinition,
        requested_abi_major,
        host_api,
        out_descriptor,
        out_api);
}
