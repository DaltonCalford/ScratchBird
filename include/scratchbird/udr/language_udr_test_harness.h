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

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"
#include "scratchbird/udr/language_udr_runtime.h"

#include <cstdint>
#include <string>
#include <vector>

namespace scratchbird::udr
{

    enum class LanguageUdrDiagnosticSeverity : uint8_t
    {
        ERROR = 0,
        WARNING = 1,
        INFO = 2
    };

    enum class LanguageUdrArtifactPreference : uint8_t
    {
        SBLR_ONLY = 0,
        NATIVE_PREFERRED = 1,
        NATIVE_REQUIRED = 2
    };

    enum class LanguageUdrNativeArtifactStatus : uint8_t
    {
        NOT_REQUESTED = 0,
        GENERATED = 1,
        FALLBACK_SBLR_ONLY = 2,
        REJECTED = 3
    };

    enum class LanguageUdrResultShape : uint8_t
    {
        UDR_RS_COMPILE_OK = 0,
        UDR_RS_COMPILE_OK_NATIVE = 1,
        UDR_RS_COMPILE_REJECT = 2,
        UDR_RS_VALIDATE_OK = 3,
        UDR_RS_VALIDATE_REJECT = 4,
        UDR_RS_NATIVE_EXECUTE_OK = 5,
        UDR_RS_NATIVE_FALLBACK_OK = 6,
        UDR_RS_NATIVE_EXECUTE_REJECT = 7
    };

    struct LanguageUdrDiagnostic
    {
        std::string code;
        LanguageUdrDiagnosticSeverity severity = LanguageUdrDiagnosticSeverity::INFO;
        std::string message;
        uint32_t line = 0;
        uint32_t column = 0;
    };

    struct LanguageUdrNativeArtifact
    {
        std::string target_triple;
        std::string object_format;
        std::string host_api_abi_version;
        std::string artifact_hash;
        uint64_t artifact_size_bytes = 0;
        std::vector<uint8_t> artifact_signature;
        std::vector<uint8_t> artifact_payload;
    };

    struct LanguageUdrNativeCompileOptions
    {
        LanguageUdrArtifactPreference artifact_preference =
            LanguageUdrArtifactPreference::SBLR_ONLY;
        std::vector<std::string> target_triples;
        std::string host_api_abi_version;
        std::string optimization_level = "O2";
        bool allow_interpreter_fallback = true;
    };

    struct LanguageUdrCompileResponse
    {
        core::ID request_id;
        bool success = false;
        LanguageUdrResultShape result_shape = LanguageUdrResultShape::UDR_RS_COMPILE_REJECT;

        std::string native_feature_key;
        std::string normalized_payload_hash;
        std::string native_ast_hash;
        std::string sblr_hash;
        std::vector<uint8_t> sblr_payload;
        std::vector<LanguageUdrDiagnostic> diagnostics;

        LanguageUdrNativeArtifactStatus native_artifact_status =
            LanguageUdrNativeArtifactStatus::NOT_REQUESTED;
        std::vector<LanguageUdrNativeArtifact> native_artifacts;
        std::string fallback_reason_code;
    };

    class LanguageUdrTestHarness
    {
    public:
        explicit LanguageUdrTestHarness(const LanguageUdrRegistry &registry);

        auto compileSingle(const LanguageUdrCompileRequest &request,
                           LanguageUdrCompileResponse &response_out,
                           core::ErrorContext *ctx = nullptr) const -> core::Status;

        auto compileBatch(const std::vector<LanguageUdrCompileRequest> &requests,
                          std::vector<LanguageUdrCompileResponse> &responses_out,
                          core::ErrorContext *ctx = nullptr) const -> core::Status;

        auto validateOnly(const LanguageUdrCompileRequest &request,
                          LanguageUdrCompileResponse &response_out,
                          core::ErrorContext *ctx = nullptr) const -> core::Status;

        auto compileNativeOptional(const LanguageUdrCompileRequest &request,
                                   const LanguageUdrNativeCompileOptions &options,
                                   LanguageUdrCompileResponse &response_out,
                                   core::ErrorContext *ctx = nullptr) const -> core::Status;

    private:
        const LanguageUdrRegistry &registry_;

        auto compileInternal(const LanguageUdrCompileRequest &request,
                             const LanguageUdrNativeCompileOptions *native_options,
                             bool validate_only,
                             LanguageUdrCompileResponse &response_out,
                             core::ErrorContext *ctx) const -> core::Status;
    };

} // namespace scratchbird::udr

