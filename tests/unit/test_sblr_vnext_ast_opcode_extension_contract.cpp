#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "scratchbird/parser/v3_emitter.h"
#include "scratchbird/sblr/v3_codec.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "scratchbird/sblr/v3_validator.h"

using scratchbird::parser::v3::ASTKind;
using scratchbird::parser::v3::V3Emitter;
using scratchbird::sblr::v3::DecodeError;
using scratchbird::sblr::v3::Instruction;
using scratchbird::sblr::v3::Opcode;
using scratchbird::sblr::v3::SchemaDef;
using scratchbird::sblr::v3::ValidationResult;
using scratchbird::sblr::v3::Value;

namespace
{
auto roundTripInstruction(const Instruction &inst, Instruction &decoded, std::string &err) -> bool
{
    scratchbird::sblr::v3::Buffer stream;
    DecodeError decode_err;
    if (!scratchbird::sblr::v3::encodeInstructionWithSchema(inst, stream, decode_err))
    {
        err = decode_err.message;
        return false;
    }
    size_t offset = 0;
    if (!scratchbird::sblr::v3::decodeInstructionWithSchema(
            stream.data(), stream.size(), offset, decoded, decode_err))
    {
        err = decode_err.message;
        return false;
    }
    if (offset != stream.size())
    {
        err = "roundtrip offset mismatch";
        return false;
    }
    return true;
}
} // namespace

TEST(SBLRVNextAstOpcodeExtensionContractTest, OpcodeRangeAndSchemasAreRegistered)
{
    EXPECT_EQ(static_cast<uint16_t>(Opcode::SBLR3_OP_DOC_PATH_FILTER), 0x6001);
    EXPECT_EQ(static_cast<uint16_t>(Opcode::SBLR3_OP_TS_BUCKET_AGG), 0x6002);
    EXPECT_EQ(static_cast<uint16_t>(Opcode::SBLR3_OP_COL_SCAN), 0x6003);
    EXPECT_EQ(static_cast<uint16_t>(Opcode::SBLR3_OP_SEARCH_DSL_EVAL), 0x6004);
    EXPECT_EQ(static_cast<uint16_t>(Opcode::SBLR3_OP_VECTOR_ANN), 0x6005);
    EXPECT_EQ(static_cast<uint16_t>(Opcode::SBLR3_OP_HYBRID_BRIDGE_EXCHANGE), 0x6006);
    EXPECT_EQ(static_cast<uint16_t>(Opcode::SBLR3_OP_HYBRID_BRIDGE_MATERIALIZE), 0x6007);

    const uint16_t opcodes[] = {
        static_cast<uint16_t>(Opcode::SBLR3_OP_DOC_PATH_FILTER),
        static_cast<uint16_t>(Opcode::SBLR3_OP_TS_BUCKET_AGG),
        static_cast<uint16_t>(Opcode::SBLR3_OP_COL_SCAN),
        static_cast<uint16_t>(Opcode::SBLR3_OP_SEARCH_DSL_EVAL),
        static_cast<uint16_t>(Opcode::SBLR3_OP_VECTOR_ANN),
        static_cast<uint16_t>(Opcode::SBLR3_OP_HYBRID_BRIDGE_EXCHANGE),
        static_cast<uint16_t>(Opcode::SBLR3_OP_HYBRID_BRIDGE_MATERIALIZE)};

    for (uint16_t opcode : opcodes)
    {
        EXPECT_TRUE(scratchbird::sblr::v3::isKnownOpcode(opcode));
        EXPECT_NE(nullptr, scratchbird::sblr::v3::opcodeName(opcode));
        const SchemaDef *schema = scratchbird::sblr::v3::schemaForOpcode(opcode);
        ASSERT_NE(nullptr, schema) << "missing schema for opcode " << opcode;
    }
}

