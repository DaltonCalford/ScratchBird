/**
 * ScratchBird Network Client (libscratchbird)
 *
 * Alpha: Native protocol over network listener (parser bridge required).
 */

#include "scratchbird/client/network_client.h"
#include "scratchbird/client/driver_config.h"
#include "scratchbird/client/sql_helpers.h"
#include "scratchbird/security/scram_auth.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <openssl/evp.h>
#include <openssl/hmac.h>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

namespace scratchbird {
namespace client {

namespace {
uint32_t getProcessId() {
#ifdef _WIN32
    return static_cast<uint32_t>(_getpid());
#else
    return static_cast<uint32_t>(getpid());
#endif
}

core::Status mapQueryError(const protocol::Message& response,
                           std::string& message,
                           core::ErrorContext* ctx) {
    uint32_t error_code = 0;
    std::string sqlstate;
    std::string detail;
    std::string hint;
    protocol::ProtocolCodec::parseQueryError(response, error_code, sqlstate, message, detail, hint, ctx);
    if (!detail.empty()) {
        message += " (" + detail + ")";
    }
    auto status = static_cast<core::Status>(error_code);
    if (ctx) {
        ctx->code = status;
        ctx->message = message;
        if (!sqlstate.empty()) {
            ctx->setSQLState(sqlstate.c_str());
        } else {
            ctx->sqlstate = core::statusToSQLState(status);
            ctx->sqlstate_text.clear();
        }
        ctx->hint = hint;
    }
    return status;
}

std::string normalizeUsername(const std::string& username) {
    std::string result;
    result.reserve(username.size());

    for (char c : username) {
        if (c == '=') {
            result += "=3D";
        } else if (c == ',') {
            result += "=2C";
        } else {
            result.push_back(c);
        }
    }

    return result;
}

struct ScramServerFirst {
    std::string nonce;
    std::vector<uint8_t> salt;
    uint32_t iterations{0};
};

bool parseScramServerFirst(const std::string& message, ScramServerFirst& parsed) {
    // Format: r=<nonce>,s=<base64-salt>,i=<iterations>
    size_t r_pos = message.find("r=");
    size_t s_pos = message.find(",s=");
    size_t i_pos = message.find(",i=");
    if (r_pos != 0 || s_pos == std::string::npos || i_pos == std::string::npos) {
        return false;
    }

    parsed.nonce = message.substr(2, s_pos - 2);
    std::string salt_b64 = message.substr(s_pos + 3, i_pos - (s_pos + 3));
    parsed.salt = security::base64Decode(salt_b64);
    try {
        parsed.iterations = static_cast<uint32_t>(std::stoul(message.substr(i_pos + 3)));
    } catch (...) {
        return false;
    }

    return parsed.iterations > 0;
}

bool parseScramServerFinal(const std::string& message, std::vector<uint8_t>& signature) {
    // Format: v=<base64-server-signature>
    if (message.rfind("v=", 0) != 0) {
        return false;
    }
    signature = security::base64Decode(message.substr(2));
    return !signature.empty();
}

const EVP_MD* scramDigest(security::ScramAlgorithm algorithm) {
    return (algorithm == security::ScramAlgorithm::SHA_256) ? EVP_sha256() : EVP_sha512();
}

bool scramSaltedPassword(const std::string& password,
                         const std::vector<uint8_t>& salt,
                         uint32_t iterations,
                         security::ScramAlgorithm algorithm,
                         std::vector<uint8_t>& out) {
    const EVP_MD* md = scramDigest(algorithm);
    const int hash_len = (algorithm == security::ScramAlgorithm::SHA_256) ? 32 : 64;
    out.assign(static_cast<size_t>(hash_len), 0);
    if (PKCS5_PBKDF2_HMAC(password.c_str(),
                          static_cast<int>(password.size()),
                          salt.data(),
                          static_cast<int>(salt.size()),
                          static_cast<int>(iterations),
                          md,
                          hash_len,
                          out.data()) != 1) {
        return false;
    }
    return true;
}

bool scramHmac(const std::vector<uint8_t>& key,
               const std::string& message,
               security::ScramAlgorithm algorithm,
               std::vector<uint8_t>& out) {
    const EVP_MD* md = scramDigest(algorithm);
    const int hash_len = (algorithm == security::ScramAlgorithm::SHA_256) ? 32 : 64;
    out.assign(static_cast<size_t>(hash_len), 0);
    unsigned int out_len = hash_len;
    if (!HMAC(md,
              key.data(),
              static_cast<int>(key.size()),
              reinterpret_cast<const unsigned char*>(message.data()),
              message.size(),
              out.data(),
              &out_len)) {
        return false;
    }
    out.resize(out_len);
    return true;
}

bool scramHash(const std::vector<uint8_t>& input,
               security::ScramAlgorithm algorithm,
               std::vector<uint8_t>& out) {
    const EVP_MD* md = scramDigest(algorithm);
    const int hash_len = (algorithm == security::ScramAlgorithm::SHA_256) ? 32 : 64;
    out.assign(static_cast<size_t>(hash_len), 0);
    unsigned int out_len = hash_len;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return false;
    }
    bool ok = EVP_DigestInit_ex(ctx, md, nullptr) == 1;
    ok = ok && (EVP_DigestUpdate(ctx, input.data(), input.size()) == 1);
    ok = ok && (EVP_DigestFinal_ex(ctx, out.data(), &out_len) == 1);
    EVP_MD_CTX_free(ctx);
    if (!ok) {
        return false;
    }
    out.resize(out_len);
    return true;
}

struct ScramClientExchange {
    std::string client_nonce;
    std::string client_first_bare;
    std::string server_first;
    std::string client_final;
    std::vector<uint8_t> expected_server_signature;
};

core::Status buildScramClientFirst(const std::string& username,
                                   ScramClientExchange& exchange,
                                   std::string& error_msg) {
    exchange.client_nonce = security::generateNonce();
    exchange.client_first_bare = "n=" + normalizeUsername(username) +
                                 ",r=" + exchange.client_nonce;
    if (exchange.client_nonce.empty()) {
        error_msg = "SCRAM nonce generation failed";
        return core::Status::INTERNAL_ERROR;
    }
    return core::Status::OK;
}

core::Status handleScramServerFirst(const std::string& password,
                                    security::ScramAlgorithm algorithm,
                                    ScramClientExchange& exchange,
                                    const std::string& server_first,
                                    std::string& error_msg) {
    ScramServerFirst parsed;
    if (!parseScramServerFirst(server_first, parsed)) {
        error_msg = "Invalid SCRAM server-first message";
        return core::Status::PROTOCOL_VIOLATION;
    }
    if (parsed.nonce.rfind(exchange.client_nonce, 0) != 0) {
        error_msg = "SCRAM nonce mismatch";
        return core::Status::PROTOCOL_VIOLATION;
    }

    std::vector<uint8_t> salted_password;
    if (!scramSaltedPassword(password, parsed.salt, parsed.iterations, algorithm, salted_password)) {
        error_msg = "SCRAM salted password derivation failed";
        return core::Status::INTERNAL_ERROR;
    }

    std::vector<uint8_t> client_key;
    if (!scramHmac(salted_password, "Client Key", algorithm, client_key)) {
        error_msg = "SCRAM client key derivation failed";
        return core::Status::INTERNAL_ERROR;
    }

    std::vector<uint8_t> stored_key;
    if (!scramHash(client_key, algorithm, stored_key)) {
        error_msg = "SCRAM stored key derivation failed";
        return core::Status::INTERNAL_ERROR;
    }

    std::vector<uint8_t> server_key;
    if (!scramHmac(salted_password, "Server Key", algorithm, server_key)) {
        error_msg = "SCRAM server key derivation failed";
        return core::Status::INTERNAL_ERROR;
    }

    std::string client_final_without_proof = "c=biws,r=" + parsed.nonce;
    std::string auth_message = exchange.client_first_bare + "," +
                               server_first + "," +
                               client_final_without_proof;

    std::vector<uint8_t> client_signature;
    if (!scramHmac(stored_key, auth_message, algorithm, client_signature)) {
        error_msg = "SCRAM client signature failed";
        return core::Status::INTERNAL_ERROR;
    }

    std::vector<uint8_t> client_proof = client_key;
    security::xorBytes(client_proof, client_signature);
    std::string proof_b64 = security::base64Encode(client_proof);

    exchange.server_first = server_first;
    exchange.client_final = client_final_without_proof + ",p=" + proof_b64;

    if (!scramHmac(server_key, auth_message, algorithm, exchange.expected_server_signature)) {
        error_msg = "SCRAM server signature failed";
        return core::Status::INTERNAL_ERROR;
    }

    return core::Status::OK;
}

core::Status verifyScramServerFinal(const std::string& server_final,
                                    const ScramClientExchange& exchange,
                                    std::string& error_msg) {
    std::vector<uint8_t> signature;
    if (!parseScramServerFinal(server_final, signature)) {
        error_msg = "Invalid SCRAM server-final message";
        return core::Status::PROTOCOL_VIOLATION;
    }
    if (signature != exchange.expected_server_signature) {
        error_msg = "SCRAM server signature mismatch";
        return core::Status::INVALID_PASSWORD;
    }
    return core::Status::OK;
}

} // namespace

NetworkClient::NetworkClient() = default;
NetworkClient::~NetworkClient() = default;

NetworkPreparedStatement::NetworkPreparedStatement() = default;
NetworkPreparedStatement::~NetworkPreparedStatement() = default;

NetworkPreparedStatement::NetworkPreparedStatement(NetworkPreparedStatement&& other) noexcept = default;
NetworkPreparedStatement& NetworkPreparedStatement::operator=(NetworkPreparedStatement&& other) noexcept = default;

size_t NetworkPreparedStatement::getParameterCount() const {
    return param_count_;
}

bool NetworkPreparedStatement::isValid() const {
    return valid_;
}

void NetworkPreparedStatement::clearParameters() {
    for (auto& param : params_) {
        param = protocol::ProtocolCodec::ColumnValue(nullptr);
    }
    for (auto& type : param_types_) {
        type = protocol::WireType::UNKNOWN;
    }
}

void NetworkPreparedStatement::setNull(size_t index) {
    if (index == 0 || index > params_.size()) return;
    params_[index - 1] = protocol::ProtocolCodec::ColumnValue(nullptr);
    if (index <= param_types_.size()) {
        param_types_[index - 1] = protocol::WireType::NULL_TYPE;
    }
}

void NetworkPreparedStatement::setNull(size_t index, protocol::WireType type) {
    if (index == 0 || index > params_.size()) return;
    params_[index - 1] = protocol::ProtocolCodec::ColumnValue(nullptr);
    if (index <= param_types_.size()) {
        param_types_[index - 1] = type;
    }
}

void NetworkPreparedStatement::setBool(size_t index, bool value) {
    if (index == 0 || index > params_.size()) return;
    params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromBool(value);
    if (index <= param_types_.size()) {
        param_types_[index - 1] = protocol::WireType::BOOLEAN;
    }
}

void NetworkPreparedStatement::setInt16(size_t index, int16_t value) {
    if (index == 0 || index > params_.size()) return;
    params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromInt32(value);
    if (index <= param_types_.size()) {
        param_types_[index - 1] = protocol::WireType::INT16;
    }
}

void NetworkPreparedStatement::setInt32(size_t index, int32_t value) {
    if (index == 0 || index > params_.size()) return;
    params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromInt32(value);
    if (index <= param_types_.size()) {
        param_types_[index - 1] = protocol::WireType::INT32;
    }
}

void NetworkPreparedStatement::setInt64(size_t index, int64_t value) {
    if (index == 0 || index > params_.size()) return;
    params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromInt64(value);
    if (index <= param_types_.size()) {
        param_types_[index - 1] = protocol::WireType::INT64;
    }
}

void NetworkPreparedStatement::setFloat(size_t index, float value) {
    if (index == 0 || index > params_.size()) return;
    params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromDouble(value);
    if (index <= param_types_.size()) {
        param_types_[index - 1] = protocol::WireType::FLOAT32;
    }
}

void NetworkPreparedStatement::setDouble(size_t index, double value) {
    if (index == 0 || index > params_.size()) return;
    params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromDouble(value);
    if (index <= param_types_.size()) {
        param_types_[index - 1] = protocol::WireType::FLOAT64;
    }
}

void NetworkPreparedStatement::setString(size_t index, const std::string& value) {
    if (index == 0 || index > params_.size()) return;
    params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromString(value);
    if (index <= param_types_.size()) {
        param_types_[index - 1] = protocol::WireType::VARCHAR;
    }
}

void NetworkPreparedStatement::setBytes(size_t index, const std::vector<uint8_t>& value) {
    if (index == 0 || index > params_.size()) return;
    params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromBytes(value.data(), value.size());
    if (index <= param_types_.size()) {
        param_types_[index - 1] = protocol::WireType::BYTEA;
    }
}

void NetworkPreparedStatement::setBytes(size_t index, const uint8_t* data, size_t length) {
    if (index == 0 || index > params_.size()) return;
    params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromBytes(data, length);
    if (index <= param_types_.size()) {
        param_types_[index - 1] = protocol::WireType::BYTEA;
    }
}

void NetworkPreparedStatement::setTimestamp(size_t index, int64_t microseconds) {
    setInt64(index, microseconds);
    if (index <= param_types_.size()) {
        param_types_[index - 1] = protocol::WireType::TIMESTAMP;
    }
}

void NetworkPreparedStatement::setDate(size_t index, int32_t days) {
    setInt32(index, days);
    if (index <= param_types_.size()) {
        param_types_[index - 1] = protocol::WireType::DATE;
    }
}

void NetworkPreparedStatement::setTime(size_t index, int64_t microseconds) {
    setInt64(index, microseconds);
    if (index <= param_types_.size()) {
        param_types_[index - 1] = protocol::WireType::TIME;
    }
}

void NetworkPreparedStatement::setUUID(size_t index, const std::vector<uint8_t>& value) {
    if (index == 0 || index > params_.size()) return;
    params_[index - 1] = protocol::ProtocolCodec::ColumnValue::fromBytes(value.data(), value.size());
    if (index <= param_types_.size()) {
        param_types_[index - 1] = protocol::WireType::UUID;
    }
}

void NetworkPreparedStatement::setUUID(size_t index, const std::string& value) {
    setString(index, value);
    if (index <= param_types_.size()) {
        param_types_[index - 1] = protocol::WireType::UUID;
    }
}

void applyDriverDefaultsFromEnv(NetworkClientConfig& config) {
    const char* host = std::getenv("SCRATCHBIRD_DRIVER_HOST");
    if (host && (config.host.empty() || config.host == "127.0.0.1" || config.host == "localhost")) {
        config.host = host;
    }

    const char* port = std::getenv("SCRATCHBIRD_DRIVER_PORT");
    if (port && config.port == network::DEFAULT_NATIVE_PORT) {
        try {
            config.port = static_cast<uint16_t>(std::stoul(port));
        } catch (...) {
        }
    }

    const char* sslmode = std::getenv("SCRATCHBIRD_DRIVER_SSLMODE");
    if (sslmode) {
        config.ssl_mode = parseSslMode(sslmode);
    }

    const char* timeout_ms = std::getenv("SCRATCHBIRD_DRIVER_CONNECT_TIMEOUT_MS");
    if (timeout_ms && config.connect_timeout_ms == network::DEFAULT_CONNECT_TIMEOUT_MS) {
        try {
            config.connect_timeout_ms = static_cast<uint32_t>(std::stoul(timeout_ms));
        } catch (...) {
        }
    }

    const char* db = std::getenv("SCRATCHBIRD_DRIVER_DATABASE");
    if (db && config.database.empty()) {
        config.database = db;
    }

    const char* app = std::getenv("SCRATCHBIRD_DRIVER_APPLICATION_NAME");
    if (app && (config.application_name.empty() || config.application_name == "scratchbird_odbc")) {
        config.application_name = app;
    }

    const char* ssl_cert = std::getenv("SCRATCHBIRD_DRIVER_SSL_CERT");
    if (ssl_cert && config.ssl_cert.empty()) {
        config.ssl_cert = ssl_cert;
    }

    const char* ssl_key = std::getenv("SCRATCHBIRD_DRIVER_SSL_KEY");
    if (ssl_key && config.ssl_key.empty()) {
        config.ssl_key = ssl_key;
    }

    const char* ssl_root = std::getenv("SCRATCHBIRD_DRIVER_SSL_ROOT_CERT");
    if (ssl_root && config.ssl_root_cert.empty()) {
        config.ssl_root_cert = ssl_root;
    }
}

core::Status NetworkClient::connect(const NetworkClientConfig& config,
                                    core::ErrorContext* ctx) {
    config_ = config;
    applyDriverDefaultsFromEnv(config_);
    last_error_.clear();

    if (!network::isNetworkInitialized()) {
        if (!network::initNetwork()) {
            last_error_ = "Failed to initialize network subsystem";
            return core::Status::INTERNAL_ERROR;
        }
    }

    network::NetworkAddress address;
    address.family = network::AddressFamily::IPV4;
    address.host = config_.host;
    address.port = config_.port;

    network::SocketOptions options;
    options.connect_timeout_ms = config_.connect_timeout_ms;
    options.read_timeout_ms = config_.read_timeout_ms;
    options.write_timeout_ms = config_.write_timeout_ms;

    socket_ = network::Socket::connect(address, options, ctx);
    if (!socket_) {
        last_error_ = "Failed to connect to network listener";
        return core::Status::CONNECTION_FAILURE;
    }

    bool require_tls = config_.ssl_mode == network::SSLMode::REQUIRE ||
                       config_.ssl_mode == network::SSLMode::VERIFY_CA ||
                       config_.ssl_mode == network::SSLMode::VERIFY_FULL;

    if (config_.ssl_mode != network::SSLMode::DISABLED) {
        security::TLSClientConfig tls_cfg;
        tls_cfg.enabled = true;
        tls_cfg.min_version = security::TLSVersion::TLS_1_3;
        tls_cfg.max_version = security::TLSVersion::TLS_1_3;
        tls_cfg.cert_file = config_.ssl_cert;
        tls_cfg.key_file = config_.ssl_key;
        tls_cfg.ca_file = config_.ssl_root_cert;
        tls_cfg.use_system_ca = config_.ssl_root_cert.empty();

        if (config_.ssl_mode == network::SSLMode::VERIFY_CA ||
            config_.ssl_mode == network::SSLMode::VERIFY_FULL) {
            tls_cfg.verify_server = true;
            if (config_.ssl_mode == network::SSLMode::VERIFY_FULL) {
                tls_cfg.expected_hostname = config_.host;
            }
        } else {
            tls_cfg.verify_server = false;
        }

        tls_ctx_ = security::TLSContext::createClient(tls_cfg, ctx);
        if (!tls_ctx_) {
            last_error_ = ctx ? ctx->message : "Failed to initialize TLS context";
            if (require_tls) {
                return core::Status::CONNECTION_FAILURE;
            }
        } else {
            tls_conn_ = std::make_unique<security::TLSConnection>(*tls_ctx_);
            auto status = tls_conn_->setFd(socket_->getFd());
            if (status == core::Status::OK) {
                tls_conn_->setSNIHostname(config_.host);
                status = tls_conn_->connect();
            }

            if (status != core::Status::OK) {
                last_error_ = tls_conn_ ? tls_conn_->getLastErrorMessage() : "TLS handshake failed";
                tls_conn_.reset();
                tls_ctx_.reset();
                if (require_tls) {
                    return status;
                }
            } else {
                tls_active_ = true;
            }
        }
    }

    // CONNECT_REQUEST
    auto connect_msg = protocol::ProtocolCodec::buildConnectRequest(
        config_.database,
        config_.application_name,
        getProcessId()
    );

    auto status = sendMessage(connect_msg, ctx);
    if (status != core::Status::OK) {
        last_error_ = "Failed to send CONNECT_REQUEST";
        return status;
    }

    protocol::Message response;
    status = receiveMessage(response, ctx);
    if (status != core::Status::OK) {
        last_error_ = "Failed to receive CONNECT_RESPONSE";
        return status;
    }

    if (response.getType() != protocol::MessageType::CONNECT_RESPONSE) {
        last_error_ = "Unexpected response to CONNECT_REQUEST";
        return core::Status::PROTOCOL_VIOLATION;
    }

    bool success = false;
    std::string error_msg;
    status = protocol::ProtocolCodec::parseConnectResponse(
        response, success, session_id_, error_msg, ctx
    );
    if (status != core::Status::OK || !success) {
        last_error_ = error_msg.empty() ? "Connection refused" : error_msg;
        return core::Status::CONNECTION_FAILURE;
    }

    if (!config_.username.empty()) {
        auto do_password_auth = [&]() -> core::Status {
            auto auth_msg = protocol::ProtocolCodec::buildAuthRequest(
                session_id_,
                config_.username,
                config_.password
            );
            auto status = sendMessage(auth_msg, ctx);
            if (status != core::Status::OK) {
                last_error_ = "Failed to send AUTH_REQUEST";
                return status;
            }

            protocol::Message response;
            status = receiveMessage(response, ctx);
            if (status != core::Status::OK) {
                last_error_ = "Failed to receive AUTH_RESPONSE";
                return status;
            }

            if (response.getType() != protocol::MessageType::AUTH_RESPONSE) {
                last_error_ = "Unexpected response to AUTH_REQUEST";
                return core::Status::PROTOCOL_VIOLATION;
            }

            bool success = false;
            uint32_t user_id = 0;
            std::string error_msg;
            status = protocol::ProtocolCodec::parseAuthResponse(
                response, success, user_id, error_msg, ctx
            );
            if (status != core::Status::OK || !success) {
                last_error_ = error_msg.empty() ? "Authentication failed" : error_msg;
                return core::Status::INVALID_PASSWORD;
            }
            return core::Status::OK;
        };

        auto do_scram_auth = [&](protocol::AuthMethod method) -> core::Status {
            ScramClientExchange exchange;
            std::string error_msg;
            auto status = buildScramClientFirst(config_.username, exchange, error_msg);
            if (status != core::Status::OK) {
                last_error_ = error_msg;
                return status;
            }

            std::string client_first = "n,," + exchange.client_first_bare;
            std::vector<uint8_t> client_first_bytes(client_first.begin(), client_first.end());
            auto auth_msg = protocol::ProtocolCodec::buildAuthRequest(
                session_id_,
                config_.username,
                method,
                client_first_bytes
            );
            status = sendMessage(auth_msg, ctx);
            if (status != core::Status::OK) {
                last_error_ = "Failed to send SCRAM client-first";
                return status;
            }

            protocol::Message response;
            status = receiveMessage(response, ctx);
            if (status != core::Status::OK) {
                last_error_ = "Failed to receive SCRAM server-first";
                return status;
            }

            if (response.getType() != protocol::MessageType::AUTH_RESPONSE) {
                last_error_ = "Unexpected response to SCRAM client-first";
                return core::Status::PROTOCOL_VIOLATION;
            }

            protocol::AuthStatus auth_status = protocol::AuthStatus::ERROR;
            uint32_t user_id = 0;
            std::string auth_error;
            std::vector<uint8_t> data;
            status = protocol::ProtocolCodec::parseAuthResponse(
                response, auth_status, user_id, auth_error, &data, ctx
            );
            if (status != core::Status::OK) {
                last_error_ = "Failed to parse SCRAM server-first";
                return status;
            }
            if (auth_status != protocol::AuthStatus::CONTINUE) {
                last_error_ = auth_error.empty() ? "SCRAM authentication failed" : auth_error;
                return core::Status::INVALID_PASSWORD;
            }

            std::string server_first(data.begin(), data.end());
            security::ScramAlgorithm algorithm =
                (method == protocol::AuthMethod::SCRAM_SHA_512)
                    ? security::ScramAlgorithm::SHA_512
                    : security::ScramAlgorithm::SHA_256;

            status = handleScramServerFirst(
                config_.password,
                algorithm,
                exchange,
                server_first,
                error_msg
            );
            if (status != core::Status::OK) {
                last_error_ = error_msg;
                return status;
            }

            std::vector<uint8_t> client_final_bytes(exchange.client_final.begin(),
                                                     exchange.client_final.end());
            auth_msg = protocol::ProtocolCodec::buildAuthRequest(
                session_id_,
                config_.username,
                method,
                client_final_bytes
            );
            status = sendMessage(auth_msg, ctx);
            if (status != core::Status::OK) {
                last_error_ = "Failed to send SCRAM client-final";
                return status;
            }

            status = receiveMessage(response, ctx);
            if (status != core::Status::OK) {
                last_error_ = "Failed to receive SCRAM server-final";
                return status;
            }

            if (response.getType() != protocol::MessageType::AUTH_RESPONSE) {
                last_error_ = "Unexpected response to SCRAM client-final";
                return core::Status::PROTOCOL_VIOLATION;
            }

            auth_status = protocol::AuthStatus::ERROR;
            auth_error.clear();
            data.clear();
            status = protocol::ProtocolCodec::parseAuthResponse(
                response, auth_status, user_id, auth_error, &data, ctx
            );
            if (status != core::Status::OK) {
                last_error_ = "Failed to parse SCRAM server-final";
                return status;
            }
            if (auth_status != protocol::AuthStatus::OK) {
                last_error_ = auth_error.empty() ? "SCRAM authentication failed" : auth_error;
                return core::Status::INVALID_PASSWORD;
            }

            std::string server_final(data.begin(), data.end());
            status = verifyScramServerFinal(server_final, exchange, error_msg);
            if (status != core::Status::OK) {
                last_error_ = error_msg;
                return status;
            }

            return core::Status::OK;
        };

        core::Status auth_status = core::Status::OK;
        if (config_.auth_method == protocol::AuthMethod::SCRAM_SHA_256 ||
            config_.auth_method == protocol::AuthMethod::SCRAM_SHA_512) {
            auth_status = do_scram_auth(config_.auth_method);
            if (auth_status != core::Status::OK && config_.allow_password_fallback) {
                auth_status = do_password_auth();
            }
        } else {
            auth_status = do_password_auth();
        }

        if (auth_status != core::Status::OK) {
            return auth_status;
        }
    }

    connected_ = true;
    in_transaction_ = false;
    return core::Status::OK;
}

void NetworkClient::disconnect() {
    if (!socket_) {
        connected_ = false;
        return;
    }

    protocol::Message msg = protocol::ProtocolCodec::buildDisconnect();
    sendMessage(msg, nullptr);

    if (tls_conn_) {
        tls_conn_->shutdown();
    }
    tls_conn_.reset();
    tls_ctx_.reset();
    tls_active_ = false;

    socket_->close();
    socket_.reset();
    connected_ = false;
    in_transaction_ = false;
}

bool NetworkClient::isConnected() const {
    return connected_ && socket_ && socket_->isOpen();
}

core::Status NetworkClient::executeQuery(const std::string& sql,
                                         NetworkResultSet& results,
                                         core::ErrorContext* ctx) {
    results.columns.clear();
    results.rows.clear();
    results.rows_affected = 0;
    results.command_tag.clear();
    if (ctx) {
        ctx->code = core::Status::OK;
        ctx->sqlstate = core::SQLSTATE_SUCCESS;
        ctx->message.clear();
    }

    if (!isConnected()) {
        last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    auto query_msg = protocol::ProtocolCodec::buildQuery(session_id_, sql, 0);
    auto status = sendMessage(query_msg, ctx);
    if (status != core::Status::OK) {
        last_error_ = "Failed to send QUERY";
        return status;
    }

    const uint32_t copy_window = config_.copy_window_bytes == 0 ? 65536 : config_.copy_window_bytes;
    const uint32_t copy_chunk = config_.copy_chunk_bytes == 0 ? 16384 : config_.copy_chunk_bytes;

    auto handle_copy_out = [&]() -> core::Status {
        std::ostream* out = copy_output_stream_ ? copy_output_stream_ : &std::cout;
        uint32_t window = 0;
        bool stream_ready = false;

        while (true) {
            protocol::Message response;
            auto status = receiveMessage(response, ctx);
            if (status != core::Status::OK) {
                last_error_ = "Failed to receive COPY OUT response";
                return status;
            }

            switch (response.getType()) {
                case protocol::MessageType::STREAM_READY: {
                    stream_ready = true;
                    window = copy_window;
                    auto ctrl = protocol::ProtocolCodec::buildStreamControl(
                        protocol::StreamControlType::START, window, 0);
                    status = sendMessage(ctrl, ctx);
                    if (status != core::Status::OK) {
                        last_error_ = "Failed to send STREAM_CONTROL";
                        return status;
                    }
                    break;
                }
                case protocol::MessageType::COPY_DATA: {
                    const uint8_t* data = nullptr;
                    size_t len = 0;
                    protocol::ProtocolCodec::parseCopyData(response, &data, &len, ctx);
                    if (len > 0) {
                        out->write(reinterpret_cast<const char*>(data),
                                   static_cast<std::streamsize>(len));
                        if (!(*out)) {
                            last_error_ = "COPY OUT write failed";
                            return core::Status::IO_ERROR;
                        }
                    }
                    if (window > 0) {
                        if (len >= window) {
                            window = 0;
                        } else {
                            window -= static_cast<uint32_t>(len);
                        }
                    }
                    if (stream_ready && window == 0) {
                        window = copy_window;
                        auto ctrl = protocol::ProtocolCodec::buildStreamControl(
                            protocol::StreamControlType::ACK, window, 0);
                        status = sendMessage(ctrl, ctx);
                        if (status != core::Status::OK) {
                            last_error_ = "Failed to send STREAM_CONTROL ACK";
                            return status;
                        }
                    }
                    break;
                }
                case protocol::MessageType::COPY_DONE:
                    return core::Status::OK;

                case protocol::MessageType::COPY_FAIL: {
                    std::string message;
                    protocol::ProtocolCodec::parseCopyFail(response, message, ctx);
                    last_error_ = message.empty() ? "COPY OUT failed" : message;
                    return core::Status::INTERNAL_ERROR;
                }
                case protocol::MessageType::QUERY_ERROR: {
                    std::string message;
                    status = mapQueryError(response, message, ctx);
                    last_error_ = message;
                    return status;
                }
                case protocol::MessageType::STREAM_END:
                    break;

                default:
                    break;
            }
        }
    };

    auto handle_copy_in = [&]() -> core::Status {
        std::istream* in = copy_input_stream_ ? copy_input_stream_ : &std::cin;
        uint32_t window = 0;
        bool done = false;
        bool stream_started = false;

        while (!done) {
            if (window == 0) {
                protocol::Message response;
                auto status = receiveMessage(response, ctx);
                if (status != core::Status::OK) {
                    last_error_ = "Failed to receive COPY IN control";
                    return status;
                }

                switch (response.getType()) {
                    case protocol::MessageType::STREAM_READY:
                        if (!stream_started) {
                            window = copy_window;
                            auto ctrl = protocol::ProtocolCodec::buildStreamControl(
                                protocol::StreamControlType::START, window, 0);
                            status = sendMessage(ctrl, ctx);
                            if (status != core::Status::OK) {
                                last_error_ = "Failed to send STREAM_CONTROL START";
                                return status;
                            }
                            stream_started = true;
                        }
                        break;
                    case protocol::MessageType::STREAM_CONTROL: {
                        protocol::StreamControlType control;
                        uint32_t new_window = 0;
                        uint32_t timeout_ms = 0;
                        status = protocol::ProtocolCodec::parseStreamControl(
                            response, control, new_window, timeout_ms, ctx);
                        if (status != core::Status::OK) {
                            last_error_ = "Malformed STREAM_CONTROL";
                            return status;
                        }
                        (void)timeout_ms;
                        if (control == protocol::StreamControlType::PAUSE) {
                            break;
                        }
                        if (control == protocol::StreamControlType::CANCEL) {
                            last_error_ = "COPY IN canceled by server";
                            return core::Status::CANCELLED;
                        }
                        window += new_window;
                        break;
                    }
                    case protocol::MessageType::COPY_FAIL: {
                        std::string message;
                        protocol::ProtocolCodec::parseCopyFail(response, message, ctx);
                        last_error_ = message.empty() ? "COPY IN failed" : message;
                        return core::Status::INTERNAL_ERROR;
                    }
                    case protocol::MessageType::QUERY_ERROR: {
                        std::string message;
                        status = mapQueryError(response, message, ctx);
                        last_error_ = message;
                        return status;
                    }
                    default:
                        break;
                }
                if (window == 0) {
                    continue;
                }
            }

            size_t to_read = std::min<size_t>(window, copy_chunk);
            std::string buffer(to_read, '\0');
            in->read(buffer.data(), static_cast<std::streamsize>(to_read));
            std::streamsize got = in->gcount();

            if (got > 0) {
                auto msg = protocol::ProtocolCodec::buildCopyData(
                    reinterpret_cast<const uint8_t*>(buffer.data()),
                    static_cast<size_t>(got));
                auto status = sendMessage(msg, ctx);
                if (status != core::Status::OK) {
                    last_error_ = "Failed to send COPY_DATA";
                    return status;
                }
                if (got >= static_cast<std::streamsize>(window)) {
                    window = 0;
                } else {
                    window -= static_cast<uint32_t>(got);
                }
            } else {
                auto msg = protocol::ProtocolCodec::buildCopyDone();
                auto status = sendMessage(msg, ctx);
                if (status != core::Status::OK) {
                    last_error_ = "Failed to send COPY_DONE";
                    return status;
                }
                done = true;
            }
        }
        return core::Status::OK;
    };

    while (true) {
        protocol::Message response;
        status = receiveMessage(response, ctx);
        if (status != core::Status::OK) {
            last_error_ = "Failed to receive response";
            return status;
        }

        switch (response.getType()) {
            case protocol::MessageType::QUERY_ERROR: {
                std::string message;
                status = mapQueryError(response, message, ctx);
                last_error_ = message;
                return status;
            }
            case protocol::MessageType::ROW_DESCRIPTION: {
                std::vector<protocol::ProtocolCodec::ColumnInfo> cols;
                protocol::ProtocolCodec::parseRowDescription(response, cols, ctx);
                results.columns.clear();
                results.columns.reserve(cols.size());
                for (const auto& col : cols) {
                    NetworkColumn out;
                    out.name = col.name;
                    out.type = col.type;
                    out.type_modifier = col.type_modifier;
                    results.columns.push_back(std::move(out));
                }
                break;
            }
            case protocol::MessageType::ROW_DATA: {
                std::vector<protocol::ProtocolCodec::ColumnValue> values;
                protocol::ProtocolCodec::parseRowData(response, values, ctx);
                results.rows.push_back(std::move(values));
                break;
            }
            case protocol::MessageType::COMMAND_COMPLETE: {
                std::string tag;
                int64_t rows_affected = 0;
                protocol::ProtocolCodec::parseCommandComplete(
                    response, tag, rows_affected, ctx
                );
                results.command_tag = tag;
                results.rows_affected = rows_affected;
                break;
            }
            case protocol::MessageType::END_OF_RESULTS:
                return core::Status::OK;

            case protocol::MessageType::COPY_IN_RESPONSE: {
                status = handle_copy_in();
                if (status != core::Status::OK) {
                    return status;
                }
                break;
            }
            case protocol::MessageType::COPY_OUT_RESPONSE: {
                status = handle_copy_out();
                if (status != core::Status::OK) {
                    return status;
                }
                break;
            }
            case protocol::MessageType::COPY_FAIL: {
                std::string message;
                protocol::ProtocolCodec::parseCopyFail(response, message, ctx);
                last_error_ = message.empty() ? "COPY failed" : message;
                return core::Status::INTERNAL_ERROR;
            }
            case protocol::MessageType::STREAM_END:
            case protocol::MessageType::STREAM_READY:
                break;

            case protocol::MessageType::TRANSACTION_STATUS: {
                if (response.getPayloadSize() >= sizeof(protocol::TransactionStatusPayload)) {
                    const auto* ts_payload = reinterpret_cast<const protocol::TransactionStatusPayload*>(
                        response.getPayload());
                    in_transaction_ = (ts_payload->status == 1);
                }
                break;
            }
            default:
                break;
        }
    }
}

core::Status NetworkClient::prepare(const std::string& sql,
                                    NetworkPreparedStatement& stmt,
                                    core::ErrorContext* ctx) {
    if (!isConnected()) {
        last_error_ = "Not connected";
        return core::Status::CONNECTION_FAILURE;
    }

    (void)ctx;
    stmt.sql_ = sql;
    stmt.param_count_ = countParameters(sql);
    stmt.params_.assign(stmt.param_count_, protocol::ProtocolCodec::ColumnValue(nullptr));
    stmt.param_types_.assign(stmt.param_count_, protocol::WireType::UNKNOWN);
    stmt.valid_ = true;
    return core::Status::OK;
}

core::Status NetworkClient::executePrepared(NetworkPreparedStatement& stmt,
                                            NetworkResultSet& results,
                                            core::ErrorContext* ctx) {
    if (!stmt.isValid()) {
        last_error_ = "Invalid prepared statement";
        return core::Status::INVALID_ARGUMENT;
    }

    std::string sql = substituteParameters(stmt.sql_, stmt.params_, stmt.param_types_);
    return executeQuery(sql, results, ctx);
}

core::Status NetworkClient::beginTransaction(core::ErrorContext* ctx) {
    auto msg = protocol::ProtocolCodec::buildBeginTransaction(session_id_, 0, false);
    auto status = sendMessage(msg, ctx);
    if (status != core::Status::OK) {
        last_error_ = "Failed to send BEGIN";
        return status;
    }

    protocol::Message response;
    status = receiveMessage(response, ctx);
    if (status != core::Status::OK) {
        last_error_ = "Failed to receive BEGIN response";
        return status;
    }

    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        std::string message;
        status = mapQueryError(response, message, ctx);
        last_error_ = message;
        return status;
    }

    in_transaction_ = true;
    return core::Status::OK;
}

core::Status NetworkClient::commit(core::ErrorContext* ctx) {
    auto msg = protocol::ProtocolCodec::buildCommit(session_id_);
    auto status = sendMessage(msg, ctx);
    if (status != core::Status::OK) {
        last_error_ = "Failed to send COMMIT";
        return status;
    }

    protocol::Message response;
    status = receiveMessage(response, ctx);
    if (status != core::Status::OK) {
        last_error_ = "Failed to receive COMMIT response";
        return status;
    }

    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        std::string message;
        status = mapQueryError(response, message, ctx);
        last_error_ = message;
        return status;
    }

