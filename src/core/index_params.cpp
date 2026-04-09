/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/index_params.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace scratchbird::core
{

namespace
{
    auto toUpperAscii(std::string value) -> std::string
    {
        std::transform(value.begin(),
                       value.end(),
                       value.begin(),
                       [](unsigned char ch) {
                           return static_cast<char>(std::toupper(ch));
                       });
        return value;
    }

    std::string trim(std::string value)
    {
        auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
        value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                                                [&](unsigned char ch) { return !is_space(ch); }));
        value.erase(std::find_if(value.rbegin(), value.rend(),
                                 [&](unsigned char ch) { return !is_space(ch); })
                        .base(),
                    value.end());
        return value;
    }

    bool parseBool(const std::string &value, bool *out)
    {
        std::string lower = value;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (lower == "1" || lower == "true" || lower == "yes" || lower == "on")
        {
            *out = true;
            return true;
        }
        if (lower == "0" || lower == "false" || lower == "no" || lower == "off")
        {
            *out = false;
            return true;
        }
        return false;
    }

    auto parseMetricsType(const std::string &value)
        -> optimizer::IndexFamilyMetricsType
    {
        const std::string upper = toUpperAscii(trim(value));
        if (upper == "ORDERED_EXACT")
        {
            return optimizer::IndexFamilyMetricsType::ORDERED_EXACT;
        }
        if (upper == "SUMMARY_CANDIDATE")
        {
            return optimizer::IndexFamilyMetricsType::SUMMARY_CANDIDATE;
        }
        if (upper == "GENERALIZED_SPATIAL")
        {
            return optimizer::IndexFamilyMetricsType::GENERALIZED_SPATIAL;
        }
        if (upper == "TEXT_SEARCH")
        {
            return optimizer::IndexFamilyMetricsType::TEXT_SEARCH;
        }
        if (upper == "ANN")
        {
            return optimizer::IndexFamilyMetricsType::ANN;
        }
        return optimizer::IndexFamilyMetricsType::UNKNOWN;
    }
} // namespace

std::string serializeIndexParams(const IndexParams &params)
{
    std::ostringstream out;
    bool first = true;
    const auto append_pair = [&](const std::string &key,
                                 const std::string &value,
                                 bool emit_when_empty = false) {
        if (!emit_when_empty && value.empty())
        {
            return;
        }
        if (!first)
        {
            out << ';';
        }
        first = false;
        out << key << '=' << value;
    };

    append_pair("family.physical", params.physical_family);
    append_pair("family.planner", params.planner_family);
    append_pair("family.mode", params.family_mode);
    append_pair("family.format_version", std::to_string(params.format_version));
    append_pair("family.alias_origin", params.alias_origin);
    append_pair("family.options_version",
                std::to_string(params.family_options_version));
    append_pair("family.lifecycle", params.lifecycle_model);
    append_pair("family.metrics_type",
                optimizer::indexFamilyMetricsTypeName(params.metrics_type));
    append_pair("family.metrics_version", std::to_string(params.metrics_version));
    append_pair("family.queryability_state", params.queryability_state);

    for (const auto &[key, value] : params.legacy_pairs)
    {
        append_pair(key, value, true);
    }

    for (const auto &[key, value] : params.raw_options)
    {
        append_pair("option." + key, value, true);
    }

    if (params.has_bloom)
    {
        append_pair("bloom.enabled", params.bloom.enabled ? "1" : "0", true);
        if (params.bloom.enabled)
        {
            append_pair("bloom.fpr", std::to_string(params.bloom.target_fpr), true);
            append_pair("bloom.meta_gpid", std::to_string(params.bloom.meta_gpid), true);
            if (params.bloom.bits_per_key > 0)
            {
                append_pair("bloom.bits_per_key",
                            std::to_string(static_cast<uint32_t>(params.bloom.bits_per_key)),
                            true);
            }
            if (params.bloom.num_hashes > 0)
            {
                append_pair("bloom.num_hashes",
                            std::to_string(static_cast<uint32_t>(params.bloom.num_hashes)),
                            true);
            }
        }
    }
    return out.str();
}

bool parseIndexParams(const std::string &params_str, IndexParams *params_out)
{
    if (!params_out)
    {
        return false;
    }

    *params_out = IndexParams{};
    if (params_str.empty())
    {
        return true;
    }

    std::istringstream stream(params_str);
    std::string token;
    while (std::getline(stream, token, ';'))
    {
        auto equals = token.find('=');
        if (equals == std::string::npos)
        {
            continue;
        }

        std::string key = trim(token.substr(0, equals));
        std::string value = trim(token.substr(equals + 1));
        if (key.empty())
        {
            continue;
        }

        if (key == "bloom.enabled")
        {
            bool enabled = false;
            if (parseBool(value, &enabled))
            {
                params_out->has_bloom = true;
                params_out->bloom.enabled = enabled;
            }
            continue;
        }
        if (key == "bloom.fpr")
        {
            try
            {
                params_out->has_bloom = true;
                params_out->bloom.target_fpr = std::stod(value);
            }
            catch (...)
            {
            }
            continue;
        }
        if (key == "bloom.meta_gpid")
        {
            try
            {
                params_out->has_bloom = true;
                params_out->bloom.meta_gpid = static_cast<GPID>(std::stoull(value));
            }
            catch (...)
            {
            }
            continue;
        }
        if (key == "bloom.bits_per_key")
        {
            try
            {
                params_out->has_bloom = true;
                params_out->bloom.bits_per_key =
                    static_cast<uint8_t>(std::stoul(value));
            }
            catch (...)
            {
            }
            continue;
        }
        if (key == "bloom.num_hashes")
        {
            try
            {
                params_out->has_bloom = true;
                params_out->bloom.num_hashes =
                    static_cast<uint8_t>(std::stoul(value));
            }
            catch (...)
            {
            }
            continue;
        }
        if (key == "family.physical")
        {
            params_out->physical_family = value;
            continue;
        }
        if (key == "family.planner")
        {
            params_out->planner_family = value;
            continue;
        }
        if (key == "family.mode")
        {
            params_out->family_mode = value;
            continue;
        }
        if (key == "family.format_version")
        {
            try
            {
                params_out->format_version =
                    static_cast<uint16_t>(std::stoul(value));
            }
            catch (...)
            {
            }
            continue;
        }
        if (key == "family.alias_origin")
        {
            params_out->alias_origin = value;
            continue;
        }
        if (key == "family.options_version")
        {
            try
            {
                params_out->family_options_version =
                    static_cast<uint16_t>(std::stoul(value));
            }
            catch (...)
            {
            }
            continue;
        }
        if (key == "family.lifecycle")
        {
            params_out->lifecycle_model = value;
            continue;
        }
        if (key == "family.metrics_type")
        {
            params_out->metrics_type = parseMetricsType(value);
            continue;
        }
        if (key == "family.metrics_version")
        {
            try
            {
                params_out->metrics_version =
                    static_cast<uint16_t>(std::stoul(value));
            }
            catch (...)
            {
            }
            continue;
        }
        if (key == "family.queryability_state")
        {
            params_out->queryability_state = value;
            continue;
        }
        if (key.rfind("option.", 0) == 0 && key.size() > 7)
        {
            params_out->raw_options.emplace(key.substr(7), value);
            continue;
        }

        params_out->legacy_pairs.emplace(key, value);
    }

    return true;
}

} // namespace scratchbird::core
