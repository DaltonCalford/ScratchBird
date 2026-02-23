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

#ifdef _WIN32
    #include <BaseTsd.h>
    #include <cstddef>
    #include <cstdint>
    #include <direct.h>
    #include <fcntl.h>
    #include <io.h>
    #include <sys/types.h>
    #include <sys/stat.h>

using ssize_t = SSIZE_T;

    #ifndef O_BINARY
        #define O_BINARY 0
    #endif

    #ifndef S_ISDIR
        #define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
    #endif

    #ifndef S_ISREG
        #define S_ISREG(mode) (((mode) & _S_IFMT) == _S_IFREG)
    #endif

static inline auto sb_pread(int fd, void* buffer, size_t count, std::int64_t offset) -> ssize_t
{
    const auto current = _lseeki64(fd, 0, SEEK_CUR);
    if (current < 0)
    {
        return -1;
    }
    if (_lseeki64(fd, offset, SEEK_SET) < 0)
    {
        return -1;
    }
    const int bytes = _read(fd, buffer, static_cast<unsigned int>(count));
    (void)_lseeki64(fd, current, SEEK_SET);
    return bytes < 0 ? static_cast<ssize_t>(-1) : static_cast<ssize_t>(bytes);
}

static inline auto sb_pwrite(int fd, const void* buffer, size_t count, std::int64_t offset) -> ssize_t
{
    const auto current = _lseeki64(fd, 0, SEEK_CUR);
    if (current < 0)
    {
        return -1;
    }
    if (_lseeki64(fd, offset, SEEK_SET) < 0)
    {
        return -1;
    }
    const int bytes = _write(fd, buffer, static_cast<unsigned int>(count));
    (void)_lseeki64(fd, current, SEEK_SET);
    return bytes < 0 ? static_cast<ssize_t>(-1) : static_cast<ssize_t>(bytes);
}

static inline auto sb_fsync(int fd) -> int
{
    return _commit(fd);
}

static inline auto sb_ftruncate(int fd, int64_t size) -> int
{
    return _chsize_s(fd, size) == 0 ? 0 : -1;
}

    #define pread sb_pread
    #define pwrite sb_pwrite
    #define fsync sb_fsync
    #define ftruncate sb_ftruncate
    #define open _open
    #define close _close
    #define read _read
    #define write _write
    #define lseek _lseeki64
    #define mkdir(path, mode) _mkdir(path)
    #define access _access

#else
    #include <unistd.h>
#endif
