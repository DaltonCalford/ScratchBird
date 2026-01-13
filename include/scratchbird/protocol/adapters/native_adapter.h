/**
 * Native ScratchBird Protocol Adapter
 *
 * ScratchBird Network Layer - Phase 3.2
 *
 * Wraps the native ScratchBird wire protocol for use with the
 * protocol adapter framework. The native protocol is optimized for
 * local IPC communication with rich type support.
 */

#pragma once

#include "scratchbird/protocol/adapters/protocol_adapter.h"
#include "scratchbird/protocol/wire_protocol.h"

namespace scratchbird {
namespace protocol {

// ============================================================================
// Native Protocol State
// ============================================================================

enum class NativeProtocolState {
    INITIAL,            // Waiting for CONNECT_REQUEST
    AUTHENTICATING,     // Processing authentication
    AUTHENTICATED,      // Authentication complete
    READY,              // Ready for queries
    QUERY_PROCESSING,   // Processing a query
    COPY_IN,            // COPY FROM STDIN streaming
    COPY_OUT,           // COPY TO STDOUT streaming
    CLOSING,            // Connection closing
    ERROR               // Protocol error
};

// ============================================================================
// Native Protocol Adapter
// ============================================================================

/**
 * Native ScratchBird Protocol Adapter
 *
 * Implements the ScratchBird native wire protocol, which provides:
 * - Full type support for all ScratchBird data types
 * - Efficient binary encoding
 * - Prepared statement support
 * - Transaction management
 * - Administrative commands
 */
class NativeAdapter : public ProtocolAdapter {
public:
    explicit NativeAdapter(const ProtocolAdapterConfig& config = ProtocolAdapterConfig());
    ~NativeAdapter() override;

    // ========================================================================
    // ProtocolHandler Interface
    // ========================================================================

    network::ProtocolType getProtocolType() const override {
        return network::ProtocolType::NATIVE;
    }

protected:
    // ========================================================================
    // ProtocolAdapter Implementation
    // ========================================================================

    core::Status parseMessage(network::Connection* conn) override;
    core::Status processMessage(network::Connection* conn) override;
    core::Status sendGreeting(network::Connection* conn) override;
    core::Status processAuthentication(network::Connection* conn) override;
    core::Status sendAuthResult(network::Connection* conn,
                                bool success,
                                const std::string& error_msg = "") override;
    core::Status sendQueryResult(network::Connection* conn,
                                 const ResultContext& result) override;
    core::Status sendProtocolError(network::Connection* conn,
                                   uint32_t error_code,
                                   const std::string& sqlstate,
                                   const std::string& message,
                                   const std::string& detail = "",
                                   const std::string& hint = "") override;

private:
    // ========================================================================
    // Message Handling
    // ========================================================================

    core::Status handleConnectRequest(network::Connection* conn);
    core::Status handleDisconnect(network::Connection* conn);
    core::Status handleAuthRequest(network::Connection* conn);
    core::Status handleQuery(network::Connection* conn);
    core::Status handleQueryCancel(network::Connection* conn);
    core::Status handlePrepare(network::Connection* conn);
    core::Status handleExecute(network::Connection* conn);
    core::Status handleCloseStatement(network::Connection* conn);
    core::Status handleDescribe(network::Connection* conn);
    core::Status handleBeginTransaction(network::Connection* conn);
    core::Status handleCommit(network::Connection* conn);
    core::Status handleRollback(network::Connection* conn);
    core::Status handleSavepoint(network::Connection* conn);
    core::Status handleReleaseSavepoint(network::Connection* conn);
    core::Status handleRollbackTo(network::Connection* conn);
    core::Status handlePing(network::Connection* conn);
    core::Status handleStatusRequest(network::Connection* conn);

    core::Status handleCopyQuery(network::Connection* conn, const QueryContext& ctx,
                                 bool from_stdin, bool to_stdout);

    // ========================================================================
    // Message Sending
    // ========================================================================

    void sendConnectResponse(network::Connection* conn, bool success,
                             const std::string& error_msg = "");
    void sendAuthResponse(network::Connection* conn, bool success,
                          const std::string& error_msg = "");
    void sendQueryError(network::Connection* conn, uint32_t error_code,
                        const std::string& sqlstate, const std::string& message);
    void sendRowDescription(network::Connection* conn,
                            const std::vector<ProtocolCodec::ColumnInfo>& columns);
    void sendRowData(network::Connection* conn,
                     const std::vector<ProtocolCodec::ColumnValue>& values);
    void sendEndOfResults(network::Connection* conn);
    void sendCommandComplete(network::Connection* conn, const std::string& tag,
                             int64_t rows_affected);
    void sendPrepareResponse(network::Connection* conn, uint32_t stmt_id, bool success,
                             const std::string& error_msg = "");
    void sendDescribeResponse(network::Connection* conn, uint32_t stmt_id,
                              const std::vector<ProtocolCodec::ColumnInfo>& columns,
                              uint16_t param_count);
    void sendTransactionStatus(network::Connection* conn, bool in_transaction);
    void sendPong(network::Connection* conn);
    void sendStatusResponse(network::Connection* conn);

    // ========================================================================
    // Helper Methods
    // ========================================================================

    void sendMessage(network::Connection* conn, const Message& msg);
    core::Status flushWriteBuffer(network::Connection* conn);
    core::Status receiveMessageBlocking(network::Connection* conn, Message& msg);

    bool parseCopyQuery(const std::string& sql, bool& from_stdin, bool& to_stdout) const;
    bool sendCopyOutChunk(network::Connection* conn, const uint8_t* data, size_t len,
                          std::string& error);
    bool readCopyInChunk(network::Connection* conn, std::string& out, bool& done,
                         std::string& error);
    bool waitForCopyOutWindow(network::Connection* conn, std::string& error);
    core::Status grantCopyInWindow(network::Connection* conn, uint32_t window_bytes);

    // ========================================================================
    // State
    // ========================================================================

    NativeProtocolState native_state_ = NativeProtocolState::INITIAL;

    // Current message being processed
    Message current_message_;

    // Session info
    uint8_t session_id_[SESSION_ID_SIZE] = {0};
    uint32_t client_version_ = 0;

    // Prepared statements (id -> query)
    uint32_t next_stmt_id_ = 1;
    std::unordered_map<uint32_t, std::string> native_prepared_statements_;

    // COPY streaming state
    uint64_t next_stream_id_ = 1;
    uint64_t copy_stream_id_ = 0;
    uint64_t copy_total_bytes_ = 0;
    uint32_t copy_out_window_bytes_ = 0;
    uint32_t copy_in_window_bytes_ = 0;
    uint32_t copy_in_window_grant_ = 0;
    uint32_t copy_in_low_watermark_ = 0;
    bool copy_out_paused_ = false;
};

} // namespace protocol
} // namespace scratchbird
