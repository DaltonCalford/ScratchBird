/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

/**
 * EngineIPCSessionHandler - Engine-side IPC execution bridge
 *
 * This is the engine half of the external parser-agent topology. Listener and
 * parser-agent processes own protocol negotiation, engine-specific SQL rules,
 * and SQL-text to SBLR compilation. This handler manages session state,
 * prepared statements, portals, and execution of precompiled engine payloads.
 *
 * Reviewers should not read this file as a general SQL parser inside the
 * engine; the deployed IPC contract is intentionally parserless on the engine
 * side.
 */
// Section 32 invariant: this file is the engine-side execution bridge for the
// parser-agent topology. It owns engine-session execution only and does not
// absorb listener negotiation, parser ownership, or public protocol ownership.

#include "scratchbird/ipc/engine_ipc_session_handler.h"
#include "scratchbird/sblr/executor.h"
#include "scratchbird/sblr/bytecode_validator.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"

#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <deque>
#include <fstream>
#include <iostream>

namespace scratchbird {
namespace ipc {

namespace {

std::string boundedString(const char* value, size_t max_len) {
    size_t len = 0;
    while (len < max_len && value[len] != '\0') {
        ++len;
    }
    return std::string(value, len);
}

std::string dialectTagForApplication(const std::string& application) {
    std::string normalized;
    normalized.reserve(application.size());
    for (unsigned char c : application) {
        normalized.push_back(static_cast<char>(std::tolower(c)));
    }

    if (normalized.find("firebird") != std::string::npos) {
        return "firebird";
    }
    if (normalized.find("postgres") != std::string::npos) {
        return "postgresql";
    }
    if (normalized.find("mysql") != std::string::npos) {
        return "mysql";
    }
    return "scratchbird";
}

std::string emulationModeForDialectTag(const std::string& dialect_tag) {
    return dialect_tag == "scratchbird" ? "native" : dialect_tag;
}

std::string mysqlCompileOs() {
#if defined(_WIN32)
    return "Win64";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

std::string mysqlCompileMachine() {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#else
    return "unknown";
#endif
}

const char* postgresqlServerVersion() {
    return "15.4 (ScratchBird 1.0)";
}

void applyIpcFieldTypeMetadata(IPCFieldDesc& field, core::DataType type) {
    switch (type) {
        case core::DataType::INT8:
        case core::DataType::UINT8:
        case core::DataType::INT16:
        case core::DataType::UINT16:
            field.type_size = 2;
            break;
        case core::DataType::INT32:
        case core::DataType::UINT32:
        case core::DataType::MEDIUMINT:
        case core::DataType::FLOAT32:
        case core::DataType::DATE:
        case core::DataType::TIME:
        case core::DataType::YEAR:
            field.type_size = 4;
            break;
        case core::DataType::INT64:
        case core::DataType::UINT64:
        case core::DataType::FLOAT64:
        case core::DataType::DECIMAL:
        case core::DataType::MONEY:
        case core::DataType::TIMESTAMP:
        case core::DataType::DATETIME:
        case core::DataType::TIME_WITH_ZONE:
        case core::DataType::DECFLOAT16:
            field.type_size = 8;
            break;
        case core::DataType::INT128:
        case core::DataType::UINT128:
        case core::DataType::DECFLOAT34:
            field.type_size = 16;
            break;
        case core::DataType::TIMESTAMP_WITH_ZONE:
            field.type_size = 12;
            break;
        case core::DataType::BOOLEAN:
        case core::DataType::BIT:
            field.type_size = 1;
            break;
        case core::DataType::CHAR:
            field.type_size = -1;
            field.type_modifier = 1;
            break;
        case core::DataType::VARCHAR:
        case core::DataType::TEXT:
        case core::DataType::JSON:
        case core::DataType::JSONB:
        case core::DataType::XML:
        case core::DataType::UUID:
        case core::DataType::INET:
        case core::DataType::CIDR:
        case core::DataType::MACADDR:
        case core::DataType::MACADDR8:
        case core::DataType::ENUM:
        case core::DataType::SET:
        case core::DataType::TSVECTOR:
        case core::DataType::TSQUERY:
            field.type_size = -1;
            field.type_modifier = 255;
            break;
        default:
            field.type_size = 0;
            break;
    }
}

void seedDialectSessionVariables(core::ConnectionContext* conn_ctx,
                                 const std::string& dialect_tag) {
    if (conn_ctx == nullptr) {
        return;
    }

    if (dialect_tag == "mysql") {
        conn_ctx->setSessionVariable("VERSION", "8.0.32-ScratchBird");
        conn_ctx->setSessionVariable("VERSION_COMMENT", "ScratchBird MySQL emulation");
        conn_ctx->setSessionVariable("VERSION_COMPILE_OS", mysqlCompileOs());
        conn_ctx->setSessionVariable("VERSION_COMPILE_MACHINE", mysqlCompileMachine());
        conn_ctx->setSessionVariable("CHARACTER_SET_CLIENT", "utf8mb4");
        conn_ctx->setSessionVariable("CHARACTER_SET_CONNECTION", "utf8mb4");
        conn_ctx->setSessionVariable("CHARACTER_SET_RESULTS", "utf8mb4");
        conn_ctx->setSessionVariable("COLLATION_CONNECTION", "utf8mb4_general_ci");
        conn_ctx->setSessionVariable("OPTIMIZER_SWITCH", "index_merge=on");
        conn_ctx->setSessionVariable("SQL_MODE", "");
        conn_ctx->setSessionVariable("TRANSACTION_ISOLATION", "REPEATABLE-READ");
        conn_ctx->setSessionVariable("DEFAULT_TRANSACTION_ISOLATION", "REPEATABLE-READ");
        conn_ctx->setSessionVariable("TX_ISOLATION", "REPEATABLE-READ");
    } else if (dialect_tag == "postgresql") {
        conn_ctx->setSessionVariable("SERVER_VERSION", postgresqlServerVersion());
        conn_ctx->setSessionVariable("SERVER_ENCODING", "UTF8");
        conn_ctx->setSessionVariable("CLIENT_ENCODING", "UTF8");
        conn_ctx->setSessionVariable("DATESTYLE", "ISO, MDY");
        conn_ctx->setSessionVariable("TIMEZONE", "UTC");
        conn_ctx->setSessionVariable("INTEGER_DATETIMES", "on");
        conn_ctx->setSessionVariable("STANDARD_CONFORMING_STRINGS", "on");
    }
}

void seedDialectSessionIdentity(core::ConnectionContext* conn_ctx,
                                const std::string& dialect_tag,
                                const std::string& database_name,
                                const std::string& username) {
    if (conn_ctx == nullptr) {
        return;
    }

    if (dialect_tag == "mysql") {
        const std::string effective_database =
            database_name.empty() ? "compat_mysql" : database_name;
        const std::string effective_user =
            username.empty() ? "root" : username;
        const std::string qualified_user = effective_user + "@localhost";
        conn_ctx->setSessionVariable("DATABASE", effective_database);
        conn_ctx->setSessionVariable("SCHEMA", effective_database);
        conn_ctx->setSessionVariable("CURRENT_USER", qualified_user);
        conn_ctx->setSessionVariable("USER", qualified_user);
        conn_ctx->setSessionVariable("SESSION_USER", qualified_user);
        conn_ctx->setSessionVariable("SYSTEM_USER", qualified_user);
    }
}

bool equalsIgnoreCaseAscii(const std::string& lhs, const char* rhs) {
    if (rhs == nullptr) {
        return false;
    }
    const size_t rhs_len = std::strlen(rhs);
    if (lhs.size() != rhs_len) {
        return false;
    }
    for (size_t i = 0; i < lhs.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
            std::tolower(static_cast<unsigned char>(rhs[i]))) {
            return false;
        }
    }
    return true;
}

std::string joinSearchPath(const std::vector<std::string>& search_path) {
    std::ostringstream out;
    for (size_t i = 0; i < search_path.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << search_path[i];
    }
    return out.str();
}

void appendIpcDebugLine(const std::string& line) {
    std::ofstream out("/tmp/sb_ipc_debug.log", std::ios::app);
    if (!out) {
        return;
    }
    out << line << '\n';
}

bool compiledQueryIpcTraceEnabled() {
    static const bool enabled = []() {
        const char* value = std::getenv("SCRATCHBIRD_TRACE_COMPILED_QUERY_IPC");
        if (value == nullptr || value[0] == '\0') {
            return false;
        }
        std::string normalized(value);
        std::transform(normalized.begin(),
                       normalized.end(),
                       normalized.begin(),
                       [](unsigned char ch) {
                           return static_cast<char>(std::toupper(ch));
                       });
        return normalized != "0" &&
               normalized != "FALSE" &&
               normalized != "NO" &&
               normalized != "OFF";
    }();
    return enabled;
}

std::string summarizeBytecode(const std::vector<uint8_t>& bytecode) {
    auto hex_slice = [&](size_t begin, size_t end) {
        std::ostringstream out;
        out << std::hex << std::setfill('0');
        for (size_t i = begin; i < end; ++i) {
            if (i != begin) {
                out << ' ';
            }
            out << std::setw(2) << static_cast<unsigned>(bytecode[i]);
        }
        return out.str();
    };

    if (bytecode.size() <= 32) {
        return hex_slice(0, bytecode.size());
    }

    return hex_slice(0, 16) + " ... " +
           hex_slice(bytecode.size() - 16, bytecode.size());
}

core::Status rejectEngineSqlTextPath(uint32_t session_id,
                                     const char* action,
                                     EngineIPCSessionHandler* handler,
                                     core::ErrorContext* ctx) {
    const std::string message =
        std::string("Engine IPC ") + action +
        " SQL-text path is disabled; parser agents must send precompiled SBLR "
        "with COMPILED_QUERY/COMPILED_PARSE";
    if (ctx != nullptr) {
        ctx->set(core::Status::NOT_SUPPORTED,
                 message.c_str(),
                 __FILE__, __LINE__, __func__);
    }
    return handler->sendError(session_id, "0A000", message);
}

struct ConnectionContextGuard {
    core::ConnectionContext* previous = nullptr;
    bool changed = false;

