/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include "scratchbird/ipc/ipc_contract_v1_1.h"

using namespace scratchbird::ipc;

// ============================================================================
// IPC Header Tests
// ============================================================================

class IPCHeaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        header_.magic = IPCHeader::MAGIC;
        header_.version = IPC_VERSION_1_1;
        header_.type = static_cast<uint16_t>(IPCMessageType::SIMPLE_QUERY);
        header_.length = 100;
        header_.request_id = 1;
        header_.session_id = 42;
        header_.timestamp = 1234567890;
        header_.flags = 0;
        header_.reserved = 0;
    }
    
    IPCHeader header_;
};

TEST_F(IPCHeaderTest, MagicIsCorrect) {
    EXPECT_EQ(IPCHeader::MAGIC, 0x53424950);  // "SBIP"
}

TEST_F(IPCHeaderTest, SizeIs40Bytes) {
    EXPECT_EQ(sizeof(IPCHeader), 40);
}

TEST_F(IPCHeaderTest, AlignmentIs8) {
    EXPECT_EQ(alignof(IPCHeader), 8);
}

TEST_F(IPCHeaderTest, IsValidReturnsTrueForValidHeader) {
    EXPECT_TRUE(header_.isValid());
}

TEST_F(IPCHeaderTest, IsValidReturnsFalseForWrongMagic) {
    header_.magic = 0x12345678;
    EXPECT_FALSE(header_.isValid());
}

TEST_F(IPCHeaderTest, IsValidReturnsFalseForWrongVersion) {
    header_.version = 0x0100;  // v1.0 instead of v1.1
    EXPECT_FALSE(header_.isValid());
}

// ============================================================================
// IPC Message Tests
// ============================================================================

class IPCMessageTest : public ::testing::Test {
protected:
    void SetUp() override {
        msg_ = std::make_unique<IPCMessage>(IPCMessageType::SIMPLE_QUERY, 42);
    }
    
    std::unique_ptr<IPCMessage> msg_;
};

TEST_F(IPCMessageTest, DefaultConstructorCreatesValidMessage) {
    IPCMessage msg;
    EXPECT_TRUE(msg.isValid());
    // Default constructor sets type to 0 via memset (not a valid message type)
    // Use parameterized constructor to set specific type
    EXPECT_EQ(static_cast<uint16_t>(msg.getType()), 0);
}

TEST_F(IPCMessageTest, ConstructorSetsTypeAndSession) {
    EXPECT_EQ(msg_->getType(), IPCMessageType::SIMPLE_QUERY);
    EXPECT_EQ(msg_->header.session_id, 42);
}

TEST_F(IPCMessageTest, GetPayloadCreatesPayloadWhenEmpty) {
    IPCMessage empty_msg;  // Empty payload
    auto* payload = empty_msg.getPayload<IPCStartupPayload>();
    // getPayload resizes and creates payload if needed
    EXPECT_NE(payload, nullptr);
    EXPECT_EQ(empty_msg.payload.size(), sizeof(IPCStartupPayload));
}

TEST_F(IPCMessageTest, GetPayloadCreatesPayloadWhenAccessed) {
    auto* payload = msg_->getPayload<IPCStartupPayload>();
    EXPECT_NE(payload, nullptr);
    EXPECT_EQ(msg_->payload.size(), sizeof(IPCStartupPayload));
}

TEST_F(IPCMessageTest, SerializeProducesCorrectSize) {
    auto data = msg_->serialize();
    EXPECT_EQ(data.size(), sizeof(IPCHeader) + msg_->payload.size());
}

