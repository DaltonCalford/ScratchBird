/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/quality_pipeline.h"
#include <nlohmann/json.hpp>

namespace scratchbird::core
{
    namespace {
        using json = nlohmann::json;

        auto jsonToTypedValue(const json& value) -> TypedValue
        {
            if (value.is_null())
            {
                return TypedValue::makeNull();
            }
            if (value.is_boolean())
            {
                return TypedValue::makeBool(value.get<bool>());
            }
            if (value.is_number_integer())
            {
                return TypedValue::makeInt64(value.get<int64_t>());
            }
            if (value.is_number_unsigned())
            {
                return TypedValue::makeUInt64(value.get<uint64_t>());
            }
            if (value.is_number_float())
            {
                return TypedValue::makeFloat64(value.get<double>());
            }
            if (value.is_string())
            {
                return TypedValue::makeText(value.get<std::string>());
            }

            return TypedValue::makeText(value.dump());
        }

        auto jsonToTypedValueForType(const json& value, DataType expected_type,
                                     ErrorContext* ctx) -> TypedValue
        {
            if (value.is_null())
            {
                return TypedValue::makeNull(expected_type);
            }

            switch (expected_type)
            {
                case DataType::VARCHAR:
                    return TypedValue::makeVarchar(value.is_string()
                        ? value.get<std::string>()
                        : value.dump());
                case DataType::TEXT:
                    return TypedValue::makeText(value.is_string()
                        ? value.get<std::string>()
                        : value.dump());
                case DataType::CHAR:
                    return TypedValue::makeChar(value.is_string()
                        ? value.get<std::string>()
                        : value.dump());
                case DataType::JSON:
                    return TypedValue::makeJSON(value.is_string()
                        ? value.get<std::string>()
                        : value.dump());
                case DataType::JSONB:
                    return TypedValue::makeText(value.is_string()
                        ? value.get<std::string>()
                        : value.dump());
                case DataType::BOOLEAN:
                    if (value.is_boolean())
                    {
                        return TypedValue::makeBool(value.get<bool>());
                    }
                    if (value.is_number())
                    {
                        return TypedValue::makeBool(value.get<double>() != 0.0);
                    }
                    break;
                case DataType::INT32:
                    if (value.is_number_integer())
                    {
                        return TypedValue::makeInt32(static_cast<int32_t>(value.get<int64_t>()));
                    }
                    break;
                case DataType::INT64:
                    if (value.is_number_integer())
                    {
                        return TypedValue::makeInt64(value.get<int64_t>());
                    }
                    break;
                case DataType::FLOAT64:
                case DataType::DECIMAL:
                case DataType::DECFLOAT16:
                case DataType::DECFLOAT34:
                    if (value.is_number())
                    {
                        return TypedValue::makeFloat64(value.get<double>());
                    }
                    break;
                default:
                    break;
            }

            if (ctx)
            {
                SET_ERROR_CONTEXT(ctx, Status::TYPE_MISMATCH,
                                  "Quality pipeline metadata value type mismatch");
            }
            return TypedValue::makeNull(expected_type);
        }

        auto extractMetadataFromJson(const json& doc,
                                     DataType expected_type,
                                     TypedValue& enriched_out,
                                     std::map<std::string, TypedValue>& metadata_out,
                                     ErrorContext* ctx) -> Status
        {
            if (!doc.is_object())
            {
                return Status::OK;
            }

            if (doc.contains("value"))
            {
                enriched_out = jsonToTypedValueForType(doc.at("value"), expected_type, ctx);
                if (enriched_out.isNull())
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "Quality pipeline JSON value is NULL");
                    return Status::INVALID_ARGUMENT;
                }
            }

            if (doc.contains("metadata") && doc.at("metadata").is_object())
            {
                for (const auto& item : doc.at("metadata").items())
                {
                    metadata_out[item.key()] = jsonToTypedValue(item.value());
                }
            }

