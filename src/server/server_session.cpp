/**
 * ScratchBird Server Session Implementation
 *
 * Local Server Architecture - Phase 3
 */

#include "scratchbird/server/server_session.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/bytecode_validator.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include "scratchbird/sblr/firebird_query_compiler.h"
#include "scratchbird/sblr/postgresql_query_compiler.h"
#include "scratchbird/sblr/mysql_query_compiler.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/auth_provider.h"
#include "scratchbird/core/audit_logger.h"
#include "scratchbird/security/scram_auth.h"

#include <cstring>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace scratchbird {
namespace server {

// ============================================================================
// ServerSession Implementation
// ============================================================================

ServerSession::ServerSession(IPCConnection* connection,
                             core::Database* database,
                             const uint8_t session_id[16])
    : connection_(connection)
    , database_(database)
    , state_(SessionState::CREATED)
    , shutdown_requested_(false)
{
    std::memcpy(session_id_, session_id, 16);

    // Create protocol session
    protocol_session_ = std::make_unique<protocol::ProtocolSession>(connection);

    // Initialize statistics
    stats_.created_at = std::chrono::steady_clock::now();
    stats_.last_activity = stats_.created_at;

    // Get client info if available
    auto peer = connection->getPeerCredentials();
    if (peer.available) {
        std::ostringstream oss;
        oss << "pid=" << peer.pid << ",uid=" << peer.uid << ",gid=" << peer.gid;
        client_info_ = oss.str();
    } else {
        client_info_ = "unknown";
    }

    // Create executor
    executor_ = std::make_unique<sblr::Executor>(database_);
}

ServerSession::~ServerSession() {
    // Ensure session is closed
    if (state_ != SessionState::CLOSED) {
        state_ = SessionState::CLOSED;
    }
}

std::string ServerSession::sessionIdString() const {
    return protocol::sessionIdToString(session_id_);
}

core::Status ServerSession::run() {
    state_ = SessionState::CREATED;

    protocol::Message msg;
    core::ErrorContext ctx;

    while (!shutdown_requested_ && connection_->isOpen()) {
        // Receive next message
        core::Status status = protocol_session_->receiveMessage(msg, &ctx);

        if (status != core::Status::OK) {
            if (status == core::Status::CONNECTION_FAILURE) {
                // Clean disconnect
                break;
            }
            // Error receiving message
            sendError("Protocol error: " + ctx.message);
            break;
        }

        // Update activity timestamp
        stats_.last_activity = std::chrono::steady_clock::now();

        // Process the message
        status = processMessage(msg, &ctx);

        if (status != core::Status::OK) {
            // Don't break on most errors, just continue
            // Only break on serious protocol/connection errors
            if (status == core::Status::CONNECTION_FAILURE ||
                status == core::Status::PROTOCOL_VIOLATION) {
                break;
            }
        }

        // Check for disconnect
        if (state_ == SessionState::CLOSING || state_ == SessionState::CLOSED) {
            break;
        }
    }

    // If the client vanished without a DISCONNECT message, preserve the transaction if possible.
    if (conn_ctx_ && state_ != SessionState::CLOSED && state_ != SessionState::CLOSING) {
        fireDatabaseTriggers(core::CatalogManager::DatabaseTriggerEvent::ON_DISCONNECT);

        core::ErrorContext detach_ctx;
        core::ID dormant_id;
        core::Status detach_status = database_->detachToDormant(conn_ctx_, dormant_id, &detach_ctx);
        if (detach_status != core::Status::OK) {
            conn_ctx_->shutdownTransaction(&detach_ctx);
            conn_ctx_.reset();
        }
    }

    static const core::ID zero_id{};
    if (database_ && database_->catalog_manager() && session_id_uuid_ != zero_id) {
        core::ErrorContext close_ctx;
        database_->catalog_manager()->closeSession(session_id_uuid_, &close_ctx);
    }

    state_ = SessionState::CLOSED;
    return core::Status::OK;
}

void ServerSession::requestShutdown() {
    shutdown_requested_ = true;
    state_ = SessionState::CLOSING;
}

core::Status ServerSession::processMessage(const protocol::Message& msg, core::ErrorContext* ctx) {
    protocol::MessageType type = msg.getType();

    switch (type) {
        case protocol::MessageType::CONNECT_REQUEST:
            return handleConnect(msg, ctx);

        case protocol::MessageType::AUTH_REQUEST:
            return handleAuth(msg, ctx);

        case protocol::MessageType::DISCONNECT:
            return handleDisconnect(msg, ctx);

        case protocol::MessageType::QUERY:
            return handleQuery(msg, ctx);

        case protocol::MessageType::BEGIN_TRANSACTION:
        case protocol::MessageType::COMMIT:
        case protocol::MessageType::ROLLBACK:
        case protocol::MessageType::SAVEPOINT:
        case protocol::MessageType::RELEASE_SAVEPOINT:
        case protocol::MessageType::ROLLBACK_TO:
            return handleTransaction(msg, ctx);

        case protocol::MessageType::PING:
            return handlePing(msg, ctx);

        case protocol::MessageType::QUERY_CANCEL:
            return handleCancel(msg, ctx);

        default:
            sendError("Unsupported message type: " +
                     std::string(protocol::messageTypeToString(type)));
            return core::Status::INVALID_ARGUMENT;
    }
}

core::Status ServerSession::handleConnect(const protocol::Message& msg, core::ErrorContext* ctx) {
    // Parse connect request using the ProtocolCodec
    std::string database, client_name;
    uint32_t client_pid;

    core::Status status = protocol::ProtocolCodec::parseConnectRequest(
        msg, database, client_name, client_pid, ctx);

    if (status != core::Status::OK) {
        sendError("Invalid connect request");
        return status;
    }

    client_info_ = client_name + " (pid=" + std::to_string(client_pid) + ")";

    // Check if database is open
    if (!database_->is_open()) {
        // Send failure response
        protocol::Message response = protocol::ProtocolCodec::buildConnectResponse(
            false, session_id_, "Database is not open");
        protocol_session_->sendMessage(response, ctx);
        return core::Status::IO_ERROR;
    }

    // Send success response with session ID
    protocol::Message response = protocol::ProtocolCodec::buildConnectResponse(
        true, session_id_, "Connected to ScratchBird");
    status = protocol_session_->sendMessage(response, ctx);

    if (status == core::Status::OK) {
        state_ = SessionState::AUTHENTICATING;
    }

    return status;
}

core::Status ServerSession::handleAuth(const protocol::Message& msg, core::ErrorContext* ctx) {
    if (state_ != SessionState::AUTHENTICATING && state_ != SessionState::CREATED) {
        sendError("Authentication not expected in current state");
        return core::Status::INVALID_ARGUMENT;
    }

    // Parse auth request using ProtocolCodec
    uint8_t session_id[16];
    std::string username;
    protocol::AuthMethod auth_method = protocol::AuthMethod::PASSWORD;
    std::vector<uint8_t> auth_payload;

    core::Status status = protocol::ProtocolCodec::parseAuthRequest(
        msg, session_id, username, auth_method, auth_payload, ctx);

    if (status != core::Status::OK) {
        sendError("Invalid auth request");
        return status;
    }

    auto finish_auth = [&](const core::AuthUserInfo& user_info,
                           const std::vector<uint8_t>& response_data) -> core::Status {
        username_ = user_info.username.empty() ? username : user_info.username;
        state_ = SessionState::AUTHENTICATED;

        // Create connection context
        core::Status connect_status = database_->connect(conn_ctx_, ctx);
        if (connect_status != core::Status::OK || !conn_ctx_) {
            sendError("Failed to initialize connection context");
            return connect_status;
        }

        // Preserve the protocol session UUID for dormant reattach diagnostics.
        core::ID protocol_session_id;
        std::memcpy(protocol_session_id.bytes.data(), session_id_, 16);
        conn_ctx_->setProtocolSessionId(protocol_session_id);

        // Set user with ID and superuser flag
        conn_ctx_->setCurrentUser(user_info.user_id, user_info.is_superuser);

        // Create and bind catalog session
        auto* catalog = database_->catalog_manager();
        if (catalog) {
            core::CatalogManager::SessionInfo session_info;
            core::Status session_status = catalog->createSession(
                user_info.user_id, user_info.authkey_id, conn_ctx_->emulationMode(),
                session_info, ctx);
            if (session_status != core::Status::OK) {
                sendError("Failed to create session");
                return session_status;
            }

            session_id_uuid_ = session_info.session_id;
            authkey_id_ = user_info.authkey_id;

            conn_ctx_->setCurrentSchemaId(session_info.current_schema_id);
            conn_ctx_->setSessionContext(session_info.session_id,
                                         user_info.authkey_id,
                                         session_info.emulation_mode,
                                         session_info.policy_epoch_global,
                                         session_info.policy_epoch_table);
        } else {
            authkey_id_ = user_info.authkey_id;
            session_id_uuid_ = core::generateUuidV7();
            conn_ctx_->setSessionContext(session_id_uuid_,
                                         authkey_id_,
                                         conn_ctx_->emulationMode(),
                                         0,
                                         0);
        }

        executor_->setConnectionContext(conn_ctx_.get());

        // Fire ON CONNECT database triggers (Firebird-style)
        fireDatabaseTriggers(core::CatalogManager::DatabaseTriggerEvent::ON_CONNECT);

        // Audit login success with session/authkey context
        auto* audit_logger = database_->audit_logger();
        if (audit_logger) {
            core::AuditEvent event = core::AuditLogger::createLoginSuccessEvent(
                user_info.user_id, username_);
            event.session_id = session_id_uuid_;
            event.authkey_id = authkey_id_;
            core::ErrorContext audit_ctx;
            audit_logger->logEvent(event, &audit_ctx);
        }

        // Send success response (use first 4 bytes of UUID as uint32 for wire protocol)
        uint32_t user_id_wire = 0;
        std::memcpy(&user_id_wire, user_info.user_id.bytes.data(),
                    std::min<size_t>(4, user_info.user_id.bytes.size()));
        protocol::Message response = protocol::ProtocolCodec::buildAuthResponse(
            protocol::AuthStatus::OK, user_id_wire, "", response_data);
        return protocol_session_->sendMessage(response, ctx);
    };

    core::AuthUserInfo user_info;
    std::string auth_error;

    auto* catalog = database_ ? database_->catalog_manager() : nullptr;
    auto* audit_logger = database_ ? database_->audit_logger() : nullptr;
    std::unique_ptr<core::AuthProvider> provider;
    if (catalog) {
        provider = core::AuthProviderFactory::createDefault(catalog, audit_logger);
    }

    if (auth_method == protocol::AuthMethod::PASSWORD) {
        std::string password(reinterpret_cast<const char*>(auth_payload.data()), auth_payload.size());
        core::AuthResult auth_result = authenticate(username, password, user_info, auth_error);

        if (auth_result == core::AuthResult::SUCCESS) {
            return finish_auth(user_info, {});
        }
    } else if (auth_method == protocol::AuthMethod::MD5) {
        if (auth_payload.size() < 4 || !provider) {
            auth_error = "Authentication failed";
        } else {
            uint8_t salt[4];
            std::memcpy(salt, auth_payload.data(), 4);
            std::string response(reinterpret_cast<const char*>(auth_payload.data() + 4),
                                 auth_payload.size() - 4);
            core::AuthResult auth_result = provider->authenticateMd5(
                username, salt, response, user_info, auth_error);
            if (auth_result == core::AuthResult::SUCCESS) {
                return finish_auth(user_info, {});
            }
        }
    } else if (auth_method == protocol::AuthMethod::SCRAM_SHA_256 ||
               auth_method == protocol::AuthMethod::SCRAM_SHA_512) {
        if (!provider) {
            auth_error = "Authentication failed";
        } else if (!scram_state_) {
            core::ScramAuthState state;
            std::string server_first;
            std::string client_first(reinterpret_cast<const char*>(auth_payload.data()),
                                     auth_payload.size());
            auto algo = (auth_method == protocol::AuthMethod::SCRAM_SHA_256)
                ? security::ScramAlgorithm::SHA_256
                : security::ScramAlgorithm::SHA_512;
            core::AuthResult auth_result = provider->beginScramAuth(
                username, client_first, algo, state, server_first, auth_error);
            if (auth_result == core::AuthResult::SUCCESS) {
                scram_state_ = std::move(state);
                std::vector<uint8_t> response_data(server_first.begin(), server_first.end());
                protocol::Message response = protocol::ProtocolCodec::buildAuthResponse(
                    protocol::AuthStatus::CONTINUE, 0, "", response_data);
                return protocol_session_->sendMessage(response, ctx);
            }
        } else {
            std::string server_final;
            std::string client_final(reinterpret_cast<const char*>(auth_payload.data()),
                                     auth_payload.size());
            core::AuthResult auth_result = provider->finishScramAuth(
                *scram_state_, client_final, user_info, server_final, auth_error);
            scram_state_.reset();
            if (auth_result == core::AuthResult::SUCCESS) {
                std::vector<uint8_t> response_data(server_final.begin(), server_final.end());
                return finish_auth(user_info, response_data);
            }
        }
    } else {
        auth_error = "Authentication failed";
    }

    stats_.queries_failed++;

    protocol::Message response = protocol::ProtocolCodec::buildAuthResponse(
        protocol::AuthStatus::ERROR, 0, "Authentication failed");
    protocol_session_->sendMessage(response, ctx);
    return core::Status::INVALID_PASSWORD;
}

core::Status ServerSession::handleDisconnect(const protocol::Message& msg, core::ErrorContext* ctx) {
    state_ = SessionState::CLOSING;

    // Fire ON DISCONNECT database triggers (Firebird-style) before closing
    fireDatabaseTriggers(core::CatalogManager::DatabaseTriggerEvent::ON_DISCONNECT);

    // Detach to dormant transaction so a privileged client can reattach later.
    if (conn_ctx_) {
        core::ID dormant_id;
        core::Status detach_status = database_->detachToDormant(conn_ctx_, dormant_id, ctx);
        if (detach_status != core::Status::OK) {
            // Fallback to hard rollback if we cannot persist dormant state.
            conn_ctx_->shutdownTransaction(ctx);
            stats_.transactions_rolled_back++;
            conn_ctx_.reset();
        }
    }

    // Close catalog session
    static const core::ID zero_id{};
    if (database_ && database_->catalog_manager() && session_id_uuid_ != zero_id) {
        database_->catalog_manager()->closeSession(session_id_uuid_, ctx);
    }

    state_ = SessionState::CLOSED;
    return core::Status::OK;
}

core::Status ServerSession::handleQuery(const protocol::Message& msg, core::ErrorContext* ctx) {
    if (state_ != SessionState::AUTHENTICATED && state_ != SessionState::IN_TRANSACTION) {
        sendError("Not authenticated", "28000");
        return core::Status::INVALID_AUTHORIZATION;
    }

    // Parse query using ProtocolCodec
    uint8_t session_id[16];
    std::string sql;
    uint8_t flags;
    std::vector<uint8_t> bytecode;

    core::Status status = protocol::ProtocolCodec::parseQuery(
        msg, session_id, sql, flags, &bytecode, ctx);
    if (status != core::Status::OK) {
        sendError("Invalid query");
        return status;
    }

    // Execute the query
    SessionState prev_state = state_;
    state_ = SessionState::EXECUTING;
    if (flags & static_cast<uint8_t>(protocol::QueryFlags::BYTECODE)) {
        status = executeBytecode(bytecode, sql, ctx);
    } else {
        status = executeQuery(sql, ctx);
    }

    // Restore state
    state_ = prev_state;

    return status;
}

core::Status ServerSession::handleTransaction(const protocol::Message& msg, core::ErrorContext* ctx) {
    if (state_ != SessionState::AUTHENTICATED && state_ != SessionState::IN_TRANSACTION) {
        sendError("Not authenticated", "28000");
        return core::Status::INVALID_AUTHORIZATION;
    }

    protocol::MessageType type = msg.getType();
    core::Status status = core::Status::OK;
    std::string result_msg;

    switch (type) {
        case protocol::MessageType::BEGIN_TRANSACTION:
            // In ScratchBird's Firebird MGA model, transactions are always active
            // BEGIN just marks the session as explicitly in transaction mode
            if (conn_ctx_) {
                // Start a new transaction (will commit any existing)
                status = conn_ctx_->startTransaction(false, core::IsolationLevel::SNAPSHOT, true, ctx);
                if (status == core::Status::OK) {
                    state_ = SessionState::IN_TRANSACTION;
                    // Fire ON TRANSACTION START database triggers (Firebird-style)
                    fireDatabaseTriggers(core::CatalogManager::DatabaseTriggerEvent::ON_TRANSACTION_START);
                    result_msg = "BEGIN";
                }
            }
            break;

        case protocol::MessageType::COMMIT:
            if (conn_ctx_) {
                // Fire ON TRANSACTION COMMIT triggers BEFORE commit (Firebird-style)
                fireDatabaseTriggers(core::CatalogManager::DatabaseTriggerEvent::ON_TRANSACTION_COMMIT);
                status = conn_ctx_->commit(ctx);
                if (status == core::Status::OK) {
                    state_ = SessionState::AUTHENTICATED;
                    stats_.transactions_committed++;
                    result_msg = "COMMIT";
                }
            }
            break;

        case protocol::MessageType::ROLLBACK:
            if (conn_ctx_) {
                // Fire ON TRANSACTION ROLLBACK triggers BEFORE rollback (Firebird-style)
                fireDatabaseTriggers(core::CatalogManager::DatabaseTriggerEvent::ON_TRANSACTION_ROLLBACK);
                status = conn_ctx_->rollback(ctx);
                state_ = SessionState::AUTHENTICATED;
                stats_.transactions_rolled_back++;
                result_msg = "ROLLBACK";
            }
            break;

        case protocol::MessageType::SAVEPOINT:
            // NET-2: Handle SAVEPOINT
            if (conn_ctx_) {
                // Parse savepoint name from message payload
                if (msg.getPayloadSize() >= sizeof(protocol::SavepointPayload)) {
                    const auto* sp_payload = reinterpret_cast<const protocol::SavepointPayload*>(msg.getPayload());
                    // Extract savepoint name (null-terminated, max 63 chars)
                    std::string sp_name(sp_payload->savepoint_name,
                                       strnlen(sp_payload->savepoint_name, sizeof(sp_payload->savepoint_name)));
                    status = conn_ctx_->createSavepoint(sp_name, ctx);
                    if (status == core::Status::OK) {
                        result_msg = "SAVEPOINT";
                    }
                } else {
                    sendError("Invalid savepoint message");
                    return core::Status::INVALID_ARGUMENT;
                }
            }
            break;

        case protocol::MessageType::RELEASE_SAVEPOINT:
            // NET-2: Handle RELEASE SAVEPOINT
            if (conn_ctx_) {
                if (msg.getPayloadSize() >= sizeof(protocol::SavepointPayload)) {
                    const auto* sp_payload = reinterpret_cast<const protocol::SavepointPayload*>(msg.getPayload());
                    std::string sp_name(sp_payload->savepoint_name,
                                       strnlen(sp_payload->savepoint_name, sizeof(sp_payload->savepoint_name)));
                    status = conn_ctx_->releaseSavepoint(sp_name, ctx);
                    if (status == core::Status::OK) {
                        result_msg = "RELEASE";
                    }
                } else {
                    sendError("Invalid release savepoint message");
                    return core::Status::INVALID_ARGUMENT;
                }
            }
            break;

        case protocol::MessageType::ROLLBACK_TO:
            // NET-2: Handle ROLLBACK TO SAVEPOINT
            if (conn_ctx_) {
                if (msg.getPayloadSize() >= sizeof(protocol::SavepointPayload)) {
                    const auto* sp_payload = reinterpret_cast<const protocol::SavepointPayload*>(msg.getPayload());
                    std::string sp_name(sp_payload->savepoint_name,
                                       strnlen(sp_payload->savepoint_name, sizeof(sp_payload->savepoint_name)));
                    status = conn_ctx_->rollbackToSavepoint(sp_name, ctx);
                    if (status == core::Status::OK) {
                        result_msg = "ROLLBACK";
                    }
                } else {
                    sendError("Invalid rollback to savepoint message");
                    return core::Status::INVALID_ARGUMENT;
                }
            }
            break;

        default:
            sendError("Unknown transaction type");
            return core::Status::INVALID_ARGUMENT;
    }

    if (status != core::Status::OK) {
        sendError("Transaction operation failed");
        return status;
    }

    // Send COMMAND_COMPLETE
    protocol::Message response = protocol::ProtocolCodec::buildCommandComplete(result_msg, 0);
    return protocol_session_->sendMessage(response, ctx);
}

core::Status ServerSession::handlePing(const protocol::Message& msg, core::ErrorContext* ctx) {
    // Parse ping to get timestamp and sequence
    uint64_t timestamp = 0;
    uint32_t sequence = 0;
    protocol::ProtocolCodec::parsePing(msg, timestamp, sequence, ctx);

    // Send PONG response with same timestamp/sequence
    protocol::Message response = protocol::ProtocolCodec::buildPong(timestamp, sequence);
    return protocol_session_->sendMessage(response, ctx);
}

core::Status ServerSession::handleCancel(const protocol::Message& msg, core::ErrorContext* ctx) {
    // NET-M1: Query cancellation support
    if (query_executing_.load(std::memory_order_acquire)) {
        // A query is currently executing - request cancellation
        if (executor_) {
            executor_->requestCancellation();
            // Send acknowledgement - the actual cancellation will happen asynchronously
            // The executing query will detect the cancellation flag and abort
            protocol::Message response = protocol::ProtocolCodec::buildCommandComplete("CANCEL", 0);
            return protocol_session_->sendMessage(response, ctx);
        }
    }

    // No query executing, nothing to cancel
    protocol::Message response = protocol::ProtocolCodec::buildCommandComplete("CANCEL", 0);
    return protocol_session_->sendMessage(response, ctx);
}

core::Status ServerSession::executeQuery(const std::string& sql, core::ErrorContext* ctx) {
    stats_.queries_executed++;

    // NET-M1: Reset cancellation state and mark query as executing
    if (executor_) {
        executor_->resetCancellation();
    }
    query_executing_.store(true, std::memory_order_release);

    // Track the statement for dormant reattach inspection (no cursor state retained).
    if (conn_ctx_) {
        conn_ctx_->beginStatementTracking(sql);
    }

    // NET-M1: Scope guard to ensure query_executing_ is reset on exit
    // This is a simple lambda-based scope guard
    struct QueryExecutingGuard {
        std::atomic<bool>& flag;
        QueryExecutingGuard(std::atomic<bool>& f) : flag(f) {}
        ~QueryExecutingGuard() { flag.store(false, std::memory_order_release); }
    } guard(query_executing_);

    struct ConnectionContextGuard {
        core::ConnectionContext* previous = nullptr;
        bool changed = false;

        explicit ConnectionContextGuard(core::ConnectionContext* current)
            : previous(core::ConnectionContext::getCurrent())
        {
            if (current && current != previous) {
                core::ConnectionContext::setCurrent(current);
                changed = true;
            }
        }

        ~ConnectionContextGuard() {
            if (changed) {
                core::ConnectionContext::setCurrent(previous);
            }
        }
    } ctx_guard(conn_ctx_.get());

    std::vector<uint8_t> bytecode;
    std::string error_msg;

    std::string dialect_tag = "SCRATCHBIRD";
    if (conn_ctx_) {
        dialect_tag = core::IdentifierUtils::toUpper(conn_ctx_->dialect_tag());
    }

    if (dialect_tag == "FIREBIRD" || dialect_tag == "FIREBIRDSQL") {
        if (!compiler_firebird_) {
            compiler_firebird_ = std::make_unique<sblr::FirebirdQueryCompiler>(database_);
        }
        auto compile_result = compiler_firebird_->compile(sql);
        if (!compile_result.success()) {
            error_msg = compile_result.errors().empty() ? "Compilation error" : compile_result.errors()[0];
        } else {
            bytecode = compile_result.bytecode();
        }
    } else if (dialect_tag == "POSTGRESQL" || dialect_tag == "POSTGRES" || dialect_tag == "PG") {
        if (!compiler_postgresql_) {
            compiler_postgresql_ = std::make_unique<sblr::PostgreSQLQueryCompiler>(database_);
        }
        auto compile_result = compiler_postgresql_->compile(sql);
        if (!compile_result.success()) {
            error_msg = compile_result.errors().empty() ? "Compilation error" : compile_result.errors()[0];
        } else {
            bytecode = compile_result.bytecode();
        }
    } else if (dialect_tag == "MYSQL") {
        if (!compiler_mysql_) {
            compiler_mysql_ = std::make_unique<sblr::MySQLQueryCompiler>(database_);
        }
        auto compile_result = compiler_mysql_->compile(sql);
        if (!compile_result.success()) {
            error_msg = compile_result.errors().empty() ? "Compilation error" : compile_result.errors()[0];
        } else {
            bytecode = compile_result.bytecode();
        }
    } else {
        if (!compiler_v2_) {
            compiler_v2_ = std::make_unique<sblr::QueryCompilerV2>(database_);
        }
        auto compile_result = compiler_v2_->compile(sql);
        if (!compile_result.success()) {
            error_msg = compile_result.errors().empty() ? "Compilation error" : compile_result.errors()[0];
        } else {
            bytecode = compile_result.bytecode();
        }
    }

    if (!error_msg.empty()) {
        stats_.queries_failed++;
        if (conn_ctx_) {
            conn_ctx_->endStatementTrackingFailure(
                static_cast<uint32_t>(core::Status::INVALID_ARGUMENT), "42000");
        }
        return sendError(error_msg, "42000", ctx);
    }

    // Execute the bytecode
    sblr::ExecutionResult exec_result = executor_->execute(bytecode);

    if (!exec_result.success()) {
        stats_.queries_failed++;
        if (conn_ctx_) {
            conn_ctx_->endStatementTrackingFailure(
                static_cast<uint32_t>(core::Status::INTERNAL_ERROR), "42000");
        }
        return sendError(exec_result.error(), "42000", ctx);
    }

    // Send results
    if (exec_result.hasResultSet()) {
        return sendResultSet(exec_result.resultSet(), ctx);
    } else {
        // DDL or DML without results
        std::string command = "OK";

        // Try to determine command type from SQL
        std::string sql_upper = sql;
        for (auto& c : sql_upper) c = static_cast<char>(std::toupper(c));

        if (sql_upper.find("INSERT") == 0) command = "INSERT";
        else if (sql_upper.find("UPDATE") == 0) command = "UPDATE";
        else if (sql_upper.find("DELETE") == 0) command = "DELETE";
        else if (sql_upper.find("CREATE") == 0) command = "CREATE";
        else if (sql_upper.find("DROP") == 0) command = "DROP";
        else if (sql_upper.find("ALTER") == 0) command = "ALTER";

        protocol::Message response = protocol::ProtocolCodec::buildCommandComplete(
            command, exec_result.affectedCount());
        if (conn_ctx_) {
            conn_ctx_->endStatementTrackingSuccess(exec_result.affectedCount());
        }
        return protocol_session_->sendMessage(response, ctx);
    }
}

core::Status ServerSession::executeBytecode(const std::vector<uint8_t>& bytecode,
                                            const std::string& sql,
                                            core::ErrorContext* ctx) {
    stats_.queries_executed++;

    if (executor_) {
        executor_->resetCancellation();
    }
    query_executing_.store(true, std::memory_order_release);

    if (conn_ctx_) {
        conn_ctx_->beginStatementTracking(sql.empty() ? "SBLR" : sql);
    }

    struct QueryExecutingGuard {
        std::atomic<bool>& flag;
        explicit QueryExecutingGuard(std::atomic<bool>& f) : flag(f) {}
        ~QueryExecutingGuard() { flag.store(false, std::memory_order_release); }
    } guard(query_executing_);

    struct ConnectionContextGuard {
        core::ConnectionContext* previous = nullptr;
        bool changed = false;

        explicit ConnectionContextGuard(core::ConnectionContext* current)
            : previous(core::ConnectionContext::getCurrent())
        {
            if (current && current != previous) {
                core::ConnectionContext::setCurrent(current);
                changed = true;
            }
        }

        ~ConnectionContextGuard() {
            if (changed) {
                core::ConnectionContext::setCurrent(previous);
            }
        }
    } ctx_guard(conn_ctx_.get());

    if (executor_) {
        core::ErrorContext validate_ctx;
        core::Status validate_status = sblr::validateBytecode(bytecode, &validate_ctx);
        if (validate_status != core::Status::OK) {
            stats_.queries_failed++;
            if (conn_ctx_) {
                conn_ctx_->endStatementTrackingFailure(
                    static_cast<uint32_t>(validate_status), "0A000");
            }
            std::string err = validate_ctx.message.empty()
                ? "Invalid bytecode"
                : validate_ctx.message;
            return sendError(err, "0A000", ctx);
        }
    }

    sblr::ExecutionResult exec_result = executor_->execute(bytecode);

    if (!exec_result.success()) {
        stats_.queries_failed++;
        if (conn_ctx_) {
            conn_ctx_->endStatementTrackingFailure(
                static_cast<uint32_t>(core::Status::INTERNAL_ERROR), "42000");
        }
        return sendError(exec_result.error(), "42000", ctx);
    }

    if (exec_result.hasResultSet()) {
        return sendResultSet(exec_result.resultSet(), ctx);
    }

    std::string command = "OK";
    if (!sql.empty()) {
        std::string sql_upper = sql;
        for (auto& c : sql_upper) c = static_cast<char>(std::toupper(c));

        if (sql_upper.find("INSERT") == 0) command = "INSERT";
        else if (sql_upper.find("UPDATE") == 0) command = "UPDATE";
        else if (sql_upper.find("DELETE") == 0) command = "DELETE";
        else if (sql_upper.find("CREATE") == 0) command = "CREATE";
        else if (sql_upper.find("DROP") == 0) command = "DROP";
        else if (sql_upper.find("ALTER") == 0) command = "ALTER";
    }

    protocol::Message response = protocol::ProtocolCodec::buildCommandComplete(
        command, exec_result.affectedCount());
    if (conn_ctx_) {
        conn_ctx_->endStatementTrackingSuccess(exec_result.affectedCount());
    }
    return protocol_session_->sendMessage(response, ctx);
}

// Helper function to convert DataType to WireType
static protocol::WireType dataTypeToWireType(core::DataType type) {
    switch (type) {
        case core::DataType::BOOLEAN:   return protocol::WireType::BOOLEAN;
        case core::DataType::INT16:     return protocol::WireType::INT16;
        case core::DataType::INT32:     return protocol::WireType::INT32;
        case core::DataType::INT64:     return protocol::WireType::INT64;
        case core::DataType::FLOAT32:   return protocol::WireType::FLOAT32;
        case core::DataType::FLOAT64:   return protocol::WireType::FLOAT64;
        case core::DataType::DECIMAL:   return protocol::WireType::DECIMAL;
        case core::DataType::CHAR:      return protocol::WireType::CHAR;
        case core::DataType::VARCHAR:   return protocol::WireType::VARCHAR;
        case core::DataType::TEXT:      return protocol::WireType::VARCHAR;
        case core::DataType::BYTEA:     return protocol::WireType::BYTEA;
        case core::DataType::DATE:      return protocol::WireType::DATE;
        case core::DataType::TIME:      return protocol::WireType::TIME;
        case core::DataType::TIMESTAMP: return protocol::WireType::TIMESTAMP;
        case core::DataType::INTERVAL:  return protocol::WireType::INTERVAL;
        case core::DataType::UUID:      return protocol::WireType::UUID;
        case core::DataType::JSON:      return protocol::WireType::JSON;
        case core::DataType::JSONB:     return protocol::WireType::JSONB;
        case core::DataType::ARRAY:     return protocol::WireType::ARRAY;
        case core::DataType::MONEY:     return protocol::WireType::MONEY;
        case core::DataType::XML:       return protocol::WireType::XML;
        case core::DataType::INET:      return protocol::WireType::INET;
        case core::DataType::CIDR:      return protocol::WireType::CIDR;
        case core::DataType::MACADDR:   return protocol::WireType::MACADDR;
        case core::DataType::TSVECTOR:  return protocol::WireType::TSVECTOR;
        case core::DataType::TSQUERY:   return protocol::WireType::TSQUERY;
        case core::DataType::NULL_TYPE: return protocol::WireType::NULL_TYPE;
        default:                        return protocol::WireType::UNKNOWN;
    }
}

core::Status ServerSession::sendResultSet(const sblr::ResultSet* results, core::ErrorContext* ctx) {
    if (!results) {
        return sendError("Internal error: null result set", "XX000", ctx);
    }

    // Build column descriptions using ColumnInfo
    std::vector<protocol::ProtocolCodec::ColumnInfo> columns;
    for (size_t i = 0; i < results->columnCount(); i++) {
        protocol::ProtocolCodec::ColumnInfo col;
        col.name = results->columnName(i);
        col.type = dataTypeToWireType(results->columnType(i));
        col.type_modifier = 0;
        columns.push_back(col);
    }

    // Send ROW_DESCRIPTION
    protocol::Message desc_msg = protocol::ProtocolCodec::buildRowDescription(columns);
    core::Status status = protocol_session_->sendMessage(desc_msg, ctx);
    if (status != core::Status::OK) return status;

    // Send ROW_DATA for each row
    for (size_t row = 0; row < results->rowCount(); row++) {
        std::vector<protocol::ProtocolCodec::ColumnValue> values;
        for (size_t col = 0; col < results->columnCount(); col++) {
            const sblr::Value& val = results->getValue(row, col);
            if (val.isNull()) {
                values.push_back(protocol::ProtocolCodec::ColumnValue(nullptr));
            } else {
                // Convert value to binary format based on type
                switch (val.type()) {
                    case core::DataType::INT32:
                        values.push_back(protocol::ProtocolCodec::ColumnValue::fromInt32(val.getInt32()));
                        break;
                    case core::DataType::INT64:
                        values.push_back(protocol::ProtocolCodec::ColumnValue::fromInt64(val.getInt64()));
                        break;
                    case core::DataType::FLOAT32:
                    case core::DataType::FLOAT64:
                        values.push_back(protocol::ProtocolCodec::ColumnValue::fromDouble(val.getFloat64()));
                        break;
                    case core::DataType::BOOLEAN:
                        values.push_back(protocol::ProtocolCodec::ColumnValue::fromBool(val.getBool()));
                        break;
                    case core::DataType::VARCHAR:
                    case core::DataType::TEXT:
                    case core::DataType::CHAR:
                    default:
                        // Fall back to string representation for text and other types
                        values.push_back(protocol::ProtocolCodec::ColumnValue::fromString(val.toString()));
                        break;
                }
            }
        }

        protocol::Message row_msg = protocol::ProtocolCodec::buildRowData(values);
        status = protocol_session_->sendMessage(row_msg, ctx);
        if (status != core::Status::OK) return status;

        stats_.rows_returned++;
    }

    if (conn_ctx_) {
        conn_ctx_->endStatementTrackingSuccess(static_cast<int64_t>(results->rowCount()));
    }

    // Send COMMAND_COMPLETE
    std::string command = "SELECT " + std::to_string(results->rowCount());
    protocol::Message complete_msg = protocol::ProtocolCodec::buildCommandComplete(
        command, static_cast<int64_t>(results->rowCount()));
    status = protocol_session_->sendMessage(complete_msg, ctx);
    if (status != core::Status::OK) return status;

    // Send END_OF_RESULTS
    protocol::Message end_msg = protocol::ProtocolCodec::buildEndOfResults();
    return protocol_session_->sendMessage(end_msg, ctx);
}

core::Status ServerSession::sendError(const std::string& message,
                                       const std::string& sqlstate,
                                       core::ErrorContext* ctx) {
    // buildQueryError(error_code, sqlstate, message, detail, hint)
    protocol::Message error_msg = protocol::ProtocolCodec::buildQueryError(
        static_cast<uint32_t>(core::Status::INTERNAL_ERROR), sqlstate, message, "", "");
    return protocol_session_->sendMessage(error_msg, ctx);
}

core::AuthResult ServerSession::authenticate(const std::string& username,
                                            const std::string& password,
                                            core::AuthUserInfo& user_info,
                                            std::string& error_msg_out) {
    // Get catalog manager from database
    auto* catalog = database_ ? database_->catalog_manager() : nullptr;
    if (!catalog) {
        // No catalog manager - allow any authentication (development mode)
        user_info.user_id = core::generateUuidV7();
        user_info.username = username;
        user_info.display_name = username;
        user_info.is_superuser = true;
        user_info.authkey_id = core::generateUuidV7();
        return core::AuthResult::SUCCESS;
    }

    auto* audit_logger = database_ ? database_->audit_logger() : nullptr;
    auto provider = core::AuthProviderFactory::createDefault(catalog, audit_logger);
    if (!provider) {
        error_msg_out = "Authentication provider unavailable";
        return core::AuthResult::PROVIDER_ERROR;
    }

    return provider->authenticate(username, password, user_info, error_msg_out);
}

bool ServerSession::fireDatabaseTriggers(core::CatalogManager::DatabaseTriggerEvent event) {
    // Get catalog manager
    auto* catalog = database_->catalog_manager();
    if (!catalog) {
        // No catalog - no triggers to fire
        return true;
    }

    // Get all triggers for this event (already sorted by position)
    std::vector<core::CatalogManager::DatabaseTriggerInfo> triggers;
    core::ErrorContext ctx;
    core::Status status = catalog->listDatabaseTriggers(event, triggers, &ctx);

    if (status != core::Status::OK) {
        // Failed to list triggers - log warning but don't fail the operation
        std::cerr << "Warning: Failed to list database triggers for event\n";
        return true;
    }

    // Execute each active trigger in order
    for (const auto& trigger : triggers) {
        if (!trigger.active) {
            continue;  // Skip inactive triggers
        }

        // Execute the trigger procedure using the Executor
        // Create a temporary executor for trigger execution
        sblr::Executor trigger_executor(database_);

        // Set connection context if available (for security and transaction state)
        if (conn_ctx_) {
            trigger_executor.setConnectionContext(conn_ctx_.get());
        }

        // Call the procedure by name (database trigger procedures have no arguments)
        auto result = trigger_executor.callProcedureByName(trigger.procedure_name);

        if (!result.success()) {
            // Trigger execution failed
            std::cerr << "Database trigger '" << trigger.trigger_name
                      << "' failed: " << result.error() << "\n";
            return false;  // Abort the operation
        }

        // Log successful trigger fire (can be removed in production)
        std::cerr << "Database trigger '" << trigger.trigger_name
                  << "' executed procedure: " << trigger.procedure_name << "()\n";
    }

    return true;  // All triggers executed successfully
}

// ============================================================================
// SessionManager Implementation
// ============================================================================

SessionManager::SessionManager() = default;

SessionManager::~SessionManager() {
    shutdownAll();
}

ServerSession* SessionManager::createSession(IPCConnection* connection, core::Database* database) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Generate session ID
    uint8_t session_id[16];
    protocol::generateSessionId(session_id);

