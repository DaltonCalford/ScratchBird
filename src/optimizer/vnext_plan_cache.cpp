/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/optimizer/vnext_plan_cache.h"
#include "scratchbird/core/vnext_metrics_event_model.h"

#include <algorithm>
#include <mutex>
#include <utility>

namespace scratchbird::optimizer
{
    namespace
    {
        constexpr const char *kCacheViolationCode = "UDR_1511";
    }

    auto VNextPlanCache::validateKey(const sblr::v3::PlanCacheKeyInput &key,
                                     std::string &message) -> bool
    {
        if (key.profile_id.empty())
        {
            message = "profile_id is required";
            return false;
        }
        if (key.profile_version.empty())
        {
            message = "profile_version is required";
            return false;
        }
        if (key.payload_format.empty())
        {
            message = "payload_format is required";
            return false;
        }
        if (key.payload_hash.empty())
        {
            message = "payload_hash is required";
            return false;
        }
        if (key.session_option_signature.empty())
        {
            message = "session_option_signature is required";
            return false;
        }
        if (key.role_context_signature.empty())
        {
            message = "role_context_signature is required";
            return false;
        }
        if (key.capability_set_hash.empty())
        {
            message = "capability_set_hash is required";
            return false;
        }
        if (key.module_version == 0)
        {
            message = "module_version must be non-zero";
            return false;
        }
        if (key.translation_rule_version == 0)
        {
            message = "translation_rule_version must be non-zero";
            return false;
        }
        if (key.catalog_epoch == 0)
        {
            message = "catalog_epoch must be non-zero";
            return false;
        }
        if (key.security_epoch == 0)
        {
            message = "security_epoch must be non-zero";
            return false;
        }
        if (key.artifact_preference != "SBLR_ONLY")
        {
            if (key.host_api_abi_version.empty())
            {
                message =
                    "host_api_abi_version is required for native artifact preference";
                return false;
            }
            if (key.target_triples_hash.empty())
            {
                message = "target_triples_hash is required for native artifact preference";
                return false;
            }
            if (key.optimization_level.empty())
            {
                message = "optimization_level is required for native artifact preference";
                return false;
            }
        }
        return true;
    }

    auto VNextPlanCache::isSortedByTargetTriple(
        const std::vector<NativeArtifactMetadata> &artifacts) -> bool
    {
        return std::is_sorted(
            artifacts.begin(),
            artifacts.end(),
            [](const NativeArtifactMetadata &lhs, const NativeArtifactMetadata &rhs)
            {
                return lhs.target_triple < rhs.target_triple;
            });
    }

    auto VNextPlanCache::validateValue(const VNextPlanCacheValue &value,
                                       std::string &message) -> bool
    {
        if (value.native_feature_key.empty())
        {
            message = "native_feature_key is required";
            return false;
        }
        if (value.normalized_payload_hash.empty())
        {
            message = "normalized_payload_hash is required";
            return false;
        }
        if (value.native_ast_hash.empty())
        {
            message = "native_ast_hash is required";
            return false;
        }
        if (value.sblr_hash.empty())
        {
            message = "sblr_hash is required";
            return false;
        }
        if (value.sblr_payload.empty())
        {
            message = "sblr_payload must not be empty";
            return false;
        }
        if (value.compile_module_id == core::ID{})
        {
            message = "compile_module_id must be non-zero";
            return false;
        }
        if (value.native_artifact_status == NativeArtifactStatus::FALLBACK_SBLR_ONLY &&
            value.fallback_reason_code.empty())
        {
            message =
                "fallback_reason_code required for FALLBACK_SBLR_ONLY artifact status";
            return false;
        }
        if (value.native_artifact_status == NativeArtifactStatus::GENERATED &&
            value.native_artifacts.empty())
        {
            message = "native_artifacts required for GENERATED artifact status";
            return false;
        }
        if (!isSortedByTargetTriple(value.native_artifacts))
        {
            message = "native_artifacts must be sorted by target_triple";
            return false;
        }
        return true;
    }

