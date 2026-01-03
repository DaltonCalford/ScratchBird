#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "unit/test_user_helpers.h"
#include "gtest/gtest.h"
#include <cstdio>

using namespace scratchbird::core;

TEST(DomainPersistenceTest, ReloadsDomainMetadata)
{
    const char* test_db = "test_domain_persistence.sbdb";
    std::remove(test_db);

    ErrorContext ctx;
    Status status = Database::create(test_db, 16384, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    Database db;
    status = db.open(test_db, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    auto* catalog = db.catalog_manager();
    ASSERT_NE(catalog, nullptr);
    EnsureUser(catalog, "test_user");

    ID schema_id;
    status = catalog->createSchema("test_schema", "test_user", schema_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    auto* dm = db.domain_manager();
    ASSERT_NE(dm, nullptr);

    std::vector<DomainConstraint> constraints;
    constraints.emplace_back(ConstraintType::CHECK, "value > 0", "positive_value");

    ID basic_id;
    DomainManager::DomainCreateOptions options;
    options.nullable = false;
    options.default_value = "";
    options.constraints = constraints;
    options.dialect_tag = "postgresql";
    options.compat_name = "int4";
    status = dm->createBasicDomain(schema_id, "positive_int", DataType::INT32, 0, 0,
                                   options, basic_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    std::vector<RecordField> fields;
    RecordField name_field("name", DataType::TEXT, false);
    fields.push_back(name_field);
    RecordField age_field("age", DataType::INT32, true);
    fields.push_back(age_field);
    RecordField score_field("score", DataType::INT32, true);
    score_field.domain_id = basic_id;
    fields.push_back(score_field);

    ID record_id;
    status = dm->createRecordDomain(schema_id, "person_record", fields, record_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    std::vector<EnumValue> enum_values;
    enum_values.emplace_back("small", 1);
    enum_values.emplace_back("medium", 2);
    enum_values.emplace_back("large", 3);

    ID enum_id;
    status = dm->createEnumDomain(schema_id, "size_enum", enum_values, enum_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    std::vector<DataType> variant_types = {DataType::INT32, DataType::TEXT};

    ID variant_id;
    status = dm->createVariantDomain(schema_id, "flexible_value", variant_types, variant_id, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    db.close();

    Database db_reopen;
    status = db_reopen.open(test_db, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;

    auto* dm_reopen = db_reopen.domain_manager();
    ASSERT_NE(dm_reopen, nullptr);

    DomainInfo basic_info;
    status = dm_reopen->getDomain(schema_id, "positive_int", basic_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(basic_info.domain_id, basic_id);
    ASSERT_EQ(basic_info.constraints.size(), 1u);
    EXPECT_EQ(basic_info.constraints[0].type, ConstraintType::CHECK);
    EXPECT_EQ(basic_info.constraints[0].expression, "value > 0");
    EXPECT_EQ(basic_info.constraints[0].name, "positive_value");
    EXPECT_EQ(basic_info.dialect_tag, "postgresql");
    EXPECT_EQ(basic_info.compat_name, "int4");

    DomainInfo record_info;
    status = dm_reopen->getDomain(schema_id, "person_record", record_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(record_info.domain_id, record_id);
    ASSERT_EQ(record_info.fields.size(), 3u);
    EXPECT_EQ(record_info.fields[0].name, "name");
    EXPECT_EQ(record_info.fields[0].type, DataType::TEXT);
    EXPECT_FALSE(record_info.fields[0].nullable);
    EXPECT_EQ(record_info.fields[2].domain_id, basic_id);

    DomainInfo enum_info;
    status = dm_reopen->getDomain(schema_id, "size_enum", enum_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(enum_info.domain_id, enum_id);
    ASSERT_EQ(enum_info.enum_values.size(), 3u);
    EXPECT_EQ(enum_info.enum_values[1].label, "medium");
    EXPECT_EQ(enum_info.enum_values[1].position, 2);

    DomainInfo variant_info;
    status = dm_reopen->getDomain(schema_id, "flexible_value", variant_info, &ctx);
    ASSERT_EQ(status, Status::OK) << ctx.message;
    EXPECT_EQ(variant_info.domain_id, variant_id);
    ASSERT_EQ(variant_info.variant_allowed_types.size(), 2u);
    EXPECT_EQ(variant_info.variant_allowed_types[0].type, DataType::INT32);
    EXPECT_EQ(variant_info.variant_allowed_types[1].type, DataType::TEXT);

    db_reopen.close();
    std::remove(test_db);
}
