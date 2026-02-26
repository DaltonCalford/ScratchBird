/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/sblr/jit/jit_reason_codes.h"

namespace scratchbird::sblr::jit
{
    auto toString(JitReasonCode code) -> const char*
    {
        switch (code)
        {
            case JitReasonCode::NONE:
                return "NONE";
            case JitReasonCode::POLICY_INTERPRETED_ONLY:
                return "POLICY_INTERPRETED_ONLY";
            case JitReasonCode::NATIVE_SCOPE_NOT_ELIGIBLE:
                return "NATIVE_SCOPE_NOT_ELIGIBLE";
            case JitReasonCode::HINT_DISABLE_EXECUTE:
                return "HINT_DISABLE_EXECUTE";
            case JitReasonCode::HINT_PREFER_VM:
                return "HINT_PREFER_VM";
            case JitReasonCode::HINT_DISABLE_COMPILE:
                return "HINT_DISABLE_COMPILE";
            case JitReasonCode::COMPILE_MODE_EXPLICIT_ONLY:
                return "COMPILE_MODE_EXPLICIT_ONLY";
            case JitReasonCode::HOTNESS_BELOW_THRESHOLD:
                return "HOTNESS_BELOW_THRESHOLD";
            case JitReasonCode::QUEUE_SATURATED:
                return "QUEUE_SATURATED";
            case JitReasonCode::ARTIFACT_NOT_FOUND:
                return "ARTIFACT_NOT_FOUND";
            case JitReasonCode::ARTIFACT_KEY_MISMATCH_TARGET_TRIPLE:
                return "ARTIFACT_KEY_MISMATCH_TARGET_TRIPLE";
            case JitReasonCode::ARTIFACT_KEY_MISMATCH_CPU_PROFILE:
                return "ARTIFACT_KEY_MISMATCH_CPU_PROFILE";
            case JitReasonCode::ARTIFACT_KEY_MISMATCH_NATIVE_ABI:
                return "ARTIFACT_KEY_MISMATCH_NATIVE_ABI";
            case JitReasonCode::ARTIFACT_KEY_MISMATCH_COMPILER_IDENTITY:
                return "ARTIFACT_KEY_MISMATCH_COMPILER_IDENTITY";
            case JitReasonCode::ARTIFACT_KEY_MISMATCH_COMPILER_VERSION:
                return "ARTIFACT_KEY_MISMATCH_COMPILER_VERSION";
            case JitReasonCode::ARTIFACT_KEY_MISMATCH_OPTIMIZATION_PROFILE:
                return "ARTIFACT_KEY_MISMATCH_OPTIMIZATION_PROFILE";
            case JitReasonCode::ARTIFACT_KEY_MISMATCH_SECURITY_POLICY:
                return "ARTIFACT_KEY_MISMATCH_SECURITY_POLICY";
            case JitReasonCode::ARTIFACT_KEY_MISMATCH_SBLR_HASH:
                return "ARTIFACT_KEY_MISMATCH_SBLR_HASH";
            case JitReasonCode::ARTIFACT_HASH_INVALID:
                return "ARTIFACT_HASH_INVALID";
            case JitReasonCode::ARTIFACT_SIGNATURE_INVALID:
                return "ARTIFACT_SIGNATURE_INVALID";
            case JitReasonCode::ARTIFACT_RETIRED:
                return "ARTIFACT_RETIRED";
            case JitReasonCode::ARTIFACT_STATE_NOT_READY:
                return "ARTIFACT_STATE_NOT_READY";
            case JitReasonCode::DEOPT_POLICY_VERSION_MISMATCH:
                return "DEOPT_POLICY_VERSION_MISMATCH";
            case JitReasonCode::DEOPT_DEPENDENCY_SIGNATURE_MISMATCH:
                return "DEOPT_DEPENDENCY_SIGNATURE_MISMATCH";
            case JitReasonCode::BACKEND_UNAVAILABLE:
                return "BACKEND_UNAVAILABLE";
            case JitReasonCode::BACKEND_COMPILE_FAILED:
                return "BACKEND_COMPILE_FAILED";
            case JitReasonCode::UNSUPPORTED_OPCODE_FAMILY:
                return "UNSUPPORTED_OPCODE_FAMILY";
            case JitReasonCode::REQUIRE_NATIVE_NOT_AVAILABLE:
                return "REQUIRE_NATIVE_NOT_AVAILABLE";
            case JitReasonCode::NATIVE_EXECUTION_FAILED:
                return "NATIVE_EXECUTION_FAILED";
        }
        return "UNKNOWN";
    }
}
