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

#include <cstdint>
#include <string>
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/function_invoker.h"

namespace scratchbird::core
{
    enum class NormalizationType : uint8_t
    {
        NONE = 0,
        LOWERCASE = 1,
        UPPERCASE = 2,
        TRIM = 3,
        TRIM_LOWERCASE = 4,
        TRIM_UPPERCASE = 5,
        CUSTOM_FUNCTION = 99
    };

    struct NormalizationConfig
    {
        NormalizationType type = NormalizationType::NONE;
        std::string custom_function_name;
    };

    class Normalization
    {
    public:
        static auto resolveConfig(const std::string& function_name) -> NormalizationConfig;

        static auto applyNormalization(const TypedValue& value,
                                       const NormalizationConfig& config,
                                       FunctionInvoker* invoker,
                                       TypedValue& normalized_out,
                                       ErrorContext* ctx = nullptr) -> Status;

    private:
        static auto applyLowercase(const TypedValue& value,
                                   TypedValue& normalized_out,
                                   ErrorContext* ctx) -> Status;

        static auto applyUppercase(const TypedValue& value,
                                   TypedValue& normalized_out,
                                   ErrorContext* ctx) -> Status;

        static auto applyTrim(const TypedValue& value,
                              TypedValue& normalized_out,
                              ErrorContext* ctx) -> Status;

        static auto applyTrimLowercase(const TypedValue& value,
                                       TypedValue& normalized_out,
                                       ErrorContext* ctx) -> Status;

        static auto applyTrimUppercase(const TypedValue& value,
                                       TypedValue& normalized_out,
                                       ErrorContext* ctx) -> Status;

        static auto applyCustomFunction(const TypedValue& value,
                                        const std::string& function_name,
                                        FunctionInvoker* invoker,
                                        TypedValue& normalized_out,
                                        ErrorContext* ctx) -> Status;
    };
} // namespace scratchbird::core
