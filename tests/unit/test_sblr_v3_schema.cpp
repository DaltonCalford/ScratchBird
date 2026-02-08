#include <gtest/gtest.h>

#include "scratchbird/sblr/v3_payloads.h"

using namespace scratchbird::sblr::v3;

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
