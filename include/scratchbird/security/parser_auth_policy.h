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

#include "scratchbird/protocol/wire_protocol.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace scratchbird::security {

inline std::string toLowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

inline std::string normalizeParserAuthSurface(std::string surface) {
    surface = toLowerAscii(std::move(surface));
    if (surface == "postgres" || surface == "postgresql") {
        return "postgresql";
    }
    if (surface == "mysql" || surface == "mariadb") {
        return "mysql";
    }
    if (surface == "firebird" || surface == "fb") {
        return "firebird";
    }
    return "native";
}

inline std::vector<protocol::AuthMethod> parserAuthMethodOrder(const std::string& surface_name) {
    const std::string surface = normalizeParserAuthSurface(surface_name);
    if (surface == "postgresql") {
        return {
            protocol::AuthMethod::SCRAM_SHA_256,
            protocol::AuthMethod::MD5,
            protocol::AuthMethod::PASSWORD,
            protocol::AuthMethod::SCRAM_SHA_512,
        };
    }
    if (surface == "mysql") {
        return {
            protocol::AuthMethod::PASSWORD,
            protocol::AuthMethod::SCRAM_SHA_256,
            protocol::AuthMethod::SCRAM_SHA_512,
            protocol::AuthMethod::MD5,
        };
    }
    if (surface == "firebird") {
        return {
            protocol::AuthMethod::PASSWORD,
            protocol::AuthMethod::SCRAM_SHA_256,
            protocol::AuthMethod::SCRAM_SHA_512,
            protocol::AuthMethod::MD5,
        };
    }
    return {
        protocol::AuthMethod::SCRAM_SHA_256,
        protocol::AuthMethod::SCRAM_SHA_512,
        protocol::AuthMethod::TOKEN,
        protocol::AuthMethod::PEER,
        protocol::AuthMethod::PASSWORD,
        protocol::AuthMethod::MD5,
    };
}

inline bool parserAuthMethodSupported(const std::string& surface_name,
                                      protocol::AuthMethod method) {
    const auto order = parserAuthMethodOrder(surface_name);
    return std::find(order.begin(), order.end(), method) != order.end();
}

inline std::vector<protocol::AuthMethod> orderAuthMethodsForSurface(
    const std::string& surface_name,
    const std::vector<protocol::AuthMethod>& methods) {
    std::vector<protocol::AuthMethod> ordered;
    ordered.reserve(methods.size());

    for (auto policy_method : parserAuthMethodOrder(surface_name)) {
        if (std::find(methods.begin(), methods.end(), policy_method) != methods.end()) {
            ordered.push_back(policy_method);
        }
    }

    for (auto method : methods) {
        if (std::find(ordered.begin(), ordered.end(), method) == ordered.end()) {
            ordered.push_back(method);
        }
    }

    return ordered;
}

inline std::vector<std::string> parserWireAuthPluginOrder(const std::string& surface_name) {
    const std::string surface = normalizeParserAuthSurface(surface_name);
    if (surface == "postgresql") {
        return {
            "SCRAM-SHA-256",
            "md5",
            "password",
        };
    }
    if (surface == "mysql") {
        return {
            "caching_sha2_password",
            "mysql_native_password",
            "mysql_clear_password",
        };
    }
    if (surface == "firebird") {
        return {
            "Legacy_Auth",
            "Srp256",
            "Srp",
        };
    }
    return {
        "scratchbird.auth.scram_sha_256",
        "scratchbird.auth.scram_sha_512",
        "scratchbird.auth.authkey_token",
        "scratchbird.auth.peer_uid",
        "scratchbird.auth.password_compat",
        "scratchbird.auth.md5_legacy",
    };
}

inline bool parserWireAuthPluginAllowed(const std::string& surface_name,
                                        const std::string& plugin_name) {
    const auto normalized_plugin = toLowerAscii(plugin_name);
    for (const auto& policy_plugin : parserWireAuthPluginOrder(surface_name)) {
        if (toLowerAscii(policy_plugin) == normalized_plugin) {
            return true;
        }
    }
    return false;
}

} // namespace scratchbird::security
