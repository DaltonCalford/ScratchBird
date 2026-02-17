#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "scratchbird/sblr/v3_codec.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "scratchbird/sblr/v3_validator.h"

using scratchbird::sblr::v3::DecodeError;
using scratchbird::sblr::v3::FieldType;
using scratchbird::sblr::v3::Instruction;
using scratchbird::sblr::v3::Opcode;
using scratchbird::sblr::v3::SchemaDef;
using scratchbird::sblr::v3::ValidationResult;
using scratchbird::sblr::v3::Value;

namespace
{
auto makeInstruction(uint16_t opcode, Value::Object payload) -> Instruction
{
    Instruction inst;
    inst.opcode = opcode;
    inst.flags = 0;
    inst.payload = Value(std::move(payload));
    return inst;
}

void writeLe32(uint32_t value, std::vector<uint8_t> &buffer, size_t offset)
{
    ASSERT_GE(buffer.size(), offset + 4);
    buffer[offset + 0] = static_cast<uint8_t>(value & 0xFF);
    buffer[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buffer[offset + 2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    buffer[offset + 3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}
} // namespace

TEST(SBLRVNextPayloadSchemaMappingContractTest, OpcodeToSchemaMappingsAreDeterministic)
{
    struct MappingCase
    {
        uint16_t opcode;
        const char *schema_name;
    };
    const std::array<MappingCase, 7> mapping = {{
        {static_cast<uint16_t>(Opcode::SBLR3_OP_DOC_PATH_FILTER), "SCHEMA_DOC_PATH_FILTER"},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_TS_BUCKET_AGG), "SCHEMA_TS_BUCKET_AGG"},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_COL_SCAN), "SCHEMA_COL_SCAN"},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_SEARCH_DSL_EVAL), "SCHEMA_SEARCH_DSL_EVAL"},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_VECTOR_ANN), "SCHEMA_VECTOR_ANN"},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_HYBRID_BRIDGE_EXCHANGE), "SCHEMA_HYBRID_BRIDGE_EXCHANGE"},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_HYBRID_BRIDGE_MATERIALIZE), "SCHEMA_HYBRID_BRIDGE_MATERIALIZE"},
    }};

    for (const auto &entry : mapping)
    {
        const SchemaDef *schema = scratchbird::sblr::v3::schemaForOpcode(entry.opcode);
        ASSERT_NE(nullptr, schema) << "missing schema for opcode " << entry.opcode;
        EXPECT_EQ(std::string(entry.schema_name), schema->name);
    }
}

TEST(SBLRVNextPayloadSchemaMappingContractTest, SchemaFieldContractsMatchSpecification)
{
    auto assertField = [](const SchemaDef *schema,
                          size_t index,
                          const char *field_name,
                          FieldType type) {
        ASSERT_NE(nullptr, schema);
        ASSERT_LT(index, schema->fields.size());
        EXPECT_EQ(std::string(field_name), schema->fields[index].name);
        EXPECT_EQ(type, schema->fields[index].type);
    };

    const SchemaDef *doc = scratchbird::sblr::v3::lookupSchema("SCHEMA_DOC_PATH_FILTER");
    ASSERT_NE(nullptr, doc);
    ASSERT_EQ(3u, doc->fields.size());
    assertField(doc, 0, "path_id", FieldType::U32);
    assertField(doc, 1, "cmp", FieldType::U8);
    assertField(doc, 2, "value_ref", FieldType::U32);

    const SchemaDef *bucket = scratchbird::sblr::v3::lookupSchema("SCHEMA_TS_BUCKET_AGG");
    ASSERT_NE(nullptr, bucket);
    ASSERT_EQ(3u, bucket->fields.size());
    assertField(bucket, 0, "bucket_ns", FieldType::U64);
    assertField(bucket, 1, "agg_count", FieldType::U16);
    assertField(bucket, 2, "agg_refs", FieldType::BYTES);

    const SchemaDef *scan = scratchbird::sblr::v3::lookupSchema("SCHEMA_COL_SCAN");
    ASSERT_NE(nullptr, scan);
    ASSERT_EQ(3u, scan->fields.size());
    assertField(scan, 0, "table_id", FieldType::U32);
    assertField(scan, 1, "proj_bitmap", FieldType::BYTES);
    assertField(scan, 2, "predicate_bitmap", FieldType::BYTES);
}