    explicit ConnectionContextGuard(core::ConnectionContext* current)
        : previous(core::ConnectionContext::getCurrent()) {
        if (current != nullptr && current != previous) {
            core::ConnectionContext::setCurrent(current);
            changed = true;
        }
    }

    ~ConnectionContextGuard() {
        if (changed) {
            core::ConnectionContext::setCurrent(previous);
        }
    }
};

void syncSessionTransactionState(core::ConnectionContext* conn_ctx,
                                 bool& in_transaction_out,
                                 bool& autocommit_out) {
    if (conn_ctx == nullptr) {
        in_transaction_out = false;
        autocommit_out = true;
        return;
    }

    in_transaction_out = conn_ctx->getCurrentXid() != 0;
    autocommit_out = conn_ctx->autocommitMode() && !conn_ctx->autocommitSuspended();
}

}  // namespace

// ============================================================================
// Prepared Statement Cache Entry with LRU
// ============================================================================

struct PreparedStatement {
    std::string name;
    std::string sql;
    std::vector<uint8_t> bytecode;
    std::vector<IPCFieldDesc> param_fields;
    std::vector<IPCFieldDesc> result_fields;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point last_used;
    uint64_t execution_count = 0;
    size_t memory_size = 0;
    bool is_valid = true;
    
    // LRU list pointers
    PreparedStatement* prev = nullptr;
    PreparedStatement* next = nullptr;
};

// ============================================================================
// LRU Cache Manager
// ============================================================================

class StatementCache {
public:
    static constexpr size_t DEFAULT_MAX_STATEMENTS = 100;
    static constexpr size_t DEFAULT_MAX_MEMORY = 10 * 1024 * 1024;  // 10MB
    
    StatementCache(size_t max_stmts = DEFAULT_MAX_STATEMENTS,
                   size_t max_memory = DEFAULT_MAX_MEMORY)
        : max_statements_(max_stmts),
          max_memory_(max_memory),
          current_memory_(0),
          head_(nullptr),
          tail_(nullptr) {
    }
    
    ~StatementCache() {
        clear();
    }
    
    // Get statement from cache
    PreparedStatement* get(const std::string& name) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        auto it = map_.find(name);
        if (it == map_.end()) {
            return nullptr;
        }
        
        auto* stmt = it->second.get();
        if (!stmt->is_valid) {
            return nullptr;
        }
        
        // Move to front (most recently used)
        removeFromList(stmt);
        addToFront(stmt);
        stmt->last_used = std::chrono::steady_clock::now();
        
        return stmt;
    }
    
    // Add statement to cache
    bool put(const std::string& name, std::unique_ptr<PreparedStatement> stmt) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        // Calculate memory size
        stmt->memory_size = stmt->sql.size() + stmt->bytecode.size() + 
                           sizeof(PreparedStatement);
        
        // Check if we need to evict
        while ((map_.size() >= max_statements_ || 
                current_memory_ + stmt->memory_size > max_memory_) && 
               tail_) {
            evictLRU();
        }
        
        // Remove existing if present
        auto it = map_.find(name);
        if (it != map_.end()) {
            removeFromList(it->second.get());
            current_memory_ -= it->second->memory_size;
            map_.erase(it);
        }
        
        // Add new statement
        auto* raw_ptr = stmt.get();
        raw_ptr->last_used = std::chrono::steady_clock::now();
        
        addToFront(raw_ptr);
        current_memory_ += raw_ptr->memory_size;
        map_[name] = std::move(stmt);
        
        return true;
    }
    
    // Remove statement from cache
    void remove(const std::string& name) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        auto it = map_.find(name);
        if (it != map_.end()) {
            removeFromList(it->second.get());
            current_memory_ -= it->second->memory_size;
            map_.erase(it);
        }
    }
    
    // Invalidate all statements
    void clear() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        
        map_.clear();
        current_memory_ = 0;
        head_ = nullptr;
        tail_ = nullptr;
    }
    
    // Get cache statistics
    struct Stats {
        size_t num_statements;
        size_t current_memory;
        size_t max_memory;
        size_t max_statements;
        size_t total_evicted;
    };
    
    Stats getStats() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        Stats stats;
        stats.num_statements = map_.size();
        stats.current_memory = current_memory_;
        stats.max_memory = max_memory_;
        stats.max_statements = max_statements_;
        stats.total_evicted = total_evicted_;
        return stats;
    }
    
    // Get list of statement names
    std::vector<std::string> getStatementNames() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        
        std::vector<std::string> names;
        names.reserve(map_.size());
        
        for (const auto& pair : map_) {
            names.push_back(pair.first);
        }
        
        return names;
    }

private:
    size_t max_statements_;
    size_t max_memory_;
    size_t current_memory_;
    size_t total_evicted_ = 0;
    
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<PreparedStatement>> map_;
    
    // LRU list
    PreparedStatement* head_;
    PreparedStatement* tail_;
    
    void addToFront(PreparedStatement* stmt) {
        stmt->next = head_;
        stmt->prev = nullptr;
        
        if (head_) {
            head_->prev = stmt;
        }
        head_ = stmt;
        
        if (!tail_) {
            tail_ = stmt;
        }
    }
    
    void removeFromList(PreparedStatement* stmt) {
        if (stmt->prev) {
            stmt->prev->next = stmt->next;
        } else {
            head_ = stmt->next;
        }
        
        if (stmt->next) {
            stmt->next->prev = stmt->prev;
        } else {
            tail_ = stmt->prev;
        }
        
        stmt->prev = nullptr;
        stmt->next = nullptr;
    }
    
    void evictLRU() {
        if (!tail_) return;
        
        auto* to_evict = tail_;
        std::string name = to_evict->name;
        
        removeFromList(to_evict);
        current_memory_ -= to_evict->memory_size;
        
        // Find and remove from map
        auto it = map_.find(name);
        if (it != map_.end()) {
            map_.erase(it);
        }
        
        total_evicted_++;
    }
};

// ============================================================================
// Portal (Cursor) Entry
// ============================================================================

struct Portal {
    std::string name;
    std::string stmt_name;  // Empty for simple query portals
    std::vector<std::optional<std::string>> bound_params;
    std::vector<bool> param_nulls;
    std::unique_ptr<sblr::ResultSet> result_set;
    size_t current_row = 0;
    bool is_open = false;
    bool is_binary = false;
};

// ============================================================================
// Session State
// ============================================================================

struct EngineSessionState {
    uint32_t session_id = 0;
    std::string database_name;
    std::string username;
    bool in_transaction = false;
    bool autocommit = true;
    TransactionIsolation isolation_level = TransactionIsolation::READ_COMMITTED;
    
    // Prepared statement cache with LRU eviction
    std::unique_ptr<StatementCache> stmt_cache;
    
    // Portals (cursors)
    std::unordered_map<std::string, std::unique_ptr<Portal>> portals;
    std::shared_mutex portals_mutex;
    
    // Execution context
    core::Database* database = nullptr;
    std::unique_ptr<core::ConnectionContext> conn_ctx;
    std::unique_ptr<sblr::Executor> executor;
    
    // COPY state
    bool in_copy_in = false;
    bool in_copy_out = false;
    size_t copy_bytes_received = 0;
    std::vector<uint8_t> copy_buffer;
    std::string copy_table_name;
    std::vector<std::string> copy_columns;
    
    // Session metadata
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point last_activity;
    std::deque<IPCMessage> outbound_messages;
    std::mutex outbound_mutex;
    
    EngineSessionState() : stmt_cache(std::make_unique<StatementCache>()) {}
};

