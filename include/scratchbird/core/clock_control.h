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

#include <chrono>
#include <cstdint>
#include <memory>

namespace scratchbird::core
{

    class ClockControl
    {
    public:
        virtual ~ClockControl() = default;

        virtual auto monotonicNow() const -> std::chrono::steady_clock::time_point = 0;
        virtual auto realtimeNowMs() const -> uint64_t = 0;
        virtual void sleepFor(std::chrono::milliseconds duration) const = 0;
    };

    auto createDefaultClockControl() -> std::unique_ptr<ClockControl>;

} // namespace scratchbird::core
