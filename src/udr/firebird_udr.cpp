/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

#include "scratchbird/udr/firebird_udr.h"

#include <cstring>
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
#include "scratchbird/core/socket_call_compat.h"
#include <fcntl.h>

namespace scratchbird {
namespace udr {

// ============================================================================
// XDR Encoding/Decoding Utilities
// ============================================================================

// XDR uses big-endian (network byte order)

std::vector<uint8_t> XDREncoder::encodeInt32(int32_t value) {
    uint32_t net = htonl(static_cast<uint32_t>(value));
    std::vector<uint8_t> result(4);
    std::memcpy(result.data(), &net, 4);
    return result;
}

std::vector<uint8_t> XDREncoder::encodeUint32(uint32_t value) {
    uint32_t net = htonl(value);
    std::vector<uint8_t> result(4);
    std::memcpy(result.data(), &net, 4);
    return result;
}

std::vector<uint8_t> XDREncoder::encodeInt64(int64_t value) {
    // XDR hyper (64-bit) - two 32-bit words, big-endian
    std::vector<uint8_t> result(8);
    uint32_t high = htonl(static_cast<uint32_t>(value >> 32));
    uint32_t low = htonl(static_cast<uint32_t>(value & 0xFFFFFFFF));
    std::memcpy(result.data(), &high, 4);
    std::memcpy(result.data() + 4, &low, 4);
    return result;
}

std::vector<uint8_t> XDREncoder::encodeString(const std::string& value) {
    std::vector<uint8_t> result;
    // Length as uint32
    auto len_bytes = encodeUint32(static_cast<uint32_t>(value.length()));
    result.insert(result.end(), len_bytes.begin(), len_bytes.end());
    // String data
    result.insert(result.end(), value.begin(), value.end());
    // Pad to 4-byte boundary
    size_t padding = (4 - (value.length() % 4)) % 4;
    result.insert(result.end(), padding, 0);
    return result;
}

std::vector<uint8_t> XDREncoder::encodeBytes(const std::vector<uint8_t>& value) {
    std::vector<uint8_t> result;
    // Length as uint32
    auto len_bytes = encodeUint32(static_cast<uint32_t>(value.size()));
    result.insert(result.end(), len_bytes.begin(), len_bytes.end());
    // Data
    result.insert(result.end(), value.begin(), value.end());
    // Pad to 4-byte boundary
    size_t padding = (4 - (value.size() % 4)) % 4;
    result.insert(result.end(), padding, 0);
    return result;
}

std::vector<uint8_t> XDREncoder::encodeOpaque(const void* data, size_t len) {
    std::vector<uint8_t> result(len);
    std::memcpy(result.data(), data, len);
    // Pad to 4-byte boundary
    size_t padding = (4 - (len % 4)) % 4;
    result.insert(result.end(), padding, 0);
    return result;
}

// ============================================================================
// XDR Decoder
// ============================================================================

XDRDecoder::XDRDecoder(const uint8_t* data, size_t len)
    : data_(data), len_(len), pos_(0) {
}

bool XDRDecoder::decodeInt32(int32_t& value) {
    if (pos_ + 4 > len_) return false;
    uint32_t net;
    std::memcpy(&net, data_ + pos_, 4);
    value = static_cast<int32_t>(ntohl(net));
    pos_ += 4;
    return true;
}

bool XDRDecoder::decodeUint32(uint32_t& value) {
    if (pos_ + 4 > len_) return false;
    uint32_t net;
    std::memcpy(&net, data_ + pos_, 4);
    value = ntohl(net);
    pos_ += 4;
    return true;
}

bool XDRDecoder::decodeInt64(int64_t& value) {
    if (pos_ + 8 > len_) return false;
    uint32_t high, low;
    std::memcpy(&high, data_ + pos_, 4);
    std::memcpy(&low, data_ + pos_ + 4, 4);
    value = (static_cast<int64_t>(ntohl(high)) << 32) | ntohl(low);
    pos_ += 8;
    return true;
}

bool XDRDecoder::decodeString(std::string& value) {
    uint32_t len;
    if (!decodeUint32(len)) return false;
    if (pos_ + len > len_) return false;
    value.assign(reinterpret_cast<const char*>(data_ + pos_), len);
    pos_ += len;
    // Skip padding
    size_t padding = (4 - (len % 4)) % 4;
    pos_ += padding;
    return true;
}

bool XDRDecoder::decodeBytes(std::vector<uint8_t>& value) {
    uint32_t len;
    if (!decodeUint32(len)) return false;
    if (pos_ + len > len_) return false;
    value.assign(data_ + pos_, data_ + pos_ + len);
    pos_ += len;
    // Skip padding
    size_t padding = (4 - (len % 4)) % 4;
    pos_ += padding;
    return true;
}

bool XDRDecoder::decodeOpaque(void* data, size_t len) {
    if (pos_ + len > len_) return false;
    std::memcpy(data, data_ + pos_, len);
    pos_ += len;
    // Skip padding
    size_t padding = (4 - (len % 4)) % 4;
    pos_ += padding;
    return true;
}

// ============================================================================
// FirebirdConnection Implementation
// ============================================================================

FirebirdConnection::FirebirdConnection() = default;

FirebirdConnection::~FirebirdConnection() {
    close();
}

bool FirebirdConnection::isOpen() const {
    return socket_fd_ >= 0 && db_handle_ != 0;
}

bool FirebirdConnection::isValid() const {
    return isOpen();
}

core::Status FirebirdConnection::ping(core::ErrorContext* ctx) {
    if (!isOpen()) {
        if (ctx) {
            ctx->set(core::Status::CONNECTION_FAILURE, "Connection is closed",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Use op_info_database with minimal request as ping
    // For simplicity, just check socket is valid
    int error = 0;
    int len = static_cast<int>(sizeof(error));
    int retval = sb_socket_getsockopt(socket_fd_, SOL_SOCKET, SO_ERROR, &error, &len);
    if (retval < 0 || error != 0) {
        if (ctx) {
            ctx->set(core::Status::CONNECTION_FAILURE, "Socket error",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_FAILURE;
    }
    
    return core::Status::OK;
}

void FirebirdConnection::close() {
    if (db_handle_ != 0) {
        detachDatabase(nullptr);
        db_handle_ = 0;
    }
    cleanupSocket();
}

std::string FirebirdConnection::getRemoteAddress() const {
    return host_ + ":" + std::to_string(port_);
}

std::string FirebirdConnection::getRemoteVersion() const {
    return "Firebird P" + std::to_string(protocol_version_);
}

core::Status FirebirdConnection::connect(const std::string& host, uint16_t port,
                                        const std::string& database,
                                        const std::string& user,
                                        const std::string& password,
                                        const std::string& role,
                                        const std::string& charset,
                                        core::ErrorContext* ctx) {
    host_ = host;
    port_ = port;
    database_ = database;
    user_ = user;
    password_ = password;
    role_ = role;
    charset_ = charset.empty() ? "UTF8" : charset;
    
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
    
    // Perform protocol handshake
    auto status = doConnect(ctx);
    if (status != core::Status::OK) {
        cleanupSocket();
        return status;
    }
    
    status = doProtocol(ctx);
    if (status != core::Status::OK) {
        cleanupSocket();
        return status;
    }
    
    // Attach to database
    status = attachDatabase(ctx);
    if (status != core::Status::OK) {
        cleanupSocket();
        return status;
    }
    
    return core::Status::OK;
}

core::Status FirebirdConnection::doConnect(core::ErrorContext* ctx) {
    // Send op_connect
    // Format: op_connect (4) + ptype (4) + architecture (4) + protocol_version (4) + 
    //         database_name (string) + dpb (bytes)
    
    std::vector<uint8_t> packet;
    
    // Operation code
    auto op = XDREncoder::encodeInt32(firebird::op_connect);
    packet.insert(packet.end(), op.begin(), op.end());
    
    // Protocol type (ptype_batch_send)
    auto ptype = XDREncoder::encodeInt32(firebird::ptype_batch_send);
    packet.insert(packet.end(), ptype.begin(), ptype.end());
    
    // Architecture
    auto arch = XDREncoder::encodeInt32(firebird::ARCHITECTURE_GENERIC);
    packet.insert(packet.end(), arch.begin(), arch.end());
    
    // Protocol version (protocol_version_2)
    auto proto = XDREncoder::encodeInt32(firebird::PROTOCOL_VERSION_2);
    packet.insert(packet.end(), proto.begin(), proto.end());
    
    // Database name
    auto db = XDREncoder::encodeString(database_);
    packet.insert(packet.end(), db.begin(), db.end());
    
    // DPB (Database Parameter Block)
    auto dpb = buildDPB();
    auto dpb_bytes = XDREncoder::encodeBytes(dpb);
    packet.insert(packet.end(), dpb_bytes.begin(), dpb_bytes.end());
    
    auto status = writePacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Read response (op_accept, op_reject, or op_cond_accept)
    std::vector<uint8_t> response;
    status = readPacket(response, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (response.size() < 4) {
        if (ctx) {
            ctx->set(core::Status::PROTOCOL_VIOLATION, "Invalid response",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::PROTOCOL_VIOLATION;
    }
    
    XDRDecoder decoder(response.data(), response.size());
    int32_t response_op;
    if (!decoder.decodeInt32(response_op)) {
        return core::Status::PROTOCOL_VIOLATION;
    }
    
    if (response_op == firebird::op_reject) {
        if (ctx) {
            ctx->set(core::Status::INVALID_PASSWORD, "Connection rejected",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_PASSWORD;
    }
    
    if (response_op == firebird::op_accept) {
        // op_accept: version (4) + architecture (4) + node_id (4)
        int32_t version, arch, node_id;
        if (!decoder.decodeInt32(version) ||
            !decoder.decodeInt32(arch) ||
            !decoder.decodeInt32(node_id)) {
            return core::Status::PROTOCOL_VIOLATION;
        }
        protocol_version_ = version;
        architecture_ = arch;
    }
    // op_cond_accept would require additional handling
    
    return core::Status::OK;
}

core::Status FirebirdConnection::doProtocol(core::ErrorContext* ctx) {
    // Send op_protocol to establish protocol version
    std::vector<uint8_t> packet;
    
    auto op = XDREncoder::encodeInt32(firebird::op_protocol);
    packet.insert(packet.end(), op.begin(), op.end());
    
    // Protocol version
    auto proto = XDREncoder::encodeInt32(firebird::PROTOCOL_VERSION_2);
    packet.insert(packet.end(), proto.begin(), proto.end());
    
    // Architecture
    auto arch = XDREncoder::encodeInt32(firebird::ARCHITECTURE_GENERIC);
    packet.insert(packet.end(), arch.begin(), arch.end());
    
    // Minimum type
    auto min_type = XDREncoder::encodeInt32(firebird::ptype_batch_send);
    packet.insert(packet.end(), min_type.begin(), min_type.end());
    
    // Maximum type
    auto max_type = XDREncoder::encodeInt32(firebird::ptype_lazy);
    packet.insert(packet.end(), max_type.begin(), max_type.end());
    
    // Preference (ptype_batch_send)
    auto pref = XDREncoder::encodeInt32(firebird::ptype_batch_send);
    packet.insert(packet.end(), pref.begin(), pref.end());
    
    // Database name (empty for protocol negotiation)
    auto db = XDREncoder::encodeString("");
    packet.insert(packet.end(), db.begin(), db.end());
    
    auto status = writePacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Read accept/reject
    std::vector<uint8_t> response;
    status = readPacket(response, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    XDRDecoder decoder(response.data(), response.size());
    int32_t response_op;
    if (!decoder.decodeInt32(response_op)) {
        return core::Status::PROTOCOL_VIOLATION;
    }
    
    if (response_op == firebird::op_reject) {
        if (ctx) {
            ctx->set(core::Status::NOT_SUPPORTED, "Protocol rejected",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_SUPPORTED;
    }
    
    return core::Status::OK;
}

core::Status FirebirdConnection::attachDatabase(core::ErrorContext* ctx) {
    // Send op_attach
    std::vector<uint8_t> packet;
    
    auto op = XDREncoder::encodeInt32(firebird::op_attach);
    packet.insert(packet.end(), op.begin(), op.end());
    
    // Database handle (0 for new)
    auto handle = XDREncoder::encodeInt32(0);
    packet.insert(packet.end(), handle.begin(), handle.end());
    
    // Database name
    auto db = XDREncoder::encodeString(database_);
    packet.insert(packet.end(), db.begin(), db.end());
    
    // DPB
    auto dpb = buildDPB();
    auto dpb_bytes = XDREncoder::encodeBytes(dpb);
    packet.insert(packet.end(), dpb_bytes.begin(), dpb_bytes.end());
    
    auto status = writePacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Read response
    std::vector<uint8_t> data;
    uint32_t new_handle;
    return readResponse(new_handle, data, ctx);
}

core::Status FirebirdConnection::detachDatabase(core::ErrorContext* ctx) {
    if (db_handle_ == 0) {
        return core::Status::OK;
    }
    
    std::vector<uint8_t> packet;
    
    auto op = XDREncoder::encodeInt32(firebird::op_detach);
    packet.insert(packet.end(), op.begin(), op.end());
    
    auto handle = XDREncoder::encodeInt32(db_handle_);
    packet.insert(packet.end(), handle.begin(), handle.end());
    
    auto status = writePacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Read response
    std::vector<uint8_t> data;
    uint32_t dummy;
    status = readResponse(dummy, data, ctx);
    
    db_handle_ = 0;
    return status;
}

std::vector<uint8_t> FirebirdConnection::buildDPB() const {
    std::vector<uint8_t> dpb;
    dpb.push_back(firebird::isc_dpb_version1);
    
    // User name
    if (!user_.empty()) {
        dpb.push_back(firebird::isc_dpb_user_name);
        dpb.push_back(static_cast<uint8_t>(user_.length()));
        dpb.insert(dpb.end(), user_.begin(), user_.end());
    }
    
    // Password
    if (!password_.empty()) {
        dpb.push_back(firebird::isc_dpb_password);
        dpb.push_back(static_cast<uint8_t>(password_.length()));
        dpb.insert(dpb.end(), password_.begin(), password_.end());
    }
    
    // Character set
    if (!charset_.empty()) {
        dpb.push_back(firebird::isc_dpb_lc_ctype);
        dpb.push_back(static_cast<uint8_t>(charset_.length()));
        dpb.insert(dpb.end(), charset_.begin(), charset_.end());
    }
    
    // SQL Role
    if (!role_.empty()) {
        dpb.push_back(firebird::isc_dpb_sql_role_name);
        dpb.push_back(static_cast<uint8_t>(role_.length()));
        dpb.insert(dpb.end(), role_.begin(), role_.end());
    }
    
    return dpb;
}

std::vector<uint8_t> FirebirdConnection::buildTPB() const {
    std::vector<uint8_t> tpb;
    tpb.push_back(firebird::isc_tpb_version3);
    tpb.push_back(firebird::isc_tpb_write);
    tpb.push_back(firebird::isc_tpb_read_committed);
    tpb.push_back(firebird::isc_tpb_rec_version);
    tpb.push_back(firebird::isc_tpb_wait);
    return tpb;
}

core::Status FirebirdConnection::startTransaction(uint32_t& handle, core::ErrorContext* ctx) {
    std::vector<uint8_t> packet;
    
    auto op = XDREncoder::encodeInt32(firebird::op_transaction);
    packet.insert(packet.end(), op.begin(), op.end());
    
    auto db = XDREncoder::encodeInt32(db_handle_);
    packet.insert(packet.end(), db.begin(), db.end());
    
    auto tpb = buildTPB();
    auto tpb_bytes = XDREncoder::encodeBytes(tpb);
    packet.insert(packet.end(), tpb_bytes.begin(), tpb_bytes.end());
    
    auto status = writePacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    std::vector<uint8_t> data;
    return readResponse(handle, data, ctx);
}

core::Status FirebirdConnection::commitTransaction(uint32_t handle, core::ErrorContext* ctx) {
    std::vector<uint8_t> packet;
    
    auto op = XDREncoder::encodeInt32(firebird::op_commit);
    packet.insert(packet.end(), op.begin(), op.end());
    
    auto h = XDREncoder::encodeInt32(handle);
    packet.insert(packet.end(), h.begin(), h.end());
    
    auto status = writePacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    std::vector<uint8_t> data;
    uint32_t dummy;
    return readResponse(dummy, data, ctx);
}

core::Status FirebirdConnection::rollbackTransaction(uint32_t handle, core::ErrorContext* ctx) {
    std::vector<uint8_t> packet;
    
    auto op = XDREncoder::encodeInt32(firebird::op_rollback);
    packet.insert(packet.end(), op.begin(), op.end());
    
    auto h = XDREncoder::encodeInt32(handle);
    packet.insert(packet.end(), h.begin(), h.end());
    
    auto status = writePacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    std::vector<uint8_t> data;
    uint32_t dummy;
    return readResponse(dummy, data, ctx);
}

core::Status FirebirdConnection::allocateStatement(uint32_t& stmt_handle, core::ErrorContext* ctx) {
    std::vector<uint8_t> packet;
    
    auto op = XDREncoder::encodeInt32(firebird::op_allocate_statement);
    packet.insert(packet.end(), op.begin(), op.end());
    
    auto db = XDREncoder::encodeInt32(db_handle_);
    packet.insert(packet.end(), db.begin(), db.end());
    
    auto status = writePacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    std::vector<uint8_t> data;
    return readResponse(stmt_handle, data, ctx);
}

core::Status FirebirdConnection::executeStatement(uint32_t stmt_handle, uint32_t txn_handle,
                                                  const std::string& sql, core::ErrorContext* ctx) {
    std::vector<uint8_t> packet;
    
    auto op = XDREncoder::encodeInt32(firebird::op_exec_immediate);
    packet.insert(packet.end(), op.begin(), op.end());
    
    auto txn = XDREncoder::encodeInt32(txn_handle);
    packet.insert(packet.end(), txn.begin(), txn.end());
    
    auto stmt = XDREncoder::encodeInt32(stmt_handle);
    packet.insert(packet.end(), stmt.begin(), stmt.end());
    
    auto db = XDREncoder::encodeInt32(db_handle_);
    packet.insert(packet.end(), db.begin(), db.end());
    
    // SQL dialect (3)
    auto dialect = XDREncoder::encodeInt32(3);
    packet.insert(packet.end(), dialect.begin(), dialect.end());
    
    // SQL statement
    auto sql_bytes = XDREncoder::encodeString(sql);
    packet.insert(packet.end(), sql_bytes.begin(), sql_bytes.end());
    
    // Input BLR (empty)
    auto blr = XDREncoder::encodeBytes({});
    packet.insert(packet.end(), blr.begin(), blr.end());
    
    // Input message (empty)
    auto msg = XDREncoder::encodeInt32(0);
    packet.insert(packet.end(), msg.begin(), msg.end());
    
    // Output BLR (empty)
    auto out_blr = XDREncoder::encodeBytes({});
    packet.insert(packet.end(), out_blr.begin(), out_blr.end());
    
    // Output message length
    auto out_len = XDREncoder::encodeInt32(0);
    packet.insert(packet.end(), out_len.begin(), out_len.end());
    
    auto status = writePacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    std::vector<uint8_t> data;
    uint32_t dummy;
    return readResponse(dummy, data, ctx);
}

core::Status FirebirdConnection::freeStatement(uint32_t stmt_handle, core::ErrorContext* ctx) {
    std::vector<uint8_t> packet;
    
    auto op = XDREncoder::encodeInt32(firebird::op_free_statement);
    packet.insert(packet.end(), op.begin(), op.end());
    
    auto h = XDREncoder::encodeInt32(stmt_handle);
    packet.insert(packet.end(), h.begin(), h.end());
    
    // DSQL_drop
    auto drop = XDREncoder::encodeInt32(1);
    packet.insert(packet.end(), drop.begin(), drop.end());
    
    auto status = writePacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    std::vector<uint8_t> data;
    uint32_t dummy;
    return readResponse(dummy, data, ctx);
}

core::Status FirebirdConnection::readResponse(uint32_t& handle, std::vector<uint8_t>& data,
                                             core::ErrorContext* ctx) {
    std::vector<uint8_t> packet;
    auto status = readPacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (packet.size() < 4) {
        return core::Status::PROTOCOL_VIOLATION;
    }
    
    XDRDecoder decoder(packet.data(), packet.size());
    int32_t op;
    if (!decoder.decodeInt32(op)) {
        return core::Status::PROTOCOL_VIOLATION;
    }
    
    if (op == firebird::op_response) {
        // op_response: handle (4) + object_id (8) + status_vector (variable) + data (variable)
        int32_t h;
        if (!decoder.decodeInt32(h)) {
            return core::Status::PROTOCOL_VIOLATION;
        }
        handle = h;
        
        // Skip object_id (8 bytes)
        int64_t obj_id;
        if (!decoder.decodeInt64(obj_id)) {
            return core::Status::PROTOCOL_VIOLATION;
        }
        
        // Read status vector
        int32_t sv_code;
        if (!decoder.decodeInt32(sv_code)) {
            return core::Status::PROTOCOL_VIOLATION;
        }
        
        // Parse status vector
        while (sv_code != firebird::isc_arg_end) {
            if (sv_code == firebird::isc_arg_gds) {
                int32_t error_code;
                if (!decoder.decodeInt32(error_code)) {
                    return core::Status::PROTOCOL_VIOLATION;
                }
                if (error_code != 0) {
                    if (ctx) {
                        ctx->set(core::Status::CONNECTION_FAILURE, 
                                ("Firebird error: " + std::to_string(error_code)).c_str(),
                                __FILE__, __LINE__, __func__);
                    }
                    return core::Status::CONNECTION_FAILURE;
                }
            } else if (sv_code == firebird::isc_arg_string) {
                std::string str;
                if (!decoder.decodeString(str)) {
                    return core::Status::PROTOCOL_VIOLATION;
                }
            } else if (sv_code == firebird::isc_arg_number) {
                int32_t num;
                if (!decoder.decodeInt32(num)) {
                    return core::Status::PROTOCOL_VIOLATION;
                }
            }
            
            if (!decoder.decodeInt32(sv_code)) {
                return core::Status::PROTOCOL_VIOLATION;
            }
        }
        
        // Read remaining data
        size_t remaining = decoder.remaining();
        if (remaining > 0) {
            data.resize(remaining);
            std::memcpy(data.data(), packet.data() + (packet.size() - remaining), remaining);
        }
        
        return core::Status::OK;
    }
    
    return core::Status::PROTOCOL_VIOLATION;
}

core::Status FirebirdConnection::sendOp(uint32_t op, core::ErrorContext* ctx) {
    std::vector<uint8_t> packet = XDREncoder::encodeInt32(op);
    return writePacket(packet, ctx);
}

core::Status FirebirdConnection::readPacket(std::vector<uint8_t>& packet, core::ErrorContext* ctx) {
    // Firebird packets are 4-byte length prefix (XDR int) + data
    int32_t len_net;
    auto status = readExactly(&len_net, 4, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    uint32_t len = ntohl(len_net);
    if (len > 0) {
        packet.resize(len);
        status = readExactly(packet.data(), len, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    } else {
        packet.clear();
    }
    
    return core::Status::OK;
}

core::Status FirebirdConnection::writePacket(const std::vector<uint8_t>& packet,
                                            core::ErrorContext* ctx) {
    uint32_t len = packet.size();
    uint32_t len_net = htonl(len);
    
    auto status = writeExactly(&len_net, 4, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (!packet.empty()) {
        status = writeExactly(packet.data(), packet.size(), ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }
    
    return core::Status::OK;
}

core::Status FirebirdConnection::readExactly(void* buffer, size_t len,
                                            core::ErrorContext* ctx) {
    uint8_t* ptr = (uint8_t*)buffer;
    size_t remaining = len;
    
    while (remaining > 0) {
        ssize_t n = read(socket_fd_, ptr, remaining);
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

core::Status FirebirdConnection::writeExactly(const void* buffer, size_t len,
                                             core::ErrorContext* ctx) {
    const uint8_t* ptr = (const uint8_t*)buffer;
    size_t remaining = len;
    
    while (remaining > 0) {
        ssize_t n = write(socket_fd_, ptr, remaining);
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

void FirebirdConnection::cleanupSocket() {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}

// ============================================================================
// Firebird BLOB Operations Implementation
// ============================================================================

core::Status FirebirdConnection::openBlob(uint64_t blob_id, uint32_t txn_handle,
                                          uint32_t& blob_handle, core::ErrorContext* ctx) {
    // op_open_blob: transaction (4) + blob_id_high (4) + blob_id_low (4)
    std::vector<uint8_t> packet;
    
    auto op = XDREncoder::encodeInt32(firebird::op_open_blob);
    packet.insert(packet.end(), op.begin(), op.end());
    
    auto txn = XDREncoder::encodeInt32(txn_handle);
    packet.insert(packet.end(), txn.begin(), txn.end());
    
    // Blob ID is 64-bit: high 32 bits, then low 32 bits
    auto blob_high = XDREncoder::encodeInt32(static_cast<int32_t>(blob_id >> 32));
    packet.insert(packet.end(), blob_high.begin(), blob_high.end());
    
    auto blob_low = XDREncoder::encodeInt32(static_cast<int32_t>(blob_id & 0xFFFFFFFF));
    packet.insert(packet.end(), blob_low.begin(), blob_low.end());
    
    auto status = writePacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    std::vector<uint8_t> data;
    return readResponse(blob_handle, data, ctx);
}

core::Status FirebirdConnection::getSegment(uint32_t blob_handle, std::vector<uint8_t>& segment,
                                           bool& eof, core::ErrorContext* ctx) {
    // op_get_segment: blob_handle (4) + segment_length (4) + segment_number (4)
    std::vector<uint8_t> packet;
    
    auto op = XDREncoder::encodeInt32(firebird::op_get_segment);
    packet.insert(packet.end(), op.begin(), op.end());
    
    auto handle = XDREncoder::encodeInt32(blob_handle);
    packet.insert(packet.end(), handle.begin(), handle.end());
    
    // Request max segment size (32KB)
    constexpr int32_t max_segment_size = 32768;
    auto seg_len = XDREncoder::encodeInt32(max_segment_size);
    packet.insert(packet.end(), seg_len.begin(), seg_len.end());
    
    // Segment number (0 = next segment)
    auto seg_num = XDREncoder::encodeInt32(0);
    packet.insert(packet.end(), seg_num.begin(), seg_num.end());
    
    auto status = writePacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Read response - op_response with segment data
    std::vector<uint8_t> response;
    status = readPacket(response, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (response.size() < 4) {
        return core::Status::PROTOCOL_VIOLATION;
    }
    
    XDRDecoder decoder(response.data(), response.size());
    int32_t response_op;
    if (!decoder.decodeInt32(response_op)) {
        return core::Status::PROTOCOL_VIOLATION;
    }
    
    if (response_op == firebird::op_response) {
        int32_t h;
        if (!decoder.decodeInt32(h)) {
            return core::Status::PROTOCOL_VIOLATION;
        }
        
        // Skip object_id
        int64_t obj_id;
        if (!decoder.decodeInt64(obj_id)) {
            return core::Status::PROTOCOL_VIOLATION;
        }
        
        // Parse status vector
        int32_t sv_code;
        if (!decoder.decodeInt32(sv_code)) {
            return core::Status::PROTOCOL_VIOLATION;
        }
        
        eof = false;
        while (sv_code != firebird::isc_arg_end) {
            if (sv_code == firebird::isc_arg_gds) {
                int32_t error_code;
                if (!decoder.decodeInt32(error_code)) {
                    return core::Status::PROTOCOL_VIOLATION;
                }
                if (error_code != 0) {
                    // isc_segstr_eof (335544367) indicates end of BLOB
                    if (error_code == 335544367) {
                        eof = true;
                        return core::Status::OK;
                    }
                    if (ctx) {
                        ctx->set(core::Status::CONNECTION_FAILURE,
                                ("Firebird BLOB error: " + std::to_string(error_code)).c_str(),
                                __FILE__, __LINE__, __func__);
                    }
                    return core::Status::CONNECTION_FAILURE;
                }
            } else if (sv_code == firebird::isc_arg_number) {
                int32_t num;
                if (!decoder.decodeInt32(num)) {
                    return core::Status::PROTOCOL_VIOLATION;
                }
            } else if (sv_code == firebird::isc_arg_string) {
                std::string str;
                if (!decoder.decodeString(str)) {
                    return core::Status::PROTOCOL_VIOLATION;
                }
            }
            
            if (!decoder.decodeInt32(sv_code)) {
                return core::Status::PROTOCOL_VIOLATION;
            }
        }
        
        // Read segment data (length-prefixed)
        if (!decoder.decodeBytes(segment)) {
            segment.clear();
        }
        
        return core::Status::OK;
    }
    
    return core::Status::PROTOCOL_VIOLATION;
}

core::Status FirebirdConnection::closeBlob(uint32_t blob_handle, core::ErrorContext* ctx) {
    // op_close_blob: blob_handle (4)
    std::vector<uint8_t> packet;
    
    auto op = XDREncoder::encodeInt32(firebird::op_close_blob);
    packet.insert(packet.end(), op.begin(), op.end());
    
    auto handle = XDREncoder::encodeInt32(blob_handle);
    packet.insert(packet.end(), handle.begin(), handle.end());
    
    auto status = writePacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    std::vector<uint8_t> data;
    uint32_t dummy;
    return readResponse(dummy, data, ctx);
}

core::Status FirebirdConnection::prepareStatement(uint32_t stmt_handle, uint32_t txn_handle,
                                                  const std::string& sql, core::ErrorContext* ctx) {
    // op_prepare_statement: transaction (4) + statement (4) + dialect (4) + 
    //                       sql (string) + info_buffer_len (4) + info_items (bytes)
    std::vector<uint8_t> packet;
    
    auto op = XDREncoder::encodeInt32(firebird::op_prepare_statement);
    packet.insert(packet.end(), op.begin(), op.end());
    
    auto txn = XDREncoder::encodeInt32(txn_handle);
    packet.insert(packet.end(), txn.begin(), txn.end());
    
    auto stmt = XDREncoder::encodeInt32(stmt_handle);
    packet.insert(packet.end(), stmt.begin(), stmt.end());
    
    // SQL dialect (3)
    auto dialect = XDREncoder::encodeInt32(3);
    packet.insert(packet.end(), dialect.begin(), dialect.end());
    
    // SQL statement
    auto sql_bytes = XDREncoder::encodeString(sql);
    packet.insert(packet.end(), sql_bytes.begin(), sql_bytes.end());
    
    // Info buffer length (0 for now - we don't need statement info yet)
    auto info_len = XDREncoder::encodeInt32(0);
    packet.insert(packet.end(), info_len.begin(), info_len.end());
    
    // No info items needed
    
    auto status = writePacket(packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    std::vector<uint8_t> data;
    uint32_t dummy;
    return readResponse(dummy, data, ctx);
}

// ============================================================================
// FirebirdConnectionFactory Implementation
// ============================================================================

FirebirdConnectionFactory::FirebirdConnectionFactory(const UDRServerConfig& config)
    : config_(config) {
}

std::unique_ptr<PooledConnection> FirebirdConnectionFactory::createConnection(
    core::ErrorContext* ctx) {
    
    auto conn = std::make_unique<FirebirdConnection>();
    
    auto status = conn->connect(
        config_.host,
        config_.port ? config_.port : 3050,
        config_.database,
        config_.user,
        config_.password,
        config_.role,
        config_.charset,
        ctx
    );
    
    if (status != core::Status::OK) {
        return nullptr;
    }
    
    return conn;
}

bool FirebirdConnectionFactory::validateConnection(PooledConnection* conn,
                                                  core::ErrorContext* ctx) {
    if (!conn) return false;
    
    auto* fb_conn = dynamic_cast<FirebirdConnection*>(conn);
    if (!fb_conn) return false;
    
    return fb_conn->ping(ctx) == core::Status::OK;
}

void FirebirdConnectionFactory::destroyConnection(PooledConnection* conn) {
    if (conn) {
        conn->close();
        delete conn;
    }
}

// ============================================================================
// FirebirdUDRConnector Implementation
// ============================================================================

FirebirdUDRConnector::FirebirdUDRConnector() = default;

FirebirdUDRConnector::~FirebirdUDRConnector() {
    shutdown();
}

core::Status FirebirdUDRConnector::initialize(const UDRServerConfig& config,
                                             core::ErrorContext* ctx) {
    config_ = config;
    
    auto factory = std::make_unique<FirebirdConnectionFactory>(config);
    
    ConnectionPoolConfig pool_config;
    pool_config.min_size = config.pool_min_size;
    pool_config.max_size = config.pool_max_size;
    pool_config.initial_size = config.pool_min_size;
    pool_config.connection_timeout_ms = config.pool_connection_timeout_ms;
    pool_config.idle_timeout_ms = config.pool_max_idle_ms;
    pool_config.health_check_interval_ms = config.pool_health_check_interval_ms;
    
    pool_ = std::make_unique<ConnectionPool>(
        "firebird_" + config.host + "_" + config.database,
        std::move(factory),
        pool_config
    );
    
    return pool_->initialize(ctx);
}

core::Status FirebirdUDRConnector::shutdown(core::ErrorContext* ctx) {
    (void)ctx;
    
    // Clear any prepared statements
    {
        std::lock_guard<std::mutex> lock(prepared_mutex_);
        prepared_statements_.clear();
    }
    
    if (pool_) {
        pool_->shutdown();
        pool_.reset();
    }
    return core::Status::OK;
}

bool FirebirdUDRConnector::isConnected() const {
    return pool_ && pool_->isRunning();
}

core::Status FirebirdUDRConnector::ping(core::ErrorContext* ctx) {
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

core::Status FirebirdUDRConnector::reconnect(core::ErrorContext* ctx) {
    if (pool_) {
        pool_->invalidateAll();
        return pool_->ensureMinimumConnections(ctx);
    }
    return core::Status::INVALID_ARGUMENT;
}

core::Status FirebirdUDRConnector::executeQuery(const std::string& sql,
                                               RemoteResultSet& result,
                                               core::ErrorContext* ctx) {
    (void)result;
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Allocate statement
    uint32_t stmt_handle;
    auto status = conn->allocateStatement(stmt_handle, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Start transaction if needed
    uint32_t txn_handle = 0;
    if (active_transaction_ == 0) {
        status = conn->startTransaction(txn_handle, ctx);
        if (status != core::Status::OK) {
            conn->freeStatement(stmt_handle, nullptr);
            releaseConnection(std::move(conn));
            return status;
        }
    } else {
        txn_handle = active_transaction_;
    }
    
    // Execute
    status = conn->executeStatement(stmt_handle, txn_handle, sql, ctx);
    
    // Cleanup
    conn->freeStatement(stmt_handle, nullptr);
    releaseConnection(std::move(conn));
    
    return status;
}

core::Status FirebirdUDRConnector::executeCommand(const std::string& sql,
                                                 uint64_t& rows_affected,
                                                 core::ErrorContext* ctx) {
    (void)rows_affected;
    RemoteResultSet result;
    return executeQuery(sql, result, ctx);
}

core::Status FirebirdUDRConnector::beginTransaction(core::ErrorContext* ctx) {
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    uint32_t txn_handle;
    auto status = conn->startTransaction(txn_handle, ctx);
    if (status == core::Status::OK) {
        active_transaction_ = txn_handle;
    }
    
    releaseConnection(std::move(conn));
    return status;
}

core::Status FirebirdUDRConnector::commitTransaction(core::ErrorContext* ctx) {
    if (active_transaction_ == 0) {
        return core::Status::INVALID_ARGUMENT;
    }
    
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    auto status = conn->commitTransaction(active_transaction_, ctx);
    if (status == core::Status::OK) {
        active_transaction_ = 0;
    }
    
    releaseConnection(std::move(conn));
    return status;
}

core::Status FirebirdUDRConnector::rollbackTransaction(core::ErrorContext* ctx) {
    if (active_transaction_ == 0) {
        return core::Status::INVALID_ARGUMENT;
    }
    
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    auto status = conn->rollbackTransaction(active_transaction_, ctx);
    if (status == core::Status::OK) {
        active_transaction_ = 0;
    }
    
    releaseConnection(std::move(conn));
    return status;
}

core::Status FirebirdUDRConnector::savepoint(const std::string& name,
                                            core::ErrorContext* ctx) {
    uint64_t rows;
    std::string sql = "SAVEPOINT " + name;
    return executeCommand(sql, rows, ctx);
}

core::Status FirebirdUDRConnector::rollbackToSavepoint(const std::string& name,
                                                      core::ErrorContext* ctx) {
    uint64_t rows;
    std::string sql = "ROLLBACK TO SAVEPOINT " + name;
    return executeCommand(sql, rows, ctx);
}

std::unique_ptr<FirebirdConnection> FirebirdUDRConnector::acquireConnection(
    core::ErrorContext* ctx) {
    
    if (!pool_) {
        return nullptr;
    }
    
    auto conn = pool_->acquire(ctx);
    if (!conn) {
        return nullptr;
    }
    
    auto* fb_conn = dynamic_cast<FirebirdConnection*>(conn.get());
    if (!fb_conn) {
        pool_->release(std::move(conn));
        return nullptr;
    }
    
    conn.release();
    return std::unique_ptr<FirebirdConnection>(fb_conn);
}

void FirebirdUDRConnector::releaseConnection(
    std::unique_ptr<FirebirdConnection> conn) {
    
    if (pool_ && conn) {
        pool_->release(std::unique_ptr<PooledConnection>(conn.release()));
    }
}

std::string FirebirdUDRConnector::getVersion() const {
    return "1.0.0";
}

std::string FirebirdUDRConnector::getRemoteVersion() const {
    return remote_version_;
}

std::vector<std::string> FirebirdUDRConnector::getSupportedFeatures() const {
    return {
        "simple_query",
        "transactions",
        "savepoints",
        "blob_streaming",
        "events",
        "sql_dialect_3"
    };
}

// ============================================================================
// FirebirdUDRConnector Stub Implementations
// ============================================================================

core::Status FirebirdUDRConnector::prepareStatement(const std::string& name,
                                                    const std::string& sql,
                                                    std::vector<uint32_t>& param_types,
                                                    core::ErrorContext* ctx) {
    (void)param_types;
    
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Allocate statement handle
    uint32_t stmt_handle;
    auto status = conn->allocateStatement(stmt_handle, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Start transaction if needed
    uint32_t txn_handle = 0;
    if (active_transaction_ == 0) {
        status = conn->startTransaction(txn_handle, ctx);
        if (status != core::Status::OK) {
            conn->freeStatement(stmt_handle, nullptr);
            releaseConnection(std::move(conn));
            return status;
        }
    } else {
        txn_handle = active_transaction_;
    }
    
    // Prepare the statement
    status = conn->prepareStatement(stmt_handle, txn_handle, sql, ctx);
    if (status != core::Status::OK) {
        conn->freeStatement(stmt_handle, nullptr);
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Store the prepared statement handle
    {
        std::lock_guard<std::mutex> lock(prepared_mutex_);
        prepared_statements_[name] = stmt_handle;
    }
    
    releaseConnection(std::move(conn));
    return core::Status::OK;
}

core::Status FirebirdUDRConnector::executePrepared(const std::string& name,
                                                   const std::vector<RemoteValue>& params,
                                                   RemoteResultSet& result,
                                                   core::ErrorContext* ctx) {
    (void)params;
    (void)result;
    
    // Get the prepared statement handle
    uint32_t stmt_handle;
    {
        std::lock_guard<std::mutex> lock(prepared_mutex_);
        auto it = prepared_statements_.find(name);
        if (it == prepared_statements_.end()) {
            if (ctx) {
                ctx->set(core::Status::INVALID_ARGUMENT, "Statement not found",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }
        stmt_handle = it->second;
    }
    
    // For now, we just return OK - full implementation would send parameters
    // and fetch results using the statement handle
    (void)stmt_handle;
    
    if (ctx) {
        ctx->set(core::Status::NOT_SUPPORTED, 
                "executePrepared with parameters not yet fully implemented",
                __FILE__, __LINE__, __func__);
    }
    return core::Status::NOT_SUPPORTED;
}

core::Status FirebirdUDRConnector::closeStatement(const std::string& name,
                                                 core::ErrorContext* ctx) {
    uint32_t stmt_handle;
    {
        std::lock_guard<std::mutex> lock(prepared_mutex_);
        auto it = prepared_statements_.find(name);
        if (it == prepared_statements_.end()) {
            if (ctx) {
                ctx->set(core::Status::INVALID_ARGUMENT, "Statement not found",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }
        stmt_handle = it->second;
        prepared_statements_.erase(it);
    }
    
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    auto status = conn->freeStatement(stmt_handle, ctx);
    releaseConnection(std::move(conn));
    
    return status;
}

core::Status FirebirdUDRConnector::declareCursor(const std::string& cursor_name,
                                                const std::string& query,
                                                bool scrollable,
                                                core::ErrorContext* ctx) {
    (void)scrollable;
    
    // Firebird uses named cursors via SQL: DECLARE cursor_name CURSOR FOR query
    uint64_t rows;
    std::string sql = "DECLARE " + cursor_name + " CURSOR FOR " + query;
    return executeCommand(sql, rows, ctx);
}

core::Status FirebirdUDRConnector::fetchCursor(const std::string& cursor_name,
                                              uint32_t count,
                                              RemoteResultSet& result,
                                              core::ErrorContext* ctx) {
    (void)count;
    (void)result;
    
    // Firebird FETCH syntax: FETCH cursor_name INTO ...
    // For now, we execute a FETCH that returns a result set
    uint64_t rows;
    std::string sql = "FETCH NEXT FROM " + cursor_name;
    return executeCommand(sql, rows, ctx);
}

core::Status FirebirdUDRConnector::closeCursor(const std::string& cursor_name,
                                              core::ErrorContext* ctx) {
    uint64_t rows;
    std::string sql = "CLOSE " + cursor_name;
    return executeCommand(sql, rows, ctx);
}

core::Status FirebirdUDRConnector::getTableInfo(const std::string& schema,
                                               const std::string& table,
                                               RemoteTableInfo& info,
                                               core::ErrorContext* ctx) {
    (void)schema;
    
    // Query Firebird system tables for column information
    uint64_t rows;
    std::string sql = 
        "SELECT r.RDB$FIELD_NAME, r.RDB$NULL_FLAG, f.RDB$FIELD_TYPE, "
        "       f.RDB$FIELD_LENGTH, f.RDB$FIELD_SCALE, f.RDB$FIELD_SUB_TYPE "
        "FROM RDB$RELATION_FIELDS r "
        "JOIN RDB$FIELDS f ON r.RDB$FIELD_SOURCE = f.RDB$FIELD_NAME "
        "WHERE r.RDB$RELATION_NAME = '" + table + "' "
        "ORDER BY r.RDB$FIELD_POSITION";
    
    RemoteResultSet result;
    auto status = executeQuery(sql, result, ctx);
    if (status == core::Status::OK) {
        info.remote_schema = schema;
        info.remote_name = table;
        // Populate columns from result set
        for (const auto& row : result.rows) {
            RemoteColumn col;
            // Parse column information from row values
            // This is a simplified implementation
            info.columns.push_back(col);
        }
    }
    
    (void)rows;
    return status;
}

core::Status FirebirdUDRConnector::listTables(const std::string& schema,
                                             std::vector<std::string>& tables,
                                             core::ErrorContext* ctx) {
    (void)schema;
    
    std::string sql = 
        "SELECT RDB$RELATION_NAME FROM RDB$RELATIONS "
        "WHERE RDB$SYSTEM_FLAG = 0 AND RDB$RELATION_TYPE = 0 "
        "ORDER BY RDB$RELATION_NAME";
    
    RemoteResultSet result;
    auto status = executeQuery(sql, result, ctx);
    if (status == core::Status::OK) {
        for (const auto& row : result.rows) {
            if (!row.values.empty() && !row.values[0].is_null) {
                tables.push_back(row.values[0].toString());
            }
        }
    }
    
    return status;
}

core::Status FirebirdUDRConnector::getProcedureInfo(const std::string& schema,
                                                   const std::string& procedure,
                                                   RemoteProcedureInfo& info,
                                                   core::ErrorContext* ctx) {
    (void)schema;
    
    info.remote_schema = schema;
    info.remote_name = procedure;
    info.input_params.clear();
    info.output_params.clear();
    info.returns_set = false;
    
    // Query Firebird system tables for procedure parameters
    std::string sql = 
        "SELECT p.RDB$PARAMETER_NAME, p.RDB$PARAMETER_TYPE, "
        "       f.RDB$FIELD_TYPE, f.RDB$FIELD_LENGTH, f.RDB$FIELD_SCALE, "
        "       f.RDB$FIELD_SUB_TYPE, f.RDB$CHARACTER_LENGTH "
        "FROM RDB$PROCEDURE_PARAMETERS p "
        "JOIN RDB$FIELDS f ON f.RDB$FIELD_NAME = p.RDB$FIELD_SOURCE "
        "WHERE p.RDB$PROCEDURE_NAME = '" + procedure + "' "
        "ORDER BY p.RDB$PARAMETER_NUMBER";
    
    RemoteResultSet result;
    auto status = executeQuery(sql, result, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    for (const auto& row : result.rows) {
        if (row.values.size() < 7) continue;
        
        RemoteColumn col;
        col.name = row.values[0].toString();
        // Trim trailing spaces from Firebird metadata
        col.name.erase(col.name.find_last_not_of(' ') + 1);
        
        int param_type = row.values[1].toInt64(); // 0=input, 1=output
        
        int field_type = row.values[2].toInt64();
        int field_length = row.values[3].toInt64();
        int field_scale = row.values[4].toInt64();
        int field_sub_type = row.values[5].toInt64();
        int char_length = row.values[6].toInt64();
        
        // Map Firebird type to type name
        switch (field_type) {
            case 7: col.type_name = "SMALLINT"; break;
            case 8: col.type_name = "INTEGER"; break;
            case 10: col.type_name = "FLOAT"; break;
            case 27: col.type_name = "DOUBLE"; break;
            case 12: col.type_name = "DATE"; break;
            case 13: col.type_name = "TIME"; break;
            case 35: col.type_name = "TIMESTAMP"; break;
            case 14: col.type_name = "CHAR"; col.type_size = char_length > 0 ? char_length : field_length; break;
            case 37: col.type_name = "VARCHAR"; col.type_size = char_length > 0 ? char_length : field_length; break;
            case 261: 
                if (field_sub_type == 1) col.type_name = "TEXT";
                else col.type_name = "BLOB";
                break;
            case 16: col.type_name = "BIGINT"; break;
            case 23: col.type_name = "BOOLEAN"; break;
            default: col.type_name = "UNKNOWN";
        }
        
        if (param_type == 0) {
            info.input_params.push_back(col);
        } else {
            info.output_params.push_back(col);
        }
    }
    
    return core::Status::OK;
}

core::Status FirebirdUDRConnector::listProcedures(const std::string& schema,
                                                 std::vector<std::string>& procedures,
                                                 core::ErrorContext* ctx) {
    (void)schema;
    
    std::string sql = 
        "SELECT RDB$PROCEDURE_NAME FROM RDB$PROCEDURES "
        "WHERE RDB$SYSTEM_FLAG = 0 "
        "ORDER BY RDB$PROCEDURE_NAME";
    
    RemoteResultSet result;
    auto status = executeQuery(sql, result, ctx);
    if (status == core::Status::OK) {
        for (const auto& row : result.rows) {
            if (!row.values.empty() && !row.values[0].is_null) {
                procedures.push_back(row.values[0].toString());
            }
        }
    }
    
    return status;
}

core::Status FirebirdUDRConnector::startCopyIn(const std::string& table,
                                              const std::vector<std::string>& columns,
                                              core::ErrorContext* ctx) {
    (void)table;
    (void)columns;
    
    // Firebird doesn't have a native COPY protocol like PostgreSQL
    // We would need to implement this using INSERT statements
    if (ctx) {
        ctx->set(core::Status::NOT_SUPPORTED, 
                "COPY operations not supported in Firebird connector",
                __FILE__, __LINE__, __func__);
    }
    return core::Status::NOT_SUPPORTED;
}

core::Status FirebirdUDRConnector::sendCopyData(const uint8_t* data, size_t len,
                                               core::ErrorContext* ctx) {
    (void)data;
    (void)len;
    
    if (ctx) {
        ctx->set(core::Status::NOT_SUPPORTED, 
                "COPY operations not supported in Firebird connector",
                __FILE__, __LINE__, __func__);
    }
    return core::Status::NOT_SUPPORTED;
}

core::Status FirebirdUDRConnector::endCopyIn(uint64_t& rows_inserted,
                                            core::ErrorContext* ctx) {
    (void)rows_inserted;
    
    if (ctx) {
        ctx->set(core::Status::NOT_SUPPORTED, 
                "COPY operations not supported in Firebird connector",
                __FILE__, __LINE__, __func__);
    }
    return core::Status::NOT_SUPPORTED;
}

core::Status FirebirdUDRConnector::startCopyOut(const std::string& query,
                                               core::ErrorContext* ctx) {
    (void)query;
    
    if (ctx) {
        ctx->set(core::Status::NOT_SUPPORTED, 
                "COPY operations not supported in Firebird connector",
                __FILE__, __LINE__, __func__);
    }
    return core::Status::NOT_SUPPORTED;
}

core::Status FirebirdUDRConnector::receiveCopyData(std::vector<uint8_t>& data,
                                                  bool& done,
                                                  core::ErrorContext* ctx) {
    (void)data;
    (void)done;
    
    if (ctx) {
        ctx->set(core::Status::NOT_SUPPORTED, 
                "COPY operations not supported in Firebird connector",
                __FILE__, __LINE__, __func__);
    }
    return core::Status::NOT_SUPPORTED;
}

} // namespace udr
} // namespace scratchbird
