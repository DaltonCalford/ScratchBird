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
#include <memory>

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"

namespace scratchbird::core
{

    enum class ControlSignal : uint8_t
    {
        NONE = 0,
        SHUTDOWN = 1,
        RELOAD = 2,
        ROTATE_LOGS = 3,
        DUMP_STATS = 4,
        IMMEDIATE_STOP = 5
    };

    struct SignalInstallSpec
    {
        bool enable_shutdown_signal{true};
        bool enable_reload_signal{true};
        bool enable_rotate_logs_signal{true};
        bool enable_dump_stats_signal{true};
        bool enable_immediate_stop_signal{true};
        bool ignore_broken_pipe{true};
    };

    class SignalControl
    {
    public:
        virtual ~SignalControl() = default;

        virtual auto install(const SignalInstallSpec& spec,
                             ErrorContext* ctx) -> Status = 0;

        virtual auto uninstall(ErrorContext* ctx) -> Status = 0;

        virtual auto poll(ControlSignal* signal_out,
                          ErrorContext* ctx) -> Status = 0;

        virtual auto inject(ControlSignal signal,
                            ErrorContext* ctx) -> Status = 0;
    };

    auto createDefaultSignalControl() -> std::unique_ptr<SignalControl>;

} // namespace scratchbird::core
