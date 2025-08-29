#ifndef SCRATCHBIRD_ENGINE_BACKGROUND_WRITER_H
#define SCRATCHBIRD_ENGINE_BACKGROUND_WRITER_H

#include "scratchbird/engine/buffer_pool.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace scratchbird::engine
{

    class BackgroundWriter
    {
      public:
        BackgroundWriter(BufferPool& pool, std::chrono::milliseconds interval,
                         std::size_t batch_pages)
            : pool_(pool), interval_(interval), batch_pages_(batch_pages)
        {
        }

        void start()
        {
            if (running_.exchange(true))
                return;
            worker_ = std::thread([this]() { run(); });
        }

        void stop()
        {
            if (!running_.exchange(false))
                return;
            if (worker_.joinable())
                worker_.join();
        }

        ~BackgroundWriter() { stop(); }

      private:
        void run()
        {
            while (running_.load()) {
                pool_.flush_dirty_batch(batch_pages_);
                std::this_thread::sleep_for(interval_);
            }
        }

        BufferPool& pool_;
        std::chrono::milliseconds interval_;
        std::size_t batch_pages_;
        std::atomic<bool> running_{false};
        std::thread worker_;
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_BACKGROUND_WRITER_H

