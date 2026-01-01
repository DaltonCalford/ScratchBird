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
