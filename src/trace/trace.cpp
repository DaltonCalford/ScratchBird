// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/trace/trace.h"

#include <atomic>
#include <mutex>
#include <random>
#include <thread>

namespace scratchbird::trace
{

    TraceBuffer::TraceBuffer(std::size_t capacity) : ring_(capacity) {}

    void TraceBuffer::clear()
    {
        std::lock_guard<std::mutex> l(mu_);
        head_ = 0;
        full_ = false;
    }

    void TraceBuffer::push(TraceEvent ev)
    {
        std::lock_guard<std::mutex> l(mu_);
        if (ring_.empty())
            return;
        ring_[head_] = std::move(ev);
        head_ = (head_ + 1) % ring_.size();
        if (head_ == 0)
            full_ = true;
    }

    std::vector<TraceEvent> TraceBuffer::snapshot() const
    {
        std::lock_guard<std::mutex> l(mu_);
        std::vector<TraceEvent> out;
        if (ring_.empty())
            return out;
        if (!full_) {
            out.assign(ring_.begin(), ring_.begin() + head_);
            return out;
        }
        out.reserve(ring_.size());
        out.insert(out.end(), ring_.begin() + head_, ring_.end());
        out.insert(out.end(), ring_.begin(), ring_.begin() + head_);
        return out;
    }

    TraceController& TraceController::instance()
    {
        static TraceController inst;
        return inst;
    }

    TraceController::TraceController() : span_id_counter_(1) {}

    void TraceController::start_profile(const TraceProfileConfig& cfg)
    {
        std::lock_guard<std::mutex> l(mu_);
        active_ = cfg;
        buffer_ = std::make_unique<TraceBuffer>(cfg.buffer_capacity);
        buffer_->clear();
    }

    void TraceController::stop_profile()
    {
        std::lock_guard<std::mutex> l(mu_);
        active_.reset();
        buffer_.reset();
    }

    bool TraceController::is_running() const
    {
        std::lock_guard<std::mutex> l(mu_);
        return active_.has_value();
    }

    TraceProfileConfig TraceController::current_config() const
    {
        std::lock_guard<std::mutex> l(mu_);
        return active_.value_or(TraceProfileConfig{});
    }

    static thread_local std::minstd_rand rng{std::random_device{}()};

    bool TraceController::should_sample() const
    {
        std::lock_guard<std::mutex> l(mu_);
        if (!active_)
            return false;
        double r = std::generate_canonical<double, 10>(rng);
        return r < active_->sample_ratio;
    }

    uint64_t TraceController::current_thread_id()
    {
        auto id = std::hash<std::thread::id>{}(std::this_thread::get_id());
        return static_cast<uint64_t>(id);
    }

    void TraceController::emit(std::string category, std::string name, uint64_t span_id,
                               uint64_t parent_span_id)
    {
        std::unique_lock<std::mutex> l(mu_);
        if (!active_ || !buffer_)
            return;
        l.unlock();
        TraceEvent ev;
        ev.ts = std::chrono::steady_clock::now();
        ev.category = std::move(category);
        ev.name = std::move(name);
        ev.tid = current_thread_id();
        ev.span_id = span_id;
        ev.parent_span_id = parent_span_id;
        buffer_->push(std::move(ev));
    }

    std::vector<TraceEvent> TraceController::collect()
    {
        std::lock_guard<std::mutex> l(mu_);
        if (!buffer_)
            return {};
        return buffer_->snapshot();
    }

    uint64_t TraceController::allocate_span_id()
    {
        return span_id_counter_.fetch_add(1, std::memory_order_relaxed);
    }

    TraceSpan::TraceSpan(std::string category, std::string name, uint64_t parent_span_id)
        : parent_id_(parent_span_id), category_(std::move(category)), name_(std::move(name))
    {
        auto& ctl = TraceController::instance();
        sampled_ = ctl.should_sample();
        if (!sampled_)
            return;
        span_id_ = ctl.allocate_span_id();
        ctl.emit(category_, name_ + ".begin", span_id_, parent_id_);
    }

    TraceSpan::~TraceSpan()
    {
        if (!sampled_)
            return;
        TraceController::instance().emit(category_, name_ + ".end", span_id_, parent_id_);
    }

} // namespace scratchbird::trace