TEST(SBLRVNextPayloadSchemaMappingContractTest, SchemaRoundTripAndEnumValidation)
{
    std::vector<Instruction> instructions;
    instructions.push_back(makeInstruction(
        static_cast<uint16_t>(Opcode::SBLR3_OP_DOC_PATH_FILTER),
        Value::Object{{"path_id", Value(static_cast<uint64_t>(11))},
                      {"cmp", Value(static_cast<uint64_t>(0))},
                      {"value_ref", Value(static_cast<uint64_t>(14))}}));
    instructions.push_back(makeInstruction(
        static_cast<uint16_t>(Opcode::SBLR3_OP_TS_BUCKET_AGG),
        Value::Object{{"bucket_ns", Value(static_cast<uint64_t>(120'000'000'000ULL))},
                      {"agg_count", Value(static_cast<uint64_t>(2))},
                      {"agg_refs", Value(Value::Bytes{0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00})}}));
    instructions.push_back(makeInstruction(
        static_cast<uint16_t>(Opcode::SBLR3_OP_COL_SCAN),
        Value::Object{{"table_id", Value(static_cast<uint64_t>(22))},
                      {"proj_bitmap", Value(Value::Bytes{0x0F})},
                      {"predicate_bitmap", Value(Value::Bytes{0xAA, 0xBB})}}));
    instructions.push_back(makeInstruction(
        static_cast<uint16_t>(Opcode::SBLR3_OP_SEARCH_DSL_EVAL),
        Value::Object{{"dsl_blob_ref", Value(static_cast<uint64_t>(5001))},
                      {"scorer_id", Value(static_cast<uint64_t>(1))}}));
    instructions.push_back(makeInstruction(
        static_cast<uint16_t>(Opcode::SBLR3_OP_VECTOR_ANN),
        Value::Object{{"index_id", Value(static_cast<uint64_t>(99))},
                      {"metric", Value(static_cast<uint64_t>(2))},
                      {"topk", Value(static_cast<uint64_t>(10))},
                      {"ef", Value(static_cast<uint64_t>(64))}}));
    instructions.push_back(makeInstruction(
        static_cast<uint16_t>(Opcode::SBLR3_OP_HYBRID_BRIDGE_EXCHANGE),
        Value::Object{{"src_track", Value(static_cast<uint64_t>(1))},
                      {"dst_track", Value(static_cast<uint64_t>(2))},
                      {"mode", Value(static_cast<uint64_t>(3))}}));
    instructions.push_back(makeInstruction(
        static_cast<uint16_t>(Opcode::SBLR3_OP_HYBRID_BRIDGE_MATERIALIZE),
        Value::Object{{"buffer_class", Value(static_cast<uint64_t>(2))},
                      {"row_shape_ref", Value(static_cast<uint64_t>(77))}}));

    for (const auto &inst : instructions)
    {
        scratchbird::sblr::v3::Buffer encoded;
        DecodeError err;
        ASSERT_TRUE(scratchbird::sblr::v3::encodeInstructionWithSchema(inst, encoded, err))
            << err.message;

        Instruction decoded;
        size_t offset = 0;
        ASSERT_TRUE(scratchbird::sblr::v3::decodeInstructionWithSchema(
            encoded.data(), encoded.size(), offset, decoded, err))
            << err.message;
        ASSERT_EQ(encoded.size(), offset);

        ValidationResult contract = scratchbird::sblr::v3::validateVNextOpcodeContract(decoded);
        EXPECT_TRUE(contract.ok) << contract.code << ": " << contract.message;
    }
}

TEST(SBLRVNextPayloadSchemaMappingContractTest, EncodedPayloadContractRejectsLengthMismatch)
{
    Instruction valid = makeInstruction(
        static_cast<uint16_t>(Opcode::SBLR3_OP_DOC_PATH_FILTER),
        Value::Object{{"path_id", Value(static_cast<uint64_t>(11))},
                      {"cmp", Value(static_cast<uint64_t>(0))},
                      {"value_ref", Value(static_cast<uint64_t>(14))}});

    scratchbird::sblr::v3::Buffer encoded;
    DecodeError err;
    ASSERT_TRUE(scratchbird::sblr::v3::encodeInstructionWithSchema(valid, encoded, err));
    ASSERT_GE(encoded.size(), 8u);

    // Corrupt payload length header to force deterministic length-reject path.
    writeLe32(static_cast<uint32_t>(encoded.size()), encoded, 4);
    ValidationResult invalid =
        scratchbird::sblr::v3::validateVNextEncodedInstructionContract(encoded.data(), encoded.size());
    ASSERT_FALSE(invalid.ok);
    EXPECT_EQ("IRX_0404", invalid.code);
}

TEST(SBLRVNextPayloadSchemaMappingContractTest, EncodedPayloadContractRejectsUnknownOpcodeInRange)
{
    std::vector<uint8_t> encoded(8, 0);
    encoded[0] = 0xFE; // opcode 0x60FE, unknown in range
    encoded[1] = 0x60;

    ValidationResult invalid =
        scratchbird::sblr::v3::validateVNextEncodedInstructionContract(encoded.data(), encoded.size());
    ASSERT_FALSE(invalid.ok);
    EXPECT_EQ("IRX_0403", invalid.code);
}

