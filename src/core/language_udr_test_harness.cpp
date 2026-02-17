/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/udr/language_udr_test_harness.h"

#include "scratchbird/core/vnext_error_codes.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <unordered_set>

namespace scratchbird::udr
{

    namespace
    {
        constexpr uint64_t kFnvOffsetBasis = 1469598103934665603ULL;
        constexpr uint64_t kFnvPrime = 1099511628211ULL;
        constexpr const char *kHostApiAbiVersion = "SB_HOST_API_V1";

        const std::array<const char *, 3> kCanonicalNativeTargets{
            "x86_64-apple-darwin",
            "x86_64-pc-linux-gnu",
            "x86_64-pc-windows-msvc"};

        auto hashHex64(const std::string &text, uint64_t salt) -> uint64_t
        {
            uint64_t h = kFnvOffsetBasis ^ salt;
            for (unsigned char c : text)
            {
                h ^= static_cast<uint64_t>(c);
                h *= kFnvPrime;
            }
            return h;
        }

        auto hashHex32(const std::string &text) -> std::string
        {
            const uint64_t hi = hashHex64(text, 0x9E3779B185EBCA87ULL);
            const uint64_t lo = hashHex64(text, 0xD1B54A32D192ED03ULL);
            std::ostringstream oss;
            oss << std::hex;
            oss.width(16);
            oss.fill('0');
            oss << hi;
            oss.width(16);
            oss.fill('0');
            oss << lo;
            return oss.str();
        }

        auto trimAscii(const std::string &input) -> std::string
        {
            size_t start = 0;
            while (start < input.size() &&
                   std::isspace(static_cast<unsigned char>(input[start])) != 0)
            {
                ++start;
            }
            size_t end = input.size();
            while (end > start &&
                   std::isspace(static_cast<unsigned char>(input[end - 1])) != 0)
            {
                --end;
            }
            return input.substr(start, end - start);
        }

        auto normalizePayloadText(const std::string &input) -> std::string
        {
            std::string out;
            out.reserve(input.size());
            bool in_space = false;
            for (unsigned char c : input)
            {
                if (std::isspace(c) != 0)
                {
                    if (!in_space)
                    {
                        out.push_back(' ');
                        in_space = true;
                    }
                    continue;
                }
                out.push_back(static_cast<char>(c));
                in_space = false;
            }
            return trimAscii(out);
        }

        auto isTextPayloadFormat(const std::string &payload_format) -> bool
        {
            static const std::unordered_set<std::string> text_formats{
                "SQL_TEXT", "CQL_TEXT", "CYPHER_TEXT", "JSON_DSL", "INFLUXQL_TEXT",
                "INFLUX_LINE", "VECTOR_API_JSON"};
            return text_formats.find(payload_format) != text_formats.end();
        }

        auto normalizePayload(const LanguageUdrCompileRequest &request) -> std::string
        {
            const std::string raw(request.payload.begin(), request.payload.end());
            if (!isTextPayloadFormat(request.payload_format))
            {
                return raw;
            }
            return normalizePayloadText(raw);
        }

        auto statusForCode(const std::string &vnext_code, core::Status fallback_status) -> core::Status
        {
            core::Status mapped = fallback_status;
            if (core::tryGetStatusForVNextErrorCode(vnext_code, mapped))
            {
                return mapped;
            }
            return fallback_status;
        }

        auto objectFormatForTriple(const std::string &triple) -> std::string
        {
            if (triple.find("windows") != std::string::npos)
            {
                return "COFF_OBJECT";
            }
            if (triple.find("apple") != std::string::npos)
            {
                return "MACHO_OBJECT";
            }
            return "ELF_OBJECT";
        }

        auto sortedUniqueTriples(std::vector<std::string> triples) -> std::vector<std::string>
        {
            for (std::string &triple : triples)
            {
                triple = trimAscii(triple);
            }
            triples.erase(std::remove_if(triples.begin(), triples.end(),
                                         [](const std::string &triple) {
                                             return triple.empty();
                                         }),
                          triples.end());
            std::sort(triples.begin(), triples.end());
            triples.erase(std::unique(triples.begin(), triples.end()), triples.end());
            return triples;
        }

        auto isSupportedTargetTriple(const std::string &triple) -> bool
        {
            return std::find(kCanonicalNativeTargets.begin(),
                             kCanonicalNativeTargets.end(),
                             triple) != kCanonicalNativeTargets.end();
        }

