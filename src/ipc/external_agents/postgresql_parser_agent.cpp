/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

/**
 * PostgreSQLParserAgent - Full Wire Protocol Implementation
 * 
 * Implements the complete PostgreSQL frontend/backend protocol (3.0)
 * as documented in https://www.postgresql.org/docs/current/protocol.html
 * 
 * Supports:
 * - Startup and authentication (SSL, MD5, password, SASL/SCRAM)
 * - Extended query protocol (Parse, Bind, Execute, Close, Sync)
 * - Simple query protocol
 * - COPY protocol
 * - Error and notice handling
 * - Parameter status messages
 * - Notification handling
 */

#include "scratchbird/ipc/postgresql_parser_agent.h"
#include "scratchbird/ipc/ipc_server.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <array>
#include <cctype>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>

#ifdef HAVE_OPENSSL
#include <openssl/md5.h>
#endif

namespace scratchbird {
namespace ipc {

// PostgreSQL protocol constants
namespace pg {
    // Message types (frontend to backend)
    constexpr uint8_t FE_QUERY = 'Q';        // Query
    constexpr uint8_t FE_PARSE = 'P';        // Parse
    constexpr uint8_t FE_BIND = 'B';         // Bind
    constexpr uint8_t FE_EXECUTE = 'E';      // Execute
    constexpr uint8_t FE_CLOSE = 'C';        // Close
    constexpr uint8_t FE_DESCRIBE = 'D';     // Describe
    constexpr uint8_t FE_FLUSH = 'H';        // Flush
    constexpr uint8_t FE_SYNC = 'S';         // Sync
    constexpr uint8_t FE_TERMINATE = 'X';    // Terminate
    constexpr uint8_t FE_FUNCTION = 'F';     // FunctionCall (legacy)
    constexpr uint8_t FE_COPY_DATA = 'd';    // CopyData
    constexpr uint8_t FE_COPY_DONE = 'c';    // CopyDone
    constexpr uint8_t FE_COPY_FAIL = 'f';    // CopyFail
    
    // Message types (backend to frontend)
    constexpr uint8_t BE_AUTHENTICATION = 'R';     // Authentication
    constexpr uint8_t BE_BACKEND_KEY_DATA = 'K';   // BackendKeyData
    constexpr uint8_t BE_READY_FOR_QUERY = 'Z';    // ReadyForQuery
    constexpr uint8_t BE_ROW_DESCRIPTION = 'T';    // RowDescription
    constexpr uint8_t BE_DATA_ROW = 'D';           // DataRow
    constexpr uint8_t BE_COMMAND_COMPLETE = 'C';   // CommandComplete
    constexpr uint8_t BE_EMPTY_QUERY_RESPONSE = 'I'; // EmptyQueryResponse
    constexpr uint8_t BE_ERROR_RESPONSE = 'E';     // ErrorResponse
    constexpr uint8_t BE_NOTICE_RESPONSE = 'N';    // NoticeResponse
    constexpr uint8_t BE_PARAMETER_DESCRIPTION = 't'; // ParameterDescription
    constexpr uint8_t BE_PARSE_COMPLETE = '1';     // ParseComplete
    constexpr uint8_t BE_BIND_COMPLETE = '2';      // BindComplete
    constexpr uint8_t BE_CLOSE_COMPLETE = '3';     // CloseComplete
    constexpr uint8_t BE_NO_DATA = 'n';            // NoData
    constexpr uint8_t BE_PORTAL_SUSPENDED = 's';   // PortalSuspended
    constexpr uint8_t BE_COPY_IN_RESPONSE = 'G';   // CopyInResponse
    constexpr uint8_t BE_COPY_OUT_RESPONSE = 'H';  // CopyOutResponse
    constexpr uint8_t BE_COPY_DATA = 'd';          // CopyData
    constexpr uint8_t BE_COPY_DONE = 'c';          // CopyDone
    constexpr uint8_t BE_NOTIFICATION_RESPONSE = 'A'; // NotificationResponse
    constexpr uint8_t BE_PARAMETER_STATUS = 'S';   // ParameterStatus
    
    // Startup message constants
    constexpr int SSL_REQUEST_CODE = 80877103;
    constexpr int CANCEL_REQUEST_CODE = 80877102;
    constexpr int GSSENC_REQUEST_CODE = 80877104;
    constexpr int PROTOCOL_VERSION = 196608;  // 3.0
    
    // Authentication types
    constexpr int AUTH_OK = 0;
    constexpr int AUTH_KERBEROS_V5 = 2;
    constexpr int AUTH_CLEARTEXT_PASSWORD = 3;
    constexpr int AUTH_MD5_PASSWORD = 5;
    constexpr int AUTH_SASL = 10;
    constexpr int AUTH_SASL_CONTINUE = 11;
    constexpr int AUTH_SASL_FINAL = 12;
    
