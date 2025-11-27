/**
 * ScratchBird Server Session Implementation
 *
 * Local Server Architecture - Phase 3
 */

#include "scratchbird/server/server_session.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/bytecode_generator.h"
#include "scratchbird/parser/lexer.h"
#include "scratchbird/parser/parser.h"
#include "scratchbird/parser/ast.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/password_hash.h"
#include "scratchbird/core/types.h"

#include <cstring>
#include <sstream>
#include <iomanip>

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

    // Create parser components
    ast_arena_ = std::make_unique<parser::ASTArena>();
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
    std::string username, password;

    core::Status status = protocol::ProtocolCodec::parseAuthRequest(
        msg, session_id, username, password, ctx);

    if (status != core::Status::OK) {
        sendError("Invalid auth request");
        return status;
    }

    // Attempt authentication
    core::ID user_id;
    bool is_superuser = false;
    bool success = authenticate(username, password, user_id, is_superuser);

    if (success) {
        username_ = username;
        state_ = SessionState::AUTHENTICATED;

        // Create connection context
        database_->connect(conn_ctx_, ctx);
        if (conn_ctx_) {
            // Set user with ID and superuser flag
            conn_ctx_->setCurrentUser(user_id, is_superuser);
            executor_->setConnectionContext(conn_ctx_.get());
        }

        // Send success response (use first 4 bytes of UUID as uint32 for wire protocol)
        uint32_t user_id_wire = 0;
        std::memcpy(&user_id_wire, user_id.bytes.data(), std::min<size_t>(4, user_id.bytes.size()));
        protocol::Message response = protocol::ProtocolCodec::buildAuthResponse(true, user_id_wire, "");
        return protocol_session_->sendMessage(response, ctx);
    } else {
        stats_.queries_failed++;

        // Send failure response
        protocol::Message response = protocol::ProtocolCodec::buildAuthResponse(
            false, 0, "Authentication failed");
        protocol_session_->sendMessage(response, ctx);
        return core::Status::INVALID_PASSWORD;
    }
}

core::Status ServerSession::handleDisconnect(const protocol::Message& msg, core::ErrorContext* ctx) {
    state_ = SessionState::CLOSING;

    // Rollback any active transaction (if we have a connection context)
    if (conn_ctx_) {
        // Rollback always succeeds, even if there's no active transaction
        conn_ctx_->rollback(ctx);
        stats_.transactions_rolled_back++;
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

    core::Status status = protocol::ProtocolCodec::parseQuery(msg, session_id, sql, flags, ctx);
    if (status != core::Status::OK) {
        sendError("Invalid query");
        return status;
    }

    // Execute the query
    SessionState prev_state = state_;
    state_ = SessionState::EXECUTING;
    status = executeQuery(sql, ctx);

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
                    result_msg = "BEGIN";
                }
            }
            break;

        case protocol::MessageType::COMMIT:
            if (conn_ctx_) {
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
                status = conn_ctx_->rollback(ctx);
                state_ = SessionState::AUTHENTICATED;
                stats_.transactions_rolled_back++;
                result_msg = "ROLLBACK";
            }
            break;

        case protocol::MessageType::SAVEPOINT:
        case protocol::MessageType::RELEASE_SAVEPOINT:
        case protocol::MessageType::ROLLBACK_TO:
            // Savepoints not yet implemented - return error
            sendError("Savepoints not yet implemented", "0A000");
            return core::Status::NOT_IMPLEMENTED;

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
    // For now, just acknowledge - actual cancellation requires async execution
    sendError("Query cancellation not implemented", "0A000");
    return core::Status::OK;
}

core::Status ServerSession::executeQuery(const std::string& sql, core::ErrorContext* ctx) {
    stats_.queries_executed++;

    // Reset parser components for reuse
    ast_arena_->reset();

    // Parse the SQL
    parser::Lexer lexer(sql);
    parser::Parser parser_instance(lexer, *ast_arena_);

    parser::ParseResult parse_result = parser_instance.parseStatement();

    if (!parse_result.success()) {
        stats_.queries_failed++;
        std::string error_msg = "Parse error";
        if (!parse_result.errors().empty()) {
            error_msg = parse_result.errors()[0].message;
        }
        return sendError(error_msg, "42601", ctx);  // syntax_error
    }

    // Generate bytecode
    sblr::BytecodeGenerator generator(parser_instance.stringPool(), database_);
    sblr::BytecodeResult bytecode_result = generator.generate(parse_result.statement());

    if (!bytecode_result.success()) {
        stats_.queries_failed++;
        std::string error_msg = "Bytecode generation error";
        if (!bytecode_result.errors().empty()) {
            error_msg = bytecode_result.errors()[0];
        }
        return sendError(error_msg, "42000", ctx);  // syntax_error_or_access_rule_violation
    }

    // Execute the bytecode
    sblr::ExecutionResult exec_result = executor_->execute(bytecode_result.bytecode());

    if (!exec_result.success()) {
        stats_.queries_failed++;
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
        return protocol_session_->sendMessage(response, ctx);
    }
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
                // Convert to string representation for now
                values.push_back(protocol::ProtocolCodec::ColumnValue::fromString(val.toString()));
            }
        }

        protocol::Message row_msg = protocol::ProtocolCodec::buildRowData(values);
        status = protocol_session_->sendMessage(row_msg, ctx);
        if (status != core::Status::OK) return status;

        stats_.rows_returned++;
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

bool ServerSession::authenticate(const std::string& username, const std::string& password,
                                  core::ID& user_id, bool& is_superuser) {
    // Default values
    user_id = core::ID{};
    is_superuser = false;

    // Get catalog manager from database
    auto* catalog = database_->catalog_manager();
    if (!catalog) {
        // No catalog manager - allow any authentication (development mode)
        // Generate a default user ID and set superuser
        user_id = core::generateUuidV7();
        is_superuser = true;
        return true;
    }

    // Look up user in catalog
    core::ErrorContext ctx;
    core::CatalogManager::UserInfo user;
    core::Status status = catalog->getUserByName(username, user, &ctx);
    if (status != core::Status::OK) {
        // User not found
        return false;
    }

    // Check if user is active
    if (!user.is_active) {
        return false;
    }

    // Verify password using PasswordHash
    if (!core::PasswordHash::verifyPassword(password, user.password_hash)) {
        return false;
    }

    // Authentication successful - set output parameters
    user_id = user.user_id;
    is_superuser = user.is_superuser;
    return true;
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
