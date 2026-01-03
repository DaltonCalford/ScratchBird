#include "scratchbird/core/domain_validation.h"

namespace scratchbird::core
{
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
