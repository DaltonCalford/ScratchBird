/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/domain_validation.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/types.h"
#include <algorithm>
#include <cctype>

namespace scratchbird::core
{
    namespace
    {
        auto toUpperAscii(std::string value) -> std::string
        {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            return value;
        }

        auto isFrozenDomain(const DomainInfo& domain) -> bool
        {
            const std::string dialect = toUpperAscii(domain.dialect_tag);
            const std::string compat = toUpperAscii(domain.compat_name);
            const std::string name = toUpperAscii(domain.domain_name);

            if (name == "[SB_CAS_DOM]FROZEN")
            {
                return true;
            }

            if (dialect == "CASSANDRA" &&
                (compat == "FROZEN" || compat.find("FROZEN<") == 0))
            {
                return true;
            }

            return false;
        }
    } // namespace

    auto DomainValidation::validateValue(const TypedValue& value,
                                         const ValidationConfig& config,
                                         FunctionInvoker* invoker,
                                         bool& is_valid_out,
                                         ErrorContext* ctx) -> Status
    {
        is_valid_out = true;
        if (config.function_name.empty())
        {
            return Status::OK;
        }

        if (value.isNull())
        {
            return Status::OK;
        }

        if (!invoker)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Function invoker not available for validation");
            return Status::INVALID_ARGUMENT;
        }

        std::vector<TypedValue> args{value};
        TypedValue result;
        Status status = invoker->callFunctionByName(config.function_name, args, result, ctx);
        if (status != Status::OK)
        {
            if (ctx && ctx->message.empty())
            {
                SET_ERROR_CONTEXT(ctx, status, "Validation function failed");
            }
            return status;
        }

        if (result.isNull())
        {
            is_valid_out = false;
            setValidationError(config, value, ctx);
            return Status::OK;
        }

        if (result.type() != DataType::BOOLEAN)
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH,
                              "Validation function must return BOOLEAN");
            return Status::TYPE_MISMATCH;
        }

        is_valid_out = result.getBool();
        if (!is_valid_out)
        {
            setValidationError(config, value, ctx);
        }

        return Status::OK;
    }

    auto DomainValidation::validateCollectionMutation(const DomainInfo& domain,
                                                      CollectionMutationKind mutation,
                                                      ErrorContext* ctx) -> Status
    {
        if (!isFrozenDomain(domain))
        {
            return Status::OK;
        }

        if (mutation == CollectionMutationKind::REPLACE_VALUE)
        {
            return Status::OK;
        }

        SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION,
                          "Frozen collection domains allow whole-value replacement only");
        return Status::CONSTRAINT_VIOLATION;
    }

    void DomainValidation::setValidationError(const ValidationConfig& config,
                                              const TypedValue& value,
                                              ErrorContext* ctx)
    {
        if (!ctx)
        {
            return;
        }

        if (!config.error_message.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, config.error_message.c_str());
            return;
        }

        std::string message = "Domain validation failed";
        if (!value.isNull())
        {
            message += " for value '" + value.toString() + "'";
        }
        SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, message.c_str());
    }
} // namespace scratchbird::core
