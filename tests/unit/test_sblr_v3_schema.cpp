#include <gtest/gtest.h>

#include "scratchbird/sblr/v3_payloads.h"

using namespace scratchbird::sblr::v3;

namespace
{
    auto hasField(const SchemaDef& schema,
                  std::string_view field_name,
                  FieldType field_type,
                  std::string_view field_ref = {}) -> bool
    {
        for (const auto& field : schema.fields)
        {
            if (field.name == field_name && field.type == field_type &&
                (field_ref.empty() || field.ref == field_ref))
            {
                return true;
            }
        }
        return false;
    }
} // namespace

TEST(SBLRV3Schema, LookupSchemas) {
    EXPECT_NE(lookupSchema("SCHEMA_SELECT"), nullptr);
    EXPECT_NE(lookupSchema("SCHEMA_DDL_CREATE_TABLE"), nullptr);
    EXPECT_NE(lookupSchema("SCHEMA_PSQL_BLOCK"), nullptr);
    EXPECT_NE(lookupSchema("SCHEMA_LITERAL_INT32"), nullptr);
}

TEST(SBLRV3Schema, OpcodeMapping) {
    const SchemaDef* s = schemaForOpcode(static_cast<uint16_t>(Opcode::SBLR3_SELECT));
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->name, "SCHEMA_SELECT");

    s = schemaForOpcode(static_cast<uint16_t>(Opcode::SBLR3_LITERAL_INT32));
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->name, "SCHEMA_LITERAL_INT32");
}

TEST(SBLRV3Schema, SelectSchemaIncludesRuntimePlanPayload) {
    const SchemaDef* schema = lookupSchema("SCHEMA_SELECT");
    ASSERT_NE(schema, nullptr);
    EXPECT_TRUE(hasField(*schema, "plan", FieldType::OPT, "bytes"));
}

TEST(SBLRV3Schema, ExplainSchemaIncludesFormattingFlags) {
    const SchemaDef* schema = lookupSchema("SCHEMA_EXPLAIN");
    ASSERT_NE(schema, nullptr);
    EXPECT_TRUE(hasField(*schema, "verbose", FieldType::OPT, "bool"));
    EXPECT_TRUE(hasField(*schema, "costs", FieldType::OPT, "bool"));
    EXPECT_TRUE(hasField(*schema, "buffers", FieldType::OPT, "bool"));
    EXPECT_TRUE(hasField(*schema, "wal", FieldType::OPT, "bool"));
    EXPECT_TRUE(hasField(*schema, "timing", FieldType::OPT, "bool"));
}
