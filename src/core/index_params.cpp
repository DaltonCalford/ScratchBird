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
} // namespace

std::string serializeIndexParams(const IndexParams &params)
{
    if (!params.has_bloom)
    {
        return {};
    }

    std::ostringstream out;
    out << "bloom.enabled=" << (params.bloom.enabled ? 1 : 0);
    if (params.bloom.enabled)
    {
        out << ";bloom.fpr=" << params.bloom.target_fpr;
        out << ";bloom.meta_gpid=" << params.bloom.meta_gpid;
        if (params.bloom.bits_per_key > 0)
        {
            out << ";bloom.bits_per_key=" << static_cast<uint32_t>(params.bloom.bits_per_key);
        }
        if (params.bloom.num_hashes > 0)
        {
            out << ";bloom.num_hashes=" << static_cast<uint32_t>(params.bloom.num_hashes);
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
    }

    return true;
}

} // namespace scratchbird::core
