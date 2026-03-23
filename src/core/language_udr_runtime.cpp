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
#include <unordered_set>

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

        auto canonicalizeProfileId(const std::string &profile_id) -> std::string
        {
            std::string normalized = toLowerAscii(trimAscii(profile_id));
            if (normalized == "scratchbird_v3" || normalized == "scratchbird_native" ||
                normalized == "native")
            {
                return "scratchbird";
            }
            if (normalized == "firebird")
            {
                return "firebirdsql";
            }
            return normalized;
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

        struct ProfileCompileContract
        {
            const char *translation_mode = "";
            std::array<const char *, 2> payload_formats{};
            size_t payload_count = 0;
        };

        auto resolveProfileCompileContract(const std::string &profile_id,
                                           ProfileCompileContract &contract_out) -> bool
        {
            const std::string normalized = canonicalizeProfileId(profile_id);

            if (normalized == "scratchbird")
            {
                contract_out.translation_mode = "SQL_REWRITE_TO_NATIVE";
                contract_out.payload_formats = {"SQL_TEXT", ""};
                contract_out.payload_count = 1;
                return true;
            }
            if (normalized == "cassandra")
            {
                contract_out.translation_mode = "CQL_REWRITE_TO_NATIVE";
                contract_out.payload_formats = {"CQL_TEXT", ""};
                contract_out.payload_count = 1;
                return true;
            }
            if (normalized == "clickhouse")
            {
                contract_out.translation_mode = "SQL_REWRITE_TO_NATIVE";
                contract_out.payload_formats = {"SQL_TEXT", ""};
                contract_out.payload_count = 1;
                return true;
            }
            if (normalized == "duckdb")
            {
                contract_out.translation_mode = "SQL_REWRITE_TO_NATIVE";
                contract_out.payload_formats = {"SQL_TEXT", ""};
                contract_out.payload_count = 1;
                return true;
            }
            if (normalized == "firebirdsql")
            {
                contract_out.translation_mode = "SQL_REWRITE_TO_NATIVE";
                contract_out.payload_formats = {"SQL_TEXT", ""};
                contract_out.payload_count = 1;
                return true;
            }
            if (normalized == "influxdb")
            {
                contract_out.translation_mode = "TIMESERIES_REWRITE_TO_NATIVE";
                contract_out.payload_formats = {"INFLUXQL_TEXT", "INFLUX_LINE"};
                contract_out.payload_count = 2;
                return true;
            }
            if (normalized == "mariadb")
            {
                contract_out.translation_mode = "SQL_REWRITE_TO_NATIVE";
                contract_out.payload_formats = {"SQL_TEXT", ""};
                contract_out.payload_count = 1;
                return true;
            }
            if (normalized == "milvus")
            {
                contract_out.translation_mode = "VECTOR_API_TO_NATIVE";
                contract_out.payload_formats = {"VECTOR_API_JSON", ""};
                contract_out.payload_count = 1;
                return true;
            }
            if (normalized == "mongodb")
            {
                contract_out.translation_mode = "DOCUMENT_PIPELINE_TO_NATIVE";
                contract_out.payload_formats = {"BSON_COMMAND", ""};
                contract_out.payload_count = 1;
                return true;
            }
            if (normalized == "mysql")
            {
                contract_out.translation_mode = "SQL_REWRITE_TO_NATIVE";
                contract_out.payload_formats = {"SQL_TEXT", ""};
                contract_out.payload_count = 1;
                return true;
            }
            if (normalized == "neo4j")
            {
                contract_out.translation_mode = "CYPHER_REWRITE_TO_NATIVE";
                contract_out.payload_formats = {"CYPHER_TEXT", ""};
                contract_out.payload_count = 1;
                return true;
            }
            if (normalized == "opensearch")
            {
                contract_out.translation_mode = "DSL_REWRITE_TO_NATIVE";
                contract_out.payload_formats = {"JSON_DSL", ""};
                contract_out.payload_count = 1;
                return true;
            }
            if (normalized == "postgresql")
            {
                contract_out.translation_mode = "SQL_REWRITE_TO_NATIVE";
                contract_out.payload_formats = {"SQL_TEXT", ""};
                contract_out.payload_count = 1;
                return true;
            }
            if (normalized == "redis")
            {
                contract_out.translation_mode = "RESP_TO_NATIVE_COMMAND";
                contract_out.payload_formats = {"RESP_ARRAY", ""};
                contract_out.payload_count = 1;
                return true;
            }
            return false;
        }

        auto payloadMatchesContract(const std::string &payload_format,
                                    const ProfileCompileContract &contract) -> bool
        {
            for (size_t i = 0; i < contract.payload_count; ++i)
            {
                if (payload_format == contract.payload_formats[i])
                {
                    return true;
                }
            }
            return false;
        }

        auto isTextPayloadFormat(const std::string &payload_format) -> bool
        {
            static const std::unordered_set<std::string> text_formats{
                "SQL_TEXT",
                "CQL_TEXT",
                "CYPHER_TEXT",
                "JSON_DSL",
                "INFLUXQL_TEXT",
                "INFLUX_LINE",
                "VECTOR_API_JSON"};
            return text_formats.find(payload_format) != text_formats.end();
        }

        auto resolveLimitOrDefault(uint32_t value, uint32_t fallback) -> uint32_t
        {
            return value == 0 ? fallback : value;
        }

        auto estimateAstNodeCount(const LanguageUdrCompileRequest &request) -> uint32_t
        {
            const size_t payload_size = request.payload.size();
            if (payload_size == 0)
            {
                return 0;
            }
            if (!isTextPayloadFormat(request.payload_format))
            {
                const size_t estimated = payload_size / 8U + 1U;
                return estimated > std::numeric_limits<uint32_t>::max()
                           ? std::numeric_limits<uint32_t>::max()
                           : static_cast<uint32_t>(estimated);
            }

            uint64_t token_count = 0;
            uint64_t punctuation_count = 0;
            bool in_token = false;
            for (uint8_t byte : request.payload)
            {
                const unsigned char c = static_cast<unsigned char>(byte);
                const bool is_token_char = (std::isalnum(c) != 0) || c == '_';
                if (is_token_char)
                {
                    if (!in_token)
                    {
                        ++token_count;
                        in_token = true;
                    }
                    continue;
                }
                in_token = false;
                switch (c)
                {
                case '(':
                case ')':
                case '[':
                case ']':
                case '{':
                case '}':
                case ',':
                case '.':
                case ';':
                case ':':
                case '+':
                case '-':
                case '*':
                case '/':
                case '%':
                case '=':
                case '<':
                case '>':
                case '!':
                case '&':
                case '|':
                case '^':
                    ++punctuation_count;
                    break;
                default:
                    break;
                }
            }

            uint64_t estimated = token_count + punctuation_count;
            if (estimated == 0)
            {
                estimated = 1;
            }
            if (estimated > std::numeric_limits<uint32_t>::max())
            {
                return std::numeric_limits<uint32_t>::max();
            }
            return static_cast<uint32_t>(estimated);
        }

        auto estimateNormalizationSteps(const LanguageUdrCompileRequest &request) -> uint32_t
        {
            uint64_t steps = static_cast<uint64_t>(request.payload.size()) + 1U;
            if (isTextPayloadFormat(request.payload_format))
            {
                uint64_t whitespace = 0;
                for (uint8_t byte : request.payload)
                {
                    if (std::isspace(static_cast<unsigned char>(byte)) != 0)
                    {
                        ++whitespace;
                    }
                }
                steps += whitespace;
            }

            if (steps > std::numeric_limits<uint32_t>::max())
            {
                return std::numeric_limits<uint32_t>::max();
            }
            return static_cast<uint32_t>(steps);
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
        entry.registration.engine_profile_id = canonicalizeProfileId(entry.registration.engine_profile_id);
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
        const std::string profile_id_norm = canonicalizeProfileId(profile_id);
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<const ModuleEntry *> profile_matches;
        std::vector<const ModuleEntry *> exact_matches;

        for (const auto &[module_id, entry] : modules_)
        {
            (void)module_id;
            if (entry.registration.engine_profile_id == profile_id_norm)
            {
                profile_matches.push_back(&entry);
            }
            if (entry.registration.engine_profile_id == profile_id_norm &&
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

        if ((request.requires_network_access && !request.sandbox_policy.allow_network_access) ||
            (request.requires_filesystem_write && !request.sandbox_policy.allow_filesystem_write))
        {
            return setUdrError(ctx, core::Status::PERMISSION_DENIED, "UDR_1508",
                               "UDR attempted disallowed operation by sandbox policy");
        }

        constexpr uint32_t kDefaultMaxPayloadBytes = 1024U * 1024U;
        constexpr uint32_t kDefaultMaxAstNodeCount = 262144U;
        constexpr uint32_t kDefaultMaxNormalizationSteps = 524288U;
        constexpr uint32_t kDefaultMaxCompileWallTimeMs = 5000U;
        constexpr uint64_t kCompileWorkUnitsPerMs = 1000ULL;

        const uint32_t payload_limit =
            resolveLimitOrDefault(request.resource_limits.max_payload_bytes, kDefaultMaxPayloadBytes);
        if (request.payload.size() > static_cast<size_t>(payload_limit))
        {
            return setUdrError(ctx, core::Status::CONFIGURATION_LIMIT_EXCEEDED, "UDR_1512",
                               "Compile request payload exceeds configured quota");
        }

        const uint32_t ast_limit =
            resolveLimitOrDefault(request.resource_limits.max_ast_node_count, kDefaultMaxAstNodeCount);
        const uint32_t ast_estimate = estimateAstNodeCount(request);
        if (ast_estimate > ast_limit)
        {
            return setUdrError(ctx, core::Status::CONFIGURATION_LIMIT_EXCEEDED, "UDR_1512",
                               "Compile request estimated AST exceeds configured quota");
        }

        const uint32_t normalization_limit = resolveLimitOrDefault(
            request.resource_limits.max_normalization_steps, kDefaultMaxNormalizationSteps);
        const uint32_t normalization_steps = estimateNormalizationSteps(request);
        if (normalization_steps > normalization_limit)
        {
            return setUdrError(ctx, core::Status::CONFIGURATION_LIMIT_EXCEEDED, "UDR_1512",
                               "Compile request normalization budget exceeded");
        }

        const uint32_t compile_limit_ms = resolveLimitOrDefault(
            request.resource_limits.max_compile_wall_time_ms, kDefaultMaxCompileWallTimeMs);
        const uint64_t compile_budget_units =
            static_cast<uint64_t>(compile_limit_ms) * kCompileWorkUnitsPerMs;
        const uint64_t estimated_compile_units =
            static_cast<uint64_t>(ast_estimate) * 4ULL + static_cast<uint64_t>(normalization_steps);
        if (estimated_compile_units > compile_budget_units)
        {
            return setUdrError(ctx, core::Status::CONFIGURATION_LIMIT_EXCEEDED, "UDR_1512",
                               "Compile request wall-time budget exceeded");
        }

        status = registry.resolveActiveModule(request.profile_id, request.profile_version,
                                              selected_module_out, ctx);
        if (status != core::Status::OK)
        {
            return status;
        }

        ProfileCompileContract contract{};
        if (!resolveProfileCompileContract(request.profile_id, contract))
        {
            return setUdrError(ctx, core::Status::INVALID_ARGUMENT, "UDR_1501",
                               "Unknown engine profile id");
        }

        if (toLowerAscii(trimAscii(selected_module_out.translation_mode)) !=
            toLowerAscii(trimAscii(contract.translation_mode)))
        {
            return setUdrError(ctx, core::Status::NOT_SUPPORTED, "UDR_1503",
                               "Installed UDR module translation mode is incompatible with profile contract");
        }

        if (!payloadMatchesContract(request.payload_format, contract))
        {
            return setUdrError(ctx, core::Status::SYNTAX_ERROR, "UDR_1505",
                               "Source payload syntax unsupported for active profile");
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