        auto appendDiagnostic(LanguageUdrCompileResponse &response,
                              const std::string &code,
                              LanguageUdrDiagnosticSeverity severity,
                              const std::string &message,
                              uint32_t line = 0,
                              uint32_t column = 0) -> void
        {
            LanguageUdrDiagnostic diag{};
            diag.code = code;
            diag.severity = severity;
            diag.message = message;
            diag.line = line;
            diag.column = column;
            response.diagnostics.push_back(std::move(diag));
            std::sort(response.diagnostics.begin(), response.diagnostics.end(),
                      [](const LanguageUdrDiagnostic &lhs,
                         const LanguageUdrDiagnostic &rhs) {
                          if (lhs.line != rhs.line)
                          {
                              return lhs.line < rhs.line;
                          }
                          if (lhs.column != rhs.column)
                          {
                              return lhs.column < rhs.column;
                          }
                          return lhs.code < rhs.code;
                      });
        }
    } // namespace

    LanguageUdrTestHarness::LanguageUdrTestHarness(const LanguageUdrRegistry &registry)
        : registry_(registry)
    {
    }

    auto LanguageUdrTestHarness::compileSingle(
        const LanguageUdrCompileRequest &request, LanguageUdrCompileResponse &response_out,
        core::ErrorContext *ctx) const -> core::Status
    {
        return compileInternal(request, nullptr, false, response_out, ctx);
    }

    auto LanguageUdrTestHarness::compileBatch(
        const std::vector<LanguageUdrCompileRequest> &requests,
        std::vector<LanguageUdrCompileResponse> &responses_out,
        core::ErrorContext *ctx) const -> core::Status
    {
        responses_out.clear();
        if (requests.empty())
        {
            SET_ERROR_CONTEXT_VNEXT(ctx, core::Status::INVALID_ARGUMENT, "UDR_1506",
                                    "Malformed batch envelope");
            return core::Status::INVALID_ARGUMENT;
        }

        responses_out.reserve(requests.size());
        for (const LanguageUdrCompileRequest &request : requests)
        {
            LanguageUdrCompileResponse response{};
            (void)compileInternal(request, nullptr, false, response, nullptr);
            responses_out.push_back(std::move(response));
        }

        // Per contract, per-item success/reject is allowed without failing the batch envelope.
        return core::Status::OK;
    }

    auto LanguageUdrTestHarness::validateOnly(
        const LanguageUdrCompileRequest &request, LanguageUdrCompileResponse &response_out,
        core::ErrorContext *ctx) const -> core::Status
    {
        return compileInternal(request, nullptr, true, response_out, ctx);
    }

    auto LanguageUdrTestHarness::compileNativeOptional(
        const LanguageUdrCompileRequest &request,
        const LanguageUdrNativeCompileOptions &options,
        LanguageUdrCompileResponse &response_out,
        core::ErrorContext *ctx) const -> core::Status
    {
        return compileInternal(request, &options, false, response_out, ctx);
    }

