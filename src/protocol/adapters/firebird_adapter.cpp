/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * Firebird Wire Protocol Adapter Implementation
 *
 * ScratchBird Network Layer - Phase 3.2
 *
 * Implements Firebird wire protocol for client compatibility.
 */

#include "scratchbird/protocol/adapters/firebird_adapter.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/sblr/firebird_query_compiler.h"

#include <cstring>
#include <random>
#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <string_view>

namespace scratchbird {
namespace protocol {

namespace {
struct FirebirdDatabaseSpec {
    std::string server;
    std::string file_path;
};

FirebirdDatabaseSpec parseFirebirdDatabaseSpec(std::string_view spec) {
    FirebirdDatabaseSpec result;
    result.file_path = std::string(spec);

    size_t colon = result.file_path.find(':');
    if (colon != std::string::npos) {
        bool is_drive = (colon == 1 &&
                         std::isalpha(static_cast<unsigned char>(result.file_path[0])) &&
                         result.file_path.size() > 2 &&
                         (result.file_path[2] == '\\' || result.file_path[2] == '/'));
        if (!is_drive) {
            result.server = result.file_path.substr(0, colon);
            result.file_path.erase(0, colon + 1);
        }
    }

    return result;
}

std::vector<std::string> splitFirebirdPathComponents(std::string_view path) {
    std::string working(path);
    std::vector<std::string> components;

    if (working.size() >= 2 && std::isalpha(static_cast<unsigned char>(working[0])) &&
        working[1] == ':') {
        std::string drive(1, static_cast<char>(std::tolower(static_cast<unsigned char>(working[0]))));
        components.push_back(drive);
        working.erase(0, 2);
    }

    while (!working.empty() && (working.front() == '/' || working.front() == '\\')) {
        working.erase(working.begin());
    }

    std::string current;
    for (char ch : working) {
        if (ch == '/' || ch == '\\') {
            if (!current.empty()) {
                components.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        components.push_back(current);
    }

    if (!components.empty()) {
        components.pop_back();
    }

    return components;
}

std::string deriveFirebirdDatabaseName(std::string_view file_path) {
    size_t last_sep = file_path.find_last_of("/\\");
    std::string base = (last_sep == std::string_view::npos)
        ? std::string(file_path)
        : std::string(file_path.substr(last_sep + 1));

    if (base.empty()) {
        return base;
    }

    size_t dot = base.find_last_of('.');
    if (dot != std::string::npos && dot + 1 < base.size()) {
        std::string ext = base.substr(dot + 1);
        for (char& ch : ext) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (ext == "fdb" || ext == "gdb") {
            base = base.substr(0, dot);
        }
    }

    return base;
}

std::string buildEmulatedFirebirdSchemaPath(const std::string& server,
                                            const std::vector<std::string>& path_components,
                                            const std::string& db_name) {
    std::string schema = "remote.emulation.firebird." + server;
    for (const auto& comp : path_components) {
        if (!comp.empty()) {
            schema.push_back('.');
            schema += comp;
        }
    }
    if (!db_name.empty()) {
        schema.push_back('.');
        schema += db_name;
    }
    return schema;
}

// Minimal BLR parser for SQLDA (scalar fields only; text/varchar/int sizes)
core::Status parseBlr(const std::vector<uint8_t>& blr,
                      std::vector<FirebirdStatement::BlrField>& fields_out,
                      uint32_t& message_length_out,
                      std::string& error_out) {
    fields_out.clear();
    message_length_out = 0;

    if (blr.empty()) {
        return core::Status::OK;  // No BLR supplied
    }

    size_t idx = 0;
    auto require = [&](size_t n) -> bool {
        if (idx + n > blr.size()) {
            error_out = "BLR truncated";
            return false;
        }
        return true;
    };

    if (!require(1)) return core::Status::INVALID_ARGUMENT;
    uint8_t version = blr[idx++];
    if (version != 4) {  // Firebird BLR version 4
        error_out = "Unsupported BLR version";
        return core::Status::INVALID_ARGUMENT;
    }

    if (!require(1) || blr[idx++] != 2) {  // blr_begin
        error_out = "Expected BLR begin";
        return core::Status::INVALID_ARGUMENT;
    }

    if (!require(1) || blr[idx++] != 4) {  // blr_message
        error_out = "Expected BLR message";
        return core::Status::INVALID_ARGUMENT;
    }

    if (!require(1)) return core::Status::INVALID_ARGUMENT; // message number
    idx++;

    if (!require(2)) return core::Status::INVALID_ARGUMENT;
    uint16_t field_count = static_cast<uint16_t>(blr[idx] | (blr[idx + 1] << 8));
    idx += 2;

    for (uint16_t i = 0; i < field_count; ++i) {
        if (!require(1)) return core::Status::INVALID_ARGUMENT;
        uint8_t opcode = blr[idx++];
        FirebirdStatement::BlrField field{};

        switch (opcode) {
            case 7: { // blr_short
                if (!require(1)) return core::Status::INVALID_ARGUMENT;
                int8_t scale = static_cast<int8_t>(blr[idx++]);
                field.dtype = opcode;
                field.scale = scale;
                field.length = 2;
                break;
            }
            case 8: { // blr_long
                if (!require(1)) return core::Status::INVALID_ARGUMENT;
                int8_t scale = static_cast<int8_t>(blr[idx++]);
                field.dtype = opcode;
                field.scale = scale;
                field.length = 4;
                break;
            }
            case 16: { // blr_int64
                if (!require(1)) return core::Status::INVALID_ARGUMENT;
                int8_t scale = static_cast<int8_t>(blr[idx++]);
                field.dtype = opcode;
                field.scale = scale;
                field.length = 8;
                break;
            }
            case 14: { // blr_text
                if (!require(2)) return core::Status::INVALID_ARGUMENT;
                uint16_t len = static_cast<uint16_t>(blr[idx] | (blr[idx + 1] << 8));
                idx += 2;
                field.dtype = opcode;
                field.scale = 0;
                field.length = len;
                field.is_text = true;
                break;
            }
            case 37: { // blr_varying
                if (!require(2)) return core::Status::INVALID_ARGUMENT;
                uint16_t len = static_cast<uint16_t>(blr[idx] | (blr[idx + 1] << 8));
                idx += 2;
                field.dtype = opcode;
                field.scale = 0;
                field.length = len + 2; // includes length prefix
                field.is_varying = true;
                break;
            }
            default:
                error_out = "Unsupported BLR datatype opcode " + std::to_string(opcode);
                return core::Status::INVALID_ARGUMENT;
        }

        fields_out.push_back(field);
    }

    if (!require(1) || blr[idx] != 255) {  // blr_end
        error_out = "Missing BLR end marker";
        return core::Status::INVALID_ARGUMENT;
    }

    // Compute message length as sum of field lengths plus NULL indicators
    uint32_t total_len = 0;
    for (const auto& f : fields_out) {
        total_len += f.length;
    }
    if (!fields_out.empty()) {
        total_len += static_cast<uint32_t>(fields_out.size()) * 2; // null indicators (int16 each)
    }
    message_length_out = total_len;
    return core::Status::OK;
}

int32_t mapStatusToFirebird(core::Status st) {
    using core::Status;
    switch (st) {
        case Status::OK:
            return firebird::ErrorCode::isc_sqlerr;
        case Status::INVALID_ARGUMENT:
        case Status::SYNTAX_ERROR:
        case Status::UNDEFINED_TABLE:
        case Status::UNDEFINED_COLUMN:
        case Status::UNDEFINED_FUNCTION:
        case Status::DUPLICATE_TABLE:
        case Status::DUPLICATE_COLUMN:
        case Status::DUPLICATE_OBJECT:
        case Status::CONSTRAINT_VIOLATION:
        case Status::NOT_NULL_VIOLATION:
        case Status::FOREIGN_KEY_VIOLATION:
        case Status::UNIQUE_VIOLATION:
        case Status::CHECK_VIOLATION:
        case Status::EXCLUSION_VIOLATION:
        case Status::TYPE_MISMATCH:
        case Status::STRING_DATA_RIGHT_TRUNCATION:
        case Status::INVALID_TEXT_REPRESENTATION:
            return firebird::ErrorCode::isc_dsql_error;
        case Status::CONNECTION_FAILURE:
        case Status::CONNECTION_DOES_NOT_EXIST:
        case Status::IO_ERROR:
        case Status::CRASH_SHUTDOWN:
        case Status::DATABASE_DROPPED:
        case Status::ADMIN_SHUTDOWN:
            return firebird::ErrorCode::isc_unavailable;
        case Status::PERMISSION_DENIED:
        case Status::INVALID_PASSWORD:
        case Status::INVALID_AUTHORIZATION:
            return firebird::ErrorCode::isc_login;
        case Status::NOT_IMPLEMENTED:
        case Status::NOT_SUPPORTED:
        case Status::STATEMENT_TOO_COMPLEX:
        case Status::TOO_MANY_COLUMNS:
        case Status::CONFIGURATION_LIMIT_EXCEEDED:
            return firebird::ErrorCode::isc_unavailable;
        default:
            return firebird::ErrorCode::isc_dsql_error;
    }
}

} // namespace

namespace {
FirebirdStatement::BlrField columnToBlrField(const ProtocolCodec::ColumnInfo& col) {
    FirebirdStatement::BlrField field{};
    switch (col.type) {
        case WireType::BOOLEAN:
            field.dtype = 7;  // blr_short for booleans (int16)
            field.length = 2;
            break;
        case WireType::INT16:
            field.dtype = 7;  // blr_short
            field.length = 2;
            break;
        case WireType::INT32:
        case WireType::DATE:
            field.dtype = 8;  // blr_long
            field.length = 4;
            break;
        case WireType::INT64:
        case WireType::TIMESTAMP:
        case WireType::TIMESTAMPTZ:
            field.dtype = 16;  // blr_int64
            field.length = 8;
            break;
        case WireType::FLOAT32:
            field.dtype = 27;  // blr_float
            field.length = 4;
            break;
        case WireType::FLOAT64:
            field.dtype = 26;  // blr_double
            field.length = 8;
            break;
        case WireType::CHAR: {
            field.dtype = 14;  // blr_text
            // type_modifier typically encodes length for fixed-width char
            uint16_t len = col.type_modifier > 0
                               ? static_cast<uint16_t>(std::min<uint32_t>(col.type_modifier, std::numeric_limits<uint16_t>::max()))
                               : static_cast<uint16_t>(32);
            field.length = len;
            field.is_text = true;
            break;
        }
        case WireType::VARCHAR:
        case WireType::JSON:
        case WireType::JSONB:
        case WireType::XML: {
            field.dtype = 37;  // blr_varying
            uint16_t len = col.type_modifier > 0
                               ? static_cast<uint16_t>(std::min<uint32_t>(col.type_modifier, std::numeric_limits<uint16_t>::max()))
                               : static_cast<uint16_t>(256);
            // BLR varying length includes the 2-byte prefix in the message
            field.length = len + 2;
            field.is_varying = true;
            break;
        }
        case WireType::BYTEA:
        case WireType::UUID:
        case WireType::INET:
        case WireType::CIDR:
        case WireType::MACADDR:
        default: {
            field.dtype = 37;  // blr_varying for generic/binary payloads
            uint16_t len = col.type_modifier > 0
                               ? static_cast<uint16_t>(std::min<uint32_t>(col.type_modifier, std::numeric_limits<uint16_t>::max()))
                               : static_cast<uint16_t>(512);
            field.length = len + 2;
            field.is_varying = true;
            break;
        }
    }
    return field;
}
} // namespace

// ============================================================================
// Constructor/Destructor
// ============================================================================

FirebirdAdapter::FirebirdAdapter(const ProtocolAdapterConfig& config)
    : ProtocolAdapter(config) {

    // Generate initial handles
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(1, 0x7FFFFFFF);

    next_db_handle_ = dist(gen);
    next_tr_handle_ = dist(gen);
    next_stmt_handle_ = dist(gen);
}

FirebirdAdapter::~FirebirdAdapter() = default;

void FirebirdAdapter::setRemoteCredentials(const std::string& username,
                                           const std::string& password) {
    username_ = username;
    remote_password_ = password;
}

// ============================================================================
// ProtocolAdapter Implementation
// ============================================================================

core::Status FirebirdAdapter::parseMessage(network::Connection* conn) {
    const auto& buffer = conn->getReadBuffer();

    // Firebird protocol: XDR encoded, starts with opcode (4 bytes)
    if (buffer.size() < 4) {
        return core::Status::IO_ERROR;  // Need more data
    }

    // Read opcode (big-endian)
    current_opcode_ = readUInt32(buffer.data());

    // Determine packet length based on opcode
    // Firebird uses variable-length packets, we need to read until complete
    // For simplicity, we'll read available data and process

    // Most operations have a minimum size
    size_t min_size = 4;  // Just opcode for some operations

    switch (current_opcode_) {
        case firebird::Opcode::op_connect:
            // Connect has: opcode + operation + version + arch + min_type + max_type +
            // preference bitmap + protocols array + user_id
            min_size = 28;  // Minimum connect packet
            break;

        case firebird::Opcode::op_attach:
        case firebird::Opcode::op_create:
            min_size = 16;  // opcode + db_handle + db_path_len (minimum)
            break;

        case firebird::Opcode::op_transaction:
            min_size = 12;  // opcode + db_handle + tpb_len
            break;

        case firebird::Opcode::op_commit:
        case firebird::Opcode::op_rollback:
        case firebird::Opcode::op_commit_retaining:
        case firebird::Opcode::op_rollback_retaining:
            min_size = 8;  // opcode + tr_handle
            break;

        case firebird::Opcode::op_allocate_statement:
            min_size = 8;  // opcode + db_handle
            break;

        case firebird::Opcode::op_prepare_statement:
        case firebird::Opcode::op_exec_immediate:
        case firebird::Opcode::op_exec_immediate2:
            min_size = 24;  // opcode + handles + sql_len (minimum)
            break;

        case firebird::Opcode::op_execute:
        case firebird::Opcode::op_execute2:
            min_size = 16;  // opcode + handles
            break;

        case firebird::Opcode::op_fetch:
            min_size = 16;  // opcode + stmt_handle + blr_len + msg_len
            break;

        case firebird::Opcode::op_free_statement:
            min_size = 12;  // opcode + stmt_handle + option
            break;

        case firebird::Opcode::op_detach:
        case firebird::Opcode::op_drop_database:
            min_size = 8;  // opcode + db_handle
            break;

        case firebird::Opcode::op_disconnect:
        case firebird::Opcode::op_ping:
            min_size = 4;  // Just opcode
            break;

        case firebird::Opcode::op_cont_auth:
            min_size = 12;  // opcode + data_len + plugin_len
            break;

        default:
            min_size = 4;
            break;
    }

    if (buffer.size() < min_size) {
        return core::Status::IO_ERROR;  // Need more data
    }

    // For now, consume available data up to a reasonable limit
    // In a full implementation, we'd parse XDR to determine exact length
    size_t packet_len = std::min(buffer.size(), static_cast<size_t>(65536));

    current_packet_.assign(buffer.begin(), buffer.begin() + packet_len);
    conn->consumeReadBuffer(packet_len);

    return core::Status::OK;
}

core::Status FirebirdAdapter::processMessage(network::Connection* conn) {
    bytes_received_ += current_packet_.size();

    switch (current_opcode_) {
        case firebird::Opcode::op_connect:
            return handleConnect(conn);

        case firebird::Opcode::op_attach:
            return handleAttach(conn);

        case firebird::Opcode::op_create:
            return handleCreateDatabase(conn);

        case firebird::Opcode::op_detach:
            return handleDetach(conn);

        case firebird::Opcode::op_drop_database:
            return handleDropDatabase(conn);

        case firebird::Opcode::op_transaction:
            return handleTransaction(conn);

        case firebird::Opcode::op_commit:
            return handleCommit(conn);

        case firebird::Opcode::op_rollback:
            return handleRollback(conn);

        case firebird::Opcode::op_commit_retaining:
            return handleCommitRetaining(conn);

        case firebird::Opcode::op_rollback_retaining:
            return handleRollbackRetaining(conn);

        case firebird::Opcode::op_allocate_statement:
            return handleAllocateStatement(conn);

        case firebird::Opcode::op_prepare_statement:
            return handlePrepareStatement(conn);

        case firebird::Opcode::op_execute:
            return handleExecute(conn);

        case firebird::Opcode::op_execute2:
            return handleExecute2(conn);

        case firebird::Opcode::op_exec_immediate:
            return handleExecImmediate(conn);

        case firebird::Opcode::op_exec_immediate2:
            return handleExecImmediate2(conn);

        case firebird::Opcode::op_fetch:
            return handleFetch(conn);

        case firebird::Opcode::op_free_statement:
            return handleFreeStatement(conn);

        case firebird::Opcode::op_set_cursor:
            return handleSetCursor(conn);

        case firebird::Opcode::op_info_database:
            return handleInfoDatabase(conn);

        case firebird::Opcode::op_info_transaction:
            return handleInfoTransaction(conn);

        case firebird::Opcode::op_info_sql:
            return handleInfoSql(conn);

        case firebird::Opcode::op_cont_auth:
            return handleContAuth(conn);

        case firebird::Opcode::op_ping:
            return handlePing(conn);

        case firebird::Opcode::op_cancel:
            return handleCancel(conn);

        case firebird::Opcode::op_disconnect:
            return handleDisconnect(conn);

        // Service Manager operations (C5.1)
        case firebird::Opcode::op_service_attach:
            return handleServiceAttach(conn);

        case firebird::Opcode::op_service_detach:
            return handleServiceDetach(conn);

        case firebird::Opcode::op_service_info:
            return handleServiceInfo(conn);

        case firebird::Opcode::op_service_start:
            return handleServiceStart(conn);

        // Event operations (C5.2)
        case firebird::Opcode::op_que_events:
            return handleQueEvents(conn);

        case firebird::Opcode::op_cancel_events:
            return handleCancelEvents(conn);

        // BLOB operations (C5.3)
        case firebird::Opcode::op_create_blob:
            return handleCreateBlob(conn);

        case firebird::Opcode::op_create_blob2:
            return handleCreateBlob2(conn);

        case firebird::Opcode::op_open_blob:
            return handleOpenBlob(conn);

        case firebird::Opcode::op_open_blob2:
            return handleOpenBlob2(conn);

        case firebird::Opcode::op_close_blob:
            return handleCloseBlob(conn);

        case firebird::Opcode::op_cancel_blob:
            return handleCancelBlob(conn);

        case firebird::Opcode::op_get_segment:
            return handleGetSegment(conn);

        case firebird::Opcode::op_put_segment:
            return handlePutSegment(conn);

        case firebird::Opcode::op_seek_blob:
            return handleSeekBlob(conn);

        default:
            sendErrorResponse(conn, firebird::ErrorCode::isc_unavailable,
                             "Unsupported operation: " + std::to_string(current_opcode_));
            return sendBuffer(conn);
    }
}

core::Status FirebirdAdapter::sendGreeting(network::Connection* /*conn*/) {
    // Firebird: Server waits for client op_connect
    return core::Status::OK;
}

core::Status FirebirdAdapter::processAuthentication(network::Connection* /*conn*/) {
    // Authentication handled in handleConnect/handleContAuth
    return core::Status::OK;
}

core::Status FirebirdAdapter::sendAuthResult(network::Connection* conn,
                                              bool success,
                                              const std::string& error_msg) {
    if (success) {
        auth_complete_ = true;
        fb_state_ = FirebirdProtocolState::AUTHENTICATED;
        // Accept is sent in handleConnect
    } else {
        sendErrorResponse(conn, firebird::ErrorCode::isc_login, error_msg);
    }
    return sendBuffer(conn);
}

core::Status FirebirdAdapter::sendQueryResult(network::Connection* conn,
                                               const ResultContext& result) {
    if (result.has_error) {
        int32_t code = result.error_code
            ? static_cast<int32_t>(result.error_code)
            : firebird::ErrorCode::isc_dsql_error;
        sendErrorResponse(conn, code, result.error_message, result.sqlstate);
        return core::Status::OK;
    }

    // For SELECT, results are sent via fetch
    // For DML, send response with affected rows
    if (result.columns.empty()) {
        std::vector<uint8_t> data;
        sendResponse(conn, 0, static_cast<uint64_t>(result.rows_affected), data);
    }

    return core::Status::OK;
}

core::Status FirebirdAdapter::sendProtocolError(network::Connection* conn,
                                                 uint32_t error_code,
                                                 const std::string& /*sqlstate*/,
                                                 const std::string& message,
                                                 const std::string& /*detail*/,
                                                 const std::string& /*hint*/) {
    int32_t fb_code = error_code == 0
        ? firebird::ErrorCode::isc_dsql_error
        : static_cast<int32_t>(error_code);
    sendErrorResponse(conn, fb_code, message);
    return core::Status::OK;
}

core::Status FirebirdAdapter::ensureFirebirdSystemTables(core::ErrorContext* ctx) {
    if (!engineDatabase()) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "Database not initialized");
        return core::Status::INVALID_ARGUMENT;
    }

