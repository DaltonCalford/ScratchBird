/**
 * Firebird Wire Protocol Adapter Implementation
 *
 * ScratchBird Network Layer - Phase 3.2
 *
 * Implements Firebird wire protocol for client compatibility.
 */

#include "scratchbird/protocol/adapters/firebird_adapter.h"
#include "scratchbird/core/error_context.h"

#include <cstring>
#include <random>
#include <algorithm>

namespace scratchbird {
namespace protocol {

// ============================================================================
// Constructor/Destructor
// ============================================================================

FirebirdAdapter::FirebirdAdapter(const ProtocolAdapterConfig& config)
    : ProtocolAdapter(config) {

    // Generate initial handles
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(1, 0x7FFFFFFF);

    next_db_handle_ = dist(gen);
    next_tr_handle_ = dist(gen);
    next_stmt_handle_ = dist(gen);
}

FirebirdAdapter::~FirebirdAdapter() = default;

// ============================================================================
// ProtocolAdapter Implementation
// ============================================================================

core::Status FirebirdAdapter::parseMessage(network::Connection* conn) {
    const auto& buffer = conn->getReadBuffer();

    // Firebird protocol: XDR encoded, starts with opcode (4 bytes)
    if (buffer.size() < 4) {
        return core::Status::IO_ERROR;  // Need more data
    }

    // Read opcode (big-endian)
    current_opcode_ = readUInt32(buffer.data());

    // Determine packet length based on opcode
    // Firebird uses variable-length packets, we need to read until complete
    // For simplicity, we'll read available data and process

    // Most operations have a minimum size
    size_t min_size = 4;  // Just opcode for some operations

    switch (current_opcode_) {
        case firebird::Opcode::op_connect:
            // Connect has: opcode + operation + version + arch + min_type + max_type +
            // preference bitmap + protocols array + user_id
            min_size = 28;  // Minimum connect packet
            break;

        case firebird::Opcode::op_attach:
        case firebird::Opcode::op_create:
            min_size = 16;  // opcode + db_handle + db_path_len (minimum)
            break;

        case firebird::Opcode::op_transaction:
            min_size = 12;  // opcode + db_handle + tpb_len
            break;

        case firebird::Opcode::op_commit:
        case firebird::Opcode::op_rollback:
        case firebird::Opcode::op_commit_retaining:
        case firebird::Opcode::op_rollback_retaining:
            min_size = 8;  // opcode + tr_handle
            break;

        case firebird::Opcode::op_allocate_statement:
            min_size = 8;  // opcode + db_handle
            break;

        case firebird::Opcode::op_prepare_statement:
        case firebird::Opcode::op_exec_immediate:
        case firebird::Opcode::op_exec_immediate2:
            min_size = 24;  // opcode + handles + sql_len (minimum)
            break;

        case firebird::Opcode::op_execute:
        case firebird::Opcode::op_execute2:
            min_size = 16;  // opcode + handles
            break;

        case firebird::Opcode::op_fetch:
            min_size = 16;  // opcode + stmt_handle + blr_len + msg_len
            break;

        case firebird::Opcode::op_free_statement:
            min_size = 12;  // opcode + stmt_handle + option
            break;

        case firebird::Opcode::op_detach:
        case firebird::Opcode::op_drop_database:
            min_size = 8;  // opcode + db_handle
            break;

        case firebird::Opcode::op_disconnect:
        case firebird::Opcode::op_ping:
            min_size = 4;  // Just opcode
            break;

        case firebird::Opcode::op_cont_auth:
            min_size = 12;  // opcode + data_len + plugin_len
            break;

        default:
            min_size = 4;
            break;
    }

    if (buffer.size() < min_size) {
        return core::Status::IO_ERROR;  // Need more data
    }

    // For now, consume available data up to a reasonable limit
    // In a full implementation, we'd parse XDR to determine exact length
    size_t packet_len = std::min(buffer.size(), static_cast<size_t>(65536));

    current_packet_.assign(buffer.begin(), buffer.begin() + packet_len);
    conn->consumeReadBuffer(packet_len);

    return core::Status::OK;
}

core::Status FirebirdAdapter::processMessage(network::Connection* conn) {
    bytes_received_ += current_packet_.size();

    switch (current_opcode_) {
        case firebird::Opcode::op_connect:
            return handleConnect(conn);

        case firebird::Opcode::op_attach:
            return handleAttach(conn);

        case firebird::Opcode::op_create:
            return handleCreateDatabase(conn);

        case firebird::Opcode::op_detach:
            return handleDetach(conn);

        case firebird::Opcode::op_drop_database:
            return handleDropDatabase(conn);

        case firebird::Opcode::op_transaction:
            return handleTransaction(conn);

        case firebird::Opcode::op_commit:
            return handleCommit(conn);

        case firebird::Opcode::op_rollback:
            return handleRollback(conn);

        case firebird::Opcode::op_commit_retaining:
            return handleCommitRetaining(conn);

        case firebird::Opcode::op_rollback_retaining:
            return handleRollbackRetaining(conn);

        case firebird::Opcode::op_allocate_statement:
            return handleAllocateStatement(conn);

        case firebird::Opcode::op_prepare_statement:
            return handlePrepareStatement(conn);

        case firebird::Opcode::op_execute:
            return handleExecute(conn);

        case firebird::Opcode::op_execute2:
            return handleExecute2(conn);

        case firebird::Opcode::op_exec_immediate:
            return handleExecImmediate(conn);

        case firebird::Opcode::op_exec_immediate2:
            return handleExecImmediate2(conn);

        case firebird::Opcode::op_fetch:
            return handleFetch(conn);

        case firebird::Opcode::op_free_statement:
            return handleFreeStatement(conn);

        case firebird::Opcode::op_set_cursor:
            return handleSetCursor(conn);

        case firebird::Opcode::op_info_database:
            return handleInfoDatabase(conn);

        case firebird::Opcode::op_info_transaction:
            return handleInfoTransaction(conn);

        case firebird::Opcode::op_info_sql:
            return handleInfoSql(conn);

        case firebird::Opcode::op_cont_auth:
            return handleContAuth(conn);

        case firebird::Opcode::op_ping:
            return handlePing(conn);

        case firebird::Opcode::op_cancel:
            return handleCancel(conn);

        case firebird::Opcode::op_disconnect:
            return handleDisconnect(conn);

        default:
            sendErrorResponse(conn, firebird::ErrorCode::isc_unavailable,
                             "Unsupported operation: " + std::to_string(current_opcode_));
            return sendBuffer(conn);
    }
}

core::Status FirebirdAdapter::sendGreeting(network::Connection* /*conn*/) {
    // Firebird: Server waits for client op_connect
    return core::Status::OK;
}

core::Status FirebirdAdapter::processAuthentication(network::Connection* /*conn*/) {
    // Authentication handled in handleConnect/handleContAuth
    return core::Status::OK;
}

core::Status FirebirdAdapter::sendAuthResult(network::Connection* conn,
                                              bool success,
                                              const std::string& error_msg) {
    if (success) {
        auth_complete_ = true;
        fb_state_ = FirebirdProtocolState::AUTHENTICATED;
        // Accept is sent in handleConnect
    } else {
        sendErrorResponse(conn, firebird::ErrorCode::isc_login, error_msg);
    }
    return sendBuffer(conn);
}

core::Status FirebirdAdapter::sendQueryResult(network::Connection* conn,
                                               const ResultContext& result) {
    if (result.has_error) {
        sendErrorResponse(conn, static_cast<int32_t>(result.error_code), result.error_message);
        return core::Status::OK;
    }

    // For SELECT, results are sent via fetch
    // For DML, send response with affected rows
    if (result.columns.empty()) {
        std::vector<uint8_t> data;
        sendResponse(conn, 0, static_cast<uint64_t>(result.rows_affected), data);
    }

    return core::Status::OK;
}

core::Status FirebirdAdapter::sendProtocolError(network::Connection* conn,
                                                 uint32_t error_code,
                                                 const std::string& /*sqlstate*/,
                                                 const std::string& message,
                                                 const std::string& /*detail*/,
                                                 const std::string& /*hint*/) {
    sendErrorResponse(conn, static_cast<int32_t>(error_code), message);
    return core::Status::OK;
}

// ============================================================================
// Operation Handling
// ============================================================================

core::Status FirebirdAdapter::handleConnect(network::Connection* conn) {
    // Parse connect packet
    size_t offset = 4;  // Skip opcode

    if (current_packet_.size() < 28) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_unavailable, "Invalid connect packet");
        return sendBuffer(conn);
    }

