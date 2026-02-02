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

#include <atomic>
#include <cstdint>

namespace scratchbird::core
{
    struct IOStatsSnapshot
    {
        uint64_t page_reads = 0;
        uint64_t page_writes = 0;
        uint64_t page_fetches = 0;
        uint64_t page_marks = 0;
    };

    struct IOStats
    {
        std::atomic<uint64_t> page_reads{0};
        std::atomic<uint64_t> page_writes{0};
        std::atomic<uint64_t> page_fetches{0};
        std::atomic<uint64_t> page_marks{0};

        IOStats() = default;
        IOStats(const IOStats& other)
        {
            page_reads.store(other.page_reads.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
            page_writes.store(other.page_writes.load(std::memory_order_relaxed),
                              std::memory_order_relaxed);
            page_fetches.store(other.page_fetches.load(std::memory_order_relaxed),
                               std::memory_order_relaxed);
            page_marks.store(other.page_marks.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
        }

        IOStats& operator=(const IOStats& other)
        {
            if (this != &other)
            {
                page_reads.store(other.page_reads.load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
                page_writes.store(other.page_writes.load(std::memory_order_relaxed),
                                  std::memory_order_relaxed);
                page_fetches.store(other.page_fetches.load(std::memory_order_relaxed),
                                   std::memory_order_relaxed);
                page_marks.store(other.page_marks.load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
            }
            return *this;
        }

        void reset()
        {
            page_reads.store(0, std::memory_order_relaxed);
            page_writes.store(0, std::memory_order_relaxed);
            page_fetches.store(0, std::memory_order_relaxed);
            page_marks.store(0, std::memory_order_relaxed);
        }

        void recordRead()
        {
            page_reads.fetch_add(1, std::memory_order_relaxed);
        }

        void recordWrite()
        {
            page_writes.fetch_add(1, std::memory_order_relaxed);
        }

        void recordFetch()
        {
            page_fetches.fetch_add(1, std::memory_order_relaxed);
        }

        void recordMark()
        {
            page_marks.fetch_add(1, std::memory_order_relaxed);
        }

        IOStatsSnapshot snapshot() const
        {
            IOStatsSnapshot out;
            out.page_reads = page_reads.load(std::memory_order_relaxed);
            out.page_writes = page_writes.load(std::memory_order_relaxed);
            out.page_fetches = page_fetches.load(std::memory_order_relaxed);
            out.page_marks = page_marks.load(std::memory_order_relaxed);
            return out;
        }
    };

    struct TableDmlDelta
    {
        uint64_t inserts = 0;
        uint64_t updates = 0;
        uint64_t deletes = 0;
        uint64_t hot_updates = 0;
        uint64_t newpage_updates = 0;

        bool empty() const
        {
            return inserts == 0 && updates == 0 && deletes == 0 &&
                   hot_updates == 0 && newpage_updates == 0;
        }

        void add(const TableDmlDelta& other)
        {
            inserts += other.inserts;
            updates += other.updates;
            deletes += other.deletes;
            hot_updates += other.hot_updates;
            newpage_updates += other.newpage_updates;
        }
    };
} // namespace scratchbird::core
