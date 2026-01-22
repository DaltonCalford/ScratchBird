#include "scratchbird/client/scratchbird_client.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "scratchbird/client/driver_config.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/type_extractor.h"

struct sb_connection {
    scratchbird::client::NetworkClient client;
    scratchbird::client::NetworkClientConfig config;
};

struct sb_prepared {
    sb_connection* conn{nullptr};
    scratchbird::client::NetworkPreparedStatement stmt;
    std::vector<std::string> param_names;
};

struct sb_result {
    scratchbird::client::NetworkResultSet results;
    size_t row_index{0};
    std::vector<std::string> column_names;
};

namespace {

constexpr int32_t kDaysFrom1970To2000 = 10957;

void set_error(sb_error* err, sb_error_code code, const std::string& message) {
    if (!err) {
        return;
    }
    err->code = code;
    std::snprintf(err->message, sizeof(err->message), "%s", message.c_str());
}

sb_error_code map_status(scratchbird::core::Status status) {
    using scratchbird::core::Status;
    switch (status) {
        case Status::OK:
            return SB_OK;
        case Status::CONNECTION_FAILURE:
        case Status::CONNECTION_DOES_NOT_EXIST:
            return SB_ERR_CONNECTION_FAILED;
        case Status::INVALID_PASSWORD:
        case Status::INVALID_AUTHORIZATION:
            return SB_ERR_AUTH_FAILED;
        case Status::PROTOCOL_VIOLATION:
            return SB_ERR_PROTOCOL;
        case Status::SYNTAX_ERROR:
            return SB_ERR_SYNTAX;
        case Status::CONSTRAINT_VIOLATION:
            return SB_ERR_CONSTRAINT;
        case Status::TYPE_MISMATCH:
        case Status::DATATYPE_MISMATCH:
            return SB_ERR_TYPE_MISMATCH;
        case Status::DEADLOCK:
            return SB_ERR_DEADLOCK;
        case Status::SERIALIZATION_FAILURE:
            return SB_ERR_SERIALIZATION;
        case Status::TRANSACTION_ABORTED:
            return SB_ERR_TXN_ABORTED;
        case Status::NO_ACTIVE_TRANSACTION:
            return SB_ERR_NO_ACTIVE_TXN;
        case Status::LOCK_TIMEOUT:
            return SB_ERR_TIMEOUT;
        case Status::OOM:
            return SB_ERR_OUT_OF_MEMORY;
        case Status::DISK_FULL:
            return SB_ERR_DISK_FULL;
        case Status::TOO_MANY_CONNECTIONS:
            return SB_ERR_TOO_MANY_CONNECTIONS;
        case Status::INVALID_ARGUMENT:
            return SB_ERR_INVALID_PARAM;
        case Status::NOT_IMPLEMENTED:
        case Status::NOT_SUPPORTED:
            return SB_ERR_NOT_IMPLEMENTED;
        case Status::OBJECT_IN_USE:
            return SB_ERR_RESOURCE_BUSY;
        default:
            return SB_ERR_UNKNOWN;
    }
}

sb_type map_wire_type(scratchbird::protocol::WireType type) {
    using scratchbird::protocol::WireType;
    switch (type) {
        case WireType::BOOLEAN:
            return SB_TYPE_BOOLEAN;
        case WireType::INT16:
            return SB_TYPE_SMALLINT;
        case WireType::INT32:
            return SB_TYPE_INTEGER;
        case WireType::INT64:
            return SB_TYPE_BIGINT;
        case WireType::FLOAT32:
            return SB_TYPE_REAL;
        case WireType::FLOAT64:
            return SB_TYPE_DOUBLE;
        case WireType::DECIMAL:
            return SB_TYPE_DECIMAL;
        case WireType::CHAR:
            return SB_TYPE_CHAR;
        case WireType::VARCHAR:
            return SB_TYPE_VARCHAR;
        case WireType::BYTEA:
            return SB_TYPE_BLOB;
        case WireType::DATE:
            return SB_TYPE_DATE;
        case WireType::TIME:
            return SB_TYPE_TIME;
        case WireType::TIMESTAMP:
            return SB_TYPE_TIMESTAMP;
        case WireType::TIMESTAMPTZ:
            return SB_TYPE_TIMESTAMP_TZ;
        case WireType::INTERVAL:
            return SB_TYPE_INTERVAL;
        case WireType::UUID:
            return SB_TYPE_UUID;
        case WireType::JSON:
        case WireType::JSONB:
            return SB_TYPE_JSON;
        case WireType::ARRAY:
            return SB_TYPE_ARRAY;
        case WireType::INET:
            return SB_TYPE_INET;
        case WireType::CIDR:
            return SB_TYPE_CIDR;
        case WireType::MACADDR:
            return SB_TYPE_MACADDR;
        default:
            return SB_TYPE_TEXT;
    }
}

void decode_date(int32_t days_since_2000, sb_value* value) {
    int64_t days_since_epoch = static_cast<int64_t>(days_since_2000) + kDaysFrom1970To2000;
    value->data.date_val.year = scratchbird::core::TypeExtractor::extractYear(days_since_epoch);
    value->data.date_val.month = scratchbird::core::TypeExtractor::extractMonth(days_since_epoch);
    value->data.date_val.day = scratchbird::core::TypeExtractor::extractDay(days_since_epoch);
}

void decode_time(int64_t micros, sb_value* value) {
    value->data.time_val.hour = scratchbird::core::TypeExtractor::extractHour(micros);
    value->data.time_val.minute = scratchbird::core::TypeExtractor::extractMinute(micros);
    value->data.time_val.second = scratchbird::core::TypeExtractor::extractSecond(micros);
    value->data.time_val.microsecond = scratchbird::core::TypeExtractor::extractMicrosecond(micros);
}

void decode_timestamp(int64_t micros, sb_value* value) {
    value->data.timestamp_val.epoch_microseconds = micros;
    value->data.timestamp_val.tz_offset_seconds = 0;
}

void parse_named_params(const std::string& sql, std::vector<std::string>& names) {
    names.clear();
    bool in_string = false;
    for (size_t i = 0; i + 1 < sql.size(); ++i) {
        char ch = sql[i];
        if (ch == '\'') {
            in_string = !in_string;
            continue;
        }
        if (in_string) {
            continue;
        }
        if ((ch == ':' || ch == '@') && std::isalpha(static_cast<unsigned char>(sql[i + 1]))) {
            size_t j = i + 1;
            while (j < sql.size() && (std::isalnum(static_cast<unsigned char>(sql[j])) || sql[j] == '_')) {
                ++j;
            }
            names.emplace_back(sql.substr(i + 1, j - i - 1));
            i = j;
        }
    }
}

int apply_bind_value(scratchbird::client::NetworkPreparedStatement& stmt, size_t index, const sb_value* value) {
    if (!value) {
        return SB_ERR_NULL_POINTER;
    }
    if (value->is_null) {
        stmt.setNull(index, scratchbird::protocol::WireType::UNKNOWN);
        return SB_OK;
    }
    switch (value->type) {
        case SB_TYPE_BOOLEAN:
            stmt.setBool(index, value->data.boolean_val != 0);
            return SB_OK;
        case SB_TYPE_SMALLINT:
            stmt.setInt16(index, value->data.smallint_val);
            return SB_OK;
        case SB_TYPE_INTEGER:
            stmt.setInt32(index, value->data.integer_val);
            return SB_OK;
        case SB_TYPE_BIGINT:
            stmt.setInt64(index, value->data.bigint_val);
            return SB_OK;
        case SB_TYPE_REAL:
            stmt.setFloat(index, value->data.real_val);
            return SB_OK;
        case SB_TYPE_DOUBLE:
            stmt.setDouble(index, value->data.double_val);
            return SB_OK;
        case SB_TYPE_CHAR:
        case SB_TYPE_VARCHAR:
        case SB_TYPE_TEXT:
        case SB_TYPE_JSON:
        case SB_TYPE_ARRAY:
        case SB_TYPE_INET:
        case SB_TYPE_CIDR:
        case SB_TYPE_MACADDR:
        case SB_TYPE_DECIMAL:
            stmt.setString(index, std::string(value->data.string_val.data, value->data.string_val.length));
            return SB_OK;
        case SB_TYPE_BLOB:
            stmt.setBytes(index,
                          reinterpret_cast<const uint8_t*>(value->data.binary_val.data),
                          value->data.binary_val.length);
            return SB_OK;
        case SB_TYPE_DATE: {
            int32_t days = scratchbird::core::TypeExtractor::ymdToDays(
                value->data.date_val.year,
                value->data.date_val.month,
                value->data.date_val.day);
            int32_t days_since_2000 = days - kDaysFrom1970To2000;
            stmt.setDate(index, days_since_2000);
            return SB_OK;
        }
        case SB_TYPE_TIME: {
            int64_t micros = (static_cast<int64_t>(value->data.time_val.hour) * 3600 +
                              static_cast<int64_t>(value->data.time_val.minute) * 60 +
                              static_cast<int64_t>(value->data.time_val.second)) * 1000000LL +
                             value->data.time_val.microsecond;
            stmt.setTime(index, micros);
            return SB_OK;
        }
        case SB_TYPE_TIMESTAMP:
        case SB_TYPE_TIMESTAMP_TZ:
            stmt.setTimestamp(index, value->data.timestamp_val.epoch_microseconds);
            return SB_OK;
        case SB_TYPE_UUID: {
            std::vector<uint8_t> data(value->data.uuid_val.bytes,
                                      value->data.uuid_val.bytes + 16);
            stmt.setUUID(index, data);
            return SB_OK;
        }
        default:
            return SB_ERR_INVALID_PARAM;
    }
}

} // namespace

