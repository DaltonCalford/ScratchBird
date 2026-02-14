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

#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include <cstdint>
#include <vector>

namespace scratchbird::core
{
    struct ErrorContext;

    enum class HeapToastLobIssueCode : uint16_t
    {
        INVALID_ARGUMENT = 1,
        INVALID_PAGE_HEADER = 2,
        INVALID_PAGE_TYPE = 3,
        INVALID_ITEM_POINTER = 4,
        INVALID_TUPLE_HEADER = 5,
        INVALID_PAYLOAD_LENGTH = 6,
        INVALID_TOAST_POINTER = 7,
        TOAST_FLAG_MISMATCH = 8,
        LOB_CHUNK_MISSING = 9
    };

    struct HeapToastLobIssue
    {
        HeapToastLobIssueCode code = HeapToastLobIssueCode::INVALID_ARGUMENT;
        uint16_t item_id = 0;
        uint32_t offset = 0;
    };

    struct HeapToastLobDiagnosticReport
    {
        uint32_t page_id = 0;
        uint16_t page_type = 0;
        uint32_t scanned_items = 0;
        std::vector<HeapToastLobIssue> issues;

        [[nodiscard]] auto ok() const -> bool
        {
            return issues.empty();
        }
    };

    class HeapToastLobDiagnostics
    {
    public:
        static auto walkPage(const uint8_t *page_data, uint32_t page_size,
                             HeapToastLobDiagnosticReport *report,
                             ErrorContext *ctx = nullptr) -> Status;

        static auto validateChunkSequence(const std::vector<uint32_t> &chunk_indices,
                                          uint64_t total_len, uint32_t chunk_size,
                                          ErrorContext *ctx = nullptr) -> Status;
    };
} // namespace scratchbird::core