    auto* catalog = engineDatabase()->catalog_manager();
    if (!catalog) {
        SET_ERROR_CONTEXT(ctx, core::Status::INVALID_ARGUMENT, "Catalog manager not available");
        return core::Status::INVALID_ARGUMENT;
    }

    FirebirdDatabaseSpec spec = parseFirebirdDatabaseSpec(database_path_.string());
    std::string server = spec.server.empty() ? "localhost" : spec.server;
    std::string db_name = deriveFirebirdDatabaseName(spec.file_path);
    if (db_name.empty()) {
        db_name = "default";
    }
    auto path_components = splitFirebirdPathComponents(spec.file_path);
    auto schema_name = buildEmulatedFirebirdSchemaPath(server, path_components, db_name);
    firebird_schema_name_ = schema_name;

    core::CatalogManager::SchemaInfo fb_schema;
    auto status = catalog->getSchema(schema_name, fb_schema, ctx);
    if (status != core::Status::OK) {
        if (status != core::Status::INVALID_ARGUMENT && status != core::Status::NOT_FOUND) {
            return status;
        }
        core::ID schema_id;
        status = catalog->createSchemaPath(schema_name,
                                           core::CatalogManager::SchemaType::REMOTE_EMULATED,
                                           schema_id,
                                           ctx);
        if (status != core::Status::OK) {
            return status;
        }
        status = catalog->getSchema(schema_id, fb_schema, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }
    firebird_schema_id_ = fb_schema.schema_id;

    core::CatalogManager::TableInfo table_info;
    status = catalog->getTable(fb_schema.schema_id, "RDB$DATABASE", table_info, ctx);
    if (status == core::Status::OK) {
        // Ensure minimal catalog views exist alongside the table
    } else {
        if (status != core::Status::INVALID_ARGUMENT) {
            return status;
        }

        std::vector<core::CatalogManager::ColumnInfo> columns;
        core::CatalogManager::ColumnInfo col{};
        col.column_name = "DUMMY";
        col.data_type = static_cast<uint16_t>(core::DataType::INT32);
        col.nullable = true;
        columns.push_back(col);

        core::ID table_id;
        status = catalog->createTable(fb_schema.schema_id, "RDB$DATABASE", columns, table_id, 0, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }

    auto ensure_view = [&](const std::string& name,
                           const std::string& definition,
                           const std::vector<std::string>& column_names = {}) -> core::Status {
        core::CatalogManager::ViewInfo view_info;
        auto s = catalog->getView(fb_schema.schema_id, name, view_info, ctx);
        if (s == core::Status::OK) {
            return core::Status::OK;
        }
        if (s != core::Status::INVALID_ARGUMENT && s != core::Status::NOT_FOUND) {
            return s;
        }
        return catalog->createView(fb_schema.schema_id, name, definition, false,
                                   false, false, column_names, core::ID{}, ctx);
    };

    auto escape_literal = [](const std::string& in) {
        std::string out;
        out.reserve(in.size() + 8);
        for (char c : in) {
            out.push_back(c);
            if (c == '\'') out.push_back('\'');
        }
        return out;
    };

    std::vector<core::CatalogManager::TableInfo> tables;
    catalog->listTables(fb_schema.schema_id, tables, ctx);

    std::unordered_map<core::ID, core::CatalogManager::TableInfo, core::IDHash> table_by_id;
    std::unordered_map<std::string, std::string> table_by_name;  // lowercase -> canonical
    for (const auto& t : tables) {
        table_by_id.emplace(t.table_id, t);
        std::string key = t.table_name;
        std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        table_by_name.emplace(key, t.table_name);
    }

    std::unordered_map<core::ID, std::vector<core::CatalogManager::ColumnInfo>, core::IDHash> columns_by_table;
    for (const auto& t : tables) {
        std::vector<core::CatalogManager::ColumnInfo> cols;
        catalog->getColumns(t.table_id, cols, ctx);
        columns_by_table.emplace(t.table_id, std::move(cols));
    }

    // Build RDB$RELATIONS
    std::string rel_sql;
    if (tables.empty()) {
        rel_sql = "SELECT NULL AS RDB$RELATION_NAME, NULL AS RDB$SYSTEM_FLAG, NULL AS RDB$VIEW_BLR WHERE 1 = 0";
    } else {
        std::ostringstream ss;
        bool first = true;
        for (const auto& t : tables) {
            if (!first) ss << " UNION ALL ";
            ss << "SELECT '" << escape_literal(t.table_name) << "' AS RDB$RELATION_NAME, 0 AS RDB$SYSTEM_FLAG, NULL AS RDB$VIEW_BLR";
            first = false;
        }
        rel_sql = ss.str();
    }
    ensure_view("RDB$RELATIONS", rel_sql);

    // Build RDB$RELATION_FIELDS and RDB$FIELDS
    std::string rel_fields_sql;
    std::string fields_sql;
    {
        std::ostringstream rf, f;
        bool rf_first = true;
        bool f_first = true;
        std::unordered_set<std::string> seen_fields;
        for (const auto& t : tables) {
            auto it = columns_by_table.find(t.table_id);
            if (it == columns_by_table.end()) {
                continue;
            }
            const auto& cols = it->second;
            for (const auto& col : cols) {
                if (!rf_first) rf << " UNION ALL ";
                rf << "SELECT '" << escape_literal(col.column_name) << "' AS RDB$FIELD_NAME, '"
                   << escape_literal(t.table_name) << "' AS RDB$RELATION_NAME, "
                   << static_cast<int>(col.ordinal) << " AS RDB$FIELD_POSITION, "
                   << (col.nullable ? 0 : 1) << " AS RDB$NULL_FLAG";
                rf_first = false;

                std::string field_key = col.column_name;
                if (seen_fields.insert(field_key).second) {
                    if (!f_first) f << " UNION ALL ";
                    uint32_t length = col.type_precision ? col.type_precision : (col.max_length ? col.max_length : 0);
                    f << "SELECT '" << escape_literal(col.column_name) << "' AS RDB$FIELD_NAME, "
                      << static_cast<int>(col.data_type) << " AS RDB$FIELD_TYPE, "
                      << "0 AS RDB$FIELD_SUB_TYPE, "
                      << length << " AS RDB$FIELD_LENGTH, "
                      << length << " AS RDB$SEGMENT_LENGTH, "
                      << (col.nullable ? 0 : 1) << " AS RDB$NULL_FLAG, "
                      << (col.default_value.empty() ? "NULL" : ("'" + escape_literal(col.default_value) + "'")) << " AS RDB$DEFAULT_SOURCE";
                    f_first = false;
                }
            }
        }
        if (rf_first) {
            rel_fields_sql = "SELECT NULL AS RDB$FIELD_NAME, NULL AS RDB$RELATION_NAME, NULL AS RDB$FIELD_POSITION, NULL AS RDB$NULL_FLAG WHERE 1 = 0";
        } else {
            rel_fields_sql = rf.str();
        }
        if (f_first) {
            fields_sql = "SELECT NULL AS RDB$FIELD_NAME, NULL AS RDB$FIELD_TYPE, NULL AS RDB$FIELD_SUB_TYPE, NULL AS RDB$FIELD_LENGTH, NULL AS RDB$SEGMENT_LENGTH, NULL AS RDB$NULL_FLAG, NULL AS RDB$DEFAULT_SOURCE WHERE 1 = 0";
        } else {
            fields_sql = f.str();
        }
    }

    ensure_view("RDB$DATABASE", "SELECT DUMMY FROM RDB$DATABASE");
    // Build RDB$INDICES and RDB$INDEX_SEGMENTS
    std::string indices_sql;
    std::string index_segments_sql;
    {
        std::ostringstream idx_ss;
        std::ostringstream seg_ss;
        bool idx_first = true;
        bool seg_first = true;
        for (const auto& t : tables) {
            std::vector<core::CatalogManager::IndexInfo> indexes;
            auto s = catalog->listIndexesForTable(t.table_id, indexes, ctx);
            if (s != core::Status::OK) {
                continue;
            }
            auto col_it = columns_by_table.find(t.table_id);
            for (const auto& idx : indexes) {
                if (!idx_first) idx_ss << " UNION ALL ";
                idx_ss << "SELECT '" << escape_literal(idx.index_name) << "' AS RDB$INDEX_NAME, '"
                       << escape_literal(t.table_name) << "' AS RDB$RELATION_NAME, "
                       << (idx.is_unique ? 1 : 0) << " AS RDB$UNIQUE_FLAG, 0 AS RDB$INDEX_TYPE";
                idx_first = false;

                if (col_it == columns_by_table.end()) {
                    continue;
                }
                for (size_t pos = 0; pos < idx.column_ids.size(); ++pos) {
                    const auto& col_id = idx.column_ids[pos];
                    std::string col_name = "COLUMN_" + std::to_string(pos);
                    for (const auto& col : col_it->second) {
                        if (col.column_id == col_id) {
                            col_name = col.column_name;
                            break;
                        }
                    }
                    if (!seg_first) seg_ss << " UNION ALL ";
                    seg_ss << "SELECT '" << escape_literal(idx.index_name) << "' AS RDB$INDEX_NAME, '"
                           << escape_literal(col_name) << "' AS RDB$FIELD_NAME, "
                           << static_cast<int>(pos) << " AS RDB$FIELD_POSITION";
                    seg_first = false;
                }
            }
        }

        if (idx_first) {
            indices_sql = "SELECT NULL AS RDB$INDEX_NAME, NULL AS RDB$RELATION_NAME, NULL AS RDB$UNIQUE_FLAG, NULL AS RDB$INDEX_TYPE WHERE 1 = 0";
        } else {
            indices_sql = idx_ss.str();
        }

        if (seg_first) {
            index_segments_sql = "SELECT NULL AS RDB$INDEX_NAME, NULL AS RDB$FIELD_NAME, NULL AS RDB$FIELD_POSITION WHERE 1 = 0";
        } else {
            index_segments_sql = seg_ss.str();
        }
    }

    // Build RDB$RELATION_CONSTRAINTS, RDB$CHECK_CONSTRAINTS, RDB$REF_CONSTRAINTS
    std::string relation_constraints_sql;
    std::string check_constraints_sql;
    std::string ref_constraints_sql;
    {
        std::ostringstream rc, cc, rf;
        bool rc_first = true;
        bool cc_first = true;
        bool rf_first = true;

        auto constraint_type = [](core::CatalogManager::ConstraintType type) -> std::string {
            switch (type) {
                case core::CatalogManager::ConstraintType::PRIMARY_KEY: return "PRIMARY KEY";
                case core::CatalogManager::ConstraintType::UNIQUE: return "UNIQUE";
                case core::CatalogManager::ConstraintType::FOREIGN_KEY: return "FOREIGN KEY";
                case core::CatalogManager::ConstraintType::CHECK: return "CHECK";
                case core::CatalogManager::ConstraintType::NOT_NULL: return "NOT NULL";
                case core::CatalogManager::ConstraintType::EXCLUSION: return "EXCLUSION";
            }
            return "UNKNOWN";
        };

        auto fk_action = [](core::CatalogManager::FKAction action) -> std::string {
            switch (action) {
                case core::CatalogManager::FKAction::NO_ACTION: return "NO ACTION";
                case core::CatalogManager::FKAction::RESTRICT: return "RESTRICT";
                case core::CatalogManager::FKAction::CASCADE: return "CASCADE";
                case core::CatalogManager::FKAction::SET_NULL: return "SET NULL";
                case core::CatalogManager::FKAction::SET_DEFAULT: return "SET DEFAULT";
            }
            return "NO ACTION";
        };

        auto fk_match = [](core::CatalogManager::FKMatchType match) -> std::string {
            switch (match) {
                case core::CatalogManager::FKMatchType::FULL: return "FULL";
                case core::CatalogManager::FKMatchType::PARTIAL: return "PARTIAL";
                case core::CatalogManager::FKMatchType::SIMPLE:
                default: return "SIMPLE";
            }
        };

        auto find_matching_constraint = [&](const core::CatalogManager::ConstraintInfo& fk) -> std::string {
            if (fk.referenced_table_id == core::ID{}) {
                return {};
            }
            std::vector<core::CatalogManager::ConstraintInfo> target;
            if (catalog->getConstraintsByType(fk.referenced_table_id,
                                              core::CatalogManager::ConstraintType::PRIMARY_KEY,
                                              target, ctx) != core::Status::OK) {
                target.clear();
            }
            if (target.empty()) {
                catalog->getConstraintsByType(fk.referenced_table_id,
                                              core::CatalogManager::ConstraintType::UNIQUE,
                                              target, ctx);
            }
            for (const auto& c : target) {
                if (c.column_names == fk.referenced_columns) {
                    return c.constraint_name;
                }
            }
            return {};
        };

        for (const auto& t : tables) {
            std::vector<core::CatalogManager::ConstraintInfo> constraints;
            auto s = catalog->getConstraintsForTable(t.table_id, constraints, ctx);
            if (s != core::Status::OK) {
                continue;
            }

            for (const auto& c : constraints) {
                if (!rc_first) rc << " UNION ALL ";
                rc << "SELECT '" << escape_literal(c.constraint_name) << "' AS RDB$CONSTRAINT_NAME, '"
                   << constraint_type(c.constraint_type) << "' AS RDB$CONSTRAINT_TYPE, '"
                   << escape_literal(t.table_name) << "' AS RDB$RELATION_NAME, "
                   << "'" << escape_literal(c.constraint_name) << "' AS RDB$INDEX_NAME, "
                   << (c.is_deferrable ? 1 : 0) << " AS RDB$DEFERRABLE, "
                   << (c.initially_deferred ? 1 : 0) << " AS RDB$INITIALLY_DEFERRED";
                rc_first = false;

                if (c.constraint_type == core::CatalogManager::ConstraintType::CHECK) {
                    if (!cc_first) cc << " UNION ALL ";
                    cc << "SELECT '" << escape_literal(c.constraint_name) << "' AS RDB$CONSTRAINT_NAME, "
                       << "'" << escape_literal(c.constraint_name) << "' AS RDB$TRIGGER_NAME";
                    cc_first = false;
                } else if (c.constraint_type == core::CatalogManager::ConstraintType::FOREIGN_KEY) {
                    auto ref = find_matching_constraint(c);
                    if (!rf_first) rf << " UNION ALL ";
                    rf << "SELECT '" << escape_literal(c.constraint_name) << "' AS RDB$CONSTRAINT_NAME, "
                       << (ref.empty() ? "NULL" : ("'" + escape_literal(ref) + "'")) << " AS RDB$CONST_NAME_UQ, "
                       << "'" << fk_match(c.match_type) << "' AS RDB$MATCH_OPTION, "
                       << "'" << fk_action(c.on_update) << "' AS RDB$UPDATE_RULE, "
                       << "'" << fk_action(c.on_delete) << "' AS RDB$DELETE_RULE";
                    rf_first = false;
                }
            }
        }

        if (rc_first) {
            relation_constraints_sql = "SELECT NULL AS RDB$CONSTRAINT_NAME, NULL AS RDB$CONSTRAINT_TYPE, NULL AS RDB$RELATION_NAME, NULL AS RDB$INDEX_NAME, NULL AS RDB$DEFERRABLE, NULL AS RDB$INITIALLY_DEFERRED WHERE 1 = 0";
        } else {
            relation_constraints_sql = rc.str();
        }

        if (cc_first) {
            check_constraints_sql = "SELECT NULL AS RDB$CONSTRAINT_NAME, NULL AS RDB$TRIGGER_NAME WHERE 1 = 0";
        } else {
            check_constraints_sql = cc.str();
        }

        if (rf_first) {
            ref_constraints_sql = "SELECT NULL AS RDB$CONSTRAINT_NAME, NULL AS RDB$CONST_NAME_UQ, NULL AS RDB$MATCH_OPTION, NULL AS RDB$UPDATE_RULE, NULL AS RDB$DELETE_RULE WHERE 1 = 0";
        } else {
            ref_constraints_sql = rf.str();
        }
    }

    ensure_view("RDB$FIELDS", fields_sql);
    ensure_view("RDB$FORMATS", "SELECT NULL AS RDB$FORMAT, NULL AS RDB$RELATION_ID WHERE 1 = 0");
    ensure_view("RDB$TYPES", "SELECT NULL AS RDB$TYPE, NULL AS RDB$FIELD_NAME WHERE 1 = 0");
    ensure_view("RDB$RELATION_FIELDS", rel_fields_sql);
    ensure_view("RDB$INDICES", indices_sql,
                {"RDB$INDEX_NAME", "RDB$RELATION_NAME", "RDB$UNIQUE_FLAG", "RDB$INDEX_TYPE"});
    ensure_view("RDB$INDEX_SEGMENTS", index_segments_sql,
                {"RDB$INDEX_NAME", "RDB$FIELD_NAME", "RDB$FIELD_POSITION"});
    ensure_view("RDB$RELATION_CONSTRAINTS", relation_constraints_sql,
                {"RDB$CONSTRAINT_NAME", "RDB$CONSTRAINT_TYPE", "RDB$RELATION_NAME",
                 "RDB$INDEX_NAME", "RDB$DEFERRABLE", "RDB$INITIALLY_DEFERRED"});
    ensure_view("RDB$CHECK_CONSTRAINTS", check_constraints_sql);
    ensure_view("RDB$REF_CONSTRAINTS", ref_constraints_sql);

    // Build RDB$TRIGGERS
    std::string triggers_sql;
    {
        std::ostringstream tr;
        bool tr_first = true;
        for (const auto& t : tables) {
            std::vector<core::CatalogManager::TriggerInfo> triggers;
            auto s = catalog->listAllTriggersForTable(t.table_id, triggers, ctx);
            if (s != core::Status::OK) {
                continue;
            }
            int32_t seq = 0;
            for (const auto& trig : triggers) {
                if (!trig.enabled) {
                    continue;
                }
                auto emit_trigger = [&](int32_t trigger_type) {
                    if (!tr_first) tr << " UNION ALL ";
                    tr << "SELECT '" << escape_literal(trig.trigger_name) << "' AS RDB$TRIGGER_NAME, '"
                       << escape_literal(t.table_name) << "' AS RDB$RELATION_NAME, "
                       << seq++ << " AS RDB$TRIGGER_SEQUENCE, "
                       << trigger_type << " AS RDB$TRIGGER_TYPE";
                    tr_first = false;
                };

                auto has_event = [&](core::CatalogManager::TriggerEvent event) {
                    return (trig.event_mask &
                            (1u << static_cast<uint8_t>(event))) != 0;
                };

                if (trig.timing == core::CatalogManager::TriggerTiming::BEFORE) {
                    if (has_event(core::CatalogManager::TriggerEvent::INSERT)) {
                        emit_trigger(1);
                    }
                    if (has_event(core::CatalogManager::TriggerEvent::UPDATE)) {
                        emit_trigger(3);
                    }
                    if (has_event(core::CatalogManager::TriggerEvent::DELETE)) {
                        emit_trigger(5);
                    }
                } else if (trig.timing == core::CatalogManager::TriggerTiming::AFTER) {
                    if (has_event(core::CatalogManager::TriggerEvent::INSERT)) {
                        emit_trigger(2);
                    }
                    if (has_event(core::CatalogManager::TriggerEvent::UPDATE)) {
                        emit_trigger(4);
                    }
                    if (has_event(core::CatalogManager::TriggerEvent::DELETE)) {
                        emit_trigger(6);
                    }
                }
            }
        }
        if (tr_first) {
            triggers_sql = "SELECT NULL AS RDB$TRIGGER_NAME, NULL AS RDB$RELATION_NAME, NULL AS RDB$TRIGGER_SEQUENCE, NULL AS RDB$TRIGGER_TYPE WHERE 1 = 0";
        } else {
            triggers_sql = tr.str();
        }
    }
    ensure_view("RDB$TRIGGERS", triggers_sql);

    // Build RDB$PROCEDURES and RDB$PROCEDURE_PARAMETERS
    std::string procedures_sql;
    std::string procedure_params_sql;
    {
        std::vector<core::CatalogManager::ProcedureInfo> procedures;
        auto s = catalog->listProcedures(procedures, ctx);
        if (s != core::Status::OK) {
            procedures.clear();
        }
        std::ostringstream proc_ss;
        std::ostringstream param_ss;
        bool proc_first = true;
        bool param_first = true;
        for (const auto& proc : procedures) {
            uint32_t output_count = 0;
            for (const auto& p : proc.parameters) {
                if (p.mode == core::CatalogManager::ParameterMode::OUT ||
                    p.mode == core::CatalogManager::ParameterMode::INOUT) {
                    ++output_count;
                }
            }
            if (!proc_first) proc_ss << " UNION ALL ";
            proc_ss << "SELECT '" << escape_literal(proc.name) << "' AS RDB$PROCEDURE_NAME, "
                    << output_count << " AS RDB$PROCEDURE_OUTPUTS";
            proc_first = false;

            for (size_t i = 0; i < proc.parameters.size(); ++i) {
                const auto& p = proc.parameters[i];
                int param_type = 0;
                switch (p.mode) {
                    case core::CatalogManager::ParameterMode::IN: param_type = 0; break;
                    case core::CatalogManager::ParameterMode::OUT: param_type = 1; break;
                    case core::CatalogManager::ParameterMode::INOUT: param_type = 2; break;
                }
                if (!param_first) param_ss << " UNION ALL ";
                param_ss << "SELECT '" << escape_literal(proc.name) << "' AS RDB$PROCEDURE_NAME, '"
                         << escape_literal(p.name) << "' AS RDB$PARAMETER_NAME, "
                         << param_type << " AS RDB$PARAMETER_TYPE, "
                         << "'" << escape_literal(p.name) << "' AS RDB$FIELD_SOURCE, "
                         << static_cast<int>(i) << " AS RDB$PARAMETER_NUMBER";
                param_first = false;
            }
        }
        if (proc_first) {
            procedures_sql = "SELECT NULL AS RDB$PROCEDURE_NAME, NULL AS RDB$PROCEDURE_OUTPUTS WHERE 1 = 0";
        } else {
            procedures_sql = proc_ss.str();
        }
        if (param_first) {
            procedure_params_sql = "SELECT NULL AS RDB$PROCEDURE_NAME, NULL AS RDB$PARAMETER_NAME, NULL AS RDB$PARAMETER_TYPE, NULL AS RDB$FIELD_SOURCE, NULL AS RDB$PARAMETER_NUMBER WHERE 1 = 0";
        } else {
            procedure_params_sql = param_ss.str();
        }
    }

    ensure_view("RDB$PROCEDURES", procedures_sql);
    ensure_view("RDB$PROCEDURE_PARAMETERS", procedure_params_sql);

    // Build RDB$VIEW_RELATIONS (map views to their base relations if known; otherwise list view names)
    auto to_lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        return s;
    };

    auto extract_from_clause = [&](const std::string& definition) -> std::vector<std::string> {
        std::vector<std::string> result;
        if (definition.empty()) {
            return result;
        }
        std::string def_lower = to_lower(definition);
        auto pos = def_lower.find(" from ");
        if (pos == std::string::npos) {
            return result;
        }
        pos += 6;  // move past " from "
        while (pos < def_lower.size()) {
            while (pos < def_lower.size() && std::isspace(static_cast<unsigned char>(def_lower[pos]))) {
                ++pos;
            }
            if (pos >= def_lower.size()) break;
            size_t start = pos;
            while (pos < def_lower.size()) {
                char c = def_lower[pos];
                if (std::isspace(static_cast<unsigned char>(c)) || c == ',' || c == ';') {
                    break;
                }
                ++pos;
            }
            if (pos > start) {
                result.emplace_back(definition.substr(start, pos - start));
            }
            while (pos < def_lower.size() && def_lower[pos] != ',' && def_lower[pos] != ';') {
                ++pos;
            }
            if (pos < def_lower.size() && (def_lower[pos] == ',' || def_lower[pos] == ';')) {
                ++pos;
            } else {
                break;
            }
        }
        return result;
    };

    std::string view_relations_sql;
    {
        std::vector<core::CatalogManager::ViewInfo> views;
        auto s = catalog->listViewsForSchema(fb_schema.schema_id, views, ctx);
        if (s != core::Status::OK) {
            views.clear();
        }
        std::ostringstream vr;
        bool vr_first = true;
        for (const auto& v : views) {
            // Attempt to resolve base relations from dependency graph
            std::vector<core::CatalogManager::DependencyInfo> deps;
            if (catalog->getDependenciesFor(v.view_id, deps, ctx) != core::Status::OK) {
                deps.clear();
            }
            size_t emitted = 0;
            for (const auto& dep : deps) {
                if (dep.dependent_type != core::CatalogManager::ObjectType::VIEW ||
                    dep.referenced_type != core::CatalogManager::ObjectType::TABLE) {
                    continue;
                }
                std::string rel_name;
                auto t_it = table_by_id.find(dep.referenced_object_id);
                if (t_it != table_by_id.end()) {
                    rel_name = t_it->second.table_name;
                }
                if (!vr_first) vr << " UNION ALL ";
                vr << "SELECT '" << escape_literal(v.name) << "' AS RDB$VIEW_NAME, "
                   << (rel_name.empty() ? "NULL" : ("'" + escape_literal(rel_name) + "'"))
                   << " AS RDB$RELATION_NAME";
                vr_first = false;
                ++emitted;
            }
            if (emitted == 0 && !v.definition.empty()) {
                auto bases = extract_from_clause(v.definition);
                for (const auto& base : bases) {
                    std::string rel_name;
                    auto name_key = to_lower(base);
                    auto by_name = table_by_name.find(name_key);
                    if (by_name != table_by_name.end()) {
                        rel_name = by_name->second;
                    } else {
                        rel_name = base;
                    }
                    if (!vr_first) vr << " UNION ALL ";
                    vr << "SELECT '" << escape_literal(v.name) << "' AS RDB$VIEW_NAME, '"
                       << escape_literal(rel_name) << "' AS RDB$RELATION_NAME";
                    vr_first = false;
                    ++emitted;
                }
            }
            if (emitted == 0) {
                if (!vr_first) vr << " UNION ALL ";
                vr << "SELECT '" << escape_literal(v.name) << "' AS RDB$VIEW_NAME, NULL AS RDB$RELATION_NAME";
                vr_first = false;
            }
        }
        if (vr_first) {
            view_relations_sql = "SELECT NULL AS RDB$VIEW_NAME, NULL AS RDB$RELATION_NAME WHERE 1 = 0";
        } else {
            view_relations_sql = vr.str();
        }
    }
    ensure_view("RDB$VIEW_RELATIONS", view_relations_sql,
                {"RDB$VIEW_NAME", "RDB$RELATION_NAME"});

    if (connection_ctx_) {
        core::ErrorContext commit_ctx;
        auto commit_status = connection_ctx_->commit(&commit_ctx);
        if (commit_status != core::Status::OK &&
            commit_status != core::Status::NO_ACTIVE_TRANSACTION) {
            if (ctx && !commit_ctx.message.empty()) {
                ctx->set(commit_status, commit_ctx.message.c_str(),
                         __FILE__, __LINE__, __func__);
            }
            return commit_status;
        }
    }

    return core::Status::OK;
}

core::Status FirebirdAdapter::compileQuery(const std::string& sql,
                                           std::vector<uint8_t>& bytecode_out,
                                           std::string& error_out) {
    core::ErrorContext ctx;
    auto status = ensureEngine(&ctx);
    if (status != core::Status::OK) {
        error_out = ctx.message;
        return status;
    }

    status = ensureFirebirdSystemTables(&ctx);
    if (status != core::Status::OK) {
        error_out = ctx.message.empty() ? "Failed to initialize Firebird system tables" : ctx.message;
        return status;
    }

    sblr::FirebirdQueryCompiler compiler(engineDatabase());
    if (firebird_schema_id_ != core::ID{}) {
        compiler.setCurrentSchema(firebird_schema_id_);
    }
    auto result = compiler.compile(sql);
    if (!result.success()) {
        error_out = result.errors().empty() ? "Compilation failed" : result.errors().front();
        return core::Status::INVALID_ARGUMENT;
    }
    bytecode_out = result.bytecode();
    return core::Status::OK;
}

core::Status FirebirdAdapter::ensureRemoteClient(core::ErrorContext* ctx) {
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
    if (!username_.empty()) {
        client_config_.username = username_;
        client_config_.password = remote_password_;
    } else {
        client_config_.username = "BOOTSTRAP";
        client_config_.password.clear();
    }

    client_ = std::make_unique<client::Connection>();
    auto status = client_->connect(client_config_, ctx);
    if (status != core::Status::OK) {
        client_.reset();
    }
    if (status == core::Status::OK && !firebird_schema_name_.empty()) {
        auto escape_literal = [](const std::string& in) {
            std::string out;
            out.reserve(in.size());
            for (char ch : in) {
                if (ch == '\'') {
                    out.push_back('\'');
                }
                out.push_back(ch);
            }
            return out;
        };
        auto set_path_sql = "SET search_path TO '" + escape_literal(firebird_schema_name_) + "'";
        auto set_status = client_->execute(set_path_sql, nullptr, ctx);
        if (set_status != core::Status::OK) {
            client_->disconnect();
            client_.reset();
            return set_status;
        }
    }
    return status;
}

core::Status FirebirdAdapter::executeRemoteQuery(const QueryContext& query,
                                                 ResultContext& result,
                                                 core::ErrorContext* ctx) {
    auto status = ensureEngine(ctx);
    if (status != core::Status::OK) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(status);
        result.error_message = ctx ? ctx->message : "Failed to initialize engine";
        return status;
    }

