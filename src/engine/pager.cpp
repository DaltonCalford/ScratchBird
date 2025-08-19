#include "scratchbird/engine/pager.h"

#include "scratchbird/engine/config.h"
#include "scratchbird/engine/monitoring.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace scratchbird::engine
{

    PageFrame* BufferCache::evict_victim()
    {
        // Grow clock vector on demand
        if (clock_.size() < capacity_) {
            clock_.push_back(nullptr);
        }
        // Find victim by second-chance clock
        for (;;) {
            hand_ = hand_ % clock_.size();
            PageFrame* f = clock_[hand_];
            if (!f) {
                // empty slot
                return nullptr;
            }
            if (f->refcount == 0) {
                if (f->referenced) {
                    f->referenced = false;
                } else {
                    // evictable
                    return f;
                }
            }
            ++hand_;
        }
    }

    PageFrame* BufferCache::fetch(const PageKey& key, bool* was_miss)
    {
        std::lock_guard<std::mutex> lg(mu_);
        if (was_miss)
            *was_miss = false;
        auto it = map_.find(key);
        if (it != map_.end()) {
            PageFrame* f = it->second.get();
            f->refcount++;
            f->referenced = true;
            Monitoring::inc_fetch_hit();
            return f;
        }
        if (was_miss)
            *was_miss = true;
        Monitoring::inc_fetch_miss();
        // Allocate new frame or evict
        PageFrame* victim = evict_victim();
        if (victim && victim->dirty) {
            // Caller must have flushed before; for now throw to surface misuse in tests
            throw std::runtime_error("BufferCache: evicting dirty frame without flush");
        }
        if (victim) {
            // Remove victim from map
            map_.erase(victim->key);
            victim->key = key;
            victim->dirty = false;
            victim->refcount = 1;
            victim->referenced = true;
            std::fill(victim->data.begin(), victim->data.end(), 0);
            map_[key] = std::unique_ptr<PageFrame>(victim);
            return victim;
        }
        // New frame
        auto pf = std::make_unique<PageFrame>();
        pf->key = key;
        pf->data.assign(page_size_, 0);
        pf->dirty = false;
        pf->refcount = 1;
        pf->referenced = true;
        PageFrame* ret = pf.get();
        map_[key] = std::move(pf);
        // place into clock
        for (auto& slot : clock_) {
            if (slot == nullptr) {
                slot = ret;
                return ret;
            }
        }
        clock_.push_back(ret);
        return ret;
    }

    void BufferCache::release(PageFrame* frame)
    {
        if (frame && frame->refcount > 0)
            frame->refcount--;
    }

    void BufferCache::mark_dirty(PageFrame* frame)
    {
        if (frame)
            frame->dirty = true;
    }

    void BufferCache::flush_dirty(FileMap& fmap)
    {
        for (auto& [key, up] : map_) {
            PageFrame* f = up.get();
            if (f->dirty && f->refcount == 0) {
                fmap.write_page(key.page_no, f->data.data());
                f->dirty = false;
                Monitoring::inc_flush();
            }
        }
    }

    Pager::Pager(FileMap* fmap, std::shared_ptr<BufferCache> cache)
        : fmap_(fmap), cache_(std::move(cache))
    {
        // Background writeback thread
        bg_thread_ = new std::thread([this]() {
            using namespace std::chrono_literals;
            while (!stop_bg_) {
                std::this_thread::sleep_for(50ms);
                if (fmap_)
                    cache_->flush_dirty(*fmap_);
            }
        });
    }

    Pager::~Pager()
    {
        stop_bg_ = true;
        if (bg_thread_) {
            bg_thread_->join();
            delete bg_thread_;
            bg_thread_ = nullptr;
        }
    }

    PageFrame* Pager::get_page(const PageKey& key, LatchMode mode)
    {
        (void)mode; // single-thread smoke: ignore latching semantics for now
        bool miss = false;
        PageFrame* f = cache_->fetch(key, &miss);
        if (f->data.empty())
            f->data.assign(cache_->page_size(), 0);
        // Load from disk for cache miss only
        if (miss) {
            fmap_->read_page(key.page_no, f->data.data());
        }
        // Optional checksum verify-on-read
        const auto& cfg = get_engine_config();
        if (cfg.checksum_policy == ChecksumPolicy::VerifyOnRead) {
            if (f->data.size() >= sizeof(ods::PageHeader)) {
                auto* ph = reinterpret_cast<ods::PageHeader*>(f->data.data());
                if (ph->checksum != 0 && ph->page_size != 0 && ph->type != 0) {
                    std::uint32_t saved = ph->checksum;
                    ph->checksum = 0;
                    std::uint32_t calc = ods::crc32c(f->data.data(), f->data.size());
                    ph->checksum = saved;
                    if (calc != saved) {
                        throw std::runtime_error("Pager: checksum mismatch on read");
                    }
                }
            }
        }
        return f;
    }

    void Pager::release(PageFrame* frame)
    {
        cache_->release(frame);
    }

    void Pager::prefetch(const PageKey& key, std::uint32_t horizon_pages)
    {
        if (!fmap_ || fmap_->segments().empty())
            return;
        auto [segIdx, off] = fmap_->map(key.page_no);
        std::size_t len = static_cast<std::size_t>(horizon_pages) * cache_->page_size();
        FileManager::prefetch_willneed(fmap_->segments()[segIdx].handle, off, len);
    }

} // namespace scratchbird::engine
