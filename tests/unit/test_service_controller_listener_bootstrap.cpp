/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

#define private public
#include "scratchbird/core/database.h"
#include "scratchbird/network/socket.h"
#include "scratchbird/server/service_controller.h"
#include "scratchbird/server/ipc_server.h"
#undef private
#include "test_helpers.h"

#ifndef _WIN32
#include <sys/stat.h>
#endif

using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::network::AddressFamily;
using scratchbird::network::NetworkAddress;
using scratchbird::network::Socket;
using scratchbird::network::SocketType;
using scratchbird::server::ProtocolConfig;
using scratchbird::server::ServiceController;

namespace {

#ifndef _WIN32

class ScopedEnvVar {
public:
    ScopedEnvVar(const char* name, const std::string& value)
        : name_(name),
          had_old_(std::getenv(name) != nullptr),
          old_value_(had_old_ ? std::getenv(name) : "") {
        setenv(name_.c_str(), value.c_str(), 1);
    }

    ~ScopedEnvVar() {
        if (had_old_) {
            setenv(name_.c_str(), old_value_.c_str(), 1);
        } else {
            unsetenv(name_.c_str());
        }
    }

private:
    std::string name_;
    bool had_old_;
    std::string old_value_;
};

bool waitForFile(const std::filesystem::path& path, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (std::filesystem::exists(path)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return std::filesystem::exists(path);
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

bool writeStubListener(const std::filesystem::path& binary_path,
                       const std::filesystem::path& args_log_path) {
    std::ofstream out(binary_path);
    if (!out.is_open()) {
        return false;
    }
    out << "#!/usr/bin/env bash\n";
    out << "printf '__invocation__\\n' >> \"" << args_log_path.string() << "\"\n";
    out << "printf '%s\\n' \"$@\" >> \"" << args_log_path.string() << "\"\n";
    out << "exit 0\n";
    out.close();
    return ::chmod(binary_path.c_str(), 0755) == 0;
}

struct ScopedListenerCleanup {
    ServiceController& controller;
    std::filesystem::path temp_dir;

    ~ScopedListenerCleanup() {
        controller.stopListeners(nullptr);
        for (auto& db : controller.databases_) {
            if (db.database && db.owned_database) {
                db.database->close();
            }
        }
        controller.databases_.clear();
        std::error_code ec;
        std::filesystem::remove_all(temp_dir, ec);
    }
};

#endif

}  // namespace

TEST(ServiceControllerListenerBootstrapTest, StartListenersPassesConfigFileToListenerProcess) {
#ifdef _WIN32
    GTEST_SKIP() << "Test relies on POSIX fork/exec behavior.";
#else
    const std::filesystem::path temp_dir =
        scratchbird::testing::uniqueTestDbPath("listener_bootstrap", "");
    std::error_code ec;
    std::filesystem::create_directories(temp_dir, ec);
    ASSERT_FALSE(ec) << ec.message();

    ServiceController controller;
    ScopedListenerCleanup cleanup{controller, temp_dir};

    const std::filesystem::path config_path = temp_dir / "sb_server.conf";
    const std::filesystem::path args_path = temp_dir / "listener_args.txt";
    const std::filesystem::path listener_binary = temp_dir / "sb_listener_native";

    {
        std::ofstream config_out(config_path);
        ASSERT_TRUE(config_out.is_open());
        config_out << "[server]\nmode=single-database\n";
    }
    ASSERT_TRUE(writeStubListener(listener_binary, args_path));

    const char* current_path = std::getenv("PATH");
    const std::string path_prefix =
        temp_dir.string() + ":" + (current_path ? std::string(current_path) : std::string());
    ScopedEnvVar scoped_path("PATH", path_prefix);

    controller.config_.protocols.clear();
    ProtocolConfig proto;
    proto.type = scratchbird::network::ProtocolType::NATIVE;
    proto.bind_address = "127.0.0.1";
    proto.port = 39092;
    proto.enabled = true;
    proto.pool_min = 1;
    proto.pool_max = 2;
    controller.config_.protocols.push_back(proto);
    controller.config_.config_file = config_path.string();
    controller.config_.control_socket_dir = temp_dir.string();
    controller.config_.shutdown_timeout_sec = 0;

    const std::filesystem::path main_db_path = temp_dir / "main.sbdb";
    ErrorContext create_ctx;
    ASSERT_EQ(scratchbird::core::Database::create(main_db_path.string(), 8192, &create_ctx),
              Status::OK) << create_ctx.message;

    // startListeners() requires at least one open database before listeners are enabled.
    ServiceController::DatabaseInstance db;
    db.name = "main";
    db.path = main_db_path.string();
    controller.databases_.push_back(std::move(db));

    ErrorContext ctx;
    ASSERT_EQ(controller.startListeners(&ctx), Status::OK) << ctx.message;
    ASSERT_TRUE(waitForFile(args_path, std::chrono::milliseconds(1000)));

    const std::string args = readTextFile(args_path);
    EXPECT_NE(args.find("--config"), std::string::npos) << args;
    EXPECT_NE(args.find(config_path.string()), std::string::npos) << args;
    EXPECT_NE(args.find("--database-owner"), std::string::npos) << args;
    EXPECT_NE(args.find("main"), std::string::npos) << args;
    EXPECT_NE(args.find("--engine-endpoint"), std::string::npos) << args;
    EXPECT_NE(args.find(scratchbird::server::getIPCPath(main_db_path.string(),
                                                        scratchbird::server::IPCMethod::AUTO)),
              std::string::npos) << args;
#endif
}

TEST(ServiceControllerListenerBootstrapTest, PortCollisionSkipsListenerLaunchAndParserBootstrap) {
#ifdef _WIN32
    GTEST_SKIP() << "Test relies on POSIX fork/exec behavior.";
#else
    const std::filesystem::path temp_dir =
        scratchbird::testing::uniqueTestDbPath("listener_port_collision", "");
    std::error_code ec;
    std::filesystem::create_directories(temp_dir, ec);
    ASSERT_FALSE(ec) << ec.message();

    ServiceController controller;
    ScopedListenerCleanup cleanup{controller, temp_dir};

    const std::filesystem::path args_path = temp_dir / "listener_args.txt";
    const std::filesystem::path listener_binary = temp_dir / "sb_listener_native";
    ASSERT_TRUE(writeStubListener(listener_binary, args_path));

    const char* current_path = std::getenv("PATH");
    const std::string path_prefix =
        temp_dir.string() + ":" + (current_path ? std::string(current_path) : std::string());
    ScopedEnvVar scoped_path("PATH", path_prefix);

    ErrorContext socket_ctx;
    auto blocker = Socket::create(AddressFamily::IPV4, SocketType::STREAM, &socket_ctx);
    ASSERT_NE(blocker, nullptr) << socket_ctx.message;
    NetworkAddress bind_addr("127.0.0.1", 0, AddressFamily::IPV4);
    ASSERT_EQ(blocker->bind(bind_addr, &socket_ctx), Status::OK) << socket_ctx.message;
    ASSERT_EQ(blocker->listen(8, &socket_ctx), Status::OK) << socket_ctx.message;
    const auto blocker_local = blocker->getLocalAddress();
    ASSERT_TRUE(blocker_local.has_value());
    const uint16_t blocked_port = blocker_local->port;

    controller.config_.protocols.clear();
    ProtocolConfig proto;
    proto.type = scratchbird::network::ProtocolType::NATIVE;
    proto.bind_address = "127.0.0.1";
    proto.port = blocked_port;
    proto.enabled = true;
    proto.pool_min = 1;
    proto.pool_max = 2;
    controller.config_.protocols.push_back(proto);
    controller.config_.control_socket_dir = temp_dir.string();

    ServiceController::DatabaseInstance db;
    db.name = "main";
    db.path = (temp_dir / "main.sbdb").string();
    controller.databases_.push_back(std::move(db));

    ErrorContext ctx;
    ASSERT_EQ(controller.startListeners(&ctx), Status::OK) << ctx.message;
    EXPECT_TRUE(controller.listeners_.empty());
    EXPECT_FALSE(std::filesystem::exists(args_path));
#endif
}

TEST(ServiceControllerListenerBootstrapTest,
     MultipleNativeListenersCanRunWithDistinctOwnersAndPorts) {
#ifdef _WIN32
    GTEST_SKIP() << "Test relies on POSIX fork/exec behavior.";
#else
    const std::filesystem::path temp_dir =
        scratchbird::testing::uniqueTestDbPath("listener_multi_owner", "");
    std::error_code ec;
    std::filesystem::create_directories(temp_dir, ec);
    ASSERT_FALSE(ec) << ec.message();

    ServiceController controller;
    ScopedListenerCleanup cleanup{controller, temp_dir};

    const std::filesystem::path args_path = temp_dir / "listener_args.txt";
    const std::filesystem::path listener_binary = temp_dir / "sb_listener_native";
    ASSERT_TRUE(writeStubListener(listener_binary, args_path));

    const char* current_path = std::getenv("PATH");
    const std::string path_prefix =
        temp_dir.string() + ":" + (current_path ? std::string(current_path) : std::string());
    ScopedEnvVar scoped_path("PATH", path_prefix);

    const std::filesystem::path main_db_path = temp_dir / "main.sbdb";
    const std::filesystem::path analytics_db_path = temp_dir / "analytics.sbdb";
    ErrorContext create_ctx;
    ASSERT_EQ(scratchbird::core::Database::create(main_db_path.string(), 8192, &create_ctx),
              Status::OK) << create_ctx.message;
    ASSERT_EQ(scratchbird::core::Database::create(analytics_db_path.string(), 8192, &create_ctx),
              Status::OK) << create_ctx.message;

    ServiceController::DatabaseInstance main_db;
    main_db.name = "main";
    main_db.path = main_db_path.string();
    controller.databases_.push_back(std::move(main_db));

    ServiceController::DatabaseInstance analytics_db;
    analytics_db.name = "analytics";
    analytics_db.path = analytics_db_path.string();
    controller.databases_.push_back(std::move(analytics_db));

    controller.config_.protocols.clear();
    ProtocolConfig primary_proto;
    primary_proto.type = scratchbird::network::ProtocolType::NATIVE;
    primary_proto.bind_address = "127.0.0.1";
    primary_proto.port = 39102;
    primary_proto.enabled = true;
    primary_proto.pool_min = 1;
    primary_proto.pool_max = 2;
    primary_proto.owner_database = "main";
    controller.config_.protocols.push_back(primary_proto);

    ProtocolConfig analytics_proto = primary_proto;
    analytics_proto.port = 39103;
    analytics_proto.owner_database = "analytics";
    controller.config_.protocols.push_back(analytics_proto);

    controller.config_.control_socket_dir = temp_dir.string();

    ErrorContext ctx;
    ASSERT_EQ(controller.startListeners(&ctx), Status::OK) << ctx.message;
    EXPECT_EQ(controller.listeners_.size(), 2U);
    ASSERT_TRUE(waitForFile(args_path, std::chrono::milliseconds(1000)));

    const std::string args = readTextFile(args_path);
    EXPECT_NE(args.find("39102"), std::string::npos) << args;
    EXPECT_NE(args.find("39103"), std::string::npos) << args;
    EXPECT_NE(args.find("--database-owner"), std::string::npos) << args;
    EXPECT_NE(args.find("main"), std::string::npos) << args;
    EXPECT_NE(args.find("analytics"), std::string::npos) << args;
    EXPECT_NE(args.find(scratchbird::server::getIPCPath(main_db_path.string(),
                                                        scratchbird::server::IPCMethod::AUTO)),
              std::string::npos) << args;
    EXPECT_NE(args.find(scratchbird::server::getIPCPath(analytics_db_path.string(),
                                                        scratchbird::server::IPCMethod::AUTO)),
              std::string::npos) << args;
#endif
}
