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
    #include <stdlib.h>
    #include <sys/types.h>
    #include <sys/stat.h>

using ssize_t = SSIZE_T;

    // MSVC does not expose several POSIX scalar types used by shared headers.
    // Provide fallback aliases when the corresponding CRT typedef macros are absent.
    #if !defined(_MODE_T_DEFINED) && !defined(_MODE_T_) && !defined(__mode_t_defined)
using mode_t = unsigned short;
    #define _MODE_T_DEFINED
    #endif
    #if !defined(_PID_T_DEFINED) && !defined(_PID_T_) && !defined(__pid_t_defined)
using pid_t = int;
    #define _PID_T_DEFINED
    #endif
    #if !defined(_UID_T_DEFINED) && !defined(_UID_T_) && !defined(__uid_t_defined)
using uid_t = unsigned int;
    #define _UID_T_DEFINED
    #endif
    #if !defined(_GID_T_DEFINED) && !defined(_GID_T_) && !defined(__gid_t_defined)
using gid_t = unsigned int;
    #define _GID_T_DEFINED
    #endif

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

static inline auto sb_open(const char* path, int flags) -> int
{
    return _open(path, flags);
}

static inline auto sb_open(const char* path, int flags, int mode) -> int
{
    return _open(path, flags, mode);
}

static inline auto sb_close(int fd) -> int
{
    return _close(fd);
}

static inline auto sb_read(int fd, void* buffer, size_t count) -> ssize_t
{
    const int bytes = _read(fd, buffer, static_cast<unsigned int>(count));
    return bytes < 0 ? static_cast<ssize_t>(-1) : static_cast<ssize_t>(bytes);
}

static inline auto sb_write(int fd, const void* buffer, size_t count) -> ssize_t
{
    const int bytes = _write(fd, buffer, static_cast<unsigned int>(count));
    return bytes < 0 ? static_cast<ssize_t>(-1) : static_cast<ssize_t>(bytes);
}

static inline auto sb_lseek(int fd, std::int64_t offset, int origin) -> std::int64_t
{
    return _lseeki64(fd, offset, origin);
}

static inline auto sb_mkdir(const char* path, int) -> int
{
    return _mkdir(path);
}

static inline auto sb_access(const char* path, int mode) -> int
{
    return _access(path, mode);
}

static inline auto sb_realpath(const char* path, char* resolved_path) -> char*
{
    if (resolved_path == nullptr)
    {
        return _fullpath(nullptr, path, _MAX_PATH);
    }
    return _fullpath(resolved_path, path, _MAX_PATH);
}

    #define pread sb_pread
    #define pwrite sb_pwrite
    #define fsync sb_fsync
    #define ftruncate sb_ftruncate

#else
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <unistd.h>

static inline auto sb_open(const char* path, int flags) -> int
{
    return ::open(path, flags);
}

static inline auto sb_open(const char* path, int flags, int mode) -> int
{
    return ::open(path, flags, mode);
}

static inline auto sb_close(int fd) -> int
{
    return ::close(fd);
}

static inline auto sb_read(int fd, void* buffer, size_t count) -> ssize_t
{
    return ::read(fd, buffer, count);
}

static inline auto sb_write(int fd, const void* buffer, size_t count) -> ssize_t
{
    return ::write(fd, buffer, count);
}

static inline auto sb_lseek(int fd, std::int64_t offset, int origin) -> std::int64_t
{
    return ::lseek(fd, offset, origin);
}

static inline auto sb_mkdir(const char* path, int mode) -> int
{
    return ::mkdir(path, static_cast<mode_t>(mode));
}

static inline auto sb_access(const char* path, int mode) -> int
{
    return ::access(path, mode);
}

static inline auto sb_realpath(const char* path, char* resolved_path) -> char*
{
    return ::realpath(path, resolved_path);
}
#endif
