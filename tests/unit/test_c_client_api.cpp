#include <arpa/inet.h>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include "scratchbird/client/connection.h"
#include "scratchbird/client/scratchbird_client.h"
#include "scratchbird/server/scratchbird_server.h"
#include "scratchbird/server/ipc_server.h"
#include "test_helpers.h"

namespace {
uint16_t reservePort() {
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return 0;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(0);
    if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        ::close(sock);
        return 0;
    }
    socklen_t len = sizeof(addr);
    if (::getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
        ::close(sock);
        return 0;
    }
    ::close(sock);
    return ntohs(addr.sin_port);
}

class ProcessGroupGuard {
public:
    explicit ProcessGroupGuard(pid_t pid) : pid_(pid) {}
    ProcessGroupGuard(const ProcessGroupGuard&) = delete;
    ProcessGroupGuard& operator=(const ProcessGroupGuard&) = delete;
    ProcessGroupGuard(ProcessGroupGuard&& other) noexcept {
        pid_ = other.pid_;
        other.pid_ = -1;
    }
    ProcessGroupGuard& operator=(ProcessGroupGuard&& other) noexcept {
        if (this != &other) {
            cleanup();
            pid_ = other.pid_;
            other.pid_ = -1;
        }
        return *this;
    }
    ~ProcessGroupGuard() {
        cleanup();
    }

private:
    void cleanup() {
        if (pid_ <= 0) {
            return;
        }
        kill(-pid_, SIGTERM);
        int status = 0;
        for (int i = 0; i < 50; ++i) {
            pid_t result = waitpid(pid_, &status, WNOHANG);
            if (result == pid_) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        kill(-pid_, SIGKILL);
        (void)waitpid(pid_, &status, 0);
        pid_ = -1;
    }

    pid_t pid_{-1};
};

void prependBuildPathToEnv() {
    const char* path_env = std::getenv("PATH");
    auto build_dir = std::filesystem::current_path().parent_path();
    auto build_src = build_dir / "src";
    std::string new_path = build_src.string() + ":" + build_dir.string();
    if (path_env && path_env[0] != '\0') {
        new_path += ":";
        new_path += path_env;
    }
    setenv("PATH", new_path.c_str(), 1);
}
} // namespace

TEST(CClientApi, NullInputs) {
    sb_error err{};
    auto* conn = sb_connect(nullptr, &err);
    EXPECT_EQ(conn, nullptr);
    EXPECT_NE(err.code, SB_OK);

    auto* result = sb_execute(nullptr, "SELECT 1", &err);
    EXPECT_EQ(result, nullptr);
    EXPECT_NE(err.code, SB_OK);

    sb_row row{};
    int fetch_status = sb_fetch(nullptr, &row, &err);
    EXPECT_NE(fetch_status, SB_OK);
}

TEST(CClientApi, IntegrationSelect) {
    const char* dsn = std::getenv("SCRATCHBIRD_C_API_URL");
    std::string local_dsn;
    std::unique_ptr<scratchbird::server::ScratchBirdServer> server;
    std::string db_name;
    std::string db_path;
    std::string control_dir;
    std::string engine_sock;
    std::optional<ProcessGroupGuard> listener_guard;
    if (!dsn || !*dsn) {
        if (!scratchbird::testing::networkTestsEnabled()) {
            GTEST_SKIP() << "Network tests disabled; set SCRATCHBIRD_TEST_NETWORK=1 to enable.";
        }
        std::filesystem::create_directories("build/database");
        std::filesystem::create_directories("build/ipc_tests");

        db_name = "c_api_integration";
        db_path = "build/database/" + db_name + ".sbdb";
        control_dir = "build/ipc_tests/c_api_control";
        std::filesystem::create_directories(control_dir);
        engine_sock = (std::filesystem::current_path() / control_dir / "engine.sock").string();
        std::error_code ec;
        std::filesystem::remove(db_path, ec);
        std::filesystem::remove(engine_sock, ec);

        scratchbird::server::ServerConfig server_cfg{};
        server_cfg.database_path = db_path;
        server_cfg.ipc_method = scratchbird::server::IPCMethod::UNIX_SOCKET;
        server_cfg.ipc_path = engine_sock;
        server_cfg.auto_create_db = true;
        server_cfg.accept_timeout_ms = 50;
        server_cfg.verbose = false;

        scratchbird::core::ErrorContext ctx;
        server = std::make_unique<scratchbird::server::ScratchBirdServer>(server_cfg);
        ASSERT_EQ(server->startAsync(&ctx), scratchbird::core::Status::OK) << ctx.message;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        scratchbird::client::Connection setup_conn;
        scratchbird::client::ConnectionConfig cc;
        cc.database_name = db_name;
        cc.ipc_method = scratchbird::server::IPCMethod::UNIX_SOCKET;
        cc.socket_path = engine_sock;
        cc.auto_start_server = false;
        cc.username = "SYSARCH";
        cc.password = "ScratchBirdBeta1!";
        ASSERT_EQ(setup_conn.connect(cc, &ctx), scratchbird::core::Status::OK) << ctx.message;
        setup_conn.disconnect();

        uint16_t port = reservePort();
        ASSERT_GT(port, 0u);
        prependBuildPathToEnv();

        pid_t pid = fork();
        ASSERT_GE(pid, 0);
        if (pid == 0) {
            setpgid(0, 0);
            std::string port_str = std::to_string(port);
            execlp("sb_listener_native",
                   "sb_listener_native",
                   "--bind", "127.0.0.1",
                   "--port", port_str.c_str(),
                   "--control-socket-dir", control_dir.c_str(),
                   "--engine-endpoint", engine_sock.c_str(),
                   "--pool-min", "1",
                   "--pool-max", "1",
                   "--spawn-strategy", "prefork",
                   "--log-level", "error",
                   nullptr);
            _exit(127);
        }
        listener_guard.emplace(pid);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        local_dsn = "host=127.0.0.1;port=" + std::to_string(port) +
                    ";database=" + db_name +
                    ";user=SYSARCH;password=ScratchBirdBeta1!;sslmode=disable;"
                    "auth_method=password;connect_timeout=2;read_timeout=5;write_timeout=5";
        dsn = local_dsn.c_str();
    }
    sb_error err{};
    sb_connection* conn = nullptr;
    for (int attempt = 0; attempt < 10 && !conn; ++attempt) {
        conn = sb_connect(dsn, &err);
        if (!conn) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    ASSERT_NE(conn, nullptr) << err.message;

    auto* result = sb_execute(conn, "SELECT 1", &err);
    ASSERT_NE(result, nullptr) << err.message;

    sb_row row{};
    ASSERT_EQ(sb_fetch(result, &row, &err), SB_OK);
    int64_t value = 0;
    EXPECT_EQ(sb_get_int64(&row, 0, &value), SB_OK);
    EXPECT_EQ(value, 1);

    sb_result_free(result);
    sb_disconnect(conn);

    if (server) {
        server->shutdown();
        server->waitForShutdown(2000);
    }
}