// ============================================================================
// EngineIPCSessionHandler Implementation
// ============================================================================

EngineIPCSessionHandler::EngineIPCSessionHandler(core::Database* database)
    : database_(database) {
    if (!database_) {
        throw std::invalid_argument("Database cannot be null");
    }
}

EngineIPCSessionHandler::~EngineIPCSessionHandler() {
    // Clean up all sessions
    std::unique_lock<std::shared_mutex> lock(sessions_mutex_);
    sessions_.clear();
}

EngineSessionState* EngineIPCSessionHandler::getSession(uint32_t session_id) {
    std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it != sessions_.end()) {
        it->second->last_activity = std::chrono::steady_clock::now();
        return it->second.get();
    }
    return nullptr;
}

core::Status EngineIPCSessionHandler::enqueueOutboundMessage(uint32_t session_id,
                                                             IPCMessage&& msg) {
    EngineSessionState* session = getSession(session_id);
    if (!session) {
        return core::Status::NOT_FOUND;
    }

    std::lock_guard<std::mutex> lock(session->outbound_mutex);
    session->outbound_messages.push_back(std::move(msg));
    return core::Status::OK;
}

// ============================================================================
// Lifecycle Methods
// ============================================================================

core::Status EngineIPCSessionHandler::onAttach(uint32_t session_id,
                                               const IPCStartupPayload& startup,
                                               core::ErrorContext* ctx) {
    std::unique_lock<std::shared_mutex> lock(sessions_mutex_);
    std::cerr << "[ipc_debug] onAttach begin session=" << session_id
              << " db=" << boundedString(startup.database, sizeof(startup.database))
              << " user=" << boundedString(startup.user, sizeof(startup.user))
              << " app=" << boundedString(startup.application, sizeof(startup.application))
              << "\n";
    
    if (sessions_.find(session_id) != sessions_.end()) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, 
                    "Session already exists", __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    
    auto session = std::make_unique<EngineSessionState>();
    session->session_id = session_id;
    session->database_name = boundedString(startup.database, sizeof(startup.database));
    session->username = boundedString(startup.user, sizeof(startup.user));
    session->database = database_;
    session->created_at = std::chrono::steady_clock::now();
    session->last_activity = session->created_at;

    const std::string application =
        boundedString(startup.application, sizeof(startup.application));
    const std::string dialect_tag = dialectTagForApplication(application);
    const std::string emulation_mode = emulationModeForDialectTag(dialect_tag);

    auto connect_status = database_->connect(session->conn_ctx, ctx);
    std::cerr << "[ipc_debug] onAttach post-connect session=" << session_id
              << " status=" << static_cast<uint32_t>(connect_status)
              << " ctx=" << (ctx ? ctx->message : std::string()) << "\n";
    if (connect_status != core::Status::OK || !session->conn_ctx) {
        if (ctx && ctx->message.empty()) {
            ctx->set(connect_status != core::Status::OK ? connect_status
                                                        : core::Status::CONNECTION_FAILURE,
                     "Failed to initialize IPC engine connection context",
                     __FILE__, __LINE__, __func__);
        }
        return connect_status != core::Status::OK ? connect_status
                                                  : core::Status::CONNECTION_FAILURE;
    }

    session->conn_ctx->setAutocommitMode(true);
    session->conn_ctx->set_dialect_tag(dialect_tag);
    if (!application.empty()) {
        session->conn_ctx->setSessionVariable("APPLICATION_NAME", application);
    }
    seedDialectSessionVariables(session->conn_ctx.get(), dialect_tag);
    seedDialectSessionIdentity(
        session->conn_ctx.get(), dialect_tag, session->database_name, session->username);

    auto* catalog = database_->catalog_manager();
    if (session->username.empty()) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT,
                     "IPC attach requires a session user",
                     __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }
    if (!catalog) {
        if (ctx) {
            ctx->set(core::Status::INTERNAL_ERROR,
                     "Catalog manager unavailable for IPC attach",
                     __FILE__, __LINE__, __func__);
        }
        return core::Status::INTERNAL_ERROR;
    }

    core::CatalogManager::UserInfo user_info;
    core::ErrorContext user_ctx;
    auto user_status = catalog->getUserByName(session->username, user_info, &user_ctx);
    std::cerr << "[ipc_debug] onAttach getUserByName session=" << session_id
              << " user=" << session->username
              << " status=" << static_cast<uint32_t>(user_status)
              << " ctx=" << user_ctx.message << "\n";
    if (user_status != core::Status::OK) {
        std::string message = user_ctx.message.empty()
                                  ? "IPC attach user not found: " + session->username
                                  : user_ctx.message;
        if (ctx) {
            ctx->set(user_status, message.c_str(), __FILE__, __LINE__, __func__);
        }
        return user_status;
    }
    if (!user_info.is_active) {
        std::string message = "IPC attach user is disabled: " + session->username;
        if (ctx) {
            ctx->set(core::Status::PERMISSION_DENIED,
                     message.c_str(),
                     __FILE__, __LINE__, __func__);
        }
        return core::Status::PERMISSION_DENIED;
    }

    const bool firebird_sysdba_override =
        dialect_tag == "firebird" && equalsIgnoreCaseAscii(session->username, "SYSDBA");
    session->conn_ctx->setCurrentUser(user_info.user_id,
                                      user_info.is_superuser || firebird_sysdba_override);

    core::CatalogManager::SessionInfo session_info;
    core::ErrorContext session_ctx;
    auto session_status = catalog->createSession(user_info.user_id,
                                                 session->conn_ctx->authKeyId(),
                                                 emulation_mode,
                                                 session_info,
                                                 &session_ctx);
    std::cerr << "[ipc_debug] onAttach createSession session=" << session_id
              << " user=" << session->username
              << " emulation=" << emulation_mode
              << " status=" << static_cast<uint32_t>(session_status)
              << " ctx=" << session_ctx.message << "\n";
    if (session_status != core::Status::OK) {
        const std::string message =
            session_ctx.message.empty() ? "Failed to create IPC catalog session"
                                        : session_ctx.message;
        if (ctx) {
            ctx->set(session_status, message.c_str(), __FILE__, __LINE__, __func__);
        }
        return session_status;
    }

    session->conn_ctx->setCurrentSchemaId(session_info.current_schema_id);
    if (!session_info.search_path.empty()) {
        session->conn_ctx->set_search_path(session_info.search_path);
    }

    core::CatalogManager::SchemaInfo schema_info;
    if (catalog->getSchema(session_info.current_schema_id, schema_info, nullptr) ==
        core::Status::OK) {
        std::string schema_path;
        if (catalog->getSchemaPath(session_info.current_schema_id, schema_path, nullptr) !=
                core::Status::OK ||
            schema_path.empty()) {
            schema_path = schema_info.schema_name;
        }
        session->conn_ctx->set_current_schema(schema_path);
        if (session_info.search_path.empty() && !schema_path.empty()) {
            session->conn_ctx->set_search_path({schema_path});
        }
    }

    session->conn_ctx->setSessionContext(session_info.session_id,
                                         session->conn_ctx->authKeyId(),
                                         session_info.emulation_mode,
                                         session_info.policy_epoch_global,
                                         session_info.policy_epoch_table);

    syncSessionTransactionState(session->conn_ctx.get(),
                                session->in_transaction,
                                session->autocommit);

    session->executor = std::make_unique<sblr::Executor>(database_);
    session->executor->setConnectionContext(session->conn_ctx.get());
    
    sessions_[session_id] = std::move(session);
    std::cerr << "[ipc_debug] onAttach complete session=" << session_id << "\n";
    
    return core::Status::OK;
}

core::Status EngineIPCSessionHandler::onDetach(uint32_t session_id,
                                               core::ErrorContext* ctx) {
    std::unique_lock<std::shared_mutex> lock(sessions_mutex_);
    
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, 
                    "Session not found", __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }
    
    // Rollback any open transaction
    if (it->second->in_transaction) {
        auto& executor = it->second->executor;
        if (executor) {
            std::vector<uint8_t> rollback_bytecode = {static_cast<uint8_t>(sblr::Opcode::ROLLBACK)};
            ConnectionContextGuard ctx_guard(it->second->conn_ctx.get());
            executor->execute(rollback_bytecode);
        }
    }
    
    sessions_.erase(it);
    return core::Status::OK;
}

// ============================================================================
// Query Execution Methods
// ============================================================================

core::Status EngineIPCSessionHandler::onSimpleQuery(uint32_t session_id,
                                                    const std::string& sql,
                                                    core::ErrorContext* ctx) {
    EngineSessionState* session = getSession(session_id);
    if (!session) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, 
                    "Session not found", __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }

    if (sql.empty()) {
        if (ctx != nullptr) {
            ctx->set(core::Status::INVALID_ARGUMENT,
                     "IPC SQL text path requires a non-empty statement",
                     __FILE__, __LINE__, __func__);
        }
        return sendError(session_id, "22023", "IPC SQL text path requires a non-empty statement");
    }

    return rejectEngineSqlTextPath(session_id, "simple-query", this, ctx);
}

