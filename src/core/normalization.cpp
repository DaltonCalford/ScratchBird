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
#include "scratchbird/core/charset.h"
#include <algorithm>
#include <cctype>

namespace scratchbird::core
{
    namespace {
        bool isStringType(DataType type)
        {
            return type == DataType::VARCHAR ||
                   type == DataType::TEXT ||
                   type == DataType::CHAR;
        }

        std::string toUpperAscii(const std::string& input)
        {
            std::string out;
            out.reserve(input.size());
            for (char ch : input)
            {
                out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
            }
            return out;
        }

        std::string trimAscii(const std::string& input)
        {
            size_t start = 0;
            while (start < input.size() &&
                   std::isspace(static_cast<unsigned char>(input[start])))
            {
                ++start;
            }

            size_t end = input.size();
            while (end > start &&
                   std::isspace(static_cast<unsigned char>(input[end - 1])))
            {
                --end;
            }

            return input.substr(start, end - start);
        }

        std::string getStringValue(const TypedValue& value)
        {
            switch (value.type())
            {
                case DataType::VARCHAR:
                    return value.getVarchar();
                case DataType::TEXT:
                    return value.getText();
                case DataType::CHAR:
                    return value.getChar();
                default:
                    return value.toString();
            }
        }

        TypedValue makeStringValue(DataType type, const std::string& text)
        {
            switch (type)
            {
                case DataType::VARCHAR:
                    return TypedValue::makeVarchar(text);
                case DataType::TEXT:
                    return TypedValue::makeText(text);
                case DataType::CHAR:
                    return TypedValue::makeChar(text);
                default:
                    return TypedValue::makeText(text);
            }
        }
    } // namespace

    auto Normalization::resolveConfig(const std::string& function_name) -> NormalizationConfig
    {
        NormalizationConfig config;
        if (function_name.empty())
        {
            return config;
        }

        std::string normalized = toUpperAscii(function_name);
        if (normalized == "LOWERCASE" || normalized == "LOWER")
        {
            config.type = NormalizationType::LOWERCASE;
            return config;
        }
        if (normalized == "UPPERCASE" || normalized == "UPPER")
        {
            config.type = NormalizationType::UPPERCASE;
            return config;
        }
        if (normalized == "TRIM")
        {
            config.type = NormalizationType::TRIM;
            return config;
        }
        if (normalized == "TRIM_LOWERCASE" || normalized == "TRIMLOWERCASE")
        {
            config.type = NormalizationType::TRIM_LOWERCASE;
            return config;
        }
        if (normalized == "TRIM_UPPERCASE" || normalized == "TRIMUPPERCASE")
        {
            config.type = NormalizationType::TRIM_UPPERCASE;
            return config;
        }

        config.type = NormalizationType::CUSTOM_FUNCTION;
        config.custom_function_name = function_name;
        return config;
    }

    auto Normalization::applyNormalization(const TypedValue& value,
                                           const NormalizationConfig& config,
                                           FunctionInvoker* invoker,
                                           TypedValue& normalized_out,
                                           ErrorContext* ctx) -> Status
    {
        if (config.type == NormalizationType::NONE || value.isNull())
        {
            normalized_out = value;
            return Status::OK;
        }

        switch (config.type)
        {
            case NormalizationType::LOWERCASE:
                return applyLowercase(value, normalized_out, ctx);
            case NormalizationType::UPPERCASE:
                return applyUppercase(value, normalized_out, ctx);
            case NormalizationType::TRIM:
                return applyTrim(value, normalized_out, ctx);
            case NormalizationType::TRIM_LOWERCASE:
                return applyTrimLowercase(value, normalized_out, ctx);
            case NormalizationType::TRIM_UPPERCASE:
                return applyTrimUppercase(value, normalized_out, ctx);
            case NormalizationType::CUSTOM_FUNCTION:
                return applyCustomFunction(value, config.custom_function_name,
                                           invoker, normalized_out, ctx);
            case NormalizationType::NONE:
                normalized_out = value;
                return Status::OK;
        }

        normalized_out = value;
        return Status::OK;
    }

    auto Normalization::applyLowercase(const TypedValue& value,
                                       TypedValue& normalized_out,
                                       ErrorContext* ctx) -> Status
    {
        if (!isStringType(value.type()))
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH,
                              "Normalization LOWERCASE expects a string value");
            return Status::TYPE_MISMATCH;
        }

        std::string str = getStringValue(value);
        normalized_out = makeStringValue(value.type(), utf8::to_lower(str));
        return Status::OK;
    }

    auto Normalization::applyUppercase(const TypedValue& value,
                                       TypedValue& normalized_out,
                                       ErrorContext* ctx) -> Status
    {
        if (!isStringType(value.type()))
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH,
                              "Normalization UPPERCASE expects a string value");
            return Status::TYPE_MISMATCH;
        }

        std::string str = getStringValue(value);
        normalized_out = makeStringValue(value.type(), utf8::to_upper(str));
        return Status::OK;
    }

    auto Normalization::applyTrim(const TypedValue& value,
                                  TypedValue& normalized_out,
                                  ErrorContext* ctx) -> Status
    {
        if (!isStringType(value.type()))
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH,
                              "Normalization TRIM expects a string value");
            return Status::TYPE_MISMATCH;
        }

        std::string str = getStringValue(value);
        normalized_out = makeStringValue(value.type(), trimAscii(str));
        return Status::OK;
    }

    auto Normalization::applyTrimLowercase(const TypedValue& value,
                                           TypedValue& normalized_out,
                                           ErrorContext* ctx) -> Status
    {
        if (!isStringType(value.type()))
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH,
                              "Normalization TRIM_LOWERCASE expects a string value");
            return Status::TYPE_MISMATCH;
        }

        std::string str = trimAscii(getStringValue(value));
        normalized_out = makeStringValue(value.type(), utf8::to_lower(str));
        return Status::OK;
    }

    auto Normalization::applyTrimUppercase(const TypedValue& value,
                                           TypedValue& normalized_out,
                                           ErrorContext* ctx) -> Status
    {
        if (!isStringType(value.type()))
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH,
                              "Normalization TRIM_UPPERCASE expects a string value");
            return Status::TYPE_MISMATCH;
        }

        std::string str = trimAscii(getStringValue(value));
        normalized_out = makeStringValue(value.type(), utf8::to_upper(str));
        return Status::OK;
    }

    auto Normalization::applyCustomFunction(const TypedValue& value,
                                            const std::string& function_name,
                                            FunctionInvoker* invoker,
                                            TypedValue& normalized_out,
                                            ErrorContext* ctx) -> Status
    {
        if (!invoker)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Function invoker not available for custom normalization");
            return Status::INVALID_ARGUMENT;
        }

        std::vector<TypedValue> args{value};
        TypedValue result;
        Status status = invoker->callFunctionByName(function_name, args, result, ctx);
        if (status != Status::OK)
        {
            if (ctx && ctx->message.empty())
            {
                SET_ERROR_CONTEXT(ctx, status, "Custom normalization failed");
            }
            return status;
        }

        if (result.isNull())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Custom normalization returned NULL");
            return Status::INVALID_ARGUMENT;
        }

        if (result.type() != value.type())
        {
            SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH,
                              "Custom normalization return type mismatch");
            return Status::TYPE_MISMATCH;
        }

        normalized_out = result;
        return Status::OK;
    }
} // namespace scratchbird::core
