/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

#include "scratchbird/udr/postgresql_udr.h"

#include <cstring>
#include <mutex>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/md5.h>

namespace scratchbird {
namespace udr {

// ============================================================================
// PostgreSQLConnection Implementation
// ============================================================================

PostgreSQLConnection::PostgreSQLConnection() = default;

PostgreSQLConnection::~PostgreSQLConnection() {
    close();
}

bool PostgreSQLConnection::isOpen() const {
    return socket_fd_ >= 0;
}

bool PostgreSQLConnection::isValid() const {
    return isOpen() && backend_pid_ > 0;
}

core::Status PostgreSQLConnection::ping(core::ErrorContext* ctx) {
    if (!isOpen()) {
        if (ctx) {
            ctx->set(core::Status::CONNECTION_FAILURE, "Connection is closed",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Send a simple query to test connection
    std::vector<uint8_t> payload = {'S', 'E', 'L', 'E', 'C', 'T', ' ', '1', '\0'};
    auto status = writeMessage(pg::MSG_QUERY, payload, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Read responses until ReadyForQuery
    char msg_type;
    std::vector<uint8_t> msg_payload;
    
    while (true) {
        status = readMessage(msg_type, msg_payload, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        
        if (msg_type == pg::MSG_ERROR) {
            return core::Status::CONNECTION_FAILURE;
        }
        
        if (msg_type == pg::MSG_READY_FOR_QUERY) {
            if (msg_payload.size() >= 1) {
                txn_status_ = msg_payload[0];
            }
            return core::Status::OK;
        }
    }
}

void PostgreSQLConnection::close() {
    if (socket_fd_ >= 0) {
        // Send terminate message
        if (isOpen()) {
            std::vector<uint8_t> empty;
            writeMessage(pg::MSG_TERMINATE, empty, nullptr);
        }
        
        cleanupSSL();
        cleanupSocket();
    }
    
    backend_pid_ = 0;
    backend_key_ = 0;
    txn_status_ = pg::TXN_IDLE;
}

std::string PostgreSQLConnection::getRemoteAddress() const {
    return host_ + ":" + std::to_string(port_);
}

std::string PostgreSQLConnection::getRemoteVersion() const {
    auto it = parameters_.find("server_version");
    if (it != parameters_.end()) {
        return it->second;
    }
    return "unknown";
}

core::Status PostgreSQLConnection::connect(const std::string& host, uint16_t port,
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
    
    // Negotiate SSL if requested
    if (ssl_mode != "disable") {
        auto status = negotiateSSL(ssl_mode, ctx);
        if (status != core::Status::OK && ssl_mode != "prefer") {
            cleanupSocket();
            return status;
        }
    }
    
    // Send startup message
    auto status = startup(database, user, ctx);
    if (status != core::Status::OK) {
        cleanupSocket();
        return status;
    }
    
    return core::Status::OK;
}

core::Status PostgreSQLConnection::startup(const std::string& database,
                                          const std::string& user,
                                          core::ErrorContext* ctx) {
    // Build startup message
    std::vector<uint8_t> msg;
    
    // Protocol version (3.0 = 196608)
    int32_t version = htonl(pg::PROTOCOL_VERSION);
    msg.insert(msg.end(), (uint8_t*)&version, (uint8_t*)&version + 4);
    
    // Parameters
    auto addParam = [&msg](const char* name, const std::string& value) {
        msg.insert(msg.end(), name, name + std::strlen(name) + 1);
        msg.insert(msg.end(), value.begin(), value.end());
        msg.push_back('\0');
    };
    
    addParam("user", user);
    addParam("database", database);
    addParam("client_encoding", "UTF8");
    addParam("DateStyle", "ISO, MDY");
    addParam("TimeZone", "UTC");
    addParam("extra_float_digits", "2");
    
    // Null terminator for parameter list
    msg.push_back('\0');
    
    // Send length + message
    int32_t len = htonl(msg.size() + 4);
    
    auto status = writeExactly(&len, 4, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    status = writeExactly(msg.data(), msg.size(), ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Read responses
    char msg_type;
    std::vector<uint8_t> payload;
    
    while (true) {
        status = readMessage(msg_type, payload, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        
        switch (msg_type) {
            case pg::MSG_ERROR: {
                std::string severity, sqlstate, message;
                readError(severity, sqlstate, message, nullptr);
                if (ctx) {
                    ctx->set(core::Status::CONNECTION_FAILURE, message.c_str(),
                            __FILE__, __LINE__, __func__);
                }
                return core::Status::CONNECTION_FAILURE;
            }
            
            case pg::MSG_AUTHENTICATION: {
                if (payload.size() < 4) {
                    return core::Status::PROTOCOL_VIOLATION;
                }
                int32_t auth_type = ntohl(*((int32_t*)payload.data()));
                
                if (auth_type == pg::AUTH_OK) {
                    // Authentication successful
                    break;
                }
                
                // Need to authenticate
                // This is handled separately after startup
                break;
            }
            
            case pg::MSG_PARAMETER_STATUS: {
                // Parse parameter
                const char* name = (const char*)payload.data();
                const char* value = name + std::strlen(name) + 1;
                parameters_[name] = value;
                break;
            }
            
            case pg::MSG_BACKEND_KEY_DATA: {
                if (payload.size() >= 8) {
                    backend_pid_ = ntohl(*((int32_t*)payload.data()));
                    backend_key_ = ntohl(*((int32_t*)(payload.data() + 4)));
                }
                break;
            }
            
            case pg::MSG_READY_FOR_QUERY: {
                if (payload.size() >= 1) {
                    txn_status_ = payload[0];
                }
                return core::Status::OK;
            }
        }
    }
}

core::Status PostgreSQLConnection::authenticate(int32_t auth_type,
                                               const std::string& password,
                                               const std::vector<uint8_t>& auth_data,
                                               core::ErrorContext* ctx) {
    switch (auth_type) {
        case pg::AUTH_OK:
            return core::Status::OK;
            
        case pg::AUTH_CLEARTEXT_PASSWORD: {
            std::vector<uint8_t> payload(password.begin(), password.end());
            payload.push_back('\0');
            return writeMessage(pg::MSG_PASSWORD_MESSAGE, payload, ctx);
        }
        
        case pg::AUTH_MD5_PASSWORD: {
            if (auth_data.size() < 4) {
                return core::Status::PROTOCOL_VIOLATION;
            }
            return handleAuthMD5(password, auth_data, ctx);
        }
        
        case pg::AUTH_SASL:
            return handleAuthSASL(password, auth_data, ctx);
            
        default:
            if (ctx) {
                ctx->set(core::Status::NOT_SUPPORTED,
                        "Authentication method not supported",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::NOT_SUPPORTED;
    }
}

core::Status PostgreSQLConnection::handleAuthMD5(const std::string& password,
                                                const std::vector<uint8_t>& salt,
                                                core::ErrorContext* ctx) {
    // Get user from parameters
    auto it = parameters_.find("user");
    std::string user = (it != parameters_.end()) ? it->second : "";
    
    std::string hashed = md5Password(password, user, salt);
    
    std::vector<uint8_t> payload(hashed.begin(), hashed.end());
    payload.push_back('\0');
    
    return writeMessage(pg::MSG_PASSWORD_MESSAGE, payload, ctx);
}

core::Status PostgreSQLConnection::handleAuthSASL(const std::string& password,
                                                 const std::vector<uint8_t>& data,
                                                 core::ErrorContext* ctx) {
    // SCRAM-SHA-256 authentication
    // This is a simplified implementation
    // Full implementation would require SASL library
    
    if (ctx) {
        ctx->set(core::Status::NOT_SUPPORTED,
                "SCRAM-SHA-256 authentication not yet implemented",
                __FILE__, __LINE__, __func__);
    }
    return core::Status::NOT_SUPPORTED;
}

std::string PostgreSQLConnection::md5Hash(const std::string& input) {
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((unsigned char*)input.data(), input.size(), digest);
    
    char hex[MD5_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(hex + i * 2, "%02x", digest[i]);
    }
    
    return std::string(hex);
}

std::string PostgreSQLConnection::md5Password(const std::string& password,
                                             const std::string& user,
                                             const std::vector<uint8_t>& salt) {
    // md5(password + user)
    std::string pass_user = md5Hash(password + user);
    
    // md5(pass_user + salt)
    std::string input = pass_user;
    input.append((char*)salt.data(), salt.size());
    
    return "md5" + md5Hash(input);
}

core::Status PostgreSQLConnection::sendQuery(const std::string& sql,
                                            core::ErrorContext* ctx) {
    std::vector<uint8_t> payload(sql.begin(), sql.end());
    payload.push_back('\0');
    return writeMessage(pg::MSG_QUERY, payload, ctx);
}

core::Status PostgreSQLConnection::sendSync(core::ErrorContext* ctx) {
    std::vector<uint8_t> empty;
    return writeMessage(pg::MSG_SYNC, empty, ctx);
}

core::Status PostgreSQLConnection::readMessage(char& msg_type,
                                              std::vector<uint8_t>& payload,
                                              core::ErrorContext* ctx) {
    // Read message type (1 byte) - for server messages, first byte is type
    auto status = readExactly(&msg_type, 1, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Read length (4 bytes, includes itself)
    int32_t len;
    status = readExactly(&len, 4, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    len = ntohl(len);
    if (len < 4) {
        if (ctx) {
            ctx->set(core::Status::PROTOCOL_VIOLATION, "Invalid message length",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::PROTOCOL_VIOLATION;
    }
    
    // Read payload
    int32_t payload_len = len - 4;
    if (payload_len > 0) {
        payload.resize(payload_len);
        status = readExactly(payload.data(), payload_len, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    } else {
        payload.clear();
    }
    
    return core::Status::OK;
}

core::Status PostgreSQLConnection::writeMessage(char msg_type,
                                               const std::vector<uint8_t>& payload,
                                               core::ErrorContext* ctx) {
    // Write message type
    auto status = writeExactly(&msg_type, 1, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Write length (includes 4 bytes for length itself)
    int32_t len = htonl(payload.size() + 4);
    status = writeExactly(&len, 4, ctx);
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

core::Status PostgreSQLConnection::readExactly(void* buffer, size_t len,
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

core::Status PostgreSQLConnection::writeExactly(const void* buffer, size_t len,
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

core::Status PostgreSQLConnection::negotiateSSL(const std::string& ssl_mode,
                                               core::ErrorContext* ctx) {
    // Send SSL request
    int32_t code = htonl(pg::SSL_CODE);
    auto status = writeExactly(&code, 4, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    // Read response (S or N)
    char response;
    status = readExactly(&response, 1, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (response == pg::SSL_YES) {
        return initSSL(ctx);
    }
    
    // SSL not supported
    if (ssl_mode == "require" || ssl_mode == "verify-ca" || ssl_mode == "verify-full") {
        if (ctx) {
            ctx->set(core::Status::CONNECTION_FAILURE, "SSL required but not supported",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_FAILURE;
    }
    
    return core::Status::OK;  // Continue without SSL
}

core::Status PostgreSQLConnection::initSSL(core::ErrorContext* ctx) {
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

void PostgreSQLConnection::cleanupSSL() {
    if (ssl_ctx_) {
        SSL_free((SSL*)ssl_ctx_);
        ssl_ctx_ = nullptr;
    }
    ssl_enabled_ = false;
}

void PostgreSQLConnection::cleanupSocket() {
    if (socket_fd_ >= 0) {
        ::close(socket_fd_);
        socket_fd_ = -1;
    }
}

core::Status PostgreSQLConnection::readError(std::string& severity,
                                            std::string& sqlstate,
                                            std::string& message,
                                            core::ErrorContext* ctx) {
    // Error message fields
    // Format: {char field_type}{string value}\0...
    // S = severity, C = code (SQLSTATE), M = message
    
    // This would parse the error message payload
    // For now, simplified version
    severity = "ERROR";
    sqlstate = "08000";
    message = "Error occurred";
    
    return core::Status::OK;
}

// ============================================================================
// PostgreSQLConnectionFactory Implementation
// ============================================================================

PostgreSQLConnectionFactory::PostgreSQLConnectionFactory(const UDRServerConfig& config)
    : config_(config) {
}

std::unique_ptr<PooledConnection> PostgreSQLConnectionFactory::createConnection(
    core::ErrorContext* ctx) {
    
    auto conn = std::make_unique<PostgreSQLConnection>();
    
    auto status = conn->connect(
        config_.host,
        config_.port ? config_.port : 5432,
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

bool PostgreSQLConnectionFactory::validateConnection(PooledConnection* conn,
                                                    core::ErrorContext* ctx) {
    if (!conn) return false;
    
    auto* pg_conn = dynamic_cast<PostgreSQLConnection*>(conn);
    if (!pg_conn) return false;
    
    return pg_conn->ping(ctx) == core::Status::OK;
}

void PostgreSQLConnectionFactory::destroyConnection(PooledConnection* conn) {
    if (conn) {
        conn->close();
        delete conn;
    }
}

// ============================================================================
// PostgreSQLUDRConnector Implementation
// ============================================================================

PostgreSQLUDRConnector::PostgreSQLUDRConnector() = default;

PostgreSQLUDRConnector::~PostgreSQLUDRConnector() {
    shutdown();
}

core::Status PostgreSQLUDRConnector::initialize(const UDRServerConfig& config,
                                               core::ErrorContext* ctx) {
    config_ = config;
    
    auto factory = std::make_unique<PostgreSQLConnectionFactory>(config);
    
    ConnectionPoolConfig pool_config;
    pool_config.min_size = config.pool_min_size;
    pool_config.max_size = config.pool_max_size;
    pool_config.initial_size = config.pool_min_size;
    pool_config.connection_timeout_ms = config.pool_connection_timeout_ms;
    pool_config.idle_timeout_ms = config.pool_max_idle_ms;
    pool_config.health_check_interval_ms = config.pool_health_check_interval_ms;
    
    pool_ = std::make_unique<ConnectionPool>(
        "postgresql_" + config.host + "_" + config.database,
        std::move(factory),
        pool_config
    );
    
    return pool_->initialize(ctx);
}

core::Status PostgreSQLUDRConnector::shutdown(core::ErrorContext* ctx) {
    if (pool_) {
        pool_->shutdown();
        pool_.reset();
    }
    return core::Status::OK;
}

bool PostgreSQLUDRConnector::isConnected() const {
    return pool_ && pool_->isRunning();
}

core::Status PostgreSQLUDRConnector::ping(core::ErrorContext* ctx) {
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

core::Status PostgreSQLUDRConnector::reconnect(core::ErrorContext* ctx) {
    if (pool_) {
        pool_->invalidateAll();
        return pool_->ensureMinimumConnections(ctx);
    }
    return core::Status::INVALID_ARGUMENT;
}

core::Status PostgreSQLUDRConnector::executeQuery(const std::string& sql,
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
    
    // Read result set
    // ... implementation would read RowDescription and DataRow messages
    
    releaseConnection(std::move(conn));
    return core::Status::OK;
}

core::Status PostgreSQLUDRConnector::executeCommand(const std::string& sql,
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
    
    // Read CommandComplete
    // ...
    
    releaseConnection(std::move(conn));
    return core::Status::OK;
}

core::Status PostgreSQLUDRConnector::beginTransaction(core::ErrorContext* ctx) {
    uint64_t rows;
    return executeCommand("BEGIN", rows, ctx);
}

core::Status PostgreSQLUDRConnector::commitTransaction(core::ErrorContext* ctx) {
    uint64_t rows;
    return executeCommand("COMMIT", rows, ctx);
}

core::Status PostgreSQLUDRConnector::rollbackTransaction(core::ErrorContext* ctx) {
    uint64_t rows;
    return executeCommand("ROLLBACK", rows, ctx);
}

core::Status PostgreSQLUDRConnector::savepoint(const std::string& name,
                                               core::ErrorContext* ctx) {
    uint64_t rows;
    return executeCommand("SAVEPOINT " + name, rows, ctx);
}

core::Status PostgreSQLUDRConnector::rollbackToSavepoint(const std::string& name,
                                                        core::ErrorContext* ctx) {
    uint64_t rows;
    return executeCommand("ROLLBACK TO SAVEPOINT " + name, rows, ctx);
}

std::unique_ptr<PostgreSQLConnection> PostgreSQLUDRConnector::acquireConnection(
    core::ErrorContext* ctx) {
    
    if (!pool_) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Pool not initialized",
                    __FILE__, __LINE__, __func__);
        }
        return nullptr;
    }
    
    auto conn = pool_->acquire(ctx);
    if (!conn) {
        return nullptr;
    }
    
    // Cast to PostgreSQLConnection
    auto* pg_conn = dynamic_cast<PostgreSQLConnection*>(conn.get());
    if (!pg_conn) {
        pool_->release(std::move(conn));
        return nullptr;
    }
    
    conn.release();
    return std::unique_ptr<PostgreSQLConnection>(pg_conn);
}

void PostgreSQLUDRConnector::releaseConnection(
    std::unique_ptr<PostgreSQLConnection> conn) {
    
    if (pool_ && conn) {
        pool_->release(std::unique_ptr<PooledConnection>(conn.release()));
    }
}

std::string PostgreSQLUDRConnector::getVersion() const {
    return "1.0.0";
}

std::string PostgreSQLUDRConnector::getRemoteVersion() const {
    return remote_version_;
}

std::vector<std::string> PostgreSQLUDRConnector::getSupportedFeatures() const {
    return {
        "simple_query",
        "extended_query",
        "prepared_statements",
        "cursors",
        "transactions",
        "savepoints",
        "ssl",
        "md5_auth",
        "copy"
    };
}

// ============================================================================
// Protocol Helper Methods
// ============================================================================

core::Status PostgreSQLConnection::sendParse(const std::string& name,
                                             const std::string& sql,
                                             const std::vector<uint32_t>& param_types,
                                             core::ErrorContext* ctx) {
    std::vector<uint8_t> payload;
    
    // Statement name (null-terminated)
    payload.insert(payload.end(), name.begin(), name.end());
    payload.push_back('\0');
    
    // Query string (null-terminated)
    payload.insert(payload.end(), sql.begin(), sql.end());
    payload.push_back('\0');
    
    // Number of parameter types
    int16_t num_params = htons(static_cast<int16_t>(param_types.size()));
    payload.insert(payload.end(), (uint8_t*)&num_params, (uint8_t*)&num_params + 2);
    
    // Parameter type OIDs
    for (uint32_t oid : param_types) {
        int32_t oid_network = htonl(oid);
        payload.insert(payload.end(), (uint8_t*)&oid_network, (uint8_t*)&oid_network + 4);
    }
    
    return writeMessage(pg::MSG_PARSE, payload, ctx);
}

core::Status PostgreSQLConnection::sendBind(const std::string& portal,
                                            const std::string& statement,
                                            const std::vector<RemoteValue>& params,
                                            core::ErrorContext* ctx) {
    std::vector<uint8_t> payload;
    
    // Portal name (null-terminated, empty = unnamed)
    payload.insert(payload.end(), portal.begin(), portal.end());
    payload.push_back('\0');
    
    // Statement name (null-terminated)
    payload.insert(payload.end(), statement.begin(), statement.end());
    payload.push_back('\0');
    
    // Parameter format codes (0 = text, 1 = binary)
    // Use 0 for all parameters (text format)
    int16_t num_formats = htons(0);
    payload.insert(payload.end(), (uint8_t*)&num_formats, (uint8_t*)&num_formats + 2);
    
    // Number of parameters
    int16_t num_params = htons(static_cast<int16_t>(params.size()));
    payload.insert(payload.end(), (uint8_t*)&num_params, (uint8_t*)&num_params + 2);
    
    // Parameter values
    for (const auto& param : params) {
        if (param.is_null) {
            // -1 length indicates NULL
            int32_t len = htonl(-1);
            payload.insert(payload.end(), (uint8_t*)&len, (uint8_t*)&len + 4);
        } else {
            int32_t len = htonl(static_cast<int32_t>(param.data.size()));
            payload.insert(payload.end(), (uint8_t*)&len, (uint8_t*)&len + 4);
            payload.insert(payload.end(), param.data.begin(), param.data.end());
        }
    }
    
    // Result format codes (0 = text)
    int16_t num_result_formats = htons(0);
    payload.insert(payload.end(), (uint8_t*)&num_result_formats, (uint8_t*)&num_result_formats + 2);
    
    return writeMessage(pg::MSG_BIND, payload, ctx);
}

core::Status PostgreSQLConnection::sendExecute(const std::string& portal,
                                               int32_t max_rows,
                                               core::ErrorContext* ctx) {
    std::vector<uint8_t> payload;
    
    // Portal name (null-terminated)
    payload.insert(payload.end(), portal.begin(), portal.end());
    payload.push_back('\0');
    
    // Maximum rows (0 = unlimited)
    int32_t max_rows_network = htonl(max_rows);
    payload.insert(payload.end(), (uint8_t*)&max_rows_network, (uint8_t*)&max_rows_network + 4);
    
    return writeMessage(pg::MSG_EXECUTE, payload, ctx);
}

core::Status PostgreSQLConnection::sendClose(char type,
                                             const std::string& name,
                                             core::ErrorContext* ctx) {
    std::vector<uint8_t> payload;
    
    // Close type: 'S' = prepared statement, 'P' = portal
    payload.push_back(type);
    
    // Name (null-terminated)
    payload.insert(payload.end(), name.begin(), name.end());
    payload.push_back('\0');
    
    return writeMessage(pg::MSG_CLOSE, payload, ctx);
}

core::Status PostgreSQLConnection::sendDescribe(char type,
                                                const std::string& name,
                                                core::ErrorContext* ctx) {
    std::vector<uint8_t> payload;
    
    // Describe type: 'S' = prepared statement, 'P' = portal
    payload.push_back(type);
    
    // Name (null-terminated)
    payload.insert(payload.end(), name.begin(), name.end());
    payload.push_back('\0');
    
    return writeMessage(pg::MSG_DESCRIBE, payload, ctx);
}

core::Status PostgreSQLConnection::sendCopyData(const uint8_t* data,
                                                size_t len,
                                                core::ErrorContext* ctx) {
    std::vector<uint8_t> payload(data, data + len);
    return writeMessage(pg::MSG_COPY_DATA, payload, ctx);
}

core::Status PostgreSQLConnection::sendCopyDone(core::ErrorContext* ctx) {
    std::vector<uint8_t> empty;
    return writeMessage(pg::MSG_COPY_DONE, empty, ctx);
}

core::Status PostgreSQLConnection::sendCopyFail(const std::string& error,
                                                core::ErrorContext* ctx) {
    std::vector<uint8_t> payload(error.begin(), error.end());
    payload.push_back('\0');
    return writeMessage(pg::MSG_COPY_FAIL, payload, ctx);
}

core::Status PostgreSQLConnection::readResultSet(RemoteResultSet& result,
                                                 core::ErrorContext* ctx) {
    char msg_type;
    std::vector<uint8_t> payload;
    
    result.clear();
    
    while (true) {
        auto status = readMessage(msg_type, payload, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        
        switch (msg_type) {
            case pg::MSG_ROW_DESCRIPTION: {
                // Parse row description
                if (payload.size() < 2) {
                    return core::Status::PROTOCOL_VIOLATION;
                }
                int16_t num_fields = ntohs(*((int16_t*)payload.data()));
                size_t offset = 2;
                
                result.columns.clear();
                for (int16_t i = 0; i < num_fields; i++) {
                    RemoteColumn col;
                    
                    // Field name (null-terminated string)
                    const char* field_name = (const char*)(payload.data() + offset);
                    col.name = field_name;
                    offset += col.name.length() + 1;
                    
                    if (offset + 18 > payload.size()) {
                        return core::Status::PROTOCOL_VIOLATION;
                    }
                    
                    // Table OID (4 bytes)
                    // col.table_oid = ntohl(*((uint32_t*)(payload.data() + offset)));
                    offset += 4;
                    
                    // Column number (2 bytes)
                    // col.column_number = ntohs(*((int16_t*)(payload.data() + offset)));
                    offset += 2;
                    
                    // Type OID (4 bytes)
                    col.type_oid = ntohl(*((uint32_t*)(payload.data() + offset)));
                    offset += 4;
                    
                    // Type size (2 bytes)
                    col.type_size = ntohs(*((int16_t*)(payload.data() + offset)));
                    offset += 2;
                    
                    // Type modifier (4 bytes)
                    col.type_modifier = ntohl(*((int32_t*)(payload.data() + offset)));
                    offset += 4;
                    
                    // Format code (2 bytes) - 0 = text, 1 = binary
                    // int16_t format = ntohs(*((int16_t*)(payload.data() + offset)));
                    offset += 2;
                    
                    result.columns.push_back(col);
                }
                break;
            }
            
            case pg::MSG_DATA_ROW: {
                if (payload.size() < 2) {
                    return core::Status::PROTOCOL_VIOLATION;
                }
                int16_t num_values = ntohs(*((int16_t*)payload.data()));
                size_t offset = 2;
                
                RemoteRow row;
                for (int16_t i = 0; i < num_values; i++) {
                    if (offset + 4 > payload.size()) {
                        return core::Status::PROTOCOL_VIOLATION;
                    }
                    
                    int32_t len = ntohl(*((int32_t*)(payload.data() + offset)));
                    offset += 4;
                    
                    RemoteValue val;
                    if (len == -1) {
                        // NULL value
                        val.is_null = true;
                    } else {
                        if (offset + len > payload.size()) {
                            return core::Status::PROTOCOL_VIOLATION;
                        }
                        val.data.assign(payload.data() + offset, payload.data() + offset + len);
                        val.is_null = false;
                        if (i < static_cast<int16_t>(result.columns.size())) {
                            val.type_oid = result.columns[i].type_oid;
                        }
                        offset += len;
                    }
                    row.values.push_back(val);
                }
                result.rows.push_back(row);
                break;
            }
            
            case pg::MSG_COMMAND_COMPLETE: {
                // Parse command tag (e.g., "INSERT 0 1", "UPDATE 3", etc.)
                std::string tag((const char*)payload.data());
                result.command_tag = tag;
                
                // Extract row count from tag
                size_t last_space = tag.rfind(' ');
                if (last_space != std::string::npos) {
                    try {
                        result.rows_affected = std::stoull(tag.substr(last_space + 1));
                    } catch (...) {
                        result.rows_affected = 0;
                    }
                }
                break;
            }
            
            case pg::MSG_EMPTY_QUERY_RESPONSE:
                // Empty query, return empty result
                return core::Status::OK;
            
            case pg::MSG_ERROR: {
                std::string severity, sqlstate, message;
                readError(severity, sqlstate, message, nullptr);
                if (ctx) {
                    ctx->set(core::Status::SYNTAX_ERROR, message.c_str(),
                            __FILE__, __LINE__, __func__);
                }
                return core::Status::SYNTAX_ERROR;
            }
            
            case pg::MSG_READY_FOR_QUERY: {
                if (payload.size() >= 1) {
                    txn_status_ = payload[0];
                }
                return core::Status::OK;
            }
            
            case pg::MSG_PARSE_COMPLETE:
            case pg::MSG_BIND_COMPLETE:
            case pg::MSG_CLOSE_COMPLETE:
            case pg::MSG_NO_DATA:
            case pg::MSG_PARAMETER_DESCRIPTION:
                // These are expected during extended query protocol
                break;
            
            case pg::MSG_PORTAL_SUSPENDED:
                result.has_more = true;
                break;
                
            default:
                // Unexpected message
                break;
        }
    }
}

core::Status PostgreSQLConnection::readCommandComplete(std::string& tag,
                                                       uint64_t& rows,
                                                       core::ErrorContext* ctx) {
    char msg_type;
    std::vector<uint8_t> payload;
    
    while (true) {
        auto status = readMessage(msg_type, payload, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        
        switch (msg_type) {
            case pg::MSG_COMMAND_COMPLETE: {
                tag = std::string((const char*)payload.data());
                size_t last_space = tag.rfind(' ');
                if (last_space != std::string::npos) {
                    try {
                        rows = std::stoull(tag.substr(last_space + 1));
                    } catch (...) {
                        rows = 0;
                    }
                }
                break;
            }
            
            case pg::MSG_ERROR: {
                std::string severity, sqlstate, message;
                readError(severity, sqlstate, message, nullptr);
                if (ctx) {
                    ctx->set(core::Status::SYNTAX_ERROR, message.c_str(),
                            __FILE__, __LINE__, __func__);
                }
                return core::Status::SYNTAX_ERROR;
            }
            
            case pg::MSG_READY_FOR_QUERY: {
                if (payload.size() >= 1) {
                    txn_status_ = payload[0];
                }
                return core::Status::OK;
            }
            
            default:
                break;
        }
    }
}

// ============================================================================
// Prepared Statement Operations
// ============================================================================

core::Status PostgreSQLUDRConnector::prepareStatement(const std::string& name,
                                                      const std::string& sql,
                                                      std::vector<uint32_t>& param_types,
                                                      core::ErrorContext* ctx) {
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
    
    // Send Parse message
    auto status = conn->sendParse(name, sql, param_types, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Send Sync
    status = conn->sendSync(ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Read response
    char msg_type;
    std::vector<uint8_t> payload;
    
    while (true) {
        status = conn->readMessage(msg_type, payload, ctx);
        if (status != core::Status::OK) {
            releaseConnection(std::move(conn));
            return status;
        }
        
        switch (msg_type) {
            case pg::MSG_PARSE_COMPLETE:
                // Success
                break;
                
            case pg::MSG_PARAMETER_DESCRIPTION: {
                // Parse parameter types
                if (payload.size() >= 2) {
                    int16_t num_params = ntohs(*((int16_t*)payload.data()));
                    param_types.clear();
                    for (int16_t i = 0; i < num_params && (2 + i * 4 + 4) <= payload.size(); i++) {
                        uint32_t oid = ntohl(*((uint32_t*)(payload.data() + 2 + i * 4)));
                        param_types.push_back(oid);
                    }
                }
                break;
            }
            
            case pg::MSG_ERROR: {
                std::string severity, sqlstate, message;
                conn->readError(severity, sqlstate, message, nullptr);
                if (ctx) {
                    ctx->set(core::Status::SYNTAX_ERROR, message.c_str(),
                            __FILE__, __LINE__, __func__);
                }
                releaseConnection(std::move(conn));
                return core::Status::SYNTAX_ERROR;
            }
            
            case pg::MSG_READY_FOR_QUERY:
                // Store prepared statement info
                {
                    std::lock_guard<std::mutex> lock(prepared_mutex_);
                    prepared_statements_[name] = param_types;
                }
                releaseConnection(std::move(conn));
                return core::Status::OK;
                
            default:
                break;
        }
    }
}

core::Status PostgreSQLUDRConnector::executePrepared(const std::string& name,
                                                     const std::vector<RemoteValue>& params,
                                                     RemoteResultSet& result,
                                                     core::ErrorContext* ctx) {
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
    
    // Generate unique portal name (empty = unnamed portal)
    std::string portal = "";
    
    // Send Bind
    auto status = conn->sendBind(portal, name, params, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Send Execute (0 = unlimited rows)
    status = conn->sendExecute(portal, 0, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Send Sync
    status = conn->sendSync(ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Read result set
    status = conn->readResultSet(result, ctx);
    
    releaseConnection(std::move(conn));
    return status;
}

core::Status PostgreSQLUDRConnector::closeStatement(const std::string& name,
                                                    core::ErrorContext* ctx) {
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
    
    // Send Close message for statement
    auto status = conn->sendClose('S', name, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Send Sync
    status = conn->sendSync(ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Read response
    char msg_type;
    std::vector<uint8_t> payload;
    
    while (true) {
        status = conn->readMessage(msg_type, payload, ctx);
        if (status != core::Status::OK) {
            releaseConnection(std::move(conn));
            return status;
        }
        
        if (msg_type == pg::MSG_ERROR) {
            std::string severity, sqlstate, message;
            conn->readError(severity, sqlstate, message, nullptr);
            if (ctx) {
                ctx->set(core::Status::SYNTAX_ERROR, message.c_str(),
                        __FILE__, __LINE__, __func__);
            }
            releaseConnection(std::move(conn));
            return core::Status::SYNTAX_ERROR;
        }
        
        if (msg_type == pg::MSG_CLOSE_COMPLETE) {
            // Success
        }
        
        if (msg_type == pg::MSG_READY_FOR_QUERY) {
            // Remove from tracking
            {
                std::lock_guard<std::mutex> lock(prepared_mutex_);
                prepared_statements_.erase(name);
            }
            releaseConnection(std::move(conn));
            return core::Status::OK;
        }
    }
}

// ============================================================================
// Cursor Operations
// ============================================================================

core::Status PostgreSQLUDRConnector::declareCursor(const std::string& cursor_name,
                                                   const std::string& query,
                                                   bool scrollable,
                                                   core::ErrorContext* ctx) {
    // Build DECLARE cursor SQL
    std::string sql = "DECLARE " + cursor_name + " ";
    if (scrollable) {
        sql += "SCROLL ";
    }
    sql += "CURSOR FOR " + query;
    
    uint64_t rows;
    return executeCommand(sql, rows, ctx);
}

core::Status PostgreSQLUDRConnector::fetchCursor(const std::string& cursor_name,
                                                 uint32_t count,
                                                 RemoteResultSet& result,
                                                 core::ErrorContext* ctx) {
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
    
    // Build FETCH SQL
    std::string sql = "FETCH ";
    if (count == 0) {
        sql += "ALL ";
    } else {
        sql += std::to_string(count) + " ";
    }
    sql += "FROM " + cursor_name;
    
    // Send query
    auto status = conn->sendQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Read result
    status = conn->readResultSet(result, ctx);
    result.cursor_name = cursor_name;
    
    releaseConnection(std::move(conn));
    return status;
}

core::Status PostgreSQLUDRConnector::closeCursor(const std::string& cursor_name,
                                                 core::ErrorContext* ctx) {
    std::string sql = "CLOSE " + cursor_name;
    uint64_t rows;
    return executeCommand(sql, rows, ctx);
}

// ============================================================================
// Schema Introspection Operations
// ============================================================================

core::Status PostgreSQLUDRConnector::getTableInfo(const std::string& schema,
                                                  const std::string& table,
                                                  RemoteTableInfo& info,
                                                  core::ErrorContext* ctx) {
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
    
    // Build query to get column info from information_schema
    std::string sql = 
        "SELECT column_name, data_type, is_nullable, column_default, "
        "       character_maximum_length, numeric_precision, ordinal_position "
        "FROM information_schema.columns "
        "WHERE table_schema = '" + schema + "' "
        "AND table_name = '" + table + "' "
        "ORDER BY ordinal_position";
    
    auto status = conn->sendQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    RemoteResultSet result;
    status = conn->readResultSet(result, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    info.remote_schema = schema;
    info.remote_name = table;
    info.columns.clear();
    
    for (const auto& row : result.rows) {
        RemoteColumn col;
        
        if (row.values.size() > 0 && !row.values[0].is_null) {
            col.name = std::string((const char*)row.values[0].data.data(), 
                                   row.values[0].data.size());
        }
        
        if (row.values.size() > 1 && !row.values[1].is_null) {
            col.type_name = std::string((const char*)row.values[1].data.data(),
                                        row.values[1].data.size());
            col.type_oid = mapTypeToOid(col.type_name);
        }
        
        if (row.values.size() > 2 && !row.values[2].is_null) {
            std::string nullable((const char*)row.values[2].data.data(),
                                row.values[2].data.size());
            col.nullable = (nullable == "YES");
        }
        
        if (row.values.size() > 3) {
            if (!row.values[3].is_null) {
                col.default_value = std::string((const char*)row.values[3].data.data(),
                                                row.values[3].data.size());
            }
        }
        
        info.columns.push_back(col);
    }
    
    // Get primary key info
    sql = 
        "SELECT kcu.column_name "
        "FROM information_schema.table_constraints tc "
        "JOIN information_schema.key_column_usage kcu "
        "  ON tc.constraint_name = kcu.constraint_name "
        "  AND tc.table_schema = kcu.table_schema "
        "WHERE tc.constraint_type = 'PRIMARY KEY' "
        "AND tc.table_schema = '" + schema + "' "
        "AND tc.table_name = '" + table + "'";
    
    status = conn->sendQuery(sql, ctx);
    if (status == core::Status::OK) {
        result.clear();
        status = conn->readResultSet(result, ctx);
        if (status == core::Status::OK) {
            info.primary_key.clear();
            for (const auto& row : result.rows) {
                if (row.values.size() > 0 && !row.values[0].is_null) {
                    info.primary_key.push_back(
                        std::string((const char*)row.values[0].data.data(),
                                   row.values[0].data.size()));
                }
            }
        }
    }
    
    releaseConnection(std::move(conn));
    return core::Status::OK;
}

core::Status PostgreSQLUDRConnector::listTables(const std::string& schema,
                                                std::vector<std::string>& tables,
                                                core::ErrorContext* ctx) {
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
    
    std::string sql = 
        "SELECT table_name FROM information_schema.tables "
        "WHERE table_schema = '" + schema + "' "
        "AND table_type = 'BASE TABLE' "
        "ORDER BY table_name";
    
    auto status = conn->sendQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    RemoteResultSet result;
    status = conn->readResultSet(result, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    tables.clear();
    for (const auto& row : result.rows) {
        if (row.values.size() > 0 && !row.values[0].is_null) {
            tables.push_back(std::string((const char*)row.values[0].data.data(),
                                        row.values[0].data.size()));
        }
    }
    
    releaseConnection(std::move(conn));
    return core::Status::OK;
}

core::Status PostgreSQLUDRConnector::getProcedureInfo(const std::string& schema,
                                                      const std::string& procedure,
                                                      RemoteProcedureInfo& info,
                                                      core::ErrorContext* ctx) {
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
    
    // Get routine info
    std::string sql = 
        "SELECT routine_type, data_type "
        "FROM information_schema.routines "
        "WHERE routine_schema = '" + schema + "' "
        "AND routine_name = '" + procedure + "'";
    
    auto status = conn->sendQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    RemoteResultSet result;
    status = conn->readResultSet(result, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    info.remote_schema = schema;
    info.remote_name = procedure;
    info.input_params.clear();
    info.output_params.clear();
    
    if (!result.rows.empty() && result.rows[0].values.size() > 1) {
        if (!result.rows[0].values[1].is_null) {
            info.return_type = std::string((const char*)result.rows[0].values[1].data.data(),
                                          result.rows[0].values[1].data.size());
        }
    }
    
    // Get parameters
    sql = 
        "SELECT parameter_name, parameter_mode, data_type, ordinal_position "
        "FROM information_schema.parameters "
        "WHERE specific_schema = '" + schema + "' "
        "AND specific_name IN ("
        "  SELECT specific_name FROM information_schema.routines "
        "  WHERE routine_schema = '" + schema + "' "
        "  AND routine_name = '" + procedure + "'"
        ") ORDER BY ordinal_position";
    
    status = conn->sendQuery(sql, ctx);
    if (status == core::Status::OK) {
        result.clear();
        status = conn->readResultSet(result, ctx);
        if (status == core::Status::OK) {
            for (const auto& row : result.rows) {
                if (row.values.size() < 3) continue;
                
                RemoteColumn param;
                
                if (!row.values[0].is_null) {
                    param.name = std::string((const char*)row.values[0].data.data(),
                                            row.values[0].data.size());
                }
                
                if (!row.values[2].is_null) {
                    param.type_name = std::string((const char*)row.values[2].data.data(),
                                                 row.values[2].data.size());
                }
                
                std::string mode = "IN";
                if (!row.values[1].is_null) {
                    mode = std::string((const char*)row.values[1].data.data(),
                                      row.values[1].data.size());
                }
                
                if (mode == "IN" || mode == "INOUT") {
                    info.input_params.push_back(param);
                }
                if (mode == "OUT" || mode == "INOUT") {
                    info.output_params.push_back(param);
                }
            }
        }
    }
    
    releaseConnection(std::move(conn));
    return core::Status::OK;
}

core::Status PostgreSQLUDRConnector::listProcedures(const std::string& schema,
                                                    std::vector<std::string>& procedures,
                                                    core::ErrorContext* ctx) {
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
    
    std::string sql = 
        "SELECT routine_name FROM information_schema.routines "
        "WHERE routine_schema = '" + schema + "' "
        "AND routine_type IN ('FUNCTION', 'PROCEDURE') "
        "ORDER BY routine_name";
    
    auto status = conn->sendQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    RemoteResultSet result;
    status = conn->readResultSet(result, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    procedures.clear();
    for (const auto& row : result.rows) {
        if (row.values.size() > 0 && !row.values[0].is_null) {
            procedures.push_back(std::string((const char*)row.values[0].data.data(),
                                            row.values[0].data.size()));
        }
    }
    
    releaseConnection(std::move(conn));
    return core::Status::OK;
}

// ============================================================================
// COPY Operations
// ============================================================================

core::Status PostgreSQLUDRConnector::startCopyIn(const std::string& table,
                                                 const std::vector<std::string>& columns,
                                                 core::ErrorContext* ctx) {
    if (!pool_) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Connector not initialized",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    if (copy_in_progress_) {
        if (ctx) {
            ctx->set(core::Status::INVALID_CURSOR_STATE, "COPY already in progress",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_CURSOR_STATE;
    }
    
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Build COPY SQL
    std::string sql = "COPY " + table;
    if (!columns.empty()) {
        sql += "(";
        for (size_t i = 0; i < columns.size(); i++) {
            if (i > 0) sql += ", ";
            sql += columns[i];
        }
        sql += ")";
    }
    sql += " FROM STDIN";
    
    auto status = conn->sendQuery(sql, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Read response - expect CopyInResponse
    char msg_type;
    std::vector<uint8_t> payload;
    
    while (true) {
        status = conn->readMessage(msg_type, payload, ctx);
        if (status != core::Status::OK) {
            releaseConnection(std::move(conn));
            return status;
        }
        
        switch (msg_type) {
            case pg::MSG_COPY_IN_RESPONSE:
                // Success - server is ready for COPY data
                copy_in_progress_ = true;
                copy_is_in_ = true;
                copy_connection_ = std::move(conn);
                return core::Status::OK;
                
            case pg::MSG_ERROR: {
                std::string severity, sqlstate, message;
                conn->readError(severity, sqlstate, message, nullptr);
                if (ctx) {
                    ctx->set(core::Status::SYNTAX_ERROR, message.c_str(),
                            __FILE__, __LINE__, __func__);
                }
                releaseConnection(std::move(conn));
                return core::Status::SYNTAX_ERROR;
            }
            
            case pg::MSG_READY_FOR_QUERY:
                // No COPY needed (e.g., zero rows)
                releaseConnection(std::move(conn));
                return core::Status::OK;
                
            default:
                break;
        }
    }
}

core::Status PostgreSQLUDRConnector::sendCopyData(const uint8_t* data,
                                                  size_t len,
                                                  core::ErrorContext* ctx) {
    if (!copy_in_progress_ || !copy_is_in_) {
        if (ctx) {
            ctx->set(core::Status::INVALID_CURSOR_STATE, "COPY IN not in progress",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_CURSOR_STATE;
    }
    
    if (!copy_connection_) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    return copy_connection_->sendCopyData(data, len, ctx);
}

core::Status PostgreSQLUDRConnector::endCopyIn(uint64_t& rows_inserted,
                                               core::ErrorContext* ctx) {
    if (!copy_in_progress_ || !copy_is_in_) {
        if (ctx) {
            ctx->set(core::Status::INVALID_CURSOR_STATE, "COPY IN not in progress",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_CURSOR_STATE;
    }
    
    if (!copy_connection_) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    // Send CopyDone
    auto status = copy_connection_->sendCopyDone(ctx);
    if (status != core::Status::OK) {
        copy_in_progress_ = false;
        copy_is_in_ = false;
        releaseConnection(std::move(copy_connection_));
        return status;
    }
    
    // Read response
    char msg_type;
    std::vector<uint8_t> payload;
    rows_inserted = 0;
    
    while (true) {
        status = copy_connection_->readMessage(msg_type, payload, ctx);
        if (status != core::Status::OK) {
            copy_in_progress_ = false;
            copy_is_in_ = false;
            releaseConnection(std::move(copy_connection_));
            return status;
        }
        
        switch (msg_type) {
            case pg::MSG_COMMAND_COMPLETE: {
                std::string tag((const char*)payload.data());
                size_t last_space = tag.rfind(' ');
                if (last_space != std::string::npos) {
                    try {
                        rows_inserted = std::stoull(tag.substr(last_space + 1));
                    } catch (...) {
                        rows_inserted = 0;
                    }
                }
                break;
            }
            
            case pg::MSG_ERROR: {
                std::string severity, sqlstate, message;
                copy_connection_->readError(severity, sqlstate, message, nullptr);
                if (ctx) {
                    ctx->set(core::Status::SYNTAX_ERROR, message.c_str(),
                            __FILE__, __LINE__, __func__);
                }
                copy_in_progress_ = false;
                copy_is_in_ = false;
                releaseConnection(std::move(copy_connection_));
                return core::Status::SYNTAX_ERROR;
            }
            
            case pg::MSG_READY_FOR_QUERY:
                copy_in_progress_ = false;
                copy_is_in_ = false;
                releaseConnection(std::move(copy_connection_));
                return core::Status::OK;
                
            default:
                break;
        }
    }
}

core::Status PostgreSQLUDRConnector::startCopyOut(const std::string& query,
                                                  core::ErrorContext* ctx) {
    if (!pool_) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Connector not initialized",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    if (copy_in_progress_) {
        if (ctx) {
            ctx->set(core::Status::INVALID_CURSOR_STATE, "COPY already in progress",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_CURSOR_STATE;
    }
    
    auto conn = acquireConnection(ctx);
    if (!conn) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    auto status = conn->sendQuery(query, ctx);
    if (status != core::Status::OK) {
        releaseConnection(std::move(conn));
        return status;
    }
    
    // Read response - expect CopyOutResponse
    char msg_type;
    std::vector<uint8_t> payload;
    
    while (true) {
        status = conn->readMessage(msg_type, payload, ctx);
        if (status != core::Status::OK) {
            releaseConnection(std::move(conn));
            return status;
        }
        
        switch (msg_type) {
            case pg::MSG_COPY_OUT_RESPONSE:
                copy_in_progress_ = true;
                copy_is_in_ = false;
                copy_connection_ = std::move(conn);
                return core::Status::OK;
                
            case pg::MSG_ERROR: {
                std::string severity, sqlstate, message;
                conn->readError(severity, sqlstate, message, nullptr);
                if (ctx) {
                    ctx->set(core::Status::SYNTAX_ERROR, message.c_str(),
                            __FILE__, __LINE__, __func__);
                }
                releaseConnection(std::move(conn));
                return core::Status::SYNTAX_ERROR;
            }
            
            case pg::MSG_READY_FOR_QUERY:
                // No COPY data
                releaseConnection(std::move(conn));
                return core::Status::OK;
                
            default:
                break;
        }
    }
}

core::Status PostgreSQLUDRConnector::receiveCopyData(std::vector<uint8_t>& data,
                                                     bool& done,
                                                     core::ErrorContext* ctx) {
    done = false;
    
    if (!copy_in_progress_ || copy_is_in_) {
        if (ctx) {
            ctx->set(core::Status::INVALID_CURSOR_STATE, "COPY OUT not in progress",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_CURSOR_STATE;
    }
    
    if (!copy_connection_) {
        return core::Status::CONNECTION_FAILURE;
    }
    
    char msg_type;
    std::vector<uint8_t> payload;
    
    auto status = copy_connection_->readMessage(msg_type, payload, ctx);
    if (status != core::Status::OK) {
        copy_in_progress_ = false;
        releaseConnection(std::move(copy_connection_));
        return status;
    }
    
    switch (msg_type) {
        case pg::MSG_COPY_DATA:
            data = std::move(payload);
            return core::Status::OK;
            
        case pg::MSG_COPY_DONE:
            // Read until ReadyForQuery
            while (true) {
                status = copy_connection_->readMessage(msg_type, payload, ctx);
                if (status != core::Status::OK) {
                    copy_in_progress_ = false;
                    releaseConnection(std::move(copy_connection_));
                    return status;
                }
                if (msg_type == pg::MSG_READY_FOR_QUERY) {
                    break;
                }
            }
            done = true;
            copy_in_progress_ = false;
            releaseConnection(std::move(copy_connection_));
            return core::Status::OK;
            
        case pg::MSG_ERROR: {
            std::string severity, sqlstate, message;
            copy_connection_->readError(severity, sqlstate, message, nullptr);
            if (ctx) {
                ctx->set(core::Status::SYNTAX_ERROR, message.c_str(),
                        __FILE__, __LINE__, __func__);
            }
            copy_in_progress_ = false;
            releaseConnection(std::move(copy_connection_));
            return core::Status::SYNTAX_ERROR;
        }
        
        case pg::MSG_READY_FOR_QUERY:
            done = true;
            copy_in_progress_ = false;
            releaseConnection(std::move(copy_connection_));
            return core::Status::OK;
            
        default:
            data.clear();
            return core::Status::OK;
    }
}

// ============================================================================
// Type Mapping Helper Methods
// ============================================================================

uint32_t PostgreSQLUDRConnector::mapTypeToOid(const std::string& type_name) const {
    static const std::unordered_map<std::string, uint32_t> type_map = {
        {"boolean", 16},
        {"bool", 16},
        {"smallint", 21},
        {"int2", 21},
        {"integer", 23},
        {"int", 23},
        {"int4", 23},
        {"bigint", 20},
        {"int8", 20},
        {"real", 700},
        {"float4", 700},
        {"double precision", 701},
        {"float8", 701},
        {"numeric", 1700},
        {"decimal", 1700},
        {"character varying", 1043},
        {"varchar", 1043},
        {"character", 1042},
        {"char", 1042},
        {"text", 25},
        {"bytea", 17},
        {"timestamp without time zone", 1114},
        {"timestamp", 1114},
        {"timestamp with time zone", 1184},
        {"timestamptz", 1184},
        {"date", 1082},
        {"time without time zone", 1083},
        {"time", 1083},
        {"time with time zone", 1266},
        {"timetz", 1266},
        {"interval", 1186},
        {"uuid", 2950},
        {"json", 114},
        {"jsonb", 3802},
        {"xml", 142},
        {"oid", 26},
        {"xid", 28},
        {"cid", 29}
    };
    
    auto it = type_map.find(type_name);
    if (it != type_map.end()) {
        return it->second;
    }
    return 0;  // Unknown type
}

std::string PostgreSQLUDRConnector::mapOidToType(uint32_t oid) const {
    static const std::unordered_map<uint32_t, std::string> oid_map = {
        {16, "boolean"},
        {21, "smallint"},
        {23, "integer"},
        {20, "bigint"},
        {700, "real"},
        {701, "double precision"},
        {1700, "numeric"},
        {1043, "character varying"},
        {1042, "character"},
        {25, "text"},
        {17, "bytea"},
        {1114, "timestamp without time zone"},
        {1184, "timestamp with time zone"},
        {1082, "date"},
        {1083, "time without time zone"},
        {1266, "time with time zone"},
        {1186, "interval"},
        {2950, "uuid"},
        {114, "json"},
        {3802, "jsonb"},
        {142, "xml"},
        {26, "oid"}
    };
    
    auto it = oid_map.find(oid);
    if (it != oid_map.end()) {
        return it->second;
    }
    return "unknown";
}

RemoteValue PostgreSQLUDRConnector::decodeValue(const std::vector<uint8_t>& data, uint32_t oid) const {
    RemoteValue val;
    val.data = data;
    val.type_oid = oid;
    val.is_null = data.empty();
    return val;
}

std::vector<uint8_t> PostgreSQLUDRConnector::encodeValue(const RemoteValue& value, uint32_t oid) const {
    if (value.is_null) {
        return {};
    }
    return value.data;
}

} // namespace udr
} // namespace scratchbird
