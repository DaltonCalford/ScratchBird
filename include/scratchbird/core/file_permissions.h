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

#include <cstdint>
#include <memory>
#include <string>

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"

namespace scratchbird::core
{

    struct FileMetadata
    {
        bool exists{false};
        bool is_regular{false};
        bool mode_supported{false};
        uint32_t mode_bits{0};
    };

    class FilePermissionsControl
    {
    public:
        virtual ~FilePermissionsControl() = default;

        virtual auto readMetadata(const std::string& path,
                                  FileMetadata* metadata_out,
                                  ErrorContext* ctx) -> Status = 0;

        virtual auto setMode(const std::string& path,
                             uint32_t mode_bits,
                             ErrorContext* ctx) -> Status = 0;
    };

    auto createDefaultFilePermissionsControl() -> std::unique_ptr<FilePermissionsControl>;

} // namespace scratchbird::core

