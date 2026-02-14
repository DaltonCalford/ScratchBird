#include <gtest/gtest.h>

#include "scratchbird/sblr/v3_canonical_feature_map.generated.h"
#include "scratchbird/sblr/v3_opcode_identity.h"
#include "scratchbird/sblr/v3_opcode_registry.h"

using namespace scratchbird::sblr::v3;

TEST(SBLRV3CanonicalFeatureMap, LoadsAuthoritativeRows) {
    EXPECT_GE(canonicalFeatureRowCount(), static_cast<std::size_t>(150));
    EXPECT_TRUE(isCanonicalFeatureOpcodeSymbol("OP_STMT_DML_SELECT"));
    EXPECT_TRUE(isCanonicalFeatureOpcodeSymbol("OP_STMT_DDL_CREATE_TABLE"));
    EXPECT_FALSE(isCanonicalFeatureOpcodeSymbol("OP_STMT_UNKNOWN_NOPE"));
}

TEST(SBLRV3OpcodeIdentity, MapsKnownStatementOpcodes) {
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_SELECT"), "OP_STMT_DML_SELECT");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_INSERT"), "OP_STMT_DML_INSERT");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_UPDATE"), "OP_STMT_DML_UPDATE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_DELETE"), "OP_STMT_DML_DELETE");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_CREATE_TABLE"), "OP_STMT_DDL_CREATE_TABLE");

    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_SELECT"));
    EXPECT_TRUE(opcodeMapsToCanonicalFeatureName("SBLR3_CREATE_TABLE"));
}

TEST(SBLRV3OpcodeIdentity, MapsExpressionAndTypeFamilies) {
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_EXPR_SUBTRACT"), "OP_EXPR_SUB");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_EXPR_MULTIPLY"), "OP_EXPR_MUL");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_TYPE_UINT128"), "OP_TYPE_UINT128");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_LITERAL_UUID"), "OP_VAL_UUID");
    EXPECT_EQ(canonicalOpcodeSymbolForV3Name("SBLR3_PSQL_FOR_SELECT"), "OP_FLOW_PSQL_FOR_SELECT");

    EXPECT_FALSE(opcodeMapsToCanonicalFeatureName("SBLR3_EXPR_SUBTRACT"));
}

TEST(SBLRV3OpcodeIdentity, MapsFromNumericOpcode) {
    const uint16_t select_opcode = static_cast<uint16_t>(Opcode::SBLR3_SELECT);
    const uint16_t version_opcode = static_cast<uint16_t>(Opcode::SBLR3_VERSION);

    EXPECT_EQ(canonicalOpcodeSymbolForOpcode(select_opcode), "OP_STMT_DML_SELECT");
    EXPECT_EQ(canonicalOpcodeSymbolForOpcode(version_opcode), "OP_MOD_BEGIN");
    EXPECT_TRUE(opcodeMapsToCanonicalFeature(select_opcode));
    EXPECT_FALSE(opcodeMapsToCanonicalFeature(version_opcode));
}

