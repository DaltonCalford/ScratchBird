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

#include <map>
#include <string>
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/function_invoker.h"

namespace scratchbird::core
{
    struct QualityConfig
    {
        std::string parse_function;
        std::string standardize_function;
        std::string enrich_function;
    };

    struct QualityResult
    {
        TypedValue parsed_value;
        TypedValue standardized_value;
        TypedValue enriched_value;
        std::map<std::string, TypedValue> metadata;
    };

    class QualityPipeline
    {
    public:
        static auto executePipeline(const TypedValue& value,
                                    const QualityConfig& config,
                                    FunctionInvoker* invoker,
                                    QualityResult& result_out,
                                    ErrorContext* ctx = nullptr) -> Status;

    private:
        static auto executeParse(const TypedValue& value,
                                 const std::string& function_name,
                                 FunctionInvoker* invoker,
                                 TypedValue& parsed_out,
                                 ErrorContext* ctx) -> Status;

        static auto executeStandardize(const TypedValue& value,
                                       const std::string& function_name,
                                       FunctionInvoker* invoker,
                                       TypedValue& standardized_out,
                                       ErrorContext* ctx) -> Status;

        static auto executeEnrich(const TypedValue& value,
                                  const std::string& function_name,
                                  FunctionInvoker* invoker,
                                  TypedValue& enriched_out,
                                  std::map<std::string, TypedValue>& metadata_out,
                                  ErrorContext* ctx) -> Status;
    };
} // namespace scratchbird::core
