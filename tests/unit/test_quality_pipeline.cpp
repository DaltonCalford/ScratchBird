#include "scratchbird/core/quality_pipeline.h"
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

TEST(QualityPipelineTest, PipelineStagesAndMetadata) {
    TestFunctionInvoker invoker;
    invoker.registerHandler("parse_fn",
                            [](const std::vector<TypedValue>&, TypedValue& out, ErrorContext*) {
                                out = TypedValue::makeVarchar("5551234");
                                return Status::OK;
                            });
    invoker.registerHandler("standardize_fn",
                            [](const std::vector<TypedValue>&, TypedValue& out, ErrorContext*) {
                                out = TypedValue::makeVarchar("+1-555-1234");
                                return Status::OK;
                            });
    invoker.registerHandler("enrich_fn",
                            [](const std::vector<TypedValue>&, TypedValue& out, ErrorContext*) {
                                out = TypedValue::makeText("{\"value\":\"+1-555-1234\",\"metadata\":{\"carrier\":\"unit\",\"timezone\":\"UTC\"}}");
                                return Status::OK;
                            });

    QualityConfig config;
    config.parse_function = "parse_fn";
    config.standardize_function = "standardize_fn";
    config.enrich_function = "enrich_fn";

    TypedValue input = TypedValue::makeVarchar("555-1234");
    QualityResult result;
    ErrorContext ctx;

    ASSERT_EQ(QualityPipeline::executePipeline(input, config, &invoker, result, &ctx), Status::OK);
    ASSERT_EQ(result.parsed_value.getVarchar(), "5551234");
    ASSERT_EQ(result.standardized_value.getVarchar(), "+1-555-1234");
    ASSERT_EQ(result.enriched_value.getVarchar(), "+1-555-1234");
    ASSERT_EQ(result.metadata.at("carrier").getText(), "unit");
    ASSERT_EQ(result.metadata.at("timezone").getText(), "UTC");
}

TEST(QualityPipelineTest, NoFunctionsKeepsValue) {
    QualityConfig config;
    TypedValue input = TypedValue::makeVarchar("raw");
    QualityResult result;
    ErrorContext ctx;

    ASSERT_EQ(QualityPipeline::executePipeline(input, config, nullptr, result, &ctx), Status::OK);
    ASSERT_EQ(result.enriched_value.getVarchar(), "raw");
    ASSERT_TRUE(result.metadata.empty());
}

TEST(QualityPipelineTest, ParseStageNullFails) {
    TestFunctionInvoker invoker;
    invoker.registerHandler("parse_fn",
                            [](const std::vector<TypedValue>&, TypedValue& out, ErrorContext*) {
                                out = TypedValue::makeNull(DataType::VARCHAR);
                                return Status::OK;
                            });

    QualityConfig config;
    config.parse_function = "parse_fn";

    TypedValue input = TypedValue::makeVarchar("raw");
    QualityResult result;
    ErrorContext ctx;

    ASSERT_EQ(QualityPipeline::executePipeline(input, config, &invoker, result, &ctx), Status::INVALID_ARGUMENT);
}
