/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
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

#include "scratchbird/client/connection.h"
#include "scratchbird/core/lsm_compression.h"
#include "scratchbird/protocol/adapters/protocol_adapter.h"
#include "scratchbird/protocol/sbwp_protocol.h"
#include "scratchbird/protocol/wire_protocol.h"
#include <chrono>
#include <unordered_set>

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
    COPY_BOTH,          // COPY BOTH streaming (bidirectional)
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
    core::Status handleBind(network::Connection* conn);
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
    core::Status handleSubscribe(network::Connection* conn);
    core::Status handleUnsubscribe(network::Connection* conn);

    core::Status handleCopyQuery(network::Connection* conn, const QueryContext& ctx,
                                 bool from_stdin, bool to_stdout, CopyFormat format);
    core::Status handleCopyData(network::Connection* conn);
    core::Status handleCopyDone(network::Connection* conn);
    core::Status handleCopyFail(network::Connection* conn);
    core::Status ensureRemoteClient(core::ErrorContext* ctx);
    core::Status executeRemoteQuery(const std::string& sql,
                                    const std::vector<uint8_t>* bytecode,
                                    ResultContext& result);

    // ========================================================================
    // Message Sending
    // ========================================================================

    void sendAuthRequest(network::Connection* conn,
                         sbwp::AuthMethod method,
                         const std::vector<uint8_t>& data = {});
    void sendAuthContinue(network::Connection* conn,
                          sbwp::AuthMethod method,
                          uint8_t stage,
                          const std::vector<uint8_t>& data);
    void sendAuthOk(network::Connection* conn,
                    const std::vector<uint8_t>& info);
    void sendQueryError(network::Connection* conn, uint32_t error_code,
                        const std::string& sqlstate, const std::string& message);
    void sendRowDescription(network::Connection* conn,
                            const std::vector<ProtocolCodec::ColumnInfo>& columns);
    void sendRowData(network::Connection* conn,
                     const std::vector<ProtocolCodec::ColumnValue>& values);
    void sendCommandComplete(network::Connection* conn, const std::string& tag,
                             int64_t rows_affected);
    void sendPortalSuspended(network::Connection* conn);
    void sendReady(network::Connection* conn);
    void sendParameterStatus(network::Connection* conn,
                             const std::string& name,
                             const std::string& value);
    void sendParameterDescription(network::Connection* conn,
                                  const std::vector<uint32_t>& param_types);
    void sendParseComplete(network::Connection* conn);
    void sendBindComplete(network::Connection* conn);
    void sendCloseComplete(network::Connection* conn);
    void sendNoData(network::Connection* conn);
    void sendQueryProgress(network::Connection* conn,
                           uint64_t rows_processed,
                           uint64_t bytes_processed);
    void sendNotification(network::Connection* conn,
                          uint32_t process_id,
                          const std::string& channel,
                          const std::vector<uint8_t>& payload,
                          uint8_t change_type,
                          uint64_t row_id);
    void sendPrepareResponse(network::Connection* conn, uint32_t stmt_id, bool success,
                             const std::string& error_msg = "");
    void sendDescribeResponse(network::Connection* conn, uint32_t stmt_id,
                              const std::vector<ProtocolCodec::ColumnInfo>& columns,
                              uint16_t param_count);
    void sendTransactionStatus(network::Connection* conn, bool in_transaction);
    void sendPong(network::Connection* conn, uint64_t timestamp, uint32_t sequence);
    void sendStatusResponse(network::Connection* conn);
    void sendCopyInResponse(network::Connection* conn, uint8_t format, uint32_t window_bytes);
    void sendCopyOutResponse(network::Connection* conn, uint8_t format, uint16_t column_count,
                             const std::vector<uint32_t>& column_formats);
    void sendCopyBothResponse(network::Connection* conn, uint8_t format, uint32_t window_bytes);
    void sendCopyData(network::Connection* conn, const uint8_t* data, size_t len);

    // ========================================================================
    // Helper Methods
    // ========================================================================

    void sendMessage(network::Connection* conn,
                     sbwp::MessageType type,
                     const std::vector<uint8_t>& payload,
                     uint8_t flags = 0,
                     uint32_t sequence_override = 0);
    core::Status flushWriteBuffer(network::Connection* conn,
                                  std::chrono::milliseconds max_wait =
                                      std::chrono::milliseconds(1000));
    core::Status receiveMessageBlocking(network::Connection* conn,
                                        sbwp::ProtocolMessage& msg);
    bool pollCancel(network::Connection* conn);

    bool parseCopyQuery(const std::string& sql, bool& from_stdin, bool& to_stdout,
                        CopyFormat* format_out) const;
    void recordCopyMetrics(const std::string& direction,
                           uint64_t rows,
                           uint64_t bytes,
                           bool error,
                           const std::chrono::steady_clock::time_point& start_time) const;
    bool sendCopyOutChunk(network::Connection* conn, const uint8_t* data, size_t len,
                          std::string& error);
    bool readCopyInChunk(network::Connection* conn, std::string& out, bool& done,
                         std::string& error);
    bool waitForCopyOutWindow(network::Connection* conn, std::string& error);
    core::Status grantCopyInWindow(network::Connection* conn, uint32_t window_bytes);
    bool waitForStreamWindow(network::Connection* conn, std::string& error);
    bool sendStreamPayload(network::Connection* conn, uint64_t stream_id,
                           const uint8_t* data, size_t len, std::string& error);

    struct PortalState {
        std::string statement_name;
        std::vector<std::string> param_values;
        std::vector<bool> param_nulls;
        bool bound = false;
        std::vector<ProtocolCodec::ColumnInfo> columns;
        std::vector<std::vector<ProtocolCodec::ColumnValue>> rows;
        size_t fetch_pos = 0;
        uint64_t rows_sent = 0;
        uint64_t bytes_sent = 0;
        uint64_t last_progress_micros = 0;
        bool completed = false;
        std::string command_tag;
        int64_t rows_affected = 0;
    };

    core::Status sendPortalResults(network::Connection* conn,
                                   PortalState& portal,
                                   uint32_t max_rows,
                                   bool backward);

    // ========================================================================
    // State
    // ========================================================================

    NativeProtocolState native_state_ = NativeProtocolState::INITIAL;

    // Current message being processed
    sbwp::ProtocolMessage current_message_;
    uint32_t current_sequence_ = 0;
    uint64_t client_features_ = 0;
    sbwp::AuthMethod auth_method_ = sbwp::AuthMethod::ScramSha256;
    bool auth_in_progress_ = false;
    bool scram_pending_ = false;
    bool cancel_requested_ = false;
    uint32_t cancel_target_sequence_ = 0;

    // Session info
    uint8_t session_id_[SESSION_ID_SIZE] = {0};
    uint32_t client_version_ = 0;

    // Prepared statements (id -> query)
    uint32_t next_stmt_id_ = 1;
    std::unordered_map<uint32_t, std::string> native_prepared_statements_;
    std::unordered_map<std::string, PortalState> portals_;
    std::unordered_set<std::string> subscribed_channels_;

    // COPY streaming state
    enum class CopyDirection { NONE, IN, OUT, BOTH };
    CopyDirection copy_direction_ = CopyDirection::NONE;
    CopyFormat copy_format_ = CopyFormat::TEXT;
    std::string copy_table_name_;
    std::string copy_query_;
    std::vector<uint8_t> copy_buffer_;
    uint64_t copy_rows_processed_ = 0;
    uint64_t copy_bytes_processed_ = 0;
    std::chrono::steady_clock::time_point copy_start_time_;
    uint64_t next_stream_id_ = 1;
    uint64_t copy_stream_id_ = 0;
    uint64_t copy_total_bytes_ = 0;
    uint32_t copy_out_window_bytes_ = 0;
    uint32_t copy_in_window_bytes_ = 0;
    uint32_t copy_in_window_grant_ = 0;
    uint32_t copy_in_low_watermark_ = 0;
    bool copy_out_paused_ = false;
    uint32_t stream_window_bytes_ = 0;
    bool stream_paused_ = false;
    static constexpr uint32_t kDefaultCopyWindow = 1024 * 1024; // 1MB default window

    client::ConnectionConfig client_config_;
    std::unique_ptr<client::Connection> client_;

    bool compression_enabled_ = false;
    std::unique_ptr<core::Compressor> wire_compressor_;
    bool progress_enabled_ = false;
};

} // namespace protocol
} // namespace scratchbird
