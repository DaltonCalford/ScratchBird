#include <gtest/gtest.h>
#include <chrono>
#include <functional>
#include <memory>
#include <vector>
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/quality_pipeline.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/opcodes.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "test_helpers.h"

using namespace scratchbird::core;
using namespace scratchbird::sblr;
using scratchbird::testing::TestDatabaseFile;

namespace
{
    void appendByte(std::vector<uint8_t>& out, uint8_t value)
    {
        out.push_back(value);
    }

    void appendInt32(std::vector<uint8_t>& out, uint32_t value)
    {
        size_t offset = out.size();
        out.resize(offset + sizeof(uint32_t));
        scratchbird::sblr::writeInt32(&out[offset], value);
    }

    void appendUVarint(std::vector<uint8_t>& out, uint64_t value)
    {
        size_t offset = out.size();
        out.resize(offset + 10);
        size_t count = scratchbird::sblr::writeUVarint(&out[offset], value);
        out.resize(offset + count);
    }

    void appendString(std::vector<uint8_t>& out, const std::string& value)
    {
        appendUVarint(out, static_cast<uint64_t>(value.size()));
        out.insert(out.end(), value.begin(), value.end());
    }

    void appendExtendedOpcode(std::vector<uint8_t>& out, ExtendedOpcode opcode)
    {
        out.push_back(static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
        size_t offset = out.size();
        out.resize(offset + sizeof(uint16_t));
        scratchbird::sblr::writeInt16(&out[offset], static_cast<uint16_t>(opcode));
    }

    std::vector<uint8_t> buildConstantReturnFunction(const std::string& value)
    {
        std::vector<uint8_t> bytecode;
        appendByte(bytecode, static_cast<uint8_t>(Opcode::VERSION));
        appendByte(bytecode, SBLR_VERSION);
        appendExtendedOpcode(bytecode, ExtendedOpcode::EXT_RETURN);
        appendByte(bytecode, 1);
        appendByte(bytecode, static_cast<uint8_t>(Opcode::LITERAL_STRING));
        appendString(bytecode, value);
        appendByte(bytecode, static_cast<uint8_t>(Opcode::END));
        return bytecode;
    }

    std::vector<uint8_t> buildIdentityReturnFunction(const std::string& param_name)
    {
        std::vector<uint8_t> bytecode;
        appendByte(bytecode, static_cast<uint8_t>(Opcode::VERSION));
        appendByte(bytecode, SBLR_VERSION);
        appendExtendedOpcode(bytecode, ExtendedOpcode::EXT_RETURN);
        appendByte(bytecode, 1);
        appendExtendedOpcode(bytecode, ExtendedOpcode::EXT_VAR_LOAD);
        appendString(bytecode, param_name);
        appendByte(bytecode, static_cast<uint8_t>(Opcode::END));
        return bytecode;
    }

    void registerFunction(CatalogManager* catalog,
                          const ID& schema_id,
                          const std::string& name,
                          DataType return_type,
                          const std::vector<CatalogManager::ParameterInfo>& params,
                          const std::vector<uint8_t>& bytecode)
    {
        CatalogManager::FunctionInfo info;
        info.function_id = generateUuidV7();
        info.schema_id = schema_id;
        info.name = name;
        ErrorContext ctx;
        info.owner_id = catalog->getSystemUserId(&ctx);
        info.parameters = params;
        info.return_type = return_type;
        info.bytecode = bytecode;
        auto now = static_cast<uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count());
        info.created_time = now;
        info.modified_time = now;

        Status status = catalog->registerFunction(info, &ctx);
        ASSERT_EQ(status, Status::OK) << ctx.message;
    }
}

class DomainQualityIntegrationTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("domain_quality", ".sbdb");

        ErrorContext ctx;
        ASSERT_EQ(Database::create(db_file_->path(), 16384, &ctx), Status::OK) << ctx.message;
        ASSERT_EQ(db_.open(db_file_->path(), &ctx), Status::OK) << ctx.message;

        catalog_ = db_.catalog_manager();
        ASSERT_NE(catalog_, nullptr);
        domain_mgr_ = db_.domain_manager();
        ASSERT_NE(domain_mgr_, nullptr);

        CatalogManager::SchemaInfo schema_info;
        ASSERT_EQ(catalog_->getSchema("PUBLIC", schema_info, &ctx), Status::OK) << ctx.message;
        schema_id_ = schema_info.schema_id;

        compiler_ = std::make_unique<QueryCompilerV2>(&db_);
        compiler_->setCurrentSchema(schema_id_);
        executor_ = std::make_unique<Executor>(&db_);
        executor_->setCurrentSchema(schema_id_);
        Status conn_status = db_.connect(connection_ctx_, &ctx);
        ASSERT_EQ(conn_status, Status::OK) << ctx.message;
        connection_ctx_->setCurrentUser(catalog_->getSystemUserId(&ctx), true);
        ConnectionContext::setCurrent(connection_ctx_.get());
        executor_->setConnectionContext(connection_ctx_.get());

        CatalogManager::ParameterInfo param;
        param.name = "value";
        param.type = DataType::TEXT;
        param.mode = CatalogManager::ParameterMode::IN;

        registerFunction(catalog_, schema_id_, "parse_const", DataType::TEXT,
                         {param}, buildConstantReturnFunction("parsed"));
        registerFunction(catalog_, schema_id_, "standardize_const", DataType::TEXT,
                         {param}, buildConstantReturnFunction("standardized"));
        registerFunction(catalog_, schema_id_, "enrich_const", DataType::TEXT,
                         {param}, buildConstantReturnFunction(
                             "{\"value\":\"enriched\",\"metadata\":{\"source\":\"integration\"}}"));
        registerFunction(catalog_, schema_id_, "identity_value", DataType::TEXT,
                         {param}, buildIdentityReturnFunction("value"));

        auto create_domain = compileAndExecute(
            "CREATE DOMAIN quality_text AS TEXT WITH QUALITY ("
            "PARSE_FUNCTION = parse_const, STANDARDIZE_FUNCTION = standardize_const, "
            "ENRICH_FUNCTION = enrich_const)");
        ASSERT_TRUE(create_domain.success()) << create_domain.error();

        DomainInfo domain_info;
        ASSERT_EQ(domain_mgr_->getDomain(schema_id_, "quality_text", domain_info, &ctx),
                  Status::OK) << ctx.message;
        domain_id_ = domain_info.domain_id;

        CatalogManager::ColumnInfo id_col;
        id_col.column_id = generateUuidV7();
        id_col.column_name = "id";
        id_col.data_type = static_cast<uint16_t>(DataType::INT32);
        id_col.nullable = false;
        id_col.ordinal = 0;

        CatalogManager::ColumnInfo val_col;
        val_col.column_id = generateUuidV7();
        val_col.column_name = "val";
        val_col.data_type = static_cast<uint16_t>(DataType::TEXT);
        val_col.nullable = false;
        val_col.ordinal = 1;
        val_col.domain_id = domain_id_;

        std::vector<CatalogManager::ColumnInfo> columns{id_col, val_col};
        ASSERT_EQ(catalog_->createTable(schema_id_, "quality_table", columns, table_id_, 0, &ctx),
                  Status::OK) << ctx.message;
    }

    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        ConnectionContext::setCurrent(nullptr);
        connection_ctx_.reset();
        db_.close();
        db_file_.reset();
    }

    ExecutionResult compileAndExecute(const std::string& sql)
    {
        auto compile_result = compiler_->compile(sql);
        if (!compile_result.success())
        {
            std::string errors;
            for (const auto& err : compile_result.errors())
            {
                errors += err + "\n";
            }
            return ExecutionResult("Compilation failed: " + errors);
        }
        return executor_->execute(compile_result.bytecode());
    }

    Database db_{};
    std::unique_ptr<TestDatabaseFile> db_file_;
    CatalogManager* catalog_ = nullptr;
    DomainManager* domain_mgr_ = nullptr;
    ID schema_id_{};
    ID domain_id_{};
    ID table_id_{};
    std::unique_ptr<QueryCompilerV2> compiler_;
    std::unique_ptr<Executor> executor_;
    std::unique_ptr<ConnectionContext> connection_ctx_;
};

TEST_F(DomainQualityIntegrationTest, AppliesQualityPipelineOnInsert)
{
    auto insert_result = compileAndExecute(
        "INSERT INTO quality_table (id, val) VALUES (1, 'raw')");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();

    auto select_result = compileAndExecute("SELECT val FROM quality_table");
    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());
    EXPECT_EQ(select_result.resultSet()->getValue(0, 0).toString(), "enriched");
}

TEST_F(DomainQualityIntegrationTest, PipelineReturnsMetadata)
{
    TypedValue value = TypedValue::makeText("raw");
    QualityResult result;
    ErrorContext ctx;

    ASSERT_EQ(domain_mgr_->executeQualityPipeline(domain_id_, value, executor_.get(), result, &ctx),
              Status::OK) << ctx.message;

    EXPECT_EQ(result.parsed_value.toString(), "parsed");
    EXPECT_EQ(result.standardized_value.toString(), "standardized");
    EXPECT_EQ(result.enriched_value.toString(), "enriched");

    auto it = result.metadata.find("source");
    ASSERT_NE(it, result.metadata.end());
    EXPECT_EQ(it->second.toString(), "integration");
}
