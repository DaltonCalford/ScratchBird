/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/network/listener_ipc_adapter.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>

namespace
{

using scratchbird::core::Status;
using scratchbird::network::AddressFamily;
using scratchbird::network::ListenerSocketConfig;

TEST(ListenerIpcAdapterTest, FrontDoorSocketLifecycle)
{
    auto acceptor = scratchbird::network::createDefaultListenerSocketAcceptor();
    ASSERT_NE(acceptor, nullptr);

    ListenerSocketConfig config;
    config.family = AddressFamily::IPV4;
    config.bind_address = "127.0.0.1";
    config.port = 0; // Ephemeral test port
    config.non_blocking = true;

    ASSERT_EQ(acceptor->start(config, nullptr), Status::OK);
    EXPECT_TRUE(acceptor->isRunning());

    scratchbird::network::NetworkAddress client_address;
    auto connection = acceptor->accept(&client_address, nullptr);
    EXPECT_EQ(connection, nullptr);

    acceptor->close();
    EXPECT_FALSE(acceptor->isRunning());
}

TEST(ListenerIpcAdapterTest, LocalControlChannelLifecycle)
{
    auto control = scratchbird::network::createDefaultLocalControlChannel();
    ASSERT_NE(control, nullptr);

    const auto tick = static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
#ifdef _WIN32
    std::string path = std::string("\\\\.\\pipe\\scratchbird-test-control-") + std::to_string(tick);
#else
    std::filesystem::path base = std::filesystem::path("build") / "ipc";
    std::filesystem::create_directories(base);
    std::string path = (base / ("test-control-" + std::to_string(tick) + ".sock")).string();
#endif

    Status status = control->start(path, nullptr);
#ifdef _WIN32
    EXPECT_EQ(status, Status::NOT_IMPLEMENTED);
#else
    ASSERT_EQ(status, Status::OK);
    EXPECT_TRUE(control->isRunning());
    EXPECT_FALSE(control->path().empty());
    control->stop();
    EXPECT_FALSE(control->isRunning());
#endif
}

} // namespace
