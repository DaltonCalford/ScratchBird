#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/jit/jit_artifact_store.h"
#include "scratchbird/sblr/jit/jit_compiler.h"
#include "scratchbird/sblr/jit/jit_llvm_toolchain.h"
#include "scratchbird/sblr/jit/jit_runtime.h"
#include "scratchbird/sblr/query_compiler_v3.h"
#include "test_helpers.h"

namespace scratchbird::sblr::jit::test
{
    struct ModulePlanBinding
    {
        core::ID object_uuid{};
        core::ID module_id{};
        core::ID plan_id{};
    };

    class SblrJitFixture : public ::testing::Test
    {
    protected:
        static auto isZeroUuid(const core::ID& id) -> bool
        {
            for (uint8_t value : id.bytes)
            {
                if (value != 0)
                {
                    return false;
                }
            }
            return true;
        }

        auto resolveDefaultSchema(core::ErrorContext* ctx) -> core::ID
        {
            std::vector<core::CatalogManager::SchemaInfo> schemas;
            core::Status status = db_->catalog_manager()->listSchemas(schemas, ctx);
            if (status == core::Status::OK && !schemas.empty())
            {
                return schemas.front().schema_id;
            }

            core::ID schema_id{};
            status = db_->catalog_manager()->createSchema("main", "SYSTEM", schema_id, ctx);
            if (status == core::Status::OK)
            {
                return schema_id;
            }
            return core::ID{};
        }

        void SetUp() override
        {
            if (!jit::llvmToolchainAvailable())
            {
                GTEST_SKIP() << "LLVM JIT provider not available in this build";
            }

            db_path_ = scratchbird::testing::uniqueTestDbPath("test_sblr_jit", ".db");
            std::filesystem::remove(db_path_);

            core::ErrorContext ctx;
            ASSERT_EQ(core::Database::create(db_path_, 8192, &ctx), core::Status::OK)
                << ctx.message;

            db_ = std::make_unique<core::Database>();
            ASSERT_EQ(db_->open(db_path_, &ctx), core::Status::OK) << ctx.message;

            const core::Status proc_status = db_->initializeProcArray(16, &ctx);
            ASSERT_TRUE(proc_status == core::Status::OK ||
                        proc_status == core::Status::INVALID_ARGUMENT)
                << ctx.message;

            ASSERT_EQ(db_->connect(conn_ctx_, &ctx), core::Status::OK) << ctx.message;
            core::ConnectionContext::setCurrent(conn_ctx_.get());
            ASSERT_EQ(conn_ctx_->initialize(&ctx), core::Status::OK) << ctx.message;

            system_user_id_ = db_->catalog_manager()->getSystemUserId(&ctx);
            conn_ctx_->setCurrentUser(system_user_id_, true);
            default_schema_id_ = resolveDefaultSchema(&ctx);
            ASSERT_FALSE(isZeroUuid(default_schema_id_));
        }

        void TearDown() override
        {
            core::ConnectionContext::setCurrent(nullptr);
            conn_ctx_.reset();
            db_.reset();
            std::filesystem::remove(db_path_);
        }

        auto compileSql(const std::string& sql) -> std::vector<uint8_t>
        {
            sblr::QueryCompilerV3 compiler(db_.get());
            compiler.setCurrentSchema(default_schema_id_);
            auto result = compiler.compile(sql);
            if (!result.success())
            {
                return {};
            }
            return result.bytecode();
        }

        auto compileCanonicalRoutine() -> std::vector<uint8_t>
        {
            auto bytecode = compileSql("SELECT 1");
            EXPECT_FALSE(bytecode.empty());
            return bytecode;
        }

