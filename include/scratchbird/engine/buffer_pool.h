#ifndef SCRATCHBIRD_ENGINE_BUFFER_POOL_H
#define SCRATCHBIRD_ENGINE_BUFFER_POOL_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace scratchbird::engine
{

    struct BufferTag {
        std::uint64_t file_id{0};
        std::uint64_t page_no{0};

        bool operator==(const BufferTag& other) const
        {
            return file_id == other.file_id && page_no == other.page_no;
        }
    };

    struct BufferTagHash {
        std::size_t operator()(const BufferTag& t) const noexcept
        {
            return static_cast<std::size_t>((t.file_id * 1315423911ull) ^ (t.page_no * 2654435761ull));
        }
    };

    struct BufferFrame {
        BufferTag tag{};
        std::vector<std::uint8_t> data;
        std::atomic<bool> dirty{false};
        std::atomic<int> refcount{0};
        std::atomic<bool> valid{false};
        std::uint8_t clock{1};
    };

    class BufferPool;

    /**
     * BufferHandle retains a reference on a frame while in scope.
     * It is move-only and releases the reference on destruction.
     */
    class BufferHandle
    {
      public:
        BufferHandle() = default;
        BufferHandle(BufferPool* pool, int index) : pool_(pool), index_(index) {}
        BufferHandle(BufferHandle&& other) noexcept { swap(other); }
        BufferHandle& operator=(BufferHandle&& other) noexcept
        {
            if (this != &other) {
                release();
                swap(other);
            }
            return *this;
        }
        BufferHandle(const BufferHandle&) = delete;
        BufferHandle& operator=(const BufferHandle&) = delete;
        ~BufferHandle() { release(); }

        bool valid() const { return pool_ != nullptr && index_ >= 0; }
        int index() const { return index_; }
        BufferFrame* frame();
        const BufferFrame* frame() const;

        void mark_dirty();

      private:
        void release();
        void swap(BufferHandle& other) noexcept
        {
            std::swap(pool_, other.pool_);
            std::swap(index_, other.index_);
        }

        BufferPool* pool_{nullptr};
        int index_{-1};
    };

    /**
     * BufferPool: clock-sweep buffer replacement with simple hashing index.
     * Thread-safe coarse locking around map and victim selection; per-frame refcounts.
     */
    class BufferPool
    {
      public:
        using FlushCallback = std::function<void(const BufferFrame&)>;

        explicit BufferPool(std::size_t capacity_pages, std::size_t page_size)
            : capacity_(capacity_pages), page_size_(page_size)
        {
            frames_.reserve(static_cast<int>(capacity_));
            for (std::size_t i = 0; i < capacity_; ++i) {
                auto f = std::make_unique<BufferFrame>();
                f->data.resize(page_size_);
                frames_.push_back(std::move(f));
            }
        }

        void set_flush_callback(FlushCallback cb)
        {
            std::lock_guard<std::mutex> lg(mu_);
            flush_cb_ = std::move(cb);
        }

        std::size_t capacity() const { return capacity_; }
        std::size_t page_size() const { return page_size_; }

        // Pin or create the frame for tag and return a handle
        BufferHandle get(const BufferTag& tag);

        // Try to flush up to max_pages dirty frames; returns flushed count
        std::size_t flush_dirty_batch(std::size_t max_pages);

        // Statistics
        struct Stats {
            std::uint64_t hits{0};
            std::uint64_t misses{0};
            std::uint64_t evictions{0};
            std::uint64_t flushes{0};
        };
        Stats get_stats() const
        {
            Stats s;
            s.hits = hits_.load();
            s.misses = misses_.load();
            s.evictions = evictions_.load();
            s.flushes = flushes_.load();
            return s;
        }

      private:
        friend class BufferHandle;

        int find_or_create_index_locked(const BufferTag& tag);
        int choose_victim_locked();
        void inc_ref(int index)
        {
            frames_[index]->refcount.fetch_add(1, std::memory_order_acq_rel);
            frames_[index]->clock = 1;
        }
        void dec_ref(int index)
        {
            frames_[index]->refcount.fetch_sub(1, std::memory_order_acq_rel);
        }
        void mark_dirty(int index) { frames_[index]->dirty.store(true, std::memory_order_release); }

        std::size_t capacity_;
        std::size_t page_size_;
        std::vector<std::unique_ptr<BufferFrame>> frames_;
        std::unordered_map<BufferTag, int, BufferTagHash> tag_to_index_;
        mutable std::mutex mu_;
        FlushCallback flush_cb_{};
        std::atomic<std::uint64_t> hits_{0}, misses_{0}, evictions_{0}, flushes_{0};
        int clock_hand_{0};
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_BUFFER_POOL_H