    auto LanguageUdrTestHarness::compileInternal(
        const LanguageUdrCompileRequest &request,
        const LanguageUdrNativeCompileOptions *native_options,
        bool validate_only,
        LanguageUdrCompileResponse &response_out,
        core::ErrorContext *ctx) const -> core::Status
    {
        response_out = LanguageUdrCompileResponse{};
        response_out.request_id = request.request_id;
        response_out.result_shape = validate_only
                                        ? LanguageUdrResultShape::UDR_RS_VALIDATE_REJECT
                                        : LanguageUdrResultShape::UDR_RS_COMPILE_REJECT;

        LanguageUdrRegistration selected_module{};
        core::ErrorContext local_ctx;
        core::Status status = LanguageUdrRuntimeBoundary::preflightCompile(
            registry_, request, selected_module, &local_ctx);
        if (status != core::Status::OK)
        {
            const std::string code = local_ctx.vnext_code.empty() ? "UDR_1506" : local_ctx.vnext_code;
            appendDiagnostic(response_out, code, LanguageUdrDiagnosticSeverity::ERROR,
                             local_ctx.message.empty() ? "Language UDR preflight reject"
                                                       : local_ctx.message);
            if (native_options != nullptr)
            {
                response_out.native_artifact_status = LanguageUdrNativeArtifactStatus::REJECTED;
            }
            if (ctx != nullptr)
            {
                const core::Status mapped = statusForCode(code, status);
                SET_ERROR_CONTEXT_VNEXT(ctx, mapped, code.c_str(), response_out.diagnostics.front().message.c_str());
            }
            return status;
        }

        response_out.success = true;
        response_out.native_feature_key =
            request.native_feature_key.empty() ? "GENERIC_FEATURE" : request.native_feature_key;

        const std::string normalized = normalizePayload(request);
        response_out.normalized_payload_hash = hashHex32(normalized);
        response_out.native_ast_hash = hashHex32(
            selected_module.module_name + "|" + request.payload_format + "|" + normalized);
        response_out.sblr_hash = hashHex32(
            response_out.native_ast_hash + "|" +
            request.profile_id + "|" +
            request.profile_version + "|" +
            response_out.native_feature_key);

        if (!validate_only)
        {
            const std::string synthetic_sblr =
                "SBLR|v3|" + response_out.sblr_hash + "|" + response_out.native_feature_key;
            response_out.sblr_payload.assign(synthetic_sblr.begin(), synthetic_sblr.end());
        }

        if (validate_only)
        {
            response_out.result_shape = LanguageUdrResultShape::UDR_RS_VALIDATE_OK;
            response_out.native_artifact_status = LanguageUdrNativeArtifactStatus::NOT_REQUESTED;
            return core::Status::OK;
        }

        response_out.result_shape = LanguageUdrResultShape::UDR_RS_COMPILE_OK;
        response_out.native_artifact_status = LanguageUdrNativeArtifactStatus::NOT_REQUESTED;

        if (native_options == nullptr)
        {
            return core::Status::OK;
        }

        if (native_options->artifact_preference == LanguageUdrArtifactPreference::SBLR_ONLY)
        {
            response_out.native_artifact_status = LanguageUdrNativeArtifactStatus::NOT_REQUESTED;
            return core::Status::OK;
        }

        const std::vector<std::string> triples = sortedUniqueTriples(native_options->target_triples);
        if (triples.empty())
        {
            const bool required =
                native_options->artifact_preference == LanguageUdrArtifactPreference::NATIVE_REQUIRED;
            if (required || !native_options->allow_interpreter_fallback)
            {
                response_out.success = false;
                response_out.result_shape = LanguageUdrResultShape::UDR_RS_NATIVE_EXECUTE_REJECT;
                response_out.sblr_payload.clear();
                response_out.native_artifact_status = LanguageUdrNativeArtifactStatus::REJECTED;
                appendDiagnostic(response_out, "UDR_1516", LanguageUdrDiagnosticSeverity::ERROR,
                                 "NATIVE_REQUIRED requested but native artifact generation is unavailable");
                if (ctx != nullptr)
                {
                    SET_ERROR_CONTEXT_VNEXT(ctx, core::Status::NOT_SUPPORTED, "UDR_1516",
                                            "NATIVE_REQUIRED requested but native artifact generation is unavailable");
                }
                return core::Status::NOT_SUPPORTED;
            }

            response_out.native_artifact_status = LanguageUdrNativeArtifactStatus::FALLBACK_SBLR_ONLY;
            response_out.fallback_reason_code = "NAT_FALLBACK_MODULE_UNAVAILABLE";
            response_out.result_shape = LanguageUdrResultShape::UDR_RS_NATIVE_FALLBACK_OK;
            appendDiagnostic(response_out, "UDR_1516", LanguageUdrDiagnosticSeverity::WARNING,
                             "Native artifact generation unavailable; using interpreter fallback");
            return core::Status::OK;
        }

        for (const std::string &triple : triples)
        {
            if (!isSupportedTargetTriple(triple))
            {
                const bool required =
                    native_options->artifact_preference == LanguageUdrArtifactPreference::NATIVE_REQUIRED;
                if (required || !native_options->allow_interpreter_fallback)
                {
                    response_out.success = false;
                    response_out.result_shape = LanguageUdrResultShape::UDR_RS_NATIVE_EXECUTE_REJECT;
                    response_out.sblr_payload.clear();
                    response_out.native_artifact_status = LanguageUdrNativeArtifactStatus::REJECTED;
                    appendDiagnostic(response_out, "UDR_1517", LanguageUdrDiagnosticSeverity::ERROR,
                                     "Requested native target triple unsupported by active profile/runtime");
                    if (ctx != nullptr)
                    {
                        SET_ERROR_CONTEXT_VNEXT(ctx, core::Status::NOT_SUPPORTED, "UDR_1517",
                                                "Requested native target triple unsupported by active profile/runtime");
                    }
                    return core::Status::NOT_SUPPORTED;
                }

                response_out.native_artifact_status = LanguageUdrNativeArtifactStatus::FALLBACK_SBLR_ONLY;
                response_out.fallback_reason_code = "NAT_FALLBACK_TARGET_UNSUPPORTED";
                response_out.result_shape = LanguageUdrResultShape::UDR_RS_NATIVE_FALLBACK_OK;
                appendDiagnostic(response_out, "UDR_1517", LanguageUdrDiagnosticSeverity::WARNING,
                                 "Native target unsupported; using interpreter fallback");
                return core::Status::OK;
            }
        }

        if (!native_options->host_api_abi_version.empty() &&
            native_options->host_api_abi_version != kHostApiAbiVersion)
        {
            const bool required =
                native_options->artifact_preference == LanguageUdrArtifactPreference::NATIVE_REQUIRED;
            if (required || !native_options->allow_interpreter_fallback)
            {
                response_out.success = false;
                response_out.result_shape = LanguageUdrResultShape::UDR_RS_NATIVE_EXECUTE_REJECT;
                response_out.sblr_payload.clear();
                response_out.native_artifact_status = LanguageUdrNativeArtifactStatus::REJECTED;
                appendDiagnostic(response_out, "UDR_1520", LanguageUdrDiagnosticSeverity::ERROR,
                                 "Native artifact host API ABI version mismatch");
                if (ctx != nullptr)
                {
                    SET_ERROR_CONTEXT_VNEXT(ctx, core::Status::NOT_SUPPORTED, "UDR_1520",
                                            "Native artifact host API ABI version mismatch");
                }
                return core::Status::NOT_SUPPORTED;
            }

            response_out.native_artifact_status = LanguageUdrNativeArtifactStatus::FALLBACK_SBLR_ONLY;
            response_out.fallback_reason_code = "NAT_FALLBACK_ABI_MISMATCH";
            response_out.result_shape = LanguageUdrResultShape::UDR_RS_NATIVE_FALLBACK_OK;
            appendDiagnostic(response_out, "UDR_1520", LanguageUdrDiagnosticSeverity::WARNING,
                             "Native ABI mismatch; using interpreter fallback");
            return core::Status::OK;
        }

        response_out.native_artifact_status = LanguageUdrNativeArtifactStatus::GENERATED;
        response_out.result_shape = LanguageUdrResultShape::UDR_RS_COMPILE_OK_NATIVE;
        response_out.native_artifacts.clear();
        response_out.native_artifacts.reserve(triples.size());

        const std::string abi = native_options->host_api_abi_version.empty()
                                    ? std::string(kHostApiAbiVersion)
                                    : native_options->host_api_abi_version;

        for (const std::string &triple : triples)
        {
            LanguageUdrNativeArtifact artifact{};
            artifact.target_triple = triple;
            artifact.object_format = objectFormatForTriple(triple);
            artifact.host_api_abi_version = abi;
            artifact.artifact_hash = hashHex32(response_out.sblr_hash + "|" + triple + "|" +
                                               native_options->optimization_level);
            const std::string payload_string = "NATIVE_OBJ|" + triple + "|" + artifact.artifact_hash;
            artifact.artifact_payload.assign(payload_string.begin(), payload_string.end());
            artifact.artifact_size_bytes = artifact.artifact_payload.size();
            const std::string signature_string = "SIG|" + artifact.artifact_hash;
            artifact.artifact_signature.assign(signature_string.begin(), signature_string.end());
            response_out.native_artifacts.push_back(std::move(artifact));
        }
        std::sort(response_out.native_artifacts.begin(), response_out.native_artifacts.end(),
                  [](const LanguageUdrNativeArtifact &lhs,
                     const LanguageUdrNativeArtifact &rhs) {
                      return lhs.target_triple < rhs.target_triple;
                  });

        return core::Status::OK;
    }

} // namespace scratchbird::udr
