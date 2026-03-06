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
    const std::array<MappingCase, 9> mapping = {{
        {static_cast<uint16_t>(Opcode::SBLR3_OP_DOC_PATH_FILTER), "SCHEMA_DOC_PATH_FILTER"},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_TS_BUCKET_AGG), "SCHEMA_TS_BUCKET_AGG"},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_COL_SCAN), "SCHEMA_COL_SCAN"},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_SEARCH_DSL_EVAL), "SCHEMA_SEARCH_DSL_EVAL"},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_VECTOR_ANN), "SCHEMA_VECTOR_ANN"},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_HYBRID_BRIDGE_EXCHANGE), "SCHEMA_HYBRID_BRIDGE_EXCHANGE"},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_HYBRID_BRIDGE_MATERIALIZE), "SCHEMA_HYBRID_BRIDGE_MATERIALIZE"},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_COMPILE_DISPATCH), "SCHEMA_UDR_COMPILE_DISPATCH"},
        {static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_EMBEDDED_SQL_COMPILE), "SCHEMA_UDR_EMBEDDED_SQL_COMPILE"},
    }};

    for (const auto &entry : mapping)
    {
        const SchemaDef *schema = scratchbird::sblr::v3::schemaForOpcode(entry.opcode);
        ASSERT_NE(nullptr, schema) << "missing schema for opcode " << entry.opcode;
        EXPECT_EQ(std::string(entry.schema_name), schema->name);
    }
}