core::Status EngineIPCSessionHandler::onCompiledQuery(uint32_t session_id,
                                                      const std::vector<uint8_t>& bytecode,
                                                      const std::string& original_sql,
                                                      core::ErrorContext* ctx) {
    EngineSessionState* session = getSession(session_id);
    if (!session) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND,
                    "Session not found", __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }

    if (bytecode.empty()) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT,
                    "Compiled query bytecode is empty", __FILE__, __LINE__, __func__);
        }
        return sendError(session_id, "22023", "Compiled query bytecode is empty");
    }

    if (compiledQueryIpcTraceEnabled()) {
        core::ErrorContext validation_ctx;
        const core::Status validation_status =
            scratchbird::sblr::validateBytecode(bytecode, &validation_ctx);
        std::ostringstream trace;
        trace << "[compiled_query_trace] side=engine"
              << " session=" << session_id
              << " dialect="
              << (session->conn_ctx ? session->conn_ctx->dialect_tag() : std::string())
              << " bytecode_len=" << bytecode.size()
              << " validation=" << static_cast<int>(validation_status)
              << " validation_msg=" << validation_ctx.message
              << " bytecode=" << summarizeBytecode(bytecode)
              << " sql=" << original_sql;
        appendIpcDebugLine(trace.str());
    }

    if (session->conn_ctx &&
        equalsIgnoreCaseAscii(session->conn_ctx->dialect_tag(), "firebird")) {
        std::ostringstream trace;
        trace << "[ipc_debug] compiled_query session=" << session_id
              << " sql=" << original_sql
              << " current_schema=" << session->conn_ctx->current_schema()
              << " search_path=" << joinSearchPath(session->conn_ctx->search_path());
        appendIpcDebugLine(trace.str());
    }

    ConnectionContextGuard ctx_guard(session->conn_ctx.get());
    auto exec_result = session->executor->execute(bytecode);
    if (!exec_result.success()) {
        return sendError(session_id, "XX000", exec_result.error());
    }

    if (exec_result.hasResultSet()) {
        auto* rs = exec_result.resultSet();
        if (session->conn_ctx &&
            equalsIgnoreCaseAscii(session->conn_ctx->dialect_tag(), "firebird") &&
            original_sql.find("CURRENT_SCHEMA") != std::string::npos &&
            rs->rowCount() > 0) {
            std::ostringstream trace;
            trace << "[ipc_debug] result session=" << session_id
                  << " sql=" << original_sql;
            for (size_t col = 0; col < rs->columnCount(); ++col) {
                trace << " col" << col << "=" << rs->getValue(0, col).toString();
            }
            appendIpcDebugLine(trace.str());
        }
        std::vector<IPCFieldDesc> fields;
        for (size_t i = 0; i < rs->columnCount(); ++i) {
            IPCFieldDesc field{};
            std::strncpy(field.name, rs->columnName(i).c_str(), sizeof(field.name) - 1);
            field.name[sizeof(field.name) - 1] = '\0';
            field.type_oid = static_cast<uint32_t>(rs->columnType(i));
            applyIpcFieldTypeMetadata(field, rs->columnType(i));
            fields.push_back(field);
        }

        auto status = sendRowDescription(session_id, fields);
        if (status != core::Status::OK) {
            return status;
        }

        for (size_t row = 0; row < rs->rowCount(); ++row) {
            std::vector<std::optional<std::string>> values;
            for (size_t col = 0; col < rs->columnCount(); ++col) {
                const auto& val = rs->getValue(row, col);
                if (val.isNull()) {
                    values.push_back(std::nullopt);
                } else {
                    values.push_back(val.toString());
                }
            }
            status = sendDataRow(session_id, values);
            if (status != core::Status::OK) {
                return status;
            }
        }

        return sendCommandComplete(session_id,
                                   "SELECT " + std::to_string(rs->rowCount()),
                                   rs->rowCount());
    }

    std::string tag = "OK";
    const uint64_t affected = exec_result.affectedCount();
    if (!original_sql.empty()) {
        std::string upper_sql = original_sql;
        for (auto& c : upper_sql) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        if (upper_sql.find("INSERT") == 0) {
            tag = "INSERT 0 " + std::to_string(affected);
        } else if (upper_sql.find("UPDATE") == 0) {
            tag = "UPDATE " + std::to_string(affected);
        } else if (upper_sql.find("DELETE") == 0) {
            tag = "DELETE " + std::to_string(affected);
        }
    }

    return sendCommandComplete(session_id, tag, affected);
}

core::Status EngineIPCSessionHandler::onParse(uint32_t session_id,
                                             const std::string& stmt_name,
                                             const std::string& sql,
                                             core::ErrorContext* ctx) {
    EngineSessionState* session = getSession(session_id);
    if (!session) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, 
                    "Session not found", __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }

    if (stmt_name.empty()) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT,
                    "Statement name is required for IPC SQL parse",
                    __FILE__, __LINE__, __func__);
        }
        return sendError(session_id, "22023", "Statement name is required for IPC SQL parse");
    }
    if (sql.empty()) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT,
                    "IPC SQL parse requires a non-empty statement",
                    __FILE__, __LINE__, __func__);
        }
        return sendError(session_id, "22023", "IPC SQL parse requires a non-empty statement");
    }

    return rejectEngineSqlTextPath(session_id, "parse", this, ctx);
}

core::Status EngineIPCSessionHandler::onCompiledParse(uint32_t session_id,
                                                      const std::string& stmt_name,
                                                      const std::vector<uint8_t>& bytecode,
                                                      const std::string& original_sql,
                                                      core::ErrorContext* ctx) {
    EngineSessionState* session = getSession(session_id);
    if (!session) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND,
                    "Session not found", __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }

    if (stmt_name.empty()) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT,
                    "Statement name is required for compiled parse",
                    __FILE__, __LINE__, __func__);
        }
        return sendError(session_id, "22023", "Statement name is required for compiled parse");
    }

    if (bytecode.empty()) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT,
                    "Compiled parse bytecode is empty", __FILE__, __LINE__, __func__);
        }
        return sendError(session_id, "22023", "Compiled parse bytecode is empty");
    }

    auto prepared = std::make_unique<PreparedStatement>();
    prepared->name = stmt_name;
    prepared->sql = original_sql;
    prepared->bytecode = bytecode;
    prepared->created_at = std::chrono::steady_clock::now();
    prepared->last_used = prepared->created_at;
    prepared->is_valid = true;

    session->stmt_cache->put(stmt_name, std::move(prepared));
    return core::Status::OK;
}

core::Status EngineIPCSessionHandler::onBind(uint32_t session_id,
                                            const std::string& portal_name,
                                            const std::string& stmt_name,
                                            const std::vector<std::optional<std::string>>& params,
                                            const std::vector<bool>& param_nulls,
                                            core::ErrorContext* ctx) {
    EngineSessionState* session = getSession(session_id);
    if (!session) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, 
                    "Session not found", __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }
    
    // Look up prepared statement
    PreparedStatement* stmt = session->stmt_cache->get(stmt_name);
    if (!stmt) {
        return sendError(session_id, "26000", "Prepared statement not found: " + stmt_name);
    }
    
    // Create portal
    auto portal = std::make_unique<Portal>();
    portal->name = portal_name;
    portal->stmt_name = stmt_name;
    portal->is_open = true;
    portal->bound_params = params;
    portal->param_nulls = param_nulls;
    
    // Store portal
    {
        std::unique_lock<std::shared_mutex> lock(session->portals_mutex);
        session->portals[portal_name] = std::move(portal);
    }
    
    return sendBindComplete(session_id);
}

