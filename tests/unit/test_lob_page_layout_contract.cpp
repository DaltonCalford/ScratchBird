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
#include "scratchbird/core/ondisk.h"

using namespace scratchbird::core;

TEST(LobPageLayoutContractTest, ToastLobPageTypesAreDistinctAndCanonical)
{
    EXPECT_EQ(PAGE_TYPE_TOAST_META, 0x0008);
    EXPECT_EQ(PAGE_TYPE_TOAST_CHUNK, 0x0009);
    EXPECT_EQ(PAGE_TYPE_LOB_META, 0x000A);
    EXPECT_EQ(PAGE_TYPE_LOB_CHUNK, 0x000B);

    EXPECT_NE(PAGE_TYPE_TOAST_META, PAGE_TYPE_LOB_META);
    EXPECT_NE(PAGE_TYPE_TOAST_CHUNK, PAGE_TYPE_LOB_CHUNK);
}

TEST(LobPageLayoutContractTest, CanonicalStructSizesMatchSpecification)
{
    EXPECT_EQ(sizeof(ToastMetaPageLayout), 56u);
    EXPECT_EQ(sizeof(ToastChunkRecordHeader), 24u);
    EXPECT_EQ(sizeof(LobMetaRecordLayout), 60u);
    EXPECT_EQ(sizeof(LobChunkRecordHeader), 24u);
}

TEST(LobPageLayoutContractTest, ChunkValidationHelpersMatchNormativeRules)
{
    // payload_len <= chunk_size
    EXPECT_TRUE(isValidLobOrToastChunkPayload(1024, 2048));
    EXPECT_FALSE(isValidLobOrToastChunkPayload(4096, 2048));
    EXPECT_FALSE(isValidLobOrToastChunkPayload(1, 0));

    // chunk_index contiguous range [0, ceil(total_len/chunk_size)-1]
    EXPECT_EQ(expectedLobOrToastChunkCount(8192, 2048), 4u);
    EXPECT_TRUE(isValidLobOrToastChunkIndex(0, 8192, 2048));
    EXPECT_TRUE(isValidLobOrToastChunkIndex(3, 8192, 2048));
    EXPECT_FALSE(isValidLobOrToastChunkIndex(4, 8192, 2048));
    EXPECT_FALSE(isValidLobOrToastChunkIndex(0, 8192, 0));
}

