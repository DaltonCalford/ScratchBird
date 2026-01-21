#include "scratchbird/client/sql_helpers.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <sstream>
#include <iomanip>

#include "scratchbird/core/firebird_datetime.h"

namespace scratchbird {
namespace client {

namespace {
int64_t floorDiv(int64_t a, int64_t b) {
    if (b == 0) {
        return 0;
    }
    int64_t q = a / b;
    int64_t r = a % b;
    if ((r != 0) && ((r < 0) != (b < 0))) {
        --q;
    }
    return q;
}

std::string escapeString(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() * 2);
    for (char c : value) {
        if (c == '\'' || c == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(c);
    }
    return escaped;
}

std::string bytesToHex(const std::vector<uint8_t>& data) {
    static const char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(data.size() * 2);
    for (uint8_t byte : data) {
        out.push_back(kHex[(byte >> 4) & 0x0F]);
        out.push_back(kHex[byte & 0x0F]);
    }
    return out;
}

std::string formatUUID(const std::vector<uint8_t>& data) {
    if (data.size() != 16) {
        return "";
    }
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < data.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            oss << '-';
        }
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

std::string formatDateFromDaysSince2000(int32_t days_since_2000) {
    const int32_t base_mjd = core::FirebirdDateTime::dateToMJD(2000, 1, 1);
    int32_t mjd = base_mjd + days_since_2000;
    return core::FirebirdDateTime::formatDate(mjd);
}

std::string formatTimeFromMicros(int64_t micros) {
    const int64_t micros_per_day = 24LL * 60LL * 60LL * 1000000LL;
    int64_t normalized = micros % micros_per_day;
    if (normalized < 0) {
        normalized += micros_per_day;
    }
    int64_t total_seconds = normalized / 1000000;
    int64_t micro_remainder = normalized % 1000000;
    int hour = static_cast<int>(total_seconds / 3600);
    int minute = static_cast<int>((total_seconds / 60) % 60);
    int second = static_cast<int>(total_seconds % 60);
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hour << ":"
        << std::setw(2) << minute << ":" << std::setw(2) << second;
    if (micro_remainder != 0) {
        oss << "." << std::setw(6) << micro_remainder;
    }
    return oss.str();
}

std::string formatTimestampFromMicros(int64_t micros) {
    int64_t seconds = floorDiv(micros, 1000000);
    int64_t micro_remainder = micros - seconds * 1000000;
    if (micro_remainder < 0) {
        micro_remainder += 1000000;
        --seconds;
    }
    std::time_t sec_time = static_cast<std::time_t>(seconds);
    std::tm* tm = std::gmtime(&sec_time);
    if (!tm) {
        return "1970-01-01 00:00:00";
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                  tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                  tm->tm_hour, tm->tm_min, tm->tm_sec);
    std::string result(buf);
    if (micro_remainder != 0) {
        std::ostringstream oss;
        oss << "." << std::setfill('0') << std::setw(6) << micro_remainder;
        result += oss.str();
    }
    return result;
}

std::string columnValueToSqlLiteral(const protocol::ProtocolCodec::ColumnValue& val,
                                    protocol::WireType type) {
    if (val.is_null) {
        return "NULL";
    }

    switch (type) {
        case protocol::WireType::BOOLEAN:
            return (!val.data.empty() && val.data[0]) ? "TRUE" : "FALSE";
        case protocol::WireType::INT16: {
            if (val.data.size() < sizeof(int16_t)) return "0";
            int16_t value;
            std::memcpy(&value, val.data.data(), sizeof(value));
            return std::to_string(value);
        }
        case protocol::WireType::INT32:
        case protocol::WireType::DATE: {
            if (val.data.size() < sizeof(int32_t)) return "0";
            int32_t value;
            std::memcpy(&value, val.data.data(), sizeof(value));
            if (type == protocol::WireType::DATE) {
                return "DATE '" + formatDateFromDaysSince2000(value) + "'";
            }
            return std::to_string(value);
        }
        case protocol::WireType::INT64:
        case protocol::WireType::TIME:
        case protocol::WireType::TIMESTAMP:
        case protocol::WireType::TIMESTAMPTZ: {
            if (val.data.size() < sizeof(int64_t)) return "0";
            int64_t value;
            std::memcpy(&value, val.data.data(), sizeof(value));
            if (type == protocol::WireType::TIME) {
                return "TIME '" + formatTimeFromMicros(value) + "'";
            }
            if (type == protocol::WireType::TIMESTAMP ||
                type == protocol::WireType::TIMESTAMPTZ) {
                return "TIMESTAMP '" + formatTimestampFromMicros(value) + "'";
            }
            return std::to_string(value);
        }
        case protocol::WireType::FLOAT32:
        case protocol::WireType::FLOAT64: {
            if (val.data.size() < sizeof(double)) return "0";
            double value;
            std::memcpy(&value, val.data.data(), sizeof(value));
            std::ostringstream oss;
            oss.precision(17);
            oss << value;
            return oss.str();
        }
        case protocol::WireType::BYTEA: {
            std::string hex = bytesToHex(val.data);
            return "X'" + hex + "'";
        }
        case protocol::WireType::UUID: {
            std::string formatted = formatUUID(val.data);
            if (!formatted.empty()) {
                return "'" + formatted + "'";
            }
            break;
        }
        case protocol::WireType::JSON:
        case protocol::WireType::JSONB:
        case protocol::WireType::VARCHAR:
        case protocol::WireType::CHAR:
        case protocol::WireType::XML:
        case protocol::WireType::INET:
        case protocol::WireType::CIDR:
        case protocol::WireType::MACADDR:
        case protocol::WireType::TSVECTOR:
        case protocol::WireType::TSQUERY:
        case protocol::WireType::RANGE:
        case protocol::WireType::ARRAY:
        case protocol::WireType::COMPOSITE:
        case protocol::WireType::DECIMAL:
        case protocol::WireType::MONEY:
            break;
        default:
            break;
    }

