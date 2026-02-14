/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "scratchbird/core/index_page_diagnostics.h"

namespace scratchbird::core
{

namespace
{

void pushIssue(IndexPageDiagnosticReport *report, IndexPageIssueCode code, uint32_t offset,
               uint32_t detail)
{
    if (report == nullptr)
    {
        return;
    }
    report->issues.push_back(IndexPageIssue{code, offset, detail});
}

} // namespace

auto IndexPageDiagnostics::validatePage(const uint8_t *page, uint32_t expected_page_size,
                                        uint16_t expected_page_type, uint16_t expected_opaque_len,
                                        const ID *expected_index_uuid,
                                        IndexPageDiagnosticReport *report,
                                        ErrorContext *ctx) -> Status
{
    if (report == nullptr || page == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "INVALID_ARGUMENT");
        return Status::INVALID_ARGUMENT;
    }
    report->issues.clear();

    const auto *header = reinterpret_cast<const PageHeader *>(page);
    if (header->magic != K_MAGIC_SBRD)
    {
        pushIssue(report, IndexPageIssueCode::INVALID_PAGE_HEADER, 0u, header->magic);
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "INVALID_PAGE_HEADER");
        return Status::PAGE_CORRUPT;
    }

    if (!isValidAlphaPageSize(header->page_size) || header->page_size != expected_page_size)
    {
        pushIssue(report, IndexPageIssueCode::INVALID_PAGE_SIZE, 0u, header->page_size);
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "INVALID_PAGE_SIZE");
        return Status::PAGE_CORRUPT;
    }

    if (header->page_type != expected_page_type || !isCanonicalIndexPageType(header->page_type))
    {
        pushIssue(report, IndexPageIssueCode::INVALID_PAGE_TYPE, 0u, header->page_type);
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "INVALID_PAGE_TYPE");
        return Status::PAGE_CORRUPT;
    }

    if ((header->flags & PAGE_FLAG_CHECKSUM_VALID) != 0u &&
        !validatePageChecksum(page, expected_page_size))
    {
        pushIssue(report, IndexPageIssueCode::INVALID_CHECKSUM, 0x0Cu, header->checksum);
        SET_ERROR_CONTEXT(ctx, Status::CHECKSUM_MISMATCH, "INVALID_CHECKSUM");
        return Status::CHECKSUM_MISMATCH;
    }

    if (header->special_size < sizeof(IndexPageHeader))
    {
        pushIssue(report, IndexPageIssueCode::INVALID_INDEX_PAGE_HEADER, 0u,
                  header->special_size);
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "INVALID_INDEX_PAGE_HEADER");
        return Status::PAGE_CORRUPT;
    }

    const uint32_t special_offset = pageSpecial(*header);
    if (special_offset > expected_page_size ||
        special_offset + sizeof(IndexPageHeader) > expected_page_size)
    {
        pushIssue(report, IndexPageIssueCode::INVALID_INDEX_PAGE_HEADER, special_offset,
                  expected_page_size);
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "INVALID_INDEX_PAGE_HEADER");
        return Status::PAGE_CORRUPT;
    }

    const auto *index_header = reinterpret_cast<const IndexPageHeader *>(page + special_offset);
    if (!isValidIndexPageFlags(index_header->flags) || index_header->reserved != 0u)
    {
        pushIssue(report, IndexPageIssueCode::INVALID_INDEX_PAGE_HEADER, special_offset,
                  index_header->flags);
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "INVALID_INDEX_PAGE_HEADER");
        return Status::PAGE_CORRUPT;
    }

    if (index_header->opaque_len != expected_opaque_len)
    {
        pushIssue(report, IndexPageIssueCode::INVALID_INDEX_OPAQUE_SIZE, special_offset + 0x1Cu,
                  index_header->opaque_len);
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "INVALID_INDEX_OPAQUE_SIZE");
        return Status::PAGE_CORRUPT;
    }

    if (!isValidIndexSiblingContract(*index_header))
    {
        pushIssue(report, IndexPageIssueCode::INVALID_INDEX_SIBLING, special_offset + 0x14u, 0u);
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "INVALID_INDEX_PAGE_HEADER");
        return Status::PAGE_CORRUPT;
    }

    if (expected_index_uuid != nullptr)
    {
        const ID index_uuid = getIndexPageHeaderUuid(*index_header);
        if (index_uuid != *expected_index_uuid)
        {
            pushIssue(report, IndexPageIssueCode::INVALID_INDEX_UUID, special_offset,
                      static_cast<uint32_t>(index_header->page_level));
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "INVALID_INDEX_PAGE_HEADER");
            return Status::PAGE_CORRUPT;
        }
    }

    return Status::OK;
}

auto IndexPageDiagnostics::walkPages(const std::vector<const uint8_t *> &pages,
                                     uint32_t expected_page_size, uint16_t expected_page_type,
                                     uint16_t expected_opaque_len,
                                     const ID *expected_index_uuid, IndexPageWalkReport *report,
                                     ErrorContext *ctx) -> Status
{
    if (report == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "INVALID_ARGUMENT");
        return Status::INVALID_ARGUMENT;
    }

    report->entries.clear();
    report->pages_ok = 0u;
    report->pages_failed = 0u;

    Status first_error = Status::OK;
    for (const uint8_t *page : pages)
    {
        IndexPageDiagnosticReport page_report{};
        ErrorContext local_ctx{};
        const Status status = validatePage(page, expected_page_size, expected_page_type,
                                           expected_opaque_len, expected_index_uuid, &page_report,
                                           &local_ctx);

        const uint32_t page_id =
            (page == nullptr)
                ? 0u
                : reinterpret_cast<const PageHeader *>(page)->page_id;

        report->entries.push_back(
            IndexPageWalkEntry{page_id, status, static_cast<uint32_t>(page_report.issues.size())});
        if (status == Status::OK)
        {
            report->pages_ok += 1u;
        }
        else
        {
            report->pages_failed += 1u;
            if (first_error == Status::OK)
            {
                first_error = status;
            }
        }
    }

    if (first_error != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, first_error, "INDEX_PAGE_WALK_FAILED");
        return first_error;
    }
    return Status::OK;
}

} // namespace scratchbird::core
