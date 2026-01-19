#include "scratchbird/odbc/odbc_client_bridge.h"

#include <cstring>
#include <iomanip>
#include <sstream>

#include "scratchbird/core/type_extractor.h"

namespace scratchbird {
namespace odbc {

namespace {
SQLRETURN statusToReturn(core::Status status) {
    return status == core::Status::OK ? SQL_SUCCESS : SQL_ERROR;
}

network::SSLMode parseSslMode(const std::string& mode) {
    if (mode == "disable") return network::SSLMode::DISABLED;
    if (mode == "allow") return network::SSLMode::ALLOW;
    if (mode == "prefer") return network::SSLMode::PREFER;
    if (mode == "require") return network::SSLMode::REQUIRE;
    if (mode == "verify_ca") return network::SSLMode::VERIFY_CA;
    if (mode == "verify_full") return network::SSLMode::VERIFY_FULL;
    return network::SSLMode::PREFER;
}

constexpr int32_t kDaysFrom1970To2000 = 10957;
constexpr int64_t kMicrosPerSecond = 1000000LL;
constexpr int64_t kMicrosPerDay = 86400LL * kMicrosPerSecond;

template <typename T>
bool decodeScalar(const std::vector<uint8_t>& data, T& out) {
    if (data.size() < sizeof(T)) {
        return false;
    }
    std::memcpy(&out, data.data(), sizeof(T));
    return true;
}

std::string formatDateFromEpochDays(int64_t days_since_epoch) {
    int32_t year = core::TypeExtractor::extractYear(days_since_epoch);
    int32_t month = core::TypeExtractor::extractMonth(days_since_epoch);
    int32_t day = core::TypeExtractor::extractDay(days_since_epoch);

    std::ostringstream ss;
    ss << std::setw(4) << std::setfill('0') << year << '-'
       << std::setw(2) << std::setfill('0') << month << '-'
       << std::setw(2) << std::setfill('0') << day;
    return ss.str();
}

std::string formatTimeFromMicros(int64_t micros) {
    int32_t hour = core::TypeExtractor::extractHour(micros);
    int32_t minute = core::TypeExtractor::extractMinute(micros);
    int32_t second = core::TypeExtractor::extractSecond(micros);
    int32_t micro = core::TypeExtractor::extractMicrosecond(micros);

    std::ostringstream ss;
    ss << std::setw(2) << std::setfill('0') << hour << ':'
       << std::setw(2) << std::setfill('0') << minute << ':'
       << std::setw(2) << std::setfill('0') << second;
    if (micro != 0) {
        ss << '.' << std::setw(6) << std::setfill('0') << micro;
    }
    return ss.str();
}

std::string formatTimestampFromMicros(int64_t micros) {
    int32_t year = core::TypeExtractor::extractTimestampYear(micros);
    int32_t month = core::TypeExtractor::extractTimestampMonth(micros);
    int32_t day = core::TypeExtractor::extractTimestampDay(micros);
    int32_t hour = core::TypeExtractor::extractTimestampHour(micros);
    int32_t minute = core::TypeExtractor::extractTimestampMinute(micros);
    int32_t second = core::TypeExtractor::extractTimestampSecond(micros);
    int32_t micro = core::TypeExtractor::extractTimestampMicrosecond(micros);

    std::ostringstream ss;
    ss << std::setw(4) << std::setfill('0') << year << '-'
       << std::setw(2) << std::setfill('0') << month << '-'
       << std::setw(2) << std::setfill('0') << day << ' '
       << std::setw(2) << std::setfill('0') << hour << ':'
       << std::setw(2) << std::setfill('0') << minute << ':'
       << std::setw(2) << std::setfill('0') << second;
    if (micro != 0) {
        ss << '.' << std::setw(6) << std::setfill('0') << micro;
    }
    return ss.str();
}

std::string formatTimestampWithOffset(int64_t micros, int16_t offset_minutes) {
    int64_t local_micros = micros + static_cast<int64_t>(offset_minutes) * 60 * kMicrosPerSecond;
    std::string base = formatTimestampFromMicros(local_micros);

    int16_t abs_offset = static_cast<int16_t>(offset_minutes < 0 ? -offset_minutes : offset_minutes);
    int16_t offset_hours = abs_offset / 60;
    int16_t offset_mins = abs_offset % 60;

    std::ostringstream ss;
    ss << base << (offset_minutes < 0 ? '-' : '+')
       << std::setw(2) << std::setfill('0') << offset_hours << ':'
       << std::setw(2) << std::setfill('0') << offset_mins;
    return ss.str();
}

std::string formatInterval(int32_t months, int32_t days, int64_t micros) {
    int64_t total_seconds = micros / kMicrosPerSecond;
    int64_t micro = micros % kMicrosPerSecond;
    int64_t hours = total_seconds / 3600;
    int64_t minutes = (total_seconds / 60) % 60;
    int64_t seconds = total_seconds % 60;

    std::ostringstream ss;
    ss << months << " months " << days << " days "
       << std::setw(2) << std::setfill('0') << hours << ':'
       << std::setw(2) << std::setfill('0') << minutes << ':'
       << std::setw(2) << std::setfill('0') << seconds;
    if (micro != 0) {
        int64_t abs_micro = micro < 0 ? -micro : micro;
        ss << '.' << std::setw(6) << std::setfill('0') << abs_micro;
    }
    return ss.str();
}

std::string formatUuid(const std::vector<uint8_t>& data) {
    if (data.size() < 16) {
        return "";
    }
    std::ostringstream ss;
    ss << std::hex << std::nouppercase << std::setfill('0');
    for (size_t i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            ss << '-';
        }
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    return ss.str();
}

} // namespace

OdbcClientBridge::OdbcClientBridge() = default;
OdbcClientBridge::~OdbcClientBridge() = default;

SQLRETURN OdbcClientBridge::connect(const ConnectionParams& params, std::string& error) {
    auto config = buildConfig(params);
    core::ErrorContext ctx;
    auto status = client_.connect(config, &ctx);
    last_status_ = status;
    if (status != core::Status::OK) {
        last_error_ = ctx.message.empty() ? client_.lastError() : ctx.message;
        error = last_error_;
    } else {
        last_error_.clear();
    }
    return statusToReturn(status);
}

void OdbcClientBridge::disconnect() {
    client_.disconnect();
}

bool OdbcClientBridge::isConnected() const {
    return client_.isConnected();
}

SQLRETURN OdbcClientBridge::executeSQL(const std::string& sql,
                                       std::vector<std::vector<std::string>>& results,
                                       std::vector<ColumnMetadata>& columns,
                                       SQLLEN& rows_affected) {
    results.clear();
    columns.clear();
    rows_affected = 0;

    client::NetworkResultSet net_results;
    core::ErrorContext ctx;
    auto status = client_.executeQuery(sql, net_results, &ctx);
    last_status_ = status;
    if (status != core::Status::OK) {
        last_error_ = ctx.message.empty() ? client_.lastError() : ctx.message;
        return statusToReturn(status);
    }
    last_error_.clear();

    columns.reserve(net_results.columns.size());
    for (const auto& col : net_results.columns) {
        columns.push_back(mapColumn(col));
    }

    results.reserve(net_results.rows.size());
    for (const auto& row : net_results.rows) {
        std::vector<std::string> out_row;
        out_row.reserve(row.size());
        for (size_t i = 0; i < row.size(); ++i) {
            protocol::WireType type = protocol::WireType::UNKNOWN;
            if (i < net_results.columns.size()) {
                type = net_results.columns[i].type;
            }
            out_row.push_back(stringifyValue(row[i], type));
        }
        results.push_back(std::move(out_row));
    }

    rows_affected = static_cast<SQLLEN>(net_results.rows_affected);
    return SQL_SUCCESS;
}

SQLRETURN OdbcClientBridge::beginTransaction() {
    auto status = client_.beginTransaction();
    last_status_ = status;
    last_error_ = status == core::Status::OK ? "" : client_.lastError();
    return statusToReturn(status);
}

SQLRETURN OdbcClientBridge::commit() {
    auto status = client_.commit();
    last_status_ = status;
    last_error_ = status == core::Status::OK ? "" : client_.lastError();
    return statusToReturn(status);
}

SQLRETURN OdbcClientBridge::rollback() {
    auto status = client_.rollback();
    last_status_ = status;
    last_error_ = status == core::Status::OK ? "" : client_.lastError();
    return statusToReturn(status);
}

client::NetworkClientConfig OdbcClientBridge::buildConfig(const ConnectionParams& params) {
    client::NetworkClientConfig cfg;
    cfg.host = params.server.empty() ? "127.0.0.1" : params.server;
    cfg.port = params.port;
    cfg.database = params.database;
    cfg.username = params.user;
    cfg.password = params.password;
    cfg.application_name = params.application_name.empty() ? "scratchbird_odbc" : params.application_name;
    cfg.connect_timeout_ms = params.connect_timeout * 1000;
    if (params.query_timeout > 0) {
        cfg.read_timeout_ms = params.query_timeout * 1000;
        cfg.write_timeout_ms = params.query_timeout * 1000;
    }
    cfg.ssl_mode = parseSslMode(params.ssl_mode);
    cfg.ssl_cert = params.ssl_cert;
    cfg.ssl_key = params.ssl_key;
    cfg.ssl_root_cert = params.ssl_root_cert;
    client::applyDriverDefaultsFromEnv(cfg);
    return cfg;
}

ColumnMetadata OdbcClientBridge::mapColumn(const client::NetworkColumn& col) {
    ColumnMetadata meta;
    meta.name = col.name;
    meta.type_name = protocol::wireTypeToString(col.type);
    meta.sql_type = mapWireType(col.type);
    return meta;
}

SQLSMALLINT OdbcClientBridge::mapWireType(protocol::WireType type) {
    switch (type) {
        case protocol::WireType::NULL_TYPE:
            return SQL_UNKNOWN_TYPE;
        case protocol::WireType::BOOLEAN:
            return SQL_BIT;
        case protocol::WireType::INT16:
            return SQL_SMALLINT;
        case protocol::WireType::INT32:
            return SQL_INTEGER;
        case protocol::WireType::INT64:
            return SQL_BIGINT;
        case protocol::WireType::FLOAT32:
            return SQL_REAL;
        case protocol::WireType::FLOAT64:
            return SQL_DOUBLE;
        case protocol::WireType::DECIMAL:
            return SQL_DECIMAL;
        case protocol::WireType::VARCHAR:
            return SQL_VARCHAR;
        case protocol::WireType::CHAR:
            return SQL_CHAR;
        case protocol::WireType::JSON:
        case protocol::WireType::JSONB:
        case protocol::WireType::XML:
        case protocol::WireType::ARRAY:
        case protocol::WireType::COMPOSITE:
        case protocol::WireType::INET:
        case protocol::WireType::CIDR:
        case protocol::WireType::MACADDR:
        case protocol::WireType::TSVECTOR:
        case protocol::WireType::TSQUERY:
        case protocol::WireType::RANGE:
            return SQL_LONGVARCHAR;
        case protocol::WireType::BYTEA:
        case protocol::WireType::GEOMETRY:
        case protocol::WireType::VECTOR:
            return SQL_LONGVARBINARY;
        case protocol::WireType::DATE:
            return SQL_TYPE_DATE;
        case protocol::WireType::TIME:
            return SQL_TYPE_TIME;
        case protocol::WireType::TIMESTAMP:
        case protocol::WireType::TIMESTAMPTZ:
            return SQL_TYPE_TIMESTAMP;
        case protocol::WireType::INTERVAL:
            return SQL_VARCHAR;
        case protocol::WireType::UUID:
            return SQL_GUID;
        case protocol::WireType::MONEY:
            return SQL_DECIMAL;
        default:
            return SQL_VARCHAR;
    }
}

std::string OdbcClientBridge::stringifyValue(const protocol::ProtocolCodec::ColumnValue& val,
                                             protocol::WireType type) {
    if (val.is_null) {
        return "";
    }

