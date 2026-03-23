/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

/**
 * FirebirdParserAgent - Full Wire Protocol Implementation
 * 
 * Implements the Firebird wire protocol (op_connect, op_accept, etc.)
 * as documented in the Firebird source code and protocol documentation.
 * 
 * Supports:
 * - Protocol 10, 11, 12, 13, 14, 15, 16
 * - Arc4 and ChaCha wire encryption
 * - Authentication (Legacy, SRP, SRP256)
 * - XDR message format
 * - Events (async notifications)
 * - BLOB streaming
 */

#include "scratchbird/ipc/firebird_parser_agent.h"
#include "scratchbird/core/emulation_package_manifest.h"
#include "scratchbird/parser/firebird/firebird_parser.h"
#include "scratchbird/sblr/v3_container.h"
#include "scratchbird/sblr/v3_opcode_registry.h"
#include "scratchbird/sblr/v3_payloads.h"
#include "scratchbird/udr/dialect_compiler_udr.h"
#include "scratchbird/udr/firebird_emulation_udr.h"
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <sys/ioctl.h>
    #include <netinet/in.h>
#endif
#include "scratchbird/core/posix_compat.h"
#include "scratchbird/core/socket_call_compat.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>


namespace scratchbird {
namespace ipc {

namespace parser = ::scratchbird::parser;

namespace {

std::atomic<uint32_t> g_firebird_info_sql_debug_logs{0};

auto effectiveFirebirdSchemaRoot(const FBClientState& state) -> std::string {
    return state.emulated_schema_root.empty() ? state.database : state.emulated_schema_root;
}

auto escapeFirebirdSqlLiteral(const std::string& in) -> std::string {
    std::string out;
    out.reserve(in.size() + 8);
    for (char ch : in) {
        out.push_back(ch);
        if (ch == '\'') {
            out.push_back('\'');
        }
    }
    return out;
}

void populateFirebirdCompilerRequest(sblr::DialectCompilerRequest& request,
                                     const FBClientState& state) {
    const std::string schema_root = effectiveFirebirdSchemaRoot(state);
    request.module_name = "firebird_emulation";
    request.session.profile_id = "firebirdsql";
    request.session.dialect_tag = "firebird";
    request.session.emulated_schema_root = schema_root;
    request.session.current_schema_name = schema_root;
    request.session.search_path = schema_root.empty()
                                      ? std::vector<std::string>{}
                                      : std::vector<std::string>{schema_root};
}

auto rewriteFirebirdSingleRowCompatibilityQuery(std::string_view sql) -> std::string {
    auto trim = [](std::string_view in) {
        size_t start = 0;
        while (start < in.size() && std::isspace(static_cast<unsigned char>(in[start]))) {
            ++start;
        }
        size_t end = in.size();
        while (end > start && std::isspace(static_cast<unsigned char>(in[end - 1]))) {
            --end;
        }
        return in.substr(start, end - start);
    };

    std::string_view normalized = trim(sql);
    if (!normalized.empty() && normalized.back() == ';') {
        normalized.remove_suffix(1);
        normalized = trim(normalized);
    }

    std::string upper;
    upper.reserve(normalized.size());
    for (char ch : normalized) {
        upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }

    constexpr std::string_view kSelect = "SELECT ";
    constexpr std::string_view kFromRdbDatabase = " FROM RDB$DATABASE";
    if (upper.rfind(kSelect, 0) != 0) {
        return std::string(sql);
    }

    const size_t from_pos = upper.find(kFromRdbDatabase);
    if (from_pos == std::string::npos) {
        return std::string(sql);
    }

    const size_t suffix_pos = from_pos + kFromRdbDatabase.size();
    for (size_t i = suffix_pos; i < upper.size(); ++i) {
        if (!std::isspace(static_cast<unsigned char>(upper[i]))) {
            return std::string(sql);
        }
    }

    std::string_view select_list = trim(normalized.substr(kSelect.size(),
                                                          from_pos - kSelect.size()));
    if (select_list.empty()) {
        return std::string(sql);
    }

    return "SELECT " + std::string(select_list);
}

auto formatHexBytes(const uint8_t* data, size_t size) -> std::string {
    if (data == nullptr || size == 0) {
        return {};
    }

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < size; ++i) {
        if (i != 0) {
            oss << ' ';
        }
        oss << std::setw(2) << static_cast<unsigned>(data[i]);
    }
    return oss.str();
}

auto firebirdSqlInfoItemName(uint8_t item) -> const char* {
    switch (item) {
        case 1: return "isc_info_end";
        case 2: return "isc_info_truncated";
        case 3: return "isc_info_error";
        case 4: return "isc_info_sql_select";
        case 5: return "isc_info_sql_bind";
        case 6: return "isc_info_sql_num_variables";
        case 7: return "isc_info_sql_describe_vars";
        case 8: return "isc_info_sql_describe_end";
        case 9: return "isc_info_sql_sqlda_seq";
        case 10: return "isc_info_sql_message_seq";
        case 11: return "isc_info_sql_type";
        case 12: return "isc_info_sql_sub_type";
        case 13: return "isc_info_sql_scale";
        case 14: return "isc_info_sql_length";
        case 15: return "isc_info_sql_null_ind";
        case 16: return "isc_info_sql_field";
        case 17: return "isc_info_sql_relation";
        case 18: return "isc_info_sql_owner";
        case 19: return "isc_info_sql_alias";
        case 20: return "isc_info_sql_sqlda_start";
        case 21: return "isc_info_sql_stmt_type";
        case 22: return "isc_info_sql_get_plan";
        case 23: return "isc_info_sql_records";
        case 24: return "isc_info_sql_batch_fetch";
        case 25: return "isc_info_sql_relation_alias";
        case 26: return "isc_info_sql_explain_plan";
        case 27: return "isc_info_sql_stmt_flags";
        case 28: return "isc_info_sql_stmt_timeout_user";
        case 29: return "isc_info_sql_stmt_timeout_run";
        case 30: return "isc_info_sql_stmt_blob_align";
        case 31: return "isc_info_sql_exec_path_blr_bytes";
        case 32: return "isc_info_sql_exec_path_blr_text";
        case 33: return "isc_info_sql_relation_schema";
        default: return "unknown";
    }
}

auto describeFirebirdInfoItems(const std::string& items) -> std::string {
    std::ostringstream oss;
    bool first = true;

    for (size_t i = 0; i < items.size(); ++i) {
        const uint8_t item = static_cast<uint8_t>(items[i]);
        if (item == 0 || item == 1) {
            break;
        }

        if (!first) {
            oss << ", ";
        }
        first = false;
        oss << firebirdSqlInfoItemName(item);

        if (item == 20) {
            if (i + 1 >= items.size()) {
                oss << "(malformed)";
                break;
            }
            const uint8_t len = static_cast<uint8_t>(items[i + 1]);
            i += 1;
            uint32_t value = 0;
            for (uint16_t n = 0; n < len && i + 1 + n < items.size() && n < 4; ++n) {
                value |= static_cast<uint32_t>(static_cast<uint8_t>(items[i + 1 + n]))
                         << (n * 8);
            }
            oss << "=" << value;
            i += len;
        }
    }

    return oss.str();
}

} // namespace

// Firebird operation codes
namespace fb {
    constexpr uint32_t arch_generic = 1;
    constexpr uint8_t CNCT_user = 1;
    constexpr uint8_t CNCT_specific_data = 7;
    constexpr uint8_t CNCT_plugin_name = 8;
    constexpr uint8_t CNCT_login = 9;
    constexpr uint8_t CNCT_plugin_list = 10;
    constexpr uint8_t CNCT_client_crypt = 11;

    // Connection
    constexpr uint32_t op_connect = 1;
    constexpr uint32_t op_exit = 2;
    constexpr uint32_t op_accept = 3;
    constexpr uint32_t op_reject = 4;
    constexpr uint32_t op_protocol = 5;
    constexpr uint32_t op_disconnect = 6;
    constexpr uint32_t op_credit = 7;
    constexpr uint32_t op_continuation = 8;
    constexpr uint32_t op_response = 9;
    
    // Database
    constexpr uint32_t op_attach = 19;
    constexpr uint32_t op_create = 20;
    constexpr uint32_t op_detach = 21;
    constexpr uint32_t op_compile = 22;
    constexpr uint32_t op_start = 23;
    constexpr uint32_t op_start_and_send = 24;
    constexpr uint32_t op_send = 25;
    constexpr uint32_t op_receive = 26;
    constexpr uint32_t op_unwind = 27;
    constexpr uint32_t op_release = 28;
    
    // Transaction
    constexpr uint32_t op_transaction = 29;
    constexpr uint32_t op_commit = 30;
    constexpr uint32_t op_rollback = 31;
    constexpr uint32_t op_prepare = 32;
    constexpr uint32_t op_reconnect = 33;
    
    // Information
    constexpr uint32_t op_info_database = 40;
    constexpr uint32_t op_info_request = 41;
    constexpr uint32_t op_info_transaction = 42;
    constexpr uint32_t op_info_blob = 43;
    
    // BLOB
    constexpr uint32_t op_create_blob = 34;
    constexpr uint32_t op_open_blob = 35;
    constexpr uint32_t op_get_segment = 36;
    constexpr uint32_t op_put_segment = 37;
    constexpr uint32_t op_cancel_blob = 38;
    constexpr uint32_t op_close_blob = 39;
    constexpr uint32_t op_batch_segments = 44;
    
    // Events
    constexpr uint32_t op_que_events = 48;
    constexpr uint32_t op_cancel_events = 49;
    constexpr uint32_t op_commit_retaining = 50;
    constexpr uint32_t op_prepare2 = 51;
    constexpr uint32_t op_ddl = 55;
    constexpr uint32_t op_rollback_retaining = 86;

    // DSQL
    constexpr uint32_t op_allocate_statement = 62;
    constexpr uint32_t op_execute = 63;
    constexpr uint32_t op_exec_immediate = 64;
    constexpr uint32_t op_fetch = 65;
    constexpr uint32_t op_fetch_response = 66;
    constexpr uint32_t op_free_statement = 67;
    constexpr uint32_t op_prepare_statement = 68;
    constexpr uint32_t op_set_cursor = 69;
    constexpr uint32_t op_info_sql = 70;
    constexpr uint32_t op_dummy = 71;
    constexpr uint32_t op_response_piggyback = 72;
    constexpr uint32_t op_start_and_receive = 73;
    constexpr uint32_t op_start_send_and_receive = 74;
    constexpr uint32_t op_exec_immediate2 = 75;
    constexpr uint32_t op_execute2 = 76;
    constexpr uint32_t op_sql_response = 78;

    // Wire encryption / auth
    constexpr uint32_t op_authenticate_user = 88;
    constexpr uint32_t op_cancel = 91;
    constexpr uint32_t op_cont_auth = 92;
    constexpr uint32_t op_ping = 93;
    constexpr uint32_t op_accept_data = 94;
    constexpr uint32_t op_crypt = 96;
    constexpr uint32_t op_crypt_callback = 97;
    constexpr uint32_t op_cond_accept = 98;
    constexpr uint32_t op_fetch_scroll = 112;
    constexpr uint32_t op_info_cursor = 113;
    
    // Protocol versions
    constexpr uint32_t PROTOCOL_VERSION10 = 10;
    constexpr uint32_t PROTOCOL_VERSION11 = 11;
    constexpr uint32_t PROTOCOL_VERSION12 = 12;
    constexpr uint32_t PROTOCOL_VERSION13 = 13;
    constexpr uint32_t PROTOCOL_VERSION14 = 14;
    constexpr uint32_t PROTOCOL_VERSION15 = 15;
    constexpr uint32_t PROTOCOL_VERSION16 = 16;
    constexpr uint32_t PROTOCOL_VERSION18 = 18;

    // Protocol type / flags
    constexpr uint32_t ptype_batch_send = 2;
    constexpr uint32_t ptype_lazy_send = 5;

    // Fetch operations
    constexpr uint32_t fetch_next = 0;
    constexpr uint32_t fetch_prior = 1;
    constexpr uint32_t fetch_first = 2;
    constexpr uint32_t fetch_last = 3;
    constexpr uint32_t fetch_absolute = 4;
    constexpr uint32_t fetch_relative = 5;
    
    // Generic codes
    constexpr uint32_t GENERIC_ERROR = 1;
    // Status vector arguments
    constexpr uint32_t isc_arg_end = 0;
    constexpr uint32_t isc_arg_gds = 1;
    constexpr uint32_t isc_arg_string = 2;
    constexpr uint32_t isc_arg_sql_state = 19;

    // Common GDS codes
    constexpr uint32_t isc_unavailable = 335544375;
    constexpr uint32_t isc_dsql_error = 335544569;
    constexpr uint32_t isc_sqlerr = 335544436;
    constexpr uint32_t isc_login = 335544472;
}

// XDR helpers
static uint32_t xdrReadUint32(const uint8_t* data) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

static void xdrWriteUint32(uint8_t* data, uint32_t value) {
    data[0] = (value >> 24) & 0xFF;
    data[1] = (value >> 16) & 0xFF;
    data[2] = (value >> 8) & 0xFF;
    data[3] = value & 0xFF;
}

static uint16_t xdrReadUint16(const uint8_t* data) {
    return (data[0] << 8) | data[1];
}

static void xdrWriteUint16(uint8_t* data, uint16_t value) {
    data[0] = (value >> 8) & 0xFF;
    data[1] = value & 0xFF;
}

static void xdrAppendUint32(std::vector<uint8_t>& out, uint32_t value) {
    uint8_t buf[4];
    xdrWriteUint32(buf, value);
    out.insert(out.end(), buf, buf + 4);
}

static void xdrAppendInt32(std::vector<uint8_t>& out, int32_t value) {
    xdrAppendUint32(out, static_cast<uint32_t>(value));
}

static void xdrAppendInt64(std::vector<uint8_t>& out, int64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<uint8_t>((static_cast<uint64_t>(value) >> shift) & 0xFF));
    }
}

static void xdrAppendBuffer(std::vector<uint8_t>& out, const uint8_t* data, size_t len) {
    xdrAppendUint32(out, static_cast<uint32_t>(len));
    if (len > 0 && data) {
        out.insert(out.end(), data, data + len);
    }
    while (out.size() % 4 != 0) {
        out.push_back(0);
    }
}

static void xdrAppendString(std::vector<uint8_t>& out, const std::string& value) {
    xdrAppendBuffer(out, reinterpret_cast<const uint8_t*>(value.data()), value.size());
}

static std::string engineErrorMessage(const IPCMessage& response) {
    auto* error = response.getPayload<IPCErrorPayload>();
    if (error != nullptr && error->message[0] != '\0') {
        return std::string(error->message);
    }
    return "Engine returned an unspecified error";
}

static std::string firebirdPortalName(uint32_t request_handle) {
    return "fb_portal_" + std::to_string(request_handle);
}

static std::string firebirdDsqlPortalName(uint32_t statement_handle) {
    return "fb_dsql_portal_" + std::to_string(statement_handle);
}

struct FBSqldaRelationBinding {
    std::string relation_schema;
    std::string relation_name;
    std::string relation_alias;
};

static auto trimAscii(std::string value) -> std::string {
    const auto first = std::find_if_not(value.begin(),
                                        value.end(),
                                        [](unsigned char ch) { return std::isspace(ch) != 0; });
    const auto last = std::find_if_not(value.rbegin(),
                                       value.rend(),
                                       [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

static auto normalizeFirebirdDpbString(std::string value) -> std::string {
    value = trimAscii(std::move(value));
    if (value.size() < 2) {
        return value;
    }

    const char quote = value.front();
    if ((quote != '\'' && quote != '"') || value.back() != quote) {
        return value;
    }

    std::string out;
    out.reserve(value.size() - 2);
    for (size_t i = 1; i + 1 < value.size(); ++i) {
        const char ch = value[i];
        if (ch == quote && i + 2 < value.size() && value[i + 1] == quote) {
            out.push_back(quote);
            ++i;
            continue;
        }
        out.push_back(ch);
    }
    return out;
}

static auto splitFirebirdSqlBatch(std::string_view sql) -> std::vector<std::string> {
    enum class ScanState {
        Normal,
        SingleQuote,
        DoubleQuote,
        LineComment,
        BlockComment,
    };

    std::vector<std::string> statements;
    std::string current;
    current.reserve(sql.size());
    ScanState state = ScanState::Normal;

    for (size_t i = 0; i < sql.size(); ++i) {
        const char ch = sql[i];
        const char next = (i + 1 < sql.size()) ? sql[i + 1] : '\0';

        switch (state) {
            case ScanState::Normal:
                if (ch == '-' && next == '-') {
                    current.push_back(ch);
                    current.push_back(next);
                    ++i;
                    state = ScanState::LineComment;
                    continue;
                }
                if (ch == '/' && next == '*') {
                    current.push_back(ch);
                    current.push_back(next);
                    ++i;
                    state = ScanState::BlockComment;
                    continue;
                }
                if (ch == '\'') {
                    current.push_back(ch);
                    state = ScanState::SingleQuote;
                    continue;
                }
                if (ch == '"') {
                    current.push_back(ch);
                    state = ScanState::DoubleQuote;
                    continue;
                }
                if (ch == ';') {
                    std::string statement = trimAscii(current);
                    if (!statement.empty()) {
                        statements.push_back(std::move(statement));
                    }
                    current.clear();
                    continue;
                }
                current.push_back(ch);
                break;

            case ScanState::SingleQuote:
                current.push_back(ch);
                if (ch == '\'' && next == '\'') {
                    current.push_back(next);
                    ++i;
                    continue;
                }
                if (ch == '\'') {
                    state = ScanState::Normal;
                }
                break;

            case ScanState::DoubleQuote:
                current.push_back(ch);
                if (ch == '"' && next == '"') {
                    current.push_back(next);
                    ++i;
                    continue;
                }
                if (ch == '"') {
                    state = ScanState::Normal;
                }
                break;

            case ScanState::LineComment:
                current.push_back(ch);
                if (ch == '\n') {
                    state = ScanState::Normal;
                }
                break;

            case ScanState::BlockComment:
                current.push_back(ch);
                if (ch == '*' && next == '/') {
                    current.push_back(next);
                    ++i;
                    state = ScanState::Normal;
                }
                break;
        }
    }

    std::string trailing = trimAscii(current);
    if (!trailing.empty()) {
        statements.push_back(std::move(trailing));
    }

    return statements;
}

static auto upperAscii(std::string value) -> std::string {
    for (char& ch : value) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return value;
}

static auto identifiersEqual(std::string_view lhs, std::string_view rhs) -> bool {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (size_t i = 0; i < lhs.size(); ++i) {
        if (std::toupper(static_cast<unsigned char>(lhs[i])) !=
            std::toupper(static_cast<unsigned char>(rhs[i]))) {
            return false;
        }
    }
    return true;
}

static auto poolString(const parser::v3::StringPool& pool,
                       parser::v3::StringPool::StringId id) -> std::string {
    return std::string(pool.get(id));
}

static auto schemaPathObjectName(const parser::v3::SchemaPath& path,
                                 const parser::v3::StringPool& pool) -> std::string {
    return path.components.empty() ? std::string() : poolString(pool, path.components.back());
}

static auto schemaPathSchemaName(const parser::v3::SchemaPath& path,
                                 const parser::v3::StringPool& pool) -> std::string {
    if (path.components.size() <= 1) {
        return {};
    }
    std::string out;
    for (size_t i = 0; i + 1 < path.components.size(); ++i) {
        if (!out.empty()) {
            out.push_back('.');
        }
        out += poolString(pool, path.components[i]);
    }
    return out;
}

static auto appendDefaultSqldaField(std::vector<FBMessageFieldDesc>& fields_out,
                                    std::vector<FBSqldaVarDesc>& sqlda_out) -> void {
    FBMessageFieldDesc value_field;
    value_field.sql_type_override = 32766; // SQL_NULL
    fields_out.push_back(value_field);

    FBMessageFieldDesc null_field;
    null_field.type_opcode = 7; // blr_short
    null_field.length = 2;
    null_field.not_nullable = true;
    null_field.sql_type_override = 500; // SQL_SHORT
    fields_out.push_back(null_field);

    sqlda_out.emplace_back();
}

static void collectRelationBinding(const parser::v3::TableRefNode* node,
                                   const parser::v3::StringPool& pool,
                                   std::vector<FBSqldaRelationBinding>& out) {
    if (node == nullptr) {
        return;
    }

    FBSqldaRelationBinding binding;
    if (node->ref_type == parser::v3::TableRefNode::Type::TABLE) {
        binding.relation_name = schemaPathObjectName(node->table_path, pool);
        binding.relation_schema = schemaPathSchemaName(node->table_path, pool);
    }
    if (node->has_alias) {
        binding.relation_alias = poolString(pool, node->alias);
    }
    else {
        binding.relation_alias = binding.relation_name;
    }

    if (!binding.relation_name.empty() || !binding.relation_alias.empty()) {
        out.push_back(std::move(binding));
    }
}

static auto resolveColumnSqlda(const parser::v3::ColumnRef& column,
                               const parser::v3::StringPool& pool,
                               const std::vector<FBSqldaRelationBinding>& sources) -> FBSqldaVarDesc {
    FBSqldaVarDesc desc;
    desc.field_name = poolString(pool, column.column_name);
    desc.alias_name = desc.field_name;

    if (!column.has_table_qualifier) {
        if (sources.size() == 1) {
            desc.relation_schema = sources.front().relation_schema;
            desc.relation_name = sources.front().relation_name;
            desc.relation_alias = sources.front().relation_alias;
        }
        return desc;
    }

    const std::string qualifier = schemaPathObjectName(column.table_path, pool);
    for (const auto& source : sources) {
        if ((!source.relation_alias.empty() && identifiersEqual(source.relation_alias, qualifier)) ||
            (!source.relation_name.empty() && identifiersEqual(source.relation_name, qualifier))) {
            desc.relation_schema = source.relation_schema;
            desc.relation_name = source.relation_name;
            desc.relation_alias = source.relation_alias;
            return desc;
        }
    }

    desc.relation_schema = schemaPathSchemaName(column.table_path, pool);
    desc.relation_name = qualifier;
    desc.relation_alias = qualifier;
    return desc;
}

static auto describeExpressionSqlda(const parser::v3::Expression* expr,
                                    const parser::v3::StringPool& pool,
                                    const std::vector<FBSqldaRelationBinding>& sources) -> FBSqldaVarDesc {
    if (expr == nullptr) {
        return {};
    }

    if (expr->kind() == parser::v3::ASTKind::ColumnRefExpr) {
        const auto* column_expr = static_cast<const parser::v3::ColumnRefExpr*>(expr);
        return resolveColumnSqlda(column_expr->column, pool, sources);
    }

    return {};
}

static void collectSqldaForSelectItems(const std::vector<parser::v3::SelectItem*>& items,
                                       const parser::v3::StringPool& pool,
                                       const std::vector<FBSqldaRelationBinding>& sources,
                                       std::vector<FBSqldaVarDesc>& sqlda_out,
                                       std::vector<FBMessageFieldDesc>& fields_out) {
    for (const auto* item : items) {
        if (item == nullptr) {
            continue;
        }
        if (item->item_type != parser::v3::SelectItem::Type::EXPRESSION) {
            continue;
        }

        FBSqldaVarDesc desc = describeExpressionSqlda(item->expr, pool, sources);
        if (item->has_alias) {
            desc.alias_name = poolString(pool, item->alias);
        }
        appendDefaultSqldaField(fields_out, sqlda_out);
        sqlda_out.back() = std::move(desc);
    }
}

static void collectSqldaSources(const parser::v3::TableRefNode* from,
                                const std::vector<parser::v3::JoinNode*>& joins,
                                const parser::v3::StringPool& pool,
                                std::vector<FBSqldaRelationBinding>& sources_out) {
    collectRelationBinding(from, pool, sources_out);
    for (const auto* join : joins) {
        if (join == nullptr) {
            continue;
        }
        collectRelationBinding(join->right, pool, sources_out);
    }
}

static void collectParameterSlotsFromExpression(const parser::v3::Expression* expr,
                                                std::vector<FBMessageFieldDesc>& fields_out,
                                                std::vector<FBSqldaVarDesc>& sqlda_out) {
    if (expr == nullptr) {
        return;
    }

    using namespace scratchbird::parser::v3;

    switch (expr->kind()) {
        case ASTKind::ParameterExpr:
            appendDefaultSqldaField(fields_out, sqlda_out);
            return;
        case ASTKind::BinaryExpr:
        {
            const auto* binary = static_cast<const BinaryExpr*>(expr);
            collectParameterSlotsFromExpression(binary->left, fields_out, sqlda_out);
            collectParameterSlotsFromExpression(binary->right, fields_out, sqlda_out);
            return;
        }
        case ASTKind::UnaryExpr:
        {
            const auto* unary = static_cast<const UnaryExpr*>(expr);
            collectParameterSlotsFromExpression(unary->operand, fields_out, sqlda_out);
            return;
        }
        case ASTKind::FunctionCallExpr:
        {
            const auto* fn = static_cast<const FunctionCallExpr*>(expr);
            for (const auto* arg : fn->arguments) {
                collectParameterSlotsFromExpression(arg, fields_out, sqlda_out);
            }
            collectParameterSlotsFromExpression(fn->filter, fields_out, sqlda_out);
            for (const auto* order_item : fn->order_by) {
                if (order_item != nullptr) {
                    collectParameterSlotsFromExpression(order_item->expr, fields_out, sqlda_out);
                }
            }
            if (fn->window != nullptr) {
                for (const auto* part : fn->window->partition_by) {
                    collectParameterSlotsFromExpression(part, fields_out, sqlda_out);
                }
                for (const auto* order_item : fn->window->order_by) {
                    if (order_item != nullptr) {
                        collectParameterSlotsFromExpression(order_item->expr, fields_out, sqlda_out);
                    }
                }
                collectParameterSlotsFromExpression(fn->window->frame_start_value, fields_out, sqlda_out);
                collectParameterSlotsFromExpression(fn->window->frame_end_value, fields_out, sqlda_out);
            }
            return;
        }
        case ASTKind::CastExpr:
        {
            const auto* cast_expr = static_cast<const CastExpr*>(expr);
            collectParameterSlotsFromExpression(cast_expr->expr, fields_out, sqlda_out);
            return;
        }
        case ASTKind::ExtractExpr:
        {
            const auto* extract_expr = static_cast<const ExtractExpr*>(expr);
            collectParameterSlotsFromExpression(extract_expr->source, fields_out, sqlda_out);
            collectParameterSlotsFromExpression(extract_expr->selector.expr, fields_out, sqlda_out);
            for (const auto* arg : extract_expr->selector.args) {
                collectParameterSlotsFromExpression(arg, fields_out, sqlda_out);
            }
            return;
        }
        case ASTKind::AlterElementExpr:
        {
            const auto* alter_expr = static_cast<const AlterElementExpr*>(expr);
            collectParameterSlotsFromExpression(alter_expr->source, fields_out, sqlda_out);
            collectParameterSlotsFromExpression(alter_expr->new_value, fields_out, sqlda_out);
            collectParameterSlotsFromExpression(alter_expr->selector.expr, fields_out, sqlda_out);
            for (const auto* arg : alter_expr->selector.args) {
                collectParameterSlotsFromExpression(arg, fields_out, sqlda_out);
            }
            return;
        }
        default:
            return;
    }
}

static void collectPreparedSqldaFromStatement(const parser::v3::Statement* statement,
                                              const parser::v3::StringPool& pool,
                                              std::vector<FBSqldaVarDesc>& input_sqlda_out,
                                              std::vector<FBMessageFieldDesc>& input_fields_out,
                                              std::vector<FBSqldaVarDesc>& output_sqlda_out,
                                              std::vector<FBMessageFieldDesc>& output_fields_out) {
    if (statement == nullptr) {
        return;
    }

    using namespace scratchbird::parser::v3;

    switch (statement->kind()) {
        case ASTKind::SelectStmt:
        {
            const auto* select = static_cast<const SelectStmt*>(statement);
            std::vector<FBSqldaRelationBinding> sources;
            collectSqldaSources(select->from, select->joins, pool, sources);
            collectSqldaForSelectItems(select->items, pool, sources, output_sqlda_out, output_fields_out);
            for (const auto* item : select->items) {
                if (item != nullptr) {
                    collectParameterSlotsFromExpression(item->expr, input_fields_out, input_sqlda_out);
                }
            }
            collectParameterSlotsFromExpression(select->where, input_fields_out, input_sqlda_out);
            collectParameterSlotsFromExpression(select->having, input_fields_out, input_sqlda_out);
            collectParameterSlotsFromExpression(select->limit, input_fields_out, input_sqlda_out);
            collectParameterSlotsFromExpression(select->offset, input_fields_out, input_sqlda_out);
            collectParameterSlotsFromExpression(select->fetch_row_count, input_fields_out, input_sqlda_out);
            for (const auto* expr : select->distinct_on) {
                collectParameterSlotsFromExpression(expr, input_fields_out, input_sqlda_out);
            }
            for (const auto* expr : select->group_by) {
                collectParameterSlotsFromExpression(expr, input_fields_out, input_sqlda_out);
            }
            for (const auto* order_item : select->order_by) {
                if (order_item != nullptr) {
                    collectParameterSlotsFromExpression(order_item->expr, input_fields_out, input_sqlda_out);
                }
            }
            for (const auto* join : select->joins) {
                if (join != nullptr) {
                    collectParameterSlotsFromExpression(join->on_condition, input_fields_out, input_sqlda_out);
                }
            }
            return;
        }
        case ASTKind::InsertStmt:
        {
            const auto* insert = static_cast<const InsertStmt*>(statement);
            std::vector<FBSqldaRelationBinding> sources;
            if (!insert->table_path.components.empty()) {
                FBSqldaRelationBinding binding;
                binding.relation_name = schemaPathObjectName(insert->table_path, pool);
                binding.relation_schema = schemaPathSchemaName(insert->table_path, pool);
                binding.relation_alias = insert->has_alias ? poolString(pool, insert->alias) : binding.relation_name;
                sources.push_back(std::move(binding));
            }
            collectSqldaForSelectItems(insert->returning, pool, sources, output_sqlda_out, output_fields_out);
            for (const auto& row : insert->values_rows) {
                for (const auto* expr : row) {
                    collectParameterSlotsFromExpression(expr, input_fields_out, input_sqlda_out);
                }
            }
            return;
        }
        case ASTKind::UpdateStmt:
        {
            const auto* update = static_cast<const UpdateStmt*>(statement);
            std::vector<FBSqldaRelationBinding> sources;
            if (!update->table_path.components.empty()) {
                FBSqldaRelationBinding binding;
                binding.relation_name = schemaPathObjectName(update->table_path, pool);
                binding.relation_schema = schemaPathSchemaName(update->table_path, pool);
                binding.relation_alias = update->has_alias ? poolString(pool, update->alias) : binding.relation_name;
                sources.push_back(std::move(binding));
            }
            collectSqldaForSelectItems(update->returning, pool, sources, output_sqlda_out, output_fields_out);
            for (const auto& item : update->set_items) {
                collectParameterSlotsFromExpression(item.second, input_fields_out, input_sqlda_out);
            }
            collectParameterSlotsFromExpression(update->where, input_fields_out, input_sqlda_out);
            return;
        }
        case ASTKind::DeleteStmt:
        {
            const auto* del = static_cast<const DeleteStmt*>(statement);
            std::vector<FBSqldaRelationBinding> sources;
            if (!del->table_path.components.empty()) {
                FBSqldaRelationBinding binding;
                binding.relation_name = schemaPathObjectName(del->table_path, pool);
                binding.relation_schema = schemaPathSchemaName(del->table_path, pool);
                binding.relation_alias = del->has_alias ? poolString(pool, del->alias) : binding.relation_name;
                sources.push_back(std::move(binding));
            }
            collectSqldaForSelectItems(del->returning, pool, sources, output_sqlda_out, output_fields_out);
            collectParameterSlotsFromExpression(del->where, input_fields_out, input_sqlda_out);
            return;
        }
        default:
            return;
    }
}

static uint32_t inferFirebirdStatementType(const std::string& sql_text) {
    const auto first = std::find_if_not(sql_text.begin(),
                                        sql_text.end(),
                                        [](unsigned char ch) { return std::isspace(ch) != 0; });
    std::string normalized;
    normalized.reserve(static_cast<size_t>(sql_text.end() - first));
    for (auto it = first; it != sql_text.end(); ++it) {
        normalized.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(*it))));
    }
    const auto starts_with = [&](const char* prefix) {
        return normalized.rfind(prefix, 0) == 0;
    };
    if (starts_with("SELECT")) return 1;
    if (starts_with("INSERT")) return 2;
    if (starts_with("UPDATE")) return 3;
    if (starts_with("DELETE")) return 4;
    if (starts_with("EXECUTE PROCEDURE")) return 8;
    if (starts_with("SET GENERATOR")) return 13;
    if (starts_with("SAVEPOINT")) return 14;
    if (starts_with("COMMIT")) return 10;
    if (starts_with("ROLLBACK")) return 11;
    if (starts_with("CREATE") || starts_with("ALTER") || starts_with("DROP") || starts_with("RECREATE")) {
        return 5;
    }
    return 5;
}

static uint32_t inferFirebirdStatementFlags(const FBDsqlStatementState& stmt_state) {
    constexpr uint32_t FLAG_HAS_CURSOR = 0x01;
    constexpr uint32_t FLAG_REPEAT_EXECUTE = 0x02;

    uint32_t flags = FLAG_REPEAT_EXECUTE;
    switch (inferFirebirdStatementType(stmt_state.sql_text)) {
        case 5:  // DDL / create db
            flags &= ~FLAG_REPEAT_EXECUTE;
            break;
        case 1:  // SELECT
            if (!stmt_state.output_message_fields.empty()) {
                flags |= FLAG_HAS_CURSOR;
            }
            break;
        default:
            break;
    }
    return flags;
}

static uint32_t inferFirebirdBatchFetchSupport(const FBDsqlStatementState& stmt_state) {
    switch (inferFirebirdStatementType(stmt_state.sql_text)) {
        case 1:  // SELECT
            return stmt_state.output_message_fields.empty() ? 0u : 1u;
        default:
            return 0u;
    }
}

static void appendFirebirdInfoItemInt(std::vector<uint8_t>& out, uint8_t item, uint32_t value);
static void appendFirebirdInfoItemBytes(std::vector<uint8_t>& out,
                                        uint8_t item,
                                        const uint8_t* data,
                                        uint16_t length);

static void appendFirebirdSqlRecordsInfo(std::vector<uint8_t>& out,
                                         const FBDsqlStatementState& stmt_state) {
    std::vector<uint8_t> payload;
    appendFirebirdInfoItemInt(payload, 13, static_cast<uint32_t>(std::min<uint64_t>(
                                           stmt_state.select_count,
                                           0xFFFFFFFFull))); // isc_info_req_select_count
    appendFirebirdInfoItemInt(payload, 14, static_cast<uint32_t>(std::min<uint64_t>(
                                           stmt_state.insert_count,
                                           0xFFFFFFFFull))); // isc_info_req_insert_count
    appendFirebirdInfoItemInt(payload, 15, static_cast<uint32_t>(std::min<uint64_t>(
                                           stmt_state.update_count,
                                           0xFFFFFFFFull))); // isc_info_req_update_count
    appendFirebirdInfoItemInt(payload, 16, static_cast<uint32_t>(std::min<uint64_t>(
                                           stmt_state.delete_count,
                                           0xFFFFFFFFull))); // isc_info_req_delete_count
    payload.push_back(1); // isc_info_end
    appendFirebirdInfoItemBytes(out,
                                23, // isc_info_sql_records
                                payload.data(),
                                static_cast<uint16_t>(payload.size()));
}

static void appendFirebirdInfoItemInt(std::vector<uint8_t>& out, uint8_t item, uint32_t value) {
    out.push_back(item);
    const uint16_t length = value <= 0xFF ? 1 : value <= 0xFFFF ? 2 : value <= 0xFFFFFF ? 3 : 4;
    out.push_back(static_cast<uint8_t>(length & 0xFF));
    out.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
    for (uint16_t i = 0; i < length; ++i) {
        out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
    }
}

static void appendFirebirdInfoItemSigned(std::vector<uint8_t>& out, uint8_t item, int32_t value) {
    appendFirebirdInfoItemInt(out, item, static_cast<uint32_t>(value));
}

static void appendFirebirdInfoItemString(std::vector<uint8_t>& out, uint8_t item, const std::string& value) {
    out.push_back(item);
    const uint16_t length = static_cast<uint16_t>(value.size());
    out.push_back(static_cast<uint8_t>(length & 0xFF));
    out.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
    out.insert(out.end(), value.begin(), value.end());
}

static void appendFirebirdInfoItemInt64(std::vector<uint8_t>& out, uint8_t item, int64_t value) {
    out.push_back(item);
    if (value >= std::numeric_limits<int32_t>::min() && value <= std::numeric_limits<int32_t>::max()) {
        out.push_back(4);
        out.push_back(0);
        const uint32_t encoded = static_cast<uint32_t>(static_cast<int32_t>(value));
        for (uint16_t i = 0; i < 4; ++i) {
            out.push_back(static_cast<uint8_t>((encoded >> (i * 8)) & 0xFF));
        }
        return;
    }

    out.push_back(8);
    out.push_back(0);
    const uint64_t encoded = static_cast<uint64_t>(value);
    for (uint16_t i = 0; i < 8; ++i) {
        out.push_back(static_cast<uint8_t>((encoded >> (i * 8)) & 0xFF));
    }
}

static void appendFirebirdInfoItemBytes(std::vector<uint8_t>& out,
                                        uint8_t item,
                                        const uint8_t* data,
                                        uint16_t length) {
    out.push_back(item);
    out.push_back(static_cast<uint8_t>(length & 0xFF));
    out.push_back(static_cast<uint8_t>((length >> 8) & 0xFF));
    out.insert(out.end(), data, data + length);
}

static void appendFirebirdInfoItemCountedString(std::vector<uint8_t>& out,
                                                uint8_t item,
                                                const std::string& value) {
    const uint8_t string_length =
        static_cast<uint8_t>(std::min<size_t>(value.size(), std::numeric_limits<uint8_t>::max()));
    out.push_back(item);
    out.push_back(static_cast<uint8_t>((string_length + 2u) & 0xFF));
    out.push_back(static_cast<uint8_t>(((string_length + 2u) >> 8) & 0xFF));
    out.push_back(1); // one string entry
    out.push_back(string_length);
    out.insert(out.end(), value.begin(), value.begin() + string_length);
}

static void appendFirebirdInfoError(std::vector<uint8_t>& out,
                                    uint8_t requested_item,
                                    uint32_t error_code) {
    std::vector<uint8_t> payload;
    payload.push_back(requested_item);
    const uint16_t code_length =
        error_code <= 0xFF ? 1 : error_code <= 0xFFFF ? 2 : error_code <= 0xFFFFFF ? 3 : 4;
    for (uint16_t i = 0; i < code_length; ++i) {
        payload.push_back(static_cast<uint8_t>((error_code >> (i * 8)) & 0xFF));
    }

    out.push_back(3); // isc_info_error
    const uint16_t payload_length = static_cast<uint16_t>(payload.size());
    out.push_back(static_cast<uint8_t>(payload_length & 0xFF));
    out.push_back(static_cast<uint8_t>((payload_length >> 8) & 0xFF));
    out.insert(out.end(), payload.begin(), payload.end());
}

static bool appendFirebirdInfoChunk(std::vector<uint8_t>& out,
                                    const std::vector<uint8_t>& chunk,
                                    uint32_t buffer_length,
                                    bool& truncated) {
    if (truncated) {
        return false;
    }

    if (buffer_length == 0) {
        out.insert(out.end(), chunk.begin(), chunk.end());
        return true;
    }

    const size_t limit = static_cast<size_t>(buffer_length - 1u);
    if (out.size() + chunk.size() > limit) {
        truncated = true;
        return false;
    }

    out.insert(out.end(), chunk.begin(), chunk.end());
    return true;
}

static std::optional<std::string> truncateFirebirdPlanText(const std::string& plan_text,
                                                           size_t max_payload_length) {
    if (plan_text.size() <= max_payload_length) {
        return plan_text;
    }

    if (max_payload_length < 4) {
        return std::nullopt;
    }

    std::string truncated = plan_text.substr(0, max_payload_length);
    while (truncated.size() > max_payload_length - 4) {
        const size_t pos = truncated.find_last_of(' ');
        if (pos == std::string::npos) {
            break;
        }
        truncated.resize(pos);
    }

    truncated += " ...";
    if (truncated.size() > max_payload_length) {
        return std::nullopt;
    }

    return truncated;
}

static std::string buildFirebirdExecPathText(const std::vector<uint8_t>& bytecode) {
    if (bytecode.empty()) {
        return {};
    }

    scratchbird::sblr::v3::Container container;
    std::string container_error;
    if (!scratchbird::sblr::v3::decodeContainer(bytecode.data(),
                                                bytecode.size(),
                                                container,
                                                container_error)) {
        return {};
    }

    std::ostringstream out;
    size_t offset = 0;
    while (offset < container.bytecode_stream.size()) {
        const size_t instruction_offset = offset;
        scratchbird::sblr::v3::Instruction inst;
        scratchbird::sblr::v3::DecodeError decode_error;
        if (!scratchbird::sblr::v3::decodeInstructionWithSchema(container.bytecode_stream.data(),
                                                                container.bytecode_stream.size(),
                                                                offset,
                                                                inst,
                                                                decode_error)) {
            break;
        }

        const char* opcode_name = scratchbird::sblr::v3::opcodeName(inst.opcode);
        out << std::setw(5) << instruction_offset << ' '
            << (opcode_name != nullptr ? opcode_name : "UNKNOWN") << '\n';
    }

    return out.str();
}

static std::string extractFirebirdPlanText(const std::vector<uint8_t>& bytecode) {
    if (bytecode.empty()) {
        return {};
    }

    scratchbird::sblr::v3::Container container;
    std::string container_error;
    if (!scratchbird::sblr::v3::decodeContainer(bytecode.data(),
                                                bytecode.size(),
                                                container,
                                                container_error)) {
        return {};
    }

    auto extract_from_object =
        [](const scratchbird::sblr::v3::Value::Object& obj) -> std::string {
            auto plan_text_it = obj.find("plan_text");
            if (plan_text_it != obj.end()) {
                if (const auto* plan_text =
                        std::get_if<std::string>(&plan_text_it->second.data)) {
                    return *plan_text;
                }
            }
            auto query_it = obj.find("query");
            if (query_it != obj.end()) {
                if (const auto* query_obj =
                        std::get_if<scratchbird::sblr::v3::Value::Object>(
                            &query_it->second.data)) {
                    auto nested_plan_text_it = query_obj->find("plan_text");
                    if (nested_plan_text_it != query_obj->end()) {
                        if (const auto* nested_plan_text =
                                std::get_if<std::string>(
                                    &nested_plan_text_it->second.data)) {
                            return *nested_plan_text;
                        }
                    }
                }
            }
            return {};
        };

    size_t offset = 0;
    while (offset < container.bytecode_stream.size()) {
        scratchbird::sblr::v3::Instruction inst;
        scratchbird::sblr::v3::DecodeError decode_error;
        if (!scratchbird::sblr::v3::decodeInstructionWithSchema(container.bytecode_stream.data(),
                                                                container.bytecode_stream.size(),
                                                                offset,
                                                                inst,
                                                                decode_error)) {
            break;
        }
        if (const auto* obj =
                std::get_if<scratchbird::sblr::v3::Value::Object>(&inst.payload.data)) {
            const std::string plan_text = extract_from_object(*obj);
            if (!plan_text.empty()) {
                return plan_text;
            }
        }
    }

    return {};
}

static auto firebirdBlrFieldSqlType(const FBMessageFieldDesc& field) -> uint32_t {
    if (field.sql_type_override != 0) {
        return field.sql_type_override;
    }

    switch (field.type_opcode) {
        case 7: return 500;   // SQL_SHORT
        case 8: return 496;   // SQL_LONG
        case 9: return 550;   // SQL_QUAD
        case 10: return 482;  // SQL_FLOAT
        case 11: return 530;  // SQL_D_FLOAT
        case 12: return 570;  // SQL_TYPE_DATE
        case 13: return 560;  // SQL_TYPE_TIME
        case 14:
        case 15: return 452;  // SQL_TEXT
        case 16: return 580;  // SQL_INT64
        case 17: return 520;  // SQL_BLOB
        case 23: return 32764; // SQL_BOOLEAN
        case 24: return 32760; // SQL_DEC16
        case 25: return 32762; // SQL_DEC34
        case 26: return 32752; // SQL_INT128
        case 27: return 480;  // SQL_DOUBLE
        case 28: return 32756; // SQL_TIME_TZ
        case 29: return 32754; // SQL_TIMESTAMP_TZ
        case 30: return 32750; // SQL_TIME_TZ_EX
        case 31: return 32748; // SQL_TIMESTAMP_TZ_EX
        case 35: return 510;  // SQL_TIMESTAMP
        case 37:
        case 38:
        case 40:
        case 41: return 448;  // SQL_VARYING
        default: return 32766; // SQL_NULL fail-closed placeholder
    }
}

static auto firebirdBlrFieldSqlLength(const FBMessageFieldDesc& field) -> uint32_t {
    switch (field.type_opcode) {
        case 37:
        case 38:
            return field.length >= 2 ? static_cast<uint32_t>(field.length - 2) : 0;
        case 40:
        case 41:
            return field.length > 0 ? static_cast<uint32_t>(field.length - 1) : 0;
        default:
            return field.length;
    }
}

static auto appendFirebirdDescribeVarInfo(std::vector<uint8_t>& out,
                                          const std::vector<FBMessageFieldDesc>& fields,
                                          const std::vector<FBSqldaVarDesc>* sqlda_fields,
                                          const std::vector<std::string>* late_field_names,
                                          uint8_t message_number,
                                          uint16_t first_index,
                                          const std::vector<uint8_t>& describe_items) -> void {
    if (fields.size() % 2 != 0) {
        return;
    }
    const size_t column_count = fields.size() / 2;
    for (size_t i = 0; i < column_count; ++i) {
        const uint16_t seq = static_cast<uint16_t>(i + 1);
        if (seq < first_index) {
            continue;
        }
        const auto& value_field = fields[i * 2];
        uint32_t sql_type = firebirdBlrFieldSqlType(value_field);
        if (!value_field.not_nullable) {
            sql_type |= 1u;
        }
        for (uint8_t item : describe_items) {
            switch (item) {
                case 8: // isc_info_sql_describe_end
                    break;
                case 9: // isc_info_sql_sqlda_seq
                    appendFirebirdInfoItemInt(out, item, seq);
                    break;
                case 10: // isc_info_sql_message_seq
                    appendFirebirdInfoItemInt(out, item, 0);
                    break;
                case 11: // isc_info_sql_type
                    appendFirebirdInfoItemInt(out, item, sql_type);
                    break;
                case 12: // isc_info_sql_sub_type
                    appendFirebirdInfoItemInt(out, item, value_field.subtype);
                    break;
                case 13: // isc_info_sql_scale
                    appendFirebirdInfoItemSigned(out, item, value_field.scale);
                    break;
                case 14: // isc_info_sql_length
                    appendFirebirdInfoItemInt(out, item, firebirdBlrFieldSqlLength(value_field));
                    break;
                case 15: // isc_info_sql_null_ind
                    appendFirebirdInfoItemInt(out, item, value_field.not_nullable ? 0u : 1u);
                    break;
                case 16: // isc_info_sql_field
                {
                    std::string value;
                    if (sqlda_fields != nullptr && i < sqlda_fields->size()) {
                        value = (*sqlda_fields)[i].field_name;
                    }
                    if (value.empty() && late_field_names != nullptr && i < late_field_names->size()) {
                        value = (*late_field_names)[i];
                    }
                    appendFirebirdInfoItemString(out, item, value);
                    break;
                }
                case 17: // isc_info_sql_relation
                {
                    const std::string value =
                        (sqlda_fields != nullptr && i < sqlda_fields->size()) ? (*sqlda_fields)[i].relation_name
                                                                              : std::string();
                    appendFirebirdInfoItemString(out, item, value);
                    break;
                }
                case 18: // isc_info_sql_owner
                {
                    const std::string value =
                        (sqlda_fields != nullptr && i < sqlda_fields->size()) ? (*sqlda_fields)[i].owner_name
                                                                              : std::string();
                    appendFirebirdInfoItemString(out, item, value);
                    break;
                }
                case 19: // isc_info_sql_alias
                {
                    std::string value;
                    if (sqlda_fields != nullptr && i < sqlda_fields->size()) {
                        value = (*sqlda_fields)[i].alias_name;
                    }
                    if (value.empty() && late_field_names != nullptr && i < late_field_names->size()) {
                        value = (*late_field_names)[i];
                    }
                    appendFirebirdInfoItemString(out, item, value);
                    break;
                }
                case 25: // isc_info_sql_relation_alias
                {
                    const std::string value =
                        (sqlda_fields != nullptr && i < sqlda_fields->size()) ? (*sqlda_fields)[i].relation_alias
                                                                              : std::string();
                    appendFirebirdInfoItemString(out, item, value);
                    break;
                }
                case 33: // isc_info_sql_relation_schema
                {
                    const std::string value =
                        (sqlda_fields != nullptr && i < sqlda_fields->size()) ? (*sqlda_fields)[i].relation_schema
                                                                              : std::string();
                    appendFirebirdInfoItemString(out, item, value);
                    break;
                }
                default:
                    break;
            }
        }
        out.push_back(8); // isc_info_sql_describe_end
    }
}

static void buildFirebirdStatementInfoBuffer(const FBDsqlStatementState& stmt_state,
                                             const std::string& items,
                                             uint32_t buffer_length,
                                             std::vector<uint8_t>& info_out) {
    info_out.clear();
    uint16_t first_index = 1;
    const std::unordered_map<uint8_t, std::vector<FBMessageFieldDesc>>* active_layouts = nullptr;
    const std::vector<FBSqldaVarDesc>* active_sqlda = nullptr;
    bool truncated = false;

    for (size_t i = 0; i < items.size(); ++i) {
        const uint8_t item = static_cast<uint8_t>(items[i]);
        if (item == 0 || item == 1) { // isc_info_end or zero terminator
            break;
        }

        std::vector<uint8_t> item_out;

        switch (item) {
            case 20: // isc_info_sql_sqlda_start
            {
                if (i + 1 >= items.size()) {
                    break;
                }
                const uint8_t len = static_cast<uint8_t>(items[i + 1]);
                i += 1;
                uint32_t value = 0;
                for (uint16_t n = 0; n < len && i + 1 + n < items.size() && n < 4; ++n) {
                    value |= static_cast<uint32_t>(static_cast<uint8_t>(items[i + 1 + n])) << (n * 8);
                }
                first_index = static_cast<uint16_t>(std::max<uint32_t>(1u, value));
                i += len;
                break;
            }
            case 4: // isc_info_sql_select
                item_out.push_back(item);
                appendFirebirdInfoChunk(info_out, item_out, buffer_length, truncated);
                active_layouts = &stmt_state.output_message_fields;
                active_sqlda = &stmt_state.output_sqlda_fields;
                break;
            case 5: // isc_info_sql_bind
                item_out.push_back(item);
                appendFirebirdInfoChunk(info_out, item_out, buffer_length, truncated);
                active_layouts = &stmt_state.input_message_fields;
                active_sqlda = &stmt_state.input_sqlda_fields;
                break;
            case 6: // isc_info_sql_num_variables
            case 7: // isc_info_sql_describe_vars
            {
                const std::vector<FBMessageFieldDesc>* fields = nullptr;
                uint8_t message_number = 0;
                if (active_layouts != nullptr && !active_layouts->empty()) {
                    auto layout_it = std::min_element(
                        active_layouts->begin(),
                        active_layouts->end(),
                        [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
                    fields = &layout_it->second;
                    message_number = layout_it->first;
                }

                const uint32_t variable_count =
                    (fields != nullptr && fields->size() % 2 == 0)
                        ? static_cast<uint32_t>(fields->size() / 2)
                        : 0u;
                appendFirebirdInfoItemInt(item_out, item, variable_count);
                if (item == 6) {
                    appendFirebirdInfoChunk(info_out, item_out, buffer_length, truncated);
                    break;
                }
                if (fields == nullptr) {
                    item_out.push_back(8); // isc_info_sql_describe_end
                    appendFirebirdInfoChunk(info_out, item_out, buffer_length, truncated);
                    break;
                }

                std::vector<uint8_t> describe_items;
                size_t j = i + 1;
                while (j < items.size()) {
                    const uint8_t describe_item = static_cast<uint8_t>(items[j]);
                    if (describe_item == 1 || describe_item == 8) {
                        break;
                    }
                    describe_items.push_back(describe_item);
                    ++j;
                }

                const std::vector<std::string>* field_names = nullptr;
                if (active_layouts == &stmt_state.output_message_fields &&
                    !stmt_state.output_field_names.empty()) {
                    field_names = &stmt_state.output_field_names;
                }
                appendFirebirdDescribeVarInfo(item_out,
                                              *fields,
                                              active_sqlda,
                                              field_names,
                                              message_number,
                                              first_index,
                                              describe_items);
                appendFirebirdInfoChunk(info_out, item_out, buffer_length, truncated);
                if (j < items.size() && static_cast<uint8_t>(items[j]) == 8) {
                    i = j;
                }
                break;
            }
            case 21: // isc_info_sql_stmt_type
                appendFirebirdInfoItemInt(item_out,
                                          item,
                                          inferFirebirdStatementType(stmt_state.sql_text));
                appendFirebirdInfoChunk(info_out, item_out, buffer_length, truncated);
                break;
            case 22: // isc_info_sql_get_plan
            case 26: // isc_info_sql_explain_plan
            {
                const std::string plan_text =
                    extractFirebirdPlanText(stmt_state.compiled_bytecode);
                if (!plan_text.empty()) {
                    std::string rendered_plan = plan_text;
                    bool plan_was_truncated = false;
                    if (buffer_length > 0) {
                        const size_t limit = static_cast<size_t>(buffer_length - 1u);
                        const size_t remaining =
                            info_out.size() < limit ? limit - info_out.size() : 0u;
                        if (remaining < 3) {
                            truncated = true;
                            break;
                        }

                        const size_t max_payload = remaining - 3u;
                        auto maybe_truncated =
                            truncateFirebirdPlanText(plan_text, max_payload);
                        if (!maybe_truncated.has_value()) {
                            truncated = true;
                            break;
                        }

                        rendered_plan = *maybe_truncated;
                        if (rendered_plan.size() < plan_text.size()) {
                            plan_was_truncated = true;
                        }
                    }

                    appendFirebirdInfoItemString(item_out, item, rendered_plan);
                    appendFirebirdInfoChunk(info_out, item_out, buffer_length, truncated);
                    if (plan_was_truncated && !truncated) {
                        truncated = true;
                    }
                }
                else {
                    appendFirebirdInfoError(item_out, item, 335544341u); // isc_infunk
                    appendFirebirdInfoChunk(info_out, item_out, buffer_length, truncated);
                }
                break;
            }
            case 24: // isc_info_sql_batch_fetch
                appendFirebirdInfoItemInt(item_out,
                                          item,
                                          inferFirebirdBatchFetchSupport(stmt_state));
                appendFirebirdInfoChunk(info_out, item_out, buffer_length, truncated);
                break;
            case 23: // isc_info_sql_records
                appendFirebirdSqlRecordsInfo(item_out, stmt_state);
                appendFirebirdInfoChunk(info_out, item_out, buffer_length, truncated);
                break;
            case 28: // isc_info_sql_stmt_timeout_user
            case 29: // isc_info_sql_stmt_timeout_run
                appendFirebirdInfoItemInt(item_out, item, 0);
                appendFirebirdInfoChunk(info_out, item_out, buffer_length, truncated);
                break;
            case 30: // isc_info_sql_stmt_blob_align
                appendFirebirdInfoItemInt(item_out, item, 4);
                appendFirebirdInfoChunk(info_out, item_out, buffer_length, truncated);
                break;
            case 31: // isc_info_sql_exec_path_blr_bytes
                if (!stmt_state.compiled_bytecode.empty()) {
                    appendFirebirdInfoItemBytes(item_out,
                                                item,
                                                stmt_state.compiled_bytecode.data(),
                                                static_cast<uint16_t>(std::min<size_t>(
                                                    stmt_state.compiled_bytecode.size(),
                                                    std::numeric_limits<uint16_t>::max())));
                    appendFirebirdInfoChunk(info_out, item_out, buffer_length, truncated);
                }
                break;
            case 32: // isc_info_sql_exec_path_blr_text
            {
                const std::string text =
                    buildFirebirdExecPathText(stmt_state.compiled_bytecode);
                if (!text.empty()) {
                    appendFirebirdInfoItemString(item_out, item, text);
                    appendFirebirdInfoChunk(info_out, item_out, buffer_length, truncated);
                }
                break;
            }
            case 27: // isc_info_sql_stmt_flags
                appendFirebirdInfoItemInt(item_out,
                                          item,
                                          inferFirebirdStatementFlags(stmt_state));
                appendFirebirdInfoChunk(info_out, item_out, buffer_length, truncated);
                break;
            default:
                appendFirebirdInfoError(item_out, item, 335544341u); // isc_infunk
                appendFirebirdInfoChunk(info_out, item_out, buffer_length, truncated);
                break;
        }

        if (truncated) {
            break;
        }
    }

    if (buffer_length > 0) {
        info_out.push_back(truncated ? 2u : 1u); // isc_info_truncated / isc_info_end
    }
}

static auto firebirdRequestMessageSize(const std::vector<FBMessageFieldDesc>& fields) -> uint32_t {
    uint32_t total = 0;
    for (const auto& field : fields) {
        uint32_t wire_length = field.length;
        switch (field.type_opcode) {
            case 7:   // blr_short
            case 8:   // blr_long
            case 10:  // blr_float
            case 12:  // blr_sql_date
            case 13:  // blr_sql_time
            case 23:  // blr_bool
                wire_length = 4;
                break;
            case 16:  // blr_int64
            case 27:  // blr_double
            case 35:  // blr_timestamp
                wire_length = 8;
                break;
            default:
                break;
        }
        total += static_cast<uint32_t>((wire_length + 3u) & ~3u);
    }
    return total;
}

static auto firebirdRequestActiveMessage(const FBCompiledRequestState& request_state)
    -> std::optional<std::pair<uint8_t, uint32_t>> {
    if (!request_state.portal_active) {
        return std::nullopt;
    }

    if (!request_state.pending_rows.empty()) {
        if (!request_state.projection_bindings.empty()) {
            const auto binding_it = std::min_element(
                request_state.projection_bindings.begin(),
                request_state.projection_bindings.end(),
                [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
            return std::make_pair(binding_it->first, 4u); // isc_info_req_send
        }
        return std::make_pair(static_cast<uint8_t>(0), 4u); // isc_info_req_send
    }

    if (!request_state.input_bindings.empty() && !request_state.input_values_ready) {
        uint8_t min_message = request_state.input_bindings.front().message_number;
        for (const auto& binding : request_state.input_bindings) {
            min_message = std::min(min_message, binding.message_number);
        }
        return std::make_pair(min_message, 5u); // isc_info_req_receive
    }

    if (!request_state.execution_complete) {
        return std::make_pair(static_cast<uint8_t>(0), 2u); // isc_info_req_active
    }

    return std::nullopt;
}

static void buildFirebirdRequestInfoBuffer(const FBCompiledRequestState& request_state,
                                           const std::string& items,
                                           uint32_t buffer_length,
                                           std::vector<uint8_t>& info_out) {
    info_out.clear();

    uint8_t max_message = 0;
    for (const auto& entry : request_state.message_fields) {
        max_message = std::max(max_message, entry.first);
    }

    uint8_t max_send = 0;
    for (const auto& entry : request_state.projection_bindings) {
        max_send = std::max(max_send, entry.first);
    }

    uint8_t max_receive = 0;
    for (const auto& binding : request_state.input_bindings) {
        max_receive = std::max(max_receive, binding.message_number);
    }

    const auto active_message = firebirdRequestActiveMessage(request_state);
    const uint32_t state_code = active_message.has_value() ? active_message->second : 3u; // inactive
    const bool message_applicable = active_message.has_value() &&
                                    (state_code == 4u || state_code == 5u);
    const uint8_t current_message = active_message.has_value() ? active_message->first : 0;

    for (size_t i = 0; i < items.size(); ++i) {
        const uint8_t item = static_cast<uint8_t>(items[i]);
        if (item == 0 || item == 1) { // zero terminator / isc_info_end
            break;
        }

        switch (item) {
            case 4: // isc_info_number_messages
                appendFirebirdInfoItemInt(info_out, item,
                                          static_cast<uint32_t>(request_state.message_fields.size()));
                break;
            case 5: // isc_info_max_message
                appendFirebirdInfoItemInt(info_out, item, max_message);
                break;
            case 6: // isc_info_max_send
                appendFirebirdInfoItemInt(info_out, item, max_send);
                break;
            case 7: // isc_info_max_receive
                appendFirebirdInfoItemInt(info_out, item, max_receive);
                break;
            case 8: // isc_info_state
                appendFirebirdInfoItemInt(info_out, item, state_code);
                break;
            case 9: // isc_info_message_number
                if (!message_applicable) {
                    appendFirebirdInfoError(info_out, item, 335544339u); // isc_infinap
                    break;
                }
                appendFirebirdInfoItemInt(info_out, item, current_message);
                break;
            case 10: // isc_info_message_size
            {
                if (!message_applicable) {
                    appendFirebirdInfoError(info_out, item, 335544339u); // isc_infinap
                    break;
                }
                auto layout_it = request_state.message_fields.find(current_message);
                if (layout_it == request_state.message_fields.end()) {
                    appendFirebirdInfoError(info_out, item, 335544339u); // isc_infinap
                    break;
                }
                appendFirebirdInfoItemInt(info_out, item,
                                          firebirdRequestMessageSize(layout_it->second));
                break;
            }
            case 13: // isc_info_req_select_count
                appendFirebirdInfoItemInt(info_out, item,
                                          static_cast<uint32_t>(request_state.pending_rows.size()));
                break;
            case 14: // isc_info_req_insert_count
            case 15: // isc_info_req_update_count
            case 16: // isc_info_req_delete_count
                appendFirebirdInfoItemInt(info_out, item, 0);
                break;
            default:
                appendFirebirdInfoError(info_out, item, 335544341u); // isc_infunk
                break;
        }
    }

    bool truncated = false;
    if (buffer_length > 0) {
        if (info_out.size() + 1 > buffer_length) {
            truncated = true;
            if (buffer_length >= 1) {
                info_out.resize(buffer_length - 1);
            }
        }
        info_out.push_back(truncated ? 2u : 1u); // isc_info_truncated / isc_info_end
    }
}

static void buildFirebirdDatabaseInfoBuffer(const FBClientState& state,
                                            const std::string& items,
                                            uint32_t buffer_length,
                                            std::vector<uint8_t>& info_out) {
    info_out.clear();

    const uint32_t attachment_id =
        state.attachment_id != 0 ? state.attachment_id
                                 : (state.session_id != 0 ? state.session_id : state.client_id);
    static const std::string firebird_version = "LI-V4.0.0.0 ScratchBird Firebird emulation";
    static constexpr uint32_t ods_version = 13;
    static constexpr uint32_t ods_minor_version = 0;
    static constexpr uint32_t attachment_charset = 4; // CS_UTF8
    static constexpr uint8_t db_class = 1; // isc_info_db_class_access
    static constexpr uint8_t impl_class = 1; // access class
    static constexpr uint8_t backward_compatible_impl = 66; // isc_info_db_impl_linux_amd64
    static constexpr uint8_t base_level_payload[] = {1u, 6u};
    static constexpr uint8_t implementation_payload[] = {
        1u, // one implementation pair
        backward_compatible_impl,
        impl_class,
    };
    static constexpr uint8_t fb_implementation_payload[] = {
        1u, // one implementation entry
        backward_compatible_impl,
        0u,
        0u,
        0u,
        impl_class,
        0u, // depth
    };

    for (size_t i = 0; i < items.size(); ++i) {
        const uint8_t item = static_cast<uint8_t>(items[i]);
        if (item == 0 || item == 1) { // zero terminator / isc_info_end
            break;
        }

        switch (item) {
            case 11: // isc_info_implementation
                appendFirebirdInfoItemBytes(info_out,
                                            item,
                                            implementation_payload,
                                            static_cast<uint16_t>(sizeof(implementation_payload)));
                break;
            case 13: // isc_info_base_level
                appendFirebirdInfoItemBytes(info_out,
                                            item,
                                            base_level_payload,
                                            static_cast<uint16_t>(sizeof(base_level_payload)));
                break;
            case 22: // isc_info_attachment_id
                appendFirebirdInfoItemInt(info_out, item, attachment_id);
                break;
            case 32: // isc_info_ods_version
                appendFirebirdInfoItemInt(info_out, item, ods_version);
                break;
            case 33: // isc_info_ods_minor_version
                appendFirebirdInfoItemInt(info_out, item, ods_minor_version);
                break;
            case 62: // isc_info_db_sql_dialect
                appendFirebirdInfoItemInt(info_out, item, 3);
                break;
            case 63: // isc_info_db_read_only
                appendFirebirdInfoItemInt(info_out, item, 0);
                break;
            case 100: // isc_info_db_class
                appendFirebirdInfoItemInt(info_out, item, db_class);
                break;
            case 101: // frb_info_att_charset
                appendFirebirdInfoItemInt(info_out, item, attachment_charset);
                break;
            case 103: // isc_info_firebird_version
                appendFirebirdInfoItemCountedString(info_out, item, firebird_version);
                break;
            case 114: // fb_info_implementation
                appendFirebirdInfoItemBytes(info_out,
                                            item,
                                            fb_implementation_payload,
                                            static_cast<uint16_t>(sizeof(fb_implementation_payload)));
                break;
            default:
                appendFirebirdInfoError(info_out, item, 335544341u); // isc_infunk
                break;
        }
    }

    bool truncated = false;
    if (buffer_length > 0) {
        if (info_out.size() + 1 > buffer_length) {
            truncated = true;
            if (buffer_length >= 1) {
                info_out.resize(buffer_length - 1);
            }
        }
        info_out.push_back(truncated ? 2u : 1u); // isc_info_truncated / isc_info_end
    }
}

static void buildFirebirdCursorInfoBuffer(const FBDsqlStatementState& stmt_state,
                                          const std::string& items,
                                          uint32_t buffer_length,
                                          std::vector<uint8_t>& info_out) {
    info_out.clear();

    bool need_length = false;
    bool truncated = false;

    for (size_t i = 0; i < items.size(); ++i) {
        const uint8_t item = static_cast<uint8_t>(items[i]);
        if (item == 0 || item == 1) { // zero terminator / isc_info_end
            break;
        }

        switch (item) {
            case 126: // isc_info_length
                need_length = true;
                break;
            case 10: // IResultSet::INF_RECORD_COUNT
                appendFirebirdInfoItemInt(
                    info_out,
                    item,
                    stmt_state.execution_complete
                        ? static_cast<uint32_t>(stmt_state.pending_rows.size())
                        : static_cast<uint32_t>(-1));
                break;
            default:
                appendFirebirdInfoError(info_out, item, 335544341u); // isc_infunk
                break;
        }
    }

    if (need_length) {
        std::vector<uint8_t> with_length;
        appendFirebirdInfoItemInt(with_length, 126, static_cast<uint32_t>(info_out.size() + 1));
        with_length.insert(with_length.end(), info_out.begin(), info_out.end());
        info_out.swap(with_length);
    }

    if (buffer_length > 0) {
        if (info_out.size() + 1 > buffer_length) {
            truncated = true;
            if (buffer_length >= 1) {
                info_out.resize(buffer_length - 1);
            }
        }
        info_out.push_back(truncated ? 2u : 1u); // isc_info_truncated / isc_info_end
    }
}

static auto decodeFirebirdTransactionTpb(const std::string& tpb,
                                         FBTransactionState& txn_state,
                                         std::string& error_out) -> bool {
    if (tpb.empty()) {
        return true;
    }

    size_t offset = 0;
    bool explicit_snapshot_number = false;
    const uint8_t version = static_cast<uint8_t>(tpb[offset++]);
    if (version != 1 && version != 3) {
        error_out = "Unsupported Firebird TPB version";
        return false;
    }

    while (offset < tpb.size()) {
        const uint8_t item = static_cast<uint8_t>(tpb[offset++]);
        switch (item) {
            case 1: // isc_tpb_consistency
                if (explicit_snapshot_number) {
                    error_out = "isc_tpb_consistency conflicts with isc_tpb_at_snapshot_number";
                    return false;
                }
                txn_state.isolation_mode = 1;
                txn_state.read_committed_mode = 0;
                break;
            case 2: // isc_tpb_concurrency
                txn_state.isolation_mode = 2;
                txn_state.read_committed_mode = 0;
                break;
            case 6: // isc_tpb_wait
            case 7: // isc_tpb_nowait
            case 14: // isc_tpb_ignore_limbo
            case 16: // isc_tpb_autocommit
            case 19: // isc_tpb_restart_requests
            case 20: // isc_tpb_no_auto_undo
            case 24: // isc_tpb_auto_release_temp_blobid
                break;
            case 8: // isc_tpb_read
                txn_state.read_only = true;
                break;
            case 9: // isc_tpb_write
                txn_state.read_only = false;
                break;
            case 15: // isc_tpb_read_committed
                if (explicit_snapshot_number) {
                    error_out = "isc_tpb_read_committed conflicts with isc_tpb_at_snapshot_number";
                    return false;
                }
                txn_state.isolation_mode = 3;
                txn_state.read_committed_mode = 0;
                break;
            case 17: // isc_tpb_rec_version
                txn_state.read_committed_mode = 1;
                break;
            case 18: // isc_tpb_no_rec_version
                txn_state.read_committed_mode = 0;
                break;
            case 21: // isc_tpb_lock_timeout
            {
                if (offset >= tpb.size()) {
                    error_out = "Malformed Firebird TPB lock timeout";
                    return false;
                }
                const uint8_t len = static_cast<uint8_t>(tpb[offset++]);
                if (len == 0 || offset + len > tpb.size() || len > 4) {
                    error_out = "Malformed Firebird TPB lock timeout";
                    return false;
                }
                uint32_t value = 0;
                for (uint8_t i = 0; i < len; ++i) {
                    value |= static_cast<uint32_t>(static_cast<uint8_t>(tpb[offset + i])) << (i * 8);
                }
                offset += len;
                txn_state.lock_timeout = value;
                break;
            }
            case 22: // isc_tpb_read_consistency
                txn_state.read_committed_mode = 2;
                break;
            case 23: // isc_tpb_at_snapshot_number
            {
                if (explicit_snapshot_number) {
                    error_out = "Duplicate Firebird TPB snapshot number";
                    return false;
                }
                if (txn_state.isolation_mode == 1 || txn_state.isolation_mode == 3) {
                    error_out = "isc_tpb_at_snapshot_number conflicts with current isolation mode";
                    return false;
                }
                if (offset >= tpb.size()) {
                    error_out = "Malformed Firebird TPB snapshot number";
                    return false;
                }
                const uint8_t len = static_cast<uint8_t>(tpb[offset++]);
                if (len == 0 || offset + len > tpb.size() || len > 8) {
                    error_out = "Malformed Firebird TPB snapshot number";
                    return false;
                }
                int64_t value = 0;
                int shift = 0;
                for (uint8_t i = 0; i + 1 < len; ++i) {
                    value += static_cast<int64_t>(static_cast<uint8_t>(tpb[offset + i])) << shift;
                    shift += 8;
                }
                value += static_cast<int64_t>(static_cast<int8_t>(tpb[offset + len - 1])) << shift;
                if (value <= 0) {
                    error_out = "Invalid Firebird TPB snapshot number";
                    return false;
                }
                txn_state.snapshot_number = static_cast<uint64_t>(value);
                explicit_snapshot_number = true;
                offset += len;
                break;
            }
            case 3:  // isc_tpb_shared
            case 4:  // isc_tpb_protected
            case 5:  // isc_tpb_exclusive
            case 10: // isc_tpb_lock_read
            case 11: // isc_tpb_lock_write
            case 12: // isc_tpb_verb_time
            case 13: // isc_tpb_commit_time
            case 25: // isc_tpb_lock_table_schema
                error_out = "Unsupported Firebird TPB option for parser-owned transaction state";
                return false;
            default:
                error_out = "Unknown Firebird TPB option";
                return false;
        }
    }

    return true;
}

static void buildFirebirdTransactionInfoBuffer(const FBTransactionState& txn_state,
                                               const std::string& items,
                                               uint32_t buffer_length,
                                               std::vector<uint8_t>& info_out) {
    info_out.clear();

    for (size_t i = 0; i < items.size(); ++i) {
        const uint8_t item = static_cast<uint8_t>(items[i]);
        if (item == 0 || item == 1) { // zero terminator / isc_info_end
            break;
        }

        switch (item) {
            case 4: // isc_info_tra_id
                appendFirebirdInfoItemInt(info_out, item, txn_state.transaction_id);
                break;
            case 5: // isc_info_tra_oldest_interesting
                appendFirebirdInfoItemInt(info_out, item, txn_state.oldest_interesting);
                break;
            case 6: // isc_info_tra_oldest_snapshot
                appendFirebirdInfoItemInt(info_out, item, txn_state.oldest_snapshot);
                break;
            case 7: // isc_info_tra_oldest_active
                appendFirebirdInfoItemInt(info_out, item, txn_state.oldest_active);
                break;
            case 8: // isc_info_tra_isolation
            {
                uint8_t payload[2];
                uint16_t length = 1;
                payload[0] = txn_state.isolation_mode;
                if (txn_state.isolation_mode == 3) {
                    payload[1] = txn_state.read_committed_mode;
                    length = 2;
                }
                appendFirebirdInfoItemBytes(info_out, item, payload, length);
                break;
            }
            case 9: // isc_info_tra_access
            {
                const uint8_t access_mode = txn_state.read_only ? 0u : 1u;
                appendFirebirdInfoItemBytes(info_out, item, &access_mode, 1);
                break;
            }
            case 10: // isc_info_tra_lock_timeout
                appendFirebirdInfoItemInt(info_out, item, txn_state.lock_timeout);
                break;
            case 11: // fb_info_tra_dbpath
                appendFirebirdInfoItemString(info_out, item, txn_state.database_path);
                break;
            case 12: // fb_info_tra_snapshot_number
                appendFirebirdInfoItemInt64(info_out, item, static_cast<int64_t>(txn_state.snapshot_number));
                break;
            default:
                appendFirebirdInfoError(info_out, item, 335544341u); // isc_infunk
                break;
        }
    }

    bool truncated = false;
    if (buffer_length > 0) {
        if (info_out.size() + 1 > buffer_length) {
            truncated = true;
            if (buffer_length >= 1) {
                info_out.resize(buffer_length - 1);
            }
        }
        info_out.push_back(truncated ? 2u : 1u); // isc_info_truncated / isc_info_end
    }
}

static constexpr int32_t FB_ISC_TIME_SECONDS_PRECISION = 10000;

static uint16_t blrReadWord(const std::vector<uint8_t>& bytes,
                            size_t& offset,
                            std::string& error_out) {
    if (offset + 2 > bytes.size()) {
        error_out = "Unexpected end of Firebird BLR stream";
        return 0;
    }
    const uint16_t value = static_cast<uint16_t>(bytes[offset]) |
                           (static_cast<uint16_t>(bytes[offset + 1]) << 8);
    offset += 2;
    return value;
}

class FirebirdBlrLayoutDecoder {
public:
    explicit FirebirdBlrLayoutDecoder(const std::vector<uint8_t>& blr)
        : blr_(blr) {
    }

    auto decode(FBCompiledRequestState& out, std::string& error_out) -> bool {
        request_state_ = &out;
        const uint8_t version = readByte(error_out);
        if (!error_out.empty()) {
            return false;
        }
        if (version != 4 && version != 5) {
            error_out = "Unsupported Firebird BLR version " + std::to_string(version);
            return false;
        }
        if (!expectByte(2, error_out)) { // blr_begin
            return false;
        }

        while (peekByte(error_out) == 4) { // blr_message
            parseMessageBlock(out, error_out);
            if (!error_out.empty()) {
                return false;
            }
        }

        if (!parseStatement(out, error_out)) {
            return false;
        }
        if (!expectByte(255, error_out)) { // blr_end
            return false;
        }
        if (!expectByte(76, error_out)) { // blr_eoc
            return false;
        }
        return true;
    }

private:
    const std::vector<uint8_t>& blr_;
    size_t offset_ = 0;
    FBCompiledRequestState* request_state_ = nullptr;

    auto readByte(std::string& error_out) -> uint8_t {
        if (offset_ >= blr_.size()) {
            error_out = "Unexpected end of Firebird BLR stream";
            return 0;
        }
        return blr_[offset_++];
    }

    auto peekByte(std::string& error_out) -> uint8_t {
        if (offset_ >= blr_.size()) {
            error_out = "Unexpected end of Firebird BLR stream";
            return 0;
        }
        return blr_[offset_];
    }

    auto expectByte(uint8_t expected, std::string& error_out) -> bool {
        const uint8_t actual = readByte(error_out);
        if (!error_out.empty()) {
            return false;
        }
        if (actual != expected) {
            error_out = "Expected Firebird BLR opcode " + std::to_string(expected) +
                        " but found " + std::to_string(actual);
            return false;
        }
        return true;
    }

    auto readString8(std::string& error_out) -> std::string {
        const uint8_t length = readByte(error_out);
        if (!error_out.empty()) {
            return {};
        }
        if (offset_ + length > blr_.size()) {
            error_out = "Unexpected end of Firebird BLR string";
            return {};
        }
        std::string out(reinterpret_cast<const char*>(blr_.data() + offset_), length);
        offset_ += length;
        return out;
    }

    void parseMessageBlock(FBCompiledRequestState& out, std::string& error_out) {
        (void)readByte(error_out); // blr_message
        if (!error_out.empty()) {
            return;
        }
        const uint8_t message_number = readByte(error_out);
        if (!error_out.empty()) {
            return;
        }
        const uint16_t field_count = blrReadWord(blr_, offset_, error_out);
        if (!error_out.empty()) {
            return;
        }

        auto& fields = out.message_fields[message_number];
        fields.reserve(field_count);
        for (uint16_t i = 0; i < field_count; ++i) {
            FBMessageFieldDesc field;
            parseTypeDescriptor(field, error_out);
            if (!error_out.empty()) {
                return;
            }
            fields.push_back(field);
        }
    }

    void parseTypeDescriptor(FBMessageFieldDesc& field, std::string& error_out) {
        const uint8_t opcode = readByte(error_out);
        if (!error_out.empty()) {
            return;
        }

        if (opcode == 20) { // blr_not_nullable
            field.not_nullable = true;
            parseTypeDescriptor(field, error_out);
            return;
        }

        field.type_opcode = opcode;
        switch (opcode) {
            case 7:   // blr_short
            case 8:   // blr_long
            case 9:   // blr_quad
            case 16:  // blr_int64
            case 24:  // blr_dec64
            case 25:  // blr_dec128
            case 26:  // blr_int128
                field.scale = static_cast<int8_t>(readByte(error_out));
                switch (opcode) {
                    case 7:
                        field.length = 2;
                        break;
                    case 8:
                        field.length = 4;
                        break;
                    case 9:
                        field.length = 8;
                        break;
                    case 16:
                        field.length = 8;
                        break;
                    case 24:
                        field.length = 8;
                        break;
                    case 25:
                    case 26:
                        field.length = 16;
                        break;
                }
                return;
            case 10:  // blr_float
                field.length = 4;
                return;
            case 11:  // blr_d_float
                field.length = 8;
                field.sql_type_override = 530; // SQL_D_FLOAT
                return;
            case 12:  // blr_sql_date
            case 13:  // blr_sql_time
                field.length = 4;
                return;
            case 17:  // blr_blob2
                field.subtype = blrReadWord(blr_, offset_, error_out);
                if (!error_out.empty()) {
                    return;
                }
                (void)blrReadWord(blr_, offset_, error_out); // text type / charset
                field.length = 8;
                field.sql_type_override = 520; // SQL_BLOB
                return;
            case 23:  // blr_bool
                field.length = 1;
                return;
            case 27:  // blr_double
                field.length = 8;
                return;
            case 28:  // blr_sql_time_tz
                field.length = 8;
                return;
            case 29:  // blr_timestamp_tz
                field.length = 12;
                return;
            case 30:  // blr_ex_time_tz
                field.length = 8;
                field.sql_type_override = 32750; // SQL_TIME_TZ_EX
                return;
            case 31:  // blr_ex_timestamp_tz
                field.length = 12;
                field.sql_type_override = 32748; // SQL_TIMESTAMP_TZ_EX
                return;
            case 35:  // blr_timestamp
                field.length = 8;
                return;
            case 14:  // blr_text
            case 40:  // blr_cstring
                field.length = blrReadWord(blr_, offset_, error_out);
                return;
            case 15:  // blr_text2
            case 41:  // blr_cstring2
                field.subtype = blrReadWord(blr_, offset_, error_out);
                if (!error_out.empty()) {
                    return;
                }
                field.length = blrReadWord(blr_, offset_, error_out);
                return;
            case 37:  // blr_varying
                field.length = static_cast<uint16_t>(blrReadWord(blr_, offset_, error_out) + 2);
                return;
            case 38:  // blr_varying2
                field.subtype = blrReadWord(blr_, offset_, error_out);
                if (!error_out.empty()) {
                    return;
                }
                field.length = static_cast<uint16_t>(blrReadWord(blr_, offset_, error_out) + 2);
                return;
            default:
                error_out = "Unsupported Firebird BLR message datatype opcode " +
                            std::to_string(opcode);
                return;
        }
    }

    auto parseStatement(FBCompiledRequestState& out, std::string& error_out) -> bool {
        const uint8_t opcode = readByte(error_out);
        if (!error_out.empty()) {
            return false;
        }

        switch (opcode) {
            case 2: { // blr_begin
                while (true) {
                    const uint8_t next = peekByte(error_out);
                    if (!error_out.empty()) {
                        return false;
                    }
                    if (next == 255) {
                        (void)readByte(error_out);
                        return true;
                    }
                    if (!parseStatement(out, error_out)) {
                        return false;
                    }
                }
            }
            case 12: // blr_receive
                (void)readByte(error_out); // message number
                if (!error_out.empty()) {
                    return false;
                }
                return parseStatement(out, error_out);
            case 14: // blr_send
                return parseSend(out, error_out);
            case 7: // blr_for
                return parseFor(out, error_out);
            default:
                error_out = "Unsupported Firebird BLR statement opcode " +
                            std::to_string(opcode);
                return false;
        }
    }

    auto parseFor(FBCompiledRequestState& out, std::string& error_out) -> bool {
        if (!parseRse(error_out)) {
            return false;
        }
        return parseStatement(out, error_out);
    }

    auto parseSend(FBCompiledRequestState& out, std::string& error_out) -> bool {
        const uint8_t output_message_number = readByte(error_out);
        if (!error_out.empty()) {
            return false;
        }

        const uint8_t next = peekByte(error_out);
        if (!error_out.empty()) {
            return false;
        }

        if (next == 2) { // blr_begin
            (void)readByte(error_out);
            if (!error_out.empty()) {
                return false;
            }
            while (true) {
                const uint8_t block_opcode = peekByte(error_out);
                if (!error_out.empty()) {
                    return false;
                }
                if (block_opcode == 255) {
                    (void)readByte(error_out);
                    return true;
                }
                if (!parseAssignment(out, output_message_number, error_out)) {
                    return false;
                }
            }
        }

        return parseAssignment(out, output_message_number, error_out);
    }

    auto parseAssignment(FBCompiledRequestState& out,
                         uint8_t output_message_number,
                         std::string& error_out) -> bool {
        if (!expectByte(1, error_out)) { // blr_assignment
            return false;
        }
        if (!skipValueExpr(error_out)) {
            return false;
        }

        const uint8_t target_opcode = readByte(error_out);
        if (!error_out.empty()) {
            return false;
        }

        FBProjectionBinding binding{};
        if (target_opcode == 25) { // blr_parameter
            binding.message_number = readByte(error_out);
            if (!error_out.empty()) {
                return false;
            }
            binding.value_index = blrReadWord(blr_, offset_, error_out);
            if (!error_out.empty()) {
                return false;
            }
        }
        else if (target_opcode == 41) { // blr_parameter2
            binding.message_number = readByte(error_out);
            if (!error_out.empty()) {
                return false;
            }
            binding.value_index = blrReadWord(blr_, offset_, error_out);
            if (!error_out.empty()) {
                return false;
            }
            binding.null_index = blrReadWord(blr_, offset_, error_out);
            if (!error_out.empty()) {
                return false;
            }
        }
        else {
            error_out = "Unsupported Firebird BLR assignment target opcode " +
                        std::to_string(target_opcode);
            return false;
        }

        if (binding.message_number != output_message_number) {
            error_out = "Firebird BLR send block targets multiple output messages";
            return false;
        }

        out.projection_bindings[output_message_number].push_back(binding);
        return true;
    }

    auto parseRse(std::string& error_out) -> bool {
        if (!expectByte(67, error_out)) { // blr_rse
            return false;
        }
        const uint8_t source_count = readByte(error_out);
        if (!error_out.empty()) {
            return false;
        }
        if (source_count != 1) {
            error_out = "Firebird BLR subset only supports single-relation record selection expressions";
            return false;
        }
        if (!skipRelationSource(error_out)) {
            return false;
        }

        while (true) {
            const uint8_t opcode = readByte(error_out);
            if (!error_out.empty()) {
                return false;
            }
            switch (opcode) {
                case 71: // blr_boolean
                    if (!skipBooleanExpr(error_out)) {
                        return false;
                    }
                    break;
                case 68: // blr_first
                    if (!skipValueExpr(error_out)) {
                        return false;
                    }
                    break;
                case 255: // blr_end
                    return true;
                default:
                    error_out = "Unsupported Firebird BLR RSE clause opcode " +
                                std::to_string(opcode);
                    return false;
            }
        }
    }

    auto skipRelationSource(std::string& error_out) -> bool {
        const uint8_t opcode = readByte(error_out);
        if (!error_out.empty()) {
            return false;
        }
        switch (opcode) {
            case 74: // blr_relation
                (void)readString8(error_out);
                return error_out.empty();
            case 146: // blr_relation2
                (void)readString8(error_out);
                if (!error_out.empty()) {
                    return false;
                }
                (void)readString8(error_out);
                return error_out.empty();
            case 148: // blr_relation3
                (void)readString8(error_out);
                if (!error_out.empty()) {
                    return false;
                }
                (void)readString8(error_out);
                if (!error_out.empty()) {
                    return false;
                }
                (void)readString8(error_out);
                return error_out.empty();
            default:
                error_out = "Unsupported Firebird BLR relation source opcode " +
                            std::to_string(opcode);
                return false;
        }
    }

    auto skipBooleanExpr(std::string& error_out) -> bool {
        const uint8_t opcode = readByte(error_out);
        if (!error_out.empty()) {
            return false;
        }

        switch (opcode) {
            case 47: // blr_eql
            case 48: // blr_neq
            case 49: // blr_gtr
            case 50: // blr_geq
            case 51: // blr_lss
            case 52: // blr_leq
                return skipValueExpr(error_out) && skipValueExpr(error_out);
            case 58: // blr_and
            case 57: // blr_or
                return skipBooleanExpr(error_out) && skipBooleanExpr(error_out);
            case 59: // blr_not
                return skipBooleanExpr(error_out);
            default:
                error_out = "Unsupported Firebird BLR boolean opcode " +
                            std::to_string(opcode);
                return false;
        }
    }

    auto skipValueExpr(std::string& error_out) -> bool {
        const uint8_t opcode = readByte(error_out);
        if (!error_out.empty()) {
            return false;
        }

        switch (opcode) {
            case 23: // blr_field
                (void)readByte(error_out);
                if (!error_out.empty()) {
                    return false;
                }
                (void)readString8(error_out);
                return error_out.empty();
            case 21: // blr_literal
                return skipLiteral(error_out);
            case 45: // blr_null
                return true;
            case 25: // blr_parameter
            {
                FBInputBinding binding{};
                binding.message_number = readByte(error_out);
                if (!error_out.empty()) {
                    return false;
                }
                binding.value_index = blrReadWord(blr_, offset_, error_out);
                if (!error_out.empty()) {
                    return false;
                }
                if (request_state_ != nullptr) {
                    request_state_->input_bindings.push_back(binding);
                }
                return true;
            }
            case 41: // blr_parameter2
            {
                FBInputBinding binding{};
                binding.message_number = readByte(error_out);
                if (!error_out.empty()) {
                    return false;
                }
                binding.value_index = blrReadWord(blr_, offset_, error_out);
                if (!error_out.empty()) {
                    return false;
                }
                binding.null_index = blrReadWord(blr_, offset_, error_out);
                if (!error_out.empty()) {
                    return false;
                }
                if (request_state_ != nullptr) {
                    request_state_->input_bindings.push_back(binding);
                }
                return true;
            }
            default:
                error_out = "Unsupported Firebird BLR value opcode " +
                            std::to_string(opcode);
                return false;
        }
    }

    auto skipLiteral(std::string& error_out) -> bool {
        const uint8_t type_opcode = readByte(error_out);
        if (!error_out.empty()) {
            return false;
        }

        switch (type_opcode) {
            case 7: // blr_short
                (void)readByte(error_out);
                (void)blrReadWord(blr_, offset_, error_out);
                return error_out.empty();
            case 8: // blr_long
                (void)readByte(error_out);
                if (offset_ + 4 > blr_.size()) {
                    error_out = "Unexpected end of Firebird BLR literal";
                    return false;
                }
                offset_ += 4;
                return true;
            case 16: // blr_int64
            case 27: // blr_double
                (void)readByte(error_out);
                if (offset_ + 8 > blr_.size()) {
                    error_out = "Unexpected end of Firebird BLR literal";
                    return false;
                }
                offset_ += 8;
                return true;
            case 14: // blr_text
                (void)blrReadWord(blr_, offset_, error_out);
                if (!error_out.empty()) {
                    return false;
                }
                return true;
            case 15: // blr_text2
                (void)blrReadWord(blr_, offset_, error_out);
                if (!error_out.empty()) {
                    return false;
                }
                (void)blrReadWord(blr_, offset_, error_out);
                return error_out.empty();
            case 37: // blr_varying
            case 38: // blr_varying2
            {
                const uint16_t length = blrReadWord(blr_, offset_, error_out);
                if (!error_out.empty()) {
                    return false;
                }
                if (offset_ + length > blr_.size()) {
                    error_out = "Unexpected end of Firebird BLR literal";
                    return false;
                }
                offset_ += length;
                return true;
            }
            case 23: // blr_bool
                (void)readByte(error_out);
                return error_out.empty();
            default:
                error_out = "Unsupported Firebird BLR literal datatype opcode " +
                            std::to_string(type_opcode);
                return false;
        }
    }
};

static auto decodeIpcDataRow(const IPCMessage& message,
                             std::vector<std::optional<std::string>>& row_out,
                             std::string& error_out) -> bool {
    auto* payload = message.getPayload<IPCDataRowPayload>();
    if (payload == nullptr) {
        error_out = "DATA_ROW payload missing";
        return false;
    }

    size_t offset = sizeof(IPCDataRowPayload);
    row_out.clear();
    row_out.reserve(payload->num_fields);
    for (uint16_t i = 0; i < payload->num_fields; ++i) {
        if (offset + sizeof(int32_t) > message.payload.size()) {
            error_out = "Malformed DATA_ROW payload";
            return false;
        }
        int32_t length = 0;
        std::memcpy(&length, message.payload.data() + offset, sizeof(length));
        offset += sizeof(length);
        if (length < 0) {
            row_out.emplace_back(std::nullopt);
            continue;
        }
        if (offset + static_cast<size_t>(length) > message.payload.size()) {
            error_out = "Malformed DATA_ROW field payload";
            return false;
        }
        row_out.emplace_back(std::string(
            reinterpret_cast<const char*>(message.payload.data() + offset),
            static_cast<size_t>(length)));
        offset += static_cast<size_t>(length);
    }
    return true;
}

static auto decodeIpcRowDescription(const IPCMessage& message,
                                    std::vector<std::string>& names_out,
                                    std::string& error_out) -> bool {
    auto* payload = message.getPayload<IPCRowDescriptionPayload>();
    if (payload == nullptr) {
        error_out = "ROW_DESCRIPTION payload missing";
        return false;
    }

    const size_t header_size = sizeof(IPCRowDescriptionPayload);
    const size_t field_bytes = static_cast<size_t>(payload->num_fields) * sizeof(IPCFieldDesc);
    if (message.payload.size() < header_size + field_bytes) {
        error_out = "Malformed ROW_DESCRIPTION payload";
        return false;
    }

    names_out.clear();
    names_out.reserve(payload->num_fields);
    const auto* fields = reinterpret_cast<const IPCFieldDesc*>(message.payload.data() + header_size);
    for (uint16_t i = 0; i < payload->num_fields; ++i) {
        names_out.emplace_back(fields[i].name);
    }
    return true;
}

static void xdrAppendOpaque(std::vector<uint8_t>& out, const uint8_t* data, size_t len) {
    if (len > 0) {
        if (data != nullptr) {
            out.insert(out.end(), data, data + len);
        }
        else {
            out.resize(out.size() + len, 0);
        }
    }
    while (out.size() % 4 != 0) {
        out.push_back(0);
    }
}

static auto consumeXdrOpaque(const std::vector<uint8_t>& packet,
                             size_t& offset,
                             size_t length,
                             std::string& out,
                             std::string& error_out) -> bool;

static auto decodeFirebirdInputField(const std::vector<uint8_t>& packet,
                                     size_t& offset,
                                     const FBMessageFieldDesc& field,
                                     std::optional<std::string>& value_out,
                                     std::string& error_out) -> bool;

static auto parseScaledIntegerValue(const std::string& text,
                                    int8_t scale,
                                    int64_t& value_out) -> bool {
    if (scale > 0) {
        return false;
    }

    std::string trimmed = trimAscii(text);
    if (trimmed.empty()) {
        return false;
    }

    bool negative = false;
    if (trimmed.front() == '+' || trimmed.front() == '-') {
        negative = trimmed.front() == '-';
        trimmed.erase(trimmed.begin());
    }
    if (trimmed.empty()) {
        return false;
    }

    std::string integer_part;
    std::string fractional_part;
    const size_t dot = trimmed.find('.');
    if (dot == std::string::npos) {
        integer_part = trimmed;
    }
    else {
        integer_part = trimmed.substr(0, dot);
        fractional_part = trimmed.substr(dot + 1);
        if (trimmed.find('.', dot + 1) != std::string::npos) {
            return false;
        }
    }

    if (integer_part.empty()) {
        integer_part = "0";
    }
    auto is_ascii_digit = [](unsigned char ch) { return std::isdigit(ch) != 0; };
    if (!std::all_of(integer_part.begin(), integer_part.end(),
                     [&](char ch) { return is_ascii_digit(static_cast<unsigned char>(ch)); }) ||
        !std::all_of(fractional_part.begin(), fractional_part.end(),
                     [&](char ch) { return is_ascii_digit(static_cast<unsigned char>(ch)); })) {
        return false;
    }

    const size_t required_fractional_digits = static_cast<size_t>(-scale);
    while (fractional_part.size() > required_fractional_digits && !fractional_part.empty() &&
           fractional_part.back() == '0') {
        fractional_part.pop_back();
    }
    if (fractional_part.size() > required_fractional_digits) {
        return false;
    }
    while (fractional_part.size() < required_fractional_digits) {
        fractional_part.push_back('0');
    }

    std::string digits = integer_part + fractional_part;
    if (digits.empty()) {
        digits = "0";
    }

    int64_t value = 0;
    for (char ch : digits) {
        const int digit = ch - '0';
        if (value > (std::numeric_limits<int64_t>::max() - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
    }
    value_out = negative ? -value : value;
    return true;
}

static auto renderScaledIntegerValue(int64_t value, int8_t scale) -> std::string {
    if (scale >= 0) {
        return std::to_string(value);
    }

    const bool negative = value < 0;
    uint64_t magnitude = negative
                           ? static_cast<uint64_t>(-(value + 1)) + 1
                           : static_cast<uint64_t>(value);
    std::string digits = std::to_string(magnitude);
    const size_t fractional_digits = static_cast<size_t>(-scale);
    if (digits.size() <= fractional_digits) {
        digits.insert(0, fractional_digits + 1 - digits.size(), '0');
    }
    const size_t split = digits.size() - fractional_digits;
    std::string rendered = digits.substr(0, split) + "." + digits.substr(split);
    if (negative) {
        rendered.insert(rendered.begin(), '-');
    }
    return rendered;
}

static auto decodeFirebirdDateValue(int32_t nday,
                                    int& year,
                                    int& month,
                                    int& day) -> void {
    nday += 2400001 - 1721119;
    const int century = (4 * nday - 1) / 146097;
    nday = 4 * nday - 1 - 146097 * century;
    int day_of_century = nday / 4;

    nday = (4 * day_of_century + 3) / 1461;
    day_of_century = 4 * day_of_century + 3 - 1461 * nday;
    day_of_century = (day_of_century + 4) / 4;

    int month_index = (5 * day_of_century - 3) / 153;
    day_of_century = 5 * day_of_century - 3 - 153 * month_index;
    day_of_century = (day_of_century + 5) / 5;

    year = 100 * century + nday;
    if (month_index < 10) {
        month = month_index + 3;
    }
    else {
        month = month_index - 9;
        year += 1;
    }
    day = day_of_century;
}

static auto encodeFirebirdDateValue(int year, int month, int day) -> int32_t {
    int normalized_month = month;
    int normalized_year = year;
    if (normalized_month > 2) {
        normalized_month -= 3;
    }
    else {
        normalized_month += 9;
        normalized_year -= 1;
    }

    const int century = normalized_year / 100;
    const int year_in_century = normalized_year - 100 * century;
    return static_cast<int32_t>(((static_cast<int64_t>(146097) * century) / 4) +
                                ((1461 * year_in_century) / 4) +
                                ((153 * normalized_month + 2) / 5) +
                                day + 1721119 - 2400001);
}

static auto decodeFirebirdTimeValue(int32_t ntime,
                                    int& hours,
                                    int& minutes,
                                    int& seconds,
                                    int& fractions) -> void {
    hours = ntime / (3600 * FB_ISC_TIME_SECONDS_PRECISION);
    ntime %= 3600 * FB_ISC_TIME_SECONDS_PRECISION;
    minutes = ntime / (60 * FB_ISC_TIME_SECONDS_PRECISION);
    ntime %= 60 * FB_ISC_TIME_SECONDS_PRECISION;
    seconds = ntime / FB_ISC_TIME_SECONDS_PRECISION;
    fractions = ntime % FB_ISC_TIME_SECONDS_PRECISION;
}

static auto encodeFirebirdTimeValue(int hours,
                                    int minutes,
                                    int seconds,
                                    int fractions) -> int32_t {
    return ((hours * 60 + minutes) * 60 + seconds) * FB_ISC_TIME_SECONDS_PRECISION + fractions;
}

static auto parseIsoDateText(const std::string& text,
                             int& year,
                             int& month,
                             int& day) -> bool {
    std::string trimmed = trimAscii(text);
    if (trimmed.size() != 10 || trimmed[4] != '-' || trimmed[7] != '-') {
        return false;
    }
    try {
        year = std::stoi(trimmed.substr(0, 4));
        month = std::stoi(trimmed.substr(5, 2));
        day = std::stoi(trimmed.substr(8, 2));
    }
    catch (...) {
        return false;
    }
    return true;
}

static auto parseIsoTimeText(const std::string& text,
                             int& hours,
                             int& minutes,
                             int& seconds,
                             int& fractions) -> bool {
    std::string trimmed = trimAscii(text);
    if (trimmed.size() < 8 || trimmed[2] != ':' || trimmed[5] != ':') {
        return false;
    }
    try {
        hours = std::stoi(trimmed.substr(0, 2));
        minutes = std::stoi(trimmed.substr(3, 2));
        seconds = std::stoi(trimmed.substr(6, 2));
    }
    catch (...) {
        return false;
    }
    fractions = 0;
    if (trimmed.size() == 8) {
        return true;
    }
    if (trimmed[8] != '.') {
        return false;
    }
    std::string fractional = trimmed.substr(9);
    if (fractional.empty() || fractional.size() > 4) {
        return false;
    }
    if (!std::all_of(fractional.begin(), fractional.end(),
                     [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)) != 0; })) {
        return false;
    }
    while (fractional.size() < 4) {
        fractional.push_back('0');
    }
    try {
        fractions = std::stoi(fractional);
    }
    catch (...) {
        return false;
    }
    return true;
}

static auto parseIsoTimestampText(const std::string& text,
                                  int& year,
                                  int& month,
                                  int& day,
                                  int& hours,
                                  int& minutes,
                                  int& seconds,
                                  int& fractions) -> bool {
    std::string trimmed = trimAscii(text);
    const size_t sep = trimmed.find(' ');
    const size_t alt_sep = trimmed.find('T');
    const size_t pos = sep != std::string::npos ? sep : alt_sep;
    if (pos == std::string::npos) {
        return false;
    }
    return parseIsoDateText(trimmed.substr(0, pos), year, month, day) &&
           parseIsoTimeText(trimmed.substr(pos + 1), hours, minutes, seconds, fractions);
}

static auto formatIsoDateText(int32_t encoded_date) -> std::string {
    int year = 0;
    int month = 0;
    int day = 0;
    decodeFirebirdDateValue(encoded_date, year, month, day);
    std::ostringstream out;
    out << std::setw(4) << std::setfill('0') << year
        << "-" << std::setw(2) << std::setfill('0') << month
        << "-" << std::setw(2) << std::setfill('0') << day;
    return out.str();
}

static auto formatIsoTimeText(int32_t encoded_time) -> std::string {
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    int fractions = 0;
    decodeFirebirdTimeValue(encoded_time, hours, minutes, seconds, fractions);
    std::ostringstream out;
    out << std::setw(2) << std::setfill('0') << hours
        << ":" << std::setw(2) << std::setfill('0') << minutes
        << ":" << std::setw(2) << std::setfill('0') << seconds;
    if (fractions != 0) {
        out << "." << std::setw(4) << std::setfill('0') << fractions;
    }
    return out.str();
}

static auto formatIsoTimestampText(int32_t encoded_date, int32_t encoded_time) -> std::string {
    return formatIsoDateText(encoded_date) + " " + formatIsoTimeText(encoded_time);
}

static void xdrAppendFloat32(std::vector<uint8_t>& out, float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    uint8_t buf[4];
    xdrWriteUint32(buf, bits);
    out.insert(out.end(), buf, buf + 4);
}

static void xdrAppendFloat64(std::vector<uint8_t>& out, double value) {
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (int shift = 56; shift >= 0; shift -= 8) {
        out.push_back(static_cast<uint8_t>((bits >> shift) & 0xFF));
    }
}

static auto parseNetworkFloat32(const std::vector<uint8_t>& packet,
                                size_t& offset,
                                float& value_out,
                                std::string& error_out) -> bool {
    if (offset + 4 > packet.size()) {
        error_out = "Malformed Firebird XDR float field";
        return false;
    }
    const uint32_t bits = xdrReadUint32(packet.data() + offset);
    offset += 4;
    std::memcpy(&value_out, &bits, sizeof(bits));
    return true;
}

static auto parseNetworkFloat64(const std::vector<uint8_t>& packet,
                                size_t& offset,
                                double& value_out,
                                std::string& error_out) -> bool {
    if (offset + 8 > packet.size()) {
        error_out = "Malformed Firebird XDR double field";
        return false;
    }
    uint64_t bits = 0;
    for (size_t i = 0; i < 8; ++i) {
        bits = (bits << 8) | packet[offset + i];
    }
    offset += 8;
    std::memcpy(&value_out, &bits, sizeof(bits));
    return true;
}

static auto parseBooleanValue(const std::string& text, uint8_t& value_out) -> bool {
    std::string trimmed = trimAscii(text);
    std::transform(trimmed.begin(), trimmed.end(), trimmed.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (trimmed == "1" || trimmed == "true" || trimmed == "t" ||
        trimmed == "yes" || trimmed == "y") {
        value_out = 1;
        return true;
    }
    if (trimmed == "0" || trimmed == "false" || trimmed == "f" ||
        trimmed == "no" || trimmed == "n") {
        value_out = 0;
        return true;
    }
    return false;
}

static auto appendFirebirdFieldValue(std::vector<uint8_t>& packet,
                                     const FBMessageFieldDesc& field,
                                     const std::optional<std::string>& value,
                                     std::string& error_out) -> bool {
    switch (field.type_opcode) {
        case 14: // blr_text
        case 15: // blr_text2
        {
            if (!value.has_value()) {
                error_out = "NULL text field without explicit null-indicator binding";
                return false;
            }
            std::string padded = *value;
            if (padded.size() > field.length) {
                error_out = "Firebird text field exceeds declared length";
                return false;
            }
            padded.resize(field.length, ' ');
            xdrAppendOpaque(packet,
                            reinterpret_cast<const uint8_t*>(padded.data()),
                            padded.size());
            return true;
        }
        case 37: // blr_varying
        case 38: // blr_varying2
        {
            if (!value.has_value()) {
                error_out = "NULL varying field without explicit null-indicator binding";
                return false;
            }
            const size_t max_len = field.length >= 2 ? field.length - 2 : 0;
            if (value->size() > max_len) {
                error_out = "Firebird varying field exceeds declared length";
                return false;
            }
            xdrAppendInt32(packet, static_cast<int32_t>(value->size()));
            xdrAppendOpaque(packet,
                            reinterpret_cast<const uint8_t*>(value->data()),
                            value->size());
            return true;
        }
        case 40: // blr_cstring
        case 41: // blr_cstring2
        {
            if (!value.has_value()) {
                error_out = "NULL cstring field without explicit null-indicator binding";
                return false;
            }
            const size_t max_len = field.length > 0 ? field.length - 1 : 0;
            const size_t logical_len = std::min(value->size(), max_len);
            xdrAppendInt32(packet, static_cast<int32_t>(logical_len));
            xdrAppendOpaque(packet,
                            reinterpret_cast<const uint8_t*>(value->data()),
                            logical_len);
            return true;
        }
        case 7: // blr_short
        {
            if (!value.has_value()) {
                error_out = "NULL short field without explicit null-indicator binding";
                return false;
            }
            int64_t parsed = 0;
            if (!parseScaledIntegerValue(*value, field.scale, parsed) ||
                parsed < std::numeric_limits<int16_t>::min() ||
                parsed > std::numeric_limits<int16_t>::max()) {
                error_out = "Firebird short field cannot be represented";
                return false;
            }
            xdrAppendInt32(packet, static_cast<int32_t>(static_cast<int16_t>(parsed)));
            return true;
        }
        case 8: // blr_long
        {
            if (!value.has_value()) {
                error_out = "NULL long field without explicit null-indicator binding";
                return false;
            }
            int64_t parsed = 0;
            if (!parseScaledIntegerValue(*value, field.scale, parsed) ||
                parsed < std::numeric_limits<int32_t>::min() ||
                parsed > std::numeric_limits<int32_t>::max()) {
                error_out = "Firebird long field cannot be represented";
                return false;
            }
            xdrAppendInt32(packet, static_cast<int32_t>(parsed));
            return true;
        }
        case 16: // blr_int64
        {
            if (!value.has_value()) {
                error_out = "NULL int64 field without explicit null-indicator binding";
                return false;
            }
            int64_t parsed = 0;
            if (!parseScaledIntegerValue(*value, field.scale, parsed)) {
                error_out = "Firebird int64 field cannot be represented";
                return false;
            }
            xdrAppendInt64(packet, parsed);
            return true;
        }
        case 10: // blr_float
        {
            if (!value.has_value()) {
                error_out = "NULL float field without explicit null-indicator binding";
                return false;
            }
            try {
                xdrAppendFloat32(packet, std::stof(trimAscii(*value)));
            }
            catch (...) {
                error_out = "Firebird float field cannot be represented";
                return false;
            }
            return true;
        }
        case 27: // blr_double
        {
            if (!value.has_value()) {
                error_out = "NULL double field without explicit null-indicator binding";
                return false;
            }
            try {
                xdrAppendFloat64(packet, std::stod(trimAscii(*value)));
            }
            catch (...) {
                error_out = "Firebird double field cannot be represented";
                return false;
            }
            return true;
        }
        case 12: // blr_sql_date
        {
            if (!value.has_value()) {
                error_out = "NULL sql_date field without explicit null-indicator binding";
                return false;
            }
            int year = 0, month = 0, day = 0;
            if (!parseIsoDateText(*value, year, month, day)) {
                error_out = "Firebird sql_date field cannot be represented";
                return false;
            }
            xdrAppendInt32(packet, encodeFirebirdDateValue(year, month, day));
            return true;
        }
        case 13: // blr_sql_time
        {
            if (!value.has_value()) {
                error_out = "NULL sql_time field without explicit null-indicator binding";
                return false;
            }
            int hours = 0, minutes = 0, seconds = 0, fractions = 0;
            if (!parseIsoTimeText(*value, hours, minutes, seconds, fractions)) {
                error_out = "Firebird sql_time field cannot be represented";
                return false;
            }
            xdrAppendInt32(packet, encodeFirebirdTimeValue(hours, minutes, seconds, fractions));
            return true;
        }
        case 35: // blr_timestamp
        {
            if (!value.has_value()) {
                error_out = "NULL timestamp field without explicit null-indicator binding";
                return false;
            }
            int year = 0, month = 0, day = 0;
            int hours = 0, minutes = 0, seconds = 0, fractions = 0;
            if (!parseIsoTimestampText(*value, year, month, day, hours, minutes, seconds, fractions)) {
                error_out = "Firebird timestamp field cannot be represented";
                return false;
            }
            xdrAppendInt32(packet, encodeFirebirdDateValue(year, month, day));
            xdrAppendInt32(packet, encodeFirebirdTimeValue(hours, minutes, seconds, fractions));
            return true;
        }
        case 23: // blr_bool
        {
            if (!value.has_value()) {
                error_out = "NULL bool field without explicit null-indicator binding";
                return false;
            }
            uint8_t encoded = 0;
            if (!parseBooleanValue(*value, encoded)) {
                error_out = "Firebird bool field cannot be represented";
                return false;
            }
            xdrAppendOpaque(packet, &encoded, sizeof(encoded));
            return true;
        }
        default:
            error_out = "Firebird row translation does not support BLR datatype opcode " +
                        std::to_string(field.type_opcode);
            return false;
    }
}

static auto appendFirebirdSendPacket(std::vector<uint8_t>& packet,
                                     uint32_t request_handle,
                                     uint32_t incarnation,
                                     uint32_t transaction_handle,
                                     uint32_t message_number,
                                     const std::optional<std::vector<std::optional<std::string>>>& row,
                                     const FBCompiledRequestState& request_state,
                                     std::string& error_out) -> bool {
    packet.clear();
    xdrAppendUint32(packet, fb::op_send);
    xdrAppendUint32(packet, request_handle);
    xdrAppendUint32(packet, incarnation);
    xdrAppendUint32(packet, transaction_handle);
    xdrAppendUint32(packet, message_number);

    if (!row.has_value()) {
        xdrAppendUint32(packet, 0);
        return true;
    }

    const uint8_t message_id = static_cast<uint8_t>(message_number);
    auto layout_it = request_state.message_fields.find(message_id);
    if (layout_it == request_state.message_fields.end()) {
        error_out = "Unknown Firebird output message layout";
        return false;
    }
    auto binding_it = request_state.projection_bindings.find(message_id);
    if (binding_it == request_state.projection_bindings.end()) {
        error_out = "Unknown Firebird output message binding set";
        return false;
    }

    const auto& fields = layout_it->second;
    const auto& bindings = binding_it->second;
    if (bindings.size() != row->size()) {
        error_out = "Firebird output row column count does not match BLR projection bindings";
        return false;
    }

    xdrAppendUint32(packet, 1);

    std::vector<std::optional<std::string>> field_values(fields.size());
    std::vector<std::optional<int16_t>> null_flags(fields.size());

    for (size_t i = 0; i < bindings.size(); ++i) {
        const auto& binding = bindings[i];
        if (binding.value_index >= fields.size()) {
            error_out = "Firebird BLR value parameter index is out of range";
            return false;
        }
        field_values[binding.value_index] = (*row)[i];
        if (binding.null_index.has_value()) {
            if (*binding.null_index >= fields.size()) {
                error_out = "Firebird BLR null-indicator parameter index is out of range";
                return false;
            }
            null_flags[*binding.null_index] = (*row)[i].has_value() ? int16_t{0} : int16_t{-1};
        }
        else if (!(*row)[i].has_value()) {
            error_out = "Firebird NULL value arrived without a parameter2 null indicator";
            return false;
        }
    }

    for (size_t field_index = 0; field_index < fields.size(); ++field_index) {
        const auto& field = fields[field_index];
        if (null_flags[field_index].has_value()) {
            xdrAppendInt32(packet, static_cast<int32_t>(*null_flags[field_index]));
            continue;
        }
        if (!field_values[field_index].has_value()) {
            if (field.not_nullable) {
                error_out = "Firebird non-nullable output field has no value";
                return false;
            }
            switch (field.type_opcode) {
                case 14:
                case 15:
                    xdrAppendOpaque(packet, nullptr, field.length);
                    continue;
                case 37:
                case 38:
                case 40:
                case 41:
                case 7:
                case 8:
                case 16:
                    xdrAppendInt32(packet, 0);
                    if (field.type_opcode == 16) {
                        packet.resize(packet.size() + 4, 0);
                    }
                    continue;
                case 23:
                {
                    const uint8_t zero = 0;
                    xdrAppendOpaque(packet, &zero, sizeof(zero));
                    continue;
                }
                default:
                    error_out = "Firebird row translation cannot synthesize default for BLR datatype opcode " +
                                std::to_string(field.type_opcode);
                    return false;
            }
        }
        if (!appendFirebirdFieldValue(packet, field, field_values[field_index], error_out)) {
            return false;
        }
    }

    return true;
}

static auto decodeFirebirdMessageOnlyLayout(
    const std::vector<uint8_t>& blr,
    std::unordered_map<uint8_t, std::vector<FBMessageFieldDesc>>& message_fields_out,
    std::string& error_out) -> bool {
    size_t offset = 0;
    auto readByte = [&](uint8_t& value_out) -> bool {
        if (offset >= blr.size()) {
            error_out = "Unexpected end of Firebird BLR message layout";
            return false;
        }
        value_out = blr[offset++];
        return true;
    };
    auto expectByte = [&](uint8_t expected) -> bool {
        uint8_t actual = 0;
        if (!readByte(actual)) {
            return false;
        }
        if (actual != expected) {
            error_out = "Expected Firebird BLR opcode " + std::to_string(expected) +
                        " but found " + std::to_string(actual);
            return false;
        }
        return true;
    };
    auto parseTypeDescriptor = [&](auto&& self, FBMessageFieldDesc& field) -> bool {
        uint8_t opcode = 0;
        if (!readByte(opcode)) {
            return false;
        }
        if (opcode == 20) {
            field.not_nullable = true;
            return self(self, field);
        }

        field.type_opcode = opcode;
        switch (opcode) {
            case 7:
            case 8:
            case 9:
            case 16:
            case 24:
            case 25:
            case 26:
            {
                uint8_t scale_byte = 0;
                if (!readByte(scale_byte)) {
                    return false;
                }
                field.scale = static_cast<int8_t>(scale_byte);
                field.length =
                    opcode == 7 ? 2 :
                    opcode == 8 ? 4 :
                    opcode == 9 ? 8 :
                    opcode == 16 ? 8 : 16;
                if (opcode == 24) {
                    field.length = 8;
                }
                return true;
            }
            case 10:
                field.length = 4;
                return true;
            case 11:
                field.length = 8;
                field.sql_type_override = 530; // SQL_D_FLOAT
                return true;
            case 12:
            case 13:
                field.length = 4;
                return true;
            case 17:
                field.subtype = blrReadWord(blr, offset, error_out);
                if (!error_out.empty()) {
                    return false;
                }
                (void)blrReadWord(blr, offset, error_out);
                field.length = 8;
                field.sql_type_override = 520; // SQL_BLOB
                return error_out.empty();
            case 23:
                field.length = 1;
                return true;
            case 27:
                field.length = 8;
                return true;
            case 28:
                field.length = 8;
                return true;
            case 29:
                field.length = 12;
                return true;
            case 30:
                field.length = 8;
                field.sql_type_override = 32750; // SQL_TIME_TZ_EX
                return true;
            case 31:
                field.length = 12;
                field.sql_type_override = 32748; // SQL_TIMESTAMP_TZ_EX
                return true;
            case 35:
                field.length = 8;
                return true;
            case 14:
            case 40:
                field.length = blrReadWord(blr, offset, error_out);
                return error_out.empty();
            case 15:
            case 41:
                field.subtype = blrReadWord(blr, offset, error_out);
                if (!error_out.empty()) {
                    return false;
                }
                field.length = blrReadWord(blr, offset, error_out);
                return error_out.empty();
            case 37:
                field.length = static_cast<uint16_t>(blrReadWord(blr, offset, error_out) + 2);
                return error_out.empty();
            case 38:
                field.subtype = blrReadWord(blr, offset, error_out);
                if (!error_out.empty()) {
                    return false;
                }
                field.length = static_cast<uint16_t>(blrReadWord(blr, offset, error_out) + 2);
                return error_out.empty();
            default:
                error_out = "Unsupported Firebird SQL BLR datatype opcode " +
                            std::to_string(opcode);
                return false;
        }
    };

    uint8_t version = 0;
    if (!readByte(version)) {
        return false;
    }
    if (version != 4 && version != 5) {
        error_out = "Unsupported Firebird BLR version " + std::to_string(version);
        return false;
    }
    if (!expectByte(2)) {
        return false;
    }

    while (offset < blr.size()) {
        if (blr[offset] == 255) {
            ++offset;
            break;
        }

        if (blr[offset] != 4) {
            error_out = "Expected Firebird BLR message block";
            return false;
        }
        ++offset;

        uint8_t message_number = 0;
        if (!readByte(message_number)) {
            return false;
        }
        const uint16_t field_count = blrReadWord(blr, offset, error_out);
        if (!error_out.empty()) {
            return false;
        }

        auto& fields = message_fields_out[message_number];
        fields.clear();
        fields.reserve(field_count);
        for (uint16_t i = 0; i < field_count; ++i) {
            FBMessageFieldDesc field;
            if (!parseTypeDescriptor(parseTypeDescriptor, field)) {
                return false;
            }
            fields.push_back(field);
        }
    }

    if (offset >= blr.size()) {
        error_out = "Missing Firebird BLR end-of-command marker";
        return false;
    }
    if (blr[offset] != 76) {
        error_out = "Expected Firebird BLR end-of-command marker";
        return false;
    }
    return true;
}

static auto decodeFirebirdSqlMessage(const std::vector<uint8_t>& packet,
                                     size_t& offset,
                                     const std::vector<FBMessageFieldDesc>& fields,
                                     bool packed,
                                     std::vector<std::optional<std::string>>& values_out,
                                     std::vector<bool>& nulls_out,
                                     std::string& error_out) -> bool {
    if (fields.size() % 2 != 0) {
        error_out = "Firebird SQL BLR message must contain value/null field pairs";
        return false;
    }

    const size_t column_count = fields.size() / 2;
    values_out.assign(column_count, std::nullopt);
    nulls_out.assign(column_count, false);

    std::vector<bool> is_null(column_count, false);
    if (packed) {
        const size_t flag_bytes = (column_count + 7) / 8;
        std::string bitmap;
        if (!consumeXdrOpaque(packet, offset, flag_bytes, bitmap, error_out)) {
            return false;
        }
        for (size_t i = 0; i < column_count; ++i) {
            const uint8_t bits = static_cast<uint8_t>(bitmap[i >> 3]);
            is_null[i] = (bits & (1u << (i & 7))) != 0;
        }
    }

    for (size_t i = 0; i < column_count; ++i) {
        const auto& value_field = fields[i * 2];
        const auto& null_field = fields[i * 2 + 1];
        if (null_field.type_opcode != 7) {
            error_out = "Firebird SQL BLR null indicator is not short";
            return false;
        }

        if (!packed) {
            std::optional<std::string> raw_value;
            if (!decodeFirebirdInputField(packet, offset, value_field, raw_value, error_out)) {
                return false;
            }
            std::optional<std::string> null_value;
            if (!decodeFirebirdInputField(packet, offset, null_field, null_value, error_out)) {
                return false;
            }
            int64_t null_flag = 0;
            if (!null_value.has_value() ||
                !parseScaledIntegerValue(*null_value, 0, null_flag)) {
                error_out = "Firebird SQL null indicator is not representable";
                return false;
            }
            is_null[i] = null_flag != 0;
            if (!is_null[i]) {
                values_out[i] = std::move(raw_value);
            }
            nulls_out[i] = is_null[i];
            continue;
        }

        if (is_null[i]) {
            nulls_out[i] = true;
            continue;
        }

        if (!decodeFirebirdInputField(packet, offset, value_field, values_out[i], error_out)) {
            return false;
        }
    }

    return true;
}

static auto appendFirebirdSqlMessage(std::vector<uint8_t>& packet,
                                     const std::optional<std::vector<std::optional<std::string>>>& row,
                                     const std::vector<FBMessageFieldDesc>& fields,
                                     bool packed,
                                     std::string& error_out) -> bool {
    if (!row.has_value()) {
        return true;
    }
    if (fields.size() % 2 != 0) {
        error_out = "Firebird SQL BLR message must contain value/null field pairs";
        return false;
    }

    const size_t column_count = fields.size() / 2;
    if (row->size() != column_count) {
        error_out = "Firebird SQL row column count does not match message layout";
        return false;
    }

    if (packed) {
        const size_t flag_bytes = (column_count + 7) / 8;
        std::vector<uint8_t> null_bitmap(flag_bytes, 0);
        for (size_t i = 0; i < column_count; ++i) {
            if (!(*row)[i].has_value()) {
                null_bitmap[i >> 3] |= static_cast<uint8_t>(1u << (i & 7));
            }
        }
        xdrAppendOpaque(packet, null_bitmap.data(), flag_bytes);
    }

    for (size_t i = 0; i < column_count; ++i) {
        const auto& value_field = fields[i * 2];
        const auto& null_field = fields[i * 2 + 1];
        if (null_field.type_opcode != 7) {
            error_out = "Firebird SQL BLR null indicator is not short";
            return false;
        }

        if (packed) {
            if (!(*row)[i].has_value()) {
                continue;
            }
            if (!appendFirebirdFieldValue(packet, value_field, (*row)[i], error_out)) {
                return false;
            }
            continue;
        }

        if ((*row)[i].has_value()) {
            if (!appendFirebirdFieldValue(packet, value_field, (*row)[i], error_out)) {
                return false;
            }
            xdrAppendInt32(packet, 0);
        }
        else {
            switch (value_field.type_opcode) {
                case 14:
                case 15:
                    xdrAppendOpaque(packet, nullptr, value_field.length);
                    break;
                case 37:
                case 38:
                case 40:
                case 41:
                case 7:
                case 8:
                case 12:
                case 13:
                case 16:
                case 27:
                case 35:
                    xdrAppendInt32(packet, 0);
                    if (value_field.type_opcode == 16 ||
                        value_field.type_opcode == 27 ||
                        value_field.type_opcode == 35) {
                        packet.resize(packet.size() + 4, 0);
                    }
                    break;
                case 10:
                {
                    const uint32_t zero = 0;
                    xdrAppendOpaque(packet, reinterpret_cast<const uint8_t*>(&zero), sizeof(zero));
                    break;
                }
                case 23:
                {
                    const uint8_t zero = 0;
                    xdrAppendOpaque(packet, &zero, sizeof(zero));
                    break;
                }
                default:
                    error_out = "Firebird SQL message cannot synthesize default for BLR datatype opcode " +
                                std::to_string(value_field.type_opcode);
                    return false;
            }
            xdrAppendInt32(packet, -1);
        }
    }

    return true;
}

static auto appendFirebirdSqlResponsePacket(std::vector<uint8_t>& packet,
                                            const std::optional<std::vector<std::optional<std::string>>>& row,
                                            const std::vector<FBMessageFieldDesc>& fields,
                                            bool packed,
                                            std::string& error_out) -> bool {
    packet.clear();
    xdrAppendUint32(packet, fb::op_sql_response);
    xdrAppendUint32(packet, row.has_value() ? 1u : 0u);
    if (!row.has_value()) {
        return true;
    }
    return appendFirebirdSqlMessage(packet, row, fields, packed, error_out);
}

static auto appendFirebirdFetchResponsePacket(std::vector<uint8_t>& packet,
                                              int32_t status,
                                              const std::optional<std::vector<std::optional<std::string>>>& row,
                                              const std::vector<FBMessageFieldDesc>& fields,
                                              bool packed,
                                              std::string& error_out) -> bool {
    packet.clear();
    xdrAppendUint32(packet, fb::op_fetch_response);
    xdrAppendInt32(packet, status);
    xdrAppendUint32(packet, row.has_value() ? 1u : 0u);
    if (!row.has_value()) {
        return true;
    }
    return appendFirebirdSqlMessage(packet, row, fields, packed, error_out);
}

static auto parseNetworkInt64(const std::vector<uint8_t>& packet,
                              size_t& offset,
                              int64_t& value_out,
                              std::string& error_out) -> bool {
    if (offset + 8 > packet.size()) {
        error_out = "Malformed Firebird XDR int64 field";
        return false;
    }
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i) {
        value = (value << 8) | packet[offset + i];
    }
    offset += 8;
    value_out = static_cast<int64_t>(value);
    return true;
}

static auto consumeXdrOpaque(const std::vector<uint8_t>& packet,
                             size_t& offset,
                             size_t length,
                             std::string& out,
                             std::string& error_out) -> bool {
    if (offset + length > packet.size()) {
        error_out = "Malformed Firebird XDR opaque field";
        return false;
    }
    out.assign(reinterpret_cast<const char*>(packet.data() + offset), length);
    offset += length;
    const size_t padding = (4 - (length % 4)) & 3;
    if (offset + padding > packet.size()) {
        error_out = "Malformed Firebird XDR opaque padding";
        return false;
    }
    offset += padding;
    return true;
}

static auto consumeXdrString(const std::vector<uint8_t>& packet,
                             size_t& offset,
                             std::string& out,
                             std::string& error_out) -> bool {
    if (offset + 4 > packet.size()) {
        error_out = "Malformed Firebird XDR string length";
        return false;
    }
    const uint32_t length = xdrReadUint32(packet.data() + offset);
    offset += 4;
    return consumeXdrOpaque(packet, offset, length, out, error_out);
}

static auto trimRightSpaces(const std::string& value) -> std::string {
    size_t end = value.size();
    while (end > 0 && value[end - 1] == ' ') {
        --end;
    }
    return value.substr(0, end);
}

static auto decodeFirebirdInputField(const std::vector<uint8_t>& packet,
                                     size_t& offset,
                                     const FBMessageFieldDesc& field,
                                     std::optional<std::string>& value_out,
                                     std::string& error_out) -> bool {
    switch (field.type_opcode) {
        case 7: // blr_short
        {
            if (offset + 4 > packet.size()) {
                error_out = "Malformed Firebird short field";
                return false;
            }
            const int16_t value = static_cast<int16_t>(xdrReadUint32(packet.data() + offset));
            offset += 4;
            value_out = renderScaledIntegerValue(value, field.scale);
            return true;
        }
        case 8: // blr_long
        {
            if (offset + 4 > packet.size()) {
                error_out = "Malformed Firebird long field";
                return false;
            }
            const int32_t value = static_cast<int32_t>(xdrReadUint32(packet.data() + offset));
            offset += 4;
            value_out = renderScaledIntegerValue(value, field.scale);
            return true;
        }
        case 16: // blr_int64
        {
            int64_t value = 0;
            if (!parseNetworkInt64(packet, offset, value, error_out)) {
                return false;
            }
            value_out = renderScaledIntegerValue(value, field.scale);
            return true;
        }
        case 10: // blr_float
        {
            float value = 0.0f;
            if (!parseNetworkFloat32(packet, offset, value, error_out)) {
                return false;
            }
            std::ostringstream out;
            out << std::setprecision(9) << value;
            value_out = out.str();
            return true;
        }
        case 27: // blr_double
        {
            double value = 0.0;
            if (!parseNetworkFloat64(packet, offset, value, error_out)) {
                return false;
            }
            std::ostringstream out;
            out << std::setprecision(17) << value;
            value_out = out.str();
            return true;
        }
        case 12: // blr_sql_date
        {
            if (offset + 4 > packet.size()) {
                error_out = "Malformed Firebird sql_date field";
                return false;
            }
            const int32_t encoded_date = static_cast<int32_t>(xdrReadUint32(packet.data() + offset));
            offset += 4;
            value_out = formatIsoDateText(encoded_date);
            return true;
        }
        case 13: // blr_sql_time
        {
            if (offset + 4 > packet.size()) {
                error_out = "Malformed Firebird sql_time field";
                return false;
            }
            const int32_t encoded_time = static_cast<int32_t>(xdrReadUint32(packet.data() + offset));
            offset += 4;
            value_out = formatIsoTimeText(encoded_time);
            return true;
        }
        case 35: // blr_timestamp
        {
            if (offset + 8 > packet.size()) {
                error_out = "Malformed Firebird timestamp field";
                return false;
            }
            const int32_t encoded_date = static_cast<int32_t>(xdrReadUint32(packet.data() + offset));
            offset += 4;
            const int32_t encoded_time = static_cast<int32_t>(xdrReadUint32(packet.data() + offset));
            offset += 4;
            value_out = formatIsoTimestampText(encoded_date, encoded_time);
            return true;
        }
        case 14: // blr_text
        case 15: // blr_text2
        {
            std::string text;
            if (!consumeXdrOpaque(packet, offset, field.length, text, error_out)) {
                return false;
            }
            value_out = trimRightSpaces(text);
            return true;
        }
        case 37: // blr_varying
        case 38: // blr_varying2
        case 40: // blr_cstring
        case 41: // blr_cstring2
        {
            if (offset + 4 > packet.size()) {
                error_out = "Malformed Firebird varying/cstring length";
                return false;
            }
            const uint16_t logical_length = static_cast<uint16_t>(xdrReadUint32(packet.data() + offset));
            offset += 4;
            std::string text;
            if (!consumeXdrOpaque(packet, offset, logical_length, text, error_out)) {
                return false;
            }
            value_out = text;
            return true;
        }
        case 23: // blr_bool
        {
            std::string raw;
            if (!consumeXdrOpaque(packet, offset, 1, raw, error_out)) {
                return false;
            }
            value_out = (!raw.empty() && static_cast<unsigned char>(raw[0]) != 0) ? "TRUE" : "FALSE";
            return true;
        }
        default:
            error_out = "Firebird input translation does not support BLR datatype opcode " +
                        std::to_string(field.type_opcode);
            return false;
    }
}

static auto decodeFirebirdInputBindings(const std::vector<uint8_t>& packet,
                                        size_t offset,
                                        uint32_t message_number,
                                        FBCompiledRequestState& request_state,
                                        std::string& error_out) -> bool {
    auto fields_it = request_state.message_fields.find(static_cast<uint8_t>(message_number));
    if (fields_it == request_state.message_fields.end()) {
        error_out = "Unknown Firebird input message layout";
        return false;
    }

    std::vector<std::optional<std::string>> field_values(fields_it->second.size());
    for (size_t i = 0; i < fields_it->second.size(); ++i) {
        if (!decodeFirebirdInputField(packet, offset, fields_it->second[i], field_values[i], error_out)) {
            return false;
        }
    }

    if (offset != packet.size()) {
        error_out = "Unexpected trailing bytes in Firebird input message";
        return false;
    }

    request_state.bound_params.resize(request_state.input_bindings.size());
    request_state.bound_param_nulls.resize(request_state.input_bindings.size(), false);
    for (size_t i = 0; i < request_state.input_bindings.size(); ++i) {
        const auto& binding = request_state.input_bindings[i];
        if (binding.message_number != static_cast<uint8_t>(message_number)) {
            continue;
        }
        if (binding.value_index >= field_values.size()) {
            error_out = "Firebird input parameter value index out of range";
            return false;
        }

        bool is_null = false;
        if (binding.null_index.has_value()) {
            if (*binding.null_index >= field_values.size()) {
                error_out = "Firebird input parameter null-indicator index out of range";
                return false;
            }
            int64_t null_flag = 0;
            if (!field_values[*binding.null_index].has_value() ||
                !parseScaledIntegerValue(*field_values[*binding.null_index], 0, null_flag)) {
                error_out = "Firebird input null-indicator field is not representable";
                return false;
            }
            is_null = null_flag != 0;
        }

        request_state.bound_param_nulls[i] = is_null;
        request_state.bound_params[i] = is_null ? std::nullopt : field_values[binding.value_index];
    }
    request_state.input_values_ready = true;
    return true;
}

static auto decodeFirebirdBasicDpb(const std::string& dpb,
                                   std::string& user_name_out,
                                   std::string& auth_plugin_out,
                                   std::vector<uint8_t>& auth_data_out,
                                   std::string& error_out) -> bool {
    if (dpb.empty()) {
        return true;
    }

    size_t offset = 0;
    const uint8_t version = static_cast<uint8_t>(dpb[offset++]);
    if (version != 1 && version != 2) {
        error_out = "Unsupported Firebird DPB version";
        return false;
    }

    while (offset < dpb.size()) {
        const uint8_t item = static_cast<uint8_t>(dpb[offset++]);
        if (offset >= dpb.size()) {
            error_out = "Malformed Firebird DPB";
            return false;
        }
        const uint8_t len = static_cast<uint8_t>(dpb[offset++]);
        if (offset + len > dpb.size()) {
            error_out = "Malformed Firebird DPB";
            return false;
        }

        switch (item) {
            case 28: // isc_dpb_user_name
                user_name_out.assign(dpb.data() + offset, len);
                break;
            case 29: // isc_dpb_password
            case 30: // isc_dpb_password_enc
                auth_data_out.assign(dpb.begin() + static_cast<std::ptrdiff_t>(offset),
                                     dpb.begin() + static_cast<std::ptrdiff_t>(offset + len));
                break;
            case 85: // isc_dpb_auth_plugin_list
            case 86: // isc_dpb_auth_plugin_name
                auth_plugin_out.assign(dpb.data() + offset, len);
                break;
            default:
                break;
        }
        offset += len;
    }

    return true;
}

static auto decodeFirebirdAttachPacket(const std::vector<uint8_t>& packet,
                                       std::string& database_path_out,
                                       std::string& user_name_out,
                                       std::string& auth_plugin_out,
                                       std::vector<uint8_t>& auth_data_out,
                                       std::string& error_out) -> bool {
    if (packet.size() < 12) {
        error_out = "Malformed Firebird attach packet";
        return false;
    }

    size_t offset = 4;
    offset += 4; // p_atch_database

    std::string dpb;
    if (!consumeXdrString(packet, offset, database_path_out, error_out)) {
        return false;
    }
    if (!consumeXdrString(packet, offset, dpb, error_out)) {
        return false;
    }

    if (!decodeFirebirdBasicDpb(dpb,
                                user_name_out,
                                auth_plugin_out,
                                auth_data_out,
                                error_out)) {
        return false;
    }
    user_name_out = normalizeFirebirdDpbString(std::move(user_name_out));
    return true;
}

static auto decodeFirebirdConnectUserId(const std::string& user_id,
                                        std::string& user_name_out,
                                        std::string& auth_plugin_out,
                                        std::vector<uint8_t>& auth_data_out,
                                        bool& wire_encrypted_out,
                                        std::string& error_out) -> bool {
    size_t offset = 0;
    std::vector<std::pair<uint8_t, std::string>> auth_parts;
    while (offset < user_id.size()) {
        if (offset + 2 > user_id.size()) {
            error_out = "Malformed Firebird connect user-id clumplet header";
            return false;
        }
        const uint8_t type = static_cast<uint8_t>(user_id[offset++]);
        const uint8_t len = static_cast<uint8_t>(user_id[offset++]);
        if (offset + len > user_id.size()) {
            error_out = "Malformed Firebird connect user-id clumplet payload";
            return false;
        }

        const char* payload = user_id.data() + offset;
        switch (type) {
            case fb::CNCT_user:
            case fb::CNCT_login:
                user_name_out.assign(payload, payload + len);
                break;
            case fb::CNCT_plugin_name:
                auth_plugin_out.assign(payload, payload + len);
                break;
            case fb::CNCT_plugin_list:
                if (auth_plugin_out.empty()) {
                    const std::string list(payload, payload + len);
                    const auto comma = list.find(',');
                    auth_plugin_out = list.substr(0, comma);
                }
                break;
            case fb::CNCT_specific_data:
                if (len == 0) {
                    auth_parts.emplace_back(0, std::string());
                    break;
                }
                auth_parts.emplace_back(static_cast<uint8_t>(payload[0]),
                                        std::string(payload + 1, payload + len));
                break;
            case fb::CNCT_client_crypt:
                if (len > 0) {
                    wire_encrypted_out = static_cast<unsigned char>(payload[0]) != 0;
                }
                break;
            default:
                break;
        }

        offset += len;
    }

    std::sort(auth_parts.begin(),
              auth_parts.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
    auth_data_out.clear();
    for (const auto& part : auth_parts) {
        auth_data_out.insert(auth_data_out.end(), part.second.begin(), part.second.end());
    }
    return true;
}

// ============================================================================
// FirebirdParserAgent Implementation
// ============================================================================

FirebirdParserAgent::FirebirdParserAgent(const ParserAgentConfig& config)
    : EmulatedParserAgent(config, "firebird") {
}

FirebirdParserAgent::~FirebirdParserAgent() {
}

core::Status FirebirdParserAgent::handleClient(int client_fd, core::ErrorContext* ctx) {
    core::EmulationPackageBundle package_bundle{};
    core::EmulationPackageRequirement package_requirement{};
    package_requirement.parser = true;
    package_requirement.compiler_udr = true;
    package_requirement.emulation_udr = true;
    core::Status package_status = core::requireEmulationPackageBundle(
        "firebirdsql",
        package_requirement,
        package_bundle,
        ctx);
    if (package_status != core::Status::OK) {
        return package_status;
    }
    uint32_t client_id = next_client_id_++;
    auto client = std::make_unique<ClientConnection>();
    client->client_id = client_id;
    client->socket_fd = client_fd;
    client->connect_time_ms = getCurrentTimeMs();
    client->last_activity_ms = client->connect_time_ms;
    client->ipc_channel = acquireIPCChannel();
    if (!client->ipc_channel) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND,
                     "No IPC channel available for Firebird parser client",
                     __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }
    {
        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        connections_[client_id] = std::move(client);
    }
    updateStats([](Stats& s) { s.active_connections++; });

    FBClientState state;
    state.client_fd = client_fd;
    state.client_id = client_id;
    state.protocol_version = fb::PROTOCOL_VERSION18;
    state.accept_version = 0;
    state.wire_encrypted = false;
    state.handle = 0;
    state.attachment_id = 0;
    
    // Handle connection
    auto status = handleConnect(state, ctx);
    if (status != core::Status::OK) {
        disconnectClient(client_id);
        sendReject(state, 1, "Connection rejected");
        return status;
    }

    {
        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        auto it = connections_.find(client_id);
        if (it != connections_.end()) {
            it->second->database = state.database;
            it->second->user = state.username;
        }
    }
    
    // Send accept
    status = sendAccept(state, ctx);
    if (status != core::Status::OK) {
        disconnectClient(client_id);
        return status;
    }
    
    // Main operation loop
    while (state.state != FBClientState::DISCONNECTED) {
        status = handleOperation(state, ctx);
        if (status != core::Status::OK) {
            if (status == core::Status::CONNECTION_CLOSED) {
                break;
            }
            sendErrorResponse(state, ctx ? ctx->message : "Error occurred");
        }
    }

    (void)cleanupAttachmentState(state, ctx);
    disconnectClient(client_id);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleConnect(FBClientState& state, core::ErrorContext* ctx) {
    // Read connect packet
    std::vector<uint8_t> packet;
    auto status = readPacket(state, packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    std::cerr << "[parser_debug] firebird connect packet size=" << packet.size() << "\n";
    
    if (packet.size() < 16) {
        return core::Status::INVALID_ARGUMENT;
    }
    
    size_t offset = 0;
    
    // Operation code
    state.last_op = xdrReadUint32(packet.data() + offset);
    offset += 4;
    
    if (state.last_op != fb::op_connect && state.last_op != fb::op_attach) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT,
                     "Initial Firebird opcode was not op_connect/op_attach",
                     __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }

    state.accept_type = 1;

    const bool donor_connect_layout =
        state.last_op == fb::op_connect &&
        packet.size() >= 8 &&
        xdrReadUint32(packet.data() + 4) == 0;
    if (donor_connect_layout) {
        if (offset + 12 > packet.size()) {
            return core::Status::INVALID_ARGUMENT;
        }

        const uint32_t connect_operation = xdrReadUint32(packet.data() + offset);
        offset += 4;
        (void)connect_operation;

        state.protocol_version = xdrReadUint32(packet.data() + offset);
        offset += 4;

        const uint32_t client_arch = xdrReadUint32(packet.data() + offset);
        offset += 4;
        (void)client_arch;

        std::string decode_error;
        if (!consumeXdrString(packet, offset, state.database, decode_error)) {
            if (ctx) {
                ctx->set(core::Status::INVALID_ARGUMENT, decode_error.c_str(), __FILE__, __LINE__, __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }

        if (offset + 4 > packet.size()) {
            return core::Status::INVALID_ARGUMENT;
        }
        const uint32_t count = xdrReadUint32(packet.data() + offset);
        offset += 4;

        std::string user_id;
        if (!consumeXdrString(packet, offset, user_id, decode_error)) {
            if (ctx) {
                ctx->set(core::Status::INVALID_ARGUMENT, decode_error.c_str(), __FILE__, __LINE__, __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }
        if (!decodeFirebirdConnectUserId(user_id,
                                         state.username,
                                         state.auth_plugin,
                                         state.auth_data,
                                         state.wire_encrypted,
                                         decode_error)) {
            if (ctx) {
                ctx->set(core::Status::INVALID_ARGUMENT, decode_error.c_str(), __FILE__, __LINE__, __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }

        for (uint32_t i = 0; i < count && offset + 20 <= packet.size(); ++i) {
            const uint32_t version = xdrReadUint32(packet.data() + offset);
            offset += 4;
            const uint32_t arch = xdrReadUint32(packet.data() + offset);
            offset += 4;
            const uint32_t min_type = xdrReadUint32(packet.data() + offset);
            offset += 4;
            const uint32_t max_type = xdrReadUint32(packet.data() + offset);
            offset += 4;
            const uint32_t weight = xdrReadUint32(packet.data() + offset);
            offset += 4;
            (void)arch;
            (void)min_type;
            (void)weight;

            if (version <= fb::PROTOCOL_VERSION18 && version > state.accept_version) {
                state.accept_version = version;
                state.accept_type = max_type;
            }
        }

        if (state.accept_version == 0) {
            if (ctx) {
                ctx->set(core::Status::INVALID_ARGUMENT,
                         "Firebird connect negotiation found no supported protocol version",
                         __FILE__, __LINE__, __func__);
            }
            return core::Status::INVALID_ARGUMENT;
        }

        std::cerr << "[parser_debug] firebird negotiated connect accept_version="
                  << state.accept_version << " accept_type=" << state.accept_type
                  << " user=" << state.username << " db=" << state.database << "\n";

        return core::Status::OK;
    }

    // Legacy parser-local connect layout retained temporarily while the
    // remaining listener stack is rebased onto the donor Firebird wire shape.
    state.protocol_version = xdrReadUint32(packet.data() + offset);
    offset += 4;
    
    // Arch type (unused)
    offset += 4;
    
    // Minimum type (unused)
    offset += 4;
    
    // Maximum type (unused)
    offset += 4;
    
    // Page size (unused for connect)
    offset += 4;
    
    // Path length
    uint32_t path_len = xdrReadUint32(packet.data() + offset);
    offset += 4;
    
    // Database path
    if (path_len > 0 && offset + path_len <= packet.size()) {
        state.database.assign(reinterpret_cast<const char*>(packet.data() + offset), path_len);
        offset += path_len;
    }
    
    // Count of protocol versions offered
    uint32_t count = xdrReadUint32(packet.data() + offset);
    offset += 4;
    
    // Protocol version list
    for (uint32_t i = 0; i < count && offset + 8 <= packet.size(); i++) {
        uint32_t version = xdrReadUint32(packet.data() + offset);
        offset += 4;
        uint32_t arch = xdrReadUint32(packet.data() + offset);
        offset += 4;
        (void)arch;
        
        // Accept highest version we support
        if (version <= fb::PROTOCOL_VERSION18 && version > state.accept_version) {
            state.accept_version = version;
        }
    }
    
    // Authentication data in the legacy packet shape.
    while (offset + 5 <= packet.size()) {
        uint8_t type = packet[offset++];
        uint16_t len = xdrReadUint16(packet.data() + offset);
        offset += 2;
        
        if (offset + len > packet.size()) break;
        
        switch (type) {
            case fb::CNCT_login:
            case fb::CNCT_user:
                state.username.assign(reinterpret_cast<const char*>(packet.data() + offset), len);
                break;
            case fb::CNCT_plugin_name:
                state.auth_plugin.assign(reinterpret_cast<const char*>(packet.data() + offset), len);
                break;
            case fb::CNCT_specific_data:
                state.auth_data.assign(packet.begin() + static_cast<std::ptrdiff_t>(offset),
                                       packet.begin() + static_cast<std::ptrdiff_t>(offset + len));
                break;
            case fb::CNCT_plugin_list:
                if (state.auth_plugin.empty()) {
                    const std::string list(reinterpret_cast<const char*>(packet.data() + offset), len);
                    const auto comma = list.find(',');
                    state.auth_plugin = list.substr(0, comma);
                }
                break;
            case fb::CNCT_client_crypt:
                if (len > 0) {
                    state.wire_encrypted = static_cast<unsigned char>(packet[offset]) != 0;
                }
                break;
            default:
                break;
        }
        offset += len;
    }
    
    return core::Status::OK;
}

core::Status FirebirdParserAgent::sendAccept(FBClientState& state, core::ErrorContext* ctx) {
    std::vector<uint8_t> packet;
    xdrAppendUint32(packet, fb::op_accept_data);
    xdrAppendUint32(packet, state.accept_version);
    xdrAppendUint32(packet, fb::arch_generic);
    xdrAppendUint32(packet, state.accept_type == 0 ? 1u : state.accept_type);
    xdrAppendBuffer(packet, nullptr, 0);
    xdrAppendString(packet, "Legacy_Auth");
    xdrAppendUint32(packet, 0);
    xdrAppendBuffer(packet, nullptr, 0);

    // After accept, parse all subsequent packets using the negotiated wire version.
    state.protocol_version = state.accept_version;
    state.state = FBClientState::CONNECTED;

    std::cerr << "[parser_debug] firebird sendAccept version=" << state.accept_version
              << " type=" << state.accept_type << " packet_size=" << packet.size() << "\n";
    
    return sendPacket(state, packet, ctx);
}

void FirebirdParserAgent::sendReject(FBClientState& state, uint32_t error_code, 
                                    const std::string& message) {
    std::vector<uint8_t> packet;

    xdrAppendUint32(packet, fb::op_reject);
    xdrAppendUint32(packet, error_code);
    xdrAppendString(packet, message);

    sendPacket(state, packet, nullptr);
}

core::Status FirebirdParserAgent::closeEngineObject(FBClientState& state,
                                                   uint32_t request_id_seed,
                                                   char type,
                                                   const std::string& name,
                                                   core::ErrorContext* ctx) {
    IPCMessage close_msg(IPCMessageType::CLOSE, 0);
    close_msg.header.request_id = state.session_id != 0 ? state.session_id : request_id_seed;
    IPCClosePayload payload{};
    payload.type = static_cast<uint8_t>(type);
    std::strncpy(payload.name, name.c_str(), sizeof(payload.name) - 1);
    payload.name[sizeof(payload.name) - 1] = '\0';
    close_msg.payload.resize(sizeof(payload));
    std::memcpy(close_msg.payload.data(), &payload, sizeof(payload));

    auto status = sendToEngine(state.client_id, close_msg, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    IPCMessage response;
    status = receiveFromEngine(state.client_id, response, ctx, 30000);
    if (status != core::Status::OK) {
        return status;
    }
    if (response.getType() == IPCMessageType::ERROR_RESPONSE) {
        return sendErrorResponse(state, engineErrorMessage(response));
    }
    if (response.getType() != IPCMessageType::CLOSE_COMPLETE) {
        return sendErrorResponse(state, "Unexpected engine response during Firebird engine close");
    }
    return core::Status::OK;
}

core::Status FirebirdParserAgent::cleanupAttachmentState(FBClientState& state,
                                                        core::ErrorContext* ctx) {
    if (state.attachment_id == 0) {
        state.dsql_statements.clear();
        state.compiled_requests.clear();
        state.transactions.clear();
        state.handle = 0;
        state.database.clear();
        return core::Status::OK;
    }

    for (auto& entry : state.dsql_statements) {
        const uint32_t statement_handle = entry.first;
        auto& stmt_state = entry.second;
        if (stmt_state.portal_active) {
            auto status = closeEngineObject(state, statement_handle, 'P',
                                            firebirdDsqlPortalName(statement_handle), ctx);
            if (status != core::Status::OK) {
                return status;
            }
            stmt_state.portal_active = false;
        }
        if (stmt_state.engine_statement_prepared && !stmt_state.stmt_name.empty()) {
            auto status = closeEngineObject(state, statement_handle, 'S', stmt_state.stmt_name, ctx);
            if (status != core::Status::OK) {
                return status;
            }
            stmt_state.engine_statement_prepared = false;
            stmt_state.statement_prepared = false;
        }
    }

    for (auto& entry : state.compiled_requests) {
        const uint32_t request_handle = entry.first;
        auto& request_state = entry.second;
        if (request_state.portal_active) {
            auto status = closeEngineObject(state, request_handle, 'P',
                                            firebirdPortalName(request_handle), ctx);
            if (status != core::Status::OK) {
                return status;
            }
            request_state.portal_active = false;
        }
        if (request_state.statement_prepared && !request_state.stmt_name.empty()) {
            auto status = closeEngineObject(state, request_handle, 'S', request_state.stmt_name, ctx);
            if (status != core::Status::OK) {
                return status;
            }
            request_state.statement_prepared = false;
        }
    }

    state.dsql_statements.clear();
    state.compiled_requests.clear();
    state.transactions.clear();
    state.attachment_id = 0;
    state.handle = 0;
    state.database.clear();
    state.emulated_schema_root.clear();
    return core::Status::OK;
}

core::Status FirebirdParserAgent::ensureEngineSession(FBClientState& state,
                                                     bool create_database_bootstrap,
                                                     core::ErrorContext* ctx) {
    if (state.session_id == 0) {
        std::string engine_database = state.database;
        const auto default_db_it = config_.options.find("default_database");
        if (default_db_it != config_.options.end() && !default_db_it->second.empty()) {
            engine_database = default_db_it->second;
        }
        std::string engine_user = "SysArch";
        const auto engine_user_it = config_.options.find("engine_user");
        if (engine_user_it != config_.options.end() && !engine_user_it->second.empty()) {
            engine_user = engine_user_it->second;
        }
        const std::string session_user = !state.username.empty() ? state.username : engine_user;

        std::cerr << "[parser_debug] ensureEngineSession client_id=" << state.client_id
                  << " engine_database=" << engine_database
                  << " emulated_db=" << state.database
                  << " user=" << state.username
                  << " engine_user=" << engine_user
                  << " session_user=" << session_user << "\n";

        IPCMessage startup(IPCMessageType::STARTUP, 0);
        IPCStartupPayload startup_payload{};
        startup_payload.process_id = state.client_id;
        startup_payload.secret_key = state.client_id;
        startup_payload.feature_flags = IPC_FEATURE_PREPARED_STATEMENTS |
                                        IPC_FEATURE_BINARY_RESULTS;
        std::strncpy(startup_payload.database, engine_database.c_str(),
                     sizeof(startup_payload.database) - 1);
        startup_payload.database[sizeof(startup_payload.database) - 1] = '\0';
        std::strncpy(startup_payload.user, session_user.c_str(),
                     sizeof(startup_payload.user) - 1);
        startup_payload.user[sizeof(startup_payload.user) - 1] = '\0';
        std::strncpy(startup_payload.application, "firebird_parser",
                     sizeof(startup_payload.application) - 1);
        startup_payload.application[sizeof(startup_payload.application) - 1] = '\0';
        startup.payload.resize(sizeof(startup_payload));
        std::memcpy(startup.payload.data(), &startup_payload, sizeof(startup_payload));

        auto status = sendToEngine(state.client_id, startup, ctx);
        if (status != core::Status::OK) {
            std::cerr << "[parser_debug] ensureEngineSession sendToEngine failed status="
                      << static_cast<int>(status)
                      << " message=" << (ctx ? ctx->message : std::string()) << "\n";
            return status;
        }

        IPCMessage startup_response;
        status = receiveFromEngine(state.client_id, startup_response, ctx, 30000);
        if (status != core::Status::OK) {
            std::cerr << "[parser_debug] ensureEngineSession receiveFromEngine failed status="
                      << static_cast<int>(status)
                      << " message=" << (ctx ? ctx->message : std::string()) << "\n";
            return status;
        }
        if (startup_response.getType() != IPCMessageType::READY) {
            std::cerr << "[parser_debug] ensureEngineSession unexpected response type="
                      << static_cast<int>(startup_response.getType()) << "\n";
            if (ctx) {
                ctx->set(core::Status::CONNECTION_FAILURE,
                         "Firebird parser did not receive IPC READY during startup",
                         __FILE__, __LINE__, __func__);
            }
            return core::Status::CONNECTION_FAILURE;
        }

        if (auto* ready = startup_response.getPayload<IPCReadyPayload>()) {
            state.session_id = ready->session_id;
            std::cerr << "[parser_debug] ensureEngineSession ready session_id="
                      << state.session_id << "\n";
            std::unique_lock<std::shared_mutex> lock(connections_mutex_);
            auto it = connections_.find(state.client_id);
            if (it != connections_.end()) {
                it->second->session_id = ready->session_id;
                it->second->database = engine_database;
                it->second->user = session_user;
            }
        }
    }

    if (!state.database.empty()) {
        udr::FirebirdSchemaBindingRequest binding_request{};
        binding_request.profile_id = "firebirdsql";
        binding_request.database_binding = state.database;
        udr::FirebirdSchemaBindingResponse binding_response{};
        core::ErrorContext binding_ctx;
        auto binding_status = udr::deriveFirebirdSchemaBinding(binding_request,
                                                               binding_response,
                                                               &binding_ctx);
        if (binding_status != core::Status::OK) {
            if (ctx && !binding_ctx.message.empty()) {
                ctx->set(binding_ctx.code,
                         binding_ctx.message.c_str(),
                         binding_ctx.file,
                         binding_ctx.line,
                         binding_ctx.function);
            }
            return binding_status;
        }
        state.emulated_schema_root = binding_response.schema_name;

        if (create_database_bootstrap) {
            udr::FirebirdLifecycleSqlRequest lifecycle_request{};
            lifecycle_request.profile_id = "firebirdsql";
            lifecycle_request.operation = udr::FirebirdEmulationLifecycleOperation::CREATE_DATABASE;
            lifecycle_request.database_spec = state.database;
            udr::FirebirdLifecycleSqlResponse lifecycle_response{};
            core::ErrorContext lifecycle_ctx;
            auto lifecycle_status = udr::renderFirebirdLifecycleSql(lifecycle_request,
                                                                    lifecycle_response,
                                                                    &lifecycle_ctx);
            if (lifecycle_status != core::Status::OK) {
                if (ctx && !lifecycle_ctx.message.empty()) {
                    ctx->set(lifecycle_ctx.code,
                             lifecycle_ctx.message.c_str(),
                             lifecycle_ctx.file,
                             lifecycle_ctx.line,
                             lifecycle_ctx.function);
                }
                return lifecycle_status;
            }

            if (!lifecycle_response.sql.empty()) {
                auto create_status =
                    executeSessionSql(state,
                                      lifecycle_response.sql,
                                      false,
                                      "Failed to provision Firebird emulated database root",
                                      ctx);
                if (create_status != core::Status::OK) {
                    return create_status;
                }
                auto commit_status =
                    executeSessionSql(state,
                                      "COMMIT",
                                      false,
                                      "Failed to commit Firebird create database bootstrap",
                                      ctx);
                if (commit_status != core::Status::OK) {
                    return commit_status;
                }
            }
        }

        auto ensure_status = ensureVirtualCatalogBinding(state, ctx);
        if (ensure_status != core::Status::OK) {
            return ensure_status;
        }

        if (!binding_response.schema_name.empty()) {
            const std::string set_search_path_sql =
                "EXECUTE PROCEDURE fb_set_search_path('" +
                escapeFirebirdSqlLiteral(binding_response.schema_name) + "')";
            auto path_status = executeSessionSql(state,
                                                 set_search_path_sql,
                                                 false,
                                                 "Failed to bind Firebird emulated schema root",
                                                 ctx);
            if (path_status != core::Status::OK) {
                return path_status;
            }
        }
    }

    return core::Status::OK;
}

core::Status FirebirdParserAgent::ensureVirtualCatalogBinding(FBClientState& state,
                                                              core::ErrorContext* ctx) {
    if (state.database.empty()) {
        return core::Status::OK;
    }

    const std::string ensure_catalog_sql =
        "EXECUTE PROCEDURE fb_ensure_virtual_catalog('" +
        escapeFirebirdSqlLiteral(state.database) + "')";
    auto ensure_status = executeSessionSql(state,
                                           ensure_catalog_sql,
                                           false,
                                           "Failed to ensure Firebird virtual catalog",
                                           ctx);
    if (ensure_status != core::Status::OK) {
        return ensure_status;
    }

    return executeSessionSql(state,
                             "COMMIT",
                             false,
                             "Failed to commit Firebird virtual catalog setup",
                             ctx);
}

core::Status FirebirdParserAgent::executeSessionSql(FBClientState& state,
                                                    const std::string& sql,
                                                    bool ignore_exists_error,
                                                    const char* failure_message,
                                                    core::ErrorContext* ctx) {
    auto set_error_from_message = [&](core::Status code,
                                      const std::string& message,
                                      const char* fallback) {
        if (ctx) {
            ctx->set(code,
                     message.empty() ? fallback : message.c_str(),
                     __FILE__, __LINE__, __func__);
        }
    };

    auto lower_ascii = [](const std::string& in) {
        std::string out = in;
        std::transform(out.begin(), out.end(), out.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return out;
    };

    core::ErrorContext query_ctx;
    auto query_status =
        executeCompiledInternalQuery(state, sql, nullptr, nullptr, &query_ctx);
    if (query_status == core::Status::OK) {
        return core::Status::OK;
    }

    const std::string engine_message =
        query_ctx.message.empty() ? std::string(failure_message) : query_ctx.message;
    const std::string lowered = lower_ascii(engine_message);
    if (ignore_exists_error &&
        (lowered.find("already exists") != std::string::npos ||
         lowered.find("duplicate") != std::string::npos ||
         lowered.find("exists") != std::string::npos)) {
        return core::Status::OK;
    }

    set_error_from_message(query_status,
                           engine_message,
                           failure_message);
    return query_status;
}

core::Status FirebirdParserAgent::executeCompiledInternalQuery(
    FBClientState& state,
    const std::string& sql_text,
    std::vector<std::string>* column_names_out,
    std::deque<std::vector<std::optional<std::string>>>* rows_out,
    core::ErrorContext* ctx) {
    const std::string effective_sql = rewriteFirebirdSingleRowCompatibilityQuery(sql_text);

    sblr::DialectCompilerRequest request{};
    request.request_id = core::generateUuidV7();
    populateFirebirdCompilerRequest(request, state);
    request.payload_format = sblr::DialectCompilerPayloadFormat::SQL_TEXT;
    request.payload.assign(effective_sql.begin(), effective_sql.end());

    sblr::DialectCompilerResponse response{};
    core::ErrorContext compile_ctx;
    auto status = sblr::compileFirebirdDialectToSblr(nullptr, request, response, &compile_ctx);
    if (status != core::Status::OK || !response.success) {
        const std::string message = !response.errors.empty()
                                        ? response.errors.front()
                                        : (compile_ctx.message.empty()
                                               ? "Firebird internal SQL to SBLR lowering failed"
                                               : compile_ctx.message);
        if (ctx) {
            ctx->set(status == core::Status::OK ? core::Status::INVALID_ARGUMENT : status,
                     message.c_str(),
                     __FILE__, __LINE__, __func__);
        }
        return status == core::Status::OK ? core::Status::INVALID_ARGUMENT : status;
    }

    status = sendCompiledQueryToEngine(state.client_id,
                                       state.session_id != 0 ? state.session_id : generateHandle(),
                                       response.bytecode,
                                       effective_sql,
                                       ctx);
    if (status != core::Status::OK) {
        return status;
    }

    if (column_names_out != nullptr) {
        column_names_out->clear();
    }
    if (rows_out != nullptr) {
        rows_out->clear();
    }

    IPCMessage engine_response;
    while (true) {
        status = receiveFromEngine(state.client_id, engine_response, ctx, 30000);
        if (status != core::Status::OK) {
            return status;
        }

        switch (engine_response.getType()) {
            case IPCMessageType::ROW_DESCRIPTION:
                if (column_names_out != nullptr) {
                    std::string decode_error;
                    if (!decodeIpcRowDescription(engine_response,
                                                 *column_names_out,
                                                 decode_error)) {
                        if (ctx) {
                            ctx->set(core::Status::INVALID_ARGUMENT,
                                     decode_error.c_str(),
                                     __FILE__, __LINE__, __func__);
                        }
                        return core::Status::INVALID_ARGUMENT;
                    }
                }
                break;
            case IPCMessageType::DATA_ROW:
                if (rows_out != nullptr) {
                    std::vector<std::optional<std::string>> row;
                    std::string decode_error;
                    if (!decodeIpcDataRow(engine_response, row, decode_error)) {
                        if (ctx) {
                            ctx->set(core::Status::INVALID_ARGUMENT,
                                     decode_error.c_str(),
                                     __FILE__, __LINE__, __func__);
                        }
                        return core::Status::INVALID_ARGUMENT;
                    }
                    rows_out->push_back(std::move(row));
                }
                break;
            case IPCMessageType::COMMAND_COMPLETE:
            case IPCMessageType::EMPTY_RESPONSE:
            case IPCMessageType::READY:
            case IPCMessageType::READY_FOR_QUERY:
                return core::Status::OK;
            case IPCMessageType::PARSE_COMPLETE:
            case IPCMessageType::BIND_COMPLETE:
            case IPCMessageType::CLOSE_COMPLETE:
            case IPCMessageType::NOTICE:
                break;
            case IPCMessageType::ERROR_RESPONSE:
            {
                const std::string engine_message = engineErrorMessage(engine_response);
                if (ctx) {
                    ctx->set(core::Status::INVALID_ARGUMENT,
                             engine_message.c_str(),
                             __FILE__, __LINE__, __func__);
                }
                return core::Status::INVALID_ARGUMENT;
            }
            default:
            {
                const std::string message =
                    "Unexpected engine response for Firebird compiled internal query: " +
                    std::string(ipcMessageTypeToString(engine_response.getType()));
                if (ctx) {
                    ctx->set(core::Status::PROTOCOL_VIOLATION,
                             message.c_str(),
                             __FILE__, __LINE__, __func__);
                }
                return core::Status::PROTOCOL_VIOLATION;
            }
        }
    }
}

core::Status FirebirdParserAgent::refreshCommittedCatalogState(FBClientState& state,
                                                               core::ErrorContext* ctx) {
    if (state.emulated_schema_root.empty()) {
        return core::Status::OK;
    }

    const std::string set_search_path_sql =
        "EXECUTE PROCEDURE fb_set_search_path('" +
        escapeFirebirdSqlLiteral(state.emulated_schema_root) + "')";
    auto path_status = executeSessionSql(state,
                                         set_search_path_sql,
                                         false,
                                         "Failed to bind Firebird emulated schema root",
                                         ctx);
    if (path_status != core::Status::OK) {
        return path_status;
    }

    auto refresh_status = executeSessionSql(state,
                                            "EXECUTE PROCEDURE fb_refresh_virtual_catalog()",
                                            false,
                                            "Failed to refresh Firebird virtual catalog",
                                            ctx);
    if (refresh_status != core::Status::OK) {
        return refresh_status;
    }

    auto commit_status = executeSessionSql(state,
                                           "COMMIT",
                                           false,
                                           "Failed to commit Firebird virtual catalog refresh",
                                           ctx);
    if (commit_status != core::Status::OK) {
        return commit_status;
    }

    return executeSessionSql(state,
                             set_search_path_sql,
                             false,
                             "Failed to restore Firebird emulated schema root",
                             ctx);
}

core::Status FirebirdParserAgent::handleOperation(FBClientState& state, core::ErrorContext* ctx) {
    std::vector<uint8_t> packet;
    auto status = readPacket(state, packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (packet.size() < 4) {
        return core::Status::INVALID_ARGUMENT;
    }
    
    uint32_t op = xdrReadUint32(packet.data());
    state.last_op = op;
    std::cerr << "[parser_debug] firebird op=" << op << " packet_size=" << packet.size() << "\n";
    
    switch (op) {
        case fb::op_connect:
            return sendErrorResponse(state, "op_connect is only valid during initial handshake");
        case fb::op_exit:
            (void)cleanupAttachmentState(state, ctx);
            state.state = FBClientState::DISCONNECTED;
            return core::Status::CONNECTION_CLOSED;
        case fb::op_protocol:
            state.accept_version = state.protocol_version;
            return sendAccept(state, ctx);
        case fb::op_attach:
            return handleAttach(state, packet, ctx);
        case fb::op_create:
            return handleCreate(state, packet, ctx);
        case fb::op_detach:
            return handleDetach(state, packet, ctx);
        case fb::op_compile:
            return handleCompile(state, packet, ctx);
        case fb::op_allocate_statement:
            return handleAllocateStatement(state, packet, ctx);
        case fb::op_prepare_statement:
            return handlePrepareStatement(state, packet, ctx);
        case fb::op_exec_immediate:
            return handleExecImmediate(state, packet, false, ctx);
        case fb::op_exec_immediate2:
            return handleExecImmediate(state, packet, true, ctx);
        case fb::op_execute:
            return handleExecuteStatement(state, packet, false, ctx);
        case fb::op_execute2:
            return handleExecuteStatement(state, packet, true, ctx);
        case fb::op_fetch:
            return handleFetchStatement(state, packet, false, ctx);
        case fb::op_fetch_scroll:
            return handleFetchStatement(state, packet, true, ctx);
        case fb::op_free_statement:
            return handleFreeStatement(state, packet, ctx);
        case fb::op_set_cursor:
            return handleSetCursor(state, packet, ctx);
        case fb::op_transaction:
            return handleTransaction(state, packet, ctx);
        case fb::op_commit:
        case fb::op_commit_retaining:
            return handleCommit(state, packet, op == fb::op_commit_retaining, ctx);
        case fb::op_rollback:
        case fb::op_rollback_retaining:
            return handleRollback(state, packet, op == fb::op_rollback_retaining, ctx);
        case fb::op_prepare:
            return handlePrepare(state, packet, ctx);
        case fb::op_prepare2:
            return handlePrepare2(state, packet, ctx);
        case fb::op_reconnect:
            if (state.attachment_id == 0) {
                state.attachment_id = generateHandle();
            }
            state.handle = state.attachment_id;
            sendResponse(state, state.handle, 0, nullptr, 0, ctx);
            return core::Status::OK;
        case fb::op_start:
            return handleStart(state, packet, false, ctx);
        case fb::op_start_and_send:
            return handleStartWithSend(state, packet, false, ctx);
        case fb::op_start_and_receive:
            return handleStart(state, packet, true, ctx);
        case fb::op_start_send_and_receive:
            return handleStartWithSend(state, packet, true, ctx);
        case fb::op_receive:
            return handleReceive(state, packet, ctx);
        case fb::op_send:
            return handleSend(state, packet, ctx);
        case fb::op_unwind:
            return handleUnwind(state, packet, ctx);
        case fb::op_release:
            return handleRelease(state, packet, ctx);
        case fb::op_info_database:
        case fb::op_info_request:
        case fb::op_info_transaction:
        case fb::op_info_blob:
        case fb::op_info_sql:
        case fb::op_info_cursor:
            return handleInfo(state, packet, op, ctx);
        case fb::op_open_blob:
        case fb::op_create_blob:
            return handleBlobOpen(state, packet, op == fb::op_create_blob, ctx);
        case fb::op_get_segment:
            return handleBlobGetSegment(state, packet, ctx);
        case fb::op_batch_segments:
        case fb::op_put_segment:
            return handleBlobPutSegment(state, packet, ctx);
        case fb::op_close_blob:
        case fb::op_cancel_blob:
            return handleBlobClose(ctx, op == fb::op_cancel_blob);
        case fb::op_que_events:
        case fb::op_cancel_events:
            sendResponse(state, 0, 0, nullptr, 0, ctx);
            return core::Status::OK;
        case fb::op_dummy:
            return sendPacket(state, std::vector<uint8_t>{0, 0, 0, static_cast<uint8_t>(fb::op_dummy)}, ctx);
        case fb::op_cancel:
            // Bounded parser-owned cancel handling: accept the async cancel packet
            // and preserve session state. No synchronous response is expected.
            return core::Status::OK;
        case fb::op_ping:
            sendResponse(state, 0, 0, nullptr, 0, ctx);
            return core::Status::OK;
        case fb::op_disconnect:
            (void)cleanupAttachmentState(state, ctx);
            state.state = FBClientState::DISCONNECTED;
            return core::Status::OK;
        case fb::op_credit:
            // Flow-control credit updates are acknowledged by preserving connection state.
            return core::Status::OK;
        case fb::op_crypt:
        case fb::op_crypt_callback:
            return handleCrypt(state, packet, ctx);
        case fb::op_authenticate_user:
        case fb::op_cont_auth:
            return handleAuthenticate(state, packet, ctx);
        default:
            return sendErrorResponse(state, "Unsupported operation: " + std::to_string(op));
    }
}

core::Status FirebirdParserAgent::handleAttach(FBClientState& state,
                                              const std::vector<uint8_t>& packet,
                                              core::ErrorContext* ctx) {
    std::string database_path;
    std::string user_name;
    std::string auth_plugin;
    std::vector<uint8_t> auth_data;
    std::string decode_error;
    if (!decodeFirebirdAttachPacket(packet, database_path, user_name, auth_plugin, auth_data, decode_error)) {
        return sendErrorResponse(state, decode_error);
    }

    std::cerr << "[parser_debug] firebird attach db=" << database_path
              << " user=" << user_name
              << " existing_db=" << state.database
              << " existing_user=" << state.username << "\n";

    if (!database_path.empty()) {
        if (!state.database.empty() && state.database != database_path) {
            return sendErrorResponse(state,
                                     "Firebird attach database path conflicts with established session database");
        }
        state.database = database_path;
    }
    if (!user_name.empty()) {
        if (!state.username.empty() && state.username != user_name) {
            return sendErrorResponse(state,
                                     "Firebird attach user conflicts with established session user");
        }
        state.username = user_name;
    }
    if (!auth_plugin.empty()) {
        state.auth_plugin = auth_plugin;
    }
    if (!auth_data.empty()) {
        state.auth_data = std::move(auth_data);
    }

    auto ensure_status = ensureEngineSession(state, false, ctx);
    if (ensure_status != core::Status::OK) {
        return ensure_status;
    }

    if (state.attachment_id != 0) {
        auto status = cleanupAttachmentState(state, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }

    // Create attachment handle
    state.attachment_id = generateHandle();
    state.handle = state.attachment_id;

    {
        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        auto it = connections_.find(state.client_id);
        if (it != connections_.end()) {
            it->second->database = state.database;
            it->second->user = state.username;
        }
    }
    
    // Send response
    sendResponse(state, state.handle, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleCreate(FBClientState& state,
                                              const std::vector<uint8_t>& packet,
                                              core::ErrorContext* ctx) {
    std::string database_path;
    std::string user_name;
    std::string auth_plugin;
    std::vector<uint8_t> auth_data;
    std::string decode_error;
    if (!decodeFirebirdAttachPacket(packet, database_path, user_name, auth_plugin, auth_data, decode_error)) {
        return sendErrorResponse(state, decode_error);
    }

    std::cerr << "[parser_debug] firebird create db=" << database_path
              << " user=" << user_name
              << " existing_db=" << state.database
              << " existing_user=" << state.username << "\n";

    if (!database_path.empty()) {
        if (!state.database.empty() && state.database != database_path) {
            return sendErrorResponse(state,
                                     "Firebird create database path conflicts with established session database");
        }
        state.database = database_path;
    }
    if (!user_name.empty()) {
        if (!state.username.empty() && state.username != user_name) {
            return sendErrorResponse(state,
                                     "Firebird create user conflicts with established session user");
        }
        state.username = user_name;
    }
    if (!auth_plugin.empty()) {
        state.auth_plugin = auth_plugin;
    }
    if (!auth_data.empty()) {
        state.auth_data = std::move(auth_data);
    }

    auto ensure_status = ensureEngineSession(state, true, ctx);
    if (ensure_status != core::Status::OK) {
        return ensure_status;
    }

    if (state.attachment_id != 0) {
        auto status = cleanupAttachmentState(state, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }

    state.attachment_id = generateHandle();
    state.handle = state.attachment_id;

    {
        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        auto it = connections_.find(state.client_id);
        if (it != connections_.end()) {
            it->second->database = state.database;
            it->second->user = state.username;
        }
    }

    sendResponse(state, state.handle, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleDetach(FBClientState& state,
                                              const std::vector<uint8_t>& packet,
                                              core::ErrorContext* ctx) {
    if (packet.size() < 8) {
        return sendErrorResponse(state, "Malformed Firebird detach packet");
    }

    const uint32_t database_handle = xdrReadUint32(packet.data() + 4);
    if (state.attachment_id == 0 || database_handle != state.attachment_id) {
        return sendErrorResponse(state, "Unknown Firebird database handle");
    }
    auto status = cleanupAttachmentState(state, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    state.state = FBClientState::CONNECTED;

    sendResponse(state, 0, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleCompile(FBClientState& state,
                                               const std::vector<uint8_t>& packet,
                                               core::ErrorContext* ctx) {
    if (packet.size() < 12) {
        return sendErrorResponse(state, "Malformed Firebird op_compile packet");
    }

    size_t offset = 4; // operation already read
    const uint32_t db_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    (void)db_handle;

    const uint32_t blr_length = xdrReadUint32(packet.data() + offset);
    offset += 4;
    if (offset + blr_length > packet.size()) {
        return sendErrorResponse(state, "Malformed Firebird BLR payload");
    }

    sblr::DialectCompilerRequest request{};
    request.request_id = core::generateUuidV7();
    populateFirebirdCompilerRequest(request, state);
    request.payload_format = sblr::DialectCompilerPayloadFormat::FIREBIRD_BLR;
    request.payload.assign(packet.begin() + offset,
                           packet.begin() + offset + blr_length);

    sblr::DialectCompilerResponse response{};
    core::ErrorContext compile_ctx;
    auto status = sblr::compileFirebirdDialectToSblr(nullptr, request, response, &compile_ctx);
    if (status != core::Status::OK || !response.success) {
        std::string message = !response.errors.empty()
                                ? response.errors.front()
                                : (compile_ctx.message.empty()
                                     ? "Firebird BLR to SBLR lowering failed"
                                     : compile_ctx.message);
        return sendErrorResponse(state, message);
    }

    FBCompiledRequestState request_state;
    request_state.stmt_name = "fb_req_pending";
    std::string layout_error;
    FirebirdBlrLayoutDecoder layout_decoder(request.payload);
    if (!layout_decoder.decode(request_state, layout_error)) {
        return sendErrorResponse(state,
                                 layout_error.empty()
                                     ? "Firebird BLR output message layout decode failed"
                                     : layout_error);
    }

    const uint32_t request_handle = generateHandle();
    const std::string stmt_name = "fb_req_" + std::to_string(request_handle);
    request_state.stmt_name = stmt_name;
    request_state.bound_params.resize(request_state.input_bindings.size());
    request_state.bound_param_nulls.resize(request_state.input_bindings.size(), false);
    request_state.input_values_ready = request_state.input_bindings.empty();
    status = sendCompiledParseToEngine(state.client_id,
                                       state.session_id != 0 ? state.session_id : request_handle,
                                       stmt_name,
                                       response.bytecode,
                                       std::string(),
                                       ctx);
    if (status != core::Status::OK) {
        return sendErrorResponse(state, "Failed to submit compiled Firebird request to engine");
    }

    IPCMessage engine_response;
    status = receiveFromEngine(state.client_id, engine_response, ctx, 30000);
    if (status != core::Status::OK) {
        return sendErrorResponse(state, "No engine response for compiled Firebird request");
    }
    if (engine_response.getType() == IPCMessageType::ERROR_RESPONSE) {
        auto* error = engine_response.getPayload<IPCErrorPayload>();
        return sendErrorResponse(state,
                                 error != nullptr ? std::string(error->message)
                                                  : "Engine rejected compiled Firebird request");
    }
    if (engine_response.getType() != IPCMessageType::PARSE_COMPLETE) {
        return sendErrorResponse(state, "Unexpected engine response for Firebird request compile");
    }

    state.compiled_requests[request_handle] = std::move(request_state);
    state.compiled_requests[request_handle].statement_prepared = true;
    state.handle = request_handle;
    sendResponse(state, request_handle, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleAllocateStatement(FBClientState& state,
                                                         const std::vector<uint8_t>& packet,
                                                         core::ErrorContext* ctx) {
    if (packet.size() < 8) {
        return sendErrorResponse(state, "Malformed Firebird op_allocate_statement packet");
    }

    const uint32_t statement_handle = generateHandle();
    FBDsqlStatementState stmt_state;
    stmt_state.stmt_name = "fb_dsql_" + std::to_string(statement_handle);
    state.dsql_statements[statement_handle] = std::move(stmt_state);
    sendResponse(state, statement_handle, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handlePrepareStatement(FBClientState& state,
                                                        const std::vector<uint8_t>& packet,
                                                        core::ErrorContext* ctx) {
    if (packet.size() < 20) {
        return sendErrorResponse(state, "Malformed Firebird op_prepare_statement packet");
    }

    size_t offset = 4;
    uint32_t transaction_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    (void)transaction_handle;
    const uint32_t statement_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    const uint32_t sql_dialect = xdrReadUint32(packet.data() + offset);
    offset += 4;
    (void)sql_dialect;

    if (transaction_handle == 0) {
        FBTransactionState txn_state;
        txn_state.transaction_id = generateHandle();
        txn_state.oldest_interesting = txn_state.transaction_id;
        txn_state.oldest_snapshot = txn_state.transaction_id;
        txn_state.oldest_active = txn_state.transaction_id;
        txn_state.snapshot_number = txn_state.transaction_id;
        txn_state.prepared = false;
        txn_state.prepare_description.clear();
        txn_state.database_path = state.database;
        transaction_handle = txn_state.transaction_id;
        state.transactions[transaction_handle] = std::move(txn_state);
    }

    std::string decode_error;
    std::string sql_text;
    if (!consumeXdrString(packet, offset, sql_text, decode_error)) {
        return sendErrorResponse(state, decode_error);
    }
    std::string items;
    if (!consumeXdrString(packet, offset, items, decode_error)) {
        return sendErrorResponse(state, decode_error);
    }
    if (offset + 4 > packet.size()) {
        return sendErrorResponse(state, "Malformed Firebird prepare buffer length");
    }
    const uint32_t buffer_length = xdrReadUint32(packet.data() + offset);
    offset += 4;
    (void)buffer_length;

    auto stmt_it = state.dsql_statements.find(statement_handle);
    if (stmt_it == state.dsql_statements.end()) {
        return sendErrorResponse(state, "Unknown Firebird DSQL statement handle");
    }

    if (stmt_it->second.portal_active) {
        auto status = closeEngineObject(state, statement_handle, 'P',
                                        firebirdDsqlPortalName(statement_handle), ctx);
        if (status != core::Status::OK) {
            return status;
        }
        stmt_it->second.portal_active = false;
    }
    if (stmt_it->second.engine_statement_prepared && !stmt_it->second.stmt_name.empty()) {
        auto status = closeEngineObject(state, statement_handle, 'S', stmt_it->second.stmt_name, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        stmt_it->second.engine_statement_prepared = false;
        stmt_it->second.statement_prepared = false;
    }
    stmt_it->second.compiled_bytecode.clear();

    const auto sql_batch = splitFirebirdSqlBatch(sql_text);
    if (sql_batch.size() > 1) {
        stmt_it->second.sql_text = std::move(sql_text);
        stmt_it->second.input_message_fields.clear();
        stmt_it->second.output_message_fields.clear();
        stmt_it->second.input_sqlda_fields.clear();
        stmt_it->second.output_sqlda_fields.clear();
        stmt_it->second.output_field_names.clear();
        stmt_it->second.pending_rows.clear();
        stmt_it->second.select_count = 0;
        stmt_it->second.insert_count = 0;
        stmt_it->second.update_count = 0;
        stmt_it->second.delete_count = 0;
        stmt_it->second.execution_complete = false;
        stmt_it->second.portal_active = false;
        stmt_it->second.statement_prepared = false;
        stmt_it->second.engine_statement_prepared = false;
        stmt_it->second.bound_params.clear();
        stmt_it->second.bound_param_nulls.clear();

        std::vector<uint8_t> info_buffer;
        buildFirebirdStatementInfoBuffer(stmt_it->second, items, buffer_length, info_buffer);
        sendResponse(state,
                     0,
                     0,
                     info_buffer.empty() ? nullptr : info_buffer.data(),
                     info_buffer.size(),
                     ctx);
        return core::Status::OK;
    }

    const std::string effective_sql = rewriteFirebirdSingleRowCompatibilityQuery(sql_text);

    sblr::DialectCompilerRequest request{};
    request.request_id = core::generateUuidV7();
    populateFirebirdCompilerRequest(request, state);
    request.payload_format = sblr::DialectCompilerPayloadFormat::SQL_TEXT;
    request.payload.assign(effective_sql.begin(), effective_sql.end());
    std::cerr << "[parser_debug] firebird prepare stage=compile_begin handle="
              << statement_handle << " sql=" << effective_sql << "\n";

    sblr::DialectCompilerResponse response{};
    core::ErrorContext compile_ctx;
    auto status = sblr::compileFirebirdDialectToSblr(nullptr, request, response, &compile_ctx);
    if (status != core::Status::OK || !response.success) {
        std::string message = !response.errors.empty()
                                ? response.errors.front()
                                : (compile_ctx.message.empty()
                                     ? "Firebird SQL to SBLR lowering failed"
                                     : compile_ctx.message);
        std::cerr << "[parser_debug] firebird prepare compile failed sql=" << effective_sql
                  << " message=" << message << "\n";
        return sendErrorResponse(state, message);
    }
    std::cerr << "[parser_debug] firebird prepare stage=compile_ok handle="
              << statement_handle << " stmt_type="
              << inferFirebirdStatementType(effective_sql) << "\n";

    stmt_it->second.sql_text = effective_sql;
    stmt_it->second.compiled_bytecode = response.bytecode;
    stmt_it->second.input_message_fields.clear();
    stmt_it->second.output_message_fields.clear();
    stmt_it->second.input_sqlda_fields.clear();
    stmt_it->second.output_sqlda_fields.clear();
    stmt_it->second.output_field_names.clear();
    stmt_it->second.pending_rows.clear();
    stmt_it->second.select_count = 0;
    stmt_it->second.insert_count = 0;
    stmt_it->second.update_count = 0;
    stmt_it->second.delete_count = 0;
    stmt_it->second.execution_complete = false;
    stmt_it->second.portal_active = false;
    stmt_it->second.statement_prepared = true;
    stmt_it->second.engine_statement_prepared = false;
    stmt_it->second.bound_params.clear();
    stmt_it->second.bound_param_nulls.clear();

    if (inferFirebirdStatementType(stmt_it->second.sql_text) == 5) {
        std::cerr << "[parser_debug] firebird prepare stage=ddl_fastpath_return handle="
                  << statement_handle << " info_size=0\n";
        sendResponse(state, 0, 0, nullptr, 0, ctx);
        return core::Status::OK;
    }

    std::cerr << "[parser_debug] firebird prepare stage=metadata_begin handle="
              << statement_handle << "\n";
    parser::firebird::Parser metadata_parser(stmt_it->second.sql_text);
    auto metadata_parse = metadata_parser.parseStatement();
    if (metadata_parse.success && metadata_parse.statement) {
        using namespace scratchbird::parser::v3;

        struct LocalSqldaField {
            FBSqldaVarDesc sqlda;
            FBMessageFieldDesc field;
        };

        struct LocalSourceMetadata {
            FBSqldaRelationBinding binding;
            std::vector<LocalSqldaField> columns;
        };

        auto quote_firebird_string = [](const std::string& value) -> std::string {
            std::string out;
            out.reserve(value.size() + 2);
            out.push_back('\'');
            for (char ch : value) {
                if (ch == '\'') {
                    out.push_back('\'');
                }
                out.push_back(ch);
            }
            out.push_back('\'');
            return out;
        };

        auto parse_int64_text = [](const std::optional<std::string>& value,
                                   int64_t fallback = 0) -> int64_t {
            if (!value.has_value()) {
                return fallback;
            }
            try {
                return std::stoll(trimAscii(*value));
            }
            catch (...) {
                return fallback;
            }
        };

        auto firebird_field_to_message = [](int16_t field_type,
                                            int16_t field_length,
                                            int16_t field_scale,
                                            int16_t charset_id,
                                            int16_t dimensions,
                                            bool nullable) -> FBMessageFieldDesc {
            FBMessageFieldDesc field;
            field.not_nullable = !nullable;
            field.scale = static_cast<int8_t>(field_scale);
            field.subtype = charset_id > 0 ? static_cast<uint16_t>(charset_id) : 0;
            if (dimensions > 0) {
                field.type_opcode = 9;
                field.length = 8;
                field.sql_type_override = 540;
                return field;
            }
            switch (field_type) {
                case 7:   // SMALLINT / NUMERIC(<=4)
                    field.type_opcode = 7;
                    field.length = 2;
                    field.sql_type_override = 500;
                    break;
                case 8:   // INTEGER / NUMERIC(<=9)
                    field.type_opcode = 8;
                    field.length = 4;
                    field.sql_type_override = 496;
                    break;
                case 9:   // QUAD
                    field.type_opcode = 9;
                    field.length = 8;
                    field.sql_type_override = 550;
                    break;
                case 10:  // FLOAT
                    field.type_opcode = 10;
                    field.length = 4;
                    field.sql_type_override = 482;
                    break;
                case 11:  // D_FLOAT
                    field.type_opcode = 11;
                    field.length = 8;
                    field.sql_type_override = 530;
                    break;
                case 12:  // DATE
                    field.type_opcode = 12;
                    field.length = 4;
                    field.sql_type_override = 570;
                    break;
                case 13:  // TIME
                    field.type_opcode = 13;
                    field.length = 4;
                    field.sql_type_override = 560;
                    break;
                case 14:  // CHAR
                    field.type_opcode = charset_id > 0 ? 15 : 14;
                    field.length = static_cast<uint16_t>(std::max<int16_t>(field_length, 0));
                    field.sql_type_override = 452;
                    break;
                case 16:  // BIGINT / NUMERIC / DECIMAL
                    field.type_opcode = 16;
                    field.length = 8;
                    field.sql_type_override = 580;
                    break;
                case 23:  // BOOLEAN
                    field.type_opcode = 23;
                    field.length = 1;
                    field.sql_type_override = 32764;
                    break;
                case 24:  // DECFLOAT(16)
                    field.type_opcode = 24;
                    field.length = 8;
                    field.sql_type_override = 32760;
                    break;
                case 25:  // DECFLOAT(34)
                    field.type_opcode = 25;
                    field.length = 16;
                    field.sql_type_override = 32762;
                    break;
                case 26:  // INT128
                    field.type_opcode = 26;
                    field.length = 16;
                    field.sql_type_override = 32752;
                    break;
                case 27:  // DOUBLE
                    field.type_opcode = 27;
                    field.length = 8;
                    field.sql_type_override = 480;
                    break;
                case 28:  // TIME WITH TIME ZONE
                    field.type_opcode = 28;
                    field.length = 8;
                    field.sql_type_override = 32756;
                    break;
                case 29:  // TIMESTAMP WITH TIME ZONE
                    field.type_opcode = 29;
                    field.length = 12;
                    field.sql_type_override = 32754;
                    break;
                case 35:  // TIMESTAMP
                    field.type_opcode = 35;
                    field.length = 8;
                    field.sql_type_override = 510;
                    break;
                case 37:  // VARCHAR
                    field.type_opcode = charset_id > 0 ? 38 : 37;
                    field.length = static_cast<uint16_t>(std::max<int16_t>(field_length, 0) + 2);
                    field.sql_type_override = 448;
                    break;
                case 261: // BLOB
                default:
                    field.type_opcode = 17;
                    field.length = 8;
                    field.sql_type_override = 520;
                    break;
            }
            return field;
        };

        struct FirebirdBootstrapColumnSpec {
            const char* name;
            int16_t field_type;
            int16_t field_length;
            int16_t field_scale;
            int16_t charset_id;
            int16_t dimensions;
            bool nullable;
        };

        auto append_bootstrap_relation_columns =
            [&](const FBSqldaRelationBinding& binding,
                std::initializer_list<FirebirdBootstrapColumnSpec> specs,
                std::vector<LocalSqldaField>& columns_out) {
                const std::string effective_schema =
                    !binding.relation_schema.empty() ? binding.relation_schema
                                                     : effectiveFirebirdSchemaRoot(state);
                const std::string effective_relation =
                    !binding.relation_name.empty() ? binding.relation_name
                                                   : binding.relation_alias;
                const std::string effective_alias =
                    !binding.relation_alias.empty() ? binding.relation_alias
                                                    : effective_relation;
                const std::string effective_owner =
                    !state.username.empty() ? state.username : "SYSDBA";

                columns_out.clear();
                columns_out.reserve(specs.size());
                for (const auto& spec : specs) {
                    LocalSqldaField column;
                    column.sqlda.field_name = spec.name;
                    column.sqlda.alias_name = spec.name;
                    column.sqlda.relation_name = effective_relation;
                    column.sqlda.relation_schema = effective_schema;
                    column.sqlda.relation_alias = effective_alias;
                    column.sqlda.owner_name = effective_owner;
                    column.field = firebird_field_to_message(spec.field_type,
                                                             spec.field_length,
                                                             spec.field_scale,
                                                             spec.charset_id,
                                                             spec.dimensions,
                                                             spec.nullable);
                    columns_out.push_back(std::move(column));
                }
            };

        auto load_bootstrap_relation_metadata =
            [&](const FBSqldaRelationBinding& binding,
                const std::string& relation_upper,
                std::vector<LocalSqldaField>& columns_out) -> bool {
                if (relation_upper == "RDB$DATABASE") {
                    append_bootstrap_relation_columns(
                        binding,
                        {{"DUMMY", 8, 4, 0, 0, 0, false}},
                        columns_out);
                    return true;
                }

                if (relation_upper == "RDB$RELATIONS") {
                    append_bootstrap_relation_columns(
                        binding,
                        {
                            {"RDB$RELATION_NAME", 37, 255, 0, 4, 0, false},
                            {"RDB$RELATION_ID", 8, 4, 0, 0, 0, true},
                            {"RDB$SYSTEM_FLAG", 7, 2, 0, 0, 0, true},
                            {"RDB$OWNER_NAME", 37, 255, 0, 4, 0, true},
                            {"RDB$DESCRIPTION", 261, 8, 0, 0, 0, true},
                            {"RDB$VIEW_BLR", 261, 8, 0, 0, 0, true},
                            {"RDB$VIEW_SOURCE", 37, 32765, 0, 4, 0, true},
                            {"RDB$RELATION_COUNTS", 16, 8, 0, 0, 0, true},
                            {"RDB$FORMAT", 7, 2, 0, 0, 0, true},
                            {"RDB$FIELD_ID", 7, 2, 0, 0, 0, true},
                            {"RDB$FLAGS", 7, 2, 0, 0, 0, true},
                            {"RDB$RELATION_TYPE", 7, 2, 0, 0, 0, true},
                            {"RDB$EXTERNAL_FILE", 37, 255, 0, 4, 0, true},
                            {"RDB$EXTERNAL_DESCRIPTION", 261, 8, 0, 0, 0, true},
                            {"RDB$SECURITY_CLASS", 37, 255, 0, 4, 0, true},
                        },
                        columns_out);
                    return true;
                }

                if (relation_upper == "RDB$FIELDS") {
                    append_bootstrap_relation_columns(
                        binding,
                        {
                            {"RDB$FIELD_NAME", 37, 255, 0, 4, 0, false},
                            {"RDB$QUERY_NAME", 37, 255, 0, 4, 0, true},
                            {"RDB$VALIDATION_BLR", 261, 8, 0, 0, 0, true},
                            {"RDB$VALIDATION_SOURCE", 37, 32765, 0, 4, 0, true},
                            {"RDB$COMPUTED_BLR", 261, 8, 0, 0, 0, true},
                            {"RDB$COMPUTED_SOURCE", 37, 32765, 0, 4, 0, true},
                            {"RDB$DEFAULT_VALUE", 37, 32765, 0, 4, 0, true},
                            {"RDB$DEFAULT_SOURCE", 37, 32765, 0, 4, 0, true},
                            {"RDB$FIELD_LENGTH", 8, 4, 0, 0, 0, true},
                            {"RDB$FIELD_SCALE", 8, 4, 0, 0, 0, true},
                            {"RDB$FIELD_TYPE", 7, 2, 0, 0, 0, true},
                            {"RDB$FIELD_SUB_TYPE", 7, 2, 0, 0, 0, true},
                            {"RDB$MISSING_VALUE", 37, 32765, 0, 4, 0, true},
                            {"RDB$MISSING_SOURCE", 37, 32765, 0, 4, 0, true},
                            {"RDB$DESCRIPTION", 37, 32765, 0, 4, 0, true},
                            {"RDB$SYSTEM_FLAG", 7, 2, 0, 0, 0, true},
                            {"RDB$QUERY_HEADER", 37, 32765, 0, 4, 0, true},
                            {"RDB$SEGMENT_LENGTH", 8, 4, 0, 0, 0, true},
                            {"RDB$EDIT_STRING", 37, 255, 0, 4, 0, true},
                            {"RDB$EXTERNAL_LENGTH", 8, 4, 0, 0, 0, true},
                            {"RDB$EXTERNAL_SCALE", 8, 4, 0, 0, 0, true},
                            {"RDB$EXTERNAL_TYPE", 8, 4, 0, 0, 0, true},
                            {"RDB$DIMENSIONS", 8, 4, 0, 0, 0, true},
                            {"RDB$NULL_FLAG", 7, 2, 0, 0, 0, true},
                            {"RDB$CHARACTER_LENGTH", 8, 4, 0, 0, 0, true},
                            {"RDB$COLLATION_ID", 8, 4, 0, 0, 0, true},
                            {"RDB$CHARACTER_SET_ID", 8, 4, 0, 0, 0, true},
                            {"RDB$FIELD_PRECISION", 8, 4, 0, 0, 0, true},
                            {"RDB$SECURITY_CLASS", 37, 255, 0, 4, 0, true},
                            {"RDB$OWNER_NAME", 37, 255, 0, 4, 0, true},
                        },
                        columns_out);
                    return true;
                }

                if (relation_upper == "RDB$RELATION_FIELDS") {
                    append_bootstrap_relation_columns(
                        binding,
                        {
                            {"RDB$RELATION_NAME", 37, 255, 0, 4, 0, false},
                            {"RDB$FIELD_NAME", 37, 255, 0, 4, 0, false},
                            {"RDB$FIELD_SOURCE", 37, 255, 0, 4, 0, false},
                            {"RDB$QUERY_NAME", 37, 255, 0, 4, 0, true},
                            {"RDB$BASE_FIELD", 37, 255, 0, 4, 0, true},
                            {"RDB$EDIT_STRING", 37, 255, 0, 4, 0, true},
                            {"RDB$FIELD_POSITION", 8, 4, 0, 0, 0, true},
                            {"RDB$QUERY_HEADER", 37, 32765, 0, 4, 0, true},
                            {"RDB$UPDATE_FLAG", 7, 2, 0, 0, 0, true},
                            {"RDB$FIELD_ID", 8, 4, 0, 0, 0, true},
                            {"RDB$VIEW_CONTEXT", 8, 4, 0, 0, 0, true},
                            {"RDB$DESCRIPTION", 37, 32765, 0, 4, 0, true},
                            {"RDB$DEFAULT_VALUE", 37, 32765, 0, 4, 0, true},
                            {"RDB$DEFAULT_SOURCE", 37, 32765, 0, 4, 0, true},
                            {"RDB$SYSTEM_FLAG", 7, 2, 0, 0, 0, true},
                            {"RDB$SECURITY_CLASS", 37, 255, 0, 4, 0, true},
                            {"RDB$COMPLEX_NAME", 37, 255, 0, 4, 0, true},
                            {"RDB$NULL_FLAG", 7, 2, 0, 0, 0, true},
                            {"RDB$COLLATION_ID", 8, 4, 0, 0, 0, true},
                            {"RDB$GENERATOR_NAME", 37, 255, 0, 4, 0, true},
                            {"RDB$IDENTITY_TYPE", 7, 2, 0, 0, 0, true},
                        },
                        columns_out);
                    return true;
                }

                return false;
            };

        auto infer_field_from_typename =
            [&](const TypeName& type_name) -> std::optional<FBMessageFieldDesc> {
                const std::string type_name_upper = upperAscii(poolString(metadata_parser.stringPool(),
                                                                          type_name.name));
                FBMessageFieldDesc field;
                if (type_name_upper == "SMALLINT" || type_name_upper == "SHORT") {
                    field.type_opcode = 7;
                    field.length = 2;
                    field.sql_type_override = 500;
                    return field;
                }
                if (type_name_upper == "INTEGER" || type_name_upper == "INT" ||
                    type_name_upper == "LONG") {
                    field.type_opcode = 8;
                    field.length = 4;
                    field.sql_type_override = 496;
                    return field;
                }
                if (type_name_upper == "BIGINT") {
                    field.type_opcode = 16;
                    field.length = 8;
                    field.sql_type_override = 580;
                    return field;
                }
                if (type_name_upper == "INT128") {
                    field.type_opcode = 26;
                    field.length = 16;
                    field.sql_type_override = 32752;
                    return field;
                }
                if (type_name_upper == "FLOAT") {
                    field.type_opcode = 10;
                    field.length = 4;
                    field.sql_type_override = 482;
                    return field;
                }
                if (type_name_upper == "DOUBLE" || type_name_upper == "DOUBLE PRECISION") {
                    field.type_opcode = 27;
                    field.length = 8;
                    field.sql_type_override = 480;
                    return field;
                }
                if (type_name_upper == "DECFLOAT" &&
                    type_name.precision.has_value() &&
                    type_name.precision.value() > 16) {
                    field.type_opcode = 25;
                    field.length = 16;
                    field.sql_type_override = 32762;
                    return field;
                }
                if (type_name_upper == "DECFLOAT") {
                    field.type_opcode = 24;
                    field.length = 8;
                    field.sql_type_override = 32760;
                    return field;
                }
                if (type_name_upper == "NUMERIC" || type_name_upper == "DECIMAL") {
                    const int32_t precision = type_name.precision.value_or(18);
                    field.scale = static_cast<int8_t>(-std::abs(type_name.scale.value_or(0)));
                    if (precision <= 4) {
                        field.type_opcode = 7;
                        field.length = 2;
                        field.sql_type_override = 500;
                    }
                    else if (precision <= 9) {
                        field.type_opcode = 8;
                        field.length = 4;
                        field.sql_type_override = 496;
                    }
                    else if (precision <= 18) {
                        field.type_opcode = 16;
                        field.length = 8;
                        field.sql_type_override = 580;
                    }
                    else {
                        field.type_opcode = 26;
                        field.length = 16;
                        field.sql_type_override = 32752;
                    }
                    return field;
                }
                if (type_name_upper == "BOOLEAN") {
                    field.type_opcode = 23;
                    field.length = 1;
                    field.sql_type_override = 32764;
                    return field;
                }
                if (type_name_upper == "DATE") {
                    field.type_opcode = 12;
                    field.length = 4;
                    field.sql_type_override = 570;
                    return field;
                }
                if (type_name_upper == "TIME" && type_name.with_time_zone) {
                    field.type_opcode = 28;
                    field.length = 8;
                    field.sql_type_override = 32756;
                    return field;
                }
                if (type_name_upper == "TIME") {
                    field.type_opcode = 13;
                    field.length = 4;
                    field.sql_type_override = 560;
                    return field;
                }
                if (type_name_upper == "TIMESTAMP" && type_name.with_time_zone) {
                    field.type_opcode = 29;
                    field.length = 12;
                    field.sql_type_override = 32754;
                    return field;
                }
                if (type_name_upper == "TIMESTAMP") {
                    field.type_opcode = 35;
                    field.length = 8;
                    field.sql_type_override = 510;
                    return field;
                }
                if (type_name_upper == "CHAR" || type_name_upper == "CHARACTER") {
                    field.type_opcode = 14;
                    field.length =
                        static_cast<uint16_t>(std::max<int32_t>(type_name.length.value_or(1), 1));
                    field.sql_type_override = 452;
                    return field;
                }
                if (type_name_upper == "VARCHAR" || type_name_upper == "CHARACTER VARYING") {
                    field.type_opcode = 37;
                    field.length = static_cast<uint16_t>(
                        std::max<int32_t>(type_name.length.value_or(1), 1) + 2);
                    field.sql_type_override = 448;
                    return field;
                }
                if (type_name_upper == "BLOB") {
                    field.type_opcode = 17;
                    field.length = 8;
                    field.sql_type_override = 520;
                    return field;
                }
                return std::nullopt;
            };

        auto infer_field_from_literal =
            [&](const Expression* expr) -> std::optional<LocalSqldaField> {
                if (expr == nullptr) {
                    return std::nullopt;
                }

                LocalSqldaField out;
                switch (expr->kind()) {
                    case ASTKind::LiteralExpr:
                    {
                        const auto* literal = static_cast<const LiteralExpr*>(expr);
                        switch (literal->literal_type) {
                            case LiteralType::INTEGER:
                                out.field.type_opcode = 16;
                                out.field.length = 8;
                                out.field.sql_type_override = 580;
                                return out;
                            case LiteralType::FLOAT:
                                out.field.type_opcode = 27;
                                out.field.length = 8;
                                out.field.sql_type_override = 480;
                                return out;
                            case LiteralType::STRING:
                            case LiteralType::BLOB:
                                out.field.type_opcode = 37;
                                out.field.length = 257;
                                out.field.sql_type_override = 448;
                                return out;
                            case LiteralType::BOOLEAN:
                                out.field.type_opcode = 23;
                                out.field.length = 1;
                                out.field.sql_type_override = 32764;
                                return out;
                            case LiteralType::NULL_VALUE:
                            case LiteralType::DEFAULT:
                                out.field.sql_type_override = 32766;
                                return out;
                        }
                        break;
                    }
                    case ASTKind::LiteralInt8Expr:
                    case ASTKind::LiteralInt16Expr:
                        out.field.type_opcode = 7;
                        out.field.length = 2;
                        out.field.sql_type_override = 500;
                        return out;
                    case ASTKind::LiteralYearExpr:
                    case ASTKind::LiteralMediumIntExpr:
                    case ASTKind::LiteralUInt8Expr:
                    case ASTKind::LiteralUInt16Expr:
                    case ASTKind::LiteralUInt32Expr:
                        out.field.type_opcode = 8;
                        out.field.length = 4;
                        out.field.sql_type_override = 496;
                        return out;
                    case ASTKind::LiteralEnumExpr:
                    case ASTKind::LiteralSetExpr:
                    case ASTKind::LiteralBitExpr:
                    case ASTKind::LiteralJsonPathExpr:
                    case ASTKind::LiteralTsVectorExpr:
                    case ASTKind::LiteralTsQueryExpr:
                        out.field.type_opcode = 37;
                        out.field.length = 8194;
                        out.field.sql_type_override = 448;
                        return out;
                    case ASTKind::LiteralRowExpr:
                    case ASTKind::LiteralCompositeExpr:
                    case ASTKind::LiteralGeometryExpr:
                    case ASTKind::LiteralRangeExpr:
                    case ASTKind::LiteralVariantExpr:
                        out.field.type_opcode = 17;
                        out.field.length = 8;
                        out.field.sql_type_override = 520;
                        return out;
                    case ASTKind::LiteralArrayExpr:
                        out.field.type_opcode = 9;
                        out.field.length = 8;
                        out.field.sql_type_override = 540;
                        return out;
                    case ASTKind::LiteralBlobLocatorExpr:
                    {
                        const auto* literal =
                            static_cast<const LiteralBlobLocatorExpr*>(expr);
                        out.field.type_opcode = 17;
                        out.field.length = 8;
                        out.field.subtype = static_cast<uint16_t>(
                            std::max<int16_t>(literal->blob_subtype, 0));
                        out.field.sql_type_override = 520;
                        return out;
                    }
                    case ASTKind::LiteralUInt64Expr:
                        out.field.type_opcode = 16;
                        out.field.length = 8;
                        out.field.sql_type_override = 580;
                        return out;
                    case ASTKind::LiteralInt128Expr:
                    case ASTKind::LiteralUInt128Expr:
                        out.field.type_opcode = 26;
                        out.field.length = 16;
                        out.field.sql_type_override = 32752;
                        return out;
                    case ASTKind::LiteralFloat32Expr:
                        out.field.type_opcode = 10;
                        out.field.length = 4;
                        out.field.sql_type_override = 482;
                        return out;
                    case ASTKind::LiteralDateTimeExpr:
                    {
                        const auto* literal = static_cast<const LiteralDateTimeExpr*>(expr);
                        if (literal->with_timezone) {
                            out.field.type_opcode = 29;
                            out.field.length = 12;
                            out.field.sql_type_override = 32754;
                        }
                        else {
                            out.field.type_opcode = 35;
                            out.field.length = 8;
                            out.field.sql_type_override = 510;
                        }
                        return out;
                    }
                    case ASTKind::LiteralTimeTzExpr:
                        out.field.type_opcode = 28;
                        out.field.length = 8;
                        out.field.sql_type_override = 32756;
                        return out;
                    case ASTKind::LiteralTimestampTzExpr:
                        out.field.type_opcode = 29;
                        out.field.length = 12;
                        out.field.sql_type_override = 32754;
                        return out;
                    default:
                        break;
                }

                return std::nullopt;
            };

        auto make_sqlda_field = [](uint8_t type_opcode,
                                   uint16_t length,
                                   uint16_t sql_type_override) -> LocalSqldaField {
            LocalSqldaField out;
            out.field.type_opcode = type_opcode;
            out.field.length = length;
            out.field.sql_type_override = sql_type_override;
            return out;
        };

        std::function<std::optional<LocalSqldaField>(
            const FunctionCallExpr*,
            const std::function<std::optional<LocalSqldaField>(const Expression*)>&)>
            infer_builtin_function_field;
        infer_builtin_function_field =
            [&](const FunctionCallExpr* fn,
                const std::function<std::optional<LocalSqldaField>(const Expression*)>&
                    infer_arg) -> std::optional<LocalSqldaField> {
                if (fn == nullptr) {
                    return std::nullopt;
                }

                const std::string fn_name =
                    upperAscii(schemaPathObjectName(fn->function_path,
                                                    metadata_parser.stringPool()));

                auto first_typed_argument = [&]() -> std::optional<LocalSqldaField> {
                    for (const auto* arg : fn->arguments) {
                        if (auto inferred = infer_arg(arg); inferred.has_value()) {
                            return inferred;
                        }
                    }
                    return std::nullopt;
                };

                if (fn_name == "COUNT" || fn_name == "ARRAY_POSITION" ||
                    fn_name == "ROW_NUMBER" || fn_name == "RANK" ||
                    fn_name == "DENSE_RANK" || fn_name == "REGR_COUNT" ||
                    fn_name == "CURRENT_CONNECTION" ||
                    fn_name == "CURRENT_SESSION" ||
                    fn_name == "CURRENT_TRANSACTION" ||
                    fn_name == "DATE_DIFF" || fn_name == "DATEDIFF") {
                    return make_sqlda_field(16, 8, 580);
                }

                if (fn_name == "CHAR_LENGTH" || fn_name == "CHARACTER_LENGTH" ||
                    fn_name == "LENGTH" || fn_name == "LEN" ||
                    fn_name == "OCTET_LENGTH" || fn_name == "SIGN") {
                    return make_sqlda_field(8, 4, 496);
                }

                if (fn_name == "AVG" || fn_name == "STDDEV" ||
                    fn_name == "STDDEV_SAMP" || fn_name == "STDDEV_POP" ||
                    fn_name == "VARIANCE" || fn_name == "VAR_SAMP" ||
                    fn_name == "VAR_POP" || fn_name == "CORR" ||
                    fn_name == "COVAR_POP" || fn_name == "REGR_SLOPE" ||
                    fn_name == "REGR_INTERCEPT" || fn_name == "REGR_R2" ||
                    fn_name == "REGR_AVGX" || fn_name == "REGR_AVGY" ||
                    fn_name == "REGR_SXX" || fn_name == "REGR_SYY" ||
                    fn_name == "REGR_SXY" || fn_name == "TS_RANK" ||
                    fn_name == "POWER" || fn_name == "ACOS" ||
                    fn_name == "ACOSH" || fn_name == "AGE" ||
                    fn_name == "ASIN" || fn_name == "ASINH" ||
                    fn_name == "ATAN" || fn_name == "ATAN2" ||
                    fn_name == "ATANH" || fn_name == "CBRT" ||
                    fn_name == "COS" || fn_name == "COSH" ||
                    fn_name == "COT" || fn_name == "DEGREES" ||
                    fn_name == "EXP" || fn_name == "LN" ||
                    fn_name == "LOG" || fn_name == "LOG10" ||
                    fn_name == "LOG2" || fn_name == "PI" ||
                    fn_name == "RADIANS" || fn_name == "SIN" ||
                    fn_name == "SINH" || fn_name == "SQRT" ||
                    fn_name == "TAN" || fn_name == "TANH") {
                    return make_sqlda_field(27, 8, 480);
                }

                if (fn_name == "CURRENT_DATE" || fn_name == "TO_DATE") {
                    return make_sqlda_field(12, 4, 570);
                }

                if (fn_name == "CURRENT_TIME") {
                    return make_sqlda_field(13, 4, 560);
                }

                if (fn_name == "NOW" || fn_name == "CURRENT_TIMESTAMP" ||
                    fn_name == "TO_TIMESTAMP") {
                    return make_sqlda_field(35, 8, 510);
                }

                if (fn_name == "CURRENT_SCHEMA" || fn_name == "SCHEMA_PATH" ||
                    fn_name == "CURRENT_USER" || fn_name == "SESSION_USER" ||
                    fn_name == "CURRENT_ROLE" || fn_name == "CONCAT" ||
                    fn_name == "CONCAT_WS" || fn_name == "LOWER" ||
                    fn_name == "UPPER" || fn_name == "TRIM" ||
                    fn_name == "LTRIM" || fn_name == "RTRIM" ||
                    fn_name == "SUBSTRING" || fn_name == "SUBSTR" ||
                    fn_name == "REPLACE" || fn_name == "TO_CHAR" ||
                    fn_name == "COLLATE" || fn_name == "FORMAT_TYPE" ||
                    fn_name == "COL_DESCRIPTION" ||
                    fn_name == "OBJ_DESCRIPTION" ||
                    fn_name == "SHOBJ_DESCRIPTION" ||
                    fn_name == "XMLAGG" ||
                    fn_name == "TO_TSVECTOR" ||
                    fn_name == "PLAINTO_TSQUERY" ||
                    fn_name == "TO_TSQUERY" ||
                    fn_name == "JSON_EXTRACT" || fn_name == "JSON_OBJECT" ||
                    fn_name == "JSON_ARRAY" || fn_name == "JSON_SET" ||
                    fn_name == "JSON_INSERT" || fn_name == "JSON_REMOVE") {
                    return make_sqlda_field(37, 8194, 448);
                }

                if (fn_name == "ENDS_WITH" || fn_name == "JSON_EXISTS" ||
                    fn_name == "TSMATCH" ||
                    fn_name == "JSON_HAS_KEY") {
                    return make_sqlda_field(23, 1, 32764);
                }

                if (fn_name == "ARRAY_SLICE" || fn_name == "ARRAY_AGG") {
                    return make_sqlda_field(9, 8, 540);
                }

                if (fn_name == "SUM" || fn_name == "MIN" || fn_name == "MAX" ||
                    fn_name == "LEAST" || fn_name == "GREATEST" ||
                    fn_name == "COALESCE" || fn_name == "NULLIF" ||
                    fn_name == "ABS" || fn_name == "FLOOR" ||
                    fn_name == "CEIL" || fn_name == "CEILING" ||
                    fn_name == "ROUND" || fn_name == "TRUNC" ||
                    fn_name == "MOD" || fn_name == "CONVERT" ||
                    fn_name == "LAG" || fn_name == "LEAD" ||
                    fn_name == "FIRST_VALUE" || fn_name == "LAST_VALUE" ||
                    fn_name == "NTH_VALUE" ||
                    fn_name == "DATE_ADD" || fn_name == "DATEADD" ||
                    fn_name == "DATE_SUB" || fn_name == "DATESUB") {
                    return first_typed_argument();
                }

                return std::nullopt;
            };

        std::unordered_map<std::string, LocalSqldaField> parameter_type_hints;
        auto parameter_expr_key = [&](const ParameterExpr* parameter) -> std::string {
            if (parameter == nullptr) {
                return {};
            }
            if (parameter->is_named && parameter->name != StringPool::INVALID_ID) {
                return "N:" +
                       poolString(metadata_parser.stringPool(), parameter->name);
            }
            return "P:" + std::to_string(parameter->index);
        };

        const std::string metadata_schema_root = effectiveFirebirdSchemaRoot(state);

        std::function<bool(const Statement*,
                           const std::vector<const CTE*>&,
                           std::vector<LocalSqldaField>&,
                           std::string&)> synthesize_statement_output_columns;

        auto run_internal_firebird_query =
            [&](const std::string& sql_text,
                std::vector<std::string>& column_names_out,
                std::deque<std::vector<std::optional<std::string>>>& rows_out,
                std::string& error_out) -> bool {
                core::ErrorContext query_ctx;
                auto status = executeCompiledInternalQuery(state,
                                                           sql_text,
                                                           &column_names_out,
                                                           &rows_out,
                                                           &query_ctx);
                if (status != core::Status::OK) {
                    error_out = query_ctx.message.empty()
                                    ? "Failed to execute Firebird metadata query"
                                    : query_ctx.message;
                    std::cerr << "[parser_debug] firebird metadata query failed sql="
                              << sql_text << " message=" << error_out << "\n";
                    return false;
                }
                return true;
            };

        auto load_relation_metadata =
            [&](const FBSqldaRelationBinding& binding,
                std::vector<LocalSqldaField>& columns_out,
                std::string& error_out) -> bool {
                columns_out.clear();
                if (binding.relation_name.empty()) {
                    return true;
                }

                const std::string effective_schema =
                    !binding.relation_schema.empty() ? binding.relation_schema
                                                     : metadata_schema_root;
                const std::string relation_upper = upperAscii(binding.relation_name);
                const bool system_relation =
                    relation_upper.rfind("RDB$", 0) == 0 ||
                    relation_upper.rfind("MON$", 0) == 0 ||
                    relation_upper.rfind("SEC$", 0) == 0;
                if (load_bootstrap_relation_metadata(binding, relation_upper, columns_out)) {
                    return true;
                }
                const std::string metadata_sql =
                    std::string(
                    "SELECT rf.RDB$FIELD_NAME, rf.RDB$FIELD_POSITION, rf.RDB$NULL_FLAG, "
                    "f.RDB$FIELD_TYPE, f.RDB$FIELD_LENGTH, f.RDB$FIELD_SCALE, "
                    "f.RDB$CHARACTER_SET_ID, f.RDB$COLLATION_ID, f.RDB$DIMENSIONS, "
                    "r.RDB$OWNER_NAME "
                    "FROM RDB$RELATION_FIELDS rf "
                    "JOIN RDB$FIELDS") +
                    " f ON f.RDB$FIELD_NAME = rf.RDB$FIELD_SOURCE "
                    "JOIN RDB$RELATIONS" +
                    " r ON r.RDB$RELATION_NAME = rf.RDB$RELATION_NAME "
                    "WHERE rf.RDB$RELATION_NAME = " +
                    quote_firebird_string(relation_upper) +
                    " ORDER BY rf.RDB$FIELD_POSITION";

                std::vector<std::string> column_names;
                std::deque<std::vector<std::optional<std::string>>> rows;
                if (!run_internal_firebird_query(metadata_sql, column_names, rows, error_out)) {
                    std::string lowered_error = error_out;
                    std::transform(lowered_error.begin(),
                                   lowered_error.end(),
                                   lowered_error.begin(),
                                   [](unsigned char ch) {
                                       return static_cast<char>(std::tolower(ch));
                                   });
                    if (system_relation &&
                        lowered_error.find("permission denied") != std::string::npos) {
                        error_out.clear();
                        return true;
                    }
                    return false;
                }

                std::cerr << "[parser_debug] firebird relation metadata relation="
                          << relation_upper << " rows=" << rows.size() << "\n";

                if (rows.empty()) {
                    const std::string view_sql =
                        "SELECT RDB$VIEW_SOURCE, RDB$OWNER_NAME "
                        "FROM RDB$RELATIONS "
                        "WHERE RDB$RELATION_NAME = " +
                        quote_firebird_string(relation_upper) +
                        " AND RDB$VIEW_SOURCE IS NOT NULL";
                    std::vector<std::string> view_column_names;
                    std::deque<std::vector<std::optional<std::string>>> view_rows;
                    std::string view_error;
                    if (run_internal_firebird_query(view_sql,
                                                    view_column_names,
                                                    view_rows,
                                                    view_error) &&
                        !view_rows.empty() &&
                        !view_rows.front().empty() &&
                        view_rows.front()[0].has_value()) {
                        parser::firebird::Parser view_parser(trimAscii(*view_rows.front()[0]));
                        auto view_parse = view_parser.parseStatement();
                        if (view_parse.success && view_parse.statement) {
                            std::vector<const CTE*> empty_scope;
                            if (synthesize_statement_output_columns(view_parse.statement.get(),
                                                                    empty_scope,
                                                                    columns_out,
                                                                    error_out) &&
                                !columns_out.empty()) {
                                const std::string owner_name =
                                    view_rows.front().size() > 1
                                        ? view_rows.front()[1].value_or("SYSDBA")
                                        : std::string("SYSDBA");
                                for (auto& column : columns_out) {
                                    if (column.sqlda.relation_name.empty()) {
                                        column.sqlda.relation_name = binding.relation_name;
                                    }
                                    if (column.sqlda.relation_schema.empty()) {
                                        column.sqlda.relation_schema = effective_schema;
                                    }
                                    if (column.sqlda.relation_alias.empty()) {
                                        column.sqlda.relation_alias = binding.relation_alias;
                                    }
                                    if (column.sqlda.owner_name.empty()) {
                                        column.sqlda.owner_name = owner_name;
                                    }
                                }
                                return true;
                            }
                        }
                    }
                }

                for (const auto& row : rows) {
                    if (row.size() < 10) {
                        continue;
                    }

                    LocalSqldaField desc;
                    desc.sqlda.field_name =
                        row[0].has_value() ? trimAscii(*row[0]) : std::string();
                    desc.sqlda.alias_name = desc.sqlda.field_name;
                    desc.sqlda.relation_name = binding.relation_name;
                    desc.sqlda.relation_schema = effective_schema;
                    desc.sqlda.relation_alias = binding.relation_alias;
                    desc.sqlda.owner_name =
                        row[9].has_value() ? trimAscii(*row[9]) : std::string("SYSDBA");

                    const int16_t field_type =
                        static_cast<int16_t>(parse_int64_text(row[3], 0));
                    const int16_t field_length =
                        static_cast<int16_t>(parse_int64_text(row[4], 0));
                    const int16_t field_scale =
                        static_cast<int16_t>(parse_int64_text(row[5], 0));
                    const int16_t charset_id =
                        static_cast<int16_t>(parse_int64_text(row[6], 0));
                    const int16_t dimensions =
                        static_cast<int16_t>(parse_int64_text(row[8], 0));
                    const bool nullable = !row[2].has_value();
                    desc.field = firebird_field_to_message(field_type,
                                                           field_length,
                                                           field_scale,
                                                           charset_id,
                                                           dimensions,
                                                           nullable);
                    columns_out.push_back(std::move(desc));
                }

                return true;
            };

        auto statement_with_clause = [](const Statement* statement) -> const WithClause* {
            if (statement == nullptr) {
                return nullptr;
            }

            switch (statement->kind()) {
                case ASTKind::SelectStmt:
                    return static_cast<const SelectStmt*>(statement)->with;
                case ASTKind::InsertStmt:
                    return static_cast<const InsertStmt*>(statement)->with;
                case ASTKind::UpdateStmt:
                    return static_cast<const UpdateStmt*>(statement)->with;
                case ASTKind::DeleteStmt:
                    return static_cast<const DeleteStmt*>(statement)->with;
                default:
                    return nullptr;
            }
        };

        auto append_sqlda_name =
            [](LocalSqldaField& field_desc, const std::string& name, size_t ordinal) {
                if (!name.empty()) {
                    field_desc.sqlda.field_name = name;
                    field_desc.sqlda.alias_name = name;
                    return;
                }

                const std::string generated = "EXPR$" + std::to_string(ordinal);
                field_desc.sqlda.field_name = generated;
                field_desc.sqlda.alias_name = generated;
            };

        auto apply_column_alias_overrides =
            [&](const std::vector<StringPool::StringId>& aliases,
                std::vector<LocalSqldaField>& columns) {
                const size_t limit = std::min(aliases.size(), columns.size());
                for (size_t i = 0; i < limit; ++i) {
                    const std::string alias_name =
                        poolString(metadata_parser.stringPool(), aliases[i]);
                    if (alias_name.empty()) {
                        continue;
                    }
                    columns[i].sqlda.field_name = alias_name;
                    columns[i].sqlda.alias_name = alias_name;
                }
            };

        auto finalize_source_metadata =
            [&](LocalSourceMetadata& meta) {
                const std::string effective_schema =
                    !meta.binding.relation_schema.empty() ? meta.binding.relation_schema
                                                          : metadata_schema_root;
                const std::string effective_relation =
                    !meta.binding.relation_name.empty() ? meta.binding.relation_name
                                                        : meta.binding.relation_alias;
                const std::string effective_alias =
                    !meta.binding.relation_alias.empty() ? meta.binding.relation_alias
                                                         : effective_relation;
                const std::string effective_owner =
                    !state.username.empty() ? state.username : "SYSDBA";

                for (size_t i = 0; i < meta.columns.size(); ++i) {
                    auto& column = meta.columns[i];
                    if (column.sqlda.field_name.empty()) {
                        append_sqlda_name(column, std::string{}, i + 1);
                    }
                    if (column.sqlda.alias_name.empty()) {
                        column.sqlda.alias_name = column.sqlda.field_name;
                    }
                    if (column.sqlda.relation_name.empty()) {
                        column.sqlda.relation_name = effective_relation;
                    }
                    if (column.sqlda.relation_schema.empty()) {
                        column.sqlda.relation_schema = effective_schema;
                    }
                    if (!effective_alias.empty()) {
                        column.sqlda.relation_alias = effective_alias;
                    }
                    if (column.sqlda.owner_name.empty()) {
                        column.sqlda.owner_name = effective_owner;
                    }
                }
            };

        auto is_placeholder_field = [](const FBMessageFieldDesc& field) -> bool {
            return field.sql_type_override == 0 || field.sql_type_override == 32766;
        };

        auto merge_local_sqlda_columns =
            [&](std::vector<LocalSqldaField>& primary,
                const std::vector<LocalSqldaField>& fallback) {
                const size_t overlap = std::min(primary.size(), fallback.size());
                for (size_t i = 0; i < overlap; ++i) {
                    if (is_placeholder_field(primary[i].field) &&
                        !is_placeholder_field(fallback[i].field)) {
                        primary[i].field = fallback[i].field;
                    }
                    if (primary[i].sqlda.field_name.empty()) {
                        primary[i].sqlda.field_name = fallback[i].sqlda.field_name;
                    }
                    if (primary[i].sqlda.alias_name.empty()) {
                        primary[i].sqlda.alias_name = fallback[i].sqlda.alias_name;
                    }
                    if (primary[i].sqlda.relation_name.empty()) {
                        primary[i].sqlda.relation_name = fallback[i].sqlda.relation_name;
                    }
                    if (primary[i].sqlda.relation_schema.empty()) {
                        primary[i].sqlda.relation_schema = fallback[i].sqlda.relation_schema;
                    }
                    if (primary[i].sqlda.relation_alias.empty()) {
                        primary[i].sqlda.relation_alias = fallback[i].sqlda.relation_alias;
                    }
                    if (primary[i].sqlda.owner_name.empty()) {
                        primary[i].sqlda.owner_name = fallback[i].sqlda.owner_name;
                    }
                }
                for (size_t i = primary.size(); i < fallback.size(); ++i) {
                    primary.push_back(fallback[i]);
                }
            };

        std::function<void(const Statement*, std::vector<LocalSqldaField>&)>
            append_recursive_placeholder_columns;
        append_recursive_placeholder_columns =
            [&](const Statement* statement, std::vector<LocalSqldaField>& columns_out) {
                if (statement == nullptr) {
                    return;
                }

                auto append_placeholder_name =
                    [&](const SelectItem* item) {
                        if (item == nullptr) {
                            return;
                        }
                        LocalSqldaField column;
                        if (item->item_type == SelectItem::Type::EXPRESSION &&
                            item->expr != nullptr) {
                            if (auto inferred = infer_field_from_literal(item->expr);
                                inferred.has_value()) {
                                column = *inferred;
                            }
                            else if (item->expr->kind() == ASTKind::CastExpr) {
                                const auto* cast_expr =
                                    static_cast<const CastExpr*>(item->expr);
                                if (auto field =
                                        infer_field_from_typename(cast_expr->target_type);
                                    field.has_value()) {
                                    column.field = *field;
                                }
                            }
                            else if (item->expr->kind() == ASTKind::ColumnRefExpr) {
                                const auto* column_expr =
                                    static_cast<const ColumnRefExpr*>(item->expr);
                                FBSqldaRelationBinding binding;
                                binding.relation_name =
                                    schemaPathObjectName(column_expr->column.table_path,
                                                         metadata_parser.stringPool());
                                binding.relation_schema =
                                    schemaPathSchemaName(column_expr->column.table_path,
                                                         metadata_parser.stringPool());

                                std::vector<LocalSqldaField> relation_columns;
                                std::string metadata_error;
                                if (!binding.relation_name.empty() &&
                                    load_relation_metadata(binding,
                                                           relation_columns,
                                                           metadata_error)) {
                                    const std::string column_name =
                                        poolString(metadata_parser.stringPool(),
                                                   column_expr->column.column_name);
                                    auto match_it =
                                        std::find_if(relation_columns.begin(),
                                                     relation_columns.end(),
                                                     [&](const LocalSqldaField& candidate) {
                                                         return identifiersEqual(
                                                             candidate.sqlda.field_name,
                                                             column_name);
                                                     });
                                    if (match_it != relation_columns.end()) {
                                        column = *match_it;
                                    }
                                }
                            }
                        }
                        if (column.field.sql_type_override == 0) {
                            column.field.sql_type_override = 32766;
                        }
                        std::string name;
                        if (item->has_alias) {
                            name = poolString(metadata_parser.stringPool(), item->alias);
                        }
                        else if (item->item_type == SelectItem::Type::EXPRESSION &&
                                 item->expr != nullptr &&
                                 item->expr->kind() == ASTKind::ColumnRefExpr) {
                            const auto* column_expr =
                                static_cast<const ColumnRefExpr*>(item->expr);
                            name = poolString(metadata_parser.stringPool(),
                                              column_expr->column.column_name);
                        }
                        append_sqlda_name(column, name, columns_out.size() + 1);
                        columns_out.push_back(std::move(column));
                    };

                switch (statement->kind()) {
                    case ASTKind::SelectStmt:
                    {
                        const auto* select = static_cast<const SelectStmt*>(statement);
                        for (const auto* item : select->items) {
                            append_placeholder_name(item);
                        }
                        if (columns_out.empty() && select->set_op_right != nullptr) {
                            append_recursive_placeholder_columns(select->set_op_right,
                                                                 columns_out);
                        }
                        return;
                    }
                    case ASTKind::InsertStmt:
                    {
                        const auto* insert = static_cast<const InsertStmt*>(statement);
                        for (const auto* item : insert->returning) {
                            append_placeholder_name(item);
                        }
                        return;
                    }
                    case ASTKind::UpdateStmt:
                    {
                        const auto* update = static_cast<const UpdateStmt*>(statement);
                        for (const auto* item : update->returning) {
                            append_placeholder_name(item);
                        }
                        return;
                    }
                    case ASTKind::DeleteStmt:
                    {
                        const auto* del = static_cast<const DeleteStmt*>(statement);
                        for (const auto* item : del->returning) {
                            append_placeholder_name(item);
                        }
                        return;
                    }
                    default:
                        return;
                }
            };

        std::function<const CTE*(std::string_view, const std::vector<const CTE*>&)> resolve_cte =
            [&](std::string_view name, const std::vector<const CTE*>& scope) -> const CTE* {
                for (auto it = scope.rbegin(); it != scope.rend(); ++it) {
                    if ((*it) != nullptr &&
                        identifiersEqual(poolString(metadata_parser.stringPool(), (*it)->name),
                                         name)) {
                        return *it;
                    }
                }
                return nullptr;
            };

        std::function<std::vector<const CTE*>(const Statement*, const std::vector<const CTE*>&)>
            extend_cte_scope =
                [&](const Statement* statement,
                    const std::vector<const CTE*>& parent_scope) -> std::vector<const CTE*> {
                    std::vector<const CTE*> scope = parent_scope;
                    if (const auto* with = statement_with_clause(statement); with != nullptr) {
                        for (const auto& cte : with->ctes) {
                            scope.push_back(&cte);
                        }
                    }
                    return scope;
                };

        std::vector<std::string> active_cte_resolutions;
        std::function<bool(const TableRefNode*,
                           const std::vector<const CTE*>&,
                           std::vector<LocalSourceMetadata>&,
                           std::string&)> append_table_ref_source_metadata;

        append_table_ref_source_metadata =
            [&](const TableRefNode* table_ref,
                const std::vector<const CTE*>& cte_scope,
                std::vector<LocalSourceMetadata>& sources_out,
                std::string& error_out) -> bool {
                if (table_ref == nullptr) {
                    return true;
                }

                LocalSourceMetadata meta;
                switch (table_ref->ref_type) {
                    case TableRefNode::Type::TABLE:
                    {
                        meta.binding.relation_name =
                            schemaPathObjectName(table_ref->table_path,
                                                 metadata_parser.stringPool());
                        meta.binding.relation_schema =
                            schemaPathSchemaName(table_ref->table_path,
                                                 metadata_parser.stringPool());
                        meta.binding.relation_alias =
                            table_ref->has_alias
                                ? poolString(metadata_parser.stringPool(), table_ref->alias)
                                : meta.binding.relation_name;

                        const CTE* cte = meta.binding.relation_schema.empty()
                                             ? resolve_cte(meta.binding.relation_name, cte_scope)
                                             : nullptr;
                        if (cte != nullptr) {
                            const std::string cte_name =
                                poolString(metadata_parser.stringPool(), cte->name);
                            auto active_it = std::find_if(active_cte_resolutions.begin(),
                                                          active_cte_resolutions.end(),
                                                          [&](const std::string& active_name) {
                                                              return identifiersEqual(active_name,
                                                                                      cte_name);
                                                          });
                            if (active_it != active_cte_resolutions.end()) {
                                bool resolved_recursive_anchor = false;
                                if (cte->query != nullptr &&
                                    cte->query->kind() == ASTKind::SelectStmt) {
                                    const auto* recursive_select =
                                        static_cast<const SelectStmt*>(cte->query);
                                    if (recursive_select->set_op_right != nullptr) {
                                        SelectStmt anchor_select = *recursive_select;
                                        anchor_select.with = nullptr;
                                        anchor_select.set_op = SetOpType::NONE;
                                        anchor_select.set_op_all = false;
                                        anchor_select.set_op_right = nullptr;
                                        std::vector<LocalSqldaField> anchor_columns;
                                        std::string anchor_error;
                                        if (synthesize_statement_output_columns(&anchor_select,
                                                                               cte_scope,
                                                                               anchor_columns,
                                                                               anchor_error) &&
                                            !anchor_columns.empty()) {
                                            meta.columns = std::move(anchor_columns);
                                            resolved_recursive_anchor = true;
                                        }
                                    }
                                }
                                if (!resolved_recursive_anchor) {
                                    append_recursive_placeholder_columns(cte->query,
                                                                         meta.columns);
                                    for (size_t i = meta.columns.size();
                                         i < cte->column_names.size();
                                         ++i) {
                                        LocalSqldaField column;
                                        column.field.sql_type_override = 32766;
                                        append_sqlda_name(column,
                                                          poolString(metadata_parser.stringPool(),
                                                                     cte->column_names[i]),
                                                          i + 1);
                                        meta.columns.push_back(std::move(column));
                                    }
                                }
                                apply_column_alias_overrides(cte->column_names, meta.columns);
                            }
                            else {
                                active_cte_resolutions.push_back(cte_name);
                                const bool ok = synthesize_statement_output_columns(cte->query,
                                                                                    cte_scope,
                                                                                    meta.columns,
                                                                                    error_out);
                                active_cte_resolutions.pop_back();
                                if (!ok) {
                                    return false;
                                }
                                apply_column_alias_overrides(cte->column_names, meta.columns);
                            }
                        }
                        else if (!load_relation_metadata(meta.binding, meta.columns, error_out)) {
                            return false;
                        }
                        break;
                    }
                    case TableRefNode::Type::SUBQUERY:
                    {
                        meta.binding.relation_name =
                            table_ref->has_alias
                                ? poolString(metadata_parser.stringPool(), table_ref->alias)
                                : "__DERIVED__";
                        meta.binding.relation_alias = meta.binding.relation_name;
                        if (!synthesize_statement_output_columns(table_ref->subquery,
                                                                 cte_scope,
                                                                 meta.columns,
                                                                 error_out)) {
                            return false;
                        }
                        apply_column_alias_overrides(table_ref->column_aliases, meta.columns);
                        break;
                    }
                    case TableRefNode::Type::FUNCTION:
                    {
                        const std::string function_name =
                            table_ref->function != nullptr
                                ? schemaPathObjectName(table_ref->function->function_path,
                                                       metadata_parser.stringPool())
                                : "FUNCTION";
                        meta.binding.relation_name = function_name;
                        meta.binding.relation_schema = metadata_schema_root;
                        meta.binding.relation_alias =
                            table_ref->has_alias
                                ? poolString(metadata_parser.stringPool(), table_ref->alias)
                                : function_name;
                        if (table_ref->function != nullptr) {
                            const std::string function_name_upper = upperAscii(function_name);
                            const std::string metadata_sql =
                                std::string(
                                "SELECT pp.RDB$PARAMETER_NAME, pp.RDB$PARAMETER_NUMBER, "
                                "pp.RDB$NULL_FLAG, f.RDB$FIELD_TYPE, f.RDB$FIELD_LENGTH, "
                                "f.RDB$FIELD_SCALE, f.RDB$CHARACTER_SET_ID, "
                                "f.RDB$COLLATION_ID, f.RDB$DIMENSIONS, p.RDB$OWNER_NAME "
                                "FROM RDB$PROCEDURE_PARAMETERS pp "
                                "JOIN RDB$FIELDS") +
                                " f ON f.RDB$FIELD_NAME = pp.RDB$FIELD_SOURCE "
                                "JOIN RDB$PROCEDURES p "
                                "ON p.RDB$PROCEDURE_NAME = pp.RDB$PROCEDURE_NAME "
                                "WHERE pp.RDB$PROCEDURE_NAME = " +
                                quote_firebird_string(function_name_upper) +
                                " AND pp.RDB$PARAMETER_TYPE = 1 "
                                "ORDER BY pp.RDB$PARAMETER_NUMBER";

                            std::vector<std::string> column_names;
                            std::deque<std::vector<std::optional<std::string>>> rows;
                            if (!run_internal_firebird_query(metadata_sql,
                                                             column_names,
                                                             rows,
                                                             error_out)) {
                                return false;
                            }

                            for (const auto& row : rows) {
                                if (row.size() < 10) {
                                    continue;
                                }

                                LocalSqldaField column;
                                column.sqlda.field_name =
                                    row[0].has_value() ? trimAscii(*row[0]) : std::string();
                                column.sqlda.alias_name = column.sqlda.field_name;
                                column.sqlda.relation_name = function_name;
                                column.sqlda.relation_schema = metadata_schema_root;
                                column.sqlda.relation_alias = meta.binding.relation_alias;
                                column.sqlda.owner_name =
                                    row[9].has_value() ? trimAscii(*row[9]) : std::string("SYSDBA");

                                const int16_t field_type =
                                    static_cast<int16_t>(parse_int64_text(row[3], 0));
                                const int16_t field_length =
                                    static_cast<int16_t>(parse_int64_text(row[4], 0));
                                const int16_t field_scale =
                                    static_cast<int16_t>(parse_int64_text(row[5], 0));
                                const int16_t charset_id =
                                    static_cast<int16_t>(parse_int64_text(row[6], 0));
                                const int16_t dimensions =
                                    static_cast<int16_t>(parse_int64_text(row[8], 0));
                                const bool nullable = !row[2].has_value();
                                column.field = firebird_field_to_message(field_type,
                                                                         field_length,
                                                                         field_scale,
                                                                         charset_id,
                                                                         dimensions,
                                                                         nullable);
                                meta.columns.push_back(std::move(column));
                            }
                        }
                        if (meta.columns.empty()) {
                            for (size_t i = 0; i < table_ref->column_aliases.size(); ++i) {
                                LocalSqldaField column;
                                column.field.type_opcode = 37;
                                column.field.length = 8194;
                                column.field.sql_type_override = 448;
                                append_sqlda_name(column,
                                                  poolString(metadata_parser.stringPool(),
                                                             table_ref->column_aliases[i]),
                                                  i + 1);
                                meta.columns.push_back(std::move(column));
                            }
                        }
                        break;
                    }
                    default:
                        return true;
                }

                finalize_source_metadata(meta);
                sources_out.push_back(std::move(meta));
                return true;
            };

        synthesize_statement_output_columns =
            [&](const Statement* statement,
                const std::vector<const CTE*>& parent_scope,
                std::vector<LocalSqldaField>& columns_out,
                std::string& error_out) -> bool {
                columns_out.clear();
                if (statement == nullptr) {
                    return true;
                }

                const std::vector<const CTE*> cte_scope =
                    extend_cte_scope(statement, parent_scope);
                std::vector<LocalSourceMetadata> local_sources;

                auto append_named_relation_source =
                    [&](const SchemaPath& table_path,
                        bool has_alias,
                        StringPool::StringId alias_id) -> bool {
                        if (table_path.components.empty()) {
                            return true;
                        }

                        LocalSourceMetadata meta;
                        meta.binding.relation_name =
                            schemaPathObjectName(table_path, metadata_parser.stringPool());
                        meta.binding.relation_schema =
                            schemaPathSchemaName(table_path, metadata_parser.stringPool());
                        meta.binding.relation_alias =
                            has_alias ? poolString(metadata_parser.stringPool(), alias_id)
                                      : meta.binding.relation_name;
                        if (!load_relation_metadata(meta.binding, meta.columns, error_out)) {
                            return false;
                        }
                        finalize_source_metadata(meta);
                        local_sources.push_back(std::move(meta));
                        return true;
                    };

                switch (statement->kind()) {
                    case ASTKind::SelectStmt:
                    {
                        const auto* select = static_cast<const SelectStmt*>(statement);
                        if (!append_table_ref_source_metadata(select->from,
                                                              cte_scope,
                                                              local_sources,
                                                              error_out)) {
                            return false;
                        }
                        for (const auto* join : select->joins) {
                            if (join != nullptr &&
                                !append_table_ref_source_metadata(join->right,
                                                                  cte_scope,
                                                                  local_sources,
                                                                  error_out)) {
                                return false;
                            }
                        }
                        break;
                    }
                    case ASTKind::InsertStmt:
                    {
                        const auto* insert = static_cast<const InsertStmt*>(statement);
                        if (!append_named_relation_source(insert->table_path,
                                                          insert->has_alias,
                                                          insert->alias)) {
                            return false;
                        }
                        break;
                    }
                    case ASTKind::UpdateStmt:
                    {
                        const auto* update = static_cast<const UpdateStmt*>(statement);
                        if (!append_named_relation_source(update->table_path,
                                                          update->has_alias,
                                                          update->alias)) {
                            return false;
                        }
                        if (!append_table_ref_source_metadata(update->from,
                                                              cte_scope,
                                                              local_sources,
                                                              error_out)) {
                            return false;
                        }
                        for (const auto* join : update->joins) {
                            if (join != nullptr &&
                                !append_table_ref_source_metadata(join->right,
                                                                  cte_scope,
                                                                  local_sources,
                                                                  error_out)) {
                                return false;
                            }
                        }
                        break;
                    }
                    case ASTKind::DeleteStmt:
                    {
                        const auto* del = static_cast<const DeleteStmt*>(statement);
                        if (!append_named_relation_source(del->table_path,
                                                          del->has_alias,
                                                          del->alias)) {
                            return false;
                        }
                        if (!append_table_ref_source_metadata(del->using_clause,
                                                              cte_scope,
                                                              local_sources,
                                                              error_out)) {
                            return false;
                        }
                        for (const auto* join : del->using_joins) {
                            if (join != nullptr &&
                                !append_table_ref_source_metadata(join->right,
                                                                  cte_scope,
                                                                  local_sources,
                                                                  error_out)) {
                                return false;
                            }
                        }
                        break;
                    }
                    case ASTKind::MergeStmt:
                    {
                        const auto* merge = static_cast<const MergeStmt*>(statement);
                        if (!append_named_relation_source(merge->target_table,
                                                          merge->target_alias !=
                                                              StringPool::INVALID_ID,
                                                          merge->target_alias)) {
                            return false;
                        }
                        if (merge->source_query != nullptr) {
                            LocalSourceMetadata meta;
                            meta.binding.relation_name =
                                merge->source_alias != StringPool::INVALID_ID
                                    ? poolString(metadata_parser.stringPool(),
                                                 merge->source_alias)
                                    : "__MERGE_SOURCE__";
                            meta.binding.relation_alias = meta.binding.relation_name;
                            if (!synthesize_statement_output_columns(merge->source_query,
                                                                     cte_scope,
                                                                     meta.columns,
                                                                     error_out)) {
                                return false;
                            }
                            local_sources.push_back(std::move(meta));
                        }
                        else if (!merge->source_table.components.empty()) {
                            LocalSourceMetadata meta;
                            meta.binding.relation_name =
                                schemaPathObjectName(merge->source_table,
                                                     metadata_parser.stringPool());
                            meta.binding.relation_schema =
                                schemaPathSchemaName(merge->source_table,
                                                     metadata_parser.stringPool());
                            meta.binding.relation_alias =
                                merge->source_alias != StringPool::INVALID_ID
                                    ? poolString(metadata_parser.stringPool(),
                                                 merge->source_alias)
                                    : meta.binding.relation_name;
                            const CTE* cte = meta.binding.relation_schema.empty()
                                                 ? resolve_cte(meta.binding.relation_name,
                                                               cte_scope)
                                                 : nullptr;
                            if (cte != nullptr) {
                                if (!synthesize_statement_output_columns(cte->query,
                                                                         cte_scope,
                                                                         meta.columns,
                                                                         error_out)) {
                                    return false;
                                }
                                apply_column_alias_overrides(cte->column_names, meta.columns);
                            }
                            else if (!load_relation_metadata(meta.binding,
                                                             meta.columns,
                                                             error_out)) {
                                return false;
                            }
                            local_sources.push_back(std::move(meta));
                        }
                        break;
                    }
                    default:
                        break;
                }

                auto find_local_source_metadata =
                    [&](const SchemaPath& table_path) -> const LocalSourceMetadata* {
                        const std::string qualifier =
                            schemaPathObjectName(table_path, metadata_parser.stringPool());
                        for (const auto& source_meta : local_sources) {
                            if ((!source_meta.binding.relation_alias.empty() &&
                                 identifiersEqual(source_meta.binding.relation_alias, qualifier)) ||
                                (!source_meta.binding.relation_name.empty() &&
                                 identifiersEqual(source_meta.binding.relation_name, qualifier))) {
                                return &source_meta;
                            }
                        }
                        return nullptr;
                    };

                auto find_local_column_metadata =
                    [&](const ColumnRef& column) -> std::optional<LocalSqldaField> {
                        if (column.has_table_qualifier) {
                            const LocalSourceMetadata* source_meta =
                                find_local_source_metadata(column.table_path);
                            if (source_meta == nullptr) {
                                return std::nullopt;
                            }
                            const std::string column_name =
                                poolString(metadata_parser.stringPool(), column.column_name);
                            for (const auto& entry : source_meta->columns) {
                                if (identifiersEqual(entry.sqlda.field_name, column_name)) {
                                    return entry;
                                }
                            }
                            return std::nullopt;
                        }

                        const std::string column_name =
                            poolString(metadata_parser.stringPool(), column.column_name);
                        const LocalSqldaField* match = nullptr;
                        for (const auto& source_meta : local_sources) {
                            for (const auto& entry : source_meta.columns) {
                                if (!identifiersEqual(entry.sqlda.field_name, column_name)) {
                                    continue;
                                }
                                if (match != nullptr) {
                                    return std::nullopt;
                                }
                                match = &entry;
                            }
                        }
                        return match != nullptr ? std::optional<LocalSqldaField>(*match)
                                                : std::nullopt;
                    };

                std::function<std::optional<LocalSqldaField>(const Expression*)> infer_local_expr_field;
                infer_local_expr_field =
                    [&](const Expression* expr) -> std::optional<LocalSqldaField> {
                        if (expr == nullptr) {
                            return std::nullopt;
                        }

                        if (auto literal_desc = infer_field_from_literal(expr);
                            literal_desc.has_value()) {
                            LocalSqldaField out;
                            out = *literal_desc;
                            return out;
                        }

                        switch (expr->kind()) {
                            case ASTKind::ParameterExpr:
                            {
                                const auto* parameter =
                                    static_cast<const ParameterExpr*>(expr);
                                const auto it =
                                    parameter_type_hints.find(parameter_expr_key(parameter));
                                if (it != parameter_type_hints.end()) {
                                    return it->second;
                                }
                                return std::nullopt;
                            }
                            case ASTKind::ColumnRefExpr:
                                return find_local_column_metadata(
                                    static_cast<const ColumnRefExpr*>(expr)->column);
                            case ASTKind::CastExpr:
                            {
                                const auto* cast_expr = static_cast<const CastExpr*>(expr);
                                if (auto field = infer_field_from_typename(cast_expr->target_type);
                                    field.has_value()) {
                                    LocalSqldaField out;
                                    out.field = *field;
                                    return out;
                                }
                                return infer_local_expr_field(cast_expr->expr);
                            }
                            case ASTKind::FunctionCallExpr:
                            {
                                const auto* fn = static_cast<const FunctionCallExpr*>(expr);
                                if (auto inferred = infer_builtin_function_field(
                                        fn, infer_local_expr_field);
                                    inferred.has_value()) {
                                    return inferred;
                                }
                                for (const auto* arg : fn->arguments) {
                                    if (auto inferred = infer_local_expr_field(arg);
                                        inferred.has_value()) {
                                        return inferred;
                                    }
                                }
                                return std::nullopt;
                            }
                            case ASTKind::ExtractExpr:
                            {
                                LocalSqldaField out;
                                out.field.type_opcode = 16;
                                out.field.length = 8;
                                out.field.sql_type_override = 580;
                                return out;
                            }
                            case ASTKind::AlterElementExpr:
                            {
                                const auto* alter_expr =
                                    static_cast<const AlterElementExpr*>(expr);
                                if (auto inferred =
                                        infer_local_expr_field(alter_expr->new_value);
                                    inferred.has_value()) {
                                    return inferred;
                                }
                                return infer_local_expr_field(alter_expr->source);
                            }
                            case ASTKind::LiteralDomainExpr:
                            {
                                const auto* literal =
                                    static_cast<const LiteralDomainExpr*>(expr);
                                return infer_local_expr_field(literal->value);
                            }
                            case ASTKind::LiteralVariantExpr:
                            {
                                const auto* literal =
                                    static_cast<const LiteralVariantExpr*>(expr);
                                return infer_local_expr_field(literal->value);
                            }
                            case ASTKind::BinaryExpr:
                            {
                                const auto* binary = static_cast<const BinaryExpr*>(expr);
                                if (binary->op == BinaryOp::EQ || binary->op == BinaryOp::NE ||
                                    binary->op == BinaryOp::LT || binary->op == BinaryOp::LE ||
                                    binary->op == BinaryOp::GT || binary->op == BinaryOp::GE ||
                                    binary->op == BinaryOp::NULL_SAFE_EQ ||
                                    binary->op == BinaryOp::AND || binary->op == BinaryOp::OR ||
                                    binary->op == BinaryOp::REGEX_MATCH ||
                                    binary->op == BinaryOp::REGEX_MATCH_CI ||
                                    binary->op == BinaryOp::REGEX_NOT_MATCH ||
                                    binary->op == BinaryOp::REGEX_NOT_MATCH_CI ||
                                    binary->op == BinaryOp::JSON_EXISTS ||
                                    binary->op == BinaryOp::JSON_EXISTS_ANY ||
                                    binary->op == BinaryOp::JSON_EXISTS_ALL ||
                                    binary->op == BinaryOp::ARRAY_CONTAINS ||
                                    binary->op == BinaryOp::ARRAY_CONTAINED_BY ||
                                    binary->op == BinaryOp::ARRAY_OVERLAP) {
                                    LocalSqldaField out;
                                    out.field.type_opcode = 23;
                                    out.field.length = 1;
                                    out.field.sql_type_override = 32764;
                                    return out;
                                }
                                if (binary->op == BinaryOp::CONCAT) {
                                    LocalSqldaField out;
                                    out.field.type_opcode = 37;
                                    out.field.length = 8194;
                                    out.field.sql_type_override = 448;
                                    return out;
                                }
                                if (auto left = infer_local_expr_field(binary->left);
                                    left.has_value()) {
                                    return left;
                                }
                                return infer_local_expr_field(binary->right);
                            }
                            case ASTKind::UnaryExpr:
                            {
                                const auto* unary = static_cast<const UnaryExpr*>(expr);
                                if (unary->op == UnaryOp::NOT ||
                                    unary->op == UnaryOp::IS_NULL ||
                                    unary->op == UnaryOp::IS_NOT_NULL) {
                                    LocalSqldaField out;
                                    out.field.type_opcode = 23;
                                    out.field.length = 1;
                                    out.field.sql_type_override = 32764;
                                    return out;
                                }
                                return infer_local_expr_field(unary->operand);
                            }
                            case ASTKind::CaseExpr:
                            {
                                const auto* case_expr = static_cast<const CaseExpr*>(expr);
                                for (const auto& when_clause : case_expr->when_clauses) {
                                    if (auto inferred =
                                            infer_local_expr_field(when_clause.then_expr);
                                        inferred.has_value()) {
                                        return inferred;
                                    }
                                }
                                return infer_local_expr_field(case_expr->else_expr);
                            }
                            case ASTKind::SubqueryExpr:
                            {
                                const auto* subquery_expr =
                                    static_cast<const SubqueryExpr*>(expr);
                                std::vector<LocalSqldaField> nested_output;
                                if (!synthesize_statement_output_columns(subquery_expr->subquery,
                                                                         cte_scope,
                                                                         nested_output,
                                                                         error_out) ||
                                    nested_output.empty()) {
                                    return std::nullopt;
                                }
                                return nested_output.front();
                            }
                            case ASTKind::ExistsExpr:
                            case ASTKind::InExpr:
                            case ASTKind::BetweenExpr:
                            case ASTKind::LikeExpr:
                            case ASTKind::IsNullExpr:
                            {
                                LocalSqldaField out;
                                out.field.type_opcode = 23;
                                out.field.length = 1;
                                out.field.sql_type_override = 32764;
                                return out;
                            }
                            case ASTKind::ArrayExpr:
                            {
                                LocalSqldaField out;
                                out.field.type_opcode = 9;
                                out.field.length = 8;
                                out.field.sql_type_override = 540;
                                return out;
                            }
                            default:
                                return std::nullopt;
                        }
                    };

                auto append_inferred_output =
                    [&](const LocalSqldaField& inferred,
                        const std::optional<std::string>& alias_override) {
                        LocalSqldaField out = inferred;
                        if (alias_override.has_value() && !alias_override->empty()) {
                            out.sqlda.alias_name = *alias_override;
                        }
                        if (out.sqlda.field_name.empty() && !out.sqlda.alias_name.empty()) {
                            out.sqlda.field_name = out.sqlda.alias_name;
                        }
                        if (out.sqlda.alias_name.empty()) {
                            if (!out.sqlda.field_name.empty()) {
                                out.sqlda.alias_name = out.sqlda.field_name;
                            }
                            else {
                                append_sqlda_name(out, std::string{}, columns_out.size() + 1);
                            }
                        }
                        if (out.sqlda.field_name.empty()) {
                            out.sqlda.field_name = out.sqlda.alias_name;
                        }
                        columns_out.push_back(std::move(out));
                    };

                switch (statement->kind()) {
                    case ASTKind::SelectStmt:
                    {
                        const auto* select = static_cast<const SelectStmt*>(statement);
                        for (const auto* item : select->items) {
                            if (item == nullptr) {
                                continue;
                            }
                            if (item->item_type == SelectItem::Type::STAR) {
                                for (const auto& source_meta : local_sources) {
                                    for (const auto& column : source_meta.columns) {
                                        columns_out.push_back(column);
                                    }
                                }
                                continue;
                            }
                            if (item->item_type == SelectItem::Type::TABLE_STAR) {
                                const LocalSourceMetadata* source_meta =
                                    find_local_source_metadata(item->table_path);
                                if (source_meta != nullptr) {
                                    for (const auto& column : source_meta->columns) {
                                        columns_out.push_back(column);
                                    }
                                }
                                continue;
                            }

                            std::optional<LocalSqldaField> inferred =
                                infer_local_expr_field(item->expr);
                            if (!inferred.has_value()) {
                                inferred = infer_local_expr_field(item->expr);
                            }
                            if (inferred.has_value()) {
                                std::optional<std::string> alias_override;
                                if (item->has_alias) {
                                    alias_override = poolString(metadata_parser.stringPool(),
                                                                item->alias);
                                }
                                append_inferred_output(*inferred, alias_override);
                            }
                            else {
                                LocalSqldaField out;
                                out.field.sql_type_override = 32766;
                                append_sqlda_name(out,
                                                  item->has_alias
                                                      ? poolString(metadata_parser.stringPool(),
                                                                   item->alias)
                                                      : std::string{},
                                                  columns_out.size() + 1);
                                columns_out.push_back(std::move(out));
                            }
                        }
                        if (select->set_op_right != nullptr) {
                            std::vector<LocalSqldaField> right_columns;
                            if (!synthesize_statement_output_columns(select->set_op_right,
                                                                     cte_scope,
                                                                     right_columns,
                                                                     error_out)) {
                                return false;
                            }
                            merge_local_sqlda_columns(columns_out, right_columns);
                        }
                        return true;
                    }
                    case ASTKind::InsertStmt:
                    {
                        const auto* insert = static_cast<const InsertStmt*>(statement);
                        for (const auto* item : insert->returning) {
                            if (item == nullptr) {
                                continue;
                            }
                            if (item->item_type == SelectItem::Type::STAR) {
                                for (const auto& source_meta : local_sources) {
                                    for (const auto& column : source_meta.columns) {
                                        columns_out.push_back(column);
                                    }
                                }
                                continue;
                            }
                            if (item->item_type == SelectItem::Type::TABLE_STAR) {
                                const LocalSourceMetadata* source_meta =
                                    find_local_source_metadata(item->table_path);
                                if (source_meta != nullptr) {
                                    for (const auto& column : source_meta->columns) {
                                        columns_out.push_back(column);
                                    }
                                }
                                continue;
                            }
                            if (auto inferred = infer_local_expr_field(item->expr);
                                inferred.has_value()) {
                                std::optional<std::string> alias_override;
                                if (item->has_alias) {
                                    alias_override = poolString(metadata_parser.stringPool(),
                                                                item->alias);
                                }
                                append_inferred_output(*inferred, alias_override);
                            }
                        }
                        return true;
                    }
                    case ASTKind::UpdateStmt:
                    {
                        const auto* update = static_cast<const UpdateStmt*>(statement);
                        for (const auto* item : update->returning) {
                            if (item == nullptr) {
                                continue;
                            }
                            if (item->item_type == SelectItem::Type::STAR) {
                                for (const auto& source_meta : local_sources) {
                                    for (const auto& column : source_meta.columns) {
                                        columns_out.push_back(column);
                                    }
                                }
                                continue;
                            }
                            if (item->item_type == SelectItem::Type::TABLE_STAR) {
                                const LocalSourceMetadata* source_meta =
                                    find_local_source_metadata(item->table_path);
                                if (source_meta != nullptr) {
                                    for (const auto& column : source_meta->columns) {
                                        columns_out.push_back(column);
                                    }
                                }
                                continue;
                            }
                            if (auto inferred = infer_local_expr_field(item->expr);
                                inferred.has_value()) {
                                std::optional<std::string> alias_override;
                                if (item->has_alias) {
                                    alias_override = poolString(metadata_parser.stringPool(),
                                                                item->alias);
                                }
                                append_inferred_output(*inferred, alias_override);
                            }
                        }
                        return true;
                    }
                    case ASTKind::DeleteStmt:
                    {
                        const auto* del = static_cast<const DeleteStmt*>(statement);
                        for (const auto* item : del->returning) {
                            if (item == nullptr) {
                                continue;
                            }
                            if (item->item_type == SelectItem::Type::STAR) {
                                for (const auto& source_meta : local_sources) {
                                    for (const auto& column : source_meta.columns) {
                                        columns_out.push_back(column);
                                    }
                                }
                                continue;
                            }
                            if (item->item_type == SelectItem::Type::TABLE_STAR) {
                                const LocalSourceMetadata* source_meta =
                                    find_local_source_metadata(item->table_path);
                                if (source_meta != nullptr) {
                                    for (const auto& column : source_meta->columns) {
                                        columns_out.push_back(column);
                                    }
                                }
                                continue;
                            }
                            if (auto inferred = infer_local_expr_field(item->expr);
                                inferred.has_value()) {
                                std::optional<std::string> alias_override;
                                if (item->has_alias) {
                                    alias_override = poolString(metadata_parser.stringPool(),
                                                                item->alias);
                                }
                                append_inferred_output(*inferred, alias_override);
                            }
                        }
                        return true;
                    }
                    default:
                        return true;
                }
            };

        std::vector<LocalSourceMetadata> source_metadata;
        std::string source_metadata_error;
        const std::vector<const CTE*> root_cte_scope =
            extend_cte_scope(metadata_parse.statement.get(), {});

        auto append_named_source_metadata =
            [&](const SchemaPath& table_path,
                bool has_alias,
                StringPool::StringId alias_id) -> bool {
                if (table_path.components.empty()) {
                    return true;
                }

                LocalSourceMetadata meta;
                meta.binding.relation_name =
                    schemaPathObjectName(table_path, metadata_parser.stringPool());
                meta.binding.relation_schema =
                    schemaPathSchemaName(table_path, metadata_parser.stringPool());
                meta.binding.relation_alias =
                    has_alias ? poolString(metadata_parser.stringPool(), alias_id)
                              : meta.binding.relation_name;
                if (!load_relation_metadata(meta.binding, meta.columns, source_metadata_error)) {
                    return false;
                }
                finalize_source_metadata(meta);
                source_metadata.push_back(std::move(meta));
                return true;
            };

        switch (metadata_parse.statement->kind()) {
            case ASTKind::SelectStmt:
            {
                const auto* select = static_cast<const SelectStmt*>(metadata_parse.statement.get());
                if (!append_table_ref_source_metadata(select->from,
                                                      root_cte_scope,
                                                      source_metadata,
                                                      source_metadata_error)) {
                    return sendErrorResponse(state, source_metadata_error);
                }
                for (const auto* join : select->joins) {
                    if (join != nullptr &&
                        !append_table_ref_source_metadata(join->right,
                                                          root_cte_scope,
                                                          source_metadata,
                                                          source_metadata_error)) {
                        return sendErrorResponse(state, source_metadata_error);
                    }
                }
                break;
            }
            case ASTKind::InsertStmt:
            {
                const auto* insert = static_cast<const InsertStmt*>(metadata_parse.statement.get());
                if (!append_named_source_metadata(insert->table_path,
                                                  insert->has_alias,
                                                  insert->alias)) {
                    return sendErrorResponse(state, source_metadata_error);
                }
                break;
            }
            case ASTKind::UpdateStmt:
            {
                const auto* update = static_cast<const UpdateStmt*>(metadata_parse.statement.get());
                if (!append_named_source_metadata(update->table_path,
                                                  update->has_alias,
                                                  update->alias) ||
                    !append_table_ref_source_metadata(update->from,
                                                      root_cte_scope,
                                                      source_metadata,
                                                      source_metadata_error)) {
                    return sendErrorResponse(state, source_metadata_error);
                }
                for (const auto* join : update->joins) {
                    if (join != nullptr &&
                        !append_table_ref_source_metadata(join->right,
                                                          root_cte_scope,
                                                          source_metadata,
                                                          source_metadata_error)) {
                        return sendErrorResponse(state, source_metadata_error);
                    }
                }
                break;
            }
            case ASTKind::DeleteStmt:
            {
                const auto* del = static_cast<const DeleteStmt*>(metadata_parse.statement.get());
                if (!append_named_source_metadata(del->table_path,
                                                  del->has_alias,
                                                  del->alias) ||
                    !append_table_ref_source_metadata(del->using_clause,
                                                      root_cte_scope,
                                                      source_metadata,
                                                      source_metadata_error)) {
                    return sendErrorResponse(state, source_metadata_error);
                }
                for (const auto* join : del->using_joins) {
                    if (join != nullptr &&
                        !append_table_ref_source_metadata(join->right,
                                                          root_cte_scope,
                                                          source_metadata,
                                                          source_metadata_error)) {
                        return sendErrorResponse(state, source_metadata_error);
                    }
                }
                break;
            }
            case ASTKind::MergeStmt:
            {
                const auto* merge = static_cast<const MergeStmt*>(metadata_parse.statement.get());
                if (!append_named_source_metadata(merge->target_table,
                                                  merge->target_alias != StringPool::INVALID_ID,
                                                  merge->target_alias)) {
                    return sendErrorResponse(state, source_metadata_error);
                }
                if (merge->source_query != nullptr) {
                    LocalSourceMetadata meta;
                    meta.binding.relation_name =
                        merge->source_alias != StringPool::INVALID_ID
                            ? poolString(metadata_parser.stringPool(), merge->source_alias)
                            : "__MERGE_SOURCE__";
                    meta.binding.relation_alias = meta.binding.relation_name;
                    if (!synthesize_statement_output_columns(merge->source_query,
                                                             root_cte_scope,
                                                             meta.columns,
                                                             source_metadata_error)) {
                        return sendErrorResponse(state, source_metadata_error);
                    }
                    source_metadata.push_back(std::move(meta));
                }
                else if (!merge->source_table.components.empty()) {
                    LocalSourceMetadata meta;
                    meta.binding.relation_name =
                        schemaPathObjectName(merge->source_table,
                                             metadata_parser.stringPool());
                    meta.binding.relation_schema =
                        schemaPathSchemaName(merge->source_table,
                                             metadata_parser.stringPool());
                    meta.binding.relation_alias =
                        merge->source_alias != StringPool::INVALID_ID
                            ? poolString(metadata_parser.stringPool(), merge->source_alias)
                            : meta.binding.relation_name;
                    const CTE* cte = meta.binding.relation_schema.empty()
                                         ? resolve_cte(meta.binding.relation_name,
                                                       root_cte_scope)
                                         : nullptr;
                    if (cte != nullptr) {
                        if (!synthesize_statement_output_columns(cte->query,
                                                                 root_cte_scope,
                                                                 meta.columns,
                                                                 source_metadata_error)) {
                            return sendErrorResponse(state, source_metadata_error);
                        }
                        apply_column_alias_overrides(cte->column_names, meta.columns);
                    }
                    else if (!load_relation_metadata(meta.binding,
                                                     meta.columns,
                                                     source_metadata_error)) {
                        return sendErrorResponse(state, source_metadata_error);
                    }
                    source_metadata.push_back(std::move(meta));
                }
                break;
            }
            default:
                break;
        }

        auto find_source_metadata =
            [&](const SchemaPath& table_path) -> const LocalSourceMetadata* {
                const std::string qualifier = schemaPathObjectName(table_path,
                                                                   metadata_parser.stringPool());
                for (const auto& source_meta : source_metadata) {
                    if ((!source_meta.binding.relation_alias.empty() &&
                         identifiersEqual(source_meta.binding.relation_alias, qualifier)) ||
                        (!source_meta.binding.relation_name.empty() &&
                         identifiersEqual(source_meta.binding.relation_name, qualifier))) {
                        return &source_meta;
                    }
                }
                return nullptr;
            };

        auto find_column_metadata =
            [&](const ColumnRef& column) -> std::optional<LocalSqldaField> {
                if (column.has_table_qualifier) {
                    const LocalSourceMetadata* source_meta =
                        find_source_metadata(column.table_path);
                    if (source_meta == nullptr) {
                        return std::nullopt;
                    }
                    const std::string column_name =
                        poolString(metadata_parser.stringPool(), column.column_name);
                    for (const auto& entry : source_meta->columns) {
                        if (identifiersEqual(entry.sqlda.field_name, column_name)) {
                            return entry;
                        }
                    }
                    return std::nullopt;
                }

                const std::string column_name =
                    poolString(metadata_parser.stringPool(), column.column_name);
                const LocalSqldaField* match = nullptr;
                for (const auto& source_meta : source_metadata) {
                    std::cerr << "[parser_debug] firebird source metadata relation="
                              << source_meta.binding.relation_name
                              << " alias=" << source_meta.binding.relation_alias
                              << " column_count=" << source_meta.columns.size()
                              << " looking_for=" << column_name << "\n";
                    for (const auto& entry : source_meta.columns) {
                        std::cerr << "[parser_debug] firebird source metadata candidate="
                                  << entry.sqlda.field_name << " sql_type="
                                  << entry.field.sql_type_override << " len="
                                  << entry.field.length << "\n";
                        if (!identifiersEqual(entry.sqlda.field_name, column_name)) {
                            continue;
                        }
                        if (match != nullptr) {
                            return std::nullopt;
                        }
                        match = &entry;
                    }
                }
                return match != nullptr ? std::optional<LocalSqldaField>(*match)
                                        : std::nullopt;
            };

        std::function<std::optional<LocalSqldaField>(const Expression*)> infer_expr_field;
        infer_expr_field = [&](const Expression* expr) -> std::optional<LocalSqldaField> {
            if (expr == nullptr) {
                return std::nullopt;
            }

            if (auto literal_desc = infer_field_from_literal(expr); literal_desc.has_value()) {
                return literal_desc;
            }

            switch (expr->kind()) {
                case ASTKind::ParameterExpr:
                {
                    const auto* parameter = static_cast<const ParameterExpr*>(expr);
                    const auto it =
                        parameter_type_hints.find(parameter_expr_key(parameter));
                    if (it != parameter_type_hints.end()) {
                        return it->second;
                    }
                    return std::nullopt;
                }
                case ASTKind::ColumnRefExpr:
                {
                    const auto* column_expr = static_cast<const ColumnRefExpr*>(expr);
                    return find_column_metadata(column_expr->column);
                }
                case ASTKind::CastExpr:
                {
                    const auto* cast_expr = static_cast<const CastExpr*>(expr);
                    if (auto field = infer_field_from_typename(cast_expr->target_type);
                        field.has_value()) {
                        LocalSqldaField out;
                        out.field = *field;
                        return out;
                    }
                    return infer_expr_field(cast_expr->expr);
                }
                case ASTKind::FunctionCallExpr:
                {
                    const auto* fn = static_cast<const FunctionCallExpr*>(expr);
                    if (auto inferred = infer_builtin_function_field(fn, infer_expr_field);
                        inferred.has_value()) {
                        return inferred;
                    }
                    for (const auto* arg : fn->arguments) {
                        if (auto inferred = infer_expr_field(arg); inferred.has_value()) {
                            return inferred;
                        }
                    }
                    return std::nullopt;
                }
                case ASTKind::ExtractExpr:
                {
                    LocalSqldaField out;
                    out.field.type_opcode = 16;
                    out.field.length = 8;
                    out.field.sql_type_override = 580;
                    return out;
                }
                case ASTKind::AlterElementExpr:
                {
                    const auto* alter_expr = static_cast<const AlterElementExpr*>(expr);
                    if (auto inferred = infer_expr_field(alter_expr->new_value);
                        inferred.has_value()) {
                        return inferred;
                    }
                    return infer_expr_field(alter_expr->source);
                }
                case ASTKind::LiteralDomainExpr:
                {
                    const auto* literal = static_cast<const LiteralDomainExpr*>(expr);
                    return infer_expr_field(literal->value);
                }
                case ASTKind::LiteralVariantExpr:
                {
                    const auto* literal = static_cast<const LiteralVariantExpr*>(expr);
                    return infer_expr_field(literal->value);
                }
                case ASTKind::BinaryExpr:
                {
                    const auto* binary = static_cast<const BinaryExpr*>(expr);
                    if (binary->op == BinaryOp::EQ || binary->op == BinaryOp::NE ||
                        binary->op == BinaryOp::LT || binary->op == BinaryOp::LE ||
                        binary->op == BinaryOp::GT || binary->op == BinaryOp::GE ||
                        binary->op == BinaryOp::NULL_SAFE_EQ || binary->op == BinaryOp::AND ||
                        binary->op == BinaryOp::OR || binary->op == BinaryOp::REGEX_MATCH ||
                        binary->op == BinaryOp::REGEX_MATCH_CI ||
                        binary->op == BinaryOp::REGEX_NOT_MATCH ||
                        binary->op == BinaryOp::REGEX_NOT_MATCH_CI ||
                        binary->op == BinaryOp::JSON_EXISTS ||
                        binary->op == BinaryOp::JSON_EXISTS_ANY ||
                        binary->op == BinaryOp::JSON_EXISTS_ALL ||
                        binary->op == BinaryOp::ARRAY_CONTAINS ||
                        binary->op == BinaryOp::ARRAY_CONTAINED_BY ||
                        binary->op == BinaryOp::ARRAY_OVERLAP) {
                        LocalSqldaField out;
                        out.field.type_opcode = 23;
                        out.field.length = 1;
                        out.field.sql_type_override = 32764;
                        return out;
                    }
                    if (binary->op == BinaryOp::CONCAT) {
                        LocalSqldaField out;
                        out.field.type_opcode = 37;
                        out.field.length = 8194;
                        out.field.sql_type_override = 448;
                        return out;
                    }
                    if (auto left = infer_expr_field(binary->left); left.has_value()) {
                        return left;
                    }
                    return infer_expr_field(binary->right);
                }
                case ASTKind::UnaryExpr:
                {
                    const auto* unary = static_cast<const UnaryExpr*>(expr);
                    if (unary->op == UnaryOp::NOT ||
                        unary->op == UnaryOp::IS_NULL ||
                        unary->op == UnaryOp::IS_NOT_NULL) {
                        LocalSqldaField out;
                        out.field.type_opcode = 23;
                        out.field.length = 1;
                        out.field.sql_type_override = 32764;
                        return out;
                    }
                    return infer_expr_field(unary->operand);
                }
                case ASTKind::CaseExpr:
                {
                    const auto* case_expr = static_cast<const CaseExpr*>(expr);
                    for (const auto& when_clause : case_expr->when_clauses) {
                        if (auto inferred = infer_expr_field(when_clause.then_expr);
                            inferred.has_value()) {
                            return inferred;
                        }
                    }
                    return infer_expr_field(case_expr->else_expr);
                }
                case ASTKind::SubqueryExpr:
                {
                    const auto* subquery_expr = static_cast<const SubqueryExpr*>(expr);
                    std::vector<LocalSqldaField> nested_output;
                    std::string nested_error;
                    if (!synthesize_statement_output_columns(subquery_expr->subquery,
                                                             root_cte_scope,
                                                             nested_output,
                                                             nested_error) ||
                        nested_output.empty()) {
                        return std::nullopt;
                    }
                    return nested_output.front();
                }
                case ASTKind::ExistsExpr:
                case ASTKind::InExpr:
                case ASTKind::BetweenExpr:
                case ASTKind::LikeExpr:
                case ASTKind::IsNullExpr:
                {
                    LocalSqldaField out;
                    out.field.type_opcode = 23;
                    out.field.length = 1;
                    out.field.sql_type_override = 32764;
                    return out;
                }
                case ASTKind::ArrayExpr:
                {
                    LocalSqldaField out;
                    out.field.type_opcode = 9;
                    out.field.length = 8;
                    out.field.sql_type_override = 540;
                    return out;
                }
                default:
                    return std::nullopt;
            }
        };

        auto append_output_field =
            [&](const LocalSqldaField& inferred,
                const std::optional<std::string>& alias_override,
                std::vector<FBSqldaVarDesc>& sqlda_out,
                std::vector<FBMessageFieldDesc>& fields_out) {
                const size_t ordinal = sqlda_out.size() + 1;
                appendDefaultSqldaField(fields_out, sqlda_out);
                fields_out[fields_out.size() - 2] = inferred.field;
                FBSqldaVarDesc sqlda = inferred.sqlda;
                if (alias_override.has_value() && !alias_override->empty()) {
                    sqlda.alias_name = *alias_override;
                }
                if (sqlda.field_name.empty() && !sqlda.alias_name.empty()) {
                    sqlda.field_name = sqlda.alias_name;
                }
                if (sqlda.alias_name.empty()) {
                    if (!sqlda.field_name.empty()) {
                        sqlda.alias_name = sqlda.field_name;
                    }
                    else {
                        sqlda.alias_name = "EXPR$" + std::to_string(ordinal);
                    }
                }
                if (sqlda.field_name.empty()) {
                    sqlda.field_name = sqlda.alias_name;
                }
                sqlda_out.back() = std::move(sqlda);
            };

        std::function<void(const SelectStmt*,
                           std::vector<FBSqldaVarDesc>&,
                           std::vector<FBMessageFieldDesc>&)> collect_select_parameter_fields;
        std::function<void(const Statement*,
                           std::vector<FBSqldaVarDesc>&,
                           std::vector<FBMessageFieldDesc>&)> collect_statement_parameter_fields;
        std::function<void(const TableRefNode*,
                           std::vector<FBSqldaVarDesc>&,
                           std::vector<FBMessageFieldDesc>&)> collect_table_ref_parameter_fields;
        std::function<void(const Expression*,
                           const std::optional<LocalSqldaField>&,
                           std::vector<FBSqldaVarDesc>&,
                           std::vector<FBMessageFieldDesc>&)> collect_parameter_fields;
        collect_parameter_fields =
            [&](const Expression* expr,
                const std::optional<LocalSqldaField>& hint,
                std::vector<FBSqldaVarDesc>& sqlda_out,
                std::vector<FBMessageFieldDesc>& fields_out) {
                if (expr == nullptr) {
                    return;
                }

                switch (expr->kind()) {
                    case ASTKind::ParameterExpr:
                    {
                        const auto* parameter = static_cast<const ParameterExpr*>(expr);
                        std::optional<LocalSqldaField> effective_hint = hint;
                        const std::string parameter_key = parameter_expr_key(parameter);
                        if (!effective_hint.has_value() && !parameter_key.empty()) {
                            const auto it = parameter_type_hints.find(parameter_key);
                            if (it != parameter_type_hints.end()) {
                                effective_hint = it->second;
                            }
                        }
                        appendDefaultSqldaField(fields_out, sqlda_out);
                        if (effective_hint.has_value()) {
                            fields_out[fields_out.size() - 2] = effective_hint->field;
                            sqlda_out.back() = effective_hint->sqlda;
                        }
                        if (!parameter_key.empty() && effective_hint.has_value()) {
                            auto it = parameter_type_hints.find(parameter_key);
                            if (it == parameter_type_hints.end()) {
                                parameter_type_hints.emplace(parameter_key, *effective_hint);
                            }
                            else {
                                if (is_placeholder_field(it->second.field) &&
                                    !is_placeholder_field(effective_hint->field)) {
                                    it->second.field = effective_hint->field;
                                }
                                if (it->second.sqlda.field_name.empty()) {
                                    it->second.sqlda.field_name =
                                        effective_hint->sqlda.field_name;
                                }
                                if (it->second.sqlda.alias_name.empty()) {
                                    it->second.sqlda.alias_name =
                                        effective_hint->sqlda.alias_name;
                                }
                                if (it->second.sqlda.relation_name.empty()) {
                                    it->second.sqlda.relation_name =
                                        effective_hint->sqlda.relation_name;
                                }
                                if (it->second.sqlda.relation_schema.empty()) {
                                    it->second.sqlda.relation_schema =
                                        effective_hint->sqlda.relation_schema;
                                }
                                if (it->second.sqlda.relation_alias.empty()) {
                                    it->second.sqlda.relation_alias =
                                        effective_hint->sqlda.relation_alias;
                                }
                                if (it->second.sqlda.owner_name.empty()) {
                                    it->second.sqlda.owner_name =
                                        effective_hint->sqlda.owner_name;
                                }
                            }
                        }
                        return;
                    }
                    case ASTKind::BinaryExpr:
                    {
                        const auto* binary = static_cast<const BinaryExpr*>(expr);
                        const auto left_hint = infer_expr_field(binary->right);
                        const auto right_hint = infer_expr_field(binary->left);
                        collect_parameter_fields(binary->left,
                                                 left_hint.has_value() ? left_hint : hint,
                                                 sqlda_out,
                                                 fields_out);
                        collect_parameter_fields(binary->right,
                                                 right_hint.has_value() ? right_hint : hint,
                                                 sqlda_out,
                                                 fields_out);
                        return;
                    }
                    case ASTKind::UnaryExpr:
                    {
                        const auto* unary = static_cast<const UnaryExpr*>(expr);
                        collect_parameter_fields(unary->operand, hint, sqlda_out, fields_out);
                        return;
                    }
                    case ASTKind::FunctionCallExpr:
                    {
                        const auto* fn = static_cast<const FunctionCallExpr*>(expr);
                        std::optional<LocalSqldaField> argument_hint = hint;
                        if (!argument_hint.has_value()) {
                            for (const auto* arg : fn->arguments) {
                                argument_hint = infer_expr_field(arg);
                                if (argument_hint.has_value()) {
                                    break;
                                }
                            }
                        }
                        for (const auto* arg : fn->arguments) {
                            collect_parameter_fields(arg,
                                                     argument_hint,
                                                     sqlda_out,
                                                     fields_out);
                        }
                        collect_parameter_fields(fn->filter, std::nullopt, sqlda_out, fields_out);
                        for (const auto* order_item : fn->order_by) {
                            if (order_item != nullptr) {
                                collect_parameter_fields(order_item->expr,
                                                         std::nullopt,
                                                         sqlda_out,
                                                         fields_out);
                            }
                        }
                        if (fn->window != nullptr) {
                            for (const auto* part : fn->window->partition_by) {
                                collect_parameter_fields(part, std::nullopt, sqlda_out, fields_out);
                            }
                            for (const auto* order_item : fn->window->order_by) {
                                if (order_item != nullptr) {
                                    collect_parameter_fields(order_item->expr,
                                                             std::nullopt,
                                                             sqlda_out,
                                                             fields_out);
                                }
                            }
                            collect_parameter_fields(fn->window->frame_start_value,
                                                     std::nullopt,
                                                     sqlda_out,
                                                     fields_out);
                            collect_parameter_fields(fn->window->frame_end_value,
                                                     std::nullopt,
                                                     sqlda_out,
                                                     fields_out);
                        }
                        return;
                    }
                    case ASTKind::CastExpr:
                    {
                        const auto* cast_expr = static_cast<const CastExpr*>(expr);
                        std::optional<LocalSqldaField> cast_hint;
                        if (auto field = infer_field_from_typename(cast_expr->target_type);
                            field.has_value()) {
                            LocalSqldaField local;
                            local.field = *field;
                            cast_hint = local;
                        }
                        collect_parameter_fields(cast_expr->expr,
                                                 cast_hint.has_value() ? cast_hint : hint,
                                                 sqlda_out,
                                                 fields_out);
                        return;
                    }
                    case ASTKind::ExtractExpr:
                    {
                        const auto* extract_expr = static_cast<const ExtractExpr*>(expr);
                        collect_parameter_fields(extract_expr->source,
                                                 std::nullopt,
                                                 sqlda_out,
                                                 fields_out);
                        collect_parameter_fields(extract_expr->selector.expr,
                                                 std::nullopt,
                                                 sqlda_out,
                                                 fields_out);
                        for (const auto* arg : extract_expr->selector.args) {
                            collect_parameter_fields(arg, std::nullopt, sqlda_out, fields_out);
                        }
                        return;
                    }
                    case ASTKind::AlterElementExpr:
                    {
                        const auto* alter_expr = static_cast<const AlterElementExpr*>(expr);
                        collect_parameter_fields(alter_expr->source,
                                                 std::nullopt,
                                                 sqlda_out,
                                                 fields_out);
                        collect_parameter_fields(alter_expr->new_value,
                                                 hint,
                                                 sqlda_out,
                                                 fields_out);
                        collect_parameter_fields(alter_expr->selector.expr,
                                                 std::nullopt,
                                                 sqlda_out,
                                                 fields_out);
                        for (const auto* arg : alter_expr->selector.args) {
                            collect_parameter_fields(arg, std::nullopt, sqlda_out, fields_out);
                        }
                        return;
                    }
                    case ASTKind::LiteralDomainExpr:
                    {
                        const auto* literal = static_cast<const LiteralDomainExpr*>(expr);
                        collect_parameter_fields(literal->value,
                                                 hint,
                                                 sqlda_out,
                                                 fields_out);
                        return;
                    }
                    case ASTKind::LiteralVariantExpr:
                    {
                        const auto* literal = static_cast<const LiteralVariantExpr*>(expr);
                        collect_parameter_fields(literal->value,
                                                 hint,
                                                 sqlda_out,
                                                 fields_out);
                        return;
                    }
                    case ASTKind::LiteralRangeExpr:
                    {
                        const auto* literal = static_cast<const LiteralRangeExpr*>(expr);
                        std::optional<LocalSqldaField> bound_hint = hint;
                        if (!bound_hint.has_value()) {
                            if (auto field = infer_field_from_typename(literal->range_base_type);
                                field.has_value()) {
                                LocalSqldaField local;
                                local.field = *field;
                                bound_hint = local;
                            }
                        }
                        collect_parameter_fields(literal->lower,
                                                 bound_hint,
                                                 sqlda_out,
                                                 fields_out);
                        collect_parameter_fields(literal->upper,
                                                 bound_hint,
                                                 sqlda_out,
                                                 fields_out);
                        return;
                    }
                    case ASTKind::LiteralArrayExpr:
                    {
                        const auto* literal = static_cast<const LiteralArrayExpr*>(expr);
                        std::optional<LocalSqldaField> element_hint = hint;
                        if (!element_hint.has_value()) {
                            if (auto field = infer_field_from_typename(literal->element_type);
                                field.has_value()) {
                                LocalSqldaField local;
                                local.field = *field;
                                element_hint = local;
                            }
                            if (!element_hint.has_value()) {
                                for (const auto* element_expr : literal->elements) {
                                    element_hint = infer_expr_field(element_expr);
                                    if (element_hint.has_value()) {
                                        break;
                                    }
                                }
                            }
                        }
                        for (const auto* element_expr : literal->elements) {
                            collect_parameter_fields(element_expr,
                                                     element_hint,
                                                     sqlda_out,
                                                     fields_out);
                        }
                        return;
                    }
                    case ASTKind::LiteralRowExpr:
                    {
                        const auto* literal = static_cast<const LiteralRowExpr*>(expr);
                        for (const auto& field : literal->fields) {
                            collect_parameter_fields(field.value,
                                                     std::nullopt,
                                                     sqlda_out,
                                                     fields_out);
                        }
                        return;
                    }
                    case ASTKind::LiteralCompositeExpr:
                    {
                        const auto* literal = static_cast<const LiteralCompositeExpr*>(expr);
                        for (const auto& field : literal->fields) {
                            collect_parameter_fields(field.value,
                                                     std::nullopt,
                                                     sqlda_out,
                                                     fields_out);
                        }
                        return;
                    }
                    case ASTKind::CaseExpr:
                    {
                        const auto* case_expr = static_cast<const CaseExpr*>(expr);
                        std::optional<LocalSqldaField> operand_hint = hint;
                        if (!operand_hint.has_value()) {
                            for (const auto& when_clause : case_expr->when_clauses) {
                                operand_hint = infer_expr_field(when_clause.when_expr);
                                if (operand_hint.has_value()) {
                                    break;
                                }
                            }
                        }
                        collect_parameter_fields(case_expr->operand,
                                                 operand_hint,
                                                 sqlda_out,
                                                 fields_out);
                        std::optional<LocalSqldaField> branch_hint = hint;
                        if (!branch_hint.has_value()) {
                            for (const auto& when_clause : case_expr->when_clauses) {
                                branch_hint = infer_expr_field(when_clause.then_expr);
                                if (branch_hint.has_value()) {
                                    break;
                                }
                            }
                            if (!branch_hint.has_value()) {
                                branch_hint = infer_expr_field(case_expr->else_expr);
                            }
                        }
                        for (const auto& when_clause : case_expr->when_clauses) {
                            collect_parameter_fields(when_clause.when_expr,
                                                     std::nullopt,
                                                     sqlda_out,
                                                     fields_out);
                            collect_parameter_fields(when_clause.then_expr,
                                                     branch_hint,
                                                     sqlda_out,
                                                     fields_out);
                        }
                        collect_parameter_fields(case_expr->else_expr,
                                                 branch_hint,
                                                 sqlda_out,
                                                 fields_out);
                        return;
                    }
                    case ASTKind::SubqueryExpr:
                    {
                        const auto* subquery_expr = static_cast<const SubqueryExpr*>(expr);
                        collect_select_parameter_fields(subquery_expr->subquery,
                                                        sqlda_out,
                                                        fields_out);
                        return;
                    }
                    case ASTKind::ExistsExpr:
                    {
                        const auto* exists_expr = static_cast<const ExistsExpr*>(expr);
                        collect_select_parameter_fields(exists_expr->subquery,
                                                        sqlda_out,
                                                        fields_out);
                        return;
                    }
                    case ASTKind::InExpr:
                    {
                        const auto* in_expr = static_cast<const InExpr*>(expr);
                        std::optional<LocalSqldaField> side_hint = infer_expr_field(in_expr->expr);
                        if (!side_hint.has_value()) {
                            for (const auto* value_expr : in_expr->values) {
                                side_hint = infer_expr_field(value_expr);
                                if (side_hint.has_value()) {
                                    break;
                                }
                            }
                        }
                        collect_parameter_fields(in_expr->expr,
                                                 side_hint.has_value() ? side_hint : hint,
                                                 sqlda_out,
                                                 fields_out);
                        for (const auto* value_expr : in_expr->values) {
                            collect_parameter_fields(value_expr,
                                                     side_hint,
                                                     sqlda_out,
                                                     fields_out);
                        }
                        if (in_expr->has_subquery) {
                            collect_select_parameter_fields(in_expr->subquery,
                                                            sqlda_out,
                                                            fields_out);
                        }
                        return;
                    }
                    case ASTKind::BetweenExpr:
                    {
                        const auto* between_expr = static_cast<const BetweenExpr*>(expr);
                        std::optional<LocalSqldaField> bound_hint = infer_expr_field(between_expr->expr);
                        if (!bound_hint.has_value()) {
                            bound_hint = infer_expr_field(between_expr->low);
                        }
                        if (!bound_hint.has_value()) {
                            bound_hint = infer_expr_field(between_expr->high);
                        }
                        collect_parameter_fields(between_expr->expr,
                                                 bound_hint.has_value() ? bound_hint : hint,
                                                 sqlda_out,
                                                 fields_out);
                        collect_parameter_fields(between_expr->low,
                                                 bound_hint,
                                                 sqlda_out,
                                                 fields_out);
                        collect_parameter_fields(between_expr->high,
                                                 bound_hint,
                                                 sqlda_out,
                                                 fields_out);
                        return;
                    }
                    case ASTKind::LikeExpr:
                    {
                        const auto* like_expr = static_cast<const LikeExpr*>(expr);
                        LocalSqldaField text_hint;
                        text_hint.field.type_opcode = 37;
                        text_hint.field.length = 8194;
                        text_hint.field.sql_type_override = 448;
                        collect_parameter_fields(like_expr->expr, text_hint, sqlda_out, fields_out);
                        collect_parameter_fields(like_expr->pattern,
                                                 text_hint,
                                                 sqlda_out,
                                                 fields_out);
                        collect_parameter_fields(like_expr->escape,
                                                 text_hint,
                                                 sqlda_out,
                                                 fields_out);
                        return;
                    }
                    case ASTKind::IsNullExpr:
                    {
                        const auto* is_null_expr = static_cast<const IsNullExpr*>(expr);
                        collect_parameter_fields(is_null_expr->expr,
                                                 hint,
                                                 sqlda_out,
                                                 fields_out);
                        return;
                    }
                    case ASTKind::ArrayExpr:
                    {
                        const auto* array_expr = static_cast<const ArrayExpr*>(expr);
                        std::optional<LocalSqldaField> element_hint = hint;
                        if (!element_hint.has_value()) {
                            for (const auto* element_expr : array_expr->elements) {
                                element_hint = infer_expr_field(element_expr);
                                if (element_hint.has_value()) {
                                    break;
                                }
                            }
                        }
                        for (const auto* element_expr : array_expr->elements) {
                            collect_parameter_fields(element_expr,
                                                     element_hint,
                                                     sqlda_out,
                                                     fields_out);
                        }
                        if (array_expr->has_subquery) {
                            collect_select_parameter_fields(array_expr->subquery,
                                                            sqlda_out,
                                                            fields_out);
                        }
                        return;
                    }
                    default:
                        return;
                }
            };

        collect_table_ref_parameter_fields =
            [&](const TableRefNode* table_ref,
                std::vector<FBSqldaVarDesc>& sqlda_out,
                std::vector<FBMessageFieldDesc>& fields_out) {
                if (table_ref == nullptr) {
                    return;
                }

                collect_parameter_fields(table_ref->sample_percent,
                                         std::nullopt,
                                         sqlda_out,
                                         fields_out);
                collect_parameter_fields(table_ref->sample_repeatable_seed,
                                         std::nullopt,
                                         sqlda_out,
                                         fields_out);

                switch (table_ref->ref_type) {
                    case TableRefNode::Type::SUBQUERY:
                        collect_statement_parameter_fields(table_ref->subquery,
                                                           sqlda_out,
                                                           fields_out);
                        return;
                    case TableRefNode::Type::FUNCTION:
                        collect_parameter_fields(table_ref->function,
                                                 std::nullopt,
                                                 sqlda_out,
                                                 fields_out);
                        return;
                    default:
                        return;
                }
            };

        collect_select_parameter_fields =
            [&](const SelectStmt* select,
                std::vector<FBSqldaVarDesc>& sqlda_out,
                std::vector<FBMessageFieldDesc>& fields_out) {
                if (select == nullptr) {
                    return;
                }
                collect_table_ref_parameter_fields(select->from, sqlda_out, fields_out);
                for (const auto* item : select->items) {
                    if (item != nullptr) {
                        collect_parameter_fields(item->expr, std::nullopt, sqlda_out, fields_out);
                    }
                }
                collect_parameter_fields(select->where, std::nullopt, sqlda_out, fields_out);
                collect_parameter_fields(select->having, std::nullopt, sqlda_out, fields_out);
                collect_parameter_fields(select->limit, std::nullopt, sqlda_out, fields_out);
                collect_parameter_fields(select->offset, std::nullopt, sqlda_out, fields_out);
                collect_parameter_fields(select->fetch_row_count,
                                         std::nullopt,
                                         sqlda_out,
                                         fields_out);
                for (const auto* expr : select->distinct_on) {
                    collect_parameter_fields(expr, std::nullopt, sqlda_out, fields_out);
                }
                for (const auto* expr : select->group_by) {
                    collect_parameter_fields(expr, std::nullopt, sqlda_out, fields_out);
                }
                for (const auto& grouping_set : select->grouping_sets) {
                    for (const auto* expr : grouping_set) {
                        collect_parameter_fields(expr,
                                                 std::nullopt,
                                                 sqlda_out,
                                                 fields_out);
                    }
                }
                for (const auto& window_entry : select->windows) {
                    const auto* window = window_entry.second;
                    if (window == nullptr) {
                        continue;
                    }
                    for (const auto* expr : window->partition_by) {
                        collect_parameter_fields(expr, std::nullopt, sqlda_out, fields_out);
                    }
                    for (const auto* order_item : window->order_by) {
                        if (order_item != nullptr) {
                            collect_parameter_fields(order_item->expr,
                                                     std::nullopt,
                                                     sqlda_out,
                                                     fields_out);
                        }
                    }
                    collect_parameter_fields(window->frame_start_value,
                                             std::nullopt,
                                             sqlda_out,
                                             fields_out);
                    collect_parameter_fields(window->frame_end_value,
                                             std::nullopt,
                                             sqlda_out,
                                             fields_out);
                }
                for (const auto* order_item : select->order_by) {
                    if (order_item != nullptr) {
                        collect_parameter_fields(order_item->expr,
                                                 std::nullopt,
                                                 sqlda_out,
                                                 fields_out);
                    }
                }
                for (const auto* join : select->joins) {
                    if (join != nullptr) {
                        collect_table_ref_parameter_fields(join->right, sqlda_out, fields_out);
                        collect_parameter_fields(join->on_condition,
                                                 std::nullopt,
                                                 sqlda_out,
                                                 fields_out);
                    }
                }
                collect_parameter_fields(select->optimize_for_rows,
                                         std::nullopt,
                                         sqlda_out,
                                         fields_out);
                collect_parameter_fields(select->firebird_plan,
                                         std::nullopt,
                                         sqlda_out,
                                         fields_out);
                if (select->set_op_right != nullptr) {
                    collect_select_parameter_fields(select->set_op_right,
                                                   sqlda_out,
                                                   fields_out);
                }
                if (select->with != nullptr) {
                    for (const auto& cte : select->with->ctes) {
                        collect_statement_parameter_fields(cte.query,
                                                           sqlda_out,
                                                           fields_out);
                    }
                }
            };

        collect_statement_parameter_fields =
            [&](const Statement* statement,
                std::vector<FBSqldaVarDesc>& sqlda_out,
                std::vector<FBMessageFieldDesc>& fields_out) {
                if (statement == nullptr) {
                    return;
                }

                switch (statement->kind()) {
                    case ASTKind::SelectStmt:
                        collect_select_parameter_fields(static_cast<const SelectStmt*>(statement),
                                                        sqlda_out,
                                                        fields_out);
                        return;
                    case ASTKind::InsertStmt:
                    {
                        const auto* insert = static_cast<const InsertStmt*>(statement);
                        if (insert->select_source != nullptr) {
                            collect_select_parameter_fields(insert->select_source,
                                                            sqlda_out,
                                                            fields_out);
                        }
                        for (const auto& row : insert->values_rows) {
                            for (const auto* expr : row) {
                                collect_parameter_fields(expr,
                                                         std::nullopt,
                                                         sqlda_out,
                                                         fields_out);
                            }
                        }
                        if (insert->on_conflict != nullptr) {
                            for (const auto& set_item : insert->on_conflict->set_items) {
                                collect_parameter_fields(set_item.second,
                                                         std::nullopt,
                                                         sqlda_out,
                                                         fields_out);
                            }
                            collect_parameter_fields(insert->on_conflict->where_action,
                                                     std::nullopt,
                                                     sqlda_out,
                                                     fields_out);
                            collect_parameter_fields(insert->on_conflict->where_target,
                                                     std::nullopt,
                                                     sqlda_out,
                                                     fields_out);
                        }
                        collect_parameter_fields(insert->conditional_if,
                                                 std::nullopt,
                                                 sqlda_out,
                                                 fields_out);
                        if (insert->with != nullptr) {
                            for (const auto& cte : insert->with->ctes) {
                                collect_statement_parameter_fields(cte.query,
                                                                   sqlda_out,
                                                                   fields_out);
                            }
                        }
                        return;
                    }
                    case ASTKind::UpdateStmt:
                    {
                        const auto* update = static_cast<const UpdateStmt*>(statement);
                        collect_table_ref_parameter_fields(update->from,
                                                           sqlda_out,
                                                           fields_out);
                        for (const auto& set_item : update->set_items) {
                            collect_parameter_fields(set_item.second,
                                                     std::nullopt,
                                                     sqlda_out,
                                                     fields_out);
                        }
                        collect_parameter_fields(update->where,
                                                 std::nullopt,
                                                 sqlda_out,
                                                 fields_out);
                        collect_parameter_fields(update->conditional_if,
                                                 std::nullopt,
                                                 sqlda_out,
                                                 fields_out);
                        for (const auto* join : update->joins) {
                            if (join != nullptr) {
                                collect_table_ref_parameter_fields(join->right,
                                                                   sqlda_out,
                                                                   fields_out);
                                collect_parameter_fields(join->on_condition,
                                                         std::nullopt,
                                                         sqlda_out,
                                                         fields_out);
                            }
                        }
                        if (update->with != nullptr) {
                            for (const auto& cte : update->with->ctes) {
                                collect_statement_parameter_fields(cte.query,
                                                                   sqlda_out,
                                                                   fields_out);
                            }
                        }
                        return;
                    }
                    case ASTKind::DeleteStmt:
                    {
                        const auto* del = static_cast<const DeleteStmt*>(statement);
                        collect_table_ref_parameter_fields(del->using_clause,
                                                           sqlda_out,
                                                           fields_out);
                        collect_parameter_fields(del->where,
                                                 std::nullopt,
                                                 sqlda_out,
                                                 fields_out);
                        collect_parameter_fields(del->conditional_if,
                                                 std::nullopt,
                                                 sqlda_out,
                                                 fields_out);
                        for (const auto* join : del->using_joins) {
                            if (join != nullptr) {
                                collect_table_ref_parameter_fields(join->right,
                                                                   sqlda_out,
                                                                   fields_out);
                                collect_parameter_fields(join->on_condition,
                                                         std::nullopt,
                                                         sqlda_out,
                                                         fields_out);
                            }
                        }
                        if (del->with != nullptr) {
                            for (const auto& cte : del->with->ctes) {
                                collect_statement_parameter_fields(cte.query,
                                                                   sqlda_out,
                                                                   fields_out);
                            }
                        }
                        return;
                    }
                    case ASTKind::MergeStmt:
                    {
                        const auto* merge = static_cast<const MergeStmt*>(statement);
                        collect_parameter_fields(merge->on_condition,
                                                 std::nullopt,
                                                 sqlda_out,
                                                 fields_out);
                        if (merge->source_query != nullptr) {
                            collect_statement_parameter_fields(merge->source_query,
                                                            sqlda_out,
                                                            fields_out);
                        }
                        for (const auto& clause : merge->when_matched) {
                            collect_parameter_fields(clause.and_condition,
                                                     std::nullopt,
                                                     sqlda_out,
                                                     fields_out);
                            for (const auto& assignment : clause.assignments) {
                                collect_parameter_fields(assignment.second,
                                                         std::nullopt,
                                                         sqlda_out,
                                                         fields_out);
                            }
                        }
                        for (const auto& clause : merge->when_not_matched) {
                            collect_parameter_fields(clause.and_condition,
                                                     std::nullopt,
                                                     sqlda_out,
                                                     fields_out);
                            for (const auto* expr : clause.values) {
                                collect_parameter_fields(expr,
                                                         std::nullopt,
                                                         sqlda_out,
                                                         fields_out);
                            }
                        }
                        for (const auto& clause : merge->when_not_matched_by_source) {
                            collect_parameter_fields(clause.and_condition,
                                                     std::nullopt,
                                                     sqlda_out,
                                                     fields_out);
                            for (const auto& assignment : clause.assignments) {
                                collect_parameter_fields(assignment.second,
                                                         std::nullopt,
                                                         sqlda_out,
                                                         fields_out);
                            }
                        }
                        return;
                    }
                    default:
                        return;
                }
            };

        auto find_target_column =
            [&](const std::vector<LocalSqldaField>& target_columns,
                StringPool::StringId column_id) -> std::optional<LocalSqldaField> {
                const std::string column_name =
                    poolString(metadata_parser.stringPool(), column_id);
                for (const auto& entry : target_columns) {
                    if (identifiersEqual(entry.sqlda.field_name, column_name)) {
                        return entry;
                    }
                }
                return std::nullopt;
            };

        std::vector<FBSqldaVarDesc> input_sqlda;
        std::vector<FBMessageFieldDesc> input_fields;
        std::vector<FBSqldaVarDesc> output_sqlda;
        std::vector<FBMessageFieldDesc> output_fields;

        {
            std::vector<FBSqldaVarDesc> hint_sqlda;
            std::vector<FBMessageFieldDesc> hint_fields;
            switch (metadata_parse.statement->kind()) {
                case ASTKind::SelectStmt:
                {
                    const auto* select =
                        static_cast<const SelectStmt*>(metadata_parse.statement.get());
                    collect_select_parameter_fields(select, hint_sqlda, hint_fields);
                    break;
                }
                case ASTKind::InsertStmt:
                {
                    const auto* insert =
                        static_cast<const InsertStmt*>(metadata_parse.statement.get());
                    const std::vector<LocalSqldaField>* target_columns =
                        source_metadata.empty() ? nullptr : &source_metadata.front().columns;
                    for (const auto& row : insert->values_rows) {
                        for (size_t i = 0; i < row.size(); ++i) {
                            std::optional<LocalSqldaField> hint;
                            if (target_columns != nullptr) {
                                if (!insert->columns.empty() && i < insert->columns.size()) {
                                    hint = find_target_column(*target_columns, insert->columns[i]);
                                }
                                else if (i < target_columns->size()) {
                                    hint = (*target_columns)[i];
                                }
                            }
                            collect_parameter_fields(row[i], hint, hint_sqlda, hint_fields);
                        }
                    }
                    if (insert->select_source != nullptr) {
                        collect_select_parameter_fields(insert->select_source,
                                                        hint_sqlda,
                                                        hint_fields);
                    }
                    if (insert->on_conflict != nullptr) {
                        for (const auto& set_item : insert->on_conflict->set_items) {
                            collect_parameter_fields(set_item.second,
                                                     std::nullopt,
                                                     hint_sqlda,
                                                     hint_fields);
                        }
                        collect_parameter_fields(insert->on_conflict->where_action,
                                                 std::nullopt,
                                                 hint_sqlda,
                                                 hint_fields);
                        collect_parameter_fields(insert->on_conflict->where_target,
                                                 std::nullopt,
                                                 hint_sqlda,
                                                 hint_fields);
                    }
                    collect_parameter_fields(insert->conditional_if,
                                             std::nullopt,
                                             hint_sqlda,
                                             hint_fields);
                    if (insert->with != nullptr) {
                        for (const auto& cte : insert->with->ctes) {
                            collect_statement_parameter_fields(cte.query,
                                                               hint_sqlda,
                                                               hint_fields);
                        }
                    }
                    break;
                }
                case ASTKind::UpdateStmt:
                {
                    const auto* update =
                        static_cast<const UpdateStmt*>(metadata_parse.statement.get());
                    const std::vector<LocalSqldaField>* target_columns =
                        source_metadata.empty() ? nullptr : &source_metadata.front().columns;
                    for (const auto& item : update->set_items) {
                        collect_parameter_fields(item.second,
                                                 target_columns != nullptr
                                                     ? find_target_column(*target_columns,
                                                                          item.first)
                                                     : std::nullopt,
                                                 hint_sqlda,
                                                 hint_fields);
                    }
                    collect_parameter_fields(update->where,
                                             std::nullopt,
                                             hint_sqlda,
                                             hint_fields);
                    collect_parameter_fields(update->conditional_if,
                                             std::nullopt,
                                             hint_sqlda,
                                             hint_fields);
                    collect_table_ref_parameter_fields(update->from,
                                                       hint_sqlda,
                                                       hint_fields);
                    for (const auto* join : update->joins) {
                        if (join != nullptr) {
                            collect_table_ref_parameter_fields(join->right,
                                                               hint_sqlda,
                                                               hint_fields);
                            collect_parameter_fields(join->on_condition,
                                                     std::nullopt,
                                                     hint_sqlda,
                                                     hint_fields);
                        }
                    }
                    if (update->with != nullptr) {
                        for (const auto& cte : update->with->ctes) {
                            collect_statement_parameter_fields(cte.query,
                                                               hint_sqlda,
                                                               hint_fields);
                        }
                    }
                    break;
                }
                case ASTKind::DeleteStmt:
                {
                    const auto* del =
                        static_cast<const DeleteStmt*>(metadata_parse.statement.get());
                    collect_parameter_fields(del->where,
                                             std::nullopt,
                                             hint_sqlda,
                                             hint_fields);
                    collect_parameter_fields(del->conditional_if,
                                             std::nullopt,
                                             hint_sqlda,
                                             hint_fields);
                    collect_table_ref_parameter_fields(del->using_clause,
                                                       hint_sqlda,
                                                       hint_fields);
                    for (const auto* join : del->using_joins) {
                        if (join != nullptr) {
                            collect_table_ref_parameter_fields(join->right,
                                                               hint_sqlda,
                                                               hint_fields);
                            collect_parameter_fields(join->on_condition,
                                                     std::nullopt,
                                                     hint_sqlda,
                                                     hint_fields);
                        }
                    }
                    if (del->with != nullptr) {
                        for (const auto& cte : del->with->ctes) {
                            collect_statement_parameter_fields(cte.query,
                                                               hint_sqlda,
                                                               hint_fields);
                        }
                    }
                    break;
                }
                case ASTKind::MergeStmt:
                {
                    const auto* merge =
                        static_cast<const MergeStmt*>(metadata_parse.statement.get());
                    const std::vector<LocalSqldaField>* target_columns =
                        source_metadata.empty() ? nullptr : &source_metadata.front().columns;
                    collect_parameter_fields(merge->on_condition,
                                             std::nullopt,
                                             hint_sqlda,
                                             hint_fields);
                    if (merge->source_query != nullptr) {
                        collect_statement_parameter_fields(merge->source_query,
                                                        hint_sqlda,
                                                        hint_fields);
                    }
                    for (const auto& clause : merge->when_matched) {
                        collect_parameter_fields(clause.and_condition,
                                                 std::nullopt,
                                                 hint_sqlda,
                                                 hint_fields);
                        for (const auto& assignment : clause.assignments) {
                            collect_parameter_fields(assignment.second,
                                                     target_columns != nullptr
                                                         ? find_target_column(*target_columns,
                                                                              assignment.first)
                                                         : std::nullopt,
                                                     hint_sqlda,
                                                     hint_fields);
                        }
                    }
                    for (const auto& clause : merge->when_not_matched) {
                        collect_parameter_fields(clause.and_condition,
                                                 std::nullopt,
                                                 hint_sqlda,
                                                 hint_fields);
                        for (size_t i = 0; i < clause.values.size(); ++i) {
                            std::optional<LocalSqldaField> hint;
                            if (target_columns != nullptr) {
                                if (!clause.columns.empty() && i < clause.columns.size()) {
                                    hint = find_target_column(*target_columns, clause.columns[i]);
                                }
                                else if (i < target_columns->size()) {
                                    hint = (*target_columns)[i];
                                }
                            }
                            collect_parameter_fields(clause.values[i],
                                                     hint,
                                                     hint_sqlda,
                                                     hint_fields);
                        }
                    }
                    for (const auto& clause : merge->when_not_matched_by_source) {
                        collect_parameter_fields(clause.and_condition,
                                                 std::nullopt,
                                                 hint_sqlda,
                                                 hint_fields);
                        for (const auto& assignment : clause.assignments) {
                            collect_parameter_fields(assignment.second,
                                                     target_columns != nullptr
                                                         ? find_target_column(*target_columns,
                                                                              assignment.first)
                                                         : std::nullopt,
                                                     hint_sqlda,
                                                     hint_fields);
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }

        auto merge_final_output_columns =
            [&](const std::vector<LocalSqldaField>& fallback_columns) {
                const size_t overlap = std::min(output_sqlda.size(), fallback_columns.size());
                for (size_t i = 0; i < overlap; ++i) {
                    if (output_fields.size() >= (i * 2 + 1) &&
                        is_placeholder_field(output_fields[i * 2]) &&
                        !is_placeholder_field(fallback_columns[i].field)) {
                        output_fields[i * 2] = fallback_columns[i].field;
                    }
                    if (output_sqlda[i].field_name.empty()) {
                        output_sqlda[i].field_name = fallback_columns[i].sqlda.field_name;
                    }
                    if (output_sqlda[i].alias_name.empty()) {
                        output_sqlda[i].alias_name = fallback_columns[i].sqlda.alias_name;
                    }
                    if (output_sqlda[i].relation_name.empty()) {
                        output_sqlda[i].relation_name = fallback_columns[i].sqlda.relation_name;
                    }
                    if (output_sqlda[i].relation_schema.empty()) {
                        output_sqlda[i].relation_schema =
                            fallback_columns[i].sqlda.relation_schema;
                    }
                    if (output_sqlda[i].relation_alias.empty()) {
                        output_sqlda[i].relation_alias =
                            fallback_columns[i].sqlda.relation_alias;
                    }
                    if (output_sqlda[i].owner_name.empty()) {
                        output_sqlda[i].owner_name = fallback_columns[i].sqlda.owner_name;
                    }
                }
                for (size_t i = output_sqlda.size(); i < fallback_columns.size(); ++i) {
                    append_output_field(fallback_columns[i],
                                        std::nullopt,
                                        output_sqlda,
                                        output_fields);
                }
            };

        switch (metadata_parse.statement->kind()) {
            case ASTKind::SelectStmt:
            {
                const auto* select = static_cast<const SelectStmt*>(metadata_parse.statement.get());
                for (const auto* item : select->items) {
                    if (item == nullptr) {
                        continue;
                    }
                    if (item->item_type == SelectItem::Type::STAR) {
                        for (const auto& source_meta : source_metadata) {
                            for (const auto& column : source_meta.columns) {
                                append_output_field(column, std::nullopt, output_sqlda, output_fields);
                            }
                        }
                        continue;
                    }
                    if (item->item_type == SelectItem::Type::TABLE_STAR) {
                        const LocalSourceMetadata* source_meta =
                            find_source_metadata(item->table_path);
                        if (source_meta != nullptr) {
                            for (const auto& column : source_meta->columns) {
                                append_output_field(column,
                                                    std::nullopt,
                                                    output_sqlda,
                                                    output_fields);
                            }
                        }
                        continue;
                    }

                    std::optional<LocalSqldaField> inferred = infer_expr_field(item->expr);
                    if (!inferred.has_value()) {
                        const size_t ordinal = output_sqlda.size() + 1;
                        appendDefaultSqldaField(output_fields, output_sqlda);
                        if (item->has_alias) {
                            output_sqlda.back().alias_name =
                                poolString(metadata_parser.stringPool(), item->alias);
                        }
                        if (output_sqlda.back().alias_name.empty()) {
                            output_sqlda.back().alias_name =
                                "EXPR$" + std::to_string(ordinal);
                        }
                        output_sqlda.back().field_name = output_sqlda.back().alias_name;
                        continue;
                    }

                    std::optional<std::string> alias_override;
                    if (item->has_alias) {
                        alias_override =
                            poolString(metadata_parser.stringPool(), item->alias);
                    }
                    append_output_field(*inferred, alias_override, output_sqlda, output_fields);
                }

                collect_select_parameter_fields(select, input_sqlda, input_fields);
                if (select->set_op_right != nullptr) {
                    std::vector<LocalSqldaField> right_columns;
                    std::string right_error;
                    if (!synthesize_statement_output_columns(select->set_op_right,
                                                             root_cte_scope,
                                                             right_columns,
                                                             right_error)) {
                        return sendErrorResponse(state, right_error);
                    }
                    merge_final_output_columns(right_columns);
                }
                break;
            }
            case ASTKind::InsertStmt:
            {
                const auto* insert = static_cast<const InsertStmt*>(metadata_parse.statement.get());
                const std::vector<LocalSqldaField>* target_columns =
                    source_metadata.empty() ? nullptr : &source_metadata.front().columns;
                for (const auto* item : insert->returning) {
                    if (item == nullptr) {
                        continue;
                    }
                    if (item->item_type == SelectItem::Type::STAR && target_columns != nullptr) {
                        for (const auto& column : *target_columns) {
                            append_output_field(column, std::nullopt, output_sqlda, output_fields);
                        }
                        continue;
                    }
                    if (item->item_type == SelectItem::Type::TABLE_STAR) {
                        const LocalSourceMetadata* source_meta =
                            find_source_metadata(item->table_path);
                        if (source_meta != nullptr) {
                            for (const auto& column : source_meta->columns) {
                                append_output_field(column,
                                                    std::nullopt,
                                                    output_sqlda,
                                                    output_fields);
                            }
                        }
                        continue;
                    }
                    std::optional<LocalSqldaField> inferred = infer_expr_field(item->expr);
                    if (inferred.has_value()) {
                        std::optional<std::string> alias_override;
                        if (item->has_alias) {
                            alias_override =
                                poolString(metadata_parser.stringPool(), item->alias);
                        }
                        append_output_field(*inferred, alias_override, output_sqlda, output_fields);
                    }
                    else {
                        const size_t ordinal = output_sqlda.size() + 1;
                        appendDefaultSqldaField(output_fields, output_sqlda);
                        if (item->has_alias) {
                            output_sqlda.back().alias_name =
                                poolString(metadata_parser.stringPool(), item->alias);
                        }
                        if (output_sqlda.back().alias_name.empty()) {
                            output_sqlda.back().alias_name =
                                "EXPR$" + std::to_string(ordinal);
                        }
                        output_sqlda.back().field_name = output_sqlda.back().alias_name;
                    }
                }

                for (const auto& row : insert->values_rows) {
                    for (size_t i = 0; i < row.size(); ++i) {
                        std::optional<LocalSqldaField> hint;
                        if (target_columns != nullptr) {
                            if (!insert->columns.empty() && i < insert->columns.size()) {
                                hint = find_target_column(*target_columns, insert->columns[i]);
                            }
                            else if (i < target_columns->size()) {
                                hint = (*target_columns)[i];
                            }
                        }
                        collect_parameter_fields(row[i], hint, input_sqlda, input_fields);
                    }
                }
                if (insert->select_source != nullptr) {
                    collect_select_parameter_fields(insert->select_source,
                                                    input_sqlda,
                                                    input_fields);
                }
                if (insert->on_conflict != nullptr) {
                    for (const auto& set_item : insert->on_conflict->set_items) {
                        collect_parameter_fields(set_item.second,
                                                 std::nullopt,
                                                 input_sqlda,
                                                 input_fields);
                    }
                    collect_parameter_fields(insert->on_conflict->where_action,
                                             std::nullopt,
                                             input_sqlda,
                                             input_fields);
                    collect_parameter_fields(insert->on_conflict->where_target,
                                             std::nullopt,
                                             input_sqlda,
                                             input_fields);
                }
                collect_parameter_fields(insert->conditional_if,
                                         std::nullopt,
                                         input_sqlda,
                                         input_fields);
                if (insert->with != nullptr) {
                    for (const auto& cte : insert->with->ctes) {
                        collect_statement_parameter_fields(cte.query,
                                                           input_sqlda,
                                                           input_fields);
                    }
                }
                break;
            }
            case ASTKind::UpdateStmt:
            {
                const auto* update = static_cast<const UpdateStmt*>(metadata_parse.statement.get());
                const std::vector<LocalSqldaField>* target_columns =
                    source_metadata.empty() ? nullptr : &source_metadata.front().columns;
                for (const auto* item : update->returning) {
                    if (item == nullptr) {
                        continue;
                    }
                    if (item->item_type == SelectItem::Type::STAR && target_columns != nullptr) {
                        for (const auto& column : *target_columns) {
                            append_output_field(column, std::nullopt, output_sqlda, output_fields);
                        }
                        continue;
                    }
                    if (item->item_type == SelectItem::Type::TABLE_STAR) {
                        const LocalSourceMetadata* source_meta =
                            find_source_metadata(item->table_path);
                        if (source_meta != nullptr) {
                            for (const auto& column : source_meta->columns) {
                                append_output_field(column,
                                                    std::nullopt,
                                                    output_sqlda,
                                                    output_fields);
                            }
                        }
                        continue;
                    }
                    if (auto inferred = infer_expr_field(item->expr); inferred.has_value()) {
                        std::optional<std::string> alias_override;
                        if (item->has_alias) {
                            alias_override =
                                poolString(metadata_parser.stringPool(), item->alias);
                        }
                        append_output_field(*inferred, alias_override, output_sqlda, output_fields);
                    }
                    else {
                        const size_t ordinal = output_sqlda.size() + 1;
                        appendDefaultSqldaField(output_fields, output_sqlda);
                        if (item->has_alias) {
                            output_sqlda.back().alias_name =
                                poolString(metadata_parser.stringPool(), item->alias);
                        }
                        if (output_sqlda.back().alias_name.empty()) {
                            output_sqlda.back().alias_name =
                                "EXPR$" + std::to_string(ordinal);
                        }
                        output_sqlda.back().field_name = output_sqlda.back().alias_name;
                    }
                }
                for (const auto& item : update->set_items) {
                    collect_parameter_fields(item.second,
                                             target_columns != nullptr
                                                 ? find_target_column(*target_columns, item.first)
                                                 : std::nullopt,
                                             input_sqlda,
                                             input_fields);
                }
                collect_parameter_fields(update->where, std::nullopt, input_sqlda, input_fields);
                collect_parameter_fields(update->conditional_if,
                                         std::nullopt,
                                         input_sqlda,
                                         input_fields);
                collect_table_ref_parameter_fields(update->from, input_sqlda, input_fields);
                for (const auto* join : update->joins) {
                    if (join != nullptr) {
                        collect_table_ref_parameter_fields(join->right,
                                                           input_sqlda,
                                                           input_fields);
                        collect_parameter_fields(join->on_condition,
                                                 std::nullopt,
                                                 input_sqlda,
                                                 input_fields);
                    }
                }
                if (update->with != nullptr) {
                    for (const auto& cte : update->with->ctes) {
                        collect_statement_parameter_fields(cte.query,
                                                           input_sqlda,
                                                           input_fields);
                    }
                }
                break;
            }
            case ASTKind::DeleteStmt:
            {
                const auto* del = static_cast<const DeleteStmt*>(metadata_parse.statement.get());
                const std::vector<LocalSqldaField>* target_columns =
                    source_metadata.empty() ? nullptr : &source_metadata.front().columns;
                for (const auto* item : del->returning) {
                    if (item == nullptr) {
                        continue;
                    }
                    if (item->item_type == SelectItem::Type::STAR && target_columns != nullptr) {
                        for (const auto& column : *target_columns) {
                            append_output_field(column, std::nullopt, output_sqlda, output_fields);
                        }
                        continue;
                    }
                    if (item->item_type == SelectItem::Type::TABLE_STAR) {
                        const LocalSourceMetadata* source_meta =
                            find_source_metadata(item->table_path);
                        if (source_meta != nullptr) {
                            for (const auto& column : source_meta->columns) {
                                append_output_field(column,
                                                    std::nullopt,
                                                    output_sqlda,
                                                    output_fields);
                            }
                        }
                        continue;
                    }
                    if (auto inferred = infer_expr_field(item->expr); inferred.has_value()) {
                        std::optional<std::string> alias_override;
                        if (item->has_alias) {
                            alias_override =
                                poolString(metadata_parser.stringPool(), item->alias);
                        }
                        append_output_field(*inferred, alias_override, output_sqlda, output_fields);
                    }
                    else {
                        const size_t ordinal = output_sqlda.size() + 1;
                        appendDefaultSqldaField(output_fields, output_sqlda);
                        if (item->has_alias) {
                            output_sqlda.back().alias_name =
                                poolString(metadata_parser.stringPool(), item->alias);
                        }
                        if (output_sqlda.back().alias_name.empty()) {
                            output_sqlda.back().alias_name =
                                "EXPR$" + std::to_string(ordinal);
                        }
                        output_sqlda.back().field_name = output_sqlda.back().alias_name;
                    }
                }
                collect_parameter_fields(del->where, std::nullopt, input_sqlda, input_fields);
                collect_parameter_fields(del->conditional_if,
                                         std::nullopt,
                                         input_sqlda,
                                         input_fields);
                collect_table_ref_parameter_fields(del->using_clause,
                                                   input_sqlda,
                                                   input_fields);
                for (const auto* join : del->using_joins) {
                    if (join != nullptr) {
                        collect_table_ref_parameter_fields(join->right,
                                                           input_sqlda,
                                                           input_fields);
                        collect_parameter_fields(join->on_condition,
                                                 std::nullopt,
                                                 input_sqlda,
                                                 input_fields);
                    }
                }
                if (del->with != nullptr) {
                    for (const auto& cte : del->with->ctes) {
                        collect_statement_parameter_fields(cte.query,
                                                           input_sqlda,
                                                           input_fields);
                    }
                }
                break;
            }
            case ASTKind::MergeStmt:
            {
                const auto* merge = static_cast<const MergeStmt*>(metadata_parse.statement.get());
                const std::vector<LocalSqldaField>* target_columns =
                    source_metadata.empty() ? nullptr : &source_metadata.front().columns;
                collect_parameter_fields(merge->on_condition, std::nullopt, input_sqlda, input_fields);
                if (merge->source_query != nullptr) {
                    collect_statement_parameter_fields(merge->source_query,
                                                    input_sqlda,
                                                    input_fields);
                }
                for (const auto& clause : merge->when_matched) {
                    collect_parameter_fields(clause.and_condition,
                                             std::nullopt,
                                             input_sqlda,
                                             input_fields);
                    for (const auto& assignment : clause.assignments) {
                        collect_parameter_fields(assignment.second,
                                                 target_columns != nullptr
                                                     ? find_target_column(*target_columns,
                                                                          assignment.first)
                                                     : std::nullopt,
                                                 input_sqlda,
                                                 input_fields);
                    }
                }
                for (const auto& clause : merge->when_not_matched) {
                    collect_parameter_fields(clause.and_condition,
                                             std::nullopt,
                                             input_sqlda,
                                             input_fields);
                    for (size_t i = 0; i < clause.values.size(); ++i) {
                        std::optional<LocalSqldaField> hint;
                        if (target_columns != nullptr) {
                            if (!clause.columns.empty() && i < clause.columns.size()) {
                                hint = find_target_column(*target_columns, clause.columns[i]);
                            }
                            else if (i < target_columns->size()) {
                                hint = (*target_columns)[i];
                            }
                        }
                        collect_parameter_fields(clause.values[i],
                                                 hint,
                                                 input_sqlda,
                                                 input_fields);
                    }
                }
                for (const auto& clause : merge->when_not_matched_by_source) {
                    collect_parameter_fields(clause.and_condition,
                                             std::nullopt,
                                             input_sqlda,
                                             input_fields);
                    for (const auto& assignment : clause.assignments) {
                        collect_parameter_fields(assignment.second,
                                                 target_columns != nullptr
                                                     ? find_target_column(*target_columns,
                                                                          assignment.first)
                                                     : std::nullopt,
                                                 input_sqlda,
                                                 input_fields);
                    }
                }
                break;
            }
            default:
                break;
        }

        stmt_it->second.input_sqlda_fields = std::move(input_sqlda);
        stmt_it->second.output_sqlda_fields = std::move(output_sqlda);
        if (!input_fields.empty()) {
            stmt_it->second.input_message_fields[0] = std::move(input_fields);
        }
        if (!output_fields.empty()) {
            stmt_it->second.output_message_fields[0] = std::move(output_fields);
        }
    }

    std::vector<uint8_t> info_buffer;
    buildFirebirdStatementInfoBuffer(stmt_it->second, items, buffer_length, info_buffer);
    sendResponse(state,
                 0,
                 0,
                 info_buffer.empty() ? nullptr : info_buffer.data(),
                 info_buffer.size(),
                 ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleExecImmediate(FBClientState& state,
                                                     const std::vector<uint8_t>& packet,
                                                     bool want_sql_response,
                                                     core::ErrorContext* ctx) {
    if (packet.size() < 20) {
        return sendErrorResponse(state, "Malformed Firebird op_exec_immediate packet");
    }

    size_t offset = 4;
    std::vector<std::optional<std::string>> bound_params;
    std::vector<bool> bound_param_nulls;
    std::unordered_map<uint8_t, std::vector<FBMessageFieldDesc>> input_message_fields;
    std::unordered_map<uint8_t, std::vector<FBMessageFieldDesc>> output_message_fields;
    uint32_t out_message_number = 0;
    std::string decode_error;

    if (want_sql_response) {
        std::string input_blr;
        if (!consumeXdrString(packet, offset, input_blr, decode_error)) {
            return sendErrorResponse(state, decode_error);
        }
        if (offset + 8 > packet.size()) {
            return sendErrorResponse(state, "Malformed Firebird exec_immediate2 input header");
        }
        const uint32_t input_message_number = xdrReadUint32(packet.data() + offset);
        offset += 4;
        const uint32_t input_message_count = xdrReadUint32(packet.data() + offset);
        offset += 4;
        if (input_message_count > 1) {
            return sendErrorResponse(state, "Invalid Firebird exec_immediate2 input message count");
        }
        if (!input_blr.empty() && input_message_count != 0) {
            if (!decodeFirebirdMessageOnlyLayout(std::vector<uint8_t>(input_blr.begin(), input_blr.end()),
                                                 input_message_fields,
                                                 decode_error)) {
                return sendErrorResponse(state, decode_error);
            }
            auto layout_it = input_message_fields.find(static_cast<uint8_t>(input_message_number));
            if (layout_it == input_message_fields.end()) {
                return sendErrorResponse(state, "Unknown Firebird exec_immediate2 input layout");
            }
            if (!decodeFirebirdSqlMessage(packet,
                                          offset,
                                          layout_it->second,
                                          state.protocol_version >= fb::PROTOCOL_VERSION13,
                                          bound_params,
                                          bound_param_nulls,
                                          decode_error)) {
                return sendErrorResponse(state, decode_error);
            }
        }

        std::string output_blr;
        if (!consumeXdrString(packet, offset, output_blr, decode_error)) {
            return sendErrorResponse(state, decode_error);
        }
        if (offset + 4 > packet.size()) {
            return sendErrorResponse(state, "Malformed Firebird exec_immediate2 output header");
        }
        out_message_number = xdrReadUint32(packet.data() + offset);
        offset += 4;
        if (!output_blr.empty()) {
            if (!decodeFirebirdMessageOnlyLayout(std::vector<uint8_t>(output_blr.begin(), output_blr.end()),
                                                 output_message_fields,
                                                 decode_error)) {
                return sendErrorResponse(state, decode_error);
            }
        }
        if (offset + 4 <= packet.size()) {
            offset += 4; // inline blob size
        }
    }

    if (offset + 12 > packet.size()) {
        return sendErrorResponse(state, "Malformed Firebird exec_immediate packet tail");
    }

    uint32_t transaction_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    const uint32_t statement_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    (void)statement_handle;
    const uint32_t sql_dialect = xdrReadUint32(packet.data() + offset);
    offset += 4;
    (void)sql_dialect;

    if (transaction_handle != 0) {
        if (state.transactions.find(transaction_handle) == state.transactions.end()) {
            return sendErrorResponse(state, "Unknown Firebird transaction handle");
        }
    }
    else {
        FBTransactionState txn_state;
        txn_state.transaction_id = generateHandle();
        txn_state.oldest_interesting = txn_state.transaction_id;
        txn_state.oldest_snapshot = txn_state.transaction_id;
        txn_state.oldest_active = txn_state.transaction_id;
        txn_state.snapshot_number = txn_state.transaction_id;
        txn_state.prepared = false;
        txn_state.prepare_description.clear();
        txn_state.database_path = state.database;
        transaction_handle = txn_state.transaction_id;
        state.transactions[transaction_handle] = std::move(txn_state);
    }

    std::string sql_text;
    if (!consumeXdrString(packet, offset, sql_text, decode_error)) {
        return sendErrorResponse(state, decode_error);
    }
    std::string items;
    if (!consumeXdrString(packet, offset, items, decode_error)) {
        return sendErrorResponse(state, decode_error);
    }
    if (offset + 4 > packet.size()) {
        return sendErrorResponse(state, "Malformed Firebird exec_immediate info buffer length");
    }
    offset += 4; // info buffer length
    if (offset + 4 <= packet.size()) {
        offset += 4; // flags, when present
    }

    const auto sql_batch = splitFirebirdSqlBatch(sql_text);
    if (sql_batch.size() > 1) {
        if (want_sql_response) {
            return sendErrorResponse(
                state,
                "Firebird exec_immediate2 batch execution is not supported with output messages");
        }
        if (!bound_params.empty()) {
            return sendErrorResponse(
                state,
                "Firebird batch execution with input parameters is not supported");
        }

        for (const std::string& statement_sql : sql_batch) {
            const std::string effective_statement_sql =
                rewriteFirebirdSingleRowCompatibilityQuery(statement_sql);
            sblr::DialectCompilerRequest batch_request{};
            batch_request.request_id = core::generateUuidV7();
            populateFirebirdCompilerRequest(batch_request, state);
            batch_request.payload_format = sblr::DialectCompilerPayloadFormat::SQL_TEXT;
            batch_request.payload.assign(effective_statement_sql.begin(),
                                         effective_statement_sql.end());

            sblr::DialectCompilerResponse batch_response{};
            core::ErrorContext batch_compile_ctx;
            auto batch_status = sblr::compileFirebirdDialectToSblr(nullptr,
                                                                   batch_request,
                                                                   batch_response,
                                                                   &batch_compile_ctx);
            if (batch_status != core::Status::OK || !batch_response.success) {
                std::string message = !batch_response.errors.empty()
                                        ? batch_response.errors.front()
                                        : (batch_compile_ctx.message.empty()
                                             ? "Firebird SQL to SBLR lowering failed"
                                             : batch_compile_ctx.message);
                std::cerr << "[parser_debug] firebird exec_immediate batch compile failed sql="
                          << effective_statement_sql << " message=" << message << "\n";
                return sendErrorResponse(state, message);
            }

            const uint32_t temp_handle = generateHandle();
            const std::string stmt_name = "fb_exec_immediate_" + std::to_string(temp_handle);
            auto parse_status = sendCompiledParseToEngine(state.client_id,
                                                          state.session_id != 0 ? state.session_id : temp_handle,
                                                          stmt_name,
                                                          batch_response.bytecode,
                                                          effective_statement_sql,
                                                          ctx);
            if (parse_status != core::Status::OK) {
                return sendErrorResponse(state,
                                         "Failed to submit Firebird exec_immediate batch statement to engine");
            }

            IPCMessage engine_response;
            while (true) {
                parse_status = receiveFromEngine(state.client_id, engine_response, ctx, 30000);
                if (parse_status != core::Status::OK) {
                    return sendErrorResponse(state,
                                             "No engine response for Firebird exec_immediate batch parse");
                }
                if (engine_response.getType() == IPCMessageType::ERROR_RESPONSE) {
                    return sendErrorResponse(state, engineErrorMessage(engine_response));
                }
                if (engine_response.getType() == IPCMessageType::PARSE_COMPLETE) {
                    break;
                }
                continue;
            }

            IPCMessage bind_msg(IPCMessageType::BIND, 0);
            bind_msg.header.request_id = state.session_id != 0 ? state.session_id : temp_handle;
            IPCBindPayload bind_payload{};
            const std::string portal_name = firebirdDsqlPortalName(temp_handle);
            std::strncpy(bind_payload.portal_name,
                         portal_name.c_str(),
                         sizeof(bind_payload.portal_name) - 1);
            bind_payload.portal_name[sizeof(bind_payload.portal_name) - 1] = '\0';
            std::strncpy(bind_payload.stmt_name, stmt_name.c_str(), sizeof(bind_payload.stmt_name) - 1);
            bind_payload.stmt_name[sizeof(bind_payload.stmt_name) - 1] = '\0';
            bind_payload.num_params = 0;
            bind_msg.payload.resize(sizeof(bind_payload));
            std::memcpy(bind_msg.payload.data(), &bind_payload, sizeof(bind_payload));

            parse_status = sendToEngine(state.client_id, bind_msg, ctx);
            if (parse_status != core::Status::OK) {
                return sendErrorResponse(state,
                                         "Failed to bind Firebird exec_immediate batch statement");
            }

            while (true) {
                parse_status = receiveFromEngine(state.client_id, engine_response, ctx, 30000);
                if (parse_status != core::Status::OK) {
                    return sendErrorResponse(state,
                                             "No engine response for Firebird exec_immediate batch bind");
                }
                if (engine_response.getType() == IPCMessageType::ERROR_RESPONSE) {
                    return sendErrorResponse(state, engineErrorMessage(engine_response));
                }
                if (engine_response.getType() == IPCMessageType::BIND_COMPLETE) {
                    break;
                }
                continue;
            }

            IPCMessage execute_msg(IPCMessageType::EXECUTE, 0);
            execute_msg.header.request_id = state.session_id != 0 ? state.session_id : temp_handle;
            IPCExecutePayload execute_payload{};
            std::strncpy(execute_payload.portal_name,
                         portal_name.c_str(),
                         sizeof(execute_payload.portal_name) - 1);
            execute_payload.portal_name[sizeof(execute_payload.portal_name) - 1] = '\0';
            execute_payload.max_rows = 0;
            execute_msg.payload.resize(sizeof(execute_payload));
            std::memcpy(execute_msg.payload.data(), &execute_payload, sizeof(execute_payload));

            parse_status = sendToEngine(state.client_id, execute_msg, ctx);
            if (parse_status != core::Status::OK) {
                return sendErrorResponse(state,
                                         "Failed to execute Firebird exec_immediate batch statement");
            }

            bool saw_complete = false;
            while (!saw_complete) {
                parse_status = receiveFromEngine(state.client_id, engine_response, ctx, 30000);
                if (parse_status != core::Status::OK) {
                    return sendErrorResponse(state,
                                             "No engine response for Firebird exec_immediate batch execute");
                }
                switch (engine_response.getType()) {
                    case IPCMessageType::COMMAND_COMPLETE:
                        saw_complete = true;
                        break;
                    case IPCMessageType::ROW_DESCRIPTION:
                    case IPCMessageType::DATA_ROW:
                    case IPCMessageType::BIND_COMPLETE:
                    case IPCMessageType::PARSE_COMPLETE:
                    case IPCMessageType::EMPTY_RESPONSE:
                    case IPCMessageType::READY:
                    case IPCMessageType::READY_FOR_QUERY:
                    case IPCMessageType::NOTICE:
                        break;
                    case IPCMessageType::ERROR_RESPONSE:
                        return sendErrorResponse(state, engineErrorMessage(engine_response));
                    default:
                        return sendErrorResponse(
                            state,
                            "Unexpected engine response during Firebird exec_immediate batch execute");
                }
            }

            auto close_status = closeEngineObject(state, temp_handle, 'P', portal_name, ctx);
            if (close_status != core::Status::OK) {
                return close_status;
            }
            close_status = closeEngineObject(state, temp_handle, 'S', stmt_name, ctx);
            if (close_status != core::Status::OK) {
                return close_status;
            }
        }

        sendResponse(state, transaction_handle, 0, nullptr, 0, ctx);
        return core::Status::OK;
    }

    const std::string effective_sql = rewriteFirebirdSingleRowCompatibilityQuery(sql_text);

    sblr::DialectCompilerRequest request{};
    request.request_id = core::generateUuidV7();
    populateFirebirdCompilerRequest(request, state);
    request.payload_format = sblr::DialectCompilerPayloadFormat::SQL_TEXT;
    request.payload.assign(effective_sql.begin(), effective_sql.end());

    sblr::DialectCompilerResponse response{};
    core::ErrorContext compile_ctx;
    auto status = sblr::compileFirebirdDialectToSblr(nullptr, request, response, &compile_ctx);
    if (status != core::Status::OK || !response.success) {
        std::string message = !response.errors.empty()
                                ? response.errors.front()
                                : (compile_ctx.message.empty()
                                     ? "Firebird SQL to SBLR lowering failed"
                                     : compile_ctx.message);
        std::cerr << "[parser_debug] firebird exec_immediate compile failed sql=" << effective_sql
                  << " message=" << message << "\n";
        return sendErrorResponse(state, message);
    }

    const uint32_t temp_handle = generateHandle();
    const std::string stmt_name = "fb_exec_immediate_" + std::to_string(temp_handle);
    status = sendCompiledParseToEngine(state.client_id,
                                       state.session_id != 0 ? state.session_id : temp_handle,
                                       stmt_name,
                                       response.bytecode,
                                       effective_sql,
                                       ctx);
    if (status != core::Status::OK) {
        return sendErrorResponse(state, "Failed to submit Firebird exec_immediate statement to engine");
    }

    IPCMessage engine_response;
    while (true) {
        status = receiveFromEngine(state.client_id, engine_response, ctx, 30000);
        if (status != core::Status::OK) {
            return sendErrorResponse(state, "No engine response for Firebird exec_immediate parse");
        }
        if (engine_response.getType() == IPCMessageType::ERROR_RESPONSE) {
            return sendErrorResponse(state, engineErrorMessage(engine_response));
        }
        if (engine_response.getType() == IPCMessageType::PARSE_COMPLETE) {
            break;
        }
        continue;
    }

    IPCMessage bind_msg(IPCMessageType::BIND, 0);
    bind_msg.header.request_id = state.session_id != 0 ? state.session_id : temp_handle;
    IPCBindPayload bind_payload{};
    const std::string portal_name = firebirdDsqlPortalName(temp_handle);
    std::strncpy(bind_payload.portal_name, portal_name.c_str(), sizeof(bind_payload.portal_name) - 1);
    bind_payload.portal_name[sizeof(bind_payload.portal_name) - 1] = '\0';
    std::strncpy(bind_payload.stmt_name, stmt_name.c_str(), sizeof(bind_payload.stmt_name) - 1);
    bind_payload.stmt_name[sizeof(bind_payload.stmt_name) - 1] = '\0';
    bind_payload.num_params = static_cast<uint16_t>(bound_params.size());

    size_t bind_payload_size = sizeof(bind_payload);
    for (size_t i = 0; i < bound_params.size(); ++i) {
        bind_payload_size += sizeof(IPCParamValue);
        if (!bound_param_nulls[i] && bound_params[i].has_value()) {
            bind_payload_size += bound_params[i]->size();
        }
    }
    bind_msg.payload.resize(bind_payload_size);
    std::memcpy(bind_msg.payload.data(), &bind_payload, sizeof(bind_payload));
    size_t bind_offset = sizeof(bind_payload);
    for (size_t i = 0; i < bound_params.size(); ++i) {
        IPCParamValue param{};
        param.type_oid = 0;
        param.format = 0;
        if (bound_param_nulls[i] || !bound_params[i].has_value()) {
            param.length = -1;
        }
        else {
            param.length = static_cast<int32_t>(bound_params[i]->size());
        }
        std::memcpy(bind_msg.payload.data() + bind_offset, &param, sizeof(param));
        bind_offset += sizeof(param);
        if (param.length > 0) {
            std::memcpy(bind_msg.payload.data() + bind_offset,
                        bound_params[i]->data(),
                        static_cast<size_t>(param.length));
            bind_offset += static_cast<size_t>(param.length);
        }
    }

    status = sendToEngine(state.client_id, bind_msg, ctx);
    if (status != core::Status::OK) {
        return sendErrorResponse(state, "Failed to bind Firebird exec_immediate statement");
    }
    while (true) {
        status = receiveFromEngine(state.client_id, engine_response, ctx, 30000);
        if (status != core::Status::OK) {
            return sendErrorResponse(state, "No engine response for Firebird exec_immediate bind");
        }
        if (engine_response.getType() == IPCMessageType::ERROR_RESPONSE) {
            return sendErrorResponse(state, engineErrorMessage(engine_response));
        }
        if (engine_response.getType() == IPCMessageType::BIND_COMPLETE) {
            break;
        }
        continue;
    }

    IPCMessage execute_msg(IPCMessageType::EXECUTE, 0);
    execute_msg.header.request_id = state.session_id != 0 ? state.session_id : temp_handle;
    IPCExecutePayload execute_payload{};
    std::strncpy(execute_payload.portal_name, portal_name.c_str(), sizeof(execute_payload.portal_name) - 1);
    execute_payload.portal_name[sizeof(execute_payload.portal_name) - 1] = '\0';
    execute_payload.max_rows = 0;
    execute_msg.payload.resize(sizeof(execute_payload));
    std::memcpy(execute_msg.payload.data(), &execute_payload, sizeof(execute_payload));

    status = sendToEngine(state.client_id, execute_msg, ctx);
    if (status != core::Status::OK) {
        return sendErrorResponse(state, "Failed to execute Firebird exec_immediate statement");
    }

    std::deque<std::vector<std::optional<std::string>>> pending_rows;
    std::vector<std::string> output_field_names;
    const uint32_t statement_type = inferFirebirdStatementType(effective_sql);
    bool saw_complete = false;
    while (!saw_complete) {
        status = receiveFromEngine(state.client_id, engine_response, ctx, 30000);
        if (status != core::Status::OK) {
            return sendErrorResponse(state, "No engine response for Firebird exec_immediate execute");
        }
        switch (engine_response.getType()) {
            case IPCMessageType::ROW_DESCRIPTION:
            {
                std::string desc_error;
                if (!decodeIpcRowDescription(engine_response, output_field_names, desc_error)) {
                    return sendErrorResponse(state,
                                             desc_error.empty()
                                                 ? "Malformed engine ROW_DESCRIPTION payload for Firebird DSQL"
                                                 : desc_error);
                }
                break;
            }
            case IPCMessageType::DATA_ROW:
            {
                std::vector<std::optional<std::string>> row;
                std::string row_error;
                if (!decodeIpcDataRow(engine_response, row, row_error)) {
                    return sendErrorResponse(state,
                                             row_error.empty()
                                                 ? "Malformed engine DATA_ROW payload for Firebird exec_immediate"
                                                 : row_error);
                }
                pending_rows.push_back(std::move(row));
                break;
            }
            case IPCMessageType::COMMAND_COMPLETE:
                saw_complete = true;
                break;
            case IPCMessageType::BIND_COMPLETE:
            case IPCMessageType::PARSE_COMPLETE:
            case IPCMessageType::EMPTY_RESPONSE:
            case IPCMessageType::READY:
            case IPCMessageType::READY_FOR_QUERY:
            case IPCMessageType::NOTICE:
                break;
            case IPCMessageType::ERROR_RESPONSE:
                return sendErrorResponse(state, engineErrorMessage(engine_response));
            default:
                return sendErrorResponse(
                    state,
                    "Unexpected engine response during Firebird exec_immediate execute: " +
                        std::string(ipcMessageTypeToString(engine_response.getType())) +
                        " (" +
                        std::to_string(static_cast<uint32_t>(engine_response.getType())) +
                        ")");
        }
    }

    if (statement_type == 5) {
        state.pending_catalog_refresh = true;
    }
    else if (statement_type == 10 && state.pending_catalog_refresh) {
        auto refresh_status = refreshCommittedCatalogState(state, ctx);
        if (refresh_status != core::Status::OK) {
            return sendErrorResponse(state,
                                     ctx && !ctx->message.empty()
                                         ? ctx->message
                                         : "Failed to refresh Firebird catalog after commit");
        }
        state.pending_catalog_refresh = false;
    }
    else if (statement_type == 11) {
        state.pending_catalog_refresh = false;
    }

    if (want_sql_response) {
        auto layout_it = output_message_fields.find(static_cast<uint8_t>(out_message_number));
        std::optional<std::vector<std::optional<std::string>>> row;
        if (layout_it != output_message_fields.end() && !pending_rows.empty()) {
            row = std::move(pending_rows.front());
        }
        std::vector<uint8_t> response_packet;
        std::string encode_error;
        if (!appendFirebirdSqlResponsePacket(response_packet,
                                             row,
                                             layout_it != output_message_fields.end() ? layout_it->second
                                                                               : std::vector<FBMessageFieldDesc>{},
                                             state.protocol_version >= fb::PROTOCOL_VERSION13,
                                             encode_error)) {
            return sendErrorResponse(state,
                                     encode_error.empty()
                                         ? "Firebird exec_immediate SQL response encoding failed"
                                         : encode_error);
        }
        status = closeEngineObject(state, temp_handle, 'P', firebirdDsqlPortalName(temp_handle), ctx);
        if (status != core::Status::OK) {
            return status;
        }
        status = closeEngineObject(state, temp_handle, 'S', stmt_name, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        status = sendPacket(state, response_packet, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        sendResponse(state, transaction_handle, 0, nullptr, 0, ctx);
        return core::Status::OK;
    }

    status = closeEngineObject(state, temp_handle, 'P', firebirdDsqlPortalName(temp_handle), ctx);
    if (status != core::Status::OK) {
        return status;
    }
    status = closeEngineObject(state, temp_handle, 'S', stmt_name, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    sendResponse(state, transaction_handle, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleExecuteStatement(FBClientState& state,
                                                        const std::vector<uint8_t>& packet,
                                                        bool want_sql_response,
                                                        core::ErrorContext* ctx) {
    if (packet.size() < 20) {
        return sendErrorResponse(state, "Malformed Firebird DSQL execute packet");
    }

    size_t offset = 4;
    const uint32_t statement_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    uint32_t transaction_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    if (transaction_handle != 0) {
        if (state.transactions.find(transaction_handle) == state.transactions.end()) {
            return sendErrorResponse(state, "Unknown Firebird transaction handle");
        }
    }
    else {
        FBTransactionState txn_state;
        txn_state.transaction_id = generateHandle();
        txn_state.oldest_interesting = txn_state.transaction_id;
        txn_state.oldest_snapshot = txn_state.transaction_id;
        txn_state.oldest_active = txn_state.transaction_id;
        txn_state.snapshot_number = txn_state.transaction_id;
        txn_state.prepared = false;
        txn_state.prepare_description.clear();
        txn_state.database_path = state.database;
        transaction_handle = txn_state.transaction_id;
        state.transactions[transaction_handle] = std::move(txn_state);
    }

    std::string input_blr;
    std::string decode_error;
    if (!consumeXdrString(packet, offset, input_blr, decode_error)) {
        return sendErrorResponse(state, decode_error);
    }
    if (offset + 8 > packet.size()) {
        return sendErrorResponse(state, "Malformed Firebird DSQL execute message header");
    }
    const uint32_t message_number = xdrReadUint32(packet.data() + offset);
    offset += 4;
    const uint32_t message_count = xdrReadUint32(packet.data() + offset);
    offset += 4;

    auto stmt_it = state.dsql_statements.find(statement_handle);
    if (stmt_it == state.dsql_statements.end()) {
        return sendErrorResponse(state, "Unknown Firebird DSQL statement handle");
    }
    if (!stmt_it->second.statement_prepared || stmt_it->second.stmt_name.empty()) {
        return sendErrorResponse(state, "Firebird DSQL statement is not prepared");
    }
    if (!stmt_it->second.engine_statement_prepared) {
        if (stmt_it->second.compiled_bytecode.empty()) {
            return sendErrorResponse(state, "Firebird DSQL statement has no compiled bytecode");
        }
        auto parse_status = sendCompiledParseToEngine(state.client_id,
                                                      state.session_id != 0 ? state.session_id : statement_handle,
                                                      stmt_it->second.stmt_name,
                                                      stmt_it->second.compiled_bytecode,
                                                      stmt_it->second.sql_text,
                                                      ctx);
        if (parse_status != core::Status::OK) {
            return sendErrorResponse(state, "Failed to submit compiled Firebird DSQL statement to engine");
        }

        IPCMessage parse_response;
        while (true) {
            parse_status = receiveFromEngine(state.client_id, parse_response, ctx, 30000);
            if (parse_status != core::Status::OK) {
                return sendErrorResponse(state, "No engine response for Firebird DSQL parse");
            }
            if (parse_response.getType() == IPCMessageType::ERROR_RESPONSE) {
                return sendErrorResponse(state, engineErrorMessage(parse_response));
            }
            if (parse_response.getType() == IPCMessageType::PARSE_COMPLETE) {
                stmt_it->second.engine_statement_prepared = true;
                break;
            }
        }
    }

    stmt_it->second.pending_rows.clear();
    stmt_it->second.current_fetch_index = -1;
    stmt_it->second.execution_complete = false;
    stmt_it->second.output_field_names.clear();
    stmt_it->second.bound_params.clear();
    stmt_it->second.bound_param_nulls.clear();
    stmt_it->second.select_count = 0;
    stmt_it->second.insert_count = 0;
    stmt_it->second.update_count = 0;
    stmt_it->second.delete_count = 0;

    if (message_count > 1) {
        return sendErrorResponse(state, "Invalid Firebird DSQL input message count");
    }

    if (!input_blr.empty()) {
        if (!decodeFirebirdMessageOnlyLayout(
                std::vector<uint8_t>(input_blr.begin(), input_blr.end()),
                stmt_it->second.input_message_fields,
                decode_error)) {
            return sendErrorResponse(state, decode_error);
        }
        if (message_count != 0) {
            auto layout_it = stmt_it->second.input_message_fields.find(static_cast<uint8_t>(message_number));
            if (layout_it == stmt_it->second.input_message_fields.end()) {
                return sendErrorResponse(state, "Unknown Firebird DSQL input message layout");
            }
            if (!decodeFirebirdSqlMessage(packet,
                                          offset,
                                          layout_it->second,
                                          state.protocol_version >= fb::PROTOCOL_VERSION13,
                                          stmt_it->second.bound_params,
                                          stmt_it->second.bound_param_nulls,
                                          decode_error)) {
                return sendErrorResponse(state, decode_error);
            }
        }
    }

    uint32_t out_message_number = 0;
    if (want_sql_response) {
        std::string output_blr;
        if (!consumeXdrString(packet, offset, output_blr, decode_error)) {
            return sendErrorResponse(state, decode_error);
        }
        if (offset + 4 > packet.size()) {
            return sendErrorResponse(state, "Malformed Firebird DSQL output message number");
        }
        out_message_number = xdrReadUint32(packet.data() + offset);
        offset += 4;

        if (!output_blr.empty()) {
            if (!decodeFirebirdMessageOnlyLayout(
                    std::vector<uint8_t>(output_blr.begin(), output_blr.end()),
                    stmt_it->second.output_message_fields,
                    decode_error)) {
                return sendErrorResponse(state, decode_error);
            }
        }
    }

    if (stmt_it->second.portal_active) {
        auto status = closeEngineObject(state, statement_handle, 'P',
                                        firebirdDsqlPortalName(statement_handle), ctx);
        if (status != core::Status::OK) {
            return status;
        }
        stmt_it->second.portal_active = false;
    }

    IPCMessage bind_msg(IPCMessageType::BIND, 0);
    bind_msg.header.request_id = state.session_id != 0 ? state.session_id : statement_handle;
    IPCBindPayload bind_payload{};
    const std::string portal_name = firebirdDsqlPortalName(statement_handle);
    std::strncpy(bind_payload.portal_name, portal_name.c_str(), sizeof(bind_payload.portal_name) - 1);
    bind_payload.portal_name[sizeof(bind_payload.portal_name) - 1] = '\0';
    std::strncpy(bind_payload.stmt_name, stmt_it->second.stmt_name.c_str(), sizeof(bind_payload.stmt_name) - 1);
    bind_payload.stmt_name[sizeof(bind_payload.stmt_name) - 1] = '\0';
    bind_payload.num_params = static_cast<uint16_t>(stmt_it->second.bound_params.size());

    size_t bind_payload_size = sizeof(bind_payload);
    for (size_t i = 0; i < stmt_it->second.bound_params.size(); ++i) {
        bind_payload_size += sizeof(IPCParamValue);
        if (!stmt_it->second.bound_param_nulls[i] && stmt_it->second.bound_params[i].has_value()) {
            bind_payload_size += stmt_it->second.bound_params[i]->size();
        }
    }
    bind_msg.payload.resize(bind_payload_size);
    std::memcpy(bind_msg.payload.data(), &bind_payload, sizeof(bind_payload));
    size_t bind_offset = sizeof(bind_payload);
    for (size_t i = 0; i < stmt_it->second.bound_params.size(); ++i) {
        IPCParamValue param{};
        param.type_oid = 0;
        param.format = 0;
        if (stmt_it->second.bound_param_nulls[i] || !stmt_it->second.bound_params[i].has_value()) {
            param.length = -1;
        }
        else {
            param.length = static_cast<int32_t>(stmt_it->second.bound_params[i]->size());
        }
        std::memcpy(bind_msg.payload.data() + bind_offset, &param, sizeof(param));
        bind_offset += sizeof(param);
        if (param.length > 0) {
            std::memcpy(bind_msg.payload.data() + bind_offset,
                        stmt_it->second.bound_params[i]->data(),
                        static_cast<size_t>(param.length));
            bind_offset += static_cast<size_t>(param.length);
        }
    }

    auto status = sendToEngine(state.client_id, bind_msg, ctx);
    if (status != core::Status::OK) {
        return sendErrorResponse(state, "Failed to bind Firebird DSQL statement");
    }

    IPCMessage engine_response;
    while (true) {
        status = receiveFromEngine(state.client_id, engine_response, ctx, 30000);
        if (status != core::Status::OK) {
            return sendErrorResponse(state, "No engine response for Firebird DSQL bind");
        }
        if (engine_response.getType() == IPCMessageType::ERROR_RESPONSE) {
            return sendErrorResponse(state, engineErrorMessage(engine_response));
        }
        if (engine_response.getType() == IPCMessageType::BIND_COMPLETE) {
            break;
        }
        continue;
    }
    stmt_it->second.portal_active = true;
    stmt_it->second.portal_active = true;

    IPCMessage execute_msg(IPCMessageType::EXECUTE, 0);
    execute_msg.header.request_id = state.session_id != 0 ? state.session_id : statement_handle;
    IPCExecutePayload execute_payload{};
    std::strncpy(execute_payload.portal_name, portal_name.c_str(), sizeof(execute_payload.portal_name) - 1);
    execute_payload.portal_name[sizeof(execute_payload.portal_name) - 1] = '\0';
    execute_payload.max_rows = 0;
    execute_msg.payload.resize(sizeof(execute_payload));
    std::memcpy(execute_msg.payload.data(), &execute_payload, sizeof(execute_payload));

    status = sendToEngine(state.client_id, execute_msg, ctx);
    if (status != core::Status::OK) {
        return sendErrorResponse(state, "Failed to execute Firebird DSQL statement");
    }

    const uint32_t statement_type = inferFirebirdStatementType(stmt_it->second.sql_text);
    bool saw_complete = false;
    while (!saw_complete) {
        status = receiveFromEngine(state.client_id, engine_response, ctx, 30000);
        if (status != core::Status::OK) {
            return sendErrorResponse(state, "No engine response for Firebird DSQL execute");
        }

        switch (engine_response.getType()) {
            case IPCMessageType::ROW_DESCRIPTION:
                break;
            case IPCMessageType::DATA_ROW:
            {
                std::vector<std::optional<std::string>> row;
                std::string row_error;
                if (!decodeIpcDataRow(engine_response, row, row_error)) {
                    return sendErrorResponse(state,
                                             row_error.empty()
                                                 ? "Malformed engine DATA_ROW payload for Firebird DSQL"
                                                 : row_error);
                }
                stmt_it->second.pending_rows.push_back(std::move(row));
                break;
            }
            case IPCMessageType::COMMAND_COMPLETE:
                if (auto* payload =
                        engine_response.getPayload<IPCCommandCompletePayload>()) {
                    switch (statement_type) {
                        case 1: // SELECT
                            stmt_it->second.select_count =
                                static_cast<uint64_t>(stmt_it->second.pending_rows.size());
                            break;
                        case 2: // INSERT
                            stmt_it->second.insert_count = payload->rows_affected;
                            break;
                        case 3: // UPDATE
                            stmt_it->second.update_count = payload->rows_affected;
                            break;
                        case 4: // DELETE
                            stmt_it->second.delete_count = payload->rows_affected;
                            break;
                        default:
                            break;
                    }
                }
                stmt_it->second.execution_complete = true;
                saw_complete = true;
                break;
            case IPCMessageType::BIND_COMPLETE:
            case IPCMessageType::PARSE_COMPLETE:
            case IPCMessageType::EMPTY_RESPONSE:
            case IPCMessageType::READY:
            case IPCMessageType::READY_FOR_QUERY:
            case IPCMessageType::NOTICE:
                break;
            case IPCMessageType::ERROR_RESPONSE:
                return sendErrorResponse(state, engineErrorMessage(engine_response));
            default:
                return sendErrorResponse(
                    state,
                    "Unexpected engine response during Firebird DSQL execute: " +
                        std::string(ipcMessageTypeToString(engine_response.getType())) +
                        " (" +
                        std::to_string(static_cast<uint32_t>(engine_response.getType())) +
                        ")");
        }
    }

    if (statement_type == 5) {
        state.pending_catalog_refresh = true;
    }
    else if (statement_type == 10 && state.pending_catalog_refresh) {
        auto refresh_status = refreshCommittedCatalogState(state, ctx);
        if (refresh_status != core::Status::OK) {
            return sendErrorResponse(state,
                                     ctx && !ctx->message.empty()
                                         ? ctx->message
                                         : "Failed to refresh Firebird catalog after commit");
        }
        state.pending_catalog_refresh = false;
    }
    else if (statement_type == 11) {
        state.pending_catalog_refresh = false;
    }

    if (want_sql_response) {
        auto layout_it = stmt_it->second.output_message_fields.find(static_cast<uint8_t>(out_message_number));
        if (layout_it == stmt_it->second.output_message_fields.end()) {
            std::vector<uint8_t> response_packet;
            std::string encode_error;
            if (!appendFirebirdSqlResponsePacket(response_packet,
                                                 std::nullopt,
                                                 {},
                                                 state.protocol_version >= fb::PROTOCOL_VERSION13,
                                                 encode_error)) {
                return sendErrorResponse(state, encode_error);
            }
            status = sendPacket(state, response_packet, ctx);
            if (status != core::Status::OK) {
                return status;
            }
            sendResponse(state, transaction_handle, 0, nullptr, 0, ctx);
            return core::Status::OK;
        }

        std::optional<std::vector<std::optional<std::string>>> row;
        if (!stmt_it->second.pending_rows.empty()) {
            row = std::move(stmt_it->second.pending_rows.front());
            stmt_it->second.pending_rows.pop_front();
        }
        std::vector<uint8_t> response_packet;
        std::string encode_error;
        if (!appendFirebirdSqlResponsePacket(response_packet,
                                             row,
                                             layout_it->second,
                                             state.protocol_version >= fb::PROTOCOL_VERSION13,
                                             encode_error)) {
            return sendErrorResponse(state,
                                     encode_error.empty()
                                         ? "Firebird SQL response encoding failed"
                                         : encode_error);
        }
        status = sendPacket(state, response_packet, ctx);
        if (status != core::Status::OK) {
            return status;
        }
        sendResponse(state, transaction_handle, 0, nullptr, 0, ctx);
        return core::Status::OK;
    }

    sendResponse(state, transaction_handle, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleFetchStatement(FBClientState& state,
                                                      const std::vector<uint8_t>& packet,
                                                      bool scroll,
                                                      core::ErrorContext* ctx) {
    if (packet.size() < 16) {
        return sendErrorResponse(state, "Malformed Firebird op_fetch packet");
    }

    size_t offset = 4;
    const uint32_t statement_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    std::string output_blr;
    std::string decode_error;
    if (!consumeXdrString(packet, offset, output_blr, decode_error)) {
        return sendErrorResponse(state, decode_error);
    }
    if (offset + 8 > packet.size()) {
        return sendErrorResponse(state, "Malformed Firebird fetch message header");
    }
    const uint32_t message_number = xdrReadUint32(packet.data() + offset);
    offset += 4;
    const uint32_t message_count = xdrReadUint32(packet.data() + offset);
    offset += 4;
    uint32_t fetch_operation = fb::fetch_next;
    int32_t fetch_position = 0;
    if (scroll) {
        if (offset + 8 > packet.size()) {
            return sendErrorResponse(state, "Malformed Firebird fetch_scroll packet");
        }
        fetch_operation = xdrReadUint32(packet.data() + offset);
        offset += 4;
        fetch_position = static_cast<int32_t>(xdrReadUint32(packet.data() + offset));
        offset += 4;
    }

    auto stmt_it = state.dsql_statements.find(statement_handle);
    if (stmt_it == state.dsql_statements.end()) {
        return sendErrorResponse(state, "Unknown Firebird DSQL statement handle");
    }

    if (!output_blr.empty()) {
        if (!decodeFirebirdMessageOnlyLayout(
                std::vector<uint8_t>(output_blr.begin(), output_blr.end()),
                stmt_it->second.output_message_fields,
                decode_error)) {
            return sendErrorResponse(state, decode_error);
        }
    }

    auto layout_it = stmt_it->second.output_message_fields.find(static_cast<uint8_t>(message_number));
    if (layout_it == stmt_it->second.output_message_fields.end()) {
        return sendErrorResponse(state, "Unknown Firebird DSQL fetch message layout");
    }
    if (!stmt_it->second.execution_complete && stmt_it->second.pending_rows.empty()) {
        return sendErrorResponse(state, "Firebird fetch requested before the statement produced rows");
    }

    const uint32_t batch_rows = message_count == 0 ? 1u : message_count;
    const int64_t row_count = static_cast<int64_t>(stmt_it->second.pending_rows.size());
    int64_t start_index = stmt_it->second.current_fetch_index + 1;
    bool forward = true;

    if (scroll) {
        switch (fetch_operation) {
            case fb::fetch_next:
                start_index = stmt_it->second.current_fetch_index + 1;
                forward = true;
                break;
            case fb::fetch_prior:
                start_index = stmt_it->second.current_fetch_index - 1;
                forward = false;
                break;
            case fb::fetch_first:
                start_index = 0;
                forward = true;
                break;
            case fb::fetch_last:
                start_index = row_count - 1;
                forward = false;
                break;
            case fb::fetch_absolute:
                if (fetch_position > 0) {
                    start_index = static_cast<int64_t>(fetch_position) - 1;
                    forward = true;
                }
                else if (fetch_position < 0) {
                    start_index = row_count + static_cast<int64_t>(fetch_position);
                    forward = false;
                }
                else {
                    start_index = -1;
                }
                break;
            case fb::fetch_relative:
                if (stmt_it->second.current_fetch_index < 0) {
                    start_index = fetch_position > 0 ? static_cast<int64_t>(fetch_position) - 1
                                                     : static_cast<int64_t>(fetch_position);
                }
                else {
                    start_index = stmt_it->second.current_fetch_index + static_cast<int64_t>(fetch_position);
                }
                forward = fetch_position >= 0;
                break;
            default:
                return sendErrorResponse(state, "Unsupported Firebird fetch_scroll operation");
        }
    }

    std::vector<int64_t> row_indexes;
    row_indexes.reserve(batch_rows);
    for (uint32_t i = 0; i < batch_rows; ++i) {
        const int64_t index = start_index + (forward ? static_cast<int64_t>(i)
                                                     : -static_cast<int64_t>(i));
        if (index < 0 || index >= row_count) {
            break;
        }
        row_indexes.push_back(index);
    }

    for (const int64_t index : row_indexes) {
        std::vector<uint8_t> response_packet;
        std::string encode_error;
        std::optional<std::vector<std::optional<std::string>>> row =
            stmt_it->second.pending_rows[static_cast<size_t>(index)];
        if (!appendFirebirdFetchResponsePacket(response_packet,
                                               0,
                                               row,
                                               layout_it->second,
                                               state.protocol_version >= fb::PROTOCOL_VERSION13,
                                               encode_error)) {
            return sendErrorResponse(state,
                                     encode_error.empty()
                                         ? "Firebird fetch row encoding failed"
                                         : encode_error);
        }
        auto status = sendPacket(state, response_packet, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }

    if (!row_indexes.empty()) {
        stmt_it->second.current_fetch_index = row_indexes.back();
    }

    std::vector<uint8_t> trailer_packet;
    std::string trailer_error;
    const bool exhausted =
        row_indexes.empty() ||
        (forward
             ? row_indexes.back() >= row_count - 1
             : row_indexes.back() <= 0);
    const int32_t trailer_status = exhausted && stmt_it->second.execution_complete ? 100 : 0;
    if (!appendFirebirdFetchResponsePacket(trailer_packet,
                                           trailer_status,
                                           std::nullopt,
                                           layout_it->second,
                                           state.protocol_version >= fb::PROTOCOL_VERSION13,
                                           trailer_error)) {
        return sendErrorResponse(state,
                                 trailer_error.empty()
                                     ? "Firebird fetch trailer encoding failed"
                                     : trailer_error);
    }
    return sendPacket(state, trailer_packet, ctx);
}

core::Status FirebirdParserAgent::handleFreeStatement(FBClientState& state,
                                                     const std::vector<uint8_t>& packet,
                                                     core::ErrorContext* ctx) {
    if (packet.size() < 12) {
        return sendErrorResponse(state, "Malformed Firebird op_free_statement packet");
    }

    size_t offset = 4;
    const uint32_t statement_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    const uint32_t option = xdrReadUint32(packet.data() + offset);
    offset += 4;

    auto stmt_it = state.dsql_statements.find(statement_handle);
    if (stmt_it == state.dsql_statements.end()) {
        return sendErrorResponse(state, "Unknown Firebird DSQL statement handle");
    }

    if (option & 1u) { // DSQL_close
        if (!stmt_it->second.portal_active && (option & (2u | 4u)) == 0) {
            return sendErrorResponse(state, "Firebird DSQL cursor is not open");
        }
        if (stmt_it->second.portal_active) {
            auto status = closeEngineObject(state, statement_handle, 'P',
                                            firebirdDsqlPortalName(statement_handle), ctx);
            if (status != core::Status::OK) {
                return status;
            }
        }
        stmt_it->second.pending_rows.clear();
        stmt_it->second.current_fetch_index = -1;
        stmt_it->second.execution_complete = false;
        stmt_it->second.portal_active = false;
    }

    if (option & (2u | 4u)) { // DSQL_drop | DSQL_unprepare
        if (stmt_it->second.portal_active) {
            auto status = closeEngineObject(state, statement_handle, 'P',
                                            firebirdDsqlPortalName(statement_handle), ctx);
            if (status != core::Status::OK) {
                return status;
            }
            stmt_it->second.portal_active = false;
        }
        if (stmt_it->second.engine_statement_prepared && !stmt_it->second.stmt_name.empty()) {
            auto status = closeEngineObject(state, statement_handle, 'S', stmt_it->second.stmt_name, ctx);
            if (status != core::Status::OK) {
                return status;
            }
        }
        stmt_it->second.statement_prepared = false;
        stmt_it->second.engine_statement_prepared = false;
        stmt_it->second.compiled_bytecode.clear();
        if (option & 2u) { // DSQL_drop
            state.dsql_statements.erase(stmt_it);
        }
        else {
            stmt_it->second.sql_text.clear();
            stmt_it->second.cursor_name.clear();
            stmt_it->second.compiled_bytecode.clear();
            stmt_it->second.input_message_fields.clear();
            stmt_it->second.output_message_fields.clear();
            stmt_it->second.input_sqlda_fields.clear();
            stmt_it->second.output_sqlda_fields.clear();
            stmt_it->second.output_field_names.clear();
            stmt_it->second.bound_params.clear();
            stmt_it->second.bound_param_nulls.clear();
            stmt_it->second.pending_rows.clear();
            stmt_it->second.select_count = 0;
            stmt_it->second.insert_count = 0;
            stmt_it->second.update_count = 0;
            stmt_it->second.delete_count = 0;
            stmt_it->second.current_fetch_index = -1;
            stmt_it->second.execution_complete = false;
            stmt_it->second.portal_active = false;
        }
    }

    sendResponse(state, 0, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleSetCursor(FBClientState& state,
                                                 const std::vector<uint8_t>& packet,
                                                 core::ErrorContext* ctx) {
    if (packet.size() < 12) {
        return sendErrorResponse(state, "Malformed Firebird op_set_cursor packet");
    }

    size_t offset = 4;
    const uint32_t statement_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    std::string cursor_name;
    std::string decode_error;
    if (!consumeXdrString(packet, offset, cursor_name, decode_error)) {
        return sendErrorResponse(state, decode_error);
    }
    if (offset + 4 > packet.size()) {
        return sendErrorResponse(state, "Malformed Firebird cursor type");
    }
    const uint32_t cursor_type = xdrReadUint32(packet.data() + offset);
    offset += 4;
    (void)cursor_type;

    auto stmt_it = state.dsql_statements.find(statement_handle);
    if (stmt_it == state.dsql_statements.end()) {
        return sendErrorResponse(state, "Unknown Firebird DSQL statement handle");
    }
    stmt_it->second.cursor_name = std::move(cursor_name);
    sendResponse(state, 0, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleTransaction(FBClientState& state,
                                                   const std::vector<uint8_t>& packet,
                                                   core::ErrorContext* ctx) {
    if (packet.size() < 8) {
        return sendErrorResponse(state, "Malformed Firebird op_transaction packet");
    }

    size_t offset = 4;
    const uint32_t database_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    std::string tpb;
    std::string decode_error;
    if (!consumeXdrString(packet, offset, tpb, decode_error)) {
        return sendErrorResponse(state, decode_error);
    }

    if (state.attachment_id == 0 || database_handle != state.attachment_id) {
        return sendErrorResponse(state, "Unknown Firebird database handle");
    }

    FBTransactionState txn_state;
    txn_state.transaction_id = generateHandle();
    txn_state.oldest_interesting = txn_state.transaction_id;
    txn_state.oldest_snapshot = txn_state.transaction_id;
    txn_state.oldest_active = txn_state.transaction_id;
    txn_state.snapshot_number = txn_state.transaction_id;
    txn_state.prepared = false;
    txn_state.prepare_description.clear();
    txn_state.database_path = state.database;
    if (!decodeFirebirdTransactionTpb(tpb, txn_state, decode_error)) {
        return sendErrorResponse(state,
                                 decode_error.empty()
                                     ? "Unsupported Firebird TPB"
                                     : decode_error);
    }

    const uint32_t transaction_handle = txn_state.transaction_id;
    state.transactions[transaction_handle] = std::move(txn_state);
    sendResponse(state, transaction_handle, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleCommit(FBClientState& state,
                                              const std::vector<uint8_t>& packet,
                                              bool retaining,
                                              core::ErrorContext* ctx) {
    if (packet.size() < 8) {
        return sendErrorResponse(state, "Malformed Firebird commit packet");
    }

    const uint32_t transaction_handle = xdrReadUint32(packet.data() + 4);
    auto txn_it = state.transactions.find(transaction_handle);
    if (txn_it == state.transactions.end()) {
        return sendErrorResponse(state, "Unknown Firebird transaction handle");
    }
    if (!retaining) {
        state.transactions.erase(txn_it);
    }
    else {
        txn_it->second.prepared = false;
        txn_it->second.prepare_description.clear();
    }
    sendResponse(state, 0, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleRollback(FBClientState& state,
                                                const std::vector<uint8_t>& packet,
                                                bool retaining,
                                                core::ErrorContext* ctx) {
    if (packet.size() < 8) {
        return sendErrorResponse(state, "Malformed Firebird rollback packet");
    }

    const uint32_t transaction_handle = xdrReadUint32(packet.data() + 4);
    auto txn_it = state.transactions.find(transaction_handle);
    if (txn_it == state.transactions.end()) {
        return sendErrorResponse(state, "Unknown Firebird transaction handle");
    }
    if (!retaining) {
        state.transactions.erase(txn_it);
    }
    else {
        txn_it->second.prepared = false;
        txn_it->second.prepare_description.clear();
    }
    sendResponse(state, 0, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handlePrepare(FBClientState& state,
                                               const std::vector<uint8_t>& packet,
                                               core::ErrorContext* ctx) {
    if (packet.size() < 8) {
        return sendErrorResponse(state, "Malformed Firebird prepare packet");
    }

    const uint32_t transaction_handle = xdrReadUint32(packet.data() + 4);
    auto txn_it = state.transactions.find(transaction_handle);
    if (txn_it == state.transactions.end()) {
        return sendErrorResponse(state, "Unknown Firebird transaction handle");
    }

    txn_it->second.prepared = true;
    txn_it->second.prepare_description.clear();
    sendResponse(state, 0, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handlePrepare2(FBClientState& state,
                                                const std::vector<uint8_t>& packet,
                                                core::ErrorContext* ctx) {
    if (packet.size() < 12) {
        return sendErrorResponse(state, "Malformed Firebird prepare2 packet");
    }

    size_t offset = 4;
    const uint32_t transaction_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    std::string prepare_data;
    std::string decode_error;
    if (!consumeXdrString(packet, offset, prepare_data, decode_error)) {
        return sendErrorResponse(state, decode_error);
    }

    auto txn_it = state.transactions.find(transaction_handle);
    if (txn_it == state.transactions.end()) {
        return sendErrorResponse(state, "Unknown Firebird transaction handle");
    }

    txn_it->second.prepared = true;
    txn_it->second.prepare_description = std::move(prepare_data);
    sendResponse(state, 0, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleStart(FBClientState& state,
                                             const std::vector<uint8_t>& packet,
                                             bool want_response,
                                             core::ErrorContext* ctx) {
    if (packet.size() < 24) {
        return sendErrorResponse(state, "Malformed Firebird start packet");
    }

    size_t offset = 4; // operation already read
    const uint32_t request_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    const uint32_t incarnation = xdrReadUint32(packet.data() + offset);
    offset += 4;
    const uint32_t transaction_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    const uint32_t message_number = xdrReadUint32(packet.data() + offset);
    offset += 4;
    const uint32_t message_count = xdrReadUint32(packet.data() + offset);
    offset += 4;

    (void)incarnation;
    (void)transaction_handle;
    (void)message_number;
    (void)message_count;

    auto request_it = state.compiled_requests.find(request_handle);
    if (request_it == state.compiled_requests.end()) {
        return sendErrorResponse(state, "Unknown compiled Firebird request handle");
    }
    request_it->second.pending_rows.clear();
    request_it->second.execution_complete = false;
    if (request_it->second.portal_active) {
        auto status = closeEngineObject(state, request_handle, 'P',
                                        firebirdPortalName(request_handle), ctx);
        if (status != core::Status::OK) {
            return status;
        }
        request_it->second.portal_active = false;
    }
    if (!request_it->second.input_bindings.empty() && !request_it->second.input_values_ready) {
        return sendErrorResponse(state, "Firebird request requires input message values before start");
    }

    IPCMessage bind_msg(IPCMessageType::BIND, 0);
    bind_msg.header.request_id = state.session_id != 0 ? state.session_id : request_handle;
    IPCBindPayload bind_payload{};
    const std::string portal_name = firebirdPortalName(request_handle);
    std::strncpy(bind_payload.portal_name, portal_name.c_str(), sizeof(bind_payload.portal_name) - 1);
    bind_payload.portal_name[sizeof(bind_payload.portal_name) - 1] = '\0';
    std::strncpy(bind_payload.stmt_name, request_it->second.stmt_name.c_str(), sizeof(bind_payload.stmt_name) - 1);
    bind_payload.stmt_name[sizeof(bind_payload.stmt_name) - 1] = '\0';
    bind_payload.num_params = static_cast<uint16_t>(request_it->second.bound_params.size());
    size_t bind_payload_size = sizeof(bind_payload);
    for (size_t i = 0; i < request_it->second.bound_params.size(); ++i) {
        bind_payload_size += sizeof(IPCParamValue);
        if (!request_it->second.bound_param_nulls[i] && request_it->second.bound_params[i].has_value()) {
            bind_payload_size += request_it->second.bound_params[i]->size();
        }
    }
    bind_msg.payload.resize(bind_payload_size);
    std::memcpy(bind_msg.payload.data(), &bind_payload, sizeof(bind_payload));
    size_t bind_offset = sizeof(bind_payload);
    for (size_t i = 0; i < request_it->second.bound_params.size(); ++i) {
        IPCParamValue param{};
        param.type_oid = 0;
        param.format = 0;
        if (request_it->second.bound_param_nulls[i] || !request_it->second.bound_params[i].has_value()) {
            param.length = -1;
        }
        else {
            param.length = static_cast<int32_t>(request_it->second.bound_params[i]->size());
        }
        std::memcpy(bind_msg.payload.data() + bind_offset, &param, sizeof(param));
        bind_offset += sizeof(param);
        if (param.length > 0) {
            std::memcpy(bind_msg.payload.data() + bind_offset,
                        request_it->second.bound_params[i]->data(),
                        static_cast<size_t>(param.length));
            bind_offset += static_cast<size_t>(param.length);
        }
    }

    auto status = sendToEngine(state.client_id, bind_msg, ctx);
    if (status != core::Status::OK) {
        return sendErrorResponse(state, "Failed to bind Firebird compiled request");
    }

    IPCMessage engine_response;
    status = receiveFromEngine(state.client_id, engine_response, ctx, 30000);
    if (status != core::Status::OK) {
        return sendErrorResponse(state, "No engine response for Firebird bind");
    }
    if (engine_response.getType() == IPCMessageType::ERROR_RESPONSE) {
        return sendErrorResponse(state, engineErrorMessage(engine_response));
    }
    if (engine_response.getType() != IPCMessageType::BIND_COMPLETE) {
        return sendErrorResponse(state, "Unexpected engine response during Firebird bind");
    }
    request_it->second.portal_active = true;

    IPCMessage execute_msg(IPCMessageType::EXECUTE, 0);
    execute_msg.header.request_id = state.session_id != 0 ? state.session_id : request_handle;
    IPCExecutePayload execute_payload{};
    std::strncpy(execute_payload.portal_name, portal_name.c_str(), sizeof(execute_payload.portal_name) - 1);
    execute_payload.portal_name[sizeof(execute_payload.portal_name) - 1] = '\0';
    execute_payload.max_rows = 0;
    execute_msg.payload.resize(sizeof(execute_payload));
    std::memcpy(execute_msg.payload.data(), &execute_payload, sizeof(execute_payload));

    status = sendToEngine(state.client_id, execute_msg, ctx);
    if (status != core::Status::OK) {
        return sendErrorResponse(state, "Failed to execute Firebird compiled request");
    }

    bool saw_complete = false;
    while (!saw_complete) {
        status = receiveFromEngine(state.client_id, engine_response, ctx, 30000);
        if (status != core::Status::OK) {
            return sendErrorResponse(state, "No engine response for Firebird execute");
        }

        switch (engine_response.getType()) {
            case IPCMessageType::ROW_DESCRIPTION:
                break;
            case IPCMessageType::DATA_ROW:
            {
                std::vector<std::optional<std::string>> row;
                std::string row_error;
                if (!decodeIpcDataRow(engine_response, row, row_error)) {
                    return sendErrorResponse(state,
                                             row_error.empty()
                                                 ? "Malformed engine DATA_ROW payload for Firebird request"
                                                 : row_error);
                }
                request_it->second.pending_rows.push_back(std::move(row));
                break;
            }
            case IPCMessageType::COMMAND_COMPLETE:
                request_it->second.input_values_ready = request_it->second.input_bindings.empty();
                request_it->second.execution_complete = true;
                saw_complete = true;
                break;
            case IPCMessageType::NOTICE:
                break;
            case IPCMessageType::ERROR_RESPONSE:
                return sendErrorResponse(state, engineErrorMessage(engine_response));
            default:
                return sendErrorResponse(state,
                                         "Unexpected engine response during Firebird execute");
        }
    }

    if (want_response) {
        const uint32_t response_message_number = !request_it->second.projection_bindings.empty()
                                                   ? request_it->second.projection_bindings.begin()->first
                                                   : message_number;
        std::optional<std::vector<std::optional<std::string>>> row;
        if (!request_it->second.pending_rows.empty()) {
            row = std::move(request_it->second.pending_rows.front());
            request_it->second.pending_rows.pop_front();
        }
        std::vector<uint8_t> send_packet;
        std::string encode_error;
        if (!appendFirebirdSendPacket(send_packet,
                                      request_handle,
                                      incarnation,
                                      transaction_handle,
                                      response_message_number,
                                      row,
                                      request_it->second,
                                      encode_error)) {
            return sendErrorResponse(state,
                                     encode_error.empty()
                                         ? "Firebird start-and-receive row encoding failed"
                                         : encode_error);
        }
        return sendPacket(state, send_packet, ctx);
    }

    sendResponse(state, 0, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleReceive(FBClientState& state,
                                              const std::vector<uint8_t>& packet,
                                              core::ErrorContext* ctx) {
    if (packet.size() < 24) {
        return sendErrorResponse(state, "Malformed Firebird receive packet");
    }

    size_t offset = 4; // operation already read
    const uint32_t request_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    const uint32_t incarnation = xdrReadUint32(packet.data() + offset);
    offset += 4;
    const uint32_t transaction_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    const uint32_t message_number = xdrReadUint32(packet.data() + offset);
    offset += 4;
    const uint32_t message_count = xdrReadUint32(packet.data() + offset);
    offset += 4;
    (void)message_count;

    auto request_it = state.compiled_requests.find(request_handle);
    if (request_it == state.compiled_requests.end()) {
        return sendErrorResponse(state, "Unknown compiled Firebird request handle");
    }

    std::optional<std::vector<std::optional<std::string>>> row;
    if (!request_it->second.pending_rows.empty()) {
        row = std::move(request_it->second.pending_rows.front());
        request_it->second.pending_rows.pop_front();
    }
    else if (!request_it->second.execution_complete) {
        return sendErrorResponse(state,
                                 "Firebird receive requested before the engine completed the request");
    }

    std::vector<uint8_t> send_packet;
    std::string encode_error;
    if (!appendFirebirdSendPacket(send_packet,
                                  request_handle,
                                  incarnation,
                                  transaction_handle,
                                  message_number,
                                  row,
                                  request_it->second,
                                  encode_error)) {
        return sendErrorResponse(state,
                                 encode_error.empty()
                                     ? "Firebird receive row encoding failed"
                                     : encode_error);
    }
    return sendPacket(state, send_packet, ctx);
}

core::Status FirebirdParserAgent::handleStartWithSend(FBClientState& state,
                                                     const std::vector<uint8_t>& packet,
                                                     bool want_response,
                                                     core::ErrorContext* ctx) {
    if (packet.size() < 24) {
        return sendErrorResponse(state, "Malformed Firebird start/send packet");
    }

    size_t offset = 4;
    const uint32_t request_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    (void)xdrReadUint32(packet.data() + offset); // incarnation
    offset += 4;
    (void)xdrReadUint32(packet.data() + offset); // transaction
    offset += 4;
    const uint32_t message_number = xdrReadUint32(packet.data() + offset);
    offset += 4;
    const uint32_t message_count = xdrReadUint32(packet.data() + offset);
    offset += 4;

    if (message_count != 1) {
        return sendErrorResponse(state, "Invalid Firebird request input message count");
    }

    auto request_it = state.compiled_requests.find(request_handle);
    if (request_it == state.compiled_requests.end()) {
        return sendErrorResponse(state, "Unknown compiled Firebird request handle");
    }

    std::string decode_error;
    if (!decodeFirebirdInputBindings(packet, offset, message_number, request_it->second, decode_error)) {
        return sendErrorResponse(state,
                                 decode_error.empty()
                                     ? "Firebird input message decode failed"
                                     : decode_error);
    }

    return handleStart(state, packet, want_response, ctx);
}

core::Status FirebirdParserAgent::handleSend(FBClientState& state,
                                            const std::vector<uint8_t>& packet,
                                            core::ErrorContext* ctx) {
    if (packet.size() < 24) {
        return sendErrorResponse(state, "Malformed Firebird send packet");
    }

    size_t offset = 4;
    const uint32_t request_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    (void)xdrReadUint32(packet.data() + offset); // incarnation
    offset += 4;
    (void)xdrReadUint32(packet.data() + offset); // transaction
    offset += 4;
    const uint32_t message_number = xdrReadUint32(packet.data() + offset);
    offset += 4;
    const uint32_t message_count = xdrReadUint32(packet.data() + offset);
    offset += 4;

    if (message_count != 1) {
        return sendErrorResponse(state, "Invalid Firebird request input message count");
    }

    auto request_it = state.compiled_requests.find(request_handle);
    if (request_it == state.compiled_requests.end()) {
        return sendErrorResponse(state, "Unknown compiled Firebird request handle");
    }

    std::string decode_error;
    if (!decodeFirebirdInputBindings(packet, offset, message_number, request_it->second, decode_error)) {
        return sendErrorResponse(state,
                                 decode_error.empty()
                                     ? "Firebird input message decode failed"
                                     : decode_error);
    }

    sendResponse(state, 0, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleUnwind(FBClientState& state,
                                              const std::vector<uint8_t>& packet,
                                              core::ErrorContext* ctx) {
    if (packet.size() < 8) {
        return sendErrorResponse(state, "Malformed Firebird op_unwind packet");
    }

    size_t offset = 4;
    const uint32_t request_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    (void)offset;

    auto request_it = state.compiled_requests.find(request_handle);
    if (request_it == state.compiled_requests.end()) {
        return sendErrorResponse(state, "Unknown compiled Firebird request handle");
    }

    if (request_it->second.portal_active) {
        auto status = closeEngineObject(state, request_handle, 'P',
                                        firebirdPortalName(request_handle), ctx);
        if (status != core::Status::OK) {
            return status;
        }
        request_it->second.portal_active = false;
    }
    request_it->second.pending_rows.clear();
    request_it->second.execution_complete = false;
    request_it->second.input_values_ready = request_it->second.input_bindings.empty();
    sendResponse(state, 0, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleRelease(FBClientState& state,
                                               const std::vector<uint8_t>& packet,
                                               core::ErrorContext* ctx) {
    if (packet.size() < 8) {
        return sendErrorResponse(state, "Malformed Firebird op_release packet");
    }

    size_t offset = 4;
    const uint32_t request_handle = xdrReadUint32(packet.data() + offset);
    offset += 4;
    (void)offset;

    auto request_it = state.compiled_requests.find(request_handle);
    if (request_it == state.compiled_requests.end()) {
        return sendErrorResponse(state, "Unknown compiled Firebird request handle");
    }

    if (request_it->second.portal_active) {
        auto status = closeEngineObject(state, request_handle, 'P',
                                        firebirdPortalName(request_handle), ctx);
        if (status != core::Status::OK) {
            return status;
        }
        request_it->second.portal_active = false;
    }
    if (request_it->second.statement_prepared && !request_it->second.stmt_name.empty()) {
        auto status = closeEngineObject(state, request_handle, 'S',
                                        request_it->second.stmt_name, ctx);
        if (status != core::Status::OK) {
            return status;
        }
    }
    state.compiled_requests.erase(request_it);
    sendResponse(state, 0, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleInfo(FBClientState& state,
                                            const std::vector<uint8_t>& packet,
                                            uint32_t op,
                                            core::ErrorContext* ctx) {
    if (op == fb::op_info_sql) {
        if (packet.size() < 16) {
            return sendErrorResponse(state, "Malformed Firebird op_info_sql packet");
        }

        size_t offset = 4;
        const uint32_t statement_handle = xdrReadUint32(packet.data() + offset);
        offset += 4;
        offset += 4; // incarnation, xdr short encoded as 4 bytes
        std::string items;
        std::string decode_error;
        if (!consumeXdrString(packet, offset, items, decode_error)) {
            return sendErrorResponse(state, decode_error);
        }
        if (offset + 4 > packet.size()) {
            return sendErrorResponse(state, "Malformed Firebird op_info_sql buffer length");
        }
        const uint32_t buffer_length = xdrReadUint32(packet.data() + offset);
        offset += 4;

        auto stmt_it = state.dsql_statements.find(statement_handle);
        if (stmt_it == state.dsql_statements.end()) {
            return sendErrorResponse(state, "Unknown Firebird DSQL statement handle");
        }

        std::vector<uint8_t> info;
        buildFirebirdStatementInfoBuffer(stmt_it->second, items, buffer_length, info);

        const uint32_t debug_seq = g_firebird_info_sql_debug_logs.fetch_add(1);
        if (debug_seq < 12) {
            std::cerr << "[parser_debug] firebird info_sql handle=" << statement_handle
                      << " req_items=[" << describeFirebirdInfoItems(items) << "]"
                      << " req_hex=" << formatHexBytes(
                             reinterpret_cast<const uint8_t*>(items.data()),
                             items.size())
                      << " resp_hex=" << formatHexBytes(info.data(), info.size())
                      << "\n";
        }

        sendResponse(state,
                     statement_handle,
                     0,
                     info.empty() ? nullptr : info.data(),
                     info.size(),
                     ctx);
        return core::Status::OK;
    }

    if (op == fb::op_info_request) {
        if (packet.size() < 16) {
            return sendErrorResponse(state, "Malformed Firebird op_info_request packet");
        }

        size_t offset = 4;
        const uint32_t request_handle = xdrReadUint32(packet.data() + offset);
        offset += 4;
        offset += 4; // incarnation, xdr short encoded as 4 bytes
        std::string items;
        std::string decode_error;
        if (!consumeXdrString(packet, offset, items, decode_error)) {
            return sendErrorResponse(state, decode_error);
        }
        if (offset + 4 > packet.size()) {
            return sendErrorResponse(state, "Malformed Firebird op_info_request buffer length");
        }
        const uint32_t buffer_length = xdrReadUint32(packet.data() + offset);
        offset += 4;
        (void)offset;

        auto request_it = state.compiled_requests.find(request_handle);
        if (request_it == state.compiled_requests.end()) {
            return sendErrorResponse(state, "Unknown compiled Firebird request handle");
        }

        std::vector<uint8_t> info;
        buildFirebirdRequestInfoBuffer(request_it->second, items, buffer_length, info);

        sendResponse(state,
                     request_handle,
                     0,
                     info.empty() ? nullptr : info.data(),
                     info.size(),
                     ctx);
        return core::Status::OK;
    }

    if (op == fb::op_info_database) {
        if (packet.size() < 16) {
            return sendErrorResponse(state, "Malformed Firebird op_info_database packet");
        }

        size_t offset = 4;
        const uint32_t database_handle = xdrReadUint32(packet.data() + offset);
        offset += 4;
        offset += 4; // incarnation, xdr short encoded as 4 bytes
        std::string items;
        std::string decode_error;
        if (!consumeXdrString(packet, offset, items, decode_error)) {
            return sendErrorResponse(state, decode_error);
        }
        if (offset + 4 > packet.size()) {
            return sendErrorResponse(state, "Malformed Firebird op_info_database buffer length");
        }
        const uint32_t buffer_length = xdrReadUint32(packet.data() + offset);
        offset += 4;
        (void)offset;

        if (state.attachment_id == 0 || database_handle != state.attachment_id) {
            return sendErrorResponse(state, "Unknown Firebird database handle");
        }

        std::vector<uint8_t> info;
        buildFirebirdDatabaseInfoBuffer(state, items, buffer_length, info);

        sendResponse(state,
                     database_handle,
                     0,
                     info.empty() ? nullptr : info.data(),
                     info.size(),
                     ctx);
        return core::Status::OK;
    }

    if (op == fb::op_info_transaction) {
        if (packet.size() < 16) {
            return sendErrorResponse(state, "Malformed Firebird op_info_transaction packet");
        }

        size_t offset = 4;
        const uint32_t transaction_handle = xdrReadUint32(packet.data() + offset);
        offset += 4;
        offset += 4; // incarnation, xdr short encoded as 4 bytes
        std::string items;
        std::string decode_error;
        if (!consumeXdrString(packet, offset, items, decode_error)) {
            return sendErrorResponse(state, decode_error);
        }
        if (offset + 4 > packet.size()) {
            return sendErrorResponse(state, "Malformed Firebird op_info_transaction buffer length");
        }
        const uint32_t buffer_length = xdrReadUint32(packet.data() + offset);
        offset += 4;
        (void)offset;

        auto txn_it = state.transactions.find(transaction_handle);
        if (txn_it == state.transactions.end()) {
            return sendErrorResponse(state, "Unknown Firebird transaction handle");
        }

        std::vector<uint8_t> info;
        buildFirebirdTransactionInfoBuffer(txn_it->second, items, buffer_length, info);

        sendResponse(state,
                     transaction_handle,
                     0,
                     info.empty() ? nullptr : info.data(),
                     info.size(),
                     ctx);
        return core::Status::OK;
    }

    if (op == fb::op_info_cursor) {
        if (packet.size() < 16) {
            return sendErrorResponse(state, "Malformed Firebird op_info_cursor packet");
        }

        size_t offset = 4;
        const uint32_t statement_handle = xdrReadUint32(packet.data() + offset);
        offset += 4;
        offset += 4; // incarnation, xdr short encoded as 4 bytes
        std::string items;
        std::string decode_error;
        if (!consumeXdrString(packet, offset, items, decode_error)) {
            return sendErrorResponse(state, decode_error);
        }
        if (offset + 4 > packet.size()) {
            return sendErrorResponse(state, "Malformed Firebird op_info_cursor buffer length");
        }
        const uint32_t buffer_length = xdrReadUint32(packet.data() + offset);
        offset += 4;
        (void)offset;

        auto stmt_it = state.dsql_statements.find(statement_handle);
        if (stmt_it == state.dsql_statements.end()) {
            return sendErrorResponse(state, "Unknown Firebird DSQL statement handle");
        }

        std::vector<uint8_t> info;
        buildFirebirdCursorInfoBuffer(stmt_it->second, items, buffer_length, info);

        sendResponse(state,
                     statement_handle,
                     0,
                     info.empty() ? nullptr : info.data(),
                     info.size(),
                     ctx);
        return core::Status::OK;
    }

    // Deterministic minimal info response; avoid uninitialized payload bytes.
    const uint8_t info_end = 1;  // isc_info_end
    sendResponse(state, 0, 0, &info_end, 1, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleBlobOpen(FBClientState& state,
                                                const std::vector<uint8_t>& packet,
                                                bool create,
                                                core::ErrorContext* ctx) {
    (void)packet;
    (void)create;
    uint32_t blob_handle = generateHandle();
    sendResponse(state, blob_handle, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleBlobGetSegment(FBClientState& state,
                                                      const std::vector<uint8_t>& packet,
                                                      core::ErrorContext* ctx) {
    (void)packet;
    sendResponse(state, 0, 0, nullptr, 0, ctx);
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleBlobPutSegment(FBClientState& state,
                                                      const std::vector<uint8_t>& packet,
                                                      core::ErrorContext* ctx) {
    (void)state;
    (void)packet;
    (void)ctx;
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleBlobClose(core::ErrorContext* ctx, bool cancel) {
    (void)ctx;
    (void)cancel;
    return core::Status::OK;
}

core::Status FirebirdParserAgent::handleCrypt(FBClientState& state,
                                             const std::vector<uint8_t>& packet,
                                             core::ErrorContext* ctx) {
    if (packet.size() < 4) {
        return sendErrorResponse(state, "Malformed Firebird crypt packet");
    }

    const uint32_t op = xdrReadUint32(packet.data());
    size_t offset = 4;
    std::string decode_error;
    if (op == fb::op_crypt) {
        std::string plugin;
        std::string key_name;
        if (!consumeXdrString(packet, offset, plugin, decode_error)) {
            return sendErrorResponse(state, decode_error);
        }
        if (!consumeXdrString(packet, offset, key_name, decode_error)) {
            return sendErrorResponse(state, decode_error);
        }
        state.encryption_plugin = plugin;
        state.wire_encrypted = !plugin.empty();
        if (!key_name.empty()) {
            state.scramble.assign(key_name.begin(), key_name.end());
        }
        sendResponse(state, 0, 0, nullptr, 0, ctx);
        return core::Status::OK;
    }

    if (op == fb::op_crypt_callback) {
        std::string callback_data;
        if (!consumeXdrString(packet, offset, callback_data, decode_error)) {
            return sendErrorResponse(state, decode_error);
        }
        if (offset + 4 <= packet.size()) {
            const uint32_t reply = xdrReadUint32(packet.data() + offset);
            (void)reply;
        }
        if (!callback_data.empty()) {
            state.auth_data.assign(callback_data.begin(), callback_data.end());
        }
        sendResponse(state, 0, 0, nullptr, 0, ctx);
        return core::Status::OK;
    }

    return sendErrorResponse(state, "Unsupported Firebird crypt opcode");
}

core::Status FirebirdParserAgent::handleAuthenticate(FBClientState& state,
                                                    const std::vector<uint8_t>& packet,
                                                    core::ErrorContext* ctx) {
    if (packet.size() < 4) {
        return sendErrorResponse(state, "Malformed Firebird authenticate packet");
    }

    const uint32_t op = xdrReadUint32(packet.data());
    if (op == fb::op_authenticate_user) {
        if (packet.size() < 20) {
            return sendErrorResponse(state, "Malformed Firebird authenticate_user packet");
        }

        size_t offset = 4;
        const uint32_t database_handle = xdrReadUint32(packet.data() + offset);
        offset += 4;
        std::string dpb;
        std::string items;
        std::string decode_error;
        if (!consumeXdrString(packet, offset, dpb, decode_error)) {
            return sendErrorResponse(state, decode_error);
        }
        if (!consumeXdrString(packet, offset, items, decode_error)) {
            return sendErrorResponse(state, decode_error);
        }
        if (offset + 4 > packet.size()) {
            return sendErrorResponse(state, "Malformed Firebird authenticate_user buffer length");
        }
        const uint32_t buffer_length = xdrReadUint32(packet.data() + offset);
        offset += 4;
        (void)items;
        (void)buffer_length;
        (void)offset;

        if (state.attachment_id == 0 || database_handle != state.attachment_id) {
            return sendErrorResponse(state, "Unknown Firebird database handle");
        }

        std::string user_name;
        std::string auth_plugin;
        std::vector<uint8_t> auth_data;
        if (!decodeFirebirdBasicDpb(dpb, user_name, auth_plugin, auth_data, decode_error)) {
            return sendErrorResponse(state, decode_error);
        }

        if (!user_name.empty()) {
            if (!state.username.empty() && state.username != user_name) {
                return sendErrorResponse(state,
                                         "Firebird authenticate_user conflicts with established session user");
            }
            state.username = user_name;
        }
        if (!auth_plugin.empty()) {
            state.auth_plugin = auth_plugin;
        }
        if (!auth_data.empty()) {
            state.auth_data = std::move(auth_data);
        }
        sendResponse(state, 0, 0, nullptr, 0, ctx);
        return core::Status::OK;
    }

    if (op == fb::op_cont_auth) {
        size_t offset = 4;
        std::string data;
        std::string plugin_name;
        std::string plugin_list;
        std::string keys;
        std::string decode_error;
        if (!consumeXdrString(packet, offset, data, decode_error)) {
            return sendErrorResponse(state, decode_error);
        }
        if (!consumeXdrString(packet, offset, plugin_name, decode_error)) {
            return sendErrorResponse(state, decode_error);
        }
        if (!consumeXdrString(packet, offset, plugin_list, decode_error)) {
            return sendErrorResponse(state, decode_error);
        }
        if (!consumeXdrString(packet, offset, keys, decode_error)) {
            return sendErrorResponse(state, decode_error);
        }
        (void)keys;

        if (!plugin_name.empty()) {
            state.auth_plugin = plugin_name;
        } else if (!plugin_list.empty() && state.auth_plugin.empty()) {
            const auto comma = plugin_list.find(',');
            state.auth_plugin = plugin_list.substr(0, comma);
        }
        state.auth_data.assign(data.begin(), data.end());
        sendResponse(state, 0, 0, nullptr, 0, ctx);
        return core::Status::OK;
    }

    return sendErrorResponse(state, "Unsupported Firebird authentication opcode");
}

void FirebirdParserAgent::sendResponse(FBClientState& state,
                                      uint32_t handle,
                                      uint32_t status_code,
                                      const uint8_t* data,
                                      size_t data_len,
                                      core::ErrorContext* ctx) {
    std::vector<uint8_t> packet;

    // op_response
    xdrAppendUint32(packet, fb::op_response);

    // Handle
    xdrAppendUint32(packet, handle);

    // Object ID (int64, unused)
    xdrAppendInt64(packet, 0);

    // Data buffer
    xdrAppendBuffer(packet, data, data_len);

    // Status vector
    if (status_code == 0) {
        xdrAppendUint32(packet, fb::isc_arg_end);
    } else {
        xdrAppendUint32(packet, fb::isc_arg_gds);
        xdrAppendUint32(packet, status_code);
        xdrAppendUint32(packet, fb::isc_arg_end);
    }

    sendPacket(state, packet, ctx);
}

core::Status FirebirdParserAgent::sendErrorResponse(FBClientState& state,
                                                   const std::string& message) {
    std::cerr << "[parser_debug] firebird error op=" << state.last_op
              << " message=" << message << "\n";

    std::vector<uint8_t> packet;

    xdrAppendUint32(packet, fb::op_response);
    xdrAppendUint32(packet, 0);    // handle
    xdrAppendInt64(packet, 0);     // object id
    xdrAppendBuffer(packet, nullptr, 0); // no data

    xdrAppendInt32(packet, fb::isc_arg_gds);
    xdrAppendInt32(packet, fb::isc_dsql_error);
    if (!message.empty()) {
        xdrAppendInt32(packet, fb::isc_arg_string);
        xdrAppendString(packet, message);
    }
    xdrAppendInt32(packet, fb::isc_arg_sql_state);
    xdrAppendString(packet, "HY000");
    xdrAppendInt32(packet, fb::isc_arg_end);

    return sendPacket(state, packet, nullptr);
}

// ============================================================================
// I/O Helpers
// ============================================================================

core::Status FirebirdParserAgent::readPacket(FBClientState& state,
                                            std::vector<uint8_t>& packet,
                                            core::ErrorContext* ctx) {
    packet.clear();

    uint8_t opcode_buf[4];
    ssize_t n = sb_socket_recv(state.client_fd, opcode_buf, 4, MSG_WAITALL);
    if (n == 0) {
        return core::Status::CONNECTION_CLOSED;
    }
    if (n != 4) {
        if (ctx) {
            ctx->set(core::Status::IO_ERROR, "Failed to read Firebird opcode",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::IO_ERROR;
    }

    packet.insert(packet.end(), opcode_buf, opcode_buf + 4);

    const uint32_t opcode = xdrReadUint32(opcode_buf);
    size_t min_size = 4;
    switch (opcode) {
        case fb::op_connect:
            min_size = 28;
            break;
        case fb::op_attach:
        case fb::op_create:
            min_size = 16;
            break;
        case fb::op_transaction:
            min_size = 12;
            break;
        case fb::op_commit:
        case fb::op_rollback:
        case fb::op_commit_retaining:
        case fb::op_rollback_retaining:
        case fb::op_detach:
        case fb::op_prepare:
        case fb::op_prepare2:
        case fb::op_info_database:
        case fb::op_info_request:
        case fb::op_info_transaction:
        case fb::op_info_cursor:
            min_size = 8;
            break;
        case fb::op_allocate_statement:
            min_size = 8;
            break;
        case fb::op_prepare_statement:
        case fb::op_exec_immediate:
        case fb::op_exec_immediate2:
            min_size = 24;
            break;
        case fb::op_execute:
        case fb::op_execute2:
        case fb::op_fetch:
        case fb::op_fetch_scroll:
        case fb::op_set_cursor:
        case fb::op_info_sql:
        case fb::op_start:
        case fb::op_start_and_send:
        case fb::op_start_and_receive:
        case fb::op_start_send_and_receive:
        case fb::op_send:
        case fb::op_receive:
            min_size = 16;
            break;
        case fb::op_free_statement:
            min_size = 12;
            break;
        case fb::op_disconnect:
        case fb::op_ping:
        case fb::op_dummy:
        case fb::op_cancel:
        case fb::op_exit:
            min_size = 4;
            break;
        case fb::op_authenticate_user:
        case fb::op_cont_auth:
        case fb::op_crypt:
        case fb::op_crypt_callback:
            min_size = 12;
            break;
        default:
            min_size = 4;
            break;
    }

    auto bytes_available = [fd = state.client_fd]() -> ssize_t {
        int available = 0;
#ifdef _WIN32
        u_long pending = 0;
        if (ioctlsocket(fd, FIONREAD, &pending) != 0) {
            return -1;
        }
        available = static_cast<int>(pending);
#else
        if (ioctl(fd, FIONREAD, &available) != 0) {
            return -1;
        }
#endif
        return static_cast<ssize_t>(available);
    };

    auto ensurePacketSize = [&](size_t target_size) -> core::Status {
        while (packet.size() < target_size) {
            const size_t remaining = target_size - packet.size();
            std::vector<uint8_t> chunk(remaining);
            n = sb_socket_recv(state.client_fd, chunk.data(), remaining, MSG_WAITALL);
            if (n <= 0) {
                if (ctx) {
                    ctx->set(core::Status::IO_ERROR,
                            ("Failed to read Firebird packet body for op=" +
                             std::to_string(opcode) +
                             " target=" + std::to_string(target_size) +
                             " have=" + std::to_string(packet.size()))
                                .c_str(),
                            __FILE__, __LINE__, __func__);
                }
                return core::Status::IO_ERROR;
            }
            packet.insert(packet.end(), chunk.begin(), chunk.begin() + n);
        }
        return core::Status::OK;
    };

    auto ensureXdrField = [&](size_t& offset) -> core::Status {
        auto status = ensurePacketSize(offset + 4);
        if (status != core::Status::OK) {
            return status;
        }
        const uint32_t length = xdrReadUint32(packet.data() + offset);
        const size_t padded_length = (static_cast<size_t>(length) + 3u) & ~size_t(3u);
        status = ensurePacketSize(offset + 4 + padded_length);
        if (status != core::Status::OK) {
            return status;
        }
        offset += 4 + padded_length;
        return core::Status::OK;
    };

    auto paddedSize = [](size_t length) -> size_t {
        return length + ((4 - (length % 4)) & 3u);
    };

    auto ensureXdrStringValue = [&](size_t& offset, std::string& out) -> core::Status {
        auto status = ensurePacketSize(offset + 4);
        if (status != core::Status::OK) {
            return status;
        }
        const uint32_t length = xdrReadUint32(packet.data() + offset);
        const size_t padded_length = paddedSize(static_cast<size_t>(length));
        status = ensurePacketSize(offset + 4 + padded_length);
        if (status != core::Status::OK) {
            return status;
        }
        out.assign(reinterpret_cast<const char*>(packet.data() + offset + 4), length);
        offset += 4 + padded_length;
        return core::Status::OK;
    };

    auto ensureSqlFieldValue = [&](const FBMessageFieldDesc& field, size_t& offset) -> core::Status {
        switch (field.type_opcode) {
            case 7:
            case 8:
            case 10:
            case 12:
            case 13:
                return ensurePacketSize(offset + 4) == core::Status::OK ? (offset += 4, core::Status::OK)
                                                                        : core::Status::IO_ERROR;
            case 11:
            case 16:
            case 24:
            case 27:
            case 28:
            case 30:
            case 35:
                return ensurePacketSize(offset + 8) == core::Status::OK ? (offset += 8, core::Status::OK)
                                                                        : core::Status::IO_ERROR;
            case 25:
            case 26:
            case 29:
            case 31:
                return ensurePacketSize(offset + 12) == core::Status::OK ? (offset += 12, core::Status::OK)
                                                                         : core::Status::IO_ERROR;
            case 14:
            case 15:
                return ensurePacketSize(offset + paddedSize(field.length)) == core::Status::OK
                           ? (offset += paddedSize(field.length), core::Status::OK)
                           : core::Status::IO_ERROR;
            case 17:
                return ensurePacketSize(offset + 8) == core::Status::OK ? (offset += 8, core::Status::OK)
                                                                        : core::Status::IO_ERROR;
            case 23:
                return ensurePacketSize(offset + 4) == core::Status::OK ? (offset += 4, core::Status::OK)
                                                                        : core::Status::IO_ERROR;
            case 37:
            case 38:
            case 40:
            case 41:
            {
                auto status = ensurePacketSize(offset + 4);
                if (status != core::Status::OK) {
                    return status;
                }
                const uint32_t length = xdrReadUint32(packet.data() + offset);
                offset += 4;
                status = ensurePacketSize(offset + paddedSize(static_cast<size_t>(length)));
                if (status != core::Status::OK) {
                    return status;
                }
                offset += paddedSize(static_cast<size_t>(length));
                return core::Status::OK;
            }
            default:
                if (ctx) {
                    ctx->set(core::Status::INVALID_ARGUMENT,
                             "Unsupported Firebird SQL field opcode in packet reader",
                             __FILE__, __LINE__, __func__);
                }
                return core::Status::INVALID_ARGUMENT;
        }
    };

    auto ensureSqlMessageValue =
        [&](size_t& offset,
            const std::vector<FBMessageFieldDesc>& fields,
            bool packed) -> core::Status {
            if (fields.size() % 2 != 0) {
                if (ctx) {
                    ctx->set(core::Status::INVALID_ARGUMENT,
                             "Firebird SQL BLR message must contain value/null field pairs",
                             __FILE__, __LINE__, __func__);
                }
                return core::Status::INVALID_ARGUMENT;
            }

            const size_t column_count = fields.size() / 2;
            if (packed) {
                const size_t flag_bytes = (column_count + 7) / 8;
                auto status = ensurePacketSize(offset + paddedSize(flag_bytes));
                if (status != core::Status::OK) {
                    return status;
                }
                const size_t bitmap_offset = offset;
                offset += paddedSize(flag_bytes);
                for (size_t i = 0; i < column_count; ++i) {
                    const uint8_t bits =
                        static_cast<uint8_t>(packet[bitmap_offset + (i >> 3)]);
                    const bool is_null = (bits & (1u << (i & 7))) != 0;
                    if (!is_null) {
                        status = ensureSqlFieldValue(fields[i * 2], offset);
                        if (status != core::Status::OK) {
                            return status;
                        }
                    }
                }
                return core::Status::OK;
            }

            for (const auto& field : fields) {
                auto status = ensureSqlFieldValue(field, offset);
                if (status != core::Status::OK) {
                    return status;
                }
            }
            return core::Status::OK;
        };

    auto status = ensurePacketSize(min_size);
    if (status != core::Status::OK) {
        return status;
    }

    bool allow_available_tail = true;
    switch (opcode) {
        case fb::op_attach:
        case fb::op_create:
        {
            size_t offset = 8;
            status = ensureXdrField(offset);
            if (status != core::Status::OK) {
                return status;
            }
            status = ensureXdrField(offset);
            if (status != core::Status::OK) {
                return status;
            }
            allow_available_tail = false;
            break;
        }
        case fb::op_compile:
        {
            status = ensurePacketSize(12);
            if (status != core::Status::OK) {
                return status;
            }
            const uint32_t blr_length = xdrReadUint32(packet.data() + 8);
            const size_t padded_length = (static_cast<size_t>(blr_length) + 3u) & ~size_t(3u);
            status = ensurePacketSize(12 + padded_length);
            if (status != core::Status::OK) {
                return status;
            }
            allow_available_tail = false;
            break;
        }
        case fb::op_transaction:
        {
            size_t offset = 8;
            status = ensureXdrField(offset);
            if (status != core::Status::OK) {
                return status;
            }
            allow_available_tail = false;
            break;
        }
        case fb::op_prepare2:
        {
            size_t offset = 8;
            status = ensureXdrField(offset);
            if (status != core::Status::OK) {
                return status;
            }
            allow_available_tail = false;
            break;
        }
        case fb::op_prepare_statement:
        {
            size_t offset = 16;
            status = ensureXdrField(offset);
            if (status != core::Status::OK) {
                return status;
            }
            status = ensureXdrField(offset);
            if (status != core::Status::OK) {
                return status;
            }
            status = ensurePacketSize(offset + 4);
            if (status != core::Status::OK) {
                return status;
            }
            allow_available_tail = false;
            break;
        }
        case fb::op_exec_immediate:
        case fb::op_exec_immediate2:
        {
            size_t offset = 4;
            if (opcode == fb::op_exec_immediate2) {
                std::string input_blr;
                status = ensureXdrStringValue(offset, input_blr);
                if (status != core::Status::OK) {
                    return status;
                }
                status = ensurePacketSize(offset + 8);
                if (status != core::Status::OK) {
                    return status;
                }
                const uint32_t input_message_number = xdrReadUint32(packet.data() + offset);
                offset += 4;
                const uint32_t input_message_count = xdrReadUint32(packet.data() + offset);
                offset += 4;
                if (!input_blr.empty() && input_message_count != 0) {
                    std::unordered_map<uint8_t, std::vector<FBMessageFieldDesc>> message_fields;
                    std::string layout_error;
                    if (!decodeFirebirdMessageOnlyLayout(
                            std::vector<uint8_t>(input_blr.begin(), input_blr.end()),
                            message_fields,
                            layout_error)) {
                        if (ctx) {
                            ctx->set(core::Status::INVALID_ARGUMENT,
                                     layout_error.c_str(),
                                     __FILE__, __LINE__, __func__);
                        }
                        return core::Status::INVALID_ARGUMENT;
                    }
                    auto it = message_fields.find(static_cast<uint8_t>(input_message_number));
                    if (it == message_fields.end()) {
                        if (ctx) {
                            ctx->set(core::Status::INVALID_ARGUMENT,
                                     "Unknown Firebird exec_immediate input message layout",
                                     __FILE__, __LINE__, __func__);
                        }
                        return core::Status::INVALID_ARGUMENT;
                    }
                    status = ensureSqlMessageValue(offset, it->second,
                                                   state.protocol_version >= fb::PROTOCOL_VERSION13);
                    if (status != core::Status::OK) {
                        return status;
                    }
                }

                std::string output_blr;
                status = ensureXdrStringValue(offset, output_blr);
                if (status != core::Status::OK) {
                    return status;
                }
                status = ensurePacketSize(offset + 4);
                if (status != core::Status::OK) {
                    return status;
                }
                offset += 4; // out message number
            }

            std::string sql_text;
            std::string items;
            status = ensurePacketSize(offset + 12);
            if (status != core::Status::OK) {
                return status;
            }
            offset += 12; // transaction, statement, dialect
            status = ensureXdrStringValue(offset, sql_text);
            if (status != core::Status::OK) {
                return status;
            }
            status = ensureXdrStringValue(offset, items);
            if (status != core::Status::OK) {
                return status;
            }
            status = ensurePacketSize(offset + 4);
            if (status != core::Status::OK) {
                return status;
            }
            offset += 4; // buffer length
            allow_available_tail = false;
            break;
        }
        case fb::op_execute:
        case fb::op_execute2:
        {
            size_t offset = 4;
            std::string input_blr;
            status = ensurePacketSize(offset + 8);
            if (status != core::Status::OK) {
                return status;
            }
            offset += 8; // statement, transaction
            status = ensureXdrStringValue(offset, input_blr);
            if (status != core::Status::OK) {
                return status;
            }
            status = ensurePacketSize(offset + 8);
            if (status != core::Status::OK) {
                return status;
            }
            const uint32_t input_message_number = xdrReadUint32(packet.data() + offset);
            offset += 4;
            const uint32_t input_message_count = xdrReadUint32(packet.data() + offset);
            offset += 4;
            if (!input_blr.empty() && input_message_count != 0) {
                std::unordered_map<uint8_t, std::vector<FBMessageFieldDesc>> message_fields;
                std::string layout_error;
                if (!decodeFirebirdMessageOnlyLayout(
                        std::vector<uint8_t>(input_blr.begin(), input_blr.end()),
                        message_fields,
                        layout_error)) {
                    if (ctx) {
                        ctx->set(core::Status::INVALID_ARGUMENT,
                                 layout_error.c_str(),
                                 __FILE__, __LINE__, __func__);
                    }
                    return core::Status::INVALID_ARGUMENT;
                }
                auto it = message_fields.find(static_cast<uint8_t>(input_message_number));
                if (it == message_fields.end()) {
                    if (ctx) {
                        ctx->set(core::Status::INVALID_ARGUMENT,
                                 "Unknown Firebird execute input message layout",
                                 __FILE__, __LINE__, __func__);
                    }
                    return core::Status::INVALID_ARGUMENT;
                }
                status = ensureSqlMessageValue(offset, it->second,
                                               state.protocol_version >= fb::PROTOCOL_VERSION13);
                if (status != core::Status::OK) {
                    return status;
                }
            }
            if (opcode == fb::op_execute2) {
                std::string output_blr;
                status = ensureXdrStringValue(offset, output_blr);
                if (status != core::Status::OK) {
                    return status;
                }
                status = ensurePacketSize(offset + 4);
                if (status != core::Status::OK) {
                    return status;
                }
                offset += 4; // out message number
            }
            if (state.protocol_version >= fb::PROTOCOL_VERSION16) {
                status = ensurePacketSize(offset + 4);
                if (status != core::Status::OK) {
                    return status;
                }
                offset += 4; // timeout
            }
            if (state.protocol_version >= fb::PROTOCOL_VERSION18) {
                status = ensurePacketSize(offset + 4);
                if (status != core::Status::OK) {
                    return status;
                }
                offset += 4; // cursor flags
            }
            allow_available_tail = false;
            break;
        }
        case fb::op_fetch:
        case fb::op_fetch_scroll:
        {
            size_t offset = 4;
            std::string output_blr;
            status = ensurePacketSize(offset + 4);
            if (status != core::Status::OK) {
                return status;
            }
            offset += 4; // statement
            status = ensureXdrStringValue(offset, output_blr);
            if (status != core::Status::OK) {
                return status;
            }
            status = ensurePacketSize(offset + 8);
            if (status != core::Status::OK) {
                return status;
            }
            offset += 8; // message number, message count
            if (opcode == fb::op_fetch_scroll) {
                status = ensurePacketSize(offset + 8);
                if (status != core::Status::OK) {
                    return status;
                }
                offset += 8; // fetch op, fetch pos
            }
            allow_available_tail = false;
            break;
        }
        case fb::op_info_sql:
        case fb::op_info_request:
        case fb::op_info_database:
        case fb::op_info_transaction:
        case fb::op_info_cursor:
        {
            size_t offset = 12;
            status = ensureXdrField(offset);
            if (status != core::Status::OK) {
                return status;
            }
            status = ensurePacketSize(offset + 4);
            if (status != core::Status::OK) {
                return status;
            }
            allow_available_tail = false;
            break;
        }
        case fb::op_set_cursor:
        {
            size_t offset = 8;
            status = ensureXdrField(offset);
            if (status != core::Status::OK) {
                return status;
            }
            status = ensurePacketSize(offset + 4);
            if (status != core::Status::OK) {
                return status;
            }
            allow_available_tail = false;
            break;
        }
        case fb::op_commit:
        case fb::op_rollback:
        case fb::op_commit_retaining:
        case fb::op_rollback_retaining:
        case fb::op_detach:
        case fb::op_prepare:
        case fb::op_allocate_statement:
        case fb::op_disconnect:
        case fb::op_ping:
        case fb::op_dummy:
        case fb::op_cancel:
        case fb::op_exit:
        case fb::op_release:
        case fb::op_unwind:
        case fb::op_reconnect:
            allow_available_tail = false;
            break;
        default:
            break;
    }

    if (allow_available_tail) {
        ssize_t available = bytes_available();
        if (available > 0) {
            const size_t extra = std::min<size_t>(static_cast<size_t>(available), 65536 - packet.size());
            if (extra > 0) {
                std::vector<uint8_t> chunk(extra);
                n = sb_socket_recv(state.client_fd, chunk.data(), extra, 0);
                if (n > 0) {
                    packet.insert(packet.end(), chunk.begin(), chunk.begin() + n);
                }
            }
        }
    }

    if (packet.size() > 10 * 1024 * 1024) {
        if (ctx) {
            ctx->set(core::Status::INVALID_ARGUMENT, "Packet too large",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }

    return core::Status::OK;
}

core::Status FirebirdParserAgent::sendPacket(FBClientState& state,
                                            const std::vector<uint8_t>& packet,
                                            core::ErrorContext* ctx) {
    if (!packet.empty()) {
        if (sb_socket_send(state.client_fd, packet.data(), packet.size(), 0) != 
            static_cast<ssize_t>(packet.size())) {
            if (ctx) {
                ctx->set(core::Status::IO_ERROR, "Failed to send Firebird packet data",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::IO_ERROR;
        }
    }
    
    return core::Status::OK;
}

core::Status FirebirdParserAgent::readFullMessage(int fd,
                                                 std::vector<uint8_t>& message,
                                                 core::ErrorContext* ctx) {
    // For Firebird, the first 4 bytes are the XDR length
    uint8_t len_buf[4];
    ssize_t n = sb_socket_recv(fd, len_buf, 4, MSG_WAITALL);
    if (n == 0) {
        return core::Status::CONNECTION_CLOSED;
    }
    if (n != 4) {
        if (ctx) {
            ctx->set(core::Status::IO_ERROR, "Failed to read message length",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::IO_ERROR;
    }
    
    uint32_t len = xdrReadUint32(len_buf);
    message.insert(message.end(), len_buf, len_buf + 4);
    
    if (len > 0) {
        std::vector<uint8_t> payload(len);
        n = sb_socket_recv(fd, payload.data(), len, MSG_WAITALL);
        if (n != static_cast<ssize_t>(len)) {
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

core::Status FirebirdParserAgent::writeMessage(int fd,
                                              const std::vector<uint8_t>& message,
                                              core::ErrorContext* ctx) {
    ssize_t n = sb_socket_send(fd, message.data(), message.size(), 0);
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

core::Status FirebirdParserAgent::translateStartupToIPC(const std::vector<uint8_t>& startup,
                                                       IPCMessage& ipc_msg,
                                                       core::ErrorContext* ctx) {
    (void)startup;
    (void)ipc_msg;
    (void)ctx;
    return core::Status::OK;
}

core::Status FirebirdParserAgent::translateIPCToResponse(const IPCMessage& ipc_msg,
                                                        std::vector<uint8_t>& response,
                                                        core::ErrorContext* ctx) {
    (void)ctx;
    response.clear();

    auto appendSuccessResponse = [&](const uint8_t* data, size_t data_len) {
        xdrAppendUint32(response, fb::op_response);
        xdrAppendUint32(response, 0);   // handle
        xdrAppendInt64(response, 0);    // object id
        xdrAppendBuffer(response, data, data_len);
        xdrAppendUint32(response, fb::isc_arg_end);
    };

    switch (ipc_msg.getType()) {
        case IPCMessageType::ROW_DESCRIPTION: {
            auto* payload = ipc_msg.getPayload<IPCRowDescriptionPayload>();
            if (!payload) {
                return core::Status::INVALID_ARGUMENT;
            }
            xdrAppendUint32(response, fb::op_sql_response);
            xdrAppendUint32(response, payload->num_fields);
            return core::Status::OK;
        }

        case IPCMessageType::DATA_ROW: {
            auto* payload = ipc_msg.getPayload<IPCDataRowPayload>();
            if (!payload) {
                return core::Status::INVALID_ARGUMENT;
            }

            std::vector<uint8_t> row_data;
            size_t offset = sizeof(IPCDataRowPayload);
            const uint8_t* data = ipc_msg.payload.data();
            const size_t payload_size = ipc_msg.payload.size();
            for (uint16_t i = 0; i < payload->num_fields && offset + sizeof(int32_t) <= payload_size; i++) {
                int32_t len = 0;
                std::memcpy(&len, data + offset, sizeof(int32_t));
                offset += sizeof(int32_t);

                if (len < 0) {
                    xdrAppendUint32(row_data, 0xFFFFFFFFu);
                    continue;
                }

                if (offset + static_cast<size_t>(len) > payload_size) {
                    break;
                }
                xdrAppendBuffer(row_data, data + offset, static_cast<size_t>(len));
                offset += static_cast<size_t>(len);
            }

            xdrAppendUint32(response, fb::op_fetch_response);
            xdrAppendUint32(response, 0);  // status
            xdrAppendUint32(response, 1);  // one row
            xdrAppendBuffer(response, row_data.data(), row_data.size());
            return core::Status::OK;
        }

        case IPCMessageType::COMMAND_COMPLETE: {
            auto* payload = ipc_msg.getPayload<IPCCommandCompletePayload>();
            uint32_t affected = 0;
            if (payload) {
                const uint64_t max_u32 = 0xFFFFFFFFull;
                affected = static_cast<uint32_t>(payload->rows_affected > max_u32
                                                     ? max_u32
                                                     : payload->rows_affected);
            }

            std::vector<uint8_t> command_data;
            xdrAppendUint32(command_data, affected);
            appendSuccessResponse(command_data.data(), command_data.size());
            return core::Status::OK;
        }

        case IPCMessageType::ERROR_RESPONSE: {
            auto* payload = ipc_msg.getPayload<IPCErrorPayload>();
            const std::string message = payload ? std::string(payload->message) : "Unknown error";
            std::string sqlstate = payload ? std::string(payload->sqlstate) : "HY000";
            if (sqlstate.size() != 5) {
                sqlstate = "HY000";
            }
            const std::string mapped_code = mapSQLStateToProtocol(sqlstate.c_str());
            uint32_t gds_code = fb::isc_dsql_error;
            if (mapped_code == "335544472") {
                gds_code = fb::isc_login;
            } else if (mapped_code == "335544375") {
                gds_code = fb::isc_unavailable;
            } else if (mapped_code == "335544436") {
                gds_code = fb::isc_sqlerr;
            } else {
                gds_code = fb::isc_dsql_error;
            }

            xdrAppendUint32(response, fb::op_response);
            xdrAppendUint32(response, 0);    // handle
            xdrAppendInt64(response, 0);     // object id
            xdrAppendBuffer(response, nullptr, 0);
            xdrAppendInt32(response, fb::isc_arg_gds);
            xdrAppendInt32(response, static_cast<int32_t>(gds_code));
            xdrAppendInt32(response, fb::isc_arg_string);
            xdrAppendString(response, message);
            xdrAppendInt32(response, fb::isc_arg_sql_state);
            xdrAppendString(response, sqlstate);
            xdrAppendInt32(response, fb::isc_arg_end);
            return core::Status::OK;
        }

        case IPCMessageType::READY:
        case IPCMessageType::READY_FOR_QUERY:
        default:
            appendSuccessResponse(nullptr, 0);
            return core::Status::OK;
    }

    return core::Status::OK;
}

IPCMessageType FirebirdParserAgent::mapClientToIPC(uint8_t msg_type) {
    (void)msg_type;
    return IPCMessageType::ERROR_RESPONSE;
}

uint8_t FirebirdParserAgent::mapIPCToClient(IPCMessageType msg_type) {
    switch (msg_type) {
        case IPCMessageType::ROW_DESCRIPTION:
            return static_cast<uint8_t>(fb::op_sql_response);
        case IPCMessageType::DATA_ROW:
            return static_cast<uint8_t>(fb::op_fetch_response);
        case IPCMessageType::COMMAND_COMPLETE:
        case IPCMessageType::ERROR_RESPONSE:
        case IPCMessageType::READY:
        case IPCMessageType::READY_FOR_QUERY:
        default:
            return static_cast<uint8_t>(fb::op_response);
    }
}

std::string FirebirdParserAgent::mapSQLStateToProtocol(const char* sqlstate) {
    if (!sqlstate || sqlstate[0] == '\0') {
        return "335544569";  // isc_dsql_error
    }

    std::string state(sqlstate);
    if (state.size() != 5) {
        return "335544569";  // isc_dsql_error
    }

    const std::string cls = state.substr(0, 2);
    if (cls == "28") {
        return "335544472";  // isc_login
    }
    if (cls == "08") {
        return "335544375";  // isc_unavailable
    }
    if (cls == "42" || cls == "23" || cls == "22" || cls == "40") {
        return "335544569";  // isc_dsql_error
    }
    if (state == "HY000" || state == "XX000") {
        return "335544436";  // isc_sqlerr
    }
    return "335544436";      // isc_sqlerr
}

void FirebirdParserAgent::mapProtocolErrorToSQLState(const std::vector<uint8_t>& error,
                                                    char* sqlstate_out) {
    if (!sqlstate_out) {
        return;
    }

    auto write_state = [&](const char* state) {
        std::memcpy(sqlstate_out, state, 5);
        sqlstate_out[5] = '\0';
    };

    write_state("HY000");
    if (error.size() < 4) {
        return;
    }

    std::string fallback = "HY000";
    size_t offset = 0;

    const uint32_t op = xdrReadUint32(error.data());
    if (op == fb::op_response) {
        if (error.size() < 20) {  // op + handle + objectid
            return;
        }
        offset = 4 + 4 + 8;

        if (offset + 4 > error.size()) {
            return;
        }
        const uint32_t data_len = xdrReadUint32(error.data() + offset);
        offset += 4;

        const size_t padded_len = (static_cast<size_t>(data_len) + 3u) & ~size_t(3u);
        if (offset + padded_len > error.size()) {
            return;
        }
        offset += padded_len;
    }

    while (offset + 4 <= error.size()) {
        const uint32_t arg = xdrReadUint32(error.data() + offset);
        offset += 4;

        if (arg == fb::isc_arg_end) {
            break;
        }

        if (arg == fb::isc_arg_gds) {
            if (offset + 4 > error.size()) {
                break;
            }
            const uint32_t gds = xdrReadUint32(error.data() + offset);
            offset += 4;

            if (gds == fb::isc_login) {
                fallback = "28000";
            } else if (gds == fb::isc_unavailable) {
                fallback = "08006";
            } else if (gds == fb::isc_dsql_error || gds == fb::isc_sqlerr) {
                fallback = "42000";
            } else {
                fallback = "HY000";
            }
            continue;
        }

        if (arg == fb::isc_arg_sql_state || arg == fb::isc_arg_string) {
            if (offset + 4 > error.size()) {
                break;
            }
            const uint32_t len = xdrReadUint32(error.data() + offset);
            offset += 4;

            const size_t padded_len = (static_cast<size_t>(len) + 3u) & ~size_t(3u);
            if (offset + padded_len > error.size()) {
                break;
            }

            if (arg == fb::isc_arg_sql_state && len == 5) {
                std::memcpy(sqlstate_out, error.data() + offset, 5);
                sqlstate_out[5] = '\0';
                return;
            }
            offset += padded_len;
            continue;
        }

        // Unknown argument class with unknown payload width.
        break;
    }

    write_state(fallback.c_str());
}

size_t FirebirdParserAgent::readMessageLength(const uint8_t* header, size_t len) {
    (void)len;
    return xdrReadUint32(header);
}

uint32_t FirebirdParserAgent::generateHandle() {
    static std::atomic<uint32_t> next_handle{1};
    return next_handle++;
}

} // namespace ipc
} // namespace scratchbird