    status = ensureFirebirdSystemTables(ctx);
    if (status != core::Status::OK) {
        result.has_error = true;
        result.error_code = static_cast<uint32_t>(status);
        result.error_message = ctx ? ctx->message : "Failed to initialize Firebird catalogs";
        return status;
    }

    status = ensureRemoteClient(ctx);
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
    if (!rs.getCommandTag().empty()) {
        result.command_tag = rs.getCommandTag();
    }
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
// Operation Handling
// ============================================================================

core::Status FirebirdAdapter::handleConnect(network::Connection* conn) {
    // Parse connect packet
    size_t offset = 4;  // Skip opcode

    if (current_packet_.size() < 28) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_unavailable, "Invalid connect packet");
        return sendBuffer(conn);
    }

    // Operation (should be op_attach or op_create)
    uint32_t operation = readUInt32(current_packet_.data() + offset);
    offset += 4;
    (void)operation;

    // Version
    uint32_t version = readUInt32(current_packet_.data() + offset);
    offset += 4;
    (void)version;

    // Architecture
    uint32_t arch = readUInt32(current_packet_.data() + offset);
    offset += 4;
    (void)arch;

    // Min/max protocol type
    uint32_t min_type = readUInt32(current_packet_.data() + offset);
    offset += 4;
    (void)min_type;

