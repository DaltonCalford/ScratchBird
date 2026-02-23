/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"

namespace scratchbird::core
{

    struct ProcessLaunchSpec
    {
        std::string executable;
        std::vector<std::string> arguments;
        std::map<std::string, std::string> environment_overrides;
        std::string working_directory;
        bool create_new_process_group{true};
    };

    struct SpawnedProcess
    {
        uint64_t process_id{0};
        uintptr_t native_handle{0};
        bool has_native_handle{false};
    };

    enum class ProcessState : uint8_t
    {
        NOT_FOUND = 0,
        RUNNING,
        EXITED,
        TIMED_OUT
    };

    struct ProcessWaitResult
    {
        ProcessState state{ProcessState::NOT_FOUND};
        int exit_code{-1};
        bool signaled{false};
        int signal_code{0};
    };

    class ProcessControl
    {
    public:
        virtual ~ProcessControl() = default;

        virtual auto spawn(const ProcessLaunchSpec& spec,
                           SpawnedProcess* out,
                           ErrorContext* ctx) -> Status = 0;

        virtual auto forkSelf(bool* is_parent_out,
                              uint64_t* child_pid_out,
                              ErrorContext* ctx) -> Status = 0;

        virtual auto wait(const SpawnedProcess& process,
                          uint32_t timeout_ms,
                          ProcessWaitResult* result,
                          ErrorContext* ctx) -> Status = 0;

        virtual auto terminate(const SpawnedProcess& process,
                               bool force,
                               ErrorContext* ctx) -> Status = 0;

        virtual auto isRunning(const SpawnedProcess& process,
                               bool* running_out,
                               ErrorContext* ctx) -> Status = 0;

        virtual auto close(SpawnedProcess* process,
                           ErrorContext* ctx) -> Status = 0;
    };

    auto createDefaultProcessControl() -> std::unique_ptr<ProcessControl>;

} // namespace scratchbird::core