    // Operation (should be op_attach or op_create)
    uint32_t operation = readUInt32(current_packet_.data() + offset);
    offset += 4;
    (void)operation;

    // Version
    uint32_t version = readUInt32(current_packet_.data() + offset);
    offset += 4;
    (void)version;

    // Architecture
    uint32_t arch = readUInt32(current_packet_.data() + offset);
    offset += 4;
    (void)arch;

    // Min/max protocol type
    uint32_t min_type = readUInt32(current_packet_.data() + offset);
    offset += 4;
    (void)min_type;

    uint32_t max_type = readUInt32(current_packet_.data() + offset);
    offset += 4;
    (void)max_type;

    // Skip preference bitmap and protocols for now
    // In a full implementation, we'd negotiate protocol version

    // For testing, accept with default protocol
    client_protocol_version_ = firebird::DEFAULT_PROTOCOL_VERSION;
    fb_state_ = FirebirdProtocolState::CONNECT_RECEIVED;

    // Send accept (or accept_data for auth)
    if (config_.require_authentication) {
        // Request authentication
        std::vector<uint8_t> auth_data;
        std::vector<uint8_t> keys;
        sendAcceptData(conn, firebird::DEFAULT_PROTOCOL_VERSION,
                      firebird::ARCH_GENERIC, 1, auth_data,
                      firebird::AUTH_PLUGIN_SRP256, false, keys);
        fb_state_ = FirebirdProtocolState::AUTH_CONTINUE;
    } else {
        // Trust authentication
        sendAccept(conn, firebird::DEFAULT_PROTOCOL_VERSION,
                  firebird::ARCH_GENERIC, 1);
        auth_complete_ = true;
        fb_state_ = FirebirdProtocolState::AUTHENTICATED;
    }

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleAttach(network::Connection* conn) {
    size_t offset = 4;  // Skip opcode

    // Database handle (0 for new attach)
    uint32_t db_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;
    (void)db_handle;

    // Database path
    std::string db_path = readString(current_packet_.data(), offset, current_packet_.size());

    // DPB (Database Parameter Buffer)
    std::vector<uint8_t> dpb = readBuffer(current_packet_.data(), offset, current_packet_.size());
    parseDpb(dpb);

    database_name_ = db_path;

    // Assign database handle
    db_handle_ = next_db_handle_++;
    fb_state_ = FirebirdProtocolState::ATTACHED;

    // Send response
    std::vector<uint8_t> response_data;
    sendResponse(conn, db_handle_, 0, response_data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleDetach(network::Connection* conn) {
    size_t offset = 4;

    uint32_t db_handle = readUInt32(current_packet_.data() + offset);

    if (db_handle != db_handle_) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "Invalid database handle");
        return sendBuffer(conn);
    }

    db_handle_ = 0;
    fb_state_ = FirebirdProtocolState::AUTHENTICATED;

    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleCreateDatabase(network::Connection* conn) {
    // Similar to attach but creates the database
    return handleAttach(conn);
}

core::Status FirebirdAdapter::handleDropDatabase(network::Connection* conn) {
    size_t offset = 4;

    uint32_t db_handle = readUInt32(current_packet_.data() + offset);

    if (db_handle != db_handle_) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "Invalid database handle");
        return sendBuffer(conn);
    }

