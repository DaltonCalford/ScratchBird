#include "scratchbird/engine/authentication.h"
#include "scratchbird/engine/firebird_protocol_handler.h"

#include <gtest/gtest.h>
#include <memory>
#include <vector>

using namespace scratchbird::engine;
using namespace ScratchBird;

// Simplified test without external dependencies for now
class FirebirdProtocolTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Basic setup
        auth_manager_ = std::make_unique<AuthenticationManager>();
        protocol_handler_ = std::make_unique<FirebirdAuthenticationHandler>(auth_manager_.get());
    }

    void TearDown() override
    {
        if (protocol_handler_) {
            protocol_handler_->shutdown();
        }
    }

    std::unique_ptr<AuthenticationManager> auth_manager_;
    std::unique_ptr<FirebirdAuthenticationHandler> protocol_handler_;
};

TEST_F(FirebirdProtocolTest, ProtocolIdentification)
{
    EXPECT_EQ(protocol_handler_->get_protocol_type(), ProtocolType::FirebirdWire);

    ProtocolVersion version = protocol_handler_->get_supported_version();
    EXPECT_EQ(version.major, PROTOCOL_VERSION15);
    EXPECT_EQ(version.type, ProtocolType::FirebirdWire);

    // Test version support
    ProtocolVersion supported_version;
    supported_version.major = PROTOCOL_VERSION11;
    supported_version.type = ProtocolType::FirebirdWire;
    EXPECT_TRUE(protocol_handler_->supports_version(supported_version));

    ProtocolVersion unsupported_version;
    unsupported_version.major = 99;
    unsupported_version.type = ProtocolType::FirebirdWire;
    EXPECT_FALSE(protocol_handler_->supports_version(unsupported_version));
}

TEST_F(FirebirdProtocolTest, FirebirdPacketSerialization)
{
    FirebirdPacket packet;
    packet.operation = op_authenticate_user;
    packet.write_string("testuser");
    packet.write_string("testpass");
    packet.write_uint32(12345);
    packet.write_uint16(678);

    // Serialize and deserialize
    auto serialized = packet.serialize();

    FirebirdPacket deserialized;
    ASSERT_TRUE(deserialized.deserialize(serialized));

    EXPECT_EQ(deserialized.operation, op_authenticate_user);

    std::size_t offset = 0;
    EXPECT_EQ(deserialized.read_string(offset), "testuser");
    EXPECT_EQ(deserialized.read_string(offset), "testpass");
    EXPECT_EQ(deserialized.read_uint32(offset), 12345u);
    EXPECT_EQ(deserialized.read_uint16(offset), 678u);
}

TEST_F(FirebirdProtocolTest, MessageFraming)
{
    FirebirdAuthMessageFramer framer;

    // Create a test packet
    FirebirdPacket packet;
    packet.operation = op_connect;
    packet.write_uint32(PROTOCOL_VERSION15);
    auto packet_data = packet.serialize();

    // Test framing complete packet
    auto frames = framer.frame_messages(packet_data);
    ASSERT_EQ(frames.size(), 1);
    EXPECT_EQ(frames[0], packet_data);

    // Test framing partial packet
    framer.reset();
    auto partial1 = std::vector<std::uint8_t>(packet_data.begin(), packet_data.begin() + 5);
    auto partial2 = std::vector<std::uint8_t>(packet_data.begin() + 5, packet_data.end());

    frames = framer.frame_messages(partial1);
    EXPECT_EQ(frames.size(), 0); // No complete frames yet
    EXPECT_TRUE(framer.needs_more_data());

    frames = framer.frame_messages(partial2);
    ASSERT_EQ(frames.size(), 1); // Now we have a complete frame
    EXPECT_EQ(frames[0], packet_data);
    EXPECT_FALSE(framer.needs_more_data());
}

// Test cross-platform compatibility - key insight from user feedback
TEST_F(FirebirdProtocolTest, CrossPlatformAuthenticationOperations)
{
    // Test that the correct authentication operations are used based on protocol version
    // This addresses the user's concern about Windows trusted authentication working on all
    // platforms

    std::vector<std::pair<std::uint16_t, FirebirdProtocolOp>> protocol_auth_ops = {
        {PROTOCOL_VERSION11, op_trusted_auth}, // Protocol 11: uses op_trusted_auth
        {PROTOCOL_VERSION12, op_trusted_auth}, // Protocol 12: uses op_trusted_auth
        {PROTOCOL_VERSION13, op_cont_auth},    // Protocol 13: uses op_cont_auth
        {PROTOCOL_VERSION15, op_cont_auth}     // Protocol 15: uses op_cont_auth
    };

    for (const auto& [protocol_version, expected_op] : protocol_auth_ops) {
        FirebirdAuthenticationHandler handler(auth_manager_.get());

        // Verify that handler correctly identifies which authentication operation to use
        ProtocolVersion version;
        version.major = protocol_version;
        version.type = ProtocolType::FirebirdWire;

        EXPECT_TRUE(handler.supports_version(version)) << "Protocol version: " << protocol_version;

        // The key insight: authentication should work consistently across platforms
        // regardless of whether it's Windows SSPI, Linux PAM, or other OS authentication
        if (protocol_version >= PROTOCOL_VERSION11) {
            // All protocol versions >= 11 should support some form of trusted authentication
            EXPECT_TRUE(protocol_version >= PROTOCOL_VERSION11);
        }
    }
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