TEST_F(IPCMessageTest, SerializeDeserializesCorrectly) {
    // Set some data in the payload
    auto* payload = msg_->getPayload<IPCStartupPayload>();
    payload->process_id = 1234;
    payload->feature_flags = IPC_FEATURE_PREPARED_STATEMENTS;
    std::strcpy(payload->database, "test_db");
    std::strcpy(payload->user, "test_user");
    
    auto data = msg_->serialize();
    
    IPCMessage deserialized;
    EXPECT_TRUE(deserialized.deserialize(data.data(), data.size()));
    EXPECT_EQ(deserialized.getType(), IPCMessageType::SIMPLE_QUERY);
    EXPECT_EQ(deserialized.header.session_id, 42);
    
    auto* deser_payload = deserialized.getPayload<IPCStartupPayload>();
    EXPECT_NE(deser_payload, nullptr);
    EXPECT_EQ(deser_payload->process_id, 1234);
    EXPECT_EQ(deser_payload->feature_flags, IPC_FEATURE_PREPARED_STATEMENTS);
    EXPECT_STREQ(deser_payload->database, "test_db");
    EXPECT_STREQ(deser_payload->user, "test_user");
}

TEST_F(IPCMessageTest, DeserializeFailsForShortData) {
    uint8_t data[10];
    EXPECT_FALSE(msg_->deserialize(data, 10));
}

TEST_F(IPCMessageTest, DeserializeFailsForInvalidMagic) {
    auto data = msg_->serialize();
    data[0] = 0xFF;  // Corrupt magic
    EXPECT_FALSE(msg_->deserialize(data.data(), data.size()));
}

TEST_F(IPCMessageTest, GetTotalSizeIsCorrect) {
    msg_->payload = std::vector<uint8_t>(100, 0);
    EXPECT_EQ(msg_->getTotalSize(), sizeof(IPCHeader) + 100);
}

// ============================================================================
// Message Type to String Tests
// ============================================================================

TEST(IPCMessageTypeToString, AllTypesHaveStringRepresentation) {
    // Connection Management
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::STARTUP), "STARTUP");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::READY), "READY");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::FEATURE_NEGOTIATE), "FEATURE_NEGOTIATE");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::TERMINATE), "TERMINATE");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::PING), "PING");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::PONG), "PONG");
    
    // Session Management
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::ATTACH), "ATTACH");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::DETACH), "DETACH");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::ATTACHED), "ATTACHED");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::DETACHED), "DETACHED");
    
    // Query Execution
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::SIMPLE_QUERY), "SIMPLE_QUERY");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::PARSE), "PARSE");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::BIND), "BIND");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::DESCRIBE), "DESCRIBE");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::EXECUTE), "EXECUTE");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::CLOSE), "CLOSE");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::SYNC), "SYNC");
    
    // Results
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::ROW_DESCRIPTION), "ROW_DESCRIPTION");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::DATA_ROW), "DATA_ROW");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::DATA_BATCH), "DATA_BATCH");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::COMMAND_COMPLETE), "COMMAND_COMPLETE");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::EMPTY_RESPONSE), "EMPTY_RESPONSE");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::PARSE_COMPLETE), "PARSE_COMPLETE");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::BIND_COMPLETE), "BIND_COMPLETE");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::CLOSE_COMPLETE), "CLOSE_COMPLETE");
    
    // COPY
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::COPY_IN_REQUEST), "COPY_IN_REQUEST");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::COPY_OUT_RESPONSE), "COPY_OUT_RESPONSE");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::COPY_DATA), "COPY_DATA");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::COPY_DONE), "COPY_DONE");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::COPY_FAIL), "COPY_FAIL");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::COPY_COMPLETE), "COPY_COMPLETE");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::STREAM_CONTROL), "STREAM_CONTROL");
    
    // Transactions
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::TXN_BEGIN), "TXN_BEGIN");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::TXN_COMMIT), "TXN_COMMIT");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::TXN_ROLLBACK), "TXN_ROLLBACK");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::SAVEPOINT), "SAVEPOINT");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::RELEASE), "RELEASE");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::ROLLBACK_TO), "ROLLBACK_TO");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::TXN_COMPLETE), "TXN_COMPLETE");
    
    // Async
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::NOTIFY_SUBSCRIBE), "NOTIFY_SUBSCRIBE");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::NOTIFY_UNSUBSCRIBE), "NOTIFY_UNSUBSCRIBE");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::NOTIFY_DELIVER), "NOTIFY_DELIVER");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::CANCEL_REQUEST), "CANCEL_REQUEST");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::CANCEL_ACK), "CANCEL_ACK");
    
    // Errors
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::ERROR_RESPONSE), "ERROR_RESPONSE");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::NOTICE), "NOTICE");
    
    // Internal
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::HEARTBEAT), "HEARTBEAT");
    EXPECT_STREQ(ipcMessageTypeToString(IPCMessageType::SHUTDOWN), "SHUTDOWN");
}

