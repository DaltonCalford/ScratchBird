#include "scratchbird/engine/index_online.h"

#include "scratchbird/engine/catalog_mem.h"
#include "scratchbird/engine/file.h"
#include "scratchbird/engine/index_btree.h"
#include "scratchbird/engine/wal.h"

#include <chrono>
#include <thread>

namespace scratchbird::engine
{

    ConcurrentIndexBuild::ConcurrentIndexBuild(IndexDefinition def, IndexBuildOptions opts,
                                               std::string base_path)
        : def_(std::move(def)), opts_(opts), base_path_(std::move(base_path))
    {
        FileOptions fo{};
        fo.direct_io = false;
        fo.preallocate_bytes = 0;
        FileMap::Layout layout{};
        layout.page_size = opts_.page_size;
        layout.pages_per_segment = 4096;
        layout.options = fo;
        FileMap fmap(layout);
        std::string dir, stem;
        auto slash = base_path_.find_last_of('/');
        if (slash == std::string::npos) {
            dir = ".";
            stem = base_path_;
        } else {
            dir = base_path_.substr(0, slash);
            stem = base_path_.substr(slash + 1);
        }
        fmap.set_base_path(dir, stem);

        staging_ = std::make_unique<BTreeIndex>(std::move(fmap), layout.page_size, def_.unique);
        staging_->create_empty();
        state_ = OnlineBuildState::Registered;
        IndexBuildMonitor::upsert(
            IndexBuildMonRow{def_.index_name, def_.relation_name, state_.load(), 0, 0});

        // Subscribe to global WAL to capture inserts/deletes while online build progresses.
        WalManager::register_global_listener([this](const WalRecord& rec) {
            if (rec.kind == WalRecKind::Insert) {
                // For demo: treat key_bytes as a UTF-8 string key
                std::string key(rec.key_bytes.begin(), rec.key_bytes.end());
                this->register_delta(IndexDelta{key, rec.row_id, false});
            } else if (rec.kind == WalRecKind::Delete) {
                std::string key(rec.key_bytes.begin(), rec.key_bytes.end());
                this->register_delta(IndexDelta{key, 0, true});
            }
        });
    }

    ConcurrentIndexBuild::~ConcurrentIndexBuild() = default;

    void ConcurrentIndexBuild::register_delta(const IndexDelta& d)
    {
        std::lock_guard<std::mutex> lg(mu_);
        if (aborted_ || state_ == OnlineBuildState::Swapped)
            return;
        q_.push(d);
        cv_.notify_all();
    }

    bool ConcurrentIndexBuild::run_backfill(
        const std::function<void(const std::function<void(const std::string&, std::uint64_t)>&)>&
            scan_fn)
    {
        state_ = OnlineBuildState::Backfill;
        IndexBuildMonitor::upsert(IndexBuildMonRow{def_.index_name, def_.relation_name,
                                                   state_.load(), backfill_rows_.load(),
                                                   delta_applied_.load()});
        std::atomic<bool> ok{true};
        auto emit = [&](const std::string& key, std::uint64_t row_id) {
            if (!ok.load())
                return;
            std::string err;
            if (!staging_->insert(key, row_id, err)) {
                ok = false;
            }
            backfill_rows_.fetch_add(1, std::memory_order_relaxed);
        };
        scan_fn(emit);
        if (!ok.load()) {
            aborted_ = true;
            state_ = OnlineBuildState::Aborted;
            IndexBuildMonitor::upsert(IndexBuildMonRow{def_.index_name, def_.relation_name,
                                                       state_.load(), backfill_rows_.load(),
                                                       delta_applied_.load()});
            return false;
        }
        state_ = OnlineBuildState::Catchup;
        IndexBuildMonitor::upsert(IndexBuildMonRow{def_.index_name, def_.relation_name,
                                                   state_.load(), backfill_rows_.load(),
                                                   delta_applied_.load()});
        return true;
    }

    void ConcurrentIndexBuild::apply_delta_locked(const IndexDelta& d)
    {
        std::string err;
        if (d.is_delete) {
            std::string e2;
            staging_->erase_equal(d.key, e2);
        } else {
            staging_->insert(d.key, d.row_id, err);
        }
        delta_applied_.fetch_add(1, std::memory_order_relaxed);
    }

