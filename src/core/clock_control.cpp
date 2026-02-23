/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/clock_control.h"

#include <thread>

namespace scratchbird::core
{

    namespace
    {

        class PlatformClockControl final : public ClockControl
        {
        public:
            auto monotonicNow() const -> std::chrono::steady_clock::time_point override
            {
                return std::chrono::steady_clock::now();
            }

            auto realtimeNowMs() const -> uint64_t override
            {
                auto now = std::chrono::system_clock::now().time_since_epoch();
                return static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
            }

            void sleepFor(std::chrono::milliseconds duration) const override
            {
                if (duration.count() <= 0)
                {
                    return;
                }
                std::this_thread::sleep_for(duration);
            }
        };

    } // namespace

    auto createDefaultClockControl() -> std::unique_ptr<ClockControl>
    {
        return std::make_unique<PlatformClockControl>();
    }

} // namespace scratchbird::core
