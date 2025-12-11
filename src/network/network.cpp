/**
 * Network Subsystem Implementation
 *
 * ScratchBird Network Layer - Phase 3.1
 *
 * Initialization and cleanup for the network subsystem.
 */

#include "scratchbird/network/network.h"

#include <atomic>
#include <mutex>

#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#endif

namespace scratchbird {
namespace network {

namespace {
    std::atomic<bool> g_network_initialized{false};
    std::mutex g_init_mutex;
}

bool initNetwork() {
    std::lock_guard<std::mutex> lock(g_init_mutex);

    if (g_network_initialized.load()) {
        return true;  // Already initialized
    }

#ifdef _WIN32
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        return false;
    }
#endif

    g_network_initialized.store(true);
    return true;
}

void cleanupNetwork() {
    std::lock_guard<std::mutex> lock(g_init_mutex);

    if (!g_network_initialized.load()) {
        return;  // Not initialized
    }

#ifdef _WIN32
    WSACleanup();
#endif

    g_network_initialized.store(false);
}

bool isNetworkInitialized() {
    return g_network_initialized.load();
}

} // namespace network
} // namespace scratchbird