        auto createModulePlanBinding() -> ModulePlanBinding
        {
            core::ErrorContext ctx;
            ModulePlanBinding binding{};
            binding.object_uuid = core::generateUuidV7();
            binding.module_id = core::generateUuidV7();
            binding.plan_id = core::generateUuidV7();
            const std::string feature_key =
                std::string("jit_select_") + binding.module_id.toString().substr(0, 8);
            uint64_t module_checksum = 0;
            for (size_t i = 0; i < sizeof(module_checksum); ++i)
            {
                module_checksum =
                    (module_checksum << 8) |
                    static_cast<uint64_t>(binding.module_id.bytes[i]);
            }

            core::CatalogManager::SblrModuleCatalogInfo module{};
            module.module_id = binding.module_id;
            module.sblr_checksum = module_checksum;
            module.feature_key = feature_key;
            module.result_shape_id = "shape_scalar";
            module.payload_schema_id = "schema_v3";
            module.container_blob_id = core::generateUuidV7();
            module.normalization_evidence_hash = 0x1234ABCDU;
            module.statement_norm_count = 0;
            module.capability_profile_version = 1;
            module.created_txid = 1;
            if (db_->catalog_manager()->upsertSblrModuleCatalogEntry(module, &ctx) !=
                core::Status::OK)
            {
                ADD_FAILURE() << ctx.message;
                return binding;
            }

            core::CatalogManager::SblrStatementNormCatalogInfo norm{};
            norm.module_id = binding.module_id;
            norm.statement_id = core::generateUuidV7();
            norm.statement_order = 0;
            norm.feature_key = feature_key;
            norm.ast_family = "dml_select";
            norm.normalization_rule_set_id = 1;
            norm.clause_presence_mask_lo = 1;
            norm.clause_presence_mask_hi = 0;
            norm.clause_order_checksum = 0x55U;
            norm.alias_rewrite_flags = 0;
            norm.created_txid = 2;
            if (db_->catalog_manager()->upsertSblrStatementNormCatalogEntry(norm, &ctx) !=
                core::Status::OK)
            {
                ADD_FAILURE() << ctx.message;
                return binding;
            }

            module.statement_norm_count = 1;
            if (db_->catalog_manager()->upsertSblrModuleCatalogEntry(module, &ctx) !=
                core::Status::OK)
            {
                ADD_FAILURE() << ctx.message;
                return binding;
            }

            core::CatalogManager::SblrPlanCatalogInfo plan{};
            plan.plan_id = binding.plan_id;
            plan.module_id = binding.module_id;
            plan.catalog_epoch = 1;
            plan.security_epoch = 1;
            plan.normalization_evidence_hash = module.normalization_evidence_hash;
            plan.plan_checksum = 0xCAFEBABEULL;
            plan.dependency_count = 0;
            plan.plan_blob_id = core::generateUuidV7();
            plan.created_txid = 3;
            if (db_->catalog_manager()->upsertSblrPlanCatalogEntry(plan, &ctx) !=
                core::Status::OK)
            {
                ADD_FAILURE() << ctx.message;
                return binding;
            }

            return binding;
        }

        auto makeCompatibilityKey(const core::ID& object_uuid,
                                  const std::vector<uint8_t>& canonical_sblr,
                                  const std::string& target = "",
                                  const std::string& abi = "sb_abi_v1",
                                  uint64_t security_policy_version = 1)
            -> jit::ArtifactCompatibilityKey
        {
            const jit::LlvmToolchainInfo& llvm_info = jit::llvmToolchainInfo();
            jit::ArtifactCompatibilityKey key{};
            key.object_uuid = object_uuid;
            key.canonical_sblr_hash =
                jit::JitArtifactStore::canonicalSblrHashHex(canonical_sblr);
            key.target_triple = jit::normalizeLlvmTargetTriple(target);
            key.cpu_feature_profile = "generic";
            key.native_abi_version = abi;
            key.compiler_identity = llvm_info.provider_identity;
            key.compiler_version = llvm_info.provider_version;
            key.optimization_profile = "O2";
            key.security_policy_version = security_policy_version;
            return key;
        }

