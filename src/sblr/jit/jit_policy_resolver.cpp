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

namespace scratchbird::sblr::jit
{
    auto JitRuntime::resolvePolicy(const JitPolicyEnvelope& policy) const
        -> JitEffectivePolicy
    {
        JitEffectivePolicy out{};
        out.compile_mode = policy.database_compile_mode;
        out.execution_policy = policy.database_execution_policy;

        out.compile_mode = policy.session_compile_mode;
        out.execution_policy = policy.session_execution_policy;

        out.compile_mode = policy.object_compile_mode;
        out.execution_policy = policy.object_execution_policy;
        out.hints = policy.hints;
        return out;
    }
}

