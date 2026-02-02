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

TEST(DomainValidationTest, SkipWhenNoFunction) {
    ValidationConfig config;
    TypedValue input = TypedValue::makeVarchar("abc");
    bool is_valid = false;
    ErrorContext ctx;

    ASSERT_EQ(DomainValidation::validateValue(input, config, nullptr, is_valid, &ctx), Status::OK);
    ASSERT_TRUE(is_valid);
}

TEST(DomainValidationTest, CustomValidationFailure) {
    TestFunctionInvoker invoker;
    invoker.registerHandler("is_valid",
                            [](const std::vector<TypedValue>&, TypedValue& out, ErrorContext*) {
                                out = TypedValue::makeBool(false);
                                return Status::OK;
                            });

    ValidationConfig config;
    config.function_name = "is_valid";
    config.error_message = "Invalid value";

    TypedValue input = TypedValue::makeVarchar("bad");
    bool is_valid = true;
    ErrorContext ctx;

    ASSERT_EQ(DomainValidation::validateValue(input, config, &invoker, is_valid, &ctx), Status::OK);
    ASSERT_FALSE(is_valid);
    ASSERT_EQ(ctx.message, "Invalid value");
}

TEST(DomainValidationTest, NullValueSkipsValidation) {
    TestFunctionInvoker invoker;
    bool called = false;
    invoker.registerHandler("is_valid",
                            [&called](const std::vector<TypedValue>&, TypedValue& out, ErrorContext*) {
                                called = true;
                                out = TypedValue::makeBool(true);
                                return Status::OK;
                            });

    ValidationConfig config;
    config.function_name = "is_valid";

    TypedValue input = TypedValue::makeNull(DataType::VARCHAR);
    bool is_valid = false;
    ErrorContext ctx;

    ASSERT_EQ(DomainValidation::validateValue(input, config, &invoker, is_valid, &ctx), Status::OK);
    ASSERT_TRUE(is_valid);
    ASSERT_FALSE(called);
}

TEST(DomainValidationTest, ValidatesFunctionSignatureViaInvoker) {
    TestFunctionInvoker invoker;
    size_t arg_count = 0;
    invoker.registerHandler("is_valid",
                            [&arg_count](const std::vector<TypedValue>& args, TypedValue& out, ErrorContext*) {
                                arg_count = args.size();
                                out = TypedValue::makeBool(true);
                                return Status::OK;
                            });

    ValidationConfig config;
    config.function_name = "is_valid";

    TypedValue input = TypedValue::makeVarchar("ok");
    bool is_valid = false;
    ErrorContext ctx;

    ASSERT_EQ(DomainValidation::validateValue(input, config, &invoker, is_valid, &ctx), Status::OK);
    ASSERT_EQ(arg_count, 1u);
}

TEST(DomainValidationTest, CustomValidationTypeMismatch) {
    TestFunctionInvoker invoker;
    invoker.registerHandler("returns_text",
                            [](const std::vector<TypedValue>&, TypedValue& out, ErrorContext*) {
                                out = TypedValue::makeVarchar("nope");
                                return Status::OK;
                            });

    ValidationConfig config;
    config.function_name = "returns_text";

    TypedValue input = TypedValue::makeVarchar("bad");
    bool is_valid = true;
    ErrorContext ctx;

    ASSERT_EQ(DomainValidation::validateValue(input, config, &invoker, is_valid, &ctx), Status::TYPE_MISMATCH);
}
