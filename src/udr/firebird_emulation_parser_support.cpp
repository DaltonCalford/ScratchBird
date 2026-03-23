/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/udr/firebird_emulation_udr.h"

#include "scratchbird/core/emulation_package_manifest.h"

#include <cctype>
#include <string_view>
#include <vector>

namespace scratchbird::udr
{

namespace
{

constexpr std::string_view kDefaultFirebirdServerToken = "firebird_localhost";

void clearErrorContextSuccess(core::ErrorContext *ctx)
{
    if (ctx == nullptr)
    {
        return;
    }
    ctx->code = core::Status::OK;
    ctx->sqlstate = core::statusToSQLState(core::Status::OK);
    ctx->sqlstate_text.clear();
    ctx->message.clear();
    ctx->vnext_code.clear();
    ctx->file = nullptr;
    ctx->line = 0;
    ctx->function = nullptr;
}

struct FirebirdDatabaseSpec
{
    std::string server;
    std::string file_path;
};

auto requireFirebirdEmulationPackage(const std::string &profile_id,
                                     core::ErrorContext *ctx) -> core::Status
{
    const core::EmulationPackageManifest *manifest = nullptr;
    return core::resolveInstalledEmulationPackage(profile_id,
                                                  core::EmulationPackageKind::EMULATION_UDR,
                                                  manifest,
                                                  ctx);
}

auto parseFirebirdDatabaseSpec(std::string_view spec) -> FirebirdDatabaseSpec
{
    FirebirdDatabaseSpec result;
    result.file_path = std::string(spec);

    size_t colon = result.file_path.find(':');
    if (colon != std::string::npos)
    {
        const bool is_drive = (colon == 1 &&
                               std::isalpha(static_cast<unsigned char>(result.file_path[0])) &&
                               result.file_path.size() > 2 &&
                               (result.file_path[2] == '\\' || result.file_path[2] == '/'));
        if (!is_drive)
        {
            result.server = result.file_path.substr(0, colon);
            result.file_path.erase(0, colon + 1);
        }
    }

    return result;
}

auto splitFirebirdPathComponents(std::string_view path) -> std::vector<std::string>
{
    std::string working(path);
    std::vector<std::string> components;

    if (working.size() >= 2 && std::isalpha(static_cast<unsigned char>(working[0])) &&
        working[1] == ':')
    {
        std::string drive(
            1, static_cast<char>(std::tolower(static_cast<unsigned char>(working[0]))));
        components.push_back(drive);
        working.erase(0, 2);
    }

    while (!working.empty() && (working.front() == '/' || working.front() == '\\'))
    {
        working.erase(working.begin());
    }

    std::string current;
    for (char ch : working)
    {
        if (ch == '/' || ch == '\\')
        {
            if (!current.empty())
            {
                components.push_back(current);
                current.clear();
            }
        }
        else
        {
            current.push_back(ch);
        }
    }
    if (!current.empty())
    {
        components.push_back(current);
    }

    if (!components.empty())
    {
        components.pop_back();
    }

    return components;
}

auto deriveFirebirdDatabaseName(std::string_view file_path) -> std::string
{
    const size_t last_sep = file_path.find_last_of("/\\");
    std::string base = (last_sep == std::string_view::npos)
                           ? std::string(file_path)
                           : std::string(file_path.substr(last_sep + 1));

    if (base.empty())
    {
        return base;
    }

    const size_t dot = base.find_last_of('.');
    if (dot != std::string::npos && dot + 1 < base.size())
    {
        std::string ext = base.substr(dot + 1);
        for (char &ch : ext)
        {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (ext == "fdb" || ext == "gdb" || ext == "sbdb")
        {
            base = base.substr(0, dot);
        }
    }

    return base;
}

auto buildEmulatedFirebirdSchemaPath(const std::string &server,
                                     const std::vector<std::string> &path_components,
                                     const std::string &db_name) -> std::string
{
    std::string schema = "emulated.firebird." + server;
    for (const auto &comp : path_components)
    {
        if (!comp.empty())
        {
            schema.push_back('.');
            schema += comp;
        }
    }
    if (!db_name.empty())
    {
        schema.push_back('.');
        schema += db_name;
    }
    return schema;
}

auto buildLegacyEmulatedFirebirdSchemaPath(const std::string &server,
                                           const std::vector<std::string> &path_components,
                                           const std::string &db_name) -> std::string
{
    std::string schema = "remote.emulation.firebird." + server;
    for (const auto &comp : path_components)
    {
        if (!comp.empty())
        {
            schema.push_back('.');
            schema += comp;
        }
    }
    if (!db_name.empty())
    {
        schema.push_back('.');
        schema += db_name;
    }
    return schema;
}

auto escapeLiteral(const std::string &in) -> std::string
{
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in)
    {
        out.push_back(c);
        if (c == '\'')
        {
            out.push_back('\'');
        }
    }
    return out;
}

} // namespace

auto deriveFirebirdSchemaBinding(const FirebirdSchemaBindingRequest &request,
                                 FirebirdSchemaBindingResponse &response,
                                 core::ErrorContext *ctx) -> core::Status
{
    auto status = requireFirebirdEmulationPackage(request.profile_id, ctx);
    if (status != core::Status::OK)
    {
        return status;
    }

    if (request.database_binding.empty())
    {
        if (ctx != nullptr)
        {
            ctx->set(core::Status::INVALID_ARGUMENT,
                     "Firebird emulation database binding is empty",
                     __FILE__,
                     __LINE__,
                     __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }

    const FirebirdDatabaseSpec spec = parseFirebirdDatabaseSpec(request.database_binding);
    response.server_name = spec.server.empty() ? std::string(kDefaultFirebirdServerToken)
                                               : spec.server;
    response.database_name = deriveFirebirdDatabaseName(spec.file_path);
    if (response.database_name.empty())
    {
        response.database_name = "default";
    }
    const auto path_components = splitFirebirdPathComponents(spec.file_path);
    response.schema_name = buildEmulatedFirebirdSchemaPath(response.server_name,
                                                           path_components,
                                                           response.database_name);
    response.legacy_schema_name = buildLegacyEmulatedFirebirdSchemaPath(response.server_name,
                                                                        path_components,
                                                                        response.database_name);
    clearErrorContextSuccess(ctx);
    return core::Status::OK;
}

auto renderFirebirdLifecycleSql(const FirebirdLifecycleSqlRequest &request,
                                FirebirdLifecycleSqlResponse &response,
                                core::ErrorContext *ctx) -> core::Status
{
    auto status = requireFirebirdEmulationPackage(request.profile_id, ctx);
    if (status != core::Status::OK)
    {
        return status;
    }

    if (request.database_spec.empty())
    {
        if (ctx != nullptr)
        {
            ctx->set(core::Status::INVALID_ARGUMENT,
                     "Firebird emulation lifecycle operation requires a database specification",
                     __FILE__,
                     __LINE__,
                     __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }

    const std::string escaped = escapeLiteral(request.database_spec);
    switch (request.operation)
    {
        case FirebirdEmulationLifecycleOperation::CREATE_DATABASE:
            response.sql = "CREATE DATABASE '" + escaped + "'";
            break;
        case FirebirdEmulationLifecycleOperation::DROP_DATABASE:
            response.sql = "DROP DATABASE '" + escaped + "'";
            break;
    }

    clearErrorContextSuccess(ctx);
    return core::Status::OK;
}

} // namespace scratchbird::udr
