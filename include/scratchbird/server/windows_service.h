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

#include <functional>
#include <memory>
#include <string>

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"

namespace scratchbird::server
{

    struct WindowsServiceOptions
    {
        std::string service_name{"ScratchBirdServer"};
    };

    class WindowsServiceHost
    {
    public:
        virtual ~WindowsServiceHost() = default;

        virtual auto runConsole(const std::function<int()>& run_callback,
                                core::ErrorContext* ctx) -> core::Status = 0;

        virtual auto runAsService(const WindowsServiceOptions& options,
                                  const std::function<int()>& run_callback,
                                  const std::function<void()>& stop_callback,
                                  core::ErrorContext* ctx) -> core::Status = 0;
    };

    auto createDefaultWindowsServiceHost() -> std::unique_ptr<WindowsServiceHost>;

} // namespace scratchbird::server
