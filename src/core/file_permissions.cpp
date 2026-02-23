/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/file_permissions.h"

#include <filesystem>

#ifndef _WIN32
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#endif

namespace scratchbird::core
{

    namespace
    {

        class PlatformFilePermissionsControl final : public FilePermissionsControl
        {
        public:
            auto readMetadata(const std::string& path,
                              FileMetadata* metadata_out,
                              ErrorContext* ctx) -> Status override
            {
                if (metadata_out == nullptr)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "metadata_out is null");
                    return Status::INVALID_ARGUMENT;
                }
                *metadata_out = FileMetadata{};

#ifdef _WIN32
                std::error_code ec;
                const std::filesystem::file_status status = std::filesystem::status(path, ec);
                if (ec)
                {
                    SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND, "File status lookup failed");
                    return Status::FILE_NOT_FOUND;
                }
                metadata_out->exists = std::filesystem::exists(status);
                metadata_out->is_regular = std::filesystem::is_regular_file(status);
                metadata_out->mode_supported = false;
                metadata_out->mode_bits = 0;
                return Status::OK;
#else
                struct stat st{};
                if (::stat(path.c_str(), &st) != 0)
                {
                    std::string message = "stat failed: " + std::string(std::strerror(errno));
                    SET_ERROR_CONTEXT(ctx, Status::FILE_NOT_FOUND, message.c_str());
                    return Status::FILE_NOT_FOUND;
                }
                metadata_out->exists = true;
                metadata_out->is_regular = S_ISREG(st.st_mode);
                metadata_out->mode_supported = true;
                metadata_out->mode_bits = static_cast<uint32_t>(st.st_mode & 0777);
                return Status::OK;
#endif
            }

            auto setMode(const std::string& path,
                         uint32_t mode_bits,
                         ErrorContext* ctx) -> Status override
            {
#ifdef _WIN32
                (void)path;
                (void)mode_bits;
                SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "chmod-style mode set is not supported");
                return Status::NOT_SUPPORTED;
#else
                if (::chmod(path.c_str(), static_cast<mode_t>(mode_bits & 0777)) != 0)
                {
                    std::string message = "chmod failed: " + std::string(std::strerror(errno));
                    SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, message.c_str());
                    return Status::IO_ERROR;
                }
                return Status::OK;
#endif
            }
        };

    } // namespace

    auto createDefaultFilePermissionsControl() -> std::unique_ptr<FilePermissionsControl>
    {
        return std::make_unique<PlatformFilePermissionsControl>();
    }

} // namespace scratchbird::core

