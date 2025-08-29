#include "scratchbird/engine/buffer_pool.h"

#include <algorithm>

namespace scratchbird::engine
{

    BufferFrame*
    BufferHandle::frame()
    {
        return (pool_ && index_ >= 0) ? pool_->frames_[index_].get() : nullptr;
    }

    const BufferFrame*
    BufferHandle::frame() const
    {
        return (pool_ && index_ >= 0) ? pool_->frames_[index_].get() : nullptr;
    }

    void BufferHandle::mark_dirty()
    {
        if (pool_ && index_ >= 0)
            pool_->mark_dirty(index_);
    }

    void BufferHandle::release()
    {
        if (pool_ && index_ >= 0) {
            pool_->dec_ref(index_);
            pool_ = nullptr;
            index_ = -1;
        }
    }

    BufferHandle
    BufferPool::get(const BufferTag& tag)
    {
        std::lock_guard<std::mutex> lg(mu_);
        int idx = find_or_create_index_locked(tag);
        if (idx >= 0)
            inc_ref(idx);
        return BufferHandle(this, idx);
    }

    int BufferPool::find_or_create_index_locked(const BufferTag& tag)
    {
        auto it = tag_to_index_.find(tag);
        if (it != tag_to_index_.end()) {
            hits_.fetch_add(1, std::memory_order_relaxed);
            return it->second;
        }
        misses_.fetch_add(1, std::memory_order_relaxed);
        // Find a free frame
        for (int i = 0; i < static_cast<int>(capacity_); ++i) {
            if (!frames_[i]->valid.load(std::memory_order_acquire) &&
                frames_[i]->refcount.load(std::memory_order_acquire) == 0) {
                frames_[i]->tag = tag;
                frames_[i]->valid.store(true, std::memory_order_release);
                frames_[i]->clock = 1;
                tag_to_index_[tag] = i;
                return i;
            }
        }
        // Choose a victim via clock-sweep
        int victim = choose_victim_locked();
        if (victim < 0)
            return -1;
        // Flush dirty if needed
        if (frames_[victim]->dirty.load(std::memory_order_acquire) && flush_cb_) {
            flush_cb_(*frames_[victim]);
            frames_[victim]->dirty.store(false, std::memory_order_release);
            flushes_.fetch_add(1, std::memory_order_relaxed);
        }
        // Remove old mapping
        auto old = frames_[victim]->tag;
        tag_to_index_.erase(old);
        // Install new tag
        frames_[victim]->tag = tag;
        frames_[victim]->valid.store(true, std::memory_order_release);
        frames_[victim]->clock = 1;
        tag_to_index_[tag] = victim;
        evictions_.fetch_add(1, std::memory_order_relaxed);
        return victim;
    }

    int BufferPool::choose_victim_locked()
    {
        int start = clock_hand_;
        const int n = static_cast<int>(capacity_);
        for (int pass = 0; pass < 2; ++pass) {
            for (int i = 0; i < n; ++i) {
                int idx = (start + i) % n;
                auto& f = frames_[idx];
                if (f->refcount.load(std::memory_order_acquire) > 0)
                    continue;
                if (f->clock == 0) {
                    clock_hand_ = (idx + 1) % n;
                    return idx;
                }
                f->clock = 0; // give second chance
            }
        }
        // As a fallback, choose the first unpinned frame
        for (int i = 0; i < n; ++i) {
            int idx = (start + i) % n;
            if (frames_[idx]->refcount.load(std::memory_order_acquire) == 0) {
                clock_hand_ = (idx + 1) % n;
                return idx;
            }
        }
        return -1; // no victim
    }

    std::size_t
    BufferPool::flush_dirty_batch(std::size_t max_pages)
    {
        std::lock_guard<std::mutex> lg(mu_);
        if (!flush_cb_ || max_pages == 0)
            return 0;
        std::size_t flushed = 0;
        for (int i = 0; i < static_cast<int>(capacity_) && flushed < max_pages; ++i) {
            auto& f = frames_[i];
            if (f->valid.load(std::memory_order_acquire) &&
                f->dirty.load(std::memory_order_acquire) &&
                f->refcount.load(std::memory_order_acquire) == 0) {
                flush_cb_(*f);
                f->dirty.store(false, std::memory_order_release);
                flushes_.fetch_add(1, std::memory_order_relaxed);
                ++flushed;
            }
        }
        return flushed;
    }

} // namespace scratchbird::engine