    in_transaction_ = false;
    return core::Status::OK;
}

core::Status NetworkClient::rollback(core::ErrorContext* ctx) {
    auto msg = protocol::ProtocolCodec::buildRollback(session_id_);
    auto status = sendMessage(msg, ctx);
    if (status != core::Status::OK) {
        last_error_ = "Failed to send ROLLBACK";
        return status;
    }

    protocol::Message response;
    status = receiveMessage(response, ctx);
    if (status != core::Status::OK) {
        last_error_ = "Failed to receive ROLLBACK response";
        return status;
    }

    if (response.getType() == protocol::MessageType::QUERY_ERROR) {
        std::string message;
        status = mapQueryError(response, message, ctx);
        last_error_ = message;
        return status;
    }

    in_transaction_ = false;
    return core::Status::OK;
}

core::Status NetworkClient::sendMessage(const protocol::Message& msg,
                                        core::ErrorContext* ctx) {
    if (!socket_ || !socket_->isOpen()) {
        SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE, "Connection closed");
        return core::Status::CONNECTION_FAILURE;
    }

    std::vector<uint8_t> buffer;
    auto status = msg.serialize(buffer);
    if (status != core::Status::OK) {
        return status;
    }

    if (tls_active_ && tls_conn_) {
        size_t offset = 0;
        while (offset < buffer.size()) {
            int written = tls_conn_->write(buffer.data() + offset,
                                           static_cast<int>(buffer.size() - offset));
            if (written <= 0) {
                SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "TLS write failed");
                return core::Status::IO_ERROR;
            }
            offset += static_cast<size_t>(written);
        }
        return core::Status::OK;
    }

    return socket_->writeExact(buffer.data(), buffer.size(), ctx);
}

