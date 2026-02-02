/**
 * MySQL Wire Protocol Adapter Implementation
 *
 * ScratchBird Network Layer - Phase 3.2
 *
 * Implements MySQL wire protocol for client compatibility.
 */

#include "scratchbird/protocol/adapters/mysql_adapter.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/telemetry.h"
#include "scratchbird/sblr/mysql_query_compiler.h"
#include "scratchbird/server/ipc_server.h"
#include "scratchbird/client/connection.h"

#include <nlohmann/json.hpp>
#include <cstring>
#include <random>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <functional>

// For SHA1 (native password auth)
#ifdef HAVE_OPENSSL
#include <openssl/sha.h>
#else
#include <functional>
#endif

namespace scratchbird {
namespace protocol {

using json = nlohmann::json;

namespace {

using MySQLCompatMode = scratchbird::parser::mysql::MySQLCompatMode;

MySQLCompatMode parseMysqlCompatValue(const std::string& value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (char ch : value) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            normalized.push_back(ch);
        }
    }

    std::string upper = scratchbird::core::IdentifierUtils::toUpper(normalized);
    if (upper == "MYSQL80" || upper == "MYSQL8" || upper == "8" || upper == "8.0" || upper == "80") {
        return MySQLCompatMode::MYSQL80;
    }
    return MySQLCompatMode::MYSQL57;
}

MySQLCompatMode resolveMysqlCompat(core::Database* db,
                                  const std::string& db_name,
                                  core::ErrorContext* ctx) {
    if (!db) {
        return MySQLCompatMode::MYSQL57;
    }

    auto* catalog = db->catalog_manager();
    if (!catalog) {
        return MySQLCompatMode::MYSQL57;
    }

    core::CatalogManager::EmulationServerInfo server_info;
    if (catalog->getEmulationServerByName("localhost", server_info, ctx) != core::Status::OK) {
        return MySQLCompatMode::MYSQL57;
    }

    core::CatalogManager::EmulatedDatabaseInfo db_info;
    if (catalog->getEmulatedDatabaseByName(server_info.server_id, db_name, db_info, ctx) != core::Status::OK) {
        return MySQLCompatMode::MYSQL57;
    }

    if (db_info.db_metadata.empty()) {
        return MySQLCompatMode::MYSQL57;
    }

    try {
        json meta = json::parse(db_info.db_metadata);
        if (!meta.contains("options") || !meta["options"].is_array()) {
            return MySQLCompatMode::MYSQL57;
        }

        for (const auto& entry : meta["options"]) {
            if (!entry.is_object()) {
                continue;
            }
            auto key_it = entry.find("key");
            auto val_it = entry.find("value");
            if (key_it == entry.end() || val_it == entry.end() ||
                !key_it->is_string() || !val_it->is_string()) {
                continue;
            }

            std::string key = scratchbird::core::IdentifierUtils::toUpper(key_it->get<std::string>());
            if (key == "MYSQL.COMPATIBILITY" || key == "MYSQL_COMPATIBILITY" || key == "MYSQLCOMPATIBILITY") {
                return parseMysqlCompatValue(val_it->get<std::string>());
            }
        }
    } catch (const json::exception&) {
        return MySQLCompatMode::MYSQL57;
    }

    return MySQLCompatMode::MYSQL57;
}

uint16_t clampWarningCount(size_t count) {
    return static_cast<uint16_t>(std::min<size_t>(count, std::numeric_limits<uint16_t>::max()));
}

struct ShowFilter {
    enum class Kind : uint8_t {
        NONE = 0,
        LIKE = 1,
        EQUALS = 2,
    };
    Kind kind = Kind::NONE;
    std::string pattern;
};

static std::string toUpperAscii(std::string input) {
    for (char& c : input) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return input;
}

static void ltrimInPlace(std::string& text) {
    size_t pos = 0;
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
        ++pos;
    }
    if (pos > 0) {
        text.erase(0, pos);
    }
}

static bool isWordBoundary(char c) {
    return !std::isalnum(static_cast<unsigned char>(c)) && c != '_';
}

static size_t findKeywordOutsideQuotes(const std::string& sql_upper,
                                       const std::string& keyword,
                                       size_t start) {
    bool in_quote = false;
    for (size_t i = start; i + keyword.size() <= sql_upper.size(); ++i) {
        char ch = sql_upper[i];
        if (ch == '\'') {
            in_quote = !in_quote;
        }
        if (in_quote) {
            continue;
        }
        if (sql_upper.compare(i, keyword.size(), keyword) == 0) {
            char before = (i == 0) ? ' ' : sql_upper[i - 1];
            char after = (i + keyword.size() < sql_upper.size())
                ? sql_upper[i + keyword.size()]
                : ' ';
            if (isWordBoundary(before) && isWordBoundary(after)) {
                return i;
            }
        }
    }
    return std::string::npos;
}

static bool parseStringLiteralAt(const std::string& sql, size_t& pos, std::string& out) {
    while (pos < sql.size() && std::isspace(static_cast<unsigned char>(sql[pos]))) {
        ++pos;
    }
    if (pos >= sql.size() || sql[pos] != '\'') {
        return false;
    }
    ++pos;
    std::string value;
    while (pos < sql.size()) {
        char ch = sql[pos++];
        if (ch == '\'') {
            if (pos < sql.size() && sql[pos] == '\'') {
                value.push_back('\'');
                ++pos;
                continue;
            }
            out = value;
            return true;
        }
        value.push_back(ch);
    }
    return false;
}

static bool matchSqlLike(const std::string& str,
                         const std::string& pattern,
                         char escape = '\\') {
    size_t s = 0, p = 0;
    size_t star_p = std::string::npos, star_s = 0;

    while (s < str.size()) {
        if (p < pattern.size()) {
            if (escape != '\0' && pattern[p] == escape && p + 1 < pattern.size()) {
                ++p;
                if (str[s] == pattern[p]) {
                    ++s;
                    ++p;
                    continue;
                }
                if (star_p != std::string::npos) {
                    p = star_p + 1;
                    s = ++star_s;
                    continue;
                }
                return false;
            }

            if (pattern[p] == '%') {
                star_p = p++;
                star_s = s;
                continue;
            }

            if (pattern[p] == '_' || pattern[p] == str[s]) {
                ++s;
                ++p;
                continue;
            }
        }

        if (star_p != std::string::npos) {
            p = star_p + 1;
            s = ++star_s;
            continue;
        }

        return false;
    }

    while (p < pattern.size() && pattern[p] == '%') {
        ++p;
    }

    return p == pattern.size();
}

static bool matchSqlLikeCase(const std::string& str,
                             const std::string& pattern,
                             char escape,
                             bool case_insensitive) {
    if (!case_insensitive) {
        return matchSqlLike(str, pattern, escape);
    }
    std::string lower_str = str;
    std::string lower_pattern = pattern;
    for (char& c : lower_str) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    for (char& c : lower_pattern) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return matchSqlLike(lower_str, lower_pattern, escape);
}

static bool parseShowFilter(const std::string& sql,
                            const std::string& sql_upper,
                            size_t start_pos,
                            ShowFilter& filter_out,
                            std::string& error_out) {
    size_t like_pos = findKeywordOutsideQuotes(sql_upper, "LIKE", start_pos);
    size_t where_pos = findKeywordOutsideQuotes(sql_upper, "WHERE", start_pos);
    if (like_pos == std::string::npos && where_pos == std::string::npos) {
        return true;
    }

    if (where_pos != std::string::npos &&
        (like_pos == std::string::npos || where_pos < like_pos)) {
        size_t pos = where_pos + 5;
        std::string identifier;
        while (pos < sql.size() && std::isspace(static_cast<unsigned char>(sql[pos]))) {
            ++pos;
        }
        while (pos < sql.size()) {
            char ch = sql[pos];
            if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '$') {
                identifier.push_back(ch);
                ++pos;
            } else {
                break;
            }
        }
        std::string identifier_upper = toUpperAscii(identifier);
        if (identifier_upper != "VARIABLE_NAME" &&
            identifier_upper != "VARIABLE" && identifier_upper != "NAME") {
            error_out = "SHOW ... WHERE supports Variable_name predicates only";
            return false;
        }

        while (pos < sql.size() && std::isspace(static_cast<unsigned char>(sql[pos]))) {
            ++pos;
        }

        ShowFilter filter;
        if (sql_upper.compare(pos, 4, "LIKE") == 0) {
            pos += 4;
            filter.kind = ShowFilter::Kind::LIKE;
        } else if (pos < sql.size() && sql[pos] == '=') {
            ++pos;
            filter.kind = ShowFilter::Kind::EQUALS;
        } else {
            error_out = "SHOW ... WHERE supports LIKE or = predicates only";
            return false;
        }

        std::string literal;
        if (!parseStringLiteralAt(sql, pos, literal)) {
            error_out = "SHOW ... WHERE expects a quoted string literal";
            return false;
        }
        filter.pattern = literal;
        filter_out = std::move(filter);
        return true;
    }

    if (like_pos != std::string::npos) {
        size_t pos = like_pos + 4;
        std::string literal;
        if (!parseStringLiteralAt(sql, pos, literal)) {
            error_out = "SHOW ... LIKE expects a quoted string literal";
            return false;
        }
        filter_out.kind = ShowFilter::Kind::LIKE;
        filter_out.pattern = literal;
    }
    return true;
}

} // namespace