    // TODO: Actually drop database

    db_handle_ = 0;
    fb_state_ = FirebirdProtocolState::AUTHENTICATED;

    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleTransaction(network::Connection* conn) {
    size_t offset = 4;

    uint32_t db_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    if (db_handle != db_handle_) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "Invalid database handle");
        return sendBuffer(conn);
    }

    // TPB (Transaction Parameter Buffer)
    std::vector<uint8_t> tpb = readBuffer(current_packet_.data(), offset, current_packet_.size());
    (void)tpb;  // Parse if needed

    // Begin transaction
    auto status = beginTransaction();
    if (status != core::Status::OK) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_unavailable, "Failed to start transaction");
        return sendBuffer(conn);
    }

    current_transaction_ = next_tr_handle_++;

    std::vector<uint8_t> data;
    sendResponse(conn, current_transaction_, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleCommit(network::Connection* conn) {
    size_t offset = 4;

    uint32_t tr_handle = readUInt32(current_packet_.data() + offset);

    if (tr_handle != current_transaction_) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_tr_handle, "Invalid transaction handle");
        return sendBuffer(conn);
    }

    auto status = commitTransaction();
    if (status != core::Status::OK) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_unavailable, "Commit failed");
        return sendBuffer(conn);
    }

    current_transaction_ = 0;

    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleRollback(network::Connection* conn) {
    size_t offset = 4;

    uint32_t tr_handle = readUInt32(current_packet_.data() + offset);

    if (tr_handle != current_transaction_) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_tr_handle, "Invalid transaction handle");
        return sendBuffer(conn);
    }

    auto status = rollbackTransaction();
    if (status != core::Status::OK) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_unavailable, "Rollback failed");
        return sendBuffer(conn);
    }

    current_transaction_ = 0;

    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleCommitRetaining(network::Connection* conn) {
    // Commit but keep transaction open
    size_t offset = 4;

    uint32_t tr_handle = readUInt32(current_packet_.data() + offset);

    if (tr_handle != current_transaction_) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_tr_handle, "Invalid transaction handle");
        return sendBuffer(conn);
    }

    // In ScratchBird, we commit and start a new transaction
    commitTransaction();
    beginTransaction();

    std::vector<uint8_t> data;
    sendResponse(conn, tr_handle, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleRollbackRetaining(network::Connection* conn) {
    size_t offset = 4;

    uint32_t tr_handle = readUInt32(current_packet_.data() + offset);

    if (tr_handle != current_transaction_) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_tr_handle, "Invalid transaction handle");
        return sendBuffer(conn);
    }

    rollbackTransaction();
    beginTransaction();

    std::vector<uint8_t> data;
    sendResponse(conn, tr_handle, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleAllocateStatement(network::Connection* conn) {
    size_t offset = 4;

    uint32_t db_handle = readUInt32(current_packet_.data() + offset);

    if (db_handle != db_handle_) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "Invalid database handle");
        return sendBuffer(conn);
    }

    // Allocate new statement handle
    uint32_t stmt_handle = next_stmt_handle_++;

    FirebirdStatement stmt;
    stmt.handle = stmt_handle;
    stmt.prepared = false;
    statements_[stmt_handle] = stmt;

    std::vector<uint8_t> data;
    sendResponse(conn, stmt_handle, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handlePrepareStatement(network::Connection* conn) {
    size_t offset = 4;

    uint32_t tr_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    uint32_t stmt_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    // SQL dialect
    uint32_t dialect = readUInt32(current_packet_.data() + offset);
    offset += 4;
    (void)dialect;

    // SQL statement
    std::string sql = readString(current_packet_.data(), offset, current_packet_.size());

    // Description items (BLR)
    std::vector<uint8_t> items = readBuffer(current_packet_.data(), offset, current_packet_.size());
    (void)items;

    if (tr_handle != current_transaction_ && current_transaction_ != 0) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_tr_handle, "Invalid transaction handle");
        return sendBuffer(conn);
    }

    auto it = statements_.find(stmt_handle);
    if (it == statements_.end()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_stmt_handle, "Invalid statement handle");
        return sendBuffer(conn);
    }

    // Store prepared statement
    it->second.query = sql;
    it->second.prepared = true;

    // Determine statement type (simplified)
    std::string upper_sql = sql;
    for (char& c : upper_sql) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (upper_sql.find("SELECT") == 0) {
        it->second.type = firebird::StatementType::TYPE_SELECT;
    } else if (upper_sql.find("INSERT") == 0) {
        it->second.type = firebird::StatementType::TYPE_INSERT;
    } else if (upper_sql.find("UPDATE") == 0) {
        it->second.type = firebird::StatementType::TYPE_UPDATE;
    } else if (upper_sql.find("DELETE") == 0) {
        it->second.type = firebird::StatementType::TYPE_DELETE;
    } else {
        it->second.type = firebird::StatementType::TYPE_DDL;
    }

    std::vector<uint8_t> data;
    sendResponse(conn, stmt_handle, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleExecute(network::Connection* conn) {
    size_t offset = 4;

    uint32_t tr_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    uint32_t stmt_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    if (tr_handle != current_transaction_ && current_transaction_ != 0) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_tr_handle, "Invalid transaction handle");
        return sendBuffer(conn);
    }

    auto it = statements_.find(stmt_handle);
    if (it == statements_.end() || !it->second.prepared) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_stmt_handle, "Statement not prepared");
        return sendBuffer(conn);
    }

    // Execute the statement
    QueryContext ctx;
    ctx.query = it->second.query;

    ResultContext result;
    executeQuery(ctx, result);

    if (result.has_error) {
        sendErrorResponse(conn, static_cast<int32_t>(result.error_code), result.error_message);
    } else {
        std::vector<uint8_t> data;
        sendResponse(conn, stmt_handle, static_cast<uint64_t>(result.rows_affected), data);
    }

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleExecute2(network::Connection* conn) {
    // Execute2 includes output parameters - for now, same as Execute
    return handleExecute(conn);
}