core::Status EngineIPCSessionHandler::onExecute(uint32_t session_id,
                                               const std::string& portal_name,
                                               uint32_t max_rows,
                                               core::ErrorContext* ctx) {
    EngineSessionState* session = getSession(session_id);
    if (!session) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, 
                    "Session not found", __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }
    
    // Look up portal
    Portal* portal = nullptr;
    {
        std::shared_lock<std::shared_mutex> lock(session->portals_mutex);
        auto it = session->portals.find(portal_name);
        if (it == session->portals.end()) {
            return sendError(session_id, "34000", "Portal not found: " + portal_name);
        }
        portal = it->second.get();
    }
    
    if (!portal->is_open) {
        return sendError(session_id, "34000", "Portal is closed: " + portal_name);
    }
    
    // Look up prepared statement if this is a bound portal
    PreparedStatement* stmt = nullptr;
    if (!portal->stmt_name.empty()) {
        stmt = session->stmt_cache->get(portal->stmt_name);
        if (!stmt) {
            return sendError(session_id, "26000", "Prepared statement not found: " + portal->stmt_name);
        }
    }
    
    // Execute
    std::vector<uint8_t> bytecode;
    if (stmt) {
        bytecode = stmt->bytecode;
        stmt->execution_count++;
    } else {
        // Simple query portal - would need to store the SQL
        return sendError(session_id, "XX000", "Unbound portal execution not implemented");
    }
    
    // Set parameters if bound
    if (!portal->bound_params.empty()) {
        // Convert optional<string> to string
        std::vector<std::string> params;
        params.reserve(portal->bound_params.size());
        for (const auto& opt : portal->bound_params) {
            params.push_back(opt.value_or(""));
        }
        session->executor->setParameters(params, portal->param_nulls);
    }

    if (session->conn_ctx &&
        equalsIgnoreCaseAscii(session->conn_ctx->dialect_tag(), "firebird")) {
        std::ostringstream trace;
        trace << "[ipc_debug] execute session=" << session_id
              << " portal=" << portal_name
              << " stmt=" << (stmt ? stmt->sql : std::string())
              << " current_schema=" << session->conn_ctx->current_schema()
              << " search_path=" << joinSearchPath(session->conn_ctx->search_path());
        appendIpcDebugLine(trace.str());
    }
    
    ConnectionContextGuard ctx_guard(session->conn_ctx.get());
    auto exec_result = session->executor->execute(bytecode);
    
    if (!exec_result.success()) {
        return sendError(session_id, "XX000", exec_result.error());
    }
    
    // Handle result
    if (exec_result.hasResultSet()) {
        auto* rs = exec_result.resultSet();
        if (session->conn_ctx &&
            equalsIgnoreCaseAscii(session->conn_ctx->dialect_tag(), "firebird") &&
            stmt != nullptr &&
            stmt->sql.find("CURRENT_SCHEMA") != std::string::npos &&
            rs->rowCount() > 0) {
            std::ostringstream trace;
            trace << "[ipc_debug] portal_result session=" << session_id
                  << " portal=" << portal_name
                  << " stmt=" << stmt->sql;
            for (size_t col = 0; col < rs->columnCount(); ++col) {
                trace << " col" << col << "=" << rs->getValue(0, col).toString();
            }
            appendIpcDebugLine(trace.str());
        }
        
        // If this is the first execute, send row description
        if (!portal->result_set) {
            std::vector<IPCFieldDesc> fields;
            for (size_t i = 0; i < rs->columnCount(); ++i) {
                IPCFieldDesc field;
                std::strncpy(field.name, rs->columnName(i).c_str(), sizeof(field.name) - 1);
                field.name[sizeof(field.name) - 1] = '\0';
                field.type_oid = static_cast<uint32_t>(rs->columnType(i));
                applyIpcFieldTypeMetadata(field, rs->columnType(i));
                fields.push_back(field);
            }
            
            auto status = sendRowDescription(session_id, fields);
            if (status != core::Status::OK) return status;
            
            portal->result_set = std::make_unique<sblr::ResultSet>(*rs);
        }
        
        // Send rows up to max_rows
        size_t rows_to_send = max_rows > 0 ? std::min(static_cast<size_t>(max_rows), 
                                                       rs->rowCount() - portal->current_row)
                                           : rs->rowCount() - portal->current_row;
        
        for (size_t i = 0; i < rows_to_send; ++i) {
            std::vector<std::optional<std::string>> values;
            for (size_t col = 0; col < rs->columnCount(); ++col) {
                const auto& val = rs->getValue(portal->current_row, col);
                if (val.isNull()) {
                    values.push_back(std::nullopt);
                } else {
                    values.push_back(val.toString());
                }
            }
            auto status = sendDataRow(session_id, values);
            if (status != core::Status::OK) return status;
            portal->current_row++;
        }
        
        // If we've sent all rows, send command complete
        if (portal->current_row >= rs->rowCount()) {
            std::string tag = "SELECT " + std::to_string(rs->rowCount());
            portal->is_open = false;
            return sendCommandComplete(session_id, tag, rs->rowCount());
        }
        
        // More rows available
        return core::Status::OK;
    } else {
        portal->is_open = false;
        std::string tag;
        uint64_t affected = exec_result.affectedCount();
        
        if (stmt) {
            std::string upper_sql = stmt->sql;
            for (auto& c : upper_sql) c = std::toupper(c);
            
            if (upper_sql.find("INSERT") == 0) {
                tag = "INSERT 0 " + std::to_string(affected);
            } else if (upper_sql.find("UPDATE") == 0) {
                tag = "UPDATE " + std::to_string(affected);
            } else if (upper_sql.find("DELETE") == 0) {
                tag = "DELETE " + std::to_string(affected);
            } else {
                tag = "OK";
            }
        } else {
            tag = "OK";
        }
        
        return sendCommandComplete(session_id, tag, affected);
    }
}

core::Status EngineIPCSessionHandler::onClose(uint32_t session_id,
                                             char type,
                                             const std::string& name,
                                             core::ErrorContext* ctx) {
    EngineSessionState* session = getSession(session_id);
    if (!session) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, 
                    "Session not found", __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }
    
    if (type == 'S') {
        // Close prepared statement
        session->stmt_cache->remove(name);
    } else if (type == 'P') {
        // Close portal
        std::unique_lock<std::shared_mutex> lock(session->portals_mutex);
        session->portals.erase(name);
    }
    
    return sendCloseComplete(session_id);
}

core::Status EngineIPCSessionHandler::onSync(uint32_t session_id,
                                            core::ErrorContext* ctx) {
    // Sync is a no-op in autocommit mode, or commits in explicit transaction mode
    return sendReady(session_id, 0);
}

// ============================================================================
// Transaction Methods
// ============================================================================

core::Status EngineIPCSessionHandler::onBegin(uint32_t session_id,
                                             core::ErrorContext* ctx) {
    EngineSessionState* session = getSession(session_id);
    if (!session) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, 
                    "Session not found", __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }

    if (!session->conn_ctx) {
        return sendError(session_id, "XX000", "Connection context not available");
    }

    if (session->conn_ctx) {
        session->conn_ctx->setAutocommitSuspended(true);
    }

    syncSessionTransactionState(session->conn_ctx.get(),
                                session->in_transaction,
                                session->autocommit);
    session->in_transaction = true;
    session->autocommit = false;
    return core::Status::OK;
}

core::Status EngineIPCSessionHandler::onCommit(uint32_t session_id,
                                              core::ErrorContext* ctx) {
    EngineSessionState* session = getSession(session_id);
    if (!session) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, 
                    "Session not found", __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }

    if (!session->conn_ctx) {
        return sendError(session_id, "XX000", "Connection context not available");
    }

    syncSessionTransactionState(session->conn_ctx.get(),
                                session->in_transaction,
                                session->autocommit);
    if (!session->in_transaction) {
        return sendError(session_id, "25P01", "No active transaction");
    }

    ConnectionContextGuard ctx_guard(session->conn_ctx.get());
    auto status = session->conn_ctx->commit(ctx);
    if (status != core::Status::OK) {
        const std::string message =
            (ctx != nullptr && !ctx->message.empty()) ? ctx->message : "Commit failed";
        return sendError(session_id, "XX000", message);
    }

    if (session->conn_ctx) {
        session->conn_ctx->setAutocommitSuspended(false);
    }

    syncSessionTransactionState(session->conn_ctx.get(),
                                session->in_transaction,
                                session->autocommit);
    
    return core::Status::OK;
}

core::Status EngineIPCSessionHandler::onRollback(uint32_t session_id,
                                                core::ErrorContext* ctx) {
    EngineSessionState* session = getSession(session_id);
    if (!session) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, 
                    "Session not found", __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }

    if (!session->conn_ctx) {
        return sendError(session_id, "XX000", "Connection context not available");
    }

    syncSessionTransactionState(session->conn_ctx.get(),
                                session->in_transaction,
                                session->autocommit);
    if (!session->in_transaction) {
        return sendError(session_id, "25P01", "No active transaction");
    }

    ConnectionContextGuard ctx_guard(session->conn_ctx.get());
    auto status = session->conn_ctx->rollback(ctx);
    if (status != core::Status::OK) {
        const std::string message =
            (ctx != nullptr && !ctx->message.empty()) ? ctx->message : "Rollback failed";
        return sendError(session_id, "XX000", message);
    }

    if (session->conn_ctx) {
        session->conn_ctx->setAutocommitSuspended(false);
    }

    syncSessionTransactionState(session->conn_ctx.get(),
                                session->in_transaction,
                                session->autocommit);
    
    return core::Status::OK;
}

