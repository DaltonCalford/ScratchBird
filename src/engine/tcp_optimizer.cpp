#include "scratchbird/engine/tcp_optimizer.h"

#include <cstring>
#include <mutex>
#include <sstream>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")
#endif

namespace ScratchBird
{

    std::error_code TCPOptimizer::configure_socket(int sockfd, const NetworkConfig& config)
    {
        if (sockfd < 0) {
            return std::make_error_code(std::errc::invalid_argument);
        }

        // Apply core TCP optimizations
        if (auto ec = set_tcp_nodelay(sockfd, config.tcp_nodelay); ec) {
            return ec;
        }

        if (auto ec = set_keepalive_options(sockfd, config); ec) {
            return ec;
        }

        if (auto ec = set_buffer_sizes(sockfd, config); ec) {
            return ec;
        }

        // Apply platform-specific optimizations
        return set_platform_specific_options(sockfd, config);
    }

    std::error_code TCPOptimizer::configure_listen_socket(int sockfd, const NetworkConfig& config)
    {
        // Apply base socket configuration
        if (auto ec = configure_socket(sockfd, config); ec) {
            return ec;
        }

        // Enable address reuse for server sockets
        int reuse = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse),
                       sizeof(reuse)) < 0) {
            return get_socket_error();
        }

#ifdef SO_REUSEPORT
        // Enable port reuse on Linux for better load balancing
        if (config.reuse_port) {
            int reuse_port = 1;
            if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT,
                           reinterpret_cast<const char*>(&reuse_port), sizeof(reuse_port)) < 0) {
                // Non-fatal error, continue
            }
        }
#endif

        return {};
    }

    std::error_code TCPOptimizer::configure_client_socket(int sockfd, const NetworkConfig& config)
    {
        // Apply base socket configuration
        return configure_socket(sockfd, config);
    }

    NetworkConfig TCPOptimizer::get_socket_config(int sockfd) const
    {
        NetworkConfig config;

        if (sockfd < 0) {
            return config;
        }

        // Query TCP_NODELAY
        int nodelay = 0;
        socklen_t len = sizeof(nodelay);
        if (getsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&nodelay), &len) ==
            0) {
            config.tcp_nodelay = (nodelay != 0);
        }

        // Query SO_KEEPALIVE
        int keepalive = 0;
        len = sizeof(keepalive);
        if (getsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<char*>(&keepalive),
                       &len) == 0) {
            // Keepalive is enabled, try to get parameters
#ifdef TCP_KEEPIDLE
            int idle = 0;
            len = sizeof(idle);
            if (getsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, reinterpret_cast<char*>(&idle),
                           &len) == 0) {
                config.tcp_keepalive_idle = std::chrono::seconds(idle);
            }
#endif

#ifdef TCP_KEEPINTVL
            int interval = 0;
            len = sizeof(interval);
            if (getsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, reinterpret_cast<char*>(&interval),
                           &len) == 0) {
                config.tcp_keepalive_interval = std::chrono::seconds(interval);
            }
#endif

#ifdef TCP_KEEPCNT
            int count = 0;
            len = sizeof(count);
            if (getsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, reinterpret_cast<char*>(&count),
                           &len) == 0) {
                config.tcp_keepalive_count = count;
            }
