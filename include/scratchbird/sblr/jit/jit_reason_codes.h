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

#include <cstdint>

namespace scratchbird::sblr::jit
{
    enum class JitReasonCode : uint16_t
    {
        NONE = 0,
        POLICY_INTERPRETED_ONLY,
        NATIVE_SCOPE_NOT_ELIGIBLE,
        HINT_DISABLE_EXECUTE,
        HINT_PREFER_VM,
        HINT_DISABLE_COMPILE,
        COMPILE_MODE_EXPLICIT_ONLY,
        HOTNESS_BELOW_THRESHOLD,
        QUEUE_SATURATED,
        ARTIFACT_NOT_FOUND,
        ARTIFACT_KEY_MISMATCH_TARGET_TRIPLE,
        ARTIFACT_KEY_MISMATCH_CPU_PROFILE,
        ARTIFACT_KEY_MISMATCH_NATIVE_ABI,
        ARTIFACT_KEY_MISMATCH_COMPILER_IDENTITY,
        ARTIFACT_KEY_MISMATCH_COMPILER_VERSION,
        ARTIFACT_KEY_MISMATCH_OPTIMIZATION_PROFILE,
        ARTIFACT_KEY_MISMATCH_SECURITY_POLICY,
        ARTIFACT_KEY_MISMATCH_SBLR_HASH,
        ARTIFACT_HASH_INVALID,
        ARTIFACT_SIGNATURE_INVALID,
        ARTIFACT_RETIRED,
        ARTIFACT_STATE_NOT_READY,
        DEOPT_POLICY_VERSION_MISMATCH,
        DEOPT_DEPENDENCY_SIGNATURE_MISMATCH,
        BACKEND_UNAVAILABLE,
        BACKEND_COMPILE_FAILED,
        UNSUPPORTED_OPCODE_FAMILY,
        REQUIRE_NATIVE_NOT_AVAILABLE,
        NATIVE_EXECUTION_FAILED
    };

    auto toString(JitReasonCode code) -> const char*;
}
