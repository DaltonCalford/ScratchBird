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

#include <algorithm>
#include <chrono>
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

        auto isArtifactLoadFailureReason(JitReasonCode reason) -> bool
        {
            switch (reason)
            {
                case JitReasonCode::ARTIFACT_HASH_INVALID:
                case JitReasonCode::ARTIFACT_BLOB_LOAD_FAILED:
                case JitReasonCode::ARTIFACT_HASH_MISMATCH:
                case JitReasonCode::ARTIFACT_PAYLOAD_INVALID:
                case JitReasonCode::ARTIFACT_SIGNATURE_INVALID:
                    return true;
                default:
                    return false;
            }
        }

        auto isRetireOnVerificationFailureReason(JitReasonCode reason) -> bool
        {
            switch (reason)
            {
                case JitReasonCode::ARTIFACT_HASH_INVALID:
                case JitReasonCode::ARTIFACT_BLOB_LOAD_FAILED:
                case JitReasonCode::ARTIFACT_HASH_MISMATCH:
                case JitReasonCode::ARTIFACT_PAYLOAD_INVALID:
                case JitReasonCode::ARTIFACT_SIGNATURE_INVALID:
                    return true;
                default:
                    return false;
            }
        }

        auto statsNowTicks() -> uint64_t
        {
            using namespace std::chrono;
            return static_cast<uint64_t>(
                duration_cast<microseconds>(system_clock::now().time_since_epoch()).count());
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
        artifact.native_blob = compile_result.native_blob;
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
            recordDispatchOutcome(request, out);
            return out;
        }

        const JitEffectivePolicy effective = resolvePolicy(request.policy);

        if (effective.execution_policy == JitExecutionPolicy::INTERPRETED_ONLY)
        {
            out.path = JitDispatchOutcome::Path::VM;
            out.reason = JitReasonCode::POLICY_INTERPRETED_ONLY;
            out.detail = "execution policy requires interpreted path";
            recordDispatchOutcome(request, out);
            return out;
        }
        if (effective.hints.disable_execute)
        {
            out.path = JitDispatchOutcome::Path::VM;
            out.reason = JitReasonCode::HINT_DISABLE_EXECUTE;
            out.detail = "native execution disabled by hint";
            recordDispatchOutcome(request, out);
            return out;
        }
        if (effective.hints.prefer_vm)
        {
            out.path = JitDispatchOutcome::Path::VM;
            out.reason = JitReasonCode::HINT_PREFER_VM;
            out.detail = "native execution bypassed by prefer-vm hint";
            recordDispatchOutcome(request, out);
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
            recordDispatchOutcome(request, out);
            return out;
        }

        if (verified.artifact.has_value() && isArtifactLoadFailureReason(verified.reason))
        {
            (void)recordArtifactFallback(verified.artifact.value(), true, ctx);
        }
        else if (verified.artifact.has_value() &&
                 verified.reason != JitReasonCode::ARTIFACT_NOT_FOUND)
        {
            (void)recordArtifactFallback(verified.artifact.value(), false, ctx);
        }

        if (verified.artifact.has_value() &&
            isRetireOnVerificationFailureReason(verified.reason))
        {
            retireUnusableArtifact(verified.artifact.value(), ctx);
        }

        if (effective.execution_policy == JitExecutionPolicy::REQUIRE_NATIVE)
        {
            out.path = JitDispatchOutcome::Path::ERROR;
            out.reason = JitReasonCode::REQUIRE_NATIVE_NOT_AVAILABLE;
            out.detail = verified.detail.empty()
                ? "REQUIRE_NATIVE requested but no valid artifact exists"
                : verified.detail;
            if (verified.artifact.has_value() &&
                isRetireOnVerificationFailureReason(verified.reason) &&
                !effective.hints.disable_compile &&
                effective.compile_mode == JitCompileMode::JIT_ALLOWED)
            {
                queueCompileForRequest(request, true, out);
            }
            recordDispatchOutcome(request, out);
            return out;
        }

        out.path = JitDispatchOutcome::Path::VM;
        out.reason = verified.reason;
        out.detail = verified.detail;

        if (verified.artifact.has_value() &&
            isRetireOnVerificationFailureReason(verified.reason) &&
            !effective.hints.disable_compile &&
            effective.compile_mode == JitCompileMode::JIT_ALLOWED)
        {
            queueCompileForRequest(request, true, out);
        }

        if (verified.reason != JitReasonCode::ARTIFACT_NOT_FOUND)
        {
            recordDispatchOutcome(request, out);
            return out;
        }

        if (effective.hints.disable_compile)
        {
            out.reason = JitReasonCode::HINT_DISABLE_COMPILE;
            out.detail = "compile suppressed by hint";
            recordDispatchOutcome(request, out);
            return out;
        }
        if (effective.compile_mode == JitCompileMode::EXPLICIT_ONLY)
        {
            out.reason = JitReasonCode::COMPILE_MODE_EXPLICIT_ONLY;
            out.detail = "compile mode is explicit-only";
            recordDispatchOutcome(request, out);
            return out;
        }
        queueCompileForRequest(request, false, out);
        recordDispatchOutcome(request, out);
        return out;
    }

    auto JitRuntime::compileExplicit(const JitRuntimeRequest& request,
                                     core::ErrorContext* ctx) -> core::Status
    {
        const auto compile_start = std::chrono::steady_clock::now();
        performance_.explicit_compile_attempt_count += 1;
        if (!isNativeEligibleSurface(request.surface))
        {
            performance_.explicit_compile_failure_count += 1;
            noteCompileResult(
                request.object_uuid,
                false,
                static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - compile_start)
                        .count()));
            return core::Status::INVALID_ARGUMENT;
        }
        if (request.canonical_sblr.empty())
        {
            performance_.explicit_compile_failure_count += 1;
            noteCompileResult(
                request.object_uuid,
                false,
                static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - compile_start)
                        .count()));
            return core::Status::INVALID_ARGUMENT;
        }
        if (isZeroUuid(request.object_uuid) || isZeroUuid(request.module_id) ||
            isZeroUuid(request.plan_id))
        {
            performance_.explicit_compile_failure_count += 1;
            noteCompileResult(
                request.object_uuid,
                false,
                static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - compile_start)
                        .count()));
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
            performance_.explicit_compile_failure_count += 1;
            noteCompileResult(
                request.object_uuid,
                false,
                static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - compile_start)
                        .count()));
            return core::Status::INVALID_ARGUMENT;
        }

        JitCompileRequest compile_request{};
        compile_request.key = request.compatibility;
        compile_request.canonical_sblr = request.canonical_sblr;
        JitCompileResult result = compiler_.compile(compile_request);
        const uint64_t compile_latency_us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - compile_start)
                .count());
        noteCompileResult(request.object_uuid, result.success, compile_latency_us);
        if (!result.success)
        {
            performance_.explicit_compile_failure_count += 1;
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
        performance_.explicit_compile_success_count += 1;
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
            performance_.compile_queue_current_depth = queue_.size();
            performance_.queued_compile_attempt_count += 1;
            const auto compile_start = std::chrono::steady_clock::now();
            JitCompileResult result = compiler_.compile(entry.compile_request);
            const uint64_t compile_latency_us = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::steady_clock::now() - compile_start)
                    .count());
            noteCompileResult(entry.object_uuid, result.success, compile_latency_us);
            if (!result.success)
            {
                performance_.queued_compile_failure_count += 1;
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
                performance_.queued_compile_success_count += 1;
                ++built;
            }
            else
            {
                performance_.queued_compile_failure_count += 1;
            }
        }
        return built;
    }

    auto JitRuntime::performanceSnapshot() const -> JitPerformanceSnapshot
    {
        return performance_;
    }

    auto JitRuntime::objectPerformance(const core::ID& object_uuid) const
        -> JitObjectPerformance
    {
        const auto it = object_performance_.find(object_uuid.toString());
        if (it == object_performance_.end())
        {
            JitObjectPerformance info{};
            info.object_uuid = object_uuid;
            return info;
        }
        return it->second;
    }

    auto JitRuntime::recordArtifactExecution(const JitArtifact& artifact,
                                             uint64_t execution_cpu_us,
                                             core::ErrorContext* ctx) -> core::Status
    {
        performance_.native_execution_count += 1;
        performance_.native_execution_cpu_us += execution_cpu_us;
        return mutateArtifactStats(
            artifact.artifact_id,
            [execution_cpu_us](core::CatalogManager::SblrArtifactStatsCatalogInfo& info) {
                info.execution_count += 1;
                info.execution_cpu_us += execution_cpu_us;
                info.last_used_at = statsNowTicks();
            },
            ctx);
    }

    auto JitRuntime::recordArtifactFallback(const JitArtifact& artifact,
                                            bool load_failure,
                                            core::ErrorContext* ctx) -> core::Status
    {
        performance_.fallback_count += 1;
        if (load_failure)
        {
            performance_.load_failure_count += 1;
        }
        objectMetricsFor(artifact.compatibility.object_uuid).fallback_count += 1;
        return mutateArtifactStats(
            artifact.artifact_id,
            [load_failure](core::CatalogManager::SblrArtifactStatsCatalogInfo& info) {
                info.fallback_count += 1;
                if (load_failure)
                {
                    info.load_failure_count += 1;
                }
                info.last_used_at = statsNowTicks();
            },
            ctx);
    }

    auto JitRuntime::queueCompileForRequest(const JitRuntimeRequest& request,
                                            bool bypass_hotness,
                                            JitDispatchOutcome& out) -> void
    {
        auto& object_stats = objectMetricsFor(request.object_uuid);
        const bool preserve_reason =
            out.reason != JitReasonCode::NONE &&
            out.reason != JitReasonCode::ARTIFACT_NOT_FOUND;
        const std::string prior_detail = out.detail;
        if (!bypass_hotness)
        {
            const std::string hot_key = hotnessKeyFor(request);
            uint32_t& seen = hotness_[hot_key];
            ++seen;
            object_stats.hotness_observation_count = seen;
            if (seen < hotness_threshold_)
            {
                out.reason = JitReasonCode::HOTNESS_BELOW_THRESHOLD;
                out.detail = "routine has not reached hotness threshold";
                performance_.hotness_below_threshold_count += 1;
                return;
            }
        }

        JitQueueEntry entry{};
        entry.queue_id = core::generateUuidV7();
        entry.object_uuid = request.object_uuid;
        entry.module_id = request.module_id;
        entry.plan_id = request.plan_id;
        entry.priority = 0;
        entry.dedupe_key = compileQueueKeyFor(request.compatibility);
        entry.compile_request.key = request.compatibility;
        entry.compile_request.canonical_sblr = request.canonical_sblr;

        JitReasonCode queue_reason = JitReasonCode::NONE;
        if (!queue_.tryEnqueue(entry, queue_reason))
        {
            if (!preserve_reason)
            {
                out.reason = queue_reason;
            }
            if (queue_reason == JitReasonCode::COMPILE_ALREADY_QUEUED)
            {
                out.detail = preserve_reason && !prior_detail.empty()
                    ? prior_detail + "; compile already queued for compatibility key"
                    : "compile already queued for compatibility key";
                performance_.compile_queue_duplicate_count += 1;
                object_stats.compile_queue_duplicate_count += 1;
            }
            else
            {
                out.detail = preserve_reason && !prior_detail.empty()
                    ? prior_detail + "; compile queue saturated"
                    : "compile queue saturated";
                performance_.compile_queue_saturated_count += 1;
            }
            return;
        }

        out.compile_queued = true;
        out.promoted_by_hotness = !bypass_hotness;
        if (!preserve_reason)
        {
            out.reason = JitReasonCode::NONE;
            out.detail = bypass_hotness
                ? "unusable artifact retired and recompile queued"
                : "compile queued";
        }
        else
        {
            out.detail = prior_detail.empty()
                ? (bypass_hotness
                       ? "unusable artifact retired and recompile queued"
                       : "compile queued")
                : prior_detail + (bypass_hotness
                                      ? "; unusable artifact retired and recompile queued"
                                      : "; compile queued");
        }
        performance_.compile_queue_enqueued_count += 1;
        performance_.compile_queue_current_depth = queue_.size();
        performance_.compile_queue_max_depth =
            std::max(performance_.compile_queue_max_depth,
                     performance_.compile_queue_current_depth);
        object_stats.compile_queue_enqueued_count += 1;
        if (!bypass_hotness)
        {
            performance_.hotness_promotion_count += 1;
            object_stats.hotness_promotion_count += 1;
        }
    }

    auto JitRuntime::recordDispatchOutcome(const JitRuntimeRequest& request,
                                           const JitDispatchOutcome& out) -> void
    {
        auto& object_stats = objectMetricsFor(request.object_uuid);
        object_stats.total_dispatch_count += 1;
        switch (out.path)
        {
            case JitDispatchOutcome::Path::VM:
                performance_.vm_dispatch_count += 1;
                object_stats.vm_dispatch_count += 1;
                break;
            case JitDispatchOutcome::Path::NATIVE:
                performance_.native_dispatch_count += 1;
                object_stats.native_dispatch_count += 1;
                break;
            case JitDispatchOutcome::Path::ERROR:
                performance_.error_dispatch_count += 1;
                object_stats.error_dispatch_count += 1;
                break;
        }
    }

    auto JitRuntime::noteCompileResult(const core::ID& object_uuid,
                                       bool success,
                                       uint64_t compile_latency_us) -> void
    {
        performance_.total_compile_latency_us += compile_latency_us;
        performance_.last_compile_latency_us = compile_latency_us;
        auto& object_stats = objectMetricsFor(object_uuid);
        if (success)
        {
            object_stats.compile_success_count += 1;
        }
        else
        {
            object_stats.compile_failure_count += 1;
        }
    }

    auto JitRuntime::retireUnusableArtifact(const JitArtifact& artifact,
                                            core::ErrorContext* ctx) -> void
    {
        if (artifact_store_.retireArtifact(artifact.artifact_id, ctx) == core::Status::OK)
        {
            performance_.retired_unusable_artifact_count += 1;
        }
    }

    auto JitRuntime::objectMetricsFor(const core::ID& object_uuid)
        -> JitObjectPerformance&
    {
        const std::string key = object_uuid.toString();
        auto [it, inserted] = object_performance_.try_emplace(key);
        if (inserted)
        {
            it->second.object_uuid = object_uuid;
        }
        return it->second;
    }

    auto JitRuntime::mutateArtifactStats(
        const core::ID& artifact_id,
        const std::function<void(core::CatalogManager::SblrArtifactStatsCatalogInfo&)>& mutator,
        core::ErrorContext* ctx) -> core::Status
    {
        if (catalog_ == nullptr || isZeroUuid(artifact_id))
        {
            return core::Status::INVALID_ARGUMENT;
        }

        core::CatalogManager::SblrArtifactStatsCatalogInfo stats{};
        core::Status status = catalog_->getSblrArtifactStatsCatalogEntry(artifact_id, stats, ctx);
        if (status == core::Status::NOT_FOUND)
        {
            stats = core::CatalogManager::SblrArtifactStatsCatalogInfo{};
            stats.artifact_id = artifact_id;
        }
        else if (status != core::Status::OK)
        {
            return status;
        }

        mutator(stats);
        return catalog_->upsertSblrArtifactStatsCatalogEntry(stats, ctx);
    }
}
