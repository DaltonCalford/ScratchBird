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

#include <memory>
#include <string>
#include <vector>

#include "scratchbird/sblr/jit/jit_artifact_store.h"
#include "scratchbird/sblr/jit/jit_reason_codes.h"

namespace scratchbird::sblr::jit
{
    struct LoweredRoutine
    {
        std::vector<uint8_t> canonical_sblr;
        std::vector<uint8_t> lowered_ir;
        bool preserves_side_effect_order = false;
    };

    struct JitCompileRequest
    {
        ArtifactCompatibilityKey key;
        std::vector<uint8_t> canonical_sblr;
    };

    struct JitCompileResult
    {
        bool success = false;
        JitReasonCode reason = JitReasonCode::NONE;
        std::string diagnostic;
        std::vector<uint8_t> native_blob;
        std::string native_blob_hash_sha256;
    };

    class JitBackend
    {
    public:
        virtual ~JitBackend() = default;
        virtual auto backendName() const -> const char* = 0;
        virtual auto compile(const LoweredRoutine& lowered,
                             const JitCompileRequest& request) -> JitCompileResult = 0;
    };

    class JitCompiler
    {
    public:
        explicit JitCompiler(std::unique_ptr<JitBackend> backend);

        auto compile(const JitCompileRequest& request) -> JitCompileResult;

    private:
        std::unique_ptr<JitBackend> backend_;
    };

    auto lowerCanonicalRoutineToIr(const JitCompileRequest& request,
                                   LoweredRoutine& lowered_out) -> JitReasonCode;

    auto checkOpcodeLegality(const std::vector<uint8_t>& canonical_sblr,
                             std::string& detail_out) -> JitReasonCode;

    auto createLlvmBackend() -> std::unique_ptr<JitBackend>;
    auto createNullBackend() -> std::unique_ptr<JitBackend>;
}

