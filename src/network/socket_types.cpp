/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * Socket Types Implementation
 *
 * Cross-platform socket type/error helpers.
 */

#include "scratchbird/network/socket_types.h"

#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
#else
    #include <cerrno>
#endif

namespace scratchbird {
namespace network {

std::string getSocketErrorString(int error_code) {
#ifdef _WIN32
    char* msg = nullptr;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&msg,
        0,
        nullptr
    );
    std::string result = msg ? msg : "Unknown error";
    if (msg) LocalFree(msg);
    // Remove trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    return result;
#else
    return std::strerror(error_code);
#endif
}

} // namespace network
} // namespace scratchbird
