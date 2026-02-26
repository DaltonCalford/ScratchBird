/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 */

#include "pam_plugin_config.h"

#include <cctype>
#include <sstream>

namespace scratchbird {
namespace security {
namespace plugins {
namespace pam {

namespace {

std::string trimAscii(std::string value) {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!value.empty() && is_space(static_cast<unsigned char>(value.front()))) {
        value.erase(value.begin());
    }
    while (!value.empty() && is_space(static_cast<unsigned char>(value.back()))) {
        value.pop_back();
    }
    return value;
}

bool parseUInt32(const std::string& value, uint32_t& out) {
    try {
        out = static_cast<uint32_t>(std::stoul(trimAscii(value)));
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> splitCsv(const std::string& value) {
    std::vector<std::string> out;
    std::istringstream input(value);
    std::string token;
    while (std::getline(input, token, ',')) {
        token = trimAscii(std::move(token));
        if (!token.empty()) {
            out.push_back(token);
        }
    }
    return out;
}

}  // namespace

PamPluginConfigStatus loadPamPluginConfig(const std::map<std::string, std::string>& values,
                                          PamPluginConfig& out,
                                          std::string* error_out) {
    auto set_error = [error_out](const std::string& message) {
        if (error_out) {
            *error_out = message;
        }
    };

    const auto svc_it = values.find("service_name");
    const auto modules_it = values.find("allowed_modules");
    if (svc_it == values.end() || modules_it == values.end()) {
        set_error("Missing required keys: service_name/allowed_modules");
        return PamPluginConfigStatus::MISSING_REQUIRED_KEY;
    }

    out.service_name = trimAscii(svc_it->second);
    out.allowed_modules = splitCsv(modules_it->second);

    auto timeout_it = values.find("conversation_timeout_ms");
    if (timeout_it != values.end() &&
        !parseUInt32(timeout_it->second, out.conversation_timeout_ms)) {
        set_error("conversation_timeout_ms must be numeric");
        return PamPluginConfigStatus::INVALID_VALUE;
    }

    return PamPluginConfigStatus::OK;
}

}  // namespace pam
}  // namespace plugins
}  // namespace security
}  // namespace scratchbird