    auto VNextPlanCache::put(const sblr::v3::PlanCacheKeyInput &key,
                             const VNextPlanCacheValue &value)
        -> VNextPlanCachePutResult
    {
        std::string validation_message;
        if (!validateKey(key, validation_message) ||
            !validateValue(value, validation_message))
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            ++stats_.errors;
            core::VNextMetricsEventModel::recordOptimizerEvent(
                "plan_cache_put", "reject", kCacheViolationCode);
            return {false, kCacheViolationCode, validation_message};
        }

        const std::string canonical_key = sblr::v3::buildPlanCacheKey(key);

        std::unique_lock<std::shared_mutex> lock(mutex_);
        if (entries_.find(canonical_key) != entries_.end())
        {
            ++stats_.errors;
            core::VNextMetricsEventModel::recordOptimizerEvent(
                "plan_cache_put", "reject", kCacheViolationCode);
            return {false, kCacheViolationCode,
                    "cache entries are immutable after write"};
        }

        CacheEntry entry;
        entry.key = key;
        entry.value = value;
        entry.canonical_key = canonical_key;
        entries_.emplace(canonical_key, std::move(entry));
        ++stats_.inserts;
        stats_.entries = entries_.size();
        core::VNextMetricsEventModel::recordOptimizerEvent(
            "plan_cache_put", "ok", "NONE");
        return {true, "", ""};
    }

    auto VNextPlanCache::get(const sblr::v3::PlanCacheKeyInput &key)
        -> VNextPlanCacheGetResult
    {
        std::string validation_message;
        if (!validateKey(key, validation_message))
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            ++stats_.errors;
            core::VNextMetricsEventModel::recordOptimizerEvent(
                "plan_cache_get", "reject", kCacheViolationCode);
            return {false, false, kCacheViolationCode, validation_message, {}};
        }

        const std::string canonical_key = sblr::v3::buildPlanCacheKey(key);
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = entries_.find(canonical_key);
        if (it == entries_.end())
        {
            ++stats_.misses;
            core::VNextMetricsEventModel::recordOptimizerEvent(
                "plan_cache_get", "miss", "NONE");
            return {true, false, "", "", {}};
        }

        if (!validateValue(it->second.value, validation_message))
        {
            entries_.erase(it);
            ++stats_.errors;
            ++stats_.misses;
            stats_.entries = entries_.size();
            core::VNextMetricsEventModel::recordOptimizerEvent(
                "plan_cache_get", "reject", kCacheViolationCode);
            return {false, false, kCacheViolationCode, validation_message, {}};
        }

        ++stats_.hits;
        core::VNextMetricsEventModel::recordOptimizerEvent(
            "plan_cache_get", "hit", "NONE");
        return {true, true, "", "", it->second.value};
    }

    auto VNextPlanCache::invalidateAll() -> uint64_t
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        const uint64_t removed = entries_.size();
        entries_.clear();
        stats_.invalidations += removed;
        stats_.entries = 0;
        core::VNextMetricsEventModel::recordOptimizerEvent(
            "plan_cache_invalidate_all", "ok", "NONE", static_cast<double>(removed));
        return removed;
    }

    auto VNextPlanCache::invalidateByPayloadHash(const std::string &payload_hash) -> uint64_t
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        uint64_t removed = 0;
        for (auto it = entries_.begin(); it != entries_.end();)
        {
            if (it->second.key.payload_hash == payload_hash)
            {
                it = entries_.erase(it);
                ++removed;
            }
            else
            {
                ++it;
            }
        }
        stats_.invalidations += removed;
        stats_.entries = entries_.size();
        core::VNextMetricsEventModel::recordOptimizerEvent(
            "plan_cache_invalidate_payload_hash", "ok", "NONE",
            static_cast<double>(removed));
        return removed;
    }

    auto VNextPlanCache::invalidateByCatalogEpoch(uint64_t expected_epoch) -> uint64_t
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        uint64_t removed = 0;
        for (auto it = entries_.begin(); it != entries_.end();)
        {
            if (it->second.key.catalog_epoch != expected_epoch)
            {
                it = entries_.erase(it);
                ++removed;
            }
            else
            {
                ++it;
            }
        }
        stats_.invalidations += removed;
        stats_.entries = entries_.size();
        core::VNextMetricsEventModel::recordOptimizerEvent(
            "plan_cache_invalidate_catalog_epoch", "ok", "NONE", static_cast<double>(removed));
        return removed;
    }

    auto VNextPlanCache::invalidateBySecurityEpoch(uint64_t expected_epoch) -> uint64_t
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        uint64_t removed = 0;
        for (auto it = entries_.begin(); it != entries_.end();)
        {
            if (it->second.key.security_epoch != expected_epoch)
            {
                it = entries_.erase(it);
                ++removed;
            }
            else
            {
                ++it;
            }
        }
        stats_.invalidations += removed;
        stats_.entries = entries_.size();
        core::VNextMetricsEventModel::recordOptimizerEvent(
            "plan_cache_invalidate_security_epoch", "ok", "NONE", static_cast<double>(removed));
        return removed;
    }

    auto VNextPlanCache::invalidateByCapabilitySetHash(const std::string &hash) -> uint64_t
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        uint64_t removed = 0;
        for (auto it = entries_.begin(); it != entries_.end();)
        {
            if (it->second.key.capability_set_hash != hash)
            {
                it = entries_.erase(it);
                ++removed;
            }
            else
            {
                ++it;
            }
        }
        stats_.invalidations += removed;
        stats_.entries = entries_.size();
        core::VNextMetricsEventModel::recordOptimizerEvent(
            "plan_cache_invalidate_capability_hash", "ok", "NONE", static_cast<double>(removed));
        return removed;
    }

    auto VNextPlanCache::invalidateByModuleVersion(uint64_t module_version) -> uint64_t
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        uint64_t removed = 0;
        for (auto it = entries_.begin(); it != entries_.end();)
        {
            if (it->second.key.module_version != module_version)
            {
                it = entries_.erase(it);
                ++removed;
            }
            else
            {
                ++it;
            }
        }
        stats_.invalidations += removed;
        stats_.entries = entries_.size();
        core::VNextMetricsEventModel::recordOptimizerEvent(
            "plan_cache_invalidate_module_version", "ok", "NONE", static_cast<double>(removed));
        return removed;
    }

    auto VNextPlanCache::invalidateByTranslationRuleVersion(uint64_t rule_version)
        -> uint64_t
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        uint64_t removed = 0;
        for (auto it = entries_.begin(); it != entries_.end();)
        {
            if (it->second.key.translation_rule_version != rule_version)
            {
                it = entries_.erase(it);
                ++removed;
            }
            else
            {
                ++it;
            }
        }
        stats_.invalidations += removed;
        stats_.entries = entries_.size();
        core::VNextMetricsEventModel::recordOptimizerEvent(
            "plan_cache_invalidate_translation_rule", "ok", "NONE", static_cast<double>(removed));
        return removed;
    }

    auto VNextPlanCache::invalidateByHostApiAbiVersion(const std::string &abi_version)
        -> uint64_t
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        uint64_t removed = 0;
        for (auto it = entries_.begin(); it != entries_.end();)
        {
            if (it->second.key.host_api_abi_version != abi_version)
            {
                it = entries_.erase(it);
                ++removed;
            }
            else
            {
                ++it;
            }
        }
        stats_.invalidations += removed;
        stats_.entries = entries_.size();
        core::VNextMetricsEventModel::recordOptimizerEvent(
            "plan_cache_invalidate_host_abi", "ok", "NONE", static_cast<double>(removed));
        return removed;
    }

    auto VNextPlanCache::invalidateByTargetTriplesHash(
        const std::string &target_triples_hash) -> uint64_t
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        uint64_t removed = 0;
        for (auto it = entries_.begin(); it != entries_.end();)
        {
            if (it->second.key.target_triples_hash != target_triples_hash)
            {
                it = entries_.erase(it);
                ++removed;
            }
            else
            {
                ++it;
            }
        }
        stats_.invalidations += removed;
        stats_.entries = entries_.size();
        core::VNextMetricsEventModel::recordOptimizerEvent(
            "plan_cache_invalidate_target_triples", "ok", "NONE", static_cast<double>(removed));
        return removed;
    }

    auto VNextPlanCache::getStats() const -> VNextPlanCacheStats
    {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        VNextPlanCacheStats out = stats_;
        out.entries = entries_.size();
        return out;
    }

    auto VNextPlanCache::resetStats() -> void
    {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        const uint64_t entries = entries_.size();
        stats_ = VNextPlanCacheStats{};
        stats_.entries = entries;
    }

} // namespace scratchbird::optimizer
