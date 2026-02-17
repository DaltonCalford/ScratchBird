/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include <cstddef>

#include <gtest/gtest.h>

#include "scratchbird/core/ondisk.h"

namespace scratchbird::core
{

TEST(VNextBootstrapContractTest, FixedBootstrapPageMapIsCanonical)
{
    EXPECT_EQ(BOOTSTRAP_PAGE_DATABASE_HEADER, 0u);
    EXPECT_EQ(BOOTSTRAP_PAGE_SYSTEM_STATE, 1u);
    EXPECT_EQ(BOOTSTRAP_PAGE_CATALOG_ROOT, 2u);
    EXPECT_EQ(BOOTSTRAP_PAGE_FSM_ROOT, 3u);
    EXPECT_EQ(BOOTSTRAP_PAGE_TX_MAP_ROOT, 4u);
    EXPECT_EQ(BOOTSTRAP_PAGE_RESERVED, 5u);
    EXPECT_EQ(BOOTSTRAP_FIXED_PAGE_COUNT, 6u);
}

TEST(VNextBootstrapContractTest, PointerSwapLayoutMatchesSpecification)
{
    EXPECT_EQ(sizeof(VNextPointerSwapRecord), 32u);
    EXPECT_EQ(offsetof(VNextPointerSwapRecord, swap_epoch), 0u);
    EXPECT_EQ(offsetof(VNextPointerSwapRecord, old_page_id), 8u);
    EXPECT_EQ(offsetof(VNextPointerSwapRecord, new_page_id), 12u);
    EXPECT_EQ(offsetof(VNextPointerSwapRecord, owner_txid), 16u);
    EXPECT_EQ(offsetof(VNextPointerSwapRecord, record_crc32c), 24u);
    EXPECT_EQ(offsetof(VNextPointerSwapRecord, state), 28u);
    EXPECT_EQ(offsetof(VNextPointerSwapRecord, reserved), 29u);
}

TEST(VNextBootstrapContractTest, PointerSwapRecordValidationChecksCrcAndState)
{
    VNextPointerSwapRecord rec{};
    rec.swap_epoch = 7u;
    rec.old_page_id = 2u;
    rec.new_page_id = 42u;
    rec.owner_txid = 1001u;
    rec.state = static_cast<uint8_t>(VNextPointerSwapState::COMMITTED);
    rec.record_crc32c = computeVNextPointerSwapRecordCrc32c(rec);

    EXPECT_TRUE(isValidVNextPointerSwapRecord(rec));

    rec.state = 9u;
    EXPECT_FALSE(isValidVNextPointerSwapRecord(rec));

    rec.state = static_cast<uint8_t>(VNextPointerSwapState::COMMITTED);
    rec.record_crc32c ^= 0x5Au;
    EXPECT_FALSE(isValidVNextPointerSwapRecord(rec));
}

TEST(VNextBootstrapContractTest, AuthoritativeSlotSelectionIsDeterministic)
{
    VNextPointerSwapRecord slot0{};
    slot0.swap_epoch = 3u;
    slot0.new_page_id = 30u;
    slot0.owner_txid = 100u;
    slot0.state = static_cast<uint8_t>(VNextPointerSwapState::COMMITTED);
    slot0.record_crc32c = computeVNextPointerSwapRecordCrc32c(slot0);

    VNextPointerSwapRecord slot1{};
    slot1.swap_epoch = 4u;
    slot1.new_page_id = 40u;
    slot1.owner_txid = 101u;
    slot1.state = static_cast<uint8_t>(VNextPointerSwapState::COMMITTED);
    slot1.record_crc32c = computeVNextPointerSwapRecordCrc32c(slot1);

    uint8_t selected = 0xFFu;
    EXPECT_EQ(selectVNextAuthoritativeSwapSlot(slot0, slot1, selected), Status::OK);
    EXPECT_EQ(selected, 1u);

    slot1.state = static_cast<uint8_t>(VNextPointerSwapState::PREPARE);
    slot1.record_crc32c = computeVNextPointerSwapRecordCrc32c(slot1);
    EXPECT_EQ(selectVNextAuthoritativeSwapSlot(slot0, slot1, selected), Status::OK);
    EXPECT_EQ(selected, 0u);

    slot1.state = static_cast<uint8_t>(VNextPointerSwapState::COMMITTED);
    slot1.swap_epoch = slot0.swap_epoch;
    slot1.record_crc32c = computeVNextPointerSwapRecordCrc32c(slot1);
    EXPECT_EQ(selectVNextAuthoritativeSwapSlot(slot0, slot1, selected), Status::PAGE_CORRUPT);
}

TEST(VNextBootstrapContractTest, SlotDirectoryValidationEnforcesAlignmentAndBounds)
{
    VNextSlotDirectoryEntry slot{};
    slot.slot_offset = VNEXT_PAYLOAD_REGION_START;
    slot.slot_len = 16u;
    slot.slot_flags = 0u;
    slot.slot_reserved = 0u;
    EXPECT_TRUE(isValidVNextSlotDirectoryEntry(slot));

    slot.slot_offset = static_cast<uint16_t>(VNEXT_PAYLOAD_REGION_START + 4u);
    EXPECT_FALSE(isValidVNextSlotDirectoryEntry(slot));

    slot.slot_offset = VNEXT_PAYLOAD_REGION_START;
    slot.slot_reserved = 1u;
    EXPECT_FALSE(isValidVNextSlotDirectoryEntry(slot));
}

} // namespace scratchbird::core

