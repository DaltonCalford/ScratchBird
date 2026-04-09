#pragma once

#include "scratchbird/core/uuidv7.h"
#include <cstdint>
#include <string>

namespace scratchbird::core
{
    // GC publication records are emitted after MGA truth is already known.
    // They publish cleanup debt or completion state to downstream consumers and
    // are not redo/WAL records.
    enum class IndexCleanupFamily : uint8_t
    {
        EXACT = 0,
        SUMMARY = 1,
        APPROXIMATE = 2
    };

    enum class IndexCleanupPublicationState : uint8_t
    {
        COMPLETE = 0,
        DEBT_PUBLISHED = 1
    };

    struct HeapReclaimPublicationContext
    {
        uint64_t sweep_generation = 0;
        uint64_t checkpoint_generation = 0;
    };

    struct IndexCleanupPublicationRecord
    {
        UuidV7Bytes table_id{};
        UuidV7Bytes index_id{};
        std::string index_name;
        uint32_t page_id = 0;
        uint32_t locality_page_id = 0;
        IndexCleanupFamily family = IndexCleanupFamily::EXACT;
        IndexCleanupPublicationState state = IndexCleanupPublicationState::COMPLETE;
        uint64_t heap_reclaim_count = 0;
        uint64_t entries_removed = 0;
        uint64_t backlog_count = 0;
        uint64_t backlog_pages = 0;
        uint64_t backlog_bytes = 0;
        bool repair_required = false;
        uint64_t sweep_generation = 0;
        uint64_t checkpoint_generation = 0;
        uint64_t published_at_us = 0;
    };

    struct IndexCleanupPublicationSummary
    {
        uint64_t exact_entries_removed = 0;
        uint64_t exact_family_completed = 0;
        uint64_t summary_family_backlog_published = 0;
        uint64_t approximate_family_backlog_published = 0;
        uint64_t backlog_count = 0;
        uint64_t backlog_pages = 0;
        uint64_t backlog_bytes = 0;
    };
} // namespace scratchbird::core
