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

#include <cstddef>

#ifdef _WIN32
#include <BaseTsd.h>
#include <winsock2.h>
#include <ws2tcpip.h>
using sb_socket_ssize_t = SSIZE_T;
#ifndef MSG_WAITALL
#define MSG_WAITALL 0
#endif

inline sb_socket_ssize_t sb_socket_send(int fd, const void* buffer, size_t length, int flags) {
    return static_cast<sb_socket_ssize_t>(
        ::send(static_cast<SOCKET>(fd), reinterpret_cast<const char*>(buffer), static_cast<int>(length), flags));
}

inline sb_socket_ssize_t sb_socket_recv(int fd, void* buffer, size_t length, int flags) {
    return static_cast<sb_socket_ssize_t>(
        ::recv(static_cast<SOCKET>(fd), reinterpret_cast<char*>(buffer), static_cast<int>(length), flags));
}

inline int sb_socket_setsockopt(int fd, int level, int optname, const void* optval, int optlen) {
    return ::setsockopt(static_cast<SOCKET>(fd), level, optname, reinterpret_cast<const char*>(optval), optlen);
}

inline int sb_socket_getsockopt(int fd, int level, int optname, void* optval, int* optlen) {
    return ::getsockopt(static_cast<SOCKET>(fd), level, optname, reinterpret_cast<char*>(optval), optlen);
}
#else
#include <sys/socket.h>
#include <unistd.h>
using sb_socket_ssize_t = ssize_t;

inline sb_socket_ssize_t sb_socket_send(int fd, const void* buffer, size_t length, int flags) {
    return ::send(fd, buffer, length, flags);
}

inline sb_socket_ssize_t sb_socket_recv(int fd, void* buffer, size_t length, int flags) {
    return ::recv(fd, buffer, length, flags);
}

inline int sb_socket_setsockopt(int fd, int level, int optname, const void* optval, int optlen) {
    return ::setsockopt(fd, level, optname, optval, static_cast<socklen_t>(optlen));
}

inline int sb_socket_getsockopt(int fd, int level, int optname, void* optval, int* optlen) {
    return ::getsockopt(fd, level, optname, optval, reinterpret_cast<socklen_t*>(optlen));
}
#endif
