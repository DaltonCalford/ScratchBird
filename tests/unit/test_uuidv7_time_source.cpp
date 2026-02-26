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

#include "scratchbird/core/time_source.h"
#include "scratchbird/core/uuidv7.h"

namespace scratchbird::core
{
    namespace
    {
        class FixedTimeSource final : public TimeSource
        {
        public:
            explicit FixedTimeSource(uint64_t now_ms)
                : now_ms_(now_ms)
            {
            }

            auto nowMs() const -> uint64_t override
            {
                return now_ms_;
            }

            auto nowMicros() const -> uint64_t override
            {
                return now_ms_ * 1000;
            }

        private:
            uint64_t now_ms_;
        };

        auto decodeUuidV7TimestampMs(const UuidV7Bytes &uuid) -> uint64_t
        {
            return (static_cast<uint64_t>(uuid.bytes[0]) << 40) |
                   (static_cast<uint64_t>(uuid.bytes[1]) << 32) |
                   (static_cast<uint64_t>(uuid.bytes[2]) << 24) |
                   (static_cast<uint64_t>(uuid.bytes[3]) << 16) |
                   (static_cast<uint64_t>(uuid.bytes[4]) << 8) |
                   static_cast<uint64_t>(uuid.bytes[5]);
        }
    } // namespace

    TEST(UuidV7TimeSourceTest, InjectedTimeSourceControlsTimestampBits)
    {
        constexpr uint64_t kFixedTsMs = 1735689600123ULL;
        FixedTimeSource source(kFixedTsMs);

        const UuidV7Bytes uuid = generateUuidV7(&source);
        EXPECT_EQ(decodeUuidV7TimestampMs(uuid), kFixedTsMs);
    }

    TEST(UuidV7TimeSourceTest, GeneratedUuidPreservesVersionAndVariantBits)
    {
        const UuidV7Bytes uuid = generateUuidV7();
        EXPECT_EQ((uuid.bytes[6] & 0xF0), 0x70);
        EXPECT_EQ((uuid.bytes[8] & 0xC0), 0x80);
    }
} // namespace scratchbird::core
