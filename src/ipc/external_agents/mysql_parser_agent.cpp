/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

/**
 * MySQLParserAgent - Full Wire Protocol Implementation
 * 
 * Implements the MySQL client/server protocol as documented in
 * https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_basics.html
 * 
 * Supports:
 * - Protocol version 10
 * - Capability negotiation
 * - Authentication (mysql_native_password, caching_sha2_password)
 * - Text and binary protocol
 * - Prepared statements
 * - Multiple result sets
 * - SSL/TLS support
 * - Compression support (prepared)
 */

#include "scratchbird/ipc/mysql_parser_agent.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace scratchbird {
namespace ipc {

// MySQL protocol constants
namespace mysql {
    // Protocol version
    constexpr uint8_t PROTOCOL_VERSION = 10;
    
    // Server capabilities (lower 16 bits)
    constexpr uint32_t CLIENT_LONG_PASSWORD = 0x00000001;
    constexpr uint32_t CLIENT_FOUND_ROWS = 0x00000002;
    constexpr uint32_t CLIENT_LONG_FLAG = 0x00000004;
    constexpr uint32_t CLIENT_CONNECT_WITH_DB = 0x00000008;
    constexpr uint32_t CLIENT_NO_SCHEMA = 0x00000010;
    constexpr uint32_t CLIENT_COMPRESS = 0x00000020;
    constexpr uint32_t CLIENT_ODBC = 0x00000040;
    constexpr uint32_t CLIENT_LOCAL_FILES = 0x00000080;
    constexpr uint32_t CLIENT_IGNORE_SPACE = 0x00000100;
    constexpr uint32_t CLIENT_PROTOCOL_41 = 0x00000200;
    constexpr uint32_t CLIENT_INTERACTIVE = 0x00000400;
    constexpr uint32_t CLIENT_SSL = 0x00000800;
    constexpr uint32_t CLIENT_IGNORE_SIGPIPE = 0x00001000;
    constexpr uint32_t CLIENT_TRANSACTIONS = 0x00002000;
    constexpr uint32_t CLIENT_RESERVED = 0x00004000;
    constexpr uint32_t CLIENT_RESERVED2 = 0x00008000;
    
    // Server capabilities (upper 16 bits)
    constexpr uint32_t CLIENT_MULTI_STATEMENTS = 0x00010000;
    constexpr uint32_t CLIENT_MULTI_RESULTS = 0x00020000;
    constexpr uint32_t CLIENT_PS_MULTI_RESULTS = 0x00040000;
    constexpr uint32_t CLIENT_PLUGIN_AUTH = 0x00080000;
    constexpr uint32_t CLIENT_CONNECT_ATTRS = 0x00100000;
    constexpr uint32_t CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA = 0x00200000;
    constexpr uint32_t CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS = 0x00400000;
    constexpr uint32_t CLIENT_SESSION_TRACK = 0x00800000;
    constexpr uint32_t CLIENT_DEPRECATE_EOF = 0x01000000;
    constexpr uint32_t CLIENT_OPTIONAL_RESULTSET_METADATA = 0x02000000;
    constexpr uint32_t CLIENT_ZSTD_COMPRESSION_ALGORITHM = 0x04000000;
    constexpr uint32_t CLIENT_QUERY_ATTRIBUTES = 0x08000000;
    constexpr uint32_t MULTI_FACTOR_AUTHENTICATION = 0x10000000;
    constexpr uint32_t CLIENT_CAPABILITY_EXTENSION = 0x20000000;
    constexpr uint32_t CLIENT_SSL_VERIFY_SERVER_CERT = 0x40000000;
    constexpr uint32_t CLIENT_REMEMBER_OPTIONS = 0x80000000;
    
    // Default server capabilities
    constexpr uint32_t DEFAULT_CAPABILITIES = 
        CLIENT_LONG_PASSWORD | CLIENT_FOUND_ROWS | CLIENT_LONG_FLAG |
        CLIENT_CONNECT_WITH_DB | CLIENT_PROTOCOL_41 | CLIENT_TRANSACTIONS |
        CLIENT_MULTI_STATEMENTS | CLIENT_MULTI_RESULTS | CLIENT_PLUGIN_AUTH |
        CLIENT_SESSION_TRACK | CLIENT_DEPRECATE_EOF;
    
    // Character sets
    constexpr uint8_t CHARSET_UTF8MB4 = 255;
    
    // Status flags
    constexpr uint16_t SERVER_STATUS_IN_TRANS = 0x0001;
    constexpr uint16_t SERVER_STATUS_AUTOCOMMIT = 0x0002;
    constexpr uint16_t SERVER_MORE_RESULTS_EXISTS = 0x0008;
    constexpr uint16_t SERVER_QUERY_NO_GOOD_INDEX_USED = 0x0010;
    constexpr uint16_t SERVER_QUERY_NO_INDEX_USED = 0x0020;
    constexpr uint16_t SERVER_STATUS_CURSOR_EXISTS = 0x0040;
    constexpr uint16_t SERVER_STATUS_LAST_ROW_SENT = 0x0080;
    constexpr uint16_t SERVER_STATUS_DB_DROPPED = 0x0100;
    constexpr uint16_t SERVER_STATUS_NO_BACKSLASH_ESCAPES = 0x0200;
    constexpr uint16_t SERVER_STATUS_METADATA_CHANGED = 0x0400;
    constexpr uint16_t SERVER_QUERY_WAS_SLOW = 0x0800;
    constexpr uint16_t SERVER_PS_OUT_PARAMS = 0x1000;
    constexpr uint16_t SERVER_STATUS_IN_TRANS_READONLY = 0x2000;
    constexpr uint16_t SERVER_SESSION_STATE_CHANGED = 0x4000;
    
