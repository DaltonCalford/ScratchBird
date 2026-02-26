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
#include <fcntl.h>

#include "test_helpers.h"
#include "scratchbird/core/portable_file_io.h"
#include "scratchbird/core/storage_lock_provider.h"

namespace scratchbird::core
{
    TEST(StorageLockProviderTest, LockConflictAndReleasePath)
    {
        const std::string lock_path =
            scratchbird::testing::uniqueTestDbPath("storage_lock_provider", ".lock");
        std::filesystem::remove(lock_path);

        const int fd_a = platform::openFd(lock_path.c_str(), O_RDWR | O_CREAT, 0644);
        ASSERT_GE(fd_a, 0);

        const int fd_b = platform::openFd(lock_path.c_str(), O_RDWR, 0644);
        ASSERT_GE(fd_b, 0);

        const StorageLockProvider &provider = getStorageLockProvider();
        int lock_errno = 0;
        ASSERT_EQ(provider.tryLockExclusive(fd_a, &lock_errno), StorageLockResult::LOCKED);

        lock_errno = 0;
        const StorageLockResult second_result = provider.tryLockExclusive(fd_b, &lock_errno);
        EXPECT_EQ(second_result, StorageLockResult::LOCK_CONFLICT);

        lock_errno = 0;
        EXPECT_TRUE(provider.unlock(fd_a, &lock_errno));

        lock_errno = 0;
        EXPECT_EQ(provider.tryLockExclusive(fd_b, &lock_errno), StorageLockResult::LOCKED);
        EXPECT_TRUE(provider.unlock(fd_b, &lock_errno));

        platform::closeFd(fd_b);
        platform::closeFd(fd_a);
        std::filesystem::remove(lock_path);
    }
} // namespace scratchbird::core
