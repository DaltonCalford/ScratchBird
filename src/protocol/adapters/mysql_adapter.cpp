/**
 * MySQL Wire Protocol Adapter Implementation
 *
 * ScratchBird Network Layer - Phase 3.2
 *
 * Implements MySQL wire protocol for client compatibility.
 */

#include "scratchbird/protocol/adapters/mysql_adapter.h"
#include "scratchbird/core/error_context.h"

#include <cstring>
#include <random>
#include <algorithm>

// For SHA1 (native password auth)
#ifdef HAVE_OPENSSL
#include <openssl/sha.h>
#else
#include <functional>
#endif

namespace scratchbird {
namespace protocol {

// ============================================================================
// Constructor/Destructor
// ============================================================================

MySqlAdapter::MySqlAdapter(const ProtocolAdapterConfig& config)
    : ProtocolAdapter(config) {

    // Generate connection ID
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist;
    connection_id_ = dist(gen);

    // Generate auth scramble (20 bytes)
    for (int i = 0; i < 20; ++i) {
        auth_scramble_[i] = static_cast<uint8_t>((dist(gen) % 94) + 33);  // Printable ASCII
    }
}

MySqlAdapter::~MySqlAdapter() = default;

// ============================================================================
// ProtocolAdapter Implementation
// ============================================================================

core::Status MySqlAdapter::parseMessage(network::Connection* conn) {
    const auto& buffer = conn->getReadBuffer();

    // MySQL packet: 3 bytes length + 1 byte sequence + payload
    if (buffer.size() < 4) {
        return core::Status::IO_ERROR;  // Need more data
    }

    uint32_t length = readInt3(buffer.data());
    uint8_t seq = readInt1(buffer.data() + 3);

    if (length > config_.max_message_size) {
        return core::Status::INVALID_ARGUMENT;
    }

    if (buffer.size() < 4 + length) {
        return core::Status::IO_ERROR;  // Need more data
    }

    // Full packet received
    sequence_id_ = seq;
    current_packet_.assign(buffer.begin() + 4, buffer.begin() + 4 + length);

    // Consume from read buffer
    conn->consumeReadBuffer(4 + length);

    return core::Status::OK;
}

core::Status MySqlAdapter::processMessage(network::Connection* conn) {
    bytes_received_ += current_packet_.size() + 4;

    switch (mysql_state_) {
        case MySqlProtocolState::HANDSHAKE_SENT:
            return handleHandshakeResponse(conn);

        case MySqlProtocolState::READY:
        case MySqlProtocolState::AUTHENTICATED:
            return handleCommand(conn);

        default:
            sendErrorPacket(conn, mysql::ErrorCode::UNKNOWN_ERROR, "HY000",
                           "Unexpected message in current state");
            return core::Status::INTERNAL_ERROR;
    }
}

core::Status MySqlAdapter::sendGreeting(network::Connection* conn) {
    sendHandshakePacket(conn);
    mysql_state_ = MySqlProtocolState::HANDSHAKE_SENT;
    return sendBuffer(conn);
}

core::Status MySqlAdapter::processAuthentication(network::Connection* /*conn*/) {
    // Authentication handled in handleHandshakeResponse
    return core::Status::OK;
}

core::Status MySqlAdapter::sendAuthResult(network::Connection* conn,
                                           bool success,
                                           const std::string& error_msg) {
    if (success) {
        sendOkPacket(conn);
        mysql_state_ = MySqlProtocolState::READY;
    } else {
        sendErrorPacket(conn, mysql::ErrorCode::ACCESS_DENIED, "28000",
                       error_msg.empty() ? "Access denied" : error_msg);
    }
    return sendBuffer(conn);
}

core::Status MySqlAdapter::sendQueryResult(network::Connection* conn,
                                            const ResultContext& result) {
    if (result.has_error) {
        return sendProtocolError(conn, result.error_code, result.sqlstate,
                                 result.error_message, result.error_detail, result.error_hint);
    }

    if (result.columns.empty()) {
        // No result set (INSERT, UPDATE, DELETE, DDL)
        sendOkPacket(conn, static_cast<uint64_t>(result.rows_affected), 0, result.command_tag);
    } else {
        // Result set
        sendResultSetHeader(conn, result.columns.size());

        // Column definitions
        for (const auto& col : result.columns) {
            sendColumnDefinition(conn, col);
        }

        // EOF after columns (if client doesn't support DEPRECATE_EOF)
        if (!(client_capabilities_ & mysql::Capability::DEPRECATE_EOF)) {
            sendEofPacket(conn);
        }

        // Rows would be sent via row_callback
        // For now, just send EOF/OK

        // Final EOF/OK
        if (client_capabilities_ & mysql::Capability::DEPRECATE_EOF) {
            sendOkPacket(conn, 0, 0, result.command_tag);
        } else {
            sendEofPacket(conn);
        }
    }

    return core::Status::OK;
}

core::Status MySqlAdapter::sendProtocolError(network::Connection* conn,
                                              uint32_t error_code,
                                              const std::string& sqlstate,
                                              const std::string& message,
                                              const std::string& /*detail*/,
                                              const std::string& /*hint*/) {
    sendErrorPacket(conn, static_cast<uint16_t>(error_code),
                   sqlstate.empty() ? "HY000" : sqlstate, message);
    return core::Status::OK;
}

// ============================================================================
// Handshake Handling
// ============================================================================

void MySqlAdapter::sendHandshakePacket(network::Connection* conn) {
    std::vector<uint8_t> payload;

    // Protocol version
    writeInt1(payload, mysql::PROTOCOL_VERSION);

    // Server version (null-terminated)
    writeNullString(payload, server_version_);

    // Connection ID
    writeInt4(payload, connection_id_);

    // Auth plugin data part 1 (8 bytes)
    payload.insert(payload.end(), auth_scramble_, auth_scramble_ + 8);

    // Filler
    writeInt1(payload, 0x00);

    // Capability flags (lower 2 bytes)
    writeInt2(payload, static_cast<uint16_t>(server_capabilities_ & 0xFFFF));

    // Character set
    writeInt1(payload, mysql::Charset::UTF8MB4_GENERAL_CI);

    // Status flags
    writeInt2(payload, server_status_);

    // Capability flags (upper 2 bytes)
    writeInt2(payload, static_cast<uint16_t>((server_capabilities_ >> 16) & 0xFFFF));

    // Auth plugin data length
    writeInt1(payload, 21);  // 8 + 12 + 1 (null terminator)

    // Reserved (10 bytes of zeros)
    for (int i = 0; i < 10; ++i) {
        writeInt1(payload, 0);
    }

    // Auth plugin data part 2 (12 bytes + null terminator)
    payload.insert(payload.end(), auth_scramble_ + 8, auth_scramble_ + 20);
    writeInt1(payload, 0x00);

    // Auth plugin name
    writeNullString(payload, "mysql_native_password");

    sendPacket(conn, payload);
}

core::Status MySqlAdapter::handleHandshakeResponse(network::Connection* conn) {
    if (current_packet_.size() < 32) {
        sendErrorPacket(conn, mysql::ErrorCode::HANDSHAKE_ERROR, "08S01",
                       "Bad handshake");
        return core::Status::INVALID_ARGUMENT;
    }

    size_t offset = 0;

    // Client capabilities (4 bytes)
    client_capabilities_ = readInt4(current_packet_.data() + offset);
    offset += 4;

    // Max packet size
    max_packet_size_ = readInt4(current_packet_.data() + offset);
    offset += 4;

    // Character set
    client_charset_ = readInt1(current_packet_.data() + offset);
    offset += 1;

    // Reserved (23 bytes)
    offset += 23;

    // Username (null-terminated)
    size_t username_offset = 0;
    username_ = readNullString(current_packet_.data() + offset, username_offset, current_packet_.size() - offset);
    offset += username_offset;

    // Auth response
    if (client_capabilities_ & mysql::Capability::PLUGIN_AUTH_LENENC_DATA) {
        auth_response_ = readLenEncString(current_packet_.data(), offset, current_packet_.size());
    } else if (client_capabilities_ & mysql::Capability::SECURE_CONNECTION) {
        if (offset < current_packet_.size()) {
            uint8_t len = readInt1(current_packet_.data() + offset);
            offset += 1;
            if (offset + len <= current_packet_.size()) {
                auth_response_.assign(reinterpret_cast<const char*>(current_packet_.data() + offset), len);
                offset += len;
            }
        }
    } else {
        size_t auth_offset = 0;
        auth_response_ = readNullString(current_packet_.data() + offset, auth_offset, current_packet_.size() - offset);
        offset += auth_offset;
    }

    // Database (if CONNECT_WITH_DB)
    if ((client_capabilities_ & mysql::Capability::CONNECT_WITH_DB) && offset < current_packet_.size()) {
        size_t db_offset = 0;
        database_name_ = readNullString(current_packet_.data() + offset, db_offset, current_packet_.size() - offset);
        offset += db_offset;
    }

    // Auth plugin name (if PLUGIN_AUTH)
    if ((client_capabilities_ & mysql::Capability::PLUGIN_AUTH) && offset < current_packet_.size()) {
        size_t plugin_offset = 0;
        auth_plugin_name_ = readNullString(current_packet_.data() + offset, plugin_offset, current_packet_.size() - offset);
    }

    // For testing, accept any authentication
    // TODO: Implement proper password validation
    return sendAuthResult(conn, true);
}

// ============================================================================
// Command Handling
// ============================================================================

core::Status MySqlAdapter::handleCommand(network::Connection* conn) {
    if (current_packet_.empty()) {
        sendErrorPacket(conn, mysql::ErrorCode::UNKNOWN_COM_ERROR, "HY000",
                       "Empty command packet");
        return core::Status::INVALID_ARGUMENT;
    }

    uint8_t command = current_packet_[0];
    resetSequence();

    switch (command) {
        case mysql::Command::COM_QUERY:
            return handleComQuery(conn);

        case mysql::Command::COM_INIT_DB:
            return handleComInitDb(conn);

        case mysql::Command::COM_PING:
            return handleComPing(conn);

        case mysql::Command::COM_QUIT:
            return handleComQuit(conn);

        case mysql::Command::COM_STMT_PREPARE:
            return handleComStmtPrepare(conn);

        case mysql::Command::COM_STMT_EXECUTE:
            return handleComStmtExecute(conn);

        case mysql::Command::COM_STMT_CLOSE:
            return handleComStmtClose(conn);

        case mysql::Command::COM_STMT_RESET:
            return handleComStmtReset(conn);

        case mysql::Command::COM_FIELD_LIST:
            return handleComFieldList(conn);

        case mysql::Command::COM_STATISTICS:
            return handleComStatistics(conn);

        case mysql::Command::COM_RESET_CONNECTION:
            return handleComResetConnection(conn);

        default:
            sendErrorPacket(conn, mysql::ErrorCode::UNKNOWN_COM_ERROR, "HY000",
                           "Unknown command: " + std::to_string(command));
            return sendBuffer(conn);
    }
}

core::Status MySqlAdapter::handleComQuery(network::Connection* conn) {
    if (current_packet_.size() < 2) {
        sendErrorPacket(conn, mysql::ErrorCode::SYNTAX_ERROR, "42000",
                       "Empty query");
        return sendBuffer(conn);
    }

    std::string query(reinterpret_cast<const char*>(current_packet_.data() + 1),
                      current_packet_.size() - 1);

    // Execute query
    QueryContext ctx;
    ctx.query = query;

    ResultContext result;
    executeQuery(ctx, result);

    sendQueryResult(conn, result);
    return sendBuffer(conn);
}

core::Status MySqlAdapter::handleComInitDb(network::Connection* conn) {
    if (current_packet_.size() < 2) {
        sendErrorPacket(conn, mysql::ErrorCode::BAD_DB_ERROR, "42000",
                       "No database specified");
        return sendBuffer(conn);
    }

    database_name_.assign(reinterpret_cast<const char*>(current_packet_.data() + 1),
                          current_packet_.size() - 1);

    // TODO: Validate database exists
    sendOkPacket(conn);
    return sendBuffer(conn);
}

core::Status MySqlAdapter::handleComPing(network::Connection* conn) {
    sendOkPacket(conn);
    return sendBuffer(conn);
}

core::Status MySqlAdapter::handleComQuit(network::Connection* conn) {
    mysql_state_ = MySqlProtocolState::CLOSING;
    conn->close(network::CloseReason::CLIENT_DISCONNECT);
    return core::Status::OK;
}

core::Status MySqlAdapter::handleComStmtPrepare(network::Connection* conn) {
    if (current_packet_.size() < 2) {
        sendErrorPacket(conn, mysql::ErrorCode::SYNTAX_ERROR, "42000",
                       "Empty statement");
        return sendBuffer(conn);
    }

    std::string query(reinterpret_cast<const char*>(current_packet_.data() + 1),
                      current_packet_.size() - 1);

    // Create prepared statement
    MySqlPreparedStatement stmt;
    stmt.id = next_stmt_id_++;
    stmt.query = query;
    stmt.num_params = 0;  // TODO: Parse query for parameters
    stmt.num_columns = 0;

    prepared_statements_[stmt.id] = stmt;

    sendPrepareOk(conn, stmt.id, stmt.num_columns, stmt.num_params);
    return sendBuffer(conn);
}

core::Status MySqlAdapter::handleComStmtExecute(network::Connection* conn) {
    if (current_packet_.size() < 5) {
        sendErrorPacket(conn, mysql::ErrorCode::UNKNOWN_ERROR, "HY000",
                       "Invalid execute packet");
        return sendBuffer(conn);
    }

    uint32_t stmt_id = readInt4(current_packet_.data() + 1);

    auto it = prepared_statements_.find(stmt_id);
    if (it == prepared_statements_.end()) {
        sendErrorPacket(conn, mysql::ErrorCode::UNKNOWN_ERROR, "HY000",
                       "Unknown statement ID: " + std::to_string(stmt_id));
        return sendBuffer(conn);
    }

    // Execute the prepared statement
    QueryContext ctx;
    ctx.query = it->second.query;

    ResultContext result;
    executeQuery(ctx, result);

    sendQueryResult(conn, result);
    return sendBuffer(conn);
}

core::Status MySqlAdapter::handleComStmtClose(network::Connection* conn) {
    if (current_packet_.size() < 5) {
        return core::Status::OK;  // Silently ignore malformed close
    }

    uint32_t stmt_id = readInt4(current_packet_.data() + 1);
    prepared_statements_.erase(stmt_id);

    // No response for COM_STMT_CLOSE
    (void)conn;
    return core::Status::OK;
}

core::Status MySqlAdapter::handleComStmtReset(network::Connection* conn) {
    if (current_packet_.size() < 5) {
        sendErrorPacket(conn, mysql::ErrorCode::UNKNOWN_ERROR, "HY000",
                       "Invalid reset packet");
        return sendBuffer(conn);
    }

    uint32_t stmt_id = readInt4(current_packet_.data() + 1);

    if (prepared_statements_.find(stmt_id) == prepared_statements_.end()) {
        sendErrorPacket(conn, mysql::ErrorCode::UNKNOWN_ERROR, "HY000",
                       "Unknown statement ID");
        return sendBuffer(conn);
    }

    sendOkPacket(conn);
    return sendBuffer(conn);
}

core::Status MySqlAdapter::handleComFieldList(network::Connection* conn) {
    // Deprecated command, return empty result
    sendEofPacket(conn);
    return sendBuffer(conn);
}

core::Status MySqlAdapter::handleComStatistics(network::Connection* conn) {
    // Return simple statistics string
    std::string stats = "Uptime: 0  Threads: 1  Questions: " +
                        std::to_string(queries_executed_) +
                        "  Slow queries: 0  Opens: 0  Flush tables: 0  " +
                        "Open tables: 0  Queries per second avg: 0.000";

    std::vector<uint8_t> payload(stats.begin(), stats.end());
    sendPacket(conn, payload);
    return sendBuffer(conn);
}

core::Status MySqlAdapter::handleComResetConnection(network::Connection* conn) {
    // Reset session state
    in_transaction_ = false;
    server_status_ = mysql::ServerStatus::AUTOCOMMIT;
    prepared_statements_.clear();

    sendOkPacket(conn);
    return sendBuffer(conn);
}

// ============================================================================
// Packet Sending
// ============================================================================

void MySqlAdapter::sendPacket(network::Connection* conn, const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> header(4);

    // Length (3 bytes, little-endian)
    writeInt3(header, static_cast<uint32_t>(payload.size()));

    // Sequence ID
    header[3] = sequence_id_++;

    writeToBuffer(conn, header.data(), header.size());
    if (!payload.empty()) {
        writeToBuffer(conn, payload.data(), payload.size());
    }
}

void MySqlAdapter::sendOkPacket(network::Connection* conn, uint64_t affected_rows,
                                 uint64_t last_insert_id, const std::string& info) {
    std::vector<uint8_t> payload;

    // Header
    writeInt1(payload, mysql::OK_PACKET);

    // Affected rows
    writeLenEncInt(payload, affected_rows);

    // Last insert ID
    writeLenEncInt(payload, last_insert_id);

    // Status flags
    if (client_capabilities_ & mysql::Capability::PROTOCOL_41) {
        writeInt2(payload, server_status_);
        writeInt2(payload, 0);  // Warnings
    }

    // Info (session state changes or message)
    if (!info.empty() && (client_capabilities_ & mysql::Capability::SESSION_TRACK)) {
        writeLenEncString(payload, info);
    }

    sendPacket(conn, payload);
}

void MySqlAdapter::sendEofPacket(network::Connection* conn) {
    std::vector<uint8_t> payload;

    writeInt1(payload, mysql::EOF_PACKET);

    if (client_capabilities_ & mysql::Capability::PROTOCOL_41) {
        writeInt2(payload, 0);  // Warnings
        writeInt2(payload, server_status_);
    }

    sendPacket(conn, payload);
}

void MySqlAdapter::sendErrorPacket(network::Connection* conn, uint16_t error_code,
                                    const std::string& sqlstate, const std::string& message) {
    std::vector<uint8_t> payload;

    // Header
    writeInt1(payload, mysql::ERR_PACKET);

    // Error code
    writeInt2(payload, error_code);

    // SQL state marker and state (if PROTOCOL_41)
    if (client_capabilities_ & mysql::Capability::PROTOCOL_41) {
        writeInt1(payload, '#');
        // SQL state (5 characters)
        std::string state = sqlstate;
        if (state.size() < 5) state.resize(5, ' ');
        payload.insert(payload.end(), state.begin(), state.begin() + 5);
    }

    // Error message
    payload.insert(payload.end(), message.begin(), message.end());

    sendPacket(conn, payload);
}

void MySqlAdapter::sendResultSetHeader(network::Connection* conn, uint64_t column_count) {
    std::vector<uint8_t> payload;
    writeLenEncInt(payload, column_count);
    sendPacket(conn, payload);
}

void MySqlAdapter::sendColumnDefinition(network::Connection* conn,
                                         const ProtocolCodec::ColumnInfo& col,
                                         const std::string& schema,
                                         const std::string& table,
                                         const std::string& org_table,
                                         const std::string& org_name) {
    std::vector<uint8_t> payload;

    // Catalog (always "def")
    writeLenEncString(payload, "def");

    // Schema
    writeLenEncString(payload, schema.empty() ? database_name_ : schema);

    // Table (virtual)
    writeLenEncString(payload, table.empty() ? "" : table);

    // Original table
    writeLenEncString(payload, org_table.empty() ? "" : org_table);

    // Column name
    writeLenEncString(payload, col.name);

    // Original column name
    writeLenEncString(payload, org_name.empty() ? col.name : org_name);

    // Length of fixed fields
    writeLenEncInt(payload, 0x0c);

    // Character set
    writeInt2(payload, mysql::Charset::UTF8MB4_GENERAL_CI);

    // Column length
    writeInt4(payload, 255);  // TODO: Use actual column length

    // Column type
    writeInt1(payload, wireTypeToMySqlType(col.type));

    // Flags
    writeInt2(payload, 0);

    // Decimals
    writeInt1(payload, 0);

    // Filler
    writeInt2(payload, 0);

    sendPacket(conn, payload);
}

void MySqlAdapter::sendResultRow(network::Connection* conn,
                                  const std::vector<ProtocolCodec::ColumnValue>& values) {
    std::vector<uint8_t> payload;

    for (const auto& val : values) {
        if (val.is_null) {
            writeInt1(payload, 0xfb);  // NULL indicator
        } else {
            // Convert vector<uint8_t> to string
            std::string data_str(val.data.begin(), val.data.end());
            writeLenEncString(payload, data_str);
        }
    }

    sendPacket(conn, payload);
}

void MySqlAdapter::sendPrepareOk(network::Connection* conn, uint32_t stmt_id,
                                  uint16_t num_columns, uint16_t num_params) {
    std::vector<uint8_t> payload;

    // Status (OK)
    writeInt1(payload, 0x00);

    // Statement ID
    writeInt4(payload, stmt_id);

    // Number of columns
    writeInt2(payload, num_columns);

    // Number of parameters
    writeInt2(payload, num_params);

    // Reserved
    writeInt1(payload, 0x00);

    // Warning count
    writeInt2(payload, 0);

    sendPacket(conn, payload);

    // Send parameter definitions if any
    // (skipped for now)

    // Send column definitions if any
    // (skipped for now)
}

// ============================================================================
// Helper Methods - Integer I/O
// ============================================================================

void MySqlAdapter::writeInt1(std::vector<uint8_t>& buf, uint8_t value) {
    buf.push_back(value);
}

void MySqlAdapter::writeInt2(std::vector<uint8_t>& buf, uint16_t value) {
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
}

void MySqlAdapter::writeInt3(std::vector<uint8_t>& buf, uint32_t value) {
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
}

void MySqlAdapter::writeInt4(std::vector<uint8_t>& buf, uint32_t value) {
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

void MySqlAdapter::writeInt8(std::vector<uint8_t>& buf, uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        buf.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

uint8_t MySqlAdapter::readInt1(const uint8_t* data) {
    return data[0];
}

uint16_t MySqlAdapter::readInt2(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t MySqlAdapter::readInt3(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16);
}

uint32_t MySqlAdapter::readInt4(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

uint64_t MySqlAdapter::readInt8(const uint8_t* data) {
    uint64_t result = 0;
    for (int i = 0; i < 8; ++i) {
        result |= static_cast<uint64_t>(data[i]) << (i * 8);
    }
    return result;
}

// ============================================================================
// Helper Methods - Length-Encoded Values
// ============================================================================

void MySqlAdapter::writeLenEncInt(std::vector<uint8_t>& buf, uint64_t value) {
    if (value < 251) {
        writeInt1(buf, static_cast<uint8_t>(value));
    } else if (value < 65536) {
        writeInt1(buf, 0xfc);
        writeInt2(buf, static_cast<uint16_t>(value));
    } else if (value < 16777216) {
        writeInt1(buf, 0xfd);
        writeInt3(buf, static_cast<uint32_t>(value));
    } else {
        writeInt1(buf, 0xfe);
        writeInt8(buf, value);
    }
}

uint64_t MySqlAdapter::readLenEncInt(const uint8_t* data, size_t& offset, size_t max_len) {
    if (offset >= max_len) return 0;

    uint8_t first = data[offset++];

    if (first < 251) {
        return first;
    } else if (first == 0xfc) {
        if (offset + 2 > max_len) return 0;
        uint64_t val = readInt2(data + offset);
        offset += 2;
        return val;
    } else if (first == 0xfd) {
        if (offset + 3 > max_len) return 0;
        uint64_t val = readInt3(data + offset);
        offset += 3;
        return val;
    } else if (first == 0xfe) {
        if (offset + 8 > max_len) return 0;
        uint64_t val = readInt8(data + offset);
        offset += 8;
        return val;
    }
    return 0;  // 0xff is error/NULL
}

void MySqlAdapter::writeLenEncString(std::vector<uint8_t>& buf, const std::string& str) {
    writeLenEncInt(buf, str.size());
    buf.insert(buf.end(), str.begin(), str.end());
}

std::string MySqlAdapter::readLenEncString(const uint8_t* data, size_t& offset, size_t max_len) {
    uint64_t len = readLenEncInt(data, offset, max_len);
    if (offset + len > max_len) return "";

    std::string result(reinterpret_cast<const char*>(data + offset), static_cast<size_t>(len));
    offset += static_cast<size_t>(len);
    return result;
}

void MySqlAdapter::writeNullString(std::vector<uint8_t>& buf, const std::string& str) {
    buf.insert(buf.end(), str.begin(), str.end());
    buf.push_back(0);
}

std::string MySqlAdapter::readNullString(const uint8_t* data, size_t& offset, size_t max_len) {
    std::string result;
    while (offset < max_len && data[offset] != 0) {
        result.push_back(static_cast<char>(data[offset++]));
    }
    if (offset < max_len) ++offset;  // Skip null terminator
    return result;
}

// ============================================================================
// Type Conversion
// ============================================================================

uint8_t MySqlAdapter::wireTypeToMySqlType(WireType type) {
    switch (type) {
        case WireType::BOOLEAN: return mysql::FieldType::TINY;
        case WireType::INT16: return mysql::FieldType::SHORT;
        case WireType::INT32: return mysql::FieldType::LONG;
        case WireType::INT64: return mysql::FieldType::LONGLONG;
        case WireType::FLOAT32: return mysql::FieldType::FLOAT;
        case WireType::FLOAT64: return mysql::FieldType::DOUBLE;
        case WireType::DECIMAL: return mysql::FieldType::NEWDECIMAL;
        case WireType::VARCHAR: return mysql::FieldType::VAR_STRING;
        case WireType::CHAR: return mysql::FieldType::STRING;
        case WireType::BYTEA: return mysql::FieldType::BLOB;
        case WireType::DATE: return mysql::FieldType::DATE;
        case WireType::TIME: return mysql::FieldType::TIME;
        case WireType::TIMESTAMP: return mysql::FieldType::TIMESTAMP;
        case WireType::TIMESTAMPTZ: return mysql::FieldType::TIMESTAMP;
        case WireType::INTERVAL: return mysql::FieldType::VAR_STRING;
        case WireType::UUID: return mysql::FieldType::VAR_STRING;
        case WireType::JSON: return mysql::FieldType::JSON;
        case WireType::JSONB: return mysql::FieldType::JSON;
        default: return mysql::FieldType::VAR_STRING;
    }
}

WireType MySqlAdapter::mysqlTypeToWireType(uint8_t type) {
    switch (type) {
        case mysql::FieldType::TINY: return WireType::INT16;
        case mysql::FieldType::SHORT: return WireType::INT16;
        case mysql::FieldType::LONG: return WireType::INT32;
        case mysql::FieldType::LONGLONG: return WireType::INT64;
        case mysql::FieldType::FLOAT: return WireType::FLOAT32;
        case mysql::FieldType::DOUBLE: return WireType::FLOAT64;
        case mysql::FieldType::DECIMAL:
        case mysql::FieldType::NEWDECIMAL: return WireType::DECIMAL;
        case mysql::FieldType::VARCHAR:
        case mysql::FieldType::VAR_STRING:
        case mysql::FieldType::STRING: return WireType::VARCHAR;
        case mysql::FieldType::BLOB:
        case mysql::FieldType::TINY_BLOB:
        case mysql::FieldType::MEDIUM_BLOB:
        case mysql::FieldType::LONG_BLOB: return WireType::BYTEA;
        case mysql::FieldType::DATE:
        case mysql::FieldType::NEWDATE: return WireType::DATE;
        case mysql::FieldType::TIME:
        case mysql::FieldType::TIME2: return WireType::TIME;
        case mysql::FieldType::TIMESTAMP:
        case mysql::FieldType::TIMESTAMP2:
        case mysql::FieldType::DATETIME:
        case mysql::FieldType::DATETIME2: return WireType::TIMESTAMP;
        case mysql::FieldType::JSON: return WireType::JSON;
        default: return WireType::VARCHAR;
    }
}

// ============================================================================
// Authentication
// ============================================================================

std::vector<uint8_t> MySqlAdapter::computeNativePasswordAuth(const std::string& password,
                                                              const uint8_t* scramble) {
    // mysql_native_password: SHA1(password) XOR SHA1(scramble + SHA1(SHA1(password)))
    std::vector<uint8_t> result;

#ifdef HAVE_OPENSSL
    if (password.empty()) {
        return result;  // Empty password = empty auth response
    }

    unsigned char sha1_pass[SHA_DIGEST_LENGTH];
    unsigned char sha1_sha1_pass[SHA_DIGEST_LENGTH];
    unsigned char sha1_combined[SHA_DIGEST_LENGTH];

    // SHA1(password)
    SHA1(reinterpret_cast<const unsigned char*>(password.c_str()),
         password.length(), sha1_pass);

    // SHA1(SHA1(password))
    SHA1(sha1_pass, SHA_DIGEST_LENGTH, sha1_sha1_pass);

    // SHA1(scramble + SHA1(SHA1(password)))
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, scramble, 20);
    SHA1_Update(&ctx, sha1_sha1_pass, SHA_DIGEST_LENGTH);
    SHA1_Final(sha1_combined, &ctx);

    // XOR
    result.resize(SHA_DIGEST_LENGTH);
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        result[i] = sha1_pass[i] ^ sha1_combined[i];
    }
#else
    // Fallback: return empty (trust auth)
    (void)password;
    (void)scramble;
#endif

    return result;
}

} // namespace protocol
} // namespace scratchbird