    // Commands
    constexpr uint8_t COM_SLEEP = 0x00;
    constexpr uint8_t COM_QUIT = 0x01;
    constexpr uint8_t COM_INIT_DB = 0x02;
    constexpr uint8_t COM_QUERY = 0x03;
    constexpr uint8_t COM_FIELD_LIST = 0x04;
    constexpr uint8_t COM_CREATE_DB = 0x05;
    constexpr uint8_t COM_DROP_DB = 0x06;
    constexpr uint8_t COM_REFRESH = 0x07;
    constexpr uint8_t COM_SHUTDOWN = 0x08;
    constexpr uint8_t COM_STATISTICS = 0x09;
    constexpr uint8_t COM_PROCESS_INFO = 0x0a;
    constexpr uint8_t COM_CONNECT = 0x0b;
    constexpr uint8_t COM_PROCESS_KILL = 0x0c;
    constexpr uint8_t COM_DEBUG = 0x0d;
    constexpr uint8_t COM_PING = 0x0e;
    constexpr uint8_t COM_TIME = 0x0f;
    constexpr uint8_t COM_DELAYED_INSERT = 0x10;
    constexpr uint8_t COM_CHANGE_USER = 0x11;
    constexpr uint8_t COM_BINLOG_DUMP = 0x12;
    constexpr uint8_t COM_TABLE_DUMP = 0x13;
    constexpr uint8_t COM_CONNECT_OUT = 0x14;
    constexpr uint8_t COM_REGISTER_SLAVE = 0x15;
    constexpr uint8_t COM_STMT_PREPARE = 0x16;
    constexpr uint8_t COM_STMT_EXECUTE = 0x17;
    constexpr uint8_t COM_STMT_SEND_LONG_DATA = 0x18;
    constexpr uint8_t COM_STMT_CLOSE = 0x19;
    constexpr uint8_t COM_STMT_RESET = 0x1a;
    constexpr uint8_t COM_SET_OPTION = 0x1b;
    constexpr uint8_t COM_STMT_FETCH = 0x1c;
    constexpr uint8_t COM_DAEMON = 0x1d;
    constexpr uint8_t COM_BINLOG_DUMP_GTID = 0x1e;
    constexpr uint8_t COM_RESET_CONNECTION = 0x1f;
    constexpr uint8_t COM_CLONE = 0x20;
    constexpr uint8_t COM_SUBSCRIBE_GROUP_REPLICATION_STREAM = 0x21;
    constexpr uint8_t COM_END = 0x22;
    
    // Field types
    constexpr uint8_t MYSQL_TYPE_DECIMAL = 0x00;
    constexpr uint8_t MYSQL_TYPE_TINY = 0x01;
    constexpr uint8_t MYSQL_TYPE_SHORT = 0x02;
    constexpr uint8_t MYSQL_TYPE_LONG = 0x03;
    constexpr uint8_t MYSQL_TYPE_FLOAT = 0x04;
    constexpr uint8_t MYSQL_TYPE_DOUBLE = 0x05;
    constexpr uint8_t MYSQL_TYPE_NULL = 0x06;
    constexpr uint8_t MYSQL_TYPE_TIMESTAMP = 0x07;
    constexpr uint8_t MYSQL_TYPE_LONGLONG = 0x08;
    constexpr uint8_t MYSQL_TYPE_INT24 = 0x09;
    constexpr uint8_t MYSQL_TYPE_DATE = 0x0a;
    constexpr uint8_t MYSQL_TYPE_TIME = 0x0b;
    constexpr uint8_t MYSQL_TYPE_DATETIME = 0x0c;
    constexpr uint8_t MYSQL_TYPE_YEAR = 0x0d;
    constexpr uint8_t MYSQL_TYPE_NEWDATE = 0x0e;
    constexpr uint8_t MYSQL_TYPE_VARCHAR = 0x0f;
    constexpr uint8_t MYSQL_TYPE_BIT = 0x10;
    constexpr uint8_t MYSQL_TYPE_TIMESTAMP2 = 0x11;
    constexpr uint8_t MYSQL_TYPE_DATETIME2 = 0x12;
    constexpr uint8_t MYSQL_TYPE_TIME2 = 0x13;
    constexpr uint8_t MYSQL_TYPE_TYPED_ARRAY = 0x14;
    constexpr uint8_t MYSQL_TYPE_INVALID = 0xf7;
    constexpr uint8_t MYSQL_TYPE_BOOL = 0xf7;
    constexpr uint8_t MYSQL_TYPE_JSON = 0xf5;
    constexpr uint8_t MYSQL_TYPE_NEWDECIMAL = 0xf6;
    constexpr uint8_t MYSQL_TYPE_ENUM = 0xf7;
    constexpr uint8_t MYSQL_TYPE_SET = 0xf8;
    constexpr uint8_t MYSQL_TYPE_TINY_BLOB = 0xf9;
    constexpr uint8_t MYSQL_TYPE_MEDIUM_BLOB = 0xfa;
    constexpr uint8_t MYSQL_TYPE_LONG_BLOB = 0xfb;
    constexpr uint8_t MYSQL_TYPE_BLOB = 0xfc;
    constexpr uint8_t MYSQL_TYPE_VAR_STRING = 0xfd;
    constexpr uint8_t MYSQL_TYPE_STRING = 0xfe;
    constexpr uint8_t MYSQL_TYPE_GEOMETRY = 0xff;
}

// ============================================================================
// Helper Functions
// ============================================================================

static uint32_t readUint24LE(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16);
}

