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
 * @file language_udr_sql_render_endpoint.cpp
 * @brief Trusted language UDR endpoint for SBLR-to-native-SQL rendering.
 */
#include "scratchbird/udr/language_udr_sql_render_endpoint.h"

#include "scratchbird/sblr/native_sql_renderer.h"

namespace scratchbird::udr
{

    namespace
    {
        auto isZeroUuid(const core::ID &id) -> bool
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

        auto fail(core::ErrorContext *ctx,
                  core::Status status,
                  const char *code,
                  const std::string &message) -> core::Status
        {
            SET_ERROR_CONTEXT_VNEXT(ctx, status, code, message.c_str());
            return status;
        }

        void appendDiagnostic(LanguageUdrSblrSqlRenderResponse &response,
                              const std::string &code,
                              LanguageUdrDiagnosticSeverity severity,
                              const std::string &message)
        {
            LanguageUdrDiagnostic diagnostic{};
            diagnostic.code = code;
            diagnostic.severity = severity;
            diagnostic.message = message;
            response.diagnostics.push_back(std::move(diagnostic));
        }
    } // namespace

    auto renderSblrToNativeSqlEndpoint(const LanguageUdrRegistry &registry,
                                       const LanguageUdrSblrSqlRenderRequest &request,
                                       LanguageUdrSblrSqlRenderResponse &response_out,
                                       core::ErrorContext *ctx) -> core::Status
    {
        response_out = LanguageUdrSblrSqlRenderResponse{};
        response_out.request_id = request.request_id;
        response_out.native_feature_key = request.native_feature_key;

        if (isZeroUuid(request.request_id) ||
            request.profile_id.empty() ||
            request.profile_version.empty() ||
            request.native_feature_key.empty() ||
            isZeroUuid(request.principal_id) ||
            request.role_context_signature.empty())
        {
            appendDiagnostic(response_out,
                             "UDR_1506",
                             LanguageUdrDiagnosticSeverity::ERROR,
                             "SBLR->SQL render request payload schema invalid");
            return fail(ctx,
                        core::Status::INVALID_ARGUMENT,
                        "UDR_1506",
                        "SBLR->SQL render request payload schema invalid");
        }

        if (!request.render_permission_granted)
        {
            appendDiagnostic(response_out,
                             "UDR_1507",
                             LanguageUdrDiagnosticSeverity::ERROR,
                             "Caller principal not allowed to invoke SBLR->SQL render endpoint");
            return fail(ctx,
                        core::Status::PERMISSION_DENIED,
                        "UDR_1507",
                        "Caller principal not allowed to invoke SBLR->SQL render endpoint");
        }

        LanguageUdrRegistration selected{};
        core::ErrorContext preflight_ctx;
        core::Status status = registry.resolveActiveModule(
            request.profile_id, request.profile_version, selected, &preflight_ctx);
        if (status != core::Status::OK)
        {
            const std::string code =
                preflight_ctx.vnext_code.empty() ? std::string("UDR_1501") : preflight_ctx.vnext_code;
            const std::string message =
                preflight_ctx.message.empty()
                    ? std::string("Failed to resolve profile module for SBLR->SQL render endpoint")
                    : preflight_ctx.message;
            appendDiagnostic(response_out, code, LanguageUdrDiagnosticSeverity::ERROR, message);
            return fail(ctx, status, code.c_str(), message);
        }

        status = registry.ensureFeatureEnabled(
            request.profile_id, request.profile_version, request.native_feature_key, &preflight_ctx);
        if (status != core::Status::OK)
        {
            const std::string code =
                preflight_ctx.vnext_code.empty() ? std::string("UDR_1504") : preflight_ctx.vnext_code;
            const std::string message =
                preflight_ctx.message.empty()
                    ? std::string("Feature key disabled for SBLR->SQL render endpoint")
                    : preflight_ctx.message;
            appendDiagnostic(response_out, code, LanguageUdrDiagnosticSeverity::ERROR, message);
            return fail(ctx, status, code.c_str(), message);
        }

        scratchbird::sblr::NativeSqlRenderResult render_result{};
        std::string render_error;
        if (!scratchbird::sblr::renderNativeSqlInstruction(
                request.root_instruction, render_result, render_error) ||
            render_result.sql.empty())
        {
            const std::string message =
                render_error.empty()
                    ? std::string("UDR produced unverifiable SBLR artifact")
                    : render_error;
            appendDiagnostic(response_out, "UDR_1510", LanguageUdrDiagnosticSeverity::ERROR, message);
            return fail(ctx, core::Status::SYNTAX_ERROR, "UDR_1510", message);
        }

        response_out.success = true;
        response_out.sql_text = render_result.sql;
        response_out.contract_id = render_result.contract_id;
        response_out.canonical_opcode_symbol = render_result.canonical_opcode_symbol;
        return core::Status::OK;
    }

} // namespace scratchbird::udr
