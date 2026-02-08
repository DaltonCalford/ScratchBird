/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0
 */

/**
 * Complete Type Mapping System
 * 
 * Maps between ScratchBird internal types and external database types:
 * - PostgreSQL OIDs
 * - MySQL field types
 * - Firebird BLR types
 * - SBWP type codes
 */

#include "scratchbird/udr/type_mapping.h"

namespace scratchbird {
namespace udr {

// ============================================================================
// PostgreSQL Type Mapping
// ============================================================================

// PostgreSQL OIDs (from pg_type.h)
namespace pg_oid {
    constexpr uint32_t BOOL = 16;
    constexpr uint32_t BYTEA = 17;
    constexpr uint32_t CHAR = 18;
    constexpr uint32_t NAME = 19;
    constexpr uint32_t INT8 = 20;
    constexpr uint32_t INT2 = 21;
    constexpr uint32_t INT2VECTOR = 22;
    constexpr uint32_t INT4 = 23;
    constexpr uint32_t REGPROC = 24;
    constexpr uint32_t TEXT = 25;
    constexpr uint32_t OID = 26;
    constexpr uint32_t TID = 27;
    constexpr uint32_t XID = 28;
    constexpr uint32_t CID = 29;
    constexpr uint32_t OIDVECTOR = 30;
    constexpr uint32_t JSON = 114;
    constexpr uint32_t XML = 142;
    constexpr uint32_t PG_NODE_TREE = 194;
    constexpr uint32_t PG_NDISTINCT = 3361;
    constexpr uint32_t PG_DEPENDENCIES = 3402;
    constexpr uint32_t PG_MCV_LIST = 5017;
    constexpr uint32_t PG_DDL_COMMAND = 32;
    constexpr uint32_t XID8 = 5069;
    constexpr uint32_t POINT = 600;
    constexpr uint32_t LSEG = 601;
    constexpr uint32_t PATH = 602;
    constexpr uint32_t BOX = 603;
    constexpr uint32_t POLYGON = 604;
    constexpr uint32_t LINE = 628;
    constexpr uint32_t FLOAT4 = 700;
    constexpr uint32_t FLOAT8 = 701;
    constexpr uint32_t ABSTIME = 702;
    constexpr uint32_t RELTIME = 703;
    constexpr uint32_t TINTERVAL = 704;
    constexpr uint32_t UNKNOWN = 705;
    constexpr uint32_t CIRCLE = 718;
    constexpr uint32_t CASH = 790;
    constexpr uint32_t MACADDR = 829;
    constexpr uint32_t INET = 869;
    constexpr uint32_t CIDR = 650;
    constexpr uint32_t MACADDR8 = 774;
    constexpr uint32_t INT2ARRAY = 1005;
    constexpr uint32_t INT4ARRAY = 1007;
    constexpr uint32_t TEXTARRAY = 1009;
    constexpr uint32_t BYTEAARRAY = 1001;
    constexpr uint32_t BPCHARARRAY = 1014;
    constexpr uint32_t VARCHARARRAY = 1015;
    constexpr uint32_t INT8ARRAY = 1016;
    constexpr uint32_t FLOAT4ARRAY = 1021;
    constexpr uint32_t FLOAT8ARRAY = 1022;
    constexpr uint32_t ACLITEM = 1033;
    constexpr uint32_t BPCHAR = 1042;
    constexpr uint32_t VARCHAR = 1043;
    constexpr uint32_t DATE = 1082;
    constexpr uint32_t TIME = 1083;
    constexpr uint32_t TIMESTAMP = 1114;
    constexpr uint32_t TIMESTAMPTZ = 1184;
    constexpr uint32_t INTERVAL = 1186;
    constexpr uint32_t TIMETZ = 1266;
    constexpr uint32_t BIT = 1560;
    constexpr uint32_t VARBIT = 1562;
    constexpr uint32_t NUMERIC = 1700;
    constexpr uint32_t REFCURSOR = 1790;
    constexpr uint32_t REGPROCEDURE = 2202;
    constexpr uint32_t REGOPER = 2203;
    constexpr uint32_t REGOPERATOR = 2204;
    constexpr uint32_t REGCLASS = 2205;
    constexpr uint32_t REGTYPE = 2206;
    constexpr uint32_t RECORD = 2249;
    constexpr uint32_t CSTRING = 2275;
    constexpr uint32_t ANY = 2276;
    constexpr uint32_t ANYARRAY = 2277;
    constexpr uint32_t VOID = 2278;
    constexpr uint32_t TRIGGER = 2279;
    constexpr uint32_t LANGUAGE_HANDLER = 2280;
    constexpr uint32_t INTERNAL = 2281;
    constexpr uint32_t OPAQUE = 2282;
    constexpr uint32_t ANYELEMENT = 2283;
    constexpr uint32_t RECORDARRAY = 2287;
    constexpr uint32_t ANYNONARRAY = 2776;
    constexpr uint32_t UUID = 2950;
    constexpr uint32_t TXID_SNAPSHOT = 2970;
    constexpr uint32_t FDW_HANDLER = 3115;
    constexpr uint32_t JSONB = 3802;
    constexpr uint32_t ANYRANGE = 3831;
    constexpr uint32_t INT4RANGE = 3904;
    constexpr uint32_t NUMRANGE = 3906;
    constexpr uint32_t TSRANGE = 3908;
    constexpr uint32_t TSTZRANGE = 3910;
    constexpr uint32_t DATERANGE = 3912;
    constexpr uint32_t INT8RANGE = 3926;
    constexpr uint32_t REGNAMESPACE = 4089;
    constexpr uint32_t REGROLE = 4096;
    constexpr uint32_t REGCOLLATION = 4191;
    constexpr uint32_t PG_LSN = 3220;
    constexpr uint32_t NUMERICARRAY = 1231;
    constexpr uint32_t UUIDARRAY = 2951;
    constexpr uint32_t JSONBARRAY = 3807;
    constexpr uint32_t REGTYPEARRAY = 2211;
    constexpr uint32_t REGCLASSARRAY = 2210;
}

uint32_t TypeMapping::toPostgreSQL(core::DataType type) {
    switch (type) {
        case core::DataType::BOOLEAN:
            return pg_oid::BOOL;
        case core::DataType::TINYINT:
        case core::DataType::SMALLINT:
            return pg_oid::INT2;
        case core::DataType::INTEGER:
            return pg_oid::INT4;
        case core::DataType::BIGINT:
            return pg_oid::INT8;
        case core::DataType::REAL:
            return pg_oid::FLOAT4;
        case core::DataType::DOUBLE:
            return pg_oid::FLOAT8;
        case core::DataType::NUMERIC:
            return pg_oid::NUMERIC;
        case core::DataType::CHAR:
            return pg_oid::BPCHAR;
        case core::DataType::VARCHAR:
            return pg_oid::VARCHAR;
        case core::DataType::TEXT:
            return pg_oid::TEXT;
        case core::DataType::DATE:
            return pg_oid::DATE;
        case core::DataType::TIME:
            return pg_oid::TIME;
        case core::DataType::TIMESTAMP:
            return pg_oid::TIMESTAMP;
        case core::DataType::TIMESTAMP_WITH_ZONE:
            return pg_oid::TIMESTAMPTZ;
        case core::DataType::INTERVAL:
            return pg_oid::INTERVAL;
        case core::DataType::BINARY:
        case core::DataType::VARBINARY:
        case core::DataType::BLOB:
            return pg_oid::BYTEA;
        case core::DataType::JSON:
            return pg_oid::JSONB;  // Prefer JSONB
        case core::DataType::XML:
            return pg_oid::XML;
        case core::DataType::UUID:
            return pg_oid::UUID;
        case core::DataType::INET:
            return pg_oid::INET;
        case core::DataType::CIDR:
            return pg_oid::CIDR;
        case core::DataType::MACADDR:
            return pg_oid::MACADDR;
        case core::DataType::BIT:
            return pg_oid::BIT;
        case core::DataType::NULL_TYPE:
            return pg_oid::UNKNOWN;
        default:
            return pg_oid::TEXT;  // Fallback
    }
}

core::DataType TypeMapping::fromPostgreSQL(uint32_t oid) {
    switch (oid) {
        case pg_oid::BOOL:
            return core::DataType::BOOLEAN;
        case pg_oid::INT2:
            return core::DataType::SMALLINT;
        case pg_oid::INT4:
            return core::DataType::INTEGER;
        case pg_oid::INT8:
            return core::DataType::BIGINT;
        case pg_oid::FLOAT4:
            return core::DataType::REAL;
        case pg_oid::FLOAT8:
            return core::DataType::DOUBLE;
        case pg_oid::NUMERIC:
            return core::DataType::NUMERIC;
        case pg_oid::CHAR:
        case pg_oid::BPCHAR:
            return core::DataType::CHAR;
        case pg_oid::VARCHAR:
            return core::DataType::VARCHAR;
        case pg_oid::TEXT:
        case pg_oid::NAME:
            return core::DataType::TEXT;
        case pg_oid::DATE:
            return core::DataType::DATE;
        case pg_oid::TIME:
        case pg_oid::TIMETZ:
            return core::DataType::TIME;
        case pg_oid::TIMESTAMP:
            return core::DataType::TIMESTAMP;
        case pg_oid::TIMESTAMPTZ:
            return core::DataType::TIMESTAMP_WITH_ZONE;
        case pg_oid::INTERVAL:
            return core::DataType::INTERVAL;
        case pg_oid::BYTEA:
            return core::DataType::BLOB;
        case pg_oid::JSON:
        case pg_oid::JSONB:
            return core::DataType::JSON;
        case pg_oid::XML:
            return core::DataType::XML;
        case pg_oid::UUID:
            return core::DataType::UUID;
        case pg_oid::INET:
            return core::DataType::INET;
        case pg_oid::CIDR:
            return core::DataType::CIDR;
        case pg_oid::MACADDR:
        case pg_oid::MACADDR8:
            return core::DataType::MACADDR;
        case pg_oid::BIT:
        case pg_oid::VARBIT:
            return core::DataType::BIT;
        case pg_oid::OID:
        case pg_oid::XID:
        case pg_oid::CID:
            return core::DataType::INTEGER;  // System OIDs as integers
        case pg_oid::CASH:
            return core::DataType::NUMERIC;
        case pg_oid::POINT:
        case pg_oid::LSEG:
        case pg_oid::BOX:
        case pg_oid::PATH:
        case pg_oid::POLYGON:
        case pg_oid::CIRCLE:
        case pg_oid::LINE:
            return core::DataType::TEXT;  // Geometric types as text
        case pg_oid::UNKNOWN:
        case pg_oid::VOID:
            return core::DataType::NULL_TYPE;
        default:
            // Check if it's an array type
            if (oid >= 1000 && oid <= 1009) return core::DataType::ARRAY;  // Array types
            if (oid >= 1021 && oid <= 1022) return core::DataType::ARRAY;
            if (oid == 1231 || oid == 2951 || oid == 3807) return core::DataType::ARRAY;
            return core::DataType::UNKNOWN;
    }
}

// ============================================================================
// MySQL Type Mapping
// ============================================================================

// MySQL field types (from mysql_com.h)
namespace mysql_type {
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

uint8_t TypeMapping::toMySQL(core::DataType type) {
    switch (type) {
        case core::DataType::BOOLEAN:
        case core::DataType::TINYINT:
            return mysql_type::MYSQL_TYPE_TINY;
        case core::DataType::SMALLINT:
            return mysql_type::MYSQL_TYPE_SHORT;
        case core::DataType::INTEGER:
        case core::DataType::MEDIUMINT:
            return mysql_type::MYSQL_TYPE_LONG;
        case core::DataType::BIGINT:
            return mysql_type::MYSQL_TYPE_LONGLONG;
        case core::DataType::FLOAT:
            return mysql_type::MYSQL_TYPE_FLOAT;
        case core::DataType::DOUBLE:
            return mysql_type::MYSQL_TYPE_DOUBLE;
        case core::DataType::NUMERIC:
            return mysql_type::MYSQL_TYPE_NEWDECIMAL;
        case core::DataType::CHAR:
            return mysql_type::MYSQL_TYPE_STRING;
        case core::DataType::VARCHAR:
            return mysql_type::MYSQL_TYPE_VARCHAR;
        case core::DataType::TEXT:
            return mysql_type::MYSQL_TYPE_BLOB;  // MySQL uses BLOB subtypes for TEXT
        case core::DataType::DATE:
            return mysql_type::MYSQL_TYPE_DATE;
        case core::DataType::TIME:
            return mysql_type::MYSQL_TYPE_TIME;
        case core::DataType::DATETIME:
            return mysql_type::MYSQL_TYPE_DATETIME;
        case core::DataType::TIMESTAMP:
        case core::DataType::TIMESTAMP_WITH_ZONE:
            return mysql_type::MYSQL_TYPE_TIMESTAMP;
        case core::DataType::YEAR:
            return mysql_type::MYSQL_TYPE_YEAR;
        case core::DataType::BINARY:
        case core::DataType::VARBINARY:
        case core::DataType::BLOB:
            return mysql_type::MYSQL_TYPE_BLOB;
        case core::DataType::JSON:
            return mysql_type::MYSQL_TYPE_JSON;
        case core::DataType::BIT:
            return mysql_type::MYSQL_TYPE_BIT;
        case core::DataType::GEOMETRY:
        case core::DataType::POINT:
        case core::DataType::LINESTRING:
        case core::DataType::POLYGON:
        case core::DataType::MULTIPOINT:
        case core::DataType::MULTILINESTRING:
        case core::DataType::MULTIPOLYGON:
        case core::DataType::GEOMETRYCOLLECTION:
            return mysql_type::MYSQL_TYPE_GEOMETRY;
        case core::DataType::NULL_TYPE:
            return mysql_type::MYSQL_TYPE_NULL;
        case core::DataType::ENUM:
            return mysql_type::MYSQL_TYPE_ENUM;
        case core::DataType::SET:
            return mysql_type::MYSQL_TYPE_SET;
        default:
            return mysql_type::MYSQL_TYPE_BLOB;
    }
}

core::DataType TypeMapping::fromMySQL(uint8_t mysql_type) {
    switch (mysql_type) {
        case mysql_type::MYSQL_TYPE_TINY:
            return core::DataType::TINYINT;
        case mysql_type::MYSQL_TYPE_SHORT:
            return core::DataType::SMALLINT;
        case mysql_type::MYSQL_TYPE_LONG:
        case mysql_type::MYSQL_TYPE_INT24:
            return core::DataType::INTEGER;
        case mysql_type::MYSQL_TYPE_LONGLONG:
            return core::DataType::BIGINT;
        case mysql_type::MYSQL_TYPE_FLOAT:
            return core::DataType::FLOAT;
        case mysql_type::MYSQL_TYPE_DOUBLE:
            return core::DataType::DOUBLE;
        case mysql_type::MYSQL_TYPE_DECIMAL:
        case mysql_type::MYSQL_TYPE_NEWDECIMAL:
            return core::DataType::NUMERIC;
        case mysql_type::MYSQL_TYPE_VARCHAR:
        case mysql_type::MYSQL_TYPE_VAR_STRING:
            return core::DataType::VARCHAR;
        case mysql_type::MYSQL_TYPE_STRING:
            return core::DataType::CHAR;
        case mysql_type::MYSQL_TYPE_BLOB:
        case mysql_type::MYSQL_TYPE_TINY_BLOB:
        case mysql_type::MYSQL_TYPE_MEDIUM_BLOB:
        case mysql_type::MYSQL_TYPE_LONG_BLOB:
            return core::DataType::BLOB;
        case mysql_type::MYSQL_TYPE_DATE:
        case mysql_type::MYSQL_TYPE_NEWDATE:
            return core::DataType::DATE;
        case mysql_type::MYSQL_TYPE_TIME:
        case mysql_type::MYSQL_TYPE_TIME2:
            return core::DataType::TIME;
        case mysql_type::MYSQL_TYPE_DATETIME:
        case mysql_type::MYSQL_TYPE_DATETIME2:
            return core::DataType::DATETIME;
        case mysql_type::MYSQL_TYPE_TIMESTAMP:
        case mysql_type::MYSQL_TYPE_TIMESTAMP2:
            return core::DataType::TIMESTAMP;
        case mysql_type::MYSQL_TYPE_YEAR:
            return core::DataType::YEAR;
        case mysql_type::MYSQL_TYPE_BIT:
            return core::DataType::BIT;
        case mysql_type::MYSQL_TYPE_JSON:
            return core::DataType::JSON;
        case mysql_type::MYSQL_TYPE_ENUM:
            return core::DataType::ENUM;
        case mysql_type::MYSQL_TYPE_SET:
            return core::DataType::SET;
        case mysql_type::MYSQL_TYPE_GEOMETRY:
            return core::DataType::GEOMETRY;
        case mysql_type::MYSQL_TYPE_NULL:
            return core::DataType::NULL_TYPE;
        default:
            return core::DataType::UNKNOWN;
    }
}

// ============================================================================
// Firebird Type Mapping
// ============================================================================

// Firebird BLR types
namespace fb_blr {
    constexpr uint32_t TEXT = 14;
    constexpr uint32_t SHORT = 7;
    constexpr uint32_t LONG = 8;
    constexpr uint32_t QUAD = 9;
    constexpr uint32_t FLOAT = 10;
    constexpr uint32_t DOUBLE = 27;
    constexpr uint32_t D_FLOAT = 11;
    constexpr uint32_t TIMESTAMP = 35;
    constexpr uint32_t BLOB = 261;
    constexpr uint32_t ARRAY = 9;  // Same as QUAD
    constexpr uint32_t INT64 = 16;
    constexpr uint32_t INT128 = 26;
    constexpr uint32_t SQL_DATE = 12;
    constexpr uint32_t SQL_TIME = 13;
    constexpr uint32_t TIME_TZ = 28;
    constexpr uint32_t TIMESTAMP_TZ = 29;
    constexpr uint32_t BOOLEAN = 23;
    constexpr uint32_t DEC16 = 24;
    constexpr uint32_t DEC34 = 25;
    constexpr uint32_t DEC_FIXED = 26;  // Alias for INT128
}

uint32_t TypeMapping::toFirebird(core::DataType type) {
    switch (type) {
        case core::DataType::BOOLEAN:
            return fb_blr::BOOLEAN;
        case core::DataType::TINYINT:
        case core::DataType::SMALLINT:
            return fb_blr::SHORT;
        case core::DataType::INTEGER:
            return fb_blr::LONG;
        case core::DataType::BIGINT:
            return fb_blr::INT64;
        case core::DataType::INT128:
            return fb_blr::INT128;
        case core::DataType::FLOAT:
            return fb_blr::FLOAT;
        case core::DataType::DOUBLE:
            return fb_blr::DOUBLE;
        case core::DataType::NUMERIC:
            return fb_blr::INT64;  // Or DEC_FIXED for high precision
        case core::DataType::CHAR:
        case core::DataType::VARCHAR:
            return fb_blr::TEXT;
        case core::DataType::DATE:
            return fb_blr::SQL_DATE;
        case core::DataType::TIME:
            return fb_blr::SQL_TIME;
        case core::DataType::TIME_WITH_ZONE:
            return fb_blr::TIME_TZ;
        case core::DataType::TIMESTAMP:
            return fb_blr::TIMESTAMP;
        case core::DataType::TIMESTAMP_WITH_ZONE:
            return fb_blr::TIMESTAMP_TZ;
        case core::DataType::BLOB:
        case core::DataType::BLOB_SUB_TYPE_TEXT:
            return fb_blr::BLOB;
        case core::DataType::ARRAY:
            return fb_blr::ARRAY;
        case core::DataType::NULL_TYPE:
            return 0;
        default:
            return fb_blr::TEXT;
    }
}

core::DataType TypeMapping::fromFirebird(uint32_t blr_type) {
    switch (blr_type) {
        case fb_blr::BOOLEAN:
            return core::DataType::BOOLEAN;
        case fb_blr::SHORT:
            return core::DataType::SMALLINT;
        case fb_blr::LONG:
            return core::DataType::INTEGER;
        case fb_blr::INT64:
            return core::DataType::BIGINT;
        case fb_blr::INT128:
            return core::DataType::INT128;
        case fb_blr::DEC16:
            return core::DataType::DECFLOAT16;
        case fb_blr::DEC34:
            return core::DataType::DECFLOAT34;
        case fb_blr::FLOAT:
            return core::DataType::FLOAT;
        case fb_blr::DOUBLE:
            return core::DataType::DOUBLE;
        case fb_blr::D_FLOAT:
            return core::DataType::DOUBLE;  // Legacy type
        case fb_blr::TEXT:
            return core::DataType::VARCHAR;
        case fb_blr::SQL_DATE:
            return core::DataType::DATE;
        case fb_blr::SQL_TIME:
            return core::DataType::TIME;
        case fb_blr::TIME_TZ:
            return core::DataType::TIME_WITH_ZONE;
        case fb_blr::TIMESTAMP:
            return core::DataType::TIMESTAMP;
        case fb_blr::TIMESTAMP_TZ:
            return core::DataType::TIMESTAMP_WITH_ZONE;
        case fb_blr::BLOB:
            return core::DataType::BLOB;
        case fb_blr::ARRAY:
            return core::DataType::ARRAY;
        default:
            return core::DataType::UNKNOWN;
    }
}

// ============================================================================
// SBWP Type Mapping
// ============================================================================

uint32_t TypeMapping::toSBWP(core::DataType type) {
    // SBWP type codes match internal DataType enum
    return static_cast<uint32_t>(type);
}

core::DataType TypeMapping::fromSBWP(uint32_t type_code) {
    if (type_code <= static_cast<uint32_t>(core::DataType::UNKNOWN)) {
        return static_cast<core::DataType>(type_code);
    }
    return core::DataType::UNKNOWN;
}

// ============================================================================
// Array Type Handling
// ============================================================================

uint32_t TypeMapping::getArrayElementType(uint32_t array_type_oid) {
    // PostgreSQL array OIDs
    switch (array_type_oid) {
        case pg_oid::INT2ARRAY:
            return pg_oid::INT2;
        case pg_oid::INT4ARRAY:
            return pg_oid::INT4;
        case pg_oid::INT8ARRAY:
            return pg_oid::INT8;
        case pg_oid::FLOAT4ARRAY:
            return pg_oid::FLOAT4;
        case pg_oid::FLOAT8ARRAY:
            return pg_oid::FLOAT8;
        case pg_oid::NUMERICARRAY:
            return pg_oid::NUMERIC;
        case pg_oid::TEXTARRAY:
            return pg_oid::TEXT;
        case pg_oid::BYTEAARRAY:
            return pg_oid::BYTEA;
        case pg_oid::BPCHARARRAY:
            return pg_oid::BPCHAR;
        case pg_oid::VARCHARARRAY:
            return pg_oid::VARCHAR;
        case pg_oid::UUIDARRAY:
            return pg_oid::UUID;
        case pg_oid::JSONBARRAY:
            return pg_oid::JSONB;
        default:
            return 0;
    }
}

uint32_t TypeMapping::getArrayTypeOid(uint32_t element_type_oid) {
    // PostgreSQL array type OIDs (element OID + 1 for most types)
    switch (element_type_oid) {
        case pg_oid::INT2:
            return pg_oid::INT2ARRAY;
        case pg_oid::INT4:
            return pg_oid::INT4ARRAY;
        case pg_oid::INT8:
            return pg_oid::INT8ARRAY;
        case pg_oid::FLOAT4:
            return pg_oid::FLOAT4ARRAY;
        case pg_oid::FLOAT8:
            return pg_oid::FLOAT8ARRAY;
        case pg_oid::NUMERIC:
            return pg_oid::NUMERICARRAY;
        case pg_oid::TEXT:
            return pg_oid::TEXTARRAY;
        case pg_oid::BYTEA:
            return pg_oid::BYTEAARRAY;
        case pg_oid::BPCHAR:
            return pg_oid::BPCHARARRAY;
        case pg_oid::VARCHAR:
            return pg_oid::VARCHARARRAY;
        case pg_oid::UUID:
            return pg_oid::UUIDARRAY;
        case pg_oid::JSONB:
            return pg_oid::JSONBARRAY;
        default:
            return 0;
    }
}

// ============================================================================
// Type Information
// ============================================================================

std::string TypeMapping::getTypeName(core::DataType type) {
    switch (type) {
        case core::DataType::UNKNOWN: return "UNKNOWN";
        case core::DataType::BOOLEAN: return "BOOLEAN";
        case core::DataType::TINYINT: return "TINYINT";
        case core::DataType::SMALLINT: return "SMALLINT";
        case core::DataType::MEDIUMINT: return "MEDIUMINT";
        case core::DataType::INTEGER: return "INTEGER";
        case core::DataType::BIGINT: return "BIGINT";
        case core::DataType::INT128: return "INT128";
        case core::DataType::DECFLOAT16: return "DECFLOAT(16)";
        case core::DataType::DECFLOAT34: return "DECFLOAT(34)";
        case core::DataType::REAL: return "REAL";
        case core::DataType::DOUBLE: return "DOUBLE";
        case core::DataType::NUMERIC: return "NUMERIC";
        case core::DataType::DATE: return "DATE";
        case core::DataType::TIME: return "TIME";
        case core::DataType::TIME_WITH_ZONE: return "TIME WITH TIME ZONE";
        case core::DataType::TIMESTAMP: return "TIMESTAMP";
        case core::DataType::TIMESTAMP_WITH_ZONE: return "TIMESTAMP WITH TIME ZONE";
        case core::DataType::INTERVAL: return "INTERVAL";
        case core::DataType::YEAR: return "YEAR";
        case core::DataType::CHAR: return "CHAR";
        case core::DataType::VARCHAR: return "VARCHAR";
        case core::DataType::TEXT: return "TEXT";
        case core::DataType::BINARY: return "BINARY";
        case core::DataType::VARBINARY: return "VARBINARY";
        case core::DataType::BLOB: return "BLOB";
        case core::DataType::JSON: return "JSON";
        case core::DataType::XML: return "XML";
        case core::DataType::UUID: return "UUID";
        case core::DataType::INET: return "INET";
        case core::DataType::CIDR: return "CIDR";
        case core::DataType::MACADDR: return "MACADDR";
        case core::DataType::BIT: return "BIT";
        case core::DataType::GEOMETRY: return "GEOMETRY";
        case core::DataType::POINT: return "POINT";
        case core::DataType::LINESTRING: return "LINESTRING";
        case core::DataType::POLYGON: return "POLYGON";
        case core::DataType::MULTIPOINT: return "MULTIPOINT";
        case core::DataType::MULTILINESTRING: return "MULTILINESTRING";
        case core::DataType::MULTIPOLYGON: return "MULTIPOLYGON";
        case core::DataType::GEOMETRYCOLLECTION: return "GEOMETRYCOLLECTION";
        case core::DataType::ENUM: return "ENUM";
        case core::DataType::SET: return "SET";
        case core::DataType::ARRAY: return "ARRAY";
        case core::DataType::COMPOSITE: return "COMPOSITE";
        case core::DataType::DOMAIN: return "DOMAIN";
        case core::DataType::ROW: return "ROW";
        case core::DataType::NULL_TYPE: return "NULL";
        case core::DataType::BLOB_SUB_TYPE_TEXT: return "BLOB SUB_TYPE TEXT";
        default: return "UNKNOWN";
    }
}

uint32_t TypeMapping::getTypeSize(core::DataType type) {
    switch (type) {
        case core::DataType::BOOLEAN:
        case core::DataType::TINYINT:
            return 1;
        case core::DataType::SMALLINT:
            return 2;
        case core::DataType::MEDIUMINT:
            return 3;
        case core::DataType::INTEGER:
        case core::DataType::FLOAT:
            return 4;
        case core::DataType::BIGINT:
        case core::DataType::DOUBLE:
        case core::DataType::TIMESTAMP:
        case core::DataType::TIMESTAMP_WITH_ZONE:
            return 8;
        case core::DataType::INT128:
        case core::DataType::DECFLOAT16:
            return 16;
        case core::DataType::DECFLOAT34:
            return 34;
        case core::DataType::TIME:
        case core::DataType::TIME_WITH_ZONE:
            return 8;
        case core::DataType::DATE:
            return 4;
        case core::DataType::INTERVAL:
            return 16;
        case core::DataType::UUID:
            return 16;
        case core::DataType::INET:
        case core::DataType::CIDR:
            return 19;  // IPv6 + prefix
        case core::DataType::MACADDR:
            return 6;
        default:
            return 0;  // Variable-length
    }
}

bool TypeMapping::isVariableLength(core::DataType type) {
    switch (type) {
        case core::DataType::VARCHAR:
        case core::DataType::TEXT:
        case core::DataType::BLOB:
        case core::DataType::JSON:
        case core::DataType::XML:
        case core::DataType::VARBINARY:
        case core::DataType::ARRAY:
        case core::DataType::COMPOSITE:
        case core::DataType::GEOMETRY:
        case core::DataType::POINT:
        case core::DataType::LINESTRING:
        case core::DataType::POLYGON:
        case core::DataType::MULTIPOINT:
        case core::DataType::MULTILINESTRING:
        case core::DataType::MULTIPOLYGON:
        case core::DataType::GEOMETRYCOLLECTION:
            return true;
        default:
            return false;
    }
}

bool TypeMapping::isNumericType(core::DataType type) {
    switch (type) {
        case core::DataType::TINYINT:
        case core::DataType::SMALLINT:
        case core::DataType::MEDIUMINT:
        case core::DataType::INTEGER:
        case core::DataType::BIGINT:
        case core::DataType::INT128:
        case core::DataType::REAL:
        case core::DataType::DOUBLE:
        case core::DataType::NUMERIC:
        case core::DataType::DECFLOAT16:
        case core::DataType::DECFLOAT34:
            return true;
        default:
            return false;
    }
}

bool TypeMapping::isStringType(core::DataType type) {
    switch (type) {
        case core::DataType::CHAR:
        case core::DataType::VARCHAR:
        case core::DataType::TEXT:
        case core::DataType::JSON:
        case core::DataType::XML:
            return true;
        default:
            return false;
    }
}

bool TypeMapping::isTemporalType(core::DataType type) {
    switch (type) {
        case core::DataType::DATE:
        case core::DataType::TIME:
        case core::DataType::TIME_WITH_ZONE:
        case core::DataType::TIMESTAMP:
        case core::DataType::TIMESTAMP_WITH_ZONE:
        case core::DataType::INTERVAL:
        case core::DataType::YEAR:
            return true;
        default:
            return false;
    }
}

bool TypeMapping::isBinaryType(core::DataType type) {
    switch (type) {
        case core::DataType::BINARY:
        case core::DataType::VARBINARY:
        case core::DataType::BLOB:
            return true;
        default:
            return false;
    }
}


// ============================================================================
// PostgreSQL Array Detection
// ============================================================================

bool TypeMapping::isPostgreSQLArray(uint32_t oid) {
    // PostgreSQL array OIDs are typically in the 1000+ range
    // Common array OIDs:
    // 1000 - bool[], 1001 - bytea[], 1002 - char[], 1003 - name[]
    // 1005 - int2[], 1007 - int4[], 1009 - text[], 1016 - int8[]
    // 1021 - float4[], 1022 - float8[]
    
    if (oid >= 1000 && oid <= 1200) {
        return true;
    }
    
    // Check for specific known array OIDs outside the range
    switch (oid) {
        case 1561: // bit[]
        case 1563: // varbit[]
        case 2201: // regclass[]
        case 2203: // regtype[]
        case 2951: // uuid[]
        case 3807: // jsonb[]
            return true;
        default:
            return false;
    }
}

} // namespace udr
} // namespace scratchbird
