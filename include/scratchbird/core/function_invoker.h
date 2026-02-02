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
#include <vector>
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/typed_value.h"

namespace scratchbird::core
{
    class FunctionInvoker
    {
    public:
        virtual ~FunctionInvoker() = default;

        virtual auto callFunctionByName(const std::string& function_name,
                                        const std::vector<TypedValue>& args,
                                        TypedValue& result_out,
                                        ErrorContext* ctx = nullptr) -> Status = 0;
    };
} // namespace scratchbird::core
