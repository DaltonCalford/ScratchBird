/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/normalization.h"
#include "scratchbird/core/function_invoker.h"
#include "gtest/gtest.h"
#include <functional>
#include <unordered_map>

using namespace scratchbird::core;

namespace {
    class TestFunctionInvoker : public FunctionInvoker
    {
    public:
        using Handler = std::function<Status(const std::vector<TypedValue>&, TypedValue&, ErrorContext*)>;

        void registerHandler(const std::string& name, Handler handler)
        {
            handlers_[name] = std::move(handler);
        }

        auto callFunctionByName(const std::string& function_name,
                                const std::vector<TypedValue>& args,
                                TypedValue& result_out,
                                ErrorContext* ctx) -> Status override
        {
            auto it = handlers_.find(function_name);
            if (it == handlers_.end())
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Function not found");
                return Status::NOT_FOUND;
            }
            return it->second(args, result_out, ctx);
        }

    private:
        std::unordered_map<std::string, Handler> handlers_;
    };
} // namespace

TEST(NormalizationTest, Builtins) {
    TypedValue input = TypedValue::makeVarchar("  MiXeD Case ");
    TypedValue output;
    ErrorContext ctx;

    NormalizationConfig trim_lower;
    trim_lower.type = NormalizationType::TRIM_LOWERCASE;
    ASSERT_EQ(Normalization::applyNormalization(input, trim_lower, nullptr, output, &ctx), Status::OK);
    ASSERT_EQ(output.getVarchar(), "mixed case");

    NormalizationConfig upper;
    upper.type = NormalizationType::UPPERCASE;
    ASSERT_EQ(Normalization::applyNormalization(input, upper, nullptr, output, &ctx), Status::OK);
    ASSERT_EQ(output.getVarchar(), "  MIXED CASE ");
}

TEST(NormalizationTest, NullPassthrough) {
    TypedValue input = TypedValue::makeNull(DataType::VARCHAR);
    TypedValue output;
    ErrorContext ctx;

    NormalizationConfig lower;
    lower.type = NormalizationType::LOWERCASE;
    ASSERT_EQ(Normalization::applyNormalization(input, lower, nullptr, output, &ctx), Status::OK);
    ASSERT_TRUE(output.isNull());
}

TEST(NormalizationTest, CustomFunction) {
    TestFunctionInvoker invoker;
    invoker.registerHandler("normalize_custom",
                            [](const std::vector<TypedValue>& args, TypedValue& out, ErrorContext*) {
                                out = TypedValue::makeVarchar(args.front().toString() + "_ok");
                                return Status::OK;
                            });

    NormalizationConfig config;
    config.type = NormalizationType::CUSTOM_FUNCTION;
    config.custom_function_name = "normalize_custom";

    TypedValue input = TypedValue::makeVarchar("value");
    TypedValue output;
    ErrorContext ctx;

    ASSERT_EQ(Normalization::applyNormalization(input, config, &invoker, output, &ctx), Status::OK);
    ASSERT_EQ(output.getVarchar(), "value_ok");
}
