#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "scratchbird/client/connection.h"
#include "scratchbird/client/network_client.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"
#include "scratchbird/network/network.h"
#include "scratchbird/network/socket.h"
#include "scratchbird/server/ipc_server.h"
#include "scratchbird/server/scratchbird_server.h"

using scratchbird::core::Status;
using scratchbird::server::IPCMethod;
using scratchbird::server::ScratchBirdServer;
using scratchbird::server::ServerConfig;
namespace client = scratchbird::client;

namespace {

class ProcessGroupGuard {
public:
    explicit ProcessGroupGuard(pid_t pid) : pid_(pid) {}
    ~ProcessGroupGuard() {
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
    }

private:
    pid_t pid_{-1};
};

uint16_t reservePort() {
    scratchbird::network::NetworkInitGuard guard;
    auto sock = scratchbird::network::Socket::create(
        scratchbird::network::AddressFamily::IPV4);
    if (!sock) {
        return 0;
    }
    scratchbird::core::ErrorContext ctx;
    scratchbird::network::NetworkAddress addr("127.0.0.1", 0);
    if (sock->bind(addr, &ctx) != Status::OK) {
        return 0;
    }
    auto local = sock->getLocalAddress();
    if (!local.has_value()) {
        return 0;
    }
    return local->port;
}

} // namespace

TEST(NativeListenerSmokeTest, EndToEndQueryAndCopyOverIPC) {
    std::filesystem::create_directories("build/database");
    std::filesystem::create_directories("build/ipc_tests");

    std::string db_name = "native_listener_smoke";
    std::string db_path = "build/database/" + db_name + ".sbdb";
    std::string control_dir = "build/ipc_tests/native_listener_smoke";
    std::filesystem::create_directories(control_dir);
    std::string engine_sock =
        (std::filesystem::current_path() / control_dir / "engine.sock").string();
    std::error_code ec;
    std::filesystem::remove(db_path, ec);

    ServerConfig server_cfg{};
    server_cfg.database_path = db_path;
    server_cfg.ipc_method = IPCMethod::UNIX_SOCKET;
    server_cfg.ipc_path = engine_sock;
    std::filesystem::remove(server_cfg.ipc_path, ec);
    server_cfg.auto_create_db = true;
    server_cfg.accept_timeout_ms = 50;
    server_cfg.verbose = false;

    scratchbird::core::ErrorContext ctx;
    auto server = std::make_unique<ScratchBirdServer>(server_cfg);
    ASSERT_EQ(server->startAsync(&ctx), Status::OK) << ctx.message;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    client::Connection setup_conn;
    client::ConnectionConfig cc;
    cc.database_name = db_name;
    cc.ipc_method = IPCMethod::UNIX_SOCKET;
    cc.socket_path = server_cfg.ipc_path;
    cc.auto_start_server = false;
    cc.username = "SYSARCH";
    cc.password = "ScratchBirdBeta1!";
    ASSERT_EQ(setup_conn.connect(cc, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(setup_conn.execute("CREATE TABLE IF NOT EXISTS copy_smoke(id INT, name TEXT)", nullptr, &ctx), Status::OK);
    ASSERT_EQ(setup_conn.execute("DELETE FROM copy_smoke", nullptr, &ctx), Status::OK);
    setup_conn.disconnect();

    uint16_t port = reservePort();
    ASSERT_GT(port, 0u);

    const char* path_env = std::getenv("PATH");
    auto build_dir = std::filesystem::current_path().parent_path();
    auto build_src = build_dir / "src";
    std::string new_path = build_src.string() + ":" + build_dir.string() +
        (path_env ? ":" + std::string(path_env) : "");
    setenv("PATH", new_path.c_str(), 1);

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
               "--engine-endpoint", server_cfg.ipc_path.c_str(),
               "--pool-min", "1",
               "--pool-max", "1",
               "--spawn-strategy", "prefork",
               "--log-level", "error",
               nullptr);
        _exit(127);
    }
    ProcessGroupGuard listener_guard(pid);

    scratchbird::client::NetworkClient client;
    scratchbird::client::NetworkClientConfig nc;
    nc.host = "127.0.0.1";
    nc.port = port;
    nc.ssl_mode = scratchbird::network::SSLMode::DISABLED;
    nc.connect_timeout_ms = 2000;
    nc.read_timeout_ms = 5000;
    nc.write_timeout_ms = 5000;
    nc.database = db_name;
    nc.username = "SYSARCH";
    nc.password = "ScratchBirdBeta1!";
    nc.auth_method = scratchbird::protocol::AuthMethod::PASSWORD;

    Status connect_status = Status::INTERNAL_ERROR;
    for (int attempt = 0; attempt < 50; ++attempt) {
        connect_status = client.connect(nc, &ctx);
        if (connect_status == Status::OK) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    ASSERT_EQ(connect_status, Status::OK) << ctx.message;

    scratchbird::client::NetworkResultSet results;
    ASSERT_EQ(client.executeQuery("SELECT 1", results, &ctx), Status::OK) << ctx.message;
    ASSERT_FALSE(results.rows.empty());

    ASSERT_EQ(client.executeQuery("DELETE FROM copy_smoke", results, &ctx), Status::OK);
    std::istringstream copy_in("1\talpha\n2\tbeta\n");
    client.setCopyInputStream(&copy_in);
    ASSERT_EQ(client.executeQuery("COPY copy_smoke FROM STDIN", results, &ctx), Status::OK) << ctx.message;
    client.setCopyInputStream(nullptr);

    ASSERT_EQ(client.executeQuery("SELECT id FROM copy_smoke ORDER BY id", results, &ctx),
              Status::OK) << ctx.message;
    ASSERT_EQ(results.rows.size(), 2u);

    std::ostringstream copy_out;
    client.setCopyOutputStream(&copy_out);
    ASSERT_EQ(client.executeQuery("COPY copy_smoke TO STDOUT", results, &ctx), Status::OK) << ctx.message;
    client.setCopyOutputStream(nullptr);
    const std::string copy_text = copy_out.str();
    EXPECT_NE(copy_text.find("alpha"), std::string::npos);
    EXPECT_NE(copy_text.find("beta"), std::string::npos);

    client.disconnect();

    server->shutdown();
    server->waitForShutdown(2000);
}