    uint32_t max_type = readUInt32(current_packet_.data() + offset);
    offset += 4;
    (void)max_type;

    // Skip preference bitmap and protocols for now
    // In a full implementation, we'd negotiate protocol version

    // For testing, accept with default protocol
    client_protocol_version_ = firebird::DEFAULT_PROTOCOL_VERSION;
    fb_state_ = FirebirdProtocolState::CONNECT_RECEIVED;

    // Send accept (or accept_data for auth)
    if (config_.require_authentication) {
        // Request authentication
        std::vector<uint8_t> auth_data;
        std::vector<uint8_t> keys;
        sendAcceptData(conn, firebird::DEFAULT_PROTOCOL_VERSION,
                      firebird::ARCH_GENERIC, 1, auth_data,
                      firebird::AUTH_PLUGIN_SRP256, false, keys);
        fb_state_ = FirebirdProtocolState::AUTH_CONTINUE;
    } else {
        // Trust authentication
        sendAccept(conn, firebird::DEFAULT_PROTOCOL_VERSION,
                  firebird::ARCH_GENERIC, 1);
        auth_complete_ = true;
        fb_state_ = FirebirdProtocolState::AUTHENTICATED;
    }

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleAttach(network::Connection* conn) {
    size_t offset = 4;  // Skip opcode

    // Database handle (0 for new attach)
    uint32_t db_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;
    (void)db_handle;

    // Database path
    std::string db_path = readString(current_packet_.data(), offset, current_packet_.size());

    // DPB (Database Parameter Buffer)
    std::vector<uint8_t> dpb = readBuffer(current_packet_.data(), offset, current_packet_.size());
    parseDpb(dpb);

    database_name_ = db_path;
    client_.reset();
    client_config_.database_name = database_name_;
    if (!config_.engine_endpoint.empty()) {
        client_config_.ipc_method = server::IPCMethod::AUTO;
        client_config_.socket_path = config_.engine_endpoint;
    } else {
        client_config_.ipc_method = server::IPCMethod::UNIX_SOCKET;
        client_config_.socket_path = server::getIPCPath(client_config_.database_name,
                                                       client_config_.ipc_method);
    }
    client_config_.username = username_.empty() ? "BOOTSTRAP" : username_;
    client_config_.auto_start_server = false;

    // Assign database handle
    db_handle_ = next_db_handle_++;
    active_transactions_.clear();
    current_transaction_ = 0;
    fb_state_ = FirebirdProtocolState::ATTACHED;

    // Send response
    std::vector<uint8_t> response_data;
    sendResponse(conn, db_handle_, 0, response_data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleDetach(network::Connection* conn) {
    size_t offset = 4;

    uint32_t db_handle = readUInt32(current_packet_.data() + offset);

    if (db_handle != db_handle_) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "Invalid database handle");
        return sendBuffer(conn);
    }

    if (client_) {
        client_->disconnect();
        client_.reset();
    }
    db_handle_ = 0;
    active_transactions_.clear();
    fb_state_ = FirebirdProtocolState::AUTHENTICATED;
    current_transaction_ = 0;

    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleCreateDatabase(network::Connection* conn) {
    // Similar to attach but creates the database
    return handleAttach(conn);
}

core::Status FirebirdAdapter::handleDropDatabase(network::Connection* conn) {
    size_t offset = 4;

    uint32_t db_handle = readUInt32(current_packet_.data() + offset);

    if (db_handle != db_handle_) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "Invalid database handle");
        return sendBuffer(conn);
    }

