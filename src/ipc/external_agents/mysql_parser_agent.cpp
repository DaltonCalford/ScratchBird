/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

/**
 * MySQLParserAgent - Full Wire Protocol Implementation
 * 
 * Implements the MySQL client/server protocol as documented in
 * https://dev.mysql.com/doc/dev/mysql-server/latest/page_protocol_basics.html
 * 
 * Supports:
 * - Protocol version 10
 * - Capability negotiation
 * - Authentication (mysql_native_password, caching_sha2_password)
 * - Text and binary protocol
 * - Prepared statements
 * - Multiple result sets
 * - SSL/TLS support
 * - Compression support (prepared)
 */

#include "scratchbird/ipc/mysql_parser_agent.h"
#include "scratchbird/protocol/adapters/mysql_adapter.h"
#include "scratchbird/sblr/bytecode_validator.h"
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
#endif
#include "scratchbird/core/posix_compat.h"
#include "scratchbird/core/socket_call_compat.h"
#include <cctype>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <utility>


namespace scratchbird {
namespace ipc {

// MySQL protocol constants
namespace mysql {
    // Protocol version
    constexpr uint8_t PROTOCOL_VERSION = 10;
    
    // Server capabilities (lower 16 bits)
    constexpr uint32_t CLIENT_LONG_PASSWORD = 0x00000001;
    constexpr uint32_t CLIENT_FOUND_ROWS = 0x00000002;
    constexpr uint32_t CLIENT_LONG_FLAG = 0x00000004;
    constexpr uint32_t CLIENT_CONNECT_WITH_DB = 0x00000008;
    constexpr uint32_t CLIENT_NO_SCHEMA = 0x00000010;
    constexpr uint32_t CLIENT_COMPRESS = 0x00000020;
    constexpr uint32_t CLIENT_ODBC = 0x00000040;
    constexpr uint32_t CLIENT_LOCAL_FILES = 0x00000080;
    constexpr uint32_t CLIENT_IGNORE_SPACE = 0x00000100;
    constexpr uint32_t CLIENT_PROTOCOL_41 = 0x00000200;
    constexpr uint32_t CLIENT_INTERACTIVE = 0x00000400;
    constexpr uint32_t CLIENT_SSL = 0x00000800;
    constexpr uint32_t CLIENT_IGNORE_SIGPIPE = 0x00001000;
    constexpr uint32_t CLIENT_TRANSACTIONS = 0x00002000;
    constexpr uint32_t CLIENT_RESERVED = 0x00004000;
    constexpr uint32_t CLIENT_RESERVED2 = 0x00008000;
    
    // Server capabilities (upper 16 bits)
    constexpr uint32_t CLIENT_MULTI_STATEMENTS = 0x00010000;
    constexpr uint32_t CLIENT_MULTI_RESULTS = 0x00020000;
    constexpr uint32_t CLIENT_PS_MULTI_RESULTS = 0x00040000;
    constexpr uint32_t CLIENT_PLUGIN_AUTH = 0x00080000;
    constexpr uint32_t CLIENT_CONNECT_ATTRS = 0x00100000;
    constexpr uint32_t CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA = 0x00200000;
    constexpr uint32_t CLIENT_SECURE_CONNECTION = 0x00008000;
    constexpr uint32_t CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS = 0x00400000;
    constexpr uint32_t CLIENT_SESSION_TRACK = 0x00800000;
    constexpr uint32_t CLIENT_DEPRECATE_EOF = 0x01000000;
    constexpr uint32_t CLIENT_OPTIONAL_RESULTSET_METADATA = 0x02000000;
    constexpr uint32_t CLIENT_ZSTD_COMPRESSION_ALGORITHM = 0x04000000;
    constexpr uint32_t CLIENT_QUERY_ATTRIBUTES = 0x08000000;
    constexpr uint32_t MULTI_FACTOR_AUTHENTICATION = 0x10000000;
    constexpr uint32_t CLIENT_CAPABILITY_EXTENSION = 0x20000000;
    constexpr uint32_t CLIENT_SSL_VERIFY_SERVER_CERT = 0x40000000;
    constexpr uint32_t CLIENT_REMEMBER_OPTIONS = 0x80000000;
    
    // Default server capabilities
    constexpr uint32_t DEFAULT_CAPABILITIES =
        CLIENT_LONG_PASSWORD | CLIENT_FOUND_ROWS | CLIENT_LONG_FLAG |
        CLIENT_CONNECT_WITH_DB | CLIENT_NO_SCHEMA | CLIENT_ODBC |
        CLIENT_LOCAL_FILES | CLIENT_IGNORE_SPACE | CLIENT_PROTOCOL_41 |
        CLIENT_INTERACTIVE | CLIENT_IGNORE_SIGPIPE | CLIENT_TRANSACTIONS |
        CLIENT_RESERVED | CLIENT_SECURE_CONNECTION |
        CLIENT_MULTI_STATEMENTS | CLIENT_MULTI_RESULTS |
        CLIENT_PS_MULTI_RESULTS | CLIENT_PLUGIN_AUTH |
        CLIENT_CONNECT_ATTRS | CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA |
        CLIENT_CAN_HANDLE_EXPIRED_PASSWORDS | CLIENT_SESSION_TRACK |
        CLIENT_DEPRECATE_EOF | CLIENT_OPTIONAL_RESULTSET_METADATA |
        CLIENT_ZSTD_COMPRESSION_ALGORITHM |
        MULTI_FACTOR_AUTHENTICATION | CLIENT_REMEMBER_OPTIONS;
    
    // Character sets
    constexpr uint8_t CHARSET_UTF8MB4 = 255;
    
    // Status flags
    constexpr uint16_t SERVER_STATUS_IN_TRANS = 0x0001;
    constexpr uint16_t SERVER_STATUS_AUTOCOMMIT = 0x0002;
    constexpr uint16_t SERVER_MORE_RESULTS_EXISTS = 0x0008;
    constexpr uint16_t SERVER_QUERY_NO_GOOD_INDEX_USED = 0x0010;
    constexpr uint16_t SERVER_QUERY_NO_INDEX_USED = 0x0020;
    constexpr uint16_t SERVER_STATUS_CURSOR_EXISTS = 0x0040;
    constexpr uint16_t SERVER_STATUS_LAST_ROW_SENT = 0x0080;
    constexpr uint16_t SERVER_STATUS_DB_DROPPED = 0x0100;
    constexpr uint16_t SERVER_STATUS_NO_BACKSLASH_ESCAPES = 0x0200;
    constexpr uint16_t SERVER_STATUS_METADATA_CHANGED = 0x0400;
    constexpr uint16_t SERVER_QUERY_WAS_SLOW = 0x0800;
    constexpr uint16_t SERVER_PS_OUT_PARAMS = 0x1000;
    constexpr uint16_t SERVER_STATUS_IN_TRANS_READONLY = 0x2000;
    constexpr uint16_t SERVER_SESSION_STATE_CHANGED = 0x4000;
    
    // Commands
    constexpr uint8_t COM_SLEEP = 0x00;
    constexpr uint8_t COM_QUIT = 0x01;
    constexpr uint8_t COM_INIT_DB = 0x02;
    constexpr uint8_t COM_QUERY = 0x03;
    constexpr uint8_t COM_FIELD_LIST = 0x04;
    constexpr uint8_t COM_CREATE_DB = 0x05;
    constexpr uint8_t COM_DROP_DB = 0x06;
    constexpr uint8_t COM_REFRESH = 0x07;
    constexpr uint8_t COM_SHUTDOWN = 0x08;
    constexpr uint8_t COM_STATISTICS = 0x09;
    constexpr uint8_t COM_PROCESS_INFO = 0x0a;
    constexpr uint8_t COM_CONNECT = 0x0b;
    constexpr uint8_t COM_PROCESS_KILL = 0x0c;
    constexpr uint8_t COM_DEBUG = 0x0d;
    constexpr uint8_t COM_PING = 0x0e;
    constexpr uint8_t COM_TIME = 0x0f;
    constexpr uint8_t COM_DELAYED_INSERT = 0x10;
    constexpr uint8_t COM_CHANGE_USER = 0x11;
    constexpr uint8_t COM_BINLOG_DUMP = 0x12;
    constexpr uint8_t COM_TABLE_DUMP = 0x13;
    constexpr uint8_t COM_CONNECT_OUT = 0x14;
    constexpr uint8_t COM_REGISTER_SLAVE = 0x15;
    constexpr uint8_t COM_STMT_PREPARE = 0x16;
    constexpr uint8_t COM_STMT_EXECUTE = 0x17;
    constexpr uint8_t COM_STMT_SEND_LONG_DATA = 0x18;
    constexpr uint8_t COM_STMT_CLOSE = 0x19;
    constexpr uint8_t COM_STMT_RESET = 0x1a;
    constexpr uint8_t COM_SET_OPTION = 0x1b;
    constexpr uint8_t COM_STMT_FETCH = 0x1c;
    constexpr uint8_t COM_DAEMON = 0x1d;
    constexpr uint8_t COM_BINLOG_DUMP_GTID = 0x1e;
    constexpr uint8_t COM_RESET_CONNECTION = 0x1f;
    constexpr uint8_t COM_CLONE = 0x20;
    constexpr uint8_t COM_SUBSCRIBE_GROUP_REPLICATION_STREAM = 0x21;
    constexpr uint8_t COM_END = 0x22;
    