#endif
        }

        // Query buffer sizes
        int rcvbuf = 0;
        len = sizeof(rcvbuf);
        if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&rcvbuf), &len) ==
            0) {
            config.socket_recv_buffer = static_cast<size_t>(rcvbuf);
        }

        int sndbuf = 0;
        len = sizeof(sndbuf);
        if (getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&sndbuf), &len) ==
            0) {
            config.socket_send_buffer = static_cast<size_t>(sndbuf);
        }

        return config;
    }

    std::string TCPOptimizer::validate_config(const NetworkConfig& config)
    {
        std::ostringstream errors;

        if (config.tcp_keepalive_idle.count() <= 0) {
            errors << "tcp_keepalive_idle must be positive; ";
        }

        if (config.tcp_keepalive_interval.count() <= 0) {
            errors << "tcp_keepalive_interval must be positive; ";
        }

        if (config.tcp_keepalive_count <= 0) {
            errors << "tcp_keepalive_count must be positive; ";
        }

        if (config.socket_recv_buffer < 4096) {
            errors << "socket_recv_buffer should be at least 4KB; ";
        }

        if (config.socket_send_buffer < 4096) {
            errors << "socket_send_buffer should be at least 4KB; ";
        }

        if (config.listen_backlog <= 0 || config.listen_backlog > 65535) {
            errors << "listen_backlog must be between 1 and 65535; ";
        }

        return errors.str();
    }

    std::string TCPOptimizer::get_platform_capabilities()
    {
        std::ostringstream caps;

#ifdef _WIN32
        caps << "Windows TCP optimizations: ";
        caps << "TCP_NODELAY, SO_KEEPALIVE, SIO_KEEPALIVE_VALS, ";
        caps << "SO_RCVBUF, SO_SNDBUF, SO_REUSEADDR";
#else
        caps << "Unix/Linux TCP optimizations: ";
        caps << "TCP_NODELAY, SO_KEEPALIVE, TCP_KEEPIDLE, TCP_KEEPINTVL, TCP_KEEPCNT, ";
        caps << "SO_RCVBUF, SO_SNDBUF, SO_REUSEADDR";

#ifdef SO_REUSEPORT
        caps << ", SO_REUSEPORT";
#endif
#ifdef TCP_USER_TIMEOUT
        caps << ", TCP_USER_TIMEOUT";
#endif
#ifdef TCP_DEFER_ACCEPT
        caps << ", TCP_DEFER_ACCEPT";
#endif
#ifdef TCP_FASTOPEN
        caps << ", TCP_FASTOPEN";
#endif
#endif

        return caps.str();
    }

    std::error_code TCPOptimizer::set_tcp_nodelay(int sockfd, bool enable)
    {
        int value = enable ? 1 : 0;
        if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&value),
                       sizeof(value)) < 0) {
            return get_socket_error();
        }
        return {};
    }

    std::error_code TCPOptimizer::set_keepalive_options(int sockfd, const NetworkConfig& config)
    {
        // Enable SO_KEEPALIVE
        int keepalive = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&keepalive),
                       sizeof(keepalive)) < 0) {
            return get_socket_error();
        }

#ifdef _WIN32
        // Windows uses SIO_KEEPALIVE_VALS
        struct tcp_keepalive ka;
        ka.onoff = 1;
        ka.keepalivetime = static_cast<ULONG>(config.tcp_keepalive_idle.count() * 1000);
        ka.keepaliveinterval = static_cast<ULONG>(config.tcp_keepalive_interval.count() * 1000);

        DWORD returned = 0;
        if (WSAIoctl(sockfd, SIO_KEEPALIVE_VALS, &ka, sizeof(ka), nullptr, 0, &returned, nullptr,
                     nullptr) == SOCKET_ERROR) {
            return get_socket_error();
        }
#else
        // Unix/Linux keepalive parameters
#ifdef TCP_KEEPIDLE
        int idle = static_cast<int>(config.tcp_keepalive_idle.count());
        if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, reinterpret_cast<const char*>(&idle),
                       sizeof(idle)) < 0) {
            return get_socket_error();
        }
#endif

#ifdef TCP_KEEPINTVL
        int interval = static_cast<int>(config.tcp_keepalive_interval.count());
        if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, reinterpret_cast<const char*>(&interval),
                       sizeof(interval)) < 0) {
            return get_socket_error();
        }
#endif

#ifdef TCP_KEEPCNT
        int count = config.tcp_keepalive_count;
        if (setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, reinterpret_cast<const char*>(&count),
                       sizeof(count)) < 0) {
            return get_socket_error();
        }
#endif

#ifdef TCP_USER_TIMEOUT
        // Set TCP user timeout if specified
        if (config.tcp_user_timeout.count() > 0) {
            unsigned int timeout_ms = static_cast<unsigned int>(
                std::chrono::duration_cast<std::chrono::milliseconds>(config.tcp_user_timeout)
                    .count());
            if (setsockopt(sockfd, IPPROTO_TCP, TCP_USER_TIMEOUT,
                           reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms)) < 0) {
                // Non-fatal error
            }
        }