    if (database_name_.empty()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "No database name available");
        return sendBuffer(conn);
    }

    auto escape_literal = [](const std::string& in) {
        std::string out;
        out.reserve(in.size());
        for (char ch : in) {
            if (ch == '\'') {
                out.push_back('\'');
            }
            out.push_back(ch);
        }
        return out;
    };

    QueryContext ctx;
    ctx.query = "DROP DATABASE '" + escape_literal(database_name_) + "'";

    ResultContext result;
    core::ErrorContext err;
    auto status = executeRemoteQuery(ctx, result, &err);
    if (status != core::Status::OK || result.has_error) {
        std::string message = result.error_message.empty() ? err.message : result.error_message;
        int32_t code = result.error_code ? static_cast<int32_t>(result.error_code)
                                         : mapStatusToFirebird(status);
        sendErrorResponse(conn, code, message.empty() ? "DROP DATABASE failed" : message, result.sqlstate);
        return sendBuffer(conn);
    }

    if (client_) {
        client_->disconnect();
        client_.reset();
    }
    db_handle_ = 0;
    active_transactions_.clear();
    fb_state_ = FirebirdProtocolState::AUTHENTICATED;
    database_name_.clear();
    firebird_schema_name_.clear();
    firebird_schema_id_ = core::ID{};

    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleTransaction(network::Connection* conn) {
    size_t offset = 4;

    uint32_t db_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    if (db_handle != db_handle_) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "Invalid database handle");
        return sendBuffer(conn);
    }

    // TPB (Transaction Parameter Buffer)
    std::vector<uint8_t> tpb = readBuffer(current_packet_.data(), offset, current_packet_.size());
    (void)tpb;  // Parse if needed

    if (!active_transactions_.empty()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_tr_handle, "Transaction already active");
        return sendBuffer(conn);
    }

    core::ErrorContext ctx;
    auto status = ensureRemoteClient(&ctx);
    if (status != core::Status::OK) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_unavailable, "Failed to connect engine");
        return sendBuffer(conn);
    }

    status = client_->beginTransaction(&ctx);
    if (status != core::Status::OK) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_unavailable, "Failed to start transaction");
        return sendBuffer(conn);
    }

    current_transaction_ = next_tr_handle_++;
    active_transactions_.insert(current_transaction_);

    std::vector<uint8_t> data;
    sendResponse(conn, current_transaction_, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleCommit(network::Connection* conn) {
    size_t offset = 4;

    uint32_t tr_handle = readUInt32(current_packet_.data() + offset);

    if (active_transactions_.find(tr_handle) == active_transactions_.end()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_tr_handle, "Invalid transaction handle");
        return sendBuffer(conn);
    }

    core::ErrorContext ctx;
    auto status = ensureRemoteClient(&ctx);
    if (status != core::Status::OK) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_unavailable, "Engine unavailable");
        return sendBuffer(conn);
    }

    status = client_->commit(&ctx);
    if (status != core::Status::OK) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_unavailable, "Commit failed");
        return sendBuffer(conn);
    }

    active_transactions_.erase(tr_handle);
    current_transaction_ = 0;

    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleRollback(network::Connection* conn) {
    size_t offset = 4;

    uint32_t tr_handle = readUInt32(current_packet_.data() + offset);

    if (active_transactions_.find(tr_handle) == active_transactions_.end()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_tr_handle, "Invalid transaction handle");
        return sendBuffer(conn);
    }

    core::ErrorContext ctx;
    auto status = ensureRemoteClient(&ctx);
    if (status != core::Status::OK) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_unavailable, "Engine unavailable");
        return sendBuffer(conn);
    }

    status = client_->rollback(&ctx);
    if (status != core::Status::OK) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_unavailable, "Rollback failed");
        return sendBuffer(conn);
    }

    active_transactions_.erase(tr_handle);
    current_transaction_ = 0;

    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleCommitRetaining(network::Connection* conn) {
    // Commit but keep transaction open
    size_t offset = 4;

    uint32_t tr_handle = readUInt32(current_packet_.data() + offset);

    if (active_transactions_.find(tr_handle) == active_transactions_.end()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_tr_handle, "Invalid transaction handle");
        return sendBuffer(conn);
    }

    core::ErrorContext ctx;
    auto status = ensureRemoteClient(&ctx);
    if (status == core::Status::OK) {
        status = client_->commit(&ctx);
    }
    if (status == core::Status::OK) {
        status = client_->beginTransaction(&ctx);
    }
    if (status != core::Status::OK) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_unavailable, "Commit retaining failed");
        return sendBuffer(conn);
    }

    std::vector<uint8_t> data;
    sendResponse(conn, tr_handle, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleRollbackRetaining(network::Connection* conn) {
    size_t offset = 4;

    uint32_t tr_handle = readUInt32(current_packet_.data() + offset);

    if (active_transactions_.find(tr_handle) == active_transactions_.end()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_tr_handle, "Invalid transaction handle");
        return sendBuffer(conn);
    }

    core::ErrorContext ctx;
    auto status = ensureRemoteClient(&ctx);
    if (status == core::Status::OK) {
        status = client_->rollback(&ctx);
    }
    if (status == core::Status::OK) {
        status = client_->beginTransaction(&ctx);
    }
    if (status != core::Status::OK) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_unavailable, "Rollback retaining failed");
        return sendBuffer(conn);
    }

    std::vector<uint8_t> data;
    sendResponse(conn, tr_handle, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleAllocateStatement(network::Connection* conn) {
    size_t offset = 4;

    uint32_t db_handle = readUInt32(current_packet_.data() + offset);

    if (db_handle != db_handle_) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "Invalid database handle");
        return sendBuffer(conn);
    }

    // Allocate new statement handle
    uint32_t stmt_handle = next_stmt_handle_++;

    FirebirdStatement stmt;
    stmt.handle = stmt_handle;
    stmt.prepared = false;
    statements_[stmt_handle] = stmt;

    std::vector<uint8_t> data;
    sendResponse(conn, stmt_handle, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handlePrepareStatement(network::Connection* conn) {
    size_t offset = 4;

    uint32_t tr_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    uint32_t stmt_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    // SQL dialect
    uint32_t dialect = readUInt32(current_packet_.data() + offset);
    offset += 4;
    (void)dialect;

    // SQL statement
    std::string sql = readString(current_packet_.data(), offset, current_packet_.size());

    // Description items (BLR) - store for future SQLDA parsing
    std::vector<uint8_t> items = readBuffer(current_packet_.data(), offset, current_packet_.size());

    if (active_transactions_.find(tr_handle) == active_transactions_.end()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_tr_handle, "Invalid transaction handle");
        return sendBuffer(conn);
    }

    auto it = statements_.find(stmt_handle);
    if (it == statements_.end()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_stmt_handle, "Invalid statement handle");
        return sendBuffer(conn);
    }

    // Store prepared statement
    it->second.query = sql;
    it->second.prepared = true;
    it->second.input_blr.clear();
    it->second.output_blr = std::move(items);
    it->second.output_fields.clear();
    it->second.output_message_length = 0;

    if (!it->second.output_blr.empty()) {
        std::string blr_err;
        auto parse_status = parseBlr(it->second.output_blr,
                                     it->second.output_fields,
                                     it->second.output_message_length,
                                     blr_err);
        if (parse_status != core::Status::OK) {
            sendErrorResponse(conn, firebird::ErrorCode::isc_unavailable,
                              blr_err.empty() ? "Unsupported BLR" : blr_err);
            return sendBuffer(conn);
        }
    }

    // Determine statement type (simplified)
    std::string upper_sql = sql;
    for (char& c : upper_sql) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (upper_sql.find("SELECT") == 0) {
        it->second.type = firebird::StatementType::TYPE_SELECT;
    } else if (upper_sql.find("INSERT") == 0) {
        it->second.type = firebird::StatementType::TYPE_INSERT;
    } else if (upper_sql.find("UPDATE") == 0) {
        it->second.type = firebird::StatementType::TYPE_UPDATE;
    } else if (upper_sql.find("DELETE") == 0) {
        it->second.type = firebird::StatementType::TYPE_DELETE;
    } else {
        it->second.type = firebird::StatementType::TYPE_DDL;
    }

    std::vector<uint8_t> data;
    sendResponse(conn, stmt_handle, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleExecute(network::Connection* conn) {
    size_t offset = 4;

    uint32_t tr_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    uint32_t stmt_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    // BLR for input parameters (if any)
    std::vector<uint8_t> input_blr = readBuffer(current_packet_.data(), offset, current_packet_.size());
    // Input message buffer (values)
    std::vector<uint8_t> input_message = readBuffer(current_packet_.data(), offset, current_packet_.size());

    if (active_transactions_.find(tr_handle) == active_transactions_.end()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_tr_handle, "Invalid transaction handle");
        return sendBuffer(conn);
    }

    auto it = statements_.find(stmt_handle);
    if (it == statements_.end() || !it->second.prepared) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_stmt_handle, "Statement not prepared");
        return sendBuffer(conn);
    }

    // Parse input BLR if provided
    if (!input_blr.empty()) {
        std::string blr_err;
        auto parse_status = parseBlr(input_blr, it->second.input_fields,
                                     it->second.input_message_length, blr_err);
        if (parse_status != core::Status::OK) {
            sendErrorResponse(conn, firebird::ErrorCode::isc_unavailable,
                              blr_err.empty() ? "Unsupported input BLR" : blr_err);
            return sendBuffer(conn);
        }
    }

    QueryContext ctx;

    // Decode parameters (text only for now)
    std::vector<std::string> param_values;
    if (!input_message.empty() && !it->second.input_fields.empty()) {
        const size_t field_count = it->second.input_fields.size();
        const size_t null_bytes = field_count * 2;

        // Validate message length when provided
        if (it->second.input_message_length > 0 &&
            input_message.size() < it->second.input_message_length) {
            sendErrorResponse(conn, firebird::ErrorCode::isc_dsql_error,
                              "Input message truncated");
            return sendBuffer(conn);
        }

        // Position of null indicator vector (trailing)
        size_t null_offset = input_message.size() >= null_bytes
                                 ? input_message.size() - null_bytes
                                 : input_message.size();
        if (it->second.input_message_length > 0 &&
            it->second.input_message_length >= null_bytes &&
            it->second.input_message_length <= input_message.size()) {
            null_offset = it->second.input_message_length - null_bytes;
        }
        std::vector<int16_t> nulls(field_count, 0);
        if (null_offset + null_bytes <= input_message.size()) {
            for (size_t i = 0; i < field_count; ++i) {
                size_t pos = null_offset + (i * 2);
                nulls[i] = static_cast<int16_t>(
                    (static_cast<uint16_t>(input_message[pos]) << 8) |
                    static_cast<uint16_t>(input_message[pos + 1]));
            }
        }

        size_t m_idx = 0;
        for (size_t idx_field = 0; idx_field < it->second.input_fields.size(); ++idx_field) {
            const auto& field = it->second.input_fields[idx_field];
            const bool is_null = nulls[idx_field] < 0;
            ctx.parameter_formats.push_back(0); // text format
            ctx.parameter_nulls.push_back(is_null);

            // Skip over field bytes if the null indicator is set
            auto advance = [&](size_t bytes) {
                size_t max_bytes = (null_offset > m_idx) ? (null_offset - m_idx) : 0;
                bytes = std::min(bytes, max_bytes);
                m_idx = std::min(input_message.size(), m_idx + bytes);
            };

            if (is_null) {
                param_values.emplace_back("");
                advance(field.length);
                continue;
            }

            if (field.is_text || field.is_varying) {
                if (m_idx + 2 > null_offset) {
                    sendErrorResponse(conn, firebird::ErrorCode::isc_dsql_error,
                                      "Input message truncated (text length)");
                    return sendBuffer(conn);
                }
                uint16_t len = static_cast<uint16_t>(
                    (static_cast<uint16_t>(input_message[m_idx]) << 8) |
                    static_cast<uint16_t>(input_message[m_idx + 1]));
                m_idx += 2;
                if (m_idx + len > null_offset) {
                    sendErrorResponse(conn, firebird::ErrorCode::isc_dsql_error,
                                      "Input message truncated (text payload)");
                    return sendBuffer(conn);
                }
                std::string val(reinterpret_cast<const char*>(&input_message[m_idx]), len);
                param_values.push_back(val);
                m_idx += len;
            } else if (field.is_numeric() && field.length == 4) {
                if (m_idx + 4 > null_offset) {
                    sendErrorResponse(conn, firebird::ErrorCode::isc_dsql_error,
                                      "Input message truncated (int32)");
                    return sendBuffer(conn);
                }
                int32_t ival = static_cast<int32_t>(
                    (input_message[m_idx] << 24) |
                    (input_message[m_idx + 1] << 16) |
                    (input_message[m_idx + 2] << 8) |
                    (input_message[m_idx + 3]));
                param_values.push_back(std::to_string(ival));
                m_idx += 4;
            } else if (field.is_numeric() && field.length == 2) {
                if (m_idx + 2 > null_offset) {
                    sendErrorResponse(conn, firebird::ErrorCode::isc_dsql_error,
                                      "Input message truncated (int16)");
                    return sendBuffer(conn);
                }
                int16_t sval = static_cast<int16_t>(
                    (static_cast<uint16_t>(input_message[m_idx]) << 8) |
                    static_cast<uint16_t>(input_message[m_idx + 1]));
                param_values.push_back(std::to_string(sval));
                m_idx += 2;
            } else {
                // Unsupported type; skip
                param_values.push_back("");
                advance(field.length);
            }
        }
    }

    // Execute the statement
    ctx.query = it->second.query;
    ctx.parameter_values = param_values;

    ResultContext result;
    core::ErrorContext err;
    auto status = executeRemoteQuery(ctx, result, &err);

    if (status != core::Status::OK || result.has_error) {
        std::string message = result.error_message.empty() ? err.message : result.error_message;
        int32_t code = result.error_code ? static_cast<int32_t>(result.error_code)
                                         : mapStatusToFirebird(status);
        sendErrorResponse(conn, code, message.empty() ? "Query failed" : message, result.sqlstate);
    } else {
        // Materialize rows for fetch if needed
        it->second.row_buffers.clear();
        it->second.fetch_pos = 0;

        if (!result.columns.empty()) {
            // If client did not supply an output BLR, synthesize field layout from column metadata
            if (it->second.output_fields.empty()) {
                uint32_t msg_len = 0;
                it->second.output_fields.reserve(result.columns.size());
                for (const auto& col : result.columns) {
                    auto f = columnToBlrField(col);
                    msg_len += f.length;
                    it->second.output_fields.push_back(f);
                }
                if (!it->second.output_fields.empty()) {
                    msg_len += static_cast<uint32_t>(it->second.output_fields.size()) * 2; // null indicators
                }
                it->second.output_message_length = msg_len;
            }
            it->second.columns = result.columns;
            for (const auto& row : result.rows) {
                std::vector<uint8_t> buf;
                std::vector<int16_t> nulls;
                nulls.reserve(row.size());

                const bool have_blr = !it->second.output_fields.empty();
                for (size_t col_idx = 0; col_idx < row.size(); ++col_idx) {
                    const auto& val = row[col_idx];
                    size_t out_len = val.data.size();
                    if (have_blr && col_idx < it->second.output_fields.size()) {
                        // Clamp to BLR-declared length when available
                        uint16_t declared_len = it->second.output_fields[col_idx].length;
                        if (declared_len > 0) {
                            out_len = std::min<size_t>(out_len, declared_len);
                        }
                    }

                    if (val.is_null) {
                        writeInt32(buf, -1);
                        nulls.push_back(-1);
                    } else {
                        writeInt32(buf, static_cast<int32_t>(out_len));
                        buf.insert(buf.end(), val.data.begin(), val.data.begin() + out_len);
                        nulls.push_back(0);
                    }
                }

                if (have_blr) {
                    for (auto n : nulls) {
                        uint16_t v = static_cast<uint16_t>(n);
                        buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
                        buf.push_back(static_cast<uint8_t>(v & 0xFF));
                    }
                }

                // Honor declared message length (pad or truncate)
                uint32_t target_len = it->second.output_message_length;
                if (target_len > 0) {
                    if (buf.size() < target_len) {
                        buf.resize(target_len, 0);
                    } else if (buf.size() > target_len) {
                        buf.resize(target_len);
                    }
                }

                it->second.row_buffers.push_back(std::move(buf));
            }
        }

        std::vector<uint8_t> data;
        sendResponse(conn, stmt_handle, static_cast<uint64_t>(result.rows_affected), data);
    }

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleExecute2(network::Connection* conn) {
    // Execute2 includes output parameters - for now, same as Execute
    return handleExecute(conn);
}

