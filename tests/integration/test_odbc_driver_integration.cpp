#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "test_helpers.h"
#include "scratchbird/odbc/odbc_driver.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/network/network.h"
#include "scratchbird/network/socket.h"
#include "scratchbird/server/ipc_server.h"
#include "scratchbird/server/scratchbird_server.h"

using scratchbird::core::Status;
using scratchbird::server::IPCMethod;
using scratchbird::server::ScratchBirdServer;
using scratchbird::server::ServerConfig;
using namespace scratchbird::odbc;

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

}  // namespace

TEST(OdbcDriverIntegrationTest, ConnectPrepareExecuteFetch) {
    std::string db_path = scratchbird::testing::uniqueTestDbPath("odbc_integration", ".sbdb");
    std::error_code ec;
    std::filesystem::remove(db_path, ec);

    uint16_t ipc_port = reservePort();
    ASSERT_GT(ipc_port, 0u);

    ServerConfig server_cfg{};
    server_cfg.database_path = db_path;
    server_cfg.ipc_method = IPCMethod::TCP_LOCALHOST;
    server_cfg.tcp_port = ipc_port;
    server_cfg.ipc_path = "127.0.0.1:" + std::to_string(ipc_port);
    server_cfg.auto_create_db = true;
    server_cfg.accept_timeout_ms = 50;
    server_cfg.verbose = false;

    scratchbird::core::ErrorContext ctx;
    auto server = std::make_unique<ScratchBirdServer>(server_cfg);
    ASSERT_EQ(server->startAsync(&ctx), Status::OK) << ctx.message;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    uint16_t port = reservePort();
    ASSERT_GT(port, 0u);

    std::string control_dir = scratchbird::testing::uniqueTestDirPath("sb_odbc_ctl");
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

    std::fprintf(stderr, "[odbc_test] listener started on port %u\n", port);

    SQLHENV env = SQL_NULL_HENV;
    SQLHDBC dbc = SQL_NULL_HDBC;
    SQLHSTMT stmt = SQL_NULL_HSTMT;

    std::fprintf(stderr, "[odbc_test] allocating handles\n");
    ASSERT_EQ(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env), SQL_SUCCESS);
    ASSERT_EQ(SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION,
                            reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0), SQL_SUCCESS);
    ASSERT_EQ(SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc), SQL_SUCCESS);

    std::string conn_str = "Driver={ScratchBird};Server=127.0.0.1;Port=" +
        std::to_string(port) + ";Database=" + db_path +
        ";UID=SYSARCH;PWD=ScratchBirdBeta1!;Timeout=2;QueryTimeout=5;SSLMode=disable";
    SQLCHAR out_conn[256] = {};
    SQLSMALLINT out_len = 0;
    std::fprintf(stderr, "[odbc_test] connecting\n");
    SQLRETURN connect_rc = SQL_ERROR;
    for (int attempt = 0; attempt < 30; ++attempt) {
        connect_rc = SQLDriverConnect(dbc, nullptr,
                                      reinterpret_cast<SQLCHAR*>(conn_str.data()), SQL_NTS,
                                      out_conn, sizeof(out_conn), &out_len,
                                      SQL_DRIVER_NOPROMPT);
        if (connect_rc == SQL_SUCCESS) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (connect_rc != SQL_SUCCESS) {
        SQLCHAR sqlstate[6] = {};
        SQLINTEGER native = 0;
        SQLCHAR message[256] = {};
        SQLSMALLINT msg_len = 0;
        SQLGetDiagRec(SQL_HANDLE_DBC, dbc, 1, sqlstate, &native, message,
                      sizeof(message), &msg_len);
        FAIL() << "SQLDriverConnect failed: " << message << " (" << sqlstate << ")";
    }
    std::fprintf(stderr, "[odbc_test] connected\n");

    ASSERT_EQ(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt), SQL_SUCCESS);
    std::fprintf(stderr, "[odbc_test] creating table\n");
    ASSERT_EQ(SQLExecDirect(stmt,
                            reinterpret_cast<SQLCHAR*>(const_cast<char*>(
                                "CREATE TABLE IF NOT EXISTS odbc_smoke (id INT, name VARCHAR(32))")),
                            SQL_NTS), SQL_SUCCESS);
    ASSERT_EQ(SQLExecDirect(stmt,
                            reinterpret_cast<SQLCHAR*>(const_cast<char*>(
                                "DELETE FROM odbc_smoke")),
                            SQL_NTS), SQL_SUCCESS);

    std::fprintf(stderr, "[odbc_test] preparing insert\n");
    ASSERT_EQ(SQLPrepare(stmt,
                         reinterpret_cast<SQLCHAR*>(const_cast<char*>(
                             "INSERT INTO odbc_smoke (id, name) VALUES (?, ?)")),
                         SQL_NTS), SQL_SUCCESS);

    SQLINTEGER id = 7;
    SQLLEN id_ind = 0;
    char name[32] = "alpha";
    SQLLEN name_ind = SQL_NTS;

    ASSERT_EQ(SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER,
                               0, 0, &id, 0, &id_ind), SQL_SUCCESS);
    ASSERT_EQ(SQLBindParameter(stmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                               sizeof(name), 0, name, sizeof(name), &name_ind), SQL_SUCCESS);
    std::fprintf(stderr, "[odbc_test] executing insert 1\n");
    ASSERT_EQ(SQLExecute(stmt), SQL_SUCCESS);

    id = 11;
    std::strncpy(name, "bravo", sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    std::fprintf(stderr, "[odbc_test] executing insert 2\n");
    ASSERT_EQ(SQLExecute(stmt), SQL_SUCCESS);

    std::fprintf(stderr, "[odbc_test] preparing select\n");
    ASSERT_EQ(SQLFreeStmt(stmt, SQL_CLOSE), SQL_SUCCESS);
    ASSERT_EQ(SQLPrepare(stmt,
                         reinterpret_cast<SQLCHAR*>(const_cast<char*>(
                             "SELECT id, name FROM odbc_smoke WHERE id = ?")),
                         SQL_NTS), SQL_SUCCESS);
    ASSERT_EQ(SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER,
                               0, 0, &id, 0, &id_ind), SQL_SUCCESS);
    std::fprintf(stderr, "[odbc_test] executing select\n");
    ASSERT_EQ(SQLExecute(stmt), SQL_SUCCESS);
    std::fprintf(stderr, "[odbc_test] fetching select\n");
    ASSERT_EQ(SQLFetch(stmt), SQL_SUCCESS);

    SQLINTEGER out_id = 0;
    SQLLEN out_id_len = 0;
    char out_name[32] = {};
    SQLLEN out_name_len = 0;
    ASSERT_EQ(SQLGetData(stmt, 1, SQL_C_LONG, &out_id, 0, &out_id_len), SQL_SUCCESS);
    ASSERT_EQ(SQLGetData(stmt, 2, SQL_C_CHAR, out_name, sizeof(out_name), &out_name_len),
              SQL_SUCCESS);
    EXPECT_EQ(out_id, 11);
    EXPECT_STREQ(out_name, "bravo");

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);

    server->shutdown();
    server->waitForShutdown(2000);
}