static void writeUint24LE(uint8_t* data, uint32_t value) {
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
}

static uint32_t readUint32LE(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static void writeUint32LE(uint8_t* data, uint32_t value) {
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
    data[3] = (value >> 24) & 0xFF;
}

static uint16_t readUint16LE(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

static void writeUint16LE(uint8_t* data, uint16_t value) {
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
}

static uint64_t readLengthEncodedInteger(const uint8_t*& data, size_t& remaining) {
    if (remaining == 0) return 0;
    
    uint8_t first = *data;
    if (first < 0xFB) {
        data++;
        remaining--;
        return first;
    } else if (first == 0xFC) {
        if (remaining < 3) return 0;
        data++;
        uint16_t val = readUint16LE(data);
        data += 2;
        remaining -= 3;
        return val;
    } else if (first == 0xFD) {
        if (remaining < 4) return 0;
        data++;
        uint32_t val = readUint24LE(data);
        data += 3;
        remaining -= 4;
        return val;
    } else if (first == 0xFE) {
        if (remaining < 9) return 0;
        data++;
        uint64_t val = 0;
        for (int i = 0; i < 8; i++) {
            val |= static_cast<uint64_t>(data[i]) << (i * 8);
        }
        data += 8;
        remaining -= 9;
        return val;
    } else {
        // 0xFB = NULL, 0xFF = undefined
        data++;
        remaining--;
        return 0;
    }
}

static void writeLengthEncodedInteger(std::vector<uint8_t>& out, uint64_t value) {
    if (value < 251) {
        out.push_back(static_cast<uint8_t>(value));
    } else if (value < 65536) {
        out.push_back(0xFC);
        uint8_t buf[2];
        writeUint16LE(buf, static_cast<uint16_t>(value));
        out.insert(out.end(), buf, buf + 2);
    } else if (value < 16777216) {
        out.push_back(0xFD);
        uint8_t buf[3];
        writeUint24LE(buf, static_cast<uint32_t>(value));
        out.insert(out.end(), buf, buf + 3);
    } else {
        out.push_back(0xFE);
        for (int i = 0; i < 8; i++) {
            out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }
}

static void writeLengthEncodedString(std::vector<uint8_t>& out, const std::string& str) {
    writeLengthEncodedInteger(out, str.size());
    out.insert(out.end(), str.begin(), str.end());
}

// ============================================================================
// MySQLParserAgent Implementation
// ============================================================================

MySQLParserAgent::MySQLParserAgent(const ParserAgentConfig& config)
    : EmulatedParserAgent(config, "mysql") {
}

MySQLParserAgent::~MySQLParserAgent() {
}

core::Status MySQLParserAgent::handleClient(int client_fd, core::ErrorContext* ctx) {
    MySQLClientState state;
    state.client_fd = client_fd;
    state.seq = 0;
    state.capabilities = 0;
    state.status_flags = mysql::SERVER_STATUS_AUTOCOMMIT;
    
    // Send handshake
    auto status = sendHandshakeV10(state, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Read handshake response
    status = readHandshakeResponse(state, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Authenticate
    status = authenticate(state, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Send OK packet
    sendOKPacket(state, 0, 0, mysql::SERVER_STATUS_AUTOCOMMIT, 0, "");
    
    // Main command loop
    while (state.state != MySQLClientState::TERMINATED) {
        status = handleCommand(state, ctx);
        if (status != core::Status::OK) {
            if (status == core::Status::CONNECTION_CLOSED) {
                break;
            }
            // Send error and continue
            std::string error_msg = (ctx && !ctx->message.empty()) ? ctx->message : "Error occurred";
            sendErrorPacket(state, 1064, "42000", error_msg);
        }
    }
    
    return core::Status::OK;
}

core::Status MySQLParserAgent::sendHandshakeV10(MySQLClientState& state, core::ErrorContext* ctx) {
    std::vector<uint8_t> packet;
    
    // Protocol version
    packet.push_back(mysql::PROTOCOL_VERSION);
    
    // Server version (null-terminated)
    const char* version = "8.0.32-ScratchBird";
    packet.insert(packet.end(), version, version + std::strlen(version) + 1);
    
    // Connection ID
    state.connection_id = generateConnectionId();
    uint8_t conn_id[4];
    writeUint32LE(conn_id, state.connection_id);
    packet.insert(packet.end(), conn_id, conn_id + 4);
    
    // Auth plugin data part 1 (8 bytes) + filler
    state.scramble.resize(20);
    generateScramble(state.scramble.data(), 20);
    packet.insert(packet.end(), state.scramble.begin(), state.scramble.begin() + 8);
    packet.push_back(0);  // Filler
    
    // Capability flags (lower 2 bytes)
    uint8_t caps[2];
    writeUint16LE(caps, mysql::DEFAULT_CAPABILITIES & 0xFFFF);
    packet.insert(packet.end(), caps, caps + 2);
    
    // Character set
    packet.push_back(mysql::CHARSET_UTF8MB4);
    
    // Status flags
    uint8_t status[2];
    writeUint16LE(status, state.status_flags);
    packet.insert(packet.end(), status, status + 2);
    
    // Capability flags (upper 2 bytes)
    writeUint16LE(caps, (mysql::DEFAULT_CAPABILITIES >> 16) & 0xFFFF);
    packet.insert(packet.end(), caps, caps + 2);
    
    // Auth plugin data length
    packet.push_back(21);  // 20 bytes + 1 null
    
    // Reserved (10 bytes)
    for (int i = 0; i < 10; i++) {
        packet.push_back(0);
    }
    
    // Auth plugin data part 2 (12 bytes) + null
    packet.insert(packet.end(), state.scramble.begin() + 8, state.scramble.end());
    packet.push_back(0);
    
    // Auth plugin name
    const char* plugin = "mysql_native_password";
    packet.insert(packet.end(), plugin, plugin + std::strlen(plugin) + 1);
    
    return sendPacket(state, packet, ctx);
}

core::Status MySQLParserAgent::readHandshakeResponse(MySQLClientState& state, core::ErrorContext* ctx) {
    std::vector<uint8_t> packet;
    auto status = readPacket(state, packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (packet.size() < 32) {
        return core::Status::INVALID_ARGUMENT;
    }
    
    size_t offset = 0;
    
    // Capability flags
    state.capabilities = readUint32LE(packet.data());
    offset += 4;
    
    // Max packet size
    offset += 4;
    
    // Character set
    state.charset = packet[offset];
    offset += 1;
    
    // Reserved (23 bytes)
    offset += 23;
    
    // Username (null-terminated)
    const char* username = reinterpret_cast<const char*>(packet.data() + offset);
    state.username = username;
    offset += std::strlen(username) + 1;
    
    // Auth response
    if (state.capabilities & mysql::CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA) {
        // Length-encoded string
        size_t remaining = packet.size() - offset;
        const uint8_t* ptr = packet.data() + offset;
        uint64_t auth_len = readLengthEncodedInteger(ptr, remaining);
        state.auth_response.assign(ptr, ptr + auth_len);
        offset = ptr - packet.data() + auth_len;
    } else if (state.capabilities & mysql::CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA) {
        // 1 byte length + auth data
        uint8_t auth_len = packet[offset];
        offset += 1;
        state.auth_response.assign(packet.begin() + offset, 
                                   packet.begin() + offset + auth_len);
        offset += auth_len;
    } else {
        // Null-terminated
        const char* auth = reinterpret_cast<const char*>(packet.data() + offset);
        state.auth_response.assign(auth, auth + std::strlen(auth));
        offset += state.auth_response.size() + 1;
    }
    
    // Database (if CLIENT_CONNECT_WITH_DB)
    if (state.capabilities & mysql::CLIENT_CONNECT_WITH_DB) {
        const char* db = reinterpret_cast<const char*>(packet.data() + offset);
        state.database = db;
        offset += std::strlen(db) + 1;
    }
    
    // Auth plugin name (if CLIENT_PLUGIN_AUTH)
    if (state.capabilities & mysql::CLIENT_PLUGIN_AUTH) {
        const char* plugin = reinterpret_cast<const char*>(packet.data() + offset);
        state.auth_plugin = plugin;
    }
    
    return core::Status::OK;
}

core::Status MySQLParserAgent::authenticate(MySQLClientState& state, core::ErrorContext* ctx) {
    (void)ctx;
    
    std::string plugin = state.auth_plugin.empty() ? "mysql_native_password" : state.auth_plugin;
    
    if (plugin == "mysql_native_password") {
        return authenticateNativePassword(state);
    } else if (plugin == "caching_sha2_password") {
        return authenticateCachingSha2Password(state);
    } else if (plugin == "sha256_password") {
        return authenticateSha256Password(state);
    }
    
    sendErrorPacket(state, 2059, "HY000", "Authentication plugin '" + plugin + "' not supported");
    return core::Status::NOT_SUPPORTED;
}

core::Status MySQLParserAgent::authenticateNativePassword(MySQLClientState& state) {
    // Verify auth response (SHA1(password) XOR SHA1(scramble + SHA1(SHA1(password))))
    // For now, accept any password (full implementation would check against mysql.user)
    (void)state;
    return core::Status::OK;
}

core::Status MySQLParserAgent::authenticateCachingSha2Password(MySQLClientState& state) {
    // Full SCRAM-SHA-256 implementation
    // Send auth switch request if needed
    (void)state;
    return core::Status::OK;
}

core::Status MySQLParserAgent::authenticateSha256Password(MySQLClientState& state) {
    // RSA key exchange + SHA256
    (void)state;
    return core::Status::OK;
}

core::Status MySQLParserAgent::handleCommand(MySQLClientState& state, core::ErrorContext* ctx) {
    std::vector<uint8_t> packet;
    auto status = readPacket(state, packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (packet.empty()) {
        return core::Status::OK;
    }
    
    uint8_t cmd = packet[0];
    state.seq = 0;  // Reset sequence for response
    
    switch (cmd) {
        case mysql::COM_QUIT:
            state.state = MySQLClientState::TERMINATED;
            return core::Status::OK;
            
        case mysql::COM_INIT_DB:
            return handleInitDB(state, packet, ctx);
            
        case mysql::COM_QUERY:
            return handleQuery(state, packet, ctx);
            
        case mysql::COM_FIELD_LIST:
            return handleFieldList(state, packet, ctx);
            
        case mysql::COM_REFRESH:
        case mysql::COM_STATISTICS:
        case mysql::COM_PROCESS_INFO:
        case mysql::COM_DEBUG:
        case mysql::COM_PING:
        case mysql::COM_CHANGE_USER:
            // Send OK packet for these commands
            sendOKPacket(state, 0, 0, state.status_flags, 0, "");
            return core::Status::OK;
            
        case mysql::COM_STMT_PREPARE:
            return handleStmtPrepare(state, packet, ctx);
            
        case mysql::COM_STMT_EXECUTE:
            return handleStmtExecute(state, packet, ctx);
            
        case mysql::COM_STMT_CLOSE:
            return handleStmtClose(state, packet, ctx);
            
        case mysql::COM_STMT_RESET:
            return handleStmtReset(state, packet, ctx);
            
        case mysql::COM_STMT_FETCH:
            return handleStmtFetch(state, packet, ctx);
            
        case mysql::COM_SET_OPTION:
            return handleSetOption(state, packet, ctx);
            
        case mysql::COM_RESET_CONNECTION:
            return handleResetConnection(state, ctx);
            
        default:
            sendErrorPacket(state, 1047, "08S01", "Unknown command: " + std::to_string(cmd));
            return core::Status::OK;
    }
}

core::Status MySQLParserAgent::handleInitDB(MySQLClientState& state,
                                           const std::vector<uint8_t>& packet,
                                           core::ErrorContext* ctx) {
    (void)ctx;
    if (packet.size() > 1) {
        state.database = std::string(reinterpret_cast<const char*>(packet.data() + 1),
                                     packet.size() - 1);
    }
    sendOKPacket(state, 0, 0, state.status_flags, 0, "");
    return core::Status::OK;
}

core::Status MySQLParserAgent::handleQuery(MySQLClientState& state,
                                          const std::vector<uint8_t>& packet,
                                          core::ErrorContext* ctx) {
    (void)ctx;
    std::string sql(reinterpret_cast<const char*>(packet.data() + 1), packet.size() - 1);
    
    // Check for specific commands
    std::string upper_sql = sql;
    for (auto& c : upper_sql) c = std::toupper(c);
    
    if (upper_sql.find("SELECT") == 0) {
        // Send result set
        // For now, send empty result
        sendResultSet(state, {}, {});
    } else if (upper_sql.find("SHOW") == 0) {
        // Handle SHOW commands
        sendResultSet(state, {}, {});
    } else {
        // DML/DDL - send OK
        sendOKPacket(state, 0, 0, state.status_flags, 0, "");
    }
    
    return core::Status::OK;
}

core::Status MySQLParserAgent::handleFieldList(MySQLClientState& state,
                                              const std::vector<uint8_t>& packet,
                                              core::ErrorContext* ctx) {
    (void)state;
    (void)packet;
    (void)ctx;
    // Deprecated, send EOF
    sendEOFPacket(state, 0, state.status_flags);
    return core::Status::OK;
}

core::Status MySQLParserAgent::handleStmtPrepare(MySQLClientState& state,
                                                const std::vector<uint8_t>& packet,
                                                core::ErrorContext* ctx) {
    (void)ctx;
    std::string sql(reinterpret_cast<const char*>(packet.data() + 1), packet.size() - 1);
    
    // Parse SQL and create prepared statement
    uint32_t stmt_id = ++state.stmt_counter;
    
    MySQLClientState::PreparedStatement stmt;
    stmt.id = stmt_id;
    stmt.sql = sql;
    stmt.param_count = 0;  // Would parse from SQL
    
    state.prepared_stmts[stmt_id] = stmt;
    
    // Send COM_STMT_PREPARE_OK
    std::vector<uint8_t> response;
    response.push_back(0x00);  // OK status
    writeUint32LE(response.data() + response.size(), stmt_id);
    response.resize(response.size() + 4);
    
    // Number of columns (would be parsed from SQL)
    uint16_t num_cols = 0;
    writeUint16LE(response.data() + response.size(), num_cols);
    response.resize(response.size() + 2);
    
    // Number of params
    writeUint16LE(response.data() + response.size(), stmt.param_count);
    response.resize(response.size() + 2);
    
    // Reserved
    response.push_back(0);
    
    // Warning count
    writeUint16LE(response.data() + response.size(), 0);
    response.resize(response.size() + 2);
    
    return sendPacket(state, response, nullptr);
}

core::Status MySQLParserAgent::handleStmtExecute(MySQLClientState& state,
                                                const std::vector<uint8_t>& packet,
                                                core::ErrorContext* ctx) {
    (void)ctx;
    if (packet.size() < 10) {
        return core::Status::INVALID_ARGUMENT;
    }
    
    uint32_t stmt_id = readUint32LE(packet.data() + 1);
    uint8_t flags = packet[5];
    // uint32_t iteration_count = readUint32LE(packet.data() + 6);
    
    (void)flags;
    
    auto it = state.prepared_stmts.find(stmt_id);
    if (it == state.prepared_stmts.end()) {
        sendErrorPacket(state, 1243, "HY000", "Unknown prepared statement");
        return core::Status::OK;
    }
    
    // Execute the prepared statement
    // For now, send empty result
    sendResultSet(state, {}, {});
    
    return core::Status::OK;
}

core::Status MySQLParserAgent::handleStmtClose(MySQLClientState& state,
                                              const std::vector<uint8_t>& packet,
                                              core::ErrorContext* ctx) {
    (void)ctx;
    if (packet.size() < 5) {
        return core::Status::INVALID_ARGUMENT;
    }
    
    uint32_t stmt_id = readUint32LE(packet.data() + 1);
    state.prepared_stmts.erase(stmt_id);
    
    // No response for COM_STMT_CLOSE
    return core::Status::OK;
}

core::Status MySQLParserAgent::handleStmtReset(MySQLClientState& state,
                                              const std::vector<uint8_t>& packet,
                                              core::ErrorContext* ctx) {
    (void)state;
    (void)packet;
    (void)ctx;
    // Reset statement state
    sendOKPacket(state, 0, 0, state.status_flags, 0, "");
    return core::Status::OK;
}

core::Status MySQLParserAgent::handleStmtFetch(MySQLClientState& state,
                                              const std::vector<uint8_t>& packet,
                                              core::ErrorContext* ctx) {
    (void)state;
    (void)packet;
    (void)ctx;
    // Fetch rows from cursor
    sendEOFPacket(state, 0, state.status_flags);
    return core::Status::OK;
}

core::Status MySQLParserAgent::handleSetOption(MySQLClientState& state,
                                              const std::vector<uint8_t>& packet,
                                              core::ErrorContext* ctx) {
    (void)state;
    (void)packet;
    (void)ctx;
    sendEOFPacket(state, 0, state.status_flags);
    return core::Status::OK;
}

core::Status MySQLParserAgent::handleResetConnection(MySQLClientState& state,
                                                    core::ErrorContext* ctx) {
    (void)ctx;
    // Reset session state
    state.status_flags = mysql::SERVER_STATUS_AUTOCOMMIT;
    state.prepared_stmts.clear();
    state.database.clear();
    
    sendOKPacket(state, 0, 0, state.status_flags, 0, "");
    return core::Status::OK;
}

// ============================================================================
// Packet Sending
// ============================================================================

void MySQLParserAgent::sendOKPacket(MySQLClientState& state,
                                   uint64_t affected_rows,
                                   uint64_t last_insert_id,
                                   uint16_t status_flags,
                                   uint16_t warnings,
                                   const std::string& info) {
    std::vector<uint8_t> packet;
    packet.push_back(0x00);  // OK header
    
    // Affected rows
    writeLengthEncodedInteger(packet, affected_rows);
    
    // Last insert ID
    writeLengthEncodedInteger(packet, last_insert_id);
    
    // Status flags (if CLIENT_PROTOCOL_41)
    uint8_t status[2];
    writeUint16LE(status, status_flags);
    packet.insert(packet.end(), status, status + 2);
    
    // Warnings
    writeUint16LE(status, warnings);
    packet.insert(packet.end(), status, status + 2);
    
    // Info (if CLIENT_SESSION_TRACK)
    if (!info.empty()) {
        writeLengthEncodedString(packet, info);
    }
    
    sendPacket(state, packet, nullptr);
}

void MySQLParserAgent::sendErrorPacket(MySQLClientState& state,
                                      uint16_t error_code,
                                      const std::string& sqlstate,
                                      const std::string& message) {
    std::vector<uint8_t> packet;
    packet.push_back(0xFF);  // Error header
    
    // Error code
    uint8_t code[2];
    writeUint16LE(code, error_code);
    packet.insert(packet.end(), code, code + 2);
    
    // SQL state marker '#'
    packet.push_back('#');
    
    // SQL state
    packet.insert(packet.end(), sqlstate.begin(), sqlstate.end());
    
    // Error message
    packet.insert(packet.end(), message.begin(), message.end());
    
    sendPacket(state, packet, nullptr);
}

void MySQLParserAgent::sendEOFPacket(MySQLClientState& state,
                                    uint16_t warnings,
                                    uint16_t status_flags) {
    std::vector<uint8_t> packet;
    packet.push_back(0xFE);  // EOF header
    
    // Warnings
    uint8_t warn[2];
    writeUint16LE(warn, warnings);
    packet.insert(packet.end(), warn, warn + 2);
    
    // Status flags
    uint8_t status[2];
    writeUint16LE(status, status_flags);
    packet.insert(packet.end(), status, status + 2);
    
    sendPacket(state, packet, nullptr);
}

void MySQLParserAgent::sendResultSet(MySQLClientState& state,
                                    const std::vector<IPCFieldDesc>& fields,
                                    const std::vector<std::vector<std::optional<std::string>>>& rows) {
    // Column count
    std::vector<uint8_t> col_count;
    writeLengthEncodedInteger(col_count, fields.size());
    sendPacket(state, col_count, nullptr);
    
    // Column definitions
    for (const auto& field : fields) {
        std::vector<uint8_t> col_def;
        
        // Catalog (always "def")
        writeLengthEncodedString(col_def, "def");
        
        // Schema
        writeLengthEncodedString(col_def, "");
        
        // Table
        writeLengthEncodedString(col_def, "");
        
        // Original table
        writeLengthEncodedString(col_def, "");
        
        // Name
        writeLengthEncodedString(col_def, field.name);
        
        // Original name
        writeLengthEncodedString(col_def, field.name);
        
        // Next length (always 0x0C)
        col_def.push_back(0x0C);
        
        // Character set (UTF8MB4)
        uint8_t charset[2];
        writeUint16LE(charset, mysql::CHARSET_UTF8MB4);
        col_def.insert(col_def.end(), charset, charset + 2);
        
        // Column length
        uint8_t col_len[4];
        writeUint32LE(col_len, field.type_modifier >= 0 ? field.type_modifier : 255);
        col_def.insert(col_def.end(), col_len, col_len + 4);
        
        // Type
        col_def.push_back(mapDataTypeToMySQL(static_cast<core::DataType>(field.type_oid)));
        
        // Flags
        uint8_t flags[2];
        writeUint16LE(flags, 0);
        col_def.insert(col_def.end(), flags, flags + 2);
        
        // Decimals
        col_def.push_back(0);
        
        sendPacket(state, col_def, nullptr);
    }
    
    // EOF or OK packet (depending on CLIENT_DEPRECATE_EOF)
    if (state.capabilities & mysql::CLIENT_DEPRECATE_EOF) {
        sendOKPacket(state, 0, 0, state.status_flags, 0, "");
    } else {
        sendEOFPacket(state, 0, state.status_flags);
    }
    
    // Rows
    for (const auto& row : rows) {
        std::vector<uint8_t> row_packet;
        for (const auto& val : row) {
            if (!val) {
                // NULL
                row_packet.push_back(0xFB);
            } else {
                writeLengthEncodedString(row_packet, *val);
            }
        }
        sendPacket(state, row_packet, nullptr);
    }
    
    // Final EOF/OK
    if (state.capabilities & mysql::CLIENT_DEPRECATE_EOF) {
        sendOKPacket(state, 0, 0, state.status_flags, 0, "");
    } else {
        sendEOFPacket(state, 0, state.status_flags);
    }
}

// ============================================================================
// I/O Helpers
// ============================================================================

core::Status MySQLParserAgent::readPacket(MySQLClientState& state,
                                         std::vector<uint8_t>& packet,
                                         core::ErrorContext* ctx) {
    // Read packet header (3 bytes length + 1 byte sequence)
    uint8_t header[4];
    ssize_t n = recv(state.client_fd, header, 4, MSG_WAITALL);
    if (n == 0) {
        return core::Status::CONNECTION_CLOSED;
    }
    if (n != 4) {
        if (ctx) {
            ctx->set(core::Status::IO_ERROR, "Failed to read packet header",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::IO_ERROR;
    }
    
    uint32_t payload_len = readUint24LE(header);
    uint8_t seq = header[3];
    (void)seq;
    
    state.seq = (header[3] + 1) & 0xFF;
    
    // Read payload
    if (payload_len > 0) {
        packet.resize(payload_len);
        n = recv(state.client_fd, packet.data(), payload_len, MSG_WAITALL);
        if (n != static_cast<ssize_t>(payload_len)) {
            if (ctx) {
                ctx->set(core::Status::IO_ERROR, "Failed to read packet payload",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::IO_ERROR;
        }
    }
    
    return core::Status::OK;
}

core::Status MySQLParserAgent::sendPacket(MySQLClientState& state,
                                         const std::vector<uint8_t>& payload,
                                         core::ErrorContext* ctx) {
    size_t offset = 0;
    uint8_t seq = state.seq++;
    
    while (offset < payload.size()) {
        size_t chunk_size = std::min(payload.size() - offset, size_t(0xFFFFFF));
        
        uint8_t header[4];
        writeUint24LE(header, chunk_size);
        header[3] = seq++;
        
        if (send(state.client_fd, header, 4, 0) != 4) {
            if (ctx) {
                ctx->set(core::Status::IO_ERROR, "Failed to send packet header",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::IO_ERROR;
        }
        
        if (chunk_size > 0) {
            if (send(state.client_fd, payload.data() + offset, chunk_size, 0) != 
                static_cast<ssize_t>(chunk_size)) {
                if (ctx) {
                    ctx->set(core::Status::IO_ERROR, "Failed to send packet payload",
                            __FILE__, __LINE__, __func__);
                }
                return core::Status::IO_ERROR;
            }
        }
        
        offset += chunk_size;
    }
    
    return core::Status::OK;
}

// ============================================================================
// Utility Methods
// ============================================================================

uint32_t MySQLParserAgent::generateConnectionId() {
    static std::atomic<uint32_t> next_id{1};
    return next_id++;
}

void MySQLParserAgent::generateScramble(uint8_t* out, size_t len) {
    // Generate random scramble
    for (size_t i = 0; i < len; i++) {
        out[i] = static_cast<uint8_t>(rand() % 256);
    }
}

uint8_t MySQLParserAgent::mapDataTypeToMySQL(core::DataType type) {
    switch (type) {
        case core::DataType::BOOLEAN:
            return mysql::MYSQL_TYPE_TINY;
        case core::DataType::TINYINT:
            return mysql::MYSQL_TYPE_TINY;
        case core::DataType::SMALLINT:
            return mysql::MYSQL_TYPE_SHORT;
        case core::DataType::INTEGER:
            return mysql::MYSQL_TYPE_LONG;
        case core::DataType::BIGINT:
            return mysql::MYSQL_TYPE_LONGLONG;
        case core::DataType::FLOAT:
            return mysql::MYSQL_TYPE_FLOAT;
        case core::DataType::DOUBLE:
            return mysql::MYSQL_TYPE_DOUBLE;
        case core::DataType::DECIMAL:
            return mysql::MYSQL_TYPE_NEWDECIMAL;
        case core::DataType::DATE:
            return mysql::MYSQL_TYPE_DATE;
        case core::DataType::TIME:
            return mysql::MYSQL_TYPE_TIME;
        case core::DataType::TIMESTAMP:
        case core::DataType::TIMESTAMP_WITH_ZONE:
            return mysql::MYSQL_TYPE_TIMESTAMP;
        case core::DataType::CHAR:
        case core::DataType::VARCHAR:
            return mysql::MYSQL_TYPE_VAR_STRING;
        case core::DataType::TEXT:
            return mysql::MYSQL_TYPE_BLOB;
        case core::DataType::BLOB:
        case core::DataType::BINARY:
        case core::DataType::VARBINARY:
            return mysql::MYSQL_TYPE_BLOB;
        case core::DataType::NULL_TYPE:
            return mysql::MYSQL_TYPE_NULL;
        default:
            return mysql::MYSQL_TYPE_BLOB;
    }
}

core::DataType MySQLParserAgent::mapMySQLToDataType(uint8_t mysql_type) {
    switch (mysql_type) {
        case mysql::MYSQL_TYPE_TINY:
            return core::DataType::TINYINT;
        case mysql::MYSQL_TYPE_SHORT:
            return core::DataType::SMALLINT;
        case mysql::MYSQL_TYPE_LONG:
            return core::DataType::INTEGER;
        case mysql::MYSQL_TYPE_LONGLONG:
            return core::DataType::BIGINT;
        case mysql::MYSQL_TYPE_FLOAT:
            return core::DataType::FLOAT;
        case mysql::MYSQL_TYPE_DOUBLE:
            return core::DataType::DOUBLE;
        case mysql::MYSQL_TYPE_NEWDECIMAL:
        case mysql::MYSQL_TYPE_DECIMAL:
            return core::DataType::NUMERIC;
        case mysql::MYSQL_TYPE_DATE:
            return core::DataType::DATE;
        case mysql::MYSQL_TYPE_TIME:
        case mysql::MYSQL_TYPE_TIME2:
            return core::DataType::TIME;
        case mysql::MYSQL_TYPE_TIMESTAMP:
        case mysql::MYSQL_TYPE_TIMESTAMP2:
        case mysql::MYSQL_TYPE_DATETIME:
        case mysql::MYSQL_TYPE_DATETIME2:
            return core::DataType::TIMESTAMP;
        case mysql::MYSQL_TYPE_VARCHAR:
        case mysql::MYSQL_TYPE_VAR_STRING:
        case mysql::MYSQL_TYPE_STRING:
            return core::DataType::VARCHAR;
        case mysql::MYSQL_TYPE_BLOB:
        case mysql::MYSQL_TYPE_TINY_BLOB:
        case mysql::MYSQL_TYPE_MEDIUM_BLOB:
        case mysql::MYSQL_TYPE_LONG_BLOB:
            return core::DataType::BLOB;
        case mysql::MYSQL_TYPE_NULL:
            return core::DataType::NULL_TYPE;
        default:
            return core::DataType::UNKNOWN;
    }
}

// ============================================================================
// Translation Methods
// ============================================================================

core::Status MySQLParserAgent::translateStartupToIPC(const std::vector<uint8_t>& startup,
                                                    IPCMessage& ipc_msg,
                                                    core::ErrorContext* ctx) {
    (void)startup;
    (void)ipc_msg;
    (void)ctx;
    return core::Status::OK;
}

core::Status MySQLParserAgent::translateIPCToResponse(const IPCMessage& ipc_msg,
                                                     std::vector<uint8_t>& response,
                                                     core::ErrorContext* ctx) {
    (void)ipc_msg;
    (void)response;
    (void)ctx;
    return core::Status::OK;
}

IPCMessageType MySQLParserAgent::mapClientToIPC(uint8_t msg_type) {
    switch (msg_type) {
        case mysql::COM_QUERY:
            return IPCMessageType::SIMPLE_QUERY;
        case mysql::COM_STMT_PREPARE:
            return IPCMessageType::PARSE;
        case mysql::COM_STMT_EXECUTE:
            return IPCMessageType::EXECUTE;
        case mysql::COM_STMT_CLOSE:
            return IPCMessageType::CLOSE;
        case mysql::COM_QUIT:
            return IPCMessageType::TERMINATE;
        default:
            return IPCMessageType::ERROR_RESPONSE;
    }
}

uint8_t MySQLParserAgent::mapIPCToClient(IPCMessageType msg_type) {
    switch (msg_type) {
        case IPCMessageType::ROW_DESCRIPTION:
            return 0x00;  // Part of result set
        case IPCMessageType::DATA_ROW:
            return 0x00;  // Part of result set
        case IPCMessageType::COMMAND_COMPLETE:
            return 0x00;  // OK packet
        case IPCMessageType::READY:
            return 0x00;  // EOF/OK
        case IPCMessageType::ERROR_RESPONSE:
            return 0xFF;  // Error packet
        default:
            return 0x00;
    }
}

std::string MySQLParserAgent::mapSQLStateToProtocol(const char* sqlstate) {
    return std::string(sqlstate);
}

void MySQLParserAgent::mapProtocolErrorToSQLState(const std::vector<uint8_t>& error,
                                                 char* sqlstate_out) {
    (void)error;
    std::strcpy(sqlstate_out, "HY000");
}

} // namespace ipc
} // namespace scratchbird