            return Status::OK;
        }
    } // namespace

    auto QualityPipeline::executePipeline(const TypedValue& value,
                                          const QualityConfig& config,
                                          FunctionInvoker* invoker,
                                          QualityResult& result_out,
                                          ErrorContext* ctx) -> Status
    {
        result_out.metadata.clear();
        result_out.parsed_value = value;
        result_out.standardized_value = value;
        result_out.enriched_value = value;

        if (value.isNull())
        {
            return Status::OK;
        }

        TypedValue parsed;
        Status status = executeParse(value, config.parse_function, invoker, parsed, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        result_out.parsed_value = parsed;

        TypedValue standardized;
        status = executeStandardize(parsed, config.standardize_function, invoker, standardized, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        result_out.standardized_value = standardized;

        TypedValue enriched;
        status = executeEnrich(standardized, config.enrich_function, invoker,
                               enriched, result_out.metadata, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        result_out.enriched_value = enriched;

        return Status::OK;
    }

    auto QualityPipeline::executeParse(const TypedValue& value,
                                       const std::string& function_name,
                                       FunctionInvoker* invoker,
                                       TypedValue& parsed_out,
                                       ErrorContext* ctx) -> Status
    {
        parsed_out = value;
        if (function_name.empty())
        {
            return Status::OK;
        }
        if (!invoker)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Function invoker not available for parse stage");
            return Status::INVALID_ARGUMENT;
        }

        std::vector<TypedValue> args{value};
        Status status = invoker->callFunctionByName(function_name, args, parsed_out, ctx);
        if (status != Status::OK)
        {
            if (ctx && ctx->message.empty())
            {
                SET_ERROR_CONTEXT(ctx, status, "Quality parse stage failed");
            }
            return status;
        }

        if (parsed_out.isNull())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Quality parse stage returned NULL");
            return Status::INVALID_ARGUMENT;
        }

        return Status::OK;
    }

    auto QualityPipeline::executeStandardize(const TypedValue& value,
                                             const std::string& function_name,
                                             FunctionInvoker* invoker,
                                             TypedValue& standardized_out,
                                             ErrorContext* ctx) -> Status
    {
        standardized_out = value;
        if (function_name.empty())
        {
            return Status::OK;
        }
        if (!invoker)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Function invoker not available for standardize stage");
            return Status::INVALID_ARGUMENT;
        }

        std::vector<TypedValue> args{value};
        Status status = invoker->callFunctionByName(function_name, args, standardized_out, ctx);
        if (status != Status::OK)
        {
            if (ctx && ctx->message.empty())
            {
                SET_ERROR_CONTEXT(ctx, status, "Quality standardize stage failed");
            }
            return status;
        }

        if (standardized_out.isNull())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Quality standardize stage returned NULL");
            return Status::INVALID_ARGUMENT;
        }

        return Status::OK;
    }

    auto QualityPipeline::executeEnrich(const TypedValue& value,
                                        const std::string& function_name,
                                        FunctionInvoker* invoker,
                                        TypedValue& enriched_out,
                                        std::map<std::string, TypedValue>& metadata_out,
                                        ErrorContext* ctx) -> Status
    {
        enriched_out = value;
        metadata_out.clear();
        if (function_name.empty())
        {
            return Status::OK;
        }
        if (!invoker)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Function invoker not available for enrich stage");
            return Status::INVALID_ARGUMENT;
        }

        std::vector<TypedValue> args{value};
        TypedValue result;
        Status status = invoker->callFunctionByName(function_name, args, result, ctx);
        if (status != Status::OK)
        {
            if (ctx && ctx->message.empty())
            {
                SET_ERROR_CONTEXT(ctx, status, "Quality enrich stage failed");
            }
            return status;
        }

        if (result.isNull())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Quality enrich stage returned NULL");
            return Status::INVALID_ARGUMENT;
        }

        if (result.type() == DataType::JSON || result.type() == DataType::JSONB ||
            result.type() == DataType::TEXT || result.type() == DataType::VARCHAR)
        {
            std::string payload = result.toString();
            try
            {
                json doc = json::parse(payload);
                Status meta_status = extractMetadataFromJson(doc, value.type(),
                                                             enriched_out, metadata_out, ctx);
                if (meta_status != Status::OK)
                {
                    return meta_status;
                }

                if (!doc.is_object() || !doc.contains("value"))
                {
                    enriched_out = result;
                }
                return Status::OK;
            }
            catch (...)
            {
                enriched_out = result;
                return Status::OK;
            }
        }

        enriched_out = result;
        return Status::OK;
    }
} // namespace scratchbird::core
