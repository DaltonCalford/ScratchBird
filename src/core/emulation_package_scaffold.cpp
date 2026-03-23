/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/emulation_package_scaffold.h"

#include <algorithm>
#include <cctype>

namespace scratchbird::core
{

    namespace
    {
        auto canonicalizeProfileId(std::string profile_id) -> std::string
        {
            std::transform(profile_id.begin(), profile_id.end(), profile_id.begin(),
                           [](unsigned char ch) {
                               return static_cast<char>(std::tolower(ch));
                           });
            if (profile_id == "firebird")
            {
                return "firebirdsql";
            }
            if (profile_id == "postgres")
            {
                return "postgresql";
            }
            return profile_id;
        }
    } // namespace

    auto builtinEmulationPackageScaffolds()
        -> const std::vector<EmulationPackageScaffold> &
    {
        static const std::vector<EmulationPackageScaffold> scaffolds = {
            {"postgresql",
             "sb_listener_pg",
             "sb_parser_pg",
             "sb_pkg_postgresql_parser",
             "sb_pkg_postgresql_compiler_udr",
             "sb_pkg_postgresql_emulation_udr",
             "sb_emulation_bundle_postgresql/v1",
             true,
             true,
             false,
             false},
            {"mysql",
             "sb_listener_mysql",
             "sb_parser_mysql",
             "sb_pkg_mysql_parser",
             "sb_pkg_mysql_compiler_udr",
             "sb_pkg_mysql_emulation_udr",
             "sb_emulation_bundle_mysql/v1",
             true,
             true,
             false,
             false},
            {"firebirdsql",
             "sb_listener_fb",
             "sb_parser_fb",
             "sb_pkg_firebird_parser",
             "sb_pkg_firebird_compiler_udr",
             "sb_pkg_firebird_emulation_udr",
             "sb_emulation_bundle_firebirdsql/v1",
             true,
             true,
             true,
             true},
        };
        return scaffolds;
    }

    auto resolveEmulationPackageScaffold(const std::string &profile_id,
                                         const EmulationPackageScaffold *&scaffold_out,
                                         ErrorContext *ctx) -> Status
    {
        scaffold_out = nullptr;
        const std::string normalized = canonicalizeProfileId(profile_id);
        for (const auto &scaffold : builtinEmulationPackageScaffolds())
        {
            if (canonicalizeProfileId(scaffold.profile_id) == normalized)
            {
                scaffold_out = &scaffold;
                return Status::OK;
            }
        }

        if (ctx != nullptr)
        {
            ctx->set(Status::NOT_FOUND,
                     ("No reusable emulation package scaffold is registered for profile '" +
                      normalized + "'")
                         .c_str(),
                     __FILE__,
                     __LINE__,
                     __func__);
        }
        return Status::NOT_FOUND;
    }

} // namespace scratchbird::core