core::Status FirebirdAdapter::handleExecImmediate(network::Connection* conn) {
    size_t offset = 4;

    uint32_t tr_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    uint32_t db_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    uint32_t dialect = readUInt32(current_packet_.data() + offset);
    offset += 4;
    (void)dialect;

    std::string sql = readString(current_packet_.data(), offset, current_packet_.size());

    if (db_handle != db_handle_) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "Invalid database handle");
        return sendBuffer(conn);
    }

    if (tr_handle != current_transaction_ && current_transaction_ != 0) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_tr_handle, "Invalid transaction handle");
        return sendBuffer(conn);
    }

    // Execute immediately
    QueryContext ctx;
    ctx.query = sql;

    ResultContext result;
    executeQuery(ctx, result);

    if (result.has_error) {
        sendErrorResponse(conn, static_cast<int32_t>(result.error_code), result.error_message);
    } else {
        std::vector<uint8_t> data;
        sendResponse(conn, 0, static_cast<uint64_t>(result.rows_affected), data);
    }

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleExecImmediate2(network::Connection* conn) {
    return handleExecImmediate(conn);
}

core::Status FirebirdAdapter::handleFetch(network::Connection* conn) {
    size_t offset = 4;

    uint32_t stmt_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    // BLR for output
    std::vector<uint8_t> blr = readBuffer(current_packet_.data(), offset, current_packet_.size());
    (void)blr;

    // Message length
    uint32_t msg_len = readUInt32(current_packet_.data() + offset);
    offset += 4;
    (void)msg_len;

    auto it = statements_.find(stmt_handle);
    if (it == statements_.end()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_stmt_handle, "Invalid statement handle");
        return sendBuffer(conn);
    }

    // For now, return empty result (no more rows)
    // In a real implementation, we'd stream rows from the query result
    std::vector<std::vector<uint8_t>> rows;
    sendFetchResponse(conn, 100, 0, rows);  // 100 = end of cursor

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleFreeStatement(network::Connection* conn) {
    size_t offset = 4;

    uint32_t stmt_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    uint32_t option = readUInt32(current_packet_.data() + offset);
    (void)option;  // DSQL_close, DSQL_drop, etc.

    statements_.erase(stmt_handle);

    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleSetCursor(network::Connection* conn) {
    // Set cursor name - acknowledge
    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);
    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleInfoDatabase(network::Connection* conn) {
    // Return database info
    std::vector<uint8_t> info;

    // For now, return minimal info
    // In a real implementation, we'd return requested info items

    sendResponse(conn, db_handle_, 0, info);
    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleInfoTransaction(network::Connection* conn) {
    std::vector<uint8_t> info;
    sendResponse(conn, current_transaction_, 0, info);
    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleInfoSql(network::Connection* conn) {
    size_t offset = 4;

    uint32_t stmt_handle = readUInt32(current_packet_.data() + offset);

    auto it = statements_.find(stmt_handle);
    if (it == statements_.end()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_stmt_handle, "Invalid statement handle");
        return sendBuffer(conn);
    }

    std::vector<uint8_t> info;
    // Return statement type, etc.

    sendResponse(conn, stmt_handle, 0, info);
    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleContAuth(network::Connection* conn) {
    size_t offset = 4;

    // Auth data
    std::vector<uint8_t> data = readBuffer(current_packet_.data(), offset, current_packet_.size());

    // Plugin name
    std::string plugin = readString(current_packet_.data(), offset, current_packet_.size());

    // For testing, accept any auth
    // In a real implementation, we'd validate SRP or other auth

    auth_complete_ = true;
    fb_state_ = FirebirdProtocolState::AUTHENTICATED;

    std::vector<uint8_t> auth_response;
    std::vector<uint8_t> keys;
    sendAcceptData(conn, protocol_version_, firebird::ARCH_GENERIC, 1,
                  auth_response, plugin, true, keys);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handlePing(network::Connection* conn) {
    // Respond to ping
    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);
    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleCancel(network::Connection* conn) {
    // Cancel current operation
    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);
    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleDisconnect(network::Connection* conn) {
    fb_state_ = FirebirdProtocolState::CLOSING;
    conn->close(network::CloseReason::CLIENT_DISCONNECT);
    return core::Status::OK;
}

// ============================================================================
// Response Sending
// ============================================================================

void FirebirdAdapter::sendPacket(network::Connection* conn, uint32_t opcode,
                                  const std::vector<uint8_t>& data) {
    std::vector<uint8_t> packet;

    // Opcode
    writeUInt32(packet, opcode);

    // Data
    packet.insert(packet.end(), data.begin(), data.end());

    writeToBuffer(conn, packet.data(), packet.size());
}

void FirebirdAdapter::sendAccept(network::Connection* conn, uint32_t version,
                                  uint32_t arch, uint32_t type) {
    std::vector<uint8_t> data;

    writeUInt32(data, version);
    writeUInt32(data, arch);
    writeUInt32(data, type);

    sendPacket(conn, firebird::Opcode::op_accept, data);
}

void FirebirdAdapter::sendAcceptData(network::Connection* conn, uint32_t version,
                                      uint32_t arch, uint32_t type,
                                      const std::vector<uint8_t>& auth_data,
                                      const std::string& plugin, bool authenticated,
                                      const std::vector<uint8_t>& keys) {
    std::vector<uint8_t> data;

    writeUInt32(data, version);
    writeUInt32(data, arch);
    writeUInt32(data, type);

    // Auth data
    writeBuffer(data, auth_data.data(), auth_data.size());

    // Plugin name
    writeString(data, plugin);

    // Authenticated flag
    writeUInt32(data, authenticated ? 1 : 0);

    // Keys
    writeBuffer(data, keys.data(), keys.size());

    sendPacket(conn, firebird::Opcode::op_accept_data, data);
}

void FirebirdAdapter::sendResponse(network::Connection* conn, uint32_t handle,
                                    uint64_t object_id, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> response;

    writeUInt32(response, handle);
    writeInt64(response, static_cast<int64_t>(object_id));

    // Data buffer
    writeBuffer(response, data.data(), data.size());

    // Status vector (empty = success)
    writeUInt32(response, firebird::ErrorCode::isc_arg_end);

    sendPacket(conn, firebird::Opcode::op_response, response);
}

void FirebirdAdapter::sendFetchResponse(network::Connection* conn, uint32_t status,
                                         uint32_t count,
                                         const std::vector<std::vector<uint8_t>>& rows) {
    std::vector<uint8_t> data;

    writeUInt32(data, status);
    writeUInt32(data, count);

    // Row data
    for (const auto& row : rows) {
        writeBuffer(data, row.data(), row.size());
    }

    sendPacket(conn, firebird::Opcode::op_fetch_response, data);
}

void FirebirdAdapter::sendSqlResponse(network::Connection* conn, uint32_t count) {
    std::vector<uint8_t> data;
    writeUInt32(data, count);
    sendPacket(conn, firebird::Opcode::op_sql_response, data);
}

void FirebirdAdapter::sendErrorResponse(network::Connection* conn, int32_t error_code,
                                         const std::string& message) {
    std::vector<uint8_t> data;

    // Handle
    writeUInt32(data, 0);

    // Object ID
    writeInt64(data, 0);

    // Empty data
    writeUInt32(data, 0);

    // Status vector
    writeInt32(data, firebird::ErrorCode::isc_arg_gds);
    writeInt32(data, error_code);
    writeInt32(data, firebird::ErrorCode::isc_arg_string);
    writeString(data, message);
    writeInt32(data, firebird::ErrorCode::isc_arg_end);

    sendPacket(conn, firebird::Opcode::op_response, data);
}

// ============================================================================
// Helper Methods
// ============================================================================

void FirebirdAdapter::writeInt32(std::vector<uint8_t>& buf, int32_t value) {
    writeUInt32(buf, static_cast<uint32_t>(value));
}

void FirebirdAdapter::writeUInt32(std::vector<uint8_t>& buf, uint32_t value) {
    // XDR: big-endian
    buf.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
}

void FirebirdAdapter::writeInt64(std::vector<uint8_t>& buf, int64_t value) {
    // XDR: big-endian
    for (int i = 7; i >= 0; --i) {
        buf.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

void FirebirdAdapter::writeBuffer(std::vector<uint8_t>& buf, const void* data, size_t len) {
    // Length-prefixed buffer with padding to 4-byte boundary
    writeUInt32(buf, static_cast<uint32_t>(len));

    if (len > 0) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        buf.insert(buf.end(), ptr, ptr + len);

        // Pad to 4-byte boundary
        size_t padding = (4 - (len % 4)) % 4;
        for (size_t i = 0; i < padding; ++i) {
            buf.push_back(0);
        }
    }
}

void FirebirdAdapter::writeString(std::vector<uint8_t>& buf, const std::string& str) {
    writeBuffer(buf, str.data(), str.size());
}

void FirebirdAdapter::writePaddedString(std::vector<uint8_t>& buf, const std::string& str) {
    writeString(buf, str);
}

int32_t FirebirdAdapter::readInt32(const uint8_t* data) {
    return static_cast<int32_t>(readUInt32(data));
}

uint32_t FirebirdAdapter::readUInt32(const uint8_t* data) {
    // XDR: big-endian
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

int64_t FirebirdAdapter::readInt64(const uint8_t* data) {
    int64_t result = 0;
    for (int i = 0; i < 8; ++i) {
        result = (result << 8) | data[i];
    }
    return result;
}

std::string FirebirdAdapter::readString(const uint8_t* data, size_t& offset, size_t max_len) {
    if (offset + 4 > max_len) return "";

    uint32_t len = readUInt32(data + offset);
    offset += 4;

    if (len == 0 || offset + len > max_len) return "";

    std::string result(reinterpret_cast<const char*>(data + offset), len);
    offset += len;

    // Skip padding
    size_t padding = (4 - (len % 4)) % 4;
    offset += padding;

    return result;
}

std::vector<uint8_t> FirebirdAdapter::readBuffer(const uint8_t* data, size_t& offset, size_t max_len) {
    if (offset + 4 > max_len) return {};

    uint32_t len = readUInt32(data + offset);
    offset += 4;

    if (len == 0 || offset + len > max_len) return {};

    std::vector<uint8_t> result(data + offset, data + offset + len);
    offset += len;

    // Skip padding
    size_t padding = (4 - (len % 4)) % 4;
    offset += padding;

    return result;
}

uint16_t FirebirdAdapter::wireTypeToFirebirdType(WireType type) {
    switch (type) {
        case WireType::BOOLEAN: return firebird::SqlType::SQL_BOOLEAN;
        case WireType::INT16: return firebird::SqlType::SQL_SHORT;
        case WireType::INT32: return firebird::SqlType::SQL_LONG;
        case WireType::INT64: return firebird::SqlType::SQL_INT64;
        case WireType::FLOAT32: return firebird::SqlType::SQL_FLOAT;
        case WireType::FLOAT64: return firebird::SqlType::SQL_DOUBLE;
        case WireType::DECIMAL: return firebird::SqlType::SQL_INT64;  // Scaled
        case WireType::VARCHAR: return firebird::SqlType::SQL_VARYING;
        case WireType::CHAR: return firebird::SqlType::SQL_TEXT;
        case WireType::BYTEA: return firebird::SqlType::SQL_BLOB;
        case WireType::DATE: return firebird::SqlType::SQL_TYPE_DATE;
        case WireType::TIME: return firebird::SqlType::SQL_TYPE_TIME;
        case WireType::TIMESTAMP: return firebird::SqlType::SQL_TIMESTAMP;
        case WireType::TIMESTAMPTZ: return firebird::SqlType::SQL_TIMESTAMP_TZ;
        default: return firebird::SqlType::SQL_VARYING;
    }
}

WireType FirebirdAdapter::firebirdTypeToWireType(uint16_t type) {
    switch (type) {
        case firebird::SqlType::SQL_BOOLEAN: return WireType::BOOLEAN;
        case firebird::SqlType::SQL_SHORT: return WireType::INT16;
        case firebird::SqlType::SQL_LONG: return WireType::INT32;
        case firebird::SqlType::SQL_INT64: return WireType::INT64;
        case firebird::SqlType::SQL_FLOAT: return WireType::FLOAT32;
        case firebird::SqlType::SQL_DOUBLE:
        case firebird::SqlType::SQL_D_FLOAT: return WireType::FLOAT64;
        case firebird::SqlType::SQL_TEXT: return WireType::CHAR;
        case firebird::SqlType::SQL_VARYING: return WireType::VARCHAR;
        case firebird::SqlType::SQL_BLOB: return WireType::BYTEA;
        case firebird::SqlType::SQL_TYPE_DATE: return WireType::DATE;
        case firebird::SqlType::SQL_TYPE_TIME:
        case firebird::SqlType::SQL_TIME_TZ: return WireType::TIME;
        case firebird::SqlType::SQL_TIMESTAMP:
        case firebird::SqlType::SQL_TIMESTAMP_TZ: return WireType::TIMESTAMP;
        default: return WireType::VARCHAR;
    }
}

void FirebirdAdapter::parseDpb(const std::vector<uint8_t>& dpb) {
    if (dpb.empty()) return;

    size_t offset = 0;

    // Version
    if (dpb[offset] == firebird::DpbItem::isc_dpb_version1 ||
        dpb[offset] == firebird::DpbItem::isc_dpb_version2) {
        offset++;
    }

    while (offset < dpb.size()) {
        uint8_t item = dpb[offset++];
        if (offset >= dpb.size()) break;

        uint8_t len = dpb[offset++];
        if (offset + len > dpb.size()) break;

        std::string value(reinterpret_cast<const char*>(dpb.data() + offset), len);
        offset += len;

        switch (item) {
            case firebird::DpbItem::isc_dpb_user_name:
                username_ = value;
                break;
            case firebird::DpbItem::isc_dpb_sql_dialect:
                if (!value.empty()) sql_dialect_ = static_cast<uint8_t>(value[0]);
                break;
            case firebird::DpbItem::isc_dpb_lc_ctype:
                client_charset_ = value;
                break;
            default:
                // Ignore other items
                break;
        }
    }
}

std::vector<uint8_t> FirebirdAdapter::buildDefaultTpb() {
    std::vector<uint8_t> tpb;

    tpb.push_back(firebird::TpbItem::isc_tpb_version3);
    tpb.push_back(firebird::TpbItem::isc_tpb_write);
    tpb.push_back(firebird::TpbItem::isc_tpb_read_committed);
    tpb.push_back(firebird::TpbItem::isc_tpb_rec_version);
    tpb.push_back(firebird::TpbItem::isc_tpb_wait);

    return tpb;
}

} // namespace protocol
} // namespace scratchbird