    // Create session
    auto session = std::make_unique<ServerSession>(connection, database, session_id);
    ServerSession* ptr = session.get();

    // Store in map
    std::string id_str = protocol::sessionIdToString(session_id);
    sessions_[id_str] = std::move(session);

    return ptr;
}

void SessionManager::removeSession(const uint8_t session_id[16]) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id_str = protocol::sessionIdToString(session_id);
    sessions_.erase(id_str);
}

ServerSession* SessionManager::getSession(const uint8_t session_id[16]) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string id_str = protocol::sessionIdToString(session_id);
    auto it = sessions_.find(id_str);
    if (it != sessions_.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<ServerSession*> SessionManager::getAllSessions() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ServerSession*> result;
    result.reserve(sessions_.size());
    for (auto& pair : sessions_) {
        result.push_back(pair.second.get());
    }
    return result;
}

size_t SessionManager::sessionCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return sessions_.size();
}

void SessionManager::shutdownAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : sessions_) {
        pair.second->requestShutdown();
    }
}

bool SessionManager::waitForShutdown(uint32_t timeout_ms) {
    auto start = std::chrono::steady_clock::now();

    while (true) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            bool all_closed = true;
            for (auto& pair : sessions_) {
                if (pair.second->isRunning()) {
                    all_closed = false;
                    break;
                }
            }
            if (all_closed) return true;
        }

        // Check timeout
        if (timeout_ms > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= timeout_ms) {
                return false;
            }
        }

        // Sleep briefly
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

