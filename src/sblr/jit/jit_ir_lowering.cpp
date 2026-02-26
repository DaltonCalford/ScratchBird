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
    auto lowerCanonicalRoutineToIr(const JitCompileRequest& request,
                                   LoweredRoutine& lowered_out) -> JitReasonCode
    {
        lowered_out = LoweredRoutine{};
        lowered_out.canonical_sblr = request.canonical_sblr;

        std::string legality_detail;
        const JitReasonCode legality =
            checkOpcodeLegality(request.canonical_sblr, legality_detail);
        if (legality != JitReasonCode::NONE)
        {
            return legality;
        }

        // Current deterministic lowering keeps an isomorphic instruction stream,
        // preserving order-sensitive side effects for VM/native equivalence.
        lowered_out.lowered_ir = request.canonical_sblr;
        lowered_out.preserves_side_effect_order = true;
        return JitReasonCode::NONE;
    }
}

