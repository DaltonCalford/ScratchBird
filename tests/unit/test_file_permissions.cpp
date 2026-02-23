/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "scratchbird/core/file_permissions.h"

namespace scratchbird::core
{

TEST(FilePermissionsControlTest, ReadMetadataForExistingFile)
{
    auto control = createDefaultFilePermissionsControl();
    ASSERT_NE(control, nullptr);

    std::filesystem::path path =
        std::filesystem::temp_directory_path() / "sb_file_permissions_metadata.tmp";
    {
        std::ofstream file(path);
        ASSERT_TRUE(file.good());
        file << "test";
    }

    ErrorContext ctx;
    FileMetadata metadata;
    ASSERT_EQ(control->readMetadata(path.string(), &metadata, &ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(metadata.exists);
    EXPECT_TRUE(metadata.is_regular);

    std::filesystem::remove(path);
}

#ifndef _WIN32
TEST(FilePermissionsControlTest, SetModeAndReadBack)
{
    auto control = createDefaultFilePermissionsControl();
    ASSERT_NE(control, nullptr);

    std::filesystem::path path =
        std::filesystem::temp_directory_path() / "sb_file_permissions_mode.tmp";
    {
        std::ofstream file(path);
        ASSERT_TRUE(file.good());
        file << "test";
    }

    ErrorContext ctx;
    ASSERT_EQ(control->setMode(path.string(), 0600, &ctx), Status::OK) << ctx.message;

    FileMetadata metadata;
    ASSERT_EQ(control->readMetadata(path.string(), &metadata, &ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(metadata.mode_supported);
    EXPECT_EQ(metadata.mode_bits, 0600u);

    std::filesystem::remove(path);
}
#endif

} // namespace scratchbird::core

