/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/sblr/jit/jit_artifact_store.h"

#include <openssl/sha.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <iomanip>
#include <sstream>

#include "scratchbird/core/uuidv7.h"

namespace scratchbird::sblr::jit
{
    namespace
    {
        auto isZeroUuidLocal(const core::ID& id) -> bool
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
    }

    JitArtifactStore::JitArtifactStore(core::CatalogManager* catalog) : catalog_(catalog)
    {
    }

    auto JitArtifactStore::canonicalSblrHashHex(const std::vector<uint8_t>& bytecode)
        -> std::string
    {
        std::array<unsigned char, SHA256_DIGEST_LENGTH> digest{};
        SHA256(bytecode.data(), bytecode.size(), digest.data());
        std::ostringstream out;
        out << std::hex << std::setfill('0');
        for (unsigned char value : digest)
        {
            out << std::setw(2) << static_cast<unsigned int>(value);
        }
        return out.str();
    }

    auto JitArtifactStore::upsertArtifact(const JitArtifact& artifact,
                                          core::ErrorContext* ctx) -> core::Status
    {
        if (catalog_ == nullptr)
        {
            return core::Status::INVALID_ARGUMENT;
        }

        core::CatalogManager::SblrArtifactCatalogInfo info{};
        info.artifact_id = isZeroUuidLocal(artifact.artifact_id)
            ? core::generateUuidV7()
            : artifact.artifact_id;
        info.module_id = artifact.module_id;
        info.plan_id = artifact.plan_id;
        info.object_uuid = artifact.compatibility.object_uuid;
        info.canonical_sblr_hash = artifact.compatibility.canonical_sblr_hash;
        info.target_platform = artifact.compatibility.target_triple;
        info.target_triple = artifact.compatibility.target_triple;
        info.cpu_feature_profile = artifact.compatibility.cpu_feature_profile;
        info.native_abi_version = artifact.compatibility.native_abi_version;
        info.compiler_id = artifact.compatibility.compiler_identity;
        info.compiler_identity = artifact.compatibility.compiler_identity;
        info.compiler_version = artifact.compatibility.compiler_version;
        info.optimization_profile = artifact.compatibility.optimization_profile;
        info.security_policy_version = artifact.compatibility.security_policy_version;
        info.artifact_state = artifact.state;
        info.binary_blob_id = isZeroUuidLocal(artifact.binary_blob_id)
            ? core::generateUuidV7()
            : artifact.binary_blob_id;
        info.hash_sha256 = artifact.has_native_hash
            ? artifact.native_hash_sha256
            : info.canonical_sblr_hash;
        info.has_signature_blob_id = artifact.has_signature_blob_id;
        info.signature_blob_id = artifact.has_signature_blob_id
            ? artifact.signature_blob_id
            : core::ID{};
        info.is_valid = true;
        return catalog_->upsertSblrArtifactCatalogEntry(info, ctx);
    }

    auto JitArtifactStore::fetchVerifiedArtifact(const ArtifactCompatibilityKey& key,
                                                 bool require_signature,
                                                 core::ErrorContext* ctx) const
        -> ArtifactVerificationResult
    {
        ArtifactVerificationResult out;
        if (catalog_ == nullptr)
        {
            out.valid = false;
            out.reason = JitReasonCode::ARTIFACT_NOT_FOUND;
            out.detail = "catalog unavailable";
            return out;
        }

        std::vector<core::CatalogManager::SblrArtifactCatalogInfo> rows;
        core::Status status = catalog_->listSblrArtifactCatalogEntries(core::ID{}, rows, ctx);
        if (status != core::Status::OK)
        {
            out.valid = false;
            out.reason = JitReasonCode::ARTIFACT_NOT_FOUND;
            out.detail = "failed to list artifacts";
            return out;
        }

        JitReasonCode last_reason = JitReasonCode::ARTIFACT_NOT_FOUND;
        std::string last_detail = "artifact not found";

        for (const auto& row : rows)
        {
            if (row.object_uuid != key.object_uuid)
            {
                continue;
            }

            if (row.artifact_state == core::CatalogManager::SblrArtifactState::RETIRED)
            {
                last_reason = JitReasonCode::ARTIFACT_RETIRED;
                last_detail = "artifact retired";
                continue;
            }

            if (core::IdentifierUtils::toUpper(row.target_triple) !=
                core::IdentifierUtils::toUpper(key.target_triple))
            {
                last_reason = JitReasonCode::ARTIFACT_KEY_MISMATCH_TARGET_TRIPLE;
                last_detail = "target_triple mismatch";
                continue;
            }

            if (core::IdentifierUtils::toUpper(row.native_abi_version) !=
                core::IdentifierUtils::toUpper(key.native_abi_version))
            {
                last_reason = JitReasonCode::ARTIFACT_KEY_MISMATCH_NATIVE_ABI;
                last_detail = "native_abi_version mismatch";
                continue;
            }

            if (core::IdentifierUtils::toUpper(row.canonical_sblr_hash) !=
                core::IdentifierUtils::toUpper(key.canonical_sblr_hash))
            {
                last_reason = JitReasonCode::ARTIFACT_KEY_MISMATCH_SBLR_HASH;
                last_detail = "canonical_sblr_hash mismatch";
                continue;
            }

            if (row.security_policy_version != key.security_policy_version)
            {
                last_reason = JitReasonCode::ARTIFACT_KEY_MISMATCH_SECURITY_POLICY;
                last_detail = "security_policy_version mismatch";
                continue;
            }

            const std::string hash_upper = core::IdentifierUtils::toUpper(row.hash_sha256);
            if (hash_upper.size() != 64U)
            {
                last_reason = JitReasonCode::ARTIFACT_HASH_INVALID;
                last_detail = "artifact hash is not SHA-256 hex";
                continue;
            }
            const bool hex_ok = std::all_of(
                hash_upper.begin(),
                hash_upper.end(),
                [](unsigned char c) {
                    return std::isdigit(c) != 0 || (c >= 'A' && c <= 'F');
                });
            if (!hex_ok)
            {
                last_reason = JitReasonCode::ARTIFACT_HASH_INVALID;
                last_detail = "artifact hash contains non-hex characters";
                continue;
            }

            if (require_signature && !row.has_signature_blob_id)
            {
                last_reason = JitReasonCode::ARTIFACT_SIGNATURE_INVALID;
                last_detail = "artifact signature required but not present";
                continue;
            }

            JitArtifact artifact{};
            artifact.artifact_id = row.artifact_id;
            artifact.module_id = row.module_id;
            artifact.plan_id = row.plan_id;
            artifact.binary_blob_id = row.binary_blob_id;
            artifact.has_signature_blob_id = row.has_signature_blob_id;
            artifact.signature_blob_id = row.signature_blob_id;
            artifact.has_native_hash = !row.hash_sha256.empty();
            artifact.native_hash_sha256 = row.hash_sha256;
            artifact.compatibility.object_uuid = row.object_uuid;
            artifact.compatibility.canonical_sblr_hash = row.canonical_sblr_hash;
            artifact.compatibility.target_triple = row.target_triple;
            artifact.compatibility.cpu_feature_profile = row.cpu_feature_profile;
            artifact.compatibility.native_abi_version = row.native_abi_version;
            artifact.compatibility.compiler_identity = row.compiler_identity;
            artifact.compatibility.compiler_version = row.compiler_version;
            artifact.compatibility.optimization_profile = row.optimization_profile;
            artifact.compatibility.security_policy_version = row.security_policy_version;
            artifact.state = row.artifact_state;
            out.valid = true;
            out.reason = JitReasonCode::NONE;
            out.artifact = artifact;
            out.detail = "artifact verified";
            return out;
        }

        out.valid = false;
        out.reason = last_reason;
        out.detail = last_detail;
        return out;
    }