sb_connection* sb_connect(const char* conn_str, sb_error* err) {
    if (!conn_str) {
        set_error(err, SB_ERR_NULL_POINTER, "Connection string is required");
        return nullptr;
    }
    auto* conn = new sb_connection();
    scratchbird::core::ErrorContext ctx;
    scratchbird::client::applyDriverDefaultsFromEnv(conn->config);
    auto status = scratchbird::client::parseDriverConnectionString(conn_str, conn->config, &ctx);
    if (status != scratchbird::core::Status::OK) {
        set_error(err, map_status(status), ctx.message);
        delete conn;
        return nullptr;
    }
    status = conn->client.connect(conn->config, &ctx);
    if (status != scratchbird::core::Status::OK) {
        set_error(err, map_status(status), ctx.message.empty() ? conn->client.lastError() : ctx.message);
        delete conn;
        return nullptr;
    }
    set_error(err, SB_OK, "");
    return conn;
}

void sb_disconnect(sb_connection* conn) {
    if (!conn) {
        return;
    }
    conn->client.disconnect();
    delete conn;
}

sb_result* sb_execute(sb_connection* conn, const char* sql, sb_error* err) {
    if (!conn || !sql) {
        set_error(err, SB_ERR_NULL_POINTER, "Connection and SQL required");
        return nullptr;
    }
    auto* result = new sb_result();
    scratchbird::core::ErrorContext ctx;
    auto status = conn->client.executeQuery(sql, result->results, &ctx);
    if (status != scratchbird::core::Status::OK) {
        set_error(err, map_status(status), ctx.message.empty() ? conn->client.lastError() : ctx.message);
        delete result;
        return nullptr;
    }
    result->column_names.reserve(result->results.columns.size());
    for (const auto& col : result->results.columns) {
        result->column_names.push_back(col.name);
    }
    set_error(err, SB_OK, "");
    return result;
}

