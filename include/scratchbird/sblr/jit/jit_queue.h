/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/sblr/jit/jit_compiler.h"
#include "scratchbird/sblr/jit/jit_reason_codes.h"

namespace scratchbird::sblr::jit
{
    struct JitQueueEntry
    {
        core::ID queue_id{};
        core::ID object_uuid{};
        core::ID module_id{};
        core::ID plan_id{};
        JitCompileRequest compile_request;
        uint8_t priority = 0;
    };

    class JitQueue
    {
    public:
        explicit JitQueue(size_t capacity = 128);

        auto capacity() const -> size_t;
        auto size() const -> size_t;
        auto setCapacity(size_t capacity) -> void;

        auto tryEnqueue(const JitQueueEntry& entry, JitReasonCode& reason_out) -> bool;
        auto tryDequeue(JitQueueEntry& out) -> bool;

    private:
        mutable std::mutex mutex_;
        size_t capacity_ = 128;
        std::deque<JitQueueEntry> queue_;
    };
}
