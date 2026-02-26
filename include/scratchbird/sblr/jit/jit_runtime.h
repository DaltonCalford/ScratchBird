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
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/status.h"
#include "scratchbird/sblr/jit/jit_artifact_store.h"
#include "scratchbird/sblr/jit/jit_compiler.h"
#include "scratchbird/sblr/jit/jit_queue.h"
#include "scratchbird/sblr/jit/jit_reason_codes.h"

namespace scratchbird::sblr::jit
{
    enum class JitCompileMode : uint8_t
    {
        EXPLICIT_ONLY = 0,
        JIT_ALLOWED = 1
    };

    enum class JitExecutionPolicy : uint8_t
    {
        INTERPRETED_ONLY = 0,
        PREFER_NATIVE = 1,
        REQUIRE_NATIVE = 2
    };

    enum class RoutineSurfaceKind : uint8_t
    {
        UNKNOWN = 0,
        FUNCTION = 1,
        TRIGGER = 2,
        PROCEDURE = 3,
        PACKAGE_MEMBER = 4
    };

    struct JitHints
    {
        bool disable_compile = false;
        bool disable_execute = false;
        bool prefer_vm = false;
    };

    struct JitPolicyEnvelope
    {
        JitCompileMode database_compile_mode = JitCompileMode::EXPLICIT_ONLY;
        JitExecutionPolicy database_execution_policy = JitExecutionPolicy::INTERPRETED_ONLY;
        JitCompileMode session_compile_mode = JitCompileMode::EXPLICIT_ONLY;
        JitExecutionPolicy session_execution_policy = JitExecutionPolicy::INTERPRETED_ONLY;
        JitCompileMode object_compile_mode = JitCompileMode::EXPLICIT_ONLY;
        JitExecutionPolicy object_execution_policy = JitExecutionPolicy::INTERPRETED_ONLY;
        JitHints hints{};
    };

    struct JitEffectivePolicy
    {
        JitCompileMode compile_mode = JitCompileMode::EXPLICIT_ONLY;
        JitExecutionPolicy execution_policy = JitExecutionPolicy::INTERPRETED_ONLY;
        JitHints hints{};
    };

    struct JitRuntimeRequest
    {
        RoutineSurfaceKind surface = RoutineSurfaceKind::UNKNOWN;
        core::ID object_uuid{};
        core::ID module_id{};
        core::ID plan_id{};
        std::vector<uint8_t> canonical_sblr;
        ArtifactCompatibilityKey compatibility;
        JitPolicyEnvelope policy;
    };

    struct JitDispatchOutcome
    {
        enum class Path : uint8_t
        {
            VM = 0,
            NATIVE = 1,
            ERROR = 2
        };

        Path path = Path::VM;
        JitReasonCode reason = JitReasonCode::NONE;
        std::string detail;
        bool compile_queued = false;
        bool promoted_by_hotness = false;
        JitArtifact artifact{};
    };

    class JitRuntime
    {
    public:
        explicit JitRuntime(core::CatalogManager* catalog);

        auto setCompileBackend(std::unique_ptr<JitBackend> backend) -> void;
        auto setHotnessThreshold(uint32_t threshold) -> void;
        auto setRequireArtifactSignature(bool enabled) -> void;
        auto setQueueCapacity(size_t capacity) -> void;
        auto resetHotnessCounters() -> void;

        auto resolvePolicy(const JitPolicyEnvelope& policy) const -> JitEffectivePolicy;

        auto selectPath(const JitRuntimeRequest& request,
                        core::ErrorContext* ctx) -> JitDispatchOutcome;

        auto compileExplicit(const JitRuntimeRequest& request,
                             core::ErrorContext* ctx) -> core::Status;

        auto rebuildArtifacts(const core::ID& object_uuid,
                              const std::function<JitCompileRequest()>& request_builder,
                              core::ErrorContext* ctx) -> core::Status;

        auto inspectArtifacts(const core::ID& object_uuid,
                              std::vector<JitArtifact>& out,
                              core::ErrorContext* ctx) const -> core::Status;

        auto retireArtifact(const core::ID& artifact_id,
                            core::ErrorContext* ctx) -> core::Status;

        auto onDependencySignatureChange(const core::ID& object_uuid,
                                         core::ErrorContext* ctx) -> core::Status;

        auto onSecurityPolicyVersionChange(const core::ID& object_uuid,
                                           core::ErrorContext* ctx) -> core::Status;

        auto drainCompileQueue(core::ErrorContext* ctx) -> size_t;

    private:
        auto hotnessKeyFor(const JitRuntimeRequest& request) const -> std::string;
        auto materializeArtifact(const JitRuntimeRequest& request,
                                 const JitCompileResult& compile_result,
                                 core::ErrorContext* ctx) -> core::Status;

        core::CatalogManager* catalog_ = nullptr;
        JitArtifactStore artifact_store_;
        JitQueue queue_;
        JitCompiler compiler_;
        uint32_t hotness_threshold_ = 3;
        bool require_artifact_signature_ = false;
        std::unordered_map<std::string, uint32_t> hotness_;
    };
}
