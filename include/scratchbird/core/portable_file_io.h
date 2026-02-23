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

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <limits>
#include <sys/types.h>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <io.h>
#else
    #include <unistd.h>
#endif

namespace scratchbird::core::platform
{
    inline auto openFd(const char* path, int flags, int mode = 0) -> int
    {
#if defined(_WIN32)
        if (path == nullptr)
        {
            errno = EINVAL;
            return -1;
        }
        if ((flags & O_CREAT) != 0)
        {
            return ::_open(path, flags, mode);
        }
        return ::_open(path, flags);
#else
        if (path == nullptr)
        {
            errno = EINVAL;
            return -1;
        }
        if ((flags & O_CREAT) != 0)
        {
            return ::open(path, flags, mode);
        }
        return ::open(path, flags);
#endif
    }

    inline auto seekFd(int fd, int64_t offset, int whence) -> int64_t
    {
#if defined(_WIN32)
        return static_cast<int64_t>(::_lseeki64(fd, static_cast<__int64>(offset), whence));
#else
        return static_cast<int64_t>(::lseek(fd, static_cast<off_t>(offset), whence));
#endif
    }

    inline auto closeFd(int fd) -> int
    {
#if defined(_WIN32)
        return ::_close(fd);
#else
        return ::close(fd);
#endif
    }

    inline auto resolveRealPath(const char* path, char* resolved_path, size_t resolved_size = 0) -> char*
    {
        if (path == nullptr)
        {
            errno = EINVAL;
            return nullptr;
        }
#if defined(_WIN32)
        if (resolved_path != nullptr)
        {
            if (resolved_size == 0)
            {
                errno = EINVAL;
                return nullptr;
            }
            return ::_fullpath(resolved_path, path, resolved_size);
        }
        size_t alloc_size = (resolved_size == 0) ? static_cast<size_t>(_MAX_PATH) : resolved_size;
        return ::_fullpath(nullptr, path, alloc_size);
#else
        (void)resolved_size;
        return ::realpath(path, resolved_path);
#endif
    }

    inline auto readAt(int fd, void* buffer, size_t size, int64_t offset) -> ssize_t
    {
#if defined(_WIN32)
        if (buffer == nullptr)
        {
            errno = EINVAL;
            return -1;
        }
        if (size == 0)
        {
            return 0;
        }
        if (offset < 0)
        {
            errno = EINVAL;
            return -1;
        }

        __int64 original_offset = _lseeki64(fd, 0, SEEK_CUR);
        if (original_offset < 0)
        {
            return -1;
        }
        if (_lseeki64(fd, static_cast<__int64>(offset), SEEK_SET) < 0)
        {
            return -1;
        }

        size_t total_read = 0;
        auto* read_ptr = static_cast<uint8_t*>(buffer);
        int saved_errno = 0;
        bool had_error = false;

        while (total_read < size)
        {
            unsigned int chunk = static_cast<unsigned int>(std::min<size_t>(
                size - total_read,
                static_cast<size_t>(std::numeric_limits<unsigned int>::max())));
            int bytes_read = _read(fd, read_ptr + total_read, chunk);
            if (bytes_read < 0)
            {
                saved_errno = errno;
                had_error = true;
                break;
            }
            if (bytes_read == 0)
            {
                break;
            }
            total_read += static_cast<size_t>(bytes_read);
        }

        (void)_lseeki64(fd, original_offset, SEEK_SET);
        if (had_error)
        {
            errno = saved_errno;
            return total_read > 0 ? static_cast<ssize_t>(total_read) : -1;
        }

        return static_cast<ssize_t>(total_read);
#else
        return ::pread(fd, buffer, size, static_cast<off_t>(offset));
#endif
    }

    inline auto writeAt(int fd, const void* buffer, size_t size, int64_t offset) -> ssize_t
    {
#if defined(_WIN32)
        if (buffer == nullptr)
        {
            errno = EINVAL;
            return -1;
        }
        if (size == 0)
        {
            return 0;
        }
        if (offset < 0)
        {
            errno = EINVAL;
            return -1;
        }

        __int64 original_offset = _lseeki64(fd, 0, SEEK_CUR);
        if (original_offset < 0)
        {
            return -1;
        }
        if (_lseeki64(fd, static_cast<__int64>(offset), SEEK_SET) < 0)
        {
            return -1;
        }

        size_t total_written = 0;
        const auto* write_ptr = static_cast<const uint8_t*>(buffer);
        int saved_errno = 0;
        bool had_error = false;

        while (total_written < size)
        {
            unsigned int chunk = static_cast<unsigned int>(std::min<size_t>(
                size - total_written,
                static_cast<size_t>(std::numeric_limits<unsigned int>::max())));
            int bytes_written = _write(fd, write_ptr + total_written, chunk);
            if (bytes_written < 0)
            {
                saved_errno = errno;
                had_error = true;
                break;
            }
            if (bytes_written == 0)
            {
                errno = EIO;
                had_error = true;
                break;
            }
            total_written += static_cast<size_t>(bytes_written);
        }

        (void)_lseeki64(fd, original_offset, SEEK_SET);
        if (had_error)
        {
            if (saved_errno != 0)
            {
                errno = saved_errno;
            }
            return total_written > 0 ? static_cast<ssize_t>(total_written) : -1;
        }

        return static_cast<ssize_t>(total_written);
#else
        return ::pwrite(fd, buffer, size, static_cast<off_t>(offset));
#endif
    }

    inline auto syncFd(int fd) -> int
    {
#if defined(_WIN32)
        return ::_commit(fd);
#else
        return ::fsync(fd);
#endif
    }
} // namespace scratchbird::core::platform