        auto makeRuntimeRequest(const ModulePlanBinding& binding,
                                const std::vector<uint8_t>& canonical_sblr)
            -> jit::JitRuntimeRequest
        {
            jit::JitRuntimeRequest request{};
            request.surface = jit::RoutineSurfaceKind::FUNCTION;
            request.object_uuid = binding.object_uuid;
            request.module_id = binding.module_id;
            request.plan_id = binding.plan_id;
            request.canonical_sblr = canonical_sblr;
            request.compatibility = makeCompatibilityKey(binding.object_uuid, canonical_sblr);
            request.policy.database_compile_mode = jit::JitCompileMode::JIT_ALLOWED;
            request.policy.database_execution_policy = jit::JitExecutionPolicy::PREFER_NATIVE;
            request.policy.session_compile_mode = jit::JitCompileMode::JIT_ALLOWED;
            request.policy.session_execution_policy = jit::JitExecutionPolicy::PREFER_NATIVE;
            request.policy.object_compile_mode = jit::JitCompileMode::JIT_ALLOWED;
            request.policy.object_execution_policy = jit::JitExecutionPolicy::PREFER_NATIVE;
            return request;
        }

        auto compileNativeArtifactBlob(const jit::ArtifactCompatibilityKey& key,
                                       const std::vector<uint8_t>& canonical_sblr)
            -> std::vector<uint8_t>
        {
            jit::JitCompiler compiler(jit::createLlvmBackend());
            jit::JitCompileRequest request{};
            request.key = key;
            request.canonical_sblr = canonical_sblr;
            const jit::JitCompileResult result = compiler.compile(request);
            EXPECT_TRUE(result.success) << result.diagnostic;
            EXPECT_FALSE(result.native_blob.empty());
            return result.native_blob;
        }

        auto makeReadyArtifact(const ModulePlanBinding& binding,
                               const std::vector<uint8_t>& canonical_sblr,
                               const jit::ArtifactCompatibilityKey& key,
                               core::CatalogManager::SblrArtifactState state =
                                   core::CatalogManager::SblrArtifactState::READY)
            -> jit::JitArtifact
        {
            jit::JitArtifact artifact{};
            artifact.artifact_id = core::generateUuidV7();
            artifact.module_id = binding.module_id;
            artifact.plan_id = binding.plan_id;
            artifact.binary_blob_id = core::generateUuidV7();
            artifact.compatibility = key;
            artifact.native_blob = compileNativeArtifactBlob(key, canonical_sblr);
            artifact.has_native_hash = true;
            artifact.native_hash_sha256 =
                jit::JitArtifactStore::canonicalSblrHashHex(artifact.native_blob);
            artifact.state = state;
            return artifact;
        }

        auto executeSqlWithJit(const std::string& sql,
                               const ModulePlanBinding& binding,
                               jit::JitCompileMode compile_mode,
                               jit::JitExecutionPolicy execution_policy,
                               const jit::JitHints& hints = {}) -> sblr::ExecutionResult
        {
            auto bytecode = compileSql(sql);
            EXPECT_FALSE(bytecode.empty());
            const jit::LlvmToolchainInfo& llvm_info = jit::llvmToolchainInfo();

            sblr::Executor executor(db_.get());
            executor.setConnectionContext(conn_ctx_.get());
            executor.setCurrentSchema(default_schema_id_);
            executor.setJitObjectBinding(binding.object_uuid, binding.module_id, binding.plan_id);
            executor.setJitCompatibilityProfile(llvm_info.host_target_triple,
                                                "generic",
                                                "sb_abi_v1",
                                                llvm_info.provider_identity,
                                                llvm_info.provider_version,
                                                "O2",
                                                1);
            executor.setJitPolicy(compile_mode, execution_policy);
            executor.setJitHints(hints);
            executor.setJitBackendLlvmEnabled(true);
            return executor.execute(bytecode);
        }

        std::string db_path_;
        std::unique_ptr<core::Database> db_;
        std::unique_ptr<core::ConnectionContext> conn_ctx_;
        core::ID default_schema_id_{};
        core::ID system_user_id_{};
    };
}
