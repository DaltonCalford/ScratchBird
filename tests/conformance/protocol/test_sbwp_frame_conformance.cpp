#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "scratchbird/protocol/sbwp_protocol.h"

#ifndef SB_PROTOCOL_CONFORMANCE_DIR
#define SB_PROTOCOL_CONFORMANCE_DIR "."
#endif

namespace scratchbird::tests {
namespace {
namespace sbwp = scratchbird::protocol::sbwp;

uint32_t readU32LE(const std::vector<uint8_t>& bytes, size_t offset) {
    return static_cast<uint32_t>(bytes[offset]) |
           (static_cast<uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<uint32_t>(bytes[offset + 3]) << 24);
}

uint64_t readU64LE(const std::vector<uint8_t>& bytes, size_t offset) {
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i) {
        value |= static_cast<uint64_t>(bytes[offset + i]) << (8 * i);
    }
    return value;
}

std::string messageTypeName(sbwp::MessageType type) {
    switch (type) {
        case sbwp::MessageType::Startup: return "Startup";
        case sbwp::MessageType::AuthResponse: return "AuthResponse";
        case sbwp::MessageType::Query: return "Query";
        case sbwp::MessageType::Parse: return "Parse";
        case sbwp::MessageType::Bind: return "Bind";
        case sbwp::MessageType::Execute: return "Execute";
        case sbwp::MessageType::Close: return "Close";
        case sbwp::MessageType::Sync: return "Sync";
        case sbwp::MessageType::Cancel: return "Cancel";
        case sbwp::MessageType::CopyData: return "CopyData";
        case sbwp::MessageType::CopyDone: return "CopyDone";
        case sbwp::MessageType::CopyFail: return "CopyFail";
        case sbwp::MessageType::Error: return "Error";
        default: return "Unknown";
    }
}

std::map<std::string, std::string> readTraceMetadata(const std::filesystem::path& file) {
    std::ifstream in(file);
    std::map<std::string, std::string> fields;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const size_t sep = line.find('=');
        if (sep == std::string::npos || sep == 0 || sep + 1 >= line.size()) {
            continue;
        }
        fields[line.substr(0, sep)] = line.substr(sep + 1);
    }
    return fields;
}

std::array<uint8_t, 16> makeAttachmentId(uint8_t seed) {
    std::array<uint8_t, 16> id{};
    for (size_t i = 0; i < id.size(); ++i) {
        id[i] = static_cast<uint8_t>(seed + i);
    }
    return id;
}

sbwp::MessageHeader makeHeader(sbwp::MessageType type,
                               uint32_t sequence,
                               uint64_t txn_id,
                               uint8_t attachment_seed) {
    sbwp::MessageHeader header;
    header.type = type;
    header.flags = 0;
    header.length = 0;
    header.sequence = sequence;
    header.attachment_id = makeAttachmentId(attachment_seed);
    header.txn_id = txn_id;
    return header;
}

sbwp::MessageHeader decodeHeaderOrFail(const std::vector<uint8_t>& frame) {
    if (frame.size() < sbwp::kHeaderSize) {
        ADD_FAILURE() << "frame shorter than SBWP header";
        return {};
    }
    std::vector<uint8_t> header_bytes(frame.begin(), frame.begin() + sbwp::kHeaderSize);
    sbwp::MessageHeader decoded;
    scratchbird::core::ErrorContext ctx;
    const auto status = sbwp::decodeHeader(header_bytes, decoded, &ctx);
    if (status != scratchbird::core::Status::OK) {
        ADD_FAILURE() << "decodeHeader failed: " << ctx.message;
    }
    return decoded;
}

std::vector<uint8_t> payloadFromFrame(const std::vector<uint8_t>& frame) {
    if (frame.size() <= sbwp::kHeaderSize) {
        return {};
    }
    return std::vector<uint8_t>(frame.begin() + sbwp::kHeaderSize, frame.end());
}

const std::filesystem::path kProtocolRoot = std::filesystem::path(SB_PROTOCOL_CONFORMANCE_DIR);
const std::filesystem::path kGoldenSbwpDir = kProtocolRoot / "golden" / "sbwp";

TEST(SBWPFrameConformance, GoldenTraceMetadataExistsForRequiredScenarios) {
    struct Scenario {
        std::string file;
        std::string expected_scenario;
        sbwp::MessageType type;
    };

    const std::vector<Scenario> scenarios = {
        {"01_startup.trace", "startup", sbwp::MessageType::Startup},
        {"02_auth_response.trace", "auth_response", sbwp::MessageType::AuthResponse},
        {"03_query.trace", "query", sbwp::MessageType::Query},
        {"04_parse.trace", "parse", sbwp::MessageType::Parse},
        {"05_bind.trace", "bind", sbwp::MessageType::Bind},
        {"06_execute.trace", "execute", sbwp::MessageType::Execute},
        {"07_close.trace", "close", sbwp::MessageType::Close},
        {"08_cancel.trace", "cancel", sbwp::MessageType::Cancel},
        {"09_copy_data.trace", "copy_data", sbwp::MessageType::CopyData},
        {"10_copy_done.trace", "copy_done", sbwp::MessageType::CopyDone},
        {"11_copy_fail.trace", "copy_fail", sbwp::MessageType::CopyFail},
    };

    for (const auto& scenario : scenarios) {
        const auto path = kGoldenSbwpDir / scenario.file;
        EXPECT_TRUE(std::filesystem::exists(path)) << path;
        const auto metadata = readTraceMetadata(path);
        ASSERT_TRUE(metadata.count("scenario") > 0) << path;
        ASSERT_TRUE(metadata.count("type") > 0) << path;
        EXPECT_EQ(metadata.at("scenario"), scenario.expected_scenario);
        EXPECT_EQ(metadata.at("type"), messageTypeName(scenario.type));
    }
}

