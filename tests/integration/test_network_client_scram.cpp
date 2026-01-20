#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <memory>
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

void dumpSysSchemaTables(ScratchBirdServer* server) {
    if (!server) {
        std::fprintf(stderr, "[catalog_dump] server unavailable\n");
        return;
    }
    auto* db = server->database();
    if (!db) {
        std::fprintf(stderr, "[catalog_dump] database unavailable\n");
        return;
    }
    auto* catalog = db->catalog_manager();
    if (!catalog) {
        std::fprintf(stderr, "[catalog_dump] catalog manager unavailable\n");
        return;
    }

    scratchbird::core::ErrorContext ctx;
    std::vector<scratchbird::core::CatalogManager::SchemaInfo> schemas;
    auto status = catalog->listSchemas(schemas, &ctx);
    if (status != Status::OK) {
        std::fprintf(stderr, "[catalog_dump] listSchemas failed: %d %s\n",
                     static_cast<int>(status), ctx.message.c_str());
        return;
    }

    scratchbird::core::CatalogManager::SchemaInfo sys_schema;
    bool found = false;
    for (const auto& schema : schemas) {
        if (schema.schema_name == "sys" || schema.schema_name == "SYS") {
            sys_schema = schema;
            found = true;
            break;
        }
    }

    if (!found) {
        std::fprintf(stderr, "[catalog_dump] sys schema not found (schemas=%zu)\n",
                     schemas.size());
        return;
    }

    std::vector<scratchbird::core::CatalogManager::TableInfo> tables;
    status = catalog->listTables(sys_schema.schema_id, tables, &ctx);
    if (status != Status::OK) {
        std::fprintf(stderr, "[catalog_dump] listTables(sys) failed: %d %s\n",
                     static_cast<int>(status), ctx.message.c_str());
        return;
    }

    const auto& policy_toast_id = catalog->policyToastTableId();
    std::fprintf(stderr, "[catalog_dump] policy_toast_table_id=%s\n",
                 policy_toast_id.toString().c_str());
    scratchbird::core::CatalogManager::TableInfo toast_info;
    scratchbird::core::ErrorContext toast_ctx;
    auto toast_status = catalog->getTable(policy_toast_id, toast_info, &toast_ctx);
    std::fprintf(stderr, "[catalog_dump] getTable(policy_toast_table_id) status=%d %s\n",
                 static_cast<int>(toast_status),
                 toast_ctx.message.empty() ? "ok" : toast_ctx.message.c_str());
    if (toast_status == Status::OK) {
        std::fprintf(stderr, "[catalog_dump] policy_toast name=%s id=%s type=%d\n",
                     toast_info.table_name.c_str(),
                     toast_info.table_id.toString().c_str(),
                     static_cast<int>(toast_info.table_type));
    }

    std::fprintf(stderr, "[catalog_dump] sys schema id=%s tables=%zu\n",
                 sys_schema.schema_id.toString().c_str(), tables.size());
    for (const auto& table : tables) {
        std::fprintf(stderr, "[catalog_dump] sys table name=%s id=%s type=%d\n",
                     table.table_name.c_str(),
                     table.table_id.toString().c_str(),
                     static_cast<int>(table.table_type));
    }
}

} // namespace

TEST(NetworkClientScramTest, ScramAuthSuccessAndFailure) {
    std::filesystem::create_directories("build/database");
    std::filesystem::create_directories("build/ipc_tests");

    std::string db_path = "build/database/native_listener_scram.sbdb";
    std::error_code ec;
    std::filesystem::remove(db_path, ec);

    ServerConfig server_cfg{};
    server_cfg.database_path = db_path;
    server_cfg.ipc_method = IPCMethod::UNIX_SOCKET;
    server_cfg.ipc_path = scratchbird::server::getIPCPath(db_path, server_cfg.ipc_method);
    std::filesystem::remove(server_cfg.ipc_path, ec);
    server_cfg.auto_create_db = true;
    server_cfg.accept_timeout_ms = 50;
    server_cfg.verbose = false;

    scratchbird::core::ErrorContext ctx;
    auto server = std::make_unique<ScratchBirdServer>(server_cfg);
    ASSERT_EQ(server->startAsync(&ctx), Status::OK) << ctx.message;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    dumpSysSchemaTables(server.get());

    client::Connection setup_conn;
    client::ConnectionConfig cc;
    cc.database_name = db_path;
    cc.ipc_method = IPCMethod::UNIX_SOCKET;
    cc.socket_path = server_cfg.ipc_path;
    cc.auto_start_server = false;
    cc.username = "SYSARCH";
    cc.password = "ScratchBirdBeta1!";
    ASSERT_EQ(setup_conn.connect(cc, &ctx), Status::OK) << ctx.message;
    ASSERT_EQ(setup_conn.execute("CREATE USER scram_user WITH PASSWORD 'Test1234!'", nullptr, &ctx),
              Status::OK) << ctx.message;
    setup_conn.disconnect();
    {
        auto* catalog = server->database() ? server->database()->catalog_manager() : nullptr;
        if (catalog) {
            scratchbird::core::CatalogManager::UserInfo user_info;
            scratchbird::core::ErrorContext user_ctx;
            auto user_status = catalog->getUserByName("scram_user", user_info, &user_ctx);
            std::fprintf(stderr, "[catalog_dump] getUser(scram_user) status=%d %s\n",
                         static_cast<int>(user_status),
                         user_ctx.message.empty() ? "ok" : user_ctx.message.c_str());
            if (user_status == Status::OK) {
                std::fprintf(stderr, "[catalog_dump] scram_user hash_len=%zu hash_prefix=%.*s\n",
                             user_info.password_hash.size(),
                             static_cast<int>(std::min<size_t>(80, user_info.password_hash.size())),
                             user_info.password_hash.c_str());
            }
        }
    }

    uint16_t port = reservePort();
    ASSERT_GT(port, 0u);

    std::string control_dir = "build/ipc_tests/native_listener_scram";
    std::filesystem::create_directories(control_dir);

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

    client::NetworkClientConfig nc;
    nc.host = "127.0.0.1";
    nc.port = port;
    nc.ssl_mode = scratchbird::network::SSLMode::DISABLED;
    nc.connect_timeout_ms = 2000;
    nc.read_timeout_ms = 5000;
    nc.write_timeout_ms = 5000;
    nc.database = db_path;
    nc.username = "scram_user";
    nc.password = "Test1234!";
    nc.auth_method = scratchbird::protocol::AuthMethod::SCRAM_SHA_256;

    client::NetworkClient ok_client;
    Status connect_status = Status::INTERNAL_ERROR;
    for (int attempt = 0; attempt < 50; ++attempt) {
        connect_status = ok_client.connect(nc, &ctx);
        if (connect_status == Status::OK) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    ASSERT_EQ(connect_status, Status::OK) << ctx.message;
    ok_client.disconnect();

    client::NetworkClient bad_client;
    nc.password = "wrong";
    Status bad_status = bad_client.connect(nc, &ctx);
    EXPECT_NE(bad_status, Status::OK);
    bad_client.disconnect();

    server->shutdown();
    server->waitForShutdown(2000);
}