TEST(IPCMessageTypeToString, UnknownTypeReturnsUnknown) {
    EXPECT_STREQ(ipcMessageTypeToString(static_cast<IPCMessageType>(0x9999)), "UNKNOWN");
}

// ============================================================================
// Feature Flag Tests
// ============================================================================

TEST(IPCFeatureFlagToString, AllFlagsHaveStringRepresentation) {
    EXPECT_STREQ(ipcFeatureFlagToString(IPC_FEATURE_PREPARED_STATEMENTS), "PREPARED_STATEMENTS");
    EXPECT_STREQ(ipcFeatureFlagToString(IPC_FEATURE_COPY_STREAMING), "COPY_STREAMING");
    EXPECT_STREQ(ipcFeatureFlagToString(IPC_FEATURE_NOTIFICATIONS), "NOTIFICATIONS");
    EXPECT_STREQ(ipcFeatureFlagToString(IPC_FEATURE_CANCEL), "CANCEL");
    EXPECT_STREQ(ipcFeatureFlagToString(IPC_FEATURE_BINARY_RESULTS), "BINARY_RESULTS");
    EXPECT_STREQ(ipcFeatureFlagToString(IPC_FEATURE_COMPRESSION), "COMPRESSION");
    EXPECT_STREQ(ipcFeatureFlagToString(IPC_FEATURE_ENCRYPTION), "ENCRYPTION");
    EXPECT_STREQ(ipcFeatureFlagToString(IPC_FEATURE_BATCH_EXECUTION), "BATCH_EXECUTION");
}

TEST(IPCFeatureFlagToString, UnknownFlagReturnsUnknown) {
    EXPECT_STREQ(ipcFeatureFlagToString(0x80000000), "UNKNOWN");
}

TEST(IPCFeatureFlags, CanCombineFlags) {
    uint32_t flags = IPC_FEATURE_PREPARED_STATEMENTS | IPC_FEATURE_COPY_STREAMING | IPC_FEATURE_CANCEL;
    EXPECT_TRUE(flags & IPC_FEATURE_PREPARED_STATEMENTS);
    EXPECT_TRUE(flags & IPC_FEATURE_COPY_STREAMING);
    EXPECT_TRUE(flags & IPC_FEATURE_CANCEL);
    EXPECT_FALSE(flags & IPC_FEATURE_NOTIFICATIONS);
}

// ============================================================================
// Message Validation Tests
// ============================================================================

TEST(ValidateIPCMessage, ValidMessageReturnsTrue) {
    IPCMessage msg(IPCMessageType::SIMPLE_QUERY);
    msg.payload.resize(sizeof(IPCSimpleQueryPayload));
    msg.header.length = msg.payload.size();
    
    std::string error;
    EXPECT_TRUE(validateIPCMessage(msg, error));
    EXPECT_TRUE(error.empty());
}

TEST(ValidateIPCMessage, InvalidHeaderReturnsFalse) {
    IPCMessage msg;
    msg.header.magic = 0x12345678;  // Invalid
    
    std::string error;
    EXPECT_FALSE(validateIPCMessage(msg, error));
    EXPECT_FALSE(error.empty());
}

TEST(ValidateIPCMessage, PayloadLengthMismatchReturnsFalse) {
    IPCMessage msg(IPCMessageType::SIMPLE_QUERY);
    msg.payload.resize(10);
    msg.header.length = 20;  // Mismatch
    
    std::string error;
    EXPECT_FALSE(validateIPCMessage(msg, error));
    EXPECT_NE(error.find("length mismatch"), std::string::npos);
}