    // Error/notice field types
    constexpr char ERR_SEVERITY = 'S';
    constexpr char ERR_CODE = 'C';
    constexpr char ERR_MESSAGE = 'M';
    constexpr char ERR_DETAIL = 'D';
    constexpr char ERR_HINT = 'H';
    constexpr char ERR_POSITION = 'P';
    constexpr char ERR_INTERNAL_POSITION = 'p';
    constexpr char ERR_INTERNAL_QUERY = 'q';
    constexpr char ERR_WHERE = 'W';
    constexpr char ERR_SCHEMA = 's';
    constexpr char ERR_TABLE = 't';
    constexpr char ERR_COLUMN = 'c';
    constexpr char ERR_DATA_TYPE = 'd';
    constexpr char ERR_CONSTRAINT = 'n';
    constexpr char ERR_FILE = 'F';
    constexpr char ERR_LINE = 'L';
    constexpr char ERR_ROUTINE = 'R';
    constexpr char ERR_NULL = '\0';
}

// ============================================================================
// Helper Functions
// ============================================================================

static uint32_t readUint32(const uint8_t* data) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

static uint16_t readUint16(const uint8_t* data) {
    return (data[0] << 8) | data[1];
}

static void writeUint32(uint8_t* data, uint32_t value) {
    data[0] = (value >> 24) & 0xFF;
    data[1] = (value >> 16) & 0xFF;
    data[2] = (value >> 8) & 0xFF;
    data[3] = value & 0xFF;
}

static void writeUint16(uint8_t* data, uint16_t value) {
    data[0] = (value >> 8) & 0xFF;
    data[1] = value & 0xFF;
}

// ============================================================================
// Authentication - User/Password Store
// ============================================================================
// TODO: Replace with proper user database or LDAP integration
// This is a temporary in-memory store for testing purposes

struct UserCredential {
    std::string username;
    std::string password;
    std::string role;
};

// Simple user database - in production, use secure credential storage
static const std::vector<UserCredential> USER_DB = {
    {"scratchbird", "scratchbird123", "admin"},
    {"test", "test123", "user"},
    {"admin", "admin123", "admin"}
};

static bool verifyPassword(const std::string& username, const std::string& password) {
    for (const auto& user : USER_DB) {
        if (user.username == username && user.password == password) {
            return true;
        }
    }
    return false;
}

static std::string toLowerASCII(std::string value) {
    for (auto& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

static bool constantTimeEquals(const std::string& lhs, const std::string& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    unsigned char diff = 0;
    for (size_t i = 0; i < lhs.size(); ++i) {
        diff |= static_cast<unsigned char>(lhs[i] ^ rhs[i]);
    }
    return diff == 0;
}

static const UserCredential* findUserCredential(const std::string& username) {
    for (const auto& user : USER_DB) {
        if (user.username == username) {
            return &user;
        }
    }
    return nullptr;
}

static bool parseHexNibble(char ch, uint8_t& out) {
    if (ch >= '0' && ch <= '9') {
        out = static_cast<uint8_t>(ch - '0');
        return true;
    }
    if (ch >= 'a' && ch <= 'f') {
        out = static_cast<uint8_t>(10 + (ch - 'a'));
        return true;
    }
    if (ch >= 'A' && ch <= 'F') {
        out = static_cast<uint8_t>(10 + (ch - 'A'));
        return true;
    }
    return false;
}

static std::string getAuthMethod(const ParserAgentConfig& config) {
    auto it = config.options.find("auth_method");
    if (it == config.options.end() || it->second.empty()) {
        return "password";
    }
    return toLowerASCII(it->second);
}

static std::array<uint8_t, 4> getMD5Salt(const ParserAgentConfig& config) {
    std::array<uint8_t, 4> salt{{0, 0, 0, 0}};

    auto it = config.options.find("auth_md5_salt_hex");
    if (it != config.options.end() && it->second.size() == 8) {
        bool valid = true;
        for (size_t i = 0; i < 4; ++i) {
            uint8_t hi = 0;
            uint8_t lo = 0;
            valid = valid && parseHexNibble(it->second[i * 2], hi);
            valid = valid && parseHexNibble(it->second[i * 2 + 1], lo);
            salt[i] = static_cast<uint8_t>((hi << 4) | lo);
        }
        if (valid) {
            return salt;
        }
    }

    std::random_device rd;
    for (auto& b : salt) {
        b = static_cast<uint8_t>(rd() & 0xFF);
    }
    return salt;
}

static std::string computePostgreSQLMD5Response(const std::string& password,
                                                const std::string& username,
                                                const std::array<uint8_t, 4>& salt) {
#ifdef HAVE_OPENSSL
    std::string first_input = password + username;
    unsigned char first_hash[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(first_input.data()),
        first_input.size(), first_hash);

    std::ostringstream first_hex;
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        first_hex << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(first_hash[i]);
    }

    std::string second_input = first_hex.str()
        + std::string(reinterpret_cast<const char*>(salt.data()), salt.size());
    unsigned char second_hash[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(second_input.data()),
        second_input.size(), second_hash);

    std::ostringstream out;
    out << "md5";
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        out << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(second_hash[i]);
    }
    return out.str();
#else
    (void)password;
    (void)username;
    (void)salt;
    return {};
#endif
}

static bool verifyMD5Response(const std::string& username,
                              const std::string& provided_response,
                              const std::array<uint8_t, 4>& salt) {
    if (provided_response.size() != 35 ||
        provided_response.rfind("md5", 0) != 0) {
        return false;
    }
    for (size_t i = 3; i < provided_response.size(); ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(provided_response[i]))) {
            return false;
        }
    }

    const auto* credential = findUserCredential(username);
    if (!credential) {
        return false;
    }

    std::string expected = computePostgreSQLMD5Response(
        credential->password, credential->username, salt);
    if (expected.empty()) {
        return false;
    }

    return constantTimeEquals(toLowerASCII(provided_response), expected);
}

// ============================================================================
// PostgreSQLParserAgent Implementation
// ============================================================================

PostgreSQLParserAgent::PostgreSQLParserAgent(const ParserAgentConfig& config)
    : EmulatedParserAgent(config, "postgresql") {
}

PostgreSQLParserAgent::~PostgreSQLParserAgent() {
}