// ============================================================================
// Constructor/Destructor
// ============================================================================

MySqlAdapter::MySqlAdapter(const ProtocolAdapterConfig& config)
    : ProtocolAdapter(config) {
    start_time_ = std::chrono::steady_clock::now();

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

core::Status MySqlAdapter::ensureRemoteClient(core::ErrorContext* ctx) {
    if (client_) {
        return core::Status::OK;
    }

    client_config_.database_name = database_name_.empty() ? "default" : database_name_;
    if (!config_.engine_endpoint.empty()) {
        client_config_.ipc_method = server::IPCMethod::AUTO;
        client_config_.socket_path = config_.engine_endpoint;
    } else {
        client_config_.ipc_method = server::IPCMethod::UNIX_SOCKET;
        client_config_.socket_path = server::getIPCPath(client_config_.database_name,
                                                        client_config_.ipc_method);
    }
    client_config_.connect_timeout_ms = config_.read_timeout_ms;
    client_config_.read_timeout_ms = config_.read_timeout_ms;
    client_config_.write_timeout_ms = config_.write_timeout_ms;
    client_config_.auto_commit = true;
    client_config_.auto_start_server = false;
    client_config_.username = username_.empty() ? "BOOTSTRAP" : username_;

    client_ = std::make_unique<client::Connection>();
    auto status = client_->connect(client_config_, ctx);
    if (status != core::Status::OK) {
        client_.reset();
        return status;
    }

    // Switch to emulated MySQL schema for this database if possible
    if (!default_db_set_) {
        std::string db_name = database_name_.empty() ? std::string("default") : database_name_;
        std::string schema_name = "remote.emulation.mysql.localhost.databases." + db_name;
        std::string use_stmt = "SET search_path TO '" + escapeLiteral(schema_name) + "'";
        client::ResultSet rs;
        auto set_status = client_->executeQuery(use_stmt, &rs, ctx);
        if (set_status == core::Status::OK) {
            default_db_set_ = true;
            bootstrapInformationSchema(ctx);
        }
    }

    return core::Status::OK;
}

core::Status MySqlAdapter::executeRemoteQuery(const QueryContext& query,
                                              ResultContext& result,
                                              core::ErrorContext* ctx) {
    auto status = ensureRemoteClient(ctx);
    if (status != core::Status::OK) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(status);
        result.error_message = ctx ? ctx->message : "Failed to connect to engine";
        return status;
    }

    client::ResultSet rs;
    std::vector<uint8_t> bytecode;
    std::string compile_error;
    auto compile_status = compileQuery(query.query, bytecode, compile_error);
    if (compile_status != core::Status::OK) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(compile_status);
        result.error_message = compile_error.empty() ? "Compilation failed" : compile_error;
        return compile_status;
    }

    status = client_->executeBytecode(bytecode, query.query, &rs, ctx);
    if (status != core::Status::OK) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(status);
        std::string err = client_->getLastError();
        if (err.empty() && ctx) {
            err = ctx->message;
        }
        result.error_message = err.empty() ? "Query execution failed" : err;
        return status;
    }

    result.columns.clear();
    for (const auto& col : rs.getColumns()) {
        ProtocolCodec::ColumnInfo info;
        info.name = col.name;
        info.type = col.type;
        info.type_modifier = col.type_modifier;
        result.columns.push_back(info);
    }

    result.rows.clear();
    const auto row_count = static_cast<size_t>(rs.getRowCount());
    for (size_t i = 0; i < row_count; ++i) {
        result.rows.push_back(rs.getRowValues(i));
    }

    result.rows_affected = rs.getRowsAffected();
    result.command_tag = rs.getCommandTag();
    if (result.command_tag.empty() || result.command_tag == "OK") {
        auto ltrim_upper = [](const std::string& input) {
            size_t pos = 0;
            while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
                ++pos;
            }
            std::string upper;
            upper.reserve(input.size() - pos);
            for (; pos < input.size(); ++pos) {
                upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(input[pos]))));
            }
            return upper;
        };
        std::string normalized = ltrim_upper(query.query);
        auto starts_with = [&](const std::string& prefix) {
            return normalized.rfind(prefix, 0) == 0;
        };

        if (row_count > 0) {
            result.command_tag = "SELECT " + std::to_string(row_count);
        } else if (starts_with("INSERT")) {
            result.command_tag = "INSERT";
        } else if (starts_with("UPDATE")) {
            result.command_tag = "UPDATE";
        } else if (starts_with("DELETE")) {
            result.command_tag = "DELETE";
        } else if (starts_with("CREATE")) {
            result.command_tag = "CREATE";
        } else if (starts_with("DROP")) {
            result.command_tag = "DROP";
        } else if (starts_with("ALTER")) {
            result.command_tag = "ALTER";
        } else {
            result.command_tag = "OK";
        }
    }

    return core::Status::OK;
}

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

        // Rows
        for (const auto& row : result.rows) {
            sendResultRow(conn, row);
        }

        // Final EOF/OK
        if (client_capabilities_ & mysql::Capability::DEPRECATE_EOF) {
            sendOkPacket(conn, 0, 0, result.command_tag);
        } else {
            sendEofPacket(conn);
        }
    }

    return core::Status::OK;
}

core::Status MySqlAdapter::compileQuery(const std::string& sql,
                                        std::vector<uint8_t>& bytecode_out,
                                        std::string& error_out) {
    core::ErrorContext ctx;
    auto status = ensureEngine(&ctx);
    if (status != core::Status::OK) {
        error_out = ctx.message;
        return status;
    }

    last_warnings_.clear();

    sblr::MySQLQueryCompiler compiler(engineDatabase());
    std::string db_name = database_name_.empty() ? std::string("default") : database_name_;
    compiler.setDefaultSchema("remote.emulation.mysql.localhost.databases." + db_name);
    compiler.setCompatibilityMode(resolveMysqlCompat(engineDatabase(), db_name, &ctx));
    auto result = compiler.compile(sql);
    last_warnings_ = result.warnings();
    if (!result.success()) {
        error_out = result.errors().empty() ? "Compilation failed" : result.errors().front();
        return core::Status::INVALID_ARGUMENT;
    }
    bytecode_out = result.bytecode();
    return core::Status::OK;
}

core::Status MySqlAdapter::sendProtocolError(network::Connection* conn,
                                              uint32_t error_code,
                                              const std::string& sqlstate,
                                              const std::string& message,
                                              const std::string& /*detail*/,
                                              const std::string& /*hint*/) {
    uint16_t mapped_code = static_cast<uint16_t>(error_code);
    std::string mapped_state = sqlstate;
    mapStatusToMySqlError(error_code, mapped_code, mapped_state);
    if (mapped_state.empty()) {
        mapped_state = "HY000";
    }
    last_errors_.clear();
    last_errors_.push_back(message);
    last_error_code_ = mapped_code;
    last_error_sqlstate_ = mapped_state;
    sendErrorPacket(conn, mapped_code, mapped_state, message);
    return core::Status::OK;
}

