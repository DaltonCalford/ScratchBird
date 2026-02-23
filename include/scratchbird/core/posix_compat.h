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

    // Windows headers and CRT compatibility headers may define these as macros,
    // which breaks member function names like Database::open/close/read/write.
    #ifdef open
        #undef open
    #endif
    #ifdef close
        #undef close
    #endif
    #ifdef read
        #undef read
    #endif
    #ifdef write
        #undef write
    #endif
    #ifdef lseek
        #undef lseek
    #endif
    #ifdef access
        #undef access
    #endif
    #ifdef mkdir
        #undef mkdir
    #endif

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

static inline auto open(const char* path, int flags) -> int
{
    return _open(path, flags);
}

static inline auto open(const char* path, int flags, int mode) -> int
{
    return _open(path, flags, mode);
}

static inline auto close(int fd) -> int
{
    return _close(fd);
}

static inline auto read(int fd, void* buffer, size_t count) -> ssize_t
{
    const int bytes = _read(fd, buffer, static_cast<unsigned int>(count));
    return bytes < 0 ? static_cast<ssize_t>(-1) : static_cast<ssize_t>(bytes);
}

static inline auto write(int fd, const void* buffer, size_t count) -> ssize_t
{
    const int bytes = _write(fd, buffer, static_cast<unsigned int>(count));
    return bytes < 0 ? static_cast<ssize_t>(-1) : static_cast<ssize_t>(bytes);
}

static inline auto lseek(int fd, std::int64_t offset, int origin) -> std::int64_t
{
    return _lseeki64(fd, offset, origin);
}

static inline auto mkdir(const char* path, int) -> int
{
    return _mkdir(path);
}

static inline auto access(const char* path, int mode) -> int
{
    return _access(path, mode);
}

    #define pread sb_pread
    #define pwrite sb_pwrite
    #define fsync sb_fsync
    #define ftruncate sb_ftruncate

#else
    #include <unistd.h>
#endif
