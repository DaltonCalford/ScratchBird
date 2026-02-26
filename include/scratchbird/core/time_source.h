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

namespace scratchbird::core
{
    class TimeSource
    {
    public:
        virtual ~TimeSource() = default;

        virtual auto nowMs() const -> uint64_t = 0;
        virtual auto nowMicros() const -> uint64_t = 0;
    };

    auto defaultTimeSource() -> const TimeSource&;
} // namespace scratchbird::core
