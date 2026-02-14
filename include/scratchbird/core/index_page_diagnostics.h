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
#include <vector>

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/status.h"

namespace scratchbird::core
{

enum class IndexPageIssueCode : uint16_t
{
    INVALID_PAGE_HEADER = 1,
    INVALID_PAGE_TYPE = 2,
    INVALID_PAGE_SIZE = 3,
    INVALID_CHECKSUM = 4,
    INVALID_INDEX_PAGE_HEADER = 5,
    INVALID_INDEX_PAGE_LEVEL = 6,
    INVALID_INDEX_OPAQUE_SIZE = 7,
    INVALID_INDEX_SIBLING = 8,
    INVALID_INDEX_UUID = 9
};

struct IndexPageIssue
{
    IndexPageIssueCode code;
    uint32_t offset;
    uint32_t detail;
};

struct IndexPageDiagnosticReport
{
    std::vector<IndexPageIssue> issues;

    [[nodiscard]] auto ok() const -> bool
    {
        return issues.empty();
    }
};

struct IndexPageWalkEntry
{
    uint32_t page_id;
    Status status;
    uint32_t issue_count;
};

struct IndexPageWalkReport
{
    std::vector<IndexPageWalkEntry> entries;
    uint32_t pages_ok{0};
    uint32_t pages_failed{0};

    [[nodiscard]] auto ok() const -> bool
    {
        return pages_failed == 0u;
    }
};

class IndexPageDiagnostics
{
public:
    static auto validatePage(const uint8_t *page, uint32_t expected_page_size,
                             uint16_t expected_page_type, uint16_t expected_opaque_len,
                             const ID *expected_index_uuid, IndexPageDiagnosticReport *report,
                             ErrorContext *ctx) -> Status;

    static auto walkPages(const std::vector<const uint8_t *> &pages, uint32_t expected_page_size,
                          uint16_t expected_page_type, uint16_t expected_opaque_len,
                          const ID *expected_index_uuid, IndexPageWalkReport *report,
                          ErrorContext *ctx) -> Status;
};

} // namespace scratchbird::core