core::Status EngineIPCSessionHandler::onSavepoint(uint32_t session_id,
                                                 const std::string& name,
                                                 core::ErrorContext* ctx) {
    EngineSessionState* session = getSession(session_id);
    if (!session) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, 
                    "Session not found", __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }
    
    if (!session->in_transaction || session->conn_ctx == nullptr) {
        return sendError(session_id, "25P01", "No active transaction");
    }

    ConnectionContextGuard ctx_guard(session->conn_ctx.get());
    const core::Status status = session->conn_ctx->createSavepoint(name, ctx);
    if (status != core::Status::OK) {
        const std::string message =
            (ctx && !ctx->message.empty()) ? ctx->message : "Failed to create savepoint";
        return sendError(session_id, "XX000", message);
    }

    return core::Status::OK;
}

// ============================================================================
// COPY Methods
// ============================================================================

core::Status EngineIPCSessionHandler::onCopyInStart(uint32_t session_id,
                                                   core::ErrorContext* ctx) {
    EngineSessionState* session = getSession(session_id);
    if (!session) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, 
                    "Session not found", __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }
    
    session->in_copy_in = true;
    session->copy_buffer.clear();
    
    return sendCopyInRequest(session_id);
}

core::Status EngineIPCSessionHandler::onCopyData(uint32_t session_id,
                                                const uint8_t* data, size_t len,
                                                core::ErrorContext* ctx) {
    EngineSessionState* session = getSession(session_id);
    if (!session) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, 
                    "Session not found", __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }
    
    if (!session->in_copy_in && !session->in_copy_out) {
        return sendError(session_id, "57014", "Not in COPY mode");
    }
    
    if (session->in_copy_in) {
        // Stream COPY data to executor - process incrementally
        if (session->executor) {
            // Parse and execute COPY data as it arrives
            // This avoids buffering large datasets in memory
            auto status = processCopyDataStream(session, data, len, ctx);
            if (status != core::Status::OK) {
                return status;
            }
        } else {
            // Executor not ready, buffer temporarily with strict limits
            constexpr size_t MAX_COPY_BUFFER_SIZE = 256 * 1024;  // 256KB max (reduced from 1MB)
            if (session->copy_buffer.size() + len > MAX_COPY_BUFFER_SIZE) {
                return sendError(session_id, "54000", 
                    "COPY buffer exceeded maximum size - executor not ready");
            }
            session->copy_buffer.insert(session->copy_buffer.end(), data, data + len);
        }
        
        // Update COPY statistics
        session->copy_bytes_received += len;
    }
    
    return core::Status::OK;
}

core::Status EngineIPCSessionHandler::onCopyDone(uint32_t session_id,
                                                core::ErrorContext* ctx) {
    EngineSessionState* session = getSession(session_id);
    if (!session) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, 
                    "Session not found", __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }
    
    if (session->in_copy_in) {
        // Process the accumulated COPY data
        // This would involve parsing the COPY format and executing INSERTs
        session->in_copy_in = false;
        session->copy_buffer.clear();
    } else if (session->in_copy_out) {
        session->in_copy_out = false;
    }
    
    return sendCopyComplete(session_id);
}

core::Status EngineIPCSessionHandler::onCopyFail(uint32_t session_id,
                                                const std::string& reason,
                                                core::ErrorContext* ctx) {
    EngineSessionState* session = getSession(session_id);
    if (!session) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, 
                    "Session not found", __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }
    
    session->in_copy_in = false;
    session->in_copy_out = false;
    session->copy_buffer.clear();
    
    return sendError(session_id, "57014", "COPY failed: " + reason);
}

// ============================================================================
// Cancel and Notifications
// ============================================================================

core::Status EngineIPCSessionHandler::onCancel(uint32_t session_id,
                                              core::ErrorContext* ctx) {
    EngineSessionState* session = getSession(session_id);
    if (!session) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND, 
                    "Session not found", __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }
    
    if (session->executor) {
        session->executor->requestCancellation();
    }
    
    return core::Status::OK;
}

core::Status EngineIPCSessionHandler::onSubscribe(uint32_t session_id,
                                                 const std::string& channel,
                                                 core::ErrorContext* ctx) {
    (void)session_id;
    (void)channel;
    // Notification system would be implemented here
    return core::Status::OK;
}

core::Status EngineIPCSessionHandler::onUnsubscribe(uint32_t session_id,
                                                   const std::string& channel,
                                                   core::ErrorContext* ctx) {
    (void)session_id;
    (void)channel;
    // Notification system would be implemented here
    return core::Status::OK;
}

// ============================================================================
// Response Methods (Implementations would send to session)
// ============================================================================

core::Status EngineIPCSessionHandler::sendRowDescription(uint32_t session_id,
                                                        const std::vector<IPCFieldDesc>& fields) {
    IPCMessage msg(IPCMessageType::ROW_DESCRIPTION, session_id);
    IPCRowDescriptionPayload payload{};
    payload.num_fields = static_cast<uint16_t>(fields.size());
    msg.payload.resize(sizeof(payload) + fields.size() * sizeof(IPCFieldDesc));
    std::memcpy(msg.payload.data(), &payload, sizeof(payload));
    if (!fields.empty()) {
        std::memcpy(msg.payload.data() + sizeof(payload),
                    fields.data(),
                    fields.size() * sizeof(IPCFieldDesc));
    }
    return enqueueOutboundMessage(session_id, std::move(msg));
}

core::Status EngineIPCSessionHandler::sendDataRow(uint32_t session_id,
                                                 const std::vector<std::optional<std::string>>& values) {
    IPCMessage msg(IPCMessageType::DATA_ROW, session_id);
    IPCDataRowPayload payload{};
    payload.num_fields = static_cast<uint16_t>(values.size());

    size_t payload_size = sizeof(payload);
    for (const auto& value : values) {
        payload_size += sizeof(int32_t);
        if (value.has_value()) {
            payload_size += value->size();
        }
    }

    msg.payload.resize(payload_size);
    std::memcpy(msg.payload.data(), &payload, sizeof(payload));
    size_t offset = sizeof(payload);
    for (const auto& value : values) {
        int32_t len = value.has_value() ? static_cast<int32_t>(value->size()) : -1;
        std::memcpy(msg.payload.data() + offset, &len, sizeof(len));
        offset += sizeof(len);
        if (value.has_value() && !value->empty()) {
            std::memcpy(msg.payload.data() + offset, value->data(), value->size());
            offset += value->size();
        }
    }
    return enqueueOutboundMessage(session_id, std::move(msg));
}

core::Status EngineIPCSessionHandler::sendCommandComplete(uint32_t session_id,
                                                         const std::string& tag,
                                                         uint64_t rows_affected) {
    IPCMessage msg(IPCMessageType::COMMAND_COMPLETE, session_id);
    IPCCommandCompletePayload payload{};
    std::strncpy(payload.tag, tag.c_str(), sizeof(payload.tag) - 1);
    payload.tag[sizeof(payload.tag) - 1] = '\0';
    payload.rows_affected = rows_affected;
    payload.last_insert_id = 0;
    msg.payload.resize(sizeof(payload));
    std::memcpy(msg.payload.data(), &payload, sizeof(payload));
    return enqueueOutboundMessage(session_id, std::move(msg));
}

core::Status EngineIPCSessionHandler::sendError(uint32_t session_id,
                                               const char* sqlstate,
                                               const std::string& message) {
    IPCMessage msg(IPCMessageType::ERROR_RESPONSE, session_id);
    IPCErrorPayload payload{};
    std::strncpy(payload.sqlstate, sqlstate != nullptr ? sqlstate : "XX000",
                 sizeof(payload.sqlstate) - 1);
    payload.sqlstate[sizeof(payload.sqlstate) - 1] = '\0';
    std::strncpy(payload.message, message.c_str(), sizeof(payload.message) - 1);
    payload.message[sizeof(payload.message) - 1] = '\0';
    msg.payload.resize(sizeof(payload));
    std::memcpy(msg.payload.data(), &payload, sizeof(payload));
    return enqueueOutboundMessage(session_id, std::move(msg));
}

core::Status EngineIPCSessionHandler::sendNotice(uint32_t session_id,
                                                const std::string& message) {
    IPCMessage msg(IPCMessageType::NOTICE, session_id);
    IPCErrorPayload payload{};
    std::strncpy(payload.sqlstate, "00000", sizeof(payload.sqlstate) - 1);
    std::strncpy(payload.message, message.c_str(), sizeof(payload.message) - 1);
    msg.payload.resize(sizeof(payload));
    std::memcpy(msg.payload.data(), &payload, sizeof(payload));
    return enqueueOutboundMessage(session_id, std::move(msg));
}

core::Status EngineIPCSessionHandler::sendReady(uint32_t session_id,
                                               uint32_t server_features) {
    IPCMessage msg(IPCMessageType::READY, session_id);
    IPCReadyPayload payload{};
    payload.session_id = session_id;
    payload.server_features = server_features;
    std::strncpy(payload.server_version, "ScratchBird IPC/1.1",
                 sizeof(payload.server_version) - 1);
    msg.payload.resize(sizeof(payload));
    std::memcpy(msg.payload.data(), &payload, sizeof(payload));
    return enqueueOutboundMessage(session_id, std::move(msg));
}

