/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/udr/language_udr_runtime.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <sstream>

namespace scratchbird::udr
{

    namespace
    {
        constexpr uint64_t kFnvOffsetBasis = 1469598103934665603ULL;
        constexpr uint64_t kFnvPrime = 1099511628211ULL;

        auto isZeroUuidLocal(const core::ID &id) -> bool
        {
            for (uint8_t b : id.bytes)
            {
                if (b != 0)
                {
                    return false;
                }
            }
            return true;
        }

        auto trimAscii(const std::string &input) -> std::string
        {
            size_t start = 0;
            while (start < input.size() &&
                   std::isspace(static_cast<unsigned char>(input[start])) != 0)
            {
                ++start;
            }
            size_t end = input.size();
            while (end > start &&
                   std::isspace(static_cast<unsigned char>(input[end - 1])) != 0)
            {
                --end;
            }
            return input.substr(start, end - start);
        }

        auto toLowerAscii(std::string value) -> std::string
        {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char c) {
                               return static_cast<char>(std::tolower(c));
                           });
            return value;
        }

        auto parseUInt32(const std::string &token, uint32_t &value_out) -> bool
        {
            if (token.empty())
            {
                return false;
            }
            uint64_t parsed = 0;
            for (char ch : token)
            {
                if (ch < '0' || ch > '9')
                {
                    return false;
                }
                parsed = parsed * 10 + static_cast<uint64_t>(ch - '0');
                if (parsed > std::numeric_limits<uint32_t>::max())
                {
                    return false;
                }
            }
            value_out = static_cast<uint32_t>(parsed);
            return true;
        }

        auto parseSemver3(const std::string &semver,
                          std::array<uint32_t, 3> &parts_out) -> bool
        {
            std::string raw = trimAscii(semver);
            if (raw.empty())
            {
                return false;
            }

            const size_t plus_pos = raw.find('+');
            if (plus_pos != std::string::npos)
            {
                raw = raw.substr(0, plus_pos);
            }
            const size_t dash_pos = raw.find('-');
            if (dash_pos != std::string::npos)
            {
                raw = raw.substr(0, dash_pos);
            }

            std::array<uint32_t, 3> parts{0, 0, 0};
            size_t cursor = 0;
            for (size_t idx = 0; idx < 3; ++idx)
            {
                const size_t dot = raw.find('.', cursor);
                const bool last = (idx == 2);
                const size_t end = (dot == std::string::npos) ? raw.size() : dot;
                if (!last && dot == std::string::npos)
                {
                    return false;
                }
                std::string token = raw.substr(cursor, end - cursor);
                if (!parseUInt32(token, parts[idx]))
                {
                    return false;
                }
                if (last)
                {
                    if (dot != std::string::npos)
                    {
                        return false;
                    }
                    break;
                }
                cursor = end + 1;
            }

            parts_out = parts;
            return true;
        }

        auto parseProfileMajor(const std::string &profile_version,
                               uint32_t &major_out) -> bool
        {
            std::string raw = trimAscii(profile_version);
            if (raw.empty())
            {
                return false;
            }
            const size_t dot = raw.find('.');
            std::string major_token = (dot == std::string::npos) ? raw : raw.substr(0, dot);
            return parseUInt32(major_token, major_out);
        }

        auto compareSemver(const std::array<uint32_t, 3> &lhs,
                           const std::array<uint32_t, 3> &rhs) -> int
        {
            for (size_t i = 0; i < lhs.size(); ++i)
            {
                if (lhs[i] < rhs[i])
                {
                    return -1;
                }
                if (lhs[i] > rhs[i])
                {
                    return 1;
                }
            }
            return 0;
        }

        auto setUdrError(core::ErrorContext *ctx, core::Status status,
                         const char *vnext_code, const std::string &message) -> core::Status
        {
            SET_ERROR_CONTEXT_VNEXT(ctx, status, vnext_code, message.c_str());
            return status;
        }
    } // namespace

    auto LanguageUdrRegistry::computeCapabilitySetHash(
        const std::vector<std::string> &enabled_feature_keys) -> std::string
    {
        std::vector<std::string> keys;
        keys.reserve(enabled_feature_keys.size());
        for (const std::string &key : enabled_feature_keys)
        {
            std::string normalized = toLowerAscii(trimAscii(key));
            if (!normalized.empty())
            {
                keys.push_back(std::move(normalized));
            }
        }
        std::sort(keys.begin(), keys.end());
        keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

        uint64_t h = kFnvOffsetBasis;
        for (const std::string &key : keys)
        {
            for (unsigned char c : key)
            {
                h ^= static_cast<uint64_t>(c);
                h *= kFnvPrime;
            }
            h ^= static_cast<uint64_t>('|');
            h *= kFnvPrime;
        }

        std::ostringstream oss;
        oss << std::hex;
        oss.width(16);
        oss.fill('0');
        oss << h;
        return oss.str();
    }

    auto LanguageUdrRegistry::registerModule(
        const LanguageUdrRegistration &registration,
        const std::vector<std::string> &enabled_feature_keys,
        core::ErrorContext *ctx) -> core::Status
    {
        if (isZeroUuidLocal(registration.module_id) ||
            registration.module_name.empty() ||
            registration.engine_profile_id.empty() ||
            registration.engine_profile_version.empty() ||
            registration.translation_mode.empty() ||
            registration.module_semver.empty() ||
            registration.artifact_hash.empty())
        {
            return setUdrError(ctx, core::Status::INVALID_ARGUMENT, "UDR_1506",
                               "Module registration record is incomplete");
        }

        std::array<uint32_t, 3> semver{};
        if (!parseSemver3(registration.module_semver, semver))
        {
            return setUdrError(ctx, core::Status::INVALID_ARGUMENT, "UDR_1506",
                               "module_semver must be MAJOR.MINOR.PATCH");
        }

        ModuleEntry entry{};
        entry.registration = registration;
        for (const std::string &key : enabled_feature_keys)
        {
            std::string normalized = toLowerAscii(trimAscii(key));
            if (!normalized.empty())
            {
                entry.enabled_features.insert(std::move(normalized));
            }
        }

        if (entry.registration.capability_set_hash.empty())
        {
            std::vector<std::string> features;
            features.reserve(entry.enabled_features.size());
            for (const std::string &feature : entry.enabled_features)
            {
                features.push_back(feature);
            }
            entry.registration.capability_set_hash = computeCapabilitySetHash(features);
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto [it, inserted] = modules_.insert_or_assign(registration.module_id, std::move(entry));
        (void)it;
        (void)inserted;
        return core::Status::OK;
    }

    auto LanguageUdrRegistry::setModuleStatus(
        const core::ID &module_id, LanguageUdrModuleStatus status,
        core::ErrorContext *ctx) -> core::Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = modules_.find(module_id);
        if (it == modules_.end())
        {
            return setUdrError(ctx, core::Status::NOT_FOUND, "UDR_1502",
                               "Module not installed");
        }

        const LanguageUdrModuleStatus current = it->second.registration.status;
        const bool allowed =
            (current == LanguageUdrModuleStatus::ACTIVE &&
             (status == LanguageUdrModuleStatus::DISABLED || status == LanguageUdrModuleStatus::REVOKED)) ||
            (current == LanguageUdrModuleStatus::DISABLED &&
             (status == LanguageUdrModuleStatus::ACTIVE || status == LanguageUdrModuleStatus::REVOKED)) ||
            (current == status);

        if (!allowed)
        {
            return setUdrError(ctx, core::Status::CONSTRAINT_VIOLATION, "UDR_1503",
                               "Invalid module state transition");
        }

        it->second.registration.status = status;
        return core::Status::OK;
    }

    auto LanguageUdrRegistry::resolveActiveModule(
        const std::string &profile_id, const std::string &profile_version,
        LanguageUdrRegistration &resolved_out, core::ErrorContext *ctx) const -> core::Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<const ModuleEntry *> profile_matches;
        std::vector<const ModuleEntry *> exact_matches;

        for (const auto &[module_id, entry] : modules_)
        {
            (void)module_id;
            if (entry.registration.engine_profile_id == profile_id)
            {
                profile_matches.push_back(&entry);
            }
            if (entry.registration.engine_profile_id == profile_id &&
                entry.registration.engine_profile_version == profile_version &&
                entry.registration.status == LanguageUdrModuleStatus::ACTIVE)
            {
                exact_matches.push_back(&entry);
            }
        }

        if (profile_matches.empty())
        {
            return setUdrError(ctx, core::Status::INVALID_ARGUMENT, "UDR_1501",
                               "Unknown engine profile id");
        }
        if (exact_matches.empty())
        {
            return setUdrError(ctx, core::Status::NOT_FOUND, "UDR_1502",
                               "Profile language UDR module not installed");
        }

        uint32_t profile_major = 0;
        if (!parseProfileMajor(profile_version, profile_major))
        {
            return setUdrError(ctx, core::Status::INVALID_ARGUMENT, "UDR_1506",
                               "profile_version is malformed");
        }

        std::vector<const ModuleEntry *> compatible;
        for (const ModuleEntry *entry : exact_matches)
        {
            std::array<uint32_t, 3> semver{};
            if (!parseSemver3(entry->registration.module_semver, semver))
            {
                continue;
            }
            if (semver[0] == profile_major)
            {
                compatible.push_back(entry);
            }
        }

        if (compatible.empty())
        {
            return setUdrError(ctx, core::Status::NOT_SUPPORTED, "UDR_1503",
                               "Installed UDR module version is incompatible with profile baseline");
        }

        std::string required_hash = compatible.front()->registration.capability_set_hash;
        for (const ModuleEntry *entry : compatible)
        {
            if (entry->registration.capability_set_hash != required_hash)
            {
                return setUdrError(ctx, core::Status::NOT_SUPPORTED, "UDR_1503",
                                   "Multiple modules for profile have mismatched capability hash");
            }
        }

        const ModuleEntry *selected = compatible.front();
        std::array<uint32_t, 3> selected_semver{};
        parseSemver3(selected->registration.module_semver, selected_semver);
        for (size_t i = 1; i < compatible.size(); ++i)
        {
            std::array<uint32_t, 3> current_semver{};
            if (!parseSemver3(compatible[i]->registration.module_semver, current_semver))
            {
                continue;
            }
            if (compareSemver(current_semver, selected_semver) > 0)
            {
                selected = compatible[i];
                selected_semver = current_semver;
            }
        }

        resolved_out = selected->registration;
        return core::Status::OK;
    }

    auto LanguageUdrRegistry::ensureFeatureEnabled(
        const std::string &profile_id, const std::string &profile_version,
        const std::string &native_feature_key, core::ErrorContext *ctx) const -> core::Status
    {
        LanguageUdrRegistration selected{};
        core::Status status = resolveActiveModule(profile_id, profile_version, selected, ctx);
        if (status != core::Status::OK)
        {
            return status;
        }

        const std::string feature_key = toLowerAscii(trimAscii(native_feature_key));
        if (feature_key.empty())
        {
            return setUdrError(ctx, core::Status::INVALID_ARGUMENT, "UDR_1506",
                               "native_feature_key is required");
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto it = modules_.find(selected.module_id);
        if (it == modules_.end())
        {
            return setUdrError(ctx, core::Status::NOT_FOUND, "UDR_1502",
                               "Profile language UDR module not installed");
        }
        if (it->second.enabled_features.find(feature_key) == it->second.enabled_features.end())
        {
            return setUdrError(ctx, core::Status::NOT_SUPPORTED, "UDR_1504",
                               "Feature key disabled by profile capability set");
        }

        return core::Status::OK;
    }

    auto LanguageUdrRuntimeBoundary::validateCompileRequest(
        const LanguageUdrCompileRequest &request, core::ErrorContext *ctx) -> core::Status
    {
        static const std::unordered_set<std::string> kPayloadFormats{
            "SQL_TEXT",
            "CQL_TEXT",
            "CYPHER_TEXT",
            "RESP_ARRAY",
            "BSON_COMMAND",
            "JSON_DSL",
            "INFLUXQL_TEXT",
            "INFLUX_LINE",
            "VECTOR_API_JSON"};

        if (isZeroUuidLocal(request.request_id) ||
            request.profile_id.empty() ||
            request.profile_version.empty() ||
            request.payload_format.empty() ||
            request.payload.empty() ||
            request.session_option_signature.empty() ||
            isZeroUuidLocal(request.principal_id) ||
            request.role_context_signature.empty() ||
            request.transaction_id == 0)
        {
            return setUdrError(ctx, core::Status::INVALID_ARGUMENT, "UDR_1506",
                               "Compile request payload schema invalid");
        }

        if (kPayloadFormats.find(request.payload_format) == kPayloadFormats.end())
        {
            return setUdrError(ctx, core::Status::INVALID_ARGUMENT, "UDR_1506",
                               "Unsupported payload_format");
        }

        return core::Status::OK;
    }

    auto LanguageUdrRuntimeBoundary::preflightCompile(
        const LanguageUdrRegistry &registry, const LanguageUdrCompileRequest &request,
        LanguageUdrRegistration &selected_module_out, core::ErrorContext *ctx) -> core::Status
    {
        core::Status status = validateCompileRequest(request, ctx);
        if (status != core::Status::OK)
        {
            return status;
        }

        if (!request.compile_permission_granted)
        {
            return setUdrError(ctx, core::Status::PERMISSION_DENIED, "UDR_1507",
                               "Caller principal not allowed to invoke language UDR");
        }

        status = registry.resolveActiveModule(request.profile_id, request.profile_version,
                                              selected_module_out, ctx);
        if (status != core::Status::OK)
        {
            return status;
        }

        if (!request.native_feature_key.empty())
        {
            status = registry.ensureFeatureEnabled(
                request.profile_id, request.profile_version, request.native_feature_key, ctx);
            if (status != core::Status::OK)
            {
                return status;
            }
        }

        return core::Status::OK;
    }

} // namespace scratchbird::udr