sb_result* sb_query(sb_connection* conn, const char* sql, sb_error* err) {
    return sb_execute(conn, sql, err);
}

int sb_fetch(sb_result* result, sb_row* row, sb_error* err) {
    if (!result || !row) {
        set_error(err, SB_ERR_NULL_POINTER, "Result and row required");
        return SB_ERR_NULL_POINTER;
    }
    if (result->row_index >= result->results.rows.size()) {
        set_error(err, SB_ERR_RESULT_EXHAUSTED, "No more rows");
        return SB_ERR_RESULT_EXHAUSTED;
    }
    row->result = result;
    row->row_index = result->row_index++;
    set_error(err, SB_OK, "");
    return SB_OK;
}

void sb_result_free(sb_result* result) {
    delete result;
}

int sb_column_count(sb_result* result) {
    if (!result) {
        return 0;
    }
    return static_cast<int>(result->results.columns.size());
}

int sb_get_column_meta(sb_result* result, int index, sb_column_meta* out) {
    if (!result || !out) {
        return SB_ERR_NULL_POINTER;
    }
    if (index < 0 || static_cast<size_t>(index) >= result->results.columns.size()) {
        return SB_ERR_INVALID_PARAM;
    }
    const auto& col = result->results.columns[static_cast<size_t>(index)];
    out->name = result->column_names[static_cast<size_t>(index)].c_str();
    out->type = map_wire_type(col.type);
    out->type_modifier = static_cast<int32_t>(col.type_modifier);
    out->nullable = 1;
    return SB_OK;
}