core::Status EngineIPCSessionHandler::sendParseComplete(uint32_t session_id) {
    IPCMessage msg(IPCMessageType::PARSE_COMPLETE, session_id);
    return enqueueOutboundMessage(session_id, std::move(msg));
}

core::Status EngineIPCSessionHandler::sendBindComplete(uint32_t session_id) {
    IPCMessage msg(IPCMessageType::BIND_COMPLETE, session_id);
    return enqueueOutboundMessage(session_id, std::move(msg));
}

core::Status EngineIPCSessionHandler::sendCloseComplete(uint32_t session_id) {
    IPCMessage msg(IPCMessageType::CLOSE_COMPLETE, session_id);
    return enqueueOutboundMessage(session_id, std::move(msg));
}

core::Status EngineIPCSessionHandler::sendCopyInRequest(uint32_t session_id) {
    IPCMessage msg(IPCMessageType::COPY_IN_REQUEST, session_id);
    IPCCopyInRequestPayload payload{};
    payload.format = 0;
    payload.num_columns = 0;
    msg.payload.resize(sizeof(payload));
    std::memcpy(msg.payload.data(), &payload, sizeof(payload));
    return enqueueOutboundMessage(session_id, std::move(msg));
}

core::Status EngineIPCSessionHandler::sendCopyOutResponse(uint32_t session_id) {
    IPCMessage msg(IPCMessageType::COPY_OUT_RESPONSE, session_id);
    IPCCopyOutResponsePayload payload{};
    payload.format = 0;
    payload.num_columns = 0;
    msg.payload.resize(sizeof(payload));
    std::memcpy(msg.payload.data(), &payload, sizeof(payload));
    return enqueueOutboundMessage(session_id, std::move(msg));
}

core::Status EngineIPCSessionHandler::sendCopyData(uint32_t session_id,
                                                  const uint8_t* data, size_t len) {
    IPCMessage msg(IPCMessageType::COPY_DATA, session_id);
    IPCCopyDataPayload payload{};
    payload.chunk_id = 0;
    payload.length = static_cast<uint32_t>(len);
    msg.payload.resize(sizeof(payload) + len);
    std::memcpy(msg.payload.data(), &payload, sizeof(payload));
    if (len > 0 && data != nullptr) {
        std::memcpy(msg.payload.data() + sizeof(payload), data, len);
    }
    return enqueueOutboundMessage(session_id, std::move(msg));
}

core::Status EngineIPCSessionHandler::sendCopyComplete(uint32_t session_id) {
    IPCMessage msg(IPCMessageType::COPY_COMPLETE, session_id);
    return enqueueOutboundMessage(session_id, std::move(msg));
}

core::Status EngineIPCSessionHandler::sendTxnComplete(uint32_t session_id) {
    IPCMessage msg(IPCMessageType::TXN_COMPLETE, session_id);
    return enqueueOutboundMessage(session_id, std::move(msg));
}

core::Status EngineIPCSessionHandler::sendNotification(uint32_t session_id,
                                                      const std::string& channel,
                                                      const std::string& payload) {
    IPCMessage msg(IPCMessageType::NOTIFY_DELIVER, session_id);
    IPCNotifyPayload notify{};
    notify.channel_id = 0;
    std::strncpy(notify.channel, channel.c_str(), sizeof(notify.channel) - 1);
    notify.channel[sizeof(notify.channel) - 1] = '\0';
    std::strncpy(notify.payload, payload.c_str(), sizeof(notify.payload) - 1);
    notify.payload[sizeof(notify.payload) - 1] = '\0';
    notify.timestamp = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    msg.payload.resize(sizeof(notify));
    std::memcpy(msg.payload.data(), &notify, sizeof(notify));
    return enqueueOutboundMessage(session_id, std::move(msg));
}

core::Status EngineIPCSessionHandler::drainOutboundMessages(uint32_t session_id,
                                                            std::vector<IPCMessage>& messages,
                                                            core::ErrorContext* ctx) {
    EngineSessionState* session = getSession(session_id);
    if (!session) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND,
                    "Session not found", __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }

    std::lock_guard<std::mutex> lock(session->outbound_mutex);
    while (!session->outbound_messages.empty()) {
        messages.push_back(std::move(session->outbound_messages.front()));
        session->outbound_messages.pop_front();
    }
    return core::Status::OK;
}

// ============================================================================
// COPY Streaming Implementation
// ============================================================================

core::Status EngineIPCSessionHandler::processCopyDataStream(EngineSessionState* session,
                                                             const uint8_t* data, 
                                                             size_t len,
                                                             core::ErrorContext* ctx) {
    // Stream COPY data to executor using direct bytecode generation (Option C)
    // This bypasses SQL parsing entirely by generating SBLR bytecode directly from CSV
    
    if (!session || !session->executor) {
        return core::Status::NOT_FOUND;
    }
    
    // Batch rows for efficient bytecode generation
    constexpr size_t BATCH_SIZE = 100;
    std::vector<std::vector<std::optional<std::string>>> batch;
    batch.reserve(BATCH_SIZE);
    
    // Parse line by line and batch
    size_t line_start = 0;
    for (size_t i = 0; i <= len; ++i) {
        // Check for end of line or end of buffer
        if (i == len || data[i] == '\n') {
            size_t line_len = i - line_start;
            
            // Skip empty lines
            if (line_len > 0) {
                // Parse CSV/TSV fields from the line
                std::string line(reinterpret_cast<const char*>(data + line_start), line_len);
                std::vector<std::optional<std::string>> fields = parseCopyLine(line);
                
                if (!fields.empty()) {
                    batch.push_back(std::move(fields));
                    session->copy_bytes_received += line_len;
                    
                    // Execute batch when full
                    if (batch.size() >= BATCH_SIZE) {
                        auto bytecode = CopyBytecodeGenerator::generateInsertBytecode(
                            session->copy_table_name,
                            session->copy_columns,
                            batch);
                        
                        if (!bytecode.empty()) {
                            // Execute the bytecode
                            ConnectionContextGuard ctx_guard(session->conn_ctx.get());
                            auto result = session->executor->execute(bytecode);
                            if (!result.success()) {
                                if (ctx) {
                                    ctx->set(core::Status::INTERNAL_ERROR,
                                            ("COPY execution failed: " + result.error()).c_str(),
                                            __FILE__, __LINE__, __func__);
                                }
                                return core::Status::INTERNAL_ERROR;
                            }
                        }
                        
                        batch.clear();
                        batch.reserve(BATCH_SIZE);
                    }
                }
            }
            
            line_start = i + 1;
        }
    }
    
    // Execute remaining rows in batch
    if (!batch.empty() && !session->copy_table_name.empty()) {
        auto bytecode = CopyBytecodeGenerator::generateInsertBytecode(
            session->copy_table_name,
            session->copy_columns,
            batch);
        
        if (!bytecode.empty()) {
            ConnectionContextGuard ctx_guard(session->conn_ctx.get());
            auto result = session->executor->execute(bytecode);
            if (!result.success()) {
                if (ctx) {
                    ctx->set(core::Status::INTERNAL_ERROR,
                            ("COPY execution failed: " + result.error()).c_str(),
                            __FILE__, __LINE__, __func__);
                }
                return core::Status::INTERNAL_ERROR;
            }
        }
    }
    
    return core::Status::OK;
}

std::vector<std::optional<std::string>> EngineIPCSessionHandler::parseCopyLine(
    const std::string& line) {
    // Parse a CSV/TSV line into fields
    // Returns std::nullopt for PostgreSQL NULL (\N)
    // Returns empty string "" for actual empty values
    std::vector<std::optional<std::string>> fields;
    std::string current;
    bool in_quotes = false;
    char delimiter = '\t';  // Default to tab-delimited
    
    // Auto-detect delimiter: if no tabs but commas present, use comma
    if (line.find('\t') == std::string::npos && line.find(',') != std::string::npos) {
        delimiter = ',';
    }
    
    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];
        
        if (c == '"') {
            if (in_quotes && i + 1 < line.length() && line[i + 1] == '"') {
                // Escaped quote
                current += '"';
                ++i;
            } else {
                // Toggle quote state
                in_quotes = !in_quotes;
            }
        } else if (c == delimiter && !in_quotes) {
            // End of field - check for NULL
            if (current == "\\N") {
                fields.push_back(std::nullopt);  // SQL NULL
            } else {
                fields.push_back(current);  // Empty string or value
            }
            current.clear();
        } else {
            current += c;
        }
    }
    
    // Handle last field
    if (current == "\\N") {
        fields.push_back(std::nullopt);  // SQL NULL
    } else {
        fields.push_back(current);  // Empty string or value
    }
    
    return fields;
}