TEST(ValidateIPCMessage, MessageTooLargeReturnsFalse) {
    IPCMessage msg(IPCMessageType::SIMPLE_QUERY);
    msg.payload.resize(IPC_MAX_MESSAGE_SIZE + 1);
    msg.header.length = msg.payload.size();
    
    std::string error;
    EXPECT_FALSE(validateIPCMessage(msg, error));
    EXPECT_NE(error.find("maximum size"), std::string::npos);
}

TEST(ValidateIPCMessage, SimpleQueryPayloadTooSmallReturnsFalse) {
    IPCMessage msg(IPCMessageType::SIMPLE_QUERY);
    msg.payload.resize(1);
    msg.header.length = 1;
    
    std::string error;
    EXPECT_FALSE(validateIPCMessage(msg, error));
    EXPECT_NE(error.find("SIMPLE_QUERY"), std::string::npos);
}

TEST(ValidateIPCMessage, ParsePayloadTooSmallReturnsFalse) {
    IPCMessage msg(IPCMessageType::PARSE);
    msg.payload.resize(10);
    msg.header.length = 10;
    
    std::string error;
    EXPECT_FALSE(validateIPCMessage(msg, error));
    EXPECT_NE(error.find("PARSE"), std::string::npos);
}

TEST(ValidateIPCMessage, BindPayloadTooSmallReturnsFalse) {
    IPCMessage msg(IPCMessageType::BIND);
    msg.payload.resize(10);
    msg.header.length = 10;
    
    std::string error;
    EXPECT_FALSE(validateIPCMessage(msg, error));
    EXPECT_NE(error.find("BIND"), std::string::npos);
}

TEST(ValidateIPCMessage, ExecutePayloadTooSmallReturnsFalse) {
    IPCMessage msg(IPCMessageType::EXECUTE);
    msg.payload.resize(10);
    msg.header.length = 10;
    
    std::string error;
    EXPECT_FALSE(validateIPCMessage(msg, error));
    EXPECT_NE(error.find("EXECUTE"), std::string::npos);
}

TEST(ValidateIPCMessage, RowDescriptionPayloadTooSmallReturnsFalse) {
    IPCMessage msg(IPCMessageType::ROW_DESCRIPTION);
    msg.payload.resize(1);
    msg.header.length = 1;
    
    std::string error;
    EXPECT_FALSE(validateIPCMessage(msg, error));
    EXPECT_NE(error.find("ROW_DESCRIPTION"), std::string::npos);
}

// ============================================================================
// Payload Structure Tests
// ============================================================================

TEST(IPCPayloadStructures, StartupPayloadSizesAreCorrect) {
    EXPECT_EQ(sizeof(IPCStartupPayload::process_id), 4);
    EXPECT_EQ(sizeof(IPCStartupPayload::secret_key), 4);
    EXPECT_EQ(sizeof(IPCStartupPayload::feature_flags), 4);
    EXPECT_EQ(sizeof(IPCStartupPayload::database), 64);
    EXPECT_EQ(sizeof(IPCStartupPayload::user), 64);
    EXPECT_EQ(sizeof(IPCStartupPayload::application), 64);
}

TEST(IPCPayloadStructures, ReadyPayloadSizesAreCorrect) {
    EXPECT_EQ(sizeof(IPCReadyPayload::session_id), 4);
    EXPECT_EQ(sizeof(IPCReadyPayload::server_features), 4);
    EXPECT_EQ(sizeof(IPCReadyPayload::server_version), 32);
}

TEST(IPCPayloadStructures, FeaturePayloadSizesAreCorrect) {
    EXPECT_EQ(sizeof(IPCFeaturePayload), 12);
}

TEST(IPCPayloadStructures, CommandCompletePayloadSizesAreCorrect) {
    EXPECT_EQ(sizeof(IPCCommandCompletePayload::tag), 64);
    EXPECT_EQ(sizeof(IPCCommandCompletePayload::rows_affected), 8);
    EXPECT_EQ(sizeof(IPCCommandCompletePayload::last_insert_id), 8);
}