TEST(SBLRVNextPayloadSchemaMappingContractTest, BridgeOpcodeRangeMappingsAreDeterministic)
{
    struct MappingCase
    {
        uint16_t opcode;
        const char *schema_name;
    };

    auto expectRange = [](uint16_t start, uint16_t end, const char *schema_name) {
        for (uint16_t opcode = start; opcode <= end; ++opcode)
        {
            const SchemaDef *schema = scratchbird::sblr::v3::schemaForOpcode(opcode);
            ASSERT_NE(nullptr, schema) << "missing schema for bridge opcode " << opcode;
            EXPECT_EQ(std::string(schema_name), schema->name)
                << "unexpected schema for bridge opcode " << opcode;
        }
    };

    expectRange(0x6101, 0x6107, "SCHEMA_CONTROL_COMMAND");
    expectRange(0x6109, 0x610C, "SCHEMA_DDL_CREATE_DOMAIN");
    expectRange(0x610D, 0x6113, "SCHEMA_DDL_ALTER_INDEX");
    expectRange(0x6114, 0x611E, "SCHEMA_CONTROL_COMMAND");
    expectRange(0x611F, 0x6139, "SCHEMA_MULTI_MODEL_QUERY");
    expectRange(0x613A, 0x615F, "SCHEMA_CONTROL_COMMAND");

    const std::array<MappingCase, 20> remote_fdw_bridge_mapping = {{
        {static_cast<uint16_t>(Opcode::SBLR3_ALTER_FOREIGN_DATA_WRAPPER),
         "SCHEMA_DDL_CREATE_FOREIGN_DATA_WRAPPER"},
        {static_cast<uint16_t>(Opcode::SBLR3_DROP_FOREIGN_DATA_WRAPPER), "SCHEMA_DDL_DROP"},
        {static_cast<uint16_t>(Opcode::SBLR3_ALTER_FOREIGN_SERVER),
         "SCHEMA_DDL_CREATE_FOREIGN_SERVER"},
        {static_cast<uint16_t>(Opcode::SBLR3_ALTER_FOREIGN_TABLE),
         "SCHEMA_DDL_CREATE_FOREIGN_TABLE"},
        {static_cast<uint16_t>(Opcode::SBLR3_ALTER_USER_MAPPING),
         "SCHEMA_DDL_CREATE_USER_MAPPING"},
        {static_cast<uint16_t>(Opcode::SBLR3_IMPORT_FOREIGN_SCHEMA), "SCHEMA_CONTROL_COMMAND"},
        {static_cast<uint16_t>(Opcode::SBLR3_ANALYZE_REMOTE_SERVER), "SCHEMA_CONTROL_COMMAND"},
        {static_cast<uint16_t>(Opcode::SBLR3_REFRESH_REMOTE_METADATA), "SCHEMA_CONTROL_COMMAND"},
        {static_cast<uint16_t>(Opcode::SBLR3_SHOW_REMOTE_CAPABILITIES), "SCHEMA_CONTROL_COMMAND"},
        {static_cast<uint16_t>(Opcode::SBLR3_SHOW_REMOTE_OBJECTS), "SCHEMA_CONTROL_COMMAND"},
        {static_cast<uint16_t>(Opcode::SBLR3_SHOW_REMOTE_COLUMNS), "SCHEMA_CONTROL_COMMAND"},
        {static_cast<uint16_t>(Opcode::SBLR3_SHOW_REMOTE_STATISTICS), "SCHEMA_CONTROL_COMMAND"},
        {static_cast<uint16_t>(Opcode::SBLR3_EXECUTE_REMOTE), "SCHEMA_CONTROL_COMMAND"},
        {static_cast<uint16_t>(Opcode::SBLR3_PREPARE_REMOTE), "SCHEMA_CONTROL_COMMAND"},
        {static_cast<uint16_t>(Opcode::SBLR3_EXECUTE_REMOTE_PREPARED), "SCHEMA_CONTROL_COMMAND"},
        {static_cast<uint16_t>(Opcode::SBLR3_DEALLOCATE_REMOTE_PREPARED), "SCHEMA_CONTROL_COMMAND"},
        {static_cast<uint16_t>(Opcode::SBLR3_BEGIN_REMOTE_TRANSACTION), "SCHEMA_CONTROL_COMMAND"},
        {static_cast<uint16_t>(Opcode::SBLR3_COMMIT_REMOTE_TRANSACTION), "SCHEMA_CONTROL_COMMAND"},
        {static_cast<uint16_t>(Opcode::SBLR3_ROLLBACK_REMOTE_TRANSACTION), "SCHEMA_CONTROL_COMMAND"},
        {static_cast<uint16_t>(Opcode::SBLR3_SHOW_REMOTE_SESSION_STATE), "SCHEMA_CONTROL_COMMAND"},
    }};
    for (const auto &entry : remote_fdw_bridge_mapping)
    {
        const SchemaDef *schema = scratchbird::sblr::v3::schemaForOpcode(entry.opcode);
        ASSERT_NE(nullptr, schema) << "missing schema for bridge opcode " << entry.opcode;
        EXPECT_EQ(std::string(entry.schema_name), schema->name)
            << "unexpected schema for bridge opcode " << entry.opcode;
    }

    const SchemaDef *create_db_emulated =
        scratchbird::sblr::v3::schemaForOpcode(static_cast<uint16_t>(Opcode::SBLR3_CREATE_DATABASE_EMULATED));
    ASSERT_NE(nullptr, create_db_emulated);
    EXPECT_EQ(std::string("SCHEMA_DDL_CREATE_DATABASE"), create_db_emulated->name);
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

    const SchemaDef *control = scratchbird::sblr::v3::lookupSchema("SCHEMA_CONTROL_COMMAND");
    ASSERT_NE(nullptr, control);
    ASSERT_EQ(6u, control->fields.size());
    assertField(control, 0, "action", FieldType::U8);
    assertField(control, 1, "object_path", FieldType::OPT);
    assertField(control, 2, "object_name", FieldType::OPT);
    assertField(control, 3, "value", FieldType::OPT);
    assertField(control, 4, "payload", FieldType::OPT);
    assertField(control, 5, "options", FieldType::SCHEMA);

    const SchemaDef *multi_model = scratchbird::sblr::v3::lookupSchema("SCHEMA_MULTI_MODEL_QUERY");
    ASSERT_NE(nullptr, multi_model);
    ASSERT_EQ(6u, multi_model->fields.size());
    assertField(multi_model, 0, "action", FieldType::U8);
    assertField(multi_model, 1, "namespace_path", FieldType::OPT);
    assertField(multi_model, 2, "source_path", FieldType::OPT);
    assertField(multi_model, 3, "query_expr", FieldType::OPT);
    assertField(multi_model, 4, "document", FieldType::OPT);
    assertField(multi_model, 5, "options", FieldType::SCHEMA);

    const SchemaDef *create_tablespace =
        scratchbird::sblr::v3::lookupSchema("SCHEMA_DDL_CREATE_TABLESPACE");
    ASSERT_NE(nullptr, create_tablespace);
    ASSERT_EQ(8u, create_tablespace->fields.size());
    assertField(create_tablespace, 0, "flags", FieldType::U64);
    assertField(create_tablespace, 1, "path", FieldType::SCHEMA_PATH);
    assertField(create_tablespace, 2, "name", FieldType::IDENT);
    assertField(create_tablespace, 3, "location", FieldType::STRING);
    assertField(create_tablespace, 4, "autoextend_enabled", FieldType::BOOL);
    assertField(create_tablespace, 5, "autoextend_size_mb", FieldType::U32);
    assertField(create_tablespace, 6, "max_size_mb", FieldType::U32);
    assertField(create_tablespace, 7, "prealloc_pages", FieldType::U32);

    const SchemaDef *create_index =
        scratchbird::sblr::v3::lookupSchema("SCHEMA_DDL_CREATE_INDEX");
    ASSERT_NE(nullptr, create_index);
    ASSERT_EQ(9u, create_index->fields.size());
    assertField(create_index, 7, "tablespace", FieldType::OPT);
    assertField(create_index, 8, "options", FieldType::SCHEMA);

    const SchemaDef *tablespace_alteration =
        scratchbird::sblr::v3::lookupSchema("TABLESPACE_ALTERATION");
    ASSERT_NE(nullptr, tablespace_alteration);
    ASSERT_EQ(4u, tablespace_alteration->fields.size());
    assertField(tablespace_alteration, 0, "action", FieldType::U8);
    assertField(tablespace_alteration, 1, "autoextend_enabled", FieldType::OPT);
    assertField(tablespace_alteration, 2, "size_mb", FieldType::OPT);
    assertField(tablespace_alteration, 3, "new_name", FieldType::OPT);

    const SchemaDef *alter_tablespace =
        scratchbird::sblr::v3::lookupSchema("SCHEMA_DDL_ALTER_TABLESPACE");
    ASSERT_NE(nullptr, alter_tablespace);
    ASSERT_EQ(3u, alter_tablespace->fields.size());
    assertField(alter_tablespace, 0, "tablespace", FieldType::SCHEMA_PATH);
    assertField(alter_tablespace, 1, "alterations", FieldType::LIST);
    assertField(alter_tablespace, 2, "options", FieldType::OPT);

    const SchemaDef *alter_table_set_tablespace =
        scratchbird::sblr::v3::lookupSchema("SCHEMA_DDL_ALTER_TABLE_SET_TABLESPACE");
    ASSERT_NE(nullptr, alter_table_set_tablespace);
    ASSERT_EQ(3u, alter_table_set_tablespace->fields.size());
    assertField(alter_table_set_tablespace, 0, "table", FieldType::SCHEMA_PATH);
    assertField(alter_table_set_tablespace, 1, "tablespace", FieldType::SCHEMA_PATH);
    assertField(alter_table_set_tablespace, 2, "online", FieldType::BOOL);

    const SchemaDef *udr_compile =
        scratchbird::sblr::v3::lookupSchema("SCHEMA_UDR_COMPILE_DISPATCH");
    ASSERT_NE(nullptr, udr_compile);
    ASSERT_EQ(14u, udr_compile->fields.size());
    assertField(udr_compile, 0, "validate_only", FieldType::BOOL);
    assertField(udr_compile, 1, "profile_id", FieldType::STRING);
    assertField(udr_compile, 2, "payload_format", FieldType::STRING);
    assertField(udr_compile, 3, "payload_bytes", FieldType::STRING);
    assertField(udr_compile, 4, "session_signature", FieldType::STRING);
    assertField(udr_compile, 5, "artifact_preference", FieldType::OPT);
    assertField(udr_compile, 6, "target_triples", FieldType::OPT);
    assertField(udr_compile, 7, "host_api_abi_version", FieldType::OPT);
    assertField(udr_compile, 8, "optimization_level", FieldType::OPT);
    assertField(udr_compile, 9, "allow_interpreter_fallback", FieldType::OPT);
    assertField(udr_compile, 10, "native_execution_mode", FieldType::OPT);
    assertField(udr_compile, 11, "native_artifact_udr_enabled", FieldType::OPT);
    assertField(udr_compile, 12, "native_target_triples", FieldType::OPT);
    assertField(udr_compile, 13, "native_host_api_abi_version", FieldType::OPT);

    const SchemaDef *udr_template =
        scratchbird::sblr::v3::lookupSchema("SCHEMA_UDR_EMBEDDED_SQL_COMPILE");
    ASSERT_NE(nullptr, udr_template);
    ASSERT_EQ(14u, udr_template->fields.size());
    assertField(udr_template, 0, "validate_only", FieldType::BOOL);
    assertField(udr_template, 1, "template_id", FieldType::STRING);
    assertField(udr_template, 2, "sql_text", FieldType::STRING);
    assertField(udr_template, 3, "profile_id", FieldType::STRING);
    assertField(udr_template, 4, "session_signature", FieldType::STRING);
    assertField(udr_template, 5, "artifact_preference", FieldType::OPT);
    assertField(udr_template, 6, "target_triples", FieldType::OPT);
    assertField(udr_template, 7, "host_api_abi_version", FieldType::OPT);
    assertField(udr_template, 8, "optimization_level", FieldType::OPT);
    assertField(udr_template, 9, "allow_interpreter_fallback", FieldType::OPT);
    assertField(udr_template, 10, "native_execution_mode", FieldType::OPT);
    assertField(udr_template, 11, "native_artifact_udr_enabled", FieldType::OPT);
    assertField(udr_template, 12, "native_target_triples", FieldType::OPT);
    assertField(udr_template, 13, "native_host_api_abi_version", FieldType::OPT);
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
    instructions.push_back(makeInstruction(
        static_cast<uint16_t>(Opcode::SBLR3_CREATE_TABLESPACE),
        Value::Object{{"flags", Value(static_cast<uint64_t>(0))},
                      {"path", Value(Value::List{Value(std::string("ts_hot"))})},
                      {"name", Value(std::string("ts_hot"))},
                      {"location", Value(std::string("/tmp/emu/location"))},
                      {"autoextend_enabled", Value(true)},
                      {"autoextend_size_mb", Value(static_cast<uint64_t>(64))},
                      {"max_size_mb", Value(static_cast<uint64_t>(0))},
                      {"prealloc_pages", Value(static_cast<uint64_t>(0))}}));
    instructions.push_back(makeInstruction(
        static_cast<uint16_t>(Opcode::SBLR3_ALTER_TABLESPACE),
        Value::Object{
            {"tablespace", Value(Value::List{Value(std::string("ts_hot"))})},
            {"alterations", Value(Value::List{
                                Value(Value::Object{
                                    {"action", Value(static_cast<uint64_t>(0))},
                                    {"autoextend_enabled", Value(false)}}),
                                Value(Value::Object{
                                    {"action", Value(static_cast<uint64_t>(1))},
                                    {"size_mb", Value(static_cast<uint64_t>(128))}}),
                                Value(Value::Object{
                                    {"action", Value(static_cast<uint64_t>(3))},
                                    {"new_name", Value(std::string("ts_cold"))}}),
                            })},
        }));
    instructions.push_back(makeInstruction(
        static_cast<uint16_t>(Opcode::SBLR3_ALTER_TABLE_SET_TABLESPACE),
        Value::Object{{"table", Value(Value::List{Value(std::string("public")),
                                                  Value(std::string("users"))})},
                      {"tablespace", Value(Value::List{Value(std::string("ts_hot"))})},
                      {"online", Value(false)}}));
    instructions.push_back(makeInstruction(
        static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_COMPILE_DISPATCH),
        Value::Object{{"validate_only", Value(false)},
                      {"profile_id", Value(std::string("native"))},
                      {"payload_format", Value(std::string("SQL_TEXT"))},
                      {"payload_bytes", Value(std::string("payload_1"))},
                      {"session_signature", Value(std::string("sig_1"))}}));
    instructions.push_back(makeInstruction(
        static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_EMBEDDED_SQL_COMPILE),
        Value::Object{{"validate_only", Value(true)},
                      {"template_id", Value(std::string("tpl_1"))},
                      {"sql_text", Value(std::string("SELECT 1"))},
                      {"profile_id", Value(std::string("native"))},
                      {"session_signature", Value(std::string("sig_2"))}}));
    instructions.push_back(makeInstruction(
        static_cast<uint16_t>(Opcode::SBLR3_SESSION_RESET),
        Value::Object{{"action", Value(static_cast<uint64_t>(1))},
                      {"object_name", Value(std::string("session"))},
                      {"options", Value(Value::Object{{"scope", Value(std::string("default"))}})}}));
    instructions.push_back(makeInstruction(
        static_cast<uint16_t>(Opcode::SBLR3_CQL_BATCH),
        Value::Object{{"action", Value(static_cast<uint64_t>(2))},
                      {"namespace_path", Value(Value::List{Value(std::string("users")),
                                                           Value(std::string("public"))})},
                      {"document", Value(Value::Bytes{0x43, 0x51, 0x4C})},
                      {"options", Value(Value::Object{{"consistency", Value(std::string("quorum"))}})}}));

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
