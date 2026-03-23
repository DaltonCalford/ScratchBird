/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/emulation_package_manifest.h"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace scratchbird::core
{

    namespace
    {
        auto toLowerAscii(std::string value) -> std::string
        {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char ch) {
                               return static_cast<char>(std::tolower(ch));
                           });
            return value;
        }

        auto canonicalizeProfileId(std::string profile_id) -> std::string
        {
            profile_id = toLowerAscii(std::move(profile_id));
            if (profile_id == "scratchbird_v3" || profile_id == "scratchbird_native" ||
                profile_id == "native")
            {
                return "scratchbird";
            }
            if (profile_id == "firebird")
            {
                return "firebirdsql";
            }
            if (profile_id == "firebird_emulation")
            {
                return "firebirdsql";
            }
            if (profile_id == "postgresql_emulation")
            {
                return "postgresql";
            }
            if (profile_id == "mysql_emulation")
            {
                return "mysql";
            }
            return profile_id;
        }

        auto kindName(EmulationPackageKind kind) -> const char *
        {
            switch (kind)
            {
                case EmulationPackageKind::PARSER:
                    return "parser";
                case EmulationPackageKind::COMPILER_UDR:
                    return "compiler_udr";
                case EmulationPackageKind::EMULATION_UDR:
                    return "emulation_udr";
            }
            return "unknown";
        }
    } // namespace

    auto builtinEmulationPackageManifests()
        -> const std::vector<EmulationPackageManifest> &
    {
        static const std::vector<EmulationPackageManifest> manifests = {
            {"scratchbird",
             EmulationPackageKind::PARSER,
             "sb_pkg_scratchbird_parser",
             "sb_scratchbird_parser/v1",
             "sb_parser_native",
             true,
             true},
            {"scratchbird",
             EmulationPackageKind::COMPILER_UDR,
             "sb_pkg_scratchbird_compiler_udr",
             "sb_dialect_compiler_udr/v1",
             "sb_udr_scratchbird_compiler",
             true,
             true},
            {"firebirdsql",
             EmulationPackageKind::PARSER,
             "sb_pkg_firebird_parser",
             "sb_firebird_parser/v1",
             "sb_parser_fb",
             true,
             true},
            {"firebirdsql",
             EmulationPackageKind::COMPILER_UDR,
             "sb_pkg_firebird_compiler_udr",
             "sb_dialect_compiler_udr/v1",
             "sb_udr_firebird_compiler",
             true,
             true},
            {"firebirdsql",
             EmulationPackageKind::EMULATION_UDR,
             "sb_pkg_firebird_emulation_udr",
             "sb_firebird_emulation_udr/v1",
             "sb_udr_firebird_runtime",
             true,
             true},
            {"postgresql",
             EmulationPackageKind::PARSER,
             "sb_pkg_postgresql_parser",
             "sb_postgresql_parser/v1",
             "sb_parser_pg",
             true,
             true},
            {"postgresql",
             EmulationPackageKind::COMPILER_UDR,
             "sb_pkg_postgresql_compiler_udr",
             "sb_dialect_compiler_udr/v1",
             "sb_udr_postgresql_compiler",
             true,
             true},
            {"postgresql",
             EmulationPackageKind::EMULATION_UDR,
             "sb_pkg_postgresql_emulation_udr",
             "sb_emulation_catalog_udr/v1",
             "sb_udr_postgresql_runtime",
             true,
             true},
            {"mysql",
             EmulationPackageKind::PARSER,
             "sb_pkg_mysql_parser",
             "sb_mysql_parser/v1",
             "sb_parser_mysql",
             true,
             true},
            {"mysql",
             EmulationPackageKind::COMPILER_UDR,
             "sb_pkg_mysql_compiler_udr",
             "sb_dialect_compiler_udr/v1",
             "sb_udr_mysql_compiler",
             true,
             true},
            {"mysql",
             EmulationPackageKind::EMULATION_UDR,
             "sb_pkg_mysql_emulation_udr",
             "sb_emulation_catalog_udr/v1",
             "sb_udr_mysql_runtime",
             true,
             true},
        };
        return manifests;
    }

    auto listRegisteredEmulationPackages(std::vector<EmulationPackageManifest> &manifests_out)
        -> Status
    {
        manifests_out.assign(builtinEmulationPackageManifests().begin(),
                             builtinEmulationPackageManifests().end());
        return Status::OK;
    }

    auto listRegisteredEmulationProfiles(std::vector<std::string> &profiles_out) -> Status
    {
        profiles_out.clear();
        std::unordered_set<std::string> seen;
        for (const auto &manifest : builtinEmulationPackageManifests())
        {
            const std::string normalized = canonicalizeProfileId(manifest.profile_id);
            if (seen.insert(normalized).second)
            {
                profiles_out.push_back(normalized);
            }
        }
        return Status::OK;
    }

    auto inspectEmulationPackageBundle(const std::string &profile_id,
                                       EmulationPackageBundle &bundle_out,
                                       ErrorContext *ctx) -> Status
    {
        bundle_out = EmulationPackageBundle{};
        const std::string normalized = canonicalizeProfileId(profile_id);
        bundle_out.profile_id = normalized;

        bool found_any = false;
        for (const auto &manifest : builtinEmulationPackageManifests())
        {
            if (canonicalizeProfileId(manifest.profile_id) != normalized)
            {
                continue;
            }

            found_any = true;
            if (!manifest.installed || !manifest.enabled)
            {
                continue;
            }

            switch (manifest.kind)
            {
                case EmulationPackageKind::PARSER:
                    bundle_out.parser_manifest = &manifest;
                    break;
                case EmulationPackageKind::COMPILER_UDR:
                    bundle_out.compiler_udr_manifest = &manifest;
                    break;
                case EmulationPackageKind::EMULATION_UDR:
                    bundle_out.emulation_udr_manifest = &manifest;
                    break;
            }
        }

        if (!found_any)
        {
            if (ctx != nullptr)
            {
                ctx->set(Status::NOT_FOUND,
                         ("No emulation package manifests are registered for profile '" +
                          normalized + "'")
                             .c_str(),
                         __FILE__,
                         __LINE__,
                         __func__);
            }
            return Status::NOT_FOUND;
        }

        return Status::OK;
    }

    auto requireEmulationPackageBundle(const std::string &profile_id,
                                       const EmulationPackageRequirement &requirement,
                                       EmulationPackageBundle &bundle_out,
                                       ErrorContext *ctx) -> Status
    {
        auto status = inspectEmulationPackageBundle(profile_id, bundle_out, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<std::string> missing;
        if (requirement.parser && bundle_out.parser_manifest == nullptr)
        {
            missing.push_back("parser");
        }
        if (requirement.compiler_udr && bundle_out.compiler_udr_manifest == nullptr)
        {
            missing.push_back("compiler_udr");
        }
        if (requirement.emulation_udr && bundle_out.emulation_udr_manifest == nullptr)
        {
            missing.push_back("emulation_udr");
        }

        if (missing.empty())
        {
            return Status::OK;
        }

        if (ctx != nullptr)
        {
            std::string message = "Required emulation package set is incomplete for profile '" +
                                  bundle_out.profile_id + "': missing ";
            for (size_t i = 0; i < missing.size(); ++i)
            {
                if (i != 0)
                {
                    message += ", ";
                }
                message += missing[i];
            }
            ctx->set(Status::NOT_FOUND, message.c_str(), __FILE__, __LINE__, __func__);
        }
        return Status::NOT_FOUND;
    }

    auto resolveInstalledEmulationPackage(const std::string &profile_id,
                                          EmulationPackageKind kind,
                                          const EmulationPackageManifest *&manifest_out,
                                          ErrorContext *ctx) -> Status
    {
        manifest_out = nullptr;
        const std::string normalized = canonicalizeProfileId(profile_id);
        for (const auto &manifest : builtinEmulationPackageManifests())
        {
            if (manifest.kind == kind &&
                canonicalizeProfileId(manifest.profile_id) == normalized)
            {
                if (!manifest.installed || !manifest.enabled)
                {
                    if (ctx != nullptr)
                    {
                        ctx->set(Status::NOT_FOUND,
                                 ("Required emulation " + std::string(kindName(kind)) +
                                  " package is not installed or enabled for profile '" +
                                  normalized + "'")
                                     .c_str(),
                                 __FILE__,
                                 __LINE__,
                                 __func__);
                    }
                    return Status::NOT_FOUND;
                }
                manifest_out = &manifest;
                return Status::OK;
            }
        }

        if (ctx != nullptr)
        {
            ctx->set(Status::NOT_FOUND,
                     ("No emulation " + std::string(kindName(kind)) +
                      " package manifest registered for profile '" + normalized + "'")
                         .c_str(),
                     __FILE__,
                     __LINE__,
                     __func__);
        }
        return Status::NOT_FOUND;
    }

} // namespace scratchbird::core