int sb_value_get(sb_row* row, int column, sb_value* out) {
    if (!row || !row->result || !out) {
        return SB_ERR_NULL_POINTER;
    }
    const auto& results = row->result->results;
    if (row->row_index >= results.rows.size()) {
        return SB_ERR_RESULT_EXHAUSTED;
    }
    if (column < 0 || static_cast<size_t>(column) >= results.rows[row->row_index].size()) {
        return SB_ERR_INVALID_PARAM;
    }
    const auto& value = results.rows[row->row_index][static_cast<size_t>(column)];
    auto wire_type = scratchbird::protocol::WireType::UNKNOWN;
    if (static_cast<size_t>(column) < results.columns.size()) {
        wire_type = results.columns[static_cast<size_t>(column)].type;
    }
    out->type = map_wire_type(wire_type);
    out->is_null = value.is_null ? 1 : 0;
    if (value.is_null) {
        return SB_OK;
    }
    const auto& data = value.data;
    switch (out->type) {
        case SB_TYPE_BOOLEAN:
            out->data.boolean_val = (!data.empty() && data[0]) ? 1 : 0;
            break;
        case SB_TYPE_SMALLINT: {
            int16_t v = 0;
            if (data.size() >= sizeof(v)) {
                std::memcpy(&v, data.data(), sizeof(v));
            }
            out->data.smallint_val = v;
            break;
        }
        case SB_TYPE_INTEGER: {
            int32_t v = 0;
            if (data.size() >= sizeof(v)) {
                std::memcpy(&v, data.data(), sizeof(v));
            }
            out->data.integer_val = v;
            break;
        }
        case SB_TYPE_BIGINT: {
            int64_t v = 0;
            if (data.size() >= sizeof(v)) {
                std::memcpy(&v, data.data(), sizeof(v));
            }
            out->data.bigint_val = v;
            break;
        }
        case SB_TYPE_REAL: {
            float v = 0;
            if (data.size() >= sizeof(v)) {
                std::memcpy(&v, data.data(), sizeof(v));
            }
            out->data.real_val = v;
            break;
        }
        case SB_TYPE_DOUBLE: {
            double v = 0;
            if (data.size() >= sizeof(v)) {
                std::memcpy(&v, data.data(), sizeof(v));
            }
            out->data.double_val = v;
            break;
        }
        case SB_TYPE_BLOB:
            out->data.binary_val.data = data.data();
            out->data.binary_val.length = data.size();
            break;
        case SB_TYPE_DATE: {
            int32_t days = 0;
            if (data.size() >= sizeof(days)) {
                std::memcpy(&days, data.data(), sizeof(days));
            }
            decode_date(days, out);
            break;
        }
        case SB_TYPE_TIME: {
            int64_t micros = 0;
            if (data.size() >= sizeof(micros)) {
                std::memcpy(&micros, data.data(), sizeof(micros));
            }
            decode_time(micros, out);
            break;
        }
        case SB_TYPE_TIMESTAMP:
        case SB_TYPE_TIMESTAMP_TZ: {
            int64_t micros = 0;
            if (data.size() >= sizeof(micros)) {
                std::memcpy(&micros, data.data(), sizeof(micros));
            }
            decode_timestamp(micros, out);
            break;
        }
        case SB_TYPE_UUID: {
            std::memset(out->data.uuid_val.bytes, 0, sizeof(out->data.uuid_val.bytes));
            if (data.size() >= 16) {
                std::memcpy(out->data.uuid_val.bytes, data.data(), 16);
            }
            break;
        }
        default:
            out->data.string_val.data = data.empty() ? "" : reinterpret_cast<const char*>(data.data());
            out->data.string_val.length = data.size();
            break;
    }
    return SB_OK;
}

int sb_get_int64(sb_row* row, int column, int64_t* out) {
    if (!out) {
        return SB_ERR_NULL_POINTER;
    }
    sb_value value{};
    auto status = sb_value_get(row, column, &value);
    if (status != SB_OK) {
        return status;
    }
    if (value.is_null) {
        *out = 0;
        return SB_OK;
    }
    switch (value.type) {
        case SB_TYPE_SMALLINT:
            *out = value.data.smallint_val;
            return SB_OK;
        case SB_TYPE_INTEGER:
            *out = value.data.integer_val;
            return SB_OK;
        case SB_TYPE_BIGINT:
            *out = value.data.bigint_val;
            return SB_OK;
        default:
            return SB_ERR_TYPE_MISMATCH;
    }
}

