#ifndef SCRATCHBIRD_ENGINE_PAGER_H
#define SCRATCHBIRD_ENGINE_PAGER_H

#include "scratchbird/engine/file.h"
#include "scratchbird/engine/ods.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    struct PageKey {
        std::uint16_t space_id{1};
        std::uint32_t page_no{0};
        bool operator==(const PageKey& o) const noexcept
        {
            return space_id == o.space_id && page_no == o.page_no;
        }
    };

    struct PageKeyHash {
        std::size_t operator()(const PageKey& k) const noexcept
        {
            return (static_cast<std::size_t>(k.space_id) << 48) ^
                   static_cast<std::size_t>(k.page_no);
        }
    };

    enum class LatchMode { Shared, Exclusive, IntentWrite };

    struct PageFrame {
        PageKey key{};
        std::vector<std::uint8_t> data; // size = page_size
        bool dirty{false};
        int refcount{0};
        bool referenced{false}; // for clock
    };

    class BufferCache
    {
      public:
        explicit BufferCache(std::uint32_t page_size, std::size_t capacity_pages)
            : page_size_(page_size), capacity_(capacity_pages)
        {
        }

        PageFrame* fetch(const PageKey& key, bool* was_miss);
        void release(PageFrame* frame);
        void mark_dirty(PageFrame* frame);
        void flush_dirty(FileMap& fmap);

        std::uint32_t page_size() const
        {
            return page_size_;
        }

      private:
        PageFrame* evict_victim();

        std::uint32_t page_size_{4096};
        std::size_t capacity_{128};
        std::unordered_map<PageKey, std::unique_ptr<PageFrame>, PageKeyHash> map_{};
        std::vector<PageFrame*> clock_{}; // circular list
        std::size_t hand_{0};
        std::mutex mu_;
    };

    class Pager
    {
      public:
        explicit Pager(FileMap* fmap, std::shared_ptr<BufferCache> cache);
        ~Pager();

        // Prefetch a page range starting at key.
        void prefetch(const PageKey& key, std::uint32_t horizon_pages);

        // Get and pin a page in given mode; load from disk if absent.
        PageFrame* get_page(const PageKey& key, LatchMode mode);

        // Unpin the page.
        void release(PageFrame* frame);

        // Mark page for write-back.
        void mark_dirty(PageFrame* frame)
        {
            cache_->mark_dirty(frame);
        }

        // Flush dirty pages (group write).
        void flush()
        {
            cache_->flush_dirty(*fmap_);
        }

      private:
        FileMap* fmap_{};
        std::shared_ptr<BufferCache> cache_;
        bool stop_bg_{false};
        std::thread* bg_thread_{nullptr};
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_PAGER_H