    switch (type) {
        case protocol::WireType::BOOLEAN:
            return (!val.data.empty() && val.data[0] != 0) ? "1" : "0";
        case protocol::WireType::INT16: {
            int16_t out = 0;
            if (!decodeScalar(val.data, out)) return "0";
            return std::to_string(out);
        }
        case protocol::WireType::INT32: {
            int32_t out = 0;
            if (!decodeScalar(val.data, out)) return "0";
            return std::to_string(out);
        }
        case protocol::WireType::INT64: {
            int64_t out = 0;
            if (!decodeScalar(val.data, out)) return "0";
            return std::to_string(out);
        }
        case protocol::WireType::FLOAT32: {
            float out = 0.0f;
            if (!decodeScalar(val.data, out)) return "0";
            std::ostringstream ss;
            ss << out;
            return ss.str();
        }
        case protocol::WireType::FLOAT64: {
            double out = 0.0;
            if (!decodeScalar(val.data, out)) return "0";
            std::ostringstream ss;
            ss << out;
            return ss.str();
        }
        case protocol::WireType::DECIMAL:
        case protocol::WireType::VARCHAR:
        case protocol::WireType::CHAR:
        case protocol::WireType::JSON:
        case protocol::WireType::JSONB:
        case protocol::WireType::XML:
        case protocol::WireType::ARRAY:
        case protocol::WireType::COMPOSITE:
        case protocol::WireType::INET:
        case protocol::WireType::CIDR:
        case protocol::WireType::MACADDR:
        case protocol::WireType::TSVECTOR:
        case protocol::WireType::TSQUERY:
        case protocol::WireType::RANGE:
            return std::string(val.data.begin(), val.data.end());
        case protocol::WireType::DATE: {
            int32_t days32 = 0;
            int64_t days64 = 0;
            if (val.data.size() == sizeof(int32_t) && decodeScalar(val.data, days32)) {
                return formatDateFromEpochDays(static_cast<int64_t>(days32) + kDaysFrom1970To2000);
            }
            if (val.data.size() == sizeof(int64_t) && decodeScalar(val.data, days64)) {
                return formatDateFromEpochDays(days64);
            }
            return std::string(val.data.begin(), val.data.end());
        }
        case protocol::WireType::TIME: {
            int64_t micros = 0;
            if (decodeScalar(val.data, micros)) {
                return formatTimeFromMicros(micros);
            }
            return std::string(val.data.begin(), val.data.end());
        }
        case protocol::WireType::TIMESTAMP: {
            int64_t micros = 0;
            if (decodeScalar(val.data, micros)) {
                return formatTimestampFromMicros(micros);
            }
            return std::string(val.data.begin(), val.data.end());
        }
        case protocol::WireType::TIMESTAMPTZ: {
            if (val.data.size() >= sizeof(int64_t) + sizeof(int16_t)) {
                int64_t micros = 0;
                int16_t offset = 0;
                std::memcpy(&micros, val.data.data(), sizeof(int64_t));
                std::memcpy(&offset, val.data.data() + sizeof(int64_t), sizeof(int16_t));
                return formatTimestampWithOffset(micros, offset);
            }
            int64_t micros = 0;
            if (decodeScalar(val.data, micros)) {
                return formatTimestampFromMicros(micros);
            }
            return std::string(val.data.begin(), val.data.end());
        }
        case protocol::WireType::INTERVAL: {
            if (val.data.size() >= sizeof(int32_t) * 2 + sizeof(int64_t)) {
                int32_t months = 0;
                int32_t days = 0;
                int64_t micros = 0;
                std::memcpy(&months, val.data.data(), sizeof(int32_t));
                std::memcpy(&days, val.data.data() + sizeof(int32_t), sizeof(int32_t));
                std::memcpy(&micros, val.data.data() + sizeof(int32_t) * 2, sizeof(int64_t));
                return formatInterval(months, days, micros);
            }
            return std::string(val.data.begin(), val.data.end());
        }
        case protocol::WireType::UUID:
            if (val.data.size() == 16) {
                return formatUuid(val.data);
            }
            return std::string(val.data.begin(), val.data.end());
        case protocol::WireType::MONEY: {
            int64_t cents = 0;
            if (!decodeScalar(val.data, cents)) {
                return std::string(val.data.begin(), val.data.end());
            }
            bool negative = cents < 0;
            int64_t abs_cents = negative ? -cents : cents;
            int64_t units = abs_cents / 100;
            int64_t frac = abs_cents % 100;
            std::ostringstream ss;
            if (negative) {
                ss << '-';
            }
            ss << units << '.' << std::setw(2) << std::setfill('0') << frac;
            return ss.str();
        }
        case protocol::WireType::BYTEA:
        case protocol::WireType::GEOMETRY:
        case protocol::WireType::VECTOR:
            return std::string(val.data.begin(), val.data.end());
        default:
            return std::string(val.data.begin(), val.data.end());
    }
}

} // namespace odbc
} // namespace scratchbird
