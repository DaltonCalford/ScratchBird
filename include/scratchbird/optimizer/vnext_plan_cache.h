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

#include "scratchbird/core/uuidv7.h"
#include "scratchbird/sblr/v3_plan_cache_key.h"

#include <cstdint>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird::optimizer
{
    enum class NativeArtifactStatus : uint8_t
    {
        NOT_REQUESTED = 0,
        GENERATED = 1,
        FALLBACK_SBLR_ONLY = 2,
        REJECTED = 3
    };

    struct NativeArtifactMetadata
    {
        std::string target_triple;
        std::string object_format;
        std::string host_api_abi_version;
        std::string artifact_hash;
        uint64_t artifact_size_bytes = 0;
    };

    struct VNextPlanCacheValue
    {
        std::string native_feature_key;
        std::string normalized_payload_hash;
        std::string native_ast_hash;
        std::string sblr_hash;
        std::vector<uint8_t> sblr_payload;
        core::ID compile_module_id;
        uint64_t compile_time_us = 0;

        NativeArtifactStatus native_artifact_status = NativeArtifactStatus::NOT_REQUESTED;
        std::vector<NativeArtifactMetadata> native_artifacts;
        std::string fallback_reason_code;
    };

    struct VNextPlanCachePutResult
    {
        bool ok = false;
        std::string error_code;
        std::string error_message;
    };

    struct VNextPlanCacheGetResult
    {
        bool ok = false;
        bool hit = false;
        std::string error_code;
        std::string error_message;
        VNextPlanCacheValue value;
    };

    struct VNextPlanCacheStats
    {
        uint64_t hits = 0;
        uint64_t misses = 0;
        uint64_t inserts = 0;
        uint64_t invalidations = 0;
        uint64_t errors = 0;
        uint64_t entries = 0;
    };

    class VNextPlanCache
    {
    public:
        VNextPlanCache() = default;

        auto put(const sblr::v3::PlanCacheKeyInput &key,
                 const VNextPlanCacheValue &value) -> VNextPlanCachePutResult;
        auto get(const sblr::v3::PlanCacheKeyInput &key) -> VNextPlanCacheGetResult;

        auto invalidateAll() -> uint64_t;
        auto invalidateByPayloadHash(const std::string &payload_hash) -> uint64_t;
        auto invalidateByCatalogEpoch(uint64_t expected_epoch) -> uint64_t;
        auto invalidateBySecurityEpoch(uint64_t expected_epoch) -> uint64_t;
        auto invalidateByCapabilitySetHash(const std::string &hash) -> uint64_t;
        auto invalidateByModuleVersion(uint64_t module_version) -> uint64_t;
        auto invalidateByTranslationRuleVersion(uint64_t rule_version) -> uint64_t;
        auto invalidateByHostApiAbiVersion(const std::string &abi_version) -> uint64_t;
        auto invalidateByTargetTriplesHash(const std::string &target_triples_hash) -> uint64_t;

        auto getStats() const -> VNextPlanCacheStats;
        auto resetStats() -> void;

    private:
        struct CacheEntry
        {
            sblr::v3::PlanCacheKeyInput key;
            VNextPlanCacheValue value;
            std::string canonical_key;
        };

        std::unordered_map<std::string, CacheEntry> entries_;
        mutable std::shared_mutex mutex_;
        mutable VNextPlanCacheStats stats_{};

        static auto validateKey(const sblr::v3::PlanCacheKeyInput &key,
                                std::string &message) -> bool;
        static auto validateValue(const VNextPlanCacheValue &value,
                                  std::string &message) -> bool;
        static auto isSortedByTargetTriple(
            const std::vector<NativeArtifactMetadata> &artifacts) -> bool;
    };

} // namespace scratchbird::optimizer
