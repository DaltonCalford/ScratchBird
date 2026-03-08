/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * @file jit_backend_llvm.cpp
 * @brief LLVM-linked provider backend for section-23 artifact generation.
 *
 * NCW-010 and NCW-011 close toolchain detection, provider build integration,
 * persisted artifact emission, and runtime envelope verification. Direct
 * lowering-to-callable-native execution remains a later closure step.
 */
#include "scratchbird/sblr/jit/jit_compiler.h"

#include "scratchbird/sblr/jit/jit_llvm_toolchain.h"

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/sblr/jit/jit_artifact_store.h"

#if defined(SCRATCHBIRD_HAVE_LLVM_JIT) && SCRATCHBIRD_HAVE_LLVM_JIT
#include <llvm/ADT/SmallVector.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/Alignment.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/TargetParser/Triple.h>
#endif

namespace scratchbird::sblr::jit
{
    namespace
    {
        class LlvmBackendProvider final : public JitBackend
        {
        public:
            auto backendName() const -> const char* override
            {
                return "llvm";
            }

            auto compile(const LoweredRoutine& lowered,
                         const JitCompileRequest& request) -> JitCompileResult override
            {
                JitCompileResult out;
                const LlvmToolchainInfo& toolchain = llvmToolchainInfo();
                if (!toolchain.available)
                {
                    out.success = false;
                    out.reason = JitReasonCode::BACKEND_UNAVAILABLE;
                    out.diagnostic =
                        "LLVM provider backend is not available in this build";
                    return out;
                }

                if (lowered.lowered_ir.empty())
                {
                    out.success = false;
                    out.reason = JitReasonCode::BACKEND_COMPILE_FAILED;
                    out.diagnostic = "lowered IR is empty";
                    return out;
                }

#if defined(SCRATCHBIRD_HAVE_LLVM_JIT) && SCRATCHBIRD_HAVE_LLVM_JIT
                const std::string target_upper =
                    core::IdentifierUtils::toUpper(request.key.target_triple);
                if (target_upper.find("FAIL") != std::string::npos)
                {
                    out.success = false;
                    out.reason = JitReasonCode::BACKEND_COMPILE_FAILED;
                    out.diagnostic =
                        "LLVM provider fault-injection rejected target triple";
                    return out;
                }

                const std::string normalized_target =
                    normalizeLlvmTargetTriple(request.key.target_triple);
                llvm::Triple triple(normalized_target);
                if (triple.getArch() == llvm::Triple::UnknownArch)
                {
                    out.success = false;
                    out.reason = JitReasonCode::BACKEND_COMPILE_FAILED;
                    out.diagnostic = "LLVM provider rejected unknown target triple";
                    return out;
                }
                if (request.key.compiler_identity != toolchain.provider_identity ||
                    request.key.compiler_version != toolchain.provider_version)
                {
                    out.success = false;
                    out.reason = JitReasonCode::BACKEND_COMPILE_FAILED;
                    out.diagnostic =
                        "compile request compiler metadata does not match LLVM provider";
                    return out;
                }

                llvm::LLVMContext context;
                llvm::Module module("scratchbird_jit_provider", context);
                module.setTargetTriple(llvm::Triple(normalized_target));
                module.setSourceFileName("scratchbird_jit_provider");

                const auto addStringGlobal =
                    [&module, &context](const char* name, const std::string& value) {
                        llvm::Constant* payload =
                            llvm::ConstantDataArray::getString(context, value, true);
                        auto* global = new llvm::GlobalVariable(
                            module,
                            payload->getType(),
                            true,
                            llvm::GlobalValue::PrivateLinkage,
                            payload,
                            name);
                        global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
                        global->setAlignment(llvm::Align(1));
                    };

                addStringGlobal("__sb_provider_identity", toolchain.provider_identity);
                addStringGlobal("__sb_provider_version", toolchain.provider_version);
                addStringGlobal("__sb_target_triple", normalized_target);
                addStringGlobal("__sb_native_abi", request.key.native_abi_version);
                addStringGlobal("__sb_opt_profile", request.key.optimization_profile);

                std::vector<llvm::Constant*> lowered_constants;
                lowered_constants.reserve(lowered.lowered_ir.size());
                llvm::Type* byte_type = llvm::Type::getInt8Ty(context);
                for (uint8_t byte : lowered.lowered_ir)
                {
                    lowered_constants.push_back(
                        llvm::ConstantInt::get(byte_type, static_cast<uint64_t>(byte)));
                }
                llvm::ArrayType* lowered_type =
                    llvm::ArrayType::get(byte_type, lowered_constants.size());
                llvm::Constant* lowered_payload =
                    llvm::ConstantArray::get(lowered_type, lowered_constants);
                auto* lowered_global = new llvm::GlobalVariable(
                    module,
                    lowered_type,
                    true,
                    llvm::GlobalValue::PrivateLinkage,
                    lowered_payload,
                    "__sb_lowered_ir");
                lowered_global->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
                lowered_global->setAlignment(llvm::Align(1));

                llvm::SmallVector<char, 0> bitcode_buffer;
                llvm::raw_svector_ostream stream(bitcode_buffer);
                llvm::WriteBitcodeToFile(module, stream);
                out.native_blob.assign(bitcode_buffer.begin(), bitcode_buffer.end());
#else
                out.native_blob = lowered.lowered_ir;
#endif
                out.native_blob_hash_sha256 =
                    JitArtifactStore::canonicalSblrHashHex(out.native_blob);
                out.success = true;
                out.reason = JitReasonCode::NONE;
                out.diagnostic =
                    "LLVM provider emitted deterministic bitcode artifact";
                return out;
            }
        };
    }

    auto createLlvmBackend() -> std::unique_ptr<JitBackend>
    {
        return std::make_unique<LlvmBackendProvider>();
    }
}
