/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/sblr/jit/jit_compiler.h"

namespace scratchbird::sblr::jit
{
    JitCompiler::JitCompiler(std::unique_ptr<JitBackend> backend)
        : backend_(std::move(backend))
    {
    }

    auto JitCompiler::compile(const JitCompileRequest& request) -> JitCompileResult
    {
        if (!backend_)
        {
            JitCompileResult result;
            result.success = false;
            result.reason = JitReasonCode::BACKEND_UNAVAILABLE;
            result.diagnostic = "no JIT backend configured";
            return result;
        }

        LoweredRoutine lowered;
        const JitReasonCode lowering_status = lowerCanonicalRoutineToIr(request, lowered);
        if (lowering_status != JitReasonCode::NONE)
        {
            JitCompileResult result;
            result.success = false;
            result.reason = lowering_status;
            result.diagnostic = "lowering rejected routine";
            return result;
        }

        if (!lowered.preserves_side_effect_order)
        {
            JitCompileResult result;
            result.success = false;
            result.reason = JitReasonCode::BACKEND_COMPILE_FAILED;
            result.diagnostic = "lowering must preserve side-effect ordering";
            return result;
        }

        return backend_->compile(lowered, request);
    }
}

