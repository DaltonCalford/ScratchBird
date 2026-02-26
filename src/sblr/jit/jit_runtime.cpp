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

#include <cstdlib>

#include "scratchbird/core/uuidv7.h"

namespace scratchbird::sblr::jit
{
    namespace
    {
        auto isZeroUuid(const core::ID& id) -> bool
        {
            for (uint8_t byte : id.bytes)
            {
                if (byte != 0)
                {
                    return false;
                }
            }
            return true;
        }

        auto isNativeEligibleSurface(RoutineSurfaceKind surface) -> bool
        {
            switch (surface)
            {
                case RoutineSurfaceKind::FUNCTION:
                case RoutineSurfaceKind::TRIGGER:
                case RoutineSurfaceKind::PROCEDURE:
                case RoutineSurfaceKind::PACKAGE_MEMBER:
                    return true;
                case RoutineSurfaceKind::UNKNOWN:
                default:
                    return false;
            }
        }
    }

    JitRuntime::JitRuntime(core::CatalogManager* catalog)
        : catalog_(catalog),
          artifact_store_(catalog),
          queue_(128),
          compiler_(createNullBackend())
    {
    }

    auto JitRuntime::setCompileBackend(std::unique_ptr<JitBackend> backend) -> void
    {
        compiler_ = JitCompiler(std::move(backend));
    }

    auto JitRuntime::setRequireArtifactSignature(bool enabled) -> void
    {
        require_artifact_signature_ = enabled;
    }

    auto JitRuntime::setQueueCapacity(size_t capacity) -> void
    {
        queue_.setCapacity(capacity);
    }

    auto JitRuntime::materializeArtifact(const JitRuntimeRequest& request,
                                         const JitCompileResult& compile_result,
                                         core::ErrorContext* ctx) -> core::Status
    {
        if (!compile_result.success)
        {
            return core::Status::NOT_IMPLEMENTED;
        }

        JitArtifact artifact{};
        artifact.artifact_id = core::generateUuidV7();
        artifact.module_id = request.module_id;
        artifact.plan_id = request.plan_id;
        artifact.binary_blob_id = core::generateUuidV7();
        artifact.compatibility = request.compatibility;
        artifact.has_native_hash = true;
        artifact.native_hash_sha256 = compile_result.native_blob_hash_sha256;
        artifact.has_signature_blob_id = false;
        artifact.state = core::CatalogManager::SblrArtifactState::READY;
        return artifact_store_.upsertArtifact(artifact, ctx);
    }

    auto JitRuntime::selectPath(const JitRuntimeRequest& request,
                                core::ErrorContext* ctx) -> JitDispatchOutcome
    {
        JitDispatchOutcome out;
        if (!isNativeEligibleSurface(request.surface))
        {
            out.path = JitDispatchOutcome::Path::VM;
            out.reason = JitReasonCode::NATIVE_SCOPE_NOT_ELIGIBLE;
            out.detail = "native execution is limited to routine/member surfaces";
            return out;
        }

        const JitEffectivePolicy effective = resolvePolicy(request.policy);

        if (effective.execution_policy == JitExecutionPolicy::INTERPRETED_ONLY)
        {
            out.path = JitDispatchOutcome::Path::VM;
            out.reason = JitReasonCode::POLICY_INTERPRETED_ONLY;
            out.detail = "execution policy requires interpreted path";
            return out;
        }
        if (effective.hints.disable_execute)
        {
            out.path = JitDispatchOutcome::Path::VM;
            out.reason = JitReasonCode::HINT_DISABLE_EXECUTE;
            out.detail = "native execution disabled by hint";
            return out;
        }
        if (effective.hints.prefer_vm)
        {
            out.path = JitDispatchOutcome::Path::VM;
            out.reason = JitReasonCode::HINT_PREFER_VM;
            out.detail = "native execution bypassed by prefer-vm hint";
            return out;
        }

        ArtifactVerificationResult verified =
            artifact_store_.fetchVerifiedArtifact(request.compatibility,
                                                 require_artifact_signature_,
                                                 ctx);
        if (verified.valid && verified.artifact.has_value())
        {
            out.path = JitDispatchOutcome::Path::NATIVE;
            out.reason = JitReasonCode::NONE;
            out.detail = "native artifact selected";
            out.artifact = verified.artifact.value();
            return out;
        }

        if (effective.execution_policy == JitExecutionPolicy::REQUIRE_NATIVE)
        {
            out.path = JitDispatchOutcome::Path::ERROR;
            out.reason = JitReasonCode::REQUIRE_NATIVE_NOT_AVAILABLE;
            out.detail = verified.detail.empty()
                ? "REQUIRE_NATIVE requested but no valid artifact exists"
                : verified.detail;
            return out;
        }

        out.path = JitDispatchOutcome::Path::VM;
        out.reason = verified.reason;
        out.detail = verified.detail;

        if (verified.reason != JitReasonCode::ARTIFACT_NOT_FOUND)
        {
            return out;
        }

        if (effective.hints.disable_compile)
        {
            out.reason = JitReasonCode::HINT_DISABLE_COMPILE;
            out.detail = "compile suppressed by hint";
            return out;
        }
        if (effective.compile_mode == JitCompileMode::EXPLICIT_ONLY)
        {
            out.reason = JitReasonCode::COMPILE_MODE_EXPLICIT_ONLY;
            out.detail = "compile mode is explicit-only";
            return out;
        }

        const std::string hot_key = hotnessKeyFor(request);
        uint32_t& seen = hotness_[hot_key];
        ++seen;
        if (seen < hotness_threshold_)
        {
            out.reason = JitReasonCode::HOTNESS_BELOW_THRESHOLD;
            out.detail = "routine has not reached hotness threshold";
            return out;
        }

        JitQueueEntry entry{};
        entry.queue_id = core::generateUuidV7();
        entry.object_uuid = request.object_uuid;
        entry.module_id = request.module_id;
        entry.plan_id = request.plan_id;
        entry.priority = 0;
        entry.compile_request.key = request.compatibility;
        entry.compile_request.canonical_sblr = request.canonical_sblr;

        JitReasonCode queue_reason = JitReasonCode::NONE;
        if (!queue_.tryEnqueue(entry, queue_reason))
        {
            out.reason = queue_reason;
            out.detail = "compile queue saturated";
            return out;
        }
        out.compile_queued = true;
        out.promoted_by_hotness = true;
        out.reason = JitReasonCode::NONE;
        out.detail = "compile queued";
        return out;
    }

