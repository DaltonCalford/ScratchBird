/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/data_masking.h"
#include "scratchbird/core/error_context.h"
#include "gtest/gtest.h"

using namespace scratchbird::core;

TEST(DataMaskingTest, NoneMaskingPassthrough)
{
    MaskingConfig config;
    config.type = MaskingType::NONE;

    std::string masked;
    ErrorContext ctx;
    Status status = DataMasking::applyMasking("secret", config, false, masked, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(masked, "secret");
}

TEST(DataMaskingTest, FullMasking)
{
    MaskingConfig config;
    config.type = MaskingType::FULL;
    config.full_mask_char = "*";

    std::string masked;
    ErrorContext ctx;
    Status status = DataMasking::applyMasking("secret", config, false, masked, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(masked, "******");
}

TEST(DataMaskingTest, PartialMaskingPattern)
{
    MaskingConfig config;
    config.type = MaskingType::PARTIAL;
    config.pattern = "XXX-XX-####";
    config.full_mask_char = "*";

    std::string masked;
    ErrorContext ctx;
    Status status = DataMasking::applyMasking("123-45-6789", config, false, masked, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(masked, "***-**-6789");
}

TEST(DataMaskingTest, PrivilegeBypass)
{
    MaskingConfig config;
    config.type = MaskingType::FULL;

    std::string masked;
    ErrorContext ctx;
    Status status = DataMasking::applyMasking("secret", config, true, masked, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(masked, "secret");
}

TEST(DataMaskingTest, UnicodeFullMasking)
{
    MaskingConfig config;
    config.type = MaskingType::FULL;
    config.full_mask_char = "*";

    std::string masked;
    ErrorContext ctx;
    std::string value = "caf\xC3\xA9";  // "cafe" with accented e (UTF-8)
    Status status = DataMasking::applyMasking(value, config, false, masked, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(masked, "****");
}

TEST(DataMaskingTest, ParsePattern)
{
    std::vector<char> parsed;
    ErrorContext ctx;
    Status status = DataMasking::parsePattern("XXX-###", parsed, &ctx);
    ASSERT_EQ(status, Status::OK);
    ASSERT_EQ(parsed.size(), 7u);
    ASSERT_EQ(parsed[0], 'X');
    ASSERT_EQ(parsed[3], '-');
    ASSERT_EQ(parsed[4], '#');
}

TEST(DataMaskingTest, EmptyPatternRejected)
{
    std::vector<char> parsed;
    ErrorContext ctx;
    Status status = DataMasking::parsePattern("", parsed, &ctx);
    ASSERT_EQ(status, Status::INVALID_ARGUMENT);
}
