#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "scratchbird/parser/parser_v3.h"
#include "scratchbird/parser/v3_emitter.h"
#include "scratchbird/sblr/v3_payloads.h"

using scratchbird::parser::v3::Parser;
using scratchbird::parser::v3::V3Emitter;
using scratchbird::sblr::v3::Container;
using scratchbird::sblr::v3::DecodeError;
using scratchbird::sblr::v3::Instruction;
using scratchbird::sblr::v3::Opcode;
using scratchbird::sblr::v3::Value;

namespace {

struct EmittedRoot {
    uint16_t opcode = 0;
    Value payload;
};

bool emitRootFromSql(const std::string& sql, EmittedRoot& out, std::string& err) {
    Parser parser(sql);
    auto parse = parser.parseStatement();
    if (!parse.success() || parse.statement() == nullptr) {
        std::ostringstream oss;
        oss << "parse failure";
        for (const auto& e : parse.errors()) {
            oss << " | " << e.message;
        }
        err = oss.str();
        return false;
    }

    V3Emitter emitter(parser.stringPool());
    Container container;
    if (!emitter.emitStatementToContainer(parse.statement(), container, err)) {
        return false;
    }

    if (container.bytecode_stream.empty()) {
        err = "empty bytecode stream";
        return false;
    }

    size_t offset = 0;
    DecodeError decode_err;
    Instruction version_inst;
    if (!scratchbird::sblr::v3::decodeInstructionWithSchema(
            container.bytecode_stream.data(),
            container.bytecode_stream.size(),
            offset,
            version_inst,
            decode_err)) {
        err = decode_err.message;
        return false;
    }
    if (version_inst.opcode != static_cast<uint16_t>(Opcode::SBLR3_VERSION)) {
        err = "missing version header opcode";
        return false;
    }

    Instruction root_inst;
    if (!scratchbird::sblr::v3::decodeInstructionWithSchema(
            container.bytecode_stream.data(),
            container.bytecode_stream.size(),
            offset,
            root_inst,
            decode_err)) {
        err = decode_err.message;
        return false;
    }

    out.opcode = root_inst.opcode;
    out.payload = root_inst.payload;
    return true;
}

const Value::Object* payloadObject(const EmittedRoot& emitted) {
    return std::get_if<Value::Object>(&emitted.payload.data);
}

const std::string* payloadString(const Value::Object& payload, const char* key) {
    auto it = payload.find(key);
    if (it == payload.end()) {
        return nullptr;
    }
    return std::get_if<std::string>(&it->second.data);
}

const bool* payloadBool(const Value::Object& payload, const char* key) {
    auto it = payload.find(key);
    if (it == payload.end()) {
        return nullptr;
    }
    return std::get_if<bool>(&it->second.data);
}

}  // namespace

TEST(ParserV3UdrCompileEmitterContractTest, CompileEmbeddedPayloadFormsEmitDeterministicPayload) {
    const std::string statement_form =
        "UDR COMPILE EMBEDDED PAYLOAD PROFILE native FORMAT SQL_TEXT BYTES payload_1 SESSION_SIGNATURE sig_1";
    const std::string clause_form =
        "COMPILE EMBEDDED PAYLOAD native, SQL_TEXT, payload_1, sig_1";

    EmittedRoot statement_emit;
    EmittedRoot clause_emit;
    std::string err;
    ASSERT_TRUE(emitRootFromSql(statement_form, statement_emit, err)) << err;
    ASSERT_TRUE(emitRootFromSql(clause_form, clause_emit, err)) << err;

    EXPECT_EQ(static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_COMPILE_DISPATCH), statement_emit.opcode);
    EXPECT_EQ(static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_COMPILE_DISPATCH), clause_emit.opcode);

    const auto* statement_payload = payloadObject(statement_emit);
    const auto* clause_payload = payloadObject(clause_emit);
    ASSERT_NE(nullptr, statement_payload);
    ASSERT_NE(nullptr, clause_payload);

    const bool* statement_validate = payloadBool(*statement_payload, "validate_only");
    const bool* clause_validate = payloadBool(*clause_payload, "validate_only");
    const std::string* statement_profile = payloadString(*statement_payload, "profile_id");
    const std::string* clause_profile = payloadString(*clause_payload, "profile_id");
    const std::string* statement_format = payloadString(*statement_payload, "payload_format");
    const std::string* clause_format = payloadString(*clause_payload, "payload_format");
    const std::string* statement_bytes = payloadString(*statement_payload, "payload_bytes");
    const std::string* clause_bytes = payloadString(*clause_payload, "payload_bytes");
    const std::string* statement_sig = payloadString(*statement_payload, "session_signature");
    const std::string* clause_sig = payloadString(*clause_payload, "session_signature");

    ASSERT_NE(nullptr, statement_validate);
    ASSERT_NE(nullptr, clause_validate);
    ASSERT_NE(nullptr, statement_profile);
    ASSERT_NE(nullptr, clause_profile);
    ASSERT_NE(nullptr, statement_format);
    ASSERT_NE(nullptr, clause_format);
    ASSERT_NE(nullptr, statement_bytes);
    ASSERT_NE(nullptr, clause_bytes);
    ASSERT_NE(nullptr, statement_sig);
    ASSERT_NE(nullptr, clause_sig);

    EXPECT_FALSE(*statement_validate);
    EXPECT_FALSE(*clause_validate);
    EXPECT_EQ("native", *statement_profile);
    EXPECT_EQ(*statement_profile, *clause_profile);
    EXPECT_EQ("SQL_TEXT", *statement_format);
    EXPECT_EQ(*statement_format, *clause_format);
    EXPECT_EQ("payload_1", *statement_bytes);
    EXPECT_EQ(*statement_bytes, *clause_bytes);
    EXPECT_EQ("sig_1", *statement_sig);
    EXPECT_EQ(*statement_sig, *clause_sig);
}

