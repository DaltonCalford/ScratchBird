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
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/toast.h"
#include "scratchbird/core/heap_toast_lob_diagnostics.h"
#include "scratchbird/core/error_context.h"
#include <cstring>
#include <vector>

using namespace scratchbird::core;

namespace
{
auto buildTupleWithPayload(const std::vector<uint8_t> &payload, uint64_t xmin)
    -> std::vector<uint8_t>
{
    TupleHeader hdr{};
    hdr.xmin = xmin;
    hdr.xmax = 0;
    hdr.back_version_gpid = INVALID_GPID;
    hdr.back_version_slot = 0;
    hdr.reserved1 = 0;
    hdr.ctid_gpid = INVALID_GPID;
    hdr.ctid_slot = 0;
    hdr.infomask = 0;
    hdr.null_bitmap_offset = 0;
    hdr.padding = 0;
    hdr.session_id = ID{};
    hdr.row_uuid = ID{};
    hdr.record_flags = 0;
    hdr.record_format = 0;
    hdr.payload_len = 0;

    std::vector<uint8_t> tuple(sizeof(TupleHeader) + payload.size(), 0);
    std::memcpy(tuple.data(), &hdr, sizeof(TupleHeader));
    if (!payload.empty())
    {
        std::memcpy(tuple.data() + sizeof(TupleHeader), payload.data(), payload.size());
    }
    return tuple;
}
} // namespace

TEST(HeapToastLobPageWalkerContractTest, ValidHeapPageProducesNoIssues)
{
    constexpr uint32_t kPageSize = 8192;
    std::vector<uint8_t> page_buffer(kPageSize, 0);
    ErrorContext ctx;
    HeapPage page(page_buffer.data(), kPageSize);
    ASSERT_EQ(page.initialize(100, &ctx), Status::OK);

    std::vector<uint8_t> payload(64, 0x5A);
    std::vector<uint8_t> tuple = buildTupleWithPayload(payload, 1000);
    uint16_t item_id = 0;
    ASSERT_EQ(page.insertTuple(tuple.data(), static_cast<uint32_t>(tuple.size()),
                               1000, &item_id, &ctx),
              Status::OK);

    HeapToastLobDiagnosticReport report;
    ASSERT_EQ(HeapToastLobDiagnostics::walkPage(page_buffer.data(), kPageSize, &report, &ctx),
              Status::OK);
    EXPECT_TRUE(report.ok());
    EXPECT_GE(report.scanned_items, 1u);
}

TEST(HeapToastLobPageWalkerContractTest, DetectsInvalidPayloadLength)
{
    constexpr uint32_t kPageSize = 8192;
    std::vector<uint8_t> page_buffer(kPageSize, 0);
    ErrorContext ctx;
    HeapPage page(page_buffer.data(), kPageSize);
    ASSERT_EQ(page.initialize(101, &ctx), Status::OK);

    std::vector<uint8_t> payload(32, 0x41);
    std::vector<uint8_t> tuple = buildTupleWithPayload(payload, 1001);
    uint16_t item_id = 0;
    ASSERT_EQ(page.insertTuple(tuple.data(), static_cast<uint32_t>(tuple.size()),
                               1001, &item_id, &ctx),
              Status::OK);

    auto *items = reinterpret_cast<ItemPointer *>(page_buffer.data() + sizeof(PageHeader));
    auto *tuple_hdr = reinterpret_cast<TupleHeader *>(page_buffer.data() + items[item_id].offset);
    tuple_hdr->payload_len = items[item_id].length; // larger than actual payload capacity

    HeapToastLobDiagnosticReport report;
    ASSERT_EQ(HeapToastLobDiagnostics::walkPage(page_buffer.data(), kPageSize, &report, &ctx),
              Status::PAGE_CORRUPT);

    bool found = false;
    for (const auto &issue : report.issues)
    {
        if (issue.code == HeapToastLobIssueCode::INVALID_PAYLOAD_LENGTH)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(HeapToastLobPageWalkerContractTest, DetectsToastFlagMismatch)
{
    constexpr uint32_t kPageSize = 8192;
    std::vector<uint8_t> page_buffer(kPageSize, 0);
    ErrorContext ctx;
    HeapPage page(page_buffer.data(), kPageSize);
    ASSERT_EQ(page.initialize(102, &ctx), Status::OK);

    ToastPointer ptr{};
    ptr.lob_uuid = generateUuidV7();
    ptr.total_len = 2048;
    ptr.chunk_size = 1024;
    ptr.compression = 0;
    ptr.flags = 0;

    std::vector<uint8_t> payload(sizeof(ToastPointer), 0);
    std::memcpy(payload.data(), &ptr, sizeof(ToastPointer));
    std::vector<uint8_t> tuple = buildTupleWithPayload(payload, 1002);

    uint16_t item_id = 0;
    ASSERT_EQ(page.insertTuple(tuple.data(), static_cast<uint32_t>(tuple.size()),
                               1002, &item_id, &ctx),
              Status::OK);

    auto *items = reinterpret_cast<ItemPointer *>(page_buffer.data() + sizeof(PageHeader));
    auto *tuple_hdr = reinterpret_cast<TupleHeader *>(page_buffer.data() + items[item_id].offset);
    tuple_hdr->record_flags &= ~TupleHeader::RHD_TOAST_PTR;

    HeapToastLobDiagnosticReport report;
    ASSERT_EQ(HeapToastLobDiagnostics::walkPage(page_buffer.data(), kPageSize, &report, &ctx),
              Status::PAGE_CORRUPT);

    bool found = false;
    for (const auto &issue : report.issues)
    {
        if (issue.code == HeapToastLobIssueCode::TOAST_FLAG_MISMATCH)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(HeapToastLobPageWalkerContractTest, ChunkSequenceValidationRejectsMissingChunk)
{
    ErrorContext ctx;
    std::vector<uint32_t> chunks = {0, 2};
    EXPECT_EQ(
        HeapToastLobDiagnostics::validateChunkSequence(chunks, 3 * 1024, 1024, &ctx),
        Status::NOT_FOUND);
}

TEST(HeapToastLobPageWalkerContractTest, ChunkSequenceValidationAcceptsContiguousSequence)
{
    ErrorContext ctx;
    std::vector<uint32_t> chunks = {0, 1, 2};
    EXPECT_EQ(
        HeapToastLobDiagnostics::validateChunkSequence(chunks, 3 * 1024, 1024, &ctx),
        Status::OK);
}
