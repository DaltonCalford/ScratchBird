/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace scratchbird {
namespace security {
namespace plugins {
namespace kerberos {

enum class KerberosPluginConfigStatus : uint8_t {
    OK = 0,
    MISSING_REQUIRED_KEY,
    INVALID_VALUE,
};

enum class KerberosRuntimeProfile : uint8_t {
    PRODUCTION = 0,
    TEST = 1,
};

struct KerberosPluginConfig {
    std::string service_principal;
    std::string keytab_path;
    bool allow_delegation = false;
    uint32_t max_replay_window_ms = 30000;
    KerberosRuntimeProfile runtime_profile = KerberosRuntimeProfile::PRODUCTION;
    std::vector<std::string> allowed_kdc_endpoints;
};

KerberosPluginConfigStatus loadKerberosPluginConfig(
    const std::map<std::string, std::string>& values,
    KerberosPluginConfig& out,
    std::string* error_out = nullptr);

}  // namespace kerberos
}  // namespace plugins
}  // namespace security
}  // namespace scratchbird
