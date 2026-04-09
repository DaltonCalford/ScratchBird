#include <gtest/gtest.h>
#include <cstring>

#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "scratchbird/sblr/v3_validator.h"

using namespace scratchbird::sblr::v3;

namespace {

Value::Object makeMinimalRetainedSymbolPayload() {
    Value::Object symbol;
    symbol["symbol_id"] = Value(uint64_t(1));
    symbol["scope_id"] = Value(uint64_t(1));
    symbol["symbol_class"] = Value("output_label_symbol");
    symbol["display_name_id"] = Value(uint64_t(1));
    symbol["ordinal"] = Value(uint64_t(0));
    symbol["symbol_origin_class"] = Value("local_user_authored_symbol");
    symbol["user_supplied"] = Value(true);
    symbol["quoted"] = Value(false);

    Value::Object scope;
    scope["scope_id"] = Value(uint64_t(1));
    scope["scope_class"] = Value("statement_root");
    scope["scope_path"] = Value("root");
    scope["ordinal"] = Value(uint64_t(0));

    Value::Object display_name;
    display_name["display_name_id"] = Value(uint64_t(1));
    display_name["display_name"] = Value("order_id");
    display_name["quoted"] = Value(false);

    Value::Object output_label;
    output_label["symbol_id"] = Value(uint64_t(1));
    output_label["position"] = Value(uint64_t(0));

    Value::Object source_order;
    source_order["scope_id"] = Value(uint64_t(1));
    source_order["symbol_ids"] = Value(Value::List{Value(uint64_t(1))});

    Value::Object payload;
    payload["format_version"] = Value(uint64_t(1));
    payload["root_opcode_symbol"] = Value("OP_STMT_DML_SELECT");
    payload["symbol_registry"] = Value(Value::List{Value(std::move(symbol))});
    payload["scope_registry"] = Value(Value::List{Value(std::move(scope))});
    payload["scope_parent_map"] = Value(Value::List{});
    payload["display_name_registry"] =
        Value(Value::List{Value(std::move(display_name))});
    payload["parameter_display_registry"] = Value(Value::List{});
    payload["output_label_registry"] =
        Value(Value::List{Value(std::move(output_label))});
    payload["placeholder_binding_registry"] = Value(Value::List{});
    payload["source_order_registry"] =
        Value(Value::List{Value(std::move(source_order))});
    return payload;
}

}  // namespace

TEST(SBLRV3Container, EncodeDecodeAndValidate) {
    Container c;
    std::memcpy(c.header.magic, "SBL3", 4);
    c.header.version_major = 3;
    c.header.version_minor = 0;
    c.header.version_patch = 0;
    c.header.flags = 0;
    c.header.timestamp_utc = 0;
    std::memset(c.header.module_id, 0, sizeof(c.header.module_id));

    c.metadata.module_name = "test";
    c.metadata.module_version = "0";
    c.metadata.dialect_id = 0;
    c.metadata.target_platform = 0;

    // Build a minimal bytecode stream: VERSION + END
    Buffer stream;
    Instruction ver;
    ver.opcode = static_cast<uint16_t>(Opcode::SBLR3_VERSION);
    ver.flags = 0;
    ver.payload = Value(Value::Bytes{0x03, 0x00, 0x00, 0x00, 0x00, 0x00});
    DecodeError err;
    ASSERT_TRUE(encodeInstructionWithSchema(ver, stream, err));

    Instruction end;
    end.opcode = static_cast<uint16_t>(Opcode::SBLR3_END);
    end.flags = 0;
    end.payload = Value(Value::Bytes{});
    ASSERT_TRUE(encodeInstructionWithSchema(end, stream, err));

    c.bytecode_stream = stream;
    c.retained_symbol_payload = makeMinimalRetainedSymbolPayload();

    std::vector<uint8_t> encoded;
    std::string e;
    ASSERT_TRUE(encodeContainer(c, encoded, e));

    Container decoded;
    ASSERT_TRUE(decodeContainer(encoded.data(), encoded.size(), decoded, e));
    ASSERT_FALSE(decoded.retained_symbol_payload.empty());
    auto it = decoded.retained_symbol_payload.find("format_version");
    ASSERT_NE(it, decoded.retained_symbol_payload.end());
    const auto* version = std::get_if<uint64_t>(&it->second.data);
    ASSERT_NE(version, nullptr);
    EXPECT_EQ(*version, 1u);

    std::string v_err;
    ASSERT_TRUE(validateContainer(encoded.data(), encoded.size(), v_err));
}

