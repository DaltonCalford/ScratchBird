/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

#include "scratchbird/ipc/parser_agent.h"

#include <cstring>
#include <unordered_map>

namespace scratchbird {
namespace ipc {

// ============================================================================
// SQLSTATE to Protocol Error Code Mappings
// ============================================================================

// PostgreSQL uses SQLSTATE directly
std::string IPCErrorMapper::sqlStateToPostgreSQL(const char* sqlstate) {
    return std::string(sqlstate);
}

// MySQL error code mapping (simplified - most common codes)
uint16_t IPCErrorMapper::sqlStateToMySQLErrorCode(const char* sqlstate) {
    static const std::unordered_map<std::string, uint16_t> mapping = {
        {"00000", 0},       // ER_OK
        {"01000", 1000},    // ER_WARN
        {"22001", 1406},    // ER_DATA_TOO_LONG
        {"22003", 1264},    // ER_OUT_OF_RANGE
        {"22007", 1292},    // ER_TRUNCATED_WRONG_VALUE
        {"23000", 1062},    // ER_DUP_ENTRY
        {"23503", 1452},    // ER_NO_REFERENCED_ROW_2
        {"23505", 1062},    // ER_DUP_ENTRY
        {"25P02", 1176},    // ER_ILLEGAL_HA
        {"28000", 1045},    // ER_ACCESS_DENIED_ERROR
        {"3D000", 1049},    // ER_BAD_DB_ERROR
        {"3F000", 1046},    // ER_NO_DB_ERROR
        {"40001", 1213},    // ER_LOCK_DEADLOCK
        {"42000", 1064},    // ER_PARSE_ERROR
        {"42501", 1142},    // ER_TABLEACCESS_DENIED_ERROR
        {"42601", 1064},    // ER_PARSE_ERROR
        {"42703", 1054},    // ER_BAD_FIELD_ERROR
        {"42P01", 1146},    // ER_NO_SUCH_TABLE
        {"42P02", 1054},    // ER_BAD_FIELD_ERROR (parameter)
        {"50001", 1819},    // ER_INNODB_IMPORT_ERROR
        {"54000", 1118},    // ER_TOO_BIG_ROWSIZE
        {"55000", 1567},    // ER_CHANGE_MASTER_PASSWORD_LENGTH
        {"XX000", 1819},    // ER_INTERNAL_ERROR
    };
    
    auto it = mapping.find(std::string(sqlstate, 5));
    if (it != mapping.end()) {
        return it->second;
    }
    return 1105;  // ER_UNKNOWN_ERROR
}

// Firebird error code mapping (simplified)
uint32_t IPCErrorMapper::sqlStateToFirebirdErrorCode(const char* sqlstate) {
    static const std::unordered_map<std::string, uint32_t> mapping = {
        {"00000", 0},
        {"01000", 1},
        {"22001", 335544321},  // arithmetic exception
        {"22003", 335544321},
        {"22007", 335544394},  // conversion error
        {"23000", 335544665},  // violation of FOREIGN KEY constraint
        {"23503", 335544665},
        {"23505", 335544349},  // violation of PRIMARY or UNIQUE KEY constraint
        {"28000", 335544472},  // Your user name and password are not defined
        {"3D000", 335544375},  // I/O error during open operation
        {"3F000", 335544375},
        {"40001", 335544336},  // deadlock
        {"42000", 335544569},  // Dynamic SQL Error
        {"42501", 335544550},  // no permission
        {"42601", 335544569},
        {"42703", 335544580},  // Column unknown
        {"42P01", 335544580},
        {"42P02", 335544580},
        {"54000", 335544336},
        {"55000", 335544656},  // database not closed
        {"XX000", 335544321},  // arithmetic exception, numeric overflow
    };
    
    auto it = mapping.find(std::string(sqlstate, 5));
    if (it != mapping.end()) {
        return it->second;
    }
    return 335544321;  // arithmetic exception (generic)
}

// ============================================================================
// Protocol Errors to SQLSTATE Mappings
// ============================================================================

void IPCErrorMapper::postgreSQLErrorToSQLState(uint8_t* pg_error, char* sqlstate_out) {
    // PostgreSQL errors include SQLSTATE in the message
    // Format: 'E' severity\0 SQLSTATE\0 message\0 ...
    // For now, extract first 5 chars or default
    if (pg_error && std::strlen(reinterpret_cast<char*>(pg_error)) >= 5) {
        std::memcpy(sqlstate_out, pg_error, 5);
        sqlstate_out[5] = 0;
        return;
    }
    std::strcpy(sqlstate_out, "XX000");
}

void IPCErrorMapper::mySQLErrorToSQLState(uint16_t mysql_errno, char* sqlstate_out) {
    static const std::unordered_map<uint16_t, const char*> mapping = {
        {0, "00000"},
        {1000, "01000"},
        {1045, "28000"},
        {1046, "3D000"},
        {1049, "42000"},
        {1054, "42703"},
        {1062, "23505"},
        {1064, "42601"},
        {1105, "XX000"},
        {1118, "54000"},
        {1142, "42501"},
        {1146, "42P01"},
        {1213, "40001"},
        {1264, "22003"},
        {1292, "22007"},
        {1406, "22001"},
        {1452, "23503"},
        {1567, "55000"},
        {1819, "XX000"},
    };
    
    auto it = mapping.find(mysql_errno);
    if (it != mapping.end()) {
        std::strcpy(sqlstate_out, it->second);
    } else {
        std::strcpy(sqlstate_out, "XX000");
    }
}

void IPCErrorMapper::firebirdErrorToSQLState(uint32_t fb_code, char* sqlstate_out) {
    static const std::unordered_map<uint32_t, const char*> mapping = {
        {0, "00000"},
        {1, "01000"},
        {335544321, "XX000"},  // arithmetic exception
        {335544336, "40001"},  // deadlock
        {335544349, "23505"},  // unique constraint violation
        {335544375, "3D000"},  // database I/O error
        {335544394, "22007"},  // conversion error
        {335544472, "28000"},  // invalid credentials
        {335544550, "42501"},  // no permission
        {335544569, "42000"},  // Dynamic SQL Error
        {335544580, "42703"},  // Column unknown
        {335544656, "55000"},  // database not closed
        {335544665, "23503"},  // foreign key violation
    };
    
    auto it = mapping.find(fb_code);
    if (it != mapping.end()) {
        std::strcpy(sqlstate_out, it->second);
    } else {
        std::strcpy(sqlstate_out, "XX000");
    }
}

// ============================================================================
// Protocol-Specific Error Message Builders
// ============================================================================

std::vector<uint8_t> IPCErrorMapper::buildPostgreSQLError(const char* sqlstate,
                                                         const std::string& message,
                                                         const std::string& detail) {
    std::vector<uint8_t> response;
    response.push_back('E');  // Error message type
    
    // Build fields
    std::vector<uint8_t> fields;
    
    // Severity (S)
    fields.push_back('S');
    fields.insert(fields.end(), "ERROR", "ERROR" + 5);
    fields.push_back(0);
    
    // SQLSTATE (C)
    fields.push_back('C');
    fields.insert(fields.end(), sqlstate, sqlstate + 5);
    fields.push_back(0);
    
    // Message (M)
    fields.push_back('M');
    fields.insert(fields.end(), message.begin(), message.end());
    fields.push_back(0);
    
    // Detail (D) - optional
    if (!detail.empty()) {
        fields.push_back('D');
        fields.insert(fields.end(), detail.begin(), detail.end());
        fields.push_back(0);
    }
    
    // Null terminator
    fields.push_back(0);
    
    // Calculate length (includes self)
    uint32_t len = 4 + fields.size();
    response.push_back((len >> 24) & 0xFF);
    response.push_back((len >> 16) & 0xFF);
    response.push_back((len >> 8) & 0xFF);
    response.push_back(len & 0xFF);
    
    response.insert(response.end(), fields.begin(), fields.end());
    
    return response;
}

std::vector<uint8_t> IPCErrorMapper::buildMySQLError(uint16_t error_code,
                                                    const std::string& sqlstate,
                                                    const std::string& message) {
    // MySQL Error packet format:
    // 1 byte: 0xFF (error indicator)
    // 2 bytes: error_code (little-endian)
    // 1 byte: '#' (sqlstate marker)
    // 5 bytes: SQLSTATE
    // N bytes: message (null-terminated)
    
    std::vector<uint8_t> packet;
    packet.push_back(0xFF);
    packet.push_back(error_code & 0xFF);
    packet.push_back((error_code >> 8) & 0xFF);
    packet.push_back('#');
    packet.insert(packet.end(), sqlstate.begin(), sqlstate.end());
    packet.insert(packet.end(), message.begin(), message.end());
    packet.push_back(0);
    
    return packet;
}

std::vector<uint8_t> IPCErrorMapper::buildFirebirdError(uint32_t error_code,
                                                       const std::string& message) {
    // Firebird error response uses XDR encoding with status vector
    // This is simplified - full implementation needs proper XDR encoding
    
    std::vector<uint8_t> response;
    
    // op_response
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x09);  // op_response = 9
    
    // Handle (0)
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x00);
    
    // Object ID (0, 0)
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x00);
    
    // Status vector
    // isc_arg_gds
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x01);  // isc_arg_gds = 1
    
    // Error code
    response.push_back((error_code >> 24) & 0xFF);
    response.push_back((error_code >> 16) & 0xFF);
    response.push_back((error_code >> 8) & 0xFF);
    response.push_back(error_code & 0xFF);
    
    // isc_arg_string for message
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x02);  // isc_arg_string = 2
    
    // Message length and content (XDR string)
    uint32_t msg_len = message.length();
    response.push_back((msg_len >> 24) & 0xFF);
    response.push_back((msg_len >> 16) & 0xFF);
    response.push_back((msg_len >> 8) & 0xFF);
    response.push_back(msg_len & 0xFF);
    response.insert(response.end(), message.begin(), message.end());
    
    // Pad to 4-byte boundary
    while (response.size() % 4 != 0) {
        response.push_back(0);
    }
    
    // isc_arg_end
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x00);
    response.push_back(0x00);  // isc_arg_end = 0
    
    return response;
}

} // namespace ipc
} // namespace scratchbird