TEST(SBWPFrameConformance, HandshakeFrameOrderingAndShapes) {
    const uint64_t features = sbwp::kFeatureSblr | sbwp::kFeatureStreaming | sbwp::kFeatureProfilePostgresql;
    const auto startup_payload = sbwp::buildStartupPayload(
        features,
        {{"client", "sb_isql"}, {"database", "sb"}, {"user", "sb_admin"}});

    const std::vector<uint8_t> auth_payload = {'s', 'b', '_', 't', 'o', 'k', 'e', 'n'};
    const std::vector<uint8_t> sync_payload;

    const auto startup_frame = sbwp::encodeMessage(
        makeHeader(sbwp::MessageType::Startup, 1, 0, 0x10), startup_payload);
    const auto auth_frame = sbwp::encodeMessage(
        makeHeader(sbwp::MessageType::AuthResponse, 2, 0, 0x10), auth_payload);
    const auto sync_frame = sbwp::encodeMessage(
        makeHeader(sbwp::MessageType::Sync, 3, 0, 0x10), sync_payload);

    const auto startup_header = decodeHeaderOrFail(startup_frame);
    const auto auth_header = decodeHeaderOrFail(auth_frame);
    const auto sync_header = decodeHeaderOrFail(sync_frame);

    EXPECT_EQ(startup_header.type, sbwp::MessageType::Startup);
    EXPECT_EQ(auth_header.type, sbwp::MessageType::AuthResponse);
    EXPECT_EQ(sync_header.type, sbwp::MessageType::Sync);
    EXPECT_LT(startup_header.sequence, auth_header.sequence);
    EXPECT_LT(auth_header.sequence, sync_header.sequence);

    const auto startup_body = payloadFromFrame(startup_frame);
    ASSERT_GE(startup_body.size(), 13u);
    EXPECT_EQ(startup_body[0], sbwp::kProtocolMajor);
    EXPECT_EQ(startup_body[1], sbwp::kProtocolMinor);
    EXPECT_EQ(readU64LE(startup_body, 4), features);

    const auto auth_body = payloadFromFrame(auth_frame);
    EXPECT_EQ(auth_body, auth_payload);

    const auto sync_body = payloadFromFrame(sync_frame);
    EXPECT_TRUE(sync_body.empty());
}

TEST(SBWPFrameConformance, QueryAndErrorFrameShape) {
    const auto query_payload = sbwp::buildQueryPayload("select 1", sbwp::kQueryFlagBinaryResult, 128, 2500);
    const auto query_frame = sbwp::encodeMessage(
        makeHeader(sbwp::MessageType::Query, 11, 42, 0x20), query_payload);

    const auto query_header = decodeHeaderOrFail(query_frame);
    EXPECT_EQ(query_header.type, sbwp::MessageType::Query);
    EXPECT_EQ(query_header.sequence, 11u);
    EXPECT_EQ(query_header.txn_id, 42u);

    const auto query_body = payloadFromFrame(query_frame);
    ASSERT_GE(query_body.size(), 12u + std::string("select 1").size() + 1u);
    EXPECT_EQ(readU32LE(query_body, 0), sbwp::kQueryFlagBinaryResult);
    EXPECT_EQ(readU32LE(query_body, 4), 128u);
    EXPECT_EQ(readU32LE(query_body, 8), 2500u);
    EXPECT_EQ(query_body.back(), 0u);

    const std::vector<uint8_t> error_payload = {
        '2', '8', '0', '0', '0', '\0',
        'p', 'e', 'r', 'm', 'i', 's', 's', 'i', 'o', 'n', ' ', 'd', 'e', 'n', 'i', 'e', 'd', '\0'};
    const auto error_frame = sbwp::encodeMessage(
        makeHeader(sbwp::MessageType::Error, 12, 42, 0x20), error_payload);
    const auto error_header = decodeHeaderOrFail(error_frame);
    EXPECT_EQ(error_header.type, sbwp::MessageType::Error);

    const auto err_body = payloadFromFrame(error_frame);
    EXPECT_EQ(err_body, error_payload);
    const auto null_count = static_cast<size_t>(std::count(err_body.begin(), err_body.end(), static_cast<uint8_t>(0)));
    EXPECT_GE(null_count, 2u);
}

