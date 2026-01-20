#include "scratchbird/client/sql_helpers.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <sstream>

namespace scratchbird {
namespace client {

namespace {
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

std::string columnValueToSqlLiteral(const protocol::ProtocolCodec::ColumnValue& val) {
    if (val.is_null) {
        return "NULL";
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
                result += columnValueToSqlLiteral(params[param_num - 1]);
            } else {
                result += sql.substr(i, j - i);
            }
            i = j;
        } else if (sql[i] == '?') {
            if (next_param < params.size()) {
                result += columnValueToSqlLiteral(params[next_param]);
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