core::Status NetworkClient::receiveMessage(protocol::Message& msg,
                                           core::ErrorContext* ctx) {
    if (!socket_ || !socket_->isOpen()) {
        SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE, "Connection closed");
        return core::Status::CONNECTION_FAILURE;
    }

    uint8_t header_buf[sizeof(protocol::MessageHeader)];
    core::Status status = readExactWithTimeout(header_buf, sizeof(header_buf), ctx);
    if (status != core::Status::OK) {
        return status;
    }

    protocol::MessageHeader header;
    status = protocol::Message::parseHeader(header_buf, header, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    msg = protocol::Message(static_cast<protocol::MessageType>(header.type));
    msg.setFlags(header.flags);

    if (header.payload_length > 0) {
        std::vector<uint8_t> payload_buf(header.payload_length);
        status = readExactWithTimeout(payload_buf.data(), header.payload_length, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        msg.setPayload(payload_buf.data(), header.payload_length);
    }

    return core::Status::OK;
}

core::Status NetworkClient::readExactWithTimeout(void* buffer, size_t size,
                                                 core::ErrorContext* ctx) {
    if (!socket_ || !socket_->isOpen()) {
        SET_ERROR_CONTEXT(ctx, core::Status::CONNECTION_FAILURE, "Connection closed");
        return core::Status::CONNECTION_FAILURE;
    }

    uint8_t* ptr = static_cast<uint8_t*>(buffer);
    size_t total_read = 0;
    const uint32_t timeout_ms = config_.read_timeout_ms;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    while (total_read < size) {
        uint32_t slice = timeout_ms;
        if (timeout_ms > 0) {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "Read timeout");
                return core::Status::IO_ERROR;
            }
            slice = static_cast<uint32_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count());
        }

        if (tls_active_ && tls_conn_) {
            if (timeout_ms > 0) {
                if (!socket_->waitReadable(static_cast<int>(slice))) {
                    SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "Read timeout");
                    return core::Status::IO_ERROR;
                }
            }
            int read = tls_conn_->read(ptr + total_read,
                                       static_cast<int>(size - total_read));
            if (read <= 0) {
                SET_ERROR_CONTEXT(ctx, core::Status::IO_ERROR, "TLS read failed");
                return core::Status::IO_ERROR;
            }
            total_read += static_cast<size_t>(read);
            continue;
        }

        size_t bytes_read = 0;
        core::Status status;
        if (timeout_ms > 0) {
            status = socket_->readWithTimeout(ptr + total_read, size - total_read,
                                              &bytes_read, slice, ctx);
        } else {
            status = socket_->read(ptr + total_read, size - total_read, &bytes_read, ctx);
        }
        if (status != core::Status::OK) {
            return status;
        }
        if (bytes_read == 0) {
            continue;
        }
        total_read += bytes_read;
    }

    return core::Status::OK;
}

} // namespace client
} // namespace scratchbird