TEST(ParserV3UdrCompileEmitterContractTest, ValidateEmbeddedPayloadSetsValidateFlag) {
    const std::string validate_form =
        "VALIDATE EMBEDDED PAYLOAD native, SQL_TEXT, payload_2, sig_2";

    EmittedRoot emit;
    std::string err;
    ASSERT_TRUE(emitRootFromSql(validate_form, emit, err)) << err;
    EXPECT_EQ(static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_COMPILE_DISPATCH), emit.opcode);

    const auto* payload = payloadObject(emit);
    ASSERT_NE(nullptr, payload);
    const bool* validate_only = payloadBool(*payload, "validate_only");
    ASSERT_NE(nullptr, validate_only);
    EXPECT_TRUE(*validate_only);
}

TEST(ParserV3UdrCompileEmitterContractTest, SqlTemplateFormsEmitDeterministicPayload) {
    const std::string statement_form =
        "UDR COMPILE SQL TEMPLATE TEMPLATE_ID tpl_a SQL_TEXT 'SELECT 1' PROFILE native SESSION_SIGNATURE sig_t";
    const std::string clause_form =
        "COMPILE SQL TEMPLATE tpl_a USING 'SELECT 1' PROFILE native SIGNATURE sig_t";

    EmittedRoot statement_emit;
    EmittedRoot clause_emit;
    std::string err;
    ASSERT_TRUE(emitRootFromSql(statement_form, statement_emit, err)) << err;
    ASSERT_TRUE(emitRootFromSql(clause_form, clause_emit, err)) << err;

    EXPECT_EQ(static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_EMBEDDED_SQL_COMPILE), statement_emit.opcode);
    EXPECT_EQ(static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_EMBEDDED_SQL_COMPILE), clause_emit.opcode);

    const auto* statement_payload = payloadObject(statement_emit);
    const auto* clause_payload = payloadObject(clause_emit);
    ASSERT_NE(nullptr, statement_payload);
    ASSERT_NE(nullptr, clause_payload);

    const bool* statement_validate = payloadBool(*statement_payload, "validate_only");
    const bool* clause_validate = payloadBool(*clause_payload, "validate_only");
    const std::string* statement_template = payloadString(*statement_payload, "template_id");
    const std::string* clause_template = payloadString(*clause_payload, "template_id");
    const std::string* statement_sql = payloadString(*statement_payload, "sql_text");
    const std::string* clause_sql = payloadString(*clause_payload, "sql_text");
    const std::string* statement_profile = payloadString(*statement_payload, "profile_id");
    const std::string* clause_profile = payloadString(*clause_payload, "profile_id");
    const std::string* statement_sig = payloadString(*statement_payload, "session_signature");
    const std::string* clause_sig = payloadString(*clause_payload, "session_signature");

    ASSERT_NE(nullptr, statement_validate);
    ASSERT_NE(nullptr, clause_validate);
    ASSERT_NE(nullptr, statement_template);
    ASSERT_NE(nullptr, clause_template);
    ASSERT_NE(nullptr, statement_sql);
    ASSERT_NE(nullptr, clause_sql);
    ASSERT_NE(nullptr, statement_profile);
    ASSERT_NE(nullptr, clause_profile);
    ASSERT_NE(nullptr, statement_sig);
    ASSERT_NE(nullptr, clause_sig);

    EXPECT_FALSE(*statement_validate);
    EXPECT_FALSE(*clause_validate);
    EXPECT_EQ("tpl_a", *statement_template);
    EXPECT_EQ(*statement_template, *clause_template);
    EXPECT_EQ("SELECT 1", *statement_sql);
    EXPECT_EQ(*statement_sql, *clause_sql);
    EXPECT_EQ("native", *statement_profile);
    EXPECT_EQ(*statement_profile, *clause_profile);
    EXPECT_EQ("sig_t", *statement_sig);
    EXPECT_EQ(*statement_sig, *clause_sig);
}

TEST(ParserV3UdrCompileEmitterContractTest, ValidateSqlTemplateSetsValidateFlag) {
    const std::string validate_form =
        "UDR VALIDATE SQL TEMPLATE TEMPLATE_ID tpl_b SQL_TEXT 'SELECT 2' PROFILE native SESSION_SIGNATURE sig_u";

    EmittedRoot emit;
    std::string err;
    ASSERT_TRUE(emitRootFromSql(validate_form, emit, err)) << err;
    EXPECT_EQ(static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_EMBEDDED_SQL_COMPILE), emit.opcode);

    const auto* payload = payloadObject(emit);
    ASSERT_NE(nullptr, payload);
    const bool* validate_only = payloadBool(*payload, "validate_only");
    ASSERT_NE(nullptr, validate_only);
    EXPECT_TRUE(*validate_only);
}