std::string EngineIPCSessionHandler::buildInsertStatement(
    const std::string& table_name,
    const std::vector<std::string>& columns,
    const std::vector<std::optional<std::string>>& values) {
    
    if (table_name.empty() || values.empty()) {
        return "";
    }
    
    std::string sql = "INSERT INTO " + table_name;
    
    // Add column list if specified
    if (!columns.empty()) {
        sql += " (";
        for (size_t i = 0; i < columns.size(); ++i) {
            if (i > 0) sql += ", ";
            sql += columns[i];
        }
        sql += ")";
    }
    
    // Add values
    sql += " VALUES (";
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) sql += ", ";
        
        // Check for NULL (std::nullopt)
        if (!values[i].has_value()) {
            sql += "NULL";
        } else if (values[i]->empty()) {
            // Empty string - NOT NULL
            sql += "''";
        } else {
            // Non-empty string - escape and quote
            sql += "'";
            for (char c : *values[i]) {
                if (c == '\'') {
                    sql += "''";  // Escape single quotes
                } else {
                    sql += c;
                }
            }
            sql += "'";
        }
    }
    sql += ")";
    
    return sql;
}

// ============================================================================
// COPY Bytecode Generator (Option C - Direct Bytecode from CSV)
// ============================================================================

std::vector<uint8_t> CopyBytecodeGenerator::generateInsertBytecode(
    const std::string& table_name,
    const std::vector<std::string>& columns,
    const std::vector<std::vector<std::optional<std::string>>>& rows) {
    
    std::vector<uint8_t> bytecode;
    
    // VERSION marker
    writeOpcode(bytecode, sblr::Opcode::VERSION);
    bytecode.push_back(0x01);  // Version 1
    
    // INSERT statement
    writeOpcode(bytecode, sblr::Opcode::INSERT);
    
    // TABLE_REF with table name
    writeOpcode(bytecode, sblr::Opcode::TABLE_REF);
    writeString(bytecode, table_name);
    
    // BEGIN_LIST for columns
    writeOpcode(bytecode, sblr::Opcode::BEGIN_LIST);
    writeUVarint(bytecode, columns.size());
    
    // Column references
    for (const auto& col : columns) {
        writeOpcode(bytecode, sblr::Opcode::COLUMN_REF);
        writeString(bytecode, col);
    }
    
    // END_LIST for columns
    writeOpcode(bytecode, sblr::Opcode::END_LIST);
    
    // BEGIN_LIST for values (rows)
    writeOpcode(bytecode, sblr::Opcode::BEGIN_LIST);
    writeUVarint(bytecode, rows.size());
    
    // Each row
    for (const auto& row : rows) {
        // BEGIN_LIST for row values
        writeOpcode(bytecode, sblr::Opcode::BEGIN_LIST);
        writeUVarint(bytecode, row.size());
        
        // Each value in row
        for (const auto& value : row) {
            writeLiteral(bytecode, value);
        }
        
        // END_LIST for row
        writeOpcode(bytecode, sblr::Opcode::END_LIST);
    }
    
    // END_LIST for values
    writeOpcode(bytecode, sblr::Opcode::END_LIST);
    
    // END of bytecode
    writeOpcode(bytecode, sblr::Opcode::END);
    
    return bytecode;
}

void CopyBytecodeGenerator::writeOpcode(std::vector<uint8_t>& bytecode, sblr::Opcode op) {
    bytecode.push_back(static_cast<uint8_t>(op));
}

void CopyBytecodeGenerator::writeUVarint(std::vector<uint8_t>& bytecode, uint64_t value) {
    // Canonical ULEB128 encoding used by the current SBLR stream writer.
    do {
        uint8_t byte = value & 0x7F;
        value >>= 7;
        if (value != 0) {
            byte |= 0x80;
        }
        bytecode.push_back(byte);
    } while (value != 0);
}

void CopyBytecodeGenerator::writeString(std::vector<uint8_t>& bytecode, const std::string& str) {
    // Write length as UVarint
    writeUVarint(bytecode, str.length());
    // Write string data
    bytecode.insert(bytecode.end(), str.begin(), str.end());
}

void CopyBytecodeGenerator::writeLiteral(std::vector<uint8_t>& bytecode, 
                                         const std::optional<std::string>& value) {
    if (!value.has_value()) {
        // SQL NULL
        writeOpcode(bytecode, sblr::Opcode::LITERAL_NULL);
    } else if (value->empty()) {
        // Empty string
        writeOpcode(bytecode, sblr::Opcode::LITERAL_STRING);
        writeUVarint(bytecode, 0);  // Length 0
    } else {
        // Try to determine type and write appropriate literal
        const std::string& str = *value;
        
        // Try integer first
        char* endptr = nullptr;
        errno = 0;
        long long int_val = std::strtoll(str.c_str(), &endptr, 10);
        if (endptr == str.c_str() + str.length() && errno == 0) {
            // Valid integer
            if (int_val >= INT32_MIN && int_val <= INT32_MAX) {
                writeOpcode(bytecode, sblr::Opcode::LITERAL_INT32);
                writeInt32(bytecode, static_cast<int32_t>(int_val));
            } else {
                writeOpcode(bytecode, sblr::Opcode::LITERAL_INT64);
                writeInt64(bytecode, int_val);
            }
            return;
        }
        
        // Try double/float
        errno = 0;
        double double_val = std::strtod(str.c_str(), &endptr);
        if (endptr == str.c_str() + str.length() && errno == 0) {
            writeOpcode(bytecode, sblr::Opcode::LITERAL_DOUBLE);
            // Write double as 8 bytes (little-endian)
            uint64_t bits;
            static_assert(sizeof(bits) == sizeof(double_val), "Size mismatch");
            std::memcpy(&bits, &double_val, sizeof(bits));
            for (int i = 0; i < 8; ++i) {
                bytecode.push_back((bits >> (i * 8)) & 0xFF);
            }
            return;
        }
        
        // Try boolean
        if (str == "true" || str == "t" || str == "yes" || str == "y" || str == "1") {
            writeOpcode(bytecode, sblr::Opcode::LITERAL_BOOLEAN);
            bytecode.push_back(1);
            return;
        }
        if (str == "false" || str == "f" || str == "no" || str == "n" || str == "0") {
            writeOpcode(bytecode, sblr::Opcode::LITERAL_BOOLEAN);
            bytecode.push_back(0);
            return;
        }
        
        // Default to string
        writeOpcode(bytecode, sblr::Opcode::LITERAL_STRING);
        writeString(bytecode, str);
    }
}

void CopyBytecodeGenerator::writeInt32(std::vector<uint8_t>& bytecode, int32_t value) {
    // Little-endian encoding
    for (int i = 0; i < 4; ++i) {
        bytecode.push_back((value >> (i * 8)) & 0xFF);
    }
}

void CopyBytecodeGenerator::writeInt64(std::vector<uint8_t>& bytecode, int64_t value) {
    // Little-endian encoding
    for (int i = 0; i < 8; ++i) {
        bytecode.push_back((value >> (i * 8)) & 0xFF);
    }
}

// Helper for session handler to use bytecode generation
std::vector<uint8_t> EngineIPCSessionHandler::generateCopyInsertBytecode(
    const std::string& table_name,
    const std::vector<std::string>& columns,
    const std::vector<std::vector<std::optional<std::string>>>& rows) {
    
    return CopyBytecodeGenerator::generateInsertBytecode(table_name, columns, rows);
}

// ============================================================================
// Management Methods
// ============================================================================

core::Status EngineIPCSessionHandler::clearPreparedStatementCache(uint32_t session_id) {
    EngineSessionState* session = getSession(session_id);
    if (!session) {
        return core::Status::NOT_FOUND;
    }
    
    // Clear the statement cache
    session->stmt_cache = std::make_unique<StatementCache>();
    return core::Status::OK;
}

std::vector<EngineIPCSessionHandler::StatementInfo> 
EngineIPCSessionHandler::getPreparedStatements(uint32_t session_id) const {
    std::vector<StatementInfo> result;
    
    // Get session (need non-const for this, but we use mutable access)
    std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
        return result;
    }
    
    // Note: In a full implementation, we would iterate the cache
    // For now, return empty vector
    return result;
}

EngineIPCSessionHandler::HandlerStats EngineIPCSessionHandler::getStats() const {
    HandlerStats stats;
    
    std::shared_lock<std::shared_mutex> lock(sessions_mutex_);
    stats.active_sessions = sessions_.size();
    stats.total_sessions = sessions_.size(); // Would track cumulative in real impl
    
    // Count prepared statements across all sessions
    for (const auto& [id, session] : sessions_) {
        // Would count from cache in real implementation
        (void)id;
        (void)session;
    }
    
    return stats;
}

} // namespace ipc
} // namespace scratchbird
