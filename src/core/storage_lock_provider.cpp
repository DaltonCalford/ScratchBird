/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/storage_lock_provider.h"

#include <cerrno>
#include <climits>

#if defined(_WIN32)
    #include <io.h>
    #include <sys/locking.h>
#else
    #include <sys/file.h>
#endif

namespace scratchbird::core
{
    namespace
    {
        class PlatformStorageLockProvider final : public StorageLockProvider
        {
        public:
            auto tryLockExclusive(int fd, int* lock_errno = nullptr) const -> StorageLockResult override
            {
#if defined(_WIN32)
                if (_lseeki64(fd, 0, SEEK_SET) < 0)
                {
                    if (lock_errno != nullptr)
                    {
                        *lock_errno = errno;
                    }
                    return StorageLockResult::IO_ERROR;
                }
                if (_locking(fd, _LK_NBLCK, LONG_MAX) == 0)
                {
                    return StorageLockResult::LOCKED;
                }
                if (lock_errno != nullptr)
                {
                    *lock_errno = errno;
                }
                if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EACCES)
                {
                    return StorageLockResult::LOCK_CONFLICT;
                }
                return StorageLockResult::IO_ERROR;
#else
                if (flock(fd, LOCK_EX | LOCK_NB) == 0)
                {
                    return StorageLockResult::LOCKED;
                }
                if (lock_errno != nullptr)
                {
                    *lock_errno = errno;
                }
                if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EACCES)
                {
                    return StorageLockResult::LOCK_CONFLICT;
                }
                return StorageLockResult::IO_ERROR;
#endif
            }

            auto unlock(int fd, int* lock_errno = nullptr) const -> bool override
            {
#if defined(_WIN32)
                if (_lseeki64(fd, 0, SEEK_SET) < 0)
                {
                    if (lock_errno != nullptr)
                    {
                        *lock_errno = errno;
                    }
                    return false;
                }
                if (_locking(fd, _LK_UNLCK, LONG_MAX) == 0)
                {
                    return true;
                }
                if (lock_errno != nullptr)
                {
                    *lock_errno = errno;
                }
                return false;
#else
                if (flock(fd, LOCK_UN) == 0)
                {
                    return true;
                }
                if (lock_errno != nullptr)
                {
                    *lock_errno = errno;
                }
                return false;
#endif
            }
        };
    } // namespace

    auto getStorageLockProvider() -> const StorageLockProvider&
    {
        static const PlatformStorageLockProvider k_provider{};
        return k_provider;
    }
} // namespace scratchbird::core
