/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/sblr/jit/jit_llvm_artifact.h"

#include "scratchbird/sblr/jit/jit_llvm_toolchain.h"

#if defined(SCRATCHBIRD_HAVE_LLVM_JIT) && SCRATCHBIRD_HAVE_LLVM_JIT
#include <llvm/ADT/StringRef.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/MemoryBufferRef.h>
#endif

namespace scratchbird::sblr::jit
{
    namespace
    {
#if defined(SCRATCHBIRD_HAVE_LLVM_JIT) && SCRATCHBIRD_HAVE_LLVM_JIT
        auto extractStringGlobal(const llvm::Module& module,
                                 const char* name,
                                 std::string& value_out) -> bool
        {
            const llvm::GlobalVariable* global = module.getNamedGlobal(name);
            if (global == nullptr || !global->hasInitializer())
            {
                return false;
            }

            const auto* constant_data =
                llvm::dyn_cast<llvm::ConstantDataSequential>(global->getInitializer());
            if (constant_data == nullptr || !constant_data->isCString())
            {
                return false;
            }

            value_out = constant_data->getAsCString().str();
            return true;
        }

        auto loweredIrSize(const llvm::Module& module) -> size_t
        {
            const llvm::GlobalVariable* global = module.getNamedGlobal("__sb_lowered_ir");
            if (global == nullptr)
            {
                return 0;
            }

            const auto* array_type = llvm::dyn_cast<llvm::ArrayType>(global->getValueType());
            if (array_type == nullptr)
            {
                return 0;
            }
            return static_cast<size_t>(array_type->getNumElements());
        }
#endif
    }

    auto verifyLlvmArtifactEnvelope(const std::vector<uint8_t>& native_blob,
                                    const ArtifactCompatibilityKey& key,
                                    std::string& detail_out) -> bool
    {
        detail_out.clear();
        if (native_blob.empty())
        {
            detail_out = "artifact payload is empty";
            return false;
        }

#if defined(SCRATCHBIRD_HAVE_LLVM_JIT) && SCRATCHBIRD_HAVE_LLVM_JIT
        llvm::LLVMContext context;
        const llvm::StringRef blob_ref(reinterpret_cast<const char*>(native_blob.data()),
                                       native_blob.size());
        const llvm::MemoryBufferRef buffer_ref(blob_ref, "scratchbird_jit_artifact");
        auto module_or_error = llvm::parseBitcodeFile(buffer_ref, context);
        if (!module_or_error)
        {
            detail_out = "artifact payload is not valid LLVM bitcode";
            return false;
        }

        const std::unique_ptr<llvm::Module>& module = *module_or_error;

        std::string provider_identity;
        std::string provider_version;
        std::string target_triple;
        std::string native_abi;
        std::string optimization_profile;
        if (!extractStringGlobal(*module, "__sb_provider_identity", provider_identity) ||
            !extractStringGlobal(*module, "__sb_provider_version", provider_version) ||
            !extractStringGlobal(*module, "__sb_target_triple", target_triple) ||
            !extractStringGlobal(*module, "__sb_native_abi", native_abi) ||
            !extractStringGlobal(*module, "__sb_opt_profile", optimization_profile))
        {
            detail_out = "artifact payload is missing required LLVM metadata globals";
            return false;
        }

        if (provider_identity != key.compiler_identity)
        {
            detail_out = "artifact provider identity does not match compatibility key";
            return false;
        }
        if (provider_version != key.compiler_version)
        {
            detail_out = "artifact provider version does not match compatibility key";
            return false;
        }
        if (normalizeLlvmTargetTriple(target_triple) != normalizeLlvmTargetTriple(key.target_triple))
        {
            detail_out = "artifact target triple does not match compatibility key";
            return false;
        }
        if (native_abi != key.native_abi_version)
        {
            detail_out = "artifact native ABI does not match compatibility key";
            return false;
        }
        if (optimization_profile != key.optimization_profile)
        {
            detail_out = "artifact optimization profile does not match compatibility key";
            return false;
        }
        if (loweredIrSize(*module) == 0)
        {
            detail_out = "artifact payload is missing lowered IR contents";
            return false;
        }

        return true;
#else
        (void)key;
        detail_out = "LLVM artifact verification unavailable in this build";
        return false;
#endif
    }
}
