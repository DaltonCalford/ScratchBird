/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/time_source.h"

#include <chrono>

namespace scratchbird::core
{
    namespace
    {
        class PlatformTimeSource final : public TimeSource
        {
        public:
            auto nowMs() const -> uint64_t override
            {
                const auto now = std::chrono::system_clock::now().time_since_epoch();
                return static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
            }

            auto nowMicros() const -> uint64_t override
            {
                const auto now = std::chrono::system_clock::now().time_since_epoch();
                return static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(now).count());
            }
        };
    } // namespace

    auto defaultTimeSource() -> const TimeSource&
    {
        static const PlatformTimeSource k_default_time_source{};
        return k_default_time_source;
    }
} // namespace scratchbird::core
