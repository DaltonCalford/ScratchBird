/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/heap_toast_lob_diagnostics.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/toast.h"
#include "scratchbird/core/error_context.h"
#include <algorithm>

namespace scratchbird::core
{
    namespace
    {
        void pushIssue(HeapToastLobDiagnosticReport *report, HeapToastLobIssueCode code,
                       uint16_t item_id, uint32_t offset)
        {
            if (report == nullptr)
            {
                return;
            }
            report->issues.push_back(HeapToastLobIssue{code, item_id, offset});
        }
    } // namespace

    auto HeapToastLobDiagnostics::walkPage(const uint8_t *page_data, uint32_t page_size,
                                           HeapToastLobDiagnosticReport *report,
                                           ErrorContext *ctx) -> Status
    {
        if (page_data == nullptr || report == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "walkPage requires non-null inputs");
            return Status::INVALID_ARGUMENT;
        }

        report->issues.clear();
        report->scanned_items = 0;

        const auto *hdr = reinterpret_cast<const PageHeader *>(page_data);
        report->page_id = hdr->page_id;
        report->page_type = hdr->page_type;

        if (hdr->magic != K_MAGIC_SBRD || hdr->page_size != page_size)
        {
            pushIssue(report, HeapToastLobIssueCode::INVALID_PAGE_HEADER, 0, 0);
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "INVALID_PAGE_HEADER");
            return Status::PAGE_CORRUPT;
        }

        const bool page_type_supported =
            hdr->page_type == PAGE_TYPE_HEAP ||
            hdr->page_type == PAGE_TYPE_TOAST_CHUNK ||
            hdr->page_type == PAGE_TYPE_LOB_CHUNK;
        if (!page_type_supported)
        {
            pushIssue(report, HeapToastLobIssueCode::INVALID_PAGE_TYPE, 0, 0);
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "INVALID_PAGE_TYPE");
            return Status::INVALID_ARGUMENT;
        }

        const auto *items =
            reinterpret_cast<const ItemPointer *>(page_data + sizeof(PageHeader));
        const uint16_t item_count = static_cast<uint16_t>(
            (pageLower(*hdr) - sizeof(PageHeader)) / sizeof(ItemPointer));
        report->scanned_items = item_count;

        for (uint16_t i = 0; i < item_count; ++i)
        {
            const ItemPointer &item = items[i];
            if (item.isUnused() || item.isDeleted())
            {
                continue;
            }

            if (!item.isValid(page_size))
            {
                pushIssue(report, HeapToastLobIssueCode::INVALID_ITEM_POINTER, i, item.offset);
                continue;
            }
            if (item.length < sizeof(TupleHeader))
            {
                pushIssue(report, HeapToastLobIssueCode::INVALID_TUPLE_HEADER, i, item.offset);
                continue;
            }

            const auto *tuple_hdr = reinterpret_cast<const TupleHeader *>(page_data + item.offset);
            const uint32_t payload_capacity = item.length - sizeof(TupleHeader);
            if (tuple_hdr->payload_len > payload_capacity)
            {
                pushIssue(report, HeapToastLobIssueCode::INVALID_PAYLOAD_LENGTH, i, item.offset);
            }

            const uint8_t *payload = page_data + item.offset + sizeof(TupleHeader);
            const bool payload_is_toast_pointer =
                payload_capacity >= sizeof(ToastPointer) &&
                ToastManager::isToastPointer(payload, sizeof(ToastPointer));
            const bool toast_flag_set = tuple_hdr->hasRecordFlag(TupleHeader::RHD_TOAST_PTR);

            if (toast_flag_set && !payload_is_toast_pointer)
            {
                pushIssue(report, HeapToastLobIssueCode::INVALID_TOAST_POINTER, i, item.offset);
            }
            if (!toast_flag_set && payload_is_toast_pointer)
            {
                pushIssue(report, HeapToastLobIssueCode::TOAST_FLAG_MISMATCH, i, item.offset);
            }
        }

        return report->ok() ? Status::OK : Status::PAGE_CORRUPT;
    }

    auto HeapToastLobDiagnostics::validateChunkSequence(const std::vector<uint32_t> &chunk_indices,
                                                        uint64_t total_len, uint32_t chunk_size,
                                                        ErrorContext *ctx) -> Status
    {
        if (chunk_size == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "chunk_size must be non-zero");
            return Status::INVALID_ARGUMENT;
        }

        if (total_len == 0 && !chunk_indices.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "non-empty chunk sequence for empty LOB");
            return Status::INVALID_ARGUMENT;
        }

        const uint64_t expected_count = expectedLobOrToastChunkCount(total_len, chunk_size);
        if (chunk_indices.size() != expected_count)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "LOB_CHUNK_MISSING");
            return Status::NOT_FOUND;
        }

        std::vector<uint32_t> sorted = chunk_indices;
        std::sort(sorted.begin(), sorted.end());
        for (uint64_t i = 0; i < expected_count; ++i)
        {
            if (sorted[static_cast<size_t>(i)] != i)
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "LOB_CHUNK_MISSING");
                return Status::NOT_FOUND;
            }
        }

        return Status::OK;
    }
} // namespace scratchbird::core