TEST(SBLRVNextAstOpcodeExtensionContractTest, AstContractShimMapsNodesToOpcodes)
{
    struct NodeCase
    {
        ASTKind node_kind;
        uint16_t expected_opcode;
        Value::Object fields;
    };

    std::vector<NodeCase> cases;
    cases.push_back(
        {ASTKind::AST_DOC_PATH_FILTER,
         static_cast<uint16_t>(Opcode::SBLR3_OP_DOC_PATH_FILTER),
         {{"path_expr", Value(static_cast<uint64_t>(10))},
          {"operator", Value(static_cast<uint64_t>(0))},
          {"value_expr", Value(static_cast<uint64_t>(22))}}});

    cases.push_back(
        {ASTKind::AST_TS_BUCKET_AGG,
         static_cast<uint16_t>(Opcode::SBLR3_OP_TS_BUCKET_AGG),
         {{"time_expr", Value(static_cast<uint64_t>(9))},
          {"bucket_size", Value(static_cast<uint64_t>(60'000'000'000ULL))},
          {"agg_list", Value(Value::List{Value(static_cast<uint64_t>(1)), Value(static_cast<uint64_t>(2))})}}});

    cases.push_back(
        {ASTKind::AST_COL_SCAN_HINT,
         static_cast<uint16_t>(Opcode::SBLR3_OP_COL_SCAN),
         {{"table_ref", Value(static_cast<uint64_t>(100))},
          {"projection_set", Value(Value::Bytes{0xAA, 0x55})},
          {"predicate_set", Value(Value::Bytes{0x0F})}}});

    cases.push_back(
        {ASTKind::AST_SEARCH_QUERY_DSL,
         static_cast<uint16_t>(Opcode::SBLR3_OP_SEARCH_DSL_EVAL),
         {{"dsl_payload_json", Value(std::string("{\"query\":\"idx\"}"))},
          {"target_index", Value(static_cast<uint64_t>(501))},
          {"scorer_id", Value(static_cast<uint64_t>(1))}}});

    cases.push_back(
        {ASTKind::AST_VECTOR_ANN_QUERY,
         static_cast<uint16_t>(Opcode::SBLR3_OP_VECTOR_ANN),
         {{"vector_expr", Value(static_cast<uint64_t>(77))},
          {"metric", Value(static_cast<uint64_t>(1))},
          {"k", Value(static_cast<uint64_t>(8))},
          {"ef_search", Value(static_cast<uint64_t>(64))}}});

    cases.push_back(
        {ASTKind::AST_HYBRID_BRIDGE,
         static_cast<uint16_t>(Opcode::SBLR3_OP_HYBRID_BRIDGE_EXCHANGE),
         {{"source_track", Value(static_cast<uint64_t>(1))},
          {"target_track", Value(static_cast<uint64_t>(2))},
          {"bridge_mode", Value(static_cast<uint64_t>(3))}}});

    for (const auto &test_case : cases)
    {
        Instruction inst;
        std::string err;
        ASSERT_TRUE(V3Emitter::emitVNextContractInstruction(
            test_case.node_kind, test_case.fields, inst, err))
            << "unexpected map failure: " << err;
        EXPECT_EQ(test_case.expected_opcode, inst.opcode);

        Instruction decoded;
        ASSERT_TRUE(roundTripInstruction(inst, decoded, err)) << err;
        ValidationResult validation = scratchbird::sblr::v3::validateVNextOpcodeContract(decoded);
        EXPECT_TRUE(validation.ok) << validation.code << ": " << validation.message;
    }
}

TEST(SBLRVNextAstOpcodeExtensionContractTest, DeterministicRejectCodesAreStable)
{
    Instruction unknown_opcode;
    unknown_opcode.opcode = 0x60FE;
    unknown_opcode.flags = 0;
    unknown_opcode.payload = Value(Value::Object{});
    ValidationResult unknown_opcode_result =
        scratchbird::sblr::v3::validateVNextOpcodeContract(unknown_opcode);
    ASSERT_FALSE(unknown_opcode_result.ok);
    EXPECT_EQ("IRX_0403", unknown_opcode_result.code);

    Instruction enum_range;
    enum_range.opcode = static_cast<uint16_t>(Opcode::SBLR3_OP_DOC_PATH_FILTER);
    enum_range.flags = 0;
    enum_range.payload = Value(Value::Object{{"path_id", Value(static_cast<uint64_t>(1))},
                                             {"cmp", Value(static_cast<uint64_t>(42))},
                                             {"value_ref", Value(static_cast<uint64_t>(2))}});
    ValidationResult enum_result = scratchbird::sblr::v3::validateVNextOpcodeContract(enum_range);
    ASSERT_FALSE(enum_result.ok);
    EXPECT_EQ("IRX_0407", enum_result.code);

    Instruction mapped;
    std::string err;
    EXPECT_FALSE(V3Emitter::emitVNextContractInstruction(
        ASTKind::LiteralExpr,
        Value::Object{},
        mapped,
        err));
    EXPECT_NE(err.find("IRX_0401"), std::string::npos);

    EXPECT_FALSE(V3Emitter::emitVNextContractInstruction(
        ASTKind::AST_DOC_PATH_FILTER,
        Value::Object{{"path_expr", Value(static_cast<uint64_t>(1))}},
        mapped,
        err));
    EXPECT_NE(err.find("IRX_0402"), std::string::npos);

    ValidationResult rewrite_missing = scratchbird::sblr::v3::validateVNextRewriteEvidenceContract(
        Value::Object{{"rewrite_rule_id", Value(std::string("rw-1"))}});
    ASSERT_FALSE(rewrite_missing.ok);
    EXPECT_EQ("IRX_0405", rewrite_missing.code);
}
