/**
 * Socket Types Implementation
 *
 * ScratchBird Network Layer - Phase 3.1
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
