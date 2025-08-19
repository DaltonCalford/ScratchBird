#ifndef SCRATCHBIRD_ENGINE_LATCH_H
#define SCRATCHBIRD_ENGINE_LATCH_H

#include "scratchbird/engine/monitoring.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace scratchbird::engine
{

    class LatchManager
    {
      public:
        void lock_shared(std::uint32_t page_no)
        {
            auto m = get_or_create(page_no);
            m->lock_shared();
            Monitoring::inc_latch_shared();
        }
        bool try_lock_shared(std::uint32_t page_no)
        {
            auto m = get_or_create(page_no);
            return m->try_lock_shared();
        }
        void unlock_shared(std::uint32_t page_no)
        {
            auto m = lookup(page_no);
            if (m)
                m->unlock_shared();
        }
        void lock_exclusive(std::uint32_t page_no)
        {
            auto m = get_or_create(page_no);
            m->lock();
            Monitoring::inc_latch_exclusive();
        }
        bool try_lock_exclusive(std::uint32_t page_no)
        {
            auto m = get_or_create(page_no);
            return m->try_lock();
        }
        void unlock_exclusive(std::uint32_t page_no)
        {
            auto m = lookup(page_no);
            if (m)
                m->unlock();
        }

      private:
        std::shared_ptr<std::shared_mutex> get_or_create(std::uint32_t page_no)
        {
            std::lock_guard<std::mutex> g(map_mu_);
            auto it = latches_.find(page_no);
            if (it != latches_.end())
                return it->second;
            auto ptr = std::make_shared<std::shared_mutex>();
            latches_.emplace(page_no, ptr);
            return ptr;
        }
        std::shared_ptr<std::shared_mutex> lookup(std::uint32_t page_no)
        {
            std::lock_guard<std::mutex> g(map_mu_);
            auto it = latches_.find(page_no);
            return it == latches_.end() ? std::shared_ptr<std::shared_mutex>() : it->second;
        }

        std::mutex map_mu_;
        std::unordered_map<std::uint32_t, std::shared_ptr<std::shared_mutex>> latches_;
    };

    struct SharedPageGuard {
        LatchManager* mgr{nullptr};
        std::uint32_t page{0};
        SharedPageGuard() = default;
        SharedPageGuard(LatchManager* m, std::uint32_t p) : mgr(m), page(p)
        {
            if (mgr && page)
                mgr->lock_shared(page);
        }
        ~SharedPageGuard()
        {
            if (mgr && page)
                mgr->unlock_shared(page);
        }
        void reset(LatchManager* m, std::uint32_t p)
        {
            if (mgr && page)
                mgr->unlock_shared(page);
            mgr = m;
            page = p;
            if (mgr && page)
                mgr->lock_shared(page);
        }
        void release()
        {
            if (mgr && page)
                mgr->unlock_shared(page);
            mgr = nullptr;
            page = 0;
        }
    };

    struct ExclusivePageGuard {
        LatchManager* mgr{nullptr};
        std::uint32_t page{0};
        ExclusivePageGuard() = default;
        ExclusivePageGuard(LatchManager* m, std::uint32_t p) : mgr(m), page(p)
        {
            if (mgr && page)
                mgr->lock_exclusive(page);
        }
        ~ExclusivePageGuard()
        {
            if (mgr && page)
                mgr->unlock_exclusive(page);
        }
        void reset(LatchManager* m, std::uint32_t p)
        {
            if (mgr && page)
                mgr->unlock_exclusive(page);
            mgr = m;
            page = p;
            if (mgr && page)
                mgr->lock_exclusive(page);
        }
        void release()
        {
            if (mgr && page)
                mgr->unlock_exclusive(page);
            mgr = nullptr;
            page = 0;
        }
    };

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_LATCH_H