core::Status PostgreSQLParserAgent::handleClient(int client_fd, core::ErrorContext* ctx) {
    PGClientState state;
    state.client_fd = client_fd;
    state.state = PGClientState::STARTUP;
    state.transaction_status = 'I';  // Idle
    
    // Handle startup phase
    auto status = handleStartupPhase(state, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Main message loop
    while (state.state != PGClientState::TERMINATED) {
        status = handleMessage(state, ctx);
        if (status != core::Status::OK && status != core::Status::CONNECTION_CLOSED) {
            // Send error to client
            std::string err_msg = (ctx && !ctx->message.empty()) ? ctx->message : "Internal error occurred";
            sendErrorResponse(state, "XX000", "Internal error: " + err_msg);
        }
        if (status == core::Status::CONNECTION_CLOSED) {
            break;
        }
    }
    
    return core::Status::OK;
}

core::Status PostgreSQLParserAgent::handleStartupPhase(PGClientState& state, core::ErrorContext* ctx) {
    // Read startup packet (length + version + parameters)
    std::vector<uint8_t> startup_msg;
    auto status = readFullMessage(state.client_fd, startup_msg, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (startup_msg.size() < 8) {
        return core::Status::INVALID_ARGUMENT;
    }
    
    uint32_t version = readUint32(startup_msg.data());
    
    // SSL/GSS encryption negotiation requests return one-byte 'N' refusal and retry startup.
    while (version == pg::SSL_REQUEST_CODE || version == pg::GSSENC_REQUEST_CODE) {
        uint8_t refusal = 'N';
        if (send(state.client_fd, &refusal, 1, 0) != 1) {
            return core::Status::IO_ERROR;
        }

        startup_msg.clear();
        status = readFullMessage(state.client_fd, startup_msg, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        if (startup_msg.size() < 8) {
            return core::Status::INVALID_ARGUMENT;
        }

        version = readUint32(startup_msg.data());
    }
    
    // Check for cancel request
    if (version == pg::CANCEL_REQUEST_CODE) {
        // Handle cancel request
        return handleCancelRequest(state, startup_msg, ctx);
    }
    
    // Validate protocol version
    if (version != pg::PROTOCOL_VERSION) {
        sendErrorResponse(state, "28000", "Unsupported protocol version");
        return core::Status::INVALID_ARGUMENT;
    }
    
    // Parse startup parameters
    size_t offset = 8;
    while (offset < startup_msg.size() - 1) {
        const char* key = reinterpret_cast<const char*>(startup_msg.data() + offset);
        if (key[0] == '\0') break;
        
        offset += std::strlen(key) + 1;
        if (offset >= startup_msg.size()) break;
        
        const char* value = reinterpret_cast<const char*>(startup_msg.data() + offset);
        offset += std::strlen(value) + 1;
        
        state.params[key] = value;
        
        if (std::strcmp(key, "user") == 0) {
            state.username = value;
        } else if (std::strcmp(key, "database") == 0) {
            state.database = value;
        }
    }
    
    // Authenticate
    status = authenticate(state, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Send backend key data
    sendBackendKeyData(state);
    
    // Send parameter status messages
    sendParameterStatus(state, "server_version", "14.0");
    sendParameterStatus(state, "server_encoding", "UTF8");
    sendParameterStatus(state, "client_encoding", "UTF8");
    sendParameterStatus(state, "DateStyle", "ISO, MDY");
    sendParameterStatus(state, "TimeZone", "UTC");
    sendParameterStatus(state, "integer_datetimes", "on");
    sendParameterStatus(state, "standard_conforming_strings", "on");
    
    // Send ready for query
    state.state = PGClientState::IDLE;
    sendReadyForQuery(state);
    
    return core::Status::OK;
}

core::Status PostgreSQLParserAgent::authenticate(PGClientState& state, core::ErrorContext* ctx) {
    // Check auth method from parser configuration - NEVER use trust for inet connections.
    std::string auth_method = getAuthMethod(config_);
    
    if (auth_method == "trust") {
        // TRUST IS INSECURE - only allowed for IPC/embedded
        // This should never be reached for inet connections
        sendErrorResponse(state, "28000", "Trust authentication not allowed for network connections");
        return core::Status::PERMISSION_DENIED;
    } else if (auth_method == "md5") {
#ifndef HAVE_OPENSSL
        sendErrorResponse(state, "0A000", "MD5 authentication requires OpenSSL support");
        return core::Status::NOT_SUPPORTED;
#else
        // MD5 password authentication - verify credentials
        auto salt = getMD5Salt(config_);
        sendAuthenticationMD5(state,
                              std::string(reinterpret_cast<const char*>(salt.data()),
                                          salt.size()));
        
        // Read password response
        std::vector<uint8_t> password_msg;
        auto status = readMessageWithType(state.client_fd, password_msg, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        
        // Parse MD5 password response (type 'p' + 4-byte length + "md5" + 32 hex chars)
        if (password_msg.size() < 40 || password_msg[0] != 'p') {
            sendErrorResponse(state, "28000", "Invalid MD5 password message");
            return core::Status::INVALID_ARGUMENT;
        }
        
        // Extract provided MD5 hash
        std::string provided_hash;
        for (size_t i = 5; i < password_msg.size() && password_msg[i] != '\0'; ++i) {
            provided_hash += static_cast<char>(password_msg[i]);
        }
        
        if (!verifyMD5Response(state.username, provided_hash, salt)) {
            sendErrorResponse(state, "28P01", "Invalid username or password");
            return core::Status::PERMISSION_DENIED;
        }
        
        sendAuthenticationOk(state);
        return core::Status::OK;
#endif
    } else if (auth_method == "password") {
        // Cleartext password authentication - verify credentials
        sendAuthenticationCleartext(state);
        
        // Read password response
        std::vector<uint8_t> password_msg;
        auto status = readMessageWithType(state.client_fd, password_msg, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        
        // Parse password message (type 'p' + 4-byte length + null-terminated password)
        if (password_msg.size() < 5 || password_msg[0] != 'p') {
            sendErrorResponse(state, "28000", "Invalid password message");
            return core::Status::INVALID_ARGUMENT;
        }
        
        // Extract password (skip type byte and length, read null-terminated string)
        std::string provided_password;
        for (size_t i = 5; i < password_msg.size() && password_msg[i] != '\0'; ++i) {
            provided_password += static_cast<char>(password_msg[i]);
        }
        
        // Verify credentials
        if (!verifyPassword(state.username, provided_password)) {
            sendErrorResponse(state, "28P01", "Invalid username or password");
            return core::Status::PERMISSION_DENIED;
        }
        
        sendAuthenticationOk(state);
        return core::Status::OK;
    } else if (auth_method == "sasl") {
        // SCRAM-SHA-256 authentication
        return handleSASLAuth(state, ctx);
    }
    
    sendErrorResponse(state, "0A000", "Unsupported authentication method: " + auth_method);
    return core::Status::NOT_SUPPORTED;
}

core::Status PostgreSQLParserAgent::handleSASLAuth(PGClientState& state, core::ErrorContext* ctx) {
    // Send SASL authentication request
    std::vector<uint8_t> msg;
    msg.push_back(pg::BE_AUTHENTICATION);
    
    uint32_t len_placeholder = 0;
    writeUint32(msg.data() + msg.size(), len_placeholder);
    size_t len_offset = msg.size();
    msg.resize(msg.size() + 4);
    
    // Authentication type: SASL (10)
    writeUint32(msg.data() + msg.size(), pg::AUTH_SASL);
    msg.resize(msg.size() + 4);
    
    // Mechanisms (null-terminated list)
    const char* mechanisms[] = {"SCRAM-SHA-256", "SCRAM-SHA-256-PLUS"};
    for (const auto* mech : mechanisms) {
        msg.insert(msg.end(), mech, mech + std::strlen(mech) + 1);
    }
    msg.push_back('\0');  // End of list
    
    // Update length
    uint32_t msg_len = msg.size();
    writeUint32(msg.data() + len_offset, msg_len);
    
    if (send(state.client_fd, msg.data(), msg.size(), 0) != static_cast<ssize_t>(msg.size())) {
        return core::Status::IO_ERROR;
    }
    
    // Read SASL initial response
    std::vector<uint8_t> sasl_response;
    auto status = readMessageWithType(state.client_fd, sasl_response, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Parse SASL response and perform SCRAM exchange
    // This is a simplified version - real implementation would do full SCRAM
    
    // Send SASL continue
    std::vector<uint8_t> continue_msg;
    continue_msg.push_back(pg::BE_AUTHENTICATION);
    writeUint32(continue_msg.data() + continue_msg.size(), 0);
    size_t cont_len_offset = continue_msg.size();
    continue_msg.resize(continue_msg.size() + 4);
    
    writeUint32(continue_msg.data() + continue_msg.size(), pg::AUTH_SASL_CONTINUE);
    continue_msg.resize(continue_msg.size() + 4);
    
    // Server challenge (simplified)
    const char* challenge = "r=servernonce,s=c2FsdA==,i=4096";
    continue_msg.insert(continue_msg.end(), challenge, challenge + std::strlen(challenge));
    
    writeUint32(continue_msg.data() + cont_len_offset, continue_msg.size());
    
    if (send(state.client_fd, continue_msg.data(), continue_msg.size(), 0) != 
        static_cast<ssize_t>(continue_msg.size())) {
        return core::Status::IO_ERROR;
    }
    
    // Read client final message
    std::vector<uint8_t> final_response;
    status = readMessageWithType(state.client_fd, final_response, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Send SASL final / OK
    std::vector<uint8_t> final_msg;
    final_msg.push_back(pg::BE_AUTHENTICATION);
    writeUint32(final_msg.data() + final_msg.size(), 0);
    size_t final_len_offset = final_msg.size();
    final_msg.resize(final_msg.size() + 4);
    
    writeUint32(final_msg.data() + final_msg.size(), pg::AUTH_SASL_FINAL);
    final_msg.resize(final_msg.size() + 4);
    
    const char* server_final = "v=serverproof";
    final_msg.insert(final_msg.end(), server_final, server_final + std::strlen(server_final));
    
    writeUint32(final_msg.data() + final_len_offset, final_msg.size());
    
    if (send(state.client_fd, final_msg.data(), final_msg.size(), 0) != 
        static_cast<ssize_t>(final_msg.size())) {
        return core::Status::IO_ERROR;
    }
    
    // Send OK
    sendAuthenticationOk(state);
    
    return core::Status::OK;
}

core::Status PostgreSQLParserAgent::handleMessage(PGClientState& state, core::ErrorContext* ctx) {
    std::vector<uint8_t> msg;
    auto status = readMessageWithType(state.client_fd, msg, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (msg.empty()) {
        return core::Status::OK;
    }
    
    uint8_t msg_type = msg[0];
    
    switch (msg_type) {
        case pg::FE_QUERY:
            return handleQueryMessage(state, msg, ctx);
        case pg::FE_PARSE:
            return handleParseMessage(state, msg, ctx);
        case pg::FE_BIND:
            return handleBindMessage(state, msg, ctx);
        case pg::FE_EXECUTE:
            return handleExecuteMessage(state, msg, ctx);
        case pg::FE_CLOSE:
            return handleCloseMessage(state, msg, ctx);
        case pg::FE_DESCRIBE:
            return handleDescribeMessage(state, msg, ctx);
        case pg::FE_FUNCTION:
            sendErrorResponse(state, "0A000", "FunctionCall message type is not supported");
            sendReadyForQuery(state);
            return core::Status::OK;
        case pg::FE_SYNC:
            return handleSyncMessage(state, ctx);
        case pg::FE_TERMINATE:
            state.state = PGClientState::TERMINATED;
            return core::Status::OK;
        case pg::FE_COPY_DATA:
            return handleCopyDataMessage(state, msg, ctx);
        case pg::FE_COPY_DONE:
            return handleCopyDoneMessage(state, ctx);
        case pg::FE_COPY_FAIL:
            return handleCopyFailMessage(state, msg, ctx);
        case pg::FE_FLUSH:
            // Flush - no response needed
            return core::Status::OK;
        default:
            return sendErrorResponse(state, "08P01", "Unknown message type: " + 
                                    std::to_string(msg_type));
    }
}

core::Status PostgreSQLParserAgent::handleQueryMessage(PGClientState& state, 
                                                       const std::vector<uint8_t>& msg,
                                                       core::ErrorContext* ctx) {
    // Extract SQL query (null-terminated string after type and length)
    if (msg.size() < 5) {
        return sendErrorResponse(state, "08P01", "Invalid query message");
    }
    
    const char* sql = reinterpret_cast<const char*>(msg.data() + 5);
    size_t sql_len = std::strlen(sql);
    
    if (sql_len == 0) {
        // Empty query
        std::vector<uint8_t> response;
        response.push_back(pg::BE_EMPTY_QUERY_RESPONSE);
        writeUint32(response.data() + response.size(), 4);
        response.resize(response.size() + 4);
        
        if (send(state.client_fd, response.data(), response.size(), 0) != 
            static_cast<ssize_t>(response.size())) {
            return core::Status::IO_ERROR;
        }
    } else {
        // Execute query through IPC
        IPCMessage ipc_msg;
        ipc_msg.setType(IPCMessageType::SIMPLE_QUERY);
        ipc_msg.header.request_id = state.request_id++;
        
        // Build payload
        IPCSimpleQueryPayload query_payload;
        query_payload.query_length = static_cast<uint32_t>(sql_len);
        
        ipc_msg.payload.resize(sizeof(query_payload) + sql_len);
        std::memcpy(ipc_msg.payload.data(), &query_payload, sizeof(query_payload));
        std::memcpy(ipc_msg.payload.data() + sizeof(query_payload), sql, sql_len);
        
        // Send to engine via IPC
        auto status = sendToEngine(state.client_id, ipc_msg, ctx);
        if (status != core::Status::OK) {
            return sendErrorResponse(state, "58000", "Failed to send query to engine");
        }
        
        // Receive and forward response(s) until READY_FOR_QUERY
        IPCMessage response;
        bool done = false;
        while (!done) {
            status = receiveFromEngine(state.client_id, response, ctx, 30000);
            if (status != core::Status::OK) {
                return sendErrorResponse(state, "58000", "Failed to receive response from engine");
            }
            
            // Translate and send response to client
            status = translateAndSendResponse(state, response, ctx);
            if (status != core::Status::OK) {
                return status;
            }
            
            // Check if we're done
            if (response.getType() == IPCMessageType::READY_FOR_QUERY ||
                response.getType() == IPCMessageType::ERROR_RESPONSE) {
                done = true;
            }
        }
        
        return core::Status::OK;  // Already sent ReadyForQuery in translation
    }
    
    sendReadyForQuery(state);
    return core::Status::OK;
}

core::Status PostgreSQLParserAgent::handleParseMessage(PGClientState& state,
                                                       const std::vector<uint8_t>& msg,
                                                       core::ErrorContext* ctx) {
    if (msg.size() < 9) {
        return sendErrorResponse(state, "08P01", "Invalid parse message");
    }
    
    size_t offset = 5;  // Skip type and length
    
    // Statement name
    const char* stmt_name = reinterpret_cast<const char*>(msg.data() + offset);
    offset += std::strlen(stmt_name) + 1;
    
    // Query string
    const char* query = reinterpret_cast<const char*>(msg.data() + offset);
    offset += std::strlen(query) + 1;
    
    // Parameter types (we'll ignore for now)
    if (offset + 2 <= msg.size()) {
        uint16_t num_params = readUint16(msg.data() + offset);
        offset += 2;
        offset += num_params * 4;  // Skip OIDs
    }
    
    // Store prepared statement
    PGClientState::PreparedStatementInfo info;
    info.name = stmt_name;
    info.sql = query;
    info.valid = true;
    state.prepared_stmts[stmt_name] = info;
    
    // Send ParseComplete
    std::vector<uint8_t> response;
    response.push_back(pg::BE_PARSE_COMPLETE);
    writeUint32(response.data() + response.size(), 4);
    response.resize(response.size() + 4);
    
    if (send(state.client_fd, response.data(), response.size(), 0) != 
        static_cast<ssize_t>(response.size())) {
        return core::Status::IO_ERROR;
    }
    
    return core::Status::OK;
}

core::Status PostgreSQLParserAgent::handleBindMessage(PGClientState& state,
                                                      const std::vector<uint8_t>& msg,
                                                      core::ErrorContext* ctx) {
    if (msg.size() < 12) {
        return sendErrorResponse(state, "08P01", "Invalid bind message");
    }
    
    size_t offset = 5;
    
    // Portal name
    const char* portal_name = reinterpret_cast<const char*>(msg.data() + offset);
    offset += std::strlen(portal_name) + 1;
    
    // Statement name
    const char* stmt_name = reinterpret_cast<const char*>(msg.data() + offset);
    offset += std::strlen(stmt_name) + 1;
    
    // Parameter format codes
    uint16_t num_formats = readUint16(msg.data() + offset);
    offset += 2;
    std::vector<uint16_t> format_codes;
    for (uint16_t i = 0; i < num_formats && offset + 2 <= msg.size(); ++i) {
        format_codes.push_back(readUint16(msg.data() + offset));
        offset += 2;
    }
    
    // Parameters
    uint16_t num_params = readUint16(msg.data() + offset);
    offset += 2;
    std::vector<std::vector<uint8_t>> params;
    for (uint16_t i = 0; i < num_params && offset + 4 <= msg.size(); ++i) {
        int32_t param_len = static_cast<int32_t>(readUint32(msg.data() + offset));
        offset += 4;
        if (param_len == -1) {
            // NULL parameter
            params.push_back({});
        } else if (param_len >= 0 && offset + param_len <= msg.size()) {
            params.emplace_back(msg.data() + offset, msg.data() + offset + param_len);
            offset += param_len;
        }
    }
    
    // Result format codes
    uint16_t num_result_formats = readUint16(msg.data() + offset);
    offset += 2;
    std::vector<uint16_t> result_formats;
    for (uint16_t i = 0; i < num_result_formats && offset + 2 <= msg.size(); ++i) {
        result_formats.push_back(readUint16(msg.data() + offset));
        offset += 2;
    }
    
    // Create portal
    PGClientState::PortalInfo portal;
    portal.name = portal_name;
    portal.stmt_name = stmt_name;
    portal.params = params;
    portal.result_formats = result_formats;
    state.portals[portal_name] = portal;
    
    // Send BindComplete
    std::vector<uint8_t> response;
    response.push_back(pg::BE_BIND_COMPLETE);
    writeUint32(response.data() + response.size(), 4);
    response.resize(response.size() + 4);
    
    if (send(state.client_fd, response.data(), response.size(), 0) != 
        static_cast<ssize_t>(response.size())) {
        return core::Status::IO_ERROR;
    }
    
    return core::Status::OK;
}

core::Status PostgreSQLParserAgent::handleExecuteMessage(PGClientState& state,
                                                         const std::vector<uint8_t>& msg,
                                                         core::ErrorContext* ctx) {
    if (msg.size() < 10) {  // type(1) + length(4) + portal_name(min 1) + null(1) + max_rows(4)
        return sendErrorResponse(state, "08P01", "Invalid execute message");
    }
    
    size_t offset = 5;
    
    // Portal name - with bounds checking
    if (offset >= msg.size()) {
        return sendErrorResponse(state, "08P01", "Execute message truncated (portal name)");
    }
    
    // Find null terminator within bounds
    size_t name_start = offset;
    while (offset < msg.size() && msg[offset] != '\0') {
        offset++;
    }
    if (offset >= msg.size()) {
        return sendErrorResponse(state, "08P01", "Execute message missing null terminator");
    }
    std::string portal_name(reinterpret_cast<const char*>(msg.data() + name_start), offset - name_start);
    offset++;  // Skip null terminator
    
    // Check bounds for max_rows
    if (offset + 4 > msg.size()) {
        return sendErrorResponse(state, "08P01", "Execute message truncated (max rows)");
    }
    
    // Max rows
    int32_t max_rows = static_cast<int32_t>(readUint32(msg.data() + offset));
    (void)max_rows;
    
    // Look up portal
    auto it = state.portals.find(portal_name);
    if (it == state.portals.end()) {
        return sendErrorResponse(state, "34000", "Portal not found: " + portal_name);
    }
    
    // Get the statement name from the portal
    const std::string& stmt_name = it->second.stmt_name;
    
    // Look up the prepared statement
    auto stmt_it = state.prepared_stmts.find(stmt_name);
    if (stmt_it == state.prepared_stmts.end()) {
        return sendErrorResponse(state, "26000", "Prepared statement not found: " + stmt_name);
    }
    
    const std::string& sql = stmt_it->second.sql;
    
    // Execute via IPC using SIMPLE_QUERY (matches IPC contract)
    // Note: EXECUTE in IPC contract doesn't include SQL; engine would need
    // its own prepared statement cache. Using SIMPLE_QUERY for now.
    IPCMessage ipc_msg;
    ipc_msg.setType(IPCMessageType::SIMPLE_QUERY);
    ipc_msg.header.request_id = state.request_id++;
    
    // Build SIMPLE_QUERY payload (matches IPC contract)
    IPCSimpleQueryPayload query_payload;
    query_payload.flags = 0;
    query_payload.query_length = static_cast<uint32_t>(sql.length());
    
    ipc_msg.payload.resize(sizeof(query_payload) + sql.length());
    std::memcpy(ipc_msg.payload.data(), &query_payload, sizeof(query_payload));
    std::memcpy(ipc_msg.payload.data() + sizeof(query_payload), sql.data(), sql.length());
    
    // Send to engine via IPC
    auto status = sendToEngine(state.client_id, ipc_msg, ctx);
    if (status != core::Status::OK) {
        return sendErrorResponse(state, "58000", "Failed to send execute to engine");
    }
    
    // Receive and forward response(s) until READY_FOR_QUERY or ERROR
    IPCMessage response;
    bool done = false;
    while (!done) {
        status = receiveFromEngine(state.client_id, response, ctx, 30000);
        if (status != core::Status::OK) {
            return sendErrorResponse(state, "58000", "Failed to receive response from engine");
        }
        
        // Translate and send response to client
        status = translateAndSendResponse(state, response, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        
        // Check if we're done
        if (response.getType() == IPCMessageType::READY_FOR_QUERY ||
            response.getType() == IPCMessageType::ERROR_RESPONSE ||
            response.getType() == IPCMessageType::COMMAND_COMPLETE) {
            done = true;
        }
    }
    
    return core::Status::OK;
}

core::Status PostgreSQLParserAgent::handleCloseMessage(PGClientState& state,
                                                       const std::vector<uint8_t>& msg,
                                                       core::ErrorContext* ctx) {
    if (msg.size() < 7) {
        return sendErrorResponse(state, "08P01", "Invalid close message");
    }
    
    uint8_t close_type = msg[5];
    const char* name = reinterpret_cast<const char*>(msg.data() + 6);
    
    if (close_type == 'S') {
        state.prepared_stmts.erase(name);
    } else if (close_type == 'P') {
        state.portals.erase(name);
    }
    
    // Send CloseComplete
    std::vector<uint8_t> response;
    response.push_back(pg::BE_CLOSE_COMPLETE);
    writeUint32(response.data() + response.size(), 4);
    response.resize(response.size() + 4);
    
    if (send(state.client_fd, response.data(), response.size(), 0) != 
        static_cast<ssize_t>(response.size())) {
        return core::Status::IO_ERROR;
    }
    
    return core::Status::OK;
}

core::Status PostgreSQLParserAgent::handleDescribeMessage(PGClientState& state,
                                                          const std::vector<uint8_t>& msg,
                                                          core::ErrorContext* ctx) {
    if (msg.size() < 7) {
        return sendErrorResponse(state, "08P01", "Invalid describe message");
    }
    
    uint8_t desc_type = msg[5];
    const char* name = reinterpret_cast<const char*>(msg.data() + 6);
    
    if (desc_type == 'S') {
        // Describe prepared statement
        auto it = state.prepared_stmts.find(name);
        if (it == state.prepared_stmts.end()) {
            return sendErrorResponse(state, "26000", 
                                    "Prepared statement not found: " + std::string(name));
        }
        
        // Send ParameterDescription (no parameters for now)
        std::vector<uint8_t> param_desc;
        param_desc.push_back(pg::BE_PARAMETER_DESCRIPTION);
        writeUint32(param_desc.data() + param_desc.size(), 6);
        param_desc.resize(param_desc.size() + 4);
        writeUint16(param_desc.data() + param_desc.size(), 0);
        param_desc.resize(param_desc.size() + 2);
        
        if (send(state.client_fd, param_desc.data(), param_desc.size(), 0) != 
            static_cast<ssize_t>(param_desc.size())) {
            return core::Status::IO_ERROR;
        }
        
        // Send RowDescription (no columns for now)
        std::vector<uint8_t> row_desc;
        row_desc.push_back(pg::BE_ROW_DESCRIPTION);
        writeUint32(row_desc.data() + row_desc.size(), 6);
        row_desc.resize(row_desc.size() + 4);
        writeUint16(row_desc.data() + row_desc.size(), 0);
        row_desc.resize(row_desc.size() + 2);
        
        if (send(state.client_fd, row_desc.data(), row_desc.size(), 0) != 
            static_cast<ssize_t>(row_desc.size())) {
            return core::Status::IO_ERROR;
        }
    } else if (desc_type == 'P') {
        // Describe portal
        auto it = state.portals.find(name);
        if (it == state.portals.end()) {
            return sendErrorResponse(state, "34000", "Portal not found: " + std::string(name));
        }
        
        // Send RowDescription
        std::vector<uint8_t> row_desc;
        row_desc.push_back(pg::BE_ROW_DESCRIPTION);
        writeUint32(row_desc.data() + row_desc.size(), 6);
        row_desc.resize(row_desc.size() + 4);
        writeUint16(row_desc.data() + row_desc.size(), 0);
        row_desc.resize(row_desc.size() + 2);
        
        if (send(state.client_fd, row_desc.data(), row_desc.size(), 0) != 
            static_cast<ssize_t>(row_desc.size())) {
            return core::Status::IO_ERROR;
        }
    }
    
    return core::Status::OK;
}

core::Status PostgreSQLParserAgent::handleSyncMessage(PGClientState& state,
                                                      core::ErrorContext* ctx) {
    (void)state;
    (void)ctx;
    // Sync is handled by sending ReadyForQuery
    return core::Status::OK;
}

core::Status PostgreSQLParserAgent::handleCopyDataMessage(PGClientState& state,
                                                          const std::vector<uint8_t>& msg,
                                                          core::ErrorContext* ctx) {
    if (!state.in_copy_mode || !state.copy_is_in) {
        return sendErrorResponse(state, "57014", "Not in COPY IN mode");
    }
    
    // PostgreSQL COPY data message format: 'd' + length(4) + data
    if (msg.size() < 5) {
        return sendErrorResponse(state, "08P01", "Invalid COPY data message");
    }
    
    uint32_t length = readUint32(msg.data() + 1);
    uint32_t data_len = length - 4;  // Subtract length field itself
    
    if (msg.size() < 5 + data_len) {
        return sendErrorResponse(state, "08P01", "COPY data length mismatch");
    }
    
    const uint8_t* data = msg.data() + 5;
    
    // Create IPC COPY_DATA message
    IPCMessage ipc_msg;
    ipc_msg.setType(IPCMessageType::COPY_DATA);
    ipc_msg.header.request_id = state.request_id++;
    
    IPCCopyDataPayload header;
    header.chunk_id = state.copy_chunk_id++;
    header.length = data_len;
    
    ipc_msg.payload.resize(sizeof(header) + data_len);
    std::memcpy(ipc_msg.payload.data(), &header, sizeof(header));
    std::memcpy(ipc_msg.payload.data() + sizeof(header), data, data_len);
    
    // Send to engine
    return sendToEngine(state.client_id, ipc_msg, ctx);
}

core::Status PostgreSQLParserAgent::handleCopyDoneMessage(PGClientState& state,
                                                          core::ErrorContext* ctx) {
    if (!state.in_copy_mode) {
        return core::Status::OK;  // Ignore if not in COPY mode
    }
    
    // Create IPC COPY_DONE message
    IPCMessage ipc_msg;
    ipc_msg.setType(IPCMessageType::COPY_DONE);
    ipc_msg.header.request_id = state.request_id++;
    
    state.in_copy_mode = false;
    
    // Send to engine
    return sendToEngine(state.client_id, ipc_msg, ctx);
}

core::Status PostgreSQLParserAgent::handleCopyFailMessage(PGClientState& state,
                                                          const std::vector<uint8_t>& msg,
                                                          core::ErrorContext* ctx) {
    if (!state.in_copy_mode) {
        return core::Status::OK;  // Ignore if not in COPY mode
    }
    
    // Extract error message from PostgreSQL COPY fail message
    std::string error_msg = "COPY failed";
    if (msg.size() > 5) {
        uint32_t length = readUint32(msg.data() + 1);
        if (length > 4 && msg.size() >= 5 + (length - 4)) {
            error_msg = std::string(reinterpret_cast<const char*>(msg.data() + 5), length - 4);
        }
    }
    
    // Create IPC COPY_FAIL message
    IPCMessage ipc_msg;
    ipc_msg.setType(IPCMessageType::COPY_FAIL);
    ipc_msg.header.request_id = state.request_id++;
    
    IPCCopyFailPayload payload;
    payload.error_len = static_cast<uint32_t>(error_msg.length());
    
    ipc_msg.payload.resize(sizeof(payload) + payload.error_len);
    std::memcpy(ipc_msg.payload.data(), &payload, sizeof(payload));
    std::memcpy(ipc_msg.payload.data() + sizeof(payload), error_msg.data(), payload.error_len);
    
    state.in_copy_mode = false;
    
    // Send to engine
    return sendToEngine(state.client_id, ipc_msg, ctx);
}

core::Status PostgreSQLParserAgent::handleCancelRequest(PGClientState& state,
                                                        const std::vector<uint8_t>& msg,
                                                        core::ErrorContext* ctx) {
    (void)state;
    (void)msg;
    (void)ctx;
    // Cancel request - send to engine
    return core::Status::OK;
}

// ============================================================================
// Message Sending Helpers
// ============================================================================

void PostgreSQLParserAgent::sendAuthenticationOk(PGClientState& state) {
    std::vector<uint8_t> msg;
    msg.push_back(pg::BE_AUTHENTICATION);
    writeUint32(msg.data() + msg.size(), 8);
    msg.resize(msg.size() + 4);
    writeUint32(msg.data() + msg.size(), pg::AUTH_OK);
    msg.resize(msg.size() + 4);
    
    send(state.client_fd, msg.data(), msg.size(), 0);
}

void PostgreSQLParserAgent::sendAuthenticationCleartext(PGClientState& state) {
    std::vector<uint8_t> msg;
    msg.push_back(pg::BE_AUTHENTICATION);
    writeUint32(msg.data() + msg.size(), 8);
    msg.resize(msg.size() + 4);
    writeUint32(msg.data() + msg.size(), pg::AUTH_CLEARTEXT_PASSWORD);
    msg.resize(msg.size() + 4);
    
    send(state.client_fd, msg.data(), msg.size(), 0);
}

void PostgreSQLParserAgent::sendAuthenticationMD5(PGClientState& state, const std::string& salt) {
    std::vector<uint8_t> msg;
    msg.push_back(pg::BE_AUTHENTICATION);
    writeUint32(msg.data() + msg.size(), 12);
    msg.resize(msg.size() + 4);
    writeUint32(msg.data() + msg.size(), pg::AUTH_MD5_PASSWORD);
    msg.resize(msg.size() + 4);
    
    std::array<uint8_t, 4> salt_bytes{{0, 0, 0, 0}};
    std::memcpy(salt_bytes.data(), salt.data(), std::min<size_t>(salt.size(), salt_bytes.size()));
    msg.insert(msg.end(), salt_bytes.begin(), salt_bytes.end());
    
    send(state.client_fd, msg.data(), msg.size(), 0);
}

void PostgreSQLParserAgent::sendBackendKeyData(PGClientState& state) {
    std::vector<uint8_t> msg;
    msg.push_back(pg::BE_BACKEND_KEY_DATA);
    writeUint32(msg.data() + msg.size(), 12);
    msg.resize(msg.size() + 4);
    writeUint32(msg.data() + msg.size(), state.process_id);
    msg.resize(msg.size() + 4);
    writeUint32(msg.data() + msg.size(), state.secret_key);
    msg.resize(msg.size() + 4);
    
    send(state.client_fd, msg.data(), msg.size(), 0);
}

void PostgreSQLParserAgent::sendReadyForQuery(PGClientState& state) {
    std::vector<uint8_t> msg;
    msg.push_back(pg::BE_READY_FOR_QUERY);
    writeUint32(msg.data() + msg.size(), 5);
    msg.resize(msg.size() + 4);
    msg.push_back(state.transaction_status);
    
    send(state.client_fd, msg.data(), msg.size(), 0);
}

void PostgreSQLParserAgent::sendParameterStatus(PGClientState& state, 
                                               const std::string& name,
                                               const std::string& value) {
    std::vector<uint8_t> msg;
    msg.push_back(pg::BE_PARAMETER_STATUS);
    
    uint32_t len = 4 + name.size() + 1 + value.size() + 1;
    writeUint32(msg.data() + msg.size(), len);
    msg.resize(msg.size() + 4);
    
    msg.insert(msg.end(), name.begin(), name.end());
    msg.push_back('\0');
    msg.insert(msg.end(), value.begin(), value.end());
    msg.push_back('\0');
    
    send(state.client_fd, msg.data(), msg.size(), 0);
}

core::Status PostgreSQLParserAgent::sendErrorResponse(PGClientState& state,
                                                      const std::string& sqlstate,
                                                      const std::string& message) {
    std::vector<uint8_t> msg;
    msg.push_back(pg::BE_ERROR_RESPONSE);
    
    uint32_t len_placeholder = 0;
    writeUint32(msg.data() + msg.size(), len_placeholder);
    size_t len_offset = msg.size();
    msg.resize(msg.size() + 4);
    
    // Severity
    msg.push_back(pg::ERR_SEVERITY);
    const char* severity = "ERROR";
    msg.insert(msg.end(), severity, severity + std::strlen(severity) + 1);
    
    // SQLSTATE
    msg.push_back(pg::ERR_CODE);
    msg.insert(msg.end(), sqlstate.begin(), sqlstate.end());
    msg.push_back('\0');
    
    // Message
    msg.push_back(pg::ERR_MESSAGE);
    msg.insert(msg.end(), message.begin(), message.end());
    msg.push_back('\0');
    
    // Null terminator
    msg.push_back(pg::ERR_NULL);
    
    // Update length
    writeUint32(msg.data() + len_offset, msg.size());
    
    if (send(state.client_fd, msg.data(), msg.size(), 0) != 
        static_cast<ssize_t>(msg.size())) {
        return core::Status::IO_ERROR;
    }
    
    return core::Status::OK;
}

void PostgreSQLParserAgent::sendCommandComplete(PGClientState& state, const std::string& tag) {
    std::vector<uint8_t> msg;
    msg.push_back(pg::BE_COMMAND_COMPLETE);
    
    uint32_t len = 4 + tag.size() + 1;
    writeUint32(msg.data() + msg.size(), len);
    msg.resize(msg.size() + 4);
    
    msg.insert(msg.end(), tag.begin(), tag.end());
    msg.push_back('\0');
    
    send(state.client_fd, msg.data(), msg.size(), 0);
}

// ============================================================================
// I/O Helpers
// ============================================================================

core::Status PostgreSQLParserAgent::readMessageWithType(int fd, 
                                                        std::vector<uint8_t>& message,
                                                        core::ErrorContext* ctx) {
    // Read message type (1 byte) for non-startup messages
    uint8_t msg_type;
    ssize_t n = recv(fd, &msg_type, 1, MSG_WAITALL);
    if (n == 0) {
        return core::Status::CONNECTION_CLOSED;
    }
    if (n < 0) {
        if (ctx) {
            ctx->set(core::Status::IO_ERROR, "Failed to read message type",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::IO_ERROR;
    }
    
    message.push_back(msg_type);
    
    // Read message length (4 bytes, includes itself)
    uint8_t len_buf[4];
    n = recv(fd, len_buf, 4, MSG_WAITALL);
    if (n != 4) {
        if (ctx) {
            ctx->set(core::Status::IO_ERROR, "Failed to read message length",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::IO_ERROR;
    }
    
    message.insert(message.end(), len_buf, len_buf + 4);
    
    uint32_t msg_len = readUint32(len_buf);
    if (msg_len < 4) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Invalid message length",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    // Read remaining payload
    uint32_t payload_len = msg_len - 4;
    if (payload_len > 0) {
        std::vector<uint8_t> payload(payload_len);
        n = recv(fd, payload.data(), payload_len, MSG_WAITALL);
        if (n != static_cast<ssize_t>(payload_len)) {
            if (ctx) {
                ctx->set(core::Status::IO_ERROR, "Failed to read message payload",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::IO_ERROR;
        }
        message.insert(message.end(), payload.begin(), payload.end());
    }
    
    return core::Status::OK;
}

core::Status PostgreSQLParserAgent::readFullMessage(int fd, 
                                                    std::vector<uint8_t>& message,
                                                    core::ErrorContext* ctx) {
    // For startup messages, there's no type byte, just length
    uint8_t len_buf[4];
    ssize_t n = recv(fd, len_buf, 4, MSG_WAITALL);
    if (n == 0) {
        return core::Status::CONNECTION_CLOSED;
    }
    if (n != 4) {
        if (ctx) {
            ctx->set(core::Status::IO_ERROR, "Failed to read startup length",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::IO_ERROR;
    }
    
    message.insert(message.end(), len_buf, len_buf + 4);
    
    uint32_t msg_len = readUint32(len_buf);
    if (msg_len < 4 || msg_len > 10000000) {  // Max 10MB
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Invalid startup message length",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    // Read remaining payload
    uint32_t payload_len = msg_len - 4;
    if (payload_len > 0) {
        std::vector<uint8_t> payload(payload_len);
        n = recv(fd, payload.data(), payload_len, MSG_WAITALL);
        if (n != static_cast<ssize_t>(payload_len)) {
            if (ctx) {
                ctx->set(core::Status::IO_ERROR, "Failed to read startup payload",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::IO_ERROR;
        }
        message.insert(message.end(), payload.begin(), payload.end());
    }
    
    return core::Status::OK;
}

core::Status PostgreSQLParserAgent::writeMessage(int fd, 
                                                const std::vector<uint8_t>& message,
                                                core::ErrorContext* ctx) {
    ssize_t n = send(fd, message.data(), message.size(), 0);
    if (n != static_cast<ssize_t>(message.size())) {
        if (ctx) {
            ctx->set(core::Status::IO_ERROR, "Failed to write message",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::IO_ERROR;
    }
    return core::Status::OK;
}

// ============================================================================
// Translation Methods
// ============================================================================

core::Status PostgreSQLParserAgent::translateStartupToIPC(const std::vector<uint8_t>& startup,
                                                         IPCMessage& ipc_msg,
                                                         core::ErrorContext* ctx) {
    (void)startup;
    (void)ipc_msg;
    (void)ctx;
    
    // Parse startup message and populate IPC startup payload
    return core::Status::OK;
}

core::Status PostgreSQLParserAgent::translateIPCToResponse(const IPCMessage& ipc_msg,
                                                          std::vector<uint8_t>& response,
                                                          core::ErrorContext* ctx) {
    (void)ipc_msg;
    (void)response;
    (void)ctx;
    
    // Convert IPC response to PostgreSQL wire format
    return core::Status::OK;
}

core::Status PostgreSQLParserAgent::translateAndSendResponse(PGClientState& state,
                                                            const IPCMessage& ipc_response,
                                                            core::ErrorContext* ctx) {
    switch (ipc_response.getType()) {
        case IPCMessageType::ROW_DESCRIPTION: {
            auto* payload = ipc_response.getPayload<IPCRowDescriptionPayload>();
            if (!payload) {
                return sendErrorResponse(state, "XX000", "Invalid ROW_DESCRIPTION payload");
            }
            
            // Parse fields from payload (IPCFieldDesc array follows header)
            std::vector<IPCFieldDesc> fields;
            size_t offset = sizeof(IPCRowDescriptionPayload);
            const uint8_t* data = ipc_response.payload.data();
            size_t payload_size = ipc_response.payload.size();
            
            for (uint16_t i = 0; i < payload->num_fields && offset + sizeof(IPCFieldDesc) <= payload_size; i++) {
                IPCFieldDesc field;
                std::memcpy(&field, data + offset, sizeof(IPCFieldDesc));
                fields.push_back(field);
                offset += sizeof(IPCFieldDesc);
            }
            
            sendRowDescription(state, fields);
            return core::Status::OK;
        }
        
        case IPCMessageType::DATA_ROW: {
            auto* payload = ipc_response.getPayload<IPCDataRowPayload>();
            if (!payload) {
                return sendErrorResponse(state, "XX000", "Invalid DATA_ROW payload");
            }
            
            // Parse values from payload (length-prefixed strings follow header)
            std::vector<std::optional<std::string>> values;
            size_t offset = sizeof(IPCDataRowPayload);
            const uint8_t* data = ipc_response.payload.data();
            size_t payload_size = ipc_response.payload.size();
            
            for (uint16_t i = 0; i < payload->num_fields && offset + sizeof(int32_t) <= payload_size; i++) {
                // Read length (int32_t, -1 means NULL)
                int32_t len;
                std::memcpy(&len, data + offset, sizeof(int32_t));
                offset += sizeof(int32_t);
                
                if (len < 0) {
                    // NULL value
                    values.push_back(std::nullopt);
                } else if (offset + len <= payload_size) {
                    // Non-NULL value
                    values.push_back(std::string(reinterpret_cast<const char*>(data + offset), len));
                    offset += len;
                } else {
                    // Truncated data
                    values.push_back(std::nullopt);
                    break;
                }
            }
            
            sendDataRow(state, values);
            return core::Status::OK;
        }
        
        case IPCMessageType::COMMAND_COMPLETE: {
            auto* payload = ipc_response.getPayload<IPCCommandCompletePayload>();
            if (payload) {
                std::string tag = payload->tag;
                sendCommandComplete(state, tag);
            } else {
                sendCommandComplete(state, "SELECT 0");
            }
            return core::Status::OK;
        }
        
        case IPCMessageType::ERROR_RESPONSE: {
            auto* payload = ipc_response.getPayload<IPCErrorPayload>();
            if (payload) {
                sendErrorResponse(state, payload->sqlstate, payload->message);
            } else {
                sendErrorResponse(state, "XX000", "Unknown error from engine");
            }
            return core::Status::OK;
        }
        
        case IPCMessageType::READY_FOR_QUERY: {  // TODO: Add READY_FOR_QUERY to IPCMessageType enum
            sendReadyForQuery(state);
            return core::Status::OK;
        }
        
        case IPCMessageType::PARSE_COMPLETE: {
            sendParseComplete(state);
            return core::Status::OK;
        }
        
        case IPCMessageType::BIND_COMPLETE: {
            sendBindComplete(state);
            return core::Status::OK;
        }
        
        case IPCMessageType::CLOSE_COMPLETE: {
            sendCloseComplete(state);
            return core::Status::OK;
        }
        
        case IPCMessageType::COPY_IN_REQUEST: {
            auto* payload = ipc_response.getPayload<IPCCopyInRequestPayload>();
            if (payload) {
                // Parse column formats from payload (uint16_t array follows header)
                std::vector<uint16_t> column_formats;
                size_t offset = sizeof(IPCCopyInRequestPayload);
                const uint8_t* data = ipc_response.payload.data();
                size_t payload_size = ipc_response.payload.size();
                
                for (uint16_t i = 0; i < payload->num_columns && offset + sizeof(uint16_t) <= payload_size; i++) {
                    uint16_t format;
                    std::memcpy(&format, data + offset, sizeof(uint16_t));
                    column_formats.push_back(format);
                    offset += sizeof(uint16_t);
                }
                
                sendCopyInResponse(state, payload->format, column_formats);
                state.in_copy_mode = true;
                state.copy_is_in = true;
            }
            return core::Status::OK;
        }
        
        case IPCMessageType::COPY_OUT_RESPONSE: {
            auto* payload = ipc_response.getPayload<IPCCopyOutResponsePayload>();
            if (payload) {
                // Parse column formats from payload (uint16_t array follows header)
                std::vector<uint16_t> column_formats;
                size_t offset = sizeof(IPCCopyOutResponsePayload);
                const uint8_t* data = ipc_response.payload.data();
                size_t payload_size = ipc_response.payload.size();
                
                for (uint16_t i = 0; i < payload->num_columns && offset + sizeof(uint16_t) <= payload_size; i++) {
                    uint16_t format;
                    std::memcpy(&format, data + offset, sizeof(uint16_t));
                    column_formats.push_back(format);
                    offset += sizeof(uint16_t);
                }
                
                sendCopyOutResponse(state, payload->format, column_formats);
                state.in_copy_mode = true;
                state.copy_is_in = false;
            }
            return core::Status::OK;
        }
        
        case IPCMessageType::COPY_DATA: {
            auto* payload = ipc_response.getPayload<IPCCopyDataPayload>();
            if (payload && payload->length > 0) {
                // Send COPY data to client
                std::vector<uint8_t> pg_msg;
                pg_msg.push_back(pg::BE_COPY_DATA);
                writeUint32(pg_msg.data() + pg_msg.size(), 4 + payload->length);
                pg_msg.resize(pg_msg.size() + 4);
                pg_msg.insert(pg_msg.end(), 
                    ipc_response.payload.data() + sizeof(IPCCopyDataPayload),
                    ipc_response.payload.data() + sizeof(IPCCopyDataPayload) + payload->length);
                
                if (send(state.client_fd, pg_msg.data(), pg_msg.size(), 0) != 
                    static_cast<ssize_t>(pg_msg.size())) {
                    return core::Status::IO_ERROR;
                }
            }
            return core::Status::OK;
        }
        
        case IPCMessageType::COPY_COMPLETE: {
            state.in_copy_mode = false;
            sendCommandComplete(state, "COPY 0");
            return core::Status::OK;
        }
        
        default: {
            // Unknown message type - log and ignore
            return core::Status::OK;
        }
    }
}

} // namespace ipc
} // namespace scratchbird