    auto JitRuntime::compileExplicit(const JitRuntimeRequest& request,
                                     core::ErrorContext* ctx) -> core::Status
    {
        if (!isNativeEligibleSurface(request.surface))
        {
            return core::Status::INVALID_ARGUMENT;
        }
        if (request.canonical_sblr.empty())
        {
            return core::Status::INVALID_ARGUMENT;
        }
        if (isZeroUuid(request.object_uuid) || isZeroUuid(request.module_id) ||
            isZeroUuid(request.plan_id))
        {
            return core::Status::INVALID_ARGUMENT;
        }
        if (request.compatibility.object_uuid != request.object_uuid ||
            request.compatibility.canonical_sblr_hash.empty() ||
            request.compatibility.target_triple.empty() ||
            request.compatibility.native_abi_version.empty() ||
            request.compatibility.compiler_identity.empty() ||
            request.compatibility.compiler_version.empty() ||
            request.compatibility.optimization_profile.empty())
        {
            return core::Status::INVALID_ARGUMENT;
        }

        JitCompileRequest compile_request{};
        compile_request.key = request.compatibility;
        compile_request.canonical_sblr = request.canonical_sblr;
        JitCompileResult result = compiler_.compile(compile_request);
        if (!result.success)
        {
            switch (result.reason)
            {
                case JitReasonCode::UNSUPPORTED_OPCODE_FAMILY:
                    return core::Status::NOT_SUPPORTED;
                case JitReasonCode::BACKEND_UNAVAILABLE:
                    return core::Status::NOT_IMPLEMENTED;
                case JitReasonCode::BACKEND_COMPILE_FAILED:
                default:
                    return core::Status::INTERNAL_ERROR;
            }
        }
        return materializeArtifact(request, result, ctx);
    }

    auto JitRuntime::rebuildArtifacts(const core::ID& object_uuid,
                                      const std::function<JitCompileRequest()>& request_builder,
                                      core::ErrorContext* ctx) -> core::Status
    {
        core::Status status = artifact_store_.retireByObjectOnPolicyVersionChange(object_uuid, ctx);
        if (status != core::Status::OK)
        {
            return status;
        }

        JitCompileRequest compile_request = request_builder();
        JitCompileResult result = compiler_.compile(compile_request);
        if (!result.success)
        {
            return core::Status::NOT_IMPLEMENTED;
        }

        JitRuntimeRequest request{};
        request.object_uuid = object_uuid;
        request.module_id = object_uuid;
        request.plan_id = object_uuid;
        request.compatibility = compile_request.key;
        request.canonical_sblr = compile_request.canonical_sblr;
        return materializeArtifact(request, result, ctx);
    }

    auto JitRuntime::inspectArtifacts(const core::ID& object_uuid,
                                      std::vector<JitArtifact>& out,
                                      core::ErrorContext* ctx) const -> core::Status
    {
        return artifact_store_.listArtifactsByObject(object_uuid, out, ctx);
    }

    auto JitRuntime::retireArtifact(const core::ID& artifact_id,
                                    core::ErrorContext* ctx) -> core::Status
    {
        return artifact_store_.retireArtifact(artifact_id, ctx);
    }

    auto JitRuntime::drainCompileQueue(core::ErrorContext* ctx) -> size_t
    {
        size_t built = 0;
        JitQueueEntry entry{};
        while (queue_.tryDequeue(entry))
        {
            JitCompileResult result = compiler_.compile(entry.compile_request);
            if (!result.success)
            {
                continue;
            }
            JitRuntimeRequest request{};
            request.object_uuid = entry.object_uuid;
            request.module_id = entry.module_id;
            request.plan_id = entry.plan_id;
            request.compatibility = entry.compile_request.key;
            request.canonical_sblr = entry.compile_request.canonical_sblr;
            if (materializeArtifact(request, result, ctx) == core::Status::OK)
            {
                ++built;
            }
        }
        return built;
    }
}