SessionStats SessionManager::getAggregateStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    SessionStats aggregate;
    aggregate.created_at = std::chrono::steady_clock::now();
    aggregate.last_activity = std::chrono::steady_clock::time_point::min();

    for (const auto& pair : sessions_) {
        const auto& s = pair.second->stats();
        aggregate.queries_executed += s.queries_executed;
        aggregate.queries_failed += s.queries_failed;
        aggregate.rows_returned += s.rows_returned;
        aggregate.bytes_sent += s.bytes_sent;
        aggregate.bytes_received += s.bytes_received;
        aggregate.transactions_committed += s.transactions_committed;
        aggregate.transactions_rolled_back += s.transactions_rolled_back;

        if (s.created_at < aggregate.created_at) {
            aggregate.created_at = s.created_at;
        }
        if (s.last_activity > aggregate.last_activity) {
            aggregate.last_activity = s.last_activity;
        }
    }

    return aggregate;
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* sessionStateToString(SessionState state) {
    switch (state) {
        case SessionState::CREATED:        return "CREATED";
        case SessionState::AUTHENTICATING: return "AUTHENTICATING";
        case SessionState::AUTHENTICATED:  return "AUTHENTICATED";
        case SessionState::EXECUTING:      return "EXECUTING";
        case SessionState::IN_TRANSACTION: return "IN_TRANSACTION";
        case SessionState::CLOSING:        return "CLOSING";
        case SessionState::CLOSED:         return "CLOSED";
        default:                           return "UNKNOWN";
    }
}

}  // namespace server
}  // namespace scratchbird
