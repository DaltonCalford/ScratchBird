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

#include <string>
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/function_invoker.h"

namespace scratchbird::core
{
    struct ValidationConfig
    {
        std::string function_name;
        std::string error_message;
    };

    class DomainValidation
    {
    public:
        static auto validateValue(const TypedValue& value,
                                  const ValidationConfig& config,
                                  FunctionInvoker* invoker,
                                  bool& is_valid_out,
                                  ErrorContext* ctx = nullptr) -> Status;

        static void setValidationError(const ValidationConfig& config,
                                       const TypedValue& value,
                                       ErrorContext* ctx);
    };
} // namespace scratchbird::core