    if (val.data.empty()) {
        return "''";
    }

    if (val.data.size() == 1) {
        return val.data[0] ? "TRUE" : "FALSE";
    } else if (val.data.size() == 2) {
        int16_t value;
        std::memcpy(&value, val.data.data(), 2);
        return std::to_string(value);
    } else if (val.data.size() == 4) {
        int32_t value;
        std::memcpy(&value, val.data.data(), 4);
        return std::to_string(value);
    } else if (val.data.size() == 8) {
        int64_t int_value;
        std::memcpy(&int_value, val.data.data(), 8);

        double dbl_value;
        std::memcpy(&dbl_value, val.data.data(), 8);

        if (std::isfinite(dbl_value) && dbl_value != static_cast<double>(int_value)) {
            std::ostringstream oss;
            oss.precision(17);
            oss << dbl_value;
            return oss.str();
        }

        return std::to_string(int_value);
    }

    std::string str(val.data.begin(), val.data.end());
    return "'" + escapeString(str) + "'";
}
}

size_t countParameters(const std::string& sql) {
    size_t param_count = 0;
    for (size_t i = 0; i < sql.size(); ++i) {
        if (sql[i] == '?') {
            ++param_count;
        } else if (sql[i] == '$' && i + 1 < sql.size() && std::isdigit(sql[i + 1])) {
            size_t num = 0;
            size_t j = i + 1;
            while (j < sql.size() && std::isdigit(sql[j])) {
                num = num * 10 + (sql[j] - '0');
                ++j;
            }
            if (num > param_count) {
                param_count = num;
            }
            i = j - 1;
        }
    }
    return param_count;
}

std::string substituteParameters(
    const std::string& sql,
    const std::vector<protocol::ProtocolCodec::ColumnValue>& params) {
    std::vector<protocol::WireType> empty_types;
    return substituteParameters(sql, params, empty_types);
}

std::string substituteParameters(
    const std::string& sql,
    const std::vector<protocol::ProtocolCodec::ColumnValue>& params,
    const std::vector<protocol::WireType>& param_types) {
    std::string result;
    result.reserve(sql.size() * 2);

    size_t i = 0;
    size_t next_param = 0;
    while (i < sql.size()) {
        if (sql[i] == '$' && i + 1 < sql.size() && std::isdigit(sql[i + 1])) {
            size_t j = i + 1;
            size_t param_num = 0;
            while (j < sql.size() && std::isdigit(sql[j])) {
                param_num = param_num * 10 + (sql[j] - '0');
                ++j;
            }

            if (param_num > 0 && param_num <= params.size()) {
                protocol::WireType type = protocol::WireType::UNKNOWN;
                if (param_num - 1 < param_types.size()) {
                    type = param_types[param_num - 1];
                }
                result += columnValueToSqlLiteral(params[param_num - 1], type);
            } else {
                result += sql.substr(i, j - i);
            }
            i = j;
        } else if (sql[i] == '?') {
            if (next_param < params.size()) {
                protocol::WireType type = protocol::WireType::UNKNOWN;
                if (next_param < param_types.size()) {
                    type = param_types[next_param];
                }
                result += columnValueToSqlLiteral(params[next_param], type);
                ++next_param;
            } else {
                result += sql[i];
            }
            ++i;
        } else if (sql[i] == '\'' && i + 1 < sql.size()) {
            result += sql[i++];
            while (i < sql.size() && sql[i] != '\'') {
                if (sql[i] == '\'' && i + 1 < sql.size() && sql[i + 1] == '\'') {
                    result += sql[i++];
                }
                result += sql[i++];
            }
            if (i < sql.size()) {
                result += sql[i++];
            }
        } else if (sql[i] == '"' && i + 1 < sql.size()) {
            result += sql[i++];
            while (i < sql.size() && sql[i] != '"') {
                if (sql[i] == '"' && i + 1 < sql.size() && sql[i + 1] == '"') {
                    result += sql[i++];
                }
                result += sql[i++];
            }
            if (i < sql.size()) {
                result += sql[i++];
            }
        } else if (sql[i] == '-' && i + 1 < sql.size() && sql[i + 1] == '-') {
            while (i < sql.size() && sql[i] != '\n') {
                result += sql[i++];
            }
        } else if (sql[i] == '/' && i + 1 < sql.size() && sql[i + 1] == '*') {
            result += sql[i++];
            result += sql[i++];
            while (i + 1 < sql.size() && !(sql[i] == '*' && sql[i + 1] == '/')) {
                result += sql[i++];
            }
            if (i + 1 < sql.size()) {
                result += sql[i++];
                result += sql[i++];
            }
        } else {
            result += sql[i++];
        }
    }

    return result;
}

} // namespace client
} // namespace scratchbird
