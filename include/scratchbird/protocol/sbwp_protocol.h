#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "scratchbird/core/error_context.h"

namespace scratchbird::protocol::sbwp {

constexpr uint32_t kProtocolMagic = 0x53425750; // "SBWP"
constexpr uint8_t kProtocolMajor = 1;
constexpr uint8_t kProtocolMinor = 1;
constexpr uint16_t kProtocolVersion = (static_cast<uint16_t>(kProtocolMajor) << 8) | kProtocolMinor;
constexpr size_t kHeaderSize = 40;
constexpr size_t kMaxMessageSize = 1024u * 1024u * 1024u;

enum class MessageType : uint8_t {
    Startup = 0x01,
    AuthResponse = 0x02,
    Query = 0x03,
    Parse = 0x04,
    Bind = 0x05,
    Describe = 0x06,
    Execute = 0x07,
    Close = 0x08,
    Sync = 0x09,
    Flush = 0x0A,
    Cancel = 0x0B,
    Terminate = 0x0C,
    CopyData = 0x0D,
    CopyDone = 0x0E,
    CopyFail = 0x0F,
    SblrExecute = 0x10,
    Subscribe = 0x11,
    Unsubscribe = 0x12,
    FederatedQuery = 0x13,
    StreamControl = 0x14,
    TxnBegin = 0x15,
    TxnCommit = 0x16,
    TxnRollback = 0x17,
    TxnSavepoint = 0x18,
    TxnRelease = 0x19,
    TxnRollbackTo = 0x1A,
    Ping = 0x1B,
    SetOption = 0x1C,
    ClusterAuth = 0x1D,
    AttachCreate = 0x1E,
    AttachDetach = 0x1F,
    AttachList = 0x20,