#endif
#endif

        return {};
    }

    std::error_code TCPOptimizer::set_buffer_sizes(int sockfd, const NetworkConfig& config)
    {
        // Set receive buffer size
        int rcvbuf = static_cast<int>(config.socket_recv_buffer);
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvbuf),
                       sizeof(rcvbuf)) < 0) {
            return get_socket_error();
        }

        // Set send buffer size
        int sndbuf = static_cast<int>(config.socket_send_buffer);
        if (setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&sndbuf),
                       sizeof(sndbuf)) < 0) {
            return get_socket_error();
        }

        return {};
    }

    std::error_code TCPOptimizer::set_platform_specific_options(int sockfd,
                                                                const NetworkConfig& config)
    {
#ifdef _WIN32
        return configure_windows_options(sockfd, config);
#else
        return configure_linux_options(sockfd, config);
#endif
    }

    std::error_code TCPOptimizer::configure_linux_options(int sockfd, const NetworkConfig& config)
    {
#ifdef TCP_DEFER_ACCEPT
        // Enable TCP_DEFER_ACCEPT for server sockets to reduce context switches
        if (config.enable_defer_accept) {
            int defer_accept = 1;
            if (setsockopt(sockfd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &defer_accept,
                           sizeof(defer_accept)) < 0) {
                // Non-fatal error, continue
            }
        }
#endif

#ifdef TCP_FASTOPEN
        // Enable TCP Fast Open if requested
        if (config.enable_fast_open) {
            int fastopen = 1;
            if (setsockopt(sockfd, IPPROTO_TCP, TCP_FASTOPEN, &fastopen, sizeof(fastopen)) < 0) {
                // Non-fatal error, continue
            }
        }
#endif

        return {};
    }

    std::error_code TCPOptimizer::configure_windows_options(int /*sockfd*/,
                                                            const NetworkConfig& /*config*/)
    {
        // Windows-specific optimizations can be added here
        // For now, the basic options (TCP_NODELAY, keepalive, buffers) are sufficient
        return {};
    }

    std::error_code TCPOptimizer::get_socket_error() const
    {
#ifdef _WIN32
        int error = WSAGetLastError();
        return std::error_code(error, std::system_category());
#else
        return std::error_code(errno, std::system_category());
#endif
    }

    // OptimizedSocket implementation
    OptimizedSocket::OptimizedSocket(int sockfd, const NetworkConfig& config)
        : sockfd_(sockfd), config_(config)
    {
        if (sockfd_ >= 0) {
            optimizer_.configure_socket(sockfd_, config_);
        }
    }

    OptimizedSocket::~OptimizedSocket()
    {
        if (sockfd_ >= 0) {
#ifdef _WIN32
            closesocket(sockfd_);
#else
            close(sockfd_);
#endif
        }
    }

    OptimizedSocket::OptimizedSocket(OptimizedSocket&& other) noexcept
        : sockfd_(other.sockfd_), config_(other.config_), optimizer_(std::move(other.optimizer_))
    {
        other.sockfd_ = -1;
    }

    OptimizedSocket& OptimizedSocket::operator=(OptimizedSocket&& other) noexcept
    {
        if (this != &other) {
            if (sockfd_ >= 0) {
#ifdef _WIN32
                closesocket(sockfd_);
#else
                close(sockfd_);
#endif
            }

            sockfd_ = other.sockfd_;
            config_ = other.config_;
            optimizer_ = std::move(other.optimizer_);
            other.sockfd_ = -1;
        }
        return *this;
    }

    std::error_code OptimizedSocket::reconfigure(const NetworkConfig& new_config)
    {
        config_ = new_config;
        if (sockfd_ >= 0) {
            return optimizer_.configure_socket(sockfd_, config_);
        }
        return {};
    }

    int OptimizedSocket::release()
    {
        int sockfd = sockfd_;
        sockfd_ = -1;
        return sockfd;
    }

    // NetworkStatsCollector implementation
    void NetworkStatsCollector::record_connection_accepted(std::chrono::nanoseconds connection_time)
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.connections_accepted++;

        // Update average connection time using exponential moving average
        if (stats_.connections_accepted == 1) {
            stats_.avg_connection_time = connection_time;
        } else {
            stats_.avg_connection_time = std::chrono::nanoseconds(
                (stats_.avg_connection_time.count() * 7 + connection_time.count()) / 8);
        }
    }

    void NetworkStatsCollector::record_connection_failed()
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.connections_failed++;
    }

    void NetworkStatsCollector::record_bytes_sent(uint64_t bytes, std::chrono::nanoseconds latency)
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.bytes_sent += bytes;

        // Update average send latency
        if (stats_.bytes_sent == bytes) {
            stats_.avg_send_latency = latency;
        } else {
            stats_.avg_send_latency = std::chrono::nanoseconds(
                (stats_.avg_send_latency.count() * 7 + latency.count()) / 8);
        }
    }

    void NetworkStatsCollector::record_bytes_received(uint64_t bytes,
                                                      std::chrono::nanoseconds latency)
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.bytes_received += bytes;

        // Update average receive latency
        if (stats_.bytes_received == bytes) {
            stats_.avg_recv_latency = latency;
        } else {
            stats_.avg_recv_latency = std::chrono::nanoseconds(
                (stats_.avg_recv_latency.count() * 7 + latency.count()) / 8);
        }
    }

    void NetworkStatsCollector::record_keepalive_timeout()
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.keepalive_timeouts++;
    }

    void NetworkStatsCollector::record_socket_error()
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.socket_errors++;
    }

    NetworkStats NetworkStatsCollector::get_stats() const
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        return stats_;
    }

    void NetworkStatsCollector::reset_stats()
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_ = NetworkStats{};
    }

} // namespace ScratchBird
