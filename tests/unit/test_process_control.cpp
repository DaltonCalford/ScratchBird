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

#include "scratchbird/core/process_control.h"

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace scratchbird::core
{

namespace
{

    auto quickExitSpec() -> ProcessLaunchSpec
    {
        ProcessLaunchSpec spec;
#ifdef _WIN32
        spec.executable = "cmd.exe";
        spec.arguments = {"/C", "exit 0"};
#else
        spec.executable = "/bin/sh";
        spec.arguments = {"-c", "exit 0"};
#endif
        return spec;
    }

    auto longRunningSpec() -> ProcessLaunchSpec
    {
        ProcessLaunchSpec spec;
#ifdef _WIN32
        spec.executable = "cmd.exe";
        spec.arguments = {"/C", "ping -n 30 127.0.0.1 > NUL"};
#else
        spec.executable = "/bin/sh";
        spec.arguments = {"-c", "sleep 30"};
#endif
        return spec;
    }

} // namespace

TEST(ProcessControlTest, SpawnsAndWaitsForExit)
{
    auto process_control = createDefaultProcessControl();
    ASSERT_NE(process_control, nullptr);

    ErrorContext ctx;
    SpawnedProcess process;
    auto spawn_status = process_control->spawn(quickExitSpec(), &process, &ctx);
    ASSERT_EQ(spawn_status, Status::OK) << ctx.message;
    ASSERT_NE(process.process_id, 0u);

    ProcessWaitResult wait_result;
    auto wait_status = process_control->wait(process, 5000, &wait_result, &ctx);
    EXPECT_EQ(wait_status, Status::OK) << ctx.message;
    EXPECT_EQ(wait_result.state, ProcessState::EXITED);
    EXPECT_EQ(wait_result.exit_code, 0);

    EXPECT_EQ(process_control->close(&process, &ctx), Status::OK);
}

TEST(ProcessControlTest, TerminatesProcess)
{
    auto process_control = createDefaultProcessControl();
    ASSERT_NE(process_control, nullptr);

    ErrorContext ctx;
    SpawnedProcess process;
    auto spawn_status = process_control->spawn(longRunningSpec(), &process, &ctx);
    ASSERT_EQ(spawn_status, Status::OK) << ctx.message;
    ASSERT_NE(process.process_id, 0u);

    bool running = false;
    EXPECT_EQ(process_control->isRunning(process, &running, &ctx), Status::OK);
    EXPECT_TRUE(running);

    auto terminate_status = process_control->terminate(process, false, &ctx);
    EXPECT_TRUE(terminate_status == Status::OK || terminate_status == Status::NOT_FOUND);

    ProcessWaitResult wait_result;
    auto wait_status = process_control->wait(process, 5000, &wait_result, &ctx);
    EXPECT_EQ(wait_status, Status::OK) << ctx.message;
    EXPECT_NE(wait_result.state, ProcessState::TIMED_OUT);

    EXPECT_EQ(process_control->close(&process, &ctx), Status::OK);
}

#ifndef _WIN32
TEST(ProcessControlTest, ForkSelfCreatesChildProcess)
{
    auto process_control = createDefaultProcessControl();
    ASSERT_NE(process_control, nullptr);

    ErrorContext ctx;
    bool is_parent = false;
    uint64_t child_pid = 0;
    ASSERT_EQ(process_control->forkSelf(&is_parent, &child_pid, &ctx), Status::OK) << ctx.message;

    if (!is_parent) {
        _exit(0);
    }

    ASSERT_GT(child_pid, 0u);
    int status = 0;
    ASSERT_EQ(waitpid(static_cast<pid_t>(child_pid), &status, 0),
              static_cast<pid_t>(child_pid));
    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(WEXITSTATUS(status), 0);
}
#endif

} // namespace scratchbird::core