TEST(SBLRV3Container, DetailedValidationEmptyStream) {
    Container c;
    std::memcpy(c.header.magic, "SBL3", 4);
    c.header.version_major = 3;
    c.header.version_minor = 0;
    c.header.version_patch = 0;
    c.header.flags = 0;
    c.header.timestamp_utc = 0;
    std::memset(c.header.module_id, 0, sizeof(c.header.module_id));

    c.metadata.module_name = "test";
    c.metadata.module_version = "0";

    std::vector<uint8_t> encoded;
    std::string e;
    ASSERT_TRUE(encodeContainer(c, encoded, e));

    ValidationResult result = validateContainerDetailed(encoded.data(), encoded.size());
    ASSERT_FALSE(result.ok);
    EXPECT_EQ(result.code, "SBLR-E-0003");
}

TEST(SBLRV3Container, DetailedValidationIncludesCanonicalSymbol) {
    Container c;
    std::memcpy(c.header.magic, "SBL3", 4);
    c.header.version_major = 3;
    c.header.version_minor = 0;
    c.header.version_patch = 0;
    c.header.flags = 0;
    c.header.timestamp_utc = 0;
    std::memset(c.header.module_id, 0, sizeof(c.header.module_id));

    c.metadata.module_name = "test";
    c.metadata.module_version = "0";

    Buffer stream;
    Instruction end_only;
    end_only.opcode = static_cast<uint16_t>(Opcode::SBLR3_END);
    end_only.flags = 0;
    end_only.payload = Value(Value::Bytes{});
    DecodeError err;
    ASSERT_TRUE(encodeInstructionWithSchema(end_only, stream, err));
    c.bytecode_stream = stream;

    std::vector<uint8_t> encoded;
    std::string e;
    ASSERT_TRUE(encodeContainer(c, encoded, e));

    ValidationResult result = validateContainerDetailed(encoded.data(), encoded.size());
    ASSERT_FALSE(result.ok);
    EXPECT_EQ(result.code, "SBLR-E-0012");
    EXPECT_EQ(result.canonical_opcode_symbol, "OP_MOD_END");
}

TEST(SBLRV3Container, DetailedValidationRejectsDanglingRetainedSymbolScope) {
    Container c;
    std::memcpy(c.header.magic, "SBL3", 4);
    c.header.version_major = 3;
    c.header.version_minor = 0;
    c.header.version_patch = 0;
    c.header.flags = 0;
    c.header.timestamp_utc = 0;
    std::memset(c.header.module_id, 0, sizeof(c.header.module_id));

    c.metadata.module_name = "test";
    c.metadata.module_version = "0";

    Buffer stream;
    Instruction ver;
    ver.opcode = static_cast<uint16_t>(Opcode::SBLR3_VERSION);
    ver.flags = 0;
    ver.payload = Value(Value::Bytes{0x03, 0x00, 0x00, 0x00, 0x00, 0x00});
    DecodeError err;
    ASSERT_TRUE(encodeInstructionWithSchema(ver, stream, err));

    Instruction end;
    end.opcode = static_cast<uint16_t>(Opcode::SBLR3_END);
    end.flags = 0;
    end.payload = Value(Value::Bytes{});
    ASSERT_TRUE(encodeInstructionWithSchema(end, stream, err));
    c.bytecode_stream = stream;

    c.retained_symbol_payload = makeMinimalRetainedSymbolPayload();
    auto symbol_registry_it = c.retained_symbol_payload.find("symbol_registry");
    ASSERT_NE(symbol_registry_it, c.retained_symbol_payload.end());
    auto* symbol_registry = std::get_if<Value::List>(&symbol_registry_it->second.data);
    ASSERT_NE(symbol_registry, nullptr);
    auto* symbol = std::get_if<Value::Object>(&symbol_registry->front().data);
    ASSERT_NE(symbol, nullptr);
    (*symbol)["scope_id"] = Value(uint64_t(99));

    std::vector<uint8_t> encoded;
    std::string e;
    ASSERT_TRUE(encodeContainer(c, encoded, e));

    ValidationResult result = validateContainerDetailed(encoded.data(), encoded.size());
    ASSERT_FALSE(result.ok);
    EXPECT_EQ(result.code, "SBLR-E-0033");
}