core::Status FirebirdAdapter::handleExecImmediate(network::Connection* conn) {
    size_t offset = 4;

    uint32_t tr_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    uint32_t db_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    uint32_t dialect = readUInt32(current_packet_.data() + offset);
    offset += 4;
    (void)dialect;

    std::string sql = readString(current_packet_.data(), offset, current_packet_.size());

    if (db_handle != db_handle_) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "Invalid database handle");
        return sendBuffer(conn);
    }

    if (active_transactions_.find(tr_handle) == active_transactions_.end()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_tr_handle, "Invalid transaction handle");
        return sendBuffer(conn);
    }

    // Execute immediately (no BLR parsing here yet)
    QueryContext ctx;
    ctx.query = sql;

    ResultContext result;
    core::ErrorContext err;
    auto status = executeRemoteQuery(ctx, result, &err);

    if (status != core::Status::OK || result.has_error) {
        std::string message = result.error_message.empty() ? err.message : result.error_message;
        int32_t code = result.error_code ? static_cast<int32_t>(result.error_code)
                                         : mapStatusToFirebird(status);
        sendErrorResponse(conn, code, message.empty() ? "Query failed" : message, result.sqlstate);
    } else {
        std::vector<uint8_t> data;
        sendResponse(conn, 0, static_cast<uint64_t>(result.rows_affected), data);
    }

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleExecImmediate2(network::Connection* conn) {
    return handleExecImmediate(conn);
}

