#include <gtest/gtest.h>
#include <chrono>
#include <functional>
#include <memory>
#include <vector>
#include "scratchbird/core/audit_logger.h"
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

    void appendInt16(std::vector<uint8_t>& out, uint16_t value)
    {
        size_t offset = out.size();
        out.resize(offset + sizeof(uint16_t));
        scratchbird::sblr::writeInt16(&out[offset], value);
    }

    void appendInt32(std::vector<uint8_t>& out, uint32_t value)
    {
        size_t offset = out.size();
        out.resize(offset + sizeof(uint32_t));
        scratchbird::sblr::writeInt32(&out[offset], value);
    }

    void appendString(std::vector<uint8_t>& out, const std::string& value)
    {
        appendInt32(out, static_cast<uint32_t>(value.size()));
        out.insert(out.end(), value.begin(), value.end());
    }

    void appendId(std::vector<uint8_t>& out, const ID& id)
    {
        out.insert(out.end(), id.bytes.begin(), id.bytes.end());
    }

    void appendExtendedOpcode(std::vector<uint8_t>& out, ExtendedOpcode opcode)
    {
        appendByte(out, static_cast<uint8_t>(Opcode::EXTENDED_OPCODE));
        appendInt16(out, static_cast<uint16_t>(opcode));
    }

    void appendLiteralString(std::vector<uint8_t>& out, const std::string& value)
    {
        appendByte(out, static_cast<uint8_t>(Opcode::LITERAL_STRING));
        appendString(out, value);
    }

    std::vector<uint8_t> buildSelectBytecode(
        uint32_t select_count,
        const std::function<void(std::vector<uint8_t>&)>& emit)
    {
        std::vector<uint8_t> bytecode;
        appendByte(bytecode, static_cast<uint8_t>(Opcode::VERSION));
        appendByte(bytecode, SBLR_VERSION);
        appendByte(bytecode, static_cast<uint8_t>(Opcode::SELECT));
        appendByte(bytecode, static_cast<uint8_t>(Opcode::BEGIN_LIST));
        appendInt32(bytecode, select_count);
        emit(bytecode);
        appendByte(bytecode, static_cast<uint8_t>(Opcode::END_LIST));
        appendByte(bytecode, static_cast<uint8_t>(Opcode::TABLE_REF));
        appendString(bytecode, "");
        return bytecode;
    }

    std::vector<uint8_t> buildConstantReturnFunction(const std::string& value)
    {
        std::vector<uint8_t> bytecode;
        appendByte(bytecode, static_cast<uint8_t>(Opcode::VERSION));
        appendByte(bytecode, SBLR_VERSION);
        appendExtendedOpcode(bytecode, ExtendedOpcode::EXT_RETURN);
        appendByte(bytecode, 1);
        appendLiteralString(bytecode, value);
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

    std::vector<uint8_t> buildBooleanReturnFunction(const std::string& param_name,
                                                    bool return_true)
    {
        std::vector<uint8_t> bytecode;
        appendByte(bytecode, static_cast<uint8_t>(Opcode::VERSION));
        appendByte(bytecode, SBLR_VERSION);
        appendExtendedOpcode(bytecode, ExtendedOpcode::EXT_RETURN);
        appendByte(bytecode, 1);
        appendExtendedOpcode(bytecode, ExtendedOpcode::EXT_VAR_LOAD);
        appendString(bytecode, param_name);
        appendExtendedOpcode(bytecode, ExtendedOpcode::EXT_VAR_LOAD);
        appendString(bytecode, param_name);
        appendByte(bytecode, static_cast<uint8_t>(return_true ? Opcode::EXPR_EQ : Opcode::EXPR_NE));
        appendByte(bytecode, static_cast<uint8_t>(Opcode::END));
        return bytecode;
    }

    std::vector<uint8_t> buildCompareStringReturnFunction(const std::string& param_name,
                                                          const std::string& literal,
                                                          Opcode compare_op)
    {
        std::vector<uint8_t> bytecode;
        appendByte(bytecode, static_cast<uint8_t>(Opcode::VERSION));
        appendByte(bytecode, SBLR_VERSION);
        appendExtendedOpcode(bytecode, ExtendedOpcode::EXT_RETURN);
        appendByte(bytecode, 1);
        appendExtendedOpcode(bytecode, ExtendedOpcode::EXT_VAR_LOAD);
        appendString(bytecode, param_name);
        appendLiteralString(bytecode, literal);
        appendByte(bytecode, static_cast<uint8_t>(compare_op));
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

class DomainE2EScenariosTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        db_file_ = std::make_unique<TestDatabaseFile>("domain_e2e", ".sbdb");

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
        ASSERT_EQ(db_.connect(conn_ctx_, &ctx), Status::OK) << ctx.message;
        ID system_user_id = catalog_->getSystemUserId(&ctx);
        conn_ctx_->setCurrentUser(system_user_id, true);
        conn_ctx_->setCurrentSchemaId(schema_id_);
        executor_->setConnectionContext(conn_ctx_.get());

        CatalogManager::ParameterInfo text_param;
        text_param.name = "value";
        text_param.type = DataType::TEXT;
        text_param.mode = CatalogManager::ParameterMode::IN;

        CatalogManager::ParameterInfo int_param;
        int_param.name = "value";
        int_param.type = DataType::INT32;
        int_param.mode = CatalogManager::ParameterMode::IN;

        registerFunction(catalog_, schema_id_, "validate_ok", DataType::BOOLEAN,
                         {text_param}, buildBooleanReturnFunction("value", true));
        registerFunction(catalog_, schema_id_, "validate_cc", DataType::BOOLEAN,
                         {text_param}, buildCompareStringReturnFunction(
                             "value", "0000-0000-0000-0000", Opcode::EXPR_NE));
        registerFunction(catalog_, schema_id_, "validate_ok_int", DataType::BOOLEAN,
                         {int_param}, buildBooleanReturnFunction("value", true));
        registerFunction(catalog_, schema_id_, "parse_identity", DataType::TEXT,
                         {text_param}, buildIdentityReturnFunction("value"));
        registerFunction(catalog_, schema_id_, "standardize_identity", DataType::TEXT,
                         {text_param}, buildIdentityReturnFunction("value"));
        registerFunction(catalog_, schema_id_, "enrich_identity", DataType::TEXT,
                         {text_param}, buildIdentityReturnFunction("value"));
        registerFunction(catalog_, schema_id_, "parse_phone", DataType::TEXT,
                         {text_param}, buildConstantReturnFunction("parsed_phone"));
        registerFunction(catalog_, schema_id_, "standardize_phone", DataType::TEXT,
                         {text_param}, buildConstantReturnFunction("standard_phone"));
        registerFunction(catalog_, schema_id_, "enrich_phone", DataType::TEXT,
                         {text_param}, buildConstantReturnFunction(
                             "{\"value\":\"enriched_phone\",\"metadata\":{\"carrier\":\"test\"}}"));

        auto create_ssn = compileAndExecute(
            "CREATE DOMAIN ssn_domain AS TEXT WITH SECURITY ("
            "ENCRYPTION = AES256, MASKING = PARTIAL, MASK_PATTERN = 'XXX-XX-####', "
            "AUDIT_ACCESS = TRUE, REQUIRE_PRIVILEGE = SELECT)");
        ASSERT_TRUE(create_ssn.success()) << create_ssn.error();

        auto create_email = compileAndExecute(
            "CREATE DOMAIN email_domain AS TEXT "
            "WITH INTEGRITY (NORMALIZATION_FUNCTION = LOWERCASE) "
            "WITH VALIDATION (FUNCTION = validate_ok, ERROR_MESSAGE = 'invalid email') "
            "WITH QUALITY (PARSE_FUNCTION = parse_identity, STANDARDIZE_FUNCTION = standardize_identity, "
            "ENRICH_FUNCTION = enrich_identity)");
        ASSERT_TRUE(create_email.success()) << create_email.error();

        auto create_username = compileAndExecute(
            "CREATE DOMAIN username_domain AS TEXT "
            "WITH INTEGRITY (UNIQUENESS = TRUE, NORMALIZATION_FUNCTION = LOWERCASE)");
        ASSERT_TRUE(create_username.success()) << create_username.error();

        auto create_phone = compileAndExecute(
            "CREATE DOMAIN phone_domain AS TEXT WITH QUALITY ("
            "PARSE_FUNCTION = parse_phone, STANDARDIZE_FUNCTION = standardize_phone, "
            "ENRICH_FUNCTION = enrich_phone)");
        ASSERT_TRUE(create_phone.success()) << create_phone.error();

        auto create_cc = compileAndExecute(
            "CREATE DOMAIN cc_domain AS TEXT WITH SECURITY ("
            "ENCRYPTION = AES256, MASKING = PARTIAL, MASK_PATTERN = 'XXXX-XXXX-XXXX-####', "
            "REQUIRE_PRIVILEGE = SELECT) "
            "WITH VALIDATION (FUNCTION = validate_cc, ERROR_MESSAGE = 'invalid cc')");
        ASSERT_TRUE(create_cc.success()) << create_cc.error();

        auto create_parent = compileAndExecute(
            "CREATE DOMAIN positive_int AS INT CHECK (VALUE > 0)");
        ASSERT_TRUE(create_parent.success()) << create_parent.error();

        auto create_child = compileAndExecute(
            "CREATE DOMAIN positive_child AS INT WITH VALIDATION ("
            "FUNCTION = validate_ok_int, ERROR_MESSAGE = 'invalid int')");
        ASSERT_TRUE(create_child.success()) << create_child.error();

        DomainInfo ssn_info;
        ASSERT_EQ(domain_mgr_->getDomain(schema_id_, "ssn_domain", ssn_info, &ctx),
                  Status::OK) << ctx.message;
        ssn_domain_id_ = ssn_info.domain_id;

        DomainInfo email_info;
        ASSERT_EQ(domain_mgr_->getDomain(schema_id_, "email_domain", email_info, &ctx),
                  Status::OK) << ctx.message;
        email_domain_id_ = email_info.domain_id;

        DomainInfo username_info;
        ASSERT_EQ(domain_mgr_->getDomain(schema_id_, "username_domain", username_info, &ctx),
                  Status::OK) << ctx.message;
        username_domain_id_ = username_info.domain_id;

        DomainInfo phone_info;
        ASSERT_EQ(domain_mgr_->getDomain(schema_id_, "phone_domain", phone_info, &ctx),
                  Status::OK) << ctx.message;
        phone_domain_id_ = phone_info.domain_id;

        DomainInfo cc_info;
        ASSERT_EQ(domain_mgr_->getDomain(schema_id_, "cc_domain", cc_info, &ctx),
                  Status::OK) << ctx.message;
        cc_domain_id_ = cc_info.domain_id;

        DomainInfo parent_info;
        ASSERT_EQ(domain_mgr_->getDomain(schema_id_, "positive_int", parent_info, &ctx),
                  Status::OK) << ctx.message;
        parent_domain_id_ = parent_info.domain_id;

        DomainInfo child_info;
        ASSERT_EQ(domain_mgr_->getDomain(schema_id_, "positive_child", child_info, &ctx),
                  Status::OK) << ctx.message;
        child_domain_id_ = child_info.domain_id;

        ASSERT_EQ(domain_mgr_->setParentDomain(child_domain_id_, parent_domain_id_, &ctx),
                  Status::OK) << ctx.message;

        ssn_table_id_ = createDomainTable("ssn_table", ssn_domain_id_, DataType::TEXT, ssn_column_id_);
        email_table_id_ = createDomainTable("email_table", email_domain_id_, DataType::TEXT, email_column_id_);
        username_table_one_id_ = createDomainTable("username_table_one", username_domain_id_,
                                                   DataType::TEXT, username_column_one_id_);
        username_table_two_id_ = createDomainTable("username_table_two", username_domain_id_,
                                                   DataType::TEXT, username_column_two_id_);
        phone_table_id_ = createDomainTable("phone_table", phone_domain_id_, DataType::TEXT, phone_column_id_);
        cc_table_id_ = createDomainTable("cc_table", cc_domain_id_, DataType::TEXT, cc_column_id_);
        inherit_table_id_ = createDomainTable("inherit_table", child_domain_id_, DataType::INT32,
                                              inherit_column_id_);

        createMultiDomainTable();

        Status status = catalog_->createUser("e2e_user", "", schema_id_, false, user_id_, &ctx);
        if (status == Status::FILE_EXISTS)
        {
            CatalogManager::UserInfo user_info;
            ASSERT_EQ(catalog_->getUserByName("e2e_user", user_info, &ctx),
                      Status::OK) << ctx.message;
            user_id_ = user_info.user_id;
        }
        else
        {
            ASSERT_EQ(status, Status::OK) << ctx.message;
        }
    }

    void TearDown() override
    {
        executor_.reset();
        compiler_.reset();
        conn_ctx_.reset();
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

    ID createDomainTable(const std::string& name,
                         const ID& domain_id,
                         DataType data_type,
                         ID& column_id_out)
    {
        ErrorContext ctx;
        CatalogManager::ColumnInfo id_col;
        id_col.column_id = generateUuidV7();
        id_col.column_name = "id";
        id_col.data_type = static_cast<uint16_t>(DataType::INT32);
        id_col.nullable = false;
        id_col.ordinal = 0;

        CatalogManager::ColumnInfo val_col;
        val_col.column_id = generateUuidV7();
        val_col.column_name = "val";
        val_col.data_type = static_cast<uint16_t>(data_type);
        val_col.nullable = false;
        val_col.ordinal = 1;
        val_col.domain_id = domain_id;

        column_id_out = val_col.column_id;
        std::vector<CatalogManager::ColumnInfo> columns{id_col, val_col};

        ID table_id;
        Status status = catalog_->createTable(schema_id_, name, columns, table_id, 0, &ctx);
        if (status != Status::OK)
        {
            ADD_FAILURE() << ctx.message;
            return ID{};
        }
        return table_id;
    }

    void createMultiDomainTable()
    {
        ErrorContext ctx;
        CatalogManager::ColumnInfo id_col;
        id_col.column_id = generateUuidV7();
        id_col.column_name = "id";
        id_col.data_type = static_cast<uint16_t>(DataType::INT32);
        id_col.nullable = false;
        id_col.ordinal = 0;

        CatalogManager::ColumnInfo email_col;
        email_col.column_id = generateUuidV7();
        email_col.column_name = "email";
        email_col.data_type = static_cast<uint16_t>(DataType::TEXT);
        email_col.nullable = false;
        email_col.ordinal = 1;
        email_col.domain_id = email_domain_id_;

        CatalogManager::ColumnInfo username_col;
        username_col.column_id = generateUuidV7();
        username_col.column_name = "username";
        username_col.data_type = static_cast<uint16_t>(DataType::TEXT);
        username_col.nullable = false;
        username_col.ordinal = 2;
        username_col.domain_id = username_domain_id_;

        CatalogManager::ColumnInfo phone_col;
        phone_col.column_id = generateUuidV7();
        phone_col.column_name = "phone";
        phone_col.data_type = static_cast<uint16_t>(DataType::TEXT);
        phone_col.nullable = false;
        phone_col.ordinal = 3;
        phone_col.domain_id = phone_domain_id_;

        std::vector<CatalogManager::ColumnInfo> columns{id_col, email_col, username_col, phone_col};
        ASSERT_EQ(catalog_->createTable(schema_id_, "multi_domain_table", columns, multi_table_id_, 0,
                                        &ctx),
                  Status::OK) << ctx.message;
    }

    void grantDomainSelect(const ID& domain_id)
    {
        ErrorContext ctx;
        ID grantor_id = catalog_->getSystemUserId(&ctx);
        uint32_t privileges = static_cast<uint32_t>(CatalogManager::Privilege::SELECT);
        ASSERT_EQ(catalog_->grantPermission(domain_id, CatalogManager::PermissionObjectType::DOMAIN,
                                            user_id_, CatalogManager::GranteeType::USER,
                                            privileges, false, grantor_id, &ctx),
                  Status::OK) << ctx.message;
    }

    Database db_{};
    std::unique_ptr<TestDatabaseFile> db_file_;
    CatalogManager* catalog_ = nullptr;
    DomainManager* domain_mgr_ = nullptr;
    ID schema_id_{};
    std::unique_ptr<ConnectionContext> conn_ctx_;

    ID ssn_domain_id_{};
    ID email_domain_id_{};
    ID username_domain_id_{};
    ID phone_domain_id_{};
    ID cc_domain_id_{};
    ID parent_domain_id_{};
    ID child_domain_id_{};

    ID ssn_table_id_{};
    ID email_table_id_{};
    ID username_table_one_id_{};
    ID username_table_two_id_{};
    ID phone_table_id_{};
    ID cc_table_id_{};
    ID inherit_table_id_{};
    ID multi_table_id_{};

    ID ssn_column_id_{};
    ID email_column_id_{};
    ID username_column_one_id_{};
    ID username_column_two_id_{};
    ID phone_column_id_{};
    ID cc_column_id_{};
    ID inherit_column_id_{};

    ID user_id_{};

    std::unique_ptr<QueryCompilerV2> compiler_;
    std::unique_ptr<Executor> executor_;
};

TEST_F(DomainE2EScenariosTest, ScenarioSsnSecurity)
{
    auto insert_result = compileAndExecute(
        "INSERT INTO ssn_table (id, val) VALUES (1, '123-45-6789')");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();

    auto select_result = compileAndExecute("SELECT val FROM ssn_table");
    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());
    EXPECT_EQ(select_result.resultSet()->getValue(0, 0).toString(), "123-45-6789");

    TypedValue value = TypedValue::makeText("123-45-6789");
    TypedValue masked;
    ErrorContext ctx;
    ASSERT_EQ(domain_mgr_->applyMasking(ssn_domain_id_, user_id_, value, masked, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(masked.toString(), "***-**-6789");

    grantDomainSelect(ssn_domain_id_);
    ASSERT_EQ(domain_mgr_->applyMasking(ssn_domain_id_, user_id_, value, masked, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(masked.toString(), "123-45-6789");

    auto* audit_logger = db_.audit_logger();
    ASSERT_NE(audit_logger, nullptr);
    uint64_t before = audit_logger->getTotalEventCount();

    auto bytecode = buildSelectBytecode(1, [&](std::vector<uint8_t>& out) {
        appendLiteralString(out, "audit");
        appendExtendedOpcode(out, ExtendedOpcode::EXT_AUDIT_DOMAIN_ACCESS);
        appendId(out, ssn_domain_id_);
        appendId(out, user_id_);
        appendId(out, ssn_table_id_);
        appendId(out, ssn_column_id_);
    });
    auto audit_result = executor_->execute(bytecode);
    ASSERT_TRUE(audit_result.success()) << audit_result.error();
    EXPECT_GT(audit_logger->getTotalEventCount(), before);
}

TEST_F(DomainE2EScenariosTest, ScenarioEmailNormalizationValidationQuality)
{
    auto insert_result = compileAndExecute(
        "INSERT INTO email_table (id, val) VALUES (1, 'User@Example.COM')");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();

    auto select_result = compileAndExecute("SELECT val FROM email_table");
    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());
    EXPECT_EQ(select_result.resultSet()->getValue(0, 0).toString(), "user@example.com");
}

TEST_F(DomainE2EScenariosTest, ScenarioUsernameGlobalUniqueness)
{
    auto insert_one = compileAndExecute(
        "INSERT INTO username_table_one (id, val) VALUES (1, 'Alice')");
    ASSERT_TRUE(insert_one.success()) << insert_one.error();

    auto insert_two = compileAndExecute(
        "INSERT INTO username_table_two (id, val) VALUES (1, 'alice')");
    ASSERT_FALSE(insert_two.success());
    EXPECT_NE(insert_two.error().find("Domain uniqueness"), std::string::npos);
}

TEST_F(DomainE2EScenariosTest, ScenarioPhoneQualityPipeline)
{
    auto insert_result = compileAndExecute(
        "INSERT INTO phone_table (id, val) VALUES (1, '555-1212')");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();

    auto select_result = compileAndExecute("SELECT val FROM phone_table");
    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());
    EXPECT_EQ(select_result.resultSet()->getValue(0, 0).toString(), "enriched_phone");

    TypedValue value = TypedValue::makeText("555-1212");
    QualityResult result;
    ErrorContext ctx;
    ASSERT_EQ(domain_mgr_->executeQualityPipeline(phone_domain_id_, value, executor_.get(), result, &ctx),
              Status::OK) << ctx.message;

    auto it = result.metadata.find("carrier");
    ASSERT_NE(it, result.metadata.end());
    EXPECT_EQ(it->second.toString(), "test");
}

TEST_F(DomainE2EScenariosTest, ScenarioCreditCardSecurityValidation)
{
    auto insert_ok = compileAndExecute(
        "INSERT INTO cc_table (id, val) VALUES (1, '4111-1111-1111-1111')");
    ASSERT_TRUE(insert_ok.success()) << insert_ok.error();

    auto insert_bad = compileAndExecute(
        "INSERT INTO cc_table (id, val) VALUES (2, '0000-0000-0000-0000')");
    ASSERT_FALSE(insert_bad.success());
    EXPECT_NE(insert_bad.error().find("invalid cc"), std::string::npos);

    auto select_result = compileAndExecute("SELECT val FROM cc_table WHERE id = 1");
    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());
    EXPECT_EQ(select_result.resultSet()->getValue(0, 0).toString(), "4111-1111-1111-1111");

    TypedValue value = TypedValue::makeText("4111-1111-1111-1111");
    TypedValue masked;
    ErrorContext ctx;
    ASSERT_EQ(domain_mgr_->applyMasking(cc_domain_id_, user_id_, value, masked, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(masked.toString(), "****-****-****-1111");

    grantDomainSelect(cc_domain_id_);
    ASSERT_EQ(domain_mgr_->applyMasking(cc_domain_id_, user_id_, value, masked, &ctx),
              Status::OK) << ctx.message;
    EXPECT_EQ(masked.toString(), "4111-1111-1111-1111");
}

TEST_F(DomainE2EScenariosTest, ScenarioMultipleDomainsSingleTable)
{
    auto insert_result = compileAndExecute(
        "INSERT INTO multi_domain_table (id, email, username, phone) "
        "VALUES (1, 'User@Example.COM', 'Bob', '555-1212')");
    ASSERT_TRUE(insert_result.success()) << insert_result.error();

    auto select_result = compileAndExecute(
        "SELECT email, username, phone FROM multi_domain_table");
    ASSERT_TRUE(select_result.success()) << select_result.error();
    ASSERT_TRUE(select_result.hasResultSet());
    EXPECT_EQ(select_result.resultSet()->getValue(0, 0).toString(), "user@example.com");
    EXPECT_EQ(select_result.resultSet()->getValue(0, 1).toString(), "bob");
    EXPECT_EQ(select_result.resultSet()->getValue(0, 2).toString(), "enriched_phone");
}

TEST_F(DomainE2EScenariosTest, ScenarioDomainInheritanceWithWithBlocks)
{
    auto insert_bad = compileAndExecute(
        "INSERT INTO inherit_table (id, val) VALUES (1, -1)");
    ASSERT_FALSE(insert_bad.success());

    auto insert_ok = compileAndExecute(
        "INSERT INTO inherit_table (id, val) VALUES (2, 5)");
    ASSERT_TRUE(insert_ok.success()) << insert_ok.error();
}
