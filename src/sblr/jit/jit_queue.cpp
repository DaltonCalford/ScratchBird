/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/sblr/jit/jit_queue.h"

namespace scratchbird::sblr::jit
{
    JitQueue::JitQueue(size_t capacity) : capacity_(capacity)
    {
    }

    auto JitQueue::capacity() const -> size_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return capacity_;
    }

    auto JitQueue::size() const -> size_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    auto JitQueue::setCapacity(size_t capacity) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        capacity_ = capacity;
        while (queue_.size() > capacity_)
        {
            queue_.pop_back();
        }
    }

    auto JitQueue::tryEnqueue(const JitQueueEntry& entry,
                              JitReasonCode& reason_out) -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= capacity_)
        {
            reason_out = JitReasonCode::QUEUE_SATURATED;
            return false;
        }
        queue_.push_back(entry);
        reason_out = JitReasonCode::NONE;
        return true;
    }

    auto JitQueue::tryDequeue(JitQueueEntry& out) -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty())
        {
            return false;
        }
        out = queue_.front();
        queue_.pop_front();
        return true;
    }
}