TEST(IPCPayloadStructures, ErrorPayloadSizesAreCorrect) {
    EXPECT_EQ(sizeof(IPCErrorPayload::sqlstate), 6);
    EXPECT_EQ(sizeof(IPCErrorPayload::message), 512);
    EXPECT_EQ(sizeof(IPCErrorPayload::detail), 1024);
    EXPECT_EQ(sizeof(IPCErrorPayload::hint), 512);
}

TEST(IPCPayloadStructures, FieldDescSizesAreCorrect) {
    EXPECT_EQ(sizeof(IPCFieldDesc::name), 64);
    EXPECT_EQ(sizeof(IPCFieldDesc::table_oid), 4);
    EXPECT_EQ(sizeof(IPCFieldDesc::column_num), 2);
    EXPECT_EQ(sizeof(IPCFieldDesc::type_oid), 2);
    EXPECT_EQ(sizeof(IPCFieldDesc::type_size), 2);
    EXPECT_EQ(sizeof(IPCFieldDesc::type_modifier), 4);
    EXPECT_EQ(sizeof(IPCFieldDesc::format), 2);
    // Total with padding is 84 (not 80 due to alignment)
    EXPECT_EQ(sizeof(IPCFieldDesc), 84);
}

TEST(IPCPayloadStructures, ParamValueSizesAreCorrect) {
    EXPECT_EQ(sizeof(IPCParamValue::type_oid), 2);
    EXPECT_EQ(sizeof(IPCParamValue::format), 2);
    EXPECT_EQ(sizeof(IPCParamValue::length), 4);
    EXPECT_EQ(sizeof(IPCParamValue), 8);
}

// ============================================================================
// Constant Tests
// ============================================================================

TEST(IPCConstants, VersionIsCorrect) {
    EXPECT_EQ(IPC_VERSION_1_1, 0x0101);
    EXPECT_EQ(IPC_CURRENT_VERSION, IPC_VERSION_1_1);
}

TEST(IPCConstants, MaxSizesAreReasonable) {
    EXPECT_EQ(IPC_MAX_MESSAGE_SIZE, 1024 * 1024);  // 1MB
    EXPECT_EQ(IPC_MAX_PAYLOAD_SIZE, 1020 * 1024);  // ~1MB
    EXPECT_EQ(IPC_MAX_SQL_LENGTH, 512 * 1024);     // 512KB
    EXPECT_EQ(IPC_MAX_PARAMS, 1024);
    EXPECT_EQ(IPC_MAX_FIELDS, 1024);
    EXPECT_EQ(IPC_MAX_BATCH_ROWS, 10000);
    EXPECT_EQ(IPC_MAX_COPY_CHUNK, 64 * 1024);      // 64KB
}

TEST(IPCConstants, FeatureFlagsArePowersOfTwo) {
    EXPECT_EQ(IPC_FEATURE_PREPARED_STATEMENTS, 0x00000001);
    EXPECT_EQ(IPC_FEATURE_COPY_STREAMING, 0x00000002);
    EXPECT_EQ(IPC_FEATURE_NOTIFICATIONS, 0x00000004);
    EXPECT_EQ(IPC_FEATURE_CANCEL, 0x00000008);
    EXPECT_EQ(IPC_FEATURE_BINARY_RESULTS, 0x00000010);
    EXPECT_EQ(IPC_FEATURE_COMPRESSION, 0x00000020);
    EXPECT_EQ(IPC_FEATURE_ENCRYPTION, 0x00000040);
    EXPECT_EQ(IPC_FEATURE_BATCH_EXECUTION, 0x00000080);
}

TEST(IPCConstants, HeaderFlagsArePowersOfTwo) {
    EXPECT_EQ(IPCHeader::FLAG_COMPRESSED, 0x00000001);
    EXPECT_EQ(IPCHeader::FLAG_ENCRYPTED, 0x00000002);
    EXPECT_EQ(IPCHeader::FLAG_URGENT, 0x00000004);
}
