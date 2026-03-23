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

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"

#include <string>
#include <vector>

namespace scratchbird::core
{

    enum class EmulationPackageKind : uint8_t
    {
        PARSER = 0,
        COMPILER_UDR = 1,
        EMULATION_UDR = 2
    };

    struct EmulationPackageManifest
    {
        std::string profile_id;
        EmulationPackageKind kind = EmulationPackageKind::PARSER;
        std::string package_name;
        std::string contract_id;
        std::string executable_name;
        bool installed = true;
        bool enabled = true;
    };

    struct EmulationPackageRequirement
    {
        bool parser = false;
        bool compiler_udr = false;
        bool emulation_udr = false;
    };

    struct EmulationPackageBundle
    {
        std::string profile_id;
        const EmulationPackageManifest *parser_manifest = nullptr;
        const EmulationPackageManifest *compiler_udr_manifest = nullptr;
        const EmulationPackageManifest *emulation_udr_manifest = nullptr;
    };

    auto builtinEmulationPackageManifests()
        -> const std::vector<EmulationPackageManifest> &;

    auto listRegisteredEmulationPackages(std::vector<EmulationPackageManifest> &manifests_out)
        -> Status;

    auto listRegisteredEmulationProfiles(std::vector<std::string> &profiles_out) -> Status;

    auto inspectEmulationPackageBundle(const std::string &profile_id,
                                       EmulationPackageBundle &bundle_out,
                                       ErrorContext *ctx = nullptr) -> Status;

    auto requireEmulationPackageBundle(const std::string &profile_id,
                                       const EmulationPackageRequirement &requirement,
                                       EmulationPackageBundle &bundle_out,
                                       ErrorContext *ctx = nullptr) -> Status;

    auto resolveInstalledEmulationPackage(const std::string &profile_id,
                                          EmulationPackageKind kind,
                                          const EmulationPackageManifest *&manifest_out,
                                          ErrorContext *ctx = nullptr) -> Status;

} // namespace scratchbird::core