    bool ConcurrentIndexBuild::run_catchup_until_quiet(std::chrono::milliseconds quiet_for,
                                                       std::size_t max_queue)
    {
        auto last_small = std::chrono::steady_clock::now();
        for (;;) {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait_for(lk, std::chrono::milliseconds(50), [&] {
                return !q_.empty() || aborted_ || state_ == OnlineBuildState::Fence;
            });
            if (aborted_) {
                state_ = OnlineBuildState::Aborted;
                IndexBuildMonitor::upsert(IndexBuildMonRow{def_.index_name, def_.relation_name,
                                                           state_.load(), backfill_rows_.load(),
                                                           delta_applied_.load()});
                return false;
            }
            while (!q_.empty()) {
                auto d = q_.front();
                q_.pop();
                auto dcopy = d;
                lk.unlock();
                apply_delta_locked(dcopy);
                lk.lock();
            }
            IndexBuildMonitor::upsert(IndexBuildMonRow{def_.index_name, def_.relation_name,
                                                       state_.load(), backfill_rows_.load(),
                                                       delta_applied_.load()});
            auto now = std::chrono::steady_clock::now();
            bool small = max_queue == 0 ? q_.empty() : (q_.size() <= max_queue);
            if (small) {
                if (now - last_small >= quiet_for)
                    break;
            } else {
                last_small = now;
            }
            if (state_ == OnlineBuildState::Fence)
                break;
        }
        return true;
    }

    bool ConcurrentIndexBuild::fence_and_swap(const std::function<void()>& fence_begin,
                                              const std::function<void()>& fence_end)
    {
        state_ = OnlineBuildState::Fence;
        IndexBuildMonitor::upsert(IndexBuildMonRow{def_.index_name, def_.relation_name,
                                                   state_.load(), backfill_rows_.load(),
                                                   delta_applied_.load()});
        fence_begin();
        {
            std::lock_guard<std::mutex> lg(mu_);
            while (!q_.empty()) {
                auto d = q_.front();
                q_.pop();
                apply_delta_locked(d);
            }
        }
        // Persist to in-memory catalog and mark active/valid
        IndexCatalogEntry ice{};
        ice.name = def_.index_name;
        ice.relation = def_.relation_name;
        ice.method = def_.method;
        ice.active = true;
        ice.valid = true;
        ice.root_page = staging_->root_page();
        CatalogMem::put_index(ice);
        auto st = staging_->compute_stats();
        StatsEntry se{};
        se.height = st.height;
        se.leaf_pages = st.leaf_pages;
        se.branch_pages = st.branch_pages;
        se.key_count = st.key_count;
        CatalogMem::put_stats(def_.index_name, se);
        state_ = OnlineBuildState::Swapped;
        IndexBuildMonitor::upsert(IndexBuildMonRow{def_.index_name, def_.relation_name,
                                                   state_.load(), backfill_rows_.load(),
                                                   delta_applied_.load()});
        fence_end();
        return true;
    }

    // IndexBuildMonitor implementation
    std::mutex IndexBuildMonitor::mu_{};
    std::unordered_map<std::string, IndexBuildMonRow> IndexBuildMonitor::rows_{};

    void IndexBuildMonitor::upsert(const IndexBuildMonRow& r)
    {
        std::lock_guard<std::mutex> lg(mu_);
        rows_[r.index_name] = r;
    }

    std::vector<IndexBuildMonRow> IndexBuildMonitor::snapshot()
    {
        std::lock_guard<std::mutex> lg(mu_);
        std::vector<IndexBuildMonRow> out;
        out.reserve(rows_.size());
        for (auto& [k, v] : rows_)
            out.push_back(v);
        return out;
    }

    std::string IndexBuildMonitor::state_to_string(OnlineBuildState s)
    {
        switch (s) {
        case OnlineBuildState::Registered:
            return "REGISTERED";
        case OnlineBuildState::Backfill:
            return "BACKFILL";
        case OnlineBuildState::Catchup:
            return "CATCHUP";
        case OnlineBuildState::Fence:
            return "FENCE";
        case OnlineBuildState::Swapped:
            return "SWAPPED";
        case OnlineBuildState::Aborted:
            return "ABORTED";
        }
        return "UNKNOWN";
    }

    std::vector<std::vector<std::string>> IndexBuildMonitor::rows_as_table()
    {
        auto snap = snapshot();
        std::vector<std::vector<std::string>> rows;
        rows.reserve(snap.size());
        for (auto& r : snap) {
            rows.push_back({r.index_name, r.relation, state_to_string(r.state),
                            std::to_string(r.backfill_rows), std::to_string(r.delta_applied)});
        }
        return rows;
    }

} // namespace scratchbird::engine