void MySqlAdapter::updateTransactionStatus(const std::string& sql, bool has_error) {
    auto ltrim_upper = [](const std::string& input) {
        size_t pos = 0;
        while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
            ++pos;
        }
        std::string upper;
        upper.reserve(input.size() - pos);
        for (; pos < input.size(); ++pos) {
            upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(input[pos]))));
        }
        return upper;
    };

    std::string normalized = ltrim_upper(sql);
    auto starts_with = [&](const std::string& prefix) {
        return normalized.rfind(prefix, 0) == 0;
    };

    if (starts_with("SET AUTOCOMMIT")) {
        if (normalized.find("=0") != std::string::npos) {
            server_status_ &= ~mysql::ServerStatus::AUTOCOMMIT;
            server_status_ |= mysql::ServerStatus::IN_TRANS;
            in_transaction_ = true;
        } else if (normalized.find("=1") != std::string::npos) {
            server_status_ |= mysql::ServerStatus::AUTOCOMMIT;
            server_status_ &= ~mysql::ServerStatus::IN_TRANS;
            in_transaction_ = false;
        }
        return;
    }

    if (starts_with("BEGIN") || starts_with("START TRANSACTION")) {
        server_status_ |= mysql::ServerStatus::IN_TRANS;
        in_transaction_ = true;
        return;
    }

    if (starts_with("COMMIT") || starts_with("ROLLBACK")) {
        server_status_ &= ~mysql::ServerStatus::IN_TRANS;
        in_transaction_ = false;
        return;
    }

    if (has_error) {
        server_status_ |= mysql::ServerStatus::IN_TRANS;
        in_transaction_ = true;
        return;
    }

    if (server_status_ & mysql::ServerStatus::AUTOCOMMIT) {
        server_status_ &= ~mysql::ServerStatus::IN_TRANS;
        in_transaction_ = false;
    } else {
        server_status_ |= mysql::ServerStatus::IN_TRANS;
        in_transaction_ = true;
    }
}

uint16_t MySqlAdapter::countParameters(const std::string& query) const {
    bool in_single = false;
    bool in_double = false;
    bool escape = false;
    uint16_t count = 0;

    for (char c : query) {
        if (escape) {
            escape = false;
            continue;
        }
        if (c == '\\') {
            escape = true;
            continue;
        }
        if (c == '\'' && !in_double) {
            in_single = !in_single;
            continue;
        }
        if (c == '"' && !in_single) {
            in_double = !in_double;
            continue;
        }
        if (c == '?' && !in_single && !in_double) {
            ++count;
        }
    }
    return count;
}

std::string MySqlAdapter::escapeLiteral(const std::string& value) const {
    std::string escaped;
    escaped.reserve(value.size() + 4);
    for (char c : value) {
        if (c == '\'' || c == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(c);
    }
    return escaped;
}

bool MySqlAdapter::decodePsParameter(uint8_t type, bool is_unsigned,
                                     const uint8_t* data, size_t& offset,
                                     size_t max_len, std::string& out_literal) {
    if (offset >= max_len) {
        return false;
    }

    switch (type) {
        case mysql::FieldType::TINY: {
            if (offset + 1 > max_len) return false;
            uint8_t raw = data[offset++];
            if (is_unsigned) {
                out_literal = std::to_string(raw);
            } else {
                out_literal = std::to_string(static_cast<int8_t>(raw));
            }
            return true;
        }
        case mysql::FieldType::SHORT: {
            if (offset + 2 > max_len) return false;
            uint16_t raw = readInt2(data + offset);
            offset += 2;
            if (is_unsigned) {
                out_literal = std::to_string(raw);
            } else {
                out_literal = std::to_string(static_cast<int16_t>(raw));
            }
            return true;
        }
        case mysql::FieldType::LONG: {
            if (offset + 4 > max_len) return false;
            uint32_t raw = readInt4(data + offset);
            offset += 4;
            if (is_unsigned) {
                out_literal = std::to_string(raw);
            } else {
                out_literal = std::to_string(static_cast<int32_t>(raw));
            }
            return true;
        }
        case mysql::FieldType::LONGLONG: {
            if (offset + 8 > max_len) return false;
            uint64_t raw = readInt8(data + offset);
            offset += 8;
            if (is_unsigned) {
                out_literal = std::to_string(raw);
            } else {
                out_literal = std::to_string(static_cast<int64_t>(raw));
            }
            return true;
        }
        case mysql::FieldType::FLOAT: {
            if (offset + sizeof(float) > max_len) return false;
            float val = 0.0f;
            std::memcpy(&val, data + offset, sizeof(float));
            offset += sizeof(float);
            out_literal = std::to_string(val);
            return true;
        }
        case mysql::FieldType::DOUBLE: {
            if (offset + sizeof(double) > max_len) return false;
            double val = 0.0;
            std::memcpy(&val, data + offset, sizeof(double));
            offset += sizeof(double);
            out_literal = std::to_string(val);
            return true;
        }
        case mysql::FieldType::DECIMAL:
        case mysql::FieldType::NEWDECIMAL:
        case mysql::FieldType::VARCHAR:
        case mysql::FieldType::VAR_STRING:
        case mysql::FieldType::STRING:
        case mysql::FieldType::BLOB:
        case mysql::FieldType::TINY_BLOB:
        case mysql::FieldType::MEDIUM_BLOB:
        case mysql::FieldType::LONG_BLOB:
        case mysql::FieldType::JSON: {
            std::string val = readLenEncString(data, offset, max_len);
            out_literal = "'" + escapeLiteral(val) + "'";
            return true;
        }
        case mysql::FieldType::DATE:
        case mysql::FieldType::TIME:
        case mysql::FieldType::TIME2:
        case mysql::FieldType::TIMESTAMP:
        case mysql::FieldType::TIMESTAMP2:
        case mysql::FieldType::DATETIME:
        case mysql::FieldType::DATETIME2: {
            uint64_t len = readLenEncInt(data, offset, max_len);
            if (offset + len > max_len) return false;
            std::string temporal(reinterpret_cast<const char*>(data + offset),
                                 static_cast<size_t>(len));
            offset += static_cast<size_t>(len);
            if (temporal.empty()) {
                out_literal = "NULL";
            } else {
                out_literal = "'" + escapeLiteral(temporal) + "'";
            }
            return true;
        }
        default: {
            std::string fallback = readLenEncString(data, offset, max_len);
            out_literal = "'" + escapeLiteral(fallback) + "'";
            return true;
        }
    }
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

    ResultContext show_result;
    if (handleShowQuery(query, show_result)) {
        sendQueryResult(conn, show_result);
        return sendBuffer(conn);
    }

    last_warnings_.clear();
    last_errors_.clear();
    last_error_code_ = 0;
    last_error_sqlstate_.clear();

    // Execute query
    QueryContext ctx;
    ctx.query = query;

    ResultContext result;
    executeRemoteQuery(ctx, result);

    updateTransactionStatus(query, result.has_error);

    sendQueryResult(conn, result);
    return sendBuffer(conn);
}

bool MySqlAdapter::handleShowQuery(const std::string& query, ResultContext& result) {
    std::string trimmed = query;
    ltrimInPlace(trimmed);
    std::string upper = toUpperAscii(trimmed);
    if (upper.rfind("SHOW", 0) != 0) {
        return false;
    }

    auto read_keyword = [&](size_t& pos) -> std::string {
        while (pos < upper.size() && std::isspace(static_cast<unsigned char>(upper[pos]))) {
            ++pos;
        }
        size_t start = pos;
        while (pos < upper.size()) {
            char ch = upper[pos];
            if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_') {
                ++pos;
                continue;
            }
            break;
        }
        if (start == pos) {
            return "";
        }
        return upper.substr(start, pos - start);
    };

    size_t pos = 4;
    std::string keyword = read_keyword(pos);
    if (keyword.empty()) {
        return false;
    }

    if (keyword == "GLOBAL" || keyword == "SESSION") {
        keyword = read_keyword(pos);
        if (keyword.empty()) {
            return false;
        }
    }

    if (keyword == "FULL") {
        keyword = read_keyword(pos);
    }

    auto build_string_result = [&](const std::vector<std::string>& columns) {
        result.columns.clear();
        for (const auto& name : columns) {
            ProtocolCodec::ColumnInfo col;
            col.name = name;
            col.type = protocol::WireType::VARCHAR;
            col.type_modifier = 0;
            result.columns.push_back(col);
        }
    };

    auto append_row = [&](std::vector<protocol::ProtocolCodec::ColumnValue> row) {
        result.rows.push_back(std::move(row));
    };

    if (keyword == "STATUS") {
        ShowFilter filter;
        std::string error;
        if (!parseShowFilter(trimmed, upper, pos, filter, error)) {
            result.has_error = true;
            result.error_message = error;
            return true;
        }

        build_string_result({"Variable_name", "Value"});

        auto emit_status = [&](const std::string& name, const std::string& value) {
            bool matches = true;
            if (filter.kind == ShowFilter::Kind::LIKE) {
                matches = matchSqlLikeCase(name, filter.pattern, '\\', true);
            } else if (filter.kind == ShowFilter::Kind::EQUALS) {
                matches = toUpperAscii(name) == toUpperAscii(filter.pattern);
            }
            if (!matches) {
                return;
            }
            append_row({protocol::ProtocolCodec::ColumnValue::fromString(name),
                        protocol::ProtocolCodec::ColumnValue::fromString(value)});
        };

        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time_).count();
        emit_status("Uptime", std::to_string(uptime));
        emit_status("Uptime_since_flush_status", std::to_string(uptime));
        emit_status("Threads_connected", "1");

        auto& metrics = core::MetricsRegistry::getInstance();
        std::string payload = metrics.exportPrometheus();
        std::istringstream stream(payload);
        std::string line;
        while (std::getline(stream, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }
            std::istringstream row(line);
            std::string name;
            std::string value;
            row >> name >> value;
            if (name.empty() || value.empty()) {
                continue;
            }
            emit_status(name, value);
        }

        return true;
    }

    if (keyword == "VARIABLES") {
        ShowFilter filter;
        std::string error;
        if (!parseShowFilter(trimmed, upper, pos, filter, error)) {
            result.has_error = true;
            result.error_message = error;
            return true;
        }

        build_string_result({"Variable_name", "Value"});

        auto emit_variable = [&](const std::string& name, const std::string& value) {
            bool matches = true;
            if (filter.kind == ShowFilter::Kind::LIKE) {
                matches = matchSqlLikeCase(name, filter.pattern, '\\', true);
            } else if (filter.kind == ShowFilter::Kind::EQUALS) {
                matches = toUpperAscii(name) == toUpperAscii(filter.pattern);
            }
            if (!matches) {
                return;
            }
            append_row({protocol::ProtocolCodec::ColumnValue::fromString(name),
                        protocol::ProtocolCodec::ColumnValue::fromString(value)});
        };

        const bool autocommit =
            (server_status_ & mysql::ServerStatus::AUTOCOMMIT) != 0;

        emit_variable("autocommit", autocommit ? "ON" : "OFF");
        emit_variable("character_set_client", "utf8mb4");
        emit_variable("character_set_connection", "utf8mb4");
        emit_variable("character_set_results", "utf8mb4");
        emit_variable("collation_connection", "utf8mb4_general_ci");
        emit_variable("sql_mode", "");
        emit_variable("time_zone", "SYSTEM");
        emit_variable("transaction_isolation", "READ-COMMITTED");
        emit_variable("version", server_version_);

        return true;
    }

    if (keyword == "PROCESSLIST") {
        core::ErrorContext ctx;
        auto status = ensureRemoteClient(&ctx);
        if (status != core::Status::OK) {
            result.has_error = true;
            result.error_message = ctx.message.empty()
                ? "Failed to connect to engine"
                : ctx.message;
            return true;
        }

        client::ResultSet rs;
        status = client_->executeQuery(
            "SELECT ID, USER, HOST, DB, COMMAND, TIME, STATE, INFO "
            "FROM information_schema.PROCESSLIST",
            &rs, &ctx);
        if (status != core::Status::OK) {
            result.has_error = true;
            result.error_message = ctx.message.empty()
                ? "Failed to load process list"
                : ctx.message;
            return true;
        }

        result.columns.clear();
        for (const auto& col : rs.getColumns()) {
            ProtocolCodec::ColumnInfo info;
            info.name = col.name;
            info.type = col.type;
            info.type_modifier = col.type_modifier;
            result.columns.push_back(info);
        }

        result.rows.clear();
        const auto row_count = static_cast<size_t>(rs.getRowCount());
        for (size_t i = 0; i < row_count; ++i) {
            result.rows.push_back(rs.getRowValues(i));
        }

        return true;
    }

    if (keyword == "WARNINGS" || keyword == "ERRORS") {
        size_t limit_pos = findKeywordOutsideQuotes(upper, "LIMIT", pos);
        int64_t offset = 0;
        int64_t count = -1;
        if (limit_pos != std::string::npos) {
            size_t parse_pos = limit_pos + 5;
            auto parse_int = [&](int64_t& out) -> bool {
                while (parse_pos < trimmed.size() &&
                       std::isspace(static_cast<unsigned char>(trimmed[parse_pos]))) {
                    ++parse_pos;
                }
                if (parse_pos >= trimmed.size()) {
                    return false;
                }
                char* end_ptr = nullptr;
                long long value = std::strtoll(trimmed.c_str() + parse_pos, &end_ptr, 10);
                if (end_ptr == trimmed.c_str() + parse_pos) {
                    return false;
                }
                out = static_cast<int64_t>(value);
                parse_pos = static_cast<size_t>(end_ptr - trimmed.c_str());
                return true;
            };

            int64_t first = 0;
            if (!parse_int(first)) {
                result.has_error = true;
                result.error_message = "SHOW ... LIMIT expects numeric values";
                return true;
            }

            size_t saved_pos = parse_pos;
            while (parse_pos < trimmed.size() &&
                   std::isspace(static_cast<unsigned char>(trimmed[parse_pos]))) {
                ++parse_pos;
            }

            if (parse_pos < trimmed.size() && trimmed[parse_pos] == ',') {
                ++parse_pos;
                int64_t second = 0;
                if (!parse_int(second)) {
                    result.has_error = true;
                    result.error_message = "SHOW ... LIMIT expects numeric values";
                    return true;
                }
                offset = first;
                count = second;
            } else if (upper.compare(parse_pos, 6, "OFFSET") == 0) {
                parse_pos += 6;
                int64_t off = 0;
                if (!parse_int(off)) {
                    result.has_error = true;
                    result.error_message = "SHOW ... LIMIT OFFSET expects numeric values";
                    return true;
                }
                offset = off;
                count = first;
            } else {
                parse_pos = saved_pos;
                count = first;
            }
        }

        result.columns.clear();
        ProtocolCodec::ColumnInfo level;
        level.name = "Level";
        level.type = protocol::WireType::VARCHAR;
        level.type_modifier = 0;
        ProtocolCodec::ColumnInfo code;
        code.name = "Code";
        code.type = protocol::WireType::INT64;
        code.type_modifier = 0;
        ProtocolCodec::ColumnInfo message;
        message.name = "Message";
        message.type = protocol::WireType::VARCHAR;
        message.type_modifier = 0;
        result.columns = {level, code, message};

        const auto& source = (keyword == "WARNINGS") ? last_warnings_ : last_errors_;
        int64_t start = std::max<int64_t>(0, offset);
        int64_t end = (count < 0) ? static_cast<int64_t>(source.size())
                                  : std::min<int64_t>(static_cast<int64_t>(source.size()),
                                                      start + count);
        for (int64_t i = start; i < end; ++i) {
            const auto& msg = source[static_cast<size_t>(i)];
            int64_t code_value = (keyword == "ERRORS")
                ? static_cast<int64_t>(last_error_code_)
                : 0;
            append_row({protocol::ProtocolCodec::ColumnValue::fromString(
                            keyword == "WARNINGS" ? "Warning" : "Error"),
                        protocol::ProtocolCodec::ColumnValue::fromInt64(code_value),
                        protocol::ProtocolCodec::ColumnValue::fromString(msg)});
        }

        return true;
    }

    return false;
}

