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

#include "scratchbird/server/windows_service.h"

namespace
{

using scratchbird::core::ErrorContext;
using scratchbird::core::Status;
using scratchbird::server::WindowsServiceOptions;

TEST(WindowsServiceHostTest, ConsoleModeExecutesCallback)
{
    auto host = scratchbird::server::createDefaultWindowsServiceHost();
    ASSERT_NE(host, nullptr);

    int call_count = 0;
    ErrorContext ctx;
    const Status status = host->runConsole([&call_count]() {
        ++call_count;
        return 0;
    }, &ctx);

    EXPECT_EQ(status, Status::OK);
    EXPECT_EQ(call_count, 1);
}

TEST(WindowsServiceHostTest, ConsoleModeReportsFailure)
{
    auto host = scratchbird::server::createDefaultWindowsServiceHost();
    ASSERT_NE(host, nullptr);

    ErrorContext ctx;
    const Status status = host->runConsole([]() { return 3; }, &ctx);
    EXPECT_EQ(status, Status::INTERNAL_ERROR);
    EXPECT_FALSE(ctx.message.empty());
}

TEST(WindowsServiceHostTest, ServiceModeContractIsPlatformGuarded)
{
    auto host = scratchbird::server::createDefaultWindowsServiceHost();
    ASSERT_NE(host, nullptr);

    ErrorContext ctx;
    WindowsServiceOptions options;
    options.service_name = "ScratchBirdUnitTestService";

#ifndef _WIN32
    const Status status = host->runAsService(
        options,
        []() { return 0; },
        []() {},
        &ctx);
    EXPECT_EQ(status, Status::NOT_SUPPORTED);
    EXPECT_FALSE(ctx.message.empty());
#else
    GTEST_SKIP() << "Windows SCM execution requires dedicated integration harness.";
#endif
}

} // namespace
