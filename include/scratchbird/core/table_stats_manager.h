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

#include "scratchbird/core/monitoring_stats.h"
#include "scratchbird/core/uuidv7.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace scratchbird::core
{
    class Database;

    struct TableStatsSnapshot
    {
        ID table_id{};
        uint64_t seq_scan_count = 0;
        int64_t last_seq_scan_at = 0;
        uint64_t seq_rows_read = 0;
        uint64_t idx_scan_count = 0;
        int64_t last_idx_scan_at = 0;
        uint64_t idx_rows_fetch = 0;
        uint64_t rows_inserted = 0;
        uint64_t rows_updated = 0;
        uint64_t rows_deleted = 0;
        uint64_t rows_hot_updated = 0;
        uint64_t rows_newpage_updated = 0;
        int64_t live_rows_estimate = 0;
        int64_t dead_rows_estimate = 0;
        uint64_t mod_since_analyze = 0;
        uint64_t ins_since_vacuum = 0;
        int64_t last_vacuum_at = 0;
        int64_t last_autovacuum_at = 0;
        int64_t last_analyze_at = 0;
        int64_t last_autoanalyze_at = 0;
        uint64_t vacuum_count = 0;
        uint64_t autovacuum_count = 0;
        uint64_t analyze_count = 0;
        uint64_t autoanalyze_count = 0;
        uint64_t total_vacuum_time_ms = 0;
        uint64_t total_autovacuum_time_ms = 0;
        uint64_t total_analyze_time_ms = 0;
        uint64_t total_autoanalyze_time_ms = 0;
    };

    class TableStatsManager
    {
    public:
        explicit TableStatsManager(Database* db);

        void recordSeqScan(const ID& table_id);
        void recordIndexScan(const ID& table_id);
        void recordSeqRowsRead(const ID& table_id, uint64_t count);
        void recordIndexRowsFetch(const ID& table_id, uint64_t count);
        void applyCommittedDelta(const ID& table_id, const TableDmlDelta& delta);

        std::vector<TableStatsSnapshot> snapshot() const;

    private:
        struct TableStats
        {
            std::atomic<uint64_t> seq_scan_count{0};
            std::atomic<int64_t> last_seq_scan_at{0};
            std::atomic<uint64_t> seq_rows_read{0};
            std::atomic<uint64_t> idx_scan_count{0};
            std::atomic<int64_t> last_idx_scan_at{0};
            std::atomic<uint64_t> idx_rows_fetch{0};
            std::atomic<uint64_t> rows_inserted{0};
            std::atomic<uint64_t> rows_updated{0};
            std::atomic<uint64_t> rows_deleted{0};
            std::atomic<uint64_t> rows_hot_updated{0};
            std::atomic<uint64_t> rows_newpage_updated{0};
            std::atomic<int64_t> live_rows_estimate{0};
            std::atomic<int64_t> dead_rows_estimate{0};
            std::atomic<uint64_t> mod_since_analyze{0};
            std::atomic<uint64_t> ins_since_vacuum{0};
            std::atomic<int64_t> last_vacuum_at{0};
            std::atomic<int64_t> last_autovacuum_at{0};
            std::atomic<int64_t> last_analyze_at{0};
            std::atomic<int64_t> last_autoanalyze_at{0};
            std::atomic<uint64_t> vacuum_count{0};
            std::atomic<uint64_t> autovacuum_count{0};
            std::atomic<uint64_t> analyze_count{0};
            std::atomic<uint64_t> autoanalyze_count{0};
            std::atomic<uint64_t> total_vacuum_time_ms{0};
            std::atomic<uint64_t> total_autovacuum_time_ms{0};
            std::atomic<uint64_t> total_analyze_time_ms{0};
            std::atomic<uint64_t> total_autoanalyze_time_ms{0};
        };

        std::shared_ptr<TableStats> getOrCreate(const ID& table_id);

        Database* db_ = nullptr;
        mutable std::mutex mutex_;
        std::unordered_map<ID, std::shared_ptr<TableStats>, IDHash> stats_;
    };
} // namespace scratchbird::core
