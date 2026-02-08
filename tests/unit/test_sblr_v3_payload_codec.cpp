#include <gtest/gtest.h>

#include "scratchbird/sblr/v3_payloads.h"

using namespace scratchbird::sblr::v3;

TEST(SBLRV3PayloadCodec, EncodeDecodeVarLoad) {
    Instruction inst;
    inst.opcode = static_cast<uint16_t>(Opcode::SBLR3_VAR_LOAD);
    inst.flags = 0;

    Value::Object var_ref;
    var_ref["name"] = Value(std::string("x"));

    Value::Object payload;
    payload["var"] = Value(var_ref);
    inst.payload = Value(payload);

    Buffer out;
    DecodeError err;
    ASSERT_TRUE(encodeInstructionWithSchema(inst, out, err)) << err.message;

    size_t off = 0;
    Instruction decoded;
    ASSERT_TRUE(decodeInstructionWithSchema(out.data(), out.size(), off, decoded, err)) << err.message;
    ASSERT_EQ(decoded.opcode, inst.opcode);

    auto* obj = std::get_if<Value::Object>(&decoded.payload.data);
    ASSERT_NE(obj, nullptr);
    auto it = obj->find("var");
    ASSERT_NE(it, obj->end());
    auto* vobj = std::get_if<Value::Object>(&it->second.data);
    ASSERT_NE(vobj, nullptr);
    auto name_it = vobj->find("name");
    ASSERT_NE(name_it, vobj->end());
    auto* name = std::get_if<std::string>(&name_it->second.data);
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(*name, "x");
}
