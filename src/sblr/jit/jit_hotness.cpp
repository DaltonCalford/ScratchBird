/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/sblr/jit/jit_runtime.h"

#include <sstream>

namespace scratchbird::sblr::jit
{
    auto JitRuntime::setHotnessThreshold(uint32_t threshold) -> void
    {
        hotness_threshold_ = threshold == 0 ? 1 : threshold;
    }

    auto JitRuntime::resetHotnessCounters() -> void
    {
        hotness_.clear();
    }

    auto JitRuntime::hotnessKeyFor(const JitRuntimeRequest& request) const -> std::string
    {
        std::ostringstream key;
        key << request.compatibility.object_uuid.toString() << ':'
            << request.compatibility.target_triple << ':'
            << request.compatibility.native_abi_version;
        return key.str();
    }
}