core::Status MySqlAdapter::handleComInitDb(network::Connection* conn) {
    if (current_packet_.size() < 2) {
        sendErrorPacket(conn, mysql::ErrorCode::BAD_DB_ERROR, "42000",
                       "No database specified");
        return sendBuffer(conn);
    }

    database_name_.assign(reinterpret_cast<const char*>(current_packet_.data() + 1),
                          current_packet_.size() - 1);
    default_db_set_ = false;
    if (client_) {
        client_->disconnect();
        client_.reset();
    }

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
    stmt.num_params = countParameters(query);
    stmt.num_columns = 0;
    stmt.param_types.assign(stmt.num_params, mysql::FieldType::VAR_STRING);
    stmt.param_unsigned.assign(stmt.num_params, 0);

    // Try to collect result-set metadata for SELECT/SHOW statements
    auto ltrim_upper = [](const std::string& input) {
        size_t pos = 0;
        while (pos < input.size() && std::isspace(static_cast<unsigned char>(input[pos]))) {
            ++pos;
        }
        std::string upper;
        upper.reserve(input.size() - pos);
        for (; pos < input.size(); ++pos) {
            upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(input[pos]))));
        }
        return upper;
    };
    std::string normalized = ltrim_upper(query);
    auto starts_with = [&](const std::string& prefix) {
        return normalized.rfind(prefix, 0) == 0;
    };
    if (starts_with("SELECT") || starts_with("WITH")) {
        core::ErrorContext ctx;
        if (ensureRemoteClient(&ctx) == core::Status::OK) {
            client::ResultSet rs;
            std::string describe_sql = query + " LIMIT 0";
            if (client_->executeQuery(describe_sql, &rs, &ctx) == core::Status::OK) {
                const auto& cols = rs.getColumns();
                for (size_t i = 0; i < cols.size(); ++i) {
                    ProtocolCodec::ColumnInfo ci;
                    ci.name = cols[i].name;
                    ci.type = cols[i].type;
                    ci.type_modifier = cols[i].type_modifier;
                    stmt.columns.push_back(ci);
                }
                stmt.num_columns = static_cast<uint16_t>(stmt.columns.size());
            }
        }
    }

    prepared_statements_[stmt.id] = stmt;

    sendPrepareOk(conn, stmt.id, stmt.num_columns, stmt.num_params);
    if (stmt.num_params > 0) {
        for (uint16_t i = 0; i < stmt.num_params; ++i) {
            ProtocolCodec::ColumnInfo param_col;
            param_col.name = "param" + std::to_string(i + 1);
            param_col.type = WireType::VARCHAR;
            sendColumnDefinition(conn, param_col, "", "", "", param_col.name);
        }
        if (!(client_capabilities_ & mysql::Capability::DEPRECATE_EOF)) {
            sendEofPacket(conn);
        }
    }
    if (stmt.num_columns > 0) {
        for (const auto& col : stmt.columns) {
            sendColumnDefinition(conn, col, database_name_);
        }
        if (!(client_capabilities_ & mysql::Capability::DEPRECATE_EOF)) {
            sendEofPacket(conn);
        }
    }
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

    last_warnings_.clear();

    // Execute the prepared statement
    QueryContext ctx;
    auto& stmt = it->second;

    size_t offset = 5;  // command byte + statement id already consumed
    if (offset >= current_packet_.size()) {
        sendErrorPacket(conn, mysql::ErrorCode::UNKNOWN_ERROR, "HY000",
                       "Malformed execute packet");
        return sendBuffer(conn);
    }

    uint8_t flags = readInt1(current_packet_.data() + offset);
    (void)flags;
    offset += 1;  // flags

    // Iteration count (always 1 for our purposes)
    if (offset + 4 > current_packet_.size()) {
        sendErrorPacket(conn, mysql::ErrorCode::UNKNOWN_ERROR, "HY000",
                        "Truncated iteration count");
        return sendBuffer(conn);
    }
    offset += 4;

    size_t null_bitmap_len = (stmt.num_params + 7) / 8;
    std::vector<bool> is_null(stmt.num_params, false);
    if (offset + null_bitmap_len > current_packet_.size()) {
        sendErrorPacket(conn, mysql::ErrorCode::UNKNOWN_ERROR, "HY000",
                       "Invalid NULL-bitmap in execute");
        return sendBuffer(conn);
    }
    for (uint16_t i = 0; i < stmt.num_params; ++i) {
        size_t byte_idx = i / 8;
        size_t bit_idx = i % 8;
        if (current_packet_[offset + byte_idx] & (1U << bit_idx)) {
            is_null[i] = true;
        }
    }
    offset += null_bitmap_len;

    if (offset >= current_packet_.size()) {
        sendErrorPacket(conn, mysql::ErrorCode::UNKNOWN_ERROR, "HY000",
                        "Missing parameter metadata");
        return sendBuffer(conn);
    }

    uint8_t new_params_bound_flag = readInt1(current_packet_.data() + offset);
    offset += 1;

    if (new_params_bound_flag) {
        if (offset + stmt.num_params * 2 > current_packet_.size()) {
            sendErrorPacket(conn, mysql::ErrorCode::UNKNOWN_ERROR, "HY000",
                            "Parameter types truncated");
            return sendBuffer(conn);
        }
        stmt.param_types.resize(stmt.num_params);
        stmt.param_unsigned.resize(stmt.num_params);
        for (uint16_t i = 0; i < stmt.num_params; ++i) {
            stmt.param_types[i] = readInt1(current_packet_.data() + offset);
            uint8_t attr = readInt1(current_packet_.data() + offset + 1);
            stmt.param_unsigned[i] = attr;
            offset += 2;
        }
    } else {
        // Ensure we have metadata
        if (stmt.param_types.size() < stmt.num_params) {
            stmt.param_types.assign(stmt.num_params, mysql::FieldType::VAR_STRING);
            stmt.param_unsigned.assign(stmt.num_params, 0);
        }
    }

    std::vector<std::string> param_literals;
    param_literals.reserve(stmt.num_params);
    for (uint16_t i = 0; i < stmt.num_params; ++i) {
        if (is_null[i]) {
            param_literals.emplace_back("NULL");
            continue;
        }
        const uint8_t* buf = current_packet_.data();
        std::string literal;
        if (!decodePsParameter(stmt.param_types[i],
                               (stmt.param_unsigned.size() > i) && (stmt.param_unsigned[i] & 0x80),
                               buf, offset, current_packet_.size(), literal)) {
            sendErrorPacket(conn, mysql::ErrorCode::UNKNOWN_ERROR, "HY000",
                            "Failed to decode parameter " + std::to_string(i));
            return sendBuffer(conn);
        }
        param_literals.push_back(literal);
    }

    // Reconstruct the SQL by substituting '?' in order
    std::string rewritten;
    rewritten.reserve(stmt.query.size() + param_literals.size() * 8);
    size_t param_idx = 0;
    for (char c : stmt.query) {
        if (c == '?' && param_idx < param_literals.size()) {
            rewritten.append(param_literals[param_idx++]);
        } else {
            rewritten.push_back(c);
        }
    }
    if (param_idx != param_literals.size()) {
        sendErrorPacket(conn, mysql::ErrorCode::UNKNOWN_ERROR, "HY000",
                        "Parameter count mismatch");
        return sendBuffer(conn);
    }
    ctx.query = rewritten;

    ResultContext result;
    executeRemoteQuery(ctx, result);

    if (!result.columns.empty()) {
        stmt.columns = result.columns;
        stmt.num_columns = static_cast<uint16_t>(result.columns.size());
    }

    updateTransactionStatus(rewritten, result.has_error);

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
    last_warnings_.clear();
    last_errors_.clear();
    last_error_code_ = 0;
    last_error_sqlstate_.clear();

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
        writeInt2(payload, clampWarningCount(last_warnings_.size()));  // Warnings
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
        writeInt2(payload, clampWarningCount(last_warnings_.size()));  // Warnings
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
    writeInt2(payload, mysqlCharsetForType(col.type));

    // Column length
    uint32_t column_length = 255;
    if (col.type_modifier > 0) {
        column_length = static_cast<uint32_t>(col.type_modifier);
    } else {
        switch (col.type) {
            case WireType::INT16: column_length = 6; break;
            case WireType::INT32: column_length = 11; break;
            case WireType::INT64: column_length = 20; break;
            case WireType::FLOAT32: column_length = 12; break;
            case WireType::FLOAT64: column_length = 22; break;
            case WireType::BYTEA: column_length = 65535; break;
            default: break;
        }
    }
    writeInt4(payload, column_length);

    // Column type
    uint8_t mysql_type = wireTypeToMySqlType(col.type);
    writeInt1(payload, mysql_type);

    // Flags
    uint16_t flags = 0;
    switch (mysql_type) {
        case mysql::FieldType::TINY:
        case mysql::FieldType::SHORT:
        case mysql::FieldType::LONG:
        case mysql::FieldType::LONGLONG:
        case mysql::FieldType::FLOAT:
        case mysql::FieldType::DOUBLE:
        case mysql::FieldType::DECIMAL:
        case mysql::FieldType::NEWDECIMAL:
            flags |= mysql::FieldFlag::NUM;
            break;
        default:
            break;
    }
    if (mysql_type == mysql::FieldType::BLOB ||
        mysql_type == mysql::FieldType::TINY_BLOB ||
        mysql_type == mysql::FieldType::MEDIUM_BLOB ||
        mysql_type == mysql::FieldType::LONG_BLOB) {
        flags |= mysql::FieldFlag::BLOB;
    }
    if (mysql_type == mysql::FieldType::TIMESTAMP) {
        flags |= mysql::FieldFlag::TIMESTAMP;
    }
    writeInt2(payload, flags);

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

