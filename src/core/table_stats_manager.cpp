#include "scratchbird/core/table_stats_manager.h"
#include "scratchbird/core/database.h"
#include <chrono>

namespace scratchbird::core
{
    namespace
    {
        uint64_t nowMicros()
        {
            return std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                .count();
        }

        bool isZeroId(const ID& id)
        {
            for (uint8_t byte : id.bytes)
            {
                if (byte != 0)
                {
                    return false;
                }
            }
            return true;
        }
    } // namespace

    TableStatsManager::TableStatsManager(Database* db) : db_(db)
    {
    }

    void TableStatsManager::recordSeqScan(const ID& table_id)
    {
        if (isZeroId(table_id))
        {
            return;
        }
        auto stats = getOrCreate(table_id);
        stats->seq_scan_count.fetch_add(1, std::memory_order_relaxed);
        stats->last_seq_scan_at.store(static_cast<int64_t>(nowMicros()),
                                      std::memory_order_relaxed);
    }

    void TableStatsManager::recordIndexScan(const ID& table_id)
    {
        if (isZeroId(table_id))
        {
            return;
        }
        auto stats = getOrCreate(table_id);
        stats->idx_scan_count.fetch_add(1, std::memory_order_relaxed);
        stats->last_idx_scan_at.store(static_cast<int64_t>(nowMicros()),
                                      std::memory_order_relaxed);
    }

    void TableStatsManager::recordSeqRowsRead(const ID& table_id, uint64_t count)
    {
        if (isZeroId(table_id) || count == 0)
        {
            return;
        }
        auto stats = getOrCreate(table_id);
        stats->seq_rows_read.fetch_add(count, std::memory_order_relaxed);
    }

    void TableStatsManager::recordIndexRowsFetch(const ID& table_id, uint64_t count)
    {
        if (isZeroId(table_id) || count == 0)
        {
            return;
        }
        auto stats = getOrCreate(table_id);
        stats->idx_rows_fetch.fetch_add(count, std::memory_order_relaxed);
    }

    void TableStatsManager::applyCommittedDelta(const ID& table_id, const TableDmlDelta& delta)
    {
        if (isZeroId(table_id) || delta.empty())
        {
            return;
        }

        auto stats = getOrCreate(table_id);
        stats->rows_inserted.fetch_add(delta.inserts, std::memory_order_relaxed);
        stats->rows_updated.fetch_add(delta.updates, std::memory_order_relaxed);
        stats->rows_deleted.fetch_add(delta.deletes, std::memory_order_relaxed);
        stats->rows_hot_updated.fetch_add(delta.hot_updates, std::memory_order_relaxed);
        stats->rows_newpage_updated.fetch_add(delta.newpage_updates, std::memory_order_relaxed);

        uint64_t mod_total = delta.inserts + delta.updates + delta.deletes;
        if (mod_total > 0)
        {
            stats->mod_since_analyze.fetch_add(mod_total, std::memory_order_relaxed);
        }
        if (delta.inserts > 0)
        {
            stats->ins_since_vacuum.fetch_add(delta.inserts, std::memory_order_relaxed);
        }

        int64_t live = stats->live_rows_estimate.load(std::memory_order_relaxed);
        live += static_cast<int64_t>(delta.inserts);
        if (delta.deletes > 0)
        {
            int64_t deletes = static_cast<int64_t>(delta.deletes);
            live = (live > deletes) ? (live - deletes) : 0;
        }
        stats->live_rows_estimate.store(live, std::memory_order_relaxed);

        int64_t dead = stats->dead_rows_estimate.load(std::memory_order_relaxed);
        dead += static_cast<int64_t>(delta.deletes + delta.updates);
        stats->dead_rows_estimate.store(dead, std::memory_order_relaxed);
    }

    std::vector<TableStatsSnapshot> TableStatsManager::snapshot() const
    {
        std::vector<TableStatsSnapshot> out;
        std::lock_guard<std::mutex> lock(mutex_);
        out.reserve(stats_.size());
        for (const auto& [table_id, stats_ptr] : stats_)
        {
            if (!stats_ptr)
            {
                continue;
            }
            TableStatsSnapshot row;
            row.table_id = table_id;
            row.seq_scan_count = stats_ptr->seq_scan_count.load(std::memory_order_relaxed);
            row.last_seq_scan_at = stats_ptr->last_seq_scan_at.load(std::memory_order_relaxed);
            row.seq_rows_read = stats_ptr->seq_rows_read.load(std::memory_order_relaxed);
            row.idx_scan_count = stats_ptr->idx_scan_count.load(std::memory_order_relaxed);
            row.last_idx_scan_at = stats_ptr->last_idx_scan_at.load(std::memory_order_relaxed);
            row.idx_rows_fetch = stats_ptr->idx_rows_fetch.load(std::memory_order_relaxed);
            row.rows_inserted = stats_ptr->rows_inserted.load(std::memory_order_relaxed);
            row.rows_updated = stats_ptr->rows_updated.load(std::memory_order_relaxed);
            row.rows_deleted = stats_ptr->rows_deleted.load(std::memory_order_relaxed);
            row.rows_hot_updated = stats_ptr->rows_hot_updated.load(std::memory_order_relaxed);
            row.rows_newpage_updated = stats_ptr->rows_newpage_updated.load(std::memory_order_relaxed);
            row.live_rows_estimate = stats_ptr->live_rows_estimate.load(std::memory_order_relaxed);
            row.dead_rows_estimate = stats_ptr->dead_rows_estimate.load(std::memory_order_relaxed);
            row.mod_since_analyze = stats_ptr->mod_since_analyze.load(std::memory_order_relaxed);
            row.ins_since_vacuum = stats_ptr->ins_since_vacuum.load(std::memory_order_relaxed);
            row.last_vacuum_at = stats_ptr->last_vacuum_at.load(std::memory_order_relaxed);
            row.last_autovacuum_at = stats_ptr->last_autovacuum_at.load(std::memory_order_relaxed);
            row.last_analyze_at = stats_ptr->last_analyze_at.load(std::memory_order_relaxed);
            row.last_autoanalyze_at = stats_ptr->last_autoanalyze_at.load(std::memory_order_relaxed);
            row.vacuum_count = stats_ptr->vacuum_count.load(std::memory_order_relaxed);
            row.autovacuum_count = stats_ptr->autovacuum_count.load(std::memory_order_relaxed);
            row.analyze_count = stats_ptr->analyze_count.load(std::memory_order_relaxed);
            row.autoanalyze_count = stats_ptr->autoanalyze_count.load(std::memory_order_relaxed);
            row.total_vacuum_time_ms =
                stats_ptr->total_vacuum_time_ms.load(std::memory_order_relaxed);
            row.total_autovacuum_time_ms =
                stats_ptr->total_autovacuum_time_ms.load(std::memory_order_relaxed);
            row.total_analyze_time_ms =
                stats_ptr->total_analyze_time_ms.load(std::memory_order_relaxed);
            row.total_autoanalyze_time_ms =
                stats_ptr->total_autoanalyze_time_ms.load(std::memory_order_relaxed);
            out.push_back(row);
        }

        return out;
    }

    std::shared_ptr<TableStatsManager::TableStats> TableStatsManager::getOrCreate(const ID& table_id)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = stats_.find(table_id);
        if (it == stats_.end())
        {
            it = stats_.emplace(table_id, std::make_shared<TableStats>()).first;
        }
        return it->second;
    }
} // namespace scratchbird::core