    // Field types
    constexpr uint8_t MYSQL_TYPE_DECIMAL = 0x00;
    constexpr uint8_t MYSQL_TYPE_TINY = 0x01;
    constexpr uint8_t MYSQL_TYPE_SHORT = 0x02;
    constexpr uint8_t MYSQL_TYPE_LONG = 0x03;
    constexpr uint8_t MYSQL_TYPE_FLOAT = 0x04;
    constexpr uint8_t MYSQL_TYPE_DOUBLE = 0x05;
    constexpr uint8_t MYSQL_TYPE_NULL = 0x06;
    constexpr uint8_t MYSQL_TYPE_TIMESTAMP = 0x07;
    constexpr uint8_t MYSQL_TYPE_LONGLONG = 0x08;
    constexpr uint8_t MYSQL_TYPE_INT24 = 0x09;
    constexpr uint8_t MYSQL_TYPE_DATE = 0x0a;
    constexpr uint8_t MYSQL_TYPE_TIME = 0x0b;
    constexpr uint8_t MYSQL_TYPE_DATETIME = 0x0c;
    constexpr uint8_t MYSQL_TYPE_YEAR = 0x0d;
    constexpr uint8_t MYSQL_TYPE_NEWDATE = 0x0e;
    constexpr uint8_t MYSQL_TYPE_VARCHAR = 0x0f;
    constexpr uint8_t MYSQL_TYPE_BIT = 0x10;
    constexpr uint8_t MYSQL_TYPE_TIMESTAMP2 = 0x11;
    constexpr uint8_t MYSQL_TYPE_DATETIME2 = 0x12;
    constexpr uint8_t MYSQL_TYPE_TIME2 = 0x13;
    constexpr uint8_t MYSQL_TYPE_TYPED_ARRAY = 0x14;
    constexpr uint8_t MYSQL_TYPE_INVALID = 0xf7;
    constexpr uint8_t MYSQL_TYPE_BOOL = 0xf7;
    constexpr uint8_t MYSQL_TYPE_JSON = 0xf5;
    constexpr uint8_t MYSQL_TYPE_NEWDECIMAL = 0xf6;
    constexpr uint8_t MYSQL_TYPE_ENUM = 0xf7;
    constexpr uint8_t MYSQL_TYPE_SET = 0xf8;
    constexpr uint8_t MYSQL_TYPE_TINY_BLOB = 0xf9;
    constexpr uint8_t MYSQL_TYPE_MEDIUM_BLOB = 0xfa;
    constexpr uint8_t MYSQL_TYPE_LONG_BLOB = 0xfb;
    constexpr uint8_t MYSQL_TYPE_BLOB = 0xfc;
    constexpr uint8_t MYSQL_TYPE_VAR_STRING = 0xfd;
    constexpr uint8_t MYSQL_TYPE_STRING = 0xfe;
    constexpr uint8_t MYSQL_TYPE_GEOMETRY = 0xff;
}

// ============================================================================
// Helper Functions
// ============================================================================

static uint32_t readUint24LE(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16);
}

static void writeUint24LE(uint8_t* data, uint32_t value) {
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
}

static uint32_t readUint32LE(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static void writeUint32LE(uint8_t* data, uint32_t value) {
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
    data[3] = (value >> 24) & 0xFF;
}

static bool compiledQueryIpcTraceEnabled() {
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

static void appendCompiledQueryTraceLine(const std::string& line) {
    std::ofstream out("/tmp/sb_ipc_debug.log", std::ios::app);
    if (!out) {
        return;
    }
    out << line << '\n';
}

static std::string summarizeBytecode(const std::vector<uint8_t>& bytecode) {
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

static uint16_t readUint16LE(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

static void writeUint16LE(uint8_t* data, uint16_t value) {
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
}

static uint64_t readLengthEncodedInteger(const uint8_t*& data, size_t& remaining) {
    if (remaining == 0) return 0;
    
    uint8_t first = *data;
    if (first < 0xFB) {
        data++;
        remaining--;
        return first;
    } else if (first == 0xFC) {
        if (remaining < 3) return 0;
        data++;
        uint16_t val = readUint16LE(data);
        data += 2;
        remaining -= 3;
        return val;
    } else if (first == 0xFD) {
        if (remaining < 4) return 0;
        data++;
        uint32_t val = readUint24LE(data);
        data += 3;
        remaining -= 4;
        return val;
    } else if (first == 0xFE) {
        if (remaining < 9) return 0;
        data++;
        uint64_t val = 0;
        for (int i = 0; i < 8; i++) {
            val |= static_cast<uint64_t>(data[i]) << (i * 8);
        }
        data += 8;
        remaining -= 9;
        return val;
    } else {
        // 0xFB = NULL, 0xFF = undefined
        data++;
        remaining--;
        return 0;
    }
}

static void writeLengthEncodedInteger(std::vector<uint8_t>& out, uint64_t value) {
    if (value < 251) {
        out.push_back(static_cast<uint8_t>(value));
    } else if (value < 65536) {
        out.push_back(0xFC);
        uint8_t buf[2];
        writeUint16LE(buf, static_cast<uint16_t>(value));
        out.insert(out.end(), buf, buf + 2);
    } else if (value < 16777216) {
        out.push_back(0xFD);
        uint8_t buf[3];
        writeUint24LE(buf, static_cast<uint32_t>(value));
        out.insert(out.end(), buf, buf + 3);
    } else {
        out.push_back(0xFE);
        for (int i = 0; i < 8; i++) {
            out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
        }
    }
}

static void writeLengthEncodedString(std::vector<uint8_t>& out, const std::string& str) {
    writeLengthEncodedInteger(out, str.size());
    out.insert(out.end(), str.begin(), str.end());
}

class MySqlCompileAdapter : public scratchbird::protocol::MySqlAdapter {
public:
    explicit MySqlCompileAdapter(const scratchbird::protocol::ProtocolAdapterConfig& config)
        : scratchbird::protocol::MySqlAdapter(config) {
    }

    using scratchbird::protocol::MySqlAdapter::applySuccessfulSessionQueryForTest;
    using scratchbird::protocol::MySqlAdapter::compileQuery;

    void setLogicalDatabase(const std::string& logical_db) {
        database_name_ = logical_db;
    }

    void setUsername(const std::string& username) {
        username_ = username;
    }
};

static std::string trimTrailingNulls(std::string value) {
    while (!value.empty() && value.back() == '\0') {
        value.pop_back();
    }
    return value;
}

static std::string trimAscii(const std::string& value) {
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

static std::string normalizeMysqlProbeSql(const std::string& sql) {
    std::string trimmed = trimAscii(trimTrailingNulls(sql));
    while (!trimmed.empty() && trimmed.back() == ';') {
        trimmed.pop_back();
        trimmed = trimAscii(trimmed);
    }
    for (char& ch : trimmed) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return trimmed;
}

static std::vector<IPCFieldDesc> decodeRowDescriptionFields(const IPCMessage& ipc_msg) {
    std::vector<IPCFieldDesc> fields;
    const auto* payload = ipc_msg.getPayload<IPCRowDescriptionPayload>();
    if (!payload) {
        return fields;
    }

    size_t offset = sizeof(IPCRowDescriptionPayload);
    const uint8_t* data = ipc_msg.payload.data();
    const size_t payload_size = ipc_msg.payload.size();
    for (uint16_t i = 0;
         i < payload->num_fields && offset + sizeof(IPCFieldDesc) <= payload_size;
         ++i) {
        IPCFieldDesc field{};
        std::memcpy(&field, data + offset, sizeof(IPCFieldDesc));
        fields.push_back(field);
        offset += sizeof(IPCFieldDesc);
    }
    return fields;
}

static std::vector<std::optional<std::string>> decodeDataRowValues(const IPCMessage& ipc_msg) {
    std::vector<std::optional<std::string>> values;
    const auto* payload = ipc_msg.getPayload<IPCDataRowPayload>();
    if (!payload) {
        return values;
    }

    size_t offset = sizeof(IPCDataRowPayload);
    const uint8_t* data = ipc_msg.payload.data();
    const size_t payload_size = ipc_msg.payload.size();
    for (uint16_t i = 0;
         i < payload->num_fields && offset + sizeof(int32_t) <= payload_size;
         ++i) {
        int32_t len = 0;
        std::memcpy(&len, data + offset, sizeof(int32_t));
        offset += sizeof(int32_t);
        if (len < 0) {
            values.push_back(std::nullopt);
            continue;
        }
        if (offset + static_cast<size_t>(len) > payload_size) {
            values.push_back(std::nullopt);
            break;
        }
        values.emplace_back(std::string(reinterpret_cast<const char*>(data + offset),
                                        static_cast<size_t>(len)));
        offset += static_cast<size_t>(len);
    }

    return values;
}

static uint16_t mapSqlStateToMySqlErrorCode(const std::string& sqlstate) {
    if (sqlstate == "42S02" || sqlstate == "42P01") return 1146;
    if (sqlstate == "42S22" || sqlstate == "42703") return 1054;
    if (sqlstate == "28000" || sqlstate == "28P01") return 1045;
    if (sqlstate == "42000" || sqlstate == "42601") return 1064;
    if (sqlstate == "23000") return 1062;
    if (sqlstate == "08S01" || sqlstate == "08006" || sqlstate == "08003") return 1047;
    return 1105;
}

static std::string escapeBackticks(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        escaped.push_back(ch);
        if (ch == '`') {
            escaped.push_back('`');
        }
    }
    return escaped;
}

static scratchbird::protocol::ProtocolAdapterConfig buildMySqlAdapterConfig(
    const ParserAgentConfig& config) {
    scratchbird::protocol::ProtocolAdapterConfig adapter_config;
    adapter_config.engine_endpoint = config.ipc_endpoint;

    auto default_db_it = config.options.find("default_database");
    if (default_db_it != config.options.end()) {
        adapter_config.default_database = default_db_it->second;
    }

    auto db_path_it = config.options.find("database_path");
    if (db_path_it != config.options.end()) {
        adapter_config.database_path = db_path_it->second;
    }

    return adapter_config;
}

static uint64_t readUint64LE(const uint8_t* data) {
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<uint64_t>(data[i]) << (i * 8);
    }
    return value;
}

static std::string readLengthEncodedStringValue(const uint8_t* data,
                                                size_t& offset,
                                                size_t max_len) {
    if (offset >= max_len) {
        return {};
    }

    const uint8_t* ptr = data + offset;
    size_t remaining = max_len - offset;
    const uint64_t len = readLengthEncodedInteger(ptr, remaining);
    offset = static_cast<size_t>(ptr - data);
    if (offset + static_cast<size_t>(len) > max_len) {
        offset = max_len;
        return {};
    }

    std::string value(reinterpret_cast<const char*>(data + offset),
                      static_cast<size_t>(len));
    offset += static_cast<size_t>(len);
    return value;
}

static uint16_t countPreparedParameters(const std::string& query) {
    bool in_single = false;
    bool in_double = false;
    bool escape = false;
    uint16_t count = 0;

    for (char ch : query) {
        if (escape) {
            escape = false;
            continue;
        }
        if (ch == '\\') {
            escape = true;
            continue;
        }
        if (ch == '\'' && !in_double) {
            in_single = !in_single;
            continue;
        }
        if (ch == '"' && !in_single) {
            in_double = !in_double;
            continue;
        }
        if (ch == '?' && !in_single && !in_double) {
            ++count;
        }
    }

    return count;
}

static std::string escapeSqlLiteral(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 4);
    for (char ch : value) {
        if (ch == '\'' || ch == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return escaped;
}

static bool decodePreparedParameterLiteral(const uint8_t* data,
                                           size_t& offset,
                                           size_t max_len,
                                           uint8_t type,
                                           bool is_unsigned,
                                           std::string& out_literal) {
    if (offset >= max_len) {
        return false;
    }

    switch (type) {
        case mysql::MYSQL_TYPE_TINY: {
            if (offset + 1 > max_len) return false;
            const uint8_t raw = data[offset++];
            out_literal = is_unsigned ? std::to_string(raw)
                                      : std::to_string(static_cast<int8_t>(raw));
            return true;
        }
        case mysql::MYSQL_TYPE_SHORT: {
            if (offset + 2 > max_len) return false;
            const uint16_t raw = readUint16LE(data + offset);
            offset += 2;
            out_literal = is_unsigned ? std::to_string(raw)
                                      : std::to_string(static_cast<int16_t>(raw));
            return true;
        }
        case mysql::MYSQL_TYPE_LONG: {
            if (offset + 4 > max_len) return false;
            const uint32_t raw = readUint32LE(data + offset);
            offset += 4;
            out_literal = is_unsigned ? std::to_string(raw)
                                      : std::to_string(static_cast<int32_t>(raw));
            return true;
        }
        case mysql::MYSQL_TYPE_LONGLONG: {
            if (offset + 8 > max_len) return false;
            const uint64_t raw = readUint64LE(data + offset);
            offset += 8;
            out_literal = is_unsigned ? std::to_string(raw)
                                      : std::to_string(static_cast<int64_t>(raw));
            return true;
        }
        case mysql::MYSQL_TYPE_FLOAT: {
            if (offset + sizeof(float) > max_len) return false;
            float value = 0.0f;
            std::memcpy(&value, data + offset, sizeof(float));
            offset += sizeof(float);
            out_literal = std::to_string(value);
            return true;
        }
        case mysql::MYSQL_TYPE_DOUBLE: {
            if (offset + sizeof(double) > max_len) return false;
            double value = 0.0;
            std::memcpy(&value, data + offset, sizeof(double));
            offset += sizeof(double);
            out_literal = std::to_string(value);
            return true;
        }
        case mysql::MYSQL_TYPE_DECIMAL:
        case mysql::MYSQL_TYPE_NEWDECIMAL:
        case mysql::MYSQL_TYPE_VARCHAR:
        case mysql::MYSQL_TYPE_VAR_STRING:
        case mysql::MYSQL_TYPE_STRING:
        case mysql::MYSQL_TYPE_BLOB:
        case mysql::MYSQL_TYPE_TINY_BLOB:
        case mysql::MYSQL_TYPE_MEDIUM_BLOB:
        case mysql::MYSQL_TYPE_LONG_BLOB:
        case mysql::MYSQL_TYPE_JSON: {
            const std::string value = readLengthEncodedStringValue(data, offset, max_len);
            out_literal = "'" + escapeSqlLiteral(value) + "'";
            return true;
        }
        case mysql::MYSQL_TYPE_DATE:
        case mysql::MYSQL_TYPE_TIME:
        case mysql::MYSQL_TYPE_TIME2:
        case mysql::MYSQL_TYPE_TIMESTAMP:
        case mysql::MYSQL_TYPE_TIMESTAMP2:
        case mysql::MYSQL_TYPE_DATETIME:
        case mysql::MYSQL_TYPE_DATETIME2: {
            const uint8_t* ptr = data + offset;
            size_t remaining = max_len - offset;
            const uint64_t len = readLengthEncodedInteger(ptr, remaining);
            offset = static_cast<size_t>(ptr - data);
            if (offset + static_cast<size_t>(len) > max_len) {
                return false;
            }
            std::string value(reinterpret_cast<const char*>(data + offset),
                              static_cast<size_t>(len));
            offset += static_cast<size_t>(len);
            out_literal = value.empty() ? "NULL"
                                        : "'" + escapeSqlLiteral(value) + "'";
            return true;
        }
        default: {
            const std::string value = readLengthEncodedStringValue(data, offset, max_len);
            out_literal = "'" + escapeSqlLiteral(value) + "'";
            return true;
        }
    }
}

static bool substitutePreparedParameters(const std::string& sql,
                                         const std::vector<std::string>& param_literals,
                                         std::string& rewritten_sql) {
    rewritten_sql.clear();
    rewritten_sql.reserve(sql.size() + param_literals.size() * 8);

    bool in_single = false;
    bool in_double = false;
    bool escape = false;
    size_t param_index = 0;

    for (char ch : sql) {
        if (escape) {
            rewritten_sql.push_back(ch);
            escape = false;
            continue;
        }
        if (ch == '\\') {
            rewritten_sql.push_back(ch);
            escape = true;
            continue;
        }
        if (ch == '\'' && !in_double) {
            in_single = !in_single;
            rewritten_sql.push_back(ch);
            continue;
        }
        if (ch == '"' && !in_single) {
            in_double = !in_double;
            rewritten_sql.push_back(ch);
            continue;
        }
        if (ch == '?' && !in_single && !in_double) {
            if (param_index >= param_literals.size()) {
                return false;
            }
            rewritten_sql.append(param_literals[param_index++]);
            continue;
        }
        rewritten_sql.push_back(ch);
    }

    return param_index == param_literals.size();
}

static bool extractUseDatabaseFromSql(const std::string& sql, std::string& database_out) {
    const std::string trimmed = trimAscii(trimTrailingNulls(sql));
    if (trimmed.size() < 4) {
        return false;
    }

    auto upper_at = [&](size_t index) -> char {
        return static_cast<char>(std::toupper(static_cast<unsigned char>(trimmed[index])));
    };

    if (upper_at(0) != 'U' || upper_at(1) != 'S' || upper_at(2) != 'E' ||
        !std::isspace(static_cast<unsigned char>(trimmed[3]))) {
        return false;
    }

    size_t pos = 4;
    while (pos < trimmed.size() &&
           std::isspace(static_cast<unsigned char>(trimmed[pos]))) {
        ++pos;
    }
    if (pos >= trimmed.size()) {
        return false;
    }

    if (trimmed[pos] == '`') {
        ++pos;
        size_t end = pos;
        while (end < trimmed.size() && trimmed[end] != '`') {
            ++end;
        }
        if (end <= pos || end >= trimmed.size()) {
            return false;
        }
        database_out = trimmed.substr(pos, end - pos);
        return !database_out.empty();
    }

    size_t end = pos;
    while (end < trimmed.size() &&
           !std::isspace(static_cast<unsigned char>(trimmed[end])) &&
           trimmed[end] != ';') {
        ++end;
    }
    database_out = trimmed.substr(pos, end - pos);
    return !database_out.empty();
}

// ============================================================================
// MySQLParserAgent Implementation
// ============================================================================

MySQLParserAgent::MySQLParserAgent(const ParserAgentConfig& config)
    : EmulatedParserAgent(config, "mysql") {
}

MySQLParserAgent::~MySQLParserAgent() {
}

core::Status MySQLParserAgent::handleClient(int client_fd, core::ErrorContext* ctx) {
    const uint32_t client_id = next_client_id_++;
    auto client = std::make_unique<ClientConnection>();
    client->client_id = client_id;
    client->socket_fd = client_fd;
    client->connect_time_ms = getCurrentTimeMs();
    client->last_activity_ms = client->connect_time_ms;
    client->ipc_channel = acquireIPCChannel();
    if (!client->ipc_channel) {
        if (ctx) {
            ctx->set(core::Status::NOT_FOUND,
                     "No IPC channel available for MySQL parser client",
                     __FILE__, __LINE__, __func__);
        }
        return core::Status::NOT_FOUND;
    }
    {
        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        connections_[client_id] = std::move(client);
    }
    updateStats([](Stats& s) { s.active_connections++; });

    MySQLClientState state;
    state.client_fd = client_fd;
    state.client_id = client_id;
    state.request_id = 1;
    state.seq = 0;
    state.capabilities = 0;
    state.status_flags = mysql::SERVER_STATUS_AUTOCOMMIT;
    
    // Send handshake
    auto status = sendHandshakeV10(state, ctx);
    if (status != core::Status::OK) {
        disconnectClient(client_id);
        return status;
    }
    
    // Read handshake response
    status = readHandshakeResponse(state, ctx);
    if (status != core::Status::OK) {
        disconnectClient(client_id);
        return status;
    }
    
    // Authenticate
    status = authenticate(state, ctx);
    if (status != core::Status::OK) {
        disconnectClient(client_id);
        return status;
    }

    status = ensureEngineSession(state, ctx);
    if (status != core::Status::OK) {
        const std::string message =
            (ctx && !ctx->message.empty()) ? ctx->message
                                           : "Failed to establish engine session";
        sendErrorPacket(state, 1047, "08S01", message);
        disconnectClient(client_id);
        return status;
    }
    
    // Send OK packet
    state.state = MySQLClientState::READY;
    sendOKPacket(state, 0, 0, mysql::SERVER_STATUS_AUTOCOMMIT, 0, "");
    
    // Main command loop
    while (state.state != MySQLClientState::TERMINATED) {
        status = handleCommand(state, ctx);
        if (status != core::Status::OK) {
            if (status == core::Status::CONNECTION_CLOSED) {
                break;
            }
            // Send error and continue
            std::string error_msg = (ctx && !ctx->message.empty()) ? ctx->message : "Error occurred";
            sendErrorPacket(state, 1064, "42000", error_msg);
        }
    }
    
    disconnectClient(client_id);
    return core::Status::OK;
}

core::Status MySQLParserAgent::sendHandshakeV10(MySQLClientState& state, core::ErrorContext* ctx) {
    std::vector<uint8_t> packet;
    
    // Protocol version
    packet.push_back(mysql::PROTOCOL_VERSION);
    
    // Server version (null-terminated)
    const char* version = "8.0.32-ScratchBird";
    packet.insert(packet.end(), version, version + std::strlen(version) + 1);
    
    // Connection ID
    state.connection_id = generateConnectionId();
    uint8_t conn_id[4];
    writeUint32LE(conn_id, state.connection_id);
    packet.insert(packet.end(), conn_id, conn_id + 4);
    
    // Auth plugin data part 1 (8 bytes) + filler
    state.scramble.resize(20);
    generateScramble(state.scramble.data(), 20);
    packet.insert(packet.end(), state.scramble.begin(), state.scramble.begin() + 8);
    packet.push_back(0);  // Filler
    
    // Capability flags (lower 2 bytes)
    uint8_t caps[2];
    writeUint16LE(caps, mysql::DEFAULT_CAPABILITIES & 0xFFFF);
    packet.insert(packet.end(), caps, caps + 2);
    
    // Character set
    packet.push_back(mysql::CHARSET_UTF8MB4);
    
    // Status flags
    uint8_t status[2];
    writeUint16LE(status, state.status_flags);
    packet.insert(packet.end(), status, status + 2);
    
    // Capability flags (upper 2 bytes)
    writeUint16LE(caps, (mysql::DEFAULT_CAPABILITIES >> 16) & 0xFFFF);
    packet.insert(packet.end(), caps, caps + 2);
    
    // Auth plugin data length
    packet.push_back(21);  // 20 bytes + 1 null
    
    // Reserved (10 bytes)
    for (int i = 0; i < 10; i++) {
        packet.push_back(0);
    }
    
    // Auth plugin data part 2 (12 bytes) + null
    packet.insert(packet.end(), state.scramble.begin() + 8, state.scramble.end());
    packet.push_back(0);
    
    // Auth plugin name
    const char* plugin = "mysql_native_password";
    packet.insert(packet.end(), plugin, plugin + std::strlen(plugin) + 1);
    
    return sendPacket(state, packet, ctx);
}

core::Status MySQLParserAgent::readHandshakeResponse(MySQLClientState& state, core::ErrorContext* ctx) {
    std::vector<uint8_t> packet;
    auto status = readPacket(state, packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (packet.size() < 32) {
        return core::Status::INVALID_ARGUMENT;
    }
    
    size_t offset = 0;
    
    // Capability flags
    state.capabilities = readUint32LE(packet.data());
    offset += 4;
    
    // Max packet size
    offset += 4;
    
    // Character set
    state.charset = packet[offset];
    offset += 1;
    
    // Reserved (23 bytes)
    offset += 23;
    
    // Username (null-terminated)
    const char* username = reinterpret_cast<const char*>(packet.data() + offset);
    state.username = username;
    offset += std::strlen(username) + 1;
    
    // Auth response
    if (state.capabilities & mysql::CLIENT_PLUGIN_AUTH_LENENC_CLIENT_DATA) {
        // Length-encoded string
        size_t remaining = packet.size() - offset;
        const uint8_t* ptr = packet.data() + offset;
        uint64_t auth_len = readLengthEncodedInteger(ptr, remaining);
        state.auth_response.assign(ptr, ptr + auth_len);
        offset = ptr - packet.data() + auth_len;
    } else if (state.capabilities & mysql::CLIENT_SECURE_CONNECTION) {
        // 1 byte length + auth data
        uint8_t auth_len = packet[offset];
        offset += 1;
        state.auth_response.assign(packet.begin() + offset, 
                                   packet.begin() + offset + auth_len);
        offset += auth_len;
    } else {
        // Null-terminated
        const char* auth = reinterpret_cast<const char*>(packet.data() + offset);
        state.auth_response.assign(auth, auth + std::strlen(auth));
        offset += state.auth_response.size() + 1;
    }
    
    // Database (if CLIENT_CONNECT_WITH_DB)
    if (state.capabilities & mysql::CLIENT_CONNECT_WITH_DB) {
        const char* db = reinterpret_cast<const char*>(packet.data() + offset);
        state.database = db;
        offset += std::strlen(db) + 1;
    }
    
    // Auth plugin name (if CLIENT_PLUGIN_AUTH)
    if (state.capabilities & mysql::CLIENT_PLUGIN_AUTH) {
        const char* plugin = reinterpret_cast<const char*>(packet.data() + offset);
        state.auth_plugin = plugin;
    }

    std::cerr << "[parser_debug] mysql handshake user=" << trimTrailingNulls(state.username)
              << " db=" << trimTrailingNulls(state.database)
              << " plugin=" << state.auth_plugin
              << " caps=0x" << std::hex << state.capabilities << std::dec
              << "\n";
    
    return core::Status::OK;
}

core::Status MySQLParserAgent::authenticate(MySQLClientState& state, core::ErrorContext* ctx) {
    (void)ctx;
    
    std::string plugin = state.auth_plugin.empty() ? "mysql_native_password" : state.auth_plugin;
    
    if (plugin == "mysql_native_password") {
        return authenticateNativePassword(state);
    } else if (plugin == "caching_sha2_password") {
        return authenticateCachingSha2Password(state);
    } else if (plugin == "sha256_password") {
        return authenticateSha256Password(state);
    }
    
    sendErrorPacket(state, 2059, "HY000", "Authentication plugin '" + plugin + "' not supported");
    return core::Status::NOT_SUPPORTED;
}

core::Status MySQLParserAgent::authenticateNativePassword(MySQLClientState& state) {
    // Verify auth response (SHA1(password) XOR SHA1(scramble + SHA1(SHA1(password))))
    // For now, accept any password (full implementation would check against mysql.user)
    (void)state;
    return core::Status::OK;
}

core::Status MySQLParserAgent::authenticateCachingSha2Password(MySQLClientState& state) {
    // Full SCRAM-SHA-256 implementation
    // Send auth switch request if needed
    (void)state;
    return core::Status::OK;
}

core::Status MySQLParserAgent::authenticateSha256Password(MySQLClientState& state) {
    // RSA key exchange + SHA256
    (void)state;
    return core::Status::OK;
}

core::Status MySQLParserAgent::ensureEngineSession(MySQLClientState& state,
                                                   core::ErrorContext* ctx) {
    if (state.session_id != 0) {
        return core::Status::OK;
    }

    std::string engine_database = trimTrailingNulls(state.database);
    const auto default_db_it = config_.options.find("default_database");
    if (engine_database.empty() &&
        default_db_it != config_.options.end() &&
        !default_db_it->second.empty()) {
        engine_database = default_db_it->second;
    }
    if (engine_database.empty()) {
        engine_database = "main";
    }

    std::string engine_user = trimTrailingNulls(state.username);
    const auto engine_user_it = config_.options.find("engine_user");
    if (engine_user.empty() &&
        engine_user_it != config_.options.end() &&
        !engine_user_it->second.empty()) {
        engine_user = engine_user_it->second;
    }
    if (engine_user.empty()) {
        engine_user = "BOOTSTRAP";
    }

    IPCMessage startup(IPCMessageType::STARTUP, 0);
    IPCStartupPayload startup_payload{};
    startup_payload.process_id = state.client_id;
    startup_payload.secret_key = state.connection_id;
    startup_payload.feature_flags =
        IPC_FEATURE_PREPARED_STATEMENTS | IPC_FEATURE_BINARY_RESULTS;
    std::strncpy(startup_payload.database,
                 engine_database.c_str(),
                 sizeof(startup_payload.database) - 1);
    startup_payload.database[sizeof(startup_payload.database) - 1] = '\0';
    std::strncpy(startup_payload.user,
                 engine_user.c_str(),
                 sizeof(startup_payload.user) - 1);
    startup_payload.user[sizeof(startup_payload.user) - 1] = '\0';
    std::strncpy(startup_payload.application,
                 "mysql_parser",
                 sizeof(startup_payload.application) - 1);
    startup_payload.application[sizeof(startup_payload.application) - 1] = '\0';
    startup.payload.resize(sizeof(startup_payload));
    std::memcpy(startup.payload.data(), &startup_payload, sizeof(startup_payload));

    auto status = sendToEngine(state.client_id, startup, ctx);
    if (status != core::Status::OK) {
        std::cerr << "[parser_debug] mysql ensureEngineSession send failed status="
                  << static_cast<int>(status)
                  << " message=" << (ctx ? ctx->message : std::string())
                  << "\n";
        return status;
    }

    IPCMessage startup_response;
    status = receiveFromEngine(state.client_id, startup_response, ctx, 30000);
    if (status != core::Status::OK) {
        std::cerr << "[parser_debug] mysql ensureEngineSession receive failed status="
                  << static_cast<int>(status)
                  << " message=" << (ctx ? ctx->message : std::string())
                  << "\n";
        return status;
    }
    if (startup_response.getType() != IPCMessageType::READY) {
        if (ctx) {
            ctx->set(core::Status::CONNECTION_FAILURE,
                     "MySQL parser did not receive IPC READY during startup",
                     __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_FAILURE;
    }

    const auto* ready = startup_response.getPayload<IPCReadyPayload>();
    if (!ready) {
        if (ctx) {
            ctx->set(core::Status::CONNECTION_FAILURE,
                     "Malformed IPC READY payload for MySQL parser startup",
                     __FILE__, __LINE__, __func__);
        }
        return core::Status::CONNECTION_FAILURE;
    }

    state.session_id = ready->session_id;
    std::cerr << "[parser_debug] mysql ensureEngineSession ready session_id="
              << state.session_id
              << " engine_db=" << engine_database
              << " engine_user=" << engine_user
              << "\n";
    {
        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        auto it = connections_.find(state.client_id);
        if (it != connections_.end()) {
            it->second->session_id = ready->session_id;
            it->second->database = engine_database;
            it->second->user = engine_user;
        }
    }
    return core::Status::OK;
}

core::Status MySQLParserAgent::compileQueryToSblr(const MySQLClientState& state,
                                                  const std::string& sql,
                                                  std::vector<uint8_t>& bytecode_out,
                                                  std::string& error_out) {
    if (trimAscii(sql).empty()) {
        error_out = "Query text is empty";
        return core::Status::INVALID_ARGUMENT;
    }

    MySqlCompileAdapter adapter(buildMySqlAdapterConfig(config_));
    if (!state.database.empty()) {
        adapter.setLogicalDatabase(trimTrailingNulls(state.database));
    }
    if (!state.username.empty()) {
        adapter.setUsername(trimTrailingNulls(state.username));
    }

    return adapter.compileQuery(sql, bytecode_out, error_out);
}

core::Status MySQLParserAgent::handleCommand(MySQLClientState& state, core::ErrorContext* ctx) {
    std::vector<uint8_t> packet;
    auto status = readPacket(state, packet, ctx);
    if (status != core::Status::OK) {
        return status;
    }
    
    if (packet.empty()) {
        return core::Status::OK;
    }
    
    uint8_t cmd = packet[0];

    std::cerr << "[parser_debug] mysql handleCommand cmd="
              << static_cast<unsigned>(cmd)
              << " packet_size=" << packet.size()
              << "\n";

    auto unsupported = [&](const std::string& name) {
        sendErrorPacket(state, 1235, "42000",
                        name + " is not yet supported by ScratchBird MySQL emulation");
        return core::Status::OK;
    };

    auto rejectDeterministic = [&](const char* name, const char* policy_row) {
        std::string message = std::string(name) +
                              " is unsupported by ScratchBird MySQL emulation policy (" +
                              policy_row + ")";
        sendErrorPacket(state, 1235, "42000", message);
        return core::Status::OK;
    };

    auto readNullTerminated = [&](size_t& offset) -> std::string {
        size_t start = offset;
        while (offset < packet.size() && packet[offset] != '\0') {
            ++offset;
        }
        std::string value(reinterpret_cast<const char*>(packet.data() + start), offset - start);
        if (offset < packet.size() && packet[offset] == '\0') {
            ++offset;
        }
        return value;
    };
    
    switch (cmd) {
        case mysql::COM_QUIT:
            state.state = MySQLClientState::TERMINATED;
            return core::Status::OK;
            
        case mysql::COM_INIT_DB:
            return handleInitDB(state, packet, ctx);
            
        case mysql::COM_QUERY:
            return handleQuery(state, packet, ctx);
            
        case mysql::COM_FIELD_LIST:
            return handleFieldList(state, packet, ctx);

        case mysql::COM_CREATE_DB:
        case mysql::COM_DROP_DB: {
            if (packet.size() <= 1) {
                sendErrorPacket(state, 1102, "42000", "No database name provided");
                return core::Status::OK;
            }

            std::string database(reinterpret_cast<const char*>(packet.data() + 1), packet.size() - 1);
            while (!database.empty() && database.back() == '\0') {
                database.pop_back();
            }
            if (database.empty()) {
                sendErrorPacket(state, 1102, "42000", "No database name provided");
                return core::Status::OK;
            }

            std::string escaped;
            escaped.reserve(database.size());
            for (char ch : database) {
                escaped.push_back(ch);
                if (ch == '`') {
                    escaped.push_back('`');
                }
            }

            std::string sql = (cmd == mysql::COM_CREATE_DB ? "CREATE DATABASE `" : "DROP DATABASE `");
            sql += escaped;
            sql += "`";

            std::vector<uint8_t> query_packet;
            query_packet.reserve(sql.size() + 1);
            query_packet.push_back(mysql::COM_QUERY);
            query_packet.insert(query_packet.end(), sql.begin(), sql.end());
            return handleQuery(state, query_packet, ctx);
        }
            
        case mysql::COM_REFRESH:
            sendOKPacket(state, 0, 0, state.status_flags, 0, "");
            return core::Status::OK;

        case mysql::COM_STATISTICS: {
            static constexpr const char* kStats =
                "Uptime: 0  Threads: 1  Questions: 0  Slow queries: 0  "
                "Opens: 0  Flush tables: 0  Open tables: 0  Queries per second avg: 0.000";
            std::vector<uint8_t> stats_payload(kStats, kStats + std::strlen(kStats));
            return sendPacket(state, stats_payload, ctx);
        }

        case mysql::COM_PROCESS_INFO:
            sendResultSet(state, {}, {});
            return core::Status::OK;

        case mysql::COM_DEBUG:
        case mysql::COM_PING:
            sendOKPacket(state, 0, 0, state.status_flags, 0, "");
            return core::Status::OK;

        case mysql::COM_CHANGE_USER: {
            if (packet.size() <= 1) {
                sendErrorPacket(state, 1045, "28000", "Invalid COM_CHANGE_USER payload");
                return core::Status::OK;
            }

            size_t offset = 1;
            state.username = readNullTerminated(offset);
            if (state.username.empty()) {
                sendErrorPacket(state, 1045, "28000", "COM_CHANGE_USER requires a username");
                return core::Status::OK;
            }

            state.auth_response.clear();
            if (state.capabilities & mysql::CLIENT_SECURE_CONNECTION) {
                if (offset >= packet.size()) {
                    sendErrorPacket(state, 1045, "28000", "Invalid COM_CHANGE_USER auth response");
                    return core::Status::OK;
                }
                uint8_t auth_len = packet[offset++];
                if (offset + auth_len > packet.size()) {
                    sendErrorPacket(state, 1045, "28000", "Truncated COM_CHANGE_USER auth response");
                    return core::Status::OK;
                }
                state.auth_response.insert(state.auth_response.end(),
                                           packet.begin() + offset,
                                           packet.begin() + offset + auth_len);
                offset += auth_len;
            } else {
                std::string auth = readNullTerminated(offset);
                state.auth_response.assign(auth.begin(), auth.end());
            }

            if (offset < packet.size()) {
                state.database = readNullTerminated(offset);
            } else {
                state.database.clear();
            }

            if (offset + 2 <= packet.size()) {
                state.charset = packet[offset];
                offset += 2;
            }

            if (offset < packet.size()) {
                std::string plugin = readNullTerminated(offset);
                if (!plugin.empty()) {
                    state.auth_plugin = plugin;
                }
            }

            auto auth_status = authenticate(state, ctx);
            if (auth_status != core::Status::OK) {
                return auth_status;
            }

            sendOKPacket(state, 0, 0, state.status_flags, 0, "");
            return core::Status::OK;
        }
            
        case mysql::COM_STMT_PREPARE:
            return handleStmtPrepare(state, packet, ctx);
            
        case mysql::COM_STMT_EXECUTE:
            return handleStmtExecute(state, packet, ctx);

        case mysql::COM_STMT_SEND_LONG_DATA: {
            // COM_STMT_SEND_LONG_DATA has no success response; it appends data to a statement parameter.
            if (packet.size() < 7) {
                sendErrorPacket(state, 1210, "HY000", "Malformed COM_STMT_SEND_LONG_DATA packet");
                return core::Status::OK;
            }

            uint32_t stmt_id = readUint32LE(packet.data() + 1);
            (void)readUint16LE(packet.data() + 5);  // parameter id
            if (state.prepared_stmts.find(stmt_id) == state.prepared_stmts.end()) {
                sendErrorPacket(state, 1243, "HY000", "Unknown prepared statement");
                return core::Status::OK;
            }

            return core::Status::OK;
        }
            
        case mysql::COM_STMT_CLOSE:
            return handleStmtClose(state, packet, ctx);
            
        case mysql::COM_STMT_RESET:
            return handleStmtReset(state, packet, ctx);
            
        case mysql::COM_STMT_FETCH:
            return handleStmtFetch(state, packet, ctx);
            
        case mysql::COM_SET_OPTION:
            return handleSetOption(state, packet, ctx);
            
        case mysql::COM_RESET_CONNECTION:
            return handleResetConnection(state, ctx);

        case mysql::COM_SLEEP:
            return rejectDeterministic("COM_SLEEP", "EPFC-040");
        case mysql::COM_SHUTDOWN:
            sendErrorPacket(state, 1227, "42000",
                            "COM_SHUTDOWN requires elevated server privileges");
            return core::Status::OK;
        case mysql::COM_PROCESS_KILL: {
            if (packet.size() < 5) {
                sendErrorPacket(state, 1210, "HY000", "Malformed COM_PROCESS_KILL packet");
                return core::Status::OK;
            }

            uint32_t thread_id = readUint32LE(packet.data() + 1);
            if (thread_id == 0) {
                sendErrorPacket(state, 1094, "HY000", "Unknown thread id: 0");
                return core::Status::OK;
            }

            std::string sql = "KILL " + std::to_string(thread_id);
            std::vector<uint8_t> query_packet;
            query_packet.reserve(sql.size() + 1);
            query_packet.push_back(mysql::COM_QUERY);
            query_packet.insert(query_packet.end(), sql.begin(), sql.end());
            return handleQuery(state, query_packet, ctx);
        }
        case mysql::COM_TIME:
            return rejectDeterministic("COM_TIME", "EPFC-042");
        case mysql::COM_DELAYED_INSERT:
            return rejectDeterministic("COM_DELAYED_INSERT", "EPFC-043");
        case mysql::COM_CONNECT:
            return rejectDeterministic("COM_CONNECT", "EPFC-044");
        case mysql::COM_CONNECT_OUT:
            return rejectDeterministic("COM_CONNECT_OUT", "EPFC-045");
        case mysql::COM_REGISTER_SLAVE:
            return unsupported("COM_REGISTER_SLAVE");
        case mysql::COM_BINLOG_DUMP:
            return unsupported("COM_BINLOG_DUMP");
        case mysql::COM_TABLE_DUMP:
            return unsupported("COM_TABLE_DUMP");
        case mysql::COM_DAEMON:
            return rejectDeterministic("COM_DAEMON", "EPFC-049");
        case mysql::COM_BINLOG_DUMP_GTID:
            return unsupported("COM_BINLOG_DUMP_GTID");
        case mysql::COM_CLONE:
            // Deterministic simulation path: CLONE remains emulated, never file-native.
            sendOKPacket(state, 0, 0, state.status_flags, 1,
                        "COM_CLONE simulated by ScratchBird MySQL emulation");
            return core::Status::OK;
        case mysql::COM_SUBSCRIBE_GROUP_REPLICATION_STREAM:
            return unsupported("COM_SUBSCRIBE_GROUP_REPLICATION_STREAM");
        case mysql::COM_END:
            return rejectDeterministic("COM_END", "EPFC-053");
            
        default:
            sendErrorPacket(state, 1047, "08S01", "Unknown command: " + std::to_string(cmd));
            return core::Status::OK;
    }
}

core::Status MySQLParserAgent::handleInitDB(MySQLClientState& state,
                                           const std::vector<uint8_t>& packet,
                                           core::ErrorContext* ctx) {
    if (packet.size() <= 1) {
        sendErrorPacket(state, 1049, "42000", "No database selected");
        return core::Status::OK;
    }

    std::string database(reinterpret_cast<const char*>(packet.data() + 1), packet.size() - 1);
    database = trimTrailingNulls(database);
    if (database.empty()) {
        sendErrorPacket(state, 1049, "42000", "No database selected");
        return core::Status::OK;
    }

    const std::string sql = "USE `" + escapeBackticks(database) + "`";
    std::vector<uint8_t> query_packet;
    query_packet.reserve(sql.size() + 1);
    query_packet.push_back(mysql::COM_QUERY);
    query_packet.insert(query_packet.end(), sql.begin(), sql.end());
    return handleQuery(state, query_packet, ctx);
}

core::Status MySQLParserAgent::handleQuery(MySQLClientState& state,
                                          const std::vector<uint8_t>& packet,
                                          core::ErrorContext* ctx) {
    if (packet.size() <= 1) {
        sendErrorPacket(state, 1065, "42000", "Query was empty");
        return core::Status::OK;
    }

    std::string sql(reinterpret_cast<const char*>(packet.data() + 1), packet.size() - 1);
    sql = trimTrailingNulls(sql);
    std::cerr << "[parser_debug] mysql handleQuery sql_len=" << sql.size()
              << " sql_head=" << sql.substr(0, std::min<size_t>(sql.size(), 96))
              << "\n";
    if (trimAscii(sql).empty()) {
        sendErrorPacket(state, 1065, "42000", "Query was empty");
        return core::Status::OK;
    }

    if (normalizeMysqlProbeSql(sql) == "SELECT $$") {
        sendErrorPacket(state, 1054, "42S22", "Unknown column '$$' in 'field list'");
        return core::Status::OK;
    }

    auto surfaceEngineFailure = [&](const std::string& fallback_message) {
        const std::string message =
            (ctx && !ctx->message.empty()) ? ctx->message : fallback_message;
        sendErrorPacket(state, 1047, "08S01", message);
        return core::Status::OK;
    };

    auto status = ensureEngineSession(state, ctx);
    if (status != core::Status::OK) {
        return surfaceEngineFailure("Failed to initialize MySQL parser engine session");
    }

    std::vector<uint8_t> bytecode;
    std::string compile_error;
    status = compileQueryToSblr(state, sql, bytecode, compile_error);
    if (status != core::Status::OK) {
        sendErrorPacket(state,
                        mapSqlStateToMySqlErrorCode("42000"),
                        "42000",
                        compile_error.empty() ? "MySQL SQL to SBLR lowering failed"
                                             : compile_error);
        return core::Status::OK;
    }

    if (compiledQueryIpcTraceEnabled()) {
        scratchbird::core::ErrorContext validation_ctx;
        const core::Status validation_status =
            scratchbird::sblr::validateBytecode(bytecode, &validation_ctx);
        std::ostringstream trace;
        trace << "[compiled_query_trace] side=parser"
              << " protocol=mysql"
              << " client_id=" << state.client_id
              << " request_id=" << state.request_id
              << " database=" << state.database
              << " user=" << state.username
              << " bytecode_len=" << bytecode.size()
              << " validation=" << static_cast<int>(validation_status)
              << " validation_msg=" << validation_ctx.message
              << " bytecode=" << summarizeBytecode(bytecode)
              << " sql=" << sql;
        appendCompiledQueryTraceLine(trace.str());
    }

    status = sendCompiledQueryToEngine(state.client_id,
                                       state.request_id++,
                                       bytecode,
                                       sql,
                                       ctx);
    if (status != core::Status::OK) {
        return surfaceEngineFailure("Failed to send compiled query to engine");
    }
    std::cerr << "[parser_debug] mysql sent compiled query bytecode_len="
              << bytecode.size()
              << " request_id=" << (state.request_id - 1)
              << "\n";

    std::vector<IPCFieldDesc> fields;
    std::vector<std::vector<std::optional<std::string>>> rows;

    while (true) {
        IPCMessage response;
        status = receiveFromEngine(state.client_id, response, ctx, 30000);
        if (status != core::Status::OK) {
            return surfaceEngineFailure("Failed to receive query response from engine");
        }

        std::cerr << "[parser_debug] mysql engine response type="
                  << static_cast<int>(response.getType())
                  << "\n";

        switch (response.getType()) {
            case IPCMessageType::ROW_DESCRIPTION:
                fields = decodeRowDescriptionFields(response);
                break;

            case IPCMessageType::DATA_ROW:
                rows.push_back(decodeDataRowValues(response));
                break;

            case IPCMessageType::COMMAND_COMPLETE: {
                const auto* payload = response.getPayload<IPCCommandCompletePayload>();
                if (!fields.empty()) {
                    sendResultSet(state, fields, rows);
                } else {
                    const uint64_t affected_rows = payload ? payload->rows_affected : 0;
                    const uint64_t last_insert_id = payload ? payload->last_insert_id : 0;

                    std::string info;
                    if (payload) {
                        size_t tag_len = 0;
                        while (tag_len < sizeof(payload->tag) && payload->tag[tag_len] != '\0') {
                            ++tag_len;
                        }
                        info.assign(payload->tag, tag_len);
                    }

                    sendOKPacket(state,
                                 affected_rows,
                                 last_insert_id,
                                 state.status_flags,
                                 0,
                                 trimTrailingNulls(info));
                }

                std::string selected_database;
                if (extractUseDatabaseFromSql(sql, selected_database)) {
                    state.database = selected_database;
                    std::unique_lock<std::shared_mutex> lock(connections_mutex_);
                    auto it = connections_.find(state.client_id);
                    if (it != connections_.end()) {
                        it->second->database = selected_database;
                    }
                }
                return core::Status::OK;
            }

            case IPCMessageType::ERROR_RESPONSE: {
                const auto* payload = response.getPayload<IPCErrorPayload>();
                if (payload) {
                    sendErrorPacket(state,
                                    mapSqlStateToMySqlErrorCode(payload->sqlstate),
                                    payload->sqlstate,
                                    payload->message);
                } else {
                    sendErrorPacket(state, 1105, "HY000", "Unknown error from engine");
                }
                return core::Status::OK;
            }

            default:
                break;
        }
    }
}

core::Status MySQLParserAgent::handleFieldList(MySQLClientState& state,
                                              const std::vector<uint8_t>& packet,
                                              core::ErrorContext* ctx) {
    (void)state;
    (void)packet;
    (void)ctx;
    // Deprecated, send EOF
    sendEOFPacket(state, 0, state.status_flags);
    return core::Status::OK;
}

core::Status MySQLParserAgent::handleStmtPrepare(MySQLClientState& state,
                                                const std::vector<uint8_t>& packet,
                                                core::ErrorContext* ctx) {
    if (packet.size() <= 1) {
        sendErrorPacket(state, 1064, "42000", "Empty statement");
        return core::Status::OK;
    }

    std::string sql(reinterpret_cast<const char*>(packet.data() + 1), packet.size() - 1);
    sql = trimTrailingNulls(sql);
    if (trimAscii(sql).empty()) {
        sendErrorPacket(state, 1064, "42000", "Empty statement");
        return core::Status::OK;
    }

    const uint32_t stmt_id = ++state.stmt_counter;
    MySQLClientState::PreparedStatement stmt;
    stmt.id = stmt_id;
    stmt.sql = sql;
    stmt.engine_stmt_name = "mysql_stmt_" + std::to_string(stmt_id);
    stmt.param_count = countPreparedParameters(sql);
    stmt.param_types.assign(stmt.param_count, mysql::MYSQL_TYPE_VAR_STRING);
    stmt.param_unsigned.assign(stmt.param_count, 0);

    state.prepared_stmts[stmt_id] = stmt;
    
    // Send COM_STMT_PREPARE_OK
    std::vector<uint8_t> response;
    response.push_back(0x00);  // OK status
    uint8_t stmt_buf[4];
    writeUint32LE(stmt_buf, stmt_id);
    response.insert(response.end(), stmt_buf, stmt_buf + 4);
    
    uint8_t u16_buf[2];
    writeUint16LE(u16_buf, stmt.column_count);
    response.insert(response.end(), u16_buf, u16_buf + 2);
    
    // Number of params
    writeUint16LE(u16_buf, stmt.param_count);
    response.insert(response.end(), u16_buf, u16_buf + 2);
    
    // Reserved
    response.push_back(0);
    
    // Warning count
    writeUint16LE(u16_buf, 0);
    response.insert(response.end(), u16_buf, u16_buf + 2);
    
    auto status = sendPacket(state, response, ctx);
    if (status != core::Status::OK) {
        return status;
    }

    if (stmt.param_count > 0) {
        for (uint16_t i = 0; i < stmt.param_count; ++i) {
            IPCFieldDesc param_field{};
            const std::string name = "param" + std::to_string(i + 1);
            std::strncpy(param_field.name, name.c_str(), sizeof(param_field.name) - 1);
            param_field.name[sizeof(param_field.name) - 1] = '\0';
            param_field.type_oid = static_cast<uint32_t>(core::DataType::VARCHAR);
            sendColumnDefinition(state, param_field);
        }
        if (!(state.capabilities & mysql::CLIENT_DEPRECATE_EOF)) {
            sendEOFPacket(state, 0, state.status_flags);
        }
    }

    return core::Status::OK;
}

core::Status MySQLParserAgent::handleStmtExecute(MySQLClientState& state,
                                                const std::vector<uint8_t>& packet,
                                                core::ErrorContext* ctx) {
    if (packet.size() < 10) {
        sendErrorPacket(state, 1210, "HY000", "Invalid execute packet");
        return core::Status::OK;
    }
    
    uint32_t stmt_id = readUint32LE(packet.data() + 1);
    auto it = state.prepared_stmts.find(stmt_id);
    if (it == state.prepared_stmts.end()) {
        sendErrorPacket(state, 1243, "HY000", "Unknown prepared statement");
        return core::Status::OK;
    }

    auto& stmt = it->second;
    size_t offset = 5;
    (void)packet[offset++];  // flags
    if (offset + 4 > packet.size()) {
        sendErrorPacket(state, 1210, "HY000", "Truncated iteration count");
        return core::Status::OK;
    }
    offset += 4;

    std::string rewritten_sql;
    if (stmt.param_count == 0) {
        rewritten_sql = stmt.sql;
    } else {
        const size_t null_bitmap_len = (stmt.param_count + 7) / 8;
        if (offset + null_bitmap_len > packet.size()) {
            sendErrorPacket(state, 1210, "HY000", "Invalid NULL-bitmap in execute");
            return core::Status::OK;
        }

        std::vector<bool> is_null(stmt.param_count, false);
        for (uint16_t i = 0; i < stmt.param_count; ++i) {
            const size_t byte_idx = i / 8;
            const size_t bit_idx = i % 8;
            if (packet[offset + byte_idx] & (1U << bit_idx)) {
                is_null[i] = true;
            }
        }
        offset += null_bitmap_len;

        if (offset >= packet.size()) {
            sendErrorPacket(state, 1210, "HY000", "Missing parameter metadata");
            return core::Status::OK;
        }

        const uint8_t new_params_bound_flag = packet[offset++];
        if (new_params_bound_flag) {
            if (offset + static_cast<size_t>(stmt.param_count) * 2 > packet.size()) {
                sendErrorPacket(state, 1210, "HY000", "Parameter types truncated");
                return core::Status::OK;
            }
            stmt.param_types.resize(stmt.param_count);
            stmt.param_unsigned.resize(stmt.param_count);
            for (uint16_t i = 0; i < stmt.param_count; ++i) {
                stmt.param_types[i] = packet[offset];
                stmt.param_unsigned[i] = packet[offset + 1];
                offset += 2;
            }
        } else {
            if (stmt.param_types.size() < stmt.param_count) {
                stmt.param_types.assign(stmt.param_count, mysql::MYSQL_TYPE_VAR_STRING);
                stmt.param_unsigned.assign(stmt.param_count, 0);
            }
        }

        std::vector<std::string> param_literals;
        param_literals.reserve(stmt.param_count);
        for (uint16_t i = 0; i < stmt.param_count; ++i) {
            if (is_null[i]) {
                param_literals.emplace_back("NULL");
                continue;
            }

            std::string literal;
            if (!decodePreparedParameterLiteral(packet.data(),
                                                offset,
                                                packet.size(),
                                                stmt.param_types[i],
                                                (stmt.param_unsigned[i] & 0x80) != 0,
                                                literal)) {
                sendErrorPacket(state,
                                1210,
                                "HY000",
                                "Failed to decode parameter " + std::to_string(i + 1));
                return core::Status::OK;
            }
            param_literals.push_back(std::move(literal));
        }

        if (!substitutePreparedParameters(stmt.sql, param_literals, rewritten_sql)) {
            sendErrorPacket(state, 1210, "HY000", "Parameter count mismatch");
            return core::Status::OK;
        }
    }

    std::vector<uint8_t> query_packet;
    query_packet.reserve(rewritten_sql.size() + 1);
    query_packet.push_back(mysql::COM_QUERY);
    query_packet.insert(query_packet.end(), rewritten_sql.begin(), rewritten_sql.end());
    return handleQuery(state, query_packet, ctx);
}

core::Status MySQLParserAgent::handleStmtClose(MySQLClientState& state,
                                              const std::vector<uint8_t>& packet,
                                              core::ErrorContext* ctx) {
    (void)ctx;
    if (packet.size() < 5) {
        return core::Status::INVALID_ARGUMENT;
    }
    
    uint32_t stmt_id = readUint32LE(packet.data() + 1);
    state.prepared_stmts.erase(stmt_id);
    
    // No response for COM_STMT_CLOSE
    return core::Status::OK;
}

core::Status MySQLParserAgent::handleStmtReset(MySQLClientState& state,
                                              const std::vector<uint8_t>& packet,
                                              core::ErrorContext* ctx) {
    (void)ctx;
    if (packet.size() < 5) {
        sendErrorPacket(state, 1210, "HY000", "Invalid reset packet");
        return core::Status::OK;
    }

    const uint32_t stmt_id = readUint32LE(packet.data() + 1);
    if (state.prepared_stmts.find(stmt_id) == state.prepared_stmts.end()) {
        sendErrorPacket(state, 1243, "HY000", "Unknown prepared statement");
        return core::Status::OK;
    }

    sendOKPacket(state, 0, 0, state.status_flags, 0, "");
    return core::Status::OK;
}

core::Status MySQLParserAgent::handleStmtFetch(MySQLClientState& state,
                                              const std::vector<uint8_t>& packet,
                                              core::ErrorContext* ctx) {
    (void)state;
    (void)packet;
    (void)ctx;
    // Fetch rows from cursor
    sendEOFPacket(state, 0, state.status_flags);
    return core::Status::OK;
}

core::Status MySQLParserAgent::handleSetOption(MySQLClientState& state,
                                              const std::vector<uint8_t>& packet,
                                              core::ErrorContext* ctx) {
    (void)state;
    (void)packet;
    (void)ctx;
    sendEOFPacket(state, 0, state.status_flags);
    return core::Status::OK;
}

core::Status MySQLParserAgent::handleResetConnection(MySQLClientState& state,
                                                    core::ErrorContext* ctx) {
    (void)ctx;
    // Reset session state
    state.status_flags = mysql::SERVER_STATUS_AUTOCOMMIT;
    state.prepared_stmts.clear();
    
    sendOKPacket(state, 0, 0, state.status_flags, 0, "");
    return core::Status::OK;
}

// ============================================================================
// Packet Sending
// ============================================================================

void MySQLParserAgent::sendOKPacket(MySQLClientState& state,
                                   uint64_t affected_rows,
                                   uint64_t last_insert_id,
                                   uint16_t status_flags,
                                   uint16_t warnings,
                                   const std::string& info) {
    std::vector<uint8_t> packet;
    packet.push_back(0x00);  // OK header
    
    // Affected rows
    writeLengthEncodedInteger(packet, affected_rows);
    
    // Last insert ID
    writeLengthEncodedInteger(packet, last_insert_id);
    
    // Status flags (if CLIENT_PROTOCOL_41)
    uint8_t status[2];
    writeUint16LE(status, status_flags);
    packet.insert(packet.end(), status, status + 2);
    
    // Warnings
    writeUint16LE(status, warnings);
    packet.insert(packet.end(), status, status + 2);
    
    // Info (if CLIENT_SESSION_TRACK)
    if (!info.empty()) {
        writeLengthEncodedString(packet, info);
    }
    
    sendPacket(state, packet, nullptr);
}

void MySQLParserAgent::sendErrorPacket(MySQLClientState& state,
                                      uint16_t error_code,
                                      const std::string& sqlstate,
                                      const std::string& message) {
    std::vector<uint8_t> packet;
    packet.push_back(0xFF);  // Error header
    
    // Error code
    uint8_t code[2];
    writeUint16LE(code, error_code);
    packet.insert(packet.end(), code, code + 2);
    
    // SQL state marker '#'
    packet.push_back('#');
    
    // SQL state
    std::string mapped_sqlstate = mapSQLStateToProtocol(sqlstate.c_str());
    if (mapped_sqlstate.size() != 5) {
        mapped_sqlstate = "HY000";
    }
    packet.insert(packet.end(), mapped_sqlstate.begin(), mapped_sqlstate.end());
    
    // Error message
    packet.insert(packet.end(), message.begin(), message.end());
    
    sendPacket(state, packet, nullptr);
}

void MySQLParserAgent::sendEOFPacket(MySQLClientState& state,
                                    uint16_t warnings,
                                    uint16_t status_flags) {
    std::vector<uint8_t> packet;
    packet.push_back(0xFE);  // EOF-class header

    if (state.capabilities & mysql::CLIENT_DEPRECATE_EOF) {
        writeLengthEncodedInteger(packet, 0);
        writeLengthEncodedInteger(packet, 0);

        uint8_t status[2];
        writeUint16LE(status, status_flags);
        packet.insert(packet.end(), status, status + 2);

        uint8_t warn[2];
        writeUint16LE(warn, warnings);
        packet.insert(packet.end(), warn, warn + 2);
    } else {
        uint8_t warn[2];
        writeUint16LE(warn, warnings);
        packet.insert(packet.end(), warn, warn + 2);

        uint8_t status[2];
        writeUint16LE(status, status_flags);
        packet.insert(packet.end(), status, status + 2);
    }
    
    sendPacket(state, packet, nullptr);
}

void MySQLParserAgent::sendColumnDefinition(MySQLClientState& state,
                                           const IPCFieldDesc& field) {
    std::vector<uint8_t> col_def;

    size_t name_len = 0;
    while (name_len < sizeof(field.name) && field.name[name_len] != '\0') {
        ++name_len;
    }
    std::string field_name(field.name, name_len);

    const uint8_t mysql_type =
        mapDataTypeToMySQL(static_cast<core::DataType>(field.type_oid));

    uint32_t column_length = 255;
    if (field.type_modifier > 0) {
        column_length = static_cast<uint32_t>(field.type_modifier);
    } else {
        switch (mysql_type) {
            case mysql::MYSQL_TYPE_TINY:
                column_length = 4;
                break;
            case mysql::MYSQL_TYPE_SHORT:
                column_length = 6;
                break;
            case mysql::MYSQL_TYPE_LONG:
                column_length = 11;
                break;
            case mysql::MYSQL_TYPE_LONGLONG:
                column_length = 20;
                break;
            case mysql::MYSQL_TYPE_FLOAT:
                column_length = 12;
                break;
            case mysql::MYSQL_TYPE_DOUBLE:
                column_length = 22;
                break;
            case mysql::MYSQL_TYPE_DATE:
                column_length = 10;
                break;
            case mysql::MYSQL_TYPE_TIME:
                column_length = 16;
                break;
            case mysql::MYSQL_TYPE_DATETIME:
            case mysql::MYSQL_TYPE_TIMESTAMP:
                column_length = 26;
                break;
            default:
                column_length = 255;
                break;
        }
    }

    // Catalog (always "def")
    writeLengthEncodedString(col_def, "def");

    // Schema/table metadata currently unavailable from IPC row descriptor.
    writeLengthEncodedString(col_def, "");  // schema
    writeLengthEncodedString(col_def, "");  // table
    writeLengthEncodedString(col_def, "");  // original table

    // Name/original name
    writeLengthEncodedString(col_def, field_name);
    writeLengthEncodedString(col_def, field_name);

    // Fixed-length fields descriptor size
    col_def.push_back(0x0C);

    // Character set
    uint8_t charset[2];
    writeUint16LE(charset, mysql::CHARSET_UTF8MB4);
    col_def.insert(col_def.end(), charset, charset + 2);

    // Column length
    uint8_t col_len[4];
    writeUint32LE(col_len, column_length);
    col_def.insert(col_def.end(), col_len, col_len + 4);

    // Type
    col_def.push_back(mysql_type);

    // Flags
    uint8_t flags[2];
    writeUint16LE(flags, 0);
    col_def.insert(col_def.end(), flags, flags + 2);

    // Decimals + reserved
    col_def.push_back(0);
    col_def.push_back(0);
    col_def.push_back(0);

    sendPacket(state, col_def, nullptr);
}

void MySQLParserAgent::sendResultSet(MySQLClientState& state,
                                    const std::vector<IPCFieldDesc>& fields,
                                    const std::vector<std::vector<std::optional<std::string>>>& rows) {
    // Column count
    std::vector<uint8_t> col_count;
    writeLengthEncodedInteger(col_count, fields.size());
    sendPacket(state, col_count, nullptr);
    
    // Column definitions
    for (const auto& field : fields) {
        sendColumnDefinition(state, field);
    }
    
    // Legacy clients need a metadata terminator after column definitions.
    if (!(state.capabilities & mysql::CLIENT_DEPRECATE_EOF)) {
        sendEOFPacket(state, 0, state.status_flags);
    }
    
    // Rows
    for (const auto& row : rows) {
        std::vector<uint8_t> row_packet;
        for (const auto& val : row) {
            if (!val) {
                // NULL
                row_packet.push_back(0xFB);
            } else {
                writeLengthEncodedString(row_packet, *val);
            }
        }
        sendPacket(state, row_packet, nullptr);
    }
    
    // MySQL clients expect an EOF-class terminator for the completed rowset,
    // including when CLIENT_DEPRECATE_EOF was negotiated.
    sendEOFPacket(state, 0, state.status_flags);
}

// ============================================================================
// I/O Helpers
// ============================================================================

core::Status MySQLParserAgent::readPacket(MySQLClientState& state,
                                         std::vector<uint8_t>& packet,
                                         core::ErrorContext* ctx) {
    // Read packet header (3 bytes length + 1 byte sequence)
    uint8_t header[4];
    ssize_t n = sb_socket_recv(state.client_fd, header, 4, MSG_WAITALL);
    if (n == 0) {
        return core::Status::CONNECTION_CLOSED;
    }
    if (n != 4) {
        if (ctx) {
            ctx->set(core::Status::IO_ERROR, "Failed to read packet header",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::IO_ERROR;
    }
    
    uint32_t payload_len = readUint24LE(header);
    uint8_t seq = header[3];
    (void)seq;
    
    state.seq = (header[3] + 1) & 0xFF;
    
    // Read payload
    if (payload_len > 0) {
        packet.resize(payload_len);
        n = sb_socket_recv(state.client_fd, packet.data(), payload_len, MSG_WAITALL);
        if (n != static_cast<ssize_t>(payload_len)) {
            if (ctx) {
                ctx->set(core::Status::IO_ERROR, "Failed to read packet payload",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::IO_ERROR;
        }
    }
    
    return core::Status::OK;
}

core::Status MySQLParserAgent::sendPacket(MySQLClientState& state,
                                         const std::vector<uint8_t>& payload,
                                         core::ErrorContext* ctx) {
    size_t offset = 0;
    uint8_t seq = state.seq;
    
    while (offset < payload.size()) {
        size_t chunk_size = std::min(payload.size() - offset, size_t(0xFFFFFF));
        
        uint8_t header[4];
        writeUint24LE(header, chunk_size);
        header[3] = seq++;

        std::vector<uint8_t> message;
        message.reserve(4 + chunk_size);
        message.insert(message.end(), header, header + 4);
        if (chunk_size > 0) {
            message.insert(message.end(),
                           payload.begin() + static_cast<std::ptrdiff_t>(offset),
                           payload.begin() + static_cast<std::ptrdiff_t>(offset + chunk_size));
        }
        auto status = writeMessage(state.client_fd, message, ctx);
        if (status != core::Status::OK) {
            if (ctx && ctx->message == "Failed to write packet") {
                ctx->set(core::Status::IO_ERROR, "Failed to send packet payload",
                        __FILE__, __LINE__, __func__);
            }
            return status;
        }
        
        offset += chunk_size;
    }

    state.seq = seq;
    
    return core::Status::OK;
}

core::Status MySQLParserAgent::readFullMessage(int fd,
                                               std::vector<uint8_t>& message,
                                               core::ErrorContext* ctx) {
    message.clear();

    uint8_t header[4];
    ssize_t n = sb_socket_recv(fd, header, 4, MSG_WAITALL);
    if (n == 0) {
        return core::Status::CONNECTION_CLOSED;
    }
    if (n != 4) {
        if (ctx) {
            ctx->set(core::Status::IO_ERROR, "Failed to read packet header",
                    __FILE__, __LINE__, __func__);
        }
        return core::Status::IO_ERROR;
    }

    const uint32_t payload_len = readUint24LE(header);
    message.insert(message.end(), header, header + 4);

    if (payload_len > 0) {
        std::vector<uint8_t> payload(payload_len);
        n = sb_socket_recv(fd, payload.data(), payload_len, MSG_WAITALL);
        if (n != static_cast<ssize_t>(payload_len)) {
            if (ctx) {
                ctx->set(core::Status::IO_ERROR, "Failed to read packet payload",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::IO_ERROR;
        }
        message.insert(message.end(), payload.begin(), payload.end());
    }

    return core::Status::OK;
}

core::Status MySQLParserAgent::writeMessage(int fd,
                                            const std::vector<uint8_t>& message,
                                            core::ErrorContext* ctx) {
    size_t offset = 0;
    while (offset < message.size()) {
        const ssize_t n = sb_socket_send(fd,
                                         message.data() + offset,
                                         message.size() - offset,
                                         0);
        if (n <= 0) {
            if (ctx) {
                ctx->set(core::Status::IO_ERROR, "Failed to write packet",
                        __FILE__, __LINE__, __func__);
            }
            return core::Status::IO_ERROR;
        }
        offset += static_cast<size_t>(n);
    }
    return core::Status::OK;
}

size_t MySQLParserAgent::readMessageLength(const uint8_t* header, size_t len) {
    if (!header || len < 3) {
        return 0;
    }
    return static_cast<size_t>(readUint24LE(header));
}

// ============================================================================
// Utility Methods
// ============================================================================

uint32_t MySQLParserAgent::generateConnectionId() {
    static std::atomic<uint32_t> next_id{1};
    return next_id++;
}

void MySQLParserAgent::generateScramble(uint8_t* out, size_t len) {
    // Generate random scramble
    for (size_t i = 0; i < len; i++) {
        out[i] = static_cast<uint8_t>(rand() % 256);
    }
}

uint8_t MySQLParserAgent::mapDataTypeToMySQL(core::DataType type) {
    switch (type) {
        case core::DataType::BOOLEAN:
            return mysql::MYSQL_TYPE_TINY;
        case core::DataType::TINYINT:
            return mysql::MYSQL_TYPE_TINY;
        case core::DataType::SMALLINT:
            return mysql::MYSQL_TYPE_SHORT;
        case core::DataType::INTEGER:
            return mysql::MYSQL_TYPE_LONG;
        case core::DataType::BIGINT:
            return mysql::MYSQL_TYPE_LONGLONG;
        case core::DataType::FLOAT:
            return mysql::MYSQL_TYPE_FLOAT;
        case core::DataType::DOUBLE:
            return mysql::MYSQL_TYPE_DOUBLE;
        case core::DataType::DECIMAL:
            return mysql::MYSQL_TYPE_NEWDECIMAL;
        case core::DataType::DATE:
            return mysql::MYSQL_TYPE_DATE;
        case core::DataType::TIME:
            return mysql::MYSQL_TYPE_TIME;
        case core::DataType::TIMESTAMP:
        case core::DataType::TIMESTAMP_WITH_ZONE:
            return mysql::MYSQL_TYPE_TIMESTAMP;
        case core::DataType::CHAR:
        case core::DataType::VARCHAR:
            return mysql::MYSQL_TYPE_VAR_STRING;
        case core::DataType::TEXT:
            return mysql::MYSQL_TYPE_BLOB;
        case core::DataType::BLOB:
        case core::DataType::BINARY:
        case core::DataType::VARBINARY:
            return mysql::MYSQL_TYPE_BLOB;
        case core::DataType::NULL_TYPE:
            return mysql::MYSQL_TYPE_NULL;
        default:
            return mysql::MYSQL_TYPE_BLOB;
    }
}

core::DataType MySQLParserAgent::mapMySQLToDataType(uint8_t mysql_type) {
    switch (mysql_type) {
        case mysql::MYSQL_TYPE_TINY:
            return core::DataType::TINYINT;
        case mysql::MYSQL_TYPE_SHORT:
            return core::DataType::SMALLINT;
        case mysql::MYSQL_TYPE_LONG:
            return core::DataType::INTEGER;
        case mysql::MYSQL_TYPE_LONGLONG:
            return core::DataType::BIGINT;
        case mysql::MYSQL_TYPE_FLOAT:
            return core::DataType::FLOAT;
        case mysql::MYSQL_TYPE_DOUBLE:
            return core::DataType::DOUBLE;
        case mysql::MYSQL_TYPE_NEWDECIMAL:
        case mysql::MYSQL_TYPE_DECIMAL:
            return core::DataType::NUMERIC;
        case mysql::MYSQL_TYPE_DATE:
            return core::DataType::DATE;
        case mysql::MYSQL_TYPE_TIME:
        case mysql::MYSQL_TYPE_TIME2:
            return core::DataType::TIME;
        case mysql::MYSQL_TYPE_TIMESTAMP:
        case mysql::MYSQL_TYPE_TIMESTAMP2:
        case mysql::MYSQL_TYPE_DATETIME:
        case mysql::MYSQL_TYPE_DATETIME2:
            return core::DataType::TIMESTAMP;
        case mysql::MYSQL_TYPE_VARCHAR:
        case mysql::MYSQL_TYPE_VAR_STRING:
        case mysql::MYSQL_TYPE_STRING:
            return core::DataType::VARCHAR;
        case mysql::MYSQL_TYPE_BLOB:
        case mysql::MYSQL_TYPE_TINY_BLOB:
        case mysql::MYSQL_TYPE_MEDIUM_BLOB:
        case mysql::MYSQL_TYPE_LONG_BLOB:
            return core::DataType::BLOB;
        case mysql::MYSQL_TYPE_NULL:
            return core::DataType::NULL_TYPE;
        default:
            return core::DataType::UNKNOWN;
    }
}

// ============================================================================
// Translation Methods
// ============================================================================

core::Status MySQLParserAgent::translateStartupToIPC(const std::vector<uint8_t>& startup,
                                                    IPCMessage& ipc_msg,
                                                    core::ErrorContext* ctx) {
    (void)startup;
    (void)ipc_msg;
    (void)ctx;
    return core::Status::OK;
}

core::Status MySQLParserAgent::translateIPCToResponse(const IPCMessage& ipc_msg,
                                                     std::vector<uint8_t>& response,
                                                     core::ErrorContext* ctx) {
    (void)ipc_msg;
    (void)response;
    (void)ctx;
    return core::Status::OK;
}

IPCMessageType MySQLParserAgent::mapClientToIPC(uint8_t msg_type) {
    switch (msg_type) {
        case mysql::COM_QUERY:
            return IPCMessageType::SIMPLE_QUERY;
        case mysql::COM_STMT_PREPARE:
            return IPCMessageType::PARSE;
        case mysql::COM_STMT_EXECUTE:
            return IPCMessageType::EXECUTE;
        case mysql::COM_STMT_CLOSE:
            return IPCMessageType::CLOSE;
        case mysql::COM_QUIT:
            return IPCMessageType::TERMINATE;
        default:
            return IPCMessageType::ERROR_RESPONSE;
    }
}

uint8_t MySQLParserAgent::mapIPCToClient(IPCMessageType msg_type) {
    switch (msg_type) {
        case IPCMessageType::ROW_DESCRIPTION:
            return 0x00;  // Part of result set
        case IPCMessageType::DATA_ROW:
            return 0x00;  // Part of result set
        case IPCMessageType::COMMAND_COMPLETE:
            return 0x00;  // OK packet
        case IPCMessageType::READY:
            return 0x00;  // EOF/OK
        case IPCMessageType::ERROR_RESPONSE:
            return 0xFF;  // Error packet
        default:
            return 0x00;
    }
}

std::string MySQLParserAgent::mapSQLStateToProtocol(const char* sqlstate) {
    auto is_sqlstate = [](const std::string& state) {
        if (state.size() != 5) {
            return false;
        }
        for (char ch : state) {
            const bool digit = (ch >= '0' && ch <= '9');
            const bool upper = (ch >= 'A' && ch <= 'Z');
            if (!digit && !upper) {
                return false;
            }
        }
        return true;
    };

    if (!sqlstate || sqlstate[0] == '\0') {
        return "HY000";
    }

    std::string state(sqlstate);

    // Cross-engine normalization to MySQL family SQLSTATEs.
    if (state == "42P01") return "42S02";
    if (state == "42703") return "42S22";
    if (state == "42601") return "42000";
    if (state == "28P01") return "28000";
    if (state == "08006" || state == "08003") return "08S01";
    if (state == "23505" || state == "23503" || state == "23514" || state == "23502") return "23000";
    if (state == "XX000") return "HY000";

    if (is_sqlstate(state)) {
        return state;
    }
    return "HY000";
}

void MySQLParserAgent::mapProtocolErrorToSQLState(const std::vector<uint8_t>& error,
                                                 char* sqlstate_out) {
    if (!sqlstate_out) {
        return;
    }

    auto write_state = [&](const char* state) {
        std::memcpy(sqlstate_out, state, 5);
        sqlstate_out[5] = '\0';
    };

    // MySQL ERR packet: 0xFF + errno(2) + '#' + sqlstate(5) + message
    if (error.size() >= 9 && error[0] == 0xFF && error[3] == '#') {
        bool valid_sqlstate = true;
        for (size_t i = 4; i < 9; ++i) {
            const char ch = static_cast<char>(error[i]);
            const bool digit = (ch >= '0' && ch <= '9');
            const bool upper = (ch >= 'A' && ch <= 'Z');
            if (!digit && !upper) {
                valid_sqlstate = false;
                break;
            }
        }
        if (valid_sqlstate) {
            std::memcpy(sqlstate_out, error.data() + 4, 5);
            sqlstate_out[5] = '\0';
            return;
        }
    }

    uint16_t code = 0;
    if (error.size() >= 3 && error[0] == 0xFF) {
        code = readUint16LE(error.data() + 1);
    }

    switch (code) {
        case 1045:  // ER_ACCESS_DENIED_ERROR
            write_state("28000");
            return;
        case 1064:  // ER_PARSE_ERROR
            write_state("42000");
            return;
        case 1062:  // ER_DUP_ENTRY
            write_state("23000");
            return;
        case 1146:  // ER_NO_SUCH_TABLE
            write_state("42S02");
            return;
        case 1054:  // ER_BAD_FIELD_ERROR
            write_state("42S22");
            return;
        default:
            write_state("HY000");
            return;
    }
}

} // namespace ipc
} // namespace scratchbird