uint16_t MySqlAdapter::mysqlCharsetForType(WireType type) const {
    switch (type) {
        case WireType::BYTEA:
            return mysql::Charset::BINARY;
        default:
            return mysql::Charset::UTF8MB4_GENERAL_CI;
    }
}

void MySqlAdapter::mapStatusToMySqlError(uint32_t status,
                                         uint16_t& error_code,
                                         std::string& sqlstate) {
    switch (static_cast<core::Status>(status)) {
        case core::Status::SYNTAX_ERROR:
        case core::Status::INVALID_ARGUMENT:
            error_code = mysql::ErrorCode::SYNTAX_ERROR;
            sqlstate = "42000";
            break;
        case core::Status::NOT_FOUND:
            error_code = mysql::ErrorCode::NO_SUCH_TABLE;
            sqlstate = "42S02";
            break;
        case core::Status::PERMISSION_DENIED:
            error_code = mysql::ErrorCode::ACCESS_DENIED;
            sqlstate = "28000";
            break;
        case core::Status::FILE_EXISTS:
        case core::Status::DUPLICATE_TABLE:
            error_code = mysql::ErrorCode::TABLE_EXISTS_ERROR;
            sqlstate = "42S01";
            break;
        case core::Status::CONSTRAINT_VIOLATION:
        case core::Status::UNIQUE_VIOLATION:
            error_code = 1062;  // ER_DUP_ENTRY
            sqlstate = "23000";
            break;
        case core::Status::FOREIGN_KEY_VIOLATION:
            error_code = 1215;  // ER_CANNOT_ADD_FOREIGN
            sqlstate = "23000";
            break;
        case core::Status::NOT_NULL_VIOLATION:
        case core::Status::NULL_VALUE_NOT_ALLOWED:
            error_code = 1048;  // ER_BAD_NULL_ERROR
            sqlstate = "23000";
            break;
        case core::Status::LOCK_TIMEOUT:
        case core::Status::LOCK_NOT_AVAILABLE:
            error_code = 1205;  // ER_LOCK_WAIT_TIMEOUT
            sqlstate = "HY000";
            break;
        case core::Status::DATETIME_FIELD_OVERFLOW:
            error_code = mysql::ErrorCode::UNKNOWN_ERROR;
            sqlstate = "22008";
            break;
        case core::Status::INVALID_DATETIME_FORMAT:
            error_code = mysql::ErrorCode::UNKNOWN_ERROR;
            sqlstate = "22007";
            break;
        case core::Status::DATATYPE_MISMATCH:
            error_code = mysql::ErrorCode::UNKNOWN_ERROR;
            sqlstate = "42804";
            break;
        case core::Status::DEADLOCK:
            error_code = 1213;  // ER_LOCK_DEADLOCK
            sqlstate = "40001";
            break;
        case core::Status::TOO_MANY_CONNECTIONS:
            error_code = 1040;  // ER_CON_COUNT_ERROR
            sqlstate = "08004";
            break;
        case core::Status::NOT_IMPLEMENTED:
            error_code = 1235;  // ER_NOT_SUPPORTED_YET
            sqlstate = "0A000";
            break;
        case core::Status::IO_ERROR:
            error_code = 2013;  // Lost connection or generic IO
            sqlstate = "HY000";
            break;
        case core::Status::INTERNAL_ERROR:
        default:
            if (error_code == 0) {
                error_code = mysql::ErrorCode::UNKNOWN_ERROR;
            }
            if (sqlstate.empty()) {
                sqlstate = "HY000";
            }
            break;
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

void MySqlAdapter::bootstrapInformationSchema(core::ErrorContext* ctx) {
    if (information_schema_bootstrapped_ || !client_) {
        return;
    }

    std::string db_name = database_name_.empty() ? "default" : database_name_;
    std::string base_schema = "remote.emulation.mysql.localhost.databases." + db_name;
    std::string info_schema = base_schema + ".information_schema";

    auto safeExec = [&](const std::string& sql) {
        client::ResultSet rs;
        client_->executeQuery(sql, &rs, ctx);
    };

    auto info_table = [&](const std::string& name) {
        return info_schema + "." + name;
    };

    safeExec("CREATE SCHEMA IF NOT EXISTS " + info_schema);
    safeExec("CREATE TABLE IF NOT EXISTS " + info_table("schemata") + " ("
             "catalog_name TEXT, schema_name TEXT, default_character_set_name TEXT, default_collation_name TEXT)");
    safeExec("CREATE TABLE IF NOT EXISTS " + info_table("tables") + " ("
             "table_schema TEXT, table_name TEXT, table_type TEXT)");
    safeExec("CREATE TABLE IF NOT EXISTS " + info_table("columns") + " ("
             "table_schema TEXT, table_name TEXT, column_name TEXT, ordinal_position INT,"
             "data_type TEXT, is_nullable TEXT, character_maximum_length INT, numeric_precision INT, numeric_scale INT)");
    safeExec("CREATE TABLE IF NOT EXISTS " + info_table("table_constraints") + " ("
             "constraint_schema TEXT, constraint_name TEXT, table_schema TEXT, table_name TEXT, constraint_type TEXT)");
    safeExec("CREATE TABLE IF NOT EXISTS " + info_table("key_column_usage") + " ("
             "constraint_schema TEXT, constraint_name TEXT, table_schema TEXT, table_name TEXT,"
             "column_name TEXT, ordinal_position INT)");
    safeExec("CREATE TABLE IF NOT EXISTS " + info_table("referential_constraints") + " ("
             "constraint_schema TEXT, constraint_name TEXT, unique_constraint_schema TEXT, unique_constraint_name TEXT,"
             "match_option TEXT, update_rule TEXT, delete_rule TEXT, table_name TEXT, referenced_table_name TEXT)");
    safeExec("CREATE TABLE IF NOT EXISTS " + info_table("statistics") + " ("
             "table_schema TEXT, table_name TEXT, non_unique INT, index_schema TEXT, index_name TEXT, "
             "seq_in_index INT, column_name TEXT, collation TEXT, cardinality BIGINT, index_type TEXT)");
    safeExec("CREATE TABLE IF NOT EXISTS " + info_table("routines") + " ("
             "routine_schema TEXT, routine_name TEXT, routine_type TEXT, data_type TEXT)");
    safeExec("CREATE TABLE IF NOT EXISTS " + info_table("triggers") + " ("
             "trigger_schema TEXT, trigger_name TEXT, event_manipulation TEXT, "
             "event_object_schema TEXT, event_object_table TEXT, "
             "action_statement TEXT, action_timing TEXT)");

    std::string delete_existing = "DELETE FROM " + info_table("schemata") + " "
                                  "WHERE schema_name = '" + escapeLiteral(db_name) + "'";
    safeExec(delete_existing);
    std::string insert_schema = "INSERT INTO " + info_table("schemata") + " "
        "(catalog_name, schema_name, default_character_set_name, default_collation_name) VALUES "
        "('def','" + escapeLiteral(db_name) + "','utf8mb4','utf8mb4_general_ci')";
    safeExec(insert_schema);

    // Describe the bootstrap tables themselves
    safeExec("DELETE FROM " + info_table("tables") + " WHERE table_schema IN ('information_schema')");
    safeExec("DELETE FROM " + info_table("columns") + " WHERE table_schema IN ('information_schema')");
    safeExec("DELETE FROM " + info_table("table_constraints") + " WHERE constraint_schema IN ('information_schema')");
    safeExec("DELETE FROM " + info_table("key_column_usage") + " WHERE constraint_schema IN ('information_schema')");
    safeExec("DELETE FROM " + info_table("referential_constraints") + " WHERE constraint_schema IN ('information_schema')");
    safeExec("DELETE FROM " + info_table("statistics") + " WHERE table_schema IN ('information_schema')");
    safeExec("DELETE FROM " + info_table("triggers") + " WHERE trigger_schema IN ('information_schema')");

    auto insertTable = [&](const std::string& name) {
        std::string sql = "INSERT INTO " + info_table("tables") + " "
                          "(table_schema, table_name, table_type) VALUES "
                          "('information_schema','" + escapeLiteral(name) + "','BASE TABLE')";
        safeExec(sql);
    };

    auto insertColumn = [&](const std::string& tbl, int pos, const std::string& col,
                            const std::string& type, const std::string& nullable,
                            int char_len = -1, int num_prec = -1, int num_scale = -1) {
        std::string sql = "INSERT INTO " + info_table("columns") + " "
                          "(table_schema, table_name, column_name, ordinal_position, data_type, is_nullable, "
                          "character_maximum_length, numeric_precision, numeric_scale) VALUES "
                          "('information_schema','" + escapeLiteral(tbl) + "','" + escapeLiteral(col) + "'," +
                          std::to_string(pos) + ",'" + escapeLiteral(type) + "','" + escapeLiteral(nullable) + "',";
        sql += (char_len >= 0 ? std::to_string(char_len) : "NULL");
        sql += ",";
        sql += (num_prec >= 0 ? std::to_string(num_prec) : "NULL");
        sql += ",";
        sql += (num_scale >= 0 ? std::to_string(num_scale) : "NULL");
        sql += ")";
        safeExec(sql);
    };

    insertTable("schemata");
    insertColumn("schemata", 1, "catalog_name", "varchar", "YES", 256);
    insertColumn("schemata", 2, "schema_name", "varchar", "NO", 256);
    insertColumn("schemata", 3, "default_character_set_name", "varchar", "YES", 64);
    insertColumn("schemata", 4, "default_collation_name", "varchar", "YES", 64);

    insertTable("tables");
    insertColumn("tables", 1, "table_schema", "varchar", "YES", 256);
    insertColumn("tables", 2, "table_name", "varchar", "YES", 256);
    insertColumn("tables", 3, "table_type", "varchar", "YES", 64);

    insertTable("columns");
    insertColumn("columns", 1, "table_schema", "varchar", "YES", 256);
    insertColumn("columns", 2, "table_name", "varchar", "YES", 256);
    insertColumn("columns", 3, "column_name", "varchar", "YES", 256);
    insertColumn("columns", 4, "ordinal_position", "int", "NO");
    insertColumn("columns", 5, "data_type", "varchar", "YES", 64);
    insertColumn("columns", 6, "is_nullable", "varchar", "YES", 3);
    insertColumn("columns", 7, "character_maximum_length", "int", "YES");
    insertColumn("columns", 8, "numeric_precision", "int", "YES");
    insertColumn("columns", 9, "numeric_scale", "int", "YES");

    insertTable("table_constraints");
    insertColumn("table_constraints", 1, "constraint_schema", "varchar", "YES", 256);
    insertColumn("table_constraints", 2, "constraint_name", "varchar", "YES", 256);
    insertColumn("table_constraints", 3, "table_schema", "varchar", "YES", 256);
    insertColumn("table_constraints", 4, "table_name", "varchar", "YES", 256);
    insertColumn("table_constraints", 5, "constraint_type", "varchar", "YES", 64);

    insertTable("key_column_usage");
    insertColumn("key_column_usage", 1, "constraint_schema", "varchar", "YES", 256);
    insertColumn("key_column_usage", 2, "constraint_name", "varchar", "YES", 256);
    insertColumn("key_column_usage", 3, "table_schema", "varchar", "YES", 256);
    insertColumn("key_column_usage", 4, "table_name", "varchar", "YES", 256);
    insertColumn("key_column_usage", 5, "column_name", "varchar", "YES", 256);
    insertColumn("key_column_usage", 6, "ordinal_position", "int", "YES");

    insertTable("referential_constraints");
    insertColumn("referential_constraints", 1, "constraint_schema", "varchar", "YES", 256);
    insertColumn("referential_constraints", 2, "constraint_name", "varchar", "YES", 256);
    insertColumn("referential_constraints", 3, "unique_constraint_schema", "varchar", "YES", 256);
    insertColumn("referential_constraints", 4, "unique_constraint_name", "varchar", "YES", 256);
    insertColumn("referential_constraints", 5, "match_option", "varchar", "YES", 64);
    insertColumn("referential_constraints", 6, "update_rule", "varchar", "YES", 16);
    insertColumn("referential_constraints", 7, "delete_rule", "varchar", "YES", 16);
    insertColumn("referential_constraints", 8, "table_name", "varchar", "YES", 256);
    insertColumn("referential_constraints", 9, "referenced_table_name", "varchar", "YES", 256);

    insertTable("statistics");
    insertColumn("statistics", 1, "table_schema", "varchar", "YES", 256);
    insertColumn("statistics", 2, "table_name", "varchar", "YES", 256);
    insertColumn("statistics", 3, "non_unique", "int", "YES");
    insertColumn("statistics", 4, "index_schema", "varchar", "YES", 256);
    insertColumn("statistics", 5, "index_name", "varchar", "YES", 256);
    insertColumn("statistics", 6, "seq_in_index", "int", "YES");
    insertColumn("statistics", 7, "column_name", "varchar", "YES", 256);
    insertColumn("statistics", 8, "collation", "varchar", "YES", 1);
    insertColumn("statistics", 9, "cardinality", "bigint", "YES");
    insertColumn("statistics", 10, "index_type", "varchar", "YES", 16);

    insertTable("routines");
    insertColumn("routines", 1, "routine_schema", "varchar", "YES", 256);
    insertColumn("routines", 2, "routine_name", "varchar", "YES", 256);
    insertColumn("routines", 3, "routine_type", "varchar", "YES", 16);
    insertColumn("routines", 4, "data_type", "varchar", "YES", 64);

    insertTable("triggers");
    insertColumn("triggers", 1, "trigger_schema", "varchar", "YES", 256);
    insertColumn("triggers", 2, "trigger_name", "varchar", "YES", 256);
    insertColumn("triggers", 3, "event_manipulation", "varchar", "YES", 16);
    insertColumn("triggers", 4, "event_object_schema", "varchar", "YES", 256);
    insertColumn("triggers", 5, "event_object_table", "varchar", "YES", 256);
    insertColumn("triggers", 6, "action_statement", "varchar", "YES", 1024);
    insertColumn("triggers", 7, "action_timing", "varchar", "YES", 16);

    // Copy real catalog projections into the emulated schema if available
    auto toString = [](const protocol::ProtocolCodec::ColumnValue& cv) {
        return std::string(cv.data.begin(), cv.data.end());
    };
    auto numberOrNull = [](const std::string& s) {
        if (s.empty()) return std::string("NULL");
        return s;
    };

    auto copyQuery = [&](const std::string& sql,
                         const std::function<void(const client::ResultSet&, size_t)>& rowHandler) {
        client::ResultSet rs;
        if (client_->executeQuery(sql, &rs, ctx) != core::Status::OK) {
            return;
        }
        int64_t rows = rs.getRowCount();
        for (int64_t i = 0; i < rows; ++i) {
            rowHandler(rs, static_cast<size_t>(i));
        }
    };

    std::string insert_prefix_schemata = "INSERT INTO " + info_table("schemata") + " "
        "(catalog_name, schema_name, default_character_set_name, default_collation_name) VALUES ";
    copyQuery("SELECT catalog_name, schema_name, default_character_set_name, default_collation_name "
              "FROM information_schema.schemata "
              "WHERE schema_name NOT IN ('information_schema','pg_catalog')",
              [&](const client::ResultSet& rs, size_t idx) {
        const auto& row = rs.getRowValues(idx);
        if (row.size() < 4) return;
        std::string sql = insert_prefix_schemata + "('def','" + escapeLiteral(toString(row[1])) +
            "','" + escapeLiteral(toString(row[2])) + "','" + escapeLiteral(toString(row[3])) + "')";
        safeExec(sql);
    });

    std::string insert_prefix_constraints = "INSERT INTO " + info_table("table_constraints") + " "
        "(constraint_schema, constraint_name, table_schema, table_name, constraint_type) VALUES ";
    copyQuery("SELECT constraint_schema, constraint_name, table_schema, table_name, constraint_type "
              "FROM information_schema.table_constraints "
              "WHERE constraint_schema NOT IN ('information_schema','pg_catalog')",
              [&](const client::ResultSet& rs, size_t idx) {
        const auto& row = rs.getRowValues(idx);
        if (row.size() < 5) return;
        std::string sql = insert_prefix_constraints + "('" +
            escapeLiteral(toString(row[0])) + "','" +
            escapeLiteral(toString(row[1])) + "','" +
            escapeLiteral(toString(row[2])) + "','" +
            escapeLiteral(toString(row[3])) + "','" +
            escapeLiteral(toString(row[4])) + "')";
        safeExec(sql);
    });

    std::string insert_prefix_kcu = "INSERT INTO " + info_table("key_column_usage") + " "
        "(constraint_schema, constraint_name, table_schema, table_name, column_name, ordinal_position) VALUES ";
    copyQuery("SELECT constraint_schema, constraint_name, table_schema, table_name, column_name, ordinal_position "
              "FROM information_schema.key_column_usage "
              "WHERE constraint_schema NOT IN ('information_schema','pg_catalog')",
              [&](const client::ResultSet& rs, size_t idx) {
        const auto& row = rs.getRowValues(idx);
        if (row.size() < 6) return;
        std::string sql = insert_prefix_kcu + "('" +
            escapeLiteral(toString(row[0])) + "','" +
            escapeLiteral(toString(row[1])) + "','" +
            escapeLiteral(toString(row[2])) + "','" +
            escapeLiteral(toString(row[3])) + "','" +
            escapeLiteral(toString(row[4])) + "'," +
            numberOrNull(toString(row[5])) + ")";
        safeExec(sql);
    });

    std::string insert_prefix_ref = "INSERT INTO " + info_table("referential_constraints") + " "
        "(constraint_schema, constraint_name, unique_constraint_schema, unique_constraint_name, "
        "match_option, update_rule, delete_rule, table_name, referenced_table_name) VALUES ";
    copyQuery("SELECT constraint_schema, constraint_name, unique_constraint_schema, unique_constraint_name, "
              "match_option, update_rule, delete_rule, table_name, referenced_table_name "
              "FROM information_schema.referential_constraints "
              "WHERE constraint_schema NOT IN ('information_schema','pg_catalog')",
              [&](const client::ResultSet& rs, size_t idx) {
        const auto& row = rs.getRowValues(idx);
        if (row.size() < 9) return;
        std::string sql = insert_prefix_ref + "('" +
            escapeLiteral(toString(row[0])) + "','" +
            escapeLiteral(toString(row[1])) + "','" +
            escapeLiteral(toString(row[2])) + "','" +
            escapeLiteral(toString(row[3])) + "','" +
            escapeLiteral(toString(row[4])) + "','" +
            escapeLiteral(toString(row[5])) + "','" +
            escapeLiteral(toString(row[6])) + "','" +
            escapeLiteral(toString(row[7])) + "','" +
            escapeLiteral(toString(row[8])) + "')";
        safeExec(sql);
    });

    std::string insert_prefix_stats = "INSERT INTO " + info_table("statistics") + " "
        "(table_schema, table_name, non_unique, index_schema, index_name, seq_in_index, column_name, collation, cardinality, index_type) VALUES ";
    copyQuery("SELECT table_schema, table_name, non_unique, index_schema, index_name, seq_in_index, "
              "column_name, collation, cardinality, index_type "
              "FROM information_schema.statistics "
              "WHERE table_schema NOT IN ('information_schema','pg_catalog')",
              [&](const client::ResultSet& rs, size_t idx) {
        const auto& row = rs.getRowValues(idx);
        if (row.size() < 10) return;
        std::string sql = insert_prefix_stats + "('" +
            escapeLiteral(toString(row[0])) + "','" +
            escapeLiteral(toString(row[1])) + "'," +
            numberOrNull(toString(row[2])) + ",'" +
            escapeLiteral(toString(row[3])) + "','" +
            escapeLiteral(toString(row[4])) + "'," +
            numberOrNull(toString(row[5])) + ",'" +
            escapeLiteral(toString(row[6])) + "','" +
            escapeLiteral(toString(row[7])) + "'," +
            numberOrNull(toString(row[8])) + ",'" +
            escapeLiteral(toString(row[9])) + "')";
        safeExec(sql);
    });

    std::string insert_prefix_routines = "INSERT INTO " + info_table("routines") + " "
        "(routine_schema, routine_name, routine_type, data_type) VALUES ";
    copyQuery("SELECT routine_schema, routine_name, routine_type, data_type "
              "FROM information_schema.routines "
              "WHERE routine_schema NOT IN ('information_schema','pg_catalog')",
              [&](const client::ResultSet& rs, size_t idx) {
        const auto& row = rs.getRowValues(idx);
        if (row.size() < 4) return;
        std::string sql = insert_prefix_routines + "('" +
            escapeLiteral(toString(row[0])) + "','" +
            escapeLiteral(toString(row[1])) + "','" +
            escapeLiteral(toString(row[2])) + "','" +
            escapeLiteral(toString(row[3])) + "')";
        safeExec(sql);
    });

    std::string insert_prefix_tables = "INSERT INTO " + info_table("tables") + " "
        "(table_schema, table_name, table_type) VALUES ";
    copyQuery("SELECT table_schema, table_name, table_type "
              "FROM information_schema.tables "
              "WHERE table_schema NOT IN ('information_schema','pg_catalog')",
              [&](const client::ResultSet& rs, size_t idx) {
        const auto& row = rs.getRowValues(idx);
        if (row.size() < 3) return;
        std::string sql = insert_prefix_tables + "('" + escapeLiteral(toString(row[0])) + "','" +
            escapeLiteral(toString(row[1])) + "','" + escapeLiteral(toString(row[2])) + "')";
        safeExec(sql);
    });

    std::string insert_prefix_columns = "INSERT INTO " + info_table("columns") + " "
        "(table_schema, table_name, column_name, ordinal_position, data_type, is_nullable, "
        "character_maximum_length, numeric_precision, numeric_scale) VALUES ";
    copyQuery("SELECT table_schema, table_name, column_name, ordinal_position, data_type, "
              "is_nullable, character_maximum_length, numeric_precision, numeric_scale "
              "FROM information_schema.columns "
              "WHERE table_schema NOT IN ('information_schema','pg_catalog')",
              [&](const client::ResultSet& rs, size_t idx) {
        const auto& row = rs.getRowValues(idx);
        if (row.size() < 9) return;
        std::string sql = insert_prefix_columns + "('" +
            escapeLiteral(toString(row[0])) + "','" +
            escapeLiteral(toString(row[1])) + "','" +
            escapeLiteral(toString(row[2])) + "'," +
            numberOrNull(toString(row[3])) + ",'" +
            escapeLiteral(toString(row[4])) + "','" +
            escapeLiteral(toString(row[5])) + "'," +
            numberOrNull(toString(row[6])) + "," +
            numberOrNull(toString(row[7])) + "," +
            numberOrNull(toString(row[8])) + ")";
        safeExec(sql);
    });

    auto* db = engineDatabase();
    auto* catalog = db ? db->catalog_manager() : nullptr;
    if (catalog) {
        auto normalize_schema_name = [&](const std::string& raw) {
            std::string prefix = base_schema;
            if (raw.rfind(prefix, 0) == 0) {
                std::string rest = raw.substr(prefix.size());
                if (!rest.empty() && rest.front() == '.') {
                    rest.erase(0, 1);
                }
                if (rest.empty()) {
                    return db_name;
                }
                return rest;
            }
            return raw;
        };

        std::vector<core::CatalogManager::SchemaInfo> schemas;
        if (catalog->listSchemas(schemas, ctx) == core::Status::OK) {
            std::string insert_prefix_triggers = "INSERT INTO " + info_table("triggers") + " "
                "(trigger_schema, trigger_name, event_manipulation, event_object_schema, "
                "event_object_table, action_statement, action_timing) VALUES ";

            for (const auto& schema : schemas) {
                std::string schema_name = normalize_schema_name(schema.full_path.empty()
                                                                  ? schema.schema_name
                                                                  : schema.full_path);
                std::vector<core::CatalogManager::TableInfo> tables;
                if (catalog->listTables(schema.schema_id, tables, ctx) != core::Status::OK) {
                    continue;
                }
                for (const auto& table : tables) {
                    std::vector<core::CatalogManager::TriggerInfo> triggers;
                    if (catalog->listAllTriggersForTable(table.table_id, triggers, ctx) != core::Status::OK) {
                        continue;
                    }
                    for (const auto& trigger : triggers) {
                        auto timing = [&]() -> std::string {
                            switch (trigger.timing) {
                                case core::CatalogManager::TriggerTiming::BEFORE: return "BEFORE";
                                case core::CatalogManager::TriggerTiming::AFTER: return "AFTER";
                                case core::CatalogManager::TriggerTiming::INSTEAD_OF: return "INSTEAD OF";
                                default: return "BEFORE";
                            }
                        }();
                        auto emit_trigger_row = [&](const std::string& event_name) {
                            std::string sql = insert_prefix_triggers + "('" +
                                escapeLiteral(schema_name) + "','" +
                                escapeLiteral(trigger.trigger_name) + "','" +
                                escapeLiteral(event_name) + "','" +
                                escapeLiteral(schema_name) + "','" +
                                escapeLiteral(table.table_name) + "','" +
                                escapeLiteral(trigger.procedure_name) + "','" +
                                escapeLiteral(timing) + "')";
                            safeExec(sql);
                        };
                        if (trigger.event_mask & (1u << static_cast<uint8_t>(
                                core::CatalogManager::TriggerEvent::INSERT))) {
                            emit_trigger_row("INSERT");
                        }
                        if (trigger.event_mask & (1u << static_cast<uint8_t>(
                                core::CatalogManager::TriggerEvent::UPDATE))) {
                            emit_trigger_row("UPDATE");
                        }
                        if (trigger.event_mask & (1u << static_cast<uint8_t>(
                                core::CatalogManager::TriggerEvent::DELETE))) {
                            emit_trigger_row("DELETE");
                        }
                    }
                }
            }
        }
    }

    information_schema_bootstrapped_ = true;
}

} // namespace protocol
} // namespace scratchbird