    AuthRequest = 0x40,
    AuthOk = 0x41,
    AuthContinue = 0x42,
    Ready = 0x43,
    RowDescription = 0x44,
    DataRow = 0x45,
    CommandComplete = 0x46,
    EmptyQuery = 0x47,
    Error = 0x48,
    Notice = 0x49,
    ParseComplete = 0x4A,
    BindComplete = 0x4B,
    CloseComplete = 0x4C,
    PortalSuspended = 0x4D,
    NoData = 0x4E,
    ParameterStatus = 0x4F,
    ParameterDescription = 0x50,
    CopyInResponse = 0x51,
    CopyOutResponse = 0x52,
    CopyBothResponse = 0x53,
    Notification = 0x54,
    FunctionResult = 0x55,
    NegotiateVersion = 0x56,
    SblrCompiled = 0x57,
    QueryPlan = 0x58,
    StreamReady = 0x59,
    StreamData = 0x5A,
    StreamEnd = 0x5B,
    TxnStatus = 0x5C,
    Pong = 0x5D,
    ClusterAuthOk = 0x5E,
    FederatedResult = 0x5F,
    Heartbeat = 0x80,
    Extension = 0x81
};

enum MessageFlags : uint8_t {
    kFlagCompressed = 0x01,
    kFlagContinued = 0x02,
    kFlagFinal = 0x04,
    kFlagUrgent = 0x08,
    kFlagEncrypted = 0x10,
    kFlagChecksum = 0x20
};

constexpr uint64_t kFeatureCompression = 1ULL << 0;
constexpr uint64_t kFeatureStreaming = 1ULL << 1;
constexpr uint64_t kFeatureSblr = 1ULL << 2;
constexpr uint64_t kFeatureFederation = 1ULL << 3;
constexpr uint64_t kFeatureNotifications = 1ULL << 4;
constexpr uint64_t kFeatureQueryPlan = 1ULL << 5;
constexpr uint64_t kFeatureBatch = 1ULL << 6;
constexpr uint64_t kFeaturePipeline = 1ULL << 7;
constexpr uint64_t kFeatureBinaryCopy = 1ULL << 8;
constexpr uint64_t kFeatureSavepoints = 1ULL << 9;
constexpr uint64_t kFeatureTwoPhase = 1ULL << 10;
constexpr uint64_t kFeatureChecksums = 1ULL << 11;

constexpr uint32_t kQueryFlagDescribeOnly = 0x01;
constexpr uint32_t kQueryFlagNoPortal = 0x02;
constexpr uint32_t kQueryFlagBinaryResult = 0x04;
constexpr uint32_t kQueryFlagIncludePlan = 0x08;
constexpr uint32_t kQueryFlagReturnSblr = 0x10;
constexpr uint32_t kQueryFlagNoCache = 0x20;

constexpr uint8_t kFormatText = 0;
constexpr uint8_t kFormatBinary = 1;

constexpr uint8_t kStreamStart = 0;
constexpr uint8_t kStreamPause = 1;
constexpr uint8_t kStreamResume = 2;
constexpr uint8_t kStreamCancel = 3;
constexpr uint8_t kStreamAck = 4;

constexpr uint8_t kSubTypeChannel = 0;
constexpr uint8_t kSubTypeTable = 1;
constexpr uint8_t kSubTypeQuery = 2;
constexpr uint8_t kSubTypeEvent = 3;

enum class AuthMethod : uint8_t {
    Ok = 0,
    Password = 1,
    Md5 = 2,
    ScramSha256 = 3,
    Certificate = 4,
    Gssapi = 5,
    Sspi = 6,
    Ldap = 7,
    Saml = 8,
    Oidc = 9,
    MfaTotp = 10,
    ClusterPki = 11
};

// Type OIDs (PostgreSQL-compatible baseline + ScratchBird extensions)
constexpr uint32_t kOidBool = 16;
constexpr uint32_t kOidBytea = 17;
constexpr uint32_t kOidChar = 18;
constexpr uint32_t kOidInt8 = 20;
constexpr uint32_t kOidInt2 = 21;
constexpr uint32_t kOidInt4 = 23;
constexpr uint32_t kOidText = 25;
constexpr uint32_t kOidJson = 114;
constexpr uint32_t kOidXml = 142;
constexpr uint32_t kOidPoint = 600;
constexpr uint32_t kOidLseg = 601;
constexpr uint32_t kOidPath = 602;
constexpr uint32_t kOidBox = 603;
constexpr uint32_t kOidPolygon = 604;
constexpr uint32_t kOidLine = 628;
constexpr uint32_t kOidFloat4 = 700;
constexpr uint32_t kOidFloat8 = 701;
constexpr uint32_t kOidCircle = 718;
constexpr uint32_t kOidMoney = 790;
constexpr uint32_t kOidMacaddr = 829;
constexpr uint32_t kOidCidr = 650;
constexpr uint32_t kOidInet = 869;
constexpr uint32_t kOidMacaddr8 = 774;
constexpr uint32_t kOidBpChar = 1042;
constexpr uint32_t kOidVarchar = 1043;
constexpr uint32_t kOidDate = 1082;
constexpr uint32_t kOidTime = 1083;
constexpr uint32_t kOidTimestamp = 1114;
constexpr uint32_t kOidTimestamptz = 1184;
constexpr uint32_t kOidInterval = 1186;
constexpr uint32_t kOidTimetz = 1266;
constexpr uint32_t kOidNumeric = 1700;
constexpr uint32_t kOidUuid = 2950;
constexpr uint32_t kOidJsonb = 3802;
constexpr uint32_t kOidRecord = 2249;
constexpr uint32_t kOidInt4Range = 3904;
constexpr uint32_t kOidNumRange = 3906;
constexpr uint32_t kOidTsRange = 3908;
constexpr uint32_t kOidTstzRange = 3910;
constexpr uint32_t kOidDateRange = 3912;
constexpr uint32_t kOidInt8Range = 3926;
constexpr uint32_t kOidTsVector = 3614;
constexpr uint32_t kOidTsQuery = 3615;
constexpr uint32_t kOidSbVector = 16386;

struct MessageHeader {
    MessageType type{MessageType::Query};
    uint8_t flags{0};
    uint32_t length{0};
    uint32_t sequence{0};
    std::array<uint8_t, 16> attachment_id{};
    uint64_t txn_id{0};
};

struct ProtocolMessage {
    MessageHeader header;
    std::vector<uint8_t> body;
};

struct ColumnInfo {
    std::string name;
    uint32_t table_oid{0};
    uint16_t column_index{0};
    uint32_t type_oid{0};
    int16_t type_size{0};
    int32_t type_modifier{0};
    uint8_t format{kFormatBinary};
    bool nullable{true};
};

struct ColumnValue {
    std::vector<uint8_t> data;
    bool is_null{false};
};

struct ParamValue {
    uint16_t format{kFormatBinary};
    std::vector<uint8_t> data;
    bool is_null{false};
    uint32_t type_oid{0};
};

struct Notification {
    uint32_t process_id{0};
    std::string channel;
    std::vector<uint8_t> payload;
    uint8_t change_type{0};
    uint64_t row_id{0};
    bool has_row{false};
};

struct QueryPlan {
    uint32_t format{0};
    uint64_t planning_time{0};
    uint64_t estimated_rows{0};
    uint64_t estimated_cost{0};
    std::vector<uint8_t> plan;
};

struct SblrCompiled {
    uint64_t hash{0};
    uint32_t version{0};
    std::vector<uint8_t> bytecode;
};

std::vector<uint8_t> encodeMessage(const MessageHeader& header,
                                   const std::vector<uint8_t>& payload);
core::Status decodeHeader(const std::vector<uint8_t>& header_bytes,
                          MessageHeader& header,
                          core::ErrorContext* ctx = nullptr);

std::vector<uint8_t> buildStartupPayload(uint64_t features,
                                         const std::map<std::string, std::string>& params);
std::vector<uint8_t> buildQueryPayload(const std::string& query,
                                       uint32_t flags,
                                       uint32_t max_rows,
                                       uint32_t timeout_ms);
std::vector<uint8_t> buildParsePayload(const std::string& statement_name,
                                       const std::string& query,
                                       const std::vector<uint32_t>& param_types);
std::vector<uint8_t> buildBindPayload(const std::string& portal_name,
                                      const std::string& statement_name,
                                      const std::vector<ParamValue>& params,
                                      const std::vector<uint16_t>& result_formats);
std::vector<uint8_t> buildDescribePayload(uint8_t describe_type, const std::string& name);
std::vector<uint8_t> buildExecutePayload(const std::string& portal_name, uint32_t max_rows);
std::vector<uint8_t> buildSblrExecutePayload(uint64_t sblr_hash,
                                             const std::vector<uint8_t>& bytecode,
                                             const std::vector<ParamValue>& params);
std::vector<uint8_t> buildSubscribePayload(uint8_t subscribe_type,
                                           const std::string& channel,
                                           const std::string& filter);
std::vector<uint8_t> buildUnsubscribePayload(const std::string& channel);
std::vector<uint8_t> buildTxnBeginPayload(uint16_t flags,
                                          uint8_t conflict_action,
                                          uint8_t autocommit_mode,
                                          uint8_t isolation_level,
                                          uint8_t access_mode,
                                          uint8_t deferrable,
                                          uint8_t wait_mode,
                                          uint32_t timeout_ms);
std::vector<uint8_t> buildTxnCommitPayload(uint8_t flags);
std::vector<uint8_t> buildTxnRollbackPayload(uint8_t flags);
std::vector<uint8_t> buildTxnSavepointPayload(const std::string& name);
std::vector<uint8_t> buildTxnReleasePayload(const std::string& name);
std::vector<uint8_t> buildTxnRollbackToPayload(const std::string& name);
std::vector<uint8_t> buildSetOptionPayload(const std::string& name, const std::string& value);
std::vector<uint8_t> buildStreamControlPayload(uint8_t control_type,
                                               uint32_t window_size,
                                               uint32_t timeout_ms);
std::vector<uint8_t> buildAttachCreatePayload(const std::string& mode,
                                              const std::string& db_name);
std::vector<uint8_t> buildAttachDetachPayload();
std::vector<uint8_t> buildAttachListPayload();
std::vector<uint8_t> buildClosePayload(uint8_t close_type, const std::string& name);
std::vector<uint8_t> buildCancelPayload(uint32_t cancel_type, uint32_t target_seq);

// ============================================================================
// COPY Message Builders
// ============================================================================

std::vector<uint8_t> buildCopyDataPayload(const uint8_t* data, size_t len);
std::vector<uint8_t> buildCopyDonePayload();
std::vector<uint8_t> buildCopyFailPayload(const std::string& error_message);
std::vector<uint8_t> buildCopyInResponsePayload(uint8_t format, uint32_t window_bytes);
std::vector<uint8_t> buildCopyOutResponsePayload(uint8_t format, uint16_t column_count,
                                                const std::vector<uint32_t>& column_formats);
std::vector<uint8_t> buildCopyBothResponsePayload(uint8_t format, uint32_t window_bytes);

// ============================================================================
// COPY Message Parsers
// ============================================================================

core::Status parseCopyData(const std::vector<uint8_t>& payload,
                          std::vector<uint8_t>& data,
                          core::ErrorContext* ctx = nullptr);
core::Status parseCopyFail(const std::vector<uint8_t>& payload,
                          std::string& error_message,
                          core::ErrorContext* ctx = nullptr);
core::Status parseCopyInResponse(const std::vector<uint8_t>& payload,
                                uint8_t& format,
                                uint32_t& window_bytes,
                                core::ErrorContext* ctx = nullptr);
core::Status parseCopyOutResponse(const std::vector<uint8_t>& payload,
                                 uint8_t& format,
                                 uint16_t& column_count,
                                 std::vector<uint32_t>& column_formats,
                                 core::ErrorContext* ctx = nullptr);

core::Status parseAuthRequest(const std::vector<uint8_t>& payload,
                              AuthMethod& method,
                              std::vector<uint8_t>& data,
                              core::ErrorContext* ctx = nullptr);
core::Status parseAuthContinue(const std::vector<uint8_t>& payload,
                               AuthMethod& method,
                               uint8_t& stage,
                               std::vector<uint8_t>& data,
                               core::ErrorContext* ctx = nullptr);
core::Status parseAuthOk(const std::vector<uint8_t>& payload,
                         std::vector<uint8_t>& session_id,
                         std::vector<uint8_t>& info,
                         core::ErrorContext* ctx = nullptr);
core::Status parseReady(const std::vector<uint8_t>& payload,
                        uint8_t& status,
                        uint64_t& txn_id,
                        uint64_t& epoch,
                        core::ErrorContext* ctx = nullptr);
core::Status parseParameterStatus(const std::vector<uint8_t>& payload,
                                  std::string& name,
                                  std::string& value,
                                  core::ErrorContext* ctx = nullptr);
core::Status parseParameterDescription(const std::vector<uint8_t>& payload,
                                       std::vector<uint32_t>& param_types,
                                       core::ErrorContext* ctx = nullptr);
core::Status parseRowDescription(const std::vector<uint8_t>& payload,
                                 std::vector<ColumnInfo>& columns,
                                 core::ErrorContext* ctx = nullptr);
core::Status parseDataRow(const std::vector<uint8_t>& payload,
                          size_t column_count,
                          std::vector<ColumnValue>& values,
                          core::ErrorContext* ctx = nullptr);
core::Status parseCommandComplete(const std::vector<uint8_t>& payload,
                                  uint8_t& command_type,
                                  uint64_t& rows,
                                  uint64_t& last_id,
                                  std::string& tag,
                                  core::ErrorContext* ctx = nullptr);
core::Status parseNotification(const std::vector<uint8_t>& payload,
                               Notification& notice,
                               core::ErrorContext* ctx = nullptr);
core::Status parseQueryPlan(const std::vector<uint8_t>& payload,
                            QueryPlan& plan,
                            core::ErrorContext* ctx = nullptr);
core::Status parseSblrCompiled(const std::vector<uint8_t>& payload,
                               SblrCompiled& compiled,
                               core::ErrorContext* ctx = nullptr);
core::Status parseErrorMessage(const std::vector<uint8_t>& payload,
                               std::string& severity,
                               std::string& sqlstate,
                               std::string& message,
                               std::string& detail,
                               std::string& hint,
                               core::ErrorContext* ctx = nullptr);
} // namespace scratchbird::protocol::sbwp
