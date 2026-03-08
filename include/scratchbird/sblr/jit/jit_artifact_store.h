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

#include <optional>
#include <string>
#include <vector>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/status.h"
#include "scratchbird/sblr/jit/jit_reason_codes.h"

namespace scratchbird::sblr::jit
{
    struct ArtifactCompatibilityKey
    {
        core::ID object_uuid{};
        std::string canonical_sblr_hash;
        std::string target_triple;
        std::string cpu_feature_profile;
        std::string native_abi_version;
        std::string compiler_identity;
        std::string compiler_version;
        std::string optimization_profile;
        uint64_t security_policy_version = 0;
    };

    struct JitArtifact
    {
        core::ID artifact_id{};
        core::ID module_id{};
        core::ID plan_id{};
        core::ID binary_blob_id{};
        core::ID signature_blob_id{};
        bool has_signature_blob_id = false;
        bool has_native_hash = false;
        std::string native_hash_sha256;
        std::vector<uint8_t> native_blob;
        std::vector<uint8_t> signature_blob;
        ArtifactCompatibilityKey compatibility;
        core::CatalogManager::SblrArtifactState state =
            core::CatalogManager::SblrArtifactState::QUEUED;
        uint64_t created_txid = 0;
        uint64_t created_at = 0;
    };

    struct ArtifactVerificationResult
    {
        bool valid = false;
        JitReasonCode reason = JitReasonCode::NONE;
        std::optional<JitArtifact> artifact;
        std::string detail;
    };

    class JitArtifactStore
    {
    public:
        explicit JitArtifactStore(core::CatalogManager* catalog);

        auto upsertArtifact(const JitArtifact& artifact,
                            core::ErrorContext* ctx) -> core::Status;

        auto fetchVerifiedArtifact(const ArtifactCompatibilityKey& key,
                                   bool require_signature,
                                   core::ErrorContext* ctx) const
            -> ArtifactVerificationResult;

        auto retireArtifact(const core::ID& artifact_id,
                            core::ErrorContext* ctx) -> core::Status;

        auto retireByObjectOnDependencySignatureChange(
            const core::ID& object_uuid,
            core::ErrorContext* ctx) -> core::Status;

        auto retireByObjectOnPolicyVersionChange(const core::ID& object_uuid,
                                                 core::ErrorContext* ctx)
            -> core::Status;

        auto listArtifactsByObject(const core::ID& object_uuid,
                                   std::vector<JitArtifact>& out,
                                   core::ErrorContext* ctx) const
            -> core::Status;

        static auto canonicalSblrHashHex(const std::vector<uint8_t>& bytecode)
            -> std::string;

    private:
        core::CatalogManager* catalog_ = nullptr;
    };
}