core::Status FirebirdAdapter::handleFetch(network::Connection* conn) {
    size_t offset = 4;

    uint32_t stmt_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    // BLR for output
    std::vector<uint8_t> blr = readBuffer(current_packet_.data(), offset, current_packet_.size());

    // Message length
    uint32_t msg_len = readUInt32(current_packet_.data() + offset);
    offset += 4;

    auto it = statements_.find(stmt_handle);
    if (it == statements_.end()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_stmt_handle, "Invalid statement handle");
        return sendBuffer(conn);
    }

    // If client supplied BLR/message length, record them (no full parsing yet)
    if (!blr.empty()) {
        it->second.output_blr = blr;
        it->second.output_fields.clear();
        std::string blr_err;
        auto parse_status = parseBlr(it->second.output_blr,
                                     it->second.output_fields,
                                     it->second.output_message_length,
                                     blr_err);
        if (parse_status != core::Status::OK) {
            sendErrorResponse(conn, firebird::ErrorCode::isc_unavailable,
                              blr_err.empty() ? "Unsupported BLR" : blr_err);
            return sendBuffer(conn);
        }
    }
    if (msg_len > 0) {
        it->second.output_message_length = msg_len;
    }

    std::vector<std::vector<uint8_t>> rows;
    uint32_t status = 0;  // 0 = success, 100 = end

    // Return all remaining rows in one fetch for simplicity
    while (it->second.fetch_pos < it->second.row_buffers.size()) {
        rows.push_back(it->second.row_buffers[it->second.fetch_pos++]);
    }

    if (rows.empty()) {
        status = 100;  // end of cursor
    }

    sendFetchResponse(conn, status, static_cast<uint32_t>(rows.size()), rows);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleFreeStatement(network::Connection* conn) {
    size_t offset = 4;

    uint32_t stmt_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    uint32_t option = readUInt32(current_packet_.data() + offset);
    (void)option;  // DSQL_close, DSQL_drop, etc.

    statements_.erase(stmt_handle);

    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleSetCursor(network::Connection* conn) {
    // Set cursor name - acknowledge
    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);
    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleInfoDatabase(network::Connection* conn) {
    // Return database info
    std::vector<uint8_t> info;

    // For now, return minimal info
    // In a real implementation, we'd return requested info items

    sendResponse(conn, db_handle_, 0, info);
    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleInfoTransaction(network::Connection* conn) {
    std::vector<uint8_t> info;
    sendResponse(conn, current_transaction_, 0, info);
    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleInfoSql(network::Connection* conn) {
    size_t offset = 4;

    uint32_t stmt_handle = readUInt32(current_packet_.data() + offset);

    auto it = statements_.find(stmt_handle);
    if (it == statements_.end()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_stmt_handle, "Invalid statement handle");
        return sendBuffer(conn);
    }

    std::vector<uint8_t> info;
    // Return statement type, etc.

    sendResponse(conn, stmt_handle, 0, info);
    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleContAuth(network::Connection* conn) {
    size_t offset = 4;

    // Auth data
    std::vector<uint8_t> data = readBuffer(current_packet_.data(), offset, current_packet_.size());

    // Plugin name
    std::string plugin = readString(current_packet_.data(), offset, current_packet_.size());

    // For testing, accept any auth
    // In a real implementation, we'd validate SRP or other auth

    auth_complete_ = true;
    fb_state_ = FirebirdProtocolState::AUTHENTICATED;

    std::vector<uint8_t> auth_response;
    std::vector<uint8_t> keys;
    sendAcceptData(conn, protocol_version_, firebird::ARCH_GENERIC, 1,
                  auth_response, plugin, true, keys);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handlePing(network::Connection* conn) {
    // Respond to ping
    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);
    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleCancel(network::Connection* conn) {
    // Cancel current operation
    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);
    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleDisconnect(network::Connection* conn) {
    fb_state_ = FirebirdProtocolState::CLOSING;
    if (client_) {
        client_->disconnect();
        client_.reset();
    }
    conn->close(network::CloseReason::CLIENT_DISCONNECT);
    return core::Status::OK;
}

// ============================================================================
// Response Sending
// ============================================================================

void FirebirdAdapter::sendPacket(network::Connection* conn, uint32_t opcode,
                                  const std::vector<uint8_t>& data) {
    std::vector<uint8_t> packet;

    // Opcode
    writeUInt32(packet, opcode);

    // Data
    packet.insert(packet.end(), data.begin(), data.end());

    writeToBuffer(conn, packet.data(), packet.size());
}

void FirebirdAdapter::sendAccept(network::Connection* conn, uint32_t version,
                                  uint32_t arch, uint32_t type) {
    std::vector<uint8_t> data;

    writeUInt32(data, version);
    writeUInt32(data, arch);
    writeUInt32(data, type);

    sendPacket(conn, firebird::Opcode::op_accept, data);
}

void FirebirdAdapter::sendAcceptData(network::Connection* conn, uint32_t version,
                                      uint32_t arch, uint32_t type,
                                      const std::vector<uint8_t>& auth_data,
                                      const std::string& plugin, bool authenticated,
                                      const std::vector<uint8_t>& keys) {
    std::vector<uint8_t> data;

    writeUInt32(data, version);
    writeUInt32(data, arch);
    writeUInt32(data, type);

    // Auth data
    writeBuffer(data, auth_data.data(), auth_data.size());

    // Plugin name
    writeString(data, plugin);

    // Authenticated flag
    writeUInt32(data, authenticated ? 1 : 0);

    // Keys
    writeBuffer(data, keys.data(), keys.size());

    sendPacket(conn, firebird::Opcode::op_accept_data, data);
}

void FirebirdAdapter::sendResponse(network::Connection* conn, uint32_t handle,
                                    uint64_t object_id, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> response;

    writeUInt32(response, handle);
    writeInt64(response, static_cast<int64_t>(object_id));

    // Data buffer
    writeBuffer(response, data.data(), data.size());

    // Status vector (empty = success)
    writeUInt32(response, firebird::ErrorCode::isc_arg_end);

    sendPacket(conn, firebird::Opcode::op_response, response);
}

void FirebirdAdapter::sendFetchResponse(network::Connection* conn, uint32_t status,
                                         uint32_t count,
                                         const std::vector<std::vector<uint8_t>>& rows) {
    std::vector<uint8_t> data;

    writeUInt32(data, status);
    writeUInt32(data, count);

    // Row data
    for (const auto& row : rows) {
        writeBuffer(data, row.data(), row.size());
    }

    sendPacket(conn, firebird::Opcode::op_fetch_response, data);
}

void FirebirdAdapter::sendSqlResponse(network::Connection* conn, uint32_t count) {
    std::vector<uint8_t> data;
    writeUInt32(data, count);
    sendPacket(conn, firebird::Opcode::op_sql_response, data);
}

void FirebirdAdapter::sendErrorResponse(network::Connection* conn, int32_t error_code,
                                         const std::string& message,
                                         const std::string& sqlstate) {
    std::vector<uint8_t> data;

    // Handle
    writeUInt32(data, 0);

    // Object ID
    writeInt64(data, 0);

    // Empty data
    writeUInt32(data, 0);

    // Status vector
    writeInt32(data, firebird::ErrorCode::isc_arg_gds);
    writeInt32(data, error_code);

    if (!message.empty()) {
        writeInt32(data, firebird::ErrorCode::isc_arg_string);
        writeString(data, message);
    }
    if (!sqlstate.empty()) {
        writeInt32(data, firebird::ErrorCode::isc_arg_sql_state);
        writeString(data, sqlstate);
    }

    writeInt32(data, firebird::ErrorCode::isc_arg_end);

    sendPacket(conn, firebird::Opcode::op_response, data);
}

// ============================================================================
// Helper Methods
// ============================================================================

void FirebirdAdapter::writeInt32(std::vector<uint8_t>& buf, int32_t value) {
    writeUInt32(buf, static_cast<uint32_t>(value));
}

void FirebirdAdapter::writeUInt32(std::vector<uint8_t>& buf, uint32_t value) {
    // XDR: big-endian
    buf.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
}

void FirebirdAdapter::writeInt64(std::vector<uint8_t>& buf, int64_t value) {
    // XDR: big-endian
    for (int i = 7; i >= 0; --i) {
        buf.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

void FirebirdAdapter::writeBuffer(std::vector<uint8_t>& buf, const void* data, size_t len) {
    // Length-prefixed buffer with padding to 4-byte boundary
    writeUInt32(buf, static_cast<uint32_t>(len));

    if (len > 0) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        buf.insert(buf.end(), ptr, ptr + len);

        // Pad to 4-byte boundary
        size_t padding = (4 - (len % 4)) % 4;
        for (size_t i = 0; i < padding; ++i) {
            buf.push_back(0);
        }
    }
}

void FirebirdAdapter::writeString(std::vector<uint8_t>& buf, const std::string& str) {
    writeBuffer(buf, str.data(), str.size());
}

void FirebirdAdapter::writePaddedString(std::vector<uint8_t>& buf, const std::string& str) {
    writeString(buf, str);
}

int32_t FirebirdAdapter::readInt32(const uint8_t* data) {
    return static_cast<int32_t>(readUInt32(data));
}

uint32_t FirebirdAdapter::readUInt32(const uint8_t* data) {
    // XDR: big-endian
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
}

int64_t FirebirdAdapter::readInt64(const uint8_t* data) {
    int64_t result = 0;
    for (int i = 0; i < 8; ++i) {
        result = (result << 8) | data[i];
    }
    return result;
}

std::string FirebirdAdapter::readString(const uint8_t* data, size_t& offset, size_t max_len) {
    if (offset + 4 > max_len) return "";

    uint32_t len = readUInt32(data + offset);
    offset += 4;

    if (len == 0 || offset + len > max_len) return "";

    std::string result(reinterpret_cast<const char*>(data + offset), len);
    offset += len;

    // Skip padding
    size_t padding = (4 - (len % 4)) % 4;
    offset += padding;

    return result;
}

std::vector<uint8_t> FirebirdAdapter::readBuffer(const uint8_t* data, size_t& offset, size_t max_len) {
    if (offset + 4 > max_len) return {};

    uint32_t len = readUInt32(data + offset);
    offset += 4;

    if (len == 0 || offset + len > max_len) return {};

    std::vector<uint8_t> result(data + offset, data + offset + len);
    offset += len;

    // Skip padding
    size_t padding = (4 - (len % 4)) % 4;
    offset += padding;

    return result;
}

uint16_t FirebirdAdapter::wireTypeToFirebirdType(WireType type) {
    switch (type) {
        case WireType::BOOLEAN: return firebird::SqlType::SQL_BOOLEAN;
        case WireType::INT16: return firebird::SqlType::SQL_SHORT;
        case WireType::INT32: return firebird::SqlType::SQL_LONG;
        case WireType::INT64: return firebird::SqlType::SQL_INT64;
        case WireType::FLOAT32: return firebird::SqlType::SQL_FLOAT;
        case WireType::FLOAT64: return firebird::SqlType::SQL_DOUBLE;
        case WireType::DECIMAL: return firebird::SqlType::SQL_INT64;  // Scaled
        case WireType::VARCHAR: return firebird::SqlType::SQL_VARYING;
        case WireType::CHAR: return firebird::SqlType::SQL_TEXT;
        case WireType::BYTEA: return firebird::SqlType::SQL_BLOB;
        case WireType::DATE: return firebird::SqlType::SQL_TYPE_DATE;
        case WireType::TIME: return firebird::SqlType::SQL_TYPE_TIME;
        case WireType::TIMESTAMP: return firebird::SqlType::SQL_TIMESTAMP;
        case WireType::TIMESTAMPTZ: return firebird::SqlType::SQL_TIMESTAMP_TZ;
        default: return firebird::SqlType::SQL_VARYING;
    }
}

WireType FirebirdAdapter::firebirdTypeToWireType(uint16_t type) {
    switch (type) {
        case firebird::SqlType::SQL_BOOLEAN: return WireType::BOOLEAN;
        case firebird::SqlType::SQL_SHORT: return WireType::INT16;
        case firebird::SqlType::SQL_LONG: return WireType::INT32;
        case firebird::SqlType::SQL_INT64: return WireType::INT64;
        case firebird::SqlType::SQL_FLOAT: return WireType::FLOAT32;
        case firebird::SqlType::SQL_DOUBLE:
        case firebird::SqlType::SQL_D_FLOAT: return WireType::FLOAT64;
        case firebird::SqlType::SQL_TEXT: return WireType::CHAR;
        case firebird::SqlType::SQL_VARYING: return WireType::VARCHAR;
        case firebird::SqlType::SQL_BLOB: return WireType::BYTEA;
        case firebird::SqlType::SQL_TYPE_DATE: return WireType::DATE;
        case firebird::SqlType::SQL_TYPE_TIME:
        case firebird::SqlType::SQL_TIME_TZ: return WireType::TIME;
        case firebird::SqlType::SQL_TIMESTAMP:
        case firebird::SqlType::SQL_TIMESTAMP_TZ: return WireType::TIMESTAMP;
        default: return WireType::VARCHAR;
    }
}

void FirebirdAdapter::parseDpb(const std::vector<uint8_t>& dpb) {
    if (dpb.empty()) return;

    size_t offset = 0;

    // Version
    if (dpb[offset] == firebird::DpbItem::isc_dpb_version1 ||
        dpb[offset] == firebird::DpbItem::isc_dpb_version2) {
        offset++;
    }

    while (offset < dpb.size()) {
        uint8_t item = dpb[offset++];
        if (offset >= dpb.size()) break;

        uint8_t len = dpb[offset++];
        if (offset + len > dpb.size()) break;

        std::string value(reinterpret_cast<const char*>(dpb.data() + offset), len);
        offset += len;

        switch (item) {
            case firebird::DpbItem::isc_dpb_user_name:
                username_ = value;
                break;
            case firebird::DpbItem::isc_dpb_sql_dialect:
                if (!value.empty()) sql_dialect_ = static_cast<uint8_t>(value[0]);
                break;
            case firebird::DpbItem::isc_dpb_lc_ctype:
                client_charset_ = value;
                break;
            default:
                // Ignore other items
                break;
        }
    }
}

std::vector<uint8_t> FirebirdAdapter::buildDefaultTpb() {
    std::vector<uint8_t> tpb;

    tpb.push_back(firebird::TpbItem::isc_tpb_version3);
    tpb.push_back(firebird::TpbItem::isc_tpb_write);
    tpb.push_back(firebird::TpbItem::isc_tpb_read_committed);
    tpb.push_back(firebird::TpbItem::isc_tpb_rec_version);
    tpb.push_back(firebird::TpbItem::isc_tpb_wait);

    return tpb;
}

// ============================================================================
// Service Manager Implementation (C5.1)
// ============================================================================

void FirebirdAdapter::parseSpb(const std::vector<uint8_t>& spb, ServiceState& state) {
    if (spb.empty()) return;

    size_t offset = 0;

    // Version
    if (spb[offset] == firebird::SpbItem::isc_spb_version1 ||
        spb[offset] == firebird::SpbItem::isc_spb_version2) {
        offset++;
    }

    while (offset < spb.size()) {
        uint8_t item = spb[offset++];
        if (offset >= spb.size()) break;

        uint8_t len = spb[offset++];
        if (offset + len > spb.size()) break;

        std::string value(reinterpret_cast<const char*>(spb.data() + offset), len);
        offset += len;

        switch (item) {
            case firebird::SpbItem::isc_spb_user_name:
                state.username = value;
                break;
            case firebird::SpbItem::isc_spb_password:
                // Password would be validated here
                break;
            default:
                // Ignore other items
                break;
        }
    }
}

core::Status FirebirdAdapter::handleServiceAttach(network::Connection* conn) {
    size_t offset = 4;  // Skip opcode

    // Service name (e.g., "service_mgr")
    std::string service_name = readString(current_packet_.data(), offset, current_packet_.size());

    // SPB (Service Parameter Block)
    std::vector<uint8_t> spb = readBuffer(current_packet_.data(), offset, current_packet_.size());

    // Parse SPB for credentials
    service_state_.handle = next_db_handle_++;
    service_state_.active = true;
    parseSpb(spb, service_state_);

    // Send response with service handle
    std::vector<uint8_t> data;
    sendResponse(conn, service_state_.handle, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleServiceDetach(network::Connection* conn) {
    size_t offset = 4;

    uint32_t svc_handle = readUInt32(current_packet_.data() + offset);

    if (svc_handle != service_state_.handle || !service_state_.active) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "Invalid service handle");
        return sendBuffer(conn);
    }

    service_state_.active = false;
    service_state_.handle = 0;
    service_state_.action = 0;

    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleServiceInfo(network::Connection* conn) {
    size_t offset = 4;

    uint32_t svc_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    // Skip incarnation
    offset += 4;

    // Info items
    std::vector<uint8_t> items = readBuffer(current_packet_.data(), offset, current_packet_.size());

    if (svc_handle != service_state_.handle || !service_state_.active) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "Invalid service handle");
        return sendBuffer(conn);
    }

    // Build info response
    std::vector<uint8_t> info_data;

    for (uint8_t item : items) {
        switch (item) {
            case firebird::SpbItem::isc_info_svc_version:
                info_data.push_back(firebird::SpbItem::isc_info_svc_version);
                writeUInt32(info_data, 5);  // Version 5.0
                break;
            case firebird::SpbItem::isc_info_svc_server_version:
                info_data.push_back(firebird::SpbItem::isc_info_svc_server_version);
                writeString(info_data, server_version_);
                break;
            case firebird::SpbItem::isc_info_svc_implementation:
                info_data.push_back(firebird::SpbItem::isc_info_svc_implementation);
                writeString(info_data, "ScratchBird");
                break;
            case firebird::SpbItem::isc_info_svc_capabilities:
                info_data.push_back(firebird::SpbItem::isc_info_svc_capabilities);
                writeUInt32(info_data, 0xFFFFFFFF);  // All capabilities
                break;
            case firebird::SpbItem::isc_info_svc_get_users:
                // Return empty user list
                info_data.push_back(firebird::SpbItem::isc_info_svc_get_users);
                info_data.push_back(firebird::isc_info_end);
                break;
            default:
                // Unknown item
                break;
        }
    }

    info_data.push_back(firebird::isc_info_end);

    sendResponse(conn, svc_handle, 0, info_data);
    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleServiceStart(network::Connection* conn) {
    size_t offset = 4;

    uint32_t svc_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    // Action block (TLV format)
    std::vector<uint8_t> action_block = readBuffer(current_packet_.data(), offset, current_packet_.size());

    if (svc_handle != service_state_.handle || !service_state_.active) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "Invalid service handle");
        return sendBuffer(conn);
    }

    // Parse action
    if (!action_block.empty()) {
        service_state_.action = action_block[0];
    }

    // For now, acknowledge the start
    // In a full implementation, we'd execute the requested service action
    std::vector<uint8_t> data;
    sendResponse(conn, svc_handle, 0, data);

    return sendBuffer(conn);
}

// ============================================================================
// Event Operations Implementation (C5.2)
// ============================================================================

core::Status FirebirdAdapter::handleQueEvents(network::Connection* conn) {
    size_t offset = 4;

    uint32_t db_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    if (db_handle != db_handle_) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "Invalid database handle");
        return sendBuffer(conn);
    }

    // Event names buffer (count + names)
    std::vector<uint8_t> events_data = readBuffer(current_packet_.data(), offset, current_packet_.size());

    // AST (asynchronous trap) address - 8 bytes
    uint64_t ast = readInt64(current_packet_.data() + offset);
    offset += 8;
    (void)ast;

    // Argument - 8 bytes
    uint64_t arg = readInt64(current_packet_.data() + offset);
    offset += 8;
    (void)arg;

    // Parse event names
    event_state_.event_names.clear();
    if (events_data.size() >= 4) {
        uint32_t count = (static_cast<uint32_t>(events_data[0]) << 24) |
                        (static_cast<uint32_t>(events_data[1]) << 16) |
                        (static_cast<uint32_t>(events_data[2]) << 8) |
                        static_cast<uint32_t>(events_data[3]);
        size_t e_offset = 4;
        for (uint32_t i = 0; i < count && e_offset < events_data.size(); ++i) {
            // Read length-prefixed string
            if (e_offset + 1 > events_data.size()) break;
            uint8_t len = events_data[e_offset++];
            if (e_offset + len > events_data.size()) break;
            std::string name(reinterpret_cast<const char*>(events_data.data() + e_offset), len);
            event_state_.event_names.push_back(name);
            e_offset += len;
        }
    }

    event_state_.event_id = next_event_id_++;
    event_state_.active = true;

    // Send response with event ID
    std::vector<uint8_t> data;
    writeUInt32(data, event_state_.event_id);
    sendResponse(conn, db_handle, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleCancelEvents(network::Connection* conn) {
    size_t offset = 4;

    uint32_t db_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    uint32_t event_id = readUInt32(current_packet_.data() + offset);

    if (db_handle != db_handle_) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "Invalid database handle");
        return sendBuffer(conn);
    }

    if (event_id == event_state_.event_id) {
        event_state_.active = false;
        event_state_.event_names.clear();
    }

    std::vector<uint8_t> data;
    sendResponse(conn, db_handle, 0, data);

    return sendBuffer(conn);
}

// ============================================================================
// BLOB Operations Implementation (C5.3)
// ============================================================================

core::Status FirebirdAdapter::handleCreateBlob(network::Connection* conn) {
    size_t offset = 4;

    uint32_t tr_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    if (active_transactions_.find(tr_handle) == active_transactions_.end()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_tr_handle, "Invalid transaction handle");
        return sendBuffer(conn);
    }

    // Create new BLOB
    BlobState blob;
    blob.blob_id = next_blob_id_++;
    blob.is_new = true;
    blob.is_segmented = true;

    uint64_t blob_id = (static_cast<uint64_t>(db_handle_) << 32) | blob.blob_id;
    blobs_[blob_id] = blob;

    std::vector<uint8_t> data;
    writeInt64(data, static_cast<int64_t>(blob_id));
    sendResponse(conn, 0, blob_id, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleCreateBlob2(network::Connection* conn) {
    // Similar to create_blob but with BPB (BLOB Parameter Block)
    return handleCreateBlob(conn);
}

core::Status FirebirdAdapter::handleOpenBlob(network::Connection* conn) {
    size_t offset = 4;

    uint32_t tr_handle = readUInt32(current_packet_.data() + offset);
    offset += 4;

    int64_t blob_id_raw = readInt64(current_packet_.data() + offset);
    uint64_t blob_id = static_cast<uint64_t>(blob_id_raw);

    if (active_transactions_.find(tr_handle) == active_transactions_.end()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_tr_handle, "Invalid transaction handle");
        return sendBuffer(conn);
    }

    auto it = blobs_.find(blob_id);
    if (it == blobs_.end()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "Invalid BLOB ID");
        return sendBuffer(conn);
    }

    it->second.position = 0;

    std::vector<uint8_t> data;
    sendResponse(conn, 0, blob_id, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleOpenBlob2(network::Connection* conn) {
    return handleOpenBlob(conn);
}

core::Status FirebirdAdapter::handleCloseBlob(network::Connection* conn) {
    size_t offset = 4;

    int64_t blob_id_raw = readInt64(current_packet_.data() + offset);
    uint64_t blob_id = static_cast<uint64_t>(blob_id_raw);

    auto it = blobs_.find(blob_id);
    if (it != blobs_.end()) {
        // Persist new blobs to storage (in real implementation)
        blobs_.erase(it);
    }

    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleCancelBlob(network::Connection* conn) {
    size_t offset = 4;

    int64_t blob_id_raw = readInt64(current_packet_.data() + offset);
    uint64_t blob_id = static_cast<uint64_t>(blob_id_raw);

    auto it = blobs_.find(blob_id);
    if (it != blobs_.end()) {
        blobs_.erase(it);
    }

    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleGetSegment(network::Connection* conn) {
    size_t offset = 4;

    int64_t blob_id_raw = readInt64(current_packet_.data() + offset);
    offset += 8;
    uint64_t blob_id = static_cast<uint64_t>(blob_id_raw);

    uint32_t seg_length = readUInt32(current_packet_.data() + offset);
    offset += 4;

    uint32_t seg_flags = readUInt32(current_packet_.data() + offset);
    (void)seg_flags;

    auto it = blobs_.find(blob_id);
    if (it == blobs_.end()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "Invalid BLOB ID");
        return sendBuffer(conn);
    }

    // Read segment
    std::vector<uint8_t> segment;
    size_t remaining = it->second.data.size() - it->second.position;
    size_t to_read = std::min(static_cast<size_t>(seg_length), remaining);

    if (to_read > 0) {
        segment.insert(segment.end(),
                      it->second.data.begin() + it->second.position,
                      it->second.data.begin() + it->second.position + to_read);
        it->second.position += to_read;
    }

    bool eof = (it->second.position >= it->second.data.size());

    // Build response
    std::vector<uint8_t> data;
    writeUInt32(data, static_cast<uint32_t>(segment.size()));
    writeBuffer(data, segment.data(), segment.size());
    writeUInt32(data, eof ? 1u : 0u);  // End of BLOB flag

    sendPacket(conn, firebird::Opcode::op_response, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handlePutSegment(network::Connection* conn) {
    size_t offset = 4;

    int64_t blob_id_raw = readInt64(current_packet_.data() + offset);
    offset += 8;
    uint64_t blob_id = static_cast<uint64_t>(blob_id_raw);

    uint32_t seg_length = readUInt32(current_packet_.data() + offset);
    offset += 4;

    auto it = blobs_.find(blob_id);
    if (it == blobs_.end()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "Invalid BLOB ID");
        return sendBuffer(conn);
    }

    // Read segment data
    if (offset + seg_length <= current_packet_.size()) {
        it->second.data.insert(it->second.data.end(),
                              current_packet_.data() + offset,
                              current_packet_.data() + offset + seg_length);
    }

    std::vector<uint8_t> data;
    sendResponse(conn, 0, 0, data);

    return sendBuffer(conn);
}

core::Status FirebirdAdapter::handleSeekBlob(network::Connection* conn) {
    size_t offset = 4;

    int64_t blob_id_raw = readInt64(current_packet_.data() + offset);
    offset += 8;
    uint64_t blob_id = static_cast<uint64_t>(blob_id_raw);

    uint32_t mode = readUInt32(current_packet_.data() + offset);
    offset += 4;

    int32_t position = readInt32(current_packet_.data() + offset);

    auto it = blobs_.find(blob_id);
    if (it == blobs_.end()) {
        sendErrorResponse(conn, firebird::ErrorCode::isc_bad_db_handle, "Invalid BLOB ID");
        return sendBuffer(conn);
    }

    // Seek mode: 0 = relative, 1 = absolute (from beginning), 2 = absolute (from end)
    switch (mode) {
        case 0:  // Relative
            it->second.position = static_cast<size_t>(
                std::max<int64_t>(0, static_cast<int64_t>(it->second.position) + position));
            break;
        case 1:  // Absolute from beginning
            it->second.position = static_cast<size_t>(std::max(0, position));
            break;
        case 2:  // Absolute from end
            it->second.position = static_cast<size_t>(
                std::max<int64_t>(0, static_cast<int64_t>(it->second.data.size()) + position));
            break;
    }

    // Clamp to data size
    if (it->second.position > it->second.data.size()) {
        it->second.position = it->second.data.size();
    }

    std::vector<uint8_t> data;
    sendResponse(conn, 0, static_cast<uint64_t>(it->second.position), data);

    return sendBuffer(conn);
}

} // namespace protocol
} // namespace scratchbird
