#include <gtest/gtest.h>
#include <cstring>

#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "scratchbird/sblr/v3_validator.h"

using namespace scratchbird::sblr::v3;

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

    std::vector<uint8_t> encoded;
    std::string e;
    ASSERT_TRUE(encodeContainer(c, encoded, e));

    Container decoded;
    ASSERT_TRUE(decodeContainer(encoded.data(), encoded.size(), decoded, e));

    std::string v_err;
    ASSERT_TRUE(validateContainer(encoded.data(), encoded.size(), v_err));
}