TEST(SBWPFrameConformance, PreparedStatementLifecycleFrames) {
    const auto parse_payload = sbwp::buildParsePayload("stmt_1", "select $1::int4", {sbwp::kOidInt4});

    sbwp::ParamValue param;
    param.format = sbwp::kFormatBinary;
    param.type_oid = sbwp::kOidInt4;
    param.data = {0x2A, 0x00, 0x00, 0x00};

    const auto bind_payload = sbwp::buildBindPayload("portal_1", "stmt_1", {param}, {sbwp::kFormatBinary});
    const auto execute_payload = sbwp::buildExecutePayload("portal_1", 64);
    const auto close_payload = sbwp::buildClosePayload(/*close_type=*/0x01, "stmt_1");

    const auto parse_frame = sbwp::encodeMessage(makeHeader(sbwp::MessageType::Parse, 21, 200, 0x30), parse_payload);
    const auto bind_frame = sbwp::encodeMessage(makeHeader(sbwp::MessageType::Bind, 22, 200, 0x30), bind_payload);
    const auto execute_frame = sbwp::encodeMessage(makeHeader(sbwp::MessageType::Execute, 23, 200, 0x30), execute_payload);
    const auto close_frame = sbwp::encodeMessage(makeHeader(sbwp::MessageType::Close, 24, 200, 0x30), close_payload);

    const auto parse_header = decodeHeaderOrFail(parse_frame);
    const auto bind_header = decodeHeaderOrFail(bind_frame);
    const auto execute_header = decodeHeaderOrFail(execute_frame);
    const auto close_header = decodeHeaderOrFail(close_frame);

    EXPECT_EQ(parse_header.type, sbwp::MessageType::Parse);
    EXPECT_EQ(bind_header.type, sbwp::MessageType::Bind);
    EXPECT_EQ(execute_header.type, sbwp::MessageType::Execute);
    EXPECT_EQ(close_header.type, sbwp::MessageType::Close);

    EXPECT_LT(parse_header.sequence, bind_header.sequence);
    EXPECT_LT(bind_header.sequence, execute_header.sequence);
    EXPECT_LT(execute_header.sequence, close_header.sequence);

    const auto parse_body = payloadFromFrame(parse_frame);
    ASSERT_GE(parse_body.size(), 10u);
    EXPECT_EQ(readU32LE(parse_body, 0), std::string("stmt_1").size());

    const auto bind_body = payloadFromFrame(bind_frame);
    ASSERT_GE(bind_body.size(), 16u);

    const auto execute_body = payloadFromFrame(execute_frame);
    ASSERT_GE(execute_body.size(), 12u);

    const auto close_body = payloadFromFrame(close_frame);
    ASSERT_GE(close_body.size(), 8u);
}

TEST(SBWPFrameConformance, CancelAndCopyFrameShapeRoundTrips) {
    const auto cancel_payload = sbwp::buildCancelPayload(/*cancel_type=*/2, /*target_seq=*/41);
    const auto cancel_frame = sbwp::encodeMessage(
        makeHeader(sbwp::MessageType::Cancel, 30, 999, 0x40), cancel_payload);
    const auto cancel_header = decodeHeaderOrFail(cancel_frame);

    EXPECT_EQ(cancel_header.type, sbwp::MessageType::Cancel);
    const auto cancel_body = payloadFromFrame(cancel_frame);
    ASSERT_EQ(cancel_body.size(), 8u);
    EXPECT_EQ(readU32LE(cancel_body, 0), 2u);
    EXPECT_EQ(readU32LE(cancel_body, 4), 41u);

    const std::vector<uint8_t> copy_data = {'a', 'b', 'c', '\n'};
    const auto copy_payload = sbwp::buildCopyDataPayload(copy_data.data(), copy_data.size());
    ASSERT_EQ(copy_payload.size(), 8u + copy_data.size());
    EXPECT_EQ(readU64LE(copy_payload, 0), copy_data.size());

    std::vector<uint8_t> parsed_copy_data;
    scratchbird::core::ErrorContext copy_ctx;
    const auto copy_parse_status = sbwp::parseCopyData(copy_payload, parsed_copy_data, &copy_ctx);
    EXPECT_EQ(copy_parse_status, scratchbird::core::Status::OK) << copy_ctx.message;
    EXPECT_EQ(parsed_copy_data, copy_data);

    const auto copy_done_payload = sbwp::buildCopyDonePayload();
    EXPECT_TRUE(copy_done_payload.empty());

    const std::string copy_fail_message = "copy stream aborted";
    const auto copy_fail_payload = sbwp::buildCopyFailPayload(copy_fail_message);
    std::string parsed_copy_fail;
    scratchbird::core::ErrorContext fail_ctx;
    const auto fail_status = sbwp::parseCopyFail(copy_fail_payload, parsed_copy_fail, &fail_ctx);
    EXPECT_EQ(fail_status, scratchbird::core::Status::OK) << fail_ctx.message;
    EXPECT_EQ(parsed_copy_fail, copy_fail_message);
}

} // namespace
} // namespace scratchbird::tests