const char* sb_get_string(sb_row* row, int column, size_t* length) {
    sb_value value{};
    if (sb_value_get(row, column, &value) != SB_OK) {
        if (length) {
            *length = 0;
        }
        return nullptr;
    }
    if (value.is_null) {
        if (length) {
            *length = 0;
        }
        return nullptr;
    }
    if (value.type == SB_TYPE_BLOB) {
        if (length) {
            *length = value.data.binary_val.length;
        }
        return reinterpret_cast<const char*>(value.data.binary_val.data);
    }
    if (length) {
        *length = value.data.string_val.length;
    }
    return value.data.string_val.data;
}

sb_prepared* sb_prepare(sb_connection* conn, const char* sql, sb_error* err) {
    if (!conn || !sql) {
        set_error(err, SB_ERR_NULL_POINTER, "Connection and SQL required");
        return nullptr;
    }
    auto* stmt = new sb_prepared();
    stmt->conn = conn;
    scratchbird::core::ErrorContext ctx;
    auto status = conn->client.prepare(sql, stmt->stmt, &ctx);
    if (status != scratchbird::core::Status::OK) {
        set_error(err, map_status(status), ctx.message);
        delete stmt;
        return nullptr;
    }
    parse_named_params(sql, stmt->param_names);
    set_error(err, SB_OK, "");
    return stmt;
}

int sb_bind_index(sb_prepared* stmt, size_t index, const sb_value* value, sb_error* err) {
    if (!stmt) {
        set_error(err, SB_ERR_INVALID_HANDLE, "Statement is null");
        return SB_ERR_INVALID_HANDLE;
    }
    auto code = apply_bind_value(stmt->stmt, index, value);
    if (code != SB_OK) {
        set_error(err, static_cast<sb_error_code>(code), "Failed to bind parameter");
    } else {
        set_error(err, SB_OK, "");
    }
    return code;
}

int sb_bind_name(sb_prepared* stmt, const char* name, const sb_value* value, sb_error* err) {
    if (!stmt || !name) {
        set_error(err, SB_ERR_NULL_POINTER, "Name required");
        return SB_ERR_NULL_POINTER;
    }
    for (size_t i = 0; i < stmt->param_names.size(); ++i) {
        if (stmt->param_names[i] == name) {
            return sb_bind_index(stmt, i + 1, value, err);
        }
    }
    set_error(err, SB_ERR_INVALID_PARAM, "Parameter name not found");
    return SB_ERR_INVALID_PARAM;
}

sb_result* sb_execute_prepared(sb_prepared* stmt, sb_error* err) {
    if (!stmt || !stmt->conn) {
        set_error(err, SB_ERR_INVALID_HANDLE, "Statement is null");
        return nullptr;
    }
    auto* result = new sb_result();
    scratchbird::core::ErrorContext ctx;
    auto status = stmt->conn->client.executePrepared(stmt->stmt, result->results, &ctx);
    if (status != scratchbird::core::Status::OK) {
        set_error(err, map_status(status), ctx.message);
        delete result;
        return nullptr;
    }
    result->column_names.reserve(result->results.columns.size());
    for (const auto& col : result->results.columns) {
        result->column_names.push_back(col.name);
    }
    set_error(err, SB_OK, "");
    return result;
}

void sb_prepared_free(sb_prepared* stmt) {
    delete stmt;
}

int sb_tx_begin(sb_connection* conn, sb_error* err) {
    if (!conn) {
        set_error(err, SB_ERR_INVALID_HANDLE, "Connection is null");
        return SB_ERR_INVALID_HANDLE;
    }
    scratchbird::core::ErrorContext ctx;
    auto status = conn->client.beginTransaction(&ctx);
    set_error(err, map_status(status), ctx.message);
    return status == scratchbird::core::Status::OK ? SB_OK : map_status(status);
}

int sb_tx_commit(sb_connection* conn, sb_error* err) {
    if (!conn) {
        set_error(err, SB_ERR_INVALID_HANDLE, "Connection is null");
        return SB_ERR_INVALID_HANDLE;
    }
    scratchbird::core::ErrorContext ctx;
    auto status = conn->client.commit(&ctx);
    set_error(err, map_status(status), ctx.message);
    return status == scratchbird::core::Status::OK ? SB_OK : map_status(status);
}

int sb_tx_rollback(sb_connection* conn, sb_error* err) {
    if (!conn) {
        set_error(err, SB_ERR_INVALID_HANDLE, "Connection is null");
        return SB_ERR_INVALID_HANDLE;
    }
    scratchbird::core::ErrorContext ctx;
    auto status = conn->client.rollback(&ctx);
    set_error(err, map_status(status), ctx.message);
    return status == scratchbird::core::Status::OK ? SB_OK : map_status(status);
}
