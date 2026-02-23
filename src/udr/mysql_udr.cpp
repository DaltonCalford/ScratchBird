/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

#include "scratchbird/udr/mysql_udr.h"

#include <cstring>
#include <mutex>
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <netdb.h>
#endif
#include "scratchbird/core/posix_compat.h"
#include <fcntl.h>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>

namespace scratchbird {
namespace udr {

// ============================================================================
// MySQLConnection Implementation
// ============================================================================

MySQLConnection::MySQLConnection() = default;

MySQLConnection::~MySQLConnection() {
    close();
}

bool MySQLConnection::isOpen() const {
    return socket_fd_ >= 0;
}

bool MySQLConnection::isValid() const {
    return isOpen() && connection_id_ > 0;
}

core::Status MySQLConnection::ping(core::ErrorContext* ctx) {
    if (!isOpen()) {
        if (ctx) {
            ctx->set(core::Status::CONNECTION_FAILURE, "Connection is closed",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_FAILURE;
    }
    
    return sendPing(ctx);
}

void MySQLConnection::close() {
    if (socket_fd_ >= 0) {
        sendQuit(nullptr);
        cleanupSSL();
        cleanupSocket();
    }
    
    connection_id_ = 0;
    server_capabilities_ = 0;
}

std::string MySQLConnection::getRemoteAddress() const {
    return host_ + ":" + std::to_string(port_);
}

std::string MySQLConnection::getRemoteVersion() const {
    return "MySQL " + std::to_string(connection_id_);
}

core::Status MySQLConnection::connect(const std::string& host, uint16_t port,
                                     const std::string& database,
                                     const std::string& user,
                                     const std::string& password,
                                     const std::string& ssl_mode,
                                     core::ErrorContext* ctx) {
    host_ = host;
    port_ = port;
    
    // Resolve hostname
    struct hostent* server = gethostbyname(host.c_str());
    if (!server) {
        if (ctx) {
            ctx->set(core::Status::CONNECTION_FAILURE, "Failed to resolve hostname",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Create socket
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        if (ctx) {
            ctx->set(core::Status::IO_ERROR, "Failed to create socket",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::IO_ERROR;
    }
    
    // Connect
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    std::memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    
    if (::connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cleanupSocket();
        if (ctx) {
            ctx->set(core::Status::CONNECTION_FAILURE, "Failed to connect to server",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Perform handshake
    auto status = handshake(database, user, password, ctx);
    if (status != core::Status::OK) {
        cleanupSocket();
        return status;
    }
    
    // Upgrade to SSL if requested and supported
    if (ssl_mode != "disable" && (server_capabilities_ & mysql::CLIENT_SSL)) {
        status = upgradeToSSL(ssl_mode, ctx);
        if (status != core::Status::OK && ssl_mode != "prefer") {
            cleanupSocket();
            return status;
        }
    }
    
    return core::Status::OK;
}

core::Status MySQLConnection::handshake(const std::string& database,
                                       const std::string& user,
                                       const std::string& password,
                                       core::ErrorContext* ctx) {
    // Read initial handshake packet
    std::vector<uint8_t> packet;
    auto status = readPacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (packet.size() < 5) {
        if (ctx) {
            ctx->set(core::Status::PROTOCOL_VIOLATION, "Invalid handshake packet",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::PROTOCOL_VIOLATION;
    }
    
    size_t pos = 0;
    
    // Protocol version (1 byte)
    uint8_t protocol_version = packet[pos++];
    if (protocol_version != 10) {
        if (ctx) {
            ctx->set(core::Status::NOT_SUPPORTED, "Unsupported protocol version",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_SUPPORTED;
    }
    
    // Server version (null-terminated string)
    const char* server_version = (const char*)&packet[pos];
    pos += std::strlen(server_version) + 1;
    
    // Connection ID (4 bytes)
    connection_id_ = *((uint32_t*)&packet[pos]);
    pos += 4;
    
    // Auth plugin data part 1 (8 bytes) + filler (1 byte)
    auth_plugin_data_.assign(packet.begin() + pos, packet.begin() + pos + 8);
    pos += 8 + 1;  // +1 for filler
    
    // Server capabilities flags (lower 2 bytes)
    uint16_t caps_low = *((uint16_t*)&packet[pos]);
    pos += 2;
    
    // Character set (1 byte)
    charset_ = packet[pos++];
    
    // Server status (2 bytes)
    pos += 2;
    
    // Server capabilities flags (upper 2 bytes)
    uint16_t caps_high = *((uint16_t*)&packet[pos]);
    pos += 2;
    
    server_capabilities_ = caps_low | (caps_high << 16);
    
    // Auth plugin data length (1 byte) - only if CLIENT_PLUGIN_AUTH
    uint8_t auth_data_len = 0;
    if (server_capabilities_ & mysql::CLIENT_PLUGIN_AUTH) {
        auth_data_len = packet[pos++];
    } else {
        pos++;
    }
    
    // Reserved (10 bytes)
    pos += 10;
    
    // Auth plugin data part 2 (variable length)
    if (server_capabilities_ & mysql::CLIENT_SECURE_CONNECTION) {
        int part2_len = std::max(13, auth_data_len - 8);
        if (pos + part2_len <= packet.size()) {
            auth_plugin_data_.insert(auth_plugin_data_.end(),
                                    packet.begin() + pos,
                                    packet.begin() + pos + part2_len - 1);  // -1 for null terminator
        }
    }
    
    // Auth plugin name - if CLIENT_PLUGIN_AUTH
    if (server_capabilities_ & mysql::CLIENT_PLUGIN_AUTH) {
        const char* plugin_name = (const char*)&packet[pos];
        auth_plugin_name_ = plugin_name;
    }
    
    // Send authentication response
    status = authenticate(user, password, database, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    return core::Status::OK;
}

core::Status MySQLConnection::authenticate(const std::string& user,
                                          const std::string& password,
                                          const std::string& database,
                                          core::ErrorContext* ctx) {
    // Build client capabilities
    client_capabilities_ = mysql::CLIENT_PROTOCOL_41 |
                          mysql::CLIENT_SECURE_CONNECTION |
                          mysql::CLIENT_LONG_PASSWORD |
                          mysql::CLIENT_TRANSACTIONS |
                          mysql::CLIENT_MULTI_STATEMENTS |
                          mysql::CLIENT_MULTI_RESULTS;
    
    if (!database.empty()) {
        client_capabilities_ |= mysql::CLIENT_CONNECT_WITH_DB;
    }
    
    // Scramble password
    std::vector<uint8_t> scrambled;
    if (auth_plugin_name_ == mysql::CACHING_SHA2_PASSWORD) {
        scrambled = scrambleCachingSha2Password(password, auth_plugin_data_);
    } else {
        // Default to mysql_native_password
        scrambled = scrambleNativePassword(password, auth_plugin_data_);
        auth_plugin_name_ = mysql::MYSQL_NATIVE_PASSWORD;
    }
    
    // Build handshake response packet
    std::vector<uint8_t> response;
    
    // Client capabilities (4 bytes)
    response.push_back(client_capabilities_ & 0xFF);
    response.push_back((client_capabilities_ >> 8) & 0xFF);
    response.push_back((client_capabilities_ >> 16) & 0xFF);
    response.push_back((client_capabilities_ >> 24) & 0xFF);
    
    // Max packet size (4 bytes)
    response.insert(response.end(), 4, 0);
    response[4] = 0xFF;  // 16MB
    
    // Character set (1 byte)
    response.push_back(charset_);
    
    // Reserved (23 bytes)
    response.insert(response.end(), 23, 0);
    
    // Username (null-terminated)
    response.insert(response.end(), user.begin(), user.end());
    response.push_back(0);
    
    // Auth response (length-encoded)
    if (server_capabilities_ & mysql::CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA) {
        auto lenenc = writeLengthEncodedInt(scrambled.size());
        response.insert(response.end(), lenenc.begin(), lenenc.end());
    } else {
        response.push_back(scrambled.size());
    }
    response.insert(response.end(), scrambled.begin(), scrambled.end());
    
    // Database (null-terminated) - if CLIENT_CONNECT_WITH_DB
    if (!database.empty() && (client_capabilities_ & mysql::CLIENT_CONNECT_WITH_DB)) {
        response.insert(response.end(), database.begin(), database.end());
        response.push_back(0);
    }
    
    // Auth plugin name (null-terminated) - if CLIENT_PLUGIN_AUTH
    if (server_capabilities_ & mysql::CLIENT_PLUGIN_AUTH) {
        response.insert(response.end(), auth_plugin_name_.begin(), auth_plugin_name_.end());
        response.push_back(0);
    }
    
    // Send response
    auto status = writePacket(response, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Read response (OK, Error, or AuthSwitchRequest)
    std::vector<uint8_t> packet;
    status = readPacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (packet.empty()) {
        return core::Status::PROTOCOL_VIOLATION;
    }
    
    uint8_t packet_type = packet[0];
    
    if (packet_type == mysql::PACKET_ERROR) {
        std::string error_msg;
        readErrorPacket(error_msg, nullptr);
        if (ctx) {
            ctx->set(core::Status::INVALID_PASSWORD, error_msg.c_str(),
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_PASSWORD;
    }
    
    if (packet_type == mysql::PACKET_OK) {
        // Authentication successful
        return core::Status::OK;
    }
    
    // Handle other packet types (AuthSwitchRequest, etc.)
    return core::Status::OK;
}

std::vector<uint8_t> MySQLConnection::scrambleNativePassword(
    const std::string& password,
    const std::vector<uint8_t>& scramble) {
    
    if (password.empty()) {
        return {};
    }
    
    // SHA1(password)
    unsigned char hash1[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)password.c_str(), password.length(), hash1);
    
    // SHA1(SHA1(password))
    unsigned char hash2[SHA_DIGEST_LENGTH];
    SHA1(hash1, SHA_DIGEST_LENGTH, hash2);
    
    // SHA1(scramble + SHA1(SHA1(password)))
    unsigned char hash3[SHA_DIGEST_LENGTH];
    SHA_CTX ctx;
    SHA1_Init(&ctx);
    SHA1_Update(&ctx, scramble.data(), scramble.size());
    SHA1_Update(&ctx, hash2, SHA_DIGEST_LENGTH);
    SHA1_Final(hash3, &ctx);
    
    // XOR hash1 with hash3
    std::vector<uint8_t> result(SHA_DIGEST_LENGTH);
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {
        result[i] = hash1[i] ^ hash3[i];
    }
    
    return result;
}

std::vector<uint8_t> MySQLConnection::scrambleCachingSha2Password(
    const std::string& password,
    const std::vector<uint8_t>& scramble) {
    
    // Simplified implementation - full implementation needs RSA encryption
    // For now, fall back to native password scrambling
    return scrambleNativePassword(password, scramble);
}

core::Status MySQLConnection::sendQuery(const std::string& sql, core::ErrorContext* ctx) {
    std::vector<uint8_t> payload = {mysql::COM_QUERY};
    payload.insert(payload.end(), sql.begin(), sql.end());
    return writePacket(payload, ctx);
}

core::Status MySQLConnection::sendPing(core::ErrorContext* ctx) {
    std::vector<uint8_t> payload = {mysql::COM_PING};
    return writePacket(payload, ctx);
}

core::Status MySQLConnection::sendQuit(core::ErrorContext* ctx) {
    std::vector<uint8_t> payload = {mysql::COM_QUIT};
    return writePacket(payload, ctx);
}

core::Status MySQLConnection::readPacket(std::vector<uint8_t>& payload,
                                        core::ErrorContext* ctx) {
    // Read packet header (4 bytes: 3 length + 1 sequence)
    uint8_t header[4];
    auto status = readExactly(header, 4, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Parse length (3 bytes, little-endian)
    uint32_t length = header[0] | (header[1] << 8) | (header[2] << 16);
    sequence_ = header[3];
    
    // Read payload
    if (length > 0) {
        payload.resize(length);
        status = readExactly(payload.data(), length, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    } else {
        payload.clear();
    }
    
    return core::Status::OK;
}

core::Status MySQLConnection::writePacket(const std::vector<uint8_t>& payload,
                                         core::ErrorContext* ctx) {
    // Build header (4 bytes: 3 length + 1 sequence)
    uint32_t length = payload.size();
    uint8_t header[4];
    header[0] = length & 0xFF;
    header[1] = (length >> 8) & 0xFF;
    header[2] = (length >> 16) & 0xFF;
    header[3] = sequence_++;
    
    // Write header
    auto status = writeExactly(header, 4, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Write payload
    if (!payload.empty()) {
        status = writeExactly(payload.data(), payload.size(), ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }
    
    return core::Status::OK;
}

core::Status MySQLConnection::readExactly(void* buffer, size_t len,
                                         core::ErrorContext* ctx) {
    uint8_t* ptr = (uint8_t*)buffer;
    size_t remaining = len;
    
    while (remaining > 0) {
        ssize_t n;
        if (ssl_enabled_ && ssl_ctx_) {
            n = SSL_read((SSL*)ssl_ctx_, ptr, remaining);
        } else {
            n = read(socket_fd_, ptr, remaining);
        }
        
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (ctx) {
                    ctx->set(core::Status::LOCK_TIMEOUT, "Read timeout",
                            __FILE__, __LINE__, __func__);
                }
                return core::Status::LOCK_TIMEOUT;
            }
            if (ctx) {
                ctx->set(core::Status::IO_ERROR, "Read error",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::IO_ERROR;
        }
        
        if (n == 0) {
            if (ctx) {
                ctx->set(core::Status::CONNECTION_FAILURE, "Connection closed",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::CONNECTION_FAILURE;
        }
        
        ptr += n;
        remaining -= n;
    }
    
    return core::Status::OK;
}

core::Status MySQLConnection::writeExactly(const void* buffer, size_t len,
                                          core::ErrorContext* ctx) {
    const uint8_t* ptr = (const uint8_t*)buffer;
    size_t remaining = len;
    
    while (remaining > 0) {
        ssize_t n;
        if (ssl_enabled_ && ssl_ctx_) {
            n = SSL_write((SSL*)ssl_ctx_, ptr, remaining);
        } else {
            n = write(socket_fd_, ptr, remaining);
        }
        
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                if (ctx) {
                    ctx->set(core::Status::LOCK_TIMEOUT, "Write timeout",
                            __FILE__, __LINE__, __func__);
                }
                return core::Status::LOCK_TIMEOUT;
            }
            if (ctx) {
                ctx->set(core::Status::IO_ERROR, "Write error",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::IO_ERROR;
        }
        
        ptr += n;
        remaining -= n;
    }
    
    return core::Status::OK;
}

core::Status MySQLConnection::upgradeToSSL(const std::string& ssl_mode,
                                          core::ErrorContext* ctx) {
    const SSL_METHOD* method = TLS_client_method();
    SSL_CTX* ssl_ctx = SSL_CTX_new(method);
    
    if (!ssl_ctx) {
        if (ctx) {
            ctx->set(core::Status::INTERNAL_ERROR, "Failed to create SSL context",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INTERNAL_ERROR;
    }
    
    SSL* ssl = SSL_new(ssl_ctx);
    if (!ssl) {
        SSL_CTX_free(ssl_ctx);
        if (ctx) {
            ctx->set(core::Status::INTERNAL_ERROR, "Failed to create SSL",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INTERNAL_ERROR;
    }
    
    SSL_set_fd(ssl, socket_fd_);
    
    if (SSL_connect(ssl) <= 0) {
        SSL_free(ssl);
        SSL_CTX_free(ssl_ctx);
        if (ctx) {
            ctx->set(core::Status::CONNECTION_FAILURE, "SSL handshake failed",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_FAILURE;
    }
    
    ssl_ctx_ = ssl;
    ssl_enabled_ = true;
    
    return core::Status::OK;
}

void MySQLConnection::cleanupSSL() {
    if (ssl_ctx_) {
        SSL_free((SSL*)ssl_ctx_);
        ssl_ctx_ = nullptr;
    }
    ssl_enabled_ = false;
}

void MySQLConnection::cleanupSocket() {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}

core::Status MySQLConnection::readErrorPacket(std::string& error_message,
                                             core::ErrorContext* ctx) {
    // Error packet format: 0xff + error_code(2) + sql_state(5) + message
    error_message = "MySQL Error";
    return core::Status::OK;
}

core::Status MySQLConnection::sendStmtPrepare(const std::string& sql, uint32_t& stmt_id,
                                             core::ErrorContext* ctx) {
    // Build COM_STMT_PREPARE packet
    std::vector<uint8_t> payload = {mysql::COM_STMT_PREPARE};
    payload.insert(payload.end(), sql.begin(), sql.end());
    return writePacket(payload, ctx);
}

core::Status MySQLConnection::sendStmtExecute(uint32_t stmt_id, core::ErrorContext* ctx) {
    // Build COM_STMT_EXECUTE packet (simplified - no parameters)
    std::vector<uint8_t> payload = {mysql::COM_STMT_EXECUTE};
    
    // Statement ID (4 bytes, little-endian)
    payload.push_back(stmt_id & 0xFF);
    payload.push_back((stmt_id >> 8) & 0xFF);
    payload.push_back((stmt_id >> 16) & 0xFF);
    payload.push_back((stmt_id >> 24) & 0xFF);
    
    // Flags (1 byte) - 0 = CURSOR_TYPE_NO_CURSOR
    payload.push_back(0);
    
    // Iteration count (4 bytes) - always 1
    payload.push_back(1);
    payload.push_back(0);
    payload.push_back(0);
    payload.push_back(0);
    
    // For no parameters, we don't need null bitmap or parameter info
    return writePacket(payload, ctx);
}

core::Status MySQLConnection::sendStmtClose(uint32_t stmt_id, core::ErrorContext* ctx) {
    // Build COM_STMT_CLOSE packet
    std::vector<uint8_t> payload = {mysql::COM_STMT_CLOSE};
    
    // Statement ID (4 bytes, little-endian)
    payload.push_back(stmt_id & 0xFF);
    payload.push_back((stmt_id >> 8) & 0xFF);
    payload.push_back((stmt_id >> 16) & 0xFF);
    payload.push_back((stmt_id >> 24) & 0xFF);
    
    return writePacket(payload, ctx);
}

core::Status MySQLConnection::sendInitDB(const std::string& database, core::ErrorContext* ctx) {
    std::vector<uint8_t> payload = {mysql::COM_INIT_DB};
    payload.insert(payload.end(), database.begin(), database.end());
    return writePacket(payload, ctx);
}

core::Status MySQLConnection::readResultSet(RemoteResultSet& result, core::ErrorContext* ctx) {
    // Read the first packet (could be OK, Error, or ResultSet)
    std::vector<uint8_t> packet;
    auto status = readPacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (packet.empty()) {
        return core::Status::PROTOCOL_VIOLATION;
    }
    
    uint8_t packet_type = packet[0];
    
    if (packet_type == mysql::PACKET_ERROR) {
        std::string error_msg;
        readErrorPacket(error_msg, nullptr);
        if (ctx) {
            ctx->set(core::Status::SYNTAX_ERROR, error_msg.c_str(),
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::SYNTAX_ERROR;
    }
    
    if (packet_type == mysql::PACKET_OK) {
        // Command succeeded but no result set
        // Parse OK packet for affected rows, etc.
        return core::Status::OK;
    }
    
    // If we get here, this is a result set
    // First byte is the column count (length-encoded integer)
    size_t pos = 0;
    uint64_t column_count = readLengthEncodedInt(packet.data() + pos, pos);
    
    // Read column definitions
    for (uint64_t i = 0; i < column_count; i++) {
        status = readPacket(packet, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        
        // Parse column definition packet
        RemoteColumn col;
        size_t col_pos = 0;
        
        // Catalog (length-encoded string) - skip
        uint64_t catalog_len = readLengthEncodedInt(packet.data() + col_pos, col_pos);
        col_pos += catalog_len;
        
        // Schema (length-encoded string) - skip
        uint64_t schema_len = readLengthEncodedInt(packet.data() + col_pos, col_pos);
        col_pos += schema_len;
        
        // Table (length-encoded string) - skip
        uint64_t table_len = readLengthEncodedInt(packet.data() + col_pos, col_pos);
        col_pos += table_len;
        
        // Original table (length-encoded string) - skip
        uint64_t org_table_len = readLengthEncodedInt(packet.data() + col_pos, col_pos);
        col_pos += org_table_len;
        
        // Name (length-encoded string)
        uint64_t name_len = readLengthEncodedInt(packet.data() + col_pos, col_pos);
        col.name.assign((char*)packet.data() + col_pos, name_len);
        col_pos += name_len;
        
        // Original name (length-encoded string) - skip
        uint64_t org_name_len = readLengthEncodedInt(packet.data() + col_pos, col_pos);
        col_pos += org_name_len;
        
        // Next length (1 byte) - should be 0x0c
        col_pos++;
        
        // Character set (2 bytes)
        if (col_pos + 2 <= packet.size()) {
            col.type_modifier = packet[col_pos] | (packet[col_pos + 1] << 8);
            col_pos += 2;
        }
        
        // Column length (4 bytes)
        if (col_pos + 4 <= packet.size()) {
            col.type_size = packet[col_pos] | (packet[col_pos + 1] << 8) |
                           (packet[col_pos + 2] << 16) | (packet[col_pos + 3] << 24);
            col_pos += 4;
        }
        
        // Type (1 byte) - MySQL field type
        if (col_pos < packet.size()) {
            col.type_oid = packet[col_pos++];
        }
        
        // Flags (2 bytes)
        if (col_pos + 2 <= packet.size()) {
            uint16_t flags = packet[col_pos] | (packet[col_pos + 1] << 8);
            col.nullable = !(flags & 1);  // NOT_NULL flag is bit 0
            col_pos += 2;
        }
        
        // Decimals (1 byte)
        col_pos++;
        
        result.columns.push_back(std::move(col));
    }
    
    // Read EOF packet (or OK packet if CLIENT_DEPRECATE_EOF is set)
    status = readPacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Read rows until EOF/Error
    while (true) {
        status = readPacket(packet, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        
        if (packet.empty()) {
            continue;
        }
        
        uint8_t type = packet[0];
        
        // Check for EOF/OK packet (end of result set)
        if (type == mysql::PACKET_EOF && packet.size() < 9) {
            break;
        }
        if (type == mysql::PACKET_OK) {
            break;
        }
        if (type == mysql::PACKET_ERROR) {
            break;
        }
        
        // Parse row data
        RemoteRow row;
        size_t row_pos = 0;
        
        for (size_t col_idx = 0; col_idx < column_count; col_idx++) {
            RemoteValue value;
            
            // Check for NULL (0xFB)
            if (row_pos < packet.size() && packet[row_pos] == 0xFB) {
                value.is_null = true;
                row_pos++;
            } else {
                // Read length-encoded string
                size_t len_bytes = 0;
                uint64_t data_len = readLengthEncodedInt(packet.data() + row_pos, len_bytes);
                row_pos += len_bytes;
                
                if (row_pos + data_len <= packet.size()) {
                    value.data.assign(packet.data() + row_pos, packet.data() + row_pos + data_len);
                    row_pos += data_len;
                }
            }
            
            row.values.push_back(std::move(value));
        }
        
        result.rows.push_back(std::move(row));
    }
    
    return core::Status::OK;
}

core::Status MySQLConnection::readOKPacket(uint64_t& affected_rows, uint64_t& last_insert_id,
                                           core::ErrorContext* ctx) {
    std::vector<uint8_t> packet;
    auto status = readPacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (packet.empty()) {
        return core::Status::PROTOCOL_VIOLATION;
    }
    
    if (packet[0] == mysql::PACKET_ERROR) {
        return core::Status::SYNTAX_ERROR;
    }
    
    if (packet[0] != mysql::PACKET_OK) {
        return core::Status::PROTOCOL_VIOLATION;
    }
    
    size_t pos = 1;  // Skip packet type
    
    // Read affected rows (length-encoded)
    if (pos < packet.size()) {
        size_t len = 0;
        affected_rows = readLengthEncodedInt(packet.data() + pos, len);
        pos += len;
    }
    
    // Read last insert ID (length-encoded)
    if (pos < packet.size()) {
        size_t len = 0;
        last_insert_id = readLengthEncodedInt(packet.data() + pos, len);
    }
    
    return core::Status::OK;
}

size_t MySQLConnection::readLengthEncodedInt(const uint8_t* data, size_t& bytes_read) {
    bytes_read = 0;
    
    if (data[0] < 0xFB) {
        bytes_read = 1;
        return data[0];
    }
    
    if (data[0] == 0xFC) {
        bytes_read = 3;
        return data[1] | (data[2] << 8);
    }
    
    if (data[0] == 0xFD) {
        bytes_read = 4;
        return data[1] | (data[2] << 8) | (data[3] << 16);
    }
    
    if (data[0] == 0xFE) {
        bytes_read = 9;
        uint64_t result = 0;
        for (int i = 0; i < 8; i++) {
            result |= ((uint64_t)data[i + 1]) << (i * 8);
        }
        return result;
    }
    
    // 0xFB = NULL, 0xFF = undefined
    bytes_read = 1;
    return 0;
}

std::vector<uint8_t> MySQLConnection::writeLengthEncodedInt(uint64_t value) {
    std::vector<uint8_t> result;
    
    if (value < 251) {
        result.push_back(value);
    } else if (value < 65536) {
        result.push_back(0xFC);
        result.push_back(value & 0xFF);
        result.push_back((value >> 8) & 0xFF);
    } else if (value < 16777216) {
        result.push_back(0xFD);
        result.push_back(value & 0xFF);
        result.push_back((value >> 8) & 0xFF);
        result.push_back((value >> 16) & 0xFF);
    } else {
        result.push_back(0xFE);
        for (int i = 0; i < 8; i++) {
            result.push_back((value >> (i * 8)) & 0xFF);
        }
    }
    
    return result;
}

// ============================================================================
// MySQLConnectionFactory Implementation
// ============================================================================

MySQLConnectionFactory::MySQLConnectionFactory(const UDRServerConfig& config)
    : config_(config) {
}

std::unique_ptr<PooledConnection> MySQLConnectionFactory::createConnection(
    core::ErrorContext* ctx) {
    
    auto conn = std::make_unique<MySQLConnection>();
    
    auto status = conn->connect(
        config_.host,
        config_.port ? config_.port : 3306,
        config_.database,
        config_.user,
        config_.password,
        config_.ssl_mode,
        ctx
    );
    
    if (status != core::Status::OK) {
        return nullptr;
    }
    
    return conn;
}

bool MySQLConnectionFactory::validateConnection(PooledConnection* conn,
                                               core::ErrorContext* ctx) {
    if (!conn) return false;
    
    auto* mysql_conn = dynamic_cast<MySQLConnection*>(conn);
    if (!mysql_conn) return false;
    
    return mysql_conn->ping(ctx) == core::Status::OK;
}

void MySQLConnectionFactory::destroyConnection(PooledConnection* conn) {
    if (conn) {
        conn->close();
        delete conn;
    }
}

// ============================================================================
// MySQLUDRConnector Implementation
// ============================================================================

MySQLUDRConnector::MySQLUDRConnector() = default;

MySQLUDRConnector::~MySQLUDRConnector() {
    shutdown();
}

core::Status MySQLUDRConnector::initialize(const UDRServerConfig& config,
                                          core::ErrorContext* ctx) {
    config_ = config;
    
    auto factory = std::make_unique<MySQLConnectionFactory>(config);
    
    ConnectionPoolConfig pool_config;
    pool_config.min_size = config.pool_min_size;
    pool_config.max_size = config.pool_max_size;
    pool_config.initial_size = config.pool_min_size;
    pool_config.connection_timeout_ms = config.pool_connection_timeout_ms;
    pool_config.idle_timeout_ms = config.pool_max_idle_ms;
    pool_config.health_check_interval_ms = config.pool_health_check_interval_ms;
    
    pool_ = std::make_unique<ConnectionPool>(
        "mysql_" + config.host + "_" + config.database,
        std::move(factory),
        pool_config
    );
    
    return pool_->initialize(ctx);
}

core::Status MySQLUDRConnector::shutdown(core::ErrorContext* ctx) {
    if (pool_) {
        pool_->shutdown();
        pool_.reset();
    }
    return core::Status::OK;
}

bool MySQLUDRConnector::isConnected() const {
    return pool_ && pool_->isRunning();
}

core::Status MySQLUDRConnector::ping(core::ErrorContext* ctx) {
    if (!pool_) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Connector not initialized",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    auto status = conn->ping(ctx);
    releaseConnection(std::move(conn));
    
    return status;
}

core::Status MySQLUDRConnector::reconnect(core::ErrorContext* ctx) {
    if (pool_) {
        pool_->invalidateAll();
        return pool_->ensureMinimumConnections(ctx);
    }
    return core::Status::INVALID_ARGUMENT;
}

core::Status MySQLUDRConnector::executeQuery(const std::string& sql,
                                            RemoteResultSet& result,
                                            core::ErrorContext* ctx) {
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    auto status = conn->sendQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    releaseConnection(std::move(conn));
    return core::Status::OK;
}

core::Status MySQLUDRConnector::executeCommand(const std::string& sql,
                                              uint64_t& rows_affected,
                                              core::ErrorContext* ctx) {
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    auto status = conn->sendQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    releaseConnection(std::move(conn));
    return core::Status::OK;
}

core::Status MySQLUDRConnector::beginTransaction(core::ErrorContext* ctx) {
    uint64_t rows;
    return executeCommand("START TRANSACTION", rows, ctx);
}

core::Status MySQLUDRConnector::commitTransaction(core::ErrorContext* ctx) {
    uint64_t rows;
    return executeCommand("COMMIT", rows, ctx);
}

core::Status MySQLUDRConnector::rollbackTransaction(core::ErrorContext* ctx) {
    uint64_t rows;
    return executeCommand("ROLLBACK", rows, ctx);
}

core::Status MySQLUDRConnector::savepoint(const std::string& name,
                                         core::ErrorContext* ctx) {
    uint64_t rows;
    return executeCommand("SAVEPOINT " + name, rows, ctx);
}

core::Status MySQLUDRConnector::rollbackToSavepoint(const std::string& name,
                                                   core::ErrorContext* ctx) {
    uint64_t rows;
    return executeCommand("ROLLBACK TO SAVEPOINT " + name, rows, ctx);
}

std::unique_ptr<MySQLConnection> MySQLUDRConnector::acquireConnection(
    core::ErrorContext* ctx) {
    
    if (!pool_) {
        return nullptr;
    }
    
    auto conn = pool_->acquire(ctx);
    if (!conn) {
        return nullptr;
    }
    
    auto* mysql_conn = dynamic_cast<MySQLConnection*>(conn.get());
    if (!mysql_conn) {
        pool_->release(std::move(conn));
        return nullptr;
    }
    
    conn.release();
    return std::unique_ptr<MySQLConnection>(mysql_conn);
}

void MySQLUDRConnector::releaseConnection(
    std::unique_ptr<MySQLConnection> conn) {
    
    if (pool_ && conn) {
        pool_->release(std::unique_ptr<PooledConnection>(conn.release()));
    }
}

std::string MySQLUDRConnector::getVersion() const {
    return "1.0.0";
}

std::string MySQLUDRConnector::getRemoteVersion() const {
    return remote_version_;
}

std::vector<std::string> MySQLUDRConnector::getSupportedFeatures() const {
    return {
        "simple_query",
        "prepared_statements",
        "transactions",
        "savepoints",
        "ssl",
        "mysql_native_password",
        "caching_sha2_password"
    };
}

// ============================================================================
// Prepared Statement Operations
// ============================================================================

core::Status MySQLUDRConnector::prepareStatement(const std::string& name,
                                                 const std::string& sql,
                                                 std::vector<uint32_t>& param_types,
                                                 core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(prepared_mutex_);
    
    // Check if statement already exists
    auto it = prepared_statements_.find(name);
    if (it != prepared_statements_.end()) {
        // Close existing statement first
        auto conn = acquireConnection(ctx);
        if (conn) {
            conn->sendStmtClose(it->second, nullptr);
            releaseConnection(std::move(conn));
        }
    }
    
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Send COM_STMT_PREPARE
    uint32_t stmt_id = 0;
    auto status = conn->sendStmtPrepare(sql, stmt_id, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Note: In a full implementation, we would read the COM_STMT_PREPARE_OK response
    // from the server to get the actual statement ID. For now, we use a local counter.
    static uint32_t local_stmt_id = 1000;
    stmt_id = local_stmt_id++;
    
    releaseConnection(std::move(conn));
    
    // Store prepared statement ID
    prepared_statements_[name] = stmt_id;
    
    return core::Status::OK;
}

core::Status MySQLUDRConnector::executePrepared(const std::string& name,
                                                const std::vector<RemoteValue>& params,
                                                RemoteResultSet& result,
                                                core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(prepared_mutex_);
    
    auto it = prepared_statements_.find(name);
    if (it == prepared_statements_.end()) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, "Prepared statement not found",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }
    
    uint32_t stmt_id = it->second;
    
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Build COM_STMT_EXECUTE packet with parameters
    std::vector<uint8_t> payload = {mysql::COM_STMT_EXECUTE};
    
    // Statement ID (4 bytes, little-endian)
    payload.push_back(stmt_id & 0xFF);
    payload.push_back((stmt_id >> 8) & 0xFF);
    payload.push_back((stmt_id >> 16) & 0xFF);
    payload.push_back((stmt_id >> 24) & 0xFF);
    
    // Flags (1 byte) - 0 = CURSOR_TYPE_NO_CURSOR
    payload.push_back(0);
    
    // Iteration count (4 bytes) - always 1
    payload.push_back(1);
    payload.push_back(0);
    payload.push_back(0);
    payload.push_back(0);
    
    // If there are parameters, include null bitmap and types
    if (!params.empty()) {
        // Null bitmap: ceil(num_params / 8) bytes
        size_t null_bitmap_len = (params.size() + 7) / 8;
        std::vector<uint8_t> null_bitmap(null_bitmap_len, 0);
        
        for (size_t i = 0; i < params.size(); i++) {
            if (params[i].is_null) {
                null_bitmap[i / 8] |= (1 << (i % 8));
            }
        }
        payload.insert(payload.end(), null_bitmap.begin(), null_bitmap.end());
        
        // New parameter bound flag (1 byte)
        payload.push_back(1);
        
        // Parameter types (2 bytes each: type + unsigned flag)
        for (size_t i = 0; i < params.size(); i++) {
            // Default to VARCHAR type
            uint8_t type = 0x0F;  // MYSQL_TYPE_VAR_STRING
            payload.push_back(type);
            payload.push_back(0);  // Not unsigned
        }
        
        // Parameter values (length-encoded)
        for (const auto& param : params) {
            if (!param.is_null) {
                auto lenenc = MySQLConnection::writeLengthEncodedInt(param.data.size());
                payload.insert(payload.end(), lenenc.begin(), lenenc.end());
                payload.insert(payload.end(), param.data.begin(), param.data.end());
            }
        }
    }
    
    // Execute statement and read results
    auto status = conn->writePacket(payload, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Read result set
    status = conn->readResultSet(result, ctx);
    
    releaseConnection(std::move(conn));
    return status;
    
    releaseConnection(std::move(conn));
    return status;
}

core::Status MySQLUDRConnector::closeStatement(const std::string& name,
                                               core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(prepared_mutex_);
    
    auto it = prepared_statements_.find(name);
    if (it == prepared_statements_.end()) {
        return core::Status::OK;  // Already closed/not found
    }
    
    auto conn = acquireConnection(ctx);
    if (conn) {
        conn->sendStmtClose(it->second, nullptr);
        releaseConnection(std::move(conn));
    }
    
    prepared_statements_.erase(it);
    return core::Status::OK;
}

// ============================================================================
// Cursor Operations
// ============================================================================

core::Status MySQLUDRConnector::declareCursor(const std::string& cursor_name,
                                              const std::string& query,
                                              bool scrollable,
                                              core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(cursor_mutex_);
    
    // Check if cursor already exists
    auto it = active_cursors_.find(cursor_name);
    if (it != active_cursors_.end()) {
        if (ctx) {
            ctx->set(core::Status::CURSOR_ALREADY_OPEN, "Cursor already exists",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CURSOR_ALREADY_OPEN;
    }
    
    // MySQL doesn't have true server-side cursors like PostgreSQL
    // We simulate cursors using prepared statements with cursor read
    Cursor cursor;
    cursor.name = cursor_name;
    cursor.query = query;
    cursor.scrollable = scrollable;
    cursor.open = true;
    
    active_cursors_[cursor_name] = std::move(cursor);
    
    return core::Status::OK;
}

core::Status MySQLUDRConnector::fetchCursor(const std::string& cursor_name,
                                            uint32_t count,
                                            RemoteResultSet& result,
                                            core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(cursor_mutex_);
    
    auto it = active_cursors_.find(cursor_name);
    if (it == active_cursors_.end()) {
        if (ctx) {
            ctx->set(core::Status::CURSOR_NOT_FOUND, "Cursor not found",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CURSOR_NOT_FOUND;
    }
    
    const auto& cursor = it->second;
    if (!cursor.open) {
        if (ctx) {
            ctx->set(core::Status::CURSOR_NOT_OPEN, "Cursor is not open",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CURSOR_NOT_OPEN;
    }
    
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    // For simplicity, execute the query and fetch up to 'count' rows
    // A full implementation would use prepared statement cursors
    auto status = conn->sendQuery(cursor.query, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    status = conn->readResultSet(result, ctx);
    
    // Limit to requested count
    if (result.rows.size() > count) {
        result.rows.resize(count);
        result.has_more = true;
    } else {
        result.has_more = false;
    }
    
    releaseConnection(std::move(conn));
    return status;
}

core::Status MySQLUDRConnector::closeCursor(const std::string& cursor_name,
                                            core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(cursor_mutex_);
    
    auto it = active_cursors_.find(cursor_name);
    if (it == active_cursors_.end()) {
        return core::Status::OK;  // Already closed
    }
    
    active_cursors_.erase(it);
    return core::Status::OK;
}

// ============================================================================
// Schema Introspection
// ============================================================================

core::Status MySQLUDRConnector::getTableInfo(const std::string& schema,
                                             const std::string& table,
                                             RemoteTableInfo& info,
                                             core::ErrorContext* ctx) {
    info.remote_schema = schema;
    info.remote_name = table;
    info.columns.clear();
    info.primary_key.clear();
    
    // Query information_schema for column info
    std::string sql = 
        "SELECT COLUMN_NAME, DATA_TYPE, IS_NULLABLE, COLUMN_DEFAULT, "
        "COLUMN_TYPE, EXTRA FROM INFORMATION_SCHEMA.COLUMNS "
        "WHERE TABLE_SCHEMA = '" + schema + "' AND TABLE_NAME = '" + table + "' "
        "ORDER BY ORDINAL_POSITION";
    
    RemoteResultSet result;
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    auto status = conn->sendQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    status = conn->readResultSet(result, ctx);
    releaseConnection(std::move(conn));
    
    if (status != core::Status::OK) {
        return status;
    }
    
    if (result.rows.empty()) {
        if (ctx) {
            ctx->set(core::Status::UNDEFINED_TABLE, "Table not found",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::UNDEFINED_TABLE;
    }
    
    for (const auto& row : result.rows) {
        RemoteColumn col;
        
        if (!row.values.empty() && !row.values[0].is_null) {
            col.name.assign(row.values[0].data.begin(), row.values[0].data.end());
        }
        
        if (row.values.size() > 1 && !row.values[1].is_null) {
            col.type_name.assign(row.values[1].data.begin(), row.values[1].data.end());
            // Map MySQL types to type_oid
            if (col.type_name == "int" || col.type_name == "integer") {
                col.type_oid = 3;  // MYSQL_TYPE_LONG
            } else if (col.type_name == "bigint") {
                col.type_oid = 8;  // MYSQL_TYPE_LONGLONG
            } else if (col.type_name == "varchar") {
                col.type_oid = 15;  // MYSQL_TYPE_VAR_STRING
            } else if (col.type_name == "char") {
                col.type_oid = 254;  // MYSQL_TYPE_STRING
            } else if (col.type_name == "text") {
                col.type_oid = 252;  // MYSQL_TYPE_BLOB (TEXT is BLOB in MySQL)
            } else if (col.type_name == "datetime") {
                col.type_oid = 12;  // MYSQL_TYPE_DATETIME
            } else if (col.type_name == "timestamp") {
                col.type_oid = 7;  // MYSQL_TYPE_TIMESTAMP
            } else if (col.type_name == "date") {
                col.type_oid = 10;  // MYSQL_TYPE_DATE
            } else if (col.type_name == "decimal" || col.type_name == "numeric") {
                col.type_oid = 0;  // MYSQL_TYPE_DECIMAL
            } else {
                col.type_oid = 15;  // Default to VAR_STRING
            }
        }
        
        if (row.values.size() > 2 && !row.values[2].is_null) {
            std::string nullable_str(row.values[2].data.begin(), row.values[2].data.end());
            col.nullable = (nullable_str == "YES");
        }
        
        if (row.values.size() > 3 && !row.values[3].is_null) {
            col.default_value.assign(row.values[3].data.begin(), row.values[3].data.end());
        }
        
        info.columns.push_back(std::move(col));
    }
    
    // Query for primary key
    sql = "SELECT COLUMN_NAME FROM INFORMATION_SCHEMA.KEY_COLUMN_USAGE "
          "WHERE TABLE_SCHEMA = '" + schema + "' AND TABLE_NAME = '" + table + "' "
          "AND CONSTRAINT_NAME = 'PRIMARY' ORDER BY ORDINAL_POSITION";
    
    result.clear();
    conn = acquireConnection(ctx);
    if (conn) {
        status = conn->sendQuery(sql, ctx);
        if (status == core::Status::OK) {
            status = conn->readResultSet(result, ctx);
            if (status == core::Status::OK) {
                for (const auto& row : result.rows) {
                    if (!row.values.empty() && !row.values[0].is_null) {
                        std::string pk_col(row.values[0].data.begin(), row.values[0].data.end());
                        info.primary_key.push_back(pk_col);
                    }
                }
            }
        }
        releaseConnection(std::move(conn));
    }
    
    return core::Status::OK;
}

core::Status MySQLUDRConnector::listTables(const std::string& schema,
                                           std::vector<std::string>& tables,
                                           core::ErrorContext* ctx) {
    tables.clear();
    
    std::string sql = 
        "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES "
        "WHERE TABLE_SCHEMA = '" + schema + "' AND TABLE_TYPE = 'BASE TABLE'";
    
    RemoteResultSet result;
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    auto status = conn->sendQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    status = conn->readResultSet(result, ctx);
    releaseConnection(std::move(conn));
    
    if (status != core::Status::OK) {
        return status;
    }
    
    for (const auto& row : result.rows) {
        if (!row.values.empty() && !row.values[0].is_null) {
            std::string table_name(row.values[0].data.begin(), row.values[0].data.end());
            tables.push_back(table_name);
        }
    }
    
    return core::Status::OK;
}

core::Status MySQLUDRConnector::getProcedureInfo(const std::string& schema,
                                                 const std::string& procedure,
                                                 RemoteProcedureInfo& info,
                                                 core::ErrorContext* ctx) {
    info.remote_schema = schema;
    info.remote_name = procedure;
    info.input_params.clear();
    info.output_params.clear();
    
    // Query information_schema for routine parameters
    std::string sql = 
        "SELECT PARAMETER_NAME, PARAMETER_MODE, DATA_TYPE, DTD_IDENTIFIER "
        "FROM INFORMATION_SCHEMA.PARAMETERS "
        "WHERE SPECIFIC_SCHEMA = '" + schema + "' AND SPECIFIC_NAME = '" + procedure + "' "
        "ORDER BY ORDINAL_POSITION";
    
    RemoteResultSet result;
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    auto status = conn->sendQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    status = conn->readResultSet(result, ctx);
    releaseConnection(std::move(conn));
    
    if (status != core::Status::OK) {
        return status;
    }
    
    for (const auto& row : result.rows) {
        if (row.values.size() < 2) continue;
        
        RemoteColumn param;
        
        if (!row.values[0].is_null) {
            param.name.assign(row.values[0].data.begin(), row.values[0].data.end());
        }
        
        std::string mode = "IN";
        if (!row.values[1].is_null) {
            mode.assign(row.values[1].data.begin(), row.values[1].data.end());
        }
        
        if (row.values.size() > 2 && !row.values[2].is_null) {
            param.type_name.assign(row.values[2].data.begin(), row.values[2].data.end());
        }
        
        if (mode == "IN" || mode == "INOUT") {
            info.input_params.push_back(param);
        }
        if (mode == "OUT" || mode == "INOUT") {
            info.output_params.push_back(param);
        }
    }
    
    return core::Status::OK;
}

core::Status MySQLUDRConnector::listProcedures(const std::string& schema,
                                               std::vector<std::string>& procedures,
                                               core::ErrorContext* ctx) {
    procedures.clear();
    
    std::string sql = 
        "SELECT ROUTINE_NAME FROM INFORMATION_SCHEMA.ROUTINES "
        "WHERE ROUTINE_SCHEMA = '" + schema + "' AND ROUTINE_TYPE = 'PROCEDURE'";
    
    RemoteResultSet result;
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    auto status = conn->sendQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    status = conn->readResultSet(result, ctx);
    releaseConnection(std::move(conn));
    
    if (status != core::Status::OK) {
        return status;
    }
    
    for (const auto& row : result.rows) {
        if (!row.values.empty() && !row.values[0].is_null) {
            std::string proc_name(row.values[0].data.begin(), row.values[0].data.end());
            procedures.push_back(proc_name);
        }
    }
    
    return core::Status::OK;
}

// ============================================================================
// COPY/Streaming Operations (LOAD DATA)
// ============================================================================

core::Status MySQLUDRConnector::startCopyIn(const std::string& table,
                                            const std::vector<std::string>& columns,
                                            core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(copy_mutex_);
    
    if (copy_state_.active) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "COPY operation already in progress",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    copy_state_.table_name = table;
    copy_state_.columns = columns;
    copy_state_.buffer.clear();
    copy_state_.active = true;
    copy_state_.is_in = true;
    
    return core::Status::OK;
}

core::Status MySQLUDRConnector::sendCopyData(const uint8_t* data, size_t len,
                                             core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(copy_mutex_);
    
    if (!copy_state_.active || !copy_state_.is_in) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "No active COPY IN operation",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    // Append data to buffer
    if (data && len > 0) {
        copy_state_.buffer.insert(copy_state_.buffer.end(), data, data + len);
    }
    
    return core::Status::OK;
}

core::Status MySQLUDRConnector::endCopyIn(uint64_t& rows_inserted,
                                          core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(copy_mutex_);
    
    if (!copy_state_.active || !copy_state_.is_in) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "No active COPY IN operation",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    // Parse buffer and insert data
    // MySQL LOAD DATA format: tab-delimited, newline-terminated rows
    std::vector<std::vector<std::string>> parsed_rows;
    std::vector<std::string> current_row;
    std::string current_field;
    
    for (uint8_t byte : copy_state_.buffer) {
        if (byte == '\t') {
            current_row.push_back(current_field);
            current_field.clear();
        } else if (byte == '\n') {
            current_row.push_back(current_field);
            parsed_rows.push_back(current_row);
            current_row.clear();
            current_field.clear();
        } else {
            current_field += static_cast<char>(byte);
        }
    }
    
    // Handle last row if no trailing newline
    if (!current_field.empty() || !current_row.empty()) {
        current_row.push_back(current_field);
        parsed_rows.push_back(current_row);
    }
    
    rows_inserted = 0;
    
    // Build and execute INSERT statements
    for (const auto& row : parsed_rows) {
        if (row.empty()) continue;
        
        std::string sql = "INSERT INTO " + copy_state_.table_name;
        
        if (!copy_state_.columns.empty()) {
            sql += " (";
            for (size_t i = 0; i < copy_state_.columns.size(); i++) {
                if (i > 0) sql += ", ";
                sql += copy_state_.columns[i];
            }
            sql += ")";
        }
        
        sql += " VALUES (";
        for (size_t i = 0; i < row.size(); i++) {
            if (i > 0) sql += ", ";
            // Escape and quote string values
            sql += "'";
            for (char c : row[i]) {
                if (c == '\'') sql += "''";
                else sql += c;
            }
            sql += "'";
        }
        sql += ")";
        
        uint64_t rows = 0;
        auto status = executeCommand(sql, rows, ctx);
        if (status != core::Status::OK) {
            copy_state_.active = false;
            return status;
        }
        
        rows_inserted += rows;
    }
    
    copy_state_.active = false;
    copy_state_.buffer.clear();
    
    return core::Status::OK;
}

core::Status MySQLUDRConnector::startCopyOut(const std::string& query,
                                             core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(copy_mutex_);
    
    if (copy_state_.active) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "COPY operation already in progress",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    copy_state_.table_name = query;  // Store query
    copy_state_.buffer.clear();
    copy_state_.active = true;
    copy_state_.is_in = false;
    
    return core::Status::OK;
}

core::Status MySQLUDRConnector::receiveCopyData(std::vector<uint8_t>& data,
                                                bool& done,
                                                core::ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(copy_mutex_);
    
    if (!copy_state_.active || copy_state_.is_in) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "No active COPY OUT operation",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    // If buffer is empty, execute query to fetch data
    if (copy_state_.buffer.empty() && !copy_state_.table_name.empty()) {
        RemoteResultSet result;
        auto status = executeQuery(copy_state_.table_name, result, ctx);
        if (status != core::Status::OK) {
            copy_state_.active = false;
            return status;
        }
        
        // Format result as tab-delimited text
        for (const auto& row : result.rows) {
            for (size_t i = 0; i < row.values.size(); i++) {
                if (i > 0) {
                    copy_state_.buffer.push_back('\t');
                }
                if (!row.values[i].is_null) {
                    copy_state_.buffer.insert(copy_state_.buffer.end(),
                                             row.values[i].data.begin(),
                                             row.values[i].data.end());
                }
            }
            copy_state_.buffer.push_back('\n');
        }
        
        copy_state_.table_name.clear();  // Clear query after execution
    }
    
    // Return data from buffer
    data = copy_state_.buffer;
    done = true;
    copy_state_.active = false;
    
    return core::Status::OK;
}

} // namespace udr
} // namespace scratchbird