    auto JitArtifactStore::retireArtifact(const core::ID& artifact_id,
                                          core::ErrorContext* ctx) -> core::Status
    {
        if (catalog_ == nullptr)
        {
            return core::Status::INVALID_ARGUMENT;
        }
        core::Status status = catalog_->deleteSblrArtifactStatsCatalogEntry(artifact_id, ctx);
        if (status != core::Status::OK && status != core::Status::NOT_FOUND)
        {
            return status;
        }
        return catalog_->deleteSblrArtifactCatalogEntry(artifact_id, ctx);
    }

    auto JitArtifactStore::listArtifactsByObject(const core::ID& object_uuid,
                                                 std::vector<JitArtifact>& out,
                                                 core::ErrorContext* ctx) const
        -> core::Status
    {
        out.clear();
        if (catalog_ == nullptr)
        {
            return core::Status::INVALID_ARGUMENT;
        }

        std::vector<core::CatalogManager::SblrArtifactCatalogInfo> rows;
        core::Status status = catalog_->listSblrArtifactCatalogEntries(core::ID{}, rows, ctx);
        if (status != core::Status::OK)
        {
            return status;
        }
        for (const auto& row : rows)
        {
            if (row.object_uuid != object_uuid)
            {
                continue;
            }
            JitArtifact artifact{};
            artifact.artifact_id = row.artifact_id;
            artifact.module_id = row.module_id;
            artifact.plan_id = row.plan_id;
            artifact.binary_blob_id = row.binary_blob_id;
            artifact.has_signature_blob_id = row.has_signature_blob_id;
            artifact.signature_blob_id = row.signature_blob_id;
            artifact.has_native_hash = !row.hash_sha256.empty();
            artifact.native_hash_sha256 = row.hash_sha256;
            artifact.compatibility.object_uuid = row.object_uuid;
            artifact.compatibility.canonical_sblr_hash = row.canonical_sblr_hash;
            artifact.compatibility.target_triple = row.target_triple;
            artifact.compatibility.cpu_feature_profile = row.cpu_feature_profile;
            artifact.compatibility.native_abi_version = row.native_abi_version;
            artifact.compatibility.compiler_identity = row.compiler_identity;
            artifact.compatibility.compiler_version = row.compiler_version;
            artifact.compatibility.optimization_profile = row.optimization_profile;
            artifact.compatibility.security_policy_version = row.security_policy_version;
            artifact.state = row.artifact_state;
            out.push_back(std::move(artifact));
        }
        return core::Status::OK;
    }

    auto JitArtifactStore::retireByObjectOnDependencySignatureChange(
        const core::ID& object_uuid,
        core::ErrorContext* ctx) -> core::Status
    {
        std::vector<JitArtifact> artifacts;
        core::Status status = listArtifactsByObject(object_uuid, artifacts, ctx);
        if (status != core::Status::OK)
        {
            return status;
        }
        for (const auto& artifact : artifacts)
        {
            status = retireArtifact(artifact.artifact_id, ctx);
            if (status != core::Status::OK && status != core::Status::NOT_FOUND)
            {
                return status;
            }
        }
        return core::Status::OK;
    }

    auto JitArtifactStore::retireByObjectOnPolicyVersionChange(
        const core::ID& object_uuid,
        core::ErrorContext* ctx) -> core::Status
    {
        return retireByObjectOnDependencySignatureChange(object_uuid, ctx);
    }
}
