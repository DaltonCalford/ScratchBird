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

namespace scratchbird::core
{
    enum class StorageLockResult : uint8_t
    {
        LOCKED = 0,
        LOCK_CONFLICT = 1,
        IO_ERROR = 2
    };

    class StorageLockProvider
    {
    public:
        virtual ~StorageLockProvider() = default;

        virtual auto tryLockExclusive(int fd, int* lock_errno = nullptr) const -> StorageLockResult = 0;
        virtual auto unlock(int fd, int* lock_errno = nullptr) const -> bool = 0;
    };

    auto getStorageLockProvider() -> const StorageLockProvider&;
} // namespace scratchbird::core
